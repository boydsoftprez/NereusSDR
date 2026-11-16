// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original contact recorder.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; one slice's contact recorder: the
//             receive tap, your voice from the transmit monitor, 24 kHz,
//             together or apart (spec 6.1, 6.2, V2), and the call taken
//             from Roundtable. AI tooling: Anthropic Claude Code.
#pragma once

#include "core/AudioEngine.h"
#include "core/NereusCoreExport.h"
#include "core/recordings/SpscFloatRing.h"

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <memory>
#include <vector>

namespace NereusSDR {

class AudioFileWriter;
class Resampler;

// The other station's call for a recording that ran from `startMs` to
// `endMs` (milliseconds since the epoch, UTC), from a Roundtable room as
// RoundtableModel::room() gives it: the station heard longest over the
// recording, counting only the part of each turn that falls inside it. A
// talker's own reported callsign beats a name listeners gave; your own call
// (compared by base callsign, so KG4VCF/P is KG4VCF) never counts. Empty
// for an empty room or when nobody is named. The base callsign is returned,
// so the call is safe in a file name.
NEREUS_CORE_EXPORT QString callForRecording(const QJsonObject& room, qint64 startMs, qint64 endMs,
                                            const QString& ownCall);

// One slice's recorder. Two sides:
//
// * The taps: consume()/skip() on the DSP thread (the slice's receive
//   audio, a SliceAudioTap) and consumeMonitor() on the transmit thread
//   (the transmit monitor, your voice as sent). Each only copies into its
//   own SpscFloatRing and returns: no lock, no allocation, no disk, no
//   encoder. A block that does not fit is dropped whole and counted.
// * The worker: begin(), drain() and finish() on the recordings worker
//   thread, which empties the rings, makes 24 kHz of each, mixes them
//   (together) or puts them left and right (apart), and writes the file.
//
// Your voice counts only while transmitting and while "keep my voice" is
// on; the receive side keeps time, so your voice is laid against the
// receive frames that arrive with it (during transmit those are the MOX
// gate's skipped frames, which arrive as silence).
class NEREUS_CORE_EXPORT ContactRecorder final : public SliceAudioTap {
public:
    static constexpr int kFileRateHz = 24000;                 // spec V2
    static constexpr int kMonoKbitPerSec = 64;                // spec V2
    static constexpr int kStereoKbitPerSec = 128;             // spec V2
    // 2.7 s of 48 kHz stereo and 2.7 s of 192 kHz mono: the worker drains
    // every 20 ms, so a ring fills only when the worker has stalled.
    static constexpr int kReceiveRingFloats = 1 << 18;
    static constexpr int kMonitorRingFloats = 1 << 19;
    // Mono frames the worker takes from a ring at once.
    static constexpr int kChunkFrames = 4096;
    // Your voice may run this far ahead of the receive frames before it is
    // written on its own (a radio whose receiver stops while keyed).
    static constexpr int kMonitorLeadFrames = kFileRateHz / 4;
    // Your voice is never held for longer than this.
    static constexpr int kMonitorMaxFrames = kFileRateHz * 2;
    // WDSP's squelches mute to a gain of exactly zero (AMSQ and SSQL
    // "muted gain" 0.0, From WDSP RXA.c:183 and RXA.c:548 in
    // third_party/wdsp/src), so a receive sample above this means the
    // squelch let something through.
    static constexpr float kSquelchOpenLevel = 1.0e-5f;

    explicit ContactRecorder(int sliceId);
    ~ContactRecorder() override;

    ContactRecorder(const ContactRecorder&) = delete;
    ContactRecorder& operator=(const ContactRecorder&) = delete;

    int sliceId() const { return m_sliceId; }

