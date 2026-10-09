#pragma once
// no-port-check: NereusSDR-original. Turning the beam to a spot, for the
// pan's spot menu and the Spot Hub, and the auto-turn preference.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/SpotBeamTurner.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Rotor control plan, Task 8.
//
// One place that answers, for a spot, "can this window turn the beam to
// it, to what bearing, and if not, why not", and that sends the turn. The
// pan's spot menu and the Spot Hub ask it, so both say the same thing:
//
//   * The bearing is the Core's (a spot record's bearingDeg) when the Core
//     served one. Otherwise it is worked out here from the window's cty.dat
//     and the station's grid square (FreeDvReporter/GridSquare, else
//     User/GridSquare, as the Core does), so the desktop's own spot sources
//     show a bearing too.
//   * A spot with a Core-served bearing turns to that bearing
//     (setRotorTarget); any other spot turns to its callsign
//     (turnRotorToCall) and the rotor's computer works the bearing out.
//   * Turn beam is offered greyed with the reason when this window has no
//     rotor, the rotor is not connected, or the spot has no bearing
//     (disabled, never hidden).
//   * Auto-turn: "Turn the beam when I tune to a spot" (Rotor/TurnOnTune,
//     a window preference, off by default). With it off, tuning to a spot
//     never moves the rotor. Turning while on the air is allowed (JJ,
//     2026-10-07; docs/architecture/2026-10-07-rotor-control-design.md).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 8).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"

#include <QObject>
#include <QPointer>
#include <QString>

namespace NereusSDR {

class DxccColorProvider;
class RadioModel;
class RotorCommandSink;
namespace RotorLink { class RotorModel; }

class NEREUS_CORE_EXPORT SpotBeamTurner : public QObject {
    Q_OBJECT

public:
    /// The window preference "Turn the beam when I tune to a spot".
    static constexpr const char* kTurnOnTuneKey = "Rotor/TurnOnTune";

    /// What the Turn beam action shows for one spot.
    struct Action {
        QString text;          ///< "Turn beam to JA1ABC (330°)", or without a bearing
        bool enabled{false};
        QString reason;        ///< why it is greyed; empty when enabled
        double bearingDeg{-1.0};  ///< short path, 0 to under 360; -1 when none
    };

    /// The window's own: RadioModel's rotor commands, its rotor object and
    /// its cty.dat table.
    explicit SpotBeamTurner(RadioModel* model, QObject* parent = nullptr);
    /// Tests: any command sink, rotor object and cty.dat table (each may be
    /// null).
    SpotBeamTurner(RotorCommandSink* commands, RotorLink::RotorModel* rotor,
                   DxccColorProvider* dxcc, QObject* parent = nullptr);

    /// The spot's short-path bearing: `servedBearingDeg` when the Core
    /// served one (0 to under 360), else worked out from this window's
    /// cty.dat and the station's grid square; -1 when there is none.
    double bearingFor(const QString& call, double servedBearingDeg) const;
    /// Why a spot with no bearing has none: no grid square (or one that
    /// cannot be read), else the call could not be placed.
    QString noBearingReason() const;

    /// Why this window cannot turn its rotor now (no rotor control, no
    /// rotor set up, the rotor not connected); empty when it can.
    QString rotorUnavailableReason() const;

    /// The Turn beam action for a spot.
    Action turnAction(const QString& call, double servedBearingDeg) const;

    /// Turn the beam to the spot. True when the command went to the rotor
    /// (or left for the Core); false with the reason otherwise, which is
    /// also emitted on turnRefused.
    bool turnBeam(const QString& call, double servedBearingDeg, QString* reason = nullptr);

    /// The auto-turn preference (Rotor/TurnOnTune, "True"/"False", default
    /// "False").
    static bool turnOnTune();
    static void setTurnOnTune(bool on);

    /// "330°": a bearing as the menus and the Spot Hub show it, whole
    /// degrees (359.6 reads 0°); empty for -1.
    static QString degreesText(double bearingDeg);

public slots:
    /// The operator tuned to a spot. With auto-turn on and the beam able to
    /// turn to it, turns; otherwise does nothing (and never moves the
    /// rotor with auto-turn off).
    void spotTuned(const QString& call, double servedBearingDeg);

signals:
    /// A turn this window asked for was refused here, with the reason in
    /// plain words (a remote Core's refusal follows on
    /// RadioModel::accessoryRequestRefused, device "rotor").
    void turnRefused(const QString& reason);

private:
    RotorCommandSink* commands() const;

    QPointer<RadioModel> m_model;
    RotorCommandSink* m_commands{nullptr};
    bool m_viaModel{false};
    QPointer<RotorLink::RotorModel> m_rotor;
    QPointer<DxccColorProvider> m_dxcc;
};

} // namespace NereusSDR
