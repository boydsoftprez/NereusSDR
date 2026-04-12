#include "TextItemEditor.h"
#include "../../meters/MeterItem.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

TextItemEditor::TextItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void TextItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    TextItem* x = qobject_cast<TextItem*>(item);
    if (!x) { return; }

    beginProgrammaticUpdate();

    m_editLabel->setText(x->label());
    m_btnTextColor->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(x->textColor().name(QColor::HexArgb)));
    m_spinFontSize->setValue(x->fontSize());
    m_chkBold->setChecked(x->bold());
    m_editSuffix->setText(x->suffix());
    m_spinDecimals->setValue(x->decimals());
    m_editIdleText->setText(x->idleText());
    m_spinMinValid->setValue(x->minValidValue());

    endProgrammaticUpdate();
}

void TextItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Text Item"));

    // Label
    m_editLabel = new QLineEdit(this);
    addRow(QStringLiteral("Label"), m_editLabel);
    connect(m_editLabel, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setLabel(m_editLabel->text());
        notifyChanged();
    });

    // Text color
    m_btnTextColor = new QPushButton(this);
    m_btnTextColor->setFixedSize(40, 18);
    auto applyTextColor = [this](const QColor& c) {
        m_btnTextColor->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    connect(m_btnTextColor, &QPushButton::clicked, this, [this, applyTextColor]() {
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->textColor(), this);
        if (chosen.isValid()) { x->setTextColor(chosen); applyTextColor(chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Text color"), m_btnTextColor);

    // Font size
    m_spinFontSize = makeIntRow(QStringLiteral("Font size"), 4, 128);
    connect(m_spinFontSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setFontSize(v);
        notifyChanged();
    });

    // Bold
    m_chkBold = makeCheckRow(QStringLiteral("Bold"));
    connect(m_chkBold, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setBold(on);
        notifyChanged();
    });

    // Suffix
    m_editSuffix = new QLineEdit(this);
    addRow(QStringLiteral("Suffix"), m_editSuffix);
    connect(m_editSuffix, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setSuffix(m_editSuffix->text());
        notifyChanged();
    });

    // Decimals
    m_spinDecimals = makeIntRow(QStringLiteral("Decimals"), 0, 6);
    connect(m_spinDecimals, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setDecimals(v);
        notifyChanged();
    });

    // Idle text
    m_editIdleText = new QLineEdit(this);
    addRow(QStringLiteral("Idle text"), m_editIdleText);
    connect(m_editIdleText, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setIdleText(m_editIdleText->text());
        notifyChanged();
    });

    // Min valid value
    m_spinMinValid = makeDoubleRow(QStringLiteral("Min valid"), -200.0, 200.0, 1.0, 1);
    connect(m_spinMinValid, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setMinValidValue(v);
        notifyChanged();
    });
}

} // namespace NereusSDR
