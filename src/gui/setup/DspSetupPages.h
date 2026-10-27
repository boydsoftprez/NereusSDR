#pragma once

// =================================================================
// src/gui/setup/DspSetupPages.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-05-04 — Issue #175 Wave 1: dropped misplaced AM TX / Carrier
//                 Level stub from AmSamSetupPage (control belongs at
//                 Thetis grpTXAM on tpTransmit, not the DSP/AM tab).
//   2026-09-25 - R-R3-49 (parity Task 4): AgcAlcSetupPage and CfcSetupPage
//                 override setTransmitSettingsPermittedAt for version 4.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: CW transmit, CwSetupPage's Keyer, Break-In, Sidetone, Key
//               Input and Options groups (R-CW-43); the radio sidetone is
//               disabled with its reason, not hidden, where the radio has
//               no sidetone generator (R-CW-40). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

//=================================================================
// setup.cs
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// src/gui/setup/DspSetupPages.h
//
// Nine DSP setup pages for the NereusSDR settings dialog.
// All controls are present as real widgets but disabled (NYI — no WDSP
// wiring yet).  Each class corresponds to one leaf item in the DSP
// category of the tree navigation.
//
// Pages:
//   AgcAlcSetupPage   — AGC / ALC / Leveler
//   NrAnfSetupPage    — Noise Reduction / ANF / EMNR
//   NbSnbSetupPage    — NB1 / NB2 / SNB
//   CwSetupPage       — Keyer / Break-In / Sidetone / Key Input / Options / APF
//   AmSamSetupPage    — SAM / Squelch  (AM TX moved to Transmit page —
//                       Thetis grpTXAM lives on tpTransmit, not the
//                       DSP/AM tab; mi0bot setup.designer.cs:47710-47711
//                       [v2.10.3.13-beta2])
//   FmSetupPage       — FM RX / FM TX
//   CfcSetupPage      — CFC / Profile
//   MnfSetupPage      — Manual Notch Filter
//
// (VoxDexpSetupPage placeholder removed in 3M-3a-iii Task 16 — the wired
//  page lives at Setup → Transmit → DEXP/VOX (DexpVoxPage from Task 14).)

#include "core/WdspTypes.h"   // NrSlot
#include "gui/SetupPage.h"
#include "gui/NereusGuiExport.h"

#include <QCheckBox>
#include <QList>
#include <functional>

class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;
class QTableWidget;

namespace NereusSDR {

class RadioModel;
class SliceModel;
class NnrControls;
enum class AGCMode : int;

// A Setup dialog's receiver authority. Ordinary dialogs use the model's
// active slice; a hosted desktop supplies its owned window selection.
class DspReceiverSelection : public QObject {
    Q_OBJECT
public:
    explicit DspReceiverSelection(QObject* parent = nullptr) : QObject(parent) {}
    void setSelector(std::function<SliceModel*()> selector,
                     std::function<bool()> requiresOwnedReceiver = {});
    bool restrictsReceiver() const;
    SliceModel* selected(RadioModel* model) const;
    void notifyChanged(RadioModel* model);
signals:
    void changed();
private:
    std::function<SliceModel*()> m_selector;
    std::function<bool()> m_requiresOwnedReceiver;
    SliceModel* m_lastSelected = nullptr;
    bool m_lastRestricted = false;
};

class ReceiverDspSetupPage : public SetupPage {
public:
    ReceiverDspSetupPage(const QString& title, RadioModel* model, QWidget* parent,
                         DspReceiverSelection* selection);
protected:
    SliceModel* selectedSlice();
    DspReceiverSelection* receiverSelection() const { return m_selection; }
    void watchReceiverSelection(std::function<void()> update);
private:
    DspReceiverSelection* m_selection;
};

// ── AGC / ALC ─────────────────────────────────────────────────────────────────

class AgcAlcSetupPage : public ReceiverDspSetupPage {
    Q_OBJECT
public:
    explicit AgcAlcSetupPage(RadioModel* model, QWidget* parent = nullptr,
                             DspReceiverSelection* selection = nullptr);

