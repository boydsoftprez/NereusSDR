// no-port-check: NereusSDR-original. See StationCatModel.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/StationCatModel.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created (CAT setup from a connected
//                                    desktop, stationCatVersion 1).
//                                    AI tooling: Claude Code.
// =================================================================

#include "models/StationCatModel.h"

#include <QJsonDocument>

#include <limits>

namespace NereusSDR {

namespace {

// Each reader takes the field when `json` has it, and fails only on a field
// of the wrong type.
bool readBool(const QJsonObject& json, const char* name, bool& out)
{
    const QJsonValue value = json.value(QLatin1String(name));
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    out = value.toBool();
    return true;
}

bool readInt(const QJsonObject& json, const char* name, int& out)
{
    const QJsonValue value = json.value(QLatin1String(name));
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (number < static_cast<double>(std::numeric_limits<int>::min())
        || number > static_cast<double>(std::numeric_limits<int>::max())
        || number != static_cast<double>(static_cast<int>(number))) {
        return false;
    }
    out = static_cast<int>(number);
    return true;
}

bool readText(const QJsonObject& json, const char* name, QString& out)
{
    const QJsonValue value = json.value(QLatin1String(name));
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isString()) {
        return false;
    }
    out = value.toString();
    return true;
}

} // namespace

QString StationCatModel::readOnlyReason()
{
    return QStringLiteral("The Core reports its CAT setup here. Change it on this app's CAT "
                          "pages.");
}

QJsonObject StationCatModel::channelConfigToJson(const CatEndpointConfig& config)
{
    return QJsonObject{
        {QStringLiteral("channel"), config.channel},
        {QStringLiteral("primarySliceId"), config.binding.primarySliceId},
        {QStringLiteral("secondarySliceId"), config.binding.secondarySliceId.value_or(-1)},
        {QStringLiteral("tcpEnabled"), config.tcpEnabled},
        {QStringLiteral("serialEnabled"), config.serialEnabled},
        {QStringLiteral("ptyEnabled"), config.ptyEnabled},
        {QStringLiteral("rigctldEnabled"), config.rigctldEnabled},
        {QStringLiteral("tcpBindAddress"), config.tcpBindAddress},
        {QStringLiteral("rigctldBindAddress"), config.rigctldBindAddress},
        {QStringLiteral("tcpPort"), config.tcpPort},
        {QStringLiteral("rigctldPort"), config.rigctldPort},
        {QStringLiteral("serialDevice"), config.serialDevice},
        {QStringLiteral("serialBaud"), config.serialBaud},
        {QStringLiteral("serialParity"), config.serialParity},
        {QStringLiteral("serialDataBits"), config.serialDataBits},
        {QStringLiteral("serialStopBits"), config.serialStopBits},
        {QStringLiteral("ptyDialect"), config.ptyDialect},
    };
}

std::optional<CatEndpointConfig> StationCatModel::channelConfigFromJson(
    const QJsonObject& json, const CatEndpointConfig& base)
{
    CatEndpointConfig config = base;
    int secondary = base.binding.secondarySliceId.value_or(-1);
    const bool read = readInt(json, "channel", config.channel)
        && readInt(json, "primarySliceId", config.binding.primarySliceId)
        && readInt(json, "secondarySliceId", secondary)
        && readBool(json, "tcpEnabled", config.tcpEnabled)
        && readBool(json, "serialEnabled", config.serialEnabled)
        && readBool(json, "ptyEnabled", config.ptyEnabled)
        && readBool(json, "rigctldEnabled", config.rigctldEnabled)
        && readText(json, "tcpBindAddress", config.tcpBindAddress)
        && readText(json, "rigctldBindAddress", config.rigctldBindAddress)
        && readInt(json, "tcpPort", config.tcpPort)
        && readInt(json, "rigctldPort", config.rigctldPort)
        && readText(json, "serialDevice", config.serialDevice)
        && readInt(json, "serialBaud", config.serialBaud)
        && readText(json, "serialParity", config.serialParity)
        && readInt(json, "serialDataBits", config.serialDataBits)
        && readText(json, "serialStopBits", config.serialStopBits)
        && readText(json, "ptyDialect", config.ptyDialect);
    if (!read) {
        return std::nullopt;
    }
    if (config.binding.primarySliceId < 0) {
        config.binding.primarySliceId = -1;
    }
    if (secondary < 0) {
        config.binding.secondarySliceId.reset();
        config.binding.secondaryIncarnation.reset();
    } else {
        config.binding.secondarySliceId = secondary;
    }
    return config;
}

