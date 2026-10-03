#pragma once

// no-port-check: AetherSDR-derived NereusSDR file; Thetis cmaster.cs /
// audio.cs references in inline cites are behavioral source-first cites
// for sample sizes / timing / mix coefficient parity only, not Thetis
// logic ports.

// =================================================================
// src/core/AudioEngine.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30 : Radio codec (JJ's ruling): m_radioOutScratch, the radio's
//                 speaker out, every receiving slice as Thetis's mixer 0,
//                 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 : R-R3-45 transmit monitor output by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code. MON plays on the
//                 speakers or the headphones, the operator's own choice,
//                 saved as audio/TxMonitor/Output (this computer's setting).
//   2026-09-24 : vaxBusOpenChanged signal (R-R3-49, R-R3-21) by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 : R-R3-45 Task 2 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Headphones in a remote window: the
//                 master tap can take the speakers' mix alone, a second tap
//                 takes the headphones mix (the Core's headphones stream),
//                 and remote playback can target the headphones output.
//   2026-09-23 : R-R3-45 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Headphones output beside the speakers (VAX
//                 design 5.3, 6.2, 6.3): opened at start() when Setup,
//                 Audio, Devices has it enabled, fed the headphones mix
//                 (the slices routed there), master volume and mute left on
//                 the speakers; headphonesAvailable() for the flags.
//   2026-09-23 : R-R3-43 Task 2 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Per-slice receiver audio taps
//                 beside the VAX tee (SliceAudioTap, four slots); the VAX
//                 tee undoes the feeding slice's own AF gain, not
//                 receiver 1's.
//   2026-09-22 : R-R3-36 fix wave by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. isCaptureReaderOpen() for the MOX
//                 admission check.
//   2026-09-22 : R-R3-36 Task 5 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. The engine owns a CaptureSupervisor
//                 and reads the PC microphone through its stable reader;
//                 start() no longer opens any input, and capture opens only
//                 on demand (acquireCaptureDemand).
//   2026-09-22 — R-R3-36 prerequisite by J.J. Boyd (KG4VCF), AI-assisted
//                 via OpenAI Codex. Separates selected PC-mic source intent
//                 from capture-bus readiness for fail-silent TX routing.
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 QAudioSink feed-and-drain pattern ported from AetherSDR
//                 `src/core/AudioEngine.{h,cpp}` (48 kHz Int16 stereo,
//                 10 ms timer drain, 200 ms buffer cap).
//   2026-04-19 — Sub-Phase 4 refactor by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code. Replaced QAudioSink direct-drain
//                 model with IAudioBus-based speakers / TX-input / VAX[1..4]
//                 ownership + MasterMixer per-slice accumulation. Inline
//                 synchronous flush on the DSP thread; no QTimer, no
//                 QAudioSink. (docs/architecture/2026-04-19-phase3o-vax-plan.md
//                 §Sub-Phase 4 Task 4.1.)
//   2026-04-19 — Sub-Phase 8.5 platform-bus wiring by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code. start() now eagerly
//                 constructs platform-native VAX RX buses (CoreAudioHalBus on
//                 macOS, LinuxPipeBus on Linux) plus a VAX TX virtual bus
//                 (m_vaxTxBus) so 3rd-party apps see the virtual devices the
//                 moment audio is running. Windows BYO wiring deferred to
//                 Sub-Phase 9.
//   2026-04-20 — Sub-Phase 10 Task 10a master-mute API by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code. Adds
//                 setMasterMuted / masterMuted / masterMutedChanged
//                 mirroring the existing setVolume pattern. Mute gates the
//                 speakers push in rxBlockReady ONLY — VAX taps continue to
//                 run regardless (per-channel VAX mute lives in the VAX
//                 applet; the local monitor mute must not silence 3rd-party
//                 apps consuming VAX). Persistence + UI (MasterOutputWidget)
//                 land in Task 10b. Design spec:
//                 docs/architecture/2026-04-19-vax-design.md §5.4 and §6.3.
//   2026-04-20 — Sub-Phase 9 Task 9.2a per-channel VAX rx gain + mute + tx
//                 gain by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Adds std::atomic storage for m_vaxRxGain[1..4],
//                 m_vaxMuted[1..4], m_vaxTxGain plus setters/getters/change-
//                 signals. rxBlockReady now skips the push entirely when a
//                 channel is muted and applies gain via a thread_local scratch
//                 buffer when gain != 1.0f. TX gain is storage-only pending
//                 Phase 3M TX pull wiring. Matches VaxApplet control-wiring
//                 rows in docs/architecture/2026-04-19-vax-design.md §6.4.
//   2026-04-27 — Phase 3M-1b E.3 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Adds txMonitorBlockReady(samples,frames)
//                 slot — the audio-thread consumer of TxChannel::sip1OutputReady.
//                 When m_txMonitorEnabled, expands mono TXA samples to
//                 interleaved stereo (L=R), applies m_txMonitorVolume via
//                 MasterMixer::setSliceGain, and accumulates into m_masterMix
//                 at kTxMonitorSlotId. kTxMonitorSlotId = -2 (negative; distinct
//                 from all non-negative RX slice IDs). Slot is pre-registered
//                 in the ctor. Plan: 3M-1b E.3. Pre-code review §4.3 + §4.4.
//   2026-04-27 — Phase 3M-1b E.4 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Adds std::atomic<bool> m_moxActive
//                 cross-thread MOX-state mirror + setMoxState() setter.
//                 rxBlockReady gates the per-slice speakers push when
//                 m_moxActive && slice->isActiveSlice() — fixes the PR #144
//                 cosmetic regression where RX audio leaked during TUN/MOX.
//                 Non-active slices (e.g. RX2) keep playing. Matches Thetis
//                 IVAC mox state-machine in audio.cs:349-384 [v2.10.3.13].
//                 Phase L (RadioModel integration) wires MoxController::moxStateChanged
//                 → setMoxState via signal/slot. Plan: 3M-1b E.4.
//                 Pre-code review §10.3 + §10.4.
//   2026-04-28 — Phase 3M-1c D.1 / D.2 by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code. Adds 720-sample mic-block
//                 accumulator (m_micBlockBuffer / m_micBlockFill / kMicBlockFrames)
//                 + micBlockReady(const float*, int) Qt signal + clearMicBuffer()
//                 method. pullTxMic feeds the accumulator and emits on every
//                 720-sample full block. Phase E will connect TxChannel as
//                 a Qt::DirectConnection slot. Source: Thetis cmaster.cs:493-518
//                 [v2.10.3.13] (mic stream index 5 = 720 samples @ 48 kHz).
//   2026-04-29 — Phase 3M-1c TX pump architecture redesign by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code. REMOVED 720-sample mic-block accumulator
//                 (kMicBlockFrames / m_micBlockBuffer / m_micBlockFill /
//                 micBlockReady signal / clearMicBuffer slot) and the
//                 bench-fix-A pumpMic timer (m_micPumpTimer /
//                 kMicPumpIntervalMs / pumpMic method).  Architectural
//                 review traced both back to a misread of
//                 cmInboundSize[5]=720 (network arrival block size, not
//                 DSP block size — Thetis's actual DSP block is 64
//                 per cmaster.c:460-487 [v2.10.3.13]).  TX pump now lives
//                 in src/core/TxWorkerThread.{h,cpp} and pulls 256 mono
//                 samples per ~5 ms tick directly via pullTxMic.  pullTxMic
//                 returns to its pre-D.1 form: drain m_txInputBus and
//                 convert to float32 mono, no accumulator side effects.
//                 Plan: docs/architecture/phase3m-1c-tx-pump-architecture-plan.md
//   2026-09-23: R-R3-44 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. VAX in a remote window: openVaxOutputs() opens
//                 the four VAX receive outputs without starting the engine,
//                 vaxOutputPacing() / writeVaxOutput() / vaxOutputHasReader()
//                 let a feeder on its own worker write a channel paced by
//                 that output's clock (per-channel lock against the owner
//                 thread replacing the output), and setVaxOutputsAllowed()
//                 lets nereusd publish no VAX devices at all.
//   2026-09-23: R-R3-23 / R-R3-07 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Remote playback plays on any
//                 speaker rate and channel count the Devices page offers:
//                 remotePlaybackFormat() names the format begin accepted,
//                 and writeRemotePlayback() takes blocks in it.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02, ruling 5.14) by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. The
//                 local VAX tee carries only the slices setVaxSliceMask()
//                 allows (the station device's own on a Core with several
//                 devices). NereusSDR-original.
//   2026-09-25: iPhone app plan Task 76 (R-IOS-31, ruling 9.2) by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. One
//                 mix per owner in place of the one master tap: up to
//                 kMaxOwnerMixes owner mixes, each summing only its own
//                 slices for its own speakers and headphones taps, and the
//                 local output carrying only setLocalOutputSliceMask()'s
//                 slices (the station device's). NereusSDR-original.
//   2026-09-27: Remote-window parity Task 32 (R-IOS-13, R-R3-49) by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. The
//                 transmit monitor to the device that holds transmit:
//                 setOwnerMixMonitor() puts it in one owner mix's speakers
//                 or headphones sum, and setTxMonitorLocal(false) keeps it
//                 off this computer's own outputs. The MOX-gated slice's
//                 own block now drains the mix (drainMixes), so MON is
//                 heard with one slice. NereusSDR-original.
//   2026-09-27: R-R3-49 slice audio view by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code. rxBlockReady reads
//                 a slice's mute, output route and VAX channel from one
//                 atomic word per slice id that RadioModel publishes on the
//                 main thread (setSliceAudioView), never from the slice list
//                 or a SliceModel. NereusSDR-original.
//   2026-09-29: Slice control plan Task 6 by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code. The slice view
//                 carries the slice's AF level, applied in the mix to the
//                 controller's sums only (JJ's ruling: WDSP's panel gain
//                 stays at 1.0). Listening: setOwnerMixListen /
//                 setLocalListen add a slice to a device's own sum at that
//                 device's level. The VAX tee and the receiver taps no
//                 longer undo the AF gain. NereusSDR-original.
// =================================================================

#include "AudioDeviceConfig.h"
#include "IAudioBus.h"
#include "audio/CaptureSupervisor.h"
#include "audio/MasterMixer.h"
#include "audio/VaxChannelMixer.h"
#include "audio/SpeakerFormatConverter.h"

