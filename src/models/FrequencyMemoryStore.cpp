#include "models/FrequencyMemoryStore.h"

#include "core/AppSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUuid>

namespace NereusSDR {
namespace {

QJsonObject toJson(const FrequencyMemory& m)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), m.id);
    o.insert(QStringLiteral("name"), m.name);
    o.insert(QStringLiteral("frequencyHz"), m.frequencyHz);
    o.insert(QStringLiteral("dspMode"), m.dspMode);
    o.insert(QStringLiteral("filterLow"), m.filterLow);
    o.insert(QStringLiteral("filterHigh"), m.filterHigh);
    o.insert(QStringLiteral("rxAntenna"), m.rxAntenna);
    o.insert(QStringLiteral("txAntenna"), m.txAntenna);
    o.insert(QStringLiteral("agcMode"), m.agcMode);
    o.insert(QStringLiteral("autoAgcEnabled"), m.autoAgcEnabled);
    o.insert(QStringLiteral("autoAgcOffset"), m.autoAgcOffset);
    o.insert(QStringLiteral("agcThreshold"), m.agcThreshold);
    o.insert(QStringLiteral("agcHang"), m.agcHang);
    o.insert(QStringLiteral("agcSlope"), m.agcSlope);
    o.insert(QStringLiteral("agcAttack"), m.agcAttack);
    o.insert(QStringLiteral("agcDecay"), m.agcDecay);
    o.insert(QStringLiteral("agcFixedGain"), m.agcFixedGain);
    o.insert(QStringLiteral("agcMaxGain"), m.agcMaxGain);
    o.insert(QStringLiteral("nbMode"), m.nbMode);
    o.insert(QStringLiteral("activeNr"), m.activeNr);
    o.insert(QStringLiteral("anfEnabled"), m.anfEnabled);
    o.insert(QStringLiteral("txPower"), m.txPower);
    o.insert(QStringLiteral("includeTxPower"), m.includeTxPower);
    o.insert(QStringLiteral("attEnabled"), m.attEnabled);
    o.insert(QStringLiteral("attDb"), m.attDb);
    o.insert(QStringLiteral("preampMode"), m.preampMode);
    o.insert(QStringLiteral("rx1Preamp"), m.rx1Preamp);
    o.insert(QStringLiteral("autoAttEnabled"), m.autoAttEnabled);
    o.insert(QStringLiteral("autoAttMode"), m.autoAttMode);
    o.insert(QStringLiteral("autoAttUndo"), m.autoAttUndo);
    o.insert(QStringLiteral("autoAttUndoDelayMs"), m.autoAttUndoDelayMs);
    o.insert(QStringLiteral("includeAtt"), m.includeAtt);
    o.insert(QStringLiteral("ritEnabled"), m.ritEnabled);
    o.insert(QStringLiteral("ritHz"), m.ritHz);
    o.insert(QStringLiteral("xitEnabled"), m.xitEnabled);
    o.insert(QStringLiteral("xitHz"), m.xitHz);
    return o;
}

