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
// =================================================================
#pragma once
#include <QObject>
#include <QMap>
#include <QString>

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
class TunerModel : public QObject {
    Q_OBJECT
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

    // Wire TgxlConnection signals to applyStatus and track connection state.
    void bindConnection(TgxlConnection* conn);

    // Apply key=value pairs from a TGXL status message.
    // From AetherSDR src/models/TunerModel.cpp:applyStatus [@0cd4559]
    void applyStatus(const QMap<QString, QString>& kvs);

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

private:
    TgxlConnection* m_conn{nullptr};
    int  m_relayC1{0}, m_relayL{0}, m_relayC2{0};
    bool m_operate{false}, m_bypass{false}, m_tuning{false};
    int  m_antA{0};
    bool m_oneByThree{false};
    bool m_present{false};
    QString m_ip;
    QString m_serial;
    QString m_model;
    float m_fwd{0.0f}, m_swr{1.0f};
};

}  // namespace NereusSDR
