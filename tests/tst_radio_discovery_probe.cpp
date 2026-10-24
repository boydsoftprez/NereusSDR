// =================================================================
// tests/tst_radio_discovery_probe.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   HPSDR/clsRadioDiscovery.cs, original licence from Thetis source is included below
//
// =================================================================
// Additional copyright holders whose code is preserved in this file via
// inline markers (upstream file-header block does not name them):
//   Reid Campbell (MI0BOT) — HermesLite 2 board-ID 6 parity test coverage
//     (preserved via inline marker on HPSDRHW::HermesLite QCOMPARE assertion)
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 3Q-2: unicast probe path tests.
//   2026-10-22 - Broadcast walk tests: startDiscovery() returns before the
//                 walk ends, a readable poll leaves the quiet counter as it
//                 was (Thetis clsRadioDiscovery.cs:964-976 [v2.10.3.15]), a
//                 steady stream of datagrams cannot hold an attempt open
//                 past its deadline, cancel, holdoff and the Core's worker
//                 thread. Counts, not milliseconds, after Longpath PRs 9,
//                 10, 251 and 267 by Martin Fischer (OE5SOS). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22 - A quiet period armed while a walk runs delays its next
//                 attempt and its next NIC, never drops them, and
//                 stopDiscovery() during the wait sends nothing after,
//                 and a reply during the wait does not skip the attempt.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22 - A connect hold stops a running walk until it ends, then
//                 the quiet period runs and the walk finishes every NIC;
//                 a scan and a probe started during the hold wait too, and
//                 the hold is keyed by owner. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-22 - Review of PR 621: the connect-hold walk case counts
//                 attempts from the moment the hold begins, reads every
//                 100 ms over a span measured on the clock, and polls for
//                 the walk's end, so a loaded runner cannot fail it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  clsRadioDiscovery.cs

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

#include <QEventLoop>
#include <QObject>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QtTest>
#include "core/RadioDiscovery.h"

#include <atomic>
#include <memory>

using namespace NereusSDR;

// Fake P1 radio that listens on a loopback port and replies to a probe.
// Keeps the test self-contained — no network dependencies.
class FakeP1Probe : public QObject {
    Q_OBJECT
public:
    explicit FakeP1Probe(QObject* parent = nullptr) : QObject(parent) {
        m_socket = new QUdpSocket(this);
        m_socket->bind(QHostAddress::LocalHost, 0);
        connect(m_socket, &QUdpSocket::readyRead, this, &FakeP1Probe::onReadyRead);
    }
    quint16 port() const { return m_socket->localPort(); }
    // Raw P1 probe as it arrived on the wire, for byte-level assertions.
    QByteArray lastP1Probe() const { return m_lastP1Probe; }
private slots:
    void onReadyRead() {
        while (m_socket->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(m_socket->pendingDatagramSize()));
            QHostAddress src;
            quint16 srcPort = 0;
            m_socket->readDatagram(buf.data(), buf.size(), &src, &srcPort);

            // P1 probe is 0xEF 0xFE 0x02 (3 bytes).
            if (buf.size() >= 3 && static_cast<quint8>(buf[0]) == 0xEF
                && static_cast<quint8>(buf[1]) == 0xFE
                && static_cast<quint8>(buf[2]) == 0x02) {
                m_lastP1Probe = buf;
                QByteArray reply(63, 0);
                reply[0] = char(0xEF); reply[1] = char(0xFE); reply[2] = 0x02;
                // MAC bytes 3..8
                reply[3] = 0x00; reply[4] = 0x1C; reply[5] = char(0xC0);
                reply[6] = char(0xA2); reply[7] = 0x14; reply[8] = char(0x8B);
                reply[9] = 75;          // firmware
                reply[10] = 6;          // boardId = HL2
                m_socket->writeDatagram(reply, src, srcPort);
            }
        }
    }
private:
    QUdpSocket* m_socket;
    QByteArray  m_lastP1Probe;
};

// A fake radio for the broadcast walk. The walk under test is pointed at
// it with setScanNicsForTest(), so every probe stays on the loopback. It
// counts the P1 probes it receives and answers the first one as told:
// at once, after a delay, or with a steady stream of one-byte datagrams
// from a thread of its own.
class WalkFakeRadio : public QObject {
    Q_OBJECT
public:
    enum class Answer { Now, After, Flood };

