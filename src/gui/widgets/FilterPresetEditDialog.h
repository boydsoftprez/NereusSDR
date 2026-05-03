// =================================================================
// src/gui/widgets/FilterPresetEditDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. Modal dialog for editing a single filter
// preset (name + low + high) via right-click on a filter button.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code (Stage C2 filter preset editor).
// =================================================================

#pragma once

#include "models/FilterPresetStore.h"
#include "core/WdspTypes.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

namespace NereusSDR {

/// Small modal dialog for editing a single filter preset.
/// Opened by right-click → "Edit this preset…" on a filter button
/// in RxApplet or VfoWidget.
///
/// Save   → calls FilterPresetStore::setPreset(mode, slot, edited).
/// Reset  → calls FilterPresetStore::resetPreset(mode, slot).
/// Cancel → no-op.
class FilterPresetEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterPresetEditDialog(FilterPresetStore* store,
                                    DSPMode mode,
                                    int slot,
                                    QWidget* parent = nullptr);

private slots:
    void onSave();
    void onResetToDefault();

private:
    void buildUi(const FilterPreset& current);
    void updateWidth();

    FilterPresetStore* m_store{nullptr};
    DSPMode            m_mode{DSPMode::USB};
    int                m_slot{0};

    QLineEdit* m_nameEdit{nullptr};
    QSpinBox*  m_lowSpin{nullptr};
    QSpinBox*  m_highSpin{nullptr};
    QLabel*    m_widthLbl{nullptr};
};

} // namespace NereusSDR
