// no-port-check: NereusSDR-original tests for container item records.
// Modification history (NereusSDR):
//   2026-10-26 - Mode grid: twelve modes in DSPMode order; a ten-button
//                 record saved before the order marker is remapped (SAM
//                 7 -> 10, DIGL 8 -> 9, DIGU 9 -> 7, SPEC and DRM hidden)
//                 and a new record is not. The MODEBTNS old-record row
//                 shows LSB to AM only, which the remap leaves in place.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - VFO display: the new colours, band text, background,
//                 layout and a fixed slice survive a save; the old-record
//                 row saves Both (2), which loads as All slices and keeps 2.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Clock, VFO display, text overlay and filter display
//                 colours, and button grid styling, survive a save and
//                 reload; records saved before those fields existed still
//                 load with the defaults. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Filter display Thetis settings and colours survive a
//                 save; a new filter display takes the Thetis values and a
//                 saved one keeps its look. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Clock layout and background fill survive a save; a new
//                 clock takes the Thetis look and a saved one keeps its
//                 own. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.

#include <QtTest>
#include <QCheckBox>
#include <QColor>
#include <QLabel>
#include <QStringList>
#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>

#include "core/UnbuiltFeatureList.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/meter_property_editors/ModeButtonItemEditor.h"
#include "gui/containers/meter_property_editors/TuneStepButtonItemEditor.h"
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
#include "models/SliceModel.h"

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
const QColor kClockBack(0x21, 0x43, 0x65, 0x90);
const QColor kVfoSmall(0x13, 0x14, 0x15);
const QColor kVfoTitle(0x16, 0x17, 0x18);
const QColor kVfoSplitBack(0x19, 0x1a, 0x1b);
const QColor kVfoSplit(0x1c, 0x1d, 0x1e);
const QColor kVfoHighlight(0x1f, 0x20, 0x21);
const QColor kVfoBandText(0x22, 0x23, 0x24);
const QColor kVfoLock(0x25, 0x26, 0x27);
const QColor kVfoSync(0x28, 0x29, 0x2a);
const QColor kVfoBack(0x2b, 0x2c, 0x2d, 0x70);
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
        clock->setLayout(ClockItem::Layout::Thetis);
        clock->setBackColour(kClockBack);
        clock->setBackEnabled(true);
    }
    if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
        vfo->setFrequencyColour(kFreq);
        vfo->setModeColour(kMode);
        vfo->setFilterColour(kFilter);
        vfo->setBandColour(kBand);
        vfo->setRxColour(kRx);
        vfo->setTxColour(kTx);
        vfo->setFrequencySmallColour(kVfoSmall);
        vfo->setTitleColour(kVfoTitle);
        vfo->setSplitBackColour(kVfoSplitBack);
        vfo->setSplitColour(kVfoSplit);
        vfo->setDigitHighlightColour(kVfoHighlight);
        vfo->setBandTextColour(kVfoBandText);
        vfo->setLockColour(kVfoLock);
        vfo->setSyncColour(kVfoSync);
        vfo->setBackgroundColour(kVfoBack);
        vfo->setShowBandText(true);
        vfo->setLayout(VfoDisplayItem::Layout::Thetis);
        vfo->setDisplayModeValue(VfoDisplayItem::fixedSliceMode(2));
    }
    if (auto* overlay = qobject_cast<TextOverlayItem*>(item)) {
        overlay->setTextBackColour1(kBack1);
        overlay->setTextBackColour2(kBack2);
        overlay->setShowTextBackColour1(true);
        overlay->setShowTextBackColour2(true);
        overlay->setLineMode(TextOverlayItem::LineMode::RxTxSwap);
        overlay->setTextXOffset1(0.25f);
        overlay->setTextYOffset1(-0.5f);
        overlay->setTextXOffset2(1.5f);
        overlay->setTextYOffset2(0.125f);
        overlay->setFontItalic1(true);
        overlay->setFontUnderline1(true);
        overlay->setFontStrikeout2(true);
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
        QCOMPARE(clock->layout(), ClockItem::Layout::Thetis);
        QCOMPARE(clock->backColour(), kClockBack);
        QVERIFY(clock->backEnabled());
    }
    if (const auto* vfo = qobject_cast<const VfoDisplayItem*>(item)) {
        QCOMPARE(vfo->frequencyColour(), kFreq);
        QCOMPARE(vfo->modeColour(), kMode);
        QCOMPARE(vfo->filterColour(), kFilter);
        QCOMPARE(vfo->bandColour(), kBand);
        QCOMPARE(vfo->rxColour(), kRx);
        QCOMPARE(vfo->txColour(), kTx);
        QCOMPARE(vfo->frequencySmallColour(), kVfoSmall);
        QCOMPARE(vfo->titleColour(), kVfoTitle);
        QCOMPARE(vfo->splitBackColour(), kVfoSplitBack);
        QCOMPARE(vfo->splitColour(), kVfoSplit);
        QCOMPARE(vfo->digitHighlightColour(), kVfoHighlight);
        QCOMPARE(vfo->bandTextColour(), kVfoBandText);
        QCOMPARE(vfo->lockColour(), kVfoLock);
        QCOMPARE(vfo->syncColour(), kVfoSync);
        QCOMPARE(vfo->backgroundColour(), kVfoBack);
        QVERIFY(vfo->showBandText());
        QCOMPARE(vfo->layout(), VfoDisplayItem::Layout::Thetis);
        QCOMPARE(vfo->displayModeValue(), VfoDisplayItem::fixedSliceMode(2));
    }
    if (const auto* overlay = qobject_cast<const TextOverlayItem*>(item)) {
        QCOMPARE(overlay->textBackColour1(), kBack1);
        QCOMPARE(overlay->textBackColour2(), kBack2);
        QVERIFY(overlay->showTextBackColour1());
        QVERIFY(overlay->showTextBackColour2());
        QCOMPARE(overlay->lineMode(), TextOverlayItem::LineMode::RxTxSwap);
        QCOMPARE(overlay->textXOffset1(), 0.25f);
        QCOMPARE(overlay->textYOffset1(), -0.5f);
        QCOMPARE(overlay->textXOffset2(), 1.5f);
        QCOMPARE(overlay->textYOffset2(), 0.125f);
        QVERIFY(overlay->fontItalic1());
        QVERIFY(overlay->fontUnderline1());
        QVERIFY(!overlay->fontStrikeout1());
        QVERIFY(!overlay->fontItalic2());
        QVERIFY(overlay->fontStrikeout2());
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
        QCOMPARE(clock->layout(), f->layout());
        QCOMPARE(clock->backColour(), f->backColour());
        QCOMPARE(clock->backEnabled(), f->backEnabled());
    }
    if (const auto* vfo = qobject_cast<const VfoDisplayItem*>(item)) {
        const auto* f = qobject_cast<const VfoDisplayItem*>(fresh);
        QCOMPARE(vfo->frequencyColour(), f->frequencyColour());
        QCOMPARE(vfo->modeColour(), f->modeColour());
        QCOMPARE(vfo->filterColour(), f->filterColour());
        QCOMPARE(vfo->bandColour(), f->bandColour());
        QCOMPARE(vfo->rxColour(), f->rxColour());
        QCOMPARE(vfo->txColour(), f->txColour());
        QCOMPARE(vfo->frequencySmallColour(), f->frequencySmallColour());
        QCOMPARE(vfo->titleColour(), f->titleColour());
        QCOMPARE(vfo->splitBackColour(), f->splitBackColour());
        QCOMPARE(vfo->splitColour(), f->splitColour());
        QCOMPARE(vfo->digitHighlightColour(), f->digitHighlightColour());
        QCOMPARE(vfo->bandTextColour(), f->bandTextColour());
        QCOMPARE(vfo->lockColour(), f->lockColour());
        QCOMPARE(vfo->syncColour(), f->syncColour());
        QCOMPARE(vfo->backgroundColour(), QColor(0x0a, 0x0a, 0x18));
        QCOMPARE(vfo->showBandText(), f->showBandText());
        QCOMPARE(vfo->layout(), VfoDisplayItem::Layout::Compact);
        QCOMPARE(vfo->displayMode(), VfoDisplayItem::DisplayMode::AllSlices);
    }
    if (const auto* overlay = qobject_cast<const TextOverlayItem*>(item)) {
        const auto* f = qobject_cast<const TextOverlayItem*>(fresh);
        QCOMPARE(overlay->textBackColour1(), f->textBackColour1());
        QCOMPARE(overlay->textBackColour2(), f->textBackColour2());
        QCOMPARE(overlay->showTextBackColour1(), f->showTextBackColour1());
        QCOMPARE(overlay->showTextBackColour2(), f->showTextBackColour2());
        // An older record keeps both lines (ruling D5), whatever a new item takes.
        QCOMPARE(overlay->lineMode(), TextOverlayItem::LineMode::Both);
        QCOMPARE(overlay->textXOffset1(), 0.0f);
        QCOMPARE(overlay->textYOffset2(), 0.0f);
        QVERIFY(!overlay->fontItalic1());
        QVERIFY(!overlay->fontUnderline2());
        QVERIFY(!overlay->fontStrikeout2());
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

// The buttons a grid shows.
int shownCount(const ButtonBoxItem& grid)
{
    int shown = 0;
    for (int i = 0; i < grid.buttonCount(); ++i) {
        if (grid.isButtonShown(i)) { ++shown; }
    }
    return shown;
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
        QTest::newRow("VFO") << QStringLiteral("VFO") << QStringLiteral("VFO|0.1|0.2|0.3|0.4|5|6|2");
        QTest::newRow("TEXTOVERLAY") << QStringLiteral("TEXTOVERLAY")
            << QStringLiteral("TEXTOVERLAY|0.1|0.2|0.3|0.4|5|6|#ffff0000|#ff00ff00|#ff111111|#ff222222|0"
               "|Arial|12|1|Courier|14|0|-1|0.2|aGVsbG8=|d29ybGQ=");
        // The record with the text back fields, before the line mode.
        QTest::newRow("TEXTOVERLAY-textback") << QStringLiteral("TEXTOVERLAY")
            << QStringLiteral("TEXTOVERLAY|0.1|0.2|0.3|0.4|5|6|#ffff0000|#ff00ff00|#ff111111|#ff222222|0"
               "|Arial|12|1|Courier|14|0|-1|0.2|aGVsbG8=|d29ybGQ=|#ff404040|#ff404040|0|0");
        QTest::newRow("FILTERDISPLAY") << QStringLiteral("FILTERDISPLAY")
            << QStringLiteral("FILTERDISPLAY|0.1|0.2|0.3|0.4|5|6|1|#ff010203|#ff040506|#ff070809|#ff0a0b0c"
               "|#ff0d0e0f|#ff101112|0|0.1|-120|-30|1|6");
        QTest::newRow("BANDBTNS") << QStringLiteral("BANDBTNS") << QStringLiteral("BANDBTNS|0.1|0.2|0.3|0.4|5|6|3|2|255");
        // LSB to AM: buttons the old-order remap leaves where they were.
        QTest::newRow("MODEBTNS") << QStringLiteral("MODEBTNS") << QStringLiteral("MODEBTNS|0.1|0.2|0.3|0.4|5|6|3|2|127");
        QTest::newRow("FILTERBTNS") << QStringLiteral("FILTERBTNS") << QStringLiteral("FILTERBTNS|0.1|0.2|0.3|0.4|5|6|3|2|255");
        // Only 1 Hz shown and active: the button the old seven-step remap
        // leaves where it was.
        QTest::newRow("TUNESTEPBTNS") << QStringLiteral("TUNESTEPBTNS") << QStringLiteral("TUNESTEPBTNS|0.1|0.2|0.3|0.4|5|6|3|0|1");
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

    // A new clock takes the Thetis look; one saved before the layout
    // existed keeps the NereusSDR layout, its colours and no fill.
    void clockNewItemThetisOldItemUnchanged()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("CLOCK"));
        std::unique_ptr<MeterItem> made(registry.createMeterItem(entry, nullptr));
        const auto* c = qobject_cast<const ClockItem*>(made.get());
        QVERIFY(c);
        QCOMPARE(c->layout(), ClockItem::Layout::Thetis);
        QCOMPARE(c->timeColour(), QColor::fromRgb(kClockThetisDefaults.timeColour));
        QCOMPARE(c->backColour(), QColor::fromRgb(kClockThetisDefaults.backColour));
        QVERIFY(c->backEnabled());

        const QString oldRecord = QStringLiteral("CLOCK|0.1|0.2|0.3|0.4|5|6|0|1|#ff112233|#ff445566|#ff778899");
        ContentEntry saved = registry.makeEntry(QStringLiteral("CLOCK"));
        saved.config.insert(QStringLiteral("legacyRecord"), oldRecord);
        std::unique_ptr<MeterItem> restored(registry.createMeterItem(saved, nullptr));
        const auto* o = qobject_cast<const ClockItem*>(restored.get());
        QVERIFY(o);
        const QStringList oldParts = oldRecord.split(QLatin1Char('|'));
        const QStringList newParts = restored->serialize().split(QLatin1Char('|'));
        QCOMPARE(newParts.mid(7, oldParts.size() - 7), oldParts.mid(7));
        QCOMPARE(o->layout(), ClockItem::Layout::NereusSDR);
        QCOMPARE(o->timeColour(), QColor(0x11, 0x22, 0x33));
        QVERIFY(!o->backEnabled());
    }

    // A new mode grid has Thetis's twelve modes in DSPMode order, and
    // button i sets DSPMode (LSB + i).
    void modeGridHasTwelveModesInDspModeOrder()
    {
        ModeButtonItem grid;
        QCOMPARE(grid.buttonCount(), 12);
        const QStringList labels{"LSB", "USB", "DSB", "CWL", "CWU", "FM", "AM",
                                 "DIGU", "SPEC", "DIGL", "SAM", "DRM"};
        for (int i = 0; i < labels.size(); ++i) {
            QCOMPARE(ModeButtonItem::modeLabel(i), labels[i]);
            QCOMPARE(ModeButtonItem::modeForIndex(i), static_cast<DSPMode>(i));
            QVERIFY(grid.isButtonShown(i));
        }
        QVERIFY(ModeButtonItem::modeLabel(12).isEmpty());
        QCOMPARE(ModeButtonItem::modeForIndex(10), DSPMode::SAM);
        QCOMPARE(ModeButtonItem::modeForIndex(8), DSPMode::SPEC);
        QCOMPARE(grid.columnRange(), std::make_pair(1, 12));
    }

    // A grid saved in the old ten-button order (LSB, USB, DSB, CWL, CWU,
    // FM, AM, SAM, DIGL, DIGU) keeps its visible buttons and active mode:
    // SAM 7 -> 10, DIGL 8 -> 9, DIGU 9 -> 7; SPEC and DRM start hidden.
    void oldModeGridRemapsToDspModeOrder()
    {
        // LSB, USB, SAM, DIGL, DIGU visible; DSB to AM hidden; DIGU active.
        const QString oldRecord = QStringLiteral("MODEBTNS|0.1|0.2|0.3|0.4|5|6|4|9|899");
        ModeButtonItem grid;
        QVERIFY(grid.deserialize(oldRecord));
        QCOMPARE(grid.activeMode(), 7);
        QCOMPARE(ModeButtonItem::modeForIndex(grid.activeMode()), DSPMode::DIGU);
        QCOMPARE(grid.visibleBits(), uint32_t((1u << 0) | (1u << 1) | (1u << 7) | (1u << 9) | (1u << 10)));
        for (int hidden : {2, 3, 4, 5, 6, 8, 11}) { QVERIFY2(!grid.isButtonShown(hidden), qPrintable(QString::number(hidden))); }
        QCOMPARE(grid.columns(), 4);

        // Old SAM and DIGL active move too.
        ModeButtonItem sam;
        QVERIFY(sam.deserialize(QStringLiteral("MODEBTNS|0|0|1|1|0|0|5|7|1023")));
        QCOMPARE(sam.activeMode(), 10);
        ModeButtonItem digl;
        QVERIFY(digl.deserialize(QStringLiteral("MODEBTNS|0|0|1|1|0|0|5|8|1023")));
        QCOMPARE(digl.activeMode(), 9);
        // Every old button shown: all but SPEC and DRM.
        for (int i = 0; i < ModeButtonItem::kModeCount; ++i) {
            QCOMPARE(sam.isButtonShown(i), i != 8 && i != 11);
        }

        // An old record with the grid styling and no marker is old too.
        ModeButtonItem styled;
        QVERIFY(styled.deserialize(QStringLiteral("MODEBTNS|0|0|1|1|0|0|5|9|4294967295|0.01|0.02|4|0.75|0|0")));
        QCOMPARE(styled.activeMode(), 7);
        QVERIFY(!styled.isButtonShown(8));
        QVERIFY(!styled.isButtonShown(11));
        QVERIFY(styled.isButtonShown(10));

        // The new record carries the order marker and is never remapped
        // again: a save and reload keeps every index.
        const QString saved = grid.serialize();
        QVERIFY2(saved.endsWith(QStringLiteral("|DSPORDER")), qPrintable(saved));
        ModeButtonItem reloaded;
        QVERIFY(reloaded.deserialize(saved));
        QCOMPARE(reloaded.activeMode(), 7);
        QCOMPARE(reloaded.visibleBits(), grid.visibleBits());
        QCOMPARE(reloaded.serialize(), saved);
        ModeButtonItem fresh;
        fresh.setActiveMode(10);
        ModeButtonItem freshCopy;
        QVERIFY(freshCopy.deserialize(fresh.serialize()));
        QCOMPARE(freshCopy.activeMode(), 10);
        QVERIFY(freshCopy.isButtonShown(8));
        QVERIFY(freshCopy.isButtonShown(11));
    }

    // The same remap through a saved layout's content entry.
    void savedLayoutOldModeGridRemaps()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("MODEBTNS"));
        entry.config.insert(QStringLiteral("legacyRecord"), QStringLiteral("MODEBTNS|0.1|0.2|0.3|0.4|5|6|4|9|899"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* grid = qobject_cast<ModeButtonItem*>(item.get());
        QVERIFY(grid);
        QCOMPARE(grid->activeMode(), 7);
        QVERIFY(grid->isButtonShown(9));
        QVERIFY(grid->isButtonShown(10));
        QVERIFY(!grid->isButtonShown(8));
        QVERIFY(!grid->isButtonShown(11));
        // A new grid from the registry shows all twelve.
        std::unique_ptr<MeterItem> fresh(registry.createMeterItem(registry.makeEntry(QStringLiteral("MODEBTNS")), nullptr));
        auto* freshGrid = qobject_cast<ModeButtonItem*>(fresh.get());
        QVERIFY(freshGrid);
        QCOMPARE(freshGrid->buttonCount(), 12);
        for (int i = 0; i < 12; ++i) { QVERIFY(freshGrid->isButtonShown(i)); }
    }

    // The editor shows a row per mode; ticking SPEC shows it on an old grid.
    void modeEditorShowsEachMode()
    {
        ModeButtonItem grid;
        QVERIFY(grid.deserialize(QStringLiteral("MODEBTNS|0|0|1|1|0|0|5|0|1023")));
        ModeButtonItemEditor editor;
        editor.setItem(&grid);
        QSignalSpy changed(&editor, &BaseItemEditor::propertyChanged);
        for (int i = 0; i < ModeButtonItem::kModeCount; ++i) {
            auto* check = editor.findChild<QCheckBox*>(QStringLiteral("modeVisible_%1").arg(i));
            QVERIFY2(check, qPrintable(QString::number(i)));
            QCOMPARE(check->isChecked(), i != 8 && i != 11);
        }
        QCOMPARE(changed.count(), 0);
        editor.findChild<QCheckBox*>(QStringLiteral("modeVisible_8"))->setChecked(true);
        QVERIFY(grid.isButtonShown(8));
        QCOMPARE(changed.count(), 1);
        editor.findChild<QCheckBox*>(QStringLiteral("modeVisible_0"))->setChecked(false);
        QVERIFY(!grid.isButtonShown(0));
        QVERIFY(grid.isButtonShown(1));
    }

    // A new tune step grid has Thetis's 26 steps, labels without "Hz", and
    // shows the first ten.
    void tuneStepGridHasTwentySixSteps()
    {
        TuneStepButtonItem grid;
        QCOMPARE(grid.buttonCount(), 26);
        QCOMPARE(TuneStepButtonItem::kStepCount, kTuneStepListSize);
        for (int i = 0; i < 26; ++i) {
            QCOMPARE(grid.isButtonShown(i), i < 10);
            QCOMPARE(TuneStepButtonItem::stepHz(i), kTuneStepList[i].stepHz);
        }
        QCOMPARE(grid.visibleBits(), uint32_t(1023));
        QCOMPARE(TuneStepButtonItem::stepLabel(0), QStringLiteral("1"));
        QCOMPARE(TuneStepButtonItem::stepLabel(8), QStringLiteral("1k"));
        QCOMPARE(TuneStepButtonItem::stepLabel(10), QStringLiteral("2.5k"));
        QCOMPARE(TuneStepButtonItem::stepLabel(12), QStringLiteral("6.25k"));
        QCOMPARE(TuneStepButtonItem::stepLabel(24), QStringLiteral("1M"));
        QCOMPARE(TuneStepButtonItem::stepLabel(25), QStringLiteral("10M"));
        QVERIFY(TuneStepButtonItem::stepLabel(26).isEmpty());
        QCOMPARE(TuneStepButtonItem::stepHz(26), 0);
        QCOMPARE(TuneStepButtonItem::stepHz(-1), 0);
        QCOMPARE(grid.columnRange(), std::make_pair(1, 10));
    }

    // A grid saved with the old seven steps (1, 10, 100, 1k, 10k, 100k, 1M)
    // keeps its visible steps and active step at their 26-step places:
    // 0, 2, 5, 8, 14, 21, 24.
    void oldTuneStepGridRemapsToStepList()
    {
        // 1k active; 1, 100, 1k and 1M shown.
        const QString oldRecord = QStringLiteral("TUNESTEPBTNS|0.1|0.2|0.3|0.4|5|6|4|3|77");
        TuneStepButtonItem grid;
        QVERIFY(grid.deserialize(oldRecord));
        QCOMPARE(grid.activeStep(), 8);
        QCOMPARE(TuneStepButtonItem::stepHz(grid.activeStep()), 1000);
        QCOMPARE(grid.visibleBits(), uint32_t((1u << 0) | (1u << 5) | (1u << 8) | (1u << 24)));
        QCOMPARE(grid.columns(), 4);

        // Every old step shown, each old active index.
        const int expected[] = {0, 2, 5, 8, 14, 21, 24};
        for (int old = 0; old < 7; ++old) {
            TuneStepButtonItem all;
            QVERIFY(all.deserialize(QStringLiteral("TUNESTEPBTNS|0|0|1|1|0|0|4|%1|127").arg(old)));
            QCOMPARE(all.activeStep(), expected[old]);
            for (int i = 0; i < 26; ++i) {
                QCOMPARE(all.isButtonShown(i), std::find(std::begin(expected), std::end(expected), i) != std::end(expected));
            }
        }
        // No step active stays so.
        TuneStepButtonItem none;
        QVERIFY(none.deserialize(QStringLiteral("TUNESTEPBTNS|0|0|1|1|0|0|4|-1|127")));
        QCOMPARE(none.activeStep(), -1);
        // A record without the visible field showed all seven.
        TuneStepButtonItem bare;
        QVERIFY(bare.deserialize(QStringLiteral("TUNESTEPBTNS|0|0|1|1|0|0|4|6")));
        QCOMPARE(bare.activeStep(), 24);
        QCOMPARE(shownCount(bare), 7);
        QVERIFY(bare.isButtonShown(14));
        QVERIFY(!bare.isButtonShown(1));

        // An old record with the grid styling and no marker is old too.
        TuneStepButtonItem styled;
        QVERIFY(styled.deserialize(QStringLiteral("TUNESTEPBTNS|0|0|1|1|0|0|4|3|4294967295|0.01|0.02|4|0.75|0|0")));
        QCOMPARE(styled.activeStep(), 8);
        QVERIFY(styled.isButtonShown(8));
        QVERIFY(!styled.isButtonShown(3));

        // The new record carries the marker and is never remapped again.
        const QString saved = grid.serialize();
        QVERIFY2(saved.endsWith(QStringLiteral("|STEPLIST")), qPrintable(saved));
        TuneStepButtonItem reloaded;
        QVERIFY(reloaded.deserialize(saved));
        QCOMPARE(reloaded.activeStep(), 8);
        QCOMPARE(reloaded.visibleBits(), grid.visibleBits());
        QCOMPARE(reloaded.serialize(), saved);
        TuneStepButtonItem fresh;
        fresh.setActiveStep(3);
        TuneStepButtonItem freshCopy;
        QVERIFY(freshCopy.deserialize(fresh.serialize()));
        QCOMPARE(freshCopy.activeStep(), 3);
        QCOMPARE(freshCopy.visibleBits(), uint32_t(1023));
    }

    // The same remap through a saved layout's content entry.
    void savedLayoutOldTuneStepGridRemaps()
    {
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(QStringLiteral("TUNESTEPBTNS"));
        entry.config.insert(QStringLiteral("legacyRecord"), QStringLiteral("TUNESTEPBTNS|0.1|0.2|0.3|0.4|5|6|4|3|77"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* grid = qobject_cast<TuneStepButtonItem*>(item.get());
        QVERIFY(grid);
        QCOMPARE(grid->activeStep(), 8);
        QCOMPARE(shownCount(*grid), 4);
        QVERIFY(grid->isButtonShown(24));
        // A new grid from the registry has 26 buttons with the first ten shown.
        std::unique_ptr<MeterItem> fresh(registry.createMeterItem(registry.makeEntry(QStringLiteral("TUNESTEPBTNS")), nullptr));
        auto* freshGrid = qobject_cast<TuneStepButtonItem*>(fresh.get());
        QVERIFY(freshGrid);
        QCOMPARE(freshGrid->buttonCount(), 26);
        for (int i = 0; i < 26; ++i) { QCOMPARE(freshGrid->isButtonShown(i), i < 10); }
    }

    // The editor shows a row per step; ticking one shows it.
    void tuneStepEditorShowsEachStep()
    {
        TuneStepButtonItem grid;
        TuneStepButtonItemEditor editor;
        editor.setItem(&grid);
        QSignalSpy changed(&editor, &BaseItemEditor::propertyChanged);
        for (int i = 0; i < TuneStepButtonItem::kStepCount; ++i) {
            auto* check = editor.findChild<QCheckBox*>(QStringLiteral("stepVisible_%1").arg(i));
            QVERIFY2(check, qPrintable(QString::number(i)));
            QCOMPARE(check->isChecked(), i < 10);
        }
        // Each row is named as the step list names it.
        QStringList labels;
        for (const QLabel* label : editor.findChildren<QLabel*>()) { labels << label->text(); }
        for (int i = 0; i < TuneStepButtonItem::kStepCount; ++i) {
            QVERIFY2(labels.contains(QString::fromLatin1(kTuneStepList[i].name)), kTuneStepList[i].name);
        }
        QCOMPARE(changed.count(), 0);
        editor.findChild<QCheckBox*>(QStringLiteral("stepVisible_25"))->setChecked(true);
        QVERIFY(grid.isButtonShown(25));
        QCOMPARE(changed.count(), 1);
        editor.findChild<QCheckBox*>(QStringLiteral("stepVisible_0"))->setChecked(false);
        QVERIFY(!grid.isButtonShown(0));
        QVERIFY(grid.isButtonShown(1));
        QCOMPARE(grid.columnRange(), std::make_pair(1, 10));
    }
};

QTEST_MAIN(TstContainerItemPersistence)
#include "tst_container_item_persistence.moc"
