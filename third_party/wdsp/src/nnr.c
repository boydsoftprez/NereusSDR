/*  nnr.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 Warren Pratt, NR0V

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

// NereusSDR modifications (2026-09-21, J.J. Boyd KG4VCF, with OpenAI Codex):
// accepted configuration/readback under the channel DSP lock; safe optional
// model handling; retain tuning on internal buffer/rate reconstruction.
// Source DSP algorithms and all upstream attribution are retained.
// NereusSDR modifications (2026-09-23, J.J. Boyd KG4VCF, with Anthropic
// Claude Code): runtime NNR limit (R-R3-40). RequestRXANNRLimit stores a
// request without the channel DSP lock; the worker applies it at its next
// block (xnnr), under the lock it already holds, by running the standard
// model only or no NNR. The caller's configuration and run request are kept
// and return when the limit is cleared. The status reports the applied limit
// and the model actually running. Later the same day: a request seen before
// its owner channel stays pending (nnr_sync_limit), and SetRXANNRModel
// requests its model through the limit instead of switching past it.

#include "comm.h"
#include "nnet.h"

#if NNET_NSLOTS != 2
#error Review the Nereus NNR status ABI when changing the number of model slots
#endif

#define NNR_PI			3.14159265358979323846
#define NNR_TWOPI		6.28318530717958647692
#define NNR_KAISER_BETA	7.857
#define NNR_PHASELEN	51

typedef struct _nnr
{
	int run;
	int requested_run;
	int position;
	int size;
	double* in_buff;
	double* out_buff;
	int rate;
	int nrate;
	int fftsize;
	int overlap;
	int hop;
	int nbins;
	int lookahead;
	double mask_floor;
	int cmode;

	int decim;
	int nphase;
	int nproto;
	double* proto;
	double* dline_dec;
	int dpos_dec;
	int dcount;
	double* dline_int;
	int dpos_int;

	double* inacc;
	int nacc;
	double* abuf;
	double* oacc;
	double* awin;
	double* swin;
	double olascale;

	double* tdin;
	double* tdout;
	double* fdout;
	double* fdin;
	fftw_plan pfor;
	fftw_plan prev;

	double* xre;
	double* xim;
	double* yre;
	double* yim;
	NNET net;
	NNET nets[NNET_NSLOTS];
	int  model;

	double* orbuf;
	int orsize;
	int orread;
	int orwrite;
	int orcount;
	int prime;
	int delay;

	int nunder;
	int nover;

	/* NereusSDR (R-R3-40): runtime limit. requested_model is the caller's
	 * accepted model; limit is the applied limit (0 none, 1 standard model
	 * only, 2 off), changed only under the channel DSP lock. limit_request
	 * and owner_channel are written without the lock by RequestRXANNRLimit. */
	int requested_model;
	int limit;
	volatile long limit_request;
	volatile long owner_channel;
} nnr, * NNR;

/* NereusSDR: these helpers expose values without leaking the opaque NNR or
 * NNET objects. Callers of exported functions own the channel lifetime. */
static int nnr_rate_supported (NNR a)
{
	return a->nrate > 0 && a->rate >= a->nrate && a->rate % a->nrate == 0;
}

static int nnr_channel_valid (int channel)
{
	return channel >= 0 && channel < MAX_CHANNELS && ch[channel].type == 0
		&& _InterlockedAnd (&ch[channel].run, 1) && rxa[channel].nnr.p != 0;
}

/* NereusSDR (R-R3-40): the model the limit lets run, and whether NNR runs. */
static int nnr_effective_slot (NNR a)
{
	return a->limit >= 1 ? 0 : a->requested_model;
}

static int nnr_effective_run (NNR a)
{
	return a->requested_run && a->limit < 2 && nnr_rate_supported (a)
		&& ok_nnet (a->net) && a->model == nnr_effective_slot (a);
}

