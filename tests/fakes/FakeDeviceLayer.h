// =================================================================
// tests/fakes/FakeDeviceLayer.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test helper.  Gives an AudioEngine the
// device layer the app runs (engine backends, the device catalogue and
// the stream supervisor) on one fake older-drivers engine whose outputs
// are FakeMatcherAudioBus.  No device is touched (R-AUD-32).
//
// The fake lists the outputs the engine tests name, under one host API,
// with "Fake default" as the system default.  `opened` hears the
// PortAudio name of every output made (empty for the system default).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 fix. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/AudioEngine.h"
#include "core/audio/PortAudioBackend.h"
#include "FakeAudioEngineBackend.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace NereusSDR::Test {

inline std::shared_ptr<FakeAudioEngineBackend> useFakeDeviceLayer(
    AudioEngine* engine, std::function<void(const QString& name)> opened)
{
    const QString hostApi = QStringLiteral("ALSA");
    auto backend = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
    QList<AudioDeviceInfo> devices;
    const QStringList names = {QStringLiteral("Fake default"),     QStringLiteral("TestDevice"),
                               QStringLiteral("Desk headphones"),  QStringLiteral("Other headphones"),
                               QStringLiteral("BuiltIn"),          QStringLiteral("Phones"),
                               QStringLiteral("old-device")};
    for (const QString& name : names) {
        AudioDeviceInfo info;
        info.backend = AudioBackendId::PortAudio;
        info.direction = AudioDeviceDirection::Output;
        info.id = portAudioDeviceId(hostApi, name);
        info.name = name;
        info.hostApi = hostApi;
        info.channelCount = 2;
        devices.append(info);
    }
    backend->setDevices(devices);
    backend->setDefault(AudioDeviceDirection::Output, portAudioDeviceId(hostApi, names.first()));
    backend->setOutputCreatedHook([opened](const AudioStreamRequest& request) {
        if (opened) {
            opened(portAudioNameOfId(request.deviceId));
        }
    });
    engine->setAudioBackendsForTest({backend});
    return backend;
}

} // namespace NereusSDR::Test
