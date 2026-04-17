// =================================================================
// tests/tst_slice_emnr.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/radio.cs
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

// tst_slice_emnr.cpp
//
// Verifies that SliceModel EMNR (NR2) setter stores + emits signal correctly.
// SliceModel is the single source of truth for VFO/DSP state. Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:2216-2232 — EMNR run flag
//   WDSP: third_party/wdsp/src/emnr.c:1283

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceEmnr : public QObject {
    Q_OBJECT

private slots:
    // ── emnrEnabled ──────────────────────────────────────────────────────────

    void emnrEnabledDefaultIsFalse() {
        // From Thetis radio.cs:2216 — rx_nr2_run default = 0
        SliceModel s;
        QCOMPARE(s.emnrEnabled(), false);
    }

    void setEmnrEnabledStoresValue() {
        SliceModel s;
        s.setEmnrEnabled(true);
        QCOMPARE(s.emnrEnabled(), true);
    }

    void setEmnrEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::emnrEnabledChanged);
        s.setEmnrEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setEmnrEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setEmnrEnabled(true);
        QSignalSpy spy(&s, &SliceModel::emnrEnabledChanged);
        s.setEmnrEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setEmnrEnabledToggleRoundTrip() {
        SliceModel s;
        s.setEmnrEnabled(true);
        s.setEmnrEnabled(false);
        QCOMPARE(s.emnrEnabled(), false);
    }
};

QTEST_MAIN(TestSliceEmnr)
#include "tst_slice_emnr.moc"
