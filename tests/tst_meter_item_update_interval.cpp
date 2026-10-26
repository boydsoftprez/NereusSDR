// no-port-check: NereusSDR-original tests for the per-item update interval.
// Modification history (NereusSDR):
//   2026-10-25 - Per-item update interval: throttle under a fake clock,
//                 saved entries without the key, new-entry default, bar
//                 presets untouched, editor range. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QCheckBox>
#include <QJsonObject>
#include <QSpinBox>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/BaseItemEditor.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"

using namespace NereusSDR;

namespace {

// Records the fake-clock time of every reading it takes.
class CountingItem final : public MeterItem {
public:
    explicit CountingItem(const qint64* clock) : m_clock(clock) {}
    void setValue(double v) override { MeterItem::setValue(v); taken.append(*m_clock); }
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter&, int, int) override {}
    QVector<qint64> taken;
private:
    const qint64* m_clock;
};

// Pushes a changing SignalPeak reading every 50 ms from 0 to 1950 ms.
void pushEvery50Ms(MeterWidget& widget, qint64& clock)
{
    for (clock = 0; clock < 2000; clock += 50) {
        widget.updateMeterValue(MeterBinding::SignalPeak, -100.0 + double(clock) / 50.0);
    }
}

} // namespace

class TestMeterItemUpdateInterval : public QObject {
    Q_OBJECT
private slots:
    void throttlesToInterval()
    {
        qint64 clock = 0;
        MeterWidget widget;
        widget.setMonotonicClockForTest([&clock]() { return clock; });
        auto* item = new CountingItem(&clock);
        item->setBindingId(MeterBinding::SignalPeak);
        item->setUpdateIntervalMs(500);
        widget.addItem(item);
        pushEvery50Ms(widget, clock);
        QCOMPARE(item->taken, (QVector<qint64>{0, 500, 1000, 1500}));
        for (int i = 1; i < item->taken.size(); ++i) {
            QVERIFY(item->taken[i] - item->taken[i - 1] >= 500);
        }
    }

    void zeroTakesEveryPoll()
    {
        qint64 clock = 0;
        MeterWidget widget;
        widget.setMonotonicClockForTest([&clock]() { return clock; });
        auto* item = new CountingItem(&clock);
        item->setBindingId(MeterBinding::SignalPeak);
        QCOMPARE(item->updateIntervalMs(), 0);
        widget.addItem(item);
        pushEvery50Ms(widget, clock);
        QCOMPARE(item->taken.size(), 40);
    }

    void acceptsUpdateContract()
    {
        CountingItem item(nullptr);
        QVERIFY(item.acceptsUpdate(0));
        QVERIFY(item.acceptsUpdate(1));
        item.setUpdateIntervalMs(100);
        QVERIFY(item.acceptsUpdate(1000));
        QVERIFY(!item.acceptsUpdate(1099));
        QVERIFY(item.acceptsUpdate(1100));
        item.setUpdateIntervalMs(-5);
        QCOMPARE(item.updateIntervalMs(), 0);
    }

    void savedEntryWithoutKeyTakesEveryPoll()
    {
        ContainerContentRegistry registry;
        ContentEntry entry;
        entry.id = QStringLiteral("saved-bar");
        entry.typeId = QStringLiteral("BAR");
        entry.name = QStringLiteral("Bar");
        entry.config.insert(QStringLiteral("legacyRecord"), BarItem().serialize());
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        QVERIFY(item != nullptr);
        QCOMPARE(item->updateIntervalMs(), 0);
        const ContentEntry captured = registry.captureMeterItem(*item);
        QVERIFY(!captured.context.contains(QStringLiteral("updateIntervalMs")));
    }

    void newBuildingBlockTakesDefault()
    {
        QCOMPARE(kDefaultUpdateIntervalMs, 100);
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("BAR"));
        QCOMPARE(entry.context.value(QStringLiteral("updateIntervalMs")).toInt(), 100);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        QVERIFY(item != nullptr);
        QCOMPARE(item->updateIntervalMs(), 100);
        item->setUpdateIntervalMs(250);
        ContentEntry captured = registry.captureMeterItem(*item);
        QCOMPARE(captured.context.value(QStringLiteral("updateIntervalMs")).toInt(), 250);
        item->setUpdateIntervalMs(0);
        captured = registry.captureMeterItem(*item, captured);
        QVERIFY(!captured.context.contains(QStringLiteral("updateIntervalMs")));
    }

    void barPresetUnchanged()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("meter.signal"));
        QVERIFY(!entry.context.contains(QStringLiteral("updateIntervalMs")));
        std::unique_ptr<MeterItem> created(registry.createMeterItem(entry, nullptr));
        QVERIFY(qobject_cast<BarPresetItem*>(created.get()) != nullptr);
        QCOMPARE(created->updateIntervalMs(), 0);
        QVERIFY(!registry.captureMeterItem(*created).context.contains(QStringLiteral("updateIntervalMs")));

        // Not throttled by the widget even with an interval set on it.
        qint64 clock = 0;
        MeterWidget widget;
        widget.setMonotonicClockForTest([&clock]() { return clock; });
        auto* face = new BarPresetItem;
        QVERIFY(face->configureVariant(QStringLiteral("Signal")));
        face->setUpdateIntervalMs(500);
        widget.addItem(face);
        BarPresetItem twin;
        QVERIFY(twin.configureVariant(QStringLiteral("Signal")));
        for (clock = 0; clock < 2000; clock += 50) {
            const double v = -100.0 + double(clock) / 50.0;
            widget.updateMeterValue(MeterBinding::SignalPeak, v);
            twin.pushBindingValue(MeterBinding::SignalPeak, v);
            QCOMPARE(face->primaryValue(), twin.primaryValue());
        }
    }

    void editorClampsAndUnchecks()
    {
        QCOMPARE(kMinUpdateIntervalMs, 15);
        QCOMPARE(kMaxUpdateIntervalMs, 5000);
        BaseItemEditor editor;
        BarItem bar;
        editor.setItem(&bar);
        auto* check = editor.findChild<QCheckBox*>(QStringLiteral("limitUpdateRate"));
        auto* spin = editor.findChild<QSpinBox*>(QStringLiteral("updateIntervalMs"));
        QVERIFY(check && spin);
        QCOMPARE(spin->minimum(), 15);
        QCOMPARE(spin->maximum(), 5000);
        QVERIFY(!check->isChecked());
        QVERIFY(!spin->isEnabled());
        check->setChecked(true);
        QVERIFY(spin->isEnabled());
        QCOMPARE(bar.updateIntervalMs(), kDefaultUpdateIntervalMs);
        spin->setValue(1);
        QCOMPARE(spin->value(), 15);
        QCOMPARE(bar.updateIntervalMs(), 15);
        spin->setValue(99999);
        QCOMPARE(spin->value(), 5000);
        QCOMPARE(bar.updateIntervalMs(), 5000);
        check->setChecked(false);
        QVERIFY(!spin->isEnabled());
        QCOMPARE(bar.updateIntervalMs(), 0);
        ContainerContentRegistry registry;
        QVERIFY(!registry.captureMeterItem(bar, registry.makeEntry(QStringLiteral("BAR")))
                     .context.contains(QStringLiteral("updateIntervalMs")));
    }
};

QTEST_MAIN(TestMeterItemUpdateInterval)
#include "tst_meter_item_update_interval.moc"
