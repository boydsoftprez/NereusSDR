// no-port-check: NereusSDR-original tests; Thetis cites name the values checked.
// NereusSDR-original infrastructure — no Thetis source ported here.
// No upstream attribution required (NereusSDR FilterDisplayItem high-res test).
//
// Modification history (NereusSDR):
//   2026-10-26 - Palette load, Thetis new-item defaults and editor ranges.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// Design note (Task 4.4):
//   FilterDisplayItem::setHighResolution(bool) / highResolution() control whether
//   paintFilterEdges() renders the actual FIR magnitude curve via
//   RxChannel::filterResponseMagnitudes() (high-res ON) or the simplified
//   box-edge passband markers only (high-res OFF).
//
//   bindRxChannel(RxChannel*) sets the non-owning channel pointer; the item
//   is safe to call even with a nullptr channel (the high-res path skips
//   paintHighResolutionFilterCurve() gracefully).

#include <QtTest/QtTest>
#include <QPainter>
#include <cmath>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include "core/RxChannel.h"
#include "gui/containers/meter_property_editors/FilterDisplayItemEditor.h"
#include "gui/meters/FilterDisplayItem.h"

using namespace NereusSDR;

class TestFilterDisplayHighResolution : public QObject {
    Q_OBJECT

private slots:
    // setHighResolution / highResolution round-trip
    void high_resolution_round_trips()
    {
        FilterDisplayItem item;
        QCOMPARE(item.highResolution(), false);  // default off

        item.setHighResolution(true);
        QCOMPARE(item.highResolution(), true);

        item.setHighResolution(false);
        QCOMPARE(item.highResolution(), false);
    }

    // setHighResolution is idempotent (no double-update on same value)
    void high_resolution_idempotent()
    {
        FilterDisplayItem item;
        item.setHighResolution(false);
        QCOMPARE(item.highResolution(), false);  // still off, no change

        item.setHighResolution(true);
        QCOMPARE(item.highResolution(), true);

        item.setHighResolution(true);   // same value — should not crash
        QCOMPARE(item.highResolution(), true);
    }

    // bindRxChannel with nullptr must not crash even when high-res is on
    void bind_nullptr_channel_does_not_crash()
    {
        FilterDisplayItem item;
        item.bindRxChannel(nullptr);
        item.setHighResolution(true);
        QCOMPARE(item.highResolution(), true);  // mode unaffected by bind
    }

    // bind-and-clear lifecycle: bindRxChannel(nullptr) clears cleanly
    void bind_and_clear_channel()
    {
        FilterDisplayItem item;
        item.setHighResolution(true);

        // Bind a temporary channel, then clear via nullptr
        {
            RxChannel ch(98, 64, 48000);
            item.bindRxChannel(&ch);
        }
        // Channel has been destroyed — clear the pointer
        item.bindRxChannel(nullptr);
        QCOMPARE(item.highResolution(), true);  // mode unaffected by bind/clear
    }

