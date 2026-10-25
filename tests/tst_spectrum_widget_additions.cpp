// =================================================================
// tests/tst_spectrum_widget_additions.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The behaviour under test is ported
// from AetherSDR (see the attribution block in src/gui/SpectrumWidget.cpp
// and docs/attribution/AETHERSDR-PROVENANCE.md); nothing in this file is.
//
// Display pill plan Task 4 (2026-10-10). Three additions to one pan's
// display, each saved per pan and announced with displaySettingChanged:
//   - NB Blank: a waterfall row whose power jumps over the rolling baseline
//     is replaced by the last good row;
//   - a fixed spacing for the vertical grid and the frequency scale;
//   - the frequency scale's text size.
//
// What is pinned here:
//   - Auto spacing draws the steps it drew before the fixed spacing existed,
//     on the grid and on the scale (they are two ladders and differ);
//   - the scale strip is 28 px tall and its text is the 10 px it was;
//   - NB Blank off changes no row; on, it judges a row by the median of
//     its pixels in dB (since 2026-10-22), replaces a row risen more than
//     the threshold and keeps one risen less, and stands still while the
//     pan transmits;
//   - NB Blank on draws a loud station on 5 % of the pixels as it is, row
//     after row, and replaces a fence charger's pulse (every pixel up),
//     counting it in waterfallRowsBlanked();
//   - a fixed spacing reaches both the grid and the scale, and crowded
//     scale labels are thinned;
//   - 14 px scale text is drawn inside the strip;
//   - all four values clamp, announce once, save and reload per pan,
//     inherit the Setup default and are cleared by "Reset this pan".
//
// Display pill plan Task 5 (2026-10-10). Two more, also per pan:
//   - the trace line's own colour (DisplayTraceColor);
//   - a background behind the 2D spectrum: on/off, a fill colour, a picture
//     and how strongly the colour covers it.
//
// What is pinned for those:
//   - with no trace colour chosen the trace is the fill colour and follows
//     it, on the QPainter path and in the GPU line vertices; once chosen,
//     the trace alone changes and the fill keeps the fill colour;
//   - the GPU fill's lower edge is the fill colour darkened (it was a fixed
//     (0, 0, 0.3)), and what that is for the default fill colour;
//   - with the background off nothing is drawn for it; on, the spectrum
//     area is the colour, with the picture over it scaled to the area;
//     Clear drops the picture, Off keeps the choice and stops drawing;
//   - a picture that cannot be read is reported once and not drawn;
//   - all five values announce once, save and reload per pan, inherit the
//     Setup default and are cleared by "Reset this pan".
// The QPainter path is drawn into an image through the widget's test seams
// (in a GPU build an offscreen grab() of a QRhiWidget shows neither path),
// and the GPU colours are read from the vertices renderGpuFrame() uploads.
// The background's GPU draw itself needs a running window to be seen.
//
// Isolation: TestSandboxInit redirects the settings path away from the
// operator's profile, and init()/cleanup() clear the in-process store
// around every slot (as tst_pan_display_settings_inherit does).
//
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Claude Opus 5.5): NB Blank's rows are risen across the
//                 whole row (a median-judged blanker ignores one bright
//                 pixel); a loud station is drawn as is and a fence pulse
//                 is replaced and counted.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <random>

#include "core/AppSettings.h"
#include "core/settings/PanDisplayKeys.h"
#include "gui/SpectrumTraceRibbon.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;
using namespace Qt::StringLiterals;

namespace {
constexpr double kCenterHz   = 14'200'000.0;
constexpr float  kQuietDbm   = -110.0f;

// MinGW imports a static constexpr member of an exported class by address
// when it is bound to a reference (the range-for and size() do), and the GUI
// DLL never exports one, so this test did not link on Windows. It reads this
// copy of SpectrumWidget::kFreqGridSpacingsHz by value instead.
constexpr std::array<int, 7> kFreqGridSpacingsHz = SpectrumWidget::kFreqGridSpacingsHz;

// How many times `spy` (on displaySettingChanged) saw `keyBase`.
int countOf(const QSignalSpy& spy, const QString& keyBase)
{
    int n = 0;
    for (const QList<QVariant>& args : spy) {
        if (args.value(0).toString() == keyBase) { ++n; }
    }
    return n;
}

// A row at kQuietDbm whose LINEAR-power mean is `factor` times a flat
// kQuietDbm row's, with all of the extra power in the one middle pixel.
// Since 2026-10-22 NB Blank judges a row by its median, which one pixel does
// not move, so this row is never a burst; it shows NB Blank off drawing it.
QVector<float> burstRow(int n, double factor)
{
    QVector<float> row(n, kQuietDbm);
    const double quietMw = std::pow(10.0, kQuietDbm / 10.0);
    const double peakMw  = quietMw * (1.0 + (factor - 1.0) * n);
    row[n / 2] = static_cast<float>(10.0 * std::log10(peakMw));
    return row;
}

// A flat row `db` dB above kQuietDbm in every pixel: what a wideband
// impulse (a fence charger's click) does to a row.
QVector<float> burstRowDb(int n, double db)
{
    return QVector<float>(n, static_cast<float>(kQuietDbm + db));
}

// A noisy floor row: `floorDbm` with uniform +/-2 dB noise, and pixels
// [signalFirst, signalFirst + signalPixels) `signalDb` over it.
QVector<float> noisyRow(std::mt19937& rng, int n, float floorDbm,
                        int signalFirst = 0, int signalPixels = 0, float signalDb = 0.0f)
{
    std::uniform_real_distribution<float> noise(-2.0f, 2.0f);
    QVector<float> row(n);
    for (int i = 0; i < n; ++i) {
        row[i] = floorDbm + noise(rng);
        if (i >= signalFirst && i < signalFirst + signalPixels) {
            row[i] += signalDb;
        }
    }
    return row;
}

// The newest waterfall row, every pixel.
QImage newestLine(const SpectrumWidget& w)
{
    const QImage& img = w.liveWaterfallForTest();
    return img.copy(0, w.liveWaterfallWriteRowForTest(), img.width(), 1);
}

// A shown widget with a waterfall image and fixed waterfall levels, so a
// pixel's colour depends only on that pixel's dBm.
void prepareWaterfall(SpectrumWidget& w)
{
    w.resize(800, 400);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setWfAgcEnabled(false);
    QVERIFY(w.waterfallHeightForTest() > 0);
}

// The newest waterfall row's colour at the middle pixel and at the first.
struct NewestRow {
    QRgb middle;
    QRgb first;
};
NewestRow newestRow(const SpectrumWidget& w)
{
    const QImage& img = w.liveWaterfallForTest();
    const int y = w.liveWaterfallWriteRowForTest();
    return {img.pixel(img.width() / 2, y), img.pixel(0, y)};
}

// The frequency scale drawn into an image taller than the strip, the strip
// starting kPad rows down. Anything drawn outside the strip shows up in the
// padding, which starts transparent.
constexpr int kPad = 20;
QImage renderScale(SpectrumWidget& w)
{
    const QRect strip = w.freqScaleRectForTest();
    QImage img(strip.width(), strip.height() + 2 * kPad,
               QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    w.drawFreqScaleForTest(p, QRect(0, kPad, strip.width(), strip.height()));
    p.end();
    return img;
}

// First and last image row holding a pixel that is not `background`, or
// {-1, -1}.
std::pair<int, int> inkRows(const QImage& img, QRgb background, int fromY, int toY)
{
    int first = -1;
    int last = -1;
    for (int y = fromY; y < toY; ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (img.pixel(x, y) != background) {
                if (first < 0) { first = y; }
                last = y;
                break;
            }
        }
    }
    return {first, last};
}

// ── Task 5 helpers ──────────────────────────────────────────────────────
const QColor kDefaultFill{0x00, 0xe5, 0xff};
const QColor kDefaultBackground{0x0a, 0x0a, 0x14};
constexpr int kAreaW = 200;
constexpr int kAreaH = 100;

// A flat trace across the middle of the default dBm range.
QVector<float> flatTrace(const SpectrumWidget& w, int n = 64)
{
    return QVector<float>(n, w.refLevel() - w.dynamicRange() / 2.0f);
}

// The 2D trace as the QPainter path draws it, on a transparent image.
QImage renderTrace(SpectrumWidget& w)
{
    QImage img(kAreaW, kAreaH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    w.drawSpectrumForTest(p, QRect(0, 0, kAreaW, kAreaH));
    p.end();
    return img;
}

// The colours of the fully opaque pixels in the image's middle column (the
// trace line: the fill under it is translucent).
QSet<QRgb> opaqueColours(const QImage& img)
{
    QSet<QRgb> seen;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb px = img.pixel(img.width() / 2, y);
        if (qAlpha(px) == 255) { seen.insert(px); }
    }
    return seen;
}

// The background as both paths paint it, on an image filled with `under`.
QImage renderBackground(SpectrumWidget& w, const QColor& under = Qt::transparent)
{
    QImage img(kAreaW, kAreaH, QImage::Format_ARGB32_Premultiplied);
    img.fill(under);
    if (w.drawsSpectrumBackgroundForTest()) {
        QPainter p(&img);
        w.paintSpectrumBackgroundForTest(p, QRect(0, 0, kAreaW, kAreaH));
        p.end();
    }
    return img;
}

// ── Line width helpers ──────────────────────────────────────────────────
// A trace that climbs, falls and runs level, so its ink does not depend on
// one slope.
QVector<float> zigzagTrace(const SpectrumWidget& w, int n = 64)
{
    QVector<float> row(n);
    const float mid = w.refLevel() - w.dynamicRange() / 2.0f;
    const float swing = w.dynamicRange() / 4.0f;
    for (int j = 0; j < n; ++j) {
        const int phase = j % 8;
        const float t = (phase < 4) ? static_cast<float>(phase) / 4.0f
                                    : static_cast<float>(8 - phase) / 4.0f;
        row[j] = (j < n / 2) ? mid + swing * (t - 0.5f) : mid;
    }
    return row;
}

// The trace alone (no fill) at this line width, drawn as paintEvent draws
// it: with antialiasing on, which is what makes a pen thinner than a pixel
// lighter.
QImage renderTraceAtWidth(float widthPx)
{
    SpectrumWidget w;
    w.setPanFillEnabled(false);
    w.setGradientEnabled(false);
    w.setLineWidth(widthPx);
    w.setRenderedPixelsForTest(zigzagTrace(w));
    QImage img(kAreaW, kAreaH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    w.drawSpectrumForTest(p, QRect(0, 0, kAreaW, kAreaH));
    p.end();
    return img;
}

// How much ink an image holds: every channel of every pixel, summed. The
// image starts transparent, so this is the trace's coverage.
qint64 inkOf(const QImage& img)
{
    qint64 total = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = img.pixel(x, y);
            total += qRed(px) + qGreen(px) + qBlue(px) + qAlpha(px);
        }
    }
    return total;
}

