// =================================================================
// tests/tst_noise_floor_tracker.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

// Auto AGC-T — NoiseFloorTracker unit tests. Lerp-based noise floor
// tracker ported from Thetis display.cs:4628-4693. Strict TDD: each
// test was written before the corresponding code path exists (RED),
// verified failing, then the minimal code to pass lands.

#include <QtTest/QtTest>

#include "core/NoiseFloorTracker.h"

#include <cmath>

using namespace NereusSDR;

class TestNoiseFloorTracker : public QObject {
    Q_OBJECT
private slots:
    void transmitHoldsBothFloorsAndAcquisitionTime()
    {
        NoiseFloorTracker t;
        t.setAcquisitionEpoch(2);
        t.feedForEpoch(QVector<float>(4096, -85.0f), 33.0f, 2);
        t.feedForEpoch(QVector<float>(4096, -85.0f), 500.0f, 2);
        QVERIFY(!t.isGood());
        t.setAcquisitionEpoch(3);
        for (int i = 0; i < 200; ++i) {
            t.feedForEpoch(QVector<float>(4096, -115.0f), 33.0f, 3);
        }
        QCOMPARE(t.noiseFloor(), -85.0f);
        QCOMPARE(t.agcNoiseFloor(), -85.0f);
        QVERIFY(!t.isGood());
        QVERIFY(!t.acquisitionCurrent());
        t.setAcquisitionEpoch(4);
        t.feedForEpoch(QVector<float>(4096, -115.0f), 2000.0f, 2);
        QVERIFY(!t.acquisitionCurrent());
        t.feedForEpoch(QVector<float>(4096, -85.0f), 500.0f, 4);
        QVERIFY(t.acquisitionCurrent());
        QVERIFY(!t.isGood());
        t.feedForEpoch(QVector<float>(4096, -85.0f), 1000.0f, 4);
        QVERIFY(t.isGood());
        QCOMPARE(t.agcNoiseFloor(), -85.0f);
    }


    void defaultsAreCorrect()
    {
        // Fresh tracker: floor at sentinel -200, not converged, 2s attack.
        NoiseFloorTracker t;
        QCOMPARE(t.noiseFloor(), -200.0f);
        QCOMPARE(t.isGood(), false);
        QCOMPARE(t.attackTimeMs(), 2000.0f);
    }

    void singleFeedUpdatesFloor()
    {
        // After one feed at -90 dBm, the floor moves from -200 toward
        // -90 but hasn't arrived (fast attack snaps to bin average on
        // first feed since m_fastAttackPending starts true).
        NoiseFloorTracker t;
        QVector<float> bins(2048, -90.0f);
        t.feed(bins, 100.0f);
        // Fast attack snaps to -90 on first feed
        QCOMPARE(t.noiseFloor(), -90.0f);
    }

    void convergesAfterManyFeeds()
    {
        // After 50 feeds at 100ms each (5s total, > 2s attack time)
        // with -95 dBm bins, the tracker should converge within 1 dB
        // and report isGood.
        NoiseFloorTracker t;
        QVector<float> bins(2048, -95.0f);
        for (int i = 0; i < 50; ++i) {
            t.feed(bins, 100.0f);
        }
        QVERIFY(std::abs(t.noiseFloor() - (-95.0f)) < 1.0f);
        QVERIFY(t.isGood());
    }

    void fastAttackResetsLerp()
    {
        // Converge to -80, then trigger fast attack and feed -110.
        // The tracker should jump close to -110 (within 5 dB).
        NoiseFloorTracker t;
        QVector<float> bins80(2048, -80.0f);
        for (int i = 0; i < 50; ++i) {
            t.feed(bins80, 100.0f);
        }
        QVERIFY(std::abs(t.noiseFloor() - (-80.0f)) < 1.0f);

        t.triggerFastAttack();
        QVector<float> bins110(2048, -110.0f);
        t.feed(bins110, 100.0f);
        // Fast attack snaps directly to bin average
        QVERIFY(std::abs(t.noiseFloor() - (-110.0f)) < 5.0f);
    }

