// no-port-check: NereusSDR-original. Observational Core/GUI telemetry adapter.
#include "RemoteTelemetryController.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteMediaController.h"
#include "core/session/StationClient.h"
#include "models/RadioModel.h"
#include <QLoggingCategory>
#include <QStringList>

Q_LOGGING_CATEGORY(lcRemoteTelemetry, "nereus.remote.telemetry")

namespace NereusSDR {
namespace {
using Metric = TelemetryHistory::Metric;
constexpr qint64 kStationFreshMs = 3000;
// R-R3-07/33: one diagnostics log line a minute for the two-hour soak.
constexpr qint64 kDiagnosticsLogIntervalMs = 60000;
// Session heartbeat pings are 20 seconds apart. A metrics tick is not a pong.
constexpr qint64 kRttFreshMs = 60000;
std::size_t index(Metric metric) { return static_cast<std::size_t>(metric); }
TelemetryHistory::MetricMask mask(Metric first, Metric last)
{
    TelemetryHistory::MetricMask result;
    for (auto i = index(first); i <= index(last); ++i) { result.set(i); }
    return result;
}
void breakRange(TelemetryHistory& history, Metric first, Metric last)
{
    for (auto i = index(first); i <= index(last); ++i) {
        history.breakMetric(static_cast<Metric>(i));
    }
}
std::optional<double> rate(quint64 current, quint64 previous, qint64 elapsed)
{
    if (elapsed <= 0 || current < previous) { return std::nullopt; }
    return double(current - previous) * 1000.0 / double(elapsed);
}
QString number(std::optional<double> value, int decimals = 1)
{
    return value ? QString::number(*value, 'f', decimals) : QStringLiteral("—");
}
QString logged(std::optional<double> value, int decimals = 1)
{
    return value ? QString::number(*value, 'f', decimals) : QStringLiteral("not measured");
}
template <typename Integer>
QString logged(std::optional<Integer> value)
{
    return value ? QString::number(*value) : QStringLiteral("not measured");
}
std::optional<double> mebibytes(std::optional<qint64> kibibytes)
{
    return kibibytes ? std::optional<double>(double(*kibibytes) / 1024.0) : std::nullopt;
}
}

RemoteTelemetryController::RemoteTelemetryController(
    StationClient* client, RemoteMediaController* media, QObject* parent,
    Clock clock, PlaybackObserver playback, TrafficObserver traffic, DelayObserver delay)
    : QObject(parent), m_client(client), m_media(media),
      m_now(std::move(clock)), m_playback(std::move(playback)), m_traffic(std::move(traffic)),
      m_delay(std::move(delay))
{
    m_clock.start();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &RemoteTelemetryController::sampleNow);
    if (client) {
        connect(client, &StationClient::telemetryReceived,
                this, &RemoteTelemetryController::receiveStation);
        connect(client, &StationClient::telemetrySessionEnded, this, [this](quint32 epoch) {
            if (epoch == m_epoch) { clearSession(); }
        });
        connect(client, &StationClient::handshakeComplete, this, [this] {
            clearSession();
            sampleNow();
        });
        connect(client, &StationClient::connectionActivityChanged, this, [this] {
            if (!m_client || !m_client->isHandshakeComplete()) { clearSession(); }
        });
    }
    sampleNow();
    if (!m_now) { m_timer.start(); }
}

void RemoteTelemetryController::setPaReadingsTarget(RadioModel* model)
{
    m_paTarget = model;
    connect(this, &RemoteTelemetryController::changed, this,
            &RemoteTelemetryController::pushPaReadings, Qt::UniqueConnection);
    pushPaReadings();
}

void RemoteTelemetryController::pushPaReadings()
{
    if (!m_paTarget) { return; }
    // The view's radio section is empty unless the Core's measurements are
    // current, so an out-of-date reading becomes absent, never a 0.
    RadioModel::PaReadings readings;
    if (m_view.state == RemoteTelemetryView::State::Current) {
        readings.paVolts = m_view.radio.paVolts;
        readings.supplyVolts = m_view.radio.supplyVolts;
        readings.paCurrentAmps = m_view.radio.paCurrentAmps;
        readings.paTemperatureCelsius = m_view.radio.paTemperatureCelsius;
    }
    m_paTarget->applyCorePaReadings(readings);
    // R-R3-32 (parity Task 14): the Core's HL2 link the same way (station
    // telemetry version 5); the throttle event count stays absent, since
    // the Core does not send it.
    RadioModel::Hl2LinkFigures hl2;
    if (m_view.state == RemoteTelemetryView::State::Current) {
        hl2.rxBytesPerSecond = m_view.radio.hl2RxBytesPerSecond;
        hl2.txBytesPerSecond = m_view.radio.hl2TxBytesPerSecond;
        hl2.throttled = m_view.radio.hl2Throttled;
        hl2.sequenceGaps = m_view.radio.hl2SequenceGaps;
    }
    m_paTarget->applyCoreHl2LinkFigures(hl2);
    // The Core's radio connection age (station telemetry version 6) for
    // the Radio Status page's Uptime, as of now: the sample's age plus how
    // long ago it arrived.
    std::optional<qint64> connectionAge;
    if (m_view.state == RemoteTelemetryView::State::Current && m_view.radio.connectionAgeMs) {
        connectionAge = *m_view.radio.connectionAgeMs + m_view.stationAgeMs.value_or(0);
    }
    m_paTarget->applyCoreConnectionAge(connectionAge);
}

qint64 RemoteTelemetryController::nowMs() const
{
    return m_now ? m_now() : m_clock.elapsed();
}

