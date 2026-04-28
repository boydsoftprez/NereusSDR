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
// tests/tst_tx_channel_real_mic_router.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is ported in this test file. The test exercises:
//   - TxChannel::driveOneTxBlock() via the NEREUS_BUILD_TESTS tickForTest()
//     seam added in Phase 3M-1b D.1.
//   - TxMicRouter::pullSamples contract (NereusSDR-original, TxMicRouter.h)
//
// This test verifies two invariants introduced by Phase 3M-1b D.1:
//
//   1. CALL CHAIN: a real (non-null) TxMicRouter attached via setMicRouter()
//      has its pullSamples() called by driveOneTxBlock(), and the samples
//      written into m_inI are exactly the values the mock emits.
//
//   2. Q=0 INVARIANT: m_inQ must be all-zero after every driveOneTxBlock()
//      cycle when a real mic source is attached.  Real mic input is mono
//      (real-valued); there is no Q component.  WDSP SSB modulation adds the
//      quadrature component internally — we must not inject garbage into m_inQ.
//
// Context (Phase 3M-1b D.1):
//   3M-1a wired driveOneTxBlock() to call pullSamples() and to explicitly
//   zero-fill m_inQ via std::fill after pullSamples returns.  The NullMicSource
//   also zero-filled m_inI, so the existing production-loop tests (which only
//   check that sendTxIq was called) did not exercise the ramp-input path or
//   directly verify the Q=0 invariant.  This test suite covers both.
//
// Pre-code review reference: §0.3 (PcMicSource architecture).
//
// Attribution for ported constants lives in TxChannel.h/cpp with verbatim
// Thetis source cites.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — New test for Phase 3M-1b Task D.1: verify real TxMicRouter
//                 drives fexchange2 with Q=0.  Test seam (tickForTest /
//                 inIForTest / inQForTest) added to TxChannel.h under
//                 NEREUS_BUILD_TESTS guard.  J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp. NEREUS_BUILD_TESTS must be defined (see CMakeLists.txt).

#include <QtTest/QtTest>

#include "core/TxChannel.h"
#include "core/TxMicRouter.h"
#include "core/RadioConnection.h"

using namespace NereusSDR;

// ── TestRampMicRouter ───────────────────────────────────────────────────────
//
// Emits a deterministic ramp: dst[i] = float(i + 1).
// Values are all non-zero so test failures produce readable diffs.
// A ramp (1, 2, 3, …, n) makes it easy to spot off-by-one errors.
class TestRampMicRouter : public TxMicRouter {
public:
    int callCount{0};

    int pullSamples(float* dst, int n) override
    {
        ++callCount;
        if (dst == nullptr || n <= 0) {
            return 0;
        }
        for (int i = 0; i < n; ++i) {
            dst[i] = static_cast<float>(i + 1);  // ramp: 1, 2, 3, …
        }
        return n;
    }
};

// ── MockConnection ──────────────────────────────────────────────────────────
// Minimal mock so TxChannel's null-connection guard in driveOneTxBlock()
// does not short-circuit before pullSamples is called.
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
    void sendTxIq(const float*, int) override {}  // consume silently
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class TestTxChannelRealMicRouter : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;   // WDSP.id(1, 0) — TX channel
    static constexpr int kBufSize   = 256; // default inputBufferSize / outputBufferSize

