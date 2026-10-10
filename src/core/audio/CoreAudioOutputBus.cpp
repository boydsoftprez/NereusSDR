// =================================================================
// src/core/audio/CoreAudioOutputBus.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See CoreAudioOutputBus.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-07, R-AUD-11, R-AUD-15,
//               R-AUD-18). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-10: setClockMatchWritePacket(): a writer of whole packets
//               (remote playback) tells the bus's clock matcher its packet
//               (R-AUD-15, bench regression). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CoreAudioOutputBus.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/CoreAudioHalProperties.h"

#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <utility>

#ifdef Q_OS_MAC
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#endif

namespace NereusSDR {

int coreAudioBufferFrames(int requestedFrames, double rangeMinimum, double rangeMaximum)
{
    int frames = requestedFrames > 0 ? requestedFrames : kCoreAudioDefaultBufferFrames;
    if (rangeMaximum >= rangeMinimum && rangeMaximum > 0.0) {
        const int low = static_cast<int>(std::ceil(rangeMinimum));
        const int high = static_cast<int>(std::floor(rangeMaximum));
        frames = std::clamp(frames, std::max(1, low), std::max(1, high));
    }
    return frames;
}

std::vector<std::int32_t> coreAudioOutputChannelMap(int deviceChannels, AudioChannelPair pair)
{
    const int first = pair.firstChannel - 1;
    const int count = pair.channelCount == 1 ? 1 : 2;
    if (deviceChannels < 1 || first < 0 || first + count > deviceChannels) {
        return {};
    }
    std::vector<std::int32_t> map(static_cast<std::size_t>(deviceChannels), -1);
    map[static_cast<std::size_t>(first)] = 0;
    if (count == 2) {
        map[static_cast<std::size_t>(first + 1)] = 1;
    }
    return map;
}

std::vector<std::int32_t> coreAudioInputChannelMap(int deviceChannels, AudioChannelPair pair)
{
    const int first = pair.firstChannel - 1;
    const int count = pair.channelCount == 1 ? 1 : 2;
    if (deviceChannels < 1 || first < 0 || first + count > deviceChannels) {
        return {};
    }
    return {first, count == 2 ? first + 1 : first};
}

DeviceRateMatcher::Config coreAudioMatcherConfig(int deviceRate, int bufferFrames, int delayMs)
{
    DeviceRateMatcher::Config config;
    config.inRate = 48000;
    config.outRate = deviceRate;
    config.writeBlockFrames = 64;
    config.callbackFrames = bufferFrames;
    config.delayMs = delayMs;
    return config;
}

std::int64_t coreAudioFramesToNs(std::int64_t frames, double sampleRate)
{
    if (sampleRate <= 0.0 || frames <= 0) {
        return 0;
    }
    return static_cast<std::int64_t>(std::llround(1e9 * static_cast<double>(frames) / sampleRate));
}

QString coreAudioTestRunError()
{
    return QStringLiteral("Audio devices are not opened in a test run");
}

#ifdef Q_OS_MAC

struct CoreAudioOutputBus::Impl {
    AudioDeviceID device = kAudioObjectUnknown;
    int deviceChannels = 0;
    AudioStreamRequest request;

    AudioComponentInstance unit = nullptr;
    std::unique_ptr<DeviceRateMatcher> matcher;
    MatcherReader reader;
    bool foldToOne = false;          // a one-channel pair
    int deviceRate = 0;
    int bufferFrames = 0;
    std::int64_t latencyNs = 0;
    AudioFormat format;
    QString error;
    std::atomic<bool> open{false};

    coreaudio::StreamWatch watch;

    // The render callback: the matcher's reader fills the client buffer.
    // No lock, no allocation, no system call.
    static OSStatus render(void* refCon, AudioUnitRenderActionFlags* /*flags*/,
                           const AudioTimeStamp* /*timeStamp*/, UInt32 /*bus*/,
                           UInt32 frames, AudioBufferList* io)
    {
        auto* self = static_cast<Impl*>(refCon);
        if (io == nullptr || io->mNumberBuffers < 1 || io->mBuffers[0].mData == nullptr) {
            return noErr;
        }
        auto* out = static_cast<float*>(io->mBuffers[0].mData);
        self->reader.read(out, static_cast<int>(frames));
        if (self->foldToOne) {
            for (UInt32 i = 0; i < frames; ++i) {
                const float mono = 0.5f * (out[2 * i] + out[2 * i + 1]);
                out[2 * i] = mono;
                out[2 * i + 1] = mono;
            }
        }
        return noErr;
    }

    void disposeUnit()
    {
        if (unit != nullptr) {
            AudioOutputUnitStop(unit);
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
            unit = nullptr;
        }
    }

