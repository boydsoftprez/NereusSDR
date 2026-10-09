// =================================================================
// src/core/audio/CoreAudioSystem.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See CoreAudioSystem.h.  The
// listener pattern (block listeners on one serial dispatch queue, removed
// and drained before release) follows our own CoreAudioHalBus.cpp reader
// watch.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-07, R-AUD-11, R-AUD-14). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: Task 8 merge (R-AUD-32): in a test run the real adapter
//               lists no device, reports no default and registers no
//               listener, as the older drivers' list is empty there. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CoreAudioSystem.h"

#include "core/audio/AudioTestBarrier.h"
#include "core/audio/CoreAudioInputStream.h"
#include "core/audio/CoreAudioOutputBus.h"

#include <QtGlobal>

#ifdef Q_OS_MAC
#include "core/audio/CoreAudioHalProperties.h"

#include <Block.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <unistd.h>

#include <mutex>
#include <set>
#include <utility>
#endif

namespace NereusSDR {

namespace {

// The HAL's transport type codes, as four-character constants.
// From macOS SDK 27.0 CoreAudio.framework AudioHardwareBase.h:607-624
// (kAudioDeviceTransportType*).
constexpr std::uint32_t fourCc(char a, char b, char c, char d)
{
    return (std::uint32_t(std::uint8_t(a)) << 24) | (std::uint32_t(std::uint8_t(b)) << 16)
           | (std::uint32_t(std::uint8_t(c)) << 8) | std::uint32_t(std::uint8_t(d));
}
constexpr std::uint32_t kTransportBuiltIn = fourCc('b', 'l', 't', 'n');
constexpr std::uint32_t kTransportUsb = fourCc('u', 's', 'b', ' ');
constexpr std::uint32_t kTransportBluetooth = fourCc('b', 'l', 'u', 'e');
constexpr std::uint32_t kTransportBluetoothLe = fourCc('b', 'l', 'e', 'a');
constexpr std::uint32_t kTransportHdmi = fourCc('h', 'd', 'm', 'i');
constexpr std::uint32_t kTransportDisplayPort = fourCc('d', 'p', 'r', 't');
constexpr std::uint32_t kTransportVirtual = fourCc('v', 'i', 'r', 't');

} // namespace

AudioTransport coreAudioTransport(std::uint32_t transportType)
{
    switch (transportType) {
    case kTransportBuiltIn:
        return AudioTransport::BuiltIn;
    case kTransportUsb:
        return AudioTransport::Usb;
    case kTransportBluetooth:
    case kTransportBluetoothLe:
        return AudioTransport::Bluetooth;
    case kTransportHdmi:
    case kTransportDisplayPort:
        return AudioTransport::Hdmi;
    case kTransportVirtual:
        return AudioTransport::Virtual;
    default:
        return AudioTransport::Unknown;
    }
}

#ifdef Q_OS_MAC

// The constants above are the SDK's own.
static_assert(kTransportBuiltIn == kAudioDeviceTransportTypeBuiltIn);
static_assert(kTransportUsb == kAudioDeviceTransportTypeUSB);
static_assert(kTransportBluetooth == kAudioDeviceTransportTypeBluetooth);
static_assert(kTransportBluetoothLe == kAudioDeviceTransportTypeBluetoothLE);
static_assert(kTransportHdmi == kAudioDeviceTransportTypeHDMI);
static_assert(kTransportDisplayPort == kAudioDeviceTransportTypeDisplayPort);
static_assert(kTransportVirtual == kAudioDeviceTransportTypeVirtual);

namespace {

QString readString(AudioObjectID object, AudioObjectPropertySelector selector)
{
    const AudioObjectPropertyAddress where = coreaudio::address(selector);
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(object, &where, 0, nullptr, &size, &value) != noErr
        || value == nullptr) {
        return {};
    }
    const QString text = QString::fromCFString(value);
    CFRelease(value);
    return text;
}

class MacCoreAudioSystem final : public ICoreAudioSystem {
public:
    MacCoreAudioSystem()
    {
        m_queue = dispatch_queue_create("com.nereussdr.coreaudio.notices", DISPATCH_QUEUE_SERIAL);
        MacCoreAudioSystem* self = this;
        m_devicesListener = Block_copy(^(UInt32, const AudioObjectPropertyAddress*) {
            self->post(AudioNotice::DevicesChanged);
        });
        m_defaultOutputListener = Block_copy(^(UInt32, const AudioObjectPropertyAddress*) {
            self->post(AudioNotice::DefaultOutputChanged);
        });
        m_defaultInputListener = Block_copy(^(UInt32, const AudioObjectPropertyAddress*) {
            self->post(AudioNotice::DefaultInputChanged);
        });
        // A device's alive or hog-mode state changing changes its entry.
        m_deviceStateListener = Block_copy(^(UInt32, const AudioObjectPropertyAddress*) {
            self->post(AudioNotice::DevicesChanged);
        });
    }

