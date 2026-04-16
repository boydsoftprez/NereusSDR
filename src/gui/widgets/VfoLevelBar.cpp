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
        p.drawLine(x, tickH - 2, x, tickH);  // tick mark
        p.drawText(QRect(x - 10, 0, 20, tickH - 2),
                   Qt::AlignCenter, QString::fromLatin1(kTickTxt[i]));
    }
    const QRect barRect(0, tickH, width() - kDbmWidth - 2, height() - tickH);

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
