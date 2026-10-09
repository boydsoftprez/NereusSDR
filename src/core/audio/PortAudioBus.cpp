// =================================================================
// src/core/audio/PortAudioBus.cpp  (NereusSDR)
// =================================================================
// See PortAudioBus.h for contract. NereusSDR-original.
//
// no-port-check: This file is NereusSDR-original (PortAudio v19.7.0
// backend for the IAudioBus interface).  An inline comment in open()
// references Thetis ChannelMaster/ivac.c:311-340 [v2.10.3.15] for
// PHILOSOPHICAL context only (Thetis uses paWinWasapiExclusive on
// Windows for OS-side SRC bypass; we use device-native-rate open on
// macOS for the same end), not as a port.  No Thetis bytes ported.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 6 (R-AUD-15, R-AUD-33): an output
//               stream's ring is a DeviceRateMatcher; push() takes 48 kHz
//               stereo, the callback reads it and writes the device's
//               channels with writeStereoToDevice; outputPacing,
//               delayParts, matcherStats, restartClockMatch and flush come
//               from the matcher.  Bug 6: the capture-name preference is
//               guarded by Q_OS_MAC.  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-08: native audio plan Task 1 (V-HW-8): the input callback hands
//               each block and its capture time to an optional hook (the
//               audio delay probe's detector). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-23 an output stream sizes its ring by
//               outputRingSamples() before it starts, so a speaker faster
//               than 48 kHz stereo still holds 100 ms. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-35 outputPacing() reports the stream's output latency
//               (Pa_GetStreamInfo) so remote audio delay can include the
//               device. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-22: R-R3-36 fix wave: strict resolution accepts exact names
//               only (matchNamedDevice). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-22: strict named-input resolution (setStrictInputDevice),
//               lastOpenFailure() and opened-device accessors for the
//               nereus-audio-capture helper (R-R3-36). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "PortAudioBus.h"
#include "../LogCategories.h"
#include "../MemoryLock.h"
#include "../PerfMonitor.h"
#include "../Resampler.h"
#include "AudioDelayProbe.h"
#include "AudioTestBarrier.h"
#include "DeviceSampleFormat.h"

#include <portaudio.h>

#include <QStandardPaths>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace NereusSDR {

