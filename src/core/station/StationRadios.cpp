// no-port-check: NereusSDR-original. The radios a Core finds and which one
// it runs.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/station/StationRadios.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See StationRadios.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 21, R-IOS-18,
//                                    R-R3-38, R-R3-49). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Phone wire batch: modelLabel and
//                                    models on each record. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/station/StationRadios.h"

#include "core/AppSettings.h"
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"

#include <QJsonArray>

namespace NereusSDR {

namespace {

QString normalized(const QString& mac)
{
    return mac.trimmed().toUpper();
}

bool sameMac(const QString& a, const QString& b)
{
    return !a.isEmpty() && normalized(a) == normalized(b);
}

} // namespace

// ── StationRadioEntry ────────────────────────────────────────────────────

QJsonObject StationRadioEntry::toFields() const
{
    // Phone wire batch: each choice as {model, label}.
    QJsonArray choices;
    for (int m : models) {
        choices.append(QJsonObject{
            {QStringLiteral("model"), m},
            {QStringLiteral("label"),
             QString::fromLatin1(displayName(static_cast<HPSDRModel>(m)))},
        });
    }
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("model"), model},
        {QStringLiteral("mac"), mac},
        {QStringLiteral("address"), address},
        {QStringLiteral("protocol"), protocol},
        {QStringLiteral("inUse"), inUse},
        {QStringLiteral("modelLabel"), modelLabel},
        {QStringLiteral("models"), choices},
    };
}

std::optional<StationRadioEntry> StationRadioEntry::fromFields(const QString& id,
                                                               const QJsonObject& fields)
{
    if (!fields.value(QStringLiteral("mac")).isString()
        || !fields.value(QStringLiteral("name")).isString()
        || !fields.value(QStringLiteral("model")).isDouble()
        || !fields.value(QStringLiteral("address")).isString()
        || !fields.value(QStringLiteral("protocol")).isDouble()
        || !fields.value(QStringLiteral("inUse")).isBool()) {
        return std::nullopt;
    }
    StationRadioEntry e;
    e.id = id;
    e.name = fields.value(QStringLiteral("name")).toString();
    e.model = fields.value(QStringLiteral("model")).toInt();
    e.mac = fields.value(QStringLiteral("mac")).toString();
    e.address = fields.value(QStringLiteral("address")).toString();
    e.protocol = fields.value(QStringLiteral("protocol")).toInt();
    e.inUse = fields.value(QStringLiteral("inUse")).toBool();
    // Phone wire batch: absent for a peer that did not declare radioModels.
    e.modelLabel = fields.value(QStringLiteral("modelLabel")).toString();
    for (const QJsonValue& choice : fields.value(QStringLiteral("models")).toArray()) {
        const QJsonValue m = choice.toObject().value(QStringLiteral("model"));
        if (m.isDouble()) {
            e.models.append(m.toInt());
        }
    }
    return e;
}

bool StationRadioEntry::operator==(const StationRadioEntry& other) const
{
    return id == other.id && name == other.name && model == other.model && mac == other.mac
        && address == other.address && protocol == other.protocol && inUse == other.inUse
        && modelLabel == other.modelLabel && models == other.models;
}

// ── The choice order ─────────────────────────────────────────────────────

StationRadios::Choice StationRadios::choose(const QList<RadioInfo>& visible,
                                            const QString& identified, const QString& saved,
                                            const QString& configured)
{
    Choice choice;
    // (1) a radio chosen from an app, (2) radio_mac; a run that already
    // identified its radio keeps it (a reconnect).
    const QString wanted = !identified.isEmpty() ? identified
        : !saved.isEmpty()                      ? saved
                                                : configured;
    if (!wanted.isEmpty()) {
        for (const RadioInfo& info : visible) {
            if (sameMac(info.macAddress, wanted)) {
                if (info.inUse) {
                    choice.pick = Pick::WaitForChosen;
                    choice.reason = QStringLiteral(
                        "The Core's radio is in use by another program. The Core will "
                        "connect when it is free.");
                    return choice;
                }
                choice.pick = Pick::Radio;
                choice.radio = info;
                return choice;
            }
        }
        choice.pick = Pick::WaitForChosen;
        choice.reason = QStringLiteral("The Core is waiting for its radio to appear on the "
                                       "network.");
        return choice;
    }
    // (3) automatic, only with exactly one radio in sight.
    QList<RadioInfo> radios;
    for (const RadioInfo& info : visible) {
        if (!info.macAddress.isEmpty()) {
            radios.append(info);
        }
    }
    if (radios.isEmpty()) {
        choice.pick = Pick::NoRadio;
        choice.reason = QStringLiteral("The Core cannot see a radio on its network.");
        return choice;
    }
    if (radios.size() > 1) {
        // (4) never the first found: the operator chooses.
        choice.pick = Pick::WaitForChoice;
        choice.reason = QStringLiteral("The Core can see more than one radio. Choose which one "
                                       "it runs.");
        return choice;
    }
    if (radios.first().inUse) {
        choice.pick = Pick::WaitForChosen;
        choice.reason = QStringLiteral("The only radio the Core can see is in use by another "
                                       "program. The Core will connect when it is free.");
        return choice;
    }
    choice.pick = Pick::Radio;
    choice.radio = radios.first();
    return choice;
}

