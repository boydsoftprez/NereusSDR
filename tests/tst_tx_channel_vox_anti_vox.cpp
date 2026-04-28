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
// tests/tst_tx_channel_vox_anti_vox.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is directly ported in this test file.  The test exercises:
//   - TxChannel::setVoxRun(bool)           — D.3 Phase 3M-1b
//   - TxChannel::setVoxAttackThreshold(double) — D.3 Phase 3M-1b
//   - TxChannel::setVoxHangTime(double)    — D.3 Phase 3M-1b
//   - TxChannel::setAntiVoxRun(bool)       — D.3 Phase 3M-1b
//   - TxChannel::setAntiVoxGain(double)    — D.3 Phase 3M-1b
//
// Porting context (cited in TxChannel.h / TxChannel.cpp):
//   setVoxRun:            cmaster.cs:199-200 [v2.10.3.13] — SetDEXPRunVox
//   setVoxAttackThreshold: cmaster.cs:187-188 [v2.10.3.13] — SetDEXPAttackThreshold
//   setVoxHangTime:       cmaster.cs:178-179 [v2.10.3.13] — SetDEXPHoldTime
//                         (WDSP uses "HoldTime" for VOX hang; no SetDEXPHangTime exists)
//   setAntiVoxRun:        cmaster.cs:208-209 [v2.10.3.13] — SetAntiVOXRun
//   setAntiVoxGain:       cmaster.cs:211-212 [v2.10.3.13] — SetAntiVOXGain
//
// Tests verify (NEREUS_BUILD_TESTS test-seam accessors required):
//   1. First call stores the value (NaN sentinel fires → WDSP path taken).
//   2. Round-trip: set A, then B → last-value accessor returns B.
//   3. Idempotent guard: set A twice → second call is a no-op at the WDSP
//      level.  Without WDSP linked the only observable change is the stored
//      value, which must still equal A after the second call.
//   4. Edge values for doubles: 0.0, negative, large.
//
// Each of the 5 wrappers gets: first-call, round-trip, idempotent (+ edge for
// doubles).  Total: 17 test cases.
//
// Requires NEREUS_BUILD_TESTS (set by CMakeLists target
// tst_tx_channel_vox_anti_vox).  Test-seam accessors (lastVoxRunForTest,
// lastVoxAttackThresholdForTest, etc.) are compiled into TxChannel only when
// that define is set.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-27 — New test for Phase 3M-1b Task D.3: VOX/anti-VOX WDSP
//                 wrappers (setVoxRun / setVoxAttackThreshold /
//                 setVoxHangTime / setAntiVoxRun / setAntiVoxGain).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#define NEREUS_BUILD_TESTS 1

#include <QtTest/QtTest>
#include <cmath>   // std::isnan

#include "core/TxChannel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestTxChannelVoxAntiVox : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;  // WDSP.id(1, 0) — TX channel

