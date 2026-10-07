// no-port-check: NereusSDR-original. See StationCatController.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/cat/StationCatController.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, stationCatVersion 1).
//                                    Review fixes: explicit rebinds, the
//                                    tester's reply in its result, device
//                                    reads at most once a second.
//                                    AI tooling: Claude Code.
// =================================================================

#include "core/cat/StationCatController.h"

#include "core/SliceOwnership.h"
#include "core/cat/CatService.h"
#include "models/RadioModel.h"
#include "models/StationCatModel.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#ifdef HAVE_SERIALPORT
#include <QSerialPortInfo>
#endif

namespace NereusSDR {

namespace {

QString addressText(const QHostAddress& address)
{
    return address.isNull() ? QString() : address.toString();
}

} // namespace

QString StationCatController::channelRefusedReason()
{
    return QStringLiteral("Configuration refused: check address, port, format and exclusive "
                          "device assignment.");
}

QString StationCatController::globalRefusedReason()
{
    return QStringLiteral("Configuration refused: check PTT source, sampled inputs and device "
                          "assignment.");
}

StationCatController::StationCatController(RadioModel* model, CatService* service,
                                           StationCatModel* station, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_service(service)
    , m_station(station)
    , m_deviceRefresh(new QTimer(this))
{
    m_deviceRefresh->setSingleShot(true);
    connect(m_deviceRefresh, &QTimer::timeout, this, &StationCatController::readDevices);
    if (m_service) {
        // Every change CatService reports republishes; each property only
        // changes (and is sent) when its text does.
        const auto all = [this]() { publishAll(); };
        connect(m_service, &CatService::configurationChanged, this, all);
        connect(m_service, &CatService::globalConfigurationChanged, this, all);
        connect(m_service, &CatService::channelStateChanged, this, all);
        connect(m_service, &CatService::transportStateChanged, this, all);
        connect(m_service, &CatService::clientCountChanged, this, all);
        connect(m_service, &CatService::rigctldClientCountChanged, this, all);
        connect(m_service, &CatService::ptyPathChanged, this, all);
        connect(m_service, &CatService::pttStateChanged, this, all);
        connect(m_service, &CatService::autoInformationChanged, this, all);
    }
    if (m_model) {
        // A slice opened or closed changes whether a binding is still live.
        connect(m_model, &RadioModel::sliceAdded, this, [this]() { publishAll(); });
        connect(m_model, &RadioModel::sliceRemoved, this, [this]() { publishAll(); });
    }
    publishAll();
}

bool StationCatController::setChannel(int channel, const QString& configJson, QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason) {
            *reason = why;
        }
        return false;
    };
    if (!m_service || !m_model || channel < 1 || channel > StationCatModel::kChannels) {
        return refuse(channelRefusedReason());
    }
    const QJsonDocument document = QJsonDocument::fromJson(configJson.toUtf8());
    const CatEndpointConfig current = m_service->channelConfig(channel);
    const std::optional<CatEndpointConfig> parsed = document.isObject()
        ? StationCatModel::channelConfigFromJson(document.object(), current)
        : std::nullopt;
    bool primaryRebind = false;
    bool secondaryRebind = false;
    if (!parsed
        || !StationCatModel::channelRebindFromJson(document.object(), &primaryRebind,
                                                   &secondaryRebind)) {
        return refuse(QStringLiteral("The CAT channel's settings were not understood."));
    }
    CatEndpointConfig config = *parsed;
    config.channel = channel;
    // The binding arrives as slice ids. A different slice, or one the
    // window says was just picked, binds to its live incarnation on the
    // Core now; the slice the channel already holds otherwise keeps the
    // incarnation it was bound with (CatService's transport-only edit), so
    // a closed slice's id is never silently rebound by an unrelated edit.
    const SliceOwnership* ownership = m_model->sliceOwnership();
    const auto live = [ownership](int sliceId) -> quint64 {
        return ownership != nullptr && sliceId >= 0 ? ownership->incarnation(sliceId) : 0;
    };
    if (primaryRebind || config.binding.primarySliceId != current.binding.primarySliceId) {
        config.binding.primaryIncarnation = live(config.binding.primarySliceId);
    } else {
        config.binding.primaryIncarnation = current.binding.primaryIncarnation;
    }
    if (!config.binding.secondarySliceId) {
        config.binding.secondaryIncarnation.reset();
    } else if (secondaryRebind
               || config.binding.secondarySliceId != current.binding.secondarySliceId) {
        config.binding.secondaryIncarnation = live(*config.binding.secondarySliceId);
    } else {
        config.binding.secondaryIncarnation = current.binding.secondaryIncarnation;
    }
    const QPointer<StationCatController> self(this);
    const bool accepted = m_service->reconfigureChannel(channel, config);
    if (!self) {
        return false;
    }
    publishAll();
    return accepted ? true : refuse(channelRefusedReason());
}

