// =================================================================
// src/core/audio/WasapiBackendWin.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only.  See
// WasapiBackendWin.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-11, R-AUD-16). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/WasapiSystemWin.h"

#include "core/audio/WasapiBackendWin.h"

#include "core/audio/AudioTestBarrier.h"
#include "core/audio/WasapiInputStreamWin.h"
#include "core/audio/WasapiOutputBusWin.h"

#include <utility>

namespace NereusSDR {

WasapiBackendWin::WasapiBackendWin() = default;

WasapiBackendWin::~WasapiBackendWin()
{
    std::lock_guard<std::mutex> lock(m_notifierMutex);
    m_notifier.reset();
}

bool WasapiBackendWin::running() const
{
    return m_running.load();
}

QList<AudioDeviceInfo> WasapiBackendWin::enumerate()
{
    QList<AudioDeviceInfo> devices;
    // R-AUD-32: a test run lists nothing and asks the system nothing.
    if (audioDevicesBarredForTestRun()) {
        return devices;
    }
    const WasapiComScope com;
    if (!com.usable()) {
        return devices;
    }
    bool answered = true;
    for (const AudioDeviceDirection direction :
         {AudioDeviceDirection::Output, AudioDeviceDirection::Input}) {
        bool directionAnswered = true;
        const QList<WasapiEndpoint> endpoints = wasapiActiveEndpoints(direction, &directionAnswered);
        answered = answered && directionAnswered;
        const std::optional<QString> defaultId = wasapiDefaultEndpointId(direction);
        for (const WasapiEndpoint& endpoint : endpoints) {
            AudioDeviceInfo info;
            info.backend = AudioBackendId::Wasapi;
            info.direction = direction;
            info.id = endpoint.id;
            info.name = endpoint.name;
            info.transport = wasapiTransport(endpoint.enumeratorName, endpoint.formFactor);
            info.state = AudioDeviceState::Present;   // only active endpoints are listed
            info.channelCount = endpoint.channels > 0 ? endpoint.channels : 2;
            info.isDefault = defaultId && *defaultId == endpoint.id;
            devices.append(info);
        }
    }
    m_running.store(answered);
    return devices;
}

std::optional<QString> WasapiBackendWin::defaultDeviceId(AudioDeviceDirection direction)
{
    if (audioDevicesBarredForTestRun()) {
        return std::nullopt;
    }
    const WasapiComScope com;
    if (!com.usable()) {
        return std::nullopt;
    }
    return wasapiDefaultEndpointId(direction);
}

void WasapiBackendWin::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    std::lock_guard<std::mutex> lock(m_notifierMutex);
    if (!m_notifier) {
        if (!sink || audioDevicesBarredForTestRun()) {
            return;
        }
        m_notifier = std::make_unique<WasapiEndpointNotifier>();
    }
    m_notifier->setSink(std::move(sink));
}

std::unique_ptr<IAudioBus> WasapiBackendWin::createOutput(const AudioStreamRequest& request)
{
    return std::make_unique<WasapiOutputBusWin>(request);
}

std::unique_ptr<IAudioInputStream> WasapiBackendWin::createInput(const AudioStreamRequest& request,
                                                                 MicChannelPick pick,
                                                                 IAudioInputSink* sink)
{
    return std::make_unique<WasapiInputStreamWin>(request, pick, sink);
}

} // namespace NereusSDR
