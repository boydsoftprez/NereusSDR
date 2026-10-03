// =================================================================
// src/core/FaultLog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native ring buffer of the last 10 fault events per device.
// See FaultLog.h for full design notes.
//
// AI tooling: Anthropic Claude Code.
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: device, plain text
//                                    and detail on every record; notices;
//                                    JSON for the mirrored list and a
//                                    remote window's copy. AI-assisted via
//                                    Anthropic Claude Code.

#include "core/FaultLog.h"
#include "core/AppSettings.h"

#include <QDateTime>
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

void FaultLog::capture(const FaultEvent& event)
{
    FaultEvent ev = event;
    if (ev.device.isEmpty()) {
        ev.device = deviceId();
    }
    if (ev.text.isEmpty()) {
        ev.text = plainTextFor(ev.device, ev.state, ev.likelyCause);
    }
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

void FaultLog::reload()
{
    // See FaultLog.h's doc comment for the full ordering contract this
    // exists to satisfy (Remote Daemon R2, Task 15). load() re-reads
    // m_deviceKey through the SAME AppSettings::instance().value() call
    // the constructor uses -- on a remote-mode GUI with a SettingsProxy
    // installed via AppSettings::setRemoteBackend(), that now answers
    // out of the connect-time snapshot instead of an empty local store.
    // Unlike the constructor's own load() call, this emits changed():
    // reload() is only useful to call on an object that may already have
    // observers (a bound UI, in particular), where the constructor path
    // never has any.
    load();
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
// R-R3-47 / R-R3-22: device, plain words, notices and the mirrored list
// ---------------------------------------------------------------------------

// static
QString FaultLog::deviceIdForKey(const QString& deviceKey)
{
    if (deviceKey.startsWith(QLatin1String("TGXL_"))) {
        return QStringLiteral("tgxl");
    }
    if (deviceKey.startsWith(QLatin1String("RfKit_"))) {
        return QStringLiteral("rfkit");
    }
    return QStringLiteral("pgxl");
}

// static
QString FaultLog::plainTextFor(const QString& device, const QString& state,
                               const QString& likelyCause)
{
    if (device == QLatin1String("pgxl") && state.startsWith(QLatin1String("FAULT"))) {
        QString text = QStringLiteral("The Power Genius reported a fault.");
        if (likelyCause == QLatin1String("SWR trip")) {
            text += QStringLiteral(" Likely cause: high SWR.");
        } else if (likelyCause == QLatin1String("Overtemp")) {
            text += QStringLiteral(" Likely cause: the amplifier was too hot.");
        } else if (likelyCause == QLatin1String("Drive too high")) {
            text += QStringLiteral(" Likely cause: too much drive from the radio.");
        }
        return text;
    }
    if (device == QLatin1String("tgxl")) {
        return QStringLiteral("The Tuner Genius reported a problem.");
    }
    if (device == QLatin1String("rfkit")) {
        return QStringLiteral("The RF-Kit amplifier reported a problem.");
    }
    return QStringLiteral("The Power Genius reported a problem.");
}

void FaultLog::captureNotice(const QString& state, const QString& text, const QString& detail)
{
    FaultEvent ev;
    ev.whenMs = QDateTime::currentMSecsSinceEpoch();
    ev.state = state;
    ev.fwdAtFaultW = 0.0f;
    ev.swrAtFault = 0.0f;
    ev.tempAtFaultC = 0.0f;
    ev.device = deviceId();
    ev.text = text;
    ev.detail = detail;
    capture(ev);
}

QString FaultLog::toJson() const
{
    QJsonArray arr;
    for (const FaultEvent& ev : m_events) {
        QJsonObject obj;
        obj["whenMs"]       = QJsonValue(ev.whenMs);
        obj["device"]       = ev.device;
        obj["state"]        = ev.state;
        obj["text"]         = ev.text;
        obj["detail"]       = ev.detail;
        obj["fwdAtFaultW"]  = static_cast<double>(ev.fwdAtFaultW);
        obj["swrAtFault"]   = static_cast<double>(ev.swrAtFault);
        obj["tempAtFaultC"] = static_cast<double>(ev.tempAtFaultC);
        obj["likelyCause"]  = ev.likelyCause;
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

// static
QVector<FaultEvent> FaultLog::eventsFromJson(const QString& json, const QString& device)
{
    QVector<FaultEvent> events;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) {
        return events;
    }
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
        // Records saved before R-R3-47 carry none of the three below.
        ev.device = obj.value("device").toString(device);
        if (ev.device.isEmpty()) {
            ev.device = device;
        }
        ev.text = obj.value("text").toString();
        if (ev.text.isEmpty()) {
            ev.text = plainTextFor(ev.device, ev.state, ev.likelyCause);
        }
        ev.detail = obj.value("detail").toString();
        events.append(ev);
        if (events.size() >= kMaxEvents) {
            break;
        }
    }
    return events;
}

void FaultLog::applyMirroredJson(const QString& json)
{
    m_events = eventsFromJson(json, deviceId());
    emit changed();
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

    m_events = eventsFromJson(raw, deviceId());
}

void FaultLog::save() const
{
    AppSettings::instance().setValue(m_deviceKey, toJson());
}

}  // namespace NereusSDR
