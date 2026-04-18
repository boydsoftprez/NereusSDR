// =================================================================
// tests/tst_hpsdr_enums.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//
// =================================================================
// Additional copyright holders whose code is preserved in this file via
// inline markers (upstream file-header blocks do not name them):
//   Reid Campbell (MI0BOT) — HermesLite 2 enum value parity test
//     (preserved via inline marker on HPSDRHW::HermesLite QCOMPARE assertion)
//   Laurence Barker (G8NJJ) — ANAN-G2 / Saturn enum value parity test
//     (preserved via inline marker on HPSDRHW::Saturn QCOMPARE assertion)
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

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

/*  network.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015-2020 Doug Wigley, W5WC

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

*/

#include <QtTest/QtTest>
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestHpsdrEnums : public QObject {
    Q_OBJECT
private slots:
    void integerValuesMatchThetis() {
        // Source: mi0bot/Thetis@Hermes-Lite enums.cs:109
        QCOMPARE(static_cast<int>(HPSDRModel::FIRST),        -1);
        QCOMPARE(static_cast<int>(HPSDRModel::HPSDR),         0);
        QCOMPARE(static_cast<int>(HPSDRModel::HERMES),        1);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN10),        2);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN10E),       3);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN100),       4);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN100B),      5);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN100D),      6);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN200D),      7);
        QCOMPARE(static_cast<int>(HPSDRModel::ORIONMKII),     8);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN7000D),     9);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN8000D),    10);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN_G2),      11);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN_G2_1K),   12);
        QCOMPARE(static_cast<int>(HPSDRModel::ANVELINAPRO3), 13);
        QCOMPARE(static_cast<int>(HPSDRModel::HERMESLITE),   14);
        QCOMPARE(static_cast<int>(HPSDRModel::REDPITAYA),    15);
        QCOMPARE(static_cast<int>(HPSDRModel::LAST),         16);

        // Source: enums.cs:388 + network.h:446 — reserved gap at 7..9
        QCOMPARE(static_cast<int>(HPSDRHW::Atlas),        0);
        QCOMPARE(static_cast<int>(HPSDRHW::Hermes),       1);
        QCOMPARE(static_cast<int>(HPSDRHW::HermesII),     2);
        QCOMPARE(static_cast<int>(HPSDRHW::Angelia),      3);
        QCOMPARE(static_cast<int>(HPSDRHW::Orion),        4);
        QCOMPARE(static_cast<int>(HPSDRHW::OrionMKII),    5);
        QCOMPARE(static_cast<int>(HPSDRHW::HermesLite),   6);  // MI0BOT: HL2 allocated number [Thetis network.h:422 / enums.cs:396]
        QCOMPARE(static_cast<int>(HPSDRHW::Saturn),      10);  // ANAN-G2: added G8NJJ [Thetis network.h:423 / enums.cs:397]
        QCOMPARE(static_cast<int>(HPSDRHW::SaturnMKII),  11);
        QCOMPARE(static_cast<int>(HPSDRHW::Unknown),    999);
    }

    void boardForModelCoversEveryModel() {
        for (int i = 0; i < static_cast<int>(HPSDRModel::LAST); ++i) {
            auto m = static_cast<HPSDRModel>(i);
            auto hw = boardForModel(m);
            QVERIFY2(hw != HPSDRHW::Unknown,
                     qPrintable(QString("HPSDRModel %1 maps to Unknown").arg(i)));
        }
    }

    void displayNameNonNullForEveryModel() {
        for (int i = 0; i < static_cast<int>(HPSDRModel::LAST); ++i) {
            auto m = static_cast<HPSDRModel>(i);
            const char* name = displayName(m);
            QVERIFY(name != nullptr);
            QVERIFY(std::string(name).size() > 0);
        }
    }
};

QTEST_APPLESS_MAIN(TestHpsdrEnums)
#include "tst_hpsdr_enums.moc"
