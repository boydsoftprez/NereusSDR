#include "FadeCoverItemEditor.h"
#include "../../meters/MeterItem.h"
#include "../../meters/FadeCoverItem.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

FadeCoverItemEditor::FadeCoverItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void FadeCoverItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(item);
    if (!fade) { return; }

    beginProgrammaticUpdate();
    m_chkFadeOnRx->setChecked(fade->fadeOnRx());
    m_chkFadeOnTx->setChecked(fade->fadeOnTx());
    m_btnColour1->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(fade->colour1().name(QColor::HexArgb)));
    m_btnColour2->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(fade->colour2().name(QColor::HexArgb)));
    m_spinAlpha->setValue(static_cast<double>(fade->alpha()));
    endProgrammaticUpdate();
}

void FadeCoverItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Fade cover"));

    // Trigger conditions
    m_chkFadeOnRx = makeCheckRow(QStringLiteral("Fade on RX"));
    connect(m_chkFadeOnRx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(m_item);
        if (!fade) { return; }
        fade->setFadeOnRx(on);
        notifyChanged();
    });

    m_chkFadeOnTx = makeCheckRow(QStringLiteral("Fade on TX"));
    connect(m_chkFadeOnTx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(m_item);
        if (!fade) { return; }
        fade->setFadeOnTx(on);
        notifyChanged();
    });

    // Colour 1
    m_btnColour1 = new QPushButton(this);
    m_btnColour1->setFixedSize(40, 18);
    connect(m_btnColour1, &QPushButton::clicked, this, [this]() {
        FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(m_item);
        if (!fade) { return; }
        const QColor chosen = QColorDialog::getColor(fade->colour1(), this,
                                                     QStringLiteral("Colour 1"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            fade->setColour1(chosen);
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
        FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(m_item);
        if (!fade) { return; }
        const QColor chosen = QColorDialog::getColor(fade->colour2(), this,
                                                     QStringLiteral("Colour 2"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            fade->setColour2(chosen);
            m_btnColour2->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Colour 2"), m_btnColour2);

    // Alpha
    m_spinAlpha = makeDoubleRow(QStringLiteral("Alpha"), 0.0, 1.0, 0.01, 2);
    connect(m_spinAlpha, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(m_item);
        if (!fade) { return; }
        fade->setAlpha(static_cast<float>(v));
        notifyChanged();
    });
}

} // namespace NereusSDR