    // R-R3-49 (parity Task 4): the TX Leveler and TX ALC groups follow the
    // transmit settings gate at version 4; the receive AGC groups on the
    // page do not.
    void setTransmitSettingsPermittedAt(int version, bool permitted,
                                        const QString& reason) override;

private:
    void updateCustomGating(AGCMode mode);
    void rebuildForActiveSlice();

    QGroupBox*  m_txLevelerGrp{nullptr};
    QGroupBox*  m_txAlcGrp{nullptr};
    bool m_txSettingsGateKnown{false};
    bool m_txSettingsPermitted{false};
    QString m_txSettingsReason;

    QComboBox*  m_agcModeCombo{nullptr};
    QSpinBox*   m_agcAttack{nullptr};
    QSpinBox*   m_agcDecay{nullptr};
    QSpinBox*   m_agcHang{nullptr};
    QSlider*    m_agcSlope{nullptr};
    QSpinBox*   m_agcMaxGain{nullptr};
    QSpinBox*   m_agcFixedGain{nullptr};
    QSlider*    m_agcHangThresh{nullptr};
    QCheckBox*  m_autoAgcChk{nullptr};
    QSpinBox*   m_autoAgcOffset{nullptr};

    // ── Phase 3M-3a-i Batch 2 Task 2 (D): TX Leveler + TX ALC controls ──
    // Wired bidirectionally to RadioModel::transmitModel() — see
    // DspSetupPages.cpp for the connect() block.
    QCheckBox*  m_txLevelerOnChk{nullptr};
    QSpinBox*   m_txLevelerTopSpin{nullptr};
    QSpinBox*   m_txLevelerDecaySpin{nullptr};
    QSpinBox*   m_txAlcMaxGainSpin{nullptr};
    QSpinBox*   m_txAlcDecaySpin{nullptr};
};

// ── NR / ANF ──────────────────────────────────────────────────────────────────

class NrAnfSetupPage : public ReceiverDspSetupPage {
    Q_OBJECT
public:
    explicit NrAnfSetupPage(RadioModel* model, QWidget* parent = nullptr,
                            DspReceiverSelection* selection = nullptr);

    // Programmatically select a sub-tab by NrSlot. Used by MainWindow to
    // route "More Settings…" popup clicks to the correct filter's sub-tab.
    void selectSubtab(NereusSDR::NrSlot slot, int openerSliceId = -1);

private:
    void rebuildForActiveSlice();
    QTabWidget* m_tabs{nullptr};  // owned by content layout
    NnrControls* m_nnrControls{nullptr};
};

// ── NB / SNB ──────────────────────────────────────────────────────────────────

class NbSnbSetupPage : public ReceiverDspSetupPage {
    Q_OBJECT
public:
    explicit NbSnbSetupPage(RadioModel* model, QWidget* parent = nullptr,
                            DspReceiverSelection* selection = nullptr);
};

// ── CW ───────────────────────────────────────────────────────────────────────

class CwSetupPage : public ReceiverDspSetupPage {
    Q_OBJECT
public:
    explicit CwSetupPage(RadioModel* model, QWidget* parent = nullptr,
                         DspReceiverSelection* selection = nullptr);

    // R-CW-40: whether the radio makes a sidetone of its own
    // (BoardCapabilities::hasSidetoneGenerator). Where it does not (the
    // Hermes Lite 2), "From the radio" is shown disabled with the reason,
    // never hidden. A page with no radio known keeps it enabled: the
    // setting is kept for the next radio.
    void setHasSidetoneGenerator(bool on);
    // Test seam: whether "From the radio" is enabled.
    bool radioSidetoneEnabledForTest() const;

    // R-CW-03: the CW settings came with transmitSettingsVersion 16. In a
    // remote window they start closed until SetupDialog pushes that gate.
    void setTransmitSettingsPermittedAt(int version, bool permitted,
                                        const QString& reason) override;

