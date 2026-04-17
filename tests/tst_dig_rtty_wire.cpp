// =================================================================
// tests/tst_dig_rtty_wire.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/enums.cs
//   Project Files/Source/Console/radio.cs
//   Project Files/Source/Console/setup.designer.cs
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   Copyright (C) 2020-2025  MI0BOT — Hermes-Lite 2 fork contributions
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2000-2025  Original authors — enums.cs placeholder
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

// tst_dig_rtty_wire.cpp
//
// S2.10b: Verify DIG offset and RTTY mark/shift client-side logic.
//
// DIG offset:
//   From Thetis console.cs:14637 (DIGUClickTuneOffset) and :14672
//   (DIGLClickTuneOffset). NereusSDR treats these as additive shifts on the
//   same setShiftFrequency path as RIT. Total shift = RIT + DIG (when active).
//   This is a client-side state test — RadioModel wiring to RxChannel is
//   integration behavior (requires WdspEngine, not unit-testable here).
//
// RTTY mark/shift → filter:
//   filterLow  = markHz − shiftHz/2 − 100
//   filterHigh = markHz + shiftHz/2 + 100
//   SliceModel.setFilter() emits filterChanged; RadioModel connects that
//   to RxChannel::setFilterFreqs(). This test verifies the computation
//   math only — RadioModel integration requires WdspEngine.
//
// Note: Thetis enums.cs:252-268 has no DSPMode::RTTY in the WDSP enum —
// RTTY is operated in DIGU/DIGL mode with the dolly/ampeak peak filter.
// NereusSDR models the bandpass using the mark/shift formula for now.

