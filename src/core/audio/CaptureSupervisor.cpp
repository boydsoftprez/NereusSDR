// =================================================================
// src/core/audio/CaptureSupervisor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Helper process supervision for
// optional microphone capture; no Thetis logic.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 36 (R-IOS-13): the remote window's
//               demand named in the log. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-08: native audio plan Task 1 (V-HW-8): ProbeEnable after Ready
//               and ProbeHit forwarding for the audio delay probe.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 7 (R-AUD-02): the saved identity
//               counts as a configuration change, and "(none)" opens no
//               microphone. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): each
//               generation makes a shared-memory region and wake and hands
//               them to the helper with AttachRing; a wake thread attaches
//               the helper's clock matcher ring to the reader and measures
//               the write-to-wake hop.  Ready needs the helper's Ready and
//               the first wake.  A Pcm record is a protocol error; the
//               device-in-use reason and the request serial are new.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-19, R-AUD-20, R-AUD-21):
//               the ASIO records.  An ASIO demand keeps the helper running
//               without a generation: mic demand going away then stops the
//               generation only.  A helper started to describe drivers
//               stops after the answer.  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-R3-36): the latest
//               helper's process id is kept after it ends
//               (lastHelperProcessId).  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: native audio plan final fix wave (R-R3-36): a hello, open
//               or stop deadline that fires before its interval has passed
//               on the steady clock waits out the rest.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureSupervisor.h"

#include "core/LogCategories.h"
#include "core/audio/AudioDelayProbe.h"
#include "core/audio/CaptureAudioBus.h"
#include "core/audio/CaptureHelperLocator.h"
#include "core/audio/CaptureProtocol.h"
#include "core/audio/CaptureShm.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/MatcherRing.h"

#include <QCoreApplication>

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutexLocker>
#include <QProcess>
#include <QRandomGenerator>
#include <QSemaphore>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace NereusSDR {

namespace {

namespace P = CaptureProtocol;
using Status = CaptureSupervisor::Status;
using State = Status::State;
using Reason = Status::Reason;

constexpr int kStderrLineChars = 512;          // one forwarded diagnostic line
constexpr int kStderrPendingBytes = 4096;      // unterminated diagnostic text kept
constexpr int kDelayWindowMs = 10'000;         // wake-hop log period while Ready
constexpr std::size_t kDelaySamplesMax = 4096; // hop samples kept per window
// The ring is sized before the helper knows the device, for the worst case
// it may open: the lowest rate and the largest device buffer (the helper's
// matcher at its real rate and buffer always fits inside).
constexpr int kRingSizingInRate = CaptureProtocol::kMinNativeRate;
constexpr int kRingSizingBurstFrames = CaptureProtocol::kMaxBufferFrames;
constexpr int kRingWriteBlockFrames = 64;      // the helper's matcher write block
constexpr int kForceKillWaitMs = 200;          // shutdown fallback after the stop bound
constexpr int kShutdownSlackMs = 300;          // wait beyond stopTimeoutMs for a clean end
constexpr int kShutdownBoundSlackMs = 500;     // shutdown() total bound beyond stopTimeoutMs

bool sameConfig(const AudioDeviceConfig& a, const AudioDeviceConfig& b)
{
    return a.deviceName == b.deviceName && a.sampleRate == b.sampleRate
        && a.channels == b.channels && a.bufferSamples == b.bufferSamples
        && a.exclusiveMode == b.exclusiveMode && a.hostApiIndex == b.hostApiIndex
        && a.driverApi == b.driverApi && a.bitDepth == b.bitDepth
        && a.eventDriven == b.eventDriven && a.bypassMixer == b.bypassMixer
        && a.manualLatencyMs == b.manualLatencyMs
        // Native audio plan Task 7: the saved identity, so "(none)" (only
        // DeviceId differs from the platform default) is a change.
        && a.engine == b.engine && a.deviceId == b.deviceId
        && a.firstChannel == b.firstChannel && a.micChannel == b.micChannel
        && a.delayMs == b.delayMs;
}

Reason reasonFromHelper(P::FailReason reason)
{
    switch (reason) {
    case P::FailReason::PermissionDenied: return Reason::PermissionDenied;
    case P::FailReason::DeviceNotFound: return Reason::DeviceNotFound;
    case P::FailReason::StartFailed: return Reason::StartFailed;
    case P::FailReason::InputLost: return Reason::InputLost;
    case P::FailReason::DeviceInUse: return Reason::DeviceInUse;
    case P::FailReason::OpenFailed:
    case P::FailReason::Internal:
    case P::FailReason::None:
        break;
    }
    return Reason::OpenFailed;
}

const char* stateName(State state)
{
    switch (state) {
    case State::Closed: return "Closed";
    case State::PreparingPermission: return "PreparingPermission";
    case State::Opening: return "Opening";
    case State::Ready: return "Ready";
    case State::Failed: return "Failed";
    case State::Stopping: return "Stopping";
    }
    return "?";
}

const char* reasonName(Reason reason)
{
    switch (reason) {
    case Reason::None: return "None";
    case Reason::PermissionDenied: return "PermissionDenied";
    case Reason::DeviceNotFound: return "DeviceNotFound";
    case Reason::OpenFailed: return "OpenFailed";
    case Reason::StartFailed: return "StartFailed";
    case Reason::InputLost: return "InputLost";
    case Reason::Timeout: return "Timeout";
    case Reason::HelperMissing: return "HelperMissing";
    case Reason::HelperDidNotStart: return "HelperDidNotStart";
    case Reason::HelperExited: return "HelperExited";
    case Reason::ProtocolError: return "ProtocolError";
    case Reason::DeviceInUse: return "DeviceInUse";
    }
    return "?";
}

// The helper's ring, checked before the reader trusts it: the header the
// helper built must describe a ring inside the region.
MatcherRingHeader* validRing(const CaptureShmRegion& region)
{
    if (region.data() == nullptr || region.size() < sizeof(MatcherRingHeader)) {
        return nullptr;
    }
    const auto* header = static_cast<const MatcherRingHeader*>(region.data());
    const std::uint32_t capacity = header->capacityFrames;
    if (header->magic != kMatcherRingMagic || header->channels != 2 || capacity == 0
        || (capacity & (capacity - 1)) != 0 || header->slewFrames >= capacity
        || matcherRingBytes(capacity, 2) > region.size()) {
        return nullptr;
    }
    return attachMatcherRing(region.data(), matcherRingBytes(capacity, 2));
}

// Wake hops the wake thread measures, read by the I/O thread each window.
struct HopLog {
    std::mutex mutex;
    std::vector<qint64> hopsNs;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Worker: lives on the supervisor's I/O thread and is the single authority for
// the helper process, the generation and the published status.  Every method
// runs on that thread; nothing here waits on the helper.
// ─────────────────────────────────────────────────────────────────────────────
class CaptureSupervisorWorker final : public QObject {
public:
    using Publish = std::function<void(const Status&)>;
    using PublishProbeHit = std::function<void(qint64)>;
    using PublishHop = std::function<void(quint32, double)>;
    using PublishAsioCaps = std::function<void(const P::AsioCapsRecord&)>;
    using PublishAsioState = std::function<void(const P::AsioState&)>;

