// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/DsrHoursMap.cpp  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 24.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/recordings/DsrHoursMap.h"

#include "gui/styles/WorkspacePreview.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace NereusSDR {

namespace {

constexpr qint64 kHourMs = 3600LL * 1000LL;
constexpr qint64 kHalfHourMs = kHourMs / 2;
// The mockup's heat strip, hour ticks and labels (hours.html drawMap).
constexpr int kHeatLeft = 2;
constexpr int kHeatWidth = 12;
constexpr int kTickLeft = 14;
constexpr int kHourTickRight = 20;
constexpr int kHalfTickRight = 17;
constexpr int kLabelLeft = 20;
constexpr int kMinLabelPx = 6;
constexpr double kOwnBracketWidth = 1.5;
constexpr double kDisabledOpacity = 0.35;
// The bracket (hours.html #bracket): 2 px #ffd25a edge, its inside at 22 %,
// 2 px beyond each end, at least 6 px tall; the newest it may reach is a
// second before now.
constexpr int kBracketEdgePx = 2;
constexpr int kBracketMinPx = 6;
constexpr qint64 kBracketNewestGapMs = 1000;

} // namespace

DsrHoursMap::DsrHoursMap(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("DsrHoursMap"));
    setFixedWidth(kWidth);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setUnavailableReason(QString());
}

void DsrHoursMap::setHours(const QList<DsrHour>& hours)
{
    m_hours = hours;
    update();
}

void DsrHoursMap::setRange(qint64 oldestUtcMs, qint64 newestUtcMs)
{
    if (oldestUtcMs == m_oldestMs && newestUtcMs == m_newestMs) {
        return;
    }
    m_oldestMs = oldestUtcMs;
    m_newestMs = newestUtcMs;
    update();
}

void DsrHoursMap::setOwnSpanMs(qint64 ms)
{
    if (ms != m_ownSpanMs) {
        m_ownSpanMs = ms;
        update();
    }
}

void DsrHoursMap::setPosition(qint64 utcMs)
{
    if (utcMs != m_positionMs) {
        m_positionMs = utcMs;
        update();
    }
}

void DsrHoursMap::setUnavailableReason(const QString& reason)
{
    m_reason = reason;
    setEnabled(reason.isEmpty());
    setToolTip(reason.isEmpty()
                   ? QStringLiteral("Every kept hour, brighter where the band was busy. Click or drag to jump.")
                   : reason);
    update();
}

qint64 DsrHoursMap::utcMsAtY(int y) const
{
    const qint64 span = m_newestMs - m_oldestMs;
    if (span <= 0 || height() <= 0) {
        return m_newestMs;
    }
    const double f = 1.0 - std::clamp(double(y) / double(height()), 0.0, 1.0);
    return m_oldestMs + qint64(f * double(span));
}

int DsrHoursMap::yForUtcMs(qint64 utcMs) const
{
    const qint64 span = m_newestMs - m_oldestMs;
    if (span <= 0) {
        return 0;
    }
    const double f = double(utcMs - m_oldestMs) / double(span);
    return int(std::lround((1.0 - f) * double(height())));
}

QColor DsrHoursMap::heat(double activity)
{
    // The mockup's ramp: dark blue for a quiet band to orange for a busy one.
    static constexpr std::array<std::array<int, 3>, 4> kStops{{{15, 28, 48}, {42, 93, 158}, {216, 176, 74},
                                                                {255, 128, 64}}};
    const double p = std::clamp(activity, 0.0, 1.0) * 3.0;
    const int i = std::min(2, int(p));
    const double f = p - double(i);
    const auto& a = kStops[size_t(i)];
    const auto& b = kStops[size_t(i) + 1];
    return QColor(int(std::lround(a[0] + (b[0] - a[0]) * f)), int(std::lround(a[1] + (b[1] - a[1]) * f)),
                  int(std::lround(a[2] + (b[2] - a[2]) * f)));
}

