// =================================================================
// src/gui/meters/OtherButtonCatalog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucOtherButtonsOptionsGrid.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - Ported the other-buttons table (OtherButtonIdHelpers
//                 CheckBoxData) for Thetis button numbers, plus the map from
//                 NereusSDR's earlier button numbers. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

/*  ucOtherButtonsOptionsGrid.cs

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

#include "OtherButtonCatalog.h"

#include <array>

namespace NereusSDR::OtherButtonCatalog {
namespace {

using Id = OtherButtonItem::ButtonId;

struct Row {
    int group;
    int bit;
    const char* caption;
    const char* tooltip;
    const char* section;
};

// From Thetis ucOtherButtonsOptionsGrid.cs:529-755 [v2.10.3.15]
// (OtherButtonIdHelpers.CheckBoxData). Captions and tooltips verbatim; the
// section is the INFO_TEXT row above each block. Icons are not ported.
constexpr Row kRows[] = {
    {0, 0, "Power", "Power on/off", "General"}, // POWER [ucOtherButtonsOptionsGrid.cs:534]
    {0, 1, "RX 2", "Enable RX2", "General"}, // RX_2 [ucOtherButtonsOptionsGrid.cs:535]
    {0, 2, "MON", "Monitor your tx audio", "General"}, // MON [ucOtherButtonsOptionsGrid.cs:536]
    {0, 3, "TUN", "Put radio into tune", "General"}, // TUN [ucOtherButtonsOptionsGrid.cs:537]
    {0, 4, "MOX", "Put radio into mox", "General"}, // MOX [ucOtherButtonsOptionsGrid.cs:538]
    {0, 5, "2TON", "Put out a two tone signal", "General"}, // TWOTON [ucOtherButtonsOptionsGrid.cs:539]
    {0, 6, "DUP", "Duplex mode, view the tx rx", "General"}, // DUP [ucOtherButtonsOptionsGrid.cs:540]
    {0, 7, "PS-A", "Puresignal auto", "General"}, // PS_A [ucOtherButtonsOptionsGrid.cs:541]
    {0, 8, "xPA", "Override OC pins with this", "General"}, // XPA [ucOtherButtonsOptionsGrid.cs:542]
    {0, 9, "Rec", "Wave quick record", "General"}, // REC [ucOtherButtonsOptionsGrid.cs:543]
    {0, 10, "Play", "Wave quick playback", "General"}, // PLAY [ucOtherButtonsOptionsGrid.cs:544]
    {1, 0, "NR", "Cycle through NR's", "Noise / ATT"}, // NR [ucOtherButtonsOptionsGrid.cs:548]
    {1, 1, "NR1", "NR1 on/off", "Noise / ATT"}, // NR1 [ucOtherButtonsOptionsGrid.cs:549]
    {1, 2, "NR2", "NR2 on/off", "Noise / ATT"}, // NR2 [ucOtherButtonsOptionsGrid.cs:550]
    {1, 3, "NR3", "NR3 on/off", "Noise / ATT"}, // NR3 [ucOtherButtonsOptionsGrid.cs:551]
    {1, 4, "NR4", "NR4 on/off", "Noise / ATT"}, // NR4 [ucOtherButtonsOptionsGrid.cs:552]
    {1, 5, "ANF", "Auto notch filter", "Noise / ATT"}, // ANF [ucOtherButtonsOptionsGrid.cs:553]
    {1, 6, "NB", "Cycle through noise blankers", "Noise / ATT"}, // NB [ucOtherButtonsOptionsGrid.cs:554]
    {1, 7, "NB1", "NB1 on/off", "Noise / ATT"}, // NB1 [ucOtherButtonsOptionsGrid.cs:555]
    {1, 8, "NB2", "NB2 on/off", "Noise / ATT"}, // NB2 [ucOtherButtonsOptionsGrid.cs:556]
    {1, 9, "SNB", "Enabled / disable spectral noise blanker", "Noise / ATT"}, // SNB [ucOtherButtonsOptionsGrid.cs:557]
    {1, 10, "MNF", "Manual not filters on/off", "Noise / ATT"}, // MNF [ucOtherButtonsOptionsGrid.cs:558]
    {1, 11, "MNF+", "Add a manual notch", "Noise / ATT"}, // MNF_PLUS [ucOtherButtonsOptionsGrid.cs:559]
    {1, 12, "S-ATT", "Step attenuator on/off", "Noise / ATT"}, // ATT_STEP [ucOtherButtonsOptionsGrid.cs:560]
    {1, 13, "ATT-0", "0dB attenuation", "Noise / ATT"}, // ATT_0 [ucOtherButtonsOptionsGrid.cs:561]
    {1, 14, "ATT-10", "10dB attenuation", "Noise / ATT"}, // ATT_10 [ucOtherButtonsOptionsGrid.cs:562]
    {1, 15, "ATT-20", "20db attenuation", "Noise / ATT"}, // ATT_20 [ucOtherButtonsOptionsGrid.cs:563]
    {1, 16, "ATT-30", "30db attenuation", "Noise / ATT"}, // ATT_30 [ucOtherButtonsOptionsGrid.cs:564]
    {1, 17, "ATT-40", "40db attenuation", "Noise / ATT"}, // ATT_40 [ucOtherButtonsOptionsGrid.cs:565]
    {1, 18, "ATT-50", "50db attenuation", "Noise / ATT"}, // ATT_50 [ucOtherButtonsOptionsGrid.cs:566]
    {1, 19, "ATT+", "Increaase attenuation", "Noise / ATT"}, // ATT_P1 [ucOtherButtonsOptionsGrid.cs:567]
    {1, 20, "ATT-", "Decrease attenuation", "Noise / ATT"}, // ATT_M1 [ucOtherButtonsOptionsGrid.cs:568]
    {1, 21, "NF", "Noise floor display on/off", "Noise / ATT"}, // NF [ucOtherButtonsOptionsGrid.cs:569]
    {2, 0, "Split", "Split on/off", "VFOs"}, // SPLT [ucOtherButtonsOptionsGrid.cs:573]
    {2, 1, "A > B", "Copy VFOa frequency to VFOb", "VFOs"}, // A_TO_B [ucOtherButtonsOptionsGrid.cs:574]
    {2, 2, "0 Beat", "0 beat to strongest signal", "VFOs"}, // ZERO_BEAT [ucOtherButtonsOptionsGrid.cs:575]
    {2, 3, "A < B", "Copy VFOa frequency to VFOb", "VFOs"}, // B_TO_A [ucOtherButtonsOptionsGrid.cs:576]
    {2, 4, "IF > V", "IF to the VFO", "VFOs"}, // IF_TO_V [ucOtherButtonsOptionsGrid.cs:577]
    {2, 5, "A <> B", "Swap VFOa and VFOb frequencies", "VFOs"}, // SWAP_AB [ucOtherButtonsOptionsGrid.cs:578]
    {2, 6, "RIT", "RIT on/off", "VFOs"}, // RIT [ucOtherButtonsOptionsGrid.cs:579]
    {2, 7, "RIT0", "0 rit", "VFOs"}, // RIT0 [ucOtherButtonsOptionsGrid.cs:580]
    {2, 8, "XIT", "XIT on/off", "VFOs"}, // XIT [ucOtherButtonsOptionsGrid.cs:581]
    {2, 9, "XIT0", "0 rit", "VFOs"}, // XIT0 [ucOtherButtonsOptionsGrid.cs:582]
    {2, 10, "SYNC", "Sync the vfos", "VFOs"}, // VFO_SYNC [ucOtherButtonsOptionsGrid.cs:583]
    {2, 11, "LockA", "Lock VFOa", "VFOs"}, // LOCK_A [ucOtherButtonsOptionsGrid.cs:584]
    {2, 12, "LockB", "Lock VFOb", "VFOs"}, // LOCK_B [ucOtherButtonsOptionsGrid.cs:585]
    {2, 13, "TS+", "Increase tunestep", "VFOs"}, // TUNE_STEP_U [ucOtherButtonsOptionsGrid.cs:586]
    {2, 14, "TS-", "Decrease tunestep", "VFOs"}, // TUNE_STEP_D [ucOtherButtonsOptionsGrid.cs:587]
    {2, 15, "STACK+", "Move up one bandstack entry", "VFOs"}, // STACK_U [ucOtherButtonsOptionsGrid.cs:588]
    {2, 16, "STACK-", "Move down on bandstack entry", "VFOs"}, // STACK_D [ucOtherButtonsOptionsGrid.cs:589]
    {3, 0, "Avg", "Display averaging on/off", "Display"}, // AVG [ucOtherButtonsOptionsGrid.cs:593]
    {3, 1, "Peak", "Peak hold on/off", "Display"}, // PEAK_HOLD [ucOtherButtonsOptionsGrid.cs:594]
    {3, 2, "CTUN", "ClickTune on/off", "Display"}, // CTUN [ucOtherButtonsOptionsGrid.cs:595]
    {3, 3, "Spectrum", "Spectrum", "Display"}, // SPECTRUM [ucOtherButtonsOptionsGrid.cs:596]
    {3, 4, "Panadapter", "Panadaptor", "Display"}, // PANADAPTER [ucOtherButtonsOptionsGrid.cs:597]
    {3, 5, "Scope", "Scope", "Display"}, // SCOPE [ucOtherButtonsOptionsGrid.cs:598]
    {3, 6, "Scope2", "Scope2", "Display"}, // SCOPE2 [ucOtherButtonsOptionsGrid.cs:599]
    {3, 7, "Phase", "Phase", "Display"}, // PHASE [ucOtherButtonsOptionsGrid.cs:600]
    {3, 8, "Waterfall", "Waterfall", "Display"}, // WATERFALL [ucOtherButtonsOptionsGrid.cs:601]
    {3, 9, "Histogram", "Histogram", "Display"}, // HISTOGRAM [ucOtherButtonsOptionsGrid.cs:602]
    {3, 10, "Panafall", "Panafall", "Display"}, // PANAFALL [ucOtherButtonsOptionsGrid.cs:603]
    {3, 11, "Panascope", "Panascope", "Display"}, // PANASCOPE [ucOtherButtonsOptionsGrid.cs:604]
    {3, 12, "Spectrascope", "Spectrascope", "Display"}, // SPECTRASCOPE [ucOtherButtonsOptionsGrid.cs:605]
    {3, 13, "Off", "Display off", "Display"}, // DISPLAY_OFF [ucOtherButtonsOptionsGrid.cs:606]
    {3, 14, "Pause", "Pause the display", "Display"}, // PAUSE [ucOtherButtonsOptionsGrid.cs:607]
    {3, 15, "Peak Blobs", "Peak blobs on/off", "Display"}, // PEAK_BLOBS [ucOtherButtonsOptionsGrid.cs:608]
    {3, 16, "Cur Info", "Show info on the cursor", "Display"}, // CURSOR_INFO [ucOtherButtonsOptionsGrid.cs:609]
    {3, 17, "Spots", "Show TCI spots", "Display"}, // SPOTS [ucOtherButtonsOptionsGrid.cs:610]
    {3, 18, "Fill", "Fill the panadaptor area", "Display"}, // FILL_SPECTRUM [ucOtherButtonsOptionsGrid.cs:611]
    {3, 19, "PAN+", "Pan the display to the right", "Display"}, // PAN_P5 [ucOtherButtonsOptionsGrid.cs:612]
    {3, 20, "PAN-", "Pan the display to the left", "Display"}, // PAN_M5 [ucOtherButtonsOptionsGrid.cs:613]
    {3, 21, "Centre", "Centre the display", "Display"}, // PAN_CENTRE [ucOtherButtonsOptionsGrid.cs:614]
    {3, 22, "ZTB", "Zoom to band", "Display"}, // ZTB [ucOtherButtonsOptionsGrid.cs:615]
    {3, 23, "0.5x", "0.5x zoom", "Display"}, // ZOOM_0P5 [ucOtherButtonsOptionsGrid.cs:616]
    {3, 24, "1x", "1x zoom", "Display"}, // ZOOM_1 [ucOtherButtonsOptionsGrid.cs:617]
    {3, 25, "2x", "2x zoom", "Display"}, // ZOOM_2 [ucOtherButtonsOptionsGrid.cs:618]
    {3, 26, "4x", "4x zoom", "Display"}, // ZOOM_4 [ucOtherButtonsOptionsGrid.cs:619]
    {3, 27, "A-Peak", "Active Peak", "Display"}, // ACTITVE_PEAK [ucOtherButtonsOptionsGrid.cs:620]
    {4, 0, "Vac1", "Vac1 on/off", "Audio / DSP"}, // VAC1 [ucOtherButtonsOptionsGrid.cs:624]
    {4, 1, "Vac2", "Vac2 on/off", "Audio / DSP"}, // VAC2 [ucOtherButtonsOptionsGrid.cs:625]
    {4, 2, "Mute", "Mute on/off", "Audio / DSP"}, // MUTE [ucOtherButtonsOptionsGrid.cs:626]
    {4, 3, "Mute All", "Mute all receivers on/off", "Audio / DSP"}, // MUTE_ALL [ucOtherButtonsOptionsGrid.cs:627]
    {4, 4, "Bin", "Binaurial mode", "Audio / DSP"}, // BIN [ucOtherButtonsOptionsGrid.cs:628]
    {4, 5, "SubRX", "Enable the sub receiver for rx1", "Audio / DSP"}, // SUBRX [ucOtherButtonsOptionsGrid.cs:629]
    {4, 6, "SwapLR", "Swap left and right speakers", "Audio / DSP"}, // PAN_SWAP [ucOtherButtonsOptionsGrid.cs:630]
    {4, 7, "SQL", "Enable squeltch", "Audio / DSP"}, // SQL [ucOtherButtonsOptionsGrid.cs:631]
    {4, 8, "Reg SQL", "Normal squeltch", "Audio / DSP"}, // SQL_SQL [ucOtherButtonsOptionsGrid.cs:632]
    {4, 9, "Voice SQL", "Voice base squeltch", "Audio / DSP"}, // SQL_VSQL [ucOtherButtonsOptionsGrid.cs:633]
    {4, 10, "SQL+", "Increase squelch sensitivity", "Audio / DSP"}, // SQL_P5 [ucOtherButtonsOptionsGrid.cs:634]
    {4, 11, "SQL-", "Decrease squelch sensitivity", "Audio / DSP"}, // SQL_M5 [ucOtherButtonsOptionsGrid.cs:635]
    {4, 12, "MIC", "Mic on/off", "Audio / DSP"}, // MIC [ucOtherButtonsOptionsGrid.cs:636]
    {4, 13, "COMP", "Compressor on/off", "Audio / DSP"}, // COMP [ucOtherButtonsOptionsGrid.cs:637]
    {4, 14, "VOX", "Vox on/off", "Audio / DSP"}, // VOX [ucOtherButtonsOptionsGrid.cs:638]
    {4, 15, "DEXP", "Downward expander on/off", "Audio / DSP"}, // DEXP [ucOtherButtonsOptionsGrid.cs:639]
    {4, 16, "RX EQ", "Receiver EQ on/off", "Audio / DSP"}, // RX_EQ [ucOtherButtonsOptionsGrid.cs:640]
    {4, 17, "TX EQ", "Transmit EQ on/off", "Audio / DSP"}, // TX_EQ [ucOtherButtonsOptionsGrid.cs:641]
    {4, 18, "TX Filter", "Show transmit filter", "Audio / DSP"}, // TX_FILTER [ucOtherButtonsOptionsGrid.cs:642]
    {4, 19, "CFC", "Coninuous frequency compression on/off", "Audio / DSP"}, // CFC [ucOtherButtonsOptionsGrid.cs:643]
    {4, 20, "CFC EQ", "CFC eq on/off", "Audio / DSP"}, // CFC_EQ [ucOtherButtonsOptionsGrid.cs:644]
    {4, 21, "MAF+", "Increase Master AF", "Audio / DSP"}, // MAF_P5 [ucOtherButtonsOptionsGrid.cs:645]
    {4, 22, "MAF-", "Decrease Master AF", "Audio / DSP"}, // MAF_M5 [ucOtherButtonsOptionsGrid.cs:646]
    {4, 23, "AF+", "Increase rx AF", "Audio / DSP"}, // AF_P5 [ucOtherButtonsOptionsGrid.cs:647]
    {4, 24, "AF-", "Decrease rx AF", "Audio / DSP"}, // AF_M5 [ucOtherButtonsOptionsGrid.cs:648]
    {4, 25, "BAL+", "Pan audio right", "Audio / DSP"}, // BAL_P5 [ucOtherButtonsOptionsGrid.cs:649]
    {4, 26, "BAL-", "Pan audio left", "Audio / DSP"}, // BAL_M5 [ucOtherButtonsOptionsGrid.cs:650]
    {4, 27, "SAF+", "Increase subrx AF", "Audio / DSP"}, // SAF_P5 [ucOtherButtonsOptionsGrid.cs:651]
    {4, 28, "SAF-", "Decrease subrx AF", "Audio / DSP"}, // SAF_M5 [ucOtherButtonsOptionsGrid.cs:652]
    {4, 29, "SBAL+", "Pan subrx audio right", "Audio / DSP"}, // SBAL_P5 [ucOtherButtonsOptionsGrid.cs:653]
    {4, 30, "SBAL-", "Pan subrx audio left", "Audio / DSP"}, // SBAL_M5 [ucOtherButtonsOptionsGrid.cs:654]
    {5, 0, "MIC+", "Increase mic gain", "Audio / DSP"}, // MIC_P1 [ucOtherButtonsOptionsGrid.cs:655]
    {5, 1, "MIC-", "Decrease mic gain", "Audio / DSP"}, // MIC_M1 [ucOtherButtonsOptionsGrid.cs:656]
    {5, 3, "COMP+", "Increase compression", "Audio / DSP"}, // COMP_P1 [ucOtherButtonsOptionsGrid.cs:657]
    {5, 4, "COMP-", "Decrease compression", "Audio / DSP"}, // COMP_M1 [ucOtherButtonsOptionsGrid.cs:658]
    {5, 5, "VOX+", "Increase vox level", "Audio / DSP"}, // VOX_P1 [ucOtherButtonsOptionsGrid.cs:659]
    {5, 6, "VOX-", "Decrease vox level", "Audio / DSP"}, // VOX_M1 [ucOtherButtonsOptionsGrid.cs:660]
    {5, 7, "Leveler", "Leveler on/off", "Audio / DSP"}, // LEVELER [ucOtherButtonsOptionsGrid.cs:661]
    {5, 8, "PhaRot", "Phase rotator on/off", "Audio / DSP"}, // PHASE_ROT [ucOtherButtonsOptionsGrid.cs:662]
    {5, 9, "Wav Record", "Wave record", "Audio / DSP"}, // WAVE_RECORD [ucOtherButtonsOptionsGrid.cs:663]
    {6, 0, "FIX", "Fixed agc", "AGC"}, // AGC_FIXED [ucOtherButtonsOptionsGrid.cs:667]
    {6, 1, "LONG", "Long agc", "AGC"}, // AGC_LONG [ucOtherButtonsOptionsGrid.cs:668]
    {6, 2, "SLOW", "Slow agc", "AGC"}, // AGC_SLOW [ucOtherButtonsOptionsGrid.cs:669]
    {6, 3, "MED", "Medium agc", "AGC"}, // AGC_MEDIUM [ucOtherButtonsOptionsGrid.cs:670]
    {6, 4, "FAST", "Fast agc", "AGC"}, // AGC_FAST [ucOtherButtonsOptionsGrid.cs:671]
    {6, 5, "CUST", "Custom agc", "AGC"}, // AGC_CUSTOM [ucOtherButtonsOptionsGrid.cs:672]
    {6, 6, "AUTO", "Auto adjust agc based on noise floor", "AGC"}, // AGC_AUTO [ucOtherButtonsOptionsGrid.cs:673]
    {6, 7, "AGC+", "Increase gain", "AGC"}, // AGC_P5 [ucOtherButtonsOptionsGrid.cs:674]
    {6, 8, "AGC-", "Decrease gain", "AGC"}, // AGC_M5 [ucOtherButtonsOptionsGrid.cs:675]
    {7, 0, "Dither", "Enable fpga dither on/off", "Hardware"}, // DITHER [ucOtherButtonsOptionsGrid.cs:678]
    {7, 1, "Random", "Enable fpga random on/off", "Hardware"}, // RANDOM [ucOtherButtonsOptionsGrid.cs:679]
    {7, 2, "48k", "48k hardware sample rate", "Hardware"}, // SR_48000 [ucOtherButtonsOptionsGrid.cs:680]
    {7, 3, "96k", "96k hardware sample rate", "Hardware"}, // SR_96000 [ucOtherButtonsOptionsGrid.cs:681]
    {7, 4, "192k", "192k hardware sample rate", "Hardware"}, // SR_192000 [ucOtherButtonsOptionsGrid.cs:682]
    {7, 5, "384k", "384k hardware sample rate", "Hardware"}, // SR_384000 [ucOtherButtonsOptionsGrid.cs:683]
    {7, 6, "768k", "768k hardware sample rate", "Hardware"}, // SR_768000 [ucOtherButtonsOptionsGrid.cs:684]
    {7, 7, "1536k", "1536k hardware sample rate", "Hardware"}, // SR_1536000 [ucOtherButtonsOptionsGrid.cs:685]
    {8, 0, "DRI+", "Increase drive", "RF Power"}, // DRIVE_P5 [ucOtherButtonsOptionsGrid.cs:689]
    {8, 1, "DRI-", "Decrease drive", "RF Power"}, // DRIVE_M5 [ucOtherButtonsOptionsGrid.cs:690]
    {8, 2, "TUN+", "Increase tune power", "RF Power"}, // TUNE_P5 [ucOtherButtonsOptionsGrid.cs:691]
    {8, 3, "TUN-", "Decrease tune power", "RF Power"}, // TUNE_M5 [ucOtherButtonsOptionsGrid.cs:692]
    {8, 4, "DRI-0", "Set drive power to 0", "RF Power"}, // DRIVE_0 [ucOtherButtonsOptionsGrid.cs:693]
    {8, 5, "TUN-0", "Set tune power to 0", "RF Power"}, // TUN_0 [ucOtherButtonsOptionsGrid.cs:694]
    {9, 0, "Setup", "Show setup", "Forms"}, // FORM_SETUP [ucOtherButtonsOptionsGrid.cs:698]
    {9, 1, "DB Man", "Show database manager", "Forms"}, // FORM_DBMAN [ucOtherButtonsOptionsGrid.cs:699]
    {9, 2, "Memory", "Show memory form", "Forms"}, // FORM_MEMORY [ucOtherButtonsOptionsGrid.cs:700]
    {9, 3, "AmpView", "Show ampview form", "Forms"}, // FORM_AMPVIEW [ucOtherButtonsOptionsGrid.cs:701]
    {9, 4, "Equaliser", "Show equaliser form", "Forms"}, // FORM_EQ [ucOtherButtonsOptionsGrid.cs:702]
    {9, 5, "XVTR", "Show transverter form", "Forms"}, // FORM_XVTR [ucOtherButtonsOptionsGrid.cs:703]
    {9, 6, "CWX", "Show CWX form", "Forms"}, // FORM_CWX [ucOtherButtonsOptionsGrid.cs:704]
    {9, 8, "Diversity", "Show diversity form", "Forms"}, // FORM_DIVERSITY [ucOtherButtonsOptionsGrid.cs:705]
    {9, 9, "Linearity", "Show linearity form", "Forms"}, // FORM_LINEARITY [ucOtherButtonsOptionsGrid.cs:706]
    {9, 10, "Wideband", "Show wideband", "Forms"}, // FORM_WB [ucOtherButtonsOptionsGrid.cs:707]
    {10, 0, "KEY", "Key cwx", "CWX"}, // CWX_KEY [ucOtherButtonsOptionsGrid.cs:711]
    {10, 1, "STOP", "Stop cwx", "CWX"}, // CWX_STOP [ucOtherButtonsOptionsGrid.cs:712]
    {10, 2, "F1", "Function key 1", "CWX"}, // CWX_F1 [ucOtherButtonsOptionsGrid.cs:713]
    {10, 3, "F2", "Function key 2", "CWX"}, // CWX_F2 [ucOtherButtonsOptionsGrid.cs:714]
    {10, 4, "F3", "Function key 3", "CWX"}, // CWX_F3 [ucOtherButtonsOptionsGrid.cs:715]
    {10, 5, "F4", "Function key 4", "CWX"}, // CWX_F4 [ucOtherButtonsOptionsGrid.cs:716]
    {10, 6, "F5", "Function key 5", "CWX"}, // CWX_F5 [ucOtherButtonsOptionsGrid.cs:717]
    {10, 7, "F6", "Function key 6", "CWX"}, // CWX_F6 [ucOtherButtonsOptionsGrid.cs:718]
    {10, 8, "F7", "Function key 7", "CWX"}, // CWX_F7 [ucOtherButtonsOptionsGrid.cs:719]
    {10, 9, "F8", "Function key 8", "CWX"}, // CWX_F8 [ucOtherButtonsOptionsGrid.cs:720]
    {10, 10, "F9", "Function key 9", "CWX"}, // CWX_F9 [ucOtherButtonsOptionsGrid.cs:721]
    {11, 0, "M1", "Macro 1", "Macros"}, // _MACRO_0 [ucOtherButtonsOptionsGrid.cs:725]
    {11, 1, "M2", "Macro 2", "Macros"}, // _MACRO_1 [ucOtherButtonsOptionsGrid.cs:726]
    {11, 2, "M3", "Macro 3", "Macros"}, // _MACRO_2 [ucOtherButtonsOptionsGrid.cs:727]
    {11, 3, "M4", "Macro 4", "Macros"}, // _MACRO_3 [ucOtherButtonsOptionsGrid.cs:728]
    {11, 4, "M5", "Macro 5", "Macros"}, // _MACRO_4 [ucOtherButtonsOptionsGrid.cs:729]
    {11, 5, "M6", "Macro 6", "Macros"}, // _MACRO_5 [ucOtherButtonsOptionsGrid.cs:730]
    {11, 6, "M7", "Macro 7", "Macros"}, // _MACRO_6 [ucOtherButtonsOptionsGrid.cs:731]
    {11, 7, "M8", "Macro 8", "Macros"}, // _MACRO_7 [ucOtherButtonsOptionsGrid.cs:732]
    {11, 8, "M9", "Macro 9", "Macros"}, // _MACRO_8 [ucOtherButtonsOptionsGrid.cs:733]
    {11, 9, "M10", "Macro 10", "Macros"}, // _MACRO_9 [ucOtherButtonsOptionsGrid.cs:734]
    {11, 10, "M11", "Macro 11", "Macros"}, // _MACRO_10 [ucOtherButtonsOptionsGrid.cs:735]
    {11, 11, "M12", "Macro 12", "Macros"}, // _MACRO_11 [ucOtherButtonsOptionsGrid.cs:736]
    {11, 12, "M13", "Macro 13", "Macros"}, // _MACRO_12 [ucOtherButtonsOptionsGrid.cs:737]
    {11, 13, "M14", "Macro 14", "Macros"}, // _MACRO_13 [ucOtherButtonsOptionsGrid.cs:738]
    {11, 14, "M15", "Macro 15", "Macros"}, // _MACRO_14 [ucOtherButtonsOptionsGrid.cs:739]
    {11, 15, "M16", "Macro 16", "Macros"}, // _MACRO_15 [ucOtherButtonsOptionsGrid.cs:740]
    {11, 16, "M17", "Macro 17", "Macros"}, // _MACRO_16 [ucOtherButtonsOptionsGrid.cs:741]
    {11, 17, "M18", "Macro 18", "Macros"}, // _MACRO_17 [ucOtherButtonsOptionsGrid.cs:742]
    {11, 18, "M19", "Macro 19", "Macros"}, // _MACRO_18 [ucOtherButtonsOptionsGrid.cs:743]
    {11, 19, "M20", "Macro 20", "Macros"}, // _MACRO_19 [ucOtherButtonsOptionsGrid.cs:744]
    {11, 20, "M21", "Macro 21", "Macros"}, // _MACRO_20 [ucOtherButtonsOptionsGrid.cs:745]
    {11, 21, "M22", "Macro 22", "Macros"}, // _MACRO_21 [ucOtherButtonsOptionsGrid.cs:746]
    {11, 22, "M23", "Macro 23", "Macros"}, // _MACRO_22 [ucOtherButtonsOptionsGrid.cs:747]
    {11, 23, "M24", "Macro 24", "Macros"}, // _MACRO_23 [ucOtherButtonsOptionsGrid.cs:748]
    {11, 24, "M25", "Macro 25", "Macros"}, // _MACRO_24 [ucOtherButtonsOptionsGrid.cs:749]
    {11, 25, "M26", "Macro 26", "Macros"}, // _MACRO_25 [ucOtherButtonsOptionsGrid.cs:750]
    {11, 26, "M27", "Macro 27", "Macros"}, // _MACRO_26 [ucOtherButtonsOptionsGrid.cs:751]
    {11, 27, "M28", "Macro 28", "Macros"}, // _MACRO_27 [ucOtherButtonsOptionsGrid.cs:752]
    {11, 28, "M29", "Macro 29", "Macros"}, // _MACRO_28 [ucOtherButtonsOptionsGrid.cs:753]
    {11, 29, "M30", "Macro 30", "Macros"}, // _MACRO_29 [ucOtherButtonsOptionsGrid.cs:754]
    {11, 30, "M31", "Macro 31", "Macros"}, // _MACRO_30 [ucOtherButtonsOptionsGrid.cs:755]
};

// NereusSDR-original: earlier ordinal -> Thetis id for the 34 core buttons
// (OtherButtonItem.h before parity Task 58). Macro i was 64 + i.
constexpr std::array<int, 34> kLegacyCore = {
    0,   // Power
    1,   // Rx2
    2,   // Mon
    3,   // Tun
    4,   // Mox
    5,   // TwoTon
    6,   // Dup
    7,   // PsA
    10,  // Play
    9,   // Rec
    37,  // Anf
    41,  // Snb
    42,  // Mnf
    96,  // Avg
    97,  // PeakHold
    98,  // Ctun
    128, // Vac1
    129, // Vac2
    130, // Mute
    132, // Bin
    133, // SubRx
    134, // PanSwap
    8,   // Xpa
    99,  // Spectrum
    100, // Panadapter
    101, // Scope
    102, // Scope2
    103, // Phase
    104, // Waterfall
    105, // Histogram
    106, // Panafall
    107, // Panascope
    108, // Spectrascope
    109, // DisplayOff
};
constexpr int kLegacyMacroBase = 64;

const Row* rowFor(int id)
{
    for (const Row& row : kRows) {
        if (row.group * 32 + row.bit == id) { return &row; }
    }
    return nullptr;
}

} // namespace

OtherButtonInfo info(Id id)
{
    const int value = static_cast<int>(id);
    OtherButtonInfo result{value / 32, value % 32, {}, {}, {}};
    if (const Row* row = rowFor(value)) {
        result.caption = QString::fromUtf8(row->caption);
        result.tooltip = QString::fromUtf8(row->tooltip);
        result.section = QString::fromUtf8(row->section);
    }
    return result;
}

bool isDefined(Id id)
{
    return rowFor(static_cast<int>(id)) != nullptr;
}

Id legacyToButtonId(int oldOrdinal)
{
    if (oldOrdinal >= 0 && oldOrdinal < static_cast<int>(kLegacyCore.size())) {
        return static_cast<Id>(kLegacyCore[static_cast<size_t>(oldOrdinal)]);
    }
    const int macro = oldOrdinal - kLegacyMacroBase;
    if (macro >= 0 && macro < kMacroButtonsPerGroup) {
        return static_cast<Id>(static_cast<int>(Id::Macro0) + macro);
    }
    return static_cast<Id>(-1);
}

int buttonIdToLegacy(Id id)
{
    const int value = static_cast<int>(id);
    for (size_t i = 0; i < kLegacyCore.size(); ++i) {
        if (kLegacyCore[i] == value) { return static_cast<int>(i); }
    }
    const int macro = value - static_cast<int>(Id::Macro0);
    if (macro >= 0 && macro < kMacroButtonsPerGroup) { return kLegacyMacroBase + macro; }
    return -1;
}

} // namespace NereusSDR::OtherButtonCatalog
