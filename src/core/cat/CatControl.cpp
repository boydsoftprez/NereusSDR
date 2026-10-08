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
//                                    desktop, the client side). Review
//                                    fixes: one signal per kind of change,
//                                    kept settings dropped by property,
//                                    explicit rebinds, the tester's reply
//                                    by command, the log's backlog, the
//                                    status bar's indicator.
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

// ── CatControl ───────────────────────────────────────────────────────────

CatIndicator CatControl::indicator() const
{
    CatIndicator indicator;
    const bool isAvailable = available();
    bool listening = false;
    bool error = false;
    int clients = 0;
    if (remote()) {
        indicator.details.append(tr("CAT on the Core's computer:"));
    }
    for (int channel = 1; channel <= 4 && isAvailable; ++channel) {
        const CatChannelStatus status = channelStatus(channel);
        listening = listening || status.listening;
        clients += status.tcpClients + status.rigctldClients;
        error = error || status.state.contains(QStringLiteral("error"), Qt::CaseInsensitive)
            || status.state.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive);
        indicator.details.append(tr("CAT%1: %2").arg(channel).arg(status.state));
    }
    if (!isAvailable) {
        indicator.details.append(unavailableReason());
    }
    indicator.text = error ? tr("Error") : listening ? tr("On (%1)").arg(clients) : tr("Off");
    return indicator;
}

// ── LocalCatControl ──────────────────────────────────────────────────────

LocalCatControl::LocalCatControl(RadioModel* model, CatService* service, QObject* parent)
    : CatControl(parent), m_model(model), m_service(service), m_serialDevices(serialDevicesHere())
{
    if (!service) {
        return;
    }
    // Each CatService change as the kind of change the pages listen for.
    const auto status = [this](int channel) { emit channelStatusChanged(channel); };
    connect(service, &CatService::configurationChanged, this, &CatControl::channelConfigChanged);
    connect(service, &CatService::channelStateChanged, this, status);
    connect(service, &CatService::clientCountChanged, this, status);
    connect(service, &CatService::rigctldClientCountChanged, this, status);
    connect(service, &CatService::ptyPathChanged, this, status);
    connect(service, &CatService::transportStateChanged, this, status);
    connect(service, &CatService::globalConfigurationChanged, this,
            &CatControl::globalConfigChanged);
    connect(service, &CatService::pttStateChanged, this, &CatControl::pttStateChanged);
    connect(service, &CatService::messageLogged, this,
            [this](int channel, bool inbound, const QByteArray& bytes) {
        emit logged(channel, inbound, bytes, QDateTime::currentMSecsSinceEpoch());
    });
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
                                         ResultCallback done, QObject* /*shownOn*/,
                                         CatRebind /*rebind*/)
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

void LocalCatControl::testCommand(int channel, const QByteArray& command, ReplyCallback done,
                                  QObject* /*shownOn*/)
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
        emit platformChanged();
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
                                          ResultCallback done, QObject* shownOn,
                                          CatRebind rebind)
{
    IStationLink* station = link();
    if (!station || !available()) {
        if (done) {
            done(false, unavailableReason());
        }
        return;
    }
    // The binding goes as slice ids, with the ones just picked; the Core
    // resolves them.
    const auto outcome = station->requestStationCatChannel(
        channel, StationCatModel::toText(
                     StationCatModel::channelCommandToJson(config, rebind.primary,
                                                           rebind.secondary)));
    if (outcome.sent && outcome.commandId != 0 && channel >= 1
        && channel <= int(m_sentChannel.size())) {
        m_sentChannel[channel - 1] = config;
        m_sentState[channel - 1] = Unconfirmed{outcome.commandId, false};
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
        m_sentState[kGlobalSlot] = Unconfirmed{outcome.commandId, false};
    }
    track(outcome.sent, outcome.reason, outcome.commandId, std::move(done), shownOn);
}

void RemoteCatControl::testCommand(int channel, const QByteArray& command, ReplyCallback done,
                                   QObject* shownOn)
{
    IStationLink* station = link();
    if (!station || !available()) {
        if (done) {
            done(false, {}, unavailableReason());
        }
        return;
    }
    // The request id names the test in lastTest, for other windows.
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
    if (outcome.commandId == 0) {
        return;
    }
    // The reply comes in the command's result, by its id.
    if (done) {
        m_tests.insert(outcome.commandId, std::move(done));
    }
    if (shownOn && m_model) {
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, shownOn);
    }
}