static int nnr_copy_configuration (NNR a, NNRConfiguration* out)
{
	memset (out, 0, sizeof (*out));
	out->model_slot = a->requested_model;
	out->position = a->position;
	out->mask_floor_db = a->mask_floor;
	if (!ok_nnet (a->net)) return 0;
	out->alpha = getAlpha_nnet (a->net);
	out->alpha_knee_db = getKnee_nnet (a->net);
	out->tau_seconds = getTau_nnet (a->net);
	out->max_gain_db = getMaxGain_nnet (a->net);
	getSmooth_nnet (a->net, &out->attack_ms, &out->release_ms);
	return 1;
}

static void nnr_copy_status (NNR a, NNRRuntimeStatus* out)
{
	memset (out, 0, sizeof (*out));
	out->ready = nnr_copy_configuration (a, &out->configuration);
	out->running = a->run != 0;
	out->rate_supported = nnr_rate_supported (a);
	for (int k = 0; k < 2; ++k)
	{
		out->model_available[k] = ok_nnet (a->nets[k]);
		out->model_source[k] = getSource_nnet (a->nets[k]);
	}
	out->dsp_rate_hz = a->rate;
	out->network_rate_hz = a->nrate;
	out->delay_samples = a->delay;
	out->test_mode = a->net ? getMode_nnet (a->net) : 0;
	out->output_mode = a->cmode;
#ifdef NNET_PROFILE
	out->profiling_available = 1;
#endif
	out->active_model_slot = a->model;
	out->limit = a->limit;
	out->requested_run = a->requested_run != 0;
}

static int nnr_between (double value, double lo, double hi)
{
	/* Both comparisons must hold: NaN and infinities are rejected too. */
	return value >= lo && value <= hi;
}

static int nnr_configuration_valid (const NNRConfiguration* p)
{
	return p && p->model_slot >= 0 && p->model_slot < NNET_NSLOTS
		&& (p->position == 0 || p->position == 1)
		&& nnr_between (p->mask_floor_db, -50.0, -10.0)
		&& nnr_between (p->alpha, 0.0, 4.0)
		&& nnr_between (p->alpha_knee_db, 0.0, 40.0)
		&& nnr_between (p->tau_seconds, 0.05, 30.0)
		&& nnr_between (p->max_gain_db, 0.0, 24.0)
		&& nnr_between (p->attack_ms, 0.0, 500.0)
		&& nnr_between (p->release_ms, 0.0, 500.0);
}

static void nnr_apply_configuration (NNR a, const NNRConfiguration* p)
{
	int slot;
	int reset;
	a->requested_model = p->model_slot;
	slot = nnr_effective_slot (a);
	reset = a->model != slot || a->position != p->position;
	setModel_nnr (a, slot);
	a->position = p->position;
	a->mask_floor = p->mask_floor_db;
	for (int k = 0; k < NNET_NSLOTS; ++k)
	{
		NNET n = a->nets[k];
		if (!ok_nnet (n)) continue;
		setFloor_nnet (n, p->mask_floor_db);
		setAlpha_nnet (n, p->alpha);
		setKnee_nnet (n, p->alpha_knee_db);
		setTau_nnet (n, p->tau_seconds);
		setMaxGain_nnet (n, p->max_gain_db);
		setSmooth_nnet (n, p->attack_ms, p->release_ms);
	}
	a->run = nnr_effective_run (a);
	if (reset) flush_nnr (a);
}

/* NereusSDR (R-R3-40): apply a pending limit request. Caller holds the
 * channel DSP lock. Model switches and NNR on/off take the same steps the
 * locked setters take (setModel_nnr, flush_nnr, RXAbp1Check/RXAbp1Set). */