    CaptureSupervisorWorker(const CaptureSupervisor::Options& options,
                            std::shared_ptr<CaptureAudioBus> reader,
                            std::shared_ptr<std::atomic<qint64>> helperPid,
                            std::shared_ptr<std::atomic<qint64>> lastHelperPid,
                            Publish publish, PublishProbeHit publishProbeHit,
                            PublishHop publishHop, PublishAsioCaps publishAsioCaps,
                            PublishAsioState publishAsioState)
        : m_options(options)
        , m_reader(std::move(reader))
        , m_helperPid(std::move(helperPid))
        , m_lastHelperPid(std::move(lastHelperPid))
        , m_publish(std::move(publish))
        , m_publishProbeHit(std::move(publishProbeHit))
        , m_publishHop(std::move(publishHop))
        , m_publishAsioCaps(std::move(publishAsioCaps))
        , m_publishAsioState(std::move(publishAsioState))
        , m_hops(std::make_shared<HopLog>())
        , m_helloTimer(new QTimer(this))
        , m_openTimer(new QTimer(this))
        , m_stopTimer(new QTimer(this))
        , m_delayTimer(new QTimer(this))
    {
        m_helloTimer->setSingleShot(true);
        m_openTimer->setSingleShot(true);
        m_stopTimer->setSingleShot(true);
        m_helloTimer->setTimerType(Qt::PreciseTimer);        // deadlines, not cadences
        m_openTimer->setTimerType(Qt::PreciseTimer);
        m_stopTimer->setTimerType(Qt::PreciseTimer);
        m_delayTimer->setInterval(kDelayWindowMs);
        connect(m_helloTimer, &QTimer::timeout, this, [this]() { onHelloTimeout(); });
        connect(m_openTimer, &QTimer::timeout, this, [this]() { onOpenTimeout(); });
        connect(m_stopTimer, &QTimer::timeout, this, [this]() { onStopTimeout(); });
        connect(m_delayTimer, &QTimer::timeout, this, [this]() { logDelayWindow(); });
        m_hops->hopsNs.reserve(kDelaySamplesMax);
    }

    ~CaptureSupervisorWorker() override
    {
        stopRing();
    }

    void setDemanded(bool demanded, quint64 request)
    {
        if (m_shuttingDown || demanded == m_demanded) {
            return;
        }
        m_demanded = demanded;
        if (demanded) {
            m_status.request = request;
        }
        if (demanded) {
            startGeneration();
            return;
        }
        if (!asioWanted()) {
            m_pendingSpawn = false;
        }
        const bool failed = (m_status.state == State::Failed);
        endGeneration();
        if (!m_process) {
            if (!failed) {
                publish(State::Closed, Reason::None);
            }
            return;
        }
        if (asioWanted()) {
            // Task 15: the helper keeps playing ASIO; only the microphone stops.
            stopGenerationOnly();
            if (!failed) {
                publish(State::Closed, Reason::None);
            }
            return;
        }
        beginStop();
        if (!failed) {
            publish(State::Stopping, Reason::None);
        }
    }

    void configure(const AudioDeviceConfig& config, quint64 request)
    {
        if (m_shuttingDown) {
            return;
        }
        m_config = config;
        m_status.request = request;
        m_status.configuredDevice = config.deviceName;
        if (m_demanded) {
            startGeneration();
            return;
        }
        if (m_status.state == State::Failed) {
            publish(State::Closed, Reason::None);
        } else {
            publish(m_status.state, m_status.reason);
        }
    }

    void retry(quint64 request)
    {
        if (m_shuttingDown) {
            return;
        }
        m_status.request = request;
        if (m_demanded) {
            startGeneration();
        } else if (m_status.state == State::Failed) {
            publish(State::Closed, Reason::None);
        }
    }

    // V-HW-8: enabled waits for the helper's Ready; disabled goes at once.
    void setProbeEnabled(bool enabled)
    {
        if (enabled == m_probeEnabled) {
            return;
        }
        m_probeEnabled = enabled;
        if (!enabled || m_helperReady) {
            sendProbeEnable(enabled);
        }
    }

    // ── ASIO (Task 15) ───────────────────────────────────────────────────────

    void setAsioDemanded(bool demanded)
    {
        if (m_shuttingDown || demanded == m_asioDemanded) {
            return;
        }
        m_asioDemanded = demanded;
        if (demanded) {
            ensureProcess();
            return;
        }
        m_asioOpen.reset();
        stopIdleHelper();
    }

    void describeAsio(const QString& driver)
    {
        if (m_shuttingDown) {
            return;
        }
        if (helperMissing()) {
            P::AsioCapsRecord none;
            none.driver = driver;
            m_publishAsioCaps(none);
            return;
        }
        ++m_asioDescribes;
        sendAsio(P::encodeAsioDescribe(P::AsioDescribe{driver}));
        ensureProcess();
    }

    void openAsio(const P::AsioOpen& open)
    {
        if (m_shuttingDown) {
            return;
        }
        if (open.uses.isEmpty()) {
            m_asioOpen.reset();
        } else {
            m_asioOpen = open;
        }
        if (helperMissing()) {
            if (!open.uses.isEmpty()) {
                publishAsioFailure(open.serial, QStringLiteral("the audio helper is not installed"));
            }
            return;
        }
        if (!m_process && open.uses.isEmpty()) {
            return;                              // nothing plays, nothing to close
        }
        sendAsio(P::encodeAsioOpen(open));
        ensureProcess();
    }

    void openAsioControlPanel()
    {
        if (m_shuttingDown || !m_process || m_dying) {
            return;
        }
        sendAsio(P::encodeAsioControlPanel());
    }

    // Stops everything; done() runs on this thread once no child is left.
    void beginShutdown(std::function<void()> done)
    {
        m_shuttingDown = true;
        m_demanded = false;
        m_asioDemanded = false;
        m_asioOpen.reset();
        m_pendingAsio.clear();
        m_asioDescribes = 0;
        m_pendingSpawn = false;
        endGeneration();
        if (!m_process) {
            done();
            return;
        }
        m_shutdownDone = std::move(done);
        beginStop();
        if (!m_stopTimer->isActive()) {
            m_stopClock.start();
            m_stopTimer->start(m_options.stopTimeoutMs);
        }
    }

    // Last resort when the ordinary stop did not end the child in time.
    void forceKill()
    {
        if (!m_process) {
            return;
        }
        m_dying = true;
        m_discardOutput = true;
        m_process->kill();
        m_process->waitForFinished(kForceKillWaitMs);
    }

private:
    // ── Generations ──────────────────────────────────────────────────────────

