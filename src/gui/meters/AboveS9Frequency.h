#pragma once

// =================================================================
// src/gui/meters/AboveS9Frequency.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - Created. The VHF S9 rule (S9 = -93 dBm at or above
//                 30 MHz) as one shared helper, applied per slice by every
//                 S-unit meter item. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
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

#include "MeterPoller.h"

namespace NereusSDR {

// The frequency at and above which S9 is -93 dBm instead of -73 dBm.
// From Thetis MeterManager.cs:358 [v2.10.3.15] (_s9Frequency = 30.0, MHz)
constexpr double kS9FrequencyHz = 30.0e6;

// The dB a signal-strength reading is raised by above the S9 frequency, so
// a scale calibrated with S9 at -73 dBm reads S9 at -93 dBm.
// From Thetis MeterManager.cs:5419-5424 [v2.10.3.15] (dbmOffsetForAboveS9Frequency)
constexpr double kAboveS9OffsetDb = 20.0;

// At or above, as Thetis tests the VFO frequency.
// From Thetis MeterManager.cs:3342, 4508 [v2.10.3.15] (_rx1VHForAbove = ... >= _s9Frequency)
constexpr bool isAboveS9Frequency(double hz)
{
    return hz >= kS9FrequencyHz;
}

// The reading a signal-strength scale positions, for a binding on a slice
// above (or below) the S9 frequency. Only the signal strength readings take
// the offset; every other binding returns dbm unchanged.
// From Thetis MeterManager.cs:41049-41052 [v2.10.3.15] (getPerc)
inline double aboveS9Adjusted(int bindingId, double dbm, bool above)
{
    if (above && (bindingId == MeterBinding::SignalPeak || bindingId == MeterBinding::SignalAvg)) {
        return dbm + kAboveS9OffsetDb;
    }
    return dbm;
}

} // namespace NereusSDR