// ── The list ─────────────────────────────────────────────────────────────

StationRadios::StationRadios(AppSettings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
}

QString StationRadios::savedChoice() const
{
    return normalized(m_settings.value(QLatin1String(kChoiceKey)).toString());
}

void StationRadios::saveChoice(const QString& mac)
{
    m_settings.setValue(QLatin1String(kChoiceKey), normalized(mac));
    m_settings.save();
}

void StationRadios::confirmChoice(const QString& mac)
{
    if (m_pending.isEmpty() || !sameMac(m_pending, mac)) {
        return;
    }
    saveChoice(m_pending);
    m_pending.clear();
}

void StationRadios::dropPendingChoice()
{
    m_pending.clear();
}

void StationRadios::setTarget(const QString& mac)
{
    m_target = normalized(mac);
}

void StationRadios::setVisible(const QList<RadioInfo>& found)
{
    m_visible.clear();
    for (const RadioInfo& info : found) {
        if (!info.macAddress.isEmpty()) {
            m_visible.append(info);
        }
    }
    emit entriesChanged();
}

void StationRadios::setCurrent(const RadioInfo& radio)
{
    if (radio.macAddress.isEmpty()) {
        clearCurrent();
        return;
    }
    m_current = radio;
    m_hasCurrent = true;
    m_waiting.clear();
    emit entriesChanged();
}

void StationRadios::clearCurrent()
{
    if (!m_hasCurrent) {
        return;
    }
    // Still one of the Core's radios: it stays listed until a scan says
    // otherwise.
    bool listed = false;
    for (const RadioInfo& info : std::as_const(m_visible)) {
        listed = listed || sameMac(info.macAddress, m_current.macAddress);
    }
    if (!listed) {
        m_visible.append(m_current);
    }
    m_hasCurrent = false;
    m_current = RadioInfo{};
    emit entriesChanged();
}

QString StationRadios::currentMac() const
{
    return m_hasCurrent ? normalized(m_current.macAddress) : QString();
}

void StationRadios::setWaiting(const QString& reason)
{
    if (m_waiting == reason) {
        return;
    }
    m_waiting = reason;
    emit entriesChanged();
}

void StationRadios::setSwitching(bool switching)
{
    m_switching = switching;
}

HPSDRModel StationRadios::overrideFor(const QString& mac) const
{
    return m_settings.modelOverride(normalized(mac));
}

int StationRadios::modelFor(const RadioInfo& radio) const
{
    const HPSDRModel override = overrideFor(radio.macAddress);
    const HPSDRModel model = override != HPSDRModel::FIRST ? override
                                                           : defaultModelForBoard(radio.boardType);
    return static_cast<int>(model);
}

std::optional<RadioInfo> StationRadios::radioFor(const QString& mac) const
{
    if (m_hasCurrent && sameMac(m_current.macAddress, mac)) {
        return m_current;
    }
    for (const RadioInfo& info : m_visible) {
        if (sameMac(info.macAddress, mac)) {
            return info;
        }
    }
    return std::nullopt;
}

