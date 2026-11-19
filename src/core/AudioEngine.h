#pragma once

// no-port-check: AetherSDR-derived Longpath file; Thetis cmaster.cs /
// audio.cs references in inline cites are behavioral source-first cites
// for sample sizes / timing / mix coefficient parity only, not Thetis
// logic ports.

// =================================================================
// src/core/AudioEngine.h  (Longpath)
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
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
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
// =================================================================

#include "AudioDeviceConfig.h"
#include "IAudioBus.h"
#include "audio/MasterMixer.h"
#include "audio/RxRatenAngleich.h"
#include "strip/MicSpectrum.h"
// NrSlot und der Rauschzweig fuer fremden Ton (2026-10-28).
#include "WdspTypes.h"

#if defined(Q_OS_LINUX)
#  include "core/audio/LinuxAudioBackend.h"
#endif

#if defined(Q_OS_LINUX) && defined(LONGPATH_HAVE_PIPEWIRE)
// Forward-declare only — AudioEngine.h must not drag in libpipewire types.
// The full type is available in AudioEngine.cpp via
// #include "core/audio/PipeWireThreadLoop.h".

namespace Longpath {

class PipeWireThreadLoop;
}
#endif

#include <QObject>
#include <QString>

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <mutex>

namespace Longpath {

class AudioTapRing;
class Resampler;


class RadioModel;
class SliceModel;

// Audio engine for Longpath (Phase 3O VAX).
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
class ExternalAudioNr;   // src/core/ExternalAudioNr.h

class AudioEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

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

    // Abgriff fuer die QSO-Aufnahme. Nicht besitzend; der Audio-Faden
    // liest den Zeiger bei jedem Block neu, damit Abschalten sofort
    // wirkt.
    std::atomic<AudioTapRing*> m_qsoTap{nullptr};
    std::atomic<AudioTapRing*> m_asrTap{nullptr};
    std::atomic<int>           m_asrTapSlice{-1};
    std::atomic<int>           m_qsoTapSlice{-1};
    // Dritter Abgriff, fuer die "off the air"-WAV-Aufnahme (Phase 3M).
    std::atomic<AudioTapRing*> m_wavRecordTap{nullptr};
    std::atomic<int>           m_wavRecordTapSlice{-1};
    // Vierter Abgriff, fuer den nativen RTTY-Decoder (2026-09-06).
    std::atomic<AudioTapRing*> m_rttyTap{nullptr};
    std::atomic<int>           m_rttyTapSlice{-1};
    // Der Abgriff fuer den nativen CW-Decoder (2026-09-21), wie der RTTY-Abgriff.
    std::atomic<AudioTapRing*> m_cwTap{nullptr};
    std::atomic<int>           m_cwTapSlice{-1};
    // Sechster Abgriff, fuer den nativen FT8/FT4-Dekoder (2026-10-15).
    std::atomic<AudioTapRing*> m_ft8Tap{nullptr};
    std::atomic<int>           m_ft8TapSlice{-1};

    // Non-owning back-pointer so rxBlockReady can look up the active
    // SliceModel to read mute / VAX-channel state. Null is safe (unit
    // tests that construct AudioEngine without a RadioModel): rxBlockReady
    // becomes a no-op.
    void setRadioModel(RadioModel* radio);

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
    void setHeadphonesConfig(const AudioDeviceConfig& cfg);
    void setTxInputConfig(const AudioDeviceConfig& cfg);

    /// Drop everything the TX-input (PC mic) ring has hoarded.
    /// (2026-08-11 voice-check bench.) The bus opens eagerly at
    /// connect, seconds before the TX pump takes its first pull, so
    /// the 1-second ring arrives at its first use PINNED FULL — and a
    /// full drop-oldest ring clips its oldest samples on every jitter
    /// burst, forever: a click straight into the mic signal that every
    /// take and every transmission inherits. TxWorkerThread calls this
    /// on the rising edge of the PC-mic splice; flush() is an atomic
    /// cursor equalisation and safe from the audio thread.
    void flushTxInputBus();

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

