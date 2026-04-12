#include "MagicEyeItemEditor.h"
#include "../../meters/MagicEyeItem.h"

#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QWidget>
#include <QColorDialog>
#include <QFileDialog>

namespace NereusSDR {

MagicEyeItemEditor::MagicEyeItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void MagicEyeItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    MagicEyeItem* x = qobject_cast<MagicEyeItem*>(item);
    if (!x) { return; }

    beginProgrammaticUpdate();

    m_btnGlowColor->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(x->glowColor().name(QColor::HexArgb)));
    m_editBezelPath->setText(x->bezelImagePath());

    endProgrammaticUpdate();
}

void MagicEyeItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Magic Eye"));

    // Glow color
    m_btnGlowColor = new QPushButton(this);
    m_btnGlowColor->setFixedSize(40, 18);
    auto applyGlowColor = [this](const QColor& c) {
        m_btnGlowColor->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };
    connect(m_btnGlowColor, &QPushButton::clicked, this, [this, applyGlowColor]() {
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->glowColor(), this);
        if (chosen.isValid()) { x->setGlowColor(chosen); applyGlowColor(chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Glow color"), m_btnGlowColor);

    // Bezel image path — line edit + Browse button in a container widget
    QWidget* bezelRow = new QWidget(this);
    QHBoxLayout* bezelLayout = new QHBoxLayout(bezelRow);
    bezelLayout->setContentsMargins(0, 0, 0, 0);
    bezelLayout->setSpacing(4);

    m_editBezelPath = new QLineEdit(bezelRow);
    bezelLayout->addWidget(m_editBezelPath);

    m_btnBrowseBezel = new QPushButton(QStringLiteral("Browse\u2026"), bezelRow);
    m_btnBrowseBezel->setFixedWidth(60);
    bezelLayout->addWidget(m_btnBrowseBezel);

    addRow(QStringLiteral("Bezel image"), bezelRow);

    connect(m_editBezelPath, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (!x) { return; }
        x->setBezelImagePath(m_editBezelPath->text());
        notifyChanged();
    });

    connect(m_btnBrowseBezel, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select Bezel Image"),
            QString(),
            QStringLiteral("Images (*.png *.jpg *.bmp *.svg);;All files (*)"));
        if (path.isEmpty()) { return; }
        MagicEyeItem* x = qobject_cast<MagicEyeItem*>(m_item);
        if (!x) { return; }
        x->setBezelImagePath(path);
        m_editBezelPath->setText(path);
        notifyChanged();
    });
}

} // namespace NereusSDR
