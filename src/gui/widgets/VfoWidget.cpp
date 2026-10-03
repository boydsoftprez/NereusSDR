// =================================================================
// src/gui/widgets/VfoWidget.cpp  (NereusSDR)
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
//   2026-09-30 - VFO flag crash lane: Slice A's close-button comment no
//                 longer names the removed m_vfoWidget. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-30 - RADE reason: the RADE row reads "off" with the slice's
//                 radeReason as its tooltip while its RADE decoder is not
//                 working (setRadeReason). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-10-01: Added approved compact STEP units during PR review by
//                 J.J. Boyd (KG4VCF), with AI assistance via OpenAI Codex.
//   2026-09-23 - R-R3-21: the VAX tab's channel selector is disabled with a
//                 plain reason on a remote-station model. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-40: a small indicator on the NNR button, with the
//                 step-back reason as its tooltip, while the Core holds the
//                 receiver below the saved NNR choice. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//                 Later the same day: its colour from StyleConstants.
//   2026-09-23 - R-R3-44: the VAX tab's channel selector works in a remote
//                 window again: it picks this computer's VAX channel for the
//                 Core's slice (kept on this computer, not the Core).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-45: Speakers and Headphones buttons in the audio
//                 block pick the receiver's output (VAX design 6.2); a
//                 plain notice says why it is silent when the headphones
//                 are chosen and none are set up. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the floating record and play buttons are hidden
//                 (UnbuiltFeatures) until the voice recorder is built.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the FM box shows only once one of its features
//                 is built (plan row fm-flag). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-45 fix wave: headphones turned on that did not open
//                 say so (setHeadphonesEnabled); a remote window's reason
//                 comes first. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-45 Task 2: setHeadphonesProblem(), a remote window's
//                 reason a receiver on the headphones is silent. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-45: the output buttons read SPEAKERS and PHONES, in
//                 capitals like the flag's other buttons (operator's
//                 captions). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: the DFNR button is hidden, and its
//                 quick controls not offered, while DFNR cannot run (a build
//                 without it, or the Core's dfnrRunnable false), as MNR and
//                 BNR are hidden. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1 (tx-followup-3): DFNR, MNR and BNR
//                 are never hidden. One that cannot run is shown disabled
//                 with the plain reason (the Core's, in a remote window) and
//                 opens no quick controls; BNR takes the row-2 cell beside
//                 SNB. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1 (tx-followup-4): the BNR button and
//                 its quick controls are gone (operator: not offered for
//                 now); row 2 keeps ANF and SNB. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 19 (R-IOS-06): the AGC labels, the AGC-T
//                 range and the slice colours come from ControlRanges.h,
//                 which the Core's catalogue reads too. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app follow-up (R-IOS-06): the AF and SQL slider
//                 ranges come from ControlRanges.h too. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, R-R3-21 (parity Task 11): the XIT button, offset
//                 and zero write the slice in a remote window as in a local
//                 one; they no longer follow the transmit permission.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 / R-R3-21 (parity Task 16): DFNR and MNR follow
//                 the station's noise reduction (the Core's in a remote
//                 window, RadioModel::noiseReductionUnavailableReason): shown
//                 always, disabled with the plain reason while they cannot
//                 run, with no quick controls. MNR is no longer hidden off a
//                 Mac. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-26 - R-R3-49 (trunk merge of parity Tasks 16 to 18): one
//                 availability path. Task 16's noiseReductionMethods and
//                 nrUnavailableReason are dropped for the trunk's
//                 nrCannotRunReason (DspAssetService); the flag also follows
//                 RadioModel::nrAvailabilityChanged, so an older Core that
//                 does not say shows DFNR and MNR disabled with the reason.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 : iPhone app plan Task 78 (R-IOS-02, R-IOS-30):
//                 setInUseByRadio (ruling 8.11). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-27 : NR1's quick controls read ControlRanges.h, their ranges
//                 and defaults corrected to Thetis's NR spinboxes (taps
//                 1-1024, delay 1-1023, gain and leak 1-1000, defaults
//                 64 / 16 / 100 / 100; R-IOS-06, R-IOS-27). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 : MNR's quick controls read ControlRanges.h; Reset restores
//                 a new slice's values (Aggressiveness 4, Bias 1.2, where it
//                 gave 6 and 1.5). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-27 : NR2, NR3, NR4 and DFNR's quick controls read ControlRanges.h
//                 too, the table the Core's catalogue sends (R-IOS-06,
//                 R-IOS-27); their values are unchanged. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The RADE row keeps the decoder's last frequency offset and
//                 re-appends it to each fresh SNR, since a remote window's
//                 Core sends the offset only when it moves. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Slice control plan Task 14b (ruling U5): on a listened
//                 flag the AF slider and Mute return as this device's own
//                 volume and mute ("Your volume"), never the slice's AF.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - core-slice take-over: the flag's Take control is
//                 disabled with the Core's words when it refuses the take.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX badge take (JJ's ruling): while the badge offers a
//                 take (TxBadgeOffer) it is enabled, says what a click will
//                 do, and a click emits txTakeRequested. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "VfoWidget.h"
#include "gui/TuneStepLabel.h"
#include "DspParamPopup.h"
#include "NnrControls.h"
#include "VaxChannelSelector.h"
#include "gui/AntennaPopupBuilder.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/OperatorReasonText.h"
#include "core/BoardCapabilities.h"
#include "core/ControlRanges.h"
#include "core/SkuUiProfile.h"
#include "core/HpsdrModel.h"
#include "core/accessories/AlexController.h"
#include "gui/StyleConstants.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/widgets/AntennaPickerMenu.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "core/dsp/DspAssetService.h"
#include "models/SliceModel.h"
#include "gui/widgets/FilterPresetEditDialog.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenu>
#include <QVariant>
#include <QFontDatabase>
#include <QSignalBlocker>
#include <QToolTip>

#include <cmath>
#include <algorithm>

namespace NereusSDR {

namespace {
// The TX badge's own words while it is held or offers a take
// (updateTransmitControlAvailability, setInUseByRadio).
constexpr const char* kSavedTransmitTooltip = "VfoSavedTransmitTooltip";
constexpr const char* kSavedTransmitDescription = "VfoSavedTransmitDescription";

// BYPS's tooltip while it may be pressed (group B fix wave: kept in one
// place, since setRxBypassPermitted puts it back).
QString rxBypassToolTip()
{
    return QStringLiteral(
        "RX Bypass on TX: routes the receive path through the bypass relay "
        "while transmitting.");
}

// A noise-reduction slider's readout: slider / divide to its places, then
// its suffix (ControlRanges::NrControl).
QString nrReadout(const ControlRanges::NrControl& control, int position)
{
    return QString::number(double(position) / control.divide, 'f', control.decimals)
        + QString::fromUtf8(control.suffix);
}

// One noise-reduction slider in `popup`, drawn from its ControlRanges entry:
// positioned at `current` (property units), writing slider x scale, its
// Reset (when the entry has one) restoring the entry's reset position.
void addNrSlider(DspParamPopup* popup, const ControlRanges::NrControl& control,
                 double current, std::function<void(double)> write,
                 const QString& tooltip = QString())
{
    const ControlRanges::NrControl entry = control;
    popup->addSlider(QString::fromUtf8(entry.label), static_cast<int>(entry.min),
                     static_cast<int>(entry.max),
                     ControlRanges::nrSliderFromValue(entry, current),
                     [entry](int v) { return nrReadout(entry, v); },
                     [entry, write = std::move(write)](int v) {
                         write(ControlRanges::nrValueFromSlider(entry, v));
                     },
                     tooltip,
                     entry.hasReset ? static_cast<int>(std::lround(entry.reset)) : INT_MIN);
}

// A noise-reduction choice's labels, in its order.
QStringList nrOptionLabels(const ControlRanges::NrControl& control)
{
    QStringList labels;
    for (std::size_t i = 0; i < control.optionCount; ++i) {
        labels.append(QString::fromUtf8(control.options[i].label));
    }
    return labels;
}
} // namespace

// 2026-05-13 bench fix (PR #238): QStackedWidget subclass that reports
// the CURRENT page's sizeHint instead of the maximum across all pages.
//
// The flag wants to shrink to fit the active tab only (not the tallest
// tab — that's an explicit user directive from 2026-04-23).  The
// original implementation marked hidden pages with
// QSizePolicy::Ignored to hide them from the stack's size
// calculation.  Unfortunately Ignored short-circuits hint propagation
// for the page's CHILDREN too: when rebuildFilterButtons grew the
// filter-preset grid inside the active page, the container's sizeHint
// kept returning (0, 0) and the flag SHRANK on adjustSize instead of
// growing.  Diagnostic at 2026-05-13 confirmed the (0,0) read.
//
// Subclassing is the canonical Qt fix: sizeHint() / minimumSizeHint()
// return only the currently-shown widget's hint.  Hidden pages don't
// need the Ignored policy any more (they don't contribute to the
// stack hint at all), so the hint chain stays clean and content
// changes inside the active page propagate up normally.
class CurrentPageSizedStack : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;
    QSize sizeHint() const override {
        if (auto* w = currentWidget()) {
            return w->sizeHint();
        }
        return QStackedWidget::sizeHint();
    }
    QSize minimumSizeHint() const override {
        if (auto* w = currentWidget()) {
            return w->minimumSizeHint();
        }
        return QStackedWidget::minimumSizeHint();
    }
};

// File-local style helpers — §A2 exception: each diverges from canonical
// Style:: on font-size (13px vs 10px), border colour (#304050 vs #205070),
// or hover behaviour (border-change vs bg-change). Do NOT collapse into
// canonical without re-auditing those visual properties first.

// Transparent/borderless antenna-row buttons (11px, 1px 4px padding).
// Diverges from buttonBaseStyle(): transparent bg, no border, larger font.
// Used with colour override suffix: e.g. vfoFlatBtnStyle() + "QPushButton { color: … }"
static inline QString vfoFlatBtnStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent; border: none;"
        "  padding: 1px 4px; font-size: 11px; font-weight: bold;"
        "}"
    );
}

// Tab-row selector buttons (12px, underline indicator, muted-blue default).
// Diverges from buttonBaseStyle(): transparent bg, underline :checked indicator,
// different font-size and base colour.
static inline QString vfoTabBtnStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent; border: none;"
        "  color: #6888a0; font-size: 12px; font-weight: bold;"
        "  padding: 2px 6px;"
        "}"
        "QPushButton:checked {"
        "  color: %1;"
        "  border-bottom: 2px solid %1;"
        "}"
    ).arg(NereusSDR::Style::kAccent);
}

// DSP toggle buttons (13px, border-change hover, green :checked).
// From AetherSDR VfoWidget.cpp:158-162.
// Diverges from buttonBaseStyle()+dspToggleStyle(): 13px vs 10px font-size;
// unchecked border #304050 vs #205070; hover changes border not background;
// checked text #ffffff vs kDspToggleText (#80ff80).
static inline QString vfoDspToggleStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %6;"
        "  border-radius: 2px; color: %2;"
        "  font-size: 13px; font-weight: bold;"
        "  padding: 2px 4px; min-width: 32px;"
        "}"
        "QPushButton:checked {"
        "  background: %3; color: #ffffff;"
        "  border: 1px solid %4;"
        "}"
        "QPushButton:hover {"
        "  border: 1px solid %5;"
        "}"
    ).arg(NereusSDR::Style::kButtonBg,
          NereusSDR::Style::kTextPrimary,
          NereusSDR::Style::kDspToggleBg,
          NereusSDR::Style::kDspToggleBorder,
          NereusSDR::Style::kBlueBorder,
          NereusSDR::Style::kOverlayBorder);
}

// Mode/filter preset buttons (13px, border-change hover, blue :checked).
// From VfoStyles.h kModeBtn — blue-checked mode/filter button (AetherSDR pattern).
// Diverges from buttonBaseStyle()+blueCheckedStyle(): 13px vs 10px font-size;
// unchecked border #304050 vs #205070; hover changes border not background.
static inline QString vfoModeBtnStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %6; border-radius: 2px;"
        "  color: %2; font-size: 13px; font-weight: bold; padding: 3px;"
        "}"
        "QPushButton:checked {"
        "  background: %3; color: %4; border: 1px solid %5;"
        "}"
        "QPushButton:hover { border: 1px solid %5; }"
    ).arg(NereusSDR::Style::kButtonBg,
          NereusSDR::Style::kTextPrimary,
          NereusSDR::Style::kBlueBg,
          NereusSDR::Style::kBlueText,
          NereusSDR::Style::kBlueBorder,
          NereusSDR::Style::kOverlayBorder);
}

// ---- Construction ----

VfoWidget::VfoWidget(QWidget* parent)
    : QWidget(parent)
{
    // Width fixed at kWidgetW per original AetherSDR pattern. Height grows
    // to fit content (no fixed height). DSP tab's 4-col grid uses ~60 px
    // buttons so 4×60=240 + margins fit within the 252 px flag width.
    setFixedWidth(kWidgetW);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);

    buildUI();
}

// Floating buttons (m_closeBtn/m_lockBtn/m_recBtn/m_playBtn) are parented to
// parentWidget() (SpectrumWidget), i.e. they are SIBLINGS of VfoWidget in
// SpectrumWidget's children list. Qt's parent chain already owns them — do
// NOT explicitly delete here. Explicit deletes caused issue #113: Qt's
// QObjectPrivate::deleteChildren() walks SpectrumWidget's children in an
// order that freed a floating button before ~VfoWidget ran, leaving the
// button pointer dangling and SIGSEGV'ing the delete. Same fix as fd03d51;
// regressed by ff94942.
VfoWidget::~VfoWidget() = default;

// See the header for why this is a separate call rather than destructor work.
// Deleting the buttons while both this flag and its SpectrumWidget parent are
// still alive keeps the ordering ours, so it cannot reproduce issue #113.
void VfoWidget::destroyFloatingButtons()
{
    for (QPushButton** btn : {&m_closeBtn, &m_lockBtn, &m_recBtn, &m_playBtn}) {
        if (*btn) {
            delete *btn;
            *btn = nullptr;
        }
    }
}

void VfoWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    // From AetherSDR VfoWidget.cpp:237-238 — margins (6, 2, 6, 0)
    mainLayout->setContentsMargins(6, 2, 6, 0);
    mainLayout->setSpacing(2);

    buildHeaderRow();
    buildFrequencyRow();
    buildSmeterRow();
    buildSnrRow();       // Phase 3R L1 — hidden unless mode == RADE
    buildTabBar();

    // Tab content stacked widget — HIDDEN by default (compact flag).
    // From AetherSDR VfoWidget.cpp:545 — m_tabStack->hide().
    // CurrentPageSizedStack subclass shrinks the stack to the active
    // page's sizeHint (instead of the max-of-all default), so the
    // flag shrinks to fit the active tab.  See class comment above.
    m_tabStack = new CurrentPageSizedStack(this);
    buildAudioTab();
    buildDspTab();
    buildModeTab();

    buildXRitTab();

    // VAX tab — Phase 3O Sub-Phase 8 Task 8.2. Hosts VaxChannelSelector
    // (visible only when the VAX tab is active, same as every other mode tab).
    auto* vaxTabWidget = new QWidget;
    auto* vaxTabLayout = new QHBoxLayout(vaxTabWidget);
    vaxTabLayout->setContentsMargins(10, 4, 10, 4);
    vaxTabLayout->setSpacing(3);
    auto* vaxTabLbl = new QLabel(QStringLiteral("VAX"), vaxTabWidget);
    vaxTabLbl->setStyleSheet(QStringLiteral("color:#8090a0;font-size:10px;"));
    vaxTabLayout->addWidget(vaxTabLbl);
    m_vaxSelector = new VaxChannelSelector(vaxTabWidget);
    vaxTabLayout->addWidget(m_vaxSelector);
    vaxTabLayout->addStretch(1);
    m_tabStack->addWidget(vaxTabWidget);

    mainLayout->addWidget(m_tabStack);
    m_tabStack->hide();  // Hidden by default — click tab to expand
    m_activeTab = -1;    // No tab active initially

    // Per user directive 2026-04-23: flag height shrinks to fit the
    // ACTIVE tab only (not the tallest tab).  CurrentPageSizedStack
    // subclass (declared at the top of this file) overrides sizeHint
    // / minimumSizeHint to return only the current page's hints, so
    // we no longer need to fiddle with each page's sizePolicy
    // (Ignored on the prior implementation broke hint propagation
    // for content INSIDE the active page — see PR #238 v5 fix and
    // the class comment).  Pages keep their default Preferred/Preferred
    // policy and the stack's overridden hint takes care of the
    // active-page-only sizing.

    setLayout(mainLayout);
    adjustSize();

    // Floating buttons are children of our PARENT (SpectrumWidget)
    // so they render outside the VFO flag bounds. Deferred until
    // first updatePosition() when parentWidget() is available.
}

void VfoWidget::buildHeaderRow()
{
    auto* hdr = new QHBoxLayout;
    hdr->setSpacing(2);
    hdr->setContentsMargins(0, 0, 0, 0);

    // RX antenna button (blue)
    m_rxAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
    m_rxAntBtn->setObjectName(QStringLiteral("m_rxAntBtn"));
    m_rxAntBtn->setStyleSheet(vfoFlatBtnStyle() +
        QStringLiteral("QPushButton { color: #4488ff; }"));
    m_rxAntBtn->setFixedHeight(18);
    // From Thetis console.resx:8277 — chkRxAnt.ToolTip
    m_rxAntBtn->setToolTip(QStringLiteral("Toggles receive antenna between RX and TX antennas for RX1"));
    connect(m_rxAntBtn, &QPushButton::clicked, this, [this]() {
        // B3: AntennaPopupBuilder — capability-gated popup (Phase 3P-I-a T22).
        QMenu menu(this);
        const QString cur = m_rxAntBtn->text();
        if (m_popupCaps && m_popupSku) {
            AntennaPopupBuilder::populate(&menu, *m_popupCaps, *m_popupSku,
                AntennaPopupBuilder::Mode::RX, cur);
        } else {
            for (const QString& ant : m_antennaList) {
                QAction* act = menu.addAction(ant);
                act->setCheckable(true);
                act->setChecked(ant == cur);
            }
        }
        menu.setStyleSheet(QString::fromLatin1(kPopupMenu));   // Phase 3P-I-a T15 — issue #98
        QAction* sel = menu.exec(m_rxAntBtn->mapToGlobal(
            QPoint(0, m_rxAntBtn->height())));
        if (sel) {
            const QString text = sel->data().isValid() ? sel->data().toString()
                                                       : sel->text();
            m_rxAntBtn->setText(text);
            emit rxAntennaChanged(text);
        }
    });
    hdr->addWidget(m_rxAntBtn);

    // RX Bypass button (grey, BYPS) — Phase 3P-I-b T9.
    // Gated on caps.hasRxBypassRelay && SkuUiProfile.hasRxBypassUi.
    // Toggles AlexController::rxOutOnTx. From Thetis HPSDR/Alex.cs:61
    // "public static bool RxOutOnTx = false;" [v2.10.3.13 @501e3f5].
    m_rxBypassBtn = new QPushButton(QStringLiteral("BYPS"), this);
    m_rxBypassBtn->setObjectName(QStringLiteral("m_rxBypassBtn"));
    m_rxBypassBtn->setCheckable(true);
    m_rxBypassBtn->setStyleSheet(vfoFlatBtnStyle() +
        QStringLiteral("QPushButton { color: #888888; }"
                       "QPushButton:checked { color: #ffcc44; background: #2a2a1a; }"));
    m_rxBypassBtn->setFixedHeight(18);
    // Maps to Thetis chkRxOutOnTx (Alex.cs:61) [cite moved from the tooltip,
    // R-R3-17].
    m_rxBypassBtn->setToolTip(rxBypassToolTip());
    m_rxBypassBtn->setVisible(false);  // hidden until setBoardCapabilities + setHpsdrSku confirm gates
    connect(m_rxBypassBtn, &QPushButton::toggled, this, [this](bool on) {
        // Group B fix wave: a receive relay setting, not a key. A remote
        // window's BYPS follows whether its Core takes it
        // (setRxBypassPermitted), not the transmit permission.
        if (m_updatingFromModel || !m_rxBypassPermitted) { return; }
        emit rxBypassToggled(on);
    });
    hdr->addWidget(m_rxBypassBtn);

    // TX antenna button (red)
    m_txAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
    m_txAntBtn->setObjectName(QStringLiteral("m_txAntBtn"));
    m_txAntBtn->setStyleSheet(vfoFlatBtnStyle() +
        QStringLiteral("QPushButton { color: #ff4444; }"));
    m_txAntBtn->setFixedHeight(18);
    // NereusSDR native — no single Thetis TX-antenna tooltip (TX ant is configured
    // via Alex board setup in Setup dialog, not via a main-window toggle)
    m_txAntBtn->setToolTip(QStringLiteral("Select TX antenna"));
    connect(m_txAntBtn, &QPushButton::clicked, this, [this]() {
        // B3: AntennaPopupBuilder TX mode — only main ANT1-3 (Phase 3P-I-a T22).
        QMenu menu(this);
        const QString cur = m_txAntBtn->text();
        if (m_popupCaps && m_popupSku) {
            AntennaPopupBuilder::populate(&menu, *m_popupCaps, *m_popupSku,
                AntennaPopupBuilder::Mode::TX, cur);
        } else {
            for (const QString& ant : m_antennaList) {
                QAction* act = menu.addAction(ant);
                act->setCheckable(true);
                act->setChecked(ant == cur);
            }
        }
        menu.setStyleSheet(QString::fromLatin1(kPopupMenu));   // Phase 3P-I-a T15 — issue #98
        QAction* sel = menu.exec(m_txAntBtn->mapToGlobal(
            QPoint(0, m_txAntBtn->height())));
        if (sel) {
            const QString text = sel->data().isValid() ? sel->data().toString()
                                                       : sel->text();
            m_txAntBtn->setText(text);
            emit txAntennaChanged(text);
        }
    });
    hdr->addWidget(m_txAntBtn);

    // Filter width label (cyan)
    m_filterWidthLbl = new QLabel(QStringLiteral("2.9K"), this);
    m_filterWidthLbl->setStyleSheet(
        QStringLiteral("color: #00c8ff; font-size: 11px; font-weight: bold;"));
    m_filterWidthLbl->setFixedHeight(18);
    hdr->addWidget(m_filterWidthLbl);

    hdr->addStretch();

    // TX badge
    m_txBadge = new QPushButton(QStringLiteral("TX"), this);
    m_txBadge->setObjectName(QStringLiteral("VfoTxBadge"));
    m_txBadge->setFixedSize(28, 18);
    m_txBadge->setCheckable(true);
    m_txBadge->setStyleSheet(
        QStringLiteral("QPushButton { background: #1a2a3a; border: 1px solid #304050;"
                        "border-radius: 3px; color: #6888a0; font-size: 10px; font-weight: bold; }"
                        "QPushButton:checked { background: #6a3030; border-color: #ff4444; color: #ff8080; }"));
    // NereusSDR native — Thetis has no per-slice TX badge (it uses chkMOX for TX state)
    m_txBadge->setToolTip(QStringLiteral("Indicates this slice is the TX slice"));
    hdr->addWidget(m_txBadge);

    // Phase 3F Sub-Epic C Task 9: TX badge click requests handoff to this slice.
    // The QPushButton stays checkable so it visually echoes setTxSlice() updates
    // pushed back from TxSliceArbiter::txBoundSliceChanged.
    connect(m_txBadge, &QPushButton::clicked, this, &VfoWidget::onTxBadgeClicked);

    // Split badge — hidden in Stage 1; wired in Stage 2 when split semantics land
    m_splitBadge = new QLabel(QStringLiteral("SPLIT"), this);
    m_splitBadge->setFixedSize(36, 18);
    m_splitBadge->setAlignment(Qt::AlignCenter);
    m_splitBadge->setStyleSheet(
        QStringLiteral("background: #1a2a3a; border: 1px solid #304050;"
                        "border-radius: 3px; color: #6888a0; font-size: 10px; font-weight: bold;"));
    m_splitBadge->setVisible(false);
    hdr->addWidget(m_splitBadge);

    // Slice letter badge
    m_sliceBadge = new QLabel(QStringLiteral("A"), this);
    m_sliceBadge->setFixedSize(18, 18);
    m_sliceBadge->setAlignment(Qt::AlignCenter);
    m_sliceBadge->setStyleSheet(
        QStringLiteral("background: #0070c0; color: white; font-size: 11px;"
                        "font-weight: bold; border-radius: 3px;"));
    hdr->addWidget(m_sliceBadge);

    static_cast<QVBoxLayout*>(layout())->addLayout(hdr);

    // Task 14a: who controls this slice. Hidden while nobody else shares it.
    m_accessLine = new QLabel(this);
    m_accessLine->setObjectName(QStringLiteral("VfoAccessLine"));
    m_accessLine->setWordWrap(true);
    m_accessLine->setStyleSheet(
        QStringLiteral("color: #c8d8e8; font-size: 10px;"));
    m_accessLine->setVisible(false);
    static_cast<QVBoxLayout*>(layout())->addWidget(m_accessLine);
}

