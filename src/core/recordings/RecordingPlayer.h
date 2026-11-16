// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording player.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; one playback of a recording: decoded on
//             the recordings worker into a lock-free ring at 48 kHz, read
//             by the audio thread (spec 6.1 Player, 6.3).
//             AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; a saved whole-pan stretch plays its
//             sound only, as the slice was set when it was saved (spec 6.3,
//             recording plan Task 25). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/SpscFloatRing.h"
#include "core/recordings/dsr/DsrReplay.h"

#include <QString>
#include <QtGlobal>

#include <atomic>
#include <memory>
#include <vector>

namespace NereusSDR {

class AudioFileReader;
class DsrRingFile;
class Resampler;

// Who hears a playback: "local" for the window that runs its own radio,
// otherwise the session's device id.
using ListenerId = QString;

struct PlaybackState {
    ListenerId listener;
    QString recordingId;
    qint64 positionMs;
    qint64 lengthMs;
    bool playing;
    bool onAir;
};

// One playback of one file from one position. The recordings worker opens
// it and keeps it decoded ahead (fill), mono at the mixer's 48 kHz; the
// audio thread takes from it (pull) without a lock, a wait or an
// allocation. A seek is a new RecordingPlayer from the new position.
//
// Threads: open() and fill() on the worker only; pull() on one audio
// thread at a time; positionMs(), lengthMs() and finished() anywhere.
class NEREUS_CORE_EXPORT RecordingPlayer {
public:
    // The mixer's rate (MasterMixer, kBufferBaseRate).
    static constexpr int kOutputRateHz = 48000;
    // Decoded ahead of the audio thread: half a second.
    static constexpr int kFillFrames = 24000;
    // The ring: room for the fill plus a whole chunk's output beyond it.
    static constexpr int kRingFrames = 65536;
    // Frames read from the file per decode step.
    static constexpr int kChunkFrames = 1024;
    // The lowest file rate played: its chunk's 48 kHz output must fit the
    // ring's room above kFillFrames.
    static constexpr int kMinFileRateHz = 8000;

    RecordingPlayer(qint64 startMs, qint64 lengthMs);
    ~RecordingPlayer();

    RecordingPlayer(const RecordingPlayer&) = delete;
    RecordingPlayer& operator=(const RecordingPlayer&) = delete;

    // Before open(): the file is a saved whole-pan stretch (a ring file),
    // played as its sound only, demodulated as `sound` says (spec 6.3,
    // "Play its sound only").
    void setStretchSound(const DsrSoundSettings& sound);
    // Worker. Opens the file at the start position; false with a reason.
    bool open(const QString& path, QString* error);
    // Worker. Decodes until kFillFrames wait in the ring or the file ends.
    void fill();

    // Audio thread. Up to `frames` mono samples into `mono48k`; the rest of
    // the buffer is silence. Returns the frames that were the recording.
    int pull(float* mono48k, int frames) noexcept;

    // The position heard so far: the start plus what pull() handed out.
    qint64 positionMs() const noexcept;
    qint64 lengthMs() const noexcept;
    // The file is decoded to its end and the audio thread has taken it all.
    bool finished() const noexcept;

private:
    void push(const float* mono, int frames);
    bool openStretch(const QString& path, QString* error);

    const qint64 m_startMs;
    std::atomic<qint64> m_lengthMs;
    std::atomic<qint64> m_consumedFrames{0};
    std::atomic<bool> m_decodeDone{false};
    SpscFloatRing m_ring{kRingFrames};

    // Worker only.
    std::unique_ptr<AudioFileReader> m_reader;
    std::unique_ptr<Resampler> m_resampler;
    int m_channels = 1;
    int m_rateHz = kOutputRateHz;
    qint64 m_expectedFrames = 0;   // 48 kHz frames from the start to the end
    qint64 m_producedFrames = 0;
    bool m_readerDone = false;
    int m_flushLeft = 0;   // zeros still to push through the resampler at the end
    std::vector<float> m_read;
    std::vector<float> m_mono;
    std::vector<float> m_out;
    // A saved whole-pan stretch: its file and its sound (worker only).
    bool m_isStretch = false;
    DsrSoundSettings m_stretchSound;
    std::unique_ptr<DsrRingFile> m_stretchRing;
    std::unique_ptr<DsrSoundRender> m_stretchRender;
};

}  // namespace NereusSDR
