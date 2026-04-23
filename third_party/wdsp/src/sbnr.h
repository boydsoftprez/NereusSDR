// =================================================================
// third_party/wdsp/src/sbnr.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/wdsp/sbnr.h @ v2.10.3.13 (commit 501e3f51)
//   Byte-for-byte import. No algorithmic changes. Original license
//   header (GPLv2+ + Samphire dual-licensing statement) preserved
//   verbatim below. Links against libspecbleach (LGPL-2.1) —
//   vendored separately in third_party/libspecbleach/.
// =================================================================

/*  snnr.h

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk

This code is based on code and ideas from  : https://github.com/vu3rdd/wdsp
and and uses RNNoise and libspecbleach
https://gitlab.xiph.org/xiph/rnnoise
https://github.com/lucianodato/libspecbleach
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

//
// =============================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Imported byte-for-byte from Thetis v2.10.3.13 @ 501e3f51.
//                No algorithmic changes. Linked against libspecbleach LGPL-2.1
//                (third_party/libspecbleach/) at WDSP build time.
//                Authored by J.J. Boyd (KG4VCF), ported with AI-assisted
//                review via Anthropic Claude Code. GPLv2+ upstream upgraded
//                to GPLv3 combined work under NereusSDR's GPLv3 umbrella
//                (per-file dual-license clause by MW0LGE unaffected).
// =============================================================================

#ifndef _sbnr_h
#define _sbnr_h

#include <specbleach_adenoiser.h>

typedef struct _sbnr
{
	int run;
    	int position;
        double *in;
        double *out;
        float reduction_amount;
        float smoothing_factor;
        float whitening_factor;
        int noise_scaling_type;
        float noise_rescale;
        float post_filter_threshold;
        SpectralBleachHandle st;
        int buffer_size;
        int rate;

        float* input;
        float* output;
} sbnr, *SBNR;

// define the public api of this module
extern SBNR create_sbnr(int run, int position, int size, double *in, double *out, int rate);
extern void destroy_sbnr(SBNR a);
extern void setSize_sbnr(SBNR a, int size);
extern void setBuffers_sbnr(SBNR a, double *in, double *out);
extern void xsbnr(SBNR a, int pos);
extern void setSamplerate_sbnr(SBNR a, int rate);

#endif // _sbnr_h