void RemoteTelemetryController::clearSession()
{
    breakRange(m_history, Metric::RadioRxMbps, Metric::AudioDeliveryDelayMs);
    m_station.reset();
    m_transportBaseline.reset();
    m_mediaBaseline.reset();
    m_watchBaseline.reset();
    m_playbackBaseline.reset();
    m_playbackEventsBaseline.reset();
    m_lastTickMs = -1;
    m_epoch = 0;
    m_stationWasStale = false;
    m_coreHostReported = false;
    m_coreReceiversReported = false;
    m_diagnosticsLogBaselineMs.reset();
    m_view = {};
    emit changed();
}

void RemoteTelemetryController::receiveStation(
    const StationTelemetrySnapshot& sample, quint32 epoch)
{
    if (!m_client || !m_client->telemetryAvailable()
        || epoch != m_client->sessionEpoch()) { return; }
    if (m_epoch != epoch) { clearSession(); m_epoch = epoch; }
    if (m_station && (sample.sequence <= m_station->sequence
                     || sample.sampledElapsedMs < m_station->sampledElapsedMs)) { return; }
    if (m_station && sample.audio.contextGeneration != m_station->audio.contextGeneration) {
        breakRange(m_history, Metric::AudioSourceFramesPerSecond, Metric::AudioSourceDropsPerSecond);
    }
    m_station = sample;
    m_stationReceivedMs = nowMs();
    m_stationWasStale = false;
    TelemetryHistory::Sample observation{m_stationReceivedMs, epoch, {}};
    auto& values = observation.values;
    values[index(Metric::RadioRxMbps)] = sample.radio.rxMbps;
    values[index(Metric::RadioTxMbps)] = sample.radio.txMbps;
    if (sample.radio.rttMs && sample.radio.rttAgeMs && *sample.radio.rttAgeMs <= kRttFreshMs) {
        values[index(Metric::RadioRttMs)] = double(*sample.radio.rttMs);
    }
    values[index(Metric::AudioSourceFramesPerSecond)] = sample.audio.sourceFramesPerSecond;
    values[index(Metric::AudioEncodedPacketsPerSecond)] = sample.audio.encodedPacketsPerSecond;
    values[index(Metric::AudioSendAcceptedPerSecond)] = sample.audio.sendAcceptedPerSecond;
    values[index(Metric::AudioSendRejectedPerSecond)] = sample.audio.sendRejectedPerSecond;
    values[index(Metric::AudioSourceDropsPerSecond)] = sample.audio.sourceDropsPerSecond;
    // An older or non-Linux Core leaves every host value absent, which
    // records a gap and never a zero.
    values[index(Metric::CoreSystemCpuPercent)] = sample.host.systemCpuPercent;
    values[index(Metric::CoreProcessCpuPercent)] = sample.host.processCpuPercent;
    values[index(Metric::CoreMemoryAvailableMiB)] = mebibytes(sample.host.memoryAvailableKiB);
    values[index(Metric::CoreProcessResidentMiB)] = mebibytes(sample.host.processResidentKiB);
    values[index(Metric::CoreHottestZoneCelsius)] = sample.host.hottestZoneCelsius;
    if (!sample.host.isEmpty()) { m_coreHostReported = true; }
    // R-R3-40: one slot per slice ID. A receiver missing from the sample, or
    // idle in it, records a gap and never a zero; so does an older Core.
    if (sample.receivers) {
        m_coreReceiversReported = true;
        for (const StationReceiverTelemetry& receiver : *sample.receivers) {
            if (receiver.sliceId >= 0
                && receiver.sliceId < TelemetryHistory::kCoreReceiverLoadSlots) {
                values[index(TelemetryHistory::coreReceiverLoadMetric(receiver.sliceId))] =
                    receiver.loadPercent;
            }
        }
    }
    m_history.append(observation, mask(Metric::RadioRxMbps, Metric::RadioRttMs)
        | mask(Metric::AudioSourceFramesPerSecond, Metric::AudioSourceDropsPerSecond)
        | mask(Metric::CoreSystemCpuPercent, Metric::CoreReceiverLoadPercentSlot4));
    refreshCurrent(m_stationReceivedMs);
    emit changed();
}

void RemoteTelemetryController::refreshCurrent(qint64 now)
{
    m_view.stationAgeMs.reset();
    m_view.radio = {};
    m_view.coreAudio = {};
    m_view.coreHost = {};
    m_view.coreHostReported = m_coreHostReported;
    m_view.coreReceivers.reset();
    m_view.coreReceiversReported = m_coreReceiversReported;
    if (!m_client || !m_client->isHandshakeComplete()) {
        m_view = {};
        return;
    }
    if (!m_client->telemetryAvailable()) { m_view.state = RemoteTelemetryView::State::Unsupported; }
    else if (!m_station) { m_view.state = RemoteTelemetryView::State::Waiting; }
    else {
        const qint64 age = qMax<qint64>(0, now - m_stationReceivedMs);
        m_view.stationAgeMs = age;
        if (age > kStationFreshMs) {
            m_view.state = RemoteTelemetryView::State::Stale;
            if (!m_stationWasStale) {
                breakRange(m_history, Metric::RadioRxMbps, Metric::RadioRttMs);
                breakRange(m_history, Metric::AudioSourceFramesPerSecond, Metric::AudioSourceDropsPerSecond);
                breakRange(m_history, Metric::CoreSystemCpuPercent, Metric::CoreReceiverLoadPercentSlot4);
                m_stationWasStale = true;
            }
        } else {
            m_view.state = RemoteTelemetryView::State::Current;
            m_view.radio = m_station->radio;
            m_view.coreAudio = m_station->audio;
            m_view.coreHost = m_station->host;
            m_view.coreReceivers = m_station->receivers;
            if (m_view.radio.rttAgeMs) {
                *m_view.radio.rttAgeMs += age;
                if (*m_view.radio.rttAgeMs > kRttFreshMs) {
                    m_view.radio.rttAgeMs.reset(); m_view.radio.rttMs.reset();
                }
            }
        }
    }
}

