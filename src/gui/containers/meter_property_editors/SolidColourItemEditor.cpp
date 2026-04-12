#include "SolidColourItemEditor.h"
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
    m_btnColour->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(solid->colour().name(QColor::HexArgb)));
    endProgrammaticUpdate();
}

void SolidColourItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Solid colour"));

    m_btnColour = new QPushButton(this);
    m_btnColour->setFixedSize(40, 18);
    connect(m_btnColour, &QPushButton::clicked, this, [this]() {
        SolidColourItem* solid = qobject_cast<SolidColourItem*>(m_item);
        if (!solid) { return; }
        const QColor chosen = QColorDialog::getColor(solid->colour(), this,
                                                     QStringLiteral("Fill colour"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            solid->setColour(chosen);
            m_btnColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Colour"), m_btnColour);
}

} // namespace NereusSDR
