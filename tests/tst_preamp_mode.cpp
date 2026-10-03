// =================================================================
// tests/tst_preamp_mode.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, Project Files/Source/Console/enums.cs
//   [v2.10.3.15], original licence from Thetis source is included below
//
// The ten Thetis preamp modes, the label to mode map of
// comboPreamp_SelectedIndexChanged, the per-board combo items that follow
// it, and the one-time move of stored modes saved before the SA modes
// existed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: written for the Level Cal preamp port by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
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

// --- From enums.cs ---
/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk
*/
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

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/StepAttenuatorController.h"
#include "models/Band.h"

using namespace NereusSDR;

namespace {

const HPSDRHW kAllBoards[] = {
    HPSDRHW::Atlas, HPSDRHW::Hermes, HPSDRHW::HermesII, HPSDRHW::Angelia,
    HPSDRHW::Orion, HPSDRHW::OrionMKII, HPSDRHW::HermesLite, HPSDRHW::Saturn,
    HPSDRHW::SaturnMKII, HPSDRHW::HermesLiteRxOnly, HPSDRHW::HermesC10,
    HPSDRHW::Andromeda,
};

QString bandPreampKey(Band b)
{
    return QStringLiteral("options/preamp/rx1Band/") + bandKeyName(b);
}

void writeStored(const QString& mac, Band b, int mode)
{
    AppSettings::instance().setHardwareValue(mac, bandPreampKey(b), mode);
}

int readStored(const QString& mac, Band b)
{
    return AppSettings::instance().hardwareValue(mac, bandPreampKey(b)).toInt();
}

// A controller for `hw` that has loaded `mac` with the current band `b`.
PreampMode loadedMode(const QString& mac, HPSDRHW hw, HPSDRModel model,
                      bool alex, Band b)
{
    StepAttenuatorController ctrl;
    ctrl.setTickTimerEnabled(false);
    ctrl.setBoardIdentity(hw, model, alex);
    ctrl.setBand(b);
    ctrl.loadSettings(mac);
    return ctrl.preampMode();
}

} // namespace

class TestPreampMode : public QObject {
    Q_OBJECT
private slots:

    // From Thetis enums.cs:236-251 [v2.10.3.15]: HPSDR_OFF..HPSDR_MINUS50
    // then SA_MINUS10, SA_MINUS20 //MW0LGE_21d, SA_MINUS30.
    void enum_carriesTheTenThetisModes()
    {
        QCOMPARE(static_cast<int>(PreampMode::Off), 0);
        QCOMPARE(static_cast<int>(PreampMode::On), 1);
        QCOMPARE(static_cast<int>(PreampMode::Minus50), 6);
        QCOMPARE(static_cast<int>(PreampMode::SaMinus10), 7);
        QCOMPARE(static_cast<int>(PreampMode::SaMinus20), 8);
        QCOMPARE(static_cast<int>(PreampMode::SaMinus30), 9);
    }

    // From Thetis console.cs:2006-2008 [v2.10.3.15].
    void offsets_coverTheStepAttenuatorModes()
    {
        QCOMPARE(rxPreampOffsetDbFor(7), 10.0f);
        QCOMPARE(rxPreampOffsetDbFor(8), 20.0f);
        QCOMPARE(rxPreampOffsetDbFor(9), 30.0f);
    }

    // From Thetis console.cs:28405-28450 [v2.10.3.15].
    void labelMap_followsComboPreampSelectedIndexChanged()
    {
        using BoardCapsTable::preampModeForLabel;
        QCOMPARE(preampModeForLabel("-20dB", /*hpsdrModel=*/true), 0);
        QCOMPARE(preampModeForLabel("-20dB", false), 8);
        QCOMPARE(preampModeForLabel("0dB", true), 1);
        QCOMPARE(preampModeForLabel("0dB", false), 1);
        QCOMPARE(preampModeForLabel("-10dB", false), 7);
        QCOMPARE(preampModeForLabel("-30dB", false), 9);
        QCOMPARE(preampModeForLabel("-10db", false), 2);
        QCOMPARE(preampModeForLabel("-20db", false), 3);
        QCOMPARE(preampModeForLabel("-30db", false), 4);
        QCOMPARE(preampModeForLabel("-40db", false), 5);
        QCOMPARE(preampModeForLabel("-50db", false), 6);
        QCOMPARE(preampModeForLabel("-60dB", false), -1);
    }

    // Every combo item carries the mode Thetis picks for its label on that
    // board (Model == HPSDR is the Atlas board).
    void items_matchTheLabelMapOnEveryBoard()
    {
        for (const HPSDRHW hw : kAllBoards) {
            for (const bool alex : {false, true}) {
                const auto list = BoardCapsTable::preampItemsForBoard(hw, alex);
                QVERIFY(!list.empty());
                for (const auto& item : list) {
                    QCOMPARE(item.modeInt,
                             BoardCapsTable::preampModeForLabel(
                                 item.label, hw == HPSDRHW::Atlas));
                }
            }
            for (const auto& item : BoardCapsTable::rx2PreampItemsForBoard(hw)) {
                QCOMPARE(item.modeInt,
                         BoardCapsTable::preampModeForLabel(
                             item.label, hw == HPSDRHW::Atlas));
            }
        }
    }