void VfoWidget::buildFrequencyRow()
{
    m_freqStack = new QStackedWidget(this);
    m_freqStack->setFixedHeight(30);

    // Display label
    m_freqLabel = new QLabel(this);
    m_freqLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_freqLabel->setStyleSheet(
        QStringLiteral("color: #00e5ff; font-size: 24px; font-weight: bold;"
                        "font-family: 'Consolas', 'Menlo', monospace;"
                        "background: transparent;"
                        "border: 1px solid rgba(255,255,255,50);"
                        "border-radius: 3px; padding: 0 4px;"));
    updateFreqLabel();
    m_freqStack->addWidget(m_freqLabel);

    // Edit field
    m_freqEdit = new QLineEdit(this);
    m_freqEdit->setAlignment(Qt::AlignRight);
    m_freqEdit->setStyleSheet(
        QStringLiteral("color: #00e5ff; font-size: 20px; font-weight: bold;"
                        "font-family: 'Consolas', 'Menlo', monospace;"
                        "background: #0a0a18; border: 1px solid #00b4d8;"
                        "border-radius: 3px; padding: 0 4px;"));
    connect(m_freqEdit, &QLineEdit::returnPressed, this, [this]() {
        const double hz = parseUserFrequency(m_freqEdit->text());
        if (hz > 0.0) {
            const double clamped = std::clamp(hz, 100000.0, 61440000.0);
            m_frequency = clamped;
            updateFreqLabel();
            emit frequencyChanged(clamped);
        }
        m_freqStack->setCurrentIndex(0);
    });
    connect(m_freqEdit, &QLineEdit::editingFinished, this, [this]() {
        m_freqStack->setCurrentIndex(0);
    });
    m_freqStack->addWidget(m_freqEdit);

    // Double-click on label → edit
    m_freqLabel->installEventFilter(this);

    static_cast<QVBoxLayout*>(layout())->addWidget(m_freqStack);
}

void VfoWidget::buildSmeterRow()
{
    auto* vboxLayout = static_cast<QVBoxLayout*>(layout());

    // 2026-05-12 bench fix: 4 px spacer between the frequency
    // border and the S-meter tick strip so the "S1 3 5 7 9 +20 +40"
    // tick labels don't visually collide with the cyan frequency-
    // display border.  The default 2 px QVBoxLayout::setSpacing()
    // applied in buildUI() left the tick labels appearing to sit
    // inside the freq box; the user reported "S NUMBERS ARE
    // OVERLAPPING WITH THE FREQUENCY READOUT AFTER ADDITION OF
    // THE RADE SNR" during bench testing of PR #238.  A localized
    // QSpacerItem keeps the rest of the row spacing at 2 px.
    vboxLayout->addSpacing(4);

    m_levelBar = new VfoLevelBar(this);
    m_levelBar->setValue(float(m_smeterDbm));  // seed with cached value (default -127)
    vboxLayout->addWidget(m_levelBar);
}

// Phase 3R K-bench (bench feedback) — verbatim port from AetherSDR
// src/gui/VfoWidget.cpp:553-560 + 3406-3445 [@0cd4559]. Single label
// pattern that combines RADE active state, sync indicator (filled vs
// empty circle), SNR value, and freq offset all on one strip line:
//
//   Active + synced:   "RADE [●] 12dB +500Hz"   (yellow/green dot per SNR)
//   Active + nosync:   "RADE [○] ---"           (grey dot, em-dash value)
//   Not active:        hidden, text cleared
//
// Hidden whenever the slice's mode is NOT RADE_U / RADE_L. setSlice()
// + dspModeChanged path calls setRadeActive(on) on every mode swap.
// I5's snrDbChanged + RadioModel's radeSyncChanged push values via
// setRadeSnr / setRadeSynced.
void VfoWidget::buildSnrRow()
{
    // From AetherSDR VfoWidget.cpp:553-560 [@0cd4559]. NereusSDR
    // divergence: lives on its own row in the vertical layout (rather
    // than inline with the freqRow as in AetherSDR) so the existing
    // m_levelBar S-meter row above stays at full width.
    m_snrRow = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(m_snrRow);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    // Single combined status label.  m_snrLabel is kept as the
    // strip-element pointer for test-seam compatibility (older
    // unit tests dereference it); m_snrValue is unused going forward
    // but kept for API stability.
    m_snrLabel = new QLabel(m_snrRow);
    m_snrLabel->setFixedHeight(16);
    m_snrLabel->setTextFormat(Qt::RichText);
    m_snrLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #00b4d8; font-size: 10px; font-weight: bold;"
        " background: transparent; border: none; padding: 0; margin: 0; }"));
    m_snrLabel->hide();

    // m_snrValue is now a no-op placeholder.  Older tests that
    // dereference it still get a non-null QLabel so they don't crash;
    // newer tests should target m_snrLabel directly.
    m_snrValue = new QLabel(m_snrRow);
    m_snrValue->hide();

    rowLayout->addWidget(m_snrLabel);
    rowLayout->addStretch(1);

    static_cast<QVBoxLayout*>(layout())->addWidget(m_snrRow);

    // Row container stays mounted (so the layout reserves its slot)
    // but the visible label is hidden until setRadeActive(true) fires.
    m_snrRow->setVisible(true);
}

// 2026-05-11 bench: unified RADE status row renderer.
// Combines the cached callsign (m_lastRadeCallsign), sync indicator
// (m_lastRadeSynced -> ●/○), and SNR (m_lastRadeSnrDb -> dB string +
// color) into the single m_snrLabel text.  The prefix is the
// callsign when known, falling back to the literal "RADE" otherwise.
// Layout per JJ bench design 2026-05-11 (layout X):
//   "<call> ● <snr>dB"    when synced AND SNR known
//   "<call> ● ---"        when synced AND no SNR snapshot
//   "<call> ○ ---"        when not synced (sticky callsign from last over)
//   "RADE ● <snr>dB"      when synced, SNR known, no callsign yet
//   "RADE ○ ---"          initial / not-yet-synced fallback
// Color rules from AetherSDR VfoWidget.cpp:3424-3432 [@0cd4559]:
//   < 5 dB  -> #e0e040 (yellow, marginal copy)
//   >= 5 dB -> #00ff88 (green, solid copy)
//   hollow / no SNR -> #505050 (grey)
static QString radePrefixForCallsign(const QString& callsign)
{
    return callsign.isEmpty() ? QStringLiteral("RADE") : callsign;
}

void VfoWidget::setRadeActive(bool on)
{
    m_radeActive = on;
    if (m_snrLabel) {
        m_snrLabel->setVisible(on);
        if (!on) {
            m_snrLabel->setText(QString());
            // Drop the cached callsign + SNR + sync on deactivate so the
            // next RADE engage starts from a clean "RADE ○ ---" state.
            m_lastRadeCallsign.clear();
            m_lastRadeSnrDb = std::numeric_limits<float>::quiet_NaN();
            m_lastRadeSynced = false;
            m_lastRadeFreqOffsetHz = std::numeric_limits<float>::quiet_NaN();
            return;
        }
    }
    // Activate path -- composed render below picks up the (empty)
    // callsign + (NaN) SNR + (false) sync caches and paints the
    // initial "RADE ○ ---" hollow-circle state.
    if (m_snrLabel) {
        const QString prefix = radePrefixForCallsign(m_lastRadeCallsign);
        // RADE reason: a slice whose decoder is not working reads "off".
        m_snrLabel->setText(
            QString("%1 <font color='#505050'>○</font> %2")
                .arg(prefix, m_radeReason.isEmpty() ? QStringLiteral("---")
                                                    : QStringLiteral("off")));
    }
}

// From AetherSDR VfoWidget.cpp:3415-3422 [@0cd4559] — setRadeSynced.
// 2026-05-11 bench: cache the sync state and re-render so a subsequent
// setRadeCallsign / setRadeSnrLabel call composes on top of the
// correct circle glyph.
void VfoWidget::setRadeSynced(bool synced)
{
    m_lastRadeSynced = synced;
    if (!m_radeActive || !m_snrLabel) {
        return;
    }
    if (!m_radeReason.isEmpty()) {
        // RADE reason: no working decoder, so no sync or SNR to show.
        m_snrLabel->setText(
            QString("%1 <font color='#505050'>○</font> off")
                .arg(radePrefixForCallsign(m_lastRadeCallsign)));
        return;
    }
    if (!synced) {
        // Unlock invalidates the decoder's last SNR snapshot. Painting that
        // stale value would also call setRadeSnrLabel(), which treats every
        // fresh SNR callback as proof of sync and would immediately undo this
        // transition.
        m_lastRadeSnrDb = std::numeric_limits<float>::quiet_NaN();
    }
    // If we don't have an SNR snapshot yet, paint the "<prefix> ●/○ ---"
    // state.  When SNR is known, setRadeSnrLabel below has the richer
    // colorized render path.
    if (std::isnan(m_lastRadeSnrDb)) {
        const QString prefix = radePrefixForCallsign(m_lastRadeCallsign);
        const QString color = synced ? QStringLiteral("#00ff88")
                                     : QStringLiteral("#505050");
        const QString glyph = synced ? QStringLiteral("●")
                                     : QStringLiteral("○");
        m_snrLabel->setText(
            QString("%1 <font color='%2'>%3</font> ---")
                .arg(prefix, color, glyph));
    } else {
        // Re-render through the SNR path with the cached value to pick
        // up the new sync glyph.
        setRadeSnrLabel(m_lastRadeSnrDb);
    }
}

// From AetherSDR VfoWidget.cpp:3424-3432 [@0cd4559] — setRadeSnr.
// 2026-05-11 bench: prefix is the cached callsign when known, falling
// back to "RADE".  Sync glyph from cached m_lastRadeSynced; the
// AetherSDR default of treating any setRadeSnrLabel call as "synced"
// is preserved by setting m_lastRadeSynced=true here (a fresh SNR
// snapshot from the RADE decoder implies sync).
void VfoWidget::setRadeSnrLabel(float snrDb)
{
    if (std::isnan(snrDb)) {
        return;
    }
    m_lastRadeSnrDb = snrDb;
    // A fresh SNR snapshot implies the decoder is producing samples,
    // which the AetherSDR setter at line 3424 implicitly treats as
    // "synced" by always painting the filled ● glyph.  Pin that here
    // so a setRadeSnrLabel call after a stale setRadeSynced(false)
    // still shows ●.  setRadeSynced(false) called LATER will overwrite
    // m_lastRadeSynced back to false.
    m_lastRadeSynced = true;
    if (!m_radeActive || !m_snrLabel) {
        return;
    }
    const QString prefix = radePrefixForCallsign(m_lastRadeCallsign);
    const QString color = (snrDb < 5.0f) ? QStringLiteral("#e0e040")
                                         : QStringLiteral("#00ff88");
    m_snrLabel->setText(
        QString("%1 <font color='%2'>●</font> %3dB")
            .arg(prefix, color)
            .arg(static_cast<int>(snrDb)));
    if (!std::isnan(m_lastRadeFreqOffsetHz)) {
        setRadeFreqOffset(m_lastRadeFreqOffsetHz);
    }
}

// From AetherSDR VfoWidget.cpp:3434-3445 [@0cd4559] — setRadeFreqOffset.
// Appends "+<Hz>Hz" or "-<Hz>Hz" to the existing label text.  Caller
// pattern: setRadeSnr() runs first (sets up "...dB" suffix), then
// setRadeFreqOffset() appends the offset.
void VfoWidget::setRadeFreqOffset(float hz)
{
    m_lastRadeFreqOffsetHz = hz;
    if (!m_radeActive || !m_snrLabel) {
        return;
    }
    QString current = m_snrLabel->text();
    int dbPos = current.indexOf(QStringLiteral("dB"));
    if (dbPos > 0) {
        QString base = current.left(dbPos + 2);
        QString sign = (hz >= 0) ? QStringLiteral("+") : QString();
        m_snrLabel->setText(
            QString("%1 %2%3Hz").arg(base, sign).arg(static_cast<int>(hz)));
    }
}

// 2026-05-11 bench: receive the EOO-decoded speaker callsign and
// rebuild the SNR row so the prefix shows the callsign instead of
// the literal "RADE".  See radePrefixForCallsign + the per-state
// repaint paths in setRadeSynced / setRadeSnrLabel for full
// composition semantics.  Empty callsign clears the cache (no-op
// for a never-set field) and falls back to the "RADE" prefix.
void VfoWidget::setRadeCallsign(const QString& callsign)
{
    if (m_lastRadeCallsign == callsign) {
        return;
    }
    m_lastRadeCallsign = callsign;
    if (!m_radeActive || !m_snrLabel) {
        return;
    }
    // Re-render through whichever path matches the current state.
    // setRadeSnrLabel covers the synced-with-SNR case; setRadeSynced
    // covers the sync-without-SNR case; the deactivate path is the
    // not-active case which we already early-returned out of.
    if (!std::isnan(m_lastRadeSnrDb)) {
        setRadeSnrLabel(m_lastRadeSnrDb);
    } else {
        setRadeSynced(m_lastRadeSynced);
    }
}

void VfoWidget::setRadeReason(const QString& reason)
{
    m_radeReason = reason;
    if (!m_snrLabel) {
        return;
    }
    // The existing reason pattern: the plain sentence is the tooltip.
    m_snrLabel->setToolTip(reason);
    m_snrLabel->setAccessibleDescription(reason);
    if (m_radeActive) {
        // Repaints "off", or the sync and SNR text when the reason clears.
        setRadeSynced(m_lastRadeSynced);
    }
}

QString VfoWidget::radeRowTextForTest() const
{
    return m_snrLabel ? m_snrLabel->text() : QString();
}

QString VfoWidget::radeRowToolTipForTest() const
{
    return m_snrLabel ? m_snrLabel->toolTip() : QString();
}

void VfoWidget::updateSnrVisibility()
{
    // AetherSDR-style: active state follows slice mode.  Setting
    // m_radeActive=false hides the label and clears stale text.
    const bool isRade = (m_currentMode == DSPMode::RADE_U
                         || m_currentMode == DSPMode::RADE_L);
    setRadeActive(isRade);
}

// Phase 3R L3 — paint the Mode tab button (m_tabButtons[2]) with the
// RADE purple accent (#a78bfa) when the active mode is either RADE
// sideband (RADE_U or RADE_L).  All other modes use the default
// vfoTabBtnStyle() (cyan accent kAccent).  The mode tab button is the
// user-visible "chip" for the current mode: its label is set to
// SliceModel::modeName(mode) in setMode().  When RADE is active the
// chip switches to purple as a visual cue that the signal chain has
// swapped from WDSP to the RADE neural codec.
void VfoWidget::updateModeTabAccent()
{
    if (m_tabButtons.size() <= 2 || !m_tabButtons[2]) {
        return;
    }
    const bool isRade = (m_currentMode == DSPMode::RADE_U
                         || m_currentMode == DSPMode::RADE_L);
    if (isRade) {
        // RADE accent style — same structure as vfoTabBtnStyle() but
        // with the cyan kAccent replaced by the purple #a78bfa.  Both
        // the unchecked text colour and the checked underline pick up
        // the purple so the chip remains visually distinct whether
        // the Mode tab page is expanded or collapsed.
        m_tabButtons[2]->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: transparent; border: none;"
            "  color: #a78bfa; font-size: 12px; font-weight: bold;"
            "  padding: 2px 6px;"
            "}"
            "QPushButton:checked {"
            "  color: #a78bfa;"
            "  border-bottom: 2px solid #a78bfa;"
            "}"));
    } else {
        // Restore the default tab style for any non-RADE mode.
        m_tabButtons[2]->setStyleSheet(vfoTabBtnStyle());
    }
}

void VfoWidget::onSnrChanged(double db)
{
    // Delegate to setRadeSnrLabel which renders the AetherSDR-style
    // combined "RADE ● Ndb" status string on m_snrLabel. NaN bypasses
    // the update; the label stays at its last-known state until
    // setRadeSynced(false) or setRadeActive(false) overrides it.
    if (qIsNaN(db)) {
        return;
    }
    setRadeSnrLabel(static_cast<float>(db));
}

void VfoWidget::buildTabBar()
{
    auto* tabLayout = new QHBoxLayout;
    tabLayout->setSpacing(0);
    tabLayout->setContentsMargins(0, 0, 0, 0);

    // Tab labels — from AetherSDR VfoWidget.cpp:522
    // [🔊] | DSP | USB | X/RIT | VAX
    QStringList tabLabels = {
        QString::fromUtf8("\xF0\x9F\x94\x8A"),  // 🔊 speaker
        QStringLiteral("DSP"),
        SliceModel::modeName(m_currentMode),
        QStringLiteral("X/RIT"),
        QStringLiteral("VAX")
    };

    // NereusSDR native — Thetis has a fixed single-panel layout with all controls
    // visible simultaneously. The tabbed sub-panel is a NereusSDR UX pattern.
    static const char* kTabTooltips[] = {
        "Show/hide audio controls (AF gain, AGC, pan, mute, squelch)",
        "Show/hide DSP controls (NB, NR, ANF, SNB, APF)",
        "Show/hide mode and filter controls",
        "Show/hide RIT/XIT and frequency-lock controls",
        "Show/hide VAX audio routing controls"
    };

    for (int i = 0; i < tabLabels.size(); ++i) {
        // Add separator before each tab except the first
        // From AetherSDR VfoWidget.cpp:523-530
        if (i > 0) {
            auto* sep = new QLabel(QStringLiteral("|"), this);
            sep->setStyleSheet(QStringLiteral(
                "QLabel { background: transparent; border: none;"
                "color: rgba(255,255,255,80); font-size: 13px; padding: 0; }"));
            sep->setFixedWidth(6);
            tabLayout->addWidget(sep);
        }

        auto* btn = new QPushButton(tabLabels[i], this);
        btn->setCheckable(true);
        btn->setStyleSheet(vfoTabBtnStyle());
        btn->setFixedHeight(24);  // 24px from AetherSDR
        btn->setToolTip(QString::fromLatin1(kTabTooltips[i]));
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            if (m_activeTab == i) {
                // Toggle: clicking active tab hides content
                m_tabStack->hide();
                m_activeTab = -1;
                for (auto* b : m_tabButtons) { b->setChecked(false); }
            } else {
                m_activeTab = i;
                // CurrentPageSizedStack (this file's QStackedWidget
                // subclass) returns only the active page's sizeHint,
                // so no per-page sizePolicy fiddling is needed —
                // setCurrentIndex alone is enough.  See PR #238 v5
                // fix and the subclass comment for why the prior
                // Ignored-policy approach was removed.
                m_tabStack->setCurrentIndex(i);
                m_tabStack->show();
                for (int j = 0; j < m_tabButtons.size(); ++j) {
                    m_tabButtons[j]->setChecked(j == i);
                }
            }
            adjustSize();
            // Notify parent to reposition
            if (parentWidget()) {
                parentWidget()->update();
            }
        });
        tabLayout->addWidget(btn, 1);  // stretch equally
        m_tabButtons.append(btn);
    }

    static_cast<QVBoxLayout*>(layout())->addLayout(tabLayout);
}

