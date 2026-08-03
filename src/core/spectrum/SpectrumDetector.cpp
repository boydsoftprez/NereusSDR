// =================================================================
// src/core/spectrum/SpectrumDetector.cpp  (NereusSDR)
// =================================================================
//
// Ported from WDSP source:
//   third_party/wdsp/src/analyzer.c:283-462 [v2.10.3.13]
//   (mirrors Thetis Project Files/Source/wdsp/analyzer.c at
//   v2.10.3.13).  Verbatim port of the detector() function.
//
// All inline comments from the upstream source are preserved per
// CLAUDE.md "Inline comment preservation" rule.  Algorithm,
// thresholds, and switch ordering are byte-for-byte equivalent —
// the only translation is C++ idiom (std::max, std::sqrt,
// std::floor, namespace) and float instead of double for our
// pipeline (precision differences negligible above the -200 dB
// floor that NereusSDR clips at upstream of this function).
//
// Original WDSP source license preserved verbatim in the header
// (SpectrumDetector.h).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-05 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-08-02  J.J. Boyd / KG4VCF  Relocated from src/gui/spectrum/ to
//                                    src/core/spectrum/ for the headless
//                                    daemon build split (R1).  No logic
//                                    change.  AI-assisted transformation via
//                                    Anthropic Claude Code.
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

