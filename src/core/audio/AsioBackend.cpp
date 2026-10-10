// =================================================================
// src/core/audio/AsioBackend.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AsioBackend.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 15 (R-AUD-01, R-AUD-07, R-AUD-11,
//               R-AUD-19, R-AUD-20, R-AUD-21, R-AUD-22). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 17 (R-AUD-07, R-AUD-19 to R-AUD-22):
//               each output's role in its use, the session's buffer and
//               rate, preferencesChanged(), and the control panel while
//               only the mic runs.  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-AUD-07): a running
//               session loses a bus whose pair the driver lacks.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AsioBackend.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/audio/AsioSession.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/CaptureShm.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <utility>

namespace NereusSDR {

namespace {

// The frames per buffer switch when the driver's caps are not known yet.
constexpr int kAsioUnknownCapsFrames = 256;
// The matcher's write block, as every output bus's (CoreAudioOutputBus).
constexpr int kAsioWriteBlockFrames = 64;

AsioSessionPreferences preferencesFromSettings()
{
    const AppSettings& s = AppSettings::instance();
    AsioSessionPreferences prefs;
    prefs.bufferFrames = s.value(QStringLiteral("audio/Asio/BufferFrames"), 0).toInt();
    prefs.sampleRate = s.value(QStringLiteral("audio/Asio/SampleRate"), 48000).toDouble();
    return prefs;
}

std::int64_t framesToNs(std::int64_t frames, double rate)
{
    if (rate <= 0.0 || frames <= 0) {
        return 0;
    }
    return static_cast<std::int64_t>(std::llround(1e9 * static_cast<double>(frames) / rate));
}

} // namespace

QString asioTestRunError()
{
    return QStringLiteral("ASIO drivers are not opened in a test run");
}

class AsioOutputBus;

struct AsioBackend::Shared {
    mutable std::mutex mutex;
    AsioHelperLink link;
    std::function<AsioSessionPreferences()> preferences = &preferencesFromSettings;
    std::function<void(AudioNotice)> noticeSink;

    // What the helper described.
    bool listAsked = false;
    bool listKnown = false;
    QStringList drivers;
    QHash<QString, AsioDriverCaps> caps;
    QSet<QString> inUse;
    QSet<QString> describing;      // asked, not answered

    // The session: every open output, in open order.
    QList<AsioOutputBus*> buses;
    QString sessionDriver;
    quint32 serial = 0;            // the last AsioOpen sent
    bool demanded = false;
    int runningFrames = 0;         // from the last running or restarted state
    double runningRate = 0.0;

    bool linked() const { return static_cast<bool>(link.describe); }
    // Sends the whole use list; called with the mutex held.
    void sendOpenLocked();
    void noticeLocked(std::vector<std::function<void()>>& after);
};

// ── The output bus ──────────────────────────────────────────────────────────

class AsioOutputBus final : public IAudioBus {
public:
    AsioOutputBus(std::shared_ptr<AsioBackend::Shared> shared, AudioStreamRequest request)
        : m_shared(std::move(shared))
        , m_request(std::move(request))
    {
    }

    ~AsioOutputBus() override { close(); }

    AsioOutputBus(const AsioOutputBus&) = delete;
    AsioOutputBus& operator=(const AsioOutputBus&) = delete;

    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override { return m_open.load(); }

    qint64 push(const char* data, qint64 bytes) override
    {
        if (!m_open.load() || !m_matcher || data == nullptr || bytes <= 0) {
            return 0;
        }
        const int frames = static_cast<int>(bytes / qint64(2 * sizeof(float)));
        m_matcher->write(reinterpret_cast<const float*>(data), frames, audioProbeNowNs());
        return bytes;
    }

    qint64 pull(char* /*data*/, qint64 /*maxBytes*/) override { return 0; }

    void flush() override
    {
        if (m_matcher) {
            m_matcher->requestFlush();
        }
    }