    bool fail(const QString& what, OSStatus status)
    {
        error = status == noErr ? what : QStringLiteral("%1 (error %2)").arg(what).arg(status);
        watch.stop();
        disposeUnit();
        reader = MatcherReader();
        matcher.reset();
        return false;
    }

    bool openUnit()
    {
        const Float64 nominal =
            coreaudio::read<Float64>(device, kAudioDevicePropertyNominalSampleRate).value_or(0.0);
        if (nominal <= 0.0) {
            return fail(QStringLiteral("The device did not report its sample rate"), noErr);
        }
        deviceRate = static_cast<int>(std::lround(nominal));

        const std::vector<std::int32_t> map = coreAudioOutputChannelMap(deviceChannels, request.pair);
        if (map.empty()) {
            return fail(QStringLiteral("The chosen channels are not on this device"), noErr);
        }
        foldToOne = request.pair.channelCount == 1;

        AudioComponentDescription description{};
        description.componentType = kAudioUnitType_Output;
        description.componentSubType = kAudioUnitSubType_HALOutput;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        AudioComponent component = AudioComponentFindNext(nullptr, &description);
        if (component == nullptr) {
            return fail(QStringLiteral("The system audio output unit is missing"), noErr);
        }
        OSStatus status = AudioComponentInstanceNew(component, &unit);
        if (status != noErr) {
            unit = nullptr;
            return fail(QStringLiteral("The audio output unit did not start"), status);
        }

        // Output on (element 0), input off (element 1).
        UInt32 enable = 1;
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                                      0, &enable, sizeof(enable));
        if (status != noErr) {
            return fail(QStringLiteral("The device's output could not be enabled"), status);
        }
        UInt32 disable = 0;
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input,
                                      1, &disable, sizeof(disable));
        if (status != noErr) {
            return fail(QStringLiteral("The device's input could not be disabled"), status);
        }
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice,
                                      kAudioUnitScope_Global, 0, &device, sizeof(device));
        if (status != noErr) {
            return fail(QStringLiteral("The device could not be chosen"), status);
        }

        // Frames per callback: the request's or 128, inside the device's range.
        const AudioValueRange range =
            coreaudio::read<AudioValueRange>(device, kAudioDevicePropertyBufferFrameSizeRange)
                .value_or(AudioValueRange{0.0, 0.0});
        UInt32 frames = static_cast<UInt32>(
            coreAudioBufferFrames(request.bufferFrames, range.mMinimum, range.mMaximum));
        const AudioObjectPropertyAddress frameSize = coreaudio::address(kAudioDevicePropertyBufferFrameSize);
        AudioObjectSetPropertyData(device, &frameSize, 0, nullptr, sizeof(frames), &frames);
        bufferFrames = static_cast<int>(
            coreaudio::read<UInt32>(device, kAudioDevicePropertyBufferFrameSize).value_or(frames));

        AudioStreamBasicDescription client{};
        client.mSampleRate = nominal;
        client.mFormatID = kAudioFormatLinearPCM;
        client.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
        client.mFramesPerPacket = 1;
        client.mChannelsPerFrame = 2;
        client.mBitsPerChannel = 32;
        client.mBytesPerFrame = 2 * sizeof(float);
        client.mBytesPerPacket = 2 * sizeof(float);
        status = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                      0, &client, sizeof(client));
        if (status != noErr) {
            return fail(QStringLiteral("The device did not take the stereo float format"), status);
        }
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output,
                                      0, map.data(), static_cast<UInt32>(map.size() * sizeof(SInt32)));
        if (status != noErr) {
            return fail(QStringLiteral("The chosen channels could not be set"), status);
        }

        // The matcher, and everything the callback reads, before it can run.
        matcher = std::make_unique<DeviceRateMatcher>(
            coreAudioMatcherConfig(deviceRate, bufferFrames, std::max(0, request.delayMs)));
        if (!matcher->valid()) {
            return fail(QStringLiteral("Clock matcher cannot run at %1 Hz").arg(deviceRate), noErr);
        }
        reader = matcher->makeReader();
        latencyNs = coreAudioFramesToNs(coreaudio::latencyFrames(device, kAudioObjectPropertyScopeOutput),
                                        nominal);

        AURenderCallbackStruct callback{};
        callback.inputProc = &Impl::render;
        callback.inputProcRefCon = this;
        status = AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                      0, &callback, sizeof(callback));
        if (status != noErr) {
            return fail(QStringLiteral("The output callback could not be set"), status);
        }
        status = AudioUnitInitialize(unit);
        if (status != noErr) {
            return fail(QStringLiteral("The audio output unit did not initialize"), status);
        }
        watch.start(device, deviceRate, "com.nereussdr.coreaudio.output");
        status = AudioOutputUnitStart(unit);
        if (status != noErr) {
            return fail(QStringLiteral("The device did not start playing"), status);
        }
        return true;
    }
};

