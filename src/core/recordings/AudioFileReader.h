// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording file reader.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; WAV and MP3 playback files (contact
//             recorder, voice keyer). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"

#include <QString>
#include <QtGlobal>

#include <memory>

namespace NereusSDR {

// Reads a WAV (PCM 8, 16, 24 or 32 bit, or 32-bit float, plain or
// extensible) or an MP3 (minimp3, the one decoder on every platform) as
// interleaved float samples. The format is taken from the file's contents,
// not its name.
class NEREUS_CORE_EXPORT AudioFileReader {
public:
    static std::unique_ptr<AudioFileReader> open(const QString& path, QString* error);
    virtual ~AudioFileReader() = default;
    virtual int sampleRateHz() const = 0;
    virtual int channels() const = 0;
    virtual qint64 lengthFrames() const = 0;
    virtual bool seekFrame(qint64 frame) = 0;
    virtual int read(float* interleaved, int maxFrames) = 0;        // 0 at the end
};

}  // namespace NereusSDR
