// no-port-check: NereusSDR-original. See StationTciModel.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/StationTciModel.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-48, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The rest of the server's settings
//                                    (JJ's ruling of 2026-09-28).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Parity Task 23: the four options and
//                                    the apps. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "models/StationTciModel.h"

#include <QJsonArray>
#include <QtGlobal>

namespace NereusSDR {

QJsonObject StationTciClient::toFields() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("address"), address},
        {QStringLiteral("subscriptions"), QJsonArray::fromStringList(subscriptions)},
        {QStringLiteral("transmitting"), transmitting},
        {QStringLiteral("lastCommand"), lastCommand},
    };
}

std::optional<StationTciClient> StationTciClient::fromFields(const QString& id,
                                                             const QJsonObject& fields)
{
    if (!fields.value(QStringLiteral("name")).isString()
        || !fields.value(QStringLiteral("address")).isString()
        || !fields.value(QStringLiteral("subscriptions")).isArray()
        || !fields.value(QStringLiteral("transmitting")).isBool()
        || !fields.value(QStringLiteral("lastCommand")).isString()) {
        return std::nullopt;
    }
    StationTciClient client;
    client.id = id;
    client.name = fields.value(QStringLiteral("name")).toString();
    client.address = fields.value(QStringLiteral("address")).toString();
    for (const QJsonValue& value : fields.value(QStringLiteral("subscriptions")).toArray()) {
        if (!value.isString()) {
            return std::nullopt;
        }
        client.subscriptions.append(value.toString());
    }
    client.transmitting = fields.value(QStringLiteral("transmitting")).toBool();
    client.lastCommand = fields.value(QStringLiteral("lastCommand")).toString();
    return client;
}

const QList<StationTciModel::Setting>& StationTciModel::settingsTable()
{
    // The keys, ranges and defaults are CatTciServerPage's and the
    // server's readers' (TciServer, TciUpdateGap, TciProtocol.h's list).
    using K = Setting::Kind;
    static const QList<Setting> table{
        {"rateLimitMs", "TciRateLimitMs", K::Int, 0, 1000, "100"},
        {"cwBecomesCwuAbove10mhz", "TciCwBecomesCwuAbove10mhz", K::Bool, 0, 1, "False"},
        {"iqSwap", "TciIqSwap", K::Bool, 0, 1, "True"},
        {"alwaysStreamIq", "TciAlwaysStreamIq", K::Bool, 0, 1, "False"},
        {"audioBlockSamples", "TciAudioStreamSamples", K::Int, 100, 2048, "2048"},
        {"txChannel", "TciTxChannel", K::TxChannel, 0, 2, "Both"},
        {"rxSensorIntervalMs", "TciRxSensorIntervalMs", K::Int, 30, 1000, "200"},
        {"txSensorIntervalMs", "TciTxSensorIntervalMs", K::Int, 30, 1000, "200"},
        {"forgetRx2VfoBOnDisconnect", "TciForgetRx2VfoBOnDisconnect", K::Bool, 0, 1,
         kTciForgetRx2VfobDefault ? "True" : "False"},
        {"useRx1VfoaForRx2Vfoa", "TciUseRx1VfoaForRx2Vfoa", K::Bool, 0, 1,
         kTciUseRx1VfoaForRx2VfoaDefault ? "True" : "False"},
        {"copyRx2VfobToVfoa", "TciCopyRx2VfobToVfoa", K::Bool, 0, 1,
         kTciCopyRx2VfobToVfoaDefault ? "True" : "False"},
    };
    return table;
}

const StationTciModel::Setting* StationTciModel::setting(const QByteArray& name)
{
    for (const Setting& entry : settingsTable()) {
        if (name == entry.name) {
            return &entry;
        }
    }
    return nullptr;
}

QString StationTciModel::txChannelText(int index)
{
    switch (index) {
    case 0: return QStringLiteral("Left");
    case 1: return QStringLiteral("Right");
    case 2: return QStringLiteral("Both");
    default: return QString();
    }
}

int StationTciModel::txChannelIndex(const QString& text)
{
    for (int index = 0; index <= 2; ++index) {
        if (text == txChannelText(index)) {
            return index;
        }
    }
    return -1;
}

