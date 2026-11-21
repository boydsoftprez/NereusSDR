// src/core/audio/TxAudioRecorder.cpp  (NereusSDR)
#include "core/audio/TxAudioRecorder.h"

#include <QAudioDevice>
#include <QDateTime>
#include <QDir>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace NereusSDR {

namespace {

void putLe16(QByteArray& out, quint16 value)
{
    char bytes[2];
    qToLittleEndian(value, bytes);
    out.append(bytes, 2);
}

void putLe32(QByteArray& out, quint32 value)
{
    char bytes[4];
    qToLittleEndian(value, bytes);
    out.append(bytes, 4);
}

void putTag(QByteArray& out, const char* tag)
{
    out.append(tag, 4);
}

quint16 readLe16(const QByteArray& data, int offset)
{
    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar*>(data.constData()) + offset);
}

quint32 readLe32(const QByteArray& data, int offset)
{
    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar*>(data.constData()) + offset);
}

} // namespace

TxAudioRecorder::TxAudioRecorder(QObject* parent) : QObject(parent)
{
    m_flushTimer.setInterval(100);
    connect(&m_flushTimer, &QTimer::timeout, this, &TxAudioRecorder::flush);
}

TxAudioRecorder::~TxAudioRecorder()
{
    stop();
    stopPlayback();
}

bool TxAudioRecorder::start(int sampleRate, QString* error)
{
    if (m_recording.load(std::memory_order_acquire)) {
        return true;
    }
    if (m_file.isOpen()) {
        m_file.close();
    }

    QString root = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (root.isEmpty()) {
        root = QDir::homePath();
    }
    const QString directory = root + QStringLiteral("/NereusSDR");
    if (!QDir().mkpath(directory)) {
        if (error) { *error = tr("Could not create the recording folder: %1").arg(directory); }
        return false;
    }

    const QString stamp = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString path = directory + QStringLiteral("/tx-audio-%1.wav").arg(stamp);
    int suffix = 1;
    while (QFile::exists(path)) {
        path = directory + QStringLiteral("/tx-audio-%1-%2.wav").arg(stamp).arg(suffix++);
    }

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = tr("Could not write the recording: %1").arg(m_file.errorString());
        }
        return false;
    }

    m_sampleRate = sampleRate > 0 ? sampleRate : 48000;
    m_dataBytes = 0;
    m_pending.clear();
    writeHeader();
    m_lastFile = path;
    m_recording.store(true, std::memory_order_release);
    m_flushTimer.start();
    emit recordingChanged(true);
    return true;
}

