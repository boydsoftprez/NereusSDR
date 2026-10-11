// =================================================================
// src/core/audio/AsioDriverWin.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  IAsioDriver over the Steinberg
// ASIO SDK, in the mic helper on Windows only.  The calls follow the
// SDK's host sample (third_party/asiosdk/host/sample/hostsample.cpp);
// the facts below cite it.  See AsioDriverWin.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-01, R-AUD-21, R-AUD-22).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-AUD-19): the driver
//               list is read again on each listing while no driver is
//               loaded, so a driver installed later is seen.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AsioDriverWin.h"

#include "core/LogCategories.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"

#include <QByteArray>

#include <algorithm>
#include <cstring>
#include <memory>

// The SDK's driver loader (host/asiodrivers.cpp:42-51), which it does not
// declare in a header; the host sample declares it the same way
// (hostsample.cpp:128).
bool loadAsioDriver(char* name);
// The SDK's one driver list (host/asiodrivers.cpp:40); ASIOExit() removes
// the current driver through it.
extern AsioDrivers* asioDrivers;

namespace NereusSDR {

namespace {

// ASIODriverInfo::name and every driver name the SDK lists are 32 bytes
// (asio.h:478-486; asiodrivers.cpp getDriverNames).
constexpr int kAsioNameBytes = 32;
constexpr int kMaxListedDrivers = 32;
// The host's ASIO version, as the host sample and cmASIO set it
// (hostsample.cpp:76, Thetis cmASIO/hostsample.cpp:506).
constexpr long kHostAsioVersion = 2;
// The rates the session may ask for (AsioDriverCaps::sampleRates).
constexpr std::array<double, 6> kProbeRates{44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};

AsioSampleType sampleTypeFor(ASIOSampleType type)
{
    switch (type) {
    case ASIOSTInt16LSB:
        return AsioSampleType::Int16Lsb;
    case ASIOSTInt24LSB:
        return AsioSampleType::Int24Lsb;
    case ASIOSTInt32LSB:
        return AsioSampleType::Int32Lsb;
    case ASIOSTFloat32LSB:
        return AsioSampleType::Float32Lsb;
    case ASIOSTFloat64LSB:
        return AsioSampleType::Float64Lsb;
    default:
        return AsioSampleType::Unsupported;
    }
}

void onBufferSwitch(long doubleBufferIndex, ASIOBool /*directProcess*/)
{
    AsioDriverWin::dispatchSwitch(doubleBufferIndex);
}

ASIOTime* onBufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex,
                                 ASIOBool /*directProcess*/)
{
    AsioDriverWin::dispatchSwitch(doubleBufferIndex);
    return params;
}

// A rate the driver changed under the session is handled as a reset: the
// session restarts at the driver's new rate and buffer size (R-AUD-20,
// R-AUD-21).
void onSampleRateDidChange(ASIOSampleRate /*rate*/)
{
    AsioDriverWin::dispatchMessage(AsioMessage::ResetRequest);
}

// The selectors the host sample answers (hostsample.cpp:355-420), with a
// buffer size change taken as a reset (D14).
long onAsioMessage(long selector, long value, void* /*message*/, double* /*opt*/)
{
    switch (selector) {
    case kAsioSelectorSupported:
        return (value == kAsioResetRequest || value == kAsioEngineVersion
                || value == kAsioResyncRequest || value == kAsioLatenciesChanged
                || value == kAsioBufferSizeChange || value == kAsioSupportsTimeInfo)
                   ? 1L
                   : 0L;
    case kAsioEngineVersion:
        return kHostAsioVersion;
    case kAsioResetRequest:
        AsioDriverWin::dispatchMessage(AsioMessage::ResetRequest);
        return 1L;
    case kAsioBufferSizeChange:
        AsioDriverWin::dispatchMessage(AsioMessage::BufferSizeChange);
        return 1L;
    case kAsioResyncRequest:
        AsioDriverWin::dispatchMessage(AsioMessage::ResyncRequest);
        return 1L;
    case kAsioLatenciesChanged:
        AsioDriverWin::dispatchMessage(AsioMessage::LatenciesChanged);
        return 1L;
    case kAsioSupportsTimeInfo:
        return 1L;
    default:
        return 0L;
    }
}

ASIOCallbacks& sdkCallbacks()
{
    static ASIOCallbacks callbacks{&onBufferSwitch, &onSampleRateDidChange, &onAsioMessage,
                                   &onBufferSwitchTimeInfo};
    return callbacks;
}

} // namespace

std::atomic<AsioDriverWin*> AsioDriverWin::s_current{nullptr};

AsioDriverWin::AsioDriverWin() = default;

AsioDriverWin::~AsioDriverWin()
{
    disposeAndUnload();
    if (asioDrivers == m_list.get()) {
        asioDrivers = nullptr;
    }
}

