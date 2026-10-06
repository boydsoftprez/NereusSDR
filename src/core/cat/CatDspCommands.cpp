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
// 2026-10-04 - Slice-bound CAT DSP commands adapted by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.
#include "CatDspCommands.h"
#include "CatModelAdapter.h"
#include "CatService.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"
#include "core/SliceOwnership.h"
#include "core/StepAttenuatorFacade.h"
#include "core/StepAttenuatorController.h"
#include "core/session/SessionMessages.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalBlocker>
#include "core/BoardCapabilities.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace NereusSDR {
namespace {
CatCommandResult error() { return {CatResultKind::Error,"?;"}; }
CatCommandResult silence() { return {CatResultKind::Silence,{}}; }
CatCommandResult payload(QByteArray value) { return {CatResultKind::Payload,value}; }
// From Thetis CAT/CATParser.cs:385-409 [v2.10.3.15]. Capability error code fact.
constexpr int kUnavailable = 7;
// From Thetis CAT/CATCommands.cs:3648-3816,5031-5156 [v2.10.3.15].
constexpr int kPercentMaximum = 100;
// From Thetis CAT/CATCommands.cs:84-105 [v2.10.3.15].
constexpr double kKenwoodAfSetDivisor = 2.55;
constexpr double kKenwoodAfQueryDivisor = 0.392;
// From Thetis CAT/CATCommands.cs:935-970,6404-6447 [v2.10.3.15].
constexpr int kSliderMinimum = -160;
constexpr int kKenwoodSquelchMaximum = 255;
constexpr double kKenwoodSquelchScale = 0.62745;
//[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale
// [original inline comment from console.cs:47298]
//[2.10.3.5]MW0LGE reverted back to a -160 to 0 scale
// [original inline comment from console.cs:47310]
// [original inline comment from console.cs:47322]
// From Thetis console.cs:47279-47330 [v2.10.3.15].
constexpr float kVoicePercentMaximum = 100.0f;
constexpr float kSliderSpan = 160.0f;
constexpr double kSliderPerPercent = 1.6;
constexpr double kFmDbPerPercent = 0.4;
// From Thetis CAT/CATCommands.cs:5031-5156 [v2.10.3.15].
constexpr float kNrReductionMaximum = 20.0f;
// turn off 'auto agc' only if different MW0LGE_21k8
// [original inline comment from CATCommands.cs:1288]
// [original inline comment from CATCommands.cs:1326]
//-W2PA Sets or reads the APF tune
// [original inline comment from CATCommands.cs:1345]
//RX2ATT = att;        // Set the console control // MW0LGE_21d step atten changes
// [original inline comment from CATCommands.cs:6192]
// Get the console setting // MW0LGE_21d step atten changes
// [original inline comment from CATCommands.cs:6198]
// From Thetis CAT/CATCommands.cs:1273-1346,1346-1382,5344-5429,6159-6207 [v2.10.3.15].
constexpr int kAgcMinimum = -20;
constexpr int kAgcMaximum = 120;
constexpr int kApfTuneMaximum = 250;
constexpr int kDigitalOffsetMaximum = 5000;
constexpr int kAttenuationMaximum = 31;
// From Thetis CAT/CATCommands.cs:2014-2048 [v2.10.3.15].
constexpr int kPhaseMaximum = 18000;
constexpr double kPhaseHundredths = 100.0;
constexpr double kHalfPhaseDegrees = 180.0;
constexpr double kFullPhaseDegrees = 360.0;
// From Thetis CAT/CATCommands.cs:10520-10591 [v2.10.3.15].
constexpr std::array<int,7> kBufferSamples{256,512,1024,2048,4096,8192,16384};
// From Thetis CAT/CATCommands.cs:3200-3243 [v2.10.3.15].
constexpr std::array<AGCMode,6> kAgcModes{AGCMode::Off,AGCMode::Long,AGCMode::Slow,AGCMode::Med,AGCMode::Fast,AGCMode::Custom};
// From Thetis CAT/CATCommands.cs:4983-5031 [v2.10.3.15].
constexpr std::array<NrSlot,5> kNrSlots{NrSlot::Off,NrSlot::NR1,NrSlot::NR2,NrSlot::NR3,NrSlot::NR4};
//MW0LGE_21d
// [original inline comment from enums.cs:247]
// From Thetis enums.cs:236-251 [v2.10.3.15].
constexpr std::array<PreampMode,10> kPreampModes{PreampMode::Off,PreampMode::On,PreampMode::Minus10,PreampMode::Minus20,PreampMode::Minus30,PreampMode::Minus40,PreampMode::Minus50,PreampMode::SaMinus10,PreampMode::SaMinus20,PreampMode::SaMinus30};
QByteArray number(qint64 value,int width) { return QByteArray::number(value).rightJustified(width,'0'); }
QByteArray signedNumber(qint64 value,int width) { return (value < 0 ? QByteArray("-") : QByteArray("+"))+number(std::abs(value),width-1); }
int bufferIndex(int samples) { const auto found=std::find(kBufferSamples.begin(),kBufferSamples.end(),samples); return found == kBufferSamples.end() ? 0 : int(found-kBufferSamples.begin()); }
bool inGroup(DSPMode mode,const QString& group) {
    if (group == "Cw") { return mode == DSPMode::CWL || mode == DSPMode::CWU; }
    if (group == "Dig") { return mode == DSPMode::DIGL || mode == DSPMode::DIGU || mode == DSPMode::SPEC || mode == DSPMode::DRM; }
    return mode == DSPMode::USB || mode == DSPMode::LSB || mode == DSPMode::DSB || mode == DSPMode::AM || mode == DSPMode::SAM;
}
}
CatDspCommands::CatDspCommands(CatModelAdapter& adapter) : m_adapter(adapter) {}
QList<QByteArray> CatDspCommands::codes() { return {"AG","GT","NB","NT","SQ","ZZAA","ZZAB","ZZAG","ZZAP","ZZAR","ZZAS","ZZAT","ZZAY","ZZBI","ZZDB","ZZDC","ZZDD","ZZDE","ZZDF","ZZDG","ZZDH","ZZEA","ZZER","ZZGT","ZZGU","ZZHA","ZZHR","ZZHU","ZZHW","ZZLA","ZZLB","ZZLC","ZZLD","ZZLE","ZZLF","ZZLG","ZZLH","ZZMA","ZZMB","ZZNA","ZZNB","ZZNC","ZZND","ZZNE","ZZNF","ZZNG","ZZNH","ZZNL","ZZNM","ZZNN","ZZNO","ZZNR","ZZNS","ZZNT","ZZNU","ZZNV","ZZNW","ZZOL","ZZOU","ZZPA","ZZPB","ZZRX","ZZRY","ZZSO","ZZSQ","ZZSR","ZZSV","ZZSX","ZZVL","ZZXT","ZZZQ","ZZZR","ZZHT","ZZHV","ZZHX"}; }
CatCommandResult CatDspCommands::execute(const CatRequest& request,CatSessionContext& context)
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
    const QList<QByteArray> unavailable{"ZZAA","ZZAB","ZZAY","ZZDB","ZZDC","ZZDF","ZZDG","ZZDH","ZZEA","ZZER","ZZHA","ZZLC","ZZLD","ZZLG","ZZLH","ZZXT"};
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:1063]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:1098]
// AFP tYpe
// [original inline comment from CATCommands.cs:1400]
//Get the number of bands
// [original inline comment from CATCommands.cs:2512]
//Create the integer array
// [original inline comment from CATCommands.cs:2513]
//Get rid of the band count
// [original inline comment from CATCommands.cs:2514]
//Parse the string into the array
// [original inline comment from CATCommands.cs:2516]
//Remove the last three used
// [original inline comment from CATCommands.cs:2519]
//Send the array to the eq form
// [original inline comment from CATCommands.cs:2521]
//Get the equalizer array
// [original inline comment from CATCommands.cs:2526]
//Get the number of bands in the array
// [original inline comment from CATCommands.cs:2527]
//Holds a temporary value
// [original inline comment from CATCommands.cs:2528]
//The return string with the number of bands added
// [original inline comment from CATCommands.cs:2529]
//Loop thru the array
// [original inline comment from CATCommands.cs:2531]
//If the value is negative, format the answer
// [original inline comment from CATCommands.cs:2535]
//Add the padding if it's a 3 band eq
// [original inline comment from CATCommands.cs:2541]
    if (unavailable.contains(code)) { return {CatResultKind::Error,"?;",kUnavailable}; }
    const QList<QByteArray> secondary{"ZZAS","ZZGU","ZZLE","ZZLF","ZZMB","ZZNC","ZZND","ZZNF","ZZNH","ZZNO","ZZNU","ZZNV","ZZNW","ZZPB","ZZRY","ZZSV","ZZSX","ZZZR"};
    const CatVfo vfo=secondary.contains(code) ? CatVfo::Secondary : CatVfo::Primary;
    const QPointer<SliceModel> slice(m_adapter.resolveSlice(binding,vfo));
    if (!slice || !m_adapter.mayRead(binding,vfo)) { return error(); }
    const CatWriteToken write=m_adapter.prepareWrite(binding,vfo,code);
    const auto valid=[&]() { return model && service && slice && service->session(context.sessionId) && m_adapter.revalidateWrite(write); };
    const auto result=[&]() { return valid() ? silence() : error(); };
    bool parsed=false;
    const qint64 input=request.suffix.toLongLong(&parsed);
    const QList<QByteArray> bufferCodes{"ZZHR","ZZHT","ZZHU","ZZHV","ZZHW","ZZHX"};
    const bool bufferCommand=bufferCodes.contains(code);
    if (!get && ((!parsed && !bufferCommand) || !valid())) { return error(); }
    const int percent=int(std::clamp(input,qint64(0),qint64(kPercentMaximum)));
