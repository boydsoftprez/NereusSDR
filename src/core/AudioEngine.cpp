// =================================================================
// src/core/AudioEngine.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 QAudioSink feed-and-drain pattern ported from AetherSDR
//                 `src/core/AudioEngine.{h,cpp}` (48 kHz Int16 stereo,
//                 10 ms timer drain, 200 ms buffer cap).
// =================================================================

#include "AudioEngine.h"
#include "AppSettings.h"
#include "LogCategories.h"

#include <QAudioSink>
#include <QMediaDevices>
#include <QTimer>

#include <cmath>
#include <cstring>

namespace NereusSDR {

// From AetherSDR: 48kHz stereo int16 = 192,000 bytes/sec
// 200ms cap = 38,400 bytes
static constexpr int kSampleRate = 48000;
static constexpr int kMaxBufferMs = 200;
static constexpr qsizetype kMaxBufferBytes = kSampleRate * 2 * sizeof(qint16) * kMaxBufferMs / 1000;

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
    // Int16 stereo 48kHz — universally supported by WASAPI/PulseAudio/CoreAudio.
    // From AetherSDR AudioEngine::makeFormat()
    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Int16);

    loadSavedDevice();

    // Log available devices
    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    qCInfo(lcAudio) << "Available audio output devices:";
    for (const QAudioDevice& dev : devices) {
        QString marker = (dev == m_outputDevice) ? QStringLiteral(" [SELECTED]")
                       : (dev == QMediaDevices::defaultAudioOutput()) ? QStringLiteral(" [DEFAULT]")
                       : QString();
        qCInfo(lcAudio) << "  -" << dev.description() << marker;
    }
}

AudioEngine::~AudioEngine()
{
    stop();
}

void AudioEngine::loadSavedDevice()
{
    auto& s = AppSettings::instance();
    QByteArray savedId = s.value(QStringLiteral("AudioOutputDeviceId")).toString().toUtf8();

    if (!savedId.isEmpty()) {
        const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
        for (const QAudioDevice& dev : devices) {
            if (dev.id() == savedId || dev.description() == QString::fromUtf8(savedId)) {
                m_outputDevice = dev;
                qCInfo(lcAudio) << "Restored saved audio device:" << dev.description();
                return;
            }
        }
        qCInfo(lcAudio) << "Saved audio device not found, using system default";
    }

    m_outputDevice = QMediaDevices::defaultAudioOutput();
}

void AudioEngine::start()
{
    if (m_running) {
        return;
    }

    createAudioSink();

    if (!m_audioSink) {
        qCWarning(lcAudio) << "Failed to create audio sink";
        return;
    }

    // Push mode: we write data to the QIODevice returned by start()
    m_audioIO = m_audioSink->start();
    if (!m_audioIO) {
        qCWarning(lcAudio) << "Failed to start audio sink — state:"
                           << static_cast<int>(m_audioSink->state())
                           << "error:" << static_cast<int>(m_audioSink->error());
        m_audioSink.reset();
        return;
    }

    // Timer-based drain: every 10ms, write available data to sink.
    // From AetherSDR AudioEngine RX timer pattern.
    m_rxTimer = new QTimer(this);
    m_rxTimer->setTimerType(Qt::PreciseTimer);
    m_rxTimer->setInterval(10);
    connect(m_rxTimer, &QTimer::timeout, this, [this]() {
        if (!m_audioSink || !m_audioIO || !m_audioIO->isOpen()
            || m_audioSink->state() == QAudio::StoppedState) {
            return;
        }

        // Drain step: snapshot the head of the buffer under the lock,
        // then write to the sink without the lock so feedAudio() (on
        // the DSP thread) is not blocked waiting on QIODevice I/O.
        const qsizetype avail = m_audioSink->bytesFree();
        QByteArray slice;
        {
            QMutexLocker locker(&m_bufferMutex);
            // Cap buffer at 200ms to prevent latency buildup.
            if (m_rxBuffer.size() > kMaxBufferBytes) {
                m_rxBuffer.remove(0, m_rxBuffer.size() - kMaxBufferBytes);
            }
            const qsizetype len = qMin(avail, m_rxBuffer.size());
            if (len > 0) {
                slice = QByteArray(m_rxBuffer.constData(), len);
                m_rxBuffer.remove(0, len);
            }
        }
        if (!slice.isEmpty()) {
            const qsizetype written = m_audioIO->write(slice.constData(), slice.size());
            if (written > 0 && written < slice.size()) {
                // Sink accepted less than offered — push the remainder
                // back to the front of the buffer so we don't drop it.
                QMutexLocker locker(&m_bufferMutex);
                m_rxBuffer.prepend(slice.constData() + written, slice.size() - written);
            }
        }
    });
    m_rxTimer->start();

    m_running = true;
    qCInfo(lcAudio) << "Audio output started:"
                    << m_format.sampleRate() << "Hz"
                    << m_format.channelCount() << "ch Int16"
                    << "on" << m_outputDevice.description();
}

