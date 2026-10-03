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
//   2026-09-30 - VFO flag crash lane: lockButtonForTest, sliceForTest.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-30 - RADE reason: setRadeReason, the RADE row's "off" state and
//                 tooltip. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-23 : R-R3-40 NNR step-back indicator on the NNR button, by
//                 J.J. Boyd (KG4VCF), with Anthropic Claude Code
//                 assistance.
//   2026-09-23 : R-R3-45 Speakers and Headphones buttons in the audio
//                 block (VAX design 6.2), with a plain notice when the
//                 headphones are chosen but none are set up. By J.J. Boyd
//                 (KG4VCF), with Anthropic Claude Code assistance.
//   2026-09-25 : R-R3-49, Sub-epic C-1 DFNR hidden while it cannot run
//                 (dfnrOffered, updateDfnrAvailability), by J.J. Boyd
//                 (KG4VCF), with Anthropic Claude Code assistance.
//   2026-09-25 : R-R3-49, Sub-epic C-1 (tx-followup-3) DFNR, MNR and BNR
//                 shown disabled with the plain reason, never hidden
//                 (nrCannotRunReason, updateNrAvailability), by J.J. Boyd
//                 (KG4VCF), with Anthropic Claude Code assistance.
//   2026-09-25 : R-R3-49, Sub-epic C-1 (tx-followup-4) the BNR button and
//                 its quick controls removed (not offered for now), by J.J.
//                 Boyd (KG4VCF), with Anthropic Claude Code assistance.
//   2026-09-26 : R-R3-49 / R-R3-21 (remote-window parity Task 16) DFNR and
//                 MNR offered by the station's noise reduction (the Core's
//                 in a remote window), disabled with the plain reason and
//                 never hidden (updateNrAvailability), by J.J. Boyd
//                 (KG4VCF), with Anthropic Claude Code assistance.
//   2026-09-26 : iPhone app plan Task 78 (R-IOS-02, R-IOS-30): a slice the
//                 radio's own PTT transmits on shows as in use by the
//                 radio. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-29 : Slice control plan Task 14b (ruling U5): on a listened
//                 flag the AF slider and Mute are this device's own
//                 volume, labeled "Your volume". J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-30 : core-slice take-over: SliceAccess::takeHeldReason.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 : TX badge take (JJ's ruling): TxBadgeOffer and
//                 txTakeRequested, a TX badge click that starts the take of
//                 transmit or of the slice. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-01: Completed attribution for bright and dim slice palette, A through H
//                 by J.J. Boyd (KG4VCF), with AI assistance via
//                 OpenAI Codex. Port introduced 2026-09-23.
//                 Source: AetherSDR src/gui/SliceColors.h [@0cd4559].
//                 Upstream has no per-file copyright header.
//                 Copyright (C) 2024-2026 Jeremy (KK7GWY) and
//                 AetherSDR contributors. GPLv3; project source:
//                 https://github.com/ten9876/AetherSDR
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

#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"
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
#include <QMenu>

#include <array>
#include <limits>
#include <optional>

namespace NereusSDR {

class VaxChannelSelector;  // forward declaration — full include in VfoWidget.cpp
class RadioModel;          // forward declaration — Phase 3F closeout (AntennaPickerMenu wiring)
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
    // SpectrumWidget respects this before its per-frame show() pass.
    void setStationPresentationAllowed(bool allowed);
    bool stationPresentationAllowed() const { return m_stationPresentationAllowed; }
    /// iPhone app plan Task 78 (the several-devices design, ruling 8.11):
    /// the radio's own PTT is transmitting on this slice (the Core's
    /// txState keyed, holderSource radioPtt, txSliceId this slice). The TX
    /// badge shows it in amber, says so, and asks nothing on a click.
    void setInUseByRadio(bool inUse);
    bool inUseByRadio() const { return m_inUseByRadio; }
    static QString inUseByRadioText();
    bool txSliceShown() const;

