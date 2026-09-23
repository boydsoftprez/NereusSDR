// =================================================================
// tests/tst_slice_tune_step_list.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source [v2.10.3.15] (commit 3759d09):
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: tune_step_list parity, ChangeTuneStepUp/Down wrap and 100 Hz
//                default guard tests by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
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
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

struct ExpectedStep {
    int stepHz;
    const char* name;
};

// Written out literally from Thetis console.cs:1955-1980 [v2.10.3.15] so the
// test does not read the expectation back from kTuneStepList.
constexpr ExpectedStep kExpected[] = {
    {1, "1Hz"},
    {2, "2Hz"},
    {10, "10Hz"},
    {25, "25Hz"},
    {50, "50Hz"},
    {100, "100Hz"},
    {250, "250Hz"},
    {500, "500Hz"},
    {1000, "1kHz"},
    {2000, "2kHz"},
    {2500, "2.5kHz"},
    {5000, "5kHz"},
    {6250, "6.25kHz"},
    {9000, "9kHz"},
    {10000, "10kHz"},
    {12500, "12.5kHz"},
    {15000, "15kHz"},
    {20000, "20kHz"},
    {25000, "25kHz"},
    {30000, "30kHz"},
    {50000, "50kHz"},
    {100000, "100kHz"},
    {250000, "250kHz"},
    {500000, "500kHz"},
    {1000000, "1MHz"},
    {10000000, "10MHz"},
};
constexpr int kExpectedSize = static_cast<int>(sizeof(kExpected) / sizeof(kExpected[0]));

} // namespace

class TestSliceTuneStepList : public QObject {
    Q_OBJECT

private slots:
    // ── Table parity ─────────────────────────────────────────────────────────

    void tableMatchesThetisPairs() {
        QCOMPARE(kExpectedSize, 26);
        QCOMPARE(kTuneStepListSize, 26);
        for (int i = 0; i < kExpectedSize; ++i) {
            QCOMPARE(kTuneStepList[i].stepHz, kExpected[i].stepHz);
            QCOMPARE(QString::fromLatin1(kTuneStepList[i].name),
                     QString::fromLatin1(kExpected[i].name));
        }
    }

    void tableIsStrictlyAscending() {
        for (int i = 1; i < kTuneStepListSize; ++i) {
            QVERIFY2(kTuneStepList[i].stepHz > kTuneStepList[i - 1].stepHz,
                     qPrintable(QStringLiteral("entry %1 not above entry %2")
                                    .arg(i).arg(i - 1)));
        }
    }

    // ── Lookup ───────────────────────────────────────────────────────────────

    void indexForHzRoundTrips() {
        for (int i = 0; i < kTuneStepListSize; ++i) {
            QCOMPARE(tuneStepIndexForHz(kTuneStepList[i].stepHz), i);
        }
    }

    void indexForHzMissReturnsMinusOne() {
        QCOMPARE(tuneStepIndexForHz(3000), -1);
        QCOMPARE(tuneStepIndexForHz(0), -1);
        QCOMPARE(tuneStepIndexForHz(-1), -1);
        QCOMPARE(tuneStepIndexForHz(20000000), -1);
    }

    // ── Wrap across all 26 entries ───────────────────────────────────────────

    void upWrapsAcrossAllEntries() {
        SliceModel s;
        s.setStepHz(1);
        QSignalSpy spy(&s, &SliceModel::stepHzChanged);

        for (int call = 1; call <= kExpectedSize; ++call) {
            s.changeTuneStepUp();
            const int expectedHz = kExpected[call % kExpectedSize].stepHz;
            QCOMPARE(s.stepHz(), expectedHz);
        }
        QCOMPARE(s.stepHz(), 1);

        QCOMPARE(spy.count(), kExpectedSize);
        for (int call = 1; call <= kExpectedSize; ++call) {
            QCOMPARE(spy.at(call - 1).at(0).toInt(),
                     kExpected[call % kExpectedSize].stepHz);
        }
        // Call 25 lands on the last entry, call 26 wraps to the first.
        QCOMPARE(spy.at(24).at(0).toInt(), 10000000);
        QCOMPARE(spy.at(25).at(0).toInt(), 1);
    }

    void downWrapsAcrossAllEntries() {
        SliceModel s;
        s.setStepHz(1);
        QSignalSpy spy(&s, &SliceModel::stepHzChanged);

        s.changeTuneStepDown();
        QCOMPARE(s.stepHz(), 10000000);

        for (int idx = kExpectedSize - 2; idx >= 0; --idx) {
            s.changeTuneStepDown();
            QCOMPARE(s.stepHz(), kExpected[idx].stepHz);
        }
        QCOMPARE(s.stepHz(), 1);

        QCOMPARE(spy.count(), kExpectedSize);
        QCOMPARE(spy.at(0).at(0).toInt(), 10000000);
        for (int n = 1; n < kExpectedSize; ++n) {
            QCOMPARE(spy.at(n).at(0).toInt(), kExpected[kExpectedSize - 1 - n].stepHz);
        }
    }

    // ── Off-list values (NereusSDR-native neighbour rule) ────────────────────

    void offListBetweenEntriesMovesToNeighbour() {
        SliceModel up;
        up.setStepHz(3000);
        up.changeTuneStepUp();
        QCOMPARE(up.stepHz(), 5000);

        SliceModel down;
        down.setStepHz(3000);
        down.changeTuneStepDown();
        QCOMPARE(down.stepHz(), 2500);
    }

    void offListAboveTopWrapsUpAndStepsDownToLast() {
        SliceModel up;
        up.setStepHz(20000000);
        up.changeTuneStepUp();
        QCOMPARE(up.stepHz(), 1);

        SliceModel down;
        down.setStepHz(20000000);
        down.changeTuneStepDown();
        QCOMPARE(down.stepHz(), 10000000);
    }

    void offListNearBottomMovesToNeighbour() {
        SliceModel up;
        up.setStepHz(3);
        up.changeTuneStepUp();
        QCOMPARE(up.stepHz(), 10);

        SliceModel down;
        down.setStepHz(3);
        down.changeTuneStepDown();
        QCOMPARE(down.stepHz(), 2);
    }

    // ── Default and migration guards ─────────────────────────────────────────

    void defaultStepIs100Hz() {
        SliceModel s;
        QCOMPARE(s.stepHz(), 100);
        QCOMPARE(tuneStepIndexForHz(100), 5);
    }

    void oldStubValuesAreOnList() {
        const int oldStub[] = {1, 10, 100, 500, 1000, 10000};
        for (int v : oldStub) {
            QVERIFY2(tuneStepIndexForHz(v) >= 0,
                     qPrintable(QStringLiteral("%1 Hz is not on the list").arg(v)));
        }
    }
};

QTEST_MAIN(TestSliceTuneStepList)
#include "tst_slice_tune_step_list.moc"
