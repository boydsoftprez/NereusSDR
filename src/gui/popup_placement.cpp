// no-port-check: pure geometry helper extracted for unit-testability
#include "gui/popup_placement.h"

#include <algorithm>

namespace NereusSDR::PopupPlacement {

QPoint clampToAvailable(const QPoint& desiredTopLeft, const QSize& popupSize,
                        const QRect& available)
{
    if (!available.isValid()) {
        return desiredTopLeft;
    }

    // right()/bottom() are the last pixel INSIDE the rect, so the last
    // top-left that still fits a w-wide popup is right() - w + 1.
    const int maxX = available.right()  - popupSize.width()  + 1;
    const int maxY = available.bottom() - popupSize.height() + 1;

    // std::clamp asserts when lo > hi, which is exactly the "popup bigger than
    // the screen" case. Pin to the top-left there instead.
    const int x = (maxX < available.left())
                      ? available.left()
                      : std::clamp(desiredTopLeft.x(), available.left(), maxX);
    const int y = (maxY < available.top())
                      ? available.top()
                      : std::clamp(desiredTopLeft.y(), available.top(), maxY);

    return QPoint(x, y);
}

} // namespace NereusSDR::PopupPlacement
