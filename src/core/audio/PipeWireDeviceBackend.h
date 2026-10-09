// =================================================================
// src/core/audio/PipeWireDeviceBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  PipeWire as one engine backend
// (R-AUD-01, R-AUD-02, R-AUD-03, R-AUD-07, R-AUD-14); no upstream logic.
//
// The backend lists the desktop's PipeWire device nodes through an
// IPipeWireDeviceSystem (the real adapter or a test's fake).  An
// "Audio/Sink" node is an output, an "Audio/Source" node an input and an
// "Audio/Duplex" node both; monitors and program streams are not listed.
// A device's id is its node.name and its name the node's description; its
// channel count is the number of the node's positions, so an interface in
// its pro-audio profile (AUX0 to AUX9) offers its pairs (R-AUD-07).  The
// transport is Bluetooth when the node's device.api is "bluez5" (R-AUD-14);
// an ALSA node carries its card and device numbers.  The system's notices
// pass straight to the catalogue.  A device with no id opens on the
// system default sink or source.
//
// running() reports the Linux engine selection when the registry gives
// one (LinuxEngineSelection.h), else whether the daemon answers.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 10 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-07, R-AUD-14). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-31): the
//               system is shared with the Linux engine selection, whose
//               answer running() reports. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/PipeWireDeviceSystem.h"

#include <QList>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

// The device list entries of `nodes`: listed nodes only, one entry per
// direction a node serves, isDefault from the default sink and source.
QList<AudioDeviceInfo> pipeWireDevicesFromNodes(const QList<PipeWireNodeRecord>& nodes,
                                                const QString& defaultSink,
                                                const QString& defaultSource);

class PipeWireDeviceBackend final : public IAudioEngineBackend {
public:
    // `selected`, when given, is the Linux engine selection's answer for
    // PipeWire; running() reports it.
    explicit PipeWireDeviceBackend(std::shared_ptr<IPipeWireDeviceSystem> system,
                                   std::function<bool()> selected = {});

    AudioBackendId id() const override { return AudioBackendId::PipeWire; }
    bool running() const override;
    QList<AudioDeviceInfo> enumerate() override;
    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    // Nullptr for an id PipeWire does not list now.
    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;

private:
    // The listed node `deviceId` names for `direction`; for an empty id the
    // system default's node, or an empty record (follow the default) when
    // it is not listed.  nullopt when a named node is not listed.
    std::optional<PipeWireNodeRecord> nodeFor(const QString& deviceId,
                                              AudioDeviceDirection direction);

    std::shared_ptr<IPipeWireDeviceSystem> m_system;
    std::function<bool()> m_selected;
};

} // namespace NereusSDR
