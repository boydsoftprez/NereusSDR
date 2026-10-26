// no-port-check: NereusSDR-original tests for the VFO display item's slice
// modes, Thetis colours, paint and published readings.
// Modification history (NereusSDR):
//   2026-10-26 - Display modes by slice letter (saved VFO B and Both),
//                 the Thetis import map, new-item defaults, the inactive
//                 slice fade, band text, the split box and its reason,
//                 sliceX_* readings and the editor wording. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QAbstractButton>
#include <QColor>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <memory>

#include "core/UnbuiltFeatureList.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/VfoDisplayItemEditor.h"
#include "gui/meters/VfoDisplayItem.h"

using namespace NereusSDR;

namespace {

constexpr int kW = 400;
constexpr int kH = 80;

VfoSliceState openSlice(int index, qint64 hz, const QString& mode, bool active)
{
    VfoSliceState s;
    s.index = index;
    s.frequencyHz = hz;
    s.mode = mode;
    s.filter = QStringLiteral("2.7k");
    s.band = QStringLiteral("20m");
    s.stepHz = 100;
    s.active = active;
    s.open = true;
    return s;
}

QImage render(VfoDisplayItem& item, QColor fill = Qt::transparent)
{
    QImage image(kW, kH, QImage::Format_ARGB32);
    image.fill(fill);
    QPainter p(&image);
    item.paint(p, kW, kH);
    p.end();
    return image;
}

int maxAlpha(const QImage& image, const QRect& area)
{
    int alpha = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            alpha = qMax(alpha, qAlpha(image.pixel(x, y)));
        }
    }
    return alpha;
}

int countColour(const QImage& image, const QColor& colour, const QRect& area)
{
    int n = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (image.pixelColor(x, y) == colour) { ++n; }
        }
    }
    return n;
}

} // namespace

class TstVfoDisplayItemModes : public QObject {
    Q_OBJECT

private slots:
    void init() { UnbuiltFeatures::resetForTest(); }
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    // A saved Both (2) loads as All slices and keeps 2.
    void savedBothLoadsAsAllSlices()
    {
        VfoDisplayItem item;
        QVERIFY(item.deserialize(QStringLiteral("VFO|0|0|1|1|-1|0|2")));
        QCOMPARE(item.displayMode(), VfoDisplayItem::DisplayMode::AllSlices);
        QCOMPARE(item.serialize().split(QLatin1Char('|')).at(7), QStringLiteral("2"));
    }

    // A saved VFO B (1) shows Slice B and re-saves as fixed Slice B (4).
    void savedVfoBLoadsAsSliceB()
    {
        VfoDisplayItem item;
        QVERIFY(item.deserialize(QStringLiteral("VFO|0|0|1|1|-1|0|1")));
        QCOMPARE(item.displayModeValue(), VfoDisplayItem::fixedSliceMode(1));
        QCOMPARE(item.displayModeValue(), 4);
        QCOMPARE(item.displayedSlices(), QVector<int>{1});
        QCOMPARE(item.serialize().split(QLatin1Char('|')).at(7), QStringLiteral("4"));

        VfoDisplayItem reloaded;
        QVERIFY(reloaded.deserialize(item.serialize()));
        QCOMPARE(reloaded.displayModeValue(), 4);
    }

    void unknownModeLoadsAsSourceSlice()
    {
        VfoDisplayItem item;
        QVERIFY(item.deserialize(QStringLiteral("VFO|0|0|1|1|-1|0|99")));
        QCOMPARE(item.displayMode(), VfoDisplayItem::DisplayMode::SourceSlice);
    }

    void thetisImportMap()
    {
        QCOMPARE(VfoDisplayItem::fromThetisDisplayMode(1), VfoDisplayItem::fixedSliceMode(0));
        QCOMPARE(VfoDisplayItem::fromThetisDisplayMode(0), int(VfoDisplayItem::DisplayMode::AllSlices));
        QCOMPARE(VfoDisplayItem::fromThetisDisplayMode(2), VfoDisplayItem::fixedSliceMode(1));
        // Out of range clamps to BOTH (MeterManager.cs:28073).
        QCOMPARE(VfoDisplayItem::fromThetisDisplayMode(7), int(VfoDisplayItem::DisplayMode::AllSlices));
        QCOMPARE(VfoDisplayItem::fromThetisDisplayMode(-1), int(VfoDisplayItem::DisplayMode::AllSlices));
    }