// Added 06/21/05 BT for CAT commands
// [original inline comment from setup.cs:5719]
//[2.10.3.5]MW0LGE no code here, TODO
// [original inline comment from setup.cs:5725]
    // From Thetis CAT/CATCommands.cs:4881-4900,6447-6470 [v2.10.3.15]. Source-inert setters.
//if(s == "1")
// [original inline comment from CATCommands.cs:6451]
//	console.SpurReduction = true;
// [original inline comment from CATCommands.cs:6452]
//else
// [original inline comment from CATCommands.cs:6453]
//	console.SpurReduction = false;
// [original inline comment from CATCommands.cs:6454]
//if(console.SpurReduction)
// [original inline comment from CATCommands.cs:6459]
//	return "1";
// [original inline comment from CATCommands.cs:6460]
//else
// [original inline comment from CATCommands.cs:6461]
    if (code == "ZZNM" || code == "ZZSR") {
        if (get) { return payload(number(0,width)); }
        if (code == "ZZSR" && request.suffix != "0" && request.suffix != "1") { return error(); }
        return result();
    }
    // From Thetis CAT/CATCommands.cs:84-105,1199-1223 [v2.10.3.15]. Master output, not slice AF.
// if the length of the parameter legal for setting this prefix
// [original inline comment from CATCommands.cs:86]
// scale 255:100 (Kenwood vs SDR)
// [original inline comment from CATCommands.cs:89]
// Set the console control
// [original inline comment from CATCommands.cs:90]
// if this is a read command
// [original inline comment from CATCommands.cs:93]
//				return AddLeadingZeros(console.AF);		// Get the console setting
// [original inline comment from CATCommands.cs:96]
// return a ?
// [original inline comment from CATCommands.cs:101]
// if the length of the parameter legal for setting this prefix
// [original inline comment from CATCommands.cs:1203]
// Set the console control
// [original inline comment from CATCommands.cs:1208]
// if this is a read command
// [original inline comment from CATCommands.cs:1212]
// Get the console setting
// [original inline comment from CATCommands.cs:1214]
// return a ?
// [original inline comment from CATCommands.cs:1218]
    if (code == "AG" || code == "ZZAG") {
        const QPointer<AudioEngine> audio(model->audioEngine());
        if (!audio) { return error(); }
        if (get) { const double value=audio->volume()*kPercentMaximum; return payload(number(qint64(std::nearbyint(code == "AG" ? value/kKenwoodAfQueryDivisor : value)),width)); }
        if (code == "AG") {
            const int raw=request.suffix.mid(1).toInt(&parsed); if (!parsed) { return error(); }
            audio->setVolume(float(std::clamp(std::nearbyint(raw/kKenwoodAfSetDivisor),0.0,double(kPercentMaximum)))/kPercentMaximum);
        } else { audio->setVolume(float(percent)/kPercentMaximum); }
        return result();
    }
    // From Thetis CAT/CATCommands.cs:281-291,3200-3243 [v2.10.3.15]. Explicit source AGC enum.