static void nnr_sync_limit (NNR a, int channel)
{
	int want = (int)_InterlockedAnd (&a->limit_request, ~0L);
	int slot, was_run;
	if (want == a->limit || want < 0 || want > 2) return;
	/* RequestRXANNRLimit's two stores are separate, so the worker can see a
	 * request before its owner channel. It stays pending until the owner is
	 * seen, so the bandpass update below is never skipped. */
	if (channel < 0 || channel >= MAX_CHANNELS) return;
	a->limit = want;
	was_run = a->run;
	slot = nnr_effective_slot (a);
	if (slot != a->model)
	{
		setModel_nnr (a, slot);
		flush_nnr (a);
	}
	a->run = nnr_effective_run (a);
	if (a->run != was_run)
	{
		flush_nnr (a);
		RXAbp1Check (channel);
		RXAbp1Set (channel);
	}
}

static double nnr_i0 (double x)
{
	double sum = 1.0, t = 1.0, xh = 0.5 * x, t2;
	int k;
	for (k = 1; k <= 60; k++)
	{
		t *= xh / (double)k;
		t2 = t * t;
		sum += t2;
		if (t2 < 1.0e-22 * sum) break;
	}
	return sum;
}

static void nnr_design_proto (NNR a)
{
	int n, M = a->nproto;
	double c = 0.5 * (double)(M - 1);
	double ic = 1.0 / nnr_i0 (NNR_KAISER_BETA);
	double fc = 0.45 * (double)a->nrate / (double)a->rate;
	double x, u, s, w, sum = 0.0;

	for (n = 0; n < M; n++)
	{
		x = (double)n - c;
		if (fabs (x) < 1.0e-12) s = 2.0 * fc;
		else                    s = sin (NNR_TWOPI * fc * x) / (NNR_PI * x);
		u = 2.0 * (double)n / (double)(M - 1) - 1.0;
		u = 1.0 - u * u;
		if (u < 0.0) u = 0.0;
		w = nnr_i0 (NNR_KAISER_BETA * sqrt (u)) * ic;
		a->proto[n] = s * w;
		sum += a->proto[n];
	}
	for (n = 0; n < M; n++) a->proto[n] /= sum;
}

static void nnr_design_windows (NNR a)
{
	int n;
	double h;
	for (n = 0; n < a->fftsize; n++)
	{
		h = 0.5 * (1.0 - cos (NNR_TWOPI * (double)n / (double)a->fftsize));
		a->awin[n] = sqrt (h);
		a->swin[n] = sqrt (h);
	}
	a->olascale = (2.0 / (double)a->overlap) / (double)a->fftsize;
}

static void nnr_fifo_put (NNR a, double v)
{
	if (a->orcount >= a->orsize)
	{
		a->orread = (a->orread + 1) % a->orsize;
		a->orcount--;
		a->nover++;
	}
	a->orbuf[a->orwrite] = v;
	a->orwrite = (a->orwrite + 1) % a->orsize;
	a->orcount++;
}

static double nnr_fifo_get (NNR a)
{
	double v;
	if (a->orcount <= 0)
	{
		a->nunder++;
		return 0.0;
	}
	v = a->orbuf[a->orread];
	a->orread = (a->orread + 1) % a->orsize;
	a->orcount--;
	return v;
}

static void nnr_push_output (NNR a, double y)
{
	int k, p, q;
	double acc;

	q = a->dpos_int;
	a->dline_int[q] = y;
	a->dline_int[q + a->nphase] = y;
	if (++a->dpos_int >= a->nphase) a->dpos_int = 0;

	for (p = 0; p < a->decim; p++)
	{
		acc = 0.0;
		for (k = 0; k < a->nphase; k++)
			acc += a->proto[k * a->decim + p] * a->dline_int[q + a->nphase - k];
		nnr_fifo_put (a, acc * (double)a->decim);
	}
}

