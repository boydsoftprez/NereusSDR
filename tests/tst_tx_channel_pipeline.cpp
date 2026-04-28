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
// tests/tst_tx_channel_pipeline.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/wdsp/TXA.c, original licence above (Warren Pratt, NR0V)
//
// Ported from Thetis wdsp/TXA.c:31-479 [v2.10.3.13]
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — New test for Phase 3M-1a Task C.2: TxChannel 31-stage TXA
//                 pipeline skeleton. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: Thetis-derived stage Run defaults are attributed per-stage
// inside TxChannel.cpp; this test file verifies those defaults by exercising
// TxChannel::stageRunning() against the compile-time stub (no HAVE_WDSP in
// unit-test builds). The stage-count correction (25→31) is documented here.

#include <QtTest/QtTest>

#include "core/TxChannel.h"
#include "core/WdspEngine.h"

using namespace NereusSDR;

// Stage count from wdsp/TXA.c [v2.10.3.13] — 31 create_*() calls in create_txa().
// Pre-code review §8.1 said "25 stages"; authoritative source has 31.
// Five stages were omitted from the pre-code table (all between EqMeter and Bp0):
//   PreEmph (index 8), Leveler (9), LvlrMeter (10), CfComp (11), CfcMeter (12).
static constexpr int kExpectedStageCount = 31;
static constexpr int kTxChannelId = 1;   // WDSP.id(1, 0) — TX channel

// ---------------------------------------------------------------------------

class TestTxChannelPipeline : public QObject {
    Q_OBJECT

private slots:

    // ── Stage count ────────────────────────────────────────────────────────

    void stageCountIs31() {
        // Stage::kStageCount must equal 31 — the number of create_*() calls
        // in wdsp/TXA.c:31-479 [v2.10.3.13].
        // This is a master-design correction: pre-code review §8.1 said 25.
        QCOMPARE(static_cast<int>(TxChannel::Stage::kStageCount), kExpectedStageCount);
    }

    void stageOutMeterValueIs30() {
        // OutMeter must be the last real stage at index 30 (kStageCount - 1).
        // From wdsp/TXA.c:462 [v2.10.3.13] — last create_*() call in create_txa().
        QCOMPARE(static_cast<int>(TxChannel::Stage::OutMeter), kExpectedStageCount - 1);
    }

    // ── TxChannel direct construction (does not require WdspEngine) ────────

    void channelIdIsStored() {
        TxChannel ch(kTxChannelId);
        QCOMPARE(ch.channelId(), kTxChannelId);
    }

    void differentChannelIdIsStored() {
        TxChannel ch(42);
        QCOMPARE(ch.channelId(), 42);
    }

    // ── Default Run states (stub path — no HAVE_WDSP in unit-test builds) ──
    //
    // The stub path in TxChannel::stageRunning() returns compile-time defaults
    // matching the first argument of each create_*() call in
    // wdsp/TXA.c:31-479 [v2.10.3.13].
    //
    // Stages that default to ON (run=1 in create_txa):
    //   Panel, MicMeter, EqMeter, LvlrMeter, CfcMeter, Bp0, CompMeter, Alc,
    //   AlcMeter, Sip1, Calcc, OutMeter
    //
    // Stages that default to OFF (run=0 or no run param):
    //   RsmpIn, Gen0, PhRot, AmSq, Eqp, PreEmph, Leveler, CfComp,
    //   Compressor, Bp1, OsCtrl, Bp2, AmMod, FmMod, Gen1, UsLew,
    //   Iqc, Cfir, RsmpOut

    void defaultRun_RsmpIn_isOff() {
        // TXA.c:40 run=0 — rsmpin (turned on later if rate conversion needed)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::RsmpIn));
    }

    void defaultRun_Gen0_isOff() {
        // TXA.c:51 run=0 — gen0 (PreGen, mode=2 noise, for 2-TONE testing)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Gen0));
    }

    void defaultRun_Panel_isOn() {
        // TXA.c:59 run=1 — panel (audio panel/level control)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::Panel));
    }

    void defaultRun_PhRot_isOff() {
        // TXA.c:71 run=0 — phrot (phase rotator)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::PhRot));
    }

