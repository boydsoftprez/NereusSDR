#pragma once

// =================================================================
// src/gui/widgets/VfoWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.resx (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/specHPSDR.cs, original licence from Thetis source is included below
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
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
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

/*
*
* Copyright (C) 2010-2018  Doug Wigley 
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#pragma once

// =================================================================
// src/gui/widgets/VfoWidget.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Floating VFO-flag widget ported from AetherSDR
//                 `src/gui/VfoWidget.{h,cpp}`. DSP field values
//                 (frequency, mode, filter, AGC) come from Thetis
//                 `console.cs`; see Copyright block.
// =================================================================

#include "core/WdspTypes.h"
#include "models/SliceModel.h"
#include "VfoLevelBar.h"
#include "ScrollableLabel.h"
#include "VfoModeContainers.h"

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPointer>

namespace NereusSDR {

class VaxChannelSelector;  // forward declaration — full include in VfoWidget.cpp
struct BoardCapabilities;  // forward declaration — Phase 3P-I-a T15
enum class HPSDRModel : int;  // forward declaration — Phase 3P-I-b T9

// Floating VFO flag widget — AetherSDR pattern.
// Each slice gets one VfoWidget, parented to SpectrumWidget.
// Positioned at the VFO marker via move() from updatePosition().
//
// Layout (top to bottom):
//   Header:    [RX ANT] [TX ANT] [Filter] ... [TX] [A]
//   Frequency: "14.225.000  MHz"  (26px mono, click-edit, wheel-tune)
//   S-Meter:   [████████░░░]  -85 dBm
//   Tab bar:   Audio | DSP | Mode | X/RIT
//   Tab pages: (collapsible content)
//
// From AetherSDR VfoWidget.h pattern.
class VfoWidget : public QWidget {
    Q_OBJECT

public:
    // Direction hint for positioning the flag relative to the VFO marker.
    enum class FlagDir { Auto, ForceLeft, ForceRight };

    explicit VfoWidget(QWidget* parent = nullptr);
    ~VfoWidget() override;

    // Parse a user-entered frequency string and return Hz, or -1.0 on failure.
    // Accepts MHz decimal ("7.23"), EU decimal ("7,23"), EU thousand-separated
    // Hz ("7.230.000"), US thousand-separated Hz ("7,230,000"), explicit unit
    // suffix ("7.23 MHz" / "7230 kHz" / "7230000 Hz" / "7.23M" / "7230k"), or
    // a plain number (best-fit unit by Red Pitaya range). Separate from the
    // returnPressed lambda so the logic is unit-testable. See issue #73.
    static double parseUserFrequency(const QString& raw);

    // --- State setters (called from model, guarded against re-emit) ---

    void setFrequency(double hz);
    void setMode(DSPMode mode);
    void setFilter(int low, int high);
    void setAgcMode(AGCMode mode);
    void setAfGain(int gain);
    void setRfGain(int gain);
    void setRxAntenna(const QString& ant);
    void setTxAntenna(const QString& ant);
    void setStepHz(int hz);
    void setSliceIndex(int index);
    void setTxSlice(bool isTx);
    void setAntennaList(const QStringList& ants);
    void setSmeter(double dbm);

    // --- RIT/XIT state setters (S1.8a — guarded against re-emit) ---
    void setRitEnabled(bool v);
    void setRitHz(int hz);
    void setXitEnabled(bool v);
    void setXitHz(int hz);

    // --- Lock state setter (S1.8a review — syncs both m_lockBtn and m_xritLockBtn) ---
    void setLocked(bool v);

    // --- DSP tab state setters (S1.8b — guarded against re-emit) ---
    // Thetis chkNB CheckState mirror — see console.cs:43513-43560 [v2.10.3.13].
    void setNbMode(NereusSDR::NbMode m);
    void setNr2Enabled(bool v);
    void setSnbEnabled(bool v);
    void setApfEnabled(bool v);
    void setApfTuneHz(int hz);

    // --- Mode container visibility (S1.9) ---
    // Shows/hides the three mode containers embedded in DspTab based on the
    // current demodulation mode, and re-evaluates the APF tune slider visibility.
    void applyModeVisibility(DSPMode mode);

    // --- Audio tab state setters (S1.8c — guarded against re-emit) ---
    void setMuted(bool v);
    void setAudioPan(double pan);      // drives m_panSlider → round(pan * 100)
    void setSsqlEnabled(bool v);
    void setSsqlThresh(double dB);     // drives m_sqlSlider → round(dB)
    void setAgcThreshold(int dBu);
    void setBinauralEnabled(bool v);

    // --- Auto AGC-T visual update (Task 6) ---
    void updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset);

    // --- Slice coupling (for mode container binding only) ---
    void setSlice(SliceModel* slice);

    // --- Positioning ---

    // Reposition the flag at the given pixel x of the VFO marker.
    // specTop is the y of the spectrum widget's top edge.
    void updatePosition(int vfoX, int specTop, FlagDir dir = FlagDir::Auto);

    int sliceIndex() const { return m_sliceIndex; }

public slots:
    // Phase 3P-I-a T15 — hide Blue/Red ANT buttons when the connected
    // board has no Alex filter (HL2 / Atlas). Called by MainWindow on
    // RadioModel::currentRadioChanged.
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

    // Phase 3P-I-b T9 — BYPS button visibility gates on both caps.hasRxBypassRelay
    // AND SkuUiProfile.hasRxBypassUi (ANAN10/ANAN8000D/G2/G2_1K etc. suppress).
    void setHpsdrSku(NereusSDR::HPSDRModel sku);

    // Phase 3P-I-b T9 — reflect AlexController::rxOutOnTx state into the BYPS button.
    void setRxBypassActive(bool on);

signals:
    void frequencyChanged(double hz);
    void modeChanged(NereusSDR::DSPMode mode);
    void filterChanged(int low, int high);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void afGainChanged(int gain);
    void rfGainChanged(int gain);
    void rxAntennaChanged(const QString& ant);
    void txAntennaChanged(const QString& ant);
    void rxBypassToggled(bool on);     // Phase 3P-I-b T9 — BYPS button click
    // Emitted when the user clicks the NB button. MainWindow cycles the
    // slice's NbMode in response.
    void nbModeCycled();
    // Emitted when the user right-clicks the NB or SNB button. MainWindow
    // opens the Setup dialog to the "NB/SNB" page. Mirrors Thetis
    // chkNB_MouseDown (console.cs:44447 [v2.10.3.13]) which calls
    // SetupForm.ShowSetupTab(SetupTab.NB_Tab).
    void openNbSetupRequested();
    void nrChanged(bool enabled);
    void anfChanged(bool enabled);
    void sliceActivationRequested(int sliceIndex);
    void closeRequested(int sliceIndex);
    void lockChanged(bool locked);

    // --- X/RIT tab signals (S1.8a) ---
    void ritEnabledChanged(bool enabled);
    void ritHzChanged(int hz);
    void xitEnabledChanged(bool enabled);
    void xitHzChanged(int hz);
    void stepCycleRequested();

    // --- DSP tab signals (S1.8b) ---
    void nr2Changed(bool enabled);    // maps to emnrEnabled in SliceModel
    void snbChanged(bool enabled);
    void apfChanged(bool enabled);
    void apfTuneHzChanged(int hz);

    // --- Audio tab signals (S1.8c) ---
    void panChanged(double pan);           // -1.0 to 1.0
    void muteChanged(bool muted);
    void binauralChanged(bool enabled);
    void squelchEnabledChanged(bool enabled);
    void squelchThreshChanged(int thresh);
    void agcThreshChanged(int dBu);
    void autoAgcToggled(bool on);

    // --- Mode tab signals (S1.8c) ---
    void quickModeRequested(int index);

    // --- Record/play signals (S1.10 — NYI in Stage 1, wired to future recording subsystem) ---
    void recordToggled(bool recording);
    void playToggled(bool playing);

    // --- NR setup dialog request (right-click on any NR bank button → Task 18) ---
    void openNrSetupRequested(NereusSDR::NrSlot slot);

    // --- Setup dialog request (e.g. AGC-T right-click → open settings) ---
    void openSetupRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void buildUI();
    void buildHeaderRow();
    void buildFrequencyRow();
    void buildSmeterRow();
    void buildTabBar();
    void buildAudioTab();
    void buildDspTab();
    void buildModeTab();
    void buildXRitTab();
    void rebuildFilterButtons(DSPMode mode);
    void updateFreqLabel();
    QString formatFilterWidth(int low, int high) const;

    // --- NR bank helpers (Sub-epic C-1, Tasks 14-15) ---
    void onActiveNrChanged(NereusSDR::NrSlot slot);
    void showNr1Popup(const QPoint& globalPos);
    void showNr2Popup(const QPoint& globalPos);
    void showNr3Popup(const QPoint& globalPos);
    void showNr4Popup(const QPoint& globalPos);
    void showDfnrPopup(const QPoint& globalPos);
    void showBnrPopup(const QPoint& globalPos);
    void showMnrPopup(const QPoint& globalPos);

    // Guard to prevent signal re-emission during model updates
    bool m_updatingFromModel{false};

    // Internal helper — update m_locked + drive both lock buttons + emit lockChanged.
    // Called by both the floating m_lockBtn toggled lambda and m_xritLockBtn toggled
    // lambda so both paths are in sync.  Must be called outside m_updatingFromModel.
    void applyLockedState(bool on);

    // Slice identity
    int m_sliceIndex{0};
    int m_stepHz{100};
    double m_frequency{14225000.0};
    DSPMode m_currentMode{DSPMode::USB};
    double m_smeterDbm{-127.0};

    // Fixed width from AetherSDR VfoWidget
    static constexpr int kWidgetW = 252;

    bool m_hasAlex{true};   // default true until caps land; preserves
                            // existing behavior during discovery.
    bool m_hasRxBypassRelay{false};    // Phase 3P-I-b T9 — BYPS button gate (caps)
    bool m_hasRxOutOnTxUi{false};      // Phase 3P-I-b T9 — BYPS button gate (SKU)

    // --- Header row ---
    QPushButton* m_rxAntBtn{nullptr};
    QPushButton* m_rxBypassBtn{nullptr};  // Phase 3P-I-b T9 — grey BYPS toggle
    QPushButton* m_txAntBtn{nullptr};
    QLabel*      m_filterWidthLbl{nullptr};
    QPushButton* m_txBadge{nullptr};
    QLabel*      m_splitBadge{nullptr};
    QLabel*      m_sliceBadge{nullptr};
    QStringList  m_antennaList{QStringLiteral("ANT1"), QStringLiteral("ANT2"), QStringLiteral("ANT3")};

    // --- Frequency row ---
    QStackedWidget* m_freqStack{nullptr};
    QLabel*         m_freqLabel{nullptr};
    QLineEdit*      m_freqEdit{nullptr};

    // --- S-meter row ---
    VfoLevelBar* m_levelBar{nullptr};

    // --- Tab bar ---
    QList<QPushButton*> m_tabButtons;
    QStackedWidget*     m_tabStack{nullptr};
    int                 m_activeTab{0};

    // --- Mode tab ---
    QComboBox*          m_modeCmb{nullptr};
    QWidget*            m_filterBtnContainer{nullptr};

    // --- Audio tab ---
    QSlider*            m_afGainSlider{nullptr};
    QLabel*             m_afGainLabel{nullptr};
    QPushButton*        m_agcBtns[5]{};          // Off/Long/Slow/Med/Fast — replaces m_agcCmb
    QSlider*            m_panSlider{nullptr};
    QLabel*             m_panLabel{nullptr};
    QPushButton*        m_muteBtn{nullptr};
    QPushButton*        m_binBtn{nullptr};
    QPushButton*        m_sqlBtn{nullptr};
    QSlider*            m_sqlSlider{nullptr};
    QSlider*            m_agcTSlider{nullptr};
    QLabel*             m_agcTLabel{nullptr};
    QWidget*            m_agcTContainer{nullptr};   // wraps the entire AGC-T row
    QLabel*             m_agcTLabelWidget{nullptr}; // "AGC-T" text label
    QPushButton*        m_agcAutoLabel{nullptr};    // "AUTO" badge — clickable toggle
    QLabel*             m_agcInfoLabel{nullptr};    // info sub-line
    bool                m_autoAgcActive{false};
    float               m_noiseFloorDbm{-200.0f};

    // --- DSP tab ---
    QPushButton*        m_nbButton{nullptr};   // tri-state NB/NB2/Off cycling button
    // NR bank — Sub-epic C-1: 7 mutually-exclusive slot buttons.
    QPushButton* m_nr1Btn  = nullptr;
    QPushButton* m_nr2Btn  = nullptr;
    QPushButton* m_nr3Btn  = nullptr;
    QPushButton* m_nr4Btn  = nullptr;
    QPushButton* m_dfnrBtn = nullptr;
    QPushButton* m_bnrBtn  = nullptr;
    QPushButton* m_mnrBtn  = nullptr;
    QPushButton*        m_anfToggle{nullptr};
    QPushButton*        m_snbToggle{nullptr};
    QPushButton*        m_apfToggle{nullptr};
    QLabel*             m_apfLabel{nullptr};        // "APF" row label (S1.9 — promoted from local)
    QSlider*            m_apfTuneSlider{nullptr};
    QLabel*             m_apfTuneLabel{nullptr};
    FmOptContainer*        m_fmContainer{nullptr};
    DigOffsetContainer*    m_digContainer{nullptr};
    RttyMarkShiftContainer* m_rttyContainer{nullptr};

    // --- VAX channel selector (Phase 3O Sub-Phase 8) — lives inside the VAX tab ---
    VaxChannelSelector*  m_vaxSelector{nullptr};

    // --- Slice coupling (for mode container binding only) ---
    QPointer<SliceModel> m_slice;

    // --- X/RIT tab ---
    QPushButton*   m_ritBtn{nullptr};
    ScrollableLabel* m_ritLabel{nullptr};
    QPushButton*   m_ritZeroBtn{nullptr};
    QPushButton*   m_xitBtn{nullptr};
    ScrollableLabel* m_xitLabel{nullptr};
    QPushButton*   m_xitZeroBtn{nullptr};
    QPushButton*   m_xritLockBtn{nullptr};
    QPushButton*   m_stepCycleBtn{nullptr};

    // --- Floating control buttons (AetherSDR pattern) ---
    // Rendered as children of parent widget, positioned beside the VFO flag.
    QPushButton* m_closeBtn{nullptr};
    QPushButton* m_lockBtn{nullptr};
    QPushButton* m_recBtn{nullptr};
    QPushButton* m_playBtn{nullptr};
    bool m_locked{false};
    bool m_onLeft{false};  // track flag side for button placement
    void buildFloatingButtons();
    void positionFloatingButtons();

    // Slice color table: A=cyan, B=magenta, C=green, D=yellow
    static QColor sliceColor(int index);
};

} // namespace NereusSDR
