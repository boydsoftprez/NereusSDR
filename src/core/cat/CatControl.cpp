// no-port-check: NereusSDR-original. What the CAT pages, the CAT applet, the
// CAT log window and the status bar read and change, in either role.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/cat/CatControl.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See CatControl.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, the client side).
//                                    AI tooling: Claude Code.
// =================================================================

#include "core/cat/CatControl.h"

#include "core/SliceOwnership.h"
#include "core/cat/CatService.h"
#include "core/session/IStationLink.h"
#include "core/session/RecordStream.h"
#include "models/RadioModel.h"
#include "models/StationCatModel.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaMethod>

#ifdef HAVE_SERIALPORT
#include <QSerialPortInfo>
#endif

namespace NereusSDR {

namespace {

const QString kListening = QStringLiteral("Listening");

QString addressText(const QHostAddress& address)
{
    return address.isNull() ? QString() : address.toString();
}

QStringList serialDevicesHere()
{
    QStringList devices;
#ifdef HAVE_SERIALPORT
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        devices.append(port.systemLocation());
    }
#endif
    return devices;
}

CatEndpointConfig defaultChannel(int channel)
{
    CatEndpointConfig config;
    config.channel = channel;
    return config;
}

} // namespace

// ── LocalCatControl ──────────────────────────────────────────────────────

LocalCatControl::LocalCatControl(RadioModel* model, CatService* service, QObject* parent)
    : CatControl(parent), m_model(model), m_service(service), m_serialDevices(serialDevicesHere())
{
    if (!service) {
        return;
    }
    const auto notify = [this] { emit changed(); };
    connect(service, &CatService::configurationChanged, this, notify);
    connect(service, &CatService::channelStateChanged, this, notify);
    connect(service, &CatService::clientCountChanged, this, notify);
    connect(service, &CatService::rigctldClientCountChanged, this, notify);
    connect(service, &CatService::ptyPathChanged, this, notify);
    connect(service, &CatService::transportStateChanged, this, notify);
    connect(service, &CatService::globalConfigurationChanged, this, notify);
    connect(service, &CatService::autoInformationChanged, this, notify);
    connect(service, &CatService::pttStateChanged, this, notify);
    connect(service, &CatService::messageLogged, this, &CatControl::logged);
}

CatEndpointConfig LocalCatControl::channelConfig(int channel) const
{
    return m_service ? m_service->channelConfig(channel) : defaultChannel(channel);
}

CatChannelStatus LocalCatControl::channelStatus(int channel) const
{
    CatChannelStatus status;
    if (!m_service) {
        return status;
    }
    const CatEndpointConfig config = m_service->channelConfig(channel);
    status.state = m_service->channelState(channel);
    status.tcp = m_service->transportState(channel, CatTransportKind::Tcp);
    status.serial = m_service->transportState(channel, CatTransportKind::Serial);
    status.pty = m_service->transportState(channel, CatTransportKind::Pty);
    status.rigctld = m_service->transportState(channel, CatTransportKind::Rigctld);
    status.tcpBoundAddress = addressText(m_service->boundAddress(channel));
    status.tcpBoundPort = m_service->boundPort(channel);
    status.rigctldBoundAddress = addressText(m_service->rigctldBoundAddress(channel));
    status.rigctldBoundPort = m_service->rigctldBoundPort(channel);
    status.tcpClients = m_service->clientCount(channel);
    status.rigctldClients = m_service->rigctldClientCount(channel);
    status.ptyPath = m_service->ptySlavePath(channel);
    status.listening = m_service->isListening(channel);
    // A binding is valid while its slice is the one it was bound to; no
    // slice is valid. The same test the Core publishes as primaryValid.
    const SliceOwnership* ownership = m_model ? m_model->sliceOwnership() : nullptr;
    const auto valid = [ownership](int sliceId, quint64 incarnation) {
        if (sliceId < 0) {
            return true;
        }
        return ownership != nullptr && incarnation != 0
            && ownership->incarnation(sliceId) == incarnation;
    };
    status.primaryValid = valid(config.binding.primarySliceId, config.binding.primaryIncarnation);
    status.secondaryValid = !config.binding.secondarySliceId
        || valid(*config.binding.secondarySliceId, config.binding.secondaryIncarnation.value_or(0));
    return status;
}

