// =================================================================
// src/core/audio/CoreAudioInputStream.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See CoreAudioInputStream.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-07, R-AUD-11, R-AUD-14).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CoreAudioInputStream.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/CoreAudioHalProperties.h"
#include "core/audio/CoreAudioOutputBus.h"
#include "core/audio/DeviceSampleFormat.h"

#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>
#include <vector>

#ifdef Q_OS_MAC
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreAudio/HostTime.h>
#endif

namespace NereusSDR {

#ifdef Q_OS_MAC

std::int64_t coreAudioHostClockOffsetNs()
{
    return audioProbeNowNs()
           - static_cast<std::int64_t>(AudioConvertHostTimeToNanos(AudioGetCurrentHostTime()));
}

std::int64_t coreAudioHostTimeToProbeNs(std::uint64_t hostTime, std::int64_t offsetNs)
{
    return static_cast<std::int64_t>(AudioConvertHostTimeToNanos(hostTime)) + offsetNs;
}

struct CoreAudioInputStream::Impl {
    AudioDeviceID device = kAudioObjectUnknown;
    int deviceChannels = 0;
    AudioStreamRequest request;
    MicChannelPick pick = MicChannelPick::Left;
    IAudioInputSink* sink = nullptr;

    AudioComponentInstance unit = nullptr;
    int deviceRate = 0;
    std::int64_t latencyNs = 0;
    std::int64_t clockOffsetNs = 0;   // audioProbeNowNs() minus host time, at open
    AudioChannelPair clientPair{1, 2};
    std::vector<float> rendered;      // the pair as the unit renders it, allocated at open
    std::vector<float> stereo;        // after the mic pick
    UInt32 capacityFrames = 0;
    AudioBufferList bufferList{};
    QString error;
    std::atomic<bool> open{false};
    coreaudio::StreamWatch watch;