    void displayedSlicesPerMode()
    {
        VfoDisplayItem item;
        item.setSliceState(0, openSlice(0, 14200000, QStringLiteral("USB"), true));
        item.setSliceState(2, openSlice(2, 7100000, QStringLiteral("LSB"), false));
        item.setSourceSliceIndex(2);
        QCOMPARE(item.displayedSlices(), QVector<int>{2});
        item.setDisplayModeValue(VfoDisplayItem::fixedSliceMode(1));
        QCOMPARE(item.displayedSlices(), QVector<int>{1});
        QVERIFY(!item.sliceState(1).open);
        item.setDisplayMode(VfoDisplayItem::DisplayMode::AllSlices);
        QCOMPARE(item.displayedSlices(), (QVector<int>{0, 1, 2}));
    }

    // A new item starts on Source slice with the Thetis colours; an old
    // record keeps the #0a0a18 background and the compact layout.
    void newItemThetisOldItemUnchanged()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("VFO"));
        std::unique_ptr<MeterItem> made(registry.createMeterItem(entry, nullptr));
        const auto* v = qobject_cast<const VfoDisplayItem*>(made.get());
        QVERIFY(v);
        const VfoDisplayThetisDefaults& d = kVfoDisplayThetisDefaults;
        QCOMPARE(v->displayMode(), VfoDisplayItem::DisplayMode::SourceSlice);
        QCOMPARE(v->layout(), VfoDisplayItem::Layout::Thetis);
        QCOMPARE(v->frequencyColour(), QColor(255, 165, 0));
        QCOMPARE(v->frequencySmallColour(), QColor(255, 165, 0));
        QCOMPARE(v->modeColour(), QColor(128, 128, 128));
        QCOMPARE(v->filterColour(), QColor(128, 128, 128));
        QCOMPARE(v->titleColour(), QColor(128, 128, 128));
        QCOMPARE(v->splitBackColour(), QColor(64, 64, 64));
        QCOMPARE(v->splitColour(), QColor(255, 165, 0));
        QCOMPARE(v->rxColour(), QColor(50, 205, 50));
        QCOMPARE(v->txColour(), QColor(255, 0, 0));
        QCOMPARE(v->bandColour(), QColor(255, 255, 255));
        QCOMPARE(v->digitHighlightColour(), QColor(128, 128, 128));
        QCOMPARE(v->bandTextColour(), QColor(50, 205, 50));
        QCOMPARE(v->lockColour(), QColor(50, 205, 50));
        QCOMPARE(v->syncColour(), QColor(50, 205, 50));
        QCOMPARE(v->backgroundColour(), QColor::fromRgb(d.backColour));
        QVERIFY(!v->showBandText());

        const QString oldRecord = QStringLiteral("VFO|0.1|0.2|0.3|0.4|5|6|0|#ff010203|#ff040506|#ff070809|#ff0a0b0c|#ff0d0e0f|#ff101112");
        ContentEntry saved = registry.makeEntry(QStringLiteral("VFO"));
        saved.config.insert(QStringLiteral("legacyRecord"), oldRecord);
        std::unique_ptr<MeterItem> restored(registry.createMeterItem(saved, nullptr));
        const auto* o = qobject_cast<const VfoDisplayItem*>(restored.get());
        QVERIFY(o);
        QCOMPARE(o->backgroundColour(), QColor(0x0a, 0x0a, 0x18));
        QCOMPARE(o->layout(), VfoDisplayItem::Layout::Compact);
        QCOMPARE(o->frequencyColour(), QColor(0x01, 0x02, 0x03));
        QCOMPARE(restored->serialize().split(QLatin1Char('|')).mid(7, 7), oldRecord.split(QLatin1Char('|')).mid(7));
    }

    // With Slices A and B open and B active, A draws at alpha 24.
    void inactiveSliceDrawsAtAlpha24_data()
    {
        QTest::addColumn<int>("layout");
        QTest::newRow("compact") << int(VfoDisplayItem::Layout::Compact);
        QTest::newRow("thetis") << int(VfoDisplayItem::Layout::Thetis);
    }
    void inactiveSliceDrawsAtAlpha24()
    {
        QFETCH(int, layout);
        VfoDisplayItem item;
        item.setRect(0, 0, 1, 1);
        item.setLayout(static_cast<VfoDisplayItem::Layout>(layout));
        item.setBackgroundColour(Qt::transparent);
        item.setDisplayMode(VfoDisplayItem::DisplayMode::AllSlices);
        item.setSliceState(0, openSlice(0, 14200000, QStringLiteral("USB"), false));
        item.setSliceState(1, openSlice(1, 7100000, QStringLiteral("LSB"), true));
        item.setSourceSliceIndex(1);
        QCOMPARE(item.displayedSlices(), (QVector<int>{0, 1}));
        const QImage image = render(item);
        // Inside the 1 px compact frame.
        const QRect sliceA(2, 2, kW / 2 - 4, kH - 4);
        const QRect sliceB(kW / 2 + 2, 2, kW / 2 - 4, kH - 4);
        const int alphaA = maxAlpha(image, sliceA);
        if (layout == int(VfoDisplayItem::Layout::Compact)) {
            QVERIFY2(alphaA > 0 && alphaA <= VfoDisplayItem::kInactiveSliceAlpha + 1, qPrintable(QString::number(alphaA)));
        } else {
            // Thetis draws each part at nFade, so a part over another
            // compounds; the split box fill alone is at 24.
            QCOMPARE(qAlpha(image.pixel(78, 32)), VfoDisplayItem::kInactiveSliceAlpha);
            QVERIFY2(alphaA < 64, qPrintable(QString::number(alphaA)));
        }
        QCOMPARE(maxAlpha(image, sliceB), 255);
    }

    // Band text is drawn only when it is on.
    void bandTextOnlyWhenEnabled()
    {
        VfoDisplayItem item;
        item.setRect(0, 0, 1, 1);
        item.setLayout(VfoDisplayItem::Layout::Thetis);
        const QColor magenta(255, 0, 255);
        item.setBandTextColour(magenta);
        VfoSliceState s = openSlice(0, 14200000, QStringLiteral("USB"), true);
        s.bandText = QStringLiteral("20M Phone");
        item.setSliceState(0, s);
        item.setSourceSliceIndex(0);
        const QRect all(0, 0, kW, kH);
        QCOMPARE(countColour(render(item, Qt::black), magenta, all), 0);
        item.setShowBandText(true);
        QVERIFY(countColour(render(item, Qt::black), magenta, all) > 0);
    }

    // The split box draws greyed (split back fill, the text in Thetis's
    // split-off black) and says why.
    void splitBoxGreyedWithReason()
    {
        VfoDisplayItem item;
        item.setRect(0, 0, 1, 1);
        item.setLayout(VfoDisplayItem::Layout::Thetis);
        const QColor back(10, 200, 220);
        const QColor splitOn(0, 0, 255);
        item.setSplitBackColour(back);
        item.setSplitColour(splitOn);
        item.setSliceState(0, openSlice(0, 14200000, QStringLiteral("USB"), true));
        item.setSourceSliceIndex(0);
        const QImage image = render(item, Qt::black);
        // Single slice: x = 0.1 to 0.2 of 2.07 x the width, y = 0.03 to 0.43 h.
        const QRect box(int(0.1 * 2.07 * kW) + 2, int(0.03 * kH) + 1,
                        int(0.1 * 2.07 * kW) - 4, int(0.4 * kH) - 2);
        QVERIFY(countColour(image, back, box) > box.width() * box.height() / 3);
        QCOMPARE(countColour(image, splitOn, QRect(0, 0, kW, kH)), 0);
        QCOMPARE(item.reasonAt(QPointF(box.center()), kW, kH), QStringLiteral("Split is not available."));
        QCOMPARE(item.reasonAt(QPointF(2, kH - 2), kW, kH), QString());
        // The sync icon is greyed and says why.
        QCOMPARE(item.reasonAt(QPointF(0.237 * 2.07 * kW, 0.7 * kH), kW, kH),
                 QStringLiteral("Sync is not available."));
    }

    void publishedReadingsBySliceLetter()
    {
        VfoDisplayItem item;
        QVERIFY(item.publishedReadings().isEmpty());   // nothing fed
        item.setSliceState(0, openSlice(0, 14200000, QStringLiteral("USB"), true));
        item.setSourceSliceIndex(0);
        QHash<QString, QVariant> r = item.publishedReadings();
        QCOMPARE(r.value(QStringLiteral("sliceA_freq")).toString(), QStringLiteral("14.200000"));
        QCOMPARE(r.value(QStringLiteral("sliceA_mode")).toString(), QStringLiteral("USB"));
        QCOMPARE(r.value(QStringLiteral("sliceA_filter")).toString(), QStringLiteral("2.7k"));
        QCOMPARE(r.value(QStringLiteral("sliceA_band")).toString(), QStringLiteral("20m"));
        QCOMPARE(r.value(QStringLiteral("sliceA_step")).toInt(), 100);
        for (auto it = r.cbegin(); it != r.cend(); ++it) { QVERIFY(!it.key().contains(QLatin1String("vfo"))); }

        item.setSliceState(1, openSlice(1, 7100000, QStringLiteral("LSB"), false));
        item.setDisplayMode(VfoDisplayItem::DisplayMode::AllSlices);
        r = item.publishedReadings();
        QCOMPARE(r.value(QStringLiteral("sliceB_mode")).toString(), QStringLiteral("LSB"));
        QCOMPARE(r.value(QStringLiteral("sliceB_freq")).toString(), QStringLiteral("7.100000"));
    }

    // The editor names slices; no VFO A / VFO B / RX1 / RX2.
    void editorWording()
    {
        VfoDisplayItem item;
        VfoDisplayItemEditor editor;
        editor.setItem(&item);
        QStringList texts;
        for (const QLabel* label : editor.findChildren<QLabel*>()) { texts << label->text(); }
        for (const QAbstractButton* button : editor.findChildren<QAbstractButton*>()) { texts << button->text(); }
        auto* mode = editor.findChild<QComboBox*>(QStringLiteral("vfoDisplayMode"));
        QVERIFY(mode);
        for (const QComboBox* combo : editor.findChildren<QComboBox*>()) {
            for (int i = 0; i < combo->count(); ++i) { texts << combo->itemText(i); }
        }
        for (const QString& text : texts) {
            for (const char* banned : {"VFO A", "VFO B", "RX1", "RX2", "Both"}) {
                QVERIFY2(!text.contains(QLatin1String(banned)), qPrintable(text));
            }
        }
        QCOMPARE(mode->itemText(0), QStringLiteral("Source slice"));
        QCOMPARE(mode->itemText(1), QStringLiteral("All slices"));
        QCOMPARE(mode->itemText(2), QStringLiteral("Slice A"));
        QCOMPARE(mode->count(), 2 + VfoDisplayItem::kMaxSliceCount);
        QCOMPARE(mode->currentData().toInt(), int(VfoDisplayItem::DisplayMode::SourceSlice));

        mode->setCurrentIndex(mode->findData(VfoDisplayItem::fixedSliceMode(2)));
        QCOMPARE(item.displayModeValue(), VfoDisplayItem::fixedSliceMode(2));

        item.setFrequencyColour(QColor(1, 2, 3));
        auto* copy = editor.findChild<QPushButton*>(QStringLiteral("vfoCopyToSmallDigits"));
        QVERIFY(copy);
        copy->click();
        QCOMPARE(item.frequencySmallColour(), QColor(1, 2, 3));
    }
};

QTEST_MAIN(TstVfoDisplayItemModes)
#include "tst_vfo_display_item_modes.moc"
