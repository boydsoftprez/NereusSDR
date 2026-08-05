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
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 LevelBar widget ported from AetherSDR
//                 `src/gui/VfoWidget.cpp:38-64`.
// =================================================================

#include "VfoLevelBar.h"
#include "VfoStyles.h"
#include <QLinearGradient>
#include <QPainter>
#include <algorithm>
namespace NereusSDR {
// From AetherSDR src/gui/VfoWidget.cpp:38-64 — LevelBar port,
// extended with an S-unit tick strip above the bar (NereusSDR native).
VfoLevelBar::VfoLevelBar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}
void VfoLevelBar::setValue(float dbm) {
    if (dbm == m_value) { return; }
    m_value = dbm;
    update();
}
double VfoLevelBar::fillFraction() const {
    const double v = std::clamp(static_cast<double>(m_value),
                                static_cast<double>(kFloorDbm),
                                static_cast<double>(kCeilingDbm));
    return (v - kFloorDbm) / (kCeilingDbm - kFloorDbm);
}
void VfoLevelBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
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
    const int tickH = 8;

    // ── dBm readout width reservation ──────────────────────────────────
    static constexpr int kDbmWidth = 52;
    const int barW = width() - kDbmWidth - 2;

    // ── Tick strip above the bar ───────────────────────────────────────
    // Tick labels for S1, S3, S5, S7, S9, +20, +40
    static constexpr float kTickDbm[]    = {-121, -109, -97, -85, -73, -53, -33};
    static constexpr const char* kTickTxt[] = {"S1","3","5","7","9","+20","+40"};
    p.setPen(kLabelMuted);
    QFont f = p.font(); f.setPixelSize(8); p.setFont(f);
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
    const int barTop = kTopPad + tickH;
    const QRect barRect(0, barTop, width() - kDbmWidth - 2, height() - barTop);

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

    // ── dBm text to the right of the bar ──────────────────────────────
    const QRect dbmRect(barRect.right() + 4, barRect.y(),
                        kDbmWidth, barRect.height());
    p.setPen(isAboveS9() ? kMeterGreen : kMeterCyan);
    QFont dbmFont = p.font();
    dbmFont.setPixelSize(10);
    dbmFont.setBold(true);
    p.setFont(dbmFont);
    p.drawText(dbmRect, Qt::AlignVCenter | Qt::AlignLeft,
               QString::number(static_cast<int>(std::round(m_value)))
               + QStringLiteral(" dBm"));
}
}
