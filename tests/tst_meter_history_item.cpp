// no-port-check: NereusSDR-original tests for the HISTORY building block.
// Modification history (NereusSDR):
//   2026-10-26 - HISTORY building block: axis 1 is fed through
//                 MeterWidget::updateMeterValue and data source variable 2,
//                 each axis keeps the update interval, manual min/max with
//                 auto scale off, v3 record round trip, older records keep
//                 their look, new items take the clsHistoryItem values.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QUuid>
#include <cmath>
#include <limits>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

namespace {

// A v2 record (17 fields) as saved before the v3 options: axis 0 auto
// scale off, axis 1 bound.
const QString kV2Record = QStringLiteral(
    "HISTORY|0|0|1|1|0|0|301|#ff00b4d8|#ffffb800|1|0|1|1|0|1|30000");

HistoryGraphItem* addHistory(MeterWidget& widget, int binding0, int binding1, int intervalMs)
{
    auto item = std::make_unique<HistoryGraphItem>();
    item->setBindingId(binding0);
    item->setBindingId1(binding1);
    item->setUpdateIntervalMs(intervalMs);
    HistoryGraphItem* raw = item.get();
    widget.addItem(item.release());
    return raw;
}

QColor pixelAt(HistoryGraphItem& item, int x, int y)
{
    QImage image(200, 100, QImage::Format_ARGB32);
    image.fill(Qt::magenta);
    QPainter p(&image);
    item.paint(p, image.width(), image.height());
    p.end();
    return image.pixelColor(x, y);
}

} // namespace

class TestMeterHistoryItem : public QObject {
    Q_OBJECT
private slots:
    // The bug: axis 1 was never fed from MeterWidget::updateMeterValue.
    void axisOneIsFedFromUpdateMeterValue()
    {
        qint64 clock = 0;
        MeterWidget widget;
        widget.setMonotonicClockForTest([&clock]() { return clock; });
        HistoryGraphItem* history = addHistory(widget, MeterBinding::SignalPeak,
                                               MeterBinding::SignalAvg, 0);
        QCOMPARE(history->readingBindings(),
                 (QSet<int>{MeterBinding::SignalPeak, MeterBinding::SignalAvg}));
        widget.updateMeterValue(MeterBinding::SignalPeak, -73.0);
        widget.updateMeterValue(MeterBinding::SignalAvg, -80.0);
        QCOMPARE(history->sampleCount0(), 1);
        QCOMPARE(history->sampleCount1(), 1);
    }

    // Both traces record under the update interval: one delivery per
    // reading per poll, each axis on its own clock.
    void bothAxesRecordUnderTheUpdateInterval()
    {
        qint64 clock = 0;
        MeterWidget widget;
        widget.setMonotonicClockForTest([&clock]() { return clock; });
        HistoryGraphItem* history = addHistory(widget, MeterBinding::SignalPeak,
                                               MeterBinding::SignalAvg, 100);
        for (clock = 0; clock < 1000; clock += 50) {
            widget.updateMeterValue(MeterBinding::SignalPeak, -73.0 - double(clock) / 100.0);
            widget.updateMeterValue(MeterBinding::SignalAvg, -80.0 - double(clock) / 100.0);
        }
        // 0, 100, ... 900 ms: ten samples each.
        QCOMPARE(history->sampleCount0(), 10);
        QCOMPARE(history->sampleCount1(), 10);
    }

    // Thetis's new item reads SIGNAL_STRENGTH on both axes: one reading
    // feeds both traces.
    void oneReadingFeedsBothAxes()
    {
        qint64 clock = 0;
        MeterWidget widget;
        widget.setMonotonicClockForTest([&clock]() { return clock; });
        HistoryGraphItem* history = addHistory(widget, MeterBinding::SignalPeak,
                                               MeterBinding::SignalPeak, 100);
        widget.updateMeterValue(MeterBinding::SignalPeak, -73.0);
        clock = 100;
        widget.updateMeterValue(MeterBinding::SignalPeak, -74.0);
        clock = 150;
        widget.updateMeterValue(MeterBinding::SignalPeak, -75.0);
        QCOMPARE(history->sampleCount0(), 2);
        QCOMPARE(history->sampleCount1(), 2);
    }