static void nnr_frame (NNR a)
{
	int k;

	for (k = 0; k < a->fftsize; k++)
		a->tdin[k] = a->abuf[k] * a->awin[k];

	fftw_execute (a->pfor);

	for (k = 0; k < a->nbins; k++)
	{
		a->xre[k] = a->fdout[2 * k + 0];
		a->xim[k] = a->fdout[2 * k + 1];
	}

	run_nnet (a->net, a->xre, a->xim, a->yre, a->yim);

	for (k = 0; k < a->nbins; k++)
	{
		a->fdin[2 * k + 0] = a->yre[k];
		a->fdin[2 * k + 1] = a->yim[k];
	}
	a->fdin[1] = 0.0;
	a->fdin[2 * (a->nbins - 1) + 1] = 0.0;

	fftw_execute (a->prev);

	for (k = 0; k < a->fftsize; k++)
		a->oacc[k] += a->tdout[k] * a->swin[k] * a->olascale;

	for (k = 0; k < a->hop; k++)
		nnr_push_output (a, a->oacc[k]);

	memmove (a->oacc, a->oacc + a->hop, (a->fftsize - a->hop) * sizeof (double));
	memset  (a->oacc + a->fftsize - a->hop, 0, a->hop * sizeof (double));
}

static void nnr_push_input (NNR a, double x)
{
	int j, p;
	double acc;

	p = a->dpos_dec;
	a->dline_dec[p] = x;
	a->dline_dec[p + a->nproto] = x;
	if (++a->dpos_dec >= a->nproto) a->dpos_dec = 0;

	if (++a->dcount < a->decim) return;
	a->dcount = 0;

	acc = 0.0;
	for (j = 0; j < a->nproto; j++)
		acc += a->proto[j] * a->dline_dec[p + a->nproto - j];

	a->inacc[a->nacc++] = acc;
	if (a->nacc >= a->hop)
	{
		a->nacc = 0;
		memmove (a->abuf, a->abuf + a->hop, (a->fftsize - a->hop) * sizeof (double));
		memcpy  (a->abuf + a->fftsize - a->hop, a->inacc, a->hop * sizeof (double));
		nnr_frame (a);
	}
}

static void nnr_reset_state (NNR a)
{
	int i;

	memset (a->dline_dec, 0, 2 * a->nproto  * sizeof (double));
	memset (a->dline_int, 0, 2 * a->nphase  * sizeof (double));
	memset (a->inacc,     0,     a->hop     * sizeof (double));
	memset (a->abuf,      0,     a->fftsize * sizeof (double));
	memset (a->oacc,      0,     a->fftsize * sizeof (double));
	memset (a->orbuf,     0,     a->orsize  * sizeof (double));

	a->dpos_dec = 0;
	a->dcount   = 0;
	a->dpos_int = 0;
	a->nacc     = 0;
	a->orread   = 0;
	a->orwrite  = 0;
	a->orcount  = 0;
	a->nunder   = 0;
	a->nover    = 0;

	flush_nnet (a->net);

	for (i = 0; i < a->prime; i++) nnr_fifo_put (a, 0.0);
}