// "Measurably less": under 60% of the reference's ink.
bool hasMeasurablyLessInk(const QImage& img, const QImage& reference)
{
    return inkOf(img) * 10 < inkOf(reference) * 6;
}

bool near(const QColor& got, const QColor& want, int tolerance = 2)
{
    return std::abs(got.red() - want.red()) <= tolerance
        && std::abs(got.green() - want.green()) <= tolerance
        && std::abs(got.blue() - want.blue()) <= tolerance
        && std::abs(got.alpha() - want.alpha()) <= tolerance;
}

// A 2:1 picture, left half green and right half blue, as a PNG in `dir`.
QString writePicture(const QTemporaryDir& dir)
{
    QImage img(40, 20, QImage::Format_RGB32);
    img.fill(QColor(0, 0, 255));
    QPainter p(&img);
    p.fillRect(0, 0, 20, 20, QColor(0, 255, 0));
    p.end();
    const QString path = dir.filePath(u"background.png"_s);
    return img.save(path) ? path : QString();
}
} // namespace

class TestSpectrumWidgetAdditions : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── Auto spacing: the steps drawn before this task ──────────────────
    //
    // The grid (drawGrid) and the scale's labels (drawFreqScale) each had
    // their own ladder. These are the values those ladders gave.
    void autoSpacing_isTodaysSteps_data()
    {
        QTest::addColumn<double>("spanHz");
        QTest::addColumn<double>("gridStepHz");
        QTest::addColumn<double>("scaleStepHz");

        QTest::newRow("3 kHz")   << 3'000.0   << 5'000.0  << 5'000.0;
        QTest::newRow("48 kHz")  << 48'000.0  << 5'000.0  << 5'000.0;
        QTest::newRow("100 kHz") << 100'000.0 << 10'000.0 << 25'000.0;
        QTest::newRow("384 kHz") << 384'000.0 << 25'000.0 << 25'000.0;
    }

    void autoSpacing_isTodaysSteps()
    {
        QFETCH(double, spanHz);
        QFETCH(double, gridStepHz);
        QFETCH(double, scaleStepHz);

        SpectrumWidget w;
        w.resize(800, 400);
        w.setFrequencyRange(kCenterHz, spanHz);

        QCOMPARE(w.freqGridSpacingHz(), 0);
        QCOMPARE(w.gridFreqStepHzForTest(), gridStepHz);
        QCOMPARE(w.scaleFreqStepHzForTest(), scaleStepHz);
    }

    // ── The horizontal dB lines are Setup's dB Step apart ───────────────
    //
    // drawGrid's lines start one step above the bottom of the range and
    // stop below the top. Before this the step was 10 dB, or 5 dB for a
    // range of 50 dB or less, whatever Setup's box said.
    void dbGridLines_areDbStepApart_data()
    {
        QTest::addColumn<float>("dbMax");
        QTest::addColumn<float>("rangeDb");
        QTest::addColumn<int>("stepDb");
        QTest::addColumn<QVector<float>>("lines");

        QTest::newRow("range 100, step 10") << -40.0f << 100.0f << 10
            << QVector<float>{-130, -120, -110, -100, -90, -80, -70, -60, -50};
        QTest::newRow("range 100, step 5") << -40.0f << 100.0f << 5
            << QVector<float>{-135, -130, -125, -120, -115, -110, -105, -100, -95, -90,
                              -85, -80, -75, -70, -65, -60, -55, -50, -45};
        QTest::newRow("range 100, step 20") << -40.0f << 100.0f << 20
            << QVector<float>{-120, -100, -80, -60};
        QTest::newRow("range 40, step 10") << -40.0f << 40.0f << 10
            << QVector<float>{-70, -60, -50};
        QTest::newRow("range 40, step 5") << -40.0f << 40.0f << 5
            << QVector<float>{-75, -70, -65, -60, -55, -50, -45};
        QTest::newRow("range 30, step 40") << -40.0f << 30.0f << 40
            << QVector<float>{};
    }

    void dbGridLines_areDbStepApart()
    {
        QFETCH(float, dbMax);
        QFETCH(float, rangeDb);
        QFETCH(int, stepDb);
        QFETCH(QVector<float>, lines);

        SpectrumWidget w;
        w.resize(800, 400);
        w.setDbmRange(dbMax - rangeDb, dbMax);
        QCOMPARE(w.refLevel(), dbMax);
        QCOMPARE(w.dynamicRange(), rangeDb);
        // The step a widget starts with is the model's default.
        QCOMPARE(w.dbGridStep(), 10);

        w.setDbGridStep(stepDb);
        QCOMPARE(w.dbGridStep(), stepDb);
        QCOMPARE(w.dbGridLinesForTest(), lines);
    }

    // A step of 0 or less is refused: the old step stays, and so do its
    // lines.
    void dbGridStep_belowOneIsRefused()
    {
        SpectrumWidget w;
        w.resize(800, 400);
        w.setDbmRange(-140.0f, -40.0f);
        w.setDbGridStep(20);
        const QVector<float> lines = w.dbGridLinesForTest();
        QCOMPARE(lines.size(), 4);

        w.setDbGridStep(0);
        QCOMPARE(w.dbGridStep(), 20);
        w.setDbGridStep(-5);
        QCOMPARE(w.dbGridStep(), 20);
        QCOMPARE(w.dbGridLinesForTest(), lines);
    }

    // ── The scale strip is 28 px tall ───────────────────────────────────
    void scaleStrip_is28pxTall()
    {
        SpectrumWidget w;
        w.resize(800, 400);
        QCOMPARE(w.freqScaleRectForTest().height(), 28);
        QCOMPARE(w.freqScaleHeight(), 28);
    }

    // ── Defaults ────────────────────────────────────────────────────────
    void defaults()
    {
        SpectrumWidget w;
        w.loadSettings();
        QCOMPARE(w.waterfallBlankingEnabled(), false);
        QCOMPARE(w.waterfallBlankingThresholdDb(), 6);
        QCOMPARE(w.freqGridSpacingHz(), 0);
        QCOMPARE(w.freqScaleFontPx(), 10);
    }

    // ── Ranges ──────────────────────────────────────────────────────────
    void values_stay_in_range()
    {
        SpectrumWidget w;

        // NB Blank's threshold: 1 to 20 dB.
        w.setWaterfallBlankingThresholdDb(0);
        QCOMPARE(w.waterfallBlankingThresholdDb(), 1);
        w.setWaterfallBlankingThresholdDb(500);
        QCOMPARE(w.waterfallBlankingThresholdDb(), 20);

        w.setFreqScaleFontPx(2);
        QCOMPARE(w.freqScaleFontPx(), 8);
        w.setFreqScaleFontPx(40);
        QCOMPARE(w.freqScaleFontPx(), 14);

        for (const int hz : kFreqGridSpacingsHz) {
            w.setFreqGridSpacingHz(hz);
            QCOMPARE(w.freqGridSpacingHz(), hz);
        }
        QCOMPARE(kFreqGridSpacingsHz.size(), size_t(7));
        // Not one of the choices: refused, the spacing stays.
        w.setFreqGridSpacingHz(3000);
        QCOMPARE(w.freqGridSpacingHz(), 100000);
        w.setFreqGridSpacingHz(-5);
        QCOMPARE(w.freqGridSpacingHz(), 100000);
        w.setFreqGridSpacingHz(0);
        QCOMPARE(w.freqGridSpacingHz(), 0);
    }

    // ── Each setter announces its key base once per change ──────────────
    void setters_announce_once()
    {
        SpectrumWidget w;
        QSignalSpy spy(&w, &SpectrumWidget::displaySettingChanged);

        w.setWaterfallBlankingEnabled(true);
        w.setWaterfallBlankingEnabled(true);
        QCOMPARE(countOf(spy, u"WaterfallBlankingEnabled"_s), 1);

        w.setWaterfallBlankingThresholdDb(12);
        w.setWaterfallBlankingThresholdDb(12);
        QCOMPARE(countOf(spy, u"WaterfallBlankingThresholdDb"_s), 1);
        // The factor's old key is never announced.
        QCOMPARE(countOf(spy, u"WaterfallBlankingThreshold"_s), 0);

        w.setFreqGridSpacingHz(5000);
        w.setFreqGridSpacingHz(5000);
        w.setFreqGridSpacingHz(3000);   // refused: no announcement
        QCOMPARE(countOf(spy, u"DisplayFreqGridSpacing"_s), 1);

        w.setFreqScaleFontPx(12);
        w.setFreqScaleFontPx(12);
        QCOMPARE(countOf(spy, u"DisplayFreqScaleFontPx"_s), 1);

        QCOMPARE(spy.count(), 4);
    }

    // ── A fixed spacing reaches the grid and the scale ──────────────────
    void fixedSpacing_reachesGridAndScale_data()
    {
        QTest::addColumn<double>("spanHz");
        QTest::newRow("3 kHz")   << 3'000.0;
        QTest::newRow("48 kHz")  << 48'000.0;
        QTest::newRow("100 kHz") << 100'000.0;
        QTest::newRow("384 kHz") << 384'000.0;
    }

    void fixedSpacing_reachesGridAndScale()
    {
        QFETCH(double, spanHz);

        SpectrumWidget w;
        w.resize(800, 400);
        w.setFrequencyRange(kCenterHz, spanHz);
        const double autoGrid  = w.gridFreqStepHzForTest();
        const double autoScale = w.scaleFreqStepHzForTest();

        w.setFreqGridSpacingHz(5000);
        QCOMPARE(w.gridFreqStepHzForTest(), 5'000.0);
        QCOMPARE(w.scaleFreqStepHzForTest(), 5'000.0);

        w.setFreqGridSpacingHz(0);
        QCOMPARE(w.gridFreqStepHzForTest(), autoGrid);
        QCOMPARE(w.scaleFreqStepHzForTest(), autoScale);
    }

    // ── Crowded labels are thinned, Auto's never are ────────────────────
    void fixedSpacing_thinsCrowdedLabels()
    {
        SpectrumWidget w;
        w.resize(800, 400);
        w.setFrequencyRange(kCenterHz, 384'000.0);

        // Auto at 384 kHz: a label every 25 kHz, 15 or 16 of them.
        renderScale(w);
        const int autoLabels = w.freqScaleLabelsDrawnForTest();
        QVERIFY2(autoLabels == 15 || autoLabels == 16, qPrintable(QString::number(autoLabels)));

        // 1 kHz lines are about 2 px apart here: 384 labels would be one
        // smear. At least 60 px each leaves room for 13 or so.
        w.setFreqGridSpacingHz(1000);
        renderScale(w);
        const int thinned = w.freqScaleLabelsDrawnForTest();
        const int stripW = w.freqScaleRectForTest().width();
        QVERIFY2(thinned >= 4, qPrintable(QString::number(thinned)));
        QVERIFY2(thinned <= stripW / 60 + 1, qPrintable(QString::number(thinned)));

        // A spacing with room to spare labels every line: 100 kHz, 3 or 4.
        w.setFreqGridSpacingHz(100000);
        renderScale(w);
        const int wide = w.freqScaleLabelsDrawnForTest();
        QVERIFY2(wide == 3 || wide == 4, qPrintable(QString::number(wide)));
    }

    // 2026-10-16: below 520 px the narrow layout drops a label that would
    // run into the one before it or past the live button. The counter
    // counts the labels drawn, so it matches the runs of ink in the strip
    // (a label's characters sit closer together than the 8 px kept
    // between labels).
    void narrowScale_countsOnlyDrawnLabels()
    {
        SpectrumWidget w;
        w.resize(400, 400);
        w.setFrequencyRange(kCenterHz, 384'000.0);
        const QImage img = renderScale(w);
        const int drawn = w.freqScaleLabelsDrawnForTest();
        const QRgb background = img.pixel(0, kPad + 1);
        int runs = 0;
        int gap = 1000;
        for (int x = 0; x < img.width(); ++x) {
            bool ink = false;
            for (int y = kPad; y < img.height() - kPad && !ink; ++y) {
                ink = img.pixel(x, y) != background;
            }
            if (ink) {
                if (gap >= 6) { ++runs; }
                gap = 0;
            } else {
                ++gap;
            }
        }
        QVERIFY2(drawn >= 2, qPrintable(QString::number(drawn)));
        // Auto at 384 kHz has 15 or 16 lines; 400 px has room for fewer.
        QVERIFY2(drawn < 15, qPrintable(QString::number(drawn)));
        QCOMPARE(drawn, runs);
    }

    // ── Scale text ──────────────────────────────────────────────────────
    //
    // 10 px is what an untouched pan draws. 14 px is taller, and all of it
    // lands inside the strip.
    void scaleText_10pxIsTheDefault_14pxFits()
    {
        SpectrumWidget w;
        w.resize(800, 400);
        w.setFrequencyRange(kCenterHz, 100'000.0);

        const QImage untouched = renderScale(w);
        const int h10 = w.freqScaleRectForTest().height();
        QCOMPARE(h10, 28);
        const QRgb background = untouched.pixel(0, kPad + 1);
        const auto ink10 = inkRows(untouched, background, kPad, kPad + h10);
        QVERIFY2(ink10.first >= 0, "no scale labels were drawn");

        // Setting the default explicitly changes nothing.
        w.setFreqScaleFontPx(10);
        QCOMPARE(renderScale(w), untouched);

        w.setFreqScaleFontPx(14);
        const int h14 = w.freqScaleRectForTest().height();
        QCOMPARE(h14, w.freqScaleHeight());
        QVERIFY(h14 >= 28);
        {
            QFont f = w.font();
            f.setPixelSize(14);
            QVERIFY2(h14 >= QFontMetrics(f).height(),
                     "the strip is shorter than a line of its own text");
        }
        const QImage big = renderScale(w);
        const auto ink14 = inkRows(big, background, kPad, kPad + h14);
        QVERIFY(ink14.first >= 0);
        QVERIFY2((ink14.second - ink14.first) > (ink10.second - ink10.first),
                 "14 px text is no taller than 10 px text");
        // Not clipped: no ink in the first or last row of the strip, and
        // none outside it (the padding is still transparent).
        QVERIFY2(ink14.first > kPad, "text touches the top of the strip");
        QVERIFY2(ink14.second < kPad + h14 - 1, "text touches the bottom of the strip");
        const auto above = inkRows(big, qRgba(0, 0, 0, 0), 0, kPad);
        const auto below = inkRows(big, qRgba(0, 0, 0, 0), kPad + h14, big.height());
        QCOMPARE(above.first, -1);
        QCOMPARE(below.first, -1);

        // Back to 10 px: what it drew at the start.
        w.setFreqScaleFontPx(10);
        QCOMPARE(renderScale(w), untouched);
    }

    // ── NB Blank off: every row is drawn as pushed ──────────────────────
    void blankOff_rowsPassThrough()
    {
        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        QCOMPARE(w.waterfallBlankingEnabled(), false);

        for (int i = 0; i < 10; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);
        QCOMPARE(quiet.middle, quiet.first);

        // Ten times the power in one row: drawn, burst and all.
        w.pushWaterfallRowForTest(burstRow(n, 10.0));
        const NewestRow burst = newestRow(w);
        QVERIFY(burst.middle != quiet.middle);
        QCOMPARE(burst.first, quiet.first);
    }

    // ── NB Blank on at 6 dB: 7 dB over is replaced, 5 dB over is kept ───
    //
    // Both rows are risen in every pixel, as a wideband impulse is.
    void blankOn_replacesABurst_keepsARise()
    {
        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);
        QCOMPARE(w.waterfallBlankingThresholdDb(), 6);

        for (int i = 0; i < 10; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);

        // 5 dB above the baseline: kept, its bright pixel is on the
        // waterfall.
        w.pushWaterfallRowForTest(burstRowDb(n, 5.0));
        QVERIFY2(newestRow(w).middle != quiet.middle,
                 "a row 5 dB above the baseline was blanked at 6 dB");

        // Settle the baseline again, then 7 dB above it: replaced by the
        // last good row, which was quiet all the way across.
        for (int i = 0; i < 40; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const int rowBefore = w.liveWaterfallWriteRowForTest();
        w.pushWaterfallRowForTest(burstRowDb(n, 7.0));
        QVERIFY2(w.liveWaterfallWriteRowForTest() != rowBefore,
                 "the replaced row still has to scroll the waterfall");
        const NewestRow replaced = newestRow(w);
        QCOMPARE(replaced.middle, quiet.middle);
        QCOMPARE(replaced.first, quiet.first);
        QCOMPARE(w.waterfallRowsBlanked(), quint64(1));

        // A wider threshold lets the same row through.
        w.setWaterfallBlankingThresholdDb(10);
        w.pushWaterfallRowForTest(burstRowDb(n, 7.0));
        QVERIFY2(newestRow(w).middle != quiet.middle,
                 "a row 7 dB above the baseline was blanked at 10 dB");
    }

    // ── The threshold is the edge, in dB, at every setting ──────────────
    void blankOn_thresholdIsInDb_data()
    {
        QTest::addColumn<int>("thresholdDb");
        QTest::newRow("1 dB")  << 1;
        QTest::newRow("6 dB")  << 6;
        QTest::newRow("12 dB") << 12;
        QTest::newRow("20 dB") << 20;
    }

    void blankOn_thresholdIsInDb()
    {
        QFETCH(int, thresholdDb);

        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);
        w.setWaterfallBlankingThresholdDb(thresholdDb);

        for (int i = 0; i < 40; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);

        // Half a dB under the threshold: kept (half a dB may not change a
        // pixel's colour, so the blanked-row count says it).
        w.pushWaterfallRowForTest(burstRowDb(n, thresholdDb - 0.5));
        QVERIFY2(w.waterfallRowsBlanked() == 0, "a row under the threshold was blanked");

        for (int i = 0; i < 40; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        // Half a dB over it: replaced.
        w.pushWaterfallRowForTest(burstRowDb(n, thresholdDb + 0.5));
        QCOMPARE(newestRow(w).middle, quiet.middle);
        QCOMPARE(w.waterfallRowsBlanked(), quint64(1));
    }

    // ── A loud station is drawn as is; a fence pulse is replaced ────────
    //
    // NB Blank at 3 dB (JJ's setting) on a noisy -120 dBm floor. A station
    // 40 dB up on 5 % of the pixels for 200 rows: every row is drawn as
    // the same widget with NB Blank off draws it (the whole-row mean of
    // linear power held the waterfall on rows like these). Then a fence
    // charger's pulse, every pixel 10 dB up: replaced by the last accepted
    // row, and counted.
    void blankOn_loudStationDrawnAsIs_fencePulseReplaced()
    {
        SpectrumWidget on;
        prepareWaterfall(on);
        on.setWaterfallBlankingEnabled(true);
        on.setWaterfallBlankingThresholdDb(3);
        SpectrumWidget off;
        prepareWaterfall(off);
        const int n = on.liveWaterfallForTest().width();
        QCOMPARE(off.liveWaterfallForTest().width(), n);
        constexpr float kFloorDbm = -120.0f;

        std::mt19937 rng(20261022u);
        const auto push = [&](const QVector<float>& row) {
            on.pushWaterfallRowForTest(row);
            off.pushWaterfallRowForTest(row);
        };
        for (int i = 0; i < 32; ++i) {
            push(noisyRow(rng, n, kFloorDbm));
            QCOMPARE(newestLine(on), newestLine(off));
        }
        const int stationFirst = n / 2 - n / 40;
        const int stationPixels = n / 20;
        for (int i = 0; i < 200; ++i) {
            push(noisyRow(rng, n, kFloorDbm, stationFirst, stationPixels, 40.0f));
            QVERIFY2(newestLine(on) == newestLine(off),
                     qPrintable(QStringLiteral("loud station row %1 was not drawn as pushed").arg(i)));
        }
        QCOMPARE(on.waterfallRowsBlanked(), quint64(0));

        for (int i = 0; i < 32; ++i) {
            push(noisyRow(rng, n, kFloorDbm));
            QCOMPARE(newestLine(on), newestLine(off));
        }
        const QImage lastAccepted = newestLine(on);
        QVector<float> pulse = noisyRow(rng, n, kFloorDbm);
        for (float& dbm : pulse) {
            dbm += 10.0f;
        }
        push(pulse);
        QCOMPARE(newestLine(on), lastAccepted);
        QVERIFY2(newestLine(off) != lastAccepted, "the pulse row draws like a floor row");
        QCOMPARE(on.waterfallRowsBlanked(), quint64(1));

        push(noisyRow(rng, n, kFloorDbm));
        QCOMPARE(newestLine(on), newestLine(off));
        QCOMPARE(on.waterfallRowsBlanked(), quint64(1));
        QCOMPARE(off.waterfallRowsBlanked(), quint64(0));
    }

    // ── The factor's old key is not read ────────────────────────────────
    //
    // Before the threshold was in dB it was stored under
    // WaterfallBlankingThreshold as a factor (15 for 1.15, or 1.15 itself).
    // Neither is taken as dB, as a default or as a pan's own value.
    void blankThreshold_oldFactorKeyIsIgnored()
    {
        AppSettings& s = AppSettings::instance();
        PanDisplayKeys::writeDefault(u"WaterfallBlankingThreshold"_s, u"15"_s);
        s.setValue(u"WaterfallBlankingThreshold_1"_s, u"1.15"_s);
        s.setValue(u"WaterfallBlankingThreshold_2"_s, u"3"_s);

        for (const int pan : {0, 1, 2}) {
            SpectrumWidget w;
            w.setPanIndex(pan);
            w.loadSettings();
            QCOMPARE(w.waterfallBlankingThresholdDb(), 6);
            QVERIFY(!w.perPanDisplayKeyBases().contains(u"WaterfallBlankingThreshold"_s));
            QVERIFY(w.perPanDisplayKeyBases().contains(u"WaterfallBlankingThresholdDb"_s));
        }

        // And it still blanks at 6 dB, not at the 3 the old key holds for
        // pan 2: a row 5 dB above the baseline is kept.
        SpectrumWidget w;
        w.setPanIndex(2);
        w.loadSettings();
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);
        for (int i = 0; i < 10; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);
        w.pushWaterfallRowForTest(burstRowDb(n, 5.0));
        QVERIFY2(newestRow(w).middle != quiet.middle,
                 "the old factor key was read as a threshold in dB");
    }

    // ── NB Blank needs history before it calls anything a burst ─────────
    void blankOn_needsEightRowsFirst()
    {
        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);

        for (int i = 0; i < 7; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);
        w.pushWaterfallRowForTest(burstRowDb(n, 10.0));
        QVERIFY(newestRow(w).middle != quiet.middle);
    }

    // ── NB Blank stands still while the pan transmits ───────────────────
    void blankOn_frozenDuringTransmit()
    {
        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);

        for (int i = 0; i < 10; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);

        w.setTxActiveForTest(true);
        // Not replaced: a burst pushed while keyed is drawn.
        w.pushWaterfallRowForTest(burstRowDb(n, 10.0));
        QVERIFY2(newestRow(w).middle != quiet.middle, "a row was blanked during transmit");
        // Forty loud rows, more than the whole baseline ring holds.
        for (int i = 0; i < 40; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, -70.0f));
        }
        const NewestRow loud = newestRow(w);
        QVERIFY(loud.first != quiet.first);
        w.setTxActiveForTest(false);

        // The baseline is still the quiet one from before the transmission:
        // a row 7 dB above it is a burst against it (against forty -70 dBm
        // rows it would be far under the baseline and kept). And the row put in its
        // place is the last good RECEIVE row, quiet in every pixel, not one
        // of the loud rows pushed while keyed.
        w.pushWaterfallRowForTest(burstRowDb(n, 7.0));
        const NewestRow after = newestRow(w);
        QCOMPARE(after.middle, quiet.middle);
        QCOMPARE(after.first, quiet.first);
    }

    // ── Turning NB Blank off forgets the baseline ───────────────────────
    void blankOffThenOn_startsAFreshBaseline()
    {
        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);
        for (int i = 0; i < 10; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);

        w.setWaterfallBlankingEnabled(false);
        w.setWaterfallBlankingEnabled(true);
        w.pushWaterfallRowForTest(burstRowDb(n, 10.0));
        QVERIFY2(newestRow(w).middle != quiet.middle,
                 "a row was judged against a baseline from before the blanker was turned off");
    }

    // ── The end of a transmission forgets the baseline (fix wave I6) ────
    //
    // The ring stands still while the pan is keyed (above); at the fall it
    // and the last good row are dropped, as AetherSDR drops them, so the
    // first receive rows are drawn as they are.
    void blankOn_unkeyStartsAFreshBaseline()
    {
        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);
        for (int i = 0; i < 10; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);

        w.setMoxOverlay(true);
        w.setMoxOverlay(false);
        w.pushWaterfallRowForTest(burstRowDb(n, 10.0));
        QVERIFY2(newestRow(w).middle != quiet.middle,
                 "a row was judged against a baseline from before the transmission");
    }

    // ── Clearing the waterfall forgets the baseline (fix wave I6) ───────
    void blankOn_waterfallClearStartsAFreshBaseline()
    {
        SpectrumWidget w;
        prepareWaterfall(w);
        const int n = w.liveWaterfallForTest().width();
        w.setWaterfallBlankingEnabled(true);
        for (int i = 0; i < 10; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(n, kQuietDbm));
        }
        const NewestRow quiet = newestRow(w);

        w.clearWaterfallHistory();
        w.pushWaterfallRowForTest(burstRowDb(n, 10.0));
        QVERIFY2(newestRow(w).middle != quiet.middle,
                 "a row from before the waterfall was cleared was drawn after it");
    }

    // ── Saved per pan, reloaded, inherited, reset ───────────────────────
    void savedPerPan_reloaded_inherited_reset()
    {
        AppSettings& s = AppSettings::instance();
        const QStringList keys{u"WaterfallBlankingEnabled"_s,
                               u"WaterfallBlankingThresholdDb"_s,
                               u"DisplayFreqGridSpacing"_s,
                               u"DisplayFreqScaleFontPx"_s};
        {
            SpectrumWidget pan1;
            pan1.setPanIndex(1);
            pan1.loadSettings();

            // Per pan, and appearance: "Reset this pan" clears them.
            for (const QString& key : keys) {
                QVERIFY2(pan1.perPanDisplayKeyBases().contains(key), qPrintable(key));
                QVERIFY2(pan1.resettablePanDisplayKeyBases().contains(key), qPrintable(key));
            }

            // An untouched pan writes none of them.
            pan1.saveSettings();
            for (const QString& key : keys) {
                QVERIFY2(!PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
            }

            pan1.setWaterfallBlankingEnabled(true);
            pan1.setWaterfallBlankingThresholdDb(12);
            pan1.setFreqGridSpacingHz(5000);
            pan1.setFreqScaleFontPx(14);
            pan1.saveSettings();
        }
        QCOMPARE(s.value(u"WaterfallBlankingEnabled_1"_s).toString(), u"True"_s);
        QCOMPARE(s.value(u"WaterfallBlankingThresholdDb_1"_s).toString(), u"12"_s);
        QVERIFY(!s.contains(u"WaterfallBlankingThreshold_1"_s));
        QCOMPARE(s.value(u"DisplayFreqGridSpacing_1"_s).toString(), u"5000"_s);
        QCOMPARE(s.value(u"DisplayFreqScaleFontPx_1"_s).toString(), u"14"_s);
        // The Setup default is not this pan's to write.
        for (const QString& key : keys) {
            QVERIFY2(!s.contains(key), qPrintable(key));
        }

        // Reloaded on the same pan.
        SpectrumWidget again;
        again.setPanIndex(1);
        again.loadSettings();
        QCOMPARE(again.waterfallBlankingEnabled(), true);
        QCOMPARE(again.waterfallBlankingThresholdDb(), 12);
        QCOMPARE(again.freqGridSpacingHz(), 5000);
        QCOMPARE(again.freqScaleFontPx(), 14);

        // Another pan has its own: nothing set, so the ship defaults.
        SpectrumWidget pan2;
        pan2.setPanIndex(2);
        pan2.loadSettings();
        QCOMPARE(pan2.waterfallBlankingEnabled(), false);
        QCOMPARE(pan2.waterfallBlankingThresholdDb(), 6);
        QCOMPARE(pan2.freqGridSpacingHz(), 0);
        QCOMPARE(pan2.freqScaleFontPx(), 10);

        // An explicit All-pans write replaces every pan override for the
        // affected setting.
        PanDisplayKeys::writeDefault(u"WaterfallBlankingEnabled"_s, u"True"_s);
        PanDisplayKeys::writeDefault(u"WaterfallBlankingThresholdDb"_s, u"9"_s);
        PanDisplayKeys::writeDefault(u"DisplayFreqGridSpacing"_s, u"10000"_s);
        PanDisplayKeys::writeDefault(u"DisplayFreqScaleFontPx"_s, u"12"_s);
        for (const QString& key : keys) {
            QVERIFY2(!PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
        }
        pan2.loadSettings();
        QCOMPARE(pan2.waterfallBlankingEnabled(), true);
        QCOMPARE(pan2.waterfallBlankingThresholdDb(), 9);
        QCOMPARE(pan2.freqGridSpacingHz(), 10000);
        QCOMPARE(pan2.freqScaleFontPx(), 12);
        again.loadSettings();
        QCOMPARE(again.waterfallBlankingEnabled(), true);
        QCOMPARE(again.waterfallBlankingThresholdDb(), 9);
        QCOMPARE(again.freqGridSpacingHz(), 10000);
        QCOMPARE(again.freqScaleFontPx(), 12);

        // Recreate pan 1's own values so Reset still proves that it drops
        // sparse overrides and returns to the All-pans values.
        again.setWaterfallBlankingEnabled(false);
        again.setWaterfallBlankingThresholdDb(12);
        again.setFreqGridSpacingHz(5000);
        again.setFreqScaleFontPx(14);
        again.saveSettings();
        for (const QString& key : keys) {
            QVERIFY2(PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
        }

        // "Reset this pan" drops pan 1's own values: it follows the default.
        QSignalSpy spy(&again, &SpectrumWidget::displaySettingChanged);
        again.resetPanDisplayOverrides();
        for (const QString& key : keys) {
            QVERIFY2(!PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
        }
        QCOMPARE(again.waterfallBlankingEnabled(), true);
        QCOMPARE(again.waterfallBlankingThresholdDb(), 9);
        QCOMPARE(again.freqGridSpacingHz(), 10000);
        QCOMPARE(again.freqScaleFontPx(), 12);
        QCOMPARE(spy.count(), 0);
    }

    // ── Stored values out of range are not taken as they are ────────────
    void storedValuesOutOfRange()
    {
        PanDisplayKeys::writeDefault(u"WaterfallBlankingThresholdDb"_s, u"400"_s);
        PanDisplayKeys::writeDefault(u"DisplayFreqGridSpacing"_s, u"3000"_s);
        PanDisplayKeys::writeDefault(u"DisplayFreqScaleFontPx"_s, u"72"_s);

        SpectrumWidget w;
        w.loadSettings();
        QCOMPARE(w.waterfallBlankingThresholdDb(), 20);
        QCOMPARE(w.freqGridSpacingHz(), 0);
        QCOMPARE(w.freqScaleFontPx(), 14);
    }

    // ════════════ Task 5: trace colour and spectrum background ══════════

    void task5_defaults()
    {
        SpectrumWidget w;
        w.loadSettings();
        QCOMPARE(w.fillColor(), kDefaultFill);
        QCOMPARE(w.traceColor(), kDefaultFill);
        QCOMPARE(w.backgroundEnabled(), false);
        QCOMPARE(w.backgroundImagePath(), QString());
        QCOMPARE(w.backgroundOpacity(), 80);
        QCOMPARE(w.backgroundFillColor(), kDefaultBackground);
        QCOMPARE(w.backgroundFillColor().name(), u"#0a0a14"_s);
        QVERIFY(!w.drawsSpectrumBackgroundForTest());
    }

    // With none chosen the trace follows the fill colour; once chosen it
    // stays put when the fill colour changes.
    void traceColour_followsFillUntilChosen()
    {
        SpectrumWidget w;
        w.setFillColor(QColor(0, 255, 0));
        QCOMPARE(w.traceColor(), QColor(0, 255, 0));

        w.setTraceColor(QColor(255, 0, 0));
        QCOMPARE(w.traceColor(), QColor(255, 0, 0));
        QCOMPARE(w.fillColor(), QColor(0, 255, 0));

        w.setFillColor(QColor(0, 0, 255));
        QCOMPARE(w.traceColor(), QColor(255, 0, 0));

        // Not a colour: refused.
        w.setTraceColor(QColor());
        QCOMPARE(w.traceColor(), QColor(255, 0, 0));
    }

    void task5_values_stay_in_range()
    {
        SpectrumWidget w;
        w.setBackgroundOpacity(-5);
        QCOMPARE(w.backgroundOpacity(), 0);
        w.setBackgroundOpacity(250);
        QCOMPARE(w.backgroundOpacity(), 100);

        w.setBackgroundFillColor(QColor());
        QCOMPARE(w.backgroundFillColor(), kDefaultBackground);
        // The colour is solid; the opacity decides what shows through.
        w.setBackgroundFillColor(QColor(10, 20, 30, 40));
        QCOMPARE(w.backgroundFillColor(), QColor(10, 20, 30, 255));
    }

    void task5_setters_announce_once()
    {
        SpectrumWidget w;
        QSignalSpy spy(&w, &SpectrumWidget::displaySettingChanged);

        w.setTraceColor(QColor(255, 0, 0));
        w.setTraceColor(QColor(255, 0, 0));
        w.setBackgroundEnabled(true);
        w.setBackgroundEnabled(true);
        w.setBackgroundOpacity(40);
        w.setBackgroundOpacity(40);
        w.setBackgroundFillColor(QColor(1, 2, 3));
        w.setBackgroundFillColor(QColor(1, 2, 3));
        QTemporaryDir dir;
        const QString picture = writePicture(dir);
        QVERIFY(!picture.isEmpty());
        w.setBackgroundImagePath(picture);
        w.setBackgroundImagePath(picture);

        QCOMPARE(countOf(spy, u"DisplayTraceColor"_s), 1);
        QCOMPARE(countOf(spy, u"DisplayBackgroundEnabled"_s), 1);
        QCOMPARE(countOf(spy, u"DisplayBackgroundOpacity"_s), 1);
        QCOMPARE(countOf(spy, u"DisplayBackgroundColor"_s), 1);
        QCOMPARE(countOf(spy, u"DisplayBackgroundImage"_s), 1);
        QCOMPARE(spy.count(), 5);
    }

    // ── The trace on the QPainter path ──────────────────────────────────
    void cpuTrace_isFillColourUntilChosen_thenItsOwn()
    {
        SpectrumWidget w;
        w.setGradientEnabled(false);
        w.setPeakHoldEnabled(false);
        w.setPanFillEnabled(true);
        w.setRenderedPixelsForTest(flatTrace(w));

        // As before this task: the line is the fill colour.
        QCOMPARE(opaqueColours(renderTrace(w)), QSet<QRgb>{kDefaultFill.rgb()});

        w.setTraceColor(QColor(255, 0, 0));
        const QImage img = renderTrace(w);
        QCOMPARE(opaqueColours(img), QSet<QRgb>{QColor(255, 0, 0).rgb()});

        // The fill under the line keeps the fill colour.
        const QColor under = img.pixelColor(kAreaW / 2, kAreaH - 5);
        QVERIFY2(under.alpha() > 0 && under.alpha() < 255, "no translucent fill under the trace");
        QCOMPARE(under.red(), 0);
        QVERIFY2(under.blue() > 200 && under.green() > 180,
                 qPrintable(under.name(QColor::HexArgb)));
    }

    // ── The trace and fill on the GPU path ──────────────────────────────
    void gpuVertices_traceColour_and_fillLowerEdge()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("this build has no GPU spectrum path");
#else
        SpectrumWidget w;
        w.setGradientEnabled(false);
        w.setRenderedPixelsForTest(flatTrace(w, 16));

        auto rgbAt = [](const QVector<float>& v, int vertex, int stride) {
            const int i = vertex * stride;
            return QColor::fromRgbF(v[i + 2], v[i + 3], v[i + 4]);
        };

        // As before this task: line and fill top are the fill colour.
        {
            const auto v = w.fftTraceVerticesForTest();
            QCOMPARE(v.stride, 6);
            QCOMPARE(v.line.size(), 16 * v.stride);
            QCOMPARE(v.fill.size(), 16 * 2 * v.stride);
            for (int j = 0; j < 16; ++j) {
                QCOMPARE(rgbAt(v.line, j, v.stride).rgb(), kDefaultFill.rgb());
                QCOMPARE(v.line[j * v.stride + 5], 0.9f);
                QCOMPARE(rgbAt(v.fill, 2 * j, v.stride).rgb(), kDefaultFill.rgb());
                // S10. The lower edge was (0, 0, 0.3) = #00004d whatever the
                // fill colour; it is now the fill colour darkened, which for
                // the default fill colour #00e5ff is #004c55.
                QCOMPARE(rgbAt(v.fill, 2 * j + 1, v.stride).name(), u"#004c55"_s);
                QCOMPARE(v.fill[(2 * j + 1) * v.stride + 1], -1.0f);
            }
        }

        // A chosen trace colour changes the line only.
        w.setTraceColor(QColor(255, 0, 0));
        {
            const auto v = w.fftTraceVerticesForTest();
            for (int j = 0; j < 16; ++j) {
                QCOMPARE(rgbAt(v.line, j, v.stride).rgb(), QColor(255, 0, 0).rgb());
                QCOMPARE(rgbAt(v.fill, 2 * j, v.stride).rgb(), kDefaultFill.rgb());
                QCOMPARE(rgbAt(v.fill, 2 * j + 1, v.stride).name(), u"#004c55"_s);
            }
        }

        // The lower edge follows another fill colour.
        w.setFillColor(QColor(255, 128, 0));
        {
            const auto v = w.fftTraceVerticesForTest();
            QCOMPARE(rgbAt(v.fill, 0, v.stride).rgb(), QColor(255, 128, 0).rgb());
            QCOMPARE(rgbAt(v.fill, 1, v.stride).rgb(),
                     QColor(255, 128, 0).darker(300).rgb());
            QCOMPARE(rgbAt(v.line, 0, v.stride).rgb(), QColor(255, 0, 0).rgb());
        }
#endif
    }

    // With the heat map on and no trace colour chosen the line keeps the
    // heat map's colours, as before; a chosen colour replaces them.
    void gpuVertices_heatMapLine()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("this build has no GPU spectrum path");
#else
        SpectrumWidget w;
        w.setGradientEnabled(true);
        w.setRenderedPixelsForTest(flatTrace(w, 4));   // t = 0.5: green

        auto v = w.fftTraceVerticesForTest();
        QCOMPARE(QColor::fromRgbF(v.line[2], v.line[3], v.line[4]).rgb(),
                 QColor(0, 255, 0).rgb());

        w.setTraceColor(QColor(255, 0, 255));
        v = w.fftTraceVerticesForTest();
        QCOMPARE(QColor::fromRgbF(v.line[2], v.line[3], v.line[4]).rgb(),
                 QColor(255, 0, 255).rgb());
        // The fill is still the heat map's.
        QCOMPARE(QColor::fromRgbF(v.fill[2], v.fill[3], v.fill[4]).rgb(),
                 QColor(0, 255, 0).rgb());
#endif
    }

    // ── Background off: nothing is drawn for it ─────────────────────────
    void backgroundOff_drawsNothing()
    {
        SpectrumWidget w;
        // Everything chosen, but off.
        w.setBackgroundFillColor(QColor(255, 0, 0));
        w.setBackgroundOpacity(30);
        QTemporaryDir dir;
        const QString picture = writePicture(dir);
        QVERIFY(!picture.isEmpty());
        w.setBackgroundImagePath(picture);

        QVERIFY(!w.drawsSpectrumBackgroundForTest());
        QVERIFY(!w.backgroundImageLoadedForTest());
        const QImage img = renderBackground(w, QColor(1, 2, 3));
        for (int y = 0; y < img.height(); y += 7) {
            for (int x = 0; x < img.width(); x += 7) {
                QCOMPARE(img.pixel(x, y), QColor(1, 2, 3).rgb());
            }
        }
    }

    // ── Background on, colour only ──────────────────────────────────────
    void backgroundOn_isTheColour()
    {
        SpectrumWidget w;
        w.setBackgroundEnabled(true);
        QVERIFY(w.drawsSpectrumBackgroundForTest());

        // The default colour first, then a chosen one. With no picture the
        // opacity has nothing to show through.
        QImage img = renderBackground(w, QColor(1, 2, 3));
        QCOMPARE(img.pixel(0, 0), kDefaultBackground.rgb());
        QCOMPARE(img.pixel(kAreaW - 1, kAreaH - 1), kDefaultBackground.rgb());

        w.setBackgroundFillColor(QColor(255, 0, 0));
        for (const int opacity : {0, 80, 100}) {
            w.setBackgroundOpacity(opacity);
            img = renderBackground(w, QColor(1, 2, 3));
            QCOMPARE(img.pixel(0, 0), QColor(255, 0, 0).rgb());
            QCOMPARE(img.pixel(kAreaW / 2, kAreaH / 2), QColor(255, 0, 0).rgb());
            QCOMPARE(img.pixel(kAreaW - 1, kAreaH - 1), QColor(255, 0, 0).rgb());
        }

        // The 3D spectrum is not given a background.
        w.setSpectrumRenderMode(1);
        QVERIFY(!w.drawsSpectrumBackgroundForTest());
        w.setSpectrumRenderMode(0);
        QVERIFY(w.drawsSpectrumBackgroundForTest());
    }

    // ── Background picture: scaled to the area, under the colour ────────
    void backgroundPicture_scaled_opacity_clear_off()
    {
        QTemporaryDir dir;
        const QString picture = writePicture(dir);
        QVERIFY(!picture.isEmpty());

        SpectrumWidget w;
        w.setBackgroundEnabled(true);
        w.setBackgroundFillColor(QColor(255, 0, 0));
        w.setBackgroundImagePath(picture);
        QCOMPARE(w.backgroundImagePath(), picture);
        QVERIFY(w.backgroundImageLoadedForTest());

        const QPoint left(kAreaW / 4, kAreaH / 2);
        const QPoint right(3 * kAreaW / 4, kAreaH / 2);

        // Opacity 0: the picture alone, its halves filling the area's.
        w.setBackgroundOpacity(0);
        QImage img = renderBackground(w);
        QVERIFY2(near(img.pixelColor(left), QColor(0, 255, 0)),
                 qPrintable(img.pixelColor(left).name()));
        QVERIFY2(near(img.pixelColor(right), QColor(0, 0, 255)),
                 qPrintable(img.pixelColor(right).name()));
        QVERIFY(near(img.pixelColor(2, 2), QColor(0, 255, 0)));
        QVERIFY(near(img.pixelColor(kAreaW - 3, kAreaH - 3), QColor(0, 0, 255)));

        // Opacity 80 (the default): the colour covers 80% of it.
        w.setBackgroundOpacity(80);
        img = renderBackground(w);
        QVERIFY2(near(img.pixelColor(left), QColor(204, 51, 0)),
                 qPrintable(img.pixelColor(left).name()));
        QVERIFY2(near(img.pixelColor(right), QColor(204, 0, 51)),
                 qPrintable(img.pixelColor(right).name()));

        // Opacity 100: the colour alone.
        w.setBackgroundOpacity(100);
        img = renderBackground(w);
        QVERIFY(near(img.pixelColor(left), QColor(255, 0, 0)));

        // Off keeps the choice and stops drawing it.
        w.setBackgroundOpacity(0);
        w.setBackgroundEnabled(false);
        QCOMPARE(w.backgroundImagePath(), picture);
        QVERIFY(!w.drawsSpectrumBackgroundForTest());
        img = renderBackground(w, QColor(1, 2, 3));
        QCOMPARE(img.pixel(left), QColor(1, 2, 3).rgb());

        // On again: the same picture is back.
        w.setBackgroundEnabled(true);
        img = renderBackground(w);
        QVERIFY(near(img.pixelColor(left), QColor(0, 255, 0)));

        // Clear removes the picture: the colour is what is left.
        w.setBackgroundImagePath(QString());
        QCOMPARE(w.backgroundImagePath(), QString());
        QVERIFY(!w.backgroundImageLoadedForTest());
        img = renderBackground(w);
        QCOMPARE(img.pixel(left), QColor(255, 0, 0).rgb());
        QCOMPARE(img.pixel(right), QColor(255, 0, 0).rgb());
    }

    // ── A picture that cannot be read ───────────────────────────────────
    void backgroundPicture_unreadable_warnsOnce_drawsNoPicture()
    {
        QTemporaryDir dir;
        const QString missing = dir.filePath(u"not-there.png"_s);
        const QRegularExpression warning(u"failed to load background image"_s);

        SpectrumWidget w;
        w.setBackgroundEnabled(true);
        w.setBackgroundFillColor(QColor(255, 0, 0));
        w.setBackgroundOpacity(0);

        // One warning is expected; a second one fails the test.
        QTest::ignoreMessage(QtWarningMsg, warning);
        QTest::failOnWarning(warning);
        w.setBackgroundImagePath(missing);

        QCOMPARE(w.backgroundImagePath(), missing);
        QVERIFY(!w.backgroundImageLoadedForTest());
        // Drawn twice: no picture, the colour, and no further warning.
        for (int i = 0; i < 2; ++i) {
            const QImage img = renderBackground(w);
            QCOMPARE(img.pixel(kAreaW / 2, kAreaH / 2), QColor(255, 0, 0).rgb());
        }
        // The widget keeps working.
        w.setBackgroundOpacity(50);
        QCOMPARE(w.backgroundOpacity(), 50);
    }

    // ── Saved per pan, reloaded, inherited, reset ───────────────────────
    void task5_savedPerPan_reloaded_inherited_reset()
    {
        QTemporaryDir dir;
        const QString picture = writePicture(dir);
        QVERIFY(!picture.isEmpty());

        AppSettings& s = AppSettings::instance();
        const QStringList keys{u"DisplayTraceColor"_s,
                               u"DisplayBackgroundEnabled"_s,
                               u"DisplayBackgroundImage"_s,
                               u"DisplayBackgroundOpacity"_s,
                               u"DisplayBackgroundColor"_s};
        {
            SpectrumWidget pan1;
            pan1.setPanIndex(1);
            pan1.loadSettings();

            for (const QString& key : keys) {
                QVERIFY2(pan1.perPanDisplayKeyBases().contains(key), qPrintable(key));
                QVERIFY2(pan1.resettablePanDisplayKeyBases().contains(key), qPrintable(key));
            }

            // An untouched pan writes none of them.
            pan1.saveSettings();
            for (const QString& key : keys) {
                QVERIFY2(!PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
            }

            pan1.setTraceColor(QColor(255, 0, 0));
            pan1.setBackgroundEnabled(true);
            pan1.setBackgroundImagePath(picture);
            pan1.setBackgroundOpacity(35);
            pan1.setBackgroundFillColor(QColor(0x10, 0x20, 0x30));
            pan1.saveSettings();
        }
        QCOMPARE(QColor::fromString(s.value(u"DisplayTraceColor_1"_s).toString()),
                 QColor(255, 0, 0));
        QCOMPARE(s.value(u"DisplayBackgroundEnabled_1"_s).toString(), u"True"_s);
        QCOMPARE(s.value(u"DisplayBackgroundImage_1"_s).toString(), picture);
        QCOMPARE(s.value(u"DisplayBackgroundOpacity_1"_s).toString(), u"35"_s);
        QCOMPARE(QColor::fromString(s.value(u"DisplayBackgroundColor_1"_s).toString()),
                 QColor(0x10, 0x20, 0x30));
        // The Setup default is not this pan's to write.
        for (const QString& key : keys) {
            QVERIFY2(!s.contains(key), qPrintable(key));
        }

        // Reloaded on the same pan, picture included.
        SpectrumWidget again;
        again.setPanIndex(1);
        again.loadSettings();
        QCOMPARE(again.traceColor(), QColor(255, 0, 0));
        QCOMPARE(again.backgroundEnabled(), true);
        QCOMPARE(again.backgroundImagePath(), picture);
        QVERIFY(again.backgroundImageLoadedForTest());
        QCOMPARE(again.backgroundOpacity(), 35);
        QCOMPARE(again.backgroundFillColor(), QColor(0x10, 0x20, 0x30));

        // Another pan has its own: nothing set, so the ship defaults.
        SpectrumWidget pan2;
        pan2.setPanIndex(2);
        pan2.loadSettings();
        QCOMPARE(pan2.traceColor(), kDefaultFill);
        QCOMPARE(pan2.backgroundEnabled(), false);
        QCOMPARE(pan2.backgroundImagePath(), QString());
        QCOMPARE(pan2.backgroundOpacity(), 80);
        QCOMPARE(pan2.backgroundFillColor(), kDefaultBackground);

        // An explicit All-pans write replaces pan overrides for the settings
        // it changes. The picture path is untouched and remains per pan.
        PanDisplayKeys::writeDefault(u"DisplayTraceColor"_s, u"#ff00ff00"_s);
        PanDisplayKeys::writeDefault(u"DisplayBackgroundEnabled"_s, u"True"_s);
        PanDisplayKeys::writeDefault(u"DisplayBackgroundOpacity"_s, u"60"_s);
        PanDisplayKeys::writeDefault(u"DisplayBackgroundColor"_s, u"#ff000040"_s);
        for (const QString& key : {u"DisplayTraceColor"_s,
                                   u"DisplayBackgroundEnabled"_s,
                                   u"DisplayBackgroundOpacity"_s,
                                   u"DisplayBackgroundColor"_s}) {
            QVERIFY2(!PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
        }
        QVERIFY(PanDisplayKeys::hasOverride(u"DisplayBackgroundImage"_s, 1));
        pan2.loadSettings();
        QCOMPARE(pan2.traceColor(), QColor(0, 255, 0));
        QCOMPARE(pan2.backgroundEnabled(), true);
        QCOMPARE(pan2.backgroundOpacity(), 60);
        QCOMPARE(pan2.backgroundFillColor(), QColor(0, 0, 0x40));
        again.loadSettings();
        QCOMPARE(again.traceColor(), QColor(0, 255, 0));
        QCOMPARE(again.backgroundEnabled(), true);
        QCOMPARE(again.backgroundImagePath(), picture);
        QCOMPARE(again.backgroundOpacity(), 60);
        QCOMPARE(again.backgroundFillColor(), QColor(0, 0, 0x40));

        // Recreate a complete set of pan overrides so Reset exercises every
        // member of the per-pan picture group.
        again.setTraceColor(QColor(255, 0, 0));
        again.setBackgroundEnabled(false);
        again.setBackgroundImagePath(picture);
        again.setBackgroundOpacity(35);
        again.setBackgroundFillColor(QColor(0x10, 0x20, 0x30));
        again.saveSettings();
        for (const QString& key : keys) {
            QVERIFY2(PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
        }

        // "Reset this pan" drops pan 1's own values: it follows the default,
        // and its picture is gone.
        QSignalSpy spy(&again, &SpectrumWidget::displaySettingChanged);
        again.resetPanDisplayOverrides();
        for (const QString& key : keys) {
            QVERIFY2(!PanDisplayKeys::hasOverride(key, 1), qPrintable(key));
        }
        QCOMPARE(again.traceColor(), QColor(0, 255, 0));
        QCOMPARE(again.backgroundEnabled(), true);
        QCOMPARE(again.backgroundImagePath(), QString());
        QVERIFY(!again.backgroundImageLoadedForTest());
        QCOMPARE(again.backgroundOpacity(), 60);
        QCOMPARE(again.backgroundFillColor(), QColor(0, 0, 0x40));
        QCOMPARE(spy.count(), 0);
    }

    // A reset with no Setup default puts the trace back on the fill colour.
    void task5_reset_traceFollowsFillAgain()
    {
        SpectrumWidget w;
        w.loadSettings();
        w.setTraceColor(QColor(255, 0, 0));
        w.saveSettings();
        QVERIFY(PanDisplayKeys::hasOverride(u"DisplayTraceColor"_s, 0));

        w.resetPanDisplayOverrides();
        QVERIFY(!PanDisplayKeys::hasOverride(u"DisplayTraceColor"_s, 0));
        QCOMPARE(w.traceColor(), kDefaultFill);
        w.setFillColor(QColor(0, 255, 0));
        QCOMPARE(w.traceColor(), QColor(0, 255, 0));
    }

    // ── Trace line width: 0.1 to 5.0 px in tenths ───────────────────────
    void lineWidth_setterHoldsTheRangeAndTenths()
    {
        SpectrumWidget w;
        QCOMPARE(w.lineWidth(), 1.0f);
        w.setLineWidth(0.0f);
        QCOMPARE(w.lineWidth(), 0.1f);
        w.setLineWidth(-3.0f);
        QCOMPARE(w.lineWidth(), 0.1f);
        w.setLineWidth(8.0f);
        QCOMPARE(w.lineWidth(), 5.0f);
        w.setLineWidth(0.3f);
        QCOMPARE(w.lineWidth(), 0.3f);
        w.setLineWidth(2.26f);
        QCOMPARE(w.lineWidth(), 2.3f);
    }

    void lineWidth_storedValueIsClampedAndRounded_data()
    {
        QTest::addColumn<QString>("stored");
        QTest::addColumn<float>("read");
        QTest::newRow("0.04 reads as 0.1") << u"0.04"_s << 0.1f;
        QTest::newRow("0 reads as 0.1")    << u"0"_s    << 0.1f;
        QTest::newRow("7 reads as 5.0")    << u"7"_s    << 5.0f;
        QTest::newRow("0.5 still 0.5")     << u"0.5"_s  << 0.5f;
        QTest::newRow("0.3 reads 0.3")     << u"0.3"_s  << 0.3f;
        QTest::newRow("1.5 reads 1.5")     << u"1.5"_s  << 1.5f;
        QTest::newRow("3 reads 3.0")       << u"3"_s    << 3.0f;
        QTest::newRow("1.26 reads 1.3")    << u"1.26"_s << 1.3f;
        QTest::newRow("not a number is the default") << u"wide"_s << 1.0f;
    }

    void lineWidth_storedValueIsClampedAndRounded()
    {
        QFETCH(QString, stored);
        QFETCH(float, read);
        PanDisplayKeys::writeDefault(u"DisplayLineWidth"_s, stored);
        SpectrumWidget w;
        w.loadSettings();
        QCOMPARE(w.lineWidth(), read);
    }

    void lineWidth_noStoredValueIsTheDefault()
    {
        SpectrumWidget w;
        w.loadSettings();
        QCOMPARE(w.lineWidth(), 1.0f);
    }

    // The QPainter path: a 0.3 px trace has measurably less ink than a
    // 1.0 px one, and a 5.0 px one has more. The check can fail: 1.0
    // against 1.0 is not "measurably less".
    void lineWidth_painterDrawsThinnerBelowOnePixel()
    {
        const QImage thin = renderTraceAtWidth(0.3f);
        const QImage one = renderTraceAtWidth(1.0f);
        const QImage oneAgain = renderTraceAtWidth(1.0f);
        const QImage wide = renderTraceAtWidth(5.0f);

        QVERIFY(inkOf(one) > 0);
        QVERIFY(inkOf(thin) > 0);
        QCOMPARE(inkOf(oneAgain), inkOf(one));
        QVERIFY2(!hasMeasurablyLessInk(oneAgain, one), "1.0 px against 1.0 px must not pass");
        QVERIFY2(hasMeasurablyLessInk(thin, one),
                 qPrintable(u"0.3 px ink %1, 1.0 px ink %2"_s.arg(inkOf(thin)).arg(inkOf(one))));
        QVERIFY2(hasMeasurablyLessInk(one, wide),
                 qPrintable(u"1.0 px ink %1, 5.0 px ink %2"_s.arg(inkOf(one)).arg(inkOf(wide))));
    }

    // The GPU path uploads the trace as geometry of its width. Over one
    // device pixel a level trace is rectangles the line width tall; at one
    // device pixel or under it is the line itself and its alpha carries
    // the width.
    void lineWidth_gpuRibbonIsTheLineWidth_data()
    {
        QTest::addColumn<float>("widthPx");
        QTest::addColumn<float>("dpr");
        QTest::addColumn<float>("tallDevicePx");
        QTest::addColumn<float>("alpha");
        QTest::newRow("0.1 px")        << 0.1f << 1.0f << 1.0f  << 0.09f;
        QTest::newRow("0.3 px")        << 0.3f << 1.0f << 1.0f  << 0.27f;
        QTest::newRow("1.0 px")        << 1.0f << 1.0f << 1.0f  << 0.9f;
        QTest::newRow("1.1 px")        << 1.1f << 1.0f << 1.1f  << 0.9f;
        QTest::newRow("1.5 px")        << 1.5f << 1.0f << 1.5f  << 0.9f;
        QTest::newRow("4.0 px")        << 4.0f << 1.0f << 4.0f  << 0.9f;
        QTest::newRow("5.0 px")        << 5.0f << 1.0f << 5.0f  << 0.9f;
        QTest::newRow("0.3 px at 2x")  << 0.3f << 2.0f << 1.0f  << 0.54f;
        QTest::newRow("0.5 px at 2x")  << 0.5f << 2.0f << 1.0f  << 0.9f;
        QTest::newRow("1.5 px at 2x")  << 1.5f << 2.0f << 3.0f  << 0.9f;
        QTest::newRow("5.0 px at 2x")  << 5.0f << 2.0f << 10.0f << 0.9f;
    }

    void lineWidth_gpuRibbonIsTheLineWidth()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("this build has no GPU spectrum path");
#else
        QFETCH(float, widthPx);
        QFETCH(float, dpr);
        QFETCH(float, tallDevicePx);
        QFETCH(float, alpha);
        constexpr int kStride = 6;
        constexpr int kPixels = 16;
        const float vpW = kAreaW * dpr;
        const float vpH = kAreaH * dpr;

        SpectrumWidget w;
        w.setGradientEnabled(false);
        w.setLineWidth(widthPx);
        w.setRenderedPixelsForTest(flatTrace(w, kPixels));
        const auto line = w.fftTraceVerticesForTest().line;
        const QVector<float> ribbon = w.fftTraceRibbonForTest(vpW, vpH, dpr);

        if (tallDevicePx <= 1.0f) {
            // The line strip itself, one device pixel wide by the
            // rasterizer, its alpha scaled below one device pixel.
            QCOMPARE(traceRibbonTopology(widthPx * dpr), TraceRibbonTopology::LineStrip);
            QCOMPARE(ribbon.size(), kPixels * kStride);
            for (int j = 0; j < kPixels; ++j) {
                for (int c = 0; c < 5; ++c) {
                    QCOMPARE(ribbon[j * kStride + c], line[j * kStride + c]);
                }
                QVERIFY(std::abs(ribbon[j * kStride + 5] - alpha) < 0.0001f);
            }
            return;
        }

        QCOMPARE(traceRibbonTopology(widthPx * dpr), TraceRibbonTopology::TriangleStrip);
        QCOMPARE(traceRibbonVertexCount(kPixels, widthPx * dpr), (kPixels - 1) * 4);
        QCOMPARE(ribbon.size(), (kPixels - 1) * 4 * kStride);
        for (int segment = 0; segment < kPixels - 1; ++segment) {
            for (int end = 0; end < 2; ++end) {
                const int j = segment + end;
                const int up = (segment * 4 + end * 2) * kStride;
                const int down = up + kStride;
                // Straight above and below the line vertex, centred on it.
                QCOMPARE(ribbon[up], line[j * kStride]);
                QCOMPARE(ribbon[down], line[j * kStride]);
                const float tall = std::abs(ribbon[up + 1] - ribbon[down + 1]) * vpH / 2.0f;
                QVERIFY2(std::abs(tall - tallDevicePx) < 0.001f,
                         qPrintable(u"vertex %1 is %2 px tall"_s.arg(j).arg(tall)));
                QVERIFY(std::abs((ribbon[up + 1] + ribbon[down + 1]) / 2.0f
                                 - line[j * kStride + 1]) < 0.0001f);
                // The trace's colour and alpha, unscaled.
                for (int c = 2; c < 5; ++c) {
                    QCOMPARE(ribbon[up + c], line[j * kStride + c]);
                    QCOMPARE(ribbon[down + c], line[j * kStride + c]);
                }
                QVERIFY(std::abs(ribbon[up + 5] - alpha) < 0.0001f);
                QVERIFY(std::abs(ribbon[down + 5] - alpha) < 0.0001f);
            }
        }
#endif
    }

    // Every segment's rectangle is the line width across that segment,
    // however steep, and consecutive rectangles meet at the shared point.
    void lineWidth_ribbonKeepsItsWidthAcrossEverySegment()
    {
        constexpr int kStride = 6;
        constexpr float kVp = 100.0f;      // square viewport: 50 px per unit
        constexpr float kWidth = 4.0f;
        const auto vertex = [](QVector<float>& v, float xPx, float yPx) {
            v << xPx / 50.0f - 1.0f << yPx / 50.0f - 1.0f << 1.0f << 1.0f << 1.0f << 0.9f;
        };
        struct Px { float x; float y; };
        const auto at = [&](const QVector<float>& ribbon, int index) {
            return Px{(ribbon[index * kStride] + 1.0f) * 50.0f,
                      (ribbon[index * kStride + 1] + 1.0f) * 50.0f};
        };
        // For each segment: both ends' pairs are the width apart, at right
        // angles to the segment, centred on the segment's ends.
        const auto checkSegments = [&](const QVector<float>& line, int n,
                                       const QVector<float>& ribbon) {
            if (ribbon.size() != (n - 1) * 4 * kStride) {
                return false;
            }
            for (int segment = 0; segment < n - 1; ++segment) {
                const Px a{(line[segment * kStride] + 1.0f) * 50.0f,
                           (line[segment * kStride + 1] + 1.0f) * 50.0f};
                const Px b{(line[(segment + 1) * kStride] + 1.0f) * 50.0f,
                           (line[(segment + 1) * kStride + 1] + 1.0f) * 50.0f};
                const float len = std::hypot(b.x - a.x, b.y - a.y);
                for (int end = 0; end < 2; ++end) {
                    const Px left = at(ribbon, segment * 4 + end * 2);
                    const Px right = at(ribbon, segment * 4 + end * 2 + 1);
                    const Px centre = end == 0 ? a : b;
                    const float gap = std::hypot(left.x - right.x, left.y - right.y);
                    const float along = ((left.x - right.x) * (b.x - a.x)
                                         + (left.y - right.y) * (b.y - a.y)) / len;
                    if (std::abs(gap - kWidth) > 0.001f || std::abs(along) > 0.001f
                        || std::abs((left.x + right.x) / 2.0f - centre.x) > 0.001f
                        || std::abs((left.y + right.y) / 2.0f - centre.y) > 0.001f) {
                        return false;
                    }
                }
            }
            return true;
        };

        // A straight 45 degree climb.
        QVector<float> slope;
        for (int j = 0; j < 5; ++j) { vertex(slope, 10.0f + 10.0f * j, 10.0f + 10.0f * j); }
        QVector<float> ribbon;
        buildSpectrumTraceRibbon(slope, 5, kStride, kVp, kVp, kWidth, ribbon);
        QVERIFY(checkSegments(slope, 5, ribbon));

        // A needle: up 40 px and straight back down within 2 px. Both runs
        // keep the full width across them (the first ribbon left them a
        // fifth of a pixel wide), and nothing reaches past half the width
        // from the tip.
        QVector<float> needle;
        vertex(needle, 49.0f, 10.0f);
        vertex(needle, 50.0f, 50.0f);
        vertex(needle, 51.0f, 10.0f);
        buildSpectrumTraceRibbon(needle, 3, kStride, kVp, kVp, kWidth, ribbon);
        QVERIFY(checkSegments(needle, 3, ribbon));
        for (int index = 2; index < 6; ++index) {
            const Px corner = at(ribbon, index);
            QVERIFY(std::abs(std::hypot(corner.x - 50.0f, corner.y - 50.0f) - kWidth / 2.0f)
                    < 0.001f);
        }

        // One vertex: nothing to draw. No viewport: rectangles with no area.
        QVector<float> one;
        vertex(one, 50.0f, 50.0f);
        buildSpectrumTraceRibbon(one, 1, kStride, kVp, kVp, kWidth, ribbon);
        QCOMPARE(ribbon.size(), 0);
        buildSpectrumTraceRibbon(slope, 5, kStride, 0.0f, 0.0f, kWidth, ribbon);
        QCOMPARE(ribbon.size(), 4 * 4 * kStride);
        for (int index = 0; index < 16; index += 2) {
            QCOMPARE(ribbon[index * kStride], ribbon[(index + 1) * kStride]);
            QCOMPARE(ribbon[index * kStride + 1], ribbon[(index + 1) * kStride + 1]);
        }
        // The buffer holds the most any width makes.
        QCOMPARE(traceRibbonMaxVertexCount(5), 16);
        QCOMPARE(traceRibbonMaxVertexCount(1), 1);
        QCOMPARE(traceRibbonVertexCount(5, 1.0f), 5);
        QCOMPARE(traceRibbonVertexCount(5, 1.1f), 16);
        QCOMPARE(traceRibbonVertexCount(0, 4.0f), 0);
    }

    void task5_storedValuesOutOfRange()
    {
        PanDisplayKeys::writeDefault(u"DisplayBackgroundOpacity"_s, u"400"_s);
        PanDisplayKeys::writeDefault(u"DisplayBackgroundColor"_s, u"not a colour"_s);
        PanDisplayKeys::writeDefault(u"DisplayTraceColor"_s, u"not a colour"_s);

        SpectrumWidget w;
        w.loadSettings();
        QCOMPARE(w.backgroundOpacity(), 100);
        QCOMPARE(w.backgroundFillColor(), kDefaultBackground);
        QCOMPARE(w.traceColor(), kDefaultFill);
    }
};

QTEST_MAIN(TestSpectrumWidgetAdditions)
#include "tst_spectrum_widget_additions.moc"
