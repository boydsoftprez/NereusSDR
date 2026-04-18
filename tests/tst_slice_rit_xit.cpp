// =================================================================
// tests/tst_slice_rit_xit.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
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

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceRitXit : public QObject {
    Q_OBJECT

private slots:
    // ── effectiveRxFrequency ─────────────────────────────────────────────────

    void effectiveRxFrequencyNoRitEqualsFrequency() {
        SliceModel s;
        s.setFrequency(14225000.0);
        s.setRitEnabled(false);
        s.setRitHz(500);
        // RIT disabled → effective = base frequency
        QCOMPARE(s.effectiveRxFrequency(), 14225000.0);
    }

    void effectiveRxFrequencyWithRitAddsOffset() {
        SliceModel s;
        s.setFrequency(14225000.0);
        s.setRitHz(300);
        s.setRitEnabled(true);
        // RIT enabled → effective = base + offset
        QCOMPARE(s.effectiveRxFrequency(), 14225300.0);
    }

    void effectiveRxFrequencyNegativeRitOffset() {
        SliceModel s;
        s.setFrequency(7200000.0);
        s.setRitHz(-150);
        s.setRitEnabled(true);
        QCOMPARE(s.effectiveRxFrequency(), 7199850.0);
    }

    void effectiveRxFrequencyZeroRitOffset() {
        SliceModel s;
        s.setFrequency(3700000.0);
        s.setRitHz(0);
        s.setRitEnabled(true);
        // Zero offset — enabled but no shift
        QCOMPARE(s.effectiveRxFrequency(), 3700000.0);
    }

    // ── setRitHz while disabled doesn't change effective frequency ────────────

    void setRitHzWhileDisabledDoesNotAffectEffective() {
        SliceModel s;
        s.setFrequency(14000000.0);
        s.setRitEnabled(false);
        s.setRitHz(1000);
        QCOMPARE(s.effectiveRxFrequency(), 14000000.0);
        // Changing Hz while disabled still no effect
        s.setRitHz(-500);
        QCOMPARE(s.effectiveRxFrequency(), 14000000.0);
    }

    // ── ritEnabled signal guard ───────────────────────────────────────────────

    void setRitEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::ritEnabledChanged);
        s.setRitEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setRitEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setRitEnabled(true);
        QSignalSpy spy(&s, &SliceModel::ritEnabledChanged);
        s.setRitEnabled(true);
        QCOMPARE(spy.count(), 0);
    }

    // ── setRitHz signal guard ─────────────────────────────────────────────────

    void setRitHzEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::ritHzChanged);
        s.setRitHz(200);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 200);
    }

    void setRitHzNoSignalOnSameValue() {
        SliceModel s;
        s.setRitHz(100);
        QSignalSpy spy(&s, &SliceModel::ritHzChanged);
        s.setRitHz(100);
        QCOMPARE(spy.count(), 0);
    }

    // ── XIT stores value, no RX effect ────────────────────────────────────────

    void setXitHzStoresValue() {
        SliceModel s;
        s.setXitHz(400);
        QCOMPARE(s.xitHz(), 400);
    }

    void setXitEnabledStoresValue() {
        SliceModel s;
        s.setXitEnabled(true);
        QVERIFY(s.xitEnabled());
    }

    void xitHzDoesNotAffectEffectiveRxFrequency() {
        // XIT is TX-side only — no RX effect in 3G-10.
        SliceModel s;
        s.setFrequency(14100000.0);
        s.setXitHz(500);
        s.setXitEnabled(true);
        // Effective RX frequency is unchanged (RIT disabled)
        QCOMPARE(s.effectiveRxFrequency(), 14100000.0);
    }

    void setXitHzEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::xitHzChanged);
        s.setXitHz(350);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 350);
    }

    void setXitHzNoSignalOnSameValue() {
        SliceModel s;
        s.setXitHz(200);
        QSignalSpy spy(&s, &SliceModel::xitHzChanged);
        s.setXitHz(200);
        QCOMPARE(spy.count(), 0);
    }

    // ── Default values ────────────────────────────────────────────────────────

    void defaultRitEnabledIsFalse() {
        SliceModel s;
        QVERIFY(!s.ritEnabled());
    }

    void defaultRitHzIsZero() {
        SliceModel s;
        QCOMPARE(s.ritHz(), 0);
    }

    void defaultXitEnabledIsFalse() {
        SliceModel s;
        QVERIFY(!s.xitEnabled());
    }

    void defaultXitHzIsZero() {
        SliceModel s;
        QCOMPARE(s.xitHz(), 0);
    }
};

QTEST_MAIN(TestSliceRitXit)
#include "tst_slice_rit_xit.moc"
