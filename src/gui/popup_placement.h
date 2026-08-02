// no-port-check: pure geometry helper extracted for unit-testability
#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

namespace NereusSDR::PopupPlacement {

// Clamp a popup's desired top-left so the whole popup lands inside `available`.
//
// Qt constrains a QMenu to the screen on its own, but NOT a plain QWidget
// carrying Qt::Popup -- which is what SpectrumOverlayMenu (the right-click
// waterfall / spectrum panel) is. Measured on Qt 6.11 / macOS 26: a 250x350
// Qt::Popup asked for y=869 on a screen whose available rect is
// QRect(0,33 1512x897) is placed at exactly y=869, so 289 px hang below the
// visible area. A QMenu given the same point is moved up to y=701.
//
// That is the whole of the "second RX" half of the 2026-07-28 bench report:
// right-clicking the LOWER pan of a 2v layout -- and especially right-clicking
// its waterfall, which is what an operator does when they want the waterfall
// controls -- opened the panel below the bottom of the screen.
//
// Slides rather than flips: the panel stays adjacent to the cursor, which is
// where the operator is looking. A popup taller or wider than the screen
// cannot fit at all, so its top-left is pinned to the available rect's
// top-left -- losing the LAST controls beats losing the title and the first
// ones. An invalid (null) `available` returns the request untouched rather
// than inventing a position.
QPoint clampToAvailable(const QPoint& desiredTopLeft, const QSize& popupSize,
                        const QRect& available);

} // namespace NereusSDR::PopupPlacement
