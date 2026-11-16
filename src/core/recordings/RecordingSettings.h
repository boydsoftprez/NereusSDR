// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording settings.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording and Digital
//             Spectrum Recorder settings (spec 6.5, 6.7). AI tooling:
//             Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/AudioFileWriter.h"

#include <QString>

namespace NereusSDR {

// Core AppSettings keys of spec 6.5 and 6.7. Core-owned: load() and save()
// use the Core's own AppSettings; a remote window never stores them.
struct NEREUS_CORE_EXPORT RecordingSettings {
    static constexpr int kQuietMinutesMin = 1;
    static constexpr int kQuietMinutesMax = 60;
    static constexpr int kDsrSizeLimitGbMin = 1;

    bool keepMyVoice = true; bool sidesApart = false;
    bool autoStartOnTransmit = false; bool autoStopWhenQuiet = true; int quietMinutes = 2;   // 1..60
    AudioFileFormat format = AudioFileFormat::Mp3; QString disk;                           // empty = Core's own disk
    bool onAirUseTxEq = false, onAirUseCompressor = false, onAirUseLeveler = false,
         onAirUseCfc = false, onAirUsePhaseRotator = false;
    bool dsrEnabled = true; QString dsrDisk; int dsrSizeLimitGb = 10;

    static RecordingSettings load();
    void save() const;
    QString recordingsFolder() const;   // <disk>/NereusSDR/Recordings, or the Core's data folder
    QString dsrFolder() const;          // <dsrDisk>/NereusSDR/Spectrum Recorder, or the Core's data folder
};

}  // namespace NereusSDR
