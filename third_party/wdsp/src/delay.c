/*  delay.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013, 2019 Warren Pratt, NR0V

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

// NereusSDR modifications (2026-10-22, J.J. Boyd KG4VCF, with AI-assisted
// transformation via Claude Code; against the pinned TAPR WDSP 2.10 tree at
// b02d5bac, whose delay.c matches Thetis v2.10.3.15 wdsp/delay.c): the
// requested delay is bounded to what the ring can hold before the unchanged
// upstream arithmetic runs (longest_delay(), honourable_delay(), and their
// two call sites). Upstream WDSP and Thetis take any request: the PureSignal
// amp-delay field (Thetis PSForm.Designer.cs:394 udPSPhnum.Maximum
// 25000000 ns [v2.10.3.15]) allows 25 ms, nearly five times the ring at
// 192 kHz, and xdelay() then reads past the end of the ring's allocation.
// The bound is ported from Longpath (oe5sos/Longpath PR 25, commit
// 7b4450a1, unchanged at 383f058). Covered by tests/tst_wdsp_delay_clamp.cpp.
//
// Modification history (NereusSDR):
//   2026-10-22 Ported Longpath's bound on the requested delay by J.J. Boyd
//              (KG4VCF), with AI-assisted transformation via Claude Code.
//
// --- From third_party/wdsp/src/delay.c (Longpath) ---
// From Longpath third_party/wdsp/src/delay.c:31-50 [@383f058]
// =============================================================================
// Modification history (NereusSDR):
//   2026-09-20 — Bound on the requested delay (NereusSDR-original). The
//                ring holds WSDEL - 1 whole samples, but nothing limited
//                what a caller could ask for: the PureSignal amp-delay
//                field allows 25 ms, which at 192 kHz is nearly five times
//                the ring. Upstream turned that straight into a start
//                index past rsize, and xdelay() folds its read index back
//                only once, so the line read heap memory beyond its
//                allocation and the "actual delay" it reported was a lie.
//                The request is now limited to the longest delay the ring
//                can realise, in seconds, before the unchanged upstream
//                arithmetic runs; SetDelayValue() therefore returns the
//                delay that is really applied. Found by
//                tests/tst_wdsp_delay_clamp.cpp (against the old file three
//                of its four cases fail). Other WDSP descendants bound the
//                same request in their own way; this is NereusSDR's own
//                version, not a copy. Martin Fischer, AI-assisted via
//                Anthropic Claude. No other change to this file.
// =============================================================================

#include "comm.h"

// From Longpath third_party/wdsp/src/delay.c:52-69 [@383f058]
// The longest delay the ring can realise: WSDEL - 1 whole samples plus
// the last of the L sub-sample phases. adelta is one phase in seconds,
// so this is a number of phases turned back into time.
static double longest_delay (DELAY a)
{
	return a->adelta * (double)((WSDEL - 1) * a->L + (a->L - 1));
}

// A request the line can honour: never negative (a NaN is treated as
// "no delay"), never longer than the ring.
static double honourable_delay (DELAY a, double tdelay)
{
	double limit = longest_delay (a);
	if (!(tdelay > 0.0)) return 0.0;
	if (tdelay > limit) return limit;
	return tdelay;
}

DELAY create_delay (int run, int size, double* in, double* out, int rate, double tdelta, double tdelay)
{
	DELAY a = (DELAY) malloc0 (sizeof (delay));
	a->run = run;
	a->size = size;
	a->in = in;
	a->out = out;
	a->rate = rate;
	a->tdelta = tdelta;
	a->tdelay = tdelay;
	a->L = (int)(0.5 + 1.0 / (a->tdelta * (double)a->rate));
	a->adelta = 1.0 / (a->rate * a->L);
	// From Longpath third_party/wdsp/src/delay.c:82 [@383f058]
	a->tdelay = honourable_delay (a, a->tdelay);
	a->ft = 0.45 / (double)a->L;
	a->ncoef = (int)(60.0 / a->ft);
	a->ncoef = (a->ncoef / a->L + 1) * a->L;
	a->cpp = a->ncoef / a->L;
	a->phnum = (int)(0.5 + a->tdelay / a->adelta);
	a->snum = a->phnum / a->L;
	a->phnum %= a->L;
	a->idx_in = 0;
	a->adelay = a->adelta * (a->snum * a->L + a->phnum);
	a->h = fir_bandpass (a->ncoef,-a->ft, +a->ft, 1.0, 1, 0, (double)a->L);	
	a->rsize = a->cpp + (WSDEL - 1);
	a->ring = (double *) malloc0 (a->rsize * sizeof (complex));
	InitializeCriticalSectionAndSpinCount ( &a->cs_update, 2500 );
	return a;
}

void destroy_delay (DELAY a)
{
	DeleteCriticalSection (&a->cs_update);
	_aligned_free (a->ring);
	_aligned_free (a->h);
	_aligned_free (a);
}

void flush_delay (DELAY a)
{
	memset (a->ring, 0, a->cpp * sizeof (complex));
	a->idx_in = 0;
}

void xdelay (DELAY a)
{
	EnterCriticalSection (&a->cs_update);
	if (a->run)
	{
		int i, j, k, idx, n;
		double Itmp, Qtmp;
		for (i = 0; i < a->size; i++)
		{
			a->ring[2 * a->idx_in + 0] = a->in[2 * i + 0];
			a->ring[2 * a->idx_in + 1] = a->in[2 * i + 1];
			Itmp = 0.0;
			Qtmp = 0.0;
			if ((n = a->idx_in + a->snum) >= a->rsize) n -= a->rsize;
			for (j = 0, k = a->L - 1 - a->phnum; j < a->cpp; j++, k+= a->L)
			{
				if ((idx = n + j) >= a->rsize) idx -= a->rsize;
				Itmp += a->ring[2 * idx + 0] * a->h[k];
				Qtmp += a->ring[2 * idx + 1] * a->h[k];
			}
			a->out[2 * i + 0] = Itmp;
			a->out[2 * i + 1] = Qtmp;
			if (--a->idx_in < 0) a->idx_in = a->rsize - 1;
		}
	}
	else if (a->out != a->in)
		memcpy (a->out, a->in, a->size * sizeof (complex));
	LeaveCriticalSection (&a->cs_update);
}

/********************************************************************************************************
*																										*
*											  Properties												*
*																										*
********************************************************************************************************/

void SetDelayRun (DELAY a, int run)
{
	EnterCriticalSection (&a->cs_update);
	a->run = run;
	LeaveCriticalSection (&a->cs_update);
}

double SetDelayValue (DELAY a, double tdelay)
{
	double adelay;
	EnterCriticalSection (&a->cs_update);
	// From Longpath third_party/wdsp/src/delay.c:160 [@383f058]
	a->tdelay = honourable_delay (a, tdelay);
	a->phnum = (int)(0.5 + a->tdelay / a->adelta);
	a->snum = a->phnum / a->L;
	a->phnum %= a->L;
	a->adelay = a->adelta * (a->snum * a->L + a->phnum);
	adelay = a->adelay;
	LeaveCriticalSection (&a->cs_update);
	return adelay;
}

void SetDelayBuffs (DELAY a, int size, double* in, double* out)
{
	EnterCriticalSection (&a->cs_update);
	a->size = size;
	a->in = in;
	a->out = out;
	LeaveCriticalSection (&a->cs_update);
}
