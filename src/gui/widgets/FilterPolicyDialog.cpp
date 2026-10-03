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
//   2026-09-24 R-R3-49 / R-R3-21: the HPF checkbox is hidden until built
//              (hpf-bcast). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//              Claude Code.
//   2026-09-24 R-R3-46 / R-R3-21: in a remote window the policy can be
//              changed on the Core (radioHardwareVersion 4, the
//              setAlexBpfMode request); an older Core keeps a plain reason
//              and nothing is sent. J.J. Boyd (KG4VCF), AI-assisted via
//              Anthropic Claude Code.
//   2026-09-30 Shared-input filters (ruling (d)): the state shows the
//              receive low-pass reason when a slice holds it. J.J. Boyd
//              (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 The shared-input reason is labeled "Shared input", since on
//              the HL2 it can be only the high-pass sentence. J.J. Boyd
//              (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#include "gui/widgets/FilterPolicyDialog.h"

#include "core/accessories/AlexController.h"
#include "core/session/IStationLink.h"
#include "gui/OperatorReasonText.h"
#include "gui/StyleConstants.h"
#include "gui/UnbuiltFeatures.h"
#include "models/RadioModel.h"

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

FilterPolicyDialog::FilterPolicyDialog(int chainIndex, RadioModel* model, QWidget* parent)
    : FilterPolicyDialog(chainIndex, &model->alexControllerMutable(), parent,
                         model->role() == RadioModel::Role::Remote
                             ? &model->filterChainState(chainIndex) : nullptr,
                         model->filterChainStateAvailable(chainIndex),
                         model->role() == RadioModel::Role::Remote
                             ? model->stationLink() : nullptr)
{
}

