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
// =================================================================

#include "core/audio/CaptureSupervisor.h"

#include "core/LogCategories.h"
#include "core/audio/CaptureAudioBus.h"
#include "core/audio/CaptureHelperLocator.h"
#include "core/audio/CaptureProtocol.h"

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutexLocker>
#include <QProcess>
#include <QSemaphore>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <functional>
#include <vector>

namespace NereusSDR {

namespace {

namespace P = CaptureProtocol;
using Status = CaptureSupervisor::Status;
using State = Status::State;
using Reason = Status::Reason;

constexpr int kStderrLineChars = 512;          // one forwarded diagnostic line
constexpr int kStderrPendingBytes = 4096;      // unterminated diagnostic text kept
constexpr int kDelayWindowMs = 10'000;         // delivery-delay log period while Ready
constexpr std::size_t kDelaySamplesMax = 4096; // delay samples kept per window
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
        && a.manualLatencyMs == b.manualLatencyMs;
}

Reason reasonFromHelper(P::FailReason reason)
{
    switch (reason) {
    case P::FailReason::PermissionDenied: return Reason::PermissionDenied;
    case P::FailReason::DeviceNotFound: return Reason::DeviceNotFound;
    case P::FailReason::StartFailed: return Reason::StartFailed;
    case P::FailReason::InputLost: return Reason::InputLost;
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
    }
    return "?";
}

qint64 steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Worker: lives on the supervisor's I/O thread and is the single authority for
// the helper process, the generation and the published status.  Every method
// runs on that thread; nothing here waits on the helper.
// ─────────────────────────────────────────────────────────────────────────────
class CaptureSupervisorWorker final : public QObject {
public:
    using Publish = std::function<void(const Status&)>;

    CaptureSupervisorWorker(const CaptureSupervisor::Options& options,
                            std::shared_ptr<CaptureAudioBus> reader,
                            std::shared_ptr<std::atomic<qint64>> helperPid,
                            Publish publish)
        : m_options(options)
        , m_reader(std::move(reader))
        , m_helperPid(std::move(helperPid))
        , m_publish(std::move(publish))
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
        m_delaysNs.reserve(kDelaySamplesMax);
    }

