// src/core/audio/TxAudioRecorder.h  (NereusSDR)
// =================================================================
// Records the processed transmit-monitor audio to a WAV file and plays it
// back on this computer's default output. Ported in spirit from Thetis's
// clsAudioRecordPlayback (Setup > Audio > Recording), narrowed to the TX
// audio check workflow: record what the TX chain sounds like, then compare
// EQ/CFC changes by playing it back.
//
// The recording tap is TxChannel::sip1OutputReady, so it captures the
// post-EQ/CFC/leveler/ALC signal regardless of whether MON itself is on.
// =================================================================
#pragma once

#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>

namespace NereusSDR {

class TxAudioRecorder : public QObject {
    Q_OBJECT
public:
    explicit TxAudioRecorder(QObject* parent = nullptr);
    ~TxAudioRecorder() override;

    bool start(int sampleRate, QString* error = nullptr);
    void stop();
    bool isRecording() const { return m_recording.load(std::memory_order_acquire); }
    QString lastFile() const { return m_lastFile; }

    bool playLast(QString* error = nullptr);
    void stopPlayback();
    bool isPlaying() const { return m_sink != nullptr; }

public slots:
    // DirectConnection from TxChannel::sip1OutputReady: copy only, under a
    // short mutex; the file write happens on the GUI thread's flush timer.
    void appendSamples(const float* samples, int frames);

signals:
    void recordingChanged(bool on);
    void playbackChanged(bool on);
    void error(const QString& message);

private slots:
    void flush();
    void onSinkStateChanged(QAudio::State state);

private:
    void writeHeader();
    void rewriteSizes();
    static bool readWav(const QString& path, QByteArray& pcm,
                        QAudioFormat& format, QString* error);
    static QByteArray convertForDevice(const QByteArray& pcm,
                                       const QAudioFormat& source,
                                       const QAudioFormat& target);

    QFile m_file;
    QMutex m_mutex;
    QByteArray m_pending;
    QTimer m_flushTimer;
    std::atomic<bool> m_recording{false};
    int m_sampleRate{48000};
    qint64 m_dataBytes{0};
    QString m_lastFile;

    std::unique_ptr<QAudioSink> m_sink;
    std::unique_ptr<QBuffer> m_buffer;
};

} // namespace NereusSDR
