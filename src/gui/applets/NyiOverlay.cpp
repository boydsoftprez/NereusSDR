#include "NyiOverlay.h"

namespace NereusSDR {

NyiOverlay::NyiOverlay(const QString& phaseHint, QWidget* parent)
    : QLabel(QStringLiteral("NYI"), parent)
{
    Q_UNUSED(phaseHint);  // No phase is named to the operator.
    setStyleSheet(QStringLiteral(
        "QLabel { background: #3a2a00; color: #ffb800;"
        " border: 1px solid #604000; border-radius: 2px;"
        " padding: 0px 3px; font-size: 8px; font-weight: bold; }"));
    setToolTip(QStringLiteral("This control is not built."));
    setFixedSize(fontMetrics().horizontalAdvance(QStringLiteral("NYI")) + 8, 14);
    raise();
}

void NyiOverlay::attachTo(QWidget* target)
{
    if (!target) { return; }
    setParent(target->parentWidget());
    const QPoint topRight = target->geometry().topRight();
    move(topRight.x() - width() + 2, topRight.y() - 2);
    show();
    raise();
}

NyiOverlay* NyiOverlay::markNyi(QWidget* target, const QString& phaseHint)
{
    Q_UNUSED(phaseHint);  // No phase is named to the operator.
    if (!target) { return nullptr; }
    target->setEnabled(false);
    target->setToolTip(QStringLiteral("This control is not built."));
    return nullptr;
}

} // namespace NereusSDR