    void setDemanded(bool demanded)
    {
        if (m_shuttingDown || demanded == m_demanded) {
            return;
        }
        m_demanded = demanded;
        if (demanded) {
            startGeneration();
            return;
        }
        m_pendingSpawn = false;
        const bool failed = (m_status.state == State::Failed);
        endGeneration();
        if (!m_process) {
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

    void configure(const AudioDeviceConfig& config)
    {
        if (m_shuttingDown) {
            return;
        }
        m_config = config;
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

    void retry()
    {
        if (m_shuttingDown) {
            return;
        }
        if (m_demanded) {
            startGeneration();
        } else if (m_status.state == State::Failed) {
            publish(State::Closed, Reason::None);
        }
    }

    // Stops everything; done() runs on this thread once no child is left.
    void beginShutdown(std::function<void()> done)
    {
        m_shuttingDown = true;
        m_demanded = false;
        m_pendingSpawn = false;
        endGeneration();
        if (!m_process) {
            done();
            return;
        }
        m_shutdownDone = std::move(done);
        beginStop();
        if (!m_stopTimer->isActive()) {
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
        ++m_generation;
        if (m_generation == 0) {
            m_generation = 1;
        }
        m_open = true;
        m_status.generation = m_generation;
        m_status.configuredDevice = m_config.deviceName;
        m_status.actualDevice.clear();
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

    // Retires the current generation: reader unavailable and flushed, open
    // deadline and delay window cancelled.  Records of it are dropped from
    // here on.
    void endGeneration()
    {
        m_open = false;
        m_helperReady = false;
        m_firstPcm = false;
        m_nextPosition = 0;
        m_openTimer->stop();
        m_openPaused = false;
        m_delayTimer->stop();
        m_delaysNs.clear();
        m_reader->setAvailable(false);
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
        m_helloTimer->start(m_options.helloTimeoutMs);
        process->start();
        // A program that cannot start may already have been released above.
        if (process == m_process && process->processId() > 0) {
            m_helperPid->store(process->processId());
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
        P::Configure configure;
        configure.generation = m_generation;
        configure.device = m_config;
        const QByteArray configureRecord = P::encodeConfigure(configure);
        const QByteArray openRecord = P::encodeOpen(P::Command{m_generation});
        if (configureRecord.isEmpty() || openRecord.isEmpty()) {
            qCWarning(lcAudio) << "capture: the device configuration cannot be sent to the helper";
            failGeneration(Reason::OpenFailed, false);
            return;
        }
        write(configureRecord);
        write(openRecord);
        m_stopGenerationOpen = true;
        m_openRemainingMs = m_options.openTimeoutMs;
        m_openPaused = false;
        m_openClock.start();
        m_openTimer->start(m_openRemainingMs);
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

    void onHelloTimeout()
    {
        if (!m_process || m_helloReceived) {
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
        if (!m_open) {
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
            return;
        }
        switch (record.type) {
        case P::RecordType::Status:
            handleStatus(record.payload);
            return;
        case P::RecordType::Pcm:
            handlePcm(record.payload);
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
            if (!m_firstPcm) {
                publish(State::PreparingPermission, Reason::None);
            }
            return;
        case P::HelperState::Opening:
            resumeOpenDeadline();
            if (!m_firstPcm) {
                publish(State::Opening, Reason::None);
            }
            return;
        case P::HelperState::Ready:
            resumeOpenDeadline();
            m_helperReady = true;
            m_status.actualDevice = status->actualDevice;
            qCInfo(lcAudio) << "capture: helper reports generation" << m_generation << "open on"
                            << status->actualDevice << status->nativeRate << "Hz"
                            << status->nativeChannels << "ch";
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

    void handlePcm(const QByteArray& payload)
    {
        const auto pcm = P::decodePcm(payload);
        if (!pcm) {
            protocolError(QStringLiteral("invalid PCM record"));
            return;
        }
        if (pcm->generation != m_generation || !m_open || !m_helperReady) {
            ++m_staleRecords;
            return;
        }
        const auto frames = static_cast<quint64>(pcm->samples.size());
        if (m_firstPcm) {
            if (pcm->framePosition < m_nextPosition) {
                protocolError(QStringLiteral("frame position went backwards (%1 < %2)")
                                  .arg(pcm->framePosition)
                                  .arg(m_nextPosition));
                return;
            }
            if (pcm->framePosition > m_nextPosition) {
                qCInfo(lcAudio) << "capture: gap of" << (pcm->framePosition - m_nextPosition)
                                << "frames; retiring buffered samples";
                m_reader->flush();
            }
        }
        m_nextPosition = pcm->framePosition + frames;

        if (m_delaysNs.size() < kDelaySamplesMax) {
            m_delaysNs.push_back(std::max<qint64>(0, steadyNowNs() - static_cast<qint64>(pcm->sentMonotonicNs)));
        }
        m_reader->writeFrames(pcm->samples.constData(), pcm->samples.size());

        if (!m_firstPcm) {
            m_firstPcm = true;
            m_openTimer->stop();
            m_openPaused = false;
            m_reader->setAvailable(true);
            m_droppedAtWindowStart = m_reader->droppedFrames();
            m_delayTimer->start();
            publish(State::Ready, Reason::None);
        }
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

    void logDelayWindow()
    {
        const quint64 dropped = m_reader->droppedFrames();
        const quint64 droppedInWindow = dropped - m_droppedAtWindowStart;
        m_droppedAtWindowStart = dropped;
        if (m_delaysNs.empty()) {
            return;
        }
        std::sort(m_delaysNs.begin(), m_delaysNs.end());
        const std::size_t last = m_delaysNs.size() - 1;
        const auto ms = [](qint64 ns) { return static_cast<double>(ns) / 1.0e6; };
        qCDebug(lcAudio).nospace() << "capture: delivery delay over " << m_delaysNs.size()
                                   << " records: p50 " << ms(m_delaysNs[last * 50 / 100])
                                   << " ms, p95 " << ms(m_delaysNs[last * 95 / 100])
                                   << " ms, max " << ms(m_delaysNs[last]) << " ms; "
                                   << droppedInWindow << " frames dropped on a full reader, "
                                   << m_staleRecords << " stale records";
        m_delaysNs.clear();
        m_staleRecords = 0;
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
        afterProcessGone();
    }

    void onFailedToStart()
    {
        qCWarning(lcAudio) << "capture: helper failed to start:" << m_process->errorString();
        releaseProcess();
        if (m_demanded && m_open) {
            failGeneration(Reason::HelperDidNotStart, false);
        }
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
            return;
        }
        if (m_pendingSpawn) {
            m_pendingSpawn = false;
            if (m_open) {
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
    Publish m_publish;

    QTimer* m_helloTimer;                     // Qt parent ownership
    QTimer* m_openTimer;
    QTimer* m_stopTimer;
    QTimer* m_delayTimer;

    // Demand and configuration, mirrored from the owner.
    bool m_demanded = false;
    bool m_shuttingDown = false;
    AudioDeviceConfig m_config;
    std::function<void()> m_shutdownDone;

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
    bool m_firstPcm = false;
    quint64 m_nextPosition = 0;
    qint64 m_openRemainingMs = 0;
    bool m_openPaused = false;
    QElapsedTimer m_openClock;

    // Delivery-delay measurement (Task 8 evidence).
    std::vector<qint64> m_delaysNs;
    quint64 m_droppedAtWindowStart = 0;
    quint64 m_staleRecords = 0;

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
{
    qRegisterMetaType<NereusSDR::CaptureSupervisor::Status>();
    if (m_options.program.isEmpty()) {
        // The installed helper beside this executable.  An empty result
        // leaves the program unset, which demand reports as HelperMissing.
        m_options.program = locateCaptureHelper();
    }
    m_thread.setObjectName(QStringLiteral("CaptureSupervisor"));
    m_worker = std::make_unique<CaptureSupervisorWorker>(
        m_options, m_reader, m_helperPid, [this](const Status& status) {
            // Runs on the I/O thread; hand the status to the owner thread.
            QMetaObject::invokeMethod(this, [this, status]() { onWorkerStatus(status); },
                                      Qt::QueuedConnection);
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
    if (m_leases.size() == 1) {
        CaptureSupervisorWorker* worker = m_worker.get();
        QMetaObject::invokeMethod(worker, [worker]() { worker->setDemanded(true); },
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
    if (m_leases.isEmpty() && !m_shutDown) {
        CaptureSupervisorWorker* worker = m_worker.get();
        QMetaObject::invokeMethod(worker, [worker]() { worker->setDemanded(false); },
                                  Qt::QueuedConnection);
    }
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
    markRestarting(&config.deviceName);
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker, config]() { worker->configure(config); },
                              Qt::QueuedConnection);
}

void CaptureSupervisor::retry()
{
    if (m_shutDown) {
        return;
    }
    markRestarting(nullptr);
    CaptureSupervisorWorker* worker = m_worker.get();
    QMetaObject::invokeMethod(worker, [worker]() { worker->retry(); }, Qt::QueuedConnection);
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
        changed = !(m_status == closed);
        m_status = closed;
    }
    if (changed) {
        emit statusChanged(closed);
    }
}

} // namespace NereusSDR
