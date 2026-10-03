// =================================================================
// src/core/audio/CaptureAudioBus.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Stable parent-side microphone
// reader fed by CaptureSupervisor from the nereus-audio-capture helper's
// PCM records; no Thetis logic.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (Ownership and interfaces).  Requirement R-R3-36.
// =================================================================

#pragma once

#include "core/IAudioBus.h"

#include <QString>
#include <QtGlobal>

#include <array>
#include <atomic>

namespace NereusSDR {

// Input-only reader.  One instance lives for the whole lifetime of its
// CaptureSupervisor, so consumers may keep the pointer.  The supervisor,
// not the consumer, decides when capture is open:
//   open()  returns true and changes nothing; close() changes nothing.
//   isOpen() is true only while the supervisor's current generation is
//            Ready.
//   pull()  returns Float32 mono 48 kHz bytes (whole frames only), 0 when
//           unavailable or empty.  Lock-free; safe from the TX worker.
//   push()  returns 0 (input only).
//   txLevel() is the peak |x| of the most recent PCM record, 0 while
//             unavailable.
//
// Storage is a single-producer single-consumer ring of kRingFrames frames
// (100 ms, the existing PortAudioBus capture ring duration).  When the ring
// is full, new frames are dropped and counted; the producer never evicts
// frames the consumer may be reading.  flush() retires everything written
// so far: the consumer skips it on its next pull().
class CaptureAudioBus final : public IAudioBus {
public:
    static constexpr int kRingFrames = 4800;
    static constexpr int kSampleRate = 48'000;

    CaptureAudioBus() = default;
    CaptureAudioBus(const CaptureAudioBus&) = delete;
    CaptureAudioBus& operator=(const CaptureAudioBus&) = delete;

    // ── IAudioBus ───────────────────────────────────────────────────────────
    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override;
    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;
    void flush() override;                      // any thread
    float rxLevel() const override;
    float txLevel() const override;
    QString backendName() const override;
    AudioFormat negotiatedFormat() const override;

    // ── Producer side: CaptureSupervisor's I/O thread only ─────────────────
    // Writes as many of frameCount samples as fit and records the record's
    // peak level; returns the frames accepted.  The rest are dropped and
    // counted in droppedFrames().
    int writeFrames(const float* samples, int frameCount);
    // Publishes or withdraws availability.  Withdrawing also zeroes the
    // level and retires buffered frames.
    void setAvailable(bool available);

    // ── Diagnostics (any thread) ────────────────────────────────────────────
    quint64 droppedFrames() const;
    int bufferedFrames() const;                 // frames the next pull() could return

private:
    std::array<float, kRingFrames> m_ring{};
    alignas(64) std::atomic<quint64> m_written{0};      // producer-owned counter
    alignas(64) std::atomic<quint64> m_read{0};         // consumer-owned counter
    std::atomic<quint64> m_discardFloor{0};             // frames below this are retired
    std::atomic<bool> m_available{false};
    std::atomic<float> m_level{0.0f};
    std::atomic<quint64> m_dropped{0};
};

} // namespace NereusSDR
