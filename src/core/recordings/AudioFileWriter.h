// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording file writer.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; WAV and MP3 recording files (contact
//             recorder, spec 6.1). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"

#include <QString>
#include <QtGlobal>

#include <memory>

class QIODevice;

namespace NereusSDR {

enum class AudioFileFormat { Wav, Mp3 };

struct AudioFileSpec {
    AudioFileFormat format;
    int sampleRateHz;
    int channels;       // 1 or 2
    int mp3KbitPerSec;  // constant bit rate, MP3 only
};

// Writes one recording. WAV is 16-bit PCM; MP3 is constant bit rate through
// LAME, with a LAME Info tag so a decoder trims the encoder's delay.
// Not thread-safe; used on the recordings worker only.
class NEREUS_CORE_EXPORT AudioFileWriter {
public:
    static std::unique_ptr<AudioFileWriter> open(const QString& path, const AudioFileSpec& spec,
                                                 QString* error);
    // The same on a device the caller opened for writing; it must be
    // seekable (the WAV header and the MP3 Info tag are written last).
    // Tests use it to stand in for a disk that refuses a write.
    static std::unique_ptr<AudioFileWriter> openDevice(std::unique_ptr<QIODevice> device,
                                                       const AudioFileSpec& spec, QString* error);
    virtual ~AudioFileWriter() = default;
    virtual bool write(const float* interleaved, int frames) = 0;   // false on a disk error
    virtual bool finish() = 0;                                      // fixes WAV header, flushes LAME
    virtual qint64 bytesWritten() const = 0;
    static bool mp3Available();                                     // false without NEREUS_HAVE_LAME
};

}  // namespace NereusSDR
