// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I6): a colour dialog pick is
//                 recorded as the user's (markColourPicked). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "SolidColourItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/MeterItem.h"

#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

SolidColourItemEditor::SolidColourItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void SolidColourItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    SolidColourItem* solid = qobject_cast<SolidColourItem*>(item);
    if (!solid) { return; }

    beginProgrammaticUpdate();
    ItemColourLook::paintSwatch(m_btnColour, QStringLiteral("Solid.colour"), solid->colour());
    endProgrammaticUpdate();
}

void SolidColourItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Solid color"));

    m_btnColour = new QPushButton(this);
    m_btnColour->setFixedSize(40, 18);
    connect(m_btnColour, &QPushButton::clicked, this, [this]() {
        SolidColourItem* solid = qobject_cast<SolidColourItem*>(m_item);
        if (!solid) { return; }
        const QColor chosen = QColorDialog::getColor(solid->colour(), this,
                                                     QStringLiteral("Fill color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            markColourPicked(QStringLiteral("Solid.colour"));
            solid->setColour(chosen);
            ItemColourLook::paintSwatch(m_btnColour, QStringLiteral("Solid.colour"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color"), m_btnColour);
}

} // namespace NereusSDR
