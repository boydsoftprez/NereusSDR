// no-port-check: NereusSDR-original property editor. Pairs with
// NereusBandButtonItemEditor.h. Copies no Thetis source.
// 2026-10-22 - Brian Sutton, AI-assisted via xAI Grok.

#include "NereusBandButtonItemEditor.h"
#include "../../meters/NereusBandButtonItem.h"

#include <QColorDialog>
#include <QPushButton>

namespace NereusSDR {
namespace {

void applySwatch(QPushButton* button, const QColor& color)
{
    button->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(color.name(QColor::HexArgb)));
}

} // namespace

NereusBandButtonItemEditor::NereusBandButtonItemEditor(QWidget* parent)
    : BandButtonItemEditor(parent)
{
    addHeader(QStringLiteral("Nereus colours"));

    auto addColor = [this](const QString& label, const QString& objectName,
                           const QString& tip, QPushButton*& slot,
                           QColor (NereusBandButtonItem::*read)() const,
                           void (NereusBandButtonItem::*write)(const QColor&)) {
        auto* button = new QPushButton(this);
        button->setObjectName(objectName);
        button->setFixedSize(40, 18);
        button->setToolTip(tip);
        connect(button, &QPushButton::clicked, this, [this, button, read, write]() {
            auto* band = qobject_cast<NereusBandButtonItem*>(m_item);
            if (!band) { return; }
            const QColor chosen = QColorDialog::getColor(
                (band->*read)(), this, button->toolTip(), QColorDialog::ShowAlphaChannel);
            if (!chosen.isValid()) { return; }
            (band->*write)(chosen);
            applySwatch(button, chosen);
            notifyChanged();
        });
        addRow(label, button);
        slot = button;
    };

    addColor(QStringLiteral("Button text"),
             QStringLiteral("nereusBandButtonTextColor"),
             QStringLiteral("Label colour on every band button."),
             m_textColorButton,
             &NereusBandButtonItem::buttonTextColor,
             &NereusBandButtonItem::setButtonTextColor);
    addColor(QStringLiteral("Background top"),
             QStringLiteral("nereusBandBackgroundTop"),
             QStringLiteral("Top colour of a band that is not selected. "
                            "Defaults to the PureSignal button."),
             m_backgroundTopButton,
             &NereusBandButtonItem::backgroundTopColor,
             &NereusBandButtonItem::setBackgroundTopColor);
    addColor(QStringLiteral("Background bottom"),
             QStringLiteral("nereusBandBackgroundBottom"),
             QStringLiteral("Bottom colour of a band that is not selected. "
                            "Defaults to the PureSignal button."),
             m_backgroundBottomButton,
             &NereusBandButtonItem::backgroundBottomColor,
             &NereusBandButtonItem::setBackgroundBottomColor);
    addColor(QStringLiteral("Selected top"),
             QStringLiteral("nereusBandSelectedTop"),
             QStringLiteral("Top colour of the selected band. "
                            "Defaults to the checked PureSignal button."),
             m_selectedTopButton,
             &NereusBandButtonItem::selectedTopColor,
             &NereusBandButtonItem::setSelectedTopColor);
    addColor(QStringLiteral("Selected bottom"),
             QStringLiteral("nereusBandSelectedBottom"),
             QStringLiteral("Bottom colour of the selected band. "
                            "Defaults to the checked PureSignal button."),
             m_selectedBottomButton,
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
    applySwatch(m_textColorButton, band->buttonTextColor());
    applySwatch(m_backgroundTopButton, band->backgroundTopColor());
    applySwatch(m_backgroundBottomButton, band->backgroundBottomColor());
    applySwatch(m_selectedTopButton, band->selectedTopColor());
    applySwatch(m_selectedBottomButton, band->selectedBottomColor());
}

} // namespace NereusSDR
