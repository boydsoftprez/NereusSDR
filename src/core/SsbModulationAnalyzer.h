// =================================================================
// src/core/SsbModulationAnalyzer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// SSB modulation monitor engine. USB and LSB suppress the carrier, so
// this does not report AM percent modulation. It measures the envelope
// of a complex I/Q stream against digital full scale (linear 1.0):
//
//   envelope     = sqrt(I^2 + Q^2)
//   peak %       = envelope / full scale * 100
//   average %    = slow average of the envelope, same scale
//   crest dB     = peak-hold relative to that average
//   dBFS         = 20*log10(envelope)
//
// Two instances are owned by RadioModel: the TX I/Q that leaves WDSP,
// and the PureSignal feedback receiver. pushIq() runs on the DSP or
// connection thread. snapshot() runs on the GUI. Readings are produced
// only from samples the caller feeds, and the caller feeds them only
// while the radio is transmitting USB or LSB.
//
// Modification history (NereusSDR):
//   2026-11-04 — Created for the SSB Mod Monitor. J.J. Boyd (KG4VCF),
//                AI-assisted via xAI Grok.
// =================================================================
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NereusSDR {

class SsbModulationAnalyzer {
public:
    struct Snapshot {
        double peakPct{0.0};       // window peak, percent of full scale
        double avgPct{0.0};        // slow average, percent of full scale
        double peakHoldPct{0.0};   // peak-hold of peakPct
        double peakDbfs{-120.0};   // window peak
        double avgDbfs{-120.0};    // slow average
        double peakHoldDbfs{-120.0};
        double crestDb{0.0};       // peak-hold above the slow average
        bool   signalPresent{false};
        std::vector<float> scope;  // percent of full scale, oldest -> newest
        int    scopeRateHz{0};
        std::uint64_t framesSeen{0};
    };

    SsbModulationAnalyzer();

    void setSampleRate(int hz);
    int  sampleRate() const;

    /// Time constant of the average-envelope tracker. Default 300 ms.
    void setAverageTimeConstantMs(double ms);
    /// How long a peak is held before it decays. Default 1500 ms.
    void setPeakHoldMs(double ms);
    /// Envelope above this linear level counts as RF. Default 0.005.
    void setPresentMinimum(double linear);
    /// Scope window length in seconds. Default 0.5 s.
    void setScopeWindowSeconds(double seconds);

    /// Forget the average, peaks, hold and scope.
    void reset();

    /// Feed interleaved I/Q floats. Thread-safe.
    void pushIq(const float* interleaved, int frames);

    /// Read the current measurements. Consumes the per-window peak.
    Snapshot snapshot();

    /// Linear amplitude that is 100% and 0 dBFS.
    static constexpr double kFullScale = 1.0;

private:
    using clock = std::chrono::steady_clock;

    void rebuildScopeLocked();
    void pushScopeLocked(float pct);
    static double toPct(double env) noexcept;
    static double toDbfs(double env) noexcept;

    mutable std::mutex m_mu;

    int    m_fs{48000};
    double m_avgTcMs{300.0};
    double m_peakHoldMs{1500.0};
    double m_presentMin{0.005};
    double m_scopeSeconds{0.5};

    double m_avg{0.0};
    bool   m_avgSeeded{false};
    double m_seedSum{0.0};
    long   m_seedCount{0};

    double m_winMax{0.0};
    bool   m_winValid{false};

    double m_holdPeak{0.0};   // undecayed peak that started the hold
    double m_holdPct{0.0};    // hold after decay
    clock::time_point m_holdAt{};

    int    m_decim{6};
    int    m_decimCount{0};
    float  m_decimAcc{0.0f};
    std::vector<float> m_scope;
    std::size_t m_scopeHead{0};
    bool   m_scopeFilled{false};

    std::uint64_t m_framesSeen{0};
};

} // namespace NereusSDR