void VfoWidget::buildAudioTab()
{
    auto* audioWidget = new QWidget;
    m_audioPage = audioWidget;
    auto* audioLayout = new QVBoxLayout(audioWidget);
    audioLayout->setContentsMargins(4, 4, 4, 4);
    audioLayout->setSpacing(4);

    // 1. AF Gain slider (preserved exactly as-is — already live-wired)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("AF"), audioWidget);
        label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(NereusSDR::Style::kLabelMid));
        label->setFixedWidth(24);
        row->addWidget(label);
        m_afNameLabel = label;

        m_afGainSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_afGainSlider->setRange(ControlRanges::kAfGainMin, ControlRanges::kAfGainMax);
        m_afGainSlider->setSingleStep(ControlRanges::kAfGainStep);
        m_afGainSlider->setValue(50);
        m_afGainSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // From Thetis console.resx:8433 [v2.10.3.15]: ptbAF.ToolTip ("AF Gain -
        // Monitor Volume for RX/TX"). R-R3-21: reworded, since here the slider sets
        // only this slice's received audio (SliceModel::afGain ->
        // RxChannel::setAfGain -> WDSP SetRXAPanelGain1). Your own
        // transmitted audio is set by Mon Vol on the TX applet.
        m_afToolTip = QStringLiteral(
            "How loud you hear this slice's received audio. 0 to 100. "
            "Your own transmitted audio has its own slider, Mon Vol, on the TX applet.");
        m_afGainSlider->setToolTip(m_afToolTip);
        row->addWidget(m_afGainSlider);

        m_afGainLabel = new QLabel(QStringLiteral("50"), audioWidget);
        m_afGainLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_afGainLabel->setFixedWidth(24);
        m_afGainLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_afGainLabel);

        connect(m_afGainSlider, &QSlider::valueChanged, this, [this](int val) {
            m_afGainLabel->setText(QString::number(val));
            if (m_updatingFromModel) { return; }
            if (isListening()) {
                // Task 14b: this device's own volume, never the slice's AF.
                m_listenVolume = val;
                emit listenVolumeRequested(m_sliceIndex, val, m_listenMuted);
                return;
            }
            m_modelAfGain = val;
            emit afGainChanged(val);
        });
        audioLayout->addLayout(row);
    }

    // 2. AGC 5-button row — replaces m_agcCmb (live-wired, no NYI badge)
    {
        // The labels come from ControlRanges.h, which the Core's catalogue
        // reads too (iPhone app Task 19).
        static_assert(ControlRanges::kAgcModes.size() == 5,
                      "the flag's AGC row has five buttons");
        // From Thetis console.resx:4554 (comboAGC.ToolTip) + console.cs:27987-28041
        // Thetis sets dynamic tooltip per AGC mode change; we use static variants.
        static const char* kAgcTooltips[] = {
            "Automatic Gain Control Mode Setting:\nFixed - Set gain with AGC-T control",
            "Automatic Gain Control Mode Setting:\nLong (Attack 2ms, Hang 2000ms, Decay 2000ms)",
            "Automatic Gain Control Mode Setting:\nSlow (Attack 2ms, Hang 1000ms, Decay 500ms)",
            "Automatic Gain Control Mode Setting:\nMedium (Attack 2ms, Hang OFF, Decay 250ms)",
            "Automatic Gain Control Mode Setting:\nFast (Attack 2ms, Hang OFF, Decay 50ms)"
        };
        auto* row = new QHBoxLayout;
        row->setSpacing(2);
        row->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 5; ++i) {
            m_agcBtns[i] = new QPushButton(
                QString::fromLatin1(ControlRanges::kAgcModes[static_cast<std::size_t>(i)].label),
                audioWidget);
            m_agcBtns[i]->setCheckable(true);
            m_agcBtns[i]->setStyleSheet(vfoDspToggleStyle());
            m_agcBtns[i]->setToolTip(QString::fromLatin1(kAgcTooltips[i]));
            row->addWidget(m_agcBtns[i]);
        }
        // Default: Med (index 3) — matches AGCMode::Med
        m_agcBtns[3]->setChecked(true);

        // Exclusive toggle: clicking one un-checks the others, emits agcModeChanged
        for (int i = 0; i < 5; ++i) {
            connect(m_agcBtns[i], &QPushButton::clicked, this, [this, i](bool checked) {
                if (!checked) {
                    // Don't allow unchecking; keep it checked
                    m_agcBtns[i]->setChecked(true);
                    return;
                }
                // Uncheck siblings
                for (int j = 0; j < 5; ++j) {
                    if (j != i) {
                        m_agcBtns[j]->setChecked(false);
                    }
                }
                if (!m_updatingFromModel) {
                    emit agcModeChanged(static_cast<AGCMode>(i));
                }
            });
        }
        audioLayout->addLayout(row);
    }

    // 3. Audio pan slider row (NYI)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("Pan"), audioWidget);
        label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(NereusSDR::Style::kLabelMid));
        label->setFixedWidth(24);
        row->addWidget(label);

        m_panSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_panSlider->setRange(-100, 100);
        m_panSlider->setSingleStep(1);
        m_panSlider->setValue(0);
        m_panSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // From Thetis radio.cs:1388 pan_dsp [v2.10.3.15]; WDSP patchpanel.c:169
        // SetRXAPanelPan (cites moved from the tooltip, R-R3-17).
        m_panSlider->setToolTip(QStringLiteral("Audio pan: left/right stereo balance (−100 = full left, 0 = center, +100 = full right)"));
        row->addWidget(m_panSlider);

        m_panLabel = new QLabel(QStringLiteral("0"), audioWidget);
        m_panLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_panLabel->setFixedWidth(24);
        m_panLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_panLabel);

        connect(m_panSlider, &QSlider::valueChanged, this, [this](int val) {
            m_panLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit panChanged(val / 100.0);
            }
        });
        audioLayout->addLayout(row);
    }

    // 4. Mute + BIN row (NYI)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_muteBtn = new QPushButton(QStringLiteral("Mute"), audioWidget);
        m_muteBtn->setCheckable(true);
        m_muteBtn->setStyleSheet(vfoDspToggleStyle());
        // From Thetis dsp.cs:393 SetRXAPanelRun [v2.10.3.15]; WDSP
        // patchpanel.c:136 (cites moved from the tooltip, R-R3-17).
        m_muteToolTip = QStringLiteral("Mute the receive audio");
        m_muteBtn->setToolTip(m_muteToolTip);
        row->addWidget(m_muteBtn);

        m_binBtn = new QPushButton(QStringLiteral("BIN"), audioWidget);
        m_binBtn->setCheckable(true);
        m_binBtn->setStyleSheet(vfoDspToggleStyle());
        // From Thetis radio.cs:1147 bin_on_dsp [v2.10.3.15]; WDSP
        // patchpanel.c:197 SetRXAPanelBinaural (cites moved from the
        // tooltip, R-R3-17).
        m_binBtn->setToolTip(QStringLiteral("Binaural audio: I and Q play in separate ears, for a stereo image in headphones"));
        row->addWidget(m_binBtn);

        row->addStretch();

        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool on) {
            if (m_updatingFromModel) { return; }
            if (isListening()) {
                // Task 14b: mutes this slice on this device only.
                m_listenMuted = on;
                emit listenVolumeRequested(m_sliceIndex, m_listenVolume, on);
                return;
            }
            m_modelMuted = on;
            emit muteChanged(on);
        });
        connect(m_binBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit binauralChanged(on);
            }
        });
        audioLayout->addLayout(row);
    }

    // 4b. R-R3-45: Speakers / Headphones, exclusive (VAX design 6.2). The
    // receiver plays on one of them. NereusSDR-native; Thetis has no
    // per-receiver output choice.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_speakersBtn = new QPushButton(QStringLiteral("SPEAKERS"), audioWidget);
        m_speakersBtn->setObjectName(QStringLiteral("outputSpeakersButton"));
        m_speakersBtn->setCheckable(true);
        m_speakersBtn->setChecked(true);
        m_speakersBtn->setStyleSheet(vfoDspToggleStyle());
        m_speakersBtn->setToolTip(QStringLiteral("Play this receiver on the speakers"));
        row->addWidget(m_speakersBtn);

        m_headphonesBtn = new QPushButton(QStringLiteral("PHONES"), audioWidget);
        m_headphonesBtn->setObjectName(QStringLiteral("outputHeadphonesButton"));
        m_headphonesBtn->setCheckable(true);
        m_headphonesBtn->setStyleSheet(vfoDspToggleStyle());
        m_headphonesBtn->setToolTip(QStringLiteral("Play this receiver on the headphones"));
        row->addWidget(m_headphonesBtn);

        row->addStretch();

        // Exclusive like the AGC row: clicking the checked one keeps it.
        auto pick = [this](SliceModel::OutputRoute route) {
            setOutputRoute(route);
            if (!m_updatingFromModel && m_slice) {
                m_slice->setOutputRoute(route);
            }
        };
        connect(m_speakersBtn, &QPushButton::clicked, this, [pick](bool) {
            pick(SliceModel::OutputRoute::Speakers);
        });
        connect(m_headphonesBtn, &QPushButton::clicked, this, [pick](bool) {
            pick(SliceModel::OutputRoute::Headphones);
        });
        audioLayout->addLayout(row);

        m_outputNotice = new QLabel(headphonesMissingText(), audioWidget);
        m_outputNotice->setObjectName(QStringLiteral("outputRouteNotice"));
        m_outputNotice->setWordWrap(true);
        m_outputNotice->setStyleSheet(
            QStringLiteral("color: %1; font-size: 10px;").arg(NereusSDR::Style::kAmberText));
        m_outputNotice->setVisible(false);
        audioLayout->addWidget(m_outputNotice);
    }

    // 5. Squelch row — SQL toggle + SQL threshold slider (NYI)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_sqlBtn = new QPushButton(QStringLiteral("SQL"), audioWidget);
        m_sqlBtn->setCheckable(true);
        m_sqlBtn->setStyleSheet(vfoDspToggleStyle());
        m_sqlBtn->setFixedWidth(40);
        // From Thetis console.resx:5631 — chkSquelch.ToolTip
        m_sqlBtn->setToolTip(QStringLiteral("Squelch Enable"));
        row->addWidget(m_sqlBtn);

        m_sqlSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_sqlSlider->setRange(ControlRanges::kSsqlThreshMin, ControlRanges::kSsqlThreshMax);
        m_sqlSlider->setSingleStep(ControlRanges::kSsqlThreshStep);
        m_sqlSlider->setValue(0);
        m_sqlSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // NereusSDR native — Thetis ptbSquelch has no ToolTip entry in console.resx
        m_sqlSlider->setToolTip(QStringLiteral("Squelch threshold. SSB: 0–100 maps to 0.0–1.0 linear. AM: dB scale. FM: linear 0–1."));
        row->addWidget(m_sqlSlider);

        connect(m_sqlBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit squelchEnabledChanged(on);
            }
        });
        connect(m_sqlSlider, &QSlider::valueChanged, this, [this](int val) {
            if (!m_updatingFromModel) {
                emit squelchThreshChanged(val);
            }
        });
        audioLayout->addLayout(row);
        // Squelch button and slider are live-wired — no NYI badge
    }

    // 6. AGC threshold slider row
    // From Thetis Project Files/Source/Console/console.cs:46048-46049 [v2.10.3.15] — agc_thresh_point, range -160..+2
    // (MW0LGE_21k9d: values are already offset as part of Display)
    {
        m_agcTContainer = new QWidget(audioWidget);
        auto* containerLayout = new QVBoxLayout(m_agcTContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(1);

        // First row: AGC-T label + slider + dB value + AUTO badge
        auto* row = new QHBoxLayout;
        m_agcTLabelWidget = new QLabel(QStringLiteral("AGC-T"), m_agcTContainer);
        m_agcTLabelWidget->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(NereusSDR::Style::kLabelMid));
        m_agcTLabelWidget->setFixedWidth(40);
        row->addWidget(m_agcTLabelWidget);

        m_agcTSlider = new QSlider(Qt::Horizontal, m_agcTContainer);
        m_agcTSlider->setRange(ControlRanges::kAgcThresholdMinDb,
                               ControlRanges::kAgcThresholdMaxDb);
        m_agcTSlider->setSingleStep(ControlRanges::kAgcThresholdStepDb);
        m_agcTSlider->setValue(-20);
        m_agcTSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // From Thetis console.resx:8397 — ptbRF.ToolTip (ptbRF is the AGC-T slider)
        m_agcTSlider->setToolTip(QStringLiteral("AGC Max Gain - Operates similarly to traditional RF Gain. Right click AUTO based on noise floor."));
        row->addWidget(m_agcTSlider);

        m_agcTLabel = new QLabel(QStringLiteral("-20"), m_agcTContainer);
        m_agcTLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_agcTLabel->setFixedWidth(32);
        m_agcTLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_agcTLabel);

        m_agcAutoLabel = new QPushButton(QStringLiteral("AUTO"), m_agcTContainer);
        m_agcAutoLabel->setStyleSheet(
            QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"
                            "color: #556; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                            "QPushButton:hover { border-color: #adff2f; }"));
        m_agcAutoLabel->setFixedHeight(14);
        m_agcAutoLabel->setFixedWidth(30);
        m_agcAutoLabel->setCursor(Qt::PointingHandCursor);
        // From Thetis v2.10.3.13 setup.designer.cs:38679 — chkAutoAGCRX1.ToolTip
        m_agcAutoLabel->setToolTip(QStringLiteral("Automatically adjust AGC based on Noise Floor"));
        connect(m_agcAutoLabel, &QPushButton::clicked, this, [this]() {
            emit autoAgcToggled(!m_autoAgcActive);
        });
        row->addWidget(m_agcAutoLabel);

        containerLayout->addLayout(row);

        // Second row: info sub-line (hidden by default)
        m_agcInfoLabel = new QLabel(m_agcTContainer);
        m_agcInfoLabel->setStyleSheet(QStringLiteral("color: #33aa33; font-size: 7px; padding: 0 2px;"));
        m_agcInfoLabel->hide();
        containerLayout->addWidget(m_agcInfoLabel);

        connect(m_agcTSlider, &QSlider::valueChanged, this, [this](int val) {
            m_agcTLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit agcThreshChanged(val);
            }
        });

        // Right-click on AGC-T slider → directly open Setup dialog
        m_agcTSlider->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_agcTSlider, &QWidget::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            emit openSetupRequested();
        });

        audioLayout->addWidget(m_agcTContainer);
    }

    audioLayout->addStretch();
    m_tabStack->addWidget(audioWidget);
}

