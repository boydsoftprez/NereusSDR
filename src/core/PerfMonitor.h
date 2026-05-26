// =================================================================
// src/core/PerfMonitor.h  (NereusSDR-native)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//
// Thread-safe singleton perf-instrument hub.  Lives only to gather
// timing samples + counters from the various render / audio / network
// threads so an in-spectrum overlay can render a 1 Hz dashboard of
// what is actually happening when the operator sees stutter.
//
// Design rules:
//
//   * Writers are wait-free (atomic counters or short critical
//     sections on a single mutex per ring).  Instrumentation overhead
//     must be << the thing being measured -- a paint frame of 8 ms
//     should not pay 200 us to record itself.
//
//   * Readers are main-thread-only (the overlay paint) and use
//     snapshot() which atomically copies the current rings + counters
//     into a value-type.  Display code never holds a lock while
//     painting.
//
//   * The monitor itself never allocates per-sample; ring buffers are
//     fixed-size arrays sized for ~1 s at 60 fps.
//
// The overlay reads snapshot() from a 1 Hz QTimer and triggers a
// paint update; the actual render is done by SpectrumWidget in its
// paintEvent so the overlay composites correctly with the GPU path.
// =================================================================
#pragma once

#include <QMutex>
#include <QString>
#include <array>
#include <atomic>
#include <cstdint>

namespace NereusSDR {

class PerfMonitor {
public:
    static PerfMonitor& instance();

    // ── Per-frame timing writers (call from any thread) ────────────
    // All values are in milliseconds.
    void recordPaintFrame(double ms);     // called from main / paint thread
    void recordInterFrameGap(double ms);  // called from main / paint thread
    void recordFftCompute(double ms);     // called from FFTEngine's spectrum thread
    void recordOverlayRebuild(double ms); // called from main thread (overlay path)

    // ── Counter writers (atomic; safe on any thread) ───────────────
    void incAudioUnderrun();
    void incUdpDrop();

    // ── Memory-pressure setter (call from a 1 Hz timer on main) ────
    // compressing == true when macOS vm_stat shows non-zero
    // compressions / decompressions in the last sample window.
    // footprintMb is the process's RSS / phys_footprint.
    void setMemoryStats(bool compressing, double footprintMb);

    // ── Snapshot read (main thread) ────────────────────────────────
    struct Snapshot {
        // Rolling stats over the last ~1 s of samples.
        double paintMsAvg{0.0};
        double paintMsMax{0.0};
        double gapMsAvg{0.0};
        double gapMsMax{0.0};
        double fftMsAvg{0.0};
        double fftMsMax{0.0};
        double ovlyMsAvg{0.0};
        double ovlyMsMax{0.0};

        // Cumulative + delta (delta = since last snapshot read).
        // delta lets the overlay show "1 in the last second" without
        // remembering the previous total client-side.
        uint64_t audioUnderrunsTotal{0};
        uint64_t audioUnderrunsDelta{0};
        uint64_t udpDropsTotal{0};
        uint64_t udpDropsDelta{0};

        // Memory pressure.
        bool memCompressing{false};
        double memFootprintMb{0.0};

        // Sample population (how many timing samples each metric
        // contributed; if 0 the metric is N/A).
        int paintSamples{0};
        int gapSamples{0};
        int fftSamples{0};
        int ovlySamples{0};
    };

    // Snapshot is destructive on the *delta* counters (it resets them
    // to zero so the next snapshot's delta is wrt this one).  The
    // total counters keep accumulating.
    Snapshot snapshotAndClearDeltas();

    // Reset all stats (e.g. operator clicks a "reset" affordance).
    void resetAll();

private:
    PerfMonitor() = default;
    PerfMonitor(const PerfMonitor&) = delete;
    PerfMonitor& operator=(const PerfMonitor&) = delete;

    // 1 second @ 60 fps = 60 samples; rounded up to 120 for headroom
    // in case the display fps is higher than the paint cadence.
    static constexpr int kRingSize = 120;

    template <typename T>
    struct Ring {
        std::array<double, kRingSize> values{};
        int size{0};       // populated count (clamped to kRingSize)
        int head{0};       // next-write index
        mutable QMutex mtx;

        void push(double v) {
            QMutexLocker lock(&mtx);
            values[head] = v;
            head = (head + 1) % kRingSize;
            if (size < kRingSize) {
                ++size;
            }
        }
        void stats(double& avg, double& max, int& samples) const {
            QMutexLocker lock(&mtx);
            samples = size;
            if (size == 0) {
                avg = 0.0;
                max = 0.0;
                return;
            }
            double sum = 0.0;
            double mx = values[0];
            for (int i = 0; i < size; ++i) {
                sum += values[i];
                if (values[i] > mx) {
                    mx = values[i];
                }
            }
            avg = sum / size;
            max = mx;
        }
        void clear() {
            QMutexLocker lock(&mtx);
            size = 0;
            head = 0;
        }
    };

    Ring<double> m_paintRing;
    Ring<double> m_gapRing;
    Ring<double> m_fftRing;
    Ring<double> m_ovlyRing;

    std::atomic<uint64_t> m_audioUnderrunsTotal{0};
    std::atomic<uint64_t> m_audioUnderrunsDelta{0};
    std::atomic<uint64_t> m_udpDropsTotal{0};
    std::atomic<uint64_t> m_udpDropsDelta{0};

    // Memory pressure; only the main-thread poller writes, snapshot
    // reads.  Plain double + bool under a mutex.
    mutable QMutex m_memMtx;
    bool m_memCompressing{false};
    double m_memFootprintMb{0.0};
};

} // namespace NereusSDR
