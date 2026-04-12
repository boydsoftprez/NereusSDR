#include "TextOverlayItemEditor.h"
#include "../../meters/TextOverlayItem.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QColorDialog>

namespace NereusSDR {

TextOverlayItemEditor::TextOverlayItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void TextOverlayItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    TextOverlayItem* x = qobject_cast<TextOverlayItem*>(item);
    if (!x) { return; }

    auto applyColor = [](QPushButton* btn, const QColor& c) {
        btn->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };

    beginProgrammaticUpdate();

    // Populate what TextOverlayItem exposes getters for.
    // Note: fontFamily1/2, fontSize1/2, fontBold1/2, textBackColour1/2,
    // panelBackColour1/2, padding, showTextBack1/2 have no public getters —
    // those widgets are left at their default/previous values.
    m_editText1->setText(x->text1());
    applyColor(m_btnColour1, x->textColour1());
    m_editText2->setText(x->text2());
    applyColor(m_btnColour2, x->textColour2());
    m_chkShowPanel->setChecked(x->showBackPanel());
    m_spinScrollX->setValue(static_cast<double>(x->scrollX()));

    endProgrammaticUpdate();
}

void TextOverlayItemEditor::buildTypeSpecific()
{
    // ---- Line 1 ----
    addHeader(QStringLiteral("Line 1"));

    m_editText1 = new QLineEdit(this);
    addRow(QStringLiteral("Text 1"), m_editText1);
    connect(m_editText1, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setText1(m_editText1->text());
        notifyChanged();
    });

    m_btnColour1 = new QPushButton(this);
    m_btnColour1->setFixedSize(40, 18);
    connect(m_btnColour1, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->textColour1(), this);
        if (chosen.isValid()) {
            x->setTextColour1(chosen);
            m_btnColour1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Colour 1"), m_btnColour1);

    m_btnBackColour1 = new QPushButton(this);
    m_btnBackColour1->setFixedSize(40, 18);
    connect(m_btnBackColour1, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setTextBackColour1(chosen);
            m_btnBackColour1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Back colour 1"), m_btnBackColour1);

    m_chkShowBack1 = makeCheckRow(QStringLiteral("Show back 1"));
    connect(m_chkShowBack1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setShowTextBackColour1(on);
        notifyChanged();
    });

    m_fontCombo1 = new QFontComboBox(this);
    addRow(QStringLiteral("Font 1"), m_fontCombo1);
    connect(m_fontCombo1, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontFamily1(f.family());
        notifyChanged();
    });

    m_spinFontSize1 = makeDoubleRow(QStringLiteral("Font size 1"), 4.0, 200.0, 1.0, 1);
    connect(m_spinFontSize1, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontSize1(static_cast<float>(v));
        notifyChanged();
    });

    m_chkBold1 = makeCheckRow(QStringLiteral("Bold 1"));
    connect(m_chkBold1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontBold1(on);
        notifyChanged();
    });

    // ---- Line 2 ----
    addHeader(QStringLiteral("Line 2"));

    m_editText2 = new QLineEdit(this);
    addRow(QStringLiteral("Text 2"), m_editText2);
    connect(m_editText2, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setText2(m_editText2->text());
        notifyChanged();
    });

    m_btnColour2 = new QPushButton(this);
    m_btnColour2->setFixedSize(40, 18);
    connect(m_btnColour2, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->textColour2(), this);
        if (chosen.isValid()) {
            x->setTextColour2(chosen);
            m_btnColour2->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Colour 2"), m_btnColour2);

    m_btnBackColour2 = new QPushButton(this);
    m_btnBackColour2->setFixedSize(40, 18);
    connect(m_btnBackColour2, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setTextBackColour2(chosen);
            m_btnBackColour2->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Back colour 2"), m_btnBackColour2);

    m_chkShowBack2 = makeCheckRow(QStringLiteral("Show back 2"));
    connect(m_chkShowBack2, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setShowTextBackColour2(on);
        notifyChanged();
    });

    m_fontCombo2 = new QFontComboBox(this);
    addRow(QStringLiteral("Font 2"), m_fontCombo2);
    connect(m_fontCombo2, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontFamily2(f.family());
        notifyChanged();
    });

    m_spinFontSize2 = makeDoubleRow(QStringLiteral("Font size 2"), 4.0, 200.0, 1.0, 1);
    connect(m_spinFontSize2, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontSize2(static_cast<float>(v));
        notifyChanged();
    });

    m_chkBold2 = makeCheckRow(QStringLiteral("Bold 2"));
    connect(m_chkBold2, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setFontBold2(on);
        notifyChanged();
    });

    // ---- Panel ----
    addHeader(QStringLiteral("Panel"));

    m_chkShowPanel = makeCheckRow(QStringLiteral("Show panel"));
    connect(m_chkShowPanel, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setShowBackPanel(on);
        notifyChanged();
    });

    m_btnPanelBack1 = new QPushButton(this);
    m_btnPanelBack1->setFixedSize(40, 18);
    connect(m_btnPanelBack1, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setPanelBackColour1(chosen);
            m_btnPanelBack1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Panel back 1"), m_btnPanelBack1);

    m_btnPanelBack2 = new QPushButton(this);
    m_btnPanelBack2->setFixedSize(40, 18);
    connect(m_btnPanelBack2, &QPushButton::clicked, this, [this]() {
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(Qt::darkGray, this);
        if (chosen.isValid()) {
            x->setPanelBackColour2(chosen);
            m_btnPanelBack2->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Panel back 2"), m_btnPanelBack2);

    // ---- Layout ----
    addHeader(QStringLiteral("Layout"));

    m_spinScrollX = makeDoubleRow(QStringLiteral("Scroll X"), -10.0, 10.0, 0.1, 2);
    connect(m_spinScrollX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setScrollX(static_cast<float>(v));
        notifyChanged();
    });

    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), 0.0, 1.0, 0.01, 2);
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        TextOverlayItem* x = qobject_cast<TextOverlayItem*>(m_item);
        if (!x) { return; }
        x->setPadding(static_cast<float>(v));
        notifyChanged();
    });
}

} // namespace NereusSDR
