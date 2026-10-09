// =================================================================
// src/core/audio/AsioBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The window's view of ASIO (R-AUD-01,
// R-AUD-07, R-AUD-11, R-AUD-19, R-AUD-20, R-AUD-21, R-AUD-22); no upstream
// logic.
//
// An ASIO driver is loaded by the mic helper, never in the window (D9).
// This backend lists the drivers the helper describes, as one output and
// one input device each with the channel counts of its caps, and plays an
// output by writing 48 kHz stereo into a clock matcher built in shared
// memory (Task 13); the helper's buffer switch reads it.  Every open ASIO
// output is one use of the one session (R-AUD-19): each open or close
// sends the helper the whole list.  The helper runs while any use is open
// (Demand::AsioDevice, through the link's setDemanded).
//
// The microphone on ASIO goes through the CaptureSupervisor as on every
// other engine (Task 13), so createInput returns nullptr.
//
// While audioDevicesBarredForTestRun() is true nothing is described and
// every open fails (R-AUD-32).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-01, R-AUD-07, R-AUD-11,
//               R-AUD-19, R-AUD-20, R-AUD-21, R-AUD-22). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/CaptureProtocol.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

// How the backend reaches the helper.  Each call may come from any thread
// and must only post (AudioEngine queues them to the CaptureSupervisor);
// none may call back into the backend before it returns.
struct AsioHelperLink {
    std::function<void(const QString& driver)> describe;            // "" lists the drivers
    std::function<void(const CaptureProtocol::AsioOpen& open)> open;
    std::function<void()> openControlPanel;
    std::function<void(bool demanded)> setDemanded;                 // Demand::AsioDevice
};

// The session's buffer and rate as the operator saved them
// (audio/Asio/BufferFrames, 0 for the driver's preferred size, and
// audio/Asio/SampleRate, 48000 by default).
struct AsioSessionPreferences {
    int bufferFrames = 0;
    double sampleRate = 48000.0;
};

// The open error every ASIO output gives in a test run.
QString asioTestRunError();

class AsioBackend final : public IAudioEngineBackend {
public:
    AsioBackend();
    ~AsioBackend() override;

    AsioBackend(const AsioBackend&) = delete;
    AsioBackend& operator=(const AsioBackend&) = delete;

    // An empty link (no describe) disconnects the backend; open outputs
    // then lose their device.
    void setHelperLink(AsioHelperLink link);
    // Replaces the AppSettings reader (tests).
    void setPreferencesSource(std::function<AsioSessionPreferences()> source);

    // The helper's answers, from the CaptureSupervisor's signals.  Any
    // thread.
    void onAsioCaps(const CaptureProtocol::AsioCapsRecord& caps);
    void onAsioState(const CaptureProtocol::AsioState& state);

    AudioBackendId id() const override { return AudioBackendId::Asio; }
    bool running() const override;
    QList<AudioDeviceInfo> enumerate() override;
    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;
    bool hasControlPanel() const override { return true; }
    void openControlPanel(const QString& deviceId) override;   // the running session's driver only
    void rescan() override;
    bool opensOneStreamAtATime() const override { return true; }

    // The described state, for the Setup page and tests.
    QStringList drivers() const;
    std::optional<AsioDriverCaps> driverCaps(const QString& driver) const;
    bool driverInUse(const QString& driver) const;
    QString sessionDriver() const;     // empty with no output open

    struct Shared;

private:
    std::shared_ptr<Shared> m_shared;
};

} // namespace NereusSDR
