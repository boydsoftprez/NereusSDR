// =================================================================
// src/core/audio/AlsaDirectBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  ALSA direct as one engine backend:
// the Linux Core's only engine (R-AUD-01, R-AUD-02, R-AUD-25, D18);
// no upstream logic.
//
// The backend lists the Core's sound cards through an IAlsaDirectSystem
// (the real adapter or a test's fake).  Each card's PCM device with
// playback is one output: its id is "<cardId>,<device>", its name the
// card's name as ALSA gives it (design choice 4), its ALSA card and device
// numbers set (so an older "Name (hw:C,D)" choice still matches it).  A
// card on the USB bus has Usb transport; a vc4hdmi card (a Raspberry Pi's
// HDMI) has Hdmi.  The default output (settled call 10) is ALSA's
// defaults.pcm.card when that card has a playback device, else the
// lowest-numbered card with one; its lowest playback device.  There are no
// inputs (the Core's mic is the radio's).  ALSA has no sound server, so
// the engine always runs.  The system's notices pass straight to the
// catalogue.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-01, R-AUD-02, R-AUD-25).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AlsaDirectSystem.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QList>
#include <QString>

#include <memory>
#include <optional>

namespace NereusSDR {

// Settled call 10: the record the Core's default card plays on.
std::optional<AlsaCardRecord> alsaDefaultRecord(const QList<AlsaCardRecord>& records,
                                                std::optional<int> configuredDefaultCard);

// The device list entries of `records`, isDefault from settled call 10.
QList<AudioDeviceInfo> alsaDevicesFromRecords(const QList<AlsaCardRecord>& records,
                                              std::optional<int> configuredDefaultCard);

class AlsaDirectBackend final : public IAudioEngineBackend {
public:
    explicit AlsaDirectBackend(std::shared_ptr<IAlsaDirectSystem> system);

    AudioBackendId id() const override { return AudioBackendId::AlsaDirect; }
    bool running() const override { return m_system != nullptr; }
    QList<AudioDeviceInfo> enumerate() override;
    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    // An empty id opens the default card; nullptr for an id not listed
    // now, or when there is no card.
    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override;
    // Not part of this design: always nullptr.
    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;

private:
    std::shared_ptr<IAlsaDirectSystem> m_system;
};

} // namespace NereusSDR