QStringList AsioDriverWin::installedDrivers()
{
    // The SDK's list reads the registry when it is made, and initialises
    // COM on this thread when it finds a driver; its destructor balances
    // that (asiolist.cpp:203,210).  While no driver is loaded it is made
    // again, so a driver installed since is listed; the new list is made
    // before the old one goes, so COM stays initialised throughout.
    // loadAsioDriver() and ASIOExit() find it through the SDK's global.
    if (!m_loaded || !m_list) {
        std::unique_ptr<AsioDrivers> fresh = std::make_unique<AsioDrivers>();
        m_list = std::move(fresh);
    }
    asioDrivers = m_list.get();
    std::array<std::array<char, kAsioNameBytes>, kMaxListedDrivers> storage{};
    std::array<char*, kMaxListedDrivers> names{};
    for (int i = 0; i < kMaxListedDrivers; ++i) {
        names[static_cast<std::size_t>(i)] = storage[static_cast<std::size_t>(i)].data();
    }
    const long count = asioDrivers->getDriverNames(names.data(), kMaxListedDrivers);
    QStringList list;
    for (long i = 0; i < count; ++i) {
        std::array<char, kAsioNameBytes>& name = storage[static_cast<std::size_t>(i)];
        name.back() = '\0';
        list.append(QString::fromLocal8Bit(name.data()));
    }
    return list;
}

std::optional<AsioDriverCaps> AsioDriverWin::load(const QString& name)
{
    disposeAndUnload();
    m_failure = AsioDriverFailure::None;
    if (!installedDrivers().contains(name)) {
        m_failure = AsioDriverFailure::NotInstalled;
        return std::nullopt;
    }
    QByteArray local = name.toLocal8Bit();
    if (local.size() >= kAsioNameBytes) {
        m_failure = AsioDriverFailure::NotInstalled;
        return std::nullopt;
    }
    if (!loadAsioDriver(local.data())) {
        qCWarning(lcAudio) << "ASIO: could not create the driver" << name;
        m_failure = AsioDriverFailure::Failed;
        return std::nullopt;
    }
    ASIODriverInfo info{};
    info.asioVersion = kHostAsioVersion;
    info.sysRef = nullptr;   // the helper has no window, as cmASIO's prepareASIO
    if (ASIOInit(&info) != ASE_OK) {
        // The driver's object exists but will not start: how a driver
        // held by another program answers (Task 15 settled call).
        info.errorMessage[sizeof(info.errorMessage) - 1] = '\0';
        qCWarning(lcAudio) << "ASIO: ASIOInit failed for" << name << ":"
                           << QString::fromLocal8Bit(info.errorMessage);
        if (asioDrivers != nullptr) {
            asioDrivers->removeCurrentDriver();
        }
        m_failure = AsioDriverFailure::InUse;
        return std::nullopt;
    }
    m_loaded = true;

    AsioDriverCaps caps;
    caps.name = name;
    long inputs = 0;
    long outputs = 0;
    long minSize = 0;
    long maxSize = 0;
    long preferred = 0;
    long granularity = 0;
    ASIOSampleRate current = 0.0;
    if (ASIOGetChannels(&inputs, &outputs) != ASE_OK
        || ASIOGetBufferSize(&minSize, &maxSize, &preferred, &granularity) != ASE_OK
        || ASIOGetSampleRate(&current) != ASE_OK) {
        qCWarning(lcAudio) << "ASIO:" << name << "did not report its channels, buffer or rate";
        disposeAndUnload();
        m_failure = AsioDriverFailure::Failed;
        return std::nullopt;
    }
    caps.inputChannels = static_cast<int>(std::max(0L, inputs));
    caps.outputChannels = static_cast<int>(std::max(0L, outputs));
    caps.minBufferFrames = static_cast<int>(minSize);
    caps.maxBufferFrames = static_cast<int>(maxSize);
    caps.preferredBufferFrames = static_cast<int>(preferred);
    caps.granularity = static_cast<int>(granularity);
    caps.currentRate = current;
    for (double rate : kProbeRates) {
        if (ASIOCanSampleRate(rate) == ASE_OK) {
            caps.sampleRates.append(rate);
        }
    }
    // One sample type for every channel: the first output's, else the
    // first input's.
    ASIOChannelInfo channel{};
    channel.channel = 0;
    channel.isInput = caps.outputChannels > 0 ? ASIOFalse : ASIOTrue;
    caps.sampleType = (caps.outputChannels > 0 || caps.inputChannels > 0)
                              && ASIOGetChannelInfo(&channel) == ASE_OK
                          ? sampleTypeFor(channel.type)
                          : AsioSampleType::Unsupported;
    long inputLatency = 0;
    long outputLatency = 0;
    if (ASIOGetLatencies(&inputLatency, &outputLatency) == ASE_OK) {
        caps.inputLatencyFrames = std::max(0L, inputLatency);
        caps.outputLatencyFrames = std::max(0L, outputLatency);
    }
    m_inputChannels = caps.inputChannels;
    m_outputChannels = caps.outputChannels;
    return caps;
}

