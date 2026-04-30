// =================================================================
// third_party/wdsp/src/cfcomp.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/wdsp/cfcomp.h @ v2.10.3.13 (commit 501e3f5)
//   Partial sync alongside cfcomp.c for the 3M-3a-ii CFC port
//   (Qg/Qe parametric-EQ tail-mix updates). Original license header
//   (GPLv2+) preserved verbatim below. cfcomp.h has no Samphire
//   contributions and therefore no dual-licensing block. See
//   docs/attribution/WDSP-PROVENANCE.md "Partial sync record" for
//   the line-level sync scope.
// =================================================================

/*  cfcomp.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2021 Warren Pratt, NR0V

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
// =============================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Partial sync from Thetis v2.10.3.13 @ 501e3f5 alongside
//                cfcomp.c for the Qg/Qe parametric-EQ tail-mix updates
//                required by the 3M-3a-ii CFC port. No NereusSDR-original
//                changes. Authored by J.J. Boyd (KG4VCF), ported with
//                AI-assisted review via Anthropic Claude Code. GPLv2+
//                upstream upgraded to GPLv3 combined work under
//                NereusSDR's GPLv3 umbrella.
// =============================================================================

#ifndef _cfcomp_h
#define _cfcomp_h

typedef struct _cfcomp
{
	int run;
	int position;
	int bsize;
	double* in;
	double* out;
	int fsize;
	int ovrlp;
	int incr;
	double* window;
	int iasize;
	double* inaccum;
	double* forfftin;
	double* forfftout;
	int msize;
	double* cmask;
	double* mask;
	int mask_ready;
	double* cfc_gain;
	double* revfftin;
	double* revfftout;
	double** save;
	int oasize;
	double* outaccum;
	double rate;
	int wintype;
	double pregain;
	double postgain;
	int nsamps;
	int iainidx;
	int iaoutidx;
	int init_oainidx;
	int oainidx;
	int oaoutidx;
	int saveidx;
	fftw_plan Rfor;
	fftw_plan Rrev;

	int comp_method;
	int nfreqs;
	double* F;
	double* G;
	double* E;
	double* Qg;
	double* Qe;
	double* fp;
	double* gp;
	double* ep;
	double* comp;
	double precomp;
	double precomplin;
	double* peq;
	int peq_run;
	double prepeq;
	double prepeqlin;
	double winfudge;

	double gain;
	double mtau;
	double mmult;
	// display stuff
	double dtau;
	double dmult;
	double* delta;
	double* delta_copy;
	double* cfc_gain_copy;
}cfcomp, *CFCOMP;

extern CFCOMP create_cfcomp (int run, int position, int peq_run, int size, double* in, double* out, int fsize, int ovrlp, 
	int rate, int wintype, int comp_method, int nfreqs, double precomp, double prepeq, double* F, double* G, double* E, double mtau, double dtau);

extern void destroy_cfcomp (CFCOMP a);

extern void flush_cfcomp (CFCOMP a);

extern void xcfcomp (CFCOMP a, int pos);

extern void setBuffers_cfcomp (CFCOMP a, double* in, double* out);

extern void setSamplerate_cfcomp (CFCOMP a, int rate);

extern void setSize_cfcomp (CFCOMP a, int size);

#endif