    /// Whether this flag's slice is the one the operator has selected:
    /// RadioModel's active slice, the single selection every pan shares. Not
    /// the pan's own front slice (SpectrumWidget::setFrontSliceIndex); with
    /// two pans open each pan has a front flag, but only one slice is
    /// selected.
    ///
    /// Read back by SpectrumWidget::sliceMarkerGeometry(), which draws the
    /// selected slice's marker in its full slice colour and every other
    /// slice's marker darker. MainWindow seeds it in createSliceFlag and fans
    /// every change out from RadioModel::activeSliceChanged, the same way it
    /// keeps the TX badge current. Defaults to true, so a flag nobody has
    /// told otherwise draws exactly as every marker did before the
    /// distinction existed.
    void setActiveSlice(bool active);
    bool isActiveSlice() const { return m_activeSlice; }

    void setAntennaList(const QStringList& ants);
    void setSmeter(double dbm);

    /// This flag's own slice frequency, in Hz.
    ///
    /// Phase 3F: read back by SpectrumWidget::updateVfoPositions so each flag
    /// on a shared pan is placed at ITS slice's frequency. The pan's m_vfoHz
    /// is a single value that tracks whichever slice most recently called
    /// setVfoFrequency, so placing from it stacked every co-hosted flag at one
    /// x. Bench-reported 2026-07-28: "A and B are still overlaid and stuck on
    /// top of each other."
    double frequency() const { return m_frequency; }

    /// This flag's own filter edges, as signed Hz offsets from its slice
    /// frequency (negative on the LSB side, exactly as SliceModel carries
    /// them).
    ///
    /// Phase 3F: read back by SpectrumWidget::sliceMarkerGeometry() so each
    /// co-hosted slice shades ITS OWN passband. Filter width is per slice, so
    /// the pan's single m_filterLowHz/m_filterHighHz pair cannot stand in for
    /// a neighbour's: a 500 Hz CW slice next to a 2.7 kHz SSB slice would draw
    /// two identical bands. Bench-reported 2026-07-28: "The pass band of the
    /// second flag disappears when not active."
    int filterLow()  const { return m_filterLowHz; }
    int filterHigh() const { return m_filterHighHz; }

    // --- RIT/XIT state setters (S1.8a — guarded against re-emit) ---
    void setRitEnabled(bool v);
    void setRitHz(int hz);
    void setXitEnabled(bool v);
    void setXitHz(int hz);

    // --- Lock state setter (S1.8a review — syncs m_lockBtn / Close-strip) ---
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

    // --- R-R3-45: speakers or headphones (VAX design 6.2) ---
    // Model -> widget, guarded against re-emit. The buttons write the bound
    // slice's route directly (wired in setSlice, like the VAX selector).
    void setOutputRoute(NereusSDR::SliceModel::OutputRoute route);
    // Whether a headphones output is open on this computer. With the
    // headphones chosen and none open, the flag says why it is silent.
    void setHeadphonesAvailable(bool available);
    // The words the flag shows in that case.
    static QString headphonesMissingText();
    // R-R3-45: in a remote window, why a receiver on the headphones is
    // silent although this computer has headphones (plain words from
    // RemoteMediaController::headphonesProblem()); empty when nothing is.
    // Shown in the same notice while the headphones are chosen.
    void setHeadphonesProblem(const QString& problem);
    // R-R3-45 fix wave: whether the headphones are turned on in Setup,
    // Audio, Devices. Turned on but not open, the flag says they could not
    // be opened rather than asking to turn them on.
    void setHeadphonesEnabled(bool enabled);
    static QString headphonesNotOpenedText();

