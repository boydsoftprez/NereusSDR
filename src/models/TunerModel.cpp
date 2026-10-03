// =================================================================
// src/models/TunerModel.cpp  (NereusSDR)
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
//                 See TunerModel.h for full divergence summary.
//   2026-09-24 - iPhone app Task 4b (R-IOS-01, R-R3-21): the reasons this
//                file sends an app are in operator words. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
#include "models/TunerModel.h"
#include "core/TgxlConnection.h"

#include <QLoggingCategory>
#include <QDebug>
#include <cmath>

Q_LOGGING_CATEGORY(lcTunerModel, "nereus.tuner.model")

namespace NereusSDR {

TunerModel::TunerModel(QObject* parent)
    : QObject(parent)
{
}

void TunerModel::setStationConnectionState(const StationConnectionState& state)
{
    const bool connected = state.phase == ConnectionPhase::Connected;
    const bool presenceChanged_ = m_present != connected;
    const bool directChanged = hasDirectConnection() != connected;
    const bool ipChanged = m_ip != (connected ? state.peerAddress : QString{});
    const bool tuningChanged_ = !connected && m_tuning;
    const bool antennaChanged = !connected && m_antA != 0;
    const bool relaysChanged = !connected && (m_relayC1 != 0 || m_relayL != 0 || m_relayC2 != 0);
    const bool metersChanged_ = !connected && (m_fwd != 0.0f || m_swr != 1.0f);

    m_configuredHost = state.configuredHost;
    m_configuredPort = state.configuredPort;
    m_connectionPhase = state.phase;
    m_connectionError = state.error;
    m_deviceModel = state.deviceModel;
    m_deviceSerial = state.deviceSerial;
    m_deviceVersion = state.deviceVersion;
    m_deviceNickname = state.deviceNickname;
    m_stationDirectConnectionValid = true;
    m_stationDirectConnection = connected;
    m_present = connected;
    m_ip = connected ? state.peerAddress : QString{};

    if (!connected) {
        // A non-admitted station lifecycle state must not leave live RF
        // telemetry looking current. These are direct assignments only;
        // remote receipt never translates into TGXL commands.
        m_relayC1 = m_relayL = m_relayC2 = 0;
        m_operate = m_bypass = m_tuning = false;
        m_antA = 0;
        m_oneByThree = false;
        m_fwd = 0.0f;
        m_swr = 1.0f;
    }

    if (presenceChanged_) { emit presenceChanged(connected); }
    if (directChanged) { emit directConnectionChanged(); }
    if (tuningChanged_) { emit tuningChanged(false); }
    if (antennaChanged) { emit antennaAChanged(0); }
    if (relaysChanged) { emit relayChanged(); }
    if (metersChanged_) { emit metersChanged(m_fwd, m_swr); }
    if (!connected || antennaChanged || ipChanged) { emit stateChanged(); }
    emit stationConnectionChanged();
}

// ── Status parsing ──────────────────────────────────────────────────────────

// From AetherSDR src/models/TunerModel.cpp:applyStatus [@0cd4559]
// NereusSDR additions: presence detection via model/serial_num keys;
// fwd/swr parsing added directly here (upstream parses these only inside
// the stateUpdated/statusUpdated lambdas of setDirectConnection).
void TunerModel::applyStatus(const QMap<QString, QString>& kvs)
{
    bool changed = false;
    bool relayChanged_ = false;
    bool metersChanged_ = false;

    for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
        const QString& key = it.key();
        const QString& val = it.value();

        if (key == "serial_num") {
            if (m_serial != val) { m_serial = val; changed = true; }
        } else if (key == "model") {
            if (m_model != val) { m_model = val; changed = true; }
        } else if (key == "operate") {
            bool on = (val == "1");
            if (m_operate != on) { m_operate = on; changed = true; }
        } else if (key == "bypass") {
            bool on = (val == "1");
            if (m_bypass != on) { m_bypass = on; changed = true; }
        } else if (key == "tuning") {
            bool on = (val == "1");
            if (m_tuning != on) {
                m_tuning = on;
                changed = true;
                emit tuningChanged(m_tuning);
            }
        } else if (key == "relayC1") {
            int v = val.toInt();
            if (m_relayC1 != v) { m_relayC1 = v; relayChanged_ = true; }
        } else if (key == "relayC2") {
            int v = val.toInt();
            if (m_relayC2 != v) { m_relayC2 = v; relayChanged_ = true; }
        } else if (key == "relayL") {
            int v = val.toInt();
            if (m_relayL != v) { m_relayL = v; relayChanged_ = true; }
        } else if (key == "antA") {
            int v = val.toInt();
            if (m_antA != v) {
                m_antA = v;
                changed = true;
                emit antennaAChanged(v);
            }
        } else if (key == "one_by_three" || key == "3way") {
            // Bench-discovered 2026-05-19: TGXL firmware v1.2.17 emits `3way=1` in
            // the info response (instead of `one_by_three=1` per the design spec).
            // Accept both keys so the 3x1 antenna switch is recognized regardless
            // of firmware variant.
            bool v = (val == "1");
            if (m_oneByThree != v) {
                m_oneByThree = v;
                changed = true;
                qCInfo(lcTunerModel)
                    << "TunerModel: 1x3 antenna switch flag set to" << v
                    << "via key" << key;
            }
        } else if (key == "ip") {
            if (m_ip != val) { m_ip = val; changed = true; }
        } else if (key == "fwd") {
            // From AetherSDR src/models/TunerModel.cpp:stateUpdated lambda [@0cd4559]
            // Upstream converts dBm -> watts via pow(10,dBm/10)/1000 inside the
            // direct-connection lambda.  applyStatus in NereusSDR stores the raw
            // float so that tests can drive it directly without a live connection.
            float v = val.toFloat();
            if (m_fwd != v) { m_fwd = v; metersChanged_ = true; }
        } else if (key == "swr") {
            // From AetherSDR src/models/TunerModel.cpp:stateUpdated lambda [@0cd4559]
            // Same reasoning as fwd above: store raw float in applyStatus path.
            float v = val.toFloat();
            if (m_swr != v) { m_swr = v; metersChanged_ = true; }
        }
        // nickname, version, ant, dhcp, netmask, gateway, ptta, pttb
        // are informational — ignore for now.
        // From AetherSDR src/models/TunerModel.cpp:29 [@0cd4559]
    }