void VfoWidget::buildDspTab()
{
    auto* dspWidget = new QWidget;
    // Default sizing — mirrors AetherSDR VfoWidget.cpp:933 exactly.
    auto* dspLayout = new QVBoxLayout(dspWidget);
    dspLayout->setContentsMargins(2, 2, 2, 2);
    dspLayout->setSpacing(3);

    // Sub-epic C-1 USER-APPROVED layout: 3×4 DSP button grid.
    // NB first (preserves cycling), then NR mutual-exclusion group (NR1-4/DFNR/MNR),
    // then ANF + SNB as independent toggles on row 2. APF toggle moves to its
    // own slider row below the grid (consistent with its CW-only visibility gate).
    //
    //   Row 0: NB  | NR1  | NR2 | NR3
    //   Row 1: NR4 | DFNR | MNR | NNR
    //   Row 2: ANF | SNB  |     |
    //
    // Overrides earlier horizontal-bank design per user directive 2026-04-23.

    auto makeToggle = [dspWidget](const QString& label) -> QPushButton* {
        auto* btn = new QPushButton(label, dspWidget);
        btn->setCheckable(true);
        btn->setStyleSheet(vfoDspToggleStyle());
        return btn;
    };

    // QGridLayout added directly to dspLayout — matches AetherSDR
    // VfoWidget.cpp:938 (no subgrid widget wrapper). Grid fills the outer
    // VBoxLayout width, QPushButton default Expanding policy stretches
    // buttons to fill each cell.
    auto* dspGrid = new QGridLayout;
    dspGrid->setSpacing(3);

    // NB cycling button (row 0, col 0) — tri-state Off → NB → NB2 → Off.
    // Mirrors Thetis chkNB — label switches "NB"/"NB2"; checked = active.
    // From Thetis console.cs:43513-43560 [v2.10.3.13].
    // Upstream tags preserved: //MW0LGE (from cited console.cs:43545) [v2.10.3.15]
    m_nbButton = makeToggle(QStringLiteral("NB"));
    m_nbButton->setToolTip(tr(
        "Noise blanker: left-click cycles Off \u2192 NB \u2192 NB2 \u2192 Off,\n"
        "right-click opens its Setup page (NB/SNB).\n"
        "NB: time-domain impulse blanker, suited to\n"
        "      sporadic crashes (powerline / ignition).\n"
        "NB2: second-generation with hold/interpolate modes,\n"
        "      suited to denser impulse noise."));
    // Right-click → Setup page. Mirrors Thetis chkNB_MouseDown
    // (console.cs:44447 [v2.10.3.13]) which calls ShowSetupTab(NB_Tab).
    m_nbButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_nbButton, &QWidget::customContextMenuRequested,
            this, [this](const QPoint&) { emit openNbSetupRequested(); });
    dspGrid->addWidget(m_nbButton, 0, 0);

    // Row 0: NB (col 0) | NR1 | NR2 | NR3
    // All NR mutex group buttons share the same kDspToggle style as NB/ANF/SNB
    // so the entire 3×4 grid is visually uniform (Option A, user feedback 2026-04-23).
    m_nr1Btn = makeToggle(QStringLiteral("NR1"));
    m_nr2Btn = makeToggle(QStringLiteral("NR2"));
    m_nr3Btn = makeToggle(QStringLiteral("NR3"));
    m_nr1Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_nr2Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_nr3Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    // Tooltips — Sub-epic C-1.
    // From Thetis console.resx:3879 — chkNR.ToolTip (closest analogue for NR1)
    m_nr1Btn->setToolTip(QStringLiteral("NR1: Adaptive LMS noise reduction. Left-click activates, right-click adjusts knobs"));
    m_nr2Btn->setToolTip(QStringLiteral("NR2: EMNR (Enhanced Multiband Noise Reduction). Left-click activates, right-click adjusts knobs"));
    m_nr3Btn->setToolTip(QStringLiteral("NR3: RNNR (Recurrent Neural Net noise reduction). Left-click activates, right-click adjusts knobs"));
    // 4×2 layout (option B) — four cols consistently filled.
    //   Row 0: NB  | NR1  | NR2 | NR3
    //   Row 1: NR4 | DFNR | MNR | NNR
    //   Row 2: SNB (alone)
    dspGrid->addWidget(m_nr1Btn, 0, 1);
    dspGrid->addWidget(m_nr2Btn, 0, 2);
    dspGrid->addWidget(m_nr3Btn, 0, 3);

    // Row 1: NR4 | DFNR | MNR | (col 3 empty)
    m_nr4Btn  = makeToggle(QStringLiteral("NR4"));
    m_dfnrBtn = makeToggle(QStringLiteral("DFNR"));  // Full label — was "DFN" (truncated at 28px); now fits at uniform width
    m_mnrBtn  = makeToggle(QStringLiteral("MNR"));
    m_nnrBtn  = makeToggle(QStringLiteral("NNR"));
    m_nr4Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_dfnrBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_mnrBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_nnrBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_nr4Btn->setToolTip(QStringLiteral("NR4: SBNR (Spectral Baseline NR). Left-click activates, right-click adjusts knobs"));
    m_dfnrBtn->setToolTip(QStringLiteral("DFNR: DeepFilter noise reduction. Left-click activates, right-click adjusts knobs"));
    m_mnrBtn->setToolTip(QStringLiteral("MNR: macOS noise reduction. Left-click activates, right-click adjusts knobs"));
    m_nnrBtn->setToolTip(QStringLiteral("NNR: neural noise reduction. Left-click activates, right-click adjusts settings"));
    m_nnrToolTip = m_nnrBtn->toolTip();
    // R-R3-49, Sub-epic C-1: each filter's own tooltip, put back when it can
    // run again (updateNrAvailability shows the reason while it cannot).
    m_dfnrToolTip = m_dfnrBtn->toolTip();
    m_mnrToolTip = m_mnrBtn->toolTip();
    dspGrid->addWidget(m_nr4Btn,  1, 0);
    dspGrid->addWidget(m_dfnrBtn, 1, 1);
    dspGrid->addWidget(m_mnrBtn,  1, 2);
    dspGrid->addWidget(m_nnrBtn,  1, 3);
    // R-R3-40: a small amber dot in the NNR button's corner while the Core
    // holds the receiver below the saved choice. Mouse events pass through
    // to the button, whose tooltip carries the same reason.
    m_nnrLimitIndicator = new QLabel(m_nnrBtn->parentWidget());
    m_nnrLimitIndicator->setObjectName(QStringLiteral("vfoNnrLimitIndicator"));
    m_nnrLimitIndicator->setFixedSize(6, 6);
    m_nnrLimitIndicator->setStyleSheet(
        QStringLiteral("QLabel { background: %1; border-radius: 3px; }")
            .arg(QLatin1String(NereusSDR::Style::kAmberText)));
    m_nnrLimitIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_nnrLimitIndicator->setVisible(false);
    dspGrid->addWidget(m_nnrLimitIndicator, 1, 3, Qt::AlignTop | Qt::AlignRight);

    // R-R3-49, Sub-epic C-1: DFNR and MNR are never hidden (operator,
    // 2026-09-25: "Not a fan of disappearing buttons but rather disabled.").
    // One that cannot run is shown disabled with the plain reason
    // (updateNrAvailability): the model's, which in a remote window is the
    // Core's, or this build's with no model. BNR (NVIDIA) is not offered
    // (operator, 2026-09-25), so row 2 keeps ANF and SNB only.
    // The shared toggle style has no disabled look, so these two add the
    // style guide's disabled colours (StyleConstants kDisabled*) to show it.
    for (QPushButton* btn : {m_dfnrBtn, m_mnrBtn}) {
        btn->setStyleSheet(vfoDspToggleStyle() + QStringLiteral(
            "QPushButton:disabled {"
            "  background: %1; color: %2; border: 1px solid %3;"
            "}").arg(NereusSDR::Style::kDisabledBg, NereusSDR::Style::kDisabledText,
                     NereusSDR::Style::kDisabledBorder));
    }
    updateNrAvailability();

    // Row 2: ANF | SNB | (cols 2-3 empty)
    m_anfToggle = makeToggle(QStringLiteral("ANF"));
    // From Thetis console.resx:4062 — chkANF.ToolTip
    m_anfToggle->setToolTip(QStringLiteral("Automatic Notch Filter"));
    m_snbToggle = makeToggle(QStringLiteral("SNB"));
    // From Thetis console.resx:3927 — chkDSPNB2.ToolTip (labeled "SNB" in Thetis UI)
    m_snbToggle->setToolTip(tr(
        "Spectral Noise Blanker: left-click toggles, right-click opens\n"
        "its Setup page (NB/SNB). Runs independently of NB/NB2 and\n"
        "targets tonal/wideband statics that time-domain blankers can't\n"
        "see."));
    // Right-click → Setup page. Mirrors Thetis chkDSPNB2_MouseDown
    // (console.cs:44451 [v2.10.3.13]) which calls ShowSetupTab(NB_Tab).
    m_snbToggle->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_snbToggle, &QWidget::customContextMenuRequested,
            this, [this](const QPoint&) { emit openNbSetupRequested(); });
    dspGrid->addWidget(m_anfToggle, 2, 0);
    dspGrid->addWidget(m_snbToggle, 2, 1);

    // Uniform size for all 9 grid buttons: 64×26 px.
    // User directive: natural button size (not cramped), zero gaps between
    // buttons so their 1px borders abut forming a continuous "board" around
    // the grid. SizePolicy::Fixed prevents cells from widening beyond button
    // size so the sub-grid hugs its content instead of stretching to the
    // flag width.
    for (auto* btn : {m_nbButton, m_nr1Btn, m_nr2Btn, m_nr3Btn,
                      m_nr4Btn, m_dfnrBtn, m_mnrBtn, m_nnrBtn,
                      m_anfToggle, m_snbToggle}) {
        if (btn) {
            btn->setFixedHeight(26);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }

    dspLayout->addLayout(dspGrid);

    // APF toggle + tune slider row — below the 3×4 grid.
    // The toggle acts as the enable button; slider + Hz label are only visible
    // when APF is enabled AND mode is CW (gated by applyModeVisibility).
    m_apfToggle = makeToggle(QStringLiteral("APF"));
    m_apfToggle->setFixedSize(63, 26);
    m_apfToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // From Thetis console.resx:348 — chkCWAPFEnabled.ToolTip
    m_apfToggle->setToolTip(QStringLiteral("Enables APF"));

    {
        auto* apfRow = new QHBoxLayout;
        apfRow->setSpacing(2);

        apfRow->addWidget(m_apfToggle);

        m_apfLabel = nullptr;  // No longer needed — toggle replaces label widget.

        m_apfTuneSlider = new QSlider(Qt::Horizontal, dspWidget);
        m_apfTuneSlider->setRange(-500, 500);
        m_apfTuneSlider->setSingleStep(1);
        m_apfTuneSlider->setValue(0);
        // From Thetis console.resx:303 — ptbCWAPFFreq.ToolTip
        m_apfTuneSlider->setToolTip(QStringLiteral("Sets the CW APF Frequency."));
        apfRow->addWidget(m_apfTuneSlider);

        m_apfTuneLabel = new QLabel(QStringLiteral("0 Hz"), dspWidget);
        m_apfTuneLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(NereusSDR::Style::kLabelMid));
        m_apfTuneLabel->setFixedWidth(44);
        m_apfTuneLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        apfRow->addWidget(m_apfTuneLabel);

        dspLayout->addLayout(apfRow);
    }

    // Mode containers — embedded, hidden by default (S1.9 wires visibility)
    m_fmContainer = new FmOptContainer(dspWidget);
    m_fmContainer->setVisible(false);
    dspLayout->addWidget(m_fmContainer);

    m_digContainer = new DigOffsetContainer(dspWidget);
    m_digContainer->setVisible(false);
    dspLayout->addWidget(m_digContainer);

    m_rttyContainer = new RttyMarkShiftContainer(dspWidget);
    m_rttyContainer->setVisible(false);
    dspLayout->addWidget(m_rttyContainer);

    // If m_slice was set before buildUI ran, forward it to the containers now.
    if (m_slice) {
        m_fmContainer->setSlice(m_slice.data());
        m_digContainer->setSlice(m_slice.data());
        m_rttyContainer->setSlice(m_slice.data());
    }

    // Signal wiring for the 6 toggles
    // NB button emits nbModeCycled() on click; MainWindow calls cycleNbMode()
    // and pushes the new mode back via setNbMode(). clicked() not toggled()
    // so we don't double-fire on programmatic setChecked() calls.
    connect(m_nbButton, &QPushButton::clicked, this, [this] {
        if (!m_updatingFromModel) { emit nbModeCycled(); }
    });
    // Sub-epic C-1: NR bank left-click = setActiveNr(slot) mutual exclusion.
    auto wireNrBtnToggle = [this](QPushButton* btn, NereusSDR::NrSlot slot) {
        connect(btn, &QPushButton::toggled, this, [this, btn, slot](bool on) {
            if (m_updatingFromModel || !m_slice) {
                return;
            }
            m_lastNrButton = btn;
            m_nrRefusal.clear();
            const NereusSDR::NrSlot requested = on ? slot : NereusSDR::NrSlot::Off;
            m_nrClickInFlight = true;
            m_slice->setActiveNr(requested);
            m_nrClickInFlight = false;
            // Fix wave I3: a refused choice (NR3 with no model on the Core)
            // leaves the receiver as it was; the buttons follow it back.
            if (m_slice && m_slice->activeNr() != requested) {
                onActiveNrChanged(m_slice->activeNr());
            }
        });
    };
    wireNrBtnToggle(m_nr1Btn,  NereusSDR::NrSlot::NR1);
    wireNrBtnToggle(m_nr2Btn,  NereusSDR::NrSlot::NR2);
    wireNrBtnToggle(m_nr3Btn,  NereusSDR::NrSlot::NR3);
    wireNrBtnToggle(m_nr4Btn,  NereusSDR::NrSlot::NR4);
    wireNrBtnToggle(m_dfnrBtn, NereusSDR::NrSlot::DFNR);
    wireNrBtnToggle(m_mnrBtn,  NereusSDR::NrSlot::MNR);
    wireNrBtnToggle(m_nnrBtn,  NereusSDR::NrSlot::NNR);

    // Sub-epic C-1: NR bank right-click = DspParamPopup quick controls.
    connect(m_nr1Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr1Popup(m_nr1Btn->mapToGlobal(pos)); });
    connect(m_nr2Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr2Popup(m_nr2Btn->mapToGlobal(pos)); });
    connect(m_nr3Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr3Popup(m_nr3Btn->mapToGlobal(pos)); });
    connect(m_nr4Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr4Popup(m_nr4Btn->mapToGlobal(pos)); });
    connect(m_dfnrBtn, &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showDfnrPopup(m_dfnrBtn->mapToGlobal(pos)); });
    connect(m_mnrBtn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showMnrPopup(m_mnrBtn->mapToGlobal(pos)); });
    connect(m_nnrBtn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNnrPopup(m_nnrBtn->mapToGlobal(pos)); });
    connect(m_anfToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit anfChanged(on); }
    });
    connect(m_snbToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit snbChanged(on); }
    });
    connect(m_apfToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit apfChanged(on); }
        // Re-evaluate APF slider visibility regardless of source (S1.9)
        applyModeVisibility(m_currentMode);
    });

    // APF tune slider — label updates always; emit only when user-driven
    connect(m_apfTuneSlider, &QSlider::valueChanged, this, [this](int hz) {
        m_apfTuneLabel->setText(QString::number(hz) + QStringLiteral(" Hz"));
        if (!m_updatingFromModel) { emit apfTuneHzChanged(hz); }
    });

    // NYI badges — NB1, NB2, NR, ANF, NR2, SNB, APF, FM/DIG/RTTY containers are
    // all live-wired (no badge). Remaining controls with badges are below.

    m_tabStack->addWidget(dspWidget);
}

void VfoWidget::buildModeTab()
{
    auto* modeWidget = new QWidget;
    auto* modeLayout = new QVBoxLayout(modeWidget);
    modeLayout->setContentsMargins(4, 4, 4, 4);
    modeLayout->setSpacing(4);

    // Mode combo row
    {
        auto* modeRow = new QHBoxLayout;
        modeRow->setSpacing(2);
        modeRow->setContentsMargins(0, 0, 0, 0);

        m_modeCmb = new QComboBox(modeWidget);
        // NereusSDR native — Thetis uses discrete radio buttons (radModeUSB, radModeLSB, ...)
        // rather than a combo box. No single Thetis control has an equivalent tooltip.
        m_modeCmb->setToolTip(QStringLiteral("Select demodulation mode"));
        // From Thetis enums.cs DSPMode — common modes
        // 11 Thetis-faithful modes + the NereusSDR-native RADE-U /
        // RADE-L entries (DSPMode::RADE_U = 12, RADE_L = 13, see
        // WdspTypes.h).  Phase 3R L3 added the RADE entries so users
        // can switch into either sideband of the FreeDV RADE neural
        // codec from the floating VFO flag.
        m_modeCmb->addItems({
            QStringLiteral("LSB"), QStringLiteral("USB"),
            QStringLiteral("AM"), QStringLiteral("CWL"),
            QStringLiteral("CWU"), QStringLiteral("FM"),
            QStringLiteral("DIGU"), QStringLiteral("DIGL"),
            QStringLiteral("SAM"), QStringLiteral("DSB"),
            QStringLiteral("DRM"),
            QStringLiteral("RADE-U"),  // Phase 3R L3, NereusSDR-native upper
            QStringLiteral("RADE-L")   // Phase 3R L3, NereusSDR-native lower
        });
        m_modeCmb->setCurrentText(QStringLiteral("USB"));
        m_modeCmb->setStyleSheet(
            QStringLiteral("QComboBox { background: #1a2a3a; color: #c8d8e8;"
                            "border: 1px solid #304050; border-radius: 3px;"
                            "padding: 1px 4px; font-size: 11px; }"
                            "QComboBox::drop-down { border: none; }"
                            "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
                            "selection-background-color: #0070c0; }"));
        connect(m_modeCmb, &QComboBox::currentTextChanged,
                this, [this](const QString& text) {
            if (!m_updatingFromModel) {
                DSPMode mode = SliceModel::modeFromName(text);
                m_currentMode = mode;
                applyModeVisibility(mode);    // S1.9 — user-driven mode change
                rebuildFilterButtons(mode);
                // Update mode tab label
                if (m_tabButtons.size() > 2) {
                    m_tabButtons[2]->setText(text);
                }
                updateSnrVisibility();        // Phase 3R L1 — paint SNR row
                updateModeTabAccent();        // Phase 3R L3 — purple accent
                emit modeChanged(mode);
            }
        });
        modeRow->addWidget(m_modeCmb, 1);  // stretch — combo fills available space
        modeLayout->addLayout(modeRow);
    }

    // Filter preset buttons (dynamic per mode)
    m_filterBtnContainer = new QWidget(modeWidget);
    modeLayout->addWidget(m_filterBtnContainer);
    rebuildFilterButtons(DSPMode::USB);

    // RF Gain slider removed — AGC-T (Audio tab) controls the same WDSP
    // max_gain parameter. Revisit when spectrum-overlay AGC-T line lands.

    modeLayout->addStretch();
    m_tabStack->addWidget(modeWidget);
}

void VfoWidget::buildXRitTab()
{
    // NereusSDR native X/RIT tab — AetherSDR pattern, control surfaces are native
    // (per feedback_source_first_ui_vs_dsp.md). DSP state is all stubs from S1.6;
    // Stage 2 wires the WDSP/SliceModel calls and removes NYI badges.
    auto* ritWidget = new QWidget;
    auto* vbox = new QVBoxLayout(ritWidget);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(2);

    static const char* kZeroBtn =
        "QPushButton {"
        "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
        "  color: #c8d8e8; font-size: 12px; font-weight: bold; padding: 1px;"
        "}"
        "QPushButton:hover { border: 1px solid #0090e0; }";

    // --- RIT row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ritBtn = new QPushButton(QStringLiteral("RIT"), ritWidget);
        m_ritBtn->setObjectName(QStringLiteral("VfoRitButton"));
        m_ritBtn->setCheckable(true);
        m_ritBtn->setStyleSheet(vfoDspToggleStyle());
        m_ritBtn->setFixedHeight(22);
        // From Thetis console.resx:4335 — chkRIT.ToolTip
        m_ritBtn->setToolTip(QStringLiteral("Receive Incremental Tuning - offset RX frequency by value below in Hz."));
        row->addWidget(m_ritBtn);

        m_ritLabel = new ScrollableLabel(ritWidget);
        m_ritLabel->setRange(-10000, 10000);
        m_ritLabel->setStep(m_stepHz);
        m_ritLabel->setValue(0);
        m_ritLabel->setFormat([](int v) {
            return QString::asprintf("%+d Hz", v);
        });
        row->addWidget(m_ritLabel, 1);

        m_ritZeroBtn = new QPushButton(QStringLiteral("0"), ritWidget);
        m_ritZeroBtn->setObjectName(QStringLiteral("VfoRitZeroButton"));
        m_ritZeroBtn->setFixedWidth(20);
        m_ritZeroBtn->setFlat(true);
        m_ritZeroBtn->setStyleSheet(kZeroBtn);
        // From Thetis console.resx:4185 — btnRITReset.ToolTip
        m_ritZeroBtn->setToolTip(QStringLiteral("Clear RIT"));
        row->addWidget(m_ritZeroBtn);

        vbox->addLayout(row);
    }

    // --- XIT row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_xitBtn = new QPushButton(QStringLiteral("XIT"), ritWidget);
        m_xitBtn->setObjectName(QStringLiteral("VfoXitButton"));
        m_xitBtn->setCheckable(true);
        m_xitBtn->setStyleSheet(vfoDspToggleStyle());
        m_xitBtn->setFixedHeight(22);
        // From Thetis console.resx:4416 — chkXIT.ToolTip
        // XIT stored in SliceModel for Phase 3M-1 TX use; client offset displayed now.
        // XIT wired in B6 — TX NCO shift functional.
        m_xitBtn->setToolTip(QStringLiteral("Transmit Incremental Tuning - offset TX frequency by the value below in Hz."));
        row->addWidget(m_xitBtn);

        m_xitLabel = new ScrollableLabel(ritWidget);
        m_xitLabel->setObjectName(QStringLiteral("VfoXitOffset"));
        m_xitLabel->setRange(-10000, 10000);
        m_xitLabel->setStep(m_stepHz);
        m_xitLabel->setValue(0);
        m_xitLabel->setFormat([](int v) {
            return QString::asprintf("%+d Hz", v);
        });
        row->addWidget(m_xitLabel, 1);

        m_xitZeroBtn = new QPushButton(QStringLiteral("0"), ritWidget);
        m_xitZeroBtn->setObjectName(QStringLiteral("VfoXitZeroButton"));
        m_xitZeroBtn->setFixedWidth(20);
        m_xitZeroBtn->setFlat(true);
        m_xitZeroBtn->setStyleSheet(kZeroBtn);
        // From Thetis console.resx:4224 — btnXITReset.ToolTip
        m_xitZeroBtn->setToolTip(QStringLiteral("Clear XIT"));
        row->addWidget(m_xitZeroBtn);

        vbox->addLayout(row);
    }

    // --- Bottom row: STEP cycle ---
    // Lock button removed (B7) — redundant with Close-strip Lock. The Close-strip
    // Lock is always visible; the X/RIT-tab Lock required a tab switch to access.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Step cycle button: emits stepCycleRequested, which
        // MainWindow::createSliceFlag routes to SliceModel::changeTuneStepUp().
        m_stepCycleBtn = new QPushButton(
            formatTuneStepLabel(m_stepHz), ritWidget);
        m_stepCycleBtn->setFlat(true);
        m_stepCycleBtn->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
                           "  color: #c8d8e8; font-size: 11px; font-weight: bold; padding: 2px 4px;"
                           "}"
                           "QPushButton:hover { border: 1px solid #0090e0; }"));
        m_stepCycleBtn->setFixedHeight(22);
        // From Thetis console.cs:29034-29038 [v2.10.3.15]: a left click on the step
        // display (txtWheelTune, whose MouseDown is bound to WheelTune_MouseDown at
        // console.Designer.cs:2870) calls ChangeTuneStepUp, which advances the step
        // and wraps. Thetis also has larger / smaller step buttons,
        // btnChangeTuneStepLarger_Click and btnChangeTuneStepSmaller_Click
        // (console.cs:30635-30643). This single button mirrors the left-click
        // behaviour.
        m_stepCycleBtn->setToolTip(QStringLiteral("Cycle tuning step size (click to advance to next step)"));
        row->addWidget(m_stepCycleBtn, 1);

        vbox->addLayout(row);
    }

    vbox->addStretch();

    // --- Signal wiring ---
    connect(m_ritBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit ritEnabledChanged(on);
        }
    });

    connect(m_ritLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_updatingFromModel) {
            emit ritHzChanged(hz);
        }
    });

    connect(m_ritZeroBtn, &QPushButton::clicked, this, [this]() {
        m_ritLabel->setValue(0);
        if (!m_updatingFromModel) {
            emit ritHzChanged(0);
        }
    });

    // R-R3-49, R-R3-21 (parity Task 11): XIT is a slice setting, written
    // in a remote window as in a local one (the Core's slice follows), and
    // not tied to the transmit permission. RIT above has the same shape.
    connect(m_xitBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit xitEnabledChanged(on);
        }
    });

    connect(m_xitLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_updatingFromModel) {
            emit xitHzChanged(hz);
        }
    });

    connect(m_xitZeroBtn, &QPushButton::clicked, this, [this]() {
        m_xitLabel->setValue(0);
        if (!m_updatingFromModel) {
            emit xitHzChanged(0);
        }
    });

    connect(m_stepCycleBtn, &QPushButton::clicked, this, [this]() {
        emit stepCycleRequested();
    });

    // RIT controls are live — no NYI badge.
    // XIT controls are live (B6) — no NYI badge.
    // LOCK removed from this tab (B7) — still present in Close-strip.

    m_tabStack->addWidget(ritWidget);
}

