// src/gui/widgets/TriBtn.cpp
// Triangle step button — shared implementation.

#include "TriBtn.h"
#include "gui/StyleConstants.h"

#include <QPainter>
#include <QPaintEvent>

namespace NereusSDR {

TriBtn::TriBtn(Dir dir, QWidget* parent)
    : QPushButton(parent), m_dir(dir)
{
    setFlat(false);
    setFixedSize(22, 22);
    setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 3px; padding: 0; margin: 0;"
        "  min-width: 0; min-height: 0;"
        "}"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %3; }"
    ).arg(Style::kButtonBg, Style::kBorderSubtle, Style::kAccent));
}

void TriBtn::paintEvent(QPaintEvent* ev)
{
    QPushButton::paintEvent(ev);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // Black triangle when pressed (on accent bg), otherwise kTextPrimary.
    p.setBrush(isDown() ? QColor(0, 0, 0) : QColor(0xc8, 0xd8, 0xe8));
    p.setPen(Qt::NoPen);
    const int cx = width() / 2;
    const int cy = height() / 2;
    QPolygon tri;
    if (m_dir == Left) {
        tri << QPoint(cx - 5, cy)
            << QPoint(cx + 4, cy - 5)
            << QPoint(cx + 4, cy + 5);
    } else {
        tri << QPoint(cx + 5, cy)
            << QPoint(cx - 4, cy - 5)
            << QPoint(cx - 4, cy + 5);
    }
    p.drawPolygon(tri);
}

} // namespace NereusSDR