    ~MacCoreAudioSystem() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_systemListening) {
                const AudioObjectPropertyAddress devices = coreaudio::address(kAudioHardwarePropertyDevices);
                const AudioObjectPropertyAddress output =
                    coreaudio::address(kAudioHardwarePropertyDefaultOutputDevice);
                const AudioObjectPropertyAddress input =
                    coreaudio::address(kAudioHardwarePropertyDefaultInputDevice);
                AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &devices, m_queue,
                                                       m_devicesListener);
                AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &output, m_queue,
                                                       m_defaultOutputListener);
                AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &input, m_queue,
                                                       m_defaultInputListener);
                m_systemListening = false;
            }
            for (const AudioObjectID device : m_watched) {
                unwatchLocked(device);
            }
            m_watched.clear();
        }
        // Anything queued before the removals has run.
        dispatch_sync(m_queue, ^{});
        dispatch_release(m_queue);
        Block_release(m_devicesListener);
        Block_release(m_defaultOutputListener);
        Block_release(m_defaultInputListener);
        Block_release(m_deviceStateListener);
    }

    QList<CoreAudioDeviceRecord> devices() override
    {
        QList<CoreAudioDeviceRecord> records;
        // R-AUD-32: a test run walks no real device.
        if (audioDevicesBarredForTestRun()) {
            return records;
        }
        const std::vector<AudioObjectID> ids =
            coreaudio::readArray<AudioObjectID>(kAudioObjectSystemObject, kAudioHardwarePropertyDevices);
        std::set<AudioObjectID> present;
        for (const AudioObjectID id : ids) {
            CoreAudioDeviceRecord record;
            record.objectId = id;
            record.uid = readString(id, kAudioDevicePropertyDeviceUID);
            if (record.uid.isEmpty()) {
                continue;
            }
            record.name = readString(id, kAudioObjectPropertyName);
            record.transportType =
                coreaudio::read<UInt32>(id, kAudioDevicePropertyTransportType).value_or(0);
            record.outputChannels = coreaudio::channelCount(id, kAudioObjectPropertyScopeOutput);
            record.inputChannels = coreaudio::channelCount(id, kAudioObjectPropertyScopeInput);
            record.alive = coreaudio::read<UInt32>(id, kAudioDevicePropertyDeviceIsAlive).value_or(1) != 0;
            record.hogPid = coreaudio::read<pid_t>(id, kAudioDevicePropertyHogMode).value_or(-1);
            present.insert(id);
            records.append(record);
        }
        // Per-device listeners follow the list: added for a new device,
        // removed for one that has gone.
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_watched.begin(); it != m_watched.end();) {
            if (present.count(*it) == 0) {
                unwatchLocked(*it);
                it = m_watched.erase(it);
            } else {
                ++it;
            }
        }
        for (const AudioObjectID id : present) {
            if (m_watched.insert(id).second) {
                const AudioObjectPropertyAddress alive = coreaudio::address(kAudioDevicePropertyDeviceIsAlive);
                const AudioObjectPropertyAddress hog = coreaudio::address(kAudioDevicePropertyHogMode);
                AudioObjectAddPropertyListenerBlock(id, &alive, m_queue, m_deviceStateListener);
                AudioObjectAddPropertyListenerBlock(id, &hog, m_queue, m_deviceStateListener);
            }
        }
        return records;
    }

    std::optional<std::uint32_t> defaultDevice(AudioDeviceDirection direction) override
    {
        if (audioDevicesBarredForTestRun()) {
            return std::nullopt;
        }
        const AudioObjectPropertySelector selector = direction == AudioDeviceDirection::Output
                                                         ? kAudioHardwarePropertyDefaultOutputDevice
                                                         : kAudioHardwarePropertyDefaultInputDevice;
        const std::optional<AudioObjectID> id = coreaudio::read<AudioObjectID>(kAudioObjectSystemObject, selector);
        if (!id || *id == kAudioObjectUnknown) {
            return std::nullopt;
        }
        return *id;
    }

    void setNoticeSink(std::function<void(AudioNotice)> sink) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        {
            std::lock_guard<std::mutex> sinkLock(m_sinkMutex);
            m_sink = std::move(sink);
        }
        if (m_systemListening || audioDevicesBarredForTestRun()) {
            return;
        }
        const AudioObjectPropertyAddress devices = coreaudio::address(kAudioHardwarePropertyDevices);
        const AudioObjectPropertyAddress output = coreaudio::address(kAudioHardwarePropertyDefaultOutputDevice);
        const AudioObjectPropertyAddress input = coreaudio::address(kAudioHardwarePropertyDefaultInputDevice);
        AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &devices, m_queue, m_devicesListener);
        AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &output, m_queue,
                                            m_defaultOutputListener);
        AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &input, m_queue, m_defaultInputListener);
        m_systemListening = true;
    }

    std::int32_t ownPid() const override { return static_cast<std::int32_t>(getpid()); }

    std::unique_ptr<IAudioBus> createOutput(const CoreAudioDeviceRecord& record,
                                            const AudioStreamRequest& request) override
    {
        return std::make_unique<CoreAudioOutputBus>(record.objectId, record.outputChannels, request);
    }

    std::unique_ptr<IAudioInputStream> createInput(const CoreAudioDeviceRecord& record,
                                                   const AudioStreamRequest& request,
                                                   MicChannelPick pick, IAudioInputSink* sink) override
    {
        return std::make_unique<CoreAudioInputStream>(record.objectId, record.inputChannels, request,
                                                      pick, sink);
    }