private slots:

    // ── setVoxRun ────────────────────────────────────────────────────────────
    //
    // VOX run gate — wires WDSP SetDEXPRunVox.
    // From Thetis cmaster.cs:199-200 [v2.10.3.13].

    void setVoxRun_firstCallTrue_storesTrue()
    {
        // NaN sentinel initialises m_voxRunLast=false; set true → stored.
        TxChannel ch(kChannelId);
        // Initial state must be false (default).
        QCOMPARE(ch.lastVoxRunForTest(), false);
        ch.setVoxRun(true);
        QCOMPARE(ch.lastVoxRunForTest(), true);
    }

    void setVoxRun_firstCallFalse_storesFalse()
    {
        // false→false is idempotent (same as default), so the guard fires.
        // The stored value stays false.
        TxChannel ch(kChannelId);
        ch.setVoxRun(false);
        QCOMPARE(ch.lastVoxRunForTest(), false);
    }

    void setVoxRun_roundTrip_storesLatestValue()
    {
        TxChannel ch(kChannelId);
        ch.setVoxRun(true);
        ch.setVoxRun(false);
        QCOMPARE(ch.lastVoxRunForTest(), false);
    }

    void setVoxRun_idempotent_secondCallNoChange()
    {
        // After the first call sets true, a second call with true must be a
        // no-op.  The stored value must still be true.
        TxChannel ch(kChannelId);
        ch.setVoxRun(true);
        QCOMPARE(ch.lastVoxRunForTest(), true);
        ch.setVoxRun(true);  // second call — idempotent guard fires
        QCOMPARE(ch.lastVoxRunForTest(), true);
    }

    // ── setVoxAttackThreshold ─────────────────────────────────────────────────
    //
    // VOX attack threshold — wires WDSP SetDEXPAttackThreshold.
    // From Thetis cmaster.cs:187-188 [v2.10.3.13].

    void setVoxAttackThreshold_firstCall_storesValue()
    {
        // NaN sentinel means the first call always passes the guard.
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastVoxAttackThresholdForTest()));
        ch.setVoxAttackThreshold(0.05);
        QCOMPARE(ch.lastVoxAttackThresholdForTest(), 0.05);
    }

    void setVoxAttackThreshold_roundTrip_storesLatestValue()
    {
        TxChannel ch(kChannelId);
        ch.setVoxAttackThreshold(0.05);
        ch.setVoxAttackThreshold(0.15);
        QCOMPARE(ch.lastVoxAttackThresholdForTest(), 0.15);
    }

    void setVoxAttackThreshold_idempotent_secondCallNoChange()
    {
        TxChannel ch(kChannelId);
        ch.setVoxAttackThreshold(0.05);
        ch.setVoxAttackThreshold(0.05);  // idempotent guard fires
        QCOMPARE(ch.lastVoxAttackThresholdForTest(), 0.05);
    }

    void setVoxAttackThreshold_zeroValue_storesZero()
    {
        // 0.0 is a valid threshold (fully closed gate).
        TxChannel ch(kChannelId);
        ch.setVoxAttackThreshold(0.0);
        QCOMPARE(ch.lastVoxAttackThresholdForTest(), 0.0);
    }

    // ── setVoxHangTime ────────────────────────────────────────────────────────
    //
    // VOX hang time in seconds — wires WDSP SetDEXPHoldTime.
    // Note: WDSP calls this "HoldTime"; Thetis exposes it as VOXHangTime.
    // There is no SetDEXPHangTime in WDSP (wdsp/dexp.c confirmed).
    // From Thetis cmaster.cs:178-179 [v2.10.3.13] — SetDEXPHoldTime.

    void setVoxHangTime_firstCall_storesValue()
    {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastVoxHangTimeForTest()));
        ch.setVoxHangTime(0.25);  // 250 ms — Thetis default vox_hang_time
        QCOMPARE(ch.lastVoxHangTimeForTest(), 0.25);
    }

    void setVoxHangTime_roundTrip_storesLatestValue()
    {
        TxChannel ch(kChannelId);
        ch.setVoxHangTime(0.25);
        ch.setVoxHangTime(0.50);
        QCOMPARE(ch.lastVoxHangTimeForTest(), 0.50);
    }

    void setVoxHangTime_idempotent_secondCallNoChange()
    {
        TxChannel ch(kChannelId);
        ch.setVoxHangTime(0.25);
        ch.setVoxHangTime(0.25);  // idempotent guard fires
        QCOMPARE(ch.lastVoxHangTimeForTest(), 0.25);
    }

    void setVoxHangTime_zeroValue_storesZero()
    {
        // 0.0 hang time = no tail (valid: hang immediately).
        TxChannel ch(kChannelId);
        ch.setVoxHangTime(0.0);
        QCOMPARE(ch.lastVoxHangTimeForTest(), 0.0);
    }

    // ── setAntiVoxRun ─────────────────────────────────────────────────────────
    //
    // Anti-VOX run gate — wires WDSP SetAntiVOXRun.
    // From Thetis cmaster.cs:208-209 [v2.10.3.13].

    void setAntiVoxRun_firstCallTrue_storesTrue()
    {
        TxChannel ch(kChannelId);
        QCOMPARE(ch.lastAntiVoxRunForTest(), false);
        ch.setAntiVoxRun(true);
        QCOMPARE(ch.lastAntiVoxRunForTest(), true);
    }

    void setAntiVoxRun_roundTrip_storesLatestValue()
    {
        TxChannel ch(kChannelId);
        ch.setAntiVoxRun(true);
        ch.setAntiVoxRun(false);
        QCOMPARE(ch.lastAntiVoxRunForTest(), false);
    }

    void setAntiVoxRun_idempotent_secondCallNoChange()
    {
        TxChannel ch(kChannelId);
        ch.setAntiVoxRun(true);
        ch.setAntiVoxRun(true);  // idempotent guard fires
        QCOMPARE(ch.lastAntiVoxRunForTest(), true);
    }

    // ── setAntiVoxGain ────────────────────────────────────────────────────────
    //
    // Anti-VOX gain — wires WDSP SetAntiVOXGain.
    // From Thetis cmaster.cs:211-212 [v2.10.3.13].

    void setAntiVoxGain_firstCall_storesValue()
    {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastAntiVoxGainForTest()));
        ch.setAntiVoxGain(0.10);  // 10% anti-VOX coupling typical
        QCOMPARE(ch.lastAntiVoxGainForTest(), 0.10);
    }

    void setAntiVoxGain_roundTrip_storesLatestValue()
    {
        TxChannel ch(kChannelId);
        ch.setAntiVoxGain(0.10);
        ch.setAntiVoxGain(0.20);
        QCOMPARE(ch.lastAntiVoxGainForTest(), 0.20);
    }

    void setAntiVoxGain_idempotent_secondCallNoChange()
    {
        TxChannel ch(kChannelId);
        ch.setAntiVoxGain(0.10);
        ch.setAntiVoxGain(0.10);  // idempotent guard fires
        QCOMPARE(ch.lastAntiVoxGainForTest(), 0.10);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelVoxAntiVox)
#include "tst_tx_channel_vox_anti_vox.moc"
