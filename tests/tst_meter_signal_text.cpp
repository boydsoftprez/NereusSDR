// no-port-check: NereusSDR-original tests for the signal text options; the
// expected values are Thetis's, cited beside each check.
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave R2: units set on an item stay when the global
//                 setting changes; others and old records follow it; the
//                 editor's Follow global. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Signal text preset and building block: the PK / AVG / FFT
//                 type glyph, the uV and dBm sub lines, the peak value
//                 without peak hold, the background, history, the saved
//                 record and new-item defaults. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>

#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/SignalTextItemEditor.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/presets/SignalTextPresetItem.h"

using namespace NereusSDR;

namespace {

constexpr int kW = 400;
constexpr int kH = 200;

std::unique_ptr<SignalTextItem> makeText(bool thetis, int binding = MeterBinding::SignalAvg)
{
    auto text = std::make_unique<SignalTextItem>();
    if (thetis) { text->applyNewItemDefaults(); }
    text->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    text->setBindingId(binding);
    return text;
}

QImage render(MeterItem& item, int w = kW, int h = kH)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    item.paint(p, w, h);
    p.end();
    return img;
}

// Pixels within `tolerance` of `colour` inside `region`.
int countNear(const QImage& img, const QColor& colour, const QRect& region, int tolerance = 40)
{
    int n = 0;
    const QRect r = region.intersected(img.rect());
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (std::abs(c.red() - colour.red()) <= tolerance
                && std::abs(c.green() - colour.green()) <= tolerance
                && std::abs(c.blue() - colour.blue()) <= tolerance) {
                ++n;
            }
        }
    }
    return n;
}

void settle(MeterItem& item, double dbm, int times = 40)
{
    for (int i = 0; i < times; ++i) { item.setValue(dbm); }
}

void settlePreset(CompositePresetItem& face, double dbm)
{
    for (int i = 0; i < 40; ++i) {
        face.pushBindingValue(face.bindingId(), dbm);
        face.advanceMeter(1000 + i * 100);
    }
}

// The record a signal text saved before the options existed (18 fields).
const QString kOldRecord = QStringLiteral(
    "SIGNALTEXT|0|0|1|1|1|5|0|1|1|1|0|#ffff0000|#ffff0000|#ffffff00|Trebuchet MS|20|0");

const QColor kProbe(0x00, 0xc8, 0xff);   // a color no other ink blends into

} // namespace

class TstMeterSignalText : public QObject {
    Q_OBJECT

private slots:
    // --- Building block (SIGNALTEXT) -------------------------------------

    void typeGlyphNamesTheReading()
    {
        // From Thetis MeterManager.cs:40556-40576 [v2.10.3.15]
        QCOMPARE(SignalTextItem::readingTypeGlyph(MeterBinding::SignalPeak), QStringLiteral("PK"));
        QCOMPARE(SignalTextItem::readingTypeGlyph(MeterBinding::SignalAvg), QStringLiteral("AVG"));
        QCOMPARE(SignalTextItem::readingTypeGlyph(MeterBinding::SignalMaxBin), QStringLiteral("FFT"));
        QVERIFY(SignalTextItem::readingTypeGlyph(MeterBinding::TxPower).isEmpty());

        // Painted top right in the type color, one shape per reading.
        const QRect corner(kW * 3 / 5, 0, kW * 2 / 5, kH / 4);
        QImage glyph[3];
        const int bindings[3] = {MeterBinding::SignalPeak, MeterBinding::SignalAvg, MeterBinding::SignalMaxBin};
        for (int i = 0; i < 3; ++i) {
            auto text = makeText(true, bindings[i]);
            text->setShowValue(false);
            text->setShowPeakValue(false);
            text->setShowSubMarker(false);
            text->setTypeColour(kProbe);
            settle(*text, -80.0);
            glyph[i] = render(*text);
            QVERIFY2(countNear(glyph[i], kProbe, corner) > 0, "type glyph drawn top right");
            QCOMPARE(countNear(glyph[i], kProbe, QRect(0, kH / 2, kW, kH / 2)), 0);
        }
        QVERIFY(glyph[0] != glyph[1]);
        QVERIFY(glyph[1] != glyph[2]);
        QVERIFY(glyph[0] != glyph[2]);

        // Show type off draws no glyph; another reading draws none either.
        auto off = makeText(true);
        off->setShowValue(false);
        off->setShowPeakValue(false);
        off->setShowSubMarker(false);
        off->setTypeColour(kProbe);
        off->setShowType(false);
        QCOMPARE(countNear(render(*off), kProbe, corner), 0);
        auto power = makeText(true, MeterBinding::TxPower);
        power->setShowValue(false);
        power->setShowPeakValue(false);
        power->setShowSubMarker(false);
        power->setTypeColour(kProbe);
        QCOMPARE(countNear(render(*power), kProbe, corner), 0);
    }

