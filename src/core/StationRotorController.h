// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/StationRotorController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native. The Core's antenna rotor: owns the RotorConnection,
// the setup saved under Rotor/* in AppSettings, the presets, the target
// and its arrival, Stop ahead of everything, and the hold dead man that
// stops a nudge when its window lets go, goes quiet or goes away.
//
// The rotor object's properties and the commands' refusals are the
// contract's (docs/architecture/2026-10-07-remote-rotor-control-v1.md);
// the link that mirrors them onto the session (rotor control plan,
// Task 4) reads the getters below and calls the command methods, each of
// which returns true when the command went to the rotor or false with the
// contract's reason word for word.
//
// Design: docs/architecture/2026-10-07-rotor-control-design.md (Core).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code. Rotor control plan, Task 3c.
// =================================================================

#pragma once

#include "core/GreatCircle.h"
#include "core/NereusCoreExport.h"
#include "core/RotorConnection.h"
#include "models/TunerModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <optional>

namespace NereusSDR {

// The accessory connection phase table (remote accessory control v1,
// "Connection phase"), which the contract's `connectionPhase` uses.
using RotorConnectionPhase = TunerModel::ConnectionPhase;

// The wire's `motion` enum. Values are only ever appended.
enum class RotorMotion {
    Stopped = 0,
    Turning = 1,   // turning to a target
    Nudging = 2,   // a window is holding a turn button
};

class NEREUS_CORE_EXPORT StationRotorController : public QObject {
    Q_OBJECT
public:
    // The hold dead man (contract, "The hold dead man"): a window repeats
    // nudgeRotor with active true every 250 ms while a turn button is
    // held; with no repeat for 750 ms the Core sends stop. Both numbers
    // are this design's choice, not a device fact. The repeat rate is the
    // windows' to keep and is not enforced here.
    static constexpr int kHoldRepeatMs = 250;
    static constexpr int kHoldLapseMs = 750;

    // Refusals, word for word (contract, "Refusals").
    static QString noRotorReason();
    static QString notConnectedReason();
    static QString azimuthOnlyReason();
    static QString notANumberReason();
    static QString outOfRangeReason();
    static QString noGridReason();
    static QString callNotPlacedReason();
    static QString rotctldMissingReason();
    static QString unknownSerialPortReason();

    // A callsign's position from the Core's cty.dat, or std::nullopt.
    using CallsignLocator = std::function<std::optional<GeoPosition>(const QString&)>;
    // The serial ports the Core's computer has.
    using SerialPortLister = std::function<QStringList()>;

    explicit StationRotorController(QObject* parent = nullptr);
    ~StationRotorController() override;

    // Reads the saved setup and connects; with driver none it stays idle
    // (phase disabled).
    void start();

    // The Core's cty.dat lookup, for turnRotorToCall. Unset, no callsign
    // can be placed.
    void setCallsignLocator(CallsignLocator locator);

    RotorConnection* connection() const { return m_connection; }

    // ── The rotor object's properties (contract, "The rotor object") ──
    RotorConnectionPhase connectionPhase() const { return m_phase; }
    QString connectionError() const { return m_connectionError; }
    const RotorConfig& config() const { return m_config; }
    RotorDriver driver() const { return m_config.driver; }
    // What the status line shows, for example "Yaesu GS-232B on COM4";
    // empty with driver none.
    QString label() const;
    // GS-232 drivers and driver 4; empty for driver 3.
    QString serialPort() const;
    // Driver 3; empty otherwise.
    QString host() const;
    // The Core's serial ports, sorted.
    QStringList serialPorts() const;
    // The same, one per line, as the property is sent.
    QString serialPortsText() const { return serialPorts().join(QLatin1Char('\n')); }
    bool rotctldAvailable() const;
    bool positionFresh() const;
    // Compass heading after the offset, 0 to under 360; -1 when unknown.
    // Where the rotor is in the overlap is spanPositionDeg().
    double azimuthDeg() const;
    double elevationDeg() const;
    double spanPositionDeg() const;
    double targetAzimuthDeg() const;
    double targetElevationDeg() const;
    // Signed travel still to make to the target along the route the
    // controller will take (negative is counter-clockwise); 0 with no
    // target or no route.
    double travelDeg() const;
    // False while the span position is unknown with a target set.
    bool routeKnown() const;
    RotorMotion motion() const;
    // `name<TAB>degrees`, one per line, in the operator's order.
    QString presets() const { return m_presets; }
    // The last fault in plain words; empty otherwise.
    QString fault() const { return m_fault; }
    // The window session holding a nudge, or 0.
    quint64 holdSessionId() const { return m_holdActive ? m_holdSession : 0; }

    // ── Commands (contract, "Commands") ──
    // Each returns true when the command went to the rotor; false with the
    // contract's reason otherwise. Turning is allowed while the radio is
    // on the air: nothing here consults transmit.
    bool setRotorTarget(double azimuthDeg, double elevationDeg, QString* reason);
    bool turnRotorToCall(const QString& call, bool longPath, QString* reason);
    // Never refused while connected; written ahead of anything queued.
    bool stopRotor(QString* reason);
    // `active` true starts a hold or keeps it going; false ends it. A
    // hold that hears nothing for kHoldLapseMs stops.
    bool nudgeRotor(RotorDirection direction, bool active, quint64 sessionId,
                    QString* reason);
    // Saves under Rotor/* and connects again; driver none disconnects and
    // forgets the setup (the presets are kept).
    bool configureRotor(const RotorConfig& config, QString* reason);
    // Disconnects, keeping the setup.
    bool disconnectRotor(QString* reason);
    // Replaces the presets (`name<TAB>degrees` per line). A heading that
    // is not a number or is outside 0 to 360 is refused.
    bool setRotorPresets(const QString& presets, QString* reason);

    // A window's session ended: a hold it was keeping stops.
    void sessionEnded(quint64 sessionId);

    // ── Test seams ──
    void setSerialPortListerForTesting(SerialPortLister lister);

signals:
    // Any property above changed (the session link sends a delta).
    void stateChanged();
    // The headings, freshness or span moved (a subset of stateChanged).
    void positionChanged();

private:
    void loadSettings();
    void saveSettings() const;
    void connectNow();
    void setPhase(RotorConnectionPhase phase, const QString& error = {});
    void endHold();
    void onHoldLapsed();
    // The turn-command refusals shared by every command: no rotor, not
    // connected.
    bool turnable(QString* reason) const;
    bool acceptTarget(double azimuthDeg, double elevationDeg, QString* reason);

    RotorConnection* m_connection{nullptr};   // Qt child
    RotorConfig m_config;
    QString m_presets;
    RotorConnectionPhase m_phase{RotorConnectionPhase::Disabled};
    QString m_connectionError;
    QString m_fault;
    // The operator wants the rotor connected (a drop is then retrying).
    bool m_wantConnected{false};
    CallsignLocator m_locator;
    SerialPortLister m_portLister;

    QTimer m_holdTimer;
    bool m_holdActive{false};
    quint64 m_holdSession{0};
    RotorDirection m_holdDirection{RotorDirection::Cw};
};

} // namespace NereusSDR
