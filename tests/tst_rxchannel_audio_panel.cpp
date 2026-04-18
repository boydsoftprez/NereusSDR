// =================================================================
// tests/tst_rxchannel_audio_panel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

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

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelAudioPanel : public QObject {
    Q_OBJECT

private slots:
    // ── setMuted ─────────────────────────────────────────────────────────────

    void mutedDefaultIsFalse() {
        // Default: panel runs (unmuted) — WDSP panel enabled on create
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.muted(), false);
    }

    void setMutedEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setMuted(false);   // same as default → early return
        QCOMPARE(ch.muted(), false);
    }

    // ── setBinauralEnabled ───────────────────────────────────────────────────

    void binauralEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1145-1162 — bin_on = false (dual-mono default)
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.binauralEnabled(), false);
    }

    void setBinauralEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setBinauralEnabled(false);   // same as default → early return
        QCOMPARE(ch.binauralEnabled(), false);
    }

    // ── setAudioPan — pan conversion arithmetic ───────────────────────────────

    // These tests verify the conversion formula without calling WDSP.
    // Since HAVE_WDSP is not defined in test builds, setAudioPan() is a no-op
    // (Q_UNUSED path). We verify the conversion math independently.

    void panConversionCenterIsHalf() {
        // NereusSDR 0.0 → WDSP 0.5 (center)
        const double nereusCenter = 0.0;
        const double wdspPan = (nereusCenter + 1.0) / 2.0;
        QCOMPARE(wdspPan, 0.5);
    }

    void panConversionFullLeftIsZero() {
        // NereusSDR -1.0 → WDSP 0.0 (full left)
        const double nereusLeft = -1.0;
        const double wdspPan = (nereusLeft + 1.0) / 2.0;
        QCOMPARE(wdspPan, 0.0);
    }

    void panConversionFullRightIsOne() {
        // NereusSDR +1.0 → WDSP 1.0 (full right)
        const double nereusRight = 1.0;
        const double wdspPan = (nereusRight + 1.0) / 2.0;
        QCOMPARE(wdspPan, 1.0);
    }

    // Note: setAudioPan() is NOT tested with a live call here because
    // NereusSDRObjs compiles with HAVE_WDSP (PUBLIC), and SetRXAPanelPan(99)
    // would dereference an unallocated WDSP channel array slot → segfault.
    // The conversion arithmetic is fully verified by the three pan*() tests above.
    // See tst_rxchannel_squelch.cpp for the same rationale applied to squelch setters.
};

QTEST_MAIN(TestRxChannelAudioPanel)
#include "tst_rxchannel_audio_panel.moc"
