// =================================================================
// src/models/TunerModel.h  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/models/TunerModel.{h,cpp} [@0cd4559].
//                 NereusSDR divergences from upstream:
//                   - bindConnection(TgxlConnection*) replaces setDirectConnection
//                     (NereusSDR does not use SmartSDR handle routing)
//                   - isPresent() driven by m_present bool (set when model/serial_num
//                     keys appear in applyStatus) vs upstream handle-based detection
//                   - commandReady(QString) signal dropped; commands forward directly
//                     via m_conn->sendCommand() / m_conn->adjustRelay()
//                   - directConnectionChanged() has no bool argument (NereusSDR)
//                   - relayChanged() signal added (plan addition over upstream)
//                   - fwd/swr parsed in applyStatus as raw floats (upstream parses
//                     them only via stateUpdated/statusUpdated direct-conn lambdas)
//   2026-09-24 - R-R3-47 / R-R3-48: BandFollow, the band-follow
//                state the `amplifier` and `rfkit` objects share. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once
#include "core/NereusCoreExport.h"
#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVariant>

namespace NereusSDR {

class TgxlConnection;

// State model for a 4O3A Tuner Genius XL (TGXL).
//
// Two data paths feed this model:
//   1. applyStatus(kvs): key=value pairs from the TGXL direct TCP connection
//      parsed by TgxlConnection (stateUpdated / statusUpdated signals).
//   2. bindConnection(conn): wires TgxlConnection::stateUpdated / statusUpdated
//      signals to applyStatus and tracks direct-connection state.
//
// Commands (autoTune, adjustRelay, setAntennaA, setOperate, setBypass) forward
// directly to the bound TgxlConnection.
// From AetherSDR src/models/TunerModel.h [@0cd4559]
class NEREUS_CORE_EXPORT TunerModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(ConnectionPhase connectionPhase READ connectionPhase NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString configuredHost READ configuredHost NOTIFY stationConnectionChanged)
    Q_PROPERTY(int configuredPort READ configuredPort NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString connectionError READ connectionError NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceModel READ deviceModel NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceSerial READ deviceSerial NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceVersion READ deviceVersion NOTIFY stationConnectionChanged)
    Q_PROPERTY(QString deviceNickname READ deviceNickname NOTIFY stationConnectionChanged)
    Q_PROPERTY(int  relayC1 READ relayC1 NOTIFY relayChanged)
    Q_PROPERTY(int  relayL  READ relayL  NOTIFY relayChanged)
    Q_PROPERTY(int  relayC2 READ relayC2 NOTIFY relayChanged)
    Q_PROPERTY(bool isOperate READ isOperate NOTIFY stateChanged)
    Q_PROPERTY(bool isBypass  READ isBypass  NOTIFY stateChanged)
    Q_PROPERTY(bool isTuning  READ isTuning  NOTIFY tuningChanged)
    Q_PROPERTY(int  antennaA  READ antennaA  NOTIFY antennaAChanged)
    Q_PROPERTY(bool hasAntennaSwitch READ hasAntennaSwitch NOTIFY stateChanged)
    Q_PROPERTY(bool isPresent READ isPresent NOTIFY presenceChanged)
    Q_PROPERTY(bool hasDirectConnection READ hasDirectConnection NOTIFY directConnectionChanged)
    Q_PROPERTY(QString tgxlIp READ tgxlIp NOTIFY stateChanged)
    Q_PROPERTY(float fwdPower READ fwdPower NOTIFY metersChanged)
    Q_PROPERTY(float swr      READ swr      NOTIFY metersChanged)

public:
    enum class ConnectionPhase {
        Disabled, Disconnected, Discovering, Connecting, Identifying,
        Retrying, Connected, Error,
    };
    Q_ENUM(ConnectionPhase)

    /// R-R3-47: whether an amplifier follows the radio's band, shared by
    /// the `amplifier` (Power Genius XL) and `rfkit` (RF2K-S) objects. Wire
    /// values are fixed; new ones are only appended.
    enum class BandFollow {
        Off = 0,              ///< the amp is not connected, or nothing to follow
        Waiting = 1,          ///< connected, not following yet (PGXL: not paired;
                              ///< RF2K-S: not connected to the TCI server)
        Following = 2,        ///< the amp follows the radio's band
        ThisComputerOnly = 3, ///< RF2K-S: the TCI server accepts only apps on
                              ///< its own computer, so the amp cannot reach it
    };
    Q_ENUM(BandFollow)