CatGlobalConfig LocalCatControl::globalConfig() const
{
    return m_service ? m_service->globalConfig() : CatGlobalConfig{};
}

QString LocalCatControl::pttState() const
{
    return m_service ? m_service->pttState() : QString();
}

CatPlatform LocalCatControl::platform() const
{
    CatPlatform platform;
#ifdef HAVE_SERIALPORT
    platform.serial = true;
#endif
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    platform.pty = true;
#endif
    // Mark and space parity are refused by QtSerialPort on macOS, and 1.5
    // stop bits exist only on Windows.
#if !defined(Q_OS_MAC)
    platform.markSpaceParity = true;
#endif
#if defined(Q_OS_WIN)
    platform.oneAndHalfStop = true;
#endif
    platform.serialDevices = m_serialDevices;
    return platform;
}

bool LocalCatControl::available() const
{
    return !m_service.isNull();
}

QString LocalCatControl::unavailableReason() const
{
    return {};
}

void LocalCatControl::reconfigureChannel(int channel, const CatEndpointConfig& config,
                                         ResultCallback done, QObject* /*shownOn*/)
{
    if (!m_service) {
        return;
    }
    const bool accepted = m_service->reconfigureChannel(channel, config);
    if (done) {
        // The local page's own words, the same the Core sends.
        done(accepted, accepted ? QString()
                                : QStringLiteral("Configuration refused: check address, port, "
                                                 "format and exclusive device assignment."));
    }
}

void LocalCatControl::reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                                        QObject* /*shownOn*/)
{
    if (!m_service) {
        return;
    }
    const bool accepted = m_service->reconfigureGlobal(config);
    if (done) {
        done(accepted, accepted ? QString()
                                : QStringLiteral("Configuration refused: check PTT source, "
                                                 "sampled inputs and device assignment."));
    }
}

void LocalCatControl::testCommand(int channel, const QByteArray& command, ReplyCallback done)
{
    if (!m_service) {
        return;
    }
    const QByteArray reply = m_service->testCommand(channel, command);
    if (done) {
        done(true, reply, QString());
    }
}

void LocalCatControl::refreshDevices()
{
    const QStringList devices = serialDevicesHere();
    if (devices != m_serialDevices) {
        m_serialDevices = devices;
        emit changed();
    }
}

// ── RemoteCatControl ─────────────────────────────────────────────────────

RemoteCatControl::RemoteCatControl(RadioModel* model, QObject* parent)
    : CatControl(parent), m_model(model)
{
    // Test request ids unlikely to meet another window's on the same Core.
    m_nextTestId = QDateTime::currentMSecsSinceEpoch() * 1000;
    if (!model) {
        return;
    }
    if (StationCatModel* station = model->stationCatModel()) {
        connect(station, &StationCatModel::stateChanged, this, &RemoteCatControl::onStateChanged);
    }
    connect(model, &RadioModel::stationLinkStateChanged, this, &RemoteCatControl::onLinkChanged);
    connect(model, &RadioModel::stationCommandFinished, this,
            &RemoteCatControl::onCommandFinished);
}

QString RemoteCatControl::notConnectedReason()
{
    return QStringLiteral("Connect to the Core to set up its CAT.");
}

IStationLink* RemoteCatControl::link() const
{
    return m_model ? m_model->stationLink() : nullptr;
}

CatEndpointConfig RemoteCatControl::channelConfig(int channel) const
{
    if (channel >= 1 && channel <= int(m_sentChannel.size()) && m_sentChannel[channel - 1]) {
        return *m_sentChannel[channel - 1];
    }
    const CatEndpointConfig base = defaultChannel(channel);
    if (!m_model || !m_model->stationCatModel()) {
        return base;
    }
    const QJsonObject config = m_model->stationCatModel()->channelObject(channel)
                                   .value(QStringLiteral("config")).toObject();
    return StationCatModel::channelConfigFromJson(config, base).value_or(base);
}