    void subLinesShowUvAndDbmInTheSubColour()
    {
        auto text = makeText(true);
        settle(*text, -73.0);
        QCOMPARE(text->subDbmText(), QStringLiteral("-73.0 dBm"));
        // Common.UVfromDBM(-73) = 10^((-73 + 107) / 20) = 50.12 uV
        QCOMPARE(text->subUvText(), QStringLiteral("50.12 uV"));

        text->setShowValue(false);
        text->setShowPeakValue(false);
        text->setShowType(false);
        text->setSubMarkerColour(kProbe);
        QVERIFY(text->showSubMarker());
        const QImage on = render(*text);
        QVERIFY(countNear(on, kProbe, QRect(0, 0, kW / 2, kH)) > 0);       // dBm, left
        QVERIFY(countNear(on, kProbe, QRect(kW / 2, 0, kW / 2, kH)) > 0);  // uV, right
        text->setShowSubMarker(false);
        QCOMPARE(countNear(render(*text), kProbe, QRect(0, 0, kW, kH)), 0);

        // No reading: dashes with the unit.
        text->setValue(kNoMeterReadingDbm);
        QCOMPARE(text->subDbmText(), QStringLiteral("-- dBm"));
        QCOMPARE(text->subUvText(), QStringLiteral("-- uV"));
    }

    void peakValueShowsWithPeakHoldOff()
    {
        auto text = makeText(true);
        QVERIFY(!text->peakHold());
        QVERIFY(text->showPeakValue());
        QVERIFY(text->peakValueShown());
        settle(*text, -60.0);
        settle(*text, -100.0, 30);
        // The peak is the history peak (MaxHistory), not the held peak.
        QVERIFY(text->displayValue() < -99.0f);
        QVERIFY(std::abs(text->maxHistory() - (-60.0f)) < 0.01f);
        QCOMPARE(text->peakValueText(), QStringLiteral("-60.0 dBm"));
        QCOMPARE(text->peakSubDbmText(), QStringLiteral("-60.0 dBm"));

        text->setShowValue(false);
        text->setShowType(false);
        text->setShowSubMarker(false);
        text->setPeakValueColour(kProbe);
        QVERIFY(countNear(render(*text), kProbe, QRect(0, 0, kW, kH)) > 0);

        // Thetis's peak sub lines are in the peak color, beside it.
        text->setShowSubMarker(true);
        text->setSubMarkerColour(Qt::white);
        const QImage withSub = render(*text);
        QVERIFY(countNear(withSub, kProbe, QRect(0, 0, kW / 3, kH / 3)) > 0);
        QVERIFY(countNear(withSub, kProbe, QRect(kW * 2 / 3, 0, kW / 3, kH / 3)) > 0);

        // A saved record keeps the peak tied to peak hold.
        SignalTextItem old;
        QVERIFY(old.deserialize(kOldRecord));
        QVERIFY(old.peakValueNeedsPeakHold());
        QVERIFY(!old.peakValueShown());
        old.setPeakHold(true);
        QVERIFY(old.peakValueShown());
    }

    void historyHonoursDurationAndIgnore()
    {
        auto text = makeText(true);
        QCOMPARE(text->historyDurationMs(), 4000);
        settle(*text, -80.0, 60);
        // 4000 ms / 100 ms interval = 40 readings.
        QCOMPARE(text->historySize(), 40);
        text->clearHistory();
        QCOMPARE(text->historySize(), 0);
        QCOMPARE(text->ignoringReadings(), 20);   // 2000 ms / 100 ms
        settle(*text, -80.0, 20);
        QCOMPARE(text->historySize(), 0);
        text->setValue(-80.0);
        QCOMPARE(text->historySize(), 1);
    }

