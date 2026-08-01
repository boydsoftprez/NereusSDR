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
// =================================================================

#include "PortAudioBus.h"
#include "../LogCategories.h"
#include "../MemoryLock.h"
#include "../PerfMonitor.h"
#include "../Resampler.h"

#include <portaudio.h>

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
PaDeviceIndex resolveDevice(const PortAudioConfig& inCfg,
                            bool wantOutput,
                            int requestedChannels)
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
#ifdef __APPLE__
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
        const QString wanted = cfg.deviceName.trimmed();
        PaDeviceIndex exactMatch     = paNoDevice;
        PaDeviceIndex substringMatch = paNoDevice;
        PaDeviceIndex crossApiExact  = paNoDevice;
        PaDeviceIndex crossApiSub    = paNoDevice;

        for (int i = 0; i < deviceCount; ++i) {
            const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
            if (!directionOk(di)) { continue; }
            const QString name = QString::fromUtf8(di->name);
            const bool sameApi = (cfg.hostApiIndex < 0)
                                 || (di->hostApi == cfg.hostApiIndex);
            const bool exact = (name.compare(wanted, Qt::CaseInsensitive) == 0);
            const bool sub   = name.contains(wanted, Qt::CaseInsensitive);

            if (sameApi && exact && exactMatch == paNoDevice) {
                exactMatch = i;
            } else if (sameApi && sub && substringMatch == paNoDevice) {
                substringMatch = i;
            } else if (!sameApi && exact && crossApiExact == paNoDevice) {
                crossApiExact = i;
            } else if (!sameApi && sub && crossApiSub == paNoDevice) {
                crossApiSub = i;
            }
        }
        if (exactMatch     != paNoDevice) { return exactMatch; }
        if (substringMatch != paNoDevice) { return substringMatch; }
        if (crossApiExact  != paNoDevice) { return crossApiExact; }
        if (crossApiSub    != paNoDevice) { return crossApiSub; }
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
    m_ring.resize(4800 * 2);

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

void PortAudioBus::setConfig(const PortAudioConfig& cfg) {
    m_cfg = cfg;
}

