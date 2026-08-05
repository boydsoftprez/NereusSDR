// =================================================================
// src/gui/setup/PgxlInterlockPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native Setup -> Transmit -> PGXL Interlock page.
// See PgxlInterlockPage.h for full design notes.
//
// AI tooling: Anthropic Claude Code.
// =================================================================

#include "gui/setup/PgxlInterlockPage.h"
#include "models/RadioModel.h"
#include "core/TxInterlockPolicy.h"

#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

namespace NereusSDR {

PgxlInterlockPage::PgxlInterlockPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_policy(model ? model->txInterlockPolicy() : nullptr)
{
    buildUi();
    loadFromPolicy();
}

void PgxlInterlockPage::buildUi()
{
    // ── Scroll area wraps the content widget so the page handles small windows.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* topLay  = new QVBoxLayout(content);
    topLay->setContentsMargins(12, 12, 12, 12);
    topLay->setSpacing(14);

    // ── Section: TX Interlock Mode ───────────────────────────────────────────
    auto* modeBox = new QGroupBox("TX Interlock Mode");
    auto* modeForm = new QFormLayout(modeBox);
    modeForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("Disabled");  // index 0 -> TxInterlockPolicy::Disabled
    m_modeCombo->addItem("Warn");      // index 1 -> TxInterlockPolicy::Warn
    m_modeCombo->addItem("Block");     // index 2 -> TxInterlockPolicy::Block
    m_modeCombo->setToolTip(
        "Disabled: interlock never interferes with TX.\n"
        "Warn: TX proceeds but a warning toast is shown when the amplifier\n"
        "  is present but not in OPERATE (or SWR gate trips).\n"
        "Block: TX is prevented when the amplifier is present but not in\n"
        "  OPERATE (or SWR gate trips).\n"
        "Default: Disabled (matches AetherSDR behavior).");
    modeForm->addRow("Interlock Mode:", m_modeCombo);

    topLay->addWidget(modeBox);

    // ── Section: Grace Period ────────────────────────────────────────────────
    auto* graceBox  = new QGroupBox("Grace Period");
    auto* graceForm = new QFormLayout(graceBox);
    graceForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_graceSpinbox = new QSpinBox;
    m_graceSpinbox->setRange(0, 30000);
    m_graceSpinbox->setSingleStep(100);
    m_graceSpinbox->setSuffix(" ms");
    m_graceSpinbox->setToolTip(
        "Grace period (ms) after the amplifier transitions to OPERATE before\n"
        "the SWR gate is enforced. Ignores SWR spikes during PA warm-up so\n"
        "the gate does not nuisance-trip on PSU current ramps. Applied by\n"
        "TxInterlockPolicy::evaluateTxRequest against the OPERATE rising\n"
        "edge timestamp. Persisted as PGXL_TxInterlockGraceMs.\n"
        "Range: 0..30000 ms.  Default: 3000 ms.");
    graceForm->addRow("Grace Period:", m_graceSpinbox);

    topLay->addWidget(graceBox);

    // ── Section: SWR Gate ────────────────────────────────────────────────────
    auto* swrBox  = new QGroupBox("SWR Gate");
    auto* swrLay  = new QVBoxLayout(swrBox);

    m_swrGateCheckbox = new QCheckBox("Enable SWR Gate");
    m_swrGateCheckbox->setToolTip(
        "When enabled, a second interlock check fires if the current SWR\n"
        "exceeds the Max SWR limit below. The Warn/Block action follows the\n"
        "same Interlock Mode setting as the amplifier-state check.");
    swrLay->addWidget(m_swrGateCheckbox);

    auto* swrMaxRow = new QHBoxLayout;
    auto* swrMaxLabel = new QLabel("Max SWR:");
    m_swrGateMaxSpinbox = new QDoubleSpinBox;
    m_swrGateMaxSpinbox->setRange(1.0, 10.0);
    m_swrGateMaxSpinbox->setSingleStep(0.1);
    m_swrGateMaxSpinbox->setDecimals(1);
    m_swrGateMaxSpinbox->setToolTip(
        "SWR ratio above which the gate triggers.\n"
        "Only active when SWR Gate is enabled.\n"
        "Range: 1.0..10.0.  Default: 3.0.");
    swrMaxRow->addWidget(swrMaxLabel);
    swrMaxRow->addWidget(m_swrGateMaxSpinbox);
    swrMaxRow->addStretch();
    swrLay->addLayout(swrMaxRow);

    topLay->addWidget(swrBox);

    // ── Help text banner ─────────────────────────────────────────────────────
    m_helpText = new QLabel(
        "The TX Interlock policy protects the amplifier from accidental TX "
        "when the PGXL is present but not in OPERATE state (e.g. still in "
        "STANDBY or FAULT). Set Mode to Block to prevent keying until the "
        "amplifier is ready. Set to Warn to allow TX with a visible warning.\n\n"
        "Changes take effect immediately for all subsequent TX requests.");
    m_helpText->setWordWrap(true);
    m_helpText->setStyleSheet("color: #8aa8c0; font-size: 11px;");
    topLay->addWidget(m_helpText);

    topLay->addStretch();

    scroll->setWidget(content);

    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->addWidget(scroll);

    // ── Wire signals ─────────────────────────────────────────────────────────
    connect(m_modeCombo,       QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PgxlInterlockPage::onModeChanged);
    connect(m_graceSpinbox,    QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PgxlInterlockPage::onGraceChanged);
    connect(m_swrGateCheckbox, &QCheckBox::toggled,
            this, &PgxlInterlockPage::onSwrGateToggled);
    connect(m_swrGateMaxSpinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PgxlInterlockPage::onSwrGateMaxChanged);
}

void PgxlInterlockPage::loadFromPolicy()
{
    if (!m_policy) {
        return;
    }
    m_loading = true;

    switch (m_policy->mode()) {
    case TxInterlockPolicy::Warn:  m_modeCombo->setCurrentIndex(1); break;
    case TxInterlockPolicy::Block: m_modeCombo->setCurrentIndex(2); break;
    default:                       m_modeCombo->setCurrentIndex(0); break;
    }

    m_graceSpinbox->setValue(m_policy->graceMs());

    m_swrGateCheckbox->setChecked(m_policy->swrGateEnabled());
    m_swrGateMaxSpinbox->setValue(static_cast<double>(m_policy->swrGateMax()));
    m_swrGateMaxSpinbox->setEnabled(m_policy->swrGateEnabled());

    m_loading = false;
}

void PgxlInterlockPage::onModeChanged(int idx)
{
    if (m_loading || !m_policy) {
        return;
    }
    TxInterlockPolicy::Mode mode = TxInterlockPolicy::Disabled;
    if (idx == 1) {
        mode = TxInterlockPolicy::Warn;
    } else if (idx == 2) {
        mode = TxInterlockPolicy::Block;
    }
    m_policy->setMode(mode);
}

void PgxlInterlockPage::onGraceChanged(int ms)
{
    if (m_loading || !m_policy) {
        return;
    }
    m_policy->setGraceMs(ms);
}

void PgxlInterlockPage::onSwrGateToggled(bool on)
{
    if (m_loading || !m_policy) {
        return;
    }
    m_policy->setSwrGateEnabled(on);
    m_swrGateMaxSpinbox->setEnabled(on);
}

void PgxlInterlockPage::onSwrGateMaxChanged(double val)
{
    if (m_loading || !m_policy) {
        return;
    }
    m_policy->setSwrGateMax(static_cast<float>(val));
}

}  // namespace NereusSDR
