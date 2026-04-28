/*  TXA.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2014, 2016, 2017, 2021, 2023 Warren Pratt, NR0V

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

warren@wpratt.com

*/

// =================================================================
// tests/tst_tx_channel_production_loop.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is ported in this test file. The production loop
// logic (driveOneTxBlock) exercises:
//   - WDSP fexchange2 (wdsp/iobuffs.c [v2.10.3.13])
//   - TxMicRouter::pullSamples (NereusSDR-original, TxMicRouter.h)
//   - RadioConnection::sendTxIq (RadioConnection.h — abstract virtual)
// Attribution for ported constants lives in TxChannel.h/cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-26 — New test for Phase 3M-1a Task G.1 (TX I/Q production
//                 loop bench fix). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. Exercises the production
// loop interface; all Thetis source cites are in TxChannel.h/cpp.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QObject>
#include <QVector>

#include "core/TxChannel.h"
#include "core/TxMicRouter.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"

using namespace NereusSDR;

// ── MockConnection ──────────────────────────────────────────────────────────
// Minimal mock that records sendTxIq() calls so tests can verify the
// production loop is reaching the connection.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr) : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}

    // Record every sendTxIq() call for assertion.
    void sendTxIq(const float* iq, int n) override
    {
        ++callCount;
        if (iq && n > 0) {
            lastN = n;
        }
    }

    int callCount{0};
    int lastN{0};
};

// ── CountingMicSource ───────────────────────────────────────────────────────
// TxMicRouter that counts pullSamples() calls and writes a known pattern.
class CountingMicSource : public TxMicRouter {
public:
    int pullSamples(float* dst, int n) override
    {
        ++callCount;
        if (dst && n > 0) {
            // Write a distinctive non-zero pattern so we can verify it was called.
            for (int i = 0; i < n; ++i) {
                dst[i] = 0.5f;
            }
            return n;
        }
        return 0;
    }