#if defined(Q_OS_LINUX)
#  include "core/audio/LinuxAudioBackend.h"
#endif

#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
// Forward-declare only — AudioEngine.h must not drag in libpipewire types.
// The full type is available in AudioEngine.cpp via
// #include "core/audio/PipeWireThreadLoop.h".
namespace NereusSDR { class PipeWireThreadLoop; }
#endif

#include <QObject>
#include <QString>

#include <array>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

namespace NereusSDR {

class RadioModel;
class SliceModel;

// Synchronous observer for the final receiver master mix.  `samples` is
// borrowed interleaved stereo float32 and is valid only for the duration of
// consume().  Implementations run on the DSP thread and must not block,
// allocate, encode, or queue this pointer for later use.
// R-R3-45: which of this computer's outputs a remote playback context
// plays on. The speakers carry master volume and mute; the headphones
// carry neither (VAX design 6.3).
enum class RemotePlaybackOutput : int { Speakers = 0, Headphones = 1 };

// R-R3-45: where the transmit monitor (MON) plays, chosen by the operator
// beside the MON button. Independent of the receivers' own routes: MON is
// how you sound on air, and it goes where you want to hear it.
enum class TxMonitorOutput : int { Speakers = 0, Headphones = 1 };

class MasterMixAudioTap {
public:
    virtual ~MasterMixAudioTap() = default;
    virtual void consume(const float* samples, int frames, int sampleRateHz) noexcept = 0;
};

// R-R3-43: synchronous observer for one receiver's own audio, tapped at the
// point the local VAX tee reads it: after the MOX gate, before slice mute,
// pan, the mix and master volume. Samples arrive with the slice's AF gain
// undone, as local VAX scales them, so the level does not follow the
// speaker slider. `samples` is borrowed interleaved stereo float32, valid
// only for the duration of consume(). skip() reports frames the receiver
// produced that the MOX gate withheld, so a consumer's positions keep
// real time. Both run on the DSP thread and must not block, allocate,
// encode, or queue the pointer for later use.
class SliceAudioTap {
public:
    virtual ~SliceAudioTap() = default;
    virtual void consume(const float* samples, int frames, int sampleRateHz) noexcept = 0;
    virtual void skip(int frames, int sampleRateHz) noexcept = 0;
};

// Audio engine for NereusSDR (Phase 3O VAX).
//
// Owns one IAudioBus per routable endpoint:
//   - m_speakersBus: the master mix goes here.
//   - m_txInputBus:  TX mic capture source (pull() wiring lands in 3M).
//   - m_vaxBus[0..3]: the four VAX slots.
//
// rxBlockReady(sliceId, samples, frames) is the single RX-audio entry
// point. Called from the DSP worker thread (RxDspWorker) with one
// interleaved stereo block per WDSP fexchange2 drain. Accumulates into
// MasterMixer, taps the slice's VAX channel if selected, and flushes the
// mixed master to m_speakersBus synchronously on the DSP thread. No
// QTimer, no QAudioSink, no m_rxBuffer, no mutex — RT-safety rests on
// PortAudioBus's lock-free SPSC ring.
//
// ── Anti-VOX tap-point signpost (3M-3a-iv post-bench refactor) ──────────
// The anti-VOX cancellation reference is forked from RxDspWorker's demod
// output BEFORE it reaches AudioEngine.  This is correct as long as the
// audio bus stage applies no processing that diverges between outputs
// (per-bus EQ, gain, mute beyond master).  Today's single-output PC speaker
// path satisfies this assumption.  WHEN OUTPUT DIVERGENCE LANDS (radio-
// speaker output with independent processing, or per-bus EQ/gain), the
// anti-VOX tap MUST move from RxDspWorker to AudioEngine's post-mixer
// summing point so the cancellation reference matches the audio actually
// leaving the speakers.  This is a tap-point relocation only; the WDSP
// DEXP block and TxChannel::sendAntiVoxData wrapper stay unchanged.
class AudioEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    // WDSP RX channels deliver 48 kHz audio to this engine regardless of
    // radio wire rate.  The master mixer preserves that rate and stereo
    // geometry; device negotiation is downstream of this point.
    static constexpr int kMasterMixSampleRateHz = 48000;

    // Audio pipeline health state — driven by the flow-state FSM.
    // Healthy  = recent successful feed (DSP audio is flowing).
    // Underrun = single-cycle audio dip (< 3 successive underruns).
    // Stalled  = ≥ 3 successive underruns (needs attention).
    // Dead     = sink not initialised (no audio device opened).
    // Drives the ConnectionSegment ♪ pip status indicator (sub-PR-2 B.4).
    enum class FlowState {
        Healthy,
        Underrun,
        Stalled,
        Dead,
    };
    Q_ENUM(FlowState)

    FlowState flowState() const noexcept { return m_flowState; }

    // Test hooks — drive the flow-state FSM without a real QAudioSink.
    // Production code calls the equivalent private state-machine methods
    // (or will be wired in sub-PR-4 when the segment integration lands).
    // TODO(sub-pr-4): wire QAudioSink::stateChanged → setFlowState for
    //   production path when the segment integration in sub-PR-4 / D.2 lands.
    void simulateSuccessfulFeed();
    void simulateUnderrun();
    void simulatePersistentUnderrun();

    // Non-owning back-pointer. rxBlockReady reads a slice's mute / route /
    // VAX-channel state from setSliceAudioView (R-R3-49), not through this
    // pointer; it uses it only as the "is there a radio" gate. Null is safe (unit tests that construct AudioEngine
    // without a RadioModel): rxBlockReady becomes a no-op.
    void setRadioModel(RadioModel* radio);

    // R-R3-49: what rxBlockReady knows about a slice. The audio thread
    // never walks RadioModel's slice list or touches a SliceModel: the
    // main thread removes and deletes slices while blocks flow. RadioModel
    // publishes this view for every slice id on each add, remove, layout
    // restore and mute / route / VAX change; rxBlockReady reads one atomic
    // word per block. A slice id with no published view (or outside
    // [0, kMaxSliceAudioViews)) is not a slice, and its block is dropped,
    // as sliceById() returning null used to drop it.
    struct SliceAudioView {
        bool present{false};
        bool muted{false};
        bool headphones{false};
        int vaxChannel{0};  // 0 = off, 1..4 = VAX N
        // Slice control plan Task 6: the slice's AF level, 0..1 (the
        // slider's 0..100). The mix applies it to the controller's sums;
        // a listener hears its own level instead, and VAX and the
        // receiver taps neither.
        float afGain{1.0f};
    };
    static constexpr int kMaxSliceAudioViews = 32;  // the VAX slice mask's width
    // Main thread only. Ids outside the range are ignored.
    void setSliceAudioView(int sliceId, const SliceAudioView& view) noexcept;
    // Any thread: one acquire load.
    SliceAudioView sliceAudioView(int sliceId) const noexcept;

    // Legacy start/stop retained for the single-entry-point symmetry the
    // rest of the codebase expects. Speakers bus is opened lazily the
    // first time setSpeakersConfig() runs.
    void start();
    void stop();
    bool isRunning() const { return m_running; }

    /// Pre-register slice ids [0, count) with the master mixer.
    ///
    /// MasterMixer::accumulate() silently drops any id it has no entry for,
    /// and its map must stay structurally frozen while audio streams: the
    /// DSP thread does a lock-free find() on an unordered_map that a
    /// main-thread insert would rehash underneath it (MasterMixer.h:52-56).
    /// So every id a slice may ever be given is registered up front, at
    /// connect, before the DSP thread starts feeding rxBlockReady().
    ///
    /// Idempotent and monotonic — repeated calls only top the map up, and
    /// count is clamped to at least 1 so slice A is always present. An id
    /// with no slice bound to it never receives an accumulate() call, so a
    /// spare registration costs one map entry and nothing else.
    ///
    /// MUST NOT be called once audio is streaming.
    void preregisterSlices(int count);

    /// Admit a slice to the mixer's readiness barrier, or withdraw it.
    ///
    /// The mixer waits for every member before it releases a block, and it
    /// has no timeout that will give up on one (a timeout cannot tell a
    /// slice that is merely late from one that has stopped, and trying
    /// produced the 2026-07-27 G2E scratchy-audio defect). So a slice that
    /// stops being fed while its mixer entry lives on MUST be withdrawn
    /// here, or the barrier waits forever and all audio stops.
    ///
    /// Callers are RadioModel's activateSliceChannel / deactivateSliceChannel
    /// pair, which is the same place the WDSP RX channel starts and stops
    /// producing. Mirrors Thetis SetAAudioMixState (aamix.c:522
    /// [v2.10.3.15]).
    ///
    /// Safe to call while audio is streaming: it only flips atomics on an
    /// entry preregisterSlices() already created.
    void setSliceStreaming(int sliceId, bool streaming);

    // Install or remove the one non-owning synchronous post-master-mix tap.
    // These control-thread calls close callback admission and wait for an
    // already-admitted callback before changing the raw pointer.  The DSP
    // thread performs only atomic admission and a synchronous invoke.
    // `clearMasterMixAudioTap` removes the tap only when `tap` still owns the
    // slot, so stopping an old source cannot detach a newer source.
    //
    // R-R3-45: the tap takes the station's program (every receiver, on the
    // speakers or the headphones, summed) unless `speakersOnly`, when it
    // takes the speakers' mix alone. A Core sends the speakers' mix alone
    // to an app that plays the headphones mix on its own stream. Either way
    // it is taken before master volume and mute.
    void setMasterMixAudioTap(MasterMixAudioTap* tap, bool speakersOnly = false);
    void clearMasterMixAudioTap(MasterMixAudioTap* tap);

    // R-R3-45: the one non-owning synchronous headphones-mix tap: the
    // receivers routed to the headphones, each at its gain, pan and mute,
    // with no master volume or mute. Its own admission gate, as the master
    // tap's, so installing or removing it never withholds a block from the
    // master tap. clear removes `tap` only while it owns the slot.
    void setHeadphonesMixAudioTap(MasterMixAudioTap* tap);
    void clearHeadphonesMixAudioTap(MasterMixAudioTap* tap);

