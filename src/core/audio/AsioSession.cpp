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
//   2026-10-09: native audio plan final fix wave (R-AUD-07, R-AUD-21): the
//               buffer switch adds every output on a channel into one mix;
//               a reset restarts at the driver's new buffer size and rate;
//               an input pair on the last input narrows to one channel;
//               s_current is stored before createBuffers.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
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

// A one-channel pair gets left plus right, halved, as writeStereoToDevice
// folds it (DeviceSampleFormat.cpp).
constexpr float kMonoFold = 0.5f;

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

// The channels a use runs on with this driver (R-AUD-07): an input pair
// that starts on the driver's last input is that one channel, as the
// native engines open a mic there; nullopt when the channels are not on
// the driver.
std::optional<AudioChannelPair> pairOnDriver(const AsioUse& use, const AsioDriverCaps& caps)
{
    const bool output = use.direction == AudioDeviceDirection::Output;
    const int channels = output ? caps.outputChannels : caps.inputChannels;
    AudioChannelPair pair = use.pair;
    if (!output && pair.channelCount > 1 && pair.firstChannel == channels) {
        pair.channelCount = 1;
    }
    if (!pairFits(pair, channels)) {
        return std::nullopt;
    }
    return pair;
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

std::optional<AudioChannelPair> AsioSession::runningPair(int index) const
{
    if (!m_running || index < 0 || index >= m_runningPairs.size()) {
        return std::nullopt;
    }
    return m_runningPairs.at(index);
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
    // A driver may call before the ops are complete (or after they go):
    // nothing runs then.
    if (!m_switchReady.load(std::memory_order_acquire)) {
        return;
    }
    const std::size_t half = static_cast<std::size_t>(bufferIndex & 1);
    const int frames = m_frames;
    float* const stereo = m_stereo.data();
    if (frames <= 0 || stereo == nullptr) {
        return;
    }
    // R-AUD-07: every output channel in use starts at silence and each use
    // on it adds in, so speakers and headphones on one pair play together.
    // The mix is sized at start: no allocation here.
    const std::size_t span = static_cast<std::size_t>(frames);
    std::fill(m_mix.begin(), m_mix.end(), 0.0f);
    for (const OutputOp& op : m_outputs) {
        if (op.reader == nullptr || op.leftSlot < 0) {
            continue;                    // plays silence
        }
        op.reader->read(stereo, frames);
        float* const left = m_mix.data() + static_cast<std::size_t>(op.leftSlot) * span;
        if (op.rightSlot < 0) {
            for (int f = 0; f < frames; ++f) {
                left[f] += (stereo[2 * f] + stereo[2 * f + 1]) * kMonoFold;
            }
        } else {
            float* const right = m_mix.data() + static_cast<std::size_t>(op.rightSlot) * span;
            for (int f = 0; f < frames; ++f) {
                left[f] += stereo[2 * f];
                right[f] += stereo[2 * f + 1];
            }
        }
    }
    // Each channel's mix goes to the driver once, as a one-channel pair
    // with the same value on both sides (the fold gives the value back).
    for (std::size_t slot = 0; slot < m_mixChannels.size(); ++slot) {
        const MixChannel& channel = m_mixChannels[slot];
        const float* const mix = m_mix.data() + slot * span;
        for (int f = 0; f < frames; ++f) {
            stereo[2 * f] = mix[f];
            stereo[2 * f + 1] = mix[f];
        }
        writeStereoToDevice(stereo, frames, nullptr, m_format, m_caps.outputChannels,
                            AudioChannelPair{channel.channel + 1, 1}, false,
                            channel.planes[half].data());
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
    m_runningPairs.clear();
    for (const AsioUse& use : m_uses) {
        const bool output = use.direction == AudioDeviceDirection::Output;
        const std::optional<AudioChannelPair> pair = pairOnDriver(use, m_caps);
        m_runningPairs.append(pair);
        if (!pair) {
            qCWarning(lcAudio) << "ASIO: a use's channels" << use.pair.firstChannel << "+"
                               << use.pair.channelCount << "are not on" << m_driverName;
            continue;
        }
        for (int channel : pairChannels(*pair)) {
            (output ? outputSet : inputSet).insert(channel);
        }
    }
    const QList<int> inputs(inputSet.begin(), inputSet.end());
    const QList<int> outputs(outputSet.begin(), outputSet.end());
    // The one pointer the driver's callbacks reach the session through,
    // stored before the buffers: a driver may post a reset from inside
    // ASIOCreateBuffers (settled call 13).  The buffer switch waits for
    // m_switchReady.
    s_current.store(this, std::memory_order_release);
    if (!m_driver.createBuffers(inputs, outputs, frames)) {
        fail(m_driver.lastFailure() == AsioDriverFailure::InUse ? AsioDriverFailure::InUse
                                                                : AsioDriverFailure::Failed,
             QStringLiteral("the ASIO driver did not create its buffers"));
        stopAndUnload();
        return false;
    }

    m_outputs.clear();
    m_inputs.clear();
    m_mixChannels.clear();
    // Each output channel in use gets one mix slot, shared by every use on it.
    std::vector<int> slotOf(static_cast<std::size_t>(std::max(0, m_caps.outputChannels)), -1);
    const auto slotFor = [this, &slotOf](int channel) {
        int& slot = slotOf[static_cast<std::size_t>(channel)];
        if (slot < 0) {
            MixChannel mix;
            mix.channel = channel;
            for (std::size_t half = 0; half < mix.planes.size(); ++half) {
                mix.planes[half].assign(slotOf.size(), nullptr);
                mix.planes[half][static_cast<std::size_t>(channel)] =
                    m_driver.buffer(AudioDeviceDirection::Output, channel, static_cast<int>(half));
            }
            slot = static_cast<int>(m_mixChannels.size());
            m_mixChannels.push_back(std::move(mix));
        }
        return slot;
    };
    for (int i = 0; i < m_uses.size(); ++i) {
        const AsioUse& use = m_uses.at(i);
        const AsioEndpoint endpoint = i < m_endpoints.size() ? m_endpoints.at(i) : AsioEndpoint{};
        const std::optional<AudioChannelPair> pair = m_runningPairs.at(i);
        if (!pair) {
            continue;
        }
        if (use.direction == AudioDeviceDirection::Output) {
            OutputOp op;
            op.reader = endpoint.reader;
            op.leftSlot = slotFor(pair->firstChannel - 1);
            const bool mono = m_caps.outputChannels == 1 || pair->channelCount == 1;
            op.rightSlot = mono ? -1 : slotFor(pair->firstChannel);
            m_outputs.push_back(op);
        } else if (endpoint.sink != nullptr) {
            InputOp op;
            op.sink = endpoint.sink;
            op.pair = *pair;
            op.pick = endpoint.pick;
            for (std::size_t half = 0; half < op.planes.size(); ++half) {
                op.planes[half].assign(static_cast<std::size_t>(m_caps.inputChannels), nullptr);
                for (int channel : pairChannels(*pair)) {
                    op.planes[half][static_cast<std::size_t>(channel)] =
                        m_driver.buffer(use.direction, channel, static_cast<int>(half));
                }
            }
            m_inputs.push_back(std::move(op));
        }
    }
    m_mix.assign(m_mixChannels.size() * static_cast<std::size_t>(frames), 0.0f);
    m_stereo.assign(static_cast<std::size_t>(frames) * 2, 0.0f);
    m_frames = frames;
    m_rate = rate;
    m_inputLatencyNs = static_cast<std::int64_t>(
        std::llround(1e9 * static_cast<double>(m_caps.inputLatencyFrames) / rate));

    m_switchReady.store(true, std::memory_order_release);
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
    // A switch already past the check finishes before stop() returns.
    m_switchReady.store(false, std::memory_order_release);
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
    m_mixChannels.clear();
    m_frames = 0;
}

// R-AUD-21, D14: a reset (or a buffer size change) restarts the session
// with the driver's new settings; ASIO's own sample stops instead.  The
// driver's new preferred buffer size and its current rate are the
// session's from here (they replace the latest open's request), so a size
// or rate set in the driver's control panel stands; the window saves them.
void AsioSession::restart()
{
    m_resetPosted.store(false, std::memory_order_release);
    if (m_driverName.isEmpty() || !m_loaded) {
        return;
    }
    qCInfo(lcAudio) << "ASIO: the driver asked for a reset; restarting" << m_driverName
                    << "with its own buffer size and rate";
    stopAndUnload();
    m_requestedFrames = 0;     // the driver's preferred size
    m_requestedRate = 0.0;     // the driver's current rate
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
