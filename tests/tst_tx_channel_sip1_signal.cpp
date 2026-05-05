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
// tests/tst_tx_channel_sip1_signal.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is ported in this test file. The test exercises:
//   - TxChannel::sip1OutputReady signal (Phase 3M-1b D.5).
//   - driveOneTxBlock() via the NEREUS_BUILD_TESTS tickForTest seam,
//     updated to (samples, frames) for the Phase 3M-1c E.1 push model.
//
// This test verifies the invariants of the D.5 sip1OutputReady signal:
//
//   1. FIRE ONCE PER TICK: the signal fires exactly once per tickForTest()
//      call when m_running is true and m_connection is set.
//
//   2. CORRECT FRAME COUNT: the second argument (frames) equals
//      m_outputBufferSize (the kBufSize constructor argument).
//
//   3. MULTI-CYCLE: the signal fires on every tick over multiple cycles.
//
//   4. NOT RUNNING: the signal does NOT fire when m_running is false
//      (driveOneTxBlock early-exits before the emit).
//
//   5. NO CONNECTION: the signal does NOT fire when m_connection is null
//      (driveOneTxBlock early-exits before the emit because m_connection
//      is null — same guard that gates the whole block).
//
// QSignalSpy caveat:
//   QSignalSpy can capture raw pointer arguments but requires the type to
//   be registered with Qt's metatype system via
//   qRegisterMetaType<const float*>("const float*").  Without registration,
//   QSignalSpy may fail to capture the pointer argument.  We register it
//   before constructing the spy.  If capture still fails (returns QVariant()
//   with an invalid value), we fall back to asserting only the frame count
//   (the load-bearing invariant for the D.5 contract).
//
//   Note: the DirectConnection-only contract means QSignalSpy can be used
//   safely here — the spy runs synchronously as part of the tickForTest()
//   call chain.  There are no threading hazards in the test setup.
//
// Pre-code review reference: §4.3 (MON path — sip1OutputReady).
//
// Attribution for ported constants lives in TxChannel.h/cpp with verbatim
// Thetis source cites.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — New test for Phase 3M-1b Task D.5: verify sip1OutputReady
//                 signal fires after each driveOneTxBlock() cycle with the
//                 correct frame count. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-04-28 — Phase 3M-1c E.1: tickForTest now takes (samples, frames).
//                 TxMicRouter is no longer the PC-mic source; tests push
//                 samples directly.  J.J. Boyd (KG4VCF), AI-assisted.
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
// before sip1OutputReady is emitted. sendTxIq consumes silently.
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

// ── Test fixture ──────────────────────────────────────────────────────────────

class TstTxChannelSip1Signal : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;   // WDSP.id(1, 0) — TX channel
    static constexpr int kBufSize   = 256; // default inputBufferSize / outputBufferSize

    // Build a deterministic ramp matching the configured input buffer size.
    static std::vector<float> ramp(int n)
    {
        std::vector<float> v(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            v[static_cast<size_t>(i)] = static_cast<float>(i + 1);
        }
        return v;
    }

private slots:

    // ── Signal fires exactly once per tick ───────────────────────────────────

    void sip1OutputReady_emitsOnTick()
    {
        // Register the raw pointer type so QSignalSpy can capture it.
        // This is a no-op if already registered; safe to call multiple times.
        qRegisterMetaType<const float*>("const float*");

        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        QSignalSpy spy(&ch, &TxChannel::sip1OutputReady);
        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        QCOMPARE(spy.count(), 1);
    }

    // ── Second signal argument (frames) matches kBufSize ─────────────────────

    void sip1OutputReady_frameCountMatchesOutputBufferSize()
    {
        qRegisterMetaType<const float*>("const float*");

        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        QSignalSpy spy(&ch, &TxChannel::sip1OutputReady);
        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        // args.at(1) is the `int frames` argument — always capturable.
        QCOMPARE(args.at(1).value<int>(), kBufSize);
    }

    // ── Signal fires on every tick across multiple cycles ────────────────────

    void sip1OutputReady_multipleCycles()
    {
        qRegisterMetaType<const float*>("const float*");

        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        QSignalSpy spy(&ch, &TxChannel::sip1OutputReady);
        const auto buf = ramp(kBufSize);

        for (int tick = 1; tick <= 5; ++tick) {
            ch.tickForTest(buf.data(), kBufSize);
            QCOMPARE(spy.count(), tick);
        }
    }

    // ── Signal does NOT fire when m_running is false ──────────────────────────

    void sip1OutputReady_notRunning_doesNotEmit()
    {
        qRegisterMetaType<const float*>("const float*");

        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        // Deliberately do NOT call setRunning(true).

        QSignalSpy spy(&ch, &TxChannel::sip1OutputReady);
        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        QCOMPARE(spy.count(), 0);
    }

    // ── Signal does NOT fire when m_connection is null ────────────────────────

    void sip1OutputReady_noConnection_doesNotEmit()
    {
        qRegisterMetaType<const float*>("const float*");

        TxChannel ch(kChannelId, kBufSize, kBufSize);
        // Deliberately do NOT call setConnection() — m_connection stays null.
        ch.setRunning(true);

        QSignalSpy spy(&ch, &TxChannel::sip1OutputReady);
        const auto buf = ramp(kBufSize);
        ch.tickForTest(buf.data(), kBufSize);

        QCOMPARE(spy.count(), 0);
    }

    // ── Frame count is correct for non-default outputBufferSize ──────────────

    void sip1OutputReady_frameCountMatchesNonDefaultOutputBufferSize()
    {
        qRegisterMetaType<const float*>("const float*");

        constexpr int kP2OutSize = 1024;  // P2 Saturn: 256 × 192 000 / 48 000
        TxChannel ch(kChannelId, 256 /*in*/, kP2OutSize /*out*/);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        QSignalSpy spy(&ch, &TxChannel::sip1OutputReady);
        const auto buf = ramp(256);
        ch.tickForTest(buf.data(), 256);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(1).value<int>(), kP2OutSize);
    }
};

QTEST_APPLESS_MAIN(TstTxChannelSip1Signal)
#include "tst_tx_channel_sip1_signal.moc"
