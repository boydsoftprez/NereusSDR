// =================================================================
// src/core/FaultLog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native ring buffer of the last 10 fault events per device.
// See FaultLog.h for full design notes.
//
// AI tooling: Anthropic Claude Code.

#include "core/FaultLog.h"
#include "core/AppSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace NereusSDR {

static constexpr int kMaxEvents = 10;

FaultLog::FaultLog(const QString& deviceKey, QObject* parent)
    : QObject(parent)
    , m_deviceKey(deviceKey)
{
    load();
}

QVector<FaultEvent> FaultLog::events() const
{
    return m_events;
}

void FaultLog::capture(const FaultEvent& ev)
{
    m_events.prepend(ev);
    while (m_events.size() > kMaxEvents) {
        m_events.removeLast();
    }
    save();
    emit changed();
}

void FaultLog::clear()
{
    m_events.clear();
    save();
    emit changed();
}

// static
QString FaultLog::likelyCauseFor(float fwd, float swr, float temp)
{
    if (swr > 2.5f)     { return "SWR trip"; }
    if (temp > 85.0f)   { return "Overtemp"; }
    if (fwd > 1900.0f)  { return "Drive too high"; }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void FaultLog::load()
{
    auto& s = AppSettings::instance();
    const QString raw = s.value(m_deviceKey, "").toString();
    if (raw.isEmpty()) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray()) {
        return;
    }

    m_events.clear();
    const QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) { continue; }
        const QJsonObject obj = v.toObject();
        FaultEvent ev;
        ev.whenMs       = obj.value("whenMs").toInteger();
        ev.state        = obj.value("state").toString();
        ev.fwdAtFaultW  = static_cast<float>(obj.value("fwdAtFaultW").toDouble());
        ev.swrAtFault   = static_cast<float>(obj.value("swrAtFault").toDouble());
        ev.tempAtFaultC = static_cast<float>(obj.value("tempAtFaultC").toDouble());
        ev.likelyCause  = obj.value("likelyCause").toString();
        m_events.append(ev);
    }
}

void FaultLog::save() const
{
    QJsonArray arr;
    for (const FaultEvent& ev : m_events) {
        QJsonObject obj;
        obj["whenMs"]       = QJsonValue(ev.whenMs);
        obj["state"]        = ev.state;
        obj["fwdAtFaultW"]  = static_cast<double>(ev.fwdAtFaultW);
        obj["swrAtFault"]   = static_cast<double>(ev.swrAtFault);
        obj["tempAtFaultC"] = static_cast<double>(ev.tempAtFaultC);
        obj["likelyCause"]  = ev.likelyCause;
        arr.append(obj);
    }
    const QString json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    AppSettings::instance().setValue(m_deviceKey, json);
}

}  // namespace NereusSDR
