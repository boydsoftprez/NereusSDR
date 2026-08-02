#pragma once

// =================================================================
// src/gui/spectrum/SpectrumDetector.h  (NereusSDR)
// =================================================================
//
// Ported from WDSP source:
//   third_party/wdsp/src/analyzer.c (mirrors Thetis Project Files/
//   Source/wdsp/analyzer.c at v2.10.3.13).  Verbatim port of the
//   detector() function (analyzer.c:283-462) which reduces N
//   linear-power FFT bins to M display pixels via the chosen
//   detector mode (Peak / Rosenfell / Average / Sample / RMS).
//
//   The WDSP analyzer performs detector reduction in linear-power
//   domain BEFORE averaging; Thetis avenger() then converts to dB
//   per av_mode (analyzer.c:464-554).  NereusSDR mirrors that
//   ordering — see SpectrumAvenger.h.
//
//   Original WDSP source license preserved verbatim below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-05 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 Verbatim port of WDSP detector() function at
//                 analyzer.c:283-462 [v2.10.3.13].  Both branches
//                 (pix_per_bin ≤ 1.0 — multi-bin per pixel —
//                 and pix_per_bin > 1.0 — multi-pixel per bin)
//                 included.  Type-narrowed from double to float
//                 to match NereusSDR's spectrum pipeline.
// =================================================================

//=================================================================
// analyzer.c
//=================================================================
//  This file is part of a program that implements a Spectrum Analyzer
//  used in conjunction with software-defined-radio hardware.
//
//  Copyright (C) 2012, 2013, 2014, 2016, 2023, 2025 Warren Pratt, NR0V
//  Copyright (C) 2012 David McQuate, WA8YWQ - Kaiser window & Bessel function added.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//  The author can be reached by email at
//
//  warren@wpratt.com
//=================================================================

#include "core/spectrum/SpectrumDetectorMode.h"

#include <QVector>

namespace NereusSDR {

/// Reduce N linear-power FFT bins to num_pixels display pixels using the
/// chosen detector mode.  Linear-power input — operate BEFORE averaging
/// per Thetis WDSP analyzer ordering (analyzer.c:283-462 [v2.10.3.13]).
///
/// Parameters mirror the original Thetis WDSP signature exactly:
///
///   detType        — SpectrumDetector enum (Peak/Rosenfell/Average/Sample/RMS)
///   m              — number of input bins (length of `bins`)
///   numPixels      — number of output pixels (length of `pixels`)
///   pixPerBin      — pixels per bin (= numPixels / m); the algorithm switches
///                    branches at 1.0
///   binPerPix      — bins per pixel (reciprocal of pixPerBin)
///   bins           — input array of m linear-power values (|X[k]|²)
///   pixels         — output array of numPixels values, written in place
///   invEnb         — inverse Equivalent Noise Bandwidth of the window
///                    (1/ENB); applied to Average/Sample/RMS only
///   fsclipL        — left fractional clip (Thetis sub-band-segment math)
///   fsclipH        — right fractional clip (same)
///   detOffset      — output-pixel offset added during pixel-index calc
///
/// For NereusSDR's typical call site: fsclipL=0, fsclipH=m, detOffset=0.
void applySpectrumDetector(NereusSDR::SpectrumDetectorMode detType,
                           int m,
                           int numPixels,
                           double pixPerBin,
                           double binPerPix,
                           const float* bins,
                           float* pixels,
                           double invEnb,
                           double fsclipL,
                           double fsclipH,
                           double detOffset);

/// QVector-friendly wrapper.  Resizes `pixels` to `numPixels` if needed.
/// Defaults fsclipL = 0, fsclipH = bins.size(), detOffset = 0 — the
/// NereusSDR call shape (no sub-band segmentation).
void applySpectrumDetector(NereusSDR::SpectrumDetectorMode detType,
                           const QVector<float>& bins,
                           int numPixels,
                           QVector<float>& pixels,
                           double invEnb);

}  // namespace NereusSDR
