// =================================================================
// src/gui/widgets/AntennaPickerMenu.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Right-click antenna picker
// submenu with chain-consequence hints for Phase 3F multi-pan UI.
// See Phase 3F design doc section 11 (UI Atlas Surfaces) and
// docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 5.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#pragma once

#include <QMenu>
#include <QStringList>

namespace NereusSDR {

class SliceModel;
class AlexController;
struct BoardCapabilities;

/// Right-click antenna picker submenu shown from VfoWidget's context
/// menu (per-slice). Lists ANT1/ANT2/ANT3 (limited by
/// BoardCapabilities::antennaInputCount) with chain-consequence hints
/// (e.g. "Chain N - current" / "(switches chain)"). Emits
/// `antennaSelected(sliceIndex, antennaName)` on user pick; consumer
/// applies the change through AlexController.
class AntennaPickerMenu : public QMenu {
    Q_OBJECT
public:
    AntennaPickerMenu(SliceModel* slice, AlexController* alex,
                       const BoardCapabilities& caps, QWidget* parent = nullptr);
    ~AntennaPickerMenu() override;

signals:
    void antennaSelected(int sliceIndex, const QString& antennaName);
};

} // namespace NereusSDR
