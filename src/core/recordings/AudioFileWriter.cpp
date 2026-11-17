// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording file writer.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; WAV and MP3 recording files (contact
//             recorder, spec 6.1). AI tooling: Anthropic Claude Code.
//
// WAV layout: the canonical 44-byte RIFF/WAVE header with a PCM "fmt "
// chunk (Microsoft's WAVEFORMATEX, wFormatTag 1). The header's two sizes
// are written as zero first and fixed by finish(), or when a write fails,
// so a file cut short still reads back up to its last whole frame
// (AudioFileReader reads a zero or overlong data size as "to the end").
//
// MP3: LAME's documented calling sequence (include/lame/lame.h, LAME 3.100:
// lame_init, lame_set_*, lame_init_params, lame_encode_buffer*,
// lame_encode_flush, lame_get_lametag_frame, lame_close). The ID3 tag is
// turned off so the first frame sits at byte 0, where lame.h says the
// LAME tag frame from lame_get_lametag_frame belongs when it is rewritten
// after the stream.

#include "AudioFileWriter.h"

#include "core/LogCategories.h"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#ifdef NEREUS_HAVE_LAME
#include <lame/lame.h>
#endif

namespace NereusSDR {

namespace {

constexpr int kWavHeaderBytes = 44;
constexpr int kWavBitsPerSample = 16;
constexpr quint16 kWavFormatPcm = 1;

bool validChannels(int channels)
{
    return channels == 1 || channels == 2;
}

void setError(QString* error, const QString& text)
{
    if (error != nullptr) {
        *error = text;
    }
}

class WavWriter final : public AudioFileWriter {
public:
    WavWriter(std::unique_ptr<QIODevice> device, const AudioFileSpec& spec)
        : m_device(std::move(device)), m_spec(spec)
    {
    }

    ~WavWriter() override
    {
        if (!m_finished) {
            finish();
        }
    }

    bool start()
    {
        const QByteArray header = makeHeader(0);
        return m_device->write(header) == header.size();
    }

    bool write(const float* interleaved, int frames) override
    {
        if (m_failed || m_finished) {
            return false;
        }
        if (frames <= 0) {
            return true;
        }
        const int samples = frames * m_spec.channels;
        m_buffer.resize(qsizetype(samples) * 2);
        char* out = m_buffer.data();
        for (int i = 0; i < samples; ++i) {
            const float clamped = std::clamp(interleaved[i], -1.0f, 1.0f);
            const auto value = static_cast<qint16>(std::lrint(clamped * 32767.0f));
            qToLittleEndian<qint16>(value, out + qsizetype(i) * 2);
        }
        const qint64 wrote = m_device->write(m_buffer);
        if (wrote == m_buffer.size()) {
            m_dataBytes += wrote;
            return true;
        }
        // The disk refused part or all of it. Keep the whole frames that
        // landed, point the header at them, and stop.
        const qint64 frameBytes = qint64(m_spec.channels) * 2;
        if (wrote > 0) {
            m_dataBytes += (wrote / frameBytes) * frameBytes;
        }
        m_failed = true;
        qCWarning(lcRecordings) << "WAV write refused after" << m_dataBytes << "data bytes:"
                                << m_device->errorString();
        patchHeader();
        return false;
    }

    bool finish() override
    {
        if (m_finished) {
            return !m_failed;
        }
        m_finished = true;
        const bool headerOk = patchHeader();
        if (auto* file = qobject_cast<QFile*>(m_device.get())) {
            file->flush();
        }
        m_device->close();
        return headerOk && !m_failed;
    }

    qint64 bytesWritten() const override { return kWavHeaderBytes + m_dataBytes; }

private:
    QByteArray makeHeader(qint64 dataBytes) const
    {
        const auto blockAlign = static_cast<quint16>(m_spec.channels * kWavBitsPerSample / 8);
        const auto byteRate = static_cast<quint32>(m_spec.sampleRateHz) * blockAlign;
        const auto data32 = static_cast<quint32>(std::min<qint64>(dataBytes, 0xFFFFFFFFLL - 36));
        QByteArray h(kWavHeaderBytes, '\0');
        char* p = h.data();
        std::copy_n("RIFF", 4, p);
        qToLittleEndian<quint32>(data32 == 0 ? 0 : data32 + 36, p + 4);
        std::copy_n("WAVE", 4, p + 8);
        std::copy_n("fmt ", 4, p + 12);
        qToLittleEndian<quint32>(16, p + 16);
        qToLittleEndian<quint16>(kWavFormatPcm, p + 20);
        qToLittleEndian<quint16>(static_cast<quint16>(m_spec.channels), p + 22);
        qToLittleEndian<quint32>(static_cast<quint32>(m_spec.sampleRateHz), p + 24);
        qToLittleEndian<quint32>(byteRate, p + 28);
        qToLittleEndian<quint16>(blockAlign, p + 32);
        qToLittleEndian<quint16>(static_cast<quint16>(kWavBitsPerSample), p + 34);
        std::copy_n("data", 4, p + 36);
        qToLittleEndian<quint32>(data32, p + 40);
        return h;
    }

