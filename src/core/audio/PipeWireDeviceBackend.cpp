// =================================================================
// src/core/audio/PipeWireDeviceBackend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PipeWireDeviceBackend.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 10 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-07, R-AUD-14). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/PipeWireDeviceBackend.h"

#include "core/LogCategories.h"

#include <utility>

namespace NereusSDR {

namespace {

const QString kSinkClass = QStringLiteral("Audio/Sink");
const QString kSourceClass = QStringLiteral("Audio/Source");
const QString kDuplexClass = QStringLiteral("Audio/Duplex");

bool servesOutput(const PipeWireNodeRecord& node)
{
    return node.mediaClass == kSinkClass || node.mediaClass == kDuplexClass;
}

bool servesInput(const PipeWireNodeRecord& node)
{
    return node.mediaClass == kSourceClass || node.mediaClass == kDuplexClass;
}

bool serves(const PipeWireNodeRecord& node, AudioDeviceDirection direction)
{
    return direction == AudioDeviceDirection::Output ? servesOutput(node) : servesInput(node);
}

AudioTransport transportOf(const PipeWireNodeRecord& node)
{
    if (node.deviceApi == QStringLiteral("bluez5")) {
        return AudioTransport::Bluetooth;
    }
    return AudioTransport::Unknown;
}

} // namespace

QList<AudioDeviceInfo> pipeWireDevicesFromNodes(const QList<PipeWireNodeRecord>& nodes,
                                                const QString& defaultSink,
                                                const QString& defaultSource)
{
    QList<AudioDeviceInfo> devices;
    for (const PipeWireNodeRecord& node : nodes) {
        // Our own VAX nodes are listed on purpose, as every engine lists them.
        if (!pipeWireNodeIsListed(node)) {
            continue;
        }
        AudioDeviceInfo info;
        info.backend = AudioBackendId::PipeWire;
        info.id = node.nodeName;
        info.name = node.description.isEmpty() ? node.nodeName : node.description;
        info.transport = transportOf(node);
        info.state = AudioDeviceState::Present;
        // A node that reports no positions plays as stereo.
        info.channelCount = node.positions.isEmpty() ? 2 : int(node.positions.size());
        if (node.deviceApi == QStringLiteral("alsa")) {
            info.alsaCard = node.alsaCard;
            info.alsaDevice = node.alsaDevice;
        }
        if (servesOutput(node)) {
            info.direction = AudioDeviceDirection::Output;
            info.isDefault = !defaultSink.isEmpty() && node.nodeName == defaultSink;
            devices.append(info);
        }
        if (servesInput(node)) {
            info.direction = AudioDeviceDirection::Input;
            info.isDefault = !defaultSource.isEmpty() && node.nodeName == defaultSource;
            devices.append(info);
        }
    }
    return devices;
}

PipeWireDeviceBackend::PipeWireDeviceBackend(std::unique_ptr<IPipeWireDeviceSystem> system)
    : m_system(std::move(system))
{
}

bool PipeWireDeviceBackend::running() const
{
    return m_system && m_system->running();
}

QList<AudioDeviceInfo> PipeWireDeviceBackend::enumerate()
{
    if (!m_system) {
        return {};
    }
    return pipeWireDevicesFromNodes(m_system->nodes(),
                                    m_system->defaultNodeName(AudioDeviceDirection::Output),
                                    m_system->defaultNodeName(AudioDeviceDirection::Input));
}

std::optional<QString> PipeWireDeviceBackend::defaultDeviceId(AudioDeviceDirection direction)
{
    if (!m_system) {
        return std::nullopt;
    }
    const QString name = m_system->defaultNodeName(direction);
    if (name.isEmpty()) {
        return std::nullopt;
    }
    return name;
}

void PipeWireDeviceBackend::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    if (m_system) {
        m_system->setNoticeSink(std::move(sink));
    }
}

std::optional<PipeWireNodeRecord> PipeWireDeviceBackend::nodeFor(const QString& deviceId,
                                                                 AudioDeviceDirection direction)
{
    const QString wanted = deviceId.isEmpty() ? m_system->defaultNodeName(direction) : deviceId;
    if (!wanted.isEmpty()) {
        for (const PipeWireNodeRecord& node : m_system->nodes()) {
            if (node.nodeName == wanted && pipeWireNodeIsListed(node) && serves(node, direction)) {
                return node;
            }
        }
    }
    if (deviceId.isEmpty()) {
        return PipeWireNodeRecord{};   // follow the system default
    }
    return std::nullopt;
}

std::unique_ptr<IAudioBus> PipeWireDeviceBackend::createOutput(const AudioStreamRequest& request)
{
    if (!m_system) {
        return nullptr;
    }
    const std::optional<PipeWireNodeRecord> node = nodeFor(request.deviceId,
                                                           AudioDeviceDirection::Output);
    if (!node) {
        qCWarning(lcAudio) << "PipeWire does not list the output" << request.deviceId;
        return nullptr;
    }
    return m_system->createOutput(*node, request);
}

std::unique_ptr<IAudioInputStream> PipeWireDeviceBackend::createInput(const AudioStreamRequest& request,
                                                                      MicChannelPick pick,
                                                                      IAudioInputSink* sink)
{
    if (!m_system) {
        return nullptr;
    }
    const std::optional<PipeWireNodeRecord> node = nodeFor(request.deviceId,
                                                           AudioDeviceDirection::Input);
    if (!node) {
        qCWarning(lcAudio) << "PipeWire does not list the input" << request.deviceId;
        return nullptr;
    }
    return m_system->createInput(*node, request, pick, sink);
}

} // namespace NereusSDR
