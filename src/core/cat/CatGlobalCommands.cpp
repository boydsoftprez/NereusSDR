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

// --- From setup.cs ---
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

// --- From enums.cs ---
/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
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

// Ported from Thetis CAT/CATCommands.cs and console.cs [v2.10.3.15].
// Modification history (NereusSDR):
// 2026-10-04 - Read the service desired global tuple during reconfiguration.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - TX/global CAT compatibility by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.
#include "CatGlobalCommands.h"
#include "CatModelAdapter.h"
#include "CatService.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/TwoToneController.h"
#include "core/PureSignal.h"
#include "core/MicProfileManager.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"
#include "core/StepAttenuatorFacade.h"
#include "core/StepAttenuatorController.h"
#include "core/BoardCapabilities.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include <QCoreApplication>
#include <QRegularExpression>
#include <QScopeGuard>
#include <algorithm>
#include <array>
#include <cmath>
namespace NereusSDR {
namespace {
CatCommandResult error() { return {CatResultKind::Error,"?;"}; }
CatCommandResult silence() { return {CatResultKind::Silence,{}}; }
CatCommandResult payload(QByteArray data) { return {CatResultKind::Payload,data}; }
// From Thetis CAT/CATParser.cs:1590-1592 [v2.10.3.15]. Capability classification fact.
constexpr int kUnavailable = 7;
QByteArray number(qint64 value,int width) { return QByteArray::number(value).rightJustified(width,'0'); }
} // namespace
CatGlobalCommands::CatGlobalCommands(CatModelAdapter& adapter, CatSettings& settings) : m_adapter(adapter), m_settings(settings) {}
QList<QByteArray> CatGlobalCommands::codes() { return {"AI","CN","CT","ID","KS","KY","OF","OS","PR","PS","SM","ZZAI","ZZBR","ZZBY","ZZCB","ZZCD","ZZCF","ZZCI","ZZCL","ZZCM","ZZCS","ZZCU","ZZDU","ZZDX","ZZFD","ZZFM","ZZFV","ZZFW","ZZFX","ZZFY","ZZGL","ZZID","ZZIO","ZZJP","ZZJQ","ZZJR","ZZJS","ZZKM","ZZKO","ZZKS","ZZKY","ZZMF","ZZMR","ZZMS","ZZMT","ZZMU","ZZMV","ZZMW","ZZMX","ZZMY","ZZMZ","ZZOA","ZZOB","ZZOC","ZZOD","ZZOE","ZZOF","ZZOG","ZZOH","ZZOJ","ZZOS","ZZOT","ZZOV","ZZOW","ZZOX","ZZOZ","ZZPS","ZZQK","ZZRM","ZZRS","ZZRV","ZZSM","ZZSN","ZZSS","ZZTA","ZZTB","ZZTF","ZZTS","ZZUA","ZZUP","ZZUX","ZZUY","ZZVA","ZZVB","ZZVC","ZZVD","ZZVF","ZZVH","ZZVI","ZZVJ","ZZVK","ZZVM","ZZVN","ZZVO","ZZVP","ZZVQ","ZZVR","ZZVT","ZZVU","ZZVV","ZZVW","ZZVX","ZZVY","ZZVZ","ZZWA","ZZWB","ZZWC","ZZWD","ZZWE","ZZWF","ZZWG","ZZWH","ZZWJ","ZZWK","ZZWL","ZZWM","ZZWN","ZZWO","ZZWP","ZZWQ","ZZWR","ZZWS","ZZWT","ZZWU","ZZWV","ZZWW","ZZXA","ZZXN","ZZXO","ZZYA","ZZYB","ZZYC","ZZZA","ZZZB","ZZZD","ZZZE","ZZZM","ZZZN","ZZZO","ZZZP","ZZZS","ZZZU","ZZZV","ZZZW","ZZZZ"}; }
CatCommandResult CatGlobalCommands::execute(const CatRequest& request,CatSessionContext& context)
{
    const QPointer<RadioModel> model(&m_adapter.radioModel());
    const QPointer<CatService> service(model->catService());
    if (!service || !service->session(context.sessionId)) { return error(); }
    const CatBinding binding=service->session(context.sessionId)->binding();
    const QByteArray code=request.code;
    const bool get=request.form == CatForm::Get;
    static const CatCommandCatalog catalog;
    const CatDescriptor* descriptor=catalog.find(code);
    if (!descriptor) { return error(); }
    const int width=descriptor->answerWidth;
    // From Thetis CAT/CATCommands.cs:151-154 [v2.10.3.15].
    //Reads or sets the CTCSS frequency
    // [original inline comment from CATCommands.cs:150]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "CN") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:157-160 [v2.10.3.15].
    //Reads or sets the CTCSS enable button
    // [original inline comment from CATCommands.cs:156]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "CT") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:468-499 [v2.10.3.15].
    //Sets or reads the CWX CW speed
    // [original inline comment from CATCommands.cs:467]
    //			int cws = 0;
    // [original inline comment from CATCommands.cs:470]
    //			// Make sure we have an instance of the form
    // [original inline comment from CATCommands.cs:471]
    //			if(console.CWXForm == null || console.CWXForm.IsDisposed)
    // [original inline comment from CATCommands.cs:472]
    //			{
    // [original inline comment from CATCommands.cs:473]
    //				try
    // [original inline comment from CATCommands.cs:474]
    //				{
    // [original inline comment from CATCommands.cs:475]
    //					console.CWXForm = new CWX(console);
    // [original inline comment from CATCommands.cs:476]
    //				}
    // [original inline comment from CATCommands.cs:477]
    //				catch
    // [original inline comment from CATCommands.cs:478]
    //				{
    // [original inline comment from CATCommands.cs:479]
    //					return parser.Error1;
    // [original inline comment from CATCommands.cs:480]
    //				}
    // [original inline comment from CATCommands.cs:481]
    //			}
    // [original inline comment from CATCommands.cs:482]
    //			if(s.Length == parser.nSet)
    // [original inline comment from CATCommands.cs:483]
    //			{
    // [original inline comment from CATCommands.cs:484]
    //				cws = Convert.ToInt32(s);
    // [original inline comment from CATCommands.cs:485]
    //				cws = Math.Max(1, cws);
    // [original inline comment from CATCommands.cs:486]
    //				cws = Math.Min(99, cws);
    // [original inline comment from CATCommands.cs:487]
    //				console.CWXForm.WPM = cws;
    // [original inline comment from CATCommands.cs:488]
    //				return "";
    // [original inline comment from CATCommands.cs:489]
    //
    // [original inline comment from CATCommands.cs:490]
    //			}
    // [original inline comment from CATCommands.cs:491]
    //			else if(s.Length == parser.nGet)
    // [original inline comment from CATCommands.cs:492]
    //			{
    // [original inline comment from CATCommands.cs:493]
    //				return AddLeadingZeros(console.CWXForm.WPM);
    // [original inline comment from CATCommands.cs:494]
    //			}
    // [original inline comment from CATCommands.cs:495]
    //			else
    // [original inline comment from CATCommands.cs:496]
    //				return parser.Error1;
    // [original inline comment from CATCommands.cs:497]
    // Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.
    if (code == "KS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:502-562 [v2.10.3.15].
    //Sends text data to CWX for conversion to Morse
    // [original inline comment from CATCommands.cs:501]
    // Make sure we are in a cw mode.
    // [original inline comment from CATCommands.cs:504]
    // Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.
    if (code == "KY") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:685-688 [v2.10.3.15].
    //Sets or reads the FM repeater offset frequency
    // [original inline comment from CATCommands.cs:684]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "OF") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:691-694 [v2.10.3.15].
    //Sets or reads the repeater offset direction
    // [original inline comment from CATCommands.cs:690]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "OS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1636-1648 [v2.10.3.15].
    //Shuts down the console
    // [original inline comment from CATCommands.cs:1635]
    //[2.10.3.6]MW0LGE Fixes #460 - needed as Midi is from another thread
    // [original inline comment from CATCommands.cs:1638]
    // Source Console.Close closes whole GUI;no approved headless/global close API. No QCoreApplication::quit remote hook
    if (code == "ZZBY") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1651-1667 [v2.10.3.15].
    // Sets or reads the CW Break In Enabled checkbox
    // [original inline comment from CATCommands.cs:1650]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZCB") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1671-1694 [v2.10.3.15].
    // Sets or reads the CW Break In Delay
    // [original inline comment from CATCommands.cs:1670]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZCD") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1697-1725 [v2.10.3.15].
    // Sets or reads the Show CW Frequency checkbox
    // [original inline comment from CATCommands.cs:1696]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZCF") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1728-1750 [v2.10.3.15].
    // Sets or reads the CW Iambic checkbox
    // [original inline comment from CATCommands.cs:1727]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZCI") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1753-1773 [v2.10.3.15].
    // Sets or reads the CW Pitch thumbwheel
    // [original inline comment from CATCommands.cs:1752]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZCL") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1776-1798 [v2.10.3.15].
    // Sets or reads the CW Monitor Disable button status
    // [original inline comment from CATCommands.cs:1775]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZCM") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1879-1900 [v2.10.3.15].
    // Sets or reads the CW Speed thumbwheel
    // [original inline comment from CATCommands.cs:1878]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZCS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:1929-1934 [v2.10.3.15].
    // Reads the CPU Usage
    // [original inline comment from CATCommands.cs:1928]
    //return parser.Error1;
    // [original inline comment from CATCommands.cs:1931]
    // Current Core has Linux-only HostTelemetrySampler/SharedHostSampler (core/daemon/HostTelemetrySampler.h31-54,100-136; StationTelemetry.h141-142), raw optional system/process CPU, first sample absent and macOS/Windows disabled. No existing RadioModel API for source selectable 0.8/0.2 smoothed CPUPercSmoothed (Thetis console26258-26274); current CAT contract remains ?;, no fake cross-platform CPU0 or undocumented raw-for-smoothed replacement.
    if (code == "ZZCU") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:2387-2446 [v2.10.3.15].
    //Constructs the state word for DDUtil
    // [original inline comment from CATCommands.cs:2385]
    //read only
    // [original inline comment from CATCommands.cs:2386]
    // swap VFOA/B
    // [original inline comment from CATCommands.cs:2394]
    // VFO Split status
    // [original inline comment from CATCommands.cs:2395]
    // TUN button status
    // [original inline comment from CATCommands.cs:2396]
    // MOX button status
    // [original inline comment from CATCommands.cs:2397]
    // status += ZZOA("") + sep;
    // [original inline comment from CATCommands.cs:2398]
    // status += ZZOB("") + sep;
    // [original inline comment from CATCommands.cs:2399]
    // status += ZZOC("") + sep;
    // [original inline comment from CATCommands.cs:2400]
    // RX2 button status
    // [original inline comment from CATCommands.cs:2403]
    // RIT button status
    // [original inline comment from CATCommands.cs:2404]
    // current display mode
    // [original inline comment from CATCommands.cs:2405]
    // AGC constant
    // [original inline comment from CATCommands.cs:2406]
    // MultiRx status
    // [original inline comment from CATCommands.cs:2407]
    // XIT status
    // [original inline comment from CATCommands.cs:2408]
    // tuning step size
    // [original inline comment from CATCommands.cs:2411]
    // RX1 DSP mode
    // [original inline comment from CATCommands.cs:2412]
    // RX2 DSP mode
    // [original inline comment from CATCommands.cs:2413]
    // RX2 DSP filter
    // [original inline comment from CATCommands.cs:2414]
    // RX1 filter index
    // [original inline comment from CATCommands.cs:2415]
    // status += ZZOF("") + sep;
    // [original inline comment from CATCommands.cs:2418]
    // RX2 band
    // [original inline comment from CATCommands.cs:2420]
    // Drive level
    // [original inline comment from CATCommands.cs:2421]
    // current band Rx1
    // [original inline comment from CATCommands.cs:2422]
    // AF gain control
    // [original inline comment from CATCommands.cs:2423]
    // CWX CW speed
    // [original inline comment from CATCommands.cs:2424]
    // Tune power level
    // [original inline comment from CATCommands.cs:2425]
    // status += ZZRV() + sep;
    // [original inline comment from CATCommands.cs:2428]
    // S meter value
    // [original inline comment from CATCommands.cs:2430]
    // RIT frequency
    // [original inline comment from CATCommands.cs:2433]
    // status += ZZTS() + sep;
    // [original inline comment from CATCommands.cs:2434]
    // XIT frequency
    // [original inline comment from CATCommands.cs:2436]
    // CPU usage
    // [original inline comment from CATCommands.cs:2439]
    // VFOA frequency
    // [original inline comment from CATCommands.cs:2442]
    // VFOB frequency
    // [original inline comment from CATCommands.cs:2443]
    // Source2387-2452 composes CWX,display,multiRX+other status into126;required production fields absent. Reject whole read instead of mixfake state
    if (code == "ZZDU") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:2454-2467 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:2451]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:2452]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:2453]
    // Current core/models have no independent CATPhoneDX/chkDX state or signal binding. Live CFC profile and CPDR APIs are separate processors (RadioModel.cpp33362-33422/7371-7380), not a DX checkbox substitute. Both forms unavailable.
    if (code == "ZZDX") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:2771-2792 [v2.10.3.15].
    //Selects or reads the FM deviation radio button
    // [original inline comment from CATCommands.cs:2770]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "ZZFD") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:3165-3197 [v2.10.3.15].
    //Sets or reads the noise gate level control
    // [original inline comment from CATCommands.cs:3164]
    // we have to remove the leading zero and replace it with the sign.
    // [original inline comment from CATCommands.cs:3190]
    // Source NoiseGate slider-160..0 scroll only labels/focus; no separate production level/readback. VOX threshold is different
    if (code == "ZZGL") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4614-4713 [v2.10.3.15].
    // start playback from slot N
    // [original inline comment from CATCommands.cs:4616]
    //stop anything
    // [original inline comment from CATCommands.cs:4621]
    // format is ZZJPxynnnqggg; (s will be xynnnqggg)
    // [original inline comment from CATCommands.cs:4625]
    // x is receiver 1-2,
    // [original inline comment from CATCommands.cs:4626]
    // y is via 0=wdsp 1=pc,
    // [original inline comment from CATCommands.cs:4627]
    // nnn is slot number 001-128
    // [original inline comment from CATCommands.cs:4628]
    // q is 0=use playback temporary limitations, 1=ignore them
    // [original inline comment from CATCommands.cs:4629]
    // ggg is gain adjust in dB from -70 to +70, 0 should be sent as +00 or -00
    // [original inline comment from CATCommands.cs:4630]
    // consider for wdsp, not used in pc playback althoug the cat command must provide it
    // [original inline comment from CATCommands.cs:4651]
    // all in sub folder cat, slot_0.wav to slot_127.wav
    // [original inline comment from CATCommands.cs:4674]
    // all ok
    // [original inline comment from CATCommands.cs:4691]
    // will report if playing
    // [original inline comment from CATCommands.cs:4706]
    // Current core/session RecordStream is telemetry records, ModMonitorRecord is DSP feedback; neither audio ARP recording/playback slots. No ARP slot engine in core/models. Every JR/JP/JQ/JS form ?; with no stop-before-validation or fake zero busy state.
    if (code == "ZZJP") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4714-4779 [v2.10.3.15].
    // start playback/record using a containter item voice record/playback item identified by its 4char id
    // [original inline comment from CATCommands.cs:4716]
    //stop anything
    // [original inline comment from CATCommands.cs:4721]
    // format is ZZJQccccrnnn; (s will be ccccrnnn)
    // [original inline comment from CATCommands.cs:4725]
    // ccccy is a 4charID for the target gadget item voice record/play
    // [original inline comment from CATCommands.cs:4726]
    // r is 0=playback 1=record
    // [original inline comment from CATCommands.cs:4727]
    // nnn is slot number 001-128
    // [original inline comment from CATCommands.cs:4728]
    // will report if recording or playing
    // [original inline comment from CATCommands.cs:4772]
    // Current core/session RecordStream is telemetry records, ModMonitorRecord is DSP feedback; neither audio ARP recording/playback slots. No ARP slot engine in core/models. Every JR/JP/JQ/JS form ?; with no stop-before-validation or fake zero busy state.
    if (code == "ZZJQ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4506-4613 [v2.10.3.15].
    // start recording to slot N
    // [original inline comment from CATCommands.cs:4508]
    //stop anything
    // [original inline comment from CATCommands.cs:4513]
    // format is ZZJRxynnnq; (s will be xynnnq)
    // [original inline comment from CATCommands.cs:4517]
    // x is receiver 1-2,
    // [original inline comment from CATCommands.cs:4518]
    // y is via 0=wdsp 1=pc,
    // [original inline comment from CATCommands.cs:4519]
    // nnn is slot number 001-128
    // [original inline comment from CATCommands.cs:4520]
    // q is 0=use recording temporary limitations, 1=ignore them
    // [original inline comment from CATCommands.cs:4521]
    //double ddcfreq;
    // [original inline comment from CATCommands.cs:4544]
    //ddcfreq = console.CentreFrequency;
    // [original inline comment from CATCommands.cs:4552]
    //ddcfreq = console.CentreRX2Frequency;
    // [original inline comment from CATCommands.cs:4559]
    //ddcfreq = console.CentreFrequency;
    // [original inline comment from CATCommands.cs:4566]
    //DDCFrequency = ddcfreq.ToString("F6", System.Globalization.CultureInfo.InvariantCulture)
    // [original inline comment from CATCommands.cs:4575]
    // all in sub folder cat, slot_0.wav to slot_127.wav
    // [original inline comment from CATCommands.cs:4577]
    // all ok
    // [original inline comment from CATCommands.cs:4591]
    // will report if recording
    // [original inline comment from CATCommands.cs:4606]
    // Current core/session RecordStream is telemetry records, ModMonitorRecord is DSP feedback; neither audio ARP recording/playback slots. No ARP slot engine in core/models. Every JR/JP/JQ/JS form ?; with no stop-before-validation or fake zero busy state.
    if (code == "ZZJR") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4495-4504 [v2.10.3.15].
    // stop record/play
    // [original inline comment from CATCommands.cs:4497]
    // Current core/session RecordStream is telemetry records, ModMonitorRecord is DSP feedback; neither audio ARP recording/playback slots. No ARP slot engine in core/models. Every JR/JP/JQ/JS form ?; with no stop-before-validation or fake zero busy state.
    if (code == "ZZJS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:3539-3556 [v2.10.3.15].
    //Sends a CWX macro
    // [original inline comment from CATCommands.cs:3538]
    // Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.
    if (code == "ZZKM") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:3509-3535 [v2.10.3.15].
    // Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.
    if (code == "ZZKO") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:3559-3579 [v2.10.3.15].
    //Sets or reads the CWX CW speed
    // [original inline comment from CATCommands.cs:3558]
    //cws = Math.Max(1, cws);
    // [original inline comment from CATCommands.cs:3566]
    //cws = Math.Min(99, cws);
    // [original inline comment from CATCommands.cs:3567]
    //[2.10.3.4]MW0LGE put in the property WPM where it should be
    // [original inline comment from CATCommands.cs:3568]
    // Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.
    if (code == "ZZKS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:3582-3645 [v2.10.3.15].
    //Sends text to CWX for conversion to Morse
    // [original inline comment from CATCommands.cs:3581]
    // Make sure we are in a cw mode.
    // [original inline comment from CATCommands.cs:3586]
    // Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.
    if (code == "ZZKY") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4023-4042 [v2.10.3.15].
    // ZZMFcccccccccccccccccccc;  Set multifunction encoder text 
    // [original inline comment from CATCommands.cs:4021]
    // cc are 15 pairs of digits 0-99 each making up an ASCII code -32 (so  'A' is 33 for example)
    // [original inline comment from CATCommands.cs:4022]
    // get ascii code
    // [original inline comment from CATCommands.cs:4033]
    // Global titlebar multifunction text30 has no core/model display interface;R1 forbids GUI access
    if (code == "ZZMF") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4276-4297 [v2.10.3.15].
    // Sets or reads the RX meter mode
    // [original inline comment from CATCommands.cs:4275]
    // Meter display mode has no production model capability within the approved ordinary endpoint scope.
    if (code == "ZZMR") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4300-4316 [v2.10.3.15].
    //Sets or reads the MultiRX Swap checkbox
    // [original inline comment from CATCommands.cs:4299]
    // RX1 extra subreceiver MultiRX has no production model capability within the approved ordinary endpoint scope.
    if (code == "ZZMS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4319-4339 [v2.10.3.15].
    // Sets or reads the TX meter mode
    // [original inline comment from CATCommands.cs:4318]
    //Added padleft 4/2/2007 BT
    // [original inline comment from CATCommands.cs:4333]
    // Current MeterModel.h has readings only; RadioModel.h5119/cpp26464-26471 explicitly defers TX display selection/save/restore. Typed TxMeterType readings do not implement mutable GUI TX meter selector. Both forms ?; without shadow enum.
    if (code == "ZZMT") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4342-4358 [v2.10.3.15].
    //Sets or reads the MultiRX button status
    // [original inline comment from CATCommands.cs:4341]
    // RX1 extra subreceiver MultiRX has no production model capability within the approved ordinary endpoint scope.
    if (code == "ZZMU") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4362-4373 [v2.10.3.15].
    //Returns the count of memory records
    // [original inline comment from CATCommands.cs:4360]
    //read only
    // [original inline comment from CATCommands.cs:4361]
    // Current core/models have no MemoryList/MemoryRecord store or full channel restoration API. ZZMW source deletes record despite XML label; ZZMX is dispatched at CATParser.cs:965-967; actual native memory storage remains unavailable. Every form ?; before mutation.
    if (code == "ZZMV") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4376-4391 [v2.10.3.15].
    //Deletes a memory channel
    // [original inline comment from CATCommands.cs:4375]
    // Current core/models have no MemoryList/MemoryRecord store or full channel restoration API. ZZMW source deletes record despite XML label; ZZMX is dispatched at CATParser.cs:965-967; actual native memory storage remains unavailable. Every form ?; before mutation.
    if (code == "ZZMW") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4394-4415 [v2.10.3.15].
    //Restores memory channel n
    // [original inline comment from CATCommands.cs:4393]
    // Current core/models have no MemoryList/MemoryRecord store or full channel restoration API. ZZMW source deletes record despite XML label; ZZMX is dispatched at CATParser.cs:965-967; actual native memory storage remains unavailable. Every form ?; before mutation.
    if (code == "ZZMX") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4418-4445 [v2.10.3.15].
    //Saves the current radio configuration to a new memory channel
    // [original inline comment from CATCommands.cs:4417]
    // ke9ns add for freq scheduler
    // [original inline comment from CATCommands.cs:4433]
    // Current core/models have no MemoryList/MemoryRecord store or full channel restoration API. ZZMW source deletes record despite XML label; ZZMX is dispatched at CATParser.cs:965-967; actual native memory storage remains unavailable. Every form ?; before mutation.
    if (code == "ZZMY") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4448-4493 [v2.10.3.15].
    //Saves the radio configuration to a specific channel number (edit)
    // [original inline comment from CATCommands.cs:4447]
    // Current core/models have no MemoryList/MemoryRecord store or full channel restoration API. ZZMW source deletes record despite XML label; ZZMX is dispatched at CATParser.cs:965-967; actual native memory storage remains unavailable. Every form ?; before mutation.
    if (code == "ZZMZ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5266 [v2.10.3.15].
    //Sets or reads the RX2 antenna (if RX2 installed)
    // [original inline comment from CATCommands.cs:5265]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOB") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5302-5306 [v2.10.3.15].
    //Sets or reads the current Antenna Mode
    // [original inline comment from CATCommands.cs:5301]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOD") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5309-5313 [v2.10.3.15].
    //Sets or reads the RX1 External Antenna checkbox
    // [original inline comment from CATCommands.cs:5308]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOE") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5316-5320 [v2.10.3.15].
    //Sets or reads the TX relay RCA jack
    // [original inline comment from CATCommands.cs:5315]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOF") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5324-5328 [v2.10.3.15].
    //Sets or reads the TX Relay Delay enables
    // [original inline comment from CATCommands.cs:5323]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOG") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5331-5335 [v2.10.3.15].
    //Sets or reads the TX Relay Delays
    // [original inline comment from CATCommands.cs:5330]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOH") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5337-5341 [v2.10.3.15].
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOJ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5369-5380 [v2.10.3.15].
    //Sets or reads the current repeater offset direction
    // [original inline comment from CATCommands.cs:5368]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "ZZOS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5384-5400 [v2.10.3.15].
    //Sets for reads the repeater frequency offset
    // [original inline comment from CATCommands.cs:5382]
    //need to resolve the negative offset question.
    // [original inline comment from CATCommands.cs:5383]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "ZZOT") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5429-5433 [v2.10.3.15].
    //Sets or reads the console ATU button
    // [original inline comment from CATCommands.cs:5428]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOV") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5436-5440 [v2.10.3.15].
    //Sets or reads the console ATU Bypass button
    // [original inline comment from CATCommands.cs:5435]
    // Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability
    if (code == "ZZOW") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4143-4156 [v2.10.3.15].
    //
    // [original inline comment from CATCommands.cs:4140]
    //ARIES ATU tune state message
    // [original inline comment from CATCommands.cs:4141]
    //write only
    // [original inline comment from CATCommands.cs:4142]
    // Current AlexController.h330/cpp634 documents Aries clamp deferred; no real Aries tune/erase inbound response integration. TGXL/Apollo/tuner APIs are different devices; controller commands remain separate integration and unavailable ordinary endpoints.
    if (code == "ZZOX") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4161-4174 [v2.10.3.15].
    //
    // [original inline comment from CATCommands.cs:4158]
    //ARIES ATU erase state message
    // [original inline comment from CATCommands.cs:4159]
    //write only
    // [original inline comment from CATCommands.cs:4160]
    // Current AlexController.h330/cpp634 documents Aries clamp deferred; no real Aries tune/erase inbound response integration. TGXL/Apollo/tuner APIs are different devices; controller commands remain separate integration and unavailable ordinary endpoints.
    if (code == "ZZOZ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:5776-5792 [v2.10.3.15].
    // Sets or reads the CW Break-In for Semi/QSK modes
    // [original inline comment from CATCommands.cs:5775]
    // Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.
    if (code == "ZZQK") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:6057-6078 [v2.10.3.15].
    //Sets or reads the RX2 button status
    // [original inline comment from CATCommands.cs:6056]
    // Receiver enable has no production model capability within the approved ordinary endpoint scope.
    if (code == "ZZRS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:6470-6474 [v2.10.3.15].
    // Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.
    if (code == "ZZSS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:6620-6645 [v2.10.3.15].
    //Sets or reads the CTCSS enable button
    // [original inline comment from CATCommands.cs:6619]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "ZZTA") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:6648-6671 [v2.10.3.15].
    //Sets or reads the CTCSS tone frequency
    // [original inline comment from CATCommands.cs:6647]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "ZZTB") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:6674-6700 [v2.10.3.15].
    // Sets or reads the Show TX Filter checkbox
    // [original inline comment from CATCommands.cs:6673]
    // Source ShowTXFilter differs GUI waterfall overlay preference; no core/model interface, R1 forbids GUI include
    if (code == "ZZTF") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:6984-6988 [v2.10.3.15].
    //Reads the XVTR Band Names
    // [original inline comment from CATCommands.cs:6983]
    // Source console.cs9655 enumerates14 enabled transverter labels each left-pad5; no transverter catalogue/enable/label model. Generic Band::XVTR seed invalid, not14 configurations
    if (code == "ZZUA") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7014 [v2.10.3.15].
    //-MW0LGE_21j  xPA on/off
    // [original inline comment from CATCommands.cs:7014]
    // Contributor MW0LGE; the original -MW0LGE_21j tag is retained above.
    // No equivalent Thetis CATxPA configured OC TX pin gate; accessory operate/interlock fields are different
    if (code == "ZZUP") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:6991 [v2.10.3.15].
    // Reads or sets the VAC Enable checkbox (Setup Form)
    // [original inline comment from CATCommands.cs:6990]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVA") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7085-7117 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7082]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7083]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7084]
    // DH1KLM_21g Changed value from 20 to 40 like it is in VAC2 RX Gain
    // [original inline comment from CATCommands.cs:7095]
    // we have to remove the leading zero and replace it with the sign.
    // [original inline comment from CATCommands.cs:7110]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVB") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7124-7156 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7121]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7122]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7123]
    // DH1KLM_21g Changed value from 20 to 40 like it is in VAC2 TX Gain
    // [original inline comment from CATCommands.cs:7134]
    // we have to remove the leading zero and replace it with the sign.
    // [original inline comment from CATCommands.cs:7149]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVC") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7163-7246 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7160]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7161]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7162]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVD") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7322-7344 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7319]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7320]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7321]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVF") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7381-7403 [v2.10.3.15].
    // Reads or sets the I/Q to VAC checkbox on the setup form
    // [original inline comment from CATCommands.cs:7380]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVH") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7406-7420 [v2.10.3.15].
    // Reads or sets the VAC Input cable
    // [original inline comment from CATCommands.cs:7405]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVI") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7423-7444 [v2.10.3.15].
    //Reads or sets the Direct I/Q Use RX2 checkbox
    // [original inline comment from CATCommands.cs:7422]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVJ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7447-7469 [v2.10.3.15].
    // Reads or sets the VAC2 Enable checkbox (Setup Form)
    // [original inline comment from CATCommands.cs:7446]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVK") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7577-7591 [v2.10.3.15].
    // Reads or sets the VAC Driver
    // [original inline comment from CATCommands.cs:7576]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVM") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7602-7616 [v2.10.3.15].
    // Reads or sets the VAC Output cable
    // [original inline comment from CATCommands.cs:7601]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVO") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7619-7641 [v2.10.3.15].
    // Reads or sets the VAC1 IQ Calibrate checkbox on the setup form
    // [original inline comment from CATCommands.cs:7618]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVP") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7644-7658 [v2.10.3.15].
    // Reads or sets the VAC2 Driver
    // [original inline comment from CATCommands.cs:7643]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVQ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7661-7675 [v2.10.3.15].
    // Reads or sets the VAC2 Input cable
    // [original inline comment from CATCommands.cs:7660]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVR") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7694-7708 [v2.10.3.15].
    // Reads or sets the VAC2 Output cable
    // [original inline comment from CATCommands.cs:7693]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVT") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7715-7804 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7712]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7713]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7714]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVU") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7810-7832 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7807]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7808]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7809]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVV") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7834-7866 [v2.10.3.15].
    // we have to remove the leading zero and replace it with the sign.
    // [original inline comment from CATCommands.cs:7859]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVW") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7873-7905 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7870]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7871]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7872]
    // we have to remove the leading zero and replace it with the sign.
    // [original inline comment from CATCommands.cs:7898]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVX") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7912-7959 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7909]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7910]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7911]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVY") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:7966-8013 [v2.10.3.15].
    /// </summary>
    // [original inline comment from CATCommands.cs:7963]
    /// <param name="s"></param>
    // [original inline comment from CATCommands.cs:7964]
    /// <returns></returns>
    // [original inline comment from CATCommands.cs:7965]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZVZ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8016-8020 [v2.10.3.15].
    //Sets or reads the F5K Mixer Mic Gain
    // [original inline comment from CATCommands.cs:8015]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWA") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8023-8027 [v2.10.3.15].
    //Sets or reads the F5K Line In RCA level
    // [original inline comment from CATCommands.cs:8022]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWB") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8030-8034 [v2.10.3.15].
    //Sets or reads the F5K Line In Phono level
    // [original inline comment from CATCommands.cs:8029]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWC") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8037-8041 [v2.10.3.15].
    //Sets or reads the F5K Mixer Line In DB9 level
    // [original inline comment from CATCommands.cs:8036]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWD") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8045-8049 [v2.10.3.15].
    // Sets or reads the F1500F5K Mixer Mic Selected Checkbox
    // [original inline comment from CATCommands.cs:8044]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWE") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8052-8056 [v2.10.3.15].
    // Sets or reads the F5K Mixer Line In RCA Checkbox
    // [original inline comment from CATCommands.cs:8051]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWF") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8059-8063 [v2.10.3.15].
    // Sets or reads the F5K Mixer Line In Phono Checkbox
    // [original inline comment from CATCommands.cs:8058]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWG") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8066-8070 [v2.10.3.15].
    // Sets or reads the F1500/F5K Mixer Line In FlexWire/DB9 Checkbox
    // [original inline comment from CATCommands.cs:8065]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWH") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8074-8078 [v2.10.3.15].
    // Sets or reads the F5K Mixer Mute All Checkbox
    // [original inline comment from CATCommands.cs:8073]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWJ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8081-8085 [v2.10.3.15].
    //Sets or reads the F5K Mixer Internal Speaker level
    // [original inline comment from CATCommands.cs:8080]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWK") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8088-8092 [v2.10.3.15].
    //Sets or reads the F5K Mixer External Speaker level
    // [original inline comment from CATCommands.cs:8087]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWL") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8095-8099 [v2.10.3.15].
    //Sets or reads the F5K Mixer Headphone level
    // [original inline comment from CATCommands.cs:8094]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWM") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8102-8106 [v2.10.3.15].
    //Sets or reads the F5K Mixer Line Out RCA level
    // [original inline comment from CATCommands.cs:8101]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWN") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8109-8113 [v2.10.3.15].
    // Sets or reads the F5KC Mixer Internal Speaker Selected Checkbox
    // [original inline comment from CATCommands.cs:8108]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWO") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8116-8120 [v2.10.3.15].
    // Sets or reads the F5K Mixer External Speaker Selected Checkbox
    // [original inline comment from CATCommands.cs:8115]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWP") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8123-8127 [v2.10.3.15].
    // Sets or reads the F1500F5K Mixer Headphone Selected Checkbox
    // [original inline comment from CATCommands.cs:8122]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWQ") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8130-8134 [v2.10.3.15].
    // Sets or reads the F1500 FlexWire Out/F5K Mixer Line Out RCA Selected Checkbox
    // [original inline comment from CATCommands.cs:8129]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWR") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8137-8141 [v2.10.3.15].
    // Sets or reads the F1500/F5K Mixer Output Mute All Checkbox
    // [original inline comment from CATCommands.cs:8136]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8145-8149 [v2.10.3.15].
    //Reads or sets the F1500 mixer form mic level
    // [original inline comment from CATCommands.cs:8144]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWT") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8152-8156 [v2.10.3.15].
    //Reads or sets the F1500 Mixer Form FireWire Input Level
    // [original inline comment from CATCommands.cs:8151]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWU") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8159-8163 [v2.10.3.15].
    //Sets ir reads the F1500 Mixer Form Phones level
    // [original inline comment from CATCommands.cs:8158]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWV") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8166-8170 [v2.10.3.15].
    //Sets or reads the F1500 Mixer Form FlexWire Out level
    // [original inline comment from CATCommands.cs:8165]
    // Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities
    if (code == "ZZWW") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8730-8747 [v2.10.3.15].
    // Current core/models have no Thetis EnableAudioAmplifier register setter. BoardCapabilities HasAudioAmplifier comments describe hardware only; speaker mute/monitor level differ. Both forms unavailable.
    if (code == "ZZXA") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8453-8475 [v2.10.3.15].
    // Reads or sets the VAC2 Direct I/Q checkbox on the setup form
    // [original inline comment from CATCommands.cs:8452]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZYA") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8478-8500 [v2.10.3.15].
    // Reads or sets the VAC2 IQ Calibrate checkbox on the setup form
    // [original inline comment from CATCommands.cs:8477]
    // Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.
    if (code == "ZZYB") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8503-8526 [v2.10.3.15].
    //Reads or sets the FM mic gain
    // [original inline comment from CATCommands.cs:8502]
    // Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.
    if (code == "ZZYC") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4128-4138 [v2.10.3.15].
    //CATHandleAriesTuneMessage
    // [original inline comment from CATCommands.cs:4125]
    //Ganymeda amplifier trip state
    // [original inline comment from CATCommands.cs:4126]
    //write only
    // [original inline comment from CATCommands.cs:4127]
    // Current RadioModel::handleGanymedeTrip(int) is functional and feeds actual MoxController::setPaTripped every asserted message (RadioModel.cpp25833-25873; global applyTxKeyBlock27954). Old safety absence is superseded. This controller-owned incoming message remains unavailable on ordinary serial/TCP CAT by approved separate-integration scope; do not permit arbitrary clients to impersonate a Ganymede protection device or clear real trips.
    if (code == "ZZZA") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8545-8550 [v2.10.3.15].
    // Zero beat has no production model capability within the approved ordinary endpoint scope.
    if (code == "ZZZB") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4046-4056 [v2.10.3.15].
    //Andromeda front panel VFO encoder down
    // [original inline comment from CATCommands.cs:4044]
    //write only
    // [original inline comment from CATCommands.cs:4045]
    // Current core/models expose no attached Andromeda identity/front-panel button/encoder/LED dispatch. Controller-owned protocol must be a separate integration; reject ordinary endpoint forms before mutation.
    if (code == "ZZZD") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4088-4103 [v2.10.3.15].
    //Andromeda front panel encoder step
    // [original inline comment from CATCommands.cs:4086]
    //write only
    // [original inline comment from CATCommands.cs:4087]
    // bottom digit
    // [original inline comment from CATCommands.cs:4093]
    // top 2 digits
    // [original inline comment from CATCommands.cs:4094]
    // Current core/models expose no attached Andromeda identity/front-panel button/encoder/LED dispatch. Controller-owned protocol must be a separate integration; reject ordinary endpoint forms before mutation.
    if (code == "ZZZE") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8683-8703 [v2.10.3.15].
    //[2.10.1.0]MW0LGE enable/disable quick split mode
    // [original inline comment from CATCommands.cs:8682]
    // Current core/models contain no Midi2Cat wheel/quick-split controller integration. Pinned source requires that controller; standard CAT slice split does not satisfy this capability. Reject both/action forms.
    if (code == "ZZZN") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8705-8728 [v2.10.3.15].
    //[2.10.1.0]MW0LGE enable/disable quick split and turn split on/off at same time
    // [original inline comment from CATCommands.cs:8704]
    // Current core/models contain no Midi2Cat wheel/quick-split controller integration. Pinned source requires that controller; standard CAT slice split does not satisfy this capability. Reject both/action forms.
    if (code == "ZZZO") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4107-4124 [v2.10.3.15].
    //Andromeda front panel pushbutton press
    // [original inline comment from CATCommands.cs:4105]
    //write only
    // [original inline comment from CATCommands.cs:4106]
    // 1-99
    // [original inline comment from CATCommands.cs:4118]
    // Current core/models expose no attached Andromeda identity/front-panel button/encoder/LED dispatch. Controller-owned protocol must be a separate integration; reject ordinary endpoint forms before mutation.
    if (code == "ZZZP") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4074-4084 [v2.10.3.15].
    //Andromeda front panel s/w version
    // [original inline comment from CATCommands.cs:4072]
    //write only
    // [original inline comment from CATCommands.cs:4073]
    // Current core/models expose no attached Andromeda identity/front-panel button/encoder/LED dispatch. Controller-owned protocol must be a separate integration; reject ordinary endpoint forms before mutation.
    if (code == "ZZZS") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:4060-4070 [v2.10.3.15].
    //Andromeda front panel VFO encoder up
    // [original inline comment from CATCommands.cs:4058]
    //write only
    // [original inline comment from CATCommands.cs:4059]
    // Current core/models expose no attached Andromeda identity/front-panel button/encoder/LED dispatch. Controller-owned protocol must be a separate integration; reject ordinary endpoint forms before mutation.
    if (code == "ZZZU") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:8664 [v2.10.3.15].
    // swap vfo wheels, vfoA becomes vfoB, B becomes A
    // [original inline comment from CATCommands.cs:8663]
    // Current core/models contain no Midi2Cat wheel/quick-split controller integration. Pinned source requires that controller; standard CAT slice split does not satisfy this capability. Reject both/action forms.
    if (code == "ZZZW") { return {CatResultKind::Error,"?;",kUnavailable}; }
    // From Thetis CAT/CATCommands.cs:291-313 [v2.10.3.15].
    // Reads the transceiver ID number
    // [original inline comment from CATCommands.cs:289]
    // this needs changing when 3rd party folks on line.
    // [original inline comment from CATCommands.cs:290]
    //SDR-1000
    // [original inline comment from CATCommands.cs:297]
    //TS-50S
    // [original inline comment from CATCommands.cs:300]
    //TS-2000
    // [original inline comment from CATCommands.cs:303]
    //TS-480
    // [original inline comment from CATCommands.cs:306]
    // From Thetis CAT/CATCommands.cs:3369-3383 [v2.10.3.15].
    // Sets the CAT Rig Type to SDR-1000
    // [original inline comment from CATCommands.cs:3367]
    //Modified 10/12/08 BT changed "SDR-1000" to "PowerSDR"
    // [original inline comment from CATCommands.cs:3368]
    //			if(s.Length == parser.nSet)
    // [original inline comment from CATCommands.cs:3371]
    //			{
    // [original inline comment from CATCommands.cs:3372]
    //				return CAT2RigType(s);
    // [original inline comment from CATCommands.cs:3373]
    //			}
    // [original inline comment from CATCommands.cs:3374]
    //			else if(s.Length == parser.nGet)
    // [original inline comment from CATCommands.cs:3375]
    //			{
    // [original inline comment from CATCommands.cs:3376]
    //				return RigType2CAT();
    // [original inline comment from CATCommands.cs:3377]
    //			}
    // [original inline comment from CATCommands.cs:3378]
    //			else
    // [original inline comment from CATCommands.cs:3379]
    //				return parser.Error1;
    // [original inline comment from CATCommands.cs:3380]
    // From Thetis CAT/CATCommands.cs:6343-6350 [v2.10.3.15].
    //Reads the radio serial number
    // [original inline comment from CATCommands.cs:6342]
    // parser.Verbose_Error_Code = 7;
    // [original inline comment from CATCommands.cs:6346]
    // ret_val = parser.Error1;
    // [original inline comment from CATCommands.cs:6347]
    // From Thetis CAT/CATCommands.cs:105-130 [v2.10.3.15].
    //			if(console.SetupForm.AllowFreqBroadcast)
    // [original inline comment from CATCommands.cs:108]
    //			{
    // [original inline comment from CATCommands.cs:109]
    //				if(s.Length == parser.nSet)
    // [original inline comment from CATCommands.cs:110]
    //				{
    // [original inline comment from CATCommands.cs:111]
    //					if(s == "0")
    // [original inline comment from CATCommands.cs:112]
    //						console.KWAutoInformation = false;
    // [original inline comment from CATCommands.cs:113]
    //					else 
    // [original inline comment from CATCommands.cs:114]
    //						console.KWAutoInformation = true;
    // [original inline comment from CATCommands.cs:115]
    //					return "";
    // [original inline comment from CATCommands.cs:116]
    //				}
    // [original inline comment from CATCommands.cs:117]
    //				else if(s.Length == parser.nGet)
    // [original inline comment from CATCommands.cs:118]
    //				{
    // [original inline comment from CATCommands.cs:119]
    //					if(console.KWAutoInformation)
    // [original inline comment from CATCommands.cs:120]
    //						return "1";
    // [original inline comment from CATCommands.cs:121]
    //					else
    // [original inline comment from CATCommands.cs:122]
    //						return "0";
    // [original inline comment from CATCommands.cs:123]
    //				}
    // [original inline comment from CATCommands.cs:124]
    //				else
    // [original inline comment from CATCommands.cs:125]
    //					return parser.Error1;
    // [original inline comment from CATCommands.cs:126]
    //			}
    // [original inline comment from CATCommands.cs:127]
    //			else
    // [original inline comment from CATCommands.cs:128]
    //				return parser.Error1;
    // [original inline comment from CATCommands.cs:129]
    // From Thetis CAT/CATCommands.cs:1223 [v2.10.3.15].
    if (code == "ID" || code == "ZZID" || code == "ZZSN" || code == "AI" || code == "ZZAI") {
        CatGlobalConfig config=service->globalConfig();
        if (code == "ID") {
            if (!get) { return error(); }
            return payload(config.rigIdentity == "PowerSDR" ? "900" : config.rigIdentity == "TS-50S" ? "013" : config.rigIdentity == "TS-480" ? "020" : "019");
        }
        if (code == "ZZSN") {
            // Source setup text is editable; correct its invalid-length raw
            // framing defect without padding/truncating a false serial.
            if (!get) { return error(); }
            return config.serialNumber.size() == width ? payload(config.serialNumber.toLatin1()) : CatCommandResult{CatResultKind::Wire,"O;"};
        }
        if (code == "ZZID") { config.rigIdentity="PowerSDR"; }
        else {
            if (!config.allowKenwoodAi) { return error(); }
            if (get) { return payload(config.aiEnabled ? "1":"0"); }
            bool ok=false; const int value=request.suffix.toInt(&ok);
            if (!ok || value<0) { return error(); }
            config.aiEnabled=value != 0;
        }
        return service->applyGlobalConfig(config) ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:734-757 [v2.10.3.15].
    // Sets or reads the console power on/off status
    // [original inline comment from CATCommands.cs:733]
    //			if(s.Length == parser.nSet)
    // [original inline comment from CATCommands.cs:736]
    //			{
    // [original inline comment from CATCommands.cs:737]
    //				if(s == "0")
    // [original inline comment from CATCommands.cs:738]
    //					console.PowerOn = false;
    // [original inline comment from CATCommands.cs:739]
    //				else if(s == "1")
    // [original inline comment from CATCommands.cs:740]
    //					console.PowerOn = true;
    // [original inline comment from CATCommands.cs:741]
    //				return "";
    // [original inline comment from CATCommands.cs:742]
    //			}
    // [original inline comment from CATCommands.cs:743]
    //			else if(s.Length == parser.nGet)
    // [original inline comment from CATCommands.cs:744]
    //			{
    // [original inline comment from CATCommands.cs:745]
    //				bool pwr = console.PowerOn;
    // [original inline comment from CATCommands.cs:746]
    //				if(pwr)
    // [original inline comment from CATCommands.cs:747]
    //					return "1";
    // [original inline comment from CATCommands.cs:748]
    //				else
    // [original inline comment from CATCommands.cs:749]
    //					return "0";
    // [original inline comment from CATCommands.cs:750]
    //			}
    // [original inline comment from CATCommands.cs:751]
    //			else
    // [original inline comment from CATCommands.cs:752]
    //			{
    // [original inline comment from CATCommands.cs:753]
    //				return parser.Error1;
    // [original inline comment from CATCommands.cs:754]
    //			}
    // [original inline comment from CATCommands.cs:755]
    // From Thetis CAT/CATCommands.cs:5706-5728 [v2.10.3.15].
    //Sets or reads the Power button status
    // [original inline comment from CATCommands.cs:5705]
    if (code == "PS" || code == "ZZPS") {
        if (get) { return payload(model->isConnected() ? "1":"0"); }
        const auto powerChangeAllowed=[&]() {
            return model && service && service->session(context.sessionId) && model->ownsLocalDsp()
                && model->otherDeviceHoldsRefusal().isEmpty()
                && (!model->moxController()->isMox() || model->moxController()->currentKeyer().isStation());
        };
        if (!powerChangeAllowed()) { return error(); }
        if (request.suffix == "1") { return model->isConnected() ? silence() : error(); }
        if (request.suffix == "0") {
            const CatSession* issuingSession = service->session(context.sessionId);
            const CatBinding originalBinding = service->adapter().snapshotBinding(binding);
            const QPointer<MoxController> mox(model->moxController());
            const quint64 ownedTag = service->txCoordinator().currentRequestTag();
            quint64 expectedGeneration = mox->acceptedRequestGeneration();
            bool superseded = false;
            // Cancellation may synchronously accept a newer operator intent.
            // Only this CAT activation's OFF belongs to the old disconnect.
            const QMetaObject::Connection observation = QObject::connect(mox, &MoxController::requestAccepted,
                service, [&](const KeyerIdentity& requester, quint64 generation, bool requestedOn) {
                    if (!requestedOn && ownedTag != 0 && requester.requestTag == ownedTag) {
                        expectedGeneration = generation;
                    } else {
                        superseded = true;
                    }
                });
            const auto cleanup = qScopeGuard([observation] { QObject::disconnect(observation); });
            service->txCoordinator().cancelAll();
            if (!powerChangeAllowed() || !mox || superseded
                || mox->acceptedRequestGeneration() != expectedGeneration
                || service->session(context.sessionId) != issuingSession) { return error(); }
            const CatBinding currentBinding = service->adapter().snapshotBinding(binding);
            if (currentBinding.primaryIncarnation != originalBinding.primaryIncarnation
                || currentBinding.secondaryIncarnation != originalBinding.secondaryIncarnation) { return error(); }
            model->disconnectFromRadio(); return model && service ? silence() : error();
        }
        return silence();
    }
    // From Thetis CAT/CATCommands.cs:721-731 [v2.10.3.15].
    // Sets or reads the Speech Compressor status
    // [original inline comment from CATCommands.cs:719]
    //Reactivated 10/21/2012 for HRD compatibility BT
    // [original inline comment from CATCommands.cs:720]
    // From Thetis CAT/CATCommands.cs:3444-3447 [v2.10.3.15].
    //Reads the installed options
    // [original inline comment from CATCommands.cs:3443]
    // From Thetis CAT/CATCommands.cs:6881-6905 [v2.10.3.15].
    // Reads the Flex 5000 temperature sensor
    // [original inline comment from CATCommands.cs:6880]
    //FWC.ReadPAADC(chan, out val);
    // [original inline comment from CATCommands.cs:6889]
    // From Thetis CAT/CATCommands.cs:1573-1599 [v2.10.3.15].
    //Sets or reads the BCI Rejection button status
    // [original inline comment from CATCommands.cs:1572]
    // From Thetis CAT/CATCommands.cs:3011-3028 [v2.10.3.15].
    //Reads FlexWire single byte value commands
    // [original inline comment from CATCommands.cs:3010]
    //FWC.FlexWire_ReadValue(addr, out val);
    // [original inline comment from CATCommands.cs:3022]
    // From Thetis CAT/CATCommands.cs:3031-3051 [v2.10.3.15].
    //Reds FlexWire double byte value commands
    // [original inline comment from CATCommands.cs:3030]
    //FWC.FlexWire_Read2Value(addr, out val);
    // [original inline comment from CATCommands.cs:3042]
    // From Thetis CAT/CATCommands.cs:3054-3070 [v2.10.3.15].
    //Sends FlexWire single byte value commands
    // [original inline comment from CATCommands.cs:3053]
    //FWC.FlexWire_WriteValue(addr, val);
    // [original inline comment from CATCommands.cs:3065]
    // From Thetis CAT/CATCommands.cs:3073-3092 [v2.10.3.15].
    //Sends FlexWire double byte value commands
    // [original inline comment from CATCommands.cs:3072]
    //FWC.FlexWire_Write2Value(addr, val1, val2);
    // [original inline comment from CATCommands.cs:3086]
    if (code == "PR") { return get ? payload("0") : error(); }
    if (code == "ZZIO") { return get ? payload("000") : error(); }
    if (code == "ZZTS") { return get && model->isConnected() && model->hardwareProfile().model == HPSDRModel::HERMES ? payload("00301") : CatCommandResult{CatResultKind::Error,"?;",kUnavailable}; }
    if (code == "ZZBR") {
        if (!model->isConnected() || model->hardwareProfile().model != HPSDRModel::HPSDR) { return error(); }
        if (get) { return payload("0"); }
        return request.suffix == "0" || request.suffix == "1" ? silence() : error();
    }
    if (code == "ZZFV" || code == "ZZFW" || code == "ZZFX" || code == "ZZFY") {
        static const QRegularExpression kHex(QStringLiteral("^[a-fA-F0-9]+$"));
        if (!kHex.match(QString::fromLatin1(request.suffix)).hasMatch()) { return error(); }
        if (code == "ZZFV" || code == "ZZFW") { return get ? payload(code == "ZZFV" ? "00":"0000") : error(); }
        return !get ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:7595-7599 [v2.10.3.15].
    // Returns the version number of the PowerSDR program
    // [original inline comment from CATCommands.cs:7594]
    //return console.CATGetVersion().PadLeft(12,'0');
    // [original inline comment from CATCommands.cs:7597]
    // From Thetis CAT/CATCommands.cs:8653-8662 [v2.10.3.15].
    // hardware version title string
    // [original inline comment from CATCommands.cs:8652]
    // add command to the return string and terminator, because it is variable length answer
    // [original inline comment from CATCommands.cs:8657]
    // From Thetis CAT/CATCommands.cs:2918-2937 [v2.10.3.15].
    // DH1KLM_21a added 7000D
    // [original inline comment from CATCommands.cs:2932]
    // From Thetis CAT/CATCommands.cs:8642-8651 [v2.10.3.15].
    // hardware model string
    // [original inline comment from CATCommands.cs:8641]
    // add command to the return string and terminator, because it is variable length answer
    // [original inline comment from CATCommands.cs:8646]
    // From Thetis CAT/CATCommands.cs:6134-6156 [v2.10.3.15].
    //Reads the primary input voltage
    // [original inline comment from CATCommands.cs:6133]
    //MW0LGE [2.10.1.0]
    // [original inline comment from CATCommands.cs:6136]
    //int val = 0;
    // [original inline comment from CATCommands.cs:6145]
    //decimal volts = 0.0m;
    // [original inline comment from CATCommands.cs:6146]
    //volts = (decimal)val / 4096m * 2.5m * 11m;
    // [original inline comment from CATCommands.cs:6147]
    //return Decimal.Round(volts, 1).ToString();
    // [original inline comment from CATCommands.cs:6148]
    if (code == "ZZVN" || code == "ZZZV") {
        if (!get) { return error(); }
        QByteArray version=QCoreApplication::applicationVersion().toLatin1(); version.replace(";","");
        if (version.isEmpty()) { return error(); }
        if (code == "ZZVN") { version=version.rightJustified(width,'0'); }
        return {CatResultKind::Wire,code+version+';'};
    }
    if (code == "ZZFM" || code == "ZZZM" || code == "ZZRV") {
        if (!get || !model->isConnected()) { return error(); }
        const HPSDRModel hardware=model->hardwareProfile().model;
        if (code == "ZZRV") {
            if (hardware == HPSDRModel::HPSDR || hardware == HPSDRModel::FIRST) { return error(); }
            const bool live=hardware == HPSDRModel::ANAN7000D || hardware == HPSDRModel::ANAN8000D || hardware == HPSDRModel::ANVELINAPRO3 || hardware == HPSDRModel::ANAN_G2 || hardware == HPSDRModel::ANAN_G2_1K;
            if (!live) { return payload("00.0"); }
            const auto volts=model->paReadings().paVolts;
            if (!volts || !std::isfinite(*volts)) { return error(); }
            return payload(QByteArray::number(*volts,'f',1).rightJustified(4,'0'));
        }
        if (code == "ZZFM") {
            switch(hardware) {
            case HPSDRModel::ANAN10: case HPSDRModel::ANAN10E: return payload("0");
            case HPSDRModel::ANAN100: case HPSDRModel::ANAN100B: case HPSDRModel::ANAN100D: case HPSDRModel::ANAN200D: case HPSDRModel::ANAN7000D: case HPSDRModel::ANAN8000D: case HPSDRModel::ANVELINAPRO3: case HPSDRModel::ANAN_G2: case HPSDRModel::ANAN_G2_1K: return payload("1");
            default: return error();
            }
        }
        QByteArray name;
        switch(hardware) {
        case HPSDRModel::HPSDR: name="HPSDR"; break;
        case HPSDRModel::HERMES: name="HERMES"; break;
        case HPSDRModel::ANAN10: name="ANAN10"; break;
        case HPSDRModel::ANAN10E: name="ANAN10E"; break;
        case HPSDRModel::ANAN100: name="ANAN100"; break;
        case HPSDRModel::ANAN100B: name="ANAN100B"; break;
        case HPSDRModel::ANAN100D: name="ANAN100D"; break;
        case HPSDRModel::ANAN200D: name="ANAN200D"; break;
        case HPSDRModel::ORIONMKII: name="ORIONMKII"; break;
        case HPSDRModel::ANAN7000D: name="ANAN7000D"; break;
        case HPSDRModel::ANAN8000D: name="ANAN8000D"; break;
        case HPSDRModel::ANAN_G2: name="ANAN_G2"; break;
        case HPSDRModel::ANAN_G2_1K: name="ANAN_G2_1K"; break;
        case HPSDRModel::ANVELINAPRO3: name="ANVELINAPRO3"; break;
        case HPSDRModel::HERMESLITE: name="HERMESLITE"; break;
        case HPSDRModel::REDPITAYA: name="REDPITAYA"; break;
        case HPSDRModel::ANAN_G2E: name="ANAN_G2E"; break;
        default: return error();
        }
        return {CatResultKind::Wire,code+name+';'};
    }
    const CatVfo vfo=code == "ZZUY" || code == "ZZXO" || (code == "ZZSM" && request.suffix == "1") ? CatVfo::Secondary : CatVfo::Primary;
    const QPointer<SliceModel> slice(m_adapter.resolveSlice(binding,vfo));
    if (!slice || !m_adapter.mayRead(binding,vfo)) { return error(); }
    const CatWriteToken write=m_adapter.prepareWrite(binding,vfo,code);
    const auto valid=[&]() { return model && service && slice && service->session(context.sessionId) && m_adapter.revalidateWrite(write); };
    // From Thetis CAT/CATCommands.cs:7514-7543 [v2.10.3.15].
    //-W2PA  Out of alphabetical order a bit, but related to ZZVL above. 
    // [original inline comment from CATCommands.cs:7511]
    //       Added two functions to individually lock VFO A and B.
    // [original inline comment from CATCommands.cs:7512]
    //-W2PA  Lock VFOA  
    // [original inline comment from CATCommands.cs:7514]
    // From Thetis CAT/CATCommands.cs:7545-7574 [v2.10.3.15].
    //-W2PA  Lock VFOB  
    // [original inline comment from CATCommands.cs:7545]
    if (code == "ZZUX" || code == "ZZUY") {
        if (get) { return payload(slice->locked() ? "1":"0"); }
        if (!valid() || (request.suffix != "0" && request.suffix != "1")) { return error(); }
        slice->setLocked(request.suffix == "1"); return valid() ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:5237-5263 [v2.10.3.15].
    //Sets or reads the RX1 antenna //[2.3.10.6]MW0LGE https://github.com/ramdor/Thetis/issues/385
    // [original inline comment from CATCommands.cs:5236]
    // From Thetis CAT/CATCommands.cs:5273-5299 [v2.10.3.15].
    //Sets or reads the TX antenna //[2.3.10.6]MW0LGE https://github.com/ramdor/Thetis/issues/385
    // [original inline comment from CATCommands.cs:5272]
    if (code == "ZZOA" || code == "ZZOC") {
        const QPointer<SliceModel> target(code == "ZZOC" ? model->txBoundSlice() : slice.data());
        if (!target || !model->isConnected() || !model->boardCapabilities().hasAlexTxRouting) { return error(); }
        const CatVfo targetVfo=target == m_adapter.resolveSlice(binding,CatVfo::Primary) ? CatVfo::Primary : CatVfo::Secondary;
        if (m_adapter.resolveSlice(binding,targetVfo) != target || !m_adapter.mayRead(binding,targetVfo)) { return error(); }
        if (get) { return payload(number(code == "ZZOC" ? model->alexController().txAnt(target->band()) : model->alexController().rxAnt(target->band()),width)); }
        const CatWriteToken antennaWrite=m_adapter.prepareWrite(binding,targetVfo,code == "ZZOC" ? "txAntenna":"rxAntenna");
        bool ok=false; const int antenna=std::clamp(request.suffix.toInt(&ok),1,3);
        if (!ok || !valid() || !m_adapter.revalidateWrite(antennaWrite) || !model->otherDeviceHoldsRefusal().isEmpty()) { return error(); }
        if (code == "ZZOC" && ((antenna == 2 && model->alexController().blockTxAnt2()) || (antenna == 3 && model->alexController().blockTxAnt3()))) { return error(); }
        const QString name=QStringLiteral("ANT%1").arg(antenna);
        if (code == "ZZOC") { target->setTxAntenna(name); } else { target->setRxAntenna(name); }
        return valid() && target && m_adapter.revalidateWrite(antennaWrite) ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:8272-8304 [v2.10.3.15].
    //Reads RX1 combined status
    // [original inline comment from CATCommands.cs:8271]
    // strip to 3 bits
    // [original inline comment from CATCommands.cs:8280]
    // 3 bits, moved left
    // [original inline comment from CATCommands.cs:8282]
    // From Thetis CAT/CATCommands.cs:8307-8339 [v2.10.3.15].
    //Reads RX2 combined status
    // [original inline comment from CATCommands.cs:8306]
    // strip to 3 bits
    // [original inline comment from CATCommands.cs:8315]
    // 3 bits, moved left
    // [original inline comment from CATCommands.cs:8317]
    if (code == "ZZXN" || code == "ZZXO") {
        const StepAttenuatorFacade* facade=model->stepAttFacade();
        if (!get || !model->isConnected() || !facade || !facade->controller()) { return error(); }
        // From Thetis CAT/CATCommands.cs:3200-3241 [v2.10.3.15]. Explicit AGC literal ordering.
        constexpr std::array<AGCMode,6> kAgc{AGCMode::Off,AGCMode::Long,AGCMode::Slow,AGCMode::Med,AGCMode::Fast,AGCMode::Custom};
        const auto agc=std::find(kAgc.begin(),kAgc.end(),slice->agcMode());
        // From Thetis enums.cs:236-251 [v2.10.3.15]. Routed native preamp uses the same values.
        const int preamp=facade->preampModeForSlice(slice->sliceIndex());
        if (agc == kAgc.end() || preamp<0 || preamp>9) { return error(); }
        // From Thetis CAT/CATCommands.cs:8272-8342 [v2.10.3.15].
        constexpr int kThreeBits=7;
        constexpr int kPreampShift=3;
        constexpr int kSqlBit=1<<6;
        constexpr int kNb1Bit=1<<7;
        constexpr int kNb2Bit=1<<8;
        constexpr int kNr1Bit=1<<9;
        constexpr int kNr2Bit=1<<10;
        constexpr int kSnbBit=1<<11;
        constexpr int kAnfBit=1<<12;
        const DSPMode mode=slice->dspMode();
        const bool sql=mode == DSPMode::FM ? slice->fmsqEnabled() : mode == DSPMode::AM || mode == DSPMode::SAM || mode == DSPMode::DSB ? slice->amsqEnabled() : slice->ssqlEnabled();
        const int bits=int(agc-kAgc.begin())+((preamp&kThreeBits)<<kPreampShift)
            +(sql ? kSqlBit:0)+(slice->nbMode() == NbMode::NB ? kNb1Bit:0)+(slice->nbMode() == NbMode::NB2 ? kNb2Bit:0)
            +(slice->activeNr() == NrSlot::NR1 ? kNr1Bit:0)+(slice->activeNr() == NrSlot::NR2 ? kNr2Bit:0)
            +(slice->snbEnabled() ? kSnbBit:0)+(slice->anfEnabled() ? kAnfBit:0);
        return payload(number(bits,width));
    }
    // From Thetis CAT/CATCommands.cs:896-932 [v2.10.3.15].
    // Reads the S Meter value  //TODO modify to consider console.S9Frequency
    // [original inline comment from CATCommands.cs:895]
    // read the main transceiver s meter
    // [original inline comment from CATCommands.cs:901]
    // From Thetis CAT/CATCommands.cs:6290-6340 [v2.10.3.15].
    // Reads the S Meter value
    // [original inline comment from CATCommands.cs:6289]
    // read the main transceiver s meter
    // [original inline comment from CATCommands.cs:6294]
    //console.RX1FilterSizeCalOffset +
    // [original inline comment from CATCommands.cs:6309]
    //console.RX1FilterSizeCalOffset +
    // [original inline comment from CATCommands.cs:6318]
    //console.RX2FilterSizeCalOffset +
    // [original inline comment from CATCommands.cs:6326]
    // From Thetis CAT/CATCommands.cs:6012-6055 [v2.10.3.15].
    // Reads the Console RX meter
    // [original inline comment from CATCommands.cs:6011]
    if (code == "SM" || code == "ZZSM" || code == "ZZRM") {
        if (!get || !model->isConnected()) { return error(); }
        const bool transmitting=model->moxController()->isMox();
        if (code == "SM" || code == "ZZSM") {
            if (transmitting || (code == "SM" && request.suffix != "0" && request.suffix != "2") || (code == "ZZSM" && request.suffix != "0" && request.suffix != "1")) { return error(); }
            double reading=0; if (!m_adapter.readRxMeter(binding,vfo,RxMeterType::SignalPeak,reading)) { return error(); }
            // From Thetis CAT/CATCommands.cs:896-935,6290-6340 [v2.10.3.15].
            constexpr double kFloor=-140;
            constexpr double kCeiling=-10;
            constexpr double kSUnitOffset=127;
            constexpr double kSUnitWidth=6;
            constexpr double kS9=9.0F;
            constexpr double kKenwoodScale=1.6667;
            constexpr int kS9Value=15;
            constexpr int kS9Dbm=73;
            constexpr int kKenwoodMax=30;
            constexpr int kExtendedScale=2;
            const float value=float(std::clamp(reading,kFloor,kCeiling));
            int sm=(int(value)-int(kFloor))*kExtendedScale;
            if (code == "SM") {
                const double sx=std::max(0.0,(value+kSUnitOffset)/kSUnitWidth);
                sm=sx<=kS9 ? std::abs(int(sx*kKenwoodScale)) : kS9Value+int(value+kS9Dbm);
                sm=std::clamp(sm,0,kKenwoodMax);
            }
            return payload(number(sm,width));
        }
        // From Thetis console.cs:17603-17638,17641-17655,17693-17703 [v2.10.3.15].
        constexpr double kAlcFloor=-20.0;
        QByteArray reading;
        if (!transmitting) {
            const std::array<RxMeterType,4> types{RxMeterType::SignalPeak,RxMeterType::SignalAvg,RxMeterType::AdcPeak,RxMeterType::AdcAvg};
            bool ok=false; const int selector=request.suffix.toInt(&ok);
            if (!ok || selector<0 || selector>=int(types.size())) { return error(); }
            double value=0; if (!m_adapter.readRxMeter(binding,CatVfo::Primary,types[size_t(selector)],value)) { return error(); }
            reading=QByteArray::number(float(value),'f',1)+(selector<2 ? " dBm":" dBFS");
        } else if (request.suffix == "4") {
            const TxChannel* channel=model->txChannel();
            if (!channel || !channel->isWdspReady()) { return error(); }
            const double raw=channel->txMeter(TxMeterType::AlcAvg);
            if (!std::isfinite(raw)) { return error(); }
            const float value=std::max(float(kAlcFloor),-calculateTxMeter(ThetisTxMeterType::Alc,raw));
            reading=QByteArray::number(value,'f',1)+" dB";
        } else {
            const RadioStatus& status=model->radioStatus(); double value=0;
            if (request.suffix == "5") { value=status.forwardPowerWatts(); reading=QByteArray::number(value,'f',0)+" W"; }
            else if (request.suffix == "7") { value=status.reflectedPowerWatts(); reading=QByteArray::number(value,'f',0)+" W"; }
            else if (request.suffix == "8") { value=status.swrRatio(); reading=QByteArray::number(value,'f',1)+" : 1"; }
            else { return error(); }
            if (!std::isfinite(value)) { return error(); }
        }
        return payload(reading.rightJustified(width,' '));
    }
    // From Thetis CAT/CATCommands.cs:8552-8556 [v2.10.3.15].
    // The service consumes current serial ZZZZ at the command boundary; other transports reject it.
    if (code == "ZZZZ") { return error(); }
    return error();
}
} // namespace NereusSDR
