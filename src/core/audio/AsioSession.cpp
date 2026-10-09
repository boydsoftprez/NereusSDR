// =================================================================
// src/core/audio/AsioSession.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The portable ASIO session rules
// (R-AUD-19 to R-AUD-21) and the buffer switch; see AsioSession.h.  The
// order of the driver calls follows cmASIO's create_cmasio, ported with
// its notices in cmasio.cpp; nothing of Thetis's code is copied here.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-19, R-AUD-20, R-AUD-21).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AsioSession.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace NereusSDR {

std::atomic<AsioSession*> AsioSession::s_current{nullptr};

namespace {

bool pairFits(const AudioChannelPair& pair, int channels)
{
    return pair.firstChannel >= 1 && pair.channelCount >= 1
        && pair.firstChannel + pair.channelCount - 1 <= channels;
}

// The 0-based channels a pair uses.
QList<int> pairChannels(const AudioChannelPair& pair)
{
    QList<int> channels;
    for (int c = 0; c < pair.channelCount; ++c) {
        channels.append(pair.firstChannel - 1 + c);
    }
    return channels;
}

} // namespace

AsioSwitchPlan planAsioSwitch(const QList<AsioUse>& current, const AsioUse& requested,
                              const AsioDriverCaps& newDriver)
{
    AsioSwitchPlan plan;
    plan.toDriver = requested.driver;
    for (const AsioUse& use : current) {
        // The requested role is the one moving by choice; a use already on
        // the new driver stays where it is (one driver at a time, R-AUD-19).
        if (use.role == requested.role || use.driver == requested.driver) {
            continue;
        }
        const int channels = use.direction == AudioDeviceDirection::Output
                                 ? newDriver.outputChannels
                                 : newDriver.inputChannels;
        const QList<AudioChannelPair> pairs = audioChannelPairs(channels);
        AsioUse moved = use;
        moved.driver = requested.driver;
        if (pairs.contains(use.pair)) {
            moved.pair = use.pair;
        } else if (!pairs.isEmpty()) {
            moved.pair = pairs.first();
        } else {
            // The new driver has no channel in this direction: the move has
            // no pair (channelCount 0) and the role goes silent there.
            moved.pair = AudioChannelPair{1, 0};
        }
        plan.moves.append(moved);
    }
    return plan;
}

bool asioBufferSizeFixed(const AsioDriverCaps& caps)
{
    // From third_party/asiosdk/common/asio.h:646-648 (ASIOGetBufferSize):
    // equal minimum and maximum sizes leave the driver one size.
    return caps.minBufferFrames > 0 && caps.minBufferFrames == caps.maxBufferFrames;
}

std::optional<DeviceSampleFormat> asioDeviceFormat(AsioSampleType type)
{
    switch (type) {
    case AsioSampleType::Int16Lsb:
        return DeviceSampleFormat::Int16;
    case AsioSampleType::Int24Lsb:
        return DeviceSampleFormat::Int24Packed;   // ASIOSTInt24LSB is packed three bytes
    case AsioSampleType::Int32Lsb:
        return DeviceSampleFormat::Int32;
    case AsioSampleType::Float32Lsb:
        return DeviceSampleFormat::Float32;
    case AsioSampleType::Float64Lsb:
        return DeviceSampleFormat::Float64;
    case AsioSampleType::Unsupported:
        break;
    }
    return std::nullopt;
}

