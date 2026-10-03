// no-port-check: NereusSDR-original. R-R3-47 / R-R3-48 / R-R3-22 station-owned RF2K-S.
// Structure from StationPgxlController.cpp. J.J. Boyd (KG4VCF), September
// 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47: a window's Reset amp error (resetError). J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-48: TCI mode once when band follow starts, not on every
// reconnect. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 10): a window's OPERATE, antenna and TCI
// mode sent as the local applet and page send them, and a saved address
// shown without dialling. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.
#include "core/StationRfKitController.h"

namespace NereusSDR {
using Phase = RfKitModel::ConnectionPhase;

namespace {
// The amp's word for TCI on /operational-interface (design doc
// 2026-05-24-rfkit-rf2ks-applet-design.md section 6.1).
const QString kTciInterface = QStringLiteral("TCI");
} // namespace

StationRfKitController::StationRfKitController(Rf2ksConnection* connection,
                                               RfKitModel* model, QObject* parent)
    : QObject(parent), m_connection(connection), m_model(model)
{
    connection->setIdentityAdmissionRequired(true);
    if (model) {
        // This controller, not the raw connection signals, reports phases.
        model->setConnectionStateOwnedByController(true);
    }
    connect(connection, &Rf2ksConnection::connected, this, [this] {
        if (!m_running) { return; }
        publish(Phase::Connected);
        maybeRequestTciMode();
    });
    connect(connection, &Rf2ksConnection::disconnected, this, [this] {
        if (!m_running) { return; }
        publish(m_connection && m_connection->reconnectPending() ? Phase::Retrying
                                                                 : Phase::Disconnected);
    });
    connect(connection, &Rf2ksConnection::reconnectScheduled, this, [this](int, int) {
        if (!m_running) { return; }
        publish(Phase::Retrying);
    });
    connect(connection, &Rf2ksConnection::connectionFailed, this,
            [this](const QString& reason) {
        if (!m_running) { return; }
        publish(Phase::Error, reason);
    });
    connect(connection, &Rf2ksConnection::operationalInterfaceUpdated, this,
            [this](const QString&, const QString&) { maybeRequestTciMode(); });
}

StationRfKitController::~StationRfKitController() = default;

void StationRfKitController::publish(Phase phase, const QString& error)
{
    if (!m_model) { return; }
    RfKitModel::StationConnectionState next = m_model->stationConnectionState();
    next.phase = phase;
    next.error = error;
    next.configuredHost = m_host;
    next.configuredPort = m_port;
    next.peerAddress = phase == Phase::Connected ? m_host : QString();
    m_model->setStationConnectionState(next);
}

void StationRfKitController::resetScope(const QString& host, quint16 port, bool enabled)
{
    const auto generation = ++m_generation;
    m_running = false;
    QPointer<StationRfKitController> self(this);
    if (m_connection) { m_connection->disconnect(); }
    if (!self || m_generation != generation) { return; }
    m_host = host;
    m_port = port;
    if (m_model) {
        RfKitModel::StationConnectionState cleared;
        cleared.configuredHost = host;
        cleared.configuredPort = port;
        cleared.phase = enabled ? Phase::Disconnected : Phase::Disabled;
        m_model->setStationConnectionState(cleared);
    }
}

void StationRfKitController::start(const QString& host, quint16 port)
{
    cancel();
    const auto generation = ++m_generation;
    m_running = true;
    m_host = host;
    m_port = port;
    if (m_model) {
        // A new address: the previous amp's identity no longer applies.
        RfKitModel::StationConnectionState next;
        next.configuredHost = host;
        next.configuredPort = port;
        next.phase = Phase::Connecting;
        m_model->setStationConnectionState(next);
    }
    QPointer<StationRfKitController> self(this);
    if (self && m_generation == generation && m_running && m_connection) {
        m_connection->connectToAmp(host, port);
    }
}

void StationRfKitController::cancel(bool disabled)
{
    ++m_generation;
    m_running = false;
    QPointer<StationRfKitController> self(this);
    if (m_connection) { m_connection->disconnect(); }
    if (!self) { return; }
    if (m_model) {
        RfKitModel::StationConnectionState next;
        next.configuredHost = m_host;
        next.configuredPort = m_port;
        next.phase = disabled ? Phase::Disabled : Phase::Disconnected;
        m_model->setStationConnectionState(next);
    }
}

bool StationRfKitController::ampAdmitted(QString* reason) const
{
    if (!m_running || !m_connection || !m_connection->isConnected()) {
        if (reason) {
            *reason = QStringLiteral("The Core is not connected to the RF-Kit amplifier.");
        }
        return false;
    }
    return true;
}

bool StationRfKitController::resetError(QString* reason)
{
    if (!ampAdmitted(reason)) {
        return false;
    }
    // The local page's "Reset amp error state" (RfKitPage::buildRf2ksTab).
    m_connection->resetError();
    if (reason) {
        reason->clear();
    }
    return true;
}

bool StationRfKitController::setOperate(bool on, QString* reason)
{
    if (!ampAdmitted(reason)) {
        return false;
    }
    // The local applet's OPERATE (MainWindow's Rf2ksApplet::operateToggled
    // handler): the amp's own words for its two states.
    m_connection->setOperateMode(on ? QStringLiteral("OPERATE") : QStringLiteral("STANDBY"));
    if (reason) {
        reason->clear();
    }
    return true;
}

bool StationRfKitController::setAntenna(int number, QString* reason)
{
    if (!ampAdmitted(reason)) {
        return false;
    }
    // Once the amp has listed its antennas, only an internal one it lists
    // and does not list as disabled is offered (the local applet's buttons
    // follow the same list: Rf2ksApplet::setAntennas).
    const QList<RfKitAntenna> listed = m_connection->antennas();
    bool anyInternal = false;
    bool usable = false;
    for (const RfKitAntenna& a : listed) {
        if (a.type != RfKitAntenna::Type::Internal) {
            continue;
        }
        anyInternal = true;
        if (a.number == number && a.state != RfKitAntenna::State::Disabled) {
            usable = true;
        }
    }
    if (anyInternal && !usable) {
        if (reason) {
            *reason = QStringLiteral("This antenna is not available on the RF-Kit amplifier.");
        }
        return false;
    }
    // The local applet's ANT button (Rf2ksApplet::antennaRequested, an
    // internal antenna, to Rf2ksConnection::setActiveAntenna).
    m_connection->setActiveAntenna(RfKitAntenna::Type::Internal, number);
    if (reason) {
        reason->clear();
    }
    return true;
}

bool StationRfKitController::setTciMode(QString* reason)
{
    if (!ampAdmitted(reason)) {
        return false;
    }
    // The local page's "Set amp to TCI mode" (RfKitPage::buildRf2ksTab).
    m_connection->setOperationalInterface(kTciInterface);
    if (reason) {
        reason->clear();
    }
    return true;
}

void StationRfKitController::showSavedEndpoint(const QString& host, quint16 port)
{
    if (!m_model) {
        return;
    }
    const Phase phase = m_model->connectionPhase();
    if (phase != Phase::Disconnected && phase != Phase::Disabled && phase != Phase::Error) {
        return;
    }
    // The next Connect (or the switch turned on) dials this address.
    if (!m_running) {
        m_host = host;
        m_port = port;
    }
    RfKitModel::StationConnectionState next = m_model->stationConnectionState();
    next.configuredHost = host;
    next.configuredPort = port;
    m_model->setStationConnectionState(next);
}

void StationRfKitController::setBandFollowWanted(bool wanted)
{
    if (wanted == m_bandFollowWanted) { return; }
    m_bandFollowWanted = wanted;
    if (wanted) {
        // M3: band follow starts: each amp may be switched once more.
        m_tciSwitched.clear();
    }
    maybeRequestTciMode();
}

void StationRfKitController::maybeRequestTciMode()
{
    // Only an admitted amp, only once its interface is known, only once per
    // amp since band follow started (M3: not per connection), and only while
    // the station's TCI server is on.
    if (!m_running || !m_bandFollowWanted || m_tciSwitched.contains(ampKey())
        || !m_connection || !m_connection->isConnected()) {
        return;
    }
    const QString current = m_connection->operationalInterface();
    if (current.isEmpty()) {
        return;
    }
    // Already in TCI mode counts: band follow is running on this amp, and a
    // later change on its front panel is the operator's.
    m_tciSwitched.insert(ampKey());
    if (current == kTciInterface) {
        return;
    }
    m_connection->setOperationalInterface(kTciInterface);
}

} // namespace NereusSDR
