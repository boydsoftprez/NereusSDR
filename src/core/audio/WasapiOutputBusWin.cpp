// =================================================================
// src/core/audio/WasapiOutputBusWin.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only.  See
// WasapiOutputBusWin.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-02, R-AUD-11, R-AUD-16).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-11): openRefusedInUse()
//               reads the last open's result.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-10: setClockMatchWritePacket(): a writer of whole packets
//               (remote playback) tells the bus's clock matcher its packet
//               (R-AUD-15, bench regression). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/WasapiSystemWin.h"

#include "core/audio/WasapiOutputBusWin.h"

#include "core/LogCategories.h"
#include "core/MemoryLock.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/RealtimeAudioPriority.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace NereusSDR {

namespace {

// The stream thread wakes at least this often to see a stop request
// while a device sends no buffer events (a sleep, a stalled driver).
constexpr DWORD kEventWaitMs = 2000;

} // namespace

struct WasapiOutputBusWin::Impl {
    AudioStreamRequest request;

    std::thread thread;
    WasapiEventHandle stopEvent;
    std::atomic<bool> running{false};
    std::atomic<int> lastResult{int(WasapiResult::Ok)};
    QString error;   // written by the stream thread only before open() returns

    std::mutex sinkMutex;
    std::function<void(const AudioStreamEvent&)> sink;

    // Built on the stream thread before open() returns; destroyed by
    // close() after the thread has ended.
    std::unique_ptr<DeviceRateMatcher> matcher;
    std::size_t matcherBytes = 0;
    MatcherReader reader;
    std::vector<float> scratch;

    // Set before open() returns.
    int rate = 0;
    int channels = 0;
    int periodFrames = 0;
    int bufferFrames = 0;
    DeviceSampleFormat format = DeviceSampleFormat::Float32;
    std::atomic<qint64> latencyNs{0};
    std::atomic<float> rxLevel{0.0f};

    void run(std::promise<bool>& started);
    void post(AudioStreamEvent::Kind kind, const QString& detail);
    void releaseMatcher();
};

void WasapiOutputBusWin::Impl::post(AudioStreamEvent::Kind kind, const QString& detail)
{
    std::function<void(const AudioStreamEvent&)> target;
    {
        std::lock_guard<std::mutex> lock(sinkMutex);
        target = sink;
    }
    if (target) {
        AudioStreamEvent event;
        event.kind = kind;
        event.detail = detail;
        target(event);
    }
}

void WasapiOutputBusWin::Impl::releaseMatcher()
{
    if (matcher) {
        unlockMemory(matcher->ring(), matcherBytes);
    }
    reader = MatcherReader();
    matcher.reset();
    matcherBytes = 0;
    scratch.clear();
}