    // Radio codec (2026-09-30): the one non-owning synchronous tap for the
    // radio's own speaker / headphone out. It takes every receiving slice,
    // whichever device owns it (JJ's ruling), on the speakers or the
    // headphones, plus MON where the local outputs carry it, at the master
    // volume, and silence while the master is muted: Thetis sends audio
    // mixer 0 (all receivers and MON) at the AF volume to the radio's
    // codec (cmaster.cs:954-957, netInterface.c:1571-1575 [v2.10.3.15]).
    // The master tap's gate, on its
    // own slot; clear removes `tap` only while it owns the slot and returns
    // once no callback into it runs. Control thread.
    void setRadioOutputTap(MasterMixAudioTap* tap);
    void clearRadioOutputTap(MasterMixAudioTap* tap);

    // R-R3-43: per-slice receiver audio taps, at most kMaxSliceAudioTaps at
    // once. Each slot has its own admission gate, so installing or removing
    // one tap never withholds a block from another tap or from the master
    // tap. setSliceAudioTap installs `tap` for `sliceId` (a tap already
    // installed is moved) and returns false when every slot is taken or the
    // arguments are invalid. clearSliceAudioTap removes `tap` wherever it is
    // installed and returns once no callback into it is running.
    // iPhone app Task 76: eight, so every slice a board can have (five at
    // most) can be tapped at once whichever devices own them; each device
    // still sends at most IMediaTransport::kMaxReceiverAudioStreams.
    static constexpr int kMaxSliceAudioTaps = 8;
    bool setSliceAudioTap(int sliceId, SliceAudioTap* tap);
    void clearSliceAudioTap(SliceAudioTap* tap);
    /// Slots holding a tap now; diagnostics and leak checks.
    int sliceAudioTapCount() const;

    // Task 1.6 — Sample-rate live-apply coordination hooks.
    //
    // pauseInput() / resumeInput() bracket the WDSP channel rebuild during a
    // sample-rate change.  In the current architecture the AudioEngine is
    // passively driven by the DSP worker (rxBlockReady() is called by
    // RxDspWorker on its thread — not by AudioEngine on its own timer).
    // Stopping the DSP worker's I/Q feed before rebuild and restarting it
    // after is sufficient to quiesce audio; these methods are coordination
    // hooks for clarity and for future active-drain (PipeWire/PortAudio
    // restart) implementations.
    //
    // reinitForSampleRate() notifies AudioEngine that the pipeline rate has
    // changed and buffer sizes should be updated.  The bus layer (PortAudio /
    // CoreAudio / PipeWire) negotiates frame sizes at open time, not per-block,
    // so the speakers bus does not need to be closed and reopened for a sample-
    // rate change on the WDSP side (WDSP always delivers 64-sample / 48 kHz
    // audio to AudioEngine regardless of the wire rate).  The method is a hook
    // for future implementations (e.g. VAX rate tracking) and records the new
    // wire rate for diagnostics.
    //
    // Thread safety: must be called from the main thread (same thread as start/stop).
    void pauseInput();
    void resumeInput();
    void reinitForSampleRate(int newWireRateHz);

    // Phase 3O: per-endpoint IAudioBus ownership.
    //
    // Live-reconfig contract (Sub-Phase 12 Task 12.2): setSpeakersConfig
    // acquires m_speakersBusMutex during tear-down + rebuild so that
    // rxBlockReady's try_lock can safely detect an in-progress reconfig
    // and drop the block (≤1 ms of silence is inaudible vs. a use-after-
    // free). setSpeakersConfig itself must NOT be called recursively
    // (not re-entrant); it applies synchronously. The 200 ms intra-control
    // debounce for rapid buffer-size scrub lives in DeviceCard, not here —
    // see addendum §2.1 "intra-control only" wording.
    // Handlers may synchronously call setSpeakersConfig (mutex is released before emit).
    void setSpeakersConfig(const AudioDeviceConfig& cfg);

    // Remote RX playback owns only the existing speakers bus. The caller must
    // stop/join its playback worker before this AudioEngine is destroyed.
    // Begin/end run on the owner thread; format/pacing/write are worker-safe.
    //
    // R-R3-23: begin accepts the speaker at the rate and channel count it
    // opened at: float samples, one or two channels, a rate from
    // kMinRemotePlaybackRateHz to kMaxRemotePlaybackRateHz (the Devices
    // page offers 44.1 kHz to 384 kHz), with playback timing.
    // remotePlaybackFormat() is that format until end, nullopt otherwise.
    // writeRemotePlayback() takes interleaved float frames in that format
    // (its channel count), at most kMaxRemotePlaybackFrames; it fails once
    // the device has reopened in another format, so the caller restarts.
    static constexpr int kMinRemotePlaybackRateHz = 8'000;
    static constexpr int kMaxRemotePlaybackRateHz = 384'000;
    static constexpr int kMaxRemotePlaybackFrames = 4096;
    //
    // R-R3-45: `output` Headphones plays on the headphones output instead,
    // under the same rules, with its own format, pacing and state, and
    // without master volume or mute (VAX design 6.3). Begin needs the
    // headphones output open (the Enabled box, or a remote window's start,
    // opens it) and fails in plain words when none is. A context on one
    // output never touches the other.
    bool beginRemotePlayback(QString* error = nullptr,
                             RemotePlaybackOutput output = RemotePlaybackOutput::Speakers);
    void endRemotePlayback(RemotePlaybackOutput output = RemotePlaybackOutput::Speakers);
    std::optional<AudioFormat> remotePlaybackFormat(
        RemotePlaybackOutput output = RemotePlaybackOutput::Speakers);
    std::optional<IAudioBus::OutputPacing> remotePlaybackPacing(
        RemotePlaybackOutput output = RemotePlaybackOutput::Speakers);
    bool writeRemotePlayback(const QVector<float>& pcm,
                             RemotePlaybackOutput output = RemotePlaybackOutput::Speakers);

    // R-R3-45: stores the headphones device and, when the headphones are
    // enabled, reopens the output on it. Emits headphonesConfigChanged.
    void setHeadphonesConfig(const AudioDeviceConfig& cfg);

    // R-R3-45: the headphones card's Enabled box. On opens the headphones
    // output on the stored device, off closes it. The card persists
    // audio/Headphones/Enabled; start() reads it.
    void setHeadphonesEnabled(bool enabled);
    bool headphonesEnabled() const { return m_headphonesEnabled; }

    // R-R3-45: true while a headphones output is open, so a receiver routed
    // to the headphones is heard. Owner thread.
    bool headphonesAvailable() const { return m_headphonesAvailable; }

    // R-R3-36: stores the TX input selection and hands it to the capture
    // supervisor. Never opens a device and never waits; capture opens only
    // while someone holds a demand (acquireCaptureDemand).
    void setTxInputConfig(const AudioDeviceConfig& cfg);
    AudioDeviceConfig txInputConfig() const;

    // R-R3-36: PC microphone capture runs in the supervised helper process.
    // captureStatus() is the supervisor's last published status;
    // acquireCaptureDemand() returns one unit of demand (the first active
    // lease starts capture, releasing the last stops it); retryCapture()
    // starts a new attempt after a failure. Owner (GUI) thread only.
    CaptureSupervisor::Status captureStatus() const;
    // True while the capture reader is open (the capture thread's own view:
    // it opens before Ready is published and closes before a restart or a
    // failure is). The MOX admission check requires it as well as Ready.
    bool isCaptureReaderOpen() const;
    CaptureSupervisor::Lease acquireCaptureDemand(CaptureSupervisor::Demand demand);
    void retryCapture();

    // Per-VAX device configuration. On Mac/Linux the VAX slots are populated
    // eagerly by start() with the platform-native virtual bus
    // (CoreAudioHalBus / LinuxPipeBus); calling setVaxConfig there replaces
    // the slot with a user-picked PortAudio device (BYO). On Windows the
    // slots stay null until setVaxConfig() runs (Sub-Phase 9 BYO wiring).
    void setVaxConfig(int channel, const AudioDeviceConfig& cfg);  // 1..4

    // Toggle a VAX slot on/off. On Mac/Linux, calling setVaxEnabled(ch, true)
    // for a channel that's already eagerly opened by start() is a no-op (the
    // platform-native bus is already live); setVaxEnabled(ch, false) closes
    // the bus regardless of how it was constructed. On Windows it remains
    // the lazy PortAudio path (creates a default-config PortAudioBus).
    void setVaxEnabled(int channel, bool on);

    // ── R-R3-44: VAX outputs in a remote window, none on the Core ─────────
    // False on nereusd (DaemonApp): the Core host publishes no VAX devices,
    // so start() opens neither the VAX outputs nor the VAX TX device, and
    // setVaxConfig / setVaxEnabled / openVaxOutputs / resetAudioSettings
    // open none. Set before start(). Owner thread.
    void setVaxOutputsAllowed(bool allowed);
    bool vaxOutputsAllowed() const { return m_vaxOutputsAllowed; }

    // iPhone app Task 73 (the several-devices design, ruling 5.14): which
    // slice ids the local VAX tee carries, one bit per id (bit N = slice
    // N). VAX on the Core's computer carries only the station device's
    // slices, so RadioModel clears the bit of every slice a device owns;
    // a slice left out queues nothing for any VAX channel and leaves the
    // channel it was on. All bits set by default: a desktop on its own
    // carries every slice, as before. Any thread; the audio thread reads it
    // without a lock.
    void setVaxSliceMask(quint32 mask) { m_vaxSliceMask.store(mask, std::memory_order_release); }
    quint32 vaxSliceMask() const { return m_vaxSliceMask.load(std::memory_order_acquire); }

    // iPhone app Task 76 (the several-devices design, ruling 9.2): which
    // slice ids the local output plays, one bit per id: the speakers and
    // headphones buses, the master tap and the headphones-mix tap carry
    // only these. The Core's local output plays the station device's mix,
    // so RadioModel sets the same mask as setVaxSliceMask(). All bits by
    // default: a desktop on its own plays every slice, as before. The
    // transmit monitor always plays locally. Any thread.
    void setLocalOutputSliceMask(quint32 mask)
    {
        m_localOutputSliceMask.store(mask, std::memory_order_release);
    }
    quint32 localOutputSliceMask() const
    {
        return m_localOutputSliceMask.load(std::memory_order_acquire);
    }