bool StationCatController::setGlobal(const QString& configJson, QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason) {
            *reason = why;
        }
        return false;
    };
    if (!m_service) {
        return refuse(globalRefusedReason());
    }
    const QJsonDocument document = QJsonDocument::fromJson(configJson.toUtf8());
    const std::optional<CatGlobalConfig> parsed = document.isObject()
        ? StationCatModel::globalConfigFromJson(document.object(), m_service->globalConfig())
        : std::nullopt;
    if (!parsed) {
        return refuse(QStringLiteral("The CAT settings were not understood."));
    }
    const QPointer<StationCatController> self(this);
    const bool accepted = m_service->reconfigureGlobal(*parsed);
    if (!self) {
        return false;
    }
    publishAll();
    return accepted ? true : refuse(globalRefusedReason());
}

bool StationCatController::testCommand(qint64 requestId, int channel, const QString& command,
                                       QString* reply, QString* reason)
{
    if (!m_service || channel < 1 || channel > StationCatModel::kChannels) {
        if (reason) {
            *reason = QStringLiteral("There is no such CAT channel.");
        }
        return false;
    }
    // The local tester sends the typed text as Latin-1 bytes.
    const QPointer<StationCatController> self(this);
    const QByteArray answer = m_service->testCommand(channel, command.toLatin1());
    if (!self || !m_station) {
        return false;
    }
    if (reply) {
        *reply = QString::fromLatin1(answer);
    }
    // Other windows see the last test; the one that sent it reads its
    // reply from the command's result.
    m_station->setLastTest(StationCatModel::toText(QJsonObject{
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("channel"), channel},
        {QStringLiteral("command"), command},
        {QStringLiteral("reply"), QString::fromLatin1(answer)},
        {QStringLiteral("accepted"), !answer.startsWith('?')},
    }));
    return true;
}

void StationCatController::refreshDevices()
{
    // Every window's page asks when it is shown: the devices are read at
    // most once a second, and a request sooner is answered when it is up.
    if (m_deviceRefresh->isActive()) {
        return;
    }
    if (!m_deviceClock.isValid() || m_deviceClock.elapsed() >= kDeviceRefreshMinimumMs) {
        readDevices();
        return;
    }
    m_deviceRefresh->start(int(kDeviceRefreshMinimumMs - m_deviceClock.elapsed()));
}

void StationCatController::setSerialDeviceListerForTest(std::function<QStringList()> lister)
{
    m_deviceLister = std::move(lister);
}

void StationCatController::readDevices()
{
    m_deviceClock.start();
    QStringList devices;
    if (m_deviceLister) {
        devices = m_deviceLister();
    } else {
#ifdef HAVE_SERIALPORT
        for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
            devices.append(port.systemLocation());
        }
#endif
    }
    m_serialDevices = devices;
    publishPlatform();
}

void StationCatController::publishAll()
{
    publishGlobal();
    for (int channel = 1; channel <= StationCatModel::kChannels; ++channel) {
        publishChannel(channel);
    }
    publishPlatform();
}

