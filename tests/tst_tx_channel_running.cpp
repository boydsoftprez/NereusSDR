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
// tests/tst_tx_channel_running.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs — TX channel-state callsites
//   Project Files/Source/Console/cmaster.cs — cfir P2 activation
//   Project Files/Source/wdsp/channel.c     — SetChannelState implementation
//   Project Files/Source/wdsp/cfir.c        — SetTXACFIRRun implementation
//   Project Files/Source/wdsp/TXA.c        — create_txa pipeline
//   Original licences above (Warren Pratt, NR0V)
//
// Ported from Thetis console.cs:29595, 29607 [v2.10.3.13]
// Ported from Thetis cmaster.cs:522-527 [v2.10.3.13]
// Ported from Thetis wdsp/channel.c:259-294 [v2.10.3.13]
// Ported from Thetis wdsp/cfir.c:233-238 [v2.10.3.13]
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-26 — New test for Phase 3M-1a Task C.4: TxChannel::setRunning()
//                 (channel state + 3M-1a active-stage activation) and
//                 setStageRunning(Stage, bool). J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: Attribution for ported constants and logic lives in
// TxChannel.h/cpp with verbatim Thetis source cites.  This test file
// exercises the interface only; all cite comments are in the implementation.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP channel IDs used in Thetis — from cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;   // WDSP.id(1, 0) — TX channel

// ---------------------------------------------------------------------------

class TestTxChannelRunning : public QObject {
    Q_OBJECT

private slots:

    // ── isRunning() initial state ──────────────────────────────────────────
    //
    // A freshly constructed TxChannel must default to stopped (m_running=false).
    // This ensures that nothing starts transmitting until setRunning(true) is
    // called explicitly by MoxController.

    void initialState_isRunningFalse() {
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.isRunning());
    }

    // ── setRunning(true) — crash-safety (no HAVE_WDSP / no open channel) ──
    //
    // Without WDSP, setRunning() is a no-op except for tracking m_running.
    // The tests below verify no crash / assertion / UB occurs.

    void setRunningOn_doesNotCrash() {
        // setRunning(true) — RX→TX transition.
        // From Thetis console.cs:29595 [v2.10.3.13]:
        //   WDSP.SetChannelState(WDSP.id(1, 0), 1, 0);
        TxChannel ch(kTxChannelId);
        ch.setRunning(true);   // no crash
    }

    void setRunningOff_doesNotCrash() {
        // setRunning(false) — TX→RX transition (drain).
        // From Thetis console.cs:29607 [v2.10.3.13]:
        //   WDSP.SetChannelState(WDSP.id(1, 0), 0, 1);
        // Preceded by: Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE  [console.cs:29603]
        TxChannel ch(kTxChannelId);
        ch.setRunning(false);  // no crash
    }

    // ── isRunning() reflects setRunning() ─────────────────────────────────
    //
    // setRunning(on) must update m_running so isRunning() returns the expected
    // value.  This is the primary observable contract for 3M-1a callers
    // (MoxController queries isRunning() before sending I/Q data).

    void setRunningOn_isRunningReturnsTrue() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(true);
        QVERIFY(ch.isRunning());
    }

    void setRunningOff_isRunningReturnsFalse() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(true);
        ch.setRunning(false);
        QVERIFY(!ch.isRunning());
    }

    // ── Toggle sequence — crash-safety ────────────────────────────────────
    //
    // Verify on→off→on cycling doesn't crash.

    void toggleOnThenOff_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(true);
        ch.setRunning(false);
    }

    void toggleOffThenOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(false);
        ch.setRunning(true);
    }

    void toggleOnOffOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(true);
        ch.setRunning(false);
        ch.setRunning(true);
    }

    // ── Idempotency ────────────────────────────────────────────────────────
    //
    // Calling setRunning(true) twice must be safe (no double-init or crash).
    // Thetis does not gate the SetChannelState call on whether the channel is
    // already in the target state — it calls it unconditionally.  NereusSDR
    // matches this: SetChannelState itself checks ch[channel].state internally
    // (wdsp/channel.c:265 [v2.10.3.13]: if (ch[channel].state != state) ...).

    void idempotentOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(true);
        ch.setRunning(true);   // second call — no crash
    }

    void idempotentOff_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(false);
        ch.setRunning(false);  // second call — no crash
    }

    void idempotentOn_isRunningStaysTrue() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(true);
        ch.setRunning(true);   // second call must not clear isRunning()
        QVERIFY(ch.isRunning());
    }

    void idempotentOff_isRunningStaysFalse() {
        TxChannel ch(kTxChannelId);
        ch.setRunning(false);
        ch.setRunning(false);  // second call must not flip isRunning()
        QVERIFY(!ch.isRunning());
    }

    // ── Rapid toggle — crash-safety ────────────────────────────────────────

    void repeatedToggle_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        for (int i = 0; i < 10; ++i) {
            ch.setRunning(true);
            ch.setRunning(false);
        }
    }

    // ── setStageRunning(Stage, bool) — crash-safety ────────────────────────
    //
    // setStageRunning() must not crash for any supported stage in either
    // run=true or run=false direction.

    void setStageRunningGen0On_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Gen0, true);
    }

    void setStageRunningGen0Off_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Gen0, false);
    }

    void setStageRunningGen1On_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Gen1, true);
    }

    void setStageRunningPanelOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Panel, true);
    }

    void setStageRunningPhRotOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::PhRot, true);
    }

    void setStageRunningAmSqOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::AmSq, true);
    }

    void setStageRunningEqpOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Eqp, true);
    }

    void setStageRunningCompressorOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Compressor, true);
    }

    void setStageRunningOsCtrlOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::OsCtrl, true);
    }

    void setStageRunningCfirOn_doesNotCrash() {
        // cfir is also activated by setRunning(true).
        // setStageRunning(Cfir, true) is the fine-grained equivalent.
        // From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] — SetTXACFIRRun.
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Cfir, true);
    }

    void setStageRunningCfirOff_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::Cfir, false);
    }

    void setStageRunningCfCompOn_doesNotCrash() {
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::CfComp, true);
    }

    // ── Unsupported stages log a warning and return (no crash) ────────────
    //
    // Stages with no public WDSP Set*Run API (rsmpin, rsmpout, uslew, meters,
    // etc.) must not crash — setStageRunning warns and is a no-op.

    void setStageRunningRsmpIn_doesNotCrash() {
        // rsmpin: managed by TXAResCheck() — wdsp/TXA.c:809-817 [v2.10.3.13]
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::RsmpIn, true);   // warning + no-op
    }

    void setStageRunningRsmpOut_doesNotCrash() {
        // rsmpout: managed by TXAResCheck() — wdsp/TXA.c:809-817 [v2.10.3.13]
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::RsmpOut, true);  // warning + no-op
    }

    void setStageRunningUsLew_doesNotCrash() {
        // uslew: no run flag — channel-upslew driven (see stageRunning comment)
        TxChannel ch(kTxChannelId);
        ch.setStageRunning(TxChannel::Stage::UsLew, true);    // warning + no-op
    }

