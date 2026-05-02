// src/gui/HGauge.cpp
#include "HGauge.h"
#include "StyleConstants.h"
#include <QPainter>
#include <QPaintEvent>

namespace NereusSDR {

HGauge::HGauge(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(30);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HGauge::setRange(double min, double max) { m_min = min; m_max = max; update(); }
void HGauge::setYellowStart(double val) { m_yellowStart = val; update(); }
void HGauge::setRedStart(double val) { m_redStart = val; update(); }
void HGauge::setReversed(bool rev) { m_reversed = rev; update(); }
void HGauge::setTitle(const QString& t) { m_title = t; update(); }
void HGauge::setUnit(const QString& u) { m_unit = u; update(); }
void HGauge::setValue(double val) { m_value = val; update(); }
void HGauge::setPeakValue(double val) { m_peak = val; update(); }
void HGauge::setTickLabels(const QStringList& labels) { m_tickLabels = labels; update(); }

void HGauge::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int barH = 12;
    const int barY = (h - barH) / 2;
    const int barX = 2;
    const int barW = w - 4;

    // Background
    p.setPen(QColor(Style::kBorderSubtle));
    p.setBrush(QColor(Style::kInsetBg));
    p.drawRoundedRect(barX, barY, barW, barH, 2, 2);

    if (m_max <= m_min) { return; }

    const double range = m_max - m_min;
    const double normalized = qBound(0.0, (m_value - m_min) / range, 1.0);

    if (m_reversed) {
        const int fillW = static_cast<int>(normalized * barW);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Style::kGaugeDanger));
        p.drawRoundedRect(barX + barW - fillW, barY + 1, fillW, barH - 2, 1, 1);
    } else {
        const int fillW = static_cast<int>(normalized * barW);
        if (fillW > 0) {
            const double yellowNorm = (m_yellowStart - m_min) / range;
            const double redNorm = (m_redStart - m_min) / range;
            const int yellowX = static_cast<int>(yellowNorm * barW);
            const int redX = static_cast<int>(redNorm * barW);

            int normalEnd = qMin(fillW, yellowX);
            if (normalEnd > 0) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(Style::kGaugeNormal));
                p.drawRoundedRect(barX + 1, barY + 1, normalEnd, barH - 2, 1, 1);
            }
            if (fillW > yellowX) {
                int warnEnd = qMin(fillW, redX) - yellowX;
                if (warnEnd > 0) {
                    p.setBrush(QColor(Style::kGaugeWarning));
                    p.drawRect(barX + 1 + yellowX, barY + 1, warnEnd, barH - 2);
                }
            }
            if (fillW > redX) {
                int dangerW = fillW - redX;
                p.setBrush(QColor(Style::kGaugeDanger));
                p.drawRect(barX + 1 + redX, barY + 1, dangerW, barH - 2);
            }
        }
    }

    // Peak hold marker
    if (m_peak > m_min) {
        const double peakNorm = qBound(0.0, (m_peak - m_min) / range, 1.0);
        const int peakX = barX + 1 + static_cast<int>(peakNorm * (barW - 2));
        p.setPen(QPen(QColor(Style::kGaugePeak), 1));
        p.drawLine(peakX, barY + 1, peakX, barY + barH - 2);
    }

    // Center title
    if (!m_title.isEmpty()) {
        p.setPen(QColor(Style::kTextSecondary));
        p.setFont(QFont(p.font().family(), 8, QFont::Bold));
        p.drawText(QRect(barX, barY, barW, barH), Qt::AlignCenter, m_title);
    }

    // Tick labels along bottom
    if (!m_tickLabels.isEmpty()) {
        p.setFont(QFont(p.font().family(), 7));
        const int n = m_tickLabels.size();
        for (int i = 0; i < n; ++i) {
            double frac = static_cast<double>(i) / (n - 1);
            int x = barX + static_cast<int>(frac * barW);
            QColor col(Style::kGaugeNormal);
            double val = m_min + frac * range;
            if (val >= m_redStart) { col = QColor(Style::kGaugeDanger); }
            else if (val >= m_yellowStart) { col = QColor(Style::kGaugeWarning); }
            p.setPen(col);
            p.drawText(QRect(x - 20, barY + barH + 1, 40, 10),
                       Qt::AlignHCenter | Qt::AlignTop, m_tickLabels[i]);
        }
    }
}

} // namespace NereusSDR