void RemoteTelemetryController::sampleNow()
{
    const qint64 now = nowMs();
    if (!m_client || !m_client->isHandshakeComplete()) {
        if (m_epoch || m_view.state != RemoteTelemetryView::State::Disconnected) { clearSession(); }
        // Prune retained history even during an extended disconnection.
        m_history.append({now, 0, {}});
        return;
    }
    if (m_epoch != m_client->sessionEpoch()) { clearSession(); m_epoch = m_client->sessionEpoch(); }
    const qint64 elapsed = m_lastTickMs < 0 ? 0 : now - m_lastTickMs;
    TelemetryHistory::Sample observation{now, m_epoch, {}};
    auto& values = observation.values;
    m_view.controlRxKbps.reset(); m_view.controlTxKbps.reset();
    m_view.coreGuiRxKbps.reset(); m_view.coreGuiTxKbps.reset();
    m_view.coreGuiTotalKbps.reset(); m_view.audioPayloadRxKbps.reset();
    m_view.audioRtpRxKbps.reset();
    m_view.coreRttMs.reset(); m_view.coreRttAgeMs.reset();
    const auto transport = m_client->transportTelemetry();
    if (transport) {
        if (m_transportBaseline && elapsed > 0) {
            auto rx = rate(transport->receivedPayloadBytes, m_transportBaseline->receivedPayloadBytes, elapsed);
            auto tx = rate(transport->acceptedPayloadBytes, m_transportBaseline->acceptedPayloadBytes, elapsed);
            if (rx) { m_view.controlRxKbps = *rx * 8.0 / 1000.0; }
            if (tx) { m_view.controlTxKbps = *tx * 8.0 / 1000.0; }
        }
        if (transport->pongRttMs && transport->pongAgeMs && *transport->pongAgeMs <= kRttFreshMs) {
            m_view.coreRttMs = transport->pongRttMs;
            m_view.coreRttAgeMs = transport->pongAgeMs;
            values[index(Metric::SessionRttMs)] = double(*transport->pongRttMs);
        }
    }
    m_transportBaseline = transport;
    values[index(Metric::SessionPayloadRxKbps)] = m_view.controlRxKbps;
    values[index(Metric::SessionPayloadTxKbps)] = m_view.controlTxKbps;

    const auto watch = m_client->auxiliaryWatchTelemetry();
    const bool watchContinuous = watch && m_watchBaseline && elapsed > 0
        && watch->receivedPayloadBytes >= m_watchBaseline->receivedPayloadBytes
        && watch->submittedPayloadBytes >= m_watchBaseline->submittedPayloadBytes;
    const auto watchRx = watchContinuous
        ? rate(watch->receivedPayloadBytes, m_watchBaseline->receivedPayloadBytes, elapsed)
        : std::nullopt;
    const auto watchTx = watchContinuous
        ? rate(watch->submittedPayloadBytes, m_watchBaseline->submittedPayloadBytes, elapsed)
        : std::nullopt;
    m_watchBaseline = watch;

    const auto media = m_traffic ? m_traffic()
        : m_media ? m_media->trafficTelemetry() : std::nullopt;
    const bool mediaContinuous = media && m_mediaBaseline
        && media->generation == m_mediaBaseline->generation && elapsed > 0
        && media->traffic.receivedDisplayPayloadBytes >= m_mediaBaseline->traffic.receivedDisplayPayloadBytes
        && media->traffic.receivedRtpBytes >= m_mediaBaseline->traffic.receivedRtpBytes
        && media->traffic.submittedDisplayPayloadBytes >= m_mediaBaseline->traffic.submittedDisplayPayloadBytes
        && media->traffic.submittedRtpBytes >= m_mediaBaseline->traffic.submittedRtpBytes
        && media->traffic.receivedTxPayloadBytes >= m_mediaBaseline->traffic.receivedTxPayloadBytes
        && media->traffic.submittedTxPayloadBytes >= m_mediaBaseline->traffic.submittedTxPayloadBytes
        && media->traffic.receivedIqPayloadBytes >= m_mediaBaseline->traffic.receivedIqPayloadBytes
        && media->traffic.submittedIqPayloadBytes >= m_mediaBaseline->traffic.submittedIqPayloadBytes;
    if (mediaContinuous) {
        const auto& current = media->traffic;
        const auto& previous = m_mediaBaseline->traffic;
        const auto displayRx = rate(current.receivedDisplayPayloadBytes, previous.receivedDisplayPayloadBytes, elapsed);
        const auto rtpRx = rate(current.receivedRtpBytes, previous.receivedRtpBytes, elapsed);
        if (rtpRx) { m_view.audioRtpRxKbps = *rtpRx * 8.0 / 1000.0; }
        const auto displayTx = rate(current.submittedDisplayPayloadBytes, previous.submittedDisplayPayloadBytes, elapsed);
        const auto rtpTx = rate(current.submittedRtpBytes, previous.submittedRtpBytes, elapsed);
        const auto txRx = rate(current.receivedTxPayloadBytes, previous.receivedTxPayloadBytes, elapsed);
        const auto txTx = rate(current.submittedTxPayloadBytes, previous.submittedTxPayloadBytes, elapsed);
        const auto iqRx = rate(current.receivedIqPayloadBytes, previous.receivedIqPayloadBytes, elapsed);
        const auto iqTx = rate(current.submittedIqPayloadBytes, previous.submittedIqPayloadBytes, elapsed);
        if (displayRx && rtpRx && txRx && iqRx && watchRx && m_view.controlRxKbps) {
            m_view.coreGuiRxKbps = *m_view.controlRxKbps
                + (*displayRx + *rtpRx + *txRx + *iqRx + *watchRx) * 8.0 / 1000.0;
        }
        if (displayTx && rtpTx && txTx && iqTx && watchTx && m_view.controlTxKbps) {
            m_view.coreGuiTxKbps = *m_view.controlTxKbps
                + (*displayTx + *rtpTx + *txTx + *iqTx + *watchTx) * 8.0 / 1000.0;
        }
        if (m_view.coreGuiRxKbps && m_view.coreGuiTxKbps) {
            m_view.coreGuiTotalKbps = *m_view.coreGuiRxKbps + *m_view.coreGuiTxKbps;
        }
    }
    // Each media lifetime has its own counters. Missing/new peers produce
    // a gap; they must never turn an unknown media rate into control-only zero.
    m_mediaBaseline = media;
    values[index(Metric::CoreGuiRxKbps)] = m_view.coreGuiRxKbps;
    values[index(Metric::CoreGuiTxKbps)] = m_view.coreGuiTxKbps;
    values[index(Metric::CoreGuiTotalKbps)] = m_view.coreGuiTotalKbps;
    values[index(Metric::AudioRtpRxKbps)] = m_view.audioRtpRxKbps;

    const auto playback = m_playback ? m_playback()
        : m_media ? m_media->audioTelemetry() : RemoteAudioReceiverTelemetry{};
    m_view.playback = playback;
    m_view.playbackActive = playback.running && playback.decodedPackets > 0
        && playback.lastDeviceProgressAgeMs && *playback.lastDeviceProgressAgeMs < 500;
    if (m_playbackBaseline && playback.generation != m_playbackBaseline->generation) {
        breakRange(m_history, Metric::PlaybackDecodedPacketsPerSecond, Metric::PlaybackPacketAgeMs);
        breakRange(m_history, Metric::AudioPayloadRxKbps, Metric::SpeakerBufferMs);
        breakRange(m_history, Metric::AudioDelayMs, Metric::AudioDeliveryDelayMs);
    }
    if (playback.running && m_playbackBaseline && m_playbackBaseline->running
        && playback.generation == m_playbackBaseline->generation && elapsed > 0) {
        const auto& previous = *m_playbackBaseline;
        if (mediaContinuous) {
            const auto opus = rate(playback.receivedAudioPayloadBytes, previous.receivedAudioPayloadBytes, elapsed);
            if (opus) { m_view.audioPayloadRxKbps = *opus * 8.0 / 1000.0; }
        }
        values[index(Metric::PlaybackDecodedPacketsPerSecond)] = rate(playback.decodedPackets, previous.decodedPackets, elapsed);
        values[index(Metric::PlaybackConcealedPacketsPerSecond)] = rate(playback.concealedPackets, previous.concealedPackets, elapsed);
        values[index(Metric::PlaybackLatePacketsPerSecond)] = rate(playback.latePackets, previous.latePackets, elapsed);
    }
    // A restart-causing interruption can happen entirely between timer ticks.
    // Lifetime event totals survive receiver contexts, including stopped ones;
    // an unavailable transition snapshot must not erase their previous baseline.
    if (playback.lifetimeUnderflows && playback.lifetimeOverflows) {
        const PlaybackEvents events{*playback.lifetimeUnderflows,
                                    *playback.lifetimeOverflows, now};
        if (m_playbackEventsBaseline) {
            const auto& previous = *m_playbackEventsBaseline;
            const auto underflows = rate(events.underflows, previous.underflows, now - previous.sampledMs);
            const auto overflows = rate(events.overflows, previous.overflows, now - previous.sampledMs);
            // A stable stopped context has no ongoing playback event rate.
            // Preserve a final nonzero event even when it stopped the receiver.
            if (playback.running || (underflows && *underflows > 0) || (overflows && *overflows > 0)) {
                values[index(Metric::PlaybackUnderflowsPerSecond)] = underflows;
                values[index(Metric::PlaybackOverflowsPerSecond)] = overflows;
            }
        }
        m_playbackEventsBaseline = events;
    }
    if (playback.running && playback.lastAdmittedPacketAgeMs) {
        values[index(Metric::PlaybackPacketAgeMs)] = double(*playback.lastAdmittedPacketAgeMs);
    }
    values[index(Metric::AudioPayloadRxKbps)] = m_view.audioPayloadRxKbps;
    if (playback.running) {
        values[index(Metric::SpeakerBufferMs)] = playback.speakerQueuedMs;
    }
    // R-R3-35: the measured delay, only while it is measured; anything
    // else (no echo, a new audio context, an older Core) is a gap.
    m_view.audioDelay = m_delay ? m_delay()
        : m_media ? m_media->audioDelay() : RemoteAudioDelayReport{};
    if (const auto& estimate = m_view.audioDelay.estimate) {
        values[index(Metric::AudioDelayMs)] = estimate->delayMs;
        values[index(Metric::AudioDelayAccuracyMs)] = estimate->boundMs;
        values[index(Metric::AudioDeliveryDelayMs)] = estimate->deliveryMs;
    }
    m_playbackBaseline = playback;
    m_lastTickMs = now;
    m_history.append(observation, mask(Metric::SessionPayloadRxKbps, Metric::SessionRttMs)
        | mask(Metric::PlaybackDecodedPacketsPerSecond, Metric::SpeakerBufferMs)
        | mask(Metric::AudioDelayMs, Metric::AudioDeliveryDelayMs));
    refreshCurrent(now);
    // The first line comes one interval after the session starts, then one
    // per interval while it lasts.
    if (!m_diagnosticsLogBaselineMs) {
        m_diagnosticsLogBaselineMs = now;
    } else if (now - *m_diagnosticsLogBaselineMs >= kDiagnosticsLogIntervalMs) {
        m_diagnosticsLogBaselineMs = now;
        logDiagnostics(now);
    }
    emit changed();
}

