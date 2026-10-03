// =================================================================
// src/gui/RemoteAudioStatus.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See RemoteAudioStatus.h.
// =================================================================

#include "gui/RemoteAudioStatus.h"
#include "gui/OperatorReasonText.h"
#include "gui/RemoteGeneration.h"
#include "core/session/media/AudioJitterBuffer.h"

#include <QStringList>

#include <algorithm>
#include <utility>

namespace NereusSDR {

RemoteAudioStatus::State deriveRemoteAudioState(const RemoteAudioStatusInputs& in)
{
    using State = RemoteAudioStatus::State;
    if (!in.mediaSession) {
        return State::NotConnected;
    }
    if (in.muted) {
        return State::MutedHere;
    }
    if (in.problem) {
        return State::PlaybackProblem;
    }
    const std::optional<RemoteAudioOffReason> reason =
        in.context ? in.context->offReason : std::nullopt;
    // Core may say radio-offline before this GUI mirrors the radio. Once it
    // does, this GUI withdraws its own request and Core answers
    // client-disabled, so the mirror has to count as well as the reason.
    if (!in.radioConnected || reason == RemoteAudioOffReason::RadioOffline) {
        return State::RadioOffline;
    }
    if (in.context && !in.context->enabled
        && reason == RemoteAudioOffReason::EncoderUnavailable) {
        return State::CoreCouldNotStart;
    }
    if (in.restarting) {
        return State::Reconnecting;
    }
    if (!in.context || !in.context->enabled) {
        return State::WaitingForAudio;
    }
    if (!in.receiverRunning) {
        return State::WaitingForAudio;
    }
    if (in.playing) {
        return State::Playing;
    }
    return State::Starting;
}

QString remoteAudioHeadline(RemoteAudioStatus::State state)
{
    using State = RemoteAudioStatus::State;
    switch (state) {
    case State::NotConnected:
        return QStringLiteral("Not connected to a Core");
    case State::WaitingForAudio:
        return QStringLiteral("Waiting for audio from Core");
    case State::MutedHere:
        return QStringLiteral("Muted on this computer");
    case State::RadioOffline:
        return QStringLiteral("Radio offline at the Core");
    case State::CoreCouldNotStart:
        return QStringLiteral("Core could not start audio");
    case State::Starting:
        return QStringLiteral("Starting audio");
    case State::Playing:
        return QStringLiteral("Playing");
    case State::Reconnecting:
        return QStringLiteral("Audio interrupted, reconnecting");
    case State::PlaybackProblem:
        return QStringLiteral("Playback problem on this computer");
    }
    return {};
}

QString remoteAudioBannerWord(RemoteAudioStatus::State state)
{
    using State = RemoteAudioStatus::State;
    switch (state) {
    case State::Playing:
        return QStringLiteral("Audio playing");
    case State::MutedHere:
        return QStringLiteral("Audio muted");
    case State::RadioOffline:
        return QStringLiteral("Radio offline");
    case State::CoreCouldNotStart:
    case State::PlaybackProblem:
        return QStringLiteral("Audio unavailable");
    case State::WaitingForAudio:
    case State::Starting:
    case State::Reconnecting:
        return QStringLiteral("Audio waiting");
    case State::NotConnected:
        return QStringLiteral("Audio stopped");
    }
    return {};
}

QString remoteAudioProblemText(RemoteAudioReceiver::Fault fault)
{
    using Fault = RemoteAudioReceiver::Fault;
    switch (fault) {
    case Fault::SpeakerOpenFailed:
        return QStringLiteral("The selected speaker device could not be opened.");
    case Fault::SpeakerTimingUnavailable:
        return QStringLiteral("The speaker device stopped reporting its timing.");
    case Fault::SpeakerCallbackTooLarge:
        return QStringLiteral("The speaker device buffer is larger than remote playback "
                              "supports. Choose a smaller buffer or another device.");
    case Fault::SpeakerStalled:
        return QStringLiteral("The speaker device stopped playing audio.");
    case Fault::SpeakerWriteFailed:
        return QStringLiteral("Audio could not be sent to the speaker device.");
    case Fault::DecoderUnavailable:
        return QStringLiteral("The audio decoder could not start on this computer.");
    case Fault::NoPackets:
    case Fault::DecodeFailed:
    case Fault::ClockBuffer:
        // The receiver restarts itself after these; none of them persists.
        return QStringLiteral("Audio was interrupted.");
    }
    return {};
}

namespace {
QString channelWords(int channels)
{
    if (channels == 2) {
        return QStringLiteral("stereo");
    }
    if (channels == 1) {
        return QStringLiteral("mono");
    }
    return QStringLiteral("%1\u00A0channels").arg(channels);
}
} // namespace

QString remoteAudioCodecText(const RemoteAudioStatus& status)
{
    if (!status.detailNegotiated) {
        return QStringLiteral("Not reported by this Core");
    }
    if (status.losslessEncoder) {
        const PcmEncoderProfile& lossless = *status.losslessEncoder;
        const qint64 packetMs = lossless.sampleRate > 0
            ? qint64(lossless.frameSamples) * 1000 / lossless.sampleRate : 0;
        const qint64 kbps = qint64(lossless.sampleRate) * lossless.channels
            * lossless.bitsPerSample / 1000;
        // U+00A0 between each number and its unit keeps them on one line.
        return QStringLiteral("Lossless %1, %2-bit, %3\u00A0kbit/s, %4\u00A0ms packets")
            .arg(channelWords(lossless.channels))
            .arg(lossless.bitsPerSample)
            .arg(kbps)
            .arg(packetMs);
    }
    if (!status.encoder) {
        return QStringLiteral("Audio is off");
    }
    const OpusEncoderProfile& profile = *status.encoder;
    const QString channels = channelWords(profile.channels);
    const qint64 packetMs = profile.sampleRate > 0
        ? qint64(profile.frameSamples) * 1000 / profile.sampleRate : 0;
    // A target, not measured traffic: constrained VBR spends less on quiet audio.
    // U+00A0 between each number and its unit keeps them on one line.
    return QStringLiteral("Opus %1, %2\u00A0kbit/s target, %3\u00A0ms packets, audio up to %4\u00A0kHz")
        .arg(channels)
        .arg(profile.targetBitrate / 1000)
        .arg(packetMs)
        .arg(profile.audioBandwidthHz / 1000);
}

QString remoteAudioQualityChoiceName(RemoteAudioQualityChoice choice)
{
    switch (choice) {
    case RemoteAudioQualityChoice::High: return QStringLiteral("High");
    case RemoteAudioQualityChoice::SaveData: return QStringLiteral("Save data");
    case RemoteAudioQualityChoice::Lossless: return QStringLiteral("Lossless");
    }
    return {};
}

QString remoteAudioProfileName(RemoteAudioProfile profile)
{
    return profile == RemoteAudioProfile::Lossless ? QStringLiteral("Lossless")
                                                   : QStringLiteral("Opus");
}

QString remoteAudioQualityReasonText(RemoteAudioQualityReason reason)
{
    switch (reason) {
    case RemoteAudioQualityReason::CoreCannotSend:
        return QStringLiteral("This Core cannot send lossless audio.");
    case RemoteAudioQualityReason::CoreNotAllowed:
        return QStringLiteral("This Core does not allow lossless audio.");
    case RemoteAudioQualityReason::ConnectionUnavailable:
        return QStringLiteral("This connection could not set up lossless audio; staying on Opus.");
    case RemoteAudioQualityReason::NetworkTooSlow:
        return QStringLiteral("The network could not carry lossless audio; staying on Opus.");
    }
    return {};
}

bool remoteReceiverAudioIsCompressed(const RemoteAudioStatus& status)
{
    if (status.state == RemoteAudioStatus::State::NotConnected) {
        return false;
    }
    if (status.qualityReason) {
        return true;  // Lossless chosen, Opus runs
    }
    bool anyRunning = false;
    for (const RemoteReceiverAudioStatus& receiver : status.receivers) {
        if (receiver.state == RemoteReceiverAudioStatus::State::Receiving
            && receiver.runningProfile) {
            if (*receiver.runningProfile == RemoteAudioProfile::Opus) {
                return true;
            }
            anyRunning = true;
        }
    }
    if (anyRunning) {
        return false;
    }
    if (status.runningProfile) {
        return *status.runningProfile == RemoteAudioProfile::Opus;
    }
    return status.chosenProfile == RemoteAudioProfile::Opus;
}

RemoteReceiverAudioNote remoteReceiverAudioNote(const RemoteAudioStatus& status,
                                                bool receiverAudioNegotiated)
{
    if (!receiverAudioNegotiated || !remoteReceiverAudioIsCompressed(status)) {
        return RemoteReceiverAudioNote::None;
    }
    return status.chosenProfile == RemoteAudioProfile::Lossless
        ? RemoteReceiverAudioNote::LosslessUnavailable
        : RemoteReceiverAudioNote::OpusChosen;
}

QString remoteAudioQualityText(const RemoteAudioStatus& status)
{
    if (status.runningProfile) {
        return remoteAudioProfileName(*status.runningProfile);
    }
    return QStringLiteral("%1 (chosen)").arg(remoteAudioProfileName(status.chosenProfile));
}

QString remoteAudioDelayText(const AudioDelayEstimate& estimate)
{
    const AudioDelayDisplay shown = roundAudioDelay(estimate.delayMs, estimate.boundMs);
    // U+00A0 between each number and its unit keeps them on one line.
    const QString text = QStringLiteral("%1\u00A0ms \u00B1 %2\u00A0ms")
                             .arg(shown.valueMs).arg(shown.accuracyMs);
    return estimate.includesDevice ? text
                                   : text + QStringLiteral(", not counting the speaker device");
}

QString remoteAudioDeliveryText(const AudioDelayEstimate& estimate)
{
    if (!estimate.deliveryMs || !estimate.deliveryBoundMs) {
        return {};
    }
    const AudioDelayDisplay shown = roundAudioDelay(*estimate.deliveryMs, *estimate.deliveryBoundMs);
    return QStringLiteral("%1\u00A0ms \u00B1 %2\u00A0ms").arg(shown.valueMs).arg(shown.accuracyMs);
}

QString remoteReceiverAudioStateText(const RemoteReceiverAudioStatus& receiver)
{
    using State = RemoteReceiverAudioStatus::State;
    switch (receiver.state) {
    case State::Waiting:
        return QStringLiteral("Waiting for the Core");
    case State::Receiving:
        if (!receiver.runningProfile) {
            return QStringLiteral("Receiving");
        }
        if (*receiver.runningProfile == RemoteAudioProfile::Opus && receiver.encoder
            && receiver.encoder->targetBitrate > 0) {
            // The rate the Core reports for this stream, never one assumed
            // here. U+00A0 keeps the number and its unit on one line.
            return QStringLiteral("Receiving, Opus %1\u00A0kbit/s")
                .arg(receiver.encoder->targetBitrate / 1000);
        }
        return QStringLiteral("Receiving, %1").arg(remoteAudioProfileName(*receiver.runningProfile));
    case State::Stopped:
        return QStringLiteral("Stopped. %1")
            .arg(OperatorReasonText::forDisplay(receiver.stopReason));
    }
    return {};
}

QString formatRemoteAudioDetails(const RemoteAudioStatus& status,
                                 const RemoteAudioReceiverTelemetry& playback,
                                 const RemoteAudioDelayReport& delay,
                                 const QHash<int, RemoteAudioReceiverTelemetry>& receiverPlayback)
{
    using State = RemoteAudioStatus::State;
    QStringList lines;
    lines << QStringLiteral("Remote audio: %1").arg(remoteAudioHeadline(status.state));
    if (status.problem) {
        lines << QStringLiteral("Problem: %1").arg(remoteAudioProblemText(*status.problem));
    }
    const QString requested = status.chosenProfile == RemoteAudioProfile::Lossless
        ? QStringLiteral("Lossless") : remoteAudioQualityChoiceName(status.chosenQuality);
    lines << QStringLiteral("Requested quality: %1").arg(requested);
    if (status.qualityReason) {
        lines << remoteAudioQualityReasonText(*status.qualityReason);
    }
    if (!status.bitrateRefusal.isEmpty()) {
        lines << status.bitrateRefusal;
    }
    lines << QStringLiteral("Current receive format: %1").arg(remoteAudioCodecText(status));
    if (status.headphonesFormat) {
        lines << QStringLiteral("Current headphones format: %1").arg(*status.headphonesFormat);
    }
    lines << QStringLiteral("Current microphone format: %1").arg(status.microphoneFormat);
    lines << QStringLiteral("Output: %1 (selected)").arg(status.selectedOutput);

    const bool showHealth = status.state != State::NotConnected
        && status.state != State::MutedHere && status.state != State::RadioOffline;
    if (showHealth) {
        // U+00A0 between each number and its unit keeps them on one line.
        lines << (playback.arrivalJitterMs
            ? QStringLiteral("Arrival jitter: %1\u00A0ms").arg(qRound(*playback.arrivalJitterMs))
            : QStringLiteral("Arrival jitter: not measured"));
        lines << (playback.expectedPackets > 0
            ? QStringLiteral("Missing packets: %1 of %2")
                  .arg(playback.missingPackets).arg(playback.expectedPackets)
            : QStringLiteral("Missing packets: none received"));
        lines << QStringLiteral("Gaps filled: %1").arg(playback.concealedPackets);
        lines << (playback.speakerQueuedMs
            ? QStringLiteral("Speaker buffer: %1\u00A0ms on this computer")
                  .arg(qRound(*playback.speakerQueuedMs))
            : QStringLiteral("Speaker buffer: not measured"));
        // R-R3-21: how long arriving audio is held against late packets.
        // It deepens after late packets and eases back on a steady link,
        // so a rise in the delay below has its reason in plain sight.
        if (playback.jitterHoldMs) {
            lines << (*playback.jitterHoldMs > double(AudioJitterBuffer::kHoldNs) / 1e6 + 0.5
                ? QStringLiteral("Network buffer: %1\u00A0ms on this computer, deepened after late packets")
                      .arg(qRound(*playback.jitterHoldMs))
                : QStringLiteral("Network buffer: %1\u00A0ms on this computer")
                      .arg(qRound(*playback.jitterHoldMs)));
        }
        // R-R3-35: only a Core that answers clock probes adds this line.
        if (delay.measurable) {
            lines << (delay.estimate
                ? QStringLiteral("Audio delay: %1").arg(remoteAudioDelayText(*delay.estimate))
                : QStringLiteral("Audio delay: not measured"));
        }
    }
    // R-R3-43: each receiver's own stream to apps, apart from the speakers.
    for (const RemoteReceiverAudioStatus& receiver : status.receivers) {
        // The letter the operator sees for the slice (RadioModel.cpp's
        // receiverLetter): slice id 0 is receiver A.
        const QChar letter(QLatin1Char(static_cast<char>('A' + std::clamp(receiver.sliceId, 0, 25))));
        lines << QStringLiteral("Receiver %1 for apps: %2")
                     .arg(letter, remoteReceiverAudioStateText(receiver));
        if (receiver.state != RemoteReceiverAudioStatus::State::Receiving) {
            continue;
        }
        const auto measured = receiverPlayback.constFind(receiver.sliceId);
        if (measured == receiverPlayback.constEnd()) {
            continue;
        }
        const RemoteAudioReceiverTelemetry& stream = *measured;
        // U+00A0 between each number and its unit keeps them on one line.
        lines << QStringLiteral("Receiver %1: arrival jitter %2, missing packets %3, "
                                "gaps filled %4")
                     .arg(letter,
                          stream.arrivalJitterMs
                              ? QStringLiteral("%1\u00A0ms").arg(qRound(*stream.arrivalJitterMs))
                              : QStringLiteral("not measured"),
                          stream.expectedPackets > 0
                              ? QStringLiteral("%1 of %2").arg(stream.missingPackets)
                                    .arg(stream.expectedPackets)
                              : QStringLiteral("none received"))
                     .arg(stream.concealedPackets);
    }
    return lines.join(QLatin1Char('\n'));
}

bool remoteAudioFailureRecovered(const RemoteAudioFailure& failure, quint32 epoch,
                                 const QString& connectionId,
                                 const std::optional<RemoteAudioContextMessage>& context,
                                 const RemoteAudioReceiverTelemetry& playback)
{
    return failure.epoch == epoch && failure.connectionId == connectionId
        && context && isNewerGeneration(context->generation, failure.contextGeneration)
        && playback.running && playback.generation > failure.receiverGeneration
        && playback.deviceConsumedFrames > 0;
}

void RemoteAudioLinkTrial::begin(qint64 nowMs)
{
    *this = RemoteAudioLinkTrial{};
    m_active = true;
    m_windowStartMs = nowMs;
}

void RemoteAudioLinkTrial::end()
{
    *this = RemoteAudioLinkTrial{};
}

RemoteAudioLinkTrial::Verdict RemoteAudioLinkTrial::observe(
    qint64 nowMs, const RemoteAudioReceiverTelemetry& playback)
{
    return observe(nowMs, std::vector<StreamSample>{StreamSample{0, playback}});
}

RemoteAudioLinkTrial::Verdict RemoteAudioLinkTrial::observe(
    qint64 nowMs, const std::vector<StreamSample>& streams)
{
    if (!m_active) {
        return Verdict::Continue;
    }
    // A stream no longer sampled is forgotten, so a new one on its key
    // counts from zero.
    for (auto it = m_streams.begin(); it != m_streams.end();) {
        const bool sampled = std::any_of(streams.cbegin(), streams.cend(),
            [&](const StreamSample& sample) { return sample.stream == it->first; });
        it = sampled ? std::next(it) : m_streams.erase(it);
    }
    bool interrupted = false;
    for (const StreamSample& sample : streams) {
        const RemoteAudioReceiverTelemetry& playback = sample.playback;
        if (!playback.running) {
            continue;
        }
        auto [found, fresh] = m_streams.try_emplace(sample.stream);
        StreamBase& stream = found->second;
        if (fresh || stream.generation != playback.generation) {
            // A fresh receiver counts from zero.
            stream.generation = playback.generation;
            stream.base = {};
        }
        const Counters now{playback.expectedPackets, playback.missingPackets,
                           playback.concealedPackets,
                           playback.decodedPackets + playback.concealedPackets,
                           playback.linkInterruptions};
        const auto grow = [](quint64 value, quint64 base) {
            return value > base ? value - base : 0;
        };
        m_window.expected += grow(now.expected, stream.base.expected);
        m_window.missing += grow(now.missing, stream.base.missing);
        m_window.concealed += grow(now.concealed, stream.base.concealed);
        m_window.played += grow(now.played, stream.base.played);
        // R-R3-21: a burst, gap or stall ridden through since the last
        // sample counts as the restart it used to cause.
        interrupted = interrupted || grow(now.interruptions, stream.base.interruptions) > 0;
        stream.base = now;
    }
    if (interrupted && noteInterruption(nowMs) == Verdict::Failed) {
        return Verdict::Failed;
    }
    if (nowMs - m_windowStartMs < kWindowMs) {
        return Verdict::Continue;
    }
    const Counters window = std::exchange(m_window, Counters{});
    m_windowStartMs = nowMs;
    if (window.expected == 0 && window.played == 0) {
        // Nothing played this window (between contexts): no verdict on it.
        return Verdict::Continue;
    }
    const double missing = window.expected > 0
        ? double(window.missing) / double(window.expected) : 0.0;
    const double concealed = window.played > 0
        ? double(window.concealed) / double(window.played) : 0.0;
    const double loss = std::max(missing, concealed);
    m_lastWindowLoss = loss;
    const bool first = m_closedWindows == 0;
    ++m_closedWindows;
    if (first) {
        return loss > kTrialLossLimit ? Verdict::Failed : Verdict::Continue;
    }
    m_badWindows = loss > kSustainedLossLimit ? m_badWindows + 1 : 0;
    return m_badWindows >= kSustainedWindows ? Verdict::Failed : Verdict::Continue;
}

RemoteAudioLinkTrial::Verdict RemoteAudioLinkTrial::noteInterruption(qint64 nowMs)
{
    if (!m_active) {
        return Verdict::Continue;
    }
    while (!m_interruptions.empty() && nowMs - m_interruptions.front() >= kRestartSpanMs) {
        m_interruptions.pop_front();
    }
    m_interruptions.push_back(nowMs);
    const int limit = m_closedWindows == 0 ? kTrialRestartLimit : kSustainedRestartLimit;
    m_failedOnInterruptions = int(m_interruptions.size()) >= limit;
    return m_failedOnInterruptions ? Verdict::Failed : Verdict::Continue;
}

} // namespace NereusSDR