CatChannelStatus RemoteCatControl::channelStatus(int channel) const
{
    CatChannelStatus status;
    if (!m_model || !m_model->stationCatModel()) {
        return status;
    }
    const QJsonObject object = m_model->stationCatModel()->channelObject(channel);
    const QJsonObject live = object.value(QStringLiteral("status")).toObject();
    status.state = live.value(QStringLiteral("state")).toString();
    status.tcp = live.value(QStringLiteral("tcp")).toString();
    status.serial = live.value(QStringLiteral("serial")).toString();
    status.pty = live.value(QStringLiteral("pty")).toString();
    status.rigctld = live.value(QStringLiteral("rigctld")).toString();
    status.tcpBoundAddress = live.value(QStringLiteral("tcpBoundAddress")).toString();
    status.tcpBoundPort = live.value(QStringLiteral("tcpBoundPort")).toInt();
    status.rigctldBoundAddress = live.value(QStringLiteral("rigctldBoundAddress")).toString();
    status.rigctldBoundPort = live.value(QStringLiteral("rigctldBoundPort")).toInt();
    status.tcpClients = live.value(QStringLiteral("tcpClients")).toInt();
    status.rigctldClients = live.value(QStringLiteral("rigctldClients")).toInt();
    status.ptyPath = live.value(QStringLiteral("ptyPath")).toString();
    status.listening = status.tcp == kListening || status.serial == kListening
        || status.pty == kListening || status.rigctld == kListening;
    status.primaryValid = object.value(QStringLiteral("primaryValid")).toBool(true);
    status.secondaryValid = object.value(QStringLiteral("secondaryValid")).toBool(true);
    return status;
}

CatGlobalConfig RemoteCatControl::globalConfig() const
{
    if (m_sentGlobal) {
        return *m_sentGlobal;
    }
    if (!m_model || !m_model->stationCatModel()) {
        return {};
    }
    const QJsonObject config = m_model->stationCatModel()->globalObject()
                                   .value(QStringLiteral("config")).toObject();
    return StationCatModel::globalConfigFromJson(config, CatGlobalConfig{})
        .value_or(CatGlobalConfig{});
}

QString RemoteCatControl::pttState() const
{
    if (!m_model || !m_model->stationCatModel()) {
        return {};
    }
    return m_model->stationCatModel()->globalObject().value(QStringLiteral("pttState")).toString();
}

CatPlatform RemoteCatControl::platform() const
{
    CatPlatform platform;
    if (!m_model || !m_model->stationCatModel()) {
        return platform;
    }
    const QJsonObject object = m_model->stationCatModel()->platformObject();
    platform.serial = object.value(QStringLiteral("serial")).toBool();
    platform.pty = object.value(QStringLiteral("pty")).toBool();
    platform.markSpaceParity = object.value(QStringLiteral("markSpaceParity")).toBool();
    platform.oneAndHalfStop = object.value(QStringLiteral("oneAndHalfStop")).toBool();
    for (const QJsonValue& device : object.value(QStringLiteral("serialDevices")).toArray()) {
        platform.serialDevices.append(device.toString());
    }
    return platform;
}

bool RemoteCatControl::available() const
{
    const IStationLink* station = link();
    return station && station->stationLinkReady() && station->stationCatAvailable();
}

QString RemoteCatControl::unavailableReason() const
{
    const IStationLink* station = link();
    if (!station || !station->stationLinkReady()) {
        return notConnectedReason();
    }
    if (!station->stationCatAvailable()) {
        return IStationLink::stationCatUnavailableReason();
    }
    return {};
}