    // iPhone app Task 76 (ruling 9.2): one mix per owner. A device's media
    // controller takes an owner mix for its session, names its slices, and
    // hangs its speakers' (or whole program) and headphones taps on it,
    // exactly as the master and headphones taps are used, from the same
    // drain and barrier, before master volume and mute. Each tap has its
    // own admission gate, so installing or removing one never withholds a
    // block from another. acquireOwnerMix returns a free slot, or -1 with
    // all taken; releaseOwnerMix removes both taps (returning once no
    // callback into them runs) and frees the slot. Control thread.
    static constexpr int kMaxOwnerMixes = 4;
    int acquireOwnerMix();
    void releaseOwnerMix(int slot);
    void setOwnerMixSliceMask(int slot, quint32 mask);
    quint32 ownerMixSliceMask(int slot) const;
    /// The owner's program (its speakers and headphones sums added) unless
    /// `speakersOnly`, as setMasterMixAudioTap. False for a slot not taken.
    bool setOwnerMixAudioTap(int slot, MasterMixAudioTap* tap, bool speakersOnly = false);
    void clearOwnerMixAudioTap(int slot, MasterMixAudioTap* tap);
    bool setOwnerHeadphonesMixAudioTap(int slot, MasterMixAudioTap* tap);
    void clearOwnerHeadphonesMixAudioTap(int slot, MasterMixAudioTap* tap);
    /// Remote-window parity Task 32: where the transmit monitor goes in this
    /// owner's mix: nowhere (the default, and after acquire and release),
    /// its speakers sum or its headphones sum, at MON's own level
    /// (setTxMonitorVolume). A program tap (not speakersOnly) carries either.
    /// Any thread; the audio thread reads it at the next drain.
    void setOwnerMixMonitor(int slot, MasterMixer::OwnerMonitor monitor);
    MasterMixer::OwnerMonitor ownerMixMonitor(int slot) const;
    /// Owner mixes taken now; diagnostics and leak checks.
    int ownerMixCount() const;

    // Slice control plan Task 6: listening. An owner mix (or the local
    // output, for the station device) hears a slice it does not control
    // at its own level, 0..1, the listener's own and never the slice's AF
    // level; muted is level 0. The level ramps, and a sum that controlled
    // the slice and now listens starts from where the controller's gain
    // was, so the hand-off neither gaps nor steps. A slice the sum also
    // controls plays at the controller's level only. Nothing is acquired
    // or released: a listener adds no stream and no owner mix. Any
    // thread; the audio thread reads the atomics at the next drain.
    void setOwnerMixListen(int slot, int sliceId, float level, bool muted);
    void clearOwnerMixListen(int slot, int sliceId);
    quint32 ownerMixListenMask(int slot) const;
    // The stored level (0 when muted), for tests and diagnostics.
    float ownerMixListenLevel(int slot, int sliceId) const;
    void setLocalListen(int sliceId, float level, bool muted);
    void clearLocalListen(int sliceId);
    quint32 localListenMask() const
    {
        return m_localListenMask.load(std::memory_order_acquire);
    }

    // A remote window opens the four VAX receive outputs start() opens (the
    // engine itself never starts there), skipping a slot that already has
    // one. It opens no VAX TX device: VAX as the microphone waits for
    // remote transmit. In a test run (QStandardPaths test mode) it opens
    // only what setVaxBusFactoryForTest() supplies. Owner thread.
    void openVaxOutputs();

    // A remote window's VAX feeder writes a channel from its own worker
    // thread, never an audio callback. vaxOutputPacing() is the output's
    // playback timing (nullopt when the output is closed or reports none);
    // writeVaxOutput() writes interleaved 48 kHz stereo with the channel's
    // VAX gain and mute applied as the local VAX tee applies them (muted
    // writes silence, so the output's clock keeps running), and returns
    // false when the output is closed or the block is not finite. Both take
    // the channel's lock, which the owner thread also takes to replace the
    // output. At most kMaxVaxWriteFrames per call.
    static constexpr int kMaxVaxWriteFrames = 4096;
    std::optional<IAudioBus::OutputPacing> vaxOutputPacing(int channel);
    bool writeVaxOutput(int channel, const float* stereo, int frames);
    // Whether an app is reading the channel's output, where the platform
    // reports it (IAudioBus::outputHasReader); nullopt otherwise or when
    // the channel has no output. Owner thread.
    std::optional<bool> vaxOutputHasReader(int channel);

#ifdef NEREUS_BUILD_TESTS
    // R-R3-44 test seam: makeVaxBus() (channel 1..4) and makeVaxTxBus()
    // (channel 0) build the platform's VAX device through this factory
    // instead, so a test never attaches to this computer's real VAX
    // devices. An empty function restores the platform. Not consulted
    // while vaxOutputsAllowed() is false.
    void setVaxBusFactoryForTest(std::function<std::unique_ptr<IAudioBus>(int channel)> factory)
    {
        m_vaxBusFactoryForTest = std::move(factory);
    }

    // R3 receiver audio fix wave follow-up: speakers, headphones and the
    // TX input (every PortAudio device the engine opens through makeBus)
    // come from this factory instead, given the device config and whether
    // it is a capture device; the engine opens what it returns. In a test
    // run (QStandardPaths test mode) without a factory the engine opens no
    // real device of either kind, PortAudio or platform VAX.
    using DeviceBusFactory =
        std::function<std::unique_ptr<IAudioBus>(const AudioDeviceConfig& cfg, bool capture)>;
    void setDeviceBusFactoryForTest(DeviceBusFactory factory)
    {
        m_deviceBusFactoryForTest = std::move(factory);
    }
    // R-R3-21: how many times any AudioEngine in this process has called
    // Pa_Initialize / Pa_Terminate. A test run calls neither
    // (PortAudioBus::portAudioBarredForTestRun).
    static int paInitializeCallsForTest();
    static int paTerminateCallsForTest();
#endif

#ifdef NEREUS_BUILD_TESTS
    // Test seam — inject a fake IAudioBus into a VAX slot so unit tests
    // can exercise the rxBlockReady tee without standing up a real
    // CoreAudioHalBus/LinuxPipeBus shm/FIFO. channel is 1..4. Takes
    // ownership of `bus`.
    void setVaxBusForTest(int channel, std::unique_ptr<IAudioBus> bus);

    // Test seam — inject a fake IAudioBus into the speakers slot so unit
    // tests can verify speakers tee without opening a PortAudio device.
    void setSpeakersBusForTest(std::unique_ptr<IAudioBus> bus);

    // Test seam — inject a fake IAudioBus into the headphones slot.
    void setHeadphonesBusForTest(std::unique_ptr<IAudioBus> bus);

    // Test seam — inject a fake IAudioBus into the TX-input slot so unit
    // tests can exercise pullTxMic without standing up a real PortAudio
    // capture device. Takes ownership of `bus`.
    // Plan: 3M-1b E.1.
    // An injected bus takes precedence over the capture supervisor's reader.
    void setTxInputBusForTest(std::unique_ptr<IAudioBus> bus);

    // R-R3-36: replaces the capture supervisor with one built from these
    // options (the scripted fake helper in tests). Valid only while no
    // capture demand is active (asserts otherwise).
    void setCaptureSupervisorOptionsForTest(CaptureSupervisor::Options options);
    // The running capture helper's process id, 0 when none.
    qint64 captureHelperProcessIdForTest() const;

    // Persistent test seam — prepare dependencies before every start().
    // stop() intentionally releases all buses, so reconnect fixtures use
    // this callback to install fresh opened fakes for each lifecycle.
    void setStartInitializerForTest(std::function<void(AudioEngine&)> initializer)
    {
        m_startInitializerForTest = std::move(initializer);
    }

    // Test seam — inject a fake IAudioBus into the VAX-TX slot so unit
    // tests can exercise pullVaxTxMic without standing up a real
    // CoreAudioHalBus / PipeWireBus. Takes ownership of `bus`.
    void setVaxTxBusForTest(std::unique_ptr<IAudioBus> bus);

    // Test seam — expose m_masterMix so tests can call mixInto() to verify
    // that txMonitorBlockReady accumulated audio into the correct slot.
    // Plan: 3M-1b E.3.
    MasterMixer& masterMixForTest() { return m_masterMix; }

    // Test seam: expose m_antiVoxMix so tests can assert what the anti-VOX
    // reference sums, what it refuses to sum, and how often it releases a
    // block. Phase 3F Sub-Epic J Task 9.
    MasterMixer& antiVoxMixForTest() { return m_antiVoxMix; }

    // Signals that withdrawal has invalidated both mixers, immediately
    // before setSliceStreaming(false) waits for admitted mix regions.
    void setWithdrawalPublishedHookForTest(std::function<void()> hook)
    {
        m_withdrawalPublishedHookForTest = std::move(hook);
    }

    /// Test seam — directly set MOX state without going through MoxController.
    /// Bypasses the signal/slot connection that RadioModel wires in Phase L so
    /// unit tests can drive the gate logic without a full radio fixture.
    /// Plan: 3M-1b E.4.
    void setMoxStateForTest(bool active) { setMoxState(active); }

#endif

    // Called by RxDspWorker when a slice produces an RX audio block.
    // samples is interleaved stereo float32, length = frames * 2.
    //
    // Feeds two mixers: m_masterMix for the speakers and m_antiVoxMix for
    // the DEXP cancellation reference. Both are drained here, so anti-VOX
    // hears the same summed audio the operator does. Phase 3F Sub-Epic J
    // Task 9 moved that tap here from RxDspWorker, where it forked slice
    // A's demod output alone; see the note at the bottom of RxDspWorker's
    // drain loop.
    void rxBlockReady(int sliceId, const float* samples, int frames);

    /// TX-monitor block consumer. Called via Qt::DirectConnection from
    /// TxChannel::sip1OutputReady on the audio thread. When monitor is
    /// enabled, expands the mono TXA samples to interleaved stereo (L=R),
    /// applies m_txMonitorVolume, and accumulates into MasterMixer at
    /// kTxMonitorSlotId so the user hears themselves through speakers.
    /// When disabled, no-op.
    ///
    /// **DirectConnection ONLY.** The samples pointer is valid only for
    /// the duration of this synchronous call. Does not queue, store, or
    /// allocate.
    ///
    /// Atomic contract: m_txMonitorEnabled and m_txMonitorVolume are both
    /// loaded with std::memory_order_acquire (same acq/rel pairing as
    /// rxBlockReady's master-volume and mute loads).
    ///
    /// Plan: 3M-1b E.3. Pre-code review §4.3 + §4.4.
    void txMonitorBlockReady(const float* samples, int frames);