    // --- Auto AGC-T visual update (Task 6) ---
    void updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset,
                              bool noiseFloorValid = true);

    // --- Small filter display mode (Task 3.4 — Appearance > Meter Styles) ---
    // R-R3-21: open one of the flag's tabs (the status bar's badges do).
    // Opening the tab already open leaves it open.
    enum class Tab { Audio = 0, Dsp = 1, Mode = 2, XRit = 3, Vax = 4 };
    void showTab(Tab tab);
    int activeTab() const { return m_activeTab; }

    void setSmallFilterMode(bool small);
    bool smallFilterMode() const { return m_smallFilterMode; }

    // --- Slice coupling (for mode container binding only) ---
    void setSlice(SliceModel* slice);

    // --- Test seams for SNR row (Phase 3R L1) ---
    // Exposed so tst_vfo_widget_snr can verify text/colour/visibility
    // contracts without depending on geometry-sensitive layout queries.
    QLabel* snrLabelForTest()      const { return m_snrLabel; }
    QLabel* snrValueLabelForTest() const { return m_snrValue; }

    // --- Stage C2: FilterPresetStore coupling ---
    // When set, rebuildFilterButtons reads user overrides from the store and
    // the right-click context menu on each filter button opens the edit dialog.
    // When nullptr, falls back to SliceModel::presetsForMode (Thetis defaults).
    void setFilterPresetStore(class FilterPresetStore* store);

    // --- Positioning ---

    // Reposition the flag at the given pixel x of the VFO marker.
    // specTop is the y of the spectrum widget's top edge.
    void updatePosition(int vfoX, int specTop, FlagDir dir = FlagDir::Auto);

    /// Destroy the four floating buttons (close / lock / record / play).
    ///
    /// They are parented to the SpectrumWidget, not to this flag, so deleting
    /// the flag alone leaves them painted on the pan forever -- visible as a
    /// second orphaned button column after a slice is removed. The destructor
    /// deliberately does NOT free them (issue #113: Qt's deleteChildren walked
    /// SpectrumWidget's children in an order that freed a button before
    /// ~VfoWidget ran, and the explicit delete then SIGSEGV'd on a dangling
    /// pointer). Calling this BEFORE the flag is destroyed avoids that
    /// entirely, because both objects are still alive and the order is ours.
    ///
    /// Idempotent; safe when the buttons were never built.
    void destroyFloatingButtons();

    /// Bring this flag, and its close/lock/record/play buttons, to the front
    /// of the shared parent's (SpectrumWidget's) stacking order.
    ///
    /// The buttons are parented to the SpectrumWidget rather than to this
    /// flag -- see destroyFloatingButtons above -- so QWidget::raise() on the
    /// flag alone only reorders the flag body. The buttons, as separate
    /// siblings, would stay wherever they last were, which can be behind a
    /// flag that used to sit above this one: a flag on top with its own
    /// button column stranded underneath a neighbour. Raising the buttons
    /// first and the flag body last matches the order
    /// positionFloatingButtons() already uses, so a flag brought to the
    /// front reads as one coherent unit.
    ///
    /// Safe to call before the buttons exist (a flag that has never had
    /// updatePosition() called on it yet): each pointer is guarded.
    void raiseAboveSiblings();

    // --- Test seam for floating-button z-order (Bench 2026-07-28, Sub-Epic J) ---
    // The four buttons are otherwise anonymous QPushButtons among the
    // SpectrumWidget's children, so a z-order test has no way to name "this
    // flag's close button" without this. Exposed read-only, same pattern as
    // the SNR-row seams below.
    QPushButton* closeButtonForTest() const { return m_closeBtn; }
    // VFO flag crash lane: the floating lock button, and the slice this
    // flag is bound to, for the remote window's close-and-reopen case.
    QPushButton* lockButtonForTest() const { return m_lockBtn; }
    SliceModel* sliceForTest() const { return m_slice.data(); }
    // R-R3-49: the floating record and play buttons, for the unbuilt
    // feature sweep.
    QPushButton* recordButtonForTest() const { return m_recBtn; }
    QPushButton* playButtonForTest() const { return m_playBtn; }
    // Fix wave I3: the last refused noise-reducer choice this flag showed.
    QString nrRefusalForTest() const { return m_nrRefusal; }
    // R-R3-49: the DSP grid's DFNR button (hidden while DFNR cannot run).
    QPushButton* dfnrButtonForTest() const { return m_dfnrBtn; }
    QPushButton* mnrButtonForTest() const { return m_mnrBtn; }

    int sliceIndex() const { return m_sliceIndex; }

    // Phase 3F Sub-Epic C Task 9: test-only seam to fire the TX-badge click
    // path without bringing up a QApplication + QPushButton event loop.
    // Always built (no NEREUSSDR_TESTING flag); cost is one indirect call
    // and the seam is undocumented in user-facing API.
    void simulateTxBadgeClick() { onTxBadgeClicked(); }

    // Narrow test seam for the context-menu stable-ID lookup. The returned
    // pointer is the same one contextMenuEvent passes to AntennaPickerMenu.
    NereusSDR::SliceModel* contextMenuSliceForTest() const;

    // Slice control and shared listening plan Task 14a: who controls the
    // slice this flag shows. Unshared is a slice nobody else reaches (no
    // line, no access actions). Controlled is this window's own control
    // ("You control", Release in the menu). Listening is a slice another
    // device controls: the flag keeps its letter and color, names the
    // controller, disables the shared tuning controls with heldReason,
    // never writes the slice, and offers Take control and Stop listening.
    struct SliceAccess {
        enum class State { Unshared, Controlled, Listening };
        State state{State::Unshared};
        QString line;
        QString heldReason;
        /// Core-slice take-over (JJ, 2026-09-30): why Take control is off
        /// on a listened slice (the Core's own words), or empty when it is
        /// offered. The action stays in the menu, disabled with it.
        QString takeHeldReason;
        bool operator==(const SliceAccess&) const = default;
    };
    void setSliceAccess(const SliceAccess& access);
    const SliceAccess& sliceAccess() const { return m_sliceAccess; }

    // TX badge take (JJ, 2026-09-30): what a click on the TX badge does
    // when it cannot make this slice the TX slice at once (another device
    // holds transmit, or this flag only listens). Offered: the badge is
    // enabled, says what a click will do, and a click emits
    // txTakeRequested. Not offered: the badge is held as before, with
    // heldReason when it is set (the slice is on the air) or today's reason.
    struct TxBadgeOffer {
        bool offered{false};
        QString toolTip;
        QString heldReason;
        bool operator==(const TxBadgeOffer&) const = default;
    };
    void setTxBadgeOffer(const TxBadgeOffer& offer);
    const TxBadgeOffer& txBadgeOffer() const { return m_txBadgeOffer; }
    bool isListening() const
    {
        return m_sliceAccess.state == SliceAccess::State::Listening;
    }
    // A request this flag sent is waiting for the Core. The text replaces
    // the access line and the access actions are disabled; empty clears it.
    void setSliceAccessPending(const QString& text);
    bool sliceAccessPending() const { return !m_accessPending.isEmpty(); }
    // The line shown under the header (pending text when a request waits).
    QString accessLineText() const;
    // Builds the right-click menu; contextMenuEvent shows what this adds.
    void populateContextMenu(QMenu& menu);

    // Slice control plan Task 14b (ruling U5): on a listened flag the AF
    // slider and Mute are this device's own volume and mute for the slice,
    // labeled "Your volume". They emit listenVolumeRequested and never
    // write the slice; the slice's AF and mute (setAfGain, setMuted) are
    // kept and shown again when the flag stops listening. level is 0..100.
    void setListenVolume(int level, bool muted);
    int listenVolume() const { return m_listenVolume; }
    bool listenMuted() const { return m_listenMuted; }

    // Test seams for Task 14a.
    QList<QWidget*> heldControlsForTest() const;
    // Test seams for Task 14b.
    QSlider* afSliderForTest() const { return m_afGainSlider; }
    QPushButton* muteButtonForTest() const { return m_muteBtn; }
    QString afNameForTest() const;
    QRect frequencyAreaForTest() const;
    bool frequencyEditOpen() const;

