// no-port-check: NereusSDR-original. See AmplifierModel.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/AmplifierModel.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  setConnectionStateOwnedByController.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "models/AmplifierModel.h"

#include "core/PgxlConnection.h"

namespace NereusSDR {

QString AmplifierModel::readOnlyReason()
{
    return QStringLiteral("The Core reports the amplifier's readings. They cannot be "
                          "changed from this app.");
}

QString AmplifierModel::receiveOnlyOperateReason()
{
    return QStringLiteral("Operating the station's amplifier or tuner waits for remote "
                          "transmit. This Core is receive-only.");
}

AmplifierModel::AmplifierModel(QObject* parent)
    : QObject(parent)
{
}

AmplifierModel::State AmplifierModel::stateFromWord(const QString& deviceState)
{
    if (deviceState == QStringLiteral("POWERUP")) { return State::PowerUp; }
    if (deviceState == QStringLiteral("STANDBY")) { return State::Standby; }
    if (deviceState == QStringLiteral("IDLE")) { return State::Idle; }
    if (deviceState == QStringLiteral("OPERATE")) { return State::Operate; }
    if (deviceState == QStringLiteral("TRANSMIT_A")) { return State::TransmitA; }
    if (deviceState == QStringLiteral("TRANSMIT_B")) { return State::TransmitB; }
    if (deviceState.startsWith(QStringLiteral("FAULT"))) { return State::Fault; }
    return State::Unknown;
}

void AmplifierModel::bindConnection(PgxlConnection* connection)
{
    if (m_conn) {
        disconnect(m_conn, nullptr, this, nullptr);
    }
    m_conn = connection;
    if (!connection) {
        return;
    }
    connect(connection, &PgxlConnection::statusUpdated,
            this, &AmplifierModel::applyStatusFrame);
    // R-R3-48: band follow runs once the amp is paired with the radio
    // (PgxlConnection::setBand sends nothing before the pairing answer).
    // On the Core `connected` means admitted; pairing follows it.
    connect(connection, &PgxlConnection::connected, this, [this] {
        setBandFollow(BandFollow::Waiting);
    });
    connect(connection, &PgxlConnection::pairingResult, this,
            [this](bool succeeded, const QString&) {
        setBandFollow(succeeded ? BandFollow::Following : BandFollow::Waiting);
    });
    connect(connection, &PgxlConnection::disconnected, this, [this] {
        setBandFollow(BandFollow::Off);
    });
    connect(connection, &PgxlConnection::connectionFailed, this, [this](const QString&) {
        setBandFollow(BandFollow::Off);
    });
    connect(connection, &PgxlConnection::connected, this, [this] {
        refreshFromConnection(ConnectionPhase::Connected, {});
    });
    connect(connection, &PgxlConnection::disconnected, this, [this] {
        // A drop after a failure keeps the failure's words.
        const ConnectionPhase phase = m_connection.phase == ConnectionPhase::Error
            ? ConnectionPhase::Error : ConnectionPhase::Disconnected;
        refreshFromConnection(phase, m_connection.error);
    });
    connect(connection, &PgxlConnection::reconnectAttempt, this, [this](int, int) {
        refreshFromConnection(ConnectionPhase::Retrying, m_connection.error);
    });
    connect(connection, &PgxlConnection::connectionFailed, this,
            [this](const QString& reason) {
        refreshFromConnection(ConnectionPhase::Error, reason);
    });
}

void AmplifierModel::refreshFromConnection(ConnectionPhase phase, const QString& error)
{
    if (m_controllerOwnsConnection) {
        return; // StationPgxlController reports the phase on the Core.
    }
    StationConnectionState next = m_connection;
    next.phase = phase;
    next.error = phase == ConnectionPhase::Connected ? QString() : error;
    if (m_conn) {
        next.configuredHost = m_conn->configuredHost();
        next.configuredPort = m_conn->configuredPort();
        next.deviceVersion = m_conn->version();
        next.peerAddress = phase == ConnectionPhase::Connected ? m_conn->peerAddress()
                                                                : QString();
    }
    setStationConnectionState(next);
}

void AmplifierModel::setAccessoryEnabled(bool enabled)
{
    m_enabled = enabled;
    StationConnectionState next = m_connection;
    if (!enabled && next.phase != ConnectionPhase::Connected) {
        next.phase = ConnectionPhase::Disabled;
        next.error.clear();
    } else if (enabled && next.phase == ConnectionPhase::Disabled) {
        next.phase = ConnectionPhase::Disconnected;
    }
    setStationConnectionState(next);
}

void AmplifierModel::setStationConnectionState(const StationConnectionState& state)
{
    StationConnectionState next = state;
    // The Core's controller reports Disabled itself (the 4O3A switch cancels
    // it), so its states are taken as they are.
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

void AmplifierModel::publishConnection(const StationConnectionState& next)
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

void AmplifierModel::applyStatusFrame(const QMap<QString, QString>& kvs)
{
    const bool gaugesChanged = applyPgxlStatus(kvs, m_gauges);
    const State nextState = stateFromWord(m_gauges.deviceState);
    const bool nextOperate = pgxlStateIsOperate(m_gauges.deviceState);
    const bool changed = gaugesChanged || !m_present || nextState != m_state
        || nextOperate != m_operate;
    m_present = true;
    m_state = nextState;
    m_operate = nextOperate;
    if (changed) {
        emit statusChanged();
    }
}

QString AmplifierModel::bandFollowText() const
{
    switch (m_bandFollow) {
    case BandFollow::Following:
        return QStringLiteral("Band follow: following the radio");
    case BandFollow::Waiting:
        return QStringLiteral("Band follow: waiting for the Power Genius to pair with the radio.");
    case BandFollow::Off:
    case BandFollow::ThisComputerOnly:
        break;
    }
    return QStringLiteral("Band follow: off while the Power Genius is not connected.");
}

void AmplifierModel::setBandFollow(BandFollow state)
{
    if (state != m_bandFollow) {
        m_bandFollow = state;
        emit bandFollowChanged();
    }
}

bool AmplifierModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    StationConnectionState connection = m_connection;
    if (propertyName == "bandFollow") {
        // R-R3-48: a plain state apply.
        const int raw = value.toInt();
        setBandFollow(raw >= 0 && raw <= static_cast<int>(BandFollow::ThisComputerOnly)
                          ? static_cast<BandFollow>(raw) : BandFollow::Off);
        return true;
    }
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
    } else {
        // A status value: a plain state apply, never a command.
        PgxlGauges gauges = m_gauges;
        bool present = m_present;
        State state = m_state;
        bool operate = m_operate;
        if (propertyName == "present") {
            present = value.toBool();
        } else if (propertyName == "state") {
            state = static_cast<State>(value.toInt());
        } else if (propertyName == "deviceState") {
            gauges.deviceState = value.toString();
        } else if (propertyName == "operate") {
            operate = value.toBool();
        } else if (propertyName == "transmitting") {
            gauges.transmitting = value.toBool();
        } else if (propertyName == "forwardPowerW") {
            gauges.forwardPowerW = value.toDouble();
        } else if (propertyName == "swr") {
            gauges.swr = value.toDouble();
        } else if (propertyName == "temperatureC") {
            gauges.temperatureC = value.toDouble();
        } else if (propertyName == "mainsVoltageV") {
            gauges.mainsVoltageV = value.toDouble();
        } else if (propertyName == "drainCurrentA") {
            gauges.drainCurrentA = value.toDouble();
        } else if (propertyName == "efficiencyText") {
            gauges.efficiencyText = value.toString();
        } else {
            return false;
        }
        const bool changed = !(gauges == m_gauges) || present != m_present
            || state != m_state || operate != m_operate;
        m_gauges = gauges;
        m_present = present;
        m_state = state;
        m_operate = operate;
        if (changed) {
            emit statusChanged();
        }
        return true;
    }
    // Remote: no enable gate, no present reset; the Core already did both.
    publishConnection(connection);
    return true;
}

} // namespace NereusSDR
