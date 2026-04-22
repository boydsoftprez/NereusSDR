// =================================================================
// src/core/audio/PortAudioBus.cpp  (NereusSDR)
// =================================================================
// See PortAudioBus.h for contract. NereusSDR-original.
// =================================================================

#include "PortAudioBus.h"

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
PaDeviceIndex resolveDevice(const PortAudioConfig& cfg,
                            bool wantOutput,
                            int requestedChannels)
{
    const int deviceCount = Pa_GetDeviceCount();
    if (deviceCount <= 0) {
        return paNoDevice;
    }

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
            if (d != paNoDevice && directionOk(Pa_GetDeviceInfo(d))) {
                return d;
            }
        }
    }

    // 3. Global default.
    const PaDeviceIndex def = wantOutput
        ? Pa_GetDefaultOutputDevice()
        : Pa_GetDefaultInputDevice();
    if (def != paNoDevice && directionOk(Pa_GetDeviceInfo(def))) {
        return def;
    }

    // 4. First enumerated device with matching direction — prefer one
    //    that satisfies the requested channel count, fall back to any
    //    direction-valid device if none does (Pa_OpenStream will then
    //    surface a clear paInvalidChannelCount error, better than
    //    silently picking step 3's default which may not even exist).
    PaDeviceIndex anyDirection = paNoDevice;
    for (int i = 0; i < deviceCount; ++i) {
        const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
        if (!directionOk(di)) { continue; }
        if (capacityOk(di))   { return i; }
        if (anyDirection == paNoDevice) { anyDirection = i; }
    }
    return anyDirection;
}

} // namespace

PortAudioBus::PortAudioBus() {
    // 1 second stereo float ring at the nominal default rate. The push
    // path wraps modulo ring size, so a longer stream than 1 s simply
    // overwrites the oldest unread samples (underruns surface as silence
    // in paCallback, not overruns).
    m_ring.resize(48000 * 2);
}

PortAudioBus::~PortAudioBus() {
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
    params.channelCount              = format.channels;
    params.sampleFormat              = paFloat32;
    params.suggestedLatency          = wantOutput ? di->defaultLowOutputLatency
                                                  : di->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(
        &m_stream,
        wantOutput ? nullptr : &params,
        wantOutput ? &params : nullptr,
        format.sampleRate, m_cfg.bufferSamples,
        paClipOff, &PortAudioBus::paCallback, this);

    if (err != paNoError) {
        m_err = QString::fromUtf8(Pa_GetErrorText(err));
        m_stream = nullptr;
        m_negFormat = {};
        m_backendName.clear();
        return false;
    }

    Pa_StartStream(m_stream);
    m_negFormat = format;

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
}

qint64 PortAudioBus::push(const char* data, qint64 bytes) {
    if (!m_stream) { return 0; }
    if (m_cfg.direction != AudioDirection::Output) { return 0; }
    const int floatCount = static_cast<int>(bytes / sizeof(float));
    const qint64 ringSize = static_cast<qint64>(m_ring.size());
    qint64 w = m_ringWrite.load(std::memory_order_relaxed);
    const float* in = reinterpret_cast<const float*>(data);
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
                             unsigned long /*flags*/,
                             void* userData) {
    PortAudioBus* self = static_cast<PortAudioBus*>(userData);
    const qint64 ringSize = static_cast<qint64>(self->m_ring.size());

    if (self->m_cfg.direction == AudioDirection::Output) {
        float* o = static_cast<float*>(out);
        const int want = static_cast<int>(frames) * self->m_negFormat.channels;

        qint64 r = self->m_ringRead.load(std::memory_order_relaxed);
        const qint64 w = self->m_ringWrite.load(std::memory_order_acquire);

        for (int i = 0; i < want; ++i) {
            if (r < w) {
                o[i] = self->m_ring[r % ringSize];
                r++;
            } else {
                o[i] = 0.0f;  // underrun -> silence
            }
        }
        self->m_ringRead.store(r, std::memory_order_release);
    } else {
        // Input mode: read captured samples from `in`, write to ring,
        // update m_txLevel (the audio here is destined for transmit).
        const float* i_in = static_cast<const float*>(in);
        const int have = static_cast<int>(frames) * self->m_negFormat.channels;

        qint64 w = self->m_ringWrite.load(std::memory_order_relaxed);
        float peak = 0.0f;
        if (i_in != nullptr) {
            for (int i = 0; i < have; ++i) {
                self->m_ring[w % ringSize] = i_in[i];
                w++;
                peak = std::max(peak, std::abs(i_in[i]));
            }
        }
        self->m_ringWrite.store(w, std::memory_order_release);
        self->m_txLevel.store(peak, std::memory_order_release);
    }
    return paContinue;
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
