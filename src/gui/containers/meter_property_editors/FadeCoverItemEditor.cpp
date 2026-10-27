#include "FadeCoverItemEditor.h"
#include "gui/meters/ItemColourLook.h"
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
    ItemColourLook::paintSwatch(m_btnColour1, QStringLiteral("FadeCover.colour1"), fade->colour1());
    ItemColourLook::paintSwatch(m_btnColour2, QStringLiteral("FadeCover.colour2"), fade->colour2());
    m_spinAlpha->setValue(static_cast<double>(fade->alpha()));
    m_chkUseBackground->setChecked(fade->useContainerBackground());
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
                                                     QStringLiteral("Color 1"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            fade->setColour1(chosen);
            ItemColourLook::paintSwatch(m_btnColour1, QStringLiteral("FadeCover.colour1"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 1"), m_btnColour1);

    // Colour 2
    m_btnColour2 = new QPushButton(this);
    m_btnColour2->setFixedSize(40, 18);
    connect(m_btnColour2, &QPushButton::clicked, this, [this]() {
        FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(m_item);
        if (!fade) { return; }
        const QColor chosen = QColorDialog::getColor(fade->colour2(), this,
                                                     QStringLiteral("Color 2"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            fade->setColour2(chosen);
            ItemColourLook::paintSwatch(m_btnColour2, QStringLiteral("FadeCover.colour2"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 2"), m_btnColour2);

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

    // NereusSDR option: paint the container background, as Thetis's fade
    // cover does, instead of the colour 1 / colour 2 gradient.
    m_chkUseBackground = makeCheckRow(QStringLiteral("Use container background"));
    m_chkUseBackground->setObjectName(QStringLiteral("useContainerBackground"));
    connect(m_chkUseBackground, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        FadeCoverItem* fade = qobject_cast<FadeCoverItem*>(m_item);
        if (!fade) { return; }
        fade->setUseContainerBackground(on);
        notifyChanged();
    });
}

} // namespace NereusSDR
