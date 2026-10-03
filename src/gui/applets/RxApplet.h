#pragma once

// =================================================================
// src/gui/applets/RxApplet.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.resx (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-23 - R-R3-46 / R-R3-21: in a remote window the attenuator row
//                 follows the Core's `stepAtt` object. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 1): the filter-preset Shift-click TX
//                 passband match follows setTransmitSettingsPermitted and
//                 says why when it cannot. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 15: the applet carries the bound
//                 slice's access. On a slice another device controls, every
//                 shared tuning and DSP control is disabled with the reason
//                 naming that device and never writes the slice; the tabs
//                 say who controls each slice, and the tab and badge menus
//                 offer Take control, Stop listening and Release. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 - TX rulings (item 3, JJ): on a listened slice the
//                 attenuator and preamp controls are held too, with the same
//                 reason. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-30: Fix wave GUI-M5: m_stepAttConnections. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

//
// Upstream source 'Project Files/Source/Console/console.resx' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

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

#include "AppletWidget.h"
#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"
#include "core/WdspTypes.h"
#include "gui/widgets/TriBtn.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"

#include <QHash>
#include <QList>
#include <QPushButton>
#include <QStringList>
#include <QVector>

#include <optional>
#include <utility>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QPaintEvent;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QMenu;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QToolButton;
class QWidget;

namespace NereusSDR {

enum class HPSDRModel : int;
class FilterPassbandWidget;
class PanadapterModel;
class SliceModel;

// RxApplet — per-slice RX controls applet.
//
// Controls (17 total):
//  1.  Slice badge (stable slice-ID letter, A onward)
//  2.  Lock button (checkable, NYI)
//  3.  RX antenna button (Tier 1 wired)
//  4.  TX antenna button (Tier 1 wired)
//  5.  Filter width label
//  6.  Mode combo (Tier 1 wired)
//  7.  Filter preset buttons × 10 (Tier 1 wired)
//  8.  FilterPassband widget (ported from AetherSDR, Tier 1 wired)
//  9.  AGC combo (Tier 1 wired)
//  10. AGC threshold slider (NYI — setAgcThreshold not in SliceModel yet)
//  11. AF gain slider (removed §B4 — TitleBar + VfoWidget cover it)
//  12. Mute button (removed §B4 bench review — VfoWidget + TitleBar are the 2 surfaces)
//  13. Audio pan slider (NYI)
//  14. Squelch toggle + slider (NYI)
//  15. RIT toggle + offset + zero (NYI)
//  16. XIT toggle + offset + zero (NYI)
//  17. Step size + up/down (NYI)
class RxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit RxApplet(SliceModel* slice, RadioModel* model,
                      QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("rx"); }
    QString appletTitle() const override { return QStringLiteral("RX"); }
    void    syncFromModel() override;

    // Attach to a different slice (or nullptr to detach).
    void setSlice(SliceModel* slice);
    // Slice control plan Task 15: the slice the applet shows and edits.
    SliceModel* slice() const { return m_slice; }

    // Set the slice letter badge (0=A, 1=B, and so on).
    void setSliceIndex(int idx);

    // Phase 3F (Bug 3): rebuild the per-slice tab row to match the live slice
    // list and check the active slice. Hidden when <= 1 slice (the static
    // badge suffices). Clicking a tab emits sliceActivationRequested.
    // Workflow ported from AetherSDR RxApplet::updateSliceButtons
    // (RxApplet.cpp:1434 [@6a142807]); the Multi-Flex foreign-slot model is
    // dropped (NereusSDR owns the radio directly, no shared-client slots).
    void updateSliceButtons(const QVector<SliceModel*>& slices,
                            int activeSliceIndex);