    void backgroundFills()
    {
        auto text = makeText(true);
        QCOMPARE(render(*text).pixelColor(1, 1), QColor(32, 32, 32));   // (:23012)
        text->setBackgroundColour(kProbe);
        QCOMPARE(render(*text).pixelColor(1, 1), kProbe);
        SignalTextItem old;
        QVERIFY(old.deserialize(kOldRecord));
        old.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QCOMPARE(render(old).pixelColor(1, 1), QColor(0x0a, 0x0a, 0x18));
    }

    void oldRecordPaintsTheSame()
    {
        SignalTextItem old;
        QVERIFY(old.deserialize(kOldRecord));
        QCOMPARE(old.typeStyle(), SignalTextItem::TypeStyle::UnitsLabel);
        QCOMPARE(old.typeColour(), QColor(0x80, 0x90, 0xa0));
        QVERIFY(!old.showSubMarker());
        QCOMPARE(old.backgroundColour(), QColor(0x0a, 0x0a, 0x18));

        // Paints as one built in code with the same fields (the look the
        // record always had), in every row arrangement it could save.
        for (bool peak : {false, true}) {
            for (bool type : {false, true}) {
                SignalTextItem built;
                built.setRect(0.0f, 0.0f, 1.0f, 1.0f);
                built.setBindingId(MeterBinding::SignalAvg);
                built.setShowType(type);
                built.setShowPeakValue(true);
                built.setPeakHold(peak);
                QString record = kOldRecord;
                record.replace(QStringLiteral("|0|1|1|1|0|#"),
                               QStringLiteral("|0|1|1|%1|%2|#").arg(type ? 1 : 0).arg(peak ? 1 : 0));
                SignalTextItem restored;
                QVERIFY(restored.deserialize(record));
                for (SignalTextItem* t : {&built, &restored}) {
                    settle(*t, -60.0);
                    settle(*t, -90.0, 3);
                }
                QCOMPARE(render(restored), render(built));
            }
        }

        // Saving an old record keeps its look.
        SignalTextItem again;
        QVERIFY(again.deserialize(old.serialize()));
        QCOMPARE(again.typeStyle(), SignalTextItem::TypeStyle::UnitsLabel);
        QVERIFY(again.peakValueNeedsPeakHold());
        QCOMPARE(again.backgroundColour(), QColor(0x0a, 0x0a, 0x18));
    }

    // Fix wave R2: an item with no unit set on it follows the global unit
    // setting; one set to dBm stays dBm.
    void unitsFollowGlobalUnlessSetOnTheItem()
    {
        MeterWidget widget;
        auto* follows = new SignalTextItem();
        auto* setHere = new SignalTextItem();
        widget.addItem(follows);
        widget.addItem(setHere);
        setHere->setOwnUnits(SignalTextItem::Units::Dbm);
        QVERIFY(!follows->unitsSetHere());
        QVERIFY(setHere->unitsSetHere());

        widget.setUnitMode(MeterItem::MeterUnit::S);
        QCOMPARE(follows->units(), SignalTextItem::Units::SUnits);
        QCOMPARE(setHere->units(), SignalTextItem::Units::Dbm);
        widget.setUnitMode(MeterItem::MeterUnit::uV);
        QCOMPARE(follows->units(), SignalTextItem::Units::Uv);
        QCOMPARE(setHere->units(), SignalTextItem::Units::Dbm);

        // Back to Follow global: the last global setting.
        setHere->followGlobalUnits();
        QCOMPARE(setHere->units(), SignalTextItem::Units::Uv);
        widget.setUnitMode(MeterItem::MeterUnit::S);
        QCOMPARE(setHere->units(), SignalTextItem::Units::SUnits);

        // A set unit is saved and comes back set.
        SignalTextItem saved;
        saved.setOwnUnits(SignalTextItem::Units::Dbm);
        SignalTextItem reloaded;
        QVERIFY(reloaded.deserialize(saved.serialize()));
        QVERIFY(reloaded.unitsSetHere());
        reloaded.setUnitMode(MeterItem::MeterUnit::S);
        QCOMPARE(reloaded.units(), SignalTextItem::Units::Dbm);
    }

