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

// Ported from Thetis Project Files/Source/Console/setup.cs , setup.Designer.cs and console.cs
// Upstream setup.Designer.cs has no top-of-file GPL header; project-level LICENSE applies.
// Modification history (NereusSDR):
// 2026-10-04 - CAT preference defaults adapted for native service by J.J. Boyd
//              (KG4VCF), AI-assisted via OpenAI Codex.

#pragma once
#include "CatTypes.h"
#include <QString>
namespace NereusSDR {
namespace CatDefaults {
// From Thetis console.cs:2335 [v2.10.3.15].
// TCPIPCat
// [original inline comment from console.cs:2334]
constexpr int kFirstTcpPort = 13013;
// From Thetis setup.cs:351-354 [v2.10.3.15].
constexpr int kSerialBaud = 115200;
constexpr int kSerialDataBits = 8;
// From Thetis setup.cs:5911,5924 [v2.10.3.15].
constexpr int kRttyOffsetHz = 2125;
// From Thetis setup.Designer.cs:59587-59596 [v2.10.3.15].
constexpr int kRttyMinimumHz = -3000;
constexpr int kRttyMaximumHz = 3000;
}

// Nereus logical channels and transport enablement are approved design defaults.
struct CatEndpointConfig {
    bool operator==(const CatEndpointConfig&) const = default;
    int channel{1};
    CatBinding binding;
    bool tcpEnabled{false}; bool serialEnabled{false}; bool ptyEnabled{false}; bool rigctldEnabled{false};
    QString tcpBindAddress{"127.0.0.1"}; QString rigctldBindAddress{"127.0.0.1"};
    int tcpPort{0}; int rigctldPort{0};
    QString serialDevice;
    // From Thetis setup.cs:351-354 [v2.10.3.15].
    int serialBaud{CatDefaults::kSerialBaud}; QString serialParity{"None"}; int serialDataBits{CatDefaults::kSerialDataBits}; QString serialStopBits{"1"};
    QString ptyDialect{"Thetis"};
};
struct CatGlobalConfig {
    bool operator==(const CatGlobalConfig&) const = default;
    // Welcome is deliberately off in the approved Nereus design.
    bool sendWelcome{false};
    // From Thetis setup.cs:355 [v2.10.3.15].
    QString rigIdentity{"TS-2000"};
    bool allowKenwoodAi{false};
    // From Thetis console.cs:17707 [v2.10.3.15].
    bool aiEnabled{false};
    // From Thetis setup.Designer.cs:59363,59417 [v2.10.3.15].
    bool aiSerial1{true}; bool aiSerial2{false}; bool aiSerial3{false}; bool aiSerial4{false}; bool aiTcp{true};
    bool digitalReportsSideband{false}; bool recenterVfo{false};
    // From Thetis setup.Designer.cs:59482 [v2.10.3.15].
    QString serialNumber{"0000-0000"};
    // From Thetis setup.cs:384 [v2.10.3.15].
    bool limitReportedPower{true};
    bool rttyOffsetAEnabled{false}; bool rttyOffsetBEnabled{false};
    // From Thetis setup.cs:5911,5924 [v2.10.3.15].
    int rttyDiguHz{CatDefaults::kRttyOffsetHz}; int rttyDiglHz{CatDefaults::kRttyOffsetHz};
    bool pttEnabled{false}; QString pttDeviceSource{"None"}; QString pttSerialDevice;
    bool pttUseCts{false}; bool pttUseDsr{false}; int pttChannel{1};
    // From Thetis setup.cs:351-354 [v2.10.3.15].
    int pttSerialBaud{CatDefaults::kSerialBaud}; QString pttSerialParity{"None"}; int pttSerialDataBits{CatDefaults::kSerialDataBits}; QString pttSerialStopBits{"1"};
};
enum class CatWireDialect { Thetis, Rigctld };
enum class CatTransportKind { Tcp, Serial, Pty, Rigctld, Tester };
} // namespace NereusSDR
