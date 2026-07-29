// no-port-check: AetherSDR-derived NereusSDR file; Thetis cmaster.cs /
// audio.cs references in inline cites are behavioral source-first cites
// for sample sizes / timing / mix coefficient parity only, not Thetis
// logic ports.

// =================================================================
// src/core/AudioEngine.cpp  (NereusSDR)
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
//                 AI-assisted via Anthropic Claude Code. start() eagerly
//                 constructs CoreAudioHalBus (macOS) / LinuxPipeBus (Linux)
//                 instances for VAX RX 1..4 and a separate VAX TX virtual
//                 bus (m_vaxTxBus). Failed open() per slot logs and
//                 degrades to silence on that channel; surviving slots
//                 stay live. Windows path stays null pending Sub-Phase 9
//                 BYO wiring.
//   2026-04-20 — Sub-Phase 10 Task 10a master-mute API by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code. Adds
//                 setMasterMuted implementation (acq_rel exchange +
//                 masterMutedChanged emission on distinct state), and a
//                 single-atomic-load mute gate around the speakers push in
//                 rxBlockReady. Gate covers the speakers bus ONLY — VAX
//                 tap path and master-mix accumulation remain unconditional
//                 so 3rd-party consumers of VAX aren't silenced by the
//                 local monitor mute. No alloc, no lock, no logging —
//                 RT-safety preserved.
//   2026-04-20 — Sub-Phase 9 Task 9.2a per-channel VAX rx gain + mute + tx
//                 gain by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. rxBlockReady grafts the mute/gain path onto
//                 the VAX tap: muted channels skip push entirely; unity
//                 gain preserves the raw-samples fast path; non-unity gain
//                 copy-multiplies into a thread_local scratch distinct from
//                 the existing master-mix scratch. setVaxRxGain /
//                 setVaxMuted / setVaxTxGain follow the acq_rel exchange
//                 pattern from setVolume, and change-signals fire only
//                 when the stored value differs from the new one.
//   2026-04-20 — Sub-Phase 12 Task 12.2 by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code. Adds per-endpoint config-changed
//                 signals (speakersConfigChanged / headphonesConfigChanged /
//                 txInputConfigChanged / vaxConfigChanged), m_speakersBusMutex
//                 live-reconfig safety (try_lock in rxBlockReady + exclusive
//                 lock in setSpeakersConfig), ensureSpeakersOpen() now reads
//                 AudioDeviceConfig::loadFromSettings("audio/Speakers"),
//                 setHeadphonesConfig() added, and MasterOutputWidget wired
//                 to speakersConfigChanged for live device-label sync.
//                 Code-review follow-up: 200ms buffer-size debounce moved from
//                 AudioEngine::setSpeakersConfig to DeviceCard (addendum §2.1);
//                 setSpeakersConfig now applies synchronously.
//   2026-04-27 — Phase 3M-1b E.3 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Adds txMonitorBlockReady(samples,frames)
//                 — the audio-thread consumer of TxChannel::sip1OutputReady.
//                 Expands mono TXA block to interleaved stereo, accumulates
//                 into m_masterMix at kTxMonitorSlotId (-2). Slot pre-registered
//                 in ctor with initial gain = m_txMonitorVolume default (0.5f).
//                 setTxMonitorVolume now pushes gain updates to MasterMixer
//                 via setSliceGain. Plan: 3M-1b E.3. Pre-code review §4.3 §4.4.
//   2026-04-28 — Phase 3M-1c D.1 / D.2 by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code. pullTxMic feeds a 720-sample
//                 accumulator and emits micBlockReady on every full block.
//                 clearMicBuffer() resets the accumulator (called on MOX-off
//                 in Phase E). Source: Thetis cmaster.cs:493-518 [v2.10.3.13].
//   2026-04-29 — Phase 3M-1c TX pump architecture redesign by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.  Reverted the D.1 accumulator + D.2
//                 clearMicBuffer + bench-fix-A pumpMic timer; pullTxMic
//                 returns to a pure drain (no accumulator side effects).
//                 TX pump now lives in src/core/TxWorkerThread.{h,cpp}.
//                 Plan: docs/architecture/phase3m-1c-tx-pump-architecture-plan.md
//   2026-05-07 — Issue #201 (master-mute echo tail) by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.  setMasterMuted(true)
//                 now calls m_speakersBus->flush() under m_speakersBusMutex on
//                 the false→true transition so already-queued samples in the
//                 PortAudio output ring don't keep draining out the device
//                 after the mute click — surfaced as a ~1 s "echo" tail on
//                 macOS Intel / Core Audio.  Pairs with new IAudioBus::flush()
//                 (default no-op) and PortAudioBus::flush() (atomic
//                 ringRead := ringWrite).
// =================================================================

#include "AudioEngine.h"

#include "AppSettings.h"
#include "LogCategories.h"
#include "RxChannel.h"        // afGain() — for VAX AF-bypass
#include "WdspEngine.h"       // rxChannel(0) lookup
#include "audio/PortAudioBus.h"
#include "../models/RadioModel.h"
#include "../models/SliceModel.h"

#ifdef Q_OS_MAC
#include "audio/CoreAudioHalBus.h"
#endif
#ifdef Q_OS_LINUX
#include "audio/LinuxPipeBus.h"
#endif
#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
#include "core/audio/PipeWireBus.h"
#include "core/audio/PipeWireThreadLoop.h"
#endif

#include <portaudio.h>

#include <algorithm>
#include <vector>

namespace NereusSDR {

namespace {

// Translate a NereusSDR AudioDeviceConfig into the IAudioBus AudioFormat
// contract. Float32 stereo is the canonical DSP format; channels is kept
// cfg-overridable but the single live path is stereo today.
AudioFormat toAudioFormat(const AudioDeviceConfig& cfg)
{
    AudioFormat f;
    f.sampleRate = cfg.sampleRate;
    f.channels   = cfg.channels;
    f.sample     = AudioFormat::Sample::Float32;
    return f;
}

} // namespace

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
#if defined(Q_OS_LINUX)
    // Cache the Linux audio backend detection result up front so Task 14's
    // dispatch (PipeWireBus vs. LinuxPipeBus pactl path) has a stable
    // answer by the time start() runs. Log only — no signal emission at
    // ctor time (nothing is listening yet; rescanLinuxBackend() is the
    // live-change path).
    m_linuxBackend = detectLinuxBackend();
    qCInfo(lcAudio) << "Linux audio backend detected:"
                    << toString(m_linuxBackend);
#  if defined(NEREUS_HAVE_PIPEWIRE)
    if (m_linuxBackend == LinuxAudioBackend::PipeWire) {
        m_pwLoop = std::make_unique<PipeWireThreadLoop>();
        if (!m_pwLoop->connect()) {
            qCWarning(lcAudio) << "PipeWire connect failed — falling back to Pactl detection";
            m_pwLoop.reset();
            m_linuxBackend = LinuxAudioBackend::Pactl;
            // No linuxBackendChanged emit here — ctor runs before any subscriber
            // (UI / SetupDialog) has been wired. rescanLinuxBackend() emits on
            // the same condition because by then subscribers exist.
        }
    }
#  endif
#endif

    // Own the Pa_Initialize/Pa_Terminate pair so the static PortAudioBus
    // enumeration helpers (hostApis / outputDevicesFor / inputDevicesFor)
    // can be called at any time from the application. Tests that run
    // without a real audio subsystem will see Pa_Initialize fail; log and
    // continue — rxBlockReady / start() degrade to a safe no-op in that
    // case.
    const PaError err = Pa_Initialize();
    if (err != paNoError) {
        qCWarning(lcAudio) << "Pa_Initialize failed:" << Pa_GetErrorText(err)
                           << "— audio subsystem will be inert.";
        m_paInitialized = false;
    } else {
        m_paInitialized = true;
        qCInfo(lcAudio) << "PortAudio initialized:" << Pa_GetVersionText();
    }

    // Pre-register the TX-monitor mixer slot so txMonitorBlockReady's
    // accumulate() call finds the entry without a main-thread insert/rehash
    // race. Initial gain matches m_txMonitorVolume default (0.5f); updated
    // atomically by setTxMonitorVolume via setSliceGain.
    // Plan: 3M-1b E.3. Pre-code review §4.3.
    m_masterMix.setSliceGain(kTxMonitorSlotId, m_txMonitorVolume.load(std::memory_order_relaxed), 0.0f);

    // The monitor only feeds during MOX, so it must never join the
    // mixer's readiness barrier: an intermittent member would stall the
    // drain for kStallTolerance periods on every TX transition.
    m_masterMix.setSliceOpportunistic(kTxMonitorSlotId, true);

    // kTxMonitorSlotId is registered with m_masterMix and ONLY with
    // m_masterMix. Upstream hands the speakers mixer RX1 + RX1S + RX2 + MON
    // and the anti-VOX mixer RX1 + RX1S + RX2 on the very next line
    // (console.cs:27650-27651 [v2.10.3.15]). Monitor audio suppressing the
    // operator's own VOX would be feedback by definition, and an
    // unregistered id is dropped by MasterMixer::accumulate, so leaving it
    // out here is the whole enforcement.

    // No fade on the anti-VOX reference. Thetis creates this mixer with
    // 0.000 on all four slew parameters (cmaster.c:159-175 [v2.10.3.15]),
    // unlike the RX mixer's 0.010 (cmaster.c:297-313 [v2.10.3.15]), because
    // DEXP needs an amplitude-faithful reference from the first sample
    // after a transition. A faded reference under-reports the audio the
    // operator is actually hearing for the length of the fade, and
    //   asig = avsig - antivox_gain * antivox_level   (dexp.c:313-316)
    // then cancels too little: a false VOX trigger, meaning unintended
    // transmit, in exactly the window a transition opens.
    //
    // The per-slice anti-click ramp (kDefaultRampFrames, 5 ms) is left
    // alone. It is ours rather than upstream's, it only bites on a slice's
    // very first block after a join, and DEXP's hang time keeps the state
    // machine out of DEXP_LOW, where antivox_level is recomputed
    // (dexp.c:288 [v2.10.3.15]), for far longer than 5 ms after an unkey.
    m_antiVoxMix.setSlewUpFrames(0);

