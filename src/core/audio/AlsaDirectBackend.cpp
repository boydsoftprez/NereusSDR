// =================================================================
// src/core/audio/AlsaDirectBackend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AlsaDirectBackend.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 12 (R-AUD-01, R-AUD-02, R-AUD-25).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AlsaDirectBackend.h"

#include "core/LogCategories.h"

#include <utility>

namespace NereusSDR {

namespace {

AudioTransport transportOf(const AlsaCardRecord& record)
{
    if (record.bus == QStringLiteral("usb")) {
        return AudioTransport::Usb;
    }
    if (record.cardId.startsWith(QStringLiteral("vc4hdmi"))) {
        return AudioTransport::Hdmi;
    }
    return AudioTransport::Unknown;
}

// The card's lowest playback device.
std::optional<AlsaCardRecord> firstOfCard(const QList<AlsaCardRecord>& records, int card)
{
    std::optional<AlsaCardRecord> best;
    for (const AlsaCardRecord& record : records) {
        if (record.card == card && (!best || record.device < best->device)) {
            best = record;
        }
    }
    return best;
}

} // namespace

std::optional<AlsaCardRecord> alsaDefaultRecord(const QList<AlsaCardRecord>& records,
                                                std::optional<int> configuredDefaultCard)
{
    if (configuredDefaultCard) {
        if (std::optional<AlsaCardRecord> configured = firstOfCard(records, *configuredDefaultCard)) {
            return configured;
        }
    }
    std::optional<int> lowest;
    for (const AlsaCardRecord& record : records) {
        if (record.card >= 0 && (!lowest || record.card < *lowest)) {
            lowest = record.card;
        }
    }
    if (!lowest) {
        return std::nullopt;
    }
    return firstOfCard(records, *lowest);
}

QList<AudioDeviceInfo> alsaDevicesFromRecords(const QList<AlsaCardRecord>& records,
                                              std::optional<int> configuredDefaultCard)
{
    const std::optional<AlsaCardRecord> def = alsaDefaultRecord(records, configuredDefaultCard);
    QList<AudioDeviceInfo> devices;
    for (const AlsaCardRecord& record : records) {
        if (record.cardId.isEmpty()) {
            continue;
        }
        AudioDeviceInfo info;
        info.backend = AudioBackendId::AlsaDirect;
        info.direction = AudioDeviceDirection::Output;
        info.id = alsaDeviceId(record);
        info.name = record.cardName.isEmpty() ? record.cardId : record.cardName;
        info.transport = transportOf(record);
        info.state = AudioDeviceState::Present;
        info.channelCount = record.channels;
        info.alsaCard = record.card;
        info.alsaDevice = record.device;
        info.isDefault = def && *def == record;
        devices.append(info);
    }
    return devices;
}

AlsaDirectBackend::AlsaDirectBackend(std::shared_ptr<IAlsaDirectSystem> system)
    : m_system(std::move(system))
{
}

QList<AudioDeviceInfo> AlsaDirectBackend::enumerate()
{
    if (!m_system) {
        return {};
    }
    return alsaDevicesFromRecords(m_system->playbackCards(), m_system->configuredDefaultCard());
}

std::optional<QString> AlsaDirectBackend::defaultDeviceId(AudioDeviceDirection direction)
{
    if (!m_system || direction != AudioDeviceDirection::Output) {
        return std::nullopt;
    }
    const std::optional<AlsaCardRecord> def =
        alsaDefaultRecord(m_system->playbackCards(), m_system->configuredDefaultCard());
    if (!def) {
        return std::nullopt;
    }
    return alsaDeviceId(*def);
}

void AlsaDirectBackend::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    if (m_system) {
        m_system->setNoticeSink(std::move(sink));
    }
}

std::unique_ptr<IAudioBus> AlsaDirectBackend::createOutput(const AudioStreamRequest& request)
{
    if (!m_system) {
        return nullptr;
    }
    const QList<AlsaCardRecord> records = m_system->playbackCards();
    std::optional<AlsaCardRecord> card;
    if (request.deviceId.isEmpty()) {
        card = alsaDefaultRecord(records, m_system->configuredDefaultCard());
    } else {
        for (const AlsaCardRecord& record : records) {
            if (alsaDeviceId(record) == request.deviceId) {
                card = record;
                break;
            }
        }
    }
    if (!card) {
        qCWarning(lcAudio) << "ALSA direct does not list the card"
                           << (request.deviceId.isEmpty() ? QStringLiteral("(default)")
                                                          : request.deviceId);
        return nullptr;
    }
    return m_system->createOutput(*card, request);
}

std::unique_ptr<IAudioInputStream> AlsaDirectBackend::createInput(const AudioStreamRequest&,
                                                                  MicChannelPick,
                                                                  IAudioInputSink*)
{
    return nullptr;
}

} // namespace NereusSDR