    // An old record (no field 26) loads as before and follows the global
    // setting; written again it keeps every field and appends a 0.
    void oldRecordFollowsGlobalAndRoundTrips()
    {
        SignalTextItem old;
        QVERIFY(old.deserialize(kOldRecord));
        QCOMPARE(old.units(), SignalTextItem::Units::Dbm);
        QVERIFY(!old.unitsSetHere());
        const QString written = old.serialize();
        QCOMPARE(written.split(QLatin1Char('|')).mid(0, 18), kOldRecord.split(QLatin1Char('|')));
        QVERIFY(written.endsWith(QStringLiteral("|0")));
        SignalTextItem again;
        QVERIFY(again.deserialize(written));
        QCOMPARE(again.serialize(), written);
        again.setUnitMode(MeterItem::MeterUnit::S);
        QCOMPARE(again.units(), SignalTextItem::Units::SUnits);
    }

    // The editor's Units row: Follow global, then each unit.
    void editorOffersFollowGlobal()
    {
        SignalTextItem item;
        SignalTextItemEditor editor;
        editor.setItem(&item);
        auto* units = editor.findChild<QComboBox*>(QStringLiteral("signalUnits"));
        QVERIFY(units);
        QVERIFY(units->isEnabled());
        QCOMPARE(units->count(), 4);
        QCOMPARE(units->itemText(0), QStringLiteral("Follow global"));
        QCOMPARE(units->currentIndex(), 0);
        units->setCurrentIndex(units->findData(static_cast<int>(SignalTextItem::Units::Dbm)));
        QVERIFY(item.unitsSetHere());
        item.setUnitMode(MeterItem::MeterUnit::S);
        QCOMPARE(item.units(), SignalTextItem::Units::Dbm);
        units->setCurrentIndex(0);
        QVERIFY(!item.unitsSetHere());
        QCOMPARE(item.units(), SignalTextItem::Units::SUnits);
        editor.setItem(&item);
        QCOMPARE(units->currentIndex(), 0);
    }

    void newFieldsRoundTrip()
    {
        auto text = makeText(true);
        text->setTypeStyle(SignalTextItem::TypeStyle::UnitsLabel);
        text->setTypeColour(QColor(1, 2, 3));
        text->setShowSubMarker(false);
        text->setSubMarkerColour(QColor(4, 5, 6, 7));
        text->setBackgroundColour(QColor(8, 9, 10));
        text->setPeakValueNeedsPeakHold(true);
        text->setHistoryDurationMs(1500);
        text->setIgnoreHistoryMs(700);
        SignalTextItem copy;
        QVERIFY(copy.deserialize(text->serialize()));
        QCOMPARE(copy.typeStyle(), SignalTextItem::TypeStyle::UnitsLabel);
        QCOMPARE(copy.typeColour(), QColor(1, 2, 3));
        QVERIFY(!copy.showSubMarker());
        QCOMPARE(copy.subMarkerColour(), QColor(4, 5, 6, 7));
        QCOMPARE(copy.backgroundColour(), QColor(8, 9, 10));
        QVERIFY(copy.peakValueNeedsPeakHold());
        QCOMPARE(copy.historyDurationMs(), 1500);
        QCOMPARE(copy.ignoreHistoryMs(), 700);
        QCOMPARE(copy.serialize(), text->serialize());

        SignalTextItem thetis;
        QVERIFY(thetis.deserialize(makeText(true)->serialize()));
        QCOMPARE(thetis.typeStyle(), SignalTextItem::TypeStyle::ReadingGlyph);
        QVERIFY(!thetis.peakValueNeedsPeakHold());
    }

