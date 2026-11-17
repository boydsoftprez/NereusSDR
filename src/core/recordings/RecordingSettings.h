// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording settings.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the Core's recording and Digital
//             Spectrum Recorder settings (spec 6.5, 6.7). AI tooling:
//             Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; the largest size limit the recorder
//             takes. AI tooling: Anthropic Claude Code.
// 2026-11-16  J.J. Boyd / KG4VCF; recording plan Task 10b: the Digital
//             Spectrum Recorder's default for a missing key depends on
//             where it runs (off in a desktop window running its own
//             radio, on in a Core). AI tooling: Anthropic Claude Code.
#pragma once

#include "core/NereusCoreExport.h"
#include "core/recordings/AudioFileWriter.h"

#include <QString>

namespace NereusSDR {

// Core AppSettings keys of spec 6.5 and 6.7. Core-owned: load() and save()
// use the Core's own AppSettings; a remote window never stores them.
struct NEREUS_CORE_EXPORT RecordingSettings {
    // Where the recordings run: a desktop window running its own radio
    // (RadioModel Role::Local in the app) or a Core (nereusd).
    enum class Host { DesktopRadio, Core };

    static constexpr int kQuietMinutesMin = 1;
    static constexpr int kQuietMinutesMax = 60;
    static constexpr int kDsrSizeLimitGbMin = 1;
    // 100 TB: far past any disk a Core has, so a typing slip is refused.
    static constexpr int kDsrSizeLimitGbMax = 100000;

    bool keepMyVoice = true; bool sidesApart = false;
    bool autoStartOnTransmit = false; bool autoStopWhenQuiet = true; int quietMinutes = 2;   // 1..60
    AudioFileFormat format = AudioFileFormat::Mp3; QString disk;                           // empty = Core's own disk
    bool onAirUseTxEq = false, onAirUseCompressor = false, onAirUseLeveler = false,
         onAirUseCfc = false, onAirUsePhaseRotator = false;
    bool dsrEnabled = true; QString dsrDisk; int dsrSizeLimitGb = 10;

    // The Digital Spectrum Recorder with no saved value: on in a new Core
    // (spec 6.7), off in a desktop window running its own radio (JJ,
    // 2026-11-16). A saved value is always kept.
    static bool dsrEnabledByDefault(Host host) { return host == Host::Core; }

    static RecordingSettings load(Host host);
    void save() const;
    QString recordingsFolder() const;   // <disk>/NereusSDR/Recordings, or the Core's data folder
    QString dsrFolder() const;          // <dsrDisk>/NereusSDR/Spectrum Recorder, or the Core's data folder
};

}  // namespace NereusSDR