    std::optional<OutputPacing> outputPacing() const override
    {
        if (!m_open.load() || !m_matcher || m_ring == nullptr) {
            return std::nullopt;
        }
        // As CoreAudioOutputBus: what the helper's buffer switch asked
        // for, through the shared ring.
        OutputPacing pacing;
        pacing.consumedFrames = m_ring->requested.load(std::memory_order_acquire);
        pacing.queuedFrames = std::max(0, static_cast<int>(std::lround(m_matcher->fillFrames())));
        pacing.capacityFrames = static_cast<int>(m_ring->rsizeFrames.load(std::memory_order_acquire));
        pacing.callbackFrames = m_frames;
        const std::int64_t latency = m_latencyNs.load();
        if (latency > 0) {
            pacing.deviceLatencyNs = latency;
        }
        return pacing;
    }

    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override { return QStringLiteral("ASIO"); }
    AudioFormat negotiatedFormat() const override { return m_format; }
    QString errorString() const override { return m_error; }
    bool openRefusedInUse() const override { return m_refusedInUse; }

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override
    {
        std::lock_guard lock(m_shared->mutex);
        m_sink = std::move(sink);
    }

    AudioDelayParts delayParts() const override
    {
        if (!m_matcher || m_rate <= 0.0) {
            return {};
        }
        const double bufferMs = 1000.0 * static_cast<double>(m_frames) / m_rate;
        return m_matcher->delayParts(bufferMs, static_cast<double>(m_latencyNs.load()) / 1e6);
    }

    bool takesStereoMix() const override { return true; }

    std::optional<DeviceRateMatcherStats> matcherStats() const override
    {
        if (!m_matcher) {
            return std::nullopt;
        }
        return m_matcher->stats();
    }

    void restartClockMatch() override
    {
        if (m_matcher) {
            m_matcher->requestRestart();
        }
    }

    // The reader is in the helper, so a fade cannot be asked of it; the
    // bus reports faded at once (settled call, Task 15).

    // ── The backend's side; the mutex is held ──
    CaptureProtocol::AsioOpenUse useLocked() const
    {
        CaptureProtocol::AsioOpenUse use;
        // Native audio plan Task 17: the helper knows which role plays here.
        use.role = m_request.role;
        use.pair = m_request.pair;
        use.direction = AudioDeviceDirection::Output;
        use.memory = m_names.memory;
        use.wake = m_names.wake;
        use.bytes = static_cast<qint64>(m_bytes);
        return use;
    }
    const QString& driverLocked() const { return m_request.deviceId; }
    bool pairOnLocked(const AsioDriverCaps& caps) const
    {
        const int last = m_request.pair.firstChannel + m_request.pair.channelCount - 1;
        return m_request.pair.firstChannel >= 1 && last <= caps.outputChannels;
    }
    int framesLocked() const { return m_frames; }
    double rateLocked() const { return m_rate; }
    void setLatencyLocked(std::int64_t ns) { m_latencyNs.store(ns); }
    std::function<void()> eventLocked(AudioStreamEvent::Kind kind, const QString& detail) const
    {
        if (!m_sink) {
            return {};
        }
        AudioStreamEvent event;
        event.kind = kind;
        event.detail = detail;
        return [sink = m_sink, event]() { sink(event); };
    }

private:
    bool fail(const QString& error)
    {
        m_error = error;
        m_ring = nullptr;
        m_matcher.reset();
        m_region.reset();
        qCWarning(lcAudio) << "ASIO output did not open:" << error;
        return false;
    }

    std::shared_ptr<AsioBackend::Shared> m_shared;
    AudioStreamRequest m_request;
    std::function<void(const AudioStreamEvent&)> m_sink;

