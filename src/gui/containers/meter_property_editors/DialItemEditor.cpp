#include "DialItemEditor.h"
#include "../../meters/DialItem.h"

#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

// Helper macro-style lambda factory used locally to reduce repetition for
// color buttons in this file only.
namespace {
void applyBtnColor(QPushButton* btn, const QColor& c)
{
    btn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(c.name(QColor::HexArgb)));
}
} // namespace

DialItemEditor::DialItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void DialItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    DialItem* d = qobject_cast<DialItem*>(item);
    if (!d) { return; }

    beginProgrammaticUpdate();
    applyBtnColor(m_btnTextColour,    d->textColour());
    applyBtnColor(m_btnCircleColour,  d->circleColour());
    applyBtnColor(m_btnPadColour,     d->padColour());
    applyBtnColor(m_btnRingColour,    d->ringColour());
    applyBtnColor(m_btnBtnOn,         d->buttonOnColour());
    applyBtnColor(m_btnBtnOff,        d->buttonOffColour());
    applyBtnColor(m_btnBtnHighlight,  d->buttonHighlightColour());
    applyBtnColor(m_btnSlowColour,    d->slowColour());
    applyBtnColor(m_btnHoldColour,    d->holdColour());
    applyBtnColor(m_btnFastColour,    d->fastColour());
    endProgrammaticUpdate();
}

void DialItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Dial colors"));

    // Helper: create a 40x18 color button and wire it.
    auto makeColorBtn = [this](const QString& label, QPushButton*& out,
                               auto getter, auto setter) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        QPushButton* btn = out; // capture by value — points to the same object
        connect(btn, &QPushButton::clicked, this, [this, btn, getter, setter]() {
            DialItem* d = qobject_cast<DialItem*>(m_item);
            if (!d) { return; }
            const QColor chosen = QColorDialog::getColor(
                (d->*getter)(), this, QString(), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                (d->*setter)(chosen);
                applyBtnColor(btn, chosen);
                notifyChanged();
            }
        });
        addRow(label, out);
    };

    makeColorBtn(QStringLiteral("Text color"),       m_btnTextColour,
                 &DialItem::textColour,   &DialItem::setTextColour);
    makeColorBtn(QStringLiteral("Circle color"),     m_btnCircleColour,
                 &DialItem::circleColour, &DialItem::setCircleColour);
    makeColorBtn(QStringLiteral("Pad color"),        m_btnPadColour,
                 &DialItem::padColour,    &DialItem::setPadColour);
    makeColorBtn(QStringLiteral("Ring color"),       m_btnRingColour,
                 &DialItem::ringColour,   &DialItem::setRingColour);

    addHeader(QStringLiteral("Button colors"));
    makeColorBtn(QStringLiteral("Button on"),        m_btnBtnOn,
                 &DialItem::buttonOnColour,        &DialItem::setButtonOnColour);
    makeColorBtn(QStringLiteral("Button off"),       m_btnBtnOff,
                 &DialItem::buttonOffColour,       &DialItem::setButtonOffColour);
    makeColorBtn(QStringLiteral("Button highlight"), m_btnBtnHighlight,
                 &DialItem::buttonHighlightColour, &DialItem::setButtonHighlightColour);

    addHeader(QStringLiteral("Speed indicator colors"));
    makeColorBtn(QStringLiteral("Slow color"),  m_btnSlowColour,
                 &DialItem::slowColour, &DialItem::setSlowColour);
    makeColorBtn(QStringLiteral("Hold color"),  m_btnHoldColour,
                 &DialItem::holdColour, &DialItem::setHoldColour);
    makeColorBtn(QStringLiteral("Fast color"),  m_btnFastColour,
                 &DialItem::fastColour, &DialItem::setFastColour);
}

} // namespace NereusSDR