    // ── Fuer die Tondiagnose (2026-10-22) ────────────────────────────
    //
    // Keine Pruef-Naht, sondern Bedienung: der Dialog unter Help zeigt
    // Geraet, Schnittstelle, Rate und die Aussetzerzaehler.
    //
    // Steht BEWUSST vor `#ifdef LONGPATH_BUILD_TESTS`. Der erste Anlauf
    // legte die beiden neben speakersBusForTest(), also INNERHALB des
    // Pruefbau-Blocks. Lokal war gruen, weil hier mit
    // -DLONGPATH_BUILD_TESTS=ON gebaut wird; der Release-Bau schaltet
    // Pruefstaende ab, und damit verschwanden die Zugriffe mitsamt dem
    // Block. Alle drei Plattformen brachen ab:
    //
    //   error: 'class Longpath::AudioEngine' has no member named
    //          'speakersBus'
    //
    // Dieselbe Falle wie 2026-10-16 bei einem `...ForTest` ohne Waechter.
    const IAudioBus* speakersBus() const { return m_speakersBus.get(); }
    const IAudioBus* txInputBus()  const { return m_txInputBus.get(); }

#ifdef LONGPATH_BUILD_TESTS
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

    /// Test seam (2026-09-23) — liegt gerade ein Lautsprecher-Bus an?
    /// Belegt, dass makeBus() im Pruefmodus KEIN echtes Geraet oeffnet:
    /// nach setSpeakersConfig() steht hier nichts mehr.
    bool hasSpeakersBusForTest() const { return m_speakersBus != nullptr; }

    /// Pruef-Naht (2026-10-04) — welcher Bus liegt gerade an, und einmal
    /// die Pruefung "lebt er noch?" anstossen. Fuer den Fall vom Mac des
    /// Betreibers: Ton im Programm, nichts aus den Lautsprechern, weil ein
    /// toter Strom als "geoeffnet" durchging.
    const IAudioBus* speakersBusForTest() const { return m_speakersBus.get(); }
    void ensureSpeakersOpenForTest() { ensureSpeakersOpen(); }

    // Test seam — inject a fake IAudioBus into the TX-input slot so unit
    // tests can exercise pullTxMic without standing up a real PortAudio
    // capture device. Takes ownership of `bus`.
    // Plan: 3M-1b E.1.
    void setTxInputBusForTest(std::unique_ptr<IAudioBus> bus);

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