    // The input callback: render, pick, hand over.  No lock, no
    // allocation, no system call other than the unit's own render.
    static OSStatus input(void* refCon, AudioUnitRenderActionFlags* flags,
                          const AudioTimeStamp* timeStamp, UInt32 busNumber,
                          UInt32 frames, AudioBufferList* /*io*/)
    {
        auto* self = static_cast<Impl*>(refCon);
        const UInt32 n = std::min(frames, self->capacityFrames);
        if (n == 0 || self->sink == nullptr) {
            return noErr;
        }
        self->bufferList.mNumberBuffers = 1;
        self->bufferList.mBuffers[0].mNumberChannels = 2;
        self->bufferList.mBuffers[0].mDataByteSize = n * 2 * sizeof(float);
        self->bufferList.mBuffers[0].mData = self->rendered.data();
        const OSStatus status = AudioUnitRender(self->unit, flags, timeStamp, busNumber, n,
                                                &self->bufferList);
        if (status != noErr) {
            return status;
        }
        readDeviceToStereo(self->rendered.data(), nullptr, true, DeviceSampleFormat::Float32, 2,
                           self->clientPair, self->pick, static_cast<int>(n), self->stereo.data());
        const UInt64 hostTime = (timeStamp != nullptr && (timeStamp->mFlags & kAudioTimeStampHostTimeValid))
                                    ? timeStamp->mHostTime
                                    : AudioGetCurrentHostTime();
        const std::int64_t captureNs =
            coreAudioHostTimeToProbeNs(hostTime, self->clockOffsetNs) - self->latencyNs;
        self->sink->onInput(self->stereo.data(), static_cast<int>(n), self->deviceRate, captureNs);
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
        const std::vector<std::int32_t> map = coreAudioInputChannelMap(deviceChannels, request.pair);
        if (map.empty()) {
            return fail(QStringLiteral("The chosen channels are not on this device"), noErr);
        }
        // The unit renders the pair on its two client channels (a
        // one-channel pair on both); the pick then reads them as a pair.
        clientPair = AudioChannelPair{1, request.pair.channelCount == 1 ? 1 : 2};

        AudioComponentDescription description{};
        description.componentType = kAudioUnitType_Output;
        description.componentSubType = kAudioUnitSubType_HALOutput;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        AudioComponent component = AudioComponentFindNext(nullptr, &description);
        if (component == nullptr) {
            return fail(QStringLiteral("The system audio input unit is missing"), noErr);
        }
        OSStatus status = AudioComponentInstanceNew(component, &unit);
        if (status != noErr) {
            unit = nullptr;
            return fail(QStringLiteral("The audio input unit did not start"), status);
        }

        // Input on (element 1), output off (element 0).
        UInt32 enable = 1;
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input,
                                      1, &enable, sizeof(enable));
        if (status != noErr) {
            return fail(QStringLiteral("The device's input could not be enabled"), status);
        }
        UInt32 disable = 0;
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                                      0, &disable, sizeof(disable));
        if (status != noErr) {
            return fail(QStringLiteral("The device's output could not be disabled"), status);
        }
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice,
                                      kAudioUnitScope_Global, 0, &device, sizeof(device));
        if (status != noErr) {
            return fail(QStringLiteral("The device could not be chosen"), status);
        }

        const AudioValueRange range =
            coreaudio::read<AudioValueRange>(device, kAudioDevicePropertyBufferFrameSizeRange)
                .value_or(AudioValueRange{0.0, 0.0});
        UInt32 frames = static_cast<UInt32>(
            coreAudioBufferFrames(request.bufferFrames, range.mMinimum, range.mMaximum));
        const AudioObjectPropertyAddress frameSize = coreaudio::address(kAudioDevicePropertyBufferFrameSize);
        AudioObjectSetPropertyData(device, &frameSize, 0, nullptr, sizeof(frames), &frames);

        AudioStreamBasicDescription client{};
        client.mSampleRate = nominal;
        client.mFormatID = kAudioFormatLinearPCM;
        client.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
        client.mFramesPerPacket = 1;
        client.mChannelsPerFrame = 2;
        client.mBitsPerChannel = 32;
        client.mBytesPerFrame = 2 * sizeof(float);
        client.mBytesPerPacket = 2 * sizeof(float);
        status = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                      1, &client, sizeof(client));
        if (status != noErr) {
            return fail(QStringLiteral("The device did not give the stereo float format"), status);
        }
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output,
                                      1, map.data(), static_cast<UInt32>(map.size() * sizeof(SInt32)));
        if (status != noErr) {
            return fail(QStringLiteral("The chosen channels could not be set"), status);
        }

        AURenderCallbackStruct callback{};
        callback.inputProc = &Impl::input;
        callback.inputProcRefCon = this;
        status = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_SetInputCallback,
                                      kAudioUnitScope_Global, 0, &callback, sizeof(callback));
        if (status != noErr) {
            return fail(QStringLiteral("The input callback could not be set"), status);
        }
        status = AudioUnitInitialize(unit);
        if (status != noErr) {
            return fail(QStringLiteral("The audio input unit did not initialize"), status);
        }

        // The render buffer: the unit's largest slice, never fewer frames
        // than the device callback's.
        UInt32 maxSlice = 0;
        UInt32 size = sizeof(maxSlice);
        AudioUnitGetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0,
                             &maxSlice, &size);
        capacityFrames = std::max<UInt32>({maxSlice, frames, 4096});
        rendered.assign(std::size_t(2) * capacityFrames, 0.0f);
        stereo.assign(std::size_t(2) * capacityFrames, 0.0f);

        latencyNs = coreAudioFramesToNs(coreaudio::latencyFrames(device, kAudioObjectPropertyScopeInput),
                                        nominal);
        // The capture clock: host time and audioProbeNowNs() are read once
        // here and their difference is applied to every block.
        clockOffsetNs = coreAudioHostClockOffsetNs();

        watch.start(device, deviceRate, "com.nereussdr.coreaudio.input");
        status = AudioOutputUnitStart(unit);
        if (status != noErr) {
            return fail(QStringLiteral("The device did not start recording"), status);
        }
        return true;
    }
};

CoreAudioInputStream::CoreAudioInputStream(std::uint32_t deviceObjectId, int deviceInputChannels,
                                           AudioStreamRequest request, MicChannelPick pick,
                                           IAudioInputSink* sink)
    : d(std::make_unique<Impl>())
{
    d->device = deviceObjectId;
    d->deviceChannels = deviceInputChannels;
    d->request = std::move(request);
    d->pick = pick;
    d->sink = sink;
}

CoreAudioInputStream::~CoreAudioInputStream()
{
    close();
}

bool CoreAudioInputStream::open()
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
        qCWarning(lcAudio) << "Core Audio input did not open:" << d->error;
        return false;
    }
    d->open.store(true);
    return true;
}

void CoreAudioInputStream::close()
{
    d->open.store(false);
    d->watch.stop();
    d->disposeUnit();   // AudioOutputUnitStop waits for the input callback
}

bool CoreAudioInputStream::isOpen() const
{
    return d->open.load();
}

QString CoreAudioInputStream::errorString() const
{
    return d->error;
}

int CoreAudioInputStream::sampleRate() const
{
    return d->deviceRate;
}

std::optional<std::int64_t> CoreAudioInputStream::inputLatencyNs() const
{
    if (!d->open.load()) {
        return std::nullopt;
    }
    return d->latencyNs;
}

void CoreAudioInputStream::setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink)
{
    d->watch.setSink(std::move(sink));
}

#endif // Q_OS_MAC

} // namespace NereusSDR
