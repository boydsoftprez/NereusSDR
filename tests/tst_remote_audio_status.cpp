// =================================================================
// tests/tst_remote_audio_status.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-23: the remote audio status
// derivation table, the failure-clearing rule, and the exact wording the
// Core connection panel and title bar show.
//
// =================================================================

#include <QtTest>

#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteAudioReceiver.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteGeneration.h"
#include "OperatorWording.h"

#include <QJsonObject>
#include <QRegularExpression>

#include <cmath>
#include <limits>
#include <optional>

using namespace NereusSDR;

namespace NereusSDR {

// Readable QCOMPARE failures for the state (found by ADL).
char* toString(RemoteAudioStatus::State state)
{
    const char* name = "unknown";
    switch (state) {
    case RemoteAudioStatus::State::NotConnected: name = "NotConnected"; break;
    case RemoteAudioStatus::State::WaitingForAudio: name = "WaitingForAudio"; break;
    case RemoteAudioStatus::State::MutedHere: name = "MutedHere"; break;
    case RemoteAudioStatus::State::RadioOffline: name = "RadioOffline"; break;
    case RemoteAudioStatus::State::CoreCouldNotStart: name = "CoreCouldNotStart"; break;
    case RemoteAudioStatus::State::Starting: name = "Starting"; break;
    case RemoteAudioStatus::State::Playing: name = "Playing"; break;
    case RemoteAudioStatus::State::Reconnecting: name = "Reconnecting"; break;
    case RemoteAudioStatus::State::PlaybackProblem: name = "PlaybackProblem"; break;
    }
    return qstrdup(name);
}

} // namespace NereusSDR

namespace {

using State = RemoteAudioStatus::State;
using Fault = RemoteAudioReceiver::Fault;

// What a default Core announces: 48 kHz stereo, 40 ms, 24 kbit/s, 8 kHz.
OpusEncoderProfile defaultProfile()
{
    OpusEncoderProfile profile;
    profile.sampleRate = 48'000;
    profile.channels = 2;
    profile.frameSamples = 1'920;
    profile.targetBitrate = 24'000;
    profile.audioBandwidthHz = 8'000;
    return profile;
}

enum class ContextKind {
    None,             // nothing accepted yet
    Enabled,          // minor 8, with the encoder
    EnabledLegacy,    // minor 7, no encoder
    DisabledLegacy,   // minor 7, no reason
    ClientDisabled,
    MediaNotReady,
    RadioOffline,
    EncoderUnavailable,
};

std::optional<RemoteAudioContextMessage> contextOf(ContextKind kind)
{
    if (kind == ContextKind::None) {
        return std::nullopt;
    }
    RemoteAudioContextMessage context;
    context.connectionId = QStringLiteral("00000000-0000-4000-8000-000000000001");
    context.revision = 3;
    context.generation = 7;
    context.ssrc = 0x1234'5678;
    context.enabled = kind == ContextKind::Enabled || kind == ContextKind::EnabledLegacy;
    if (kind == ContextKind::Enabled) {
        context.encoder = defaultProfile();
    }
    switch (kind) {
    case ContextKind::ClientDisabled:
        context.offReason = RemoteAudioOffReason::ClientDisabled;
        break;
    case ContextKind::MediaNotReady:
        context.offReason = RemoteAudioOffReason::MediaNotReady;
        break;
    case ContextKind::RadioOffline:
        context.offReason = RemoteAudioOffReason::RadioOffline;
        break;
    case ContextKind::EncoderUnavailable:
        context.offReason = RemoteAudioOffReason::EncoderUnavailable;
        break;
    default:
        break;
    }
    return context;
}

// The receiver faults that only restart playback, never persist.
const QList<Fault> kInterruptionFaults{Fault::NoPackets,
                                       Fault::DecodeFailed, Fault::ClockBuffer};

const QList<State> kAllStates{State::NotConnected, State::WaitingForAudio, State::MutedHere,
                              State::RadioOffline, State::CoreCouldNotStart, State::Starting,
                              State::Playing, State::Reconnecting, State::PlaybackProblem};

const QList<Fault> kAllFaults{Fault::SpeakerOpenFailed, Fault::SpeakerTimingUnavailable,
                              Fault::SpeakerCallbackTooLarge, Fault::SpeakerStalled,
                              Fault::SpeakerWriteFailed, Fault::DecoderUnavailable,
                              Fault::NoPackets,
                              Fault::DecodeFailed, Fault::ClockBuffer};

// A failure recorded in session epoch 4, connection A, context 10, while
// receiver generation 5 was playing.
RemoteAudioFailure recordedFailure()
{
    RemoteAudioFailure failure;
    failure.fault = Fault::SpeakerStalled;
    failure.epoch = 4;
    failure.connectionId = QStringLiteral("00000000-0000-4000-8000-00000000000a");
    failure.contextGeneration = 10;
    failure.receiverGeneration = 5;
    return failure;
}

// Everything matching recovery needs: a newer enabled context of the same
// session played by a newer running receiver whose speaker consumed audio.
struct Recovery {
    quint32 epoch = 4;
    QString connectionId = QStringLiteral("00000000-0000-4000-8000-00000000000a");
    std::optional<RemoteAudioContextMessage> context;
    RemoteAudioReceiverTelemetry playback;

    Recovery()
    {
        RemoteAudioContextMessage accepted;
        accepted.connectionId = connectionId;
        accepted.revision = 9;
        accepted.generation = 11;
        accepted.enabled = true;
        accepted.ssrc = 0x1234'5678;
        accepted.encoder = defaultProfile();
        context = accepted;
        playback.generation = 6;
        playback.running = true;
        playback.decodedPackets = 3;
        playback.deviceConsumedFrames = 480;
    }

    bool recovered(const RemoteAudioFailure& failure = recordedFailure()) const
    {
        return remoteAudioFailureRecovered(failure, epoch, connectionId, context, playback);
    }
};

} // namespace

class TstRemoteAudioStatus final : public QObject {
    Q_OBJECT

private slots:
    // A fallback must not describe requested Lossless as active L16, nor
    // claim 48 kbps when an older Core actually answers with 24.
    void diagnosticsSeparateRequestedQualityFromCurrentFormat()
    {
        RemoteAudioStatus status;
        status.state = RemoteAudioStatus::State::Playing;
        status.detailNegotiated = true;
        status.profileChoiceAvailable = true;
        status.chosenProfile = RemoteAudioProfile::Lossless;
        status.runningProfile = RemoteAudioProfile::Opus;
        status.encoder = defaultProfile();
        status.qualityReason = RemoteAudioQualityReason::NetworkTooSlow;
        const QString details = formatRemoteAudioDetails(status, {});
        QVERIFY(details.contains(QStringLiteral("Requested quality: Lossless")));
        QVERIFY(details.contains(QStringLiteral("Current receive format: Opus stereo, 24\u00A0kbit/s target")));
        QVERIFY(details.contains(QStringLiteral("Current microphone format:")));
        QVERIFY(details.contains(QStringLiteral("The network could not carry lossless audio")));
        QVERIFY(!details.contains(QStringLiteral("Current receive format: Lossless")));
        status.bitrateRefusal = QStringLiteral("The Core refused this requested audio bitrate.");
        status.headphonesFormat = QStringLiteral("Opus stereo, 48 kbps target");
        status.microphoneFormat = QStringLiteral("Opus mono, 24 kbps target (not sending)");
        const QString separate = formatRemoteAudioDetails(status, {});
        QVERIFY(separate.contains(status.bitrateRefusal));
        QVERIFY(separate.contains(QStringLiteral("Current headphones format: Opus stereo, 48 kbps target")));
        QVERIFY(separate.contains(QStringLiteral("Current microphone format: Opus mono, 24 kbps target (not sending)")));
        QVERIFY(separate.contains(QStringLiteral("Current receive format: Opus stereo, 24\u00A0kbit/s target")));
    }