    explicit WalkFakeRadio(Answer answer, int delayMs = 0, QObject* parent = nullptr)
        : QObject(parent), m_answer(answer), m_delayMs(delayMs) {
        m_socket = new QUdpSocket(this);
        m_socket->bind(QHostAddress::LocalHost, 0);
        connect(m_socket, &QUdpSocket::readyRead, this, &WalkFakeRadio::onReadyRead);
    }
    ~WalkFakeRadio() override { stopFlood(); }

    quint16 port() const { return m_socket->localPort(); }
    int p1Probes() const { return m_p1Probes; }
    int floodSent() const { return m_floodSent.load(); }

    // The walk's NIC list: this radio's loopback address, no general
    // broadcast, so nothing leaves the machine.
    QList<RadioDiscovery::ScanNic> nics() const {
        RadioDiscovery::ScanNic nic;
        nic.name = QStringLiteral("loopback-test");
        nic.local = QHostAddress::LocalHost;
        nic.directed = QHostAddress::LocalHost;
        nic.generalBroadcast = false;
        nic.port = port();
        return {nic};
    }

    void stopFlood() {
        m_floodStop.store(true);
        if (m_flood) {
            m_flood->wait();
            m_flood.reset();
        }
    }

private slots:
    void onReadyRead() {
        while (m_socket->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(m_socket->pendingDatagramSize()));
            QHostAddress src;
            quint16 srcPort = 0;
            m_socket->readDatagram(buf.data(), buf.size(), &src, &srcPort);
            if (buf.size() < 3 || static_cast<quint8>(buf[0]) != 0xEF
                || static_cast<quint8>(buf[1]) != 0xFE
                || static_cast<quint8>(buf[2]) != 0x02) {
                continue;   // the P2 probe, not answered
            }
            ++m_p1Probes;
            if (m_p1Probes != 1) {
                continue;   // answer the first probe only
            }
            switch (m_answer) {
            case Answer::Now:
                reply(src, srcPort);
                break;
            case Answer::After:
                QTimer::singleShot(m_delayMs, this, [this, src, srcPort]() { reply(src, srcPort); });
                break;
            case Answer::Flood:
                startFlood(srcPort);
                break;
            }
        }
    }

private:
    void reply(const QHostAddress& to, quint16 port) {
        QByteArray reply(63, 0);
        reply[0] = char(0xEF); reply[1] = char(0xFE); reply[2] = 0x02;
        reply[3] = 0x00; reply[4] = 0x1C; reply[5] = char(0xC0);
        reply[6] = char(0xA2); reply[7] = 0x14; reply[8] = char(0x8B);
        reply[9] = 75;          // firmware
        reply[10] = 6;          // boardId = HL2
        m_socket->writeDatagram(reply, to, port);
    }

    // One-byte datagrams every 2 ms, far more often than any poll window
    // the tests use, until stopFlood().
    void startFlood(quint16 port) {
        m_floodStop.store(false);
        m_flood.reset(QThread::create([this, port]() {
            QUdpSocket out;
            const QByteArray junk(1, 0);
            while (!m_floodStop.load()) {
                out.writeDatagram(junk, QHostAddress::LocalHost, port);
                m_floodSent.fetch_add(1);
                QThread::msleep(2);
            }
        }));
        m_flood->start();
    }

    Answer m_answer;
    int m_delayMs;
    QUdpSocket* m_socket{nullptr};
    int m_p1Probes{0};
    std::unique_ptr<QThread> m_flood;
    std::atomic<bool> m_floodStop{false};
    std::atomic<int> m_floodSent{0};
};

class TstRadioDiscoveryProbe : public QObject {
    Q_OBJECT
private slots:
    // The post-disconnect quiet deadline is process-wide, so an arm in one
    // test function would otherwise defer probes in every later one.
    void init() { RadioDiscovery::clearHoldOffForTest(); }

