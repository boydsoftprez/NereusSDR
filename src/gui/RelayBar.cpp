// src/gui/RelayBar.cpp

// =================================================================
// src/gui/RelayBar.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR, GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Extracted RelayBar widget from AetherSDR
//                 src/gui/HGauge.h (inner class) into standalone
//                 NereusSDR widget by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

#include "RelayBar.h"
#include "StyleConstants.h"

#include <QPainter>
#include <QWheelEvent>
#include <QFontMetrics>

namespace NereusSDR {

// From AetherSDR src/gui/HGauge.h:206-288 [@0cd4559]
RelayBar::RelayBar(const QString& label, QWidget* parent)
    : QWidget(parent), m_label(label)
{
    setFixedHeight(18);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip("Scroll to adjust relay position");
}

void RelayBar::setValue(int byteValue)
{
    if (m_value == byteValue) return;
    m_value = byteValue;
    update();
}

void RelayBar::setScrollEnabled(bool on)
{
    m_scrollEnabled = on;
    setCursor(on ? Qt::SizeVerCursor : Qt::ArrowCursor);
}

// From AetherSDR src/gui/HGauge.h:229-242 [@0cd4559]
void RelayBar::wheelEvent(QWheelEvent* e)
{
    if (!m_scrollEnabled) {
        QWidget::wheelEvent(e);
        return;
    }
    // Clamp to ±1: KDE/Cinnamon send 960 per notch (#504)
    m_angleAccum += e->angleDelta().y();
    constexpr int step = 120;
    int emitted = 0;
    while (m_angleAccum >= step && emitted == 0) {
        m_angleAccum -= step;
        emit relayAdjusted(+1);
        ++emitted;
    }
    while (m_angleAccum <= -step && emitted == 0) {
        m_angleAccum += step;
        emit relayAdjusted(-1);
        ++emitted;
    }
    if (emitted) m_angleAccum = 0;  // discard leftover inflation
    e->accept();
}

// From AetherSDR src/gui/HGauge.h:244-281 [@0cd4559]
void RelayBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int labelW = 24;
    const int valueW = 28;
    const int barX = labelW;
    const int barW = w - labelW - valueW - 4;
    const int barY = 3;
    const int barH = h - 6;

    // Label
    QFont f = font();
    f.setPixelSize(10);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(Style::kTextPrimary));
    const QFontMetrics fmLabel(f);
    p.drawText(0, barY + barH / 2 + fmLabel.ascent() / 2, m_label);

    // Bar background
    p.fillRect(barX, barY, barW, barH, QColor(Style::kPanelBg));
    p.setPen(QColor("#203040"));
    p.drawRect(barX, barY, barW - 1, barH - 1);

    // Filled portion
    float frac = qBound(0.0f, m_value / 255.0f, 1.0f);
    int fillW = static_cast<int>(frac * (barW - 2));
    if (fillW > 0)
        p.fillRect(barX + 1, barY + 1, fillW, barH - 2, QColor(Style::kAccent));

    // Value text
    p.setPen(QColor(Style::kTextPrimary));
    const QString valText = QString::number(m_value);
    const QFontMetrics fm(f);
    p.drawText(w - valueW, barY + barH / 2 + fm.ascent() / 2, valText);
}

} // namespace NereusSDR
