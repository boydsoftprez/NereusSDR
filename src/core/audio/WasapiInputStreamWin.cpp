// =================================================================
// src/core/audio/WasapiInputStreamWin.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only.  See
// WasapiInputStreamWin.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-02, R-AUD-11, R-AUD-16).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/WasapiSystemWin.h"

#include "core/audio/WasapiInputStreamWin.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/RealtimeAudioPriority.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace NereusSDR {

namespace {

// The stream thread wakes at least this often to see a stop request
// while a device sends no buffer events.
constexpr DWORD kEventWaitMs = 2000;

constexpr std::int64_t kNsPerSecond = 1'000'000'000;

} // namespace

struct WasapiInputStreamWin::Impl {
    AudioStreamRequest request;
    MicChannelPick pick = MicChannelPick::Left;
    IAudioInputSink* inputSink = nullptr;

    std::thread thread;
    WasapiEventHandle stopEvent;
    std::atomic<bool> running{false};
    std::atomic<int> lastResult{int(WasapiResult::Ok)};
    QString error;   // written by the stream thread only before open() returns

    std::mutex sinkMutex;
    std::function<void(const AudioStreamEvent&)> eventSink;

    std::vector<float> stereo;   // built before open() returns
    int rate = 0;
    std::atomic<qint64> latencyNs{0};

    void run(std::promise<bool>& started);
    void post(AudioStreamEvent::Kind kind, const QString& detail);
};

void WasapiInputStreamWin::Impl::post(AudioStreamEvent::Kind kind, const QString& detail)
{
    std::function<void(const AudioStreamEvent&)> target;
    {
        std::lock_guard<std::mutex> lock(sinkMutex);
        target = eventSink;
    }
    if (target) {
        AudioStreamEvent event;
        event.kind = kind;
        event.detail = detail;
        target(event);
    }
}

void WasapiInputStreamWin::Impl::run(std::promise<bool>& started)
{
    const WasapiComScope com;
    if (!com.usable()) {
        error = QStringLiteral("COM did not start");
        lastResult.store(int(WasapiResult::Other));
        started.set_value(false);
        return;
    }
    WasapiOpenedStream stream;
    const WasapiResult result =
        wasapiOpenStream(request.deviceId, AudioDeviceDirection::Input, request.exclusive,
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
    WasapiComPtr<IAudioCaptureClient> capture;
    HRESULT hr = stream.client->GetService(kWasapiIidAudioCaptureClient, capture.putVoid());
    if (FAILED(hr) || !capture) {
        error = QStringLiteral("no capture client");
        lastResult.store(int(wasapiResultOf(hr)));
        started.set_value(false);
        return;
    }
    // A packet is never larger than the buffer.
    stereo.assign(std::size_t(2 * stream.bufferFrames), 0.0f);
    rate = stream.rate;
    latencyNs.store(qint64(stream.latencyNs));
    qCInfo(lcAudio) << "Windows audio input" << stream.deviceName
                    << (stream.exclusive ? "exclusive" : "shared") << rate << "Hz"
                    << stream.channels << "channels, period" << stream.periodFrames
                    << "frames, buffer" << stream.bufferFrames;
    running.store(true);
    started.set_value(true);

    AudioPriorityToken* priority = elevateAudioThreadPriority();   // Pro Audio (MMCSS)
    hr = stream.client->Start();
    HRESULT failure = FAILED(hr) ? hr : S_OK;

    if (SUCCEEDED(failure)) {
        const HANDLE handles[2] = {stopEvent.get(), stream.bufferEvent.get()};
        const DeviceSampleFormat format = stream.format;
        const int channels = stream.channels;
        const int maxFrames = stream.bufferFrames;
        const AudioChannelPair pair = request.pair;
        IAudioCaptureClient* captureClient = capture.get();
        IAudioInputSink* target = inputSink;
        float* out = stereo.data();
        // The device loop: no lock, no allocation, only the stream's
        // buffer calls and the event wait.
        bool stop = false;
        while (!stop) {
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
            UINT32 packet = 0;
            hr = captureClient->GetNextPacketSize(&packet);
            while (SUCCEEDED(hr) && packet > 0) {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                hr = captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                if (FAILED(hr)) {
                    break;
                }
                const int n = std::min(int(frames), maxFrames);
                if (n > 0) {
                    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
                        std::memset(out, 0, std::size_t(2 * n) * sizeof(float));
                    } else {
                        readDeviceToStereo(data, nullptr, true, format, channels, pair, pick, n,
                                           out);
                    }
                    // The packet's first frame was captured n frames ago.
                    const std::int64_t nowNs = audioProbeNowNs();
                    const std::int64_t frame0Ns = nowNs - std::int64_t(n) * kNsPerSecond / rate;
                    if (target != nullptr) {
                        target->onInput(out, n, rate, frame0Ns);
                    }
                }
                hr = captureClient->ReleaseBuffer(frames);
                if (FAILED(hr)) {
                    break;
                }
                hr = captureClient->GetNextPacketSize(&packet);
            }
            if (FAILED(hr)) {
                failure = hr;
                stop = true;
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
        qCWarning(lcAudio) << "Windows audio input stopped:" << stream.deviceName
                           << QStringLiteral("0x%1").arg(quint32(failure), 8, 16, QLatin1Char('0'));
        post(kind, stream.deviceName);
    }
}

WasapiInputStreamWin::WasapiInputStreamWin(AudioStreamRequest request, MicChannelPick pick,
                                           IAudioInputSink* sink)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->request = std::move(request);
    m_impl->pick = pick;
    m_impl->inputSink = sink;
}

WasapiInputStreamWin::~WasapiInputStreamWin()
{
    close();
}

bool WasapiInputStreamWin::open()
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
    if (!ready.get()) {
        d.thread.join();
        d.stopEvent.reset();
        d.stereo.clear();
        const WasapiResult result = WasapiResult(d.lastResult.load());
        qCWarning(lcAudio) << "Windows audio input did not open:" << d.error;
        // R-AUD-11: a device another program holds posts DeviceBusy.
        if (const std::optional<AudioStreamEvent::Kind> kind = wasapiStreamEvent(result)) {
            d.post(*kind, d.error);
        }
        return false;
    }
    return true;
}

void WasapiInputStreamWin::close()
{
    Impl& d = *m_impl;
    if (d.thread.joinable()) {
        SetEvent(d.stopEvent.get());
        d.thread.join();
    }
    d.running.store(false);
    d.stopEvent.reset();
    d.stereo.clear();
}

bool WasapiInputStreamWin::isOpen() const
{
    return m_impl->running.load();
}

QString WasapiInputStreamWin::errorString() const
{
    return m_impl->error;
}

int WasapiInputStreamWin::sampleRate() const
{
    return m_impl->rate;
}

std::optional<std::int64_t> WasapiInputStreamWin::inputLatencyNs() const
{
    const qint64 latency = m_impl->latencyNs.load();
    if (latency <= 0) {
        return std::nullopt;
    }
    return std::int64_t(latency);
}

void WasapiInputStreamWin::setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink)
{
    std::lock_guard<std::mutex> lock(m_impl->sinkMutex);
    m_impl->eventSink = std::move(sink);
}

WasapiResult WasapiInputStreamWin::lastOpenResult() const
{
    return WasapiResult(m_impl->lastResult.load());
}

} // namespace NereusSDR