public slots:
    // Phase 3P-I-a T15 — hide Blue/Red ANT buttons when the connected
    // board has no Alex filter (HL2 / Atlas). Called by MainWindow on
    // RadioModel::currentRadioChanged.
    void setBoardCapabilities(const NereusSDR::BoardCapabilities& caps);

    // Phase 3P-I-b T9 — BYPS button visibility gates on both caps.hasRxBypassRelay
    // AND SkuUiProfile.hasRxBypassUi (ANAN10/ANAN8000D/G2/G2_1K etc. suppress).
    void setHpsdrSku(NereusSDR::HPSDRModel sku);
    // R-R3-46 test seam: the RX-only antenna labels the antenna menu uses
    // (empty until setHpsdrSku has run).
    QStringList rxOnlyAntennaLabelsForTest() const {
        return m_popupSku ? QStringList(m_popupSku->rxOnlyLabels.cbegin(),
                                        m_popupSku->rxOnlyLabels.cend())
                          : QStringList();
    }

    // Phase 3P-I-b T9 — reflect AlexController::rxOutOnTx state into the BYPS button.
    void setRxBypassActive(bool on);

    // Remote-station presentation gate.  TX-slice handoff remains displayed
    // but cannot issue client or station work. XIT is a slice setting and
    // does not follow it (R-R3-49, parity Task 11); nor does BYPS, which
    // follows setRxBypassPermitted (group B fix wave).
    void setTransmitPermitted(bool permitted, const QString& reason = QString());
    // Group B fix wave: whether BYPS (RX bypass on TX) may change the
    // radio's relay setting; disabled with `reason` when not.
    void setRxBypassPermitted(bool permitted, const QString& reason = QString());

    // Phase 3F closeout — non-owning RadioModel pointer used by contextMenuEvent
    // to construct an AntennaPickerMenu with the live slice, AlexController, and
    // BoardCapabilities. Without this set, the antenna submenu falls back to a
    // stub ANT1/ANT2/ANT3 list (no chain-consequence hints). MainWindow calls
    // this in wireSliceToSpectrum (Slice A) and in the sliceAdded handler
    // (Slice B+).
    void setRadioModel(NereusSDR::RadioModel* model);

