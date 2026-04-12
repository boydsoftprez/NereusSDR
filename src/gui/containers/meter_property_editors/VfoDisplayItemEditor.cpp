#include "VfoDisplayItemEditor.h"
#include "../../meters/VfoDisplayItem.h"

#include <QComboBox>
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

constexpr const char* kComboStyle =
    "QComboBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 2px 4px; min-height: 18px;"
    "}"
    "QComboBox QAbstractItemView {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #205070;"
    "  selection-background-color: #00b4d8;"
    "}";
} // namespace

VfoDisplayItemEditor::VfoDisplayItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void VfoDisplayItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(item);
    if (!v) { return; }

    beginProgrammaticUpdate();
    m_comboDisplayMode->setCurrentIndex(
        m_comboDisplayMode->findData(static_cast<int>(v->displayMode())));
    applyBtnColor(m_btnFreqColour,   v->frequencyColour());
    applyBtnColor(m_btnModeColour,   v->modeColour());
    applyBtnColor(m_btnFilterColour, v->filterColour());
    applyBtnColor(m_btnBandColour,   v->bandColour());
    applyBtnColor(m_btnRxColour,     v->rxColour());
    applyBtnColor(m_btnTxColour,     v->txColour());
    endProgrammaticUpdate();
}

void VfoDisplayItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("VFO Display"));

    m_comboDisplayMode = new QComboBox(this);
    m_comboDisplayMode->setStyleSheet(kComboStyle);
    m_comboDisplayMode->addItem(QStringLiteral("VFO A"),
        static_cast<int>(VfoDisplayItem::DisplayMode::VfoA));
    m_comboDisplayMode->addItem(QStringLiteral("VFO B"),
        static_cast<int>(VfoDisplayItem::DisplayMode::VfoB));
    m_comboDisplayMode->addItem(QStringLiteral("Both"),
        static_cast<int>(VfoDisplayItem::DisplayMode::VfoBoth));
    addRow(QStringLiteral("Display mode"), m_comboDisplayMode);
    connect(m_comboDisplayMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(m_item);
        if (!v) { return; }
        v->setDisplayMode(static_cast<VfoDisplayItem::DisplayMode>(
            m_comboDisplayMode->currentData().toInt()));
        notifyChanged();
    });

    addHeader(QStringLiteral("Colors"));

    auto makeColorBtn = [this](const QString& label, QPushButton*& out,
                               auto setter) {
        out = new QPushButton(this);
        out->setFixedSize(40, 18);
        QPushButton* btn = out;
        connect(btn, &QPushButton::clicked, this, [this, btn, setter]() {
            VfoDisplayItem* v = qobject_cast<VfoDisplayItem*>(m_item);
            if (!v) { return; }
            const QColor chosen = QColorDialog::getColor(
                Qt::gray, this, QString(), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid()) {
                (v->*setter)(chosen);
                applyBtnColor(btn, chosen);
                notifyChanged();
            }
        });
        addRow(label, out);
    };

    makeColorBtn(QStringLiteral("Frequency"),  m_btnFreqColour,   &VfoDisplayItem::setFrequencyColour);
    makeColorBtn(QStringLiteral("Mode"),       m_btnModeColour,   &VfoDisplayItem::setModeColour);
    makeColorBtn(QStringLiteral("Filter"),     m_btnFilterColour, &VfoDisplayItem::setFilterColour);
    makeColorBtn(QStringLiteral("Band"),       m_btnBandColour,   &VfoDisplayItem::setBandColour);
    makeColorBtn(QStringLiteral("RX label"),   m_btnRxColour,     &VfoDisplayItem::setRxColour);
    makeColorBtn(QStringLiteral("TX label"),   m_btnTxColour,     &VfoDisplayItem::setTxColour);
}

} // namespace NereusSDR
