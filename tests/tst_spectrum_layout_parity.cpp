// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd / KG4VCF
// no-port-check: NereusSDR-original behavioral regressions for issues #141/#147.

#include <QtTest/QtTest>
#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QWheelEvent>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

void sendMouse(SpectrumWidget& widget, QEvent::Type type, QPoint point,
               Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(point), widget.mapToGlobal(QPointF(point)),
                      button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
}

void sendWheel(SpectrumWidget& widget, QPoint point)
{
    QWheelEvent event(QPointF(point), widget.mapToGlobal(QPointF(point)),
                      QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &event);
}

} // namespace

class TestSpectrumLayoutParity final : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    void spectrumBoundaryReservesChrome_data()
    {
        QTest::addColumn<int>("height");
        QTest::addColumn<int>("spectrumBottom");
        // The existing GPU layout reserves a 4px divider and 28px frequency
        // bar before splitting the remaining height 40% / 60%.
        QTest::newRow("400px") << 400 << 147;
        QTest::newRow("500px") << 500 << 187;
        QTest::newRow("700px") << 700 << 267;
    }

    void spectrumBoundaryReservesChrome()
    {
        QFETCH(int, height);
        QFETCH(int, spectrumBottom);
        SpectrumWidget widget;
        widget.resize(800, height);
        // This is the actual production rectangle used to paint and hit-test
        // notch markers, not a parallel implementation of the layout helper.
        QCOMPARE(widget.notchSpecRectForTest().height(), spectrumBottom);
    }

    void dividerFollowsThePointer()
    {
        SpectrumWidget widget;
        widget.resize(800, 400);
        widget.setConnectionState(ConnectionState::Connected);
        for (int target : {74, 220, 330}) {
            const int current = widget.notchSpecRectForTest().height();
            sendMouse(widget, QEvent::MouseButtonPress, QPoint(200, current),
                      Qt::LeftButton, Qt::LeftButton);
            sendMouse(widget, QEvent::MouseMove, QPoint(200, target),
                      Qt::NoButton, Qt::LeftButton);
            sendMouse(widget, QEvent::MouseButtonRelease, QPoint(200, target),
                      Qt::LeftButton, Qt::NoButton);
            // Up to one pixel of float-to-int rounding is allowed. Dividing
            // pointer Y by the entire widget height moves the GPU divider
            // tens of pixels away from the pointer on this fixture.
            QVERIFY2(qAbs(widget.notchSpecRectForTest().height() - target) <= 1,
                     "The rendered spectrum boundary did not follow divider drag");
        }
    }

    void dbmWheelStopsAtTheSpectrumBoundary()
    {
        SpectrumWidget widget;
        widget.resize(800, 400);
        widget.setConnectionState(ConnectionState::Connected);
        widget.setDbmScaleVisible(true);
        QSignalSpy changed(&widget, &SpectrumWidget::dbmRangeChangeRequested);
        const float before = widget.dynamicRange();
        const int stripX = widget.width() - 18;
        // y=155 lies below the canonical 147px spectrum, but above the old
        // full-widget-height boundary (160). It belongs to frequency chrome,
        // and scrolling there must not adjust the spectrum's amplitude range.
        sendWheel(widget, QPoint(stripX, 155));
        QCOMPARE(changed.count(), 0);
        QCOMPARE(widget.dynamicRange(), before);
        sendWheel(widget, QPoint(stripX, 146));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(widget.dynamicRange(), before - 5.0f);
    }

    void cpuPaintPlacesFrequencyBarBetweenPanels()
    {
#ifdef NEREUS_GPU_SPECTRUM
        QSKIP("QWidget paint output is checked in the CPU build");
#else
        SpectrumWidget widget;
        widget.resize(800, 400);
        widget.setConnectionState(ConnectionState::Connected);
        widget.setDbmScaleVisible(false);
        widget.setFreqLabelAlign(FreqLabelAlign::Off);
        QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::magenta);
        widget.render(&image);
        const QColor frequencyBackground(0x10, 0x15, 0x20);
        // Read actual paintEvent output away from marker, label and LIVE
        // chrome. The bar starts at 147 + 4 and ends just before y=179.
        QCOMPARE(image.pixelColor(200, 153), frequencyBackground);
        QCOMPARE(image.pixelColor(200, 176), frequencyBackground);
        QVERIFY(image.pixelColor(200, 390) != frequencyBackground);
#endif
    }
};

QTEST_MAIN(TestSpectrumLayoutParity)
#include "tst_spectrum_layout_parity.moc"