QList<StationRadioEntry> StationRadios::entries() const
{
    QList<StationRadioEntry> out;
    const auto entryFor = [this](const RadioInfo& info, bool inUse) {
        StationRadioEntry e;
        e.mac = normalized(info.macAddress);
        e.id = e.mac;
        e.name = info.displayName();
        e.model = modelFor(info);
        e.address = info.address.isNull() ? QString() : info.address.toString();
        e.protocol = info.protocol == ProtocolVersion::Protocol2 ? 2 : 1;
        e.inUse = inUse;
        // Phone wire batch: the label Setup shows for the model (displayName,
        // HpsdrModel.h, Thetis enums.cs), and the models the board can run
        // as: compatibleModels (HardwareProfile.cpp), the board check of
        // Thetis NetworkIO.cs:160-176 [v2.10.3.15], which the model combo
        // offers and setModel() below enforces.
        //   //[2.10.3.9]MW0LGE added board check, issue icon shown in setup
        e.modelLabel = QString::fromLatin1(displayName(static_cast<HPSDRModel>(e.model)));
        for (HPSDRModel m : compatibleModels(info.boardType)) {
            e.models.append(static_cast<int>(m));
        }
        return e;
    };
    if (m_hasCurrent) {
        out.append(entryFor(m_current, true));
    }
    for (const RadioInfo& info : m_visible) {
        if (m_hasCurrent && sameMac(info.macAddress, m_current.macAddress)) {
            continue;
        }
        out.append(entryFor(info, false));
    }
    return out;
}

// ── Requests ─────────────────────────────────────────────────────────────

QString StationRadios::unknownRadioReason()
{
    return QStringLiteral("The Core cannot see that radio. Scan again, then choose it.");
}

QString StationRadios::switchingReason()
{
    return QStringLiteral("The Core is changing its radio. Try again when it has finished.");
}

QString StationRadios::inUseReason()
{
    return QStringLiteral("The Core is using this radio. Choose another radio first.");
}

QString StationRadios::pairedDeviceReason()
{
    return QStringLiteral("Change the Core's radio from a paired device.");
}

bool StationRadios::select(const QString& mac, QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason != nullptr) {
            *reason = why;
        }
        return false;
    };
    if (m_switching) {
        return refuse(switchingReason());
    }
    const std::optional<RadioInfo> radio = radioFor(mac);
    if (!radio) {
        return refuse(unknownRadioReason());
    }
    if (m_hasCurrent && sameMac(m_current.macAddress, mac)) {
        return true; // already the Core's radio
    }
    if (radio->inUse) {
        return refuse(QStringLiteral("That radio is in use by another program."));
    }
    // Fix wave, I6: this run's pending choice; saved only once it connects
    // (confirmChoice), so a Core restarted before then never reloads it.
    m_pending = normalized(mac);
    m_switching = true;
    if (onSelect) {
        onSelect(normalized(mac));
    }
    return true;
}

bool StationRadios::rescan(QString* /*reason: a rescan is always taken*/)
{
    if (onRescan) {
        onRescan();
    }
    return true;
}

bool StationRadios::setModel(const QString& mac, int model, QString* reason)
{
    const auto refuse = [reason](const QString& why) {
        if (reason != nullptr) {
            *reason = why;
        }
        return false;
    };
    const std::optional<RadioInfo> radio = radioFor(mac);
    if (!radio) {
        return refuse(unknownRadioReason());
    }
    if (!compatibleModels(radio->boardType).contains(static_cast<HPSDRModel>(model))) {
        return refuse(QStringLiteral("That model does not match this radio."));
    }
    // The local Edit radio's override: saved for this radio, applied at its
    // next connect.
    m_settings.setModelOverride(normalized(mac), static_cast<HPSDRModel>(model));
    m_settings.save();
    emit entriesChanged();
    return true;
}

bool StationRadios::forget(const QString& mac, QString* reason)
{
    // Fix wave, M1: also the radio the Core is reconnecting to or waiting
    // for, and a pending choice.
    if ((m_hasCurrent && sameMac(m_current.macAddress, mac)) || sameMac(m_target, mac)
        || sameMac(m_pending, mac)) {
        if (reason != nullptr) {
            *reason = inUseReason();
        }
        return false;
    }
    const QString key = normalized(mac);
    m_settings.forgetRadio(key);
    if (savedChoice() == key) {
        m_settings.remove(QLatin1String(kChoiceKey));
    }
    m_settings.save();
    for (int i = m_visible.size() - 1; i >= 0; --i) {
        if (sameMac(m_visible.at(i).macAddress, key)) {
            m_visible.removeAt(i);
        }
    }
    emit entriesChanged();
    return true;
}

} // namespace NereusSDR
