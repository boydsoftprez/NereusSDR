// =================================================================
// src/core/audio/WasapiSystemWin.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only (built inside
// if(WIN32) in CMakeLists.txt).  The Windows audio engine's calls into
// the system (R-AUD-02, R-AUD-03, R-AUD-14, R-AUD-16): COM on the calling
// thread, the endpoint list, the endpoint notices, and opening one shared
// or exclusive event-driven stream.  Every decision these calls follow is
// in WasapiPolicy, which is tested on every system.
//
// Include this header first in a Windows audio source file: it sets the
// Windows version the IAudioClient3 declarations need before any Windows
// header is read (these files are built without the precompiled header).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-14, R-AUD-16). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#pragma once

// IAudioClient3 is a Windows 10 interface; as DnsSdAdvertiserWindows.cpp
// does, raise an older target before the first Windows header.
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// mmreg.h comes before the other audio headers (PortAudio's MinGW note,
// pa_win_wasapi.c:79).
#include <windows.h>
#include <mmreg.h>
#include <objbase.h>
#include <propidl.h>
#include <propsys.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/WasapiPolicy.h"

#include <QList>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace NereusSDR {

// Interface and class ids, defined in WasapiSystemWin.cpp with their
// cites, so no Windows import library has to supply them.
extern const GUID kWasapiClsidMMDeviceEnumerator;
extern const IID kWasapiIidMMDeviceEnumerator;
extern const IID kWasapiIidMMNotificationClient;
extern const IID kWasapiIidAudioClient;
extern const IID kWasapiIidAudioClient3;
extern const IID kWasapiIidAudioRenderClient;
extern const IID kWasapiIidAudioCaptureClient;

// A COM interface pointer that releases what it holds.
template <class T>
class WasapiComPtr {
public:
    WasapiComPtr() = default;
    ~WasapiComPtr() { reset(); }
    WasapiComPtr(const WasapiComPtr&) = delete;
    WasapiComPtr& operator=(const WasapiComPtr&) = delete;
    WasapiComPtr(WasapiComPtr&& other) noexcept : m_p(other.m_p) { other.m_p = nullptr; }
    WasapiComPtr& operator=(WasapiComPtr&& other) noexcept
    {
        if (this != &other) {
            reset();
            m_p = other.m_p;
            other.m_p = nullptr;
        }
        return *this;
    }

    T* get() const { return m_p; }
    T* operator->() const { return m_p; }
    explicit operator bool() const { return m_p != nullptr; }
    void reset()
    {
        if (m_p != nullptr) {
            m_p->Release();
            m_p = nullptr;
        }
    }
    // For an out parameter: releases what it held first.
    T** put()
    {
        reset();
        return &m_p;
    }
    void** putVoid() { return reinterpret_cast<void**>(put()); }

private:
    T* m_p = nullptr;
};

// COM on this thread for this scope (multithreaded).  A thread already in
// a single-threaded apartment keeps it and COM still works there.
class WasapiComScope {
public:
    WasapiComScope();
    ~WasapiComScope();
    WasapiComScope(const WasapiComScope&) = delete;
    WasapiComScope& operator=(const WasapiComScope&) = delete;
    bool usable() const { return m_usable; }

private:
    bool m_usable = false;
    bool m_uninitialize = false;
};

// A Win32 event handle closed at the end of its scope.
class WasapiEventHandle {
public:
    WasapiEventHandle() = default;
    ~WasapiEventHandle() { reset(); }
    WasapiEventHandle(const WasapiEventHandle&) = delete;
    WasapiEventHandle& operator=(const WasapiEventHandle&) = delete;
    bool create();          // auto-reset, not signalled
    HANDLE get() const { return m_handle; }
    void reset();

private:
    HANDLE m_handle = nullptr;
};

// The HRESULT as WasapiPolicy reads it.
WasapiResult wasapiResultOf(HRESULT hr);

QString wasapiWideToQString(const wchar_t* text);

struct WasapiEndpoint {
    QString id;               // the endpoint ID string
    QString name;             // PKEY_Device_FriendlyName
    QString enumeratorName;   // PKEY_Device_EnumeratorName
    int formFactor = -1;      // PKEY_AudioEndpoint_FormFactor
    int channels = 0;         // the mix format's
};

// The active endpoints of one direction (DEVICE_STATE_ACTIVE only).  The
// caller holds a WasapiComScope.  serviceAnswered false: the system's
// audio service did not answer.
QList<WasapiEndpoint> wasapiActiveEndpoints(AudioDeviceDirection direction, bool* serviceAnswered);

// The console default endpoint of one direction (design choice 1).
std::optional<QString> wasapiDefaultEndpointId(AudioDeviceDirection direction);

// IMMNotificationClient registration for the life of the object.  Each
// notice is mapped by wasapiNoticeFor and handed to the sink; the client
// only posts (no blocking, no register or unregister inside it, and the
// object, not the system, holds its last reference).  Registered and
// unregistered on a short-lived thread of its own in the multithreaded
// apartment, whatever apartment the caller's thread is in.
class WasapiEndpointNotifier {
public:
    using Sink = std::function<void(AudioNotice)>;
    WasapiEndpointNotifier();
    ~WasapiEndpointNotifier();
    WasapiEndpointNotifier(const WasapiEndpointNotifier&) = delete;
    WasapiEndpointNotifier& operator=(const WasapiEndpointNotifier&) = delete;

    bool registered() const;
    void setSink(Sink sink);   // any thread; empty drops notices

private:
    struct Client;
    struct SinkState;
    std::shared_ptr<SinkState> m_state;
    std::unique_ptr<Client> m_client;
    WasapiComPtr<IMMDeviceEnumerator> m_enumerator;
    bool m_registered = false;
};

// One event-driven stream, initialised and not started.
struct WasapiOpenedStream {
    WasapiComPtr<IMMDevice> device;
    WasapiComPtr<IAudioClient> client;
    WasapiEventHandle bufferEvent;
    QString deviceName;
    DeviceSampleFormat format = DeviceSampleFormat::Float32;
    int channels = 0;
    int rate = 0;
    int bufferFrames = 0;
    int periodFrames = 0;
    bool exclusive = false;
    std::int64_t latencyNs = 0;
};

// Opens deviceId (empty: the console default) for direction.  Shared: the
// mix format at wasapiSharedPeriodFrames through IAudioClient3, or the
// ordinary event-driven shared stream when IAudioClient3 fails.
// Exclusive: the first of wasapiExclusiveFormatOrder() the device takes,
// at wasapiExclusivePeriodHns, realigned once on BufferSizeNotAligned.
// The caller holds a WasapiComScope on the thread that will run the
// stream.  error says what failed.
WasapiResult wasapiOpenStream(const QString& deviceId, AudioDeviceDirection direction,
                              bool exclusive, int requestedFrames, WasapiOpenedStream& stream,
                              QString& error);

// Whether the endpoint is still DEVICE_STATE_ACTIVE.
bool wasapiEndpointActive(IMMDevice* device);

// Runs fn on a new thread in the multithreaded apartment and waits for it.
void wasapiRunInMta(const std::function<void()>& fn);

} // namespace NereusSDR
