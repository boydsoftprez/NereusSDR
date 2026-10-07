#pragma once
// no-port-check: NereusSDR-original. The Core's CAT setup from a connected
// desktop: the `stationCat` publisher and its four commands.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/cat/StationCatController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// On the Core (the Local role: nereusd, or a desktop running its own
// radio) this fills StationCatModel from CatService and slice changes, and
// applies a window's setStationCatChannel, setStationCatGlobal,
// testStationCatCommand and refreshStationCatDevices through CatService,
// as the Core's TCI server's settings go through StationTciController.
//
// A window sends a channel's binding as slice ids only. Each id is
// resolved here, at apply time, to the slice's live incarnation on the
// Core when it names a different slice than the channel holds; an id the
// channel already holds keeps the incarnation it was bound with, as a
// transport-only edit in the local window does, so a channel bound to a
// slice that was closed stays an invalid binding until a slice is picked.
//
// The design: docs/architecture/2026-10-07-remote-cat-setup-plan.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, stationCatVersion 1).
//                                    AI tooling: Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

namespace NereusSDR {

class CatService;
class RadioModel;
class StationCatModel;

class NEREUS_CORE_EXPORT StationCatController : public QObject {
    Q_OBJECT
public:
    /// The plan's refusal text for a channel the Core did not take (the
    /// local page's own words).
    static QString channelRefusedReason();
    /// The same for the global settings.
    static QString globalRefusedReason();

    StationCatController(RadioModel* model, CatService* service, StationCatModel* station,
                         QObject* parent = nullptr);

    /// setStationCatChannel: `configJson` is a StationCatModel channel
    /// config object; fields it leaves out keep the channel's values.
    bool setChannel(int channel, const QString& configJson, QString* reason);
    /// setStationCatGlobal: `configJson` is a StationCatModel global config
    /// object; fields it leaves out keep the Core's values.
    bool setGlobal(const QString& configJson, QString* reason);
    /// testStationCatCommand: runs `command` on `channel` as the local
    /// tester does; the reply lands in the object's lastTest.
    bool testCommand(qint64 requestId, int channel, const QString& command, QString* reason);
    /// refreshStationCatDevices: reads this computer's serial ports again.
    void refreshDevices();

    /// Fills every property from CatService now.
    void publishAll();

private:
    void publishGlobal();
    void publishChannel(int channel);
    void publishPlatform();

    QPointer<RadioModel> m_model;
    QPointer<CatService> m_service;
    QPointer<StationCatModel> m_station;
    QStringList m_serialDevices;
};

} // namespace NereusSDR