void TxAudioRecorder::stop()
{
    if (!m_recording.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    m_flushTimer.stop();
    flush();
    if (m_file.isOpen()) {
        rewriteSizes();
        m_file.close();
    }
    emit recordingChanged(false);
}

void TxAudioRecorder::appendSamples(const float* samples, int frames)
{
    if (!m_recording.load(std::memory_order_acquire)
        || samples == nullptr || frames <= 0) {
        return;
    }
    QByteArray chunk(frames * 2, 0);
    char* bytes = chunk.data();
    for (int i = 0; i < frames; ++i) {
        const float clamped = std::clamp(samples[i], -1.0f, 1.0f);
        const qint16 value = static_cast<qint16>(
            std::lround(static_cast<double>(clamped) * 32767.0));
        qToLittleEndian(value, bytes + i * 2);
    }
    QMutexLocker lock(&m_mutex);
    m_pending.append(chunk);
}

void TxAudioRecorder::flush()
{
    QByteArray chunk;
    {
        QMutexLocker lock(&m_mutex);
        if (m_pending.isEmpty()) {
            return;
        }
        chunk.swap(m_pending);
    }
    if (!m_file.isOpen()) {
        return;
    }
    const qint64 written = m_file.write(chunk);
    if (written > 0) {
        m_dataBytes += written;
    }
}

void TxAudioRecorder::writeHeader()
{
    QByteArray header;
    putTag(header, "RIFF");
    putLe32(header, 36);            // patched on stop
    putTag(header, "WAVE");
    putTag(header, "fmt ");
    putLe32(header, 16);
    putLe16(header, 1);             // PCM
    putLe16(header, 1);             // mono
    putLe32(header, static_cast<quint32>(m_sampleRate));
    putLe32(header, static_cast<quint32>(m_sampleRate * 2));
    putLe16(header, 2);             // block align
    putLe16(header, 16);            // bits per sample
    putTag(header, "data");
    putLe32(header, 0);             // patched on stop
    m_file.write(header);
}

void TxAudioRecorder::rewriteSizes()
{
    const quint32 dataBytes = static_cast<quint32>(
        std::min<qint64>(m_dataBytes, 0xFFFFFFFFLL));
    const quint32 riffBytes = dataBytes > 0xFFFFFFFFu - 36u
                                  ? 0xFFFFFFFFu : dataBytes + 36u;
    m_file.seek(4);
    QByteArray value;
    putLe32(value, riffBytes);
    m_file.write(value);
    m_file.seek(40);
    value.clear();
    putLe32(value, dataBytes);
    m_file.write(value);
}

bool TxAudioRecorder::readWav(const QString& path, QByteArray& pcm,
                              QAudioFormat& format, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) { *error = TxAudioRecorder::tr("Could not open the recording."); }
        return false;
    }
    const QByteArray all = file.readAll();
    if (all.size() < 44 || all.mid(0, 4) != "RIFF"
        || all.mid(8, 4) != "WAVE") {
        if (error) { *error = TxAudioRecorder::tr("The recording is not a WAV file."); }
        return false;
    }

    int offset = 12;
    QAudioFormat source;
    int dataOffset = -1;
    int dataSize = 0;
    while (offset + 8 <= all.size()) {
        const QByteArray id = all.mid(offset, 4);
        const quint32 size = readLe32(all, offset + 4);
        const int body = offset + 8;
        if (id == "fmt " && body + 16 <= all.size()) {
            const quint16 audioFormat = readLe16(all, body);
            const quint16 channels = readLe16(all, body + 2);
            const quint32 rate = readLe32(all, body + 4);
            const quint16 bits = readLe16(all, body + 14);
            if (audioFormat != 1 || bits != 16 || channels < 1 || channels > 2) {
                if (error) {
                    *error = TxAudioRecorder::tr(
                        "Only 16-bit PCM mono/stereo WAV recordings can be played.");
                }
                return false;
            }
            source.setSampleRate(static_cast<int>(rate));
            source.setChannelCount(channels);
            source.setSampleFormat(QAudioFormat::Int16);
        } else if (id == "data") {
            dataOffset = body;
            dataSize = static_cast<int>(std::min<quint32>(
                size, static_cast<quint32>(all.size() - body)));
        }
        offset = body + static_cast<int>(size) + (size & 1u);
    }

    if (dataOffset < 0 || source.sampleRate() <= 0 || dataSize <= 0) {
        if (error) { *error = TxAudioRecorder::tr("The WAV file has no audio data."); }
        return false;
    }
    pcm = all.mid(dataOffset, dataSize);
    format = source;
    return true;
}

