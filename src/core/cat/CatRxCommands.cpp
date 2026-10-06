// --- From CATCommands.cs ---
//=================================================================
// CATCommands.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
/*
Modifications to support the Behringer Midi controllers
by Chris Codella, W2PA, April 2017.  Indicated by //-W2PA comment lines.
Added extended CAT commands for APF funtions - May 2017.
*/
//=================================================================

// --- From console.cs ---
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
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// Ported from Thetis CAT/CATCommands.cs and console.cs [v2.10.3.15].
// Modification history (NereusSDR):
// 2026-10-04 - Read the service desired global tuple during reconfiguration.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - Stable slice CAT RX commands adapted by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.
#include "CatRxCommands.h"
#include "CatService.h"
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace NereusSDR {
namespace {
// From Thetis CAT/CATCommands.cs:9221-9261 [v2.10.3.15]. StrVFOFreq PadRight11.
constexpr int kFrequencyWidth = 11;
// From Thetis CAT/CATStructs.xml:1039-1066,1601-1621 [v2.10.3.15]. Descriptor facts, no XML logic port.
constexpr int kSignedAnswerWidth = 5;
// From Thetis CAT/CATCommands.cs:317-468 [v2.10.3.15]. Sign plus PadLeft5 magnitude.
constexpr int kStatusOffsetWidth = 6;
// From Thetis CAT/CATCommands.cs:9856-9950,9406-9828 [v2.10.3.15]. Two-digit mode/filter-code tokens.
constexpr int kCodeWidth = 2;
// From Thetis CAT/CATCommands.cs:6247-6257 [v2.10.3.15]. Center substring first4, remaining width.
constexpr int kCenterFieldWidth = 4;
// From Thetis CAT/CATCommands.cs:4209-4242 [v2.10.3.15]. Whole name+index+colon PadLeft7.
constexpr int kModeListFieldWidth = 7;
// From Thetis CAT/CATParser.cs:385-409,1553-1611 [v2.10.3.15]. Error-code fact; native classification, no parser logic port.
constexpr int kFeatureNotAvailable = 7;
CatCommandResult error() { return {CatResultKind::Error, "?;"}; }
CatCommandResult silence() { return {CatResultKind::Silence, {}}; }
CatCommandResult payload(const QByteArray& value) { return {CatResultKind::Payload, value}; }
QByteArray number(qint64 value, int width) { return QByteArray::number(value).rightJustified(width, '0'); }
QByteArray signedNumber(int value, int width) { return (value < 0 ? QByteArray("-") : QByteArray("+")) + number(value < 0 ? -qint64(value) : qint64(value), width - 1); }
//-W2PA Transfer focus to VAR1                                                               
// [original inline comment from CATCommands.cs:2857]
// From Thetis CAT/CATCommands.cs:5904-5940,8208-8243,2850-3011 [v2.10.3.15].
// Nereus defect correction: source sign+AddLeadingZeros(abs).Substring(1)
// loses the leading magnitude digit at10000+. Preserve the actual value and
// report representability failure rather than a false successful signed reading.
// Extended source framing otherwise returns an unmatched payload verbatim;
// this guard deliberately supplies O; without changing descriptor width5.
CatCommandResult signedQuery(int value) {
    const QByteArray answer=signedNumber(value,kSignedAnswerWidth);
    return answer.size() == kSignedAnswerWidth ? payload(answer) : CatCommandResult{CatResultKind::Error,"O;"};
}
// converts Kenwood single digit mode code to SDR mode
// [original inline comment from CATCommands.cs:9952]
// converts SDR mode to Kenwood single digit mode code
// [original inline comment from CATCommands.cs:9995]
//				case DSPMode.SAM:		//possible fix for SAM problem
// [original inline comment from CATCommands.cs:10016]
//Construct an array of the PowerSDR.Band enums.
// [original inline comment from CATCommands.cs:10047]
//If the 2m xverter is present, set the last index to B2M
// [original inline comment from CATCommands.cs:10048]
//otherwise, set it to B6M.
// [original inline comment from CATCommands.cs:10049]
// From Thetis CAT/CATCommands.cs:9856-9950 [v2.10.3.15]. Explicit names: source12/13 are not native RADE.
constexpr std::array<DSPMode, 12> kModes{DSPMode::LSB,DSPMode::USB,DSPMode::DSB,DSPMode::CWL,DSPMode::CWU,DSPMode::FM,DSPMode::AM,DSPMode::DIGU,DSPMode::SPEC,DSPMode::DIGL,DSPMode::SAM,DSPMode::DRM};
// From Thetis CAT/CATCommands.cs:4209-4242 [v2.10.3.15]. Pad entire name/index/colon field.
constexpr std::array<const char*, 12> kModeNames{"LSB","USB","DSB","CWL","CWU","FM","AM","DIGU","SPEC","DIGL","SAM","DRM"};
int modeIndex(DSPMode mode) { const auto found = std::find(kModes.begin(), kModes.end(), mode); return found == kModes.end() ? -1 : int(found-kModes.begin()); }
//[2.10.3.9]MW0LGE refactored, and tweaked for special cases to match original
// [original inline comment from CATCommands.cs:10388]
// From Thetis CAT/CATCommands.cs:10329-10384 [v2.10.3.15]. MHz converted to native integer Hz.
constexpr std::array<int,16> kExplicitSteps{1,10,25,50,100,250,500,1000,5000,9000,10000,100000,250000,500000,1000000,10000000};
//[2.10.3.9]MW0LGE refactored, and tweaked for special cases to match original
// [original inline comment from CATCommands.cs:10388]
// From Thetis CAT/CATCommands.cs:10386-10486 [v2.10.3.15].
//[2.10.3.9]MW0LGE refactored, and tweaked for special cases to match original
// [original inline comment from CATCommands.cs:10388]
// 10e0 (1Hz)
// [original inline comment from CATCommands.cs:10391]
// 10e0 (2Hz)
// [original inline comment from CATCommands.cs:10392]
// 10e1 (10Hz)
// [original inline comment from CATCommands.cs:10393]
// 10e1 (25Hz)
// [original inline comment from CATCommands.cs:10394]
//"0001", // 10e1 (50Hz)
// [original inline comment from CATCommands.cs:10395]
// 10e2 (100Hz)
// [original inline comment from CATCommands.cs:10396]
//"0010", // 10e2 (250Hz)
// [original inline comment from CATCommands.cs:10397]
//"0010", // 10e2 (500Hz)
// [original inline comment from CATCommands.cs:10398]
// 10e3 (1kHz)
// [original inline comment from CATCommands.cs:10399]
// 10e3 (2kHz)
// [original inline comment from CATCommands.cs:10400]
// 10e3 (2.5kHz)
// [original inline comment from CATCommands.cs:10401]
//"0011", // 10e3 (5kHz)
// [original inline comment from CATCommands.cs:10402]
// 10e3 (6.25kHz)
// [original inline comment from CATCommands.cs:10403]
//"0011", // 10e3 (9kHz)
// [original inline comment from CATCommands.cs:10404]
// 10e4 (10kHz)
// [original inline comment from CATCommands.cs:10405]
// 10e4 (12.5kHz)
// [original inline comment from CATCommands.cs:10406]
// 10e4 (15kHz)
// [original inline comment from CATCommands.cs:10407]
// 10e4 (20kHz)
// [original inline comment from CATCommands.cs:10408]
// 10e4 (25kHz)
// [original inline comment from CATCommands.cs:10409]
// 10e4 (30kHz)
// [original inline comment from CATCommands.cs:10410]
// 10e4 (50kHz)
// [original inline comment from CATCommands.cs:10411]
// 10e5 (100kHz)
// [original inline comment from CATCommands.cs:10412]
//"0101", // 10e5 (250kHz)
// [original inline comment from CATCommands.cs:10413]
//"0101", // 10e5 (500kHz)
// [original inline comment from CATCommands.cs:10414]
// 10e6 (1MHz)
// [original inline comment from CATCommands.cs:10415]
// 10e7 (10MHz)
// [original inline comment from CATCommands.cs:10416]
// some default
// [original inline comment from CATCommands.cs:10420]
//// Modified 2/25/07 to accomodate changes to console where odd step sizes added.  BT
// [original inline comment from CATCommands.cs:10424]
//string stepval = "";
// [original inline comment from CATCommands.cs:10425]
//int step = pSize;
// [original inline comment from CATCommands.cs:10426]
//switch(step)
// [original inline comment from CATCommands.cs:10427]
//{
// [original inline comment from CATCommands.cs:10428]
//	case 0:
// [original inline comment from CATCommands.cs:10429]
//		stepval = "0000";	//10e0 = 1 hz
// [original inline comment from CATCommands.cs:10430]
//		break;
// [original inline comment from CATCommands.cs:10431]
//	case 1:
// [original inline comment from CATCommands.cs:10432]
//		stepval = "0001";	//10e1 = 10 hz
// [original inline comment from CATCommands.cs:10433]
//		break;
// [original inline comment from CATCommands.cs:10434]
//	case 2:
// [original inline comment from CATCommands.cs:10435]
//		stepval = "1000";	//special default for 50 hz
// [original inline comment from CATCommands.cs:10436]
//		break;
// [original inline comment from CATCommands.cs:10437]
//	case 3:
// [original inline comment from CATCommands.cs:10438]
//		stepval = "0010";	//10e2 = 100 hz
// [original inline comment from CATCommands.cs:10439]
//		break;
// [original inline comment from CATCommands.cs:10440]
//	case 4:
// [original inline comment from CATCommands.cs:10441]
//		stepval = "1001";	//special default for 250 hz
// [original inline comment from CATCommands.cs:10442]
//		break;
// [original inline comment from CATCommands.cs:10443]
//	case 5:
// [original inline comment from CATCommands.cs:10444]
//		stepval = "1010";	//10e3 = 1 kHz default for 500 hz
// [original inline comment from CATCommands.cs:10445]
//		break;
// [original inline comment from CATCommands.cs:10446]
//	case 6:
// [original inline comment from CATCommands.cs:10447]
//		stepval = "0011";	//10e3 = 1 kHz
// [original inline comment from CATCommands.cs:10448]
//		break;
// [original inline comment from CATCommands.cs:10449]
//	case 7:
// [original inline comment from CATCommands.cs:10450]
//		stepval = "1011";	//special default for 5 kHz
// [original inline comment from CATCommands.cs:10451]
//		break;
// [original inline comment from CATCommands.cs:10452]
//	case 8:
// [original inline comment from CATCommands.cs:10453]
//		stepval = "1100";	//special default for 9 kHz
// [original inline comment from CATCommands.cs:10454]
//		break;
// [original inline comment from CATCommands.cs:10455]
//	case 9:
// [original inline comment from CATCommands.cs:10456]
//		stepval = "0100";	//10e4 = 10 khZ
// [original inline comment from CATCommands.cs:10457]
//		break;
// [original inline comment from CATCommands.cs:10458]
//	case 10:
// [original inline comment from CATCommands.cs:10459]
//		stepval = "0101";	//10e5 = 100 kHz
// [original inline comment from CATCommands.cs:10460]
//		break;
// [original inline comment from CATCommands.cs:10461]
//	case 11:
// [original inline comment from CATCommands.cs:10462]
//	stepval = "1101";   //special default for 250 kHz
// [original inline comment from CATCommands.cs:10463]
//	break;
// [original inline comment from CATCommands.cs:10464]
//case 12:
// [original inline comment from CATCommands.cs:10465]
//	stepval = "1110";   //special default for 500 kHz
// [original inline comment from CATCommands.cs:10466]
//	break;
// [original inline comment from CATCommands.cs:10467]
//	case 13:
// [original inline comment from CATCommands.cs:10468]
//		stepval = "0110";	//10e6 = 1 mHz
// [original inline comment from CATCommands.cs:10469]
//		break;
// [original inline comment from CATCommands.cs:10470]
//	case 14:
// [original inline comment from CATCommands.cs:10471]
//		stepval = "0111";	//10e7 = 10 mHz
// [original inline comment from CATCommands.cs:10472]
//		break;
// [original inline comment from CATCommands.cs:10473]
//}
// [original inline comment from CATCommands.cs:10474]
//return stepval;
// [original inline comment from CATCommands.cs:10475]
constexpr std::array<const char*,26> kStepStrings{"0000","0000","0001","0001","1000","0010","1001","1010","0011","0011","0011","1011","0011","1100","0100","0100","0100","0100","0100","0100","0100","0101","1101","1110","0110","0111"};
// From Thetis CAT/CATCommands.cs:5904-5940,8208-8243 [v2.10.3.15].
constexpr int kOffsetLimit = 99999;
//-W2PA Transfer focus to VAR1                                                               
// [original inline comment from CATCommands.cs:2857]
// From Thetis CAT/CATCommands.cs:2850-2918,2944-3011 [v2.10.3.15].
constexpr int kEdgeLimit = 10000;
// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off
// [original inline comment from CATCommands.cs:5883]
// From Thetis CAT/CATCommands.cs:5877-5904,6107-6130,8181-8208,8393-8420 [v2.10.3.15].
constexpr int kOffsetStep = 10;
// From Thetis CAT/CATCommands.cs:10076-10324 [v2.10.3.15]. Supported native bands, absent transverter slots omitted.
constexpr std::array<Band,14> kBands{Band::GEN,Band::Band160m,Band::Band80m,Band::Band60m,Band::Band40m,Band::Band30m,Band::Band20m,Band::Band17m,Band::Band15m,Band::Band12m,Band::Band10m,Band::Band6m,Band::Band2m,Band::WWV};
constexpr std::array<const char*,14> kBandTokens{"888","160","080","060","040","030","020","017","015","012","010","006","002","999"};
QByteArray kenwoodMode(DSPMode mode, bool digitalSideband) {
    // From Thetis CAT/CATCommands.cs:9953-10050 [v2.10.3.15].
    switch(mode) {
    case DSPMode::LSB: return "1"; case DSPMode::USB: return "2"; case DSPMode::CWU: return "3";
    case DSPMode::FM: return "4"; case DSPMode::AM: return "5";
    case DSPMode::DIGL: return digitalSideband ? "1" : "6"; case DSPMode::CWL: return "7";
    case DSPMode::DIGU: return digitalSideband ? "2" : "9"; default: return {};
    }
}
int rttyOffset(const SliceModel& slice, CatVfo vfo, const CatGlobalConfig& config) {
// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0 as we might not be
// [original inline comment from CATCommands.cs:2665]
    // From Thetis CAT/CATCommands.cs:2653-2753 [v2.10.3.15].
    if (!(vfo == CatVfo::Primary ? config.rttyOffsetAEnabled : config.rttyOffsetBEnabled)) { return 0; }
    if (slice.dspMode() == DSPMode::DIGU) { return config.rttyDiguHz; }
    if (slice.dspMode() == DSPMode::DIGL) { return -config.rttyDiglHz; }
    return 0;
}
bool doubleSideband(DSPMode mode) {
    return mode == DSPMode::AM || mode == DSPMode::DSB || mode == DSPMode::DRM || mode == DSPMode::FM || mode == DSPMode::SAM;
}
// need to get current values 
// [original inline comment from CATCommands.cs:9368]
// not implemented yet 
// [original inline comment from CATCommands.cs:9370]
// Debug.WriteLine("center: " + center  + " width: " + width); 
// [original inline comment from CATCommands.cs:9374]
// new_lo and new_hi calculated assuming a USB mode .. do the right thing 
// [original inline comment from CATCommands.cs:9380]
// for lsb and other modes 
// [original inline comment from CATCommands.cs:9381]
// fixme -- needs more thinking 
// [original inline comment from CATCommands.cs:9382]
// System.Console.WriteLine("zzsf: " + new_lo + " " + new_hi); 
// [original inline comment from CATCommands.cs:9398]
// Converts interger filter frequency into Kenwood SL/SH codes
// [original inline comment from CATCommands.cs:9405]
// Converts a frequency code pair to frequency in hz according to
// [original inline comment from CATCommands.cs:9505]
// the Kenwood TS-2000 spec.  Receives code and calling methd as parameters
// [original inline comment from CATCommands.cs:9506]
// Get the current console mode
// [original inline comment from CATCommands.cs:9513]
// Get the frequency group(SSB/SL, SSB/SH, DSB/SL, and DSB/SH)
// [original inline comment from CATCommands.cs:9527]
// return the frequency for the current DSP mode and calling method
// [original inline comment from CATCommands.cs:9543]
//SL SSB
// [original inline comment from CATCommands.cs:9546]
//SH SSB
// [original inline comment from CATCommands.cs:9587]
//SL DSB
// [original inline comment from CATCommands.cs:9628]
//SH DSB
// [original inline comment from CATCommands.cs:9645]
//			int freq = 0;
// [original inline comment from CATCommands.cs:9666]
//			switch(console.RX1DSPMode)
// [original inline comment from CATCommands.cs:9667]
//			{
// [original inline comment from CATCommands.cs:9668]
//				case DSPMode.CWL:
// [original inline comment from CATCommands.cs:9669]
//				case DSPMode.CWU:
// [original inline comment from CATCommands.cs:9670]
//				case DSPMode.LSB:
// [original inline comment from CATCommands.cs:9671]
//				case DSPMode.USB:
// [original inline comment from CATCommands.cs:9672]
//				{
// [original inline comment from CATCommands.cs:9673]
//					switch(c)	//c = filter code, n = SH or SL
// [original inline comment from CATCommands.cs:9674]
//					{
// [original inline comment from CATCommands.cs:9675]
//						case "00":
// [original inline comment from CATCommands.cs:9676]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9677]
//								freq = 10;
// [original inline comment from CATCommands.cs:9678]
//							else
// [original inline comment from CATCommands.cs:9679]
//								freq = 1400;
// [original inline comment from CATCommands.cs:9680]
//							break;
// [original inline comment from CATCommands.cs:9681]
//						case "01":
// [original inline comment from CATCommands.cs:9682]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9683]
//								freq = 50;
// [original inline comment from CATCommands.cs:9684]
//							else
// [original inline comment from CATCommands.cs:9685]
//								freq = 1600;
// [original inline comment from CATCommands.cs:9686]
//							break;
// [original inline comment from CATCommands.cs:9687]
//						case "02":
// [original inline comment from CATCommands.cs:9688]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9689]
//								freq = 100;
// [original inline comment from CATCommands.cs:9690]
//							else
// [original inline comment from CATCommands.cs:9691]
//								freq = 1800;
// [original inline comment from CATCommands.cs:9692]
//							break;
// [original inline comment from CATCommands.cs:9693]
//						case "03":
// [original inline comment from CATCommands.cs:9694]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9695]
//								freq = 200;
// [original inline comment from CATCommands.cs:9696]
//							else
// [original inline comment from CATCommands.cs:9697]
//								freq = 2000;
// [original inline comment from CATCommands.cs:9698]
//							break;
// [original inline comment from CATCommands.cs:9699]
//						case "04":
// [original inline comment from CATCommands.cs:9700]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9701]
//								freq = 300;
// [original inline comment from CATCommands.cs:9702]
//							else
// [original inline comment from CATCommands.cs:9703]
//								freq = 2200;
// [original inline comment from CATCommands.cs:9704]
//							break;
// [original inline comment from CATCommands.cs:9705]
//						case "05":
// [original inline comment from CATCommands.cs:9706]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9707]
//								freq = 400;
// [original inline comment from CATCommands.cs:9708]
//							else
// [original inline comment from CATCommands.cs:9709]
//								freq = 2400;
// [original inline comment from CATCommands.cs:9710]
//							break;
// [original inline comment from CATCommands.cs:9711]
//						case "06":
// [original inline comment from CATCommands.cs:9712]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9713]
//								freq = 500;
// [original inline comment from CATCommands.cs:9714]
//							else
// [original inline comment from CATCommands.cs:9715]
//								freq = 2600;
// [original inline comment from CATCommands.cs:9716]
//							break;
// [original inline comment from CATCommands.cs:9717]
//						case "07":
// [original inline comment from CATCommands.cs:9718]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9719]
//								freq = 600;
// [original inline comment from CATCommands.cs:9720]
//							else
// [original inline comment from CATCommands.cs:9721]
//								freq = 2800;
// [original inline comment from CATCommands.cs:9722]
//							break;
// [original inline comment from CATCommands.cs:9723]
//						case "08":
// [original inline comment from CATCommands.cs:9724]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9725]
//								freq = 700;
// [original inline comment from CATCommands.cs:9726]
//							else
// [original inline comment from CATCommands.cs:9727]
//								freq = 3000;
// [original inline comment from CATCommands.cs:9728]
//							break;
// [original inline comment from CATCommands.cs:9729]
//						case "09":
// [original inline comment from CATCommands.cs:9730]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9731]
//								freq = 800;
// [original inline comment from CATCommands.cs:9732]
//							else
// [original inline comment from CATCommands.cs:9733]
//								freq = 3400;
// [original inline comment from CATCommands.cs:9734]
//							break;
// [original inline comment from CATCommands.cs:9735]
//						case "10":
// [original inline comment from CATCommands.cs:9736]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9737]
//								freq = 900;
// [original inline comment from CATCommands.cs:9738]
//							else
// [original inline comment from CATCommands.cs:9739]
//								freq = 4000;
// [original inline comment from CATCommands.cs:9740]
//							break;
// [original inline comment from CATCommands.cs:9741]
//						case "11":
// [original inline comment from CATCommands.cs:9742]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9743]
//								freq = 1000;
// [original inline comment from CATCommands.cs:9744]
//							else
// [original inline comment from CATCommands.cs:9745]
//								freq = 5000;
// [original inline comment from CATCommands.cs:9746]
//							break;
// [original inline comment from CATCommands.cs:9747]
//						default:
// [original inline comment from CATCommands.cs:9748]
//							break;
// [original inline comment from CATCommands.cs:9749]
//					}
// [original inline comment from CATCommands.cs:9750]
//					break;
// [original inline comment from CATCommands.cs:9751]
//				}
// [original inline comment from CATCommands.cs:9752]
//				case DSPMode.AM:
// [original inline comment from CATCommands.cs:9753]
//				case DSPMode.DRM:
// [original inline comment from CATCommands.cs:9754]
//				case DSPMode.DSB:
// [original inline comment from CATCommands.cs:9755]
//				case DSPMode.FMN:
// [original inline comment from CATCommands.cs:9756]
//				case DSPMode.SAM:
// [original inline comment from CATCommands.cs:9757]
//				{
// [original inline comment from CATCommands.cs:9758]
//					switch(c)
// [original inline comment from CATCommands.cs:9759]
//					{
// [original inline comment from CATCommands.cs:9760]
//						case "00":
// [original inline comment from CATCommands.cs:9761]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9762]
//								freq = 10;
// [original inline comment from CATCommands.cs:9763]
//							else
// [original inline comment from CATCommands.cs:9764]
//								freq = 2500;
// [original inline comment from CATCommands.cs:9765]
//							break;
// [original inline comment from CATCommands.cs:9766]
//						case "01":
// [original inline comment from CATCommands.cs:9767]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9768]
//								freq = 100;
// [original inline comment from CATCommands.cs:9769]
//							else
// [original inline comment from CATCommands.cs:9770]
//								freq = 3000;
// [original inline comment from CATCommands.cs:9771]
//							break;
// [original inline comment from CATCommands.cs:9772]
//						case "02":
// [original inline comment from CATCommands.cs:9773]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9774]
//								freq = 200;
// [original inline comment from CATCommands.cs:9775]
//							else
// [original inline comment from CATCommands.cs:9776]
//								freq = 4000;
// [original inline comment from CATCommands.cs:9777]
//							break;
// [original inline comment from CATCommands.cs:9778]
//						case "03":
// [original inline comment from CATCommands.cs:9779]
//							if(n == "SL")
// [original inline comment from CATCommands.cs:9780]
//								freq = 500;
// [original inline comment from CATCommands.cs:9781]
//							else
// [original inline comment from CATCommands.cs:9782]
//								freq = 5000;
// [original inline comment from CATCommands.cs:9783]
//							break;
// [original inline comment from CATCommands.cs:9784]
//					}
// [original inline comment from CATCommands.cs:9785]
//				}
// [original inline comment from CATCommands.cs:9786]
//				break;
// [original inline comment from CATCommands.cs:9787]
//			}
// [original inline comment from CATCommands.cs:9788]
//			return freq;
// [original inline comment from CATCommands.cs:9789]
// c = code, n = SH or SL
// [original inline comment from CATCommands.cs:9795]
//split the bandwidth at the center frequency
// [original inline comment from CATCommands.cs:9807]
// get the upper limit from the lower value set
// [original inline comment from CATCommands.cs:9815]
// since we need the more positive value
// [original inline comment from CATCommands.cs:9816]
// closest to the center freq in lsb modes
// [original inline comment from CATCommands.cs:9817]
// do the reverse here, the less positive value
// [original inline comment from CATCommands.cs:9820]
// is away from the center freq
// [original inline comment from CATCommands.cs:9821]
// Set the bandwith equally across the center freq
// [original inline comment from CATCommands.cs:9831]
// reset the frequency to the nominal value (in case it's been changed)
// [original inline comment from CATCommands.cs:9838]
// subtract the SL value from the lower half of the bandwidth
// [original inline comment from CATCommands.cs:9844]
// From Thetis CAT/CATCommands.cs:9406-9828 [v2.10.3.15]. Exact code tables and lookup limits.
constexpr std::array<int,12> kSsbLow{0,50,100,200,300,400,500,600,700,800,900,1000};
constexpr std::array<int,12> kSsbHigh{1400,1600,1800,2000,2200,2400,2600,2800,3000,3400,4000,5000};
constexpr std::array<int,4> kDsbLow{0,100,200,500};
constexpr std::array<int,4> kDsbHigh{2500,3000,4000,5000};
constexpr std::array<int,11> kSsbLowLimits{25,75,150,250,350,450,550,650,750,850,950};
constexpr std::array<int,11> kSsbHighLimits{1500,1700,1900,2100,2300,2500,2700,2900,3200,3700,4500};
constexpr std::array<int,3> kDsbLowLimits{50,150,350};
constexpr std::array<int,3> kDsbHighLimits{2750,3500,4500};
int frequencyCode(int hz, bool high, bool dsb) {
    const qint64 magnitude = hz < 0 ? -qint64(hz) : qint64(hz);
    if (dsb) { const auto& limits= high ? kDsbHighLimits : kDsbLowLimits; return int(std::lower_bound(limits.begin(),limits.end(),magnitude)-limits.begin()); }
    const auto& limits=high ? kSsbHighLimits : kSsbLowLimits; return int(std::lower_bound(limits.begin(),limits.end(),magnitude)-limits.begin());
}
int codeFrequency(int code, bool high, bool dsb) {
    if (dsb) { const auto& table=high ? kDsbHigh : kDsbLow; return code >= 0 && code < int(table.size()) ? table[code] : 0; }
    const auto& table=high ? kSsbHigh : kSsbLow; return code >= 0 && code < int(table.size()) ? table[code] : 0;
}
} // namespace
CatRxCommands::CatRxCommands(CatModelAdapter& adapter, CatTxCoordinator& tx, CatSettings& settings)
    : m_adapter(adapter), m_txCoordinator(tx), m_settings(settings) {}
QList<QByteArray> CatRxCommands::codes() { return {"BD","BU","DN","FA","FB","FR","FT","IF","MD","QI","RC","RD","RT","RU","SH","SL","UP","XT","ZZAC","ZZAD","ZZAE","ZZAF","ZZAU","ZZBA","ZZBB","ZZBD","ZZBE","ZZBF","ZZBG","ZZBM","ZZBP","ZZBS","ZZBT","ZZBU","ZZCN","ZZCO","ZZDA","ZZDM","ZZDN","ZZDO","ZZDP","ZZDQ","ZZDR","ZZFA","ZZFB","ZZFH","ZZFI","ZZFJ","ZZFL","ZZFR","ZZFS","ZZFT","ZZIF","ZZIS","ZZIT","ZZIU","ZZMD","ZZME","ZZML","ZZMN","ZZPD","ZZPE","ZZPO","ZZPY","ZZPZ","ZZQM","ZZQR","ZZQS","ZZRA","ZZRB","ZZRC","ZZRD","ZZRF","ZZRH","ZZRL","ZZRT","ZZRU","ZZSA","ZZSB","ZZSD","ZZSF","ZZSG","ZZSH","ZZSP","ZZST","ZZSU","ZZSW","ZZSY","ZZSZ","ZZTV","ZZVS","ZZXC","ZZXD","ZZXF","ZZXS","ZZXU","ZZXV","ZZYR","ZZZT"}; }
CatCommandResult CatRxCommands::execute(const CatRequest& request, CatSessionContext& context)
{
    // Nereus adaptation: capture the actual session's incarnation binding, never endpoint config or GUI focus.
    const QPointer<RadioModel> model(&m_adapter.radioModel());
    const QPointer<CatService> service(model->catService());
    const CatSession* session = service ? service->session(context.sessionId) : nullptr;
    if (!session) { return error(); }
    const CatBinding binding=session->binding();
    const QByteArray code=request.code;
    const bool get=request.form == CatForm::Get;
    bool parsed=false; const qint64 input=request.suffix.toLongLong(&parsed);
    const CatGlobalConfig global=service->globalConfig();
    // From Thetis CAT/CATCommands.cs:223-237 [v2.10.3.15]. Source-inert receiver selector.
    if (code == "FR") { return get ? payload("0") : silence(); }
    // From Thetis CAT/CATCommands.cs:4209-4242 [v2.10.3.15]. Common modes only; AM sidebands unsupported.
    if (code == "ZZML") {
        if (!get) { return error(); }
        QByteArray list; for (int index=0; index<int(kModes.size()); ++index) { if (index) { list += ':'; } list += (QByteArray(kModeNames[index])+number(index,kCodeWidth)+':').rightJustified(kModeListFieldWidth,' ').chopped(1); }
        return payload(list);
    }
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:5965]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:6001]
    // From Thetis CAT/CATCommands.cs:5820-5870,5940-6012 [v2.10.3.15]. Runtime service preferences.
    if (code == "ZZRA" || code == "ZZRB" || code == "ZZRH" || code == "ZZRL") {
        CatGlobalConfig config=global;
        if (get) {
            if (code == "ZZRA") { return payload(config.rttyOffsetAEnabled ? "1" : "0"); }
            if (code == "ZZRB") { return payload(config.rttyOffsetBEnabled ? "1" : "0"); }
            return payload(signedNumber(code == "ZZRH" ? config.rttyDiguHz : config.rttyDiglHz,kSignedAnswerWidth));
        }
        if (code == "ZZRA" || code == "ZZRB") {
            if (request.suffix != "0" && request.suffix != "1") { return silence(); }
            if (code == "ZZRA") { config.rttyOffsetAEnabled=input == 1; } else { config.rttyOffsetBEnabled=input == 1; }
        } else {
            if (!parsed) { return error(); }
            const int offset=int(std::clamp(input,qint64(CatDefaults::kRttyMinimumHz),qint64(CatDefaults::kRttyMaximumHz)));
            if (code == "ZZRH") { config.rttyDiguHz=offset; } else { config.rttyDiglHz=offset; }
        }
        return service->applyGlobalConfig(config) ? silence() : error();
    }
    // Explicit unavailable model/UI capabilities: no cached or fictitious getters.
    const QList<QByteArray> unavailable{"QI","ZZVS","ZZFI","ZZIS","ZZDM","ZZQR","ZZDA","ZZBG","ZZQM","ZZIT","ZZIU","ZZXV","ZZQS","ZZPD","ZZPZ","ZZPO","ZZMN","ZZFJ","ZZTV","ZZSY","ZZPY","ZZPE","ZZDP","ZZDQ","ZZDR","ZZDN","ZZDO","ZZYR","ZZZT"};
    if (unavailable.contains(code)) { return {CatResultKind::Error,"?;",kFeatureNotAvailable}; }
    CatVfo vfo=CatVfo::Primary;
    const QList<QByteArray> secondary{"FB","ZZFB","ZZME","ZZFR","ZZFS","ZZBT","ZZBA","ZZBB","ZZBE","ZZBF","ZZBM","ZZBP","ZZCO","ZZSG","ZZSH"};
    if (secondary.contains(code) || (code == "ZZSZ" && request.suffix != "0")) { vfo=CatVfo::Secondary; }
    const QPointer<SliceModel> primary(m_adapter.resolveSlice(binding,CatVfo::Primary));
    const QPointer<SliceModel> second(m_adapter.resolveSlice(binding,CatVfo::Secondary));
    const auto selectedVfo = [&]() -> std::optional<CatVfo> {
        if (model->txSliceArbiter()->isHandoffPending()) { return {}; }
        const SliceModel* tx=model->txBoundSlice();
        if (primary && tx == primary) { return CatVfo::Primary; }
        if (second && tx == second) { return CatVfo::Secondary; }
        return {};
    };