    // Pull TX-mic audio samples from the bound TX-input bus.
    //
    // Drains m_txInputBus->pull(...), converts the raw byte buffer to
    // float32 mono samples, and writes up to `n` samples to `dst`.
    // Returns the number of samples actually written; returns 0 if
    // m_txInputBus is null (mic not configured), dst is null, n <= 0,
    // or if the bus has no data ready.
    //
    // Threading (Phase 3M-1c TX pump architecture redesign):
    //   Called from TxWorkerThread::onPumpTick at ~5 ms cadence.  The
    //   underlying m_txInputBus uses a lock-free SPSC ring, so this
    //   method does not block; the bus's audio-callback producer thread
    //   (e.g., PortAudio's HAL callback) and the TxWorkerThread consumer
    //   are the SPSC pair.
    //
    //   The legacy D.1 720-sample accumulator + micBlockReady signal +
    //   clearMicBuffer were removed in the TX pump architecture redesign;
    //   pullTxMic is now a pure drain with no accumulator side effects.
    //
    // Format conversion contract:
    //   - If the bus negotiated format is Int16 (typical mic device),
    //     each Int16 sample is normalised to float32 by dividing by
    //     32768.0f. Only the left channel (channel 0) is used; the
    //     right channel (if stereo) is discarded, producing mono output.
    //   - If the bus negotiated format is Float32, the left channel is
    //     taken directly. Multichannel buses discard all but channel 0.
    //   - Other sample formats (Int24, Int32) are unsupported; returns 0.
    //
    // Caller (TxWorkerThread::onPumpTick) is responsible for resampling
    // if the bus rate doesn't match the TXA DSP rate.  In 3M-1c, both
    // are 48 kHz so no resample needed.
    //
    // Plan: 3M-1b E.1 (initial introduction); 3M-1c TX pump architecture
    // redesign (removal of accumulator side effects).
    int pullTxMic(float* dst, int n);

    // Pull VAX-TX audio samples from the VAX TX shared-memory bus.
    //
    // This is the consumer side of the VAX TX route: 3rd-party apps
    // write audio to the "NereusSDR TX" CoreAudio device on macOS (or
    // the equivalent virtual sink on Linux/PipeWire); the HAL plugin
    // captures it into /nereussdr-vax-tx shared memory; this accessor
    // pulls from that shm.
    //
    // Closes the long-standing TODO at AudioEngine.cpp:306 ("pull TX
    // audio from m_vaxTxBus when [...] consumer that pulls from
    // m_vaxTxBus / mic lives — Sub-Phase 9").
    //
    // Drains m_vaxTxBus->pull(...), downmixes interleaved stereo
    // float32 → mono float32 by averaging L+R, and writes up to `n`
    // samples to `dst`. Returns the number of mono samples written;
    // returns 0 if m_vaxTxBus is null, not open, dst is null, n <= 0,
    // or no data is ready.
    //
    // Format contract: VAX TX shm is fixed at 48 kHz stereo float32 by
    // the plugin↔CoreAudioHalBus contract (hal-plugin/NereusSDRVAX.cpp
    // makePCMFormat + CoreAudioHalBus negotiated format); other
    // formats are not expected and treated as "no data".
    //
    // Threading: called from the WDSP audio thread (via
    // VaxTxMicSource::pullSamples → CompositeTxMicRouter dispatch).
    // Audio-thread-safe: m_vaxTxBus->pull is lock-free (POSIX shm
    // ring); a thread_local scratch buffer absorbs the stereo source
    // bytes so we do not allocate per call.
    int pullVaxTxMic(float* dst, int n);

    /// Returns the operator's PC-mic source selection independently of
    /// whether the capture bus currently exists or is open. Worker routing
    /// uses this fail-silent intent query so capture loss cannot expose the
    /// already-drained radio mic block to WDSP, RADE, or VOX.
    bool isPcMicSelected() const noexcept;

    /// Phase 3M-1c TX pump v3 — PC mic override gate.
    ///
    /// Returns true when PC mic is selected and capture is ready. Gated by:
    ///   1. m_micSourceWantsPc (true iff TransmitModel::micSource ==
    ///      MicSource::Pc; updated by onMicSourceChanged()).
    ///   2. the TX input (the capture reader, or an injected test bus) is
    ///      open; the capture reader is open only while capture is Ready.
    ///
    /// Both conditions must be true. Existing callers that need the combined
    /// readiness predicate retain this API; worker source routing uses
    /// isPcMicSelected() so unavailable input cannot change the chosen source.
    bool isPcMicOverrideActive() const noexcept;

    /// Phase VAX-TX (eager-borg-d64bed, 2026-05-06) — VAX mic override gate.
    ///
    /// Returns true when the worker should overlay VAX TX samples (audio
    /// routed by a 3rd-party app to "NereusSDR TX") on top of the radio
    /// mic samples in m_in.  Gated by:
    ///   1. m_micSourceWantsVax (true iff TransmitModel::micSource ==
    ///      MicSource::Vax; updated by onMicSourceChanged()).
    ///   2. m_vaxTxBus exists and is open.
    ///
    /// Mutually exclusive with isPcMicOverrideActive() at the source-
    /// selector level — TransmitModel::micSource is a single enum value,
    /// so only one of the two flags is ever true at a time.  The worker
    /// checks the VAX gate first since selecting MicSource::Vax means
    /// the user explicitly chose VAX over PC mic.
    bool isVaxMicOverrideActive() const noexcept;

    // Master volume (0.0–1.0). Read on the DSP thread, written on the
    // main thread. Preserves the existing AF-gain wiring in
    // RadioModel::wireSliceSignals.
    void setVolume(float volume);
    float volume() const { return m_masterVolume.load(std::memory_order_acquire); }

    // Master mute. Read on the DSP thread, written on the main thread.
    // Gates the speakers push in rxBlockReady ONLY — VAX taps run
    // regardless (per-channel VAX mute is owned by the VAX applet, not
    // AudioEngine). Sub-Phase 10 Task 10a; persistence + menu-bar
    // MasterOutputWidget wiring land in Task 10b.
    void setMasterMuted(bool muted);
    bool masterMuted() const { return m_masterMuted.load(std::memory_order_acquire); }

    /// Update the cross-thread MOX-state mirror used by rxBlockReady.
    /// Wired by RadioModel (Phase L) to MoxController::moxStateChanged via
    /// signal/slot (Qt::DirectConnection, audio thread).
    ///
    /// Audio-thread reads via std::atomic<bool> with acquire ordering;
    /// main-thread writes via this setter with release ordering.
    ///
    /// Matches Thetis IVAC mox state-machine in audio.cs:349-384
    /// [v2.10.3.13]: when MOX is on, the active TX slice's RX audio is
    /// silenced; non-active slices keep playing.
    ///
    /// Plan: 3M-1b E.4. Pre-code review §10.3 + §10.4.
    void setMoxState(bool active);
    bool moxState() const { return m_moxActive.load(std::memory_order_acquire); }

    /// Active/listening focus does not alter the stable TX-bound identity
    /// captured at key-down. Retained as the existing signal target; no-op.
    void onActiveSliceChanged();

    /// TX monitor (MON) enable. When true, TXA siphon audio is mixed into
    /// the master output during MOX (the user hears themselves).
    ///
    /// Atomic: written by main thread via this setter, read by audio thread
    /// in E.3's txMonitorBlockReady slot. Idempotent: skips signal emit if
    /// value unchanged.
    ///
    /// Default false (mon off at startup per plan §0 row 9).
    ///
    /// Plan: 3M-1b E.2. Pre-code review §4.4.
    void setTxMonitorEnabled(bool enabled);
    bool txMonitorEnabled() const { return m_txMonitorEnabled.load(std::memory_order_acquire); }

    /// TX monitor volume (0.0..1.0). Atomic; clamped on set.
    ///
    /// Default 0.5f (matches Thetis fixed mix coefficient at audio.cs:417;
    /// see pre-code review §12.5).
    ///
    /// Plan: 3M-1b E.2. Pre-code review §4.4.
    void setTxMonitorVolume(float volume);
    float txMonitorVolume() const { return m_txMonitorVolume.load(std::memory_order_acquire); }

    /// R-R3-45: where MON plays, the speakers (default, as before) or the
    /// headphones. Main thread: stores the atomic the audio thread reads in
    /// txMonitorBlockReady, saves audio/TxMonitor/Output ("Speakers" or
    /// "Headphones"), and emits txMonitorOutputChanged on change. The mixer
    /// crossfades the monitor from one output to the other over its ramp,
    /// so a change while transmitting neither gaps nor doubles.
    ///
    /// With the headphones chosen and no headphones output open, MON is
    /// heard nowhere; the control beside MON says why (headphonesAvailable).
    ///
    /// On the Core this setter decides where MON plays on the Core's own
    /// outputs. A remote device's choice never calls it: remote-window
    /// parity Task 32 routes MON into that device's own owner mix
    /// (setOwnerMixMonitor) instead.
    void setTxMonitorOutput(TxMonitorOutput output);

    /// Remote-window parity Task 32 (JJ's MON ruling, 2026-09-26): whether
    /// MON plays on this computer's own outputs (the speakers or headphones
    /// bus, and the master and headphones-mix taps). True by default, as
    /// before; the Core sets it false while a remote device holds
    /// transmit, whose MON plays only on that device. Any thread; the audio
    /// thread reads it at the next drain.
    void setTxMonitorLocal(bool local) { m_txMonitorLocal.store(local, std::memory_order_release); }
    bool txMonitorLocal() const { return m_txMonitorLocal.load(std::memory_order_acquire); }
    TxMonitorOutput txMonitorOutput() const
    {
        return m_txMonitorToHeadphones.load(std::memory_order_acquire)
            ? TxMonitorOutput::Headphones
            : TxMonitorOutput::Speakers;
    }

    // Per-channel VAX controls (Sub-Phase 9 Task 9.2a). Main-thread writes,
    // audio-thread reads, via std::atomic — matches the setVolume /
    // m_masterVolume handshake. `channel` is 1..4; out-of-range calls are
    // silent no-ops. setVaxRxGain clamps to [0.0, 1.0] before comparing
    // against the prior value; change-signals fire only when the value
    // actually changes. setVaxTxGain is storage + signal only — applying it
    // on the TX pull side lives in Phase 3M (see TODO next to m_vaxTxGain).
    void setVaxRxGain(int channel, float gain);
    void setVaxMuted(int channel, bool muted);
    void setVaxTxGain(float gain);