namespace {

// Resolve a PortAudio device index from a PortAudioConfig, with a robust
// fallback chain. Returns paNoDevice if no suitable device exists on the
// system. Fixes issue #112: the 0.2.2 IAudioBus refactor hard-coded
// Pa_GetDefaultOutputDevice(), which returns paNoDevice on Linux hosts
// that lack an ALSA default (e.g. Ubuntu/PipeWire without pipewire-alsa).
// It also ignored cfg.deviceName and cfg.hostApiIndex so users couldn't
// work around it by picking a specific device in Setup → Audio → Devices.
//
// Resolution order:
//   1. If deviceName non-empty: match against all devices of the right
//      direction (preferring cfg.hostApiIndex if specified). Matching is
//      case-insensitive and prefers exact match, falling back to
//      substring match — PortAudio device names vary subtly by host API
//      and Qt settings may round-trip a slightly different string.
//   2. Default device for cfg.hostApiIndex (if >= 0 and valid).
//   3. PortAudio default output / input device.
//   4. First enumerated device with the correct direction (channels > 0).
//      This is the critical fallback for the #112 scenario — even when
//      there is no ALSA default, PortAudio typically still enumerates
//      "hw:0,0" etc., which at least lets audio reach the user.
//
// strictNamed: when true and deviceName is non-empty, step 1 is the only
// step and accepts exact names only (no substring match); a name that
// matches nothing returns paNoDevice instead of falling
// through to the defaults (the capture helper's "never silently switch
// microphones" rule, R-R3-36).
PaDeviceIndex resolveDevice(const PortAudioConfig& inCfg,
                            bool wantOutput,
                            int requestedChannels,
                            bool strictNamed = false)
{
    const int deviceCount = Pa_GetDeviceCount();
    if (deviceCount <= 0) {
        return paNoDevice;
    }

    // macOS / Linux: when resolving a CAPTURE default and the user hasn't
    // pinned a specific device, default-input enumeration is unreliable
    // because virtual capture devices (Teams Audio, Zoom, NereusSDR/AetherSDR
    // VAX, Splashtop, BlackHole, etc.) often appear as the system default
    // and silently deliver zero samples. Prefer a real hardware mic by name.
    PortAudioConfig effectiveCfg = inCfg;
    // Capture-only: positive name marker for hardware mics. Used to prefer
    // the actual hardware microphone over any virtual device that may
    // appear in the system enumeration (Teams Audio, ZoomAudioDevice,
    // BlackHole, NereusSDR/AetherSDR VAX/DAX, Splashtop, etc.). The
    // virtual-mic landscape is too varied to enumerate every vendor in
    // a deny-list, so we match the hardware naming convention instead.
    const auto isHardwareMicName = [](const QString& name) -> bool {
        static const char* kHardwareMicMarkers[] = {
            "Microphone",  // CoreAudio default name for built-in / USB mics
            "Built-in",
            "Internal",
            "Mic Input",
        };
        for (const char* marker : kHardwareMicMarkers) {
            if (name.contains(QLatin1String(marker), Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    };

    // For CAPTURE on macOS, prefer a hardware-named device BEFORE trusting
    // any "default". The system default (Pa or Core Audio) is frequently
    // hijacked by virtual mics. Priority order:
    //   1. MacBook Pro / Built-in / Internal — strong hardware match
    //   2. anything else with "Microphone" but NOT "iPhone" (Continuity
    //      Camera mics are often unavailable when iPhone is disconnected)
#ifdef Q_OS_MAC
    if (!wantOutput && effectiveCfg.deviceName.isEmpty()) {
        QString tier1, tier2;
        for (int i = 0; i < deviceCount; ++i) {
            const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
            if (!di || !di->name || di->maxInputChannels <= 0) continue;
            const QString n = QString::fromUtf8(di->name);
            if (n.contains(QLatin1String("iPhone"), Qt::CaseInsensitive)) continue;
            if (tier1.isEmpty() && (n.contains(QLatin1String("MacBook"), Qt::CaseInsensitive)
                                    || n.contains(QLatin1String("Built-in"), Qt::CaseInsensitive)
                                    || n.contains(QLatin1String("Internal"), Qt::CaseInsensitive))) {
                tier1 = n;
            } else if (tier2.isEmpty() && isHardwareMicName(n)) {
                tier2 = n;
            }
        }
        if (!tier1.isEmpty()) effectiveCfg.deviceName = tier1;
        else if (!tier2.isEmpty()) effectiveCfg.deviceName = tier2;
    }
#endif
    const PortAudioConfig& cfg = effectiveCfg;

    auto directionOk = [wantOutput](const PaDeviceInfo* di) {
        if (!di) { return false; }
        return wantOutput ? di->maxOutputChannels > 0
                          : di->maxInputChannels  > 0;
    };

    // Step 4 capacity check: prefer devices that actually support the
    // requested channel count. Without this, the first enumerated output
    // on a mono-first system (e.g. a USB webcam enumerated ahead of the
    // onboard stereo card) causes Pa_OpenStream to fail with
    // paInvalidChannelCount even though a later device would succeed.
    auto capacityOk = [wantOutput, requestedChannels](const PaDeviceInfo* di) {
        if (!di) { return false; }
        const int avail = wantOutput ? di->maxOutputChannels
                                     : di->maxInputChannels;
        return avail >= requestedChannels;
    };

    // 1. Named-device match.
    if (!cfg.deviceName.isEmpty()) {
        QVector<PortAudioBus::NamedDeviceCandidate> candidates;
        QVector<PaDeviceIndex> candidateIndex;
        for (int i = 0; i < deviceCount; ++i) {
            const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
            if (!directionOk(di)) { continue; }
            candidates.push_back({QString::fromUtf8(di->name), di->hostApi});
            candidateIndex.push_back(i);
        }
        const int match = PortAudioBus::matchNamedDevice(
            candidates, cfg.deviceName, cfg.hostApiIndex, strictNamed);
        if (match >= 0) {
            return candidateIndex[match];
        }
        if (strictNamed) {
            return paNoDevice;
        }
        // Named device not found: fall through to defaults rather than
        // erroring out — better silent fallback than no audio at all.
    }

    // 2. Host-API default.
    if (cfg.hostApiIndex >= 0) {
        const PaHostApiInfo* hai = Pa_GetHostApiInfo(cfg.hostApiIndex);
        if (hai) {
            const PaDeviceIndex d = wantOutput
                ? hai->defaultOutputDevice
                : hai->defaultInputDevice;
            const PaDeviceInfo* di = (d != paNoDevice) ? Pa_GetDeviceInfo(d) : nullptr;
            if (d != paNoDevice && directionOk(di)
                && (wantOutput || (di && di->name && isHardwareMicName(QString::fromUtf8(di->name))))) {
                return d;
            }
        }
    }

    // 3. Global default.
    const PaDeviceIndex def = wantOutput
        ? Pa_GetDefaultOutputDevice()
        : Pa_GetDefaultInputDevice();
    const PaDeviceInfo* defDi = (def != paNoDevice) ? Pa_GetDeviceInfo(def) : nullptr;
    if (def != paNoDevice && directionOk(defDi)
        && (wantOutput || (defDi && defDi->name && isHardwareMicName(QString::fromUtf8(defDi->name))))) {
        return def;
    }

    // 4. First enumerated device with matching direction — prefer one
    //    that satisfies the requested channel count, fall back to any
    //    direction-valid device if none does (Pa_OpenStream will then
    //    surface a clear paInvalidChannelCount error, better than
    //    silently picking step 3's default which may not even exist).
    //    For capture, prefer devices whose name suggests hardware
    //    (e.g. "Microphone", "Built-in") to dodge any virtual
    //    device that snuck through.
    PaDeviceIndex anyDirection = paNoDevice;
    PaDeviceIndex hardwareCandidate = paNoDevice;
    for (int i = 0; i < deviceCount; ++i) {
        const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
        if (!directionOk(di)) { continue; }
        if (capacityOk(di)) {
            // Capture: prefer a hardware-mic-named device.
            if (!wantOutput && hardwareCandidate == paNoDevice && di && di->name
                && isHardwareMicName(QString::fromUtf8(di->name))) {
                hardwareCandidate = i;
            }
            if (anyDirection == paNoDevice) { anyDirection = i; }
        }
    }
    if (hardwareCandidate != paNoDevice) return hardwareCandidate;
    return anyDirection;
}

} // namespace

PortAudioBus::PortAudioBus() {
    // R-AUD-15: this ring now serves the input direction only; an output
    // stream queues in its DeviceRateMatcher (prepareOutputMatcher).
    // [original comment follows, written when the ring served output]
    // 100 ms stereo float ring (4800 stereo frames * 2 channels = 9600
    // floats) at the nominal 48 kHz device rate.  Sized as the
    // capacity ceiling, NOT the typical fill: with the DSP-thread
    // producer (RxDspWorker via DirectConnection lambda, see
    // RadioModel) and a 128-frame PortAudio callback, steady-state
    // ring level oscillates around 10-40 ms.  We sat at 200 ms
    // briefly while diagnosing crackle on the wrong-binary bench
    // (a missing-rebuild artifact); the underlying jitter was always
    // a main-thread / signal-routing problem fixed by Lever 2
    // (DirectConnection lambdas in RadioModel), not a buffer-size
    // problem, so dropping back to 100 ms reclaims latency that the
    // larger ring was hiding.  Worst-case audio latency through this
    // ring is now ~100 ms (totally full) instead of ~200 ms; typical
    // is unchanged because steady-state fill is well below either
    // cap.  Writer modulo-wraps as before; reader (paCallback)
    // detects when the writer has stomped on the read position and
    // skips forward to the oldest still-valid sample.  Drop-oldest
    // overrun semantics: a CPU stall produces a brief silence gap
    // rather than scrambled bytes, smoothed by the kCrossfadeFrames
    // ramp on the resume sample (see paCallback below).  Pre-fix the
    // ring was 48000 * 2 (1 second) with no overrun handling, which
    // made the display drift up to a full second ahead of audio and
    // produced the "audio replays" symptom on stall recovery.
    m_ring.resize(kDefaultRingSamples);

    // 2026-05-26 KG4VCF: pin the audio ring so heavy memory pressure
    // (parallel builds, Spotlight indexing) can not compress / page
    // it out.  Any access to a compressed page costs a decompression
    // stall on the audio thread -- one of the dominant causes of
    // under-load audio jitter we measured.  Lock failure is logged
    // by MemoryLock and the ring continues to work as pageable
    // memory, so this is best-effort.
    NereusSDR::lockMemory(m_ring.data(),
                          m_ring.size() * sizeof(float),
                          "PortAudioBus::m_ring");
}

PortAudioBus::~PortAudioBus() {
    NereusSDR::unlockMemory(m_ring.data(),
                            m_ring.size() * sizeof(float));
    close();
}

bool PortAudioBus::prepareOutputMatcher(int deviceRate, int deviceChannels)
{
    releaseOutputMatcher();
    DeviceRateMatcher::Config config;
    config.inRate = 48000;
    config.outRate = deviceRate;
    config.callbackFrames = m_cfg.bufferSamples > 0 ? m_cfg.bufferSamples : kOutputChunkFrames;
    config.delayMs = 0;
    auto matcher = std::make_unique<DeviceRateMatcher>(config);
    if (!matcher->valid() || deviceChannels <= 0) {
        return false;
    }
    // Pin the matcher's ring as the old output ring was (2026-05-26
    // KG4VCF: a compressed page costs a decompression stall on the audio
    // thread).  Best effort; MemoryLock logs a failure.
    NereusSDR::lockMemory(matcher->ring(), DeviceRateMatcher::ringBytes(config),
                          "PortAudioBus::m_outputMatcher");
    m_outputReader = matcher->makeReader();
    m_outputScratch.assign(std::size_t(2 * kOutputChunkFrames), 0.0f);
    m_outputDeviceChannels = deviceChannels;
    m_outputDeviceRate = deviceRate;
    m_outputMatcherBytes = DeviceRateMatcher::ringBytes(config);
    m_outputMatcher = std::move(matcher);
    return true;
}

void PortAudioBus::releaseOutputMatcher()
{
    // Only when no callback can run: before the stream starts, or after
    // Pa_StopStream has joined the audio thread.
    if (m_outputMatcher) {
        NereusSDR::unlockMemory(m_outputMatcher->ring(), m_outputMatcherBytes);
    }
    m_outputReader = MatcherReader();
    m_outputMatcher.reset();
    m_outputScratch.clear();
    m_outputDeviceChannels = 0;
    m_outputDeviceRate = 0;
    m_outputMatcherBytes = 0;
}

void PortAudioBus::setConfig(const PortAudioConfig& cfg) {
    m_cfg = cfg;
}

bool PortAudioBus::open(const AudioFormat& format) {
    if (m_stream) {
        close();
    }

    // No callback can run until Pa_StartStream below. A fresh open starts
    // with no queued audio or stale device-clock/discard state.
    m_ringRead.store(0, std::memory_order_relaxed);
    m_ringWrite.store(0, std::memory_order_relaxed);
    m_outputCallbackFrames.store(0, std::memory_order_relaxed);
    m_outputLatencyNs.store(-1, std::memory_order_relaxed);
    m_inputLatencyNs.store(-1, std::memory_order_relaxed);
    releaseOutputMatcher();

    const bool wantOutput = (m_cfg.direction == AudioDirection::Output);
    m_openFailure = OpenFailure::None;
    m_openedDeviceName.clear();

    PaStreamParameters params;
    PaError err = paNoError;
    const PaDeviceInfo* di = nullptr;

    // Strict resolution applies only to a named input device (R-R3-36).
    const bool strictNamed = !wantOutput && m_strictInputDevice
                             && !m_cfg.deviceName.trimmed().isEmpty();
    params.device = resolveDevice(m_cfg, wantOutput, format.channels, strictNamed);
    if (params.device == paNoDevice) {
        m_openFailure = OpenFailure::DeviceNotFound;
        if (strictNamed) {
            m_err = QStringLiteral("device-not-found: ") + m_cfg.deviceName.trimmed();
        } else {
            m_err = wantOutput
                ? QStringLiteral("No output device found")
                : QStringLiteral("No input device found");
        }
        return false;
    }
    di = Pa_GetDeviceInfo(params.device);
    if (di == nullptr) {
        m_openFailure = OpenFailure::OpenFailed;
        m_err = QStringLiteral("Pa_GetDeviceInfo returned null for resolved device");
        return false;
    }
    // Clamp channelCount to what the device actually supports. Mono mics
    // (e.g. "MacBook Pro Microphone") would otherwise fail Pa_OpenStream
    // when format.channels==2 (default). Track the effective channel
    // count so m_negFormat reflects the actual stream layout below.
    int effectiveChannels = format.channels;
    {
        const int devMax = wantOutput ? di->maxOutputChannels : di->maxInputChannels;
        if (devMax > 0 && effectiveChannels > devMax) {
            effectiveChannels = devMax;
        }
    }
    params.channelCount              = effectiveChannels;
    params.sampleFormat              = paFloat32;
    params.suggestedLatency          = wantOutput ? di->defaultLowOutputLatency
                                                  : di->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    // ---- Capture (mic) path: open at device native rate, resample on our side. ----
    //
    // From Thetis ChannelMaster/ivac.c:311-340 [v2.10.3.15] — Thetis sets
    // paWinWasapiExclusive on the WASAPI host API specifically to bypass the
    // Windows shared-mixer sample-rate converter when the configured rate
    // differs from the device's native rate.  CoreAudio has no public
    // exclusive-mode equivalent, but opening the PortAudio stream at the
    // device's native rate accomplishes the same thing: CoreAudio's AUHAL
    // does not insert a sample-rate-converter unit when the requested rate
    // already matches the device.  Under load that AUHAL SRC delivers
    // bursty / sub-rate samples which manifests as audible "digital
    // jitter" on on-air TX -- the exact bench symptom we are fixing.
    //
    // We then resample on our own clock with the r8brain wrapper that
    // already serves the RADE 48->16 TX path (third_party/r8brain,
    // src/core/Resampler.h).  Downstream consumers continue to see the
    // requested rate via negotiatedFormat() so this is invisible above
    // PortAudioBus.
    //
    // Output (speaker) path keeps the existing behaviour -- the on-air
    // jitter we are chasing is mic-input specific and the speaker side
    // already has its own well-behaved path.
    const int requestedRate    = format.sampleRate;
    const int deviceNativeRate = static_cast<int>(di->defaultSampleRate);
    int       openRate         = requestedRate;
    bool      needResample     = false;
    if (!wantOutput && deviceNativeRate > 0 && deviceNativeRate != requestedRate) {
        openRate     = deviceNativeRate;
        needResample = true;
    }

    err = Pa_OpenStream(
        &m_stream,
        wantOutput ? nullptr : &params,
        wantOutput ? &params : nullptr,
        static_cast<double>(openRate), m_cfg.bufferSamples,
        paClipOff, &PortAudioBus::paCallback, this);

    if (err != paNoError) {
        m_openFailure = OpenFailure::OpenFailed;
        m_err = QString::fromUtf8(Pa_GetErrorText(err));
        m_stream = nullptr;
        m_negFormat = {};
        m_backendName.clear();
        m_inputResampler.reset();
        m_resampleScratch.clear();
        m_monoScratch.clear();
        m_inputStreamChannels = 0;
        m_nativeSampleRate = 0;
        return false;
    }

    // Everything paCallback reads must be published BEFORE the stream
    // starts.  Pa_StartStream() hands the stream to the host API's
    // audio thread, which can invoke the callback immediately -- so
    // building the resampler and its scratch buffers after the start
    // call (as this did when first written) let a first callback race
    // the writes: it could see a half-constructed unique_ptr, a
    // zero-length scratch vector, or take the no-resampler branch and
    // push native-rate samples straight into the ring.  Codex review,
    // PR #291.
    m_negFormat = format;            // report the *requested* rate upstream
    m_negFormat.channels = reportedCaptureChannels(effectiveChannels,
                                                   !wantOutput && needResample);
    m_nativeSampleRate = openRate;
    m_inputStreamChannels = wantOutput ? 0 : effectiveChannels;

    // R-AUD-15: an output stream's queue is a clock matcher from the
    // 48 kHz stereo mix to the stream's own rate and channels (this
    // replaces R-R3-23's ring sized by outputRingSamples()).  No callback
    // runs yet, so it may be built here.  A rate the matcher cannot run is
    // an open failure.
    if (wantOutput && !prepareOutputMatcher(openRate, effectiveChannels)) {
        m_openFailure = OpenFailure::OpenFailed;
        m_err = QStringLiteral("Clock matcher cannot run at %1 Hz").arg(openRate);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        m_negFormat = {};
        m_backendName.clear();
        m_inputStreamChannels = 0;
        m_nativeSampleRate = 0;
        return false;
    }

    if (needResample) {
        // Worst-case per-callback input frames at the native rate:
        // bufferSamples (PA callback frames) * effectiveChannels.  At the
        // output side that produces roughly bufferSamples * (req / native)
        // frames; we add 4x slack to absorb r8brain's startup-priming
        // burst and any per-call output-length variance.
        const int worstInputSamples =
            m_cfg.bufferSamples * std::max(1, effectiveChannels);
        const int worstOutputSamples =
            static_cast<int>(
                static_cast<double>(worstInputSamples)
                * static_cast<double>(requestedRate)
                / static_cast<double>(openRate))
            * 4 + 256;
        m_resampleScratch.assign(static_cast<size_t>(worstOutputSamples), 0.0f);
        // Downmix destination for the multi-channel case.  Sized from
        // the configured callback block, not a fixed 1024-float stack
        // array -- the Audio setup page offers buffer sizes up to 2048,
        // and the old cap silently discarded every frame past 1024,
        // starving the TX producer by half or more.  Codex review,
        // PR #291.  x2 headroom in case a host API hands us a larger
        // block than we asked for.
        m_monoScratch.assign(
            static_cast<size_t>(std::max(1, m_cfg.bufferSamples) * 2), 0.0f);
        m_inputResampler = std::make_unique<Resampler>(
            static_cast<double>(openRate),
            static_cast<double>(requestedRate),
            worstInputSamples);
        qCInfo(lcAudio).noquote()
            << QStringLiteral("PortAudioBus: mic opened at native %1 Hz, "
                              "resampling to %2 Hz via r8brain "
                              "(Thetis paWinWasapiExclusive analogue, "
                              "bypasses CoreAudio AUHAL SRC); "
                              "%3-channel stream reported as %4-channel.")
                .arg(openRate).arg(requestedRate)
                .arg(effectiveChannels).arg(m_negFormat.channels);
    } else {
        m_inputResampler.reset();
        m_resampleScratch.clear();
        m_monoScratch.clear();
        qCInfo(lcAudio).noquote()
            << QStringLiteral("PortAudioBus: %1 opened at %2 Hz (native), "
                              "no resampler needed.")
                .arg(wantOutput ? QStringLiteral("output")
                                : QStringLiteral("mic"))
                .arg(openRate);
    }

    // V-HW-8: the input latency the delay probe's capture time falls back
    // on, published before the callback can run.
    if (!wantOutput) {
        const PaStreamInfo* streamInfo = Pa_GetStreamInfo(m_stream);
        const double latencySeconds = streamInfo != nullptr ? streamInfo->inputLatency : 0.0;
        m_inputLatencyNs.store(std::isfinite(latencySeconds) && latencySeconds > 0.0
                                   ? static_cast<qint64>(std::llround(latencySeconds * 1e9))
                                   : qint64{-1},
                               std::memory_order_release);
    }

    // Callback-visible state is now fully published; safe to start.
    err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        m_openFailure = OpenFailure::StartFailed;
        m_err = QString::fromUtf8(Pa_GetErrorText(err));
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        m_negFormat = {};
        m_backendName.clear();
        m_inputResampler.reset();
        m_resampleScratch.clear();
        m_monoScratch.clear();
        m_inputStreamChannels = 0;
        m_nativeSampleRate = 0;
        releaseOutputMatcher();
        return false;
    }

    // Defensive null-check on host-API lookup. With a device handed back
    // by Pa_GetDefault{Output,Input}Device this should never be null, but
    // keep the backend name well-defined if it ever is.
    // R-R3-35: the output latency PortAudio reports for this stream, from a
    // buffer the callback fills to the device output. A zero or missing
    // value is treated as unknown rather than as no delay.
    if (wantOutput) {
        const PaStreamInfo* streamInfo = Pa_GetStreamInfo(m_stream);
        const double latencySeconds = streamInfo != nullptr ? streamInfo->outputLatency : 0.0;
        m_outputLatencyNs.store(std::isfinite(latencySeconds) && latencySeconds > 0.0
                                    ? static_cast<qint64>(std::llround(latencySeconds * 1e9))
                                    : qint64{-1},
                                std::memory_order_release);
    }

    const PaHostApiInfo* hai = Pa_GetHostApiInfo(di->hostApi);
    if (hai != nullptr && hai->name != nullptr) {
        m_backendName = QString::fromUtf8(hai->name);
    } else {
        m_backendName.clear();
    }
    m_openedDeviceName = (di->name != nullptr) ? QString::fromUtf8(di->name) : QString();
    return true;
}

int PortAudioBus::openedStreamChannels() const {
    if (!m_stream) {
        return 0;
    }
    return (m_inputStreamChannels > 0) ? m_inputStreamChannels : m_negFormat.channels;
}

void PortAudioBus::close() {
    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }
    // Release the input resampler + its scratch buffer.  Safe here
    // because Pa_StopStream above has joined the audio thread, so no
    // more paCallback invocations can be in flight.
    m_inputResampler.reset();
    m_resampleScratch.clear();
    m_nativeSampleRate = 0;
    m_openedDeviceName.clear();
    // Pa_StopStream joins the callback before this reset. Reopening must not
    // inherit queued output, a prior discard floor, or device consumption.
    m_ringRead.store(0, std::memory_order_relaxed);
    m_ringWrite.store(0, std::memory_order_relaxed);
    m_outputCallbackFrames.store(0, std::memory_order_relaxed);
    releaseOutputMatcher();
    // Cumulative drop / underrun / PA-flag counters remain queryable
    // via ringOverrunEvents() / ringOverrunSamples() /
    // ringUnderrunEvents() and the m_paOutputUnderflowEvents /
    // m_paOutputOverflowEvents members.  Used by future support
    // tooling; no per-close log spam.
}

qint64 PortAudioBus::push(const char* data, qint64 bytes) {
    if (m_cfg.direction != AudioDirection::Output) { return 0; }
    // R-AUD-15: the mix goes into the clock matcher, which exists only
    // while the stream is open.
    if (!m_outputMatcher || data == nullptr || bytes <= 0) { return 0; }
    const int frames = static_cast<int>(bytes / qint64(2 * sizeof(float)));
    if (frames <= 0) { return 0; }
    const float* in = reinterpret_cast<const float*>(data);
    float peak = 0.0f;
    for (int i = 0; i < frames * 2; ++i) {
        peak = std::max(peak, std::abs(in[i]));
    }
    m_outputMatcher->write(in, frames, audioProbeNowNs());
    m_rxLevel.store(peak, std::memory_order_release);
    return bytes;
}

void PortAudioBus::flush() {
    // Issue #201: drop any unread samples queued so they don't keep
    // draining out the device after a mute click.  Output: the clock
    // matcher drops what is queued at its writer's next write
    // (R-AUD-15).  Input: equalize the ring's read/write cursors.
    if (m_cfg.direction == AudioDirection::Output) {
        if (m_outputMatcher) {
            m_outputMatcher->requestFlush();
        }
        return;
    }
    if (m_ring.empty() || !m_stream) {
        return;
    }
    const qint64 w = m_ringWrite.load(std::memory_order_acquire);
    m_ringRead.store(w, std::memory_order_release);
}

int PortAudioBus::outputCallbackFramesNow() const
{
    return std::max(m_cfg.bufferSamples,
                    m_outputCallbackFrames.load(std::memory_order_acquire));
}

std::optional<IAudioBus::OutputPacing> PortAudioBus::outputPacing() const
{
    if (m_cfg.direction != AudioDirection::Output || !m_outputMatcher) {
        return std::nullopt;
    }
    // R-AUD-15: every frame the device asked for (dry-run fill included,
    // as the old ring counted its silent frames), the matcher's fill and
    // its automatic size, all at the device rate.
    DeviceRateMatcher& matcher = *m_outputMatcher;
    const MatcherRingHeader* ring = matcher.ring();
    OutputPacing pacing;
    pacing.consumedFrames = ring->requested.load(std::memory_order_acquire);
    pacing.queuedFrames = std::max(0, static_cast<int>(std::lround(matcher.fillFrames())));
    pacing.capacityFrames = static_cast<int>(ring->rsizeFrames.load(std::memory_order_acquire));
    pacing.callbackFrames = outputCallbackFramesNow();
    if (const qint64 latencyNs = m_outputLatencyNs.load(std::memory_order_acquire);
        latencyNs > 0) {
        pacing.deviceLatencyNs = latencyNs;
    }
    return pacing;
}

AudioDelayParts PortAudioBus::delayParts() const
{
    if (m_cfg.direction != AudioDirection::Output || !m_outputMatcher
        || m_outputDeviceRate <= 0) {
        return {};
    }
    const double bufferMs = 1000.0 * static_cast<double>(outputCallbackFramesNow())
                            / static_cast<double>(m_outputDeviceRate);
    const qint64 latencyNs = m_outputLatencyNs.load(std::memory_order_acquire);
    const double latencyMs = latencyNs > 0 ? static_cast<double>(latencyNs) / 1e6 : 0.0;
    return m_outputMatcher->delayParts(bufferMs, latencyMs);
}

std::optional<DeviceRateMatcherStats> PortAudioBus::matcherStats() const
{
    if (!m_outputMatcher) {
        return std::nullopt;
    }
    return m_outputMatcher->stats();
}

void PortAudioBus::restartClockMatch()
{
    if (m_outputMatcher) {
        m_outputMatcher->requestRestart();
    }
}

qint64 PortAudioBus::pull(char* data, qint64 maxBytes) {
    if (!m_stream) { return 0; }
    if (m_cfg.direction != AudioDirection::Input) { return 0; }
    const int maxFloats = static_cast<int>(maxBytes / sizeof(float));
    const qint64 ringSize = static_cast<qint64>(m_ring.size());
    qint64 r = m_ringRead.load(std::memory_order_relaxed);
    const qint64 w = m_ringWrite.load(std::memory_order_acquire);
    float* dst = reinterpret_cast<float*>(data);
    int count = 0;
    while (count < maxFloats && r < w) {
        dst[count] = m_ring[r % ringSize];
        ++count;
        ++r;
    }
    m_ringRead.store(r, std::memory_order_release);
    return static_cast<qint64>(count) * static_cast<qint64>(sizeof(float));
}

int PortAudioBus::paCallback(const void* in, void* out,
                             unsigned long frames,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             unsigned long flags,
                             void* userData) {
    PortAudioBus* self = static_cast<PortAudioBus*>(userData);
    const qint64 ringSize = static_cast<qint64>(self->m_ring.size());

    // PortAudio reports backend-level anomalies via the callback's
    // `flags` parameter.  paOutputUnderflow = the OS audio device
    // played silence because we did not supply samples fast enough
    // at the host-API layer (independent of our internal ring);
    // paOutputOverflow = data we supplied was discarded.  We count
    // both into atomic event counters that are queryable through the
    // PortAudioBus public API for support tooling.
    if (flags & paOutputUnderflow) {
        self->m_paOutputUnderflowEvents.fetch_add(
            1, std::memory_order_relaxed);
        // 2026-05-26 KG4VCF: mirror to PerfMonitor so the in-spectrum
        // perf overlay can show "audio underruns: N in last 1 s".
        NereusSDR::PerfMonitor::instance().incAudioUnderrun();
    }
    if (flags & paOutputOverflow) {
        self->m_paOutputOverflowEvents.fetch_add(
            1, std::memory_order_relaxed);
    }
    if (flags & paPrimingOutput) {
        // Expected during stream startup; not a problem.
    }

    if (self->m_cfg.direction == AudioDirection::Output) {
        float* o = static_cast<float*>(out);
        const int previousQuantum = self->m_outputCallbackFrames.load(std::memory_order_relaxed);
        if (frames > static_cast<unsigned long>(previousQuantum)) {
            self->m_outputCallbackFrames.store(static_cast<int>(frames), std::memory_order_release);
        }
        const int channels = self->m_outputDeviceChannels;
        if (o == nullptr || channels <= 0) {
            return paContinue;
        }
        if (!self->m_outputReader.valid()) {
            std::memset(o, 0, sizeof(float) * static_cast<std::size_t>(frames)
                                  * static_cast<std::size_t>(channels));
            return paContinue;
        }

        // 2026-05-26 KG4VCF perf instrumentation: report the fill level
        // (ms of unread audio queued for the device) BEFORE this callback
        // drains its samples.  R-AUD-15: the queue is the clock matcher's,
        // whose fill the writer publishes at the device rate.
        {
            const int rateHz = self->m_outputDeviceRate;
            if (rateHz > 0) {
                const double fillFrames = self->m_outputMatcher->fillFrames();
                NereusSDR::PerfMonitor::instance().recordAudioFillMs(
                    std::max(0.0, fillFrames) * 1000.0 / static_cast<double>(rateHz));
            }
        }

        // R-AUD-15: read stereo from the matcher (it slews a dry run and
        // crossfades an overrun skip) and write it in the stream's own
        // channels, a block of kOutputChunkFrames at a time.  No lock, no
        // allocation: the scratch was sized before the stream started.
        float* const scratch = self->m_outputScratch.data();
        int done = 0;
        const int total = static_cast<int>(frames);
        while (done < total) {
            const int n = std::min(total - done, kOutputChunkFrames);
            self->m_outputReader.read(scratch, n);
            writeStereoToDevice(scratch, n, o + static_cast<std::ptrdiff_t>(done) * channels,
                                DeviceSampleFormat::Float32, channels, AudioChannelPair{},
                                true, nullptr);
            done += n;
        }
    } else {
        // Input mode: read captured samples from `in`, write to ring,
        // update m_txLevel (the audio here is destined for transmit).
        //
        // When m_inputResampler is non-null, PortAudio is delivering at
        // the device's native rate and we resample to m_negFormat.sampleRate
        // before pushing to the ring.  This is the Thetis-pattern
        // paWinWasapiExclusive-equivalent for CoreAudio AUHAL SRC bypass
        // (see open() for the full source-first rationale).  Resampler is
        // owned by this bus and only this callback writes to its state,
        // so the call is thread-safe.
        const float* i_in = static_cast<const float*>(in);
        // Deinterleave against the ACTUAL stream layout.  While the
        // resampler is engaged m_negFormat.channels reports 1 (the ring
        // carries downmixed mono), so reading the stride from there
        // would misparse a multi-channel device.  Codex review, PR #291.
        const int channels = (self->m_inputStreamChannels > 0)
                                 ? self->m_inputStreamChannels
                                 : self->m_negFormat.channels;
        const int have = static_cast<int>(frames) * channels;

        qint64 w = self->m_ringWrite.load(std::memory_order_relaxed);
        float peak = 0.0f;

        // V-HW-8: the delay probe's detector sees the device's own block
        // with the time its frame 0 reached the converter.
        InputBlockHook* const hook = self->m_inputHook.load(std::memory_order_acquire);
        if (hook != nullptr && i_in != nullptr && frames > 0) {
            const qint64 latencyNs = self->m_inputLatencyNs.load(std::memory_order_acquire);
            const std::int64_t captureNs = audioProbeCaptureNs(
                audioProbeNowNs(),
                timeInfo != nullptr ? timeInfo->currentTime : 0.0,
                timeInfo != nullptr ? timeInfo->inputBufferAdcTime : 0.0,
                static_cast<int>(frames), self->m_nativeSampleRate,
                latencyNs > 0 ? static_cast<double>(latencyNs) / 1e9 : 0.0);
            hook->onInputBlock(i_in, static_cast<int>(frames), channels,
                               self->m_nativeSampleRate, captureNs);
        }

        if (i_in == nullptr) {
            self->m_txLevel.store(0.0f, std::memory_order_release);
        } else if (self->m_inputResampler) {
            // Resample at native rate to negotiated rate.  Resampler::
            // processInto expects mono float input; for stereo input we
            // downmix to mono first into a small stack-alloc scratch.
            // Most macOS built-in mics are 1-channel anyway, so the
            // stereo branch is the rare path.
            //
            // The output of processInto goes through the ring at the
            // negotiated (requested) rate -- downstream sees the same
            // 48 kHz cadence it has always seen, just without the AUHAL
            // SRC artifacts.  Because we downmix here, the ring carries
            // ONE float per output frame and negotiatedFormat() reports
            // 1 channel to match (see reportedCaptureChannels).
            const float* monoIn = i_in;
            int monoN = static_cast<int>(frames);
            if (channels >= 2) {
                // m_monoScratch is preallocated in open() from
                // m_cfg.bufferSamples; downmixToMono clamps to its
                // capacity and returns what it actually wrote, so a
                // host API handing us an oversized block truncates
                // visibly (counter below) instead of silently.
                const int cap = static_cast<int>(self->m_monoScratch.size());
                monoN = downmixToMono(i_in, monoN, channels,
                                      self->m_monoScratch.data(), cap);
                const int dropped = static_cast<int>(frames) - monoN;
                if (dropped > 0) {
                    self->m_downmixDroppedFrames.fetch_add(
                        static_cast<quint64>(dropped),
                        std::memory_order_relaxed);
                }
                monoIn = self->m_monoScratch.data();
            }
            if (monoN <= 0) {
                self->m_txLevel.store(0.0f, std::memory_order_release);
                self->m_ringWrite.store(w, std::memory_order_release);
                return paContinue;
            }
            const int outN = self->m_inputResampler->processInto(
                monoIn, monoN,
                self->m_resampleScratch.data(),
                static_cast<int>(self->m_resampleScratch.size()));
            for (int i = 0; i < outN; ++i) {
                self->m_ring[w % ringSize] = self->m_resampleScratch[i];
                w++;
                peak = std::max(peak, std::abs(self->m_resampleScratch[i]));
            }
            self->m_txLevel.store(peak, std::memory_order_release);
        } else {
            // No resampler -- device opened at the requested rate, push
            // bytes straight through.  This is the original path,
            // unchanged.
            for (int i = 0; i < have; ++i) {
                self->m_ring[w % ringSize] = i_in[i];
                w++;
                peak = std::max(peak, std::abs(i_in[i]));
            }
            self->m_txLevel.store(peak, std::memory_order_release);
        }
        self->m_ringWrite.store(w, std::memory_order_release);
    }
    return paContinue;
}

int PortAudioBus::matchNamedDevice(const QVector<NamedDeviceCandidate>& candidates,
                                   const QString& wanted, int hostApiIndex, bool strict)
{
    const QString name = wanted.trimmed();
    int exactMatch = -1;
    int substringMatch = -1;
    int crossApiExact = -1;
    int crossApiSub = -1;
    for (int i = 0; i < candidates.size(); ++i) {
        const NamedDeviceCandidate& c = candidates[i];
        const bool sameApi = hostApiIndex < 0 || c.hostApi == hostApiIndex;
        const bool exact = c.name.compare(name, Qt::CaseInsensitive) == 0;
        const bool sub = c.name.contains(name, Qt::CaseInsensitive);
        if (sameApi && exact && exactMatch < 0) {
            exactMatch = i;
        } else if (sameApi && sub && substringMatch < 0) {
            substringMatch = i;
        } else if (!sameApi && exact && crossApiExact < 0) {
            crossApiExact = i;
        } else if (!sameApi && sub && crossApiSub < 0) {
            crossApiSub = i;
        }
    }
    if (exactMatch >= 0) {
        return exactMatch;
    }
    // R-R3-36: strict resolution never substitutes a device whose name
    // merely contains the configured one ("USB Mic 2" for "USB Mic").
    if (!strict && substringMatch >= 0) {
        return substringMatch;
    }
    if (crossApiExact >= 0) {
        return crossApiExact;
    }
    if (!strict && crossApiSub >= 0) {
        return crossApiSub;
    }
    return -1;
}

int PortAudioBus::downmixToMono(const float* interleaved, int frames,
                                int channels, float* out, int outCapacity)
{
    if (interleaved == nullptr || out == nullptr
        || frames <= 0 || outCapacity <= 0) {
        return 0;
    }
    const int ch = std::max(1, channels);
    const int n  = std::min(frames, outCapacity);
    if (ch == 1) {
        std::copy(interleaved, interleaved + n, out);
        return n;
    }
    const float inv = 1.0f / static_cast<float>(ch);
    for (int i = 0; i < n; ++i) {
        float acc = 0.0f;
        for (int c = 0; c < ch; ++c) {
            acc += interleaved[i * ch + c];
        }
        out[i] = acc * inv;
    }
    return n;
}

bool PortAudioBus::portAudioBarredForTestRun()
{
    // R-AUD-32: the one no-device rule every engine shares.
    return audioDevicesBarredForTestRun();
}

QVector<PortAudioBus::HostApiInfo> PortAudioBus::hostApis() {
    QVector<HostApiInfo> out;
    if (portAudioBarredForTestRun()) { return out; }
    const int n = Pa_GetHostApiCount();
    for (int i = 0; i < n; ++i) {
        const PaHostApiInfo* h = Pa_GetHostApiInfo(i);
        if (h) { out.push_back({i, QString::fromUtf8(h->name)}); }
    }
    return out;
}

QVector<PortAudioBus::DeviceInfo> PortAudioBus::outputDevicesFor(int hostApiIndex) {
    QVector<DeviceInfo> out;
    if (portAudioBarredForTestRun()) { return out; }
    const int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(i);
        if (!d || d->hostApi != hostApiIndex || d->maxOutputChannels <= 0) { continue; }
        // d->defaultSampleRate is double; all PortAudio host APIs report integer rates.
        out.push_back({
            i, QString::fromUtf8(d->name),
            d->maxOutputChannels, d->maxInputChannels,
            static_cast<int>(d->defaultSampleRate), d->hostApi
        });
    }
    return out;
}

QVector<PortAudioBus::DeviceInfo> PortAudioBus::inputDevicesFor(int hostApiIndex) {
    QVector<DeviceInfo> out;
    if (portAudioBarredForTestRun()) { return out; }
    const int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(i);
        if (!d || d->hostApi != hostApiIndex || d->maxInputChannels <= 0) { continue; }
        out.push_back({
            i, QString::fromUtf8(d->name),
            d->maxOutputChannels, d->maxInputChannels,
            static_cast<int>(d->defaultSampleRate), d->hostApi
        });
    }
    return out;
}

} // namespace NereusSDR