//Added padleft fix 4/2/2007 BT
// [original inline comment from CATCommands.cs:284]
    if (code == "GT" || code == "ZZGT" || code == "ZZGU") {
        if (get) { const auto found=std::find(kAgcModes.begin(),kAgcModes.end(),slice->agcMode()); return found == kAgcModes.end() ? error() : payload(number(found-kAgcModes.begin(),width)); }
        if (input < 0 || input >= qint64(kAgcModes.size())) { return error(); }
        slice->setAgcMode(kAgcModes[size_t(input)]); return result();
    }
// turn off 'auto agc' only if different MW0LGE_21k8
// [original inline comment from CATCommands.cs:1288]
// [original inline comment from CATCommands.cs:1326]
//-W2PA Sets or reads the APF tune
// [original inline comment from CATCommands.cs:1345]
    // From Thetis CAT/CATCommands.cs:1273-1346 [v2.10.3.15]. Changed manual threshold disables auto AGC.
// turn off 'auto agc' only if different MW0LGE_21k8
// [original inline comment from CATCommands.cs:1288]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:1300]
// turn off 'auto agc' only if different MW0LGE_21k8
// [original inline comment from CATCommands.cs:1326]
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:1338]
    if (code == "ZZAR" || code == "ZZAS") {
        if (get) { return payload(signedNumber(slice->agcThreshold(),width)); }
        const int value=int(std::clamp(input,qint64(kAgcMinimum),qint64(kAgcMaximum)));
        if (value != slice->agcThreshold() && slice->autoAgcEnabled()) { slice->setAutoAgcEnabled(false); if (!valid()) { return error(); } }
        slice->setAgcThreshold(value); return result();
    }
//-W2PA Sets or reads the APF tune
// [original inline comment from CATCommands.cs:1345]
    // From Thetis CAT/CATCommands.cs:1346-1382,5344-5429,4862-4881,5031-5156,3648-3816 [v2.10.3.15]. Typed native scalar setters.
