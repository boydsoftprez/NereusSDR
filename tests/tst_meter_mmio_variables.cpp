// no-port-check: NereusSDR-original test; it checks values cited from Thetis.
// =================================================================
// tests/tst_meter_mmio_variables.cpp  (NereusSDR)
// =================================================================
//
// Several data source (MMIO) variables per container item, after Thetis
// MeterVariables: the per-type count, the second variable driving the AGC
// bar's average marker through the widget and the poller, the
// mmioVariables context key (absent when unused, round-tripped through
// capture, restore and the portable codec) and the editor's row pairs.
//
// Modification history (NereusSDR):
//   2026-10-26 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest>
#include <QJsonArray>
#include <QLineEdit>
#include <QSignalSpy>

#include "core/mmio/MmioEndpoint.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"

#include <cmath>
#include <memory>

using namespace NereusSDR;

namespace {

int dataSourceRows(const ContentPropertyEditor& editor)
{
    int rows = 0;
    for (const QLineEdit* edit : editor.findChildren<QLineEdit*>()) {
        if (edit->objectName().startsWith(QLatin1String("mmioGuid"))) { ++rows; }
    }
    return rows;
}

// Advance the item's meter until its smoothing settles.
void settle(BarPresetItem& bar, qint64& now)
{
    for (int i = 0; i < 200; ++i) { now += 100; bar.advanceMeter(now); }
}

} // namespace

class TstMeterMmioVariables : public QObject {
    Q_OBJECT
private slots:
    void variableCountFollowsThetisMeterVariables()
    {
        // From Thetis MeterManager.cs:22684-22738 [v2.10.3.15].
        for (const char* type : {"meter.adc", "meter.agc", "meter.mic", "meter.alc", "meter.eq",
                                 "meter.leveler", "meter.comp", "meter.cfc", "meter.crossNeedle",
                                 "meter.historyGraph", "meter.customBar"}) {
            QCOMPARE(MeterItem::mmioVariableCount(QLatin1String(type)), 2);
        }
        QCOMPARE(MeterItem::mmioVariableCount(QStringLiteral("meter.ananMulti")), 7);
        QCOMPARE(MeterItem::mmioVariableCount(QStringLiteral("meter.signal")), 1);
        QCOMPARE(MeterItem::mmioVariableCount(QStringLiteral("meter.agcGain")), 1);
        QCOMPARE(MeterItem::mmioVariableCount(QStringLiteral("BAR")), 1);
        QCOMPARE(MeterItem::mmioVariableCount(QStringLiteral("meter.clock")), 0);
    }

    void perIndexBindingsKeepIndexZeroAsTheExistingPair()
    {
        BarPresetItem bar;
        const QUuid first = QUuid::createUuid();
        const QUuid second = QUuid::createUuid();
        bar.setMmioBinding(first, QStringLiteral("alpha"));
        QCOMPARE(bar.mmioGuid(0), first);
        QCOMPARE(bar.mmioVariable(0), QStringLiteral("alpha"));
        bar.setMmioBinding(0, second, QStringLiteral("beta"));
        QCOMPARE(bar.mmioGuid(), second);
        QCOMPARE(bar.mmioVariable(), QStringLiteral("beta"));
        bar.clearMmioBinding();
        QVERIFY(!bar.hasAnyMmioBinding());
        bar.setMmioBinding(1, first, QStringLiteral("gamma"));
        QVERIFY(!bar.hasMmioBinding());
        QVERIFY(bar.hasMmioBinding(1));
        QVERIFY(bar.hasAnyMmioBinding());
        QCOMPARE(bar.mmioSourceKey(1), first.toString(QUuid::WithoutBraces) + QChar(0) + QStringLiteral("gamma"));
        bar.setMmioBinding(1, first, QStringLiteral("--DEFAULT--"));
        QVERIFY(!bar.hasMmioBinding(1));
        // Out of range indexes are ignored.
        bar.setMmioBinding(MeterItem::kMaxMmioVariables, first, QStringLiteral("x"));
        QVERIFY(bar.mmioGuid(MeterItem::kMaxMmioVariables).isNull());
        QVERIFY(bar.mmioVariable(-1).isEmpty());
    }

