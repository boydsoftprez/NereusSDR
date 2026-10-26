// no-port-check: NereusSDR-original tests for container item records.
// Modification history (NereusSDR):
//   2026-10-25 - Clock, VFO display, text overlay and filter display
//                 colours, and button grid styling, survive a save and
//                 reload; records saved before those fields existed still
//                 load with the defaults. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Filter display Thetis settings and colours survive a
//                 save; a new filter display takes the Thetis values and a
//                 saved one keeps its look. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.

#include <QtTest>
#include <QColor>
#include <QStringList>
#include <functional>
#include <memory>

#include "core/UnbuiltFeatureList.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ButtonBoxItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/NereusBandButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"

using namespace NereusSDR;

namespace {

// Values that differ from every item's defaults.
const QColor kTime(0x11, 0x22, 0x33);
const QColor kDate(0x44, 0x55, 0x66);
const QColor kTitle(0x77, 0x88, 0x99);
const QColor kFreq(0x01, 0x02, 0x03);
const QColor kMode(0x04, 0x05, 0x06);
const QColor kFilter(0x07, 0x08, 0x09);
const QColor kBand(0x0a, 0x0b, 0x0c);
const QColor kRx(0x0d, 0x0e, 0x0f);
const QColor kTx(0x10, 0x11, 0x12);
const QColor kBack1(0x12, 0x34, 0x56);
const QColor kBack2(0x65, 0x43, 0x21, 0x80);   // alpha kept
const QColor kFilterText(0x33, 0x66, 0x99);
constexpr float kBorder = 0.07f;
constexpr float kMargin = 0.2f;
constexpr float kRadius = 9.5f;
constexpr float kRatio = 2.25f;

std::unique_ptr<MeterItem> makeItem(const QString& typeId)
{
    if (typeId == QLatin1String("CLOCK")) { return std::make_unique<ClockItem>(nullptr, false); }
    if (typeId == QLatin1String("VFO")) { return std::make_unique<VfoDisplayItem>(); }
    if (typeId == QLatin1String("TEXTOVERLAY")) { return std::make_unique<TextOverlayItem>(); }
    if (typeId == QLatin1String("FILTERDISPLAY")) { return std::make_unique<FilterDisplayItem>(); }
    if (typeId == QLatin1String("BANDBTNS")) { return std::make_unique<BandButtonItem>(); }
    if (typeId == QLatin1String("MODEBTNS")) { return std::make_unique<ModeButtonItem>(); }
    if (typeId == QLatin1String("FILTERBTNS")) { return std::make_unique<FilterButtonItem>(); }
    if (typeId == QLatin1String("ANTENNABTNS")) { return std::make_unique<AntennaButtonItem>(); }
    if (typeId == QLatin1String("TUNESTEPBTNS")) { return std::make_unique<TuneStepButtonItem>(); }
    if (typeId == QLatin1String("OTHERBTNS")) { return std::make_unique<OtherButtonItem>(); }
    if (typeId == QLatin1String("VOICERECPLAY")) { return std::make_unique<VoiceRecordPlayItem>(); }
    if (typeId == QLatin1String("NEREUSBAND")) { return std::make_unique<NereusBandButtonItem>(); }
    return nullptr;
}

// Sets every field this fix saves to a non-default value.
void setNonDefaults(MeterItem* item)
{
    if (auto* clock = qobject_cast<ClockItem*>(item)) {
        clock->setTimeColour(kTime);
        clock->setDateColour(kDate);
        clock->setTypeTitleColour(kTitle);
    }
    if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
        vfo->setFrequencyColour(kFreq);
        vfo->setModeColour(kMode);
        vfo->setFilterColour(kFilter);
        vfo->setBandColour(kBand);
        vfo->setRxColour(kRx);
        vfo->setTxColour(kTx);
    }
    if (auto* overlay = qobject_cast<TextOverlayItem*>(item)) {
        overlay->setTextBackColour1(kBack1);
        overlay->setTextBackColour2(kBack2);
        overlay->setShowTextBackColour1(true);
        overlay->setShowTextBackColour2(true);
    }
    if (auto* filter = qobject_cast<FilterDisplayItem*>(item)) {
        filter->setTextColour(kFilterText);
    }
    if (auto* grid = qobject_cast<ButtonBoxItem*>(item)) {
        grid->setBorderWidth(kBorder);
        grid->setMargin(kMargin);
        grid->setCornerRadius(kRadius);
        grid->setHeightRatio(kRatio);
        grid->setFadeOnRx(true);
        grid->setFadeOnTx(true);
    }
}