    void defaultRun_MicMeter_isOn() {
        // TXA.c:80 run=1 — micmeter
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::MicMeter));
    }

    void defaultRun_AmSq_isOff() {
        // TXA.c:95 run=0 — amsq (AM squelch)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::AmSq));
    }

    void defaultRun_Eqp_isOff() {
        // TXA.c:115 run=0 — eqp (parametric EQ, OFF by default)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Eqp));
    }

    void defaultRun_EqMeter_isOn() {
        // TXA.c:130 run=1 — eqmeter (gated on eqp.run via second param)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::EqMeter));
    }

    void defaultRun_PreEmph_isOff() {
        // TXA.c:145 run=0 — preemph (pre-emphasis filter)
        // NOTE: absent from pre-code review §8.1 table — stage count correction.
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::PreEmph));
    }

    void defaultRun_Leveler_isOff() {
        // TXA.c:158 run=0 — leveler (wcpagc, disabled by default)
        // NOTE: absent from pre-code review §8.1 table — stage count correction.
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Leveler));
    }

    void defaultRun_LvlrMeter_isOn() {
        // TXA.c:183 run=1 — lvlrmeter (gated on leveler.run)
        // NOTE: absent from pre-code review §8.1 table — stage count correction.
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::LvlrMeter));
    }

    void defaultRun_CfComp_isOff() {
        // TXA.c:202 run=0 — cfcomp (multiband compander)
        // NOTE: absent from pre-code review §8.1 table — stage count correction.
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::CfComp));
    }

    void defaultRun_CfcMeter_isOn() {
        // TXA.c:224 run=1 — cfcmeter (gated on cfcomp.run)
        // NOTE: absent from pre-code review §8.1 table — stage count correction.
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::CfcMeter));
    }

    void defaultRun_Bp0_isOn() {
        // TXA.c:239 run=1 — bp0 (mandatory BPF, always runs)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::Bp0));
    }

    void defaultRun_Compressor_isOff() {
        // TXA.c:253 run=0 — compressor (OFF by default)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Compressor));
    }

    void defaultRun_Bp1_isOff() {
        // TXA.c:260 run=0 — bp1 (only runs when compressor is used)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Bp1));
    }

    void defaultRun_OsCtrl_isOff() {
        // TXA.c:274 run=0 — osctrl (output soft clip)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::OsCtrl));
    }

    void defaultRun_Bp2_isOff() {
        // TXA.c:282 run=0 — bp2 (only runs when compressor is used)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Bp2));
    }

    void defaultRun_CompMeter_isOn() {
        // TXA.c:296 run=1 — compmeter (gated on compressor.run)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::CompMeter));
    }

    void defaultRun_Alc_isOn() {
        // TXA.c:311 run=1 — alc (ALC, always-on AGC)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::Alc));
    }

    void defaultRun_AmMod_isOff() {
        // TXA.c:336 run=0 — ammod (AM modulation, OFF by default)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::AmMod));
    }

    void defaultRun_FmMod_isOff() {
        // TXA.c:345 run=0 — fmmod (FM modulation, OFF by default)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::FmMod));
    }

    void defaultRun_Gen1_isOff() {
        // TXA.c:361 run=0 — gen1 (PostGen, TUNE tone source, off at rest)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Gen1));
    }

    void defaultRun_UsLew_isOff() {
        // TXA.c:369 — uslew: create_uslew() has no "run" parameter.
        // The upslew ramp is driven by the channel-level upslew flag
        // (ch[].iob.ch_upslew), not a stage-level run bit.
        // stageRunning(UsLew) always returns false by design; the ramp
        // activates at channel-state change time, not via a run field.
        // From wdsp/TXA.c:369-377 [v2.10.3.13] + wdsp/slew.c:62-75 [v2.10.3.13].
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::UsLew));
    }

    void defaultRun_AlcMeter_isOn() {
        // TXA.c:379 run=1 — alcmeter (ALC telemetry)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::AlcMeter));
    }

    void defaultRun_Sip1_isOn() {
        // TXA.c:394 run=1 — sip1 (siphon for TX spectrum capture)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::Sip1));
    }

    void defaultRun_Calcc_isOn() {
        // TXA.c:405 run=1 — calcc (PureSignal calibration, on but unused until 3M-4)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::Calcc));
    }

    void defaultRun_Iqc_isOff() {
        // TXA.c:424 run=0 — iqc (IQ correction)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Iqc));
    }

    void defaultRun_Cfir_isOff() {
        // TXA.c:434 run=0 — cfir (custom CIC FIR, turned on if needed)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::Cfir));
    }

    void defaultRun_RsmpOut_isOff() {
        // TXA.c:451 run=0 — rsmpout (output resampler, turned on if needed)
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.stageRunning(TxChannel::Stage::RsmpOut));
    }

    void defaultRun_OutMeter_isOn() {
        // TXA.c:462 run=1 — outmeter (output telemetry)
        TxChannel ch(kTxChannelId);
        QVERIFY(ch.stageRunning(TxChannel::Stage::OutMeter));
    }

    // ── WdspEngine integration (creates TxChannel wrapper) ─────────────────
    //
    // Without WDSP (unit-test build), createTxChannel fails at the
    // not-initialized guard and returns nullptr.  The stub-path tests above
    // cover stageRunning() by constructing TxChannel directly.
    // These tests verify the engine API contract and that the map is managed
    // correctly around the create/destroy lifecycle.

    void engineTxChannelIsNullWithoutWdsp() {
        // With WDSP not initialized, createTxChannel cannot build the channel.
        WdspEngine engine;
        TxChannel* tx = engine.createTxChannel(kTxChannelId);
        QVERIFY(tx == nullptr);
    }

    void engineTxChannelLookupIsNullWithoutWdsp() {
        // txChannel() returns nullptr when no channel was ever created.
        WdspEngine engine;
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);
    }

    void engineDestroyTxChannelIsIdempotent() {
        // destroyTxChannel on a never-created channel must not crash.
        WdspEngine engine;
        engine.destroyTxChannel(kTxChannelId);  // first call — safe no-op
        engine.destroyTxChannel(kTxChannelId);  // second call — still safe
    }

    void engineTxChannelIsNullAfterDestroy() {
        // After destroy, txChannel lookup returns nullptr.
        WdspEngine engine;
        engine.createTxChannel(kTxChannelId);   // returns nullptr (no WDSP)
        engine.destroyTxChannel(kTxChannelId);
        QVERIFY(engine.txChannel(kTxChannelId) == nullptr);
    }

    // ── Stage count consistency: enum covers every stage in create_txa() ───

    void allStagesAreCovered() {
        // Loop over all stage ordinals and verify stageRunning() doesn't crash.
        // This is a smoke test that every case in the switch is reachable.
        TxChannel ch(kTxChannelId);
        int count = 0;
        for (int i = 0; i < static_cast<int>(TxChannel::Stage::kStageCount); ++i) {
            // stageRunning() must not crash for any valid stage ordinal.
            (void)ch.stageRunning(static_cast<TxChannel::Stage>(i));
            ++count;
        }
        QCOMPARE(count, kExpectedStageCount);
    }

    void onStageCountMatchesSpec() {
        // Count how many stages default to ON and OFF per create_txa() spec.
        // ON (run=1):  Panel, MicMeter, EqMeter, LvlrMeter, CfcMeter, Bp0,
        //              CompMeter, Alc, AlcMeter, Sip1, Calcc, OutMeter  → 12
        // OFF (run=0 or no run): everything else → 19
        // Total: 31
        TxChannel ch(kTxChannelId);
        int onCount  = 0;
        int offCount = 0;
        for (int i = 0; i < static_cast<int>(TxChannel::Stage::kStageCount); ++i) {
            if (ch.stageRunning(static_cast<TxChannel::Stage>(i))) {
                ++onCount;
            } else {
                ++offCount;
            }
        }
        QCOMPARE(onCount,  12);   // 12 stages ON by default in create_txa()
        QCOMPARE(offCount, 19);   // 19 stages OFF (including UsLew with no run param)
        QCOMPARE(onCount + offCount, kExpectedStageCount);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelPipeline)
#include "tst_tx_channel_pipeline.moc"
