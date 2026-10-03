// =================================================================
// src/core/session/media/DaemonAudioSender.cpp  (NereusSDR)
// =================================================================

#include "DaemonAudioSender.h"
#include "core/AudioEngine.h"

#include "core/session/media/DaemonAudioSource.h"
#include "core/session/media/OpusAudioCodec.h"

#include <QElapsedTimer>

#include <algorithm>
#include <memory>

namespace NereusSDR {

static_assert(PcmAudioCodecConfig::kBlockFrames == DaemonAudioSource::kBlockFrames,
              "a lossless block must be the capture block the source delivers");

DaemonAudioSender::DaemonAudioSender(AudioEngine* audioEngine, QObject* parent)
    : DaemonAudioSender(audioEngine, OpusAudioCodecConfig{}, parent)
{
}

DaemonAudioSender::DaemonAudioSender(AudioEngine* audioEngine,
                                     const OpusAudioCodecConfig& codecConfig,
                                     QObject* parent)
    : QObject(parent)
    , m_source(std::make_unique<DaemonAudioSource>())
    , m_encoder(std::make_unique<OpusAudioEncoder>(codecConfig))
{
    m_source->setAudioEngine(audioEngine);
    m_pacingClock = [clock = std::make_shared<QElapsedTimer>()] {
        if (!clock->isValid()) { clock->start(); }
        return clock->nsecsElapsed();
    };
    m_drainTimer.setInterval(kDrainIntervalMs);
    m_drainTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_drainTimer, &QTimer::timeout, this, &DaemonAudioSender::drain);
}

DaemonAudioSender::~DaemonAudioSender()
{
    stop();
}

bool DaemonAudioSender::start(quint32 ssrc, quint16 firstSequence,
                              quint32 firstTimestamp)
{
    // A start always defines a new capture/codec epoch, including a caller
    // reusing this object after an interrupted client connection.
    stop();
    if (ssrc == 0 || !profileReady() || m_source->audioEngine() == nullptr) {
        return false;
    }

    m_encoder->reset();
    m_pendingLossless.clear();
    m_lastPacingNs = m_pacingClock();
    m_pacingAllowanceUnits = 0;
    m_ssrc = ssrc;
    m_nextSequence = firstSequence;
    m_baseTimestamp = firstTimestamp;
    m_nextTimestamp = firstTimestamp;
    m_source->start();
    if (!m_source->isRunning()) {
        m_ssrc = 0;
        return false;
    }
    m_telemetry = {};
    ++m_lifecycleGeneration;
    m_running = true;
    m_drainTimer.start();
    return true;
}

void DaemonAudioSender::stop()
{
    ++m_lifecycleGeneration;
    m_drainTimer.stop();
    m_running = false;
    m_ssrc = 0;
    m_pendingLossless.clear();
    if (m_source) {
        m_source->stop();
    }
}

bool DaemonAudioSender::isRunning() const noexcept
{
    return m_running && m_source && m_source->isRunning();
}

bool DaemonAudioSender::setProfile(RemoteAudioProfile profile)
{
    if (m_running) {
        return false;
    }
    m_profile = profile;
    return true;
}

bool DaemonAudioSender::setSliceSource(int sliceId)
{
    if (m_running) {
        return false;
    }
    return m_source->setSliceSource(sliceId);
}

bool DaemonAudioSender::setOwnerMix(int slot)
{
    if (m_running) {
        return false;
    }
    return m_source->setOwnerMix(slot);
}

int DaemonAudioSender::sliceSource() const noexcept
{
    return m_source->sliceSource();
}

void DaemonAudioSender::setPacingClockForTest(PacingClock clock)
{
    if (clock) { m_pacingClock = std::move(clock); }
}

bool DaemonAudioSender::setCaptureClock(DaemonAudioSource::CaptureClock clock)
{
    return m_source->setCaptureClock(std::move(clock));
}

void DaemonAudioSender::noteCaptured(const DaemonAudioBlock& block, quint32 timestamp)
{
    m_telemetry.hasCaptureStamp = true;
    m_telemetry.captureTimestamp = timestamp + static_cast<quint32>(DaemonAudioSource::kBlockFrames);
    m_telemetry.captureNs = block.capturedNs;
}

bool DaemonAudioSender::profileReady() const
{
    return m_profile == RemoteAudioProfile::Lossless
        ? m_packetiser.isReady() : (m_encoder && m_encoder->isReady());
}

std::optional<OpusEncoderProfile> DaemonAudioSender::encoderProfile() const
{
    return m_encoder ? m_encoder->profile() : std::nullopt;
}

DaemonAudioSenderTelemetry DaemonAudioSender::telemetry() const noexcept
{
    DaemonAudioSenderTelemetry snapshot = m_telemetry;
    if (m_source) {
        snapshot.source = m_source->telemetry();
    }
    return snapshot;
}