    // Sub-Phase 12 Task 12.4 — DSP rate / block-size persistence.
    // Persists audio/DspRate and audio/DspBlockSize. Live-apply to the
    // WDSP channel pipeline is deferred until the channel-rebuild
    // infrastructure lands; the setter logs and marks a deferred apply.
    // TODO(sub-phase-12-dsp-live-apply): delegate to WdspEngine once
    // channel teardown/rebuild infrastructure is available.
    void setDspSampleRate(int rate);
    void setDspBlockSize(int blockSize);

    // R-R3-45 fix wave: owner thread. Grows the DSP thread's mix scratch
    // (speakers, headphones, program, anti-VOX and VAX sums) to hold
    // `frames` frames,
    // while no block is being mixed; never shrinks. The constructor sizes
    // it to kMixScratchMinFrames, setDspBlockSize and start() to the DSP
    // block size.
    static constexpr int kMixScratchMinFrames = 4096;
    void ensureMixScratchFrames(int frames);
    int mixScratchFrames() const { return m_mixScratchFrames.load(std::memory_order_acquire); }

    // Sub-Phase 12 Task 12.4 — Reset all audio settings (addendum §2.5).
    // Clears all audio/* keys from AppSettings, preserving
    // slice/<N>/VaxChannel and tx/OwnerSlot. Then rebuilds buses from
    // seeded defaults and emits the config-changed signal cascade so
    // subscribed UIs refresh.
    // In a remote window, clear this computer's audio choices only; the
    // Core's DSP settings are outside this engine's ownership.
    void resetAudioSettings(bool operatorLocalOnly = false);

    float vaxRxGain(int channel) const;
    bool  vaxMuted(int channel) const;
    float vaxTxGain() const { return m_vaxTxGain.load(std::memory_order_acquire); }

    // Meter readouts for VaxApplet. Safe when the slot is empty / not open:
    // returns 0.0f so the UI can still bind and show a quiet meter.
    float vaxRxLevel(int channel) const;
    float vaxTxLevel() const;

    // Peak input level (0.0–1.0 normalized) from the PC Mic capture input
    // (the capture reader, or an injected test bus). Used by
    // AudioTxInputPage's Test Mic VU bar (I.2). Returns 0.0f when capture
    // is not Ready. Safe to call
    // from the main thread — reads std::atomic<float> in IAudioBus.
    float pcMicInputLevel() const;

    // True when the VAX slot's IAudioBus has been minted AND its open() call
    // succeeded. False when the slot is empty (pre-start, user-disabled via
    // setVaxEnabled(false)) OR when makeVaxBus() / makeBus() failed to open
    // the underlying device (CoreAudio HAL not loaded, shm mmap failed,
    // PortAudio cable unplugged mid-session, etc.).  Drives the Setup →
    // Audio → VAX card banner so users see an amber "unavailable" state
    // rather than a false-positive green "bound" when the route is broken.
    bool isVaxBusOpen(int channel) const;

#if defined(Q_OS_LINUX)
    LinuxAudioBackend linuxBackend() const { return m_linuxBackend; }

    // Re-runs detection and re-emits linuxBackendChanged if the result
    // differs from the cached value. Does not tear down existing audio
    // buses — caller (MainWindow's Rescan button) is responsible for
    // that if they want a live-switch.
    void rescanLinuxBackend();
#endif

public slots:
    /// Phase 3M-1c TX pump v3 — slot wired by RadioModel to
    /// TransmitModel::micSourceChanged.  Updates m_micSourceWantsPc.
    /// `selectedSourceIsPc == true` means the user picked PC mic.
    void onMicSourceChanged(bool selectedSourceIsPc);

    /// Phase VAX-TX — companion to onMicSourceChanged for the VAX
    /// override gate.  RadioModel's micSourceChanged lambda calls
    /// both onMicSourceChanged(src == Pc) and onMicSourceChangedVax(src == Vax)
    /// so the two gates are kept in sync without a public enum-shaped
    /// API change.
    void onMicSourceChangedVax(bool selectedSourceIsVax);

signals:
    // Emitted when the audio pipeline health state changes.
    // Drives the ConnectionSegment ♪ pip status indicator (sub-PR-2 B.4).
    // Production wiring of QAudioSink::stateChanged → setFlowState
    // lands with the segment integration in sub-PR-4 / D.2.
    void flowStateChanged(NereusSDR::AudioEngine::FlowState state);

    /// One mixed anti-VOX reference block, interleaved stereo float32,
    /// length = frames * 2. Emitted from rxBlockReady on the DSP thread,
    /// once per audio period, whenever the anti-VOX mixer's readiness
    /// barrier releases a block.
    ///
    /// **DirectConnection ONLY.** `samples` points at the engine's
    /// scratch buffer that is valid for the duration of this synchronous
    /// emit and is overwritten by the next block. A queued connection would
    /// copy the pointer, not the audio, and hand the consumer a buffer that
    /// has already moved on. Same contract as txMonitorBlockReady's
    /// incoming samples.
    ///
    /// Phase 3F Sub-Epic J Task 9. The consumer is
    /// TxWorkerThread::onAntiVoxBlockReady, wired DirectConnection in
    /// RadioModel::wireConnectionSignals. That slot exists precisely to
    /// honour the contract above: it copies on the DSP thread while the
    /// pointer is live, then does its own owned, queued hop to reach WDSP
    /// DEXP. Any future consumer owes the same discipline. `const float*`
    /// is not a registered metatype, so a queued connect fails loudly at
    /// connect time rather than dangling, but do not rely on that as the
    /// safety net.
    void antiVoxBlockReady(const float* samples, int frames);

    void volumeChanged(float volume);
    void masterMutedChanged(bool muted);
    // Plan: 3M-1b E.2. Pre-code review §4.4.
    void txMonitorEnabledChanged(bool enabled);
    void txMonitorVolumeChanged(float volume);
    // R-R3-45: the MON output choice changed.
    void txMonitorOutputChanged(NereusSDR::TxMonitorOutput output);
    void vaxRxGainChanged(int channel, float gain);
    void vaxMutedChanged(int channel, bool muted);
    void vaxTxGainChanged(float gain);
    /// R-R3-21 (R3 unfinished controls fix wave M3): isVaxBusOpen(channel)
    /// may have changed (setVaxEnabled, setVaxConfig, openVaxOutputs,
    /// resetAudioSettings, stop). A container's VAX button and Setup >
    /// Audio > VAX's channel card follow it at once.
    void vaxBusOpenChanged(int channel);

    // (Phase 3M-1c D.1 added a micBlockReady(const float*, int) signal
    //  that fired on every kMicBlockFrames=720-sample accumulator block.
    //  The TX pump architecture redesign (2026-04-29) removed the signal
    //  and the accumulator entirely.  TX pump moved to TxWorkerThread,
    //  which calls pullTxMic directly without an intermediate signal.
    //  See plan §5.2 for the rationale.)

    // Sub-Phase 12 Task 12.4 — DSP parameter and audio-reset signals.
    void dspSampleRateChanged(int rate);
    void dspBlockSizeChanged(int blockSize);
    void audioSettingsReset();

    // Sub-Phase 12 Task 12.2 — per-endpoint config-changed signals.
    // Each carries the AudioDeviceConfig the engine actually negotiated
    // after opening the bus (or the last-good config if the open failed).
    // DeviceCard's "Negotiated" pill subscribes to these.
    void speakersConfigChanged(NereusSDR::AudioDeviceConfig cfg);
    void headphonesConfigChanged(NereusSDR::AudioDeviceConfig cfg);
    // R-R3-45: the headphones output opened or closed.
    void headphonesAvailableChanged(bool available);
    // R-R3-45 fix wave: the headphones card's Enabled box changed.
    void headphonesEnabledChanged(bool enabled);
    void txInputConfigChanged(NereusSDR::AudioDeviceConfig cfg);
    // R-R3-36: re-emits CaptureSupervisor::statusChanged on the owner thread.
    void captureStatusChanged(const NereusSDR::CaptureSupervisor::Status& status);
    void vaxConfigChanged(int channel, NereusSDR::AudioDeviceConfig cfg);

#if defined(Q_OS_LINUX)
    void linuxBackendChanged(LinuxAudioBackend oldBackend,
                             LinuxAudioBackend newBackend);
#endif

private:
    // Slot ID for the TX-monitor channel in MasterMixer. Negative so it
    // cannot collide with any non-negative RX slice ID. -1 is avoided as
    // a common "invalid" sentinel; -2 is used here.
    // Plan: 3M-1b E.3. Pre-code review §4.3.
    static constexpr int kTxMonitorSlotId = -2;

    // Sub-Phase 12: speakers-bus rebuild (called directly from setSpeakersConfig).
    void applySpeakersConfig(const AudioDeviceConfig& cfg);

    // Translate AudioDeviceConfig → AudioFormat + PortAudioConfig and
    // open the given bus slot. Used for the speakers / TX-mic / Windows-BYO
    // VAX paths; the platform-native VAX RX/TX virtual buses are minted via
    // makeVaxBus() / makeVaxTxBus() instead.
    std::unique_ptr<IAudioBus> makeBus(const AudioDeviceConfig& cfg,
                                       bool capture);

    // Sub-Phase 8.5: construct + open the platform-native VAX RX bus for
    // `channel` (1..4). macOS → CoreAudioHalBus(Role::VaxN). Linux →
    // PipeWireBus(Role::VaxN) when backend is PipeWire, LinuxPipeBus(Role::VaxN)
    // for Pactl, nullptr for None. Windows → returns nullptr; Windows BYO
    // wiring lands in Sub-Phase 9 via setVaxConfig().
    std::unique_ptr<IAudioBus> makeVaxBus(int channel);

    // R-R3-44: start()'s and openVaxOutputs()'s loop over VAX 1..4.
    void openVaxOutputSlots();

    // Sub-Phase 8.5: construct + open the platform-native VAX TX virtual
    // bus. Opened so coreaudiod / pactl register the virtual TX device for
    // 3rd-party apps. Pulled from in Phase 3M when txOwnerSlot() != MicDirect.
    std::unique_ptr<IAudioBus> makeVaxTxBus();