// we have to remove the leading zero and replace it with the sign.
// [original inline comment from CATCommands.cs:1371]
    if (code == "ZZAT") { if (get) { return payload(signedNumber(slice->apfTuneHz(),width)); } slice->setApfTuneHz(int(std::clamp(input,qint64(-kApfTuneMaximum),qint64(kApfTuneMaximum)))); return result(); }
    if (code == "ZZOL" || code == "ZZOU") {
        if (get) { return payload(number(code == "ZZOL" ? slice->diglOffsetHz() : slice->diguOffsetHz(),width)); }
        const int value=int(std::clamp(input,qint64(0),qint64(kDigitalOffsetMaximum)));
        if (code == "ZZOL") { slice->setDiglOffsetHz(value); } else { slice->setDiguOffsetHz(value); } return result();
    }
    if (code == "ZZNL") { if (get) { return payload(number(slice->nb1Threshold(),width)); } slice->setNb1Threshold(int(input)); return result(); }
    if (code == "ZZNG" || code == "ZZNH") { if (get) { return payload(number(int((float(slice->nr4Reduction())/kNrReductionMaximum)*float(kPercentMaximum)),width)); } slice->setNr4Reduction((kNrReductionMaximum/float(kPercentMaximum))*percent); return result(); }
    if (code == "ZZLA" || code == "ZZLE") { if (get) { return payload(number(slice->afGain(),width)); } slice->setAfGain(percent); return result(); }
    if (code == "ZZLB" || code == "ZZLF") { if (get) { return payload(number(qint64(std::nearbyint(50+50*slice->audioPan())),width)); } slice->setAudioPan((percent-50)/50.0); return result(); }
    // From Thetis CAT/CATCommands.cs:647-685,4786-4862 [v2.10.3.15]. Disabling one NB does not disable the other.
//			if(s.Length == parser.nSet && (s == "0" || s == "1"))
// [original inline comment from CATCommands.cs:649]
//			{
// [original inline comment from CATCommands.cs:650]
//				console.CATNB1 = Convert.ToInt32(s);
// [original inline comment from CATCommands.cs:651]
//				return "";
// [original inline comment from CATCommands.cs:652]
//			}
// [original inline comment from CATCommands.cs:653]
//			else if(s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:654]
//			{
// [original inline comment from CATCommands.cs:655]
//				return console.CATNB1.ToString();
// [original inline comment from CATCommands.cs:656]
//			}
// [original inline comment from CATCommands.cs:657]
//			else
// [original inline comment from CATCommands.cs:658]
//			{
// [original inline comment from CATCommands.cs:659]
//				return parser.Error1;
// [original inline comment from CATCommands.cs:660]
//			}
// [original inline comment from CATCommands.cs:661]
    if (code == "NB" || code == "ZZNA" || code == "ZZNB" || code == "ZZNC" || code == "ZZND") {
        const NbMode target=(code == "ZZNB" || code == "ZZND") ? NbMode::NB2 : NbMode::NB;
        if (get) { return payload(slice->nbMode() == target ? "1" : "0"); }
        if (request.suffix != "0" && request.suffix != "1") { return error(); }
        if (input == 1 || slice->nbMode() == target) { slice->setNbMode(input == 1 ? target : NbMode::Off); } return result();
    }
//Sets or reads the RX1 antenna //[2.3.10.6]MW0LGE https://github.com/ramdor/Thetis/issues/385
// [original inline comment from CATCommands.cs:5236]
    // From Thetis CAT/CATCommands.cs:4936-5031,5191-5237 [v2.10.3.15]. Native NR admission remains authoritative.
    if (code == "ZZNE" || code == "ZZNF" || code == "ZZNR" || code == "ZZNS" || code == "ZZNV" || code == "ZZNW") {
        const bool selector=code == "ZZNE" || code == "ZZNF";
        const NrSlot target=(code == "ZZNS" || code == "ZZNW") ? NrSlot::NR2 : NrSlot::NR1;
        if (get) { if (!selector) { return payload(slice->activeNr() == target ? "1" : "0"); } const auto found=std::find(kNrSlots.begin(),kNrSlots.end(),slice->activeNr()); return found == kNrSlots.end() ? error() : payload(number(found-kNrSlots.begin(),width)); }
        if (selector ? (input < 0 || input >= qint64(kNrSlots.size())) : (request.suffix != "0" && request.suffix != "1")) { return error(); }
        const NrSlot desired=selector ? kNrSlots[size_t(input)] : (input == 1 ? target : (slice->activeNr() == target ? NrSlot::Off : slice->activeNr()));
        slice->setActiveNr(desired); return valid() && slice->activeNr() == desired ? silence() : error();
    }
//-W2PA Sets or reads the APF button on/off status
// [original inline comment from CATCommands.cs:1249]
    // From Thetis CAT/CATCommands.cs:1250-1273,666-685,4900-4936,5156-5191,3889-3943,8589-8642,1673-1698 [v2.10.3.15].
    const auto toggle=[&](bool old,auto setter) {
        if (get) { return payload(old ? "1" : "0"); }
        if (request.suffix != "0" && request.suffix != "1") { return error(); }
        (slice.data()->*setter)(input == 1); return result();
    };
