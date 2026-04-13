// tst_meter_presets.cpp — ItemGroup preset composition parity tests.
//
// Phase D / E of the meter-parity port plan. Each factory test builds the
// preset, asserts it has the right number of items in the right z-order
// with bindings + ranges matching the Thetis source referenced in the
// factory's source comment.

#include <QtTest/QtTest>
#include <QDir>
#include <QImage>
#include <QPainter>

#include "gui/meters/ItemGroup.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding

using namespace NereusSDR;

class TstMeterPresets : public QObject
{
    Q_OBJECT

private:
    template <typename T>
    static T* findOfType(ItemGroup* group, int nth = 0)
    {
        int seen = 0;
        for (MeterItem* mi : group->items()) {
            if (T* cast = qobject_cast<T*>(mi)) {
                if (seen == nth) { return cast; }
                ++seen;
            }
        }
        return nullptr;
    }

    template <typename T>
    static int countOfType(ItemGroup* group)
    {
        int n = 0;
        for (MeterItem* mi : group->items()) {
            if (qobject_cast<T*>(mi)) { ++n; }
        }
        return n;
    }

private slots:
    // ---- Phase D1: createSMeterPreset rebuilt from addSMeterBar ----

    void SMeter_preset_is_now_a_bar_not_a_needle()
    {
        // Pre-D1 createSMeterPreset built a single bare NeedleItem.
        // After D1 it builds the Thetis addSMeterBar composition:
        // SolidColour bg (z=1), BarItem Line-style with calibration
        // (z=2), ScaleItem ShowType + GeneralScale (z=3), ClickBoxItem
        // (z=4). Needle count should be zero.
        ItemGroup* g = ItemGroup::createSMeterPreset(
            MeterBinding::SignalPeak, QStringLiteral("S-Meter"), nullptr);
        QVERIFY(g != nullptr);

        QCOMPARE(countOfType<NeedleItem>(g), 0);
        QCOMPARE(countOfType<BarItem>(g),    1);
        QCOMPARE(countOfType<ScaleItem>(g),  1);
        QCOMPARE(countOfType<SolidColourItem>(g), 1);

        delete g;
    }

    void SMeter_bar_matches_Thetis_addSMeterBar_calibration()
    {
        // Thetis addSMeterBar (MeterManager.cs:21529-21555):
        //   Style               = BarStyle.Line
        //   AttackRatio         = 0.8f
        //   DecayRatio          = 0.2f
        //   HistoryDuration     = 4000
        //   ShowHistory         = true
        //   ShowValue           = true
        //   Colour              = CadetBlue
        //   ShowMarker          = true
        //   PeakHoldMarkerColour = Red
        //   HistoryColour       = Color.FromArgb(128, Red)
        //   FontColour          = Yellow
        //   ScaleCalibration    = { -133->0.00, -73->0.50, -13->0.99 }
        ItemGroup* g = ItemGroup::createSMeterPreset(
            MeterBinding::SignalPeak, QStringLiteral("S-Meter"), nullptr);
        BarItem* bar = findOfType<BarItem>(g);
        QVERIFY(bar != nullptr);

        QCOMPARE(bar->barStyle(),            BarItem::BarStyle::Line);
        QCOMPARE(bar->attackRatio(),         0.8f);
        QCOMPARE(bar->decayRatio(),          0.2f);
        QCOMPARE(bar->historyDurationMs(),   4000);
        QCOMPARE(bar->showHistory(),         true);
        QCOMPARE(bar->showValue(),           true);
        QCOMPARE(bar->showMarker(),          true);
        QCOMPARE(bar->peakHoldMarkerColour(), QColor(Qt::red));
        QCOMPARE(bar->historyColour(),       QColor(255, 0, 0, 128));
        QCOMPARE(bar->fontColour(),          QColor(Qt::yellow));

        // Non-linear calibration — the core of the S-meter port.
        QCOMPARE(bar->scaleCalibrationSize(), 3);
        QCOMPARE(bar->valueToNormalizedX(-133.0), 0.0f);
        QCOMPARE(bar->valueToNormalizedX(-73.0),  0.5f);
        QCOMPARE(bar->valueToNormalizedX(-13.0),  0.99f);

        QCOMPARE(bar->bindingId(), MeterBinding::SignalPeak);

        delete g;
    }

