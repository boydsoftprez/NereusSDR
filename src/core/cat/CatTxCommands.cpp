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
// 2026-10-04 - Admit session-owned OFF before unrelated primary readability,
//              by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - TX/global CAT compatibility by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.
#include "CatTxCommands.h"
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
QByteArray signedNumber(qint64 value,int width) { return (value<0 ? QByteArray("-"):QByteArray("+"))+number(std::abs(value),width-1); }
} // namespace
CatTxCommands::CatTxCommands(CatModelAdapter& adapter, CatTxCoordinator& coordinator, CatSettings& settings) : m_adapter(adapter), m_coordinator(coordinator), m_settings(settings) {}
QList<QByteArray> CatTxCommands::codes() { return {"MG","MO","PC","RX","TX","ZZTU","ZZCP","ZZUS","ZZUT","ZZMG","ZZTH","ZZTL","ZZCT","ZZVE","ZZVG","ZZXH","ZZGE","ZZMO","ZZTX","ZZPC","ZZET","ZZTP","ZZEB","ZZTI","ZZTM","ZZTO","ZZLI"}; }
CatCommandResult CatTxCommands::execute(const CatRequest& request,CatSessionContext& context)
{
    const QPointer<RadioModel> model(&m_adapter.radioModel());
    const QPointer<CatService> service(model->catService());
    if (!service || !service->session(context.sessionId)) { return error(); }
    const QByteArray code=request.code;
    const bool get=request.form == CatForm::Get;
    // Owned OFF is session cleanup, independent of unrelated RX readability.
    // The coordinator retains kind, session, tag and accepted-generation checks.
    const bool ownedOff=code == "RX" || (!get && request.suffix == "0"
        && (code == "ZZTX" || code == "ZZTU" || code == "ZZUT"));
    if (ownedOff) {
        if (!context.transmitAllowed) { return error(); }
        const CatTransmitKind kind=code == "ZZTU" ? CatTransmitKind::Tune
            : code == "ZZUT" ? CatTransmitKind::TwoTone : CatTransmitKind::Ptt;
        m_coordinator.releaseTransmit(context.sessionId,kind);
        return model && service ? silence() : error();
    }
    const CatBinding binding=service->session(context.sessionId)->binding();
    const QPointer<SliceModel> primary(m_adapter.resolveSlice(binding,CatVfo::Primary));
    if (!primary || !m_adapter.mayRead(binding,CatVfo::Primary)) { return error(); }
    const QPointer<TransmitModel> tx(&model->transmitModel());
    static const CatCommandCatalog catalog;
    const CatDescriptor* descriptor=catalog.find(code);
    if (!descriptor) { return error(); }
    const int width=descriptor->answerWidth;
    const CatWriteToken token=m_adapter.prepareWrite(binding,CatVfo::Primary,code);
    // Reuse station authority, including the installed foreign-holder probe.
    // This does not impose an off-air restriction on live transmitter controls.
    const auto valid=[&]() {
        if (!model || !service || !tx || !primary || !service->session(context.sessionId)
            || !m_adapter.revalidateWrite(token) || !model->ownsLocalDsp()
            || !model->otherDeviceHoldsRefusal().isEmpty()) { return false; }
        const MoxController* mox=model->moxController();
        return !mox || !mox->isMox() || mox->currentKeyer().isStation();
    };
    const auto result=[&]() { return valid() ? silence() : error(); };
    bool parsed=false; const qint64 input=request.suffix.toLongLong(&parsed);
    // From Thetis CAT/CATCommands.cs:721-754,5532-5567,6795-6856 [v2.10.3.15].
    constexpr int kPercentMax=100;
    // From Thetis CAT/CATCommands.cs:594-616,3910-3956 [v2.10.3.15].
    constexpr double kMicSetDivisor=1.43;
    constexpr double kMicReadDivisor=.7;
    constexpr int kMicMinimum=-50;
    constexpr int kMicMaximum=70;
    // From Thetis CAT/CATCommands.cs:1841-1861 [v2.10.3.15]; console.Designer.cs:6042-6043.
    constexpr int kCompressorMaximum=20;
    // From Thetis CAT/CATCommands.cs:7347-7378 [v2.10.3.15]; console.Designer.cs:6018-6019.
    constexpr int kVoxScale=1000;
    constexpr int kVoxFloor=-80;
    // From Thetis CAT/CATCommands.cs:8243-8269,6704-6724,6748-6768 [v2.10.3.15].
    constexpr int kVoxHangMaximum=4000;
    constexpr int kHighMinimum=500;
    constexpr int kHighMaximum=20000;
    constexpr int kLowMaximum=2000;
    const int percent=int(std::clamp(input,qint64(0),qint64(kPercentMax)));
    // From Thetis CAT/CATCommands.cs:822-827 [v2.10.3.15].
    // Sets or reads the transceiver receive mode status
    // [original inline comment from CATCommands.cs:820]
    // write only but spec shows an answer parameter for a read???
    // [original inline comment from CATCommands.cs:821]
    //return ZZTX("0");
    // [original inline comment from CATCommands.cs:826]
    // From Thetis CAT/CATCommands.cs:970-975 [v2.10.3.15].
    // Sets the transmitter on, write only
    // [original inline comment from CATCommands.cs:967]
    // will eventually need eiter Commander change or ZZ code
    // [original inline comment from CATCommands.cs:968]
    // since it is not CAT compliant as it is
    // [original inline comment from CATCommands.cs:969]
    //return ZZTX("1");
    // [original inline comment from CATCommands.cs:974]
    // From Thetis CAT/CATCommands.cs:6961-6981 [v2.10.3.15].
    //Sets or reads the MOX button status
    // [original inline comment from CATCommands.cs:6960]
    // From Thetis CAT/CATCommands.cs:6908-6932 [v2.10.3.15].
    // Sets or reads the TUN button on/off status
    // [original inline comment from CATCommands.cs:6907]
    // From Thetis CAT/CATCommands.cs:7049-7077 [v2.10.3.15].
    //-W2PA  Toggle two tone test  
    // [original inline comment from CATCommands.cs:7049]
    if (code == "RX" || code == "TX" || code == "ZZTX" || code == "ZZTU" || code == "ZZUT") {
        if (get && code != "RX" && code != "TX") {
            if (code == "ZZTU") { return payload(model->isTune() ? "1":"0"); }
            if (code == "ZZUT") { const TwoToneController* tones=model->twoToneController(); return tones ? payload(tones->isActive() ? "1":"0") : error(); }
            return payload(model->moxController()->isMox() ? "1":"0");
        }
        if (code != "RX" && code != "TX" && request.suffix != "0" && request.suffix != "1") { return error(); }
        if (!context.transmitAllowed) { return error(); }
        if (!valid()) { return error(); }
        const QPointer<SliceModel> selected(model->txBoundSlice());
        if (!selected) { return error(); }
        const CatVfo vfo=selected == primary ? CatVfo::Primary : CatVfo::Secondary;
        if (m_adapter.resolveSlice(binding,vfo) != selected || !m_adapter.mayChange(binding,vfo,"mox")) { return error(); }
        const CatTransmitKind kind=code == "ZZTU" ? CatTransmitKind::Tune : code == "ZZUT" ? CatTransmitKind::TwoTone : CatTransmitKind::Ptt;
        return m_coordinator.requestTransmit(context.sessionId,selected->sliceIndex(),kind) ? silence() : error();
    }
    // From Thetis CAT/CATCommands.cs:7043-7047 [v2.10.3.15].
    //-W2PA  Initiate PS Single Cal
    // [original inline comment from CATCommands.cs:7043]
    // From Thetis CAT/CATCommands.cs:3865-3889 [v2.10.3.15].
    // Sets or reads the PS-A button on/off status
    // [original inline comment from CATCommands.cs:3864]
    if (code == "ZZUS" || code == "ZZLI") {
        const QPointer<PureSignal> ps(model->pureSignal());
        if (!model->isConnected() || !ps || !ps->canActuate() || !model->boardCapabilities().hasPureSignal) { return {CatResultKind::Error,"?;",kUnavailable}; }
        if (code == "ZZLI" && get) { return payload(ps->isAutoCalEnabled() ? "1":"0"); }
        if (!valid()) { return error(); }
        if (code == "ZZUS") {
            // Existing PureSignal::singleCalibrate capability refuses disabled processing.
            if (!context.transmitAllowed || !ps->runCalibrationProcessing() || model->stationOnAirRefusal(nullptr)) { return error(); }
            ps->singleCalibrate();
        } else {
            if (request.suffix != "0" && request.suffix != "1") { return error(); }
            ps->setAutoCalEnabled(request.suffix == "1");
        }
        return result();
    }
    // From Thetis CAT/CATCommands.cs:6727-6745 [v2.10.3.15].
    //Inhibits power output when using external antennas, tuners, etc.
    // [original inline comment from CATCommands.cs:6726]
    if (code == "ZZTI") {
        if (get || (request.suffix != "0" && request.suffix != "1") || !valid()) { return error(); }
        model->setRxOnly(request.suffix == "1"); return result();
    }
    // From Thetis CAT/CATCommands.cs:2549-2587 [v2.10.3.15].
    //Sets or reads the TX EQ settings
    // [original inline comment from CATCommands.cs:2548]
    //Get the number of bands
    // [original inline comment from CATCommands.cs:2553]
    //Create the integer array
    // [original inline comment from CATCommands.cs:2554]
    //Get rid of the band count
    // [original inline comment from CATCommands.cs:2555]
    //Parse the string into the array
    // [original inline comment from CATCommands.cs:2557]
    //Remove the last three used
    // [original inline comment from CATCommands.cs:2560]
    //Send the array to the eq form
    // [original inline comment from CATCommands.cs:2562]
    //Get the equalizer array
    // [original inline comment from CATCommands.cs:2567]
    //Get the number of bands in the array
    // [original inline comment from CATCommands.cs:2568]
    //Holds a temporary value
    // [original inline comment from CATCommands.cs:2569]
    //The return string with the number of bands added
    // [original inline comment from CATCommands.cs:2570]
    //Loop thru the array
    // [original inline comment from CATCommands.cs:2572]
    //If the value is negative, format the answer
    // [original inline comment from CATCommands.cs:2576]
    //Add the padding if it's a 3 band eq
    // [original inline comment from CATCommands.cs:2582]
    if (code == "ZZEB") {
        // Correct upstream three/ten-band array mismatch. All eleven fields
        // validate before any native signal; keep frequencies and EQ mode.
        constexpr int kEqBands=10;
        constexpr int kEqWidth=36;
        // Native TransmitModel.cpp:3551-3573 remains the approved gain range.
        constexpr int kEqMinimum=-12;
        constexpr int kEqMaximum=15;
        if (get) {
            QByteArray value="010";
            const auto gain=[](int v) { return v<0 ? QByteArray("-")+number(-v,2) : number(v,3); };
            value+=gain(tx->txEqPreamp()); for(int i=0;i<kEqBands;++i) { value+=gain(tx->txEqBand(i)); }
            return payload(value);
        }
        if (!valid() || request.suffix.size()!=kEqWidth || request.suffix.left(3)!="010") { return error(); }
        std::array<int,kEqBands+1> values{};
        static const QRegularExpression kGain(QStringLiteral("^[+-]?[0-9]+$"));
        for(int i=0;i<=kEqBands;++i) {
            const QByteArray field=request.suffix.mid(3+3*i,3); bool ok=false;
            const int gain=field.toInt(&ok);
            if (!ok || !kGain.match(QString::fromLatin1(field)).hasMatch()) { return error(); }
            // Existing production EQ gain range remains authoritative.
            values[size_t(i)]=std::clamp(gain,kEqMinimum,kEqMaximum);
        }
        tx->setTxEqPreamp(values[0]); if (!valid()) { return error(); }
        for(int i=0;i<kEqBands;++i) { tx->setTxEqBand(i,values[size_t(i+1)]); if (!valid()) { return error(); } }
        return result();
    }
    // From Thetis CAT/CATCommands.cs:6860-6878 [v2.10.3.15].
    //Sets or reads the TX Profile
    // [original inline comment from CATCommands.cs:6859]
    if (code == "ZZTP") {
        const QPointer<MicProfileManager> manager(model->micProfileManager()); if (!manager) { return error(); }
        const QStringList names=manager->profileNames();
        if (get) { const int index=names.indexOf(manager->activeProfileName()); return index>=0 && index<100 ? payload(number(index,width)) : error(); }
        if (!parsed || input<0 || input>=names.size() || !valid()) { return error(); }
        QString reason;
        if (!model->selectTxProfileForStation(names[int(input)],&reason,false)) { return error(); }
        return result();
    }
    const bool literalToggle=code == "MO" || code == "ZZMO" || code == "ZZCP";
    if (!get && ((!parsed && !literalToggle) || !valid())) { return error(); }
    // From Thetis CAT/CATCommands.cs:594-616 [v2.10.3.15].
    // Sets or reads the Mic Gain thumbwheel
    // [original inline comment from CATCommands.cs:593]
    // scale 100:70 (Kenwood vs SDR)
    // [original inline comment from CATCommands.cs:602]
    // From Thetis CAT/CATCommands.cs:4177-4206 [v2.10.3.15].
    //Sets or reads the Mic gain control
    // [original inline comment from CATCommands.cs:4176]
    //[2.10.3.6]MW0LGE can also have -. Could have changed catsructs but not sure on cat msg formats from other sources other than midi so left with the +1
    // [original inline comment from CATCommands.cs:4189]
    // we have to remove the leading zero and replace it with the sign.
    // [original inline comment from CATCommands.cs:4201]
    if (code == "MG" || code == "ZZMG") {
        if (get) { return payload(code == "MG" ? number(qint64(std::nearbyint(tx->micGainDb()/kMicReadDivisor)),width) : signedNumber(tx->micGainDb(),width)); }
        const int value=code == "MG" ? int(std::nearbyint(percent/kMicSetDivisor)) : int(std::clamp(input,qint64(kMicMinimum),qint64(kMicMaximum)));
        tx->setMicGainDb(value); return result();
    }
    // From Thetis CAT/CATCommands.cs:698-717 [v2.10.3.15].
    // Sets or reads the PA output thumbwheel
    // [original inline comment from CATCommands.cs:697]
    //			int pwr = 0;
    // [original inline comment from CATCommands.cs:700]
    //
    // [original inline comment from CATCommands.cs:701]
    //			if(s.Length == parser.nSet)
    // [original inline comment from CATCommands.cs:702]
    //			{
    // [original inline comment from CATCommands.cs:703]
    //				pwr = Convert.ToInt32(s);
    // [original inline comment from CATCommands.cs:704]
    //				console.PWR = pwr;
    // [original inline comment from CATCommands.cs:705]
    //				return "";
    // [original inline comment from CATCommands.cs:706]
    //			}
    // [original inline comment from CATCommands.cs:707]
    //			else if(s.Length == parser.nGet)
    // [original inline comment from CATCommands.cs:708]
    //			{
    // [original inline comment from CATCommands.cs:709]
    //				return AddLeadingZeros(console.PWR);
    // [original inline comment from CATCommands.cs:710]
    //			}
    // [original inline comment from CATCommands.cs:711]
    //			else
    // [original inline comment from CATCommands.cs:712]
    //			{
    // [original inline comment from CATCommands.cs:713]
    //				return parser.Error1;
    // [original inline comment from CATCommands.cs:714]
    //			}
    // [original inline comment from CATCommands.cs:715]
    // From Thetis CAT/CATCommands.cs:5570-5592 [v2.10.3.15].
    //Sets or reads the Drive level
    // [original inline comment from CATCommands.cs:5569]
    //MW0LGE_22b
    // [original inline comment from CATCommands.cs:5583]
    // From Thetis CAT/CATCommands.cs:6795-6856 [v2.10.3.15].
    //Sets or reads the Tune Power level
    // [original inline comment from CATCommands.cs:6794]
    // check the min/max control settings
    // [original inline comment from CATCommands.cs:6799]
    //MW0LGE_22b changed
    // [original inline comment from CATCommands.cs:6805]
    //if (console.TXTunePower) console.PWR = tl; //MW0LGE_22b changed
    // [original inline comment from CATCommands.cs:6820]
    //else console.SetupForm.TunePower = tl;
    // [original inline comment from CATCommands.cs:6821]
    // if this is a read command
    // [original inline comment from CATCommands.cs:6825]
    //MW0LGE_22b changed
    // [original inline comment from CATCommands.cs:6848]
    //if (console.TXTunePower) return AddLeadingZeros(console.PWR);
    // [original inline comment from CATCommands.cs:6849]
    //else return AddLeadingZeros(console.SetupForm.TunePower);
    // [original inline comment from CATCommands.cs:6850]
    // return a ?
    // [original inline comment from CATCommands.cs:6854]
    if (code == "PC" || code == "ZZPC" || code == "ZZTO") {
        const DrivePowerSource source=code == "ZZTO" ? tx->tuneDrivePowerSource() : DrivePowerSource::DriveSlider;
        int value=tx->power();
        if (source == DrivePowerSource::TuneSlider) { if (!tx->tuneTxBandKnown() || !model->txBoundSlice()) { return error(); } value=tx->tunePowerForTxBand(); }
        if (source == DrivePowerSource::Fixed) { value=tx->tunePower(); }
        if (get) {
            if (service->globalConfig().limitReportedPower) {
                if (source == DrivePowerSource::DriveSlider && tx->powerSliderLimitEnabled()) { value=std::min(value,tx->powerLimit()); }
                if (source == DrivePowerSource::TuneSlider) { value=std::min(value,tx->tunePowerLimit()); }
            }
            return payload(number(value,width));
        }
        if (source == DrivePowerSource::DriveSlider) { tx->setPower(std::min(percent,rfPowerSliderMaxFor(model->hardwareProfile().model))); }
        else if (source == DrivePowerSource::TuneSlider) { tx->setTunePowerForBand(model->txBoundSlice()->band(),percent); }
        else { tx->setTunePower(percent); }
        return result();
    }
    // From Thetis CAT/CATCommands.cs:619-640 [v2.10.3.15].
    // Sets or reads the Monitor status
    // [original inline comment from CATCommands.cs:618]
    //			if(s.Length == parser.nSet)
    // [original inline comment from CATCommands.cs:621]
    //			{
    // [original inline comment from CATCommands.cs:622]
    //				if(s == "0")
    // [original inline comment from CATCommands.cs:623]
    //					console.MON = false;
    // [original inline comment from CATCommands.cs:624]
    //				else if(s == "1")
    // [original inline comment from CATCommands.cs:625]
    //					console.MON = true;
    // [original inline comment from CATCommands.cs:626]
    //				return "";
    // [original inline comment from CATCommands.cs:627]
    //			}
    // [original inline comment from CATCommands.cs:628]
    //			else if(s.Length == parser.nGet)
    // [original inline comment from CATCommands.cs:629]
    //			{
    // [original inline comment from CATCommands.cs:630]
    //				bool retval = console.MON;
    // [original inline comment from CATCommands.cs:631]
    //				if(retval)
    // [original inline comment from CATCommands.cs:632]
    //					return "1";
    // [original inline comment from CATCommands.cs:633]
    //				else
    // [original inline comment from CATCommands.cs:634]
    //					return "0";
    // [original inline comment from CATCommands.cs:635]
    //			}
    // [original inline comment from CATCommands.cs:636]
    //			else
    // [original inline comment from CATCommands.cs:637]
    //				return parser.Error1;
    // [original inline comment from CATCommands.cs:638]
    // From Thetis CAT/CATCommands.cs:4253-4273 [v2.10.3.15].
    //Sets or reads the Monitor (MON) button status
    // [original inline comment from CATCommands.cs:4252]
    // From Thetis CAT/CATCommands.cs:1858-1876 [v2.10.3.15].
    // Sets or reads the compander button status
    // [original inline comment from CATCommands.cs:1857]
    // From Thetis CAT/CATCommands.cs:7249 [v2.10.3.15].
    //Reads or sets the VOX Enable button status
    // [original inline comment from CATCommands.cs:7248]
    // From Thetis CAT/CATCommands.cs:3140-3162 [v2.10.3.15].
    // Sets or reads the noise gate enable button status
    // [original inline comment from CATCommands.cs:3139]
    // From Thetis CAT/CATCommands.cs:2632-2648 [v2.10.3.15].
    //Sets or reads the TXEQ button status
    // [original inline comment from CATCommands.cs:2631]
    if (code == "MO" || code == "ZZMO" || code == "ZZCP" || code == "ZZVE" || code == "ZZGE" || code == "ZZET") {
        if (get) {
            const bool value=code == "ZZCP" ? tx->cpdrOn() : code == "ZZVE" ? tx->voxEnabled() : code == "ZZGE" ? tx->dexpEnabled() : code == "ZZET" ? tx->txEqEnabled() : tx->monEnabled();
            return payload(value ? "1":"0");
        }
        if (request.suffix != "0" && request.suffix != "1") { return code == "MO" || code == "ZZMO" || code == "ZZCP" ? result() : error(); }
        const bool on=request.suffix == "1";
        if (code == "ZZCP") { tx->setCpdrOn(on); }
        else if (code == "ZZVE") { if (on && !context.transmitAllowed) { return error(); } tx->setVoxEnabled(on); }
        else if (code == "ZZGE") { tx->setDexpEnabled(on); }
        else if (code == "ZZET") { tx->setTxEqEnabled(on); }
        else { tx->setMonEnabled(on); }
        return result();
    }
    // From Thetis CAT/CATCommands.cs:1903-1926 [v2.10.3.15].
    //Reads or sets the compander threshold
    // [original inline comment from CATCommands.cs:1902]
    //[2.10.3.6]MW0LGE was 0
    // [original inline comment from CATCommands.cs:1909]
    //was 20
    // [original inline comment from CATCommands.cs:1910]
    // From Thetis CAT/CATCommands.cs:7347-7378 [v2.10.3.15].
    //Reads or set the VOX Gain control
    // [original inline comment from CATCommands.cs:7346]
    // From Thetis CAT/CATCommands.cs:8243-8269 [v2.10.3.15].
    //Reads or set the VOX Delay control
    // [original inline comment from CATCommands.cs:8242]
    // From Thetis CAT/CATCommands.cs:6704-6724 [v2.10.3.15].
    // Sets or reads the TX filter high setting
    // [original inline comment from CATCommands.cs:6703]
    // check the min/max control settings
    // [original inline comment from CATCommands.cs:6708]
    // if this is a read command
    // [original inline comment from CATCommands.cs:6716]
    // return a ?
    // [original inline comment from CATCommands.cs:6722]
    // From Thetis CAT/CATCommands.cs:6748-6768 [v2.10.3.15].
    // Sets or reads the TX filter low setting
    // [original inline comment from CATCommands.cs:6747]
    // check the min/max control settings
    // [original inline comment from CATCommands.cs:6752]
    // if this is a read command
    // [original inline comment from CATCommands.cs:6760]
    // return a ?
    // [original inline comment from CATCommands.cs:6766]
    // From Thetis CAT/CATCommands.cs:6771-6791 [v2.10.3.15].
    //Sets or reads the TX Monitor level
    // [original inline comment from CATCommands.cs:6770]
    // check the min/max control settings
    // [original inline comment from CATCommands.cs:6775]
    // if this is a read command
    // [original inline comment from CATCommands.cs:6783]
    // return a ?
    // [original inline comment from CATCommands.cs:6789]
    if (code == "ZZCT") { if (get) { return payload(number(tx->cpdrLevelDb(),width)); } tx->setCpdrLevelDb(int(std::clamp(input,qint64(0),qint64(kCompressorMaximum)))); return result(); }
    if (code == "ZZVG") { if (get) { return payload(number((tx->voxThresholdDb()-kVoxFloor)*kVoxScale/(-kVoxFloor),width)); } tx->setVoxThresholdDb(int(kVoxFloor+(-kVoxFloor)*std::clamp(input,qint64(0),qint64(kVoxScale))/double(kVoxScale))); return result(); }
    if (code == "ZZXH") { if (get) { return payload(number(tx->voxHangTimeMs(),width)); } tx->setVoxHangTimeMs(int(std::clamp(input,qint64(0),qint64(kVoxHangMaximum)))); return result(); }
    if (code == "ZZTH") { if (get) { return payload(number(tx->filterHigh(),width)); } tx->setFilterHigh(int(std::clamp(input,qint64(kHighMinimum),qint64(kHighMaximum)))); return result(); }
    if (code == "ZZTL") { if (get) { return payload(number(tx->filterLow(),width)); } tx->setFilterLow(int(std::clamp(input,qint64(0),qint64(kLowMaximum)))); return result(); }
    if (code == "ZZTM") { if (get) { return payload(number(qint64(std::nearbyint(tx->monitorVolume()*kPercentMax)),width)); } tx->setMonitorVolume(float(percent)/kPercentMax); return result(); }
    return error();
}
} // namespace NereusSDR
