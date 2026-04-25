#pragma once

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
// =================================================================

#include "AudioDeviceConfig.h"
#include "IAudioBus.h"
#include "audio/MasterMixer.h"

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
#include <memory>
#include <mutex>

namespace NereusSDR {

class RadioModel;
class SliceModel;

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
class AudioEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

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

#endif

    // Called by RxDspWorker when a slice produces an RX audio block.
    // samples is interleaved stereo float32, length = frames * 2.
    void rxBlockReady(int sliceId, const float* samples, int frames);

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

signals:
    void volumeChanged(float volume);
    void masterMutedChanged(bool muted);
    void vaxRxGainChanged(int channel, float gain);
    void vaxMutedChanged(int channel, bool muted);
    void vaxTxGainChanged(float gain);

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
    void txInputConfigChanged(NereusSDR::AudioDeviceConfig cfg);
    void vaxConfigChanged(int channel, NereusSDR::AudioDeviceConfig cfg);

#if defined(Q_OS_LINUX)
    void linuxBackendChanged(LinuxAudioBackend oldBackend,
                             LinuxAudioBackend newBackend);
#endif

private:
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

    RadioModel* m_radio{nullptr};

    // Sub-Phase 12 Task 12.2 — live-reconfig safety mutex for the speakers
    // bus. setSpeakersConfig() acquires this during tear-down + rebuild.
    // rxBlockReady() uses try_lock and drops the block if it can't acquire
    // (≤1 ms of silence is inaudible vs. a use-after-free). NOT held in the
    // audio callback path (acquires try_lock only; never blocks).
    std::mutex m_speakersBusMutex;

    std::unique_ptr<IAudioBus> m_speakersBus;
    std::unique_ptr<IAudioBus> m_headphonesBus;
    std::unique_ptr<IAudioBus> m_txInputBus;
    // Sub-Phase 8.5: platform-native VAX TX virtual bus. Distinct from
    // m_txInputBus, which is the OS mic-capture device owned by MicDirect.
    // Opened in start(), reset in stop(); consumption is a Phase 3M concern.
    std::unique_ptr<IAudioBus> m_vaxTxBus;
    std::array<std::unique_ptr<IAudioBus>, 4> m_vaxBus;
    MasterMixer m_masterMix;

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

    // Was Pa_Initialize() actually successful? Guards the matching
    // Pa_Terminate() in the destructor so unit tests that construct
    // AudioEngine without a real audio subsystem don't hit a spurious
    // terminate.
    bool m_paInitialized{false};
    bool m_running{false};
    // Was sliceId 0 registered with MasterMixer? startup-only invariant
    // per design-decision D6 (plan); prevents a main-thread insert/rehash
    // race against the audio thread's lock-free find().
    bool m_slicePreregistered{false};

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
