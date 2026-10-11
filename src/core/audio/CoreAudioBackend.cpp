// =================================================================
// src/core/audio/CoreAudioBackend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See CoreAudioBackend.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-07, R-AUD-11, R-AUD-14). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CoreAudioBackend.h"

#include <utility>

namespace NereusSDR {

namespace {

AudioDeviceState stateOf(const CoreAudioDeviceRecord& record, std::int32_t ownPid)
{
    if (!record.alive) {
        return AudioDeviceState::NotConnected;
    }
    // R-AUD-11: hog mode held by another process (-1 is free).
    if (record.hogPid != -1 && record.hogPid != ownPid) {
        return AudioDeviceState::InUse;
    }
    return AudioDeviceState::Present;
}

} // namespace

CoreAudioBackend::CoreAudioBackend(std::unique_ptr<ICoreAudioSystem> system)
    : m_system(std::move(system))
{
}

QList<CoreAudioDeviceRecord> CoreAudioBackend::refreshRecords()
{
    QList<CoreAudioDeviceRecord> records = m_system ? m_system->devices()
                                                    : QList<CoreAudioDeviceRecord>{};
    std::lock_guard<std::mutex> lock(m_mutex);
    m_records = records;
    return records;
}

QList<AudioDeviceInfo> CoreAudioBackend::enumerate()
{
    QList<AudioDeviceInfo> devices;
    if (!m_system) {
        return devices;
    }
    const QList<CoreAudioDeviceRecord> records = refreshRecords();
    const std::optional<std::uint32_t> defaultOutput =
        m_system->defaultDevice(AudioDeviceDirection::Output);
    const std::optional<std::uint32_t> defaultInput =
        m_system->defaultDevice(AudioDeviceDirection::Input);
    const std::int32_t ownPid = m_system->ownPid();
    for (const CoreAudioDeviceRecord& record : records) {
        AudioDeviceInfo info;
        info.backend = AudioBackendId::CoreAudio;
        info.id = record.uid;
        info.name = record.name;
        info.transport = coreAudioTransport(record.transportType);
        info.state = stateOf(record, ownPid);
        if (record.outputChannels > 0) {
            info.direction = AudioDeviceDirection::Output;
            info.channelCount = record.outputChannels;
            info.isDefault = defaultOutput && *defaultOutput == record.objectId;
            devices.append(info);
        }
        if (record.inputChannels > 0) {
            info.direction = AudioDeviceDirection::Input;
            info.channelCount = record.inputChannels;
            info.isDefault = defaultInput && *defaultInput == record.objectId;
            devices.append(info);
        }
    }
    return devices;
}

std::optional<QString> CoreAudioBackend::defaultDeviceId(AudioDeviceDirection direction)
{
    if (!m_system) {
        return std::nullopt;
    }
    // Asked live: a default change arrives before any re-list.
    const std::optional<std::uint32_t> object = m_system->defaultDevice(direction);
    if (!object) {
        return std::nullopt;
    }
    auto find = [&](const QList<CoreAudioDeviceRecord>& records) -> std::optional<QString> {
        for (const CoreAudioDeviceRecord& record : records) {
            if (record.objectId == *object) {
                return record.uid;
            }
        }
        return std::nullopt;
    };
    QList<CoreAudioDeviceRecord> known;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        known = m_records;
    }
    if (std::optional<QString> uid = find(known)) {
        return uid;
    }
    // A device that arrived since the last listing.
    return find(refreshRecords());
}

void CoreAudioBackend::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    if (m_system) {
        m_system->setNoticeSink(std::move(sink));
    }
}

std::optional<CoreAudioDeviceRecord> CoreAudioBackend::recordFor(const AudioStreamRequest& request)
{
    if (!m_system) {
        return std::nullopt;
    }
    std::optional<std::uint32_t> wantedObject;
    if (request.deviceId.isEmpty()) {
        wantedObject = m_system->defaultDevice(request.direction);
        if (!wantedObject) {
            return std::nullopt;
        }
    }
    auto find = [&](const QList<CoreAudioDeviceRecord>& records)
        -> std::optional<CoreAudioDeviceRecord> {
        for (const CoreAudioDeviceRecord& record : records) {
            const bool match = wantedObject ? record.objectId == *wantedObject
                                            : record.uid == request.deviceId;
            if (match) {
                return record;
            }
        }
        return std::nullopt;
    };
    QList<CoreAudioDeviceRecord> known;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        known = m_records;
    }
    if (std::optional<CoreAudioDeviceRecord> record = find(known)) {
        return record;
    }
    return find(refreshRecords());
}

std::unique_ptr<IAudioBus> CoreAudioBackend::createOutput(const AudioStreamRequest& request)
{
    AudioStreamRequest out = request;
    out.direction = AudioDeviceDirection::Output;
    const std::optional<CoreAudioDeviceRecord> record = recordFor(out);
    if (!record || record->outputChannels <= 0) {
        return nullptr;
    }
    return m_system->createOutput(*record, out);
}

std::unique_ptr<IAudioInputStream> CoreAudioBackend::createInput(const AudioStreamRequest& request,
                                                                 MicChannelPick pick,
                                                                 IAudioInputSink* sink)
{
    AudioStreamRequest in = request;
    in.direction = AudioDeviceDirection::Input;
    const std::optional<CoreAudioDeviceRecord> record = recordFor(in);
    if (!record || record->inputChannels <= 0) {
        return nullptr;
    }
    return m_system->createInput(*record, in, pick, sink);
}

} // namespace NereusSDR