    // (Phase 3M-1c bench-fix-A added an m_micPumpTimer here that drove
    //  pullTxMic at 5 ms cadence to keep the D.1 720-sample accumulator
    //  ticking after the E.1 push-slot refactor dropped TxChannel's
    //  QTimer.  The TX pump architecture redesign (2026-04-29) deleted
    //  both the timer and the accumulator.  TX pump now lives in
    //  src/core/TxWorkerThread.{h,cpp}, which calls pullTxMic directly.)
}

AudioEngine::~AudioEngine()
{
    // FORWARD CONTRACT #1: stop() drops every IAudioBus member explicitly
    // here, while m_pwLoop is still alive. Implicit member-teardown order
    // (m_pwLoop declared LAST → destroyed LAST) is the second line of defense.
    // See AudioEngine.h §"FORWARD CONTRACT #1 — DECLARED LAST".
    stop();
    if (m_paInitialized) {
        Pa_Terminate();
        m_paInitialized = false;
    }
}

void AudioEngine::setRadioModel(RadioModel* radio)
{
    m_radio = radio;
}

#if defined(Q_OS_LINUX)
void AudioEngine::rescanLinuxBackend()
{
    const auto previous = m_linuxBackend;
    m_linuxBackend = detectLinuxBackend();
    if (m_linuxBackend == previous) { return; }

    qCInfo(lcAudio) << "Linux audio backend changed:"
                    << toString(previous) << "→"
                    << toString(m_linuxBackend);

#  if defined(NEREUS_HAVE_PIPEWIRE)
    // Transitioning TO PipeWire (None/Pactl → PipeWire): create m_pwLoop.
    // On connect failure revert m_linuxBackend rather than emitting a
    // phantom-PipeWire signal with no backing loop.
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && !m_pwLoop) {
        m_pwLoop = std::make_unique<PipeWireThreadLoop>();
        if (!m_pwLoop->connect()) {
            qCWarning(lcAudio) << "PipeWire connect failed during rescan — reverting to"
                               << toString(previous);
            m_pwLoop.reset();
            m_linuxBackend = previous;
            return;  // skip the linuxBackendChanged emit
        }
    }
    // NOTE: transitioning AWAY from PipeWire (PipeWire → Pactl/None)
    // requires bus teardown first per Forward Contract #1 (PipeWireThreadLoop.cpp
    // §"FORWARD CONTRACT #1"). Buses in m_vaxBus[N] and m_vaxTxBus may be
    // PipeWire-backed; destroying m_pwLoop while they're alive crashes on
    // m_loop->lock(). For now, app restart is the supported recovery path
    // for the PipeWire daemon dying mid-session. A future task will add
    // graceful downgrade with proper bus teardown.
#  endif

    emit linuxBackendChanged(previous, m_linuxBackend);
}
#endif

void AudioEngine::preregisterSlices(int count)
{
    // Slice A always exists, so a caller that has no radio (unit tests, a
    // disconnected engine) still gets id 0 — this is the behaviour the
    // former slice-0-only pre-registration had.
    const int wanted = std::max(1, count);
    for (int id = m_preregisteredSlices; id < wanted; ++id) {
        // Unity gain, centre pan: identical to the slice-0 registration
        // this generalises, so slice A behaviour is unchanged and the
        // extra ids are inert until a slice actually binds to them.
        m_masterMix.setSliceGain(id, 1.0f, 0.0f);

        // Same slots on the anti-VOX instance, registered here for the same
        // reason: accumulate() drops ids it has no entry for, and the map
        // must not be mutated once the DSP thread is reading it lock-free.
        // Unity gain rather than the operator's per-slice volume, because
        // the reference is the receiver audio itself: upstream feeds
        // pcm->rcvr[rx].audio[j] to both mixers unmodified and lets each
        // apply its own (cmaster.c:370-372 [v2.10.3.15]).
        m_antiVoxMix.setSliceGain(id, 1.0f, 0.0f);
    }
    if (wanted > m_preregisteredSlices) {
        m_preregisteredSlices = wanted;
    }
}

void AudioEngine::setSliceStreaming(int sliceId, bool streaming)
{
    m_masterMix.setSliceStreaming(sliceId, streaming);

    // Membership mirrors the speakers mixer, and every membership change in
    // the engine funnels through here so the two cannot drift apart.
    //
    // Mandatory rather than stylistic. setMoxState withdraws the gated slice
    // for the length of a transmission, and neither barrier has a timeout
    // that would give up on a member (MasterMixer.h, divergence 3). A slice
    // left enrolled here would wedge the anti-VOX mix for the whole over,
    // leaving DEXP with a stale antivox_level exactly when the operator is
    // keyed up.
    //
    // Upstream keeps the pair in step the same way: the two mixers are
    // driven by adjacent SetAAudioMixStates / SetAntiVOXSourceStates calls
    // on every transition (console.cs:27650-27651 [v2.10.3.15]).
    m_antiVoxMix.setSliceStreaming(sliceId, streaming);
}

void AudioEngine::start()
{
    if (m_running) {
        return;
    }

    // Pre-register every slice id this radio can host with MasterMixer,
    // before any audio-thread accumulate() can race against a main-thread
    // unordered_map insert+rehash on first-block (design-decision D6,
    // plan §Sub-Phase 4 Task 4.1).
    //
    // Only slice 0 was registered until Sub-Epic I connected the
    // multi-slice data plane. MasterMixer::accumulate drops any id it has
    // no entry for, so slices B-E were demodulated by RxDspWorker and then
    // silently discarded here — the "secondary slices produce no audio"
    // bench symptom. Enrolling a slice lazily on its first block is not an
    // option: that is a main-thread insert into a map the DSP thread is
    // already reading lock-free.
    //
    // boardCapabilities() rather than RadioModel::maxSlices(): start() runs
    // from the WDSP-init lambda in connectToRadio, before m_connection is
    // assigned, so the accessor would still report the disconnected default
    // of 1. Same reason the WDSP RX channel pool is sized off caps directly
    // (RadioModel.cpp §"open the WDSP channel pool").
    preregisterSlices(m_radio ? m_radio->boardCapabilities().maxSlices : 1);

    ensureSpeakersOpen();
    ensureTxInputOpen();

    // Sub-Phase 8.5: eagerly construct platform-native VAX RX buses + the
    // VAX TX virtual bus on macOS / Linux so coreaudiod / pactl publish the
    // virtual devices the moment audio is running. Each slot opens
    // independently — a single failure (e.g. HAL plugin not installed,
    // pactl missing) logs a warning, leaves the slot null, and the
    // rxBlockReady tee silently skips that channel.
    //
    // On Windows m_vaxBus / m_vaxTxBus stay null here.
    // TODO(sub-phase-9-byo): wire user-picked virtual cables via
    // setVaxConfig() once the Setup → Audio → VAX BYO UI lands.
    for (int channel = 1; channel <= 4; ++channel) {
        const int idx = channel - 1;
        if (m_vaxBus[idx]) {
            // Caller wired an explicit device via setVaxConfig() before
            // start() ran — honour that and don't clobber it with the
            // platform-native bus.
            continue;
        }
        m_vaxBus[idx] = makeVaxBus(channel);
        if (m_vaxBus[idx]) {
            qCInfo(lcAudio) << "VAX" << channel << "bus opened (eager)"
                            << "[" << m_vaxBus[idx]->backendName() << "]";
        }
    }

    if (!m_vaxTxBus) {
        m_vaxTxBus = makeVaxTxBus();
        if (m_vaxTxBus) {
            qCInfo(lcAudio) << "VAX TX bus opened (eager)"
                            << "[" << m_vaxTxBus->backendName() << "]";
        } else {
            qCWarning(lcAudio) << "VAX TX bus open failed — TX disabled "
                                  "(see preceding LinuxPipeBus / PipeWireBus log)";
        }
        // TODO(phase3M): pull TX audio from m_vaxTxBus when
        // TransmitModel::txOwnerSlot() != MicDirect. The bus is opened
        // here so 3rd-party apps see the virtual TX device immediately;
        // no consumer is wired in this phase.
    }

    m_running = true;

    qCInfo(lcAudio) << "AudioEngine started ("
                    << (m_speakersBus && m_speakersBus->isOpen()
                            ? "speakers bus open"
                            : "speakers bus NOT open")
                    << ")";
}

void AudioEngine::stop()
{
    // Close every owned bus unconditionally — setVaxConfig / setHeadphonesConfig
    // etc. may populate bus slots even when start() was never called (test
    // paths, SetupDialog preview on a freshly constructed engine). If we only
    // reset buses when m_running is true the implicit member-destructor order
    // tears down m_pwLoop AFTER the bus dtors try to call m_loop->lock(),
    // causing a SEGFAULT on Linux/PipeWire.  unique_ptr::reset() on a null
    // pointer is a no-op, so resetting unpopulated slots is always safe.
    //
    // LinuxPipeBus::close() invokes QProcess to drive `pactl unload-module`,
    // which requires a running event loop on the calling thread; AudioEngine
    // is parented to the main (GUI) thread by RadioModel so stop() always
    // runs there. The same contract covers the destructor path (~unique_ptr
    // → ~LinuxPipeBus → close()). The CoreAudioHalBus / PortAudioBus dtors
    // have no main-thread requirement.
    m_speakersBus.reset();
    m_headphonesBus.reset();
    m_txInputBus.reset();
    // Reset VAX TX before iterating m_vaxBus so the close ordering is
    // TX-first then RX-1..4 — symmetric with the start() construction
    // order (RX-1..4 then TX) inverted on teardown, and lets a future
    // TX-poll consumer release any reference to the TX bus before the
    // RX taps come down.
    m_vaxTxBus.reset();
    for (auto& bus : m_vaxBus) {
        bus.reset();
    }

    if (!m_running) {
        return;
    }
    m_running = false;
    qCInfo(lcAudio) << "AudioEngine stopped";
}