    void SMeter_scale_has_showType_and_GeneralScale()
    {
        // Thetis addSMeterBar (MeterManager.cs:21557-21564):
        //   cs.ShowType = true
        //   cs.ReadingSource matches the bar's binding
        //   cs.ZOrder = 3
        // Plus the renderScale dispatch at MeterManager.cs:31911-31916
        // uses generalScale(6, 3, -1, 60, 2, 20, ..., 0.5f, true, true)
        // for SIGNAL_STRENGTH — we port that to the NereusSDR
        // GeneralScale params exactly.
        ItemGroup* g = ItemGroup::createSMeterPreset(
            MeterBinding::SignalPeak, QStringLiteral("S-Meter"), nullptr);
        ScaleItem* scale = findOfType<ScaleItem>(g);
        QVERIFY(scale != nullptr);

        QCOMPARE(scale->showType(),   true);
        QCOMPARE(scale->bindingId(),  MeterBinding::SignalPeak);
        QCOMPARE(scale->scaleStyle(), ScaleItem::ScaleStyle::GeneralScale);
        QCOMPARE(scale->lowLongTicks(),  6);
        QCOMPARE(scale->highLongTicks(), 3);

        delete g;
    }

    void SMeter_accepts_alternate_bindings()
    {
        // addSMeterBar is parameterised on the reading so the same
        // composition works for SIGNAL_STRENGTH, AVG_SIGNAL_STRENGTH,
        // and SIGNAL_MAX_BIN (MeterManager.cs:21499-21510).
        for (int b : {MeterBinding::SignalPeak,
                      MeterBinding::SignalAvg,
                      MeterBinding::SignalMaxBin}) {
            ItemGroup* g = ItemGroup::createSMeterPreset(
                b, QStringLiteral("SMeter"), nullptr);
            QVERIFY(g);
            BarItem* bar = findOfType<BarItem>(g);
            QVERIFY(bar);
            QCOMPARE(bar->bindingId(), b);
            QCOMPARE(bar->scaleCalibrationSize(), 3);
            delete g;
        }
    }