    struct StationConnectionState {
        QString configuredHost;
        quint16 configuredPort{0};
        ConnectionPhase phase{ConnectionPhase::Disconnected};
        QString error;
        QString deviceModel;
        QString deviceSerial;
        QString deviceVersion;
        QString deviceNickname;
        QString peerAddress;
    };

    explicit TunerModel(QObject* parent = nullptr);

    int  relayC1() const { return m_relayC1; }
    int  relayL()  const { return m_relayL;  }
    int  relayC2() const { return m_relayC2; }
    bool isOperate() const { return m_operate; }
    bool isBypass()  const { return m_bypass; }
    bool isTuning()  const { return m_tuning; }
    int  antennaA()  const { return m_antA;   }
    bool hasAntennaSwitch() const { return m_oneByThree; }
    bool isPresent() const { return m_present; }
    bool hasDirectConnection() const;
    QString tgxlIp() const { return m_ip; }
    float fwdPower() const { return m_fwd; }
    float swr()      const { return m_swr; }
    ConnectionPhase connectionPhase() const { return m_connectionPhase; }
    QString configuredHost() const { return m_configuredHost; }
    int configuredPort() const { return m_configuredPort; }
    QString connectionError() const { return m_connectionError; }
    QString deviceModel() const { return m_deviceModel; }
    QString deviceSerial() const { return m_deviceSerial; }
    QString deviceVersion() const { return m_deviceVersion; }
    QString deviceNickname() const { return m_deviceNickname; }

    // Core-owned TGXL lifecycle snapshots are applied atomically. This path
    // is observational: it never creates a local socket or emits a command.
    void setStationConnectionState(const StationConnectionState& state);

    // Wire TgxlConnection signals to applyStatus and track connection state.
    void bindConnection(TgxlConnection* conn);

    // Apply key=value pairs from a TGXL status message.
    // From AetherSDR src/models/TunerModel.cpp:applyStatus [@0cd4559]
    void applyStatus(const QMap<QString, QString>& kvs);

    // Remote Daemon R2 Task 8: StateMirror::applyInbound()'s hook for the
    // 13 TunerModel properties above, none of which carries a Q_PROPERTY
    // WRITE (applyStatus() is the only writer; they reflect what the
    // hardware itself reports back). Called by name through
    // QMetaObject::invokeMethod, so `propertyName` is one of the 13.
    // `value` has already been decoded to the property's native type.
    //
    // isOperate/isBypass/antennaA translate the intent into the SAME
    // command slot the local TunerApplet already drives (setOperate,
    // setBypass, setAntennaA below) -- without this, the whole ATU is
    // unreachable from a remote GUI, not just displayed stale. Every other
    // property is hardware telemetry with no legitimate remote-write path
    // and is refused.
    //
    // Returns an empty string when applied; otherwise a reason, leaving
    // TunerModel's own state untouched (the command slots below no-op
    // safely with no bound connection; this hook never assigns m_operate /
    // m_bypass / m_antA directly -- applyStatus() is the only writer of
    // those, exactly as it is for a local operator's command).
    Q_INVOKABLE QString applyMirroredValue(const QByteArray& propertyName,
                                           const QVariant& value);

    // Client-side station telemetry adapter. Unlike applyMirroredValue(),
    // this never translates values into commands for TgxlConnection.
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);

public slots:
    void autoTune();
    void adjustRelay(int relay, int dir);
    void setAntennaA(int antA);
    void setOperate(bool on);
    void setBypass(bool on);

signals:
    void relayChanged();
    void stateChanged();
    void tuningChanged(bool tuning);
    void antennaAChanged(int antA);
    void presenceChanged(bool present);
    void directConnectionChanged();
    void metersChanged(float fwd, float swr);
    void stationConnectionChanged();

private:
    TgxlConnection* m_conn{nullptr};
    int  m_relayC1{0}, m_relayL{0}, m_relayC2{0};
    bool m_operate{false}, m_bypass{false}, m_tuning{false};
    int  m_antA{0};
    bool m_oneByThree{false};
    bool m_present{false};
    // A remote GUI has no TGXL socket of its own. Once Core projects this
    // property, it becomes the authoritative answer for the client model;
    // local-direct models continue to derive it from m_conn.
    bool m_stationDirectConnectionValid{false};
    bool m_stationDirectConnection{false};
    QString m_ip;
    QString m_serial;
    QString m_model;
    float m_fwd{0.0f}, m_swr{1.0f};
    ConnectionPhase m_connectionPhase{ConnectionPhase::Disconnected};
    QString m_configuredHost;
    int m_configuredPort{0};
    QString m_connectionError;
    QString m_deviceModel;
    QString m_deviceSerial;
    QString m_deviceVersion;
    QString m_deviceNickname;
};

}  // namespace NereusSDR
