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
    if (m_dragging) {
        jumpAt(int(event->position().y()));
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DsrHoursMap::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace NereusSDR