bool PortAudioBus::open(const AudioFormat& format) {
    if (m_stream) {
        close();
    }

    const bool wantOutput = (m_cfg.direction == AudioDirection::Output);

    PaStreamParameters params;
    PaError err = paNoError;
    const PaDeviceInfo* di = nullptr;

    params.device = resolveDevice(m_cfg, wantOutput, format.channels);
    if (params.device == paNoDevice) {
        m_err = wantOutput
            ? QStringLiteral("No output device found")
            : QStringLiteral("No input device found");
        return false;
    }
    di = Pa_GetDeviceInfo(params.device);
    if (di == nullptr) {
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

    // Callback-visible state is now fully published; safe to start.
    err = Pa_StartStream(m_stream);
    if (err != paNoError) {
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
        return false;
    }

    // Defensive null-check on host-API lookup. With a device handed back
    // by Pa_GetDefault{Output,Input}Device this should never be null, but
    // keep the backend name well-defined if it ever is.
    const PaHostApiInfo* hai = Pa_GetHostApiInfo(di->hostApi);
    if (hai != nullptr && hai->name != nullptr) {
        m_backendName = QString::fromUtf8(hai->name);
    } else {
        m_backendName.clear();
    }
    return true;
}

void PortAudioBus::close() {
    if (!m_stream) {
        return;
    }
    Pa_StopStream(m_stream);
    Pa_CloseStream(m_stream);
    m_stream = nullptr;
    // Release the input resampler + its scratch buffer.  Safe here
    // because Pa_StopStream above has joined the audio thread, so no
    // more paCallback invocations can be in flight.
    m_inputResampler.reset();
    m_resampleScratch.clear();
    m_nativeSampleRate = 0;
    // Cumulative drop / underrun / PA-flag counters remain queryable
    // via ringOverrunEvents() / ringOverrunSamples() /
    // ringUnderrunEvents() and the m_paOutputUnderflowEvents /
    // m_paOutputOverflowEvents members.  Used by future support
    // tooling; no per-close log spam.
}

qint64 PortAudioBus::push(const char* data, qint64 bytes) {
    if (!m_stream) { return 0; }
    if (m_cfg.direction != AudioDirection::Output) { return 0; }
    const int floatCount = static_cast<int>(bytes / sizeof(float));
    const qint64 ringSize = static_cast<qint64>(m_ring.size());
    qint64 w = m_ringWrite.load(std::memory_order_relaxed);
    const float* in = reinterpret_cast<const float*>(data);

    // Drop-oldest accounting: if this push would put the writer more than
    // one ring's worth ahead of the reader, the oldest unread samples
    // about to be modulo-overwritten are effectively dropped.  We do NOT
    // advance m_ringRead from here (that would race with paCallback's own
    // store; only the audio thread writes to m_ringRead).  Instead the
    // paCallback detects the same condition on its next entry and skips
    // forward to the oldest still-valid sample.  Counting the event here
    // gives diagnostics a single producer-side perspective.
    const qint64 readPos = m_ringRead.load(std::memory_order_acquire);
    const qint64 afterWrite = w + floatCount;
    if (afterWrite - readPos > ringSize) {
        m_dropEvents.fetch_add(1, std::memory_order_relaxed);
        m_dropSamples.fetch_add(
            static_cast<quint64>(afterWrite - readPos - ringSize),
            std::memory_order_relaxed);
        // Counters are observable via ringOverrunEvents() /
        // ringOverrunSamples().  paCallback performs the catch-up jump
        // on its next entry; the crossfade ramp on resume keeps the
        // event inaudible to the listener.
    }

    float peak = 0.0f;
    for (int i = 0; i < floatCount; ++i) {
        m_ring[w % ringSize] = in[i];
        w++;
        peak = std::max(peak, std::abs(in[i]));
    }
    m_ringWrite.store(w, std::memory_order_release);
    m_rxLevel.store(peak, std::memory_order_release);
    return bytes;
}

void PortAudioBus::flush() {
    // Issue #201: drop any unread samples queued in the ring so they
    // don't keep draining out the device after a mute click.  Output
    // mode: callers (AudioEngine on mute) want unread samples dropped.
    // Input mode: callers (a future Setup → Audio "drop stale capture"
    // path) want unread captured samples dropped.  In both modes the
    // operation is the same: equalize read/write cursors atomically.
    //
    // Race with the audio thread:
    //  - Output mode: paCallback advances m_ringRead; push() advances
    //    m_ringWrite.  If we set ringRead := ringWrite atomically, the
    //    callback may have JUST advanced ringRead one tick before our
    //    store; the store still leaves r ≤ w, so the next callback
    //    iteration reads `r < w` as false and outputs silence.  No
    //    torn-read window.
    //  - Input mode: paCallback advances m_ringWrite; pull() advances
    //    m_ringRead.  Symmetric reasoning applies.
    //
    // No mutex needed — the cursors are std::atomic<qint64> and the
    // single store is sequenced after the load by acquire/release
    // ordering.  The PortAudio device's own internal output buffer
    // (~5–20 ms latency on Core Audio / WASAPI) still plays its
    // already-handed-off samples; that's below the threshold of
    // perception and outside this layer's reach.
    if (!m_stream) {
        return;
    }
    const qint64 w = m_ringWrite.load(std::memory_order_acquire);
    m_ringRead.store(w, std::memory_order_release);
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
                             const PaStreamCallbackTimeInfo* /*timeInfo*/,
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
        const int want = static_cast<int>(frames) * self->m_negFormat.channels;

        qint64 r = self->m_ringRead.load(std::memory_order_relaxed);
        const qint64 w = self->m_ringWrite.load(std::memory_order_acquire);

        // 2026-05-26 KG4VCF perf instrumentation: report the ring fill
        // level (ms of unread audio still in the producer->consumer
        // ring) BEFORE this callback drains its samples.  This is the
        // speakers-output path on every platform (CoreAudioHalBus
        // covers only the VAX digital-app channels); the perf overlay
        // surfaces avg / min over a 2 s window.  If min craters
        // toward 0 we are about to underrun even when paOutputUnderflow
        // is still 0.
        {
            const qint64 fillSamples = w - r;  // total samples (interleaved)
            const int    rateHz      = self->m_negFormat.sampleRate;
            const int    fillChans   = self->m_negFormat.channels;
            if (rateHz > 0 && fillChans > 0) {
                const double samplesPerMs =
                    static_cast<double>(rateHz)
                    * static_cast<double>(fillChans) / 1000.0;
                const double fillMs =
                    static_cast<double>(fillSamples) / samplesPerMs;
                NereusSDR::PerfMonitor::instance().recordAudioFillMs(fillMs);
            }
        }

        // Drop-oldest catch-up: if we have fallen so far behind that the
        // writer stomped on our read position (w - r exceeds the ring
        // size), the bytes at our current r have been overwritten with
        // newer samples and reading them would produce scrambled audio.
        // Jump forward to the oldest still-valid sample (w - ringSize)
        // so we resume on contiguous, in-order audio.  The crossfade
        // counter below smooths the discontinuity so the listener hears
        // a brief volume dip instead of a hard click.
        bool startCrossfade = false;
        if (w - r > ringSize) {
            r = w - ringSize;
            startCrossfade = true;
        }

        // Crossfade between the previous sample value and the new ring
        // sample over kCrossfadeFrames stereo frames.  Triggered on
        // drop-oldest catch-up AND on underrun-to-resume transitions.
        // ~3 ms at 48 kHz / 2 channels — short enough to be inaudible as
        // a "dip" but long enough to mask the click that a hard jump or
        // silence-to-signal transition would otherwise produce.
        const int channels = self->m_negFormat.channels;
        float lastL = self->m_lastOutL;
        float lastR = self->m_lastOutR;
        int crossfadeRem = self->m_crossfadeFramesRem;
        if (startCrossfade && crossfadeRem == 0) {
            crossfadeRem = kCrossfadeFrames;
        }

        bool wasUnderrun = (r >= w);
        // Track underrun leading edge so we count distinct events, not
        // every silent frame in a run.  Initial state (callback fired
        // with an empty ring) counts as one event.
        bool sawSilenceStart = false;
        if (wasUnderrun) {
            self->m_underrunEvents.fetch_add(
                1, std::memory_order_relaxed);
            sawSilenceStart = true;
        }
        for (int i = 0; i < want; ++i) {
            float target;
            if (r < w) {
                target = self->m_ring[r % ringSize];
                r++;
                // Underrun-to-resume edge: start a fresh crossfade to
                // bring the listener gently from silence (or stale
                // last-sample) up to the live signal.
                if (wasUnderrun && crossfadeRem == 0) {
                    crossfadeRem = kCrossfadeFrames;
                }
                wasUnderrun = false;
            } else {
                target = 0.0f;  // underrun -> silence (with crossfade below)
                if (!wasUnderrun && !sawSilenceStart) {
                    // Transitioned from "had data" to "empty" mid-callback.
                    self->m_underrunEvents.fetch_add(1, std::memory_order_relaxed);
                    sawSilenceStart = true;
                }
                wasUnderrun = true;
            }

            // Per-channel last-sample tracking (stereo only path is
            // exercised in practice; mono falls through cleanly).
            float& last = ((i & 1) && channels >= 2) ? lastR : lastL;
            if (crossfadeRem > 0) {
                const float t = 1.0f - (static_cast<float>(crossfadeRem)
                                        / static_cast<float>(kCrossfadeFrames));
                o[i] = last + (target - last) * t;
                // Decrement once per stereo frame (after the R sample).
                if (channels < 2 || (i & 1) == 1) {
                    crossfadeRem--;
                }
            } else {
                o[i] = target;
            }
            last = o[i];
        }
        self->m_ringRead.store(r, std::memory_order_release);
        self->m_lastOutL = lastL;
        self->m_lastOutR = lastR;
        self->m_crossfadeFramesRem = crossfadeRem;
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

QVector<PortAudioBus::HostApiInfo> PortAudioBus::hostApis() {
    QVector<HostApiInfo> out;
    const int n = Pa_GetHostApiCount();
    for (int i = 0; i < n; ++i) {
        const PaHostApiInfo* h = Pa_GetHostApiInfo(i);
        if (h) { out.push_back({i, QString::fromUtf8(h->name)}); }
    }
    return out;
}

QVector<PortAudioBus::DeviceInfo> PortAudioBus::outputDevicesFor(int hostApiIndex) {
    QVector<DeviceInfo> out;
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