    // D9: the note under Key-down to RF when the speed limits it, or empty.
    static QString keyDownClampNote(int keyDownMs, int speedWpm);
    // R-CW-13: the radio hang time readout for a break-in mode.
    static QString radioHangTimeText(int breakInMode, int breakInDelayMs, int keyUpDelayMs);

private:
    void buildTransmitGroups(RadioModel* model);
    void showFromModel();
    void refreshStates();
    void refreshBoard();
    void refreshSerialPorts();

    RadioModel* m_cwModel = nullptr;
    bool m_updatingFromModel = false;
    bool m_cwSettingsPermitted = true;
    QString m_cwSettingsReason;
    bool m_hasSidetoneGenerator = true;
    bool m_isHl2 = false;

    // Keyer
    QComboBox* m_keyerMode = nullptr;
    QSpinBox*  m_keyerWeight = nullptr;
    QSlider*   m_keyerSpeed = nullptr;
    QLabel*    m_keyerSpeedValue = nullptr;
    QSlider*   m_pitch = nullptr;
    QLabel*    m_pitchValue = nullptr;
    QCheckBox* m_reversePaddles = nullptr;
    QCheckBox* m_strictSpacing = nullptr;
    QLabel*    m_keyerHl2Note = nullptr;
    // Break-In
    QComboBox* m_breakInMode = nullptr;
    QSlider*   m_breakInDelay = nullptr;
    QLabel*    m_breakInDelayValue = nullptr;
    QSpinBox*  m_keyDownDelay = nullptr;
    QLabel*    m_keyDownNote = nullptr;
    QSpinBox*  m_keyUpDelay = nullptr;
    QLabel*    m_hangTime = nullptr;
    // Sidetone
    QCheckBox* m_radioSidetone = nullptr;
    QCheckBox* m_computerSidetone = nullptr;
    QSlider*   m_sidetoneLevel = nullptr;
    QLabel*    m_sidetoneLevelValue = nullptr;
    QLabel*    m_sidetoneHl2Note = nullptr;
    // Key Input (serial port)
    QComboBox* m_serialPort = nullptr;
    QLabel*    m_serialStatus = nullptr;
    QCheckBox* m_serialPaddles = nullptr;
    QComboBox* m_serialKeyLine = nullptr;
    QComboBox* m_serialPttLine = nullptr;
    // Options
    QCheckBox* m_autoModeSwitch = nullptr;
    QCheckBox* m_autoModeReturn = nullptr;
    QSpinBox*  m_autoModeReturnMs = nullptr;
};

// ── AM / SAM ─────────────────────────────────────────────────────────────────

class AmSamSetupPage : public ReceiverDspSetupPage {
    Q_OBJECT
public:
    explicit AmSamSetupPage(RadioModel* model, QWidget* parent = nullptr,
                            DspReceiverSelection* selection = nullptr);
};

// ── FM ───────────────────────────────────────────────────────────────────────

class FmSetupPage : public ReceiverDspSetupPage {
    Q_OBJECT
public:
    explicit FmSetupPage(RadioModel* model, QWidget* parent = nullptr,
                         DspReceiverSelection* selection = nullptr);
};

// (VoxDexpSetupPage placeholder removed in 3M-3a-iii Task 16 — wired page
//  lives at Setup → Transmit → DEXP/VOX (DexpVoxPage from Task 14).)

// ── CFC ──────────────────────────────────────────────────────────────────────
//
// Phase 3M-3a-ii Batch 5: full implementation of the Setup → DSP → CFC page.
// Mirrors Thetis tpDSPCFC tab layout 1:1 (setup.Designer.cs grpPhRot at
// 46162-46280 [v2.10.3.13]).  Hosts three group boxes:
//   1. Phase Rotator   — chkPHROTEnable / chkPHROTReverse / udPhRotFreq /
//                        udPHROTStages, bidirectional with TransmitModel
//                        phase-rotator-* properties.
//   2. CFC             — Enable / Post-EQ Enable / PreComp / PostEqGain
//                        global controls + a [Configure CFC bands…] button
//                        that emits openCfcDialogRequested (Batch 6 wires
//                        the per-band TxCfcDialog).
//   3. CESSB           — Enable toggle + explanatory label per WDSP
//                        TXA.c:846-855 [v2.10.3.13] (CESSB gated on CPDR
//                        via the bp2 ladder).

class NEREUS_GUI_EXPORT CfcSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit CfcSetupPage(RadioModel* model, QWidget* parent = nullptr);

