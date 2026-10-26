#include "LedItemEditor.h"
#include "../../meters/LEDItem.h"

#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QColorDialog>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>

namespace NereusSDR {

LedItemEditor::LedItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
    // Update interval range from Thetis setup.designer.cs:63735-63744 [v2.10.3.15]
    // (nudLedIndicator_UpdateInterval: 50-10000)
    setUpdateIntervalRange(kLedMinUpdateIntervalMs, kLedMaxUpdateIntervalMs);
}

void LedItemEditor::applyPlacedRows(bool placed)
{
    beginProgrammaticUpdate();
    if (placed) {
        // Padding range from Thetis setup.designer.cs:63987-64005 [v2.10.3.15]
        // (nudLedIndicator_PanelPadding: 0.001-10, step 0.001)
        m_spinPadding->setDecimals(3);
        m_spinPadding->setRange(kLedMinPanelPadding, kLedMaxPanelPadding);
        m_spinPadding->setSingleStep(0.001);
        m_spinPadding->setToolTip(QStringLiteral(
            "Height of the panel as a ratio of the width."));
    } else {
        m_spinPadding->setDecimals(1);
        m_spinPadding->setRange(0.0, 50.0);
        m_spinPadding->setSingleStep(0.5);
        m_spinPadding->setToolTip(QStringLiteral(
            "Space in pixels between the light and the edge of the item."));
    }
    for (QDoubleSpinBox* spin : {m_spinOffsetX, m_spinOffsetY, m_spinSizeX, m_spinSizeY}) {
        spin->setEnabled(placed);
    }
    endProgrammaticUpdate();
}

