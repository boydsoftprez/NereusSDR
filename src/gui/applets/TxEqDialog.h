// =================================================================
// src/gui/applets/TxEqDialog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/eqform.cs (legacy 10-band TX EQ
//   layout — grpTXEQ region: chkTXEQEnabled, tbTXEQPre, tbTXEQ0..9,
//   udTXEQ0..9, plus setTXEQProfile handler).  Original licence from
//   Thetis source is included below.
//
//   Layout reference: eqform.cs lines 1021-1561 (grpTXEQ control
//   list, slider/spinbox geometry).  TX-only — Thetis EQForm hosts
//   both RX and TX EQ in one dialog; NereusSDR splits them and this
//   file covers TX only.  Profile load/save is intentionally out of
//   scope for Phase 3M-3a-i Batch 3 (deferred to Batch 4 / A.2).
//
// Nc / Mp / Ctfmode / Wintype combos: Thetis exposes these via
// hidden Setup pages, not EQForm.  NereusSDR surfaces them in this
// dialog as a Thetis-userland-parity-with-our-spin (see
// docs/architecture — "feedback_thetis_userland_parity") so power
// users can reach the WDSP filter controls without leaving the EQ
// surface.  Default values match WDSP TXA.c create_eqp() exactly:
// nc=2048, mp=false, ctfmode=0, wintype=0.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — Phase 3M-3a-i Batch 3 (Task A.1): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  Modeless singleton
//                 dialog; bidirectional binding to TransmitModel.
//   2026-04-29 — Phase 3M-3a-i Batch 4 (Task A.2): TX profile combo
//                 + Save / Save As / Delete buttons added by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  The combo populates from
//                 MicProfileManager::profileNames() — Thetis has no
//                 separate "TX EQ profile" so the same profile bank
//                 used by TxApplet's TX-Profile combo and the Setup
//                 → Audio → TX Profile editor is exposed here.
//                 Selecting a profile updates the visible EQ controls
//                 AND silently updates mic gain / VOX / Leveler / ALC
//                 (their UI homes are elsewhere).  Mirrors Thetis
//                 setup.cs:9505-9656 [v2.10.3.13] btnTXProfileSave_Click
//                 / btnTXProfileDelete_Click semantics.
// =================================================================

//=================================================================
// eqform.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#pragma once

#include <QDialog>
#include <QPointer>
#include <array>
#include <functional>

class QCheckBox;
class QComboBox;
class QPushButton;
class QSlider;
class QSpinBox;

namespace NereusSDR {

class MicProfileManager;
class RadioModel;
class TransmitModel;

// TxEqDialog — modeless 10-band TX EQ dialog.
//
// Layout (mirrors Thetis grpTXEQ structure):
//
//   ┌── Top row ──────────────────────────────────────────────────────┐
//   │  [Enable]   Nc [____]   [Mp]   Ctfmode [v]   Window [v]         │
//   ├── Band columns (preamp + 10 bands) ─────────────────────────────┤
//   │  Pre  B1   B2   B3   B4   B5   B6   B7   B8   B9   B10          │
//   │  ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲   +15 dB  │
//   │  │    │    │    │    │    │    │    │    │    │    │            │
//   │  │    │    │    │    │    │    │    │    │    │    │     0 dB  │
//   │  │    │    │    │    │    │    │    │    │    │    │            │
//   │  ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼   −12 dB  │
//   │ [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB]          │
//   │      [Hz][Hz] [Hz] [Hz] [Hz] [Hz] [Hz] [Hz] [Hz] [Hz]           │
//   └─────────────────────────────────────────────────────────────────┘
//
// All controls are bidirectionally bound to RadioModel::transmitModel()
// via a m_updatingFromModel echo guard (mirrors VfoWidget /
// VoxSettingsPopup pattern).  WDSP plumbing is already in place from
// Phase 3M-3a-i Batches 1 & 2 — this dialog is pure UI on top.
//
// Lifecycle: dialog is a modeless singleton owned by the static
// instance() helper (WA_DeleteOnClose=false).  Calling instance()
// repeatedly returns the same pointer — caller does NOT own.
class TxEqDialog : public QDialog {
    Q_OBJECT

public:
    explicit TxEqDialog(RadioModel* radio, QWidget* parent = nullptr);
    ~TxEqDialog() override;