void RemoteTelemetryController::logDiagnostics(qint64 now) const
{
    // R-R3-07/33: the soak reads the receiver counters and the Core
    // computer's load from this one line. Absent values say so.
    const auto& p = m_view.playback;
    const StationHostTelemetry host = m_station ? m_station->host : StationHostTelemetry{};
    const RemoteDisplayTelemetry display =
        m_media ? m_media->displayTelemetry() : RemoteDisplayTelemetry{};
    const std::optional<double> driftPpm = p.driftRatio
        ? std::optional<double>((*p.driftRatio - 1.0) * 1'000'000.0) : std::nullopt;
    QStringList fields;
    fields << QStringLiteral("context=%1").arg(p.generation)
           << QStringLiteral("running=%1").arg(p.running ? QStringLiteral("yes") : QStringLiteral("no"))
           << QStringLiteral("admitted=%1").arg(p.acceptedPackets)
           << QStringLiteral("startDiscardedPackets=%1").arg(p.startDiscardedPackets)
           << QStringLiteral("decoded=%1").arg(p.decodedPackets)
           << QStringLiteral("concealed=%1").arg(p.concealedPackets)
           << QStringLiteral("late=%1").arg(p.latePackets)
           << QStringLiteral("invalid=%1").arg(p.invalidPackets)
           << QStringLiteral("duplicate=%1").arg(p.duplicatePackets)
           << QStringLiteral("rejectedHeaders=%1").arg(p.rejectedHeaders)
           << QStringLiteral("expected=%1").arg(p.expectedPackets)
           << QStringLiteral("missing=%1").arg(p.missingPackets)
           << QStringLiteral("underflows=%1").arg(p.underflows)
           << QStringLiteral("overflows=%1").arg(p.overflows)
           << QStringLiteral("lifetimeUnderflows=%1").arg(logged(p.lifetimeUnderflows))
           << QStringLiteral("lifetimeOverflows=%1").arg(logged(p.lifetimeOverflows))
           << QStringLiteral("deviceConsumedFrames=%1").arg(p.deviceConsumedFrames)
           << QStringLiteral("lastAdmittedPacketAgeMs=%1").arg(logged(p.lastAdmittedPacketAgeMs))
           << QStringLiteral("arrivalJitterMs=%1").arg(logged(p.arrivalJitterMs))
           << QStringLiteral("speakerQueuedMs=%1").arg(logged(p.speakerQueuedMs))
           << QStringLiteral("reorderQueuedMs=%1").arg(logged(p.reorderQueuedMs))
           << QStringLiteral("jitterHoldMs=%1").arg(logged(p.jitterHoldMs))
           << QStringLiteral("burstDroppedPackets=%1").arg(p.burstDroppedPackets)
           << QStringLiteral("streamGapReanchors=%1").arg(p.streamGapReanchors)
           << QStringLiteral("trimmedPackets=%1").arg(p.trimmedPackets)
           << QStringLiteral("skippedAudioMs=%1").arg(p.skippedAudioMs, 0, 'f', 0)
           << QStringLiteral("rewoundIntervals=%1").arg(p.rewoundIntervals)
           << QStringLiteral("linkInterruptions=%1").arg(p.linkInterruptions)
           << QStringLiteral("driftRatio=%1").arg(logged(p.driftRatio, 9))
           << QStringLiteral("driftPpm=%1").arg(logged(driftPpm, 1))
           << QStringLiteral("audioDelayMs=%1").arg(logged(m_view.audioDelay.estimate
                  ? std::optional<double>(m_view.audioDelay.estimate->delayMs) : std::nullopt))
           << QStringLiteral("audioDelayAccuracyMs=%1").arg(logged(m_view.audioDelay.estimate
                  ? std::optional<double>(m_view.audioDelay.estimate->boundMs) : std::nullopt))
           << QStringLiteral("audioDelayIncludesDevice=%1").arg(m_view.audioDelay.estimate
                  ? (m_view.audioDelay.estimate->includesDevice ? QStringLiteral("yes") : QStringLiteral("no"))
                  : QStringLiteral("not measured"))
           << QStringLiteral("deliveryDelayMs=%1").arg(logged(m_view.audioDelay.estimate
                  ? m_view.audioDelay.estimate->deliveryMs : std::nullopt))
           // R-R3-21 / R-R3-08: the display on the audio's clock and how it
           // rode through the link.
           << QStringLiteral("displayKeyframeWaits=%1").arg(display.keyframeWaits)
           << QStringLiteral("displayKeyframeRequests=%1").arg(display.keyframeRequests)
           << QStringLiteral("displayRowsBlended=%1").arg(display.rowsBlended)
           << QStringLiteral("displayRowsRepeated=%1").arg(display.rowsRepeated)
           << QStringLiteral("displayLargestArrivalGapMs=%1").arg(logged(display.largestArrivalGapMs))
           << QStringLiteral("displayDelayMs=%1").arg(logged(display.displayDelayMs))
           << QStringLiteral("displayItemsDropped=%1").arg(display.itemsDropped)
           << QStringLiteral("displayRowsDropped=%1").arg(display.rowsDropped)
           << QStringLiteral("coreTelemetryAgeMs=%1").arg(logged(m_station
                  ? std::optional<qint64>(qMax<qint64>(0, now - m_stationReceivedMs)) : std::nullopt))
           << QStringLiteral("coreSystemCpuPercent=%1").arg(logged(host.systemCpuPercent))
           // nereusd's share of all CPUs together, not top's per-core figure
           // (top's I toggle switches between the two).
           << QStringLiteral("coreProcessCpuPercentOfAllCpus=%1").arg(logged(host.processCpuPercent))
           << QStringLiteral("coreMemoryAvailableKiB=%1").arg(logged(host.memoryAvailableKiB))
           << QStringLiteral("coreMemoryTotalKiB=%1").arg(logged(host.memoryTotalKiB))
           << QStringLiteral("coreProcessResidentKiB=%1").arg(logged(host.processResidentKiB))
           << QStringLiteral("coreHottestZoneCelsius=%1").arg(logged(host.hottestZoneCelsius))
           << QStringLiteral("coreHottestZone=%1").arg(host.hottestZoneName.isEmpty()
                  ? QStringLiteral("not measured")
                  : QLatin1Char('"') + host.hottestZoneName.simplified() + QLatin1Char('"'));
    // R-R3-40: each Core receiver's processing load (percent of real time)
    // and the wait of its latest input, as the Core last reported them.
    const std::optional<QVector<StationReceiverTelemetry>> receivers =
        m_station ? m_station->receivers : std::nullopt;
    if (!receivers) {
        fields << QStringLiteral("coreReceivers=not measured");
    } else {
        fields << QStringLiteral("coreReceivers=%1").arg(receivers->size());
        for (const StationReceiverTelemetry& receiver : *receivers) {
            const QString slice = receiver.sliceId < 26
                ? QString(QChar(QLatin1Char('A').unicode() + receiver.sliceId))
                : QString::number(receiver.sliceId);
            fields << QStringLiteral("coreSlice%1LoadPercent=%2")
                          .arg(slice, logged(receiver.loadPercent))
                   << QStringLiteral("coreSlice%1InputDelayMs=%2")
                          .arg(slice).arg(receiver.inputDelayMs)
                   << QStringLiteral("coreSlice%1SkippedInputMs=%2")
                          .arg(slice).arg(receiver.skippedInputMs);
        }
    }
    qCInfo(lcRemoteTelemetry).noquote()
        << QStringLiteral("Remote diagnostics:") << fields.join(QLatin1Char(' '));
}