    void accepted_frame_geometry_and_paint_do_not_advance_waterfall()
    {
        FilterDisplayItem item;
        QVector<float> trace(384, -91.0f);
        QVector<float> waterfall(384, -83.0f);
        item.presentFrame(trace, waterfall, 14'100'000.0, 20'000.0, false, true);
        QVERIFY(item.frameAvailable());
        QCOMPARE(item.framePixels(), 384);
        QCOMPARE(item.frameCentreHz(), 14'100'000.0);
        QCOMPARE(item.frameSpanHz(), 20'000.0);
        for (int i = 0; i < 3; ++i) {
            item.presentFrame(trace, waterfall, 14'100'000.0, 20'000.0, false, true);
        }
        const QImage initial = item.waterfallImageForTest().copy();
        QImage canvas(400, 220, QImage::Format_RGB32);
        canvas.fill(Qt::black);
        {
            QPainter painter(&canvas);
            item.paint(painter, 400, 220);
            item.paint(painter, 400, 220);
        }
        QCOMPARE(item.waterfallImageForTest(), initial);
        for (int i = 0; i < 4; ++i) {
            item.presentFrame(trace, waterfall, 14'100'000.0, 20'000.0, false, true);
        }
        QVERIFY(item.waterfallImageForTest() != initial);
        item.clearFrame();
        QVERIFY(!item.frameAvailable());
    }

    void rf_markers_follow_accepted_crop_in_each_container()
    {
        QVector<float> bins(1024, -90.0f);
        FilterDisplayItem a;
        FilterDisplayItem b;
        a.presentFrame(bins, bins, 14'100'000.0, 20'000.0, false, false);
        b.presentFrame(bins, bins, 14'110'000.0, 20'000.0, false, false);
        a.setRfMarkers(14'098'000.0, 14'102'000.0, 0, 0, {});
        b.setRfMarkers(14'108'000.0, 14'112'000.0, 0, 0, {});
        QCOMPARE(a.rxLowPixelForTest(), b.rxLowPixelForTest());
        QCOMPARE(a.rxHighPixelForTest(), b.rxHighPixelForTest());
        QCOMPARE(a.rxLowPixelForTest(), 410);
        QCOMPARE(a.rxHighPixelForTest(), 614);
        a.setRfMarkers(14'087'000.0, 14'089'000.0, 0, 0, {});
        QCOMPARE(a.rxLowPixelForTest(), -1);
        QCOMPARE(a.rxHighPixelForTest(), -1);
    }

    void fir_curve_uses_accepted_rf_window()
    {
        const auto rendered = [](double centreHz) {
            FilterDisplayItem item;
            item.setDisplayMode(FilterDisplayItem::DisplayMode::Panadapter);
            item.setHighResolution(true);
            const QVector<float> noise(1024, -90.0f);
            item.presentFrame(noise, noise, centreHz, 20'000.0, false, false);
            QVector<double> response(25, -70.0);
            response[0] = -10.0;
            response[1] = -10.0;
            item.setRfFilterResponse(response, 0.0, 1000.0, 100'000.0);
            QImage image(400, 200, QImage::Format_RGB32);
            image.fill(Qt::black);
            QPainter painter(&image);
            item.paint(painter, 400, 200);
            painter.end();
            return image;
        };
        const auto curveY = [](const QImage& image, int x) {
            for (int y = 0; y < image.height(); ++y) {
                const QColor colour(image.pixel(x, y));
                if (colour.blue() > colour.green() + 5
                    && colour.green() > colour.red() + 20
                    && colour.blue() > colour.red() + 50) { return y; }
            }
            return -1;
        };
        const QImage centred = rendered(100'000.0);
        const QImage shifted = rendered(110'000.0);
        QVERIFY(std::abs(curveY(centred, 200) - 24) <= 4);
        QVERIFY(std::abs(curveY(centred, 40) - 174) <= 4);
        QVERIFY(std::abs(curveY(shifted, 5) - 24) <= 4);
        QVERIFY(std::abs(curveY(shifted, 200) - 174) <= 4);
    }

    void mini_waterfall_palette_uses_thetis_color_rules()
    {
        const auto colour = [](FilterDisplayItem::WaterfallPalette palette, float db,
                               const QVector<QColor>& custom = {}) {
            FilterDisplayItem item;
            item.setWaterfallPalette(palette);
            item.setCustomWaterfallGradient(custom, custom);
            const QVector<float> row(1024, db);
            for (int i = 0; i < 4; ++i) {
                item.presentFrame(row, row, 14'100'000.0, 20'000.0, false, true);
            }
            return QColor(item.waterfallImageForTest().pixel(0, 0));
        };
        using Palette = FilterDisplayItem::WaterfallPalette;
        QCOMPARE(colour(Palette::Enhanced, -40.0f), QColor(192, 124, 255));
        QCOMPARE(colour(Palette::Spectran, -40.0f), QColor(240, 240, 240));
        QCOMPARE(colour(Palette::BlackWhite, -90.0f), QColor(127, 127, 127));
        QCOMPARE(colour(Palette::LinRad, -90.0f), QColor(0, 160, 120));
        QVERIFY(colour(Palette::LinLog, -90.0f) != colour(Palette::LinRad, -90.0f));
        QCOMPARE(colour(Palette::LinAuto, -90.0f), QColor(252, 252, 252));
        QVector<QColor> lut(101, QColor(12, 34, 56));
        QCOMPARE(colour(Palette::Custom, -90.0f, lut), QColor(12, 34, 56));
    }

    // The saved palette loads as itself whatever the frame interval, and a
    // palette outside WaterfallPalette loads as Enhanced. A bad frame
    // interval still loads as 4.
    void saved_palette_loads_independent_of_frame_interval()
    {
        using Palette = FilterDisplayItem::WaterfallPalette;
        const QStringList base = FilterDisplayItem().serialize().split(QLatin1Char('|'));
        QVERIFY(base.size() > 19);
        const auto load = [&base](const QString& palette, const QString& interval,
                                  FilterDisplayItem& item) {
            QStringList parts = base;
            parts[18] = palette;
            parts[19] = interval;
            return item.deserialize(parts.join(QLatin1Char('|')));
        };

        FilterDisplayItem blackWhite;
        QVERIFY(load(QStringLiteral("2"), QStringLiteral("-1"), blackWhite));
        QCOMPARE(blackWhite.waterfallPalette(), Palette::BlackWhite);
        QCOMPARE(blackWhite.serialize().split(QLatin1Char('|')).at(19), QStringLiteral("4"));

        for (const QString& bad : {QStringLiteral("-1"), QStringLiteral("7")}) {
            FilterDisplayItem item;
            item.setWaterfallPalette(Palette::LinRad);
            QVERIFY(load(bad, QStringLiteral("6"), item));
            QCOMPARE(item.waterfallPalette(), Palette::Enhanced);
            QCOMPARE(item.serialize().split(QLatin1Char('|')).at(19), QStringLiteral("6"));
        }
    }

    // A new item takes the Thetis values (MeterManager.cs:27651-27705
    // [v2.10.3.15]); a constructed item keeps the older look.
    void new_item_takes_thetis_defaults()
    {
        FilterDisplayItem older;
        QCOMPARE(older.verticalRatio(), 0.02f);
        QCOMPARE(older.notchColour(), QColor(0xff, 0xa5, 0x00));

        FilterDisplayItem item;
        item.applyNewItemDefaults();
        QCOMPARE(item.verticalRatio(), 0.2f);
        QVERIFY(item.showFilterLimits());
        QVERIFY(!item.fixedRxZoom());
        QVERIFY(!item.fixedTxZoom());
        QCOMPARE(item.rxZoom(), 1.0f);
        QCOMPARE(item.txZoom(), 1.0f);
        QCOMPARE(item.sidebandsScale(), 0.0f);
        QCOMPARE(item.cwScale(), 0.0f);
        QCOMPARE(item.othersScale(), 0.0f);
        QCOMPARE(item.displayMode(), FilterDisplayItem::DisplayMode::Panafall);
        QCOMPARE(item.fontScale(), 1.0f);
        QCOMPARE(item.waterfallFrameInterval(), 4);
        QVERIFY(item.greyWhenNotActive());
        QVERIFY(!item.sidebandMode());
        QVERIFY(!item.snapLines());
        QVERIFY(!item.autoZoom());
        QVERIFY(!item.showCharacteristic());
        QCOMPARE(item.characteristicLow(), -250.0f);
        QCOMPARE(item.dataLineColour(), QColor(0x32, 0xcd, 0x32));    // LimeGreen
        QCOMPARE(item.waterfallLowColour(), QColor(Qt::black));
        QCOMPARE(item.textColour(), QColor(Qt::white));
        QCOMPARE(item.numberHighlightColour(), QColor(0x8b, 0x00, 0x00)); // DarkRed
        QCOMPARE(item.edgesColourRX(), QColor(0xff, 0xff, 0x00));     // Yellow
        QCOMPARE(item.edgesColourTX(), QColor(0xff, 0x00, 0x00));     // Red
        QCOMPARE(item.edgeHighlightColour(), QColor(Qt::white));
        QCOMPARE(item.meterBackColour(), QColor(Qt::black));
        QCOMPARE(item.notchColour(), QColor(0xff, 0x45, 0x00));       // OrangeRed
        QCOMPARE(item.notchHighlightColour(), QColor(0x32, 0xcd, 0x32));
        QCOMPARE(item.extentsColour(), QColor(0x80, 0x80, 0x80));     // Gray
        QCOMPARE(item.snapLineColour(), QColor(0x80, 0x80, 0x80));
        QCOMPARE(item.settingOnColour(), QColor(0x64, 0x95, 0xed));   // CornflowerBlue
        QCOMPARE(item.buttonHighlightColour(), QColor(0x80, 0x80, 0x80));
        QCOMPARE(QString::fromLatin1(FilterDisplayItem::kMnfLabel), QStringLiteral(" MNF "));
        QCOMPARE(QString::fromLatin1(FilterDisplayItem::kMnfPlusLabel), QStringLiteral(" +MNF "));
        QCOMPARE(QString::fromLatin1(FilterDisplayItem::kSnapLabel), QStringLiteral(" SNAP "));
        QCOMPARE(QString::fromLatin1(FilterDisplayItem::kFitLabel), QStringLiteral(" FIT "));
    }

    // The editor offers the Thetis ranges and writes through to the item.
    void editor_enforces_thetis_ranges()
    {
        FilterDisplayItem item;
        item.applyNewItemDefaults();
        FilterDisplayItemEditor editor;
        editor.setItem(&item);

        const auto doubleRange = [&editor](const char* name, double lo, double hi) {
            auto* spin = editor.findChild<QDoubleSpinBox*>(QLatin1String(name));
            QVERIFY2(spin, name);
            QVERIFY2(qAbs(spin->minimum() - lo) < 1e-6, name);
            QVERIFY2(qAbs(spin->maximum() - hi) < 1e-6, name);
        };
        doubleRange("filterVerticalRatio", 0.15, 1.0);
        doubleRange("filterRxZoom", 1.0, 10.0);
        doubleRange("filterTxZoom", 1.0, 20.0);
        doubleRange("filterSidebandsScale", 0.0, 10.0);
        doubleRange("filterCwScale", 0.0, 10.0);
        doubleRange("filterOthersScale", 0.0, 10.0);
        doubleRange("filterFontScale", 0.01, 4.0);
        doubleRange("filterCharacteristicLow", -300.0, -50.0);
        auto* frames = editor.findChild<QSpinBox*>(QStringLiteral("filterFrameInterval"));
        QVERIFY(frames);
        QCOMPARE(frames->minimum(), 1);
        QCOMPARE(frames->maximum(), 1000);
        QCOMPARE(frames->value(), 4);

        // Values past the range land at its edge on the item.
        auto* txZoom = editor.findChild<QDoubleSpinBox*>(QStringLiteral("filterTxZoom"));
        txZoom->setValue(25.0);
        QCOMPARE(item.txZoom(), 20.0f);
        auto* low = editor.findChild<QDoubleSpinBox*>(QStringLiteral("filterCharacteristicLow"));
        low->setValue(-400.0);
        QCOMPARE(item.characteristicLow(), -300.0f);
        frames->setValue(0);
        QCOMPARE(item.waterfallFrameInterval(), 1);
        auto* snap = editor.findChild<QCheckBox*>(QStringLiteral("filterSnapLines"));
        QVERIFY(snap);
        QVERIFY(!snap->isChecked());
        snap->setChecked(true);
        QVERIFY(item.snapLines());
    }
};

QTEST_MAIN(TestFilterDisplayHighResolution)
#include "tst_filter_display_high_resolution.moc"
