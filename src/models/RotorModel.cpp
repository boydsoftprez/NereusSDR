// no-port-check: NereusSDR-original. See RotorModel.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/RotorModel.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 4b).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "models/RotorModel.h"

#include "core/StationRotorController.h"

#include <QtGlobal>

#include <cmath>

namespace NereusSDR::RotorLink {

namespace {

// A heading or travel from the wire: a finite number, else the contract's
// "unknown".
double finiteOr(const QVariant& value, double fallback)
{
    const double d = value.toDouble();
    return std::isfinite(d) ? d : fallback;
}

template <typename E>
E enumWithin(const QVariant& value, int last, E fallback)
{
    const int v = value.toInt();
    return v >= 0 && v <= last ? static_cast<E>(v) : fallback;
}

} // namespace

QString RotorModel::readOnlyReason()
{
    return QStringLiteral("The Core reports its rotor here. Use the rotor controls to turn "
                          "it or change its setup.");
}

RotorModel::RotorModel(QObject* parent)
    : QObject(parent)
{
    m_hostRefresh.setInterval(kHostRefreshMs);
    connect(&m_hostRefresh, &QTimer::timeout, this, [this] {
        refreshHost();
        refreshFromController();
    });
}

RotorModel::State RotorModel::stateFrom(const StationRotorController& controller,
                                        const QString& serialPorts, bool rotctldAvailable)
{
    const RotorConfig& config = controller.config();
    State s;
    s.connectionPhase = controller.connectionPhase();
    s.connectionError = controller.connectionError();
    s.driver = static_cast<Driver>(static_cast<int>(controller.driver()));
    s.label = controller.label();
    s.serialPort = controller.serialPort();
    s.baud = config.baud;
    s.host = controller.host();
    s.port = static_cast<int>(config.port);
    s.serialPorts = serialPorts;
    s.axes = static_cast<Axes>(static_cast<int>(config.axes));
    s.rangeDeg = static_cast<int>(std::lround(config.rangeDeg));
    s.endStop = static_cast<EndStop>(static_cast<int>(config.endStop));
    s.offsetDeg = config.offsetDeg;
    // The contract: Hamlib's model for driver 4, 0 otherwise.
    s.hamlibModel = config.driver == RotorDriver::RotctldStarted ? config.hamlibModel : 0;
    s.rotctldAvailable = rotctldAvailable;
    s.targetAzimuthDeg = controller.targetAzimuthDeg();
    s.targetElevationDeg = controller.targetElevationDeg();
    s.motion = static_cast<Motion>(static_cast<int>(controller.motion()));
    s.presets = controller.presets();
    s.fault = controller.fault();
    s.spanPositionDeg = controller.spanPositionDeg();
    s.travelDeg = controller.travelDeg();
    s.routeKnown = controller.routeKnown();
    s.positionFresh = controller.positionFresh();
    s.azimuthDeg = controller.azimuthDeg();
    s.elevationDeg = controller.elevationDeg();
    // The contract: off connected, the headings and targets read -1 and
    // motion stopped.
    if (s.connectionPhase != TunerModel::ConnectionPhase::Connected) {
        s.azimuthDeg = -1.0;
        s.elevationDeg = -1.0;
        s.targetAzimuthDeg = -1.0;
        s.targetElevationDeg = -1.0;
        s.travelDeg = 0.0;
        s.routeKnown = true;
        s.motion = Motion::Stopped;
    }
    return s;
}

void RotorModel::bindController(StationRotorController* controller)
{
    disconnect(m_stateConnection);
    disconnect(m_positionConnection);
    disconnect(m_destroyedConnection);
    m_hostRefresh.stop();
    m_controller = controller;
    if (controller == nullptr) {
        return;
    }
    m_stateConnection = connect(controller, &StationRotorController::stateChanged, this,
                                &RotorModel::refreshFromController);
    m_positionConnection = connect(controller, &StationRotorController::positionChanged, this,
                                   &RotorModel::refreshFromController);
    m_destroyedConnection = connect(controller, &QObject::destroyed, this, [this] {
        m_controller = nullptr;
        m_hostRefresh.stop();
    });
    refreshHost();
    refreshFromController();
    m_hostRefresh.start();
}

void RotorModel::refreshHost()
{
    if (m_controller == nullptr) {
        return;
    }
    m_hostSerialPorts = m_controller->serialPortsText();
    m_hostRotctld = m_controller->rotctldAvailable();
}

void RotorModel::refreshFromController()
{
    if (m_controller == nullptr) {
        return;
    }
    setState(stateFrom(*m_controller, m_hostSerialPorts, m_hostRotctld));
}

void RotorModel::setState(const State& state)
{
    if (state == m_state) {
        return;
    }
    const bool positionMoved = state.spanPositionDeg != m_state.spanPositionDeg
        || state.travelDeg != m_state.travelDeg || state.routeKnown != m_state.routeKnown
        || state.positionFresh != m_state.positionFresh
        || state.azimuthDeg != m_state.azimuthDeg || state.elevationDeg != m_state.elevationDeg;
    State before = m_state;
    m_state = state;
    // The rest of the state, compared without the position.
    before.spanPositionDeg = state.spanPositionDeg;
    before.travelDeg = state.travelDeg;
    before.routeKnown = state.routeKnown;
    before.positionFresh = state.positionFresh;
    before.azimuthDeg = state.azimuthDeg;
    before.elevationDeg = state.elevationDeg;
    if (positionMoved) {
        emit positionChanged();
    }
    if (!(before == state)) {
        emit stateChanged();
    }
}

bool RotorModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    State next = m_state;
    if (propertyName == "connectionPhase") {
        next.connectionPhase = enumWithin(value, static_cast<int>(TunerModel::ConnectionPhase::Error),
                                          TunerModel::ConnectionPhase::Disabled);
    } else if (propertyName == "connectionError") {
        next.connectionError = value.toString();
    } else if (propertyName == "driver") {
        next.driver = enumWithin(value, static_cast<int>(Driver::RotctldStarted), Driver::None);
    } else if (propertyName == "label") {
        next.label = value.toString();
    } else if (propertyName == "serialPort") {
        next.serialPort = value.toString();
    } else if (propertyName == "baud") {
        next.baud = value.toInt();
    } else if (propertyName == "host") {
        next.host = value.toString();
    } else if (propertyName == "port") {
        next.port = qBound(0, value.toInt(), 65535);
    } else if (propertyName == "serialPorts") {
        next.serialPorts = value.toString();
    } else if (propertyName == "axes") {
        next.axes = enumWithin(value, static_cast<int>(Axes::AzimuthElevation), Axes::Azimuth);
    } else if (propertyName == "rangeDeg") {
        next.rangeDeg = value.toInt();
    } else if (propertyName == "endStop") {
        next.endStop = enumWithin(value, static_cast<int>(EndStop::South), EndStop::None);
    } else if (propertyName == "spanPositionDeg") {
        next.spanPositionDeg = finiteOr(value, -1.0);
    } else if (propertyName == "travelDeg") {
        next.travelDeg = finiteOr(value, 0.0);
    } else if (propertyName == "routeKnown") {
        next.routeKnown = value.toBool();
    } else if (propertyName == "offsetDeg") {
        next.offsetDeg = finiteOr(value, 0.0);
    } else if (propertyName == "hamlibModel") {
        next.hamlibModel = value.toInt();
    } else if (propertyName == "rotctldAvailable") {
        next.rotctldAvailable = value.toBool();
    } else if (propertyName == "positionFresh") {
        next.positionFresh = value.toBool();
    } else if (propertyName == "azimuthDeg") {
        next.azimuthDeg = finiteOr(value, -1.0);
    } else if (propertyName == "elevationDeg") {
        next.elevationDeg = finiteOr(value, -1.0);
    } else if (propertyName == "targetAzimuthDeg") {
        next.targetAzimuthDeg = finiteOr(value, -1.0);
    } else if (propertyName == "targetElevationDeg") {
        next.targetElevationDeg = finiteOr(value, -1.0);
    } else if (propertyName == "motion") {
        next.motion = enumWithin(value, static_cast<int>(Motion::Nudging), Motion::Stopped);
    } else if (propertyName == "presets") {
        next.presets = value.toString();
    } else if (propertyName == "fault") {
        next.fault = value.toString();
    } else {
        return false;
    }
    setState(next);
    return true;
}

} // namespace NereusSDR::RotorLink
