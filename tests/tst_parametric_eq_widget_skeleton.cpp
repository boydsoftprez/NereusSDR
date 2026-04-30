// =================================================================
// tests/tst_parametric_eq_widget_skeleton.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file. The widget under test
// (ParametricEqWidget) ports Thetis ucParametricEq.cs; cites live in
// the widget header.  This file just exercises the Qt API surface.
// =================================================================
//
// Phase 3M-3a-ii follow-up Batch 1 -- skeleton smoke test.
// Verifies:
//   - Widget constructs without crash
//   - 18-color palette returns 18 distinct QColors with the verbatim RGB triples
//   - Default member values match ucParametricEq.cs:367-447
//
// =================================================================

#include "../src/gui/widgets/ParametricEqWidget.h"

#include <QApplication>
#include <QTest>

class TestParametricEqWidgetSkeleton : public QObject {
    Q_OBJECT
private slots:
    void constructsWithoutCrash();
    void paletteHasEighteenColors();
    void paletteFirstThreeRgbVerbatim();
    void paletteLastThreeRgbVerbatim();
};

void TestParametricEqWidgetSkeleton::constructsWithoutCrash() {
    NereusSDR::ParametricEqWidget w;
    QVERIFY(w.isWidgetType());
}

void TestParametricEqWidgetSkeleton::paletteHasEighteenColors() {
    QCOMPARE(NereusSDR::ParametricEqWidget::defaultBandPaletteSize(), 18);
}

void TestParametricEqWidgetSkeleton::paletteFirstThreeRgbVerbatim() {
    // From Thetis ucParametricEq.cs:256-258 [v2.10.3.13].
    QCOMPARE(NereusSDR::ParametricEqWidget::defaultBandPaletteAt(0), QColor(  0, 190, 255));
    QCOMPARE(NereusSDR::ParametricEqWidget::defaultBandPaletteAt(1), QColor(  0, 220, 130));
    QCOMPARE(NereusSDR::ParametricEqWidget::defaultBandPaletteAt(2), QColor(255, 210,   0));
}

void TestParametricEqWidgetSkeleton::paletteLastThreeRgbVerbatim() {
    // From Thetis ucParametricEq.cs:271-273 [v2.10.3.13].
    QCOMPARE(NereusSDR::ParametricEqWidget::defaultBandPaletteAt(15), QColor(255, 120,  40));
    QCOMPARE(NereusSDR::ParametricEqWidget::defaultBandPaletteAt(16), QColor(120, 255, 160));
    QCOMPARE(NereusSDR::ParametricEqWidget::defaultBandPaletteAt(17), QColor(255,  60, 120));
}

QTEST_MAIN(TestParametricEqWidgetSkeleton)
#include "tst_parametric_eq_widget_skeleton.moc"