signals:
    void frequencyChanged(double hz);
    void modeChanged(NereusSDR::DSPMode mode);
    void filterChanged(int low, int high);
    // Shift+click on a filter preset — TX passband should snap to match
    // the RX preset's audio Hz range.  MainWindow wires this to
    // TransmitModel::setFilterLow/setFilterHigh.
    void txFilterMatchRequested(int audioLow, int audioHigh);
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
    void openNrSetupForSliceRequested(NereusSDR::NrSlot slot, int sliceId);
    void openNnrModelsRequested(int sliceId);

    // --- Setup dialog request (e.g. AGC-T right-click → open settings) ---
    void openSetupRequested();

    // Phase 3F Sub-Epic C Task 9: emitted when operator clicks the TX badge
    // on this slice's flag. MainWindow forwards to
    // RadioModel::txSliceArbiter()->requestHandoff(sliceIndex), which drops
    // MOX (RF-safe) before flipping the TX-bound slice.
    void txHandoffRequested(int sliceIndex);
    // TX badge take (JJ, 2026-09-30): the badge was clicked while it offers
    // a take (TxBadgeOffer). MainWindow takes the slice, then transmit, as
    // needed, and makes the slice the TX slice. Nothing keys.
    void txTakeRequested(int sliceIndex);

    // Emitted by setActiveSlice() when the state actually changes. The
    // hosting SpectrumWidget listens (addVfoWidget) so its cached marker
    // overlay is redrawn in the new colours.
    void activeSliceChanged(bool active);

    // Phase 3F Sub-Epic E Task 4: right-click context menu intent signals.
    // MainWindow listens and routes to SliceModel / FilterPolicyDialog /
    // RadioModel::removeSlice. antennaChangeRequested is fired by the
    // AntennaPickerMenu (Task 5) integration shipped in Phase 3F closeout.
    void sampleRateRequested(int sliceIndex, int hz);
    void filterPolicyRequested(int chainIndex);
    void removeSliceRequested(int sliceIndex);
    void antennaChangeRequested(int sliceIndex, const QString& antennaName);
    // R-R3-21: the right-click Diversity entry. MainWindow opens the
    // Diversity dialog, the same one Tools > Diversity opens.
    void diversityRequested();

    // Task 14a: the flag's access actions. MainWindow sends each as the
    // matching slice request and shows the Core's answer on the flag.
    void takeControlRequested(int sliceIndex);
    void releaseRequested(int sliceIndex);
    void stopListeningRequested(int sliceIndex);
    // Task 14b: "Your volume" or Mute moved on a listened flag. level is
    // 0..100; MainWindow sends it as this device's listening level.
    void listenVolumeRequested(int sliceIndex, int level, bool muted);