    // Retires whatever generation is current, then opens a new one.
    void startGeneration()
    {
        endGeneration();
        // Native audio plan Task 7: "(none)" opens nothing.  The demand is
        // kept; a later configure() with a device opens it.
        if (m_config.isNone()) {
            m_pendingSpawn = false;
            m_status.configuredDevice.clear();
            m_status.actualDevice.clear();
            qCInfo(lcAudio) << "capture: no microphone chosen; nothing is opened";
            if (asioWanted()) {
                stopGenerationOnly();
            } else {
                beginStop();
            }
            publish(State::Closed, Reason::None);
            return;
        }
        ++m_generation;
        if (m_generation == 0) {
            m_generation = 1;
        }
        m_open = true;
        m_status.generation = m_generation;
        m_status.configuredDevice = m_config.deviceName;
        m_status.actualDevice.clear();
        clearDeviceFacts();
        qCInfo(lcAudio) << "capture: opening generation" << m_generation << "for"
                        << (m_config.deviceName.isEmpty() ? QStringLiteral("the system default")
                                                          : m_config.deviceName);

        if (helperMissing()) {
            qCWarning(lcAudio) << "capture: helper program is not available:" << m_options.program;
            m_open = false;
            publish(State::Failed, Reason::HelperMissing);
            return;
        }
        publish(State::Opening, Reason::None);
        if (!m_process) {
            spawn();
            return;
        }
        if (m_dying) {
            m_pendingSpawn = true;              // the old child is on its way out
            return;
        }
        // Reuse the running helper: a pending Stop is superseded.
        m_awaitingStopped = false;
        m_stopTimer->stop();
        if (m_helloReceived) {
            sendConfigureOpen();
        }
    }

    // Retires the current generation: reader unavailable, its ring
    // detached and the region gone, open deadline and delay window
    // cancelled.  Records of it are dropped from here on.
    void endGeneration()
    {
        m_open = false;
        m_helperReady = false;
        m_ringWoke = false;
        m_readyPublished = false;
        m_openTimer->stop();
        m_openPaused = false;
        m_delayTimer->stop();
        m_reader->setAvailable(false);
        stopRing();
        {
            std::lock_guard<std::mutex> lock(m_hops->mutex);
            m_hops->hopsNs.clear();
        }
    }

    void clearDeviceFacts()
    {
        m_status.nativeRate = 0;
        m_status.deviceLatencyMs = 0.0;
        m_status.deviceBufferMs = 0.0;
        m_status.hopMs.reset();
    }

    // ── ASIO helpers (Task 15) ───────────────────────────────────────────────

    // The helper is wanted for ASIO: a held demand, or a request not yet
    // answered.
    bool asioWanted() const
    {
        return m_asioDemanded || m_asioDescribes > 0 || !m_pendingAsio.isEmpty();
    }

    // A running helper is kept (a Stop it was about to get is dropped);
    // otherwise one is started, or started once the old one has gone.
    void ensureProcess()
    {
        if (m_shuttingDown || helperMissing()) {
            return;
        }
        if (!m_process) {
            spawn();
            return;
        }
        if (m_dying) {
            m_pendingSpawn = true;
            return;
        }
        if (m_awaitingStopped) {
            m_awaitingStopped = false;
            m_stopTimer->stop();
        }
    }

    // Ends the microphone's generation in the helper and keeps it running.
    void stopGenerationOnly()
    {
        if (m_process && !m_dying && m_helloReceived && m_stopGenerationOpen) {
            write(P::encodeStop(P::Command{m_generation}));
        }
        m_stopGenerationOpen = false;
    }

    // Nothing holds the helper any more: it is stopped as before.
    void stopIdleHelper()
    {
        if (m_demanded || asioWanted() || !m_process || m_dying) {
            return;
        }
        beginStop();
    }

    void sendAsio(const QByteArray& record)
    {
        if (record.isEmpty()) {
            qCWarning(lcAudio) << "capture: an ASIO request could not be encoded";
            return;
        }
        if (m_process && m_helloReceived && !m_dying) {
            write(record);
            return;
        }
        m_pendingAsio.append(record);
    }

    void flushAsio()
    {
        const QList<QByteArray> pending = std::move(m_pendingAsio);
        m_pendingAsio.clear();
        for (const QByteArray& record : pending) {
            write(record);
        }
    }

    void publishAsioFailure(quint32 serial, const QString& detail)
    {
        P::AsioState state;
        state.serial = serial;
        state.state = P::AsioStateKind::Failed;
        state.detail = detail.left(P::kMaxStringChars);
        m_publishAsioState(state);
    }

    void handleAsioCaps(const QByteArray& payload)
    {
        const auto caps = P::decodeAsioCaps(payload);
        if (!caps) {
            protocolError(QStringLiteral("invalid ASIO caps"));
            return;
        }
        if (m_asioDescribes > 0) {
            --m_asioDescribes;
        }
        m_publishAsioCaps(*caps);
        stopIdleHelper();
    }

    void handleAsioState(const QByteArray& payload)
    {
        const auto state = P::decodeAsioState(payload);
        if (!state) {
            protocolError(QStringLiteral("invalid ASIO state"));
            return;
        }
        m_publishAsioState(*state);
    }

    // The helper ended: requests it never answered are dropped, and an
    // ASIO open it held is reported failed, so the outputs reopen.
    void asioAfterProcessGone()
    {
        m_asioDescribes = 0;
        m_pendingAsio.clear();
        if (m_asioOpen) {
            const quint32 serial = m_asioOpen->serial;
            m_asioOpen.reset();
            if (!m_shuttingDown) {
                publishAsioFailure(serial, QStringLiteral("the audio helper stopped"));
            }
        }
    }

    // ── Shared ring (R-AUD-17) ───────────────────────────────────────────────

    // Ends the wake thread, then lets go of the ring and the region.  The
    // helper's own mapping stays valid until it closes it.
    void stopRing()
    {
        if (m_region) {
            m_region->shutdownWake();
        }
        if (m_waiter.joinable()) {
            m_waiter.join();
        }
        m_reader->detachRing();
        if (m_region) {
            m_region->unlinkNames();
            m_region.reset();
        }
    }

    // The wake thread of one generation: the first wake attaches the ring
    // the helper built; every wake records the hop from the helper's write
    // and updates the level.  It never touches the worker's state; results
    // go to the I/O thread queued, where they are checked against the
    // current generation.
    void startWaiter(quint32 generation)
    {
        CaptureShmRegion* region = m_region.get();
        std::shared_ptr<CaptureAudioBus> reader = m_reader;
        std::shared_ptr<HopLog> hops = m_hops;
        m_waiter = std::thread([this, region, reader, hops, generation]() {
            bool attached = false;
            while (region->waitWake()) {
                if (!attached) {
                    MatcherRingHeader* ring = validRing(*region);
                    if (ring == nullptr || !reader->attachRing(ring)) {
                        QMetaObject::invokeMethod(
                            this, [this, generation]() { onRingInvalid(generation); },
                            Qt::QueuedConnection);
                        return;
                    }
                    attached = true;
                    QMetaObject::invokeMethod(
                        this, [this, generation]() { onFirstWake(generation); },
                        Qt::QueuedConnection);
                }
                const std::int64_t written = reader->lastWriteNs();
                if (written > 0) {
                    const qint64 hop = std::max<qint64>(0, audioProbeNowNs() - written);
                    std::lock_guard<std::mutex> lock(hops->mutex);
                    if (hops->hopsNs.size() < kDelaySamplesMax) {
                        hops->hopsNs.push_back(hop);
                    }
                }
                reader->noteWake();
            }
        });
    }