int asioSessionBufferFrames(const AsioDriverCaps& caps, int requestedFrames)
{
    const int minimum = caps.minBufferFrames;
    const int maximum = caps.maxBufferFrames;
    if (minimum <= 0 || maximum < minimum) {
        return requestedFrames > 0 ? requestedFrames : caps.preferredBufferFrames;
    }
    if (asioBufferSizeFixed(caps)) {
        return minimum;
    }
    int wanted = requestedFrames;
    if (wanted <= 0) {
        wanted = caps.preferredBufferFrames > 0 ? caps.preferredBufferFrames : minimum;
    }
    wanted = std::clamp(wanted, minimum, maximum);
    // From third_party/asiosdk/common/asio.h:640-645 (ASIOGetBufferSize):
    // granularity -1 means sizes from the minimum in powers of two.
    if (caps.granularity == -1) {
        int size = minimum;
        while (size < wanted && size <= maximum / 2) {
            size *= 2;
        }
        return size;
    }
    if (caps.granularity > 0) {
        const int steps = (wanted - minimum + caps.granularity - 1) / caps.granularity;
        int size = minimum + steps * caps.granularity;
        if (size > maximum) {
            size = minimum + ((maximum - minimum) / caps.granularity) * caps.granularity;
        }
        return size;
    }
    return wanted;
}

AsioSession::AsioSession(IAsioDriver& driver)
    : m_driver(driver)
    , m_notifier(std::make_unique<AsioSessionNotifier>())
{
    m_driver.setCallbacks(&AsioSession::dispatchBufferSwitch, &AsioSession::dispatchMessage);
}

AsioSession::~AsioSession()
{
    close();
}

bool AsioSession::open(const QString& driver, int bufferFrames, double rate,
                       const QList<AsioUse>& uses, const QList<AsioEndpoint>& endpoints)
{
    stopAndUnload();
    m_driverName = driver;
    m_requestedFrames = bufferFrames;
    m_requestedRate = rate;
    m_uses = uses;
    m_endpoints = endpoints;
    return start();
}

void AsioSession::close()
{
    stopAndUnload();
    m_driverName.clear();
    m_uses.clear();
    m_endpoints.clear();
    m_resetPosted.store(false, std::memory_order_release);
}

void AsioSession::onDriverMessage(AsioMessage message)
{
    switch (message) {
    case AsioMessage::ResetRequest:
    case AsioMessage::BufferSizeChange:
        // R-AUD-21, D14: the driver asks to be torn down and set up again;
        // that cannot happen inside its own call, so it is posted to the
        // session's thread, once however often the driver asks.
        if (!m_resetPosted.exchange(true, std::memory_order_acq_rel)) {
            QMetaObject::invokeMethod(m_notifier.get(), [this]() { restart(); },
                                      Qt::QueuedConnection);
        }
        return;
    case AsioMessage::ResyncRequest:
    case AsioMessage::LatenciesChanged:
        // Non-fatal: the matchers follow the clock either way.
        return;
    }
}

int AsioSession::restartCount() const
{
    return m_restarts;
}

AsioDriverCaps AsioSession::caps() const
{
    return m_caps;
}

AsioSessionNotifier* AsioSession::notifier() const
{
    return m_notifier.get();
}

bool AsioSession::isOpen() const
{
    return m_running;
}

QString AsioSession::driverName() const
{
    return m_driverName;
}

int AsioSession::bufferFrames() const
{
    return m_frames;
}

double AsioSession::sampleRate() const
{
    return m_rate;
}

AsioDriverFailure AsioSession::lastFailure() const
{
    return m_failure;
}

QString AsioSession::errorString() const
{
    return m_error;
}

void AsioSession::dispatchBufferSwitch(int bufferIndex)
{
    AsioSession* const session = s_current.load(std::memory_order_acquire);
    if (session != nullptr) {
        session->bufferSwitch(bufferIndex);
    }
}

void AsioSession::dispatchMessage(AsioMessage message)
{
    AsioSession* const session = s_current.load(std::memory_order_acquire);
    if (session != nullptr) {
        session->onDriverMessage(message);
    }
}

