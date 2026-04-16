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

// Enable/disable the audio panel (mute when run=0, unmute when run=1).
// From Thetis Project Files/Source/Console/dsp.cs:393-394 — P/Invoke decl
// WDSP: third_party/wdsp/src/patchpanel.c:126
void SetRXAPanelRun(int channel, int run);

// Set stereo pan position. pan=0.0 → full left, pan=0.5 → center, pan=1.0 → full right.
// WDSP applies sin-law panning: adjusts gain2I/gain2Q via sin(pan*PI).
// NereusSDR uses -1..+1 range; convert with wdsp_pan = (nereus_pan + 1.0) / 2.0.
// From Thetis Project Files/Source/Console/radio.cs:1386-1403
//   pan default = 0.5f (center), dsp.cs:402-403 P/Invoke decl
// WDSP: third_party/wdsp/src/patchpanel.c:159
void SetRXAPanelPan(int channel, double pan);

// bin=0 → copy=1 → dual mono (Q := I, same audio on both channels)
// bin=1 → copy=0 → binaural (I and Q separate, for headphone stereo image)
// From Thetis radio.cs:1157 — Thetis default BinOn=false → dual mono
// WDSP: third_party/wdsp/src/patchpanel.c:187
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

// EMNR sub-parameters — From Thetis dsp.cs:243-297 P/Invoke declarations
// WDSP: third_party/wdsp/src/emnr.c:1298,1306,1314,1322
void SetRXAEMNRgainMethod(int channel, int method);
void SetRXAEMNRnpeMethod(int channel, int method);
void SetRXAEMNRaeRun(int channel, int run);
void SetRXAEMNRPosition(int channel, int position);

// ---------------------------------------------------------------------------
// Spectral noise blanker (snb.h) — From Thetis dsp.cs P/Invoke declarations
// ---------------------------------------------------------------------------

// WDSP: third_party/wdsp/src/snb.c:579
void SetRXASNBARun(int channel, int run);

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

// NB2 advanced sub-parameters — called after create_nobEXT to override defaults.
// Declared in Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:922-937
// WDSP: third_party/wdsp/src/nobII.c:658,686,697,707,727

// mode: 0=zero, 1=sample-hold, 2=mean-hold, 3=hold-sample, 4=interpolate
// From Thetis specHPSDR.cs:937 — SetEXTNOBMode(int id, int mode)
// WDSP: third_party/wdsp/src/nobII.c:658
void SetEXTNOBMode(int id, int mode);

// tau: slew time constant in seconds (sets both advslewtime and hangslewtime)
// From Thetis specHPSDR.cs:922 — SetEXTNOBTau(int id, double tau)
// WDSP: third_party/wdsp/src/nobII.c:686
void SetEXTNOBTau(int id, double tau);

// hangtime: hang time in seconds after blanked impulse
// From Thetis specHPSDR.cs:925 — SetEXTNOBHangtime(int id, double time)
// WDSP: third_party/wdsp/src/nobII.c:697
void SetEXTNOBHangtime(int id, double time);

// advtime: advance/lead time in seconds before detected impulse
// From Thetis specHPSDR.cs:928 — SetEXTNOBAdvtime(int id, double time)
// WDSP: third_party/wdsp/src/nobII.c:707
void SetEXTNOBAdvtime(int id, double time);

// threshold: detection threshold (dimensionless ratio; create default 30.0)
// From Thetis specHPSDR.cs:934 — SetEXTNOBThreshold(int id, double thresh)
// WDSP: third_party/wdsp/src/nobII.c:727
void SetEXTNOBThreshold(int id, double thresh);

// ---------------------------------------------------------------------------
// APF — Audio Peak Filter (apfshadow.c / apfshadow.h)
//
// The APF shadow coordinates 4 underlying filter types via a single facade.
// From Thetis Project Files/Source/Console/radio.cs:1910-2008
// WDSP: third_party/wdsp/src/apfshadow.c:45,93,117,141,165
// ---------------------------------------------------------------------------

// Select filter type: 0=double-pole, 1=matched, 2=gaussian, 3=bi-quad
// From Thetis radio.cs:1986 — _rx_apf_type = 3 (bi-quad)
void SetRXASPCWSelection(int channel, int selection);

// Enable/disable APF processing
// From Thetis radio.cs:1910 — rx_apf_run default = false
void SetRXASPCWRun(int channel, int run);

// Set center frequency in Hz
// From Thetis radio.cs:1929 — rx_apf_freq default = 600.0 Hz
void SetRXASPCWFreq(int channel, double f_center);

// Set bandwidth in Hz
// From Thetis radio.cs:1948 — rx_apf_bw default = 600.0 Hz
void SetRXASPCWBandwidth(int channel, double bandwidth);

// Set gain (linear scale)
// From Thetis radio.cs:1967 — rx_apf_gain default = 1.0
void SetRXASPCWGain(int channel, double gain);

// ---------------------------------------------------------------------------
// Squelch — SSB (ssql.c / ssql.h)
//
// From Thetis Project Files/Source/Console/radio.cs:1185-1229
// WDSP: third_party/wdsp/src/ssql.c:331,339
// threshold: 0.0..1.0 linear (Thetis default _fSSqlThreshold = 0.16f)
// ---------------------------------------------------------------------------

void SetRXASSQLRun(int channel, int run);

void SetRXASSQLThreshold(int channel, double threshold);

// ---------------------------------------------------------------------------
// Squelch — AM (amsq.h)
//
// From Thetis Project Files/Source/Console/radio.cs:1164-1178, 1293-1310
// WDSP: third_party/wdsp/src/amsq.c — threshold in dB (pow(10,t/20) applied internally)
// threshold: rx_squelch_threshold default = -150.0 dB
// ---------------------------------------------------------------------------

void SetRXAAMSQRun(int channel, int run);

void SetRXAAMSQThreshold(int channel, double threshold);

void SetRXAAMSQMaxTail(int channel, double tail);

// ---------------------------------------------------------------------------
// Squelch — FM (fmsq.c / fmsq.h)
//
// From Thetis Project Files/Source/Console/radio.cs:1274-1329
// WDSP: third_party/wdsp/src/fmsq.c:236,244
// SetRXAFMSQRun is NOT declared in fmsq.h but is exported from fmsq.c.
// threshold: linear 0..1 scale (Thetis fm_squelch_threshold = 1.0f, not dB)
// ---------------------------------------------------------------------------

void SetRXAFMSQRun(int channel, int run);

void SetRXAFMSQThreshold(int channel, double threshold);

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
