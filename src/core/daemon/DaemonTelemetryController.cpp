// no-port-check: NereusSDR-original. See DaemonTelemetryController.h.

#include "core/daemon/DaemonTelemetryController.h"

#include "core/HermesLiteBandwidthMonitor.h"
#include "core/RadioConnection.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace NereusSDR {
namespace {

std::optional<double> rate(std::uint64_t current, std::uint64_t previous,
                           qint64 elapsedMs)
{
    if (elapsedMs <= 0 || current < previous) {
        return std::nullopt;
    }
    return static_cast<double>(current - previous) * 1000.0
        / static_cast<double>(elapsedMs);
}

} // namespace

DaemonTelemetryController::DaemonTelemetryController(
    StationServer* server, RadioModel* radioModel,
    DaemonMediaController* mediaController, QObject* parent,
    MonotonicClock clock, AudioDiagnosticsProvider audioDiagnosticsProvider,
    std::unique_ptr<HostTelemetrySampler> hostSampler,
    ReceiverLoadProvider receiverLoadProvider,
    std::shared_ptr<SharedHostSampler> sharedHostSampler)
    : QObject(parent)
    , m_server(server)
    , m_radioModel(radioModel)
    , m_mediaController(mediaController)
    , m_clock(std::move(clock))
    , m_audioDiagnosticsProvider(std::move(audioDiagnosticsProvider))
    , m_hostSampler(std::move(sharedHostSampler))
    , m_receiverLoadProvider(std::move(receiverLoadProvider))
{
    m_processClock.start();
    if (!m_clock) {
        m_clock = [this] { return m_processClock.elapsed(); };
    }
    if (!m_hostSampler) {
        m_hostSampler = std::make_shared<SharedHostSampler>(
            std::move(hostSampler), [this] { return clockNowMs(); });
    }
    if (!m_audioDiagnosticsProvider) {
        m_audioDiagnosticsProvider = [this] {
            return m_mediaController ? m_mediaController->audioDiagnostics()
                                     : DaemonAudioDiagnostics{};
        };
    }

    if (!m_receiverLoadProvider) {
        m_receiverLoadProvider = [this] { return radioModelReceiverLoads(); };
    }

    m_timer.setInterval(kSamplePeriodMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this,
            &DaemonTelemetryController::sampleNow);
    if (m_server) {
        connect(m_server, &StationServer::telemetrySessionStarted, this,
                &DaemonTelemetryController::onSessionStarted);
        connect(m_server, &StationServer::telemetrySessionEnded, this,
                &DaemonTelemetryController::onSessionEnded);
    }
    if (m_radioModel) {
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                &DaemonTelemetryController::onRadioConnectionStateChanged);
    }
}

DaemonTelemetryController::~DaemonTelemetryController()
{
    stopCollecting();
    retireRadioConnection();
}

qint64 DaemonTelemetryController::clockNowMs() const
{
    return std::max<qint64>(0, m_clock ? m_clock() : 0);
}

qint64 DaemonTelemetryController::sessionElapsedMs() const
{
    return std::max<qint64>(0, clockNowMs() - m_sessionStartedMs);
}

void DaemonTelemetryController::bindToSession(quint64 epoch)
{
    m_boundEpoch = epoch;
    if (m_epoch != epoch && m_server && m_server->telemetryAvailable(epoch)) {
        onSessionStarted(epoch);
    }
}

void DaemonTelemetryController::onSessionStarted(quint64 epoch)
{
    // iPhone app Task 76: one controller per session. A bound one serves
    // its own session alone; one on its own never leaves a live session
    // for a newer one.
    if (m_boundEpoch != 0 && epoch != m_boundEpoch) {
        return;
    }
    if (m_boundEpoch == 0 && m_epoch != 0 && m_epoch != epoch && m_server
        && m_server->telemetryAvailable(m_epoch)) {
        return;
    }
    stopCollecting();
    if (epoch == 0 || !m_server || !m_server->telemetryAvailable(epoch)) {
        return;
    }

    m_epoch = epoch;
    m_sessionStartedMs = clockNowMs();
    m_sequence = 0;
    m_audioBaseline.reset();
    m_radioObservation.reset();
    m_hostCpuBaselinePending = true;
    synchronizeRadioConnection();
    requestRadioObservation();
    if (m_automaticSamplingEnabled) {
        m_timer.start();
    }
}

void DaemonTelemetryController::onSessionEnded(quint64 epoch)
{
    if (epoch == m_epoch) {
        stopCollecting();
    }
}

void DaemonTelemetryController::stopCollecting()
{
    m_timer.stop();
    m_epoch = 0;
    m_sequence = 0;
    m_audioBaseline.reset();
    m_radioObservation.reset();
    m_outstandingRadioRequestId = 0;
    m_requestedConnection.clear();
}

