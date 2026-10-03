// no-port-check: NereusSDR-original. R-R3-47 / R-R3-22 station-owned PGXL identity.
// Structure from StationTgxlController.cpp; captured wire evidence in
// StationPgxlController.h. J.J. Boyd (KG4VCF), September 2026; AI-assisted
// via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22: deviceSettings, the amp's own settings for
// a window. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47: an amp on another network is refused saying how to
// allow it (only this amp: its address or serial). J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 9): showSavedEndpoint. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "core/StationPgxlController.h"
#include "core/AppSettings.h"
#include "core/LanDiscovery.h"
#include "core/LogCategories.h"
#include "core/StationNetwork.h"
#include <QHostAddress>

namespace NereusSDR {
using Phase = AmplifierModel::ConnectionPhase;

namespace {
// PGXL_PingSec on the Core. Unset reads as off: the amp's answer to `ping`
// has never been captured (captures/*.pcapng carry no `ping` on TCP 9008),
// so the Core does not send one unless the station asks for it.
int stationPingSeconds()
{
    return qMax(0, AppSettings::instance().value(QStringLiteral("PGXL_PingSec"),
                                                 QStringLiteral("0")).toInt());
}
} // namespace

StationPgxlController::StationPgxlController(PgxlConnection* connection,
                                             AmplifierModel* model, QObject* parent)
    : QObject(parent), m_connection(connection), m_model(model)
{
    connection->setIdentityAdmissionRequired(true);
    connection->setAutoPingIntervalSec(stationPingSeconds());
    if (model) {
        // This controller, not the raw connection signals, reports the phase.
        model->setConnectionStateOwnedByController(true);
    }
    // R-R3-47 / R-R3-22: the amp's own settings, through the connection's
    // own command methods (the local Advanced page's commands).
    StationDeviceSettings::Wire wire;
    wire.connected = [this] { return m_connection && m_connection->isConnected(); };
    wire.writeSetup = [this](const QMap<QString, QString>& fields) {
        return m_connection ? m_connection->writeSetup(fields) : quint32(0);
    };
    wire.readSetup = [this] { return m_connection ? m_connection->readSetup() : quint32(0); };
    wire.writeIfconf = [this](const QString& ip, const QString& netmask,
                              const QString& gateway, bool dhcp) {
        return m_connection ? m_connection->writeIfconf(ip, netmask, gateway, dhcp)
                            : quint32(0);
    };
    wire.readIfconf = [this] { return m_connection ? m_connection->readIfconf() : quint32(0); };
    wire.save = [this] { return m_connection ? m_connection->save() : quint32(0); };
    m_settings = new StationDeviceSettings(StationDeviceSettings::Device::Pgxl, std::move(wire),
                                           this);
    connect(connection, &PgxlConnection::replyReceived, m_settings,
            &StationDeviceSettings::onReply);
    connect(connection, &PgxlConnection::disconnected, m_settings,
            &StationDeviceSettings::onDisconnected);
    connect(connection, &PgxlConnection::identityProtocolProgress, this,
            [this](quint64 token, const QString& peer, quint16 port, const QString& version) {
        if (!m_running) { return; }
        identify(token, peer, port);
        m_state.deviceVersion = version;
        publish();
    });
    connect(connection, &PgxlConnection::nativeInfoReceived, this,
            [this](const PgxlIdentityInfo& info) {
        if (!current(info.socketAttemptToken)) { return; }
        m_nativeInfo = info;
        m_state.deviceSerial = info.serial;
        if (!info.version.isEmpty()) { m_state.deviceVersion = info.version; }
        tryAdmit();
    });
    connect(connection, &PgxlConnection::connected, this, [this] {
        if (!current(m_attempt)) { return; }
        stopDiscovery();
        m_state.phase = Phase::Connected;
        m_state.error.clear();
        m_state.peerAddress = m_peer;
        publish();
    });
    connect(connection, &PgxlConnection::disconnected, this, [this] {
        if (!m_running) { return; }
        stopDiscovery();
        m_attempt = 0;
        if (m_connection && m_connection->reconnectPending()) {
            m_state.phase = Phase::Retrying;
        } else if (m_state.phase != Phase::Error) {
            // Keep a terminal error when automatic retry is off.
            m_state.phase = Phase::Disconnected;
        }
        m_state.peerAddress.clear();
        publish();
    });
    connect(connection, &PgxlConnection::connectionFailed, this,
            [this](const QString& reason) {
        if (!m_running) { return; }
        stopDiscovery();
        m_attempt = 0;
        m_state.phase = Phase::Error;
        m_state.error = reason;
        m_state.peerAddress.clear();
        publish();
    });
    connect(connection, &PgxlConnection::reconnectAttempt, this, [this](int, int) {
        if (!m_running) { return; }
        stopDiscovery();
        m_attempt = 0;
        m_state.phase = Phase::Retrying;
        m_state.peerAddress.clear();
        publish();
    });
}

StationPgxlController::~StationPgxlController() { stopDiscovery(); }

void StationPgxlController::publish()
{
    if (m_model) { m_model->setStationConnectionState(m_state); }
}

void StationPgxlController::clearIdentity()
{
    m_nativeInfo = {};
    m_discoveredModel.clear();
    m_discoveredSerial.clear();
    m_discoveredNickname.clear();
    m_peer.clear();
    m_peerPort = 0;
    m_state.deviceModel.clear();
    m_state.deviceSerial.clear();
    m_state.deviceVersion.clear();
    m_state.deviceNickname.clear();
    m_state.peerAddress.clear();
}

void StationPgxlController::resetScope(const QString& host, quint16 port, bool enabled)
{
    const auto generation = ++m_generation;
    m_running = false;
    m_attempt = 0;
    stopDiscovery();
    QPointer<StationPgxlController> self(this);
    if (m_connection) { m_connection->disconnect(); }
    if (!self || m_generation != generation) { return; }

    clearIdentity();
    m_settings->reset();
    m_state = {};
    m_state.configuredHost = host;
    m_state.configuredPort = port;
    m_state.phase = enabled ? Phase::Disconnected : Phase::Disabled;
    publish();
}

void StationPgxlController::start(const QString& host, quint16 port)
{
    cancel();
    const auto generation = ++m_generation;
    m_running = true;
    m_state.configuredHost = host;
    m_state.configuredPort = port;
    m_state.phase = Phase::Connecting;
    m_state.error.clear();
    QPointer<StationPgxlController> self(this);
    publish();
    if (self && m_generation == generation && m_running && m_connection) {
        m_connection->connectToPgxl(host, port);
    }
}

void StationPgxlController::cancel(bool disabled)
{
    ++m_generation;
    m_running = false;
    m_attempt = 0;
    stopDiscovery();
    if (m_connection) { m_connection->disconnect(); }
    clearIdentity();
    m_settings->reset();
    m_state.phase = disabled ? Phase::Disabled : Phase::Disconnected;
    m_state.error.clear();
    publish();
}

void StationPgxlController::showSavedEndpoint(const QString& host, quint16 port)
{
    if (m_state.phase != Phase::Disconnected && m_state.phase != Phase::Disabled
        && m_state.phase != Phase::Error) {
        return;
    }
    m_state.configuredHost = host;
    m_state.configuredPort = port;
    publish();
}

void StationPgxlController::applyConnectionSettings()
{
    if (!m_connection) { return; }
    m_connection->applyConnectionSettings();
    m_connection->setAutoPingIntervalSec(stationPingSeconds());
    // Automatic retry turned off while a retry was pending: nothing will be
    // dialled, so the phase says disconnected rather than retrying.
    if (m_running && m_state.phase == Phase::Retrying && !m_connection->reconnectPending()) {
        m_state.phase = Phase::Disconnected;
        publish();
    }
}

bool StationPgxlController::current(quint64 attempt) const
{
    return m_running && attempt != 0 && attempt == m_attempt && m_connection
        && attempt == m_connection->socketAttemptToken();
}

void StationPgxlController::stopDiscovery()
{
    if (!m_discovery) { return; }
    auto* retired = m_discovery.data();
    m_discovery.clear();
    retired->stop();
    QObject::disconnect(retired, nullptr, this, nullptr);
    retired->deleteLater();
}

void StationPgxlController::identify(quint64 attempt, const QString& peer, quint16 port)
{
    stopDiscovery();
    clearIdentity();
    m_attempt = attempt;
    m_peer = peer;
    m_peerPort = port;
    m_state.phase = Phase::Identifying;
    m_state.error.clear();
    auto* discovery = new LanDiscovery(this);
    discovery->setIdentitySensitiveDeduplication(true);
    if (m_stationBind) { discovery->setStationBind(*m_stationBind); }
    m_discovery = discovery;
    connect(discovery, &LanDiscovery::deviceDiscovered, this,
            [this, discovery, attempt](const QString& product, const QString& ip,
                quint16 receivedPort, const QString&, const QString& serial,
                const QString& nickname) {
        if (!current(attempt) || m_discovery != discovery
            || QHostAddress(ip) != QHostAddress(m_peer) || receivedPort != m_peerPort) {
            return;
        }
        m_discoveredModel = product;
        m_discoveredSerial = serial;
        m_discoveredNickname = nickname;
        m_state.deviceModel = product;
        if (product != expectedProduct()) {
            // A Tuner Genius (or anything else) at the amp's address: never
            // admitted, so it is never paired and gets no command but `info`.
            m_state.deviceSerial = serial;
            // In operator words: the station sends this to an app as the
            // connection error (iPhone app Part A fix wave, R-IOS-01).
            qCInfo(lcConnection) << "Expected PowerGeniusXL at" << m_peer << "; observed"
                                    << product << "serial" << serial;
            m_connection->rejectIdentity(attempt,
                QStringLiteral("The device at this address reports itself as %1, not a Power Genius. "
                               "Check the amplifier's address and port.")
                    .arg(product));
            return;
        }
        m_state.deviceNickname = nickname;
        tryAdmit();
    });
    connect(discovery, &LanDiscovery::scanFinished, this, [this, discovery, attempt] {
        if (!current(attempt) || m_discovery != discovery) { return; }
        if (m_discoveredSerial.isEmpty()) {
            qCInfo(lcConnection) << "No matching PGXL discovery announcement for"
                                    << m_peer << m_peerPort;
            // M7 (R-R3-47): heard, or configured, on another network than
            // the radio's: say so, and which Core setting allows it.
            QString offNetwork;
            if (m_stationBind && !QHostAddress(m_peer).isNull()
                && !m_stationBind->acceptsPeer(QHostAddress(m_peer))) {
                offNetwork = m_peer;
            }
            // Follow-up 8: only this device: its configured address, or
            // the serial it gave in its own info reply.
            for (const auto& heard : discovery->ignoredOffNetwork()) {
                const QString& product = heard.product;
                const bool thisDevice = QHostAddress(heard.address) == QHostAddress(m_peer)
                    || (!heard.serial.isEmpty() && heard.serial == m_nativeInfo.serial);
                if (offNetwork.isEmpty() && thisDevice
                    && product == expectedProduct()) {
                    offNetwork = heard.address;
                }
            }
            if (!offNetwork.isEmpty()) {
                m_connection->rejectIdentity(attempt,
                    StationNetwork::offNetworkReason(QStringLiteral("Power Genius"), offNetwork));
                return;
            }
            m_connection->rejectIdentity(attempt,
                QStringLiteral("The Core did not find a Power Genius at this address on its network. "
                               "Check the amplifier's address and port."));
        }
        // A matching announcement with the info reply still pending is
        // bounded by PgxlConnection's own identity timer.
    });
    discovery->start(); // LanDiscovery's three-second window.
    publish();
}

void StationPgxlController::tryAdmit()
{
    if (!current(m_attempt)) { return; }
    if (m_discoveredSerial.isEmpty() || m_nativeInfo.serial.isEmpty()
        || m_discoveredModel != expectedProduct()) {
        publish();
        return;
    }
    const auto attempt = m_attempt;
    stopDiscovery();
    // Serial mismatch is refused inside admitIdentity (connectionFailed).
    m_connection->admitIdentity(attempt, m_discoveredSerial);
}
} // namespace NereusSDR
