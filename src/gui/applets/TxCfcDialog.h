// =================================================================
// src/gui/applets/TxCfcDialog.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmCFCConfig.{cs,Designer.cs}
//   [v2.10.3.13] — the per-band CFC (Continuous Frequency Compressor)
//   editor.  Original licence reproduced below.
//
// Layout reference:
//   - frmCFCConfig.Designer.cs:267-286 [v2.10.3.13] — nudCFC_f
//     (per-band frequency, range 0..20000 Hz).
//   - frmCFCConfig.Designer.cs:217-236 [v2.10.3.13] — nudCFC_c
//     (per-band compression amount, range 0..16 dB).
//   - frmCFCConfig.Designer.cs:564-583 [v2.10.3.13] — nudCFC_gain
//     (per-band post-EQ gain, range -24..+24 dB).
//   - frmCFCConfig.Designer.cs:408-422 [v2.10.3.13] — nudCFC_precomp
//     (global pre-comp scalar, 0..16 dB).
//   - frmCFCConfig.Designer.cs:337-351 [v2.10.3.13] — nudCFC_posteqgain
//     (global post-EQ make-up gain, -24..+24 dB).
//
// NereusSDR-spin (deviation, intentional):
//   Thetis exposes per-band edits via a `nudCFC_selected_band` (1..nfreqs)
//   plus a band-edit pane.  NereusSDR substitutes a 10-row × 3-column grid
//   so all bands are visible at once — same visual density as the existing
//   TxEqDialog (3M-3a-i Batch 3) and TxApplet.  Documented in the dialog
//   header comment per `feedback_thetis_userland_parity`.
//
// Profile bank: mirrors TxEqDialog's profile combo + Save / Save As / Delete
// buttons (3M-3a-i Batch 4 Task A.2).  Same profile bank — switching a
// profile applies the saved CFC values along with EQ / mic / VOX / Lev / ALC
// (silently in the model — only the EQ + CFC controls show on this dialog).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task A): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  Modeless lazy-singleton
//                 owned by TxApplet (m_cfcDialog).  Bidirectional
//                 binding to TransmitModel for 32 controls (10 freq +
//                 10 comp + 10 post-EQ band gain + 2 globals).
// =================================================================

//=================================================================
// frmCFCConfig.cs
//=================================================================
//  frmCFCConfig.cs
//
// This file is part of a program that implements a Software-Defined Radio.
//
// This code/file can be found on GitHub : https://github.com/ramdor/Thetis
//
// Copyright (C) 2020-2026 Richard Samphire MW0LGE
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// The author can be reached by email at
//
// mw0lge@grange-lane.co.uk
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

class QComboBox;
class QPushButton;
class QSpinBox;

namespace NereusSDR {

class MicProfileManager;
class TransmitModel;

// TxCfcDialog — modeless 10-band CFC editor.
//
// Layout (NereusSDR-spin: 10-row grid, mirrors Thetis frmCFCConfig 1:1
// for the global scalars + per-band field set):
//
//   ┌── Profile row ────────────────────────────────────────────────┐
//   │ Profile: [combo]   [Save]  [Save As…]  [Delete]               │
//   ├── Globals row ────────────────────────────────────────────────┤
//   │ Pre-Comp:    [spinbox]  dB    Post-EQ Gain: [spinbox]  dB     │
//   ├── Per-band grid (10 rows × FREQ / COMP / POST-EQ) ────────────┤
//   │ #  Freq (Hz)   Comp (dB)   Post-EQ (dB)                       │
//   │ 1  [____]      [____]      [____]                             │
//   │ 2  [____]      [____]      [____]                             │
//   │ ...                                                           │
//   │ 10 [____]      [____]      [____]                             │
//   ├── Bottom row ─────────────────────────────────────────────────┤
//   │ [Reset to factory defaults]              [Close]              │
//   └───────────────────────────────────────────────────────────────┘
//
// All controls are bidirectionally bound to TransmitModel via an
// m_updatingFromModel echo guard (mirrors TxEqDialog pattern).  The
// underlying TM ↔ TxChannel routing was wired in 3M-3a-ii Batch 2.
//
// Lifecycle: modeless dialog owned by TxApplet (m_cfcDialog).  WA_DeleteOnClose
// is forced false in the ctor so the dialog survives close+reopen for fast
// re-launch.  Caller (TxApplet::requestOpenCfcDialog) lazy-creates and reuses
// the same instance.
class TxCfcDialog : public QDialog {
    Q_OBJECT

public:
    explicit TxCfcDialog(TransmitModel* tm,
                         MicProfileManager* mgr,
                         QWidget* parent = nullptr);
    ~TxCfcDialog() override;

