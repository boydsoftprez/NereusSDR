// =================================================================
// src/core/AmModulationAnalyzer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// AM modulation monitor engine.  Measures positive / negative peak
// modulation percentage, carrier level and an envelope scope trace from
// a complex I/Q stream, the way a hardware AM modulation monitor (an RF
// detector feeding a peak-reading meter) does it:
//
//   envelope[n]  = sqrt(I^2 + Q^2)
//   carrier      = slow average of the envelope (the detector's DC level;
//                  AM audio is zero-mean so the envelope mean IS the carrier)
//   +peak %      = (max(envelope) / carrier - 1) * 100
//   -peak %      = (1 - min(envelope) / carrier) * 100
//
// Two instances are owned by RadioModel: one tapped on the TX I/Q that
// leaves WDSP (the exact modulation the software generates) and one fed
// from the PureSignal feedback receiver (the PA output as sampled by the
// radio).  pushIq() is called from DSP / connection threads; snapshot()
// is called from the GUI at ~30 fps.  A single mutex guards the state;
// blocks are small (64-frame TX blocks) so contention is negligible.
//
// Modification history (NereusSDR):
//   2026-09-08 — Created for the AM Mod Monitor applet (Lee, with
//                 AI-assisted implementation via Anthropic Claude Code).
// =================================================================
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NereusSDR {

class AmModulationAnalyzer {
public:
    struct Snapshot {
        double   posPeakPct{0.0};     // peak in the window since the previous snapshot
        double   negPeakPct{0.0};
        double   posHoldPct{0.0};     // peak-hold (held for peakHoldMs, then decays)
        double   negHoldPct{0.0};
        double   carrierLevel{0.0};   // linear envelope units (WDSP TX I/Q: 0..1)
        double   carrierDbfs{-120.0}; // 20*log10(carrierLevel)
        bool     carrierPresent{false};
        bool     carrierLow{false};
        bool     carrierHigh{false};
        std::vector<float> scope;     // % modulation, oldest -> newest
        int      scopeRateHz{0};
        std::uint64_t framesSeen{0};
    };

    AmModulationAnalyzer();

    void setSampleRate(int hz);
    int  sampleRate() const;

    /// Time constant of the carrier (DC) tracker.  Default 1500 ms.
    void setCarrierTimeConstantMs(double ms);
    /// How long a peak is held before it starts to decay.  Default 1500 ms.
    void setPeakHoldMs(double ms);
    /// Carrier presence / sanity thresholds in linear envelope units.
    /// Defaults: present > 0.005, low < 0.05, high > 0.98.
    void setCarrierThresholds(double presentMin, double lowBelow, double highAbove);
    /// Scope window length in seconds (default 0.5 s).
    void setScopeWindowSeconds(double seconds);

    /// Forget carrier estimate, peaks, hold and scope.  Call at MOX-on.
    void reset();

    /// Feed interleaved I/Q floats.  Thread-safe.
    void pushIq(const float* interleaved, int frames);

    /// Read the current measurements.  Consumes the per-window peaks.
    Snapshot snapshot();

private:
    using clock = std::chrono::steady_clock;

    void rebuildScopeLocked();
    void pushScopeLocked(float pct);
    double modPct(double env) const noexcept;

    mutable std::mutex m_mu;

    int    m_fs{48000};
    double m_carrierTcMs{1500.0};
    double m_peakHoldMs{1500.0};
    double m_presentMin{0.005};
    double m_lowBelow{0.05};
    double m_highAbove{0.98};
    double m_scopeSeconds{0.5};

    // carrier tracker.  Seeded from a plain average over the first
    // ~100 ms after reset (a single 64-sample block of a low-frequency
    // tone is a fraction of a cycle and would bias the reference for the
    // whole time constant), then tracked with an exponential average.
    double m_carrier{0.0};
    bool   m_carrierSeeded{false};
    double m_seedSum{0.0};
    long   m_seedCount{0};

    // per-window (since last snapshot) extremes, linear envelope
    double m_winMax{-1.0};
    double m_winMin{-1.0};
    bool   m_winValid{false};

    // peak hold (percent)
    double m_holdPos{0.0};
    double m_holdNeg{0.0};
    clock::time_point m_holdPosAt{};
    clock::time_point m_holdNegAt{};

    // scope ring buffer (percent), decimated
    int    m_decim{6};
    int    m_decimCount{0};
    float  m_decimAcc{0.0f};   // largest-magnitude sample in the current group
    std::vector<float> m_scope;
    std::size_t m_scopeHead{0};
    bool   m_scopeFilled{false};

    std::uint64_t m_framesSeen{0};
};

} // namespace NereusSDR
