// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I6): a colour dialog pick is
//                 recorded as the user's (markColourPicked). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "TextItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/MeterItem.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QComboBox>
#include <QFontDatabase>

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
    ItemColourLook::paintSwatch(m_btnTextColor, QStringLiteral("Text.textColor"), x->textColor());
    m_spinFontSize->setValue(x->fontSize());
    m_chkBold->setChecked(x->bold());
    m_editSuffix->setText(x->suffix());
    m_spinDecimals->setValue(x->decimals());
    m_editIdleText->setText(x->idleText());
    m_spinMinValid->setValue(x->minValidValue());

    // Empty family is the application font (the first entry). A family not
    // installed here is added so the saved name still shows.
    int familyIndex = 0;
    if (!x->fontFamily().isEmpty()) {
        familyIndex = m_comboFontFamily->findData(x->fontFamily());
        if (familyIndex < 0) {
            m_comboFontFamily->addItem(x->fontFamily(), x->fontFamily());
            familyIndex = m_comboFontFamily->count() - 1;
        }
    }
    m_comboFontFamily->setCurrentIndex(familyIndex);
    m_chkItalic->setChecked(x->italic());
    m_chkCentred->setChecked(x->centred());

    endProgrammaticUpdate();
}

void TextItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Text Item"));

    // Label
    m_editLabel = new QLineEdit(this);
    m_editLabel->setToolTip(QStringLiteral(
        "Make the whole label %value% to show the reading, %valueWithType% to show "
        "it with its unit, or %group% to show the display group's name."));
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
        ItemColourLook::paintSwatch(m_btnTextColor, QStringLiteral("Text.textColor"), c);
    };
    connect(m_btnTextColor, &QPushButton::clicked, this, [this, applyTextColor]() {
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->textColor(), this);
        if (chosen.isValid()) { markColourPicked(QStringLiteral("Text.textColor")); x->setTextColor(chosen); applyTextColor(chosen); notifyChanged(); }
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

    // Font family
    m_comboFontFamily = new QComboBox(this);
    m_comboFontFamily->setObjectName(QStringLiteral("textFontFamily"));
    m_comboFontFamily->addItem(QStringLiteral("Default"), QString());
    for (const QString& family : QFontDatabase::families()) {
        m_comboFontFamily->addItem(family, family);
    }
    addRow(QStringLiteral("Font"), m_comboFontFamily);
    connect(m_comboFontFamily, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setFontFamily(m_comboFontFamily->currentData().toString());
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

    // Italic
    m_chkItalic = makeCheckRow(QStringLiteral("Italic"));
    m_chkItalic->setObjectName(QStringLiteral("textItalic"));
    connect(m_chkItalic, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setItalic(on);
        notifyChanged();
    });

    // Centred
    m_chkCentred = makeCheckRow(QStringLiteral("Centered"));
    m_chkCentred->setObjectName(QStringLiteral("textCentred"));
    connect(m_chkCentred, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextItem* x = qobject_cast<TextItem*>(m_item);
        if (!x) { return; }
        x->setCentred(on);
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
