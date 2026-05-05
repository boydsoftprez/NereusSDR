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
#include "models/Band.h"

#include <QList>
#include <QPushButton>
#include <QStringList>
#include <QVector>

#include <optional>
#include <utility>

class QCheckBox;
class QComboBox;
class QPaintEvent;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QSlider;
class QSpinBox;
class QStackedWidget;

namespace NereusSDR {

enum class HPSDRModel : int;
class FilterPassbandWidget;
class PanadapterModel;
class SliceModel;

// RxApplet — per-slice RX controls applet.
//
// Controls (17 total):
//  1.  Slice badge (A/B/C/D)
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

    // Set the slice letter badge (0=A, 1=B, 2=C, 3=D)
    void setSliceIndex(int idx);

    // --- Auto AGC-T visual update (Task 7 — matches VfoWidget) ---
    void updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset);

    // Set the antenna list shown in the RX/TX antenna menus.
    void setAntennaList(const QStringList& ants);

public slots:
    // Phase 3P-I-a T16 — gate ANT buttons on caps.hasAlex + antenna count.
    // Hidden on HL2/Atlas and any board without an Alex front-end.
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

    // Per-SKU UI overlay for antenna popup (B3) — mirrors VfoWidget::setHpsdrSku.
    // Called by MainWindow on currentRadioChanged after setBoardCapabilities.
    void setHpsdrSku(NereusSDR::HPSDRModel sku);

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

    // Test-only: returns antenna number (1/2/3) shown by each button.
    // Phase 3P-F Task 4: verifies per-band wiring to AlexController.
    int activeRxAntennaForTest() const;
    int activeTxAntennaForTest() const;

    // Test-only: current text of the ATT/S-ATT/A-ATT label.
    // Issue #174: verifies the mi0bot-Thetis console.cs:21342-21365
    // [v2.10.3.13-beta2] HL2 A-ATT label flip on auto-att toggle.
    QString attLabelTextForTest() const;
private:
#endif

signals:
    void autoAgcToggled(bool on);
    void openSetupRequested();

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

    static QString formatFilterWidth(int low, int high);

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