static void calc_nnr (NNR a)
{
	a->hop    = a->fftsize / a->overlap;
	a->nbins  = a->fftsize / 2 + 1;
	a->decim  = a->rate / a->nrate;

	if (a->decim < 1 || a->decim * a->nrate != a->rate)
	{
		dprintf ("nnr: dsp_rate %d is not an integer multiple of nrate %d - block disabled\n",
			a->rate, a->nrate);
		a->decim = 1;
		a->run = 0;
	}

	a->nphase = NNR_PHASELEN;
	a->nproto = a->nphase * a->decim;

	a->prime = a->decim * a->hop;

	a->delay = a->decim * (a->fftsize - a->hop + a->lookahead * a->hop)
			 + a->nproto - 1
			 + a->prime;

	a->orsize = a->prime + 2 * a->size + a->decim * (a->fftsize + a->hop) + 64;

	a->proto     = (double*) malloc0 (a->nproto      * sizeof (double));
	a->dline_dec = (double*) malloc0 (2 * a->nproto  * sizeof (double));
	a->dline_int = (double*) malloc0 (2 * a->nphase  * sizeof (double));
	a->inacc     = (double*) malloc0 (a->hop         * sizeof (double));
	a->abuf      = (double*) malloc0 (a->fftsize     * sizeof (double));
	a->oacc      = (double*) malloc0 (a->fftsize     * sizeof (double));
	a->awin      = (double*) malloc0 (a->fftsize     * sizeof (double));
	a->swin      = (double*) malloc0 (a->fftsize     * sizeof (double));
	a->tdin      = (double*) malloc0 (a->fftsize     * sizeof (double));
	a->tdout     = (double*) malloc0 (a->fftsize     * sizeof (double));
	a->fdout     = (double*) malloc0 (2 * a->nbins   * sizeof (double));
	a->fdin      = (double*) malloc0 (2 * a->nbins   * sizeof (double));
	a->xre       = (double*) malloc0 (a->nbins       * sizeof (double));
	a->xim       = (double*) malloc0 (a->nbins       * sizeof (double));
	a->yre       = (double*) malloc0 (a->nbins       * sizeof (double));
	a->yim       = (double*) malloc0 (a->nbins       * sizeof (double));
	a->orbuf     = (double*) malloc0 (a->orsize      * sizeof (double));

	nnr_design_proto   (a);
	nnr_design_windows (a);

	a->pfor = fftw_plan_dft_r2c_1d (a->fftsize, a->tdin, (fftw_complex*)a->fdout, FFTW_PATIENT);
	a->prev = fftw_plan_dft_c2r_1d (a->fftsize, (fftw_complex*)a->fdin, a->tdout, FFTW_PATIENT);

	{
		int k;
		for (k = 0; k < NNET_NSLOTS; k++) a->nets[k] = 0;

		a->nets[0] = create_nnet_slot (0, a->nbins, a->lookahead,
									   a->mask_floor);
		for (k = 1; k < NNET_NSLOTS; k++)
		{
			NNET m = create_nnet_slot (k, a->nbins, a->lookahead,
									   a->mask_floor);
			if (m == 0) continue;
			if (!ok_nnet (m) || getBins_nnet (m) != a->nbins)
			{
				dprintf ("nnr: model slot %d rejected - it wants %d bins, "
						 "the channel is built for %d\n",
						 k, getBins_nnet (m), a->nbins);
				destroy_nnet (m);
				continue;
			}
			a->nets[k] = m;
			dprintf ("nnr: model slot %d ready, %d frame(s) of lookahead\n",
					 k, getLookahead_nnet (m));
		}
		a->model = 0;
		a->net   = a->nets[0];
	}

	{
		int la = getLookahead_nnet (a->net);
		if (la != a->lookahead)
		{
			dprintf ("nnr: model uses %d frame(s) of lookahead, channel was "
					 "created for %d - using the model's\n", la, a->lookahead);
			a->lookahead = la;
		}
		a->delay = a->decim * (a->fftsize - a->hop + a->lookahead * a->hop)
				 + a->nproto - 1
				 + a->prime;
	}

	nnr_reset_state (a);
	a->run = nnr_effective_run (a);

	dprintf ("nnr: rate=%d nrate=%d decim=%d N=%d hop=%d bins=%d proto=%d prime=%d delay=%d (%.2f ms)\n",
		a->rate, a->nrate, a->decim, a->fftsize, a->hop, a->nbins, a->nproto,
		a->prime, a->delay, 1000.0 * (double)a->delay / (double)a->rate);
}