private:
    // On the queue: only posts.
    void post(AudioNotice notice)
    {
        std::function<void(AudioNotice)> copy;
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            copy = m_sink;
        }
        if (copy) {
            copy(notice);
        }
    }

    void unwatchLocked(AudioObjectID device)
    {
        const AudioObjectPropertyAddress alive = coreaudio::address(kAudioDevicePropertyDeviceIsAlive);
        const AudioObjectPropertyAddress hog = coreaudio::address(kAudioDevicePropertyHogMode);
        AudioObjectRemovePropertyListenerBlock(device, &alive, m_queue, m_deviceStateListener);
        AudioObjectRemovePropertyListenerBlock(device, &hog, m_queue, m_deviceStateListener);
    }

    dispatch_queue_t m_queue = nullptr;
    AudioObjectPropertyListenerBlock m_devicesListener = nullptr;
    AudioObjectPropertyListenerBlock m_defaultOutputListener = nullptr;
    AudioObjectPropertyListenerBlock m_defaultInputListener = nullptr;
    AudioObjectPropertyListenerBlock m_deviceStateListener = nullptr;

    std::mutex m_mutex;                 // listener registration and m_watched
    bool m_systemListening = false;
    std::set<AudioObjectID> m_watched;

    std::mutex m_sinkMutex;             // never held while m_mutex is wanted
    std::function<void(AudioNotice)> m_sink;
};

} // namespace

std::unique_ptr<ICoreAudioSystem> makeCoreAudioSystem()
{
    return std::make_unique<MacCoreAudioSystem>();
}

#endif // Q_OS_MAC

} // namespace NereusSDR