QString RemoteTelemetryController::bannerText() const
{
    QStringList parts;
    switch (m_view.state) {
    case RemoteTelemetryView::State::Disconnected: return {};
    case RemoteTelemetryView::State::Unsupported: parts << tr("measurements not offered"); break;
    case RemoteTelemetryView::State::Waiting: parts << tr("waiting for measurements"); break;
    case RemoteTelemetryView::State::Stale: parts << tr("measurements out of date"); break;
    case RemoteTelemetryView::State::Current:
        parts << (m_view.radio.connected
            ? tr("Radio ↓%1 ↑%2 Mbps").arg(number(m_view.radio.rxMbps), number(m_view.radio.txMbps))
            : tr("Radio offline"));
        break;
    }
    if (m_view.coreGuiTotalKbps) {
        const bool megabits = *m_view.coreGuiTotalKbps >= 1000.0;
        const double divisor = megabits ? 1000.0 : 1.0;
        const auto rateText = [divisor](std::optional<double> kbps) {
            return number(kbps ? std::optional<double>(*kbps / divisor) : std::nullopt);
        };
        parts << tr("Core ↓%1 ↑%2 total %3 %4")
            .arg(rateText(m_view.coreGuiRxKbps), rateText(m_view.coreGuiTxKbps),
                 rateText(m_view.coreGuiTotalKbps), megabits ? tr("Mbps") : tr("kbps"));
    }
    if (m_view.audioPayloadRxKbps) { parts << tr("Audio %1 kbps").arg(number(m_view.audioPayloadRxKbps)); }
    parts << tr("Core RTT %1 ms").arg(m_view.coreRttMs ? QString::number(*m_view.coreRttMs) : QStringLiteral("—"));
    // R-R3-23: with a media controller, its own persistent status (which
    // survives a fault the receiver does not recover from by itself) is the
    // banner word; the running/decoding heuristic below is only a fallback
    // for callers with no media controller, so existing deterministic
    // banner tests built without one keep their exact wording.
    parts << (m_media ? remoteAudioBannerWord(m_media->audioStatus().state)
        : m_view.playbackActive ? tr("Audio playing")
        : m_view.playback.running ? tr("Audio waiting") : tr("Audio stopped"));
    return parts.join(QStringLiteral("  ·  "));
}

