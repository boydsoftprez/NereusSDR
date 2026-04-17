// =================================================================
// tests/tst_reading_name.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
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
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// tst_reading_name.cpp — readingName(bindingId) maps MeterBinding constants
// to the Thetis-spelled labels rendered by ScaleItem::ShowType.
//
// Source of truth: Thetis MeterManager.cs:2258-2318 (ReadingName() switch).

#include <QtTest/QtTest>

#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding namespace

using namespace NereusSDR;

class TstReadingName : public QObject
{
    Q_OBJECT

private slots:
    void rx_bindings_match_thetis()
    {
        QCOMPARE(readingName(MeterBinding::SignalPeak),   QStringLiteral("Signal Peak"));
        QCOMPARE(readingName(MeterBinding::SignalAvg),    QStringLiteral("Signal Average"));
        QCOMPARE(readingName(MeterBinding::AdcPeak),      QStringLiteral("ADC Peak"));
        QCOMPARE(readingName(MeterBinding::AdcAvg),       QStringLiteral("ADC Average"));
        QCOMPARE(readingName(MeterBinding::AgcGain),      QStringLiteral("AGC Gain"));
        QCOMPARE(readingName(MeterBinding::AgcPeak),      QStringLiteral("AGC Peak"));
        QCOMPARE(readingName(MeterBinding::AgcAvg),       QStringLiteral("AGC Average"));
        QCOMPARE(readingName(MeterBinding::SignalMaxBin), QStringLiteral("Signal Max FFT Bin"));
        QCOMPARE(readingName(MeterBinding::PbSnr),        QStringLiteral("Estimated PBSNR"));
    }

    void tx_bindings_match_thetis()
    {
        QCOMPARE(readingName(MeterBinding::TxPower),        QStringLiteral("Power"));
        QCOMPARE(readingName(MeterBinding::TxReversePower), QStringLiteral("Reverse Power"));
        QCOMPARE(readingName(MeterBinding::TxSwr),          QStringLiteral("SWR"));
        QCOMPARE(readingName(MeterBinding::TxMic),          QStringLiteral("MIC"));
        QCOMPARE(readingName(MeterBinding::TxComp),         QStringLiteral("Compression"));
        QCOMPARE(readingName(MeterBinding::TxAlc),          QStringLiteral("ALC"));
        QCOMPARE(readingName(MeterBinding::TxEq),           QStringLiteral("EQ"));
        QCOMPARE(readingName(MeterBinding::TxLeveler),      QStringLiteral("Leveler"));
        QCOMPARE(readingName(MeterBinding::TxLevelerGain),  QStringLiteral("Leveler Gain"));
        QCOMPARE(readingName(MeterBinding::TxAlcGain),      QStringLiteral("ALC Gain"));
        QCOMPARE(readingName(MeterBinding::TxAlcGroup),     QStringLiteral("ALC Group"));
        QCOMPARE(readingName(MeterBinding::TxCfc),          QStringLiteral("CFC Compression Average"));
        QCOMPARE(readingName(MeterBinding::TxCfcGain),      QStringLiteral("CFC Compression"));
    }

    void hardware_and_rotator_bindings()
    {
        QCOMPARE(readingName(MeterBinding::HwVolts),  QStringLiteral("Volts"));
        QCOMPARE(readingName(MeterBinding::HwAmps),   QStringLiteral("Amps"));
        QCOMPARE(readingName(MeterBinding::RotatorAz), QStringLiteral("Azimuth"));
        QCOMPARE(readingName(MeterBinding::RotatorEle),QStringLiteral("Elevation"));
    }

    void unknown_binding_returns_empty()
    {
        // Thetis falls through to reading.ToString() for unmapped values.
        // NereusSDR returns an empty string instead — the render pass
        // skips empty titles.
        QCOMPARE(readingName(-1),    QString());
        QCOMPARE(readingName(99999), QString());
    }
};

QTEST_MAIN(TstReadingName)
#include "tst_reading_name.moc"
