// =================================================================
// src/core/wdsp_api.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/specHPSDR.cs, original licence from Thetis source is included below
//   Project Files/Source/wdsp/gen.c, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-26 — SetTXAPostGen* declarations (4 functions: Mode, ToneMag,
//                 ToneFreq, Run) added by J.J. Boyd (KG4VCF) during 3M-1a
//                 Task C.3 — TxChannel::setTuneTone WDSP wiring. Signatures
//                 match wdsp/gen.c:784-813 [v2.10.3.13]. AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-04-26 — SetTXACFIRRun + per-stage TXA Run setters (SetTXAPreGenRun,
//                 SetTXAPanelRun, SetTXAPHROTRun, SetTXAAMSQRun, SetTXAEQRun,
//                 SetTXACompressorRun, SetTXAosctrlRun, SetTXACFCOMPRun)
//                 added by J.J. Boyd (KG4VCF) during 3M-1a Task C.4 —
//                 TxChannel::setRunning / setStageRunning WDSP wiring.
//                 Signatures match wdsp/ source files [v2.10.3.13].
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
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

/*
*
* Copyright (C) 2010-2018  Doug Wigley 
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

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

// =====================================================================
// NR1 — Adaptive Noise Reduction (WDSP anr.c, Warren Pratt NR0V)
// From Thetis wdsp/anr.h:47-52 [v2.10.3.13]. Channel = WDSP channel id.
// =====================================================================

void SetRXAANRRun(int channel, int setit);

void SetRXAANRVals(int channel, int taps, int delay, double gain, double leakage);

void SetRXAANRTaps(int channel, int taps);

void SetRXAANRDelay(int channel, int delay);

void SetRXAANRGain(int channel, double gain);

void SetRXAANRLeakage(int channel, double leakage);

void SetRXAANRPosition(int channel, int position);  // 0=pre-AGC, 1=post-AGC

// =====================================================================
// ANF — Automatic Notch Filter (WDSP anf.c)
// From Thetis wdsp/anf.h [v2.10.3.13].
// =====================================================================

void SetRXAANFRun(int channel, int setit);

// =====================================================================
// NR2 — EMNR (WDSP emnr.c, Warren Pratt NR0V).
// From Thetis wdsp/emnr.h + setup.cs [v2.10.3.13].
// =====================================================================

void SetRXAEMNRRun(int channel, int run);

void SetRXAEMNRPosition(int channel, int position);

void SetRXAEMNRgainMethod(int channel, int method);

void SetRXAEMNRnpeMethod(int channel, int method);

void SetRXAEMNRaeRun(int channel, int run);

void SetRXAEMNRaeZetaThresh(int channel, double zetathresh);

void SetRXAEMNRaePsi(int channel, double psi);

void SetRXAEMNRtrainZetaThresh(int channel, double thresh);

void SetRXAEMNRtrainT2(int channel, double t2);

// Post-processing cascade — the "Noise post proc" group in Thetis screenshot.
// From Thetis wdsp/emnr.c:951-995 [v2.10.3.13]. Internal scaling: Thetis
// setup.cs divides the UI int value by 100 before passing (e.g. UI 15 → 0.15).
void SetRXAEMNRpost2Run(int channel, int run);

void SetRXAEMNRpost2Nlevel(int channel, double nlevel);

void SetRXAEMNRpost2Factor(int channel, double factor);

void SetRXAEMNRpost2Rate(int channel, double tc);

void SetRXAEMNRpost2Taper(int channel, int taper);

// =====================================================================
// NR3 — RNNR (WDSP rnnr.c, Samphire MW0LGE, rnnoise backend).
// From Thetis wdsp/rnnr.c:161-176, 348-401 [v2.10.3.13].
// =====================================================================

void SetRXARNNRRun(int channel, int run);

void SetRXARNNRPosition(int channel, int position);

void SetRXARNNRUseDefaultGain(int channel, int use_default_gain);

// Global (not per-channel): loads an rnnoise .bin model. Empty path ("")
// reverts to the baked-in default model.
void RNNRloadModel(const char* file_path);

// =====================================================================
// NR4 — SBNR (WDSP sbnr.c, Samphire MW0LGE, libspecbleach backend).
// From Thetis wdsp/sbnr.c:144-241 [v2.10.3.13].
// =====================================================================

void SetRXASBNRRun(int channel, int run);

void SetRXASBNRreductionAmount(int channel, float amount);

void SetRXASBNRsmoothingFactor(int channel, float factor);

void SetRXASBNRwhiteningFactor(int channel, float factor);

void SetRXASBNRnoiseRescale(int channel, float factor);

void SetRXASBNRpostFilterThreshold(int channel, float threshold);

void SetRXASBNRnoiseScalingType(int channel, int noise_scaling_type);

// ---------------------------------------------------------------------------
// Spectral noise blanker (snb.h) — From Thetis dsp.cs P/Invoke declarations
// ---------------------------------------------------------------------------

// WDSP: third_party/wdsp/src/snb.c:579
void SetRXASNBARun(int channel, int run);

// SNB tuning — channel-id based setters callable post-create.
// All declared in WDSP third_party/wdsp/src/snb.c:595-663 [v2.10.3.13].
// Thetis wires these from setup.cs SNB controls (udSNBK1/K2/OutputBW);
// NereusSDR wires them from Setup → DSP → NB/SNB → SNB group.
void SetRXASNBAovrlp           (int channel, int ovrlp);
void SetRXASNBAasize           (int channel, int size);
void SetRXASNBAnpasses         (int channel, int npasses);
void SetRXASNBAk1              (int channel, double k1);
void SetRXASNBAk2              (int channel, double k2);
void SetRXASNBAbridge          (int channel, int bridge);
void SetRXASNBApresamps        (int channel, int presamps);
void SetRXASNBApostsamps       (int channel, int postsamps);
void SetRXASNBApmultmin        (int channel, double pmultmin);
void SetRXASNBAOutputBandwidth (int channel, double flow, double fhigh);

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

// NB1 post-create setters — called after create_anbEXT to override defaults.
// Declared in Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:965-977
// WDSP: third_party/wdsp/src/nob.c
// Thetis cmaster.c:43-53 [v2.10.3.13] create-time defaults:
//   tau=0.0001, hangtime=0.0001, advtime=0.0001, backtau=0.05, threshold=30.0

void SetEXTANBTau(int id, double tau);
void SetEXTANBHangtime(int id, double time);
void SetEXTANBAdvtime(int id, double time);
void SetEXTANBBacktau(int id, double tau);
void SetEXTANBThreshold(int id, double thresh);

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

// backtau: averaging time constant for signal magnitude (seconds)
// From Thetis specHPSDR.cs:931 — SetEXTNOBBacktau(int id, double tau)
// WDSP: third_party/wdsp/src/nobII.c
void SetEXTNOBBacktau(int id, double tau);

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

// ---------------------------------------------------------------------------
// TX PostGen (gen1) — TUNE-tone carrier (gen.c)
//
// gen1 is the last DSP stage before up-slew ramp and output metering.
// Mode 0 = sine tone (used for TUNE carrier).
// Mode 1 = two-tone (used for 2-TONE IMD test — 3M-3a scope).
//
// From Thetis wdsp/gen.c:783-813 [v2.10.3.13].
// Call-site: console.cs:30031-30040 [v2.10.3.13] — chkTUN_CheckedChanged.
// Ported by NereusSDR Task C.3 (3M-1a).
// ---------------------------------------------------------------------------

// From Thetis wdsp/gen.c:784-789 [v2.10.3.13] — txa[ch].gen1.p->run
void SetTXAPostGenRun(int channel, int run);

// From Thetis wdsp/gen.c:792-797 [v2.10.3.13] — txa[ch].gen1.p->mode
// mode: 0=sine tone, 1=two-tone, 2=noise, 3=sweep, 4=sawtooth, 5=triangle, 6=pulse
void SetTXAPostGenMode(int channel, int mode);

// From Thetis wdsp/gen.c:800-805 [v2.10.3.13] — txa[ch].gen1.p->tone.mag
void SetTXAPostGenToneMag(int channel, double mag);

// From Thetis wdsp/gen.c:808-813 [v2.10.3.13] — txa[ch].gen1.p->tone.freq + calc_tone()
// Signed Hz; caller provides sign per DSP mode (LSB/CWL/DIGL → negative).
void SetTXAPostGenToneFreq(int channel, double freq);

// ---------------------------------------------------------------------------
// TX stage Run setters — TxChannel::setRunning() + setStageRunning()
//
// These are called from TxChannel::setRunning() (cfir only) and from
// TxChannel::setStageRunning() to enable/disable individual TXA pipeline
// stages.  Each corresponds to a PORT-exported WDSP function.
//
// NOTE: rsmpin and rsmpout have NO exported Set*Run API. Their run flags are
// managed internally by WDSP's TXAResCheck() (wdsp/TXA.c:809-817
// [v2.10.3.13]), which sets run=1 iff the relevant rates differ.  They are
// therefore absent from this list.
//
// Ported by NereusSDR Task C.4 (3M-1a).
// ---------------------------------------------------------------------------

// cfir (stage 28): custom CIC FIR filter, used for Protocol 2 output path.
// Thetis cmaster.cs:522-527 [v2.10.3.13]: false for P1 (USB), true for P2.
// From Thetis wdsp/cfir.c:233-238 [v2.10.3.13] and wdsp/cfir.h:71.
void SetTXACFIRRun(int channel, int run);

// gen0 (stage 1): TX PreGen, mode=2 noise. Used for 2-TONE (3M-3a).
// From Thetis wdsp/gen.c:636-641 [v2.10.3.13].
void SetTXAPreGenRun(int channel, int run);

// panel (stage 2): audio panel / mic gain.
// From Thetis wdsp/patchpanel.c:201-206 [v2.10.3.13] and patchpanel.h:74.
void SetTXAPanelRun(int channel, int run);

// phrot (stage 3): phase rotator for SSB carrier-phase correction.
// From Thetis wdsp/iir.c:665-670 [v2.10.3.13].
void SetTXAPHROTRun(int channel, int run);

// amsq (stage 5): TX AM squelch / downward expander.
// From Thetis wdsp/amsq.c:246-252 [v2.10.3.13] and amsq.h:83.
void SetTXAAMSQRun(int channel, int run);

// eqp (stage 6): TX parametric EQ.
// From Thetis wdsp/eq.c:742-747 [v2.10.3.13].
void SetTXAEQRun(int channel, int run);

// compressor (stage 14): TX speech compressor (COMP).
// Also adjusts bp1/bp2 bandpass routing via TXASetupBPFilters().
// From Thetis wdsp/compress.c:99-109 [v2.10.3.13] and compress.h:60.
void SetTXACompressorRun(int channel, int run);

// osctrl (stage 16): CESSB overshoot control.
// From Thetis wdsp/osctrl.c:142-147 [v2.10.3.13].
void SetTXAosctrlRun(int channel, int run);

// cfcomp (stage 11): continuous frequency compander.
// From Thetis wdsp/cfcomp.c:632-637 [v2.10.3.13].
void SetTXACFCOMPRun(int channel, int run);

// ── TX channel default config — from deskhpsdr/src/transmitter.c:1459-1473 [@120188f]
// These calls are invoked AFTER OpenChannel(... type=1 ...) to put the WDSP TX
// channel into a sane state.  Without them ALC's max gain integrator runs to
// infinity on silent input (NullMicSource) and produces inf samples on output.
//
// alc (stage 14, wcpAGC): TX ALC.
// From Thetis wdsp/wcpAGC.c:570-610 [v2.10.3.13].
void SetTXAALCSt(int channel, int state);          // wcpAGC.c:570 — ALC on/off (1 = on)
void SetTXAALCAttack(int channel, int attack);     // wcpAGC.c:578 — attack ms (1 = 1 ms)
void SetTXAALCDecay(int channel, int decay);       // wcpAGC.c:586 — decay ms (10 = 10 ms)
void SetTXAALCMaxGain(int channel, double maxgain); // wcpAGC.c:604 — max gain dB (0 = unity, no amplification)

// bp0 (stage 8): mandatory TX bandpass filter.
// From Thetis wdsp/bandpass.c — SetTXABandpass{Window,Run}.
void SetTXABandpassWindow(int channel, int window); // 1 = 7-term Blackman-Harris
void SetTXABandpassRun(int channel, int run);

// SetTXAMode: configure TXA pipeline for an operating mode (LSB/USB/AM/FM/...).
// Activates ammod/fmmod/preemph stages per mode and triggers TXASetupBPFilters.
// Mode integer matches NereusSDR DSPMode enum (LSB=0, USB=1, ..., DRM=11).
// From Thetis wdsp/TXA.c:753-789 [v2.10.3.13].
void SetTXAMode(int channel, int mode);

// SetTXABandpassFreqs: configure bp0 / bp1 / bp2 bandpass cutoffs in IQ space.
// LSB: low=-high_audio, high=-low_audio (e.g. -2850, -150).
// USB: low=+low_audio, high=+high_audio (e.g. +150, +2850).
// AM/DSB/SAM: symmetric (-high, +high).
// From Thetis wdsp/TXA.c:792-799 [v2.10.3.13].
// Cite: deskhpsdr/src/transmitter.c:2091-2191 [@120188f] — tx_set_filter
//   per-mode mapping from audio bandpass to IQ-space bandpass.
void SetTXABandpassFreqs(int channel, double f_low, double f_high);

// gen0 (stage 1): PreGen — pure tone generator (used for 2-TONE; off in 3M-1a).
// From Thetis wdsp/gen.c.
void SetTXAPreGenMode(int channel, int mode);       // 0 = sine
void SetTXAPreGenToneMag(int channel, double mag);
void SetTXAPreGenToneFreq(int channel, double freq);

// panel (stage 2): audio patch panel — selects which input source feeds the chain.
// From Thetis wdsp/patchpanel.c.
void SetTXAPanelSelect(int channel, int select);    // 2 = use Mic I sample (mono mic)

} // extern "C"

#endif // HAVE_WDSP
