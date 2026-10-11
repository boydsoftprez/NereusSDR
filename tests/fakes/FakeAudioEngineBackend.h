// =================================================================
// tests/fakes/FakeAudioEngineBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test fake for the native audio
// engines (R-AUD-03, R-AUD-32).  No device.
//
// An engine backend whose device list, defaults, running flag and notices
// the test sets.  It records the thread every enumerate() and
// defaultDeviceId() call runs on, can hold enumerate() until the test
// releases it, records every createOutput() / createInput() request, and
// makes FakeMatcherAudioBus outputs and FakeAudioInputStream inputs.
// Every setter and postNotice() may be called from any thread.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03, R-AUD-32). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 (R-AUD-06): fadeRequests() counts
//               the fades asked of every output it made.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 fix: setOutputCreatedHook() tells
//               a test of each output made.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 8 (R-AUD-18): setWorkgroupDevice()
//               gives the outputs made for one device id an
//               audioWorkgroupDevice(). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: early-review fix wave (R-AUD-06, R-AUD-08, R-AUD-16):
//               outputs can fail to open by device id
//               (setFailingOutputs), use a set callback size and fade
//               time, and report which are still alive (outputAlive);
//               opensOneStreamAtATime() is settable.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: final review fix (R-AUD-03, R-AUD-06): setRescanHook()
//               runs inside rescan(); sinkSetsDuringCalls() counts notice
//               sinks set while another call is inside the backend.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDelayProbe.h"
#include "core/audio/IAudioEngineBackend.h"
#include "FakeMatcherAudioBus.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace NereusSDR {

// An input whose pumpForTest() calls its sink as a device callback would.
class FakeAudioInputStream final : public IAudioInputStream {
public:
    FakeAudioInputStream(AudioStreamRequest request, MicChannelPick pick, IAudioInputSink* sink)
        : m_request(std::move(request))
        , m_pick(pick)
        , m_sink(sink)
    {
    }

    bool open() override
    {
        if (!m_openResult) {
            m_error = QStringLiteral("Fake open failure");
            return false;
        }
        m_open.store(true);
        return true;
    }
    void close() override { m_open.store(false); }
    bool isOpen() const override { return m_open.load(); }
    QString errorString() const override { return m_error; }
    int sampleRate() const override { return m_request.sampleRate; }
    std::optional<std::int64_t> inputLatencyNs() const override { return m_latencyNs; }

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_eventSink = std::move(sink);
    }

    // Calls the sink with frames of the samples set by setInputSamples()
    // (repeated), or silence.  Returns the frames delivered.
    int pumpForTest(int frames)
    {
        if (!m_open.load() || m_sink == nullptr || frames <= 0) {
            return 0;
        }
        m_block.assign(std::size_t(2 * frames), 0.0f);
        if (!m_samples.empty()) {
            for (std::size_t i = 0; i < m_block.size(); ++i) {
                m_block[i] = m_samples[(m_cursor + i) % m_samples.size()];
            }
            m_cursor = (m_cursor + m_block.size()) % m_samples.size();
        }
        m_sink->onInput(m_block.data(), frames, m_request.sampleRate, audioProbeNowNs());
        return frames;
    }

    void emitEventForTest(const AudioStreamEvent& event)
    {
        std::function<void(const AudioStreamEvent&)> sink;
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            sink = m_eventSink;
        }
        if (sink) {
            sink(event);
        }
    }

    // Interleaved stereo float the input delivers.
    void setInputSamples(std::vector<float> stereo)
    {
        m_samples = std::move(stereo);
        m_cursor = 0;
    }
    void setOpenResult(bool ok) { m_openResult = ok; }
    void setInputLatencyNs(std::optional<std::int64_t> ns) { m_latencyNs = ns; }
    const AudioStreamRequest& request() const { return m_request; }
    MicChannelPick pick() const { return m_pick; }

private:
    AudioStreamRequest m_request;
    MicChannelPick m_pick;
    IAudioInputSink* m_sink;
    bool m_openResult = true;
    std::atomic<bool> m_open{false};
    QString m_error;
    std::optional<std::int64_t> m_latencyNs;
    std::vector<float> m_samples;
    std::size_t m_cursor = 0;
    std::vector<float> m_block;
    std::mutex m_sinkMutex;
    std::function<void(const AudioStreamEvent&)> m_eventSink;
};

class FakeAudioEngineBackend final : public IAudioEngineBackend {
public:
    struct InputRequest {
        AudioStreamRequest request;
        MicChannelPick pick = MicChannelPick::Left;
    };