    void probeReplyFillsRadioInfo() {
        FakeP1Probe radio;
        RadioDiscovery disc;
        QSignalSpy spy(&disc, &RadioDiscovery::radioDiscovered);

        disc.probeAddress(QHostAddress::LocalHost, radio.port(), std::chrono::milliseconds(500));

        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        const auto info = spy.takeFirst().at(0).value<RadioInfo>();
        QCOMPARE(info.boardType, HPSDRHW::HermesLite);
        QCOMPARE(info.firmwareVersion, 75);
        QCOMPARE(info.macAddress, QStringLiteral("00:1C:C0:A2:14:8B"));
        QCOMPARE(info.protocol, ProtocolVersion::Protocol1);
    }

    // Regression guard, 2026-07-27 (ANAN-G2E disconnect lockup).
    //
    // The P1 discovery probe goes to UDP 1024, which is also the Protocol 2
    // "General" command port.  P2 gateware claims any port-1024 datagram whose
    // byte 4 is zero and parses the remainder as a General command
    // (General_CC.v:90/106, TAPR Hermes_Protocol_2_C10_v11.0.5 for ANAN-G2E).
    // With an all-zero tail that write clears HW_timer_enable (byte 38),
    // freezing the board's ~2 s deadman, plus PA_enable and Alex_enable
    // (bytes 58/59) — so a plain LAN scan reconfigures every P2 radio on the
    // subnet and disarms its watchdog.
    //
    // Byte 4 must therefore stay non-zero.  P1 discovery is unaffected: every
    // P1 gateware matches only the command byte and never reads byte 4.
    void p1ProbeDoesNotImpersonateP2GeneralCommand() {
        FakeP1Probe radio;
        RadioDiscovery disc;
        QSignalSpy spy(&disc, &RadioDiscovery::radioDiscovered);

        disc.probeAddress(QHostAddress::LocalHost, radio.port(),
                          std::chrono::milliseconds(500));
        QVERIFY(spy.wait(1000));

        const QByteArray probe = radio.lastP1Probe();
        // Still a well-formed P1 discovery frame.
        QCOMPARE(probe.size(), 63);
        QCOMPARE(static_cast<quint8>(probe[0]), quint8(0xEF));
        QCOMPARE(static_cast<quint8>(probe[1]), quint8(0xFE));
        QCOMPARE(static_cast<quint8>(probe[2]), quint8(0x02));
        // ...but byte 4 is non-zero, so P2 General_CC bails at its command
        // check instead of latching our padding as a config write.
        const quint8 b4 = static_cast<quint8>(probe[4]);
        QVERIFY2(b4 != 0x00,
                 "P1 probe byte 4 is zero — P2 gateware will accept this frame "
                 "as a General command and disarm its watchdog");
        // Byte 4 is also the P2 *command* byte (sdr_receive.v): 2=discovery,
        // 3=set-IP (broadcast-gated, writes EEPROM), 4=erase, 5=program,
        // 6=FPGA reset. Our probes are broadcast, so the pad must not collide
        // with any of them or a plain LAN scan starts issuing P2 commands.
        QVERIFY2(b4 < 0x02 || b4 > 0x06,
                 "P1 probe byte 4 collides with a P2 command (2..6) — a "
                 "broadcast scan would issue discovery/set-IP/erase/reset");
    }

    // Regression guard, 2026-07-27 (ANAN-G2E disconnect lockup).
    //
    // After a disconnect the radio's stop-transition and ~2 s firmware
    // deadman must settle before any probe reaches it (Thetis is silent
    // after its stop; our auto-scan fired 7-15 ms after run=0 and both
    // observed G2E lockups happened in that window).  holdOffScans() must
    // DEFER a probe — it still completes, but only after the quiet period.
    void probeDuringHoldOffIsDeferredNotDropped() {
        FakeP1Probe radio;
        RadioDiscovery disc;
        QSignalSpy spy(&disc, &RadioDiscovery::radioDiscovered);

        constexpr int kQuietMs = 400;
        disc.holdOffScans(std::chrono::milliseconds(kQuietMs));

        QElapsedTimer clock;
        clock.start();
        disc.probeAddress(QHostAddress::LocalHost, radio.port(),
                          std::chrono::milliseconds(500));

        // Not dropped: the reply still arrives...
        QVERIFY(spy.wait(2000));
        QCOMPARE(spy.count(), 1);
        // ...and not early: the probe waited out the quiet period first.
        QVERIFY2(clock.elapsed() >= kQuietMs,
                 qPrintable(QStringLiteral(
                     "probe completed %1 ms after holdOffScans(%2) — it was "
                     "sent inside the post-disconnect quiet period")
                     .arg(clock.elapsed()).arg(kQuietMs)));
    }