void VfoWidget::rebuildFilterButtons(DSPMode mode)
{
    // 2026-05-13 bench fix (PR #238 v7 — actual root cause): the
    // previous "delete layout + new QGridLayout(parent)" pattern
    // produced a container whose sizeHint permanently returned
    // (0, 0) — diagnostic logs showed this regardless of button
    // count.  Suspected cause: a Qt quirk around deleted-then-
    // immediately-recreated layouts on the same widget where the
    // new layout doesn't register as the widget's layout cleanly.
    //
    // v7: keep the SAME QGridLayout across rebuilds.  Just clear
    // its child widgets via takeAt() and add the new buttons.
    // The layout-as-widget-property association never breaks, so
    // sizeHint propagation stays intact.
    auto* grid = qobject_cast<QGridLayout*>(m_filterBtnContainer->layout());
    if (!grid) {
        grid = new QGridLayout(m_filterBtnContainer);
        grid->setSpacing(2);
        grid->setContentsMargins(0, 0, 0, 0);
    } else {
        // Remove existing buttons from the existing grid.
        while (QLayoutItem* item = grid->takeAt(0)) {
            if (QWidget* w = item->widget()) {
                w->deleteLater();
            }
            delete item;
        }
    }
    // 2026-05-12 bench fix (PR #238): pin the column count so a
    // single-preset mode (RADE_U / RADE_L) doesn't collapse to a
    // 1×1 grid and stretch its lone button across the full
    // container width.  QGridLayout infers columns from populated
    // cells; without explicit stretches a single addWidget(btn,0,0)
    // leaves the grid 1 column wide.  Same kCols=3 used by the
    // for-loop below.  Mirrors the matching fix in RxApplet's
    // m_filterGrid.
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    // Stage C2: prefer FilterPresetStore (user overrides over Thetis defaults).
    // Fall back to SliceModel::presetsForMode if no store is available.
    // InitFilterPresets source: Thetis console.cs:5180-5575 [v2.10.3.13].
    // 3-column layout matches RxApplet's 3-column grid (i/kCols × i%kCols).
    QList<FilterPreset> storePresets;
    if (m_filterPresetStore) {
        storePresets = m_filterPresetStore->presetsForMode(mode);
    } else {
        const auto pairs = SliceModel::presetsForMode(mode);
        for (int idx = 0; idx < pairs.size(); ++idx) {
            FilterPreset fp;
            fp.name = QStringLiteral("F%1").arg(idx + 1);
            fp.low  = pairs[idx].first;
            fp.high = pairs[idx].second;
            storePresets.append(fp);
        }
    }

    auto [defLow, defHigh] = SliceModel::defaultFilterForMode(mode);

    static constexpr int kCols = 3;
    const int count = qMin(storePresets.size(), 10);

    for (int i = 0; i < count; ++i) {
        const FilterPreset& fp = storePresets[i];
        const int low  = fp.low;
        const int high = fp.high;
        const int widthHz = qAbs(high - low);
        QString label;
        if (widthHz >= 1000) {
            label = QStringLiteral("%1K").arg(widthHz / 1000.0, 0, 'g', 2);
        } else {
            label = QStringLiteral("%1").arg(widthHz);
        }

        auto* btn = new QPushButton(label, m_filterBtnContainer);
        btn->setCheckable(true);
        btn->setStyleSheet(vfoModeBtnStyle());
        btn->setFixedHeight(22);
        btn->setProperty("filterLow", low);
        btn->setProperty("filterHigh", high);
        // Tooltip: show name + filter edges
        btn->setToolTip(QStringLiteral("%1: %2 Hz to %3 Hz")
            .arg(fp.name.isEmpty() ? QStringLiteral("F%1").arg(i + 1) : fp.name)
            .arg(low).arg(high));
        // Check if this matches current filter
        if (low == defLow && high == defHigh) {
            btn->setChecked(true);
        }
        // Exclusive toggle: click selects this preset, emits filterChanged.
        // Shift+click also emits txFilterMatchRequested so the TX passband
        // snaps to the same audio Hz range as the RX (Thetis-style
        // alignment shortcut).
        connect(btn, &QPushButton::clicked, this, [this, low, high, mode, btn](bool checked) {
            if (!checked) {
                // Don't allow unchecking the active preset — keep it toggled on
                btn->setChecked(true);
                return;
            }
            // Uncheck all other filter buttons (exclusive group)
            for (auto* child : m_filterBtnContainer->findChildren<QPushButton*>()) {
                if (child != btn) {
                    child->setChecked(false);
                }
            }
            if (!m_updatingFromModel) {
                emit filterChanged(low, high);
                if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
                    // Convert IQ-space preset to TX audio Hz: LSB family
                    // flips magnitude order, USB family is identity,
                    // symmetric uses (0, |high|).
                    const bool isSymmetric =
                        mode == DSPMode::AM || mode == DSPMode::SAM
                     || mode == DSPMode::DSB || mode == DSPMode::FM
                     || mode == DSPMode::DRM;
                    int audioLow, audioHigh;
                    if (isSymmetric) {
                        audioLow  = 0;
                        audioHigh = qAbs(high);
                    } else {
                        const int aLow  = qAbs(low);
                        const int aHigh = qAbs(high);
                        audioLow  = qMin(aLow, aHigh);
                        audioHigh = qMax(aLow, aHigh);
                    }
                    emit txFilterMatchRequested(audioLow, audioHigh);
                }
            }
        });

        // Stage C2: right-click context menu → edit / reset this preset.
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        const int slot = i;
        connect(btn, &QPushButton::customContextMenuRequested, this,
                [this, slot, mode](const QPoint& pos) {
            if (!m_filterPresetStore) { return; }
            QMenu menu(this);
            menu.setStyleSheet(QString::fromLatin1(kPopupMenu));  // Stage C2 — issue #98 parity
            QAction* editAct  = menu.addAction(QStringLiteral("Edit this preset…"));
            QAction* resetAct = menu.addAction(QStringLiteral("Reset this preset"));
            QAction* chosen = menu.exec(qobject_cast<QWidget*>(sender())->mapToGlobal(pos));
            if (chosen == editAct) {
                auto* dlg = new FilterPresetEditDialog(m_filterPresetStore, mode, slot, this);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->exec();
            } else if (chosen == resetAct) {
                m_filterPresetStore->resetPreset(mode, slot);
            }
        });

        grid->addWidget(btn, i / kCols, i % kCols);
    }
    // grid is already the layout of m_filterBtnContainer (either
    // installed during initial construction via the QGridLayout
    // constructor with the parent arg, or reused on subsequent
    // rebuilds via the qobject_cast at the top of this function).
    // No setLayout call needed.

    // 2026-05-12 bench fix (PR #238 v2): force the entire flag
    // layout chain to re-resolve after the rebuild.  v1 invalidated
    // grid -> container -> parentTab and called adjustSize(), but
    // that skipped m_tabStack (a QStackedWidget) which caches its
    // sizeHint from the active page.  Hidden pages in the stack
    // are QSizePolicy::Ignored (buildUI lines 455-458, 988-990),
    // so the stack ONLY consults the active page's hint — and that
    // hint stays stale unless explicitly invalidated.  Result:
    // transitioning RADE -> SSB (1 button cleared, 10 added) left
    // the flag at the 1-row height with the new 4-row SSB grid
    // clipped at the top.
    //
    // Five-step invalidate chain that actually works:
    //   1. grid->invalidate()              — new grid is dirty
    //   2. m_filterBtnContainer->update    — propagate up one level
    //   3. parentTab->updateGeometry()     — propagate to mode tab
    //   4. m_tabStack->updateGeometry()    — invalidate stack cache
    //   5. layout()->invalidate+activate   — invalidate VfoWidget's
    //                                         own QVBoxLayout
    //   6. adjustSize()                    — commit new frame
    // 2026-05-12 bench fix (PR #238 v3): force the flag to resize
    // after rebuilding the preset buttons.  v1 (column stretches)
    // and v2 (per-level adjustSize) both failed because
    // adjustSize() on a layout-managed widget gets overridden by
    // the parent layout's next pass, and Qt's layout
    // invalidations are POSTED EVENTS that don't fire until the
    // next event-loop tick — so by the time the final adjustSize
    // ran on `this`, every ancestor still had its stale cached
    // sizeHint.
    //
    // v3: invalidate bottom-up via updateGeometry (just marks the
    // cache dirty at each level), then drain the queued
    // QEvent::LayoutRequest events synchronously with
    // sendPostedEvents BEFORE the final adjustSize.  The drain
    // forces Qt to re-poll every level's sizeHint with the
    // rebuilt content; adjustSize then reads the FRESH hint and
    // commits the new frame.  No hide/show flash, no per-level
    // adjustSize, no event-loop deferral.
    // 2026-05-13 bench fix (PR #238 v8 — final): drain queued
    // LayoutRequest events with nullptr receiver so the
    // sizeHint chain is fresh when adjustSize reads it.
    //
    // grid->addWidget() in the loop above queues LayoutRequest
    // events on m_filterBtnContainer (the grid's parent widget),
    // not on VfoWidget itself.  An earlier attempt used
    // `sendPostedEvents(this, …)` which only drains events posted
    // to `this` — so the container-level requests stayed queued
    // and adjustSize() read stale hints.  Passing nullptr as the
    // receiver drains LayoutRequest for every widget tree-wide,
    // which is what we want.
    //
    // adjustSize() then commits the new flag frame.  Together
    // with the layout-reuse refactor at the top of this function
    // (v7) and the CurrentPageSizedStack subclass (v6), this is
    // the third and final piece of the layout-resize chain.
    // 2026-05-13 bench fix (PR #238 v9): belt-and-suspenders.
    // Even with the layout-reuse refactor (v7) and the
    // LayoutRequest drain (v8) the FLAG's sizeHint sometimes
    // under-reports the actual height needed for the filter
    // buttons (seen at the bench when the user has the mode tab
    // open and switches between modes with very different button
    // counts).  Force a minimum height on m_filterBtnContainer
    // computed from the actual button geometry so the parent
    // layout chain MUST reserve the right amount of space
    // regardless of what sizeHint() returns.
    //
    //   rows  = ceil(count / kCols)
    //   minH  = rows * btnH + (rows - 1) * spacing
    //
    // With kCols=3, kBtnH=22, spacing=2:
    //   RADE 1 btn  -> 1 row -> minH = 22
    //   LSB  10 btns -> 4 rows -> minH = 4*22 + 3*2 = 94
    {
        constexpr int kBtnH    = 22;
        constexpr int kSpacing = 2;
        const int rows = count > 0 ? (count + kCols - 1) / kCols : 0;
        const int minH = rows > 0 ? rows * kBtnH + (rows - 1) * kSpacing : 0;
        m_filterBtnContainer->setMinimumHeight(minH);
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    adjustSize();
}

// ---- Stage C2: FilterPresetStore coupling ----

void VfoWidget::setFilterPresetStore(FilterPresetStore* store)
{
    // Disconnect from old store if any.
    if (m_filterPresetStore) {
        disconnect(m_filterPresetStore, &FilterPresetStore::presetsChanged,
                   this, nullptr);
    }
    m_filterPresetStore = store;
    if (m_filterPresetStore) {
        connect(m_filterPresetStore, &FilterPresetStore::presetsChanged,
                this, [this](DSPMode mode) {
            // Only rebuild when the changed mode matches the currently-shown mode.
            if (mode == m_currentMode) {
                rebuildFilterButtons(mode);
                // The active highlight is restored by setFilter() which the model
                // will call (or already has set via the filterChanged guard path).
            }
        });
    }
    // Rebuild immediately so existing buttons reflect the store state.
    rebuildFilterButtons(m_currentMode);
}

// ---- State setters (guarded) ----

void VfoWidget::setFrequency(double hz)
{
    m_updatingFromModel = true;
    m_frequency = hz;
    updateFreqLabel();
    m_updatingFromModel = false;
}

void VfoWidget::setMode(DSPMode mode)
{
    m_updatingFromModel = true;
    m_currentMode = mode;
    QString name = SliceModel::modeName(mode);
    m_modeCmb->setCurrentText(name);
    if (m_tabButtons.size() > 2) {
        m_tabButtons[2]->setText(name);
    }
    rebuildFilterButtons(mode);
    applyModeVisibility(mode);    // S1.9 — model-driven mode change
    updateSnrVisibility();        // Phase 3R L1 — show SNR row only in RADE
    updateModeTabAccent();        // Phase 3R L3 — purple accent in RADE
    m_updatingFromModel = false;
}

void VfoWidget::setFilter(int low, int high)
{
    m_updatingFromModel = true;
    // Kept, not just rendered as text: SpectrumWidget reads these back through
    // filterLow()/filterHigh() to shade THIS slice's passband. See the header.
    m_filterLowHz = low;
    m_filterHighHz = high;
    m_filterWidthLbl->setText(formatFilterWidth(low, high));
    // Update checked state of filter buttons — match by stored property
    for (auto* btn : m_filterBtnContainer->findChildren<QPushButton*>()) {
        int bLow = btn->property("filterLow").toInt();
        int bHigh = btn->property("filterHigh").toInt();
        btn->setChecked(bLow == low && bHigh == high);
    }
    m_updatingFromModel = false;
}

void VfoWidget::setAgcMode(AGCMode mode)
{
    m_updatingFromModel = true;
    int idx = static_cast<int>(mode);
    for (int i = 0; i < 5; ++i) {
        if (m_agcBtns[i]) {
            m_agcBtns[i]->setChecked(i == idx);
        }
    }
    m_updatingFromModel = false;
}

void VfoWidget::setAfGain(int gain)
{
    m_modelAfGain = gain;
    // Task 14b: a listened flag shows this device's own volume instead.
    if (isListening()) { return; }
    m_updatingFromModel = true;
    m_afGainSlider->setValue(gain);
    m_afGainLabel->setText(QString::number(gain));
    m_updatingFromModel = false;
}

void VfoWidget::setRfGain(int)
{
    // RF Gain slider removed — AGC-T controls the same parameter.
}

void VfoWidget::setRxAntenna(const QString& ant)
{
    m_updatingFromModel = true;
    m_rxAntBtn->setText(ant);
    m_updatingFromModel = false;
}

void VfoWidget::setTxAntenna(const QString& ant)
{
    m_updatingFromModel = true;
    m_txAntBtn->setText(ant);
    m_updatingFromModel = false;
}

void VfoWidget::setStepHz(int hz)
{
    m_stepHz = hz;
    if (m_ritLabel) {
        m_ritLabel->setStep(hz);
    }
    if (m_xitLabel) {
        m_xitLabel->setStep(hz);
    }
    if (m_stepCycleBtn) {
        m_stepCycleBtn->setText(formatTuneStepLabel(hz));
    }
}

// Phase 3F Sub-Epic C Task 9: emit handoff request to MainWindow for forwarding.
void VfoWidget::onTxBadgeClicked()
{
    // Task 78: the radio's own transmission is not this window's to move.
    if (m_inUseByRadio) {
        m_txBadge->setChecked(false);
        return;
    }
    // TX badge take (JJ, 2026-09-30): a click that starts a take. The
    // badge's own toggle is undone: the TX mark follows the Core.
    if (txBadgeTakeOffered()) {
        m_txBadge->setChecked(m_txMarked);
        emit txTakeRequested(m_sliceIndex);
        return;
    }
    if (!m_transmitPermitted) { return; }
    // Task 14a: a slice another device controls is not this window's to
    // make the TX slice.
    if (isListening()) { return; }
    emit txHandoffRequested(m_sliceIndex);
}

bool VfoWidget::txBadgeTakeOffered() const
{
    // Offered only where a click could not make the slice the TX slice at
    // once, never while a slice request waits or the radio's own PTT
    // transmits on this frequency.
    return m_txBadgeOffer.offered && m_accessPending.isEmpty() && !m_inUseByRadio
        && (!m_transmitPermitted || isListening());
}

void VfoWidget::setTxBadgeOffer(const TxBadgeOffer& offer)
{
    if (m_txBadgeOffer == offer) { return; }
    m_txBadgeOffer = offer;
    updateTransmitControlAvailability();
}

void VfoWidget::setSliceIndex(int index)
{
    m_sliceIndex = index;
    if (index >= 0) {
        m_sliceBadge->setText(QString(QChar(QLatin1Char(static_cast<char>('A' + index)))));
        QColor c = sliceColor(index);
        m_sliceBadge->setStyleSheet(
            QStringLiteral("background: %1; color: white; font-size: 11px;"
                            "font-weight: bold; border-radius: 3px;").arg(c.name()));
    }
}

void VfoWidget::setTxSlice(bool isTx)
{
    m_txMarked = isTx;
    m_txBadge->setChecked(isTx && !m_inUseByRadio);
}

void VfoWidget::setStationPresentationAllowed(bool allowed)
{
    if (m_stationPresentationAllowed == allowed) { return; }
    m_stationPresentationAllowed = allowed;
    if (!allowed) { hide(); }
    positionFloatingButtons();
}

bool VfoWidget::txSliceShown() const
{
    return m_txBadge && m_txBadge->isChecked();
}

QString VfoWidget::inUseByRadioText()
{
    return QStringLiteral("The radio is transmitting on this frequency.");
}

void VfoWidget::setInUseByRadio(bool inUse)
{
    if (m_inUseByRadio == inUse) { return; }
    m_inUseByRadio = inUse;
    m_txBadge->setProperty("inUseByRadio", inUse);
    if (inUse) {
        m_txBadge->setChecked(false);
        m_txBadge->setStyleSheet(
            QStringLiteral("QPushButton { background: #3a2a10; border: 1px solid #d09020;"
                           "border-radius: 3px; color: #ffc040; font-size: 10px; font-weight: bold; }"));
        m_txBadge->setToolTip(inUseByRadioText());
        m_txBadge->setAccessibleDescription(inUseByRadioText());
    } else {
        m_txBadge->setStyleSheet(
            QStringLiteral("QPushButton { background: #1a2a3a; border: 1px solid #304050;"
                           "border-radius: 3px; color: #6888a0; font-size: 10px; font-weight: bold; }"
                           "QPushButton:checked { background: #6a3030; border-color: #ff4444; color: #ff8080; }"));
        m_txBadge->setToolTip(QStringLiteral("Indicates this slice is the TX slice"));
        m_txBadge->setAccessibleDescription(QString());
    }
    // TX badge take: a held or offering badge keeps these as the words it
    // returns to, and shows its own now.
    if (m_txBadge->property(kSavedTransmitTooltip).isValid()) {
        m_txBadge->setProperty(kSavedTransmitTooltip, m_txBadge->toolTip());
        m_txBadge->setProperty(kSavedTransmitDescription, m_txBadge->accessibleDescription());
        updateTransmitControlAvailability();
    }
}

void VfoWidget::setActiveSlice(bool active)
{
    if (m_activeSlice == active) {
        return;
    }
    m_activeSlice = active;
    emit activeSliceChanged(active);
}

void VfoWidget::setAntennaList(const QStringList& ants)
{
    m_antennaList = ants;
}

void VfoWidget::setSmeter(double dbm)
{
    m_smeterDbm = dbm;
    if (m_levelBar) {
        m_levelBar->setValue(float(dbm));
    }
}