void DaemonTelemetryController::onRadioConnectionStateChanged(ConnectionState)
{
    synchronizeRadioConnection();
    if (m_epoch != 0) {
        requestRadioObservation();
    }
}

void DaemonTelemetryController::synchronizeRadioConnection()
{
    RadioConnection* next = nullptr;
    if (m_radioModel && m_radioModel->isConnected()) {
        next = m_radioModel->connection();
    }
    if (next == m_radioConnection) {
        return;
    }

    retireRadioConnection();
    m_radioConnection = next;
    if (!m_radioConnection) {
        return;
    }

    m_radioRequestConnection = connect(
        this, &DaemonTelemetryController::radioTelemetryRequested,
        m_radioConnection, &RadioConnection::collectTelemetryObservation,
        Qt::QueuedConnection);
    m_radioReplyConnection = connect(
        m_radioConnection, &RadioConnection::telemetryObservationReady,
        this, &DaemonTelemetryController::onRadioObservation,
        Qt::QueuedConnection);
}

void DaemonTelemetryController::retireRadioConnection()
{
    QObject::disconnect(m_radioRequestConnection);
    QObject::disconnect(m_radioReplyConnection);
    m_radioRequestConnection = {};
    m_radioReplyConnection = {};
    m_radioConnection.clear();
    m_requestedConnection.clear();
    m_outstandingRadioRequestId = 0;
    m_radioObservation.reset();
}

void DaemonTelemetryController::requestRadioObservation()
{
    synchronizeRadioConnection();
    if (m_epoch == 0 || !m_radioConnection
        || m_outstandingRadioRequestId != 0) {
        return;
    }

    ++m_nextRadioRequestId;
    if (m_nextRadioRequestId == 0) {
        ++m_nextRadioRequestId;
    }
    m_outstandingRadioRequestId = m_nextRadioRequestId;
    m_requestedConnection = m_radioConnection;
    m_radioRequestElapsedMs = sessionElapsedMs();
    emit radioTelemetryRequested(m_outstandingRadioRequestId);
}

void DaemonTelemetryController::onRadioObservation(
    quint64 requestId, double rxMbps, double txMbps, bool hasRtt,
    qint64 rttMs, qint64 rttAgeMs, RadioDiagnosticsObservation diagnostics)
{
    if (m_epoch == 0 || requestId == 0
        || requestId != m_outstandingRadioRequestId
        || !m_requestedConnection
        || m_requestedConnection != m_radioConnection
        || sender() != m_requestedConnection.data()) {
        return;
    }

    RadioObservation observation;
    observation.rxMbps = rxMbps;
    observation.txMbps = txMbps;
    observation.diagnostics = std::move(diagnostics);
    observation.requestedElapsedMs = m_radioRequestElapsedMs;
    if (hasRtt && rttMs >= 0 && rttAgeMs >= 0) {
        observation.rttMs = rttMs;
        observation.rttAgeMs = rttAgeMs;
    }
    m_radioObservation = observation;
    m_outstandingRadioRequestId = 0;
    m_requestedConnection.clear();
}

void DaemonTelemetryController::applyRadioObservation(
    StationTelemetrySnapshot& snapshot, qint64 sampledElapsedMs) const
{
    snapshot.radio.connected = m_radioModel && m_radioModel->isConnected()
        && m_radioConnection;
    if (!snapshot.radio.connected || !m_radioObservation) {
        return;
    }

    const qint64 observationAgeMs = std::max<qint64>(
        0, sampledElapsedMs - m_radioObservation->requestedElapsedMs);
    const bool fresh = observationAgeMs <= kSamplePeriodMs * kObservationStalePeriods;
    if (fresh) {
        snapshot.radio.rxMbps = m_radioObservation->rxMbps;
        snapshot.radio.txMbps = m_radioObservation->txMbps;
        if (m_radioObservation->rttMs && m_radioObservation->rttAgeMs) {
            snapshot.radio.rttMs = m_radioObservation->rttMs;
            snapshot.radio.rttAgeMs = *m_radioObservation->rttAgeMs
                + observationAgeMs;
        }
        if (m_radioObservation->diagnostics.radioUdpBasePort) {
            snapshot.radio.radioUdpBasePort =
                *m_radioObservation->diagnostics.radioUdpBasePort;
        }
    }

    // A stale queued observation may retain historical transition counts,
    // but it cannot claim a current overloaded/clear bit. Use request time
    // (rather than reply time) so queuing only makes ages older.
    constexpr qint64 kMaxExactJsonInteger = 9007199254740991LL;
    const auto adjustedAge = [observationAgeMs](qint64 age) {
        return age >= kMaxExactJsonInteger - observationAgeMs
            ? kMaxExactJsonInteger : age + observationAgeMs;
    };
    const int adcCount = m_radioModel
        ? std::clamp(m_radioModel->boardCapabilities().adcCount, 0, 3) : 0;
    QVector<StationAdcOverloadTelemetry> adcs;
    for (int adc = 0; adc < adcCount; ++adc) {
        const RadioAdcOverloadObservation& status =
            m_radioObservation->diagnostics.adcOverloads[adc];
        if (!status.known) { continue; }
        StationAdcOverloadTelemetry entry;
        entry.adc = adc;
        entry.eventsSinceConnection = status.eventsSinceConnection;
        entry.statusAgeMs = adjustedAge(status.statusAgeMs);
        if (fresh && *entry.statusAgeMs <= kSamplePeriodMs * kObservationStalePeriods) {
            entry.overloaded = status.active;
        }
        if (status.lastOverloadAgeMs) {
            entry.lastOverloadAgeMs = adjustedAge(*status.lastOverloadAgeMs);
        }
        adcs.append(entry);
    }
    if (!adcs.isEmpty()) {
        snapshot.radio.adcOverloads = adcs;
    }
}

