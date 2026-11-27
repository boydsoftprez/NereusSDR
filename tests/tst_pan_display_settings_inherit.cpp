// =================================================================
// tests/tst_pan_display_settings_inherit.cpp  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
// 2026-10-12 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//   Requested Clarity persistence and pure/page-scoped display helpers.
//
// no-port-check: NereusSDR-original. Per-pan display settings keys are a
// NereusSDR construct. Thetis has a fixed RX1/RX2 pair with hand-written
// RX2 duplicates of some settings and no per-pan scheme to port.
//
// Bench report 2026-07-30 (JJ, KG4VCF): "the second-N panadapters seem to
// not honour the settings for display, pan 1 does, seems no way to adjust
// the others."
//
// Two causes. Setup pushed every display control through a single
// SpectrumWidget captured once at startup (fixed in MainWindow, covered by
// bench row 27, not reachable from a unit test without a MainWindow). The
// other is here: a pan opening for the first time has no keys of its own,
// so every read fell through to the hardcoded ship defaults and the new
// pan came up looking nothing like the one beside it.
//
// The model chosen then (2026-07-30): a pan follows pan 0 until it is
// given a setting of its own, then it is independent. Implemented as a
// fallback in the read path rather than by copying keys, so "untouched"
// keeps tracking instead of freezing whatever was there at creation.
//
// Display pill plan Task 1 (2026-10-10) keeps that model and names what
// is followed: the Setup default, stored in the bare key
// ("DisplayLineWidth"). Every pan, pan 0 included, keeps a value of its own
// in "<base>_<N>" and writes it only when it differs from what it would
// inherit (core/settings/PanDisplayKeys.h). Before, pan 0 wrote the bare
// key itself, so "follows pan 0" and "follows the default" were one thing;
// an install from then still loads the same values, which
// an_old_install_loads_what_it_loaded_before pins. That case was written
// first and seen passing on the code before the split.
//
// Cases rewritten for the new placement (each asserted that pan 0's save
// lands in the bare key, or that another pan inherits what pan 0 saved):
// pan_zero_reads_its_own_key_then_the_setup_default (was
// pan_zero_reads_its_own_key_only),
// trace_fill_color_alpha_survives_a_fresh_widget,
// trace_fill_color_follows_the_setup_default_until_overridden (was
// ..._follows_pan_zero_until_overridden) and
// existing_grid_color_is_shared_and_retires_old_pan_values
// (was ..._inherits_pan_zero_...). The cases that spelled "this pan has no
// key" as an empty string now remove the key: an empty value in a pan's
// own key is a value (see PanDisplayKeys.h, "Presence").
//
// Isolation: TestSandboxInit redirects the settings path away from the
// operator's profile, and init()/cleanup() clear the in-process store
// around every slot (as tst_display_settings_golden does).
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/settings/PanDisplayKeys.h"
#include "gui/SpectrumWidget.h"
#include "gui/ColorSwatchButton.h"
#include "gui/widgets/GradientPickerWidget.h"
#include "models/DisplaySettingsModel.h"

using namespace NereusSDR;
using namespace Qt::StringLiterals;

namespace {
// Two keys with different read paths: a float through readFloat and a
// colour through readColor. Both must inherit, and they did not share an
// implementation before the 2026-07-30 change.
//
// Display pill plan Task 9: the float was DisplayGridMax until the receive
// range moved to the per-band store (PanadapterModel through PanModels),
// which no pan key reaches any more; tst_pan_models covers that. The line
// width goes through the same readFloat path.
const QString kFloatKey  = u"DisplayLineWidth"_s;
const QString kFloatKey0 = u"DisplayLineWidth_0"_s;
const QString kFloatKey1 = u"DisplayLineWidth_1"_s;
const QString kFillColorKey = u"DisplayFillColor"_s;
const QString kFillColorKey0 = u"DisplayFillColor_0"_s;
const QString kFillColorKey1 = u"DisplayFillColor_1"_s;
const QString kFillColorKey2 = u"DisplayFillColor_2"_s;
const QString kGridColorKey = u"DisplayGridColor"_s;
const QString kGridColorKey1 = u"DisplayGridColor_1"_s;
const QString kOpacityKey = u"DisplayWfOpacity"_s;

// Every key whose name ends in "_<pan>".
QStringList keysOfPan(int pan)
{
    const QString suffix = u"_%1"_s.arg(pan);
    QStringList out;
    const QStringList all = AppSettings::instance().allKeys();
    for (const QString& key : all) {
        if (key.endsWith(suffix)) { out.append(key); }
    }
    out.sort();
    return out;
}

// Every key and value except pan `pan`'s overrides of `bases`.
QMap<QString, QString> everythingExceptPanOverrides(int pan, const QStringList& bases)
{
    QSet<QString> own;
    for (const QString& base : bases) {
        own.insert(PanDisplayKeys::panKey(base, pan));
    }
    QMap<QString, QString> out;
    auto& s = AppSettings::instance();
    const QStringList all = s.allKeys();
    for (const QString& key : all) {
        if (!own.contains(key)) { out.insert(key, s.value(key).toString()); }
    }
    return out;
}

QMap<QString, QString> markerAndTrackingValues(const SpectrumWidget& pan)
{
    const auto boolean = [](bool value) { return value ? u"True"_s : u"False"_s; };
    return {
        {u"DisplayShowBinWidth"_s, boolean(pan.showBinWidth())},
        {u"DisplayShowNoiseFloor"_s, boolean(pan.showNoiseFloor())},
        {u"DisplayShowNoiseFloorPosition"_s, QString::number(static_cast<int>(pan.showNoiseFloorPosition()))},
        {u"DisplayNoiseFloorColor"_s, pan.noiseFloorColor().name(QColor::HexArgb)},
        {u"DisplayNoiseFloorTextColor"_s, pan.noiseFloorTextColor().name(QColor::HexArgb)},
        {u"DisplayNoiseFloorFastColor"_s, pan.noiseFloorFastColor().name(QColor::HexArgb)},
        {u"DisplayNoiseFloorLineWidth"_s, QString::number(pan.noiseFloorLineWidth())},
        {u"DisplayNoiseFloorShiftDb"_s, QString::number(pan.nfShiftDbm())},
        {u"DisplayShowPeakValueOverlay"_s, boolean(pan.showPeakValueOverlay())},
        {u"DisplayPeakValuePosition"_s, QString::number(static_cast<int>(pan.peakValuePosition()))},
        {u"DisplayPeakTextDelayMs"_s, QString::number(pan.peakTextDelayMs())},
        {u"DisplayPeakValueColor"_s, pan.peakValueColor().name(QColor::HexArgb)},
        {u"DisplayAdjustGridMinToNoiseFloor"_s, boolean(pan.adjustGridMinToNoiseFloor())},
        {u"DisplayNFOffsetGridFollow"_s, QString::number(pan.nfOffsetGridFollow())},
        {u"DisplayMaintainNFAdjustDelta"_s, boolean(pan.maintainNFAdjustDelta())}
    };
}

QMap<QString, QString> peakDetailValues(const SpectrumWidget& pan)
{
    const auto boolean = [](bool value) { return value ? u"True"_s : u"False"_s; };
    return {
        {u"DisplayActivePeakHoldEnabled"_s, boolean(pan.activePeakHoldEnabled())},
        {u"DisplayActivePeakHoldDurationMs"_s, QString::number(pan.activePeakHoldDurationMs())},
        {u"DisplayActivePeakHoldDropDbPerSec"_s, QString::number(pan.activePeakHoldDropDbPerSec())},
        {u"DisplayActivePeakHoldFill"_s, boolean(pan.activePeakHoldFill())},
        {u"DisplayActivePeakHoldOnTx"_s, boolean(pan.activePeakHoldOnTx())},
        {u"DisplayActivePeakHoldColor"_s, ColorSwatchButton::colorToHex(pan.activePeakHoldColor())},
        {u"DisplayPeakBlobsEnabled"_s, boolean(pan.peakBlobsEnabled())},
        {u"DisplayPeakBlobsCount"_s, QString::number(pan.peakBlobsCount())},
        {u"DisplayPeakBlobsInsideFilterOnly"_s, boolean(pan.peakBlobsInsideFilterOnly())},
        {u"DisplayPeakBlobsHoldEnabled"_s, boolean(pan.peakBlobsHoldEnabled())},
        {u"DisplayPeakBlobsHoldMs"_s, QString::number(pan.peakBlobsHoldMs())},
        {u"DisplayPeakBlobsHoldDrop"_s, boolean(pan.peakBlobsHoldDrop())},
        {u"DisplayPeakBlobsFallDbPerSec"_s, QString::number(pan.peakBlobsFallDbPerSec())},
        {u"DisplayPeakBlobColor"_s, ColorSwatchButton::colorToHex(pan.peakBlobColor())},
        {u"DisplayPeakBlobTextColor"_s, ColorSwatchButton::colorToHex(pan.peakBlobTextColor())},
        {u"DisplayGridStep"_s, QString::number(pan.dbGridStep())}
    };
}

struct SharedAppearanceColor {
    QString key;
    void (SpectrumWidget::*set)(const QColor&);
    QColor (SpectrumWidget::*get)() const;
    QColor ship;
};
const QList<SharedAppearanceColor>& sharedAppearanceColors()
{
    static const QList<SharedAppearanceColor> rows = {
        {u"DisplayGridColor"_s, &SpectrumWidget::setGridColor, &SpectrumWidget::gridColor, QColor(255, 255, 255, 40)},
        {u"DisplayGridFineColor"_s, &SpectrumWidget::setGridFineColor, &SpectrumWidget::gridFineColor, QColor(255, 255, 255, 20)},
        {u"DisplayHGridColor"_s, &SpectrumWidget::setHGridColor, &SpectrumWidget::hGridColor, QColor(255, 255, 255, 40)},
        {u"DisplayGridTextColor"_s, &SpectrumWidget::setGridTextColor, &SpectrumWidget::gridTextColor, QColor(0xdc, 0xdc, 0xe1)},
        {u"DisplayBandEdgeColor"_s, &SpectrumWidget::setBandEdgeColor, &SpectrumWidget::bandEdgeColor, QColor(255, 0, 0)},
        {u"DisplayRxZeroLineColor"_s, &SpectrumWidget::setRxZeroLineColor, &SpectrumWidget::rxZeroLineColor, QColor(255, 0, 0)},
        {u"DisplayTxZeroLineColor"_s, &SpectrumWidget::setTxZeroLineColor, &SpectrumWidget::txZeroLineColor, QColor(255, 184, 0)},
        {u"DisplayRxFilterColor"_s, &SpectrumWidget::setRxFilterColor, &SpectrumWidget::rxFilterColor, QColor(0, 180, 216, 80)},
        {u"DisplayTxFilterColor"_s, &SpectrumWidget::setTxFilterColor, &SpectrumWidget::txFilterColor, QColor(255, 120, 60, 46)},
    };
    return rows;
}
struct SharedTransmitWaterfallSetting {
    QString key;
    QString historic;
    QString pillOwn;
    QString edited;
    std::function<QString(const SpectrumWidget&)> read;
    std::function<void(SpectrumWidget&, const QString&)> apply;
};
const QList<SharedTransmitWaterfallSetting>& sharedTransmitWaterfallSettings()
{
    static const QList<SharedTransmitWaterfallSetting> rows = {
        {u"DisplayTxWfPalette"_s,
         QString::number(static_cast<int>(WfColorScheme::Spectran)),
         QString::number(static_cast<int>(WfColorScheme::BlackWhite)),
         QString::number(static_cast<int>(WfColorScheme::Custom)),
         [](const SpectrumWidget& pan) { return QString::number(static_cast<int>(pan.txWfPalette())); },
         [](SpectrumWidget& pan, const QString& value) { pan.setTxWfPalette(static_cast<WfColorScheme>(value.toInt())); }},
        {u"DisplayTxWfLowColor"_s, u"#44112233"_s, u"#55445566"_s, u"#89234567"_s,
         [](const SpectrumWidget& pan) { return pan.txWfLowColor().name(QColor::HexArgb).toUpper(); },
         [](SpectrumWidget& pan, const QString& value) { pan.setTxWfLowColor(QColor::fromString(value)); }},
        {u"DisplayTxWfLowLevel"_s, u"-60"_s, u"-80"_s, u"-65"_s,
         [](const SpectrumWidget& pan) { return QString::number(pan.txWfLowLevel()); },
         [](SpectrumWidget& pan, const QString& value) { pan.setTxWfLowLevel(value.toInt()); }},
        {u"DisplayTxWfHighLevel"_s, u"20"_s, u"30"_s, u"25"_s,
         [](const SpectrumWidget& pan) { return QString::number(pan.txWfHighLevel()); },
         [](SpectrumWidget& pan, const QString& value) { pan.setTxWfHighLevel(value.toInt()); }},
    };
    return rows;
}
QString customGradientFixture(const QColor& low, const QColor& high)
{
    GradientPickerWidget picker;
    picker.setStopsForTest({{0.0f, low, true}, {1.0f, high, true}});
    return picker.encodedText();
}
void configureManualWaterfall(SpectrumWidget& pan, WfColorScheme scheme)
{
    pan.setClarityEnabled(false);
    pan.setWfAgcEnabled(false);
    pan.setWaterfallNFAGCEnabled(false);
    pan.setWfUseSpectrumMinMax(false);
    pan.setWfBlackLevel(57);
    pan.setWfColorGain(60);
    pan.setWfLowThreshold(-120);
    pan.setWfHighThreshold(-80);
    pan.setWfColorScheme(scheme);
}
} // namespace

