// =================================================================
// tests/fakes/FakeMatcherAudioBus.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test fake for the native audio
// engines (R-AUD-03, R-AUD-15, R-AUD-32).  No device.
//
// An output bus as a native engine makes one.  With takesStereoMix it
// holds a real DeviceRateMatcher: push() writes 48 kHz stereo float into
// it and pumpForTest(frames) reads it as the device callback would.
// Without it, push() keeps the bytes it is given.  emitEventForTest()
// calls the stream event sink as a backend's device thread would.
//
// The FakeAudioBus that 37 tests use is a different fake and stays as it
// is.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03, R-AUD-32). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 6 (R-AUD-15): flush() asks the
//               matcher to drop what is queued; outputPacing() as a
//               PortAudio output reports it. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 (R-AUD-06): requestFadeOut()
//               is counted (fadeRequestCount(), and a shared counter the
//               backend reads after the bus is gone); fadedOut() is true.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 8 (R-AUD-18): a settable
//               audioWorkgroupDevice(). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: early-review fix wave (R-AUD-06, R-AUD-08): a fade can
//               take setFadeTimeForTest() ms; the bus marks a shared flag
//               when it is destroyed (alive) and counts a close before its
//               fade ended (closedUnfaded).  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-10: setClockMatchWritePacket() reaches the matcher, as a
//               native engine's bus, and writeClockForTest times the
//               writes for a test on its own clock (R-AUD-15, bench
//               regression). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioDelayParts.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace NereusSDR {

class FakeMatcherAudioBus final : public IAudioBus {
public:
    static constexpr int kInRate = 48000;

    explicit FakeMatcherAudioBus(AudioStreamRequest request, bool takesStereoMix = true,
                                 int callbackFrames = 128)
        : m_request(std::move(request))
        , m_takesStereoMix(takesStereoMix)
        , m_callbackFrames(callbackFrames)
    {
    }

    ~FakeMatcherAudioBus() override
    {
        if (m_fadeRequests > 0 && !fadedOut() && m_closedUnfaded) {
            m_closedUnfaded->fetch_add(1);
        }
        if (m_alive) {
            m_alive->store(false);
        }
    }

    bool open(const AudioFormat& format) override
    {
        if (!m_openResult) {
            m_error = QStringLiteral("Fake open failure");
            return false;
        }
        m_format = format;
        if (m_takesStereoMix) {
            DeviceRateMatcher::Config config;
            config.inRate = kInRate;
            config.outRate = m_request.sampleRate;
            config.callbackFrames = m_callbackFrames;
            config.delayMs = m_request.delayMs;
            m_matcher = std::make_unique<DeviceRateMatcher>(config);
            if (!m_matcher->valid()) {
                m_matcher.reset();
                m_error = QStringLiteral("Fake matcher configuration cannot run");
                return false;
            }
            m_reader = m_matcher->makeReader();
            m_readBuffer.assign(std::size_t(2 * m_callbackFrames), 0.0f);
        }
        m_open.store(true);
        return true;
    }

    void close() override
    {
        m_open.store(false);
        m_reader = MatcherReader();
        m_matcher.reset();
    }

    bool isOpen() const override { return m_open.load(); }

    qint64 push(const char* data, qint64 bytes) override
    {
        if (!m_open.load() || data == nullptr || bytes <= 0) {
            return 0;
        }
        ++m_pushes;
        if (m_matcher) {
            const int frames = int(bytes / qint64(2 * sizeof(float)));
            m_matcher->write(reinterpret_cast<const float*>(data), frames,
                             writeClockForTest ? writeClockForTest() : audioProbeNowNs());
            return bytes;
        }
        m_bytes.append(data, int(bytes));
        return bytes;
    }

    // The time of each write, for a test that runs on its own clock; unset,
    // the wall clock as a native engine's bus.  Set before the first push.
    std::function<std::int64_t()> writeClockForTest;

