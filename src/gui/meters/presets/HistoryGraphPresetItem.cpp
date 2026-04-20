// =================================================================
// src/gui/meters/presets/HistoryGraphPresetItem.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR original — no Thetis upstream.
//
// no-port-check: NereusSDR-local preset. Collapsed from
// src/gui/meters/ItemGroup.cpp:1485-1520 (createHistoryPreset helper).
// Ships under the project's GPLv3 terms (root LICENSE); no per-file
// verbatim upstream header required because no Thetis source code was
// consulted in writing this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Original implementation for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted scaffolding via Claude
//                Opus 4.7. Rolling-buffer line graph + right-edge
//                readout painted directly via QPainter; replaces the
//                three-child SolidColourItem/HistoryGraphItem/TextItem
//                composition used in createHistoryPreset.
// =================================================================

#include "gui/meters/presets/HistoryGraphPresetItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>

namespace NereusSDR {

HistoryGraphPresetItem::HistoryGraphPresetItem(QObject* parent)
    : MeterItem(parent)
{
    setBindingId(MeterBinding::SignalAvg);
}

void HistoryGraphPresetItem::setCapacity(int n)
{
    m_capacity = (n > 1) ? n : 1;
    while (m_samples.size() > m_capacity) {
        m_samples.removeFirst();
    }
}

void HistoryGraphPresetItem::setValue(double v)
{
    MeterItem::setValue(v);
    m_samples.append(v);
    while (m_samples.size() > m_capacity) {
        m_samples.removeFirst();
    }
}

void HistoryGraphPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    p.save();
    p.fillRect(rect, m_backdropColor);

    // Top 80% = graph, bottom 20% = readout (NereusSDR-original
    // layout from createHistoryPreset).
    const int graphH   = rect.height() * 80 / 100;
    const int readoutH = rect.height() - graphH;
    const QRect graphRect(rect.x(), rect.y(), rect.width(), graphH);
    const QRect readoutRect(rect.x(), rect.y() + graphH, rect.width(),
                            readoutH);

    // Scale ticks along the right edge of the graph (4 horizontal
    // reference lines).
    p.setPen(QColor(0x2a, 0x38, 0x4a));
    for (int t = 1; t < 4; ++t) {
        const int y = graphRect.y() + graphRect.height() * t / 4;
        p.drawLine(graphRect.x(), y,
                   graphRect.x() + graphRect.width(), y);
    }

    // Line graph — map each sample to [0..1] of the graph height.
    // Default dBm range [-140, 0] for visual scale (real range
    // depends on bound meter; configurable via min/max if needed).
    if (!m_samples.isEmpty() && graphRect.width() > 1) {
        QPen pen(m_lineColor);
        pen.setWidthF(1.5);
        p.setPen(pen);
        const int N = m_samples.size();
        const double vMin = -140.0;
        const double vMax = 0.0;
        const double span = vMax - vMin;
        QPointF prev;
        for (int i = 0; i < N; ++i) {
            const double v = m_samples[i];
            const double t = (span > 0) ? (v - vMin) / span : 0.0;
            const double tClamped = qBound(0.0, t, 1.0);
            const int x = graphRect.x()
                        + static_cast<int>(static_cast<double>(i) / qMax(1, N - 1)
                                           * graphRect.width());
            const int y = graphRect.y() + graphRect.height()
                        - static_cast<int>(tClamped * graphRect.height());
            const QPointF pt(x, y);
            if (i > 0) {
                p.drawLine(prev, pt);
            }
            prev = pt;
        }
    }

    // Readout — bottom strip.
    if (m_showReadout) {
        p.setPen(m_readoutColor);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, readoutH * 0.6));
        p.setFont(f);
        const double latest = m_samples.isEmpty() ? -140.0 : m_samples.last();
        p.drawText(readoutRect,
                   Qt::AlignCenter,
                   QStringLiteral("%1 dBm").arg(latest, 0, 'f', 1));
    }
    p.restore();
}

QString HistoryGraphPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),        QStringLiteral("HistoryGraphPreset"));
    o.insert(QStringLiteral("x"),           static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),           static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),           static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),           static_cast<double>(m_h));
    o.insert(QStringLiteral("bindingId"),   bindingId());
    o.insert(QStringLiteral("capacity"),    m_capacity);
    o.insert(QStringLiteral("showReadout"), m_showReadout);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool HistoryGraphPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("HistoryGraphPreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    setBindingId(o.value(QStringLiteral("bindingId")).toInt(bindingId()));
    setCapacity(o.value(QStringLiteral("capacity")).toInt(m_capacity));
    m_showReadout = o.value(QStringLiteral("showReadout")).toBool(m_showReadout);
    return true;
}

} // namespace NereusSDR
