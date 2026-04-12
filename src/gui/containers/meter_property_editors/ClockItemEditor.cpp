#include "ClockItemEditor.h"
#include "../../meters/ClockItem.h"

#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

namespace {
void applyBtnColor(QPushButton* btn, const QColor& c)
{
    btn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(c.name(QColor::HexArgb)));
}
} // namespace

ClockItemEditor::ClockItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void ClockItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    ClockItem* c = qobject_cast<ClockItem*>(item);
    if (!c) { return; }

    beginProgrammaticUpdate();
    m_chkShow24Hour->setChecked(c->show24Hour());
    m_chkShowType->setChecked(c->showType());
    // ClockItem has no color read-back getters; buttons retain last value.
    endProgrammaticUpdate();
}

void ClockItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Clock"));

    m_chkShow24Hour = makeCheckRow(QStringLiteral("24-hour format"));
    connect(m_chkShow24Hour, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        c->setShow24Hour(on);
        notifyChanged();
    });

    m_chkShowType = makeCheckRow(QStringLiteral("Show UTC/local label"));
    connect(m_chkShowType, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        c->setShowType(on);
        notifyChanged();
    });

    addHeader(QStringLiteral("Colors"));

    m_btnTimeColour = new QPushButton(this);
    m_btnTimeColour->setFixedSize(40, 18);
    connect(m_btnTimeColour, &QPushButton::clicked, this, [this]() {
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::white, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { c->setTimeColour(chosen); applyBtnColor(m_btnTimeColour, chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Time color"), m_btnTimeColour);

    m_btnDateColour = new QPushButton(this);
    m_btnDateColour->setFixedSize(40, 18);
    connect(m_btnDateColour, &QPushButton::clicked, this, [this]() {
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::gray, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { c->setDateColour(chosen); applyBtnColor(m_btnDateColour, chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Date color"), m_btnDateColour);

    m_btnTitleColour = new QPushButton(this);
    m_btnTitleColour->setFixedSize(40, 18);
    connect(m_btnTitleColour, &QPushButton::clicked, this, [this]() {
        ClockItem* c = qobject_cast<ClockItem*>(m_item);
        if (!c) { return; }
        const QColor chosen = QColorDialog::getColor(
            Qt::darkGray, this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { c->setTypeTitleColour(chosen); applyBtnColor(m_btnTitleColour, chosen); notifyChanged(); }
    });
    addRow(QStringLiteral("Title color"), m_btnTitleColour);
}

} // namespace NereusSDR
