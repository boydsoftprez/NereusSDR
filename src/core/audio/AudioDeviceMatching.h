// =================================================================
// src/core/audio/AudioDeviceMatching.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Saved device identity matching and
// the one-time settings migration to the native engines (R-AUD-04,
// R-AUD-05); no upstream logic.
//
// Matching runs ID first, then name within the same engine only, never
// across engines (bug 1: a Windows audio choice reopening on MME).  Two
// allowances widen the name step: on Windows audio a saved name of exactly
// 31 characters (MME's cut) matches an endpoint whose name starts with it,
// and on the Linux engines a saved ALSA name ending in "(hw:C,D)" matches
// the device reporting that card and device number.
//
// Migration reads the old keys (DriverApi, DeviceName, ExclusiveMode)
// under a device prefix and writes the Engine key once, following the
// R-AUD-05 table.  The old keys are never deleted.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 4 (R-AUD-04, R-AUD-05). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/AudioDeviceConfig.h"
#include "core/audio/AudioDeviceTypes.h"

#include <QList>
#include <QString>

#include <optional>

namespace NereusSDR {

struct AudioDeviceMatch {
    AudioDeviceInfo device;
    bool byId = false;   // false: matched by name; the caller saves the ID back
};

// ID first, then name within the same engine only, with the two allowances.
// Returns nullopt for a platform-default or "(none)" config (the caller
// handles those), and when nothing in the saved engine matches.  A config
// with no Engine (migration postponed) is matched as older drivers.
std::optional<AudioDeviceMatch> matchSavedAudioDevice(const AudioDeviceConfig& saved,
                                                      const QList<AudioDeviceInfo>& candidates);

enum class AudioMigrationResult { Migrated, Unchanged, Postponed };

struct AudioMigrationContext {
    std::optional<AudioEngineKind> nativeEngine;   // R-AUD-02's first native choice here
    bool nativeRunning = false;                    // its sound server answers
    bool windows = false;                          // Windows naming rules
    // Not named "linux": GCC's GNU modes (the default here, -std=gnu++20)
    // predefine linux as a macro, so that name would not compile on Linux.
    bool onLinux = false;
    bool alsaDirectOnly = false;                   // the Linux Core (Task 12): ALSA direct is the only engine
};

// Migrates one prefix ("audio/Speakers", ...).  A prefix that already has
// Engine is never touched.
AudioMigrationResult migrateAudioDeviceKeys(const QString& prefix, const AudioMigrationContext& context);

// Speakers, Headphones, TxInput, Vax1..Vax4.
void migrateAllAudioDeviceKeys(const AudioMigrationContext& context);

} // namespace NereusSDR
