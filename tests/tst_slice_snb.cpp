// =================================================================
// tests/tst_slice_snb.cpp  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/dsp.cs
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2013-2017  Warren Pratt (NR0V) [dsp.cs]
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   Copyright (C) 2017-2019  Chris Codella (W2PA) — console.cs / setup.cs / NetworkIO.cs inline mods
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2020-2025  Joe (WD5Y) — console.cs UI tweaks / mute / ForeColor
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

// tst_slice_snb.cpp
//
// Verifies that SliceModel SNB setter stores + emits signal correctly.
// SliceModel is the single source of truth for VFO/DSP state. Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/console.cs:36347 — SNB run flag
//   From Thetis Project Files/Source/Console/dsp.cs:692-693 — P/Invoke decl
//   WDSP: third_party/wdsp/src/snb.c:579

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceSnb : public QObject {
    Q_OBJECT

private slots:
    // ── snbEnabled ───────────────────────────────────────────────────────────

    void snbEnabledDefaultIsFalse() {
        // SNB off at startup — Thetis chkDSPNB2.Checked default = false
        SliceModel s;
        QCOMPARE(s.snbEnabled(), false);
    }

    void setSnbEnabledStoresValue() {
        SliceModel s;
        s.setSnbEnabled(true);
        QCOMPARE(s.snbEnabled(), true);
    }

    void setSnbEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::snbEnabledChanged);
        s.setSnbEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setSnbEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setSnbEnabled(true);
        QSignalSpy spy(&s, &SliceModel::snbEnabledChanged);
        s.setSnbEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setSnbEnabledToggleRoundTrip() {
        SliceModel s;
        s.setSnbEnabled(true);
        s.setSnbEnabled(false);
        QCOMPARE(s.snbEnabled(), false);
    }
};

QTEST_MAIN(TestSliceSnb)
#include "tst_slice_snb.moc"
