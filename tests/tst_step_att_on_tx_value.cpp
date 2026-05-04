// =================================================================
// tests/tst_step_att_on_tx_value.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//                 Phase 1 Agent 1D of #167 — exercises
//                 StepAttenuatorController::setAttOnTxValue, the setter
//                 used by the ATT-on-TX-on-power-change safety gate
//                 (Thetis console.cs:46740-46748 [v2.10.3.13]).
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// referencing setup.cs:3988-4017 [v2.10.3.13] (mi0bot HL2 range fork) and
// console.cs:46740-46748 [v2.10.3.13]. No C# is translated here.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/StepAttenuatorController.h"
#include "models/Band.h"

using namespace NereusSDR;

class TestStepAttOnTxValue : public QObject {
    Q_OBJECT

private:
    static constexpr const char* kTestMac = "AA:BB:CC:DD:EE:FF";
    void clearAppSettings() { AppSettings::instance().clear(); }

private slots:
    void initTestCase() { clearAppSettings(); }
    void init()          { clearAppSettings(); }
    void cleanup()       { clearAppSettings(); }

    // ── Test 1: round-trip set / get ─────────────────────────────────────────
    // setAttOnTxValue(31) followed by attOnTxValue() returns 31.
    // Mirrors Thetis SetupForm.ATTOnTX setter (mi0bot setup.cs:3988-4017
    // [v2.10.3.13]), which clamps then writes the value to the active band's
    // TX step-attenuator slot.
    void setAttOnTxValue_roundTrip()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);

        ctrl.setAttOnTxValue(31);
        QCOMPARE(ctrl.attOnTxValue(), 31);

        ctrl.setAttOnTxValue(15);
        QCOMPARE(ctrl.attOnTxValue(), 15);

        ctrl.setAttOnTxValue(0);
        QCOMPARE(ctrl.attOnTxValue(), 0);
    }

    // ── Test 2: range clamp ──────────────────────────────────────────────────
    // From Thetis (mi0bot) setup.cs:3988-4017 [v2.10.3.13] ATTOnTX setter:
    //   if (value > 31) value = 31;
    //   if (HPSDRModel.HERMESLITE) { if (value < -28) value = -28; }
    //   else { if (value < 0) value = 0; }
    //
    // NereusSDR controller exposes [m_minAttDb, m_maxAttDb] as the bounds; HL2
    // boards register min=-28 / max=32 via setMin/MaxAttenuation in the
    // connect path (P1 full parity §4.1).  This test uses the HL2 bounds so
    // the negative path can be exercised; legacy boards keep min=0.
    void setAttOnTxValue_clampsToBounds()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);
        ctrl.setMinAttenuation(-28);   // HL2 lower bound
        ctrl.setMaxAttenuation(32);    // HL2 upper bound (not used by clamp;
                                       // value clamps to 31 per Thetis ATTOnTX)

        // Above-max values clamp to 31 (Thetis ATTOnTX always caps at 31,
        // regardless of board — setup.cs:3999 [v2.10.3.13]).
        ctrl.setAttOnTxValue(100);
        QCOMPARE(ctrl.attOnTxValue(), 31);

        // Negative-but-in-HL2-range round-trips unchanged.
        ctrl.setAttOnTxValue(-15);
        QCOMPARE(ctrl.attOnTxValue(), -15);

        // Below HL2 floor clamps to -28.
        ctrl.setAttOnTxValue(-50);
        QCOMPARE(ctrl.attOnTxValue(), -28);
    }

    // ── Test 3: legacy-board lower bound ─────────────────────────────────────
    // Default m_minAttDb=0; non-HL2 boards keep that.  Negative inputs clamp
    // to zero, mirroring Thetis non-HL2 path (setup.cs:4007 [v2.10.3.13]
    // //MW0LGE [2.9.0.7] added after mi0bot source review).
    void setAttOnTxValue_clampsToZero_onLegacyBoard()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);
        // Default min=0, max=31.

        ctrl.setAttOnTxValue(-5);
        QCOMPARE(ctrl.attOnTxValue(), 0);

        ctrl.setAttOnTxValue(99);
        QCOMPARE(ctrl.attOnTxValue(), 31);
    }

    // ── Test 4: per-MAC persistence round-trip ──────────────────────────────
    // saveSettings + loadSettings round-trips the value through the
    // hardware/<mac>/options/stepAtt/txBand/<bandKey> key.  Pattern matches
    // existing per-band TX ATT persistence in StepAttenuatorController.cpp
    // saveSettings() / loadSettings().
    void setAttOnTxValue_persistsPerMac()
    {
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setBand(Band::Band20m);
            ctrl.setAttOnTxValue(22);
            ctrl.saveSettings(kTestMac);
        }

        // Fresh controller: load persisted value back.
        StepAttenuatorController ctrl2;
        ctrl2.setTickTimerEnabled(false);
        ctrl2.loadSettings(kTestMac);
        ctrl2.setBand(Band::Band20m);
        QCOMPARE(ctrl2.attOnTxValue(), 22);
    }

    // ── Test 5: writes per-band TX storage (current band) ────────────────────
    // The setter writes to the per-band slot for the current TX band, so
    // txAttenuationForBand(currentBand) reflects the new value.  Mirrors
    // Thetis TxAttenData setter (console.cs:10612 [v2.10.3.13]):
    //   setTXstepAttenuatorForBand(_tx_band, _tx_attenuator_data);
    //   //[2.10.3.6]MW0LGE att_fixes #399
    void setAttOnTxValue_writesCurrentBandSlot()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);

        ctrl.setBand(Band::Band20m);
        ctrl.setAttOnTxValue(17);
        QCOMPARE(ctrl.txAttenuationForBand(Band::Band20m), 17);

        // Switching band leaves 20m's stored value intact and the new band
        // starts at its prior storage (default 0).
        ctrl.setBand(Band::Band40m);
        QCOMPARE(ctrl.txAttenuationForBand(Band::Band40m), 0);
        QCOMPARE(ctrl.txAttenuationForBand(Band::Band20m), 17);

        // setAttOnTxValue on the new band writes only that band's slot.
        ctrl.setAttOnTxValue(11);
        QCOMPARE(ctrl.txAttenuationForBand(Band::Band40m), 11);
        QCOMPARE(ctrl.txAttenuationForBand(Band::Band20m), 17);
    }
};

QTEST_MAIN(TestStepAttOnTxValue)
#include "tst_step_att_on_tx_value.moc"