//			if(s.Length == parser.nSet)
// [original inline comment from CATCommands.cs:239]
//			{
// [original inline comment from CATCommands.cs:240]
//				if(s == "1")
// [original inline comment from CATCommands.cs:241]
//				{
// [original inline comment from CATCommands.cs:242]
//					console.VFOSplit = true;
// [original inline comment from CATCommands.cs:243]
//				}
// [original inline comment from CATCommands.cs:244]
//				else if(s == "0")
// [original inline comment from CATCommands.cs:245]
//				{
// [original inline comment from CATCommands.cs:246]
//					console.VFOSplit = false;
// [original inline comment from CATCommands.cs:247]
//				}
// [original inline comment from CATCommands.cs:248]
//				return "";
// [original inline comment from CATCommands.cs:249]
//			}
// [original inline comment from CATCommands.cs:250]
//			else if(s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:251]
//			{
// [original inline comment from CATCommands.cs:252]
//				return ZZSP(s);
// [original inline comment from CATCommands.cs:253]
//			}
// [original inline comment from CATCommands.cs:254]
//			else
// [original inline comment from CATCommands.cs:255]
//				return parser.Error1;
// [original inline comment from CATCommands.cs:256]
    // From Thetis CAT/CATCommands.cs:6353-6383,6507-6534 [v2.10.3.15]. Actual confirmed TX binding.
    if (code == "FT" || code == "ZZSP" || code == "ZZSW") {
        const std::optional<CatVfo> selection=selectedVfo();
        if (get) { return selection && m_adapter.mayRead(binding,*selection) ? payload(*selection == CatVfo::Primary ? "0" : "1") : error(); }
        if (request.suffix != "0" && request.suffix != "1") { return error(); }
        if (code == "ZZSW" && request.suffix == "0") { return silence(); }
        if (code == "ZZSW" && !selection) { return error(); }
        const CatVfo target = code == "ZZSW" ? (*selection == CatVfo::Primary ? CatVfo::Secondary : CatVfo::Primary) : (input == 1 ? CatVfo::Secondary : CatVfo::Primary);
        const QPointer<SliceModel> slice(m_adapter.resolveSlice(binding,target));
        if (!context.transmitAllowed || !slice || !m_adapter.mayChange(binding,target,"txSelection")) { return error(); }
        const bool accepted=m_txCoordinator.requestTxSelection(context.sessionId,slice->sliceIndex());
        if (!model || !service || !slice || !service->session(context.sessionId)) { return error(); }
        return accepted && selectedVfo() == target ? silence() : error();
    }
    const bool txCommand=code == "XT" || code == "ZZXS" || code == "ZZXF" || code == "ZZXC" || code == "ZZXD" || code == "ZZXU" || code == "ZZFT";
    if (txCommand) { const auto selection=selectedVfo(); if (!selection) { return error(); } vfo=*selection; }
    const QPointer<SliceModel> slice(m_adapter.resolveSlice(binding,vfo));
    if (!slice) { return error(); }
    const auto readable=[&]() { return m_adapter.mayRead(binding,vfo); };
    const auto token=[&](const QByteArray& property) { return m_adapter.prepareWrite(binding,vfo,property); };
    const auto stillValid=[&](const CatWriteToken& write) { return model && service && slice && service->session(context.sessionId) && m_adapter.revalidateWrite(write); };
    const auto setFrequency=[&](double hz) {
        const CatWriteToken write=token("frequency");
        if (!stillValid(write) || slice->locked() || !std::isfinite(hz) || hz < 0 || hz > SliceModel::kMaxReceiveFrequencyHz) { return error(); }
        slice->setFrequency(hz);
        return stillValid(write) && slice->frequency() == hz ? silence() : error();
    };
