#pragma once

// =================================================================
// src/gui/setup/AudioDriverList.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The Setup device cards' Driver
// list, Device list, state notes, delay readout and Rescan note, as pure
// functions over the device catalogue's data so they test without
// widgets.
//
// Design spec: docs/architecture/2026-10-08-native-audio-engines-design.md
// (R-AUD-01, R-AUD-03, R-AUD-06, R-AUD-08 to R-AUD-11, R-AUD-14,
// R-AUD-15, R-AUD-16, D10).
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-06,
//               R-AUD-08 to R-AUD-11, R-AUD-14, R-AUD-15, R-AUD-16, D10).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/AudioDeviceConfig.h"
#include "core/audio/AudioDelayParts.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioStreamHost.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace NereusSDR {

class IAudioDeviceCatalog;

struct AudioDriverEntry {
    std::optional<AudioEngineKind> engine;   // nullopt: a heading row ("Older drivers")
    QString hostApi;                          // older drivers: the PortAudio host API name
    QString label;                            // "Windows audio, shared", "MME", "PipeWire (not running)"
    bool enabled = true;
    QString disabledReason;
};

// R-AUD-01 order for this system, from the catalogue's backends and their
// running flags.
QList<AudioDriverEntry> audioDriverEntries(const IAudioDeviceCatalog& catalogue, bool daemonCore);

struct AudioDeviceEntry {
    QString deviceId;          // empty: "(platform default)"
    QString label;             // audioDeviceEntryLabel(), or "(platform default)", or "<name> (not connected)"
    QString group;             // the interface name when it has pairs, else empty
    AudioChannelPair pair;
    AudioDeviceState state = AudioDeviceState::Present;
    bool bluetooth = false;
};

// The Device list for one driver and direction: "(platform default)" first,
// then each device (pairs grouped), then the saved choice as
// "<name> (not connected)" when it is missing.
QList<AudioDeviceEntry> audioDeviceEntries(const IAudioDeviceCatalog& catalogue,
                                           AudioEngineKind engine, const QString& hostApi,
                                           AudioDeviceDirection direction,
                                           const AudioDeviceConfig& saved);

// The R-AUD-08 to R-AUD-11 sentence, empty when playing.
QString audioRoleNote(AudioRole role, const AudioRoleStatus& status);

// "Now <ms> ms from the radio to <device>" (outputs), "Now <ms> ms from
// <device> to the radio" (the mic), "Now -- ms" with no total or no device.
QString audioDelayLine(AudioRole role, const AudioDelayParts& parts, const QString& deviceName);

// R-AUD-06.
QString rescanNote(const IAudioDeviceCatalog& catalogue);

// ── Shared by the cards and the Sound system line ───────────────────────
// The short name an older driver's host API shows under "Older drivers":
// "MME", "DirectSound", "WDM-KS", "JACK" or "ALSA".
QString olderDriverDisplayName(const QString& hostApi);

// R-AUD-14: the Microphone page's note for a Bluetooth mic.
QString bluetoothMicNote(const QString& name);

// True when the catalogue lists only Core Audio (the Mac), so Rescan
// devices has nothing to do.
bool rescanHasNothingToDo(const IAudioDeviceCatalog& catalogue);

// R-AUD-01: the words after "Sound system:" for the catalogue's system:
// "Windows audio (WASAPI)" (with " and ASIO (<asioDriver>)" while
// asioDriver is not empty), "Core Audio", the PipeWire or PulseAudio
// sentence, or "None found. ..." on Linux with neither running.  Empty
// when the catalogue does not say which system it is.
QString soundSystemDescription(const IAudioDeviceCatalog& catalogue, const QString& asioDriver);
// True on Linux with neither PipeWire nor PulseAudio running.
bool soundSystemMissing(const IAudioDeviceCatalog& catalogue);
// Appends " Older drivers in use: <names>." (names joined with ", ") to a
// description, adding the full stop it lacks; unchanged when none is.
QString withOlderDriversInUse(const QString& description, const QStringList& olderDrivers);

} // namespace NereusSDR
