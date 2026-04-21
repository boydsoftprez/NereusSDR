// =================================================================
// tests/tst_alex_filter_map.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Exercises AlexFilterMap with expected HPF/LPF
//                 breakpoint values lifted from console.cs:6830-6942
//                 and 7168-7234. Authored in C++20/Qt6 for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
// =================================================================

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

#include <QtTest/QtTest>
#include "core/codec/AlexFilterMap.h"

using namespace NereusSDR::codec::alex;

class TestAlexFilterMap : public QObject {
    Q_OBJECT
private slots:
    // From Thetis console.cs:6830-6942 [@501e3f5]
    void hpfBypass_under_1_5MHz()       { QCOMPARE(computeHpf(1.0),  quint8(0x20)); }
    void hpf_1_5_to_6_5_MHz()           { QCOMPARE(computeHpf(3.5),  quint8(0x10)); }
    void hpf_6_5_to_9_5_MHz()           { QCOMPARE(computeHpf(7.0),  quint8(0x08)); }
    void hpf_9_5_to_13_MHz()            { QCOMPARE(computeHpf(10.0), quint8(0x04)); }
    void hpf_13_to_20_MHz()             { QCOMPARE(computeHpf(14.1), quint8(0x01)); }
    void hpf_20_to_50_MHz()             { QCOMPARE(computeHpf(28.0), quint8(0x02)); }
    void hpf_6m_preamp_50_MHz_and_up()  { QCOMPARE(computeHpf(50.0), quint8(0x40)); }

    // From Thetis console.cs:7168-7234 [@501e3f5]
    void lpf_160m_under_2MHz()  { QCOMPARE(computeLpf(1.9),   quint8(0x08)); }
    void lpf_80m_2_to_4_MHz()   { QCOMPARE(computeLpf(3.8),   quint8(0x04)); }
    void lpf_60_40m()           { QCOMPARE(computeLpf(7.1),   quint8(0x02)); }
    void lpf_30_20m()           { QCOMPARE(computeLpf(14.1),  quint8(0x01)); }
    void lpf_17_15m()           { QCOMPARE(computeLpf(21.0),  quint8(0x40)); }
    void lpf_12_10m()           { QCOMPARE(computeLpf(28.0),  quint8(0x20)); }
    void lpf_6m_29_7_and_up()   { QCOMPARE(computeLpf(50.0),  quint8(0x10)); }

    // Boundary edges — values exactly on the breakpoint go to the upper band
    void hpf_edge_1_5_MHz_exact()  { QCOMPARE(computeHpf(1.5),  quint8(0x10)); }
    void hpf_edge_50_MHz_exact()   { QCOMPARE(computeHpf(50.0), quint8(0x40)); }
    void lpf_edge_2_0_MHz_exact()  { QCOMPARE(computeLpf(2.0),  quint8(0x04)); }
    void lpf_edge_29_7_MHz_exact() { QCOMPARE(computeLpf(29.7), quint8(0x10)); }
};

QTEST_APPLESS_MAIN(TestAlexFilterMap)
#include "tst_alex_filter_map.moc"