class TestPanDisplaySettingsInherit : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void timeWindowUsesEachPansTimeCadenceAndResetDefaults()
    {
        SpectrumWidget pan0;
        SpectrumWidget pan1;
        pan0.setPanIndex(0);
        pan1.setPanIndex(1);
        for (SpectrumWidget* pan : {&pan0, &pan1}) {
            pan->setFrequencyRange(14'200'000.0, 192'000.0);
            pan->setDdcCenterFrequency(14'200'000.0);
            pan->setSampleRate(192'000.0);
            pan->setDisplayFps(10);
            pan->setSpectrumAveraging(SpectrumAveraging::TimeWindow);
            pan->setWaterfallAveraging(SpectrumAveraging::TimeWindow);
        }
        pan0.setSpectrumAverageTimeMs(600); // Six trace frames.
        pan1.setSpectrumAverageTimeMs(400); // Four trace frames.
        pan1.setWaterfallAverageTimeMs(600); // Six independent WF frames.
        const auto feed = [](SpectrumWidget& pan, float power) {
            pan.updateSpectrumLinear(0, QVector<float>(1024, power), 1.0, 0.0);
        };
        for (float power : {1.0f, 3.0f, 5.0f}) {
            feed(pan0, power);
            feed(pan1, power);
        }
        QVERIFY(std::abs(pan1.renderedPixels()[400] - 4.7712125f) < 0.0001f);
        QVERIFY(std::abs(pan1.wfRenderedPixels()[400] - 4.7712125f) < 0.0001f);
        pan1.setSpectrumAverageTimeMs(200); // Two frames, restarts that plane.
        feed(pan1, 7.0f);
        QVERIFY(std::abs(pan1.renderedPixels()[400] - 8.4509802f) < 0.0001f);
        // Spectrum's time edit preserves waterfall's already retained row.
        QVERIFY(std::abs(pan1.wfRenderedPixels()[400] - 6.0206000f) < 0.0001f);
        pan1.setDisplayFps(20); // 200 ms now retains four frames.
        for (float power : {1.0f, 3.0f, 5.0f}) { feed(pan1, power); }
        QVERIFY(std::abs(pan1.renderedPixels()[400] - 4.7712125f) < 0.0001f);
        pan1.saveSettings();
        SpectrumWidget restarted;
        restarted.setPanIndex(1);
        restarted.loadSettings();
        restarted.setFrequencyRange(14'200'000.0, 192'000.0);
        restarted.setDdcCenterFrequency(14'200'000.0);
        restarted.setSampleRate(192'000.0);
        restarted.setDisplayFps(20);
        QCOMPARE(restarted.spectrumAverageTimeMs(), 200);
        for (float power : {1.0f, 3.0f, 5.0f}) { feed(restarted, power); }
        QVERIFY(std::abs(restarted.renderedPixels()[400] - 4.7712125f) < 0.0001f);
        restarted.resetPanDisplayOverridesForKeys({u"DisplaySpectrumAverageTimeMs"_s});
        QCOMPARE(restarted.spectrumAverageTimeMs(), 30);
        for (float power : {1.0f, 3.0f, 5.0f}) { feed(restarted, power); }
        QVERIFY(std::abs(restarted.renderedPixels()[400] - 6.0206000f) < 0.0001f);
        QCOMPARE(pan0.spectrumAverageTimeMs(), 600);
        QVERIFY(std::abs(pan0.renderedPixels()[400] - 4.7712125f) < 0.0001f);
    }

    void cleanup() { AppSettings::instance().clear(); }

    void shared_appearance_colors_migrate_update_and_omit_pan_inventory_data()
    {
        QTest::addColumn<int>("rowIndex");
        const QList<SharedAppearanceColor>& rows = sharedAppearanceColors();
        for (int i = 0; i < rows.size(); ++i) {
            QTest::newRow(qPrintable(rows[i].key)) << i;
        }
    }

    void shared_appearance_colors_migrate_update_and_omit_pan_inventory()
    {
        QFETCH(int, rowIndex);
        const SharedAppearanceColor& row = sharedAppearanceColors()[rowIndex];
        auto& settings = AppSettings::instance();
        const QColor historic(0x12, 0x34, 0x56, 0x78);
        const QColor edited(0x23, 0x45, 0x67, 0x89);
        const QString backup = row.key + u"_backup"_s;
        const QString nonCanonical = row.key + u"_01"_s;
        settings.setValue(row.key, historic.name(QColor::HexArgb));
        settings.setValue(row.key + u"_0"_s, u"#ffabcdef"_s);
        settings.setValue(row.key + u"_7"_s, u"#ff123456"_s);
        settings.setValue(backup, u"keep backup"_s);
        settings.setValue(nonCanonical, u"keep noncanonical"_s);
        SpectrumWidget first;
        SpectrumWidget second;
        second.setPanIndex(2);
        first.loadSettings();
        second.loadSettings();
        QCOMPARE((first.*row.get)(), historic);
        QCOMPARE((second.*row.get)(), historic);
        QVERIFY(!settings.contains(row.key + u"_0"_s));
        QVERIFY(!settings.contains(row.key + u"_7"_s));
        QCOMPARE(settings.value(backup).toString(), u"keep backup"_s);
        QCOMPARE(settings.value(nonCanonical).toString(), u"keep noncanonical"_s);
        QVERIFY(!first.perPanDisplayKeyBases().contains(row.key));
        QVERIFY(!first.resettablePanDisplayKeyBases().contains(row.key));
        QSignalSpy firstChanged(&first, &SpectrumWidget::displaySettingChanged);
        QSignalSpy secondChanged(&second, &SpectrumWidget::displaySettingChanged);
        (first.*row.set)(edited);
        QCOMPARE((first.*row.get)(), edited);
        QCOMPARE((second.*row.get)(), edited);
        QVERIFY(firstChanged.count() > 0);
        QVERIFY(secondChanged.count() > 0);
        first.saveSettings();
        second.saveSettings(); // an inactive cache cannot restore historic color
        QCOMPARE(QColor::fromString(settings.value(row.key).toString()), edited);
        QVERIFY(!settings.contains(row.key + u"_0"_s));
        QVERIFY(!settings.contains(row.key + u"_2"_s));
        first.resetPanDisplayOverrides();
        QCOMPARE((first.*row.get)(), edited);
        SpectrumWidget reopened;
        reopened.setPanIndex(7);
        reopened.loadSettings();
        QCOMPARE((reopened.*row.get)(), edited);
    }

    void shared_appearance_reset_keeps_each_pans_trace_fill_and_background()
    {
        SpectrumWidget first;
        SpectrumWidget second;
        first.setPanIndex(1);
        second.setPanIndex(2);
        first.loadSettings();
        second.loadSettings();
        const QColor trace(0x11, 0x22, 0x33, 0x88);
        const QColor fill(0x22, 0x33, 0x44, 0x99);
        const QColor background(0x33, 0x44, 0x55);
        first.setTraceColor(trace);
        first.setFillColor(fill);
        first.setBackgroundFillColor(background);
        const QColor secondTrace = second.traceColor();
        const QColor secondFill = second.fillColor();
        const QColor secondBackground = second.backgroundFillColor();
        for (const SharedAppearanceColor& row : sharedAppearanceColors()) {
            (first.*row.set)(QColor(0x65, 0x43, 0x21, 0x87));
        }
        first.resetDisplayColorsToDefaults();
        QCOMPARE(first.traceColor(), trace);
        QCOMPARE(first.fillColor(), fill);
        QCOMPARE(first.backgroundFillColor(), background);
        QCOMPARE(second.traceColor(), secondTrace);
        QCOMPARE(second.fillColor(), secondFill);
        QCOMPARE(second.backgroundFillColor(), secondBackground);
        // Exact inherited native seeds; trace/fill/background are outside
        // this reset even when they have explicit per-pan colors.
        for (const SharedAppearanceColor& row : sharedAppearanceColors()) {
            QCOMPARE((first.*row.get)(), row.ship);
            QCOMPARE((second.*row.get)(), row.ship);
        }
    }

    void receive_custom_load_draws_the_existing_computer_gradient_on_every_pan()
    {
        const QColor low(0x10, 0x20, 0x30);
        const QColor high(0xe0, 0xc0, 0xa0);
        const QString encoded = customGradientFixture(low, high);
        AppSettings::instance().setValue(u"DisplayWfCustomStops"_s, encoded);
        GradientPickerWidget reference;
        reference.setEncodedText(encoded);
        SpectrumWidget first;
        SpectrumWidget second;
        second.setPanIndex(3);
        first.loadSettings();
        second.loadSettings();
        configureManualWaterfall(first, WfColorScheme::Custom);
        configureManualWaterfall(second, WfColorScheme::Custom);
        for (const float dbm : {-120.0f, -100.0f, -80.0f}) {
            const QRgb expected = reference.colorAtPercent((dbm + 120.0f) / 40.0f).rgb();
            QCOMPARE(first.dbmToRgbForTest(dbm), expected);
            QCOMPARE(second.dbmToRgbForTest(dbm), expected);
        }
    }

    void receive_custom_updates_are_shared_persisted_and_independent_of_transmit()
    {
        SpectrumWidget first;
        SpectrumWidget second;
        second.setPanIndex(2);
        first.loadSettings();
        second.loadSettings();
        configureManualWaterfall(first, WfColorScheme::Custom);
        configureManualWaterfall(second, WfColorScheme::Custom);
        const QString transmit = customGradientFixture(Qt::red, Qt::yellow);
        first.setTxWfGradient(transmit);
        const QString encoded = customGradientFixture(Qt::green, Qt::blue);
        QSignalSpy changed(&second, &SpectrumWidget::displaySettingChanged);
        QVERIFY(QMetaObject::invokeMethod(&first, "setRxWfGradient", Qt::DirectConnection,
                                         Q_ARG(QString, encoded)));
        QCOMPARE(first.dbmToRgbForTest(-120), QColor(Qt::green).rgb());
        QCOMPARE(second.dbmToRgbForTest(-80), QColor(Qt::blue).rgb());
        QVERIFY(changed.count() > 0);
        QCOMPARE(first.txWfGradient(), transmit);
        QCOMPARE(first.wfLowThreshold(), -120.0f);
        QCOMPARE(first.wfHighThreshold(), -80.0f);
        first.saveSettings();
        second.saveSettings();
        QCOMPARE(AppSettings::instance().value(u"DisplayWfCustomStops"_s).toString(), encoded);
        QVERIFY(!first.perPanDisplayKeyBases().contains(u"DisplayWfCustomStops"_s));
        QVERIFY(!first.resettablePanDisplayKeyBases().contains(u"DisplayWfCustomStops"_s));
        QVERIFY(!AppSettings::instance().contains(u"DisplayWfCustomStops_0"_s));
        QVERIFY(!AppSettings::instance().contains(u"DisplayWfCustomStops_2"_s));
        first.resetPanDisplayOverrides();
        configureManualWaterfall(first, WfColorScheme::Custom);
        QCOMPARE(first.dbmToRgbForTest(-120), QColor(Qt::green).rgb());
        SpectrumWidget restarted;
        restarted.setPanIndex(7);
        restarted.loadSettings();
        configureManualWaterfall(restarted, WfColorScheme::Custom);
        QCOMPARE(restarted.dbmToRgbForTest(-80), QColor(Qt::blue).rgb());
    }

    void transmit_waterfall_levels_and_colors_belong_to_this_computer_data()
    {
        QTest::addColumn<int>("rowIndex");
        const auto& rows = sharedTransmitWaterfallSettings();
        for (int i = 0; i < rows.size(); ++i) { QTest::newRow(qPrintable(rows[i].key)) << i; }
    }

    void transmit_waterfall_levels_and_colors_belong_to_this_computer()
    {
        QFETCH(int, rowIndex);
        const auto& row = sharedTransmitWaterfallSettings()[rowIndex];
        auto& settings = AppSettings::instance();
        // Accepted HTML:400-407 assigns all four to CP (this computer).
        settings.setValue(row.key, row.historic);
        settings.setValue(row.key + u"_0"_s, row.pillOwn);
        settings.setValue(row.key + u"_7"_s, row.pillOwn);
        settings.setValue(row.key + u"_backup"_s, u"keep backup"_s);
        settings.setValue(row.key + u"_01"_s, u"keep noncanonical"_s);
        SpectrumWidget first;
        SpectrumWidget second;
        second.setPanIndex(2);
        first.loadSettings();
        second.loadSettings();
        QCOMPARE(row.read(first), row.historic);
        QCOMPARE(row.read(second), row.historic);
        QVERIFY(!settings.contains(row.key + u"_0"_s));
        QVERIFY(!settings.contains(row.key + u"_7"_s));
        QCOMPARE(settings.value(row.key + u"_backup"_s).toString(), u"keep backup"_s);
        QCOMPARE(settings.value(row.key + u"_01"_s).toString(), u"keep noncanonical"_s);
        configureManualWaterfall(first, WfColorScheme::Enhanced);
        configureManualWaterfall(second, WfColorScheme::Spectran);
        second.setWfLowThreshold(-110);
        second.setWfHighThreshold(-70);
        const QString rxDefinition = customGradientFixture(Qt::green, Qt::blue);
        first.setRxWfGradient(rxDefinition);
        const auto firstRx = first.effectiveWaterfallLevels();
        const auto secondRx = second.effectiveWaterfallLevels();
        bool callbackObserved = false;
        bool everyCacheCurrent = false;
        connect(&first, &SpectrumWidget::txWfSettingsChanged, &first, [&]() {
            callbackObserved = true;
            everyCacheCurrent = row.read(first) == row.edited && row.read(second) == row.edited;
            second.saveSettings(); // all recipient caches must already agree
        });
        row.apply(first, row.edited);
        QVERIFY(callbackObserved);
        QVERIFY(everyCacheCurrent);
        QCOMPARE(row.read(first), row.edited);
        QCOMPARE(row.read(second), row.edited);
        QCOMPARE(first.wfColorScheme(), WfColorScheme::Enhanced);
        QCOMPARE(second.wfColorScheme(), WfColorScheme::Spectran);
        QCOMPARE(first.effectiveWaterfallLevels(), firstRx);
        QCOMPARE(second.effectiveWaterfallLevels(), secondRx);
        QCOMPARE(first.rxWfGradient(), rxDefinition);
        QCOMPARE(second.rxWfGradient(), rxDefinition);
        first.saveSettings();
        second.saveSettings();
        QCOMPARE(settings.value(row.key).toString().toUpper(), row.edited.toUpper());
        QVERIFY(!first.perPanDisplayKeyBases().contains(row.key));
        QVERIFY(!first.resettablePanDisplayKeyBases().contains(row.key));
        QVERIFY(!settings.contains(row.key + u"_0"_s));
        QVERIFY(!settings.contains(row.key + u"_2"_s));
        first.resetPanDisplayOverridesForKeys({row.key});
        QCOMPARE(row.read(first), row.edited);
        QCOMPARE(first.effectiveWaterfallLevels(), firstRx);
        SpectrumWidget reopened;
        reopened.setPanIndex(7);
        reopened.loadSettings();
        QCOMPARE(row.read(reopened), row.edited);
    }

    void transmit_custom_definition_is_shared_and_independent_of_receive_and_palette()
    {
        auto& settings = AppSettings::instance();
        const QString historic = customGradientFixture(Qt::red, Qt::yellow);
        const QString pillOwn = customGradientFixture(Qt::cyan, Qt::magenta);
        const QString receive = customGradientFixture(Qt::green, Qt::blue);
        settings.setValue(u"DisplayTxWfGradient"_s, historic);
        settings.setValue(u"DisplayTxWfGradient_0"_s, pillOwn);
        settings.setValue(u"DisplayTxWfGradient_7"_s, pillOwn);
        settings.setValue(u"DisplayTxWfGradient_backup"_s, u"keep backup"_s);
        settings.setValue(u"DisplayWfCustomStops"_s, receive);
        SpectrumWidget first;
        SpectrumWidget second;
        second.setPanIndex(2);
        first.loadSettings();
        second.loadSettings();
        QCOMPARE(first.txWfGradient(), historic);
        QCOMPARE(second.txWfGradient(), historic);
        QVERIFY(!settings.contains(u"DisplayTxWfGradient_0"_s));
        QVERIFY(!settings.contains(u"DisplayTxWfGradient_7"_s));
        QCOMPARE(settings.value(u"DisplayTxWfGradient_backup"_s).toString(), u"keep backup"_s);
        first.setTxWfPalette(WfColorScheme::Custom);
        second.setTxWfPalette(WfColorScheme::Enhanced);
        first.setTxWfGradient(pillOwn);
        QCOMPARE(second.txWfGradient(), pillOwn);
        QCOMPARE(second.txWfPalette(), WfColorScheme::Enhanced);
        // Palette is CP too; the RX palette remains a pan choice.
        QCOMPARE(first.txWfPalette(), WfColorScheme::Enhanced);
        QCOMPARE(settings.value(u"DisplayWfCustomStops"_s).toString(), receive);
        first.saveSettings();
        second.saveSettings();
        QCOMPARE(settings.value(u"DisplayTxWfGradient"_s).toString(), pillOwn);
        QVERIFY(!first.perPanDisplayKeyBases().contains(u"DisplayTxWfGradient"_s));
        QVERIFY(!first.resettablePanDisplayKeyBases().contains(u"DisplayTxWfGradient"_s));
        QVERIFY(!settings.contains(u"DisplayTxWfGradient_0"_s));
        QVERIFY(!settings.contains(u"DisplayTxWfGradient_2"_s));
        first.resetPanDisplayOverrides();
        QCOMPARE(first.txWfGradient(), pillOwn);
        SpectrumWidget restarted;
        restarted.setPanIndex(7);
        restarted.loadSettings();
        QCOMPARE(restarted.txWfGradient(), pillOwn);
    }

    void receive_custom_without_a_definition_keeps_default_palette()
    {
        SpectrumWidget custom;
        SpectrumWidget standard;
        custom.loadSettings();
        standard.loadSettings();
        configureManualWaterfall(custom, WfColorScheme::Custom);
        configureManualWaterfall(standard, WfColorScheme::Default);
        for (const float dbm : {-120.0f, -110.0f, -100.0f, -90.0f, -80.0f}) {
            QCOMPARE(custom.dbmToRgbForTest(dbm), standard.dbmToRgbForTest(dbm));
        }
    }

    void non_clarity_modes_apply_centered_trims_to_effective_pair()
    {
        SpectrumWidget pan;
        pan.loadSettings();
        pan.setClarityEnabled(false);
        pan.setWfAgcEnabled(false);
        pan.setWaterfallNFAGCEnabled(false);
        pan.setWfLowThreshold(-120);
        pan.setWfHighThreshold(-80);
        pan.setWfBlackLevel(67);
        pan.setWfColorGain(70);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-115, -83}));
        QCOMPARE(pan.wfLowThreshold(), -120.0f);
        QCOMPARE(pan.wfHighThreshold(), -80.0f);

        pan.setWfAgcEnabled(true);
        pan.composeWaterfallActiveThresholds(QVector<float>(1024, -100));
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-107, -83}));

        pan.setWaterfallNFAGCEnabled(true);
        pan.setCoreWaterfallLevelsAvailable(true);
        pan.setCoreWaterfallLevels(-105, -80);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-100, -83}));

        pan.setWfAgcEnabled(false);
        pan.setWaterfallNFAGCEnabled(false);
        pan.setDbmRange(-140, -40);
        pan.setWfUseSpectrumMinMax(true);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-135, -43}));
        QCOMPARE(pan.wfLowThreshold(), -120.0f);
        QCOMPARE(pan.wfHighThreshold(), -80.0f);
    }

    void effective_waterfall_gap_and_automatic_high_carry_preserve_saved_pair()
    {
        SpectrumWidget pan;
        pan.loadSettings();
        pan.setClarityEnabled(false);
        pan.setWfAgcEnabled(false);
        pan.setWfLowThreshold(-100);
        pan.setWfHighThreshold(-98);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-100, -95}));
        QCOMPARE(pan.wfHighThreshold(), -98.0f);
        pan.setWfAgcEnabled(true);
        pan.setCoreWaterfallLevelsAvailable(true);
        pan.setCoreWaterfallLevels(-80.5f, -98);
        // JS Math.round(-80.5) is -80: the accepted pre-trim carry puts
        // high at -75 while the exact final minimum gap is -75.5.
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-80.5f, -75}));
        QCOMPARE(pan.wfHighThreshold(), -98.0f);
        pan.setCoreWaterfallLevels(-130, -98);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-130, -98}));
    }

    void scale_links_runtime_levels_without_rewriting_manual_endpoints()
    {
        SpectrumWidget pan;
        pan.setPanIndex(1); pan.loadSettings();
        pan.setClarityEnabled(false);
        pan.setWfAgcEnabled(false);
        pan.setWfLowThreshold(-125);
        pan.setWfHighThreshold(-75);
        pan.saveSettings();
        pan.setDbmRange(-140, -40);
        pan.setWfUseSpectrumMinMax(true);
        QCOMPARE(pan.wfLowThreshold(), -125.0f);
        QCOMPARE(pan.wfHighThreshold(), -75.0f);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-140, -40}));
        pan.setDbmRange(-130, -30);
        QCOMPARE(pan.wfLowThreshold(), -125.0f);
        QCOMPARE(pan.wfHighThreshold(), -75.0f);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-130, -30}));
        pan.setWfUseSpectrumMinMax(false);
        QCOMPARE(pan.effectiveWaterfallLevels(), (std::pair<float, float>{-125, -75}));
    }

    void track_floor_caps_min_at_stored_step_even_below_movement_gate()
    {
        SpectrumWidget pan;
        pan.loadSettings();
        pan.setAdjustGridMinToNoiseFloor(true);
        pan.setDbmRange(-80, -70);
        pan.testApplyNoiseFloor(-75);
        QCOMPARE(pan.gridMin(), -80);
        QCOMPARE(pan.gridMax(), -70);
        pan.setDbGridStep(15);
        pan.testApplyNoiseFloor(-79);
        // The floor moved less than 2 dB, but the new step needs 15 dB.
        QCOMPARE(pan.gridMin(), -85);
        QCOMPARE(pan.gridMax(), -70);
        pan.setDbmRange(-71, -70);
        pan.testApplyNoiseFloor(-71.5f);
        QCOMPARE(pan.gridMin(), -85);
        QCOMPARE(pan.gridMax(), -70);
        pan.setDbmRange(-69, -70);
        pan.testApplyNoiseFloor(-69.5f);
        QCOMPARE(pan.gridMin(), -85);
        QCOMPARE(pan.gridMax(), -70);
        const QVector<float> lines = pan.dbGridLinesForTest();
        QVERIFY(lines.isEmpty());
    }

    void track_floor_offset_acts_immediately_from_accepted_floor_without_save()
    {
        SpectrumWidget pan;
        pan.loadSettings();
        pan.setAdjustGridMinToNoiseFloor(true);
        pan.setDbmRange(-140, -40);
        pan.testApplyNoiseFloor(-120);
        QCOMPARE(pan.gridMin(), -120);
        pan.setNFOffsetGridFollow(5);
        QCOMPARE(pan.gridMin(), -115);
        pan.setMaintainNFAdjustDelta(true);
        const int span = pan.gridMax() - pan.gridMin();
        pan.testApplyNoiseFloor(-100);
        QCOMPARE(pan.gridMin(), -95);
        QCOMPARE(pan.gridMax() - pan.gridMin(), span);
    }

    void effective_level_notifications_are_local_and_write_no_settings()
    {
        SpectrumWidget pan, neighbor;
        pan.setPanIndex(1); pan.loadSettings();
        neighbor.setPanIndex(2); neighbor.loadSettings();
        pan.setClarityEnabled(false);
        pan.setWfAgcEnabled(true);
        pan.setWfHighThreshold(-80);
        pan.setCoreWaterfallLevelsAvailable(true);
        pan.saveSettings();
        const QMap<QString, QString> before = everythingExceptPanOverrides(-1, {});
        QSignalSpy changed(&pan, &SpectrumWidget::displaySettingChanged);
        QSignalSpy other(&neighbor, &SpectrumWidget::displaySettingChanged);
        pan.setCoreWaterfallLevels(-100, -80);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), u"DisplayWfEffectiveLevels"_s);
        QCOMPARE(other.count(), 0);
        QCOMPARE(everythingExceptPanOverrides(-1, {}), before);
        pan.setCoreWaterfallLevels(-100, -80);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(everythingExceptPanOverrides(-1, {}), before);
        QVERIFY(!pan.perPanDisplayKeyBases().contains(u"DisplayWfEffectiveLevels"_s));
    }

    void peak_details_and_db_step_save_restart_and_reset_only_one_pan()
    {
        auto& settings = AppSettings::instance();
        SpectrumWidget first, second;
        first.setPanIndex(1); first.loadSettings();
        second.setPanIndex(2); second.loadSettings();
        const QMap<QString, QString> inherited = peakDetailValues(first);
        for (auto it = inherited.cbegin(); it != inherited.cend(); ++it) {
            settings.setValue(it.key(), it.value());
        }
        QSignalSpy changed(&first, &SpectrumWidget::displaySettingChanged);
        first.setActivePeakHoldEnabled(true);
        first.setActivePeakHoldDurationMs(3500);
        first.setActivePeakHoldDropDbPerSec(12);
        first.setActivePeakHoldFill(true);
        first.setActivePeakHoldOnTx(true);
        first.setActivePeakHoldColor(QColor(0x12, 0x34, 0x56, 0x78));
        first.setPeakBlobsEnabled(true);
        first.setPeakBlobsCount(5);
        first.setPeakBlobsInsideFilterOnly(true);
        first.setPeakBlobsHoldEnabled(true);
        first.setPeakBlobsHoldMs(900);
        first.setPeakBlobsHoldDrop(true);
        first.setPeakBlobsFallDbPerSec(15);
        first.setPeakBlobColor(QColor(0x23, 0x45, 0x67, 0x89));
        first.setPeakBlobTextColor(QColor(0x34, 0x56, 0x78, 0x9a));
        first.setDbGridStep(7);
        const QMap<QString, QString> own = peakDetailValues(first);
        QCOMPARE(changed.count(), own.size());
        QCOMPARE(peakDetailValues(second), inherited);
        first.saveSettings();
        for (auto it = own.cbegin(); it != own.cend(); ++it) {
            QCOMPARE(settings.value(it.key()).toString(), inherited.value(it.key()));
            QCOMPARE(settings.value(PanDisplayKeys::panKey(it.key(), 1)).toString(), it.value());
            QVERIFY(!settings.contains(PanDisplayKeys::panKey(it.key(), 2)));
            QCOMPARE(first.perPanDisplayValue(it.key()), it.value());
            QVERIFY(first.resettablePanDisplayKeyBases().contains(it.key()));
        }
        SpectrumWidget reopened;
        reopened.setPanIndex(1); reopened.loadSettings();
        QCOMPARE(peakDetailValues(reopened), own);
        first.resetPanDisplayOverrides();
        QCOMPARE(peakDetailValues(first), inherited);
        first.saveSettings();
        for (auto it = own.cbegin(); it != own.cend(); ++it) {
            QVERIFY(!settings.contains(PanDisplayKeys::panKey(it.key(), 1)));
        }
    }

    void peak_overrides_load_from_own_keys_and_equal_master_clears_them()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(u"DisplayActivePeakHoldDurationMs"_s, u"2000"_s);
        settings.setValue(u"DisplayActivePeakHoldDurationMs_1"_s, u"3500"_s);
        settings.setValue(u"DisplayPeakBlobColor"_s, u"#FF4500FF"_s);
        settings.setValue(u"DisplayPeakBlobColor_1"_s, u"#23456789"_s);
        settings.setValue(u"DisplayGridStep"_s, u"10"_s);
        settings.setValue(u"DisplayGridStep_1"_s, u"7"_s);
        SpectrumWidget first, second;
        first.setPanIndex(1); first.loadSettings();
        second.setPanIndex(2); second.loadSettings();
        QCOMPARE(first.activePeakHoldDurationMs(), 3500);
        QCOMPARE(first.peakBlobColor(), QColor(0x23, 0x45, 0x67, 0x89));
        QCOMPARE(first.dbGridStep(), 7);
        QCOMPARE(second.activePeakHoldDurationMs(), 2000);
        QCOMPARE(second.dbGridStep(), 10);
        QSignalSpy changed(&first, &SpectrumWidget::displaySettingChanged);
        first.setActivePeakHoldDurationMs(3500);
        first.setPeakBlobColor(QColor(0x23, 0x45, 0x67, 0x89));
        first.setDbGridStep(7);
        QCOMPARE(changed.count(), 0);
        first.setActivePeakHoldDurationMs(2000);
        first.setPeakBlobColor(second.peakBlobColor());
        first.setDbGridStep(10);
        first.saveSettings();
        QVERIFY(!settings.contains(u"DisplayActivePeakHoldDurationMs_1"_s));
        QVERIFY(!settings.contains(u"DisplayPeakBlobColor_1"_s));
        QVERIFY(!settings.contains(u"DisplayGridStep_1"_s));
    }

    void peak_master_reset_clears_closed_pan_override_and_keeps_other_pages()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(u"DisplayActivePeakHoldDurationMs"_s, u"2000"_s);
        settings.setValue(u"DisplayActivePeakHoldDurationMs_9"_s, u"4500"_s);
        SpectrumWidget pan;
        pan.setPanIndex(1); pan.loadSettings();
        pan.setActivePeakHoldDurationMs(3500);
        pan.setWfOpacity(61);
        pan.saveSettings();
        PanDisplayKeys::writeDefault(u"DisplayActivePeakHoldDurationMs"_s, u"2500"_s);
        pan.resetPanDisplayOverridesForKeys({u"DisplayActivePeakHoldDurationMs"_s});
        QCOMPARE(pan.activePeakHoldDurationMs(), 2500);
        QCOMPARE(pan.wfOpacity(), 61);
        QVERIFY(!settings.contains(u"DisplayActivePeakHoldDurationMs_1"_s));
        QVERIFY(!settings.contains(u"DisplayActivePeakHoldDurationMs_9"_s));
        QCOMPARE(settings.value(u"DisplayWfOpacity_1"_s).toString(), u"61"_s);
    }

    void clarity_choice_belongs_to_the_pans_persistent_reset_scope()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(u"ClarityEnabled"_s, u"True"_s);
        settings.setValue(u"ClarityEnabled_1"_s, u"False"_s);
        SpectrumWidget pan;
        pan.setPanIndex(1);
        pan.loadSettings();
        QVERIFY(pan.perPanDisplayKeyBases().contains(u"ClarityEnabled"_s));
        QVERIFY(pan.resettablePanDisplayKeyBases().contains(u"ClarityEnabled"_s));
        QCOMPARE(pan.perPanShipDefault(u"ClarityEnabled"_s), u"True"_s);
        pan.saveSettings();
        QCOMPARE(settings.value(u"ClarityEnabled"_s).toString(), u"True"_s);
        QCOMPARE(settings.value(u"ClarityEnabled_1"_s).toString(), u"False"_s);
        pan.resetPanDisplayOverrides();
        pan.saveSettings();
        QVERIFY(!settings.contains(u"ClarityEnabled_1"_s));
        QCOMPARE(settings.value(u"ClarityEnabled"_s).toString(), u"True"_s);
    }

    void clarity_choices_announce_and_save_only_the_requested_pan()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(u"ClarityEnabled"_s, u"False"_s);
        SpectrumWidget pan0, pan1;
        pan0.setPanIndex(0); pan0.loadSettings();
        pan1.setPanIndex(1); pan1.loadSettings();
        QVERIFY(!pan0.clarityEnabled());
        QVERIFY(!pan1.clarityEnabled());
        QSignalSpy changed(&pan0, &SpectrumWidget::displaySettingChanged);
        pan0.setClarityEnabled(false);
        QCOMPARE(changed.count(), 0);
        QVERIFY(!settings.contains(u"ClarityEnabled_0"_s));
        pan0.setClarityEnabled(true);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), u"ClarityEnabled"_s);
        QVERIFY(pan0.settingsSavePendingForTest());
        QVERIFY(!pan0.clarityActive());
        QVERIFY(!pan1.clarityEnabled());
        pan0.saveSettings();
        QCOMPARE(settings.value(u"ClarityEnabled"_s).toString(), u"False"_s);
        QCOMPARE(settings.value(u"ClarityEnabled_0"_s).toString(), u"True"_s);
        QVERIFY(!settings.contains(u"ClarityEnabled_1"_s));
        SpectrumWidget reopened;
        reopened.setPanIndex(0); reopened.loadSettings();
        QVERIFY(reopened.clarityEnabled());
        pan0.setClarityActive(true);
        pan0.saveSettings();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(settings.value(u"ClarityEnabled_0"_s).toString(), u"True"_s);
        pan0.setClarityEnabled(true);
        QCOMPARE(changed.count(), 1);
        pan0.setClarityEnabled(false);
        pan0.saveSettings();
        QCOMPARE(changed.count(), 2);
        QVERIFY(!settings.contains(u"ClarityEnabled_0"_s));
    }

    void per_page_reset_preserves_pending_other_values_and_rejects_view_keys()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(u"ClarityEnabled"_s, u"True"_s);
        SpectrumWidget pan;
        pan.setPanIndex(1); pan.loadSettings();
        pan.setClarityEnabled(false);
        pan.setWfOpacity(61);
        pan.setFrequencyRange(14'100'000.0, 96000.0);
        QSignalSpy reset(&pan, &SpectrumWidget::panDisplayReset);
        pan.resetPanDisplayOverridesForKeys({u"ClarityEnabled"_s, u"DisplayBandwidth"_s,
                                             u"NotAPictureKey"_s});
        QCOMPARE(reset.count(), 1);
        QVERIFY(pan.clarityEnabled());
        QVERIFY(!settings.contains(u"ClarityEnabled_1"_s));
        QCOMPARE(pan.wfOpacity(), 61);
        QCOMPARE(pan.bandwidth(), 96000.0);
        QCOMPARE(settings.value(u"DisplayWfOpacity_1"_s).toString(), u"61"_s);
        QCOMPARE(settings.value(u"DisplayBandwidth_1"_s).toString(), u"96000"_s);
        const QMap<QString, QString> stored = everythingExceptPanOverrides(-1, {});
        pan.resetPanDisplayOverridesForKeys({u"DisplayBandwidth"_s, u"NotAPictureKey"_s});
        QCOMPARE(reset.count(), 1);
        QCOMPARE(everythingExceptPanOverrides(-1, {}), stored);
    }

    void current_display_values_are_read_without_flushing_or_announcing()
    {
        SpectrumWidget pan;
        pan.setPanIndex(1); pan.loadSettings();
        pan.setWfOpacity(61);
        pan.setPanFillEnabled(false);
        QSignalSpy changed(&pan, &SpectrumWidget::displaySettingChanged);
        const QMap<QString, QString> before = everythingExceptPanOverrides(-1, {});
        QVERIFY(pan.settingsSavePendingForTest());
        QCOMPARE(pan.perPanDisplayValue(u"DisplayWfOpacity"_s), u"61"_s);
        QCOMPARE(pan.perPanDisplayValue(u"DisplayPanFill"_s), u"False"_s);
        QCOMPARE(pan.perPanDisplayValue(u"ClarityEnabled"_s), u"True"_s);
        QVERIFY(pan.perPanDisplayValue(u"Display3DFloorDepth"_s).isEmpty());
        QVERIFY(pan.perPanDisplayValue(u"Unknown"_s).isEmpty());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(everythingExceptPanOverrides(-1, {}), before);
        QVERIFY(pan.settingsSavePendingForTest());
    }

    void closed_pan_ship_defaults_match_every_live_inventory_key_without_writes()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(u"ClosedDefaultsSentinel"_s, u"unchanged"_s);
        const QMap<QString, QString> stored = everythingExceptPanOverrides(-1, {});
        const qsizetype widgetsBefore = QApplication::allWidgets().size();
        const QString closedClarity = SpectrumWidget::perPanShipDefaultForClosedPan(
            u"ClarityEnabled"_s);
        const QString closedFill = DisplaySettingsModel::perPanShipDefaultForClosedPan(
            u"DisplayPanFill"_s);
        QCOMPARE(closedClarity, u"True"_s);
        QCOMPARE(closedFill, u"True"_s);
        QCOMPARE(QApplication::allWidgets().size(), widgetsBefore);
        QCOMPARE(everythingExceptPanOverrides(-1, {}), stored);

        SpectrumWidget fresh;
        fresh.loadSettings();
        const QStringList keys = fresh.perPanDisplayKeyBases();
        const QStringList modelKeys = fresh.displaySettings()->perPanKeyBases();
        const QMap<QString, QString> afterConstruction = everythingExceptPanOverrides(-1, {});
        const qsizetype widgetsWithFresh = QApplication::allWidgets().size();
        QSignalSpy changed(&fresh, &SpectrumWidget::displaySettingChanged);
        QSignalSpy reset(&fresh, &SpectrumWidget::panDisplayReset);
        QMap<QString, QString> defaults;
        for (const QString& key : keys) {
            const QString closed = SpectrumWidget::perPanShipDefaultForClosedPan(key);
            QCOMPARE(closed, fresh.perPanShipDefault(key));
            QCOMPARE(closed, fresh.perPanDisplayValue(key));
            defaults.insert(key, closed);
        }
        for (const QString& key : modelKeys) {
            const QString closed = DisplaySettingsModel::perPanShipDefaultForClosedPan(key);
            QCOMPARE(closed, fresh.displaySettings()->perPanShipDefault(key));
            QCOMPARE(closed, fresh.displaySettings()->perPanValue(key));
            QCOMPARE(closed, defaults.value(key));
        }
        QVERIFY(SpectrumWidget::perPanShipDefaultForClosedPan(u"Unknown"_s).isEmpty());
        QVERIFY(SpectrumWidget::perPanShipDefaultForClosedPan(u"Display3DFloorDepth"_s).isEmpty());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(reset.count(), 0);
        QCOMPARE(QApplication::allWidgets().size(), widgetsWithFresh);
        QCOMPARE(everythingExceptPanOverrides(-1, {}), afterConstruction);

        // Saved masters, overrides and current edits cannot affect ship
        // defaults. In particular a closed-pan query must not load a
        // temporary widget from these settings to find its answer.
        settings.setValue(u"ClarityEnabled"_s, u"False"_s);
        settings.setValue(u"DisplayWfOpacity"_s, u"25"_s);
        settings.setValue(u"DisplayPanFill_9"_s, u"False"_s);
        fresh.setWfOpacity(61);
        fresh.setPanFillEnabled(false);
        const QMap<QString, QString> edited = everythingExceptPanOverrides(-1, {});
        changed.clear();
        for (const QString& key : keys) {
            QCOMPARE(SpectrumWidget::perPanShipDefaultForClosedPan(key), defaults.value(key));
        }
        for (const QString& key : modelKeys) {
            QCOMPARE(DisplaySettingsModel::perPanShipDefaultForClosedPan(key), defaults.value(key));
        }
        QCOMPARE(everythingExceptPanOverrides(-1, {}), edited);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(reset.count(), 0);
        QCOMPARE(QApplication::allWidgets().size(), widgetsWithFresh);
        QVERIFY(fresh.settingsSavePendingForTest());
    }

    void markers_and_tracking_keep_pan_choices_across_restart_and_reset()
    {
        auto& s = AppSettings::instance();
        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        SpectrumWidget pan2;
        pan2.setPanIndex(2);
        const QMap<QString, QString> inherited = markerAndTrackingValues(pan1);
        for (auto it = inherited.cbegin(); it != inherited.cend(); ++it) {
            s.setValue(it.key(), it.value());
        }
        QSignalSpy changed(&pan1, &SpectrumWidget::displaySettingChanged);
        pan1.setShowBinWidth(true);
        pan1.setShowNoiseFloor(true);
        pan1.setShowNoiseFloorPosition(SpectrumWidget::OverlayPosition::TopRight);
        pan1.setNoiseFloorColor(QColor(u"#ff123456"_s));
        pan1.setNoiseFloorTextColor(QColor(u"#ff234567"_s));
        pan1.setNoiseFloorFastColor(QColor(u"#ff345678"_s));
        pan1.setNoiseFloorLineWidth(2.5F);
        pan1.setNFShiftDbm(3.5F);
        pan1.setShowPeakValueOverlay(true);
        pan1.setPeakValuePosition(SpectrumWidget::OverlayPosition::BottomLeft);
        pan1.setPeakTextDelayMs(750);
        pan1.setPeakValueColor(QColor(u"#ff456789"_s));
        pan1.setAdjustGridMinToNoiseFloor(true);
        pan1.setNFOffsetGridFollow(6);
        pan1.setMaintainNFAdjustDelta(true);
        const QMap<QString, QString> own = markerAndTrackingValues(pan1);
        QCOMPARE(markerAndTrackingValues(pan2), inherited);
        QCOMPARE(changed.count(), own.size());
        pan1.saveSettings();
        for (auto it = own.cbegin(); it != own.cend(); ++it) {
            QCOMPARE(s.value(it.key()).toString(), inherited.value(it.key()));
            QCOMPARE(s.value(PanDisplayKeys::panKey(it.key(), 1)).toString(), it.value());
            QVERIFY(!s.contains(PanDisplayKeys::panKey(it.key(), 2)));
        }
        SpectrumWidget reopened;
        reopened.setPanIndex(1);
        reopened.loadSettings();
        QCOMPARE(markerAndTrackingValues(reopened), own);
        QCOMPARE(markerAndTrackingValues(pan2), inherited);
        pan1.resetPanDisplayOverrides();
        QCOMPARE(markerAndTrackingValues(pan1), inherited);
        pan1.saveSettings();
        for (auto it = own.cbegin(); it != own.cend(); ++it) {
            QVERIFY(!s.contains(PanDisplayKeys::panKey(it.key(), 1)));
        }
    }

    void calibration_holds_and_restores_each_pans_own_tracking_choice()
    {
        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        SpectrumWidget pan2;
        pan2.setPanIndex(2);
        pan1.setAdjustGridMinToNoiseFloor(true);
        pan1.saveSettings();
        pan2.saveSettings();
        SpectrumWidget::setGridFollowSaveHold(true);
        const bool heldPan1 = pan1.adjustGridMinToNoiseFloor();
        const bool heldPan2 = pan2.adjustGridMinToNoiseFloor();
        pan1.saveSettings();
        pan2.saveSettings();
        const QString savedPan1 = PanDisplayKeys::read(
            u"DisplayAdjustGridMinToNoiseFloor"_s, 1, u"False"_s).toString();
        const QString savedPan2 = PanDisplayKeys::read(
            u"DisplayAdjustGridMinToNoiseFloor"_s, 2, u"False"_s).toString();
        AppSettings::instance().setValue(u"DisplayAdjustGridMinToNoiseFloor_3"_s, u"True"_s);
        SpectrumWidget latePan;
        latePan.setPanIndex(3);
        latePan.loadSettings();
        const bool heldLatePan = latePan.adjustGridMinToNoiseFloor();
        latePan.saveSettings();
        const QString savedLatePan = PanDisplayKeys::read(
            u"DisplayAdjustGridMinToNoiseFloor"_s, 3, u"False"_s).toString();
        SpectrumWidget::setGridFollowSaveHold(std::nullopt);
        QVERIFY(!heldPan1);
        QVERIFY(!heldPan2);
        QCOMPARE(savedPan1, u"True"_s);
        QCOMPARE(savedPan2, u"False"_s);
        QVERIFY(!heldLatePan);
        QCOMPARE(savedLatePan, u"True"_s);
        QVERIFY(pan1.adjustGridMinToNoiseFloor());
        QVERIFY(!pan2.adjustGridMinToNoiseFloor());
        QVERIFY(latePan.adjustGridMinToNoiseFloor());
    }

    void normalization_is_per_pan_and_announces_live_changes()
    {
        auto& s = AppSettings::instance();
        s.setValue(u"DisplayDispNormalize"_s, u"True"_s);
        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        SpectrumWidget pan2;
        pan2.setPanIndex(2);
        pan2.loadSettings();
        QVERIFY(pan1.dispNormalize());
        QVERIFY(pan2.dispNormalize());
        QSignalSpy changed(&pan1, &SpectrumWidget::displaySettingChanged);

        pan1.setDispNormalize(false);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), u"DisplayDispNormalize"_s);
        QVERIFY(pan1.settingsSavePendingForTest());
        QVERIFY(pan2.dispNormalize());
        pan1.saveSettings();
        QCOMPARE(s.value(u"DisplayDispNormalize"_s).toString(), u"True"_s);
        QCOMPARE(s.value(u"DisplayDispNormalize_1"_s).toString(), u"False"_s);
        QVERIFY(!s.contains(u"DisplayDispNormalize_2"_s));

        SpectrumWidget reopened;
        reopened.setPanIndex(1);
        reopened.loadSettings();
        QVERIFY(!reopened.dispNormalize());
        pan1.setDispNormalize(false);
        QCOMPARE(changed.count(), 1);
        pan1.setDispNormalize(true);
        pan1.saveSettings();
        QVERIFY(!s.contains(u"DisplayDispNormalize_1"_s));
        pan1.setDispNormalize(false);
        pan1.saveSettings();
        pan1.resetPanDisplayOverrides();
        QVERIFY(pan1.dispNormalize());
        QVERIFY(!s.contains(u"DisplayDispNormalize_1"_s));
    }

    // ── 1. An untouched pan inherits the Setup default ───────────────────
    //
    // The finding. Before the 2026-07-30 fix this returned the hardcoded
    // ship default, so a new pan opened looking nothing like the one it was
    // placed beside.
    void an_untouched_pan_reads_the_setup_default()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFloatKey, u"3"_s);   // the Setup default says 3

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();

        QVERIFY2(qFuzzyCompare(pan1.lineWidth(), 3.0F),
            qPrintable(u"pan 1 should inherit the default line width of 3, got %1"_s
                           .arg(static_cast<double>(pan1.lineWidth()))));
    }

    // ── 2. Its own value wins once it has one ────────────────────────────
    //
    // The other half of "follow until you say otherwise". Without this the
    // fallback would be a permanent link and the per-pan keys that already
    // exist in the settings file would be unreachable.
    void a_pan_with_its_own_value_does_not_inherit()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFloatKey,  u"3"_s);
        s.setValue(kFloatKey1, u"4.5"_s);

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();

        QVERIFY2(qFuzzyCompare(pan1.lineWidth(), 4.5F),
            qPrintable(u"pan 1 set its own line width of 4.5, got %1"_s
                           .arg(static_cast<double>(pan1.lineWidth()))));
    }

    // ── 3. Pan 0 is a pan like any other ─────────────────────────────────
    //
    // It follows the Setup default until it has "<base>_0", and then that
    // wins. (Before the split pan 0's own key WAS the bare key.)
    void pan_zero_reads_its_own_key_then_the_setup_default()
    {
        auto& s = AppSettings::instance();
        s.setValue(kFloatKey, u"2.5"_s);

        SpectrumWidget following;
        following.setPanIndex(0);
        following.loadSettings();
        QVERIFY(qFuzzyCompare(following.lineWidth(), 2.5F));

        s.setValue(kFloatKey0, u"3.5"_s);
        SpectrumWidget own;
        own.setPanIndex(0);
        own.loadSettings();
        QVERIFY(qFuzzyCompare(own.lineWidth(), 3.5F));
    }

    // ── 4. Neither set still lands on the ship default ───────────────────
    //
    // First launch, no settings file. The inheritance must not swallow the
    // calibrated defaults, which were tuned against a live ANAN-G2 and are
    // what a new install is judged on.
    void with_nothing_set_the_ship_default_still_applies()
    {
        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();

        QVERIFY2(qFuzzyCompare(pan1.lineWidth(), 1.0F),
            qPrintable(u"expected the 1.0 ship default, got %1"_s
                           .arg(static_cast<double>(pan1.lineWidth()))));
        // The range a pan shows before the band store gives it one.
        QVERIFY2(qFuzzyCompare(pan1.refLevel() + 1.0F, -48.0F + 1.0F),
            qPrintable(u"expected the -48.0 ship default, got %1"_s
                           .arg(static_cast<double>(pan1.refLevel()))));
    }

    void trace_fill_color_alpha_survives_a_fresh_widget()
    {
        auto& s = AppSettings::instance();
        const QColor selected(12, 84, 193, 73);

        SpectrumWidget writer;
        writer.setPanIndex(0);
        writer.setFillColor(selected);
        writer.saveSettings();

        // Pan 0's own key, not the Setup default.
        QCOMPARE(s.value(kFillColorKey0).toString(), selected.name(QColor::HexArgb));
        QVERIFY(!s.contains(kFillColorKey));
        SpectrumWidget reader;
        reader.setPanIndex(0);
        reader.loadSettings();
        QCOMPARE(reader.fillColor(), selected);
    }

    void trace_fill_color_follows_the_setup_default_until_overridden()
    {
        auto& s = AppSettings::instance();
        const QColor first(10, 20, 30, 40);
        const QColor second(110, 120, 130, 140);
        const QColor later(200, 190, 180, 170);
        const QColor panZeros(1, 2, 3, 4);

        PanDisplayKeys::writeDefault(kFillColorKey, first.name(QColor::HexArgb));

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        QCOMPARE(pan1.fillColor(), first);
        // Following is not copying: saving an untouched pan stores nothing.
        pan1.saveSettings();
        QVERIFY(!s.contains(kFillColorKey1));

        pan1.setFillColor(second);
        pan1.saveSettings();
        QCOMPARE(s.value(kFillColorKey1).toString(), second.name(QColor::HexArgb));
        QCOMPARE(s.value(kFillColorKey).toString(), first.name(QColor::HexArgb));

        // A master change reaches a previously independent pan too.
        PanDisplayKeys::writeDefault(kFillColorKey, later.name(QColor::HexArgb));
        SpectrumWidget independent;
        independent.setPanIndex(1);
        independent.loadSettings();
        QCOMPARE(independent.fillColor(), later);

        // Pan 0 choosing a colour is pan 0's business alone.
        SpectrumWidget pan0;
        pan0.setPanIndex(0);
        pan0.loadSettings();
        QCOMPARE(pan0.fillColor(), later);
        pan0.setFillColor(panZeros);
        pan0.saveSettings();
        QCOMPARE(s.value(kFillColorKey0).toString(), panZeros.name(QColor::HexArgb));
        QCOMPARE(s.value(kFillColorKey).toString(), later.name(QColor::HexArgb));

        SpectrumWidget inheriting;
        inheriting.setPanIndex(2);
        inheriting.loadSettings();
        QCOMPARE(inheriting.fillColor(), later);
        QVERIFY(!s.contains(kFillColorKey2));
    }

    void trace_fill_color_uses_cyan_for_unset_or_invalid_storage()
    {
        auto& s = AppSettings::instance();

        SpectrumWidget fresh;
        fresh.setPanIndex(1);
        fresh.loadSettings();
        QCOMPARE(fresh.fillColor(), QColor(0x00, 0xe5, 0xff));

        s.setValue(kFillColorKey1, u"invalid-colour"_s);
        SpectrumWidget invalid;
        invalid.setPanIndex(1);
        invalid.loadSettings();
        QCOMPARE(invalid.fillColor(), QColor(0x00, 0xe5, 0xff));
    }

    void existing_grid_color_is_shared_and_retires_old_pan_values()
    {
        auto& s = AppSettings::instance();
        const QColor first(40, 70, 100, 130);
        const QColor second(12, 34, 56, 78);

        PanDisplayKeys::writeDefault(kGridColorKey, first.name(QColor::HexArgb));

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        QCOMPARE(pan1.gridColor(), first);
        QVERIFY(!s.contains(kGridColorKey1));

        pan1.setGridColor(second);
        pan1.saveSettings();
        QVERIFY(!s.contains(kGridColorKey1));
        QCOMPARE(s.value(kGridColorKey).toString(), second.name(QColor::HexArgb));
        SpectrumWidget overrideReader;
        overrideReader.setPanIndex(1);
        overrideReader.loadSettings();
        QCOMPARE(overrideReader.gridColor(), second);

        s.setValue(kGridColorKey1, u"invalid-colour"_s);
        SpectrumWidget invalidReader;
        invalidReader.setPanIndex(1);
        invalidReader.loadSettings();
        QCOMPARE(invalidReader.gridColor(), second);
        QVERIFY(!s.contains(kGridColorKey1));
        QCOMPARE(s.value(kGridColorKey).toString(), second.name(QColor::HexArgb));
    }

    // ── Old-install guard ────────────────────────────────────────────────
    //
    // The settings every user's display loads from. An install written
    // before the Setup-default / per-pan-override split holds bare keys for
    // pan 0 and a full set of "_1" keys for pan 1 (the old save wrote every
    // key on every save). Both pans must load exactly what they loaded
    // then. Written first and seen passing on the code before the split;
    // one row per read path (model int / float / bool, readFloat, readInt,
    // readBool, readColor, the transmit-grid raw reads and the transmit
    // waterfall keys that were read without a pan-0 fallback).
    void an_old_install_loads_what_it_loaded_before()
    {
        auto& s = AppSettings::instance();
        struct Row { QString base; QString pan0; QString pan1; };
        const QList<Row> rows = {
            // Display pill plan Task 9: the two range keys are the one
            // exception to "loads what it loaded before". The receive range
            // is the band store's now, so these are never read; they are
            // also never rewritten or deleted, which the loop after the
            // saves checks along with every other row.
            { u"DisplayGridMax"_s,            u"-30"_s,        u"-55"_s },
            { u"DisplayGridMin"_s,            u"-110"_s,       u"-125"_s },
            { u"DisplayWfColorGain"_s,        u"61"_s,         u"23"_s },
            { u"DisplayPanFill"_s,            u"False"_s,      u"True"_s },
            { u"Display3DGain"_s,             u"33"_s,         u"88"_s },
            { u"DisplayWfHighLevel"_s,        u"-40"_s,        u"-71"_s },
            { u"DisplayWfOpacity"_s,          u"80"_s,         u"35"_s },
            { u"DisplayWfUpdatePeriodMs"_s,   u"50"_s,         u"90"_s },
            { u"DisplayGridEnabled"_s,        u"False"_s,      u"True"_s },
            { u"DisplayWfAgc"_s,              u"False"_s,      u"True"_s },
            { u"DisplayLineWidth"_s,          u"2.5"_s,        u"3"_s },
            { u"DisplayFillColor"_s,          u"#ff102030"_s,  u"#80a0b0c0"_s },
            { u"DisplayRxFilterColor"_s,      u"#40112233"_s,  u"#50445566"_s },
            { u"DisplayTxGridRefLevel"_s,     u"10"_s,         u"-5"_s },
            { u"DisplayTxGridDynamicRange"_s, u"90"_s,         u"70"_s },
            { u"DisplayTxWfLowLevel"_s,       u"-60"_s,        u"-80"_s },
            { u"DisplayTxWfPalette"_s,        u"2"_s,          u"5"_s },
            { u"DisplayTxWfLowColor"_s,       u"#FF010203"_s,  u"#FF0A0B0C"_s },
            // Historic computer TX definition wins; numbered pill
            // definitions retire under the approved shared scope.
            { u"DisplayTxWfGradient"_s,       u"0|#FF000000|1|#FFFFFFFF"_s, QString() },
            { u"DisplayWaterfallHistoryMs"_s, u"300000"_s,     u"600000"_s },
        };
        for (const Row& r : rows) {
            s.setValue(r.base, r.pan0);
            s.setValue(r.base + u"_1"_s, r.pan1);
        }

        SpectrumWidget pan0;
        pan0.setPanIndex(0);
        pan0.loadSettings();
        QCOMPARE(pan0.refLevel(), -48.0F);       // not the old key's -30
        QCOMPARE(pan0.dynamicRange(), 68.0F);
        QCOMPARE(pan0.wfColorGain(), 61);
        QCOMPARE(pan0.panFillEnabled(), false);
        QCOMPARE(pan0.dssGain(), 33);
        QCOMPARE(pan0.wfHighThreshold(), -40.0F);
        QCOMPARE(pan0.wfOpacity(), 80);
        QCOMPARE(pan0.wfUpdatePeriodMs(), 50);
        QCOMPARE(pan0.gridEnabled(), false);
        QCOMPARE(pan0.wfAgcEnabled(), false);
        QCOMPARE(pan0.lineWidth(), 2.5F);
        QCOMPARE(pan0.fillColor(), QColor::fromString(u"#ff102030"_s));
        QCOMPARE(pan0.rxFilterColor(), QColor::fromString(u"#40112233"_s));
        QCOMPARE(pan0.transmitRefLevel(), 10.0F);
        QCOMPARE(pan0.transmitDynamicRange(), 90.0F);
        QCOMPARE(pan0.txWfLowLevel(), -60);
        QCOMPARE(static_cast<int>(pan0.txWfPalette()), 2);
        QCOMPARE(pan0.txWfLowColor(), QColor::fromString(u"#FF010203"_s));
        QCOMPARE(pan0.txWfGradient(), u"0|#FF000000|1|#FFFFFFFF"_s);
        QCOMPARE(pan0.waterfallHistoryMs(), qint64(300000));

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        QCOMPARE(pan1.refLevel(), -48.0F);       // not the old key's -55
        QCOMPARE(pan1.dynamicRange(), 68.0F);
        QCOMPARE(pan1.wfColorGain(), 23);
        QCOMPARE(pan1.panFillEnabled(), true);
        QCOMPARE(pan1.dssGain(), 88);
        QCOMPARE(pan1.wfHighThreshold(), -71.0F);
        QCOMPARE(pan1.wfOpacity(), 35);
        QCOMPARE(pan1.wfUpdatePeriodMs(), 90);
        QCOMPARE(pan1.gridEnabled(), true);
        QCOMPARE(pan1.wfAgcEnabled(), true);
        QCOMPARE(pan1.lineWidth(), 3.0F);
        QCOMPARE(pan1.fillColor(), QColor::fromString(u"#80a0b0c0"_s));
        QCOMPARE(pan1.rxFilterColor(), QColor::fromString(u"#40112233"_s));
        // Historic bare TX pair wins; the pill numbered keys are retired.
        QCOMPARE(pan1.transmitRefLevel(), 10.0F);
        QCOMPARE(pan1.transmitDynamicRange(), 90.0F);
        QCOMPARE(pan1.txWfLowLevel(), -60);
        QCOMPARE(static_cast<int>(pan1.txWfPalette()), 2);
        QCOMPARE(pan1.txWfLowColor(), QColor::fromString(u"#FF010203"_s));
        QCOMPARE(pan1.txWfGradient(), u"0|#FF000000|1|#FFFFFFFF"_s);
        QCOMPARE(pan1.waterfallHistoryMs(), qint64(600000));

        // And the first save after the upgrade leaves that install as it
        // was: pan 0 matches the bare keys so it writes nothing of its own,
        // and pan 1 rewrites its own keys with the values they held.
        pan0.saveSettings();
        pan1.saveSettings();
        for (const Row& r : rows) {
            QVERIFY2(!s.contains(r.base + u"_0"_s), qPrintable(r.base));
            QCOMPARE(s.value(r.base).toString(), r.pan0);
            if (r.base == u"DisplayTxGridRefLevel"_s
                    || r.base == u"DisplayTxGridDynamicRange"_s
                    || r.base == u"DisplayRxFilterColor"_s
                    || r.base == u"DisplayTxWfGradient"_s
                    || r.base == u"DisplayTxWfLowLevel"_s
                    || r.base == u"DisplayTxWfHighLevel"_s
                    || r.base == u"DisplayTxWfPalette"_s
                    || r.base == u"DisplayTxWfLowColor"_s) {
                QVERIFY2(!s.contains(r.base + u"_1"_s), qPrintable(r.base));
                continue;
            }
            QVERIFY2(s.contains(r.base + u"_1"_s), qPrintable(r.base));
            QCOMPARE(s.value(r.base + u"_1"_s).toString(), r.pan1);
        }
        SpectrumWidget pan1Again;
        pan1Again.setPanIndex(1);
        pan1Again.loadSettings();
        QCOMPARE(pan1Again.lineWidth(), 3.0F);
        QCOMPARE(pan1Again.wfOpacity(), 35);
        QCOMPARE(pan1Again.fillColor(), QColor::fromString(u"#80a0b0c0"_s));
        QCOMPARE(pan1Again.txWfGradient(), u"0|#FF000000|1|#FFFFFFFF"_s);
    }

    // ── Sparse overrides (display pill plan Task 1) ──────────────────────

    // The acceptance case. With a Setup default of 80 and no key of their
    // own, pan 0 and pan 2 both show 80. Pan 2 changes one value, and the
    // save writes that one key: no other "_2" key appears.
    void one_change_writes_one_key()
    {
        auto& s = AppSettings::instance();
        s.setValue(kOpacityKey, u"80"_s);

        SpectrumWidget pan0;
        pan0.setPanIndex(0);
        pan0.loadSettings();
        QCOMPARE(pan0.wfOpacity(), 80);

        SpectrumWidget pan2;
        pan2.setPanIndex(2);
        pan2.loadSettings();
        QCOMPARE(pan2.wfOpacity(), 80);

        pan2.setWfOpacity(60);
        pan2.saveSettings();
        QCOMPARE(keysOfPan(2), QStringList{u"DisplayWfOpacity_2"_s});
        QCOMPARE(s.value(u"DisplayWfOpacity_2"_s).toString(), u"60"_s);
        QCOMPARE(s.value(kOpacityKey).toString(), u"80"_s);
    }

    // A pan set back to the master's value follows it again.
    void a_pan_set_back_to_the_inherited_value_clears_its_key()
    {
        auto& s = AppSettings::instance();
        s.setValue(kOpacityKey, u"80"_s);
        s.setValue(u"DisplayWfOpacity_2"_s, u"60"_s);

        SpectrumWidget pan2;
        pan2.setPanIndex(2);
        pan2.loadSettings();
        QCOMPARE(pan2.wfOpacity(), 60);
        pan2.setWfOpacity(80);
        pan2.saveSettings();

        QVERIFY(!s.contains(u"DisplayWfOpacity_2"_s));
        QVERIFY(keysOfPan(2).isEmpty());
        QCOMPARE(pan2.wfOpacity(), 80);
    }

    // Pan 0 changing a value writes "<base>_0", never the bare key, through
    // the coalesced save and through the setters that write at once.
    void pan_zero_writes_its_own_key_never_the_default()
    {
        auto& s = AppSettings::instance();
        s.setValue(kOpacityKey, u"80"_s);
        const QColor rx(1, 2, 3, 4);

        SpectrumWidget pan0;
        pan0.setPanIndex(0);
        pan0.loadSettings();
        pan0.setWfOpacity(55);
        pan0.setLineWidth(3.0F);
        pan0.setRefLevel(-20.0F);              // live only (Task 9): no key
        pan0.setRxFilterColor(rx);             // writes at once
        pan0.setWaterfallHistoryMs(300000);    // writes at once
        QCOMPARE(s.value(u"DisplayRxFilterColor"_s).toString(), rx.name(QColor::HexArgb));
        QVERIFY(!s.contains(u"DisplayRxFilterColor_0"_s));
        QCOMPARE(s.value(u"DisplayWaterfallHistoryMs_0"_s).toString(), u"300000"_s);
        pan0.saveSettings();

        QCOMPARE(s.value(u"DisplayWfOpacity_0"_s).toString(), u"55"_s);
        QCOMPARE(s.value(u"DisplayLineWidth_0"_s).toString(), u"3"_s);
        QVERIFY(!s.contains(u"DisplayGridMax_0"_s));
        QVERIFY(!s.contains(u"DisplayGridMin_0"_s));
        QVERIFY(!s.contains(u"DisplayGridMax"_s));
        QCOMPARE(s.value(kOpacityKey).toString(), u"80"_s);
        // No bare per-pan key was created or changed by pan 0.
        const QStringList bases = pan0.perPanDisplayKeyBases();
        for (const QString& base : bases) {
            if (base == kOpacityKey) { continue; }
            QVERIFY2(!s.contains(base), qPrintable(base));
        }
    }

    // A widget nobody touched stores nothing of its own, on a fresh install
    // and for any pan. This is also the guard that every fallback handed to
    // the sparse write is the same default the load uses for that key: a
    // mismatch shows up here as a key written for a value nobody changed.
    void an_untouched_widget_saves_no_per_pan_key_data()
    {
        QTest::addColumn<int>("pan");
        QTest::newRow("pan 0") << 0;
        QTest::newRow("pan 3") << 3;
    }
    void an_untouched_widget_saves_no_per_pan_key()
    {
        QFETCH(int, pan);
        auto& s = AppSettings::instance();

        // After a load of nothing, which is how every pan starts. (A widget
        // saved without ever loading is not a state the application
        // reaches, and one member, the transmit filter on the receive
        // waterfall, is constructed off while its ship default is on.)
        SpectrumWidget loaded;
        loaded.setPanIndex(pan);
        loaded.loadSettings();
        loaded.saveSettings();

        const QStringList bases = loaded.perPanDisplayKeyBases();
        for (const QString& base : bases) {
            QVERIFY2(!s.contains(PanDisplayKeys::panKey(base, pan)), qPrintable(base));
            QVERIFY2(!s.contains(base), qPrintable(base));
        }
    }

    // perPanDisplayKeyBases() is what a reset clears, so it has to be what
    // a save writes: no key listed that is never written, none repeated,
    // the model's twelve included, and nothing global or per band in it
    // (the receive range is per band since display pill plan Task 9).
    void the_key_list_is_what_a_save_writes()
    {
        auto& s = AppSettings::instance();
        SpectrumWidget w;
        w.setPanIndex(4);
        const QStringList bases = w.perPanDisplayKeyBases();

        QCOMPARE(QSet<QString>(bases.cbegin(), bases.cend()).size(), bases.size());
        const QStringList modelBases = w.displaySettings()->perPanKeyBases();
        QCOMPARE(modelBases.size(), 12);
        for (const QString& base : modelBases) {
            QVERIFY2(bases.contains(base), qPrintable(base));
        }
        for (const QString& expected : {
                 u"DisplayWfOpacity"_s, u"DisplayBandwidth"_s, u"DisplayLineWidth"_s,
                 u"DisplayWaterfallHistoryMs"_s,
                 u"DisplaySubRxFilterColor"_s,
                 u"WaterfallNFAGCEnabled"_s, u"DisplayTxViewBandwidth"_s }) {
            QVERIFY2(bases.contains(expected), qPrintable(expected));
        }
        // The approved rebuild makes these picture choices per pan too.
        const QMap<QString, QString> markerValues = markerAndTrackingValues(w);
        for (auto it = markerValues.cbegin(); it != markerValues.cend(); ++it) {
            QVERIFY2(bases.contains(it.key()), qPrintable(it.key()));
        }
        const QMap<QString, QString> peakValues = peakDetailValues(w);
        for (auto it = peakValues.cbegin(); it != peakValues.cend(); ++it) {
            QVERIFY2(bases.contains(it.key()), qPrintable(it.key()));
        }
        QVERIFY(bases.contains(u"DisplayDispNormalize"_s));
        for (const QString& notPerPan : {
                 u"DisplayFftSize"_s, u"DisplayFftWindow"_s, u"DisplayHzPerBinTarget"_s,
                 u"DisplayGridMax"_s, u"DisplayGridMin"_s,
                 u"DisplayTxGridRefLevel"_s, u"DisplayTxGridDynamicRange"_s, u"DisplayTxWfGradient"_s,
                 u"DisplayTxWfLowLevel"_s, u"DisplayTxWfHighLevel"_s,
                 u"DisplayTxWfPalette"_s, u"DisplayTxWfLowColor"_s,
                 u"DisplaySpectrumFps"_s, u"BandPlanFontSize"_s, u"Display3DFloorDepth"_s }) {
            QVERIFY2(!bases.contains(notPerPan), qPrintable(notPerPan));
        }

        for (const SharedAppearanceColor& row : sharedAppearanceColors()) {
            QVERIFY2(!bases.contains(row.key), qPrintable(row.key));
        }

        // Every listed key is written when its value differs from the
        // master, and no unlisted key is written. Invalid fixture values
        // force a difference without depending on each field's ship default.
        for (const QString& base : bases) {
            s.setValue(PanDisplayKeys::defaultKey(base), u"different-default"_s);
            s.setValue(PanDisplayKeys::panKey(base, 4), u"not-a-value"_s);
        }
        w.saveSettings();
        for (const QString& base : bases) {
            QVERIFY2(s.value(PanDisplayKeys::panKey(base, 4)).toString() != u"not-a-value"_s,
                     qPrintable(base));
        }
        // And a save writes no "_4" key that is not listed.
        QCOMPARE(keysOfPan(4).size(), bases.size());
    }

    // ── Reset this pan (display pill plan Task 1) ────────────────────────

    // The acceptance case: every override of pan 1 goes, the LIVE widget
    // reports what it now inherits (the Setup default where there is one,
    // the ship default where there is not) without a restart, the signal
    // fires once, and nothing that is not pan 1's override is touched: the
    // Setup defaults, another pan's keys, the station's FFT keys and
    // per-band grid, and the settings every pan shares.
    void reset_drops_the_pans_overrides_and_reloads_the_live_widget()
    {
        auto& s = AppSettings::instance();
        const QColor defaultFill = QColor::fromString(u"#ff102030"_s);
        // Setup defaults.
        s.setValue(kOpacityKey, u"80"_s);
        s.setValue(kFillColorKey, defaultFill.name(QColor::HexArgb));
        s.setValue(u"DisplayGridMax"_s, u"-30"_s);
        s.setValue(u"DisplayGridMin"_s, u"-110"_s);
        // The station's display keys and the per-band grid.
        s.setValue(u"DisplayFftSize"_s, u"16384"_s);
        s.setValue(u"DisplayFftWindow"_s, u"3"_s);
        s.setValue(u"DisplayHzPerBinTarget"_s, u"12.5"_s);
        s.setValue(u"DisplaySpectrumFps"_s, u"45"_s);
        s.setValue(u"DisplayGridMax_20m"_s, u"-25"_s);
        s.setValue(u"DisplayGridMin_20m"_s, u"-130"_s);
        // A setting every pan shares, and another pan's override.
        s.setValue(u"DisplayShowBinWidth"_s, u"True"_s);
        s.setValue(u"DisplayWfOpacity_2"_s, u"15"_s);

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        pan1.setFrequencyRange(14'100'000.0, 50'000.0);
        pan1.setWfOpacity(35);
        pan1.setFillColor(QColor(9, 8, 7, 6));
        pan1.setRefLevel(-55.0F);
        pan1.setDynamicRange(40.0F);
        pan1.setGridEnabled(false);
        pan1.setWfAgcEnabled(false);
        pan1.setLineWidth(3.0F);
        pan1.setPeakHoldEnabled(true);
        pan1.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        pan1.setDssGain(12);
        pan1.setWfUpdatePeriodMs(90);
        pan1.setTxWfLowLevel(-80);
        pan1.setTxWfGradient(u"0|#FF000000|1|#FFFFFFFF"_s);
        pan1.setRxFilterColor(QColor(1, 2, 3, 4));
        pan1.setWaterfallHistoryMs(300000);
        pan1.setWfTimestampPosition(SpectrumWidget::TimestampPosition::Left);
        pan1.saveSettings();

        const QStringList bases = pan1.resettablePanDisplayKeyBases();
        // Pin every deliberate per-pan edit, rather than a count that
        // accidentally depended on shared TX/color keys living here.
        QStringList expectedOwn;
        for (const QString& base : {
                 u"DisplayBandwidth"_s, u"DisplayWfOpacity"_s, u"DisplayFillColor"_s,
                 u"DisplayGridEnabled"_s, u"DisplayWfAgc"_s, u"DisplayLineWidth"_s,
                 u"DisplayPeakHoldEnabled"_s, u"DisplaySpectrumRenderMode"_s,
                 u"Display3DGain"_s, u"DisplayWfUpdatePeriodMs"_s,
                 u"DisplayWaterfallHistoryMs"_s,
                 u"DisplayWfTimestampPos"_s }) {
            expectedOwn.append(PanDisplayKeys::panKey(base, 1));
        }
        expectedOwn.sort();
        QCOMPARE(keysOfPan(1), expectedOwn);
        const QMap<QString, QString> before = everythingExceptPanOverrides(1, bases);

        QSignalSpy resetSpy(&pan1, &SpectrumWidget::panDisplayReset);
        QSignalSpy modeSpy(&pan1, &SpectrumWidget::spectrumRenderModeChanged);
        QSignalSpy spanSpy(&pan1, &SpectrumWidget::bandwidthChangeRequested);
        QSignalSpy peakSpy(&pan1, &SpectrumWidget::peakHoldEnabledChanged);
        pan1.resetPanDisplayOverrides();

        QCOMPARE(resetSpy.count(), 1);
        for (const QString& base : bases) {
            QVERIFY2(!s.contains(PanDisplayKeys::panKey(base, 1)), qPrintable(base));
        }
        // The zoom is the pan's view, not its look: its key is all that is
        // left (see reset_leaves_the_pans_view_alone).
        QCOMPARE(keysOfPan(1), QStringList{u"DisplayBandwidth_1"_s});
        QCOMPARE(everythingExceptPanOverrides(1, bases), before);
        QCOMPARE(s.value(u"DisplayFftSize"_s).toString(), u"16384"_s);
        QCOMPARE(s.value(u"DisplayFftWindow"_s).toString(), u"3"_s);
        QCOMPARE(s.value(u"DisplayHzPerBinTarget"_s).toString(), u"12.5"_s);
        QCOMPARE(s.value(u"DisplaySpectrumFps"_s).toString(), u"45"_s);
        QCOMPARE(s.value(u"DisplayGridMax_20m"_s).toString(), u"-25"_s);
        QCOMPARE(s.value(u"DisplayGridMin_20m"_s).toString(), u"-130"_s);
        QCOMPARE(s.value(u"DisplayShowBinWidth"_s).toString(), u"True"_s);
        QCOMPARE(s.value(u"DisplayWfOpacity_2"_s).toString(), u"15"_s);

        // Inherited from the Setup default.
        QCOMPARE(pan1.wfOpacity(), 80);
        QCOMPARE(pan1.fillColor(), defaultFill);
        // The receive range is the band store's (display pill plan Task
        // 9), not this pan's look: the reset leaves the live range alone
        // and the old bare range keys above are not read.
        QCOMPARE(pan1.refLevel(), -55.0F);
        QCOMPARE(pan1.dynamicRange(), 40.0F);
        QCOMPARE(pan1.displaySettings()->refLevel(), -55.0F);
        QCOMPARE(pan1.displaySettings()->dynamicRange(), 40.0F);
        // Inherited from the ship default.
        QCOMPARE(pan1.gridEnabled(), true);
        QCOMPARE(pan1.wfAgcEnabled(), true);
        QCOMPARE(pan1.lineWidth(), 1.0F);
        QCOMPARE(pan1.peakHoldEnabled(), false);
        QCOMPARE(pan1.spectrumRenderMode(), static_cast<int>(SpectrumRenderMode::Mode2D));
        QCOMPARE(pan1.displaySettings()->spectrumRenderMode(),
                 static_cast<int>(SpectrumRenderMode::Mode2D));
        QCOMPARE(pan1.dssGain(), 70);
        QCOMPARE(pan1.displaySettings()->dssGain(), 70);
        QCOMPARE(pan1.wfUpdatePeriodMs(), 30);
        QCOMPARE(pan1.txWfLowLevel(), -80);
        QCOMPARE(pan1.txWfGradient(), u"0|#FF000000|1|#FFFFFFFF"_s);
        // Shared Appearance colors survive a pan picture reset.
        QCOMPARE(pan1.rxFilterColor(), QColor(1, 2, 3, 4));
        QCOMPARE(pan1.waterfallHistoryMs(), qint64(20 * 60 * 1000));
        QCOMPARE(pan1.wfTimestampPosition(), SpectrumWidget::TimestampPosition::None);
        QCOMPARE(pan1.bandwidth(), 50'000.0);

        // The live widget was told, the way the matching setters tell it:
        // the 3D ring is dropped by the mode change, peak hold was switched
        // off. The span did not move.
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(modeSpy.at(0).at(0).toInt(), static_cast<int>(SpectrumRenderMode::Mode2D));
        QCOMPARE(spanSpy.count(), 0);
        QCOMPARE(peakSpy.count(), 1);
        QCOMPARE(peakSpy.at(0).at(0).toBool(), false);

        // Nothing is left to write, and a save proves it.
        QVERIFY(!pan1.settingsSavePendingForTest());
        pan1.saveSettings();
        QCOMPARE(keysOfPan(1), QStringList{u"DisplayBandwidth_1"_s});

        // A second reset of a pan with nothing to drop is harmless.
        pan1.resetPanDisplayOverrides();
        QCOMPARE(resetSpy.count(), 2);
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(spanSpy.count(), 0);
        QCOMPARE(pan1.wfOpacity(), 80);
    }

    // The pan's view is not its look. A pan that is zoomed and has CTUN
    // changed keeps both through a reset, the keys and the live values,
    // while an appearance key beside them goes. Covers a view that was
    // already saved and one still waiting for the save the reset cancels.
    void reset_leaves_the_pans_view_alone_data()
    {
        QTest::addColumn<bool>("savedFirst");
        QTest::newRow("view already saved") << true;
        QTest::newRow("view still waiting for its save") << false;
    }
    void reset_leaves_the_pans_view_alone()
    {
        QFETCH(bool, savedFirst);
        auto& s = AppSettings::instance();
        s.setValue(kOpacityKey, u"80"_s);
        s.setValue(u"DisplayTxViewBandwidth_1"_s, u"12000"_s);

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        pan1.setFrequencyRange(14'100'000.0, 48'000.0);   // zoomed in
        pan1.setCtunEnabled(false);                       // ship default is on
        pan1.setWfOpacity(35);                            // an appearance value
        pan1.setGridEnabled(false);
        if (savedFirst) {
            pan1.saveSettings();
            QCOMPARE(s.value(u"DisplayBandwidth_1"_s).toString(), u"48000"_s);
            QCOMPARE(s.value(u"DisplayCtunEnabled_1"_s).toString(), u"False"_s);
            QCOMPARE(s.value(u"DisplayWfOpacity_1"_s).toString(), u"35"_s);
        }

        const QStringList resettable = pan1.resettablePanDisplayKeyBases();
        const QStringList all = pan1.perPanDisplayKeyBases();
        QCOMPARE(resettable.size(), all.size() - 3);
        for (const QString& view : { u"DisplayBandwidth"_s, u"DisplayTxViewBandwidth"_s,
                                     u"DisplayCtunEnabled"_s }) {
            QVERIFY2(all.contains(view), qPrintable(view));
            QVERIFY2(!resettable.contains(view), qPrintable(view));
        }
        QVERIFY(resettable.contains(u"DisplayCalOffset"_s));
        QVERIFY(resettable.contains(kOpacityKey));

        QSignalSpy spanSpy(&pan1, &SpectrumWidget::bandwidthChangeRequested);
        pan1.resetPanDisplayOverrides();

        // The appearance went back to what the pan inherits...
        QVERIFY(!s.contains(u"DisplayWfOpacity_1"_s));
        QVERIFY(!s.contains(u"DisplayGridEnabled_1"_s));
        QCOMPARE(pan1.wfOpacity(), 80);
        QCOMPARE(pan1.gridEnabled(), true);
        // ...and the view did not move, live or stored.
        QCOMPARE(pan1.bandwidth(), 48'000.0);
        QCOMPARE(pan1.ctunEnabled(), false);
        QCOMPARE(spanSpy.count(), 0);
        QCOMPARE(s.value(u"DisplayBandwidth_1"_s).toString(), u"48000"_s);
        QCOMPARE(s.value(u"DisplayCtunEnabled_1"_s).toString(), u"False"_s);
        QCOMPARE(s.value(u"DisplayTxViewBandwidth_1"_s).toString(), u"12000"_s);
        QCOMPARE(keysOfPan(1), (QStringList{u"DisplayBandwidth_1"_s, u"DisplayCtunEnabled_1"_s,
                                            u"DisplayTxViewBandwidth_1"_s}));

        // A restart comes back the same way.
        SpectrumWidget again;
        again.setPanIndex(1);
        again.loadSettings();
        QCOMPARE(again.bandwidth(), 48'000.0);
        QCOMPARE(again.wfOpacity(), 80);
    }

    // A save scheduled before the reset does not rewrite the removed keys.
    void a_save_scheduled_before_the_reset_is_dropped()
    {
        auto& s = AppSettings::instance();
        s.setValue(kOpacityKey, u"80"_s);
        s.setValue(u"DisplayWfOpacity_1"_s, u"35"_s);

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        pan1.setWfOpacity(60);
        pan1.setGridEnabled(false);
        QVERIFY(pan1.settingsSavePendingForTest());

        pan1.resetPanDisplayOverrides();

        QVERIFY(!pan1.settingsSavePendingForTest());
        QCOMPARE(keysOfPan(1), QStringList());
        // The timer of the dropped save is still due. Let it fire: the
        // next save a later change schedules is the one that runs it out,
        // and neither may bring a removed key back.
        QCoreApplication::processEvents();
        QCOMPARE(keysOfPan(1), QStringList());
        QCOMPARE(pan1.wfOpacity(), 80);
        QCOMPARE(pan1.gridEnabled(), true);

        // The pan can be changed again afterwards, and saves again.
        pan1.setWfOpacity(20);
        QVERIFY(pan1.settingsSavePendingForTest());
        pan1.saveSettings();
        QCOMPARE(keysOfPan(1), QStringList{u"DisplayWfOpacity_1"_s});
    }

    // Reset while transmitting. The live grid is the transmit grid then, so
    // it takes the inherited transmit values, and the receive range (the
    // band store's, display pill plan Task 9; no pan key holds it) is
    // still what the un-key comes back to.
    void reset_while_keyed_puts_each_grid_in_its_own_place()
    {
        auto& s = AppSettings::instance();
        s.setValue(u"DisplayTxGridRefLevel"_s, u"55"_s);
        s.setValue(u"DisplayTxGridDynamicRange"_s, u"70"_s);

        SpectrumWidget pan1;
        pan1.setPanIndex(1);
        pan1.loadSettings();
        pan1.setDbmRange(-110.0F, -30.0F);   // the receive range
        pan1.setDisplayDuplex(true);   // keep the receive span while keyed
        pan1.setMoxOverlay(true);
        QCOMPARE(pan1.refLevel(), 55.0F);

        pan1.resetPanDisplayOverrides();

        // A pan reset retains the one-computer TX pair (55 / 70).
        QCOMPARE(pan1.refLevel(), 55.0F);
        QCOMPARE(pan1.dynamicRange(), 70.0F);
        QCOMPARE(pan1.transmitRefLevel(), 55.0F);
        pan1.saveSettings();
        QCOMPARE(keysOfPan(1), QStringList());

        pan1.setMoxOverlay(false);
        QCOMPARE(pan1.refLevel(), -30.0F);
        QCOMPARE(pan1.dynamicRange(), 80.0F);
        pan1.saveSettings();
        QCOMPARE(keysOfPan(1), QStringList());
    }
};

QTEST_MAIN(TestPanDisplaySettingsInherit)
#include "tst_pan_display_settings_inherit.moc"
