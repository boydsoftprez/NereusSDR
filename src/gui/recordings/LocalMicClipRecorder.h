#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/LocalMicClipRecorder.h  (NereusSDR)
// =================================================================
//
// Records one voice keyer message from this computer's microphone, before
// any processing, into a 48 kHz mono 16-bit WAV file on this computer
// (V3). The voice keyer panel then puts the file into a message with
// RecordingsAccess::importVoice, which sends it to the Core. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (Voice keyer sending), 6.6 and V3.
//
// The microphone is the one the transmitter reads (AudioEngine::pullTxMic
// behind CompositeTxMicRouter's MicSource::Pc). Its clip tap copies what
// the transmitter is given; nothing is taken from it, and no gain is
// applied here. A clip stops itself at 2 minutes.
//
// Everything runs on the thread that owns the recorder: a timer takes what
// the tap holds, resamples it to 48 kHz when the microphone runs at
// another rate, and writes it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 12.
// =================================================================

#include <QObject>
#include <QString>
#include <QTemporaryDir>

#include <memory>
#include <vector>

#include "gui/NereusGuiExport.h"

class QTimer;

namespace NereusSDR {

class AudioEngine;
class AudioFileWriter;
class Resampler;

/// Where a clip's sound comes from: the microphone's samples, mono, as
/// they arrive. A test gives its own.
class MicClipSource {
public:
    virtual ~MicClipSource() = default;
    /// Starts collecting; what came before is not kept.
    virtual void open() = 0;
    virtual void close() = 0;
    /// Up to `n` samples into `dst`; the number taken (0 when none wait).
    virtual int read(float* dst, int n) = 0;
    /// The rate of what read() gives.
    virtual int sampleRateHz() const = 0;

    /// This computer's microphone, through `engine`'s clip tap.
    static std::unique_ptr<MicClipSource> forAudioEngine(AudioEngine* engine);
};

class NEREUS_GUI_EXPORT LocalMicClipRecorder : public QObject {
    Q_OBJECT

public:
    static constexpr int kSampleRateHz = 48000;   // V3
    static constexpr qint64 kMaxMs = 120000;      // V3: up to 2 minutes
    static constexpr int kPollMs = 40;

    explicit LocalMicClipRecorder(std::unique_ptr<MicClipSource> source, QObject* parent = nullptr);
    ~LocalMicClipRecorder() override;

    /// Starts a new clip (an unfinished one is dropped). Emits failed()
    /// when no file can be made.
    void start();
    /// Ends the clip: finished() with its file, or failed() when nothing
    /// was heard.
    void stop();
    /// Drops the clip in progress without a file.
    void cancel();

    bool isRecording() const { return m_recording; }
    /// The clip's file: the one being written, or the last one finished.
    QString wavPath() const { return m_path; }
    /// The peak of the last samples taken, 0 to 1.
    float level() const { return m_level; }
    /// How long the clip is so far.
    qint64 elapsedMs() const;

    /// Takes what the source holds now (the timer does this on its own).
    void pumpForTest() { pump(); }

    static QString nothingHeardReason()
    {
        return QStringLiteral("No sound came from this computer's microphone.");
    }
    static QString cannotWriteReason()
    {
        return QStringLiteral("The recording cannot be kept on this computer.");
    }

signals:
    void levelChanged(float level);
    void finished(const QString& path, qint64 ms);
    void failed(const QString& reason);

private:
    void pump();
    void writeSamples(const float* samples, int count);
    void finish();
    void closeSource();

    std::unique_ptr<MicClipSource> m_source;
    std::unique_ptr<AudioFileWriter> m_writer;
    std::unique_ptr<Resampler> m_resampler;
    std::unique_ptr<QTemporaryDir> m_dir;
    QTimer* m_timer = nullptr;
    QString m_path;
    bool m_recording = false;
    qint64 m_frames = 0;   // written, at 48 kHz
    int m_sourceRate = kSampleRateHz;
    float m_level = 0.0f;
    int m_clips = 0;
    std::vector<float> m_in;
    std::vector<float> m_out;
};

}  // namespace NereusSDR