#include <QtTest/QtTest>
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestDigRttyWire : public QObject {
    Q_OBJECT

private slots:
    // ── DIG offset: SliceModel stores per-mode offsets ────────────────────────

    void diglOffsetStoredSeparately() {
        SliceModel s;
        s.setDspMode(DSPMode::DIGL);
        s.setDiglOffsetHz(1500);
        s.setDiguOffsetHz(0);
        QCOMPARE(s.diglOffsetHz(), 1500);
        QCOMPARE(s.diguOffsetHz(), 0);
    }

    void diguOffsetStoredSeparately() {
        SliceModel s;
        s.setDspMode(DSPMode::DIGU);
        s.setDiguOffsetHz(2000);
        s.setDiglOffsetHz(0);
        QCOMPARE(s.diguOffsetHz(), 2000);
        QCOMPARE(s.diglOffsetHz(), 0);
    }

    void diglAndDiguOffsetAreIndependent() {
        SliceModel s;
        s.setDiglOffsetHz(1500);
        s.setDiguOffsetHz(2000);
        // Changing DIGL does not affect DIGU and vice versa
        s.setDiglOffsetHz(100);
        QCOMPARE(s.diguOffsetHz(), 2000);
        s.setDiguOffsetHz(300);
        QCOMPARE(s.diglOffsetHz(), 100);
    }

    void diglOffsetDefaultIsZero() {
        // From Thetis console.cs:14672 — DIGLClickTuneOffset default 0 Hz
        // (NereusSDR defaults diglOffsetHz to 0 per SliceModel; Thetis's
        // digl_click_tune_offset default is 2210 but that governs filter
        // preset centering, not the demodulation shift we model here.)
        SliceModel s;
        QCOMPARE(s.diglOffsetHz(), 0);
    }

    void diguOffsetDefaultIsZero() {
        // From Thetis console.cs:14637 — DIGUClickTuneOffset default 0 Hz
        SliceModel s;
        QCOMPARE(s.diguOffsetHz(), 0);
    }

    void diglOffsetEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::diglOffsetHzChanged);
        s.setDiglOffsetHz(500);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 500);
    }

    void diguOffsetEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::diguOffsetHzChanged);
        s.setDiguOffsetHz(750);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 750);
    }

    void diglOffsetNoSignalOnSameValue() {
        SliceModel s;
        s.setDiglOffsetHz(400);
        QSignalSpy spy(&s, &SliceModel::diglOffsetHzChanged);
        s.setDiglOffsetHz(400);
        QCOMPARE(spy.count(), 0);
    }

    // ── RTTY mark/shift: default values ──────────────────────────────────────

    void rttyMarkDefaultIs2295() {
        // From Thetis setup.designer.cs:40635-40637 — RTTY MARK default 2295 Hz
        SliceModel s;
        QCOMPARE(s.rttyMarkHz(), 2295);
    }

    void rttyShiftDefaultIs170() {
        // From Thetis radio.cs:2043-2044 — rx_dolly_freq1=2295, rx_dolly_freq0=2125
        // shift = 2295 − 2125 = 170 Hz
        SliceModel s;
        QCOMPARE(s.rttyShiftHz(), 170);
    }

    // ── RTTY filter computation math ──────────────────────────────────────────
    //
    // filterLow  = markHz − shiftHz/2 − 100
    // filterHigh = markHz + shiftHz/2 + 100

    void rttyFilterComputationDefaultValues() {
        // mark=2295, shift=170
        // filterLow  = 2295 − 170/2 − 100 = 2295 − 85 − 100 = 2110
        // filterHigh = 2295 + 170/2 + 100 = 2295 + 85 + 100 = 2480
        const int mark  = 2295;
        const int shift = 170;
        const int expectedLow  = mark - shift / 2 - 100;  // 2110
        const int expectedHigh = mark + shift / 2 + 100;  // 2480
        QCOMPARE(expectedLow,  2110);
        QCOMPARE(expectedHigh, 2480);
    }

    void rttyFilterComputationCustomValues() {
        // mark=2125 (Baudot space), shift=170
        // filterLow  = 2125 − 85 − 100 = 1940
        // filterHigh = 2125 + 85 + 100 = 2310
        const int mark  = 2125;
        const int shift = 170;
        const int expectedLow  = mark - shift / 2 - 100;
        const int expectedHigh = mark + shift / 2 + 100;
        QCOMPARE(expectedLow,  1940);
        QCOMPARE(expectedHigh, 2310);
    }

    void rttyFilterComputationWideShift() {
        // mark=2295, shift=850 (wide FSK)
        // filterLow  = 2295 − 425 − 100 = 1770
        // filterHigh = 2295 + 425 + 100 = 2820
        const int mark  = 2295;
        const int shift = 850;
        const int expectedLow  = mark - shift / 2 - 100;
        const int expectedHigh = mark + shift / 2 + 100;
        QCOMPARE(expectedLow,  1770);
        QCOMPARE(expectedHigh, 2820);
    }

    // ── RTTY mark/shift SliceModel signals ────────────────────────────────────

    void rttyMarkEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::rttyMarkHzChanged);
        s.setRttyMarkHz(2000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2000);
    }

    void rttyShiftEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::rttyShiftHzChanged);
        s.setRttyShiftHz(200);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 200);
    }

    void rttyMarkNoSignalOnSameValue() {
        SliceModel s;
        s.setRttyMarkHz(2295);
        QSignalSpy spy(&s, &SliceModel::rttyMarkHzChanged);
        s.setRttyMarkHz(2295);
        QCOMPARE(spy.count(), 0);
    }

    void rttyShiftNoSignalOnSameValue() {
        SliceModel s;
        s.setRttyShiftHz(170);
        QSignalSpy spy(&s, &SliceModel::rttyShiftHzChanged);
        s.setRttyShiftHz(170);
        QCOMPARE(spy.count(), 0);
    }

    // ── RTTY mode: setFilter is called via RadioModel in integration ──────────
    //
    // This verifies that SliceModel::setFilter works correctly for the
    // computed values; RadioModel connects rttyMarkHzChanged/rttyShiftHzChanged
    // to call slice->setFilter(low, high) when mode == RTTY.

    void setFilterUpdatesFilterLowHigh() {
        // Note: Thetis has no DSPMode::RTTY — RTTY uses DSPMode::DIGU or DIGL
        // per Thetis enums.cs:252-268. The RTTY bandpass filter applies in DIG
        // modes via mark/shift → setFilter() computation in RadioModel.
        SliceModel s;
        s.setDspMode(DSPMode::DIGU);  // RTTY is operated in DIG mode

        QSignalSpy spy(&s, &SliceModel::filterChanged);
        s.setFilter(2110, 2480);  // computed from mark=2295, shift=170

        QCOMPARE(spy.count(), 1);
        QCOMPARE(s.filterLow(),  2110);
        QCOMPARE(s.filterHigh(), 2480);
    }
};

QTEST_MAIN(TestDigRttyWire)
#include "tst_dig_rtty_wire.moc"