    void derivationFollowsThePrecedenceTable_data()
    {
        QTest::addColumn<bool>("mediaSession");
        QTest::addColumn<bool>("muted");
        QTest::addColumn<bool>("radioConnected");
        QTest::addColumn<ContextKind>("context");
        QTest::addColumn<bool>("receiverRunning");
        QTest::addColumn<bool>("playing");
        QTest::addColumn<bool>("restarting");
        QTest::addColumn<bool>("problem");
        QTest::addColumn<State>("expected");

        // mediaSession, muted, radioConnected, context, running, playing,
        // restarting, problem -> state
        QTest::newRow("no media session, idle")
            << false << false << false << ContextKind::None
            << false << false << false << false << State::NotConnected;
        QTest::newRow("no media session outranks every other input")
            << false << true << false << ContextKind::Enabled
            << true << true << true << true << State::NotConnected;
        QTest::newRow("muted outranks a problem and the radio offline")
            << true << true << false << ContextKind::RadioOffline
            << false << false << true << true << State::MutedHere;
        QTest::newRow("muted while playing")
            << true << true << true << ContextKind::Enabled
            << true << true << false << false << State::MutedHere;
        QTest::newRow("problem outranks the radio offline")
            << true << false << false << ContextKind::RadioOffline
            << false << false << false << true << State::PlaybackProblem;
        QTest::newRow("problem outranks encoder unavailable and a restart")
            << true << false << true << ContextKind::EncoderUnavailable
            << false << false << true << true << State::PlaybackProblem;
        QTest::newRow("problem persists while a newer context plays")
            << true << false << true << ContextKind::Enabled
            << true << true << false << true << State::PlaybackProblem;
        QTest::newRow("radio not connected in this GUI")
            << true << false << false << ContextKind::None
            << false << false << false << false << State::RadioOffline;
        QTest::newRow("Core reports radio-offline before this GUI's mirror")
            << true << false << true << ContextKind::RadioOffline
            << false << false << false << false << State::RadioOffline;
        QTest::newRow("client-disabled reply once this GUI's mirror is offline")
            << true << false << false << ContextKind::ClientDisabled
            << false << false << false << false << State::RadioOffline;
        QTest::newRow("radio offline outranks encoder unavailable and a restart")
            << true << false << false << ContextKind::EncoderUnavailable
            << false << false << true << false << State::RadioOffline;
        QTest::newRow("encoder unavailable")
            << true << false << true << ContextKind::EncoderUnavailable
            << false << false << false << false << State::CoreCouldNotStart;
        QTest::newRow("encoder unavailable outranks a restart")
            << true << false << true << ContextKind::EncoderUnavailable
            << false << false << true << false << State::CoreCouldNotStart;
        QTest::newRow("restart scheduled")
            << true << false << true << ContextKind::Enabled
            << false << false << true << false << State::Reconnecting;
        QTest::newRow("restart outranks a disabled context")
            << true << false << true << ContextKind::ClientDisabled
            << false << false << true << false << State::Reconnecting;
        QTest::newRow("restart outranks no context")
            << true << false << true << ContextKind::None
            << false << false << true << false << State::Reconnecting;
        QTest::newRow("no context yet")
            << true << false << true << ContextKind::None
            << false << false << false << false << State::WaitingForAudio;
        QTest::newRow("client-disabled context")
            << true << false << true << ContextKind::ClientDisabled
            << false << false << false << false << State::WaitingForAudio;
        QTest::newRow("media-not-ready context")
            << true << false << true << ContextKind::MediaNotReady
            << false << false << false << false << State::WaitingForAudio;
        QTest::newRow("minor-7 disabled context without a reason")
            << true << false << true << ContextKind::DisabledLegacy
            << false << false << false << false << State::WaitingForAudio;
        QTest::newRow("enabled context, receiver not running")
            << true << false << true << ContextKind::Enabled
            << false << false << false << false << State::WaitingForAudio;
        QTest::newRow("enabled context, playing flag without a running receiver")
            << true << false << true << ContextKind::Enabled
            << false << true << false << false << State::WaitingForAudio;
        QTest::newRow("playing")
            << true << false << true << ContextKind::Enabled
            << true << true << false << false << State::Playing;
        QTest::newRow("minor-7 context playing")
            << true << false << true << ContextKind::EnabledLegacy
            << true << true << false << false << State::Playing;
        QTest::newRow("running, speaker progress not yet seen")
            << true << false << true << ContextKind::Enabled
            << true << false << false << false << State::Starting;
    }

    void derivationFollowsThePrecedenceTable()
    {
        QFETCH(bool, mediaSession);
        QFETCH(bool, muted);
        QFETCH(bool, radioConnected);
        QFETCH(ContextKind, context);
        QFETCH(bool, receiverRunning);
        QFETCH(bool, playing);
        QFETCH(bool, restarting);
        QFETCH(bool, problem);
        QFETCH(State, expected);

        RemoteAudioStatusInputs inputs;
        inputs.mediaSession = mediaSession;
        inputs.muted = muted;
        inputs.radioConnected = radioConnected;
        inputs.context = contextOf(context);
        inputs.receiverRunning = receiverRunning;
        inputs.playing = playing;
        inputs.restarting = restarting;
        if (problem) {
            inputs.problem = Fault::SpeakerStalled;
        }
        QCOMPARE(deriveRemoteAudioState(inputs), expected);
    }

    void headlinesAndBannerWordsAreExact()
    {
        const QList<std::tuple<State, QString, QString>> expected{
            {State::NotConnected, QStringLiteral("Not connected to a Core"),
             QStringLiteral("Audio stopped")},
            {State::WaitingForAudio, QStringLiteral("Waiting for audio from Core"),
             QStringLiteral("Audio waiting")},
            {State::MutedHere, QStringLiteral("Muted on this computer"),
             QStringLiteral("Audio muted")},
            {State::RadioOffline, QStringLiteral("Radio offline at the Core"),
             QStringLiteral("Radio offline")},
            {State::CoreCouldNotStart, QStringLiteral("Core could not start audio"),
             QStringLiteral("Audio unavailable")},
            {State::Starting, QStringLiteral("Starting audio"),
             QStringLiteral("Audio waiting")},
            {State::Playing, QStringLiteral("Playing"), QStringLiteral("Audio playing")},
            {State::Reconnecting, QStringLiteral("Audio interrupted, reconnecting"),
             QStringLiteral("Audio waiting")},
            {State::PlaybackProblem, QStringLiteral("Playback problem on this computer"),
             QStringLiteral("Audio unavailable")},
        };
        QCOMPARE(expected.size(), kAllStates.size());
        for (const auto& [state, headline, banner] : expected) {
            QCOMPARE(remoteAudioHeadline(state), headline);
            QCOMPARE(remoteAudioBannerWord(state), banner);
        }
    }

