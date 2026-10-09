// =================================================================
// src/core/audio/AudioDeviceTypes.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Plain value types for the audio
// engines and the live device catalogue (R-AUD-03, R-AUD-07); no upstream
// logic.
//
// AudioEngineKind is what the operator picks (and what the Engine settings
// key stores); AudioBackendId is the code that lists and opens devices, so
// both Windows kinds share one backend.  AudioChannelPair is the pair model
// of R-AUD-07: a device with more than two channels is offered as one
// entry per pair, an odd last channel as a one-channel entry.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03, R-AUD-07). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

#include <optional>

namespace NereusSDR {

enum class AudioEngineKind {
    PortAudio,
    CoreAudio,
    WindowsShared,
    WindowsExclusive,
    Asio,
    PipeWire,
    PulseAudio,
    AlsaDirect
};

enum class AudioBackendId { PortAudio, CoreAudio, Wasapi, Asio, PipeWire, PulseAudio, AlsaDirect };

enum class AudioDeviceDirection { Output, Input };

enum class AudioTransport { Unknown, BuiltIn, Usb, Bluetooth, Hdmi, Virtual };

enum class AudioDeviceState { Present, NotConnected, InUse };

struct AudioChannelPair {
    int firstChannel = 1;   // 1-based
    int channelCount = 2;   // 2, or 1 for a one-channel device or an odd last channel
    friend bool operator==(const AudioChannelPair&, const AudioChannelPair&) = default;
};

struct AudioDeviceInfo {
    AudioBackendId backend = AudioBackendId::PortAudio;
    AudioDeviceDirection direction = AudioDeviceDirection::Output;
    QString id;          // the engine's saved identity (spec "Saved identity")
    QString name;        // display name
    QString hostApi;     // older drivers only: PortAudio's host API name
    AudioTransport transport = AudioTransport::Unknown;
    AudioDeviceState state = AudioDeviceState::Present;
    int channelCount = 2;
    int alsaCard = -1;   // where the engine reports them (Linux)
    int alsaDevice = -1;
    bool isDefault = false;
    friend bool operator==(const AudioDeviceInfo&, const AudioDeviceInfo&) = default;
};

// The Engine settings key: "PortAudio", "CoreAudio", "WindowsShared",
// "WindowsExclusive", "ASIO", "PipeWire", "PulseAudio", "AlsaDirect".
QString audioEngineKey(AudioEngineKind kind);
std::optional<AudioEngineKind> audioEngineFromKey(const QString& key);

// The label the operator sees ("Older drivers", "Core Audio", ...).
QString audioEngineLabel(AudioEngineKind kind);

// Both Windows kinds map to Wasapi.
AudioBackendId audioBackendFor(AudioEngineKind kind);

// R-AUD-07: 2 channels gives {1, 2}; 1 gives {1, 1}; 5 gives {1, 2},
// {3, 2}, {5, 1}.  Empty for a channel count below 1.
QList<AudioChannelPair> audioChannelPairs(int channelCount);

// "Outputs 3-4", "Inputs 1-2", "Output 5".
QString audioPairLabel(AudioDeviceDirection direction, const AudioChannelPair& pair);

// The device list entry: the name alone for a device of one or two
// channels, else the name, a space, U+00B7, a space and
// the pair label.
QString audioDeviceEntryLabel(const AudioDeviceInfo& info, const AudioChannelPair& pair);

// The MicChannel settings key: "Left", "Right", "Both".
enum class MicChannelPick { Left, Right, Both };
QString micChannelKey(MicChannelPick pick);
std::optional<MicChannelPick> micChannelFromKey(const QString& key);

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::AudioDeviceDirection)