static void decalc_nnr (NNR a)
{
	{
		int k;
		for (k = 0; k < NNET_NSLOTS; k++)
			if (a->nets[k]) { destroy_nnet (a->nets[k]); a->nets[k] = 0; }
		a->net = 0;
	}

	fftw_destroy_plan (a->prev);
	fftw_destroy_plan (a->pfor);

	_aligned_free (a->orbuf);
	_aligned_free (a->yim);
	_aligned_free (a->yre);
	_aligned_free (a->xim);
	_aligned_free (a->xre);
	_aligned_free (a->fdin);
	_aligned_free (a->fdout);
	_aligned_free (a->tdout);
	_aligned_free (a->tdin);
	_aligned_free (a->swin);
	_aligned_free (a->awin);
	_aligned_free (a->oacc);
	_aligned_free (a->abuf);
	_aligned_free (a->inacc);
	_aligned_free (a->dline_int);
	_aligned_free (a->dline_dec);
	_aligned_free (a->proto);
}

NNR create_nnr
	(
	int run,
	int position,
	int size,
	double* in_buff,
	double* out_buff,
	int rate,
	int nrate,
	int fftsize,
	int overlap,
	int lookahead,
	double mask_floor,
	int cmode
	)
{
	NNR a = (NNR) malloc0 (sizeof (nnr));
	a->run        = run;
	a->requested_run = run != 0;
	a->position   = position;
	a->size       = size;
	a->in_buff    = in_buff;
	a->out_buff   = out_buff;
	a->rate       = rate;
	a->nrate      = nrate;
	a->fftsize    = fftsize;
	a->overlap    = overlap;
	a->lookahead  = lookahead;
	a->mask_floor = mask_floor;
	a->cmode      = cmode;
	a->owner_channel = -1;
	calc_nnr (a);
	return a;
}

void destroy_nnr (NNR a)
{
	decalc_nnr (a);
	_aligned_free (a);
}

void flush_nnr (NNR a)
{
	nnr_reset_state (a);
}

void xnnr (NNR a, int pos)
{
	/* NereusSDR (R-R3-40): one atomic read per call; a pending limit is
	 * applied here, by the worker, under the DSP lock it already holds. */
	if (_InterlockedAnd (&a->limit_request, ~0L) != a->limit)
		nnr_sync_limit (a, (int)_InterlockedAnd (&a->owner_channel, ~0L));
	if (a->run && a->position == pos)
	{
		int i;
		double y;

		for (i = 0; i < a->size; i++)
			nnr_push_input (a, a->in_buff[2 * i + 0]);

		for (i = 0; i < a->size; i++)
		{
			y = nnr_fifo_get (a);
			a->out_buff[2 * i + 0] = y;
			a->out_buff[2 * i + 1] = (a->cmode == 0) ? y : 0.0;
		}

		if (a->nunder || a->nover)
		{
			dprintf ("nnr: FIFO under=%d over=%d count=%d size=%d\n",
				a->nunder, a->nover, a->orcount, a->orsize);
			a->nunder = 0;
			a->nover  = 0;
		}
	}
	else if (a->in_buff != a->out_buff)
		memcpy (a->out_buff, a->in_buff, a->size * sizeof (complex));
}

void setBuffers_nnr (NNR a, double* in, double* out)
{
	a->in_buff  = in;
	a->out_buff = out;
}

void setSamplerate_nnr (NNR a, int rate)
{
	NNRConfiguration saved;
	int have_saved = nnr_copy_configuration (a, &saved);
	int test_mode = a->net ? getMode_nnet (a->net) : 0;
	decalc_nnr (a);
	a->rate = rate;
	calc_nnr (a);
	if (have_saved && ok_nnet (a->nets[saved.model_slot]))
		nnr_apply_configuration (a, &saved);
	else
		a->requested_model = a->model;
	for (int k = 0; k < NNET_NSLOTS; ++k)
		if (a->nets[k]) setMode_nnet (a->nets[k], test_mode);
}

void setSize_nnr (NNR a, int size)
{
	NNRConfiguration saved;
	int have_saved = nnr_copy_configuration (a, &saved);
	int test_mode = a->net ? getMode_nnet (a->net) : 0;
	decalc_nnr (a);
	a->size = size;
	calc_nnr (a);
	if (have_saved && ok_nnet (a->nets[saved.model_slot]))
		nnr_apply_configuration (a, &saved);
	else
		a->requested_model = a->model;
	for (int k = 0; k < NNET_NSLOTS; ++k)
		if (a->nets[k]) setMode_nnet (a->nets[k], test_mode);
}