//			if(s.Length == parser.nSet && (s == "0" || s == "1"))
// [original inline comment from CATCommands.cs:668]
//			{
// [original inline comment from CATCommands.cs:669]
//				console.CATANF = Convert.ToInt32(s);
// [original inline comment from CATCommands.cs:670]
//				return "";
// [original inline comment from CATCommands.cs:671]
//			}
// [original inline comment from CATCommands.cs:672]
//			else if(s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:673]
//			{
// [original inline comment from CATCommands.cs:674]
//				return console.CATANF.ToString();
// [original inline comment from CATCommands.cs:675]
//			}
// [original inline comment from CATCommands.cs:676]
//			else
// [original inline comment from CATCommands.cs:677]
//			{
// [original inline comment from CATCommands.cs:678]
//				return parser.Error1;
// [original inline comment from CATCommands.cs:679]
//			}
// [original inline comment from CATCommands.cs:680]
    if (code == "NT" || code == "ZZNT" || code == "ZZNU") { return toggle(slice->anfEnabled(),&SliceModel::setAnfEnabled); }
    if (code == "ZZNN" || code == "ZZNO") { return toggle(slice->snbEnabled(),&SliceModel::setSnbEnabled); }
//return console.APFbtn;
// [original inline comment from CATCommands.cs:1262]
    if (code == "ZZAP") { return toggle(slice->apfEnabled(),&SliceModel::setApfEnabled); }
    if (code == "ZZZQ" || code == "ZZZR") { return toggle(slice->autoAgcEnabled(),&SliceModel::setAutoAgcEnabled); }
    if (code == "ZZMA" || code == "ZZMB") { return toggle(slice->muted(),&SliceModel::setMuted); }
    if (code == "ZZBI") { return toggle(slice->binauralEnabled(),&SliceModel::setBinauralEnabled); }
//[2.10.3.5]MW0LGE 2 is vsql
// [original inline comment from CATCommands.cs:6390]
// [original inline comment from CATCommands.cs:6493]
    // From Thetis CAT/CATCommands.cs:6388-6404,6491-6507 [v2.10.3.15]. Normal and voice SQL are separate flags.
//[2.10.3.5]MW0LGE 2 is vsql
// [original inline comment from CATCommands.cs:6390]
//[2.10.3.5]MW0LGE 2 is vsql
// [original inline comment from CATCommands.cs:6493]
    if (code == "ZZSO" || code == "ZZSV") {
        const bool fm=slice->dspMode() == DSPMode::FM;
        if (get) { return payload(slice->ssqlEnabled() ? "2" : ((fm ? slice->fmsqEnabled() : slice->amsqEnabled()) ? "1" : "0")); }
        if (request.suffix != "0" && request.suffix != "1" && request.suffix != "2") { return error(); }
        slice->setSsqlEnabled(input == 2); if (!valid()) { return error(); }
        slice->setAmsqEnabled(input == 1 && !fm); if (!valid()) { return error(); }
        slice->setFmsqEnabled(input == 1 && fm); return result();
    }
    // From Thetis CAT/CATCommands.cs:935-970,6404-6447,6534-6579; console.cs:47279-47330,47637-47677 [v2.10.3.15].
// used by chkSquelch_CheckStateChanged
// [original inline comment from console.cs:47279]
// used by chkSquelch_CheckStateChanged
// [original inline comment from console.cs:47282]
// off //NOTE: no break here so that the sql threshold values are set, ready for us clicking the sql button
// [original inline comment from console.cs:47292]
// sql
// [original inline comment from console.cs:47294]
//FM Squelch
// [original inline comment from console.cs:47295]
//nValue = ptbSquelch.Value; // 0-100
// [original inline comment from console.cs:47297]
//[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale
// [original inline comment from console.cs:47298]
//[2.10.3.5]MW0LGE reverted back to a -160 to 0 scale
// [original inline comment from console.cs:47310]
// vsq
// [original inline comment from console.cs:47321]
//[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale
// [original inline comment from console.cs:47322]
// off
// [original inline comment from console.cs:47642]
// sql
// [original inline comment from console.cs:47644]
//FM Squelch
// [original inline comment from console.cs:47645]
//[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale
// [original inline comment from console.cs:47647]
// 0-100
// [original inline comment from console.cs:47657]
//[2.10.3.5]MW0LGE reverted back to a -160 to 0 scale
// [original inline comment from console.cs:47659]
// vsq
// [original inline comment from console.cs:47670]
//[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale
// [original inline comment from console.cs:47671]
// [2.9.3.5]MW0LGE reverted back
// [original inline comment from console.cs:19534]
// [2.9.3.5]MW0LGE reverted back to -160 to 0
// [original inline comment from console.cs:19539]
// [2.9.3.5]MW0LGE reverted back to -160 to 0
// [original inline comment from console.cs:19551]
// [2.9.3.5]MW0LGE reverted back to -160 to 0
// [original inline comment from console.cs:19556]
    // Nereus adaptation: reconstruct the shared slider from actual DSP thresholds.
    // Correct the upstream extended FM double-negation; retain enable flags without temporary GUI toggling.
