// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording file reader.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; WAV and MP3 playback files (contact
//             recorder, voice keyer). AI tooling: Anthropic Claude Code.
//
// WAV: RIFF chunks are walked in order and anything but "fmt " and "data"
// is skipped (word aligned, RIFF's pad byte), so files from other tools
// with LIST, FLLR, bext or fact chunks read. Format tags 1 (PCM), 3 (IEEE
// float) and 0xFFFE (WAVE_FORMAT_EXTENSIBLE, whose SubFormat GUID starts
// with the same tag) are read, as in Microsoft's mmreg.h. A data size of
// zero, or one running past the end of the file, means "to the end": that
// is what a recording cut short by a full disk or a crash carries.
//
// MP3: minimp3 (third_party/minimp3, CC0), the one decoder on every
// platform; decision and evidence in docs/attribution/LAME-PROVENANCE.md.
// mp3dec_ex reads the LAME Info tag and trims the encoder delay and the
// padding, and seeks to an exact sample.

#include "AudioFileReader.h"

#include "core/LogCategories.h"

#include <QByteArray>
#include <QFile>
#include <QtEndian>

#include <algorithm>
#include <cstring>

// minimp3's own configuration switches, read by its header; the
// implementation is compiled in this one translation unit.
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
// The vendored decoder is not our code: keep its size_t/int conversions out
// of our warning-free build.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4267)
#endif
#include <minimp3_ex.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace NereusSDR {

namespace {

constexpr quint16 kWavFormatPcm = 1;
constexpr quint16 kWavFormatFloat = 3;
constexpr quint16 kWavFormatExtensible = 0xFFFE;

void setError(QString* error, const QString& text)
{
    if (error != nullptr) {
        *error = text;
    }
}

class WavReader final : public AudioFileReader {
public:
    explicit WavReader(std::unique_ptr<QFile> file) : m_file(std::move(file)) {}

    bool parse(QString* error)
    {
        const QByteArray riff = m_file->read(12);
        if (riff.size() != 12 || std::memcmp(riff.constData() + 8, "WAVE", 4) != 0) {
            setError(error, QStringLiteral("Not a WAV file."));
            return false;
        }
        bool haveFmt = false;
        while (true) {
            const QByteArray chunk = m_file->read(8);
            if (chunk.size() != 8) {
                break;
            }
            const quint32 size = qFromLittleEndian<quint32>(chunk.constData() + 4);
            const qint64 bodyStart = m_file->pos();
            if (std::memcmp(chunk.constData(), "fmt ", 4) == 0) {
                const QByteArray fmt = m_file->read(std::min<qint64>(size, 40));
                if (fmt.size() < 16) {
                    break;
                }
                quint16 tag = qFromLittleEndian<quint16>(fmt.constData());
                m_channels = qFromLittleEndian<quint16>(fmt.constData() + 2);
                m_rate = int(qFromLittleEndian<quint32>(fmt.constData() + 4));
                m_blockAlign = qFromLittleEndian<quint16>(fmt.constData() + 12);
                m_bits = qFromLittleEndian<quint16>(fmt.constData() + 14);
                if (tag == kWavFormatExtensible && fmt.size() >= 26) {
                    tag = qFromLittleEndian<quint16>(fmt.constData() + 24);
                }
                m_float = tag == kWavFormatFloat;
                if ((tag != kWavFormatPcm && tag != kWavFormatFloat) || m_channels <= 0
                    || m_rate <= 0
                    || (m_float ? m_bits != 32
                                : (m_bits != 8 && m_bits != 16 && m_bits != 24 && m_bits != 32))
                    || m_blockAlign != m_channels * (m_bits / 8)) {
                    setError(error, QStringLiteral("This WAV encoding is not supported."));
                    return false;
                }
                haveFmt = true;
            } else if (std::memcmp(chunk.constData(), "data", 4) == 0) {
                if (!haveFmt) {
                    break;
                }
                m_dataStart = bodyStart;
                const qint64 available = m_file->size() - bodyStart;
                qint64 bytes = size;
                if (bytes == 0 || bytes > available) {
                    bytes = available;
                }
                m_frames = bytes / m_blockAlign;
                return m_file->seek(m_dataStart);
            }
            if (!m_file->seek(bodyStart + qint64(size) + (size & 1U))) {
                break;
            }
        }
        setError(error, QStringLiteral("The WAV file has no audio."));
        return false;
    }

    int sampleRateHz() const override { return m_rate; }
    int channels() const override { return m_channels; }
    qint64 lengthFrames() const override { return m_frames; }

    bool seekFrame(qint64 frame) override
    {
        if (frame < 0 || frame > m_frames) {
            return false;
        }
        if (!m_file->seek(m_dataStart + frame * m_blockAlign)) {
            return false;
        }
        m_position = frame;
        return true;
    }