#include "SpectrumDetector.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// From WDSP analyzer.c:283-462 [v2.10.3.13] — verbatim port.
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
                           double detOffset)
{
    if (m <= 0 || numPixels <= 0 || !bins || !pixels) {
        return;
    }

    const int detTypeInt = static_cast<int>(detType);

    int i;
    int imin;
    int ilim;
    int pix_count = 0;
    int rose;
    int fell;
    int next_pix_count;
    int bcount;
    int last_pix_count = 0;
    double prev_maxi;
    double mini;
    double maxi;
    double psum;

    if (pixPerBin <= 1.0)
    {
        if (fsclipL == std::floor(fsclipL)) imin = 0;
        else                                 imin = 1;
        if (fsclipH == std::floor(fsclipH)) ilim = m;
        else                                 ilim = m - 1;
        switch (detTypeInt)
        {

        case 0:     // positive peak
            for (i = 0; i < numPixels; i++)
                pixels[i] = -1.0e30f;

            for (i = imin; i < ilim; i++)
            {
                pix_count = static_cast<int>(detOffset + static_cast<double>(i) * pixPerBin);
                if (pix_count >= numPixels) pix_count = numPixels - 1;
                if (bins[i] > pixels[pix_count])
                    pixels[pix_count] = bins[i];
            }
            break;

        case 1:     // rosenfell
            rose         = 0;
            fell         = 0;
            mini         = +1.0e30;
            maxi         = -1.0e30;
            prev_maxi    = -1.0e30;

            for (i = imin; i < ilim; i++)       // for each FFT bin
            {
                // determine the pixel number that this FFT bin goes into
                pix_count = static_cast<int>(detOffset + static_cast<double>(i) * pixPerBin);
                if (pix_count >= numPixels) pix_count = numPixels - 1;
                // determine the pixel number for the NEXT FFT bin
                next_pix_count = static_cast<int>(static_cast<double>(i + 1) * pixPerBin);
                // update the minimum and maximum of the set of bins within the pixel
                if (bins[i] <   mini)     mini = bins[i];
                if (bins[i] >   maxi)     maxi = bins[i];
                // if the next bin is also within the pixel && there is a next bin,
                //    compare its value with the current bin and update rose and fell
                if (next_pix_count == pix_count && i < ilim - 1)
                {
                    // NOTE:  when next_pix_count != pix_count, rose and fell do not get updated;
                    //    that's OK because we do NOT need to know if there's a rise or fall across bins
                    if (bins[i + 1] > bins[i]) rose    = 1;
                    if (bins[i + 1] < bins[i]) fell    = 1;
                }
                // if the next bin is NOT within the pixel || there is no next bin, finalize the pixel
                //    value and reset parameters
                else
                {
                    if (rose && fell) {
                        if (pix_count & 1)              // odd pixel
                            pixels[pix_count] = static_cast<float>(std::max(prev_maxi, maxi));
                        else                            // even pixel
                            pixels[pix_count] = static_cast<float>(mini);
                    }
                    else {
                        pixels[pix_count] = static_cast<float>(maxi);
                    }
                    rose = 0;
                    fell = 0;
                    prev_maxi = maxi;
                    mini = +1.0e30;
                    maxi = -1.0e30;
                }
            }
            break;

        case 2:     // average - adjusted for window's equivalent noise bandwidth
            psum = 0.0;
            bcount = 0;
            for (i = imin; i < ilim; i++)
            {
                last_pix_count = pix_count;
                pix_count = static_cast<int>(detOffset + static_cast<double>(i) * pixPerBin);
                if (pix_count >= numPixels) pix_count = numPixels - 1;
                if (pix_count == last_pix_count)
                {
                    psum += bins[i];
                    bcount++;
                }
                else
                {
                    pixels[last_pix_count] = static_cast<float>(psum / static_cast<double>(bcount) * invEnb);
                    psum = bins[i];
                    bcount = 1;
                }
                if (i == ilim - 1)
                {
                    pixels[pix_count] = static_cast<float>(psum / static_cast<double>(bcount) * invEnb);
                }
            }
            break;

        case 3:     // sample - adjusted for window's equivalent noise bandwidth
            bcount = 0;
            for (i = imin; i < ilim; i++)
            {
                last_pix_count = pix_count;
                pix_count = static_cast<int>(detOffset + static_cast<double>(i) * pixPerBin);
                if (pix_count >= numPixels) pix_count = numPixels - 1;
                if (pix_count == last_pix_count)
                {
                    bcount++;
                }
                else
                {
                    pixels[last_pix_count] = static_cast<float>(bins[i - bcount / 2 - 1] * invEnb);
                    bcount = 1;
                }
                if (i == ilim - 1)
                {
                    pixels[pix_count] = static_cast<float>(bins[i - bcount / 2] * invEnb);
                }
            }
            break;

        case 4:     // rms
            psum = 0.0;
            bcount = 0;
            for (i = imin; i < ilim; i++)
            {
                last_pix_count = pix_count;
                pix_count = static_cast<int>(detOffset + static_cast<double>(i) * pixPerBin);
                if (pix_count >= numPixels) pix_count = numPixels - 1;
                if (pix_count == last_pix_count)
                {
                    psum += static_cast<double>(bins[i]) * static_cast<double>(bins[i]);
                    bcount++;
                }
                else
                {
                    pixels[last_pix_count] = static_cast<float>(std::sqrt(psum / static_cast<double>(bcount)) * invEnb);
                    psum = static_cast<double>(bins[i]) * static_cast<double>(bins[i]);
                    bcount = 1;
                }
                if (i == ilim - 1)
                {
                    pixels[pix_count] = static_cast<float>(std::sqrt(psum / static_cast<double>(bcount)) * invEnb);
                }
            }
            break;
        }
    }
    else
    {
        double frac;
        double pix_pos = fsclipL - std::floor(fsclipL);
        int ampl_comp = (detTypeInt == 2) || (detTypeInt == 3) || (detTypeInt == 4);
        for (i = 1; i < m; i++)
        {
            while (pix_pos < (static_cast<double>(i) + 1.0e-06) && pix_count < numPixels)
            {
                frac = pix_pos - static_cast<double>(i - 1);
                double v = static_cast<double>(bins[i - 1]) * (1.0 - frac)
                         + static_cast<double>(bins[i]) * frac;
                if (ampl_comp) v *= invEnb;
                pixels[pix_count] = static_cast<float>(v);
                pix_count++;
                pix_pos += binPerPix;
            }
        }
    }
}

void applySpectrumDetector(NereusSDR::SpectrumDetectorMode detType,
                           const QVector<float>& bins,
                           int numPixels,
                           QVector<float>& pixels,
                           double invEnb)
{
    const int m = bins.size();
    if (m <= 0 || numPixels <= 0) {
        pixels.clear();
        return;
    }
    if (pixels.size() != numPixels) {
        pixels.resize(numPixels);
    }
    const double pixPerBin = static_cast<double>(numPixels) / static_cast<double>(m);
    const double binPerPix = static_cast<double>(m) / static_cast<double>(numPixels);

    applySpectrumDetector(detType,
                          m,
                          numPixels,
                          pixPerBin,
                          binPerPix,
                          bins.constData(),
                          pixels.data(),
                          invEnb,
                          0.0,                       // fsclipL — no sub-band clip
                          static_cast<double>(m),    // fsclipH — full bin range
                          0.0);                      // detOffset — no offset
}

}  // namespace NereusSDR
