// =================================================================
// src/core/codec/AlexFilterMap.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:6830-6942 (setAlexHPF)
//   Project Files/Source/Console/console.cs:7177-7243 (setAlexLPF) [v2.10.3.15]
//   Project Files/Source/ChannelMaster/netInterface.c:680-725 (SetAlexLPFBits) [v2.10.3.15]
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Lifted from P2RadioConnection::computeAlexHpf/Lpf
//                (which had ported the same console.cs logic) into a
//                shared header so P1RadioConnection can call it too.
//                Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-25: applyAlex1HpfSwitches: the Alex tab's high-pass
//                switches (HPF Bypass on TX, HPF Bypass on PureSignal
//                feedback, HPF Bypass, Disable 6m LNA on RX / TX) applied to the RX1 high-pass word as Thetis's
//                setAlexHPF / setBPF1ForOrionIISaturn apply them. Plan
//                Task 14 fix wave (R-R3-49). J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's setAlexLPF
//                does, written to the Alex0 / Alex1 masks as netInterface.c
//                SetAlexLPFBits writes them (radioHardwareVersion 10). J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The low-pass edges' ranges (setup.designer.cs spinner
//                Minimum / Maximum) and the neighbour rule (setup.cs
//                udAlex*LPF*_ValueChanged) shared by the tab and the Core.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// === Verbatim Thetis console.cs header (lines 1-50) ===
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
//
// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12
// --- From netInterface.c ---
/*
 * netinterface.c
 * Copyright (C) 2006,2007  Bill Tracey (bill@ejwt.com) (KD5TFD)
 * Copyright (C) 2010-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
// =================================================================

#include "AlexFilterMap.h"

#include <cmath>
#include <initializer_list>
#include <utility>

namespace NereusSDR::codec::alex {

// From Thetis console.cs:6830-6942 [@501e3f5]
// Upstream tags preserved: //N1GP (from cited console.cs:6830) [v2.10.3.15]
// Upstream inline attribution preserved verbatim:
//   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
// Decision rationale: spec §6.3.1
quint8 computeHpf(double freqMhz)
{
    // The ladder with the spinners' shipped edges (AlexHpfEdges::
    // thetisDefaults): below 1.8 MHz and above 61.44 MHz no row holds the
    // frequency, and Thetis sends the bypass.
    return selectAlexHpfRow(freqMhz, AlexHpfEdges::thetisDefaults().hpf);
}

// From Thetis console.cs:6953-7067 setBPF1ForOrionIISaturn [v2.10.3.15]
//
// The Orion MkII / Saturn boards replaced the high-pass ladder above with a
// band-pass bank wired to the SAME relay bits, so the byte values are
// unchanged and only the crossovers move.  Getting this wrong is silent: the
// radio still hears the band, it just does it behind the neighbouring filter,
// so adjacent-band energy that the hardware could have rejected reaches the
// front end instead.
//
// Thetis reads each edge from a user-editable Setup spinner via the BPF1_*
// getters at setup.cs:5193-5251 [v2.10.3.15]; the values below are those
// spinners' shipped defaults, decoded from setup.designer.cs [v2.10.3.15]:
//
//   ud1_5BPF1Start :24982 = 1.5        ud1_5BPF1End :25023 =  2.099999
//   ud6_5BPF1Start :25064 = 2.1        ud6_5BPF1End :25105 =  5.499999
//   ud9_5BPF1Start :25146 = 5.5        ud9_5BPF1End :25187 = 10.999999
//   ud13BPF1Start  :25440 = 11.0       ud13BPF1End  :25247 = 21.999999
//   ud20BPF1Start  :25277 = 22.0       ud20BPF1End  :25217 = 34.999999
//   ud6BPF1Start   :25481 = 35.0       ud6BPF1End   :25522 = 61.44
//
// Thetis writes each range as `freq >= Start && freq <= End`, and its End
// defaults sit one microhertz below the next Start purely so the Setup rows
// read as non-overlapping.  Collapsing that into a strict `<` chain (as the
// legacy ladder above already does) closes a sub-Hz window that would
// otherwise fall through to bypass.  deskhpsdr's independent implementation
// makes exactly the same call, and lands on exactly the same crossovers, at
// new_protocol.c:1314-1327 [@f3d857c]:
//     if      (BPFfreq <  1500000LL) alex0 |= ALEX_ANAN7000_RX_BYPASS_BPF;
//     else if (BPFfreq <  2100000LL) alex0 |= ALEX_ANAN7000_RX_160_BPF;
//     else if (BPFfreq <  5500000LL) alex0 |= ALEX_ANAN7000_RX_80_60_BPF;
//     else if (BPFfreq < 11000000LL) alex0 |= ALEX_ANAN7000_RX_40_30_BPF;
//     else if (BPFfreq < 22000000LL) alex0 |= ALEX_ANAN7000_RX_20_15_BPF;
//     else if (BPFfreq < 35000000LL) alex0 |= ALEX_ANAN7000_RX_12_10_BPF;
//     else                           alex0 |= ALEX_ANAN7000_RX_6_PRE_BPF;
//
// The one place the two upstreams differ is the top: Thetis stops the 6 m
// row at BPF1_6End (61.44 MHz) and bypasses above it, deskhpsdr has no upper
// bound.  Thetis is the port source, so the 61.44 ceiling is honoured here.
quint8 computeBpf1(double freqMhz)
{
    // The bank with the spinners' shipped edges (AlexHpfEdges::
    // thetisDefaults). Frequencies are whole hertz, so Thetis's
    // `>= Start && <= End` over the one-hertz gaps between rows gives the
    // crossovers above.
    return selectAlexHpfRow(freqMhz, AlexHpfEdges::thetisDefaults().bpf1);
}

// From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15], original C#:
//
//     private void setAlex1HPF(double freq)
//     {
//         if ((HardwareSpecific.Hardware == HPSDRHW.OrionMKII) || (HardwareSpecific.Hardware == HPSDRHW.Saturn)
//            || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
//         {
//             setBPF1ForOrionIISaturn(freq);
//         }
//         else
//         {
//             setAlexHPF(freq);
//         }
//     }
//
// Board coverage note (NereusSDR divergence, deliberate):
//   Thetis names OrionMKII / Saturn / HermesC10.  NereusSDR additionally
//   routes SaturnMKII here.  Thetis carries SaturnMKII as an enum slot only:
//   enums.cs:399 [v2.10.3.15] "SaturnMKII = 11,  // ANAN-G2: MKII board?" and
//   ChannelMaster/network.h:424 [v2.10.3.15] are its ONLY two occurrences in
//   the entire upstream tree, with no behaviour attached anywhere.  It is an
//   ANAN-G2 board revision (NereusSDR maps it to HPSDRModel::ANAN_G2 at
//   P2RadioConnection.h:825), so it physically carries the G2 band-pass bank;
//   routing it to the legacy high-pass ladder would reintroduce the very
//   defect this function exists to fix.  SettingsHygiene.cpp already
//   classified SaturnMKII as a BPF1 board before this function existed, so
//   this also keeps one answer to the question in the codebase.
bool usesBpf1Preselector(NereusSDR::HPSDRHW board) noexcept
{
    switch (board) {
        case NereusSDR::HPSDRHW::OrionMKII:   // ANAN-7000DLE / 8000DLE / AnvelinaPro3 / RedPitaya
        case NereusSDR::HPSDRHW::Saturn:      // ANAN-G2 / ANAN-G2-1K
        case NereusSDR::HPSDRHW::SaturnMKII:  // ANAN-G2 MkII board revision (see note above)
        case NereusSDR::HPSDRHW::HermesC10:   // ANAN-G2E  //N1GP G2E added (HermesC10) //DK1HLM
            return true;
        default:
            return false;
    }
}

// From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
quint8 computeRxPreselector(double freqMhz, NereusSDR::HPSDRHW board)
{
    return usesBpf1Preselector(board) ? computeBpf1(freqMhz)
                                      : computeHpf(freqMhz);
}

// ---------------------------------------------------------------------------
// The Alex tab's rows (see the declaration for the Thetis lines).
// ---------------------------------------------------------------------------

namespace {

// Each row's selection, in Thetis's row order.
// From Thetis console.cs:6857-6944 [v2.10.3.15] (setAlexHPF) and
// console.cs:6972-7059 (setBPF1ForOrionIISaturn), console.cs:7081-7168
// (setAlex2HPF): SetAlexHPFBits(0x10), (0x08), (0x04), (0x01), (0x02),
// (0x40) for the 1.5, 6.5, 9.5, 13, 20 MHz and 6 m BPF/LNA rows.
constexpr std::array<quint8, kAlexHpfRowCount> kRowBits = {
    0x10, 0x08, 0x04, 0x01, 0x02, 0x40,
};
constexpr quint8 kBypassBits = 0x20;

// Whole hertz. Thetis compares the spinners' decimal values exactly; this
// compares whole hertz, which agrees for every value the six-decimal spinners
// can hold. It differs only for a fractional-hertz frequency falling in the
// 1 Hz gap between two rows' edges (for example 2.5000005 MHz), which rounds
// into one of the neighbouring rows here where Thetis finds no row.
qint64 toHz(double mhz) noexcept
{
    return static_cast<qint64>(std::llround(mhz * 1.0e6));
}

AlexHpfRows rows(std::initializer_list<std::pair<double, double>> edges) noexcept
{
    AlexHpfRows out{};
    int i = 0;
    for (const auto& [start, end] : edges) {
        out[static_cast<size_t>(i)].startMhz = start;
        out[static_cast<size_t>(i)].endMhz = end;
        ++i;
    }
    return out;
}

} // namespace

// The spinners' shipped values, decoded from setup.designer.cs [v2.10.3.15]:
//   udAlex1_5HPFStart :23832 = 1.8   udAlex1_5HPFEnd :23873 =  6.499999
//   udAlex6_5HPFStart :23914 = 6.5   udAlex6_5HPFEnd :23955 =  9.499999
//   udAlex9_5HPFStart :23996 = 9.5   udAlex9_5HPFEnd :24037 = 12.999999
//   udAlex13HPFStart  :24291 = 13    udAlex13HPFEnd  :24097 = 19.999999
//   udAlex20HPFStart  :24127 = 20    udAlex20HPFEnd  :24067 = 49.999999
//   udAlex6BPFStart   :24343 = 50    udAlex6BPFEnd   :24384 = 61.44
//   ud1_5BPF1Start    :24982 = 1.5   ud1_5BPF1End    :25023 =  2.099999
//   ud6_5BPF1Start    :25064 = 2.1   ud6_5BPF1End    :25105 =  5.499999
//   ud9_5BPF1Start    :25146 = 5.5   ud9_5BPF1End    :25187 = 10.999999
//   ud13BPF1Start     :25440 = 11    ud13BPF1End     :25247 = 21.999999
//   ud20BPF1Start     :25277 = 22    ud20BPF1End     :25217 = 34.999999
//   ud6BPF1Start      :25481 = 35    ud6BPF1End      :25522 = 61.44
//   udAlex21_5HPFStart:26891 = 1.5   udAlex21_5HPFEnd:26861 =  2.099999
//   udAlex26_5HPFStart:26831 = 2.1   udAlex26_5HPFEnd:26801 =  5.499999
//   udAlex29_5HPFStart:26771 = 5.5   udAlex29_5HPFEnd:26741 = 10.999999
//   udAlex213HPFStart :26621 = 11    udAlex213HPFEnd :26681 = 21.999999
//   udAlex220HPFStart :26651 = 22    udAlex220HPFEnd :26711 = 34.999999
//   udAlex26BPFStart  :26483 = 35    udAlex26BPFEnd  :26513 = 61.44
// Every per-row bypass and the Alex-2 master default unchecked
// (console.cs:18808 alex2_hpf_bypass = false, 18823 alex1_5bphpf_bypass =
// false and the rest).
AlexHpfEdges AlexHpfEdges::thetisDefaults() noexcept
{
    AlexHpfEdges e;
    e.hpf = rows({{1.8, 6.499999}, {6.5, 9.499999}, {9.5, 12.999999},
                  {13.0, 19.999999}, {20.0, 49.999999}, {50.0, 61.44}});
    e.bpf1 = rows({{1.5, 2.099999}, {2.1, 5.499999}, {5.5, 10.999999},
                   {11.0, 21.999999}, {22.0, 34.999999}, {35.0, 61.44}});
    e.alex2 = e.bpf1;
    return e;
}

// From Thetis console.cs:6857-6870 [v2.10.3.15] (setAlexHPF, the first row;
// every row and the other two functions have the same shape)
//   if ((decimal)freq >= SetupForm.udAlex1_5HPFStart.Value && // 1.5 MHz HPF
//        (decimal)freq <= SetupForm.udAlex1_5HPFEnd.Value)
//   {
//       if (alex1_5bphpf_bypass)
//       {
//           NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
//           SetupForm.radBPHPFled.Checked = true;
//       }
//       else
//       {
//           NetworkIO.SetAlexHPFBits(0x10);
// and no row:
// From Thetis console.cs:6946-6950 [v2.10.3.15]
//   else
//   {
//       NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
quint8 selectAlexHpfRow(double freqMhz, const AlexHpfRows& rows) noexcept
{
    const qint64 hz = toHz(freqMhz);
    for (size_t i = 0; i < rows.size(); ++i) {
        const AlexHpfRow& row = rows[i];
        if (hz >= toHz(row.startMhz) && hz <= toHz(row.endMhz)) {
            return row.bypass ? kBypassBits : kRowBits[i];
        }
    }
    return kBypassBits;
}

quint8 computeRxPreselector(double freqMhz, NereusSDR::HPSDRHW board,
                            const AlexHpfEdges& edges) noexcept
{
    return selectAlexHpfRow(freqMhz, usesBpf1Preselector(board) ? edges.bpf1 : edges.hpf);
}

quint8 computeAlex2Hpf(double freqMhz, const AlexHpfEdges& edges) noexcept
{
    if (edges.alex2Bypass) {
        return kBypassBits;
    }
    return selectAlexHpfRow(freqMhz, edges.alex2);
}

// From Thetis setup.cs:6190-6205 [v2.10.3.15] (quoted in the header)
bool lpfBypassAvailable(NereusSDR::HPSDRModel model) noexcept
{
    // REDPITAYA keeps the box (//DH1KLM), as does every other board.
    switch (model) {
        case NereusSDR::HPSDRModel::ANAN8000D:
        case NereusSDR::HPSDRModel::ANAN7000D:
        case NereusSDR::HPSDRModel::ANAN_G2:
        case NereusSDR::HPSDRModel::ANAN_G2_1K:
        case NereusSDR::HPSDRModel::ANVELINAPRO3:
            return false;
        default:
            return true;
    }
}

bool usesAlex2Hpf(NereusSDR::HPSDRModel model) noexcept
{
    switch (model) {
        case NereusSDR::HPSDRModel::ORIONMKII:
        case NereusSDR::HPSDRModel::ANAN7000D:
        case NereusSDR::HPSDRModel::ANAN8000D:
        case NereusSDR::HPSDRModel::ANAN_G2:
        case NereusSDR::HPSDRModel::ANAN_G2_1K:
        case NereusSDR::HPSDRModel::ANVELINAPRO3:
        case NereusSDR::HPSDRModel::REDPITAYA:  //DH1KLM
            return true;
        default:
            return false;
    }
}

// From Thetis console.cs:7177-7243 [v2.10.3.15]
// Decision rationale: spec §6.3.1
//
// Board-independent by design. Thetis has a single setAlexLPF with no
// HardwareSpecific branch (console.cs:7177-7270 [v2.10.3.15]) and deskhpsdr
// agrees at alex.h:110 [@f3d857c]: "The TX bits are just as for the generic
// case."  The MkII boards changed the RX front end, not this bank, so there
// is no BPF1 equivalent to add here. The selection is setAlexLPF's over the
// rows' shipped values; the connections select over the saved rows.
quint8 computeLpf(double freqMhz)
{
    return selectAlexLpf(freqMhz, AlexLpfEdges::thetisDefaults());
}

// ---------------------------------------------------------------------------
// The Alex-1 Filters tab's low-pass rows (see the declaration).
// ---------------------------------------------------------------------------

// From Thetis setup.designer.cs [v2.10.3.15], the udAlex*LPFStart / End
// spinners' shipped values:
//   udAlex160mLPFStart 0 (:24881)        udAlex160mLPFEnd 2.5 (:24851)
//   udAlex80mLPFStart 2.500001 (:24821)  udAlex80mLPFEnd 5 (:24791)
//   udAlex40mLPFStart 5.000001 (:24761)  udAlex40mLPFEnd 8 (:24731)
//   udAlex20mLPFStart 8.000001 (:24611)  udAlex20mLPFEnd 16.5 (:24671)
//   udAlex15mLPFStart 16.500001 (:24641) udAlex15mLPFEnd 24.0 (:24701)
//   udAlex10mLPFStart 24.000001 (:24522) udAlex10mLPFEnd 35.6 (:24492)
//   udAlex6mLPFStart 35.600001 (:24581)  udAlex6mLPFEnd 61.44 (:24552)
AlexLpfEdges AlexLpfEdges::thetisDefaults() noexcept
{
    AlexLpfEdges e;
    e.rows = {{
        {0.0, 2.5},             // 160m
        {2.500001, 5.0},        // 80m
        {5.000001, 8.0},        // 40m
        {8.000001, 16.5},       // 20m
        {16.500001, 24.0},      // 15m
        {24.000001, 35.6},      // 10m
        {35.600001, 61.44},     // 6m
    }};
    return e;
}

namespace {

// setAlexLPF's test order and each test's selection, as row indices into
// AlexLpfRows (160m 0, 80m 1, 40m 2, 20m 3, 15m 4, 10m 5, 6m 6).
// From Thetis console.cs:7188-7234 [v2.10.3.15]
struct LpfTest {
    int row;
    quint8 bits;
};
constexpr std::array<LpfTest, kAlexLpfRowCount> kLpfTestOrder = {{
    {3, 0x01},  // 30/20m LPF
    {2, 0x02},  // 60/40m LPF
    {1, 0x04},  // 80m LPF
    {0, 0x08},  // 160m LPF
    {6, 0x10},  // 6m LPF
    {5, 0x20},  // 12/10m LPF
    {4, 0x40},  // 17/15 LPF
}};
constexpr quint8 kLpf6m = 0x10;

} // namespace

quint8 selectAlexLpf(double freqMhz, const AlexLpfEdges& edges) noexcept
{
    const qint64 hz = toHz(freqMhz);
    for (const LpfTest& t : kLpfTestOrder) {
        const AlexLpfRow& row = edges.rows[static_cast<size_t>(t.row)];
        if (hz >= toHz(row.startMhz) && hz <= toHz(row.endMhz)) {
            return t.bits;
        }
    }
    return kLpf6m;  // 6m LPF
}

// From Thetis ChannelMaster/netInterface.c:680-725 [v2.10.3.15]
// if not MOX, write to alex1 if a TX setting else write to alex0
bool setAlexLpfBits(AlexLpfMasks& masks, quint8 bits, bool isTx, bool isMox) noexcept
{
    bool alex0Changed = false;
    bool alex1Changed = false;

    if (isMox || isTx) {        // true if Alex1 should be written
        if (masks.alex1 != bits) {
            alex1Changed = true;
            masks.alex1 = bits;
        }
    }

    if (isMox || !isTx) {       // true if Alex0 should be written
        if (masks.alex0 != bits) {
            alex0Changed = true;
            masks.alex0 = bits;
        }
    }

    return alex1Changed || alex0Changed;
}

// From Thetis console.cs:7177-7243 [v2.10.3.15]
bool setAlexLpf(AlexLpfMasks& masks, double freqMhz, bool freqIsTx, bool mox,
                bool lpfBypass, bool alexPresent, const AlexLpfEdges& edges) noexcept
{
    if (!mox && lpfBypass) {
        return setAlexLpfBits(masks, kLpf6m, false, mox);  // 6m LPF
    }

    if (alexPresent) {  // Thetis: alexpresent && !initializing
        return setAlexLpfBits(masks, selectAlexLpf(freqMhz, edges), freqIsTx, mox);
    }
    return false;
}

// ---------------------------------------------------------------------------
// The low-pass edges' ranges and the neighbour rule (see the declarations).
// ---------------------------------------------------------------------------

namespace {

bool validLpfRow(int row) noexcept
{
    return row >= 0 && row < kAlexLpfRowCount;
}

double lpfEdgeMin(int row, bool isEnd) noexcept
{
    const AlexLpfEdgeLimits& l = kAlexLpfEdgeLimits[static_cast<size_t>(row)];
    return isEnd ? l.endMin : l.startMin;
}

double lpfEdgeMax(int row, bool isEnd) noexcept
{
    const AlexLpfEdgeLimits& l = kAlexLpfEdgeLimits[static_cast<size_t>(row)];
    return isEnd ? l.endMax : l.startMax;
}

double& lpfEdge(AlexLpfRows& rows, int row, bool isEnd) noexcept
{
    AlexLpfRow& r = rows[static_cast<size_t>(row)];
    return isEnd ? r.endMhz : r.startMhz;
}

double lpfEdge(const AlexLpfRows& rows, int row, bool isEnd) noexcept
{
    const AlexLpfRow& r = rows[static_cast<size_t>(row)];
    return isEnd ? r.endMhz : r.startMhz;
}

// Thetis's (decimal)0.000001 step, in whole micro-MHz (hertz).
constexpr qint64 kLpfNeighbourStepHz = 1;

} // namespace

bool alexLpfEdgeAllowed(int row, bool isEnd, double mhz) noexcept
{
    if (!validLpfRow(row) || !std::isfinite(mhz)) {
        return false;
    }
    const qint64 hz = toHz(mhz);
    return hz >= toHz(lpfEdgeMin(row, isEnd)) && hz <= toHz(lpfEdgeMax(row, isEnd));
}

double clampAlexLpfEdge(int row, bool isEnd, double mhz, double fallback) noexcept
{
    if (!validLpfRow(row)) {
        return mhz;
    }
    const double lo = lpfEdgeMin(row, isEnd);
    const double hi = lpfEdgeMax(row, isEnd);
    double v = mhz;
    if (!std::isfinite(v)) {
        v = std::isfinite(fallback) ? fallback : lo;
    }
    if (toHz(v) < toHz(lo)) {
        return lo;
    }
    if (toHz(v) > toHz(hi)) {
        return hi;
    }
    return v;
}

// From Thetis setup.cs:15888-15994 [v2.10.3.15] (the udAlex*LPFStart / End
// ValueChanged handlers, quoted at the declaration).
std::optional<AlexLpfEdgeMove> alexLpfNeighbourMove(const AlexLpfRows& rows,
                                                    int row, bool isEnd)
{
    if (!validLpfRow(row)) {
        return std::nullopt;
    }
    const qint64 value = toHz(lpfEdge(rows, row, isEnd));
    std::optional<AlexLpfEdgeMove> move;
    const auto target = [&](int r, bool end, qint64 hz) {
        move = AlexLpfEdgeMove{r, end, static_cast<double>(hz) / 1.0e6};
    };

    if (row == 0 && !isEnd) {
        // if (udAlex160mLPFStart.Value >= udAlex160mLPFEnd.Value + (decimal)0.000001)
        const qint64 end = toHz(rows[0].endMhz);
        if (value >= end + kLpfNeighbourStepHz) {
            target(0, true, value + kLpfNeighbourStepHz);
        }
    } else if (row == 0 && isEnd) {
        // if (udAlex160mLPFEnd.Value <= udAlex160mLPFStart.Value) ...
        // else if (udAlex160mLPFEnd.Value >= udAlex80mLPFStart.Value)
        if (value <= toHz(rows[0].startMhz)) {
            target(0, false, value - kLpfNeighbourStepHz);
        } else if (value >= toHz(rows[1].startMhz)) {
            target(1, false, value + kLpfNeighbourStepHz);
        }
    } else if (!isEnd) {
        // if (udAlex<N>LPFStart.Value <= udAlex<prev>LPFEnd.Value)
        if (value <= toHz(rows[static_cast<size_t>(row - 1)].endMhz)) {
            target(row - 1, true, value - kLpfNeighbourStepHz);
        }
    } else if (row < kAlexLpfRowCount - 1) {
        // if (udAlex<N>LPFEnd.Value >= udAlex<next>LPFStart.Value)
        if (value >= toHz(rows[static_cast<size_t>(row + 1)].startMhz)) {
            target(row + 1, false, value + kLpfNeighbourStepHz);
        }
    }
    // udAlex6mLPFEnd has no handler.

    if (!move) {
        return std::nullopt;
    }
    const double current = lpfEdge(rows, move->row, move->isEnd);
    move->mhz = clampAlexLpfEdge(move->row, move->isEnd, move->mhz, current);
    // Setting a spinner to the value it holds fires no ValueChanged.
    if (toHz(move->mhz) == toHz(current)) {
        return std::nullopt;
    }
    return move;
}

std::vector<AlexLpfEdgeMove> applyAlexLpfEdgeEdit(AlexLpfRows& rows, int row,
                                                  bool isEnd, double mhz)
{
    std::vector<AlexLpfEdgeMove> moved;
    if (!validLpfRow(row)) {
        return moved;
    }
    double& edited = lpfEdge(rows, row, isEnd);
    edited = clampAlexLpfEdge(row, isEnd, mhz, edited);

    // Each moved edge fires its own handler, as Thetis's spinners do. Every
    // step moves one edge one row outward and the ranges hold it, so the
    // chain is short; the bound only guards against a loop.
    constexpr int kMaxSteps = 32;
    int curRow = row;
    bool curEnd = isEnd;
    for (int step = 0; step < kMaxSteps; ++step) {
        const std::optional<AlexLpfEdgeMove> move = alexLpfNeighbourMove(rows, curRow, curEnd);
        if (!move) {
            break;
        }
        lpfEdge(rows, move->row, move->isEnd) = move->mhz;
        moved.push_back(*move);
        curRow = move->row;
        curEnd = move->isEnd;
    }
    return moved;
}

// ---------------------------------------------------------------------------
// receiveLpfFrequencyMhz: the receive-side counterpart of computeLpf's input.
//
// Straight translation of the body of Thetis's UpdateAlexTXFilter, minus the
// `if (!_mox)` wrapper, which the callers carry because they also have to
// decide which of the two masks the wire word takes.
//
// From Thetis console.cs:15487-15498 UpdateAlexTXFilter [v2.10.3.15]
//   if (!_rx2_preamp_present && chkRX2.Checked)
//   {
//       if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
//       else setAlexLPF(rx2_dds_freq_mhz, false);
//   }
//   else setAlexLPF(rx1_dds_freq_mhz, false);
// ---------------------------------------------------------------------------
double receiveLpfFrequencyMhz(double rx1Mhz, double rx2Mhz,
                              bool rx2Live, bool rx2PreampPresent) noexcept
{
    if (!rx2PreampPresent && rx2Live) {
        return (rx1Mhz > rx2Mhz) ? rx1Mhz : rx2Mhz;
    }
    return rx1Mhz;
}

// ---------------------------------------------------------------------------
// applyAlex1HpfSwitches: see the declaration for the Thetis lines.
//
// Thetis writes 0x20 in place of the band's selection (SetAlexHPFBits(0x20),
// which clears every other high-pass relay, netInterface.c:604-621
// [v2.10.3.15]); it does not add the bypass to the selection.
// ---------------------------------------------------------------------------
quint8 applyAlex1HpfSwitches(quint8 selected, NereusSDR::HPSDRHW board,
                             bool keyed, bool pureSignalRunning,
                             const Alex1HpfSwitches& switches) noexcept
{
    static constexpr quint8 kBypass = 0x20;
    const bool bandPassBoard = usesBpf1Preselector(board);
    if (keyed && (switches.hpfBypassOnTx
                  || (bandPassBoard && switches.hpfBypassOnPs && pureSignalRunning))) {
        return kBypass;
    }
    // From Thetis console.cs:6850-6855 [v2.10.3.15] (setAlexHPF), and the
    // same at 6965-6970 (setBPF1ForOrionIISaturn):
    //   if (alex_hpf_bypass)
    //   {
    //       NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
    if (switches.hpfBypass) {
        return kBypass;
    }
    // The 6 m branch, where the band's selection is the BPF/LNA (0x40):
    //   From Thetis console.cs:6931-6936 [v2.10.3.15] (setAlexHPF), and the
    //   same at 7046-7051 (setBPF1ForOrionIISaturn):
    //     else if ((decimal)freq >= SetupForm.udAlex6BPFStart.Value && // 6m BPF/LNA
    //              (decimal)freq <= SetupForm.udAlex6BPFEnd.Value)
    //     {
    //         if (alex6bphpf_bypass || disable_6m_lna_on_rx || (_mox && disable_6m_lna_on_tx))
    //         {
    //             NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
    // The per-row 6 m bypass (alex6bphpf_bypass) is the row's own, applied
    // where the row is selected (selectAlexHpfRow).
    static constexpr quint8 k6mBpfLna = 0x40;
    if (selected == k6mBpfLna
        && (switches.disable6mLnaOnRx || (keyed && switches.disable6mLnaOnTx))) {
        return kBypass;
    }
    return selected;
}

} // namespace NereusSDR::codec::alex