//			if(s.Length == parser.nSet)
// [original inline comment from CATCommands.cs:191]
//			{
// [original inline comment from CATCommands.cs:192]
//				s = s.Insert(5,separator);		//reinsert the global numeric separator
// [original inline comment from CATCommands.cs:193]
//				console.VFOAFreq = double.Parse(s);
// [original inline comment from CATCommands.cs:194]
//				return "";
// [original inline comment from CATCommands.cs:195]
//			}
// [original inline comment from CATCommands.cs:196]
//			else if(s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:197]
//				return StrVFOFreq("A");
// [original inline comment from CATCommands.cs:198]
//			else
// [original inline comment from CATCommands.cs:199]
//				return parser.Error1;
// [original inline comment from CATCommands.cs:200]
//			if(s.Length == parser.nSet)
// [original inline comment from CATCommands.cs:207]
//			{
// [original inline comment from CATCommands.cs:208]
//				s = s.Insert(5,separator);
// [original inline comment from CATCommands.cs:209]
//				console.VFOBFreq = double.Parse(s);
// [original inline comment from CATCommands.cs:210]
//				return "";
// [original inline comment from CATCommands.cs:211]
//			}
// [original inline comment from CATCommands.cs:212]
//			else if(s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:213]
//				return StrVFOFreq("B");
// [original inline comment from CATCommands.cs:214]
//			else
// [original inline comment from CATCommands.cs:215]
//				return parser.Error1;
// [original inline comment from CATCommands.cs:216]
// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0 as we might not be
// [original inline comment from CATCommands.cs:2665]
// parsing anything here, ie being called directly from midi
// [original inline comment from CATCommands.cs:2666]
// Changed to pass optional padding length, use 11
// [original inline comment from CATCommands.cs:2667]
// MW0LGE changed to take into consideration the flag
// [original inline comment from CATCommands.cs:2673]
//[2.10.3.12]MW0LGE might be a good idea to use RX2 mode if RX2 enabled.
// [original inline comment from CATCommands.cs:2706]
//probably other places in this cat command code with similar issues
// [original inline comment from CATCommands.cs:2707]
// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0, as we might not be
// [original inline comment from CATCommands.cs:2716]
// parsing anything here, ie being called directly from midi
// [original inline comment from CATCommands.cs:2717]
// Changed to pass optional padding length, use 11
// [original inline comment from CATCommands.cs:2718]
// MW0LGE changed to take into consideration the flag
// [original inline comment from CATCommands.cs:2724]
//[2.10.3.12]MW0LGE as above
// [original inline comment from CATCommands.cs:2734]
// [2.10.3.12]MW0LGE -- addleadingzeros uses parser.nAns which will be 0 as we might not be
// [original inline comment from CATCommands.cs:2665]
    // From Thetis CAT/CATCommands.cs:189-223,2653-2796 [v2.10.3.15]. Native Hz and inverse wire offset.
    if (code == "FA" || code == "FB" || code == "ZZFA" || code == "ZZFB") {
        const int offset=rttyOffset(*slice,vfo,global);
        if (get) { return readable() ? payload(number(qint64(std::nearbyint(slice->frequency()+offset)),kFrequencyWidth)) : error(); }
        return parsed ? setFrequency(double(input-offset)) : error();
    }
    if (code == "ZZFT") { return get && readable() ? payload(number(model->txFrequencyForSlice(slice),kFrequencyWidth)) : error(); }
    // From Thetis CAT/CATCommands.cs:566-594,3949-4023,9856-10050 [v2.10.3.15]. Explicit mode mappings.
    if (code == "MD" || code == "ZZMD" || code == "ZZME") {
        if (get) {
            if (!readable()) { return error(); }
            const int index=modeIndex(slice->dspMode());
            const QByteArray reply=code == "MD" ? kenwoodMode(slice->dspMode(),global.digitalReportsSideband) : (index < 0 ? QByteArray() : number(index,kCodeWidth));
            return reply.isEmpty() ? error() : payload(reply);
        }
        if (!parsed && code != "ZZME") { return error(); }
        if (!parsed && code == "ZZME") { return silence(); }
        DSPMode mode=DSPMode::USB;
        if (code != "MD") {
            if (code == "ZZME" && request.suffix == "08") { return error(); }
            if (code == "ZZMD" && (input < 0 || input >= qint64(kModes.size()))) { return error(); }
            if (input < 0 || input >= qint64(kModes.size()) || request.suffix != number(input,kCodeWidth)) { return silence(); }
            mode=kModes[input];
        }
        else {
            if (input < 1 || input > 9) { return error(); }
            switch(input) {
            case 1: mode=global.digitalReportsSideband ? DSPMode::DIGL : DSPMode::LSB; break;
            case 2: mode=global.digitalReportsSideband ? DSPMode::DIGU : DSPMode::USB; break;
            case 3: mode=DSPMode::CWU; break; case 4: mode=DSPMode::FM; break; case 5: mode=DSPMode::AM; break;
            case 6: mode=DSPMode::DIGL; break; case 7: mode=DSPMode::CWL; break; case 9: mode=DSPMode::DIGU; break; default: break;
            }
        }
        const CatWriteToken write=token("dspMode"); if (!stillValid(write)) { return error(); }
        slice->setDspMode(mode); return stillValid(write) && slice->dspMode() == mode ? silence() : error();
    }
