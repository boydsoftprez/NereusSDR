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
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave RD-I10: the VAX tee try-locks
//                                    the channel's bus lock and skips the
//                                    push while another thread holds it.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec: setRadioOutputTap, the
//                                    station's program at the master
//                                    volume for the radio's own speaker
//                                    out. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec (JJ's ruling): the radio
//                                    tap takes the mixer's radio sum,
//                                    every receiving slice whichever
//                                    device owns it, as Thetis's mixer 0.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 6: the AF
//                                    level is applied in the mix to the
//                                    controller's sums (JJ's ruling, a
//                                    departure from Thetis radio.cs, which
//                                    sets it as WDSP's panel gain); each
//                                    listener hears the slice at its own
//                                    level; VAX and the receiver taps no
//                                    longer undo the AF gain, so VAX stays
//                                    audible at AF 0. AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Task 24: remote-window audio reset
//                                    keeps Core-owned DSP settings.
//                                    AI-assisted implementation via Codex.
//   2026-09-24 : R-R3-45 transmit monitor output by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code. setTxMonitorOutput
//                 picks the speakers or the headphones for MON;
//                 txMonitorBlockReady builds MON into that sum only;
//                 resetAudioSettings puts it back on the speakers.
//   2026-09-24 : setVaxEnabled, setVaxConfig, openVaxOutputSlots,
//                 resetAudioSettings and stop() emit vaxBusOpenChanged
//                 (R-R3-49, R-R3-21) by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 : R-R3-45 fix wave by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. The mix, headphones and program
//                 scratch are engine members sized off the DSP thread
//                 (ensureMixScratchFrames); headphonesEnabledChanged.
//   2026-09-23 : R-R3-45 Task 2 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. The master tap can take the
//                 speakers' mix alone, a headphones-mix tap beside it feeds
//                 a remote window's headphones stream, and remote playback
//                 can play on the headphones output.
//   2026-09-23 : R-R3-45 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Speakers or headphones per receiver (VAX
//                 design 6.2): the master mixer builds both sums, the
//                 headphones output opens at start() when enabled and gets
//                 its own format converter, master volume and mute stay on
//                 the speakers (design 6.3), the remote program tap carries
//                 both sums, and the anti-VOX reference leaves out receivers
//                 on the headphones (their audio is not in the room).
//   2026-09-23 : R3 receiver audio fix wave follow-up by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//                 setDeviceBusFactoryForTest(); in a test run with no
//                 test device the engine opens no real PortAudio or VAX
//                 device.
//   2026-09-23 : R-R3-43 Task 2 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Per-slice receiver audio taps
//                 beside the VAX tee; the VAX tee undoes the feeding
//                 slice's own AF gain, not receiver 1's.
//   2026-09-22 : R-R3-36 fix wave by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. isCaptureReaderOpen() for the MOX
//                 admission check.
//   2026-09-22 : R-R3-36 Task 5 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. ensureTxInputOpen() removed: start()
//                 opens no input. The engine owns a CaptureSupervisor built
//                 in the constructor with the persisted audio/TxInput config;
//                 the TX path reads its stable reader, stop() leaves it alone,
//                 and setTxInputConfig() only reconfigures the supervisor.
//   2026-09-22 — R-R3-36 prerequisite by J.J. Boyd (KG4VCF), AI-assisted
//                 via OpenAI Codex. Adds a source-intent-only PC-mic query
//                 while preserving the existing combined readiness query.
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
//   2026-09-23: R-R3-44 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. openVaxOutputs() (a remote window's VAX
//                 outputs without starting the engine), vaxOutputPacing() /
//                 writeVaxOutput() / vaxOutputHasReader() for its VAX
//                 feeder, a per-channel lock around VAX output replacement,
//                 and setVaxOutputsAllowed(false) so nereusd publishes no VAX
//                 devices. The local VAX tee is unchanged.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02, ruling 5.14) by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. The
//                 local VAX tee skips a slice setVaxSliceMask() leaves out.
//                 NereusSDR-original.
//   2026-09-25: iPhone app plan Task 76 (R-IOS-31, ruling 9.2) by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. One
//                 mix per owner from the one drain (owner mixes with their
//                 own taps), and a local output mask. NereusSDR-original.
//   2026-09-27: Remote-window parity Task 32 (R-IOS-13, R-R3-49) by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code. An
//                 owner mix may carry the transmit monitor (its speakers or
//                 headphones sum), and the local outputs may leave it out
//                 while a remote device holds transmit. The MOX-gated
//                 slice's block drains the mix too (drainMixes, split out
//                 of rxBlockReady unchanged), so MON is heard when that is
//                 the only slice. NereusSDR-original.
//   2026-09-27: R-R3-49 by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. rxBlockReady reads the slice's mute, route
//                 and VAX channel from setSliceAudioView's per-id atomic
//                 word instead of RadioModel::sliceById (a use-after-free
//                 when a slice was removed while audio ran).
//                 NereusSDR-original.
// =================================================================

#include "AudioEngine.h"
#include "core/settings/SettingsScope.h"

#include "AppSettings.h"
#include "LogCategories.h"
#include "WdspEngine.h"       // rxChannel(0) lookup
#include "audio/CaptureAudioBus.h"
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

#include <QCoreApplication>
#include <QScopeGuard>
#include <QStandardPaths>

#include <portaudio.h>

#include <algorithm>
#include <bit>
#include <array>
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

// R-R3-21: process-wide Pa_Initialize / Pa_Terminate calls by any
// AudioEngine, read by the test seam below.
std::atomic<int> g_paInitializeCalls{0};
std::atomic<int> g_paTerminateCalls{0};

} // namespace

#ifdef NEREUS_BUILD_TESTS
int AudioEngine::paInitializeCallsForTest()
{
    return g_paInitializeCalls.load(std::memory_order_relaxed);
}