void DaemonTelemetryController::applyRadioStatus(StationTelemetrySnapshot& snapshot) const
{
    // R-R3-32 / R-R3-46 (remote-window parity Task 6): what the Core's own
    // window shows for its radio, read as it is now: the PA readings
    // (RadioModel::paReadings(), each absent when the radio has none) and
    // the link counters (RadioConnection::linkStats(), atomics its receive
    // path writes, safe to read from here). Only while the radio is
    // connected; StationServer strips all of it for a peer below minor 11.
    if (!snapshot.radio.connected || !m_radioModel || !m_radioConnection) {
        return;
    }
    snapshot.radio.connectionAgeMs = m_radioModel->connectionAgeMs();
    const RadioModel::PaReadings pa = m_radioModel->paReadings();
    snapshot.radio.paVolts = pa.paVolts;
    snapshot.radio.supplyVolts = pa.supplyVolts;
    snapshot.radio.paCurrentAmps = pa.paCurrentAmps;
    snapshot.radio.paTemperatureCelsius = pa.paTemperatureCelsius;
    const RadioLinkStats::Snapshot link = m_radioConnection->linkStats();
    snapshot.radio.udpPacketsSeen = static_cast<qint64>(
        std::min<quint64>(link.udpPacketsSeen, 9007199254740991ULL));
    snapshot.radio.packetLossPercent = link.packetLossPercent;
    snapshot.radio.jitterMs = link.jitterMs;
    snapshot.radio.packetGapMs = link.packetGapMs;
    const int rateHz = m_radioModel->connectionSampleRateHz();
    if (rateHz > 0) {
        snapshot.radio.sampleRateHz = rateHz;
    }
    // R-R3-32 (remote-window parity Task 14, stationTelemetryVersion 5):
    // the Hermes Lite 2 link as the Core's own HL2 I/O tab, Radio Status and
    // Connection Quality show it, from the bandwidth monitor the Core's
    // connection feeds. Only a radio with the monitor (the HL2) reports it.
    if (m_radioModel->boardCapabilities().hasBandwidthMonitor) {
        const HermesLiteBandwidthMonitor& bw = m_radioModel->bwMonitor();
        const double rx = bw.ep6IngressBytesPerSec();
        const double tx = bw.ep2EgressBytesPerSec();
        if (std::isfinite(rx) && rx >= 0.0) { snapshot.radio.hl2RxBytesPerSecond = rx; }
        if (std::isfinite(tx) && tx >= 0.0) { snapshot.radio.hl2TxBytesPerSecond = tx; }
        snapshot.radio.hl2Throttled = bw.isThrottled();
        snapshot.radio.hl2SequenceGaps = std::max(0, bw.ep6SequenceErrorCount());
    }
}