//string temp;
// [original inline comment from CATCommands.cs:331]
// Get the rit/xit status
// [original inline comment from CATCommands.cs:333]
// Get the incremental tuning value for whichever control is selected
// [original inline comment from CATCommands.cs:338]
// Format the IT value
// [original inline comment from CATCommands.cs:343]
// Get the rx - tx status
// [original inline comment from CATCommands.cs:348]
// Get the step size
// [original inline comment from CATCommands.cs:351]
// Get the vfo split status
// [original inline comment from CATCommands.cs:354]
//Get the mode
// [original inline comment from CATCommands.cs:359]
//			temp = Mode2KString(console.RX1DSPMode);   //possible fix for SAM problem
// [original inline comment from CATCommands.cs:360]
//			if(temp == parser.Error1)
// [original inline comment from CATCommands.cs:361]
//				temp = " ";
// [original inline comment from CATCommands.cs:362]
//			rtn += StrVFOFreq("A");						// VFO A frequency			11 bytes
// [original inline comment from CATCommands.cs:370]
// Console step frequency	 4 bytes
// [original inline comment from CATCommands.cs:371]
// incremental tuning value	 6 bytes
// [original inline comment from CATCommands.cs:372]
// RIT status				 1 byte
// [original inline comment from CATCommands.cs:373]
// XIT status				 1 byte
// [original inline comment from CATCommands.cs:374]
// dummy for memory bank	 3 bytes
// [original inline comment from CATCommands.cs:375]
// tx-rx status				 1 byte
// [original inline comment from CATCommands.cs:376]
//			rtn += temp;
// [original inline comment from CATCommands.cs:377]
//			rtn += Mode2KString(console.RX1DSPMode);	// current mode			 1 bytes
// [original inline comment from CATCommands.cs:378]
// dummy for FR/FT			 1 byte
// [original inline comment from CATCommands.cs:384]
// dummy for scan status	 1 byte
// [original inline comment from CATCommands.cs:385]
// VFO Split status			 1 byte
// [original inline comment from CATCommands.cs:386]
// dummy for the balance	 4 bytes
// [original inline comment from CATCommands.cs:387]
// total bytes				35
// [original inline comment from CATCommands.cs:388]
//			// Initalize the command word
// [original inline comment from CATCommands.cs:390]
//			string cmd_string = "";
// [original inline comment from CATCommands.cs:391]
//			string temp;
// [original inline comment from CATCommands.cs:392]
//
// [original inline comment from CATCommands.cs:393]
//			// Get VFOA's frequency (P1 - 11 bytes)
// [original inline comment from CATCommands.cs:394]
//			if(console.VFOSplit)
// [original inline comment from CATCommands.cs:395]
//				cmd_string += StrVFOFreq("B");
// [original inline comment from CATCommands.cs:396]
//			else
// [original inline comment from CATCommands.cs:397]
//				cmd_string += StrVFOFreq("A");
// [original inline comment from CATCommands.cs:398]
//
// [original inline comment from CATCommands.cs:399]
//			// Get the step size index (P2 - 4 bytes)
// [original inline comment from CATCommands.cs:400]
//			cmd_string += ZZST();
// [original inline comment from CATCommands.cs:401]
//
// [original inline comment from CATCommands.cs:402]
//			// Determine which incremental tuning control is active
// [original inline comment from CATCommands.cs:403]
//			// and get the value for the active control 
// [original inline comment from CATCommands.cs:404]
//			string rit = "0";
// [original inline comment from CATCommands.cs:405]
//			string xit = "0";
// [original inline comment from CATCommands.cs:406]
//			int ITValue = 0;
// [original inline comment from CATCommands.cs:407]
//
// [original inline comment from CATCommands.cs:408]
//			if(console.RITOn)
// [original inline comment from CATCommands.cs:409]
//				rit = "1";
// [original inline comment from CATCommands.cs:410]
//			else if(console.XITOn)
// [original inline comment from CATCommands.cs:411]
//				xit = "1";
// [original inline comment from CATCommands.cs:412]
//
// [original inline comment from CATCommands.cs:413]
//			if(rit == "1")
// [original inline comment from CATCommands.cs:414]
//				ITValue = console.RITValue;
// [original inline comment from CATCommands.cs:415]
//			else if(xit == "1")
// [original inline comment from CATCommands.cs:416]
//				ITValue = console.XITValue;
// [original inline comment from CATCommands.cs:417]
//
// [original inline comment from CATCommands.cs:418]
//			// Add the ITValue to the command string (P3 - 6 bytes
// [original inline comment from CATCommands.cs:419]
//			if(ITValue < 0)
// [original inline comment from CATCommands.cs:420]
//				cmd_string += "-"+Convert.ToString(Math.Abs(ITValue)).PadLeft(5,'0');
// [original inline comment from CATCommands.cs:421]
//			else
// [original inline comment from CATCommands.cs:422]
//				cmd_string += "+"+Convert.ToString(Math.Abs(ITValue)).PadLeft(5,'0');
// [original inline comment from CATCommands.cs:423]
//
// [original inline comment from CATCommands.cs:424]
//			// Add the RIT/XIT status bits (P4 and P5, one byte each)
// [original inline comment from CATCommands.cs:425]
//			cmd_string += rit+xit;
// [original inline comment from CATCommands.cs:426]
//				
// [original inline comment from CATCommands.cs:427]
//			// Skip the memory channel stuff, the SDR1K doesn't use banks and channels per se
// [original inline comment from CATCommands.cs:428]
//			// (P6 - 1 byte, P7 - 2 bytes)
// [original inline comment from CATCommands.cs:429]
//			cmd_string += "000";
// [original inline comment from CATCommands.cs:430]
//
// [original inline comment from CATCommands.cs:431]
//			// Set the current MOX state (P8 - 1 byte)(what the heck is this for?)
// [original inline comment from CATCommands.cs:432]
//			if(console.MOX)
// [original inline comment from CATCommands.cs:433]
//				cmd_string += "1";
// [original inline comment from CATCommands.cs:434]
//			else
// [original inline comment from CATCommands.cs:435]
//				cmd_string += "0";
// [original inline comment from CATCommands.cs:436]
//
// [original inline comment from CATCommands.cs:437]
//			// Get the SDR mode.  (P9 - 1 byte)
// [original inline comment from CATCommands.cs:438]
//			temp = Mode2KString(console.RX1DSPMode);
// [original inline comment from CATCommands.cs:439]
//			if(temp.Length == 1)	// if the answer is not an error message ?;
// [original inline comment from CATCommands.cs:440]
//				cmd_string += temp;
// [original inline comment from CATCommands.cs:441]
//			else
// [original inline comment from CATCommands.cs:442]
//				cmd_string += " ";	// return a blank if it's an error
// [original inline comment from CATCommands.cs:443]
//
// [original inline comment from CATCommands.cs:444]
//			// Set the FR/FT commands which determines the transmit and receive
// [original inline comment from CATCommands.cs:445]
//			// VFO's. VFO A = 0, VFO B = 1. (P10 - 1 byte)
// [original inline comment from CATCommands.cs:446]
//			if(console.VFOSplit)
// [original inline comment from CATCommands.cs:447]
//				cmd_string += "1";
// [original inline comment from CATCommands.cs:448]
//			else
// [original inline comment from CATCommands.cs:449]
//				cmd_string += "0";
// [original inline comment from CATCommands.cs:450]
//
// [original inline comment from CATCommands.cs:451]
//
// [original inline comment from CATCommands.cs:452]
//			// Set the Scan code to 0 
// [original inline comment from CATCommands.cs:453]
//			// The Scan code might be implemented but the frequency range would
// [original inline comment from CATCommands.cs:454]
//			// have to be manually entered. (P11 - 1 byte)
// [original inline comment from CATCommands.cs:455]
//			cmd_string += "0";
// [original inline comment from CATCommands.cs:456]
//
// [original inline comment from CATCommands.cs:457]
//			// Set the Split operation code (P12 - 1 byte)
// [original inline comment from CATCommands.cs:458]
//			cmd_string += ZZSP("");
// [original inline comment from CATCommands.cs:459]
//
// [original inline comment from CATCommands.cs:460]
//			// Set the remaining CTCSS tone and shift bits to 0 (P13 - P15, 4 bytes)
// [original inline comment from CATCommands.cs:461]
//			cmd_string += "0000";
// [original inline comment from CATCommands.cs:462]
//
// [original inline comment from CATCommands.cs:463]
//			return cmd_string;
// [original inline comment from CATCommands.cs:464]
// Get the rit/xit status
// [original inline comment from CATCommands.cs:3395]
// Get the incremental tuning value for whichever control is selected
// [original inline comment from CATCommands.cs:3400]
// Format the IT value
// [original inline comment from CATCommands.cs:3405]
// Get the rx - tx status
// [original inline comment from CATCommands.cs:3410]
// Get the step size
// [original inline comment from CATCommands.cs:3413]
// Get the vfo split status
// [original inline comment from CATCommands.cs:3416]
//			rtn += StrVFOFreq("A");						// VFO A frequency			11 bytes
// [original inline comment from CATCommands.cs:3428]
// Console step frequency	 4 bytes
// [original inline comment from CATCommands.cs:3429]
// incremental tuning value	 6 bytes
// [original inline comment from CATCommands.cs:3430]
// RIT status				 1 byte
// [original inline comment from CATCommands.cs:3431]
// XIT status				 1 byte
// [original inline comment from CATCommands.cs:3432]
// dummy for memory bank	 3 bytes
// [original inline comment from CATCommands.cs:3433]
// tx-rx status				 1 byte
// [original inline comment from CATCommands.cs:3434]
// current mode				 2 bytes
// [original inline comment from CATCommands.cs:3435]
// dummy for FR/FT			 1 byte
// [original inline comment from CATCommands.cs:3436]
// dummy for scan status	 1 byte
// [original inline comment from CATCommands.cs:3437]
// VFO Split status			 1 byte
// [original inline comment from CATCommands.cs:3438]
// dummy for the balance	 4 bytes
// [original inline comment from CATCommands.cs:3439]
    // From Thetis CAT/CATCommands.cs:317-468,3386-3444 [v2.10.3.15]. No first500 blocking pacing sleeps.
    if (code == "IF" || code == "ZZIF") {
        const auto selection=selectedVfo();
        if (!selection || !primary || !m_adapter.mayRead(binding,CatVfo::Primary) || !m_adapter.mayRead(binding,*selection)) { return error(); }
        const SliceModel* tx=model->txBoundSlice(); const bool rit=primary->ritEnabled(); const bool xit=!rit && tx->xitEnabled();
        const int incremental=rit ? primary->ritHz() : (xit ? tx->xitHz() : 0);
        const int step=tuneStepIndexForHz(primary->stepHz());
        const int mode=modeIndex(primary->dspMode());
        const QByteArray modeString=code == "IF" ? kenwoodMode(primary->dspMode(),global.digitalReportsSideband) : (mode < 0 ? QByteArray() : number(mode,kCodeWidth));
        if (modeString.isEmpty() || step < 0 || step >= int(kStepStrings.size())) { return error(); }
        return payload(number(qint64(std::nearbyint(primary->frequency()+rttyOffset(*primary,CatVfo::Primary,global))),kFrequencyWidth)
            + kStepStrings[step] + signedNumber(incremental,kStatusOffsetWidth) + (rit ? "1" : "0") + (xit ? "1" : "0")
            + "000" + (model->moxController()->isMox() ? "1" : "0") + modeString + "00"
            + (*selection == CatVfo::Secondary ? "1" : "0") + "0000");
    }
// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off
// [original inline comment from CATCommands.cs:5883]
    // From Thetis CAT/CATCommands.cs:773-822,5870-5940,6082-6130,8174-8243,8342-8420 [v2.10.3.15].
//			console.RITValue = 0;
// [original inline comment from CATCommands.cs:775]
//			return "";
// [original inline comment from CATCommands.cs:776]
//			if(s.Length == parser.nSet)
// [original inline comment from CATCommands.cs:790]
//			{
// [original inline comment from CATCommands.cs:791]
//				if(s == "0")
// [original inline comment from CATCommands.cs:792]
//					console.RITOn = false;
// [original inline comment from CATCommands.cs:793]
//				else if(s == "1") 
// [original inline comment from CATCommands.cs:794]
//					console.RITOn = true;
// [original inline comment from CATCommands.cs:795]
//				return "";
// [original inline comment from CATCommands.cs:796]
//			}
// [original inline comment from CATCommands.cs:797]
//			else if(s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:798]
//			{
// [original inline comment from CATCommands.cs:799]
//				bool rit = console.RITOn;
// [original inline comment from CATCommands.cs:800]
//				if(rit)
// [original inline comment from CATCommands.cs:801]
//					return "1";
// [original inline comment from CATCommands.cs:802]
//				else
// [original inline comment from CATCommands.cs:803]
//					return "0";
// [original inline comment from CATCommands.cs:804]
//			}
// [original inline comment from CATCommands.cs:805]
//			else
// [original inline comment from CATCommands.cs:806]
//			{
// [original inline comment from CATCommands.cs:807]
//				return parser.Error1;
// [original inline comment from CATCommands.cs:808]
//			}
// [original inline comment from CATCommands.cs:809]
//			if(s.Length == parser.nSet)
// [original inline comment from CATCommands.cs:1007]
//			{
// [original inline comment from CATCommands.cs:1008]
//				if(s == "0")
// [original inline comment from CATCommands.cs:1009]
//					console.XITOn = false;
// [original inline comment from CATCommands.cs:1010]
//				else
// [original inline comment from CATCommands.cs:1011]
//					if(s == "1") 
// [original inline comment from CATCommands.cs:1012]
//					console.XITOn = true;
// [original inline comment from CATCommands.cs:1013]
//				return "";
// [original inline comment from CATCommands.cs:1014]
//			}
// [original inline comment from CATCommands.cs:1015]
//			else if(s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:1016]
//			{
// [original inline comment from CATCommands.cs:1017]
//				bool xit = console.XITOn;
// [original inline comment from CATCommands.cs:1018]
//				if(xit)
// [original inline comment from CATCommands.cs:1019]
//					return "1";
// [original inline comment from CATCommands.cs:1020]
//				else
// [original inline comment from CATCommands.cs:1021]
//					return "0";
// [original inline comment from CATCommands.cs:1022]
//			}
// [original inline comment from CATCommands.cs:1023]
//			else
// [original inline comment from CATCommands.cs:1024]
//			{
// [original inline comment from CATCommands.cs:1025]
//				return parser.Error1;
// [original inline comment from CATCommands.cs:1026]
//			}
// [original inline comment from CATCommands.cs:1027]
// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off
// [original inline comment from CATCommands.cs:5883]
//switch(console.RX1DSPMode)
// [original inline comment from CATCommands.cs:5885]
//{
// [original inline comment from CATCommands.cs:5886]
//	case DSPMode.CWL:
// [original inline comment from CATCommands.cs:5887]
//	case DSPMode.CWU:
// [original inline comment from CATCommands.cs:5888]
//		console.RITValue -= 10;
// [original inline comment from CATCommands.cs:5889]
//		break;
// [original inline comment from CATCommands.cs:5890]
//	case DSPMode.LSB:
// [original inline comment from CATCommands.cs:5891]
//	case DSPMode.USB:
// [original inline comment from CATCommands.cs:5892]
//		console.RITValue -= 50;  
// [original inline comment from CATCommands.cs:5893]
//                    break;
// [original inline comment from CATCommands.cs:5894]
//            }
// [original inline comment from CATCommands.cs:5895]
//-W2PA Changed to be same step in all modes.
// [original inline comment from CATCommands.cs:5896]
// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off
// [original inline comment from CATCommands.cs:6113]
//switch(console.RX1DSPMode)
// [original inline comment from CATCommands.cs:6116]
//{
// [original inline comment from CATCommands.cs:6117]
//	case DSPMode.CWL:
// [original inline comment from CATCommands.cs:6118]
//	case DSPMode.CWU:
// [original inline comment from CATCommands.cs:6119]
//		console.RITValue += 10;
// [original inline comment from CATCommands.cs:6120]
//		break;
// [original inline comment from CATCommands.cs:6121]
//	case DSPMode.LSB:
// [original inline comment from CATCommands.cs:6122]
//	case DSPMode.USB:
// [original inline comment from CATCommands.cs:6123]
//		console.RITValue += 50;  
// [original inline comment from CATCommands.cs:6124]
//		break;
// [original inline comment from CATCommands.cs:6125]
//            }
// [original inline comment from CATCommands.cs:6126]
//-W2PA Changed to operate in all modes.
// [original inline comment from CATCommands.cs:6127]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:5929]
// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off
// [original inline comment from CATCommands.cs:8187]
//switch(console.RX1DSPMode)
// [original inline comment from CATCommands.cs:8189]
//{
// [original inline comment from CATCommands.cs:8190]
//	case DSPMode.CWL:
// [original inline comment from CATCommands.cs:8191]
//	case DSPMode.CWU:
// [original inline comment from CATCommands.cs:8192]
//		console.RITValue -= 10;
// [original inline comment from CATCommands.cs:8193]
//		break;
// [original inline comment from CATCommands.cs:8194]
//	case DSPMode.LSB:
// [original inline comment from CATCommands.cs:8195]
//	case DSPMode.USB:
// [original inline comment from CATCommands.cs:8196]
//		console.RITValue -= 50;  
// [original inline comment from CATCommands.cs:8197]
//                    break;
// [original inline comment from CATCommands.cs:8198]
//            }
// [original inline comment from CATCommands.cs:8199]
//-W2PA Changed to be same step in all modes.
// [original inline comment from CATCommands.cs:8200]
// && console.RITOn)  //-W2PA Want to be able to change RIT value even if it's off
// [original inline comment from CATCommands.cs:8399]
//switch(console.RX1DSPMode)
// [original inline comment from CATCommands.cs:8401]
//{
// [original inline comment from CATCommands.cs:8402]
//	case DSPMode.CWL:
// [original inline comment from CATCommands.cs:8403]
//	case DSPMode.CWU:
// [original inline comment from CATCommands.cs:8404]
//		console.RITValue -= 10;
// [original inline comment from CATCommands.cs:8405]
//		break;
// [original inline comment from CATCommands.cs:8406]
//	case DSPMode.LSB:
// [original inline comment from CATCommands.cs:8407]
//	case DSPMode.USB:
// [original inline comment from CATCommands.cs:8408]
//		console.RITValue -= 50;  
// [original inline comment from CATCommands.cs:8409]
//                    break;
// [original inline comment from CATCommands.cs:8410]
//            }
// [original inline comment from CATCommands.cs:8411]
//-W2PA Changed to be same step in all modes.
// [original inline comment from CATCommands.cs:8412]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:8233]
    const bool ritEnable=code == "RT" || code == "ZZRT";
    const bool xitEnable=code == "XT" || code == "ZZXS";
    if (ritEnable || xitEnable) {
        if (get) { return readable() ? payload((ritEnable ? slice->ritEnabled() : slice->xitEnabled()) ? "1" : "0") : error(); }
        if (request.suffix != "0" && request.suffix != "1") { return error(); }
        const CatWriteToken write=token(ritEnable ? "ritEnabled" : "xitEnabled"); if (!stillValid(write)) { return error(); }
        if (ritEnable) { slice->setRitEnabled(input == 1); } else { slice->setXitEnabled(input == 1); }
        return stillValid(write) ? silence() : error();
    }
    const QList<QByteArray> offsets{"RC","RD","RU","ZZRC","ZZRD","ZZRU","ZZRF","ZZXC","ZZXD","ZZXU","ZZXF"};
    if (offsets.contains(code)) {
        const bool xit=txCommand; const bool clear=code == "RC" || code == "ZZRC" || code == "ZZXC";
        const bool absolute=code == "ZZRF" || code == "ZZXF";
        const int old=xit ? slice->xitHz() : slice->ritHz();
        if (get && absolute) { return readable() ? signedQuery(old) : error(); }
        const bool down=code == "RD" || code == "ZZRD" || code == "ZZXD";
        const qint64 desired=clear ? 0 : (request.suffix.isEmpty() ? qint64(old)+(down ? -kOffsetStep : kOffsetStep) : input);
        if (!clear && !request.suffix.isEmpty() && !parsed) { return error(); }
        const int hz=int(std::clamp(desired,qint64(-kOffsetLimit),qint64(kOffsetLimit)));
        const CatWriteToken write=token(xit ? "xitHz" : "ritHz"); if (!stillValid(write)) { return error(); }
        if (xit) { slice->setXitHz(hz); } else { slice->setRitHz(hz); }
        return stillValid(write) && (xit ? slice->xitHz() : slice->ritHz()) == hz ? silence() : error();
    }