QVariant StationTciModel::valueIn(const State& state, const Setting& setting)
{
    const QByteArray name(setting.name);
    if (name == "rateLimitMs") { return state.rateLimitMs; }
    if (name == "cwBecomesCwuAbove10mhz") { return state.cwBecomesCwuAbove10mhz; }
    if (name == "iqSwap") { return state.iqSwap; }
    if (name == "alwaysStreamIq") { return state.alwaysStreamIq; }
    if (name == "audioBlockSamples") { return state.audioBlockSamples; }
    if (name == "txChannel") { return state.txChannel; }
    if (name == "rxSensorIntervalMs") { return state.rxSensorIntervalMs; }
    if (name == "txSensorIntervalMs") { return state.txSensorIntervalMs; }
    if (name == "forgetRx2VfoBOnDisconnect") { return state.forgetRx2VfoBOnDisconnect; }
    if (name == "useRx1VfoaForRx2Vfoa") { return state.useRx1VfoaForRx2Vfoa; }
    if (name == "copyRx2VfobToVfoa") { return state.copyRx2VfobToVfoa; }
    return {};
}

void StationTciModel::setIn(State& state, const Setting& setting, const QVariant& value)
{
    const QByteArray name(setting.name);
    if (name == "rateLimitMs") { state.rateLimitMs = value.toInt(); }
    else if (name == "cwBecomesCwuAbove10mhz") { state.cwBecomesCwuAbove10mhz = value.toBool(); }
    else if (name == "iqSwap") { state.iqSwap = value.toBool(); }
    else if (name == "alwaysStreamIq") { state.alwaysStreamIq = value.toBool(); }
    else if (name == "audioBlockSamples") { state.audioBlockSamples = value.toInt(); }
    else if (name == "txChannel") { state.txChannel = value.toInt(); }
    else if (name == "rxSensorIntervalMs") { state.rxSensorIntervalMs = value.toInt(); }
    else if (name == "txSensorIntervalMs") { state.txSensorIntervalMs = value.toInt(); }
    else if (name == "forgetRx2VfoBOnDisconnect") { state.forgetRx2VfoBOnDisconnect = value.toBool(); }
    else if (name == "useRx1VfoaForRx2Vfoa") { state.useRx1VfoaForRx2Vfoa = value.toBool(); }
    else if (name == "copyRx2VfobToVfoa") { state.copyRx2VfobToVfoa = value.toBool(); }
}

QString StationTciModel::readOnlyReason()
{
    return QStringLiteral("The Core reports its TCI server here. Turn it on or off with "
                          "this app's TCI switch.");
}

StationTciModel::StationTciModel(QObject* parent)
    : QObject(parent)
{
}

void StationTciModel::setState(const State& state)
{
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

bool StationTciModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    State next = m_state;
    if (propertyName == "enabled") {
        next.enabled = value.toBool();
    } else if (propertyName == "port") {
        next.port = qBound(0, value.toInt(), 65535);
    } else if (propertyName == "listening") {
        next.listening = value.toBool();
    } else if (propertyName == "stationAddress") {
        next.stationAddress = value.toString();
    } else if (propertyName == "error") {
        next.error = value.toString();
    } else if (propertyName == "emulateExpertSdr3") {
        next.emulateExpertSdr3 = value.toBool();
    } else if (propertyName == "emulateSunSdr2Pro") {
        next.emulateSunSdr2Pro = value.toBool();
    } else if (propertyName == "cwluBecomesCw") {
        next.cwluBecomesCw = value.toBool();
    } else if (propertyName == "sendInitialState") {
        next.sendInitialState = value.toBool();
    } else if (const Setting* found = setting(propertyName)) {
        // JJ's ruling of 2026-09-28: the rest of the server's settings,
        // held to their ranges as the Core holds them.
        QVariant bounded = value;
        if (found->kind != Setting::Kind::Bool) {
            bounded = qBound(found->min, value.toInt(), found->max);
        }
        setIn(next, *found, bounded);
    } else {
        return false;
    }
    setState(next);
    return true;
}

void StationTciModel::setClients(const QList<StationTciClient>& clients)
{
    if (clients == m_clients) {
        return;
    }
    m_clients = clients;
    emit clientsChanged();
}

} // namespace NereusSDR