#define NNR_ALL_MODELS(a, call)                                            \
	do {                                                                   \
		int _k;                                                            \
		for (_k = 0; _k < NNET_NSLOTS; _k++)                               \
			if (ok_nnet ((a)->nets[_k])) { NNET n = (a)->nets[_k]; call; } \
	} while (0)

int setModel_nnr (NNR a, int slot)
{
	if (slot < 0 || slot >= NNET_NSLOTS || !ok_nnet (a->nets[slot]))
	{
		dprintf ("nnr: model slot %d is not loaded - staying on slot %d\n",
				 slot, a->model);
		return a->model;
	}
	if (slot == a->model) return a->model;

	a->model = slot;
	a->net   = a->nets[slot];
	flush_nnet (a->net);

	a->lookahead = getLookahead_nnet (a->net);
	a->delay = a->decim * (a->fftsize - a->hop + a->lookahead * a->hop)
			 + a->nproto - 1
			 + a->prime;

	dprintf ("nnr: switched to model slot %d, lookahead %d, delay %d (%.2f ms)\n",
			 slot, a->lookahead, a->delay,
			 1000.0 * (double)a->delay / (double)a->rate);
	return a->model;
}

int getModel_nnr (NNR a)
{
	return a->model;
}

int getDelay_nnr (NNR a)
{
	return a->delay;
}

int getRun_nnr(NNR a)
{
	return a->run;
}


/********************************************************************************************************
*																										*
*											RXA Properties												*
*																										*
********************************************************************************************************/

