// =================================================================
// tests/tst_rxchannel_agc_advanced.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

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

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelAgcAdvanced : public QObject {
    Q_OBJECT

private slots:
    // ── setAgcThreshold ──────────────────────────────────────────────────────

    void agcThresholdDefaultIsMinusTwenty() {
        // From Thetis console.cs:45977 — agc_thresh_point default = -20
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcThreshold(), -20);
    }

    void setAgcThresholdEarlyReturnOnSameValue() {
        // Same value as default — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcThreshold(-20);   // same as default → early return
        QCOMPARE(ch.agcThreshold(), -20);
    }

    // ── setAgcHang ───────────────────────────────────────────────────────────

    void agcHangDefaultIs250() {
        // From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcHang(), 250);
    }

    void setAgcHangEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcHang(250);   // same as default → early return
        QCOMPARE(ch.agcHang(), 250);
    }

    // ── setAgcSlope ──────────────────────────────────────────────────────────

    void agcSlopeDefaultIsZero() {
        // From Thetis radio.cs:1107-1108 — rx_agc_slope = 0
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcSlope(), 0);
    }

    void setAgcSlopeEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcSlope(0);   // same as default → early return
        QCOMPARE(ch.agcSlope(), 0);
    }

    // ── setAgcAttack ─────────────────────────────────────────────────────────

    void agcAttackDefaultIsTwo() {
        // From WDSP wcpAGC.c create_wcpagc — tau_attack default 2 ms
        // From Thetis dsp.cs:116-117 — P/Invoke declared, UI disabled/hidden
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcAttack(), 2);
    }

    void setAgcAttackEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcAttack(2);   // same as default → early return
        QCOMPARE(ch.agcAttack(), 2);
    }

    // ── setAgcDecay ──────────────────────────────────────────────────────────

    void agcDecayDefaultIs250() {
        // From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcDecay(), 250);
    }

    void setAgcDecayEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcDecay(250);   // same as default → early return
        QCOMPARE(ch.agcDecay(), 250);
    }
};

QTEST_MAIN(TestRxChannelAgcAdvanced)
#include "tst_rxchannel_agc_advanced.moc"