private slots:
    // Phase 3F Sub-Epic C Task 9: TX badge click slot. Emits
    // txHandoffRequested with the slice this flag represents.
    void onTxBadgeClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    // Phase 3F Sub-Epic E Task 4: right-click pops the multi-pan context
    // menu (Make TX, Antenna >, Sample rate >, Diversity >, Filter policy,
    // Remove slice).
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void buildUI();
    void buildHeaderRow();
    void buildFrequencyRow();
    void buildSmeterRow();
    void buildSnrRow();        // Phase 3R L1 — RADE SNR display
    void updateSnrVisibility();  // Phase 3R L1 — show/hide based on mode
    void updateModeTabAccent();  // Phase 3R L3 — RADE purple chip accent
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
    void onNrSelectionRefused(const QString& reason);
    void showNr1Popup(const QPoint& globalPos);
    void showNr2Popup(const QPoint& globalPos);
    void showNr3Popup(const QPoint& globalPos);
    void showNr4Popup(const QPoint& globalPos);
    void showNnrPopup(const QPoint& globalPos);
    // R-R3-40: the small indicator on the NNR button while the Core holds
    // the receiver below the saved NNR choice.
    void onNnrLimitChanged(int limit);
    void requestNrSetup(NereusSDR::NrSlot slot);
    void showDfnrPopup(const QPoint& globalPos);
    // R-R3-49, Sub-epic C-1: why a noise filter cannot run (the model's
    // word, the Core's in a remote window, or this build's with no model),
    // empty when it can; and applying that to the DSP grid's DFNR and MNR
    // buttons: shown always, disabled with the reason while it cannot.
    QString nrCannotRunReason(NereusSDR::NrSlot slot) const;
    void updateNrAvailability();
    void showMnrPopup(const QPoint& globalPos);

    // Guard to prevent signal re-emission during model updates
    bool m_updatingFromModel{false};

    // Stage C2: optional user-override preset store.
    // Non-owning; lifetime managed by RadioModel.  Null until MainWindow wires it.
    class FilterPresetStore* m_filterPresetStore{nullptr};

    // Phase 3F closeout — non-owning RadioModel pointer used by contextMenuEvent
    // to build a live AntennaPickerMenu. Null until MainWindow calls
    // setRadioModel(); the contextMenuEvent falls back to a stub antenna submenu
    // when null. See setRadioModel() above for the wiring contract.
    NereusSDR::RadioModel* m_radioModel{nullptr};
    // R-R3-49: the model's DFNR and MNR availability signals.
    std::array<QMetaObject::Connection, 3> m_nrAvailabilityConns;

    // Internal helper — update m_locked + drive Close-strip lock button + emit lockChanged.
    // Called by the floating m_lockBtn toggled lambda.  X/RIT-tab Lock removed (B7).
    // Must be called outside m_updatingFromModel.
    void applyLockedState(bool on);

    // Slice identity
    int m_sliceIndex{0};
    bool m_activeSlice{true};  // see setActiveSlice()
    int m_stepHz{100};
    double m_frequency{14225000.0};
    // Signed Hz offsets from m_frequency. Seeded with the same LSB defaults
    // SpectrumWidget's pan-level m_filterLowHz/m_filterHighHz carry (Thetis
    // LSB default), so a flag that has not yet been handed a filter draws the
    // identical band the pan drew for it before the per-slice split.
    int m_filterLowHz{-2850};
    int m_filterHighHz{-150};
    DSPMode m_currentMode{DSPMode::USB};
    double m_smeterDbm{-127.0};

    // Fixed width from AetherSDR VfoWidget
    static constexpr int kWidgetW = 252;

    bool m_hasAlex{true};   // default true until caps land; preserves
                            // existing behavior during discovery.
    bool m_hasRxBypassRelay{false};    // Phase 3P-I-b T9 — BYPS button gate (caps)
    bool m_hasRxOutOnTxUi{false};      // Phase 3P-I-b T9 — BYPS button gate (SKU)
    bool m_smallFilterMode{false};     // Task 3.4 — small filter display
    bool m_transmitPermitted{true};
    // Group B fix wave: BYPS follows whether the Core takes RX bypass on TX.
    bool m_rxBypassPermitted{true};
    QString m_transmitPermissionReason;
    void updateTransmitControlAvailability();

    // B3: stored caps + SKU profile for AntennaPopupBuilder in popup lambdas.
    std::optional<BoardCapabilities> m_popupCaps;
    std::optional<SkuUiProfile>      m_popupSku;

    // --- Header row ---
    QPushButton* m_rxAntBtn{nullptr};
    QPushButton* m_rxBypassBtn{nullptr};  // Phase 3P-I-b T9 — grey BYPS toggle
    QPushButton* m_txAntBtn{nullptr};
    QLabel*      m_filterWidthLbl{nullptr};
    QPushButton* m_txBadge{nullptr};
    bool m_inUseByRadio{false};
    // TX badge take: the TX mark setTxSlice last showed, and the offer.
    bool m_txMarked{false};
    TxBadgeOffer m_txBadgeOffer;
    bool txBadgeTakeOffered() const;
    QLabel*      m_splitBadge{nullptr};
    QLabel*      m_sliceBadge{nullptr};
    QStringList  m_antennaList{QStringLiteral("ANT1"), QStringLiteral("ANT2"), QStringLiteral("ANT3")};

    // --- Frequency row ---
    QStackedWidget* m_freqStack{nullptr};
    QLabel*         m_freqLabel{nullptr};
    QLineEdit*      m_freqEdit{nullptr};

    // --- S-meter row ---
    VfoLevelBar* m_levelBar{nullptr};

    // --- SNR row (Phase 3R L1) ---
    // Surfaces SliceModel::snrDb (set by RadeChannel via I5 routing).
    // Two labels: "SNR" (static text) + value ("+N dB" / " -   - ").
    // Row visibility tracks m_currentMode being either RADE sideband
    // (DSPMode::RADE_U or DSPMode::RADE_L).
    QLabel* m_snrLabel{nullptr};
    QLabel* m_snrValue{nullptr};
    QWidget* m_snrRow{nullptr};  // parent row for show/hide
    bool     m_radeActive{false};

    // 2026-05-11 bench: cached EOO-decoded speaker callsign + last SNR
    // + last sync state so setRadeCallsign / setRadeSnrLabel /
    // setRadeSynced can each repaint the SNR row without losing the
    // other two pieces.  Empty m_lastRadeCallsign = unknown speaker;
    // NaN m_lastRadeSnr = no SNR snapshot yet.
    QString m_lastRadeCallsign;
    float   m_lastRadeSnrDb{std::numeric_limits<float>::quiet_NaN()};
    bool    m_lastRadeSynced{false};
    // The decoder's last frequency offset, re-appended to each fresh SNR
    // text. A local decoder sends it on every tick right after the SNR; a
    // remote window's Core sends it only when it moves, so without this an
    // SNR change alone would drop it from the flag. NaN = none yet.
    float   m_lastRadeFreqOffsetHz{std::numeric_limits<float>::quiet_NaN()};
    // RADE reason: why the slice's RADE decoder is not working (empty when
    // it is), from SliceModel::radeReasonChanged.
    QString m_radeReason;

    // Slot wired to SliceModel::snrDbChanged. Updates m_snrValue text
    // + stylesheet color (grey/yellow/green) based on NaN-state and the
    // 5 dB threshold.
    void onSnrChanged(double db);

    // From AetherSDR VfoWidget.cpp:3406-3445 [@0cd4559] — RADE status
    // label setters. The single m_snrLabel combines active-state
    // (visibility), sync indicator (filled/hollow circle), SNR value
    // (color-coded), and freq offset (appended).