    // Test seam (2026-09-06) — arms m_tapWriteDelayHookForTest so a test
    // can hold the simulated audio thread inside the setXxxTap(nullptr,
    // ...) race window (busy already announced, tap pointer already
    // loaded, write() not yet called) and prove the wait in setXxxTap
    // really blocks until that write() has returned. See
    // tst_audio_engine_tap_teardown_race.cpp.
    void setTapWriteDelayHookForTest(std::function<void()> hook)
    {
        m_tapWriteDelayHookForTest = std::move(hook);
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
    /// ── Abgriff fuer die QSO-Aufnahme ────────────────────────────────
    ///
    /// Der Audio-Faden schreibt den demodulierten Ton EINER Scheibe in
    /// einen bereitgestellten Zwischenspeicher; abgeholt wird im
    /// Hauptfaden. Kein Signal, kein Schloss, keine
    /// Speicheranforderung — die drei Dinge, die im Audio-Rueckruf
    /// nichts zu suchen haben.
    ///
    /// Abgegriffen wird VOR MasterMixer und Lautstaerkeregler, also
    /// derselbe Punkt wie der VAX-Abgriff. Eine Aufnahme soll nicht
    /// leiser werden, weil jemand am Lautsprecher dreht.
    ///
    /// `ring` gehoert dem Aufrufer und muss laenger leben als der
    /// Abgriff. Zum Abschalten nullptr uebergeben; danach darf der
    /// Zwischenspeicher weg — der Audio-Faden liest den Zeiger bei
    /// jedem Block neu.
    void setQsoTap(AudioTapRing* ring, int sliceId);

    /// ── Zweiter Abgriff, fuer die Spracherkennung (2026-08-23) ──────
    ///
    /// Genau derselbe Bau wie der QSO-Abgriff, und aus demselben Grund
    /// ein EIGENER: die Aufnahme und die Erkennung sollen gleichzeitig
    /// laufen koennen. Ein geteilter Ring haette einen Leser zu wenig —
    /// wer zuerst liest, nimmt dem anderen die Daten weg.
    ///
    /// Kein Signal, kein Schloss, keine Speicheranforderung im
    /// Tonfaden. `ring` gehoert dem Aufrufer und muss laenger leben als
    /// der Abgriff; zum Abschalten nullptr uebergeben.
    void setAsrTap(AudioTapRing* ring, int sliceId);

    /// ── Dritter Abgriff, fuer die "off the air"-WAV-Aufnahme (Phase 3M) ──
    ///
    /// Genau derselbe Bau wie die beiden anderen Abgriffe, wieder ein
    /// EIGENER Ring: Aufnahme, ASR und Spracherkennung sollen
    /// gleichzeitig laufen koennen, ohne sich beim Lesen zu stoeren.
    ///
    /// Design doc: docs/architecture/phase3m-recording-design.md §7.1.
    ///
    /// Kein Signal, kein Schloss, keine Speicheranforderung im
    /// Tonfaden. `ring` gehoert dem Aufrufer und muss laenger leben als
    /// der Abgriff; zum Abschalten nullptr uebergeben.
    void setWavRecordTap(AudioTapRing* ring, int sliceId);

    /// ── Vierter Abgriff, fuer den nativen RTTY-Decoder (2026-09-06) ──
    ///
    /// Genau derselbe Bau wie die drei anderen Abgriffe, wieder ein
    /// EIGENER Ring aus demselben Grund: der RTTY-Decoder soll neben
    /// Aufnahme/ASR/WAV-Mitschnitt laufen koennen, ohne sich beim Lesen
    /// zu stoeren.
    ///
    /// Design doc: docs/architecture/2026-09-06-rtty-decoder-scoping.md.
    ///
    /// Kein Signal, kein Schloss, keine Speicheranforderung im
    /// Tonfaden. `ring` gehoert dem Aufrufer und muss laenger leben als
    /// der Abgriff; zum Abschalten nullptr uebergeben.
    void setRttyTap(AudioTapRing* ring, int sliceId);
    /// Dasselbe fuer den CW-Decoder (CwDecoderApplet, 2026-09-21): ein
    /// eigener Ring, damit RTTY und CW sich beim Lesen nie stoeren.
    void setCwTap(AudioTapRing* ring, int sliceId);
    /// Und dasselbe fuer den nativen FT8/FT4-Dekoder (Ft8Applet,
    /// 2026-10-15). Wieder ein EIGENER Ring: FT8 liest in langen
    /// Fenstern (15 s), CW und RTTY in kurzen Haeppchen — ein
    /// geteilter Ring haette einen Leser zu wenig.
    void setFt8Tap(AudioTapRing* ring, int sliceId);

    /// ── FT8 auf einem zweiten Band (2026-11-19) ──────────────────────
    ///
    /// Betreiber: „ich will 2 bänder … gleichzeitig empfangen umd ggf.
    /// eine dxcc station zu hören."
    ///
    /// Ein Lauschband hat KEINE Scheibe in Longpath und gehoert NICHT
    /// in den Lautsprecher. Sein Ton (bei der Flex der DAX-Strom der
    /// Lauschscheibe, siehe core/FlexLauschscheibe.h) geht darum nicht
    /// durch rxBlockReady() -- dort laege er im Mix, in jedem anderen
    /// Abgriff und im FT8-Abgriff der Hauptscheibe --, sondern allein
    /// in den Ring seines Platzes. Ein Platz je Lauschband; heute wird
    /// nur Platz 0 benutzt, die FLEX-8400 hat nur zwei Scheiben.
    static constexpr int kFt8LauschPlaetze = 3;
    /// Den Ring eines Lauschplatzes setzen; nullptr schaltet ab und
    /// wartet, bis kein Schreiber mehr am alten Ring haengt.
    void setFt8LauschTap(int platz, AudioTapRing* ring);
    /// Ton eines Lauschbandes: verschraenktes Stereo in `sourceRateHz`,
    /// wird auf 48 kHz gewandelt und NUR in den Ring dieses Platzes
    /// geschrieben.
    void feedFt8LauschAudio(int platz, const float* interleavedStereo,
                            int frames, int sourceRateHz);

    /// Den RX-Driftausgleich beim naechsten Tonblock neu anfangen
    /// lassen. Aufzurufen, wenn eine BEKANNTE Luecke im Empfangston
    /// liegt -- eine eigene Sendung etwa -- damit der Regler sie nicht
    /// als Taktabweichung liest. Siehe m_rxDriftZuruecksetzen.
    void rxDriftNeuAnfangen()
    {
        m_rxDriftZuruecksetzen.store(true, std::memory_order_release);
    }

    void rxBlockReady(int sliceId, const float* samples, int frames);

    // ── KiwiSDR-Ton (Stufe 5, 2026-08-23) ───────────────────────────
    //
    // Ein KiwiSDR liefert fertig entschluesselten Ton mit 24 kHz,
    // verschraenkt Stereo, float32 — nicht I/Q. Er tritt darum NICHT
    // in die DSP-Kette ein, sondern genau dort, wo deren Ergebnis
    // sonst ankommt: an der Spur SEINER Scheibe im MasterMixer.
    //
    // Der Gewinn daraus ist der eigentliche Grund fuer diesen
    // Zuschnitt: Lautstaerke, Stummschaltung, Schwenk, Anti-VOX, der
    // VAX-Abgriff, die QSO-Aufnahme und die MOX-Sperre gelten fuer
    // Kiwi-Ton, ohne dass davon irgendetwas noch einmal geschrieben
    // werden muesste. feedKiwiSdrAudioData tastet um und ruft dann
    // rxBlockReady — mehr ist es nicht.
    //
    // AetherSDR fuehrt dafuer einen eigenen Pfad mit eigener
    // Rauschminderung je Quelle (m_kiwiSdrNr2, m_kiwiSdrRxBuffer und
    // ein Dutzend weiterer Felder). Das ist maechtiger und kostet ein
    // zweites Regelwerk daneben. Wir nehmen zuerst den Weg, bei dem
    // eine Kiwi-Scheibe sich wie jede andere Scheibe verhaelt.
    //
    // ── EINE Bedingung, und sie ist nicht verhandelbar ──────────────
    //
    // Die Spur einer Scheibe hat GENAU EINEN Erzeuger. Speist ein
    // KiwiSDR sie, darf der DDC desselben Geraets es nicht auch tun —
    // sonst schreiben zwei Faeden in denselben Ring. Darum schaltet
    // setKiwiSdrAudioSourceEnabled die Scheibe zugleich auf
    // "opportunistisch": sie ist dann kein Mitglied der Bereitschafts-
    // schranke mehr und kann bei einem Netzaussetzer nicht den ganzen
    // Mix anhalten.
    void feedKiwiSdrAudioData(int sliceId, const QByteArray& pcm24kStereoFloat);
    void setKiwiSdrAudioSourceEnabled(int sliceId, bool enabled);
    void removeKiwiSdrAudioSource(int sliceId);
    bool kiwiSdrAudioEnabled(int sliceId) const;

    // ── SunSDR-Ton (TCI-Client, 2026-08-24) ──────────────────────────
    //
    // Derselbe Zuschnitt wie KiwiSDR oben — ein SunSDR2 QRP liefert
    // ueber ExpertSDR2/TCI ebenfalls fertig entschluesselten RX-Ton,
    // nicht I/Q, und tritt darum genau wie Kiwi-Ton nicht in die
    // DSP-Kette ein, sondern an rxBlockReady.
    //
    // EIN Unterschied zu Kiwi: die Quellrate ist NICHT fest. TCI erlaubt
    // audio_samplerate jederzeit umzustellen (der IQ-Strom lief in der
    // Messung bis 192 kHz, siehe docs/TCI-SunSDR-gemessen.md — der Ton
    // blieb dort zwar durchgaengig bei 48 kHz, aber das Protokoll
    // erzwingt das nicht). feedSunSdrAudioData nimmt die Rate deshalb
    // als Parameter statt sie wie bei Kiwi im Wandler fest anzunehmen;
    // aendert sie sich seit dem letzten Ruf, wird der Wandler verworfen
    // und neu angelegt — sonst traegt der alte Wandler einen
    // Filterzustand fuer eine Rate, die nicht mehr stimmt, und das
    // Ergebnis knackt beim ersten Block danach.
    //
    // Dieselbe Bedingung wie bei Kiwi gilt unveraendert: die Spur einer
    // Scheibe hat GENAU EINEN Erzeuger. setSunSdrAudioSourceEnabled
    // schaltet die Scheibe darum ebenso auf "opportunistisch".
    void feedSunSdrAudioData(int sliceId, const float* interleavedStereo,
                             int frames, int sourceRateHz);
    void setSunSdrAudioSourceEnabled(int sliceId, bool enabled);
    void removeSunSdrAudioSource(int sliceId);
    bool sunSdrAudioEnabled(int sliceId) const;

    // ── Flex-Ton (VITA-49, Stufe 2, 2026-10-18) ──────────────────────
    //
    // Der dritte fremde Toneingang, und derselbe Zuschnitt wie die zwei
    // darueber: bei Flex rechnet das RADIO, es liefert fertigen Ton und
    // nicht I/Q. Also nicht in die DSP-Kette, sondern an rxBlockReady.
    //
    // Gemessen am 2026-10-18 an einer FLEX-8400
    // (docs/protocols/flex-vita49-gemessen.md §5): Paketklasse 0x03E3,
    // big-endian float32, Stereo verschraenkt, kein Unterkopf, 128
    // Rahmen je Paket — und **24 012,3 Hz** ueber ein 24-Sekunden-
    // Fenster gezaehlt, also 24 kHz auf 0,05 % genau. Die Rate ist
    // trotzdem ein Parameter und keine Annahme im Wandler: sie ist
    // gezaehlt, nicht aus einem Protokollfeld gelesen, und wenn sich
    // das je aendert, soll es EINE Stelle sein.
    //
    // Dieselbe Bedingung wie bei Kiwi und SunSDR: die Spur einer
    // Scheibe hat GENAU EINEN Erzeuger, darum schaltet
    // setFlexAudioSourceEnabled die Scheibe ebenso auf
    // "opportunistisch" — ein Netzaussetzer zum Flex darf nicht den
    // ganzen Mix anhalten.
    void feedFlexAudioData(int sliceId, const float* interleavedStereo,
                           int frames, int sourceRateHz);
    void setFlexAudioSourceEnabled(int sliceId, bool enabled);
    void removeFlexAudioSource(int sliceId);
    bool flexAudioEnabled(int sliceId) const;

    /// Rauschminderung für Ton, der schon demoduliert ankommt
    /// (Flex, Kiwi, TCI) — 2026-10-28.
    ///
    /// Der Betreiber: „aber jeder nr sollte anders funktionieren! das
    /// ist bei anvelina ja schon!" Stimmt: dort laufen vier
    /// WDSP-Verfahren. Fremder Ton läuft nie durch WDSP, also gab es
    /// dort gar keines. Diese Stufe legt die beiden Verfahren, die auf
    /// TON arbeiten können (NR3 rnnoise, MNR Apple), hinter den
    /// Wandler auf 48 kHz. NR1/NR2/NR4 sind WDSP-Einheiten und bleiben
    /// draussen — siehe ExternalAudioNr.h.
    ///
    /// `amount` 0..1. Ein Steckplatz ohne Gegenstück schaltet die
    /// Stufe ab; dann kostet sie nichts.
    void setExternalAudioNr(int sliceId, NrSlot slot, float amount);
    /// Den Zweig einer Scheibe vergessen (Scheibe weg, Quelle weg).
    void removeExternalAudioNr(int sliceId);

    /// Den Rauschzweig auf einen fertigen 48-kHz-Block anwenden.
    /// Gemeinsam fuer Flex und Kiwi; ohne gewaehltes Verfahren ein
    /// Rueckkehrbefehl.
    void wendeFremdtonRauschenAn(int sliceId, QByteArray& block);

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

    /// The post-modulator siphon, for measuring what actually goes out:
    /// occupied bandwidth, splatter, the shape of the transmitted
    /// spectrum. Read from the GUI thread via snapshot(); the audio
    /// thread only ever memcpys into it.
    ///
    /// Filled whether or not the TX monitor is switched on — the
    /// measurement is about the transmitter, not about listening.
    const MicSpectrum& txSiphonSpectrum() const noexcept
    { return m_txSiphonRing; }

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

    // TX-Mikrofon-Klick-Untersuchung (2026-09-08): siehe
    // IAudioBus::nsSinceLastCallback(). -1, wenn kein Bus offen ist
    // oder der Bus-Typ es nicht unterstuetzt.
    qint64 txMicNsSinceLastCallback() const {
        return m_txInputBus ? m_txInputBus->nsSinceLastCallback() : -1;
    }

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
    // the plugin↔CoreAudioHalBus contract (hal-plugin/LongpathVAX.cpp
    // makePCMFormat + CoreAudioHalBus negotiated format); other
    // formats are not expected and treated as "no data".
    //
    // Threading: called from the WDSP audio thread (via
    // VaxTxMicSource::pullSamples → CompositeTxMicRouter dispatch).
    // Audio-thread-safe: m_vaxTxBus->pull is lock-free (POSIX shm
    // ring); a thread_local scratch buffer absorbs the stereo source
    // bytes so we do not allocate per call.
    int pullVaxTxMic(float* dst, int n);

    /// Phase 3M-1c TX pump v3 — PC mic override gate.
    ///
    /// Returns true when the worker should overlay PC mic samples on
    /// top of the radio mic samples in m_in.  Gated by:
    ///   1. m_micSourceWantsPc (true iff TransmitModel::micSource ==
    ///      MicSource::Pc; updated by onMicSourceChanged()).
    ///   2. m_txInputBus exists and is open.
    ///
    /// Both conditions are read atomically; both must be true.  Mirrors
    /// the conditional invocation of `asioIN(pcm->in[stream])` at
    /// Thetis cmaster.c:379 [v2.10.3.13].
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

    /// Silence the receiver while the operator listens to themselves.
    ///
    /// Reported from the bench: with "Hear myself" on, the band noise
    /// and your own voice arrive together and neither can be judged.
    /// Muting the receiver by hand and forgetting to unmute it is worse,
    /// so this is tied to the monitor and restored with it.
    ///
    /// NOT setMasterMuted(): that gates the push to the speakers, which
    /// happens AFTER the master mix has been drained, so it would
    /// silence the monitor along with the receiver. This gates the RX
    /// accumulate instead — the monitor is a separate slot and goes
    /// through untouched.
    ///
    /// The operator's own mute and volume are not read or written here.
    /// A temporary state that borrows a control the operator also uses
    /// is a state that eventually gets left behind in the wrong
    /// position.
    void setRxMutedForMonitor(bool muted);
    bool rxMutedForMonitor() const
    { return m_rxMutedForMonitor.load(std::memory_order_acquire); }
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

    // Sub-Phase 12 Task 12.4 — VAC feedback-loop tuning (per addendum §2.4).
    // The four fields map to Thetis IVAC feedback tuning knobs. Persists to
    // audio/VacFeedback/<channel>/{Gain,SlewTimeMs,PropRing,FfRing}.
    // Live-apply is deferred to Phase 3M IVAC port.
    // TODO(sub-phase-12-vac-feedback-live-apply): wire into IVAC engine.
    struct VacFeedbackParams {
        float gain      = 1.0f;
        int   slewTimeMs = 5;
        int   propRing   = 2;
        int   ffRing     = 2;
    };
    void setVacFeedbackParams(int channel, const VacFeedbackParams& params);

    // Sub-Phase 12 Task 12.4 — Reset all audio settings (addendum §2.5).
    // Clears all audio/* keys from AppSettings, preserving
    // slice/<N>/VaxChannel and tx/OwnerSlot. Then rebuilds buses from
    // seeded defaults and emits the config-changed signal cascade so
    // subscribed UIs refresh.
    void resetAudioSettings();

    float vaxRxGain(int channel) const;
    bool  vaxMuted(int channel) const;
    float vaxTxGain() const { return m_vaxTxGain.load(std::memory_order_acquire); }

    // Meter readouts for VaxApplet. Safe when the slot is empty / not open:
    // returns 0.0f so the UI can still bind and show a quiet meter.
    float vaxRxLevel(int channel) const;
    float vaxTxLevel() const;

    // Peak input level (0.0–1.0 normalized) from the PC Mic capture bus
    // (m_txInputBus). Used by AudioTxInputPage's Test Mic VU bar (I.2).
    // Returns 0.0f when m_txInputBus is null or not open. Safe to call
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
    void flowStateChanged(Longpath::AudioEngine::FlowState state);

    /// One mixed anti-VOX reference block, interleaved stereo float32,
    /// length = frames * 2. Emitted from rxBlockReady on the DSP thread,
    /// once per audio period, whenever the anti-VOX mixer's readiness
    /// barrier releases a block.
    ///
    /// **DirectConnection ONLY.** `samples` points at a thread_local
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
    void vaxRxGainChanged(int channel, float gain);
    void vaxMutedChanged(int channel, bool muted);
    void vaxTxGainChanged(float gain);

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
    void speakersConfigChanged(Longpath::AudioDeviceConfig cfg);
    void headphonesConfigChanged(Longpath::AudioDeviceConfig cfg);
    void txInputConfigChanged(Longpath::AudioDeviceConfig cfg);
    void vaxConfigChanged(int channel, Longpath::AudioDeviceConfig cfg);

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

    // Open m_txInputBus with the persisted device or platform-default mic
    // capture so PhoneCwApplet's mic-level meter has signal without
    // requiring Setup configuration. Users can override later via
    // Setup → Audio → Devices (or → TX Input). Loaded from
    // audio/TxInput AppSettings keys (loadFromSettings returns a
    // default-constructed config on first run → empty deviceName →
    // platform default mic).
    void ensureTxInputOpen();

    RadioModel* m_radio{nullptr};

    // Sub-Phase 12 Task 12.2 — live-reconfig safety mutex for the speakers
    // bus. setSpeakersConfig() acquires this during tear-down + rebuild.
    // rxBlockReady() uses try_lock and drops the block if it can't acquire
    // (≤1 ms of silence is inaudible vs. a use-after-free). NOT held in the
    // audio callback path (acquires try_lock only; never blocks).
    std::mutex m_speakersBusMutex;
    // Same contract for the headphones bus: rxBlockReady try_locks it
    // around the push, setHeadphonesConfig holds it across tear-down +
    // rebuild. Added with the 2026-08-11 bench fix that first wired the
    // headphones bus into the mix path at all.
    std::mutex m_headphonesBusMutex;

    std::unique_ptr<IAudioBus> m_speakersBus;

    // ── Driftausgleich am Lautsprecherweg (#131, 2026-10-06) ─────────────
    //
    // Funkgeraet und Tonkarte haben zwei Quarze; gemessen 4 ppm, was den
    // 100-ms-Ring in rund fuenf Stunden zum Ueberlaufen bringt. Der
    // Angleich sitzt VOR dem push und laeuft damit auf dem DSP-Faden --
    // kein Rueckruf, keine Sperre (CLAUDE.md).
    //
    // NUR hier, nicht am Kopfhoererweg: der hat im MasterMixer schon ein
    // eigenes nachgefuehrtes Polster, und zwei Regelungen auf demselben
    // Strom arbeiten gegeneinander. Betreiberentscheidung 2026-10-06.
    //
    // Ab Werk AUS. Ein neuer Umtaster im Hoerweg einer laufenden Station
    // gehoert nicht ungefragt eingeschaltet; `RxDriftAusgleich=True`
    // schaltet ihn ein, und danach entscheidet das Ohr.
    std::unique_ptr<RxRatenAngleich> m_rxDrift;

    // ── Der Driftausgleich darf eine Sendepause nicht als Drift lesen ─
    //
    // Waehrend einer eigenen Sendung kommt kein Empfangston; der
    // Ausgangspuffer wird derweil leergezogen. Das ERSTE Paket danach
    // zeigt dem Regler einen leeren Ring, er liest daraus einen
    // riesigen Verbrauch und reiszt das Verhaeltnis hoch -- am
    // 2026-10-28 auf 1,001745 (1744 ppm) und 0,998784 (-1216 ppm),
    // wo ein echter Quarz um wenige ppm abweicht. Mit diesem
    // Verhaeltnis wird die ganze folgende Hoerperiode umgetastet, und
    // FT8 vertraegt das nicht: drei Hoerfenster hintereinander
    // "0 Funde", dann wieder 17.
    //
    // Gesetzt wird aus dem Hauptfaden, gelesen und geloescht im
    // Tonfaden -- `zuruecksetzen()` selbst fasst Zustand an, den der
    // Tonfaden benutzt, und gehoert deshalb nicht in einen fremden.
    std::atomic<bool> m_rxDriftZuruecksetzen{false};
    std::unique_ptr<IAudioBus> m_headphonesBus;
    std::unique_ptr<IAudioBus> m_txInputBus;

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
    MasterMixer m_masterMix;

    // KiwiSDR-Ton: je Scheibe ein Wandler von 24 kHz auf die
    // Betriebsrate, dazu der Satz der eingeschalteten Scheiben. Beide
    // werden NUR vom Steuerfaden angefasst; der Ton-Faden liest hier
    // nichts.
    std::map<int, std::unique_ptr<Resampler>> m_kiwiResamplers;
    std::set<int> m_kiwiEnabledSlices;
    std::vector<float> m_kiwiScratch;

    // SunSDR-Ton (TCI-Client): wie oben bei Kiwi, aber die Quellrate ist
    // nicht fest (siehe feedSunSdrAudioData) — der Wandler traegt darum
    // die Rate, fuer die er gebaut wurde, mit sich, statt sie separat in
    // einer zweiten Abbildung zu fuehren, die aus dem Takt geraten
    // koennte. NUR vom Steuerfaden angefasst, wie bei Kiwi.
    struct SunSdrResamplerSlot {
        std::unique_ptr<Resampler> resampler;
        int rateHz = 0;
    };
    std::map<int, SunSdrResamplerSlot> m_sunSdrResamplers;
    std::set<int> m_sunSdrEnabledSlices;

    // Flex-Ton: derselbe Zuschnitt. Der Wandler traegt die Rate, fuer
    // die er gebaut wurde, mit sich — siehe feedFlexAudioData.
    struct FlexResamplerSlot {
        std::unique_ptr<Resampler> resampler;
        int rateHz = 0;
    };
    std::map<int, FlexResamplerSlot> m_flexResamplers;
    /// Je Scheibe ein Rauschzweig für fremden Ton. Angelegt erst,
    /// wenn ein Verfahren gewählt wird — siehe setExternalAudioNr.
    std::map<int, std::unique_ptr<ExternalAudioNr>> m_externalAudioNr;
    std::set<int> m_flexEnabledSlices;

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

    // Control-to-audio withdrawal handshake. The audio thread never waits:
    // it either enters a region or drops a block while admission is closed.
    // The control thread invalidates both mixers, then waits for already
    // admitted regions to finish before withdrawal returns.
    std::atomic<bool> m_mixAdmissionClosed{false};
    std::atomic<unsigned> m_mixRegionsInFlight{0};

    // Per-tap quiescence counters (2026-09-06 race fix). setXxxTap(nullptr,
    // ...) callers free or reuse the AudioTapRing immediately afterward
    // (see e.g. QsoRecorderController's dtor comment "Erst den Abgriff
    // loesen, dann sterben"), but until this fix setXxxTap only stored two
    // plain atomics with no handshake — a rxBlockReady() call already past
    // the tap pointer's load on the real audio thread could still be inside
    // tap->write() when the ring is destroyed underneath it. rxBlockReady
    // announces itself here (fetch_add) BEFORE loading the tap pointer, and
    // setXxxTap(nullptr, ...) publishes the null pointer THEN waits for the
    // counter to drain — see writeToTapIfCurrent / waitForTapQuiescence in
    // AudioEngine.cpp. Deliberately one counter per tap rather than reusing
    // m_mixRegionsInFlight above: that counter (and its admission-closed
    // gate) stops the ENTIRE mix pipeline for every slice, which is fine
    // for the rare full-slice teardown it guards but would glitch every
    // other slice's audio each time an operator merely stops a QSO
    // recording, ASR, WAV capture, or the RTTY decoder.
    std::atomic<unsigned> m_qsoTapBusy{0};
    std::atomic<unsigned> m_asrTapBusy{0};
    std::atomic<unsigned> m_wavRecordTapBusy{0};
    std::atomic<unsigned> m_rttyTapBusy{0};
    std::atomic<unsigned> m_cwTapBusy{0};
    std::atomic<unsigned> m_ft8TapBusy{0};

    // Die Lauschplaetze (2026-11-19), siehe setFt8LauschTap. Die
    // Platznummer steht im Kennungsfeld, damit derselbe Schreiber
    // (writeToTapIfCurrent) wie bei allen anderen Abgriffen gilt.
    std::array<std::atomic<AudioTapRing*>, kFt8LauschPlaetze> m_ft8LauschTap{};
    std::array<std::atomic<int>,           kFt8LauschPlaetze> m_ft8LauschId{};
    std::array<std::atomic<unsigned>,      kFt8LauschPlaetze> m_ft8LauschBusy{};
    struct Ft8LauschWandler {
        std::unique_ptr<Resampler> resampler;
        int rateHz{0};
    };
    std::array<Ft8LauschWandler, kFt8LauschPlaetze> m_ft8LauschWandler;

    // Test-only seam for the four counters above (2026-09-06). Fires on
    // the audio thread from inside writeToTapIfCurrent (AudioEngine.cpp),
    // after the matching busy counter has been incremented and the tap
    // pointer loaded, immediately before tap->write() runs — i.e. inside
    // the exact window the fix closes. A no-op std::function outside
    // tests (checked, never called), so it costs nothing in production;
    // the setter that arms it is gated under LONGPATH_BUILD_TESTS below
    // like every other *ForTest seam in this class.
    std::function<void()> m_tapWriteDelayHookForTest;

#ifdef LONGPATH_BUILD_TESTS
    std::function<void()> m_withdrawalPublishedHookForTest;
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
    // worker thread via isPcMicOverrideActive().  Default false matches
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
    std::atomic<bool>  m_rxMutedForMonitor{false};
    std::atomic<bool>  m_txMonitorEnabled{false};  // default off per plan §0 row 9

    // ── The last sixteen seconds of what is actually going out ───────
    //
    // Fed from txMonitorBlockReady BEFORE the monitor-enabled check, so
    // the measurement does not depend on whether the operator happens
    // to be listening to themselves. The siphon runs unconditionally
    // (TXA.c:394, run=1), so this fills whenever the transmit chain is
    // running — during a real transmission as well as during the
    // off-air monitor.
    //
    // MicSpectrum is reused rather than copied. Despite the name it is
    // not about microphones: it is a lock-free ring of recent mono
    // audio with a snapshot read, which is exactly what is wanted, and
    // a second lock-free ring on the transmit path is not a thing to
    // have two of.
    MicSpectrum m_txSiphonRing;
    // Default 0.5f — mirrors the fixed coefficient used in Thetis audio.cs
    // for the aaudio mix path; Longpath exposes this as user-adjustable
    // volume (pre-code review §4.4). Not a port; AudioEngine is Longpath-native.
    std::atomic<float> m_txMonitorVolume{0.5f};

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

#if defined(Q_OS_LINUX) && defined(LONGPATH_HAVE_PIPEWIRE)
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

} // namespace Longpath