    void agcSecondVariableDrivesTheAverageMarker()
    {
        ContainerContentRegistry registry;
        const QUuid guid = QUuid::createUuid();
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.agc"));
        entry.context.insert(QStringLiteral("mmioVariables"),
                             QJsonArray{QJsonObject{{"guid", guid.toString(QUuid::WithoutBraces)}, {"variable", "avg"}}});
        MeterWidget widget;
        auto* bar = qobject_cast<BarPresetItem*>(registry.createMeterItem(entry, &widget, ContentRenderMode::Preview));
        QVERIFY(bar);
        QVERIFY(!bar->hasMmioBinding());
        QVERIFY(bar->hasMmioBinding(1));
        widget.addItem(bar);
        qint64 now = 0;
        bar->advanceMeter(now);

        widget.updateMmioValue(bar, 1, -40.0);
        // The radio's own average reading no longer moves the marker.
        widget.updateMeterValue(MeterBinding::AgcAvg, -100.0);
        settle(*bar, now);
        QVERIFY(bar->hasAverageReading());
        QVERIFY2(std::abs(bar->averageValue() - -40.0) < 0.5, qPrintable(QString::number(bar->averageValue())));

        // The main reading still comes from the radio.
        widget.updateMeterValue(MeterBinding::AgcPeak, 10.0);
        settle(*bar, now);
        QVERIFY(bar->hasPrimaryReading());
        QVERIFY2(std::abs(bar->primaryValue() - 10.0) < 0.5, qPrintable(QString::number(bar->primaryValue())));

        // A source without a reading hides the marker.
        widget.updateMmioValue(bar, 1, -400.0, QStringLiteral("External meter source is unavailable."));
        QVERIFY(!bar->hasAverageReading());
    }

    void pollerDeliversEveryVariable()
    {
        MeterPoller poller;
        MmioEndpoint endpoint;
        endpoint.setGuid(QUuid::createUuid());
        const QUuid guid = endpoint.guid();
        const QString prefix = guid.toString(QUuid::WithoutBraces) + QLatin1Char('.');
        endpoint.mergeBatch({{prefix + "avg", -55.0}});
        poller.setMmioEndpointSourceForTest([&](const QUuid& requested) -> MmioEndpoint* {
            return requested == guid ? &endpoint : nullptr;
        });
        MeterWidget widget;
        auto* bar = new BarPresetItem(&widget);
        bar->configureVariant(QStringLiteral("Agc"));
        bar->setMmioBinding(1, guid, QStringLiteral("avg"));
        widget.addItem(bar);
        QSignalSpy feed(&poller, &MeterPoller::mmioReadingUpdated);
        poller.addTarget(&widget);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(feed.count(), 1);
        QCOMPARE(feed.first().at(1).toString(), QStringLiteral("avg"));
        qint64 now = 0;
        settle(*bar, now);
        QVERIFY(bar->hasAverageReading());
        QVERIFY2(std::abs(bar->averageValue() - -55.0) < 0.5, qPrintable(QString::number(bar->averageValue())));

        // A copy of the item in another widget replays the cached reading.
        MeterWidget preview;
        auto* copy = new BarPresetItem(&preview);
        copy->configureVariant(QStringLiteral("Agc"));
        copy->setMmioBinding(1, guid, QStringLiteral("avg"));
        preview.addItem(copy);
        poller.replayReadings(&preview, {});
        settle(*copy, now);
        QVERIFY(copy->hasAverageReading());
        QVERIFY2(std::abs(copy->averageValue() - -55.0) < 0.5, qPrintable(QString::number(copy->averageValue())));
    }