QJsonObject StationCatModel::globalConfigToJson(const CatGlobalConfig& config)
{
    return QJsonObject{
        {QStringLiteral("sendWelcome"), config.sendWelcome},
        {QStringLiteral("rigIdentity"), config.rigIdentity},
        {QStringLiteral("allowKenwoodAi"), config.allowKenwoodAi},
        {QStringLiteral("aiEnabled"), config.aiEnabled},
        {QStringLiteral("aiSerial1"), config.aiSerial1},
        {QStringLiteral("aiSerial2"), config.aiSerial2},
        {QStringLiteral("aiSerial3"), config.aiSerial3},
        {QStringLiteral("aiSerial4"), config.aiSerial4},
        {QStringLiteral("aiTcp"), config.aiTcp},
        {QStringLiteral("digitalReportsSideband"), config.digitalReportsSideband},
        {QStringLiteral("recenterVfo"), config.recenterVfo},
        {QStringLiteral("serialNumber"), config.serialNumber},
        {QStringLiteral("limitReportedPower"), config.limitReportedPower},
        {QStringLiteral("rttyOffsetAEnabled"), config.rttyOffsetAEnabled},
        {QStringLiteral("rttyOffsetBEnabled"), config.rttyOffsetBEnabled},
        {QStringLiteral("rttyDiguHz"), config.rttyDiguHz},
        {QStringLiteral("rttyDiglHz"), config.rttyDiglHz},
        {QStringLiteral("pttEnabled"), config.pttEnabled},
        {QStringLiteral("pttDeviceSource"), config.pttDeviceSource},
        {QStringLiteral("pttSerialDevice"), config.pttSerialDevice},
        {QStringLiteral("pttUseCts"), config.pttUseCts},
        {QStringLiteral("pttUseDsr"), config.pttUseDsr},
        {QStringLiteral("pttChannel"), config.pttChannel},
        {QStringLiteral("pttSerialBaud"), config.pttSerialBaud},
        {QStringLiteral("pttSerialParity"), config.pttSerialParity},
        {QStringLiteral("pttSerialDataBits"), config.pttSerialDataBits},
        {QStringLiteral("pttSerialStopBits"), config.pttSerialStopBits},
    };
}

std::optional<CatGlobalConfig> StationCatModel::globalConfigFromJson(const QJsonObject& json,
                                                                     const CatGlobalConfig& base)
{
    CatGlobalConfig config = base;
    const bool read = readBool(json, "sendWelcome", config.sendWelcome)
        && readText(json, "rigIdentity", config.rigIdentity)
        && readBool(json, "allowKenwoodAi", config.allowKenwoodAi)
        && readBool(json, "aiEnabled", config.aiEnabled)
        && readBool(json, "aiSerial1", config.aiSerial1)
        && readBool(json, "aiSerial2", config.aiSerial2)
        && readBool(json, "aiSerial3", config.aiSerial3)
        && readBool(json, "aiSerial4", config.aiSerial4)
        && readBool(json, "aiTcp", config.aiTcp)
        && readBool(json, "digitalReportsSideband", config.digitalReportsSideband)
        && readBool(json, "recenterVfo", config.recenterVfo)
        && readText(json, "serialNumber", config.serialNumber)
        && readBool(json, "limitReportedPower", config.limitReportedPower)
        && readBool(json, "rttyOffsetAEnabled", config.rttyOffsetAEnabled)
        && readBool(json, "rttyOffsetBEnabled", config.rttyOffsetBEnabled)
        && readInt(json, "rttyDiguHz", config.rttyDiguHz)
        && readInt(json, "rttyDiglHz", config.rttyDiglHz)
        && readBool(json, "pttEnabled", config.pttEnabled)
        && readText(json, "pttDeviceSource", config.pttDeviceSource)
        && readText(json, "pttSerialDevice", config.pttSerialDevice)
        && readBool(json, "pttUseCts", config.pttUseCts)
        && readBool(json, "pttUseDsr", config.pttUseDsr)
        && readInt(json, "pttChannel", config.pttChannel)
        && readInt(json, "pttSerialBaud", config.pttSerialBaud)
        && readText(json, "pttSerialParity", config.pttSerialParity)
        && readInt(json, "pttSerialDataBits", config.pttSerialDataBits)
        && readText(json, "pttSerialStopBits", config.pttSerialStopBits);
    if (!read) {
        return std::nullopt;
    }
    return config;
}

QString StationCatModel::toText(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject StationCatModel::fromText(const QString& text)
{
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8());
    return document.isObject() ? document.object() : QJsonObject{};
}

StationCatModel::StationCatModel(QObject* parent)
    : QObject(parent)
{
}

QString StationCatModel::channel(int channel) const
{
    return channel >= 1 && channel <= kChannels ? m_channels[channel - 1] : QString();
}

void StationCatModel::setGlobal(const QString& json)
{
    if (json == m_global) {
        return;
    }
    m_global = json;
    emit stateChanged();
}

void StationCatModel::setChannel(int channel, const QString& json)
{
    if (channel < 1 || channel > kChannels || json == m_channels[channel - 1]) {
        return;
    }
    m_channels[channel - 1] = json;
    emit stateChanged();
}

void StationCatModel::setPlatform(const QString& json)
{
    if (json == m_platform) {
        return;
    }
    m_platform = json;
    emit stateChanged();
}

void StationCatModel::setLastTest(const QString& json)
{
    if (json == m_lastTest) {
        return;
    }
    m_lastTest = json;
    emit stateChanged();
}

bool StationCatModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    const QString text = value.toString();
    if (propertyName == "global") {
        setGlobal(text);
    } else if (propertyName == "channel1") {
        setChannel(1, text);
    } else if (propertyName == "channel2") {
        setChannel(2, text);
    } else if (propertyName == "channel3") {
        setChannel(3, text);
    } else if (propertyName == "channel4") {
        setChannel(4, text);
    } else if (propertyName == "platform") {
        setPlatform(text);
    } else if (propertyName == "lastTest") {
        setLastTest(text);
    } else {
        return false;
    }
    return true;
}

} // namespace NereusSDR
