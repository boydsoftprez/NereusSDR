// =================================================================
// src/core/IqLevelMonitor.h  (NereusSDR)
// =================================================================
// Measures the raw I/Q level each hardware DDC delivers, so the Core log
// shows what the receiver was fed: RMS and peak in dBFS (a complex sample of
// magnitude 1.0 is 0 dBFS), how many sample pairs were not finite, and
// whether an ADC reported an overload. ReceiverManager::feedIqData feeds it
// and logs one line per DDC at most every kReportIntervalNs.
//
// Threading: accumulate() and takeIfDue() run on the thread that calls
// feedIqData (the connection thread), under ReceiverManager's routing lock.
// noteAdcOverflow() may run on any thread (an atomic bit mask).
//
// no-port-check: NereusSDR-original diagnostics; no Thetis counterpart.
//
// Modification history (NereusSDR):
//   2026-10-22 - Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code (diagnostics for the STRIX Core hot noise
//                 floor on slice A).
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"

#include <QList>
#include <QtGlobal>

#include <array>
#include <atomic>
#include <optional>

namespace NereusSDR {

class NEREUS_CORE_EXPORT IqLevelMonitor {
public:
    // At most one report per this interval (10 s).
    static constexpr qint64 kReportIntervalNs = 10'000'000'000LL;
    // Hardware DDC indices 0..kMaxDdc-1 are measured; others are ignored.
    static constexpr int kMaxDdc = 8;
    // Level reported for a DDC whose samples were all zero (no -inf in logs).
    static constexpr double kSilentDbfs = -200.0;

    struct DdcLevel {
        int ddc{-1};
        qint64 pairs{0};       // finite sample pairs measured
        qint64 nonFinite{0};   // pairs with a NaN or infinite I or Q
        double rmsDbfs{kSilentDbfs};
        double peakDbfs{kSilentDbfs};
    };

    struct Report {
        QList<DdcLevel> levels;   // DDCs that delivered samples, by index
        quint32 overloadMask{0};  // bit n: ADC n reported an overload
    };

    // Adds one batch of interleaved I/Q floats (I0, Q0, I1, Q1, ...).
    void accumulate(int ddc, const float* interleaved, qsizetype floatCount);

    // Records an ADC overload report until the next report (any thread).
    void noteAdcOverflow(int adc);

    // The first call starts the interval and returns nothing. Later calls
    // return the interval's levels once kReportIntervalNs (or the test
    // interval) has passed since the interval started, and start the next.
    std::optional<Report> takeIfDue(qint64 nowNs);

    // Test seam: a shorter interval (0 reports on every call after the first).
    void setReportIntervalNsForTest(qint64 intervalNs) { m_intervalNs = intervalNs; }

private:
    struct Sums {
        double sumSquares{0.0};
        double peakSquare{0.0};
        qint64 pairs{0};
        qint64 nonFinite{0};
    };

    std::array<Sums, kMaxDdc> m_sums{};
    std::atomic<quint32> m_overloadMask{0};
    qint64 m_intervalStartNs{0};
    bool m_started{false};
    qint64 m_intervalNs{kReportIntervalNs};
};

} // namespace NereusSDR
