// =================================================================
// src/core/audio/CoreAudioBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Core Audio as one engine backend on
// the Mac (R-AUD-01, R-AUD-02, R-AUD-03, R-AUD-07, R-AUD-11, R-AUD-14);
// no upstream logic.
//
// The backend lists the system's devices through an ICoreAudioSystem: one
// entry per direction a device has channels for, its id the device UID
// and its name the HAL's name.  A device that is not alive is
// NotConnected; one whose hog mode another process holds is InUse
// (R-AUD-11); the transport comes from the HAL's transport type
// (R-AUD-14).  The system's notices reach the catalogue unchanged.  A
// stream opens on the device whose UID the request names, or the system
// default when it names none.  Core Audio lists update by themselves, so
// rescan() does nothing (R-AUD-06).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-07, R-AUD-11, R-AUD-14). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/CoreAudioSystem.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QList>
#include <QString>

#include <memory>
#include <mutex>
#include <optional>

namespace NereusSDR {

class CoreAudioBackend final : public IAudioEngineBackend {
public:
    explicit CoreAudioBackend(std::unique_ptr<ICoreAudioSystem> system);

    AudioBackendId id() const override { return AudioBackendId::CoreAudio; }
    bool running() const override { return m_system != nullptr; }
    QList<AudioDeviceInfo> enumerate() override;
    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;

private:
    QList<CoreAudioDeviceRecord> refreshRecords();
    std::optional<CoreAudioDeviceRecord> recordFor(const AudioStreamRequest& request);

    std::unique_ptr<ICoreAudioSystem> m_system;
    std::mutex m_mutex;                       // guards m_records (catalogue and main thread)
    QList<CoreAudioDeviceRecord> m_records;   // from the last listing
};

} // namespace NereusSDR
