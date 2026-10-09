// =================================================================
// src/core/audio/DeviceRateMatcher.h  (NereusSDR)
// =================================================================
// Independently implemented from WDSP rmatch interface.
// no-port-check: NereusSDR-original interface.  The matching logic is
// ported in DeviceRateMatcher.cpp, which carries the upstream headers and
// cites.
//
// The clock matcher between the DSP side and one audio device (R-AUD-15,
// D2, D7, D34).  WDSP's rmatch holds a critical section on both sides;
// here the two sides share only a lock-free MatcherRing: the writer (the
// DSP side) resamples with WDSP's variable resampler and runs the control
// loop, and the reader (the device callback) copies frames, slews a dry
// run and crossfades an overrun skip.  Neither side moves the other's
// index, and the reader takes no lock, allocates nothing and makes no
// system call.
//
// The writer replays the reads it finds since its last write into the
// control, so the control sees the same sequence of read and write events
// rmatch's control() sees, measured from one thread.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 2 (R-AUD-15). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDelayParts.h"
#include "core/audio/MatcherRing.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace NereusSDR {

// The device side.  One reader per ring.  Built (and its slew buffers
// allocated) off the device thread; read() is the device callback's call.
class MatcherReader {
public:
    MatcherReader();   // defaulted in the .cpp, where State is complete
    explicit MatcherReader(MatcherRingHeader* ring);   // allocates its slew buffer here, never in read()
    MatcherReader(MatcherReader&&) noexcept;
    MatcherReader& operator=(MatcherReader&&) noexcept;
    ~MatcherReader();

    bool valid() const;
    void read(float* interleaved, int frames);         // no lock, no allocation, no system call
    void requestFadeOut();                             // any thread
    bool fadedOut() const;                             // any thread

private:
    struct State;
    std::unique_ptr<State> m_state;
};

// The DSP side.  One writer thread at a time.
class DeviceRateMatcher {
public:
    static constexpr std::array<int, 6> kDelayStepsMs{2, 3, 5, 10, 20, 40};

    struct Config {
        int inRate = 48000;
        int outRate = 48000;
        int writeBlockFrames = 64;    // xvarsamp's fixed size
        int callbackFrames = 128;     // the device callback
        int delayMs = 0;              // 0 automatic, else one of kDelayStepsMs
    };

    static std::size_t ringBytes(const Config& config);

    explicit DeviceRateMatcher(const Config& config);                        // owns its ring
    DeviceRateMatcher(const Config& config, void* memory, std::size_t bytes); // ring in the caller's memory
    ~DeviceRateMatcher();

    DeviceRateMatcher(const DeviceRateMatcher&) = delete;
    DeviceRateMatcher& operator=(const DeviceRateMatcher&) = delete;

    // False when the configuration cannot run (a rate or size out of range,
    // the caller's memory too small, or, without WDSP, inRate != outRate).
    // Callers treat an invalid matcher as an open failure.
    bool valid() const;

    void write(const float* interleavedStereo, int frames, std::int64_t nowNs); // writer thread
    void requestFlush();     // any thread; the writer drops what is queued at its next write
    void requestRestart();   // any thread; the writer restarts the control at its next write

    MatcherReader makeReader();
    MatcherRingHeader* ring();

    double ratio() const;
    int delayStepMs() const;
    double fillFrames() const;
    int resamplerDelayFrames() const;
    AudioDelayParts delayParts(double deviceBufferMs, double deviceLatencyMs) const;
    DeviceRateMatcherStats stats() const;

#ifdef NEREUS_BUILD_TESTS
    // Tests only: the matcher plays at a fixed ratio with control off.
    void forceRatioForTest(std::optional<double> ratio);
#endif

private:
    struct Writer;

    void init(const Config& config, void* memory, std::size_t bytes);

    Config m_config;
    std::unique_ptr<std::byte[]> m_ownedMemory;
    MatcherRingHeader* m_ring = nullptr;
    std::unique_ptr<Writer> m_writer;

    std::atomic<bool> m_flushRequested{false};
    std::atomic<bool> m_restartRequested{false};
    std::atomic<int> m_delayStepMs{0};
    std::atomic<bool> m_controlActive{false};
    std::atomic<bool> m_forceRatio{false};
    std::atomic<double> m_forcedRatio{1.0};
};

} // namespace NereusSDR
