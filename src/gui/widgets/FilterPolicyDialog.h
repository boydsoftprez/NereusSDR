// =================================================================
// src/gui/widgets/FilterPolicyDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Per-chain BPF policy popup
// for Phase 3F multi-pan UI atlas. See Phase 3F design doc section
// 11 (UI Atlas Surfaces) and
// docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 3.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#pragma once

#include <QDialog>

namespace NereusSDR {

class AlexController;

/// Modal dialog opened from the WIDE badge or CH tag in the
/// SpectrumStatusOverlay (per-pan), and from VfoWidget's right-click
/// context menu (per-slice). Lets the operator override per-ADC BPF
/// policy (Auto / ForceBand / ForceBypass) and shows the current
/// effective state with the reason text computed by AlexController.
class FilterPolicyDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterPolicyDialog(int chainIndex, AlexController* alex, QWidget* parent = nullptr);
    ~FilterPolicyDialog() override;
};

} // namespace NereusSDR