    std::unique_ptr<CaptureShmRegion> m_region;
    std::unique_ptr<DeviceRateMatcher> m_matcher;
    MatcherRingHeader* m_ring = nullptr;
    CaptureShmNames m_names;
    std::size_t m_bytes = 0;
    int m_frames = 0;
    double m_rate = 0.0;
    std::atomic<std::int64_t> m_latencyNs{0};
    AudioFormat m_format;
    QString m_error;
    bool m_refusedInUse = false;
    std::atomic<bool> m_open{false};
};

bool AsioOutputBus::open(const AudioFormat& format)
{
    if (m_open.load()) {
        return true;
    }
    m_error.clear();
    m_refusedInUse = false;
    if (audioDevicesBarredForTestRun()) {
        m_error = asioTestRunError();
        return false;
    }
    const QString& driver = m_request.deviceId;
    std::vector<std::function<void()>> after;
    {
        std::lock_guard lock(m_shared->mutex);
        if (!m_shared->linked()) {
            return fail(QStringLiteral("The ASIO helper is not available"));
        }
        if (driver.isEmpty()) {
            return fail(QStringLiteral("No ASIO driver is chosen"));
        }
        if (m_shared->inUse.contains(driver)) {
            m_refusedInUse = true;
            return fail(QStringLiteral("Another program is using %1").arg(driver));
        }
        // R-AUD-19: one driver at a time.
        if (!m_shared->buses.isEmpty() && m_shared->sessionDriver != driver) {
            return fail(QStringLiteral("%1 is in use by NereusSDR; one ASIO driver at a time")
                            .arg(m_shared->sessionDriver));
        }
        // R-AUD-20: one buffer and one rate for the whole session.  A
        // session already running gives its own; else the saved values,
        // inside the driver's caps when they are known.
        const AsioSessionPreferences prefs = m_shared->preferences
                                                 ? m_shared->preferences()
                                                 : AsioSessionPreferences{};
        const auto known = m_shared->caps.constFind(driver);
        if (!m_shared->buses.isEmpty() && m_shared->runningFrames > 0) {
            m_frames = m_shared->runningFrames;
            m_rate = m_shared->runningRate;
        } else if (known != m_shared->caps.constEnd()) {
            m_frames = asioSessionBufferFrames(*known, prefs.bufferFrames);
            m_rate = known->sampleRates.contains(prefs.sampleRate) ? prefs.sampleRate
                                                                     : known->currentRate;
        } else {
            m_frames = prefs.bufferFrames > 0 ? prefs.bufferFrames : kAsioUnknownCapsFrames;
            m_rate = prefs.sampleRate;
        }
        if (known != m_shared->caps.constEnd() && !pairOnLocked(*known)) {
            return fail(QStringLiteral("The chosen channels are not on %1").arg(driver));
        }
        if (m_frames <= 0 || m_rate <= 0.0) {
            return fail(QStringLiteral("%1 did not report a buffer size and rate").arg(driver));
        }

        DeviceRateMatcher::Config config;
        config.inRate = 48000;
        config.outRate = static_cast<int>(std::lround(m_rate));
        config.writeBlockFrames = kAsioWriteBlockFrames;
        config.callbackFrames = m_frames;
        config.delayMs = std::max(0, m_request.delayMs);
        m_bytes = DeviceRateMatcher::ringBytes(config);
        if (m_bytes == 0 || static_cast<qint64>(m_bytes) > CaptureProtocol::kMaxRingBytes) {
            return fail(QStringLiteral("Clock matcher cannot run at %1 Hz").arg(config.outRate));
        }
        m_names = makeCaptureShmNames(QCoreApplication::applicationPid(),
                                      QRandomGenerator::global()->generate());
        m_region = CaptureShmRegion::create(m_names, m_bytes);
        if (!m_region) {
            return fail(QStringLiteral("No shared memory for the ASIO output"));
        }
        m_matcher = std::make_unique<DeviceRateMatcher>(config, m_region->data(), m_bytes);
        if (!m_matcher->valid()) {
            return fail(QStringLiteral("Clock matcher cannot run at %1 Hz").arg(config.outRate));
        }
        m_ring = m_matcher->ring();

        m_shared->sessionDriver = driver;
        m_shared->buses.append(this);
        m_shared->sendOpenLocked();
        if (!m_shared->demanded) {
            m_shared->demanded = true;
            if (m_shared->link.setDemanded) {
                m_shared->link.setDemanded(true);
            }
        }
    }
    m_format = format;
    m_open.store(true);
    return true;
}

void AsioOutputBus::close()
{
    {
        std::lock_guard lock(m_shared->mutex);
        if (m_shared->buses.removeOne(this)) {
            if (m_shared->buses.isEmpty()) {
                m_shared->sessionDriver.clear();
                m_shared->runningFrames = 0;
                m_shared->runningRate = 0.0;
            }
            if (m_shared->linked()) {
                m_shared->sendOpenLocked();
            }
            if (m_shared->buses.isEmpty() && m_shared->demanded) {
                m_shared->demanded = false;
                if (m_shared->link.setDemanded) {
                    m_shared->link.setDemanded(false);
                }
            }
        }
        m_sink = nullptr;
    }
    m_open.store(false);
    m_ring = nullptr;
    m_matcher.reset();
    m_region.reset();
}

// ── The backend ─────────────────────────────────────────────────────────────

void AsioBackend::Shared::sendOpenLocked()
{
    CaptureProtocol::AsioOpen open;
    open.serial = ++serial;
    if (serial == 0) {
        open.serial = serial = 1;
    }
    if (!buses.isEmpty()) {
        open.driver = sessionDriver;
        // The latest open's values (R-AUD-20).
        open.bufferFrames = buses.last()->framesLocked();
        open.rate = buses.last()->rateLocked();
        for (const AsioOutputBus* bus : buses) {
            open.uses.append(bus->useLocked());
        }
    }
    if (link.open) {
        link.open(open);
    }
}

void AsioBackend::Shared::noticeLocked(std::vector<std::function<void()>>& after)
{
    if (noticeSink) {
        after.push_back([sink = noticeSink]() { sink(AudioNotice::DevicesChanged); });
    }
}

AsioBackend::AsioBackend()
    : m_shared(std::make_shared<Shared>())
{
}

AsioBackend::~AsioBackend()
{
    std::lock_guard lock(m_shared->mutex);
    m_shared->link = AsioHelperLink{};
    m_shared->noticeSink = nullptr;
}

void AsioBackend::setHelperLink(AsioHelperLink link)
{
    std::vector<std::function<void()>> after;
    {
        std::lock_guard lock(m_shared->mutex);
        const bool wasLinked = m_shared->linked();
        m_shared->link = std::move(link);
        m_shared->listAsked = false;
        m_shared->describing.clear();
        if (wasLinked && !m_shared->linked()) {
            m_shared->demanded = false;
            for (const AsioOutputBus* bus : m_shared->buses) {
                if (auto event = bus->eventLocked(AudioStreamEvent::Kind::DeviceLost,
                                                  QStringLiteral("The ASIO helper went away"))) {
                    after.push_back(std::move(event));
                }
            }
        }
        m_shared->noticeLocked(after);
    }
    for (const auto& call : after) {
        call();
    }
}

void AsioBackend::setPreferencesSource(std::function<AsioSessionPreferences()> source)
{
    std::lock_guard lock(m_shared->mutex);
    m_shared->preferences = std::move(source);
}

void AsioBackend::onAsioCaps(const CaptureProtocol::AsioCapsRecord& record)
{
    std::vector<std::function<void()>> after;
    {
        std::lock_guard lock(m_shared->mutex);
        bool changed = false;
        if (!m_shared->listKnown || m_shared->drivers != record.drivers) {
            changed = true;
            m_shared->drivers = record.drivers;
            m_shared->listKnown = true;
        }
        if (!record.driver.isEmpty()) {
            m_shared->describing.remove(record.driver);
            const bool wasInUse = m_shared->inUse.contains(record.driver);
            if (record.inUse != wasInUse) {
                changed = true;
                if (record.inUse) {
                    m_shared->inUse.insert(record.driver);
                } else {
                    m_shared->inUse.remove(record.driver);
                }
            }
            if (record.caps) {
                const auto it = m_shared->caps.constFind(record.driver);
                if (it == m_shared->caps.constEnd() || !(*it == *record.caps)) {
                    changed = true;
                    m_shared->caps.insert(record.driver, *record.caps);
                }
            }
        }
        if (changed) {
            m_shared->noticeLocked(after);
        }
    }
    for (const auto& call : after) {
        call();
    }
}

void AsioBackend::onAsioState(const CaptureProtocol::AsioState& state)
{
    using Kind = CaptureProtocol::AsioStateKind;
    std::vector<std::function<void()>> after;
    {
        std::lock_guard lock(m_shared->mutex);
        if (state.serial != m_shared->serial) {
            return;   // an answer to an open since replaced
        }
        switch (state.state) {
        case Kind::Running:
        case Kind::Restarted: {
            m_shared->runningFrames = state.bufferFrames;
            m_shared->runningRate = state.rate;
            const bool wasInUse = m_shared->inUse.remove(state.driver);
            const std::int64_t latency = framesToNs(state.outputLatencyFrames, state.rate);
            const auto known = m_shared->caps.constFind(state.driver);
            for (AsioOutputBus* bus : m_shared->buses) {
                // A bus opened before the driver's caps were known, on a
                // pair the driver lacks: the helper dropped it, so it plays
                // nothing.  It is lost (and its reopen is refused with the
                // reason) rather than silent with no status.
                if (known != m_shared->caps.constEnd() && !bus->pairOnLocked(*known)) {
                    if (auto event = bus->eventLocked(
                            AudioStreamEvent::Kind::DeviceLost,
                            QStringLiteral("The chosen channels are not on %1").arg(state.driver))) {
                        after.push_back(std::move(event));
                    }
                    continue;
                }
                bus->setLatencyLocked(latency);
                // A buffer or rate the bus's matcher was not built for: it
                // reopens with the session's (R-AUD-20, R-AUD-21).
                if (bus->framesLocked() != state.bufferFrames || bus->rateLocked() != state.rate) {
                    if (auto event = bus->eventLocked(AudioStreamEvent::Kind::FormatChanged,
                                                      state.detail)) {
                        after.push_back(std::move(event));
                    }
                }
            }
            if (wasInUse) {
                m_shared->noticeLocked(after);
            }
            break;
        }
        case Kind::InUse:
            // R-AUD-11: every role on the driver reads in use.
            m_shared->inUse.insert(state.driver);
            for (const AsioOutputBus* bus : m_shared->buses) {
                if (bus->driverLocked() == state.driver) {
                    if (auto event = bus->eventLocked(AudioStreamEvent::Kind::DeviceBusy,
                                                      state.detail)) {
                        after.push_back(std::move(event));
                    }
                }
            }
            m_shared->noticeLocked(after);
            break;
        case Kind::Failed:
            for (const AsioOutputBus* bus : m_shared->buses) {
                if (auto event = bus->eventLocked(AudioStreamEvent::Kind::DeviceLost, state.detail)) {
                    after.push_back(std::move(event));
                }
            }
            break;
        case Kind::Closed:
            break;
        }
    }
    for (const auto& call : after) {
        call();
    }
}

bool AsioBackend::running() const
{
    if (audioDevicesBarredForTestRun()) {
        return false;
    }
    std::lock_guard lock(m_shared->mutex);
    return m_shared->linked();
}

QList<AudioDeviceInfo> AsioBackend::enumerate()
{
    if (audioDevicesBarredForTestRun()) {
        return {};
    }
    std::lock_guard lock(m_shared->mutex);
    if (!m_shared->linked()) {
        return {};
    }
    // Each driver is described once (settled call, Task 15); the answers
    // come back through onAsioCaps, which posts DevicesChanged.
    if (!m_shared->listAsked) {
        m_shared->listAsked = true;
        m_shared->link.describe(QString());
    }
    QList<AudioDeviceInfo> devices;
    for (const QString& driver : std::as_const(m_shared->drivers)) {
        const auto known = m_shared->caps.constFind(driver);
        if (known == m_shared->caps.constEnd() && !m_shared->describing.contains(driver)) {
            m_shared->describing.insert(driver);
            m_shared->link.describe(driver);
        }
        const AudioDeviceState state = m_shared->inUse.contains(driver)
                                           ? AudioDeviceState::InUse
                                           : AudioDeviceState::Present;
        const AudioDeviceDirection directions[] = {AudioDeviceDirection::Output,
                                                   AudioDeviceDirection::Input};
        for (const AudioDeviceDirection direction : directions) {
            int channels = 2;
            if (known != m_shared->caps.constEnd()) {
                channels = direction == AudioDeviceDirection::Output ? known->outputChannels
                                                                     : known->inputChannels;
                if (channels <= 0) {
                    continue;
                }
            }
            AudioDeviceInfo info;
            info.backend = AudioBackendId::Asio;
            info.direction = direction;
            info.id = driver;
            info.name = driver;
            info.state = state;
            info.channelCount = channels;
            devices.append(info);
        }
    }
    return devices;
}

std::optional<QString> AsioBackend::defaultDeviceId(AudioDeviceDirection /*direction*/)
{
    return std::nullopt;   // ASIO has no system default
}

void AsioBackend::setNoticeSink(std::function<void(AudioNotice)> sink)
{
    std::lock_guard lock(m_shared->mutex);
    m_shared->noticeSink = std::move(sink);
}

std::unique_ptr<IAudioBus> AsioBackend::createOutput(const AudioStreamRequest& request)
{
    return std::make_unique<AsioOutputBus>(m_shared, request);
}

std::unique_ptr<IAudioInputStream> AsioBackend::createInput(const AudioStreamRequest& /*request*/,
                                                            MicChannelPick /*pick*/,
                                                            IAudioInputSink* /*sink*/)
{
    // The microphone on ASIO is opened by the helper's session through the
    // CaptureSupervisor (Task 13's path), not here.
    return nullptr;
}

void AsioBackend::openControlPanel(const QString& deviceId)
{
    std::lock_guard lock(m_shared->mutex);
    // R-AUD-22: the panel of the driver the helper has loaded.  With no
    // output open the helper may still run the driver for the mic; it
    // opens the panel only for a running session (Task 17).
    if (m_shared->linked() && m_shared->link.openControlPanel && !deviceId.isEmpty()
        && (deviceId == m_shared->sessionDriver || m_shared->sessionDriver.isEmpty())) {
        m_shared->link.openControlPanel();
    }
}

void AsioBackend::rescan()
{
    std::lock_guard lock(m_shared->mutex);
    m_shared->listAsked = false;
    m_shared->listKnown = false;
    m_shared->caps.clear();
    m_shared->inUse.clear();
    m_shared->describing.clear();
}

QStringList AsioBackend::drivers() const
{
    std::lock_guard lock(m_shared->mutex);
    return m_shared->drivers;
}

std::optional<AsioDriverCaps> AsioBackend::driverCaps(const QString& driver) const
{
    std::lock_guard lock(m_shared->mutex);
    const auto it = m_shared->caps.constFind(driver);
    if (it == m_shared->caps.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

bool AsioBackend::driverInUse(const QString& driver) const
{
    std::lock_guard lock(m_shared->mutex);
    return m_shared->inUse.contains(driver);
}

QString AsioBackend::sessionDriver() const
{
    std::lock_guard lock(m_shared->mutex);
    return m_shared->sessionDriver;
}

int AsioBackend::sessionBufferFrames() const
{
    std::lock_guard lock(m_shared->mutex);
    return m_shared->buses.isEmpty() ? 0 : m_shared->runningFrames;
}

double AsioBackend::sessionRate() const
{
    std::lock_guard lock(m_shared->mutex);
    return m_shared->buses.isEmpty() ? 0.0 : m_shared->runningRate;
}

void AsioBackend::preferencesChanged()
{
    std::lock_guard lock(m_shared->mutex);
    // R-AUD-20: the next open takes the saved buffer and rate, not the
    // running session's.
    m_shared->runningFrames = 0;
    m_shared->runningRate = 0.0;
}

} // namespace NereusSDR