void VfoWidget::setRitEnabled(bool v)
{
    if (m_ritBtn && m_ritBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_ritBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setRitHz(int hz)
{
    if (m_ritLabel && m_ritLabel->value() != hz) {
        m_updatingFromModel = true;
        m_ritLabel->setValue(hz);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setXitEnabled(bool v)
{
    if (m_xitBtn && m_xitBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_xitBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setXitHz(int hz)
{
    if (m_xitLabel && m_xitLabel->value() != hz) {
        m_updatingFromModel = true;
        m_xitLabel->setValue(hz);
        m_updatingFromModel = false;
    }
}

// ---- DSP tab state setters (S1.8b) ----

// Label + styling mirror Thetis console.cs:43518-43546 [v2.10.3.13]:
//   Off → label "NB", dim background, unchecked
//   NB  → label "NB", active background, checked
//   NB2 → label "NB2", active background, indeterminate (shown as checked)
void VfoWidget::setNbMode(NereusSDR::NbMode m)
{
    if (!m_nbButton) { return; }
    m_updatingFromModel = true;
    switch (m) {
        case NereusSDR::NbMode::Off:
            m_nbButton->setText(QStringLiteral("NB"));
            m_nbButton->setChecked(false);
            break;
        case NereusSDR::NbMode::NB:
            m_nbButton->setText(QStringLiteral("NB"));
            m_nbButton->setChecked(true);
            break;
        case NereusSDR::NbMode::NB2:
            m_nbButton->setText(QStringLiteral("NB2"));
            m_nbButton->setChecked(true);
            break;
    }
    m_updatingFromModel = false;
}

void VfoWidget::setNr2Enabled(bool v)
{
    // Legacy adapter called by MainWindow — forward to the NR2 slot button.
    // Kept for API compatibility during the transition; Task 18 will rewire
    // MainWindow to call onActiveNrChanged directly via the signal.
    onActiveNrChanged(v ? NereusSDR::NrSlot::NR2 : NereusSDR::NrSlot::Off);
}

void VfoWidget::onActiveNrChanged(NereusSDR::NrSlot slot)
{
    if (!m_nr1Btn) { return; }  // not yet built
    QSignalBlocker b1(m_nr1Btn),  b2(m_nr2Btn),  b3(m_nr3Btn), b4(m_nr4Btn);
    QSignalBlocker b5(m_dfnrBtn), b7(m_mnrBtn), b8(m_nnrBtn);
    m_nr1Btn->setChecked(slot  == NereusSDR::NrSlot::NR1);
    m_nr2Btn->setChecked(slot  == NereusSDR::NrSlot::NR2);
    m_nr3Btn->setChecked(slot  == NereusSDR::NrSlot::NR3);
    m_nr4Btn->setChecked(slot  == NereusSDR::NrSlot::NR4);
    m_dfnrBtn->setChecked(slot == NereusSDR::NrSlot::DFNR);
    m_mnrBtn->setChecked(slot  == NereusSDR::NrSlot::MNR);
    m_nnrBtn->setChecked(slot  == NereusSDR::NrSlot::NNR);
}

void VfoWidget::onNnrLimitChanged(int limit)
{
    if (!m_nnrBtn || !m_nnrLimitIndicator) { return; }  // not yet built
    // The slice words the reason for this window: a remote window names
    // the Core computer, a local one this computer.
    const QString reason = m_slice && limit != 0 ? m_slice->nnrLimitText() : QString();
    m_nnrLimitIndicator->setVisible(!reason.isEmpty());
    m_nnrLimitIndicator->setToolTip(reason);
    m_nnrBtn->setToolTip(reason.isEmpty() ? m_nnrToolTip : reason);
}

// Fix wave I3: say why a noise reducer did not turn on, at the button that
// asked, in the receiver's plain words.
void VfoWidget::onNrSelectionRefused(const QString& reason)
{
    if (m_slice) {
        onActiveNrChanged(m_slice->activeNr());
    }
    // Follow-up item 3: one message per refused click, at the control that
    // was clicked. A choice made elsewhere says why there.
    if (!m_nrClickInFlight) {
        return;
    }
    // A Core refusal is shown in user words; the raw text is logged.
    m_nrRefusal = reason.isEmpty() ? reason : OperatorReasonText::forDisplay(reason);
    QWidget* anchor = m_lastNrButton ? static_cast<QWidget*>(m_lastNrButton.data())
                                     : static_cast<QWidget*>(m_nr3Btn);
    if (anchor && anchor->isVisible() && !m_nrRefusal.isEmpty()) {
        QToolTip::showText(anchor->mapToGlobal(QPoint(0, anchor->height())), m_nrRefusal, anchor);
    }
}

void VfoWidget::setSnbEnabled(bool v)
{
    if (m_snbToggle && m_snbToggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_snbToggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setApfEnabled(bool v)
{
    if (m_apfToggle && m_apfToggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_apfToggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setApfTuneHz(int hz)
{
    if (m_apfTuneSlider && m_apfTuneSlider->value() != hz) {
        m_updatingFromModel = true;
        m_apfTuneSlider->setValue(hz);
        m_updatingFromModel = false;
    }
}

// ---- Mode container visibility (S1.9) ----

void VfoWidget::applyModeVisibility(DSPMode mode)
{
    // Mode containers embedded in DspTab — show only the one matching
    // the active demodulation mode.
    if (m_fmContainer) {
        // R-R3-49: no empty FM box while every FM control is unbuilt.
        m_fmContainer->setVisible(mode == DSPMode::FM
                                  && FmOptContainer::hasBuiltControls());
    }
    if (m_digContainer) {
        m_digContainer->setVisible(mode == DSPMode::DIGL || mode == DSPMode::DIGU);
    }
    if (m_rttyContainer) {
        // RTTY is a DIGL sub-mode — mark/shift controls shown alongside DIG offset
        m_rttyContainer->setVisible(mode == DSPMode::DIGL);
    }

    // APF tune slider — visible only when APF is enabled AND mode is CW.
    // m_apfToggle (the enable button) is always visible; only the slider + Hz
    // label are gated. m_apfLabel was removed in the Sub-epic C-1 3×4 redesign.
    bool apfVisible = (m_apfToggle && m_apfToggle->isChecked())
                      && (mode == DSPMode::CWL || mode == DSPMode::CWU);
    if (m_apfTuneSlider) {
        m_apfTuneSlider->setVisible(apfVisible);
    }
    if (m_apfTuneLabel) {
        m_apfTuneLabel->setVisible(apfVisible);
    }
}

// ---- Audio tab state setters (S1.8c — guarded against re-emit) ----

void VfoWidget::setMuted(bool v)
{
    m_modelMuted = v;
    // Task 14b: a listened flag shows this device's own mute instead.
    if (isListening()) { return; }
    if (m_muteBtn && m_muteBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_muteBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setAudioPan(double pan)
{
    if (m_panSlider) {
        int val = static_cast<int>(std::round(pan * 100.0));
        if (m_panSlider->value() != val) {
            m_updatingFromModel = true;
            m_panSlider->setValue(val);
            if (m_panLabel) {
                m_panLabel->setText(QString::number(val));
            }
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::setSsqlEnabled(bool v)
{
    if (m_sqlBtn && m_sqlBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_sqlBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setSsqlThresh(double dB)
{
    if (m_sqlSlider) {
        int val = static_cast<int>(std::round(dB));
        val = std::max(0, std::min(100, val));
        if (m_sqlSlider->value() != val) {
            m_updatingFromModel = true;
            m_sqlSlider->setValue(val);
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::setAgcThreshold(int dBu)
{
    if (m_agcTSlider) {
        int val = std::max(ControlRanges::kAgcThresholdMinDb,
                           std::min(ControlRanges::kAgcThresholdMaxDb, dBu));
        if (m_agcTSlider->value() != val) {
            m_updatingFromModel = true;
            m_agcTSlider->setValue(val);
            if (m_agcTLabel) {
                m_agcTLabel->setText(QString::number(val));
            }
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset,
                                   bool noiseFloorValid)
{
    m_autoAgcActive = autoOn;
    m_noiseFloorDbm = noiseFloorDbm;

    if (!m_agcTSlider || !m_agcTContainer) {
        return;
    }

    if (autoOn) {
        // AUTO badge → bright green (active) — only the button illuminates
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a2a1a; border: 1px solid #adff2f;"
                                "color: #adff2f; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                                "QPushButton:hover { background: #2a3a2a; }"));
        }

        // Show info sub-line
        if (m_agcInfoLabel) {
            m_agcInfoLabel->setText(
                noiseFloorValid
                    ? QStringLiteral("NF %1 dB \u00b7 offset +%2")
                          .arg(static_cast<int>(noiseFloorDbm))
                          .arg(static_cast<int>(offset))
                    : QStringLiteral("NF awaiting measurement"));
            m_agcInfoLabel->show();
        }
    } else {
        // AUTO badge → dim gray (inactive)
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"
                                "color: #556; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                                "QPushButton:hover { border-color: #adff2f; }"));
        }
        // Hide info sub-line
        if (m_agcInfoLabel) {
            m_agcInfoLabel->hide();
        }
    }
}

void VfoWidget::setBinauralEnabled(bool v)
{
    if (m_binBtn && m_binBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_binBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

// ---- R-R3-45: speakers or headphones ----

QString VfoWidget::headphonesMissingText()
{
    return QStringLiteral("Silent: no headphones are set up. "
                          "Turn them on in Setup, Audio, Devices.");
}

void VfoWidget::setOutputRoute(SliceModel::OutputRoute route)
{
    const bool headphones = route == SliceModel::OutputRoute::Headphones;
    const bool wasUpdating = m_updatingFromModel;
    m_updatingFromModel = true;
    if (m_speakersBtn) {
        m_speakersBtn->setChecked(!headphones);
    }
    if (m_headphonesBtn) {
        m_headphonesBtn->setChecked(headphones);
    }
    m_updatingFromModel = wasUpdating;
    updateOutputNotice();
}

void VfoWidget::setHeadphonesAvailable(bool available)
{
    m_headphonesAvailable = available;
    updateOutputNotice();
}

QString VfoWidget::headphonesNotOpenedText()
{
    return QStringLiteral("Silent: the headphones could not be opened.");
}

void VfoWidget::setHeadphonesEnabled(bool enabled)
{
    m_headphonesEnabled = enabled;
    updateOutputNotice();
}

void VfoWidget::setHeadphonesProblem(const QString& problem)
{
    m_headphonesProblem = problem;
    updateOutputNotice();
}

void VfoWidget::updateOutputNotice()
{
    if (!m_outputNotice) {
        return;
    }
    const bool headphones = m_headphonesBtn && m_headphonesBtn->isChecked();
    // R-R3-45: a remote window's own reason comes first (a Core that
    // cannot send the headphones mix plays the receiver on the speakers, so
    // "silent" would be wrong; a headphones device that failed). Then this
    // computer's: headphones turned on that did not open, or none set up.
    if (!m_headphonesProblem.isEmpty()) {
        m_outputNotice->setText(m_headphonesProblem);
    } else if (!m_headphonesAvailable) {
        m_outputNotice->setText(m_headphonesEnabled ? headphonesNotOpenedText()
                                                    : headphonesMissingText());
    }
    m_outputNotice->setVisible(headphones
                               && (!m_headphonesAvailable || !m_headphonesProblem.isEmpty()));
}

// ---- Slice coupling (for mode container binding only) ----

void VfoWidget::setSlice(SliceModel* slice)
{
    // Drop prior VAX bindings before m_slice is reassigned — otherwise a
    // repeat setSlice() leaks connections: each click would re-invoke the
    // old lambda, and vaxChannelChanged from a stale SliceModel could
    // clobber the selector away from the currently bound slice.
    if (m_vaxSelector) {
        if (m_slice) {
            disconnect(m_slice, &SliceModel::vaxChannelChanged,
                       m_vaxSelector, &VaxChannelSelector::setValue);
        }
        disconnect(m_vaxSelector, &VaxChannelSelector::valueChanged,
                   this, nullptr);
    }

    if (m_slice) {
        disconnect(m_slice, &SliceModel::outputRouteChanged,
                   this, &VfoWidget::setOutputRoute);
        disconnect(m_slice, &SliceModel::nnrLimitChanged,
                   this, &VfoWidget::onNnrLimitChanged);
        disconnect(m_slice, &SliceModel::nrSelectionRefused, this, nullptr);
        disconnect(m_slice, &SliceModel::radeReasonChanged,
                   this, &VfoWidget::setRadeReason);
    }
    m_slice = QPointer<SliceModel>(slice);
    if (m_fmContainer) {
        m_fmContainer->setSlice(slice);
    }
    if (m_digContainer) {
        m_digContainer->setSlice(slice);
    }
    if (m_rttyContainer) {
        m_rttyContainer->setSlice(slice);
    }

    // R-R3-45: speakers or headphones. The buttons write m_slice directly;
    // the slice's change signal brings the flag back in step.
    if (slice) {
        connect(slice, &SliceModel::outputRouteChanged,
                this, &VfoWidget::setOutputRoute);
    }
    setOutputRoute(slice ? slice->outputRoute()
                         : SliceModel::OutputRoute::Speakers);

    // Sub-epic C-1: NR bank — sync from slice activeNr and initial state.
    if (slice) {
        connect(slice, &SliceModel::activeNrChanged,
                this, &VfoWidget::onActiveNrChanged);
        onActiveNrChanged(slice->activeNr());
        connect(slice, &SliceModel::nnrLimitChanged,
                this, &VfoWidget::onNnrLimitChanged, Qt::UniqueConnection);
        connect(slice, &SliceModel::nrSelectionRefused,
                this, &VfoWidget::onNrSelectionRefused);
    }
    onNnrLimitChanged(slice ? slice->nnrLimit() : 0);

    // Phase 3R L1: SNR row binding. RadeChannel pushes snrDb via the
    // I5 signal-graph (RadeChannel::snrChanged -> RadioModel::onRadeSnrChanged
    // -> SliceModel::setSnrDb). The slice's snrDbChanged is the
    // edge-triggered source we paint from. Seed with the current value
    // so a slice rebinding after a previous SNR update shows the right
    // text immediately.
    if (slice) {
        connect(slice, &SliceModel::snrDbChanged,
                this, &VfoWidget::onSnrChanged);
        onSnrChanged(slice->snrDb());
    }
    // RADE reason: why the slice's RADE decoder is not working, if it is
    // not (the Core's, mirrored, on a remote window).
    if (slice) {
        connect(slice, &SliceModel::radeReasonChanged,
                this, &VfoWidget::setRadeReason, Qt::UniqueConnection);
    }
    setRadeReason(slice ? slice->radeReason() : QString());

    // VAX selector — bidirectional wiring (Phase 3O Sub-Phase 8 Task 8.2)
    if (m_vaxSelector && slice) {
        connect(m_vaxSelector, &VaxChannelSelector::valueChanged,
                this, [this](int ch) {
            if (m_slice) {
                m_slice->setVaxChannel(ch);
            }
        });
        connect(slice, &SliceModel::vaxChannelChanged,
                m_vaxSelector, &VaxChannelSelector::setValue);
        // Sync widget to current model state (e.g. restored from AppSettings)
        m_vaxSelector->setValue(slice->vaxChannel());
    }
}

// ---- Floating control buttons (AetherSDR pattern) ----
// Close, Lock, Record, Play — rendered on parent SpectrumWidget

// Plan 4 follow-up: opaque backgrounds so the floating buttons remain
// visible when a coloured filter overlay (TX or RX) is painted underneath
// them.  Original alpha=15/40 was nearly transparent; against the new
// translucent filter bands the buttons effectively disappeared.  The dark
// blue base matches the spectrum chrome palette and stays distinct from
// either filter colour.
static const char* kFloatingBtn =
    "QPushButton {"
    "  background: rgba(20,30,50,230); border: 1px solid rgba(80,100,130,180);"
    "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0;"
    "}"
    "QPushButton:hover {"
    "  background: rgba(40,55,80,240);"
    "}";

static const char* kFloatingBtnClose =
    "QPushButton {"
    "  background: rgba(20,30,50,230); border: 1px solid rgba(80,100,130,180);"
    "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0;"
    "}"
    "QPushButton:hover {"
    "  background: rgba(204,32,32,220); color: #ffffff;"
    "}";

void VfoWidget::buildFloatingButtons()
{
    QWidget* parent = parentWidget();
    if (!parent || m_closeBtn) {
        return;  // Already built or no parent
    }

    auto makeBtn = [&](const QString& text, const char* style) -> QPushButton* {
        auto* btn = new QPushButton(text, parent);
        btn->setFixedSize(20, 20);
        btn->setStyleSheet(style);
        btn->show();
        return btn;
    };

    // Close button — wired
    m_closeBtn = makeBtn(QStringLiteral("\u2715"), kFloatingBtnClose);
    // NereusSDR native — Thetis has no per-slice close button
    m_closeBtn->setToolTip(QStringLiteral("Close slice"));
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        // Task 14a: on a slice this window only listens to, the close
        // button stops listening; it never removes another device's slice.
        if (isListening()) {
            emit stopListeningRequested(m_sliceIndex);
            return;
        }
        emit closeRequested(m_sliceIndex);
    });
    // Phase 3F (Bug 2): Slice A (index 0) is the last-slice invariant —
    // RadioModel::removeSlice refuses to remove the final slice. Hiding the
    // close button on Slice A keeps the affordance honest (a button that
    // does nothing reads as broken). (Its flag is now torn down like every
    // other when a Core closes Slice A: VFO flag crash lane, 2026-09-30.)
    if (m_sliceIndex == 0 && !isListening()) {
        m_closeBtn->hide();
    }

    // Lock button — wired
    m_lockBtn = makeBtn(QStringLiteral("\U0001F513"), kFloatingBtn);
    // From Thetis console.resx:5787 — chkVFOLock.ToolTip
    m_lockBtn->setToolTip(QStringLiteral("Keeps the VFO from changing while in the middle of a QSO."));
    m_lockBtn->setCheckable(true);
    connect(m_lockBtn, &QPushButton::toggled, this, [this](bool locked) {
        if (!m_updatingFromModel) {
            applyLockedState(locked);
        }
    });
    // Task 14a: the lock writes the slice too, so it is held on a listened
    // flag; the close button's words follow the access.
    if (isListening()) {
        holdForListening(m_lockBtn);
        m_closeBtn->setToolTip(tr("Stop listening"));
    }

    // Record button: checkable, disabled (no consumer yet)
    m_recBtn = makeBtn(QStringLiteral("\u23FA"), kFloatingBtn);
    // From Thetis console.resx:2028 — ckQuickRec.ToolTip
    m_recBtn->setToolTip(QStringLiteral("Quick Record of \"off the air\" signals"));
    m_recBtn->setCheckable(true);
    connect(m_recBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit recordToggled(on);
        }
    });
    m_recBtn->setEnabled(false);  // nothing behind it until the voice recorder is built

    // Play button: checkable, disabled (no consumer yet)
    m_playBtn = makeBtn(QStringLiteral("\u25B6"), kFloatingBtn);
    // From Thetis console.resx:1941 — ckQuickPlay.ToolTip
    m_playBtn->setToolTip(QStringLiteral("Quick Playback of signals recorded \"off the air\""));
    m_playBtn->setCheckable(true);
    connect(m_playBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit playToggled(on);
        }
    });
    m_playBtn->setEnabled(false);  // nothing behind it until the voice recorder is built

    // R-R3-49: record and play are hidden until the voice recorder is
    // built; positionFloatingButtons() keeps them out of the strip.
    UnbuiltFeatures::hideUnlessBuilt(m_recBtn, UnbuiltFeature::Voice);
    UnbuiltFeatures::hideUnlessBuilt(m_playBtn, UnbuiltFeature::Voice);
}

// ---- Lock state: applyLockedState + setLocked (S1.8a review — I3) ----
// applyLockedState is the single path for all lock changes — called by the
// floating m_lockBtn toggled lambda.  setLocked is the inbound edge driven
// by SliceModel::lockedChanged.
// X/RIT-tab Lock removed in B7 (redundant with Close-strip Lock).

void VfoWidget::applyLockedState(bool on)
{
    // Snapshot the incoming guard state so we can restore it around each
    // button update and correctly decide whether to emit at the end.
    const bool wasUpdating = m_updatingFromModel;

    // Update state
    m_locked = on;

    // Drive floating lock button — set guard while calling setChecked so its
    // toggled signal does not re-enter applyLockedState.
    if (m_lockBtn) {
        m_updatingFromModel = true;
        m_lockBtn->setChecked(on);
        m_lockBtn->setText(on ? QStringLiteral("\U0001F512") : QStringLiteral("\U0001F513"));
        if (on) {
            m_lockBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: rgba(255,100,100,80); border: none;"
                "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0; }"
                "QPushButton:hover { background: rgba(255,100,100,120); }"));
        } else {
            m_lockBtn->setStyleSheet(kFloatingBtn);
        }
        m_updatingFromModel = wasUpdating;
    }

    // Only emit lockChanged when the change originates from a user action
    // (i.e., guard was false when this call began).  When called from
    // setLocked() the guard is set true and we skip the emit, preventing
    // a model → widget → model feedback loop.
    if (!wasUpdating) {
        emit lockChanged(on);
    }
}

void VfoWidget::setLocked(bool v)
{
    if (m_locked == v) {
        return;
    }
    // Guard true → applyLockedState will update both buttons but will NOT
    // emit lockChanged back toward the model.
    m_updatingFromModel = true;
    applyLockedState(v);
    m_updatingFromModel = false;
}

void VfoWidget::positionFloatingButtons()
{
    if (!m_closeBtn) {
        return;
    }

    // Stack vertically on the opposite side of the flag from the VFO marker
    // From AetherSDR VfoWidget.cpp:1724-1749
    int btnX;
    if (m_onLeft) {
        // Flag is on the left of marker → buttons on right side of flag
        btnX = x() + width() + 2;
    } else {
        // Flag is on the right of marker → buttons on left side of flag
        btnX = x() - 22;
    }

    // Clamp to parent bounds
    if (parentWidget()) {
        btnX = std::clamp(btnX, 0, parentWidget()->width() - 20);
    }

    int btnY = y();

    // Phase 3F (Bug 2): the close button is hidden on Slice A (index 0);
    // keep it hidden here and let the remaining buttons fill the gap so the
    // strip has no empty slot at the top.
    // Task 14a: on a listened flag the close button stops listening, which
    // is fine on Slice A too.
    const bool closeShown = (m_sliceIndex != 0) || isListening();

    // R-R3-49: record and play take no slot while the voice recorder is
    // not built.
    const bool voiceBuilt = UnbuiltFeatures::isBuilt(UnbuiltFeature::Voice);

    QPushButton* btns[] = {m_closeBtn, m_lockBtn, m_recBtn, m_playBtn};
    for (QPushButton* btn : btns) {
        if (!voiceBuilt && (btn == m_recBtn || btn == m_playBtn)) {
            btn->hide();
            continue;
        }
        const bool isCloseBtn = (btn == m_closeBtn);
        const bool show = m_stationPresentationAllowed && isVisible()
            && (closeShown || !isCloseBtn);
        if (isCloseBtn && !closeShown) {
            btn->hide();
            continue;  // don't advance btnY — next button takes the top slot
        }
        btn->move(btnX, btnY);
        btn->setVisible(show);
        if (show) {
            btn->raise();
        }
        btnY += 22;
    }
}

// The active-flag-on-top invariant (Bench 2026-07-28, Sub-Epic J): this
// flag's own close/lock/record/play buttons are parented to the
// SpectrumWidget, not to this flag (see destroyFloatingButtons above), so
// QWidget::raise() on the flag body alone is not enough -- the buttons are
// separate siblings and stay wherever they were last left, which can be
// behind a flag that used to sit above this one. Same btns[] order as
// positionFloatingButtons() above: buttons first, flag body last, so the
// body also ends up above its own buttons.
void VfoWidget::raiseAboveSiblings()
{
    QPushButton* btns[] = {m_closeBtn, m_lockBtn, m_recBtn, m_playBtn};
    for (QPushButton* btn : btns) {
        if (btn) {
            btn->raise();
        }
    }
    raise();
}

// ---- Positioning ----

void VfoWidget::updatePosition(int vfoX, int specTop, FlagDir dir)
{
    // Build floating buttons on first call (parent is now available)
    if (!m_closeBtn && parentWidget()) {
        buildFloatingButtons();
    }

    int flagW = width();
    int parentW = parentWidget() ? parentWidget()->width() : 2000;
    bool onLeft = false;

    if (dir == FlagDir::ForceLeft) {
        onLeft = true;
    } else if (dir == FlagDir::ForceRight) {
        onLeft = false;
    } else {
        // Auto: flag goes OPPOSITE side of passband so it doesn't cover signals.
        // From AetherSDR VfoWidget.cpp:1696 — onLeft = !lowerSideband
        bool lowerSideband = (m_currentMode == DSPMode::LSB ||
                              m_currentMode == DSPMode::DIGL ||
                              m_currentMode == DSPMode::CWL);
        onLeft = !lowerSideband;
    }

    int x;
    if (onLeft) {
        x = vfoX - flagW;
        // Flip to right if clipped off left edge
        if (x < 0) {
            x = vfoX;
            onLeft = false;
        }
    } else {
        x = vfoX;
        // Flip to left if clipped off right edge
        if (x + flagW > parentW) {
            x = vfoX - flagW;
            onLeft = true;
        }
    }

    // Final clamp to stay on screen
    x = std::clamp(x, 0, std::max(0, parentW - flagW));

    m_onLeft = onLeft;
    move(x, specTop);
    positionFloatingButtons();
}

// ---- Painting ----

void VfoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark panel background — from AetherSDR VfoWidget::paintEvent
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x0a, 0x0a, 0x14, 230));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    // Subtle border
    p.setPen(QColor(255, 255, 255, 30));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    // Colored top border matching slice color
    QColor c = sliceColor(m_sliceIndex);
    p.setPen(QPen(c, 2));
    p.drawLine(2, 1, width() - 3, 1);
}

void VfoWidget::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    emit sliceActivationRequested(m_sliceIndex);

    // Double-click on frequency area → enter edit mode. Task 14a: not on a
    // slice another device controls.
    if (event->type() == QEvent::MouseButtonDblClick && !isListening()) {
        QRect freqRect = m_freqStack->geometry();
        if (freqRect.contains(event->pos())) {
            // Format current frequency as MHz for editing
            double mhz = m_frequency / 1e6;
            m_freqEdit->setText(QString::number(mhz, 'f', 6));
            m_freqEdit->selectAll();
            m_freqStack->setCurrentIndex(1);
            m_freqEdit->setFocus();
        }
    }
}

void VfoWidget::wheelEvent(QWheelEvent* event)
{
    event->accept();
    if (m_locked || isListening()) {
        return;
    }
    int delta = event->angleDelta().y();
    if (delta == 0) {
        return;
    }
    int steps = (delta > 0) ? 1 : -1;
    double newFreq = m_frequency + steps * m_stepHz;
    newFreq = std::clamp(newFreq, 100000.0, 61440000.0);

    if (!qFuzzyCompare(newFreq, m_frequency)) {
        m_frequency = newFreq;
        updateFreqLabel();
        emit frequencyChanged(newFreq);
    }
}

// Phase 3F Sub-Epic E Task 4: right-click context menu.
// Per docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 4. Antenna submenu is stubbed; AntennaPickerMenu (Task 5) lands
// the SKU-aware antenna list with chain-consequence hints. Diversity
// is greyed pending Sub-Epic G enable on Slice A + 2-ADC SKUs. Filter
// policy currently routes through chainIndex=0; once slice-to-chain
// mapping is exposed on VfoWidget, switch to the real chainIndex.
void VfoWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    populateContextMenu(menu);
    menu.setStyleSheet(QString::fromLatin1(kPopupMenu));   // Phase 3P-I-a T15 — issue #98
    menu.exec(event->globalPos());
}

void VfoWidget::populateContextMenu(QMenu& menu)
{
    // Task 14a: the access actions come first. While a request waits for
    // the Core they are shown disabled with what the flag is waiting for.
    const bool listening = isListening();
    const auto addAccessAction = [this, &menu](const QString& text,
                                               void (VfoWidget::*signal)(int)) {
        QAction* act = menu.addAction(text);
        if (!m_accessPending.isEmpty()) {
            act->setEnabled(false);
            act->setToolTip(m_accessPending);
        }
        connect(act, &QAction::triggered, this, [this, signal]() {
            if (!m_accessPending.isEmpty()) { return; }
            emit (this->*signal)(m_sliceIndex);
        });
    };
    if (listening) {
        addAccessAction(tr("Take control"), &VfoWidget::takeControlRequested);
        // Core-slice take-over: off with the Core's words when it refuses.
        if (!m_sliceAccess.takeHeldReason.isEmpty() && m_accessPending.isEmpty()) {
            QAction* take = menu.actions().constLast();
            take->setEnabled(false);
            take->setToolTip(m_sliceAccess.takeHeldReason);
        }
        addAccessAction(tr("Stop listening"), &VfoWidget::stopListeningRequested);
        menu.addSeparator();
    } else if (m_sliceAccess.state == SliceAccess::State::Controlled) {
        addAccessAction(tr("Release"), &VfoWidget::releaseRequested);
        menu.addSeparator();
    }
    const int firstSharedAction = menu.actions().size();

    // Make this the TX slice
    QAction* makeTxAct = menu.addAction(QStringLiteral("Make this the TX slice"));
    if (!m_transmitPermitted) {
        makeTxAct->setEnabled(false);
        makeTxAct->setToolTip(m_transmitPermissionReason);
    }
    connect(makeTxAct, &QAction::triggered, this, [this]() {
        if (!m_transmitPermitted || isListening()) { return; }
        emit txHandoffRequested(m_sliceIndex);
    });

    menu.addSeparator();

    // Phase 3F closeout — AntennaPickerMenu integration (Sub-Epic E Task 5
    // consumer wire-up). The picker derives the live slice band, lists
    // ANT1/ANT2/ANT3 limited by BoardCapabilities::antennaInputCount, marks
    // the current antenna checked, and emits antennaSelected on user pick.
    // We forward to antennaChangeRequested; MainWindow routes to
    // SliceModel::setRxAntenna. Falls back to a stub ANT1/ANT2 submenu when
    // no RadioModel / slice is wired (e.g. test contexts).
    bool builtPicker = false;
    if (m_radioModel) {
        if (SliceModel* slice = contextMenuSliceForTest()) {
            AlexController* alex = &m_radioModel->alexControllerMutable();
            const BoardCapabilities& caps = m_radioModel->boardCapabilities();
            auto* picker = new AntennaPickerMenu(slice, alex, caps, &menu);
            picker->setTitle(QStringLiteral("Antenna >"));
            menu.addMenu(picker);
            connect(picker, &AntennaPickerMenu::antennaSelected, this,
                    [this](int sliceIdx, const QString& antName) {
                emit antennaChangeRequested(sliceIdx, antName);
            });
            builtPicker = true;
        }
    }
    if (!builtPicker) {
        QMenu* antMenu = menu.addMenu(QStringLiteral("Antenna >"));
        antMenu->addAction(QStringLiteral("ANT1"));
        antMenu->addAction(QStringLiteral("ANT2"));
    }

    // ── Sample rate submenu ─────────────────────────────────────────────
    // Phase 3F Sub-Epic I closeout, defect G2. The rate is a property of the
    // DDC stream this slice is hosted on, not of the slice, so co-hosted
    // slices share it. On Protocol 1 one rate covers the WHOLE radio (the
    // rate is srBits in C&C bank 0), so say so in the title rather than let a
    // per-slice context menu imply a private rate.
    const bool rateIsRadioWide =
        m_radioModel != nullptr && m_radioModel->sampleRateIsRadioWide();
    QMenu* rateMenu = menu.addMenu(
        rateIsRadioWide ? QStringLiteral("Sample rate (whole radio) >")
                        : QStringLiteral("Sample rate >"));

    // Offer only what the connected board accepts. P1 saturates srBits at 3
    // for anything >= 384 kHz, so a P2-only entry picked on a P1 radio would
    // leave the client configured for a width the radio is not sending.
    // Disconnected: fall back to the full P2 ladder so the menu is not empty.
    QVector<int> rates =
        m_radioModel ? m_radioModel->allowedStreamSampleRates() : QVector<int>{};
    if (rates.isEmpty()) {
        rates = {48000, 96000, 192000, 384000, 768000, 1536000};
    }

    // Check the rate the stream actually resolved to. SliceModel::sampleRateHz
    // is RadioModel's mirror of that (see its Q_PROPERTY doc), so co-hosted
    // flags agree and the checkmark cannot show a stale per-slice wish.
    int resolvedRateHz = 0;
    if (m_radioModel != nullptr) {
        if (SliceModel* s = m_radioModel->sliceById(m_sliceIndex)) {
            resolvedRateHz = s->sampleRateHz();
        }
    }

    for (int hz : rates) {
        QAction* act = rateMenu->addAction(QStringLiteral("%1 kHz").arg(hz / 1000));
        act->setCheckable(true);
        act->setChecked(hz == resolvedRateHz);
        connect(act, &QAction::triggered, this, [this, hz]() {
            emit sampleRateRequested(m_sliceIndex, hz);
        });
    }

    menu.addSeparator();

    // R-R3-21: opens the Diversity dialog (Tools > Diversity), which holds
    // the diversity controls. It used to be a greyed placeholder.
    QAction* divAct = menu.addAction(QStringLiteral("Diversity..."));
    connect(divAct, &QAction::triggered, this, [this]() {
        emit diversityRequested();
    });

    // Filter policy (opens FilterPolicyDialog via chainIndex=0 default).
    QAction* filterAct = menu.addAction(QStringLiteral("Filter policy..."));
    connect(filterAct, &QAction::triggered, this, [this]() {
        emit filterPolicyRequested(0);
    });

    menu.addSeparator();

    QAction* removeAct = menu.addAction(QStringLiteral("Remove slice"));
    connect(removeAct, &QAction::triggered, this, [this]() {
        emit removeSliceRequested(m_sliceIndex);
    });

    // Task 14a: every entry below the access actions changes the shared
    // slice (or the radio for it), so on a listened flag each is shown
    // disabled with who controls the slice.
    if (listening) {
        const QList<QAction*> actions = menu.actions();
        for (int i = firstSharedAction; i < actions.size(); ++i) {
            QAction* act = actions.at(i);
            if (act->isSeparator()) { continue; }
            act->setEnabled(false);
            act->setToolTip(m_sliceAccess.heldReason);
        }
    }
    menu.setToolTipsVisible(true);
}

// ---- Helpers ----

double VfoWidget::parseUserFrequency(const QString& raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) { return -1.0; }

    // Detect and strip unit suffix (longest match first so "MHz" wins over "Hz").
    double mult = 0.0;
    bool hasUnit = false;
    const auto tryStripSuffix = [&](const char* suffix, double m) {
        if (s.endsWith(QLatin1String(suffix), Qt::CaseInsensitive)) {
            s.chop(qstrlen(suffix));
            mult = m;
            hasUnit = true;
            return true;
        }
        return false;
    };
    tryStripSuffix("MHz", 1e6)
        || tryStripSuffix("kHz", 1e3)
        || tryStripSuffix("Hz",  1.0)
        || tryStripSuffix("M",   1e6)
        || tryStripSuffix("K",   1e3);
    s = s.trimmed();
    if (s.isEmpty()) { return -1.0; }

    const int nDots   = s.count(QLatin1Char('.'));
    const int nCommas = s.count(QLatin1Char(','));

    // Normalize separators. The goal: end up with at most one '.' as the
    // decimal separator, with any grouping separators removed.
    if (nDots >= 2 && nCommas == 0) {
        // "7.230.000" (or with unit: "7.230.000 Hz") — dots are thousand
        // separators. When no unit was given, default to Hz since that's
        // the only sensible interpretation of a multi-dot number.
        s.remove(QLatin1Char('.'));
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas >= 2 && nDots == 0) {
        // "7,230,000" — US thousand-separated Hz value.
        s.remove(QLatin1Char(','));
        if (!hasUnit) { mult = 1.0; }
    } else if (nDots > 0 && nCommas > 0) {
        // Mixed: the last occurrence is the decimal, the rest are thousands.
        // The presence of thousand separators means the user is writing a
        // Hz value (e.g. "7,230,000.50"); no unit makes no other sense.
        if (s.lastIndexOf(QLatin1Char('.')) > s.lastIndexOf(QLatin1Char(','))) {
            s.remove(QLatin1Char(','));
        } else {
            s.remove(QLatin1Char('.'));
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas == 1 && nDots == 0) {
        // Single comma — ambiguous. If a unit suffix was already parsed, a
        // three-digit tail is a US-style thousands separator
        // (e.g. "7,230 kHz" → 7,230 kHz), anything else is EU decimal
        // ("7,23 MHz" → 7.23 MHz). Without a unit, fall through to EU
        // decimal — the historical behavior — because a bare "7,23" with
        // no grouping context reads as a decimal in every locale that
        // writes it that way.
        const int commaIdx  = s.indexOf(QLatin1Char(','));
        const int tailCount = s.size() - commaIdx - 1;
        if (hasUnit && tailCount == 3) {
            s.remove(QLatin1Char(','));
        } else {
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
    }
    // else: at most a single '.' (C-locale ready) or a plain integer.

    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || v < 0.0) { return -1.0; }

    if (mult != 0.0) {
        return v * mult;
    }

    // Plain number, no unit, no grouping separators. Matches the Thetis
    // MHz-decimal convention when a decimal point is present. For bare
    // integers, pick the first unit (MHz → kHz → Hz) whose interpretation
    // lies in the Red Pitaya tuning range — this rescues users who typed
    // "7230" (intending kHz) or "7230000" (intending Hz) without guessing
    // wrong like the prior heuristic did (issue #73).
    constexpr double kMinHz = 100000.0;     // 100 kHz floor
    constexpr double kMaxHz = 61440000.0;   // 61.44 MHz ceiling
    const bool isDecimal = (nDots == 1);
    if (isDecimal) {
        return v * 1e6;  // Thetis convention: decimal number is MHz
    }
    const double asMHz = v * 1e6;
    const double asKHz = v * 1e3;
    const double asHz  = v;
    if (asMHz >= kMinHz && asMHz <= kMaxHz) { return asMHz; }
    if (asKHz >= kMinHz && asKHz <= kMaxHz) { return asKHz; }
    if (asHz  >= kMinHz && asHz  <= kMaxHz) { return asHz;  }
    // No interpretation in range: fall back to MHz; caller will clamp.
    return asMHz;
}

void VfoWidget::updateFreqLabel()
{
    // Format: "14.225.000" (MHz with period separators every 3 digits after decimal)
    // From Thetis txtVFOAFreq format: freq.ToString("f6")
    double mhz = m_frequency / 1e6;
    int intPart = static_cast<int>(mhz);
    int fracPart = static_cast<int>(std::round((mhz - intPart) * 1e6));
    int khz = fracPart / 1000;
    int hz = fracPart % 1000;

    m_freqLabel->setText(QStringLiteral("%1.%2.%3")
        .arg(intPart)
        .arg(khz, 3, 10, QLatin1Char('0'))
        .arg(hz, 3, 10, QLatin1Char('0')));

    // Also update filter width display
    // (will be properly synced from setFilter)
}

QString VfoWidget::formatFilterWidth(int low, int high) const
{
    int width = std::abs(high - low);
    if (width >= 1000) {
        return QStringLiteral("%1K").arg(width / 1000.0, 0, 'f', 1);
    }
    return QString::number(width);
}

QColor VfoWidget::sliceColor(int index)
{
    // From AetherSDR SliceColors.h (the table is ControlRanges.h's
    // kSliceColours, which the Core's catalogue reads too).
    // From AetherSDR src/gui/SliceColors.h:5, 16-23 [@0cd4559]:
    // index all eight bright entries by slice id % 8.
    return QColor(static_cast<QRgb>(ControlRanges::sliceColour(index % kSliceColorCount)));
}

QColor VfoWidget::sliceDimColor(int index)
{
    // From AetherSDR src/gui/SliceColors.h:5, 16-23 [@0cd4559]: the dim half
    // (dr, dg, db) of all eight kSliceColors entries, indexed by slice id % 8
    // as there. Current AetherSDR carries the same eight values as
    // color.slice.dim.a-h in resources/themes/default-dark.json:227-234
    // [@9f81dc00].
    switch (index % kSliceColorCount) {
    case 0: return QColor(0x00, 0x60, 0x80);  // A = cyan
    case 1: return QColor(0x80, 0x20, 0x80);  // B = magenta
    case 2: return QColor(0x20, 0x80, 0x20);  // C = green
    case 3: return QColor(0x80, 0x80, 0x00);  // D = yellow
    case 4: return QColor(0x80, 0x50, 0x00);  // E = orange
    case 5: return QColor(0x00, 0x70, 0x60);  // F = teal
    case 6: return QColor(0x80, 0x30, 0x40);  // G = coral
    case 7: return QColor(0x58, 0x40, 0x80);  // H = lavender
    default: return QColor(0x00, 0x60, 0x80);
    }
}

// Phase 3P-I-a T15 — gate RX/TX ANT buttons on Alex presence and antenna count.
// Spec: docs/architecture/antenna-routing-design.md §6.1 Rule 2 + Rule 4.
void VfoWidget::setBoardCapabilities(const BoardCapabilities& caps)
{
    m_hasAlex = caps.hasAlex;
    m_hasRxBypassRelay = caps.hasRxBypassRelay;
    const bool showAnt = caps.hasAlex && caps.antennaInputCount >= 3;
    if (m_rxAntBtn) { m_rxAntBtn->setVisible(showAnt); }
    if (m_txAntBtn) { m_txAntBtn->setVisible(showAnt); }
    if (m_rxBypassBtn) { m_rxBypassBtn->setVisible(m_hasRxBypassRelay && m_hasRxOutOnTxUi); }

    // B3: store for AntennaPopupBuilder in popup lambdas.
    m_popupCaps = caps;
}

// Phase 3P-I-b T9 — per-SKU BYPS button gate. Called by MainWindow on
// RadioModel::currentRadioChanged alongside setBoardCapabilities.
//
// The button toggles AlexController::rxOutOnTx (Thetis Alex.cs:61
// chkRxOutOnTx), so gate on profile.hasRxOutOnTx — not hasRxBypassUi,
// which is the separate chkDisableRXOut (Alex.cs:65 rx_out_override)
// "Disable RX Bypass relay" override.
// From Thetis setup.cs:6268-6273 [v2.10.3.13] — chkRxOutOnTx visibility per HPSDRModel.
// G8NJJ. will need more work ofr high power PA  [original inline comment from setup.cs:6277, ANAN_G2_1K branch of the same per-SKU switch]
void VfoWidget::setHpsdrSku(HPSDRModel sku)
{
    const SkuUiProfile profile = skuUiProfileFor(sku);
    m_hasRxOutOnTxUi = profile.hasRxOutOnTx;
    if (m_rxBypassBtn) { m_rxBypassBtn->setVisible(m_hasRxBypassRelay && m_hasRxOutOnTxUi); }

    // B3: store for AntennaPopupBuilder in popup lambdas.
    m_popupSku = profile;
}

// Phase 3P-I-b T9 — reflect AlexController::rxOutOnTx into the BYPS button.
// Guard against signal re-emission so model → UI sync doesn't loop back.
void VfoWidget::setRxBypassActive(bool on)
{
    if (!m_rxBypassBtn) { return; }
    if (m_rxBypassBtn->isChecked() == on) { return; }
    const bool prev = m_updatingFromModel;
    m_updatingFromModel = true;
    m_rxBypassBtn->setChecked(on);
    m_updatingFromModel = prev;
}

// Phase 3F closeout — AntennaPickerMenu requires live slice + AlexController +
// BoardCapabilities access. Storing the RadioModel pointer here lets the
// contextMenuEvent build a real picker instead of the stub fallback. Pointer
// is non-owning; lifetime is RadioModel-owned and MainWindow-scoped.
void VfoWidget::setRadioModel(RadioModel* model)
{
    for (QMetaObject::Connection& conn : m_nrAvailabilityConns) {
        if (conn) {
            disconnect(conn);
            conn = {};
        }
    }
    m_radioModel = model;
    // R-R3-49, Sub-epic C-1: DFNR and MNR are enabled only while the Core
    // can run them.
    if (model && model->dspAssets()) {
        m_nrAvailabilityConns[0] = connect(model->dspAssets(),
                                           &DspAssetService::dfnrAvailabilityChanged,
                                           this, &VfoWidget::updateNrAvailability);
        m_nrAvailabilityConns[1] = connect(model->dspAssets(),
                                           &DspAssetService::mnrAvailabilityChanged,
                                           this, &VfoWidget::updateNrAvailability);
        // And whether an older Core says at all (dspAssetVersion).
        m_nrAvailabilityConns[2] = connect(model, &RadioModel::nrAvailabilityChanged,
                                           this, &VfoWidget::updateNrAvailability);
    }
    updateNrAvailability();
    if (model && model->role() == RadioModel::Role::Remote) {
        setTransmitPermitted(false);
        // Group B fix wave: until MainWindow hears the Core takes it.
        setRxBypassPermitted(false, QString());
        // R-R3-44: the VAX selector stays live. In a remote window it picks
        // this computer's VAX channel for the Core's slice; the remote model
        // keeps the choice on this computer (RadioModel::
        // setRemoteVaxChannelStore) and RemoteVaxRouter feeds the channel
        // from the Core's receiver stream.
    }
}

QString VfoWidget::nrCannotRunReason(NereusSDR::NrSlot slot) const
{
    // With a model it says (the Core's word, mirrored, in a remote window);
    // without one, this build decides.
    return m_radioModel ? m_radioModel->nrCannotRunReason(slot)
                        : RadioModel::nrCannotRunInThisBuildReason(slot);
}

void VfoWidget::updateNrAvailability()
{
    // Shown always; disabled with the plain reason while it cannot run. A
    // slice holding one is turned off by the Core (RadioModel::
    // turnOffDfnrWithoutModel, turnOffNrThatCannotRun).
    const struct {
        QPushButton* button;
        NereusSDR::NrSlot slot;
        const QString* ownTip;
    } filters[] = {
        {m_dfnrBtn, NereusSDR::NrSlot::DFNR, &m_dfnrToolTip},
        {m_mnrBtn, NereusSDR::NrSlot::MNR, &m_mnrToolTip},
    };
    for (const auto& f : filters) {
        if (!f.button) {
            continue;
        }
        const QString reason = nrCannotRunReason(f.slot);
        f.button->setEnabled(reason.isEmpty());
        f.button->setToolTip(reason.isEmpty() ? *f.ownTip : reason);
        f.button->setAccessibleDescription(reason);
    }
}

void VfoWidget::setRxBypassPermitted(bool permitted, const QString& reason)
{
    // Group B fix wave: BYPS (RX bypass on TX) writes the Core's
    // AlexController through `alexAntennas` in a remote window; disabled
    // with the reason when the Core does not take it.
    m_rxBypassPermitted = permitted;
    if (!m_rxBypassBtn) { return; }
    m_rxBypassBtn->setEnabled(permitted);
    const QString tip = permitted ? rxBypassToolTip()
        : (reason.isEmpty()
               ? tr("Connect to the Core to change the radio's hardware settings.")
               : reason);
    m_rxBypassBtn->setToolTip(tip);
    m_rxBypassBtn->setAccessibleDescription(permitted ? QString() : tip);
}

void VfoWidget::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_transmitPermitted = permitted;
    m_transmitPermissionReason = reason.isEmpty()
        ? tr("Transmit controls are unavailable until the Core confirms transmit permission.")
        : reason;
    updateTransmitControlAvailability();
}

void VfoWidget::updateTransmitControlAvailability()
{
    const auto apply = [this](QWidget* control) {
        if (!control) { return; }
        static constexpr auto kSavedTooltip = kSavedTransmitTooltip;
        static constexpr auto kSavedDescription = kSavedTransmitDescription;
        static constexpr auto kSavedEnabled = "VfoSavedTransmitEnabled";
        // TX badge take (JJ, 2026-09-30): a badge that offers a take is
        // enabled and says what a click will do.
        const bool offered = txBadgeTakeOffered();
        // Task 14a: a listened slice holds the TX badge too; the checked
        // (red on the air) state is left alone so it still shows.
        const bool held = !offered && (!m_transmitPermitted || isListening());
        QString reason = !m_transmitPermitted
            ? m_transmitPermissionReason : m_sliceAccess.heldReason;
        if (!m_txBadgeOffer.heldReason.isEmpty()) {
            reason = m_txBadgeOffer.heldReason;
        } else if (m_txBadgeOffer.offered && !m_accessPending.isEmpty()) {
            reason = m_accessPending;
        } else if (m_txBadgeOffer.offered && m_inUseByRadio) {
            reason = inUseByRadioText();
        }
        if (held || offered) {
            if (!control->property(kSavedTooltip).isValid()) {
                control->setProperty(kSavedTooltip, control->toolTip());
                control->setProperty(kSavedDescription, control->accessibleDescription());
                control->setProperty(kSavedEnabled, control->isEnabled());
            }
            const QString words = offered ? m_txBadgeOffer.toolTip : reason;
            control->setEnabled(offered);
            control->setToolTip(words);
            control->setAccessibleDescription(words);
            return;
        }
        if (control->property(kSavedTooltip).isValid()) {
            control->setEnabled(control->property(kSavedEnabled).toBool());
            control->setToolTip(control->property(kSavedTooltip).toString());
            control->setAccessibleDescription(control->property(kSavedDescription).toString());
            control->setProperty(kSavedTooltip, QVariant());
            control->setProperty(kSavedDescription, QVariant());
            control->setProperty(kSavedEnabled, QVariant());
        }
    };

    // R-R3-49 (parity Task 11): XIT is not here; it writes the slice.
    // Group B fix wave: nor is BYPS (setRxBypassPermitted).
    apply(m_txBadge);
}

// ---- Task 14a: slice access ----

void VfoWidget::setSliceAccess(const SliceAccess& access)
{
    if (m_sliceAccess == access) { return; }
    m_sliceAccess = access;
    applySliceAccess();
}

void VfoWidget::setSliceAccessPending(const QString& text)
{
    if (m_accessPending == text) { return; }
    m_accessPending = text;
    applySliceAccess();
}

QString VfoWidget::accessLineText() const
{
    return m_accessPending.isEmpty() ? m_sliceAccess.line : m_accessPending;
}

QList<QWidget*> VfoWidget::heldControlsForTest() const
{
    QList<QWidget*> controls = listeningHeldControls();
    if (m_txBadge) { controls.append(m_txBadge); }
    return controls;
}

QString VfoWidget::afNameForTest() const
{
    return m_afNameLabel ? m_afNameLabel->text() : QString();
}

QList<QWidget*> VfoWidget::listeningHeldControls() const
{
    // The shared tuning controls. BYPS is not here: it is the radio's
    // hardware, gated by setRxBypassPermitted. Task 14b: every tab page is
    // held but the audio page, where only the AF slider (with its name and
    // value) and Mute stay live, as this device's own volume and mute.
    QList<QWidget*> controls;
    for (QWidget* control : {static_cast<QWidget*>(m_rxAntBtn),
                             static_cast<QWidget*>(m_txAntBtn),
                             static_cast<QWidget*>(m_freqStack),
                             static_cast<QWidget*>(m_lockBtn)}) {
        if (control) { controls.append(control); }
    }
    if (!m_tabStack) { return controls; }
    for (int i = 0; i < m_tabStack->count(); ++i) {
        QWidget* page = m_tabStack->widget(i);
        if (page != m_audioPage) {
            controls.append(page);
            continue;
        }
        const QList<QWidget*> children =
            page->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* child : children) {
            if (child == m_afNameLabel || child == m_afGainSlider
                || child == m_afGainLabel || child == m_muteBtn) {
                continue;
            }
            controls.append(child);
        }
    }
    return controls;
}

QRect VfoWidget::frequencyAreaForTest() const
{
    return m_freqStack ? m_freqStack->geometry() : QRect();
}

bool VfoWidget::frequencyEditOpen() const
{
    return m_freqStack && m_freqStack->currentIndex() == 1;
}

void VfoWidget::holdForListening(QWidget* control) const
{
    if (!control) { return; }
    static constexpr auto kSavedTooltip = "VfoSavedAccessTooltip";
    static constexpr auto kSavedDescription = "VfoSavedAccessDescription";
    static constexpr auto kSavedEnabled = "VfoSavedAccessEnabled";
    if (isListening()) {
        if (!control->property(kSavedTooltip).isValid()) {
            control->setProperty(kSavedTooltip, control->toolTip());
            control->setProperty(kSavedDescription, control->accessibleDescription());
            control->setProperty(kSavedEnabled, control->isEnabled());
        }
        control->setEnabled(false);
        control->setToolTip(m_sliceAccess.heldReason);
        control->setAccessibleDescription(m_sliceAccess.heldReason);
        return;
    }
    if (control->property(kSavedTooltip).isValid()) {
        control->setEnabled(control->property(kSavedEnabled).toBool());
        control->setToolTip(control->property(kSavedTooltip).toString());
        control->setAccessibleDescription(control->property(kSavedDescription).toString());
        control->setProperty(kSavedTooltip, QVariant());
        control->setProperty(kSavedDescription, QVariant());
        control->setProperty(kSavedEnabled, QVariant());
    }
}

void VfoWidget::applySliceAccess()
{
    const bool listening = isListening();

    // A frequency typed before the access changed is abandoned rather than
    // sent for a slice this window no longer controls.
    if (listening && m_freqStack && m_freqStack->currentIndex() == 1) {
        m_freqStack->setCurrentIndex(0);
    }

    for (QWidget* control : listeningHeldControls()) {
        holdForListening(control);
    }
    updateTransmitControlAvailability();
    applyAudioBinding();

    if (m_accessLine) {
        const QString text = accessLineText();
        m_accessLine->setText(text);
        const bool shown = !text.isEmpty();
        if (m_accessLine->isVisibleTo(this) != shown) {
            m_accessLine->setVisible(shown);
            adjustSize();
        }
    }

    if (m_closeBtn) {
        m_closeBtn->setToolTip(listening ? tr("Stop listening")
                                         : QStringLiteral("Close slice"));
        positionFloatingButtons();
    }
}

void VfoWidget::setListenVolume(int level, bool muted)
{
    m_listenVolume = std::clamp(level, 0, 100);
    m_listenMuted = muted;
    if (isListening()) {
        applyAudioBinding();
    }
}

void VfoWidget::applyAudioBinding()
{
    // Task 14b (ruling U5): a listened flag's slider and Mute are this
    // device's own volume and mute; any other flag's are the slice's AF
    // and mute, as before.
    if (!m_afGainSlider || !m_muteBtn) { return; }
    const bool listening = isListening();
    const int value = listening ? m_listenVolume : m_modelAfGain;
    const bool muted = listening ? m_listenMuted : m_modelMuted;
    const bool wasUpdating = m_updatingFromModel;
    m_updatingFromModel = true;
    m_afGainSlider->setValue(value);
    m_afGainLabel->setText(QString::number(value));
    m_muteBtn->setChecked(muted);
    m_updatingFromModel = wasUpdating;

    if (m_afNameLabel) {
        if (listening) {
            m_afNameLabel->setText(tr("Your volume"));
            m_afNameLabel->setMinimumWidth(24);
            m_afNameLabel->setMaximumWidth(QWIDGETSIZE_MAX);
        } else {
            m_afNameLabel->setText(QStringLiteral("AF"));
            m_afNameLabel->setFixedWidth(24);
        }
    }
    m_afGainSlider->setToolTip(listening
        ? tr("How loud you hear this slice on this device. 0 to 100. "
             "The controller and other listeners do not hear the change.")
        : m_afToolTip);
    m_muteBtn->setToolTip(listening
        ? tr("Mute this slice on this device only.")
        : m_muteToolTip);
}

SliceModel* VfoWidget::contextMenuSliceForTest() const
{
    return m_radioModel ? m_radioModel->sliceById(m_sliceIndex) : nullptr;
}

void VfoWidget::showTab(Tab tab)
{
    const int index = static_cast<int>(tab);
    if (index < 0 || index >= m_tabButtons.size() || m_activeTab == index) {
        return;
    }
    // The tab button's own handler opens the page and resizes the flag.
    m_tabButtons[index]->click();
}

// --- Task 3.4: Small filter display mode (Appearance > Meter Styles) ---

void VfoWidget::setSmallFilterMode(bool small)
{
    if (m_smallFilterMode == small) { return; }
    m_smallFilterMode = small;
    update();   // trigger repaint to apply visual changes
}

// ---- Sub-epic C-1: NR bank DspParamPopup builders (Task 15) ----
// Each popup shows the 3-5 most-adjusted knobs for the given NR slot.
// "More Settings…" fires openNrSetupRequested(slot) routed by MainWindow in Task 18.
// Ranges, defaults and Reset values come from ControlRanges.h, where each
// keeps its source (Thetis, AetherSDR or NereusSDR's own); the Core's
// catalogue sends the same table.

void VfoWidget::showNr1Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR1 (ANR — Adaptive LMS). Thetis's NR spinbox ranges and defaults and
    // its SetRXAANRVals conversion (gain x 1e-6, leak x 1e-3), from
    // ControlRanges.h. Gain and leak are stored in the WDSP domain.
    using namespace ControlRanges;
    addNrSlider(p, kNr1Taps, m_slice->nr1Taps(),
                [this](double v) { if (m_slice) m_slice->setNr1Taps(static_cast<int>(std::lround(v))); });
    addNrSlider(p, kNr1Delay, m_slice->nr1Delay(),
                [this](double v) { if (m_slice) m_slice->setNr1Delay(static_cast<int>(std::lround(v))); });
    addNrSlider(p, kNr1Gain, m_slice->nr1Gain(),
                [this](double v) { if (m_slice) m_slice->setNr1Gain(v); });
    addNrSlider(p, kNr1Leak, m_slice->nr1Leakage(),
                [this](double v) { if (m_slice) m_slice->setNr1Leakage(v); });
    p->addRadioGroup(QString::fromUtf8(kNr1Position.label), nrOptionLabels(kNr1Position),
                     static_cast<int>(m_slice->nr1Position()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr1Position(static_cast<NereusSDR::NrPosition>(v));
                     });
    p->finalize([this]() { requestNrSetup(NereusSDR::NrSlot::NR1); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showNr2Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR2 (EMNR, Enhanced Multiband Noise Reduction). Thetis's labels,
    // choices and defaults, from ControlRanges.h.
    using namespace ControlRanges;
    p->addRadioGroup(QString::fromUtf8(kNr2GainMethod.label), nrOptionLabels(kNr2GainMethod),
                     static_cast<int>(m_slice->nr2GainMethod()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr2GainMethod(static_cast<NereusSDR::EmnrGainMethod>(v));
                     });
    p->addRadioGroup(QString::fromUtf8(kNr2NpeMethod.label), nrOptionLabels(kNr2NpeMethod),
                     static_cast<int>(m_slice->nr2NpeMethod()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr2NpeMethod(static_cast<NereusSDR::EmnrNpeMethod>(v));
                     });
    p->addCheckbox(QString::fromUtf8(kNr2AeFilter.label), m_slice->nr2AeFilter(),
                   [this](bool v) { if (m_slice) m_slice->setNr2AeFilter(v); });
    p->addCheckbox(QString::fromUtf8(kNr2Post2Run.label), m_slice->nr2Post2Run(),
                   [this](bool v) { if (m_slice) m_slice->setNr2Post2Run(v); });
    addNrSlider(p, kNr2Post2Factor, m_slice->nr2Post2Factor(),
                [this](double v) { if (m_slice) m_slice->setNr2Post2Factor(v); });
    addNrSlider(p, kNr2Post2Rate, m_slice->nr2Post2Rate(),
                [this](double v) { if (m_slice) m_slice->setNr2Post2Rate(v); });
    p->finalize([this]() { requestNrSetup(NereusSDR::NrSlot::NR2); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showNr3Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR3 (RNNR, Recurrent Neural Net NR). Thetis's position and fixed
    // input gain, from ControlRanges.h.
    using namespace ControlRanges;
    p->addRadioGroup(QString::fromUtf8(kNr3Position.label), nrOptionLabels(kNr3Position),
                     static_cast<int>(m_slice->nr3Position()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr3Position(static_cast<NereusSDR::NrPosition>(v));
                     });
    p->addCheckbox(QString::fromUtf8(kNr3UseDefaultGain.label), m_slice->nr3UseDefaultGain(),
                   [this](bool v) { if (m_slice) m_slice->setNr3UseDefaultGain(v); });
    // "Load Model…" opens Setup NR3 page where file dialog lives (Task 17).
    p->finalize([this]() { requestNrSetup(NereusSDR::NrSlot::NR3); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showNr4Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR4 (SBNR, Spectral Baseline NR). Thetis's labels; the ranges and
    // defaults ControlRanges.h holds (NereusSDR's where they differ).
    using namespace ControlRanges;
    addNrSlider(p, kNr4Reduction, m_slice->nr4Reduction(),
                [this](double v) { if (m_slice) m_slice->setNr4Reduction(v); });
    addNrSlider(p, kNr4Smoothing, m_slice->nr4Smoothing(),
                [this](double v) { if (m_slice) m_slice->setNr4Smoothing(v); });
    addNrSlider(p, kNr4Whitening, m_slice->nr4Whitening(),
                [this](double v) { if (m_slice) m_slice->setNr4Whitening(v); });
    addNrSlider(p, kNr4Rescale, m_slice->nr4Rescale(),
                [this](double v) { if (m_slice) m_slice->setNr4Rescale(v); });
    addNrSlider(p, kNr4PostThresh, m_slice->nr4PostThresh(),
                [this](double v) { if (m_slice) m_slice->setNr4PostThresh(v); });
    p->addRadioGroup(QString::fromUtf8(kNr4Algo.label), nrOptionLabels(kNr4Algo),
                     static_cast<int>(m_slice->nr4Algo()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr4Algo(static_cast<NereusSDR::SbnrAlgo>(v));
                     });
    p->finalize([this]() { requestNrSetup(NereusSDR::NrSlot::NR4); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showDfnrPopup(const QPoint& globalPos)
{
    // R-R3-49: no quick controls for a DFNR that cannot run.
    if (!m_slice || !nrCannotRunReason(NereusSDR::NrSlot::DFNR).isEmpty()) { return; }
    auto* p = new DspParamPopup(this);

    // DFNR (DeepFilterNet3), a post-WDSP filter that is not in Thetis. Its
    // ranges and Reset (AetherSDR's defaults) from ControlRanges.h.
    using namespace ControlRanges;
    addNrSlider(p, kDfnrAttenLimit, m_slice->dfnrAttenLimit(),
                [this](double v) { if (m_slice) m_slice->setDfnrAttenLimit(v); },
                tr("Maximum noise attenuation in dB (0 = bypass, 100 = maximum). "
                   "Default 100. Higher values suppress more noise but may clip speech peaks."));

    addNrSlider(p, kDfnrPostFilterBeta, m_slice->dfnrPostFilterBeta(),
                [this](double v) { if (m_slice) m_slice->setDfnrPostFilterBeta(v); },
                tr("Post-filter aggressiveness (0 = disabled, 0.30+ = aggressive). "
                   "Default 0 (off). Higher values reduce "
                   "residual musical-noise artifacts but may over-attenuate "
                   "consonants. Typical tuning: start at 0.05-0.10 and nudge up."));

    p->finalize([this]() { requestNrSetup(NereusSDR::NrSlot::DFNR); },
                /*onReset=*/[]() { /* per-slider resetters push via valueChanged */ });
    p->showAt(globalPos);
}

void VfoWidget::showMnrPopup(const QPoint& globalPos)
{
    // R-R3-49: no quick controls for an MNR the Core cannot run.
    if (!m_slice || !nrCannotRunReason(NereusSDR::NrSlot::MNR).isEmpty()) { return; }
    auto* p = new DspParamPopup(this);

    // MNR (macOS Accelerate MMSE-Wiener NR). 6 runtime-tunable knobs; their
    // ranges and Reset values come from ControlRanges.h, and Reset restores
    // a new slice's values (MacNRFilter's DEF_*).
    using namespace ControlRanges;
    addNrSlider(p, kMnrStrength, m_slice->mnrStrength(),
                [this](double v) { if (m_slice) { m_slice->setMnrStrength(v); } },
                tr("Dry/wet blend.\n"
                   "  0%   = bypass (filter runs but output = input)\n"
                   "  100% = full NR (output = filter result)\n"
                   "  200% = over-drive (phase-flip, destructive)\n"
                   "Default 100."));

    addNrSlider(p, kMnrOversub, m_slice->mnrOversub(),
                [this](double v) { if (m_slice) { m_slice->setMnrOversub(v); } },
                tr("MMSE-Wiener oversubtraction factor. Higher values attenuate "
                   "low-SNR bins more aggressively while leaving high-SNR (voice) "
                   "bins closer to unity.\n"
                   "  1    = very gentle\n"
                   "  4    = noticeable NR (default)\n"
                   "  20+  = underwater/robotic\n"
                   "  200+ = diminishing returns"));

    addNrSlider(p, kMnrFloor, m_slice->mnrFloor(),
                [this](double v) { if (m_slice) { m_slice->setMnrFloor(v); } },
                tr("Minimum Wiener gain per bin (×0.001).\n"
                   "  0    = total silence (filter can zero a bin)\n"
                   "  50   = -26 dB max attenuation (default)\n"
                   "  1000 = 0 dB (bin never attenuated)\n"
                   "  2000 = amplify (destructive)\n"
                   "Lower floor = more aggressive noise subtraction but more "
                   "musical-noise artifacts."));

    addNrSlider(p, kMnrAlpha, m_slice->mnrAlpha(),
                [this](double v) { if (m_slice) { m_slice->setMnrAlpha(v); } },
                tr("Decision-directed smoothing coefficient.\n"
                   "  0.00 = no smoothing (fast/chattery tracking)\n"
                   "  0.92 = Ephraim-Malah classic (default)\n"
                   "  1.00 = frozen (prior SNR never updates)\n"
                   "Balances NR speed vs. musical-noise artifacts."));

    addNrSlider(p, kMnrBias, m_slice->mnrBias(),
                [this](double v) { if (m_slice) { m_slice->setMnrBias(v); } },
                tr("Min-statistics noise-floor bias correction.\n"
                   "  <1.0 = underestimate noise floor (less NR, more signal)\n"
                   "  1.2  = balanced (default)\n"
                   "  >3.0 = overestimate noise floor (more NR, may erode signal)\n"
                   "If NR is too weak, nudge Bias up. If it's eating speech, nudge down."));

    addNrSlider(p, kMnrGsmooth, m_slice->mnrGsmooth(),
                [this](double v) { if (m_slice) { m_slice->setMnrGsmooth(v); } },
                tr("Temporal (per-bin) gain smoothing.\n"
                   "  0.00 = instant (more musical noise, fast transients)\n"
                   "  0.70 = balanced (default)\n"
                   "  1.00 = frozen (gain never updates; the filter is stuck)\n"
                   "Higher = smoother but slower to react to changing noise."));

    // Wire Reset button (finalize's second callback) to restore the
    // factory defaults on every slider. DspParamPopup::finalize runs the
    // per-slider resetters registered by addSlider's /*factory=*/ arg.
    p->finalize([this]() { requestNrSetup(NereusSDR::NrSlot::MNR); },
                /*onReset=*/[]() {
                    // Per-slider resetters registered via addSlider's
                    // factoryDefault arg already push slider → onChange →
                    // SliceModel. This empty callback exists only so the
                    // Reset button renders in the popup footer (finalize
                    // hides it when onReset is null).
                });
    p->showAt(globalPos);
}

void VfoWidget::showNnrPopup(const QPoint& globalPos)
{
    if (!m_slice) { return; }

    auto* popup = new DspParamPopup(this);
    auto* controls = new NnrControls(m_radioModel, m_slice.data(),
                                     NnrControls::Presentation::Compact, popup);
    popup->addWidget(controls);
    connect(controls, &NnrControls::bindingInvalidated, popup, &QWidget::close);
    connect(controls, &NnrControls::openMoreSettingsRequested, popup,
            [this, popup](int) {
                requestNrSetup(NrSlot::NNR);
                popup->close();
            });
    connect(controls, &NnrControls::openModelsRequested, popup,
            [this, popup](int sliceId) {
                emit openNnrModelsRequested(sliceId);
                popup->close();
            });
    popup->showAt(globalPos);
}

void VfoWidget::requestNrSetup(NrSlot slot)
{
    emit openNrSetupRequested(slot);
    emit openNrSetupForSliceRequested(slot, m_slice ? m_slice->sliceIndex() : m_sliceIndex);
}

} // namespace NereusSDR
