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
            if (m_oneByThree != v) { m_oneByThree = v; changed = true; }
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
        connect(m_conn, &TgxlConnection::stateUpdated, this,
                [this](const QMap<QString, QString>& kvs) {
            bool changed = false;
            bool relay = false;
            if (kvs.contains("relayC1")) {
                int v = kvs.value("relayC1").toInt();
                if (m_relayC1 != v) { m_relayC1 = v; relay = true; }
            }
            if (kvs.contains("relayL")) {
                int v = kvs.value("relayL").toInt();
                if (m_relayL != v) { m_relayL = v; relay = true; }
            }
            if (kvs.contains("relayC2")) {
                int v = kvs.value("relayC2").toInt();
                if (m_relayC2 != v) { m_relayC2 = v; relay = true; }
            }
            if (kvs.contains("antA")) {
                int v = kvs.value("antA").toInt();
                if (m_antA != v) {
                    m_antA = v;
                    changed = true;
                    emit antennaAChanged(v);
                }
            }
            if (relay) { emit relayChanged(); changed = true; }
            if (changed) { emit stateChanged(); }
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
        // Also parse antA + meters from 1/sec status poll responses.
        // From AetherSDR src/models/TunerModel.cpp:statusUpdated lambda [@0cd4559]
        connect(m_conn, &TgxlConnection::statusUpdated, this,
                [this](const QMap<QString, QString>& kvs) {
            if (kvs.contains("antA")) {
                int v = kvs.value("antA").toInt();
                if (m_antA != v) {
                    m_antA = v;
                    emit antennaAChanged(v);
                    emit stateChanged();
                }
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
    return m_conn && m_conn->isConnected();
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
void TunerModel::autoTune()
{
    if (!m_conn || !m_conn->isConnected()) {
        qCDebug(lcTunerModel) << "TunerModel::autoTune: no connection, ignoring";
        return;
    }
    qCDebug(lcTunerModel) << "TunerModel: tune start";
    m_conn->sendCommand(QStringLiteral("tune start"));
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
