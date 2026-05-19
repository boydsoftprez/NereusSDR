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

#include "WdspTypes.h"
#include "dsp/ChannelConfig.h"

#include <QObject>
#include <QString>

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
    // Channel ID convention: Thetis uses `chid(inid(1, 0), 0)`, which with
    // NereusSDR's single-RX (CMsubrcvr=1, CMrcvr=1) layout resolves to
    // channel 1.  C# equivalent: `WDSP.id(1, 0)` — dsp.cs:926-944 [v2.10.3.13]
    // case 2 returns `CMsubrcvr * CMrcvr = 1 * 1 = 1`.
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
    // Slot 5 per the wdsp-integration.md §11.1 documented channel-id design
    // (0=RX1, 1-4 reserved for RX1-div / RX2 / RX2-div / TX, 5=PS feedback).
    // Avoids collision with the current actual code (0=RX1, 1=TX) and leaves
    // headroom for future RX2 / diversity slots without renumbering.
    static constexpr int kPsFeedbackChannelId   = 5;
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
    // From Thetis Console/console.cs:957 [@501e3f5] (RXA_S_AV selector)
    double getRxaSignalAverage(int channel) const;

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
    // From Thetis Console/dsp.cs:849-850 [@501e3f5] (P/Invoke)
    // From Thetis wdsp/analyzer.c:830 [@501e3f5] (DSP body)
    double getMaxBinDbm(int displayChannel) const;

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
#endif
};

} // namespace NereusSDR
