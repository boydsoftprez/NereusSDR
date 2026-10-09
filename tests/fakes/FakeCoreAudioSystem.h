// =================================================================
// tests/fakes/FakeCoreAudioSystem.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test fake for the Core Audio engine
// (R-AUD-01, R-AUD-03, R-AUD-11, R-AUD-14, R-AUD-32).  No device.
//
// An ICoreAudioSystem whose device records, defaults and own process id
// the test sets.  postNotice() calls the notice sink as a HAL listener
// would; createOutput() and createInput() record the device and request
// they are given and make a FakeMatcherAudioBus or FakeAudioInputStream.
// Every setter may be called from any thread.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-03, R-AUD-11,
//               R-AUD-14). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/audio/CoreAudioSystem.h"
#include "FakeAudioEngineBackend.h"
#include "FakeMatcherAudioBus.h"

#include <QList>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace NereusSDR {

class FakeCoreAudioSystem final : public ICoreAudioSystem {
public:
    struct OpenRequest {
        CoreAudioDeviceRecord record;
        AudioStreamRequest request;
        MicChannelPick pick = MicChannelPick::Left;
    };

    QList<CoreAudioDeviceRecord> devices() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_deviceCalls;
        return m_records;
    }

    std::optional<std::uint32_t> defaultDevice(AudioDeviceDirection direction) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return direction == AudioDeviceDirection::Output ? m_defaultOutput : m_defaultInput;
    }

    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sink = std::move(sink);
    }

    std::int32_t ownPid() const override { return m_ownPid; }

    std::unique_ptr<IAudioBus> createOutput(const CoreAudioDeviceRecord& record,
                                            const AudioStreamRequest& request) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outputs.push_back(OpenRequest{record, request, MicChannelPick::Left});
        return std::make_unique<FakeMatcherAudioBus>(request);
    }

    std::unique_ptr<IAudioInputStream> createInput(const CoreAudioDeviceRecord& record,
                                                   const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_inputs.push_back(OpenRequest{record, request, pick});
        return std::make_unique<FakeAudioInputStream>(request, pick, sink);
    }

    // -- What the test sets ---------------------------------------------
    void setRecords(QList<CoreAudioDeviceRecord> records)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_records = std::move(records);
    }
    void setDefault(AudioDeviceDirection direction, std::optional<std::uint32_t> objectId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        (direction == AudioDeviceDirection::Output ? m_defaultOutput : m_defaultInput) = objectId;
    }
    void setOwnPid(std::int32_t pid) { m_ownPid = pid; }

    // Calls the sink on the calling thread, as a HAL listener would.
    void postNotice(AudioNotice notice)
    {
        std::function<void(AudioNotice)> sink;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            sink = m_sink;
        }
        if (sink) {
            sink(notice);
        }
    }

    // -- What the test reads --------------------------------------------
    bool hasNoticeSink() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<bool>(m_sink);
    }
    int deviceCalls() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_deviceCalls;
    }
    std::vector<OpenRequest> outputs() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_outputs;
    }
    std::vector<OpenRequest> inputs() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_inputs;
    }

private:
    mutable std::mutex m_mutex;
    QList<CoreAudioDeviceRecord> m_records;
    std::optional<std::uint32_t> m_defaultOutput;
    std::optional<std::uint32_t> m_defaultInput;
    std::int32_t m_ownPid = 4242;
    std::function<void(AudioNotice)> m_sink;
    int m_deviceCalls = 0;
    std::vector<OpenRequest> m_outputs;
    std::vector<OpenRequest> m_inputs;
};

} // namespace NereusSDR