// ---------------------------------------------------------------------------
// pauseInput / resumeInput / reinitForSampleRate — Task 1.6
//
// Coordination hooks for the sample-rate live-apply coordinator in RadioModel.
//
// In the current architecture AudioEngine is driven purely passively:
// RxDspWorker calls rxBlockReady() from the DSP thread; AudioEngine does not
// have its own pump timer.  Quiescing audio during a rate change is therefore
// accomplished by stopping the I/Q feed into the DSP worker (done by
// RadioModel::setSampleRateLive before calling rebuild), not by stopping
// AudioEngine itself.
//
// pauseInput() and resumeInput() are coordination markers — they log the
// transition and are the natural extension point for future implementations
// that require an explicit bus close/reopen (e.g. PipeWire rate negotiation
// on a rate-constrained device).
//
// reinitForSampleRate() records the new wire rate for diagnostics and is
// the extension point for VAX-rate tracking in Phase 3M.  WDSP always
// delivers 64-sample / 48 kHz audio blocks to AudioEngine regardless of
// the wire rate, so the speakers bus does NOT need to be closed and reopened.
// ---------------------------------------------------------------------------
void AudioEngine::pauseInput()
{
    qCDebug(lcAudio) << "AudioEngine: pauseInput (sample-rate live-apply quiesce)";
    // Hook: future active-drain implementations (PipeWire, PortAudio restart)
    // go here.  For now the caller (RadioModel::setSampleRateLive) stops the
    // DSP worker feed before calling this; the bus drains naturally via the
    // absence of rxBlockReady() calls.
}

void AudioEngine::resumeInput()
{
    qCDebug(lcAudio) << "AudioEngine: resumeInput (sample-rate live-apply resume)";
    // Hook: counterpart to pauseInput(). Future active-drain implementations
    // re-open the bus here.  Current architecture: DSP worker resumes feeding
    // after RadioModel::setSampleRateLive reconnects the I/Q signal.
}

void AudioEngine::reinitForSampleRate(int newWireRateHz)
{
    // WDSP decimates input_rate → 48 kHz and always delivers 64-sample
    // blocks to AudioEngine.  The speakers bus frame-size is set at open
    // time and does not need to change when the wire rate changes.
    // This method is a forward-compatibility hook for VAX rate tracking
    // and PipeWire per-stream rate negotiation (Phase 3M).
    qCDebug(lcAudio) << "AudioEngine: reinitForSampleRate" << newWireRateHz
                     << "Hz (note: WDSP output remains 64 samples @ 48 kHz; "
                        "bus does not need to be reopened)";
}

std::unique_ptr<IAudioBus> AudioEngine::makeBus(const AudioDeviceConfig& cfg,
                                                bool capture)
{
    // PortAudio path — used for speakers / mic / Windows-BYO VAX devices.
    // Platform-native VAX RX/TX virtual buses use makeVaxBus() /
    // makeVaxTxBus() (Sub-Phase 8.5).
    auto bus = std::make_unique<PortAudioBus>();
    PortAudioConfig pcfg;
    pcfg.direction     = capture ? AudioDirection::Input
                                 : AudioDirection::Output;
    pcfg.hostApiIndex  = cfg.hostApiIndex;
    pcfg.deviceName    = cfg.deviceName;
    pcfg.bufferSamples = cfg.bufferSamples;
    pcfg.exclusiveMode = cfg.exclusiveMode;
    bus->setConfig(pcfg);

    const AudioFormat fmt = toAudioFormat(cfg);
    if (!bus->open(fmt)) {
        qCWarning(lcAudio) << "IAudioBus open failed:" << bus->errorString();
        return nullptr;
    }
    return bus;
}

std::unique_ptr<IAudioBus> AudioEngine::makeVaxBus(int channel)
{
    // Sub-Phase 8.5: platform-native VAX RX virtual bus. Format is fixed at
    // 48 kHz stereo float32 — this is the contract both CoreAudioHalBus and
    // LinuxPipeBus enforce in open(), and it matches the spec §8.1 wire
    // format the HAL plugin / pactl source expose to consumer apps.
    if (channel < 1 || channel > 4) {
        return nullptr;
    }

    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels   = 2;
    fmt.sample     = AudioFormat::Sample::Float32;

#if defined(Q_OS_MAC)
    CoreAudioHalBus::Role role = CoreAudioHalBus::Role::Vax1;
    switch (channel) {
        case 1: role = CoreAudioHalBus::Role::Vax1; break;
        case 2: role = CoreAudioHalBus::Role::Vax2; break;
        case 3: role = CoreAudioHalBus::Role::Vax3; break;
        case 4: role = CoreAudioHalBus::Role::Vax4; break;
    }
    auto bus = std::make_unique<CoreAudioHalBus>(role);
    if (!bus->open(fmt)) {
        qCWarning(lcAudio) << "CoreAudioHalBus open failed for VAX" << channel
                           << ":" << bus->errorString();
        return nullptr;
    }
    return bus;
#elif defined(Q_OS_LINUX)
#  ifdef NEREUS_HAVE_PIPEWIRE
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && m_pwLoop) {
        PipeWireBus::Role role = PipeWireBus::Role::Vax1;
        switch (channel) {
            case 1: role = PipeWireBus::Role::Vax1; break;
            case 2: role = PipeWireBus::Role::Vax2; break;
            case 3: role = PipeWireBus::Role::Vax3; break;
            case 4: role = PipeWireBus::Role::Vax4; break;
        }
        auto bus = std::make_unique<PipeWireBus>(role, m_pwLoop.get());
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "PipeWireBus open failed for VAX"
                               << channel << ":" << bus->errorString();
            return nullptr;
        }
        return bus;
    }
#  endif
    // Pactl fallback: existing LinuxPipeBus path — unchanged.
    if (m_linuxBackend == LinuxAudioBackend::Pactl) {
        LinuxPipeBus::Role role = LinuxPipeBus::Role::Vax1;
        switch (channel) {
            case 1: role = LinuxPipeBus::Role::Vax1; break;
            case 2: role = LinuxPipeBus::Role::Vax2; break;
            case 3: role = LinuxPipeBus::Role::Vax3; break;
            case 4: role = LinuxPipeBus::Role::Vax4; break;
        }
        auto bus = std::make_unique<LinuxPipeBus>(role);
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "LinuxPipeBus open failed for VAX" << channel
                               << ":" << bus->errorString();
            return nullptr;
        }
        return bus;
    }
    qCInfo(lcAudio) << "Linux VAX" << channel << "disabled — no audio backend";
    return nullptr;
#else
    // Windows: TODO(sub-phase-9-byo): wire user-picked virtual cables via
    // setVaxConfig(). Returning nullptr here leaves the slot empty so the
    // rxBlockReady tee silently skips this channel until BYO config arrives.
    (void)channel;
    return nullptr;
#endif
}

std::unique_ptr<IAudioBus> AudioEngine::makeVaxTxBus()
{
    // Sub-Phase 8.5: platform-native VAX TX virtual bus. Opened so coreaudiod
    // / pactl publish the virtual TX device for 3rd-party apps that write
    // outgoing audio (WSJT-X, fldigi, VARA). Consumption (pull() into
    // TxChannel) is a Phase 3M concern — see m_vaxTxBus comment in
    // AudioEngine.h and the TODO in start().
    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels   = 2;
    fmt.sample     = AudioFormat::Sample::Float32;

#if defined(Q_OS_MAC)
    auto bus = std::make_unique<CoreAudioHalBus>(CoreAudioHalBus::Role::TxInput);
    if (!bus->open(fmt)) {
        qCWarning(lcAudio) << "CoreAudioHalBus open failed for VAX TX:"
                           << bus->errorString();
        return nullptr;
    }
    return bus;
#elif defined(Q_OS_LINUX)
#  ifdef NEREUS_HAVE_PIPEWIRE
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && m_pwLoop) {
        auto bus = std::make_unique<PipeWireBus>(
            PipeWireBus::Role::TxInput, m_pwLoop.get());
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "PipeWireBus open failed for VAX TX:"
                               << bus->errorString();
            return nullptr;
        }
        return bus;
    }
#  endif
    // Pactl fallback: existing LinuxPipeBus path — unchanged.
    auto bus = std::make_unique<LinuxPipeBus>(LinuxPipeBus::Role::TxInput);
    if (!bus->open(fmt)) {
        qCWarning(lcAudio) << "LinuxPipeBus open failed for VAX TX:"
                           << bus->errorString();
        return nullptr;
    }
    return bus;
#else
    // Windows: TODO(sub-phase-9-byo): wire user-picked virtual cables via
    // setVaxConfig() (a future TX-side equivalent setter).
    return nullptr;
#endif
}

// ---------------------------------------------------------------------------
// Task 14: split output stream factories.
// On PipeWire, each method opens a dedicated PipeWireBus with the appropriate
// Role. The targetNode / sourceNode override is passed through to PipeWireBus
// so callers (Tasks 15-16, per-slice routing) can bind to a specific node.
// All other platforms (macOS, Windows) and the non-PipeWire Linux paths return
// nullptr — later sub-phases (Task 17+) may wire QAudioSinkAdapter for Primary
// on non-PipeWire Linux, but that is out of Task 14 scope.
// ---------------------------------------------------------------------------

std::unique_ptr<IAudioBus> AudioEngine::makeTxInputBus(const QString& sourceNode)
{
    // 48 kHz stereo Float32 — canonical DSP format.
    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels   = 2;
    fmt.sample     = AudioFormat::Sample::Float32;

#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && m_pwLoop) {
        // TxInput is a capture (INPUT direction) stream; sourceNode names the
        // PipeWire source node to link from (e.g. the mic or a software
        // source). Empty = PipeWire default routing.
        auto bus = std::make_unique<PipeWireBus>(
            PipeWireBus::Role::TxInput, m_pwLoop.get(), sourceNode);
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "PipeWireBus open failed for TxInput:"
                               << bus->errorString();
            return nullptr;
        }
        return bus;
    }
    (void)sourceNode;  // PipeWire available but backend is Pactl/None
#else
    (void)sourceNode;  // Non-Linux or Linux without libpipewire-0.3
#endif
    qCInfo(lcAudio) << "makeTxInputBus: no PipeWire backend — returning nullptr";
    return nullptr;
}

std::unique_ptr<IAudioBus> AudioEngine::makePrimaryOut(const QString& targetNode)
{
    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels   = 2;
    fmt.sample     = AudioFormat::Sample::Float32;

#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && m_pwLoop) {
        auto bus = std::make_unique<PipeWireBus>(
            PipeWireBus::Role::Primary, m_pwLoop.get(), targetNode);
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "PipeWireBus open failed for Primary:"
                               << bus->errorString();
            return nullptr;
        }
        return bus;
    }
    (void)targetNode;
#else
    (void)targetNode;
#endif
    qCInfo(lcAudio) << "makePrimaryOut: no PipeWire backend — returning nullptr";
    return nullptr;
}

