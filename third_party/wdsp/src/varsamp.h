/*  varsamp.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017 Warren Pratt, NR0V

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

// NereusSDR modifications (2026-10-09, J.J. Boyd KG4VCF, with Anthropic Claude
// Code; against the pinned TAPR WDSP 2.10 tree at b02d5bac, whose varsamp.c and
// varsamp.h match Thetis v2.10.3.15): varsamp keeps a second, phase-major copy
// of its coefficients (ht) beside h, and hshift reads its taps from that copy.
// Upstream reads rsize taps R doubles apart (8 KB at R = 1024) for every output
// sample; with 4 KB pages that touches a different page for each tap, which made
// a 48 kHz rmatch stream cost about ten times the processor time on aarch64
// Linux it costs on macOS. The values read and the arithmetic are unchanged, so
// the output is bit-identical (R-AUD-15).

#ifndef _varsamp_h
#define _varsamp_h

typedef struct _varsamp
{
	int run;
	int size;
	double* in;
	double* out;
	int in_rate;
	int out_rate;
	double fcin;
	double fc;
	double fc_low;
	double gain;
	int idx_in;
	int ncoef;
	double* h;
	int rsize;
	double* ring;
	double var;
	int varmode;
	double cvar;
	double inv_cvar;
	double old_inv_cvar;
	double dicvar;
	double delta;
	double* hs;
	double* ht;		// NereusSDR: h by phase, (R + 1) rows of rsize taps; ht[p * rsize + m] = h[p + m * R]
	int R;
	double h_offset;
	double isamps;
	double nom_ratio;
} varsamp, *VARSAMP;

extern VARSAMP create_varsamp ( int run, int size, double* in, double* out,  
	int in_rate, int out_rate, double fc, double fc_low, int R, double gain, double var, int varmode);

extern void destroy_varsamp (VARSAMP a);

extern void flush_varsamp (VARSAMP a);

extern int xvarsamp (VARSAMP a, double var);

extern void setBuffers_varsamp (VARSAMP a, double* in, double* out);

extern void setSize_varsamp (VARSAMP a, int size);

extern void setInRate_varsamp (VARSAMP a, int rate);

extern void setOutRate_varsamp (VARSAMP a, int rate);

extern void setFCLow_varsamp (VARSAMP a, double fc_low);

extern void setBandwidth_varsamp (VARSAMP a, double fc_low, double fc_high);

#endif