    // R-R3-49 (parity Task 4): the phase rotator, CFC and CESSB settings
    // follow the transmit settings gate at version 4.
    void setTransmitSettingsPermittedAt(int version, bool permitted,
                                        const QString& reason) override;

signals:
    /// Emitted when the [Configure CFC bands…] button is clicked.  Batch 6
    /// will wire the parent SetupDialog to route this to the modeless
    /// TxCfcDialog (10-band CFC editor).  For Batch 5 the signal lands but
    /// no dialog launches.
    void openCfcDialogRequested();

private:
    // ── Phase Rotator group ──────────────────────────────────────────────────
    QCheckBox* m_phRotEnableChk{nullptr};
    QCheckBox* m_phRotReverseChk{nullptr};
    QSpinBox*  m_phRotFreqSpin{nullptr};
    QSpinBox*  m_phRotStagesSpin{nullptr};

    // ── CFC group ────────────────────────────────────────────────────────────
    QCheckBox* m_cfcEnableChk{nullptr};
    QCheckBox* m_cfcPostEqEnableChk{nullptr};
    QSpinBox*  m_cfcPrecompSpin{nullptr};
    QSpinBox*  m_cfcPostEqGainSpin{nullptr};
    class QPushButton* m_cfcBandsBtn{nullptr};

    // ── CESSB group ──────────────────────────────────────────────────────────
    QCheckBox* m_cessbEnableChk{nullptr};
};

// ── MNF ──────────────────────────────────────────────────────────────────────
//
// Setup → DSP → MNF. Thetis's own tab name (setup.designer.cs:44141
// [v2.10.3.15], this.tpDSPMNF.Text = "MNF"); everything operator-facing
// outside Settings says TNF, mirroring upstream's own tpDSPMNF vs chkTNF
// split. TNF design section 9.

class MnfSetupPage : public ReceiverDspSetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr,
                          DspReceiverSelection* selection = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    // Full table rebuild. Wired to NotchModel's structural signals with
    // Qt::QueuedConnection so a rebuild never destroys the cell widget whose
    // signal is being emitted right now (the row Delete button above all).
    void rebuildTable();
    // Value-only refresh of one row. Destroys nothing, so it is safe to run
    // synchronously from inside a cell widget's own signal.
    void refreshRow(int notchId);
    void commitRow(int notchId);
    // Open the Settings-side edit window. Thetis's SetupForm.NotchAdminBusy is
    // AddActive | EditActive (setup.cs:17728-17735 [v2.10.3.15]); an in-place
    // table edit is the same window, opening on the first value change.
    void beginAdminEdit();
    // Clear the Settings-side edit lock. NotchModel's mutators are shared with
    // the panadapter path and reject writes while adminBusy is set, so every
    // page-side write clears it first, exactly as Thetis's ENTER button does.
    void endAdminEdit();
    // Re-read RXANBPGetMinNotchWidth off the active slice's channel and
    // re-arm the follow connection onto whichever channel that now is.
    void refreshMinNotchWidth();

    // ── Multi Notch Filter group ─────────────────────────────────────────
    QTableWidget*      m_notchTable{nullptr};
    class QPushButton* m_addBtn{nullptr};
    QCheckBox*         m_autoIncreaseChk{nullptr};
    QCheckBox*         m_visualNotchChk{nullptr};
    QLabel*            m_minWidthLbl{nullptr};

    // Follow connection onto the current RxChannel's minNotchWidthChanged.
    // Held so it can be dropped when the active slice moves to a different
    // channel; a stale one would keep reporting the old channel's value.
    QMetaObject::Connection m_minWidthConn;

    // Table row → NotchModel notch id. The row lambdas capture the id, never
    // the row, so a reorder cannot mis-target a notch.
    QList<int> m_rowIds;
    bool m_rebuilding{false};
};

} // namespace NereusSDR
