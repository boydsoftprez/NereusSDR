#pragma once
// no-port-check: NereusSDR-original. The Core's RF-Kit RF2K-S status as the
// mirrored, read-only `rfkit` object.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/RfKitModel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-R3-47 / R-R3-22.
//
// What the Core knows about its RF-Kit RF2K-S: the Tuner Genius's
// connection-state shape plus operate and the amp's meters as its REST
// interface reports them (W, SWR ratio, degrees C, V, A). The Core mirrors
// it as the read-only `rfkit` object (remoteRfKitControlVersion 1); a local
// window reads the same object in-process.
//
// Bound (the Core, a local window) it follows an Rf2ksConnection. Unbound
// (a remote window) it only holds the Core's values as they arrive through
// applyStationValue(); it never opens a connection or sends a request.
//
// The wire contract: docs/architecture/2026-09-23-remote-accessory-control-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-48: the antenna and
//                                    tuner rows, the operating interface,
//                                    band follow, and the Core's
//                                    controller owning the connection
//                                    state (remoteRfKitControlVersion 2).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/Rf2ksConnection.h"
#include "models/TunerModel.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

namespace NereusSDR {


class NEREUS_CORE_EXPORT RfKitModel : public QObject {
    Q_OBJECT
    // The Tuner Genius's connection-state shape (TunerModel), same enum.
    Q_PROPERTY(NereusSDR::TunerModel::ConnectionPhase connectionPhase READ connectionPhase NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString configuredHost READ configuredHost NOTIFY stationConnectionChanged)
    Q_PROPERTY(int configuredPort READ configuredPort NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString connectionError READ connectionError NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceModel READ deviceModel NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceSerial READ deviceSerial NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceVersion READ deviceVersion NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceNickname READ deviceNickname NOTIFY stationConnectionChanged)
    // Operate and the meters.
    Q_PROPERTY(bool present READ present NOTIFY statusChanged)
    Q_PROPERTY(bool operate READ operate NOTIFY statusChanged)
    Q_PROPERTY(double forwardPowerW READ forwardPowerW NOTIFY statusChanged)
    Q_PROPERTY(double reflectedPowerW READ reflectedPowerW NOTIFY statusChanged)
    Q_PROPERTY(double swr READ swr NOTIFY statusChanged)
    Q_PROPERTY(double temperatureC READ temperatureC NOTIFY statusChanged)
    Q_PROPERTY(double voltageV READ voltageV NOTIFY statusChanged)
    Q_PROPERTY(double currentA READ currentA NOTIFY statusChanged)
    // R-R3-47: the amp's operating interface as it reports it ("TCI",
    // "UDP", "CAT", "UNIV"; empty before the first reading).
    Q_PROPERTY(QString operationalInterface READ operationalInterface NOTIFY statusChanged)
    // R-R3-47: the antenna row. Bit N-1 of a mask is internal antenna N
    // (1 to 4): present is every internal antenna the amp lists, disabled
    // the ones it lists as disabled. The active antenna is a number (0:
    // none reported) and whether it is an external one.
    Q_PROPERTY(int antennaPresentMask READ antennaPresentMask NOTIFY statusChanged)
    Q_PROPERTY(int antennaDisabledMask READ antennaDisabledMask NOTIFY statusChanged)
    Q_PROPERTY(int activeAntennaNumber READ activeAntennaNumber NOTIFY statusChanged)
    Q_PROPERTY(bool activeAntennaExternal READ activeAntennaExternal NOTIFY statusChanged)
    // R-R3-47: the tuner row, as /tuner reports it.
    Q_PROPERTY(NereusSDR::RfKitModel::TunerMode tunerMode READ tunerMode NOTIFY statusChanged)
    Q_PROPERTY(QString tunerSetup READ tunerSetup NOTIFY statusChanged)
    Q_PROPERTY(int tunerInductanceNh READ tunerInductanceNh NOTIFY statusChanged)
    Q_PROPERTY(int tunerCapacitancePf READ tunerCapacitancePf NOTIFY statusChanged)
    Q_PROPERTY(int tunerFrequencyKhz READ tunerFrequencyKhz NOTIFY statusChanged)
    Q_PROPERTY(int tunerSegmentKhz READ tunerSegmentKhz NOTIFY statusChanged)
    // R-R3-48: whether the amp follows the radio's band through the TCI
    // server, and the address and port to enter on the amp when it does not.
    Q_PROPERTY(NereusSDR::TunerModel::BandFollow bandFollow READ bandFollow NOTIFY bandFollowChanged)
    Q_PROPERTY(QString bandFollowAddress READ bandFollowAddress NOTIFY bandFollowChanged)
    Q_PROPERTY(int bandFollowPort READ bandFollowPort NOTIFY bandFollowChanged)

public:
    using ConnectionPhase = TunerModel::ConnectionPhase;
    using StationConnectionState = TunerModel::StationConnectionState;
    using BandFollow = TunerModel::BandFollow;

    /// The tuner's mode. Wire values are fixed; new ones are only appended.
    enum class TunerMode {
        Unknown = 0,     ///< no reading yet, or a word this build does not know
        Bypass = 1,      ///< BYPASS
        Manual = 2,      ///< MANUAL
        AutoTuning = 3,  ///< AUTO_TUNING: a tune is running
        Auto = 4,        ///< AUTO
    };
    Q_ENUM(TunerMode)

    /// Why a window cannot change this object: the Core refuses every write.
    static QString readOnlyReason();