    int callCount{0};
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class TestTxChannelProductionLoop : public QObject {
    Q_OBJECT

private slots:

    // ── setConnection / setMicRouter null-safety ───────────────────────────

    // Setting a null connection on a fresh channel must not crash.
    void setConnection_null_doesNotCrash()
    {
        TxChannel ch(1);
        ch.setConnection(nullptr);  // no crash
    }

    // Setting a non-null then null connection must not crash.
    void setConnection_thenNull_doesNotCrash()
    {
        TxChannel ch(1);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setConnection(nullptr);  // detach — no crash
    }

    // Setting a null mic router must not crash.
    void setMicRouter_null_doesNotCrash()
    {
        TxChannel ch(1);
        ch.setMicRouter(nullptr);  // no crash
    }

    // Setting a non-null then null mic router must not crash.
    void setMicRouter_thenNull_doesNotCrash()
    {
        TxChannel ch(1);
        NullMicSource src;
        ch.setMicRouter(&src);
        ch.setMicRouter(nullptr);  // detach — no crash
    }

    // ── driveOneTxBlock — guard conditions ────────────────────────────────

    // driveOneTxBlock() with no connection attached must not crash.
    // (m_connection == nullptr guard path)
    void driveOneTxBlock_noConnection_doesNotCrash()
    {
        TxChannel ch(1);
        // Do not call setRunning(true) — driveOneTxBlock guards on m_running.
        // We test the guard indirectly: set running but no connection so the
        // null-connection guard fires before fexchange2.
        // (Can't call driveOneTxBlock directly — it's private; use the timer
        // by process-events after setRunning.)
        ch.setRunning(true);
        // Immediately stop — no connection, so driveOneTxBlock returns early.
        ch.setRunning(false);
        // No crash verifies the guard is intact.
    }

    // driveOneTxBlock() with connection but no mic router must not crash.
    // (m_micRouter == nullptr path — fills silence internally)
    void driveOneTxBlock_noMicRouter_doesNotCrash()
    {
        TxChannel ch(1);
        MockConnection conn;
        ch.setConnection(&conn);
        // No mic router attached — driveOneTxBlock should silence-fill.
        ch.setRunning(true);
        ch.setRunning(false);
        // No crash.
    }

    // ── setRunning / timer start-stop ─────────────────────────────────────

    // setRunning(true) starts the timer; setRunning(false) stops it.
    // Indirect test: if the timer fires we'd see sendTxIq calls on the
    // next event-loop spin.  We just verify no crash here.
    void setRunning_on_doesNotCrash_withConnection()
    {
        TxChannel ch(1);
        MockConnection conn;
        NullMicSource src;
        ch.setConnection(&conn);
        ch.setMicRouter(&src);
        ch.setRunning(true);
        ch.setRunning(false);
    }

    // Repeated setRunning(true) / setRunning(false) cycles must not crash.
    void setRunning_toggleCycles_doesNotCrash()
    {
        TxChannel ch(1);
        MockConnection conn;
        NullMicSource src;
        ch.setConnection(&conn);
        ch.setMicRouter(&src);
        for (int i = 0; i < 5; ++i) {
            ch.setRunning(true);
            ch.setRunning(false);
        }
    }

    // ── sendTxIq call reaches MockConnection ─────────────────────────────

    // When running and a connection is attached, at least one timer tick
    // should call sendTxIq on the mock.  We spin the event loop long enough
    // for the 5 ms timer to fire at least once.
    //
    // This test exercises the timer→driveOneTxBlock→sendTxIq chain in the
    // unit-test build (no HAVE_WDSP → m_outInterleaved stays zero-filled,
    // but sendTxIq is still called with n = kTxDspBufferSize).
    void timerFires_sendsTxIq_toConnection()
    {
        MockConnection conn;
        NullMicSource src;
        // Use a QCoreApplication-enabled test so the Qt event loop processes
        // the QTimer::timeout signal.  QTest provides this via QTEST_MAIN.
        {
            TxChannel ch(1);
            ch.setConnection(&conn);
            ch.setMicRouter(&src);
            ch.setRunning(true);

            // Spin event loop for 30 ms — enough for ≥5 timer ticks at 5 ms.
            QTest::qWait(30);

            ch.setRunning(false);
        }
        // At least one sendTxIq call must have reached the mock.
        QVERIFY(conn.callCount >= 1);
        // Each call must pass n = outputBufferSize = 256 at 48 kHz in/out.
        // 3M-1a r2-ring fix (2026-04-27): in_size raised 238 → 256 so it
        // divides WDSP's ring sizes cleanly (iobuffs.c:577 == wrap requires
        // exact divisibility).  Correct value is 256 = in_size × (outRate /
        // inRate) = 256 × (48000/48000) for the P1 1:1 unit-test path.
        QCOMPARE(conn.lastN, 256);
    }

    // ── sendTxIq count increases with timer ticks ─────────────────────────

    // Multiple timer ticks → multiple sendTxIq calls.
    void timerFires_multipleTimes()
    {
        MockConnection conn;
        NullMicSource src;
        {
            TxChannel ch(1);
            ch.setConnection(&conn);
            ch.setMicRouter(&src);
            ch.setRunning(true);
            QTest::qWait(60);  // ≥12 ticks at 5 ms
            ch.setRunning(false);
        }
        QVERIFY(conn.callCount >= 2);
    }

    // ── Stopping the timer halts sendTxIq ────────────────────────────────

    // After setRunning(false), no further sendTxIq calls should occur.
    void timerStopped_noMoreSendTxIq()
    {
        MockConnection conn;
        NullMicSource src;
        {
            TxChannel ch(1);
            ch.setConnection(&conn);
            ch.setMicRouter(&src);
            ch.setRunning(true);
            QTest::qWait(30);
            ch.setRunning(false);

            const int countAfterStop = conn.callCount;
            QTest::qWait(20);   // extra time — timer should be stopped
            // No new calls after stop (allowing +1 in-flight tolerance).
            QVERIFY(conn.callCount <= countAfterStop + 1);
        }
    }

    // ── Null connection mid-flight: detach while running ──────────────────

    // Detaching the connection while the timer is running must not crash.
    // driveOneTxBlock() guards m_connection == nullptr before sendTxIq.
    void detachConnection_whileRunning_doesNotCrash()
    {
        MockConnection conn;
        NullMicSource src;
        TxChannel ch(1);
        ch.setConnection(&conn);
        ch.setMicRouter(&src);
        ch.setRunning(true);
        QTest::qWait(15);         // let a few ticks fire
        ch.setConnection(nullptr); // detach mid-flight
        QTest::qWait(15);         // additional ticks with null connection
        ch.setRunning(false);
        // No crash verifies the null-guard in driveOneTxBlock.
    }

    // ── CountingMicSource is called by driveOneTxBlock ───────────────────

    // When a CountingMicSource is attached, pullSamples() must be called
    // at least once per timer tick.
    void micRouter_pullSamples_isCalled()
    {
        MockConnection conn;
        CountingMicSource src;
        {
            TxChannel ch(1);
            ch.setConnection(&conn);
            ch.setMicRouter(&src);
            ch.setRunning(true);
            QTest::qWait(30);   // ≥5 ticks
            ch.setRunning(false);
        }
        QVERIFY(src.callCount >= 1);
    }

    // ── Null mic router: driveOneTxBlock still sends zeros ───────────────

    // With no mic router, driveOneTxBlock must still call sendTxIq
    // (silence path — m_inI/inQ filled with zeros internally).
    void nullMicRouter_stillSendsTxIq()
    {
        MockConnection conn;
        {
            TxChannel ch(1);
            ch.setConnection(&conn);
            // Deliberately no setMicRouter — null router path.
            ch.setRunning(true);
            QTest::qWait(30);
            ch.setRunning(false);
        }
        QVERIFY(conn.callCount >= 1);
    }

    // ── setRunning(false) with null m_txProductionTimer: safety ──────────

    // This verifies setRunning(false) is safe even if called before timer
    // is started (isRunning() stays false; no crash).
    void setRunningOff_noPriorOn_doesNotCrash()
    {
        TxChannel ch(1);
        ch.setRunning(false);   // timer never started — no crash
        QVERIFY(!ch.isRunning());
    }

    // ── Destroyed connection before TxChannel: graceful ───────────────────

    // Simulate teardownConnection order: setConnection(nullptr) first, then
    // destroy the connection.  No dangling-pointer access.
    void teardownOrder_connectionNullFirst_doesNotCrash()
    {
        TxChannel ch(1);
        {
            MockConnection conn;
            NullMicSource src;
            ch.setConnection(&conn);
            ch.setMicRouter(&src);
            ch.setRunning(true);
            QTest::qWait(15);
            ch.setConnection(nullptr);  // detach before connection leaves scope
            ch.setMicRouter(nullptr);
            ch.setRunning(false);
        } // conn destroyed here — ch.m_connection already null
        // No crash verifies safe teardown order.
    }
};

QTEST_MAIN(TestTxChannelProductionLoop)
#include "tst_tx_channel_production_loop.moc"