    qint64 pull(char*, qint64) override { return 0; }

    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("FakeMatcher"); }
    AudioFormat negotiatedFormat() const override { return m_format; }
    QString errorString() const override { return m_error; }

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_sink = std::move(sink);
    }

    AudioDelayParts delayParts() const override
    {
        if (!m_matcher) {
            return {};
        }
        const double bufferMs = 1000.0 * double(m_callbackFrames) / double(m_request.sampleRate);
        return m_matcher->delayParts(bufferMs, 0.0);
    }

    bool takesStereoMix() const override { return m_takesStereoMix; }

    std::uint32_t audioWorkgroupDevice() const override { return m_workgroupDevice; }
    void setAudioWorkgroupDevice(std::uint32_t device) { m_workgroupDevice = device; }

    std::optional<DeviceRateMatcherStats> matcherStats() const override
    {
        if (!m_matcher) {
            return std::nullopt;
        }
        return m_matcher->stats();
    }

    // As PortAudioBus with a matcher: every frame the device asked for,
    // the matcher's fill and its automatic size (Task 6).
    std::optional<OutputPacing> outputPacing() const override
    {
        if (!m_matcher) {
            return std::nullopt;
        }
        OutputPacing pacing;
        pacing.consumedFrames = quint64(m_pumpedFrames.load());
        pacing.queuedFrames = std::max(0, int(m_matcher->fillFrames() + 0.5));
        pacing.capacityFrames = m_matcher->stats().rsizeFrames;
        pacing.callbackFrames = m_callbackFrames;
        return pacing;
    }

    // As PortAudioBus: a flush asks the matcher to drop what is queued at
    // its writer's next write (Task 6, the master mute's flush).
    void flush() override
    {
        if (m_matcher) {
            m_matcher->requestFlush();
        }
    }

    // Task 7: the Rescan fade.  The fake fades at once, or after
    // setFadeTimeForTest() ms.
    void requestFadeOut() override
    {
        if (m_fadeRequests == 0) {
            m_fadeRequestedAt = std::chrono::steady_clock::now();
        }
        ++m_fadeRequests;
        if (m_fadeCounter) {
            m_fadeCounter->fetch_add(1);
        }
    }
    bool fadedOut() const override
    {
        return m_fadeTimeMs <= 0 || m_fadeRequests == 0
               || std::chrono::steady_clock::now() - m_fadeRequestedAt
                      >= std::chrono::milliseconds(m_fadeTimeMs);
    }
    void setFadeTimeForTest(int ms) { m_fadeTimeMs = ms; }
    // Set false when the bus is destroyed.
    void setAliveFlagForTest(std::shared_ptr<std::atomic<bool>> alive)
    {
        m_alive = std::move(alive);
        m_alive->store(true);
    }
    // Counts a bus destroyed after a fade request, before its fade ended.
    void setClosedUnfadedCounterForTest(std::shared_ptr<std::atomic<int>> counter)
    {
        m_closedUnfaded = std::move(counter);
    }
    void setFadeCounterForTest(std::shared_ptr<std::atomic<int>> counter)
    {
        m_fadeCounter = std::move(counter);
    }
    int fadeRequestCount() const { return m_fadeRequests; }

    void restartClockMatch() override
    {
        ++m_restarts;
        if (m_matcher) {
            m_matcher->requestRestart();
        }
    }

    void setClockMatchWritePacket(int frames, bool waited) override
    {
        if (m_matcher) {
            m_matcher->setWritePacketFrames(frames, waited);
        }
    }

    // Reads frames from the matcher as the device callback would, at most
    // the callback size per read.  Returns the frames read (zero when the
    // bus is closed or holds no matcher).  The last read stays in
    // lastRead().
    int pumpForTest(int frames)
    {
        if (!m_open.load() || !m_reader.valid() || frames <= 0) {
            return 0;
        }
        int done = 0;
        while (done < frames) {
            const int n = std::min(frames - done, m_callbackFrames);
            m_reader.read(m_readBuffer.data(), n);
            m_lastReadFrames = n;
            done += n;
        }
        m_pumpedFrames.fetch_add(done);
        return done;
    }

    void emitEventForTest(const AudioStreamEvent& event)
    {
        std::function<void(const AudioStreamEvent&)> sink;
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            sink = m_sink;
        }
        if (sink) {
            sink(event);
        }
    }

    // Test inspectors.
    const AudioStreamRequest& request() const { return m_request; }
    DeviceRateMatcher* matcher() { return m_matcher.get(); }
    const float* lastRead() const { return m_readBuffer.data(); }
    int lastReadFrames() const { return m_lastReadFrames; }
    std::int64_t pumpedFrames() const { return m_pumpedFrames.load(); }
    int pushCount() const { return m_pushes; }
    int restartCount() const { return m_restarts; }
    const QByteArray& bytes() const { return m_bytes; }
    void setOpenResult(bool ok) { m_openResult = ok; }

private:
    AudioStreamRequest m_request;
    bool m_takesStereoMix;
    std::uint32_t m_workgroupDevice = 0;
    int m_callbackFrames;
    bool m_openResult = true;
    std::atomic<bool> m_open{false};
    AudioFormat m_format;
    QString m_error;
    std::unique_ptr<DeviceRateMatcher> m_matcher;
    MatcherReader m_reader;
    std::vector<float> m_readBuffer;
    int m_lastReadFrames = 0;
    std::atomic<std::int64_t> m_pumpedFrames{0};
    int m_pushes = 0;
    int m_restarts = 0;
    int m_fadeRequests = 0;
    int m_fadeTimeMs = 0;
    std::chrono::steady_clock::time_point m_fadeRequestedAt{};
    std::shared_ptr<std::atomic<int>> m_fadeCounter;
    std::shared_ptr<std::atomic<bool>> m_alive;
    std::shared_ptr<std::atomic<int>> m_closedUnfaded;
    QByteArray m_bytes;
    std::mutex m_sinkMutex;
    std::function<void(const AudioStreamEvent&)> m_sink;
};

} // namespace NereusSDR