FrequencyMemory fromJson(const QJsonObject& o)
{
    FrequencyMemory m;
    m.id = o.value(QStringLiteral("id")).toString();
    m.name = o.value(QStringLiteral("name")).toString();
    m.frequencyHz = o.value(QStringLiteral("frequencyHz")).toDouble();
    m.dspMode = o.value(QStringLiteral("dspMode")).toInt();
    m.filterLow = o.value(QStringLiteral("filterLow")).toInt();
    m.filterHigh = o.value(QStringLiteral("filterHigh")).toInt();
    m.rxAntenna = o.value(QStringLiteral("rxAntenna")).toString();
    m.txAntenna = o.value(QStringLiteral("txAntenna")).toString();
    m.agcMode = o.value(QStringLiteral("agcMode")).toInt();
    m.autoAgcEnabled = o.value(QStringLiteral("autoAgcEnabled")).toBool();
    m.autoAgcOffset = o.value(QStringLiteral("autoAgcOffset")).toDouble();
    m.agcThreshold = o.value(QStringLiteral("agcThreshold")).toInt();
    m.agcHang = o.value(QStringLiteral("agcHang")).toInt();
    m.agcSlope = o.value(QStringLiteral("agcSlope")).toInt();
    m.agcAttack = o.value(QStringLiteral("agcAttack")).toInt();
    m.agcDecay = o.value(QStringLiteral("agcDecay")).toInt();
    m.agcFixedGain = o.value(QStringLiteral("agcFixedGain")).toInt();
    m.agcMaxGain = o.value(QStringLiteral("agcMaxGain")).toInt();
    m.nbMode = o.value(QStringLiteral("nbMode")).toInt();
    m.activeNr = o.value(QStringLiteral("activeNr")).toInt();
    m.anfEnabled = o.value(QStringLiteral("anfEnabled")).toBool();
    m.txPower = o.value(QStringLiteral("txPower")).toInt();
    m.includeTxPower = o.value(QStringLiteral("includeTxPower")).toBool(true);
    m.attEnabled = o.value(QStringLiteral("attEnabled")).toBool();
    m.attDb = o.value(QStringLiteral("attDb")).toInt();
    m.preampMode = o.value(QStringLiteral("preampMode")).toInt();
    m.rx1Preamp = o.value(QStringLiteral("rx1Preamp")).toBool();
    m.autoAttEnabled = o.value(QStringLiteral("autoAttEnabled")).toBool();
    m.autoAttMode = o.value(QStringLiteral("autoAttMode")).toInt();
    m.autoAttUndo = o.value(QStringLiteral("autoAttUndo")).toBool();
    m.autoAttUndoDelayMs = o.value(QStringLiteral("autoAttUndoDelayMs")).toInt();
    m.includeAtt = o.value(QStringLiteral("includeAtt")).toBool(true);
    m.ritEnabled = o.value(QStringLiteral("ritEnabled")).toBool();
    m.ritHz = o.value(QStringLiteral("ritHz")).toInt();
    m.xitEnabled = o.value(QStringLiteral("xitEnabled")).toBool();
    m.xitHz = o.value(QStringLiteral("xitHz")).toInt();
    return m;
}

} // namespace

void FrequencyMemoryStore::load()
{
    m_entries.clear();
    const QString text = AppSettings::instance().value(kSettingsKey).toString();
    if (text.trimmed().isEmpty()) { return; }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) { return; }

    const QJsonArray entries = doc.isArray()
        ? doc.array()
        : doc.object().value(QStringLiteral("entries")).toArray();
    m_entries.reserve(entries.size());
    for (const QJsonValue& value : entries) {
        if (value.isObject()) { m_entries.append(fromJson(value.toObject())); }
    }
}

void FrequencyMemoryStore::save() const
{
    QJsonArray entries;
    for (const FrequencyMemory& entry : m_entries) { entries.append(toJson(entry)); }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("entries"), entries);
    AppSettings::instance().setValue(
        kSettingsKey,
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

FrequencyMemory* FrequencyMemoryStore::find(const QString& id)
{
    for (FrequencyMemory& entry : m_entries) {
        if (entry.id == id) { return &entry; }
    }
    return nullptr;
}

const FrequencyMemory* FrequencyMemoryStore::find(const QString& id) const
{
    for (const FrequencyMemory& entry : m_entries) {
        if (entry.id == id) { return &entry; }
    }
    return nullptr;
}

void FrequencyMemoryStore::add(FrequencyMemory entry)
{
    if (entry.id.isEmpty()) {
        entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    m_entries.append(entry);
    save();
}

void FrequencyMemoryStore::update(const FrequencyMemory& entry)
{
    for (FrequencyMemory& existing : m_entries) {
        if (existing.id == entry.id) {
            existing = entry;
            save();
            return;
        }
    }
}

void FrequencyMemoryStore::remove(const QString& id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            m_entries.removeAt(i);
            save();
            return;
        }
    }
}

} // namespace NereusSDR