std::unique_ptr<IAudioBus> AudioEngine::makeSidetoneOut(const QString& targetNode)
{
    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels   = 2;
    fmt.sample     = AudioFormat::Sample::Float32;

#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && m_pwLoop) {
        auto bus = std::make_unique<PipeWireBus>(
            PipeWireBus::Role::Sidetone, m_pwLoop.get(), targetNode);
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "PipeWireBus open failed for Sidetone:"
                               << bus->errorString();
            return nullptr;
        }
        return bus;
    }
    (void)targetNode;
#else
    (void)targetNode;
#endif
    qCInfo(lcAudio) << "makeSidetoneOut: no PipeWire backend — returning nullptr";
    return nullptr;
}

std::unique_ptr<IAudioBus> AudioEngine::makeMonitorOut(const QString& targetNode)
{
    AudioFormat fmt;
    fmt.sampleRate = 48000;
    fmt.channels   = 2;
    fmt.sample     = AudioFormat::Sample::Float32;

#if defined(Q_OS_LINUX) && defined(NEREUS_HAVE_PIPEWIRE)
    if (m_linuxBackend == LinuxAudioBackend::PipeWire && m_pwLoop) {
        auto bus = std::make_unique<PipeWireBus>(
            PipeWireBus::Role::Monitor, m_pwLoop.get(), targetNode);
        if (!bus->open(fmt)) {
            qCWarning(lcAudio) << "PipeWireBus open failed for Monitor:"
                               << bus->errorString();
            return nullptr;
        }
        return bus;
    }
    (void)targetNode;
#else
    (void)targetNode;
#endif
    qCInfo(lcAudio) << "makeMonitorOut: no PipeWire backend — returning nullptr";
    return nullptr;
}

void AudioEngine::ensureSpeakersOpen()
{
    if (m_speakersBus && m_speakersBus->isOpen()) {
        return;
    }
    if (!m_paInitialized) {
        return;
    }

    // Sub-Phase 12 Task 12.2: read persisted config instead of hardcoded
    // defaults. On a fresh install with no audio/Speakers/* keys,
    // loadFromSettings returns a default-constructed AudioDeviceConfig
    // (empty deviceName) → makeBus treats it as "platform default" —
    // same behavior as the pre-Sub-Phase-12 code.
    const AudioDeviceConfig cfg =
        AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));

    m_speakersBus = makeBus(cfg, /*capture=*/false);
    if (m_speakersBus) {
        m_speakersFormat = m_speakersBus->negotiatedFormat();
        qCInfo(lcAudio) << "Speakers bus opened @"
                        << m_speakersFormat.sampleRate << "Hz /"
                        << m_speakersFormat.channels << "ch"
                        << "[" << m_speakersBus->backendName() << "]";
        emit speakersConfigChanged(cfg);
    }
}

void AudioEngine::ensureTxInputOpen()
{
    if (m_txInputBus && m_txInputBus->isOpen()) {
        return;
    }
    if (!m_paInitialized) {
        return;
    }

    // Phase 3M-1b: open the platform-default mic on start() so the
    // PhoneCwApplet mic-level meter (and PcMicSource on TX) has signal
    // without requiring the user to visit Setup → Audio → Devices.
    // Persisted choice (audio/TxInput keys) takes priority on subsequent
    // launches; first run with no keys returns a default-constructed
    // AudioDeviceConfig (empty deviceName → platform-default mic).
    const AudioDeviceConfig cfg =
        AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput"));

    m_txInputBus = makeBus(cfg, /*capture=*/true);
    if (m_txInputBus) {
        qCInfo(lcAudio) << "TX input bus opened (eager) @"
                        << m_txInputBus->negotiatedFormat().sampleRate << "Hz /"
                        << m_txInputBus->negotiatedFormat().channels << "ch"
                        << "[" << m_txInputBus->backendName() << "]";
        emit txInputConfigChanged(cfg);
    } else {
        qCWarning(lcAudio) << "TX input bus open failed — mic level meter inert";
    }
}

void AudioEngine::setSpeakersConfig(const AudioDeviceConfig& cfg)
{
    // Sub-Phase 12 Task 12.2: applies synchronously.  The 200 ms intra-control
    // debounce for rapid buffer-size scrub lives in DeviceCard (buffer-size
    // combo only) — not here.  Mutex released before emit so handlers may
    // call setSpeakersConfig without deadlocking.
    applySpeakersConfig(cfg);
}

void AudioEngine::applySpeakersConfig(const AudioDeviceConfig& cfg)
{
    if (!m_paInitialized) {
        return;
    }

    // Hold the mutex during tear-down + rebuild. rxBlockReady uses
    // try_lock and drops the block if it can't acquire (≤1 ms of silence
    // is inaudible vs. a use-after-free on the old bus pointer).
    std::unique_lock<std::mutex> lk(m_speakersBusMutex);

    m_speakersBus.reset();
    m_speakersBus = makeBus(cfg, /*capture=*/false);

    AudioDeviceConfig negotiated = cfg;  // carry non-bus fields through
    if (m_speakersBus) {
        m_speakersFormat = m_speakersBus->negotiatedFormat();
        qCInfo(lcAudio) << "Speakers bus reconfigured @"
                        << m_speakersFormat.sampleRate << "Hz /"
                        << m_speakersFormat.channels << "ch";
    } else {
        qCWarning(lcAudio) << "setSpeakersConfig: bus open failed for device"
                           << cfg.deviceName << "— audio silenced on speakers";
    }

    lk.unlock();  // release before emitting so signal handlers can call
                  // setSpeakersConfig without deadlocking
    emit speakersConfigChanged(negotiated);
}

void AudioEngine::setHeadphonesConfig(const AudioDeviceConfig& cfg)
{
    m_headphonesBus.reset();
    if (!m_paInitialized) {
        return;
    }
    m_headphonesBus = makeBus(cfg, /*capture=*/false);
    if (m_headphonesBus) {
        qCInfo(lcAudio) << "Headphones bus opened"
                        << "[" << m_headphonesBus->backendName() << "]";
    }
    emit headphonesConfigChanged(cfg);
}

void AudioEngine::setTxInputConfig(const AudioDeviceConfig& cfg)
{
    // TX pull() wiring lands in Phase 3M. We declare ownership + setter
    // per the Sub-Phase 4 plan so the bus is construction-ready; the bus
    // is inert until TxChannel starts pulling.
    m_txInputBus.reset();
    if (!m_paInitialized) {
        return;
    }
    m_txInputBus = makeBus(cfg, /*capture=*/true);
    if (m_txInputBus) {
        qCInfo(lcAudio) << "TX input bus opened"
                        << "[" << m_txInputBus->backendName() << "]";
    }
    emit txInputConfigChanged(cfg);
}

void AudioEngine::setVaxConfig(int channel, const AudioDeviceConfig& cfg)
{
    // BYO override path. Replaces whatever bus is currently in the slot
    // (platform-native eager bus from start(), or a previous BYO PortAudio
    // bus) with a user-picked PortAudio device. On Mac/Linux this is
    // intentional — power users may prefer to route VAX through a third-
    // party virtual cable instead of the bundled HAL plugin / pactl
    // module. On Windows this is the only VAX path (Sub-Phase 9 BYO).
    if (channel < 1 || channel > 4) {
        return;
    }
    const int idx = channel - 1;
    m_vaxBus[idx].reset();

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    // Empty deviceName = user has not picked a BYO override. Fall back to
    // the platform-native HAL bus (shmem bridge) rather than PortAudio's
    // platform default — the latter resolves to the speakers device on a
    // machine with no virtual cable installed and causes raw VAX audio to
    // bleed through the speakers (pre-master-volume tee, uncontrollable).
    // Matches the addendum §2.2 default-on-Mac/Linux contract.
    if (cfg.deviceName.isEmpty()) {
        m_vaxBus[idx] = makeVaxBus(channel);
        if (m_vaxBus[idx]) {
            qCInfo(lcAudio) << "VAX" << channel
                            << "bus restored (native HAL fallback)"
                            << "[" << m_vaxBus[idx]->backendName() << "]";
        }
        emit vaxConfigChanged(channel, cfg);
        return;
    }
#endif

    if (!m_paInitialized) {
        return;
    }
    m_vaxBus[idx] = makeBus(cfg, /*capture=*/false);
    if (m_vaxBus[idx]) {
        qCInfo(lcAudio) << "VAX" << channel << "bus reconfigured (BYO)"
                        << "[" << m_vaxBus[idx]->backendName() << "]";
    }
    emit vaxConfigChanged(channel, cfg);
}

void AudioEngine::setVaxEnabled(int channel, bool on)
{
    if (channel < 1 || channel > 4) {
        return;
    }
    const int idx = channel - 1;
    if (!on) {
        m_vaxBus[idx].reset();
        return;
    }
    // Already populated (eagerly opened by start() on Mac/Linux, or via a
    // prior setVaxConfig BYO call): no-op. Empty slot: mint the
    // platform-native bus (Mac/Linux) or the PortAudio default (Windows
    // BYO) below.
    if (m_vaxBus[idx]) {
        return;
    }

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    // Re-mint the platform-native bus that start() would have used. Lets
    // a user toggle a VAX channel off and back on without restarting
    // AudioEngine.
    m_vaxBus[idx] = makeVaxBus(channel);
    if (m_vaxBus[idx]) {
        qCInfo(lcAudio) << "VAX" << channel << "bus re-enabled"
                        << "[" << m_vaxBus[idx]->backendName() << "]";
    }
#else
    // Windows lazy PortAudio fallback: enable with defaults if no explicit
    // setVaxConfig has been wired yet. Real config lands via the
    // VirtualCableDetector / Setup→Audio→VAX BYO UI in Sub-Phase 9.
    if (!m_paInitialized) {
        return;
    }
    AudioDeviceConfig defaults;
    m_vaxBus[idx] = makeBus(defaults, /*capture=*/false);
#endif
}

#ifdef NEREUS_BUILD_TESTS
void AudioEngine::setVaxBusForTest(int channel, std::unique_ptr<IAudioBus> bus)
{
    if (channel < 1 || channel > 4) {
        return;
    }
    m_vaxBus[channel - 1] = std::move(bus);
}

void AudioEngine::setSpeakersBusForTest(std::unique_ptr<IAudioBus> bus)
{
    m_speakersBus = std::move(bus);
}

void AudioEngine::setHeadphonesBusForTest(std::unique_ptr<IAudioBus> bus)
{
    m_headphonesBus = std::move(bus);
}

