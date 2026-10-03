// =================================================================
// tests/tst_active_peak_hold.cpp  (NereusSDR)
// =================================================================
//
// Task 2.5 — ActivePeakHoldTrace unit tests.
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
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

// Tests run WDSP-free (no QApplication needed for pure data logic).

#include <QtTest/QtTest>
#include "core/spectrum/ActivePeakHoldTrace.h"

#include <cmath>

using namespace NereusSDR;

class TestActivePeakHold : public QObject {
    Q_OBJECT

private slots:

    // ---- Construction + size ----

    void default_construction_has_zero_size()
    {
        ActivePeakHoldTrace trace;
        QCOMPARE(trace.size(), 0);
        QVERIFY(!trace.enabled());
    }

    void construction_with_nBins_sets_size()
    {
        ActivePeakHoldTrace trace(256);
        QCOMPARE(trace.size(), 256);
        // All bins should start at -infinity
        QVERIFY(!std::isfinite(trace.peak(0)));
        QVERIFY(!std::isfinite(trace.peak(255)));
    }

    void resize_updates_size_and_clears()
    {
        ActivePeakHoldTrace trace(64);
        trace.setEnabled(true);
        QVector<float> bins(64, -40.0f);
        trace.update(bins);
        QCOMPARE(trace.peak(0), -40.0f);

        trace.resize(128);
        QCOMPARE(trace.size(), 128);
        // All peaks reset to -infinity after resize
        QVERIFY(!std::isfinite(trace.peak(0)));
        QVERIFY(!std::isfinite(trace.peak(127)));
    }

    // ---- Disabled does nothing ----

    void disabled_does_not_update()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(false);

        QVector<float> bins(256, -40.0f);
        trace.update(bins);