    explicit FakeAudioEngineBackend(AudioBackendId id = AudioBackendId::CoreAudio)
        : m_id(id)
    {
    }

    // -- IAudioEngineBackend --------------------------------------------
    AudioBackendId id() const override { return m_id; }
    bool running() const override { return m_running.load(); }

    QList<AudioDeviceInfo> enumerate() override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_enumerateThreads.push_back(QThread::currentThread());
        ++m_enumerateCalls;
        enterCallLocked();
        m_cv.notify_all();
        m_cv.wait(lock, [this] { return !m_holdEnumerate; });
        --m_callsInFlight;
        return m_devices;
    }

    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_defaultThreads.push_back(QThread::currentThread());
        ++m_defaultCalls;
        enterCallLocked();
        --m_callsInFlight;
        return direction == AudioDeviceDirection::Output ? m_defaultOutput : m_defaultInput;
    }

    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_callsInFlight > 0 || m_rescansInFlight > 0) {
                ++m_sinkSetsDuringCalls;
            }
        }
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_noticeSink = std::move(sink);
    }

    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override
    {
        auto bus = std::make_unique<FakeMatcherAudioBus>(request, m_takesStereoMix,
                                                         m_callbackFrames);
        bus->setFadeCounterForTest(m_fadeRequests);
        bus->setFadeTimeForTest(m_fadeTimeMs);
        bus->setClosedUnfadedCounterForTest(m_closedUnfaded);
        auto alive = std::make_shared<std::atomic<bool>>(true);
        bus->setAliveFlagForTest(alive);
        std::function<void(const AudioStreamRequest&)> hook;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            bus->setAudioWorkgroupDevice(m_workgroupDevices.value(request.deviceId, 0));
            bus->setOpenResult(!m_failingOutputs.contains(request.deviceId));
            m_outputRequests.push_back(request);
            m_outputAlive.push_back(alive);
            m_lastOutput = bus.get();
            hook = m_outputCreatedHook;
        }
        if (hook) {
            hook(request);
        }
        return bus;
    }

    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override
    {
        auto input = std::make_unique<FakeAudioInputStream>(request, pick, sink);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_inputRequests.push_back(InputRequest{request, pick});
        m_lastInput = input.get();
        return input;
    }

    bool hasControlPanel() const override { return m_hasControlPanel; }
    void openControlPanel(const QString& deviceId) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_controlPanelOpens.push_back(deviceId);
    }
    void rescan() override
    {
        std::function<void()> hook;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_rescansInFlight;
            hook = m_rescanHook;
        }
        m_rescans.fetch_add(1);
        if (hook) {
            hook();
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        --m_rescansInFlight;
    }
    bool opensOneStreamAtATime() const override { return m_oneStreamAtATime.load(); }

    // -- What the test sets ---------------------------------------------
    void setDevices(QList<AudioDeviceInfo> devices)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices = std::move(devices);
    }
    void addDevice(const AudioDeviceInfo& info)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices.append(info);
    }
    void removeDevice(const QString& id, AudioDeviceDirection direction)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices.removeIf([&](const AudioDeviceInfo& d) {
            return d.id == id && d.direction == direction;
        });
    }
    void setDefault(AudioDeviceDirection direction, std::optional<QString> id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        (direction == AudioDeviceDirection::Output ? m_defaultOutput : m_defaultInput) = std::move(id);
    }
    void setRunning(bool running) { m_running.store(running); }
    // Outputs made for these request device ids fail to open ("" is the
    // system default).  Applies to outputs made after the call.
    void setFailingOutputs(QStringList deviceIds)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failingOutputs = std::move(deviceIds);
    }
    void setCallbackFrames(int frames) { m_callbackFrames = frames; }
    void setFadeTimeMs(int ms) { m_fadeTimeMs = ms; }
    void setOpensOneStreamAtATime(bool one) { m_oneStreamAtATime.store(one); }
    void setTakesStereoMix(bool takes) { m_takesStereoMix = takes; }
    void setHasControlPanel(bool has) { m_hasControlPanel = has; }
    // The audioWorkgroupDevice() of every output made for deviceId ("" is
    // the default); 0 when not set.
    void setWorkgroupDevice(const QString& deviceId, std::uint32_t device)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workgroupDevices.insert(deviceId, device);
    }

    // Calls the notice sink on the calling thread, as a system notice would.
    void postNotice(AudioNotice notice)
    {
        std::function<void(AudioNotice)> sink;
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            sink = m_noticeSink;
        }
        if (sink) {
            sink(notice);
        }
    }

    // Holds every enumerate() until releaseEnumerate().
    void holdEnumerate()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_holdEnumerate = true;
    }
    void releaseEnumerate()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_holdEnumerate = false;
        m_cv.notify_all();
    }

    // -- What the test reads --------------------------------------------
    int enumerateCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_enumerateCalls;
    }
    int defaultCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_defaultCalls;
    }
    std::vector<QThread*> enumerateThreads() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_enumerateThreads;
    }
    std::vector<QThread*> defaultThreads() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_defaultThreads;
    }
    bool hasNoticeSink() const
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        return static_cast<bool>(m_noticeSink);
    }
    // Called (outside the fake's lock) for every output createOutput() makes.
    void setOutputCreatedHook(std::function<void(const AudioStreamRequest&)> hook)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outputCreatedHook = std::move(hook);
    }

    // Runs inside every rescan() (outside the fake's lock), on the
    // calling thread; it may block.
    void setRescanHook(std::function<void()> hook)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rescanHook = std::move(hook);
    }
    // setNoticeSink() calls made while an enumerate(), defaultDeviceId()
    // or rescan() was in progress on another thread.
    int sinkSetsDuringCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sinkSetsDuringCalls;
    }

    std::vector<AudioStreamRequest> outputRequests() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_outputRequests;
    }
    std::vector<InputRequest> inputRequests() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_inputRequests;
    }
    std::vector<QString> controlPanelOpens() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_controlPanelOpens;
    }
    int rescanCount() const { return m_rescans.load(); }
    // Whether the output made for outputRequests()[index] still exists.
    bool outputAlive(std::size_t index) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return index < m_outputAlive.size() && m_outputAlive[index]->load();
    }
    int aliveOutputs() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<int>(std::count_if(m_outputAlive.begin(), m_outputAlive.end(),
                                              [](const auto& alive) { return alive->load(); }));
    }
    // Outputs destroyed after a fade request, before their fade ended.
    int closedUnfaded() const { return m_closedUnfaded->load(); }
    // requestFadeOut() calls on every output this backend made.
    int fadeRequests() const { return m_fadeRequests->load(); }
    // The most enumerate() / defaultDeviceId() calls ever in progress at
    // once; a held enumerate() counts until it returns.
    int maxConcurrentCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_maxCallsInFlight;
    }
    // The last bus or input made; valid while its owner keeps it.
    FakeMatcherAudioBus* lastOutput() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastOutput;
    }
    FakeAudioInputStream* lastInput() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastInput;
    }

