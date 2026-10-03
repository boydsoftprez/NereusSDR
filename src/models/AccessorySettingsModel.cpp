// no-port-check: NereusSDR-original. See AccessorySettingsModel.h.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/AccessorySettingsModel.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-47, R-R3-22). AI-assisted
//                                    via Anthropic Claude Code.
// =================================================================

#include "models/AccessorySettingsModel.h"

namespace NereusSDR {

QString AccessorySettingsModel::readOnlyReason()
{
    return QStringLiteral("The Core reports the amplifier's and tuner's own settings. "
                          "Change them from this app's Setup pages.");
}

AccessorySettingsModel::AccessorySettingsModel(QObject* parent)
    : QObject(parent)
{
}

void AccessorySettingsModel::setPgxl(const Device& device)
{
    if (m_pgxl == device) {
        return;
    }
    m_pgxl = device;
    emit pgxlChanged();
}

void AccessorySettingsModel::setTgxl(const Device& device)
{
    if (m_tgxl == device) {
        return;
    }
    m_tgxl = device;
    emit tgxlChanged();
}

bool AccessorySettingsModel::applyDeviceValue(Device* device, const QByteArray& name,
                                              const QVariant& value)
{
    if (name == "Nickname") {
        device->nickname = value.toString();
    } else if (name == "BiasMode") {
        device->biasMode = value.toString();
    } else if (name == "FanMode") {
        device->fanMode = value.toString();
    } else if (name == "LedIntensity") {
        device->ledIntensity = value.toInt();
    } else if (name == "NetworkKnown") {
        device->networkKnown = value.toBool();
    } else if (name == "Dhcp") {
        device->dhcp = value.toBool();
    } else if (name == "Address") {
        device->address = value.toString();
    } else if (name == "Netmask") {
        device->netmask = value.toString();
    } else if (name == "Gateway") {
        device->gateway = value.toString();
    } else if (name == "Answer") {
        device->answer = value.toString();
    } else if (name == "AnswerAccepted") {
        device->answerAccepted = value.toBool();
    } else if (name == "AnswerCount") {
        device->answerCount = value.toLongLong();
    } else {
        return false;
    }
    return true;
}

bool AccessorySettingsModel::applyStationValue(const QByteArray& propertyName,
                                               const QVariant& value)
{
    // A plain state apply for every property: nothing here is ever a
    // command to an accessory.
    if (propertyName.startsWith("pgxl")) {
        Device next = m_pgxl;
        if (!applyDeviceValue(&next, propertyName.mid(4), value)) {
            return false;
        }
        setPgxl(next);
        return true;
    }
    if (propertyName.startsWith("tgxl")) {
        Device next = m_tgxl;
        const QByteArray rest = propertyName.mid(4);
        // The Tuner Genius has no hardware settings on the wire.
        if (rest == "BiasMode" || rest == "FanMode" || rest == "LedIntensity"
            || !applyDeviceValue(&next, rest, value)) {
            return false;
        }
        setTgxl(next);
        return true;
    }
    return false;
}

} // namespace NereusSDR
