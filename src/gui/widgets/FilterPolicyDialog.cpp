// =================================================================
// src/gui/widgets/FilterPolicyDialog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Per Phase 3F UI atlas plan
// (docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 3). HPF checkbox is scaffolded; binding to AlexController HPF
// state lands in Sub-Epic G diversity polish.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#include "gui/widgets/FilterPolicyDialog.h"

#include "core/accessories/AlexController.h"
#include "gui/StyleConstants.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QStringLiteral>
#include <QVBoxLayout>

namespace NereusSDR {

FilterPolicyDialog::FilterPolicyDialog(int chainIndex, AlexController* alex, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Chain %1 - Filter Policy").arg(chainIndex));
    setStyleSheet(QStringLiteral("background: %1; color: %2;")
                      .arg(QLatin1String(Style::kPanelBg),
                           QLatin1String(Style::kTextPrimary)));
    setFixedWidth(420);

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(14, 14, 14, 14);

    // Current state group
    auto* stateGroup = new QGroupBox(QStringLiteral("Current state"), this);
    stateGroup->setStyleSheet(QLatin1String(Style::kGroupBoxStyle));
    auto* stateLayout = new QVBoxLayout(stateGroup);
    const auto& state = alex->adcState(chainIndex);
    const QString effectiveText =
        (state.effective == AlexController::BpfEffective::Filtered)
            ? QStringLiteral("Filtered")
            : (state.effective == AlexController::BpfEffective::WidebandLocked)
                  ? QStringLiteral("BYPASS (wideband)")
                  : QStringLiteral("BYPASS");
    auto* stateLbl = new QLabel(
        QStringLiteral("Effective: %1\nReason: %2").arg(effectiveText, state.reasonText),
        stateGroup);
    stateLbl->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px;"));
    stateLbl->setWordWrap(true);
    stateLayout->addWidget(stateLbl);
    main->addWidget(stateGroup);

    // BPF mode group
    auto* modeGroup = new QGroupBox(QStringLiteral("BPF mode"), this);
    modeGroup->setStyleSheet(QLatin1String(Style::kGroupBoxStyle));
    auto* modeLayout = new QVBoxLayout(modeGroup);
    auto* btnGroup = new QButtonGroup(this);

    auto* autoBtn = new QRadioButton(
        QStringLiteral("Auto - Filter when single-band, bypass when multi-band"), modeGroup);
    auto* forceBandBtn = new QRadioButton(
        QStringLiteral("Force filter (TX-bound band)"), modeGroup);
    auto* forceByBtn = new QRadioButton(
        QStringLiteral("Force bypass - Always wideband"), modeGroup);
    btnGroup->addButton(autoBtn, int(AlexController::BpfMode::Auto));
    btnGroup->addButton(forceBandBtn, int(AlexController::BpfMode::ForceBand));
    btnGroup->addButton(forceByBtn, int(AlexController::BpfMode::ForceBypass));

    autoBtn->setStyleSheet(QLatin1String(Style::kRadioButtonStyle));
    forceBandBtn->setStyleSheet(QLatin1String(Style::kRadioButtonStyle));
    forceByBtn->setStyleSheet(QLatin1String(Style::kRadioButtonStyle));

    switch (alex->bpfMode(chainIndex)) {
        case AlexController::BpfMode::Auto:        autoBtn->setChecked(true); break;
        case AlexController::BpfMode::ForceBand:   forceBandBtn->setChecked(true); break;
        case AlexController::BpfMode::ForceBypass: forceByBtn->setChecked(true); break;
    }

    modeLayout->addWidget(autoBtn);
    modeLayout->addWidget(forceBandBtn);
    modeLayout->addWidget(forceByBtn);
    main->addWidget(modeGroup);

    // HPF checkbox - scaffolded for Sub-Epic G diversity polish.
    auto* hpfBox = new QCheckBox(QStringLiteral("HPF (broadcast band reject) enabled"), this);
    hpfBox->setChecked(true);  // Default; bind to AlexController HPF state in Sub-Epic G.
    hpfBox->setStyleSheet(QLatin1String(Style::kCheckBoxStyle));
    main->addWidget(hpfBox);

    // Footer buttons
    auto* footer = new QHBoxLayout();
    footer->addStretch(1);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footer->addWidget(cancelBtn);
    auto* applyBtn = new QPushButton(QStringLiteral("Apply"), this);
    applyBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
    connect(applyBtn, &QPushButton::clicked, this,
            [this, alex, chainIndex, btnGroup]() {
        alex->setBpfMode(chainIndex,
                         static_cast<AlexController::BpfMode>(btnGroup->checkedId()));
        accept();
    });
    footer->addWidget(applyBtn);
    main->addLayout(footer);
}

FilterPolicyDialog::~FilterPolicyDialog() = default;

} // namespace NereusSDR