void WasapiOutputBusWin::Impl::run(std::promise<bool>& started)
{
    // COM on this thread for the stream's whole life (the stream objects
    // below are released before it ends).
    const WasapiComScope com;
    if (!com.usable()) {
        error = QStringLiteral("COM did not start");
        lastResult.store(int(WasapiResult::Other));
        started.set_value(false);
        return;
    }
    WasapiOpenedStream stream;
    const WasapiResult result =
        wasapiOpenStream(request.deviceId, AudioDeviceDirection::Output, request.exclusive,
                         request.bufferFrames, stream, error);
    lastResult.store(int(result));
    if (result != WasapiResult::Ok) {
        started.set_value(false);
        return;
    }
    if (request.pair.firstChannel < 1 || request.pair.firstChannel > stream.channels) {
        error = QStringLiteral("the device has no channel %1").arg(request.pair.firstChannel);
        lastResult.store(int(WasapiResult::Other));
        started.set_value(false);
        return;
    }

    // R-AUD-15: the clock matcher, 48 kHz in, the device's rate out, read
    // one period at a time (the device buffer is the period).
    DeviceRateMatcher::Config config;
    config.inRate = 48000;
    config.outRate = stream.rate;
    config.callbackFrames = stream.periodFrames;
    config.delayMs = std::max(0, request.delayMs);
    auto built = std::make_unique<DeviceRateMatcher>(config);
    if (!built->valid()) {
        error = QStringLiteral("the clock matcher cannot run at %1 Hz").arg(stream.rate);
        lastResult.store(int(WasapiResult::Other));
        started.set_value(false);
        return;
    }
    matcherBytes = DeviceRateMatcher::ringBytes(config);
    lockMemory(built->ring(), matcherBytes, "WasapiOutputBusWin::matcher");
    reader = built->makeReader();
    scratch.assign(std::size_t(2 * stream.periodFrames), 0.0f);
    matcher = std::move(built);

    WasapiComPtr<IAudioRenderClient> render;
    HRESULT hr = stream.client->GetService(kWasapiIidAudioRenderClient, render.putVoid());
    if (FAILED(hr) || !render) {
        error = QStringLiteral("no render client");
        lastResult.store(int(wasapiResultOf(hr)));
        started.set_value(false);
        return;
    }
    // Silence in the whole buffer before the stream starts.
    BYTE* first = nullptr;
    if (SUCCEEDED(render->GetBuffer(UINT32(stream.bufferFrames), &first))) {
        render->ReleaseBuffer(UINT32(stream.bufferFrames), AUDCLNT_BUFFERFLAGS_SILENT);
    }

    rate = stream.rate;
    channels = stream.channels;
    periodFrames = stream.periodFrames;
    bufferFrames = stream.bufferFrames;
    format = stream.format;
    latencyNs.store(qint64(stream.latencyNs));
    qCInfo(lcAudio) << "Windows audio output" << stream.deviceName
                    << (stream.exclusive ? "exclusive" : "shared") << rate << "Hz" << channels
                    << "channels, period" << periodFrames << "frames, buffer" << bufferFrames;
    running.store(true);
    started.set_value(true);

    AudioPriorityToken* priority = elevateAudioThreadPriority();   // Pro Audio (MMCSS)
    hr = stream.client->Start();
    HRESULT failure = FAILED(hr) ? hr : S_OK;

    if (SUCCEEDED(failure)) {
        const HANDLE handles[2] = {stopEvent.get(), stream.bufferEvent.get()};
        const bool exclusive = stream.exclusive;
        const std::size_t bytesPerFrame =
            std::size_t(deviceSampleBytes(format)) * std::size_t(channels);
        const AudioChannelPair pair = request.pair;
        IAudioClient* client = stream.client.get();
        IAudioRenderClient* renderClient = render.get();
        float* stereo = scratch.data();
        // The device loop: no lock, no allocation, only the stream's
        // buffer calls and the event wait.
        for (;;) {
            const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, kEventWaitMs);
            if (wait == WAIT_OBJECT_0) {
                break;   // close()
            }
            if (wait == WAIT_TIMEOUT) {
                continue;
            }
            if (wait != WAIT_OBJECT_0 + 1) {
                failure = E_FAIL;
                break;
            }
            UINT32 padding = 0;
            if (!exclusive) {
                hr = client->GetCurrentPadding(&padding);
                if (FAILED(hr)) {
                    failure = hr;
                    break;
                }
            }
            const int frames = wasapiFramesToWrite(exclusive, bufferFrames, int(padding));
            if (frames <= 0) {
                continue;
            }
            BYTE* data = nullptr;
            hr = renderClient->GetBuffer(UINT32(frames), &data);
            if (FAILED(hr)) {
                failure = hr;
                break;
            }
            int done = 0;
            while (done < frames) {
                const int n = std::min(frames - done, periodFrames);
                reader.read(stereo, n);
                writeStereoToDevice(stereo, n, data + std::size_t(done) * bytesPerFrame, format,
                                    channels, pair, true, nullptr);
                done += n;
            }
            hr = renderClient->ReleaseBuffer(UINT32(frames), 0);
            if (FAILED(hr)) {
                failure = hr;
                break;
            }
        }
        stream.client->Stop();
    }
    leaveAudioThreadPriority(priority);

    if (FAILED(failure)) {
        running.store(false);
        const WasapiResult why = wasapiResultOf(failure);
        const AudioStreamEvent::Kind kind =
            wasapiRunningStreamEvent(why, wasapiEndpointActive(stream.device.get()));
        qCWarning(lcAudio) << "Windows audio output stopped:" << stream.deviceName
                           << QStringLiteral("0x%1").arg(quint32(failure), 8, 16, QLatin1Char('0'));
        post(kind, stream.deviceName);
    }
}

WasapiOutputBusWin::WasapiOutputBusWin(AudioStreamRequest request)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->request = std::move(request);
}

WasapiOutputBusWin::~WasapiOutputBusWin()
{
    close();
}

bool WasapiOutputBusWin::open(const AudioFormat& /*format*/)
{
    close();
    Impl& d = *m_impl;
    d.error.clear();
    // R-AUD-32: no test run opens a device.
    if (audioDevicesBarredForTestRun()) {
        d.error = QStringLiteral("Audio devices are not opened in a test run");
        d.lastResult.store(int(WasapiResult::Other));
        return false;
    }
    if (!d.stopEvent.create()) {
        d.error = QStringLiteral("no stop event");
        d.lastResult.store(int(WasapiResult::Other));
        return false;
    }
    std::promise<bool> started;
    std::future<bool> ready = started.get_future();
    d.thread = std::thread([&d, promise = std::move(started)]() mutable { d.run(promise); });
    const bool ok = ready.get();
    if (!ok) {
        d.thread.join();
        d.releaseMatcher();
        d.stopEvent.reset();
        const WasapiResult result = WasapiResult(d.lastResult.load());
        qCWarning(lcAudio) << "Windows audio output did not open:" << d.error;
        // R-AUD-11: a device another program holds posts DeviceBusy.
        if (const std::optional<AudioStreamEvent::Kind> kind = wasapiStreamEvent(result)) {
            d.post(*kind, d.error);
        }
        return false;
    }
    return true;
}