    // Task 14: split output stream factories (Linux PipeWire path; other
    // platforms return nullptr pending later sub-phase wiring).
    // sourceNode / targetNode — PipeWire node.name to bind to; empty = default
    // routing decided by PipeWire policy.
    std::unique_ptr<IAudioBus> makeTxInputBus(const QString& sourceNode = {});
    std::unique_ptr<IAudioBus> makePrimaryOut(const QString& targetNode = {});
    std::unique_ptr<IAudioBus> makeSidetoneOut(const QString& targetNode = {});
    std::unique_ptr<IAudioBus> makeMonitorOut(const QString& targetNode = {});

    // Open m_speakersBus with a sensible platform default if nothing
    // has been wired by the time start() runs. Keeps the live RX path
    // audible without a Setup→Audio→Devices UI in Sub-Phase 4.
    void ensureSpeakersOpen();

    // R-R3-36: (re)creates m_captureSupervisor from options, forwards its
    // status and applies m_txInputConfig. Never opens capture by itself.
    void installCaptureSupervisor(CaptureSupervisor::Options options);

    // The input the TX path reads: an injected test bus when present,
    // otherwise the capture supervisor's stable reader.
    IAudioBus* txInputSource() const noexcept;

    RadioModel* m_radio{nullptr};

    // R-R3-49: setSliceAudioView's packed words, one per slice id. Written
    // on the main thread, read on the audio thread; lock-free, and they
    // live as long as the engine, so no block can outlive them.
    // Slice control plan Task 6: 64 bits, the AF level's float in the high
    // half.
    std::array<std::atomic<quint64>, kMaxSliceAudioViews> m_sliceAudioViews{};

    // Sub-Phase 12 Task 12.2 — live-reconfig safety mutex for the speakers
    // bus. setSpeakersConfig() acquires this during tear-down + rebuild.
    // rxBlockReady() uses try_lock and drops the block if it can't acquire
    // (≤1 ms of silence is inaudible vs. a use-after-free). NOT held in the
    // audio callback path (acquires try_lock only; never blocks).
    std::mutex m_speakersBusMutex;

    std::unique_ptr<IAudioBus> m_speakersBus;
    bool m_remotePlayback{false}; // protected by m_speakersBusMutex
    AudioFormat m_remotePlaybackFormat; // protected by m_speakersBusMutex
    // R-R3-45: the headphones output. Replaced only on the owner thread
    // under m_headphonesBusMutex; the DSP thread's push takes it with
    // try_lock and drops the block rather than wait, like the speakers.
    std::mutex m_headphonesBusMutex;
    std::unique_ptr<IAudioBus> m_headphonesBus;
    AudioDeviceConfig m_headphonesConfig;  // owner thread
    bool m_headphonesEnabled{false};       // owner thread
    bool m_headphonesAvailable{false};     // owner thread
    // R-R3-45: remote playback on the headphones; protected by
    // m_headphonesBusMutex, as m_remotePlayback is by the speakers'.
    bool m_remoteHeadphonesPlayback{false};
    AudioFormat m_remoteHeadphonesFormat;
    bool beginRemoteHeadphonesPlayback(QString* error);
    // Test-injected TX input only (setTxInputBusForTest); production reads
    // the capture supervisor's reader. R-R3-36.
    std::unique_ptr<IAudioBus> m_txInputBus;

    // R-R3-36: the persisted audio/TxInput selection and the supervisor of
    // the capture helper. Created in the constructor; lives until the
    // engine is destroyed, so its reader pointer stays valid for the TX
    // worker across start()/stop().
    AudioDeviceConfig m_txInputConfig;
    std::unique_ptr<CaptureSupervisor> m_captureSupervisor;

    // (Phase 3M-1c D.1 added a kMicBlockFrames=720-sample mic-block
    //  accumulator + clearMicBuffer + bench-fix-A pumpMic timer.  The
    //  TX pump architecture redesign (2026-04-29) removed all of them.
    //  Pump now lives in src/core/TxWorkerThread.{h,cpp}, which calls
    //  pullTxMic directly at ~5 ms cadence.  See plan §5.2.)

    // Sub-Phase 8.5: platform-native VAX TX virtual bus. Distinct from
    // m_txInputBus, which is the OS mic-capture device owned by MicDirect.
    // Opened in start(), reset in stop(); consumption is a Phase 3M concern.
    std::unique_ptr<IAudioBus> m_vaxTxBus;
    std::array<std::unique_ptr<IAudioBus>, 4> m_vaxBus;
    // R-R3-44: taken by the owner thread whenever it replaces a VAX output
    // and by a remote window's VAX feeder around every pacing read and
    // write. The local VAX tee on the DSP thread only try-locks it, and
    // skips the push when it is held (RD-I10).
    std::array<std::mutex, 4> m_vaxBusMutex;
    // R-R3-44: see setVaxOutputsAllowed().
    // iPhone app Task 73: see setVaxSliceMask().
    std::atomic<quint32> m_vaxSliceMask{0xFFFFFFFFu};
    bool m_vaxOutputsAllowed{true};
#ifdef NEREUS_BUILD_TESTS
    std::function<std::unique_ptr<IAudioBus>(int)> m_vaxBusFactoryForTest;
    DeviceBusFactory m_deviceBusFactoryForTest;
#endif
    MasterMixer m_masterMix;

    // Second mixer whose output is the anti-VOX reference, not the speakers.
    //
    // Thetis runs exactly this: a per-transmitter aamix instance
    // (pcm->xmtr[i].pavoxmix, cmaster.c:159-175 [v2.10.3.15]) fed by every
    // sub-receiver in the same loop that feeds the speakers mix
    // (cmaster.c:371-372 [v2.10.3.15]), with membership managed explicitly
    // through SetAAudioMixStates (cmaster.c:584-588 [v2.10.3.15]), the same
    // call the RX mixer uses. `active = 0` in that create_aamix call is the
    // pre-power-on default and nothing more: console.cs:27650-27771
    // [v2.10.3.15] sets the membership mask on every power and MOX
    // transition. So this instance is barrier-paced exactly like
    // m_masterMix, and its membership rides the same setSliceStreaming
    // calls.
    //
    // The TX monitor is deliberately never registered here. Upstream draws
    // this mask from RX1 + RX1S + RX2 while handing the speakers mixer
    // RX1 + RX1S + RX2 + MON on the line above it (console.cs:27650-27651
    // [v2.10.3.15]), and monitor audio suppressing the operator's own VOX
    // would be feedback by definition.
    MasterMixer m_antiVoxMix;

    // R-R3-44 (fix wave): the local VAX tee's per-channel mix. Slices that
    // share a VAX channel are summed into one block per period instead of
    // each pushing its own. Membership rides the same setSliceStreaming
    // calls as the two mixers above.
    VaxChannelMixer m_vaxMix;

    // R-R3-23 (fix wave): the master mix converted to the speaker device's
    // own rate and channel count before the push. Configured wherever the
    // speakers bus is replaced, under m_speakersBusMutex, which the DSP
    // thread's push also holds.
    SpeakerFormatConverter m_speakersConverter;
    void configureSpeakersConverter();

    // R-R3-45: the same conversion for the headphones device, configured
    // under m_headphonesBusMutex wherever the headphones bus is replaced.
    SpeakerFormatConverter m_headphonesConverter;
    void configureHeadphonesConverter();
    // Owner thread: (re)open or close the headphones output from
    // m_headphonesConfig and m_headphonesEnabled, then publish.
    void reopenHeadphones();
    void publishHeadphonesAvailable();

    // Control-to-audio withdrawal handshake. The audio thread never waits:
    // it either enters a region or drops a block while admission is closed.
    // The control thread invalidates both mixers, then waits for already
    // admitted regions to finish before withdrawal returns.
    std::atomic<bool> m_mixAdmissionClosed{false};
    std::atomic<unsigned> m_mixRegionsInFlight{0};
    // R-R3-45 fix wave: rxBlockReady's mix scratch; see
    // ensureMixScratchFrames(). Replaced only behind the gate above.
    std::vector<float> m_mixScratch;
    std::vector<float> m_hpMixScratch;
    std::vector<float> m_programScratch;
    std::vector<float> m_radioOutScratch; // the radio's speaker out (radio codec)
    std::vector<float> m_avMixScratch;   // anti-VOX reference
    std::vector<float> m_vaxScratch;     // one VAX channel's mix
    std::atomic<int> m_mixScratchFrames{0};

    // One receive-only remote-media observer.  Its lifetime is owned by the
    // caller; set/clear establish the quiescent boundary before the raw
    // pointer changes.  Keep this separate from the mixer admission gate:
    // changing media lifecycle must never suppress the local speaker mix.
    std::atomic<MasterMixAudioTap*> m_masterMixAudioTap{nullptr};
    std::atomic<bool> m_masterMixTapAdmissionClosed{false};
    std::atomic<unsigned> m_masterMixTapCallsInFlight{0};
    std::mutex m_masterMixTapControlMutex;
    // R-R3-45: set with the tap, while its gate is closed and drained.
    std::atomic<bool> m_masterMixTapSpeakersOnly{false};
    // R-R3-45: the headphones-mix tap, gated exactly as the master tap.
    std::atomic<MasterMixAudioTap*> m_headphonesMixAudioTap{nullptr};
    std::atomic<bool> m_headphonesMixTapAdmissionClosed{false};
    std::atomic<unsigned> m_headphonesMixTapCallsInFlight{0};
    std::mutex m_headphonesMixTapControlMutex;

    // R-R3-43 receiver taps: the master tap's gate, one per slot. sliceId
    // is -1 while the slot is free; it and the tap change only while the
    // slot's gate is closed and drained.
    struct SliceTapSlot {
        std::atomic<int> sliceId{-1};
        std::atomic<SliceAudioTap*> tap{nullptr};
        std::atomic<bool> admissionClosed{false};
        std::atomic<unsigned> callsInFlight{0};
    };
    std::array<SliceTapSlot, kMaxSliceAudioTaps> m_sliceTaps;
    std::mutex m_sliceTapControlMutex;