    void onFirstWake(quint32 generation)
    {
        if (generation != m_generation || !m_open) {
            return;
        }
        m_ringWoke = true;
        maybeReady();
    }

    void onRingInvalid(quint32 generation)
    {
        if (generation != m_generation || !m_open) {
            return;
        }
        protocolError(QStringLiteral("the shared region does not hold a valid clock matcher ring"));
    }

    // Ready: the helper's Ready and the ring's first wake, in either order.
    void maybeReady()
    {
        if (!m_open || !m_helperReady || !m_ringWoke || m_readyPublished) {
            return;
        }
        m_readyPublished = true;
        m_openTimer->stop();
        m_openPaused = false;
        m_reader->setAvailable(true);
        m_overrunsAtWindowStart = m_reader->overruns();
        m_dryRunsAtWindowStart = m_reader->dryRuns();
        m_delayTimer->start();
        publish(State::Ready, Reason::None);
    }

    void failGeneration(Reason reason, bool killChild)
    {
        qCWarning(lcAudio) << "capture: generation" << m_generation << "failed:"
                           << reasonName(reason);
        endGeneration();
        publish(State::Failed, reason);
        if (killChild) {
            killChildProcess();
        }
    }

    bool helperMissing() const
    {
        if (m_options.program.isEmpty()) {
            return true;
        }
        const QFileInfo info(m_options.program);
        return info.isAbsolute() && !info.exists();
    }

    // ── Process ──────────────────────────────────────────────────────────────

