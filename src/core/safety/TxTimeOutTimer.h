// =================================================================
// src/core/safety/TxTimeOutTimer.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/TimeOutTimerManager.cs,
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - iPhone app plan Task 38 (R-IOS-04, D29, R-IOS-21) by
//                J.J. Boyd (KG4VCF): the transmit time-out, ported from
//                TimeOutTimerManager (the one-second tick, the MOX time-out
//                from the rising edge of MOX, the ping time-out). The limits
//                come from a settings source read at every tick, so the
//                limit for whoever is keyed (the station and computers, or
//                phones and tablets) applies at once. AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

/*  TimeOutTimerManager.cs

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

// The transmit time-out (iPhone app plan Task 38, R-IOS-04, D29).
//
// Thetis's TimeOutTimerManager is a static class with a background thread
// that wakes once a second. While MOX is on and either time-out is enabled
// it checks:
//   - the MOX time-out: the seconds since the rising edge of MOX reach the
//     limit;
//   - the ping time-out (only when the MOX one has not fired): it pings its
//     host (900 ms at most), and a success moves the last good ping to now;
//     it fires when the last good ping is older than its limit.
// When either fires it calls back with "MOX" or "PING", every second while
// the condition holds; console.cs's timeOutTimer then runs
// StopAllTx(msg + " Time Out Timer").
//
// NereusSDR: a QObject on the owner's thread.
//   - The tick is a one-second QTimer that runs while MOX is on (Thetis's
//     ticker runs always and does nothing unkeyed); tick() is public so a
//     test drives it with an injected clock.
//   - The limits come from a settings source read at every tick (Thetis's
//     setup page pushes them through MoxTimeOut / PingTimeOut; here the
//     owner answers with the limit for whoever is keyed now).
//   - The ping runs through an injected pinger that answers later, so the
//     tick never blocks the event loop; the default runs the system's ping
//     once. A reply counts only for the key it was sent in.
//   - The clock is monotonic (Thetis reads DateTime.UtcNow), so a wall
//     clock change never fires or holds off the time-out.
//
// =================================================================
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <functional>

class QTimer;

namespace NereusSDR {

class TxTimeOutTimer : public QObject {
    Q_OBJECT

public:
    /// The limits that apply to the key now. Thetis's MoxTimeOut(seconds,
    /// enabled) and PingTimeOut(host, seconds, enabled), as one value.
    struct Settings {
        bool moxEnabled{false};
        int moxSeconds{kDefaultSeconds};
        bool pingEnabled{false};
        int pingSeconds{kDefaultSeconds};
        QString pingHost{QStringLiteral("8.8.8.8")};
    };

    // From Thetis setup.designer.cs:10294 [v2.10.3.15]: udMoxToTSeconds
    // Value 180, with Maximum 1800 and Minimum 30 at 10279-10288.
    // udPingToTSeconds has the same three values (10226-10245).
    static constexpr int kDefaultSeconds = 180;
    static constexpr int kMinimumSeconds = 30;
    static constexpr int kMaximumSeconds = 1800;
    // From Thetis TimeOutTimerManager.cs:183 [v2.10.3.15]:
    //   PingReply pr = _ping.Send(_hostAddress, 900); // wait 900ms max
    static constexpr int kPingWaitMs = 900;
    // From Thetis TimeOutTimerManager.cs:202 [v2.10.3.15]:
    //   Thread.Sleep(1000 - pingRoundTrip);
    static constexpr int kTickMs = 1000;

    /// Milliseconds on a monotonic clock.
    using Clock = std::function<qint64()>;
    /// Reads the limits for the key now.
    using SettingsSource = std::function<Settings()>;
    /// Pings `host` once and calls `done(success)` later, on the owner's
    /// thread, within kPingWaitMs.
    using Pinger = std::function<void(const QString& host, std::function<void(bool)> done)>;

    explicit TxTimeOutTimer(QObject* parent = nullptr);
    ~TxTimeOutTimer() override;

    void setSettingsSource(SettingsSource source);
    /// Test seam: the clock. The default is a monotonic QElapsedTimer.
    void setClock(Clock clock);
    /// Test seam: the pinger. The default is systemPing.
    void setPinger(Pinger pinger);

    /// Thetis onMox: connect to MoxController::moxChanged.
    void onMox(int rx, bool oldMox, bool newMox);

    /// One tick of Thetis's tickLoop. The QTimer calls it once a second
    /// while MOX is on; a test calls it itself.
    void tick();

    bool isMox() const { return m_mox; }
    /// True while the one-second QTimer runs (MOX on).
    bool isTicking() const;

    /// Whole seconds left before the MOX time-out fires, rounded up; -1
    /// when MOX is off or the MOX time-out is off for the key now.
    int remainingSeconds() const;

    /// Runs the system's ping once (at most kPingWaitMs), answering on the
    /// owner's thread. `host` must already be a valid address.
    static void systemPing(QObject* context, const QString& host, std::function<void(bool)> done);

signals:
    /// "MOX" or "PING", with the limit that fired. Thetis's callback
    /// (ToTOccured); like it, repeated every tick while the condition holds.
    void timedOut(const QString& which, int limitSeconds);

private:
    qint64 now() const;
    Settings settings() const;

    QElapsedTimer m_monotonic;
    SettingsSource m_settingsSource;
    Clock m_clock;
    Pinger m_pinger;
    QTimer* m_ticker{nullptr};

    // From Thetis TimeOutTimerManager.cs:67-69 [v2.10.3.15]:
    //   private static DateTime _lastPing; _lastMox; bool _mox;
    qint64 m_lastPingMs{0};
    qint64 m_lastMoxMs{0};
    bool m_mox{false};
    // Each rising edge starts a new key; a ping reply from an older key is
    // ignored.
    quint64 m_keyGeneration{0};
};

} // namespace NereusSDR
