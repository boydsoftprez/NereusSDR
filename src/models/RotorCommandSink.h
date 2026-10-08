#pragma once
// no-port-check: NereusSDR-original. One way for a window to turn the
// antenna rotor, whether this process runs it or a remote Core does.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/RotorCommandSink.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Rotor control plan, Task 5.
//
// The GUI turns the rotor through this and never asks where the rotor
// runs. RadioModel implements it: on a desktop running its own radio (and
// on nereusd) the local StationRotorController takes the command; in a
// window on a remote Core the command goes over the station link
// (IStationLink::requestRotorTarget and friends). What the rotor then does
// arrives on RadioModel::rotorModel() like everything else about it.
//
// Headings are compass degrees, 0 to 360 (360 means north), as the remote
// rotor control contract says
// (docs/architecture/2026-10-07-remote-rotor-control-v1.md, "Commands").
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 5).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 6: what the
//                                    Rotor applet needs (whether a window
//                                    may turn the rotor, turn to a callsign,
//                                    the nudge hold, the last command's id).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 7: the rotor
//                                    setup (configureRotor) and the presets
//                                    (setRotorPresets), for the Rotor Setup
//                                    page. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Final review I3: a setup view says when
//                                    it opens and closes, so the rotor's
//                                    computer reads its ports only then.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"

#include <QString>
#include <QtGlobal>

namespace NereusSDR {

class NEREUS_CORE_EXPORT RotorCommandSink {
public:
    /// The contract's `nudgeRotor` direction enum (0 CCW, 1 CW, 2 down,
    /// 3 up). Values are only ever appended.
    enum class Nudge {
        Ccw = 0,
        Cw = 1,
        Down = 2,
        Up = 3,
    };

    /// The contract's `configureRotor` arguments, in its wire numbers
    /// (driver 0 none, 1 GS-232A, 2 GS-232B, 3 rotctld already running,
    /// 4 rotctld started by the Core; axes 0 azimuth, 1 azimuth and
    /// elevation; endStop 0 none, 1 north, 2 south). The defaults are the
    /// contract's ("Core-owned settings").
    struct Setup {
        int driver{0};
        QString serialPort;
        int baud{9600};
        QString host;
        int port{4533};
        int hamlibModel{0};   // used for driver 4 only
        int axes{0};
        int endStop{1};
        int rangeDeg{360};    // 360, or 450 for a rotor with overlap
        double offsetDeg{0.0};
    };

    virtual ~RotorCommandSink() = default;

    /// True when this window can turn a rotor at all: this process runs
    /// the rotor, or the remote Core advertises remoteRotorControlVersion
    /// 1. False with the contract's reason otherwise ("This Core does not
    /// control a rotor. Updating the Core may help."). Whether a rotor is
    /// set up and connected is the rotor object's to say.
    virtual bool rotorControlAvailable(QString* reason) const = 0;

    /// Turn to `azimuthDeg` (compass, 0 to 360) and, on an az/el rotor,
    /// `elevationDeg` (0 to 90; -1 leaves elevation as it is). True when
    /// the command went to the rotor (locally) or left for the Core
    /// (remotely; the Core's verdict follows on stationCommandFinished).
    /// False with the reason in plain words otherwise.
    virtual bool requestRotorTarget(double azimuthDeg, double elevationDeg,
                                    QString* reason) = 0;
    /// Stop now. Same answer as requestRotorTarget.
    virtual bool requestStopRotor(QString* reason) = 0;
    /// Turn to `call`: the Core works out the bearing from its cty.dat and
    /// the station's grid square, the long path when `longPath`. Same
    /// answer as requestRotorTarget.
    virtual bool requestTurnRotorToCall(const QString& call, bool longPath,
                                        QString* reason) = 0;
    /// A turn button held (`active` true, repeated every 250 ms while
    /// held: the contract's hold dead man) or let go (`active` false).
    /// Same answer as requestRotorTarget.
    virtual bool requestNudgeRotor(Nudge direction, bool active, QString* reason) = 0;
    /// Save the rotor setup on the rotor's computer and connect again;
    /// driver 0 disconnects and forgets the setup. Same answer as
    /// requestRotorTarget; refusals are the contract's ("That rotor setup
    /// is not valid.", "That serial port is not on the Core's computer.",
    /// "Hamlib's rotctld is not installed on the Core's computer.").
    virtual bool requestConfigureRotor(const Setup& setup, QString* reason) = 0;
    /// Replace the presets (`name<TAB>degrees` per line, the `presets`
    /// property's form). Same answer as requestRotorTarget.
    virtual bool requestRotorPresets(const QString& presets, QString* reason) = 0;
    /// A rotor setup view opened (true) or closed (false). The rotor's
    /// computer reads its serial ports and looks for rotctld only while a
    /// rotor is set up or a setup view is open; a view on a remote Core
    /// asks it to (`refreshRotorPorts`) while open. Nothing to refuse.
    virtual void setRotorSetupViewOpen(bool open) { Q_UNUSED(open); }
    /// The id the last rotor command left for a remote Core under, so a
    /// window can tell the Core's verdict on it apart
    /// (RadioModel::stationCommandFinished); 0 when it went to this
    /// process's own rotor, which answers at once.
    virtual quint32 lastRotorCommandId() const = 0;
};

} // namespace NereusSDR
