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
//   - TxChannel::driveOneTxBlock(samples, frames) via the
//     NEREUS_BUILD_TESTS tickForTest(samples, frames) seam.
//   - The push-driven mic-block contract introduced in Phase 3M-1c E.1.
//
// This test verifies two invariants:
//
//   1. PUSHED SAMPLES LAND IN m_inI: after a tickForTest(samples, n) call
//      with non-null samples and n == m_inputBufferSize, m_inI contains
//      exactly those samples (verified element-by-element with a ramp).
//
//   2. Q=0 INVARIANT: m_inQ must be all-zero after every push cycle.
//      Real mic input is mono (real-valued); there is no Q component.
//      WDSP SSB modulation adds the quadrature internally — we must not
//      inject garbage into m_inQ.
//
// Context: Phase 3M-1b D.1 added the original tickForTest() seam (no
// arguments), which drove the timer-pulled router-based model.  Phase
// 3M-1c E.1 retired the timer in favour of a (samples, frames) push slot;
// this test was rewritten to use the new contract while preserving the
// same fundamental invariants (samples land in m_inI; Q stays zero).
//
// The TxMicRouter is no longer the PC-mic source: it is retained on
// TxChannel only for the future Radio-mic source path.  These tests do
// not attach a TxMicRouter — that decoupling is the entire point of E.1.
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
//   2026-04-28 — Phase 3M-1c E.1: rewrote tests for the push-driven
//                 model.  tickForTest now takes (samples, frames); the
//                 TxMicRouter pull path is no longer exercised because
//                 TxChannel no longer pulls from it for the PC-mic path.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp. NEREUS_BUILD_TESTS must be defined (see CMakeLists.txt).

#include <QtTest/QtTest>

#include <vector>

#include "core/TxChannel.h"
#include "core/RadioConnection.h"

using namespace NereusSDR;

// ── MockConnection ──────────────────────────────────────────────────────────
// Minimal mock so TxChannel's null-connection guard does not short-circuit
// before m_inI / m_inQ are populated.
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
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void sendTxIq(const float*, int) override {}  // consume silently
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class TestTxChannelRealMicRouter : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;   // WDSP.id(1, 0) — TX channel
    static constexpr int kBufSize   = 256; // default inputBufferSize / outputBufferSize

    // Build a deterministic ramp [1, 2, 3, …, n] so any off-by-one or
    // truncation surfaces as a readable diff.
    static std::vector<float> ramp(int n)
    {
        std::vector<float> v(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            v[static_cast<size_t>(i)] = static_cast<float>(i + 1);
        }
        return v;
    }

private slots:

    // ── Pushed samples land in m_inI[i] = i+1 ────────────────────────────────

    void inI_matchesPushedRamp_spotCheck()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        const auto& inI = ch.inIForTest();
        QCOMPARE(static_cast<int>(inI.size()), kBufSize);
        QCOMPARE(inI[0], 1.0f);
        QCOMPARE(inI[1], 2.0f);
        QCOMPARE(inI[kBufSize / 2], static_cast<float>(kBufSize / 2 + 1));
        QCOMPARE(inI[kBufSize - 1], static_cast<float>(kBufSize));
    }

    void inI_matchesPushedRamp_exhaustive()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        const auto& inI = ch.inIForTest();
        for (int i = 0; i < kBufSize; ++i) {
            const float expected = static_cast<float>(i + 1);
            if (inI[i] != expected) {
                QFAIL(qPrintable(
                    QString("inI[%1] = %2, expected %3")
                        .arg(i).arg(static_cast<double>(inI[i]))
                        .arg(static_cast<double>(expected))));
            }
        }
    }

    // ── Q=0 invariant ────────────────────────────────────────────────────────

    void inQ_isZeroFilledAfterPush()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

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

    void multipleCycles_inQRemainsZero()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);

        for (int cycle = 0; cycle < 5; ++cycle) {
            ch.tickForTest(buf.data(), kBufSize);

            const auto& inQ = ch.inQForTest();
            for (int i = 0; i < kBufSize; ++i) {
                if (inQ[i] != 0.0f) {
                    QFAIL(qPrintable(
                        QString("cycle %1: inQ[%2] = %3, expected 0.0")
                            .arg(cycle).arg(i).arg(static_cast<double>(inQ[i]))));
                }
            }
        }
    }

    // ── Multi-cycle freshness: each push overwrites m_inI ────────────────────

    void multipleCycles_inIRefreshedEachPush()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        // Push three identical ramps; m_inI should match on each.
        const auto buf = ramp(kBufSize);

        for (int cycle = 0; cycle < 3; ++cycle) {
            ch.tickForTest(buf.data(), kBufSize);

            const auto& inI = ch.inIForTest();
            QCOMPARE(inI[0], 1.0f);
            QCOMPARE(inI[kBufSize - 1], static_cast<float>(kBufSize));
        }
    }

    // ── Buffer sizes match m_inputBufferSize ────────────────────────────────

    void inI_sizeMatchesInputBufferSize()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        QCOMPARE(static_cast<int>(ch.inIForTest().size()), kBufSize);
    }

    void inQ_sizeMatchesInputBufferSize()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        QCOMPARE(static_cast<int>(ch.inQForTest().size()), kBufSize);
    }

    // ── !m_running guard: push is a no-op (m_inI not touched) ────────────────

    void tickForTest_notRunning_doesNotPopulateInI()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        // Deliberately do NOT call setRunning(true).

        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        // m_inI should be the constructor-initialised zero-fill, not the ramp.
        const auto& inI = ch.inIForTest();
        QCOMPARE(inI[0], 0.0f);
        QCOMPARE(inI[kBufSize - 1], 0.0f);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelRealMicRouter)
#include "tst_tx_channel_real_mic_router.moc"