void RemoteCatControl::track(bool sent, const QString& reason, quint32 commandId,
                             ResultCallback done, QObject* shownOn)
{
    if (!sent) {
        if (done) {
            done(false, reason);
        }
        return;
    }
    if (commandId == 0) {
        return;
    }
    if (done) {
        m_pending.insert(commandId, std::move(done));
    }
    if (shownOn && m_model) {
        m_model->noteAccessoryRequestShownOnPage(commandId, shownOn);
    }
}

void RemoteCatControl::reconfigureChannel(int channel, const CatEndpointConfig& config,
                                          ResultCallback done, QObject* shownOn)
{
    IStationLink* station = link();
    if (!station || !available()) {
        if (done) {
            done(false, unavailableReason());
        }
        return;
    }
    // The binding goes as slice ids only; the Core resolves them.
    const auto outcome = station->requestStationCatChannel(
        channel, StationCatModel::toText(StationCatModel::channelConfigToJson(config)));
    if (outcome.sent && outcome.commandId != 0 && channel >= 1
        && channel <= int(m_sentChannel.size())) {
        m_sentChannel[channel - 1] = config;
        m_sentChannelState[channel - 1] = Unconfirmed{outcome.commandId, false};
    }
    track(outcome.sent, outcome.reason, outcome.commandId, std::move(done), shownOn);
}

void RemoteCatControl::reconfigureGlobal(const CatGlobalConfig& config, ResultCallback done,
                                         QObject* shownOn)
{
    IStationLink* station = link();
    if (!station || !available()) {
        if (done) {
            done(false, unavailableReason());
        }
        return;
    }
    const auto outcome = station->requestStationCatGlobal(
        StationCatModel::toText(StationCatModel::globalConfigToJson(config)));
    if (outcome.sent && outcome.commandId != 0) {
        m_sentGlobal = config;
        m_sentGlobalState = Unconfirmed{outcome.commandId, false};
    }
    track(outcome.sent, outcome.reason, outcome.commandId, std::move(done), shownOn);
}

void RemoteCatControl::testCommand(int channel, const QByteArray& command, ReplyCallback done)
{
    IStationLink* station = link();
    if (!station || !available()) {
        if (done) {
            done(false, {}, unavailableReason());
        }
        return;
    }
    const qint64 requestId = ++m_nextTestId;
    // The local tester sends the typed text as Latin-1 bytes; the Core
    // turns the text back into them.
    const auto outcome =
        station->requestStationCatTest(requestId, channel, QString::fromLatin1(command));
    if (!outcome.sent) {
        if (done) {
            done(false, {}, outcome.reason);
        }
        return;
    }
    if (!done) {
        return;
    }
    m_tests.insert(requestId, std::move(done));
    // A refusal from the Core ends the wait for the reply.
    if (outcome.commandId != 0) {
        m_pending.insert(outcome.commandId, [this, requestId](bool accepted,
                                                              const QString& reason) {
            if (accepted) {
                return;
            }
            const ReplyCallback waiting = m_tests.take(requestId);
            if (waiting) {
                waiting(false, {}, reason);
            }
        });
    }
}

void RemoteCatControl::refreshDevices()
{
    if (IStationLink* station = link(); station && available()) {
        station->requestStationCatRefreshDevices();
    }
}

void RemoteCatControl::applyLogBatch(const RecordBatch& batch)
{
    const QPointer<RemoteCatControl> self(this);
    for (const RecordUpsert& upsert : batch.upserts) {
        const int channel = upsert.fields.value(QStringLiteral("channel")).toInt();
        const bool inbound = upsert.fields.value(QStringLiteral("inbound")).toBool();
        // The Core sends the bytes as their Latin-1 text.
        const QByteArray bytes = upsert.fields.value(QStringLiteral("text")).toString().toLatin1();
        emit logged(channel, inbound, bytes);
        if (!self) {
            return;
        }
    }
}

void RemoteCatControl::connectNotify(const QMetaMethod& signal)
{
    if (signal == QMetaMethod::fromSignal(&CatControl::logged)) {
        followLogLater();
    }
    CatControl::connectNotify(signal);
}

