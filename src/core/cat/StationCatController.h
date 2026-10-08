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
//                                    Review fixes: explicit rebinds, the
//                                    tester's reply in its result, device
//                                    reads at most once a second.
//                                    AI tooling: Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

class QTimer;

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
    /// refreshStationCatDevices reads the serial devices at most this
    /// often; a request sooner is answered when the time is up.
    static constexpr int kDeviceRefreshMinimumMs = 1000;

    StationCatController(RadioModel* model, CatService* service, StationCatModel* station,
                         QObject* parent = nullptr);

    /// setStationCatChannel: `configJson` is a StationCatModel channel
    /// config object; fields it leaves out keep the channel's values. A
    /// slice id that differs from the channel's, or one flagged
    /// primaryRebind / secondaryRebind, binds to its live slice; any other
    /// keeps the binding the channel holds.
    bool setChannel(int channel, const QString& configJson, QString* reason);
    /// setStationCatGlobal: `configJson` is a StationCatModel global config
    /// object; fields it leaves out keep the Core's values.
    bool setGlobal(const QString& configJson, QString* reason);
    /// testStationCatCommand: runs `command` on `channel` as the local
    /// tester does; the reply is `reply` (the command's result) and lands
    /// in the object's lastTest for other windows.
    bool testCommand(qint64 requestId, int channel, const QString& command, QString* reply,
                     QString* reason);
    /// refreshStationCatDevices: reads this computer's serial ports again,
    /// at most once each kDeviceRefreshMinimumMs.
    void refreshDevices();

    /// Tests: what reads the serial devices (QSerialPortInfo otherwise).
    void setSerialDeviceListerForTest(std::function<QStringList()> lister);

    /// Fills every property from CatService now.
    void publishAll();

private:
    void publishGlobal();
    void publishChannel(int channel);
    void publishPlatform();
    void readDevices();

    QPointer<RadioModel> m_model;
    QPointer<CatService> m_service;
    QPointer<StationCatModel> m_station;
    QStringList m_serialDevices;
    std::function<QStringList()> m_deviceLister;
    /// Since the devices were last read; a request within the minimum
    /// waits on m_deviceRefresh.
    QElapsedTimer m_deviceClock;
    QTimer* m_deviceRefresh{nullptr};
};

} // namespace NereusSDR
