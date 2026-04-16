// src/core/NoiseFloorEstimator.h
//
// Percentile-based noise-floor estimator for the Clarity adaptive display
// tuning feature (Phase 3G-9c). Given a frame of FFT bin magnitudes in dB,
// returns the dB value at a configured percentile (default 30th).
//
// The percentile-based approach is inherently robust against strong local
// carriers — a carrier sits above the 30th percentile cutoff without moving
// the estimate. Contrast with Thetis's processNoiseFloor() in display.cs:5866
// which uses a caller-filtered linear average; Clarity uses a percentile so
// no upstream filter is needed.
//
// Algorithm: std::nth_element partial sort on a reusable work buffer.
// O(n) average case, reused across calls so no per-frame allocation.
//
// Pure math — no Qt signals/slots, not a QObject.

#pragma once

#include <QVector>

namespace NereusSDR {

class NoiseFloorEstimator {
public:
    // percentile in [0.0, 1.0]. 0.30 = 30th percentile, the Clarity default
    // locked in 2026-04-15-display-refactor-design.md §6.2.2.
    explicit NoiseFloorEstimator(float percentile = 0.30f);

    void  setPercentile(float p);
    float percentile() const noexcept { return m_percentile; }

    // Returns the dB value at the configured percentile of `bins`.
    // `bins` is read-only — internally copied to a reusable work buffer
    // so callers can reuse their own vectors without surprise mutations.
    // Returns qQNaN() if `bins` is empty.
    float estimate(const QVector<float>& bins);

private:
    float          m_percentile;
    QVector<float> m_workBuf;
};

}  // namespace NereusSDR