        // Peaks must remain -infinity when disabled
        QVERIFY(!std::isfinite(trace.peak(0)));
    }

    void disabled_does_not_decay()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);
        QVector<float> bins(256, -40.0f);
        trace.update(bins);
        QCOMPARE(trace.peak(0), -40.0f);

        trace.setEnabled(false);
        trace.tickFrame(30);

        // Peaks should be unchanged (tickFrame no-ops when disabled)
        QCOMPARE(trace.peak(0), -40.0f);
    }

    // ---- Peak tracking ----

    void peak_value_persists_until_decay()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);
        trace.setDurationMs(0);
        trace.setDropDbPerSec(6.0);

        QVector<float> bins(256, -100.0f);
        bins[100] = -40.0f;
        trace.update(bins);
        trace.tickFrame(30);   // raised this frame: elapsed 0, no fall
        QCOMPARE(trace.peak(100), -40.0f);

        // Next frame: live value drops to -50. 6 dB/s / 30 fps = 0.2 dB/frame.
        bins[100] = -50.0f;
        trace.update(bins);    // -50 < -40 so peak unchanged
        trace.tickFrame(30);   // 33 ms old > 0 ms hold: peak -> -40.2

        QVERIFY(qAbs(trace.peak(100) - (-40.2f)) < 0.001f);
    }

    void peak_raises_on_stronger_signal()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);

        QVector<float> bins(256, -60.0f);
        trace.update(bins);
        QCOMPARE(trace.peak(0), -60.0f);

        bins[0] = -30.0f;
        trace.update(bins);
        QCOMPARE(trace.peak(0), -30.0f);  // raised to stronger value
    }

    void peak_does_not_lower_on_weaker_signal()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);
        trace.setDropDbPerSec(0.0);  // no decay so only update() can change values

        QVector<float> bins(256, -30.0f);
        trace.update(bins);

        bins[0] = -60.0f;
        trace.update(bins);
        QCOMPARE(trace.peak(0), -30.0f);  // stays at stronger previous value
    }

    // ---- Decay ----

    void decay_reduces_peak_by_expected_amount()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);
        trace.setDurationMs(0);
        trace.setDropDbPerSec(6.0);

        QVector<float> bins(256, -40.0f);
        trace.update(bins);
        trace.tickFrame(30);   // the frame it was raised in: no fall
        QCOMPARE(trace.peak(0), -40.0f);

        // One frame later at 30 fps: a 0.2 dB drop.
        trace.tickFrame(30);
        QVERIFY(qAbs(trace.peak(0) - (-40.2f)) < 0.001f);
    }

    // Thetis display.cs:5359-5363 [v2.10.3.15]: a bin falls only once
    // local_frame_start - peak.Time > the hold delay.
    void peak_holds_for_the_hold_duration_then_falls()
    {
        ActivePeakHoldTrace trace(8);
        trace.setEnabled(true);
        trace.setDurationMs(100);
        trace.setDropDbPerSec(25.0);   // 1 dB a frame at 25 fps (40 ms frames)

        trace.update(QVector<float>(8, -40.0f));
        // Frames at 0, 40 and 80 ms are not more than 100 ms old.
        for (int frame = 0; frame < 3; ++frame) {
            trace.tickFrame(25);
            QCOMPARE(trace.peak(0), -40.0f);
        }
        // 120 ms > 100 ms: it falls, one frame's worth at a time.
        trace.tickFrame(25);
        QVERIFY(qAbs(trace.peak(0) - (-41.0f)) < 0.001f);
        trace.tickFrame(25);
        QVERIFY(qAbs(trace.peak(0) - (-42.0f)) < 0.001f);
    }

    void a_raised_bin_starts_its_hold_again()
    {
        ActivePeakHoldTrace trace(2);
        trace.setEnabled(true);
        trace.setDurationMs(100);
        trace.setDropDbPerSec(25.0);
        trace.update(QVector<float>(2, -40.0f));
        for (int frame = 0; frame < 6; ++frame) { trace.tickFrame(25); }
        QVERIFY(trace.peak(0) < -40.5f);
        // A new maximum at bin 0 only: it holds again from this frame.
        trace.update(QVector<float>{-35.0f, -90.0f});
        for (int frame = 0; frame < 3; ++frame) { trace.tickFrame(25); }
        QCOMPARE(trace.peak(0), -35.0f);
        QVERIFY(trace.peak(1) < -41.5f);   // bin 1 kept falling
    }

    // Thetis display.cs:5011 [v2.10.3.15]: without "Also in TX" the trace is
    // off while this receiver transmits: no update, no fall, not drawn.
    void on_tx_false_freezes_and_hides_while_transmitting()
    {
        ActivePeakHoldTrace trace(4);
        trace.setEnabled(true);
        trace.setDurationMs(0);
        trace.setDropDbPerSec(30.0);
        trace.update(QVector<float>(4, -40.0f));
        trace.tickFrame(30);
        QVERIFY(trace.active());

        trace.setTxActive(true);
        QVERIFY(!trace.active());
        trace.update(QVector<float>(4, -10.0f));
        trace.tickFrame(30);
        trace.tickFrame(30);
        QCOMPARE(trace.peak(0), -40.0f);   // neither raised nor fallen

        trace.setOnTx(true);
        QVERIFY(trace.active());
        trace.update(QVector<float>(4, -10.0f));
        QCOMPARE(trace.peak(0), -10.0f);
    }

    // Thetis display.cs:4527-4530, 859-877, 4219-4221, 5011 [v2.10.3.15]:
    // ResetSpectrumPeaks holds the trace back for 500 ms; it neither rises,
    // falls nor draws until a frame starting more than 500 ms after the
    // reset has ended.
    void clear_holds_the_trace_back_for_500_ms()
    {
        ActivePeakHoldTrace trace(4);
        trace.setEnabled(true);
        trace.setDurationMs(0);
        trace.setDropDbPerSec(25.0);
        trace.update(QVector<float>(4, -40.0f));
        trace.tickFrame(25);                   // frame 0 ms; clock now 40
        QCOMPARE(trace.peak(0), -40.0f);

        trace.clear();                         // reset at 40 ms: delay to 540 ms
        // Frames starting at 40 .. 520 ms: nothing raised.
        for (int frame = 0; frame < 13; ++frame) {
            trace.update(QVector<float>(4, -30.0f));
            QVERIFY(!std::isfinite(trace.peak(0)));
            trace.tickFrame(25);
        }
        // The frame starting at 560 ms (> 540) ends the delay; the next draws.
        trace.update(QVector<float>(4, -30.0f));
        trace.tickFrame(25);
        trace.update(QVector<float>(4, -30.0f));
        QCOMPARE(trace.peak(0), -30.0f);
    }

    void on_tx_true_allows_update_during_tx()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);
        trace.setOnTx(true);    // continue updating during TX
        trace.setTxActive(true);

        QVector<float> bins(256, -40.0f);
        trace.update(bins);
        QCOMPARE(trace.peak(0), -40.0f);
    }

    void tx_inactive_always_allows_update()
    {
        // When TX is NOT active, onTx flag is irrelevant — updates always proceed.
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);
        trace.setOnTx(false);
        trace.setTxActive(false);  // not transmitting

        QVector<float> bins(256, -40.0f);
        trace.update(bins);
        QCOMPARE(trace.peak(0), -40.0f);
    }

    // ---- clear() ----

    void clear_resets_all_peaks_to_infinity()
    {
        ActivePeakHoldTrace trace(256);
        trace.setEnabled(true);
        QVector<float> bins(256, -40.0f);
        trace.update(bins);
        QCOMPARE(trace.peak(0), -40.0f);

        trace.clear();
        QVERIFY(!std::isfinite(trace.peak(0)));
        QVERIFY(!std::isfinite(trace.peak(255)));
    }

    // ---- Accessors ----

    void peak_out_of_range_returns_neg_infinity()
    {
        ActivePeakHoldTrace trace(16);
        QVERIFY(!std::isfinite(trace.peak(-1)));
        QVERIFY(!std::isfinite(trace.peak(16)));
        QVERIFY(!std::isfinite(trace.peak(1000)));
    }

    void configuration_round_trip()
    {
        ActivePeakHoldTrace trace;
        trace.setEnabled(true);
        trace.setDurationMs(3000);
        trace.setDropDbPerSec(12.0);
        trace.setFill(true);
        trace.setOnTx(true);

        QVERIFY(trace.enabled());
        QCOMPARE(trace.durationMs(), 3000);
        QCOMPARE(trace.dropDbPerSec(), 12.0);
        QVERIFY(trace.fill());
    }
};

QTEST_APPLESS_MAIN(TestActivePeakHold)
#include "tst_active_peak_hold.moc"
