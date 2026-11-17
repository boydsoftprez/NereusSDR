// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/LocalMicClipRecorder.cpp  (NereusSDR)
// =================================================================
//
// See LocalMicClipRecorder.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 12.
// =================================================================

#include "gui/recordings/LocalMicClipRecorder.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "core/AudioEngine.h"
#include "core/LogCategories.h"
#include "core/Resampler.h"
#include "core/recordings/AudioFileWriter.h"

namespace NereusSDR {

namespace {

// Samples taken from the source at a time; within the resampler's block.
constexpr int kChunk = 2048;
// Room for one resampled chunk at any rate up to 4x below 48 kHz.
constexpr int kOutSlack = 4096;
// How fast the level falls back between peaks, per poll.
constexpr float kLevelFall = 0.6f;

class EngineClipSource final : public MicClipSource {
public:
    explicit EngineClipSource(AudioEngine* engine) : m_engine(engine) {}
    ~EngineClipSource() override { close(); }
    void open() override
    {
        if (m_engine != nullptr) {
            m_engine->setTxMicClipTap(true);
        }
    }
    void close() override
    {
        if (m_engine != nullptr) {
            m_engine->setTxMicClipTap(false);
        }
    }
    int read(float* dst, int n) override { return m_engine ? m_engine->drainTxMicClipTap(dst, n) : 0; }
    int sampleRateHz() const override
    {
        return m_engine ? m_engine->txMicClipTapRateHz() : LocalMicClipRecorder::kSampleRateHz;
    }

private:
    // The engine may go first at shutdown (it is the model's).
    QPointer<AudioEngine> m_engine;
};

}  // namespace

std::unique_ptr<MicClipSource> MicClipSource::forAudioEngine(AudioEngine* engine)
{
    return std::make_unique<EngineClipSource>(engine);
}

LocalMicClipRecorder::LocalMicClipRecorder(std::unique_ptr<MicClipSource> source, QObject* parent)
    : QObject(parent), m_source(std::move(source))
{
    m_timer = new QTimer(this);
    m_timer->setInterval(kPollMs);
    connect(m_timer, &QTimer::timeout, this, &LocalMicClipRecorder::pump);
    m_in.resize(static_cast<size_t>(kChunk));
}

LocalMicClipRecorder::~LocalMicClipRecorder()
{
    closeSource();
}

qint64 LocalMicClipRecorder::elapsedMs() const
{
    return m_frames * 1000 / kSampleRateHz;
}

void LocalMicClipRecorder::closeSource()
{
    m_timer->stop();
    if (m_source) {
        m_source->close();
    }
}

void LocalMicClipRecorder::start()
{
    cancel();
    if (!m_dir) {
        m_dir = std::make_unique<QTemporaryDir>();
    }
    if (!m_dir->isValid() || !m_source) {
        qCWarning(lcRecordings) << "no folder for a voice message clip";
        emit failed(cannotWriteReason());
        return;
    }
    ++m_clips;
    m_path = QDir(m_dir->path()).filePath(QStringLiteral("Voice message %1.wav").arg(m_clips));
    QString error;
    m_writer = AudioFileWriter::open(m_path, AudioFileSpec{AudioFileFormat::Wav, kSampleRateHz, 1, 0}, &error);
    if (!m_writer) {
        qCWarning(lcRecordings) << "cannot write a voice message clip at" << m_path << error;
        emit failed(cannotWriteReason());
        return;
    }
    m_frames = 0;
    m_sourceRate = 0;
    m_resampler.reset();
    m_level = 0.0f;
    m_recording = true;
    m_source->open();
    m_timer->start();
    emit levelChanged(m_level);
}

void LocalMicClipRecorder::cancel()
{
    if (!m_recording) {
        return;
    }
    m_recording = false;
    closeSource();
    m_writer.reset();
    QFile::remove(m_path);
    m_level = 0.0f;
    emit levelChanged(m_level);
}

void LocalMicClipRecorder::stop()
{
    if (!m_recording) {
        return;
    }
    pump();
    if (m_recording) {
        finish();
    }
}

void LocalMicClipRecorder::pump()
{
    if (!m_recording || !m_source) {
        return;
    }
    float peak = 0.0f;
    for (;;) {
        const int got = m_source->read(m_in.data(), kChunk);
        if (got <= 0) {
            break;
        }
        for (int i = 0; i < got; ++i) {
            peak = std::max(peak, std::abs(m_in[static_cast<size_t>(i)]));
        }
        const int rate = m_source->sampleRateHz();
        if (rate != m_sourceRate) {
            m_sourceRate = rate;
            m_resampler.reset();
            if (rate > 0 && rate != kSampleRateHz) {
                m_resampler = std::make_unique<Resampler>(static_cast<double>(rate),
                                                          static_cast<double>(kSampleRateHz), kChunk);
                m_out.resize(static_cast<size_t>(std::ceil(static_cast<double>(kChunk) * kSampleRateHz / rate))
                             + kOutSlack);
            }
        }
        if (m_resampler) {
            const int made = m_resampler->processInto(m_in.data(), got, m_out.data(), static_cast<int>(m_out.size()));
            writeSamples(m_out.data(), made);
        } else {
            writeSamples(m_in.data(), got);
        }
        if (!m_recording) {
            return;   // the 2 minutes are up
        }
    }
    m_level = std::max(peak, m_level * kLevelFall);
    emit levelChanged(m_level);
}

void LocalMicClipRecorder::writeSamples(const float* samples, int count)
{
    constexpr qint64 kMaxFrames = kMaxMs * kSampleRateHz / 1000;
    const int take = static_cast<int>(std::min<qint64>(count, kMaxFrames - m_frames));
    if (take > 0) {
        if (!m_writer->write(samples, take)) {
            qCWarning(lcRecordings) << "a voice message clip could not be written at" << m_path;
            m_recording = false;
            closeSource();
            m_writer.reset();
            QFile::remove(m_path);
            emit failed(cannotWriteReason());
            return;
        }
        m_frames += take;
    }
    if (m_frames >= kMaxFrames) {
        finish();
    }
}

void LocalMicClipRecorder::finish()
{
    m_recording = false;
    closeSource();
    const bool written = m_writer && m_writer->finish();
    m_writer.reset();
    m_level = 0.0f;
    emit levelChanged(m_level);
    if (!written) {
        qCWarning(lcRecordings) << "a voice message clip could not be finished at" << m_path;
        QFile::remove(m_path);
        emit failed(cannotWriteReason());
        return;
    }
    if (m_frames == 0) {
        QFile::remove(m_path);
        emit failed(nothingHeardReason());
        return;
    }
    emit finished(m_path, elapsedMs());
}

}  // namespace NereusSDR