    void isGoodAfterAttackPeriod()
    {
        // isGood() is false until elapsed exceeds attackTimeMs (2000ms).
        NoiseFloorTracker t;
        QVector<float> bins(2048, -95.0f);

        // First feed triggers fast attack — elapsed resets to 0
        t.feed(bins, 100.0f);
        QVERIFY(!t.isGood());

        // Feed 19 more frames at 100ms each = 1900ms elapsed (< 2000)
        for (int i = 0; i < 19; ++i) {
            t.feed(bins, 100.0f);
        }
        QVERIFY(!t.isGood());

        // One more frame = 2000ms elapsed (>= 2000)
        t.feed(bins, 100.0f);
        QVERIFY(t.isGood());
    }

    void emptyBinsNoOp()
    {
        // Empty bins vector doesn't crash and leaves floor at sentinel.
        NoiseFloorTracker t;
        QVector<float> empty;
        t.feed(empty, 100.0f);
        QCOMPARE(t.noiseFloor(), -200.0f);
        QCOMPARE(t.isGood(), false);
    }

    // NereusSDR-original. The floor Auto AGC-T reads is each frame's mean
    // plus 10 * log10(N / 4096), where N is the frame's size: nothing at
    // 4096, 3.01 dB per doubling above it.
    void agcReferenceOffsetIsThreeDbPerDoublingFrom4096()
    {
        QCOMPARE(NoiseFloorTracker::kAgcReferenceFftSize, 4096);
        QCOMPARE(NoiseFloorTracker::agcReferenceOffsetDb(4096), 0.0f);
        for (int step = 1; step <= 6; ++step) {
            const float expected = 3.0103f * static_cast<float>(step);
            QVERIFY(std::abs(NoiseFloorTracker::agcReferenceOffsetDb(4096 << step) - expected)
                    < 0.001f);
        }
        QVERIFY(std::abs(NoiseFloorTracker::agcReferenceOffsetDb(2048) + 3.0103f) < 0.001f);
        QCOMPARE(NoiseFloorTracker::agcReferenceOffsetDb(0), 0.0f);
    }

    // A tracker fed 4096-bin frames (the Core's) gives the same number
    // twice, bit for bit, through the snap and the lerp.
    void agcFloorEqualsTheFloorAt4096Bins()
    {
        NoiseFloorTracker t;
        QCOMPARE(t.agcNoiseFloor(), -200.0f);
        t.feed(QVector<float>(4096, -90.0f), 33.0f);
        QCOMPARE(t.agcNoiseFloor(), t.noiseFloor());
        for (int i = 0; i < 40; ++i) {
            t.feed(QVector<float>(4096, -110.0f + static_cast<float>(i)), 33.0f);
            QCOMPARE(t.agcNoiseFloor(), t.noiseFloor());
        }
        t.feed(QVector<float>(), 33.0f);
        QCOMPARE(t.agcNoiseFloor(), t.noiseFloor());
    }

    // The same noise seen through a larger FFT: each bin is 3.01 dB lower
    // per doubling and the referred floor stays where it was, through a
    // size change with no restart. The floor per bin glides down as before.
    void agcFloorStaysPutWhenTheFrameSizeChanges()
    {
        // 0.05 dB: the frame mean is a single-precision sum, which is
        // that coarse over 131072 bins.
        constexpr float kTolerance = 0.05f;
        NoiseFloorTracker t;
        t.feed(QVector<float>(4096, -100.0f), 33.0f);
        QVERIFY(std::abs(t.noiseFloor() + 100.0f) < kTolerance);
        QCOMPARE(t.agcNoiseFloor(), t.noiseFloor());
        for (int step = 1; step <= 5; ++step) {
            const float perBin = -100.0f - 3.0103f * static_cast<float>(step);
            for (int i = 0; i < 10; ++i) {
                t.feed(QVector<float>(4096 << step, perBin), 33.0f);
                QVERIFY(!t.fastAttackPending());
                QVERIFY(std::abs(t.agcNoiseFloor() + 100.0f) < kTolerance);
            }
        }
        // 50 frames at 33 ms against the 2 s attack: the floor per bin is
        // well on its way down and has not arrived.
        QVERIFY(t.noiseFloor() < -103.0f);
        QVERIFY(t.noiseFloor() > -115.05f);
        // A fast attack snaps both.
        t.triggerFastAttack();
        t.feed(QVector<float>(131072, -115.0515f), 33.0f);
        QVERIFY(std::abs(t.noiseFloor() + 115.0515f) < kTolerance);
        QVERIFY(std::abs(t.agcNoiseFloor() + 100.0f) < kTolerance);
    }
};

QTEST_MAIN(TestNoiseFloorTracker)
#include "tst_noise_floor_tracker.moc"