    // Level Cal fix wave: RX2's list per board (console.cs:40883-40889
    // [v2.10.3.15]): the four-step list on the dual-ADC boards, the G2's
    // later board revision included; on/off everywhere else, the G2E too.
    void rx2Items_followThetissModelList()
    {
        for (const HPSDRHW hw : {HPSDRHW::Angelia, HPSDRHW::Orion, HPSDRHW::OrionMKII,
                                 HPSDRHW::Saturn, HPSDRHW::SaturnMKII}) {
            QCOMPARE(BoardCapsTable::rx2PreampItemsForBoard(hw).size(), size_t(4));
        }
        for (const HPSDRHW hw : {HPSDRHW::Atlas, HPSDRHW::Hermes, HPSDRHW::HermesII,
                                 HPSDRHW::HermesLite, HPSDRHW::HermesC10}) {
            QCOMPARE(BoardCapsTable::rx2PreampItemsForBoard(hw).size(), size_t(2));
        }
    }

    // A mode stored before the SA modes keeps the label it showed.
    void migration_keepsTheLabelShown()
    {
        using BoardCapsTable::preampModeFromV1;
        // The 4-step list (HL2, G2, OrionMKII, Hermes without Alex):
        // -10dB / -20dB / -30dB were stored as 2 / 3 / 4.
        QCOMPARE(preampModeFromV1(HPSDRHW::HermesLite, false, 1), 1);
        QCOMPARE(preampModeFromV1(HPSDRHW::HermesLite, false, 2), 7);
        QCOMPARE(preampModeFromV1(HPSDRHW::HermesLite, false, 3), 8);
        QCOMPARE(preampModeFromV1(HPSDRHW::HermesLite, false, 4), 9);
        QCOMPARE(preampModeFromV1(HPSDRHW::Saturn, false, 3), 8);
        QCOMPARE(preampModeFromV1(HPSDRHW::OrionMKII, true, 4), 9);
        QCOMPARE(preampModeFromV1(HPSDRHW::HermesC10, false, 2), 7);
        // Off (the preamp switched out, 20 dB) is SA_MINUS20 off Atlas.
        QCOMPARE(preampModeFromV1(HPSDRHW::Saturn, false, 0), 8);
        QCOMPARE(preampModeFromV1(HPSDRHW::Hermes, true, 0), 8);
        // Alex items keep their lowercase modes.
        QCOMPARE(preampModeFromV1(HPSDRHW::Hermes, true, 2), 2);
        QCOMPARE(preampModeFromV1(HPSDRHW::Orion, true, 6), 6);
        // Atlas keeps every value: -20dB is HPSDR_OFF there.
        for (int v = 0; v <= 6; ++v) {
            QCOMPARE(preampModeFromV1(HPSDRHW::Atlas, true, v), v);
        }
    }

    void load_migratesStoredModesOnce()
    {
        const QString mac = QStringLiteral("aa:bb:cc:9e:a1:01");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(mac);
        writeStored(mac, Band::Band40m, 2);
        writeStored(mac, Band::Band20m, 3);
        writeStored(mac, Band::Band10m, 0);

        QCOMPARE(loadedMode(mac, HPSDRHW::HermesLite, HPSDRModel::HERMESLITE,
                            false, Band::Band40m),
                 PreampMode::SaMinus10);
        QCOMPARE(readStored(mac, Band::Band40m), 7);
        QCOMPARE(readStored(mac, Band::Band20m), 8);
        QCOMPARE(readStored(mac, Band::Band10m), 8);

        // A later value in the new numbering is not moved again.
        // The HL2 has no Alex, so Minus20 reads as Off (console.cs:19220-19227
        // [v2.10.3.15]); the stored value stays as written.
        writeStored(mac, Band::Band20m, 3);
        QCOMPARE(loadedMode(mac, HPSDRHW::HermesLite, HPSDRModel::HERMESLITE,
                            false, Band::Band20m),
                 PreampMode::Off);
        QCOMPARE(readStored(mac, Band::Band20m), 3);
        s.clearHardwareValues(mac);
    }

    void load_keepsAtlasValues()
    {
        const QString mac = QStringLiteral("aa:bb:cc:9e:a1:02");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(mac);
        writeStored(mac, Band::Band40m, 0);
        writeStored(mac, Band::Band20m, 3);
        QCOMPARE(loadedMode(mac, HPSDRHW::Atlas, HPSDRModel::HPSDR, true,
                            Band::Band40m),
                 PreampMode::Off);
        QCOMPARE(readStored(mac, Band::Band20m), 3);
        s.clearHardwareValues(mac);
    }

    // With no board known the stored values are read as they are and the
    // move waits for a load that knows the board.
    void load_withoutBoardLeavesValuesForLater()
    {
        const QString mac = QStringLiteral("aa:bb:cc:9e:a1:03");
        auto& s = AppSettings::instance();
        s.clearHardwareValues(mac);
        writeStored(mac, Band::Band40m, 2);
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.setBand(Band::Band40m);
            ctrl.loadSettings(mac);
            QCOMPARE(ctrl.preampMode(), PreampMode::Minus10);
        }
        QCOMPARE(readStored(mac, Band::Band40m), 2);
        QCOMPARE(loadedMode(mac, HPSDRHW::Saturn, HPSDRModel::ANAN_G2, false,
                            Band::Band40m),
                 PreampMode::SaMinus10);
        s.clearHardwareValues(mac);
    }
};

QTEST_MAIN(TestPreampMode)
#include "tst_preamp_mode.moc"
