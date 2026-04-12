#include "LedItemEditor.h"
#include "../../meters/LEDItem.h"

#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QColorDialog>

namespace NereusSDR {

LedItemEditor::LedItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void LedItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    LEDItem* x = qobject_cast<LEDItem*>(item);
    if (!x) { return; }

    auto applyColor = [](QPushButton* btn, const QColor& c) {
        btn->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };

    beginProgrammaticUpdate();

    const int shapeIdx = m_comboShape->findData(static_cast<int>(x->ledShape()));
    m_comboShape->setCurrentIndex(shapeIdx >= 0 ? shapeIdx : 0);

    const int styleIdx = m_comboStyle->findData(static_cast<int>(x->ledStyle()));
    m_comboStyle->setCurrentIndex(styleIdx >= 0 ? styleIdx : 0);

    applyColor(m_btnTrueColour, x->trueColour());
    applyColor(m_btnFalseColour, x->falseColour());

    m_chkShowPanel->setChecked(x->showBackPanel());
    applyColor(m_btnPanelBack1, x->panelBackColour1());
    applyColor(m_btnPanelBack2, x->panelBackColour2());

    m_chkBlink->setChecked(x->blink());
    m_chkPulsate->setChecked(x->pulsate());
    m_spinPadding->setValue(static_cast<double>(x->padding()));

    m_spinGreenThreshold->setValue(x->greenThreshold());
    m_spinAmberThreshold->setValue(x->amberThreshold());
    m_spinRedThreshold->setValue(x->redThreshold());

    endProgrammaticUpdate();
}

void LedItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("LED Item"));

    // Shape
    m_comboShape = new QComboBox(this);
    m_comboShape->addItem(QStringLiteral("Round"),    static_cast<int>(LEDItem::LedShape::Round));
    m_comboShape->addItem(QStringLiteral("Square"),   static_cast<int>(LEDItem::LedShape::Square));
    m_comboShape->addItem(QStringLiteral("Triangle"), static_cast<int>(LEDItem::LedShape::Triangle));
    addRow(QStringLiteral("Shape"), m_comboShape);
    connect(m_comboShape, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setLedShape(static_cast<LEDItem::LedShape>(m_comboShape->currentData().toInt()));
        notifyChanged();
    });

    // Style
    m_comboStyle = new QComboBox(this);
    m_comboStyle->addItem(QStringLiteral("Flat"), static_cast<int>(LEDItem::LedStyle::Flat));
    m_comboStyle->addItem(QStringLiteral("3D"),   static_cast<int>(LEDItem::LedStyle::ThreeD));
    addRow(QStringLiteral("Style"), m_comboStyle);
    connect(m_comboStyle, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setLedStyle(static_cast<LEDItem::LedStyle>(m_comboStyle->currentData().toInt()));
        notifyChanged();
    });

    // Colors
    m_btnTrueColour = new QPushButton(this);
    m_btnTrueColour->setFixedSize(40, 18);
    connect(m_btnTrueColour, &QPushButton::clicked, this, [this]() {
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->trueColour(), this);
        if (chosen.isValid()) {
            x->setTrueColour(chosen);
            m_btnTrueColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("True colour"), m_btnTrueColour);

    m_btnFalseColour = new QPushButton(this);
    m_btnFalseColour->setFixedSize(40, 18);
    connect(m_btnFalseColour, &QPushButton::clicked, this, [this]() {
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->falseColour(), this);
        if (chosen.isValid()) {
            x->setFalseColour(chosen);
            m_btnFalseColour->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("False colour"), m_btnFalseColour);

    // Panel
    m_chkShowPanel = makeCheckRow(QStringLiteral("Show panel"));
    connect(m_chkShowPanel, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setShowBackPanel(on);
        notifyChanged();
    });

    m_btnPanelBack1 = new QPushButton(this);
    m_btnPanelBack1->setFixedSize(40, 18);
    connect(m_btnPanelBack1, &QPushButton::clicked, this, [this]() {
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->panelBackColour1(), this);
        if (chosen.isValid()) {
            x->setPanelBackColour1(chosen);
            m_btnPanelBack1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Panel back RX"), m_btnPanelBack1);

    m_btnPanelBack2 = new QPushButton(this);
    m_btnPanelBack2->setFixedSize(40, 18);
    connect(m_btnPanelBack2, &QPushButton::clicked, this, [this]() {
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->panelBackColour2(), this);
        if (chosen.isValid()) {
            x->setPanelBackColour2(chosen);
            m_btnPanelBack2->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Panel back TX"), m_btnPanelBack2);

    // Animation
    addHeader(QStringLiteral("Animation"));

    m_chkBlink = makeCheckRow(QStringLiteral("Blink"));
    connect(m_chkBlink, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setBlink(on);
        notifyChanged();
    });

    m_chkPulsate = makeCheckRow(QStringLiteral("Pulsate"));
    connect(m_chkPulsate, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setPulsate(on);
        notifyChanged();
    });

    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.0, 50.0, 0.5, 1);
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setPadding(static_cast<float>(v));
        notifyChanged();
    });

    // Thresholds
    addHeader(QStringLiteral("Thresholds"));

    m_spinGreenThreshold = makeDoubleRow(QStringLiteral("Green threshold"), -200.0, 200.0, 1.0, 1);
    connect(m_spinGreenThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setGreenThreshold(v);
        notifyChanged();
    });

    m_spinAmberThreshold = makeDoubleRow(QStringLiteral("Amber threshold"), -200.0, 10000.0, 1.0, 1);
    connect(m_spinAmberThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setAmberThreshold(v);
        notifyChanged();
    });

    m_spinRedThreshold = makeDoubleRow(QStringLiteral("Red threshold"), -200.0, 10000.0, 1.0, 1);
    connect(m_spinRedThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setRedThreshold(v);
        notifyChanged();
    });
}

} // namespace NereusSDR