    // --- Auto AGC-T visual update (Task 7 — matches VfoWidget) ---
    void updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset,
                              bool noiseFloorValid = true);

    // Set the antenna list shown in the RX/TX antenna menus.
    void setAntennaList(const QStringList& ants);

    // Slice control plan Task 15: who controls the bound slice, from this
    // window's point of view (the flag's SliceAccess). Listening disables
    // every shared tuning and DSP control with heldReason as its tooltip
    // and the applet never writes the slice; Controlled and Unshared
    // restore them. TX rulings (JJ, 2026-09-30, item 3): the attenuator and
    // preamp controls are held on a listened slice too.
    void setSliceAccess(const VfoWidget::SliceAccess& access);
    const VfoWidget::SliceAccess& sliceAccess() const { return m_sliceAccess; }
    bool isListening() const
    {
        return m_sliceAccess.state == VfoWidget::SliceAccess::State::Listening;
    }
    // The access of every slice with a tab, keyed by slice id: the tab
    // tooltips say who controls each slice, and the tab and badge menus
    // offer the matching access actions.
    void setSliceTabAccess(const QHash<int, VfoWidget::SliceAccess>& access);
    // While a Take control, Stop listening or Release request waits on the
    // Core, the access actions are disabled with this text; empty clears it.
    void setSliceAccessPending(const QString& pending);
    // Adds the access actions for slice `sliceId` to `menu`: Take control
    // and Stop listening on a listened slice, Release on a controlled one,
    // nothing on an unshared one.
    void populateSliceMenu(QMenu& menu, int sliceId);

public slots:
    // Phase 3P-I-a T16 — gate ANT buttons on caps.hasAlex + antenna count.
    // Hidden on HL2/Atlas and any board without an Alex front-end.
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

    // Per-SKU UI overlay for antenna popup (B3) — mirrors VfoWidget::setHpsdrSku.
    // Called by MainWindow on currentRadioChanged after setBoardCapabilities.
    void setHpsdrSku(NereusSDR::HPSDRModel sku);

    // R-R3-49 (parity Task 1): the transmit settings gate. The
    // filter-preset Shift-click TX passband match follows it; while it is
    // closed the RX preset still applies and transmitSettingRefused says
    // why. A remote-station model starts denied.
    void setTransmitSettingsPermitted(bool permitted, const QString& reason = QString());

#ifdef NEREUS_BUILD_TESTS
public:
    // Test-only: returns current step-att spinbox maximum (for range assertions).
    // Phase 3P-A Task 15.
    int stepAttMaxForTest() const;

    // Test-only: returns the number of visible ADC OVL badges.
    // Phase 3P-B Task 10: 1 for single-ADC boards, 2 for dual-ADC boards.
    int visibleOvlBadgeCountForTest() const;

    // Test-only: returns the item count in the preamp combo at construction.
    // Phase 3P-C Step 3: verifies per-board populate from BoardCapabilities.
    int preampComboItemCountForTest() const;
    // R-R3-46: the preamp items shown, and the S-ATT minimum.
    QStringList preampComboLabelsForTest() const;
    int stepAttMinForTest() const;

    // Test-only: returns antenna number (1/2/3) shown by each button.
    // Phase 3P-F Task 4: verifies per-band wiring to AlexController.
    int activeRxAntennaForTest() const;
    int activeTxAntennaForTest() const;

    // Test-only: current text of the ATT/S-ATT/A-ATT label.
    // Issue #174: verifies the mi0bot-Thetis console.cs:21342-21365
    // [v2.10.3.13-beta2] HL2 A-ATT label flip on auto-att toggle.
    QString attLabelTextForTest() const;

    // Slice control plan Task 15: the controls held while listening, and a
    // slice tab's tooltip.
    QList<QWidget*> heldControlsForTest() const { return listeningHeldControls(); }
    QString sliceTabToolTipForTest(int sliceId) const;
private:
#endif

signals:
    void autoAgcToggled(bool on);
    void openSetupRequested();
    // Phase 3F (Bug 3): a slice tab was clicked; MainWindow routes this to
    // RadioModel::setActiveSlice. Mirrors AetherSDR
    // RxApplet::sliceActivationRequested (RxApplet.h:96 [@6a142807]).
    void sliceActivationRequested(int sliceIndex);
    // R-R3-49 (parity Task 1): a Shift-click could not also set the TX
    // passband; `reason` is plain words for the operator.
    void transmitSettingRefused(const QString& reason);
    // Slice control plan Task 15: the access actions of the tab and badge
    // menus, carrying the slice id. MainWindow runs them against the Core
    // the same way as the flag's.
    void takeControlRequested(int sliceId);
    void releaseRequested(int sliceId);
    void stopListeningRequested(int sliceId);

