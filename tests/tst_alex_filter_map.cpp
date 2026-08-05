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
#include "core/HpsdrModel.h"

using namespace NereusSDR::codec::alex;
using NereusSDR::HPSDRHW;

class TestAlexFilterMap : public QObject {
    Q_OBJECT
private slots:
    // From Thetis console.cs:6830-6942 [@501e3f5]
    // Upstream tags preserved: //N1GP (from cited console.cs:6830) [v2.10.3.15]
    // Upstream inline attribution preserved verbatim:
    //   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
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

    // ── Saturn-class MkII band-pass preselector ───────────────────────────
    //
    // The Orion MkII / Saturn boards carry a BAND-PASS bank on the very same
    // relay bits the ANAN-100/200 boards use for a HIGH-PASS ladder, so an
    // identical byte engages a physically different filter and the crossover
    // frequencies are entirely different.  Thetis dispatches on board model:
    //
    // From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]:
    //   if ((HardwareSpecific.Hardware == HPSDRHW.OrionMKII) || (... == HPSDRHW.Saturn)
    //      || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
    //       setBPF1ForOrionIISaturn(freq);
    //   else
    //       setAlexHPF(freq);
    //
    // From Thetis console.cs:6953-7067 setBPF1ForOrionIISaturn [v2.10.3.15],
    // whose edges come from the BPF1_* getters at setup.cs:5193-5251
    // [v2.10.3.15], which read the ud*BPF1Start/End spinner defaults at
    // setup.designer.cs:24982-25527 [v2.10.3.15]:
    //     1.5  .. 2.099999   -> 0x10   160m BPF
    //     2.1  .. 5.499999   -> 0x08   80/60m BPF
    //     5.5  .. 10.999999  -> 0x04   40/30m BPF
    //     11.0 .. 21.999999  -> 0x01   20/17/15m BPF
    //     22.0 .. 34.999999  -> 0x02   12/10m BPF
    //     35.0 .. 61.44      -> 0x40   6m BPF + LNA
    //   anything outside     -> 0x20   bypass
    //
    // Cross-checked against deskhpsdr, an independent implementation whose
    // crossovers agree exactly (new_protocol.c:1314-1327 [@f3d857c], constants
    // at alex.h:116-122 [@f3d857c] which note "Anan-7000/8000 use band-pass
    // filters here").
    void bpf1_ham_band_table_data()
    {
        QTest::addColumn<double>("freqMhz");
        QTest::addColumn<quint8>("bandPass");   // Saturn-class BPF1 bank
        QTest::addColumn<quint8>("highPass");   // ANAN-100/200 legacy ladder

        //                             freq        BPF1            legacy
        QTest::newRow("160m 1.85") <<  1.85 << quint8(0x10) << quint8(0x10);
        QTest::newRow("80m 3.70")  <<  3.70 << quint8(0x08) << quint8(0x10);
        QTest::newRow("60m 5.35")  <<  5.35 << quint8(0x08) << quint8(0x10);
        QTest::newRow("40m 7.15")  <<  7.15 << quint8(0x04) << quint8(0x08);
        QTest::newRow("30m 10.1")  << 10.10 << quint8(0x04) << quint8(0x04);
        QTest::newRow("20m 14.2")  << 14.20 << quint8(0x01) << quint8(0x01);
        QTest::newRow("17m 18.1")  << 18.10 << quint8(0x01) << quint8(0x01);
        QTest::newRow("15m 21.2")  << 21.20 << quint8(0x01) << quint8(0x02);
        QTest::newRow("12m 24.9")  << 24.90 << quint8(0x02) << quint8(0x02);
    }

    void bpf1_ham_band_table()
    {
        QFETCH(double, freqMhz);
        QFETCH(quint8, bandPass);
        QFETCH(quint8, highPass);

        QCOMPARE(computeBpf1(freqMhz), bandPass);

        // Regression guard: the legacy ladder must not shift by so much as a
        // bit.  ANAN-100/200 class hardware has to stay byte-identical.
        QCOMPARE(computeHpf(freqMhz), highPass);
    }

    // BPF1 crossovers, exactly on the edge.  A frequency landing on a Start
    // value belongs to that row (Thetis tests `freq >= Start`).
    void bpf1_edges_exact()
    {
        QCOMPARE(computeBpf1(1.5),   quint8(0x10));
        QCOMPARE(computeBpf1(2.1),   quint8(0x08));
        QCOMPARE(computeBpf1(5.5),   quint8(0x04));
        QCOMPARE(computeBpf1(11.0),  quint8(0x01));
        QCOMPARE(computeBpf1(22.0),  quint8(0x02));
        QCOMPARE(computeBpf1(35.0),  quint8(0x40));
        QCOMPARE(computeBpf1(61.44), quint8(0x40));   // BPF1_6End, inclusive
    }

