// =================================================================
// tests/tst_slice_squelch.cpp  (NereusSDR)
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

// tst_slice_squelch.cpp
//
// Verifies that SliceModel squelch (SSB/AM/FM) setters store + emit signals
// correctly. SliceModel is the single source of truth for VFO/DSP state.
// Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1185-1229 — SSQL
//   From Thetis Project Files/Source/Console/radio.cs:1164-1178 — AMSQ threshold
//   From Thetis Project Files/Source/Console/radio.cs:1293-1329 — AMSQ/FMSQ run
//   From Thetis Project Files/Source/Console/radio.cs:1274-1291 — FMSQ threshold
//   WDSP: third_party/wdsp/src/ssql.c:331,339, amsq.c, fmsq.c:236,244

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceSquelch : public QObject {
    Q_OBJECT

private slots:
    // ── ssqlEnabled ──────────────────────────────────────────────────────────

    void ssqlEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1185 — _bSSqlOn default = false
        SliceModel s;
        QCOMPARE(s.ssqlEnabled(), false);
    }

    void setSsqlEnabledStoresValue() {
        SliceModel s;
        s.setSsqlEnabled(true);
        QCOMPARE(s.ssqlEnabled(), true);
    }

    void setSsqlEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::ssqlEnabledChanged);
        s.setSsqlEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setSsqlEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setSsqlEnabled(true);
        QSignalSpy spy(&s, &SliceModel::ssqlEnabledChanged);
        s.setSsqlEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setSsqlEnabledToggleRoundTrip() {
        SliceModel s;
        s.setSsqlEnabled(true);
        s.setSsqlEnabled(false);
        QCOMPARE(s.ssqlEnabled(), false);
    }

    // ── ssqlThresh ───────────────────────────────────────────────────────────

    void ssqlThreshDefaultValue() {
        // From Thetis radio.cs:1187 — _fSSqlThreshold = 0.16f (0..1 linear)
        // Model stores slider units 0–100; 16 maps to 0.16 at WDSP boundary.
        SliceModel s;
        QCOMPARE(s.ssqlThresh(), 16.0);
    }

    void setSsqlThreshStoresValue() {
        SliceModel s;
        s.setSsqlThresh(-80.0);
        QCOMPARE(s.ssqlThresh(), -80.0);
    }

    void setSsqlThreshEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::ssqlThreshChanged);
        s.setSsqlThresh(-90.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), -90.0);
    }

    // ── amsqEnabled ──────────────────────────────────────────────────────────

    void amsqEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1293 — rx_am_squelch_on default = false
        SliceModel s;
        QCOMPARE(s.amsqEnabled(), false);
    }

    void setAmsqEnabledStoresValue() {
        SliceModel s;
        s.setAmsqEnabled(true);
        QCOMPARE(s.amsqEnabled(), true);
    }

    void setAmsqEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::amsqEnabledChanged);
        s.setAmsqEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setAmsqEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setAmsqEnabled(true);
        QSignalSpy spy(&s, &SliceModel::amsqEnabledChanged);
        s.setAmsqEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    // ── amsqThresh ───────────────────────────────────────────────────────────

    void amsqThreshDefaultValue() {
        // From Thetis radio.cs:1164 — rx_squelch_threshold = -150.0f dB
        SliceModel s;
        QCOMPARE(s.amsqThresh(), -150.0);
    }

    void setAmsqThreshStoresValue() {
        SliceModel s;
        s.setAmsqThresh(-60.0);
        QCOMPARE(s.amsqThresh(), -60.0);
    }

    void setAmsqThreshEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::amsqThreshChanged);
        s.setAmsqThresh(-70.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), -70.0);
    }

    // ── fmsqEnabled ──────────────────────────────────────────────────────────

    void fmsqEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1312 — rx_fm_squelch_on default = false
        SliceModel s;
        QCOMPARE(s.fmsqEnabled(), false);
    }

    void setFmsqEnabledStoresValue() {
        SliceModel s;
        s.setFmsqEnabled(true);
        QCOMPARE(s.fmsqEnabled(), true);
    }

    void setFmsqEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::fmsqEnabledChanged);
        s.setFmsqEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setFmsqEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setFmsqEnabled(true);
        QSignalSpy spy(&s, &SliceModel::fmsqEnabledChanged);
        s.setFmsqEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setFmsqEnabledToggleRoundTrip() {
        SliceModel s;
        s.setFmsqEnabled(true);
        s.setFmsqEnabled(false);
        QCOMPARE(s.fmsqEnabled(), false);
    }

    // ── fmsqThresh ───────────────────────────────────────────────────────────

    void fmsqThreshDefaultValue() {
        // SliceModel default -150.0 dB (maps to ~3.16e-8 linear; near squelch open)
        // Note: Thetis fm_squelch_threshold = 1.0f is linear — conversion happens
        // in RxChannel::setFmsqThresh via pow(10.0, dB/20.0)
        SliceModel s;
        QCOMPARE(s.fmsqThresh(), -150.0);
    }

    void setFmsqThreshStoresValue() {
        SliceModel s;
        s.setFmsqThresh(-40.0);
        QCOMPARE(s.fmsqThresh(), -40.0);
    }

    void setFmsqThreshEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::fmsqThreshChanged);
        s.setFmsqThresh(-50.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), -50.0);
    }
};

QTEST_MAIN(TestSliceSquelch)
#include "tst_slice_squelch.moc"
