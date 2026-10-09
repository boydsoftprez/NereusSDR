// =================================================================
// src/core/audio/PulseAudioBackend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PulseAudioBackend.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 11 (R-AUD-01, R-AUD-02, R-AUD-07,
//               R-AUD-14, R-AUD-31). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/PulseAudioBackend.h"

#include "core/LogCategories.h"

#include <utility>

namespace NereusSDR {

namespace {

AudioTransport transportOf(const PulseDeviceRecord& record)
{
    if (record.busProperty == QStringLiteral("bluetooth")) {
        return AudioTransport::Bluetooth;
    }
    if (record.busProperty == QStringLiteral("usb")) {
        return AudioTransport::Usb;
    }
    return AudioTransport::Unknown;
}

AudioDeviceDirection directionOf(const PulseDeviceRecord& record)
{
    return record.isSink ? AudioDeviceDirection::Output : AudioDeviceDirection::Input;
}

} // namespace

QList<AudioDeviceInfo> pulseDevicesFromRecords(const QList<PulseDeviceRecord>& records,
                                               const QString& defaultSink,
                                               const QString& defaultSource)
{
    QList<AudioDeviceInfo> devices;
    for (const PulseDeviceRecord& record : records) {
        if (!pulseDeviceIsListed(record)) {
            continue;
        }
        AudioDeviceInfo info;
        info.backend = AudioBackendId::PulseAudio;
        info.direction = directionOf(record);
        info.id = record.name;
        info.name = record.description.isEmpty() ? record.name : record.description;
        info.transport = transportOf(record);
        info.state = AudioDeviceState::Present;
        // A device that reports no map plays as stereo.
        info.channelCount = record.channelMap.isEmpty() ? 2 : int(record.channelMap.size());
        info.alsaCard = record.alsaCard;
        info.alsaDevice = record.alsaDevice;
        const QString& wanted = record.isSink ? defaultSink : defaultSource;
        info.isDefault = !wanted.isEmpty() && record.name == wanted;
        devices.append(info);
    }
    return devices;
}

PulseAudioBackend::PulseAudioBackend(std::shared_ptr<IPulseAudioSystem> system,
                                     std::function<bool()> selected)
    : m_system(std::move(system))
    , m_selected(std::move(selected))
{
}

bool PulseAudioBackend::running() const
{
    if (!m_system) {
        return false;
    }
    return m_selected ? m_selected() : m_system->running();
}

QList<AudioDeviceInfo> PulseAudioBackend::enumerate()
{
    if (!m_system) {
        return {};
    }
    return pulseDevicesFromRecords(m_system->devices(),
                                   m_system->defaultName(AudioDeviceDirection::Output),
                                   m_system->defaultName(AudioDeviceDirection::Input));
}

std::optional<QString> PulseAudioBackend::defaultDeviceId(AudioDeviceDirection direction)
{
    if (!m_system) {
        return std::nullopt;
    }
    const QString name = m_system->defaultName(direction);
    if (name.isEmpty()) {
        return std::nullopt;
    }
    return name;
}

void PulseAudioBackend::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    if (m_system) {
        m_system->setNoticeSink(std::move(sink));
    }
}

std::optional<PulseDeviceRecord> PulseAudioBackend::deviceFor(const QString& deviceId,
                                                              AudioDeviceDirection direction)
{
    const QString wanted = deviceId.isEmpty() ? m_system->defaultName(direction) : deviceId;
    if (!wanted.isEmpty()) {
        for (const PulseDeviceRecord& record : m_system->devices()) {
            if (record.name == wanted && pulseDeviceIsListed(record)
                && directionOf(record) == direction) {
                return record;
            }
        }
    }
    if (deviceId.isEmpty()) {
        PulseDeviceRecord followDefault;   // an empty name follows the server default
        followDefault.isSink = direction == AudioDeviceDirection::Output;
        return followDefault;
    }
    return std::nullopt;
}

std::unique_ptr<IAudioBus> PulseAudioBackend::createOutput(const AudioStreamRequest& request)
{
    if (!m_system) {
        return nullptr;
    }
    const std::optional<PulseDeviceRecord> device = deviceFor(request.deviceId,
                                                              AudioDeviceDirection::Output);
    if (!device) {
        qCWarning(lcAudio) << "PulseAudio does not list the output" << request.deviceId;
        return nullptr;
    }
    return m_system->createOutput(*device, request);
}

std::unique_ptr<IAudioInputStream> PulseAudioBackend::createInput(const AudioStreamRequest& request,
                                                                  MicChannelPick pick,
                                                                  IAudioInputSink* sink)
{
    if (!m_system) {
        return nullptr;
    }
    const std::optional<PulseDeviceRecord> device = deviceFor(request.deviceId,
                                                              AudioDeviceDirection::Input);
    if (!device) {
        qCWarning(lcAudio) << "PulseAudio does not list the input" << request.deviceId;
        return nullptr;
    }
    return m_system->createInput(*device, request, pick, sink);
}

} // namespace NereusSDR