void WasapiOutputBusWin::close()
{
    Impl& d = *m_impl;
    if (d.thread.joinable()) {
        SetEvent(d.stopEvent.get());
        d.thread.join();
    }
    d.running.store(false);
    d.releaseMatcher();
    d.stopEvent.reset();
}

bool WasapiOutputBusWin::isOpen() const
{
    return m_impl->running.load();
}

qint64 WasapiOutputBusWin::push(const char* data, qint64 bytes)
{
    Impl& d = *m_impl;
    if (!d.matcher || data == nullptr || bytes <= 0) {
        return 0;
    }
    const int frames = int(bytes / qint64(2 * sizeof(float)));
    if (frames <= 0) {
        return 0;
    }
    const float* in = reinterpret_cast<const float*>(data);
    float peak = 0.0f;
    for (int i = 0; i < frames * 2; ++i) {
        peak = std::max(peak, std::abs(in[i]));
    }
    d.matcher->write(in, frames, audioProbeNowNs());
    d.rxLevel.store(peak, std::memory_order_release);
    return bytes;
}

qint64 WasapiOutputBusWin::pull(char* /*data*/, qint64 /*maxBytes*/)
{
    return 0;
}

void WasapiOutputBusWin::flush()
{
    if (m_impl->matcher) {
        m_impl->matcher->requestFlush();
    }
}

std::optional<IAudioBus::OutputPacing> WasapiOutputBusWin::outputPacing() const
{
    const Impl& d = *m_impl;
    if (!d.matcher) {
        return std::nullopt;
    }
    // As PortAudioBus: every frame the device asked for, the matcher's
    // fill and its automatic size, at the device rate.
    DeviceRateMatcher& matcher = *d.matcher;
    const MatcherRingHeader* ring = matcher.ring();
    OutputPacing pacing;
    pacing.consumedFrames = ring->requested.load(std::memory_order_acquire);
    pacing.queuedFrames = std::max(0, int(std::lround(matcher.fillFrames())));
    pacing.capacityFrames = int(ring->rsizeFrames.load(std::memory_order_acquire));
    pacing.callbackFrames = d.periodFrames;
    if (const qint64 latency = d.latencyNs.load(); latency > 0) {
        pacing.deviceLatencyNs = latency;
    }
    return pacing;
}

float WasapiOutputBusWin::rxLevel() const
{
    return m_impl->rxLevel.load(std::memory_order_acquire);
}

QString WasapiOutputBusWin::backendName() const
{
    return audioEngineLabel(m_impl->request.exclusive ? AudioEngineKind::WindowsExclusive
                                                      : AudioEngineKind::WindowsShared);
}

AudioFormat WasapiOutputBusWin::negotiatedFormat() const
{
    // What push() takes; the matcher and the device format are this
    // bus's own (takesStereoMix).
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    return format;
}

QString WasapiOutputBusWin::errorString() const
{
    return m_impl->error;
}

bool WasapiOutputBusWin::openRefusedInUse() const
{
    // R-AUD-11: the open's DeviceBusy is posted before the engine sets the
    // event sink, so the open result carries it instead.
    return !isOpen() && wasapiOpenResult(lastOpenResult()) == AudioOpenResult::InUse;
}

void WasapiOutputBusWin::setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink)
{
    std::lock_guard<std::mutex> lock(m_impl->sinkMutex);
    m_impl->sink = std::move(sink);
}

AudioDelayParts WasapiOutputBusWin::delayParts() const
{
    const Impl& d = *m_impl;
    if (!d.matcher || d.rate <= 0) {
        return {};
    }
    const double bufferMs = 1000.0 * double(d.periodFrames) / double(d.rate);
    const qint64 latency = d.latencyNs.load();
    const double latencyMs = latency > 0 ? double(latency) / 1e6 : 0.0;
    return d.matcher->delayParts(bufferMs, latencyMs);
}

std::optional<DeviceRateMatcherStats> WasapiOutputBusWin::matcherStats() const
{
    if (!m_impl->matcher) {
        return std::nullopt;
    }
    return m_impl->matcher->stats();
}

void WasapiOutputBusWin::restartClockMatch()
{
    if (m_impl->matcher) {
        m_impl->matcher->requestRestart();
    }
}

void WasapiOutputBusWin::setClockMatchWritePacket(int frames, bool waited)
{
    if (m_impl->matcher) {
        m_impl->matcher->setWritePacketFrames(frames, waited);
    }
}

void WasapiOutputBusWin::requestFadeOut()
{
    if (m_impl->reader.valid()) {
        m_impl->reader.requestFadeOut();
    }
}

bool WasapiOutputBusWin::fadedOut() const
{
    const Impl& d = *m_impl;
    if (!d.running.load() || !d.reader.valid()) {
        return true;
    }
    return d.reader.fadedOut();
}

WasapiResult WasapiOutputBusWin::lastOpenResult() const
{
    return WasapiResult(m_impl->lastResult.load());
}

} // namespace NereusSDR
