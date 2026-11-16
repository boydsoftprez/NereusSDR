// =================================================================
// src/gui/widgets/VfoLevelBar.cpp  (NereusSDR)
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
//   2026-11-04 - setSourceTag: MAX or AV above the readout, in the tick
//                 strip (overlay design D41). NereusSDR-native. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-12 - JJ's option B: fit small workspace text to the approved
//                 11 logical-pixel floor. J.J. Boyd (KG4VCF), AI-assisted
//                 via OpenAI Codex. Classic geometry and colors retained.
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 LevelBar widget ported from AetherSDR
//                 `src/gui/VfoWidget.cpp:38-64`.
//   2026-09-22: No-reading display (R-R3-13) by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.  A level at or below -400 dBm, or a non-finite
//                 one, empties the bar and shows "-- dBm".
//                 NereusSDR-native; no upstream equivalent.
// =================================================================

#include "VfoLevelBar.h"
#include "VfoStyles.h"
#include "gui/styles/WorkspacePreview.h"
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <algorithm>
#include <cmath>
namespace NereusSDR {
// From AetherSDR src/gui/VfoWidget.cpp:38-64 — LevelBar port,
// extended with an S-unit tick strip above the bar (NereusSDR native).
VfoLevelBar::VfoLevelBar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}
void VfoLevelBar::setValue(float dbm) {
    // NereusSDR (R-R3-13): the -400 dBm sentinel or a non-finite level is
    // no reading, not a signal at the bar floor.
    const bool noReading = !std::isfinite(dbm) || dbm <= kNoReadingDbm;
    if (noReading) {
        if (m_noReading) { return; }
        m_noReading = true;
        m_value = kNoReadingDbm;
        update();
        return;
    }
    if (!m_noReading && dbm == m_value) { return; }
    m_noReading = false;
    m_value = dbm;
    update();
}
void VfoLevelBar::setSourceTag(const QString& tag) {
    if (tag == m_sourceTag) { return; }
    m_sourceTag = tag;
    update();
}
QString VfoLevelBar::readoutText() const {
    if (m_noReading) {
        return QStringLiteral("-- dBm");
    }
    return QString::number(static_cast<int>(std::round(m_value)))
        + QStringLiteral(" dBm");
}
double VfoLevelBar::fillFraction() const {
    if (m_noReading) { return 0.0; }
    const double v = std::clamp(static_cast<double>(m_value),
                                static_cast<double>(kFloorDbm),
                                static_cast<double>(kCeilingDbm));
    return (v - kFloorDbm) / (kCeilingDbm - kFloorDbm);
}
QSize VfoLevelBar::sizeHint() const {
    if (!WorkspacePreview::enabled()) { return {230, 26}; }
    QFont ticks = font();
    ticks.setPixelSize(WorkspacePreview::readableTextPixelSize(8));
    QFont readout = font();
    readout.setPixelSize(WorkspacePreview::readableTextPixelSize(10));
    readout.setBold(true);
    return {230, 4 + QFontMetrics(ticks, this).height() + 2
                    + std::max(14, QFontMetrics(readout, this).height())};
}
QSize VfoLevelBar::minimumSizeHint() const { return {200, sizeHint().height()}; }
namespace {
// 2026-05-12 bench fix (PR #238): push the tick strip down by
// kTopPad pixels so the "S1 3 5 7 9 +20 +40" labels never sit
// flush against the bottom of the frequency-display border
// above this widget.  The earlier QVBoxLayout addSpacing(4)
// helped, but the user reported the s-meter still visually
// overlapped the rounded freq rectangle.  Pushing the tick
// strip down inside this widget guarantees clearance
// regardless of how Qt resolves the layout's stretch policy
// after the RADE SNR row was added below.
constexpr int kTopPad = 4;
}
VfoLevelBar::Layout VfoLevelBar::barLayout() const {
    QFont tickFont = font();
    tickFont.setPixelSize(WorkspacePreview::readableTextPixelSize(8));
    QFont dbmFont = font();
    dbmFont.setPixelSize(WorkspacePreview::readableTextPixelSize(10));
    dbmFont.setBold(true);
    const QFontMetrics tickMetrics(tickFont, this);
    const QFontMetrics dbmMetrics(dbmFont, this);
    const int tickH = WorkspacePreview::enabled() ? tickMetrics.height() + 2 : 8;
    // ── dBm readout width reservation ──────────────────────────────────
    const int kDbmWidth = WorkspacePreview::enabled()
        ? std::max(52, dbmMetrics.horizontalAdvance(QStringLiteral("-130 dBm")) + 2) : 52;
    const int readoutGap = WorkspacePreview::enabled() ? 4 : 2;
    return {tickH, kDbmWidth, width() - kDbmWidth - readoutGap, kTopPad + tickH};
}
QRect VfoLevelBar::readoutRect() const {
    const Layout l = barLayout();
    return {l.barW - 1 + 4, l.barTop, l.dbmWidth, height() - l.barTop};
}
QRect VfoLevelBar::sourceTagRect() const {
    // Above the readout, on the tick labels' own line.
    const Layout l = barLayout();
    return {l.barW - 1 + 4, kTopPad, l.dbmWidth, l.tickH - 2};
}
void VfoLevelBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    QFont tickFont = p.font();
    tickFont.setPixelSize(WorkspacePreview::readableTextPixelSize(8));
    QFont dbmFont = p.font();
    dbmFont.setPixelSize(WorkspacePreview::readableTextPixelSize(10));
    dbmFont.setBold(true);
    const Layout layout = barLayout();
    const int tickH = layout.tickH;
    const int barW = layout.barW;

    // ── Tick strip above the bar ───────────────────────────────────────
    // Tick labels for S1, S3, S5, S7, S9, +20, +40
    static constexpr float kTickDbm[]    = {-121, -109, -97, -85, -73, -53, -33};
    static constexpr const char* kTickTxt[] = {"S1","3","5","7","9","+20","+40"};
    p.setPen(kLabelMuted);
    p.setFont(tickFont);
    for (int i = 0; i < 7; ++i) {
        double frac = (kTickDbm[i] - kFloorDbm) / (kCeilingDbm - kFloorDbm);
        int x = static_cast<int>(frac * (barW - 1));
        p.drawLine(x, kTopPad + tickH - 2, x, kTopPad + tickH);  // tick mark

        // 2026-05-12 bench fix (PR #238): per-tick alignment so the
        // leftmost label ("S1") doesn't get clipped at the bar's
        // left edge and the rightmost label ("+40") doesn't bleed
        // into the dBm readout's reserved zone on the right.
        // Centered placement worked for the middle ticks but the
        // edge ticks' centered box extended ±10 px past the bar
        // bounds, putting "+40" visually inside the "-65 dBm" text.
        //   i == 0 (leftmost "S1"):  left-align label at tick x.
        //   i >= 5 (+20 / +40):      right-align label ending at x.
        //   otherwise:               centered around tick x.
        constexpr int kEdgeLabelW = 24;
        constexpr int kMidLabelW  = 20;
        QRect labelRect;
        int alignment;
        if (i == 0) {
            labelRect = QRect(x, kTopPad, kEdgeLabelW, tickH - 2);
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
        } else if (i >= 5) {
            labelRect = QRect(x - kEdgeLabelW, kTopPad, kEdgeLabelW, tickH - 2);
            alignment = Qt::AlignRight | Qt::AlignVCenter;
        } else {
            labelRect = QRect(x - kMidLabelW / 2, kTopPad, kMidLabelW, tickH - 2);
            alignment = Qt::AlignCenter;
        }
        p.drawText(labelRect, alignment, QString::fromLatin1(kTickTxt[i]));
    }
    const int barTop = layout.barTop;
    const QRect barRect(0, barTop, barW, height() - barTop);

    // ── Bar itself (ported from AetherSDR LevelBar::paintEvent) ────────
    p.fillRect(barRect, QColor(0x10, 0x10, 0x1c));
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawRect(barRect.adjusted(0, 0, -1, -1));
    const int innerW = barRect.width() - 2;
    const int fillW = static_cast<int>(fillFraction() * innerW);
    if (fillW > 0) {
        const int x0 = barRect.x() + 1;
        const int y0 = barRect.y() + 1;
        const int h  = barRect.height() - 2;
        // Continuous gradient: cyan → green across the S9 boundary
        const double s9Frac = (kS9Dbm - kFloorDbm) / (kCeilingDbm - kFloorDbm);
        QLinearGradient grad(x0, 0, x0 + innerW, 0);
        grad.setColorAt(0.0, kMeterCyan);
        grad.setColorAt(s9Frac, kMeterCyan);
        grad.setColorAt(std::min(s9Frac + 0.15, 1.0), kMeterGreen);
        grad.setColorAt(1.0, kMeterGreen);
        p.fillRect(x0, y0, fillW, h, QBrush(grad));
    }

    // ── Which reading it is, above the readout (overlay design D41) ───
    if (!m_sourceTag.isEmpty()) {
        QFont tagFont = tickFont;
        tagFont.setBold(true);
        p.setFont(tagFont);
        p.setPen(QColor(0xdc, 0xdc, 0xe1));
        // Clipped because a glyph can draw outside its box on some fonts,
        // and the tag must change nothing beyond its own box.
        const QRect tagRect = sourceTagRect();
        p.setClipRect(tagRect);
        p.drawText(tagRect, Qt::AlignVCenter | Qt::AlignLeft, m_sourceTag);
        p.setClipping(false);
    }

    // ── dBm text to the right of the bar ──────────────────────────────
    const QRect dbmRect = readoutRect();
    p.setPen(isAboveS9() ? kMeterGreen : kMeterCyan);
    p.setFont(dbmFont);
    p.drawText(dbmRect, Qt::AlignVCenter | Qt::AlignLeft, readoutText());
}
}
