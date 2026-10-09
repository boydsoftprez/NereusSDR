// =================================================================
// src/core/audio/CoreAudioHalProperties.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Small HAL property readers the Core
// Audio engine's files share (native audio plan Task 8); Mac only, never
// called from a device callback.  No upstream logic.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-11). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QtGlobal>

#ifdef Q_OS_MAC

#include "core/IAudioBus.h"

#include <QString>

#include <Block.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <unistd.h>

#include <cmath>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace NereusSDR::coreaudio {

inline AudioObjectPropertyAddress address(AudioObjectPropertySelector selector,
                                          AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
{
    return AudioObjectPropertyAddress{selector, scope, kAudioObjectPropertyElementMain};
}

template <typename T>
std::optional<T> read(AudioObjectID object, AudioObjectPropertySelector selector,
                      AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
{
    const AudioObjectPropertyAddress where = address(selector, scope);
    T value{};
    UInt32 size = sizeof(T);
    if (AudioObjectGetPropertyData(object, &where, 0, nullptr, &size, &value) != noErr
        || size != sizeof(T)) {
        return std::nullopt;
    }
    return value;
}

// A property that is an array of T (the device list, a device's streams).
template <typename T>
std::vector<T> readArray(AudioObjectID object, AudioObjectPropertySelector selector,
                         AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
{
    const AudioObjectPropertyAddress where = address(selector, scope);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(object, &where, 0, nullptr, &size) != noErr || size == 0) {
        return {};
    }
    std::vector<T> values(size / sizeof(T));
    if (AudioObjectGetPropertyData(object, &where, 0, nullptr, &size, values.data()) != noErr) {
        return {};
    }
    values.resize(size / sizeof(T));
    return values;
}

// The channels of every stream in kAudioDevicePropertyStreamConfiguration.
inline int channelCount(AudioObjectID device, AudioObjectPropertyScope scope)
{
    const AudioObjectPropertyAddress where = address(kAudioDevicePropertyStreamConfiguration, scope);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &where, 0, nullptr, &size) != noErr
        || size < sizeof(AudioBufferList)) {
        return 0;
    }
    std::vector<std::byte> storage(size);
    auto* list = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &where, 0, nullptr, &size, list) != noErr) {
        return 0;
    }
    int channels = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i) {
        channels += static_cast<int>(list->mBuffers[i].mNumberChannels);
    }
    return channels;
}

// kAudioDevicePropertyLatency + kAudioDevicePropertySafetyOffset + the
// first stream's kAudioStreamPropertyLatency, in frames, for one scope.
inline UInt32 latencyFrames(AudioObjectID device, AudioObjectPropertyScope scope)
{
    UInt32 frames = read<UInt32>(device, kAudioDevicePropertyLatency, scope).value_or(0)
                    + read<UInt32>(device, kAudioDevicePropertySafetyOffset, scope).value_or(0);
    const std::vector<AudioStreamID> streams =
        readArray<AudioStreamID>(device, kAudioDevicePropertyStreams, scope);
    if (!streams.empty()) {
        frames += read<UInt32>(streams.front(), kAudioStreamPropertyLatency).value_or(0);
    }
    return frames;
}

// A stream's device listeners (R-AUD-03, R-AUD-11): alive false posts
// DeviceLost, hog mode taken by another process posts DeviceBusy, and a
// nominal rate other than the stream's posts FormatChanged.  They run on
// a serial queue of the watch's own and only post to the sink.
class StreamWatch {
public:
    StreamWatch() = default;
    ~StreamWatch() { stop(); }
    StreamWatch(const StreamWatch&) = delete;
    StreamWatch& operator=(const StreamWatch&) = delete;

    void setSink(std::function<void(const AudioStreamEvent&)> sink)
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_sink = std::move(sink);
    }

    void start(AudioObjectID device, int streamRate, const char* queueLabel)
    {
        stop();
        m_device = device;
        m_rate = streamRate;
        m_queue = dispatch_queue_create(queueLabel, DISPATCH_QUEUE_SERIAL);
        StreamWatch* self = this;
        m_listener = Block_copy(^(UInt32 count, const AudioObjectPropertyAddress* addresses) {
            self->onProperties(count, addresses);
        });
        for (const AudioObjectPropertySelector selector : kWatched) {
            const AudioObjectPropertyAddress where = address(selector);
            AudioObjectAddPropertyListenerBlock(m_device, &where, m_queue, m_listener);
        }
    }

    void stop()
    {
        if (m_queue == nullptr) {
            return;
        }
        for (const AudioObjectPropertySelector selector : kWatched) {
            const AudioObjectPropertyAddress where = address(selector);
            AudioObjectRemovePropertyListenerBlock(m_device, &where, m_queue, m_listener);
        }
        // Anything queued before the removals has run.
        dispatch_sync(m_queue, ^{});
        dispatch_release(m_queue);
        m_queue = nullptr;
        Block_release(m_listener);
        m_listener = nullptr;
    }

private:
    static constexpr AudioObjectPropertySelector kWatched[] = {
        kAudioDevicePropertyDeviceIsAlive,
        kAudioDevicePropertyHogMode,
        kAudioDevicePropertyNominalSampleRate,
    };

    void post(AudioStreamEvent::Kind kind, const QString& detail)
    {
        std::function<void(const AudioStreamEvent&)> copy;
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            copy = m_sink;
        }
        if (copy) {
            copy(AudioStreamEvent{kind, detail});
        }
    }

    // On the queue.
    void onProperties(UInt32 count, const AudioObjectPropertyAddress* addresses)
    {
        for (UInt32 i = 0; i < count; ++i) {
            switch (addresses[i].mSelector) {
            case kAudioDevicePropertyDeviceIsAlive:
                if (read<UInt32>(m_device, kAudioDevicePropertyDeviceIsAlive).value_or(0) == 0) {
                    post(AudioStreamEvent::Kind::DeviceLost, QStringLiteral("The device went away"));
                }
                break;
            case kAudioDevicePropertyHogMode: {
                const pid_t hog = read<pid_t>(m_device, kAudioDevicePropertyHogMode).value_or(-1);
                if (hog != -1 && hog != getpid()) {
                    post(AudioStreamEvent::Kind::DeviceBusy,
                         QStringLiteral("Another program took the device for itself"));
                }
                break;
            }
            case kAudioDevicePropertyNominalSampleRate: {
                // Design choice 3: the supervisor reopens at the new rate.
                const Float64 rate =
                    read<Float64>(m_device, kAudioDevicePropertyNominalSampleRate).value_or(0.0);
                if (rate > 0.0 && static_cast<int>(std::lround(rate)) != m_rate) {
                    post(AudioStreamEvent::Kind::FormatChanged,
                         QStringLiteral("The device's sample rate changed"));
                }
                break;
            }
            default:
                break;
            }
        }
    }

    AudioObjectID m_device = kAudioObjectUnknown;
    int m_rate = 0;
    dispatch_queue_t m_queue = nullptr;
    AudioObjectPropertyListenerBlock m_listener = nullptr;
    std::mutex m_sinkMutex;
    std::function<void(const AudioStreamEvent&)> m_sink;
};

} // namespace NereusSDR::coreaudio

#endif // Q_OS_MAC
