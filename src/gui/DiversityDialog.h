// src/gui/DiversityDialog.h
#pragma once

// =================================================================
// src/gui/DiversityDialog.h  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 — Created for Phase 3F Sub-Epic G Task 4 (simplified,
//                bench-minimum). NereusSDR-original code (no Thetis
//                port; no upstream attribution required). The full
//                Thetis Diversity UI (DiversityRadarWidget polar plot,
//                8-memory slots, direction finding, auto-find-null,
//                PS-pause UX) is explicitly deferred to a post-bench
//                polish iteration; this dialog ships only the operator
//                surface needed to engage diversity at the bench.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation
//                via Anthropic Claude Code.
//   2026-05-27 — Sub-Epic G Tasks 12 / 3 / 21: embed
//                DiversityRadarWidget (polar lobe), 8-memory slot row
//                M1-M8 with per-band AppSettings persistence, and the
//                MOX-active PS-HOLD overlay.  Still NereusSDR-original
//                in structure (Qt6 dialog vs C# WinForms).
// =================================================================

#include <QDialog>

class QCheckBox;
class QSlider;
class QLabel;

namespace NereusSDR {

class DiversityRadarWidget;
class RadioModel;
class SliceModel;

// DiversityDialog — bench-minimum Diversity surface (Phase 3F Sub-Epic G T4).
//
// Modeless singleton dialog opened from Tools > Diversity Dialog...
// (Ctrl+Shift+D). Operates on Slice A only (single-diversity bench path);
// per-slice and per-pan diversity expansion lands when the WDSP pdiv[]
// DivId allocator follow-up ships (see RadioModel T13 wire-up note).
//
// Controls:
//   - Enable checkbox (Slice A diversity engage)
//   - Phase slider (0..360 deg, 0.1-deg precision)
//   - Gain slider (-20..+20 dB, 0.1-dB precision)
//   - Status label
//
// State flows two-way:
//   user -> dialog -> SliceModel setters -> persistence (T2) AND
//                                           WDSP wrappers (T13)
//   external SliceModel change (e.g. persistence load) -> dialog refresh
//   via QSignalBlocker-guarded refreshFromSlice() to avoid echo loops.
class DiversityDialog : public QDialog {
    Q_OBJECT
public:
    explicit DiversityDialog(RadioModel* radioModel, QWidget* parent = nullptr);
    ~DiversityDialog() override;

private slots:
    void onEnableToggled(bool on);
    void onPhaseChanged(int sliderValue);
    void onGainChanged(int sliderValue);
    void refreshFromSlice();

private:
    SliceModel* sliceA() const;  // m_radioModel->slices().value(0), nullptr-safe

    RadioModel* m_radioModel{nullptr};

    QCheckBox*            m_enableBox{nullptr};
    QSlider*              m_phaseSlider{nullptr};
    QSlider*              m_gainSlider{nullptr};
    QLabel*               m_phaseLabel{nullptr};
    QLabel*               m_gainLabel{nullptr};
    QLabel*               m_statusLabel{nullptr};
    // Phase 3F Sub-Epic G Task 12: polar sensitivity radar embed.
    DiversityRadarWidget* m_radar{nullptr};
};

} // namespace NereusSDR