void StationCatController::publishGlobal()
{
    if (!m_station || !m_service) {
        return;
    }
    m_station->setGlobal(StationCatModel::toText(QJsonObject{
        {QStringLiteral("config"), StationCatModel::globalConfigToJson(m_service->globalConfig())},
        {QStringLiteral("aiActive"), m_service->autoInformationActive()},
        {QStringLiteral("pttState"), m_service->pttState()},
    }));
}

void StationCatController::publishChannel(int channel)
{
    if (!m_station || !m_service) {
        return;
    }
    const CatEndpointConfig config = m_service->channelConfig(channel);
    const SliceOwnership* ownership = m_model ? m_model->sliceOwnership() : nullptr;
    // Whether the slice the channel was bound to is still live: "Invalid
    // binding" in a window when it is not. No slice is a valid binding.
    const auto valid = [ownership](int sliceId, quint64 incarnation) {
        if (sliceId < 0) {
            return true;
        }
        return ownership != nullptr && incarnation != 0
            && ownership->incarnation(sliceId) == incarnation;
    };
    const bool primaryValid =
        valid(config.binding.primarySliceId, config.binding.primaryIncarnation);
    const bool secondaryValid = !config.binding.secondarySliceId
        || valid(*config.binding.secondarySliceId, config.binding.secondaryIncarnation.value_or(0));
    const QJsonObject status{
        {QStringLiteral("state"), m_service->channelState(channel)},
        {QStringLiteral("tcp"), m_service->transportState(channel, CatTransportKind::Tcp)},
        {QStringLiteral("serial"), m_service->transportState(channel, CatTransportKind::Serial)},
        {QStringLiteral("pty"), m_service->transportState(channel, CatTransportKind::Pty)},
        {QStringLiteral("rigctld"),
         m_service->transportState(channel, CatTransportKind::Rigctld)},
        {QStringLiteral("tcpBoundAddress"), addressText(m_service->boundAddress(channel))},
        {QStringLiteral("tcpBoundPort"), static_cast<int>(m_service->boundPort(channel))},
        {QStringLiteral("rigctldBoundAddress"),
         addressText(m_service->rigctldBoundAddress(channel))},
        {QStringLiteral("rigctldBoundPort"),
         static_cast<int>(m_service->rigctldBoundPort(channel))},
        {QStringLiteral("tcpClients"), m_service->clientCount(channel)},
        {QStringLiteral("rigctldClients"), m_service->rigctldClientCount(channel)},
        {QStringLiteral("ptyPath"), m_service->ptySlavePath(channel)},
    };
    m_station->setChannel(channel, StationCatModel::toText(QJsonObject{
        {QStringLiteral("config"), StationCatModel::channelConfigToJson(config)},
        {QStringLiteral("primaryValid"), primaryValid},
        {QStringLiteral("secondaryValid"), secondaryValid},
        {QStringLiteral("status"), status},
    }));
}

void StationCatController::publishPlatform()
{
    if (!m_station) {
        return;
    }
#ifdef HAVE_SERIALPORT
    constexpr bool kSerial = true;
#else
    constexpr bool kSerial = false;
#endif
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    constexpr bool kPty = true;
#else
    constexpr bool kPty = false;
#endif
    // Mark and space parity are refused on macOS, and 1.5 stop bits exist
    // only on Windows, as the local CAT pages offer them.
#if defined(Q_OS_MAC)
    constexpr bool kMarkSpaceParity = false;
#else
    constexpr bool kMarkSpaceParity = true;
#endif
#if defined(Q_OS_WIN)
    constexpr bool kOneAndHalfStop = true;
#else
    constexpr bool kOneAndHalfStop = false;
#endif
    m_station->setPlatform(StationCatModel::toText(QJsonObject{
        {QStringLiteral("serial"), kSerial},
        {QStringLiteral("pty"), kPty},
        {QStringLiteral("markSpaceParity"), kMarkSpaceParity},
        {QStringLiteral("oneAndHalfStop"), kOneAndHalfStop},
        {QStringLiteral("serialDevices"), QJsonArray::fromStringList(m_serialDevices)},
    }));
}

} // namespace NereusSDR
