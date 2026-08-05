// =================================================================
// tests/tst_sku_ui_profile.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-22 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 3P-I-b T1: per-SKU label + flag
//                 fixture for SkuUiProfile.
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
// Fixture verifies every HPSDRModel maps to the label + flag set Thetis
// setup.cs:19832-20375 assigns. Regression guard: if a new SKU is added
// without a SkuUiProfile.cpp entry, the compile-time default fall-through
// will still return RX1/RX2/XVTR with no flags, and the unknown-SKU
// fixture here will catch it.
//
// Source: Thetis setup.cs:19832-20375 [v2.10.3.13 @501e3f5]

#include <QTest>
#include "core/SkuUiProfile.h"

using namespace NereusSDR;

class TestSkuUiProfile : public QObject {
    Q_OBJECT

private slots:
    void hermes_RX1_RX2_XVTR() {
        const auto p = skuUiProfileFor(HPSDRModel::HERMES);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("RX1"));
        QCOMPARE(p.rxOnlyLabels[1], QStringLiteral("RX2"));
        QCOMPARE(p.rxOnlyLabels[2], QStringLiteral("XVTR"));
        QVERIFY(p.hasExt1OutOnTx);
        QVERIFY(p.hasExt2OutOnTx);
        QVERIFY(p.hasRxOutOnTx);
        QVERIFY(!p.hasRxBypassUi);
        QVERIFY(p.hasAntennaTab);
        QCOMPARE(p.antennaTabLabel, QStringLiteral("Alex"));
    }

    void anan10_all_hidden() {
        const auto p = skuUiProfileFor(HPSDRModel::ANAN10);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("RX1"));
        QVERIFY(!p.hasExt1OutOnTx);
        QVERIFY(!p.hasExt2OutOnTx);
        QVERIFY(!p.hasRxOutOnTx);
        QVERIFY(!p.hasRxBypassUi);
        QCOMPARE(p.antennaTabLabel, QStringLiteral("Ant/Filters"));
    }

    void anan100_EXT2_EXT1_XVTR() {
        const auto p = skuUiProfileFor(HPSDRModel::ANAN100);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("EXT2"));
        QCOMPARE(p.rxOnlyLabels[1], QStringLiteral("EXT1"));
        QCOMPARE(p.rxOnlyLabels[2], QStringLiteral("XVTR"));
        QVERIFY(p.hasExt1OutOnTx);
        QVERIFY(p.hasExt2OutOnTx);
        QVERIFY(p.hasRxOutOnTx);
        QVERIFY(p.hasRxBypassUi);
    }

    void anan7000d_BYPS_EXT1_XVTR() {
        const auto p = skuUiProfileFor(HPSDRModel::ANAN7000D);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("BYPS"));
        QCOMPARE(p.rxOnlyLabels[1], QStringLiteral("EXT1"));
        QCOMPARE(p.rxOnlyLabels[2], QStringLiteral("XVTR"));
        QVERIFY(p.hasExt1OutOnTx);
        QVERIFY(p.hasExt2OutOnTx);
        QVERIFY(!p.hasRxOutOnTx);
        QVERIFY(!p.hasRxBypassUi);
    }

    void anan8000d_BYPS_all_hidden() {
        const auto p = skuUiProfileFor(HPSDRModel::ANAN8000D);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("BYPS"));
        QVERIFY(!p.hasExt1OutOnTx);
        QVERIFY(!p.hasExt2OutOnTx);
        QVERIFY(!p.hasRxOutOnTx);
    }

    void anan_g2_EXT_checkboxes_visible() {
        const auto p = skuUiProfileFor(HPSDRModel::ANAN_G2);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("BYPS"));
        QVERIFY(p.hasExt1OutOnTx);
        QVERIFY(p.hasExt2OutOnTx);
        QVERIFY(!p.hasRxOutOnTx);
        QVERIFY(!p.hasRxBypassUi);  // setup.cs:6174 — chkDisableRXOut hidden on G2
    }

    void anan_g2_1k_all_hidden() {
        const auto p = skuUiProfileFor(HPSDRModel::ANAN_G2_1K);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("BYPS"));
        QVERIFY(!p.hasExt1OutOnTx);
        QVERIFY(!p.hasExt2OutOnTx);
    }

    void hermeslite_tab_hidden() {
        const auto p = skuUiProfileFor(HPSDRModel::HERMESLITE);
        QVERIFY(!p.hasAntennaTab);
        QVERIFY(!p.hasExt1OutOnTx);
        QVERIFY(!p.hasExt2OutOnTx);
        QVERIFY(!p.hasRxOutOnTx);
        QVERIFY(!p.hasRxBypassUi);
    }

    // Three independently-coded NereusSDR-native cases (no Thetis switch
    // entry in the 19832-20405 block). Each deserves its own regression
    // pin — they're the cases most likely to drift if someone edits the
    // fallback policy.

    void hpsdr_native_fallback() {
        // HPSDR (pre-ANAN Atlas) has no Thetis antenna-overlay case;
        // NereusSDR-native default per SkuUiProfile.cpp:74-83.
        const auto p = skuUiProfileFor(HPSDRModel::HPSDR);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("RX1"));  // struct default
        QVERIFY(p.hasExt1OutOnTx);
        QVERIFY(p.hasExt2OutOnTx);
        QVERIFY(p.hasRxOutOnTx);
        QVERIFY(p.hasRxBypassUi);  // differs from HERMES (true vs false)
        QCOMPARE(p.antennaTabLabel, QStringLiteral("Alex"));
    }

    void orionmkii_native_grouping() {
        // ORIONMKII has no explicit Thetis case; NereusSDR groups with
        // ANAN100 family per SkuUiProfile.cpp:117-131.
        const auto p = skuUiProfileFor(HPSDRModel::ORIONMKII);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("EXT2"));
        QCOMPARE(p.rxOnlyLabels[1], QStringLiteral("EXT1"));
        QCOMPARE(p.rxOnlyLabels[2], QStringLiteral("XVTR"));
        QVERIFY(p.hasExt1OutOnTx);
        QVERIFY(p.hasExt2OutOnTx);
        QVERIFY(p.hasRxOutOnTx);
        QVERIFY(p.hasRxBypassUi);
        QCOMPARE(p.antennaTabLabel, QStringLiteral("Ant/Filters"));
    }

    void redpitaya_BYPS_EXT1_XVTR() {
        // Thetis setup.cs:20355-20405 [v2.10.3.13 @501e3f5] //DH1KLM
        const auto p = skuUiProfileFor(HPSDRModel::REDPITAYA);
        QCOMPARE(p.rxOnlyLabels[0], QStringLiteral("BYPS"));
        QCOMPARE(p.rxOnlyLabels[1], QStringLiteral("EXT1"));
        QCOMPARE(p.rxOnlyLabels[2], QStringLiteral("XVTR"));
        QVERIFY(p.hasExt1OutOnTx);
        QVERIFY(p.hasExt2OutOnTx);
        QVERIFY(!p.hasRxOutOnTx);
        QVERIFY(!p.hasRxBypassUi);
    }

    // Task C1 — ANAN_G2E EXT label overrides.
    // Source: Thetis setup.cs:19904-19929 [v2.10.3.15] //N1GP G2E added

    void g2e_ext2Label_isRxBypass() {
        // setup.cs:19904-19929 [v2.10.3.15] //N1GP G2E added
        const auto profile = skuUiProfileFor(HPSDRModel::ANAN_G2E);
        QCOMPARE(profile.ext1OutOnTxLabel, QStringLiteral("Ext 1 on Tx"));
        QCOMPARE(profile.ext2OutOnTxLabel, QStringLiteral("Rx BYPASS on Tx"));
        QCOMPARE(profile.antennaTabLabel, QStringLiteral("Ant/Filters"));
        QCOMPARE(profile.rxOnlyLabels[0], QStringLiteral("BYPS"));
        QCOMPARE(profile.rxOnlyLabels[1], QStringLiteral("EXT1"));
        QCOMPARE(profile.rxOnlyLabels[2], QStringLiteral("XVTR"));
        QVERIFY(profile.hasExt1OutOnTx);
        QVERIFY(profile.hasExt2OutOnTx);
        QVERIFY(!profile.hasRxOutOnTx);
        QVERIFY(!profile.hasRxBypassUi);
    }

    void defaultExt2Label_isExt2OnTx() {
        // Every non-G2E SKU keeps the struct default "Ext 2 on Tx".
        const auto profile = skuUiProfileFor(HPSDRModel::ANAN_G2);
        QCOMPARE(profile.ext1OutOnTxLabel, QStringLiteral("Ext 1 on Tx"));
        QCOMPARE(profile.ext2OutOnTxLabel, QStringLiteral("Ext 2 on Tx"));
    }
};

QTEST_APPLESS_MAIN(TestSkuUiProfile)
#include "tst_sku_ui_profile.moc"