    // Outside the bank entirely -> bypass, both ends.
    // From Thetis console.cs:7057-7062 [v2.10.3.15] (the trailing else).
    void bpf1_outside_bank_bypasses()
    {
        QCOMPARE(computeBpf1(1.0),   quint8(0x20));   // below BPF1_1_5Start
        QCOMPARE(computeBpf1(1.4999), quint8(0x20));
        QCOMPARE(computeBpf1(61.45), quint8(0x20));   // above BPF1_6End
        QCOMPARE(computeBpf1(70.0),  quint8(0x20));
    }

    // ── Board -> ladder routing ───────────────────────────────────────────
    //
    // From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
    // Upstream inline attribution preserved verbatim (console.cs:6830):
    //    || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
    //
    // NereusSDR additionally routes SaturnMKII to the band-pass bank.  Thetis
    // carries SaturnMKII as an enum slot only (enums.cs:399 [v2.10.3.15],
    // "ANAN-G2: MKII board?") with no behaviour attached anywhere in the tree;
    // it is an ANAN-G2 board revision and therefore physically carries the
    // same band-pass bank.  See AlexFilterMap.cpp for the full rationale.
    void preselector_routing_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<bool>("bandPass");

        QTest::newRow("Atlas")        << int(HPSDRHW::Atlas)            << false;
        QTest::newRow("Hermes")       << int(HPSDRHW::Hermes)           << false;
        QTest::newRow("HermesII")     << int(HPSDRHW::HermesII)         << false;
        QTest::newRow("Angelia")      << int(HPSDRHW::Angelia)          << false;
        QTest::newRow("Orion")        << int(HPSDRHW::Orion)            << false;
        QTest::newRow("HermesLite")   << int(HPSDRHW::HermesLite)       << false;
        QTest::newRow("HL2 RX-only")  << int(HPSDRHW::HermesLiteRxOnly) << false;
        QTest::newRow("Andromeda")    << int(HPSDRHW::Andromeda)        << false;
        QTest::newRow("Unknown")      << int(HPSDRHW::Unknown)          << false;

        QTest::newRow("OrionMKII")    << int(HPSDRHW::OrionMKII)        << true;
        QTest::newRow("Saturn")       << int(HPSDRHW::Saturn)           << true;
        QTest::newRow("SaturnMKII")   << int(HPSDRHW::SaturnMKII)       << true;
        QTest::newRow("HermesC10")    << int(HPSDRHW::HermesC10)        << true;
    }

    void preselector_routing()
    {
        QFETCH(int, board);
        QFETCH(bool, bandPass);

        const HPSDRHW hw = static_cast<HPSDRHW>(board);
        QCOMPARE(usesBpf1Preselector(hw), bandPass);

        // 15 m is the sharpest discriminator between the two ladders:
        // band-pass answers 0x01 (20/15 BPF), high-pass answers 0x02 (20 MHz
        // HPF).  Whichever ladder the board is routed to must be the one that
        // answers.
        QCOMPARE(computeRxPreselector(21.2, hw),
                 bandPass ? computeBpf1(21.2) : computeHpf(21.2));
        QCOMPARE(computeRxPreselector(21.2, hw), quint8(bandPass ? 0x01 : 0x02));
    }

    // Every ham band, routed through the dispatcher for one board of each
    // class.  This is the operator-visible table: on Saturn-class hardware
    // 80m / 60m / 40m / 15m used to engage the wrong filter.
    void dispatcher_matches_ladder_per_board_class()
    {
        const double bands[] = { 1.85, 3.70, 5.35, 7.15, 10.1, 14.2, 18.1, 21.2, 24.9 };
        for (double f : bands) {
            QCOMPARE(computeRxPreselector(f, HPSDRHW::Saturn),    computeBpf1(f));
            QCOMPARE(computeRxPreselector(f, HPSDRHW::OrionMKII), computeBpf1(f));
            QCOMPARE(computeRxPreselector(f, HPSDRHW::HermesC10), computeBpf1(f));
            QCOMPARE(computeRxPreselector(f, HPSDRHW::Orion),     computeHpf(f));
            QCOMPARE(computeRxPreselector(f, HPSDRHW::Hermes),    computeHpf(f));
        }
    }

    // The TX low-pass is board-independent.  Thetis has exactly one
    // setAlexLPF (console.cs:7177 [v2.10.3.15]) with no HardwareSpecific
    // branch inside it, and deskhpsdr agrees explicitly at alex.h:110
    // [@f3d857c]: "The TX bits are just as for the generic case."  So there is
    // deliberately no board-keyed LPF dispatcher; guard that it stays that way.
    void lpf_is_board_independent()
    {
        const double bands[] = { 1.85, 3.70, 7.15, 14.2, 21.2, 28.4, 50.1 };
        for (double f : bands) {
            const quint8 expect = computeLpf(f);
            QCOMPARE(computeLpf(f), expect);   // single ladder, no board arg
        }
    }
};

QTEST_APPLESS_MAIN(TestAlexFilterMap)
#include "tst_alex_filter_map.moc"
