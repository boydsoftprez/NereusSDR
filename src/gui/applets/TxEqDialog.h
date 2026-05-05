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
//   file covers TX only.  RX EQ lives in the EqApplet widget.
//
//   Parametric panel reference: eqform.cs:235-2862 (control set in
//   the InitializeComponent block) + cs:2862-2911 (chkLegacyEQ
//   handler).  Defaults from cs:928-967 (ucParametricEq1 widget
//   property block).
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
//                 via Anthropic Claude Code.  The combo populated from
//                 MicProfileManager::profileNames() — Thetis has no
//                 separate "TX EQ profile" so the same profile bank
//                 used by TxApplet's TX-Profile combo and the Setup
//                 → Audio → TX Profile editor was exposed here.
//                 Selecting a profile updated the visible EQ controls
//                 AND silently updated mic gain / VOX / Leveler / ALC
//                 (their UI homes are elsewhere).  Mirrored Thetis
//                 setup.cs:9505-9656 [v2.10.3.13] btnTXProfileSave_Click
//                 / btnTXProfileDelete_Click semantics.
//   2026-04-30 — Phase 3M-3a-ii follow-up sub-PR Batch 9: legacy /
//                 parametric panel toggle added by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.  Profile combo + Save / Save As / Delete buttons
//                 dropped — profile management lives on the TxApplet's
//                 TX-Profile combo and on the Console / Setup → Audio →
//                 TX Profile editor.  chkLegacyEQ checkbox swaps between
//                 a QStackedWidget holding the legacy 10-band slider
//                 panel (preserved from Batch 3) and a new parametric
//                 panel embedding a single ParametricEqWidget plus the
//                 edit row + right column controls per eqform.cs:235-2911
//                 [v2.10.3.13].  Legacy band-column sliders / spinboxes
//                 now pick up Style::sliderVStyle() + kSpinBoxStyle —
//                 fixes a styling regression where they rendered with
//                 the system default look-and-feel.
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

class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QWidget;

namespace NereusSDR {

class ParametricEqWidget;
class RadioModel;
class TransmitModel;

// TxEqDialog — modeless TX EQ dialog with two layouts.
//
//   ┌── Top row ──────────────────────────────────────────────────────┐
//   │  [Legacy EQ]   [Enable]   Nc [____]   [Mp]   Cutoff [v]   ...   │
//   ├── QStackedWidget swap ──────────────────────────────────────────┤
//   │                                                                  │
//   │  Legacy panel (chkLegacyEQ checked)                             │
//   │   ┌── Band columns ───────────────────────────────────────────┐ │
//   │   │  Pre  B1   B2   B3   B4   B5   B6   B7   B8   B9   B10   │ │
//   │   │   ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲    ▲    │ │
//   │   │   │    │    │    │    │    │    │    │    │    │    │    │ │
//   │   │   ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼    ▼    │ │
//   │   │  [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB] [dB]  │ │
//   │   │       [Hz][Hz] [Hz] [Hz] [Hz] [Hz] [Hz] [Hz] [Hz] [Hz]   │ │
//   │   └───────────────────────────────────────────────────────────┘ │
//   │                                                                  │
//   │  Parametric panel (chkLegacyEQ unchecked)                       │
//   │   ┌── Edit row ──────────────────────────────────────────────┐ │
//   │   │  # [_] f [____] Hz  Gain [__] dB  Q [__]  Preamp [__]    │ │
//   │   │                                            [Reset]       │ │
//   │   ├── ParametricEqWidget + right column ────────────────────┤ │
//   │   │                                          [□ Log scale]   │ │
//   │   │                                          [□ Use Q Fact.] │ │
//   │   │  +24 ┐                                   [□ Live Update] │ │
//   │   │      │                  ╱─╮              ⚠              │ │
//   │   │      │     ●───────────╯  ●─────●        Low  [____] Hz │ │
//   │   │      │                                   High [____] Hz │ │
//   │   │  -24 ┘                                                   │ │
//   │   │      0                                  16k              │ │
//   │   │                                                          │ │
//   │   │                                          ( ) 5-band      │ │
//   │   │                                          (•) 10-band     │ │
//   │   │                                          ( ) 18-band     │ │
//   │   └───────────────────────────────────────────────────────────┘ │
//   └─────────────────────────────────────────────────────────────────┘
//
// The legacy panel is bidirectionally bound to RadioModel::transmit-
// Model() via a m_updatingFromModel echo guard (mirrors VfoWidget
// pattern).  WDSP plumbing is already in place from
// Phase 3M-3a-i Batches 1 & 2 — this dialog is pure UI on top.
//
// The parametric panel embeds a ParametricEqWidget (Tasks 1-5).
// Its points round-trip through TransmitModel.txEqParaEqData (Task 6
// JSON blob) when the user releases the mouse or types a value.
//
// Lifecycle: dialog is a modeless singleton owned by the static
// instance() helper.  WA_DeleteOnClose=false; closeEvent() ignores
// and hides instead of destroying so the singleton survives close /
// hide cycles.
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