//Will need code to select receiver when n Receivers enabled.
// [original inline comment from CATCommands.cs:940]
//for now, ignore rx number.
// [original inline comment from CATCommands.cs:941]
//convert to a double and add the scale factor (160 = 255)
// [original inline comment from CATCommands.cs:944]
// lower bound
// [original inline comment from CATCommands.cs:947]
// upper bound
// [original inline comment from CATCommands.cs:948]
//level = level*0.62745;				// scale factor
// [original inline comment from CATCommands.cs:949]
// scale factor
// [original inline comment from CATCommands.cs:950]
// return rx+AddLeadingZeros(console.Squelch).Substring(1);
// [original inline comment from CATCommands.cs:956]
// Map -160 to 0 to 0 to 255 for TS-2000 SQ command
// [original inline comment from CATCommands.cs:957]
// lower bound
// [original inline comment from CATCommands.cs:6416]
// upper bound
// [original inline comment from CATCommands.cs:6417]
// lower bound
// [original inline comment from CATCommands.cs:6546]
// upper bound
// [original inline comment from CATCommands.cs:6547]
    if (code == "SQ" || code == "ZZSQ" || code == "ZZSX") {
        if (!model->isConnected() || !model->ownsLocalDsp()) { return error(); }
        const bool voice=slice->ssqlEnabled(); const bool fm=slice->dspMode() == DSPMode::FM;
        const double offset=model->rxMeterOffsetDbForSlice(slice->sliceIndex());
        if (!std::isfinite(offset)) { return error(); }
        if (get) {
            const double raw=voice ? kSliderMinimum+kSliderPerPercent*slice->ssqlThresh() : (fm ? kSliderMinimum+kSliderPerPercent*(-slice->fmsqThresh()/kFmDbPerPercent) : slice->amsqThresh()+offset);
            const int slider=int(std::clamp(std::nearbyint(raw),double(kSliderMinimum),0.0));
            if (code == "SQ") { return payload(request.suffix.left(1)+number(qint64(std::nearbyint((1.0-std::abs(double(slider)/kSliderSpan))*kKenwoodSquelchMaximum)),width-1)); }
            return payload(number(std::abs(slider),width));
        }
        int slider=0;
        if (code == "SQ") { const qint64 raw=request.suffix.mid(1).toLongLong(&parsed); if (!parsed) { return error(); } slider=int(std::nearbyint(kSliderMinimum+std::clamp(raw,qint64(0),qint64(kKenwoodSquelchMaximum))*kKenwoodSquelchScale)); }
        else { slider=-int(std::clamp(input,qint64(0),qint64(fm ? kPercentMaximum : -kSliderMinimum))); }
        const int sqlPercent=int(((slider+kSliderSpan)/kSliderSpan)*kVoicePercentMaximum);
        if (voice) { slice->setSsqlThresh(sqlPercent); } else if (fm) { slice->setFmsqThresh(-kFmDbPerPercent*sqlPercent); } else { slice->setAmsqThresh(slider-offset); }
        return result();
    }
