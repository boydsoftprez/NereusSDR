#pragma once
// no-port-check: NereusSDR-original. The Core's antenna rotor as the
// mirrored, read-only `rotor` object.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/RotorModel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Rotor control plan, Task 4b.
//
// What the Core's rotor is doing: its connection, its setup, where it
// points and where it is going, its presets and its last fault. The Core
// mirrors it as the read-only `rotor` object (remoteRotorControlVersion 1)
// and every window reads the same copy. It changes only through the rotor
// commands (setRotorTarget, turnRotorToCall, stopRotor, nudgeRotor,
// configureRotor, disconnectRotor, setRotorPresets), never by writing this
// object.
//
// The class lives in NereusSDR::RotorLink because NereusSDR::RotorModel is
// already the Hamlib model list entry (core/RotorModels.h, ported from
// Longpath). The wire names a class by its short name, so the contract's
// class `RotorModel` is this one.
//
// The wire contract: docs/architecture/2026-10-07-remote-rotor-control-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 4b).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "models/TunerModel.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>

namespace NereusSDR {

class StationRotorController;

namespace RotorLink {

class NEREUS_CORE_EXPORT RotorModel : public QObject {
    Q_OBJECT
    // The contract's property table, in its order. Where the rotor points
    // (and how sure the Core is of it) moves on positionChanged; everything
    // else on stateChanged.
    Q_PROPERTY(NereusSDR::TunerModel::ConnectionPhase connectionPhase READ connectionPhase NOTIFY stateChanged)
    Q_PROPERTY(QString connectionError READ connectionError NOTIFY stateChanged)
    Q_PROPERTY(NereusSDR::RotorLink::RotorModel::Driver driver READ driver NOTIFY stateChanged)
    Q_PROPERTY(QString label READ label NOTIFY stateChanged)
    Q_PROPERTY(QString serialPort READ serialPort NOTIFY stateChanged)
    Q_PROPERTY(int baud READ baud NOTIFY stateChanged)
    Q_PROPERTY(QString host READ host NOTIFY stateChanged)
    Q_PROPERTY(int port READ port NOTIFY stateChanged)
    Q_PROPERTY(QString serialPorts READ serialPorts NOTIFY stateChanged)
    Q_PROPERTY(NereusSDR::RotorLink::RotorModel::Axes axes READ axes NOTIFY stateChanged)
    Q_PROPERTY(int rangeDeg READ rangeDeg NOTIFY stateChanged)
    Q_PROPERTY(NereusSDR::RotorLink::RotorModel::EndStop endStop READ endStop NOTIFY stateChanged)
    Q_PROPERTY(double spanPositionDeg READ spanPositionDeg NOTIFY positionChanged)
    Q_PROPERTY(double travelDeg READ travelDeg NOTIFY positionChanged)
    Q_PROPERTY(bool routeKnown READ routeKnown NOTIFY positionChanged)
    Q_PROPERTY(double offsetDeg READ offsetDeg NOTIFY stateChanged)
    Q_PROPERTY(int hamlibModel READ hamlibModel NOTIFY stateChanged)
    Q_PROPERTY(bool rotctldAvailable READ rotctldAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool positionFresh READ positionFresh NOTIFY positionChanged)
    Q_PROPERTY(double azimuthDeg READ azimuthDeg NOTIFY positionChanged)
    Q_PROPERTY(double elevationDeg READ elevationDeg NOTIFY positionChanged)
    Q_PROPERTY(double targetAzimuthDeg READ targetAzimuthDeg NOTIFY stateChanged)
    Q_PROPERTY(double targetElevationDeg READ targetElevationDeg NOTIFY stateChanged)
    Q_PROPERTY(NereusSDR::RotorLink::RotorModel::Motion motion READ motion NOTIFY stateChanged)
    Q_PROPERTY(QString presets READ presets NOTIFY stateChanged)
    Q_PROPERTY(QString fault READ fault NOTIFY stateChanged)

public:
    // The contract's enum tables. Values are only ever appended.
    enum class Driver {
        None = 0,
        Gs232a = 1,
        Gs232b = 2,
        Rotctld = 3,          // Hamlib rotctld already running (host, port)
        RotctldStarted = 4,   // Hamlib rotctld started by the Core
    };
    Q_ENUM(Driver)
    enum class Axes {
        Azimuth = 0,
        AzimuthElevation = 1,
    };
    Q_ENUM(Axes)
    enum class EndStop {
        None = 0,
        North = 1,
        South = 2,
    };
    Q_ENUM(EndStop)
    enum class Motion {
        Stopped = 0,
        Turning = 1,
        Nudging = 2,
    };
    Q_ENUM(Motion)

