// tst_setup_helpers.cpp
//
// Phase 3G-9a — unit smoke for SetupHelpers factory functions. Verifies
// bidirectional slider↔spinbox sync without signal feedback loops and that
// returned widgets match the requested range/initial/suffix.

#include <QtTest/QtTest>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSignalSpy>

#include "gui/setup/SetupHelpers.h"

using namespace NereusSDR;

class TestSetupHelpers : public QObject {
    Q_OBJECT
private slots:

    // --- makeSliderRow (int) -------------------------------------------------

    void sliderRow_honorsRangeAndInitial()
    {
        SliderRow row = makeSliderRow(10, 60, 30, QStringLiteral(" fps"));
        QVERIFY(row.slider);
        QVERIFY(row.spin);
        QVERIFY(row.container);

        QCOMPARE(row.slider->minimum(), 10);
        QCOMPARE(row.slider->maximum(), 60);
        QCOMPARE(row.slider->value(),   30);

        QCOMPARE(row.spin->minimum(), 10);
        QCOMPARE(row.spin->maximum(), 60);
        QCOMPARE(row.spin->value(),   30);
        QCOMPARE(row.spin->suffix(),  QStringLiteral(" fps"));

        delete row.container;
    }

    void sliderRow_sliderDragUpdatesSpinbox()
    {
        SliderRow row = makeSliderRow(0, 100, 50);
        row.slider->setValue(77);
        QCOMPARE(row.spin->value(), 77);
        delete row.container;
    }

    void sliderRow_spinboxEntryUpdatesSlider()
    {
        SliderRow row = makeSliderRow(0, 100, 50);
        row.spin->setValue(22);
        QCOMPARE(row.slider->value(), 22);
        delete row.container;
    }

    void sliderRow_noFeedbackLoop()
    {
        // Dragging the slider must emit slider->valueChanged exactly once,
        // not bounce back through the spinbox and emit again.
        SliderRow row = makeSliderRow(0, 100, 50);
        QSignalSpy spy(row.slider, &QSlider::valueChanged);
        row.slider->setValue(60);
        QCOMPARE(spy.count(), 1);

        // And typing into the spinbox must emit spin->valueChanged exactly once.
        QSignalSpy spinSpy(row.spin, qOverload<int>(&QSpinBox::valueChanged));
        row.spin->setValue(70);
        QCOMPARE(spinSpy.count(), 1);

        delete row.container;
    }

    // --- makeDoubleSliderRow -------------------------------------------------

    void doubleSliderRow_honorsRangeAndDecimals()
    {
        SliderRowD row = makeDoubleSliderRow(-30.0, 30.0, 0.0, 1, QStringLiteral(" dBm"));
        QVERIFY(row.slider);
        QVERIFY(row.spin);

        QCOMPARE(row.spin->minimum(),  -30.0);
        QCOMPARE(row.spin->maximum(),   30.0);
        QCOMPARE(row.spin->value(),      0.0);
        QCOMPARE(row.spin->decimals(),   1);
        QCOMPARE(row.spin->suffix(),   QStringLiteral(" dBm"));

        // Slider granularity is 10^decimals = 10 units per unit.
        QCOMPARE(row.slider->minimum(), -300);
        QCOMPARE(row.slider->maximum(),  300);
        QCOMPARE(row.slider->value(),     0);

        delete row.container;
    }

    void doubleSliderRow_bidirectionalSync()
    {
        SliderRowD row = makeDoubleSliderRow(-30.0, 30.0, 0.0, 1);
        row.spin->setValue(12.5);
        QCOMPARE(row.slider->value(), 125);

        row.slider->setValue(-75);
        QCOMPARE(row.spin->value(), -7.5);

        delete row.container;
    }
};

QTEST_MAIN(TestSetupHelpers)
#include "tst_setup_helpers.moc"