public:
    void setRadeActive(bool on);
    void setRadeSynced(bool synced);
    void setRadeSnrLabel(float snrDb);
    void setRadeFreqOffset(float hz);

    // 2026-05-11 bench: cache + render the EOO-decoded speaker callsign
    // in the RADE status row.  When non-empty, the prefix "RADE" is
    // replaced with the callsign so the user sees who they are
    // copying (e.g. "KK7GWY ● 12dB").  Empty string clears the cache
    // and the row falls back to "RADE ● 12dB".  Sourced from
    // SliceModel::lastRadeRxCallsignChanged.  All three setters above
    // (active/synced/snr) re-render through repaintRadeRow() so the
    // callsign survives subsequent SNR pushes.
    void setRadeCallsign(const QString& callsign);

    // RADE reason (2026-09-30): why the slice is in RADE with no working
    // decoder (SliceModel::radeReason, the Core's on a remote window). While
    // it is set the row reads "<prefix> ○ off" and its tooltip is the
    // reason; empty puts the row back to its sync and SNR text.
    void setRadeReason(const QString& reason);
    QString radeRowTextForTest() const;
    QString radeRowToolTipForTest() const;

    // Slice color table: A=cyan, B=magenta, C=green, D=yellow, E=orange,
    // F=teal, G=coral, H=lavender, indexed by slice index % 8.
    // From AetherSDR SliceColors.h. Public static so the RX applet's
    // per-slice tab row (Phase 3F Bug 3) shares the exact flag palette.
    static QColor sliceColor(int index);

    // From AetherSDR src/gui/SliceColors.h:26 [@0cd4559]: the palette holds
    // eight colours, one per slice A to H.
    static constexpr int kSliceColorCount = 8;

    // The darker partner of each sliceColor() entry, used for the panadapter
    // markers of slices the operator has not selected (see setActiveSlice).
    static QColor sliceDimColor(int index);

