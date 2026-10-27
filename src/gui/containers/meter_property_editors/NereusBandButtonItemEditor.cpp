// no-port-check: NereusSDR-original property editor. Pairs with
// NereusBandButtonItemEditor.h. Copies no Thetis source.
// 2026-10-22 - Brian Sutton, AI-assisted via xAI Grok.
// 2026-10-26 - PR 677 review fix wave (I6): a colour dialog pick is recorded
//              as the user's. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//              Claude Code.

#include "NereusBandButtonItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/NereusBandButtonItem.h"

#include <QColorDialog>
#include <QPushButton>

namespace NereusSDR {
namespace {

void applySwatch(QPushButton* button, const QString& field, const QColor& color)
{
    ItemColourLook::paintSwatch(button, field, color);
}

} // namespace

NereusBandButtonItemEditor::NereusBandButtonItemEditor(QWidget* parent)
    : BandButtonItemEditor(parent)
{
    addHeader(QStringLiteral("Nereus colours"));

    auto addColor = [this](const QString& label, const QString& objectName,
                           const QString& tip, QPushButton*& slot,
                           const QString& field,
                           QColor (NereusBandButtonItem::*read)() const,
                           void (NereusBandButtonItem::*write)(const QColor&)) {
        auto* button = new QPushButton(this);
        button->setObjectName(objectName);
        button->setFixedSize(40, 18);
        button->setToolTip(tip);
        connect(button, &QPushButton::clicked, this, [this, button, tip, field, read, write]() {
            auto* band = qobject_cast<NereusBandButtonItem*>(m_item);
            if (!band) { return; }
            const QColor chosen = QColorDialog::getColor(
                (band->*read)(), this, tip, QColorDialog::ShowAlphaChannel);
            if (!chosen.isValid()) { return; }
            markColourPicked(field);
            (band->*write)(chosen);
            applySwatch(button, field, chosen);
            notifyChanged();
        });
        addRow(label, button);
        slot = button;
    };

    addColor(QStringLiteral("Button text"),
             QStringLiteral("nereusBandButtonTextColor"),
             QStringLiteral("Label colour on every band button."),
             m_textColorButton,
             QStringLiteral("NereusBand.buttonTextColor"),
             &NereusBandButtonItem::buttonTextColor,
             &NereusBandButtonItem::setButtonTextColor);
    addColor(QStringLiteral("Background top"),
             QStringLiteral("nereusBandBackgroundTop"),
             QStringLiteral("Top colour of a band that is not selected. "
                            "Defaults to the PureSignal button."),
             m_backgroundTopButton,
             QStringLiteral("NereusBand.backgroundTopColor"),
             &NereusBandButtonItem::backgroundTopColor,
             &NereusBandButtonItem::setBackgroundTopColor);
    addColor(QStringLiteral("Background bottom"),
             QStringLiteral("nereusBandBackgroundBottom"),
             QStringLiteral("Bottom colour of a band that is not selected. "
                            "Defaults to the PureSignal button."),
             m_backgroundBottomButton,
             QStringLiteral("NereusBand.backgroundBottomColor"),
             &NereusBandButtonItem::backgroundBottomColor,
             &NereusBandButtonItem::setBackgroundBottomColor);
    addColor(QStringLiteral("Selected top"),
             QStringLiteral("nereusBandSelectedTop"),
             QStringLiteral("Top colour of the selected band. "
                            "Defaults to the checked PureSignal button."),
             m_selectedTopButton,
             QStringLiteral("NereusBand.selectedTopColor"),
             &NereusBandButtonItem::selectedTopColor,
             &NereusBandButtonItem::setSelectedTopColor);
    addColor(QStringLiteral("Selected bottom"),
             QStringLiteral("nereusBandSelectedBottom"),
             QStringLiteral("Bottom colour of the selected band. "
                            "Defaults to the checked PureSignal button."),
             m_selectedBottomButton,
             QStringLiteral("NereusBand.selectedBottomColor"),
             &NereusBandButtonItem::selectedBottomColor,
             &NereusBandButtonItem::setSelectedBottomColor);
}

void NereusBandButtonItemEditor::setItem(MeterItem* item)
{
    BandButtonItemEditor::setItem(item);
    auto* band = qobject_cast<NereusBandButtonItem*>(item);
    if (!band || !m_textColorButton || !m_backgroundTopButton || !m_backgroundBottomButton
        || !m_selectedTopButton || !m_selectedBottomButton) {
        return;
    }
    applySwatch(m_textColorButton, QStringLiteral("NereusBand.buttonTextColor"), band->buttonTextColor());
    applySwatch(m_backgroundTopButton, QStringLiteral("NereusBand.backgroundTopColor"), band->backgroundTopColor());
    applySwatch(m_backgroundBottomButton, QStringLiteral("NereusBand.backgroundBottomColor"), band->backgroundBottomColor());
    applySwatch(m_selectedTopButton, QStringLiteral("NereusBand.selectedTopColor"), band->selectedTopColor());
    applySwatch(m_selectedBottomButton, QStringLiteral("NereusBand.selectedBottomColor"), band->selectedBottomColor());
}

} // namespace NereusSDR