    // ── Test seams (mirror TxEqDialog A.2 hooks) ──────────────────
    using SaveAsPromptHook       = std::function<std::pair<bool, QString>(const QString& seed)>;
    using OverwriteConfirmHook   = std::function<bool(const QString& name)>;
    using DeleteConfirmHook      = std::function<bool(const QString& name)>;
    using ResetConfirmHook       = std::function<bool()>;
    using RejectionMessageHook   = std::function<void(const QString& msg)>;

    void setSaveAsPromptHook    (SaveAsPromptHook hook);
    void setOverwriteConfirmHook(OverwriteConfirmHook hook);
    void setDeleteConfirmHook   (DeleteConfirmHook hook);
    void setResetConfirmHook    (ResetConfirmHook hook);
    void setRejectionMessageHook(RejectionMessageHook hook);

    // ── Widget accessors for tests ────────────────────────────────
    QComboBox*   profileCombo() const { return m_profileCombo; }
    QPushButton* saveBtn()      const { return m_saveBtn; }
    QPushButton* saveAsBtn()    const { return m_saveAsBtn; }
    QPushButton* deleteBtn()    const { return m_deleteBtn; }
    QPushButton* resetBtn()     const { return m_resetBtn; }
    QPushButton* closeBtn()     const { return m_closeBtn; }
    QSpinBox*    precompSpin()  const { return m_precompSpin; }
    QSpinBox*    postEqGainSpin() const { return m_postEqGainSpin; }
    QSpinBox*    freqSpin(int idx)        const { return (idx >= 0 && idx < 10) ? m_freqSpins[idx]        : nullptr; }
    QSpinBox*    compSpin(int idx)        const { return (idx >= 0 && idx < 10) ? m_compSpins[idx]        : nullptr; }
    QSpinBox*    postEqBandSpin(int idx)  const { return (idx >= 0 && idx < 10) ? m_postEqBandSpins[idx]  : nullptr; }

private slots:
    // Profile / button slots (mirrors TxEqDialog A.2).
    void onProfileComboChanged(const QString& name);
    void onSaveClicked();
    void onSaveAsClicked();
    void onDeleteClicked();
    void onResetDefaultsClicked();

    // TM → UI sync (echo-guarded).
    void syncFromModel();

    // Profile manager → combo refresh.
    void refreshProfileCombo();
    void onActiveProfileChanged(const QString& name);

private:
    void buildUi();
    void wireSignals();
    bool persistProfile(const QString& name, bool setActiveAfter);

    QPointer<TransmitModel>     m_tm;        // non-owning
    QPointer<MicProfileManager> m_mgr;       // non-owning, may be null pre-connect
    bool m_updatingFromModel = false;

    // ── Profile bank widgets (mirrors TxEqDialog) ─────────────────
    QComboBox*   m_profileCombo  = nullptr;
    QPushButton* m_saveBtn       = nullptr;
    QPushButton* m_saveAsBtn     = nullptr;
    QPushButton* m_deleteBtn     = nullptr;

    // ── Bottom strip ──────────────────────────────────────────────
    QPushButton* m_resetBtn      = nullptr;
    QPushButton* m_closeBtn      = nullptr;

    // ── Global CFC scalars ────────────────────────────────────────
    QSpinBox*    m_precompSpin   = nullptr;  // Pre-Comp 0..16 dB
    QSpinBox*    m_postEqGainSpin= nullptr;  // Post-EQ Gain -24..+24 dB

    // ── 10-row per-band grid (3 spinboxes per row) ────────────────
    std::array<QSpinBox*, 10> m_freqSpins{};         // Hz
    std::array<QSpinBox*, 10> m_compSpins{};         // dB (compression)
    std::array<QSpinBox*, 10> m_postEqBandSpins{};   // dB (post-EQ band gain)

    // ── Test hooks ────────────────────────────────────────────────
    SaveAsPromptHook     m_saveAsHook;
    OverwriteConfirmHook m_overwriteHook;
    DeleteConfirmHook    m_deleteHook;
    ResetConfirmHook     m_resetHook;
    RejectionMessageHook m_rejectionHook;
};

} // namespace NereusSDR
