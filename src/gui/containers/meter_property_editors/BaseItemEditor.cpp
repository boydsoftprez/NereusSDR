#include "BaseItemEditor.h"

#include "../../meters/MeterItem.h"
#include "../../meters/MeterPoller.h"
#include "../MmioVariablePickerPopup.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

namespace NereusSDR {

namespace {

// Shared style tokens with ContainerSettingsDialog — kept local to
// avoid cross-include churn.
constexpr const char* kSpinStyle =
    "QDoubleSpinBox, QSpinBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 1px 4px; min-height: 18px;"
    "}";

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

constexpr const char* kCheckStyle =
    "QCheckBox { color: #c8d8e8; }";

constexpr const char* kLabelStyle =
    "QLabel { color: #8090a0; font-size: 10px; }";

constexpr const char* kHeaderStyle =
    "QLabel { color: #8aa8c0; font-weight: bold; font-size: 11px;"
    "  background: #1a2a38; padding: 2px 4px; }";

// Phase 3G-6 block 4b: minimum widths so fields don't shrink-wrap
// to their default size.
constexpr int kDefaultFieldWidth = 140;

} // namespace

BaseItemEditor::BaseItemEditor(QWidget* parent)
    : QWidget(parent)
{
    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(6, 6, 6, 6);
    m_root->setSpacing(4);

    m_form = new QFormLayout();
    m_form->setContentsMargins(0, 0, 0, 0);
    m_form->setSpacing(3);
    m_form->setLabelAlignment(Qt::AlignRight);
    m_root->addLayout(m_form);

    buildBaseForm();
    // Subclasses append their own rows after the base form. We can
    // call the virtual here because subclasses override and append
    // their widgets to m_form, not re-run base setup.
    // NOTE: Because this is called from the base constructor, the
    // subclass vtable is not active. Subclasses must explicitly
    // call buildTypeSpecific() at the end of their own ctor.

    m_root->addStretch();
}

void BaseItemEditor::buildBaseForm()
{
    addHeader(QStringLiteral("Common"));

    m_spinX = makeDoubleRow(QStringLiteral("X"), 0.0, 1.0);
    m_spinY = makeDoubleRow(QStringLiteral("Y"), 0.0, 1.0);
    m_spinW = makeDoubleRow(QStringLiteral("W"), 0.0, 1.0);
    m_spinH = makeDoubleRow(QStringLiteral("H"), 0.0, 1.0);

    m_comboBinding = new QComboBox(this);
    m_comboBinding->setStyleSheet(kComboStyle);
    m_comboBinding->setMinimumWidth(kDefaultFieldWidth);
    m_comboBinding->setToolTip(QStringLiteral(
        "Built-in meter source: WDSP RX/TX channels, PA hardware "
        "(volts/amps/temp), and rotator readings. For external MMIO "
        "endpoint variables use the MMIO Variable\u2026 button."));
    populateBindingCombo();
    connect(m_comboBinding, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setBindingId(m_comboBinding->currentData().toInt());
        notifyChanged();
    });
    // Phase 3G-6 block 5: Binding row composes the WDSP binding
    // combo with a "Variable…" button that opens the MMIO picker.
    // The button text flips to the picked variable name after a
    // binding is set, or back to "Variable…" when cleared.
    auto* bindingRow = new QWidget(this);
    auto* bindingLay = new QHBoxLayout(bindingRow);
    bindingLay->setContentsMargins(0, 0, 0, 0);
    bindingLay->setSpacing(4);
    bindingLay->addWidget(m_comboBinding, 1);
    auto* btnVariable = new QPushButton(QStringLiteral("MMIO Variable\u2026"), bindingRow);
    btnVariable->setToolTip(QStringLiteral(
        "Bind this item to an MMIO endpoint variable. For built-in "
        "WDSP / PA / rotator meters use the dropdown to the left."));
    bindingLay->addWidget(btnVariable);
    connect(btnVariable, &QPushButton::clicked, this, [this, btnVariable]() {
        if (!m_item) { return; }
        MmioVariablePickerPopup dlg(m_item->mmioGuid(),
                                     m_item->mmioVariable(),
                                     this);
        if (dlg.exec() != QDialog::Accepted) { return; }
        if (dlg.wasCleared()) {
            m_item->clearMmioBinding();
            btnVariable->setText(QStringLiteral("Variable\u2026"));
        } else {
            m_item->setMmioBinding(dlg.selectedGuid(), dlg.selectedVariable());
            btnVariable->setText(dlg.selectedVariable());
        }
        notifyChanged();
    });
    addRow(QStringLiteral("Binding"), bindingRow);

    m_spinZ = makeIntRow(QStringLiteral("Z-order"), 0, 999);

    m_chkOnlyRx = makeCheckRow(QStringLiteral("Only when RX"));
    m_chkOnlyTx = makeCheckRow(QStringLiteral("Only when TX"));
    m_spinDisplayGroup = makeIntRow(QStringLiteral("Display group"), 0, 15);

