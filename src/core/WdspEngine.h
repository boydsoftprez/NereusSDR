#pragma once

// =================================================================
// src/core/WdspEngine.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/cmaster.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-05-03 — Phase 3M-3a-iii Task 20 by J.J. Boyd (KG4VCF):
//                 Per-TX-channel DEXP buffer storage added to back the
//                 create_dexp() callsite that was missing from
//                 createTxChannel until 2026-05-03.  See WdspEngine.cpp
//                 for the full root-cause / port narrative.
//                 AI-assisted transformation via Anthropic Claude Code.
// =================================================================

/*  cmaster.cs

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

/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

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

#include "dsp/ChannelConfig.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

#ifdef NEREUS_BUILD_TESTS
// Forward declaration for test-only friend access (see end of class).  The
// test class lives in the global namespace because it inherits from QObject
// in tests/tst_wdsp_engine_tx_channel.cpp without a NereusSDR namespace
// wrapper — friend declarations need the fully-qualified name.
class TestWdspEngineTxChannel;
// Phase 3M-3a-iii Task 20: same pattern for the create_dexp lifecycle test.
class TstWdspEngineDexpInit;
// Phase 3M-4 Task 4: same pattern for the PsFeedbackChannel lifecycle test.
class TstPsFeedbackChannel;
// Phase 3R Task J3: same pattern for the SliceModel RADE mode-swap test;
// needs to flip m_initialized = true so the RxChannel seed at the head of
// the test does not bail out on the !m_initialized guard.
class TestSliceModelRadeSwap;
// Phase 3R Task L2: same friendship for the RadeApplet UI test which
// constructs a real RadioModel + RadeChannel fixture.
class TestRadeApplet;
// Phase 3F Sub-Epic I closeout, defect H1: the per-stream drain-geometry
// test primes the engine so createRxChannel can seed real RX channels.
class TestStreamPoolBinding;
// Phase 3F: the channel-id map test primes the engine so it can watch
// which ids openRxChannelPool actually opens.
class TestWdspChannelIdMap;
class TestRadioModelMoxHardwareFlip;
// TNF Task 1: the notch tune-frequency test primes the engine so
// createRxChannel opens real RX channels. RXANBPSetTuneFrequency
// dereferences rxa[channel].ndb.p before it compares (nbp.c:477-479), so
// the kTestChannel = 99 never-opened-channel hatch is unavailable here.
class TestNotchTuneFrequency;
#endif

namespace NereusSDR {

class RxChannel;
class TxChannel;
class PsFeedbackChannel;
class RadeChannel;

// Central WDSP manager. Owns all RxChannel instances and manages
// system-level initialization (FFTW wisdom, impulse cache).
//
// Owned by RadioModel. Created once per radio connection.
// Thread safety: create/destroy on main thread only.
//                processIq called from audio callback via RxChannel.
//
// Ported from Thetis cmaster.cs:491 (CMCreateCMaster) and
// cmaster.c:32-93 (create_rcvr).
class WdspEngine : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initializedChanged)

public:
    explicit WdspEngine(QObject* parent = nullptr);
    ~WdspEngine() override;

    // --- System lifecycle ---

    // Check if wisdom file needs to be generated (first run).
    // If true, initialize() will take 30-60s and emit wisdomProgress.
    static bool needsWisdomGeneration(const QString& configDir);

    // Initialize WDSP: load FFTW wisdom, initialize impulse cache.
    // configDir: directory for wisdom file and impulse cache.
    // Wisdom runs async — listen to initializedChanged for completion.
    bool initialize(const QString& configDir);

    // Shutdown WDSP: save impulse cache, destroy all channels, free resources.
    void shutdown();

    bool isInitialized() const { return m_initialized; }

    // --- WDSP channel-id map (Phase 3F) --------------------------------
    //
    // WDSP keys every channel off one global array, `struct _ch
    // ch[MAX_CHANNELS]` (third_party/wdsp/src/channel.c:29), and
    // OpenChannel overwrites `ch[channel]` and starts a fresh wdspmain
    // thread without closing whatever was there (channel.c:75-101).  Two
    // subsystems that pick the same id therefore both "work" until the
    // second one silently orphans the first one's thread, iobuffs,
    // semaphores and critical sections.  The ids below are the single
    // allocation table for the whole application.
    //
    // Numbering comes from upstream, not from convenience.
    //
    // From Thetis ChannelMaster/cmsetup.c:176-191 [v2.10.3.15] — chid():
    //   case 0:  ch_id = pcm->cmSubRCVR * stream + subrx;          // rx
    //   case 1:  ch_id = stream + (pcm->cmSubRCVR - 1) * pcm->cmRCVR;  // tx
    // C# mirror, From Thetis dsp.cs:926-944 [v2.10.3.15] — WDSP.id():
    //   case 2:  return cmaster.CMsubrcvr * cmaster.CMrcvr;        // txa
    // Upstream's radio structure is cmRCVR = 5, cmSubRCVR = 2
    // (From Thetis cmaster.cs:412,419 [v2.10.3.15]), cmXMTR = 1
    // (cmaster.cs:502), so Thetis lays out ch[0..9] = RX and ch[10] = TX.
    // Both counts are compile-time maxima: create_rcvr opens all
    // cmRCVR * cmSubRCVR channels at CreateRadio time no matter how many
    // receivers the connected radio actually has, so the TX id is a fixed
    // constant rather than something that moves per radio.
    //
    // NereusSDR's radio structure is one WDSP channel per slice with no
    // sub-receivers, i.e. cmSubRCVR = 1, cmRCVR = kMaxSliceChannels,
    // cmXMTR = 1.  Substituting into chid() gives:
    //   rx:  ch_id = 1 * slice + 0             = slice
    //   tx:  ch_id = kMaxSliceChannels + 0     = kMaxSliceChannels
    // The upstream formula therefore reproduces the Phase 3F invariant
    // "WDSP channel id == slice index" exactly, and puts TX immediately
    // above the RX block.  No reconciliation was needed.
    //
    // kMaxSliceChannels is the ceiling over every SKU's
    // BoardCapabilities::maxSlices (largest today: 5, the Saturn /
    // Angelia / Orion / HermesC10 class).  Sizing the reserved block off
    // the maximum rather than off the connected radio is what keeps
    // kTxChannelId a constant, exactly as cmRCVR does upstream.
    // tst_wdsp_channel_id_map pins the ceiling against the caps table.
    static constexpr int kMaxSliceChannels = 5;

    // RX slice channels occupy [0, kMaxSliceChannels).
    static constexpr int kFirstSliceChannelId = 0;

    // TX channel id = cmSubRCVR * cmRCVR = 1 * kMaxSliceChannels.
    static constexpr int kTxChannelId = kMaxSliceChannels;

    // --- RX Channel management ---

    // Create an RX channel with the given parameters.
    // Returns the new RxChannel (owned by WdspEngine) or nullptr on failure.
    // channelId: WDSP channel number (0-31). Must be unique.
    //
    // Default parameters match our P2 DDC configuration:
    //   inputBufferSize=238 (one P2 packet), dspBufferSize=4096,
    //   all rates=48000 (no resampling needed)
    //
    // From Thetis cmaster.c:72-86 (OpenChannel call in create_rcvr)
    RxChannel* createRxChannel(int channelId,
                               int inputBufferSize = 238,
                               int dspBufferSize = 4096,
                               int inputSampleRate = 48000,
                               int dspSampleRate = 48000,
                               int outputSampleRate = 48000);

    // Destroy an RX channel by ID. The RxChannel pointer becomes invalid.
    void destroyRxChannel(int channelId);

    // Look up an existing RX channel by WDSP channel ID.
    RxChannel* rxChannel(int channelId) const;

    // --- External Diversity management ---------------------------------
    //
    // WDSP owns exactly two external-diversity slots in a process-wide
    // pdiv[MAX_EXT_DIVS] table (third_party/wdsp/src/div.c:104-105). These
    // IDs are independent of RXA channel IDs, so their lifecycle belongs to
    // WdspEngine rather than RxChannel.
    //
    // Upstream ownership/order:
    //   CreateRadio -> create_sync -> create_divEXT(0, 0, 2, 1024)
    //   InboundBlock -> xdivEXT
    //   DestroyRadio -> destroy_sync -> destroy_divEXT
    // From Thetis ChannelMaster/cmsetup.c:89-102 and sync.c:32-51 [@501e3f5].
    bool createExternalDiversity(int id, int inputs, int complexSamples);
    void configureExternalDiversity(int id, int output,
                                    const double* iRotate,
                                    const double* qRotate,
                                    int inputs);
    bool processExternalDiversity(int id, int complexSamples,
                                  double** inputs, double* output);
    void setExternalDiversityRunning(int id, bool running);
    void destroyExternalDiversity(int id);

#ifdef NEREUS_BUILD_TESTS
    // Injectable C-API table for lifecycle/order tests. Production builds
    // bind the corresponding members to the real WDSP symbols in the
    // constructor and do not expose a replacement seam.
    struct ExternalDiversityApiForTest {
        void (*create)(int, int, int, int);
        void (*destroy)(int);
        void (*process)(int, int, double**, double*);
        void (*setRun)(int, int);
        void (*setNr)(int, int);
        void (*setOutput)(int, int);
        void (*setRotate)(int, int, double*, double*);
    };
    void setExternalDiversityApiForTest(
        const ExternalDiversityApiForTest& api);
#endif

    // Rebuild an RX channel in-place: capture state, destroy the existing
    // WDSP channel, recreate with new config, reapply state.
    //
    // Returns elapsed milliseconds (≥ 0 on success). Returns -1 if the
    // channel ID is not found or WdspEngine is not initialized.
    //
    // Thread safety: call on main thread only. The audio thread must not
    // be feeding samples into the channel during rebuild (caller is
    // responsible for pausing the feed).
    //
    // ⚠ Avoid for live rate changes — destroying the C++ wrapper
    // invalidates every cached raw pointer (RadioModel, TxWorkerThread,
    // PureSignal, MeterPoller, TwoToneController, TxCfcDialog,
    // TxChannel::s_voxKeyInstance).  Use setRxChannelRate / Thetis-style
    // SetInputSamplerate path for live rate changes instead.
    qint64 rebuildRxChannel(int channelId, const ChannelConfig& cfg);

    // Live sample-rate change for an existing RX channel.  Mirrors
    // ChannelMaster/cmaster.c::SetXcmInrate at lines 453-507 [v2.10.3.13]:
    // updates the channel's input rate / input buffsize without destroying
    // the C++ wrapper.  RxChannel raw pointers stay valid across the call.
    //
    // Returns true on success, false if the channel ID is not found.
    // Thread safety: call on main thread only.  Caller is responsible for
    // draining the channel via SetChannelState(0, drain=1) and stopping
    // the radio data flow before calling — see RadioModel::setSampleRateLive
    // for the full Thetis-faithful sequence ported from setup.cs:7003-7159.
    bool setRxChannelRate(int channelId, int newRateHz);

    // --- Per-board ChannelMaster-layer WDSP calls (Phase B4'/B5') ---
    //
    // These wrap ChannelMaster-exported symbols that Thetis calls at connect
    // time from clsHardwareSpecific.cs:85-191 [v2.10.3.15].  In NereusSDR,
    // the symbols resolve to glue stubs in netinterface_stub.c until the
    // ChannelMaster module is ported.
    //
    // Call both at connect time (RadioModel::connectToRadio) using values from
    // HardwareProfile — see the §B6' wiring block in RadioModel.cpp.

    // From Thetis cmaster.SetADCSupply / txgain.c:164 [v2.10.3.15] —
    // per-board PA over-drive protection scaling.  v is 33 or 50 (volts).
    // txid is the transmitter index (always 0 for current OpenHPSDR boards).
    // Skips the call when v == 0 (sentinel "not set / use WDSP default").
    // Thread safety: call on main thread only, before DSP is running.
    void setAdcSupply(int txid, int v);

    // From Thetis NetworkIO.LRAudioSwap / netInterface.c:1409 [v2.10.3.15] —
    // per-board L/R audio stream stereo-pair swap for the outbound P2/ETH
    // audio path (sendOutbound()).  swap is 0 (no swap, modern boards) or
    // 1 (swap, Hermes-family boards).
    // Thread safety: call on main thread only, before DSP is running.
    void setLRAudioSwap(int swap);

    // --- RADE Channel management (Phase 3R Task J2) ---
    //
    // RADE (Radio Autoencoder) is a neural-codec digital voice mode
    // wrapped by RadeChannel (src/core/RadeChannel.{h,cpp}, Tasks I1-I3).
    // It is NOT a WDSP channel: WDSP has no knowledge of RADE.  The
    // lifecycle below is pure C++ object management - no OpenChannel /
    // CloseChannel / WDSP-side initialization.  createRadeChannel does
    // not require m_initialized = true.
    //
    // Channel-ID namespace: RadeChannel IDs share the integer space
    // with createRxChannel / createTxChannel by convention (Phase 3R
    // Task J3 maps each slice ID 0..N to one channel at a time).
    // WdspEngine itself does not enforce that constraint - callers
    // (J3's setDspMode swap) are responsible for sequencing
    // destroy-old-RxChannel then create-new-RadeChannel and vice
    // versa.

    // Create a RadeChannel for the given slice ID.  The channel is
    // parented to the WdspEngine so QObject ownership cleans up
    // correctly on engine destruction.  Returns the channel pointer
    // on success.  If a channel with the same id already exists,
    // returns the existing pointer without constructing a new one
    // (mirrors createRxChannel's pre-existence guard at the head of
    // WdspEngine.cpp:368-371).
    //
    // The returned channel is NOT auto-started; the caller (J3 mode
    // swap) is responsible for calling RadeChannel::start(modelPath)
    // so it can supply the right model-path / sentinel handling.
    RadeChannel* createRadeChannel(int channelId);

    // Destroy a RadeChannel by ID.  Calls RadeChannel::stop() (which
    // is idempotent for an already-stopped channel) before erasing
    // the wrapper.  The pointer becomes invalid after this call.
    // Idempotent: destroying a non-existent id is a safe no-op.
    void destroyRadeChannel(int channelId);

    // Look up an existing RadeChannel by ID.  Returns nullptr if no
    // channel with that ID is registered.
    RadeChannel* radeChannel(int channelId) const;

    // --- TX Channel management ---

    // TX channel constants derived from Thetis cmaster.c:177-190 [v2.10.3.13].
    // From cmaster.c:184  — channel type 1 = TX (vs. RX = 0).
    static constexpr int kTxChannelType    = 1;
    // From cmaster.c:190  — block until output available (bfo = 1 for TX).
    static constexpr int kTxBlockOnOutput  = 1;
    // From cmaster.c:187  — tslewup  = 0.010 s (10 ms channel-level state envelope).
    static constexpr double kTxTSlewUpSecs   = 0.010;
    // From cmaster.c:189  — tslewdown = 0.010 s (10 ms channel-level state envelope).
    static constexpr double kTxTSlewDownSecs = 0.010;
    // From cmaster.c:182  — DSP sample rate for TX channel = 96000 Hz.
    static constexpr int kTxDspSampleRate  = 96000;
    // DSP buffer size for TX channel = 2048 samples.
    //
    // Deviation from Thetis: cmaster.c:180 [v2.10.3.13] hardcodes 4096.
    // We adopt deskhpsdr's 2048 (transmitter.c:1072 [@120188f] —
    // `tx->dsp_size = 2048`) for two reasons:
    //   1. WDSP iobuffs.c:577 wraps r2_outidx with `==` rather than modulo:
    //        `if ((a->r2_outidx += a->out_size) == a->r2_active_buffsize) ...`
    //      With dsp_size=4096 + in_size=238 (P2 5 ms tick), out_size=952
    //      and r2_active_buffsize=16384, which is not a multiple of 952 —
    //      the wrap never triggers and fexchange2 reads past the end of
    //      r2_baseptr into random heap (verified by bench, r2_outidx grew
    //      unbounded to >900 000).  With dsp_size=2048 + in_size=256
    //      (this header), out_size=1024 and r2_active_buffsize=8192,
    //      which divides cleanly (8 wraps per ring cycle).
    //   2. ~50 % lower TX pipeline latency: dsp_insize/in_rate +
    //      dsp_outsize/out_rate drops from 85 ms to 42 ms.
    static constexpr int kTxDspBufferSize  = 2048;

    // Create a TX channel with the given parameters.
    //
    // Channel ID convention: pass kTxChannelId.  Thetis uses
    // `chid(inid(1, 0), 0)`; with NereusSDR's radio structure
    // (CMsubrcvr=1, CMrcvr=kMaxSliceChannels) that resolves to
    // kMaxSliceChannels.  C# equivalent: `WDSP.id(1, 0)` —
    // dsp.cs:926-944 [v2.10.3.15] case 2 returns `CMsubrcvr * CMrcvr`.
    // See the channel-id map at the top of this class for why the RX
    // block is sized off the ceiling rather than off the live radio.
    //
    // Phase 3F: this used to be a literal 1, which sat inside the RX
    // slice pool.  On a 5-slice SKU the pool opened ch[1] as an RXA and
    // txSetup then overwrote it with a TXA, orphaning the RX thread.
    //
    // Opens the WDSP TX channel (OpenChannel type=1) and constructs the
    // TxChannel C++ wrapper around the 31-stage TXA pipeline that WDSP built.
    // Returns the TxChannel pointer on success, nullptr if WDSP is not
    // initialized.
    //
    // Default parameters match our P2 configuration:
    //   inputBufferSize=256 (5.33 ms at 48 kHz; satisfies WDSP r1 ring
    //                       wrap math — must divide DSP_MULT × dsp_insize
    //                       = 2 × 1024 = 2048; 2048/256 = 8 ✓),
    //   dspBufferSize=kTxDspBufferSize (2048, deskhpsdr-derived; see
    //                                  the `kTxDspBufferSize` definition
    //                                  above for the full rationale),
    //   inputRate=48000, dspRate=96000, outputRate=48000.
    //
    // From Thetis cmaster.c:177-190 (create_xmtr OpenChannel call) [v2.10.3.13]
    TxChannel* createTxChannel(int channelId,
                               int inputBufferSize = 256,
                               int dspBufferSize = kTxDspBufferSize,
                               int inputSampleRate = 48000,
                               int dspSampleRate = kTxDspSampleRate,
                               int outputSampleRate = 48000);

    // Destroy a TX channel by ID. Idempotent — safe to call even if the
    // channel was never created or was already destroyed.
    void destroyTxChannel(int channelId);

    // Look up an existing TX channel by WDSP channel ID.
    // Returns nullptr if not found. If the channel exists, the pointer is
    // always non-null (wrapper is always constructed alongside the WDSP channel).
    TxChannel* txChannel(int channelId) const;

    // --- PureSignal feedback channel management (Phase 3M-4 Task 4) ---

    // PS feedback channel id.  Type=0 (RX) per WDSP channel.c convention.
    //
    // Sits immediately above the TX channel.  PureSignal feedback has no
    // WDSP-channel analogue upstream (Thetis runs it inside the TX
    // channel via SetPSFeedbackRate(txch, ps_rate), cmaster.cs:539
    // [v2.10.3.15]), so this is a NereusSDR extension.  Upstream's
    // closest concept is a "special stream", and those are numbered after
    // the transmitters — From Thetis ChannelMaster/cmsetup.c:86-89
    // [v2.10.3.15]: `sp0id(stream) = stream - pcm->cmRCVR - pcm->cmXMTR`.
    // Keeping the same rx / tx / special ordering puts PS at kTxChannelId
    // + 1.
    //
    // Phase 3F: was a literal 5, which the RX slice pool now owns on
    // 5-slice SKUs.  Derived from kTxChannelId so the block can never
    // drift back into the pool.
    static constexpr int kPsFeedbackChannelId   = kTxChannelId + 1;
    static constexpr int kPsFeedbackChannelType = 0;   // RX type (cmaster.c:184)

    // Default PS feedback rate for G2-class boards per cmaster.cs:424
    // [v2.10.3.13] (`ps_rate = 192000`).  HL2 uses rx1_rate via the
    // BoardCapabilities::psSampleRate=0 sentinel; the PureSignal coordinator
    // (Task 7) re-applies the per-board rate before MOX.
    static constexpr int kPsFeedbackDefaultSampleRate = 192000;

    // Look up the PureSignal feedback channel wrapper.  Returns nullptr
    // until openPsFeedbackChannel() (called from finishInitialization in
    // production builds, or from openPsFeedbackChannelForTesting() in
    // tests) has run.
    PsFeedbackChannel* psFeedbackChannel() const;

#ifdef NEREUS_BUILD_TESTS
    // Test-only helper that synchronously opens the PS feedback channel
    // without going through the async wisdom path.  Mirrors the
    // m_initialized=true friend-access trick from
    // tst_wdsp_engine_dexp_init.cpp; safe to call only after
    // m_initialized=true was set via friend access.
    //
    // Production code path: openPsFeedbackChannel() is invoked from
    // finishInitialization() right after the impulse cache loads.  Tests
    // bypass that to avoid the 30-60s wisdom build.
    void openPsFeedbackChannelForTesting();
#endif

    // Rebuild a TX channel in-place: capture state, destroy the existing
    // WDSP channel, recreate with new config, reapply state.
    //
    // Returns elapsed milliseconds (>= 0 on success). Returns -1 if the
    // channel ID is not found or WdspEngine is not initialized.
    //
    // Thread safety: call on main thread only. The TX worker thread must not
    // be running (setRunning(false) + thread stop before calling this).
    qint64 rebuildTxChannel(int channelId, const ChannelConfig& cfg);

    // --- Metering ---

    // Returns the averaged S-meter reading (dBm) from the RXA pipeline.
    // Reads WDSP RXA_S_AV via GetRXAMeter; the value is only meaningful
    // after the channel has been activated with SetChannelState(channel, 1).
    // Returns -140.0 sentinel when the engine is not yet initialized.
    // Prefer RxChannel::getMeter(RxMeterType::SignalAvg) when a channel
    // wrapper is available -- this helper is for callers that hold only
    // the engine and a raw WDSP channel id (e.g. the Analog S-Meter path).
    //
    // From Thetis Console/dsp.cs:387-388 [@501e3f5] (P/Invoke)
    // From Thetis Console/dsp.cs:957 [@501e3f5] (RXA_S_AV selector inside
    // CalculateRXMeter; the adjacent ADC_REAL case at dsp.cs:959 carries a
    // //MW0LGE [2.9.0.7] inline tag that we preserve per GPL attribution.)
    double getRxaSignalAverage(int channel) const;

    // Returns the peak S-meter reading (dBm) from the RXA pipeline.
    // Reads WDSP RXA_S_PK via GetRXAMeter (enum value 0).
    // Used by SMeterWidget RxMode::SMeter and RxMode::SMeterPeak paths
    // in MeterPoller::pollSMeter() (Task 41, Phase 3P-II).
    // Returns -140.0 sentinel when the engine is not yet initialized.
    //
    // From Thetis Console/dsp.cs:387-388 [@501e3f5] (P/Invoke)
    // From Thetis Console/dsp.cs:889 [@501e3f5] (rxaMeterType enum: RXA_S_PK = 0)
    // From Thetis Console/dsp.cs:954 [@501e3f5] (RXA_S_PK selector inside
    // CalculateRXMeter; adjacent ADC_REAL case at dsp.cs:959 carries a
    // //MW0LGE [2.9.0.7] inline tag that we preserve per GPL attribution.)
    double getRxaSignalPeak(int channel) const;

    // Configure the strongest-bin-in-passband detector for a display channel.
    // Pass the current DDC sample rate, the active filter edges (Hz, relative
    // to DDC center), the smoothing tau (seconds), and the display frame rate.
    // run is always 1 (detector enabled); call with run=0 via the raw WDSP
    // API to disable.  Defaults match the Thetis developer example in
    // wdsp/analyzer.c:1442 [@501e3f5]:
    //   SetupDetectMaxBin(1, 0, 0, 0, 192000.0, -3000.0, -300.0, 0.5, 60)
    //
    // From Thetis Console/dsp.cs:846-847 [@501e3f5] (P/Invoke)
    // From Thetis wdsp/analyzer.c:775 [@501e3f5] (DSP body)
    // Call site: Thetis Console/console.cs:51150 [@501e3f5]
    void setupMaxBinDetector(int displayChannel,
                             int ss = 0, int LO = 0,
                             double rateHz = 192000.0,
                             double fLowHz = -3000.0,
                             double fHighHz = -300.0,
                             double tauSeconds = 0.5,
                             int frameRate = 60);

    // Returns the strongest-bin dBm value from the configured detector.
    // Returns -400.0 (WDSP's dmb_max_dB initial value) if the detector
    // has never processed a display frame.
    //
    // Algorithm ported from Thetis wdsp/analyzer.c:830 [@501e3f5].
    // NereusSDR-native: runs against FFTEngine dBm bins; see
    // setupMaxBinDetector docstring for the full rationale.
    double getMaxBinDbm(int displayChannel) const;

    // Set the CTUN slice-to-DDC frequency offset in Hz for a detector.
    //
    // FFTEngine bins are emitted in DDC baseband (bins[N/2] = DDC NCO
    // freq).  With CTUN enabled (default), the user's tuned slice does
    // NOT match DDC center; the slice sits at an offset within the DDC
    // passband.  Without applying that offset to the bin scan range,
    // MaxBin scans bins near DDC center (typically noise floor) instead
    // of the user's signal, and the meter does not track modulation.
    //
    // sliceOffsetHz = sliceFreqHz - ddcCenterHz (signed; 0 when CTUN off
    // because hardware DDC NCO follows the slice).  Re-call whenever
    // either side moves.  Defaults to 0; thread-safe via the same
    // m_maxBinDetectors store as setupMaxBinDetector / getMaxBinDbm.
    //
    // NereusSDR-only API: Thetis's WDSP analyzer subsystem (CreateAnalyzer
    // + SetAnalyzer + Spectrum) is fed by the SHIFTED WDSP channel so its
    // analyzer DC is always the slice DC.  NereusSDR taps FFTEngine ahead
    // of the WDSP shift, so we apply the shift in our MaxBin scan.
    void setMaxBinSliceOffsetHz(int displayChannel, double sliceOffsetHz);

public slots:
    // Fed by FFTEngine::fftReady.  binsDbm is FFT-shifted (negative freqs
    // first, then positive).  Updates every active MaxBinDetector.
    //
    // Algorithm ported from Thetis wdsp/analyzer.c:800-822 [@501e3f5]:
    // scan for max in configured [firstBin, lastBin] window; apply
    // slow-release smoothing (decay = exp(-1/(tau*fps))), fast peak attack.
    // NereusSDR-native: binsDbm already in dBm so no magnitude-to-dB step.
    //
    // 2026-05-22 bench fix: this path now serves as the fallback source
    // for MaxBin. The primary source is setMaxBinDbmFromSpectrum below,
    // which feeds the post detector + avenger pixel peak from
    // SpectrumWidget so the meter matches what the operator sees on the
    // spectrum trace. The raw per-bin FFT power scanned here can be
    // ~12-17 dB below the spectrum's displayed pixel value because the
    // detector pipeline reconstructs window-spread integrated power that
    // a single bin can't show on its own.
    void onSpectrumBinsForMaxBin(int receiverId, const QVector<float>& binsDbm);

    // 2026-05-22 bench fix: direct override of the MaxBin detector's
    // smoothed value from SpectrumWidget's m_renderedPixels (the post
    // detector + avenger output that the operator sees). Called once per
    // render frame; bypasses the per-frame peak-hold-with-decay smoothing
    // because m_renderedPixels already includes the avenger's time
    // smoothing. Stamps d.active=true so getMaxBinDbm returns the new
    // value instead of the -400 sentinel before the first call.
    void setMaxBinDbmFromSpectrum(int displayChannel, double dbm);

signals:
    void initializedChanged(bool initialized);
    // Emitted during wisdom generation. percent=0-100, status=what's being planned.
    void wisdomProgress(int percent, const QString& status);

private:
    bool m_initialized{false};
    QString m_configDir;

    // True when wisdom was regenerated this session.
    // Used by finishInitialization() to skip loading a now-stale impulse
    // cache file (mirrors Thetis radio.cs:151-158 [v2.10.3.13] rebuilt guard).
    bool m_wisdomWasRebuilt{false};

    // Finish initialization after WDSPwisdom completes.
    // wisdomWasRebuilt: true when WDSPwisdom generated a new file this session.
    void finishInitialization(bool wisdomWasRebuilt);

    // RX channels keyed by WDSP channel ID.
    std::map<int, std::unique_ptr<RxChannel>> m_rxChannels;

    struct ExternalDiversitySlot {
        std::atomic_bool created{false};
        std::atomic_bool running{false};
        int inputs{0};
        int complexSamples{0};
    };
    static constexpr int kExternalDiversitySlots = 2;
    static bool validExternalDiversityId(int id)
    {
        return id >= 0 && id < kExternalDiversitySlots;
    }
    std::array<ExternalDiversitySlot, kExternalDiversitySlots>
        m_externalDiversity;

    void (*m_extDivCreate)(int, int, int, int){nullptr};
    void (*m_extDivDestroy)(int){nullptr};
    void (*m_extDivProcess)(int, int, double**, double*){nullptr};
    void (*m_extDivSetRun)(int, int){nullptr};
    void (*m_extDivSetNr)(int, int){nullptr};
    void (*m_extDivSetOutput)(int, int){nullptr};
    void (*m_extDivSetRotate)(int, int, double*, double*){nullptr};
    void destroyAllExternalDiversity();

    // RADE channels keyed by slice ID (Phase 3R Task J2).  Shares the
    // integer namespace with m_rxChannels / m_txChannels by convention;
    // callers (J3's setDspMode mode swap) are responsible for sequencing
    // destroy-old then create-new on transitions.  std::map is chosen to
    // match the existing RX channel container.
    std::map<int, std::unique_ptr<RadeChannel>> m_radeChannels;

    // TX channels keyed by WDSP channel ID.
    // Each entry holds a TxChannel C++ wrapper around the 31-stage TXA
    // pipeline that WDSP constructs when OpenChannel(type=1) is called.
    // destroyTxChannel's erase() runs the unique_ptr destructor automatically.
    std::map<int, std::unique_ptr<TxChannel>> m_txChannels;

    // NereusSDR-native strongest-bin-in-passband detector state.
    //
    // Algorithm from Thetis wdsp/analyzer.c:688-830 [@501e3f5]; implemented
    // here because the WDSP analyzer pipeline (CreateAnalyzer + SetAnalyzer
    // + Spectrum buffer feed) is not wired in NereusSDR -- FFTEngine uses raw
    // FFTW3 directly.  The public API (setupMaxBinDetector / getMaxBinDbm)
    // preserves the Thetis names; the implementation runs the same scan +
    // slow-release smoothing on the dBm bins emitted by FFTEngine::fftReady
    // via onSpectrumBinsForMaxBin.
    struct MaxBinDetector {
        bool   active{false};
        double rate{192000.0};
        double fLow{-3000.0};
        double fHigh{-300.0};
        // CTUN slice offset within DDC baseband (Hz).  Added to fLow/fHigh
        // when computing the bin scan window so MaxBin tracks the user's
        // tuned slice, not the DDC NCO center.  See setMaxBinSliceOffsetHz
        // for the full rationale.  Defaults to 0 (CTUN off; DDC follows
        // slice; offset is zero).
        double sliceOffsetHz{0.0};
        double tau{0.5};
        int    frameRate{60};
        double decay{0.0};      // exp(-1.0 / (tau * frameRate))
        double maxDb{-400.0};   // smoothed max in dBm (Thetis dmb_max_dB sentinel)
    };
    // Indexed by display channel (disp).  Grown on demand in setupMaxBinDetector.
    QVector<MaxBinDetector> m_maxBinDetectors;

    // Per-TX-channel DEXP in/out buffer (Phase 3M-3a-iii Task 20).
    //
    // Backs the create_dexp() callsite in createTxChannel.  Mirrors Thetis
    // ChannelMaster's pcm->in[in_id] buffer at cmaster.c:285 [v2.10.3.13]
    // (allocated for every TX-stream slot, passed to BOTH create_dexp's
    // `in` and `out` parameters at cmaster.c:134-135 [v2.10.3.13]) — but
    // NereusSDR uses a parallel-only architecture, so this buffer is
    // private to the DEXP detector and never feeds the fexchange0 audio
    // path (TxWorkerThread::m_in is a separate buffer that fexchange0
    // reads).  TxWorkerThread::dispatchOneBlock copies a snapshot of m_in
    // into this buffer once per audio block (via TxChannel::pumpDexp)
    // before calling xdexp(channelId).
    //
    // Sized 2 * inputBufferSize doubles to hold complex (interleaved I/Q)
    // samples — matches Thetis's complex-sample layout at cmaster.c:285
    // (`getbuffsize(pcm->cmMAXInRate) * sizeof(complex)`).
    //
    // Ownership: WdspEngine.  Lifetime: must outlive the WDSP DEXP DSP
    // module (pdexp[id]) — the WDSP module retains the raw pointer set
    // by create_dexp until destroy_dexp clears it.  destroyTxChannel
    // destroys the WDSP DEXP via destroy_dexp BEFORE erasing this map,
    // so the ordering is correct.
    std::map<int, std::vector<double>> m_dexpBuffers;

    // Phase 3M-4 Task 4: PureSignal feedback RX channel.  Single instance
    // per WdspEngine, opened during finishInitialization() (or via the
    // openPsFeedbackChannelForTesting() helper in NEREUS_BUILD_TESTS
    // builds).  Held as unique_ptr — destruction order matters: the
    // destructor (~WdspEngine via shutdown()) must run CloseChannel(5)
    // BEFORE the unique_ptr destructor erases the wrapper, mirroring the
    // destroyTxChannel / destroyRxChannel lifecycle.
    std::unique_ptr<PsFeedbackChannel> m_psFeedbackChannel;

    // Open the WDSP-side PS feedback channel (OpenChannel + state=1) and
    // construct the wrapper.  Idempotent — second call returns silently.
    // Called from finishInitialization() in production, or from
    // openPsFeedbackChannelForTesting() under NEREUS_BUILD_TESTS.
    void openPsFeedbackChannel();

    // Close the WDSP-side PS feedback channel and destroy the wrapper.
    // Idempotent — called from shutdown() when the engine is torn down.
    void closePsFeedbackChannel();

#ifdef NEREUS_BUILD_TESTS
    // Test-only friend: lets unit tests bypass async wisdom load by setting
    // m_initialized = true directly so they can exercise createTxChannel /
    // createRxChannel without a running event loop or a real WDSP wisdom
    // file.  Production builds (without NEREUS_BUILD_TESTS) never see this.
    friend class ::TestWdspEngineTxChannel;
    // Phase 3M-3a-iii Task 20: same friendship for the create_dexp test.
    friend class ::TstWdspEngineDexpInit;
    // Phase 3M-4 Task 4: same friendship for the PsFeedbackChannel test.
    friend class ::TstPsFeedbackChannel;
    // Phase 3R Task J3: same friendship for the SliceModel RADE mode-swap test.
    friend class ::TestSliceModelRadeSwap;
    // Phase 3R Task L2: same friendship for the RadeApplet UI test.
    friend class ::TestRadeApplet;
    // Phase 3F Sub-Epic I closeout, defect H1: same friendship for the
    // per-stream drain-geometry test, which seeds one real RX channel per
    // slice so it can watch setRxChannelRate follow a stream's rate change.
    friend class ::TestStreamPoolBinding;
    // Phase 3F: same friendship for the channel-id map test, which drives
    // RadioModel::openRxChannelPool and asserts on the ids it opened.
    friend class ::TestWdspChannelIdMap;
    // Authoritative-TX regression: seed nonzero RX channels without the
    // asynchronous wisdom lifecycle so MOX can prove which exact ID moves.
    friend class ::TestRadioModelMoxHardwareFlip;
    // TNF Task 1: same friendship for the notch tune-frequency test, which
    // needs really opened RX channels rather than an unopened slot.
    friend class ::TestNotchTuneFrequency;
#endif
};

} // namespace NereusSDR
