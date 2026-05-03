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
// No Thetis code is ported in this test file. The production loop logic
// (driveOneTxBlock) exercises:
//   - WDSP fexchange2 (wdsp/iobuffs.c [v2.10.3.13])
//   - RadioConnection::sendTxIq (RadioConnection.h — abstract virtual)
// Attribution for ported constants lives in TxChannel.h/cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-26 — New test for Phase 3M-1a Task G.1 (TX I/Q production
//                 loop bench fix). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-04-28 — Phase 3M-1c E.1 — converted timer-driven tests to
//                 synchronous tickForTest(samples, frames) push calls.
//                 The QTimer was removed in E.1; tests that used to spin
//                 QTest::qWait() to let timer ticks fire now drive the
//                 push slot directly.  J.J. Boyd (KG4VCF), AI-assisted
//                 transformation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. Exercises the production
// loop interface; all Thetis source cites are in TxChannel.h/cpp.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QObject>
#include <QVector>

#include <vector>

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
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTT(bool) override {}
    void setMicXlr(bool) override {}

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

// ── Test fixture ─────────────────────────────────────────────────────────────

class TestTxChannelProductionLoop : public QObject {
    Q_OBJECT

    static constexpr int kBufSize = 256;  // default inputBufferSize / outputBufferSize

    // Build a non-zero block of size n — pushed via tickForTest to exercise
    // the production slot synchronously.
    static std::vector<float> ramp(int n)
    {
        std::vector<float> v(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            v[static_cast<size_t>(i)] = static_cast<float>(i + 1);
        }
        return v;
    }

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

    // Setting a null mic router must not crash.  TxMicRouter is retained
    // in 3M-1c E.1 for the future Radio-mic source path; the PC-mic path
    // no longer pulls from it.
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

    // setRunning(true) without a connection must not crash.
    void setRunning_noConnection_doesNotCrash()
    {
        TxChannel ch(1);
        ch.setRunning(true);
        ch.setRunning(false);
    }

    // setRunning(true) with a connection but no router must not crash.
    void setRunning_noMicRouter_doesNotCrash()
    {
        TxChannel ch(1);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);
        ch.setRunning(false);
    }

    // ── setRunning toggle cycles ───────────────────────────────────────────

    void setRunning_on_doesNotCrash_withConnection()
    {
        TxChannel ch(1);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);
        ch.setRunning(false);
    }

    void setRunning_toggleCycles_doesNotCrash()
    {
        TxChannel ch(1);
        MockConnection conn;
        ch.setConnection(&conn);
        for (int i = 0; i < 5; ++i) {
            ch.setRunning(true);
            ch.setRunning(false);
        }
    }

    // ── Push slot drives sendTxIq ─────────────────────────────────────────
    //
    // 3M-1c E.1: the QTimer is gone; tests now push samples via the
    // tickForTest(samples, frames) seam.  One push -> one sendTxIq call.

    void pushFullBlock_sendsTxIq_toConnection()
    {
        MockConnection conn;
        TxChannel ch(1);
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        QCOMPARE(conn.callCount, 1);
        // n = outputBufferSize = 256 at 48 kHz in/out.
        QCOMPARE(conn.lastN, kBufSize);
    }

    // Multiple synchronous pushes → multiple sendTxIq calls.
    void pushMultipleBlocks_yieldsMultipleSendTxIq()
    {
        MockConnection conn;
        TxChannel ch(1);
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        for (int i = 0; i < 5; ++i) {
            ch.tickForTest(buf.data(), kBufSize);
        }

        QCOMPARE(conn.callCount, 5);
    }

    // ── Stopping disarms the slot ─────────────────────────────────────────

    // After setRunning(false), pushed samples are ignored (slot guard).
    void slotIgnoresPushAfterStop()
    {
        MockConnection conn;
        TxChannel ch(1);
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);
        ch.setRunning(false);
        ch.tickForTest(buf.data(), kBufSize);  // ignored — slot disarmed

        QCOMPARE(conn.callCount, 1);
    }

    // ── Null connection mid-flight: detach and continue pushing ────────────

    void detachConnection_pushIsSilent()
    {
        MockConnection conn;
        TxChannel ch(1);
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);
        QCOMPARE(conn.callCount, 1);

        ch.setConnection(nullptr);  // detach — slot now no-ops on push
        ch.tickForTest(buf.data(), kBufSize);
        QCOMPARE(conn.callCount, 1);

        ch.setRunning(false);
    }

    // ── Null mic router: silence push still drives sendTxIq ────────────────
    //
    // The TxMicRouter is no longer pulled from; the silence path is the
    // explicit (nullptr, 0) push case for TUNE-tone PostGen output.

    void nullSamples_stillSendsTxIq()
    {
        MockConnection conn;
        TxChannel ch(1);
        ch.setConnection(&conn);
        ch.setRunning(true);

        ch.tickForTest(nullptr, 0);

        QCOMPARE(conn.callCount, 1);
    }

    // ── setRunning(false) before any setRunning(true): safety ──────────────

    void setRunningOff_noPriorOn_doesNotCrash()
    {
        TxChannel ch(1);
        ch.setRunning(false);
        QVERIFY(!ch.isRunning());
    }

    // ── Destroyed connection before TxChannel: graceful ───────────────────

    void teardownOrder_connectionNullFirst_doesNotCrash()
    {
        TxChannel ch(1);
        {
            MockConnection conn;
            NullMicSource src;
            ch.setConnection(&conn);
            ch.setMicRouter(&src);
            ch.setRunning(true);

            const auto buf = ramp(kBufSize);
            ch.tickForTest(buf.data(), kBufSize);

            ch.setConnection(nullptr);
            ch.setMicRouter(nullptr);
            ch.setRunning(false);
        }
        // No crash verifies safe teardown order.
    }
};

QTEST_MAIN(TestTxChannelProductionLoop)
#include "tst_tx_channel_production_loop.moc"