    // ── Test / introspection accessors ────────────────────────────
    QCheckBox*          legacyToggle()    const { return m_legacyToggle; }
    QStackedWidget*     panelStack()      const { return m_panelStack; }
    QWidget*            legacyPanel()     const { return m_legacyPanel; }
    QWidget*            parametricPanel() const { return m_parametricPanel; }
    ParametricEqWidget* parametricWidget() const { return m_parametricWidget; }
    QButtonGroup*       bandCountGroup() const { return m_bandCountGroup; }

private slots:
    // ── User-driven control changes → TransmitModel (legacy panel) ─
    void onEnableToggled(bool on);
    void onPreampChanged(int dB);
    void onBandValueChanged();   // shared — finds sender index
    void onFreqValueChanged();   // shared — finds sender index
    void onNcChanged(int nc);
    void onMpToggled(bool mp);
    void onCtfmodeChanged(int mode);
    void onWintypeChanged(int wintype);

    // ── Legacy-vs-parametric panel toggle ──────────────────────────
    void onLegacyToggled(bool legacy);

    // ── Parametric panel slots ─────────────────────────────────────
    void onParametricPointsChanged(bool isDragging);
    void onParametricGlobalGainChanged(bool isDragging);
    void onParametricSelectedChanged(bool isDragging);
    void onParametricResetClicked();
    void onParametricBandCountChanged();
    void onParametricLowFreqChanged(int hz);
    void onParametricHighFreqChanged(int hz);
    void onParametricLogScaleToggled(bool on);
    void onParametricUseQFactorsToggled(bool on);
    void onParametricLiveUpdateToggled(bool on);
    void onParametricSelectedBandChanged(int oneBased);
    void onParametricFreqSpinChanged(int hz);
    void onParametricGainSpinChanged(double db);
    void onParametricQSpinChanged(double q);
    void onParametricPreampSpinChanged(double db);

    // ── Model → UI sync (echo-guarded) ─────────────────────────────
    void syncFromModel();
    // Loads txEqParaEqData JSON into the parametric widget.  Called on
    // initial sync AND on the model's txEqParaEqDataChanged signal so
    // profile activation re-populates the parametric panel.  Without
    // this, the next user edit would overwrite the just-loaded curve
    // (Codex P1 #2 on PR #159).
    void syncParametricFromModel();

protected:
    // Hide-on-close per Thetis frmCFCConfig.cs:477-482 [v2.10.3.13]
    // pattern — TxApplet keeps the singleton alive for fast re-show.
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    QWidget* buildLegacyPanel();
    QWidget* buildParametricPanel();
    void wireSignals();

    // Sync the parametric panel's edit-row spinboxes from the currently-
    // selected ParametricEqWidget point.
    void updateEditRowFromSelection();
    // Push the current ParametricEqWidget points into TransmitModel::
    // setTxEqParaEqData (JSON round-trip) AND push the parametric
    // curve directly to WDSP via pushParametricCurveToWdsp().
    void pushParametricToModel();

    // Build (F[10], G[11]) from parametric widget state and push via
    // TxChannel::setTxEqProfile.  Called on every parametric edit
    // (from pushParametricToModel) and on toggle into parametric mode
    // (from onLegacyToggled) so WDSP switches curves immediately
    // without waiting for the user's first edit.
    void pushParametricCurveToWdsp();

    // Build (F[10], G[11]) from legacy txEqFreq/txEqBand/txEqPreamp
    // model state and push via TxChannel::setTxEqProfile.  Called on
    // toggle BACK to legacy mode so WDSP restores the legacy curve
    // immediately (the legacy slider setters re-fire pushEqProfile
    // via RadioModel only on user edit; without this, the parametric
    // curve would persist on WDSP until the user nudged a slider).
    void pushLegacyCurveToWdsp();

    QPointer<RadioModel> m_radio;          // non-owning
    bool m_updatingFromModel = false;
    bool m_ignoreUpdates     = false;

    // ── Legacy panel widgets ───────────────────────────────────────
    QStackedWidget* m_panelStack       = nullptr;
    QWidget*        m_legacyPanel      = nullptr;
    QWidget*        m_parametricPanel  = nullptr;
    QCheckBox*      m_legacyToggle     = nullptr;

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

    // ── Parametric panel widgets ───────────────────────────────────
    ParametricEqWidget* m_parametricWidget = nullptr;
    // Edit row.
    QSpinBox*       m_paraSelectedBandSpin = nullptr;
    QSpinBox*       m_paraFreqSpin         = nullptr;
    QDoubleSpinBox* m_paraGainSpin         = nullptr;
    QDoubleSpinBox* m_paraQSpin            = nullptr;
    QDoubleSpinBox* m_paraPreampSpin       = nullptr;
    QPushButton*    m_paraResetBtn         = nullptr;
    // Right column.
    QCheckBox*      m_paraLogScaleChk     = nullptr;
    QCheckBox*      m_paraUseQFactorsChk  = nullptr;
    QCheckBox*      m_paraLiveUpdateChk   = nullptr;
    QSpinBox*       m_paraLowSpin         = nullptr;
    QSpinBox*       m_paraHighSpin        = nullptr;
    QRadioButton*   m_paraBands5Radio     = nullptr;
    QRadioButton*   m_paraBands10Radio    = nullptr;
    QRadioButton*   m_paraBands18Radio    = nullptr;
    QButtonGroup*   m_bandCountGroup      = nullptr;
};

} // namespace NereusSDR