void AsioSession::bufferSwitch(int bufferIndex)
{
    const std::size_t half = static_cast<std::size_t>(bufferIndex & 1);
    const int frames = m_frames;
    float* const stereo = m_stereo.data();
    if (frames <= 0 || stereo == nullptr) {
        return;
    }
    for (OutputOp& op : m_outputs) {
        if (op.reader != nullptr) {
            op.reader->read(stereo, frames);
        } else {
            std::memset(stereo, 0, sizeof(float) * 2 * static_cast<std::size_t>(frames));
        }
        writeStereoToDevice(stereo, frames, nullptr, m_format, m_caps.outputChannels, op.pair,
                            false, op.planes[half].data());
    }
    if (m_inputs.empty()) {
        return;
    }
    const std::int64_t captureNs = audioProbeNowNs() - m_inputLatencyNs;
    const int rate = static_cast<int>(std::lround(m_rate));
    for (InputOp& op : m_inputs) {
        readDeviceToStereo(nullptr, op.planes[half].data(), false, m_format, m_caps.inputChannels,
                           op.pair, op.pick, frames, stereo);
        op.sink->onInput(stereo, frames, rate, captureNs);
    }
}

// cmASIO's set-up order (create_cmasio, cmasio.cpp): the driver by name,
// then its rate and buffer size, then the channels' buffers, then start.
bool AsioSession::start()
{
    m_failure = AsioDriverFailure::None;
    m_error.clear();
    if (m_driverName.isEmpty()) {
        fail(AsioDriverFailure::Failed, QStringLiteral("no ASIO driver was chosen"));
        return false;
    }
    const std::optional<AsioDriverCaps> caps = m_driver.load(m_driverName);
    if (!caps) {
        const AsioDriverFailure why = m_driver.lastFailure();
        if (why == AsioDriverFailure::InUse) {
            fail(why, QStringLiteral("the ASIO driver is in use by another program"));
        } else if (why == AsioDriverFailure::NotInstalled) {
            fail(why, QStringLiteral("the ASIO driver is not installed"));
        } else {
            fail(AsioDriverFailure::Failed, QStringLiteral("the ASIO driver did not load"));
        }
        return false;
    }
    m_loaded = true;
    m_caps = *caps;

    const std::optional<DeviceSampleFormat> format = asioDeviceFormat(m_caps.sampleType);
    if (!format) {
        fail(AsioDriverFailure::Failed, QStringLiteral("the ASIO driver's sample format is not supported"));
        stopAndUnload();
        return false;
    }
    m_format = *format;

    // R-AUD-20: one rate for the session.
    double rate = m_requestedRate > 0.0 ? m_requestedRate : m_caps.currentRate;
    if (rate <= 0.0) {
        fail(AsioDriverFailure::Failed, QStringLiteral("the ASIO driver reports no sample rate"));
        stopAndUnload();
        return false;
    }
    if (rate != m_caps.currentRate && !m_driver.setSampleRate(rate)) {
        fail(m_driver.lastFailure() == AsioDriverFailure::InUse ? AsioDriverFailure::InUse
                                                                : AsioDriverFailure::Failed,
             QStringLiteral("the ASIO driver cannot run at %1 Hz").arg(rate));
        stopAndUnload();
        return false;
    }
    m_caps.currentRate = rate;

    // R-AUD-20: one buffer size for the session.
    const int frames = asioSessionBufferFrames(m_caps, m_requestedFrames);
    if (frames <= 0) {
        fail(AsioDriverFailure::Failed, QStringLiteral("the ASIO driver reports no buffer size"));
        stopAndUnload();
        return false;
    }

    // Only the channels a use plays or records on get buffers.
    std::set<int> inputSet;
    std::set<int> outputSet;
    for (const AsioUse& use : m_uses) {
        const bool output = use.direction == AudioDeviceDirection::Output;
        const int channels = output ? m_caps.outputChannels : m_caps.inputChannels;
        if (!pairFits(use.pair, channels)) {
            qCWarning(lcAudio) << "ASIO: a use's channels" << use.pair.firstChannel << "+"
                               << use.pair.channelCount << "are not on" << m_driverName;
            continue;
        }
        for (int channel : pairChannels(use.pair)) {
            (output ? outputSet : inputSet).insert(channel);
        }
    }
    const QList<int> inputs(inputSet.begin(), inputSet.end());
    const QList<int> outputs(outputSet.begin(), outputSet.end());
    if (!m_driver.createBuffers(inputs, outputs, frames)) {
        fail(m_driver.lastFailure() == AsioDriverFailure::InUse ? AsioDriverFailure::InUse
                                                                : AsioDriverFailure::Failed,
             QStringLiteral("the ASIO driver did not create its buffers"));
        stopAndUnload();
        return false;
    }

    m_outputs.clear();
    m_inputs.clear();
    for (int i = 0; i < m_uses.size(); ++i) {
        const AsioUse& use = m_uses.at(i);
        const AsioEndpoint endpoint = i < m_endpoints.size() ? m_endpoints.at(i) : AsioEndpoint{};
        const bool output = use.direction == AudioDeviceDirection::Output;
        const int channels = output ? m_caps.outputChannels : m_caps.inputChannels;
        if (!pairFits(use.pair, channels)) {
            continue;
        }
        std::array<std::vector<void*>, 2> planes;
        for (std::size_t half = 0; half < planes.size(); ++half) {
            planes[half].assign(static_cast<std::size_t>(channels), nullptr);
            for (int channel : pairChannels(use.pair)) {
                planes[half][static_cast<std::size_t>(channel)] =
                    m_driver.buffer(use.direction, channel, static_cast<int>(half));
            }
        }
        if (output) {
            OutputOp op;
            op.reader = endpoint.reader;
            op.pair = use.pair;
            op.planes = std::move(planes);
            m_outputs.push_back(std::move(op));
        } else if (endpoint.sink != nullptr) {
            InputOp op;
            op.sink = endpoint.sink;
            op.pair = use.pair;
            op.pick = endpoint.pick;
            op.planes = std::move(planes);
            m_inputs.push_back(std::move(op));
        }
    }
    m_stereo.assign(static_cast<std::size_t>(frames) * 2, 0.0f);
    m_frames = frames;
    m_rate = rate;
    m_inputLatencyNs = static_cast<std::int64_t>(
        std::llround(1e9 * static_cast<double>(m_caps.inputLatencyFrames) / rate));

    // The one pointer the driver's callbacks reach the session through.
    s_current.store(this, std::memory_order_release);
    if (!m_driver.start()) {
        fail(AsioDriverFailure::Failed, QStringLiteral("the ASIO driver did not start"));
        stopAndUnload();
        return false;
    }
    m_running = true;
    qCInfo(lcAudio) << "ASIO: running" << m_driverName << "at" << rate << "Hz," << frames
                    << "frames," << m_outputs.size() << "outputs," << m_inputs.size() << "inputs";
    return true;
}