void verifyNonDefaults(const MeterItem* item)
{
    if (const auto* clock = qobject_cast<const ClockItem*>(item)) {
        QCOMPARE(clock->timeColour(), kTime);
        QCOMPARE(clock->dateColour(), kDate);
        QCOMPARE(clock->typeTitleColour(), kTitle);
    }
    if (const auto* vfo = qobject_cast<const VfoDisplayItem*>(item)) {
        QCOMPARE(vfo->frequencyColour(), kFreq);
        QCOMPARE(vfo->modeColour(), kMode);
        QCOMPARE(vfo->filterColour(), kFilter);
        QCOMPARE(vfo->bandColour(), kBand);
        QCOMPARE(vfo->rxColour(), kRx);
        QCOMPARE(vfo->txColour(), kTx);
    }
    if (const auto* overlay = qobject_cast<const TextOverlayItem*>(item)) {
        QCOMPARE(overlay->textBackColour1(), kBack1);
        QCOMPARE(overlay->textBackColour2(), kBack2);
        QVERIFY(overlay->showTextBackColour1());
        QVERIFY(overlay->showTextBackColour2());
    }
    if (const auto* filter = qobject_cast<const FilterDisplayItem*>(item)) {
        QCOMPARE(filter->textColour(), kFilterText);
    }
    if (const auto* grid = qobject_cast<const ButtonBoxItem*>(item)) {
        QCOMPARE(grid->borderWidth(), kBorder);
        QCOMPARE(grid->margin(), kMargin);
        QCOMPARE(grid->cornerRadius(), kRadius);
        QCOMPARE(grid->heightRatio(), kRatio);
        QVERIFY(grid->fadeOnRx());
        QVERIFY(grid->fadeOnTx());
    }
}

// The new fields equal those of a freshly made item of the same type.
void verifyDefaults(const MeterItem* item, const MeterItem* fresh)
{
    if (const auto* clock = qobject_cast<const ClockItem*>(item)) {
        const auto* f = qobject_cast<const ClockItem*>(fresh);
        QCOMPARE(clock->timeColour(), f->timeColour());
        QCOMPARE(clock->dateColour(), f->dateColour());
        QCOMPARE(clock->typeTitleColour(), f->typeTitleColour());
    }
    if (const auto* vfo = qobject_cast<const VfoDisplayItem*>(item)) {
        const auto* f = qobject_cast<const VfoDisplayItem*>(fresh);
        QCOMPARE(vfo->frequencyColour(), f->frequencyColour());
        QCOMPARE(vfo->modeColour(), f->modeColour());
        QCOMPARE(vfo->filterColour(), f->filterColour());
        QCOMPARE(vfo->bandColour(), f->bandColour());
        QCOMPARE(vfo->rxColour(), f->rxColour());
        QCOMPARE(vfo->txColour(), f->txColour());
    }
    if (const auto* overlay = qobject_cast<const TextOverlayItem*>(item)) {
        const auto* f = qobject_cast<const TextOverlayItem*>(fresh);
        QCOMPARE(overlay->textBackColour1(), f->textBackColour1());
        QCOMPARE(overlay->textBackColour2(), f->textBackColour2());
        QCOMPARE(overlay->showTextBackColour1(), f->showTextBackColour1());
        QCOMPARE(overlay->showTextBackColour2(), f->showTextBackColour2());
    }
    if (const auto* filter = qobject_cast<const FilterDisplayItem*>(item)) {
        QCOMPARE(filter->textColour(), qobject_cast<const FilterDisplayItem*>(fresh)->textColour());
    }
    if (const auto* grid = qobject_cast<const ButtonBoxItem*>(item)) {
        const auto* f = qobject_cast<const ButtonBoxItem*>(fresh);
        QCOMPARE(grid->borderWidth(), f->borderWidth());
        QCOMPARE(grid->margin(), f->margin());
        QCOMPARE(grid->cornerRadius(), f->cornerRadius());
        QCOMPARE(grid->heightRatio(), f->heightRatio());
        QCOMPARE(grid->fadeOnRx(), f->fadeOnRx());
        QCOMPARE(grid->fadeOnTx(), f->fadeOnTx());
    }
}

} // namespace

