#include "HistoryGraphItemEditor.h"
#include "../../meters/HistoryGraphItem.h"
#include "../../meters/MeterPoller.h"

#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QColorDialog>

namespace NereusSDR {

HistoryGraphItemEditor::HistoryGraphItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void HistoryGraphItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(item);
    if (!x) { return; }

    auto applyColor = [](QPushButton* btn, const QColor& c) {
        btn->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                .arg(c.name(QColor::HexArgb)));
    };

    beginProgrammaticUpdate();

    m_spinCapacity->setValue(x->capacity());
    applyColor(m_btnLineColor0, x->lineColor0());
    applyColor(m_btnLineColor1, x->lineColor1());
    m_chkShowGrid->setChecked(x->showGrid());
    m_chkAutoScale0->setChecked(x->autoScale0());
    m_chkAutoScale1->setChecked(x->autoScale1());
    m_chkShowScale0->setChecked(x->showScale0());
    m_chkShowScale1->setChecked(x->showScale1());

    const int idx = m_comboBinding1->findData(x->bindingId1());
    m_comboBinding1->setCurrentIndex(idx >= 0 ? idx : 0);

    endProgrammaticUpdate();
}

void HistoryGraphItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("History Graph"));

    m_spinCapacity = makeIntRow(QStringLiteral("Capacity"), 10, 3000);
    connect(m_spinCapacity, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setCapacity(v);
        notifyChanged();
    });

    // Line colors
    m_btnLineColor0 = new QPushButton(this);
    m_btnLineColor0->setFixedSize(40, 18);
    connect(m_btnLineColor0, &QPushButton::clicked, this, [this]() {
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->lineColor0(), this);
        if (chosen.isValid()) {
            x->setLineColor0(chosen);
            m_btnLineColor0->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Line color 0"), m_btnLineColor0);

    m_btnLineColor1 = new QPushButton(this);
    m_btnLineColor1->setFixedSize(40, 18);
    connect(m_btnLineColor1, &QPushButton::clicked, this, [this]() {
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        const QColor chosen = QColorDialog::getColor(x->lineColor1(), this);
        if (chosen.isValid()) {
            x->setLineColor1(chosen);
            m_btnLineColor1->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                    .arg(chosen.name(QColor::HexArgb)));
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Line color 1"), m_btnLineColor1);

    // Grid and scaling
    m_chkShowGrid = makeCheckRow(QStringLiteral("Show grid"));
    connect(m_chkShowGrid, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setShowGrid(on);
        notifyChanged();
    });

    m_chkAutoScale0 = makeCheckRow(QStringLiteral("Auto scale axis 0"));
    connect(m_chkAutoScale0, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setAutoScale0(on);
        notifyChanged();
    });

    m_chkAutoScale1 = makeCheckRow(QStringLiteral("Auto scale axis 1"));
    connect(m_chkAutoScale1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setAutoScale1(on);
        notifyChanged();
    });

    m_chkShowScale0 = makeCheckRow(QStringLiteral("Show scale axis 0"));
    connect(m_chkShowScale0, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setShowScale0(on);
        notifyChanged();
    });

    m_chkShowScale1 = makeCheckRow(QStringLiteral("Show scale axis 1"));
    connect(m_chkShowScale1, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setShowScale1(on);
        notifyChanged();
    });

    // Second axis binding
    addHeader(QStringLiteral("Axis 1 Binding"));

    m_comboBinding1 = new QComboBox(this);
    struct B { int id; const char* label; };
    static const B kBindings[] = {
        {MeterBinding::SignalPeak,      "RX: Signal Peak"},
        {MeterBinding::SignalAvg,       "RX: Signal Avg"},
        {MeterBinding::AdcPeak,         "RX: ADC Peak"},
        {MeterBinding::AdcAvg,          "RX: ADC Avg"},
        {MeterBinding::AgcGain,         "RX: AGC Gain"},
        {MeterBinding::AgcPeak,         "RX: AGC Peak"},
        {MeterBinding::AgcAvg,          "RX: AGC Avg"},
        {MeterBinding::TxPower,         "TX: Forward Power"},
        {MeterBinding::TxReversePower,  "TX: Reverse Power"},
        {MeterBinding::TxSwr,           "TX: SWR"},
        {MeterBinding::TxMic,           "TX: Mic"},
        {MeterBinding::TxAlc,           "TX: ALC"},
        {MeterBinding::HwVolts,         "HW: Volts"},
        {MeterBinding::HwAmps,          "HW: Amps"},
        {MeterBinding::HwTemperature,   "HW: Temperature"},
    };
    m_comboBinding1->addItem(QStringLiteral("(none)"), -1);
    for (const auto& b : kBindings) {
        m_comboBinding1->addItem(QString::fromLatin1(b.label), b.id);
    }
    addRow(QStringLiteral("Binding axis 1"), m_comboBinding1);
    connect(m_comboBinding1, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isProgrammaticUpdate()) { return; }
        HistoryGraphItem* x = qobject_cast<HistoryGraphItem*>(m_item);
        if (!x) { return; }
        x->setBindingId1(m_comboBinding1->currentData().toInt());
        notifyChanged();
    });
}

} // namespace NereusSDR