    // Wire base spin/check changes to the item.
    connect(m_spinX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setRect(static_cast<float>(v), m_item->y(),
                        m_item->itemWidth(), m_item->itemHeight());
        notifyChanged();
    });
    connect(m_spinY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setRect(m_item->x(), static_cast<float>(v),
                        m_item->itemWidth(), m_item->itemHeight());
        notifyChanged();
    });
    connect(m_spinW, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setRect(m_item->x(), m_item->y(),
                        static_cast<float>(v), m_item->itemHeight());
        notifyChanged();
    });
    connect(m_spinH, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setRect(m_item->x(), m_item->y(),
                        m_item->itemWidth(), static_cast<float>(v));
        notifyChanged();
    });
    connect(m_spinZ, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setZOrder(v);
        notifyChanged();
    });
    connect(m_chkOnlyRx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setOnlyWhenRx(on);
        notifyChanged();
    });
    connect(m_chkOnlyTx, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setOnlyWhenTx(on);
        notifyChanged();
    });
    connect(m_spinDisplayGroup, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate() || !m_item) { return; }
        m_item->setDisplayGroup(v);
        notifyChanged();
    });
}

void BaseItemEditor::populateBindingCombo()
{
    struct B { int id; const char* label; };
    // Order mirrors MeterPoller.h. Block 5 adds MMIO variables at
    // the tail when ExternalVariableEngine is wired in.
    static const B kBindings[] = {
        {MeterBinding::SignalPeak,   "RX: Signal Peak"},
        {MeterBinding::SignalAvg,    "RX: Signal Avg"},
        {MeterBinding::AdcPeak,      "RX: ADC Peak"},
        {MeterBinding::AdcAvg,       "RX: ADC Avg"},
        {MeterBinding::AgcGain,      "RX: AGC Gain"},
        {MeterBinding::AgcPeak,      "RX: AGC Peak"},
        {MeterBinding::AgcAvg,       "RX: AGC Avg"},
        {MeterBinding::SignalMaxBin, "RX: Signal Max Bin"},
        {MeterBinding::PbSnr,        "RX: PB SNR"},
        {MeterBinding::TxPower,      "TX: Forward Power"},
        {MeterBinding::TxReversePower, "TX: Reverse Power"},
        {MeterBinding::TxSwr,        "TX: SWR"},
        {MeterBinding::TxMic,        "TX: Mic"},
        {MeterBinding::TxComp,       "TX: Compressor"},
        {MeterBinding::TxAlc,        "TX: ALC"},
        {MeterBinding::TxEq,         "TX: EQ"},
        {MeterBinding::TxLeveler,    "TX: Leveler"},
        {MeterBinding::TxLevelerGain,"TX: Leveler Gain"},
        {MeterBinding::TxAlcGain,    "TX: ALC Gain"},
        {MeterBinding::TxAlcGroup,   "TX: ALC Group"},
        {MeterBinding::TxCfc,        "TX: CFC"},
        {MeterBinding::TxCfcGain,    "TX: CFC Gain"},
        {MeterBinding::HwVolts,      "HW: Volts"},
        {MeterBinding::HwAmps,       "HW: Amps"},
        {MeterBinding::HwTemperature,"HW: Temperature"},
        {MeterBinding::RotatorAz,    "Rotator: Azimuth"},
        {MeterBinding::RotatorEle,   "Rotator: Elevation"},
    };
    m_comboBinding->addItem(QStringLiteral("(none)"), -1);
    for (const auto& b : kBindings) {
        m_comboBinding->addItem(QString::fromLatin1(b.label), b.id);
    }
}

void BaseItemEditor::setItem(MeterItem* item)
{
    m_item = item;
    setEnabled(item != nullptr);
    if (!item) { return; }
    loadBaseFields();
}

void BaseItemEditor::loadBaseFields()
{
    beginProgrammaticUpdate();
    m_spinX->setValue(m_item->x());
    m_spinY->setValue(m_item->y());
    m_spinW->setValue(m_item->itemWidth());
    m_spinH->setValue(m_item->itemHeight());
    m_spinZ->setValue(m_item->zOrder());
    const int idx = m_comboBinding->findData(m_item->bindingId());
    m_comboBinding->setCurrentIndex(idx >= 0 ? idx : 0);
    m_chkOnlyRx->setChecked(m_item->onlyWhenRx());
    m_chkOnlyTx->setChecked(m_item->onlyWhenTx());
    m_spinDisplayGroup->setValue(m_item->displayGroup());
    endProgrammaticUpdate();
}

QDoubleSpinBox* BaseItemEditor::makeDoubleRow(const QString& label,
                                              double min, double max,
                                              double step, int decimals)
{
    auto* spin = new QDoubleSpinBox(this);
    spin->setStyleSheet(kSpinStyle);
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setMinimumWidth(kDefaultFieldWidth);
    addRow(label, spin);
    return spin;
}

QSpinBox* BaseItemEditor::makeIntRow(const QString& label, int min, int max)
{
    auto* spin = new QSpinBox(this);
    spin->setStyleSheet(kSpinStyle);
    spin->setRange(min, max);
    spin->setMinimumWidth(kDefaultFieldWidth);
    addRow(label, spin);
    return spin;
}

QCheckBox* BaseItemEditor::makeCheckRow(const QString& label)
{
    auto* chk = new QCheckBox(this);
    chk->setStyleSheet(kCheckStyle);
    addRow(label, chk);
    return chk;
}

void BaseItemEditor::addRow(const QString& label, QWidget* widget)
{
    auto* lbl = new QLabel(label, this);
    lbl->setStyleSheet(kLabelStyle);
    m_form->addRow(lbl, widget);
}

void BaseItemEditor::addHeader(const QString& text)
{
    auto* hdr = new QLabel(text, this);
    hdr->setStyleSheet(kHeaderStyle);
    m_form->addRow(hdr);
}

void BaseItemEditor::notifyChanged()
{
    if (!isProgrammaticUpdate()) {
        emit propertyChanged();
    }
}

} // namespace NereusSDR
