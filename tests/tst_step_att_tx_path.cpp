// =================================================================
// tests/tst_step_att_tx_path.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-26 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//                 Task: Phase 3M-1a Task F.2 — StepAttenuatorController
//                 TX-path activation tests.
//                 Thetis cite: console.cs:29546-29576 [v2.10.3.13]
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// no-port-check: Test file exercises NereusSDR API; the Thetis logic
// being tested is cited in StepAttenuatorController.cpp via inline cites
// referencing console.cs:29546-29576 [v2.10.3.13]. No C# is translated here.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/WdspTypes.h"
#include "models/Band.h"

using namespace NereusSDR;

// ── MockTxConnection ─────────────────────────────────────────────────────────
// Minimal RadioConnection that records setTxStepAttenuation calls.
// Also tracks setPreamp calls to verify HPSDR preamp save/restore.
class MockTxConnection : public RadioConnection {
    Q_OBJECT
public:
    int lastTxStepAtt{-1};      // last value passed to setTxStepAttenuation
    int txStepAttCallCount{0};  // number of calls to setTxStepAttenuation
    bool lastSetPreampArg{false};
    int setPreampCallCount{0};

    explicit MockTxConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    // ── Pure-virtual stubs ────────────────────────────────────────────────────
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool en) override {
        lastSetPreampArg = en;
        ++setPreampCallCount;
    }
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setWatchdogEnabled(bool) override {}
    void sendTxIq(const float*, int) override {}
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

    void setTxStepAttenuation(int dB) override {
        lastTxStepAtt = dB;
        ++txStepAttCallCount;
    }
};

// ── Helpers ───────────────────────────────────────────────────────────────────

// Build a controller pre-configured for non-HPSDR (standard board), with
// ATT-on-TX enabled, PS-A off (force-31 active when m_forceAttWhenPsOff=true),
// 20m band, USB mode, and a given stored TX ATT for 20m.
static StepAttenuatorController* makeStdCtrl(int txAtt20m = 10)
{
    auto* ctrl = new StepAttenuatorController();
    ctrl->setTickTimerEnabled(false);
    ctrl->setIsHpsdrBoard(false);
    ctrl->setAttOnTxEnabled(true);
    ctrl->setForceAttWhenPsOff(true);
    ctrl->setPsActive(false);                  // PS-A off
    ctrl->setCurrentDspMode(DSPMode::USB);
    ctrl->setBand(Band::Band20m);
    ctrl->setTxAttenuationForBand(Band::Band20m, txAtt20m);
    return ctrl;
}

// ── Test class ───────────────────────────────────────────────────────────────
class TestStepAttTxPath : public QObject {
    Q_OBJECT

private:
    void clearAppSettings() { AppSettings::instance().clear(); }

private slots:
    void initTestCase() { clearAppSettings(); }
    void init()          { clearAppSettings(); }
    void cleanup()       { clearAppSettings(); }

    // ── Test 1: ATT-on-TX disabled — no TX ATT change ────────────────────────
    // When m_attOnTxEnabled is false, onMoxHardwareFlipped(true) must push
    // 0 dB (cleared) regardless of band storage.
    // From Thetis console.cs:29575 [v2.10.3.13]:
    //   NetworkIO.SetTxAttenData(0);
    //   Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes
    void isTxTrue_attOnTxDisabled_pushesZero()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setIsHpsdrBoard(false);
        ctrl.setAttOnTxEnabled(false);   // ← disabled
        ctrl.setTxAttenuationForBand(Band::Band20m, 15);
        ctrl.setBand(Band::Band20m);

        auto* mock = new MockTxConnection();
        ctrl.setRadioConnection(mock);

        ctrl.onMoxHardwareFlipped(true);

        QCOMPARE(mock->lastTxStepAtt, 0);
        QCOMPARE(mock->txStepAttCallCount, 1);
        QCOMPARE(ctrl.lastTxStepAttForTest(), 0);