QStringList RemoteTelemetryController::performanceOverlayLines(const RemoteTelemetryView& view)
{
    // The overlay's own terse columns (SpectrumWidget's perf overlay), each
    // value from the Core's latest telemetry; a missing one reads "-",
    // never 0.
    const auto value = [](std::optional<double> v, int decimals) {
        return v ? QString::number(*v, 'f', decimals) : QStringLiteral("-");
    };
    if (view.state != RemoteTelemetryView::State::Current) {
        return {QStringLiteral("the Core: no current readings")};
    }
    const StationRadioTelemetry& radio = view.radio;
    QStringList lines{QStringLiteral("the Core:")};
    lines << QStringLiteral("radio  lost %1% (5 s) gap %2 ms")
                 .arg(value(radio.packetLossPercent, 2), value(radio.packetGapMs, 1));
    if (radio.hl2SequenceGaps) {
        lines << QStringLiteral("radio  sequence gaps %1").arg(*radio.hl2SequenceGaps);
    }
    lines << QStringLiteral("audio  drops %1/s not sent %2/s")
                 .arg(value(view.coreAudio.sourceDropsPerSecond, 1),
                      value(view.coreAudio.sendRejectedPerSecond, 1));
    return lines;
}

std::optional<double> RemoteTelemetryController::coreCpuPercent(const RemoteTelemetryView& view,
                                                                bool system, QString* reason)
{
    if (reason) { reason->clear(); }
    if (view.state == RemoteTelemetryView::State::Unsupported) {
        if (reason) {
            *reason = QStringLiteral("This Core does not report its CPU. Updating the Core may help.");
        }
        return std::nullopt;
    }
    if (view.state != RemoteTelemetryView::State::Current) {
        if (reason) { *reason = QStringLiteral("The Core's CPU reading is not current."); }
        return std::nullopt;
    }
    const std::optional<double> value =
        system ? view.coreHost.systemCpuPercent : view.coreHost.processCpuPercent;
    if (!value && reason) {
        *reason = QStringLiteral("This Core does not measure its CPU.");
    }
    return value;
}

