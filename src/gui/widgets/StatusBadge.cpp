// src/gui/widgets/StatusBadge.cpp
#include "StatusBadge.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>

namespace NereusSDR {

StatusBadge::StatusBadge(QWidget* parent) : QWidget(parent)
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(6, 1, 6, 1);
    hbox->setSpacing(3);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setObjectName(QStringLiteral("StatusBadge_Icon"));
    // Minimum / Fixed: don't shrink below the icon glyph's natural width.
    m_iconLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    hbox->addWidget(m_iconLabel);

    m_textLabel = new QLabel(this);
    m_textLabel->setObjectName(QStringLiteral("StatusBadge_Text"));
    // Critical: default Preferred lets QHBoxLayout clip "USB" → "U" when
    // the parent dashboard is constrained. Minimum / Fixed locks the label
    // at its natural text width so the full label always shows.
    m_textLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    hbox->addWidget(m_textLabel);

    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);

    // Size policy: claim sizeHint() worth of horizontal space so a parent
    // QHBoxLayout under pressure can't squeeze the badge below its content.
    // Without this, multi-char labels like "USB" / "2.4k" / "AGC-S" clip
    // to single chars when the dashboard is constrained on the status bar.
    // Vertical: Fixed so the badge stays at its natural 18 px height.
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    applyStyle();
}

void StatusBadge::setIcon(const QString& icon)
{
    if (m_icon == icon) { return; }
    m_icon = icon;
    m_iconLabel->setText(icon);
    m_iconLabel->updateGeometry();
    updateGeometry();
}

void StatusBadge::setLabel(const QString& label)
{
    if (m_label == label) { return; }
    m_label = label;
    m_textLabel->setText(label);
    // Compute minimum width from font metrics rather than sizeHint().
    // sizeHint() can lazy-evaluate and return stale values; fontMetrics
    // gives the true pixel width for the new text immediately.
    // Padding budget: 6 left + 6 right margin + 3 spacing if an icon is
    // also present + the icon's own width.
    const QFontMetrics fm(m_textLabel->font());
    const int textWidth  = fm.horizontalAdvance(label);
    const int iconWidth  = m_iconLabel->text().isEmpty() ? 0
                         : (fm.horizontalAdvance(m_iconLabel->text()) + 3);
    setMinimumWidth(textWidth + iconWidth + 14);   // 14 = layout margins+slack
    m_textLabel->updateGeometry();
    updateGeometry();
}

void StatusBadge::setVariant(Variant v)
{
    if (m_variant == v) { return; }
    m_variant = v;
    applyStyle();
}

void StatusBadge::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    } else if (event->button() == Qt::RightButton) {
        emit rightClicked(event->globalPosition().toPoint());
    }
    QWidget::mousePressEvent(event);
}

void StatusBadge::applyStyle()
{
    QString fg, bg;
    switch (m_variant) {
        case Variant::Info:
            fg = QStringLiteral("#5fa8ff");
            bg = QStringLiteral("rgba(95,168,255,26)");   // 0.10 alpha
            break;
        case Variant::On:
            fg = QStringLiteral("#5fff8a");
            bg = QStringLiteral("rgba(95,255,138,26)");
            break;
        case Variant::Off:
            fg = QStringLiteral("#404858");
            bg = QStringLiteral("rgba(64,72,88,46)");      // 0.18 alpha
            break;
        case Variant::Warn:
            fg = QStringLiteral("#ffd700");
            bg = QStringLiteral("rgba(255,215,0,30)");
            break;
        case Variant::Tx:
            fg = QStringLiteral("#ff6060");
            bg = QStringLiteral("rgba(255,96,96,51)");     // 0.20 alpha
            break;
    }

    setStyleSheet(QStringLiteral(
        "NereusSDR--StatusBadge { background: %1; border-radius: 3px; }"
        "QLabel { color: %2; font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 10px; font-weight: 600; line-height: 1.4; }"
    ).arg(bg, fg));
}

} // namespace NereusSDR
