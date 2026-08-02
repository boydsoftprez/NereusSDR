// =================================================================
// tests/tst_spectrum_reducer_widget_parity.cpp  (NereusSDR)
// =================================================================
//
// R1 Task 5 -- the standing guarantee that came out of extracting
// crop-and-reduce from SpectrumWidget into core.
//
// What SpectrumWidget renders must equal what a bare SpectrumReducer
// produces from the same inputs, bit for bit.  That equality is the whole
// premise of the daemon split: nereusd runs the reducer with no widget in
// the process and its spectrum must be the same spectrum.  A future edit
// that quietly puts widget-specific behaviour back into the reduction path
// (a geometry read, a clamp applied only on the widget side, an extra
// avenger reset) breaks this test rather than shipping two subtly
// different spectra.
//
// The widget supplies ReducerConfig::pixels from its own geometry, so the
// test reproduces that expression -- qMax(width() - reservedRightEdgeWidth(),
// 800) -- and nothing else about the widget may influence the result.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 3 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QApplication>

#include <cmath>

#include "core/spectrum/SpectrumReducer.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

// Deterministic linear-power bins with tones at asymmetric positions, so a
// crop offset error moves a tone into the wrong pixel instead of cancelling
// out by symmetry.
QVector<float> makeBins(int n, int frame)
{
    QVector<float> bins(n, 1.0e-12f);
    for (int i = 0; i < n; ++i) {
        const double phase = static_cast<double>(i) * 0.017 + frame * 0.31;
        bins[i] += static_cast<float>(std::abs(std::sin(phase))) * 1.0e-11f;
    }
    bins[n / 2]           = 1.0e-3f;
    bins[n / 4 + 7]       = 3.0e-5f;
    bins[(3 * n) / 4 - 3] = 7.0e-6f;
    return bins;
}

// The widget's own av_mode mapping (SpectrumWidget.cpp updateSpectrumLinear,
// wire-format codes per WDSP analyzer.c:464 [v2.10.3.13]).
int avengerMode(SpectrumAveraging m)
{
    switch (m) {
    case SpectrumAveraging::None:         return 0;
    case SpectrumAveraging::Recursive:    return 1;
    case SpectrumAveraging::TimeWindow:   return 2;
    case SpectrumAveraging::LogRecursive: return 3;
    default: return 0;
    }
}

}  // namespace

class TstSpectrumReducerWidgetParity : public QObject
{
    Q_OBJECT

private:
    void runCase(int widgetW,
                 bool stripVisible,
                 double centreHz,
                 double spanHz,
                 double streamCentreHz,
                 double sampleRateHz,
                 SpectrumDetector det,
                 SpectrumAveraging avg,
                 float alpha,
                 int binCount,
                 int frames)
    {
        SpectrumWidget w;
        w.resize(widgetW, 600);
        w.setDbmScaleVisible(stripVisible);
        w.setSampleRate(sampleRateHz);
        w.setDdcCenterFrequency(streamCentreHz);
        w.setFrequencyRange(centreHz, spanHz);
        w.setSpectrumDetector(det);
        w.setSpectrumAveraging(avg);
        w.setWaterfallDetector(det);
        w.setWaterfallAveraging(avg);
        w.setAverageAlpha(alpha);

        // The one thing the widget is still allowed to contribute.
        ReducerConfig cfg;
        cfg.pixels         = qMax(w.width() - w.reservedRightEdgeWidth(), 800);
        cfg.centreHz       = centreHz;
        cfg.spanHz         = spanHz;
        cfg.streamCentreHz = streamCentreHz;
        cfg.sampleRateHz   = sampleRateHz;
        cfg.detector       = det;
        cfg.averageMode    = avengerMode(avg);
        cfg.averageTau     = static_cast<double>(w.spectrumAverageAlpha());

        SpectrumReducer spec;
        SpectrumReducer wf;
        spec.setConfig(cfg);

        ReducerConfig wfCfg = cfg;
        wfCfg.averageTau = static_cast<double>(w.waterfallAverageAlpha());
        wf.setConfig(wfCfg);

        const double windowEnb = 2.0;
        const double dbmOffset = -12.5;

        for (int f = 0; f < frames; ++f) {
            const QVector<float> bins = makeBins(binCount, f);
            w.updateSpectrumLinear(0, bins, windowEnb, dbmOffset);
            spec.reduce(bins, windowEnb, dbmOffset);
            wf.reduce(bins, windowEnb, dbmOffset);
        }

        const QString tag = QStringLiteral("w=%1 strip=%2 span=%3 det=%4 avg=%5")
                                .arg(widgetW)
                                .arg(stripVisible)
                                .arg(spanHz)
                                .arg(static_cast<int>(det))
                                .arg(static_cast<int>(avg));

        QVERIFY2(w.renderedPixels().size() == cfg.pixels,
                 qPrintable(tag + QStringLiteral(": widget produced %1 pixels, "
                                                 "config asked for %2")
                                      .arg(w.renderedPixels().size())
                                      .arg(cfg.pixels)));
        QVERIFY2(w.renderedPixels() == spec.output(),
                 qPrintable(tag + QStringLiteral(": spectrum plane diverged")));
        QVERIFY2(w.wfRenderedPixels() == wf.output(),
                 qPrintable(tag + QStringLiteral(": waterfall plane diverged")));
    }

private slots:

    // Sweep the axes that used to be entangled with widget state: widget
    // width (including the headless width-0 case that falls through to the
    // 800 floor), dBm strip visibility (which changes the reserved right
    // edge and therefore the pixel count), and the crop window.
    void widgetMatchesBareReducerAcrossGeometryAndCrop()
    {
        struct Win { double c; double s; double d; double sr; };
        const Win windows[] = {
            { 14'200'000.0, 192'000.0, 14'200'000.0, 192'000.0 },  // full span
            { 14'200'000.0,  96'000.0, 14'200'000.0, 192'000.0 },  // zoomed 2x
            { 14'290'000.0, 192'000.0, 14'200'000.0, 192'000.0 },  // off centre
            { 14'212'345.0,   6'000.0, 14'200'000.0, 768'000.0 },  // deep zoom
            { 13'000'000.0, 192'000.0, 14'200'000.0, 192'000.0 },  // off stream
        };

        for (const Win& win : windows) {
            for (int widgetW : {0, 900, 1280, 1921}) {
                for (bool strip : {false, true}) {
                    runCase(widgetW, strip, win.c, win.s, win.d, win.sr,
                            SpectrumDetector::Peak,
                            SpectrumAveraging::LogRecursive,
                            0.4f, 4096, 5);
                }
            }
        }
    }

    // Every detector against every averaging mode, so no combination gets
    // its own private code path on one side of the split.
    void widgetMatchesBareReducerAcrossDetectorAndAveraging()
    {
        const SpectrumDetector dets[] = {
            SpectrumDetector::Peak,
            SpectrumDetector::Rosenfell,
            SpectrumDetector::Average,
            SpectrumDetector::Sample,
            SpectrumDetector::RMS,
        };
        const SpectrumAveraging avgs[] = {
            SpectrumAveraging::None,
            SpectrumAveraging::Recursive,
            SpectrumAveraging::TimeWindow,
            SpectrumAveraging::LogRecursive,
        };

        for (const auto det : dets) {
            for (const auto avg : avgs) {
                runCase(1280, true,
                        14'200'000.0, 96'000.0, 14'200'000.0, 192'000.0,
                        det, avg, 0.35f, 8192, 6);
            }
        }
    }
};

QTEST_MAIN(TstSpectrumReducerWidgetParity)
#include "tst_spectrum_reducer_widget_parity.moc"
