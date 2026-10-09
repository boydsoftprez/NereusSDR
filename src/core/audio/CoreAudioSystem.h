// =================================================================
// src/core/audio/CoreAudioSystem.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The seam between the Core Audio
// engine and the Mac's audio system (R-AUD-01, R-AUD-03, R-AUD-11,
// R-AUD-14, R-AUD-32); no upstream logic.
//
// ICoreAudioSystem is what CoreAudioBackend asks of the system: the
// devices with their UID, name, transport, channels, alive and hog-mode
// state, the two system defaults, the system's notices, and the streams.
// The real adapter (makeCoreAudioSystem, CoreAudioSystem.cpp) answers
// through the HAL; a test installs tests/fakes/FakeCoreAudioSystem.h.
//
// The real adapter's listeners (the device list, both defaults, and per
// device its alive and hog-mode state) run on one private serial dispatch
// queue and only post the notice to the sink.  The per-device listeners
// are added and removed inside devices().  While
// audioDevicesBarredForTestRun() is true the streams it makes refuse to
// open (R-AUD-32).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-07, R-AUD-11, R-AUD-14). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QList>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

struct CoreAudioDeviceRecord {
    std::uint32_t objectId = 0;      // AudioObjectID, valid until the device goes
    QString uid;                     // kAudioDevicePropertyDeviceUID: the saved DeviceId
    QString name;                    // kAudioObjectPropertyName
    std::uint32_t transportType = 0; // kAudioDevicePropertyTransportType
    int outputChannels = 0;          // kAudioDevicePropertyStreamConfiguration, output scope
    int inputChannels = 0;           // the same, input scope
    bool alive = true;               // kAudioDevicePropertyDeviceIsAlive
    std::int32_t hogPid = -1;        // kAudioDevicePropertyHogMode
};

class ICoreAudioSystem {
public:
    virtual ~ICoreAudioSystem() = default;
    virtual QList<CoreAudioDeviceRecord> devices() = 0;                      // catalogue thread
    virtual std::optional<std::uint32_t> defaultDevice(AudioDeviceDirection) = 0;
    virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;  // listeners on a private serial queue
    virtual std::int32_t ownPid() const = 0;
    virtual std::unique_ptr<IAudioBus> createOutput(const CoreAudioDeviceRecord&, const AudioStreamRequest&) = 0;
    virtual std::unique_ptr<IAudioInputStream> createInput(const CoreAudioDeviceRecord&, const AudioStreamRequest&, MicChannelPick, IAudioInputSink*) = 0;
};

std::unique_ptr<ICoreAudioSystem> makeCoreAudioSystem();   // Q_OS_MAC only

// The HAL's transport type code as R-AUD-14's transport: 'bltn' BuiltIn,
// 'usb ' Usb, 'blue' and 'blea' Bluetooth, 'hdmi' and 'dprt' Hdmi, 'virt'
// Virtual, anything else Unknown.
AudioTransport coreAudioTransport(std::uint32_t transportType);

} // namespace NereusSDR