void AudioEngine::setTxInputBusForTest(std::unique_ptr<IAudioBus> bus)
{
    m_txInputBus = std::move(bus);
}

void AudioEngine::setVaxTxBusForTest(std::unique_ptr<IAudioBus> bus)
{
    m_vaxTxBus = std::move(bus);
}

#endif

void AudioEngine::rxBlockReady(int sliceId, const float* samples, int frames)
{
    if (!m_radio || samples == nullptr || frames <= 0) {
        return;
    }

    // SliceModel exposes muted() / setMuted() plus vaxChannel(). The
    // design spec uses audioMuted() as shorthand for the same property;
    // the alias is intentionally not introduced here (design-decision D5,
    // plan §Sub-Phase 4 Task 4.1). A formal rename (if chosen) happens in
    // Sub-Phase 9 alongside per-slice volume / pan control surfaces.
    SliceModel* slice = m_radio->sliceById(sliceId);
    if (slice == nullptr) {
        return;
    }

    // 3M-1b E.4 fold: silence the active TX slice's RX audio during MOX.
    // Non-active slices (e.g. RX2 when TX is on VFO-A, or a second slice in
    // a future multi-RX configuration) keep playing — matching Thetis IVAC
    // mox state-machine in audio.cs:349-384 [v2.10.3.13].
    //
    // m_moxActive acquire load provides the ordering barrier before the
    // non-atomic isActiveSlice() read on slice. A stale read of isActiveSlice
    // is harmless: worst case one ~10 ms block leaks before the next barrier.
    // Silenced: active TX slice's RX audio gated during MOX.
    //
    // This queues silence rather than returning outright, and the slice
    // keeps its place in the mixer's readiness barrier. The barrier has no
    // timeout that would give up on a member, so withholding the feed
    // would hold the drain for the whole transmission and silence every
    // other slice along with it.
    //
    // Thetis takes the stream out of the mix instead, on every MOX
    // transition (console.cs:27650-27771 [v2.10.3.15], via
    // SetAAudioMixStates). We cannot call the equivalent from here: this
    // is the audio thread and setSliceStreaming() takes the slice-map
    // mutex. Same audible result, since the slice contributes nothing.
    //
    // Nothing is queued and nothing is pushed: a gated slice must not put
    // anything on the speakers bus, which is the PR #144 regression where RX
    // audio leaked during TUN/MOX, pinned by
    // tst_audio_engine_rx_leak_during_mox.
    //
    // Dropping out of the mix without wedging it is handled on the main
    // thread in setMoxState(), which withdraws this slice from the readiness
    // barrier for the duration of the transmission. So this really is just a
    // return, and the slice's ring is left exactly as the transmission found
    // it.
    if (m_moxActive.load(std::memory_order_acquire) && slice->isActiveSlice()) {
        return;  // silenced — active TX slice's RX audio gated during MOX
    }

    // Always queue, even when muted. MasterMixer treats mute as a ramp
    // TARGET rather than a gate, so the slice fades out over ~5 ms
    // instead of clicking. Withholding the feed here would defeat that
    // and also drop the slice out of the readiness barrier, which is
    // what the mute path used to do.
    // Mute rides in as an argument rather than through setSliceMuted(),
    // which takes the slice-map mutex: this is the audio thread, and
    // CLAUDE.md's rule is that it never holds a lock.
    m_masterMix.accumulate(sliceId, samples, frames, slice->muted());

    // Anti-VOX hears exactly what the speakers hear. From Thetis
    // cmaster.c:370-372 [v2.10.3.15], every sub-receiver's audio is handed
    // to the transmitter's anti-VOX mixer in the same loop, and from the
    // same buffer, that feeds the speakers mix:
    //   xMixAudio (0, 0, chid (stream, j), pcm->rcvr[rx].audio[j]);
    //   for (k = 0; k < pcm->cmXMTR; k++)
    //       xMixAudio (pcm->xmtr[k].pavoxmix, -1, chid (stream, j), ...);
    //
    // Mute rides in for the same reason it does above, with one deliberate
    // difference from upstream. Thetis reflects an RX mute in the speakers
    // mixer's `what` mask only (audio.cs:1291-1309 [v2.10.3.15]) and leaves
    // the anti-VOX mask alone, yet describes that mask's source as "use
    // audio going to hardware minus MON" (cmaster.cs:946 [v2.10.3.15]). A
    // muted slice contributes nothing to the speakers, so nothing of it is
    // in the room for the microphone to pick up, and counting it would have
    // DEXP subtract audio that was never there. Following the stated intent
    // rather than the mask.
    m_antiVoxMix.accumulate(sliceId, samples, frames, slice->muted());

    // VAX tap receives raw demodulated audio — pre-MasterMixer gain/pan,
    // pre-master-volume — matching Thetis VAC behavior and the spec §3.4
    // pseudocode. Per-channel mute skips the push; non-unity per-channel
    // gain copy-multiplies into a thread_local scratch (distinct from
    // the master-mix `mix` scratch below) so the unity-gain fast path
    // stays zero-copy. See docs/architecture/2026-04-19-vax-design.md
    // §3.4 and §6.4.
    const int vaxCh = slice->vaxChannel();
    if (vaxCh >= 1 && vaxCh <= 4) {
        const int vaxIdx = vaxCh - 1;

        // Mute wins over gain: when muted the tap skips push() entirely —
        // spec says "tags / level UI still reflect routing, but no
        // downstream audio" and we don't waste the bus-push bandwidth.
        const bool muted = m_vaxMuted[vaxIdx].load(std::memory_order_acquire);
        if (!muted) {
            // Snapshot the bus pointer into a local so the isOpen() check
            // and the push() below observe the same IAudioBus instance.
            // The live-reconfig contract (AudioEngine.h) forbids
            // setVaxConfig / setVaxEnabled mid-block, but the snapshot
            // eliminates any torn-read window should a caller violate it.
            IAudioBus* vaxBus = m_vaxBus[vaxIdx].get();
            if (vaxBus != nullptr && vaxBus->isOpen()) {
                const float gainUser =
                    m_vaxRxGain[vaxIdx].load(std::memory_order_acquire);

                // AF-Gain bypass for VAX (post-v0.3.2 AF-Gain rewire fix).
                //
                // Commit e61658f routed AF Gain through WDSP's PanelGain1
                // stage (third_party/wdsp/src/rxa.c:538 [v2.10.3.14]),
                // which is upstream of `samples` here.  Pre-fix WDSP shipped
                // PanelGain1=4.0 (+12 dB) silently and the AF slider acted
                // as a post-DSP scalar elsewhere — VAX inherited the +12 dB
                // and felt "really hot".  Post-fix, VAX inherits whatever
                // attenuation the speaker slider is currently applying,
                // which is wrong for digital-mode apps that expect a stable
                // calibrated level.
                //
                // Inverse-scale by 1/afGain so VAX recovers the pre-
                // PanelGain1 signal level.  Clamped to skip compensation
                // when the slider is essentially muted (≤ 0.001) — div-
                // by-zero guard.  At full mute VAX goes silent (samples
                // were already multiplied by ~0 inside WDSP); proper
                // VAX-independent-of-mute requires the larger pre-PanelGain1
                // tap (Option C in 2026-05-08 design discussion).
                float afInverse = 1.0f;
                if (m_radio && m_radio->wdspEngine()) {
                    if (RxChannel* rx = m_radio->wdspEngine()->rxChannel(0)) {
                        const double afGain = rx->afGain();
                        if (afGain > 0.001) {
                            afInverse = static_cast<float>(1.0 / afGain);
                        }
                    }
                }

                const float gain = gainUser * afInverse;

                const qint64 payloadBytes =
                    static_cast<qint64>(frames) * 2 * sizeof(float);
                if (gain == 1.0f) {
                    // Fast path — raw samples, zero copy.  Hit when both
                    // the per-channel VAX gain and AF slider are at 1.0.
                    vaxBus->push(reinterpret_cast<const char*>(samples),
                                 payloadBytes);
                } else {
                    // Distinct from `mix` scratch below — that one is
                    // reserved for the master-mix accumulate path and
                    // must not be clobbered by the VAX tee. Grows once
                    // per thread via resize(), zero-alloc thereafter.
                    static thread_local std::vector<float> vaxScratch;
                    const int stereoFloats = frames * 2;
                    if (static_cast<int>(vaxScratch.size()) < stereoFloats) {
                        vaxScratch.resize(static_cast<size_t>(stereoFloats));
                    }
                    for (int i = 0; i < stereoFloats; ++i) {
                        vaxScratch[i] = samples[i] * gain;
                    }
                    vaxBus->push(
                        reinterpret_cast<const char*>(vaxScratch.data()),
                        payloadBytes);
                }
            }
        }
    }

    // Flush synchronously on the DSP thread. thread_local scratch so the
    // per-block vector reuse costs zero allocation after the first block
    // per thread. Channel count = 2 is intentionally hard-coded here:
    // the MasterMixer contract and the DSP pipeline both emit stereo.
    static thread_local std::vector<float> mix;
    if (static_cast<int>(mix.size()) < frames * 2) {
        mix.resize(static_cast<size_t>(frames) * 2);
    }

    // Phase 3F: the mixer decides whether a block leaves, not us. It
    // returns 0 until every slice feeding the mix this period has
    // delivered, so N slices produce ONE push instead of N. Draining
    // unconditionally here is what handed the sink two blocks per period
    // on the 2026-07-26 G2E bench and distorted the audio.
    //
    // With a single slice the barrier is satisfied by this very call, so
    // the push happens in the same call stack at the same instant it
    // always did: no added latency on the common path.
    const int mixed = m_masterMix.tryDrain(mix.data(), frames);

    // Drain the anti-VOX reference in the same call stack, so both mixes are
    // paced by their own barrier over the same period.
    //
    // Cadence is the whole point. The slice-0 gate this replaces guaranteed
    // exactly one block per outSize/outRate seconds, which is what DEXP was
    // configured for: SetAntiVOXSize / SetAntiVOXRate describe a block, and
    // xdexp walks antivox_size samples through a single-pole IIR calibrated
    // in sample steps at antivox_rate on each pass (dexp.c:288-297
    // [v2.10.3.15]). Delivery is destructive, not accumulating
    // (dexp.c:708-715 [v2.10.3.15]), so a second block inside one period
    // silently replaces the first. Barrier pacing supplies that cadence now:
    // one drained block per period whatever the stream width.
    //
    // Deliberately AHEAD of the `mixed <= 0` return below rather than after
    // the speakers push. The two barriers are congruent today (same ids,
    // same membership funnel, same feed), so the return would only ever skip
    // a drain that was going to yield nothing anyway; sitting ahead of it
    // means the anti-VOX reference does not silently inherit a future
    // divergence in the speakers path. Nothing else separates the two: the
    // master volume and master-mute gates below apply to `mix`, never to
    // `avMix`, matching upstream, where each mixer carries its own volume
    // and the anti-VOX instance is created at 1.0 (cmaster.c:167
    // [v2.10.3.15]).
    static thread_local std::vector<float> avMix;
    if (static_cast<int>(avMix.size()) < frames * 2) {
        avMix.resize(static_cast<size_t>(frames) * 2);
    }
    const int avFrames = m_antiVoxMix.tryDrain(avMix.data(), frames);
    if (avFrames > 0) {
        // DirectConnection only: avMix is thread_local scratch and the next
        // block overwrites it. See the signal's contract in AudioEngine.h.
        //
        // The consumer, TxWorkerThread::onAntiVoxBlockReady, therefore runs
        // synchronously on this thread inside this emit. It returns on an
        // atomic load while the operator has anti-VOX off, which is the
        // default; with anti-VOX on it copies the block, which is one
        // allocation per period on this thread. That is what the retired
        // slice-0 fork did too, one frame up the stack in
        // RxDspWorker::processIqBatch, except unconditionally.
        emit antiVoxBlockReady(avMix.data(), avFrames);
    }

    if (mixed <= 0) {
        return;
    }
    const int stereoFloats = mixed * 2;

    const float vol = m_masterVolume.load(std::memory_order_acquire);
    if (vol != 1.0f) {
        for (int i = 0; i < stereoFloats; ++i) {
            mix[i] *= vol;
        }
    }

    // Same snapshot idiom as the VAX tap: one load into a local so the
    // isOpen() guard and the push() observe the same IAudioBus.
    //
    // Master-mute gate (Sub-Phase 10 Task 10a): single atomic acquire
    // load per block. Gates the speakers push ONLY — the master-mix
    // accumulate() above and the VAX tap earlier in this method run
    // unconditionally, so 3rd-party apps consuming a VAX channel keep
    // receiving audio while the local monitor is muted. No alloc, no
    // logging — RT-safety preserved.  setMasterMuted(true) also flushes
    // the speakers bus's queued samples (issue #201) so already-buffered
    // pre-mute audio doesn't keep draining out the device after the
    // gate engages — see PortAudioBus::flush() and the call site in
    // setMasterMuted().
    //
    // Sub-Phase 12 live-reconfig: try_lock the speakers bus mutex only
    // around the push itself.  Previously this was held for the entire
    // rxBlockReady (including master-mix accumulate + VAX tap + volume
    // multiply), which meant a contending setSpeakersConfig on the GUI
    // thread dropped the whole block (~1.3 ms of audio) — audible pops.
    // Scoping down to just the push means a contending reconfig drops
    // at most the speakers-push step; the VAX tap and master-mix
    // accumulate keep running uninterrupted.
    if (!m_masterMuted.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> speakersLk(m_speakersBusMutex,
                                                std::try_to_lock);
        if (speakersLk.owns_lock()) {
            IAudioBus* speakersBus = m_speakersBus.get();
            if (speakersBus != nullptr && speakersBus->isOpen()) {
                speakersBus->push(
                    reinterpret_cast<const char*>(mix.data()),
                    static_cast<qint64>(stereoFloats) * sizeof(float));
            }
        }
        // A contending writer holding m_speakersBusMutex
        // (setSpeakersConfig, master-mute flush) silently drops the
        // current block.  The try_to_lock scope is narrowed to just
        // the push specifically to keep that drop window short and
        // bounded; we no longer trace mutex misses since the bench
        // confirmed the contention is rare enough to be inaudible.
    }
}