//-W2PA  Out of alphabetical order a bit, but related to ZZVL above. 
// [original inline comment from CATCommands.cs:7511]
//-W2PA  Lock VFOA  
// [original inline comment from CATCommands.cs:7514]
    // From Thetis CAT/CATCommands.cs:7473-7514 [v2.10.3.15]. Both legal suffixes cycle locks.
    if (code == "ZZVL") {
        const QPointer<SliceModel> second(m_adapter.resolveSlice(binding,CatVfo::Secondary));
        if (second && !m_adapter.mayRead(binding,CatVfo::Secondary)) { return error(); }
        if (get) { return payload(slice->locked() || (second && second->locked()) ? "1" : "0"); }
        if (request.suffix != "0" && request.suffix != "1") { return error(); }
        const bool nextPrimary=!(second && second->locked());
        const bool nextSecond=slice->locked() && !(second && second->locked());
        if (nextSecond && !second) { return error(); }
        const CatWriteToken secondWrite=m_adapter.prepareWrite(binding,CatVfo::Secondary,"locked");
        if (second && !m_adapter.revalidateWrite(secondWrite)) { return error(); }
        const bool hadSecond=bool(second);
        const bool primaryChanged=slice->locked() != nextPrimary;
        const bool secondChanged=second && second->locked() != nextSecond;
        {
            const QSignalBlocker primaryBlocker(slice.data());
            const QSignalBlocker secondaryBlocker(second.data());
            slice->setLocked(nextPrimary);
            if (second) { second->setLocked(nextSecond); }
        }
        // Both actual flags commit before callbacks. Revocation never rolls back a new owner's state.
        if (primaryChanged) { emit slice->lockedChanged(nextPrimary); }
        if (!valid() || (hadSecond && (!second || !m_adapter.revalidateWrite(secondWrite)))) { return error(); }
        if (secondChanged) { if (!second) { return error(); } emit second->lockedChanged(nextSecond); }
        return valid() && (!hadSecond || (second && m_adapter.revalidateWrite(secondWrite))) ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:3260-3368,10520-10591 [v2.10.3.15]. Global mode-group options via existing coalescer.
//console.DSPBufCWTX = width;
// [original inline comment from CATCommands.cs:3319]
//console.SetupForm.DSPCWTXBuffer = width;
// [original inline comment from CATCommands.cs:3320]
    if (code == "ZZHR" || code == "ZZHT" || code == "ZZHU" || code == "ZZHV" || code == "ZZHW" || code == "ZZHX") {
        const QString group=(code == "ZZHU" || code == "ZZHV") ? QStringLiteral("Cw") : ((code == "ZZHW" || code == "ZZHX") ? QStringLiteral("Dig") : QStringLiteral("Phone"));
        const bool tx=code == "ZZHT" || code == "ZZHX";
        const QString key=QStringLiteral("DspOptionsBufferSize")+group+(tx ? QStringLiteral("Tx") : QStringLiteral("Rx"));
        if (get) {
            int samples=AppSettings::instance().value(key,64).toInt();
            if (model->isConnected() && model->ownsLocalDsp()) {
                if (!tx && inGroup(slice->dspMode(),group)) { RxChannel* channel=model->rxChannelForSlice(slice->sliceIndex()); if (channel && channel->isWdspReady()) { samples=channel->dspBlockSize(); } }
                const SliceModel* target=model->txBoundSlice();
                if (tx && target && inGroup(target->dspMode(),group)) { TxChannel* channel=model->txChannel(); if (channel && channel->isWdspReady()) { samples=channel->txDspBlockSize(); } }
            }
            return payload(number(bufferIndex(samples),width));
        }
        if (request.suffix.size() != 1 || !QByteArray("0123456789Vv+-").contains(request.suffix[0])) { return error(); }
        if (code == "ZZHV") { return result(); }
        if (!model->isConnected() || !m_adapter.mayChangeGlobalDsp()) { return error(); }
        // Source switches the literal string, including default for V/v/+/- admitted by the parser.
        const int index=request.suffix[0]-'0';
        const int samples=index >= 0 && index < int(kBufferSamples.size()) ? kBufferSamples[size_t(index)] : kBufferSamples.front();
        AppSettings::instance().setValue(key,samples); if (!valid()) { return error(); }
        model->scheduleRemoteDspOptionsApply(key); return result();
    }
//RX2ATT = att;        // Set the console control // MW0LGE_21d step atten changes
// [original inline comment from CATCommands.cs:6192]
// Get the console setting // MW0LGE_21d step atten changes
// [original inline comment from CATCommands.cs:6198]
    // From Thetis CAT/CATCommands.cs:5443-5570,6159-6207 [v2.10.3.15]. Resolve the actual slice ADC in the bound facade.
// PreampMode e_mode = console.CATPreamp;
// [original inline comment from CATCommands.cs:5449]
//case "5":
// [original inline comment from CATCommands.cs:5525]
//    console.RX2PreampMode = PreampMode.HPSDR_MINUS40;
// [original inline comment from CATCommands.cs:5526]
//    break;
// [original inline comment from CATCommands.cs:5527]
//case "6":
// [original inline comment from CATCommands.cs:5528]
//    console.RX2PreampMode = PreampMode.HPSDR_MINUS50;
// [original inline comment from CATCommands.cs:5529]
//    break;
// [original inline comment from CATCommands.cs:5530]
//if (s.Length == parser.nGet)
// [original inline comment from CATCommands.cs:5549]
//{
// [original inline comment from CATCommands.cs:5550]
//    if (console.RX2PreampMode == PreampMode.HPSDR_OFF)
// [original inline comment from CATCommands.cs:5551]
//        return "0";
// [original inline comment from CATCommands.cs:5552]
//    else
// [original inline comment from CATCommands.cs:5553]
//        return "1";
// [original inline comment from CATCommands.cs:5554]
//}
// [original inline comment from CATCommands.cs:5555]
//else if (s.Length == parser.nSet)
// [original inline comment from CATCommands.cs:5556]
//{
// [original inline comment from CATCommands.cs:5557]
//    if (s == "1")
// [original inline comment from CATCommands.cs:5558]
//        console.RX2PreampMode = PreampMode.HPSDR_ON;
// [original inline comment from CATCommands.cs:5559]
//    else
// [original inline comment from CATCommands.cs:5560]
//        console.RX2PreampMode = PreampMode.HPSDR_OFF;
// [original inline comment from CATCommands.cs:5561]
//    return "";
// [original inline comment from CATCommands.cs:5562]
//}
// [original inline comment from CATCommands.cs:5563]
//else
// [original inline comment from CATCommands.cs:5564]
//    return parser.Error1;
// [original inline comment from CATCommands.cs:5565]
// if the length of the parameter legal for setting this prefix
// [original inline comment from CATCommands.cs:6163]
// Set the console control
// [original inline comment from CATCommands.cs:6168]
// if this is a read command
// [original inline comment from CATCommands.cs:6172]
// Get the console setting
// [original inline comment from CATCommands.cs:6174]
// return a ?
// [original inline comment from CATCommands.cs:6178]
// if the length of the parameter legal for setting this prefix
// [original inline comment from CATCommands.cs:6187]
//RX2ATT = att;        // Set the console control // MW0LGE_21d step atten changes
// [original inline comment from CATCommands.cs:6192]
// if this is a read command
// [original inline comment from CATCommands.cs:6196]
// Get the console setting // MW0LGE_21d step atten changes
// [original inline comment from CATCommands.cs:6198]
// return a ?
// [original inline comment from CATCommands.cs:6202]
    if (code == "ZZPA" || code == "ZZPB" || code == "ZZRX" || code == "ZZRY") {
        const QPointer<StepAttenuatorFacade> facade(model->stepAttFacade());
        if (!model->isConnected() || !facade || !facade->isBound() || !facade->controller()) { return error(); }
        const int id=slice->sliceIndex(); const bool preamp=code == "ZZPA" || code == "ZZPB";
        if (get) {
            if (!preamp) { return payload(number(facade->attenuationDbForSlice(id),width)); }
            const auto found=std::find(kPreampModes.begin(),kPreampModes.end(),PreampMode(facade->preampModeForSlice(id)));
            return found == kPreampModes.end() ? error() : payload(number(found-kPreampModes.begin(),width));
        }
        if (!preamp) { facade->setAttenuationDbForSlice(id,int(std::clamp(input,qint64(0),qint64(kAttenuationMaximum)))); return facade && result().kind == CatResultKind::Silence ? silence() : error(); }
        if (input < 0 || input >= qint64(kPreampModes.size())) { return error(); }
        if (input > (code == "ZZPB" ? 4 : 6)) { return result(); }
        const int mode=int(kPreampModes[size_t(input)]);
        const BoardCapabilities& caps=model->boardCapabilities();
        const auto items=facade->sliceUsesRx2(id) ? BoardCapsTable::rx2PreampItemsForBoard(caps.board) : BoardCapsTable::preampItemsForBoard(caps.board,caps.hasAlexFilters);
        if (!items.empty() && std::none_of(items.begin(),items.end(),[mode](const BoardCapsTable::PreampItem& item) { return item.modeInt == mode; })) { return error(); }
        facade->setPreampModeForSlice(id,mode);
        return facade && valid() && facade->preampModeForSlice(id) == mode ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:2014-2078 [v2.10.3.15]. Coordinated native diversity transaction, not a raw setter.
    if (code == "ZZDD") {
        if (slice->sliceIndex() != 0 || !model->boardCapabilities().hasDiversityReceiver || !model->isConnected()) { return error(); }
        if (get) { double phase=slice->diversityPhaseDeg(); if (phase > kHalfPhaseDegrees) { phase-=kFullPhaseDegrees; } return payload(signedNumber(qint64(std::nearbyint(phase*kPhaseHundredths)),width)); }
        double phase=double(std::clamp(input,qint64(-kPhaseMaximum),qint64(kPhaseMaximum)))/kPhaseHundredths;
        if (phase < 0) { phase+=kFullPhaseDegrees; }
        slice->setDiversityPhaseDeg(phase); return result();
    }
    if (code == "ZZDE") {
        const QJsonObject state=QJsonDocument::fromJson(model->diversityState().toUtf8()).object();
        const QJsonValue liveId=state.value("live").toObject().value("sliceId");
        const int source=liveId.isNull() || liveId.isUndefined() ? -1 : liveId.toInt(-1);
        if (source >= 0 && source != slice->sliceIndex()) { return error(); }
        if (get) { return payload(source == slice->sliceIndex() ? "1" : "0"); }
        if (request.suffix != "0" && request.suffix != "1") { return error(); }
        const int target=input == 1 ? slice->sliceIndex() : -1;
        const SliceOwnership* ownership=model->sliceOwnership();
        const auto field=[](const QByteArray& name,qint64 value) { return MirrorUpdate{0,name,MirrorWireKind::Int64,QVariant::fromValue<qint64>(value)}; };
        const auto identity=[ownership](int id,bool incarnation) -> qint64 { return id < 0 ? 0 : qint64(incarnation ? ownership->incarnation(id) : ownership->controlRevision(id)); };
        const SessionMessage invoke=SessionMessages::commandInvoke("diversity.setTarget",0,
            {{0,"enabled",MirrorWireKind::Bool,QVariant(input == 1)},field("stateRevision",qint64(model->diversityStateRevision())),
             field("sourceSliceId",source),field("sourceIncarnation",identity(source,true)),field("sourceControlRevision",identity(source,false)),
             field("targetSliceId",target),field("targetIncarnation",identity(target,true)),field("targetControlRevision",identity(target,false))});
        const SessionMessage reply=model->invokeDiversityAsStationDevice(invoke);
        return valid() && reply.accepted ? silence() : error();
    }
    return error();
}
}
