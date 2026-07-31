// =================================================================
// src/core/codec/AlexFilterMap.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:6830-6942 (setAlexHPF)
//   Project Files/Source/Console/console.cs:7168-7234 (setAlexLPF)
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
// =================================================================

#include "AlexFilterMap.h"

namespace NereusSDR::codec::alex {

// From Thetis console.cs:6830-6942 [@501e3f5]
// Upstream tags preserved: //N1GP (from cited console.cs:6830) [v2.10.3.15]
// Upstream inline attribution preserved verbatim:
//   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
// Decision rationale: spec §6.3.1
quint8 computeHpf(double freqMhz)
{
    if (freqMhz < 1.5)  { return 0x20; }    // bypass
    if (freqMhz < 6.5)  { return 0x10; }    // 1.5 MHz HPF
    if (freqMhz < 9.5)  { return 0x08; }    // 6.5 MHz HPF
    if (freqMhz < 13.0) { return 0x04; }    // 9.5 MHz HPF
    if (freqMhz < 20.0) { return 0x01; }    // 13 MHz HPF
    if (freqMhz < 50.0) { return 0x02; }    // 20 MHz HPF
    return 0x40;                             // 6m preamp
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
    if (freqMhz < 1.5)    { return 0x20; }   // below the bank: bypass
    if (freqMhz < 2.1)    { return 0x10; }   // 160m BPF
    if (freqMhz < 5.5)    { return 0x08; }   // 80/60m BPF
    if (freqMhz < 11.0)   { return 0x04; }   // 40/30m BPF
    if (freqMhz < 22.0)   { return 0x01; }   // 20/17/15m BPF
    if (freqMhz < 35.0)   { return 0x02; }   // 12/10m BPF
    if (freqMhz <= 61.44) { return 0x40; }   // 6m BPF + LNA
    return 0x20;                              // above the bank: bypass
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

// From Thetis console.cs:7168-7234 [@501e3f5]
// Decision rationale: spec §6.3.1
//
// TX low-pass only, and board-independent by design. Thetis has a single
// setAlexLPF with no HardwareSpecific branch (console.cs:7177-7270
// [v2.10.3.15]) and deskhpsdr agrees at alex.h:110 [@f3d857c]: "The TX bits
// are just as for the generic case."  The MkII boards changed the RX front
// end, not this bank, so there is no BPF1 equivalent to add here.
quint8 computeLpf(double freqMhz)
{
    if (freqMhz < 2.0)   { return 0x08; }   // 160m LPF
    if (freqMhz < 4.0)   { return 0x04; }   // 80m LPF
    if (freqMhz < 7.3)   { return 0x02; }   // 60/40m LPF
    if (freqMhz < 14.35) { return 0x01; }   // 30/20m LPF
    if (freqMhz < 21.45) { return 0x40; }   // 17/15m LPF
    if (freqMhz < 29.7)  { return 0x20; }   // 12/10m LPF
    return 0x10;                             // 6m LPF
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

} // namespace NereusSDR::codec::alex