CoreAudioOutputBus::CoreAudioOutputBus(std::uint32_t deviceObjectId, int deviceOutputChannels,
                                       AudioStreamRequest request)
    : m_deviceObjectId(deviceObjectId)
    , d(std::make_unique<Impl>())
{
    d->device = deviceObjectId;
    d->deviceChannels = deviceOutputChannels;
    d->request = std::move(request);
}

CoreAudioOutputBus::~CoreAudioOutputBus()
{
    close();
}

bool CoreAudioOutputBus::open(const AudioFormat& format)
{
    if (d->open.load()) {
        return true;
    }
    if (audioDevicesBarredForTestRun()) {
        d->error = coreAudioTestRunError();
        return false;
    }
    d->error.clear();
    if (!d->openUnit()) {
        qCWarning(lcAudio) << "Core Audio output did not open:" << d->error;
        return false;
    }
    // As PortAudioBus: the requested format is reported upstream; push()
    // takes the 48 kHz stereo mix whatever the device runs at.
    d->format = format;
    d->open.store(true);
    return true;
}

void CoreAudioOutputBus::close()
{
    if (!d) {
        return;
    }
    d->open.store(false);
    d->watch.stop();
    d->disposeUnit();   // AudioOutputUnitStop waits for the render callback
    d->reader = MatcherReader();
    d->matcher.reset();
}

bool CoreAudioOutputBus::isOpen() const
{
    return d->open.load();
}

qint64 CoreAudioOutputBus::push(const char* data, qint64 bytes)
{
    if (!d->open.load() || !d->matcher || data == nullptr || bytes <= 0) {
        return 0;
    }
    const int frames = static_cast<int>(bytes / qint64(2 * sizeof(float)));
    d->matcher->write(reinterpret_cast<const float*>(data), frames, audioProbeNowNs());
    return bytes;
}

qint64 CoreAudioOutputBus::pull(char* /*data*/, qint64 /*maxBytes*/)
{
    return 0;
}

void CoreAudioOutputBus::flush()
{
    if (d->matcher) {
        d->matcher->requestFlush();
    }
}

std::optional<IAudioBus::OutputPacing> CoreAudioOutputBus::outputPacing() const
{
    if (!d->open.load() || !d->matcher) {
        return std::nullopt;
    }
    // As PortAudioBus (Task 6): every frame the device asked for, the
    // matcher's fill and its automatic size, at the device rate.
    const MatcherRingHeader* ring = d->matcher->ring();
    OutputPacing pacing;
    pacing.consumedFrames = ring->requested.load(std::memory_order_acquire);
    pacing.queuedFrames = std::max(0, static_cast<int>(std::lround(d->matcher->fillFrames())));
    pacing.capacityFrames = static_cast<int>(ring->rsizeFrames.load(std::memory_order_acquire));
    pacing.callbackFrames = d->bufferFrames;
    if (d->latencyNs > 0) {
        pacing.deviceLatencyNs = d->latencyNs;
    }
    return pacing;
}

QString CoreAudioOutputBus::backendName() const
{
    return QStringLiteral("Core Audio");
}

AudioFormat CoreAudioOutputBus::negotiatedFormat() const
{
    return d->format;
}

QString CoreAudioOutputBus::errorString() const
{
    return d->error;
}

void CoreAudioOutputBus::setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink)
{
    d->watch.setSink(std::move(sink));
}

AudioDelayParts CoreAudioOutputBus::delayParts() const
{
    if (!d->matcher || d->deviceRate <= 0) {
        return {};
    }
    const double bufferMs = 1000.0 * static_cast<double>(d->bufferFrames)
                            / static_cast<double>(d->deviceRate);
    return d->matcher->delayParts(bufferMs, static_cast<double>(d->latencyNs) / 1e6);
}

std::optional<DeviceRateMatcherStats> CoreAudioOutputBus::matcherStats() const
{
    if (!d->matcher) {
        return std::nullopt;
    }
    return d->matcher->stats();
}

void CoreAudioOutputBus::restartClockMatch()
{
    if (d->matcher) {
        d->matcher->requestRestart();
    }
}

void CoreAudioOutputBus::setClockMatchWritePacket(int frames, bool waited)
{
    if (d->matcher) {
        d->matcher->setWritePacketFrames(frames, waited);
    }
}

void CoreAudioOutputBus::requestFadeOut()
{
    if (d->reader.valid()) {
        d->reader.requestFadeOut();
    }
}

bool CoreAudioOutputBus::fadedOut() const
{
    if (!d->open.load() || !d->reader.valid()) {
        return true;
    }
    return d->reader.fadedOut();
}

int CoreAudioOutputBus::deviceRate() const
{
    return d->deviceRate;
}

int CoreAudioOutputBus::bufferFrames() const
{
    return d->bufferFrames;
}

#endif // Q_OS_MAC

} // namespace NereusSDR