FilterPolicyDialog::FilterPolicyDialog(int chainIndex, AlexController* alex, QWidget* parent,
                                       const AlexController::AlexAdcState* stationState,
                                       bool stationStateAvailable, IStationLink* station)
    : QDialog(parent)
{
    // R-R3-46 / R-R3-21: a remote window changes the Core's policy only when
    // the Core takes the change from this app (radioHardwareVersion 4).
    const bool remote = stationState != nullptr;
    const bool remoteEditable = remote && stationStateAvailable && station != nullptr
        && station->filterPolicyEditAvailable();
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
    const auto& state = stationState ? *stationState : alex->adcState(chainIndex);
    const QString effectiveText =
        (state.effective == AlexController::BpfEffective::Filtered)
            ? QStringLiteral("Filtered")
            : (state.effective == AlexController::BpfEffective::WidebandLocked)
                  ? QStringLiteral("BYPASS (wideband)")
                  : QStringLiteral("BYPASS");
    auto* stateLbl = new QLabel(
        stationState && !stationStateAvailable
            ? tr("Core filter state is not available.")
            : QStringLiteral("Effective: %1\nReason: %2").arg(effectiveText, state.reasonText)
                  // Shared-input filters, ruling (d): the receive low-pass on
                  // this input when a slice holds it, and on the HL2 the
                  // broadcast-band high-pass when a slice needs it off. The
                  // reason can be that high-pass sentence alone, so the label
                  // names the input rather than the low-pass.
                  + (state.lowPassReason.isEmpty()
                         ? QString()
                         : QStringLiteral("\nShared input: %1").arg(state.lowPassReason)),
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
    modeGroup->setObjectName(QStringLiteral("filterPolicyModeGroup"));
    autoBtn->setObjectName(QStringLiteral("filterPolicyAuto"));
    forceBandBtn->setObjectName(QStringLiteral("filterPolicyForceFilter"));
    forceByBtn->setObjectName(QStringLiteral("filterPolicyForceBypass"));
    btnGroup->addButton(autoBtn, int(AlexController::BpfMode::Auto));
    btnGroup->addButton(forceBandBtn, int(AlexController::BpfMode::ForceBand));
    btnGroup->addButton(forceByBtn, int(AlexController::BpfMode::ForceBypass));

    autoBtn->setStyleSheet(QLatin1String(Style::kRadioButtonStyle));
    forceBandBtn->setStyleSheet(QLatin1String(Style::kRadioButtonStyle));
    forceByBtn->setStyleSheet(QLatin1String(Style::kRadioButtonStyle));

    switch (state.mode) {
        case AlexController::BpfMode::Auto:        autoBtn->setChecked(true); break;
        case AlexController::BpfMode::ForceBand:   forceBandBtn->setChecked(true); break;
        case AlexController::BpfMode::ForceBypass: forceByBtn->setChecked(true); break;
    }

    modeLayout->addWidget(autoBtn);
    modeLayout->addWidget(forceBandBtn);
    modeLayout->addWidget(forceByBtn);
    modeGroup->setEnabled(!remote || remoteEditable);
    modeGroup->setVisible(!remote || stationStateAvailable);
    main->addWidget(modeGroup);
    QLabel* note = nullptr;
    if (remote && stationStateAvailable) {
        const QString noteText = remoteEditable
            ? tr("This is the Core's filter policy. A change applies on the Core.")
            : station != nullptr
                ? OperatorReasonText::forDisplay(station->filterPolicyUnavailableReason())
                : tr("Connect to the Core to change the filter policy.");
        note = new QLabel(noteText, this);
        note->setObjectName(QStringLiteral("filterPolicyNote"));
        note->setWordWrap(true);
        main->addWidget(note);
    }

    // HPF checkbox - scaffolded for Sub-Epic G diversity polish.
    auto* hpfBox = new QCheckBox(QStringLiteral("HPF (broadcast band reject) enabled"), this);
    hpfBox->setChecked(true);  // Default; bind to AlexController HPF state in Sub-Epic G.
    hpfBox->setStyleSheet(QLatin1String(Style::kCheckBoxStyle));
    hpfBox->setVisible(stationState == nullptr);
    hpfBox->setObjectName(QStringLiteral("filterPolicyHpfCheck"));
    // R-R3-49 (hpf-bcast): nothing reads this checkbox yet; hidden until the
    // broadcast band reject filter is built.
    UnbuiltFeatures::hideUnlessBuilt(hpfBox, UnbuiltFeature::HpfBroadcastReject);
    main->addWidget(hpfBox);

    // Footer buttons
    auto* footer = new QHBoxLayout();
    footer->addStretch(1);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    cancelBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footer->addWidget(cancelBtn);
    auto* applyBtn = new QPushButton(remote && !remoteEditable ? tr("Close") : tr("Apply"), this);
    applyBtn->setObjectName(QStringLiteral("filterPolicyApply"));
    applyBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
    connect(applyBtn, &QPushButton::clicked, this,
            [this, alex, chainIndex, btnGroup, remote, remoteEditable, station, note]() {
        if (remote) {
            // R-R3-46 / R-R3-21: the Core applies the policy and every
            // window follows its published chain state; nothing is changed
            // here on the way out. The choice shown is always sent, even
            // when it is the policy the dialog opened on: the Core's policy
            // may have changed since, and the Core takes an unchanged
            // policy as done.
            const int wanted = btnGroup->checkedId();
            if (!remoteEditable) { accept(); return; }
            const IStationLink::CommandOutcome outcome =
                station->requestFilterPolicy(chainIndex, wanted);
            if (!outcome.sent) {
                if (note) { note->setText(OperatorReasonText::forDisplay(outcome.reason)); }
                return;
            }
            accept();
            return;
        }
        alex->setBpfMode(chainIndex,
                         static_cast<AlexController::BpfMode>(btnGroup->checkedId()));
        accept();
    });
    footer->addWidget(applyBtn);
    main->addLayout(footer);
}

FilterPolicyDialog::~FilterPolicyDialog() = default;

} // namespace NereusSDR