class TstContainerItemPersistence : public QObject {
    Q_OBJECT

    // Each type with the exact record its serialize() wrote before this
    // change, old fields set away from their defaults where there are any.
    static void addRecords()
    {
        QTest::addColumn<QString>("typeId");
        QTest::addColumn<QString>("oldRecord");
        QTest::newRow("CLOCK") << QStringLiteral("CLOCK") << QStringLiteral("CLOCK|0.1|0.2|0.3|0.4|5|6|0|0");
        QTest::newRow("VFO") << QStringLiteral("VFO") << QStringLiteral("VFO|0.1|0.2|0.3|0.4|5|6|1");
        QTest::newRow("TEXTOVERLAY") << QStringLiteral("TEXTOVERLAY")
            << QStringLiteral("TEXTOVERLAY|0.1|0.2|0.3|0.4|5|6|#ffff0000|#ff00ff00|#ff111111|#ff222222|0"
               "|Arial|12|1|Courier|14|0|-1|0.2|aGVsbG8=|d29ybGQ=");
        QTest::newRow("FILTERDISPLAY") << QStringLiteral("FILTERDISPLAY")
            << QStringLiteral("FILTERDISPLAY|0.1|0.2|0.3|0.4|5|6|1|#ff010203|#ff040506|#ff070809|#ff0a0b0c"
               "|#ff0d0e0f|#ff101112|0|0.1|-120|-30|1|6");
        QTest::newRow("BANDBTNS") << QStringLiteral("BANDBTNS") << QStringLiteral("BANDBTNS|0.1|0.2|0.3|0.4|5|6|3|2|255");
        QTest::newRow("MODEBTNS") << QStringLiteral("MODEBTNS") << QStringLiteral("MODEBTNS|0.1|0.2|0.3|0.4|5|6|3|2|255");
        QTest::newRow("FILTERBTNS") << QStringLiteral("FILTERBTNS") << QStringLiteral("FILTERBTNS|0.1|0.2|0.3|0.4|5|6|3|2|255");
        QTest::newRow("TUNESTEPBTNS") << QStringLiteral("TUNESTEPBTNS") << QStringLiteral("TUNESTEPBTNS|0.1|0.2|0.3|0.4|5|6|3|2|127");
        QTest::newRow("ANTENNABTNS") << QStringLiteral("ANTENNABTNS") << QStringLiteral("ANTENNABTNS|0.1|0.2|0.3|0.4|5|6|3|255");
        QTest::newRow("OTHERBTNS") << QStringLiteral("OTHERBTNS") << QStringLiteral("OTHERBTNS|0.1|0.2|0.3|0.4|5|6|3|254");
        QTest::newRow("VOICERECPLAY") << QStringLiteral("VOICERECPLAY") << QStringLiteral("VOICERECPLAY|0.1|0.2|0.3|0.4|5|6|3");
        QTest::newRow("NEREUSBAND") << QStringLiteral("NEREUSBAND")
            << QStringLiteral("NEREUSBAND|0.1|0.2|0.3|0.4|5|6|3|2|255|#ff0a141e|#ff28323c|#ff46505a|#ff0c5028|#ff062814");
    }

private slots:
    void init()
    {
        // The registry offers these only once their features are built.
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::ContainerFilterDisplay, true);
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Voice, true);
    }
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    void roundTripKeepsEveryField_data() { addRecords(); }
    void roundTripKeepsEveryField()
    {
        QFETCH(QString, typeId);
        auto item = makeItem(typeId);
        QVERIFY(item);
        setNonDefaults(item.get());
        const QString saved = item->serialize();
        QVERIFY(saved.startsWith(typeId + QLatin1Char('|')));

        auto restored = makeItem(typeId);
        QVERIFY(restored->deserialize(saved));
        verifyNonDefaults(restored.get());
        QCOMPARE(restored->serialize(), saved);
    }

    void oldRecordLoadsWithDefaults_data() { addRecords(); }
    void oldRecordLoadsWithDefaults()
    {
        QFETCH(QString, typeId);
        QFETCH(QString, oldRecord);
        auto restored = makeItem(typeId);
        QVERIFY(restored->deserialize(oldRecord));
        // The old fields come back as saved: the new record is the old
        // one with the new fields appended.
        QVERIFY2(restored->serialize().startsWith(oldRecord + QLatin1Char('|')),
                 qPrintable(restored->serialize()));
        QCOMPARE(restored->x(), 0.1f);
        QCOMPARE(restored->zOrder(), 6);
        const auto fresh = makeItem(typeId);
        verifyDefaults(restored.get(), fresh.get());
    }

    // The path a saved layout takes: capture into a content entry, write
    // and read the workspace document, then build the item from the entry.
    void savedLayoutKeepsEveryField_data() { addRecords(); }
    void savedLayoutKeepsEveryField()
    {
        QFETCH(QString, typeId);
        ContainerContentRegistry registry;
        QVERIFY2(registry.isAvailable(typeId), qPrintable(typeId));
        ContentEntry entry = registry.makeEntry(typeId);
        std::unique_ptr<MeterItem> live(registry.createMeterItem(entry, nullptr));
        QVERIFY(live);
        setNonDefaults(live.get());
        entry = registry.captureMeterItem(*live, entry);

        WorkspaceDocument doc;
        doc.mainContainerId = QStringLiteral("A");
        ContainerDocument container;
        container.id = doc.mainContainerId;
        container.contents.append(entry);
        doc.containers.append(container);
        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document.containers.first().contents.size(), 1);

        std::unique_ptr<MeterItem> reloaded(
            registry.createMeterItem(decoded.document.containers.first().contents.first(), nullptr));
        QVERIFY(reloaded);
        verifyNonDefaults(reloaded.get());
    }

    // A layout saved before this change: its entry carries only the old
    // record and loads with the new fields at their defaults.
    void savedLayoutFromOldRecordLoads_data() { addRecords(); }
    void savedLayoutFromOldRecordLoads()
    {
        QFETCH(QString, typeId);
        QFETCH(QString, oldRecord);
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(typeId);
        entry.config.insert(QStringLiteral("legacyRecord"), oldRecord);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        QVERIFY(item);
        const auto fresh = makeItem(typeId);
        verifyDefaults(item.get(), fresh.get());
    }

    // Every Thetis filter display setting and colour comes back after a
    // save and reload, through the layout document.
    void filterDisplayThetisSettingsSurviveSave()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("FILTERDISPLAY"));
        std::unique_ptr<MeterItem> live(registry.createMeterItem(entry, nullptr));
        auto* f = qobject_cast<FilterDisplayItem*>(live.get());
        QVERIFY(f);
        f->setVerticalRatio(0.65f);
        f->setShowFilterLimits(false);
        f->setFixedRxZoom(true);
        f->setFixedTxZoom(true);
        f->setRxZoom(3.5f);
        f->setTxZoom(12.0f);
        f->setSidebandsScale(1.5f);
        f->setCwScale(2.5f);
        f->setOthersScale(3.5f);
        f->setFontScale(1.75f);
        f->setSidebandMode(true);
        f->setWaterfallFrameInterval(17);
        f->setGreyWhenNotActive(false);
        f->setSnapLines(true);
        f->setAutoZoom(true);
        f->setShowCharacteristic(true);
        f->setCharacteristicLow(-120.0f);
        f->setNumberHighlightColour(QColor(1, 2, 3));
        f->setEdgeHighlightColour(QColor(4, 5, 6));
        f->setExtentsColour(QColor(7, 8, 9));
        f->setNotchHighlightColour(QColor(10, 11, 12));
        f->setSnapLineColour(QColor(13, 14, 15));
        f->setSettingOnColour(QColor(16, 17, 18, 128));
        f->setButtonHighlightColour(QColor(19, 20, 21));
        entry = registry.captureMeterItem(*live, entry);

        WorkspaceDocument doc;
        doc.mainContainerId = QStringLiteral("A");
        ContainerDocument container;
        container.id = doc.mainContainerId;
        container.contents.append(entry);
        doc.containers.append(container);
        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        std::unique_ptr<MeterItem> reloaded(
            registry.createMeterItem(decoded.document.containers.first().contents.first(), nullptr));
        const auto* r = qobject_cast<const FilterDisplayItem*>(reloaded.get());
        QVERIFY(r);
        QCOMPARE(r->verticalRatio(), 0.65f);
        QVERIFY(!r->showFilterLimits());
        QVERIFY(r->fixedRxZoom());
        QVERIFY(r->fixedTxZoom());
        QCOMPARE(r->rxZoom(), 3.5f);
        QCOMPARE(r->txZoom(), 12.0f);
        QCOMPARE(r->sidebandsScale(), 1.5f);
        QCOMPARE(r->cwScale(), 2.5f);
        QCOMPARE(r->othersScale(), 3.5f);
        QCOMPARE(r->fontScale(), 1.75f);
        QVERIFY(r->sidebandMode());
        QCOMPARE(r->waterfallFrameInterval(), 17);
        QVERIFY(!r->greyWhenNotActive());
        QVERIFY(r->snapLines());
        QVERIFY(r->autoZoom());
        QVERIFY(r->showCharacteristic());
        QCOMPARE(r->characteristicLow(), -120.0f);
        QCOMPARE(r->numberHighlightColour(), QColor(1, 2, 3));
        QCOMPARE(r->edgeHighlightColour(), QColor(4, 5, 6));
        QCOMPARE(r->extentsColour(), QColor(7, 8, 9));
        QCOMPARE(r->notchHighlightColour(), QColor(10, 11, 12));
        QCOMPARE(r->snapLineColour(), QColor(13, 14, 15));
        QCOMPARE(r->settingOnColour(), QColor(16, 17, 18, 128));
        QCOMPARE(r->buttonHighlightColour(), QColor(19, 20, 21));
        QCOMPARE(reloaded->serialize(), live->serialize());
    }

    // A new filter display takes the Thetis values; one saved before the
    // settings existed keeps its saved look and the constructor values.
    void filterDisplayNewItemThetisOldItemUnchanged()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("FILTERDISPLAY"));
        std::unique_ptr<MeterItem> made(registry.createMeterItem(entry, nullptr));
        const auto* f = qobject_cast<const FilterDisplayItem*>(made.get());
        QVERIFY(f);
        QCOMPARE(f->verticalRatio(), kFilterDisplayThetisDefaults.verticalRatio);
        QCOMPARE(f->notchColour(), QColor::fromRgb(kFilterDisplayThetisDefaults.notchColour));
        QCOMPARE(f->characteristicLow(), -250.0f);
        QVERIFY(f->greyWhenNotActive());

        const QString oldRecord = QStringLiteral(
            "FILTERDISPLAY|0.1|0.2|0.3|0.4|5|6|1|#ff010203|#ff040506|#ff070809|#ff0a0b0c"
            "|#ff0d0e0f|#ff101112|0|0.1|-120|-30|1|6|#ff336699");
        ContentEntry saved = registry.makeEntry(QStringLiteral("FILTERDISPLAY"));
        saved.config.insert(QStringLiteral("legacyRecord"), oldRecord);
        std::unique_ptr<MeterItem> restored(registry.createMeterItem(saved, nullptr));
        const auto* o = qobject_cast<const FilterDisplayItem*>(restored.get());
        QVERIFY(o);
        // The entry places the item, so compare from the display mode on.
        const QStringList oldParts = oldRecord.split(QLatin1Char('|'));
        const QStringList newParts = restored->serialize().split(QLatin1Char('|'));
        QCOMPARE(newParts.mid(7, oldParts.size() - 7), oldParts.mid(7));
        QCOMPARE(o->verticalRatio(), 0.1f);
        QCOMPARE(o->notchColour(), QColor(0x0d, 0x0e, 0x0f));
        const FilterDisplayItem fresh;
        QCOMPARE(o->showFilterLimits(), fresh.showFilterLimits());
        QCOMPARE(o->fontScale(), fresh.fontScale());
        QCOMPARE(o->characteristicLow(), fresh.characteristicLow());
        QCOMPARE(o->extentsColour(), fresh.extentsColour());
        QCOMPARE(o->buttonHighlightColour(), fresh.buttonHighlightColour());
        QCOMPARE(o->waterfallFrameInterval(), 6);
        QCOMPARE(o->waterfallPalette(), FilterDisplayItem::WaterfallPalette::Spectran);
    }
};

QTEST_MAIN(TstContainerItemPersistence)
#include "tst_container_item_persistence.moc"
