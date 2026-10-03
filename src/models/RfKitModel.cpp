// no-port-check: NereusSDR-original. See RfKitModel.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/RfKitModel.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-48: antenna and tuner
//                                    rows, operating interface, band
//                                    follow, controller-owned connection
//                                    state. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "models/RfKitModel.h"

#include "core/Rf2ksConnection.h"

namespace NereusSDR {

QString RfKitModel::readOnlyReason()
{
    return QStringLiteral("The Core reports the RF-Kit amplifier's readings. They "
                          "cannot be changed from this app.");
}

RfKitModel::RfKitModel(QObject* parent)
    : QObject(parent)
{
}

void RfKitModel::bindConnection(Rf2ksConnection* connection)
{
    if (m_conn) {
        disconnect(m_conn, nullptr, this, nullptr);
    }
    m_conn = connection;
    if (!connection) {
        return;
    }
    connect(connection, &Rf2ksConnection::powerUpdated, this, &RfKitModel::applyPower);
    connect(connection, &Rf2ksConnection::operateModeUpdated,
            this, &RfKitModel::applyOperateMode);
    connect(connection, &Rf2ksConnection::infoUpdated, this, &RfKitModel::applyInfo);
    connect(connection, &Rf2ksConnection::operationalInterfaceUpdated, this,
            [this](const QString& iface, const QString&) { applyOperationalInterface(iface); });
    connect(connection, &Rf2ksConnection::antennasUpdated, this, &RfKitModel::applyAntennas);
    connect(connection, &Rf2ksConnection::activeAntennaUpdated,
            this, &RfKitModel::applyActiveAntenna);
    connect(connection, &Rf2ksConnection::tunerUpdated, this, &RfKitModel::applyTuner);
    // R-R3-47: on the Core, StationRfKitController reports the phases.
    connect(connection, &Rf2ksConnection::connected, this, [this] {
        if (m_controllerOwnsConnection) { return; }
        refreshFromConnection(ConnectionPhase::Connected, {});
    });
    connect(connection, &Rf2ksConnection::disconnected, this, [this] {
        if (m_controllerOwnsConnection) { return; }
        const ConnectionPhase phase = m_connection.phase == ConnectionPhase::Error
            ? ConnectionPhase::Error : ConnectionPhase::Disconnected;
        refreshFromConnection(phase, m_connection.error);
    });
    connect(connection, &Rf2ksConnection::connectionFailed, this,
            [this](const QString& reason) {
        if (m_controllerOwnsConnection) { return; }
        refreshFromConnection(ConnectionPhase::Error, reason);
    });
}

QString RfKitModel::bandFollowText() const
{
    switch (m_bandFollow) {
    case BandFollow::Following:
        return QStringLiteral("Band follow: following the radio");
    case BandFollow::Waiting:
        if (m_bandFollowAddress.isEmpty() || m_bandFollowPort <= 0) {
            return QStringLiteral("Band follow: waiting for the amplifier to connect to the TCI server.");
        }
        return QStringLiteral("Band follow: enter %1, port %2 as the TCI server on the amplifier.")
            .arg(m_bandFollowAddress).arg(m_bandFollowPort);
    case BandFollow::ThisComputerOnly:
        return QStringLiteral("Band follow: the TCI server accepts only apps on its own computer, "
                              "so the amplifier cannot reach it.");
    case BandFollow::Off:
        break;
    }
    return QStringLiteral("Band follow: off. Turn on the TCI server so the amplifier can follow "
                          "the radio.");
}

void RfKitModel::setBandFollow(BandFollow state, const QString& address, int port)
{
    if (state == m_bandFollow && address == m_bandFollowAddress && port == m_bandFollowPort) {
        return;
    }
    m_bandFollow = state;
    m_bandFollowAddress = address;
    m_bandFollowPort = port;
    emit bandFollowChanged();
}

void RfKitModel::applyOperationalInterface(const QString& iface)
{
    if (iface != m_operationalInterface) {
        m_operationalInterface = iface;
        emit statusChanged();
    }
}

void RfKitModel::applyAntennas(const QList<RfKitAntenna>& antennas)
{
    int present = 0;
    int disabled = 0;
    for (const RfKitAntenna& a : antennas) {
        if (a.type != RfKitAntenna::Type::Internal || a.number < 1 || a.number > 4) {
            continue;
        }
        const int bit = 1 << (a.number - 1);
        present |= bit;
        if (a.state == RfKitAntenna::State::Disabled) {
            disabled |= bit;
        }
    }
    if (present != m_antennaPresentMask || disabled != m_antennaDisabledMask) {
        m_antennaPresentMask = present;
        m_antennaDisabledMask = disabled;
        emit statusChanged();
    }
}

void RfKitModel::applyActiveAntenna(const RfKitAntenna& antenna)
{
    const bool external = antenna.type == RfKitAntenna::Type::External;
    const int number = qMax(0, antenna.number);
    if (number != m_activeAntennaNumber || external != m_activeAntennaExternal) {
        m_activeAntennaNumber = number;
        m_activeAntennaExternal = external;
        emit statusChanged();
    }
}

void RfKitModel::applyTuner(const RfKitTunerSnapshot& tuner)
{
    TunerMode mode = TunerMode::Unknown;
    switch (tuner.mode) {
    case RfKitTunerSnapshot::Mode::Bypass: mode = TunerMode::Bypass; break;
    case RfKitTunerSnapshot::Mode::Manual: mode = TunerMode::Manual; break;
    case RfKitTunerSnapshot::Mode::AutoTuning: mode = TunerMode::AutoTuning; break;
    case RfKitTunerSnapshot::Mode::Auto: mode = TunerMode::Auto; break;
    case RfKitTunerSnapshot::Mode::Unknown: break;
    }
    const bool changed = mode != m_tunerMode || tuner.setup != m_tunerSetup
        || tuner.lValuenH != m_tunerInductanceNh || tuner.cValuepF != m_tunerCapacitancePf
        || tuner.tunedFrequencyKHz != m_tunerFrequencyKhz
        || tuner.segmentSizeKHz != m_tunerSegmentKhz;
    m_tunerMode = mode;
    m_tunerSetup = tuner.setup;
    m_tunerInductanceNh = tuner.lValuenH;
    m_tunerCapacitancePf = tuner.cValuepF;
    m_tunerFrequencyKhz = tuner.tunedFrequencyKHz;
    m_tunerSegmentKhz = tuner.segmentSizeKHz;
    if (changed) {
        emit statusChanged();
    }
}

QList<RfKitAntenna> RfKitModel::antennas() const
{
    QList<RfKitAntenna> list;
    for (int number = 1; number <= 4; ++number) {
        const int bit = 1 << (number - 1);
        if ((m_antennaPresentMask & bit) == 0) {
            continue;
        }
        RfKitAntenna a;
        a.type = RfKitAntenna::Type::Internal;
        a.number = number;
        if ((m_antennaDisabledMask & bit) != 0) {
            a.state = RfKitAntenna::State::Disabled;
        } else if (!m_activeAntennaExternal && m_activeAntennaNumber == number) {
            a.state = RfKitAntenna::State::Active;
        } else {
            a.state = RfKitAntenna::State::Available;
        }
        list.append(a);
    }
    return list;
}

RfKitAntenna RfKitModel::activeAntenna() const
{
    RfKitAntenna a;
    a.type = m_activeAntennaExternal ? RfKitAntenna::Type::External
                                     : RfKitAntenna::Type::Internal;
    a.number = m_activeAntennaNumber;
    a.state = RfKitAntenna::State::Active;
    return a;
}

RfKitTunerSnapshot RfKitModel::tuner() const
{
    RfKitTunerSnapshot snap;
    switch (m_tunerMode) {
    case TunerMode::Bypass: snap.mode = RfKitTunerSnapshot::Mode::Bypass; break;
    case TunerMode::Manual: snap.mode = RfKitTunerSnapshot::Mode::Manual; break;
    case TunerMode::AutoTuning: snap.mode = RfKitTunerSnapshot::Mode::AutoTuning; break;
    case TunerMode::Auto: snap.mode = RfKitTunerSnapshot::Mode::Auto; break;
    case TunerMode::Unknown: snap.mode = RfKitTunerSnapshot::Mode::Unknown; break;
    }
    snap.setup = m_tunerSetup;
    snap.lValuenH = m_tunerInductanceNh;
    snap.cValuepF = m_tunerCapacitancePf;
    snap.tunedFrequencyKHz = m_tunerFrequencyKhz;
    snap.segmentSizeKHz = m_tunerSegmentKhz;
    return snap;
}

void RfKitModel::refreshFromConnection(ConnectionPhase phase, const QString& error)
{
    StationConnectionState next = m_connection;
    next.phase = phase;
    next.error = phase == ConnectionPhase::Connected ? QString() : error;
    if (m_conn) {
        next.configuredHost = m_conn->peerAddress();
        next.configuredPort = m_conn->peerPort();
        next.peerAddress = phase == ConnectionPhase::Connected ? m_conn->peerAddress()
                                                                : QString();
    }
    setStationConnectionState(next);
}

void RfKitModel::setAccessoryEnabled(bool enabled)
{
    m_enabled = enabled;
    if (m_controllerOwnsConnection) {
        return; // R-R3-47: the Core's controller reports Disabled itself.
    }
    StationConnectionState next = m_connection;
    if (!enabled && next.phase != ConnectionPhase::Connected) {
        next.phase = ConnectionPhase::Disabled;
        next.error.clear();
    } else if (enabled && next.phase == ConnectionPhase::Disabled) {
        next.phase = ConnectionPhase::Disconnected;
    }
    setStationConnectionState(next);
}

void RfKitModel::setStationConnectionState(const StationConnectionState& state)
{
    StationConnectionState next = state;
    // The Core's controller reports Disabled itself (the station's switch
    // cancels it), so its states are taken as they are.
    if (!m_enabled && !m_controllerOwnsConnection
        && next.phase != ConnectionPhase::Connected) {
        next.phase = ConnectionPhase::Disabled;
    }
    // Readings are only live on a connection: off it, present says the
    // values are the last ones read. The values themselves stay.
    if (next.phase != ConnectionPhase::Connected && m_present) {
        m_present = false;
        emit statusChanged();
    }
    publishConnection(next);
}

void RfKitModel::publishConnection(const StationConnectionState& next)
{
    const bool changed = next.phase != m_connection.phase
        || next.configuredHost != m_connection.configuredHost
        || next.configuredPort != m_connection.configuredPort
        || next.error != m_connection.error
        || next.deviceModel != m_connection.deviceModel
        || next.deviceSerial != m_connection.deviceSerial
        || next.deviceVersion != m_connection.deviceVersion
        || next.deviceNickname != m_connection.deviceNickname;
    m_connection = next;
    if (changed) {
        emit stationConnectionChanged();
    }
}

void RfKitModel::applyPower(const RfKitPowerSnapshot& snapshot)
{
    const double forward = snapshot.forwardW;
    const double reflected = snapshot.reflectedW;
    const double swr = static_cast<double>(snapshot.swr);
    const double temperature = static_cast<double>(snapshot.temperatureC);
    const double voltage = static_cast<double>(snapshot.voltageV);
    const double current = static_cast<double>(snapshot.currentA);
    const bool changed = !m_present || forward != m_forwardPowerW
        || reflected != m_reflectedPowerW || swr != m_swr
        || temperature != m_temperatureC || voltage != m_voltageV
        || current != m_currentA;
    m_present = true;
    m_forwardPowerW = forward;
    m_reflectedPowerW = reflected;
    m_swr = swr;
    m_temperatureC = temperature;
    m_voltageV = voltage;
    m_currentA = current;
    if (changed) {
        emit statusChanged();
    }
}

void RfKitModel::applyOperateMode(const QString& mode)
{
    const bool operate = mode == QStringLiteral("OPERATE");
    if (operate != m_operate) {
        m_operate = operate;
        emit statusChanged();
    }
}

void RfKitModel::applyInfo(const QString& device, const QString& softwareVersion,
                           const QString& customName)
{
    StationConnectionState next = m_connection;
    next.deviceModel = device;
    next.deviceVersion = softwareVersion;
    next.deviceNickname = customName;
    publishConnection(next);
}

bool RfKitModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    StationConnectionState connection = m_connection;
    if (propertyName == "connectionPhase") {
        connection.phase = static_cast<ConnectionPhase>(value.toInt());
    } else if (propertyName == "configuredHost") {
        connection.configuredHost = value.toString();
    } else if (propertyName == "configuredPort") {
        connection.configuredPort = static_cast<quint16>(qBound(0, value.toInt(), 65535));
    } else if (propertyName == "connectionError") {
        connection.error = value.toString();
    } else if (propertyName == "deviceModel") {
        connection.deviceModel = value.toString();
    } else if (propertyName == "deviceSerial") {
        connection.deviceSerial = value.toString();
    } else if (propertyName == "deviceVersion") {
        connection.deviceVersion = value.toString();
    } else if (propertyName == "deviceNickname") {
        connection.deviceNickname = value.toString();
    } else if (propertyName == "bandFollow" || propertyName == "bandFollowAddress"
               || propertyName == "bandFollowPort") {
        // R-R3-48: a plain state apply.
        BandFollow state = m_bandFollow;
        QString address = m_bandFollowAddress;
        int port = m_bandFollowPort;
        if (propertyName == "bandFollow") {
            const int raw = value.toInt();
            state = raw >= 0 && raw <= static_cast<int>(BandFollow::ThisComputerOnly)
                ? static_cast<BandFollow>(raw) : BandFollow::Off;
        } else if (propertyName == "bandFollowAddress") {
            address = value.toString();
        } else {
            port = qBound(0, value.toInt(), 65535);
        }
        setBandFollow(state, address, port);
        return true;
    } else if (propertyName == "operationalInterface" || propertyName == "tunerSetup") {
        QString& text = propertyName == "tunerSetup" ? m_tunerSetup : m_operationalInterface;
        if (text != value.toString()) {
            text = value.toString();
            emit statusChanged();
        }
        return true;
    } else if (propertyName == "tunerMode") {
        const int raw = value.toInt();
        const TunerMode mode = raw >= 0 && raw <= static_cast<int>(TunerMode::Auto)
            ? static_cast<TunerMode>(raw) : TunerMode::Unknown;
        if (mode != m_tunerMode) {
            m_tunerMode = mode;
            emit statusChanged();
        }
        return true;
    } else if (propertyName == "antennaPresentMask" || propertyName == "antennaDisabledMask"
               || propertyName == "activeAntennaNumber" || propertyName == "tunerInductanceNh"
               || propertyName == "tunerCapacitancePf" || propertyName == "tunerFrequencyKhz"
               || propertyName == "tunerSegmentKhz") {
        int* number = propertyName == "antennaPresentMask" ? &m_antennaPresentMask
            : propertyName == "antennaDisabledMask" ? &m_antennaDisabledMask
            : propertyName == "activeAntennaNumber" ? &m_activeAntennaNumber
            : propertyName == "tunerInductanceNh" ? &m_tunerInductanceNh
            : propertyName == "tunerCapacitancePf" ? &m_tunerCapacitancePf
            : propertyName == "tunerFrequencyKhz" ? &m_tunerFrequencyKhz
            : &m_tunerSegmentKhz;
        if (*number != value.toInt()) {
            *number = value.toInt();
            emit statusChanged();
        }
        return true;
    } else if (propertyName == "activeAntennaExternal") {
        if (m_activeAntennaExternal != value.toBool()) {
            m_activeAntennaExternal = value.toBool();
            emit statusChanged();
        }
        return true;
    } else {
        // A status value: a plain state apply, never a request to the amp.
        bool* flag = nullptr;
        double* number = nullptr;
        if (propertyName == "present") { flag = &m_present; }
        else if (propertyName == "operate") { flag = &m_operate; }
        else if (propertyName == "forwardPowerW") { number = &m_forwardPowerW; }
        else if (propertyName == "reflectedPowerW") { number = &m_reflectedPowerW; }
        else if (propertyName == "swr") { number = &m_swr; }
        else if (propertyName == "temperatureC") { number = &m_temperatureC; }
        else if (propertyName == "voltageV") { number = &m_voltageV; }
        else if (propertyName == "currentA") { number = &m_currentA; }
        else { return false; }
        bool changed = false;
        if (flag) {
            changed = *flag != value.toBool();
            *flag = value.toBool();
        } else {
            changed = *number != value.toDouble();
            *number = value.toDouble();
        }
        if (changed) {
            emit statusChanged();
        }
        return true;
    }
    publishConnection(connection);
    return true;
}

} // namespace NereusSDR
