// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - the GPU spectrum trace sits on its grid, with the band plan
// shown and with the 1 Hz normalization on.
//
// Ported from Longpath tests/tst_spectrum_trace_on_grid.cpp [@383f058]
// (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath, PR 100,
// commit c3ce95bf; no top-of-file GPL header upstream, the project-level
// LICENSE applies). The scenario (a flat -120 dBm trace, range -150..-60,
// 4096 bins, a 10 pt band plan) and the leading comment are Longpath's.
// Upstream measures the GPU framebuffer, which the offscreen platform
// cannot give; here the same cases are checked on the vertices
// renderGpuFrame() uploads, against dbmToYf(), which places the grid, the
// dBm scale and every overlay.
//
// Modification history (NereusSDR):
//   2026-10-22: Ported from Longpath by J.J. Boyd (KG4VCF), with
//               AI-assisted transformation via Claude Code.
//
// --- From tst_spectrum_trace_on_grid.cpp (Longpath) ---
//
// Die Kurve muss auf dem Gitter liegen -- gemessen am GPU-Framebuffer.
//
// Befund 2026-09-25 (Rendering-Nacht): Gitter, dBm-Leiste und alle
// Ueberlagerungen rechnen dBm -> Hoehe mit dbmToY/dbmToYf. Die lassen
// unten den Bandplan-Streifen frei: der Boden des Bereichs liegt an
// dessen Oberkante. Die GPU-Kurve dagegen rechnete ueber die VOLLE
// Hoehe -- mit Bandplan (beim Betreiber Schrift 6 -> 10 px) lag sie am
// Rauschboden um fast die Streifenhoehe zu tief, oben stimmte sie. Und
// die 1-Hz-Normierung (normalizeShiftDb) kam in der GPU-Kurve gar nicht
// vor, obwohl Gitter und Ueberlagerungen sie anwenden.
//
// Messung: ein flaches Spektrum genau auf einer Gitterlinie (-120 dBm,
// Spanne -150..-60, Linien alle 10 dB). Liegt die Kurve richtig, verdeckt
// sie ihre Linie, und sie steht genau einen Linienabstand unter der
// naechsten sichtbaren (-110). Liegt sie daneben, taucht die -120-Linie
// auf, oder der Abstand stimmt nicht.
// [upstream line 2, a marker exempting the file from the port checks as
//  Longpath-original, is left out: in NereusSDR this file is a registered
//  Longpath port]

#include <QtTest/QtTest>

#include <cmath>

#include "gui/SpectrumWidget.h"
#include "models/BandPlanManager.h"

using namespace NereusSDR;

namespace {

constexpr float kTraceDbm = -120.0f;
constexpr float kPeakDbm  = -100.0f;
constexpr int   kPixels   = 64;
constexpr int   kStride   = 6;   // x, y, r, g, b, a

// A vertex's y (-1 bottom, +1 top of the spectrum viewport) as a logical
// pixel row of the spectrum area. The viewport is the spectrum rect, so
// NDC +1 is its top edge and -1 its bottom edge (top + height).
double ndcToY(float ndc, const QRect& r)
{
    return r.y() + (1.0 - static_cast<double>(ndc)) * 0.5 * r.height();
}

// A trace vertex lands exactly one pixel below dbmToYf(): dbmToYf() counts
// from r.bottom() (top + height - 1), the viewport's NDC -1 is its bottom
// edge (top + height). With the same floor (the top of the band plan strip)
// and the same shift, the offset is 1 px in every row, band plan and
// normalization included; the tolerance only absorbs float rounding.
constexpr double kVertexBelowGridPx = 1.0;
constexpr double kTolerancePx = 0.05;

} // namespace

class TstSpectrumTraceOnGrid : public QObject {
    Q_OBJECT
private slots:
    void traceSitsOnItsGridLine_data()
    {
        QTest::addColumn<bool>("bandPlan");
        QTest::addColumn<bool>("normalize");
        QTest::addColumn<float>("calDb");
        QTest::newRow("no band plan") << false << false << 0.0f;
        QTest::newRow("band plan") << true << false << 0.0f;
        // GPU-Kurve und Gitter verschieben sich um dieselbe Normierung
        // (wie im CPU-Pfad). Vorher bekam nur das Gitter sie.
        // [original inline comment from Longpath tests/tst_spectrum_trace_on_grid.cpp:91-92]
        QTest::newRow("1 Hz normalization") << false << true << 0.0f;
        QTest::newRow("band plan and 1 Hz normalization") << true << true << 0.0f;
        // +15: kein Vielfaches von 10, sonst fiele das verschobene Gitter
        // wieder auf Linien.
        // [original inline comment from Longpath tests/tst_spectrum_trace_on_grid.cpp:98-99]
        QTest::newRow("calibration +15 dB") << false << false << 15.0f;
    }