    // Modeless-singleton accessor.  First call constructs; subsequent
    // calls return the same pointer.  Caller responsibility is to
    // call show()+raise()+activateWindow() to bring the dialog
    // forward.  The dialog persists between opens (WA_DeleteOnClose
    // is forced false in the ctor so close-button or programmatic
    // hide preserves the singleton).
    static TxEqDialog* instance(RadioModel* radio, QWidget* parent = nullptr);

    // ── A.2 test seams ─────────────────────────────────────────────
    // Override the QInputDialog::getText / QMessageBox::question modal
    // chain with deterministic hooks.  When unset, the dialog falls
    // back to the real Qt dialogs (production behaviour).
    using SaveAsPromptHook       = std::function<std::pair<bool, QString>(const QString& seed)>;
    using OverwriteConfirmHook   = std::function<bool(const QString& name)>;
    using DeleteConfirmHook      = std::function<bool(const QString& name)>;
    using RejectionMessageHook   = std::function<void(const QString& msg)>;

    void setSaveAsPromptHook    (SaveAsPromptHook hook);
    void setOverwriteConfirmHook(OverwriteConfirmHook hook);
    void setDeleteConfirmHook   (DeleteConfirmHook hook);
    void setRejectionMessageHook(RejectionMessageHook hook);

    // ── A.2 widget accessors for tests ─────────────────────────────
    QComboBox*   profileCombo() const { return m_profileCombo; }
    QPushButton* saveBtn()      const { return m_saveBtn; }
    QPushButton* saveAsBtn()    const { return m_saveAsBtn; }
    QPushButton* deleteBtn()    const { return m_deleteBtn; }

private slots:
    // ── User-driven control changes → TransmitModel ────────────────
    void onEnableToggled(bool on);
    void onPreampChanged(int dB);
    void onBandValueChanged();   // shared — finds sender index
    void onFreqValueChanged();   // shared — finds sender index
    void onNcChanged(int nc);
    void onMpToggled(bool mp);
    void onCtfmodeChanged(int mode);
    void onWintypeChanged(int wintype);

    // ── A.2 profile-bank handlers ──────────────────────────────────
    void onProfileComboChanged(const QString& name);
    void onSaveClicked();
    void onSaveAsClicked();
    void onDeleteClicked();

    // ── Model → UI sync (echo-guarded) ─────────────────────────────
    void syncFromModel();

    // ── Profile manager → combo refresh ────────────────────────────
    void refreshProfileCombo();
    void onActiveProfileChanged(const QString& name);

private:
    void buildUi();
    void wireSignals();

    // Helper: actually write the named profile + set active (no prompts).
    // Returns true on success.  Used by the Save / Save-As paths after
    // the prompt flow has resolved the target name.
    bool persistProfile(const QString& name, bool setActiveAfter);

    QPointer<RadioModel> m_radio;          // non-owning
    bool m_updatingFromModel = false;

    QCheckBox*    m_enableChk     = nullptr;
    QSlider*      m_preampSlider  = nullptr;
    QSpinBox*     m_preampSpin    = nullptr;
    std::array<QSlider*,  10> m_bandSliders{};
    std::array<QSpinBox*, 10> m_bandSpins{};
    std::array<QSpinBox*, 10> m_freqSpins{};
    QSpinBox*     m_ncSpin        = nullptr;
    QCheckBox*    m_mpChk         = nullptr;
    QComboBox*    m_ctfmodeCombo  = nullptr;
    QComboBox*    m_wintypeCombo  = nullptr;

    // ── A.2 profile-bank widgets ───────────────────────────────────
    QComboBox*    m_profileCombo  = nullptr;
    QPushButton*  m_saveBtn       = nullptr;
    QPushButton*  m_saveAsBtn     = nullptr;
    QPushButton*  m_deleteBtn     = nullptr;

    // ── A.2 test hooks (unset → real Qt dialogs are used) ──────────
    SaveAsPromptHook     m_saveAsHook;
    OverwriteConfirmHook m_overwriteHook;
    DeleteConfirmHook    m_deleteHook;
    RejectionMessageHook m_rejectionHook;
};

} // namespace NereusSDR