bool AudioEngine::isPcMicOverrideActive() const noexcept
{
    // Phase 3M-1c TX pump v3 — both conditions must hold for the worker
    // to overlay PC mic samples on radio mic samples:
    //   - the user explicitly selected MicSource::Pc (m_micSourceWantsPc)
    //   - we have an open TX-input bus to pull from
    if (!m_micSourceWantsPc.load(std::memory_order_acquire)) {
        return false;
    }
    return (m_txInputBus != nullptr) && m_txInputBus->isOpen();
}

void AudioEngine::onMicSourceChanged(bool selectedSourceIsPc)
{
    m_micSourceWantsPc.store(selectedSourceIsPc, std::memory_order_release);
}

bool AudioEngine::isVaxMicOverrideActive() const noexcept
{
    // Phase VAX-TX (eager-borg-d64bed, 2026-05-06).  Both conditions must
    // hold for the worker to overlay VAX TX samples on radio mic samples:
    //   - the user explicitly selected MicSource::Vax (m_micSourceWantsVax)
    //   - the VAX TX bus exists and is open (so pullVaxTxMic has somewhere
    //     to read from)
    if (!m_micSourceWantsVax.load(std::memory_order_acquire)) {
        return false;
    }
    return (m_vaxTxBus != nullptr) && m_vaxTxBus->isOpen();
}

void AudioEngine::onMicSourceChangedVax(bool selectedSourceIsVax)
{
    m_micSourceWantsVax.store(selectedSourceIsVax, std::memory_order_release);
}

int AudioEngine::pullTxMic(float* dst, int n)
{
    // Plan: 3M-1b E.1. Pre-code review §0.3 (PcMicSource arch).
    if (m_txInputBus == nullptr || dst == nullptr || n <= 0) {
        return 0;
    }

    const AudioFormat fmt = m_txInputBus->negotiatedFormat();
    const int channels = (fmt.channels > 0) ? fmt.channels : 1;

    int bytesPerSample = 0;
    if (fmt.sample == AudioFormat::Sample::Int16) {
        bytesPerSample = 2;
    } else if (fmt.sample == AudioFormat::Sample::Float32) {
        bytesPerSample = 4;
    } else {
        // Int24 and Int32 are not supported on the TX-mic path.
        qCWarning(lcAudio) << "pullTxMic: unsupported sample format"
                           << static_cast<int>(fmt.sample);
        return 0;
    }

    // To produce n mono output samples we need n * channels source samples.
    const int needSrcSamples = n * channels;
    const qint64 needBytes = static_cast<qint64>(needSrcSamples) * bytesPerSample;

    // thread_local scratch avoids heap allocation on every audio-thread call.
    // Grows once per thread; zero-alloc thereafter.
    static thread_local std::vector<char> scratch;
    if (static_cast<qint64>(scratch.size()) < needBytes) {
        scratch.resize(static_cast<size_t>(needBytes));
    }

    const qint64 gotBytes = m_txInputBus->pull(scratch.data(), needBytes);
    if (gotBytes <= 0) {
        return 0;
    }

    const int gotSrcSamples = static_cast<int>(gotBytes / bytesPerSample);
    const int gotMonoSamples = gotSrcSamples / channels;

    if (fmt.sample == AudioFormat::Sample::Int16) {
        const int16_t* src = reinterpret_cast<const int16_t*>(scratch.data());
        for (int i = 0; i < gotMonoSamples; ++i) {
            // Take left channel (index 0 in each interleaved frame).
            dst[i] = static_cast<float>(src[i * channels]) / 32768.0f;
        }
    } else {
        // Float32
        const float* src = reinterpret_cast<const float*>(scratch.data());
        for (int i = 0; i < gotMonoSamples; ++i) {
            dst[i] = src[i * channels];
        }
    }

    // Phase 3M-1c TX pump architecture redesign (2026-04-29): no
    // accumulator side effects.  TxWorkerThread::onPumpTick is the
    // sole caller and uses the returned sample count directly.

    return gotMonoSamples;
}

int AudioEngine::pullVaxTxMic(float* dst, int n)
{
    // VaxTxMicSource → CompositeTxMicRouter → TxChannel mic input.
    // Closes the AudioEngine.cpp:306 TODO ("pull TX audio from
    // m_vaxTxBus when [...] consumer that pulls from m_vaxTxBus / mic
    // lives — Sub-Phase 9").
    //
    // VAX TX shm is fixed at 48 kHz stereo float32 by the
    // plugin↔CoreAudioHalBus contract (see makePCMFormat in
    // hal-plugin/NereusSDRVAX.cpp + CoreAudioHalBus negotiated
    // format).  We assume that contract here — if a Linux backend
    // ever negotiates a different format, this method will need the
    // same dispatch shape as pullTxMic.
    if (m_vaxTxBus == nullptr || dst == nullptr || n <= 0) {
        return 0;
    }
    if (!m_vaxTxBus->isOpen()) {
        return 0;
    }

    constexpr int kVaxTxChannels        = 2;
    constexpr int kVaxTxBytesPerSample  = static_cast<int>(sizeof(float));
    constexpr int kVaxTxBytesPerFrame   = kVaxTxChannels * kVaxTxBytesPerSample;

    // To produce n mono output samples we pull n stereo frames.
    const qint64 needBytes = static_cast<qint64>(n) * kVaxTxBytesPerFrame;

    // thread_local scratch avoids heap allocation on every audio-thread call.
    // Grows once per thread; zero-alloc thereafter.
    static thread_local std::vector<char> scratch;
    if (static_cast<qint64>(scratch.size()) < needBytes) {
        scratch.resize(static_cast<size_t>(needBytes));
    }

    const qint64 gotBytes = m_vaxTxBus->pull(scratch.data(), needBytes);
    if (gotBytes <= 0) {
        return 0;
    }

    const int gotFrames = static_cast<int>(gotBytes / kVaxTxBytesPerFrame);
    const float* src = reinterpret_cast<const float*>(scratch.data());

    // Stereo → mono via 0.5 * (L + R).  Apps writing to "NereusSDR
    // TX" send stereo (FreeDV/WSJT-X usually mirror to both channels);
    // averaging is the conservative downmix that preserves level when
    // both channels carry the same content and avoids one-sided clipping
    // when only one channel is active.
    for (int i = 0; i < gotFrames; ++i) {
        const float l = src[i * 2];
        const float r = src[i * 2 + 1];
        dst[i] = 0.5f * (l + r);
    }
    return gotFrames;
}