QByteArray TxAudioRecorder::convertForDevice(const QByteArray& pcm,
                                             const QAudioFormat& source,
                                             const QAudioFormat& target)
{
    const int srcCh = source.channelCount();
    const int dstCh = target.channelCount();
    const int srcRate = source.sampleRate();
    const int dstRate = target.sampleRate();
    if (srcCh <= 0 || dstCh <= 0 || srcRate <= 0 || dstRate <= 0) {
        return {};
    }
    const int srcFrames = pcm.size() / (2 * srcCh);
    if (srcFrames <= 0) {
        return {};
    }
    const int dstFrames = std::max(1, int(qint64(srcFrames) * dstRate / srcRate));
    const bool targetFloat = target.sampleFormat() == QAudioFormat::Float;
    QByteArray out;
    out.resize(dstFrames * dstCh * (targetFloat ? 4 : 2));
    auto sourceSample = [&pcm, srcCh](int frame, int channel) -> float {
        const int index = frame * srcCh + std::clamp(channel, 0, srcCh - 1);
        const qint16 value = qFromLittleEndian<qint16>(
            reinterpret_cast<const uchar*>(pcm.constData()) + index * 2);
        return static_cast<float>(value) / 32768.0f;
    };

    for (int frame = 0; frame < dstFrames; ++frame) {
        const double sourcePosition =
            static_cast<double>(frame) * srcRate / dstRate;
        const int base = std::min(int(sourcePosition), srcFrames - 1);
        const int next = std::min(base + 1, srcFrames - 1);
        const float fraction = static_cast<float>(sourcePosition - base);
        for (int channel = 0; channel < dstCh; ++channel) {
            float sample = 0.0f;
            if (srcCh == 1) {
                sample = sourceSample(base, 0) * (1.0f - fraction)
                    + sourceSample(next, 0) * fraction;
            } else if (dstCh == 1) {
                sample = 0.5f * ((sourceSample(base, 0) * (1.0f - fraction)
                                  + sourceSample(next, 0) * fraction)
                                 + (sourceSample(base, 1) * (1.0f - fraction)
                                    + sourceSample(next, 1) * fraction));
            } else {
                const int srcChannel = channel % srcCh;
                sample = sourceSample(base, srcChannel) * (1.0f - fraction)
                    + sourceSample(next, srcChannel) * fraction;
            }
            const int outIndex = frame * dstCh + channel;
            if (targetFloat) {
                const float value = std::clamp(sample, -1.0f, 1.0f);
                std::memcpy(out.data() + outIndex * 4, &value, 4);
            } else {
                const qint16 value = static_cast<qint16>(
                    std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0));
                qToLittleEndian(value, out.data() + outIndex * 2);
            }
        }
    }
    return out;
}

bool TxAudioRecorder::playLast(QString* error)
{
    if (m_lastFile.isEmpty() || !QFile::exists(m_lastFile)) {
        if (error) { *error = tr("There is no TX audio recording to play yet."); }
        return false;
    }
    QByteArray pcm;
    QAudioFormat source;
    if (!readWav(m_lastFile, pcm, source, error)) {
        return false;
    }

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        if (error) { *error = tr("No default audio output is available."); }
        return false;
    }
    QAudioFormat target = source;
    if (!device.isFormatSupported(target)) {
        target = device.preferredFormat();
        if (target.sampleFormat() != QAudioFormat::Float
            && target.sampleFormat() != QAudioFormat::Int16) {
            target.setSampleFormat(QAudioFormat::Int16);
        }
        if (!device.isFormatSupported(target)) {
            if (error) {
                *error = tr("The default audio output cannot play this recording.");
            }
            return false;
        }
    }

    const QByteArray playable =
        (target == source) ? pcm : convertForDevice(pcm, source, target);
    if (playable.isEmpty()) {
        if (error) { *error = tr("The recording could not be converted for playback."); }
        return false;
    }

    stopPlayback();
    m_buffer = std::make_unique<QBuffer>();
    m_buffer->setData(playable);
    m_buffer->open(QIODevice::ReadOnly);
    m_sink = std::make_unique<QAudioSink>(device, target);
    connect(m_sink.get(), &QAudioSink::stateChanged,
            this, &TxAudioRecorder::onSinkStateChanged);
    m_sink->start(m_buffer.get());
    if (m_sink->error() != QAudio::NoError) {
        if (error) {
            *error = tr("The audio output could not start: %1")
                         .arg(m_sink->error() == QAudio::OpenError
                                  ? tr("device open failed")
                                  : tr("device I/O error"));
        }
        stopPlayback();
        return false;
    }
    emit playbackChanged(true);
    return true;
}

void TxAudioRecorder::stopPlayback()
{
    if (!m_sink) {
        return;
    }
    m_sink->stop();
    m_sink.reset();
    m_buffer.reset();
    emit playbackChanged(false);
}

void TxAudioRecorder::onSinkStateChanged(QAudio::State state)
{
    if (state == QAudio::IdleState || state == QAudio::StoppedState) {
        // Defer: QAudioSink is inside its own state callback.
        QMetaObject::invokeMethod(this, [this] { stopPlayback(); },
                                  Qt::QueuedConnection);
    }
}

} // namespace NereusSDR