PORT void
SetRXANNRRun (int channel, int setit)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	NNR a = rxa[channel].nnr.p;
	nnr_sync_limit (a, channel);
	a->requested_run = setit != 0;
	setit = nnr_effective_run (a);
	if (a->run != setit)
	{
		a->run = setit;
		flush_nnr (a);
	}
	RXAbp1Check (channel);
	RXAbp1Set (channel);
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRPosition (int channel, int position)
{
	if (!nnr_channel_valid (channel) || (position != 0 && position != 1)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	rxa[channel].nnr.p->position = position;
	flush_nnr (rxa[channel].nnr.p);
	RXAbp1Check (channel);
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRMaskFloor (int channel, double floor_db)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	rxa[channel].nnr.p->mask_floor = floor_db;
	NNR_ALL_MODELS (rxa[channel].nnr.p, setFloor_nnet (n, floor_db));
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT int
SetRXANNRModel (int channel, int slot)
{
	if (!nnr_channel_valid (channel)) return -1;
	int now;
	NNR a;
	EnterCriticalSection (&ch[channel].csDSP);
	a = rxa[channel].nnr.p;
	/* NereusSDR (R-R3-40): the slot is the caller's requested model, run
	 * through the runtime limit as ConfigureRXANNR does; a slot that is not
	 * loaded leaves the request unchanged. Returns the model running. */
	nnr_sync_limit (a, channel);
	if (slot >= 0 && slot < NNET_NSLOTS && ok_nnet (a->nets[slot]))
		a->requested_model = slot;
	now = setModel_nnr (a, nnr_effective_slot (a));
	LeaveCriticalSection (&ch[channel].csDSP);
	return now;
}

PORT int
GetRXANNRModel (int channel)
{
	if (!nnr_channel_valid (channel)) return -1;
	int now;
	EnterCriticalSection (&ch[channel].csDSP);
	now = getModel_nnr (rxa[channel].nnr.p);
	LeaveCriticalSection (&ch[channel].csDSP);
	return now;
}

PORT void
SetRXANNRcmode (int channel, int cmode)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	rxa[channel].nnr.p->cmode = cmode;
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRTestMode (int channel, int mode)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	NNR_ALL_MODELS (rxa[channel].nnr.p, setMode_nnet (n, mode));
	flush_nnr (rxa[channel].nnr.p);
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRAlpha (int channel, double alpha)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	NNR_ALL_MODELS (rxa[channel].nnr.p, setAlpha_nnet (n, alpha));
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRAlphaKnee (int channel, double knee_db)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	NNR_ALL_MODELS (rxa[channel].nnr.p, setKnee_nnet (n, knee_db));
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRTau (int channel, double tau)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	NNR_ALL_MODELS (rxa[channel].nnr.p, setTau_nnet (n, tau));
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRMaxGain (int channel, double gmax_db)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	NNR_ALL_MODELS (rxa[channel].nnr.p, setMaxGain_nnet (n, gmax_db));
	LeaveCriticalSection (&ch[channel].csDSP);
}

PORT void
SetRXANNRSmooth (int channel, double att_ms, double rel_ms)
{
	if (!nnr_channel_valid (channel)) return;
	EnterCriticalSection (&ch[channel].csDSP);
	NNR_ALL_MODELS (rxa[channel].nnr.p, setSmooth_nnet (n, att_ms, rel_ms));
	LeaveCriticalSection (&ch[channel].csDSP);
}


/* NereusSDR owning readback and atomic accepted-configuration API. */
PORT int GetRXANNRStatus (int channel, NNRRuntimeStatus* out)
{
    if (!out || !nnr_channel_valid (channel)) return 0;
    EnterCriticalSection (&ch[channel].csDSP);
    nnr_copy_status (rxa[channel].nnr.p, out);
    LeaveCriticalSection (&ch[channel].csDSP);
    return 1;
}

PORT int ConfigureRXANNR (int channel, const NNRConfiguration* requested,
                         NNRRuntimeStatus* accepted)
{
    if (!accepted || !nnr_configuration_valid (requested)
        || !nnr_channel_valid (channel)) return 0;
    EnterCriticalSection (&ch[channel].csDSP);
    NNR a = rxa[channel].nnr.p;
    nnr_sync_limit (a, channel);
    if (!ok_nnet (a->nets[requested->model_slot]))
    {
        nnr_copy_status (a, accepted);
        LeaveCriticalSection (&ch[channel].csDSP);
        return 0;
    }
    nnr_apply_configuration (a, requested);
    RXAbp1Check (channel);
    RXAbp1Set (channel);
    nnr_copy_status (a, accepted);
    LeaveCriticalSection (&ch[channel].csDSP);
    return 1;
}

PORT int SetRXANNRDiagnostics (int channel, int test_mode, int output_mode)
{
    if (!nnr_channel_valid (channel) || test_mode < 0 || test_mode > 2
        || output_mode < 0 || output_mode > 1) return 0;
    EnterCriticalSection (&ch[channel].csDSP);
    NNR a = rxa[channel].nnr.p;
    if (!ok_nnet (a->net))
    {
        LeaveCriticalSection (&ch[channel].csDSP);
        return 0;
    }
    NNR_ALL_MODELS (a, setMode_nnet (n, test_mode));
    a->cmode = output_mode;
    flush_nnr (a);
    LeaveCriticalSection (&ch[channel].csDSP);
    return 1;
}

/* NereusSDR (R-R3-40): request a runtime limit without the channel DSP lock.
 * 0 none, 1 standard model only, 2 off. The worker applies it at its next
 * block; a locked NNR setter applies a pending request first. */
PORT void RequestRXANNRLimit (int channel, int limit)
{
    NNR a;
    if (limit < 0 || limit > 2 || !nnr_channel_valid (channel)) return;
    a = rxa[channel].nnr.p;
    InterlockedExchange (&a->owner_channel, (long)channel);
    InterlockedExchange (&a->limit_request, (long)limit);
}
