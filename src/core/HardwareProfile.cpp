// =================================================================
// src/core/HardwareProfile.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsHardwareSpecific.cs
//   Project Files/Source/Console/HPSDR/NetworkIO.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// src/core/HardwareProfile.cpp
//
// Source: mi0bot/Thetis clsHardwareSpecific.cs:85-184, NetworkIO.cs:164-171

#include "HardwareProfile.h"

namespace NereusSDR {

// From Thetis clsHardwareSpecific.cs:85-184
HardwareProfile profileForModel(HPSDRModel model)
{
    HardwareProfile p;
    p.model = model;

    switch (model) {
        case HPSDRModel::HERMES:
            p.effectiveBoard    = HPSDRHW::Hermes;
            p.adcCount          = 1;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = true;
            break;
        case HPSDRModel::ANAN10:
            p.effectiveBoard    = HPSDRHW::Hermes;
            p.adcCount          = 1;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = true;
            break;
        case HPSDRModel::ANAN10E:
            p.effectiveBoard    = HPSDRHW::HermesII;
            p.adcCount          = 1;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = true;
            break;
        case HPSDRModel::ANAN100:
            p.effectiveBoard    = HPSDRHW::Hermes;
            p.adcCount          = 1;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = true;
            break;
        case HPSDRModel::ANAN100B:
            p.effectiveBoard    = HPSDRHW::HermesII;
            p.adcCount          = 1;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = true;
            break;
        case HPSDRModel::ANAN100D:
            p.effectiveBoard    = HPSDRHW::Angelia;
            p.adcCount          = 2;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::ANAN200D:
            p.effectiveBoard    = HPSDRHW::Orion;
            p.adcCount          = 2;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::ORIONMKII:
            p.effectiveBoard    = HPSDRHW::OrionMKII;
            p.adcCount          = 2;
            p.mkiiBpf           = true;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::ANAN7000D:
            p.effectiveBoard    = HPSDRHW::OrionMKII;
            p.adcCount          = 2;
            p.mkiiBpf           = true;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::ANAN8000D:
            p.effectiveBoard    = HPSDRHW::OrionMKII;
            p.adcCount          = 2;
            p.mkiiBpf           = true;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::ANAN_G2:
            p.effectiveBoard    = HPSDRHW::Saturn;
            p.adcCount          = 2;
            p.mkiiBpf           = true;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::ANAN_G2_1K:
            p.effectiveBoard    = HPSDRHW::Saturn;
            p.adcCount          = 2;
            p.mkiiBpf           = true;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::ANVELINAPRO3:
            p.effectiveBoard    = HPSDRHW::OrionMKII;
            p.adcCount          = 2;
            p.mkiiBpf           = true;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::HERMESLITE:
            p.effectiveBoard    = HPSDRHW::HermesLite;
            p.adcCount          = 1;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = true;
            break;
        case HPSDRModel::REDPITAYA:
            p.effectiveBoard    = HPSDRHW::OrionMKII;
            p.adcCount          = 2;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 50;
            p.lrAudioSwap       = false;
            break;
        case HPSDRModel::HPSDR:
            p.effectiveBoard    = HPSDRHW::Atlas;
            p.adcCount          = 1;
            p.mkiiBpf           = false;
            p.adcSupplyVoltage  = 33;
            p.lrAudioSwap       = true;
            break;
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:
            break;
    }

    p.caps = &BoardCapsTable::forBoard(p.effectiveBoard);
    return p;
}

HPSDRModel defaultModelForBoard(HPSDRHW board)
{
    // Walk HPSDRModel enum, return first match.
    for (int i = static_cast<int>(HPSDRModel::FIRST) + 1;
         i < static_cast<int>(HPSDRModel::LAST); ++i) {
        auto m = static_cast<HPSDRModel>(i);
        if (boardForModel(m) == board) {
            return m;
        }
    }

    // SaturnMKII (0x0B) has no dedicated HPSDRModel enum entry yet.
    // Map it to ANAN_G2 so it gets Saturn-family capabilities (2 ADC, P2).
    if (board == HPSDRHW::SaturnMKII) {
        return HPSDRModel::ANAN_G2;
    }

    return HPSDRModel::HERMES;
}

// From Thetis NetworkIO.cs:164-171
QList<HPSDRModel> compatibleModels(HPSDRHW board)
{
    QList<HPSDRModel> result;

    for (int i = static_cast<int>(HPSDRModel::FIRST) + 1;
         i < static_cast<int>(HPSDRModel::LAST); ++i) {
        auto m = static_cast<HPSDRModel>(i);

        // Primary match: model's native board matches discovered board.
        if (boardForModel(m) == board) {
            result.append(m);
            continue;
        }

        // Special cross-board compatibility cases.
        // From Thetis NetworkIO.cs:164-171
        switch (m) {
            case HPSDRModel::REDPITAYA:
                // RedPitaya is compatible with Hermes (0x01) or OrionMKII (0x05)
                if (board == HPSDRHW::Hermes || board == HPSDRHW::OrionMKII) {
                    result.append(m);
                }
                break;
            case HPSDRModel::ANAN10:
            case HPSDRModel::ANAN100:
                // ANAN-10/100 are compatible with Hermes or HermesII
                if (board == HPSDRHW::Hermes || board == HPSDRHW::HermesII) {
                    result.append(m);
                }
                break;
            case HPSDRModel::ANAN10E:
            case HPSDRModel::ANAN100B:
                // ANAN-10E/100B are compatible with Hermes or HermesII
                if (board == HPSDRHW::Hermes || board == HPSDRHW::HermesII) {
                    result.append(m);
                }
                break;
            case HPSDRModel::ANAN_G2:
            case HPSDRModel::ANAN_G2_1K:
                // Saturn-family models also match SaturnMKII board byte
                if (board == HPSDRHW::SaturnMKII) {
                    result.append(m);
                }
                break;
            default:
                break;
        }
    }

    return result;
}

} // namespace NereusSDR
