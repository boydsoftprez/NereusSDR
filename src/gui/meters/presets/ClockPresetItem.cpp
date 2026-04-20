// =================================================================
// src/gui/meters/presets/ClockPresetItem.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1552-1572 (createClockPreset helper).
// Ships under the project's GPLv3 terms (root LICENSE); no per-file
// verbatim upstream header required because no Thetis source code was
// consulted in writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. Single-rect paint using QDateTime for the
//                current time; toggles UTC vs local via the m_utc
//                flag which round-trips through serialisation.
// =================================================================

#include "gui/meters/presets/ClockPresetItem.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

namespace NereusSDR {

ClockPresetItem::ClockPresetItem(QObject* parent)
    : MeterItem(parent)
{
}

void ClockPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }
    p.save();
    p.fillRect(rect, m_backdropColor);

    const QDateTime now = m_utc
        ? QDateTime::currentDateTimeUtc()
        : QDateTime::currentDateTime();
    const QString stamp = now.toString(QStringLiteral("HH:mm:ss"))
        + (m_utc ? QStringLiteral(" UTC") : QString());

    p.setPen(m_textColor);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(qMax(8.0, rect.height() * 0.55));
    p.setFont(f);
    p.drawText(rect, Qt::AlignCenter, stamp);
    p.restore();
}

QString ClockPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"), QStringLiteral("ClockPreset"));
    o.insert(QStringLiteral("x"),    static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),    static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),    static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),    static_cast<double>(m_h));
    o.insert(QStringLiteral("utc"),  m_utc);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool ClockPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("ClockPreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    m_utc = o.value(QStringLiteral("utc")).toBool(m_utc);
    return true;
}

} // namespace NereusSDR
