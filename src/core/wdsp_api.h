// wdsp_api.h — Extern "C" declarations for WDSP functions used by NereusSDR.
//
// Signatures match WDSP v1.29 (TAPR/OpenHPSDR-wdsp) exactly.
// See CLAUDE.md "WDSP Calls — Extra Caution" before adding new declarations.
//
// INREAL = float, OUTREAL = float (from comm.h)

#pragma once

#ifdef HAVE_WDSP

extern "C" {

// ---------------------------------------------------------------------------
// Channel lifecycle (channel.h)
// ---------------------------------------------------------------------------

void OpenChannel(int channel, int in_size, int dsp_size,
                 int input_samplerate, int dsp_rate, int output_samplerate,
                 int type, int state,
                 double tdelayup, double tslewup,
                 double tdelaydown, double tslewdown, int bfo);

void CloseChannel(int channel);

int  SetChannelState(int channel, int state, int dmode);

void SetAllRates(int channel, int in_rate, int dsp_rate, int out_rate);

void SetInputBuffsize(int channel, int in_size);

void SetDSPBuffsize(int channel, int dsp_size);

void SetInputSamplerate(int channel, int samplerate);

void SetDSPSamplerate(int channel, int samplerate);

void SetOutputSamplerate(int channel, int samplerate);

// ---------------------------------------------------------------------------
// I/Q exchange (iobuffs.h) — INREAL=float, OUTREAL=float
// ---------------------------------------------------------------------------

void fexchange2(int channel, float* Iin, float* Qin,
                float* Iout, float* Qout, int* error);

// ---------------------------------------------------------------------------
// RX mode (RXA.h)
// ---------------------------------------------------------------------------

void SetRXAMode(int channel, int mode);

// ---------------------------------------------------------------------------
// Bandpass filter (bandpass.h / bandpass.c)
// ---------------------------------------------------------------------------

void SetRXABandpassFreqs(int channel, double f_low, double f_high);

// CRITICAL: SetRXABandpassFreqs only updates bp1, which only runs when
// AMD/SNBA/EMNR/ANF/ANR is enabled. For plain SSB the active filter is
// nbp0, controlled by RXANBPSetFreqs. Thetis always calls BOTH together
// (rxa.cs:110-111, radio.cs:603-604). Call both in setFilterFreqs.
void RXANBPSetFreqs(int channel, double flow, double fhigh);

void SetRXABandpassNC(int channel, int nc);

void SetRXABandpassMP(int channel, int mp);

void SetRXABandpassWindow(int channel, int wintype);

// ---------------------------------------------------------------------------
// Frequency shift (shift.h)
// ---------------------------------------------------------------------------

void SetRXAShiftRun(int channel, int run);

void SetRXAShiftFreq(int channel, double fshift);

// ---------------------------------------------------------------------------
// Notch bandpass shift (nbp.h) — From Thetis radio.cs:1418
// ---------------------------------------------------------------------------

void RXANBPSetShiftFrequency(int channel, double shift);

// ---------------------------------------------------------------------------
// Patch panel (patchpanel.h) — final mix stage in RXA pipeline
// ---------------------------------------------------------------------------

// bin=0 → copy=1 → dual mono (Q := I, same audio on both channels)
// bin=1 → copy=0 → binaural (I and Q separate, for headphone stereo image)
// From Thetis radio.cs:1157 — Thetis default BinOn=false → dual mono
void SetRXAPanelBinaural(int channel, int bin);

// ---------------------------------------------------------------------------
// AGC (wcpAGC.h)
// ---------------------------------------------------------------------------

void SetRXAAGCMode(int channel, int mode);

void SetRXAAGCFixed(int channel, double fixed_agc);

void SetRXAAGCAttack(int channel, int attack);

void SetRXAAGCDecay(int channel, int decay);

void SetRXAAGCHang(int channel, int hang);

void SetRXAAGCTop(int channel, double max_agc);

void GetRXAAGCTop(int channel, double* max_agc);

void SetRXAAGCSlope(int channel, int slope);

void SetRXAAGCThresh(int channel, double thresh, double size, double rate);

void GetRXAAGCThresh(int channel, double* thresh, double size, double rate);

void SetRXAAGCHangLevel(int channel, double hangLevel);

void SetRXAAGCHangThreshold(int channel, int hangthreshold);

// ---------------------------------------------------------------------------
// Noise reduction (anr.h, anf.h, emnr.c)
// ---------------------------------------------------------------------------

void SetRXAANRRun(int channel, int setit);

void SetRXAANRVals(int channel, int taps, int delay, double gain, double leakage);

void SetRXAANFRun(int channel, int setit);

void SetRXAEMNRRun(int channel, int run);

// ---------------------------------------------------------------------------
// Noise blanker — external (nob.h, nob.c)
// ---------------------------------------------------------------------------

void create_anbEXT(int id, int run, int buffsize, double samplerate,
                   double tau, double hangtime, double advtime,
                   double backtau, double threshold);

void destroy_anbEXT(int id);

void flush_anbEXT(int id);

void xanbEXT(int id, double* in, double* out);

// Float version (nob.c — not in header, but exported)
void xanbEXTF(int id, float* I, float* Q);

// ---------------------------------------------------------------------------
// Noise blanker II — external (nobII.h, nobII.c)
// ---------------------------------------------------------------------------

void create_nobEXT(int id, int run, int mode, int buffsize, double samplerate,
                   double slewtime, double hangtime, double advtime,
                   double backtau, double threshold);

void destroy_nobEXT(int id);

void flush_nobEXT(int id);

void xnobEXT(int id, double* in, double* out);

// Float version (nobII.c — not in header, but exported)
void xnobEXTF(int id, float* I, float* Q);

// ---------------------------------------------------------------------------
// Squelch (amsq.h)
// ---------------------------------------------------------------------------

void SetRXAAMSQRun(int channel, int run);

void SetRXAAMSQThreshold(int channel, double threshold);

void SetRXAAMSQMaxTail(int channel, double tail);

// ---------------------------------------------------------------------------
// Metering (meter.h)
// ---------------------------------------------------------------------------

double GetRXAMeter(int channel, int mt);

double GetTXAMeter(int channel, int mt);

// ---------------------------------------------------------------------------
// Wisdom + impulse cache (wisdom.c, impulse_cache.h)
// ---------------------------------------------------------------------------

int  WDSPwisdom(char* directory);

char* wisdom_get_status(void);

void init_impulse_cache(int use);

void destroy_impulse_cache(void);

int  save_impulse_cache(const char* path);

int  read_impulse_cache(const char* path);

void use_impulse_cache(int use);

// ---------------------------------------------------------------------------
// Version (version.c)
// ---------------------------------------------------------------------------

int GetWDSPVersion(void);

} // extern "C"

#endif // HAVE_WDSP
