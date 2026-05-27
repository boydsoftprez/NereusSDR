// =================================================================
// src/gui/widgets/TxBoundConfirmDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Modal dialog shown when
// adding a new slice would require re-routing the TX-bound slice's
// chain to a different antenna (Phase 3F multi-pan UI). See Phase 3F
// design doc section 5 (antenna conflict policy) and
// docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 7.
//
// Consumer wire-up (RadioModel::addSliceOnPan conflict detection ->
// this dialog -> apply outcome to AlexController) lands when the
// antenna-auto-switch + conflict-resolve pipeline is built in
// Sub-Epic E Tasks 8-13.
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
#include <QVector>

namespace NereusSDR {

class SliceModel;

/// Modal dialog shown when adding a new slice would require re-routing
/// the TX-bound slice's chain to a different antenna. Per design
/// section 5. Three outcomes: Cancelled / UseExistingAntenna /
/// ConfirmReroute. `atRiskSlices` is reserved for a future iteration
/// that enumerates which slices would be affected by the re-route in
/// the body of the dialog.
class TxBoundConfirmDialog : public QDialog {
    Q_OBJECT
public:
    enum Outcome { Cancelled, UseExistingAntenna, ConfirmReroute };

    TxBoundConfirmDialog(const QString& proposedAntenna,
                          const QString& existingAntenna,
                          const QVector<SliceModel*>& atRiskSlices,
                          QWidget* parent = nullptr);

    Outcome outcome() const { return m_outcome; }

private:
    Outcome m_outcome {Cancelled};
};

} // namespace NereusSDR
