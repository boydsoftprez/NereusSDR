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

/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

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
// tests/tst_tx_channel_mic_mute.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is directly ported in this test file.  The test exercises:
//   - TxChannel::setMicPreamp(double)          — D.6 Phase 3M-1b
//   - TxChannel::recomputeTxAPanelGain1()      — D.6 Phase 3M-1b
//
// Porting context (cited in TxChannel.h / TxChannel.cpp):
//   setMicPreamp / recomputeTxAPanelGain1:
//     console.cs:28805-28817 [v2.10.3.13] — setAudioMicGain() mute/unmute
//     audio.cs:216-243       [v2.10.3.13] — MicPreamp property → CMSetTXAPanelGain1
//     wdsp/patchpanel.c:209  [v2.10.3.13] — SetTXAPanelGain1 implementation
//     dsp.cs:411-412         [v2.10.3.13] — SetTXAPanelGain1 DLL import
//
// Key semantic:
//   chkMicMute.Checked == true  → mic IS active (counter-intuitive naming!)
//                                 → MicPreamp = pow(10, gain_db/20)  (non-zero)
//   chkMicMute.Checked == false → mic is MUTED
//                                 → MicPreamp = 0.0
//   D.6's setMicPreamp(0.0) maps exactly to the mute=true (Checked==false) path.
//
// Tests verify (NEREUS_BUILD_TESTS test-seam accessor required):
//   1. First call with a non-zero value stores the value (NaN sentinel fires).
//   2. Zero value (mute case) stores 0.0 correctly (NaN guard passes for 0.0).
//   3. Round-trip: set A, then B → accessor returns B.
//   4. Idempotent guard: set A twice → second call is a no-op (value unchanged).
//   5. NaN initial state confirmed by test-seam accessor.
//   6. recomputeTxAPanelGain1() does not crash (WDSP null-guard fires without
//      an open channel; verifies no UB in the stub path).
//   7. recomputeTxAPanelGain1() after setMicPreamp — latest value is available
//      to recompute (m_micPreampLast was set by setMicPreamp).
//
// WDSP-call verification is not possible without a mock; in unit-test builds
// txa[].rsmpin.p == nullptr so recomputeTxAPanelGain1 is a documented no-op.
// The tests confirm storage state changes correctly and no crash occurs.
//
// Total test cases: 8
//
// Requires NEREUS_BUILD_TESTS (set by CMakeLists target tst_tx_channel_mic_mute).
// Test-seam accessor (lastMicPreampForTest) is compiled into TxChannel only
// when that define is set.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — New test for Phase 3M-1b Task D.6: TxChannel mic-mute path
//                 (setMicPreamp / recomputeTxAPanelGain1).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#define NEREUS_BUILD_TESTS 1

#include <QtTest/QtTest>
#include <cmath>   // std::isnan

#include "core/TxChannel.h"

using namespace NereusSDR;

class TestTxChannelMicMute : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;  // WDSP.id(1, 0) — TX channel

private slots:

    // ── setMicPreamp ──────────────────────────────────────────────────────────
    //
    // Mic preamp linear scalar — wires WDSP SetTXAPanelGain1.
    // From Thetis console.cs:28805-28817 [v2.10.3.13] — setAudioMicGain.

    void setMicPreamp_initialState_isNaN()
    {
        // m_micPreampLast initialises to quiet_NaN so the first call
        // (any value, including 0.0) always passes the guard.
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastMicPreampForTest()));
    }

    void setMicPreamp_nonZeroValue_storesIt()
    {
        // NaN sentinel means first call always passes the guard.
        // Audio.MicPreamp = Math.Pow(10.0, gain_db / 20.0) path in Thetis.
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastMicPreampForTest()));
        ch.setMicPreamp(0.5);  // typical mic gain scalar
        QCOMPARE(ch.lastMicPreampForTest(), 0.5);
    }

    void setMicPreamp_zeroValue_storesZero()
    {
        // This is the mute case (Thetis chkMicMute.Checked == false path):
        //   Audio.MicPreamp = 0.0; → SetTXAPanelGain1(ch, 0.0) → mic silenced.
        // NaN sentinel must pass even for 0.0 on first call.
        //
        // From Thetis console.cs:28813-28815 [v2.10.3.13]:
        //   else { Audio.MicPreamp = 0.0; _mic_muted = true; }
        TxChannel ch(kChannelId);
        ch.setMicPreamp(0.0);
        QCOMPARE(ch.lastMicPreampForTest(), 0.0);
    }

    void setMicPreamp_roundTrip_storesLatestValue()
    {
        TxChannel ch(kChannelId);
        ch.setMicPreamp(0.5);
        ch.setMicPreamp(0.0);  // mute
        QCOMPARE(ch.lastMicPreampForTest(), 0.0);
    }

    void setMicPreamp_roundTrip_muteToUnmute()
    {
        // Unmute after mute: 0.0 → 0.5
        TxChannel ch(kChannelId);
        ch.setMicPreamp(0.0);  // mute
        ch.setMicPreamp(0.5);  // unmute (restore gain)
        QCOMPARE(ch.lastMicPreampForTest(), 0.5);
    }

    void setMicPreamp_idempotent_secondCallNoChange()
    {
        // Once the idempotent guard stores 0.5, a second call with 0.5
        // must be a no-op.  The stored value stays 0.5.
        TxChannel ch(kChannelId);
        ch.setMicPreamp(0.5);
        QCOMPARE(ch.lastMicPreampForTest(), 0.5);
        ch.setMicPreamp(0.5);  // idempotent guard fires
        QCOMPARE(ch.lastMicPreampForTest(), 0.5);
    }

    // ── recomputeTxAPanelGain1 ────────────────────────────────────────────────
    //
    // Force-push the stored gain to WDSP.
    // Without an open WDSP channel (unit-test build), the call is a documented
    // no-op.  These tests verify no crash and that storage state is correct.

    void recomputeTxAPanelGain1_doesNotCrash()
    {
        // Without HAVE_WDSP or with txa[].rsmpin.p == nullptr, calling
        // recomputeTxAPanelGain1() must be safe (no UB, no crash).
        // m_micPreampLast is NaN here — the null-guard fires before the
        // WDSP call; no segfault or assertion failure expected.
        TxChannel ch(kChannelId);
        ch.recomputeTxAPanelGain1();  // no crash
        // If we get here without an exception or crash, the test passes.
        QVERIFY(true);
    }

    void recomputeTxAPanelGain1_afterSetMicPreamp_pushesLatest()
    {
        // After setMicPreamp stores 0.0, recomputeTxAPanelGain1() must
        // not change m_micPreampLast (it only pushes to WDSP).  The
        // stored value stays 0.0.
        TxChannel ch(kChannelId);
        ch.setMicPreamp(0.0);
        QCOMPARE(ch.lastMicPreampForTest(), 0.0);
        ch.recomputeTxAPanelGain1();
        // Value unchanged — recompute only pushes, doesn't alter storage.
        QCOMPARE(ch.lastMicPreampForTest(), 0.0);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelMicMute)
#include "tst_tx_channel_mic_mute.moc"
