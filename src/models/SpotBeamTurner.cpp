// no-port-check: NereusSDR-original. See SpotBeamTurner.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/SpotBeamTurner.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 8).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "models/SpotBeamTurner.h"

#include "core/AppSettings.h"
#include "core/DxccColorProvider.h"
#include "core/GreatCircle.h"
#include "core/SpotSourceHost.h"
#include "core/StationRotorController.h"
#include "models/RadioModel.h"
#include "models/RotorCommandSink.h"
#include "models/RotorModel.h"

#include <cmath>

namespace NereusSDR {

namespace {

// A spot record's "not known" (the remote rotor control contract, "Bearings
// on spots"); never 0, which is north.
constexpr double kNoBearingDeg = -1.0;

bool isBearing(double deg)
{
    return std::isfinite(deg) && deg >= 0.0 && deg < 360.0;
}

void setReason(QString* out, const QString& reason)
{
    if (out != nullptr) {
        *out = reason;
    }
}

} // namespace

SpotBeamTurner::SpotBeamTurner(RadioModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_commands(model)
    , m_viaModel(true)
    , m_rotor(model ? model->rotorModel() : nullptr)
    , m_dxcc(model ? model->dxccColorProvider() : nullptr)
{
}

SpotBeamTurner::SpotBeamTurner(RotorCommandSink* commands, RotorLink::RotorModel* rotor,
                               DxccColorProvider* dxcc, QObject* parent)
    : QObject(parent)
    , m_commands(commands)
    , m_rotor(rotor)
    , m_dxcc(dxcc)
{
}

RotorCommandSink* SpotBeamTurner::commands() const
{
    // The window's RadioModel is its command sink; once it is gone there is
    // nothing to send through.
    if (m_viaModel && m_model.isNull()) {
        return nullptr;
    }
    return m_commands;
}

double SpotBeamTurner::bearingFor(const QString& call, double servedBearingDeg) const
{
    if (isBearing(servedBearingDeg)) {
        return servedBearingDeg;
    }
    // Not served by a Core (the desktop's own spot sources, or a Core that
    // could not place it): this window's cty.dat and the station's grid
    // square, worked out as the Core works out a spot record's bearingDeg.
    const double local = SpotSourceHost::spotBearingDeg(call, m_dxcc.data(),
                                                        SpotSourceHost::freedvGridSquare());
    return isBearing(local) ? local : kNoBearingDeg;
}

QString SpotBeamTurner::noBearingReason() const
{
    if (!GreatCircle::fromMaidenhead(SpotSourceHost::freedvGridSquare()).has_value()) {
        return StationRotorController::noGridReason();
    }
    return StationRotorController::callNotPlacedReason();
}

QString SpotBeamTurner::rotorUnavailableReason() const
{
    RotorCommandSink* sink = commands();
    if (sink == nullptr) {
        return StationRotorController::noRotorReason();
    }
    QString reason;
    if (!sink->rotorControlAvailable(&reason)) {
        return reason.isEmpty() ? StationRotorController::noRotorReason() : reason;
    }
    if (m_rotor.isNull() || m_rotor->driver() == RotorLink::RotorModel::Driver::None) {
        return StationRotorController::noRotorReason();
    }
    if (m_rotor->connectionPhase() != TunerModel::ConnectionPhase::Connected) {
        return StationRotorController::notConnectedReason();
    }
    return {};
}

SpotBeamTurner::Action SpotBeamTurner::turnAction(const QString& call,
                                                  double servedBearingDeg) const
{
    Action a;
    a.bearingDeg = bearingFor(call, servedBearingDeg);
    a.text = isBearing(a.bearingDeg)
        ? QStringLiteral("Turn beam to %1 (%2)").arg(call, degreesText(a.bearingDeg))
        : QStringLiteral("Turn beam to %1").arg(call);
    a.reason = rotorUnavailableReason();
    if (a.reason.isEmpty() && !isBearing(a.bearingDeg)) {
        a.reason = noBearingReason();
    }
    a.enabled = a.reason.isEmpty();
    return a;
}

bool SpotBeamTurner::turnBeam(const QString& call, double servedBearingDeg, QString* reason)
{
    const Action a = turnAction(call, servedBearingDeg);
    if (!a.enabled) {
        setReason(reason, a.reason);
        emit turnRefused(a.reason);
        return false;
    }
    QString why;
    // A Core-served bearing is the Core's own number: turn to it. Any other
    // spot turns to its callsign, so the rotor's computer works the bearing
    // out from its own cty.dat and grid square.
    const bool sent = isBearing(servedBearingDeg)
        ? commands()->requestRotorTarget(servedBearingDeg, -1.0, &why)
        : commands()->requestTurnRotorToCall(call.trimmed().toUpper(), false, &why);
    if (!sent) {
        setReason(reason, why);
        emit turnRefused(why);
    }
    return sent;
}

bool SpotBeamTurner::turnOnTune()
{
    return AppSettings::instance()
               .value(QString::fromLatin1(kTurnOnTuneKey), QStringLiteral("False"))
               .toString()
        == QLatin1String("True");
}

void SpotBeamTurner::setTurnOnTune(bool on)
{
    AppSettings::instance().setValue(QString::fromLatin1(kTurnOnTuneKey),
                                     on ? QStringLiteral("True") : QStringLiteral("False"));
}

QString SpotBeamTurner::degreesText(double bearingDeg)
{
    if (!isBearing(bearingDeg)) {
        return {};
    }
    int whole = static_cast<int>(std::lround(bearingDeg));
    if (whole >= 360) {
        whole -= 360;
    }
    return QString::number(whole) + QChar(0x00B0);
}

void SpotBeamTurner::spotTuned(const QString& call, double servedBearingDeg)
{
    // Off by default: with it off, tuning never moves the rotor.
    if (!turnOnTune()) {
        return;
    }
    // A tune is not a request to turn: with no rotor to turn or no bearing
    // to turn to, the tune happens and nothing is said.
    if (!turnAction(call, servedBearingDeg).enabled) {
        return;
    }
    turnBeam(call, servedBearingDeg);
}

} // namespace NereusSDR