void DsrHoursMap::setBracket(qint64 fromUtcMs, qint64 toUtcMs)
{
    const qint64 newest = m_newestMs - kBracketNewestGapMs;
    qint64 len = std::max(kMinBracketMs, toUtcMs - fromUtcMs);
    len = std::min(len, std::max<qint64>(kMinBracketMs, newest - m_oldestMs));
    qint64 to = std::min(newest, std::max(toUtcMs, m_oldestMs + len));
    qint64 from = std::max(m_oldestMs, to - len);
    if (from >= to) {
        from = to - kMinBracketMs;
    }
    m_hasBracket = true;
    m_bracketFromMs = from;
    m_bracketToMs = to;
    update();
    emit bracketChanged(from, to);
}

void DsrHoursMap::clearBracket()
{
    if (!m_hasBracket) {
        return;
    }
    m_hasBracket = false;
    m_bracketDrag = BracketDrag::None;
    update();
}

QRect DsrHoursMap::bracketRect() const
{
    if (!m_hasBracket || m_newestMs <= m_oldestMs) {
        return {};
    }
    const int yTop = yForUtcMs(m_bracketToMs);
    const int yBottom = yForUtcMs(m_bracketFromMs);
    return QRect(0, yTop - kBracketEdgePx, width(), std::max(kBracketMinPx, yBottom - yTop + 2 * kBracketEdgePx));
}

void DsrHoursMap::dragBracketTo(int y)
{
    const qint64 dt = utcMsAtY(y) - utcMsAtY(m_bracketDragY);
    const qint64 newest = m_newestMs - kBracketNewestGapMs;
    qint64 from = m_bracketFromMs;
    qint64 to = m_bracketToMs;
    if (m_bracketDrag == BracketDrag::From) {
        // hours.html: the order of its max and min, so a range that shrank
        // meanwhile never inverts the bounds.
        from = std::max(m_oldestMs, std::min(to - kMinBracketMs, m_bracketDragFromMs + dt));
    } else if (m_bracketDrag == BracketDrag::To) {
        to = std::min(newest, std::max(from + kMinBracketMs, m_bracketDragToMs + dt));
    } else {
        const qint64 len = m_bracketDragToMs - m_bracketDragFromMs;
        from = std::max(m_oldestMs, std::min(newest - len, m_bracketDragFromMs + dt));
        to = from + len;
    }
    if (from != m_bracketFromMs || to != m_bracketToMs) {
        m_bracketFromMs = from;
        m_bracketToMs = to;
        update();
        emit bracketChanged(from, to);
    }
}

