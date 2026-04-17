// =================================================================
// src/core/HardwareProfile.h  (NereusSDR)
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

// src/core/HardwareProfile.h
//
// Maps an HPSDRModel enum to hardware overrides (ADC count, BPF style,
// supply voltage, audio swap, effective board identity).
//
// Source: mi0bot/Thetis clsHardwareSpecific.cs:85-184

#pragma once

#include "HpsdrModel.h"
#include "BoardCapabilities.h"
#include <QList>

namespace NereusSDR {

struct HardwareProfile {
    HPSDRModel               model{HPSDRModel::HERMES};
    HPSDRHW                  effectiveBoard{HPSDRHW::Hermes};
    const BoardCapabilities* caps{nullptr};
    int                      adcCount{1};
    bool                     mkiiBpf{false};
    int                      adcSupplyVoltage{33};
    bool                     lrAudioSwap{true};
};

// Compute a HardwareProfile for the given model.
// From Thetis clsHardwareSpecific.cs:85-184
HardwareProfile profileForModel(HPSDRModel model);

// Return the default (auto-guessed) HPSDRModel for a discovered board byte.
HPSDRModel defaultModelForBoard(HPSDRHW board);

// Return the list of HPSDRModel values compatible with a discovered board byte.
// From Thetis NetworkIO.cs:164-171
QList<HPSDRModel> compatibleModels(HPSDRHW board);

} // namespace NereusSDR
