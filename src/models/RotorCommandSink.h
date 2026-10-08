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
// =================================================================

#include "core/NereusCoreExport.h"

#include <QString>

namespace NereusSDR {

class NEREUS_CORE_EXPORT RotorCommandSink {
public:
    virtual ~RotorCommandSink() = default;

    /// Turn to `azimuthDeg` (compass, 0 to 360) and, on an az/el rotor,
    /// `elevationDeg` (0 to 90; -1 leaves elevation as it is). True when
    /// the command went to the rotor (locally) or left for the Core
    /// (remotely; the Core's verdict follows on stationCommandFinished).
    /// False with the reason in plain words otherwise.
    virtual bool requestRotorTarget(double azimuthDeg, double elevationDeg,
                                    QString* reason) = 0;
    /// Stop now. Same answer as requestRotorTarget.
    virtual bool requestStopRotor(QString* reason) = 0;
};

} // namespace NereusSDR
