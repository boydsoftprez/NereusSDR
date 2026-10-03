// =================================================================
// src/core/safety/TxTimeOutTimer.cpp  (NereusSDR)
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

#include "core/safety/TxTimeOutTimer.h"

#include <QHostAddress>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <memory>
#include <utility>

namespace NereusSDR {

// From Thetis TimeOutTimerManager.cs:77-99 [v2.10.3.15] (Initialise):
//   _lastMox = DateTime.UtcNow; _lastPing = _lastMox;
//   _moxTimeOutEnabled = false; _pingTimeoutEnabled = false;
//   _mox = _console.MOX; _console.MoxChangeHandlers += onMox;
//   //ticker always runs, 1 second interval
// Both time-outs start off (the settings source's defaults); the owner
// connects onMox to MoxController::moxChanged.
TxTimeOutTimer::TxTimeOutTimer(QObject* parent)
    : QObject(parent)
    , m_ticker(new QTimer(this))
{
    m_monotonic.start();
    m_lastMoxMs = now();
    m_lastPingMs = m_lastMoxMs;
    m_ticker->setInterval(kTickMs);
    m_ticker->setTimerType(Qt::PreciseTimer);
    connect(m_ticker, &QTimer::timeout, this, &TxTimeOutTimer::tick);
    m_pinger = [this](const QString& host, std::function<void(bool)> done) {
        systemPing(this, host, std::move(done));
    };
}

TxTimeOutTimer::~TxTimeOutTimer()
{
    // A ping still running answers nobody: its process is a child and goes
    // with this object, without calling back into a half-destroyed timer.
    const QList<QProcess*> pings = findChildren<QProcess*>(QString(), Qt::FindDirectChildrenOnly);
    for (QProcess* ping : pings) {
        ping->blockSignals(true);
    }
}

void TxTimeOutTimer::setSettingsSource(SettingsSource source)
{
    m_settingsSource = std::move(source);
}

void TxTimeOutTimer::setClock(Clock clock)
{
    m_clock = std::move(clock);
    m_lastMoxMs = now();
    m_lastPingMs = m_lastMoxMs;
}

void TxTimeOutTimer::setPinger(Pinger pinger)
{
    m_pinger = std::move(pinger);
}

qint64 TxTimeOutTimer::now() const
{
    return m_clock ? m_clock() : m_monotonic.elapsed();
}

TxTimeOutTimer::Settings TxTimeOutTimer::settings() const
{
    return m_settingsSource ? m_settingsSource() : Settings{};
}

bool TxTimeOutTimer::isTicking() const
{
    return m_ticker->isActive();
}

// From Thetis TimeOutTimerManager.cs:146-158 [v2.10.3.15]:
//   public static void onMox(int rx, bool oldMox, bool newMox)
//   {
//       lock (_locker)
//       {
//           _mox = newMox;
//
//           if (newMox && !oldMox)
//           {
//               _lastMox = DateTime.UtcNow;
//               _lastPing = _lastMox;
//           }
//       }
//   }
// NereusSDR: the one-second tick runs while MOX is on, from the rising edge.
void TxTimeOutTimer::onMox(int rx, bool oldMox, bool newMox)
{
    Q_UNUSED(rx);
    m_mox = newMox;

    if (newMox && !oldMox) {
        m_lastMoxMs = now();
        m_lastPingMs = m_lastMoxMs;
        ++m_keyGeneration;
        m_ticker->start();
    }
    if (!newMox) {
        m_ticker->stop();
    }
}

// From Thetis TimeOutTimerManager.cs:159-204 [v2.10.3.15] (tickLoop), one
// pass of the loop:
//   if (_mox && (_moxTimeOutEnabled || _pingTimeoutEnabled))
//   {
//       DateTime now = DateTime.UtcNow;
//
//       // basic mox ToT
//       if (_moxTimeOutEnabled)
//           totMox = (now - _lastMox).TotalSeconds >= _moxTimeOutSeconds;
//
//       // ping ToT
//       if (!totMox && _pingTimeoutEnabled)
//       {
//           try
//           {
//               PingReply pr = _ping.Send(_hostAddress, 900); // wait 900ms max
//               if (pr.Status == IPStatus.Success)
//                   _lastPing = now;
//               pingRoundTrip = (int)pr.RoundtripTime; // reduce sleep time by the time it took for the ping
//           }
//           catch
//           {
//           }
//           totPing = (now - _lastPing).TotalSeconds >= _pingTimeOutSeconds;
//       }
//   }
//
//   if (totMox || totPing)
//   {
//       string msg = totMox ? "MOX" : "PING";
//       _callback?.Invoke(msg); // ok to keep calling this every second if needed
//   }
// NereusSDR: the ping answers later (never a blocking wait on the event
// loop), so the ping decision is taken when it answers, with this tick's
// `now` and limits, as Thetis takes it after its Send returns. A reply
// that arrives after the key ended, or from an earlier key, is ignored.
void TxTimeOutTimer::tick()
{
    if (!m_mox) {
        return;
    }
    const Settings limits = settings();
    if (!limits.moxEnabled && !limits.pingEnabled) {
        return;
    }
    const qint64 tickNow = now();

    // basic mox ToT
    bool totMox = false;
    if (limits.moxEnabled) {
        totMox = (tickNow - m_lastMoxMs) >= static_cast<qint64>(limits.moxSeconds) * 1000;
    }
    if (totMox) {
        // _callback?.Invoke(msg); // ok to keep calling this every second if needed
        emit timedOut(QStringLiteral("MOX"), limits.moxSeconds);
        return;
    }

    // ping ToT
    if (limits.pingEnabled && m_pinger) {
        const quint64 generation = m_keyGeneration;
        const int pingSeconds = limits.pingSeconds;
        QPointer<TxTimeOutTimer> self(this);
        m_pinger(limits.pingHost, [self, generation, tickNow, pingSeconds](bool success) {
            if (!self || generation != self->m_keyGeneration || !self->m_mox) {
                return;
            }
            if (success) {
                self->m_lastPingMs = std::max(self->m_lastPingMs, tickNow);
            }
            const bool totPing =
                (tickNow - self->m_lastPingMs) >= static_cast<qint64>(pingSeconds) * 1000;
            if (totPing) {
                emit self->timedOut(QStringLiteral("PING"), pingSeconds);
            }
        });
    }
}

int TxTimeOutTimer::remainingSeconds() const
{
    if (!m_mox) {
        return -1;
    }
    const Settings limits = settings();
    if (!limits.moxEnabled) {
        return -1;
    }
    const qint64 leftMs =
        static_cast<qint64>(limits.moxSeconds) * 1000 - (now() - m_lastMoxMs);
    if (leftMs <= 0) {
        return 0;
    }
    return static_cast<int>((leftMs + 999) / 1000);
}

// NereusSDR-original: Thetis uses .NET's Ping class. Qt has no ICMP
// socket, so this runs the system's ping once and kills it after
// kPingWaitMs. Unix ping exits 0 only when a reply came back; Windows ping
// can exit 0 on "Destination host unreachable", so there a reply line
// ("TTL=") is required too. The host is passed as one argument, never
// through a shell, and must parse as an address.
void TxTimeOutTimer::systemPing(QObject* context, const QString& host,
                                std::function<void(bool)> done)
{
    QHostAddress address;
    if (context == nullptr || !address.setAddress(host)) {
        QTimer::singleShot(0, context, [done = std::move(done)]() { done(false); });
        return;
    }

    auto* process = new QProcess(context);
    auto answered = std::make_shared<bool>(false);
    auto answer = [process, answered, done = std::move(done)](bool success) {
        if (*answered) {
            return;
        }
        *answered = true;
        done(success);
        process->deleteLater();
    };

    QStringList arguments;
#ifdef Q_OS_WIN
    arguments << QStringLiteral("-n") << QStringLiteral("1") << QStringLiteral("-w")
              << QString::number(kPingWaitMs) << address.toString();
#else
    arguments << QStringLiteral("-c") << QStringLiteral("1") << address.toString();
#endif

    connect(process, &QProcess::finished, process,
            [process, answer](int exitCode, QProcess::ExitStatus status) {
                bool success = status == QProcess::NormalExit && exitCode == 0;
#ifdef Q_OS_WIN
                success = success
                    && process->readAllStandardOutput().contains(QByteArrayLiteral("TTL="));
#else
                Q_UNUSED(process);
#endif
                answer(success);
            });
    connect(process, &QProcess::errorOccurred, process,
            [answer](QProcess::ProcessError) { answer(false); });
    QTimer::singleShot(kPingWaitMs, process, [process, answer]() {
        process->kill();
        answer(false);
    });
    process->start(QStringLiteral("ping"), arguments);
}

} // namespace NereusSDR
