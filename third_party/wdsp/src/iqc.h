// =================================================================
// Historical PureSignal 2 provenance record retained by NereusSDR
// =================================================================
//
// The block below describes the retired iqc.h predecessor imported from
// Thetis v2.10.3.13 in May 2026.  On 2026-09-22 it was superseded by the
// pinned TAPR OpenHPSDR WDSP 2.10 baseline at
// b02d5bac675dd2f33ec2bab2b339f79a597c47dd.  The current implementation and
// its Nereus compatibility/cancellation changes begin after this historical
// record; the old provenance and notices remain here for attribution only.
// =================================================================

// =================================================================
// third_party/wdsp/src/iqc.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/wdsp/iqc.h @ v2.10.3.13 (commit 501e3f5)
//   Verbatim vendor alongside iqc.c (Phase 3M-4 Task 2).  No
//   NereusSDR-level edits.  Original NR0V GPLv2-or-later license
//   header preserved verbatim below.
// =================================================================

/*  iqc.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V

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

// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Phase 3M-4 Task 2: vendored verbatim from Thetis
//                v2.10.3.13 @501e3f51 by J.J. Boyd (KG4VCF), with
//                AI-assisted source-first protocol via Anthropic
//                Claude Code. No source-level modifications.
//                Cross-platform compatibility via existing
//                third_party/wdsp/src/linux_port.h shim.
//   2026-05-06 — Phase 3M-4 Task 3 attribution fix: NereusSDR-block
//                "Ported from Thetis source" preamble added above the
//                original NR0V license header, mirroring cfcomp.h.
// =================================================================

/*  iqc.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2026 Warren Pratt, NR0V

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

warren@pratt.one

*/

// NereusSDR modifications (2026-09-30 notice, J.J. Boyd KG4VCF, with Anthropic
// Claude Code; changes made between 2026-09-22 and 2026-09-30 against the
// pinned TAPR WDSP 2.10 tree at b02d5bac): adds the stopping field to the IQC
// struct; SetTXAiqcSwap and SetTXAiqcStart return int; and declares
// SetTXAiqcSwapChecked, SetTXAiqcStartChecked, SetTXAiqcStopping,
// RequestTXAiqcEnd, StopTXAiqcQuiescent, ApplyTXAiqcRetained and
// GetTXAiqcCorrectionAvailable.
// The "No NereusSDR-level edits" line in the historical record above
// describes the retired May 2026 Thetis vendor only, not this file.

#ifndef _iqc_h
#define _iqc_h
#include "nurbs_spline.h"
typedef struct _iqc
{
	NS_Spline   *m_spline[2], *c_spline[2], *s_spline[2];
	CurveEMA    m_calavg[2], c_calavg[2], s_calavg[2];
	double      m_prev_y[2], c_prev_y[2], s_prev_y[2];

	volatile long run;
	volatile long busy;
	volatile long stopping;
	int size;
	double* in;
	double* out;
	double rate;
	int cset;
	double tup;
	double* cup;
	int count;
	int ntup;
	int state;
	
} iqc, *IQC;

extern IQC create_iqc(int run, int size, double* in, double* out, double rate, double tup);

extern void destroy_iqc (IQC a);

extern void flush_iqc (IQC a);

extern void xiqc (IQC a);

extern void setBuffers_iqc (IQC a, double* in, double* out);

extern void setSamplerate_iqc (IQC a, int rate);

extern void setSize_iqc (IQC a, int size);

// TXA Properties

extern void GetTXAiqcValues(int channel, 
	NS_Spline** m_spline, CurveEMA* m_calavg, double* m_prev_y,
	NS_Spline** c_spline, CurveEMA* c_calavg, double* c_prev_y,
	NS_Spline** s_spline, CurveEMA* s_calavg, double* s_prev_y);

extern int SetTXAiqcSwap(int channel, 
	NS_Spline* m_spline, CurveEMA* m_calavg, double m_prev_y,
	NS_Spline* c_spline, CurveEMA* c_calavg, double c_prev_y,
	NS_Spline* s_spline, CurveEMA* s_calavg, double s_prev_y);

// Nereus session retirement: the cancellation flag is tested while csDSP is
// held, immediately before the IQC transition is installed.  A null flag
// preserves the upstream SetTXAiqcSwap behavior.
extern int SetTXAiqcSwapChecked(int channel,
	NS_Spline* m_spline, CurveEMA* m_calavg, double m_prev_y,
	NS_Spline* c_spline, CurveEMA* c_calavg, double c_prev_y,
	NS_Spline* s_spline, CurveEMA* s_calavg, double s_prev_y,
	volatile LONG* cancelled);

extern int SetTXAiqcStart(int channel, 
	NS_Spline* m_spline, CurveEMA* m_calavg, double m_prev_y,
	NS_Spline* c_spline, CurveEMA* c_calavg, double c_prev_y,
	NS_Spline* s_spline, CurveEMA* s_calavg, double s_prev_y);

extern int SetTXAiqcStartChecked(int channel,
	NS_Spline* m_spline, CurveEMA* m_calavg, double m_prev_y,
	NS_Spline* c_spline, CurveEMA* c_calavg, double c_prev_y,
	NS_Spline* s_spline, CurveEMA* s_calavg, double s_prev_y,
	volatile LONG* cancelled);

// no-port-check: Nereus CALCC installs under cs_update -> csDSP, then drops
// cs_update before waiting for audio. A return of 1 transfers all three
// spline pointers to IQC; cancellation after acceptance does not undo that
// ownership. The existing Checked APIs still wait synchronously.
extern int InstallTXAiqcStartChecked(int channel,
	NS_Spline* m_spline, CurveEMA* m_calavg, double m_prev_y,
	NS_Spline* c_spline, CurveEMA* c_calavg, double c_prev_y,
	NS_Spline* s_spline, CurveEMA* s_calavg, double s_prev_y,
	volatile LONG* cancelled);

extern int InstallTXAiqcSwapChecked(int channel,
	NS_Spline* m_spline, CurveEMA* m_calavg, double m_prev_y,
	NS_Spline* c_spline, CurveEMA* c_calavg, double c_prev_y,
	NS_Spline* s_spline, CurveEMA* s_calavg, double s_prev_y,
	volatile LONG* cancelled);

extern void SetTXAiqcEnd (int channel);

extern void SetTXAiqcStopping (int channel, int stopping);

extern int RequestTXAiqcEnd (int channel);

// Nereus quiescent correction control. The stop variant is used only after
// the host has stopped TXA processing; apply retains the upstream BEGIN ramp
// for the next processed block. Both calls are serialized by csDSP.
extern int StopTXAiqcQuiescent (int channel);

extern int ApplyTXAiqcRetained (int channel);

extern int GetTXAiqcCorrectionAvailable (int channel, int* available);

#endif
