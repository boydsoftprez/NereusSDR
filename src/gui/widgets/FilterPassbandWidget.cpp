// src/gui/widgets/FilterPassbandWidget.cpp
// Ported from AetherSDR src/gui/FilterPassbandWidget.cpp

// =================================================================
// src/gui/widgets/FilterPassbandWidget.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Ported from AetherSDR `src/gui/FilterPassbandWidget.cpp`
//                 (filter low/high drag + shift-band visualisation).
// =================================================================

#include "FilterPassbandWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

FilterPassbandWidget::FilterPassbandWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setCursor(Qt::SizeAllCursor);
}

void FilterPassbandWidget::setFilter(int lo, int hi)
{
    if (lo == m_lo && hi == m_hi) { return; }
    m_lo = lo;
    m_hi = hi;
    update();
}

void FilterPassbandWidget::setMode(const QString& mode)
{
    if (mode == m_mode) { return; }
    m_mode = mode;
    update();
}

// ─── Paint ──────────────────────────────────────────────────────────────────
// From AetherSDR FilterPassbandWidget::paintEvent()

void FilterPassbandWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // Background — from AetherSDR
    p.fillRect(rect(), QColor(0x0a, 0x0a, 0x18));

    // Border
    p.setPen(QColor(0x20, 0x30, 0x40));
    p.drawRect(0, 0, w - 1, h - 1);

    // ── Static trapezoid shape ──────────────────────────────────────────
    // Fixed geometry — shape doesn't change with filter width
    // From AetherSDR FilterPassbandWidget.cpp lines 53-64
    const int margin = 16;
    const int topY = 8;
    const int botY = h - 16;  // room for labels
    const int loX = margin;
    const int hiX = w - margin;
    const int kSkirt = (hiX - loX) / 7;

    // Filter shape: left skirt, flat top, right skirt (no bottom, no fill)
    // From AetherSDR: cyan (#00b4d8), 1.5px
    p.setPen(QPen(QColor(0x00, 0xb4, 0xd8), 1.5));
    p.drawLine(loX, botY, loX + kSkirt, topY);          // left skirt
    p.drawLine(loX + kSkirt, topY, hiX - kSkirt, topY);  // flat top
    p.drawLine(hiX - kSkirt, topY, hiX, botY);           // right skirt

    // Dashed vertical lines at filter edges (3px inside the skirt tops)
    p.setPen(QPen(QColor(0x00, 0xb4, 0xd8, 120), 1, Qt::DashLine));
    p.drawLine(loX + kSkirt + 3, 2, loX + kSkirt + 3, h - 2);
    p.drawLine(hiX - kSkirt - 3, 2, hiX - kSkirt - 3, h - 2);

    // ── Labels ──────────────────────────────────────────────────────────
    // From AetherSDR FilterPassbandWidget.cpp lines 72-99
    QFont font = p.font();
    font.setPixelSize(10);
    p.setFont(font);
    const QFontMetrics fm(font);

    // Bandwidth label (centered at bottom)
    const int bw = std::abs(m_hi - m_lo);
    const QString bwText = bw >= 1000
        ? QStringLiteral("%1.%2K").arg(bw / 1000).arg((bw % 1000) / 100)
        : QString::number(bw);
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    p.drawText((loX + hiX) / 2 - fm.horizontalAdvance(bwText) / 2, botY + 12, bwText);

    // Passband center offset (below top line)
    const int center = (m_lo + m_hi) / 2;
    const QString centerText = std::abs(center) >= 1000
        ? QStringLiteral("%1.%2K").arg(std::abs(center) / 1000).arg((std::abs(center) % 1000) / 100)
        : QString::number(std::abs(center));
    p.setPen(QColor(0x90, 0xa0, 0xb0));
    p.drawText((loX + hiX) / 2 - fm.horizontalAdvance(centerText) / 2, topY + 12, centerText);

    // Lo label (centered on left slant bottom point)
    const QString loText = QString::number(std::abs(m_lo));
    p.setPen(QColor(0x80, 0x90, 0xa0));
    p.drawText(loX - fm.horizontalAdvance(loText) / 2, botY + 12, loText);

    // Hi label (centered on right slant bottom point)
    const QString hiText = QString::number(std::abs(m_hi));
    p.drawText(hiX - fm.horizontalAdvance(hiText) / 2, botY + 12, hiText);
}

// ─── Mouse interaction ──────────────────────────────────────────────────────
// From AetherSDR FilterPassbandWidget.cpp lines 104-202

void FilterPassbandWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) { return; }
    m_dragStartPos = ev->pos();
    m_dragStartLo = m_lo;
    m_dragStartHi = m_hi;

    // Three zones: left of lo line = drag low edge, right of hi line = drag high edge,
    // center = shift. From AetherSDR FilterPassbandWidget.cpp lines 112-125
    constexpr int margin = 16;
    const int kSkirt = (width() - 32) / 7;
    const int loLineX = margin + kSkirt + 3;
    const int hiLineX = width() - margin - kSkirt - 3;

    if (ev->pos().x() <= loLineX) {
        m_dragMode = DragLo;
        setCursor(Qt::SizeHorCursor);
    } else if (ev->pos().x() >= hiLineX) {
        m_dragMode = DragHi;
        setCursor(Qt::SizeHorCursor);
    } else {
        m_dragMode = DragShift;
        setCursor(Qt::SizeAllCursor);
    }
}

void FilterPassbandWidget::mouseMoveEvent(QMouseEvent* ev)
{
    // Hover cursor feedback — three zones: left edge, center, right edge.
    // Only call setCursor when the shape actually changes to avoid
    // excessive CGImageCreate calls on macOS (EXC_BREAKPOINT in Qt cursor code).
    // From AetherSDR FilterPassbandWidget.cpp lines 133-142
    if (m_dragMode == DragNone) {
        constexpr int margin = 16;
        constexpr int kSkirt = 16;
        const int loLineX = margin + kSkirt + 8;
        const int hiLineX = width() - margin - kSkirt - 8;
        const int x = ev->pos().x();
        const Qt::CursorShape wanted = (x <= loLineX || x >= hiLineX)
            ? Qt::SizeHorCursor : Qt::SizeAllCursor;
        if (cursor().shape() != wanted) {
            setCursor(wanted);
        }
        return;
    }

    const int dx = ev->pos().x() - m_dragStartPos.x();
    const int dy = ev->pos().y() - m_dragStartPos.y();

    // From AetherSDR FilterPassbandWidget.cpp lines 148-151
    const int usableW = width() - 32;
    const int usableH = height();
    const double hzPerPxH = 6000.0 / std::max(usableW, 1);
    const double hzPerPxV = 4000.0 / std::max(usableH, 1);

    int newLo = m_dragStartLo;
    int newHi = m_dragStartHi;

    // From AetherSDR FilterPassbandWidget.cpp lines 156-172
    if (m_dragMode == DragShift) {
        // Horizontal: shift passband, vertical: symmetric width
        int shiftHz = static_cast<int>(dx * hzPerPxH);
        int bwChange = static_cast<int>(-dy * hzPerPxV);
        shiftHz = (shiftHz / 50) * 50;
        bwChange = (bwChange / 50) * 50;
        newLo = m_dragStartLo + shiftHz - bwChange / 2;
        newHi = m_dragStartHi + shiftHz + bwChange / 2;
    } else if (m_dragMode == DragLo) {
        int deltaHz = static_cast<int>(dx * hzPerPxH);
        deltaHz = (deltaHz / 50) * 50;
        newLo = m_dragStartLo + deltaHz;
    } else if (m_dragMode == DragHi) {
        int deltaHz = static_cast<int>(dx * hzPerPxH);
        deltaHz = (deltaHz / 50) * 50;
        newHi = m_dragStartHi + deltaHz;
    }

    // Enforce minimum bandwidth
    // From AetherSDR FilterPassbandWidget.cpp lines 175-185
    if (newHi - newLo < kMinBw) {
        if (m_dragMode == DragLo) {
            newLo = newHi - kMinBw;
        } else if (m_dragMode == DragHi) {
            newHi = newLo + kMinBw;
        } else {
            const int mid = (newLo + newHi) / 2;
            newLo = mid - kMinBw / 2;
            newHi = mid + kMinBw / 2;
        }
    }

    // Snap to 50 Hz grid
    // From AetherSDR FilterPassbandWidget.cpp lines 188-189
    newLo = (newLo / 50) * 50;
    newHi = (newHi / 50) * 50;

    if (newLo != m_lo || newHi != m_hi) {
        m_lo = newLo;
        m_hi = newHi;
        update();
        emit filterChanged(m_lo, m_hi);
    }
}

void FilterPassbandWidget::mouseReleaseEvent(QMouseEvent*)
{
    m_dragMode = DragNone;
}

} // namespace NereusSDR