void DsrHoursMap::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    if (!m_reason.isEmpty()) {
        p.setOpacity(kDisabledOpacity);
    }
    const int w = width();
    const int h = height();
    p.fillRect(rect(), QColor(8, 8, 18, 240));
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawLine(0, 0, 0, h);
    if (m_newestMs > m_oldestMs) {
        // Heat, newest at the top, one band per kept hour.
        for (const DsrHour& hour : std::as_const(m_hours)) {
            const qint64 startMs = hour.startUtcNs / 1000000LL;
            const int yTop = std::max(0, yForUtcMs(std::min(startMs + kHourMs, m_newestMs)));
            const int yBottom = std::min(h, yForUtcMs(std::max(startMs, m_oldestMs)));
            if (yBottom > yTop) {
                p.fillRect(QRect(kHeatLeft, yTop, kHeatWidth, yBottom - yTop), heat(hour.activity));
            }
        }
        // This window's own minutes: no wait for the Core to draw them.
        if (m_ownSpanMs > 0) {
            const int yOwn = std::min(h, yForUtcMs(m_newestMs - m_ownSpanMs));
            p.fillRect(QRectF(0.0, 0.0, kOwnBracketWidth, double(yOwn)), QColor(120, 160, 190, 140));
        }
        // Hour marks, a short tick on each half hour.
        QFont f = WorkspacePreview::readableFont(font(), this);
        f.setPixelSize(9);
        f.setWeight(QFont::DemiBold);
        f = WorkspacePreview::readableFont(f, this);
        // "00:00" fits between the ticks and the map's right edge.
        const int labelRoom = w - kLabelLeft - 1;
        while (f.pixelSize() > kMinLabelPx
               && QFontMetrics(f, this).horizontalAdvance(QStringLiteral("00:00")) > labelRoom) {
            f.setPixelSize(f.pixelSize() - 1);
        }
        p.setFont(f);
        const qint64 firstMark = ((m_oldestMs + kHalfHourMs - 1) / kHalfHourMs) * kHalfHourMs;
        for (qint64 t = firstMark; t < m_newestMs; t += kHalfHourMs) {
            const int y = yForUtcMs(t);
            const bool full = t % kHourMs == 0;
            p.setPen(QColor(0x50, 0x70, 0x80));
            p.drawLine(kTickLeft, y, full ? kHourTickRight : kHalfTickRight, y);
            if (full) {
                p.setPen(QColor(0x80, 0xa0, 0xb0));
                const QString label = QDateTime::fromMSecsSinceEpoch(t, QTimeZone::utc())
                                          .toString(QStringLiteral("HH:mm"));
                p.drawText(kLabelLeft, y + 3, label);
            }
        }
        // Where the waterfall's top row is.
        if (m_positionMs > 0) {
            const int py = std::clamp(yForUtcMs(m_positionMs), 0, h);
            p.fillRect(QRect(0, py - 1, w, 2), Qt::white);
            QPainterPath tri;
            tri.moveTo(w, py - 5);
            tri.lineTo(w - 6, py);
            tri.lineTo(w, py + 5);
            tri.closeSubpath();
            p.fillPath(tri, Qt::white);
        }
        // The stretch to save, above everything (hours.html #bracket).
        if (m_hasBracket) {
            const QRect r = bracketRect();
            p.setPen(QPen(QColor(0xff, 0xd2, 0x5a), kBracketEdgePx));
            p.setBrush(QColor(255, 210, 90, 56));
            p.drawRoundedRect(QRectF(r).adjusted(1.0, 1.0, -1.0, -1.0), 2.0, 2.0);
        }
    }
}

void DsrHoursMap::jumpAt(int y)
{
    if (m_newestMs > m_oldestMs) {
        emit jumpRequested(utcMsAtY(y));
    }
}

void DsrHoursMap::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_reason.isEmpty() && m_hasBracket) {
        // An edge moves that end, the middle moves it whole.
        const QRect r = bracketRect();
        const int y = int(event->position().y());
        BracketDrag part = BracketDrag::None;
        if (std::abs(y - (r.top() + kBracketEdgePx)) <= kBracketGrabPx) {
            part = BracketDrag::To;
        } else if (std::abs(y - (r.bottom() - kBracketEdgePx)) <= kBracketGrabPx) {
            part = BracketDrag::From;
        } else if (r.contains(int(event->position().x()), y)) {
            part = BracketDrag::Whole;
        }
        if (part != BracketDrag::None) {
            m_bracketDrag = part;
            m_bracketDragY = y;
            m_bracketDragFromMs = m_bracketFromMs;
            m_bracketDragToMs = m_bracketToMs;
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton && m_reason.isEmpty()) {
        m_dragging = true;
        jumpAt(int(event->position().y()));
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DsrHoursMap::mouseMoveEvent(QMouseEvent* event)
{
    if (m_bracketDrag != BracketDrag::None) {
        dragBracketTo(int(event->position().y()));
        event->accept();
        return;
    }
    if (m_dragging) {
        jumpAt(int(event->position().y()));
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DsrHoursMap::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_bracketDrag != BracketDrag::None) {
        m_bracketDrag = BracketDrag::None;
        event->accept();
        return;
    }
    if (m_dragging) {
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace NereusSDR
