// SPDX-License-Identifier: GPL-3.0-or-later
// NereusSDR — Copyright (C) 2026 JJ Boyd / KG4VCF
//
// Sub-epic E: waterfall scrollback unit tests. NereusSDR-original.

#include <QtTest/QtTest>
#include <QImage>
#include <QVector>

// We test the math helpers in isolation by re-implementing them as a parallel
// shim. This avoids needing a full QApplication + GPU widget for what are
// pure-arithmetic checks. The production code lives in SpectrumWidget — when
// modifying the formulas, update both copies (CI ensures parity via the
// kMaxWaterfallHistoryRows constant pulled directly from the production
// header). All cap-dependent test values reference kMaxRows directly so a
// future cap change requires zero test edits.

#include "../src/gui/SpectrumWidget.h"

namespace {

constexpr int kMaxRows = NereusSDR::SpectrumWidget::kMaxWaterfallHistoryRows;
constexpr qint64 k20MinMs = 20LL * 60 * 1000;

// Mirrors SpectrumWidget::waterfallHistoryCapacityRows() — pure arithmetic.
int capacityRows(qint64 depthMs, int periodMs) {
    const int p = std::max(1, periodMs);
    const int rows = static_cast<int>((depthMs + p - 1) / p);
    return std::min(rows, kMaxRows);
}

// Mirrors SpectrumWidget::historyRowIndexForAge() with explicit args.
int rowIndexForAge(int writeRow, int rowCount, int ringHeight, int ageRows) {
    if (ringHeight <= 0 || ageRows < 0 || ageRows >= rowCount) return -1;
    return (writeRow + ageRows) % ringHeight;
}

} // namespace

class TstWaterfallScrollback : public QObject
{
    Q_OBJECT
private slots:
    // ── Capacity / cap arithmetic ──────────────────────────────────────────
    //
    // Cap break-even at 20 min depth = 1 200 000 / kMaxRows. With
    // kMaxRows = 16 384, that's ~73 ms. Periods <= 73 ms are cap-bound;
    // periods > 73 ms are depth-bound.

    void capacityCappedAtMaxRows_30msPeriod_20minDepth() {
        // 20 min × 60 000 / 30 = 40 000 → clamped to kMaxRows.
        QCOMPARE(capacityRows(k20MinMs, 30), kMaxRows);
    }
    void capacityUncapped_100msPeriod_20minDepth() {
        // 20 min × 60 000 / 100 = 12 000 → uncapped (under kMaxRows = 16 384).
        QCOMPARE(capacityRows(k20MinMs, 100), 12000);
    }
    void capacityUncapped_500msPeriod_20minDepth() {
        // 20 min × 60 000 / 500 = 2 400 → uncapped.
        QCOMPARE(capacityRows(k20MinMs, 500), 2400);
    }
    void capacityUncapped_1000msPeriod_20minDepth() {
        QCOMPARE(capacityRows(k20MinMs, 1000), 1200);
    }
    void capacityRespects60sDepth() {
        // 60 s depth at 30 ms = 2 000 rows — well under any reasonable cap.
        QCOMPARE(capacityRows(60 * 1000, 30), 2000);
    }
    void capacityZeroPeriodGuard() {
        // 0 period clamped to 1 — must not divide by zero, then capped.
        QCOMPARE(capacityRows(k20MinMs, 0), kMaxRows);
    }

    // ── Row-index / age math ───────────────────────────────────────────────

    void rowIndexForAge_age0_isWriteRow() {
        QCOMPARE(rowIndexForAge(/*writeRow=*/100, /*rowCount=*/kMaxRows,
                                /*ringHeight=*/kMaxRows, /*ageRows=*/0),
                 100);
    }
    void rowIndexForAge_wraps() {
        // writeRow=kMaxRows-6, age=10 → (kMaxRows-6+10) % kMaxRows = 4
        QCOMPARE(rowIndexForAge(kMaxRows - 6, kMaxRows, kMaxRows, 10), 4);
    }
    void rowIndexForAge_outOfBounds_returnsMinus1() {
        QCOMPARE(rowIndexForAge(0, 100, kMaxRows, 100), -1);
        QCOMPARE(rowIndexForAge(0, 100, kMaxRows, -1), -1);
    }

    // ── Ring buffer wrap-around ────────────────────────────────────────────

    void wrapAround_writePastCapacity_keepsCapRows() {
        // Simulate kMaxRows + 616 writes into a kMaxRows-row ring; verify
        // m_wfHistoryRowCount saturates at kMaxRows and writeRow wraps.
        // We instrument via a minimal stand-in (not a real SpectrumWidget)
        // because the production appendHistoryRow needs a live QImage and
        // a parent QWidget. The contract is small enough to mirror.
        constexpr int kHeight = kMaxRows;
        constexpr int kIters = kMaxRows + 616;  // one full wrap + a known offset
        int writeRow = 0;
        int rowCount = 0;
        for (int i = 0; i < kIters; ++i) {
            writeRow = (writeRow - 1 + kHeight) % kHeight;
            if (rowCount < kHeight) { ++rowCount; }
        }
        QCOMPARE(rowCount, kMaxRows);
        // (kHeight - kIters) mod kHeight, kept positive:
        //     ((kMaxRows - (kMaxRows + 616)) mod kMaxRows + kMaxRows) mod kMaxRows
        //   = ((-616) mod kMaxRows + kMaxRows) mod kMaxRows
        //   = kMaxRows - 616
        QCOMPARE(writeRow, kMaxRows - 616);
    }
};

QTEST_MAIN(TstWaterfallScrollback)
#include "tst_waterfall_scrollback.moc"