    void spawn()
    {
        m_process = new QProcess(this);
        m_records = P::RecordReader();
        m_helloReceived = false;
        m_dying = false;
        m_stderrPending.clear();
        QProcess* process = m_process;
        connect(process, &QProcess::started, this, [this, process]() {
            if (process == m_process) {
                m_helperPid->store(process->processId());
                m_lastHelperPid->store(process->processId());
            }
        });
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
            if (process == m_process) {
                onStdout();
            }
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
            if (process == m_process) {
                onStderr();
            }
        });
        connect(process, &QProcess::finished, this,
                [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (process == m_process) {
                        onFinished(exitCode, exitStatus);
                    }
                });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
            if (process == m_process && error == QProcess::FailedToStart) {
                onFailedToStart();
            }
        });
        process->setProgram(m_options.program);
        process->setArguments(m_options.arguments);
        m_helloClock.start();
        m_helloTimer->start(m_options.helloTimeoutMs);
        process->start();
        // A program that cannot start may already have been released above.
        if (process == m_process && process->processId() > 0) {
            m_helperPid->store(process->processId());
            m_lastHelperPid->store(process->processId());
        }
    }

    void killChildProcess()
    {
        m_helloTimer->stop();
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_dying = true;
            m_discardOutput = true;
            m_process->kill();
        }
    }

    // Asks the child to end: Stop for an open generation (then Shutdown once
    // it answers Stopped), otherwise Shutdown at once.  Bounded by
    // stopTimeoutMs, after which the child is killed.
    void beginStop()
    {
        m_helloTimer->stop();
        if (!m_process || m_dying) {
            return;
        }
        if (m_helloReceived && m_stopGenerationOpen) {
            write(P::encodeStop(P::Command{m_generation}));
            m_awaitingStopped = true;
        } else {
            sendShutdown();
        }
        m_stopClock.start();
        m_stopTimer->start(m_options.stopTimeoutMs);
    }

    void sendShutdown()
    {
        m_awaitingStopped = false;
        write(P::encodeShutdown());
        m_process->closeWriteChannel();
        m_dying = true;
    }

    void sendConfigureOpen()
    {
        // A reused helper: the last generation's ring goes first.
        stopRing();
        DeviceRateMatcher::Config sizing;
        sizing.inRate = kRingSizingInRate;
        sizing.outRate = P::kSampleRate;
        sizing.writeBlockFrames = kRingWriteBlockFrames;
        sizing.callbackFrames = kRingSizingBurstFrames;
        sizing.delayMs = 0;
        const std::size_t bytes = DeviceRateMatcher::ringBytes(sizing);
        const CaptureShmNames names = makeCaptureShmNames(
            QCoreApplication::applicationPid(), QRandomGenerator::global()->generate());
        if (bytes > 0 && static_cast<qint64>(bytes) <= P::kMaxRingBytes) {
            m_region = CaptureShmRegion::create(names, bytes);
        }
        if (!m_region) {
            qCWarning(lcAudio) << "capture: no shared region for the microphone ring";
            failGeneration(Reason::OpenFailed, false);
            return;
        }

        P::Configure configure;
        configure.generation = m_generation;
        configure.device = m_config;
        P::AttachRing attach;
        attach.generation = m_generation;
        attach.memory = names.memory;
        attach.wake = names.wake;
        attach.bytes = static_cast<qint64>(bytes);
        attach.inRate = kRingSizingInRate;
        const QByteArray configureRecord = P::encodeConfigure(configure);
        const QByteArray attachRecord = P::encodeAttachRing(attach);
        const QByteArray openRecord = P::encodeOpen(P::Command{m_generation});
        if (configureRecord.isEmpty() || attachRecord.isEmpty() || openRecord.isEmpty()) {
            qCWarning(lcAudio) << "capture: the device configuration cannot be sent to the helper";
            failGeneration(Reason::OpenFailed, false);
            return;
        }
        startWaiter(m_generation);
        write(configureRecord);
        write(attachRecord);
        write(openRecord);
        m_stopGenerationOpen = true;
        m_openRemainingMs = m_options.openTimeoutMs;
        m_openPaused = false;
        m_openClock.start();
        m_openTimer->start(m_openRemainingMs);
    }

    void sendProbeEnable(bool enabled)
    {
        if (!m_process || !m_helloReceived || m_dying) {
            return;
        }
        qCInfo(lcAudio) << "capture: audio delay probe" << (enabled ? "on" : "off")
                        << "for generation" << m_generation;
        write(P::encodeProbeEnable(enabled));
    }

    void write(const QByteArray& record)
    {
        if (m_process && m_process->state() == QProcess::Running) {
            m_process->write(record);
        }
    }

    // ── Deadlines ────────────────────────────────────────────────────────────

    void pauseOpenDeadline()
    {
        if (m_openPaused || !m_openTimer->isActive()) {
            return;
        }
        m_openRemainingMs = std::max<qint64>(0, m_openRemainingMs - m_openClock.elapsed());
        m_openTimer->stop();
        m_openPaused = true;
    }

    void resumeOpenDeadline()
    {
        if (!m_openPaused) {
            return;
        }
        m_openPaused = false;
        m_openClock.start();
        m_openTimer->start(static_cast<int>(m_openRemainingMs));
    }

    // A deadline timer may fire before its interval has passed on the steady
    // clock (Windows rounds timer waits to its tick).  A deadline means the
    // whole interval went by, so an early firing waits out the rest.
    static bool rearmIfEarly(QTimer* timer, const QElapsedTimer& clock, qint64 intervalMs)
    {
        const qint64 left = intervalMs - clock.elapsed();
        if (left > 0) {
            timer->start(static_cast<int>(left));
            return true;
        }
        return false;
    }

    void onHelloTimeout()
    {
        if (!m_process || m_helloReceived) {
            return;
        }
        if (rearmIfEarly(m_helloTimer, m_helloClock, m_options.helloTimeoutMs)) {
            return;
        }
        qCWarning(lcAudio) << "capture: helper sent no hello within" << m_options.helloTimeoutMs << "ms";
        if (m_open) {
            failGeneration(Reason::HelperDidNotStart, true);
        } else {
            killChildProcess();
        }
    }

    void onOpenTimeout()
    {
        if (!m_open || m_openPaused) {
            return;
        }
        if (rearmIfEarly(m_openTimer, m_openClock, m_openRemainingMs)) {
            return;
        }
        qCWarning(lcAudio) << "capture: no microphone samples within" << m_options.openTimeoutMs
                           << "ms of opening generation" << m_generation;
        failGeneration(Reason::Timeout, true);
    }

    void onStopTimeout()
    {
        if (!m_process) {
            return;
        }
        if (rearmIfEarly(m_stopTimer, m_stopClock, m_options.stopTimeoutMs)) {
            return;
        }
        qCWarning(lcAudio) << "capture: helper did not stop within" << m_options.stopTimeoutMs
                           << "ms; killing it";
        m_awaitingStopped = false;
        m_dying = true;
        m_discardOutput = true;
        m_process->kill();
    }

    // ── Child output ─────────────────────────────────────────────────────────

    void onStdout()
    {
        const QByteArray bytes = m_process->readAllStandardOutput();
        if (m_discardOutput) {
            return;
        }
        m_records.append(bytes.constData(), bytes.size());
        while (m_process && !m_discardOutput) {
            const auto record = m_records.next();
            if (!record) {
                break;
            }
            handleRecord(*record);
        }
        if (m_process && !m_discardOutput && m_records.error() != P::RecordReader::Error::None) {
            protocolError(QStringLiteral("malformed record (reader error %1)")
                              .arg(static_cast<int>(m_records.error())));
        }
    }

    void onStderr()
    {
        m_stderrPending.append(m_process->readAllStandardError());
        for (;;) {
            const qsizetype newline = m_stderrPending.indexOf('\n');
            if (newline < 0) {
                break;
            }
            forwardDiagnostic(m_stderrPending.left(newline));
            m_stderrPending.remove(0, newline + 1);
        }
        if (m_stderrPending.size() > kStderrPendingBytes) {
            forwardDiagnostic(m_stderrPending);
            m_stderrPending.clear();
        }
    }

    void forwardDiagnostic(const QByteArray& line)
    {
        QString text = QString::fromUtf8(line.left(kStderrLineChars * 4)).trimmed();
        if (text.isEmpty()) {
            return;
        }
        if (text.size() > kStderrLineChars) {
            text.truncate(kStderrLineChars);
            text.append(QStringLiteral(" [truncated]"));
        }
        qCInfo(lcAudio).noquote() << "capture helper stderr:" << text;
    }

    void handleRecord(const P::Record& record)
    {
        if (!m_helloReceived) {
            if (record.type != P::RecordType::Hello) {
                protocolError(QStringLiteral("first record was not hello"));
                return;
            }
            const auto hello = P::decodeHello(record.payload);
            if (!hello || hello->protocol != P::kVersion) {
                protocolError(QStringLiteral("unsupported hello"));
                return;
            }
            m_helloReceived = true;
            m_helloTimer->stop();
            qCInfo(lcAudio) << "capture: helper" << hello->pid << "build" << hello->build << "ready";
            if (m_open && !m_dying) {
                sendConfigureOpen();
            }
            if (!m_dying) {
                flushAsio();
            }
            return;
        }
        switch (record.type) {
        case P::RecordType::Status:
            handleStatus(record.payload);
            return;
        case P::RecordType::RingAttached:
            handleRingAttached(record.payload);
            return;
        case P::RecordType::Pcm:
            // Version 3: the audio is in the shared ring only.
            protocolError(QStringLiteral("a Pcm record, which protocol version 3 never sends"));
            return;
        case P::RecordType::ProbeHit:
            handleProbeHit(record.payload);
            return;
        case P::RecordType::AsioCaps:
            handleAsioCaps(record.payload);
            return;
        case P::RecordType::AsioState:
            handleAsioState(record.payload);
            return;
        default:
            protocolError(QStringLiteral("unexpected record type %1").arg(static_cast<int>(record.type)));
            return;
        }
    }

    void handleStatus(const QByteArray& payload)
    {
        const auto status = P::decodeStatus(payload);
        if (!status) {
            protocolError(QStringLiteral("invalid status"));
            return;
        }
        if (status->state == P::HelperState::Stopped) {
            if (m_awaitingStopped && status->generation == m_generation) {
                sendShutdown();
            }
            return;
        }
        if (status->generation != m_generation || !m_open) {
            qCDebug(lcAudio) << "capture: dropped status for generation" << status->generation
                             << "(current" << m_generation << ")";
            return;
        }
        switch (status->state) {
        case P::HelperState::Permission:
            pauseOpenDeadline();
            if (!m_readyPublished) {
                publish(State::PreparingPermission, Reason::None);
            }
            return;
        case P::HelperState::Opening:
            resumeOpenDeadline();
            if (!m_readyPublished) {
                publish(State::Opening, Reason::None);
            }
            return;
        case P::HelperState::Ready:
            resumeOpenDeadline();
            m_helperReady = true;
            m_status.actualDevice = status->actualDevice;
            m_status.nativeRate = status->nativeRate;
            m_status.deviceLatencyMs = static_cast<double>(status->latencyUs) / 1000.0;
            m_status.deviceBufferMs = status->nativeRate > 0
                ? 1000.0 * static_cast<double>(status->bufferFrames)
                      / static_cast<double>(status->nativeRate)
                : 0.0;
            qCInfo(lcAudio) << "capture: helper reports generation" << m_generation << "open on"
                            << status->actualDevice << status->nativeRate << "Hz"
                            << status->nativeChannels << "ch, latency" << status->latencyUs
                            << "us, buffer" << status->bufferFrames << "frames";
            if (m_probeEnabled) {
                sendProbeEnable(true);
            }
            maybeReady();
            return;
        case P::HelperState::Failed:
            qCWarning(lcAudio) << "capture: helper failed generation" << m_generation << ":"
                               << status->detail;
            m_stopGenerationOpen = false;
            failGeneration(reasonFromHelper(status->reason), false);
            return;
        case P::HelperState::Stopped:
            return;
        }
    }

    // The helper attached this generation's region: both names can go, so
    // nothing is left behind if either side dies from here on.
    void handleRingAttached(const QByteArray& payload)
    {
        const auto command = P::decodeCommand(payload);
        if (!command) {
            protocolError(QStringLiteral("invalid ring attached record"));
            return;
        }
        if (command->generation != m_generation || !m_open || !m_region) {
            return;
        }
        m_region->unlinkNames();
    }

    void handleProbeHit(const QByteArray& payload)
    {
        const auto captureNs = P::decodeProbeHit(payload);
        if (!captureNs) {
            protocolError(QStringLiteral("invalid probe hit"));
            return;
        }
        if (!m_probeEnabled || !m_open) {
            return;
        }
        m_publishProbeHit(static_cast<qint64>(*captureNs));
    }

    void protocolError(const QString& detail)
    {
        qCWarning(lcAudio).noquote() << "capture: protocol error from helper:" << detail;
        m_discardOutput = true;
        if (m_open) {
            failGeneration(Reason::ProtocolError, true);
        } else {
            killChildProcess();
        }
    }

    // Every kDelayWindowMs while Ready: the write-to-wake hop and the ring's
    // overruns and dry runs over the window; the median hop goes to the
    // owner's status for the delay readout.
    void logDelayWindow()
    {
        const quint64 overruns = m_reader->overruns();
        const quint64 dryRuns = m_reader->dryRuns();
        const quint64 overrunsInWindow = overruns - std::min(overruns, m_overrunsAtWindowStart);
        const quint64 dryRunsInWindow = dryRuns - std::min(dryRuns, m_dryRunsAtWindowStart);
        m_overrunsAtWindowStart = overruns;
        m_dryRunsAtWindowStart = dryRuns;
        std::vector<qint64> hops;
        {
            std::lock_guard<std::mutex> lock(m_hops->mutex);
            hops.swap(m_hops->hopsNs);
            m_hops->hopsNs.reserve(kDelaySamplesMax);
        }
        if (hops.empty()) {
            qCDebug(lcAudio) << "capture: no ring wakes in the last window;" << overrunsInWindow
                             << "overruns," << dryRunsInWindow << "dry runs";
            return;
        }
        std::sort(hops.begin(), hops.end());
        const std::size_t last = hops.size() - 1;
        const auto ms = [](qint64 ns) { return static_cast<double>(ns) / 1.0e6; };
        const double p50 = ms(hops[last * 50 / 100]);
        qCDebug(lcAudio).nospace() << "capture: ring wake hop over " << hops.size()
                                   << " wakes: p50 " << p50
                                   << " ms, p95 " << ms(hops[last * 95 / 100])
                                   << " ms, max " << ms(hops[last]) << " ms; "
                                   << overrunsInWindow << " overruns, " << dryRunsInWindow
                                   << " dry runs";
        m_status.hopMs = p50;
        m_lastPublished.hopMs = p50;
        m_publishHop(m_generation, p50);
    }

    // ── Child exit ───────────────────────────────────────────────────────────

    void onFinished(int exitCode, QProcess::ExitStatus exitStatus)
    {
        const bool expected = m_dying;
        const bool hadHello = m_helloReceived;
        qCInfo(lcAudio) << "capture: helper exited, code" << exitCode
                        << (exitStatus == QProcess::CrashExit ? "(crashed or killed)" : "")
                        << (expected ? "as requested" : "unexpectedly");
        releaseProcess();
        if (!expected && m_demanded && m_open) {
            failGeneration(hadHello ? Reason::HelperExited : Reason::HelperDidNotStart, false);
        }
        if (!m_pendingSpawn) {
            asioAfterProcessGone();
        }
        afterProcessGone();
    }

    void onFailedToStart()
    {
        qCWarning(lcAudio) << "capture: helper failed to start:" << m_process->errorString();
        releaseProcess();
        if (m_demanded && m_open) {
            failGeneration(Reason::HelperDidNotStart, false);
        }
        m_pendingAsio.clear();
        asioAfterProcessGone();
        afterProcessGone();
    }

    void releaseProcess()
    {
        m_helloTimer->stop();
        m_stopTimer->stop();
        m_helperPid->store(0);
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
        m_dying = false;
        m_discardOutput = false;
        m_helloReceived = false;
        m_awaitingStopped = false;
        m_stopGenerationOpen = false;
    }

    void afterProcessGone()
    {
        if (m_shutdownDone) {
            auto done = std::move(m_shutdownDone);
            m_shutdownDone = nullptr;
            done();
            return;
        }
        if (!m_demanded) {
            if (m_status.state != State::Failed) {
                publish(State::Closed, Reason::None);
            }
            if (!m_pendingSpawn || !asioWanted()) {
                m_pendingSpawn = false;
                return;
            }
        }
        if (m_pendingSpawn) {
            m_pendingSpawn = false;
            if (m_open || asioWanted()) {
                spawn();
            }
        }
    }

    void publish(State state, Reason reason)
    {
        m_status.state = state;
        m_status.reason = reason;
        if (state == State::Closed) {
            m_status.actualDevice.clear();
        }
        if (state != State::Ready) {
            clearDeviceFacts();
        }
        if (m_status == m_lastPublished) {
            return;
        }
        m_lastPublished = m_status;
        qCInfo(lcAudio) << "capture: status" << stateName(state) << reasonName(reason)
                        << "generation" << m_status.generation;
        m_publish(m_status);
    }

    CaptureSupervisor::Options m_options;
    std::shared_ptr<CaptureAudioBus> m_reader;
    std::shared_ptr<std::atomic<qint64>> m_helperPid;
    std::shared_ptr<std::atomic<qint64>> m_lastHelperPid;
    Publish m_publish;
    PublishProbeHit m_publishProbeHit;
    PublishHop m_publishHop;
    PublishAsioCaps m_publishAsioCaps;
    PublishAsioState m_publishAsioState;
    std::shared_ptr<HopLog> m_hops;

    QTimer* m_helloTimer;                     // Qt parent ownership
    QTimer* m_openTimer;
    QTimer* m_stopTimer;
    QTimer* m_delayTimer;

    // Demand and configuration, mirrored from the owner.
    bool m_demanded = false;
    bool m_probeEnabled = false;              // V-HW-8
    bool m_shuttingDown = false;
    AudioDeviceConfig m_config;
    std::function<void()> m_shutdownDone;

    // ASIO (Task 15).
    bool m_asioDemanded = false;
    int m_asioDescribes = 0;                  // describes not yet answered
    std::optional<P::AsioOpen> m_asioOpen;    // the uses the helper plays
    QList<QByteArray> m_pendingAsio;          // sent once the helper says hello

    // Child process.
    QProcess* m_process = nullptr;            // Qt parent ownership; deleteLater on exit
    P::RecordReader m_records;
    QByteArray m_stderrPending;
    bool m_helloReceived = false;
    bool m_dying = false;                     // Shutdown sent or killed; waiting for exit
    bool m_discardOutput = false;             // killed: remaining output is ignored
    bool m_awaitingStopped = false;
    bool m_stopGenerationOpen = false;        // the child holds an Open for m_generation
    bool m_pendingSpawn = false;

    // Generation.
    quint32 m_generation = 0;
    bool m_open = false;                      // demanded, not failed, not retired
    bool m_helperReady = false;
    bool m_ringWoke = false;                  // the ring's first wake was valid
    bool m_readyPublished = false;
    qint64 m_openRemainingMs = 0;
    bool m_openPaused = false;
    QElapsedTimer m_openClock;
    QElapsedTimer m_helloClock;               // started with m_helloTimer
    QElapsedTimer m_stopClock;                // started with m_stopTimer

    // The generation's shared ring (R-AUD-17).
    std::unique_ptr<CaptureShmRegion> m_region;
    std::thread m_waiter;

    // Wake-hop measurement window.
    quint64 m_overrunsAtWindowStart = 0;
    quint64 m_dryRunsAtWindowStart = 0;

    Status m_status;
    Status m_lastPublished;
};

