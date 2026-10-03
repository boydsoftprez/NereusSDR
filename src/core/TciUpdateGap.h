// =================================================================
// src/core/TciUpdateGap.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/TCIServer.cs,
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24 - Receiver and transmit gaps plan, Task 10 (R-R3-49) by
//                J.J. Boyd (KG4VCF): the shortest gap between outgoing
//                vfo, dds and tx_frequency updates to each TCI app, ported
//                from TCPIPtciSocketListener VFOChange / CentreChange /
//                TXFrequencyChange. AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 12 (R-R3-49) by
//                J.J. Boyd (KG4VCF): offer takes each line's gate from the
//                event that queued it, so an if line never has its gate
//                inferred from its position. AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================

/*  TCIServer.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// The shortest gap between outgoing frequency updates to one TCI app.
//
// Thetis gives each app (TCPIPtciSocketListener) three independent gates,
// each a Stopwatch plus a one-shot Timer, all using the one m_nRateLimit the
// server was started with (udTCIRateLimit, 0..1000 ms, default 100):
//   VFOChange          -> vfo (and its if) lines,   m_swVFO / m_tmVFOtimer
//   CentreChange       -> dds (and its if) lines,   m_swCentre / m_tmCentretimer
//   TXFrequencyChange  -> tx_frequency lines,       m_swTXFrequency / m_tmTXFrequency
//
// From Thetis TCIServer.cs:6421-6440 [v2.10.3.15] (VFOChange; CentreChange
// and TXFrequencyChange at 6441-6480 are the same shape):
//   if (m_tmVFOtimer != null)
//   {
//       m_tmVFOtimer.Change(Timeout.Infinite, Timeout.Infinite);
//       m_tmVFOtimer = null;
//   }
//   bool bOK = !m_swVFO.IsRunning || (m_swVFO.IsRunning && m_swVFO.ElapsedMilliseconds > m_nRateLimit);
//   if (bOK)
//   {
//       vfoFrequencyChange(vfod);
//       if (m_nRateLimit > 0) m_swVFO.Restart();
//   }
//   else
//   {
//       m_tmVFOtimer = new System.Threading.Timer(VFOcallback, vfod, m_nRateLimit, Timeout.Infinite);
//   }
//
// So an update goes out at once when the gap since the last immediate send
// has passed; otherwise the latest one waits m_nRateLimit ms from its own
// arrival, and a newer arrival cancels and restarts that wait. The deferred
// send (VFOcallback -> vfoFrequencyChange) does not restart the stopwatch.
//
// NereusSDR shape: the server hands each app its frames once per drain tick
// (already deduplicated by TciVfoCoalescer, Layer 3). One tick's frames of
// one gate are one Thetis event. Time is passed in, so the logic is testable
// without clocks, and the one-shot Timer becomes a due time the server's
// drain tick checks (TciServer's 5 ms tick).
//
// One deliberate difference, recorded in the Task 10 report: Thetis's
// waiting slot holds one VFOData, so a second receiver's update inside the
// gap replaces the first receiver's waiting update, and the first
// receiver's last frequency never reaches the app. Here the waiting frames
// are kept per wire key (latest wins per key, as in TciVfoCoalescer), so the
// timing is Thetis's and every receiver's last frequency still arrives.

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <array>
#include <optional>
#include <vector>

namespace NereusSDR {

class TciUpdateGap {
public:
    // The three Thetis gates.
    enum class Gate { Vfo = 0, Centre = 1, TxFrequency = 2 };
    static constexpr int kGateCount = 3;

    // From Thetis setup.designer.cs:58645-58664 [v2.10.3.15] - udTCIRateLimit
    // Minimum 0, Maximum 1000, Value 100 (label "Rate Limit (ms)").
    static constexpr int kMinGapMs = 0;
    static constexpr int kMaxGapMs = 1000;
    static constexpr int kDefaultGapMs = 100;

    // The setting the operator's value is saved under (Setup > Network >
    // TCI Server > Rate limit). NereusSDR-original key name; the msg/s key
    // it replaces (TciRateLimitMsgsPerSec) is dropped by settings schema v8.
    static constexpr const char* kSettingKey = "TciRateLimitMs";

    // Which gate a TCI line belongs to on its own, or none for every other
    // line. vfo and if go with the VFO gate, dds with the centre gate,
    // tx_frequency and tx_frequency_thetis with the TX frequency gate
    // (Thetis VFOdata thread, TCIServer.cs:1371-1400 [v2.10.3.15]). An if
    // line's gate also depends on the line before it; see gatesOf.
    static std::optional<Gate> gateOf(const QString& frame);

    // The gate of each line of one tick, for lines that arrive without one.
    // As gateOf, except that an if line straight after a dds line for the
    // same receiver belongs to that centre event and so to the centre gate,
    // as Thetis sends a centre change's dds and if together
    // (TCIServer.cs:1378-1382 [v2.10.3.15]). Only the string-only offer
    // below infers gates this way; TciServer passes the gate each event
    // bound to its lines when it queued them (Task 12), because a merged
    // coalescer slot or a line queued outside the coalescer breaks the
    // adjacency (rereview of the fix wave, N2).
    static std::vector<std::optional<Gate>> gatesOf(const QStringList& frames);

    // The wire key a waiting frame is kept under: the command plus the
    // receiver (and channel for vfo / if), so a newer value replaces an
    // older one for the same receiver only.
    static QString keyOf(const QString& frame);

    // Clamped to kMinGapMs..kMaxGapMs. A change starts every gate afresh,
    // as Thetis does by starting a new server (TCIServer.cs:6666-6668
    // [v2.10.3.15]). Lines already waiting move to the new due time: the
    // gate's last immediate send plus the new gap, or the next drain tick
    // if that is already past, so a changed gap applies at once.
    void setGapMs(int ms);
    int gapMs() const { return m_gapMs; }

    // One drain tick's lines for this app, in order, at nowMs. Returns the
    // lines to send now, in order. Lines of a gate whose gap has not passed
    // wait; lines outside the three gates always go.
    QStringList offer(const QStringList& frames, qint64 nowMs);

    // As offer, with each line's gate given (gates.size() == frames.size()).
    // A line with no gate given (nullopt) takes gateOf's answer, so an if
    // line reaches the centre gate only when the centre event said so.
    QStringList offer(const QStringList& frames,
                      const std::vector<std::optional<Gate>>& gates, qint64 nowMs);

    // Waiting lines whose time has come (the Thetis one-shot Timer firing).
    QStringList takeDue(qint64 nowMs);

    // True while any gate holds a waiting line.
    bool hasWaiting() const;

    // Drop every waiting line and stop every gate.
    void clear();

private:
    struct GateState {
        // Thetis Stopwatch: when the last immediate send restarted it.
        std::optional<qint64> restartedAtMs;
        // The last immediate send, kept when setGapMs starts the gate
        // afresh, so waiting lines can be moved to the new gap.
        std::optional<qint64> lastSentAtMs;
        // Thetis one-shot Timer: when the waiting lines go.
        std::optional<qint64> dueAtMs;
        // Waiting lines, latest per key, in first-arrival order.
        QStringList waitingOrder;
        QHash<QString, QString> waiting;
    };

    QStringList releaseWaiting(GateState& gate, const QStringList& skipKeys);

    int m_gapMs{kDefaultGapMs};
    std::array<GateState, kGateCount> m_gates;
};

} // namespace NereusSDR