void DaemonAudioSender::drain()
{
    if (!isRunning()) {
        return;
    }
    const quint64 drainGeneration = m_lifecycleGeneration;
    if (m_profile == RemoteAudioProfile::Lossless) {
        drainLossless(drainGeneration);
        return;
    }

    for (int count = 0; count < kMaxBlocksPerDrain; ++count) {
        const std::optional<DaemonAudioBlock> block = m_source->takeBlock();
        if (!block.has_value()) {
            return;
        }
        ++m_telemetry.consumedBlocks;

        // The capture position, rather than timer cadence, is the RTP clock.
        // Thus source queue loss remains visible as a timestamp gap.  The
        // unsigned addition intentionally supplies normal RTP wraparound.
        const quint32 timestamp = m_baseTimestamp
            + static_cast<quint32>(block->samplePosition);
        m_nextTimestamp = timestamp + DaemonAudioSource::kBlockFrames;
        noteCaptured(*block, timestamp);
        const OpusRtpEncodeResult encoded = m_encoder->encode(
            block->pcmInterleaved, m_nextSequence, timestamp, m_ssrc);
        if (encoded.status != OpusAudioCodecStatus::Accepted) {
            ++m_telemetry.encodeFailures;
            continue;
        }
        ++m_telemetry.encodedPackets;
        m_telemetry.hasLastEmittedPacket = true;
        m_telemetry.lastEmittedSequence = m_nextSequence;
        m_telemetry.lastEmittedTimestamp = timestamp;
        ++m_nextSequence;
        emit packetReady(encoded.packet);
        // A direct packetReady recipient may stop the sender or start a
        // new capture epoch. Do not consume old queued PCM into that new
        // epoch.
        if (m_lifecycleGeneration != drainGeneration || !isRunning()) {
            return;
        }
    }
}

void DaemonAudioSender::drainLossless(quint64 drainGeneration)
{
    // The allowance this tick has earned since the last one, capped.
    const qint64 now = m_pacingClock();
    const qint64 elapsed = std::max<qint64>(0, now - m_lastPacingNs);
    m_lastPacingNs = now;
    // Whole numbers: one packet is 10 ms (1e7 ns) of allowance per
    // kLosslessPacketsPer10Ms, so each elapsed nanosecond earns that many
    // units and a packet costs 1e7. Bounded well inside qint64: the cap is
    // applied to each tick and a tick's elapsed time is added only up to it.
    constexpr qint64 kUnitsPerPacket = 10'000'000;
    constexpr qint64 kCapUnits = qint64(kMaxLosslessPacketsPerDrain) * kUnitsPerPacket;
    const qint64 earned = std::min<qint64>(elapsed, kCapUnits) * kLosslessPacketsPer10Ms;
    const bool lateTick = earned > kCapUnits;
    m_pacingAllowanceUnits = std::min<qint64>(kCapUnits, m_pacingAllowanceUnits + earned);
    const int allowed = static_cast<int>(m_pacingAllowanceUnits / kUnitsPerPacket);
    for (int sent = 0; sent < allowed;) {
        if (m_pendingLossless.isEmpty()) {
            const std::optional<DaemonAudioBlock> block = m_source->takeBlock();
            if (!block.has_value()) {
                return;
            }
            ++m_telemetry.consumedBlocks;
            // As for Opus, the capture position is the RTP clock.
            const quint32 timestamp = m_baseTimestamp
                + static_cast<quint32>(block->samplePosition);
            m_nextTimestamp = timestamp + DaemonAudioSource::kBlockFrames;
            noteCaptured(*block, timestamp);
            // R-R3-23: ten 192-frame packets per block, sequence +1 and
            // timestamp +192 each, so the next block continues the clock.
            // Sequence numbers are given out here, so nextSequence() is the
            // first number after the block even while some of it waits.
            const QList<QByteArray> packets = m_packetiser.packetiseBlock(
                block->pcmInterleaved, m_nextSequence, timestamp, m_ssrc);
            if (packets.isEmpty()) {
                ++m_telemetry.encodeFailures;
                continue;
            }
            for (qsizetype index = 0; index < packets.size(); ++index) {
                m_pendingLossless.append({packets.at(index), m_nextSequence,
                    timestamp + static_cast<quint32>(index * PcmAudioCodecConfig::kPacketFrames)});
                ++m_nextSequence;
                ++m_telemetry.encodedPackets;
            }
        }
        const PendingPacket next = m_pendingLossless.takeFirst();
        m_telemetry.hasLastEmittedPacket = true;
        m_telemetry.lastEmittedSequence = next.sequence;
        m_telemetry.lastEmittedTimestamp = next.timestamp;
        ++sent;
        m_pacingAllowanceUnits -= kUnitsPerPacket;
        emit packetReady(next.packet);
        // A direct packetReady recipient may stop the sender or start a new
        // capture epoch (which discards what is still waiting). Send nothing
        // more of this block into that epoch.
        if (m_lifecycleGeneration != drainGeneration || !isRunning()) {
            return;
        }
    }
    // The tick earned more than the cap by itself (it came over 20 ms after
    // the last) and sent the whole cap: the timer ran late with audio
    // waiting. An on-time tick never counts, even when idle ticks have let
    // the allowance reach the cap.
    if (lateTick && allowed == kMaxLosslessPacketsPerDrain) {
        ++m_telemetry.losslessCappedTicks;
    }
}

} // namespace NereusSDR