void AudioEngine::setVolume(float volume)
{
    volume = std::clamp(volume, 0.0f, 1.0f);
    // acq_rel pairs with the DSP-thread acquire load in rxBlockReady on
    // weak memory models (ARM / Apple Silicon); release alone would not
    // synchronize the read-side observation order here.
    const float prev = m_masterVolume.exchange(volume, std::memory_order_acq_rel);
    if (prev != volume) {
        emit volumeChanged(volume);
    }
}

void AudioEngine::setMasterMuted(bool muted)
{
    // Same acq_rel / acquire pairing as setVolume above — the
    // DSP-thread read in rxBlockReady uses acquire; a plain release
    // would not synchronize the read-side observation order on weak
    // memory models.
    const bool prev = m_masterMuted.exchange(muted, std::memory_order_acq_rel);
    if (prev == muted) {
        return;
    }

    // Issue #201 (macOS Intel / Core Audio): on the false→true
    // transition, drop any samples already queued in the speakers bus's
    // internal ring.  rxBlockReady's mute gate stops NEW pushes, but
    // the PortAudio output ring (1 s capacity — see PortAudioBus.cpp)
    // can hold up to a second of pre-mute audio that would otherwise
    // keep playing out the device after the click, surfacing as a
    // ~1 s "echo" tail.  The flush atomically equalizes the ring's
    // read/write cursors so the next paCallback iteration sees no
    // unread samples and outputs silence.  No flush on unmute — the
    // DSP thread resuming pushes is enough, and a flush there would
    // create an audible click.
    //
    // Hold m_speakersBusMutex around the flush so a concurrent
    // setSpeakersConfig (which tears down + rebuilds m_speakersBus)
    // can't free the bus out from under us.  rxBlockReady try_locks
    // this same mutex; ≤1 ms of dropped speakers-push from contention
    // here is inaudible (and only happens on the rare race of mute +
    // device-reconfigure simultaneously).
    if (muted) {
        std::lock_guard<std::mutex> lk(m_speakersBusMutex);
        if (m_speakersBus) {
            m_speakersBus->flush();
        }
    }

    emit masterMutedChanged(muted);
}

// Plan: 3M-1b E.4. Pre-code review §10.3 + §10.4.
void AudioEngine::setMoxState(bool active)
{
    // Same acq_rel / acquire pairing as setMasterMuted above — the
    // DSP-thread read in rxBlockReady uses acquire; a plain release
    // would not synchronize the read-side observation order on weak
    // memory models (ARM / Apple Silicon).
    //
    // Wired by RadioModel (Phase L) to MoxController::moxStateChanged.
    // No change-signal emitted — MOX state is authoritative in MoxController;
    // this is a cross-thread mirror only.
    m_moxActive.store(active, std::memory_order_release);

    // Take the gated slice out of the mixer's readiness barrier for the
    // duration of the transmission, and put it back afterwards.
    //
    // This is where Thetis does it too: console.cs:27650-27771 [v2.10.3.15]
    // calls SetAAudioMixStates on every MOX transition, dropping RX1 / RX1S /
    // RX2 from the mix and restoring them on unkey. Doing it here rather than
    // on the audio thread is what makes that possible for us: setSliceStreaming
    // takes the slice-map mutex, and CLAUDE.md forbids locking in rxBlockReady.
    //
    // Without this, rxBlockReady's MOX gate would stop feeding a slice that is
    // still a barrier member, and the drain would wait on it for the whole
    // transmission, silencing every other slice. The earlier fix for that fed
    // the gated slice silence from the audio thread instead, which worked but
    // churned the slice's ring on every transition (ensureRing, drop-oldest,
    // and up to kRingBlocks of stale silence queued ahead of the real audio on
    // unkey). Withdrawing membership leaves the ring completely untouched.
    // Remember exactly which slice was withdrawn and re-admit that same one,
    // rather than re-reading activeSlice() on the way out. The active slice
    // can change during a transmission (TX handoff, a pan closing), and
    // withdrawing X then re-admitting Y would strand X out of the mix with
    // nothing to put it back: it would stay silent until something else
    // re-admitted it, which is what "does not restore until the flag is
    // retuned" looks like from the operator's seat.
    //
    // Routed through AudioEngine::setSliceStreaming rather than straight at
    // m_masterMix, so the anti-VOX mixer is withdrawn and re-admitted in the
    // same breath. Poking one mixer directly is how the two would drift
    // apart, and a slice left enrolled in the anti-VOX barrier alone would
    // wedge that mix for the whole transmission.
    // Release whatever is currently withdrawn FIRST, on both edges, and only
    // then take a new withdrawal. Not just tidiness: moxStateChanged does not
    // alternate, and the earlier `if (active) ... else ...` shape assumed it
    // did.
    //
    // MoxController commits m_mox synchronously but emits moxStateChanged at
    // the END of a timer walk, and stopAllTimers() (MoxController.cpp:552)
    // cancels a pending emit when the next transition arrives before the walk
    // finishes. Re-keying inside the 30 ms release window therefore delivers
    // two `true` edges with no `false` between them. Clearing
    // m_moxWithdrawnSlice on the second one and re-reading activeSlice() then
    // stranded the first slice: withdrawn from both readiness barriers with
    // nothing left to put it back, and the barriers have no timeout that
    // would release it (MasterMixer.h, divergence 3). It stayed out for the
    // life of the process, so the mix stopped being paced by it and its
    // contribution went clumped -- the artefact the barrier exists to
    // prevent.
    //
    // Upstream cannot reach this because it does not track a delta at all.
    // UpdateAAudioMixerStates (console.cs:27631 [v2.10.3.15]) restates
    // ABSOLUTE membership for every stream on every transition --
    //   SetAAudioMixStates(ptr, id, streams, states)   cmaster.cs:255
    // takes an addressed-stream mask AND a desired-state mask -- and
    // chkMOX_CheckedChanged2 calls it on both edges (console.cs:29630 RX->TX,
    // console.cs:29672 TX->RX [v2.10.3.15]). Releasing before taking is the
    // one-slot equivalent: correct for any edge sequence, not only for a
    // strictly alternating one.
    if (m_moxWithdrawnSlice >= 0) {
        setSliceStreaming(m_moxWithdrawnSlice, true);
        m_moxWithdrawnSlice = -1;
    }

    if (active && m_radio != nullptr) {
        if (SliceModel* slice = m_radio->activeSlice()) {
            m_moxWithdrawnSlice = slice->sliceIndex();
            setSliceStreaming(m_moxWithdrawnSlice, false);
        }
    }
}

void AudioEngine::onActiveSliceChanged()
{
    // The withdrawal above samples activeSlice() at the edge; rxBlockReady's
    // gate reads isActiveSlice() on every block. Moving the active slice
    // mid-over separates the two: the newly active slice is gated and stops
    // feeding, but it was never withdrawn, so it stays a barrier member with
    // nothing queued and the drain waits on it (MasterMixer.cpp tryDrain).
    // Meanwhile the slice that WAS withdrawn is feeding again but is not a
    // member. Measured cost on a two-slice bench harness: the mix drops from
    // one push per period to none for the rest of the transmission.
    //
    // Re-running the same release-then-take keeps the withdrawal pointed at
    // whatever the gate is currently silencing. This is the half of
    // UpdateAAudioMixerStates (console.cs:27631 [v2.10.3.15]) that a one-slot
    // delta cannot get for free: upstream recomputes absolute membership for
    // every stream from current conditions each time it is called, so its
    // masks cannot drift away from what is actually being fed.
    if (!m_moxActive.load(std::memory_order_acquire)) {
        return;  // nothing withdrawn while MOX is off
    }
    setMoxState(true);
}

// Plan: 3M-1b E.2. Pre-code review §4.4.
void AudioEngine::setTxMonitorEnabled(bool enabled)
{
    // Same acq_rel / acquire pairing as setMasterMuted above — the
    // audio-thread read in E.3's txMonitorBlockReady uses acquire; a
    // plain release would not synchronize on weak memory models (ARM /
    // Apple Silicon).
    const bool prev = m_txMonitorEnabled.exchange(enabled, std::memory_order_acq_rel);
    if (prev == enabled) {
        return;  // idempotent
    }
    emit txMonitorEnabledChanged(enabled);
}

// Plan: 3M-1b E.2 + E.3. Pre-code review §4.4.
void AudioEngine::setTxMonitorVolume(float volume)
{
    const float clamped = std::clamp(volume, 0.0f, 1.0f);
    // Same acq_rel / acquire pairing as setVolume above.
    const float prev = m_txMonitorVolume.exchange(clamped, std::memory_order_acq_rel);
    if (prev == clamped) {
        return;  // idempotent (float == compared after clamp)
    }
    // Push the new gain into MasterMixer so accumulate() picks it up on
    // the next audio-thread call. setSliceGain acquires m_sliceMapMutex
    // (main-thread only; not called from the audio callback).
    m_masterMix.setSliceGain(kTxMonitorSlotId, clamped, 0.0f);
    emit txMonitorVolumeChanged(clamped);
}

// Plan: 3M-1b E.3. Pre-code review §4.3 + §4.4.
//
// Receives the TXA Sip1 siphon output from TxChannel::sip1OutputReady via
// Qt::DirectConnection (audio thread, same callsite as rxBlockReady). When
// TX monitor is enabled, expands the mono TXA block to interleaved stereo
// (L = R = each sample) and accumulates into MasterMixer at kTxMonitorSlotId.
// The per-slot gain is maintained by setTxMonitorVolume via
// MasterMixer::setSliceGain; accumulate() reads it atomically, so no
// per-sample multiply is needed here.
//
// MasterMixer::mixInto() is called as usual from rxBlockReady; the TX-monitor
// contribution is included in the next RX flush (or as soon as mixInto() is
// called by whichever RX block arrives first). At typical SSB block sizes the
// two are synchronised; a small (~1 block) latency is acceptable and matches
// Thetis's aaudio asynchronous mix path.
//
// RT-safety contract:
//   - atomic acquire loads for m_txMonitorEnabled; no lock, no alloc.
//   - thread_local scratch vector for the stereo expansion; zero-alloc after
//     the first call from a given thread.
//   - MasterMixer::accumulate() is lock-free on the audio thread (map is
//     structurally stable after ctor pre-registration; gains are atomics).
void AudioEngine::txMonitorBlockReady(const float* samples, int frames)
{
    if (!m_txMonitorEnabled.load(std::memory_order_acquire)) {
        return;
    }
    if (samples == nullptr || frames <= 0) {
        return;
    }

    // Expand mono TXA samples to interleaved stereo (L=R) so MasterMixer
    // sees the same format as RX blocks. thread_local so no allocation after
    // the first block per DSP thread.
    static thread_local std::vector<float> stereoScratch;
    const int stereoFloats = frames * 2;
    if (static_cast<int>(stereoScratch.size()) < stereoFloats) {
        stereoScratch.resize(static_cast<size_t>(stereoFloats));
    }
    for (int i = 0; i < frames; ++i) {
        stereoScratch[i * 2 + 0] = samples[i];  // L
        stereoScratch[i * 2 + 1] = samples[i];  // R
    }

    // Accumulate into MasterMixer. The slot's gain (= m_txMonitorVolume)
    // was written by setTxMonitorVolume via setSliceGain and is read
    // atomically inside accumulate(). No separate multiply needed here.
    m_masterMix.accumulate(kTxMonitorSlotId, stereoScratch.data(), frames);
}

