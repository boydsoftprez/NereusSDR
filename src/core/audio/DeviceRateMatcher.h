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
//   2026-10-09: resamplerDelayFramesFor() for the PC mic's window side.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               Native audio plan Task 13 (R-AUD-18).
//   2026-10-10: setWritePacketFrames(): a writer of whole packets (remote
//               playback) gets a size that holds one, and stats() says
//               whether one fits (R-AUD-15, bench regression). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
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

    // A writer that writes whole packets says how long one is, in input
    // frames (remote playback: 1920 for Opus, 192 for lossless); 0 is the
    // block writer the matcher is built for.  Any thread; applied at the
    // writer's next write, which then starts afresh as a restart does.
    // With a packet the size is never below a step whose ring holds the
    // packet, the device's callback and one write block, and the automatic
    // size starts at the device's callback plus one packet.  Silence after
    // a restart, a flush or a dry run then stops half a packet short of the
    // target, so the packet that follows fits.  The matcher still makes no
    // room for a burst: the writer asks stats() whether a packet fits
    // (queuedFrames + packetOutFrames at or below packetHighWaterFrames)
    // and keeps the burst until it does.
    //
    // `waited` says the packet about to be written was kept back for room
    // (it is part of a burst), so the fill it meets was set by the room
    // check and says nothing of the clocks.  The control then steers by
    // the packets that came in their own time: the fill each of them met,
    // plus half a packet, is held at the target, and the device's rate is
    // counted from one such packet to the next.  A dry run restarts a
    // packet writer's control: the source stopped, so the rates measured
    // across it are not the clocks'.  Calling it again with another value,
    // 0 included, starts the next stream from its own first size.
    static constexpr int kMaxWritePacketFrames = 65536;
    void setWritePacketFrames(int frames, bool waited = false);

    MatcherReader makeReader();
    MatcherRingHeader* ring();

    double ratio() const;
    int delayStepMs() const;
    double fillFrames() const;
    int resamplerDelayFrames() const;
    // The same delay for any rates, for a reader in another process (the
    // PC mic's window side).  0 for a rate that is not positive.
    static int resamplerDelayFramesFor(int inRate, int outRate);
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
    // The packet writer: what was asked (with a count of the changes) and
    // what the writer has applied.
    std::atomic<int> m_writePacketRequested{0};
    std::atomic<bool> m_writePacketWaited{false};
    std::atomic<std::uint32_t> m_writePacketGeneration{0};
    std::atomic<std::uint32_t> m_writePacketApplied{0};
    std::atomic<int> m_packetFrames{0};
    std::atomic<int> m_packetOutFrames{0};
    std::atomic<int> m_packetHighWaterFrames{0};
    std::atomic<bool> m_controlActive{false};
    std::atomic<bool> m_forceRatio{false};
    std::atomic<double> m_forcedRatio{1.0};
};

} // namespace NereusSDR