// ─────────────────────────────────────────────────────────────────────────────
// Lease
// ─────────────────────────────────────────────────────────────────────────────

CaptureSupervisor::Lease::Lease() = default;

CaptureSupervisor::Lease::Lease(CaptureSupervisor* owner, quint64 id)
    : m_owner(owner), m_id(id)
{
}

CaptureSupervisor::Lease::Lease(Lease&& other) noexcept
    : m_owner(std::move(other.m_owner)), m_id(other.m_id)
{
    other.m_owner.clear();
    other.m_id = 0;
}

CaptureSupervisor::Lease& CaptureSupervisor::Lease::operator=(Lease&& other) noexcept
{
    if (this != &other) {
        release();
        m_owner = std::move(other.m_owner);
        m_id = other.m_id;
        other.m_owner.clear();
        other.m_id = 0;
    }
    return *this;
}

CaptureSupervisor::Lease::~Lease()
{
    release();
}

void CaptureSupervisor::Lease::release()
{
    if (!m_owner.isNull() && m_id != 0) {
        m_owner->releaseLease(m_id);
    }
    m_owner.clear();
    m_id = 0;
}

bool CaptureSupervisor::Lease::isActive() const
{
    return !m_owner.isNull() && m_id != 0 && m_owner->hasLease(m_id);
}