void AudioEngine::setVaxRxGain(int channel, float gain)
{
    if (channel < 1 || channel > 4) {
        return;
    }
    gain = std::clamp(gain, 0.0f, 1.0f);
    const int idx = channel - 1;
    // Same acq_rel handshake as setVolume — pairs with the DSP-thread
    // acquire load in rxBlockReady.
    const float prev =
        m_vaxRxGain[idx].exchange(gain, std::memory_order_acq_rel);
    if (prev != gain) {
        emit vaxRxGainChanged(channel, gain);
    }
}

void AudioEngine::setVaxMuted(int channel, bool muted)
{
    if (channel < 1 || channel > 4) {
        return;
    }
    const int idx = channel - 1;
    const bool prev =
        m_vaxMuted[idx].exchange(muted, std::memory_order_acq_rel);
    if (prev != muted) {
        emit vaxMutedChanged(channel, muted);
    }
}

void AudioEngine::setVaxTxGain(float gain)
{
    gain = std::clamp(gain, 0.0f, 1.0f);
    const float prev = m_vaxTxGain.exchange(gain, std::memory_order_acq_rel);
    if (prev != gain) {
        emit vaxTxGainChanged(gain);
    }
}

float AudioEngine::vaxRxGain(int channel) const
{
    if (channel < 1 || channel > 4) {
        return 0.0f;
    }
    return m_vaxRxGain[channel - 1].load(std::memory_order_acquire);
}

bool AudioEngine::vaxMuted(int channel) const
{
    if (channel < 1 || channel > 4) {
        return false;
    }
    return m_vaxMuted[channel - 1].load(std::memory_order_acquire);
}

float AudioEngine::vaxRxLevel(int channel) const
{
    if (channel < 1 || channel > 4) {
        return 0.0f;
    }
    const IAudioBus* bus = m_vaxBus[channel - 1].get();
    if (bus == nullptr || !bus->isOpen()) {
        return 0.0f;
    }
    return bus->rxLevel();
}

bool AudioEngine::isVaxBusOpen(int channel) const
{
    if (channel < 1 || channel > 4) {
        return false;
    }
    const IAudioBus* bus = m_vaxBus[channel - 1].get();
    return bus != nullptr && bus->isOpen();
}

float AudioEngine::vaxTxLevel() const
{
    const IAudioBus* bus = m_vaxTxBus.get();
    if (bus == nullptr || !bus->isOpen()) {
        return 0.0f;
    }
    return bus->txLevel();
}

// ── PC Mic input level (3M-1b I.2) ───────────────────────────────────────────
//
// Provides a peak-amplitude readout from the TX-input bus (the PC's capture
// device, owned by m_txInputBus).  The PortAudioBus audio callback updates
// m_txLevel (std::atomic<float>) each callback cycle; this accessor reads
// it lock-free from the main thread.
//
// Returns 0.0f when m_txInputBus is null (mic not configured) or the bus is
// not open (stream not started yet, or startup failed).
//
// Used by AudioTxInputPage's Test Mic VU bar (I.2) to show live mic level
// without opening a separate capture stream.

float AudioEngine::pcMicInputLevel() const
{
    const IAudioBus* bus = m_txInputBus.get();
    if (bus == nullptr || !bus->isOpen()) {
        return 0.0f;
    }
    return bus->txLevel();
}

// Sub-Phase 12 Task 12.4 — DSP sample-rate / block-size persistence.
// ---------------------------------------------------------------------------

void AudioEngine::setDspSampleRate(int rate)
{
    AppSettings::instance().setValue(QStringLiteral("audio/DspRate"),
                                     QString::number(rate));
    // TODO(sub-phase-12-dsp-live-apply): delegate to WdspEngine once
    // channel teardown/rebuild infrastructure is available.
    qCInfo(lcAudio) << "DspRate change queued — applied on next channel rebuild"
                    << "(requested:" << rate << "Hz)";
    emit dspSampleRateChanged(rate);
}

void AudioEngine::setDspBlockSize(int blockSize)
{
    AppSettings::instance().setValue(QStringLiteral("audio/DspBlockSize"),
                                     QString::number(blockSize));
    // TODO(sub-phase-12-dsp-live-apply): delegate to WdspEngine once
    // channel teardown/rebuild infrastructure is available.
    qCInfo(lcAudio) << "DspBlockSize change queued — applied on next channel rebuild"
                    << "(requested:" << blockSize << ")";
    emit dspBlockSizeChanged(blockSize);
}

// Sub-Phase 12 Task 12.4 — VAC feedback-loop tuning persistence.
// ---------------------------------------------------------------------------

void AudioEngine::setVacFeedbackParams(int channel, const VacFeedbackParams& params)
{
    if (channel < 1 || channel > 4) {
        qCWarning(lcAudio) << "setVacFeedbackParams: channel" << channel
                           << "out of range (1..4) — ignored";
        return;
    }
    const QString prefix =
        QStringLiteral("audio/VacFeedback/%1").arg(channel);
    auto& s = AppSettings::instance();
    s.setValue(prefix + QStringLiteral("/Gain"),
               QString::number(static_cast<double>(params.gain), 'f', 4));
    s.setValue(prefix + QStringLiteral("/SlewTimeMs"),
               QString::number(params.slewTimeMs));
    s.setValue(prefix + QStringLiteral("/PropRing"),
               QString::number(params.propRing));
    s.setValue(prefix + QStringLiteral("/FfRing"),
               QString::number(params.ffRing));
    // TODO(sub-phase-12-vac-feedback-live-apply): wire into IVAC engine once
    // Phase 3M IVAC port lands.
    qCInfo(lcAudio) << "VacFeedback params persisted; live-apply deferred to Phase 3M IVAC port"
                    << "(channel:" << channel
                    << "gain:" << params.gain
                    << "slewTimeMs:" << params.slewTimeMs
                    << "propRing:" << params.propRing
                    << "ffRing:" << params.ffRing << ")";
}

// Sub-Phase 12 Task 12.4 — resetAudioSettings (addendum §2.5).
// ---------------------------------------------------------------------------

void AudioEngine::resetAudioSettings()
{
    auto& s = AppSettings::instance();
    const QStringList keys = s.allKeys();

    // Delete all audio/* keys (addendum §2.5).
    // slice/<N>/VaxChannel and tx/OwnerSlot are implicitly preserved because
    // they live under the "slice/" and "tx/" namespaces — no key can start
    // with both "audio/" and "slice/" simultaneously, so no explicit exclusion
    // guard is needed here.
    for (const QString& key : keys) {
        if (key.startsWith(QStringLiteral("audio/"))) {
            s.remove(key);
        }
    }

    // Force a speakers-bus rebuild from defaults.  ensureSpeakersOpen()
    // early-exits when the bus is already open, so the previous call here
    // left the pre-reset device/config live at runtime until the next app
    // restart.  setSpeakersConfig(empty) goes through applySpeakersConfig
    // which tears down and rebuilds under m_speakersBusMutex, and emits
    // speakersConfigChanged so MasterOutputWidget + AudioDevicesPage
    // refresh.  Empty deviceName → makeBus opens PortAudio's platform
    // default, matching addendum §2.5 "rebuild buses from seeded defaults".
    setSpeakersConfig(AudioDeviceConfig{});

    // Rebuild each VAX bus as well — previously we only emitted the config-
    // changed signal, but rxBlockReady kept pushing audio to whatever bus
    // was live pre-reset (stale BYO PortAudio bus, or the prior native HAL
    // bus tied to the wiped settings).  Mirror the setVaxConfig native-HAL
    // fallback contract: on Mac/Linux re-mint the platform HAL bus; on
    // Windows leave the slot null until the user picks a device.
    for (int ch = 1; ch <= 4; ++ch) {
        const int idx = ch - 1;
        m_vaxBus[idx].reset();
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        m_vaxBus[idx] = makeVaxBus(ch);
        if (m_vaxBus[idx]) {
            qCInfo(lcAudio) << "VAX" << ch
                            << "bus restored to native HAL (reset)"
                            << "[" << m_vaxBus[idx]->backendName() << "]";
        }
#endif
        emit vaxConfigChanged(ch, AudioDeviceConfig{});
    }

    emit audioSettingsReset();

    qCInfo(lcAudio) << "Audio settings reset to defaults (all audio/* keys cleared)";
}

// ---------------------------------------------------------------------------
// Flow-state FSM (sub-PR-2 B.4)
//
// Tracks the audio pipeline health for the ConnectionSegment ♪ pip.
// Production wiring of QAudioSink::stateChanged → setFlowState lands with
// the segment integration in sub-PR-4 / D.2.
// ---------------------------------------------------------------------------

void AudioEngine::setFlowState(FlowState s)
{
    if (m_flowState == s) { return; }
    m_flowState = s;
    emit flowStateChanged(s);
}

void AudioEngine::simulateSuccessfulFeed()
{
    m_successiveUnderruns = 0;
    setFlowState(FlowState::Healthy);
}

void AudioEngine::simulateUnderrun()
{
    m_successiveUnderruns++;
    if (m_successiveUnderruns >= 3) {
        setFlowState(FlowState::Stalled);
    } else {
        setFlowState(FlowState::Underrun);
    }
}

void AudioEngine::simulatePersistentUnderrun()
{
    m_successiveUnderruns = 3;
    setFlowState(FlowState::Stalled);
}

} // namespace NereusSDR