    struct State {
        TunerModel::ConnectionPhase connectionPhase{TunerModel::ConnectionPhase::Disabled};
        QString connectionError;
        Driver driver{Driver::None};
        QString label;
        QString serialPort;
        int baud{9600};
        QString host;
        int port{4533};
        QString serialPorts;
        Axes axes{Axes::Azimuth};
        int rangeDeg{360};
        EndStop endStop{EndStop::North};
        double offsetDeg{0.0};
        int hamlibModel{0};
        bool rotctldAvailable{false};
        double targetAzimuthDeg{-1.0};
        double targetElevationDeg{-1.0};
        Motion motion{Motion::Stopped};
        QString presets;
        QString fault;
        // Position (positionChanged).
        double spanPositionDeg{-1.0};
        double travelDeg{0.0};
        bool routeKnown{true};
        bool positionFresh{false};
        double azimuthDeg{-1.0};
        double elevationDeg{-1.0};
        bool operator==(const State& other) const = default;
    };

    /// Why a window cannot write this object: the Core refuses every write.
    static QString readOnlyReason();
    /// How often the Core looks again at its serial ports and for rotctld
    /// while bound (both are slow to ask, and the position moves several
    /// times a second). This design's choice, not a device fact;
    /// configureRotor always checks the ports as they are.
    static constexpr int kHostRefreshMs = 5000;

    explicit RotorModel(QObject* parent = nullptr);

    TunerModel::ConnectionPhase connectionPhase() const { return m_state.connectionPhase; }
    QString connectionError() const { return m_state.connectionError; }
    Driver driver() const { return m_state.driver; }
    QString label() const { return m_state.label; }
    QString serialPort() const { return m_state.serialPort; }
    int baud() const { return m_state.baud; }
    QString host() const { return m_state.host; }
    int port() const { return m_state.port; }
    QString serialPorts() const { return m_state.serialPorts; }
    Axes axes() const { return m_state.axes; }
    int rangeDeg() const { return m_state.rangeDeg; }
    EndStop endStop() const { return m_state.endStop; }
    double spanPositionDeg() const { return m_state.spanPositionDeg; }
    double travelDeg() const { return m_state.travelDeg; }
    bool routeKnown() const { return m_state.routeKnown; }
    double offsetDeg() const { return m_state.offsetDeg; }
    int hamlibModel() const { return m_state.hamlibModel; }
    bool rotctldAvailable() const { return m_state.rotctldAvailable; }
    bool positionFresh() const { return m_state.positionFresh; }
    double azimuthDeg() const { return m_state.azimuthDeg; }
    double elevationDeg() const { return m_state.elevationDeg; }
    double targetAzimuthDeg() const { return m_state.targetAzimuthDeg; }
    double targetElevationDeg() const { return m_state.targetElevationDeg; }
    Motion motion() const { return m_state.motion; }
    QString presets() const { return m_state.presets; }
    QString fault() const { return m_state.fault; }
    State state() const { return m_state; }

    /// The Core: follow its rotor controller from now on (nullptr stops).
    void bindController(StationRotorController* controller);
    /// The object as `controller` reports it, with the serial ports and
    /// rotctld as last looked up.
    static State stateFrom(const StationRotorController& controller,
                           const QString& serialPorts, bool rotctldAvailable);

    /// The whole state at once (the Core's controller, or a test).
    void setState(const State& state);

    /// A remote window: one of the Core's values arriving.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);

signals:
    void stateChanged();
    void positionChanged();

private:
    void refreshFromController();
    void refreshHost();

    State m_state;
    StationRotorController* m_controller{nullptr};
    QMetaObject::Connection m_stateConnection;
    QMetaObject::Connection m_positionConnection;
    QMetaObject::Connection m_destroyedConnection;
    QTimer m_hostRefresh;
    QString m_hostSerialPorts;
    bool m_hostRotctld{false};
};

} // namespace RotorLink
} // namespace NereusSDR