    // iPhone app Task 76: the owner mixes. Each tap has the master tap's
    // gate. `taken` and the mask are written on the control thread (under
    // m_ownerMixControlMutex); the DSP thread reads them without a lock.
    struct MixTapGate {
        std::atomic<MasterMixAudioTap*> tap{nullptr};
        std::atomic<bool> admissionClosed{false};
        std::atomic<unsigned> callsInFlight{0};
    };
    struct OwnerMixSlot {
        std::atomic<bool> taken{false};
        std::atomic<quint32> sliceMask{0};
        MixTapGate program;
        std::atomic<bool> programSpeakersOnly{false};
        MixTapGate headphones;
        // Task 32: MasterMixer::OwnerMonitor, as an int for the atomic.
        std::atomic<int> monitor{0};
        // Slice control plan Task 6: the slices this owner listens to, and
        // its level for each (0 when muted).
        std::atomic<quint32> listenMask{0};
        std::array<std::atomic<float>, kMaxSliceAudioViews> listenLevels{};
    };
    std::array<OwnerMixSlot, kMaxOwnerMixes> m_ownerMixes;
    mutable std::mutex m_ownerMixControlMutex;
    std::atomic<quint32> m_localOutputSliceMask{0xFFFFFFFFu};
    // Slice control plan Task 6: the local output's listening, as an
    // owner's.
    std::atomic<quint32> m_localListenMask{0};
    std::array<std::atomic<float>, kMaxSliceAudioViews> m_localListenLevels{};
    // Each owner's two sums and one program scratch, sized with the mix
    // scratch (ensureMixScratchFrames), never on the DSP thread.
    std::array<std::vector<float>, kMaxOwnerMixes> m_ownerSpeakersScratch;
    std::array<std::vector<float>, kMaxOwnerMixes> m_ownerHeadphonesScratch;
    // Radio codec (2026-09-30): the radio output tap (setRadioOutputTap),
    // on the owner mixes' gate. m_radioOutputControlMutex serialises set
    // and clear.
    MixTapGate m_radioOutputTap;
    std::mutex m_radioOutputControlMutex;
    static void closeAndDrainMixTap(MixTapGate& gate);
    static void invokeMixTap(MixTapGate& gate, const float* samples, int frames) noexcept;
    bool validOwnerMixSlot(int slot) const
    {
        return slot >= 0 && slot < kMaxOwnerMixes;
    }
    void closeAndDrainSliceTap(SliceTapSlot& slot);
    // DSP thread, from rxBlockReady only: drain both mixers, run the taps
    // and push the outputs. Remote-window parity Task 32 split it out so
    // the MOX-gated slice's call drains too, with `monitorOnly`: then only
    // while no slice is a barrier member (MON alone is queued), and never
    // the anti-VOX mixer, which MON is not in.
    void drainMixes(int frames, bool monitorOnly = false);
    // DSP thread. Hands each tap for `sliceId` the block as the slice's
    // receiver produced it: the AF level is applied in the mix, after the
    // taps (slice control plan Task 6).
    void feedSliceTaps(int sliceId, const float* samples, int frames) noexcept;
    void skipSliceTaps(int sliceId, int frames) noexcept;

#ifdef NEREUS_BUILD_TESTS
    std::function<void()> m_withdrawalPublishedHookForTest;
    std::function<void(AudioEngine&)> m_startInitializerForTest;
#endif

    // Speakers format last negotiated. frames passed to rxBlockReady may
    // vary per block; the bus handles that internally via its ring. Kept
    // for diagnostics.
    AudioFormat m_speakersFormat;

    // Written by setVolume() on the UI thread, read by rxBlockReady() on
    // the DSP thread. Atomic for the cross-thread handshake per
    // CLAUDE.md C++ style guide.
    std::atomic<float> m_masterVolume{0.5f};

    // Written by setMasterMuted() on the UI thread, read by
    // rxBlockReady() on the DSP thread. Same acq_rel / acquire pairing
    // as m_masterVolume above.
    std::atomic<bool> m_masterMuted{false};

    // Plan: 3M-1b E.4. Pre-code review §10.3 + §10.4.
    // Cross-thread MOX-state mirror. Written by main-thread setMoxState()
    // (wired by RadioModel in Phase L from MoxController::moxStateChanged).
    // Read by audio-thread rxBlockReady via acquire load; written via
    // release store (same acq/rel pairing as m_masterMuted above).
    // Defaults false (MOX off at startup).
    //
    // Matches Thetis IVAC mox state-machine in audio.cs:349-384 [v2.10.3.13]:
    // when MOX is on, the TX-bound slice's RX audio is silenced; other
    // listening slices keep playing.
    std::atomic<bool> m_moxActive{false};

    // Which slice setMoxState() withdrew from the mixer's readiness barrier
    // on key-down, so key-up re-admits that exact slice. Main thread only.
    // -1 when nothing is withdrawn.
    std::atomic<int> m_moxWithdrawnSlice{-1};

    // Phase 3M-1c TX pump v3 — PC mic override gate.
    // Written by onMicSourceChanged() on the main thread (slot wired
    // by RadioModel to TransmitModel::micSourceChanged).  Read by the
    // worker thread via isPcMicSelected().  Default false matches
    // a fresh radio session before TransmitModel::micSourceChanged
    // fires.  When the radio is HL2 (no mic jack), RadioModel forces
    // micSource=PC via setMicSourceLocked, and the resulting
    // micSourceChanged emit lands here as true.
    std::atomic<bool> m_micSourceWantsPc{false};

    // Phase VAX-TX (eager-borg-d64bed, 2026-05-06) — VAX mic override gate.
    // Written by onMicSourceChangedVax() on the main thread (slot wired by
    // RadioModel to TransmitModel::micSourceChanged).  Read by the worker
    // thread via isVaxMicOverrideActive().  Mutually exclusive with
    // m_micSourceWantsPc at the source-selector level.
    std::atomic<bool> m_micSourceWantsVax{false};

    // Plan: 3M-1b E.2. Pre-code review §4.4.
    // Written by setTxMonitorEnabled() on the main thread, read by the
    // audio thread in E.3's txMonitorBlockReady slot. Same acq_rel /
    // acquire pairing as m_masterMuted above.
    std::atomic<bool>  m_txMonitorEnabled{false};  // default off per plan §0 row 9
    // Default 0.5f — mirrors the fixed coefficient used in Thetis audio.cs
    // for the aaudio mix path; NereusSDR exposes this as user-adjustable
    // volume (pre-code review §4.4). Not a port; AudioEngine is NereusSDR-native.
    std::atomic<float> m_txMonitorVolume{0.5f};
    // R-R3-45: MON on the headphones sum instead of the speakers. Written
    // by setTxMonitorOutput() on the main thread, read by
    // txMonitorBlockReady() on the audio thread.
    std::atomic<bool>  m_txMonitorToHeadphones{false};
    // Task 32: MON on this computer's own outputs (setTxMonitorLocal).
    std::atomic<bool>  m_txMonitorLocal{true};

    // Sub-Phase 9 Task 9.2a — per-channel VAX rx gain / mute and master
    // VAX tx gain. Main-thread writes via set*() setters, DSP-thread
    // reads inside rxBlockReady for the RX path. One atomic per control
    // per channel; defaults are unity-gain and not-muted so a fresh
    // AudioEngine (or one that has never had a setter called) preserves
    // the pre-Sub-Phase-9 passthrough behavior exactly.
    std::array<std::atomic<float>, 4> m_vaxRxGain{{1.0f, 1.0f, 1.0f, 1.0f}};
    std::array<std::atomic<bool>,  4> m_vaxMuted{};
    // TODO(phase3M): apply m_vaxTxGain in TX pull path. Storage-only in
    // Sub-Phase 9 — the consumer that pulls from m_vaxTxBus / mic lives
    // in Phase 3M (TxChannel). Kept here so the VaxApplet tx slider has
    // a setter to bind to today.
    std::atomic<float> m_vaxTxGain{1.0f};

    // Flow-state FSM (sub-PR-2 B.4).
    void setFlowState(FlowState s);
    FlowState m_flowState{FlowState::Dead};
    int       m_successiveUnderruns{0};

    // Was Pa_Initialize() actually successful? Guards the matching
    // Pa_Terminate() in the destructor so unit tests that construct
    // AudioEngine without a real audio subsystem don't hit a spurious
    // terminate.
    bool m_paInitialized{false};
    // The device paths may run: PortAudio came up, or this is a test run,
    // where makeBus supplies only the test's fake devices (R-R3-21).
    bool m_deviceLayerReady{false};
    bool m_running{false};
    // High-water mark of slice ids registered with MasterMixer: ids
    // [0, m_preregisteredSlices) have an entry. Startup-only invariant per
    // design-decision D6 (plan); prevents a main-thread insert/rehash race
    // against the audio thread's lock-free find(). See preregisterSlices().
    int m_preregisteredSlices{0};

#if defined(Q_OS_LINUX)
    // Cached Linux audio backend detected by detectLinuxBackend() in the
    // ctor. Task 14 consults this to dispatch to PipeWireBus vs. the
    // existing LinuxPipeBus (pactl) path. Re-runnable via
    // rescanLinuxBackend() (Setup → Audio Rescan).
    LinuxAudioBackend m_linuxBackend = LinuxAudioBackend::None;
#endif

#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
    // FORWARD CONTRACT #1 — DECLARED LAST. DO NOT MOVE THIS MEMBER EARLIER.
    //
    // C++ destroys class members in REVERSE declaration order. m_pwLoop is
    // declared after all bus members (m_vaxBus, m_speakersBus, m_headphonesBus,
    // m_txInputBus, m_vaxTxBus, m_masterMix) so that on destruction the buses
    // are torn down BEFORE the loop. Each ~PipeWireBus() calls close() →
    // PipeWireStream::close() which takes m_loop->lock(); the loop must still
    // be alive at that point. If m_pwLoop were declared earlier (higher up in
    // the class), its destructor would run first — the loop thread would stop,
    // and then the bus dtors would attempt to take a lock on a destroyed loop,
    // deadlocking or crashing the audio thread.
    //
    // The DSP producer (rxBlockReady) must also have stopped feeding push()
    // before destruction. AudioEngine::stop() handles this; the caller
    // (RadioModel teardown) stops the DSP worker thread before calling stop().
    // See also: PipeWireThreadLoop.cpp:23-30.
    std::unique_ptr<PipeWireThreadLoop> m_pwLoop;
#endif
};

} // namespace NereusSDR