QString RemoteTelemetryController::detailText() const
{
    if (m_view.state == RemoteTelemetryView::State::Disconnected) { return tr("No current measurements while disconnected."); }
    QStringList text{bannerText()};
    if (m_view.stationAgeMs) { text << tr("Core measurements received %1 ms ago.").arg(*m_view.stationAgeMs); }
    text << tr("Radio rates: between the Core and the radio, in Mbps. Control traffic: between this app and the Core, not counting audio, display or network overhead.");
    text << tr("Control traffic received %1 / sent %2 kbit/s").arg(number(m_view.controlRxKbps), number(m_view.controlTxKbps));
    text << tr("Traffic seen by this app: Core→app %1 / app→Core %2 / total %3 kbps.")
        .arg(number(m_view.coreGuiRxKbps), number(m_view.coreGuiTxKbps), number(m_view.coreGuiTotalKbps));
    text << tr("Total includes control, display, audio, media transmit keepalive, raw I/Q, and separate transmit watch messages. Audio content received (Opus or lossless): %1 kbps, already included in total. No audio is sent to the Core in receive-only mode.")
        .arg(number(m_view.audioPayloadRxKbps));
    text << tr("Audio packets received: %1 kbps, including packet headers and packets this computer later dropped. Audio content counts the sound in the packets it accepted, including duplicates.")
        .arg(number(m_view.audioRtpRxKbps));
    text << tr("These counts exclude encryption, VPN and network overhead. Outgoing media counts valid sends attempted through the transport, including queued or failed library sends; that does not prove delivery.");
    text << (m_view.coreRttAgeMs ? tr("Core RTT: round trip to the Core and back, measured %1 ms ago.").arg(*m_view.coreRttAgeMs)
        : tr("Core RTT: not measured recently."));
    text << (m_view.radio.rttMs && m_view.radio.rttAgeMs
        ? tr("Radio RTT: %1 ms, measured %2 ms ago.").arg(*m_view.radio.rttMs).arg(*m_view.radio.rttAgeMs)
        : tr("Radio RTT: unavailable."));
    text << tr("RTT graphs hold the last measurement between pings; age advances independently and stale values disappear.");
    // R-R3-32 (parity Task 6): the Core's own readings of its radio, each
    // named as the Core's; an absent or out-of-date one says so, never 0.
    const StationRadioTelemetry& radio = m_view.radio;
    const QString unavailable = tr("unavailable");
    const auto measured = [&unavailable](std::optional<double> value, int decimals,
                                         const QString& unit) {
        return value ? QString::number(*value, 'f', decimals) + unit : unavailable;
    };
    text << tr("PA voltage from the Core: %1.").arg(measured(radio.paVolts, 1, tr("\u00A0V")));
    // Group A follow-up (group B fix wave): the AIN6 reading, named as
    // Thetis names it (setup.designer.cs:51365 [v2.10.3.15], "DC Voltage",
    // a label Thetis ships hidden, and computeHermesDCVoltage at
    // console.cs:24782 [v2.10.3.15], which computes the reading); on
    // MkII-class boards it is not the supply.
    text << tr("DC voltage from the Core: %1.").arg(measured(radio.supplyVolts, 1, tr("\u00A0V")));
    text << tr("Packet loss between the Core and the radio, from the Core: %1 over the last 5 seconds.")
        .arg(measured(radio.packetLossPercent, 2, tr("\u00A0%")));
    text << tr("Radio jitter from the Core: %1.").arg(measured(radio.jitterMs, 2, tr("\u00A0ms")));
    text << tr("Longest gap between radio packets in the last second, from the Core: %1.")
        .arg(measured(radio.packetGapMs, 1, tr("\u00A0ms")));
    text << tr("Radio sample rate from the Core: %1.")
        .arg(radio.sampleRateHz ? tr("%1\u00A0kHz").arg(double(*radio.sampleRateHz) / 1000.0, 0, 'g', 6)
                                : unavailable);
    text << tr("UDP packets seen from the radio since it connected, from the Core: %1.")
        .arg(radio.udpPacketsSeen ? QString::number(*radio.udpPacketsSeen) : unavailable);
    text << tr("Core audio: %1 frames/s; encoded %2, sent %3, not sent %4 packets/s; dropped before encoding %5 events/s.")
        .arg(number(m_view.coreAudio.sourceFramesPerSecond, 0), number(m_view.coreAudio.encodedPacketsPerSecond),
             number(m_view.coreAudio.sendAcceptedPerSecond), number(m_view.coreAudio.sendRejectedPerSecond),
             number(m_view.coreAudio.sourceDropsPerSecond));
    const auto& p = m_view.playback;
    text << tr("Speaker buffering on this computer: %1 ms (audio waiting for the speaker only). This excludes network, encoder, arrival smoothing and audio-device delay.")
        .arg(number(p.running ? p.speakerQueuedMs : std::nullopt));
    // R-R3-35: a Core that answers clock probes gets the measured delay; an
    // older one keeps exactly the line it always had.
    const RemoteAudioDelayReport& delay = m_view.audioDelay;
    if (!delay.measurable) {
        text << tr("End-to-end audio latency is not measured. Core RTT is a control round trip, not one-way audio latency; RTT/2 is not used.");
    } else if (delay.estimate) {
        text << tr("Audio delay: %1, from the Core's audio to this computer's speaker. The \u00B1 is how accurately this computer knows the Core's clock; half the round trip is never shown as the delay.")
            .arg(remoteAudioDelayText(*delay.estimate));
        const QString delivery = remoteAudioDeliveryText(*delay.estimate);
        text << (delivery.isEmpty()
            ? tr("Delivery delay: not measured.")
            : tr("Delivery delay: %1, from the Core's audio to this computer's player, before the speaker queue.").arg(delivery));
    } else {
        text << tr("Audio delay: not measured. It needs audio playing and answers from the Core.");
    }
    // Fix wave M1: "discarded before playback" is the connect-time backlog
    // trimmed before anything was heard; the receiver keeps it out of
    // "admitted", so the two counts do not overlap.
    text << tr("Audio stream %1: accepted %2, discarded before playback %3, decoded %4, concealed %5, late %6, invalid %7, duplicate %8, rejected headers %9.")
        .arg(p.generation).arg(p.acceptedPackets).arg(p.startDiscardedPackets)
        .arg(p.decodedPackets).arg(p.concealedPackets)
        .arg(p.latePackets).arg(p.invalidPackets).arg(p.duplicatePackets).arg(p.rejectedHeaders);
    text << tr("Playback underflows %1 / overflows %2; device consumed %3 frames; last accepted packet %4 ms ago.")
        .arg(p.underflows).arg(p.overflows).arg(p.deviceConsumedFrames)
        .arg(p.lastAdmittedPacketAgeMs ? QString::number(*p.lastAdmittedPacketAgeMs) : QStringLiteral("—"));
    if (p.lifetimeUnderflows && p.lifetimeOverflows) {
        text << tr("Playback interruptions since this app started: %1 underflows / %2 overflows, including earlier audio streams.")
            .arg(*p.lifetimeUnderflows).arg(*p.lifetimeOverflows);
    }
    // R-R3-23: each measurement labelled with what it is, not protocol jargon.
    // U+00A0 between each number and its unit keeps them on one line.
    text << (p.arrivalJitterMs
        ? tr("Arrival jitter: %1\u00A0ms, measured on this computer.").arg(qRound(*p.arrivalJitterMs))
        : tr("Arrival jitter: not measured."));
    text << (p.expectedPackets > 0
        ? tr("Missing packets: %1 of %2, sequence numbers never received.")
              .arg(p.missingPackets).arg(p.expectedPackets)
        : tr("Missing packets: none received."));
    text << tr("Gaps filled: %1, concealed 40\u00A0ms intervals.").arg(p.concealedPackets);
    text << (p.speakerQueuedMs
        ? tr("Speaker buffer: %1\u00A0ms, audio queued for this computer's speaker, not total delay.")
              .arg(qRound(*p.speakerQueuedMs))
        : tr("Speaker buffer: not measured."));
    text << (p.reorderQueuedMs
        ? tr("Reorder buffer: %1\u00A0ms on this computer, packets held so that late arrivals play in order.")
              .arg(qRound(*p.reorderQueuedMs))
        : tr("Reorder buffer: not measured."));
    // R-R3-21: the adaptive hold behind the reorder buffer.
    if (p.jitterHoldMs) {
        text << tr("Network buffer: %1\u00A0ms on this computer, how long arriving audio waits for late packets; it deepens after late packets and eases back when the link is steady.")
            .arg(qRound(*p.jitterHoldMs));
    }
    if (p.linkInterruptions > 0 || p.skippedIntervals > 0) {
        text << tr("Network stalls, gaps and bursts ridden through without restarting the audio: %1. Audio lost at a burst: %2\u00A0ms. Audio skipped to bring the delay back down: %3\u00A0ms.")
            .arg(p.linkInterruptions).arg(qRound(p.burstDroppedAudioMs))
            .arg(qRound(p.skippedAudioMs));
    }
    // R-R3-07: the receiver reports its clock correction as a ratio near
    // 1.0; shown here as (ratio - 1) x 1e6 parts per million.
    text << (p.driftRatio
        ? tr("Clock drift: %1\u00A0parts per million, the rate correction this computer applies to match the Core's audio clock.")
              .arg(qRound((*p.driftRatio - 1.0) * 1'000'000.0))
        : tr("Clock drift: not measured."));
    text << tr("Sent does not prove delivered. Gaps filled and drops before encoding are events, not a packet-loss percentage.");
    return text.join(QLatin1Char('\n'));
}
} // namespace NereusSDR
