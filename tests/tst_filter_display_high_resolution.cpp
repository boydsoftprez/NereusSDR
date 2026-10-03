// NereusSDR-original infrastructure — no Thetis source ported here.
// No upstream attribution required (NereusSDR FilterDisplayItem high-res test).
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
#include "core/RxChannel.h"
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
};

QTEST_MAIN(TestFilterDisplayHighResolution)
#include "tst_filter_display_high_resolution.moc"