    int read(float* interleaved, int maxFrames) override
    {
        const int frames = int(std::min<qint64>(maxFrames, m_frames - m_position));
        if (frames <= 0) {
            return 0;
        }
        m_raw.resize(qsizetype(frames) * m_blockAlign);
        const qint64 got = m_file->read(m_raw.data(), m_raw.size());
        const int gotFrames = got > 0 ? int(got / m_blockAlign) : 0;
        const int bytesPerSample = m_bits / 8;
        const char* in = m_raw.constData();
        const int samples = gotFrames * m_channels;
        for (int i = 0; i < samples; ++i) {
            const char* s = in + qsizetype(i) * bytesPerSample;
            float v = 0.0f;
            if (m_float) {
                const quint32 bits = qFromLittleEndian<quint32>(s);
                std::memcpy(&v, &bits, sizeof v);
            } else if (m_bits == 8) {
                v = (float(quint8(s[0])) - 128.0f) / 128.0f;
            } else if (m_bits == 16) {
                v = float(qFromLittleEndian<qint16>(s)) / 32768.0f;
            } else if (m_bits == 24) {
                qint32 x = qint32(quint32(quint8(s[0])) | (quint32(quint8(s[1])) << 8)
                                  | (quint32(quint8(s[2])) << 16));
                if ((x & 0x800000) != 0) {
                    x -= 0x1000000;
                }
                v = float(x) / 8388608.0f;
            } else {
                v = float(double(qFromLittleEndian<qint32>(s)) / 2147483648.0);
            }
            interleaved[i] = v;
        }
        m_position += gotFrames;
        return gotFrames;
    }

private:
    std::unique_ptr<QFile> m_file;
    QByteArray m_raw;
    qint64 m_dataStart = 0;
    qint64 m_frames = 0;
    qint64 m_position = 0;
    int m_rate = 0;
    int m_channels = 0;
    int m_bits = 0;
    int m_blockAlign = 0;
    bool m_float = false;
};

class Mp3Reader final : public AudioFileReader {
public:
    explicit Mp3Reader(std::unique_ptr<QFile> file)
        : m_file(std::move(file)), m_dec(std::make_unique<mp3dec_ex_t>())
    {
        std::memset(m_dec.get(), 0, sizeof(mp3dec_ex_t));
        m_io.read = &Mp3Reader::readCb;
        m_io.read_data = this;
        m_io.seek = &Mp3Reader::seekCb;
        m_io.seek_data = this;
    }

    ~Mp3Reader() override
    {
        if (m_opened) {
            mp3dec_ex_close(m_dec.get());
        }
    }

    bool parse(QString* error)
    {
        if (mp3dec_ex_open_cb(m_dec.get(), &m_io, MP3D_SEEK_TO_SAMPLE) != 0) {
            setError(error, QStringLiteral("Not an MP3 file, or it is damaged."));
            return false;
        }
        m_opened = true;
        if (m_dec->info.channels <= 0 || m_dec->info.hz <= 0 || m_dec->samples == 0) {
            setError(error, QStringLiteral("The MP3 file has no audio."));
            return false;
        }
        return true;
    }

    int sampleRateHz() const override { return m_dec->info.hz; }
    int channels() const override { return m_dec->info.channels; }
    qint64 lengthFrames() const override { return qint64(m_dec->samples) / m_dec->info.channels; }

    bool seekFrame(qint64 frame) override
    {
        if (frame < 0 || frame > lengthFrames()) {
            return false;
        }
        return mp3dec_ex_seek(m_dec.get(), uint64_t(frame) * uint64_t(m_dec->info.channels)) == 0;
    }

    int read(float* interleaved, int maxFrames) override
    {
        if (maxFrames <= 0) {
            return 0;
        }
        const int channels = m_dec->info.channels;
        const size_t got = mp3dec_ex_read(m_dec.get(), interleaved, size_t(maxFrames) * size_t(channels));
        return int(got / size_t(channels));
    }

private:
    static size_t readCb(void* buf, size_t size, void* user)
    {
        auto* self = static_cast<Mp3Reader*>(user);
        const qint64 got = self->m_file->read(static_cast<char*>(buf), qint64(size));
        return got > 0 ? size_t(got) : 0;
    }

    static int seekCb(uint64_t position, void* user)
    {
        auto* self = static_cast<Mp3Reader*>(user);
        return self->m_file->seek(qint64(position)) ? 0 : -1;
    }

    std::unique_ptr<QFile> m_file;
    std::unique_ptr<mp3dec_ex_t> m_dec;
    mp3dec_io_t m_io{};
    bool m_opened = false;
};

}  // namespace

std::unique_ptr<AudioFileReader> AudioFileReader::open(const QString& path, QString* error)
{
    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Cannot open %1: %2").arg(path, file->errorString()));
        return nullptr;
    }
    const QByteArray magic = file->peek(12);
    if (magic.size() >= 12 && std::memcmp(magic.constData(), "RIFF", 4) == 0
        && std::memcmp(magic.constData() + 8, "WAVE", 4) == 0) {
        auto reader = std::make_unique<WavReader>(std::move(file));
        if (!reader->parse(error)) {
            qCWarning(lcRecordings) << "cannot read" << path << (error ? *error : QString());
            return nullptr;
        }
        return reader;
    }
    auto reader = std::make_unique<Mp3Reader>(std::move(file));
    if (!reader->parse(error)) {
        qCWarning(lcRecordings) << "cannot read" << path << (error ? *error : QString());
        return nullptr;
    }
    return reader;
}

}  // namespace NereusSDR
