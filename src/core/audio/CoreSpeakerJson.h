// =================================================================
// src/core/audio/CoreSpeakerJson.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The text forms of the Core
// speaker's structured values on the link (R-AUD-25, R-AUD-30, D23, D31,
// settled call 15); no upstream logic.
//
// RadioModel's coreSpeakerState, coreSpeakerDevices, coreSpeakerDevice
// and coreSpeakerDetails cross the link as compact JSON text.  Each
// decoder takes exactly its key set, with the value types written below,
// and returns nullopt for anything else (an extra key, a missing one, a
// wrong type or an unknown state word).
//
//   state:   {"chosen":"...","desktop":false,"playing":"...","state":"playing"}
//            state is playing, notConnected, inUse, noCard or waitingForPick.
//   devices: [{"id":"Device,0","name":"USB Audio Device","state":"present"}]
//            state is present, notConnected or inUse.
//   device:  {"id":"...","name":"..."}; {"id":"","name":""} for the Core's
//            default, {"id":"(none)","name":""} for "(none)".
//   details: {"bufferFrames":128,"delayMs":0,"delayNowMs":12.5,
//             "negotiated":"...","sampleRate":48000}
//
// QJsonDocument writes the keys in sorted order.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 21 (R-AUD-25, R-AUD-28, R-AUD-30).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioStreamHost.h"

#include <QList>
#include <QPair>
#include <QString>

#include <optional>

namespace NereusSDR {

enum class CoreSpeakerStateKind { Playing, NotConnected, InUse, NoCard, WaitingForPick };

struct CoreSpeakerState {
    CoreSpeakerStateKind kind = CoreSpeakerStateKind::NoCard;
    QString playingName;      // the card playing now; empty when silent
    QString chosenName;       // empty for "(the Core's default)"
    bool desktop = false;     // the box starts into a desktop (R-AUD-30)
    friend bool operator==(const CoreSpeakerState&, const CoreSpeakerState&) = default;
};

struct CoreSpeakerCard {
    QString id;
    QString name;
    AudioDeviceState state = AudioDeviceState::Present;
    friend bool operator==(const CoreSpeakerCard&, const CoreSpeakerCard&) = default;
};

struct CoreSpeakerDetails {
    int bufferFrames = 0;
    int delayMs = 0;
    int sampleRate = 48000;
    QString negotiated;
    double delayNowMs = -1.0;
    friend bool operator==(const CoreSpeakerDetails&, const CoreSpeakerDetails&) = default;
};

QString coreSpeakerStateToJson(const CoreSpeakerState& state);
std::optional<CoreSpeakerState> coreSpeakerStateFromJson(const QString& json);

QString coreSpeakerDevicesToJson(const QList<CoreSpeakerCard>& cards);
std::optional<QList<CoreSpeakerCard>> coreSpeakerDevicesFromJson(const QString& json);

QString coreSpeakerDeviceToJson(const QString& id, const QString& name);
std::optional<QPair<QString, QString>> coreSpeakerDeviceFromJson(const QString& json);

QString coreSpeakerDetailsToJson(const CoreSpeakerDetails& details);
std::optional<CoreSpeakerDetails> coreSpeakerDetailsFromJson(const QString& json);

// The speakers role's status as the Core speaker reports it:
//   Playing                         -> playing on playingName
//   PlayingOnDefault, NotConnected  -> notConnected, playing the default card
//   PlayingOnDefault, InUse         -> inUse, playing the default card
//   PlayingOnDefault, no reason     -> playing
//   Silent, NotConnected            -> notConnected, playing nothing
//   Silent, InUse                   -> inUse, playing nothing
//   Silent, NoDevice or no reason   -> noCard
//   WaitingForPick                  -> waitingForPick
//   Off ("(none)" picked)           -> noCard
CoreSpeakerState coreSpeakerStateFor(const AudioRoleStatus& status, bool desktop);

} // namespace NereusSDR