    // Presence detection: mark present when identity keys arrive.
    // NereusSDR uses a m_present bool because there is no SmartSDR handle
    // mechanism; upstream's isPresent() returns !m_handle.isEmpty().
    if (!m_present && (!m_model.isEmpty() || !m_serial.isEmpty())) {
        m_present = true;
        emit presenceChanged(true);
    }

    if (relayChanged_) {
        emit relayChanged();
    }
    if (metersChanged_) {
        emit metersChanged(m_fwd, m_swr);
    }
    if (changed || relayChanged_) {
        emit stateChanged();
    }
}

// ── Connection binding ────────────────────────────────────────────────────

// From AetherSDR src/models/TunerModel.cpp:setDirectConnection [@0cd4559]
// Renamed bindConnection (NereusSDR divergence; no SmartSDR handle routing).
// directConnectionChanged() emits with no bool argument in NereusSDR.
void TunerModel::bindConnection(TgxlConnection* conn)
{
    if (m_conn == conn) { return; }
    if (m_conn) {
        disconnect(m_conn, nullptr, this, nullptr);
    }
    m_conn = conn;
    if (m_conn) {
        connect(m_conn, &TgxlConnection::connected, this, [this]() {
            qCDebug(lcTunerModel) << "TunerModel: direct TGXL connection established";
            emit directConnectionChanged();
        });
        connect(m_conn, &TgxlConnection::disconnected, this, [this]() {
            qCDebug(lcTunerModel) << "TunerModel: direct TGXL connection lost";
            emit directConnectionChanged();
        });
        // Update relay values from direct state pushes.
        // From AetherSDR src/models/TunerModel.cpp:stateUpdated lambda [@0cd4559]
        // NereusSDR: fwd/swr conversion (dBm->watts, return-loss->SWR ratio)
        // lives here in the direct-connection path, matching upstream.
        // Bench-fix 2026-05-19: route ALL non-meter keys through applyStatus
        // so identity / 1x3-switch / antA / relay parsing matches the public
        // test harness path. Without this, info R-frames containing 3way=1
        // would route through statusUpdated -> a lambda that only knew about
        // antA/fwd/swr, leaving m_oneByThree=false and hiding the antenna
        // buttons even on a confirmed 1x3 TGXL.
        connect(m_conn, &TgxlConnection::stateUpdated, this,
                [this](const QMap<QString, QString>& kvs) {
            // Strip meter fields before calling applyStatus so the raw dBm /
            // return-loss values don't leak into m_fwd / m_swr and emit a
            // metersChanged with wrong units before this lambda's conversion.
            QMap<QString, QString> nonMeterKvs = kvs;
            nonMeterKvs.remove(QStringLiteral("fwd"));
            nonMeterKvs.remove(QStringLiteral("swr"));
            if (!nonMeterKvs.isEmpty()) {
                applyStatus(nonMeterKvs);
            }

            // Forward power and SWR from direct TGXL connection (#625).
            // TGXL reports fwd in dBm and swr as return loss (negative dB).
            // Convert to watts and SWR ratio for the gauge.
            // Always emit when meter fields are present — suppressing identical
            // values caused meter-freeze when SWR settled to exactly 1.0 (#1530).
            // From AetherSDR src/models/TunerModel.cpp:stateUpdated lambda [@0cd4559]
            bool meters = false;
            if (kvs.contains("fwd")) {
                float dBm = kvs.value("fwd").toFloat();
                float watts = std::pow(10.0f, dBm / 10.0f) / 1000.0f;
                m_fwd = watts;
                meters = true;
            }
            if (kvs.contains("swr")) {
                float rl = kvs.value("swr").toFloat();  // return loss in dB (negative from TGXL)
                float rho = std::pow(10.0f, rl / 20.0f);  // rl is already negative
                float ratio = (rho < 0.999f) ? (1.0f + rho) / (1.0f - rho) : 99.9f;
                m_swr = ratio;
                meters = true;
            }
            if (meters) { emit metersChanged(m_fwd, m_swr); }
        });
        // Also parse antA + meters from 1/sec status poll responses, and
        // identity / 1x3-switch keys from `info` R-frame replies (which
        // TgxlConnection routes through statusUpdated with the full body).
        // Bench-fix 2026-05-19: route through applyStatus so the
        // 3way / one_by_three key in the info reply actually flips
        // m_oneByThree -- without this the antenna buttons stayed hidden on
        // a confirmed 1x3 TGXL because the old statusUpdated lambda only
        // knew about antA / fwd / swr.
        // From AetherSDR src/models/TunerModel.cpp:statusUpdated lambda [@0cd4559]
        connect(m_conn, &TgxlConnection::statusUpdated, this,
                [this](const QMap<QString, QString>& kvs) {
            QMap<QString, QString> nonMeterKvs = kvs;
            nonMeterKvs.remove(QStringLiteral("fwd"));
            nonMeterKvs.remove(QStringLiteral("swr"));
            if (!nonMeterKvs.isEmpty()) {
                applyStatus(nonMeterKvs);
            }
            // Forward power and SWR from direct TGXL status poll (#625).
            // Always emit — see #1530 for why equality suppression was removed.
            // From AetherSDR src/models/TunerModel.cpp:statusUpdated lambda [@0cd4559]
            bool meters = false;
            if (kvs.contains("fwd")) {
                float dBm = kvs.value("fwd").toFloat();
                float watts = std::pow(10.0f, dBm / 10.0f) / 1000.0f;
                m_fwd = watts;
                meters = true;
            }
            if (kvs.contains("swr")) {
                float rl = kvs.value("swr").toFloat();  // return loss in dB (negative from TGXL)
                float rho = std::pow(10.0f, rl / 20.0f);  // rl is already negative
                float ratio = (rho < 0.999f) ? (1.0f + rho) / (1.0f - rho) : 99.9f;
                m_swr = ratio;
                meters = true;
            }
            if (meters) { emit metersChanged(m_fwd, m_swr); }
        });
    }
}