void RemoteCatControl::disconnectNotify(const QMetaMethod& signal)
{
    // An invalid method: everything was disconnected at once.
    if (!signal.isValid() || signal == QMetaMethod::fromSignal(&CatControl::logged)) {
        followLogLater();
    }
    CatControl::disconnectNotify(signal);
}

void RemoteCatControl::followLogLater()
{
    // Qt may hold its connection lock around connectNotify() and
    // disconnectNotify(), and isSignalConnected() takes it: look after
    // they return.
    QMetaObject::invokeMethod(this, &RemoteCatControl::followLog, Qt::QueuedConnection);
}

void RemoteCatControl::followLog()
{
    // The Core's log is followed only while something listens for it (the
    // CAT log window). The link subscribes again after each new session.
    const bool wanted = isSignalConnected(QMetaMethod::fromSignal(&CatControl::logged));
    IStationLink* station = link();
    if (station == m_logLink && wanted == m_logFollowed) {
        return;
    }
    // A new link is told only when the log is wanted; a link this control
    // no longer uses is left alone (it may be gone).
    const bool tell = station && (wanted || (station == m_logLink && m_logFollowed));
    m_logLink = station;
    m_logFollowed = wanted;
    if (tell) {
        station->requestCatLog(wanted);
    }
}

void RemoteCatControl::onStateChanged()
{
    const QPointer<RemoteCatControl> self(this);
    // The Core sends its change after its answer: an accepted setting is
    // in the mirror now.
    for (std::size_t i = 0; i < m_sentChannel.size(); ++i) {
        if (m_sentChannelState[i].accepted) {
            m_sentChannel[i].reset();
            m_sentChannelState[i] = {};
        }
    }
    if (m_sentGlobalState.accepted) {
        m_sentGlobal.reset();
        m_sentGlobalState = {};
    }
    // A test command's reply, by the request id this window sent.
    if (!m_tests.isEmpty() && m_model && m_model->stationCatModel()) {
        const QJsonObject last = m_model->stationCatModel()->lastTestObject();
        const qint64 requestId = last.value(QStringLiteral("requestId")).toInteger();
        const ReplyCallback waiting = m_tests.take(requestId);
        if (waiting) {
            waiting(true, last.value(QStringLiteral("reply")).toString().toLatin1(), QString());
            if (!self) {
                return;
            }
        }
    }
    emit changed();
}

void RemoteCatControl::onLinkChanged()
{
    const QPointer<RemoteCatControl> self(this);
    const IStationLink* station = link();
    if (!station || !station->stationLinkReady()) {
        // No answer will come over a link that is down.
        m_pending.clear();
        m_tests.clear();
        for (std::size_t i = 0; i < m_sentChannel.size(); ++i) {
            m_sentChannel[i].reset();
            m_sentChannelState[i] = {};
        }
        m_sentGlobal.reset();
        m_sentGlobalState = {};
    }
    followLog();
    if (!self) {
        return;
    }
    emit changed();
}

void RemoteCatControl::settle(quint32 commandId, bool accepted)
{
    // A refused setting is dropped at once; an accepted one when the
    // Core's change reaches the mirror.
    const auto answer = [commandId, accepted](Unconfirmed& state) {
        if (state.commandId != commandId) {
            return false;
        }
        if (accepted) {
            state.accepted = true;
            return false;
        }
        state = {};
        return true;
    };
    for (std::size_t i = 0; i < m_sentChannel.size(); ++i) {
        if (answer(m_sentChannelState[i])) {
            m_sentChannel[i].reset();
        }
    }
    if (answer(m_sentGlobalState)) {
        m_sentGlobal.reset();
    }
}

void RemoteCatControl::onCommandFinished(quint32 commandId, bool accepted, const QString& reason)
{
    settle(commandId, accepted);
    const ResultCallback done = m_pending.take(commandId);
    if (done) {
        done(accepted, reason);
    }
}

} // namespace NereusSDR