    // Codex review, PR #306.  teardownConnection() arms the quiet period
    // twice: once on entry (so nothing scans during teardown) and again after
    // the protocol disconnect completes (so the window is measured from the
    // stop frame, not from teardown entry, which cost 680 ms on the bench).
    // That is only safe because holdOffScans keeps the LATER deadline — a
    // second, shorter arm must never pull the deadline in.
    void holdOffScansExtendsButNeverShortens() {
        RadioDiscovery disc;

        disc.holdOffScans(std::chrono::milliseconds(5000));
        const qint64 afterLong = disc.holdOffRemainingMs();
        QVERIFY(afterLong > 4000);

        // Shorter arm must not shorten the window.
        disc.holdOffScans(std::chrono::milliseconds(50));
        QVERIFY2(disc.holdOffRemainingMs() > 4000,
                 "a shorter holdOffScans() pulled the deadline in — the "
                 "second arm in teardownConnection() would truncate the "
                 "post-stop quiet period");

        // Longer arm must extend it.
        disc.holdOffScans(std::chrono::milliseconds(9000));
        QVERIFY2(disc.holdOffRemainingMs() > 8000,
                 "holdOffScans() failed to extend the deadline");
    }

    // Codex review, PR #306.  RadioModel::discovery() is not the only
    // RadioDiscovery in the process — AddCustomRadioDialog.cpp:589 builds its
    // own.  A per-object deadline would let that dialog probe a
    // just-disconnected radio inside the quiet window, re-entering the
    // post-stop race.  The deadline must be process-wide.
    void holdOffIsSharedAcrossInstances() {
        RadioDiscovery armer;
        armer.holdOffScans(std::chrono::milliseconds(5000));

        RadioDiscovery other;   // e.g. the one AddCustomRadioDialog creates
        QVERIFY2(other.holdOffRemainingMs() > 4000,
                 "a second RadioDiscovery instance ignored the quiet period — "
                 "the Add Radio dialog could probe a stopping radio");
    }

    // ── The broadcast walk (2026-10-22) ──────────────────────────────────
    //
    // Every check below counts events (signals, poll windows, datagrams);
    // none races the wall clock (Longpath PR 267 [@1ca46ba2]: a millisecond
    // threshold between the right and the wrong behaviour failed on a busy
    // runner, a count does not).

    // startDiscovery() starts the walk and returns before it ends: the
    // event loop runs (a zero timer fires) before discoveryFinished, and
    // the reply still arrives through radioDiscovered. Until 2026-10-22 the
    // whole walk ran inside the call (Longpath measured 4656 ms on the GUI
    // thread).
    void startDiscoveryReturnsBeforeTheWalkEnds() {
        // Declared first: the discovery's destructor emits discoveryFinished
        // again, and the slot below must still have its list then.
        QStringList order;
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({2, 4, 40});
        QSignalSpy started(&disc, &RadioDiscovery::discoveryStarted);
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);

        connect(&disc, &RadioDiscovery::discoveryFinished, this,
                [&order]() { order << QStringLiteral("finished"); });
        QTimer::singleShot(0, this, [&order]() { order << QStringLiteral("event loop"); });

