// =================================================================
// src/core/audio/WasapiSystemWin.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only.  See
// WasapiSystemWin.h.  Interface signatures are the Windows headers'
// (PortAudio's bundled MinGW copies at src/hostapi/wasapi/mingw-include
// for IAudioClient, IAudioRenderClient, IAudioCaptureClient and the
// MMDevice interfaces; windows-rs 0.54.0 Win32/Media/Audio/mod.rs:
// 1513-1529 for IAudioClient3, which those copies predate).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-14, R-AUD-16). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/WasapiSystemWin.h"

#include "core/LogCategories.h"

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// The policy's mirrors, checked against the real headers.

static_assert(std::uint32_t(AUDCLNT_E_DEVICE_INVALIDATED) == kWasapiDeviceInvalidated,
              "AUDCLNT_E_DEVICE_INVALIDATED mirror");
static_assert(std::uint32_t(AUDCLNT_E_DEVICE_IN_USE) == kWasapiDeviceInUse,
              "AUDCLNT_E_DEVICE_IN_USE mirror");
static_assert(std::uint32_t(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED) == kWasapiExclusiveModeNotAllowed,
              "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED mirror");
static_assert(std::uint32_t(AUDCLNT_E_UNSUPPORTED_FORMAT) == kWasapiUnsupportedFormat,
              "AUDCLNT_E_UNSUPPORTED_FORMAT mirror");
static_assert(std::uint32_t(AUDCLNT_E_SERVICE_NOT_RUNNING) == kWasapiServiceNotRunning,
              "AUDCLNT_E_SERVICE_NOT_RUNNING mirror");
#ifdef AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED
static_assert(std::uint32_t(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) == kWasapiBufferSizeNotAligned,
              "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED mirror");
#else
// A header without it (PortAudio defines it the same way,
// pa_win_wasapi.c:321-322).
static_assert(std::uint32_t(AUDCLNT_ERR(0x019)) == kWasapiBufferSizeNotAligned,
              "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED mirror");
#endif
static_assert(int(eRender) == 0 && int(eCapture) == 1 && int(eAll) == 2, "EDataFlow mirror");
static_assert(int(eConsole) == 0 && int(eMultimedia) == 1 && int(eCommunications) == 2,
              "ERole mirror");

// ---------------------------------------------------------------------------
// Ids.  GUID values as PortAudio defines them, pa_win_wasapi.c:256-285,
// and IMMNotificationClient's as mmdeviceapi.h:196 (the bundled copy)
// declares it.

const GUID kWasapiClsidMMDeviceEnumerator = {
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}};
const IID kWasapiIidMMDeviceEnumerator = {
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};
const IID kWasapiIidMMNotificationClient = {
    0x7991eec9, 0x7e89, 0x4d85, {0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0}};
const IID kWasapiIidAudioClient = {
    0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}};
const IID kWasapiIidAudioClient3 = {
    0x7ed4ee07, 0x8e67, 0x4cd4, {0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42}};
const IID kWasapiIidAudioRenderClient = {
    0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}};
const IID kWasapiIidAudioCaptureClient = {
    0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}};

namespace {

// KSDATAFORMAT_SUBTYPE_PCM and _IEEE_FLOAT, pa_win_wasapi.c:283-285.
const GUID kSubtypePcm = {
    0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID kSubtypeIeeeFloat = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

// PKEY_Device_FriendlyName (functiondiscoverykeys_devpkey.h:11, the
// bundled copy), PKEY_Device_EnumeratorName (windows-rs 0.54.0
// Win32/Devices/FunctionDiscovery/mod.rs:1003) and
// PKEY_AudioEndpoint_FormFactor (mmdeviceapi.h:130, the bundled copy).
const PROPERTYKEY kKeyFriendlyName = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
const PROPERTYKEY kKeyEnumeratorName = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 24};
const PROPERTYKEY kKeyFormFactor = {
    {0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}}, 0};

struct CoTaskMemDeleter {
    void operator()(void* p) const { CoTaskMemFree(p); }
};
using WaveFormatPtr = std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter>;

EDataFlow flowFor(AudioDeviceDirection direction)
{
    return direction == AudioDeviceDirection::Output ? eRender : eCapture;
}

