// =================================================================
// tests/tst_hpsdr_enums.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/enums.cs
//   Project Files/Source/ChannelMaster/network.h
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2000-2025  Original authors
//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]
//   Copyright (C) 2020-2025  Richard Samphire (MW0LGE)
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2020-2025  MI0BOT — Hermes-Lite 2 fork contributions
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
// =================================================================

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
        QCOMPARE(static_cast<int>(HPSDRHW::HermesLite),   6);
        QCOMPARE(static_cast<int>(HPSDRHW::Saturn),      10);
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