        ctrl.setRadioConnection(nullptr);
        delete mock;
    }

    // ── Test 2: ATT-on-TX enabled, 20m USB, PS on, no-force → stored value ──
    // No force-31 condition active: PS-A is on, mode is USB (non-CW).
    // The stored TX ATT for 20m should be applied as-is.
    // From Thetis console.cs:29561-29568 [v2.10.3.13]:
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
    //   int txAtt = getTXstepAttenuatorForBand(_tx_band);
    //   SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes
    void isTxTrue_noForce_appliesStoredTxAtt()
    {
        auto* ctrl = makeStdCtrl(/*txAtt20m=*/15);
        ctrl->setPsActive(true);          // PS-A ON → not "PS off"
        ctrl->setForceAttWhenPsOff(true); // flag on, but PS is active → no force

        auto* mock = new MockTxConnection();
        ctrl->setRadioConnection(mock);

        ctrl->onMoxHardwareFlipped(true);

        QCOMPARE(mock->lastTxStepAtt, 15);
        QCOMPARE(ctrl->lastTxStepAttForTest(), 15);

        ctrl->setRadioConnection(nullptr);
        delete ctrl;
        delete mock;
    }

    // ── Test 3: PS off + forceAttWhenPsOff → force 31 dB ────────────────────
    // Thetis console.cs:29563 [v2.10.3.13] //MW0LGE [2.9.0.7] added:
    //   if ((!chkFWCATUBypass.Checked && _forceATTwhenPSAoff) || CW) txAtt = 31
    void isTxTrue_psOff_forceFlag_forces31()
    {
        auto* ctrl = makeStdCtrl(/*txAtt20m=*/10);
        ctrl->setPsActive(false);         // PS-A off
        ctrl->setForceAttWhenPsOff(true); // force enabled

        auto* mock = new MockTxConnection();
        ctrl->setRadioConnection(mock);

        ctrl->onMoxHardwareFlipped(true);

        QCOMPARE(mock->lastTxStepAtt, 31);
        QCOMPARE(ctrl->lastTxStepAttForTest(), 31);

        ctrl->setRadioConnection(nullptr);
        delete ctrl;
        delete mock;
    }

    // ── Test 4: CWL mode → force 31 dB ──────────────────────────────────────
    // From Thetis console.cs:29561-29568 [v2.10.3.13]: CWL forces 31 dB.
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
    //   (radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWL || ...) txAtt = 31
    //   SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes
    void isTxTrue_cwlMode_forces31()
    {
        auto* ctrl = makeStdCtrl(/*txAtt20m=*/8);
        ctrl->setPsActive(true);          // PS-A on (force-by-PS path disabled)
        ctrl->setCurrentDspMode(DSPMode::CWL);

        auto* mock = new MockTxConnection();
        ctrl->setRadioConnection(mock);

        ctrl->onMoxHardwareFlipped(true);

        QCOMPARE(mock->lastTxStepAtt, 31);
        QCOMPARE(ctrl->lastTxStepAttForTest(), 31);

        ctrl->setRadioConnection(nullptr);
        delete ctrl;
        delete mock;
    }

    // ── Test 5: CWU mode → force 31 dB ──────────────────────────────────────
    // From Thetis console.cs:29561-29568 [v2.10.3.13]: CWU also forces 31 dB.
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
    //   (... || DSPMode.CWU)) txAtt = 31; //[2.10.3.6]MW0LGE att_fixes
    void isTxTrue_cwuMode_forces31()
    {
        auto* ctrl = makeStdCtrl(/*txAtt20m=*/8);
        ctrl->setPsActive(true);
        ctrl->setCurrentDspMode(DSPMode::CWU);

        auto* mock = new MockTxConnection();
        ctrl->setRadioConnection(mock);

        ctrl->onMoxHardwareFlipped(true);

        QCOMPARE(mock->lastTxStepAtt, 31);

        ctrl->setRadioConnection(nullptr);
        delete ctrl;
        delete mock;
    }

    // ── Test 6: USB mode + PS on → stored TX ATT (no force) ─────────────────
    // PS-A on, USB mode: neither force condition is active.
    // Stored TX ATT (12 dB for 20m) should be applied unchanged.
    void isTxTrue_usbMode_psOn_appliesStoredAtt()
    {
        auto* ctrl = makeStdCtrl(/*txAtt20m=*/12);
        ctrl->setPsActive(true);          // PS-A on
        ctrl->setCurrentDspMode(DSPMode::USB);

        auto* mock = new MockTxConnection();
        ctrl->setRadioConnection(mock);

        ctrl->onMoxHardwareFlipped(true);

        QCOMPARE(mock->lastTxStepAtt, 12);

        ctrl->setRadioConnection(nullptr);
        delete ctrl;
        delete mock;
    }

    // ── Test 7: isTx=false → restore RX path (clear TX ATT to 0) ────────────
    // TX→RX: TX step ATT must be cleared. RX ATT restore uses the existing
    // per-band RX value.
    void isTxFalse_clearsTxAtt()
    {
        auto* ctrl = makeStdCtrl(/*txAtt20m=*/20);
        ctrl->setAttenuation(10, 0);      // RX ATT = 10 dB for 20m

        auto* mock = new MockTxConnection();
        ctrl->setRadioConnection(mock);

        // Simulate MOX on then off.
        ctrl->onMoxHardwareFlipped(true);
        QCOMPARE(mock->lastTxStepAtt, 31); // force-31 (PS off + forceFlag)

        ctrl->onMoxHardwareFlipped(false);
        QCOMPARE(mock->lastTxStepAtt, 0);  // TX ATT cleared on TX→RX
        QCOMPARE(ctrl->lastTxStepAttForTest(), 0);

        ctrl->setRadioConnection(nullptr);
        delete ctrl;
        delete mock;
    }

    // ── Test 8: HPSDR board, isTx=true → saves preamp, sets Minus20 ─────────
    // From Thetis console.cs:29550-29554 [v2.10.3.13]:
    //   temp_mode = RX1PreampMode;
    //   RX1PreampMode = PreampMode.HPSDR_OFF;  // set to -20dB
    void isTxTrue_hpsdrBoard_savesPreampAndForcesMinus20()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setIsHpsdrBoard(true);
        ctrl.setAttOnTxEnabled(true);
        ctrl.setPreampMode(PreampMode::On);  // current mode = On

        auto* mock = new MockTxConnection();
        ctrl.setRadioConnection(mock);

        ctrl.onMoxHardwareFlipped(true);

        // Saved mode must be the pre-TX mode (On).
        QCOMPARE(ctrl.savedPreampModeForTest(), PreampMode::On);

        // Current mode must now be Minus20 (≡ HPSDR_OFF).
        QCOMPARE(ctrl.preampMode(), PreampMode::Minus20);

        // No TX step ATT push on HPSDR path.
        QCOMPARE(mock->txStepAttCallCount, 0);

        ctrl.setRadioConnection(nullptr);
        delete mock;
    }

    // ── Test 9: HPSDR board, isTx=false → restores saved preamp ─────────────
    // From Thetis TX→RX restore: RX1PreampMode = temp_mode.
    void isTxFalse_hpsdrBoard_restoresSavedPreamp()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setIsHpsdrBoard(true);
        ctrl.setAttOnTxEnabled(true);
        ctrl.setPreampMode(PreampMode::On);

        auto* mock = new MockTxConnection();
        ctrl.setRadioConnection(mock);

        // Go TX: saves On, sets Minus20.
        ctrl.onMoxHardwareFlipped(true);
        QCOMPARE(ctrl.preampMode(), PreampMode::Minus20);

        // Go RX: should restore On.
        ctrl.onMoxHardwareFlipped(false);
        QCOMPARE(ctrl.preampMode(), PreampMode::On);

        ctrl.setRadioConnection(nullptr);
        delete mock;
    }

    // ── Test 10: shouldForce31Db predicate — table-driven ────────────────────
    // Exhaustively covers all combinations of the two inputs.
    // From Thetis console.cs:29563-29566 [v2.10.3.13] //MW0LGE [2.9.0.7] added.
    void shouldForce31Db_tableDriven()
    {
        struct Case {
            DSPMode mode;
            bool isPsOff;
            bool forceFlag;   // m_forceAttWhenPsOff
            bool attOnTx;     // m_attOnTxEnabled
            bool expected;
            const char* label;
        };

        // Table covers all four §6.3 conditions and the master-disable guard.
        const Case cases[] = {
            // attOnTx disabled → never force regardless of other conditions
            { DSPMode::USB,  true,  true,  false, false, "attOnTx=off → no force" },
            { DSPMode::CWL,  true,  true,  false, false, "attOnTx=off+CWL → no force" },
            // PS off + forceFlag → force
            { DSPMode::USB,  true,  true,  true,  true,  "psOff+forceFlag → force" },
            // PS on, forceFlag on → no force (PS is active)
            { DSPMode::USB,  false, true,  true,  false, "psOn+forceFlag → no force" },
            // PS off, forceFlag off → no force from PS path
            { DSPMode::USB,  true,  false, true,  false, "psOff+noForceFlag → no force" },
            // CWL regardless of PS/force → force
            { DSPMode::CWL,  false, false, true,  true,  "CWL → force" },
            { DSPMode::CWL,  true,  false, true,  true,  "CWL+psOff → force" },
            // CWU regardless of PS/force → force
            { DSPMode::CWU,  false, false, true,  true,  "CWU → force" },
            { DSPMode::CWU,  true,  true,  true,  true,  "CWU+psOff+forceFlag → force" },
            // Non-CW modes, PS on, no-force → no force
            { DSPMode::LSB,  false, true,  true,  false, "LSB+psOn → no force" },
            { DSPMode::USB,  false, false, true,  false, "USB+psOn+noFlag → no force" },
            { DSPMode::FM,   false, false, true,  false, "FM → no force" },
        };

        for (const auto& c : cases) {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setAttOnTxEnabled(c.attOnTx);
            ctrl.setForceAttWhenPsOff(c.forceFlag);

            bool result = ctrl.shouldForce31Db(c.mode, c.isPsOff);
            if (result != c.expected) {
                qWarning("FAIL case: %s — expected %d got %d",
                         c.label, (int)c.expected, (int)result);
            }
            QVERIFY2(result == c.expected, c.label);
        }
    }

    // ── Test 11: TX ATT per-band storage round-trip via setter/getter ─────────
    // Verifies setTxAttenuationForBand / txAttenuationForBand round-trip for
    // all 14 Band enum values.
    void txAttByBand_roundTrip()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        // Write a distinct value per band.
        // Per-band TX ATT is HF amateur + GEN/WWV/XVTR; SWL bands
        // (Phase 3L Band enum extension) inherit ham values.
        for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
            ctrl.setTxAttenuationForBand(static_cast<Band>(b), b);
        }

        // Read back and verify.
        // Per-band TX ATT is HF amateur + GEN/WWV/XVTR; SWL bands
        // (Phase 3L Band enum extension) inherit ham values.
        for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
            int expected = b;
            int got = ctrl.txAttenuationForBand(static_cast<Band>(b));
            QCOMPARE(got, expected);
        }
    }

    // ── Phase 3L: signed HL2 ATT range honoured by setAttenuation clamp ───────
    // Codex P1 in PR #157: setAttenuation() was hard-clamping below zero, so
    // any negative value the HL2 user picked got snapped back to 0 before
    // reaching the codec.  After the fix, the lower bound is m_minAttDb
    // (set via setMinAttenuation), not literal 0.
    void setAttenuation_clampsToSignedMin_forHl2Range()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setMinAttenuation(-28);   // mi0bot HL2 lower bound
        ctrl.setMaxAttenuation(32);    // mi0bot HL2 upper bound

        // Negative-but-in-range values must round-trip unchanged.
        ctrl.setAttenuation(-15, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), -15);

        ctrl.setAttenuation(-28, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), -28);

        // Below-min values clamp to m_minAttDb, not to 0.
        ctrl.setAttenuation(-100, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), -28);

        // Above-max values clamp to m_maxAttDb (existing behaviour preserved).
        ctrl.setAttenuation(999, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), 32);

        // Zero in the middle of the HL2 range works (regression check —
        // the old clamp made this the de-facto floor).
        ctrl.setAttenuation(0, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), 0);
    }

    // Legacy boards (ANAN/Hermes) keep m_minAttDb=0 and behaviour is
    // identical to pre-PR #157 — negative inputs still snap to 0.
    void setAttenuation_legacyZeroMin_unchanged()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        // Default m_minAttDb is 0; m_maxAttDb default whatever — set to 31.
        ctrl.setMaxAttenuation(31);

        ctrl.setAttenuation(-5, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), 0);
        ctrl.setAttenuation(15, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), 15);
        ctrl.setAttenuation(99, /*rx=*/0);
        QCOMPARE(ctrl.attenuatorDb(), 31);
    }

    // ── Test 12: no connection — graceful no-op ───────────────────────────────
    // When no RadioConnection is set, onMoxHardwareFlipped must not crash.
    void noConnection_gracefulNoOp()
    {
        auto* ctrl = makeStdCtrl(10);
        // Do NOT call setRadioConnection — m_connection is null.

        // Must not crash on RX→TX.
        ctrl->onMoxHardwareFlipped(true);
        // Test seam: force-31 active (PS off + forceFlag), so last pushed = 31.
        QCOMPARE(ctrl->lastTxStepAttForTest(), 31);

        // Must not crash on TX→RX.
        ctrl->onMoxHardwareFlipped(false);
        // TX→RX clears to 0.
        QCOMPARE(ctrl->lastTxStepAttForTest(), 0);

        delete ctrl;
    }
};

QTEST_MAIN(TestStepAttTxPath)
#include "tst_step_att_tx_path.moc"
