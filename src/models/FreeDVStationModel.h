// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - FreeDVStationModel: live station map keyed by Socket.IO sid.
//
// Modeled on freedv-gui src/gui/dialogs/freedv_reporter.h::ReporterData
// [@77e793a] (the dialog's internal stations map). Drives the standalone
// FreeDVReporterDialog (Phase G) plus the Stations tab in SpotHub (if any).
//
// Distance + heading calculations from freedv-gui's
// calculateLatLonFromGridSquare_ / calculateDistance_ /
// calculateBearingInDegrees_ helpers [@77e793a]. Haversine-based
// great-circle distance + initial bearing.
//
// NEW model with no AetherSDR equivalent. AetherSDR's FreeDvClient
// throws away 9 of the 14 station fields per design doc Section 4
// Flow 2; this model is the gap-fill.
//
// License (upstream): freedv-gui carries an LGPLv2.1+ root license
// (`freedv-gui/COPYING`); the specific `freedv_reporter.h` /
// `freedv_reporter.cpp` files have no per-file Copyright header, so the
// project root header applies. LGPL is upgrade-compatible to GPLv2-or-later
// when linked into a GPL work (LGPL section 3 conversion clause), which is
// the model NereusSDR uses.
//
// Ported from freedv-gui src/gui/dialogs/freedv_reporter.h:367-417
// (`ReporterData` per-station inner struct shape) and
// freedv-gui src/gui/dialogs/freedv_reporter.cpp:2312-2410
// (calculateDistance_ / calculateLatLonFromGridSquare_ /
// calculateBearingInDegrees_ / DegreesToRadians_ / RadiansToDegrees_)
// [@77e793a].
//
// Copyright (C) 2026 NereusSDR contributors.
// Distance / heading math: derived from freedv-gui source (LGPLv2.1+,
// copyright the freedv-gui contributors / FreeDV project).
//
// Modification history (NereusSDR)
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D3. Initial create.
//                                    NEW model with no AetherSDR
//                                    equivalent. Holds QHash<QString sid,
//                                    FreeDVStation> indexed by Socket.IO
//                                    session ID. Subscribes to
//                                    FreeDVReporterClient signals
//                                    (stationAdded / stationUpdated /
//                                    stationRemoved) and re-emits them
//                                    after applying the haversine
//                                    distance / initial-bearing
//                                    computation when our grid is set.
//                                    AI tooling: Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QHash>
#include <QString>

#include "core/FreeDVStation.h"

namespace NereusSDR {

// NEW class - no upstream equivalent.
// Sink for FreeDVReporterClient station events; presents the live
// QHash<sid, FreeDVStation> to the FreeDVReporterDialog and the
// Spot Hub Stations tab.
class FreeDVStationModel : public QObject {
    Q_OBJECT
public:
    explicit FreeDVStationModel(QObject* parent = nullptr);

    QHash<QString, FreeDVStation> stations() const;
    FreeDVStation stationBySid(const QString& sid) const;
    int stationCount() const;

    // When set (non-empty 4-or-6 character Maidenhead grid), the
    // model recomputes distanceKm / headingDeg for every station
    // already in the map and stamps the same fields onto subsequent
    // add / update events. Empty grid disables the computation.
    void setOurGridSquare(const QString& grid);

public slots:
    void onStationAdded(const QString& sid, const FreeDVStation& info);
    void onStationUpdated(const QString& sid, const FreeDVStation& info);
    void onStationRemoved(const QString& sid);
    void clear();

signals:
    void stationAdded(const QString& sid, const FreeDVStation& info);
    void stationUpdated(const QString& sid, const FreeDVStation& info);
    void stationRemoved(const QString& sid);
    void cleared();

private:
    // Stamp distanceKm / headingDeg onto `info` based on m_ourGrid +
    // info.gridSquare. No-op when either grid is empty / shorter than
    // 4 characters.
    void applyDistanceHeading(FreeDVStation& info) const;

    QHash<QString, FreeDVStation> m_stations;
    QString m_ourGrid;
};

} // namespace NereusSDR