    bool patchHeader()
    {
        if (!m_device->isOpen() || !m_device->seek(0)) {
            return false;
        }
        const QByteArray header = makeHeader(m_dataBytes);
        const bool ok = m_device->write(header) == header.size();
        m_device->seek(kWavHeaderBytes + m_dataBytes);
        if (auto* file = qobject_cast<QFile*>(m_device.get())) {
            file->flush();
        }
        return ok;
    }

    std::unique_ptr<QIODevice> m_device;
    AudioFileSpec m_spec;
    QByteArray m_buffer;
    qint64 m_dataBytes = 0;
    bool m_failed = false;
    bool m_finished = false;
};

#ifdef NEREUS_HAVE_LAME

// The sample rates an MPEG-1, MPEG-2 or MPEG-2.5 Layer III stream can
// carry (ISO/IEC 11172-3 and 13818-3; the list LAME's own
// SmpFrqIndex accepts).
constexpr std::array<int, 9> kMp3SampleRates = {8000,  11025, 12000, 16000, 22050,
                                                 24000, 32000, 44100, 48000};

class Mp3Writer final : public AudioFileWriter {
public:
    Mp3Writer(std::unique_ptr<QIODevice> device, const AudioFileSpec& spec)
        : m_device(std::move(device)), m_spec(spec)
    {
    }

    ~Mp3Writer() override
    {
        if (!m_finished) {
            finish();
        }
        if (m_lame != nullptr) {
            lame_close(m_lame);
        }
    }

    bool start(QString* error)
    {
        if (std::find(kMp3SampleRates.begin(), kMp3SampleRates.end(), m_spec.sampleRateHz)
            == kMp3SampleRates.end()) {
            setError(error, QStringLiteral("MP3 cannot carry a %1 Hz sample rate.")
                                .arg(m_spec.sampleRateHz));
            return false;
        }
        if (m_spec.mp3KbitPerSec <= 0) {
            setError(error, QStringLiteral("The MP3 bit rate is not set."));
            return false;
        }
        m_lame = lame_init();
        if (m_lame == nullptr) {
            setError(error, QStringLiteral("The MP3 encoder could not start."));
            return false;
        }
        lame_set_in_samplerate(m_lame, m_spec.sampleRateHz);
        lame_set_out_samplerate(m_lame, m_spec.sampleRateHz);
        lame_set_num_channels(m_lame, m_spec.channels);
        lame_set_mode(m_lame, m_spec.channels == 1 ? MONO : JOINT_STEREO);
        lame_set_VBR(m_lame, vbr_off);
        lame_set_brate(m_lame, m_spec.mp3KbitPerSec);
        lame_set_bWriteVbrTag(m_lame, 1);
        lame_set_write_id3tag_automatic(m_lame, 0);
        if (lame_init_params(m_lame) < 0) {
            setError(error, QStringLiteral("The MP3 encoder refused %1 kbit/s at %2 Hz.")
                                .arg(m_spec.mp3KbitPerSec)
                                .arg(m_spec.sampleRateHz));
            return false;
        }
        return true;
    }

    bool write(const float* interleaved, int frames) override
    {
        if (m_failed || m_finished) {
            return false;
        }
        if (frames <= 0) {
            return true;
        }
        // lame.h: "mp3buf_size in bytes = 1.25*num_samples + 7200".
        m_mp3.resize(size_t(frames) + size_t(frames) / 4 + 7200);
        int encoded = 0;
        if (m_spec.channels == 1) {
            encoded = lame_encode_buffer_ieee_float(m_lame, interleaved, interleaved, frames,
                                                    m_mp3.data(), int(m_mp3.size()));
        } else {
            encoded = lame_encode_buffer_interleaved_ieee_float(
                m_lame, interleaved, frames, m_mp3.data(), int(m_mp3.size()));
        }
        if (encoded < 0) {
            m_failed = true;
            qCWarning(lcRecordings) << "MP3 encoder fails, LAME error" << encoded;
            return false;
        }
        return put(encoded);
    }

