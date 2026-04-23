// =================================================================
// tests/tst_antenna_labels.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-22 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 3P-I-b T2: delegator test for
//                 AntennaLabels + rxOnlyLabels.
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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
//
// Exercises the AntennaLabels facade (antennaLabels() + rxOnlyLabels()).
// Fixture values come indirectly from SkuUiProfile, which itself cites
// Thetis setup.cs:19846-20375. This test is a pure unit test of the
// delegator surface — it doesn't re-embed Thetis data.
//
// Source: Thetis setup.cs:19846-20375 [v2.10.3.13 @501e3f5]
// Phase 3P-I-b T2.

#include <QTest>

#include "core/AntennaLabels.h"
#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"

using namespace NereusSDR;

class TestAntennaLabels : public QObject {
    Q_OBJECT

private slots:
    void antennaLabels_emptyOnNoAlex() {
        BoardCapabilities caps;
        caps.hasAlex = false;
        caps.antennaInputCount = 0;
        QVERIFY(antennaLabels(caps).isEmpty());
    }

    void antennaLabels_threePortsOnAlex() {
        BoardCapabilities caps;
        caps.hasAlex = true;
        caps.antennaInputCount = 3;
        const auto labels = antennaLabels(caps);
        QCOMPARE(labels.size(), 3);
        QCOMPARE(labels.at(0), QStringLiteral("ANT1"));
        QCOMPARE(labels.at(1), QStringLiteral("ANT2"));
        QCOMPARE(labels.at(2), QStringLiteral("ANT3"));
    }

    void rxOnlyLabels_hermes() {
        const auto labels = rxOnlyLabels(skuUiProfileFor(HPSDRModel::HERMES));
        QCOMPARE(labels[0], QStringLiteral("RX1"));
        QCOMPARE(labels[1], QStringLiteral("RX2"));
        QCOMPARE(labels[2], QStringLiteral("XVTR"));
    }

    void rxOnlyLabels_anan_g2() {
        const auto labels = rxOnlyLabels(skuUiProfileFor(HPSDRModel::ANAN_G2));
        QCOMPARE(labels[0], QStringLiteral("BYPS"));
        QCOMPARE(labels[1], QStringLiteral("EXT1"));
        QCOMPARE(labels[2], QStringLiteral("XVTR"));
    }

    void rxOnlyLabels_anan100_EXT() {
        const auto labels = rxOnlyLabels(skuUiProfileFor(HPSDRModel::ANAN100));
        QCOMPARE(labels[0], QStringLiteral("EXT2"));
        QCOMPARE(labels[1], QStringLiteral("EXT1"));
        QCOMPARE(labels[2], QStringLiteral("XVTR"));
    }
};

QTEST_APPLESS_MAIN(TestAntennaLabels)
#include "tst_antenna_labels.moc"
