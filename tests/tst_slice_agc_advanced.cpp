// =================================================================
// tests/tst_slice_agc_advanced.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

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

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceAgcAdvanced : public QObject {
    Q_OBJECT

private slots:
    // ── agcThreshold ─────────────────────────────────────────────────────────

    void agcThresholdDefaultIsMinusTwenty() {
        SliceModel s;
        QCOMPARE(s.agcThreshold(), -20);
    }

    void setAgcThresholdStoresValue() {
        SliceModel s;
        s.setAgcThreshold(-80);
        QCOMPARE(s.agcThreshold(), -80);
    }

    void setAgcThresholdEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcThresholdChanged);
        s.setAgcThreshold(-100);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), -100);
    }

    void setAgcThresholdNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcThreshold(-50);
        QSignalSpy spy(&s, &SliceModel::agcThresholdChanged);
        s.setAgcThreshold(-50);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    // ── agcHang ──────────────────────────────────────────────────────────────

    void agcHangDefaultIs250() {
        // From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
        SliceModel s;
        QCOMPARE(s.agcHang(), 250);
    }

    void setAgcHangStoresValue() {
        SliceModel s;
        s.setAgcHang(500);
        QCOMPARE(s.agcHang(), 500);
    }

    void setAgcHangEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcHangChanged);
        s.setAgcHang(750);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 750);
    }

    void setAgcHangNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcHang(100);
        QSignalSpy spy(&s, &SliceModel::agcHangChanged);
        s.setAgcHang(100);
        QCOMPARE(spy.count(), 0);
    }

    // ── agcSlope ─────────────────────────────────────────────────────────────

    void agcSlopeDefaultIsZero() {
        // From Thetis radio.cs:1107-1108 — rx_agc_slope = 0
        SliceModel s;
        QCOMPARE(s.agcSlope(), 0);
    }

    void setAgcSlopeStoresValue() {
        SliceModel s;
        s.setAgcSlope(5);
        QCOMPARE(s.agcSlope(), 5);
    }

    void setAgcSlopeEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcSlopeChanged);
        s.setAgcSlope(10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 10);
    }

    void setAgcSlopeNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcSlope(3);
        QSignalSpy spy(&s, &SliceModel::agcSlopeChanged);
        s.setAgcSlope(3);
        QCOMPARE(spy.count(), 0);
    }

    // ── agcAttack ────────────────────────────────────────────────────────────

    void agcAttackDefaultIsTwo() {
        // From WDSP wcpAGC.c create_wcpagc — tau_attack default 2 ms
        SliceModel s;
        QCOMPARE(s.agcAttack(), 2);
    }

    void setAgcAttackStoresValue() {
        SliceModel s;
        s.setAgcAttack(8);
        QCOMPARE(s.agcAttack(), 8);
    }

    void setAgcAttackEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcAttackChanged);
        s.setAgcAttack(5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
    }

    void setAgcAttackNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcAttack(2);
        QSignalSpy spy(&s, &SliceModel::agcAttackChanged);
        s.setAgcAttack(2);
        QCOMPARE(spy.count(), 0);
    }

    // ── agcDecay ─────────────────────────────────────────────────────────────

    void agcDecayDefaultIs250() {
        // From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
        SliceModel s;
        QCOMPARE(s.agcDecay(), 250);
    }

    void setAgcDecayStoresValue() {
        SliceModel s;
        s.setAgcDecay(1000);
        QCOMPARE(s.agcDecay(), 1000);
    }

    void setAgcDecayEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcDecayChanged);
        s.setAgcDecay(300);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 300);
    }

    void setAgcDecayNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcDecay(250);
        QSignalSpy spy(&s, &SliceModel::agcDecayChanged);
        s.setAgcDecay(250);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSliceAgcAdvanced)
#include "tst_slice_agc_advanced.moc"