    void makeEntryTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("SIGNALTEXT"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* text = qobject_cast<SignalTextItem*>(item.get());
        QVERIFY(text);
        QCOMPARE(text->backgroundColour(), QColor(32, 32, 32));        // (:23012)
        QCOMPARE(text->bindingId(), MeterBinding::SignalAvg);           // (:23021)
        QCOMPARE(text->historyDurationMs(), 4000);                      // (:23026)
        QCOMPARE(text->ignoreHistoryMs(), 2000);                        // (:21648)
        QCOMPARE(text->subMarkerColour(), QColor(0x80, 0x80, 0x80));    // Gray (:23028)
        QVERIFY(text->showValue());                                     // (:23029)
        QVERIFY(text->showPeakValue());                                 // (:23030)
        QCOMPARE(text->colour(), QColor(0xff, 0xff, 0x00));             // Yellow (:23031)
        QCOMPARE(text->fontSize(), 56.0f);                              // (:23032)
        QCOMPARE(text->peakValueColour(), QColor(0xff, 0x00, 0x00));    // Red (:21655)
        QVERIFY(!text->peakHold());                                     // (:21656)
        QVERIFY(text->showSubMarker());                                 // (:21658)
        QVERIFY(text->showType());                                      // (:21667)
        QCOMPARE(text->typeColour(), QColor(0x80, 0x80, 0x80));         // Gray (:21668)
        QCOMPARE(text->typeStyle(), SignalTextItem::TypeStyle::ReadingGlyph);
        QVERIFY(!text->peakValueNeedsPeakHold());
    }

    void vhfSliceUsesS9AtMinus93()
    {
        auto text = makeText(true);
        text->setUnitMode(MeterItem::MeterUnit::S);
        settle(*text, -93.0);
        QCOMPARE(text->valueText(), QStringLiteral("S5"));
        QCOMPARE(text->peakValueText(), QStringLiteral("S5"));
        text->setAboveS9Frequency(true);
        QCOMPARE(text->valueText(), QStringLiteral("S9"));
        QCOMPARE(text->peakValueText(), QStringLiteral("S9"));
        // The sub lines are the raw reading at any frequency.
        QCOMPARE(text->subDbmText(), QStringLiteral("-93.0 dBm"));
    }

    void editorShowsTheOptions()
    {
        auto text = makeText(true);
        SignalTextItemEditor editor;
        editor.setItem(text.get());
        bool sawGlyph = false;
        for (QComboBox* combo : editor.findChildren<QComboBox*>()) {
            if (combo->currentData().toInt() == static_cast<int>(SignalTextItem::TypeStyle::ReadingGlyph)
                && combo->currentText().contains(QStringLiteral("PK"))) {
                sawGlyph = true;
            }
        }
        QVERIFY(sawGlyph);
        int checked = 0;
        for (QCheckBox* box : editor.findChildren<QCheckBox*>()) {
            if (box->isChecked()) { ++checked; }
        }
        QVERIFY(checked > 0);
        // Turning the sub line off in the editor reaches the item.
        for (QCheckBox* box : editor.findChildren<QCheckBox*>()) {
            box->setChecked(false);
        }
        QVERIFY(!text->showSubMarker());
    }

    // --- Preset (meter.signalText) ---------------------------------------

    void presetTypeGlyphFollowsTheReading()
    {
        const QRect corner(260 * 3 / 5, 0, 260 * 2 / 5, 120 / 3);
        QImage frames[3];
        const int bindings[3] = {MeterBinding::SignalPeak, MeterBinding::SignalAvg, MeterBinding::SignalMaxBin};
        for (int i = 0; i < 3; ++i) {
            SignalTextPresetItem face;
            face.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            QVERIFY(face.applyConfiguration({{"showType", true}, {"typeColor", kProbe.name(QColor::HexArgb)},
                                             {"showTitle", false}, {"bindingId", bindings[i]}}));
            settlePreset(face, -80.0);
            frames[i] = render(face, 260, 120);
            QVERIFY(countNear(frames[i], kProbe, corner) > 0);
        }
        QVERIFY(frames[0] != frames[1]);
        QVERIFY(frames[1] != frames[2]);
        SignalTextPresetItem off;
        off.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QVERIFY(off.applyConfiguration({{"typeColor", kProbe.name(QColor::HexArgb)}, {"showTitle", false}}));
        settlePreset(off, -80.0);
        QCOMPARE(countNear(render(off, 260, 120), kProbe, corner), 0);
    }

