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
// =================================================================

#include "AudioEngine.h"

#include "AppSettings.h"
#include "LogCategories.h"
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

void AudioEngine::start()
{
    if (m_running) {
        return;
    }

    // Pre-register sliceId 0 with MasterMixer at startup, before any
    // audio-thread accumulate() can race against a main-thread
    // unordered_map insert+rehash on first-block (design-decision D6,
    // plan §Sub-Phase 4 Task 4.1). Multi-slice enrollment is a
    // Sub-Phase 9+ concern; slice-0 covers the single-RX live path.
    if (!m_slicePreregistered) {
        m_masterMix.setSliceGain(0, 1.0f, 0.0f);
        m_slicePreregistered = true;
    }

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
    SliceModel* slice = m_radio->sliceAt(sliceId);
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
    if (m_moxActive.load(std::memory_order_acquire) && slice->isActiveSlice()) {
        return;  // silenced — active TX slice's RX audio gated during MOX
    }

    if (!slice->muted()) {
        m_masterMix.accumulate(sliceId, samples, frames);
    }

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
                const float gain =
                    m_vaxRxGain[vaxIdx].load(std::memory_order_acquire);
                const qint64 payloadBytes =
                    static_cast<qint64>(frames) * 2 * sizeof(float);
                if (gain == 1.0f) {
                    // Fast path — raw samples, zero copy.
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
    const int stereoFloats = frames * 2;
    if (static_cast<int>(mix.size()) < stereoFloats) {
        mix.resize(static_cast<size_t>(stereoFloats));
    }
    m_masterMix.mixInto(mix.data(), frames);

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
    // logging — RT-safety preserved.
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
    if (prev != muted) {
        emit masterMutedChanged(muted);
    }
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