// ─────────────────────────────────────────────────────────────────────────────
// CaptureSupervisor
// ─────────────────────────────────────────────────────────────────────────────

CaptureSupervisor::CaptureSupervisor(Options options, QObject* parent)
    : QObject(parent)
    , m_options(std::move(options))
    , m_reader(std::make_shared<CaptureAudioBus>())
    , m_helperPid(std::make_shared<std::atomic<qint64>>(0))
    , m_lastHelperPid(std::make_shared<std::atomic<qint64>>(0))
{
    qRegisterMetaType<NereusSDR::CaptureSupervisor::Status>();
    qRegisterMetaType<NereusSDR::CaptureProtocol::AsioCapsRecord>();
    qRegisterMetaType<NereusSDR::CaptureProtocol::AsioState>();
    if (m_options.program.isEmpty()) {
        // The installed helper beside this executable.  An empty result
        // leaves the program unset, which demand reports as HelperMissing.
        m_options.program = locateCaptureHelper();
    }
    m_thread.setObjectName(QStringLiteral("CaptureSupervisor"));
    m_worker = std::make_unique<CaptureSupervisorWorker>(
        m_options, m_reader, m_helperPid, m_lastHelperPid, [this](const Status& status) {
            // Runs on the I/O thread; hand the status to the owner thread.
            QMetaObject::invokeMethod(this, [this, status]() { onWorkerStatus(status); },
                                      Qt::QueuedConnection);
        },
        [this](qint64 captureNs) {
            // V-HW-8: on the I/O thread; the hit is emitted on the owner thread.
            QMetaObject::invokeMethod(this, [this, captureNs]() {
                if (!m_shutDown && m_probeEnabled) {
                    emit probeHit(captureNs);
                }
            }, Qt::QueuedConnection);
        },
        [this](quint32 generation, double hopMs) {
            QMetaObject::invokeMethod(this, [this, generation, hopMs]() {
                onWorkerHop(generation, hopMs);
            }, Qt::QueuedConnection);
        },
        [this](const P::AsioCapsRecord& caps) {
            QMetaObject::invokeMethod(this, [this, caps]() {
                if (!m_shutDown) {
                    emit asioCaps(caps);
                }
            }, Qt::QueuedConnection);
        },
        [this](const P::AsioState& state) {
            QMetaObject::invokeMethod(this, [this, state]() {
                if (!m_shutDown) {
                    emit asioState(state);
                }
            }, Qt::QueuedConnection);
        });
    m_worker->moveToThread(&m_thread);
    m_thread.start();
}

CaptureSupervisor::~CaptureSupervisor()
{
    shutdown();
}

namespace {

// Who holds a capture demand, for the log.
const char* demandName(CaptureSupervisor::Demand demand)
{
    switch (demand) {
    case CaptureSupervisor::Demand::TestMic:
        return "Test Mic";
    case CaptureSupervisor::Demand::RemoteWindow:
        return "the remote window's microphone uplink";
    case CaptureSupervisor::Demand::AsioDevice:
        return "an ASIO device";
    case CaptureSupervisor::Demand::LocalSession:
        break;
    }
    return "the local session";
}

} // namespace

CaptureSupervisor::Lease CaptureSupervisor::acquire(Demand demand)
{
    if (m_shutDown) {
        return Lease();
    }
    const quint64 id = m_nextLeaseId++;
    m_leases.insert(id, demand);
    qCInfo(lcAudio) << "capture: demand acquired by" << demandName(demand)
                    << "(" << m_leases.size() << "active)";
    if (demand == Demand::AsioDevice) {
        if (asioLeaseCount() == 1) {
            CaptureSupervisorWorker* worker = m_worker.get();
            QMetaObject::invokeMethod(worker, [worker]() { worker->setAsioDemanded(true); },
                                      Qt::QueuedConnection);
        }
        return Lease(this, id);
    }
    if (micLeaseCount() == 1) {
        const quint64 request = ++m_requestSerial;
        CaptureSupervisorWorker* worker = m_worker.get();
        QMetaObject::invokeMethod(worker, [worker, request]() { worker->setDemanded(true, request); },
                                  Qt::QueuedConnection);
    }
    return Lease(this, id);
}