private:

    // --- Tab bar ---
    QList<QPushButton*> m_tabButtons;
    QStackedWidget*     m_tabStack{nullptr};
    int                 m_activeTab{0};
    bool                m_stationPresentationAllowed{true};

    // --- Mode tab ---
    QComboBox*          m_modeCmb{nullptr};
    QWidget*            m_filterBtnContainer{nullptr};

    // --- Audio tab ---
    QWidget*            m_audioPage{nullptr};
    QLabel*             m_afNameLabel{nullptr};
    QSlider*            m_afGainSlider{nullptr};
    QLabel*             m_afGainLabel{nullptr};
    // Task 14b: the slice's AF and mute (from the model) and this device's
    // own listening volume and mute; the slider and Mute show one pair.
    int                 m_modelAfGain{50};
    bool                m_modelMuted{false};
    int                 m_listenVolume{50};
    bool                m_listenMuted{false};
    QString             m_afToolTip;
    QString             m_muteToolTip;
    QPushButton*        m_agcBtns[5]{};          // Off/Long/Slow/Med/Fast — replaces m_agcCmb
    QSlider*            m_panSlider{nullptr};
    QLabel*             m_panLabel{nullptr};
    QPushButton*        m_muteBtn{nullptr};
    QPushButton*        m_binBtn{nullptr};
    // R-R3-45: exclusive Speakers / Headphones pair + the silent notice.
    QPushButton*        m_speakersBtn{nullptr};
    QPushButton*        m_headphonesBtn{nullptr};
    QLabel*             m_outputNotice{nullptr};
    bool                m_headphonesAvailable{false};
    QString             m_headphonesProblem;  // R-R3-45
    bool                m_headphonesEnabled{false};  // R-R3-45 fix wave
    void updateOutputNotice();
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
    QPushButton* m_mnrBtn  = nullptr;
    QPushButton* m_nnrBtn  = nullptr;
    QLabel*      m_nnrLimitIndicator = nullptr;   // R-R3-40
    QString      m_nnrToolTip;
    QString      m_dfnrToolTip;   // R-R3-49: each filter's own tooltip
    QString      m_mnrToolTip;
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
    // Fix wave I3: the NR button last clicked, where a refusal is shown.
    QPointer<QPushButton> m_lastNrButton;
    QString m_nrRefusal;
    // Follow-up item 3: set only while this flag's own NR click is being
    // applied, so a refusal of a choice made elsewhere (the DSP menu) is
    // not shown here as well.
    bool m_nrClickInFlight{false};

    // --- X/RIT tab ---
    QPushButton*   m_ritBtn{nullptr};
    ScrollableLabel* m_ritLabel{nullptr};
    QPushButton*   m_ritZeroBtn{nullptr};
    QPushButton*   m_xitBtn{nullptr};
    ScrollableLabel* m_xitLabel{nullptr};
    QPushButton*   m_xitZeroBtn{nullptr};
    QPushButton*   m_stepCycleBtn{nullptr};

    // --- Floating control buttons (AetherSDR pattern) ---
    // Rendered as children of parent widget, positioned beside the VFO flag.
    QPushButton* m_closeBtn{nullptr};
    QPushButton* m_lockBtn{nullptr};
    QPushButton* m_recBtn{nullptr};
    QPushButton* m_playBtn{nullptr};
    bool m_locked{false};
    bool m_onLeft{false};  // track flag side for button placement

    // --- Task 14a: slice access ---
    QLabel*     m_accessLine{nullptr};
    SliceAccess m_sliceAccess;
    QString     m_accessPending;
    void applySliceAccess();
    void applyAudioBinding();
    QList<QWidget*> listeningHeldControls() const;
    void holdForListening(QWidget* control) const;
    void buildFloatingButtons();
    void positionFloatingButtons();
};

} // namespace NereusSDR