private:
    void enterCallLocked()
    {
        ++m_callsInFlight;
        m_maxCallsInFlight = std::max(m_maxCallsInFlight, m_callsInFlight);
    }

    const AudioBackendId m_id;
    std::atomic<bool> m_running{true};
    bool m_takesStereoMix = true;
    bool m_hasControlPanel = false;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    QList<AudioDeviceInfo> m_devices;
    std::optional<QString> m_defaultOutput;
    std::optional<QString> m_defaultInput;
    QHash<QString, std::uint32_t> m_workgroupDevices;
    bool m_holdEnumerate = false;
    int m_enumerateCalls = 0;
    int m_defaultCalls = 0;
    int m_callsInFlight = 0;
    int m_maxCallsInFlight = 0;
    std::vector<QThread*> m_enumerateThreads;
    std::vector<QThread*> m_defaultThreads;
    std::vector<AudioStreamRequest> m_outputRequests;
    std::function<void(const AudioStreamRequest&)> m_outputCreatedHook;
    std::function<void()> m_rescanHook;
    int m_rescansInFlight = 0;
    int m_sinkSetsDuringCalls = 0;
    std::vector<InputRequest> m_inputRequests;
    std::vector<QString> m_controlPanelOpens;
    FakeMatcherAudioBus* m_lastOutput = nullptr;
    FakeAudioInputStream* m_lastInput = nullptr;
    std::atomic<int> m_rescans{0};
    std::shared_ptr<std::atomic<int>> m_fadeRequests = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<int>> m_closedUnfaded = std::make_shared<std::atomic<int>>(0);
    std::vector<std::shared_ptr<std::atomic<bool>>> m_outputAlive;
    QStringList m_failingOutputs;
    int m_callbackFrames = 128;
    int m_fadeTimeMs = 0;
    std::atomic<bool> m_oneStreamAtATime{false};

    mutable std::mutex m_sinkMutex;
    std::function<void(AudioNotice)> m_noticeSink;
};

} // namespace NereusSDR
