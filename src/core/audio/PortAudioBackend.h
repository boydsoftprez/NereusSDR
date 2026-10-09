// =================================================================
// src/core/audio/PortAudioBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  PortAudio as one engine backend
// (R-AUD-01, R-AUD-06): the older drivers ("Older drivers" in Setup);
// no upstream logic.
//
// The backend lists the host APIs no native engine replaces: "MME",
// "Windows DirectSound" and "Windows WDM-KS" on Windows, "JACK Audio
// Connection Kit" and "ALSA" on Linux, none on the Mac.  While
// includeReplacedHostApis is true it also lists the host APIs a native
// engine replaces ("Core Audio" on the Mac, "Windows WASAPI" on
// Windows), so nothing goes silent before that system's engine lands.
// A device's id is its PortAudio name and hostApi its host API name.
// Outputs open as a PortAudioBus on that host API.  The PC microphone is
// captured by the helper process, so createInput() opens nothing.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-01, R-AUD-06, R-AUD-32).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAudioEngineBackend.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

struct PortAudioDeviceRecord {
    QString hostApi;
    QString name;
    int outputChannels = 0;
    int inputChannels = 0;
    bool isDefaultOutput = false;
    bool isDefaultInput = false;
};

using PortAudioListFn = std::function<QList<PortAudioDeviceRecord>()>;

enum class OlderDriverPlatform { Mac, Windows, Linux };

// The platform this build runs on.
OlderDriverPlatform currentOlderDriverPlatform();

// The host API names offered under "Older drivers" on `platform`, in the
// order shown; with includeReplacedHostApis the replaced one comes first.
QStringList olderDriverHostApis(OlderDriverPlatform platform, bool includeReplacedHostApis);

// PortAudio's devices through PortAudioBus::hostApis / outputDevicesFor /
// inputDevicesFor, one record per device and host API.  The default flags
// mark PortAudio's default output and input.  Empty in a test run
// (R-AUD-32): no PortAudio call is made.
QList<PortAudioDeviceRecord> listPortAudioDevices();

class PortAudioBackend final : public IAudioEngineBackend {
public:
    PortAudioBackend(PortAudioListFn list, OlderDriverPlatform platform,
                     bool includeReplacedHostApis);

    AudioBackendId id() const override { return AudioBackendId::PortAudio; }
    bool running() const override { return true; }
    QList<AudioDeviceInfo> enumerate() override;
    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;   // PortAudio posts none
    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;
    // Terminates PortAudio and initialises it again, so it lists the
    // devices present now.  No PortAudio call in a test run.
    void rescan() override;

private:
    bool offered(const QString& hostApi) const;

    PortAudioListFn m_list;
    QStringList m_hostApis;
    std::optional<QString> m_defaultOutput;   // from the last enumerate()
    std::optional<QString> m_defaultInput;
};

} // namespace NereusSDR
