// =================================================================
// src/core/audio/PulseAudioBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  PulseAudio as one engine backend
// (R-AUD-01, R-AUD-02, R-AUD-07, R-AUD-14, R-AUD-31); no upstream logic.
//
// The backend lists the desktop's PulseAudio sinks and sources through an
// IPulseAudioSystem (the real adapter or a test's fake).  A sink is an
// output and a source an input; monitor sources are not listed.  A
// device's id is its name and its display name its description; its
// channel count is the number of channels in its channel map, so pairs
// are offered only when the map lists more than two (R-AUD-07, where the
// interface's profile exposes its channels).  The transport is Bluetooth
// when device.bus is "bluetooth" and USB when it is "usb" (R-AUD-14); an
// ALSA device carries its card and device numbers.  Our own VAX devices
// are listed as every engine lists them.  The system's notices pass
// straight to the catalogue.  A device with no id opens on the server's
// default sink or source.
//
// running() reports the Linux engine selection when the registry gives
// one (LinuxEngineSelection.h: PulseAudio runs only when the server that
// answers is not PipeWire's own PulseAudio service), else whether the
// server answers.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-02, R-AUD-07,
//               R-AUD-14, R-AUD-31). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/PulseAudioSystem.h"

#include <QList>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

// The device list entries of `records`: listed devices only, isDefault
// from the default sink and source.
QList<AudioDeviceInfo> pulseDevicesFromRecords(const QList<PulseDeviceRecord>& records,
                                               const QString& defaultSink,
                                               const QString& defaultSource);

class PulseAudioBackend final : public IAudioEngineBackend {
public:
    // `selected`, when given, is the Linux engine selection's answer for
    // PulseAudio; running() reports it.
    explicit PulseAudioBackend(std::shared_ptr<IPulseAudioSystem> system,
                               std::function<bool()> selected = {});

    AudioBackendId id() const override { return AudioBackendId::PulseAudio; }
    bool running() const override;
    QList<AudioDeviceInfo> enumerate() override;
    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    // Nullptr for an id PulseAudio does not list now.
    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;

private:
    // The listed device `deviceId` names for `direction`; for an empty id
    // the server default's device, or an empty record (follow the default)
    // when it is not listed.  nullopt when a named device is not listed.
    std::optional<PulseDeviceRecord> deviceFor(const QString& deviceId,
                                               AudioDeviceDirection direction);

    std::shared_ptr<IPulseAudioSystem> m_system;
    std::function<bool()> m_selected;
};

} // namespace NereusSDR
