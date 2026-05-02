#pragma once

// =================================================================
// src/gui/setup/hardware/OcOutputsTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.designer.cs:tpOCHFControl,
//   tpOCSWLControl (tcOCOutputs container tab pages)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Initial placeholder stub. J.J. Boyd (KG4VCF).
//   2026-04-20 — Refactored into parent QTabWidget hosting two sub-sub-tabs:
//                HF (OcOutputsHfTab — full RX/TX matrix + actions + USB BCD
//                + Ext PA + live OC state) and SWL (placeholder for a
//                follow-up commit). Phase 3P-D Task 2. J.J. Boyd (KG4VCF).
//   2026-05-02 — P1 full-parity §4.3: User Dig Out group with 4 bit-toggle
//                checkboxes wired to TransmitModel::userDigOut, gated on
//                BoardCapabilities::hasPennyLane. J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Anthropic Claude Code.
// =================================================================

//=================================================================
// setup.designer.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <QVariant>
#include <QWidget>
#include <array>

class QCheckBox;
class QGroupBox;
class QTabWidget;

namespace NereusSDR {

class RadioModel;
class OcMatrix;
class OcOutputsHfTab;
class OcOutputsSwlTab;
struct RadioInfo;
struct BoardCapabilities;

// OcOutputsTab — parent "OC Outputs" tab in Hardware Config.
//
// Hosts two sub-sub-tabs that mirror Thetis tcOCOutputs:
//   0. HF  — OcOutputsHfTab (full RX/TX matrix + pin actions + USB BCD
//             + Ext PA + live OC pin state) — Phase 3P-D Task 2
//   1. SWL — placeholder; same matrix shape with SWL band plan — follow-up
//
// State is backed by an OcMatrix instance (Phase 3P-D Task 1) which handles
// per-MAC AppSettings persistence under hardware/<mac>/oc/...
//
// Source: Thetis tcOCOutputs (setup.designer.cs tpOCHFControl + tpOCSWLControl)
// [@501e3f5]
class OcOutputsTab : public QWidget {
    Q_OBJECT
public:
    explicit OcOutputsTab(RadioModel* model, QWidget* parent = nullptr);

    // Called by HardwarePage when a radio connects. Extracts the MAC address
    // and triggers OcMatrix::load() to hydrate per-MAC state.
    void populate(const RadioInfo& info, const BoardCapabilities& caps);

    // Called by HardwarePage on app restore. State is owned by OcMatrix
    // (AppSettings under hardware/<mac>/oc/...) — this is a no-op stub so
    // the HardwarePage API contract is met without extra coupling.
    void restoreSettings(const QMap<QString, QVariant>& settings);

    // ── Test seams (P1 full-parity §4.3 — User Dig Out) ──────────────────
    // Exposed for tst_board_capability_flag_wiring; not part of the
    // HardwarePage API contract.

    // Returns true iff the User Dig Out QGroupBox is currently visible
    // (uses isHidden() rather than isVisibleTo() to avoid the shown-
    // ancestor requirement that would force the test to show() the
    // widget).
    bool userDigOutGroupVisibleForTest() const;

    // Returns the checked state of the i'th User Dig Out checkbox
    // (i ∈ [0..3], i.e. bit 0 = Pin 1, ..., bit 3 = Pin 4).
    bool userDigOutCheckboxCheckedForTest(int bit) const;

    // Toggles the i'th checkbox programmatically (drives the checkbox's
    // setChecked which fires the same signal path as a user click).
    void toggleUserDigOutCheckboxForTest(int bit, bool checked);

signals:
    // Present for HardwarePage API compatibility. OcOutputsTab routes all
    // state through OcMatrix (AppSettings), so this signal is never emitted
    // from this tab — the write-through path used by other tabs does not
    // apply here. HardwarePage connects to it but will never receive a firing.
    void settingChanged(const QString& key, const QVariant& value);

private:
    // ── User Dig Out helpers (P1 full-parity §4.3) ─────────────────────────
    void buildUserDigOutGroup();
    // Recompute the 4-bit nibble from checkbox states and push it into
    // TransmitModel.  Guarded by m_syncingFromModel to avoid feedback.
    void onCheckboxToggled();
    // Mirror TransmitModel::userDigOut into the four checkboxes.  Sets
    // m_syncingFromModel for the duration so the resulting toggled()
    // signals don't write back to the model.
    void onModelUserDigOutChanged(int nibble);

    RadioModel*      m_model{nullptr};
    OcMatrix*        m_ocMatrix{nullptr};   // non-owning — owned by RadioModel (Phase 3P-D Task 3)
    QTabWidget*      m_subTabs{nullptr};
    OcOutputsHfTab*  m_hfTab{nullptr};
    OcOutputsSwlTab* m_swlTab{nullptr};
    QWidget*         m_vhfTab{nullptr};     // placeholder for tab-count parity

    // ── User Dig Out (P1 full-parity §4.3) ─────────────────────────────────
    // 4 bit-toggle checkboxes wired to TransmitModel::userDigOut.
    // bit 0 = Pin 1, bit 1 = Pin 2, bit 2 = Pin 3, bit 3 = Pin 4.
    // Group is visibility-gated on caps.hasPennyLane (set in populate()).
    static constexpr int kUserDigOutBits = 4;
    QGroupBox*                                       m_userDigOutGroup{nullptr};
    std::array<QCheckBox*, kUserDigOutBits>          m_userDigOutChecks{};

    // Echo-loop guard: when set, checkbox toggled() handlers skip the
    // write-back to TransmitModel.  Used during onModelUserDigOutChanged.
    bool m_syncingFromModel{false};
};

} // namespace NereusSDR