    void entryWithoutTheKeyBehavesAsToday()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.agc"));
        QVERIFY(!entry.context.contains(QStringLiteral("mmioVariables")));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr, ContentRenderMode::Preview));
        QVERIFY(item);
        QVERIFY(!item->hasAnyMmioBinding());
        for (int index = 1; index < MeterItem::kMaxMmioVariables; ++index) {
            QVERIFY(item->mmioGuid(index).isNull());
            QVERIFY(item->mmioVariable(index).isEmpty());
        }
        const ContentEntry captured = registry.captureMeterItem(*item, entry);
        QVERIFY(!captured.context.contains(QStringLiteral("mmioVariables")));

        // Clearing the only extra variable removes the key again.
        item->setMmioBinding(1, QUuid::createUuid(), QStringLiteral("avg"));
        ContentEntry withKey = registry.captureMeterItem(*item, entry);
        QVERIFY(withKey.context.contains(QStringLiteral("mmioVariables")));
        item->setMmioBinding(1, QUuid(), QString());
        QVERIFY(!registry.captureMeterItem(*item, withKey).context.contains(QStringLiteral("mmioVariables")));
    }

    void indexOneRoundTripsThroughCaptureAndRestore()
    {
        ContainerContentRegistry registry;
        const QUuid second = QUuid::createUuid();
        const QUuid fourth = QUuid::createUuid();
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.ananMulti"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr, ContentRenderMode::Preview));
        QVERIFY(item);
        item->setMmioBinding(1, second, QStringLiteral("beta"));
        item->setMmioBinding(3, fourth, QStringLiteral("delta"));
        const ContentEntry captured = registry.captureMeterItem(*item, entry);
        const QJsonArray variables = captured.context.value(QStringLiteral("mmioVariables")).toArray();
        QCOMPARE(variables.size(), 3);
        QCOMPARE(variables.at(0).toObject().value("guid").toString(), second.toString(QUuid::WithoutBraces));
        QCOMPARE(variables.at(0).toObject().value("variable").toString(), QStringLiteral("beta"));
        QCOMPARE(variables.at(1).toObject().value("guid").toString(), QString());
        QCOMPARE(variables.at(1).toObject().value("variable").toString(), QString());
        QCOMPARE(variables.at(2).toObject().value("variable").toString(), QStringLiteral("delta"));

        // Through the portable codec and back into an item.
        ContainerDocument document;
        document.id = QStringLiteral("mmio-variables");
        document.layout = ContentLayout::VerticalStack;
        document.contents = {captured};
        const DocumentResult decoded =
            ContainerDocumentCodec::importContainer(ContainerDocumentCodec::exportContainer(document));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        const ContentEntry imported = decoded.document.containers.first().contents.first();
        QCOMPARE(imported.context.value(QStringLiteral("mmioVariables")), captured.context.value(QStringLiteral("mmioVariables")));
        std::unique_ptr<MeterItem> restored(registry.createMeterItem(imported, nullptr, ContentRenderMode::Preview));
        QVERIFY(restored);
        QCOMPARE(restored->mmioGuid(1), second);
        QCOMPARE(restored->mmioVariable(1), QStringLiteral("beta"));
        QVERIFY(!restored->hasMmioBinding(2));
        QCOMPARE(restored->mmioGuid(3), fourth);
        QCOMPARE(restored->mmioVariable(3), QStringLiteral("delta"));
        QVERIFY(!restored->hasMmioBinding());
        QCOMPARE(registry.captureMeterItem(*restored, imported).context.value(QStringLiteral("mmioVariables")),
                 captured.context.value(QStringLiteral("mmioVariables")));
    }

    void editorShowsOneRowPairPerVariable()
    {
        ContainerContentRegistry registry;
        ContentPropertyEditor editor(registry);
        editor.setEntry(registry.makeEntry(QStringLiteral("meter.ananMulti")));
        QCOMPARE(dataSourceRows(editor), 7);
        editor.setEntry(registry.makeEntry(QStringLiteral("meter.agc")));
        QCOMPARE(dataSourceRows(editor), 2);
        editor.setEntry(registry.makeEntry(QStringLiteral("meter.signal")));
        QCOMPARE(dataSourceRows(editor), 1);
        QVERIFY(!editor.findChild<QLineEdit*>(QStringLiteral("mmioVariable1")));
    }

    void editorWritesTheSecondVariable()
    {
        ContainerContentRegistry registry;
        ContentPropertyEditor editor(registry);
        ContentEntry edited;
        connect(&editor, &ContentPropertyEditor::entryEdited, this, [&](const ContentEntry& e) { edited = e; });
        editor.setEntry(registry.makeEntry(QStringLiteral("meter.agc")));
        auto* guid = editor.findChild<QLineEdit*>(QStringLiteral("mmioGuid1"));
        auto* variable = editor.findChild<QLineEdit*>(QStringLiteral("mmioVariable1"));
        QVERIFY(guid);
        QVERIFY(variable);
        const QString id = QStringLiteral("00112233-4455-6677-8899-aabbccddeeff");
        guid->setText(id);
        emit guid->editingFinished();
        variable->setText(QStringLiteral("avg"));
        emit variable->editingFinished();
        const QJsonArray variables = edited.context.value(QStringLiteral("mmioVariables")).toArray();
        QCOMPARE(variables.size(), 1);
        QCOMPARE(variables.at(0).toObject().value("guid").toString(), id);
        QCOMPARE(variables.at(0).toObject().value("variable").toString(), QStringLiteral("avg"));
        // The first variable's keys are untouched.
        QVERIFY(edited.context.value(QStringLiteral("mmioVariable")).toString().isEmpty());

        // The saved entry shows its values when opened again; clearing
        // both removes the key.
        editor.setEntry(edited);
        guid = editor.findChild<QLineEdit*>(QStringLiteral("mmioGuid1"));
        variable = editor.findChild<QLineEdit*>(QStringLiteral("mmioVariable1"));
        QCOMPARE(guid->text(), id);
        QCOMPARE(variable->text(), QStringLiteral("avg"));
        guid->clear();
        emit guid->editingFinished();
        variable->clear();
        emit variable->editingFinished();
        QVERIFY(!edited.context.contains(QStringLiteral("mmioVariables")));
    }
};

QTEST_MAIN(TstMeterMmioVariables)
#include "tst_meter_mmio_variables.moc"