bool TunerModel::hasDirectConnection() const
{
    // From AetherSDR src/models/TunerModel.cpp:hasDirectConnection [@0cd4559]
    if (m_stationDirectConnectionValid) {
        return m_stationDirectConnection;
    }
    return m_conn && m_conn->isConnected();
}

// ── Remote Daemon R2 Task 8: inbound mirror hook ────────────────────────────
//
// NereusSDR-original; no Thetis/AetherSDR equivalent (the ATU's remote-write
// path is a Remote Daemon R2 concept). Three properties translate onto the
// exact command slots below that a local TunerApplet already calls; the rest
// are hardware telemetry TunerModel only ever learns from applyStatus(), and
// are refused rather than assigned directly -- writing m_operate / m_bypass
// / m_antA here instead of going through the command slots would silently
// desync the model from what the tuner itself is doing, which is exactly
// the class of bug applyStatus()-is-the-only-writer exists to prevent.
QString TunerModel::applyMirroredValue(const QByteArray& propertyName, const QVariant& value)
{
    if (propertyName == "isOperate") {
        setOperate(value.toBool());
        return QString();
    }
    if (propertyName == "isBypass") {
        setBypass(value.toBool());
        return QString();
    }
    if (propertyName == "antennaA") {
        const int ant = value.toInt();
        // Same bound setAntennaA() itself enforces below. Checked here too
        // so an out-of-range remote write is refused honestly rather than
        // silently swallowed by setAntennaA()'s own guard and reported as
        // applied.
        if (ant < 1 || ant > 3) {
            return QStringLiteral("The tuner's antenna must be 1, 2 or 3.");
        }
        setAntennaA(ant);
        return QString();
    }

    return QStringLiteral("The tuner reports this itself; it cannot be changed from here.");
}