    void traceSitsOnItsGridLine()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("this build has no GPU spectrum path");
#else
        QFETCH(bool, bandPlan);
        QFETCH(bool, normalize);
        QFETCH(float, calDb);

        BandPlanManager plans;
        plans.loadPlans();

        SpectrumWidget w;
        w.resize(1000, 700);
        w.setSampleRate(192'000.0);
        w.setDbmRange(-150.0f, -60.0f);
        // The 1 Hz normalization applies with Average, Sample and RMS only.
        w.setSpectrumDetector(SpectrumDetector::Average);
        w.setBandPlanFontSize(bandPlan ? 10 : 0);
        w.setBandPlanManager(bandPlan ? &plans : nullptr);
        w.setDispNormalize(normalize);
        w.setDbmCalOffset(calDb);

        const QRect r = w.spectrumRectForTest();
        QVERIFY(r.height() > 100);
        if (bandPlan) {
            // The grid's floor is above the strip, or this row proves nothing.
            QVERIFY2(w.gridYForTest(-150.0f) < r.y() + r.height() - 10.0f,
                     "band plan strip not reserved");
        }
        if (normalize) {
            // 4096 Bins: 46,875 Hz, Normierung -16,7 dB -- kein Vielfaches der
            // 10-dB-Linien (bei 2048 Bins waeren es -19,7, fast genau zwei).
            // [original inline comment from Longpath tests/tst_spectrum_trace_on_grid.cpp:134-135]
            QVERIFY(std::abs(w.binWidthHz() - 46.875) < 1e-9);
        }

        w.setRenderedPixelsForTest(QVector<float>(kPixels, kTraceDbm));
        w.setPeakHoldPixelsForTest(QVector<float>(kPixels, kPeakDbm));

        const double gridTrace = w.gridYForTest(kTraceDbm) + kVertexBelowGridPx;
        const auto v = w.fftTraceVerticesForTest();
        QCOMPARE(v.line.size(), kPixels * kStride);
        for (int j = 0; j < kPixels; ++j) {
            const double y = ndcToY(v.line[j * kStride + 1], r);
            QVERIFY2(std::abs(y - gridTrace) <= kTolerancePx,
                     qPrintable(QStringLiteral("trace pixel %1 at y=%2, expected %3 (1 px below its grid line)")
                                    .arg(j).arg(y, 0, 'f', 3).arg(gridTrace, 0, 'f', 3)));
            // The fill's lower edge still reaches the bottom of the area,
            // under the band plan strip, as the CPU path fills to its bottom.
            QCOMPARE(v.fill[(2 * j + 1) * kStride + 1], -1.0f);
        }

        const double gridPeak = w.gridYForTest(kPeakDbm) + kVertexBelowGridPx;
        const QVector<float> peak = w.fftPeakVerticesForTest();
        QCOMPARE(peak.size(), kPixels * kStride);
        for (int j = 0; j < kPixels; ++j) {
            const double y = ndcToY(peak[j * kStride + 1], r);
            QVERIFY2(std::abs(y - gridPeak) <= kTolerancePx,
                     qPrintable(QStringLiteral("peak hold pixel %1 at y=%2, expected %3 (1 px below its grid line)")
                                    .arg(j).arg(y, 0, 'f', 3).arg(gridPeak, 0, 'f', 3)));
        }
#endif
    }

    // With the normalization on, a new bin count redraws the static overlay
    // (grid and dBm scale), or the grid stays on the old bin width's shift
    // while the trace takes the new one (Longpath commit c3ce95bf).
    // Since PR 619 the static overlay is rebuilt only when something marks
    // it, so this row guards updateSpectrumLinear's check: a new bin count
    // with the normalization on marks the overlay.
    void normalizedGridRedrawsWhenBinCountChanges()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("this build has no GPU spectrum path");
#else
        SpectrumWidget w;
        w.resize(1000, 700);
        w.setDdcCenterFrequency(14'100'000.0);
        w.setSampleRate(192'000.0);
        w.setFrequencyRange(14'100'000.0, 192'000.0);
        w.setSpectrumDetector(SpectrumDetector::Average);
        w.setDispNormalize(true);

        const float level = static_cast<float>(std::pow(10.0, -12.0));
        w.updateSpectrumLinear(0, QVector<float>(4096, level), 1.0, 0.0);
        const double before = w.binWidthHz();

        w.clearOverlayStaticDirtyForTest();
        w.updateSpectrumLinear(0, QVector<float>(2048, level), 1.0, 0.0);
        QVERIFY(w.binWidthHz() != before);
        QVERIFY2(w.overlayStaticDirtyForTest(),
                 "the grid keeps the old bin width's normalization");
#endif
    }
};

QTEST_MAIN(TstSpectrumTraceOnGrid)
#include "tst_spectrum_trace_on_grid.moc"