WasapiFlow policyFlow(EDataFlow flow)
{
    if (flow == eRender) {
        return WasapiFlow::Render;
    }
    if (flow == eCapture) {
        return WasapiFlow::Capture;
    }
    return WasapiFlow::All;
}

WasapiRole policyRole(ERole role)
{
    if (role == eMultimedia) {
        return WasapiRole::Multimedia;
    }
    if (role == eCommunications) {
        return WasapiRole::Communications;
    }
    return WasapiRole::Console;
}

bool sameKey(const PROPERTYKEY& a, const PROPERTYKEY& b)
{
    return a.pid == b.pid && IsEqualGUID(a.fmtid, b.fmtid);
}

QString stringProperty(IPropertyStore* store, const PROPERTYKEY& key)
{
    PROPVARIANT value;
    PropVariantInit(&value);
    QString text;
    if (SUCCEEDED(store->GetValue(key, &value)) && value.vt == VT_LPWSTR
        && value.pwszVal != nullptr) {
        text = wasapiWideToQString(value.pwszVal);
    }
    PropVariantClear(&value);
    return text;
}

int uintProperty(IPropertyStore* store, const PROPERTYKEY& key)
{
    PROPVARIANT value;
    PropVariantInit(&value);
    int result = -1;
    if (SUCCEEDED(store->GetValue(key, &value)) && value.vt == VT_UI4) {
        result = int(value.ulVal);
    }
    PropVariantClear(&value);
    return result;
}

QString endpointId(IMMDevice* device)
{
    LPWSTR raw = nullptr;
    if (FAILED(device->GetId(&raw)) || raw == nullptr) {
        return {};
    }
    QString id = wasapiWideToQString(raw);
    CoTaskMemFree(raw);
    return id;
}

bool makeEnumerator(WasapiComPtr<IMMDeviceEnumerator>& enumerator, HRESULT* hrOut)
{
    const HRESULT hr = CoCreateInstance(kWasapiClsidMMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                        kWasapiIidMMDeviceEnumerator, enumerator.putVoid());
    if (hrOut != nullptr) {
        *hrOut = hr;
    }
    return SUCCEEDED(hr) && enumerator;
}

bool activateClient(IMMDevice* device, WasapiComPtr<IAudioClient>& client, HRESULT* hrOut)
{
    const HRESULT hr = device->Activate(kWasapiIidAudioClient, CLSCTX_ALL, nullptr,
                                        client.putVoid());
    if (hrOut != nullptr) {
        *hrOut = hr;
    }
    return SUCCEEDED(hr) && client;
}

// The mix format as a policy candidate; nullopt for a tag NereusSDR
// cannot write.
std::optional<WasapiFormatCandidate> candidateOf(const WAVEFORMATEX& format)
{
    const int bits = int(format.wBitsPerSample);
    constexpr WORD kExtensibleExtra = WORD(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && format.cbSize >= kExtensibleExtra) {
        const auto* x = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&format);
        const int valid = x->Samples.wValidBitsPerSample != 0 ? int(x->Samples.wValidBitsPerSample)
                                                              : bits;
        if (IsEqualGUID(x->SubFormat, kSubtypeIeeeFloat)) {
            return WasapiFormatCandidate{WasapiSampleType::Float, bits, valid};
        }
        if (IsEqualGUID(x->SubFormat, kSubtypePcm)) {
            return WasapiFormatCandidate{WasapiSampleType::Int, bits, valid};
        }
        return std::nullopt;
    }
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return WasapiFormatCandidate{WasapiSampleType::Float, bits, bits};
    }
    if (format.wFormatTag == WAVE_FORMAT_PCM) {
        return WasapiFormatCandidate{WasapiSampleType::Int, bits, bits};
    }
    return std::nullopt;
}

DWORD channelMaskOf(const WAVEFORMATEX& format)
{
    constexpr WORD kExtensibleExtra = WORD(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    if (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && format.cbSize >= kExtensibleExtra) {
        return reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&format)->dwChannelMask;
    }
    return 0;
}

