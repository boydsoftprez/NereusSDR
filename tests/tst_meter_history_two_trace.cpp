// no-port-check: NereusSDR-original tests for the two-trace signal history.
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave M2: trace 2's veil names its reason. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Signal history trace 2 on its own slice (D2), the saved
//                 one-trace face, data source variable 2, the HISTORY axis
//                 1 reading beside a data source. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QComboBox>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QUuid>
#include <memory>
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerSourceAdapter.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/HistoryGraphPresetItem.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
namespace {
constexpr int kSliceA = 7;
constexpr int kSliceB = 23;
QJsonObject sliceContext(int id) { return {{"sliceId", id}}; }
QJsonObject followContext() { return {{"rxSourceMode", "followSelectedRx"}}; }
// Both traces follow their readings at once, so a value reads exactly.
void makeImmediate(CompositePresetItem& face)
{
    QJsonArray channels = face.configuration()["channels"].toArray();
    for (int i = 0; i < channels.size(); ++i) {
        QJsonObject c = channels[i].toObject(); c["attack"] = 1; c["decay"] = 1; channels[i] = c;
    }
    // A short window, so a few polls draw a long line.
    QVERIFY(face.applyConfiguration({{"channels", channels}, {"historyMs", 1000}}));
}
// The configuration a signal history saved before trace 2 existed: the
// same keys with one channel and no trace2Context.
QJsonObject oneTraceConfiguration()
{
    CompositePresetItem fresh(CompositePresetItem::Face::History);
    QJsonObject config = fresh.configuration();
    config["channels"] = QJsonArray{config["channels"].toArray().first()};
    config.remove("trace2Context");
    return config;
}
QImage render(CompositePresetItem& face)
{
    QImage image(640, 240, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    QPainter painter(&image);
    face.paintForLayer(painter, 640, 240, MeterItem::Layer::Background);
    face.paintForLayer(painter, 640, 240, MeterItem::Layer::OverlayDynamic);
    painter.end();
    return image;
}
// The moving layer alone, over nothing: a veil pixel keeps its own alpha.
QImage renderOverlay(CompositePresetItem& face)
{
    QImage image(640, 240, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    QPainter painter(&image);
    face.paintForLayer(painter, 640, 240, MeterItem::Layer::OverlayDynamic);
    painter.end();
    return image;
}
int pixelsLike(const QImage& image, const QColor& ink, const QRect& area = {})
{
    const QRect box = area.isNull() ? image.rect() : area.intersected(image.rect());
    int count = 0;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        for (int x = box.left(); x <= box.right(); ++x) {
            const QColor p = image.pixelColor(x, y);
            if (qAbs(p.red() - ink.red()) < 40 && qAbs(p.green() - ink.green()) < 40 && qAbs(p.blue() - ink.blue()) < 40 && p.alpha() > 200) { ++count; }
        }
    }
    return count;
}
}
class TestMeterHistoryTwoTrace : public QObject {
    Q_OBJECT
private:
    struct Station {
        RadioModel model{RadioModel::Role::Remote};
        SliceModel* selected = nullptr;
        QString session = QStringLiteral("station");
        qint64 time = 100;
        MeterPoller poller;
        Station()
        {
            model.addSliceWithStationId(kSliceA);
            model.addSliceWithStationId(kSliceB);
            model.sliceById(kSliceA)->setStreamIndex(0); model.sliceById(kSliceB)->setStreamIndex(1);
            model.sliceById(kSliceA)->setSignalAverageDbm(-35); model.sliceById(kSliceB)->setSignalAverageDbm(-95);
            selected = model.sliceById(kSliceA);
            poller.setMonotonicSourceForTest([this] { return time; });
            poller.setRxSourceIdentitySource([this](const QJsonObject& context) {
                return ContainerSourceAdapter::sourceIdentity(&model, context, selected, session);
            });
            poller.setRxReadingSource([this](const QJsonObject& context, int binding) {
                return ContainerSourceAdapter::reading(&model, context, selected, binding, true, true, {}, session);
            });
        }
        void poll()
        {
            QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
            time += 100;
        }
    };
private slots:
    // Step 1: an old one-trace configuration first.
    void oldOneTraceConfigurationLoadsAndLooksTheSame()
    {
        const QJsonObject old = oneTraceConfiguration();
        QCOMPARE(old["channels"].toArray().size(), 1);
        // The saved record and the saved properties both carry one channel.
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry("meter.historyGraph");
        entry.config["legacyRecord"] = QString::fromUtf8(QJsonDocument(old).toJson(QJsonDocument::Compact));
        entry.config["properties"] = old;
        std::unique_ptr<MeterItem> created(registry.createMeterItem(entry, nullptr, ContentRenderMode::Live));
        QVERIFY2(created, "A saved one-trace signal history must load");
        auto* loaded = qobject_cast<CompositePresetItem*>(created.get());
        QVERIFY(loaded);
        QCOMPARE(loaded->configuration()["channels"].toArray().size(), 2);
        QVERIFY(!loaded->trace2Shown());
        QVERIFY(!loaded->configuration().contains("trace2Context"));
        QCOMPARE(loaded->readingBindings(), (QSet<int>{MeterBinding::SignalAvg}));
        // Looks the same: the same trace 1 samples paint the same pixels as
        // a face that never had a saved record, and trace 2 draws nothing.
        CompositePresetItem reference(CompositePresetItem::Face::History);
        QVERIFY(reference.applyConfiguration(old));
        for (CompositePresetItem* face : {loaded, &reference}) {
            for (int t = 0; t <= 600; t += 100) { face->pushBindingValue(MeterBinding::SignalAvg, -120 + t / 10.0); face->advanceMeter(t); }
        }
        loaded->pushTrace2Value(-40);
        loaded->advanceMeter(700); reference.advanceMeter(700);
        QCOMPARE(loaded->trace2SampleCountForTest(), 0);
        const QImage a = render(*loaded);
        QCOMPARE(a, render(reference));
        QCOMPARE(pixelsLike(a, QColor(Qt::red)), 0);
        QVERIFY(pixelsLike(a, QColor(Qt::yellow)) > 50);
        // An edit of a one-channel configuration is padded, never rejected.
        CompositePresetItem edited(CompositePresetItem::Face::History);
        QVERIFY(edited.applyConfiguration({{"trace2Context", sliceContext(kSliceB)}}));
        QVERIFY(edited.trace2Shown());
        QVERIFY(edited.deserialize(QString::fromUtf8(QJsonDocument(old).toJson(QJsonDocument::Compact))));
        QVERIFY2(!edited.trace2Shown(), "A saved one-trace face loads with trace 2 off");
    }
    void eachTraceRecordsItsOwnSlice()
    {
        Station station;
        MeterWidget meter;
        auto* face = new HistoryGraphPresetItem(&meter);
        makeImmediate(*face);
        QVERIFY(face->applyConfiguration({{"trace2Context", sliceContext(kSliceB)}}));
        meter.addItem(face);
        const QJsonObject entryContext = sliceContext(kSliceA);
        station.poller.setTargetContext(&meter, entryContext);
        face->attachTrace2(&station.poller, entryContext, station.session);
        QVERIFY(face->trace2UnavailableReason().isEmpty());
        for (int i = 0; i < 4; ++i) { station.poll(); }
        QCOMPARE(face->channelValue(0), -35.0);
        QCOMPARE(face->channelValue(1), -95.0);
        QVERIFY(face->historySampleCountForTest() >= 3);
        QVERIFY(face->trace2SampleCountForTest() >= 3);
        // Trace 2's slice moves; trace 1's does not see it.
        station.model.sliceById(kSliceB)->setSignalAverageDbm(-60);
        station.poll();
        QCOMPARE(face->channelValue(0), -35.0);
        QCOMPARE(face->channelValue(1), -60.0);
        // Both lines are drawn, in their own colors.
        const QImage image = render(*face);
        QVERIFY(pixelsLike(image, QColor(Qt::yellow)) > 50);
        QVERIFY(pixelsLike(image, QColor(Qt::red)) > 50);
        // Trace 2 off: it leaves the poller and keeps no line.
        QVERIFY(face->applyConfiguration({{"trace2Context", QJsonValue(QJsonValue::Null)}}));
        face->attachTrace2(&station.poller, entryContext, station.session);
        station.poll(); station.poll();
        QVERIFY(!face->trace2Shown());
        QCOMPARE(face->trace2SampleCountForTest(), 0);
    }
    void followSelectedTraceSwitchesWithTheSelectedSlice()
    {
        Station station;
        MeterWidget meter;
        auto* face = new HistoryGraphPresetItem(&meter);
        makeImmediate(*face);
        QVERIFY(face->applyConfiguration({{"trace2Context", followContext()}}));
        meter.addItem(face);
        const QJsonObject entryContext = sliceContext(kSliceA);
        station.poller.setTargetContext(&meter, entryContext);
        face->attachTrace2(&station.poller, entryContext, station.session);
        for (int i = 0; i < 3; ++i) { station.poll(); }
        QCOMPARE(face->channelValue(1), -35.0);
        const int before = face->trace2SampleCountForTest();
        QVERIFY(before >= 2);
        station.selected = station.model.sliceById(kSliceB);
        station.poll();
        QCOMPARE(face->channelValue(1), -95.0);
        QVERIFY2(face->trace2SampleCountForTest() <= 1, "The old slice's line goes when the selected slice changes");
        // Trace 1 keeps its fixed slice.
        QCOMPARE(face->channelValue(0), -35.0);
        QVERIFY(face->historySampleCountForTest() > before);
    }
    void foreignTraceShowsItsOwnVeilOnly()
    {
        Station station;
        MeterWidget meter;
        auto* face = new HistoryGraphPresetItem(&meter);
        makeImmediate(*face);
        QJsonObject foreign = sliceContext(kSliceB); foreign["sessionId"] = QStringLiteral("other-radio");
        QVERIFY(face->applyConfiguration({{"trace2Context", foreign}, {"showReadout", true}}));
        meter.addItem(face);
        const QJsonObject entryContext = sliceContext(kSliceA);
        station.poller.setTargetContext(&meter, entryContext);
        face->attachTrace2(&station.poller, entryContext, station.session);
        QCOMPARE(face->trace2UnavailableReason(), HistoryGraphPresetItem::otherRadioText());
        for (int i = 0; i < 4; ++i) { station.poll(); }
        QVERIFY(face->channelHasReading(0));
        QVERIFY(!face->channelHasReading(1));
        QCOMPARE(face->trace2SampleCountForTest(), 0);
        // The entry itself is available: no veil over the whole face.
        QVERIFY(face->bindingUnavailableReason(MeterBinding::SignalAvg).isEmpty());
        const QImage image = renderOverlay(*face);
        const QRect cell = face->trace2ReadoutRect(640, 240).toAlignedRect();
        // A corner of the cell: the reason's text sits on the middle row.
        const QPoint probe(cell.left() + 1, cell.top() + 1);
        const QColor veil = image.pixelColor(probe);
        QVERIFY2(veil.alpha() >= 150 && veil.red() < 40 && veil.blue() < 60, "Trace 2's readout takes the unavailable veil");
        // The veil says why (fix wave M2), not a bare "--".
        QCOMPARE(face->trace2ReadoutText(), HistoryGraphPresetItem::otherRadioText());
        // Trace 1 keeps drawing its line.
        QVERIFY(pixelsLike(image, QColor(Qt::yellow)) > 50);
        // The same trace on this window's radio reads again, unveiled.
        face->updateTrace2Session(entryContext, QStringLiteral("other-radio"));
        QVERIFY(face->trace2UnavailableReason().isEmpty());
        QVERIFY(face->trace2ReadoutText() != HistoryGraphPresetItem::otherRadioText());
        QCOMPARE(renderOverlay(*face).pixelColor(probe).alpha(), 0);
    }
    void dataSourceVariableTwoFeedsTraceTwo()
    {
        // From Thetis MeterManager.cs:22670,18132-18163 [v2.10.3.15]: variable 2
        // is the right axis.
        MeterWidget meter;
        auto* face = new HistoryGraphPresetItem(&meter);
        makeImmediate(*face);
        QVERIFY(!face->trace2Shown());
        face->setMmioBinding(1, QUuid::createUuid(), QStringLiteral("level"));
        meter.addItem(face);
        QVERIFY(face->trace2Shown());
        QVERIFY(face->trace2FromDataSource());
        meter.updateMmioValue(face, 1, -50.0);
        face->advanceMeter(0);
        QCOMPARE(face->channelValue(1), -50.0);
        QCOMPARE(face->trace2SampleCountForTest(), 1);
        // The entry's reading still feeds trace 1 only.
        meter.updateMeterValue(MeterBinding::SignalAvg, -80.0);
        face->advanceMeter(100);
        QCOMPARE(face->channelValue(0), -80.0);
        QCOMPARE(face->channelValue(1), -50.0);
        // No consumer is registered for a data source trace.
        MeterPoller poller;
        face->attachTrace2(&poller, sliceContext(kSliceA), {});
        QVERIFY(face->trace2SourceContext(sliceContext(kSliceA)).isEmpty());
    }
    void historyAxisOneReadsBesideADataSource()
    {
        // From Thetis MeterManager.cs:18101-18165 [v2.10.3.15]: each axis reads
        // its data source or its reading on its own.
        MeterWidget meter;
        auto* item = new HistoryGraphItem(&meter);
        item->setBindingId(MeterBinding::SignalPeak);
        item->setBindingId1(MeterBinding::SignalPeak);
        item->setMmioBinding(QUuid::createUuid(), QStringLiteral("level"));
        meter.addItem(item);
        meter.updateMeterValue(MeterBinding::SignalPeak, -70.0);
        QCOMPARE(item->sampleCount1(), 1);
        QCOMPARE(item->sampleCount0(), 0);
        meter.updateMmioValue(item, -20.0);
        QCOMPARE(item->sampleCount0(), 1);
        // Variable 2 on axis 1 as well: no radio reading reaches the item.
        item->setMmioBinding(1, QUuid::createUuid(), QStringLiteral("other"));
        meter.updateMeterValue(MeterBinding::SignalPeak, -71.0);
        QCOMPARE(item->sampleCount1(), 1);
        QCOMPARE(item->sampleCount0(), 1);
    }
    void editorSourcePicksTraceTwoSlice()
    {
        ContainerContentRegistry registry;
        ContentPropertyEditor editor(registry);
        ContentEntry entry = registry.makeEntry("meter.historyGraph");
        // New faces take Thetis's trace colors and start with trace 2 off.
        const QJsonArray channels = entry.config["properties"].toObject()["channels"].toArray();
        QCOMPARE(channels.size(), 2);
        QCOMPARE(QColor(channels[0].toObject()["color"].toString()), QColor(Qt::red));
        QCOMPARE(QColor(channels[1].toObject()["color"].toString()), QColor(Qt::yellow));
        QVERIFY(!entry.config["properties"].toObject().contains("trace2Context"));
        ContentEntry edited;
        connect(&editor, &ContentPropertyEditor::entryEdited, this, [&](const ContentEntry& value) { edited = value; });
        editor.setEntry(entry);
        auto* source = editor.findChild<QComboBox*>("historyTrace2Source");
        QVERIFY(source);
        QCOMPARE(source->currentText(), QStringLiteral("Off"));
        QVERIFY(source->findText(QStringLiteral("Follow selected RX")) >= 0);
        source->setCurrentIndex(source->findText(QStringLiteral("Slice B")));
        QCOMPARE(edited.config["properties"].toObject()["trace2Context"].toObject(), (QJsonObject{{"sliceId", 1}}));
        editor.setEntry(edited);
        source = editor.findChild<QComboBox*>("historyTrace2Source");
        QCOMPARE(source->currentText(), QStringLiteral("Slice B"));
        source->setCurrentIndex(source->findText(QStringLiteral("Follow selected RX")));
        QCOMPARE(edited.config["properties"].toObject()["trace2Context"].toObject(), followContext());
        source->setCurrentIndex(source->findText(QStringLiteral("Off")));
        QVERIFY(!edited.config["properties"].toObject().contains("trace2Context"));
        // The entry's own Source row is unchanged by trace 2's.
        QVERIFY(!edited.context.contains("sliceId"));
    }
    void malformedTraceTwoSourcesAreRejected()
    {
        CompositePresetItem face(CompositePresetItem::Face::History);
        QVERIFY(!face.applyConfiguration({{"trace2Context", 3}}));
        QVERIFY(!face.applyConfiguration({{"trace2Context", QJsonObject{}}}));
        QVERIFY(!face.applyConfiguration({{"trace2Context", QJsonObject{{"rxSourceMode", "other"}}}}));
        QVERIFY(!face.applyConfiguration({{"trace2Context", QJsonObject{{"sliceId", -1}}}}));
        QVERIFY(!face.applyConfiguration({{"trace2Context", QJsonObject{{"sliceId", 1}, {"sessionId", 4}}}}));
        QVERIFY(!face.trace2Shown());
        CompositePresetItem eye(CompositePresetItem::Face::Eye);
        QVERIFY(!eye.applyConfiguration({{"trace2Context", sliceContext(1)}}));
    }
};
QTEST_MAIN(TestMeterHistoryTwoTrace)
#include "tst_meter_history_two_trace.moc"