    bool finish() override
    {
        if (m_finished) {
            return !m_failed;
        }
        m_finished = true;
        bool ok = !m_failed;
        if (ok) {
            m_mp3.resize(7200);
            const int flushed = lame_encode_flush(m_lame, m_mp3.data(), int(m_mp3.size()));
            ok = flushed >= 0 && put(flushed);
        }
        if (ok) {
            // The Info tag frame goes over the placeholder LAME wrote first.
            const size_t tagBytes = lame_get_lametag_frame(m_lame, nullptr, 0);
            if (tagBytes > 0) {
                std::vector<unsigned char> tag(tagBytes);
                const size_t got = lame_get_lametag_frame(m_lame, tag.data(), tag.size());
                if (got > 0 && got <= tag.size() && m_device->seek(0)) {
                    ok = m_device->write(reinterpret_cast<const char*>(tag.data()), qint64(got))
                         == qint64(got);
                    m_device->seek(m_bytes);
                }
            }
        }
        if (auto* file = qobject_cast<QFile*>(m_device.get())) {
            file->flush();
        }
        m_device->close();
        return ok;
    }

    qint64 bytesWritten() const override { return m_bytes; }

private:
    bool put(int bytes)
    {
        if (bytes <= 0) {
            return true;
        }
        const qint64 wrote = m_device->write(reinterpret_cast<const char*>(m_mp3.data()), bytes);
        if (wrote > 0) {
            m_bytes += wrote;
        }
        if (wrote != bytes) {
            m_failed = true;
            qCWarning(lcRecordings) << "MP3 write refused after" << m_bytes << "bytes:"
                                    << m_device->errorString();
            if (auto* file = qobject_cast<QFile*>(m_device.get())) {
                file->flush();
            }
            return false;
        }
        return true;
    }

    std::unique_ptr<QIODevice> m_device;
    AudioFileSpec m_spec;
    lame_global_flags* m_lame = nullptr;
    std::vector<unsigned char> m_mp3;
    qint64 m_bytes = 0;
    bool m_failed = false;
    bool m_finished = false;
};

#endif  // NEREUS_HAVE_LAME

}  // namespace

bool AudioFileWriter::mp3Available()
{
#ifdef NEREUS_HAVE_LAME
    return true;
#else
    return false;
#endif
}

std::unique_ptr<AudioFileWriter> AudioFileWriter::open(const QString& path,
                                                       const AudioFileSpec& spec, QString* error)
{
    if (spec.format == AudioFileFormat::Mp3 && !mp3Available()) {
        // Checked before the file is made, so a fallback to WAV leaves no
        // empty .mp3 behind.
        setError(error, QStringLiteral("This Core was built without MP3 support."));
        return nullptr;
    }
    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        setError(error, QStringLiteral("Cannot create %1: %2").arg(path, file->errorString()));
        qCWarning(lcRecordings) << "cannot create" << path << file->errorString();
        return nullptr;
    }
    return openDevice(std::move(file), spec, error);
}

std::unique_ptr<AudioFileWriter> AudioFileWriter::openDevice(std::unique_ptr<QIODevice> device,
                                                             const AudioFileSpec& spec,
                                                             QString* error)
{
    if (!device || !device->isOpen() || !device->isWritable() || device->isSequential()) {
        setError(error, QStringLiteral("The recording file is not open for writing."));
        return nullptr;
    }
    if (!validChannels(spec.channels) || spec.sampleRateHz <= 0) {
        setError(error, QStringLiteral("A recording needs one or two channels and a sample rate."));
        return nullptr;
    }
    if (spec.format == AudioFileFormat::Wav) {
        auto writer = std::make_unique<WavWriter>(std::move(device), spec);
        if (!writer->start()) {
            setError(error, QStringLiteral("The disk refused the recording's first bytes."));
            return nullptr;
        }
        return writer;
    }
#ifdef NEREUS_HAVE_LAME
    auto writer = std::make_unique<Mp3Writer>(std::move(device), spec);
    if (!writer->start(error)) {
        qCWarning(lcRecordings) << "MP3 encoder fails to start:" << (error ? *error : QString());
        return nullptr;
    }
    return writer;
#else
    setError(error, QStringLiteral("This Core was built without MP3 support."));
    return nullptr;
#endif
}

}  // namespace NereusSDR