    void presetSubLinesAndPeakValue()
    {
        SignalTextPresetItem face;
        face.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QVERIFY(face.applyConfiguration({{"showSubMarker", true}, {"subMarkerColor", kProbe.name(QColor::HexArgb)},
                                         {"showPeakValue", false}}));
        settlePreset(face, -73.0);
        const QImage on = render(face, 260, 160);
        QVERIFY(countNear(on, kProbe, QRect(0, 0, 130, 160)) > 0);    // dBm, left
        QVERIFY(countNear(on, kProbe, QRect(130, 0, 130, 160)) > 0);  // uV, right
        QVERIFY(face.applyConfiguration({{"showSubMarker", false}}));
        QCOMPARE(countNear(render(face, 260, 160), kProbe, QRect(0, 0, 260, 160)), 0);

        // Peak value in its color, with peak hold off.
        const QColor peak(0xff, 0x00, 0xff);
        QVERIFY(face.applyConfiguration({{"showPeakValue", true}, {"peakValueColor", peak.name(QColor::HexArgb)}}));
        QVERIFY(!face.configuration()["channels"].toArray().first().toObject()["peakHold"].toBool());
        QVERIFY(countNear(render(face, 260, 160), peak, QRect(0, 80, 260, 80)) > 0);

        // Bad colors are refused.
        QVERIFY(!face.applyConfiguration({{"typeColor", "not a color"}}));
        QVERIFY(!face.applyConfiguration({{"subMarkerColor", 3}}));
        QVERIFY(!face.applyConfiguration({{"showType", "yes"}}));
    }

    void presetOldRecordKeepsItsLook()
    {
        SignalTextPresetItem built;
        built.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QJsonObject old = built.configuration();
        for (const char* key : {"showType", "typeColor", "showSubMarker", "subMarkerColor", "peakValueColor"}) {
            QVERIFY(old.contains(QLatin1String(key)));
            old.remove(QLatin1String(key));
        }
        SignalTextPresetItem restored;
        QVERIFY(restored.applyConfiguration(old));
        QVERIFY(!restored.configuration()["showType"].toBool());
        QVERIFY(!restored.configuration()["showSubMarker"].toBool());
        QVERIFY(restored.configuration()["peakValueColor"].toString().isEmpty());
        for (SignalTextPresetItem* f : {&built, &restored}) {
            settlePreset(*f, -80.0);
        }
        QCOMPARE(render(restored, 260, 160), render(built, 260, 160));
        QCOMPARE(restored.preferredFaceHeight(), built.preferredFaceHeight());
    }

    void presetNewItemTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("meter.signalText"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* face = qobject_cast<CompositePresetItem*>(item.get());
        QVERIFY(face);
        const QJsonObject c = face->configuration();
        QVERIFY(c["showType"].toBool());                                           // (:21667)
        QCOMPARE(QColor(c["typeColor"].toString()), QColor(0x80, 0x80, 0x80));     // (:21668)
        QVERIFY(c["showSubMarker"].toBool());                                      // (:21658)
        QCOMPARE(QColor(c["subMarkerColor"].toString()), QColor(0x80, 0x80, 0x80));// (:23028)
        QCOMPARE(QColor(c["peakValueColor"].toString()), QColor(0xff, 0x00, 0x00));// (:21655)
        QCOMPARE(QColor(c["backdropColor"].toString()), QColor(32, 32, 32));       // (:23012)
        QVERIFY(c["showPeakValue"].toBool());                                      // (:23030)
        QCOMPARE(c["fontSize"].toInt(), 56);                                       // (:23032)
        const QJsonObject signal = c["channels"].toArray().first().toObject();
        QCOMPARE(signal["bindingId"].toInt(), MeterBinding::SignalAvg);            // (:23021)
        QCOMPARE(signal["attack"].toDouble(), double(0.8f));                       // (:23023)
        QCOMPARE(signal["decay"].toDouble(), double(0.2f));                        // (:23024)
        QCOMPARE(signal["historyMs"].toInt(), 4000);                               // (:23026)
        QVERIFY(!signal["showHistory"].toBool());                                  // (:23027)
        QCOMPARE(QColor(signal["color"].toString()), QColor(0xff, 0xff, 0x00));    // (:23031)
        QVERIFY(!signal["peakHold"].toBool());
        QVERIFY(face->editableFields().contains(QStringLiteral("subMarkerColor")));
        QVERIFY(face->preferredFaceHeight() > 0);
    }
};

QTEST_MAIN(TstMeterSignalText)
#include "tst_meter_signal_text.moc"
