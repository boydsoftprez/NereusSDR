#include "SpacerItemEditor.h"
#include "../../meters/MeterItem.h"
#include "../../meters/SpacerItem.h"

#include <QCheckBox>
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
    m_chkRxTxColours->setChecked(spacer->rxTxColourMode());
    m_chkPaddingSetsHeight->setChecked(spacer->paddingSetsHeight());
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
                                                     QStringLiteral("Color 1"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            spacer->setColour1(chosen);
            m_btnColour1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 1"), m_btnColour1);

    // Colour 2
    m_btnColour2 = new QPushButton(this);
    m_btnColour2->setFixedSize(40, 18);
    connect(m_btnColour2, &QPushButton::clicked, this, [this]() {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        const QColor chosen = QColorDialog::getColor(spacer->colour2(), this,
                                                     QStringLiteral("Color 2"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            spacer->setColour2(chosen);
            m_btnColour2->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 2"), m_btnColour2);

    // Receive / transmit colors (off: a gradient from color 1 to color 2)
    m_chkRxTxColours = makeCheckRow(QStringLiteral("Receive / transmit colors"));
    m_chkRxTxColours->setToolTip(QStringLiteral(
        "Show color 1 while receiving and color 2 while transmitting, fading in on each change. "
        "Off: a gradient from color 1 at the top to color 2 at the bottom."));
    connect(m_chkRxTxColours, &QCheckBox::toggled, this, [this](bool on) {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (isProgrammaticUpdate() || !spacer) { return; }
        spacer->setRxTxColourMode(on);
        notifyChanged();
    });

    // Padding: range and step from Thetis setup.designer.cs:63167-63192
    // [v2.10.3.15] (nudMeterItemSpacerPadding: 0.001-10, step 0.001).
    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.001, 10.0, 0.001, 3);
    m_spinPadding->setToolTip(QStringLiteral(
        "Size of the spacer as a ratio of its width, used when padding sets the height."));
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        spacer->setPadding(static_cast<float>(v));
        notifyChanged();
    });

    m_chkPaddingSetsHeight = makeCheckRow(QStringLiteral("Padding sets height"));
    m_chkPaddingSetsHeight->setToolTip(QStringLiteral(
        "Draw the spacer as tall as its width times the padding. Off: it fills its area."));
    connect(m_chkPaddingSetsHeight, &QCheckBox::toggled, this, [this](bool on) {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (isProgrammaticUpdate() || !spacer) { return; }
        spacer->setPaddingSetsHeight(on);
        notifyChanged();
    });
}

} // namespace NereusSDR
