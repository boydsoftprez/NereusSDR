// =================================================================
// src/core/audio/WasapiPolicy.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See WasapiPolicy.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-11, R-AUD-14, R-AUD-16). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/WasapiPolicy.h"

#include <algorithm>

namespace NereusSDR {

namespace {

// REFERENCE_TIME counts 100 ns units: 10'000'000 per second.
constexpr std::int64_t kHnsPerSecond = 10'000'000;

// AUDCLNT_ERR(n) = 0x88890000 | n; the top bit set is a failure.
constexpr std::uint32_t kHresultFailureBit = 0x80000000u;

} // namespace

QList<WasapiFormatCandidate> wasapiExclusiveFormatOrder()
{
    return {
        {WasapiSampleType::Float, 32, 32},
        {WasapiSampleType::Int, 32, 32},
        {WasapiSampleType::Int, 32, 24},
        {WasapiSampleType::Int, 16, 16},
    };
}

DeviceSampleFormat wasapiDeviceFormat(const WasapiFormatCandidate& candidate)
{
    if (candidate.type == WasapiSampleType::Float) {
        return candidate.containerBits == 64 ? DeviceSampleFormat::Float64
                                             : DeviceSampleFormat::Float32;
    }
    switch (candidate.containerBits) {
    case 16:
        return DeviceSampleFormat::Int16;
    case 24:
        return DeviceSampleFormat::Int24Packed;
    default:
        return DeviceSampleFormat::Int32;
    }
}

std::optional<DeviceSampleFormat> wasapiMixSampleFormat(const WasapiFormatCandidate& candidate)
{
    if (candidate.validBits <= 0 || candidate.validBits > candidate.containerBits) {
        return std::nullopt;
    }
    if (candidate.type == WasapiSampleType::Float) {
        if (candidate.containerBits == 32 || candidate.containerBits == 64) {
            return wasapiDeviceFormat(candidate);
        }
        return std::nullopt;
    }
    if (candidate.containerBits == 16 || candidate.containerBits == 24
        || candidate.containerBits == 32) {
        return wasapiDeviceFormat(candidate);
    }
    return std::nullopt;
}

int wasapiSharedPeriodFrames(const std::optional<WasapiEnginePeriods>& periods, int requestedFrames)
{
    if (!periods) {
        return 0;
    }
    const WasapiEnginePeriods& p = *periods;
    const int minFrames = std::max(1, p.minFrames);
    const int maxFrames = std::max(minFrames, p.maxFrames);
    if (requestedFrames <= 0) {
        return minFrames;
    }
    int frames = requestedFrames;
    if (p.fundamentalFrames > 0) {
        frames = ((requestedFrames + p.fundamentalFrames - 1) / p.fundamentalFrames)
                 * p.fundamentalFrames;
    }
    return std::clamp(frames, minFrames, maxFrames);
}

std::int64_t wasapiAlignedPeriodHns(int bufferFrames, int rate)
{
    if (bufferFrames <= 0 || rate <= 0) {
        return 0;
    }
    // (10'000'000 x frames / rate) + 0.5, truncated: round half up, in
    // integers so 441 frames at 44.1 kHz is exactly 100000.
    const std::int64_t twice = 2 * kHnsPerSecond * std::int64_t(bufferFrames) + std::int64_t(rate);
    return twice / (2 * std::int64_t(rate));
}

std::int64_t wasapiExclusivePeriodHns(std::int64_t minimumPeriodHns, int requestedFrames, int rate)
{
    if (requestedFrames <= 0 || rate <= 0) {
        return minimumPeriodHns;
    }
    return std::max(minimumPeriodHns, wasapiAlignedPeriodHns(requestedFrames, rate));
}

int wasapiFramesForHns(std::int64_t hns, int rate)
{
    if (hns <= 0 || rate <= 0) {
        return 0;
    }
    const std::int64_t twice = 2 * hns * std::int64_t(rate) + kHnsPerSecond;
    return int(twice / (2 * kHnsPerSecond));
}

int wasapiFramesToWrite(bool exclusive, int bufferFrames, int paddingFrames)
{
    if (exclusive) {
        return std::max(0, bufferFrames);
    }
    return std::max(0, bufferFrames - paddingFrames);
}

std::optional<AudioNotice> wasapiNoticeFor(WasapiNotification notification, WasapiFlow flow,
                                           WasapiRole role)
{
    switch (notification) {
    case WasapiNotification::DeviceAdded:
    case WasapiNotification::DeviceRemoved:
    case WasapiNotification::StateChanged:
    case WasapiNotification::NameChanged:
        return AudioNotice::DevicesChanged;
    case WasapiNotification::DefaultChanged:
        // Design choice 1: the default device, never the default
        // communications (or multimedia) device.
        if (role != WasapiRole::Console) {
            return std::nullopt;
        }
        if (flow == WasapiFlow::Render) {
            return AudioNotice::DefaultOutputChanged;
        }
        if (flow == WasapiFlow::Capture) {
            return AudioNotice::DefaultInputChanged;
        }
        // Both directions: a re-list reads both defaults again.
        return AudioNotice::DevicesChanged;
    case WasapiNotification::OtherPropertyChanged:
        return std::nullopt;
    }
    return std::nullopt;
}

AudioTransport wasapiTransport(const QString& enumeratorName, int formFactor)
{
    if (enumeratorName.compare(QStringLiteral("BTHENUM"), Qt::CaseInsensitive) == 0
        || enumeratorName.compare(QStringLiteral("BTHHFENUM"), Qt::CaseInsensitive) == 0) {
        return AudioTransport::Bluetooth;
    }
    if (enumeratorName.compare(QStringLiteral("USB"), Qt::CaseInsensitive) == 0) {
        return AudioTransport::Usb;
    }
    if (formFactor == kWasapiFormFactorDigitalAudioDisplayDevice) {
        return AudioTransport::Hdmi;
    }
    return AudioTransport::Unknown;
}

WasapiResult wasapiResultFor(std::uint32_t hresult)
{
    if ((hresult & kHresultFailureBit) == 0) {
        return WasapiResult::Ok;
    }
    switch (hresult) {
    case kWasapiDeviceInvalidated:
        return WasapiResult::DeviceInvalidated;
    case kWasapiDeviceInUse:
        return WasapiResult::DeviceInUse;
    case kWasapiExclusiveModeNotAllowed:
        return WasapiResult::ExclusiveNotAllowed;
    case kWasapiUnsupportedFormat:
        return WasapiResult::UnsupportedFormat;
    case kWasapiBufferSizeNotAligned:
        return WasapiResult::BufferSizeNotAligned;
    case kWasapiServiceNotRunning:
        return WasapiResult::ServiceNotRunning;
    default:
        return WasapiResult::Other;
    }
}

AudioOpenResult wasapiOpenResult(WasapiResult result)
{
    switch (result) {
    case WasapiResult::Ok:
        return AudioOpenResult::Opened;
    case WasapiResult::DeviceInUse:
    case WasapiResult::ExclusiveNotAllowed:
        return AudioOpenResult::InUse;
    case WasapiResult::DeviceInvalidated:
        return AudioOpenResult::NotFound;
    case WasapiResult::UnsupportedFormat:
    case WasapiResult::BufferSizeNotAligned:
    case WasapiResult::ServiceNotRunning:
    case WasapiResult::Other:
        return AudioOpenResult::Failed;
    }
    return AudioOpenResult::Failed;
}

std::optional<AudioStreamEvent::Kind> wasapiStreamEvent(WasapiResult result)
{
    switch (result) {
    case WasapiResult::DeviceInUse:
    case WasapiResult::ExclusiveNotAllowed:
        return AudioStreamEvent::Kind::DeviceBusy;
    case WasapiResult::DeviceInvalidated:
        return AudioStreamEvent::Kind::DeviceLost;
    case WasapiResult::Ok:
    case WasapiResult::UnsupportedFormat:
    case WasapiResult::BufferSizeNotAligned:
    case WasapiResult::ServiceNotRunning:
    case WasapiResult::Other:
        return std::nullopt;
    }
    return std::nullopt;
}

AudioStreamEvent::Kind wasapiRunningStreamEvent(WasapiResult result, bool endpointStillActive)
{
    if (result == WasapiResult::DeviceInvalidated && endpointStillActive) {
        return AudioStreamEvent::Kind::FormatChanged;
    }
    return wasapiStreamEvent(result).value_or(AudioStreamEvent::Kind::ResetRequested);
}

} // namespace NereusSDR