bool TunerModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    if (propertyName == QByteArrayLiteral("configuredHost")) {
        if (m_configuredHost != value.toString()) { m_configuredHost = value.toString(); emit stationConnectionChanged(); }
        return true;
    }
    if (propertyName == QByteArrayLiteral("configuredPort")) {
        if (m_configuredPort != value.toInt()) { m_configuredPort = value.toInt(); emit stationConnectionChanged(); }
        return true;
    }
    if (propertyName == QByteArrayLiteral("connectionPhase")) {
        const auto phase = static_cast<ConnectionPhase>(value.toInt());
        if (m_connectionPhase != phase) { m_connectionPhase = phase; emit stationConnectionChanged(); }
        return true;
    }
    if (propertyName == QByteArrayLiteral("connectionError")) {
        if (m_connectionError != value.toString()) { m_connectionError = value.toString(); emit stationConnectionChanged(); }
        return true;
    }
    if (propertyName == QByteArrayLiteral("deviceModel")) {
        if (m_deviceModel != value.toString()) { m_deviceModel = value.toString(); emit stationConnectionChanged(); }
        return true;
    }
    if (propertyName == QByteArrayLiteral("deviceSerial")) {
        if (m_deviceSerial != value.toString()) { m_deviceSerial = value.toString(); emit stationConnectionChanged(); }
        return true;
    }
    if (propertyName == QByteArrayLiteral("deviceVersion")) {
        if (m_deviceVersion != value.toString()) { m_deviceVersion = value.toString(); emit stationConnectionChanged(); }
        return true;
    }
    if (propertyName == QByteArrayLiteral("deviceNickname")) {
        if (m_deviceNickname != value.toString()) { m_deviceNickname = value.toString(); emit stationConnectionChanged(); }
        return true;
    }
    // This is intentionally a direct state adapter. Calling the public
    // command slots here would invert a Core telemetry update into a second
    // hardware command from the GUI process.
    if (propertyName == QByteArrayLiteral("relayC1")) {
        const int next = value.toInt();
        if (m_relayC1 != next) {
            m_relayC1 = next;
            emit relayChanged();
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("relayL")) {
        const int next = value.toInt();
        if (m_relayL != next) {
            m_relayL = next;
            emit relayChanged();
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("relayC2")) {
        const int next = value.toInt();
        if (m_relayC2 != next) {
            m_relayC2 = next;
            emit relayChanged();
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("isOperate")) {
        const bool next = value.toBool();
        if (m_operate != next) {
            m_operate = next;
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("isBypass")) {
        const bool next = value.toBool();
        if (m_bypass != next) {
            m_bypass = next;
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("isTuning")) {
        const bool next = value.toBool();
        if (m_tuning != next) {
            m_tuning = next;
            emit tuningChanged(next);
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("antennaA")) {
        const int next = value.toInt();
        if (m_antA != next) {
            m_antA = next;
            emit antennaAChanged(next);
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("hasAntennaSwitch")) {
        const bool next = value.toBool();
        if (m_oneByThree != next) {
            m_oneByThree = next;
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("isPresent")) {
        const bool next = value.toBool();
        if (m_present != next) {
            m_present = next;
            emit presenceChanged(next);
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("hasDirectConnection")) {
        const bool previous = hasDirectConnection();
        m_stationDirectConnectionValid = true;
        m_stationDirectConnection = value.toBool();
        if (previous != m_stationDirectConnection) {
            emit directConnectionChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("tgxlIp")) {
        const QString next = value.toString();
        if (m_ip != next) {
            m_ip = next;
            emit stateChanged();
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("fwdPower")) {
        const float next = value.toFloat();
        if (m_fwd != next) {
            m_fwd = next;
            emit metersChanged(m_fwd, m_swr);
        }
        return true;
    }
    if (propertyName == QByteArrayLiteral("swr")) {
        const float next = value.toFloat();
        if (m_swr != next) {
            m_swr = next;
            emit metersChanged(m_fwd, m_swr);
        }
        return true;
    }
    return false;
}

// ── Commands ─────────────────────────────────────────────────────────────────

// From AetherSDR src/models/TunerModel.cpp:setOperate [@0cd4559]
// NereusSDR: no handle guard (handle is a SmartSDR concept); guard on m_conn.
void TunerModel::setOperate(bool on)
{
    if (!m_conn || !m_conn->isConnected()) {
        qCDebug(lcTunerModel) << "TunerModel::setOperate: no connection, ignoring";
        return;
    }
    qCDebug(lcTunerModel) << "TunerModel: setOperate" << on;
    m_conn->sendCommand(QString("operate=%1").arg(on ? "1" : "0"));
}

// From AetherSDR src/models/TunerModel.cpp:setBypass [@0cd4559]
void TunerModel::setBypass(bool on)
{
    if (!m_conn || !m_conn->isConnected()) {
        qCDebug(lcTunerModel) << "TunerModel::setBypass: no connection, ignoring";
        return;
    }
    qCDebug(lcTunerModel) << "TunerModel: setBypass" << on;
    m_conn->sendCommand(QString("bypass=%1").arg(on ? "1" : "0"));
}

// From AetherSDR src/models/TunerModel.cpp:autoTune [@0cd4559]
// Bench-fix 2026-05-19: pcap analysis (flex-pgxl-tgxl-capture stream 1,
// .19 TunerGeniusDesktop -> .234 TGXL :9010) shows the actually-used
// wire command for an operator-initiated tune cycle is `autotune`, not
// `tune start`. The latter was AetherSDR's port-time guess and TGXL
// silently ACK'd it with hex=0 but never engaged the sweep -- bench
// observation 22:29 on 2026-05-19. Switching to `autotune` matches the
// frame format the real PGXL/TGXL ecosystem uses.
void TunerModel::autoTune()
{
    if (!m_conn || !m_conn->isConnected()) {
        qCDebug(lcTunerModel) << "TunerModel::autoTune: no connection, ignoring";
        return;
    }
    qCInfo(lcTunerModel) << "TunerModel: autotune";
    m_conn->sendCommand(QStringLiteral("autotune"));
}

// From AetherSDR src/models/TunerModel.cpp:setAntennaA [@0cd4559]
void TunerModel::setAntennaA(int ant)
{
    if (!m_conn || !m_conn->isConnected()) {
        qCDebug(lcTunerModel) << "TunerModel::setAntennaA: no direct connection";
        return;
    }
    if (ant < 1 || ant > 3) { return; }
    qCDebug(lcTunerModel) << "TunerModel: activate ant=" << ant;
    m_conn->sendCommand(QString("activate ant=%1").arg(ant));
}

// From AetherSDR src/models/TunerModel.cpp:adjustRelay [@0cd4559]
void TunerModel::adjustRelay(int relay, int dir)
{
    if (!m_conn || !m_conn->isConnected()) {
        qCDebug(lcTunerModel) << "TunerModel::adjustRelay: no direct connection";
        return;
    }
    m_conn->adjustRelay(relay, dir);
}

}  // namespace NereusSDR