    explicit RfKitModel(QObject* parent = nullptr);

    ConnectionPhase connectionPhase() const { return m_connection.phase; }
    QString configuredHost() const { return m_connection.configuredHost; }
    int configuredPort() const { return m_connection.configuredPort; }
    QString connectionError() const { return m_connection.error; }
    QString deviceModel() const { return m_connection.deviceModel; }
    QString deviceSerial() const { return m_connection.deviceSerial; }
    QString deviceVersion() const { return m_connection.deviceVersion; }
    QString deviceNickname() const { return m_connection.deviceNickname; }

    bool present() const { return m_present; }
    bool operate() const { return m_operate; }
    double forwardPowerW() const { return m_forwardPowerW; }
    double reflectedPowerW() const { return m_reflectedPowerW; }
    double swr() const { return m_swr; }
    double temperatureC() const { return m_temperatureC; }
    double voltageV() const { return m_voltageV; }
    double currentA() const { return m_currentA; }
    QString operationalInterface() const { return m_operationalInterface; }
    int antennaPresentMask() const { return m_antennaPresentMask; }
    int antennaDisabledMask() const { return m_antennaDisabledMask; }
    int activeAntennaNumber() const { return m_activeAntennaNumber; }
    bool activeAntennaExternal() const { return m_activeAntennaExternal; }
    TunerMode tunerMode() const { return m_tunerMode; }
    QString tunerSetup() const { return m_tunerSetup; }
    int tunerInductanceNh() const { return m_tunerInductanceNh; }
    int tunerCapacitancePf() const { return m_tunerCapacitancePf; }
    int tunerFrequencyKhz() const { return m_tunerFrequencyKhz; }
    int tunerSegmentKhz() const { return m_tunerSegmentKhz; }
    BandFollow bandFollow() const { return m_bandFollow; }
    QString bandFollowAddress() const { return m_bandFollowAddress; }
    int bandFollowPort() const { return m_bandFollowPort; }

    /// The whole connection state (a Core-side controller builds on it).
    StationConnectionState stationConnectionState() const { return m_connection; }

    /// R-R3-48: the band-follow line the amp page and applet show.
    QString bandFollowText() const;

    /// R-R3-47: the Core's StationRfKitController reports the connection
    /// state through setStationConnectionState(); the bound connection
    /// then feeds only the amp's readings and identity.
    void setConnectionStateOwnedByController(bool owned) { m_controllerOwnsConnection = owned; }

    /// R-R3-48: band follow as the Core (or a local window) sees it.
    void setBandFollow(BandFollow state, const QString& address, int port);

    /// Follow `connection`: its power, operate and info reports, and its
    /// connect, drop and failure as the connection phase. The Core and a
    /// local window only; a remote window never binds.
    void bindConnection(Rf2ksConnection* connection);

    /// Whether the station has the RF-Kit switched on (rfKitEnabled). Off
    /// reads as phase Disabled whenever the amp is not connected.
    void setAccessoryEnabled(bool enabled);

    /// A whole connection state at once (a Core-side controller, or a test).
    void setStationConnectionState(const StationConnectionState& state);

    /// One /power report.
    void applyPower(const RfKitPowerSnapshot& snapshot);
    /// One /operate-mode report ("OPERATE" or "STANDBY").
    void applyOperateMode(const QString& mode);
    /// One /info report.
    void applyInfo(const QString& device, const QString& softwareVersion,
                   const QString& customName);
    /// R-R3-47: one /operational-interface report.
    void applyOperationalInterface(const QString& iface);
    /// R-R3-47: one /antennas report.
    void applyAntennas(const QList<RfKitAntenna>& antennas);
    /// R-R3-47: one /antennas/active report.
    void applyActiveAntenna(const RfKitAntenna& antenna);
    /// R-R3-47: one /tuner report.
    void applyTuner(const RfKitTunerSnapshot& tuner);

    /// The antenna list and active antenna as the applet draws them.
    QList<RfKitAntenna> antennas() const;
    RfKitAntenna activeAntenna() const;
    /// The tuner row as the applet draws it.
    RfKitTunerSnapshot tuner() const;

    /// A remote window: one of the Core's values arriving.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);

signals:
    void stationConnectionChanged();
    void statusChanged();
    void bandFollowChanged();

private:
    void publishConnection(const StationConnectionState& next);
    void refreshFromConnection(ConnectionPhase phase, const QString& error);

    QPointer<Rf2ksConnection> m_conn;
    StationConnectionState m_connection;
    bool m_enabled{true};
    bool m_controllerOwnsConnection{false};
    QString m_operationalInterface;
    int m_antennaPresentMask{0};
    int m_antennaDisabledMask{0};
    int m_activeAntennaNumber{0};
    bool m_activeAntennaExternal{false};
    TunerMode m_tunerMode{TunerMode::Unknown};
    QString m_tunerSetup;
    int m_tunerInductanceNh{0};
    int m_tunerCapacitancePf{0};
    int m_tunerFrequencyKhz{0};
    int m_tunerSegmentKhz{0};
    BandFollow m_bandFollow{BandFollow::Off};
    QString m_bandFollowAddress;
    int m_bandFollowPort{0};
    bool m_present{false};
    bool m_operate{false};
    double m_forwardPowerW{0.0};
    double m_reflectedPowerW{0.0};
    double m_swr{1.0};
    double m_temperatureC{0.0};
    double m_voltageV{0.0};
    double m_currentA{0.0};
};

} // namespace NereusSDR
