// =================================================================
// src/core/audio/PortAudioBackend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PortAudioBackend.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-01, R-AUD-06, R-AUD-32).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/PortAudioBackend.h"

#include "core/LogCategories.h"
#include "core/audio/PortAudioBus.h"

#include <portaudio.h>

#include <utility>

namespace NereusSDR {

namespace {

// PortAudio's own host API names (Pa_GetHostApiInfo()->name).
const QString kMme = QStringLiteral("MME");
const QString kDirectSound = QStringLiteral("Windows DirectSound");
const QString kWdmKs = QStringLiteral("Windows WDM-KS");
const QString kWasapi = QStringLiteral("Windows WASAPI");
const QString kJack = QStringLiteral("JACK Audio Connection Kit");
const QString kAlsa = QStringLiteral("ALSA");
const QString kCoreAudio = QStringLiteral("Core Audio");

} // namespace

OlderDriverPlatform currentOlderDriverPlatform()
{
#if defined(Q_OS_WIN)
    return OlderDriverPlatform::Windows;
#elif defined(Q_OS_MAC)
    return OlderDriverPlatform::Mac;
#else
    return OlderDriverPlatform::Linux;
#endif
}

QStringList olderDriverHostApis(OlderDriverPlatform platform, bool includeReplacedHostApis)
{
    switch (platform) {
    case OlderDriverPlatform::Windows:
        if (includeReplacedHostApis) {
            return {kWasapi, kMme, kDirectSound, kWdmKs};
        }
        return {kMme, kDirectSound, kWdmKs};
    case OlderDriverPlatform::Linux:
        return {kJack, kAlsa};
    case OlderDriverPlatform::Mac:
        if (includeReplacedHostApis) {
            return {kCoreAudio};
        }
        return {};
    }
    return {};
}

QList<PortAudioDeviceRecord> listPortAudioDevices()
{
    QList<PortAudioDeviceRecord> records;
    if (PortAudioBus::portAudioBarredForTestRun()) {
        return records;
    }
    const PaDeviceIndex defaultOutput = Pa_GetDefaultOutputDevice();
    const PaDeviceIndex defaultInput = Pa_GetDefaultInputDevice();
    const QVector<PortAudioBus::HostApiInfo> apis = PortAudioBus::hostApis();
    for (const PortAudioBus::HostApiInfo& api : apis) {
        // One record per device: a device with outputs and inputs is listed
        // by both helpers under the same index.
        QList<int> indices;
        auto recordFor = [&](const PortAudioBus::DeviceInfo& device) {
            if (indices.contains(device.index)) {
                return;
            }
            indices.append(device.index);
            PortAudioDeviceRecord record;
            record.hostApi = api.name;
            record.name = device.name;
            record.outputChannels = device.maxOutputChannels;
            record.inputChannels = device.maxInputChannels;
            record.isDefaultOutput = device.index == defaultOutput;
            record.isDefaultInput = device.index == defaultInput;
            records.append(record);
        };
        for (const PortAudioBus::DeviceInfo& device : PortAudioBus::outputDevicesFor(api.index)) {
            recordFor(device);
        }
        for (const PortAudioBus::DeviceInfo& device : PortAudioBus::inputDevicesFor(api.index)) {
            recordFor(device);
        }
    }
    return records;
}

PortAudioBackend::PortAudioBackend(PortAudioListFn list, OlderDriverPlatform platform,
                                   bool includeReplacedHostApis)
    : m_list(std::move(list))
    , m_hostApis(olderDriverHostApis(platform, includeReplacedHostApis))
{
}

bool PortAudioBackend::offered(const QString& hostApi) const
{
    return m_hostApis.contains(hostApi);
}

QList<AudioDeviceInfo> PortAudioBackend::enumerate()
{
    QList<AudioDeviceInfo> devices;
    m_defaultOutput.reset();
    m_defaultInput.reset();
    const QList<PortAudioDeviceRecord> records = m_list ? m_list() : QList<PortAudioDeviceRecord>{};
    for (const PortAudioDeviceRecord& record : records) {
        if (!offered(record.hostApi)) {
            continue;
        }
        AudioDeviceInfo info;
        info.backend = AudioBackendId::PortAudio;
        info.id = record.name;
        info.name = record.name;
        info.hostApi = record.hostApi;
        if (record.outputChannels > 0) {
            info.direction = AudioDeviceDirection::Output;
            info.channelCount = record.outputChannels;
            info.isDefault = record.isDefaultOutput;
            if (record.isDefaultOutput && !m_defaultOutput) {
                m_defaultOutput = record.name;
            }
            devices.append(info);
        }
        if (record.inputChannels > 0) {
            info.direction = AudioDeviceDirection::Input;
            info.channelCount = record.inputChannels;
            info.isDefault = record.isDefaultInput;
            if (record.isDefaultInput && !m_defaultInput) {
                m_defaultInput = record.name;
            }
            devices.append(info);
        }
    }
    return devices;
}

std::optional<QString> PortAudioBackend::defaultDeviceId(AudioDeviceDirection direction)
{
    return direction == AudioDeviceDirection::Output ? m_defaultOutput : m_defaultInput;
}

void PortAudioBackend::setNoticeSink(std::function<void(AudioNotice)> /*sink*/)
{
    // PortAudio reports no device changes; Rescan lists them again.
}

std::unique_ptr<IAudioBus> PortAudioBackend::createOutput(const AudioStreamRequest& request)
{
    PortAudioConfig config;
    config.direction = AudioDirection::Output;
    config.deviceName = request.deviceId;
    if (request.bufferFrames > 0) {
        config.bufferSamples = request.bufferFrames;
    }
    config.exclusiveMode = request.exclusive;
    if (!request.hostApi.isEmpty()) {
        for (const PortAudioBus::HostApiInfo& api : PortAudioBus::hostApis()) {
            if (api.name == request.hostApi) {
                config.hostApiIndex = api.index;
                break;
            }
        }
    }
    auto bus = std::make_unique<PortAudioBus>();
    bus->setConfig(config);
    return bus;
}

std::unique_ptr<IAudioInputStream> PortAudioBackend::createInput(const AudioStreamRequest& /*request*/,
                                                                 MicChannelPick /*pick*/,
                                                                 IAudioInputSink* /*sink*/)
{
    // The PC microphone is captured by the helper process (R-R3-36).
    return nullptr;
}

void PortAudioBackend::rescan()
{
    if (PortAudioBus::portAudioBarredForTestRun()) {
        return;
    }
    Pa_Terminate();
    const PaError err = Pa_Initialize();
    if (err != paNoError) {
        qCWarning(lcAudio) << "Older drivers: PortAudio did not start again:" << Pa_GetErrorText(err);
        return;
    }
    qCInfo(lcAudio) << "Older drivers: PortAudio started again to list the devices present now";
}

} // namespace NereusSDR
