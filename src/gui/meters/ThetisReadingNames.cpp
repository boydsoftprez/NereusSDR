// =================================================================
// src/gui/meters/ThetisReadingNames.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - Thetis Reading enum names for data out, LED conditions
//                 and text overlay tokens, mapped from MeterBinding ids.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

#include "ThetisReadingNames.h"
#include "MeterPoller.h"

namespace NereusSDR {

namespace {

struct ReadingNameEntry {
    int bindingId;
    const char* name;
};

// Each name is the Thetis Reading enum entry, lowercased.
constexpr ReadingNameEntry kReadingNames[] = {
    // RX
    { MeterBinding::SignalPeak,     "signal_strength" },     // From Thetis MeterManager.cs:83 [v2.10.3.15]
    { MeterBinding::SignalAvg,      "avg_signal_strength" }, // From Thetis MeterManager.cs:84 [v2.10.3.15]
    { MeterBinding::AdcPeak,        "adc_pk" },              // From Thetis MeterManager.cs:85 [v2.10.3.15]
    { MeterBinding::AdcAvg,         "adc_av" },              // From Thetis MeterManager.cs:86 [v2.10.3.15]
    { MeterBinding::AgcPeak,        "agc_pk" },              // From Thetis MeterManager.cs:87 [v2.10.3.15]
    { MeterBinding::AgcAvg,         "agc_av" },              // From Thetis MeterManager.cs:88 [v2.10.3.15]
    { MeterBinding::AgcGain,        "agc_gain" },            // From Thetis MeterManager.cs:89 [v2.10.3.15]
    { MeterBinding::PbSnr,          "estimated_pbsnr" },     // From Thetis MeterManager.cs:90 [v2.10.3.15]
    { MeterBinding::SignalMaxBin,   "signal_max_bin" },      // From Thetis MeterManager.cs:170 [v2.10.3.15]
    // TX
    { MeterBinding::TxMic,          "mic" },                 // From Thetis MeterManager.cs:92 [v2.10.3.15]
    { MeterBinding::TxPower,        "pwr" },                 // From Thetis MeterManager.cs:93 [v2.10.3.15]
    { MeterBinding::TxAlc,          "alc" },                 // From Thetis MeterManager.cs:94 [v2.10.3.15]
    { MeterBinding::TxEq,           "eq" },                  // From Thetis MeterManager.cs:95 [v2.10.3.15]
    { MeterBinding::TxLeveler,      "leveler" },             // From Thetis MeterManager.cs:96 [v2.10.3.15]
    { MeterBinding::TxComp,         "comp" },                // From Thetis MeterManager.cs:97 [v2.10.3.15]
    { MeterBinding::TxAlcGain,      "alc_g" },               // From Thetis MeterManager.cs:98 [v2.10.3.15]
    { MeterBinding::TxAlcGroup,     "alc_group" },           // From Thetis MeterManager.cs:99 [v2.10.3.15]
    { MeterBinding::TxLevelerGain,  "lvl_g" },               // From Thetis MeterManager.cs:100 [v2.10.3.15]
    { MeterBinding::TxMicPeak,      "mic_pk" },              // From Thetis MeterManager.cs:101 [v2.10.3.15]
    { MeterBinding::TxAlcPeak,      "alc_pk" },              // From Thetis MeterManager.cs:102 [v2.10.3.15]
    { MeterBinding::TxEqPeak,       "eq_pk" },               // From Thetis MeterManager.cs:103 [v2.10.3.15]
    { MeterBinding::TxLevelerPeak,  "leveler_pk" },          // From Thetis MeterManager.cs:104 [v2.10.3.15]
    { MeterBinding::TxCompPeak,     "comp_pk" },             // From Thetis MeterManager.cs:105 [v2.10.3.15]
    { MeterBinding::TxCfcPeak,      "cfc_pk" },              // From Thetis MeterManager.cs:108 [v2.10.3.15]
    { MeterBinding::TxCfc,          "cfc_av" },              // From Thetis MeterManager.cs:109 [v2.10.3.15]
    { MeterBinding::TxCfcGain,      "cfc_g" },               // From Thetis MeterManager.cs:110 [v2.10.3.15]
    { MeterBinding::TxReversePower, "reverse_pwr" },         // From Thetis MeterManager.cs:113 [v2.10.3.15]
    { MeterBinding::TxSwr,          "swr" },                 // From Thetis MeterManager.cs:114 [v2.10.3.15]
    // volts/amps
    { MeterBinding::HwVolts,        "volts" },               // From Thetis MeterManager.cs:128 [v2.10.3.15]
    { MeterBinding::HwAmps,         "amps" },                // From Thetis MeterManager.cs:129 [v2.10.3.15]
    // rotator
    { MeterBinding::RotatorAz,      "az" },                  // From Thetis MeterManager.cs:132 [v2.10.3.15]
    { MeterBinding::RotatorEle,     "ele" },                 // From Thetis MeterManager.cs:133 [v2.10.3.15]
    // MeterBinding::HwTemperature has no Thetis Reading entry.
};

} // namespace

QString thetisReadingName(int bindingId)
{
    for (const ReadingNameEntry& entry : kReadingNames) {
        if (entry.bindingId == bindingId) {
            return QString::fromLatin1(entry.name);
        }
    }
    return {};
}

int bindingForThetisReadingName(const QString& name)
{
    if (name.isEmpty()) {
        return -1;
    }
    for (const ReadingNameEntry& entry : kReadingNames) {
        if (name.compare(QLatin1String(entry.name), Qt::CaseInsensitive) == 0) {
            return entry.bindingId;
        }
    }
    return -1;
}

} // namespace NereusSDR