WAVEFORMATEXTENSIBLE exclusiveFormat(const WasapiFormatCandidate& candidate, int channels,
                                     int rate, DWORD channelMask)
{
    WAVEFORMATEXTENSIBLE x;
    std::memset(&x, 0, sizeof(x));
    x.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    x.Format.nChannels = WORD(channels);
    x.Format.nSamplesPerSec = DWORD(rate);
    x.Format.wBitsPerSample = WORD(candidate.containerBits);
    x.Format.nBlockAlign = WORD(channels * candidate.containerBits / 8);
    x.Format.nAvgBytesPerSec = DWORD(rate) * DWORD(x.Format.nBlockAlign);
    x.Format.cbSize = WORD(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    x.Samples.wValidBitsPerSample = WORD(candidate.validBits);
    x.dwChannelMask = channelMask;
    x.SubFormat = candidate.type == WasapiSampleType::Float ? kSubtypeIeeeFloat : kSubtypePcm;
    return x;
}

QString hrText(HRESULT hr)
{
    return QStringLiteral("0x%1").arg(quint32(hr), 8, 16, QLatin1Char('0'));
}

} // namespace

// ---------------------------------------------------------------------------

WasapiComScope::WasapiComScope()
{
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        m_usable = true;
        m_uninitialize = true;
    } else if (hr == RPC_E_CHANGED_MODE) {
        m_usable = true;   // a single-threaded apartment already; COM works there
    } else {
        qCWarning(lcAudio) << "Windows audio: COM did not start on this thread" << hrText(hr);
    }
}

WasapiComScope::~WasapiComScope()
{
    if (m_uninitialize) {
        CoUninitialize();
    }
}

bool WasapiEventHandle::create()
{
    reset();
    m_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return m_handle != nullptr;
}

void WasapiEventHandle::reset()
{
    if (m_handle != nullptr) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
}

WasapiResult wasapiResultOf(HRESULT hr)
{
    return wasapiResultFor(std::uint32_t(hr));
}

QString wasapiWideToQString(const wchar_t* text)
{
    return text != nullptr ? QString::fromWCharArray(text) : QString();
}

QList<WasapiEndpoint> wasapiActiveEndpoints(AudioDeviceDirection direction, bool* serviceAnswered)
{
    QList<WasapiEndpoint> endpoints;
    if (serviceAnswered != nullptr) {
        *serviceAnswered = true;
    }
    WasapiComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = S_OK;
    if (!makeEnumerator(enumerator, &hr)) {
        qCWarning(lcAudio) << "Windows audio: no device enumerator" << hrText(hr);
        if (serviceAnswered != nullptr) {
            *serviceAnswered = false;
        }
        return endpoints;
    }
    WasapiComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(flowFor(direction), DEVICE_STATE_ACTIVE, collection.put());
    if (FAILED(hr) || !collection) {
        qCWarning(lcAudio) << "Windows audio: endpoints not listed" << hrText(hr);
        if (serviceAnswered != nullptr && wasapiResultOf(hr) == WasapiResult::ServiceNotRunning) {
            *serviceAnswered = false;
        }
        return endpoints;
    }
    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) {
        return endpoints;
    }
    for (UINT i = 0; i < count; ++i) {
        WasapiComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, device.put())) || !device) {
            continue;
        }
        WasapiEndpoint endpoint;
        endpoint.id = endpointId(device.get());
        if (endpoint.id.isEmpty()) {
            continue;
        }
        WasapiComPtr<IPropertyStore> store;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, store.put())) && store) {
            endpoint.name = stringProperty(store.get(), kKeyFriendlyName);
            endpoint.enumeratorName = stringProperty(store.get(), kKeyEnumeratorName);
            endpoint.formFactor = uintProperty(store.get(), kKeyFormFactor);
        }
        if (endpoint.name.isEmpty()) {
            endpoint.name = endpoint.id;
        }
        WasapiComPtr<IAudioClient> client;
        if (activateClient(device.get(), client, nullptr)) {
            WAVEFORMATEX* raw = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&raw)) && raw != nullptr) {
                const WaveFormatPtr mix(raw);
                endpoint.channels = int(mix->nChannels);
            }
        }
        endpoints.append(endpoint);
    }
    return endpoints;
}

std::optional<QString> wasapiDefaultEndpointId(AudioDeviceDirection direction)
{
    WasapiComPtr<IMMDeviceEnumerator> enumerator;
    if (!makeEnumerator(enumerator, nullptr)) {
        return std::nullopt;
    }
    WasapiComPtr<IMMDevice> device;
    // Design choice 1: the console default, not the communications one.
    if (FAILED(enumerator->GetDefaultAudioEndpoint(flowFor(direction), eConsole, device.put()))
        || !device) {
        return std::nullopt;
    }
    const QString id = endpointId(device.get());
    if (id.isEmpty()) {
        return std::nullopt;
    }
    return id;
}

