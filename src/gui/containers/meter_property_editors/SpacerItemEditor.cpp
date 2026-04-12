#include "SpacerItemEditor.h"
#include "../../meters/MeterItem.h"
#include "../../meters/SpacerItem.h"

#include <QDoubleSpinBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

SpacerItemEditor::SpacerItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void SpacerItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    SpacerItem* spacer = qobject_cast<SpacerItem*>(item);
    if (!spacer) { return; }

    beginProgrammaticUpdate();
    m_btnColour1->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(spacer->colour1().name(QColor::HexArgb)));
    m_btnColour2->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(spacer->colour2().name(QColor::HexArgb)));
    m_spinPadding->setValue(static_cast<double>(spacer->padding()));
    endProgrammaticUpdate();
}

void SpacerItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Spacer"));

    // Colour 1
    m_btnColour1 = new QPushButton(this);
    m_btnColour1->setFixedSize(40, 18);
    connect(m_btnColour1, &QPushButton::clicked, this, [this]() {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        const QColor chosen = QColorDialog::getColor(spacer->colour1(), this,
                                                     QStringLiteral("Colour 1"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            spacer->setColour1(chosen);
            m_btnColour1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Colour 1"), m_btnColour1);

    // Colour 2
    m_btnColour2 = new QPushButton(this);
    m_btnColour2->setFixedSize(40, 18);
    connect(m_btnColour2, &QPushButton::clicked, this, [this]() {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        const QColor chosen = QColorDialog::getColor(spacer->colour2(), this,
                                                     QStringLiteral("Colour 2"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            spacer->setColour2(chosen);
            m_btnColour2->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Colour 2"), m_btnColour2);

    // Padding
    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.0, 1.0, 0.01, 3);
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        spacer->setPadding(static_cast<float>(v));
        notifyChanged();
    });
}

} // namespace NereusSDR