        disc.startDiscovery();
        QCOMPARE(started.count(), 1);
        QCOMPARE(finished.count(), 0);
        QVERIFY(disc.scanRunningForTest());

        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(found.count(), 1);
        QCOMPARE(order.value(0), QStringLiteral("event loop"));
        const auto info = found.first().at(0).value<RadioInfo>();
        QCOMPARE(info.macAddress, QStringLiteral("00:1C:C0:A2:14:8B"));
        QCOMPARE(disc.scanStatsForTest().attempts, 2);
        QCOMPARE(radio.p1Probes(), 2);   // one per attempt, as before
        QVERIFY(!disc.scanRunningForTest());
    }

    // From Thetis clsRadioDiscovery.cs:964-976 [v2.10.3.15]: only a poll
    // that comes back not readable advances quietPolls; a readable one
    // neither counts nor resets it. The reply comes after two quiet windows
    // of eight. Thetis's semantics end the attempt after eight quiet
    // windows in all; the old reset-on-readable would need two plus eight.
    // The count does not depend on when the reply lands, only on its
    // landing before the attempt ends.
    void aReadablePollLeavesTheQuietCounterAlone() {
        constexpr int kQuiet = 8;
        constexpr int kPollMs = 50;
        WalkFakeRadio radio(WalkFakeRadio::Answer::After, 5 * kPollMs / 2);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({1, kQuiet, kPollMs});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);

        disc.startDiscovery();
        QVERIFY(finished.wait(10000));

        const RadioDiscovery::ScanStats stats = disc.scanStatsForTest();
        qInfo().noquote() << QStringLiteral("quiet windows %1, readable windows %2, datagrams %3")
                                 .arg(stats.quietPolls).arg(stats.readableWindows).arg(stats.datagrams);
        QVERIFY2(found.count() == 1, "the reply did not arrive before the attempt ended");
        QVERIFY(stats.readableWindows >= 1);
        QCOMPARE(stats.deadlineEnds, 0);
        QCOMPARE(stats.quietPolls, kQuiet);
    }

    // NereusSDR addition (Thetis has no wall-clock limit): a datagram every
    // 2 ms keeps every 50 ms poll window readable, so the quiet counter
    // never moves and only the attempt's deadline can end it. Without the
    // deadline the walk never finishes and the wait below fails.
    void steadyDatagramsCannotHoldAnAttemptOpen() {
        constexpr int kQuiet = 4;
        WalkFakeRadio radio(WalkFakeRadio::Answer::Flood);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({1, kQuiet, 50});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);

        disc.startDiscovery();
        const bool ended = finished.wait(20000);
        radio.stopFlood();
        QVERIFY2(ended, "a steady stream of datagrams held the walk open");

        const RadioDiscovery::ScanStats stats = disc.scanStatsForTest();
        qInfo().noquote() << QStringLiteral("quiet windows %1, readable windows %2, datagrams %3, deadline ends %4")
                                 .arg(stats.quietPolls).arg(stats.readableWindows)
                                 .arg(stats.datagrams).arg(stats.deadlineEnds);
        QCOMPARE(stats.deadlineEnds, 1);
        QVERIFY(stats.quietPolls < kQuiet);
        QVERIFY(stats.datagrams > kQuiet);
        QCOMPARE(finished.count(), 1);
    }

    // stopDiscovery() ends a running walk at once and reports its end once;
    // no probe goes out after it.
    void stopDiscoveryEndsARunningWalkOnce() {
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({3, 5, 40});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);

        disc.startDiscovery();
        QVERIFY(disc.scanRunningForTest());
        disc.stopDiscovery();
        QCOMPARE(finished.count(), 1);
        QVERIFY(!disc.scanRunningForTest());

        // Longer than the whole walk would have taken.
        QTest::qWait(3 * 5 * 40 + 200);
        QCOMPARE(finished.count(), 1);
        QVERIFY(radio.p1Probes() <= 1);
    }

    // A second start while a walk runs joins it: one start, one finish.
    void aStartDuringAWalkJoinsIt() {
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({1, 3, 40});
        QSignalSpy started(&disc, &RadioDiscovery::discoveryStarted);
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);

        disc.startDiscovery();
        disc.startDiscovery();
        QCOMPARE(started.count(), 1);
        QVERIFY(finished.wait(5000));
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(radio.p1Probes(), 1);
    }

    // The post-disconnect quiet period still defers a walk, never drops it.
    void aWalkDuringHoldOffIsDeferredNotDropped() {
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({1, 3, 40});
        QSignalSpy started(&disc, &RadioDiscovery::discoveryStarted);
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);

        disc.holdOffScans(std::chrono::milliseconds(150));
        disc.startDiscovery();
        QCOMPARE(started.count(), 0);
        QCOMPARE(radio.p1Probes(), 0);

        QVERIFY(started.wait(5000));
        QVERIFY(finished.wait(5000));
        QCOMPARE(found.count(), 1);
    }

    // Review of PR 600 (Critical). A disconnect can now land while a walk
    // runs, and the quiet period it arms must hold back the walk's next
    // attempt and its next NIC, not only a walk that starts later. No probe
    // is sent while holdOffRemainingMs() is above zero: the remaining time
    // only falls, so a probe the fake radio has counted was sent before a
    // read that saw time left, and that is the case asserted against. No
    // millisecond threshold decides the result.
    void aHoldOffArmedDuringAWalkDelaysItsNextProbes() {
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        // Two NICs on the same fake radio, two attempts each: four probes.
        disc.setScanNicsForTest(radio.nics() + radio.nics());
        disc.setTimingForTest({2, 3, 100});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);

        // Waits out the quiet period and checks that no probe beyond
        // `probesSoFar` arrives and the walk does not end while it lasts.
        const auto holdsBack = [&](int probesSoFar) {
            while (disc.holdOffRemainingMs() > 0) {
                QCOMPARE(radio.p1Probes(), probesSoFar);
                QCOMPARE(finished.count(), 0);
                QTest::qWait(10);
            }
        };

        disc.startDiscovery();
        QTRY_COMPARE_WITH_TIMEOUT(radio.p1Probes(), 1, 5000);

        // Armed during NIC 1's first attempt: its second attempt waits.
        disc.holdOffScans(std::chrono::milliseconds(500));
        holdsBack(1);
        if (QTest::currentTestFailed()) {
            return;
        }
        QTRY_COMPARE_WITH_TIMEOUT(radio.p1Probes(), 2, 5000);

        // Armed during NIC 1's last attempt: NIC 2's first attempt waits.
        disc.holdOffScans(std::chrono::milliseconds(500));
        holdsBack(2);
        if (QTest::currentTestFailed()) {
            return;
        }

        // Then the rest of the walk runs, and it ends once.
        QVERIFY(finished.wait(5000));
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(radio.p1Probes(), 4);
        QCOMPARE(disc.scanStatsForTest().attempts, 4);
        QCOMPARE(found.count(), 1);
    }

    // stopDiscovery() while an attempt waits out the quiet period ends the
    // walk at once, and nothing is sent when the quiet period ends.
    void stopDiscoveryDuringAHoldOffWaitSendsNothingAfter() {
        constexpr int kQuiet = 3;
        constexpr int kPollMs = 40;
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({3, kQuiet, kPollMs});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);

        disc.startDiscovery();
        QTRY_COMPARE_WITH_TIMEOUT(radio.p1Probes(), 1, 5000);
        disc.holdOffScans(std::chrono::milliseconds(400));

        // The first attempt's quiet budget runs out; the second attempt
        // would start in the same call and is held back.
        QTRY_VERIFY_WITH_TIMEOUT(disc.scanStatsForTest().quietPolls >= kQuiet, 5000);
        QVERIFY(disc.holdOffRemainingMs() > 0);
        QCOMPARE(disc.scanStatsForTest().attempts, 1);

        disc.stopDiscovery();
        QCOMPARE(finished.count(), 1);
        QVERIFY(!disc.scanRunningForTest());

        // Past the quiet period, then longer than the rest of the walk.
        QTRY_COMPARE_WITH_TIMEOUT(disc.holdOffRemainingMs(), qint64(0), 5000);
        QTest::qWait(3 * kQuiet * kPollMs + 200);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(radio.p1Probes(), 1);
        QCOMPARE(disc.scanStatsForTest().attempts, 1);
    }

    // Re-review of PR 600: a reply that arrives while an attempt waits out
    // the quiet period must not end that attempt unsent. The quiet count is
    // still full from the attempt that ended, so a poll window opened by
    // the reply used to end the waiting attempt and, on the last NIC, the
    // walk. One NIC, two attempts: the reply to probe 1 comes during the
    // wait, and probe 2 must still go out after it.
    void aReplyDuringAHoldOffWaitDoesNotSkipTheHeldAttempt() {
        constexpr int kQuiet = 3;
        constexpr int kPollMs = 40;
        // Answers probe 1 after 300 ms: after the first attempt's quiet
        // budget (120 ms) and inside the 1000 ms quiet period armed below.
        WalkFakeRadio radio(WalkFakeRadio::Answer::After, 300);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({2, kQuiet, kPollMs});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);
        bool replyInsideTheWait = false;
        connect(&disc, &RadioDiscovery::radioDiscovered, this,
                [&disc, &replyInsideTheWait]() {
            replyInsideTheWait = disc.holdOffRemainingMs() > 0
                && disc.scanStatsForTest().attempts == 1;
        });

        disc.startDiscovery();
        QTRY_COMPARE_WITH_TIMEOUT(radio.p1Probes(), 1, 5000);
        disc.holdOffScans(std::chrono::milliseconds(1000));

        // No probe and no end while the quiet period lasts (see
        // aHoldOffArmedDuringAWalkDelaysItsNextProbes for why this read
        // order makes it a count, not a time).
        while (disc.holdOffRemainingMs() > 0) {
            QCOMPARE(radio.p1Probes(), 1);
            QCOMPARE(finished.count(), 0);
            QTest::qWait(10);
        }
        QCOMPARE(found.count(), 1);
        QVERIFY2(replyInsideTheWait, "the reply did not land during the wait; the case tested nothing");

        QVERIFY(finished.wait(5000));
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(radio.p1Probes(), 2);
        QCOMPARE(disc.scanStatsForTest().attempts, 2);
    }

    // Review of PR 600 (Important 2). A connect hold has no deadline: the
    // walk sends nothing for as long as it lasts, however many attempt
    // lengths that is, then waits the quiet period endConnectHold() arms,
    // then finishes every NIC. Two NICs, two attempts each: four probes.
    void aConnectHoldStopsAWalkUntilItEndsAndTheQuietPeriodPasses() {
        constexpr int kQuiet = 3;
        constexpr int kPollMs = 100;
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics() + radio.nics());
        disc.setTimingForTest({2, kQuiet, kPollMs});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        int owner = 0;

        disc.startDiscovery();
        QTRY_VERIFY_WITH_TIMEOUT(radio.p1Probes() >= 1, 5000);
        // The hold may begin after any attempt short of the last: a loaded
        // runner can be slow to get here. Counted where the walk sends
        // (scanStatsForTest), so a probe still in flight cannot blur it.
        RadioDiscovery::beginConnectHold(&owner);
        const int attemptsAtHold = disc.scanStatsForTest().attempts;
        QVERIFY2(disc.isScanRunning() && attemptsAtHold < 4,
                 "the walk ended before the hold began; nothing tested");
        QVERIFY(RadioDiscovery::connectHoldActive());
        QVERIFY(disc.holdOffRemainingMs() > 0);

        // Longer than three attempts at their hard deadline pass, measured
        // on the clock and read every 100 ms; the walk neither sends nor
        // ends. A slow runner only makes the wait longer.
        const qint64 kSpanMs = 3LL * RadioDiscovery::attemptDeadlineFactorForTest()
                               * kQuiet * kPollMs;
        QElapsedTimer held;
        held.start();
        while (held.elapsed() < kSpanMs) {
            QTest::qWait(100);
            QCOMPARE(disc.scanStatsForTest().attempts, attemptsAtHold);
            QCOMPARE(finished.count(), 0);
        }
        QVERIFY(disc.isScanRunning());

        // The connection is up: the quiet period starts now, and the walk
        // still waits it out (a count, not a time: see
        // aHoldOffArmedDuringAWalkDelaysItsNextProbes).
        RadioDiscovery::endConnectHold(&owner, std::chrono::milliseconds(1000));
        QVERIFY(!RadioDiscovery::connectHoldActive());
        QVERIFY(disc.holdOffRemainingMs() > 0);
        while (disc.holdOffRemainingMs() > 0) {
            QCOMPARE(disc.scanStatsForTest().attempts, attemptsAtHold);
            QCOMPARE(finished.count(), 0);
            QTest::qWait(10);
        }

        // Then the same walk resumes and walks both NICs.
        // Polled, not waited on: a slow runner may see it end first.
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(disc.scanStatsForTest().attempts, 4);
        QTRY_COMPARE_WITH_TIMEOUT(radio.p1Probes(), 4, 2000);
    }

    // A scan and a unicast probe asked for during a connect hold wait for
    // it and the quiet period after it, then run.
    void aScanAndAProbeAskedForDuringAConnectHoldWait() {
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        RadioDiscovery disc;
        disc.setScanNicsForTest(radio.nics());
        disc.setTimingForTest({1, 3, 40});
        QSignalSpy started(&disc, &RadioDiscovery::discoveryStarted);
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);
        FakeP1Probe target;
        int owner = 0;

        RadioDiscovery::beginConnectHold(&owner);
        disc.startDiscovery();
        disc.probeAddress(QHostAddress::LocalHost, target.port(), std::chrono::milliseconds(150));
        for (int i = 0; i < 40; ++i) {
            QTest::qWait(10);
            QCOMPARE(started.count(), 0);
            QCOMPARE(radio.p1Probes(), 0);
            QVERIFY(target.lastP1Probe().isEmpty());
        }

        RadioDiscovery::endConnectHold(&owner, std::chrono::milliseconds(200));
        while (disc.holdOffRemainingMs() > 0) {
            QCOMPARE(started.count(), 0);
            QCOMPARE(radio.p1Probes(), 0);
            QVERIFY(target.lastP1Probe().isEmpty());
            QTest::qWait(10);
        }
        QVERIFY(finished.wait(5000));
        QCOMPARE(started.count(), 1);
        QCOMPARE(radio.p1Probes(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!target.lastP1Probe().isEmpty(), 5000);
    }

    // The hold is keyed by owner: a second begin is the same hold, one end
    // releases it, and an end with no begin arms no quiet period (a
    // connection that ends after its hold was already released, or a model
    // that never connected).
    void aConnectHoldIsKeyedByOwner() {
        RadioDiscovery disc;
        int a = 0;
        int b = 0;

        RadioDiscovery::endConnectHold(&a, std::chrono::milliseconds(3000));
        QCOMPARE(disc.holdOffRemainingMs(), qint64(0));

        RadioDiscovery::beginConnectHold(&a);
        RadioDiscovery::beginConnectHold(&a);
        RadioDiscovery::beginConnectHold(&b);
        RadioDiscovery::endConnectHold(&a, std::chrono::milliseconds(0));
        QVERIFY(RadioDiscovery::connectHoldActive());   // b still holds
        RadioDiscovery::endConnectHold(&b, std::chrono::milliseconds(0));
        QVERIFY(!RadioDiscovery::connectHoldActive());
        QCOMPARE(disc.holdOffRemainingMs(), qint64(0));
    }

    // The Core runs discovery on a worker thread with its own event loop
    // until discoveryFinished (DaemonApp::attemptRadioDiscovery). The walk
    // must finish and find the radio there too.
    void aWalkOnAWorkerThreadFindsTheRadio() {
        WalkFakeRadio radio(WalkFakeRadio::Answer::Now);
        const QList<RadioDiscovery::ScanNic> nics = radio.nics();
        auto result = std::make_shared<QList<RadioInfo>>();
        std::unique_ptr<QThread> worker(QThread::create([result, nics]() {
            RadioDiscovery discovery;
            discovery.setScanNicsForTest(nics);
            discovery.setTimingForTest({1, 3, 40});
            QEventLoop loop;
            QObject::connect(&discovery, &RadioDiscovery::discoveryFinished,
                             &loop, &QEventLoop::quit);
            QTimer::singleShot(0, &discovery, [&discovery]() { discovery.startDiscovery(); });
            QTimer::singleShot(10000, &loop, &QEventLoop::quit);
            loop.exec();
            *result = discovery.discoveredRadios();
        }));
        QSignalSpy done(worker.get(), &QThread::finished);
        worker->start();
        QVERIFY(done.wait(15000));
        worker->wait();
        QCOMPARE(result->size(), 1);
        QCOMPARE(result->first().macAddress, QStringLiteral("00:1C:C0:A2:14:8B"));
    }

    void timeoutEmitsProbeFailed() {
        RadioDiscovery disc;
        QSignalSpy spy(&disc, &RadioDiscovery::probeFailed);
        // Localhost port that nothing is listening on — guaranteed timeout.
        disc.probeAddress(QHostAddress::LocalHost, 1, std::chrono::milliseconds(150));
        QVERIFY(spy.wait(500));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TstRadioDiscoveryProbe)
#include "tst_radio_discovery_probe.moc"
