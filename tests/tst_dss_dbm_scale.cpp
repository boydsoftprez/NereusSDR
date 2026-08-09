// From AetherSDR SpectrumWidget.cpp:17223-17335 [@1872028c] (shared dBm-
// scale chrome/labels helpers plus drawDbmScale3D itself). Task 11 of the
// 3D stacked-trace spectrum plan (design doc docs/architecture/2026-08-08-
// 3d-stacked-trace-spectrum-design.md, plan docs/architecture/2026-08-08-3d-
// stacked-trace-spectrum-plan.md).
//
// Pins: in 3D mode the amplitude scale is anchored to the drifting MEASURED
// noise floor (dssFloorDbm()) plus the rounded 3D Span, not to the Ref
// level the 2D scale uses -- so its rendered output must move when the
// measured floor moves, and the existing 2D drawDbmScale() must stay
// byte-for-byte untouched (see the "2D path" section of the task-11
// report for how that was confirmed).

#include <QTest>
#include <QImage>
#include <QPainter>
#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestDssDbmScale : public QObject {
    Q_OBJECT

private slots:
    // I2 fix (final review): the GPU-path 3D dBm-scale overlay cache must
    // dirty when the noise floor drifts far enough to change the rounded
    // label set drawDbmScaleLabels() draws, but must NOT dirty on every
    // sub-pixel lerp step (the overlay rebuild is this file's own
    // documented dominant per-paint cost -- see the "2026-05-25 perf fix"
    // comments in SpectrumWidget.cpp). floorDbm values below are chosen
    // 10 dB clear of the nearest 20 dB grid line at this dbmRange (see
    // tst_spectrum_dbm_strip.cpp's dssRoundedLabelSet_matchesKnownValues
    // for why an exact-grid-line floor is the wrong precondition to test
    // against) -- m_dssFloorDepth defaults to 6, so a -124 measured floor
    // is a -130 dssFloorDbm().
    void overlayFreshness_dirtiesOnLabelChangingFloorShift() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-140.0f, -40.0f);

        w.setMeasuredNoiseFloorForTest(-124.0f);
        w.refreshDssScaleOverlayFreshnessForTest();  // bakes the reference
        w.clearOverlayStaticDirtyForTest();

        // Full 20 dB shift (one adaptive step at this range -- see
        // adaptiveStepDb_selectsByRange in tst_spectrum_dbm_strip.cpp):
        // guaranteed to change the rounded label set.
        w.setMeasuredNoiseFloorForTest(-104.0f);
        w.refreshDssScaleOverlayFreshnessForTest();
        QVERIFY2(w.overlayStaticDirtyForTest(),
                 "a floor shift large enough to change the rounded label "
                 "set must dirty the cached 3D dBm-scale overlay");
    }

    void overlayFreshness_ignoresSubLabelFloorDrift() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-140.0f, -40.0f);

        w.setMeasuredNoiseFloorForTest(-124.0f);
        w.refreshDssScaleOverlayFreshnessForTest();  // bakes the reference
        w.clearOverlayStaticDirtyForTest();

        // A lerp-sized drift (well under a tenth of a dB) that cannot
        // cross a 20 dB grid line starting 10 dB clear of the nearest one.
        w.setMeasuredNoiseFloorForTest(-124.05f);
        w.refreshDssScaleOverlayFreshnessForTest();
        QVERIFY2(!w.overlayStaticDirtyForTest(),
                 "a sub-pixel floor drift that does not change any rounded "
                 "label must NOT dirty the cached 3D dBm-scale overlay -- "
                 "see the perf rationale in "
                 "SpectrumWidget::updateDssScaleOverlayFreshness()");
    }

    // In 3D the scale is anchored to the drifting noise floor, not to the
    // Ref level, so its labels must move when the floor moves. This is the
    // task brief's own given test; dssFloorDbm() itself is already pinned
    // exhaustively by tst_dss_floor_and_span.cpp (Task 9), so treat this as
    // a precondition sanity check only -- render3D_shiftsWithMeasuredFloor
    // below is what actually proves drawDbmScale3D() binds to it.
    void labels_followTheFloorAnchorIn3D() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-140.0f, -40.0f);
        w.setMeasuredNoiseFloorForTest(-120.0f);
        const float a = w.dssFloorDbm();
        w.setMeasuredNoiseFloorForTest(-100.0f);
        const float b = w.dssFloorDbm();
        QVERIFY2(std::abs(b - a) > 19.0f,
                 "3D scale anchor must track the measured floor");
    }

    // Strengthens labels_followTheFloorAnchorIn3D: that test only shows
    // dssFloorDbm() (the DEPENDENCY) moves -- it never calls drawDbmScale3D
    // at all, so it cannot catch a bug where the floorDbm PARAMETER is
    // ignored or hardcoded inside the function that actually paints it.
    // Renders twice with two floors that are known (from the test above)
    // to differ by more than 19 dB and requires the two images to differ
    // pixel-for-pixel -- a hardcoded/ignored floorDbm would render both
    // canvases identically and this would catch it.
    void render3D_shiftsWithMeasuredFloor() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-140.0f, -40.0f);
        const QRect rect(0, 0, 200, 280);

        w.setMeasuredNoiseFloorForTest(-120.0f);
        const float floorA = w.dssFloorDbm();
        QImage canvasA(200, 300, QImage::Format_ARGB32_Premultiplied);
        canvasA.fill(Qt::transparent);
        {
            QPainter p(&canvasA);
            w.drawDbmScale3DForTest(p, rect, floorA);
        }

        w.setMeasuredNoiseFloorForTest(-100.0f);
        const float floorB = w.dssFloorDbm();
        QImage canvasB(200, 300, QImage::Format_ARGB32_Premultiplied);
        canvasB.fill(Qt::transparent);
        {
            QPainter p(&canvasB);
            w.drawDbmScale3DForTest(p, rect, floorB);
        }

        QVERIFY2(std::abs(floorB - floorA) > 19.0f,
                 "precondition: measured floor moved (see "
                 "labels_followTheFloorAnchorIn3D)");
        QVERIFY2(canvasA != canvasB,
                 "drawDbmScale3D's rendered output must change when its "
                 "floorDbm parameter changes -- comparing two floors that "
                 "are equal for an unrelated reason would not catch a "
                 "hardcoded/ignored parameter");
    }

    // Fast-follow (post-review): drawDbmScale3D, buildDssImage (Task 10),
    // and writeDssMeshUbo (Task 9) all read dssSpanDb() rounded to the
    // nearest 0.5 dB, via one shared, named accessor -- dssRoundedSpanDb().
    // Pins the accessor's own arithmetic. 33.25 is deliberately not a
    // 0.5 dB multiple, and both -140.0f/-106.75f and 33.25/33.5 are exactly
    // representable in float32 (0.75 and 0.25 are both binary fractions),
    // so this can use a tight tolerance rather than a loose one.
    void roundedSpanDb_roundsToNearestHalfDb() {
        SpectrumWidget w;
        w.setDbmRange(-140.0f, -106.75f);
        QVERIFY2(std::abs(w.dssSpanDb() - 33.25f) < 1e-6f,
                 "precondition: raw span is exactly 33.25 (not a 0.5 dB multiple)");
        QVERIFY2(std::abs(w.dssRoundedSpanDb() - 33.5f) < 1e-6f,
                 "dssRoundedSpanDb() must round 33.25 to the nearest 0.5 dB (33.5)");
    }

    // The actual "agreement" pin the review asked for. writeDssMeshUbo()
    // cannot be called from a unit test (it needs a live QRhi resource
    // batch and m_dssUbo -- see task-9-report.md's own empirical-only
    // verification of that function), so this proves the strongest thing
    // that IS directly testable: drawDbmScale3D's real, black-box rendered
    // output is IDENTICAL to an image independently reconstructed from
    // public pieces using dssRoundedSpanDb() -- the exact same accessor
    // writeDssMeshUbo's source now calls for its rangeDb field (confirmed
    // by reading SpectrumWidget.cpp directly, not assumed). If
    // drawDbmScale3D ever reverts to computing its own independent
    // rounding (or drops rounding entirely) while writeDssMeshUbo keeps
    // calling dssRoundedSpanDb(), the two would generally still LOOK
    // similar but no longer be governed by the same source of truth --
    // this test catches that divergence at the drawDbmScale3D end because
    // it is the only end a unit test can reach.
    void render3D_matchesTheRoundedSpanNotTheRawSpan() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        // Not a 0.5 dB multiple, so a caller that forgot to round would
        // disagree with dssRoundedSpanDb() by a measurable, non-tiny 0.25 dB.
        w.setDbmRange(-140.0f, -106.75f);
        w.setMeasuredNoiseFloorForTest(-120.0f);
        const float floorDbm = w.dssFloorDbm();
        const float rounded = w.dssRoundedSpanDb();
        QVERIFY2(std::abs(rounded - w.dssSpanDb()) > 0.01f,
                 "precondition: rounding actually changes the value for this input");

        const QRect rect(0, 0, 200, 280);

        QImage actual(200, 300, QImage::Format_ARGB32_Premultiplied);
        actual.fill(Qt::transparent);
        {
            QPainter p(&actual);
            w.drawDbmScale3DForTest(p, rect, floorDbm);
        }

        // Independently reconstructed via the SAME two public pieces
        // drawDbmScale3D itself is built from (chrome, then labels at
        // floorDbm+span/span), fed the rounded span read through the public
        // accessor rather than duplicating drawDbmScale3D's own formula.
        QImage expected(200, 300, QImage::Format_ARGB32_Premultiplied);
        expected.fill(Qt::transparent);
        {
            QPainter p(&expected);
            w.drawDbmScaleChromeForTest(p, rect);
            w.drawDbmScaleLabelsForTest(p, rect, floorDbm + rounded, rounded);
        }

        QCOMPARE(actual, expected);
    }

    void drawDbmScale3D_paintsWithoutCrashing() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-140.0f, -40.0f);
        w.setMeasuredNoiseFloorForTest(-120.0f);
        QImage canvas(200, 300, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter p(&canvas);
        w.drawDbmScale3DForTest(p, QRect(0, 0, 200, 280), w.dssFloorDbm());
        p.end();
        // Something was drawn into the strip.
        bool anyInk = false;
        for (int y = 0; y < canvas.height() && !anyInk; ++y) {
            for (int x = 0; x < canvas.width(); ++x) {
                if (qAlpha(canvas.pixel(x, y)) != 0) { anyInk = true; break; }
            }
        }
        QVERIFY(anyInk);
    }

    void degenerateRect_isRefusedNotDivided() {
        SpectrumWidget w;
        QImage canvas(10, 10, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&canvas);
        w.drawDbmScale3DForTest(p, QRect(0, 0, 0, 0), -140.0f);   // must not crash
        p.end();
        QVERIFY(true);
    }

    // drawDbmScaleLabels' `rangeDb <= 0.0f` guard is unreachable through
    // drawDbmScale3DForTest() alone: dssSpanDb() floors at 1.0f (Task 9's
    // SpectrumWidget::dssSpanDb()), so drawDbmScale3D() can never compute a
    // non-positive span in production -- the guard is faithful, ported
    // defense-in-depth, not currently reachable from that entry point.
    // Exercises it directly via drawDbmScaleLabelsForTest so the degenerate-
    // span requirement is proven rather than argued. topDbm=0/rangeDb=0 is
    // chosen deliberately: firstLabel collapses to exactly 0, so WITHOUT the
    // guard the tick loop would run one iteration and compute an exact 0/0
    // float division (frac = (topDbm-dbm)/rangeDb) -- the guard must return
    // before that division is ever reached.
    void drawDbmScaleLabels_guardsNonPositiveSpan() {
        SpectrumWidget w;
        QImage canvas(200, 300, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        {
            QPainter p(&canvas);
            w.drawDbmScaleLabelsForTest(p, QRect(0, 0, 200, 280), -140.0f, 0.0f);
            w.drawDbmScaleLabelsForTest(p, QRect(0, 0, 200, 280), 0.0f, 0.0f);
            w.drawDbmScaleLabelsForTest(p, QRect(0, 0, 200, 280), -140.0f, -10.0f);
        }
        // The guard returns before any drawing call, so nothing lands on
        // the canvas -- a positive, checkable assertion beyond "did not
        // crash", and one a deleted guard would very likely flip (the
        // no-guard bottom-label fallback draws unconditionally whenever
        // bottomY >= labelTop, which is true for this rect).
        bool anyInk = false;
        for (int y = 0; y < canvas.height() && !anyInk; ++y) {
            for (int x = 0; x < canvas.width(); ++x) {
                if (qAlpha(canvas.pixel(x, y)) != 0) { anyInk = true; break; }
            }
        }
        QVERIFY2(!anyInk, "rangeDb <= 0 must draw nothing, not just avoid crashing");
    }
};

QTEST_MAIN(TestDssDbmScale)
#include "tst_dss_dbm_scale.moc"
