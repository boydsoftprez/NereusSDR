// =================================================================
// src/models/bandstack/BandStackSeeds.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsBandStackManager.cs [v2.10.3.15 @3759d09],
//   original licence from Thetis source is included below
//
// The band stack's factory rows for each region: the seven routines and
// the region dispatch from clsBandStackManager.cs. Thetis's CTUN, zoom
// slider and center columns are "false, 150, 0.0" on every row and are not
// carried: a NereusSDR entry keeps its own center and span.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

// --- From clsBandStackManager.cs ---
/*  clsBandStackManager.cs

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

#include "BandStackSeeds.h"

#include "core/LogCategories.h"

#include <span>

namespace NereusSDR {

namespace {

struct RawSeedRow {
    const char* band;
    const char* mode;
    const char* filter;
    double frequencyMhz;
};

// AddRegion1BandStack
constexpr RawSeedRow kRegion1Rows[] = {
    // From Thetis clsBandStackManager.cs:2022-2098 [v2.10.3.15]
    //"160M", "CWL", "F1", 1.805000, false, 150, 0.0,
    {"160M", "CWL", "F1", 1.810000},
    {"160M", "DIGU", "F1", 1.838000},
    {"160M", "LSB", "F6", 1.843000},
    {"160M", "LSB", "F6", 1.850000},
    {"80M", "CWL", "F1", 3.505000},
    {"80M", "CWL", "F1", 3.510000},
    {"80M", "DIGU", "F1", 3.590000},
    {"80M", "LSB", "F6", 3.750000},
    // From Thetis clsBandStackManager.cs:2034 [v2.10.3.15]
    //"80M", "LSB", "F6", 3.900000, false, 150, 0.0, // MW0LGE_21d should not be in region 1
    //"60M", "USB", "F6", 5.250000, false, 150, 0.0,
    //"60M", "USB", "F6", 5.325000, false, 150, 0.0,
    //"60M", "USB", "F6", 5.400000, false, 150, 0.0,
    {"60M", "USB", "F6", 5.354000},
    {"40M", "CWL", "F1", 7.005000},
    {"40M", "CWL", "F1", 7.010000},
    {"40M", "DIGU", "F1", 7.045000},
    {"40M", "LSB", "F6", 7.09000},
    {"40M", "LSB", "F6", 7.10000},
    {"30M", "CWU", "F1", 10.107000},
    {"30M", "CWU", "F1", 10.110000},
    {"30M", "CWU", "F1", 10.115000},
    {"30M", "CWU", "F1", 10.120000},
    {"30M", "DIGU", "F1", 10.140000},
    {"20M", "CWU", "F1", 14.005000},
    {"20M", "CWU", "F1", 14.010000},
    {"20M", "DIGU", "F1", 14.085000},
    {"20M", "USB", "F6", 14.155000},
    {"20M", "USB", "F6", 14.225000},
    {"17M", "CWU", "F1", 18.070000},
    {"17M", "CWU", "F1", 18.078000},
    {"17M", "DIGU", "F1", 18.100000},
    {"17M", "USB", "F6", 18.120000},
    {"17M", "USB", "F6", 18.140000},
    {"15M", "CWU", "F1", 21.005000},
    {"15M", "CWU", "F1", 21.010000},
    {"15M", "DIGU", "F1", 21.090000},
    {"15M", "USB", "F6", 21.210000},
    {"15M", "USB", "F6", 21.290000},
    {"12M", "CWU", "F1", 24.900000},
    {"12M", "CWU", "F1", 24.905000},
    {"12M", "DIGU", "F1", 24.920000},
    {"12M", "USB", "F6", 24.940000},
    {"12M", "USB", "F6", 24.950000},
    {"10M", "CWU", "F1", 28.005000},
    {"10M", "CWU", "F1", 28.010000},
    {"10M", "DIGU", "F1", 28.120000},
    {"10M", "USB", "F6", 28.400000},
    {"10M", "USB", "F6", 28.450000},
    {"6M", "CWU", "F1", 50.080000},
    {"6M", "CWU", "F1", 50.090000},
    {"6M", "USB", "F6", 50.150000},
    {"6M", "USB", "F6", 50.165000},
    {"6M", "DIGU", "F1", 50.250000},
    {"2M", "CWU", "F1", 144.040000},
    {"2M", "CWU", "F1", 144.050000},
    {"2M", "DIGU", "F1", 144.138000},
    {"2M", "USB", "F6", 144.300000},
    {"2M", "USB", "F6", 144.310000},
    {"WWV", "SAM", "F7", 2.500000},
    {"WWV", "SAM", "F7", 5.000000},
    {"WWV", "SAM", "F7", 10.000000},
    {"WWV", "SAM", "F7", 15.000000},
    {"WWV", "SAM", "F7", 20.000000},
    {"WWV", "SAM", "F5", 25.000000},
    {"GEN", "SAM", "F6", 13.845000},
    {"GEN", "SAM", "F7", 5.975000},
    {"GEN", "SAM", "F7", 9.550000},
    {"GEN", "SAM", "F7", 3.850000},
    {"GEN", "SAM", "F8", 0.590000},
};

// AddRegion2BandStack
constexpr RawSeedRow kRegion2Rows[] = {
    // From Thetis clsBandStackManager.cs:2099-2177 [v2.10.3.15]
    {"160M", "CWL", "F5", 1.810000},
    {"160M", "CWU", "F1", 1.835000},
    {"160M", "LSB", "F6", 1.840000},
    {"160M", "LSB", "F6", 1.845000},
    {"160M", "LSB", "F6", 1.845000},
    {"80M", "CWU", "F1", 3.501000},
    {"80M", "CWU", "F1", 3.520000},
    {"80M", "LSB", "F6", 3.650000},
    {"80M", "LSB", "F6", 3.715000},
    {"80M", "SAM", "F5", 3.875000},
    //"60M", "USB", "F6", 5.330500, false, 150, 0.0,
    //"60M", "USB", "F6", 5.346500, false, 150, 0.0,
    {"60M", "USB", "F6", 5.354000},
    //"60M", "USB", "F6", 5.357000, false, 150, 0.0,
    //"60M", "USB", "F6", 5.371500, false, 150, 0.0,
    //"60M", "USB", "F6", 5.403500, false, 150, 0.0,
    {"40M", "CWU", "F1", 7.001000},
    {"40M", "CWU", "F3", 7.021000},
    {"40M", "LSB", "F6", 7.152000},
    {"40M", "LSB", "F6", 7.180000},
    {"40M", "LSB", "F6", 7.255000},
    {"30M", "CWU", "F1", 10.107000},
    {"30M", "CWU", "F1", 10.110000},
    {"30M", "CWU", "F1", 10.120000},
    {"30M", "CWU", "F1", 10.130000},
    {"30M", "CWU", "F5", 10.140000},
    {"20M", "CWU", "F1", 14.010000},
    {"20M", "CWU", "F1", 14.020000},
    {"20M", "USB", "F6", 14.155000},
    {"20M", "USB", "F6", 14.230000},
    {"20M", "USB", "F6", 14.334000},
    {"17M", "CWU", "F1", 18.070000},
    {"17M", "CWU", "F1", 18.090000},
    {"17M", "USB", "F6", 18.125000},
    {"17M", "USB", "F6", 18.135000},
    {"17M", "USB", "F6", 18.140000},
    {"15M", "CWU", "F1", 21.001000},
    {"15M", "CWU", "F1", 21.021000},
    {"15M", "USB", "F6", 21.205000},
    {"15M", "USB", "F6", 21.255000},
    {"15M", "USB", "F6", 21.300000},
    {"12M", "CWU", "F1", 24.895000},
    {"12M", "CWU", "F1", 24.898000},
    {"12M", "USB", "F6", 24.931000},
    {"12M", "USB", "F6", 24.940000},
    {"12M", "USB", "F6", 24.950000},
    {"10M", "CWU", "F1", 28.010000},
    {"10M", "CWU", "F1", 28.020000},
    {"10M", "USB", "F6", 28.305000},
    {"10M", "USB", "F6", 28.350000},
    {"10M", "USB", "F6", 28.450000},
    {"6M", "CWU", "F1", 50.010000},
    {"6M", "CWU", "F1", 50.015000},
    {"6M", "USB", "F6", 50.125000},
    {"6M", "USB", "F6", 50.130000},
    {"6M", "USB", "F6", 50.200000},
    {"2M", "CWU", "F1", 144.010000},
    {"2M", "CWU", "F1", 144.015000},
    {"2M", "USB", "F6", 144.200000},
    {"2M", "USB", "F6", 144.220000},
    {"2M", "USB", "F6", 144.210000},
    {"WWV", "SAM", "F7", 2.500000},
    {"WWV", "SAM", "F7", 5.000000},
    {"WWV", "SAM", "F7", 10.000000},
    {"WWV", "SAM", "F7", 15.000000},
    {"WWV", "SAM", "F7", 20.000000},
    {"GEN", "SAM", "F5", 13.845000},
    {"GEN", "SAM", "F5", 9.550000},
    {"GEN", "SAM", "F5", 5.975000},
    {"GEN", "SAM", "F5", 3.250000},
    {"GEN", "SAM", "F4", 0.590000},
};

// AddRegion3BandStack
constexpr RawSeedRow kRegion3Rows[] = {
    // From Thetis clsBandStackManager.cs:2178-2235 [v2.10.3.15]
    {"160M", "CWL", "F1", 1.820000},
    {"160M", "DIGU", "F1", 1.832000},
    {"160M", "LSB", "F6", 1.843000},
    {"80M", "CWL", "F1", 3.510000},
    {"80M", "DIGU", "F1", 3.580000},
    {"80M", "LSB", "F6", 3.750000},
    {"60M", "USB", "F6", 5.354000},
    //"60M", "USB", "F6", 5.264000, false, 150, 0.0,
    //"60M", "USB", "F6", 5.357000, false, 150, 0.0,
    //"60M", "USB", "F6", 5.403500, false, 150, 0.0,
    {"40M", "CWL", "F1", 7.010000},
    {"40M", "DIGU", "F1", 7.035000},
    {"40M", "LSB", "F6", 7.12000},
    {"30M", "CWU", "F1", 10.110000},
    {"30M", "CWU", "F1", 10.120000},
    {"30M", "DIGU", "F1", 10.140000},
    {"20M", "CWU", "F1", 14.010000},
    {"20M", "DIGU", "F1", 14.085000},
    {"20M", "USB", "F6", 14.225000},
    {"17M", "CWU", "F1", 18.078000},
    {"17M", "DIGU", "F1", 18.100000},
    {"17M", "USB", "F6", 18.140000},
    {"15M", "CWU", "F1", 21.010000},
    {"15M", "DIGU", "F1", 21.090000},
    {"15M", "USB", "F6", 21.300000},
    {"12M", "CWU", "F1", 24.900000},
    {"12M", "DIGU", "F1", 24.920000},
    {"12M", "USB", "F6", 24.940000},
    {"10M", "CWU", "F1", 28.010000},
    {"10M", "DIGU", "F1", 28.120000},
    {"10M", "USB", "F6", 28.400000},
    {"6M", "CWU", "F1", 50.090000},
    {"6M", "USB", "F6", 50.150000},
    {"6M", "DIGU", "F1", 50.250000},
    {"2M", "CWU", "F1", 144.050000},
    {"2M", "DIGU", "F1", 144.138000},
    {"2M", "USB", "F6", 144.200000},
    {"WWV", "SAM", "F5", 2.500000},
    {"WWV", "SAM", "F5", 5.000000},
    {"WWV", "SAM", "F5", 10.000000},
    {"WWV", "SAM", "F5", 15.000000},
    {"WWV", "SAM", "F5", 20.000000},
    {"WWV", "SAM", "F5", 25.000000},
    {"WWV", "USB", "F6", 3.330000},
    {"WWV", "USB", "F6", 7.850000},
    {"WWV", "USB", "F6", 14.670000},
    {"GEN", "SAM", "F6", 13.845000},
    {"GEN", "SAM", "F7", 5.975000},
    {"GEN", "SAM", "F7", 9.550000},
    {"GEN", "SAM", "F7", 3.850000},
    {"GEN", "SAM", "F8", 0.590000},
};

// AddBandStackSWL
constexpr RawSeedRow kSwlRows[] = {
    // From Thetis clsBandStackManager.cs:2236-2314 [v2.10.3.15]
    {"LMF", "SAM", "F4", 0.560000},
    {"LMF", "SAM", "F4", 0.720000},
    {"LMF", "SAM", "F4", 0.780000},
    {"LMF", "SAM", "F4", 1.000000},
    {"LMF", "SAM", "F4", 1.700000},

    {"120M", "SAM", "F4", 2.400000},
    {"120M", "SAM", "F4", 2.410000},
    {"120M", "SAM", "F4", 2.420000},

    {"90M", "SAM", "F4", 3.300000},
    {"90M", "SAM", "F4", 3.310000},
    {"90M", "SAM", "F4", 3.320000},

    {"61M", "SAM", "F4", 4.700000},
    {"61M", "SAM", "F4", 4.800000},
    {"61M", "SAM", "F4", 4.820000},

    {"49M", "SAM", "F4", 5.600000},
    {"49M", "SAM", "F4", 5.700000},
    {"49M", "SAM", "F4", 5.800000},
    {"49M", "SAM", "F4", 5.900000},
    {"49M", "SAM", "F4", 6.000000},
    {"49M", "SAM", "F4", 6.200000},


    {"41M", "SAM", "F4", 7.310000},
    {"41M", "SAM", "F4", 7.400000},
    {"41M", "SAM", "F4", 7.500000},


    {"31M", "SAM", "F4", 9.100000},
    {"31M", "SAM", "F4", 9.200000},
    {"31M", "SAM", "F4", 9.300000},
    {"31M", "SAM", "F4", 9.400000},
    {"31M", "SAM", "F4", 9.500000},
    {"31M", "SAM", "F4", 9.600000},


    {"25M", "SAM", "F4", 11.700000},
    {"25M", "SAM", "F4", 11.800000},
    {"25M", "SAM", "F4", 11.900000},

    {"22M", "SAM", "F4", 13.600000},
    {"22M", "SAM", "F4", 13.700000},
    {"22M", "SAM", "F4", 13.800000},

    {"19M", "SAM", "F4", 15.200000},
    {"19M", "SAM", "F4", 15.300000},
    {"19M", "SAM", "F4", 15.400000},

    {"16M", "SAM", "F4", 17.500000},
    {"16M", "SAM", "F4", 17.600000},
    {"16M", "SAM", "F4", 17.700000},

    {"14M", "SAM", "F4", 18.900000},
    {"14M", "SAM", "F4", 19.000000},
    {"14M", "SAM", "F4", 19.100000},

    {"13M", "SAM", "F4", 21.500000},
    {"13M", "SAM", "F4", 21.600000},
    {"13M", "SAM", "F4", 21.700000},

    {"11M", "SAM", "F4", 25.700000},
    {"11M", "SAM", "F4", 26.000000},
    {"11M", "SAM", "F4", 26.500000},
    {"11M", "SAM", "F4", 27.000000},
    {"11M", "SAM", "F4", 27.500000},
    {"11M", "SAM", "F4", 27.800000},
};

// AddUK_PlusBandStack
constexpr RawSeedRow kUkPlusRows[] = {
    // From Thetis clsBandStackManager.cs:2315-2399 [v2.10.3.15]
    {"160M", "CWL", "F1", 1.810000},
    {"160M", "CWU", "F1", 1.820000},
    {"160M", "DIGU", "F1", 1.838000},
    {"160M", "USB", "F6", 1.840000},
    {"160M", "USB", "F6", 1.845000},
    {"80M", "CWL", "F1", 3.505000},
    {"80M", "CWU", "F1", 3.510000},
    {"80M", "DIGU", "F1", 3.590000},
    {"80M", "LSB", "F6", 3.750000},
    {"80M", "LSB", "F6", 3.770000},
    {"60M", "USB", "F6", 5.258500},
    {"60M", "USB", "F6", 5.276000},
    {"60M", "USB", "F6", 5.288500},
    {"60M", "USB", "F6", 5.298000},
    {"60M", "USB", "F6", 5.313000},
    {"60M", "USB", "F6", 5.333000},
    {"60M", "USB", "F6", 5.354000},
    {"60M", "USB", "F6", 5.362000},
    {"60M", "USB", "F6", 5.378000},
    {"60M", "USB", "F6", 5.395000},
    {"60M", "USB", "F6", 5.403500},
    {"40M", "CWL", "F1", 7.005000},
    {"40M", "CWU", "F1", 7.010000},
    {"40M", "DIGU", "F1", 7.045000},
    {"40M", "LSB", "F6", 7.09000},
    {"40M", "LSB", "F6", 7.10000},
    {"30M", "CWU", "F1", 10.105000},
    {"30M", "CWU", "F1", 10.110000},
    {"30M", "CWU", "F1", 10.120000},
    {"30M", "CWU", "F1", 10.130000},
    {"30M", "DIGU", "F1", 10.140000},
    {"20M", "CWU", "F1", 14.005000},
    {"20M", "CWU", "F1", 14.010000},
    {"20M", "DIGU", "F1", 14.085000},
    {"20M", "USB", "F6", 14.145000},
    {"20M", "USB", "F6", 14.225000},
    {"17M", "CWU", "F1", 18.070000},
    {"17M", "CWU", "F1", 18.078000},
    {"17M", "DIGU", "F1", 18.100000},
    {"17M", "USB", "F6", 18.140000},
    {"17M", "USB", "F6", 18.150000},
    {"15M", "CWU", "F1", 21.005000},
    {"15M", "CWU", "F1", 21.010000},
    {"15M", "DIGU", "F1", 21.090000},
    {"15M", "USB", "F6", 21.250000},
    {"15M", "USB", "F6", 21.300000},
    {"12M", "CWU", "F1", 24.900000},
    {"12M", "CWU", "F1", 24.910000},
    {"12M", "DIGU", "F1", 24.920000},
    {"12M", "USB", "F6", 24.940000},
    {"12M", "USB", "F6", 24.950000},
    {"10M", "CWU", "F1", 28.005000},
    {"10M", "CWU", "F1", 28.010000},
    {"10M", "DIGU", "F1", 28.120000},
    {"10M", "USB", "F6", 28.400000},
    {"10M", "USB", "F6", 28.450000},
    {"6M", "CWU", "F1", 50.090000},
    {"6M", "CWU", "F1", 50.095000},
    {"6M", "USB", "F6", 50.150000},
    {"6M", "USB", "F6", 50.160000},
    {"6M", "DIGU", "F1", 50.250000},
    {"2M", "CWU", "F1", 144.030000},
    {"2M", "CWU", "F1", 144.050000},
    {"2M", "DIGU", "F1", 144.138000},
    {"2M", "USB", "F6", 144.300000},
    {"2M", "USB", "F6", 144.310000},
    {"WWV", "SAM", "F7", 2.500000},
    {"WWV", "SAM", "F7", 5.000000},
    {"WWV", "SAM", "F7", 10.000000},
    {"WWV", "SAM", "F7", 15.000000},
    {"WWV", "SAM", "F7", 20.000000},
    {"WWV", "SAM", "F5", 25.000000},
    {"GEN", "SAM", "F6", 13.845000},
    {"GEN", "SAM", "F7", 5.975000},
    {"GEN", "SAM", "F7", 9.550000},
    {"GEN", "SAM", "F7", 3.850000},
    {"GEN", "SAM", "F8", 0.590000},
};

// AddUS_PlusBandStack
constexpr RawSeedRow kUsPlusRows[] = {
    // From Thetis clsBandStackManager.cs:2400-2411 [v2.10.3.15]
    {"60M", "USB", "F6", 5.332000 - (0.002800 / 2.0f)},
    {"60M", "USB", "F6", 5.348000 - (0.002800 / 2.0f)},
    {"60M", "USB", "F6", 5.358500 - (0.002800 / 2.0f)},
    {"60M", "USB", "F6", 5.373000 - (0.002800 / 2.0f)},
    {"60M", "USB", "F6", 5.405000 - (0.002800 / 2.0f)},
};

// AddSwedenBandStack
constexpr RawSeedRow kSwedenRows[] = {
    // From Thetis clsBandStackManager.cs:2412-2491 [v2.10.3.15]
    {"160M", "CWL", "F1", 1.805000},
    {"160M", "CWL", "F1", 1.810000},
    {"160M", "DIGU", "F1", 1.838000},
    {"160M", "LSB", "F6", 1.843000},
    {"160M", "LSB", "F6", 1.850000},
    {"80M", "CWL", "F1", 3.505000},
    {"80M", "CWL", "F1", 3.510000},
    {"80M", "DIGU", "F1", 3.590000},
    {"80M", "LSB", "F6", 3.750000},
    {"80M", "LSB", "F6", 3.900000},
    {"60M", "USB", "F6", 5.310000},
    {"60M", "USB", "F6", 5.320000},
    {"60M", "USB", "F6", 5.380000},
    {"60M", "USB", "F6", 5.390000},
    {"40M", "CWL", "F1", 7.005000},
    {"40M", "CWL", "F1", 7.010000},
    {"40M", "DIGU", "F1", 7.045000},
    {"40M", "LSB", "F6", 7.09000},
    {"40M", "LSB", "F6", 7.10000},
    {"30M", "CWU", "F1", 10.107000},
    {"30M", "CWU", "F1", 10.110000},
    {"30M", "CWU", "F1", 10.115000},
    {"30M", "CWU", "F1", 10.120000},
    {"30M", "DIGU", "F1", 10.140000},
    {"20M", "CWU", "F1", 14.005000},
    {"20M", "CWU", "F1", 14.010000},
    {"20M", "DIGU", "F1", 14.085000},
    {"20M", "USB", "F6", 14.155000},
    {"20M", "USB", "F6", 14.225000},
    {"17M", "CWU", "F1", 18.070000},
    {"17M", "CWU", "F1", 18.078000},
    {"17M", "DIGU", "F1", 18.100000},
    {"17M", "USB", "F6", 18.120000},
    {"17M", "USB", "F6", 18.140000},
    {"15M", "CWU", "F1", 21.005000},
    {"15M", "CWU", "F1", 21.010000},
    {"15M", "DIGU", "F1", 21.090000},
    {"15M", "USB", "F6", 21.210000},
    {"15M", "USB", "F6", 21.290000},
    {"12M", "CWU", "F1", 24.900000},
    {"12M", "CWU", "F1", 24.905000},
    {"12M", "DIGU", "F1", 24.920000},
    {"12M", "USB", "F6", 24.940000},
    {"12M", "USB", "F6", 24.950000},
    {"10M", "CWU", "F1", 28.005000},
    {"10M", "CWU", "F1", 28.010000},
    {"10M", "DIGU", "F1", 28.120000},
    {"10M", "USB", "F6", 28.400000},
    {"10M", "USB", "F6", 28.450000},
    {"6M", "CWU", "F1", 50.080000},
    {"6M", "CWU", "F1", 50.090000},
    {"6M", "USB", "F6", 50.150000},
    {"6M", "USB", "F6", 50.165000},
    {"6M", "DIGU", "F1", 50.250000},
    {"2M", "CWU", "F1", 144.040000},
    {"2M", "CWU", "F1", 144.050000},
    {"2M", "DIGU", "F1", 144.138000},
    {"2M", "USB", "F6", 144.300000},
    {"2M", "USB", "F6", 144.310000},
    {"WWV", "SAM", "F5", 2.500000},
    {"WWV", "SAM", "F5", 5.000000},
    {"WWV", "SAM", "F5", 10.000000},
    {"WWV", "SAM", "F5", 15.000000},
    {"WWV", "SAM", "F5", 20.000000},
    {"WWV", "SAM", "F5", 25.000000},
    {"WWV", "USB", "F6", 3.330000},
    {"WWV", "USB", "F6", 7.850000},
    {"WWV", "USB", "F6", 14.670000},
    {"GEN", "SAM", "F6", 13.845000},
    {"GEN", "SAM", "F7", 5.975000},
    {"GEN", "SAM", "F7", 9.550000},
    {"GEN", "SAM", "F7", 3.850000},
    {"GEN", "SAM", "F8", 0.590000},
};

struct SeedBandName {
    QLatin1StringView name;
    Band band;
};

// From Thetis clsBandStackManager.cs:1915-1955 [v2.10.3.15]
// Only the strings the seed tables use, and no XVTR: transverter bands have
// no seeds (band stack design V12). Thetis has a BLMF band and NereusSDR does
// not, so LMF goes to GEN (band stack design V16).
constexpr SeedBandName kSeedBandNames[] = {
    {QLatin1StringView("GEN"), Band::GEN},

    {QLatin1StringView("160M"), Band::Band160m},
    {QLatin1StringView("80M"), Band::Band80m},
    {QLatin1StringView("60M"), Band::Band60m},
    {QLatin1StringView("40M"), Band::Band40m},
    {QLatin1StringView("30M"), Band::Band30m},
    {QLatin1StringView("20M"), Band::Band20m},
    {QLatin1StringView("17M"), Band::Band17m},
    {QLatin1StringView("15M"), Band::Band15m},
    {QLatin1StringView("12M"), Band::Band12m},
    {QLatin1StringView("10M"), Band::Band10m},
    {QLatin1StringView("6M"), Band::Band6m},
    {QLatin1StringView("2M"), Band::Band2m},

    {QLatin1StringView("WWV"), Band::WWV},

    {QLatin1StringView("LMF"), Band::GEN},

    {QLatin1StringView("120M"), Band::Band120m},
    {QLatin1StringView("90M"), Band::Band90m},
    {QLatin1StringView("61M"), Band::Band61m},
    {QLatin1StringView("49M"), Band::Band49m},
    {QLatin1StringView("41M"), Band::Band41m},
    {QLatin1StringView("31M"), Band::Band31m},
    {QLatin1StringView("25M"), Band::Band25m},
    {QLatin1StringView("22M"), Band::Band22m},
    {QLatin1StringView("19M"), Band::Band19m},
    {QLatin1StringView("16M"), Band::Band16m},
    {QLatin1StringView("14M"), Band::Band14m},
    {QLatin1StringView("13M"), Band::Band13m},
    {QLatin1StringView("11M"), Band::Band11m},
};

// From Thetis clsBandStackManager.cs:1981-2002 [v2.10.3.15]
void appendRows(QList<SeedRow>& out, std::span<const RawSeedRow> rows,
                safety::Region region, bool bIgnore60m = false)
{
    for (const RawSeedRow& row : rows) {
        const std::optional<Band> band = seedBandFromName(QLatin1StringView(row.band));
        const QLatin1StringView filter(row.filter);
        bool filterOk = false;
        const int filterNumber = filter.startsWith(QLatin1Char('F'))
                                     ? filter.sliced(1).toInt(&filterOk)
                                     : 0;
        if (!band.has_value() || !filterOk) {
            // A row is dropped, never filed under GEN. The per-routine row
            // counts in tst_bandstack_seeds fail if this ever happens.
            qCWarning(lcApp) << "Band stack seed row not loaded:" << row.band << row.filter;
            continue;
        }

        if (bIgnore60m && *band == Band::Band60m) {
            continue;
        }

        bool locked = false;
        if (region == safety::Region::UnitedStates && *band == Band::Band60m) {
            locked = true; //pre-lock 60m band entries for the US
        }
        if (*band == Band::WWV) {
            locked = true; // pre-lock the WWV's
        }

        out.append(SeedRow{*band, QString::fromLatin1(row.mode), filterNumber,
                           qRound64(row.frequencyMhz * 1e6), locked});
    }
}

} // namespace

std::optional<Band> seedBandFromName(const QString& name)
{
    for (const SeedBandName& entry : kSeedBandNames) {
        if (name == entry.name) {
            return entry.band;
        }
    }
    return std::nullopt;
}

QList<SeedRow> seedsForRegion(safety::Region region)
{
    using safety::Region;

    QList<SeedRow> rows;

    // From Thetis clsBandStackManager.cs:1084-1158 [v2.10.3.15]
    // By name: NereusSDR's region numbers are not Thetis's FRSRegion numbers.
    switch (region) {
    case Region::Australia:
        appendRows(rows, kRegion2Rows, region);
        break;
    case Region::UnitedStates:
        appendRows(rows, kRegion2Rows, region, true);
        appendRows(rows, kUsPlusRows, region);
        break;
    case Region::Japan:
        //AddRegionJapanBandStack();
        appendRows(rows, kRegion3Rows, region);
        break;
    case Region::India:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Spain:
    case Region::Slovakia:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Europe:
    case Region::Italy:
    case Region::Germany:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Israel:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::UnitedKingdom:
        appendRows(rows, kUkPlusRows, region);
        break;
    case Region::Norway:
    case Region::Denmark:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Latvia:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Bulgaria:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Greece:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Hungary:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Netherlands:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::France:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Russia:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Sweden:
        appendRows(rows, kSwedenRows, region);
        break;
    case Region::Region1:
        appendRows(rows, kRegion1Rows, region);
        break;
    case Region::Region2:
        appendRows(rows, kRegion2Rows, region);
        break;
    case Region::Region3:
        appendRows(rows, kRegion3Rows, region);
        break;
    }

    // add to all
    appendRows(rows, kSwlRows, region);

    return rows;
}

QString regionName(safety::Region region)
{
    using safety::Region;

    // General Options' region list (GeneralOptionsPage.cpp, comboFRSRegion).
    switch (region) {
    case Region::Australia:     return QStringLiteral("Australia");
    case Region::Europe:        return QStringLiteral("Europe");
    case Region::India:         return QStringLiteral("India");
    case Region::Italy:         return QStringLiteral("Italy");
    case Region::Israel:        return QStringLiteral("Israel");
    case Region::Japan:         return QStringLiteral("Japan");
    case Region::Spain:         return QStringLiteral("Spain");
    case Region::UnitedKingdom: return QStringLiteral("United Kingdom");
    case Region::UnitedStates:  return QStringLiteral("United States");
    case Region::Norway:        return QStringLiteral("Norway");
    case Region::Denmark:       return QStringLiteral("Denmark");
    case Region::Sweden:        return QStringLiteral("Sweden");
    case Region::Latvia:        return QStringLiteral("Latvia");
    case Region::Slovakia:      return QStringLiteral("Slovakia");
    case Region::Bulgaria:      return QStringLiteral("Bulgaria");
    case Region::Greece:        return QStringLiteral("Greece");
    case Region::Hungary:       return QStringLiteral("Hungary");
    case Region::Netherlands:   return QStringLiteral("Netherlands");
    case Region::France:        return QStringLiteral("France");
    case Region::Russia:        return QStringLiteral("Russia");
    case Region::Region1:       return QStringLiteral("IARU Region 1");
    case Region::Region2:       return QStringLiteral("IARU Region 2");
    case Region::Region3:       return QStringLiteral("IARU Region 3");
    case Region::Germany:       return QStringLiteral("Germany");
    }
    return QString();
}

} // namespace NereusSDR