int AudioEngine::paTerminateCallsForTest()
{
    return g_paTerminateCalls.load(std::memory_order_relaxed);
}
#endif

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
    // R-R3-45 fix wave: the mix scratch rxBlockReady uses, sized here and
    // grown only by ensureMixScratchFrames(), never on the DSP thread.
    m_mixScratch.assign(static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
    m_hpMixScratch.assign(static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
    m_programScratch.assign(static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
    m_radioOutScratch.assign(static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
    m_avMixScratch.assign(static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
    m_vaxScratch.assign(static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
    for (int k = 0; k < kMaxOwnerMixes; ++k) {
        m_ownerSpeakersScratch[static_cast<size_t>(k)].assign(
            static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
        m_ownerHeadphonesScratch[static_cast<size_t>(k)].assign(
            static_cast<size_t>(kMixScratchMinFrames) * 2, 0.0f);
    }
    m_mixScratchFrames.store(kMixScratchMinFrames, std::memory_order_seq_cst);
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
    //
    // R-R3-21: a test run never initialises PortAudio. On Linux its ALSA
    // host API opens every PCM to probe it, and on macOS it walks
    // CoreAudio, so the device guard in makeBus alone still let a test
    // touch the real audio devices. The device layer counts as ready
    // anyway, so the device paths still reach makeBus, which hands out the
    // test's fake devices or nothing (see setDeviceBusFactoryForTest).
    if (PortAudioBus::portAudioBarredForTestRun()) {
        m_paInitialized = false;
        m_deviceLayerReady = true;
        qCInfo(lcAudio) << "PortAudio not initialized: test run";
    } else {
        g_paInitializeCalls.fetch_add(1, std::memory_order_relaxed);
        const PaError err = Pa_Initialize();
        if (err != paNoError) {
            qCWarning(lcAudio) << "Pa_Initialize failed:" << Pa_GetErrorText(err)
                               << "(the audio subsystem will be inert)";
            m_paInitialized = false;
        } else {
            m_paInitialized = true;
            qCInfo(lcAudio) << "PortAudio initialized:" << Pa_GetVersionText();
        }
        m_deviceLayerReady = m_paInitialized;
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

    // R-R3-36: the PC microphone is captured by the supervised helper
    // process, and only while someone holds a capture demand. Creating the
    // supervisor applies the persisted selection without opening anything.
    m_txInputConfig =
        AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/TxInput"));

    // R-R3-45: the headphones selection, opened by start() when enabled.
    m_headphonesConfig =
        AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Headphones"));
    m_headphonesEnabled = AppSettings::instance()
        .value(QStringLiteral("audio/Headphones/Enabled"), QStringLiteral("False"))
        .toString() == QStringLiteral("True");
    // R-R3-45: where MON plays. Speakers unless the operator chose the
    // headphones; this computer's setting, like the device choices.
    m_txMonitorToHeadphones.store(
        AppSettings::instance()
            .value(QStringLiteral("audio/TxMonitor/Output"), QStringLiteral("Speakers"))
            .toString() == QStringLiteral("Headphones"),
        std::memory_order_release);
    // The supervisor needs an application object: its I/O thread runs an
    // event loop and its status reaches this thread through queued calls.
    // Every process that captures has one; an engine built without one
    // (application-less unit tests) has no capture input, exactly like an
    // engine whose capture never became Ready.
    if (QCoreApplication::instance() != nullptr) {
        installCaptureSupervisor({});
    }

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
    // Bounded (stop deadline plus slack); kills a helper that ignores Stop.
    if (m_captureSupervisor) {
        m_captureSupervisor->shutdown();
    }
    if (m_paInitialized) {
        g_paTerminateCalls.fetch_add(1, std::memory_order_relaxed);
        Pa_Terminate();
        m_paInitialized = false;
    }
}

void AudioEngine::setRadioModel(RadioModel* radio)
{
    m_radio = radio;
}

// R-R3-49: one word per slice id, so a block reads a slice's whole view in
// one lock-free load and never sees half of one publish. Slice control plan
// Task 6: 64 bits now, the AF level's float bits in the high half.
namespace {
constexpr quint64 kViewPresent = 1u << 0;
constexpr quint64 kViewMuted = 1u << 1;
constexpr quint64 kViewHeadphones = 1u << 2;
constexpr int kViewVaxShift = 3;
constexpr quint64 kViewVaxMask = 0x7u;
constexpr int kViewAfShift = 32;
static_assert(std::atomic<quint64>::is_always_lock_free,
              "the audio thread reads the slice view without a lock");
}  // namespace

void AudioEngine::setSliceAudioView(int sliceId, const SliceAudioView& view) noexcept
{
    if (sliceId < 0 || sliceId >= kMaxSliceAudioViews) {
        return;
    }
    quint64 word = 0;
    if (view.present) {
        const int vax = (view.vaxChannel >= 1 && view.vaxChannel <= 4) ? view.vaxChannel : 0;
        // NaN reads as 0 (std::clamp would pass it through).
        const float af = view.afGain >= 0.0f ? std::min(view.afGain, 1.0f) : 0.0f;
        word = kViewPresent | (view.muted ? kViewMuted : 0u)
            | (view.headphones ? kViewHeadphones : 0u)
            | (static_cast<quint64>(vax) << kViewVaxShift)
            | (static_cast<quint64>(std::bit_cast<quint32>(af)) << kViewAfShift);
    }
    m_sliceAudioViews[static_cast<size_t>(sliceId)].store(word, std::memory_order_release);
}

AudioEngine::SliceAudioView AudioEngine::sliceAudioView(int sliceId) const noexcept
{
    SliceAudioView view;
    if (sliceId < 0 || sliceId >= kMaxSliceAudioViews) {
        return view;
    }
    const quint64 word =
        m_sliceAudioViews[static_cast<size_t>(sliceId)].load(std::memory_order_acquire);
    view.present = (word & kViewPresent) != 0;
    view.muted = (word & kViewMuted) != 0;
    view.headphones = (word & kViewHeadphones) != 0;
    view.vaxChannel = static_cast<int>((word >> kViewVaxShift) & kViewVaxMask);
    view.afGain = view.present
        ? std::bit_cast<float>(static_cast<quint32>(word >> kViewAfShift))
        : 1.0f;
    return view;
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
    if (!streaming) {
        // Close first so no new audio callback can enter behind the
        // invalidation and escape the acknowledgment wait below.
        m_mixAdmissionClosed.store(true, std::memory_order_seq_cst);
    }

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

    // R-R3-44 (fix wave): the VAX channel mixes wait on their slices the
    // same way, so a withdrawn slice must leave its channel's mix too, or a
    // channel it shares would stop for the whole withdrawal.
    m_vaxMix.setSliceStreaming(sliceId, streaming);

#ifdef NEREUS_BUILD_TESTS
    if (!streaming && m_withdrawalPublishedHookForTest) {
        m_withdrawalPublishedHookForTest();
    }
#endif

    if (!streaming) {
        unsigned inFlight =
            m_mixRegionsInFlight.load(std::memory_order_seq_cst);
        while (inFlight != 0) {
            m_mixRegionsInFlight.wait(inFlight,
                                      std::memory_order_seq_cst);
            inFlight =
                m_mixRegionsInFlight.load(std::memory_order_seq_cst);
        }
        m_mixAdmissionClosed.store(false, std::memory_order_seq_cst);
    }
}

void AudioEngine::start()
{
    if (m_running) {
        return;
    }

#ifdef NEREUS_BUILD_TESTS
    if (m_startInitializerForTest) {
        m_startInitializerForTest(*this);
    }
#endif

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
    // R-R3-45 fix wave: the mix scratch covers the configured DSP block.
    ensureMixScratchFrames(AppSettings::instance()
                               .value(QStringLiteral("audio/DspBlockSize"), QStringLiteral("0"))
                               .toString().toInt());

    ensureSpeakersOpen();
    // R-R3-45: the headphones output opens at startup too, when Setup,
    // Audio, Devices has it enabled (audio/Headphones/Enabled). Before this
    // it opened only on a Devices card change and nothing ever fed it.
    if (m_headphonesEnabled
        && !(m_headphonesBus && m_headphonesBus->isOpen())) {
        reopenHeadphones();
    }
    // R-R3-36: no TX input is opened here. PC microphone capture starts only
    // when RadioModel (local session with PC mic selected) or Test Mic
    // acquires a capture demand, and it never blocks this call.

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
    //
    // R-R3-44: the loop moved into openVaxOutputSlots(), which a remote
    // window reaches through openVaxOutputs(); on nereusd
    // (vaxOutputsAllowed() false) it opens nothing, and neither is the VAX
    // TX device opened below.
    openVaxOutputSlots();
    if (!m_vaxOutputsAllowed) {
        qCInfo(lcAudio) << "VAX devices are not published on this computer (Core)";
    }

    if (!m_vaxTxBus && m_vaxOutputsAllowed) {
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
    {
        std::lock_guard<std::mutex> lock(m_speakersBusMutex);
        m_remotePlayback = false;
        m_speakersBus.reset();
    }
    {
        std::lock_guard<std::mutex> lock(m_headphonesBusMutex);
        m_remoteHeadphonesPlayback = false;
        m_headphonesBus.reset();
        configureHeadphonesConverter();
    }
    publishHeadphonesAvailable();
    // Drops only a test-injected TX input. The capture supervisor and its
    // reader outlive stop(): the TX worker may still be reading it until
    // RadioModel has stopped the worker and released its demand (R-R3-36).
    m_txInputBus.reset();
    // Reset VAX TX before iterating m_vaxBus so the close ordering is
    // TX-first then RX-1..4 — symmetric with the start() construction
    // order (RX-1..4 then TX) inverted on teardown, and lets a future
    // TX-poll consumer release any reference to the TX bus before the
    // RX taps come down.
    m_vaxTxBus.reset();
    for (int idx = 0; idx < 4; ++idx) {
        bool wasOpen = false;
        {
            std::lock_guard<std::mutex> lock(m_vaxBusMutex[idx]);
            wasOpen = m_vaxBus[idx] && m_vaxBus[idx]->isOpen();
            m_vaxBus[idx].reset();
        }
        // R-R3-21: an output that was open is closed; announced once the
        // bus lock is released.
        if (wasOpen) { emit vaxBusOpenChanged(idx + 1); }
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
#ifdef NEREUS_BUILD_TESTS
    // Fix wave follow-up: a test never opens this computer's real devices.
    if (m_deviceBusFactoryForTest) {
        std::unique_ptr<IAudioBus> fake = m_deviceBusFactoryForTest(cfg, capture);
        if (!fake || !fake->open(toAudioFormat(cfg))) {
            return nullptr;
        }
        return fake;
    }
    if (QStandardPaths::isTestModeEnabled()) {
        qCInfo(lcAudio) << "Audio device not opened: test run with no test devices";
        return nullptr;
    }
#endif
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
    if (channel < 1 || channel > 4 || !m_vaxOutputsAllowed) {
        return nullptr;
    }
#ifdef NEREUS_BUILD_TESTS
    if (m_vaxBusFactoryForTest) {
        return m_vaxBusFactoryForTest(channel);
    }
    // Fix wave follow-up: a test run never attaches to this computer's
    // real VAX devices (the macOS ring is shared with any NereusSDR here).
    if (QStandardPaths::isTestModeEnabled()) {
        qCInfo(lcAudio) << "VAX device not opened: test run with no test VAX devices";
        return nullptr;
    }
#endif

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
    //
    // R-R3-44: nereusd publishes no VAX device of either direction.
    if (!m_vaxOutputsAllowed) {
        return nullptr;
    }
#ifdef NEREUS_BUILD_TESTS
    if (m_vaxBusFactoryForTest) {
        return m_vaxBusFactoryForTest(0);
    }
    if (QStandardPaths::isTestModeEnabled()) {
        qCInfo(lcAudio) << "VAX TX device not opened: test run with no test VAX devices";
        return nullptr;
    }
#endif
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
    if (!m_deviceLayerReady) {
        return;
    }

    // Sub-Phase 12 Task 12.2: read persisted config instead of hardcoded
    // defaults. On a fresh install with no audio/Speakers/* keys,
    // loadFromSettings returns a default-constructed AudioDeviceConfig
    // (empty deviceName) → makeBus treats it as "platform default" —
    // same behavior as the pre-Sub-Phase-12 code.
    const AudioDeviceConfig cfg =
        AudioDeviceConfig::loadFromSettings(QStringLiteral("audio/Speakers"));

    {
        std::lock_guard<std::mutex> lk(m_speakersBusMutex);
        m_speakersBus = makeBus(cfg, /*capture=*/false);
        configureSpeakersConverter();
    }
    if (m_speakersBus) {
        m_speakersFormat = m_speakersBus->negotiatedFormat();
        qCInfo(lcAudio) << "Speakers bus opened @"
                        << m_speakersFormat.sampleRate << "Hz /"
                        << m_speakersFormat.channels << "ch"
                        << "[" << m_speakersBus->backendName() << "]";
        emit speakersConfigChanged(cfg);
    }
}

void AudioEngine::installCaptureSupervisor(CaptureSupervisor::Options options)
{
    m_captureSupervisor.reset();
    m_captureSupervisor = std::make_unique<CaptureSupervisor>(std::move(options));
    connect(m_captureSupervisor.get(), &CaptureSupervisor::statusChanged,
            this, &AudioEngine::captureStatusChanged);
    m_captureSupervisor->configure(m_txInputConfig);
}

IAudioBus* AudioEngine::txInputSource() const noexcept
{
    if (m_txInputBus) {
        return m_txInputBus.get();
    }
    return m_captureSupervisor ? m_captureSupervisor->reader() : nullptr;
}

AudioDeviceConfig AudioEngine::txInputConfig() const
{
    return m_txInputConfig;
}

CaptureSupervisor::Status AudioEngine::captureStatus() const
{
    return m_captureSupervisor ? m_captureSupervisor->status() : CaptureSupervisor::Status{};
}

bool AudioEngine::isCaptureReaderOpen() const
{
    return m_captureSupervisor != nullptr && m_captureSupervisor->reader()->isOpen();
}

CaptureSupervisor::Lease AudioEngine::acquireCaptureDemand(CaptureSupervisor::Demand demand)
{
    if (!m_captureSupervisor) {
        return CaptureSupervisor::Lease();
    }
    return m_captureSupervisor->acquire(demand);
}

void AudioEngine::retryCapture()
{
    if (m_captureSupervisor) {
        m_captureSupervisor->retry();
    }
}

bool AudioEngine::beginRemotePlayback(QString* error, RemotePlaybackOutput output)
{
    if (output == RemotePlaybackOutput::Headphones) {
        return beginRemoteHeadphonesPlayback(error);
    }
    if (m_running) {
        if (error) { *error = QStringLiteral("Local DSP already owns speaker playback"); }
        return false;
    }
    // No remote worker exists yet. This may emit speakersConfigChanged, so
    // do not hold the bus mutex across opening or that notification.
    ensureSpeakersOpen();
    std::lock_guard<std::mutex> lock(m_speakersBusMutex);
    if (!m_speakersBus || !m_speakersBus->isOpen()) {
        if (error) { *error = QStringLiteral("Could not open the selected speaker device"); }
        return false;
    }
    // R-R3-23: any rate and channel count the Devices page offers. The
    // receiver matches the stream to this rate and mixes it to one channel
    // for a mono device; a format outside that is refused in plain words.
    const AudioFormat format = m_speakersBus->negotiatedFormat();
    if ((format.channels != 1 && format.channels != 2)
        || format.sampleRate < kMinRemotePlaybackRateHz
        || format.sampleRate > kMaxRemotePlaybackRateHz
        || format.sample != AudioFormat::Sample::Float32) {
        if (error) {
            *error = QStringLiteral("Remote audio cannot play on a speaker device set to "
                                    "%1 channels at %2 Hz")
                         .arg(format.channels).arg(format.sampleRate);
        }
        return false;
    }
    if (!m_speakersBus->outputPacing()) {
        if (error) { *error = QStringLiteral("The selected speaker device does not report its playback timing"); }
        return false;
    }
    m_speakersBus->flush();
    m_remotePlaybackFormat = format;
    m_remotePlayback = true;
    return true;
}

bool AudioEngine::beginRemoteHeadphonesPlayback(QString* error)
{
    // R-R3-45: the speakers' rules, on the headphones output.
    if (m_running) {
        if (error) { *error = QStringLiteral("Local DSP already owns headphones playback"); }
        return false;
    }
    // The headphones are opened by the Enabled box, or when a remote window
    // starts (MainWindow); never here, so begin cannot report a device
    // change to the controller that is starting playback.
    std::lock_guard<std::mutex> lock(m_headphonesBusMutex);
    if (!m_headphonesBus || !m_headphonesBus->isOpen()) {
        if (error) {
            *error = m_headphonesEnabled
                ? QStringLiteral("Could not open the selected headphones device")
                : QStringLiteral("No headphones are set up on this computer");
        }
        return false;
    }
    const AudioFormat format = m_headphonesBus->negotiatedFormat();
    if ((format.channels != 1 && format.channels != 2)
        || format.sampleRate < kMinRemotePlaybackRateHz
        || format.sampleRate > kMaxRemotePlaybackRateHz
        || format.sample != AudioFormat::Sample::Float32) {
        if (error) {
            *error = QStringLiteral("Remote audio cannot play on a headphones device set to "
                                    "%1 channels at %2 Hz")
                         .arg(format.channels).arg(format.sampleRate);
        }
        return false;
    }
    if (!m_headphonesBus->outputPacing()) {
        if (error) { *error = QStringLiteral("The selected headphones device does not report its playback timing"); }
        return false;
    }
    m_headphonesBus->flush();
    m_remoteHeadphonesFormat = format;
    m_remoteHeadphonesPlayback = true;
    return true;
}

void AudioEngine::endRemotePlayback(RemotePlaybackOutput output)
{
    if (output == RemotePlaybackOutput::Headphones) {
        std::lock_guard<std::mutex> lock(m_headphonesBusMutex);
        m_remoteHeadphonesPlayback = false;
        m_remoteHeadphonesFormat = AudioFormat{};
        if (m_headphonesBus) { m_headphonesBus->flush(); }
        return;
    }
    std::lock_guard<std::mutex> lock(m_speakersBusMutex);
    m_remotePlayback = false;
    m_remotePlaybackFormat = AudioFormat{};
    if (m_speakersBus) { m_speakersBus->flush(); }
}

std::optional<AudioFormat> AudioEngine::remotePlaybackFormat(RemotePlaybackOutput output)
{
    if (output == RemotePlaybackOutput::Headphones) {
        std::lock_guard<std::mutex> lock(m_headphonesBusMutex);
        if (!m_remoteHeadphonesPlayback) { return std::nullopt; }
        return m_remoteHeadphonesFormat;
    }
    std::lock_guard<std::mutex> lock(m_speakersBusMutex);
    if (!m_remotePlayback) { return std::nullopt; }
    return m_remotePlaybackFormat;
}

std::optional<IAudioBus::OutputPacing> AudioEngine::remotePlaybackPacing(
    RemotePlaybackOutput output)
{
    if (output == RemotePlaybackOutput::Headphones) {
        std::lock_guard<std::mutex> lock(m_headphonesBusMutex);
        if (!m_remoteHeadphonesPlayback || !m_headphonesBus || !m_headphonesBus->isOpen()
            || m_headphonesBus->negotiatedFormat() != m_remoteHeadphonesFormat) {
            return std::nullopt;
        }
        return m_headphonesBus->outputPacing();
    }
    std::lock_guard<std::mutex> lock(m_speakersBusMutex);
    // A device reopened in another format no longer matches the stream the
    // receiver makes for it: no pacing, so the receiver restarts.
    if (!m_remotePlayback || !m_speakersBus || !m_speakersBus->isOpen()
        || m_speakersBus->negotiatedFormat() != m_remotePlaybackFormat) { return std::nullopt; }
    return m_speakersBus->outputPacing();
}

bool AudioEngine::writeRemotePlayback(const QVector<float>& pcm, RemotePlaybackOutput output)
{
    // Bounded worker-side scratch; never called by the device callback.
    // The block is interleaved in the format begin accepted (one or two
    // channels); the channel count is checked again under the lock.
    if (pcm.isEmpty() || pcm.size() > qsizetype(kMaxRemotePlaybackFrames) * 2) { return false; }
    if (output == RemotePlaybackOutput::Headphones) {
        // R-R3-45: no master volume or mute on the headphones (design 6.3).
        for (qsizetype i = 0; i < pcm.size(); ++i) {
            if (!std::isfinite(pcm[i])) { return false; }
        }
        std::lock_guard<std::mutex> lock(m_headphonesBusMutex);
        if (!m_remoteHeadphonesPlayback || !m_headphonesBus || !m_headphonesBus->isOpen()
            || m_headphonesBus->negotiatedFormat() != m_remoteHeadphonesFormat) { return false; }
        const int channels = m_remoteHeadphonesFormat.channels;
        if (channels <= 0 || pcm.size() % channels != 0
            || pcm.size() / channels > kMaxRemotePlaybackFrames) { return false; }
        const int frames = int(pcm.size() / channels);
        const auto pacing = m_headphonesBus->outputPacing();
        if (!pacing || pacing->capacityFrames - pacing->queuedFrames < frames) { return false; }
        const auto bytes = static_cast<qint64>(pcm.size()) * qint64(sizeof(float));
        return m_headphonesBus->push(reinterpret_cast<const char*>(pcm.constData()), bytes)
            == bytes;
    }
    std::array<float, kMaxRemotePlaybackFrames * 2> scaled;
    const float gain = m_masterVolume.load(std::memory_order_acquire);
    for (qsizetype i = 0; i < pcm.size(); ++i) {
        if (!std::isfinite(pcm[i])) { return false; }
        scaled[static_cast<size_t>(i)] = pcm[i] * gain;
    }
    std::lock_guard<std::mutex> lock(m_speakersBusMutex);
    if (!m_remotePlayback || !m_speakersBus || !m_speakersBus->isOpen()
        || m_speakersBus->negotiatedFormat() != m_remotePlaybackFormat) { return false; }
    const int channels = m_remotePlaybackFormat.channels;
    if (channels <= 0 || pcm.size() % channels != 0
        || pcm.size() / channels > kMaxRemotePlaybackFrames) { return false; }
    const int frames = int(pcm.size() / channels);
    if (m_masterMuted.load(std::memory_order_acquire)) { return true; }
    const auto pacing = m_speakersBus->outputPacing();
    if (!pacing || pacing->capacityFrames - pacing->queuedFrames < frames) { return false; }
    const auto bytes = static_cast<qint64>(pcm.size()) * qint64(sizeof(float));
    return m_speakersBus->push(reinterpret_cast<const char*>(scaled.data()), bytes) == bytes;
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
    if (!m_deviceLayerReady) {
        return;
    }

    // Hold the mutex during tear-down + rebuild. rxBlockReady uses
    // try_lock and drops the block if it can't acquire (≤1 ms of silence
    // is inaudible vs. a use-after-free on the old bus pointer).
    std::unique_lock<std::mutex> lk(m_speakersBusMutex);

    m_speakersBus.reset();
    m_speakersBus = makeBus(cfg, /*capture=*/false);
    configureSpeakersConverter();

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
    m_headphonesConfig = cfg;
    if (!m_deviceLayerReady) {
        return;
    }
    // R-R3-45: the device opens only while the headphones are enabled.
    reopenHeadphones();
    emit headphonesConfigChanged(cfg);
}

void AudioEngine::setHeadphonesEnabled(bool enabled)
{
    if (m_headphonesEnabled == enabled
        && (m_headphonesBus != nullptr) == enabled) {
        return;
    }
    const bool changed = m_headphonesEnabled != enabled;
    m_headphonesEnabled = enabled;
    reopenHeadphones();
    if (changed) {
        emit headphonesEnabledChanged(enabled);
    }
}

void AudioEngine::reopenHeadphones()
{
    {
        // Held across tear-down and rebuild; the DSP thread's push uses
        // try_lock and drops a block rather than wait (as the speakers do).
        std::lock_guard<std::mutex> lk(m_headphonesBusMutex);
        m_headphonesBus.reset();
        if (m_headphonesEnabled) {
            m_headphonesBus = makeBus(m_headphonesConfig, /*capture=*/false);
        }
        configureHeadphonesConverter();
    }
    if (m_headphonesBus) {
        qCInfo(lcAudio) << "Headphones bus opened"
                        << "[" << m_headphonesBus->backendName() << "]";
    } else if (m_headphonesEnabled) {
        qCWarning(lcAudio) << "Headphones bus open failed for device"
                           << m_headphonesConfig.deviceName
                           << "- receivers on the headphones are silent";
    }
    publishHeadphonesAvailable();
}

void AudioEngine::publishHeadphonesAvailable()
{
    const bool available = m_headphonesBus && m_headphonesBus->isOpen();
    if (available == m_headphonesAvailable) {
        return;
    }
    m_headphonesAvailable = available;
    emit headphonesAvailableChanged(available);
}

void AudioEngine::configureHeadphonesConverter()
{
    // Caller holds m_headphonesBusMutex.
    if (m_headphonesBus) {
        const AudioFormat format = m_headphonesBus->negotiatedFormat();
        m_headphonesConverter.configure(format.sampleRate, format.channels);
    } else {
        m_headphonesConverter.configure(0, 0);
    }
}

void AudioEngine::setTxInputConfig(const AudioDeviceConfig& cfg)
{
    // R-R3-36: no native bus is reset or opened here, so the TX worker's
    // reader is never replaced under it. The supervisor retires the old
    // input and, if capture is demanded, prepares the new one on its own
    // thread.
    m_txInputConfig = cfg;
    if (m_captureSupervisor) {
        m_captureSupervisor->configure(cfg);
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
    // R-R3-21: the output is replaced (or closed) on every path below;
    // announced once the bus lock is released, whichever branch returns.
    const auto announce = qScopeGuard([this, channel] { emit vaxBusOpenChanged(channel); });
    const int idx = channel - 1;
    // R-R3-44: a remote window's VAX feeder writes this slot from its own
    // worker; it waits while the output is replaced.
    std::unique_lock<std::mutex> busLock(m_vaxBusMutex[idx]);
    m_vaxBus[idx].reset();
    if (!m_vaxOutputsAllowed) {
        return;
    }

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
        busLock.unlock();
        emit vaxConfigChanged(channel, cfg);
        return;
    }
#endif

    if (!m_deviceLayerReady) {
        return;
    }
    m_vaxBus[idx] = makeBus(cfg, /*capture=*/false);
    if (m_vaxBus[idx]) {
        qCInfo(lcAudio) << "VAX" << channel << "bus reconfigured (BYO)"
                        << "[" << m_vaxBus[idx]->backendName() << "]";
    }
    busLock.unlock();
    emit vaxConfigChanged(channel, cfg);
}

void AudioEngine::setVaxEnabled(int channel, bool on)
{
    if (channel < 1 || channel > 4) {
        return;
    }
    // R-R3-21: emitted once the bus lock below is released, whichever
    // branch returns.
    const auto announce = qScopeGuard([this, channel] { emit vaxBusOpenChanged(channel); });
    const int idx = channel - 1;
    // R-R3-44: see setVaxConfig().
    std::lock_guard<std::mutex> busLock(m_vaxBusMutex[idx]);
    if (!on) {
        m_vaxBus[idx].reset();
        return;
    }
    if (!m_vaxOutputsAllowed) {
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
    if (!m_deviceLayerReady) {
        return;
    }
    AudioDeviceConfig defaults;
    m_vaxBus[idx] = makeBus(defaults, /*capture=*/false);
#endif
}

// ---------------------------------------------------------------------------
// R-R3-44: VAX outputs in a remote window, none on the Core
// ---------------------------------------------------------------------------

void AudioEngine::setVaxOutputsAllowed(bool allowed)
{
    m_vaxOutputsAllowed = allowed;
}

void AudioEngine::openVaxOutputs()
{
#ifdef NEREUS_BUILD_TESTS
    // A remote window built by a test (the window harness, the gating
    // sweeps) must not attach to this computer's real VAX devices: the
    // macOS ring is shared with any NereusSDR running here. A test that
    // wants outputs supplies them through setVaxBusFactoryForTest().
    if (QStandardPaths::isTestModeEnabled() && !m_vaxBusFactoryForTest) {
        qCInfo(lcAudio) << "VAX outputs not opened: test run with no test VAX devices";
        return;
    }
#endif
    openVaxOutputSlots();
}

void AudioEngine::openVaxOutputSlots()
{
    for (int channel = 1; channel <= 4; ++channel) {
        const int idx = channel - 1;
        bool opened = false;
        {
            std::lock_guard<std::mutex> busLock(m_vaxBusMutex[idx]);
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
                opened = true;
            }
        }
        // R-R3-21: announced once the bus lock is released.
        if (opened) { emit vaxBusOpenChanged(channel); }
    }
}

std::optional<IAudioBus::OutputPacing> AudioEngine::vaxOutputPacing(int channel)
{
    if (channel < 1 || channel > 4) {
        return std::nullopt;
    }
    std::lock_guard<std::mutex> busLock(m_vaxBusMutex[channel - 1]);
    IAudioBus* bus = m_vaxBus[channel - 1].get();
    if (bus == nullptr || !bus->isOpen()) {
        return std::nullopt;
    }
    return bus->outputPacing();
}

bool AudioEngine::writeVaxOutput(int channel, const float* stereo, int frames)
{
    if (channel < 1 || channel > 4 || stereo == nullptr || frames <= 0
        || frames > kMaxVaxWriteFrames) {
        return false;
    }
    const int idx = channel - 1;
    // The same channel gain and mute the local tee in rxBlockReady applies.
    // The receiver stream already carries the slice's audio with its AF gain
    // undone (feedSliceTaps on the Core), the tee's other factor.
    const bool muted = m_vaxMuted[idx].load(std::memory_order_acquire);
    const float gain = muted ? 0.0f : m_vaxRxGain[idx].load(std::memory_order_acquire);
    // Worker-thread scratch, never an audio callback: 32 KiB on the stack.
    std::array<float, kMaxVaxWriteFrames * 2> scaled;
    const int count = frames * 2;
    for (int i = 0; i < count; ++i) {
        if (!std::isfinite(stereo[i])) {
            return false;
        }
        scaled[static_cast<size_t>(i)] = stereo[i] * gain;
    }
    std::lock_guard<std::mutex> busLock(m_vaxBusMutex[idx]);
    IAudioBus* bus = m_vaxBus[idx].get();
    if (bus == nullptr || !bus->isOpen()) {
        return false;
    }
    const auto bytes = static_cast<qint64>(count) * static_cast<qint64>(sizeof(float));
    return bus->push(reinterpret_cast<const char*>(scaled.data()), bytes) == bytes;
}

std::optional<bool> AudioEngine::vaxOutputHasReader(int channel)
{
    if (channel < 1 || channel > 4) {
        return std::nullopt;
    }
    // Owner thread, the only thread that replaces the output, so no lock:
    // the platform query (CoreAudio's device list) must not hold up a
    // feeder writing this channel.
    const IAudioBus* bus = m_vaxBus[channel - 1].get();
    if (bus == nullptr || !bus->isOpen()) {
        return std::nullopt;
    }
    return bus->outputHasReader();
}

void AudioEngine::configureSpeakersConverter()
{
    // Caller holds m_speakersBusMutex. A closed or missing bus leaves the
    // converter passing through (nothing is pushed to it anyway).
    if (m_speakersBus) {
        const AudioFormat format = m_speakersBus->negotiatedFormat();
        m_speakersConverter.configure(format.sampleRate, format.channels);
    } else {
        m_speakersConverter.configure(0, 0);
    }
}

#ifdef NEREUS_BUILD_TESTS
void AudioEngine::setVaxBusForTest(int channel, std::unique_ptr<IAudioBus> bus)
{
    if (channel < 1 || channel > 4) {
        return;
    }
    std::lock_guard<std::mutex> busLock(m_vaxBusMutex[channel - 1]);
    m_vaxBus[channel - 1] = std::move(bus);
}

void AudioEngine::setSpeakersBusForTest(std::unique_ptr<IAudioBus> bus)
{
    std::lock_guard<std::mutex> lk(m_speakersBusMutex);
    m_speakersBus = std::move(bus);
    configureSpeakersConverter();
}

void AudioEngine::setHeadphonesBusForTest(std::unique_ptr<IAudioBus> bus)
{
    {
        std::lock_guard<std::mutex> lk(m_headphonesBusMutex);
        m_headphonesBus = std::move(bus);
        configureHeadphonesConverter();
    }
    publishHeadphonesAvailable();
}

void AudioEngine::setTxInputBusForTest(std::unique_ptr<IAudioBus> bus)
{
    m_txInputBus = std::move(bus);
}

void AudioEngine::setCaptureSupervisorOptionsForTest(CaptureSupervisor::Options options)
{
    Q_ASSERT_X(!m_captureSupervisor || !m_captureSupervisor->hasDemand(),
               "AudioEngine::setCaptureSupervisorOptionsForTest",
               "capture demand is active");
    installCaptureSupervisor(std::move(options));
}

qint64 AudioEngine::captureHelperProcessIdForTest() const
{
    return m_captureSupervisor ? m_captureSupervisor->helperProcessId() : 0;
}

void AudioEngine::setVaxTxBusForTest(std::unique_ptr<IAudioBus> bus)
{
    m_vaxTxBus = std::move(bus);
}

#endif

void AudioEngine::setMasterMixAudioTap(MasterMixAudioTap* tap, bool speakersOnly)
{
    std::lock_guard<std::mutex> controlLock(m_masterMixTapControlMutex);

    // Close first, then wait for an already-admitted DSP callback.  The
    // callback checks the gate again after incrementing, so one that raced
    // this store cannot invoke a pointer being replaced.
    m_masterMixTapAdmissionClosed.store(true, std::memory_order_seq_cst);
    unsigned calls = m_masterMixTapCallsInFlight.load(std::memory_order_seq_cst);
    while (calls != 0) {
        m_masterMixTapCallsInFlight.wait(calls, std::memory_order_relaxed);
        calls = m_masterMixTapCallsInFlight.load(std::memory_order_seq_cst);
    }
    // R-R3-45: what the tap takes, published with it while no callback runs.
    m_masterMixTapSpeakersOnly.store(speakersOnly, std::memory_order_seq_cst);
    m_masterMixAudioTap.store(tap, std::memory_order_seq_cst);
    m_masterMixTapAdmissionClosed.store(false, std::memory_order_seq_cst);
}

void AudioEngine::setHeadphonesMixAudioTap(MasterMixAudioTap* tap)
{
    // R-R3-45: the master tap's gate, on its own slot.
    std::lock_guard<std::mutex> controlLock(m_headphonesMixTapControlMutex);
    m_headphonesMixTapAdmissionClosed.store(true, std::memory_order_seq_cst);
    unsigned calls = m_headphonesMixTapCallsInFlight.load(std::memory_order_seq_cst);
    while (calls != 0) {
        m_headphonesMixTapCallsInFlight.wait(calls, std::memory_order_relaxed);
        calls = m_headphonesMixTapCallsInFlight.load(std::memory_order_seq_cst);
    }
    m_headphonesMixAudioTap.store(tap, std::memory_order_seq_cst);
    m_headphonesMixTapAdmissionClosed.store(false, std::memory_order_seq_cst);
}

void AudioEngine::clearHeadphonesMixAudioTap(MasterMixAudioTap* tap)
{
    if (tap == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> controlLock(m_headphonesMixTapControlMutex);
    m_headphonesMixTapAdmissionClosed.store(true, std::memory_order_seq_cst);
    unsigned calls = m_headphonesMixTapCallsInFlight.load(std::memory_order_seq_cst);
    while (calls != 0) {
        m_headphonesMixTapCallsInFlight.wait(calls, std::memory_order_relaxed);
        calls = m_headphonesMixTapCallsInFlight.load(std::memory_order_seq_cst);
    }
    MasterMixAudioTap* expected = tap;
    m_headphonesMixAudioTap.compare_exchange_strong(expected, nullptr,
                                                    std::memory_order_seq_cst,
                                                    std::memory_order_seq_cst);
    m_headphonesMixTapAdmissionClosed.store(false, std::memory_order_seq_cst);
}

void AudioEngine::clearMasterMixAudioTap(MasterMixAudioTap* tap)
{
    if (tap == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> controlLock(m_masterMixTapControlMutex);
    m_masterMixTapAdmissionClosed.store(true, std::memory_order_seq_cst);
    unsigned calls = m_masterMixTapCallsInFlight.load(std::memory_order_seq_cst);
    while (calls != 0) {
        m_masterMixTapCallsInFlight.wait(calls, std::memory_order_relaxed);
        calls = m_masterMixTapCallsInFlight.load(std::memory_order_seq_cst);
    }
    MasterMixAudioTap* expected = tap;
    m_masterMixAudioTap.compare_exchange_strong(expected, nullptr,
                                                std::memory_order_seq_cst,
                                                std::memory_order_seq_cst);
    m_masterMixTapAdmissionClosed.store(false, std::memory_order_seq_cst);
}

void AudioEngine::closeAndDrainMixTap(MixTapGate& gate)
{
    // As the master tap: close first, then wait for an admitted callback.
    gate.admissionClosed.store(true, std::memory_order_seq_cst);
    unsigned calls = gate.callsInFlight.load(std::memory_order_seq_cst);
    while (calls != 0) {
        gate.callsInFlight.wait(calls, std::memory_order_relaxed);
        calls = gate.callsInFlight.load(std::memory_order_seq_cst);
    }
}

void AudioEngine::invokeMixTap(MixTapGate& gate, const float* samples, int frames) noexcept
{
    // DSP thread: the master tap's admission, on this gate.
    if (gate.admissionClosed.load(std::memory_order_seq_cst)) {
        return;
    }
    gate.callsInFlight.fetch_add(1, std::memory_order_seq_cst);
    if (!gate.admissionClosed.load(std::memory_order_seq_cst)) {
        MasterMixAudioTap* tap = gate.tap.load(std::memory_order_seq_cst);
        if (tap != nullptr) {
            tap->consume(samples, frames, kMasterMixSampleRateHz);
        }
    }
    if (gate.callsInFlight.fetch_sub(1, std::memory_order_seq_cst) == 1) {
        gate.callsInFlight.notify_all();
    }
}

void AudioEngine::setRadioOutputTap(MasterMixAudioTap* tap)
{
    std::lock_guard<std::mutex> lock(m_radioOutputControlMutex);
    closeAndDrainMixTap(m_radioOutputTap);
    m_radioOutputTap.tap.store(tap, std::memory_order_seq_cst);
    m_radioOutputTap.admissionClosed.store(false, std::memory_order_seq_cst);
}

void AudioEngine::clearRadioOutputTap(MasterMixAudioTap* tap)
{
    if (tap == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_radioOutputControlMutex);
    closeAndDrainMixTap(m_radioOutputTap);
    MasterMixAudioTap* expected = tap;
    m_radioOutputTap.tap.compare_exchange_strong(expected, nullptr, std::memory_order_seq_cst,
                                                 std::memory_order_seq_cst);
    m_radioOutputTap.admissionClosed.store(false, std::memory_order_seq_cst);
}

int AudioEngine::acquireOwnerMix()
{
    std::lock_guard<std::mutex> lock(m_ownerMixControlMutex);
    for (int k = 0; k < kMaxOwnerMixes; ++k) {
        OwnerMixSlot& slot = m_ownerMixes[static_cast<size_t>(k)];
        if (!slot.taken.load(std::memory_order_acquire)) {
            slot.sliceMask.store(0, std::memory_order_release);
            slot.monitor.store(0, std::memory_order_release);
            slot.listenMask.store(0, std::memory_order_release);
            slot.taken.store(true, std::memory_order_release);
            return k;
        }
    }
    return -1;
}

void AudioEngine::releaseOwnerMix(int slot)
{
    if (!validOwnerMixSlot(slot)) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_ownerMixControlMutex);
    OwnerMixSlot& owner = m_ownerMixes[static_cast<size_t>(slot)];
    closeAndDrainMixTap(owner.program);
    owner.program.tap.store(nullptr, std::memory_order_seq_cst);
    owner.program.admissionClosed.store(false, std::memory_order_seq_cst);
    closeAndDrainMixTap(owner.headphones);
    owner.headphones.tap.store(nullptr, std::memory_order_seq_cst);
    owner.headphones.admissionClosed.store(false, std::memory_order_seq_cst);
    owner.sliceMask.store(0, std::memory_order_release);
    owner.monitor.store(0, std::memory_order_release);
    owner.listenMask.store(0, std::memory_order_release);
    owner.taken.store(false, std::memory_order_release);
}

// Slice control plan Task 6: the level lands before the bit, so a drain
// that sees the bit sees a level for it.
void AudioEngine::setOwnerMixListen(int slot, int sliceId, float level, bool muted)
{
    if (!validOwnerMixSlot(slot) || sliceId < 0 || sliceId >= kMaxSliceAudioViews) {
        return;
    }
    OwnerMixSlot& owner = m_ownerMixes[static_cast<size_t>(slot)];
    const float stored = muted || !(level > 0.0f) ? 0.0f : std::min(level, 1.0f);
    owner.listenLevels[static_cast<size_t>(sliceId)].store(stored, std::memory_order_release);
    owner.listenMask.fetch_or(quint32{1} << sliceId, std::memory_order_acq_rel);
}

void AudioEngine::clearOwnerMixListen(int slot, int sliceId)
{
    if (!validOwnerMixSlot(slot) || sliceId < 0 || sliceId >= kMaxSliceAudioViews) {
        return;
    }
    m_ownerMixes[static_cast<size_t>(slot)].listenMask.fetch_and(~(quint32{1} << sliceId),
                                                                 std::memory_order_acq_rel);
}

quint32 AudioEngine::ownerMixListenMask(int slot) const
{
    return validOwnerMixSlot(slot)
        ? m_ownerMixes[static_cast<size_t>(slot)].listenMask.load(std::memory_order_acquire)
        : 0u;
}

float AudioEngine::ownerMixListenLevel(int slot, int sliceId) const
{
    if (!validOwnerMixSlot(slot) || sliceId < 0 || sliceId >= kMaxSliceAudioViews) {
        return 0.0f;
    }
    return m_ownerMixes[static_cast<size_t>(slot)]
        .listenLevels[static_cast<size_t>(sliceId)]
        .load(std::memory_order_acquire);
}

void AudioEngine::setLocalListen(int sliceId, float level, bool muted)
{
    if (sliceId < 0 || sliceId >= kMaxSliceAudioViews) {
        return;
    }
    const float stored = muted || !(level > 0.0f) ? 0.0f : std::min(level, 1.0f);
    m_localListenLevels[static_cast<size_t>(sliceId)].store(stored, std::memory_order_release);
    m_localListenMask.fetch_or(quint32{1} << sliceId, std::memory_order_acq_rel);
}

void AudioEngine::clearLocalListen(int sliceId)
{
    if (sliceId < 0 || sliceId >= kMaxSliceAudioViews) {
        return;
    }
    m_localListenMask.fetch_and(~(quint32{1} << sliceId), std::memory_order_acq_rel);
}

void AudioEngine::setOwnerMixMonitor(int slot, MasterMixer::OwnerMonitor monitor)
{
    if (!validOwnerMixSlot(slot)) {
        return;
    }
    m_ownerMixes[static_cast<size_t>(slot)].monitor.store(static_cast<int>(monitor),
                                                          std::memory_order_release);
}

MasterMixer::OwnerMonitor AudioEngine::ownerMixMonitor(int slot) const
{
    return validOwnerMixSlot(slot)
        ? static_cast<MasterMixer::OwnerMonitor>(
              m_ownerMixes[static_cast<size_t>(slot)].monitor.load(std::memory_order_acquire))
        : MasterMixer::OwnerMonitor::None;
}

void AudioEngine::setOwnerMixSliceMask(int slot, quint32 mask)
{
    if (!validOwnerMixSlot(slot)) {
        return;
    }
    m_ownerMixes[static_cast<size_t>(slot)].sliceMask.store(mask, std::memory_order_release);
}

quint32 AudioEngine::ownerMixSliceMask(int slot) const
{
    return validOwnerMixSlot(slot)
        ? m_ownerMixes[static_cast<size_t>(slot)].sliceMask.load(std::memory_order_acquire)
        : 0u;
}

bool AudioEngine::setOwnerMixAudioTap(int slot, MasterMixAudioTap* tap, bool speakersOnly)
{
    if (!validOwnerMixSlot(slot)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_ownerMixControlMutex);
    OwnerMixSlot& owner = m_ownerMixes[static_cast<size_t>(slot)];
    if (!owner.taken.load(std::memory_order_acquire)) {
        return false;
    }
    closeAndDrainMixTap(owner.program);
    owner.programSpeakersOnly.store(speakersOnly, std::memory_order_seq_cst);
    owner.program.tap.store(tap, std::memory_order_seq_cst);
    owner.program.admissionClosed.store(false, std::memory_order_seq_cst);
    return true;
}

void AudioEngine::clearOwnerMixAudioTap(int slot, MasterMixAudioTap* tap)
{
    if (!validOwnerMixSlot(slot) || tap == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_ownerMixControlMutex);
    OwnerMixSlot& owner = m_ownerMixes[static_cast<size_t>(slot)];
    closeAndDrainMixTap(owner.program);
    MasterMixAudioTap* expected = tap;
    owner.program.tap.compare_exchange_strong(expected, nullptr, std::memory_order_seq_cst,
                                              std::memory_order_seq_cst);
    owner.program.admissionClosed.store(false, std::memory_order_seq_cst);
}

bool AudioEngine::setOwnerHeadphonesMixAudioTap(int slot, MasterMixAudioTap* tap)
{
    if (!validOwnerMixSlot(slot)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_ownerMixControlMutex);
    OwnerMixSlot& owner = m_ownerMixes[static_cast<size_t>(slot)];
    if (!owner.taken.load(std::memory_order_acquire)) {
        return false;
    }
    closeAndDrainMixTap(owner.headphones);
    owner.headphones.tap.store(tap, std::memory_order_seq_cst);
    owner.headphones.admissionClosed.store(false, std::memory_order_seq_cst);
    return true;
}

void AudioEngine::clearOwnerHeadphonesMixAudioTap(int slot, MasterMixAudioTap* tap)
{
    if (!validOwnerMixSlot(slot) || tap == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_ownerMixControlMutex);
    OwnerMixSlot& owner = m_ownerMixes[static_cast<size_t>(slot)];
    closeAndDrainMixTap(owner.headphones);
    MasterMixAudioTap* expected = tap;
    owner.headphones.tap.compare_exchange_strong(expected, nullptr, std::memory_order_seq_cst,
                                                 std::memory_order_seq_cst);
    owner.headphones.admissionClosed.store(false, std::memory_order_seq_cst);
}

int AudioEngine::ownerMixCount() const
{
    std::lock_guard<std::mutex> lock(m_ownerMixControlMutex);
    int count = 0;
    for (const OwnerMixSlot& slot : m_ownerMixes) {
        if (slot.taken.load(std::memory_order_acquire)) {
            ++count;
        }
    }
    return count;
}

void AudioEngine::closeAndDrainSliceTap(SliceTapSlot& slot)
{
    // As the master tap: close first, then wait for an admitted callback.
    // The callback re-checks the gate after incrementing, so one that
    // raced this store cannot use a pointer being replaced.
    slot.admissionClosed.store(true, std::memory_order_seq_cst);
    unsigned calls = slot.callsInFlight.load(std::memory_order_seq_cst);
    while (calls != 0) {
        slot.callsInFlight.wait(calls, std::memory_order_relaxed);
        calls = slot.callsInFlight.load(std::memory_order_seq_cst);
    }
}

bool AudioEngine::setSliceAudioTap(int sliceId, SliceAudioTap* tap)
{
    if (sliceId < 0 || tap == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> controlLock(m_sliceTapControlMutex);
    SliceTapSlot* target = nullptr;
    for (SliceTapSlot& slot : m_sliceTaps) {
        if (slot.tap.load(std::memory_order_seq_cst) == tap) {
            target = &slot;
            break;
        }
    }
    if (target == nullptr) {
        for (SliceTapSlot& slot : m_sliceTaps) {
            if (slot.tap.load(std::memory_order_seq_cst) == nullptr) {
                target = &slot;
                break;
            }
        }
    }
    if (target == nullptr) {
        return false;
    }
    closeAndDrainSliceTap(*target);
    target->sliceId.store(sliceId, std::memory_order_seq_cst);
    target->tap.store(tap, std::memory_order_seq_cst);
    target->admissionClosed.store(false, std::memory_order_seq_cst);
    return true;
}

void AudioEngine::clearSliceAudioTap(SliceAudioTap* tap)
{
    if (tap == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> controlLock(m_sliceTapControlMutex);
    for (SliceTapSlot& slot : m_sliceTaps) {
        if (slot.tap.load(std::memory_order_seq_cst) != tap) {
            continue;
        }
        closeAndDrainSliceTap(slot);
        slot.tap.store(nullptr, std::memory_order_seq_cst);
        slot.sliceId.store(-1, std::memory_order_seq_cst);
        slot.admissionClosed.store(false, std::memory_order_seq_cst);
    }
}

int AudioEngine::sliceAudioTapCount() const
{
    int count = 0;
    for (const SliceTapSlot& slot : m_sliceTaps) {
        if (slot.tap.load(std::memory_order_seq_cst) != nullptr) {
            ++count;
        }
    }
    return count;
}

void AudioEngine::feedSliceTaps(int sliceId, const float* samples, int frames) noexcept
{
    // Slice control plan Task 6: the block as the receiver produced it. The
    // AF level is applied in the mix, after this point, so there is no AF
    // gain to undo here any more (the 1 / AF scaling is gone). A block no
    // tap wants costs four atomic loads.
    for (SliceTapSlot& slot : m_sliceTaps) {
        if (slot.sliceId.load(std::memory_order_relaxed) != sliceId
            || slot.admissionClosed.load(std::memory_order_seq_cst)) {
            continue;
        }
        slot.callsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (!slot.admissionClosed.load(std::memory_order_seq_cst)
            && slot.sliceId.load(std::memory_order_seq_cst) == sliceId) {
            if (SliceAudioTap* tap = slot.tap.load(std::memory_order_seq_cst)) {
                tap->consume(samples, frames, kMasterMixSampleRateHz);
            }
        }
        if (slot.callsInFlight.fetch_sub(1, std::memory_order_seq_cst) == 1) {
            slot.callsInFlight.notify_all();
        }
    }
}

void AudioEngine::skipSliceTaps(int sliceId, int frames) noexcept
{
    if (frames <= 0) {
        return;
    }
    for (SliceTapSlot& slot : m_sliceTaps) {
        if (slot.sliceId.load(std::memory_order_relaxed) != sliceId
            || slot.admissionClosed.load(std::memory_order_seq_cst)) {
            continue;
        }
        slot.callsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (!slot.admissionClosed.load(std::memory_order_seq_cst)
            && slot.sliceId.load(std::memory_order_seq_cst) == sliceId) {
            if (SliceAudioTap* tap = slot.tap.load(std::memory_order_seq_cst)) {
                tap->skip(frames, kMasterMixSampleRateHz);
            }
        }
        if (slot.callsInFlight.fetch_sub(1, std::memory_order_seq_cst) == 1) {
            slot.callsInFlight.notify_all();
        }
    }
}

void AudioEngine::rxBlockReady(int sliceId, const float* samples, int frames)
{
    // R-R3-45 fix wave: seq_cst on both sides of this gate (a Dekker
    // pattern: store-closed then load-count against increment then
    // load-closed), as the tap gates beside it already are. Acquire and
    // release alone let each side miss the other's write.
    if (m_mixAdmissionClosed.load(std::memory_order_seq_cst)) {
        return;
    }
    m_mixRegionsInFlight.fetch_add(1, std::memory_order_seq_cst);
    struct MixRegionGuard {
        std::atomic<unsigned>& count;
        ~MixRegionGuard()
        {
            if (count.fetch_sub(1, std::memory_order_seq_cst) == 1) {
                count.notify_all();
            }
        }
    } mixRegion{m_mixRegionsInFlight};
    // Pairs with the control thread's close-before-invalidate sequence. If
    // close raced the first check, acknowledge without touching either mix.
    if (m_mixAdmissionClosed.load(std::memory_order_seq_cst)) {
        return;
    }

    if (!m_radio || samples == nullptr || frames <= 0) {
        return;
    }

    // SliceModel exposes muted() / setMuted() plus vaxChannel(). The
    // design spec uses audioMuted() as shorthand for the same property;
    // the alias is intentionally not introduced here (design-decision D5,
    // plan §Sub-Phase 4 Task 4.1). A formal rename (if chosen) happens in
    // Sub-Phase 9 alongside per-slice volume / pan control surfaces.
    //
    // R-R3-49: read from the view RadioModel publishes, not from the slice
    // list. sliceById() walked m_slices here while the main thread took a
    // slice out of it and deleted the SliceModel (ThreadSanitizer: a
    // use-after-free on this thread whenever a slice was removed while
    // audio ran).
    const SliceAudioView slice = sliceAudioView(sliceId);
    if (!slice.present) {
        return;
    }

    // 3M-1b E.4 fold: silence the TX-bound slice's RX audio during MOX.
    // Listening focus can move independently, so the gate follows the stable
    // ID captured at key-down rather than SliceModel::isActiveSlice().
    //
    // The acquire loads pair with setMoxState's releases so the gate observes
    // the captured ID before it observes MOX active.
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
    if (m_moxActive.load(std::memory_order_acquire)
        && sliceId == m_moxWithdrawnSlice.load(std::memory_order_acquire)) {
        // R-R3-43: a receiver tap hears nothing either, but is told how
        // many frames were withheld so its positions keep real time.
        skipSliceTaps(sliceId, frames);
        // Remote-window parity Task 32: nothing of this slice is queued,
        // but while no other slice is a barrier member its call drains the
        // mix, so MON (the transmit monitor, an opportunistic slot that
        // never holds the barrier) is heard when this is the only slice.
        // Without it a one-slice station drained nothing for the whole
        // transmission and MON was silent, here and in the holder's own
        // audio. With another slice playing, that slice's call drains the
        // period as before, so no period gets two blocks. Thetis's mixer
        // runs on the transmitter's stream alone while RX1 is out of the
        // mix:
        // From Thetis audio.cs:407-424 [v2.10.3.15] (Audio.MON, which calls
        // SetAAudioMixWhat for the transmitter's stream).
        drainMixes(frames, /*monitorOnly=*/true);
        return;  // silenced — TX-bound slice's RX audio gated during MOX
    }

    // Always queue, even when muted. MasterMixer treats mute as a ramp
    // TARGET rather than a gate, so the slice fades out over ~5 ms
    // instead of clicking. Withholding the feed here would defeat that
    // and also drop the slice out of the readiness barrier, which is
    // what the mute path used to do.
    // Mute rides in as an argument rather than through setSliceMuted(),
    // which takes the slice-map mutex: this is the audio thread, and
    // CLAUDE.md's rule is that it never holds a lock.
    //
    // R-R3-45: the route rides in the same way (VAX design 6.2): the mixer
    // builds this slice into the speakers sum or the headphones sum.
    const bool toHeadphones = slice.headphones;
    //
    // Slice control plan Task 6 (JJ's ruling): the slice's AF level rides
    // in too. WDSP's panel gain stays at 1.0, so the mixer applies AF to
    // the controller's sums and each listener's own level to its sum.
    m_masterMix.accumulate(sliceId, samples, frames, slice.muted,
                           toHeadphones, slice.afGain);

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
    //
    // R-R3-45: a slice on the headphones is left out by the same reasoning:
    // the reference is drained speakers-only (tryDrain(out, n)), so a
    // headphones slice is queued, keeps its barrier place, and adds
    // nothing, since its audio is not in the room either.
    //
    // Slice control plan Task 6: at the AF level, as the speakers hear it.
    m_antiVoxMix.accumulate(sliceId, samples, frames, slice.muted,
                            toHeadphones, slice.afGain);

    // VAX tap receives raw demodulated audio — pre-MasterMixer gain/pan,
    // pre-master-volume, matching Thetis VAC behavior and the spec §3.4
    // pseudocode. Per-channel mute skips the push; the per-channel gain
    // scales the block as it is queued for the channel's mix below. See docs/architecture/2026-04-19-vax-design.md
    // §3.4 and §6.4.
    // R-R3-43 receiver taps: the same point as the VAX tee below (no AF
    // level, no VAX channel gain or mute), so a remote VAX or TCI app gets
    // the level a local one would.
    feedSliceTaps(sliceId, samples, frames);

    // R-R3-44 (fix wave): slices that share a VAX channel are summed into
    // one block per period (VaxChannelMixer). Each used to push its own
    // block, so a channel carrying two slices got two periods of audio per
    // real period and its device ring overran.
    static_assert(VaxChannelMixer::kMaxSlices >= WdspEngine::kMaxSliceChannels,
                  "every slice id needs a VAX mix slot");
    // iPhone app Task 73 (ruling 5.14): VAX on this computer carries only
    // the slices its mask allows (the station device's own on a Core with
    // several devices); another device's slice is heard on that device.
    const bool vaxCarried = sliceId < 0 || sliceId >= 32
        || ((m_vaxSliceMask.load(std::memory_order_acquire) >> sliceId) & 1u) != 0;
    const int vaxCh = vaxCarried ? slice.vaxChannel : 0;
    const bool vaxValid = vaxCh >= 1 && vaxCh <= 4;
    float vaxGain = 1.0f;
    if (vaxValid) {
        const int vaxIdx = vaxCh - 1;
        const float gainUser =
            m_vaxRxGain[vaxIdx].load(std::memory_order_acquire);

        // Slice control plan Task 6 (JJ's ruling): the AF level is applied
        // in the mix, after this tee, and WDSP's panel gain stays at 1.0, so
        // `samples` is already the level VAX wants. The 1 / AF gain scaling
        // that undid the panel gain here is gone, and with it the silence at
        // AF 0: VAX stays audible whatever the speaker AF slider says. This
        // departs from Thetis, where radio.cs sets AF as the receiver's
        // panel gain (SetRXAPanelGain1) and VAC hears it.
        vaxGain = gainUser;
    }
    // A slice with no (or an out-of-range) channel leaves any channel it
    // was on, so it stops holding that channel's mix.
    m_vaxMix.accumulate(sliceId, vaxValid ? vaxCh : 0, samples, frames, vaxGain);

    if (vaxValid) {
        const int vaxIdx = vaxCh - 1;
        // Distinct from `mix` scratch below: that one is reserved for the
        // master-mix accumulate path. R-R3-45 fix wave: an engine member
        // sized off this thread with the mix scratch (never resized here);
        // the loop below drains any backlog a capacity at a time.
        std::vector<float>& vaxScratch = m_vaxScratch;
        const int vaxDrainFrames =
            std::min(frames, m_mixScratchFrames.load(std::memory_order_seq_cst));
        // Mute wins over gain: when muted the channel's block is taken and
        // not pushed; spec says "tags / level UI still reflect routing,
        // but no downstream audio". Taking it keeps the mix in step.
        const bool muted = m_vaxMuted[vaxIdx].load(std::memory_order_acquire);
        // RD-I10 (fix wave 2026-09-30): the owner thread replaces or closes
        // this output, and a remote window's feeder writes it, under
        // m_vaxBusMutex. The tee try-locks it, as the speakers push below
        // does: a contending replace or feeder write drops this block's
        // push instead of the tee waiting, or reading a bus being freed.
        // The mix is still drained, so it stays in step.
        std::unique_lock<std::mutex> vaxLk(m_vaxBusMutex[vaxIdx], std::try_to_lock);
        IAudioBus* vaxBus = vaxLk.owns_lock() ? m_vaxBus[vaxIdx].get() : nullptr;
        // Normally one block. More only after a slice on the channel was
        // late: then the backlog every slice has queued goes out at once.
        int mixedVax = 0;
        while ((mixedVax = m_vaxMix.tryDrain(vaxCh, vaxScratch.data(), vaxDrainFrames)) > 0) {
            if (!muted && vaxBus != nullptr && vaxBus->isOpen()) {
                vaxBus->push(reinterpret_cast<const char*>(vaxScratch.data()),
                             static_cast<qint64>(mixedVax) * 2
                                 * static_cast<qint64>(sizeof(float)));
            }
        }
    }

    drainMixes(frames);
}

// Remote-window parity Task 32: the drain, the taps and the output pushes,
// split out of rxBlockReady unchanged so the MOX-gated slice's own call
// runs them too. Called only from rxBlockReady, inside its mix region.
void AudioEngine::drainMixes(int frames, bool monitorOnly)
{
    // Flush synchronously on the DSP thread. thread_local scratch so the
    // per-block vector reuse costs zero allocation after the first block
    // per thread. Channel count = 2 is intentionally hard-coded here:
    // the MasterMixer contract and the DSP pipeline both emit stereo.
    //
    // R-R3-45 fix wave: the scratch is owned by the engine and sized off
    // this thread (constructor, setDspBlockSize, start), so nothing here
    // allocates. Each drain takes at most the scratch's frames. A producer
    // whose blocks are larger than that (not seen: the scratch starts at
    // the mixer ring's own 4096-frame floor and follows the DSP block size)
    // leaves the rest queued in its ring, the queue grows every period, and
    // the ring's drop-oldest overflow then discards audio. So the fix for
    // such a block is to size the scratch first, which setDspBlockSize does.
    std::vector<float>& mix = m_mixScratch;
    const int drainFrames =
        std::min(frames, m_mixScratchFrames.load(std::memory_order_seq_cst));

    // Phase 3F: the mixer decides whether a block leaves, not us. It
    // returns 0 until every slice feeding the mix this period has
    // delivered, so N slices produce ONE push instead of N. Draining
    // unconditionally here is what handed the sink two blocks per period
    // on the 2026-07-26 G2E bench and distorted the audio.
    //
    // With a single slice the barrier is satisfied by this very call, so
    // the push happens in the same call stack at the same instant it
    // always did: no added latency on the common path.
    //
    // R-R3-45: the same drain builds the headphones sum, the slices routed
    // there, so both outputs are paced by one barrier.
    std::vector<float>& hpMix = m_hpMixScratch;
    // iPhone app Task 76 (ruling 9.2): the same drain builds one mix per
    // owner with a tap on it, each of its own slices, and the local sums
    // carry only the local output's slices (the station device's).
    std::array<MasterMixer::OwnerOutput, kMaxOwnerMixes> owners{};
    // Slice control plan Task 6: each owner's listen levels, copied once
    // per drain so the mixer reads plain floats.
    std::array<std::array<float, kMaxSliceAudioViews>, kMaxOwnerMixes> ownerListenLevels{};
    int ownerCount = 0;
    for (int k = 0; k < kMaxOwnerMixes; ++k) {
        OwnerMixSlot& slot = m_ownerMixes[static_cast<size_t>(k)];
        MasterMixer::OwnerOutput& owner = owners[static_cast<size_t>(ownerCount)];
        owner = {};
        if (!slot.taken.load(std::memory_order_acquire)
            || (slot.program.tap.load(std::memory_order_acquire) == nullptr
                && slot.headphones.tap.load(std::memory_order_acquire) == nullptr)) {
            continue;
        }
        owner.sliceMask = slot.sliceMask.load(std::memory_order_acquire);
        owner.speakers = m_ownerSpeakersScratch[static_cast<size_t>(k)].data();
        owner.headphones = m_ownerHeadphonesScratch[static_cast<size_t>(k)].data();
        // Remote-window parity Task 32: the transmit monitor, when this
        // owner's device holds transmit (DaemonMediaController sets it).
        owner.monitor = static_cast<MasterMixer::OwnerMonitor>(
            slot.monitor.load(std::memory_order_acquire));
        // Slice control plan Task 6: its listening, keyed by its slot so a
        // listen ramp carries across drains whatever the other slots do.
        owner.listenMask = slot.listenMask.load(std::memory_order_acquire);
        if (owner.listenMask != 0) {
            std::array<float, kMaxSliceAudioViews>& levels =
                ownerListenLevels[static_cast<size_t>(ownerCount)];
            for (int id = 0; id < kMaxSliceAudioViews; ++id) {
                levels[static_cast<size_t>(id)] =
                    slot.listenLevels[static_cast<size_t>(id)].load(std::memory_order_acquire);
            }
            owner.listenLevels = levels.data();
        }
        owner.listenSlot = k;
        ++ownerCount;
    }
    const quint32 localListen = m_localListenMask.load(std::memory_order_acquire);
    std::array<float, kMaxSliceAudioViews> localListenLevels{};
    if (localListen != 0) {
        for (int id = 0; id < kMaxSliceAudioViews; ++id) {
            localListenLevels[static_cast<size_t>(id)] =
                m_localListenLevels[static_cast<size_t>(id)].load(std::memory_order_acquire);
        }
    }
    // Task 32 (JJ's MON ruling): MON stays off this computer's outputs, and
    // the master taps, while a remote device holds transmit.
    // Radio codec (JJ's ruling 2026-09-30): the radio's speaker out is a
    // second sum built beside the local ones, every receiving slice whatever
    // the local mask says; summed only while the radio tap is installed.
    std::vector<float>& radioSum = m_radioOutScratch;
    const bool radioTapped =
        m_radioOutputTap.tap.load(std::memory_order_acquire) != nullptr;
    const int mixed = m_masterMix.tryDrain(
        mix.data(), hpMix.data(), drainFrames,
        m_localOutputSliceMask.load(std::memory_order_acquire), owners.data(), ownerCount,
        m_txMonitorLocal.load(std::memory_order_acquire), monitorOnly, localListen,
        localListenLevels.data(), radioTapped ? radioSum.data() : nullptr);

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
    // R-R3-45 fix wave: an engine member sized off this thread, as `mix`.
    std::vector<float>& avMix = m_avMixScratch;
    // Task 32: the MOX-gated slice's monitor-only call leaves the anti-VOX
    // reference to the members' own calls (MON is never in it).
    const int avFrames = monitorOnly ? 0 : m_antiVoxMix.tryDrain(avMix.data(), drainFrames);
    if (avFrames > 0) {
        // DirectConnection only: avMix is the engine's scratch and the next
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

    // R3 receive-only master tap.  This is the output of MasterMixer, after
    // per-slice gain/mute/pan and the readiness barrier, and deliberately
    // before local speaker volume/mute/device handling.  A local-monitor
    // setting therefore cannot alter the station's remote mixed program.
    // The raw pointer is valid only during this synchronous call.
    if (!m_masterMixTapAdmissionClosed.load(std::memory_order_seq_cst)) {
        m_masterMixTapCallsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (!m_masterMixTapAdmissionClosed.load(std::memory_order_seq_cst)) {
            MasterMixAudioTap* tap =
                m_masterMixAudioTap.load(std::memory_order_seq_cst);
            if (tap != nullptr
                && m_masterMixTapSpeakersOnly.load(std::memory_order_seq_cst)) {
                // R-R3-45: an app that plays the headphones mix on its own
                // stream gets the speakers' mix alone here.
                tap->consume(mix.data(), mixed, kMasterMixSampleRateHz);
            } else if (tap != nullptr) {
                // R-R3-45: the station's program is every receiver, on
                // whichever local output it plays, so the local speakers or
                // headphones choice cannot change it either.
                std::vector<float>& program = m_programScratch;
                for (int i = 0; i < stereoFloats; ++i) {
                    program[static_cast<size_t>(i)] = mix[static_cast<size_t>(i)]
                        + hpMix[static_cast<size_t>(i)];
                }
                tap->consume(program.data(), mixed, kMasterMixSampleRateHz);
            }
        }
        if (m_masterMixTapCallsInFlight.fetch_sub(1, std::memory_order_seq_cst) == 1) {
            m_masterMixTapCallsInFlight.notify_all();
        }
    }

    // R-R3-45: the headphones mix for a remote window's headphones stream,
    // on its own gate, before any local device handling.
    if (!m_headphonesMixTapAdmissionClosed.load(std::memory_order_seq_cst)) {
        m_headphonesMixTapCallsInFlight.fetch_add(1, std::memory_order_seq_cst);
        if (!m_headphonesMixTapAdmissionClosed.load(std::memory_order_seq_cst)) {
            MasterMixAudioTap* tap =
                m_headphonesMixAudioTap.load(std::memory_order_seq_cst);
            if (tap != nullptr) {
                tap->consume(hpMix.data(), mixed, kMasterMixSampleRateHz);
            }
        }
        if (m_headphonesMixTapCallsInFlight.fetch_sub(1, std::memory_order_seq_cst) == 1) {
            m_headphonesMixTapCallsInFlight.notify_all();
        }
    }

    // iPhone app Task 76: each owner's taps, on their own gates. The owner
    // outputs were built in slot order, skipping slots without a tap, so
    // walk the slots the same way. A program tap takes the owner's two
    // sums added, in place (the speakers scratch is this owner's alone),
    // unless it takes the speakers' mix alone.
    {
        int built = 0;
        for (int k = 0; k < kMaxOwnerMixes && built < ownerCount; ++k) {
            OwnerMixSlot& slot = m_ownerMixes[static_cast<size_t>(k)];
            float* const speakers = m_ownerSpeakersScratch[static_cast<size_t>(k)].data();
            if (owners[static_cast<size_t>(built)].speakers != speakers) {
                continue;
            }
            ++built;
            float* const headphones = m_ownerHeadphonesScratch[static_cast<size_t>(k)].data();
            invokeMixTap(slot.headphones, headphones, mixed);
            if (!slot.programSpeakersOnly.load(std::memory_order_acquire)) {
                for (int i = 0; i < stereoFloats; ++i) {
                    speakers[i] += headphones[i];
                }
            }
            invokeMixTap(slot.program, speakers, mixed);
        }
    }

    // R-R3-45: the headphones output. No master volume or mute: design 6.3
    // puts both on the speakers. Same try_lock idiom as the speakers push
    // below, so a contending reopen drops this block instead of waiting.
    {
        std::unique_lock<std::mutex> hpLk(m_headphonesBusMutex, std::try_to_lock);
        if (hpLk.owns_lock()) {
            IAudioBus* headphonesBus = m_headphonesBus.get();
            if (headphonesBus != nullptr && headphonesBus->isOpen()) {
                if (m_headphonesConverter.passthrough()) {
                    headphonesBus->push(
                        reinterpret_cast<const char*>(hpMix.data()),
                        static_cast<qint64>(stereoFloats) * sizeof(float));
                } else {
                    const int hpSamples =
                        m_headphonesConverter.convert(hpMix.data(), mixed);
                    if (hpSamples > 0) {
                        headphonesBus->push(
                            reinterpret_cast<const char*>(m_headphonesConverter.output()),
                            static_cast<qint64>(hpSamples) * sizeof(float));
                    }
                }
            }
        }
    }

    const float vol = m_masterVolume.load(std::memory_order_acquire);
    if (vol != 1.0f) {
        for (int i = 0; i < stereoFloats; ++i) {
            mix[i] *= vol;
        }
    }

    // Radio codec (2026-09-30): the radio's own speaker out. Thetis hands
    // the receive audio mixer's output, every receiver and MON, to the
    // radio when the codec is the radio's (netInterface.c:1571-1575
    // [v2.10.3.15]):
    //   switch (pcm->audioCodecId)
    //   {
    //   case HERMES:
    //       SendpOutboundRx(OutBound);
    //       break;
    // which makes OutBound the output of audio mixer 0 (cmaster.c:408-412
    // [v2.10.3.15]: SetAAudioMixOutputPointer (0, 0, pcm->OutboundRx);),
    // at the AF volume, the mixer's own volume (cmaster.cs:954-957
    // [v2.10.3.15]):
    //   public static void CMSetAudioVolume(double volume)
    //   {
    //       cmaster.SetAAudioMixVolume((void*)0, 0, volume);
    //   }
    // Mixer 0 takes RX1, RX1S and RX2 whoever listens to them
    // (console.cs:27650-27664 [v2.10.3.15], UpdateAAudioMixerStates), and
    // MON on the same mixer (audio.cs:417-418 [v2.10.3.15]:
    // SetAAudioMixWhat(0, 0, WDSP.id(1, 0), mon)), so what the radio plays
    // while transmitting is the receivers plus the transmit monitor.
    // Here (JJ's ruling 2026-09-30): the mixer's radio sum, every receiving
    // slice whichever device owns it, both routes, and MON exactly while
    // this computer's outputs carry it: while a remote device holds
    // transmit MON stays off the radio's speaker as off the local ones
    // (Task 32). At the master volume, or silence while the master is
    // muted, so the radio's stream never stops.
    if (radioTapped) {
        if (m_masterMuted.load(std::memory_order_acquire)) {
            std::fill(radioSum.begin(), radioSum.begin() + stereoFloats, 0.0f);
        } else if (vol != 1.0f) {
            for (int i = 0; i < stereoFloats; ++i) {
                radioSum[static_cast<size_t>(i)] *= vol;
            }
        }
        invokeMixTap(m_radioOutputTap, radioSum.data(), mixed);
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
                if (m_speakersConverter.passthrough()) {
                    speakersBus->push(
                        reinterpret_cast<const char*>(mix.data()),
                        static_cast<qint64>(stereoFloats) * sizeof(float));
                } else {
                    // R-R3-23 (fix wave): the device's own rate and channel
                    // count, not the 48 kHz stereo mix read as if it were
                    // (a 96 kHz device played it at twice the speed, a mono
                    // one read interleaved stereo). Preallocated at open.
                    const int samples = m_speakersConverter.convert(mix.data(), mixed);
                    if (samples > 0) {
                        speakersBus->push(
                            reinterpret_cast<const char*>(m_speakersConverter.output()),
                            static_cast<qint64>(samples) * sizeof(float));
                    }
                }
            }
        }
        // A contending writer holding m_speakersBusMutex
        // (setSpeakersConfig, master-mute flush) silently drops the
        // current block.  The try_to_lock scope is narrowed to just
        // the push specifically to keep that drop window short and
        // bounded; we no longer trace mutex misses since the bench
        // confirmed the contention is rare enough to be inaudible.
    }}

bool AudioEngine::isPcMicSelected() const noexcept
{
    return m_micSourceWantsPc.load(std::memory_order_acquire);
}

bool AudioEngine::isPcMicOverrideActive() const noexcept
{
    // Combined selection/readiness query for callers that need both:
    //   - the user explicitly selected MicSource::Pc (m_micSourceWantsPc)
    //   - we have an open TX-input bus to pull from
    // Worker source routing uses isPcMicSelected() independently of readiness.
    if (!isPcMicSelected()) {
        return false;
    }
    const IAudioBus* bus = txInputSource();
    return (bus != nullptr) && bus->isOpen();
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
    // R-R3-36: the capture reader (or an injected test bus); the reader
    // returns nothing unless capture is Ready.
    IAudioBus* const bus = txInputSource();
    if (bus == nullptr || dst == nullptr || n <= 0) {
        return 0;
    }

    const AudioFormat fmt = bus->negotiatedFormat();
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

    const qint64 gotBytes = bus->pull(scratch.data(), needBytes);
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
    // rather than consulting mutable listening focus on the way out. The
    // active slice can change during a transmission, and
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
    if (active) {
        // Duplicate true edges are possible when a release walk is cancelled
        // by an immediate re-key. Keep the original key-down identity rather
        // than releasing and re-sampling mutable UI focus.
        if (m_moxWithdrawnSlice.load(std::memory_order_acquire) < 0
            && m_radio != nullptr) {
            if (SliceModel* const slice = m_radio->txBoundSlice()) {
                const int sliceId = slice->sliceIndex();
                setSliceStreaming(sliceId, false);
                m_moxWithdrawnSlice.store(sliceId, std::memory_order_release);
            }
        }
        m_moxActive.store(true, std::memory_order_release);
        return;
    }

    m_moxActive.store(false, std::memory_order_release);
    const int withdrawn =
        m_moxWithdrawnSlice.exchange(-1, std::memory_order_acq_rel);
    if (withdrawn >= 0) {
        setSliceStreaming(withdrawn, true);
    }
}

void AudioEngine::onActiveSliceChanged()
{
    // Listening focus is independent of transmit authority. The bound slice
    // captured at key-down remains gated until key-up, so an active-slice
    // change requires no mixer membership update.
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

// R-R3-45: the operator's MON output. The audio thread reads the atomic on
// the next monitor block and hands it to MasterMixer::accumulate, which
// ramps the slot out of one sum and into the other (the same crossfade a
// receiver gets when its route changes), so a change mid-transmission has
// no gap and no doubling. MON stays out of the anti-VOX mixer either way.
void AudioEngine::setTxMonitorOutput(TxMonitorOutput output)
{
    const bool headphones = output == TxMonitorOutput::Headphones;
    const bool prev =
        m_txMonitorToHeadphones.exchange(headphones, std::memory_order_acq_rel);
    if (prev == headphones) {
        return;  // idempotent
    }
    AppSettings::instance().setValue(
        QStringLiteral("audio/TxMonitor/Output"),
        headphones ? QStringLiteral("Headphones") : QStringLiteral("Speakers"));
    emit txMonitorOutputChanged(output);
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
    //
    // R-R3-45: the operator's MON output rides in as the route, so the
    // mixer builds MON into the speakers sum or the headphones sum, never
    // both. Only m_masterMix: MON is never in the anti-VOX reference.
    m_masterMix.accumulate(kTxMonitorSlotId, stereoScratch.data(), frames,
                           /*muted*/ false,
                           m_txMonitorToHeadphones.load(std::memory_order_acquire));
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
// Provides a peak-amplitude readout from the TX input (R-R3-36: the capture
// supervisor's reader, whose level is the peak of the latest PCM record from
// the helper, or an injected test bus).  Lock-free read from the main thread.
//
// Returns 0.0f when capture is not Ready (no demand, still opening, or
// failed).
//
// Used by AudioTxInputPage's Test Mic VU bar (I.2) to show live mic level
// without opening a separate capture stream.

float AudioEngine::pcMicInputLevel() const
{
    const IAudioBus* bus = txInputSource();
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

void AudioEngine::ensureMixScratchFrames(int frames)
{
    // R-R3-45 fix wave: owner thread. Grows the DSP thread's mix scratch
    // while no block is being mixed: the same close-then-drain gate
    // setSliceStreaming uses, so rxBlockReady never sees a vector being
    // replaced and never allocates itself.
    if (frames <= m_mixScratchFrames.load(std::memory_order_acquire)) {
        return;
    }
    m_mixAdmissionClosed.store(true, std::memory_order_seq_cst);
    unsigned inFlight = m_mixRegionsInFlight.load(std::memory_order_seq_cst);
    while (inFlight != 0) {
        m_mixRegionsInFlight.wait(inFlight, std::memory_order_seq_cst);
        inFlight = m_mixRegionsInFlight.load(std::memory_order_seq_cst);
    }
    const auto floats = static_cast<size_t>(frames) * 2;
    m_mixScratch.assign(floats, 0.0f);
    m_hpMixScratch.assign(floats, 0.0f);
    m_programScratch.assign(floats, 0.0f);
    m_radioOutScratch.assign(floats, 0.0f);
    m_avMixScratch.assign(floats, 0.0f);
    m_vaxScratch.assign(floats, 0.0f);
    for (int k = 0; k < kMaxOwnerMixes; ++k) {
        m_ownerSpeakersScratch[static_cast<size_t>(k)].assign(floats, 0.0f);
        m_ownerHeadphonesScratch[static_cast<size_t>(k)].assign(floats, 0.0f);
    }
    m_mixScratchFrames.store(frames, std::memory_order_seq_cst);
    m_mixAdmissionClosed.store(false, std::memory_order_seq_cst);
}

void AudioEngine::setDspBlockSize(int blockSize)
{
    ensureMixScratchFrames(blockSize);
    AppSettings::instance().setValue(QStringLiteral("audio/DspBlockSize"),
                                     QString::number(blockSize));
    // TODO(sub-phase-12-dsp-live-apply): delegate to WdspEngine once
    // channel teardown/rebuild infrastructure is available.
    qCInfo(lcAudio) << "DspBlockSize change queued — applied on next channel rebuild"
                    << "(requested:" << blockSize << ")";
    emit dspBlockSizeChanged(blockSize);
}

// Sub-Phase 12 Task 12.4 — resetAudioSettings (addendum §2.5).
// ---------------------------------------------------------------------------

void AudioEngine::resetAudioSettings(bool operatorLocalOnly)
{
    auto& s = AppSettings::instance();
    const QStringList keys = s.allKeys();

    // Delete all audio/* keys (addendum §2.5).
    // slice/<N>/VaxChannel and tx/OwnerSlot are implicitly preserved because
    // they live under the "slice/" and "tx/" namespaces — no key can start
    // with both "audio/" and "slice/" simultaneously, so no explicit exclusion
    // guard is needed here.
    for (const QString& key : keys) {
        if (key.startsWith(QStringLiteral("audio/"))
            && (!operatorLocalOnly || classifySettingsKey(key) == SettingsScope::OperatorLocal)) {
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

    // R-R3-45: audio/Headphones/Enabled is gone with the rest, so the
    // headphones close (the default is off) and forget their device.
    m_headphonesConfig = AudioDeviceConfig{};
    setHeadphonesEnabled(false);
    emit headphonesConfigChanged(AudioDeviceConfig{});
    // R-R3-45: audio/TxMonitor/Output went too, so MON is back on the
    // speakers. Not through setTxMonitorOutput(), which would write the
    // key straight back.
    if (m_txMonitorToHeadphones.exchange(false, std::memory_order_acq_rel)) {
        emit txMonitorOutputChanged(TxMonitorOutput::Speakers);
    }

    // Rebuild each VAX bus as well — previously we only emitted the config-
    // changed signal, but rxBlockReady kept pushing audio to whatever bus
    // was live pre-reset (stale BYO PortAudio bus, or the prior native HAL
    // bus tied to the wiped settings).  Mirror the setVaxConfig native-HAL
    // fallback contract: on Mac/Linux re-mint the platform HAL bus; on
    // Windows leave the slot null until the user picks a device.
    for (int ch = 1; ch <= 4; ++ch) {
        const int idx = ch - 1;
        {
            std::lock_guard<std::mutex> busLock(m_vaxBusMutex[idx]);
            m_vaxBus[idx].reset();
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
            m_vaxBus[idx] = makeVaxBus(ch);
            if (m_vaxBus[idx]) {
                qCInfo(lcAudio) << "VAX" << ch
                                << "bus restored to native HAL (reset)"
                                << "[" << m_vaxBus[idx]->backendName() << "]";
            }
#endif
        }
        emit vaxBusOpenChanged(ch);  // R-R3-21: the output was rebuilt or closed
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