    // Data source variable 2 feeds axis 1 and replaces its reading.
    void dataSourceVariableTwoFeedsAxisOne()
    {
        QCOMPARE(MeterItem::mmioVariableCount(QStringLiteral("HISTORY")), 2);
        MeterWidget widget;
        HistoryGraphItem* history = addHistory(widget, MeterBinding::SignalPeak,
                                               MeterBinding::SignalAvg, 0);
        history->setMmioBinding(1, QUuid::createUuid(), QStringLiteral("level"));
        QCOMPARE(history->readingBindings(), (QSet<int>{MeterBinding::SignalPeak}));
        widget.updateMmioValue(history, 1, 12.5);
        widget.updateMeterValue(MeterBinding::SignalAvg, -80.0);
        QCOMPARE(history->sampleCount1(), 1);
        // No reading from the source: nothing plotted.
        widget.updateMmioValue(history, 1, 3.0, QStringLiteral("No reading"));
        history->setSecondaryValue(1, std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(history->sampleCount1(), 1);
    }

    void manualRangeIsUsedWhenAutoScaleIsOff()
    {
        HistoryGraphItem history;
        history.applyNewItemDefaults();
        for (double v : {-73.0, -70.0, -76.0}) {
            history.setValue(v);
            history.setValue1(v);
        }
        float lo = 0.0f, hi = 0.0f;
        history.axisRange(0, lo, hi);
        QVERIFY(lo > -80.0f && hi < -60.0f);  // auto scaled around the samples

        history.setAutoScale0(false);
        history.setManualMin(0, -120.0f);
        history.setManualMax(0, -20.0f);
        history.axisRange(0, lo, hi);
        QCOMPARE(lo, -120.0f);
        QCOMPARE(hi, -20.0f);

        // Axis 1 keeps auto scale until its own switch is off.
        history.axisRange(1, lo, hi);
        QVERIFY(lo > -80.0f && hi < -60.0f);
        history.setAutoScale1(false);
        history.axisRange(1, lo, hi);
        QCOMPARE(lo, -150.0f);
        QCOMPARE(hi, 0.0f);

        // Designer ranges clamp.
        history.setManualMin(1, -50000.0f);
        history.setManualMax(1, 50000.0f);
        QCOMPARE(history.manualMin(1), kMinHistoryAxisMin);
        QCOMPARE(history.manualMax(1), kMaxHistoryAxisMax);
        history.setVerticalRatio(0.01f);
        QCOMPARE(history.verticalRatio(), kMinHistoryVerticalRatio);
    }

    void optionsRoundTrip()
    {
        HistoryGraphItem saved;
        saved.setVerticalRatio(0.25f);
        saved.setBackColour(QColor(0x11, 0x22, 0x33));
        saved.setLinesColour(QColor(0x44, 0x55, 0x66));
        saved.setTimeColour(QColor(0x77, 0x88, 0x99));
        saved.setManualMin(0, -140.5f);
        saved.setManualMax(0, -10.0f);
        saved.setManualMin(1, 0.0f);
        saved.setManualMax(1, 100.0f);
        const QString record = saved.serialize();
        QCOMPARE(record.split(QLatin1Char('|')).size(), 25);

        HistoryGraphItem loaded;
        QVERIFY(loaded.deserialize(record));
        QCOMPARE(loaded.verticalRatio(), 0.25f);
        QCOMPARE(loaded.backColour(), QColor(0x11, 0x22, 0x33));
        QCOMPARE(loaded.linesColour(), QColor(0x44, 0x55, 0x66));
        QCOMPARE(loaded.timeColour(), QColor(0x77, 0x88, 0x99));
        QVERIFY(loaded.hasManualRange());
        QCOMPARE(loaded.manualMin(0), -140.5f);
        QCOMPARE(loaded.manualMax(0), -10.0f);
        QCOMPARE(loaded.manualMin(1), 0.0f);
        QCOMPARE(loaded.manualMax(1), 100.0f);
        // The v3 fields come back as written. (Capacity follows the
        // duration on load, so the whole record is compared from there.)
        QCOMPARE(loaded.serialize().split(QLatin1Char('|')).mid(17),
                 record.split(QLatin1Char('|')).mid(17));
    }

    // An older 17-field record loads with the older look, and keeps it when
    // saved again.
    void olderRecordKeepsItsLook()
    {
        HistoryGraphItem history;
        QVERIFY(history.deserialize(kV2Record));
        QCOMPARE(history.durationMs(), 30000);
        QCOMPARE(history.bindingId1(), 1);
        QVERIFY(!history.autoScale0());
        QVERIFY(!history.hasManualRange());
        QCOMPARE(history.backColour(), QColor(0x0a, 0x0a, 0x18));
        QCOMPARE(history.linesColour(), QColor(0x20, 0x30, 0x40));
        QCOMPARE(history.timeColour(), QColor(0x80, 0x90, 0xa0));
        QCOMPARE(history.lineColor0(), QColor(0x00, 0xb4, 0xd8));
        QCOMPARE(history.updateIntervalMs(), 0);

        // Background and border pixels (before any trace is drawn).
        QCOMPARE(pixelAt(history, 100, 50), QColor(0x0a, 0x0a, 0x18));
        QCOMPARE(pixelAt(history, 0, 50), QColor(0x20, 0x30, 0x40));

        // Auto scale off without a manual range still auto ranges, as
        // these records always drew.
        history.setValue(-73.0);
        history.setValue(-70.0);
        float lo = 0.0f, hi = 0.0f;
        history.axisRange(0, lo, hi);
        QVERIFY(lo > -80.0f && hi < -60.0f);

        HistoryGraphItem again;
        QVERIFY(again.deserialize(history.serialize()));
        QVERIFY(!again.hasManualRange());
        QCOMPARE(again.backColour(), QColor(0x0a, 0x0a, 0x18));
        QCOMPARE(again.linesColour(), QColor(0x20, 0x30, 0x40));
        QCOMPARE(again.timeColour(), QColor(0x80, 0x90, 0xa0));
    }

    void newItemsTakeThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("HISTORY"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* history = qobject_cast<HistoryGraphItem*>(item.get());
        QVERIFY(history != nullptr);
        QCOMPARE(history->verticalRatio(), 0.5f);
        QCOMPARE(history->backColour(), QColor(Qt::black));
        QCOMPARE(history->lineColor0(), QColor(Qt::red));
        QCOMPARE(history->lineColor1(), QColor(Qt::yellow));
        QCOMPARE(history->linesColour(), QColor(Qt::white));
        QCOMPARE(history->timeColour(), QColor(0x80, 0x80, 0x80));
        QCOMPARE(history->bindingId(), MeterBinding::SignalPeak);
        QCOMPARE(history->bindingId1(), MeterBinding::SignalPeak);
        QCOMPARE(history->durationMs(), 60000);
        QVERIFY(history->hasManualRange());
        QCOMPARE(history->manualMin(0), -150.0f);
        QCOMPARE(history->manualMax(0), 0.0f);
        QCOMPARE(history->manualMin(1), -150.0f);
        QCOMPARE(history->manualMax(1), 0.0f);
        QVERIFY(history->autoScale0());
        QVERIFY(history->autoScale1());
        QVERIFY(!history->showScale1());
        QCOMPARE(history->updateIntervalMs(), 100);
    }
};

QTEST_MAIN(TestMeterHistoryItem)
#include "tst_meter_history_item.moc"