    void problemTextsArePlainEnglish()
    {
        QCOMPARE(remoteAudioProblemText(Fault::SpeakerOpenFailed),
                 QStringLiteral("The selected speaker device could not be opened."));
        QCOMPARE(remoteAudioProblemText(Fault::SpeakerTimingUnavailable),
                 QStringLiteral("The speaker device stopped reporting its timing."));
        QCOMPARE(remoteAudioProblemText(Fault::SpeakerCallbackTooLarge),
                 QStringLiteral("The speaker device buffer is larger than remote playback "
                                "supports. Choose a smaller buffer or another device."));
        QCOMPARE(remoteAudioProblemText(Fault::SpeakerStalled),
                 QStringLiteral("The speaker device stopped playing audio."));
        QCOMPARE(remoteAudioProblemText(Fault::SpeakerWriteFailed),
                 QStringLiteral("Audio could not be sent to the speaker device."));
        QCOMPARE(remoteAudioProblemText(Fault::DecoderUnavailable),
                 QStringLiteral("The audio decoder could not start on this computer."));
        // The receiver recovers from these by itself; they never persist.
        for (Fault fault : kInterruptionFaults) {
            QCOMPARE(remoteAudioProblemText(fault), QStringLiteral("Audio was interrupted."));
        }
    }

    void codecTextNamesWhatCoreReported()
    {
        RemoteAudioStatus status;
        QCOMPARE(remoteAudioCodecText(status), QStringLiteral("Not reported by this Core"));
        status.detailNegotiated = true;
        QCOMPARE(remoteAudioCodecText(status), QStringLiteral("Audio is off"));

        status.encoder = defaultProfile();
        QCOMPARE(remoteAudioCodecText(status),
                 QStringLiteral("Opus stereo, 24\u00A0kbit/s target, 40\u00A0ms packets, audio up to 8\u00A0kHz"));

        // Every number comes from the reported profile, never an assumed one.
        OpusEncoderProfile other = defaultProfile();
        other.targetBitrate = 48'000;
        other.audioBandwidthHz = 12'000;
        other.frameSamples = 960;
        status.encoder = other;
        QCOMPARE(remoteAudioCodecText(status),
                 QStringLiteral("Opus stereo, 48\u00A0kbit/s target, 20\u00A0ms packets, audio up to 12\u00A0kHz"));
        other = defaultProfile();
        other.channels = 1;
        other.audioBandwidthHz = 20'000;
        status.encoder = other;
        QCOMPARE(remoteAudioCodecText(status),
                 QStringLiteral("Opus mono, 24\u00A0kbit/s target, 40\u00A0ms packets, audio up to 20\u00A0kHz"));

        // A minor-7 Core cannot report one, whatever the value holds.
        status.detailNegotiated = false;
        QCOMPARE(remoteAudioCodecText(status), QStringLiteral("Not reported by this Core"));
    }