// ---------------------------------------------------------------------------
// Endpoint notices.

struct WasapiEndpointNotifier::SinkState {
    std::mutex mutex;
    Sink sink;
};

// The system calls these on its own threads.  Each maps the notice and
// hands it to the sink, which only posts.  The notifier owns the object;
// Release never deletes it, so the system never holds the last reference.
struct WasapiEndpointNotifier::Client final : public IMMNotificationClient {
    explicit Client(std::shared_ptr<SinkState> state) : m_state(std::move(state)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kWasapiIidMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ULONG(m_refs.fetch_add(1) + 1); }
    ULONG STDMETHODCALLTYPE Release() override { return ULONG(m_refs.fetch_sub(1) - 1); }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR /*pwstrDeviceId*/,
                                                   DWORD /*dwNewState*/) override
    {
        post(WasapiNotification::StateChanged, WasapiFlow::All, WasapiRole::Console);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR /*pwstrDeviceId*/) override
    {
        post(WasapiNotification::DeviceAdded, WasapiFlow::All, WasapiRole::Console);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR /*pwstrDeviceId*/) override
    {
        post(WasapiNotification::DeviceRemoved, WasapiFlow::All, WasapiRole::Console);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role,
                                                     LPCWSTR /*pwstrDefaultDeviceId*/) override
    {
        post(WasapiNotification::DefaultChanged, policyFlow(flow), policyRole(role));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR /*pwstrDeviceId*/,
                                                     const PROPERTYKEY key) override
    {
        post(sameKey(key, kKeyFriendlyName) ? WasapiNotification::NameChanged
                                            : WasapiNotification::OtherPropertyChanged,
             WasapiFlow::All, WasapiRole::Console);
        return S_OK;
    }

private:
    void post(WasapiNotification notification, WasapiFlow flow, WasapiRole role)
    {
        const std::optional<AudioNotice> notice = wasapiNoticeFor(notification, flow, role);
        if (!notice) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (m_state->sink) {
            m_state->sink(*notice);
        }
    }

    std::shared_ptr<SinkState> m_state;
    std::atomic<long> m_refs{1};
};

WasapiEndpointNotifier::WasapiEndpointNotifier()
    : m_state(std::make_shared<SinkState>())
    , m_client(std::make_unique<Client>(m_state))
{
    wasapiRunInMta([this] {
        HRESULT hr = S_OK;
        if (!makeEnumerator(m_enumerator, &hr)) {
            qCWarning(lcAudio) << "Windows audio: device notices unavailable" << hrText(hr);
            return;
        }
        hr = m_enumerator->RegisterEndpointNotificationCallback(m_client.get());
        if (FAILED(hr)) {
            qCWarning(lcAudio) << "Windows audio: device notices not registered" << hrText(hr);
            m_enumerator.reset();
            return;
        }
        m_registered = true;
    });
}

WasapiEndpointNotifier::~WasapiEndpointNotifier()
{
    setSink({});
    wasapiRunInMta([this] {
        if (m_enumerator && m_registered) {
            m_enumerator->UnregisterEndpointNotificationCallback(m_client.get());
        }
        m_registered = false;
        m_enumerator.reset();
    });
    // m_client is destroyed after the enumerator let go of it.
}

bool WasapiEndpointNotifier::registered() const
{
    return m_registered;
}

void WasapiEndpointNotifier::setSink(Sink sink)
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->sink = std::move(sink);
}

// ---------------------------------------------------------------------------
// Streams.

bool wasapiEndpointActive(IMMDevice* device)
{
    if (device == nullptr) {
        return false;
    }
    DWORD state = 0;
    return SUCCEEDED(device->GetState(&state)) && state == DEVICE_STATE_ACTIVE;
}