bool AsioDriverWin::createBuffers(const QList<int>& inputChannels,
                                  const QList<int>& outputChannels, int bufferFrames)
{
    if (!m_loaded || m_buffers || bufferFrames <= 0) {
        m_failure = AsioDriverFailure::Failed;
        return false;
    }
    std::vector<ASIOBufferInfo> infos;
    infos.reserve(static_cast<std::size_t>(inputChannels.size() + outputChannels.size()));
    for (int channel : inputChannels) {
        if (channel >= 0 && channel < m_inputChannels) {
            ASIOBufferInfo info{};
            info.isInput = ASIOTrue;
            info.channelNum = channel;
            infos.push_back(info);
        }
    }
    for (int channel : outputChannels) {
        if (channel >= 0 && channel < m_outputChannels) {
            ASIOBufferInfo info{};
            info.isInput = ASIOFalse;
            info.channelNum = channel;
            infos.push_back(info);
        }
    }
    if (infos.empty()) {
        m_failure = AsioDriverFailure::Failed;
        return false;
    }
    if (ASIOCreateBuffers(infos.data(), static_cast<long>(infos.size()), bufferFrames,
                          &sdkCallbacks()) != ASE_OK) {
        qCWarning(lcAudio) << "ASIO: ASIOCreateBuffers failed for" << bufferFrames << "frames";
        m_failure = AsioDriverFailure::Failed;
        return false;
    }
    m_buffers = true;
    // The driver may post a message (a reset) as soon as it holds our
    // callbacks, before ASIOStart.
    s_current.store(this, std::memory_order_release);
    m_inputBuffers.assign(static_cast<std::size_t>(m_inputChannels), {nullptr, nullptr});
    m_outputBuffers.assign(static_cast<std::size_t>(m_outputChannels), {nullptr, nullptr});
    for (const ASIOBufferInfo& info : infos) {
        auto& halves = info.isInput == ASIOTrue ? m_inputBuffers : m_outputBuffers;
        halves[static_cast<std::size_t>(info.channelNum)] = {info.buffers[0], info.buffers[1]};
    }
    return true;
}

bool AsioDriverWin::setSampleRate(double rate)
{
    if (!m_loaded || rate <= 0.0) {
        return false;
    }
    if (ASIOCanSampleRate(rate) != ASE_OK || ASIOSetSampleRate(rate) != ASE_OK) {
        m_failure = AsioDriverFailure::Failed;
        return false;
    }
    return true;
}

bool AsioDriverWin::start()
{
    if (!m_buffers) {
        return false;
    }
    s_current.store(this, std::memory_order_release);
    if (ASIOStart() != ASE_OK) {
        m_failure = AsioDriverFailure::Failed;
        return false;
    }
    m_running = true;
    return true;
}

void AsioDriverWin::stop()
{
    if (m_running) {
        ASIOStop();          // no buffer switch runs once it returns
        m_running = false;
    }
}

void AsioDriverWin::disposeAndUnload()
{
    stop();
    if (m_buffers) {
        ASIODisposeBuffers();
        m_buffers = false;
    }
    if (m_loaded) {
        ASIOExit();          // removes the current driver (asio.cpp:103-115)
        m_loaded = false;
    }
    AsioDriverWin* self = this;
    s_current.compare_exchange_strong(self, nullptr, std::memory_order_acq_rel);
    m_inputBuffers.clear();
    m_outputBuffers.clear();
    m_inputChannels = 0;
    m_outputChannels = 0;
}

void AsioDriverWin::openControlPanel()
{
    if (m_loaded) {
        ASIOControlPanel();
    }
}

void AsioDriverWin::setCallbacks(std::function<void(int bufferIndex)> bufferSwitch,
                                 std::function<void(AsioMessage)> message)
{
    m_bufferSwitch = std::move(bufferSwitch);
    m_message = std::move(message);
}

void* AsioDriverWin::buffer(AudioDeviceDirection direction, int channel, int bufferIndex)
{
    const auto& halves = direction == AudioDeviceDirection::Input ? m_inputBuffers : m_outputBuffers;
    if (channel < 0 || channel >= static_cast<int>(halves.size()) || bufferIndex < 0
        || bufferIndex > 1) {
        return nullptr;
    }
    return halves[static_cast<std::size_t>(channel)][static_cast<std::size_t>(bufferIndex)];
}

AsioDriverFailure AsioDriverWin::lastFailure() const
{
    return m_failure;
}

void AsioDriverWin::dispatchSwitch(long doubleBufferIndex)
{
    AsioDriverWin* const driver = s_current.load(std::memory_order_acquire);
    if (driver != nullptr && driver->m_bufferSwitch) {
        driver->m_bufferSwitch(static_cast<int>(doubleBufferIndex & 1));
    }
}

void AsioDriverWin::dispatchMessage(AsioMessage message)
{
    AsioDriverWin* const driver = s_current.load(std::memory_order_acquire);
    if (driver != nullptr && driver->m_message) {
        driver->m_message(message);
    }
}

} // namespace NereusSDR
