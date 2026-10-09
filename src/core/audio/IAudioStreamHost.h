// =================================================================
// src/core/audio/IAudioStreamHost.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The roles the stream supervisor
// looks after, the states it reports and the host that opens and closes
// each role's stream (R-AUD-08 to R-AUD-14); no upstream logic.
//
// The host (AudioEngine) is called on the main thread only.  openRole()
// opens or reopens the role's stream; a stream already open for the role
// is replaced only when the new one opens, so a failed open leaves the
// role's current stream as it was.  A Pending result (the PC mic, opened
// by the capture helper) is completed later through
// AudioStreamSupervisor::onOpenFinished().  closeRole() closes whatever
// the role has open; it is harmless when nothing is open.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 5 (R-AUD-08 to R-AUD-14). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/AudioDeviceConfig.h"
#include "core/audio/AudioDeviceTypes.h"

#include <QMetaType>
#include <QString>

#include <optional>

namespace NereusSDR {

enum class AudioRole { Speakers, Headphones, TxInput, Vax1, Vax2, Vax3, Vax4 };

inline constexpr int kAudioRoleCount = 7;

enum class AudioRoleState { Playing, PlayingOnDefault, Silent, WaitingForPick, Off };

enum class AudioRoleReason { None, NotConnected, InUse, NoDevice };

struct AudioRoleStatus {
    AudioRoleState state = AudioRoleState::Off;
    AudioRoleReason reason = AudioRoleReason::None;
    AudioDeviceConfig chosen;
    QString chosenName;          // empty for the platform default
    QString playingName;         // the device in use now; empty when silent or off
    bool playingBluetooth = false;
    friend bool operator==(const AudioRoleStatus&, const AudioRoleStatus&) = default;
};

enum class AudioOpenResult { Opened, Pending, InUse, NotFound, Failed };

class IAudioStreamHost {                       // main thread
public:
    virtual ~IAudioStreamHost() = default;
    // device nullopt: the engine's system default.
    virtual AudioOpenResult openRole(AudioRole role, AudioEngineKind engine,
                                     const std::optional<AudioDeviceInfo>& device,
                                     const AudioDeviceConfig& config) = 0;
    virtual void closeRole(AudioRole role) = 0;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::AudioRole)
Q_DECLARE_METATYPE(NereusSDR::AudioRoleStatus)