//-W2PA Transfer focus to VAR1                                                               
// [original inline comment from CATCommands.cs:2857]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:2874]
//				return AddLeadingZeros((int) console.RX1FilterLow);
// [original inline comment from CATCommands.cs:2876]
//-W2PA Transfer focus to VAR1                                                             
// [original inline comment from CATCommands.cs:2894]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:2911]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:2967]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:3003]
//-W2PA Transfer focus to VAR1                                                               
// [original inline comment from CATCommands.cs:2857]
    // From Thetis CAT/CATCommands.cs:2850-2918,2944-3011,6247-6257,9363-9853 [v2.10.3.15]. Atomic edge pair.
    const bool lowEdge=code == "ZZFL" || code == "ZZFS";
    if (lowEdge || code == "ZZFH" || code == "ZZFR" || code == "ZZSF" || code == "SH" || code == "SL") {
        int low=slice->filterLow(); int high=slice->filterHigh();
        const bool coded=code == "SH" || code == "SL";
        const DSPMode mode=slice->dspMode(); const bool dsb=doubleSideband(mode); const bool negative=mode == DSPMode::LSB || mode == DSPMode::CWL;
        if (get) {
            if (!readable()) { return error(); }
            if (!coded) { return signedQuery(lowEdge ? low : high); }
            if (!dsb && mode != DSPMode::USB && mode != DSPMode::LSB && mode != DSPMode::CWU && mode != DSPMode::CWL) { return payload({}); }
            const int edge=code == "SH" ? (negative ? low : high) : (negative ? high : low);
            return payload(number(frequencyCode(edge,code == "SH",dsb),kCodeWidth));
        }
        if (!parsed && !coded) { return error(); }
        if (code == "ZZSF") {
            const int center=request.suffix.left(kCenterFieldWidth).toInt(); const int width=request.suffix.mid(kCenterFieldWidth).toInt();
            if (!center || !width) { return silence(); }
            low=std::max(0,center-width/2); high=center+width/2;
            if (mode == DSPMode::LSB) { const int scratch=high; high=-low; low=-scratch; }
            if (mode == DSPMode::AM || mode == DSPMode::SAM) { low=-high; }
        } else if (coded) {
            const bool sh=code == "SH"; const int hz=codeFrequency(request.suffix == number(input,kCodeWidth) ? int(input) : -1,sh,dsb);
            if (dsb) {
                const int nominal=sh ? hz : codeFrequency(frequencyCode(high*2,true,true),true,true);
                high=nominal/2; low=-nominal/2+(sh ? 0 : hz);
            } else if (mode == DSPMode::USB || mode == DSPMode::CWU) { if (sh) { high=hz; } else { low=hz; } }
            else if (negative) { if (sh) { low=-hz; } else { high=-hz; } }
            else { return silence(); }
        } else { const int hz=int(std::clamp(input,qint64(-kEdgeLimit),qint64(kEdgeLimit))); if (lowEdge) { low=hz; } else { high=hz; } }
        const CatWriteToken write=token("filterLow"); const CatWriteToken highWrite=token("filterHigh");
        if (!stillValid(write) || !stillValid(highWrite)) { return error(); }
        slice->setFilter(low,high);
        return stillValid(write) && stillValid(highWrite) && slice->filterLow() == low && slice->filterHigh() == high ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:1801-1859 [v2.10.3.15]. Real station stream pin, shared cohosts.
    if (code == "ZZCN" || code == "ZZCO") {
        if (slice->streamIndex() < 0) { return error(); }
        if (get) { return readable() ? payload(slice->streamCtunPinned() ? "1" : "0") : error(); }
        if (request.suffix != "0" && request.suffix != "1") { return error(); }
        const CatWriteToken write=token("frequency"); if (!stillValid(write)) { return error(); }
        const bool accepted=model->requestStreamCtunPinned(slice->sliceIndex(),input == 1);
        return stillValid(write) && accepted && slice->streamCtunPinned() == (input == 1) ? silence() : error();
    }
// MW0LGE_21g
// [original inline comment from CATCommands.cs:1620]
    // From Thetis CAT/CATCommands.cs:1109-1199,1382-1632,6207-6247,6257-6290,6477-6487,6602-6620 [v2.10.3.15].
//			int step = console.StepSize;
// [original inline comment from CATCommands.cs:165]
//			double[] wheel_tune_list;
// [original inline comment from CATCommands.cs:166]
//			wheel_tune_list = new double[13];		// initialize wheel tuning list array
// [original inline comment from CATCommands.cs:167]
//			wheel_tune_list[0]  =  0.000001;
// [original inline comment from CATCommands.cs:168]
//			wheel_tune_list[1]  =  0.000010;
// [original inline comment from CATCommands.cs:169]
//			wheel_tune_list[2]  =  0.000050;
// [original inline comment from CATCommands.cs:170]
//			wheel_tune_list[3]  =  0.000100;
// [original inline comment from CATCommands.cs:171]
//			wheel_tune_list[4]  =  0.000250;
// [original inline comment from CATCommands.cs:172]
//			wheel_tune_list[5]  =  0.000500;
// [original inline comment from CATCommands.cs:173]
//			wheel_tune_list[6]  =  0.001000;
// [original inline comment from CATCommands.cs:174]
//			wheel_tune_list[7]  =  0.005000;
// [original inline comment from CATCommands.cs:175]
//			wheel_tune_list[8]  =  0.009000;
// [original inline comment from CATCommands.cs:176]
//			wheel_tune_list[9]  =  0.010000;
// [original inline comment from CATCommands.cs:177]
//			wheel_tune_list[10] =  0.100000;
// [original inline comment from CATCommands.cs:178]
//			wheel_tune_list[11] =  1.000000;
// [original inline comment from CATCommands.cs:179]
//			wheel_tune_list[12] = 10.000000;
// [original inline comment from CATCommands.cs:180]
//
// [original inline comment from CATCommands.cs:181]
//			console.VFOAFreq = console.VFOAFreq - wheel_tune_list[step];
// [original inline comment from CATCommands.cs:182]
//			return "";
// [original inline comment from CATCommands.cs:183]
//			int step = console.StepSize;
// [original inline comment from CATCommands.cs:980]
//			double[] wheel_tune_list;
// [original inline comment from CATCommands.cs:981]
//			wheel_tune_list = new double[13];		// initialize wheel tuning list array
// [original inline comment from CATCommands.cs:982]
//			wheel_tune_list[0]  =  0.000001;
// [original inline comment from CATCommands.cs:983]
//			wheel_tune_list[1]  =  0.000010;
// [original inline comment from CATCommands.cs:984]
//			wheel_tune_list[2]  =  0.000050;
// [original inline comment from CATCommands.cs:985]
//			wheel_tune_list[3]  =  0.000100;
// [original inline comment from CATCommands.cs:986]
//			wheel_tune_list[4]  =  0.000250;
// [original inline comment from CATCommands.cs:987]
//			wheel_tune_list[5]  =  0.000500;
// [original inline comment from CATCommands.cs:988]
//			wheel_tune_list[6]  =  0.001000;
// [original inline comment from CATCommands.cs:989]
//			wheel_tune_list[7]  =  0.005000;
// [original inline comment from CATCommands.cs:990]
//			wheel_tune_list[8]  =  0.009000;
// [original inline comment from CATCommands.cs:991]
//			wheel_tune_list[9]  =  0.010000;
// [original inline comment from CATCommands.cs:992]
//			wheel_tune_list[10] =  0.100000;
// [original inline comment from CATCommands.cs:993]
//			wheel_tune_list[11] =  1.000000;
// [original inline comment from CATCommands.cs:994]
//			wheel_tune_list[12] = 10.000000;
// [original inline comment from CATCommands.cs:995]
//
// [original inline comment from CATCommands.cs:996]
//			console.VFOAFreq = console.VFOAFreq + wheel_tune_list[step];
// [original inline comment from CATCommands.cs:997]
//			return "";
// [original inline comment from CATCommands.cs:998]
    if (code == "ZZAC" || code == "ZZST" || code == "ZZSD" || code == "ZZSU") {
        const int index=tuneStepIndexForHz(slice->stepHz());
        if (!get && code == "ZZAC" && !parsed) { return error(); }
        if (index < 0 || index >= kTuneStepListSize) { return error(); }
        if (get) { return readable() ? payload(code == "ZZST" ? QByteArray(kStepStrings[index]) : number(index,kCodeWidth)) : error(); }
        const int desired=code == "ZZSD" ? (index+kTuneStepListSize-1)%kTuneStepListSize : (code == "ZZSU" ? (index+1)%kTuneStepListSize : int(input));
        if (desired < 0 || desired >= kTuneStepListSize) { return error(); }
        const CatWriteToken write=token("stepHz"); if (!stillValid(write)) { return error(); }
        slice->setStepHz(kTuneStepList[desired].stepHz); return stillValid(write) ? silence() : error();
    }
    const QList<QByteArray> tunes{"DN","UP","ZZSA","ZZSB","ZZSG","ZZSH","ZZAD","ZZAU","ZZAE","ZZAF","ZZBE","ZZBF","ZZBM","ZZBP","ZZSZ"};
    if (tunes.contains(code)) {
        if (!request.suffix.isEmpty() && !parsed) { return error(); }
        if (!primary || !m_adapter.mayRead(binding,CatVfo::Primary)) { return error(); }
        const int step=primary->stepHz();
        // From Thetis console.cs:31285-31297 [v2.10.3.15]. SnapTune with num_steps=1; native Hz.
        // convert frequency to Hz -- use long to support >4GHz
        // [original inline comment from console.cs:31287]
        // do integer division to end up on a step size boundary
        // [original inline comment from console.cs:31288]
        // handle when starting frequency was already on a step size boundary and tuning down
        // [original inline comment from console.cs:31290]
        // off boundary -- add one as the divide takes care of one step
        // [original inline comment from console.cs:31292]
        // increment by the number of steps (positive or negative
        // [original inline comment from console.cs:31294]
        // multiply back up to get hz
        // [original inline comment from console.cs:31296]
        // return freq in MHz
        // [original inline comment from console.cs:31297]
        if (code == "ZZSZ") { return setFrequency((qint64(std::nearbyint(slice->frequency()))/step+1)*double(step)); }
        const bool down=code == "DN" || code == "ZZSA" || code == "ZZSG" || code == "ZZAD" || code == "ZZAE" || code == "ZZBE" || code == "ZZBM";
        const bool explicitStep=code == "ZZAD" || code == "ZZAU" || code == "ZZBM" || code == "ZZBP";
        const bool count=code == "ZZAE" || code == "ZZAF" || code == "ZZBE" || code == "ZZBF";
        if (count && input < 0) { return error(); }
        const double delta=explicitStep ? (input >= 0 && input < qint64(kExplicitSteps.size()) ? kExplicitSteps[input] : 0) : double(step)*(count ? input : 1);
        return setFrequency(slice->frequency()+(down ? -delta : delta));
    }
//			BandDown();
// [original inline comment from CATCommands.cs:136]
//			return "";
// [original inline comment from CATCommands.cs:137]
//			BandUp();
// [original inline comment from CATCommands.cs:145]
//			return "";
// [original inline comment from CATCommands.cs:146]
// MW0LGE_21g
// [original inline comment from CATCommands.cs:1620]
// MW0LGE_21g
// [original inline comment from CATCommands.cs:1620]
    // From Thetis CAT/CATCommands.cs:134-151,1430-1452,1603-1632,10076-10324 [v2.10.3.15]. Existing scoped native band transaction.
    const QList<QByteArray> bands{"BD","BU","ZZBD","ZZBU","ZZBS","ZZBT","ZZBA","ZZBB"};
    if (bands.contains(code)) {
        const Band current=bandFromFrequency(slice->frequency());
        const auto found=std::find(kBands.begin(),kBands.end(),current);
        if (get) { return readable() && found != kBands.end() ? payload(kBandTokens[found-kBands.begin()]) : error(); }
        Band desired=current;
        if (code == "ZZBS" || code == "ZZBT") {
            const auto name=std::find_if(kBandTokens.begin(),kBandTokens.end(),[&](const char* item) { return request.suffix == item; });
            if (name == kBandTokens.end()) { return error(); } desired=kBands[name-kBandTokens.begin()];
        } else {
            const bool down=code == "BD" || code == "ZZBD" || code == "ZZBA";
            const int index=found == kBands.end() ? 0 : int(found-kBands.begin());
            desired=kBands[(index+int(kBands.size())+(down ? -1 : 1))%int(kBands.size())];
        }
        const CatWriteToken write=token("band"); if (!stillValid(write) || slice->locked()) { return error(); }
        model->onBandButtonClicked(slice,desired);
        return stillValid(write) && bandFromFrequency(slice->frequency()) == desired ? silence() : error();
    }
    return error();
}
} // namespace NereusSDR