void RemoteCatControl::refreshDevices()
{
    if (IStationLink* station = link(); station && available()) {
        station->requestStationCatRefreshDevices();
    }
}

void RemoteCatControl::applyTestReply(quint32 commandId, const QString& reply)
{
    const ReplyCallback waiting = m_tests.take(commandId);
    if (waiting) {
        waiting(true, reply.toLatin1(), QString());
    }
}

int RemoteCatControl::unconfirmedCount() const
{
    int count = m_sentGlobal ? 1 : 0;
    for (const std::optional<CatEndpointConfig>& sent : m_sentChannel) {
        count += sent ? 1 : 0;
    }
    return count;
}

void RemoteCatControl::applyLogBatch(const RecordBatch& batch)
{
    // Each line is shown once. The Core sends its recent lines again with
    // each subscription (after a new session too): those already shown
    // are skipped by their rising ids. A stream begun again (a new
    // generation, or a Core that started over and numbers from 1) starts
    // the count over.
    if (batch.generation != m_logGeneration) {
        m_logGeneration = batch.generation;
        m_lastLogId = 0;
    }
    if (batch.reset) {
        qint64 newest = 0;
        for (const RecordUpsert& upsert : batch.upserts) {
            newest = qMax(newest, upsert.id.toLongLong());
        }
        if (newest < m_lastLogId) {
            m_lastLogId = 0;
        }
    }
    const QPointer<RemoteCatControl> self(this);
    for (const RecordUpsert& upsert : batch.upserts) {
        const qint64 id = upsert.id.toLongLong();
        if (id <= m_lastLogId) {
            continue;
        }
        m_lastLogId = id;
        const int channel = upsert.fields.value(QStringLiteral("channel")).toInt();
        const bool inbound = upsert.fields.value(QStringLiteral("inbound")).toBool();
        // The Core sends the bytes as their Latin-1 text, and when it saw
        // them.
        const QByteArray bytes = upsert.fields.value(QStringLiteral("text")).toString().toLatin1();
        const qint64 time = upsert.fields.value(QStringLiteral("time")).toInteger();
        emit logged(channel, inbound, bytes, time > 0 ? time : QDateTime::currentMSecsSinceEpoch());
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
    if (wanted && !m_logFollowed) {
        // A window opened (again): it is sent the Core's recent lines.
        m_lastLogId = 0;
    }
    m_logLink = station;
    m_logFollowed = wanted;
    if (tell) {
        // As many recent lines as the window keeps.
        station->requestCatLog(wanted, kLogLines);
    }
}

bool RemoteCatControl::mirrorHolds(int slot) const
{
    if (!m_model || !m_model->stationCatModel()) {
        return false;
    }
    const StationCatModel* station = m_model->stationCatModel();
    if (slot == kGlobalSlot) {
        return m_sentGlobal
            && station->globalObject().value(QStringLiteral("config")).toObject()
                == StationCatModel::globalConfigToJson(*m_sentGlobal);
    }
    const std::optional<CatEndpointConfig>& sent = m_sentChannel[std::size_t(slot)];
    return sent
        && station->channelObject(slot + 1).value(QStringLiteral("config")).toObject()
            == StationCatModel::channelConfigToJson(*sent);
}

void RemoteCatControl::drop(int slot)
{
    if (slot == kGlobalSlot) {
        m_sentGlobal.reset();
    } else {
        m_sentChannel[std::size_t(slot)].reset();
    }
    m_sentState[std::size_t(slot)] = {};
}

void RemoteCatControl::dropAll()
{
    for (int slot = 0; slot <= kGlobalSlot; ++slot) {
        drop(slot);
    }
}

int RemoteCatControl::settle(quint32 commandId, bool accepted)
{
    if (commandId == 0) {
        return -1;
    }
    for (int slot = 0; slot <= kGlobalSlot; ++slot) {
        Unconfirmed& state = m_sentState[std::size_t(slot)];
        if (state.commandId != commandId) {
            continue;
        }
        // Refused: the Core's settings stand. Accepted: the Core's change
        // follows its answer, unless the mirror already holds what was sent
        // (a change that changes nothing sends none).
        if (!accepted) {
            drop(slot);
            return slot;
        }
        if (mirrorHolds(slot)) {
            drop(slot);
            return -1;
        }
        state.accepted = true;
        return -1;
    }
    return -1;
}

bool RemoteCatControl::settleOnDelta(int slot)
{
    if (!m_sentState[std::size_t(slot)].accepted) {
        return false;
    }
    drop(slot);
    return true;
}

void RemoteCatControl::emitConfigChanged(int slot)
{
    if (slot == kGlobalSlot) {
        emit globalConfigChanged();
    } else if (slot >= 0) {
        emit channelConfigChanged(slot + 1);
    }
}

void RemoteCatControl::onStateChanged()
{
    // A delta's properties are applied one at a time: its changes are
    // read and told once all of them are in, so a page sees them together.
    if (m_stateQueued) {
        return;
    }
    m_stateQueued = true;
    QMetaObject::invokeMethod(this, &RemoteCatControl::applyState, Qt::QueuedConnection);
}

void RemoteCatControl::applyState()
{
    m_stateQueued = false;
    if (!m_model || !m_model->stationCatModel()) {
        return;
    }
    const StationCatModel* station = m_model->stationCatModel();
    // One delta may carry several properties: each is read now, then the
    // changes are told, by kind. lastTest changes nothing a page shows.
    bool globalConfig = false;
    bool ptt = false;
    std::array<bool, 4> channelConfig{};
    std::array<bool, 4> channelStatus{};
    const QJsonObject global = station->globalObject();
    if (global != m_seenGlobal) {
        globalConfig = global.value(QStringLiteral("config")) != m_seenGlobal.value(QStringLiteral("config"));
        ptt = global.value(QStringLiteral("pttState")) != m_seenGlobal.value(QStringLiteral("pttState"));
        m_seenGlobal = global;
        // The Core's change to the property a kept setting covers.
        globalConfig = settleOnDelta(kGlobalSlot) || globalConfig;
    }
    for (std::size_t i = 0; i < m_seenChannel.size(); ++i) {
        const QJsonObject now = station->channelObject(int(i) + 1);
        QJsonObject& was = m_seenChannel[i];
        if (now == was) {
            continue;
        }
        channelConfig[i] = now.value(QStringLiteral("config")) != was.value(QStringLiteral("config"));
        channelStatus[i] = now.value(QStringLiteral("status")) != was.value(QStringLiteral("status"))
            || now.value(QStringLiteral("primaryValid")) != was.value(QStringLiteral("primaryValid"))
            || now.value(QStringLiteral("secondaryValid"))
                != was.value(QStringLiteral("secondaryValid"));
        was = now;
        channelConfig[i] = settleOnDelta(int(i)) || channelConfig[i];
    }
    const QJsonObject platformNow = station->platformObject();
    const bool platform = platformNow != m_seenPlatform;
    m_seenPlatform = platformNow;

    const QPointer<RemoteCatControl> self(this);
    if (globalConfig) {
        emit globalConfigChanged();
        if (!self) { return; }
    }
    if (ptt) {
        emit pttStateChanged();
        if (!self) { return; }
    }
    for (std::size_t i = 0; i < m_seenChannel.size(); ++i) {
        if (channelConfig[i]) {
            emit channelConfigChanged(int(i) + 1);
            if (!self) { return; }
        }
        if (channelStatus[i]) {
            emit channelStatusChanged(int(i) + 1);
            if (!self) { return; }
        }
    }
    if (platform) {
        emit platformChanged();
    }
}

void RemoteCatControl::onLinkChanged()
{
    const QPointer<RemoteCatControl> self(this);
    const IStationLink* station = link();
    if (!station || !station->stationLinkReady()) {
        // No answer will come over a link that is down.
        m_pending.clear();
        m_tests.clear();
        dropAll();
    }
    followLog();
    if (!self) {
        return;
    }
    emit availabilityChanged();
}

void RemoteCatControl::onCommandFinished(quint32 commandId, bool accepted, const QString& reason)
{
    const QPointer<RemoteCatControl> self(this);
    const int dropped = settle(commandId, accepted);
    if (dropped >= 0) {
        // The Core's settings stand again.
        emitConfigChanged(dropped);
        if (!self) {
            return;
        }
    }
    // A test the Core refused, or accepted without a reply.
    if (const ReplyCallback test = m_tests.take(commandId)) {
        test(accepted, {}, accepted ? QString() : reason);
        if (!self) {
            return;
        }
    }
    const ResultCallback done = m_pending.take(commandId);
    if (done) {
        done(accepted, reason);
    }
}

} // namespace NereusSDR
