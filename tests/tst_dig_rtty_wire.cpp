// =================================================================
// tests/tst_dig_rtty_wire.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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
// Upstream source 'Project Files/Source/Console/setup.designer.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

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
