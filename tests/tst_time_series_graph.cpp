// =================================================================
// tests/tst_time_series_graph.cpp  (NereusSDR)
// =================================================================
// R-R3-32/33 regression: the production graph path must preserve explicit
// telemetry discontinuities before its existing pixel-column downsampling.
// =================================================================

#include <QtTest/QtTest>

#include "gui/TimeSeriesGraphWidget.h"

using namespace NereusSDR;

class TstTimeSeriesGraph : public QObject {
    Q_OBJECT

private slots:
    void explicitBreakInSamePixelColumnCreatesSeparateSubpaths()
    {
        TimeSeriesGraphWidget::Series series;
        series.points = {{1.0, 1.0}, {1.1, 2.0}, {1.2, 3.0}, {2.0, 4.0}};
        series.breakBefore = {false, false, true, false};
        // The last point before and first after the break deliberately round
        // to one pixel. Each side has a line segment, so Qt cannot discard a
        // move-only subpath while still exposing a forbidden cross-break line.
        const QPainterPath path = TimeSeriesGraphWidget::buildLinePath(
            series, {{10.10, 20.0}, {11.00, 30.0}, {11.20, 40.0}, {12.00, 50.0}});

        QCOMPARE(path.elementCount(), 4);
        QVERIFY(path.elementAt(0).type == QPainterPath::MoveToElement);
        QVERIFY(path.elementAt(1).type == QPainterPath::LineToElement);
        QVERIFY(path.elementAt(2).type == QPainterPath::MoveToElement);
        QVERIFY(path.elementAt(3).type == QPainterPath::LineToElement);
        QVERIFY(qAbs(path.elementAt(1).x - 11.00) < 0.001);
        QVERIFY(qAbs(path.elementAt(2).x - 11.20) < 0.001);
    }

    void existingMaximumGapAlsoCreatesSeparateSubpaths()
    {
        TimeSeriesGraphWidget::Series series;
        series.points = {{1.0, 1.0}, {2.0, 2.0}, {6.0, 3.0}, {7.0, 4.0}};
        series.maxConnectGapSeconds = 3.0;
        const QPainterPath path = TimeSeriesGraphWidget::buildLinePath(
            series, {{10.0, 20.0}, {11.0, 30.0}, {11.2, 40.0}, {12.0, 50.0}});

        QCOMPARE(path.elementCount(), 4);
        QVERIFY(path.elementAt(0).type == QPainterPath::MoveToElement);
        QVERIFY(path.elementAt(1).type == QPainterPath::LineToElement);
        QVERIFY(path.elementAt(2).type == QPainterPath::MoveToElement);
        QVERIFY(path.elementAt(3).type == QPainterPath::LineToElement);
    }
};

QTEST_GUILESS_MAIN(TstTimeSeriesGraph)
#include "tst_time_series_graph.moc"
