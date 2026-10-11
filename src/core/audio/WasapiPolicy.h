// =================================================================
// src/core/audio/WasapiPolicy.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Every decision the Windows audio
// engine makes (R-AUD-02, R-AUD-03, R-AUD-11, R-AUD-14, R-AUD-16), kept
// portable so it is tested on every system; the Windows-only adapter
// files (Wasapi*Win) only call the system and follow these answers.
//
// Values mirrored from the Windows headers are cited where they are
// defined; WasapiSystemWin.cpp static_asserts each one against the real
// macro or enum, so a wrong mirror stops the Windows build.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-11, R-AUD-14, R-AUD-16). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/IAudioStreamHost.h"

#include <QList>
#include <QString>

#include <cstdint>
#include <optional>

namespace NereusSDR {

enum class WasapiSampleType { Float, Int };

struct WasapiFormatCandidate {
    WasapiSampleType type;
    int containerBits;
    int validBits;
    friend bool operator==(const WasapiFormatCandidate&, const WasapiFormatCandidate&) = default;
};

// Exclusive try order (settled call 14): float32, int32, 24 valid in a
// 32-bit container, int16.
QList<WasapiFormatCandidate> wasapiExclusiveFormatOrder();

// The format writeStereoToDevice writes for a candidate.  24 valid bits in
// a 32-bit container write as Int32: Windows puts the valid bits at the
// top of the container, so a full-scale 32-bit sample carries them.
DeviceSampleFormat wasapiDeviceFormat(const WasapiFormatCandidate& candidate);

// The shared stream's mix format, as DeviceSampleFormat can write it:
// float 32 or 64, int 16, int 24 packed, int in a 32-bit container.
// nullopt for anything else (the stream does not open).
std::optional<DeviceSampleFormat> wasapiMixSampleFormat(const WasapiFormatCandidate& candidate);

struct WasapiEnginePeriods {
    int defaultFrames;
    int fundamentalFrames;
    int minFrames;
    int maxFrames;
};

// Shared: IAudioClient3's smallest period (or the request rounded up to a
// multiple of the fundamental, clamped to min..max); nullopt periods mean
// IAudioClient3 failed and the stream uses the ordinary event-driven
// shared stream at the engine's default period (the function returns 0).
int wasapiSharedPeriodFrames(const std::optional<WasapiEnginePeriods>& periods, int requestedFrames);

// Exclusive: the device's minimum period, or the request if larger; and
// the realignment after AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
// hns = (10'000'000 x frames / rate) + 0.5.
std::int64_t wasapiExclusivePeriodHns(std::int64_t minimumPeriodHns, int requestedFrames, int rate);
std::int64_t wasapiAlignedPeriodHns(int bufferFrames, int rate);

// The frames one period of the device's buffer holds, rounded to nearest:
// how many frames a period of hns lasts at rate.
int wasapiFramesForHns(std::int64_t hns, int rate);

// Frames to write at one buffer event: exclusive event-driven streams
// fill the whole buffer at each event; shared streams fill what the
// engine has played (the buffer less its padding).
int wasapiFramesToWrite(bool exclusive, int bufferFrames, int paddingFrames);

// Mirrors EDataFlow and ERole (mmdeviceapi.h, the MinGW copy PortAudio
// bundles at src/hostapi/wasapi/mingw-include/mmdeviceapi.h:151-163).
enum class WasapiFlow { Render, Capture, All };
enum class WasapiRole { Console, Multimedia, Communications };

enum class WasapiNotification {
    DeviceAdded,
    DeviceRemoved,
    StateChanged,
    DefaultChanged,
    NameChanged,
    OtherPropertyChanged
};

// IMMNotificationClient's notices as the catalogue hears them.  The list
// changes for an added, removed, re-stated or renamed endpoint; the
// default changes only for the console role (design choice 1: the
// default device, not the default communications device).
std::optional<AudioNotice> wasapiNoticeFor(WasapiNotification notification, WasapiFlow flow,
                                           WasapiRole role);

// R-AUD-14: the transport from PKEY_Device_EnumeratorName and
// PKEY_AudioEndpoint_FormFactor.  "BTHENUM" or "BTHHFENUM" (any case) is
// Bluetooth, "USB" is Usb, form factor 9 (DigitalAudioDisplayDevice,
// named HDMI in older headers) is Hdmi, anything else Unknown.  The
// Bluetooth rule is pending the bench.
inline constexpr int kWasapiFormFactorDigitalAudioDisplayDevice = 9;
AudioTransport wasapiTransport(const QString& enumeratorName, int formFactor);

// AUDCLNT_ERR(n) = 0x88890000 | n: FACILITY_AUDCLNT 0x889 with the error
// severity (PortAudio's bundled mingw-include/audioclient.h:1133-1134).
// The first five as at audioclient.h:1139-1150 there; 0x019 as at
// pa_win_wasapi.c:321-322 (PortAudio defines it where an older header
// lacks it) and windows-rs 0.54.0 Win32/Media/Audio/mod.rs:3583.
inline constexpr std::uint32_t kWasapiDeviceInvalidated = 0x88890004u;
inline constexpr std::uint32_t kWasapiUnsupportedFormat = 0x88890008u;
inline constexpr std::uint32_t kWasapiDeviceInUse = 0x8889000Au;
inline constexpr std::uint32_t kWasapiExclusiveModeNotAllowed = 0x8889000Eu;
inline constexpr std::uint32_t kWasapiServiceNotRunning = 0x88890010u;
inline constexpr std::uint32_t kWasapiBufferSizeNotAligned = 0x88890019u;

enum class WasapiResult {
    Ok,
    DeviceInvalidated,
    DeviceInUse,
    ExclusiveNotAllowed,
    UnsupportedFormat,
    BufferSizeNotAligned,
    ServiceNotRunning,
    Other
};

// Any success code (the top bit clear) is Ok.
WasapiResult wasapiResultFor(std::uint32_t hresult);

// How a failed open reads to the supervisor: held by another program
// (exclusive in use, or exclusive refused) is InUse (R-AUD-11); an
// endpoint that went away is NotFound; anything else Failed.
AudioOpenResult wasapiOpenResult(WasapiResult result);

// The stream event a result posts: DeviceBusy for InUse, DeviceLost for
// an invalidated endpoint, none otherwise.
std::optional<AudioStreamEvent::Kind> wasapiStreamEvent(WasapiResult result);

// The event a running stream posts when a buffer call fails and its
// thread stops.  An invalidated endpoint that is still active had its
// format changed and reopens on the same device (design choice 3);
// otherwise as wasapiStreamEvent, and any other failure asks for a
// rebuild (ResetRequested), so a stopped stream is never left silent.
AudioStreamEvent::Kind wasapiRunningStreamEvent(WasapiResult result, bool endpointStillActive);

} // namespace NereusSDR
