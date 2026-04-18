#pragma once

// =================================================================
// src/core/AudioEngine.h  (NereusSDR)
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

#include <QObject>
#include <QString>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QByteArray>
#include <QMutex>

#include <memory>

QT_BEGIN_NAMESPACE
class QAudioSink;
class QIODevice;
class QTimer;
QT_END_NAMESPACE

namespace NereusSDR {

// Audio engine for NereusSDR.
// Receives decoded audio from WDSP and plays it through QAudioSink.
//
// Pattern from AetherSDR AudioEngine:
//   - feedAudio() appends to m_rxBuffer
//   - 10ms timer drains m_rxBuffer to QAudioSink via bytesFree()/write()
//   - Int16 stereo format (universal WASAPI/PulseAudio/CoreAudio support)
//   - Buffer capped at 200ms to prevent latency buildup
//
// Format: 48000 Hz, stereo, Int16, push mode.
class AudioEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    // Start/stop audio output.
    void start();
    void stop();
    bool isRunning() const { return m_running; }

    // Feed decoded audio from WDSP (float32 L/R → converted to int16 stereo).
    void feedAudio(int receiverId, const float* leftSamples,
                   const float* rightSamples, int sampleCount);

    // Play a 1kHz test tone to verify audio output works.
    void playTestTone(int durationMs = 2000);

    // Device management (persisted via AppSettings).
    void setOutputDevice(const QAudioDevice& device);
    QAudioDevice outputDevice() const { return m_outputDevice; }

    // Volume (0.0 to 1.0)
    void setVolume(float volume);
    float volume() const { return m_volume; }

signals:
    void outputDeviceChanged(const QAudioDevice& device);
    void volumeChanged(float volume);

private:
    void createAudioSink();
    void loadSavedDevice();

    QAudioDevice m_outputDevice;
    float m_volume{0.5f};
    bool m_running{false};

    QAudioFormat m_format;
    std::unique_ptr<QAudioSink> m_audioSink;
    QIODevice* m_audioIO{nullptr};  // owned by QAudioSink

    // Buffer + timer drain pattern (from AetherSDR).
    // m_bufferMutex guards m_rxBuffer because feedAudio() is invoked
    // from the DSP worker thread (RxDspWorker) while the rx timer drain
    // runs on this object's thread (main). Held briefly: append in
    // feedAudio, and remove/write in the timer callback.
    QByteArray m_rxBuffer;
    QTimer* m_rxTimer{nullptr};
    QMutex m_bufferMutex;
};

} // namespace NereusSDR