void AsioSession::stopAndUnload()
{
    if (m_running) {
        m_driver.stop();          // no callback runs once it returns
        m_running = false;
    }
    AsioSession* self = this;
    s_current.compare_exchange_strong(self, nullptr, std::memory_order_acq_rel);
    if (m_loaded) {
        m_driver.disposeAndUnload();
        m_loaded = false;
    }
    m_outputs.clear();
    m_inputs.clear();
    m_frames = 0;
}

// R-AUD-21, D14: a reset (or a buffer size change) restarts the session
// with the driver's new settings; ASIO's own sample stops instead.
void AsioSession::restart()
{
    m_resetPosted.store(false, std::memory_order_release);
    if (m_driverName.isEmpty() || !m_loaded) {
        return;
    }
    qCInfo(lcAudio) << "ASIO: the driver asked for a reset; restarting" << m_driverName;
    stopAndUnload();
    if (start()) {
        ++m_restarts;
        emit m_notifier->restarted();
        return;
    }
    emit m_notifier->failed(m_error);
}

void AsioSession::fail(AsioDriverFailure failure, const QString& detail)
{
    m_failure = failure;
    m_error = detail;
    qCWarning(lcAudio) << "ASIO:" << m_driverName << ":" << detail;
}

} // namespace NereusSDR