WasapiResult wasapiOpenStream(const QString& deviceId, AudioDeviceDirection direction,
                              bool exclusive, int requestedFrames, WasapiOpenedStream& stream,
                              QString& error)
{
    stream.exclusive = exclusive;
    WasapiComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = S_OK;
    if (!makeEnumerator(enumerator, &hr)) {
        error = QStringLiteral("no device enumerator ") + hrText(hr);
        return wasapiResultOf(hr);
    }
    if (deviceId.isEmpty()) {
        hr = enumerator->GetDefaultAudioEndpoint(flowFor(direction), eConsole, stream.device.put());
    } else {
        const std::wstring wide = deviceId.toStdWString();
        hr = enumerator->GetDevice(wide.c_str(), stream.device.put());
    }
    if (FAILED(hr) || !stream.device) {
        error = QStringLiteral("endpoint not found ") + hrText(hr);
        return WasapiResult::DeviceInvalidated;
    }
    {
        WasapiComPtr<IPropertyStore> store;
        if (SUCCEEDED(stream.device->OpenPropertyStore(STGM_READ, store.put())) && store) {
            stream.deviceName = stringProperty(store.get(), kKeyFriendlyName);
        }
    }

    if (!activateClient(stream.device.get(), stream.client, &hr)) {
        error = QStringLiteral("no audio client ") + hrText(hr);
        return wasapiResultOf(hr);
    }
    WAVEFORMATEX* rawMix = nullptr;
    hr = stream.client->GetMixFormat(&rawMix);
    if (FAILED(hr) || rawMix == nullptr) {
        error = QStringLiteral("no mix format ") + hrText(hr);
        return wasapiResultOf(hr);
    }
    const WaveFormatPtr mix(rawMix);
    stream.channels = int(mix->nChannels);
    stream.rate = int(mix->nSamplesPerSec);
    if (stream.channels <= 0 || stream.rate <= 0) {
        error = QStringLiteral("mix format has no channels or rate");
        return WasapiResult::UnsupportedFormat;
    }

    if (!exclusive) {
        const std::optional<WasapiFormatCandidate> mixCandidate = candidateOf(*mix);
        const std::optional<DeviceSampleFormat> mixFormat =
            mixCandidate ? wasapiMixSampleFormat(*mixCandidate) : std::nullopt;
        if (!mixFormat) {
            error = QStringLiteral("mix format cannot be written");
            return WasapiResult::UnsupportedFormat;
        }
        stream.format = *mixFormat;

        // R-AUD-16: IAudioClient3's low-latency shared stream.
        std::optional<WasapiEnginePeriods> periods;
        {
            WasapiComPtr<IAudioClient3> client3;
            if (SUCCEEDED(stream.client->QueryInterface(kWasapiIidAudioClient3, client3.putVoid()))
                && client3) {
                UINT32 defaultFrames = 0;
                UINT32 fundamentalFrames = 0;
                UINT32 minFrames = 0;
                UINT32 maxFrames = 0;
                if (SUCCEEDED(client3->GetSharedModeEnginePeriod(mix.get(), &defaultFrames,
                                                                 &fundamentalFrames, &minFrames,
                                                                 &maxFrames))) {
                    periods = WasapiEnginePeriods{int(defaultFrames), int(fundamentalFrames),
                                                  int(minFrames), int(maxFrames)};
                }
                const int frames = wasapiSharedPeriodFrames(periods, requestedFrames);
                if (frames > 0) {
                    hr = client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                              UINT32(frames), mix.get(), nullptr);
                    if (SUCCEEDED(hr)) {
                        stream.periodFrames = frames;
                    } else if (wasapiResultOf(hr) == WasapiResult::DeviceInUse) {
                        error = QStringLiteral("held by another program ") + hrText(hr);
                        return WasapiResult::DeviceInUse;
                    } else {
                        qCWarning(lcAudio) << "Windows audio: low-latency shared stream refused"
                                           << hrText(hr) << "; using the ordinary shared stream";
                    }
                }
            }
        }
        if (stream.periodFrames == 0) {
            // The ordinary event-driven shared stream at the engine's
            // default period, on a fresh client.
            if (!activateClient(stream.device.get(), stream.client, &hr)) {
                error = QStringLiteral("no audio client ") + hrText(hr);
                return wasapiResultOf(hr);
            }
            hr = stream.client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                           0, 0, mix.get(), nullptr);
            if (FAILED(hr)) {
                error = QStringLiteral("shared stream refused ") + hrText(hr);
                return wasapiResultOf(hr);
            }
            REFERENCE_TIME defaultPeriod = 0;
            REFERENCE_TIME minimumPeriod = 0;
            if (SUCCEEDED(stream.client->GetDevicePeriod(&defaultPeriod, &minimumPeriod))) {
                stream.periodFrames = wasapiFramesForHns(std::int64_t(defaultPeriod), stream.rate);
            }
        }
    } else {
        // R-AUD-16: exclusive, event-driven, the device's best format.
        REFERENCE_TIME defaultPeriod = 0;
        REFERENCE_TIME minimumPeriod = 0;
        hr = stream.client->GetDevicePeriod(&defaultPeriod, &minimumPeriod);
        if (FAILED(hr)) {
            error = QStringLiteral("no device period ") + hrText(hr);
            return wasapiResultOf(hr);
        }
        const DWORD channelMask = channelMaskOf(*mix);
        std::optional<WAVEFORMATEXTENSIBLE> chosen;
        for (const WasapiFormatCandidate& candidate : wasapiExclusiveFormatOrder()) {
            const WAVEFORMATEXTENSIBLE format =
                exclusiveFormat(candidate, stream.channels, stream.rate, channelMask);
            hr = stream.client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &format.Format,
                                                  nullptr);
            if (hr == S_OK) {
                chosen = format;
                stream.format = wasapiDeviceFormat(candidate);
                break;
            }
            const WasapiResult result = wasapiResultOf(hr);
            if (result == WasapiResult::DeviceInUse || result == WasapiResult::ExclusiveNotAllowed) {
                error = QStringLiteral("exclusive use refused ") + hrText(hr);
                return result;
            }
        }
        if (!chosen) {
            error = QStringLiteral("no exclusive format the device takes");
            return WasapiResult::UnsupportedFormat;
        }
        std::int64_t period = wasapiExclusivePeriodHns(std::int64_t(minimumPeriod),
                                                       requestedFrames, stream.rate);
        hr = stream.client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                       REFERENCE_TIME(period), REFERENCE_TIME(period),
                                       &chosen->Format, nullptr);
        if (wasapiResultOf(hr) == WasapiResult::BufferSizeNotAligned) {
            // Realign once: the aligned size from this client, then a new
            // client at that period.
            UINT32 alignedFrames = 0;
            hr = stream.client->GetBufferSize(&alignedFrames);
            if (FAILED(hr)) {
                error = QStringLiteral("no aligned buffer size ") + hrText(hr);
                return wasapiResultOf(hr);
            }
            period = wasapiAlignedPeriodHns(int(alignedFrames), stream.rate);
            if (!activateClient(stream.device.get(), stream.client, &hr)) {
                error = QStringLiteral("no audio client ") + hrText(hr);
                return wasapiResultOf(hr);
            }
            hr = stream.client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                           AUDCLNT_STREAMFLAGS_EVENTCALLBACK, REFERENCE_TIME(period),
                                           REFERENCE_TIME(period), &chosen->Format, nullptr);
        }
        if (FAILED(hr)) {
            error = QStringLiteral("exclusive stream refused ") + hrText(hr);
            return wasapiResultOf(hr);
        }
        stream.periodFrames = wasapiFramesForHns(period, stream.rate);
    }

    if (!stream.bufferEvent.create()) {
        error = QStringLiteral("no buffer event");
        return WasapiResult::Other;
    }
    hr = stream.client->SetEventHandle(stream.bufferEvent.get());
    if (FAILED(hr)) {
        error = QStringLiteral("buffer event refused ") + hrText(hr);
        return wasapiResultOf(hr);
    }
    UINT32 bufferFrames = 0;
    hr = stream.client->GetBufferSize(&bufferFrames);
    if (FAILED(hr) || bufferFrames == 0) {
        error = QStringLiteral("no buffer size ") + hrText(hr);
        return FAILED(hr) ? wasapiResultOf(hr) : WasapiResult::Other;
    }
    stream.bufferFrames = int(bufferFrames);
    if (stream.periodFrames <= 0 || stream.periodFrames > stream.bufferFrames) {
        stream.periodFrames = stream.bufferFrames;
    }
    REFERENCE_TIME latency = 0;
    if (SUCCEEDED(stream.client->GetStreamLatency(&latency)) && latency > 0) {
        stream.latencyNs = std::int64_t(latency) * 100;   // 100 ns units
    }
    return WasapiResult::Ok;
}

void wasapiRunInMta(const std::function<void()>& fn)
{
    std::thread worker([&fn] {
        const WasapiComScope com;
        if (com.usable()) {
            fn();
        }
    });
    worker.join();
}

} // namespace NereusSDR