    // R-R3-23: a Core configured with audio_bitrate = 48000 encodes at that
    // target with fullband sound, reports both in the minor-8 audio context,
    // and the GUI names them from the report alone.
    void coreAt48000ReportsThatTargetInItsContext()
    {
        OpusAudioCodecConfig high;
        high.bitrate = 48'000;
        const OpusAudioEncoder encoder(high);
        if (!encoder.isReady()) {
            QSKIP("Opus encoder is unavailable in this build");
        }
        const std::optional<OpusEncoderProfile> profile = encoder.profile();
        QVERIFY(profile.has_value());
        QCOMPARE(profile->targetBitrate, 48'000);
        QCOMPARE(profile->audioBandwidthHz, 20'000);

        std::optional<RemoteAudioContextMessage> context = contextOf(ContextKind::Enabled);
        QVERIFY(context.has_value());
        context->encoder = profile;
        const QJsonObject wire = encodeRemoteAudioContext(*context, true);
        QCOMPARE(wire.value(QStringLiteral("encoder")).toObject()
                     .value(QStringLiteral("targetBitrate")).toInteger(),
                 qint64{48'000});
        QCOMPARE(wire.value(QStringLiteral("encoder")).toObject()
                     .value(QStringLiteral("audioBandwidthHz")).toInteger(),
                 qint64{20'000});
        const std::optional<RemoteAudioContextMessage> accepted =
            decodeRemoteAudioContext(wire, true);
        QVERIFY(accepted.has_value());
        QVERIFY(accepted->encoder.has_value());
        QCOMPARE(*accepted->encoder, *profile);

        RemoteAudioStatus status;
        status.detailNegotiated = true;
        status.encoder = accepted->encoder;
        QCOMPARE(remoteAudioCodecText(status),
                 QStringLiteral("Opus stereo, 48\u00A0kbit/s target, 40\u00A0ms packets, audio up to 20\u00A0kHz"));
    }

    void operatorWordingCarriesNoProtocolTerms()
    {
        static const QRegularExpression forbidden(
            QStringLiteral("\\[|\\]|\\bRTP\\b|SSRC|generation|epoch|revision|context|R-R3|"
                           "\\bphase\\b|\\bminor\\b"),
            QRegularExpression::CaseInsensitiveOption);
        QStringList words;
        for (State state : kAllStates) {
            words << remoteAudioHeadline(state) << remoteAudioBannerWord(state);
        }
        for (Fault fault : kAllFaults) {
            words << remoteAudioProblemText(fault);
        }
        RemoteAudioStatus status;
        words << remoteAudioCodecText(status);
        status.detailNegotiated = true;
        words << remoteAudioCodecText(status);
        status.encoder = defaultProfile();
        words << remoteAudioCodecText(status);
        for (const QString& text : words) {
            QVERIFY2(!text.isEmpty(), "every state and fault has operator wording");
            QVERIFY2(!forbidden.match(text).hasMatch(), qPrintable(text));
        }
    }

    void statusValueComparesEveryField()
    {
        const RemoteAudioStatus base;
        QVERIFY(base == RemoteAudioStatus{});
        RemoteAudioStatus changed = base;
        changed.state = State::Playing;
        QVERIFY(!(changed == base));
        changed = base;
        changed.detailNegotiated = true;
        QVERIFY(!(changed == base));
        changed = base;
        changed.encoder = defaultProfile();
        QVERIFY(!(changed == base));
        changed = base;
        changed.selectedOutput = QStringLiteral("System default");
        QVERIFY(!(changed == base));
        changed = base;
        changed.problem = Fault::SpeakerStalled;
        QVERIFY(!(changed == base));
        changed = base;
        changed.retryAvailable = true;
        QVERIFY(!(changed == base));
        changed = base;
        changed.chosenProfile = RemoteAudioProfile::Lossless;
        QVERIFY(!(changed == base));
        changed = base;
        changed.profileChoiceAvailable = true;
        QVERIFY(!(changed == base));
        changed = base;
        changed.runningProfile = RemoteAudioProfile::Opus;
        QVERIFY(!(changed == base));
        changed = base;
        changed.losslessEncoder = l16EncoderProfile();
        QVERIFY(!(changed == base));
        changed = base;
        changed.qualityReason = RemoteAudioQualityReason::NetworkTooSlow;
        QVERIFY(!(changed == base));
    }

    // R-R3-23: the section shows the audio quality the Core runs, or the
    // choice before it reports one, and says why in plain words when
    // Lossless was chosen and Opus runs. An older Core with Opus chosen
    // shows no quality line at all (the exact texts above).
    void qualityLinesShowWhatRunsAndWhy()
    {
        RemoteAudioStatus status;
        status.state = State::Playing;
        status.detailNegotiated = true;
        status.selectedOutput = QStringLiteral("System default");
        status.chosenProfile = RemoteAudioProfile::Lossless;
        status.profileChoiceAvailable = true;
        status.runningProfile = RemoteAudioProfile::Lossless;
        status.losslessEncoder = l16EncoderProfile();
        const RemoteAudioReceiverTelemetry playback;
        QCOMPARE(formatRemoteAudioDetails(status, playback), QStringLiteral(
            "Remote audio: Playing\n"
            "Requested quality: Lossless\n"
            "Current receive format: Lossless stereo, 16-bit, 1536\u00A0kbit/s, 4\u00A0ms packets\n"
            "Current microphone format: Not available on this connection\n"
            "Output: System default (selected)\n"
            "Arrival jitter: not measured\n"
            "Missing packets: none received\n"
            "Gaps filled: 0\n"
            "Speaker buffer: not measured"));

        // Refused by the Core's own setting: Opus runs, and it says why.
        status.runningProfile = RemoteAudioProfile::Opus;
        status.losslessEncoder.reset();
        status.encoder = defaultProfile();
        status.qualityReason = RemoteAudioQualityReason::CoreNotAllowed;
        QVERIFY(formatRemoteAudioDetails(status, playback).startsWith(QStringLiteral(
            "Remote audio: Playing\n"
            "Requested quality: Lossless\n"
            "This Core does not allow lossless audio.\n"
            "Current receive format: Opus stereo, 24\u00A0kbit/s target")));

        // The link trial failed.
        status.qualityReason = RemoteAudioQualityReason::NetworkTooSlow;
        QVERIFY(formatRemoteAudioDetails(status, playback).contains(QStringLiteral(
            "Requested quality: Lossless\n"
            "The network could not carry lossless audio; staying on Opus.\n")));

        // Before the Core reports what it runs: the choice.
        status.runningProfile.reset();
        status.qualityReason.reset();
        QVERIFY(formatRemoteAudioDetails(status, playback).contains(
            QStringLiteral("Requested quality: Lossless\n")));

        // An older Core with Lossless chosen: the choice shows, and why it
        // cannot be had.
        status.profileChoiceAvailable = false;
        status.runningProfile = RemoteAudioProfile::Opus;
        status.qualityReason = RemoteAudioQualityReason::CoreCannotSend;
        QVERIFY(formatRemoteAudioDetails(status, playback).contains(QStringLiteral(
            "Requested quality: Lossless\nThis Core cannot send lossless audio.\n")));

        QCOMPARE(remoteAudioProfileName(RemoteAudioProfile::Opus), QStringLiteral("Opus"));
        QCOMPARE(remoteAudioProfileName(RemoteAudioProfile::Lossless), QStringLiteral("Lossless"));
        QCOMPARE(remoteAudioQualityReasonText(RemoteAudioQualityReason::ConnectionUnavailable),
                 QStringLiteral("This connection could not set up lossless audio; staying on Opus."));
    }

    // R-R3-43 / R-R3-44: whether the receiver streams feeding apps are
    // Opus. They follow the one choice and its fallback: a running stream
    // says what it runs; before one runs, what the Core runs for the
    // speakers; before that, the choice. Lossless chosen but falling back
    // is compressed whatever ran last. No media session: not compressed.
    void receiverAudioCompressionFollowsChoiceAndFallback()
    {
        RemoteAudioStatus status;
        QVERIFY(!remoteReceiverAudioIsCompressed(status));  // NotConnected

        status.state = State::WaitingForAudio;
        status.chosenProfile = RemoteAudioProfile::Opus;
        QVERIFY(remoteReceiverAudioIsCompressed(status));
        status.chosenProfile = RemoteAudioProfile::Lossless;
        QVERIFY(!remoteReceiverAudioIsCompressed(status));

        status.state = State::Playing;
        status.runningProfile = RemoteAudioProfile::Lossless;
        QVERIFY(!remoteReceiverAudioIsCompressed(status));
        status.runningProfile = RemoteAudioProfile::Opus;
        QVERIFY(remoteReceiverAudioIsCompressed(status));

        // A running receiver stream wins over the speakers' profile.
        RemoteReceiverAudioStatus receiver;
        receiver.sliceId = 0;
        receiver.state = RemoteReceiverAudioStatus::State::Receiving;
        receiver.runningProfile = RemoteAudioProfile::Lossless;
        status.receivers = {receiver};
        QVERIFY(!remoteReceiverAudioIsCompressed(status));
        receiver.runningProfile = RemoteAudioProfile::Opus;
        status.receivers = {receiver};
        QVERIFY(remoteReceiverAudioIsCompressed(status));

        // A waiting or stopped stream says nothing; the speakers' does.
        receiver.state = RemoteReceiverAudioStatus::State::Waiting;
        receiver.runningProfile.reset();
        status.receivers = {receiver};
        status.runningProfile = RemoteAudioProfile::Lossless;
        QVERIFY(!remoteReceiverAudioIsCompressed(status));

        // Lossless chosen, the network could not carry it.
        receiver.state = RemoteReceiverAudioStatus::State::Receiving;
        receiver.runningProfile = RemoteAudioProfile::Lossless;
        status.receivers = {receiver};
        status.qualityReason = RemoteAudioQualityReason::NetworkTooSlow;
        QVERIFY(remoteReceiverAudioIsCompressed(status));

        status.state = State::NotConnected;
        QVERIFY(!remoteReceiverAudioIsCompressed(status));
    }

    // R-R3-43 / R-R3-23: a receiver stream's line names the rate the Core
    // reports for it (48 kbit/s from a current Core, 24 from an older one),
    // never one assumed here; lossless, or no report, names the profile.
    void receiverStateTextNamesTheReportedRate()
    {
        const QChar nbsp(0x00A0);
        RemoteReceiverAudioStatus receiver;
        receiver.sliceId = 1;
        QCOMPARE(remoteReceiverAudioStateText(receiver), QStringLiteral("Waiting for the Core"));
        receiver.state = RemoteReceiverAudioStatus::State::Receiving;
        QCOMPARE(remoteReceiverAudioStateText(receiver), QStringLiteral("Receiving"));
        receiver.runningProfile = RemoteAudioProfile::Opus;
        QCOMPARE(remoteReceiverAudioStateText(receiver), QStringLiteral("Receiving, Opus"));

        OpusAudioCodecConfig receiverConfig;
        receiverConfig.bitrate = 48'000;
        const OpusAudioEncoder encoder(receiverConfig);
        if (!encoder.isReady()) {
            QSKIP("Opus encoder is unavailable in this build");
        }
        receiver.encoder = encoder.profile();
        QVERIFY(receiver.encoder.has_value());
        const QString at48 = remoteReceiverAudioStateText(receiver);
        QCOMPARE(at48, QStringLiteral("Receiving, Opus 48") + nbsp + QStringLiteral("kbit/s"));
        receiver.encoder = defaultProfile();
        const QString at24 = remoteReceiverAudioStateText(receiver);
        QCOMPARE(at24, QStringLiteral("Receiving, Opus 24") + nbsp + QStringLiteral("kbit/s"));

        // Lossless shows no Opus rate, even with a stale one beside it.
        receiver.runningProfile = RemoteAudioProfile::Lossless;
        QCOMPARE(remoteReceiverAudioStateText(receiver), QStringLiteral("Receiving, Lossless"));

        // And the details line carries it.
        RemoteAudioStatus status;
        status.state = State::Playing;
        receiver.runningProfile = RemoteAudioProfile::Opus;
        receiver.encoder = encoder.profile();
        status.receivers = {receiver};
        const QString details = formatRemoteAudioDetails(status, {}, {}, {});
        QVERIFY2(details.contains(QStringLiteral("Receiver B for apps: ") + at48), qPrintable(details));
        for (const QString& text : {at48, at24}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
        RemoteReceiverAudioStatus other = receiver;
        other.encoder = defaultProfile();
        QVERIFY(!(other == receiver));
    }

    // R-R3-43 / R-R3-44 fix wave: which note the VAX page shows. None
    // without receiver streams from the Core or while they are lossless;
    // OpusChosen when Opus is the choice; LosslessUnavailable when Lossless
    // is the choice but Opus runs (a fallback or a refusal).
    void receiverAudioNoteSaysWhyItIsCompressed()
    {
        RemoteAudioStatus status;
        status.state = State::Playing;
        status.chosenProfile = RemoteAudioProfile::Opus;
        status.runningProfile = RemoteAudioProfile::Opus;
        QCOMPARE(remoteReceiverAudioNote(status, /*receiverAudioNegotiated=*/false),
                 RemoteReceiverAudioNote::None);
        QCOMPARE(remoteReceiverAudioNote(status, true), RemoteReceiverAudioNote::OpusChosen);

        status.chosenProfile = RemoteAudioProfile::Lossless;
        status.runningProfile = RemoteAudioProfile::Lossless;
        QCOMPARE(remoteReceiverAudioNote(status, true), RemoteReceiverAudioNote::None);

        status.runningProfile = RemoteAudioProfile::Opus;
        status.qualityReason = RemoteAudioQualityReason::NetworkTooSlow;
        QCOMPARE(remoteReceiverAudioNote(status, true),
                 RemoteReceiverAudioNote::LosslessUnavailable);
        QCOMPARE(remoteReceiverAudioNote(status, false), RemoteReceiverAudioNote::None);

        status.state = State::NotConnected;
        QCOMPARE(remoteReceiverAudioNote(status, true), RemoteReceiverAudioNote::None);
    }

    // Everything the quality choice puts in front of the operator stays in
    // user words: no wire or engineering terms.
    void qualityWordingCarriesNoInternalTerms()
    {
        static const QRegularExpression forbidden(
            QStringLiteral("\\bRTP\\b|\\bPCM\\b|\\bL16\\b|payload|codec|SSRC|\\bpeer\\b|"
                           "protocol|session|capabilit|revision|generation|minor|refus"),
            QRegularExpression::CaseInsensitiveOption);
        QStringList words;
        for (const RemoteAudioQualityReason reason :
             {RemoteAudioQualityReason::CoreCannotSend, RemoteAudioQualityReason::CoreNotAllowed,
              RemoteAudioQualityReason::ConnectionUnavailable,
              RemoteAudioQualityReason::NetworkTooSlow}) {
            words << remoteAudioQualityReasonText(reason);
        }
        RemoteAudioStatus status;
        status.detailNegotiated = true;
        status.losslessEncoder = l16EncoderProfile();
        words << remoteAudioCodecText(status) << remoteAudioQualityText(status);
        status.runningProfile = RemoteAudioProfile::Lossless;
        words << remoteAudioQualityText(status);
        for (const QString& text : words) {
            QVERIFY(!text.isEmpty());
            QVERIFY2(!forbidden.match(text).hasMatch(), qPrintable(text));
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
        }
    }

    // R-R3-21: the connection panel shows how long arriving audio is held
    // against late packets, and says so when late packets deepened it,
    // just before the delay it explains.
    void networkBufferLineShowsTheAdaptiveHold()
    {
        RemoteAudioStatus status;
        status.state = State::Playing;
        status.detailNegotiated = true;
        status.encoder = defaultProfile();
        status.selectedOutput = QStringLiteral("System default");
        RemoteAudioReceiverTelemetry playback;
        const QString without = formatRemoteAudioDetails(status, playback);
        QVERIFY(!without.contains(QStringLiteral("Network buffer")));
        playback.jitterHoldMs = 80.0;
        RemoteAudioDelayReport delay;
        delay.measurable = true;
        const QString steady = formatRemoteAudioDetails(status, playback, delay);
        QVERIFY(steady.contains(QStringLiteral(
            "Speaker buffer: not measured\nNetwork buffer: 80\u00A0ms on this computer\n"
            "Audio delay: not measured")));
        playback.jitterHoldMs = 372.4;
        const QString deepened = formatRemoteAudioDetails(status, playback);
        QVERIFY(deepened.contains(QStringLiteral(
            "Network buffer: 372\u00A0ms on this computer, deepened after late packets")));
        for (const QString& line : deepened.split(QLatin1Char('\n'))) {
            if (line.startsWith(QStringLiteral("Network buffer"))) {
                QVERIFY2(OperatorWording::isPlain(line), qPrintable(line));
            }
        }
    }

    // R-R3-35: the connection panel's delay line. Only a Core that answers
    // clock probes adds it; the figure carries its accuracy, says when the
    // speaker device is not counted, and uses no internal terms.
    void audioDelayLineOnlyWhenTheCoreCanMeasureIt()
    {
        RemoteAudioStatus status;
        status.state = State::Playing;
        status.detailNegotiated = true;
        status.encoder = defaultProfile();
        status.selectedOutput = QStringLiteral("System default");
        RemoteAudioReceiverTelemetry playback;
        const QString today = formatRemoteAudioDetails(status, playback);
        QCOMPARE(formatRemoteAudioDetails(status, playback, RemoteAudioDelayReport{}), today);
        QVERIFY(!today.contains(QStringLiteral("Audio delay")));

        RemoteAudioDelayReport delay;
        delay.measurable = true;
        QCOMPARE(formatRemoteAudioDetails(status, playback, delay),
                 today + QStringLiteral("\nAudio delay: not measured"));
        delay.estimate = AudioDelayEstimate{85.4, 0.4, false, 60.0, 0.4};
        QCOMPARE(formatRemoteAudioDetails(status, playback, delay),
                 today + QStringLiteral("\nAudio delay: 85\u00A0ms \u00B1 1\u00A0ms, "
                                        "not counting the speaker device"));
        delay.estimate->includesDevice = true;
        delay.estimate->delayMs = 102.6;
        delay.estimate->boundMs = 1.2;
        QCOMPARE(remoteAudioDelayText(*delay.estimate),
                 QStringLiteral("103\u00A0ms \u00B1 2\u00A0ms"));
        QCOMPARE(remoteAudioDeliveryText(*delay.estimate),
                 QStringLiteral("60\u00A0ms \u00B1 1\u00A0ms"));
        delay.estimate->deliveryMs.reset();
        QVERIFY(remoteAudioDeliveryText(*delay.estimate).isEmpty());
        // Muted here: no health lines, so no delay line either.
        status.state = State::MutedHere;
        QVERIFY(!formatRemoteAudioDetails(status, playback, delay).contains(
            QStringLiteral("Audio delay")));

        static const QRegularExpression forbidden(
            QStringLiteral("\\bRTP\\b|\\bPCM\\b|payload|codec|SSRC|\\bpeer\\b|"
                           "protocol|session|capabilit|revision|generation|minor|epoch|"
                           "telemetry|handshake|snapshot|endpoint|RTT"),
            QRegularExpression::CaseInsensitiveOption);
        AudioDelayEstimate estimate{85.4, 0.4, false, 60.0, 0.4};
        for (const QString& text : {remoteAudioDelayText(estimate),
                                    remoteAudioDeliveryText(estimate)}) {
            QVERIFY2(!forbidden.match(text).hasMatch(), qPrintable(text));
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
        }
    }

    // R-R3-23 link trial: the rule, sample by sample. Each window is
    // RemoteAudioLinkTrial::kWindowMs; 250 packets a second is lossless.
    void linkTrialConstantsAreTheDocumentedOnes()
    {
        QCOMPARE(RemoteAudioLinkTrial::kWindowMs, qint64(5'000));
        QCOMPARE(RemoteAudioLinkTrial::kTrialLossLimit, 0.02);
        QCOMPARE(RemoteAudioLinkTrial::kSustainedLossLimit, 0.01);
        QCOMPARE(RemoteAudioLinkTrial::kSustainedWindows, 3);
        QCOMPARE(RemoteAudioLinkTrial::kTrialRestartLimit, 1);
        QCOMPARE(RemoteAudioLinkTrial::kSustainedRestartLimit, 2);
        QCOMPARE(RemoteAudioLinkTrial::kRestartSpanMs, qint64(60'000));
    }
    void linkTrialFirstWindowFailsAboveTwoPercent_data()
    {
        QTest::addColumn<int>("lostPerMille");
        QTest::addColumn<bool>("fails");
        QTest::newRow("clean") << 0 << false;
        QTest::newRow("1.5% passes") << 15 << false;
        QTest::newRow("2.0% passes") << 20 << false;
        QTest::newRow("3.0% fails") << 30 << true;
    }
    void linkTrialFirstWindowFailsAboveTwoPercent()
    {
        QFETCH(int, lostPerMille);
        QFETCH(bool, fails);
        using Verdict = RemoteAudioLinkTrial::Verdict;
        RemoteAudioLinkTrial trial;
        QVERIFY(!trial.active());
        trial.begin(1'000);
        QVERIFY(trial.active());
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 1;
        // One sample a second, 250 packets each, a steady share missing and
        // filled in.
        Verdict verdict = Verdict::Continue;
        for (int second = 1; second <= 5; ++second) {
            playback.expectedPackets = quint64(250 * second);
            playback.missingPackets = quint64(250 * second * lostPerMille / 1000);
            playback.concealedPackets = playback.missingPackets;
            playback.decodedPackets = playback.expectedPackets - playback.missingPackets;
            verdict = trial.observe(1'000 + second * 1'000, playback);
            if (second < 5) { QCOMPARE(verdict, Verdict::Continue); }
        }
        QCOMPARE(verdict, fails ? Verdict::Failed : Verdict::Continue);
        QVERIFY(trial.lastWindowLoss().has_value());
        // Whole packets: 1.5% of 1250 is 18 lost, 1.44%.
        QVERIFY(std::abs(*trial.lastWindowLoss() * 1000 - lostPerMille) < 1.0);
    }
    void linkTrialLaterFailsOnlyOnSustainedLoss()
    {
        using Verdict = RemoteAudioLinkTrial::Verdict;
        RemoteAudioLinkTrial trial;
        trial.begin(0);
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 7;
        qint64 now = 0;
        // Each call closes one 5 s window with `lost` of 1250 packets lost.
        const auto window = [&](quint64 lost) {
            playback.expectedPackets += 1250;
            playback.missingPackets += lost;
            playback.concealedPackets += lost;
            playback.decodedPackets += 1250 - lost;
            now += RemoteAudioLinkTrial::kWindowMs;
            return trial.observe(now, playback);
        };
        QCOMPARE(window(0), Verdict::Continue);   // the trial window passes
        QCOMPARE(window(19), Verdict::Continue);  // 1.5%: one bad window
        QCOMPARE(window(19), Verdict::Continue);  // two
        QCOMPARE(window(5), Verdict::Continue);   // 0.4%: the run resets
        QCOMPARE(window(19), Verdict::Continue);
        QCOMPARE(window(19), Verdict::Continue);
        QCOMPARE(window(19), Verdict::Failed);    // three in a row

        // A burst above the trial limit in one later window is not enough.
        trial.begin(0);
        now = 0;
        playback = {};
        playback.running = true;
        playback.generation = 8;
        QCOMPARE(window(0), Verdict::Continue);
        QCOMPARE(window(100), Verdict::Continue); // 8% once
        QCOMPARE(window(0), Verdict::Continue);
    }
    // R-R3-43: one trial over every lossless stream. Two streams that each
    // lose 1.5% pass alone and together; a stream losing 4.8% fails the
    // window for all of them, even beside a clean one that dilutes it to
    // 2.4%. Each stream counts from its own generation, and a stream that
    // leaves and comes back counts from zero.
    void linkTrialCountsEveryLosslessStream()
    {
        using Verdict = RemoteAudioLinkTrial::Verdict;
        using Sample = RemoteAudioLinkTrial::StreamSample;
        const auto stream = [](int key, quint64 generation, quint64 expected, quint64 lost) {
            Sample sample;
            sample.stream = key;
            sample.playback.running = true;
            sample.playback.generation = generation;
            sample.playback.expectedPackets = expected;
            sample.playback.missingPackets = lost;
            sample.playback.concealedPackets = lost;
            sample.playback.decodedPackets = expected - lost;
            return sample;
        };
        RemoteAudioLinkTrial trial;
        trial.begin(0);
        QCOMPARE(trial.observe(1'000, {stream(-1, 1, 0, 0), stream(1, 3, 0, 0)}),
                 Verdict::Continue);
        QCOMPARE(trial.observe(5'000, {stream(-1, 1, 1250, 19), stream(1, 3, 1250, 19)}),
                 Verdict::Continue);
        QVERIFY(std::abs(*trial.lastWindowLoss() - 38.0 / 2500.0) < 1e-9);

        trial.begin(0);
        QCOMPARE(trial.observe(1'000, {stream(-1, 1, 0, 0), stream(2, 1, 0, 0)}),
                 Verdict::Continue);
        QCOMPARE(trial.observe(5'000, {stream(-1, 1, 1250, 0), stream(2, 1, 1250, 60)}),
                 Verdict::Failed);
        QVERIFY(std::abs(*trial.lastWindowLoss() - 60.0 / 2500.0) < 1e-9);

        // Stream 4 leaves; back on the same key with a counter below its old
        // base (a new receiver), it still counts everything it has played.
        trial.begin(0);
        QCOMPARE(trial.observe(1'000, {stream(4, 1, 0, 0)}), Verdict::Continue);
        QCOMPARE(trial.observe(2'000, {stream(4, 1, 100, 0)}), Verdict::Continue);
        QCOMPARE(trial.observe(3'000, std::vector<Sample>{}), Verdict::Continue);
        QCOMPARE(trial.observe(5'000, {stream(4, 1, 50, 5)}), Verdict::Failed);
        QVERIFY(std::abs(*trial.lastWindowLoss() - 5.0 / 150.0) < 1e-9);
    }
    void linkTrialCountsInterruptions()
    {
        using Verdict = RemoteAudioLinkTrial::Verdict;
        RemoteAudioLinkTrial idle;
        QCOMPARE(idle.noteInterruption(0), Verdict::Continue); // not running

        RemoteAudioLinkTrial first;
        first.begin(0);
        QCOMPARE(first.noteInterruption(2'000), Verdict::Failed); // in the trial window

        RemoteAudioLinkTrial later;
        later.begin(0);
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 1;
        playback.expectedPackets = 1250;
        playback.decodedPackets = 1250;
        QCOMPARE(later.observe(5'000, playback), Verdict::Continue);
        QCOMPARE(later.noteInterruption(10'000), Verdict::Continue);
        QCOMPARE(later.noteInterruption(71'000), Verdict::Continue); // 61 s apart
        QCOMPARE(later.noteInterruption(100'000), Verdict::Failed);  // second within 60 s
        later.end();
        QVERIFY(!later.active());
    }
    // R-R3-21 (review I3): an arrival burst, a stream gap or a stall no
    // longer restarts the receiver; it rides through and counts in
    // linkInterruptions. The trial still counts each as an interruption
    // (at most one a sample), so a stalling link falls back to Opus as the
    // trial's contract says: any one in the first window, then the second
    // within 60 s. A new receiver generation counts from zero.
    void linkTrialCountsInterruptionsTheReceiverRodeThrough()
    {
        using Verdict = RemoteAudioLinkTrial::Verdict;
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 1;
        playback.expectedPackets = 250;
        playback.decodedPackets = 250;

        RemoteAudioLinkTrial first;
        first.begin(0);
        QCOMPARE(first.observe(1'000, playback), Verdict::Continue);
        playback.linkInterruptions = 1;
        QCOMPARE(first.observe(2'000, playback), Verdict::Failed);
        QVERIFY(first.failedOnInterruptions());

        RemoteAudioLinkTrial later;
        later.begin(0);
        playback.linkInterruptions = 0;
        qint64 now = 0;
        for (int second = 1; second <= 5; ++second) {
            now = second * 1'000;
            playback.expectedPackets = playback.decodedPackets = quint64(250 * second);
            QCOMPARE(later.observe(now, playback), Verdict::Continue);
        }
        // One stall: a passing event. Several passes in one sample are one.
        playback.linkInterruptions = 3;
        QCOMPARE(later.observe(now += 1'000, playback), Verdict::Continue);
        QCOMPARE(later.observe(now += 1'000, playback), Verdict::Continue); // no growth
        // A new generation counts from zero: its lower counter is no
        // interruption (nor a negative one).
        playback.generation = 2;
        playback.linkInterruptions = 0;
        QCOMPARE(later.observe(now += 1'000, playback), Verdict::Continue);
        // A second stall within 60 s of the first fails the link.
        playback.linkInterruptions = 1;
        QCOMPARE(later.observe(now += 1'000, playback), Verdict::Failed);
        QVERIFY(later.failedOnInterruptions());
    }
    void linkTrialFollowsReceiverGenerations()
    {
        using Verdict = RemoteAudioLinkTrial::Verdict;
        RemoteAudioLinkTrial trial;
        trial.begin(0);
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 3;
        playback.expectedPackets = 600;
        playback.decodedPackets = 600;
        QCOMPARE(trial.observe(2'000, playback), Verdict::Continue);
        // A restart: the new receiver counts from zero. Its counters are
        // smaller than the old ones, which must not read as loss or wrap.
        RemoteAudioReceiverTelemetry stopped;
        QCOMPARE(trial.observe(2'500, stopped), Verdict::Continue); // ignored
        playback.generation = 4;
        playback.expectedPackets = 650;
        playback.decodedPackets = 650;
        QCOMPARE(trial.observe(5'000, playback), Verdict::Continue);
        QVERIFY(trial.lastWindowLoss().has_value());
        QCOMPARE(*trial.lastWindowLoss(), 0.0);

        // A window in which nothing played gives no verdict at all.
        RemoteAudioLinkTrial quiet;
        quiet.begin(0);
        QCOMPARE(quiet.observe(6'000, stopped), Verdict::Continue);
        QVERIFY(!quiet.lastWindowLoss().has_value());
    }

    void failureClearsOnlyOnMatchingRecovery()
    {
        // Every condition met: cleared.
        QVERIFY(Recovery().recovered());

        // Another session never clears it.
        Recovery otherEpoch;
        otherEpoch.epoch = 5;
        QVERIFY(!otherEpoch.recovered());
        Recovery otherConnection;
        otherConnection.connectionId = QStringLiteral("00000000-0000-4000-8000-00000000000b");
        QVERIFY(!otherConnection.recovered());

        // No accepted context, the failed context itself, or an older one.
        Recovery noContext;
        noContext.context.reset();
        QVERIFY(!noContext.recovered());
        Recovery sameContext;
        sameContext.context->generation = 10;
        QVERIFY(!sameContext.recovered());
        Recovery olderContext;
        olderContext.context->generation = 9;
        QVERIFY(!olderContext.recovered());

        // Time alone, or a receiver that is not running: not cleared.
        Recovery notRunning;
        notRunning.playback.running = false;
        QVERIFY(!notRunning.recovered());

        // The failed receiver generation, or an older one, cannot clear it.
        Recovery sameReceiver;
        sameReceiver.playback.generation = 5;
        QVERIFY(!sameReceiver.recovered());
        Recovery olderReceiver;
        olderReceiver.playback.generation = 4;
        QVERIFY(!olderReceiver.recovered());

        // A newer context and receiver whose speaker has consumed nothing.
        Recovery noProgress;
        noProgress.playback.deviceConsumedFrames = 0;
        QVERIFY(!noProgress.recovered());
    }

    // R-R3-23 Task 4: the Core connection panel's formatted section, one
    // scenario per acceptance row.
    void formatRemoteAudioDetailsCoversEveryRow()
    {
        // Playing, with a reported codec: every line present, health
        // measurements formatted and rounded.
        {
            RemoteAudioStatus status;
            status.state = State::Playing;
            status.detailNegotiated = true;
            status.encoder = defaultProfile();
            status.selectedOutput = QStringLiteral("System default");
            RemoteAudioReceiverTelemetry playback;
            playback.arrivalJitterMs = 3.2;
            playback.missingPackets = 2;
            playback.expectedPackets = 100;
            playback.concealedPackets = 5;
            playback.speakerQueuedMs = 118.7;
            // The reorder buffer is for the telemetry details, not this panel.
            playback.reorderQueuedMs = 80.0;
            QCOMPARE(formatRemoteAudioDetails(status, playback), QStringLiteral(
                "Remote audio: Playing\n"
                "Requested quality: High\n"
                "Current receive format: Opus stereo, 24\u00A0kbit/s target, 40\u00A0ms packets, audio up to 8\u00A0kHz\n"
                "Current microphone format: Not available on this connection\n"
                "Output: System default (selected)\n"
                "Arrival jitter: 3\u00A0ms\n"
                "Missing packets: 2 of 100\n"
                "Gaps filled: 5\n"
                "Speaker buffer: 119\u00A0ms on this computer"));
        }

        // A minor-7 Core: no codec detail, health still measured and shown.
        {
            RemoteAudioStatus status;
            status.state = State::Playing;
            status.detailNegotiated = false;
            status.selectedOutput = QStringLiteral("System default");
            RemoteAudioReceiverTelemetry playback;
            playback.arrivalJitterMs = 1.4;
            playback.missingPackets = 0;
            playback.expectedPackets = 50;
            playback.concealedPackets = 0;
            playback.speakerQueuedMs = 12.0;
            QCOMPARE(formatRemoteAudioDetails(status, playback), QStringLiteral(
                "Remote audio: Playing\n"
                "Requested quality: High\n"
                "Current receive format: Not reported by this Core\n"
                "Current microphone format: Not available on this connection\n"
                "Output: System default (selected)\n"
                "Arrival jitter: 1\u00A0ms\n"
                "Missing packets: 0 of 50\n"
                "Gaps filled: 0\n"
                "Speaker buffer: 12\u00A0ms on this computer"));
        }

        // PlaybackProblem: the Problem line appears, health stays visible.
        {
            RemoteAudioStatus status;
            status.state = State::PlaybackProblem;
            status.detailNegotiated = true;
            status.problem = Fault::SpeakerStalled;
            status.selectedOutput = QStringLiteral("USB DAC");
            RemoteAudioReceiverTelemetry playback;
            playback.arrivalJitterMs = 0.4;
            playback.missingPackets = 7;
            playback.expectedPackets = 200;
            playback.concealedPackets = 3;
            playback.speakerQueuedMs = 0.0;
            QCOMPARE(formatRemoteAudioDetails(status, playback), QStringLiteral(
                "Remote audio: Playback problem on this computer\n"
                "Problem: The speaker device stopped playing audio.\n"
                "Requested quality: High\n"
                "Current receive format: Audio is off\n"
                "Current microphone format: Not available on this connection\n"
                "Output: USB DAC (selected)\n"
                "Arrival jitter: 0\u00A0ms\n"
                "Missing packets: 7 of 200\n"
                "Gaps filled: 3\n"
                "Speaker buffer: 0\u00A0ms on this computer"));
        }

        // MutedHere: health is omitted, but a problem behind the mute still
        // shows (matches the real session: mute is what shows, the problem
        // stays behind it).
        {
            RemoteAudioStatus status;
            status.state = State::MutedHere;
            status.detailNegotiated = true;
            status.encoder = defaultProfile();
            status.selectedOutput = QStringLiteral("System default");
            status.problem = Fault::SpeakerStalled;
            RemoteAudioReceiverTelemetry playback;
            playback.arrivalJitterMs = 5.0;
            playback.missingPackets = 1;
            playback.expectedPackets = 10;
            playback.concealedPackets = 2;
            playback.speakerQueuedMs = 15.0;
            QCOMPARE(formatRemoteAudioDetails(status, playback), QStringLiteral(
                "Remote audio: Muted on this computer\n"
                "Problem: The speaker device stopped playing audio.\n"
                "Requested quality: High\n"
                "Current receive format: Opus stereo, 24\u00A0kbit/s target, 40\u00A0ms packets, audio up to 8\u00A0kHz\n"
                "Current microphone format: Not available on this connection\n"
                "Output: System default (selected)"));
        }

        // Unmeasured values: "not measured" / "none received", and
        // a Core that negotiated detail but has no encoder in the current
        // (disabled) context reads "Audio is off".
        {
            RemoteAudioStatus status;
            status.state = State::WaitingForAudio;
            status.detailNegotiated = true;
            status.selectedOutput = QStringLiteral("System default");
            RemoteAudioReceiverTelemetry playback;
            QCOMPARE(formatRemoteAudioDetails(status, playback), QStringLiteral(
                "Remote audio: Waiting for audio from Core\n"
                "Requested quality: High\n"
                "Current receive format: Audio is off\n"
                "Current microphone format: Not available on this connection\n"
                "Output: System default (selected)\n"
                "Arrival jitter: not measured\n"
                "Missing packets: none received\n"
                "Gaps filled: 0\n"
                "Speaker buffer: not measured"));
        }

        // NotConnected and RadioOffline also omit health, like MutedHere.
        {
            RemoteAudioReceiverTelemetry playback;
            playback.arrivalJitterMs = 9.0;
            RemoteAudioStatus notConnected;
            notConnected.state = State::NotConnected;
            QVERIFY(!formatRemoteAudioDetails(notConnected, playback).contains(QStringLiteral("Arrival jitter")));
            RemoteAudioStatus offline;
            offline.state = State::RadioOffline;
            QVERIFY(!formatRemoteAudioDetails(offline, playback).contains(QStringLiteral("Arrival jitter")));
        }
    }

    void failureClearingIsWrapAware()
    {
        RemoteAudioFailure nearWrap = recordedFailure();
        nearWrap.contextGeneration = std::numeric_limits<quint32>::max();
        Recovery wrapped;
        wrapped.context->generation = 1; // two contexts later, past the wrap
        QVERIFY(wrapped.recovered(nearWrap));

        // Half the ring back is older, not newer, however large it looks.
        RemoteAudioFailure early = recordedFailure();
        early.contextGeneration = 10;
        Recovery farAhead;
        farAhead.context->generation = 10u + 0x8000'0000u;
        QVERIFY(!farAhead.recovered(early));
        farAhead.context->generation = 10u + 0x7FFF'FFFFu;
        QVERIFY(farAhead.recovered(early));
    }

    // The one generation test RemoteMediaController and RemoteAudioStatus
    // share.
    void sharedGenerationTestIsWrapAware()
    {
        QVERIFY(!isNewerGeneration(5, 5));
        QVERIFY(isNewerGeneration(6, 5));
        QVERIFY(!isNewerGeneration(5, 6));
        QVERIFY(isNewerGeneration(0, std::numeric_limits<quint32>::max()));
        QVERIFY(isNewerGeneration(0x7FFF'FFFFu, 0));
        QVERIFY(!isNewerGeneration(0x8000'0000u, 0));
    }
};

QTEST_GUILESS_MAIN(TstRemoteAudioStatus)
#include "tst_remote_audio_status.moc"