void LedItemEditor::refreshConditionRows()
{
    LEDItem* x = qobject_cast<LEDItem*>(m_item);
    const bool has = x && x->hasCondition();
    for (QDoubleSpinBox* spin : {m_spinGreenThreshold, m_spinAmberThreshold, m_spinRedThreshold}) {
        spin->setEnabled(!has);
    }
    QString error;
    if (has) {
        const LedConditionResult check = x->checkCondition(x->condition());
        if (!check.ok) {
            error = check.error;
        }
    }
    m_lblConditionError->setText(error);
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
    m_chkPlaced->setChecked(x->placed());
    applyPlacedRows(x->placed());
    m_spinPadding->setValue(static_cast<double>(x->padding()));
    m_spinOffsetX->setValue(static_cast<double>(x->offsetX()));
    m_spinOffsetY->setValue(static_cast<double>(x->offsetY()));
    m_spinSizeX->setValue(static_cast<double>(x->sizeX()));
    m_spinSizeY->setValue(static_cast<double>(x->sizeY()));
    m_chkShowTrue->setChecked(x->showTrue());
    m_chkShowFalse->setChecked(x->showFalse());
    m_chkProcessWhenHidden->setChecked(x->processWhenHidden());
    m_chkNoTxTrue->setChecked(x->noTx());
    m_chkNoTxFalse->setChecked(x->noTxFalse());

    m_editCondition->setText(x->condition());
    m_spinGreenThreshold->setValue(x->greenThreshold());
    m_spinAmberThreshold->setValue(x->amberThreshold());
    m_spinRedThreshold->setValue(x->redThreshold());
    refreshConditionRows();

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
    addRow(QStringLiteral("True color"), m_btnTrueColour);

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
    addRow(QStringLiteral("False color"), m_btnFalseColour);

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

    // A placed light takes one light mode (Thetis radLed_light_on_off /
    // blink / pulsate), so checking one clears the other; the fill look
    // keeps both.
    m_chkBlink = makeCheckRow(QStringLiteral("Blink"));
    m_chkBlink->setToolTip(QStringLiteral(
        "Placed light: flash twice when it turns true. "
        "Fill light: flash on and off."));
    connect(m_chkBlink, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        if (x->placed() && on) {
            x->setLightMode(LEDItem::LightMode::Blink);
            beginProgrammaticUpdate();
            m_chkPulsate->setChecked(false);
            endProgrammaticUpdate();
        } else {
            x->setBlink(on);
        }
        notifyChanged();
    });

    m_chkPulsate = makeCheckRow(QStringLiteral("Pulsate"));
    m_chkPulsate->setToolTip(QStringLiteral(
        "Placed light: fade up and down while true. "
        "Fill light: fade the light in and out."));
    connect(m_chkPulsate, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        if (x->placed() && on) {
            x->setLightMode(LEDItem::LightMode::Pulsate);
            beginProgrammaticUpdate();
            m_chkBlink->setChecked(false);
            endProgrammaticUpdate();
        } else {
            x->setPulsate(on);
        }
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

    // Placement
    addHeader(QStringLiteral("Placement"));

    m_chkPlaced = makeCheckRow(QStringLiteral("Place by offset and size"));
    m_chkPlaced->setToolTip(QStringLiteral(
        "Draw the light at the offset and size below, inside a panel as tall as the width "
        "times the padding, fading between its colors. Off: the light fills the item."));
    connect(m_chkPlaced, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setPlaced(on);
        // The padding changes unit with the look, so it starts again at
        // that look's default.
        x->setPadding(on ? kLedThetisDefaults.padding : kLedFillDefaultPadding);
        if (on && x->blink() && x->pulsate()) {
            x->setLightMode(LEDItem::LightMode::Blink);
        }
        applyPlacedRows(on);
        beginProgrammaticUpdate();
        m_spinPadding->setValue(static_cast<double>(x->padding()));
        m_chkPulsate->setChecked(x->pulsate());
        endProgrammaticUpdate();
        notifyChanged();
    });

    // Offset and size range from Thetis setup.designer.cs:63894-63974 [v2.10.3.15]
    // (nudLedIndicator_xSize / xOffset: -10..10, step 0.001)
    auto offsetSizeRow = [this](const QString& label, const QString& tip,
                                void (LEDItem::*setter)(float)) {
        QDoubleSpinBox* spin = makeDoubleRow(label, kLedMinOffsetSize, kLedMaxOffsetSize, 0.001, 3);
        spin->setToolTip(tip);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this, setter](double v) {
            if (isProgrammaticUpdate()) { return; }
            LEDItem* x = qobject_cast<LEDItem*>(m_item);
            if (!x) { return; }
            (x->*setter)(static_cast<float>(v));
            notifyChanged();
        });
        return spin;
    };
    m_spinOffsetX = offsetSizeRow(QStringLiteral("Offset X"),
        QStringLiteral("Centre of the light from the left, as a ratio of the width."),
        &LEDItem::setOffsetX);
    m_spinOffsetY = offsetSizeRow(QStringLiteral("Offset Y"),
        QStringLiteral("Centre of the light from the top, as a ratio of the width."),
        &LEDItem::setOffsetY);
    m_spinSizeX = offsetSizeRow(QStringLiteral("Size X"),
        QStringLiteral("Width of the light as a ratio of the width."),
        &LEDItem::setSizeX);
    m_spinSizeY = offsetSizeRow(QStringLiteral("Size Y"),
        QStringLiteral("Height of the light as a ratio of the width."),
        &LEDItem::setSizeY);

    m_chkShowTrue = makeCheckRow(QStringLiteral("Show when true"));
    m_chkShowTrue->setToolTip(QStringLiteral("Draw the light while it is true."));
    connect(m_chkShowTrue, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setShowTrue(on);
        notifyChanged();
    });

    m_chkShowFalse = makeCheckRow(QStringLiteral("Show when false"));
    m_chkShowFalse->setToolTip(QStringLiteral("Draw the light while it is false."));
    connect(m_chkShowFalse, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setShowFalse(on);
        notifyChanged();
    });

    m_chkProcessWhenHidden = makeCheckRow(QStringLiteral("Process when hidden"));
    m_chkProcessWhenHidden->setToolTip(QStringLiteral(
        "Keep working out whether the light is true while its container is hidden."));
    connect(m_chkProcessWhenHidden, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setProcessWhenHidden(on);
        notifyChanged();
    });

    // No TX when true / false (Thetis chkLed_notx_true and chkLed_notx_false,
    // setup.designer.cs:3995-3996 [v2.10.3.15]; LEDItem::setNoTx and
    // setNoTxFalse).
    m_chkNoTxTrue = makeCheckRow(QStringLiteral("Stop transmitting when the LED turns on"));
    m_chkNoTxTrue->setObjectName(QStringLiteral("ledNoTxTrue"));
    m_chkNoTxTrue->setToolTip(QStringLiteral(
        "While this radio transmits, transmitting stops each time the LED turns on."));
    connect(m_chkNoTxTrue, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setNoTx(on);
        notifyChanged();
    });

    m_chkNoTxFalse = makeCheckRow(QStringLiteral("Stop transmitting when the LED turns off"));
    m_chkNoTxFalse->setObjectName(QStringLiteral("ledNoTxFalse"));
    m_chkNoTxFalse->setToolTip(QStringLiteral(
        "While this radio transmits, transmitting stops each time the LED turns off."));
    connect(m_chkNoTxFalse, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setNoTxFalse(on);
        notifyChanged();
    });
    // The two long labels wrap rather than widen the label column.
    for (QWidget* field : {static_cast<QWidget*>(m_chkNoTxTrue), static_cast<QWidget*>(m_chkNoTxFalse)}) {
        if (auto* label = qobject_cast<QLabel*>(m_form->labelForField(field))) { label->setWordWrap(true); }
    }

    // Condition, beside the thresholds (Thetis txtLedIndicator_condition).
    addHeader(QStringLiteral("Condition"));

    m_editCondition = new QLineEdit(this);
    m_editCondition->setObjectName(QStringLiteral("ledCondition"));
    m_editCondition->setPlaceholderText(QStringLiteral("%signal_strength% > -73"));
    m_editCondition->setToolTip(QStringLiteral(
        "When to light the LED, worked out from readings, for example "
        "%swr% >= 2 or (%swr% >= 2) && !(%mic% < -20). "
        "Use numbers, \"text\", %names%, + - * /, < <= > >= == !=, && || ! and brackets. "
        "Names are the readings of this slice, the clock values, and data "
        "source values written %ABCD.name%. Leave it empty to use the thresholds."));
    addRow(QStringLiteral("Condition"), m_editCondition);
    connect(m_editCondition, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (isProgrammaticUpdate()) { return; }
        LEDItem* x = qobject_cast<LEDItem*>(m_item);
        if (!x) { return; }
        x->setCondition(text);
        refreshConditionRows();
        notifyChanged();
    });

    // The reason the condition cannot be worked out; empty while it can.
    // The LED stays false while there is one.
    m_lblConditionError = new QLabel(this);
    m_lblConditionError->setWordWrap(true);
    m_lblConditionError->setStyleSheet(QStringLiteral("QLabel { color: #ff8060; }"));
    m_lblConditionError->setToolTip(QStringLiteral(
        "While the condition has a mistake the LED stays false."));
    addRow(QString(), m_lblConditionError);

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

    for (QDoubleSpinBox* spin : {m_spinGreenThreshold, m_spinAmberThreshold, m_spinRedThreshold}) {
        spin->setToolTip(QStringLiteral("Used while the condition is empty."));
    }
}

} // namespace NereusSDR
