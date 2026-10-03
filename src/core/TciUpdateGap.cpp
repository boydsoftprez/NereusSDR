// =================================================================
// src/core/TciUpdateGap.cpp  (NereusSDR)
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

#include "TciUpdateGap.h"

#include <algorithm>
#include <vector>

namespace NereusSDR {

namespace {

// The command name of a TCI line: everything before ':' (or ';').
QString commandOf(const QString& frame)
{
    qsizetype end = frame.indexOf(QLatin1Char(':'));
    if (end < 0) {
        end = frame.indexOf(QLatin1Char(';'));
    }
    return (end < 0 ? frame : frame.left(end)).trimmed().toLower();
}

// The arguments of a TCI line, without the trailing ';'.
QStringList argsOf(const QString& frame)
{
    const qsizetype colon = frame.indexOf(QLatin1Char(':'));
    if (colon < 0) {
        return {};
    }
    QString rest = frame.mid(colon + 1);
    if (rest.endsWith(QLatin1Char(';'))) {
        rest.chop(1);
    }
    return rest.split(QLatin1Char(','));
}

} // namespace

std::optional<TciUpdateGap::Gate> TciUpdateGap::gateOf(const QString& frame)
{
    // From Thetis TCIServer.cs:1371-1400 [v2.10.3.15] - what each event
    // sends: SendTXInfo -> sendTXFrequencyChanged; cen -> sendDDS (+ sendIF);
    // otherwise sendIF + sendVFO.
    const QString cmd = commandOf(frame);
    if (cmd == QLatin1String("vfo") || cmd == QLatin1String("if")) {
        return Gate::Vfo;
    }
    if (cmd == QLatin1String("dds")) {
        return Gate::Centre;
    }
    if (cmd == QLatin1String("tx_frequency") || cmd == QLatin1String("tx_frequency_thetis")) {
        return Gate::TxFrequency;
    }
    return std::nullopt;
}

std::vector<std::optional<TciUpdateGap::Gate>> TciUpdateGap::gatesOf(const QStringList& frames)
{
    // The event that made an if line decides its gate. From Thetis
    // TCIServer.cs:1378-1382 [v2.10.3.15], a centre event sends its dds and
    // then its if, both under CentreChange's gate:
    //   if (vfoData.cen)
    //   {
    //       sendDDS(vfoData.rx, (long)(vfoData.centreMHz * 1e6));
    //       if (vfoData.sendIF) sendIF(vfoData.rx, vfoData.chan, (int)vfoData.offsetHz);
    //   }
    // while a VFO event sends if then vfo under VFOChange's gate
    // (TCIServer.cs:1397-1398 [v2.10.3.15]). A tick keeps the order the
    // lines were made in (TciVfoCoalescer drains in first-arrival order), so
    // an if line straight after a dds line for the same receiver came from
    // that centre event; any other if line came from a VFO event.
    std::vector<std::optional<Gate>> gates;
    gates.reserve(static_cast<std::size_t>(frames.size()));
    for (qsizetype i = 0; i < frames.size(); ++i) {
        std::optional<Gate> gate = gateOf(frames.at(i));
        if (gate == Gate::Vfo && i > 0
            && commandOf(frames.at(i)) == QLatin1String("if")
            && commandOf(frames.at(i - 1)) == QLatin1String("dds")) {
            const QStringList ifArgs = argsOf(frames.at(i));
            const QStringList ddsArgs = argsOf(frames.at(i - 1));
            if (!ifArgs.isEmpty() && !ddsArgs.isEmpty()
                && ifArgs.at(0).trimmed() == ddsArgs.at(0).trimmed()) {
                gate = Gate::Centre;
            }
        }
        gates.push_back(gate);
    }
    return gates;
}

QString TciUpdateGap::keyOf(const QString& frame)
{
    const QString cmd = commandOf(frame);
    const QStringList args = argsOf(frame);
    if ((cmd == QLatin1String("vfo") || cmd == QLatin1String("if")) && args.size() >= 2) {
        return cmd + QLatin1Char(':') + args.at(0).trimmed() + QLatin1Char(',')
             + args.at(1).trimmed();
    }
    if (cmd == QLatin1String("dds") && !args.isEmpty()) {
        return cmd + QLatin1Char(':') + args.at(0).trimmed();
    }
    return cmd;
}

void TciUpdateGap::setGapMs(int ms)
{
    m_gapMs = std::clamp(ms, kMinGapMs, kMaxGapMs);
    for (GateState& gate : m_gates) {
        // Lines already waiting take the new gap too, measured from the
        // gate's last immediate send, the point Thetis's stopwatch measures
        // from. A due time already past goes on the next drain tick.
        if (gate.dueAtMs.has_value() && gate.lastSentAtMs.has_value()) {
            gate.dueAtMs = *gate.lastSentAtMs + m_gapMs;
        }
        gate.restartedAtMs.reset();
    }
}

QStringList TciUpdateGap::releaseWaiting(GateState& gate, const QStringList& skipKeys)
{
    QStringList out;
    for (const QString& key : std::as_const(gate.waitingOrder)) {
        if (!skipKeys.contains(key)) {
            out << gate.waiting.value(key);
        }
    }
    gate.waitingOrder.clear();
    gate.waiting.clear();
    gate.dueAtMs.reset();
    return out;
}

QStringList TciUpdateGap::offer(const QStringList& frames, qint64 nowMs)
{
    return offer(frames, gatesOf(frames), nowMs);
}

QStringList TciUpdateGap::offer(const QStringList& frames,
                                const std::vector<std::optional<Gate>>& givenGates,
                                qint64 nowMs)
{
    // Task 12 (R-R3-49): the event that queued a line named its gate; a
    // line that came without one falls back to its command.
    std::vector<std::optional<Gate>> gates;
    gates.reserve(static_cast<std::size_t>(frames.size()));
    for (qsizetype i = 0; i < frames.size(); ++i) {
        const auto idx = static_cast<std::size_t>(i);
        if (idx < givenGates.size() && givenGates[idx].has_value()) {
            gates.push_back(givenGates[idx]);
        } else {
            gates.push_back(gateOf(frames.at(i)));
        }
    }

    // Which gates this tick carries, and the keys it carries for each.
    std::array<bool, kGateCount> present{};
    std::array<QStringList, kGateCount> keys;
    for (qsizetype i = 0; i < frames.size(); ++i) {
        if (const auto gate = gates[static_cast<std::size_t>(i)]) {
            const int g = static_cast<int>(*gate);
            present[g] = true;
            keys[g] << keyOf(frames.at(i));
        }
    }

    // One Thetis event per gate present.
    std::array<bool, kGateCount> sendNow{};
    std::array<QStringList, kGateCount> released;
    for (int g = 0; g < kGateCount; ++g) {
        if (!present[g]) {
            continue;
        }
        GateState& gate = m_gates[g];
        // From Thetis TCIServer.cs:6423-6427 [v2.10.3.15]: a new event
        // cancels the waiting timer.
        gate.dueAtMs.reset();

        // From Thetis TCIServer.cs:6429 [v2.10.3.15]:
        //   bool bOK = !m_swVFO.IsRunning || (m_swVFO.IsRunning && m_swVFO.ElapsedMilliseconds > m_nRateLimit);
        const bool ok = !gate.restartedAtMs.has_value()
                     || (nowMs - *gate.restartedAtMs) > m_gapMs;
        if (ok) {
            // From Thetis TCIServer.cs:6431-6435 [v2.10.3.15]: send now and,
            // with a gap set, restart the stopwatch.
            sendNow[g] = true;
            // Waiting lines for other receivers go with it; a waiting line
            // this tick replaces is dropped (latest wins).
            released[g] = releaseWaiting(gate, keys[g]);
            if (m_gapMs > 0) {
                gate.restartedAtMs = nowMs;
            }
            gate.lastSentAtMs = nowMs;
        } else {
            // From Thetis TCIServer.cs:6436-6439 [v2.10.3.15]: wait
            // m_nRateLimit ms from this event.
            gate.dueAtMs = nowMs + m_gapMs;
        }
    }

    QStringList out;
    std::array<bool, kGateCount> releasedEmitted{};
    for (qsizetype i = 0; i < frames.size(); ++i) {
        const QString& frame = frames.at(i);
        const auto gate = gates[static_cast<std::size_t>(i)];
        if (!gate) {
            out << frame;
            continue;
        }
        const int g = static_cast<int>(*gate);
        if (sendNow[g]) {
            // Older waiting lines go ahead of this tick's lines.
            if (!releasedEmitted[g]) {
                out << released[g];
                releasedEmitted[g] = true;
            }
            out << frame;
            continue;
        }
        GateState& state = m_gates[g];
        const QString key = keyOf(frame);
        if (!state.waiting.contains(key)) {
            state.waitingOrder << key;
        }
        state.waiting.insert(key, frame);
    }
    return out;
}

QStringList TciUpdateGap::takeDue(qint64 nowMs)
{
    QStringList out;
    for (GateState& gate : m_gates) {
        if (gate.dueAtMs.has_value() && nowMs >= *gate.dueAtMs) {
            // From Thetis TCIServer.cs:6411-6415 [v2.10.3.15] VFOcallback:
            // the timer sends the waiting update; the stopwatch is not
            // restarted.
            out << releaseWaiting(gate, {});
        }
    }
    return out;
}

bool TciUpdateGap::hasWaiting() const
{
    return std::any_of(m_gates.cbegin(), m_gates.cend(),
                       [](const GateState& gate) { return !gate.waitingOrder.isEmpty(); });
}

void TciUpdateGap::clear()
{
    for (GateState& gate : m_gates) {
        gate = GateState{};
    }
}

} // namespace NereusSDR