private:
    void buildUi();
    void connectSlice(SliceModel* s);
    void disconnectSlice(SliceModel* s);
    void updateFilterLabel();
    void rebuildFilterButtons(DSPMode mode);
    void updateFilterButtons();
    void applyFilterPreset(int low, int high);

    // Phase 3P-F Task 4: read AlexController per-band assignments and push
    // them into SliceModel so the antenna buttons reflect the active band.
    void populateAntennaButtons(NereusSDR::Band band);
    // R-R3-46: preamp items and S-ATT range for the Core's board (remote).
    void rebuildPreampAndAttRangeForBoard(NereusSDR::HPSDRHW board, bool alexFilters,
                                          int minDb);
    // R-R3-46 / R-R3-21: a remote window's ATT/S-ATT row, preamp combo and
    // RX1 preamp toggle follow the Core's `stepAtt` object and write to it.
    void wireRemoteStepAtt();
    // Enables the row while the Core takes its edits; otherwise disables it
    // with the plain reason the object carries.
    void applyRemoteStepAttAvailability();
    // Shows the object's values in the row (signals blocked).
    void showRemoteStepAttValues();
    // R-R3-46 / R-R3-11: the S-ATT value of this slice's own ADC (the other
    // ADC's own attenuator for a slice on it), local or remote.
    void showStepAttValueForSlice();
    // R-R3-46 / R-R3-11: label, shown control and preamp availability of
    // the slice's own ADC.
    void refreshAttForSlice();
    // Level Cal: the preamp items of slice A's input (RX1's list) or, for a
    // slice on the other ADC, RX2's own (Thetis comboRX2Preamp's list),
    // keeping the current choice when the list offers it.
    void fillPreampCombo(bool rx2);
    // Level Cal: selects the preamp mode the combo's list belongs to
    // (RX1's, or RX2's own for a slice on the other ADC), local or remote.
    void showPreampModeForSlice();
    // Builds the RX1 preamp toggle (dual-ADC boards) into the OVL row once;
    // later calls return the existing one. R-R3-46: a remote window learns
    // its board only when the Core's radio arrives, so it builds it then.
    void ensureRx1PreampToggle();

    static QString formatFilterWidth(int low, int high);

    // Slice control plan Task 15: hold or restore every shared control for
    // the current access, and the list of those controls.
    void applySliceAccess();
    void holdForListening(QWidget* control);
    QList<QWidget*> listeningHeldControls() const;
    // TX rulings (item 3): an attenuator or preamp control's own enabled
    // state and tooltip; while held for listening they are kept for the
    // restore instead.
    void setAttControlState(QWidget* control, bool enabled, const QString& tip);
    void showSliceMenu(int sliceId, QWidget* anchor, const QPoint& pos);

    VfoWidget::SliceAccess             m_sliceAccess;
    QHash<int, VfoWidget::SliceAccess> m_tabAccess;
    QString                            m_accessPending;
    int                                m_badgeSliceId = 0;  // the badge's slice id

    // ── Model ──────────────────────────────────────────────────────────────
    SliceModel*      m_slice = nullptr;
    PanadapterModel* m_pan   = nullptr;  // observed for bandChanged (Phase 3P-F Task 4)
    QStringList m_antList{QStringLiteral("ANT1"), QStringLiteral("ANT2"), QStringLiteral("ANT3")};

    // Stored board capabilities and SKU profile for antenna popup construction
    // (AntennaPopupBuilder B3). Populated by setBoardCapabilities() + setHpsdrSku().
    // Optional so we can detect "not yet set" (null = no radio connected).
    std::optional<BoardCapabilities> m_popupCaps;
    std::optional<SkuUiProfile>      m_popupSku;

    // Filter presets for the active mode — (low_hz, high_hz) pairs from SliceModel::presetsForMode().
    // Rebuilt on every dspModeChanged via rebuildFilterButtons(mode).
    QList<std::pair<int, int>> m_filterPresets;

    // ── Phase 3F (Bug 3): per-slice tab row (above Row 1) ─────────────────
    // One checkable QToolButton per live slice (A/B/C...). Hidden when the
    // slice count is <= 1. Exclusive group; the active slice is checked.
    QWidget*               m_sliceTabRow   = nullptr;
    QHBoxLayout*           m_sliceTabLayout= nullptr;
    QButtonGroup*          m_sliceGroup    = nullptr;
    QVector<QToolButton*>  m_sliceBtns;

    // ── Row 1: badge | lock | rx ant | tx ant | filter label ──────────────
    QLabel*      m_sliceBadge     = nullptr;   // Control 1
    QPushButton* m_lockBtn        = nullptr;   // Control 2
    QPushButton* m_rxAntBtn       = nullptr;   // Control 3
    QPushButton* m_txAntBtn       = nullptr;   // Control 4
    QLabel*      m_filterWidthLbl = nullptr;   // Control 5

    // ── Mode combo ────────────────────────────────────────────────────────
    QComboBox*   m_modeCombo      = nullptr;   // Control 6

    // ── Left column ───────────────────────────────────────────────────────
    // Control 17: Step size row
    TriBtn*      m_stepDown       = nullptr;
    QLabel*      m_stepLabel      = nullptr;
    TriBtn*      m_stepUp         = nullptr;

    // Control 7: Filter preset grid (10 buttons, 3×4 layout)
    QVector<QPushButton*> m_filterBtns;
    QWidget*     m_filterContainer = nullptr;
    QGridLayout* m_filterGrid      = nullptr;

    // Control 8: FilterPassband (ported from AetherSDR FilterPassbandWidget)
    FilterPassbandWidget* m_filterPassband = nullptr;

    // ── Right column ──────────────────────────────────────────────────────
    // Mute button removed §B4 bench review — VfoWidget + TitleBar are the 2 surfaces.
    // Control 13: Audio pan
    QSlider*     m_panSlider   = nullptr;

    // Control 14: Squelch
    QPushButton* m_sqlBtn      = nullptr;
    QSlider*     m_sqlSlider   = nullptr;

    // ATT/S-ATT row (between Squelch and AGC)
    QLabel*         m_attLabel{nullptr};
    QStackedWidget* m_attStack{nullptr};
    QComboBox*      m_preampCombo{nullptr};   // Page 0: ATT mode
    // Level Cal: the board the preamp lists come from, and whether the
    // combo holds RX2's list (a slice on the other ADC).
    NereusSDR::HPSDRHW m_preampBoard{NereusSDR::HPSDRHW::Hermes};
    bool            m_preampAlex{false};
    bool            m_preampShowsRx2{false};
    // GUI-M5 (fix wave): connectSlice's attenuator connections, dropped
    // before the next slice's are made.
    QList<QMetaObject::Connection> m_stepAttConnections;
    QSpinBox*       m_stepAttSpin{nullptr};   // Page 1: S-ATT mode

    // Controls 9 + 10: AGC
    QComboBox*   m_agcCombo    = nullptr;   // Control 9
    QSlider*     m_agcTSlider  = nullptr;   // Control 10
    QWidget*     m_agcTContainer{nullptr};
    QLabel*      m_agcTLabelWidget{nullptr};
    QLabel*      m_agcTLabel{nullptr};       // dB value readout
    QPushButton* m_agcAutoLabel{nullptr};  // clickable AUTO toggle
    QLabel*      m_agcInfoLabel{nullptr};
    bool         m_autoAgcActive{false};
    float        m_noiseFloorDbm{-200.0f};

    // Control 15: RIT
    QPushButton* m_ritOnBtn    = nullptr;
    QLabel*      m_ritLabel    = nullptr;
    QPushButton* m_ritZero     = nullptr;
    TriBtn*      m_ritMinus    = nullptr;
    TriBtn*      m_ritPlus     = nullptr;

    // Control 16: XIT
    bool         m_transmitSettingsPermitted = true;  // R-R3-49
    QString      m_transmitSettingsReason;            // R-R3-49
    QPushButton* m_xitOnBtn    = nullptr;
    QLabel*      m_xitLabel    = nullptr;
    QPushButton* m_xitZero     = nullptr;
    TriBtn*      m_xitMinus    = nullptr;
    TriBtn*      m_xitPlus     = nullptr;

    // Phase 3P-B Task 10: per-ADC ADC OVL badges.
    // Index 0 = ADC0 ("OVL" on single-ADC boards, "OVL₀" on dual-ADC).
    // Index 1 = ADC1 ("OVL₁" on dual-ADC boards only; nullptr on single-ADC).
    // Gate: BoardCapabilities::p2PreampPerAdc — true for OrionMKII family.
    // (p2PreampPerAdc is the proxy for "dual-ADC board" added in Task 6.)
    QLabel*      m_ovlBadges[3]{nullptr, nullptr, nullptr};
    QHBoxLayout* m_ovlRow{nullptr};

    // Phase 3P-B Task 10: RX1 preamp toggle for dual-ADC boards only.
    // Visible only when BoardCapabilities::p2PreampPerAdc=true.
    QCheckBox*   m_rx1PreampToggle{nullptr};
};

} // namespace NereusSDR