void AudioEngine::stop()
{
    if (!m_running) {
        return;
    }

    if (m_rxTimer) {
        m_rxTimer->stop();
        delete m_rxTimer;
        m_rxTimer = nullptr;
    }

    if (m_audioSink) {
        m_audioSink->stop();
    }

    m_audioIO = nullptr;
    m_audioSink.reset();
    {
        QMutexLocker locker(&m_bufferMutex);
        m_rxBuffer.clear();
    }
    m_running = false;

    qCInfo(lcAudio) << "Audio output stopped";
}

void AudioEngine::feedAudio(int receiverId, const float* leftSamples,
                            const float* rightSamples, int sampleCount)
{
    Q_UNUSED(receiverId);

    if (!m_running) {
        return;
    }

    // Convert float32 L/R → interleaved int16 stereo (LRLRLR...)
    // Apply volume scaling during conversion.
    const int byteCount = sampleCount * 2 * sizeof(qint16);
    QByteArray pcm(byteCount, Qt::Uninitialized);
    qint16* dst = reinterpret_cast<qint16*>(pcm.data());

    const float vol = m_volume;
    for (int i = 0; i < sampleCount; ++i) {
        float l = leftSamples[i] * vol;
        float r = rightSamples[i] * vol;
        dst[i * 2]     = static_cast<qint16>(qBound(-1.0f, l, 1.0f) * 32767.0f);
        dst[i * 2 + 1] = static_cast<qint16>(qBound(-1.0f, r, 1.0f) * 32767.0f);
    }

    // Append to buffer — timer drains to QAudioSink. feedAudio() may
    // be called from the DSP worker thread, so the append must be
    // serialized against the timer drain on the main thread.
    {
        QMutexLocker locker(&m_bufferMutex);
        m_rxBuffer.append(pcm);
    }
}

void AudioEngine::playTestTone(int durationMs)
{
    if (!m_running) {
        start();
    }
    if (!m_running) {
        qCWarning(lcAudio) << "Cannot play test tone — audio not available";
        return;
    }

    qCInfo(lcAudio) << "Playing 1kHz test tone for" << durationMs << "ms";

    const int totalSamples = kSampleRate * durationMs / 1000;
    const float freq = 1000.0f;
    const float amplitude = 0.3f;

    // Generate int16 stereo PCM
    QByteArray pcm(totalSamples * 2 * sizeof(qint16), Qt::Uninitialized);
    qint16* dst = reinterpret_cast<qint16*>(pcm.data());

    for (int i = 0; i < totalSamples; ++i) {
        qint16 sample = static_cast<qint16>(
            amplitude * std::sin(2.0f * 3.14159265f * freq * i / kSampleRate) * 32767.0f);
        dst[i * 2]     = sample;
        dst[i * 2 + 1] = sample;
    }

    // Append to buffer — timer will drain it
    {
        QMutexLocker locker(&m_bufferMutex);
        m_rxBuffer.append(pcm);
    }
    qCInfo(lcAudio) << "Test tone queued:" << pcm.size() << "bytes";
}

void AudioEngine::setOutputDevice(const QAudioDevice& device)
{
    if (m_outputDevice == device) {
        return;
    }

    m_outputDevice = device;

    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("AudioOutputDeviceId"), QString::fromUtf8(device.id()));
    s.save();

    qCInfo(lcAudio) << "Audio output device changed to" << device.description();

    if (m_running) {
        stop();
        start();
    }

    emit outputDeviceChanged(device);
}

void AudioEngine::setVolume(float volume)
{
    volume = qBound(0.0f, volume, 1.0f);
    if (!qFuzzyCompare(m_volume, volume)) {
        m_volume = volume;
        emit volumeChanged(volume);
    }
}

void AudioEngine::createAudioSink()
{
    QAudioDevice device = m_outputDevice;
    if (device.isNull()) {
        device = QMediaDevices::defaultAudioOutput();
    }

    if (device.isNull()) {
        qCWarning(lcAudio) << "No audio output device available";
        return;
    }

    // From AetherSDR: on Windows, don't trust isFormatSupported().
    // Just try to open the sink — WASAPI shared mode handles conversion.
    m_audioSink = std::make_unique<QAudioSink>(device, m_format);

    qCInfo(lcAudio) << "Audio sink created on" << device.description()
                    << "format:" << m_format.sampleRate() << "Hz"
                    << m_format.channelCount() << "ch"
                    << "Int16"
                    << "buffer:" << m_audioSink->bufferSize() << "bytes";
}

} // namespace NereusSDR