void CaptureSupervisor::releaseLease(quint64 id)
{
    const auto it = m_leases.constFind(id);
    if (it == m_leases.constEnd()) {
        return;
    }
    const Demand demand = it.value();
    m_leases.erase(it);
    qCInfo(lcAudio) << "capture: demand released by" << demandName(demand)
                    << "(" << m_leases.size() << "active)";
    if (m_shutDown) {
        return;
    }
    CaptureSupervisorWorker* worker = m_worker.get();
    if (demand == Demand::AsioDevice) {
        if (asioLeaseCount() == 0) {
            QMetaObject::invokeMethod(worker, [worker]() { worker->setAsioDemanded(false); },
                                      Qt::QueuedConnection);
        }
        return;
    }
    if (micLeaseCount() == 0) {
        QMetaObject::invokeMethod(worker, [worker]() { worker->setDemanded(false, 0); },
                                  Qt::QueuedConnection);
    }
}

int CaptureSupervisor::micLeaseCount() const
{
    return static_cast<int>(m_leases.size()) - asioLeaseCount();
}

int CaptureSupervisor::asioLeaseCount() const
{
    int count = 0;
    for (const Demand demand : m_leases) {
        count += demand == Demand::AsioDevice ? 1 : 0;
    }
    return count;
}

void CaptureSupervisor::describeAsio(const QString& driver)
{
    if (m_shutDown) {
        return;
    }
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker, driver]() { worker->describeAsio(driver); },
                              Qt::QueuedConnection);
}

void CaptureSupervisor::openAsio(const CaptureProtocol::AsioOpen& open)
{
    if (m_shutDown) {
        return;
    }
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker, open]() { worker->openAsio(open); },
                              Qt::QueuedConnection);
}

void CaptureSupervisor::openAsioControlPanel()
{
    if (m_shutDown) {
        return;
    }
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker]() { worker->openAsioControlPanel(); },
                              Qt::QueuedConnection);
}

bool CaptureSupervisor::hasLease(quint64 id) const
{
    return m_leases.contains(id);
}

void CaptureSupervisor::configure(const AudioDeviceConfig& config)
{
    if (m_shutDown || sameConfig(config, m_config)) {
        return;
    }
    m_config = config;
    const quint64 request = ++m_requestSerial;
    markRestarting(&config.deviceName);
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker, config, request]() {
        worker->configure(config, request);
    }, Qt::QueuedConnection);
}

void CaptureSupervisor::setProbeEnabled(bool enabled)
{
    if (m_shutDown || enabled == m_probeEnabled) {
        return;
    }
    m_probeEnabled = enabled;
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker, enabled]() { worker->setProbeEnabled(enabled); },
                              Qt::QueuedConnection);
}

void CaptureSupervisor::retry()
{
    if (m_shutDown) {
        return;
    }
    const quint64 request = ++m_requestSerial;
    markRestarting(nullptr);
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker, request]() { worker->retry(request); },
                              Qt::QueuedConnection);
}

CaptureSupervisor::Status CaptureSupervisor::status() const
{
    QMutexLocker lock(&m_statusMutex);
    return m_status;
}

CaptureAudioBus* CaptureSupervisor::reader() const
{
    return m_reader.get();
}

qint64 CaptureSupervisor::helperProcessId() const
{
    return m_helperPid->load();
}

qint64 CaptureSupervisor::lastHelperProcessId() const
{
    return m_lastHelperPid->load();
}

// R-R3-36: with demand held, configure() and retry() make the capture
// thread retire the current generation and open a new one. The owner's
// copy would say Ready until that thread's next status arrives, so it is
// marked Opening (for the next generation) here, before the restart is
// queued, and published. A Ready of the old generation still in the queue
// is dropped.
void CaptureSupervisor::markRestarting(const QString* configuredDevice)
{
    if (!hasDemand()) {
        return;
    }
    Status marked;
    {
        QMutexLocker lock(&m_statusMutex);
        if (m_status.state != Status::State::Ready) {
            return;
        }
        // The status is the next generation's: the capture thread's restart
        // opens generation + 1 and publishes this same Opening.
        m_readyGenerationFloor = m_status.generation + 1;
        m_status.generation = m_readyGenerationFloor;
        m_status.state = Status::State::Opening;
        m_status.reason = Status::Reason::None;
        m_status.actualDevice.clear();
        m_status.nativeRate = 0;
        m_status.deviceLatencyMs = 0.0;
        m_status.deviceBufferMs = 0.0;
        m_status.hopMs.reset();
        m_status.request = m_requestSerial;
        if (configuredDevice != nullptr) {
            m_status.configuredDevice = *configuredDevice;
        }
        marked = m_status;
    }
    emit statusChanged(marked);
}

void CaptureSupervisor::onWorkerStatus(const Status& status)
{
    if (m_shutDown) {
        return;
    }
    if (status.state == Status::State::Ready && status.generation < m_readyGenerationFloor) {
        return;
    }
    {
        QMutexLocker lock(&m_statusMutex);
        if (m_status == status) {
            return;
        }
        m_status = status;
    }
    emit statusChanged(status);
}

// A measurement, not a state change: the owner's copy takes it without a
// signal (Status::operator== leaves hopMs out).
void CaptureSupervisor::onWorkerHop(quint32 generation, double hopMs)
{
    QMutexLocker lock(&m_statusMutex);
    if (m_status.generation == generation && m_status.state == Status::State::Ready) {
        m_status.hopMs = hopMs;
    }
}

void CaptureSupervisor::shutdown()
{
    if (m_shutDown) {
        return;
    }
    m_shutDown = true;
    m_leases.clear();

    QElapsedTimer elapsed;
    elapsed.start();
    const int bound = m_options.stopTimeoutMs + kShutdownBoundSlackMs;
    if (m_thread.isRunning()) {
        auto done = std::make_shared<QSemaphore>();
        CaptureSupervisorWorker* worker = m_worker.get();
        QMetaObject::invokeMethod(worker, [worker, done]() {
            worker->beginShutdown([done]() { done->release(); });
        }, Qt::QueuedConnection);
        if (!done->tryAcquire(1, m_options.stopTimeoutMs + kShutdownSlackMs)) {
            qCWarning(lcAudio) << "capture: helper still running after the stop bound; forcing it";
            QMetaObject::invokeMethod(worker, [worker]() { worker->forceKill(); },
                                      Qt::BlockingQueuedConnection);
        }
        m_thread.quit();
        const qint64 left = std::max<qint64>(50, bound - elapsed.elapsed());
        if (!m_thread.wait(QDeadlineTimer(left))) {
            qCWarning(lcAudio) << "capture: I/O thread did not stop in time";
        }
    }
    if (!m_thread.isRunning()) {
        m_worker.reset();
    } else {
        // The thread is wedged; never delete its objects from here.
        static_cast<void>(m_worker.release());
    }
    m_reader->setAvailable(false);

    Status closed;
    closed.configuredDevice = m_config.deviceName;
    bool changed = false;
    {
        QMutexLocker lock(&m_statusMutex);
        closed.generation = m_status.generation;
        closed.request = m_status.request;
        changed = !(m_status == closed);
        m_status = closed;
    }
    if (changed) {
        emit statusChanged(closed);
    }
}

} // namespace NereusSDR