    // --- Phase E: 3-row stacked dump (ALC + EQ + Mic) ---
    //
    // Simulates what appendPresetRow() does in the dialog:
    //   for each preset,
    //     compute yPos = max bottom of existing items under the 0.7 threshold,
    //     slotH = min(0.10, 0.95 - yPos),
    //     clone each preset item with rect rescaled into [yPos, yPos+slotH].
    // Renders the resulting 9-item list into /tmp/threeRowStack.png.
    void three_rows_stack_into_a_single_container()
    {
        QList<MeterItem*> working;

        auto appendPreset = [&working](ItemGroup* src) {
            // max bottom of items with h <= 0.7 (skip backgrounds)
            float yPos = 0.0f;
            for (MeterItem* mi : working) {
                if (mi->itemHeight() > 0.7f) { continue; }
                const float b = mi->y() + mi->itemHeight();
                if (b > yPos) { yPos = b; }
            }
            const float slotH = qMin(0.10f, 0.95f - yPos);
            for (MeterItem* srcItem : src->items()) {
                // Deep-clone via serialize round trip. The specific
                // subclass we produce depends on the tag.
                const QString blob = srcItem->serialize();
                MeterItem* clone = nullptr;
                if      (blob.startsWith(QLatin1String("BAR|")))    clone = new BarItem();
                else if (blob.startsWith(QLatin1String("SCALE|")))  clone = new ScaleItem();
                else if (blob.startsWith(QLatin1String("SOLID|")))  clone = new SolidColourItem();
                else if (blob.startsWith(QLatin1String("TEXT|")))   clone = new TextItem();
                if (!clone) { continue; }
                clone->deserialize(blob);
                clone->setRect(clone->x(),
                               yPos + clone->y() * slotH,
                               clone->itemWidth(),
                               clone->itemHeight() * slotH);
                working.append(clone);
            }
            delete src;
        };

        appendPreset(ItemGroup::createAlcPreset(nullptr));
        appendPreset(ItemGroup::createEqBarPreset(nullptr));
        appendPreset(ItemGroup::createMicPreset(nullptr));

        QVERIFY(working.size() >= 9);  // 3 items per preset × 3

        // Feed a realistic value to every bar so paint() has
        // something to render.
        for (MeterItem* mi : working) {
            if (auto* bar = qobject_cast<BarItem*>(mi)) {
                for (int i = 0; i < 10; ++i) { bar->setValue(-6.0); }
            }
        }

        const int W = 480;
        const int H = 320;  // tall enough to show 3 stacked rows
        QImage img(W, H, QImage::Format_ARGB32);
        img.fill(QColor(15, 15, 26));
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            QList<MeterItem*> ordered = working;
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](MeterItem* a, MeterItem* b) {
                return a->zOrder() < b->zOrder();
            });
            for (MeterItem* mi : ordered) {
                mi->paint(p, W, H);
            }
        }
        QVERIFY(img.save(QStringLiteral("/tmp/threeRowStack.png"), "PNG"));
        qDeleteAll(working);
    }

    // --- Phase E: ALC bar row PNG dump ---
    void ALC_bar_row_dump_to_png()
    {
        ItemGroup* g = ItemGroup::createAlcPreset(nullptr);
        QVERIFY(g);
        QVERIFY(g->items().size() >= 3);

        // Feed a realistic TX ALC reading (-6 dBm of compression).
        BarItem* bar = nullptr;
        for (MeterItem* mi : g->items()) {
            if (auto* b = qobject_cast<BarItem*>(mi)) { bar = b; break; }
        }
        QVERIFY(bar);
        for (int i = 0; i < 20; ++i) { bar->setValue(-6.0); }
        bar->setValue(2.0);   // push a transient peak above 0 dB
        for (int i = 0; i < 10; ++i) { bar->setValue(-6.0); }

        const int W = 480;
        const int H = 80;   // single row — shorter than the S-meter
        QImage img(W, H, QImage::Format_ARGB32);
        img.fill(QColor(15, 15, 26));

        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            QVector<MeterItem*> ordered = g->items();
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](MeterItem* a, MeterItem* b) {
                return a->zOrder() < b->zOrder();
            });
            for (MeterItem* mi : ordered) {
                mi->paint(p, W, H);
            }
        }
        QVERIFY(img.save(QStringLiteral("/tmp/alcBarRow.png"), "PNG"));
        delete g;
    }

    // --- Headless visual dump of the full createSMeterPreset composition ---
    // Renders every item in the preset through its real paint() function
    // into a shared QImage and writes the result to /tmp/sMeterPreset.png
    // so the image can be read back out-of-process for debugging. This is
    // what the container would actually show if a MeterWidget routed its
    // paint through QPainter instead of QRhi.
    void SMeter_dump_full_composition_to_png()
    {
        ItemGroup* g = ItemGroup::createSMeterPreset(
            MeterBinding::SignalPeak, QStringLiteral("S-Meter"), nullptr);
        QVERIFY(g);

        // Feed a realistic live signal so the bar + markers + text are
        // visibly populated (-60 dBm sits comfortably between S0 and S9).
        BarItem* bar = findOfType<BarItem>(g);
        QVERIFY(bar);
        for (int i = 0; i < 20; ++i) { bar->setValue(-60.0); }
        // Push a higher peak to exercise the peak-hold marker.
        bar->setValue(-30.0);
        for (int i = 0; i < 10; ++i) { bar->setValue(-60.0); }

        const int W = 480;
        const int H = 120;
        QImage img(W, H, QImage::Format_ARGB32);
        img.fill(QColor(15, 15, 26));  // NereusSDR dark navy

        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            // Sort items by zOrder so background paints before bar
            // paints before scale — matching what MeterWidget does in
            // its layered render loop.
            QVector<MeterItem*> ordered = g->items();
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](MeterItem* a, MeterItem* b) {
                return a->zOrder() < b->zOrder();
            });
            for (MeterItem* mi : ordered) {
                mi->paint(p, W, H);
            }
        }

        const QString outPath = QStringLiteral("/tmp/sMeterPreset.png");
        QVERIFY2(img.save(outPath, "PNG"),
                 "failed to save /tmp/sMeterPreset.png");

        delete g;
    }
};

QTEST_MAIN(TstMeterPresets)
#include "tst_meter_presets.moc"