#ifdef HAVE_WDSP
    // ── WDSP struct-field tests (only when WDSP is compiled in) ───────────
    //
    // These tests require that an OpenChannel(type=1) call has been made to
    // initialize the TXA pipeline.  Without WdspEngine::initialize(), the
    // txa[] global is zero-initialized (all pointers null) and these tests
    // would segfault.  They are therefore SKIPPED until a full integration
    // harness that calls WdspEngine::initialize() is available.
    //
    // The null-guard in setRunning() / setStageRunning() catches this case
    // (see TxChannel.cpp) and returns early rather than crashing.

    void wdsp_setRunningOn_channelStateIsOn() {
        // After setRunning(true), isRunning() must be true and (with an open
        // channel) the WDSP channel state would be 1.
        // From Thetis wdsp/channel.c:259-294 [v2.10.3.13] — SetChannelState.
        //
        // SKIPPED: the unit-test build constructs TxChannel standalone; the
        // txa[].rsmpin.p null-guard fires and returns early (no segfault, but
        // also no WDSP struct assertion). Replace with a field check once an
        // integration harness that opens kTxChannelId is available.
        QSKIP("Integration harness pending: WDSP channel not opened in this test build.");
    }

    void wdsp_setRunningOff_channelStateIsOff() {
        // After setRunning(false), isRunning() must be false.
        // From Thetis wdsp/channel.c:259-294 [v2.10.3.13] — SetChannelState.
        //
        // SKIPPED — see wdsp_setRunningOn_channelStateIsOn above for rationale.
        QSKIP("Integration harness pending: WDSP channel not opened in this test build.");
    }

    void wdsp_setRunningOn_cfirIsOn() {
        // After setRunning(true), cfir must be active (SetTXACFIRRun was called).
        // From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] — SetTXACFIRRun.
        // From Thetis cmaster.cs:526-527 [v2.10.3.13] — cfir always ON for P2.
        //
        // SKIPPED — see wdsp_setRunningOn_channelStateIsOn above for rationale.
        // When integration harness arrives, replace with:
        //   QVERIFY(txa[kTxChannelId].cfir.p->run == 1);
        QSKIP("Integration harness pending: WDSP channel not opened in this test build.");
    }

    void wdsp_setRunningOff_cfirIsOff() {
        // After setRunning(false), cfir must be inactive.
        // From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] — SetTXACFIRRun.
        //
        // SKIPPED — see wdsp_setRunningOn_channelStateIsOn above for rationale.
        QSKIP("Integration harness pending: WDSP channel not opened in this test build.");
    }
#endif // HAVE_WDSP
};

QTEST_APPLESS_MAIN(TestTxChannelRunning)
#include "tst_tx_channel_running.moc"