    // ---- The taps. Copy and return.
    void consume(const float* samples, int frames, int sampleRateHz) noexcept override
    {
        if (!m_capturing.load(std::memory_order_acquire) || samples == nullptr || frames <= 0) {
            return;
        }
        m_receiveRateHz.store(sampleRateHz, std::memory_order_relaxed);
        if (!m_receiveRing.tryPush(samples, frames * 2)) {
            m_receiveOverflows.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Frames the MOX gate withheld: silence, so the file keeps real time.
    void skip(int frames, int sampleRateHz) noexcept override
    {
        if (!m_capturing.load(std::memory_order_acquire) || frames <= 0) {
            return;
        }
        m_receiveRateHz.store(sampleRateHz, std::memory_order_relaxed);
        if (!m_receiveRing.tryPushZeros(frames * 2)) {
            m_receiveOverflows.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // The transmit monitor: mono at the transmit channel's output rate.
    void consumeMonitor(const float* mono, int frames, int sampleRateHz) noexcept
    {
        if (!hearingMyVoice() || mono == nullptr || frames <= 0 || sampleRateHz <= 0) {
            return;
        }
        m_monitorRateHz.store(sampleRateHz, std::memory_order_relaxed);
        if (!m_monitorRing.tryPush(mono, frames)) {
            m_monitorOverflows.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // ---- Any thread.
    void setCapturing(bool on) { m_capturing.store(on, std::memory_order_release); }
    bool capturing() const { return m_capturing.load(std::memory_order_acquire); }
    void setTransmitting(bool on) { m_transmitting.store(on, std::memory_order_release); }
    void setKeepMyVoice(bool on) { m_keepMyVoice.store(on, std::memory_order_release); }
    // Your voice is going into the recording now.
    bool hearingMyVoice() const noexcept
    {
        return m_capturing.load(std::memory_order_acquire)
            && m_transmitting.load(std::memory_order_acquire)
            && m_keepMyVoice.load(std::memory_order_acquire);
    }
    quint64 receiveOverflows() const { return m_receiveOverflows.load(std::memory_order_relaxed); }
    quint64 monitorOverflows() const { return m_monitorOverflows.load(std::memory_order_relaxed); }
    // Whether the receive side carried any sound since the last call.
    bool takeHeardSound() { return m_heardSound.exchange(false, std::memory_order_acq_rel); }

    // ---- The recordings worker.
    // Takes an open writer (24 kHz; one channel together, two apart).
    void begin(std::unique_ptr<AudioFileWriter> writer, bool sidesApart);
    bool isOpen() const { return m_writer != nullptr; }
    // Empties the rings into the file. False once the writer refuses a
    // write (a full disk): the file is finished with what was written.
    bool drain();
    // Drains, then finishes the file. False when a write or the finish
    // failed; what was written is kept either way.
    bool finish();
    bool failed() const { return m_failed; }
    qint64 framesWritten() const { return m_framesWritten; }
    qint64 lengthMs() const { return m_framesWritten * 1000 / kFileRateHz; }

private:
    int resampleInto(std::unique_ptr<Resampler>& resampler, int& resamplerRate, int rateHz,
                     const float* in, int frames);
    bool writeFrames(const float* receive, int frames);
    bool fail();

    const int m_sliceId;

    // Tap side.
    std::atomic<bool> m_capturing{false};
    std::atomic<bool> m_transmitting{false};
    std::atomic<bool> m_keepMyVoice{true};
    std::atomic<int> m_receiveRateHz{AudioEngine::kMasterMixSampleRateHz};
    std::atomic<int> m_monitorRateHz{AudioEngine::kMasterMixSampleRateHz};
    std::atomic<quint64> m_receiveOverflows{0};
    std::atomic<quint64> m_monitorOverflows{0};
    std::atomic<bool> m_heardSound{false};
    SpscFloatRing m_receiveRing{kReceiveRingFloats};
    SpscFloatRing m_monitorRing{kMonitorRingFloats};

    // Worker side.
    std::unique_ptr<AudioFileWriter> m_writer;
    bool m_sidesApart = false;
    bool m_failed = false;
    qint64 m_framesWritten = 0;
    std::unique_ptr<Resampler> m_receiveResampler;
    int m_receiveResamplerRate = 0;
    std::unique_ptr<Resampler> m_monitorResampler;
    int m_monitorResamplerRate = 0;
    std::vector<float> m_scratch;       // a chunk from a ring
    std::vector<float> m_mono;          // the receive chunk, mono
    std::vector<float> m_resampled;     // 24 kHz
    std::vector<float> m_monitorPending;  // your voice at 24 kHz, not yet written
    std::vector<float> m_out;           // interleaved frames for the writer
    std::vector<float> m_silence;
};

}  // namespace NereusSDR
