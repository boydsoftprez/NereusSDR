// no-port-check: NereusSDR-original. R-R3-22 station-owned TGXL identity.
// Captured wire contract: captures/flex-tgxl-direct-NOTES.md:67-93;
// discovery aliases and admission policy: remote-daemon-r3-plan, task 4d.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via OpenAI Codex.
// 2026-09-24: R-R3-47 / R-R3-22: faultObserved. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22: deviceSettings, the tuner's own settings
// for a window. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47: one connection fault per outage; a tuner on another
// network is refused saying how to allow it. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 8): showSavedEndpoint. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "core/StationTgxlController.h"
#include "core/LanDiscovery.h"
#include "core/LogCategories.h"
#include "core/StationNetwork.h"
#include <QHostAddress>

namespace NereusSDR {
using Phase = TunerModel::ConnectionPhase;

StationTgxlController::StationTgxlController(TgxlConnection* connection,
                                           TunerModel* model, QObject* parent)
    : QObject(parent), m_connection(connection), m_model(model)
{
    connection->setIdentityAdmissionRequired(true);
    // R-R3-47 / R-R3-22: the tuner's own settings, through the
    // connection's own command methods (the local Advanced page's commands).
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
    m_settings = new StationDeviceSettings(StationDeviceSettings::Device::Tgxl, std::move(wire),
                                           this);
    connect(connection, &TgxlConnection::replyReceived, m_settings,
            &StationDeviceSettings::onReply);
    connect(connection, &TgxlConnection::disconnected, m_settings,
            &StationDeviceSettings::onDisconnected);
    connect(connection, &TgxlConnection::identityProtocolProgress, this,
            [this](quint64 token, const QString& peer, quint16 port, const QString&) {
        if (m_running) { identify(token, peer, port); }
    });
    connect(connection, &TgxlConnection::nativeInfoReceived, this,
            [this](const TgxlIdentityInfo& info) {
        if (!current(info.socketAttemptToken)) { return; }
        m_nativeInfo = info;
        m_state.deviceSerial = info.serial;
        m_state.deviceVersion = info.version;
        m_state.deviceNickname = info.nickname;
        tryAdmit();
    });
    connect(connection, &TgxlConnection::connected, this, [this] {
        if (!current(m_attempt)) { return; }
        stopDiscovery();
        m_state.phase = Phase::Connected;
        m_state.error.clear();
        m_state.peerAddress = m_peer;
        m_outageFaulted = false;   // I2: the outage, if any, is over
        publish();
    });
    connect(connection, &TgxlConnection::disconnected, this, [this] {
        if (!m_running) { return; }
        // R-R3-47: an operator's disconnect or cancel clears m_running
        // first, so a drop here is the tuner going away.
        const bool wasConnected = m_state.phase == Phase::Connected;
        stopDiscovery();
        m_attempt = 0;
        if (m_connection && m_connection->reconnectPending()) {
            m_state.phase = Phase::Retrying;
        } else if (m_state.phase != Phase::Error) {
            // Preserve the terminal error when automatic retry is disabled.
            m_state.phase = Phase::Disconnected;
        }
        m_state.peerAddress.clear();
        const bool record = wasConnected && !m_outageFaulted;
        if (wasConnected) {
            m_outageFaulted = true;   // I2: this drop is the outage's fault
        }
        QPointer<StationTgxlController> self(this);
        publish();
        if (self && record) {
            emit faultObserved(QStringLiteral("link"),
                               QStringLiteral("The Tuner Genius stopped answering."), QString());
        }
    });
    connect(connection, &TgxlConnection::connectionFailed, this,
            [this](const QString& reason) {
        if (!m_running) { return; }
        stopDiscovery();
        m_attempt = 0;
        // I2: every backoff step fails again and passes through Retrying;
        // only the outage's first failure is a fault.
        const bool newError = !m_outageFaulted;
        m_outageFaulted = true;
        m_state.phase = Phase::Error;
        m_state.error = reason;
        m_state.peerAddress.clear();
        QPointer<StationTgxlController> self(this);
        publish();
        if (self && newError) {
            emit faultObserved(QStringLiteral("connection"),
                               QStringLiteral("The Core could not connect to the Tuner Genius."),
                               reason);
        }
    });
    connect(connection, &TgxlConnection::reconnectAttempt, this, [this](int, int) {
        if (!m_running) { return; }
        stopDiscovery();
        m_attempt = 0;
        m_state.phase = Phase::Retrying;
        m_state.peerAddress.clear();
        publish();
    });
}

StationTgxlController::~StationTgxlController() { stopDiscovery(); }

void StationTgxlController::publish()
{
    if (m_model) { m_model->setStationConnectionState(m_state); }
}

void StationTgxlController::clearIdentity()
{
    m_nativeInfo = {};
    m_discoveredModel.clear();
    m_discoveredSerial.clear();
    m_peer.clear();
    m_peerPort = 0;
    m_state.deviceModel.clear();
    m_state.deviceSerial.clear();
    m_state.deviceVersion.clear();
    m_state.deviceNickname.clear();
    m_state.peerAddress.clear();
}

void StationTgxlController::resetScope(const QString& host, quint16 port,
                                       bool enabled)
{
    const auto generation = ++m_generation;
    m_running = false;
    m_attempt = 0;
    stopDiscovery();

    // Retire every socket/timer from the previous radio before publishing the
    // newly selected MAC's saved endpoint. A direct disconnected consumer may
    // replace or delete this controller, so validate ownership afterward.
    QPointer<StationTgxlController> self(this);
    if (m_connection) { m_connection->disconnect(); }
    if (!self || m_generation != generation) { return; }

    clearIdentity();
    m_settings->reset();
    m_outageFaulted = false;
    m_state = {};
    m_state.configuredHost = host;
    m_state.configuredPort = port;
    m_state.phase = enabled ? Phase::Disconnected : Phase::Disabled;
    publish();
}

void StationTgxlController::start(const QString& host, quint16 port)
{
    cancel();
    const auto generation = ++m_generation;
    m_running = true;
    m_outageFaulted = false;
    m_state.configuredHost = host;
    m_state.configuredPort = port;
    m_state.phase = Phase::Connecting;
    m_state.error.clear();
    QPointer<StationTgxlController> self(this);
    publish();
    if (self && m_generation == generation && m_running && m_connection) {
        m_connection->connectToTgxl(host, port);
    }
}

void StationTgxlController::cancel(bool disabled)
{
    ++m_generation;
    m_running = false;
    m_attempt = 0;
    stopDiscovery();
    if (m_connection) { m_connection->disconnect(); }
    clearIdentity();
    m_settings->reset();
    m_outageFaulted = false;
    m_state.phase = disabled ? Phase::Disabled : Phase::Disconnected;
    m_state.error.clear();
    publish();
}

void StationTgxlController::showSavedEndpoint(const QString& host, quint16 port)
{
    using Phase = TunerModel::ConnectionPhase;
    if (m_state.phase != Phase::Disconnected && m_state.phase != Phase::Disabled
        && m_state.phase != Phase::Error) {
        return;
    }
    m_state.configuredHost = host;
    m_state.configuredPort = port;
    publish();
}

bool StationTgxlController::current(quint64 attempt) const
{
    return m_running && attempt != 0 && attempt == m_attempt && m_connection
        && attempt == m_connection->socketAttemptToken();
}

void StationTgxlController::stopDiscovery()
{
    if (!m_discovery) { return; }
    auto* retired = m_discovery.data();
    m_discovery.clear();
    retired->stop();
    QObject::disconnect(retired, nullptr, this, nullptr);
    retired->deleteLater();
}

void StationTgxlController::identify(quint64 attempt, const QString& peer, quint16 port)
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
                quint16 receivedPort, const QString&, const QString& serial, const QString&) {
        if (!current(attempt) || m_discovery != discovery
            || QHostAddress(ip) != QHostAddress(m_peer) || receivedPort != m_peerPort) {
            return;
        }
        m_discoveredModel = product;
        m_discoveredSerial = serial;
        m_state.deviceModel = product;
        if (product != QStringLiteral("TunerGenius")
            && product != QStringLiteral("TunerGeniusXL")) {
            m_state.deviceSerial = serial;
            // In operator words: the station sends this to an app as the
            // connection error (iPhone app Part A fix wave, R-IOS-01).
            qCInfo(lcConnection) << "Expected TunerGenius/TunerGeniusXL at" << m_peer << "; observed"
                                    << product << "serial" << serial;
            m_connection->rejectIdentity(attempt,
                QStringLiteral("The device at this address reports itself as %1, not a Tuner Genius. "
                               "Check the tuner's address and port.")
                    .arg(product));
            return;
        }
        tryAdmit();
    });
    connect(discovery, &LanDiscovery::scanFinished, this, [this, discovery, attempt] {
        if (!current(attempt) || m_discovery != discovery) { return; }
        if (m_discoveredSerial.isEmpty()) {
            qCInfo(lcConnection) << "No matching TGXL discovery announcement for"
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
                    && (product == QStringLiteral("TunerGenius")
                        || product == QStringLiteral("TunerGeniusXL"))) {
                    offNetwork = heard.address;
                }
            }
            if (!offNetwork.isEmpty()) {
                m_connection->rejectIdentity(attempt,
                    StationNetwork::offNetworkReason(QStringLiteral("Tuner Genius"), offNetwork));
                return;
            }
            m_connection->rejectIdentity(attempt,
                QStringLiteral("The Core did not find a Tuner Genius at this address on its network. "
                               "Check the tuner's address and port."));
        }
        // Matching discovery with native info still pending is bounded by
        // TgxlConnection's independent identity timer.
    });
    discovery->start(); // Existing three-second LAN discovery window.
    publish();
}

void StationTgxlController::tryAdmit()
{
    if (!current(m_attempt)) { return; }
    if (m_discoveredSerial.isEmpty() || m_nativeInfo.serial.isEmpty()) {
        publish();
        return;
    }
    const auto attempt = m_attempt;
    stopDiscovery();
    m_connection->admitIdentity(attempt, m_discoveredSerial);
}
} // namespace NereusSDR
