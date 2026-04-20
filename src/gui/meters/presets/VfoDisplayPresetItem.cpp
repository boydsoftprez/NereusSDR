// =================================================================
// src/gui/meters/presets/VfoDisplayPresetItem.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1537-1551 (createVfoDisplayPreset
// helper). Ships under the project's GPLv3 terms (root LICENSE);
// no per-file verbatim upstream header required because no Thetis
// source code was consulted in writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. Paints the frequency as
//                "NNN.NNN.NNN" MHz-style digits plus top-line
//                band/mode labels; serialisation keeps all three
//                strings so persisted containers restore their
//                last-displayed frequency on relaunch.
// =================================================================

#include "gui/meters/presets/VfoDisplayPresetItem.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

namespace NereusSDR {

VfoDisplayPresetItem::VfoDisplayPresetItem(QObject* parent)
    : MeterItem(parent)
{
}

void VfoDisplayPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }
    p.save();
    p.fillRect(rect, m_backdropColor);

    // Top ~30% — band / mode label strip.
    const int labelH = rect.height() * 30 / 100;
    const QRect labelRect(rect.x(), rect.y(), rect.width(), labelH);
    p.setPen(m_labelColor);
    QFont lf = p.font();
    lf.setPointSizeF(qMax(8.0, labelH * 0.6));
    p.setFont(lf);
    p.drawText(labelRect.adjusted(4, 0, 0, 0),
               Qt::AlignLeft | Qt::AlignVCenter,
               m_bandLabel);
    p.drawText(labelRect.adjusted(0, 0, -4, 0),
               Qt::AlignRight | Qt::AlignVCenter,
               m_modeLabel);

    // Bottom ~70% — large frequency digits (MHz.kHz.Hz).
    const QRect digitRect(rect.x(), rect.y() + labelH,
                          rect.width(), rect.height() - labelH);
    const double mhz = m_freqHz / 1.0e6;
    const QString digitStr =
        QStringLiteral("%1 MHz").arg(mhz, 0, 'f', 3);

    p.setPen(m_digitColor);
    QFont df = p.font();
    df.setBold(true);
    df.setPointSizeF(qMax(12.0, digitRect.height() * 0.7));
    p.setFont(df);
    p.drawText(digitRect, Qt::AlignCenter, digitStr);
    p.restore();
}

QString VfoDisplayPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),       QStringLiteral("VfoDisplayPreset"));
    o.insert(QStringLiteral("x"),          static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),          static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),          static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),          static_cast<double>(m_h));
    o.insert(QStringLiteral("freqHz"),     m_freqHz);
    o.insert(QStringLiteral("bandLabel"),  m_bandLabel);
    o.insert(QStringLiteral("modeLabel"),  m_modeLabel);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool VfoDisplayPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("VfoDisplayPreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    m_freqHz    = o.value(QStringLiteral("freqHz")).toDouble(m_freqHz);
    m_bandLabel = o.value(QStringLiteral("bandLabel")).toString(m_bandLabel);
    m_modeLabel = o.value(QStringLiteral("modeLabel")).toString(m_modeLabel);
    return true;
}

} // namespace NereusSDR
