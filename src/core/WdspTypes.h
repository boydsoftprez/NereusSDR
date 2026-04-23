// =================================================================
// src/core/WdspTypes.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
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

#pragma once

namespace NereusSDR {

// Demodulation mode. Values match WDSP's internal mode enum.
// From Thetis dsp.cs DSPMode
enum class DSPMode : int {
    LSB  = 0,
    USB  = 1,
    DSB  = 2,
    CWL  = 3,
    CWU  = 4,
    FM   = 5,
    AM   = 6,
    DIGU = 7,
    SPEC = 8,
    DIGL = 9,
    SAM  = 10,
    DRM  = 11
};

// AGC operating mode. Values match WDSP wcpAGC enum.
// From Thetis dsp.cs AGCMode
enum class AGCMode : int {
    Off    = 0,
    Long   = 1,
    Slow   = 2,
    Med    = 3,
    Fast   = 4,
    Custom = 5
};

// RX meter types. Values match WDSP GetRXAMeter 'mt' argument.
// From Thetis wdsp.cs rxaMeterType
enum class RxMeterType : int {
    SignalPeak   = 0,   // RXA_S_PK
    SignalAvg    = 1,   // RXA_S_AV
    AdcPeak      = 2,   // RXA_ADC_PK
    AdcAvg       = 3,   // RXA_ADC_AV
    AgcGain      = 4,   // RXA_AGC_GAIN
    AgcPeak      = 5,   // RXA_AGC_PK — MW0LGE [2.9.0.7] added pk + av + last [Thetis dsp.cs:881]
    AgcAvg       = 6    // RXA_AGC_AV — MW0LGE [2.9.0.7] added av [Thetis dsp.cs:882]
};

// TX meter types. Values match WDSP GetTXAMeter 'mt' argument.
// From Thetis wdsp.cs txaMeterType
enum class TxMeterType : int {
    MicPeak      = 0,
    MicAvg       = 1,
    EqPeak       = 2,
    EqAvg        = 3,
    CfcPeak      = 4,
    CfcAvg       = 5,   // CFC_AV — MW0LGE [2.9.0.7] added av [Thetis dsp.cs:883]
    CfcGain      = 6,
    CompPeak     = 7,
    CompAvg      = 8,
    AlcPeak      = 9,
    AlcAvg       = 10,
    AlcGain      = 11,
    OutPeak      = 12,
    OutAvg       = 13,
    LevelerPeak  = 14,
    LevelerAvg   = 15,
    LevelerGain  = 16
};

// WDSP channel type for OpenChannel 'type' parameter.
enum class ChannelType : int {
    RX = 0,
    TX = 1
};

// Noise-reduction mode for the NR/NR2 stage.
// Off = disabled, ANR = classic noise reduction, EMNR = enhanced NR2.
enum class NrMode      : int { Off = 0, ANR = 1, EMNR = 2 };

// Noise-blanker mode.
// From Thetis console.cs:43513-43560 [v2.10.3.13] — chkNB tri-state mapping:
//   CheckState.Unchecked    → Off (0)
//   CheckState.Checked      → NB  (1)  ≡ nob.c (Whitney blanker)
//   CheckState.Indeterminate→ NB2 (2)  ≡ nobII.c (second-gen blanker)
enum class NbMode : int { Off = 0, NB = 1, NB2 = 2 };

// From Thetis console.cs:43297-43450 [v2.10.3.13] — SelectNR() enforces
// at-most-one-NR-per-channel by setting all 4 RXANR*Run flags atomically.
// NereusSDR extends the set with 3 post-WDSP external filters (DFNR/BNR/MNR)
// that don't exist in Thetis.
enum class NrSlot : int {
    Off  = 0,
    NR1  = 1,   // WDSP anr.c     (Warren Pratt, NR0V)
    NR2  = 2,   // WDSP emnr.c    (Warren Pratt, NR0V)
    NR3  = 3,   // WDSP rnnr.c    (Samphire MW0LGE, rnnoise backend)
    NR4  = 4,   // WDSP sbnr.c    (Samphire MW0LGE, libspecbleach backend)
    DFNR = 5,   // AetherSDR DeepFilterFilter (DeepFilterNet3, post-WDSP)
    BNR  = 6,   // AetherSDR NvidiaBnrFilter (NVIDIA Broadcast, Windows+NVIDIA, post-WDSP)
    MNR  = 7    // AetherSDR MacNRFilter (Apple Accelerate, macOS, post-WDSP)
};

// From Thetis wdsp/anr.h:102 [v2.10.3.13] — SetRXAANRPosition(int channel, int position)
// Applies to NR1, NR2, NR3 equally (all three are WDSP stages with Position).
// NR4/DFNR/BNR/MNR have no Position control.
enum class NrPosition : int {
    PreAgc  = 0,
    PostAgc = 1
};

// From Thetis setup.cs:17354-17468 [v2.10.3.13] — EMNR Gain Method radio group.
enum class EmnrGainMethod : int {
    Linear  = 0,
    Log     = 1,
    Gamma   = 2,   // default per Thetis EMNR_DEFAULT_GAIN_METHOD
    Trained = 3
};

// From Thetis setup.cs:17374-17404 [v2.10.3.13] — EMNR NPE Method radio group.
enum class EmnrNpeMethod : int {
    Osms  = 0,   // default
    Mmse  = 1,
    Nstat = 2
};

// From Thetis setup.cs:34511-34527 [v2.10.3.13] — SBNR Algo 1/2/3 radio group.
// Calls SetRXASBNRnoiseScalingType(channel, 0/1/2).
enum class SbnrAlgo : int {
    Algo1 = 0,
    Algo2 = 1,   // default per Thetis screenshot (selected in shipped config)
    Algo3 = 2
};

// Squelch type active on a slice.
enum class SquelchMode : int { Off, Voice, AM, FM };

// AGC hang-time class — maps to Thetis custom hang bucket in setup.cs.
enum class AgcHangMode : int { Off, Fast, Med, Slow };

// FM transmit mode (repeater offset direction).
// Values match Thetis enums.cs:380 — FMTXMode (order is memory-form; take care before rearranging).
// From Thetis console.cs:20873 — current_fm_tx_mode = FMTXMode.Simplex
enum class FmTxMode : int { High = 0, Simplex = 1, Low = 2 };  // High = TX above RX (+), Simplex = no repeater offset (S), Low = TX below RX (-)

} // namespace NereusSDR

// Qt metatype registration for enum Q_PROPERTYs.
// Required so NrSlot / NrPosition / EmnrGainMethod / EmnrNpeMethod / SbnrAlgo
// can be used as Q_PROPERTY types without triggering "unable to find metatype"
// warnings at runtime. Mirror the pattern used by NbMode below.
#include <QMetaType>
Q_DECLARE_METATYPE(NereusSDR::NrMode)
Q_DECLARE_METATYPE(NereusSDR::NbMode)
Q_DECLARE_METATYPE(NereusSDR::NrSlot)
Q_DECLARE_METATYPE(NereusSDR::NrPosition)
Q_DECLARE_METATYPE(NereusSDR::EmnrGainMethod)
Q_DECLARE_METATYPE(NereusSDR::EmnrNpeMethod)
Q_DECLARE_METATYPE(NereusSDR::SbnrAlgo)
