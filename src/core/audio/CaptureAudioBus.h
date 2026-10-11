// =================================================================
// src/core/audio/CaptureAudioBus.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Stable parent-side microphone
// reader over the clock matcher ring the nereus-audio-capture helper
// writes in shared memory; no Thetis logic.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (Ownership and interfaces).  Requirements R-R3-36, R-AUD-17.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): the reader
//               reads the helper's clock matcher ring in shared memory
//               instead of a ring fed from Pcm records.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: mic drain fix (R-AUD-17, R-R3-36): pull() is paced by
//               the 48 kHz clock, so a caller draining until it returns 0
//               ends; setClockForTest().  J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"

#include <QString>
#include <QtGlobal>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace NereusSDR {

struct MatcherRingHeader;

// Input-only reader.  One instance lives for the whole lifetime of its
// CaptureSupervisor, so consumers may keep the pointer.  The supervisor,
// not the consumer, decides when capture is open:
//   open()  returns true and changes nothing; close() changes nothing.
//   isOpen() is true only while the supervisor's current generation is
//            Ready.
//   pull()  returns Float32 mono 48 kHz bytes (whole frames only), 0 while
//           closed.  The mic is the ring's left channel.  The reader is
//           the clock matcher's output clock, so pull() is paced by 48 kHz:
//           it returns at most the frames the clock has run since the
//           previous pull, plus kPaceSlackFrames of early credit (and that
//           credit stops growing at kPaceCreditCapFrames while nobody
//           pulls).  A paced caller (the TX worker's 5 ms blocks) gets
//           every frame it asks for, the matcher filling a dry run with its
//           slew or silence; a caller that drains until pull() returns 0
//           (the remote window's uplink) gets the clock's frames and then
//           0, never an endless run of padding.  Lock-free, no allocation;
//           one caller at a time (the TX worker, or the remote window's
//           uplink, which runs with no local radio).
//   push()  returns 0 (input only).
//   txLevel() is the peak |x| of the frames the helper wrote at its latest
//             wake (the newest kLevelWindowFrames at most), 0 while closed.
//   flush() changes nothing: the clock matcher holds the ring at its
//           target fill, so nothing stale builds up.
class CaptureAudioBus final : public IAudioBus {
public:
    static constexpr int kSampleRate = 48'000;
    static constexpr int kLevelWindowFrames = 480;
    // pull()'s pacing (see above): 10 ms of slack for a caller that pulls
    // a little early, and 20 ms of credit kept across a late pull.
    static constexpr int kPaceSlackFrames = 480;
    static constexpr int kPaceCreditCapFrames = 960;
    using Clock = std::int64_t (*)();   // nanoseconds, steady

    CaptureAudioBus();
    ~CaptureAudioBus() override;
    CaptureAudioBus(const CaptureAudioBus&) = delete;
    CaptureAudioBus& operator=(const CaptureAudioBus&) = delete;

    // ── IAudioBus ───────────────────────────────────────────────────────────
    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override;
    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;
    void flush() override;
    float rxLevel() const override;
    float txLevel() const override;
    QString backendName() const override;
    AudioFormat negotiatedFormat() const override;

    // ── Owner side: CaptureSupervisor's threads, one at a time ─────────────
    // Starts reading the ring the helper built (the reader allocates here).
    // False when a ring is already attached or ring is nullptr.
    bool attachRing(MatcherRingHeader* ring);
    // Stops reading it: closes the reader, then waits for a running pull or
    // diagnostic read to leave the ring before the caller unmaps it.
    void detachRing();
    // Publishes or withdraws availability.  Withdrawing also zeroes the
    // level.
    void setAvailable(bool available);
    // The supervisor's wake thread, after each helper wake: updates the
    // level from the frames written since the last call.
    void noteWake();

    // ── Diagnostics (any thread); nullopt / 0 with no ring attached ────────
    bool ringAttached() const;
    std::optional<double> fillFrames() const;
    quint64 overruns() const;
    quint64 dryRuns() const;
    std::int64_t lastWriteNs() const;   // audioProbeNowNs()'s clock; 0 with no write

    // Tests: the clock pull() paces by (audioProbeNowNs by default).  Set
    // it before the first pull.
    void setClockForTest(Clock clock);

private:
    struct Source;

    // Runs fn(source) with the source held against detachRing(); returns
    // fallback with none attached.
    template <typename Fn, typename T>
    T withSource(Fn&& fn, T fallback) const;

    std::unique_ptr<Source> m_owned;              // owner threads only
    std::atomic<Source*> m_source{nullptr};
    mutable std::atomic<int> m_busy{0};
    std::atomic<bool> m_available{false};
    std::atomic<float> m_level{0.0f};
    std::uint64_t m_levelSeen = 0;                // wake thread only
    Clock m_clock;
};

} // namespace NereusSDR