void DaemonTelemetryController::applyAudioObservation(
    StationTelemetrySnapshot& snapshot, qint64 sampledElapsedMs)
{
    const DaemonAudioDiagnostics diagnostics = m_audioDiagnosticsProvider
        ? m_audioDiagnosticsProvider() : DaemonAudioDiagnostics{};
    snapshot.audio.active = diagnostics.activeContext;
    snapshot.audio.contextGeneration = diagnostics.contextGeneration;
    if (!diagnostics.activeContext) {
        m_audioBaseline.reset();
        return;
    }

    const AudioBaseline current{
        diagnostics.contextGeneration,
        sampledElapsedMs,
        diagnostics.sender.source.capturedValidRateFrames,
        diagnostics.sender.source.sourceDropEvents,
        diagnostics.sender.encodedPackets,
        diagnostics.sender.encodeFailures,
        diagnostics.sendAccepted,
        diagnostics.sendRejected,
    };

    if (m_audioBaseline
        && m_audioBaseline->contextGeneration == current.contextGeneration) {
        const qint64 elapsedMs = current.sampledElapsedMs
            - m_audioBaseline->sampledElapsedMs;
        const std::optional<double> sourceFrames = rate(
            current.sourceFrames, m_audioBaseline->sourceFrames, elapsedMs);
        const std::optional<double> sourceDrops = rate(
            current.sourceDrops, m_audioBaseline->sourceDrops, elapsedMs);
        const std::optional<double> encodedPackets = rate(
            current.encodedPackets, m_audioBaseline->encodedPackets, elapsedMs);
        const std::optional<double> encodeFailures = rate(
            current.encodeFailures, m_audioBaseline->encodeFailures, elapsedMs);
        const std::optional<double> sendAccepted = rate(
            current.sendAccepted, m_audioBaseline->sendAccepted, elapsedMs);
        const std::optional<double> sendRejected = rate(
            current.sendRejected, m_audioBaseline->sendRejected, elapsedMs);

        // A reset in any counter retires the whole baseline. Mixing rates
        // from different effective lifetimes would make one snapshot
        // internally inconsistent.
        if (sourceFrames && sourceDrops && encodedPackets && encodeFailures
            && sendAccepted && sendRejected) {
            snapshot.audio.sourceFramesPerSecond = sourceFrames;
            snapshot.audio.sourceDropsPerSecond = sourceDrops;
            snapshot.audio.encodedPacketsPerSecond = encodedPackets;
            snapshot.audio.encodeFailuresPerSecond = encodeFailures;
            snapshot.audio.sendAcceptedPerSecond = sendAccepted;
            snapshot.audio.sendRejectedPerSecond = sendRejected;
        }
    }
    m_audioBaseline = current;
}

StationReceiverTelemetry DaemonTelemetryController::receiverTelemetry(
    int sliceId, const ReceiverDspLoad& load)
{
    StationReceiverTelemetry receiver;
    receiver.sliceId = sliceId;
    // Idle means nothing was processed in the interval, which is not proof
    // of no load (ReceiverDspLoad::idle): leave the load absent.
    if (!load.idle && std::isfinite(load.load) && load.load >= 0.0) {
        receiver.loadPercent = load.load * 100.0;
    }
    receiver.inputDelayMs = std::max<qint64>(0, load.inputDelayMs);
    receiver.skippedInputMs = std::max<qint64>(0, load.droppedInputMs);
    return receiver;
}

std::optional<QVector<StationReceiverTelemetry>>
DaemonTelemetryController::radioModelReceiverLoads() const
{
    if (!m_radioModel) {
        return std::nullopt;
    }
    // R-R3-40: the snapshot RadioModel's sampler cached at its latest
    // 500 ms tick. A slice with no snapshot yet (no WDSP channel, or only
    // its first reading so far) is left out rather than reported as zero.
    QVector<StationReceiverTelemetry> receivers;
    for (SliceModel* slice : m_radioModel->slices()) {
        if (slice == nullptr || receivers.size() >= kMaxStationReceivers) {
            continue;
        }
        const int sliceId = slice->sliceIndex();
        const std::optional<ReceiverDspLoad> load = m_radioModel->receiverDspLoad(sliceId);
        if (!load) {
            continue;
        }
        receivers.append(receiverTelemetry(sliceId, *load));
    }
    std::sort(receivers.begin(), receivers.end(),
              [](const StationReceiverTelemetry& a, const StationReceiverTelemetry& b) {
                  return a.sliceId < b.sliceId;
              });
    return receivers;
}

void DaemonTelemetryController::sampleNow()
{
    if (m_epoch == 0 || !m_server) {
        return;
    }

    synchronizeRadioConnection();
    const qint64 sampledElapsedMs = sessionElapsedMs();
    StationTelemetrySnapshot snapshot;
    ++m_sequence;
    if (m_sequence == 0) {
        ++m_sequence;
    }
    snapshot.sequence = m_sequence;
    snapshot.sampledElapsedMs = sampledElapsedMs;
    applyRadioObservation(snapshot, sampledElapsedMs);
    applyRadioStatus(snapshot);
    applyAudioObservation(snapshot, sampledElapsedMs);
    snapshot.host = m_hostSampler->reading();
    if (m_hostCpuBaselinePending) {
        // The shared reading's interval may have begun before this session.
        m_hostCpuBaselinePending = false;
        snapshot.host.systemCpuPercent.reset();
        snapshot.host.processCpuPercent.reset();
    }
    snapshot.receivers = m_receiverLoadProvider();
    m_server->sendTelemetry(snapshot, m_epoch);
    requestRadioObservation();
}

} // namespace NereusSDR
