// src/gui/widgets/OverflowChip.cpp
#include "OverflowChip.h"

#include <QHBoxLayout>
#include <QLabel>

namespace NereusSDR {

OverflowChip::OverflowChip(QWidget* parent) : QWidget(parent)
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(6, 1, 6, 1);
    hbox->setSpacing(0);

    m_glyph = new QLabel(QStringLiteral("…"), this);
    m_glyph->setObjectName(QStringLiteral("OverflowChip_Glyph"));
    m_glyph->setAlignment(Qt::AlignCenter);
    hbox->addWidget(m_glyph);

    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setCursor(Qt::WhatsThisCursor);   // hint: hover-for-detail

    setStyleSheet(QStringLiteral(
        "NereusSDR--OverflowChip {"
        " background: rgba(64,72,88,46);"
        " border-radius: 3px;"
        "}"
        "QLabel#OverflowChip_Glyph {"
        " color: #8aa8c0;"
        " font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 14px; font-weight: 700;"
        " background: transparent; border: none;"
        " padding: 0 2px;"
        "}"
    ));

    setVisible(false);
}

void OverflowChip::setDroppedItems(const QStringList& items)
{
    if (m_items == items) { return; }
    m_items = items;

    if (items.isEmpty()) {
        setToolTip(QString());
        return;
    }

    QString tip = tr("Hidden to fit window:");
    for (const QString& item : items) {
        tip += QStringLiteral("\n  • ") + item;
    }
    setToolTip(tip);
    // No setVisible() here (final-fix-wave finding 4): this chip is now
    // registered with ChromeBarController at rung 0, which makes the
    // controller the sole writer of visibility, via the availability axis
    // (MainWindow wires !items.isEmpty() through setItemAvailable right
    // after calling this). That also means its own ~26 px is finally
    // counted in the fold width budget, instead of popping up unbudgeted
    // exactly when folding starts.
}

} // namespace NereusSDR