private slots:

    // ── Call-chain: pullSamples() is invoked by driveOneTxBlock ──────────────
    //
    // After setMicRouter(&mock) and one tickForTest(), the mock's callCount
    // must be > 0.  This is the most fundamental invariant: a real mic source
    // actually gets called, not silently bypassed.

    void inI_pullSamples_isCalledAfterTick()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);
        ch.tickForTest();

        QVERIFY(mock.callCount > 0);
    }

    // ── inI matches the ramp emitted by the test source ──────────────────────
    //
    // After tickForTest(), m_inI[i] must equal float(i + 1).
    // This verifies that driveOneTxBlock() copies pullSamples() output into
    // m_inI without corruption and with the correct element count.

    void inI_matchesRampFromTestSource()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);
        ch.tickForTest();

        const auto& inI = ch.inIForTest();
        QCOMPARE(static_cast<int>(inI.size()), kBufSize);

        // Spot-check first, middle, and last values of the ramp.
        QCOMPARE(inI[0], 1.0f);
        QCOMPARE(inI[1], 2.0f);
        QCOMPARE(inI[kBufSize / 2], static_cast<float>(kBufSize / 2 + 1));
        QCOMPARE(inI[kBufSize - 1], static_cast<float>(kBufSize));
    }

    void inI_fullRampMatchesExpected()
    {
        // Exhaustive check: every element must match the ramp exactly.
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);
        ch.tickForTest();

        const auto& inI = ch.inIForTest();
        for (int i = 0; i < kBufSize; ++i) {
            const float expected = static_cast<float>(i + 1);
            if (inI[i] != expected) {
                // QFAIL with a descriptive message on first mismatch.
                QFAIL(qPrintable(
                    QString("inI[%1] = %2, expected %3")
                        .arg(i).arg(static_cast<double>(inI[i]))
                        .arg(static_cast<double>(expected))));
            }
        }
    }

    // ── Q=0 invariant: inQ is all-zeros after tick with real mic source ───────
    //
    // Real mic input is mono (real-valued); there is no quadrature component.
    // driveOneTxBlock() must zero-fill m_inQ after pullSamples(), regardless
    // of what the previous tick left in the buffer.
    //
    // Without this explicit zero-fill, stale WDSP output or residual data
    // from a previous cycle could leak into the Q input and corrupt the SSB
    // modulation produced by fexchange2.
    //
    // From TxChannel.cpp driveOneTxBlock() [Phase 3M-1a G.1]:
    //   m_micRouter->pullSamples(m_inI.data(), inN);
    //   std::fill(m_inQ.begin(), m_inQ.end(), 0.0f);   // Q=0 for real-valued mic

    void inQ_isZeroFilledAfterTick()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;   // emits non-zero ramp into m_inI

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);
        ch.tickForTest();

        const auto& inQ = ch.inQForTest();
        QCOMPARE(static_cast<int>(inQ.size()), kBufSize);

        for (int i = 0; i < kBufSize; ++i) {
            if (inQ[i] != 0.0f) {
                QFAIL(qPrintable(
                    QString("inQ[%1] = %2, expected 0.0 (Q=0 invariant violated)")
                        .arg(i).arg(static_cast<double>(inQ[i]))));
            }
        }
    }

    // ── Multi-cycle: inQ stays zero across consecutive ticks ─────────────────
    //
    // If WDSP (when compiled in) were to write into m_inQ as a side-effect of
    // fexchange2, or if driveOneTxBlock() stopped zeroing m_inQ after the
    // first tick, later ticks would expose stale data.  This test catches that
    // regression by running five ticks and asserting Q=0 each time.

    void multipleCycles_inQRemainsZero()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);

        for (int cycle = 0; cycle < 5; ++cycle) {
            ch.tickForTest();

            const auto& inQ = ch.inQForTest();
            for (int i = 0; i < kBufSize; ++i) {
                if (inQ[i] != 0.0f) {
                    QFAIL(qPrintable(
                        QString("cycle %1: inQ[%2] = %3, expected 0.0 (Q=0 invariant violated)")
                            .arg(cycle).arg(i).arg(static_cast<double>(inQ[i]))));
                }
            }
        }
    }

    // ── Multi-cycle: inI still matches ramp on every tick ────────────────────
    //
    // Each tick must re-invoke pullSamples() and overwrite m_inI with the
    // latest ramp.  A single callCount increment per tick is verified; the
    // ramp values must also be fresh each cycle (TestRampMicRouter emits the
    // same ramp every call, so inI[0..n-1] = {1,2,…,n} always holds).

    void multipleCycles_inIMatchesRampEachTick()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);

        for (int cycle = 0; cycle < 3; ++cycle) {
            ch.tickForTest();

            QCOMPARE(mock.callCount, cycle + 1);  // one call per tick

            const auto& inI = ch.inIForTest();
            QCOMPARE(inI[0], 1.0f);
            QCOMPARE(inI[kBufSize - 1], static_cast<float>(kBufSize));
        }
    }

    // ── Buffer size sanity: inI and inQ have the expected size ───────────────

    void inI_sizeMatchesInputBufferSize()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);
        ch.tickForTest();

        QCOMPARE(static_cast<int>(ch.inIForTest().size()), kBufSize);
    }

    void inQ_sizeMatchesInputBufferSize()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        ch.setRunning(true);
        ch.tickForTest();

        QCOMPARE(static_cast<int>(ch.inQForTest().size()), kBufSize);
    }

    // ── tickForTest() no-op guard: !m_running short-circuits driveOneTxBlock ─
    //
    // If setRunning(false), driveOneTxBlock() returns early before pullSamples.
    // Verify callCount stays 0 and no crash occurs.

    void tickForTest_notRunning_doesNotCallPullSamples()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        TestRampMicRouter mock;

        ch.setConnection(&conn);
        ch.setMicRouter(&mock);
        // Deliberately do NOT call setRunning(true) — driveOneTxBlock early-exits.
        ch.tickForTest();

        QCOMPARE(mock.callCount, 0);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelRealMicRouter)
#include "tst_tx_channel_real_mic_router.moc"
