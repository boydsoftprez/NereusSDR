// =================================================================
// src/gui/setup/FilterPresetsSetupPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. Setup → DSP → Filter Presets page:
// lets users edit the name, low, and high cutoffs of every preset
// slot (F1..F10) for each DSP mode, reorder them, and reset to
// Thetis verbatim defaults.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
// =================================================================

#pragma once

#include "gui/SetupPage.h"
#include "core/WdspTypes.h"

#include <QComboBox>
#include <QTableWidget>

namespace NereusSDR {

class FilterPresetStore;

/// Setup → DSP → Filter Presets page.
///
/// Layout:
///   Mode selector combo (top)
///   Table: #  |  Name  |  Low (Hz)  |  High (Hz)  |  Width (Hz)  |  ↑↓ buttons
///   Buttons: Reset This Row | Reset All Rows for This Mode
///   Global:  Reset Every Mode to Thetis Defaults
class FilterPresetsSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit FilterPresetsSetupPage(FilterPresetStore* store,
                                    RadioModel* model = nullptr,
                                    QWidget* parent = nullptr);

    // SetupPage override — refreshes table from store on show.
    void syncFromModel() override;

private slots:
    void onModeChanged(int index);
    void onResetThisRow();
    void onResetThisMode();
    void onResetAll();
    void onPresetsChanged(NereusSDR::DSPMode mode);

private:
    void buildUi();
    void populateTable();
    void commitTableRow(int row);
    DSPMode currentMode() const;
    void moveRow(int row, int direction);  // direction: -1 (up), +1 (down)

    FilterPresetStore* m_store{nullptr};

    QComboBox*    m_modeCombo{nullptr};
    QTableWidget* m_table{nullptr};

    bool m_updatingTable{false};
};

} // namespace NereusSDR
