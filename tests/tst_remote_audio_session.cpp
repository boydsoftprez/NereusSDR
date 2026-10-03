// =================================================================
// tests/tst_remote_audio_session.cpp  (NereusSDR)
// =================================================================
// Real DTLS/SRTP daemon-to-remote audio exercise.  LoopbackTransport is used
// only for the authenticated control plane; both media peers use the default
// LibDataChannel transport and carry actual RTP/Opus over their local link.
// =================================================================

#include <QtTest>
#include "RealtimeTestLoad.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/HpsdrModel.h"
#include "core/safety/TransmitHolder.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/PacedAudioBus.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteMediaController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QScopeGuard>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTimer>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

using namespace NereusSDR;

namespace {

constexpr int kFrames = 480;
constexpr double kPi = 3.14159265358979323846;

// The encoder object a default Core announces (R-R3-21: 48 kbit/s fullband).
QJsonObject defaultEncoderJson()
{
    return {{QStringLiteral("codec"), QStringLiteral("opus")},
            {QStringLiteral("sampleRate"), 48000},
            {QStringLiteral("channels"), 2},
            {QStringLiteral("frameSamples"), 1920},
            {QStringLiteral("targetBitrate"), 48000},
            {QStringLiteral("audioBandwidthHz"), 20000}};
}

// Every context the GUI accepted, captured when it said so.
struct AcceptedContexts {
    QList<RemoteAudioContextMessage> contexts;
    int signalsWithoutContext = 0;
    QMetaObject::Connection connection;

    explicit AcceptedContexts(RemoteMediaController& media)
    {
        connection = QObject::connect(
            &media, &RemoteMediaController::audioContextAccepted, &media, [this, &media] {
                if (const std::optional<RemoteAudioContextMessage> context =
                        media.acceptedAudioContext()) {
                    contexts.append(*context);
                } else {
                    ++signalsWithoutContext;
                }
            });
    }
    ~AcceptedContexts() { QObject::disconnect(connection); }
    AcceptedContexts(const AcceptedContexts&) = delete;
    AcceptedContexts& operator=(const AcceptedContexts&) = delete;

    bool any(const std::function<bool(const RemoteAudioContextMessage&)>& match) const
    {
        return std::any_of(contexts.cbegin(), contexts.cend(), match);
    }

    // One signal per accepted context: no context is reported twice.
    bool eachReportedOnce() const
    {
        for (qsizetype index = 1; index < contexts.size(); ++index) {
            if (contexts.at(index).connectionId == contexts.at(index - 1).connectionId
                && contexts.at(index).generation == contexts.at(index - 1).generation) {
                return false;
            }
        }
        return true;
    }
};

QList<QJsonObject> audioContexts(const QSignalSpy& controls)
{
    QList<QJsonObject> contexts;
    for (const auto& call : controls) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("audio-context")) {
            contexts.append(message);
        }
    }
    return contexts;
}

QJsonObject latestAudioContext(const QSignalSpy& controls)
{
    for (auto it = controls.crbegin(); it != controls.crend(); ++it) {
        const QJsonObject message = it->at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("audio-context")) {
            return message;
        }
    }
    return {};
}

double channelEnergy(const QVector<float>& samples, int channel, int firstFrame = 0)
{
    double total = 0.0;
    for (int frame = firstFrame; frame * 2 + channel < samples.size(); ++frame) {
        const double sample = samples.at(frame * 2 + channel);
        total += sample * sample;
    }
    return total;
}

double correlation(const QVector<float>& samples, int firstFrame = 0)
{
    double left = 0.0;
    double right = 0.0;
    double cross = 0.0;
    for (int frame = firstFrame; frame * 2 + 1 < samples.size(); ++frame) {
        const double l = samples.at(frame * 2);
        const double r = samples.at(frame * 2 + 1);
        left += l * l;
        right += r * r;
        cross += l * r;
    }
    return left > 0.0 && right > 0.0 ? cross / std::sqrt(left * right) : 1.0;
}

double toneAmplitude(const QVector<float>& samples, int channel, double hz,
                     int firstFrame = 0)
{
    double cosine = 0.0;
    double sine = 0.0;
    int frames = 0;
    for (int frame = firstFrame; frame * 2 + channel < samples.size(); ++frame) {
        const double phase = 2.0 * kPi * hz * static_cast<double>(frame) / 48000.0;
        const double sample = samples.at(frame * 2 + channel);
        cosine += sample * std::cos(phase);
        sine += sample * std::sin(phase);
        ++frames;
    }
    return frames > 0 ? 2.0 * std::hypot(cosine, sine) / frames : 0.0;
}

// R-R3-23: audio detail can be absent while newer, independent media
// capabilities are present. Check the entire negotiated start shape after
// removing only its fresh connection ID, and require the legacy audio
// control shape separately.
QJsonObject modernStartWithoutConnection(bool audioProfile)
{
    QJsonObject start{{QStringLiteral("op"), QStringLiteral("start")},
                      {QStringLiteral("mediaRelayRoutingVersion"), 1},
                      {QStringLiteral("mediaTunnelVersion"), 1},
                      {QStringLiteral("miniDisplayVersion"), 1},
                      {QStringLiteral("remoteIqVersion"), 1}};
    if (audioProfile) { start.insert(QStringLiteral("audioProfileVersion"), 1); }
    return start;
}

bool startMatches(const QJsonObject& control, const QJsonObject& expectedWithoutConnection)
{
    if (control.value(QStringLiteral("connectionId")).toString().isEmpty()) { return false; }
    QJsonObject shape = control;
    shape.remove(QStringLiteral("connectionId"));
    if (shape != expectedWithoutConnection) {
        qWarning() << "start shape" << shape << "expected" << expectedWithoutConnection;
        return false;
    }
    return true;
}

bool onlyExpectedAudioControls(const QSignalSpy& coreControls,
                               const QJsonObject& expectedStartWithoutConnection)
{
    const QStringList audioKeys{QStringLiteral("connectionId"), QStringLiteral("enabled"),
                                QStringLiteral("op"), QStringLiteral("revision")};
    int starts = 0;
    int audio = 0;
    for (const auto& call : coreControls) {
        const QJsonObject control = call.at(0).toJsonObject();
        const QString op = control.value(QStringLiteral("op")).toString();
        QStringList keys = control.keys();
        keys.sort();
        if (op == QLatin1String("start")) {
            ++starts;
            if (!startMatches(control, expectedStartWithoutConnection)) { return false; }
        } else if (op == QLatin1String("audio")) {
            ++audio;
            if (keys != audioKeys) { return false; }
        }
    }
    return starts > 0 && audio > 0;
}

// R-R3-35: the clock probes this GUI sent the Core.
int clockProbesIn(const QSignalSpy& coreControls)
{
    int probes = 0;
    for (const auto& call : coreControls) {
        if (call.at(0).toJsonObject().value(QStringLiteral("op"))
            == QLatin1String("clock-probe")) {
            ++probes;
        }
    }
    return probes;
}

// The shared real session: Core and GUI over DTLS/SRTP, paced GUI speaker.
using Harness = Test::RemoteAudioSessionHarness;

} // namespace

class TstRemoteAudioSession final : public QObject {
    Q_OBJECT

private slots:
    // The load when a real-time case failed (R-R3-21, R-R3-40).
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }

    // R-R3-23: the remote audio choice is stored in this computer's
    // settings; keep this test's writes out of the operator's own file.
    void initTestCase()
    {
        const QString profile = QStringLiteral("remote-audio-session-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void encryptedAudioSurvivesMuteResumeAndSessionReconnect()
    {
        Harness h;
        // Keep controller destruction ahead of both AudioEngines: its remote
        // receiver owns worker callbacks and its peer owns libdatachannel.
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        AcceptedContexts accepted(remoteMedia);
        const std::optional<OpusEncoderProfile> coreProfile = OpusAudioEncoder().profile();
        QVERIFY(coreProfile.has_value());
        // The GUI logs the profile Core reported, not one it assumes.
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "^Remote audio receiving: Opus 48000 Hz, 2 channels, 1920-sample frames, "
            "target 48000 bit/s, audio bandwidth 20000 Hz, context \\d+$")));

        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(kFrames); });
        source.start();
        speaker.start();

        // R-R3-23: this Core lacks the lossless choice but still advertises
        // independent media features. Its start names those negotiated
        // features, without audioProfileVersion. R-R3-35: it also predates
        // measured delay, so it is sent no clock probe.
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        h.hideAudioProfile = true;
        h.hideAudioClock = true;
        // Desktop remote transmit: such a Core predates the microphone line
        // too (the window is sent no remoteTxVersion).
        h.declareRemoteTx = false;
        // Parity Task 32: and the transmit monitor.
        h.hideTxMonitorAudio = true;
        h.connectSession();
        QVERIFY(remoteMedia.audioDetailNegotiated());
        QVERIFY(!remoteMedia.audioProfileNegotiated());
        QVERIFY(!remoteMedia.audioClockNegotiated());
        QCOMPARE(h.client.capabilities().mediaRelayRoutingVersion, 1);
        QCOMPARE(h.client.capabilities().mediaTunnelVersion, 1);
        QCOMPARE(h.client.capabilities().miniDisplayVersion, 1);
        QCOMPARE(h.client.capabilities().remoteIqVersion, 1);
        QTRY_VERIFY2_WITH_TIMEOUT(!latestAudioContext(controls).isEmpty(),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QVERIFY(onlyExpectedAudioControls(coreControls, modernStartWithoutConnection(false)));
        const QJsonObject initial = latestAudioContext(controls);
        // Minor 8: the eight keys plus the profile Core actually encodes with.
        QCOMPARE(initial.size(), 9);
        QCOMPARE(initial.value(QStringLiteral("encoder")).toObject(), defaultEncoderJson());
        QVERIFY(!initial.contains(QStringLiteral("reason")));
        QVERIFY(initial.value(QStringLiteral("enabled")).toBool());
        QVERIFY(initial.value(QStringLiteral("generation")).toInteger() > 0);
        const QString initialConnection = initial.value(QStringLiteral("connectionId")).toString();
        const quint32 initialGeneration = static_cast<quint32>(
            initial.value(QStringLiteral("generation")).toInteger());
        QTRY_VERIFY(remoteMedia.acceptedAudioContext().has_value());
        {
            const RemoteAudioContextMessage context = *remoteMedia.acceptedAudioContext();
            QCOMPARE(context.generation, initialGeneration);
            QVERIFY(context.enabled);
            QVERIFY(context.encoder.has_value());
            QCOMPARE(*context.encoder, *coreProfile);
            QVERIFY(!context.offReason.has_value());
        }

        const int initialHeardFrame = h.remoteBus->heard.size() / 2;
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size()
                                     >= (initialHeardFrame + 48000) * 2
                                 && channelEnergy(h.remoteBus->heard, 0, initialHeardFrame) > 2.0
                                 && channelEnergy(h.remoteBus->heard, 1, initialHeardFrame) > 2.0,
                                 15000);
        const int audibleStartFrame = initialHeardFrame;
        // The approved 24 kb/s stereo profile retains each panned program
        // with measured ~23 dB separation, not near-zero correlation. Check
        // the two known tones directly: each intended channel must retain at
        // least 18 dB (8x amplitude) over the same tone in the other channel.
        const double left617 = toneAmplitude(h.remoteBus->heard, 0, 617.0, audibleStartFrame);
        const double left1579 = toneAmplitude(h.remoteBus->heard, 0, 1579.0, audibleStartFrame);
        const double right617 = toneAmplitude(h.remoteBus->heard, 1, 617.0, audibleStartFrame);
        const double right1579 = toneAmplitude(h.remoteBus->heard, 1, 1579.0, audibleStartFrame);
        qInfo() << "Encrypted audio spectral amplitudes L617/L1579/R617/R1579"
                << left617 << left1579 << right617 << right1579;
        QVERIFY(left617 > right617 * 8.0);
        QVERIFY(right1579 > left1579 * 8.0);
        QVERIFY(std::abs(correlation(h.remoteBus->heard, audibleStartFrame)) < 0.95);
        QVERIFY(h.remoteBus->peakQueued > 0);
        QVERIFY(h.remoteBus->peakQueued <= 1440);
        QCOMPARE(remoteErrors.count(), 0);
        // This computer says it is playing, and names the profile Core reported.
        QTRY_COMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::Playing);
        QCOMPARE(remoteAudioCodecText(remoteMedia.audioStatus()),
                 QStringLiteral("Opus stereo, 48\u00A0kbit/s target, 40\u00A0ms packets, audio up to 20\u00A0kHz"));

        const double stationPanA = h.station.sliceById(h.sliceA)->audioPan();
        const double stationPanB = h.station.sliceById(h.sliceB)->audioPan();
        const int stationAfGainA = h.station.sliceById(h.sliceA)->afGain();
        const int stationAfGainB = h.station.sliceById(h.sliceB)->afGain();
        const bool stationMutedA = h.station.sliceById(h.sliceA)->muted();
        const bool stationMutedB = h.station.sliceById(h.sliceB)->muted();
        const int flushesBeforeMute = h.remoteBus->flushes;
        h.remote.audioEngine()->setMasterMuted(true);
        // Muting is this computer's own choice, and it says so at once.
        QCOMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::MutedHere);
        QVERIFY(!remoteMedia.audioStatus().retryAvailable);
        QTRY_VERIFY_WITH_TIMEOUT(!latestAudioContext(controls)
                                     .value(QStringLiteral("enabled")).toBool(), 5000);
        QCOMPARE(latestAudioContext(controls).size(), 9);
        QCOMPARE(latestAudioContext(controls).value(QStringLiteral("reason")).toString(),
                 QStringLiteral("client-disabled"));
        QVERIFY(!latestAudioContext(controls).contains(QStringLiteral("encoder")));
        QTRY_VERIFY(remoteMedia.acceptedAudioContext().has_value()
                    && !remoteMedia.acceptedAudioContext()->enabled);
        QVERIFY(remoteMedia.acceptedAudioContext()->offReason.has_value());
        QCOMPARE(*remoteMedia.acceptedAudioContext()->offReason,
                 RemoteAudioOffReason::ClientDisabled);
        QVERIFY(!remoteMedia.acceptedAudioContext()->encoder.has_value());
        QVERIFY(h.remoteBus->flushes > flushesBeforeMute);
        QVERIFY(h.remoteBus->outputPacing().has_value());
        QCOMPARE(h.remoteBus->outputPacing()->queuedFrames, 0);
        // Still muted here once Core has stopped, with no codec in use; the
        // station's own slice gain, pan and mute are untouched.
        QCOMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::MutedHere);
        QCOMPARE(remoteAudioCodecText(remoteMedia.audioStatus()), QStringLiteral("Audio is off"));
        QCOMPARE(h.station.sliceById(h.sliceA)->audioPan(), stationPanA);
        QCOMPARE(h.station.sliceById(h.sliceB)->audioPan(), stationPanB);
        QCOMPARE(h.station.sliceById(h.sliceA)->afGain(), stationAfGainA);
        QCOMPARE(h.station.sliceById(h.sliceB)->afGain(), stationAfGainB);
        QCOMPARE(h.station.sliceById(h.sliceA)->muted(), stationMutedA);
        QCOMPARE(h.station.sliceById(h.sliceB)->muted(), stationMutedB);

        const int heardBeforeResume = h.remoteBus->heard.size() / 2;
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_VERIFY_WITH_TIMEOUT(latestAudioContext(controls)
                                     .value(QStringLiteral("enabled")).toBool(), 5000);
        const QJsonObject resumed = latestAudioContext(controls);
        QVERIFY(static_cast<quint32>(resumed.value(QStringLiteral("generation")).toInteger())
                > initialGeneration);
        QVERIFY(resumed.value(QStringLiteral("firstSequence")).isDouble());
        QVERIFY(resumed.value(QStringLiteral("firstTimestamp")).isDouble());
        QCOMPARE(resumed.value(QStringLiteral("encoder")).toObject(), defaultEncoderJson());
        QTRY_VERIFY_WITH_TIMEOUT(channelEnergy(h.remoteBus->heard, 0, heardBeforeResume) > 0.5
                                 && channelEnergy(h.remoteBus->heard, 1, heardBeforeResume) > 0.5,
                                 10000);
        QTRY_COMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::Playing);

        h.client.disconnectFromStation(QStringLiteral("test reconnect"));
        QTRY_VERIFY_WITH_TIMEOUT(!h.client.mediaAvailable(), 5000);
        // A retired media session forgets what it accepted.
        QTRY_VERIFY(!remoteMedia.acceptedAudioContext().has_value());
        const int heardBeforeReconnect = h.remoteBus->heard.size() / 2;
        h.connectSession();
        // The new session's first context can be off (media-not-ready) until
        // its media peer is ready; Core then sends the enabled one
        // (DaemonMediaController's MediaPeer::ready handler reconciles audio).
        // Wait for that enabled context, not just the new connection.
        QTRY_VERIFY2_WITH_TIMEOUT(!latestAudioContext(controls).isEmpty()
                                      && latestAudioContext(controls)
                                                 .value(QStringLiteral("connectionId")).toString()
                                             != initialConnection
                                      && latestAudioContext(controls)
                                             .value(QStringLiteral("enabled")).toBool(),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const QJsonObject reconnected = latestAudioContext(controls);
        QVERIFY(reconnected.value(QStringLiteral("enabled")).toBool());
        QVERIFY(reconnected.value(QStringLiteral("ssrc")) != initial.value(QStringLiteral("ssrc")));
        QCOMPARE(reconnected.value(QStringLiteral("encoder")).toObject(), defaultEncoderJson());
        QTRY_VERIFY(remoteMedia.acceptedAudioContext().has_value()
                    && remoteMedia.acceptedAudioContext()->connectionId
                        == reconnected.value(QStringLiteral("connectionId")).toString());
        QTRY_VERIFY_WITH_TIMEOUT(channelEnergy(h.remoteBus->heard, 0, heardBeforeReconnect) > 0.5
                                 && channelEnergy(h.remoteBus->heard, 1, heardBeforeReconnect) > 0.5,
                                 15000);
        QCOMPARE(remoteErrors.count(), 0);
        // Mute, resume and reconnect kept the negotiated start and legacy
        // audio-control shapes.
        QVERIFY(onlyExpectedAudioControls(coreControls, modernStartWithoutConnection(false)));
        QCOMPARE(clockProbesIn(coreControls), 0);
        QVERIFY(!remoteMedia.audioDelay().measurable);
        QVERIFY(!remoteMedia.audioDelay().estimate);
        QVERIFY(!formatRemoteAudioDetails(remoteMedia.audioStatus(), remoteMedia.audioTelemetry(),
                                          remoteMedia.audioDelay())
                     .contains(QStringLiteral("Audio delay")));
        // Every accepted context carried exactly the detail its state calls for.
        QCOMPARE(accepted.signalsWithoutContext, 0);
        QVERIFY(accepted.eachReportedOnce());
        QVERIFY(!accepted.contexts.isEmpty());
        for (const RemoteAudioContextMessage& context : accepted.contexts) {
            QCOMPARE(context.encoder.has_value(), context.enabled);
            QCOMPARE(context.offReason.has_value(), !context.enabled);
        }

        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void minorSevenPeersKeepLegacyAudioContext()
    {
        Harness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        AcceptedContexts accepted(remoteMedia);
        // A minor-7 Core reports no profile; the GUI says so rather than
        // naming one it assumes.
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "^Remote audio receiving: codec profile not reported by Core, context \\d+$")));

        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(kFrames); });
        source.start();
        speaker.start();

        // Both ends agree minor 7: a Core and a GUI from before the audio
        // status detail. Audio must still start, on the eight-key context.
        // Core's first context is preceded by a forged copy in the minor-8
        // shape with a different first sequence; a minor-7 GUI must refuse
        // it without advancing its generation.
        h.connectSession(quint16{7}, [](const QJsonObject& real) {
            QJsonObject forged = real;
            forged.insert(QStringLiteral("encoder"), defaultEncoderJson());
            forged.insert(QStringLiteral("firstSequence"),
                          (real.value(QStringLiteral("firstSequence")).toInteger() + 1000)
                              % 65536);
            return forged;
        });
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(!h.server.remoteAudioStatusAvailable());
        QVERIFY(!h.client.remoteAudioStatusAvailable());
        QVERIFY(!remoteMedia.audioDetailNegotiated());
        QTRY_VERIFY2_WITH_TIMEOUT(latestAudioContext(controls)
                                      .value(QStringLiteral("enabled")).toBool(),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QCOMPARE(latestAudioContext(controls).size(), 8);
        // Minor 7 negotiated no optional start fields: exactly op and the
        // fresh connectionId reach Core, with the legacy audio controls.
        QVERIFY(onlyExpectedAudioControls(coreControls,
                                          {{QStringLiteral("op"), QStringLiteral("start")}}));

        const int heardBefore = h.remoteBus->heard.size() / 2;
        QTRY_VERIFY_WITH_TIMEOUT(channelEnergy(h.remoteBus->heard, 0, heardBefore) > 0.5
                                 && channelEnergy(h.remoteBus->heard, 1, heardBefore) > 0.5,
                                 15000);

        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(!latestAudioContext(controls)
                                     .value(QStringLiteral("enabled")).toBool(), 5000);
        QTRY_VERIFY(remoteMedia.acceptedAudioContext().has_value()
                    && !remoteMedia.acceptedAudioContext()->enabled);

        const QList<QJsonObject> received = audioContexts(controls);
        QCOMPARE(h.stationLink->forgedContexts, 1);
        QVERIFY(received.size() >= 3);
        // The forged copy reached the GUI, then the real one, same generation.
        QCOMPARE(received.at(0).size(), 9);
        QCOMPARE(received.at(0).value(QStringLiteral("generation")).toInteger(),
                 received.at(1).value(QStringLiteral("generation")).toInteger());
        for (qsizetype index = 1; index < received.size(); ++index) {
            QCOMPARE(received.at(index).size(), 8);
            QVERIFY(!received.at(index).contains(QStringLiteral("encoder")));
            QVERIFY(!received.at(index).contains(QStringLiteral("reason")));
        }
        QVERIFY(!accepted.contexts.isEmpty());
        QCOMPARE(qint64{accepted.contexts.constFirst().generation},
                 received.at(1).value(QStringLiteral("generation")).toInteger());
        QCOMPARE(qint64{accepted.contexts.constFirst().firstSequence},
                 received.at(1).value(QStringLiteral("firstSequence")).toInteger());
        QCOMPARE(accepted.signalsWithoutContext, 0);
        QVERIFY(accepted.eachReportedOnce());
        for (const RemoteAudioContextMessage& context : accepted.contexts) {
            QVERIFY(!context.encoder.has_value());
            QVERIFY(!context.offReason.has_value());
        }
        QCOMPARE(remoteErrors.count(), 0);

        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void minorEightReportsWhyAudioIsOffAndRefusesForgedContexts_data()
    {
        QTest::addColumn<bool>("mirrorFirst");
        QTest::newRow("ordinary-delivery") << false;
        QTest::newRow("mirrored-offline-before-context") << true;
    }

    void minorEightReportsWhyAudioIsOffAndRefusesForgedContexts()
    {
        QFETCH(bool, mirrorFirst);
        Harness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        AcceptedContexts accepted(remoteMedia);

        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(kFrames); });
        source.start();
        speaker.start();

        // Core's first context is preceded by a forged copy in the minor-7
        // shape with a different first sequence; a minor-8 GUI must refuse
        // it without advancing its generation. The Core predates the
        // lossless choice (R-R3-23), so its contexts are the minor-8 shape.
        h.hideAudioProfile = true;
        h.connectSession(std::nullopt, [](const QJsonObject& real) {
            QJsonObject forged = real;
            forged.remove(QStringLiteral("encoder"));
            forged.remove(QStringLiteral("reason"));
            forged.insert(QStringLiteral("firstSequence"),
                          (real.value(QStringLiteral("firstSequence")).toInteger() + 1000)
                              % 65536);
            return forged;
        });
        QVERIFY(h.server.remoteAudioStatusAvailable());
        QVERIFY(remoteMedia.audioDetailNegotiated());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.acceptedAudioContext().has_value()
                                      && remoteMedia.acceptedAudioContext()->enabled,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        {
            const QList<QJsonObject> received = audioContexts(controls);
            QCOMPARE(h.stationLink->forgedContexts, 1);
            QVERIFY(received.size() >= 2);
            QCOMPARE(received.at(0).size(), 8);
            QCOMPARE(received.at(0).value(QStringLiteral("generation")).toInteger(),
                     received.at(1).value(QStringLiteral("generation")).toInteger());
            QVERIFY(!accepted.contexts.isEmpty());
            QCOMPARE(qint64{accepted.contexts.constFirst().generation},
                     received.at(1).value(QStringLiteral("generation")).toInteger());
            QCOMPARE(qint64{accepted.contexts.constFirst().firstSequence},
                     received.at(1).value(QStringLiteral("firstSequence")).toInteger());
        }
        const int heardBefore = h.remoteBus->heard.size() / 2;
        QTRY_VERIFY_WITH_TIMEOUT(channelEnergy(h.remoteBus->heard, 0, heardBefore) > 0.5
                                 && channelEnergy(h.remoteBus->heard, 1, heardBefore) > 0.5,
                                 15000);

        QStringList boundaryEvents;
        QList<QJsonObject> sentContexts;
        QList<QJsonObject> sentRequests;
        bool mirroredBeforeDelivery = false;
        const auto record = [&](const QString& direction, const QJsonObject& value) {
            if (boundaryEvents.size() >= 32) { return; }
            boundaryEvents.append(QStringLiteral("%1 revision=%2 generation=%3 enabled=%4 reason=%5")
                .arg(direction).arg(value.value(QStringLiteral("revision")).toInteger())
                .arg(value.value(QStringLiteral("generation")).toInteger())
                .arg(value.value(QStringLiteral("enabled")).toBool())
                .arg(value.value(QStringLiteral("reason")).toString()));
        };
        const auto sentConnection = connect(h.stationLink, &Test::LoopbackTransport::outboundText,
            &remoteMedia, [&](const QByteArray& wire) {
                SessionMessage message;
                if (!SessionMessages::decode(wire, &message)
                    || message.kind != SessionMessageKind::MediaControl
                    || message.mediaPayload.value(QStringLiteral("op"))
                        != QLatin1String("audio-context")) { return; }
                if (sentContexts.size() < 32) { sentContexts.append(message.mediaPayload); }
                record(QStringLiteral("Core sent"), message.mediaPayload);
                if (mirrorFirst && !mirroredBeforeDelivery
                    && message.mediaPayload.value(QStringLiteral("reason")).toString()
                        == remoteAudioOffReasonToWire(RemoteAudioOffReason::RadioOffline)) {
                    mirroredBeforeDelivery = true;
                    h.remote.setConnectionStateForTest(ConnectionState::Disconnected);
                }
            });
        const auto requestedConnection = connect(h.stationLink->peerForTest(),
            &Test::LoopbackTransport::outboundText, &remoteMedia, [&](const QByteArray& wire) {
                SessionMessage message;
                if (!SessionMessages::decode(wire, &message)
                    || message.kind != SessionMessageKind::MediaControl
                    || message.mediaPayload.value(QStringLiteral("op"))
                        != QLatin1String("audio")) { return; }
                if (sentRequests.size() < 32) { sentRequests.append(message.mediaPayload); }
                record(QStringLiteral("GUI requested"), message.mediaPayload);
            });
        const auto receivedConnection = connect(&h.client, &StationClient::mediaControlReceived,
            &remoteMedia, [&](const QJsonObject& value, quint32) {
                if (value.value(QStringLiteral("op")) == QLatin1String("audio-context")) {
                    record(QStringLiteral("GUI received"), value);
                }
            });
        const auto releaseObservers = qScopeGuard([&] {
            disconnect(sentConnection);
            disconnect(requestedConnection);
            disconnect(receivedConnection);
        });
        const auto evidence = [&] { return boundaryEvents.join(QLatin1Char('\n')); };

        // The station radio drops with audio wanted. Core says why at once,
        // ahead of the GUI's own mirror of the radio state.
        // Publication is immediate; queued delivery can follow the mirror's
        // newer audio request, which must supersede this older reply.
        h.station.setConnectionStateForTest(ConnectionState::Disconnected);
        std::optional<RemoteAudioContextMessage> offlineContext;
        for (const QJsonObject& payload : sentContexts) {
            const auto context = decodeRemoteAudioContext(payload, true, false);
            if (context && !context->enabled
                && context->offReason == RemoteAudioOffReason::RadioOffline) {
                offlineContext = context;
                break;
            }
        }
        QVERIFY2(offlineContext.has_value(), qPrintable(evidence()));
        QTRY_VERIFY2_WITH_TIMEOUT(([&] {
            for (const QJsonObject& payload : audioContexts(controls)) {
                const auto context = decodeRemoteAudioContext(payload, true, false);
                if (context && context->generation == offlineContext->generation
                    && context->revision == offlineContext->revision
                    && context->offReason == RemoteAudioOffReason::RadioOffline) {
                    return true;
                }
            }
            return false;
        })(), qPrintable(evidence()), 5000);
        // Once the GUI mirrors the offline radio it withdraws its own
        // request, and Core reports that choice.
        QTRY_VERIFY_WITH_TIMEOUT(!h.remote.isConnected(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(accepted.contexts.constLast().offReason
                                     == RemoteAudioOffReason::ClientDisabled, 5000);
        QVERIFY2(!sentRequests.isEmpty(), qPrintable(evidence()));
        QCOMPARE(accepted.contexts.constLast().revision,
                 quint32(sentRequests.constLast().value(QStringLiteral("revision")).toInteger()));
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state,
                                  RemoteAudioStatus::State::RadioOffline, 5000);
        if (mirrorFirst) {
            QVERIFY(mirroredBeforeDelivery);
            QVERIFY(accepted.contexts.constLast().revision > offlineContext->revision);
            QVERIFY2(!accepted.any([&](const RemoteAudioContextMessage& context) {
                return context.generation == offlineContext->generation;
            }), qPrintable(evidence()));
        }

        // The radio returns, and so does audio.
        const int heardBeforeReturn = h.remoteBus->heard.size() / 2;
        h.station.setConnectionStateForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(h.remote.isConnected(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(accepted.contexts.constLast().enabled, 10000);
        QVERIFY(accepted.contexts.constLast().encoder.has_value());
        QTRY_VERIFY_WITH_TIMEOUT(channelEnergy(h.remoteBus->heard, 0, heardBeforeReturn) > 0.5
                                 && channelEnergy(h.remoteBus->heard, 1, heardBeforeReturn) > 0.5,
                                 15000);

        QCOMPARE(accepted.signalsWithoutContext, 0);
        QVERIFY(accepted.eachReportedOnce());
        for (const RemoteAudioContextMessage& context : accepted.contexts) {
            QCOMPARE(context.encoder.has_value(), context.enabled);
            QCOMPARE(context.offReason.has_value(), !context.enabled);
        }
        QCOMPARE(remoteErrors.count(), 0);

        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }
    // R-R3-23: lossless over the real encrypted session. The choice made on
    // this computer goes with the media start and every audio request; the
    // Core sends 4 ms L16 packets over DTLS/SRTP; this computer plays them
    // at the full 1536 kbit/s with nothing missing, and the link trial,
    // which watches the first 5 s and then keeps watching, lets it run. Opus
    // comes back when chosen, and lossless is replayed on reconnect.
    void losslessPlaysOverTheRealEncryptedSession()
    {
        Harness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        const auto restoreChoice = qScopeGuard([&remoteMedia] {
            remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Opus);
        });
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
            "^Remote audio receiving: lossless L16 48000 Hz, 2 channels, 192-sample packets, "
            "16-bit, payload type 96, context \\d+$")));

        // The station and this computer's speaker both keep wall-clock time
        // (timer wakeups can coalesce under load), so the measured rate is
        // the real 48 kHz stream's.
        QElapsedTimer clock;
        clock.start();
        qint64 fedFrames = 0;
        qint64 renderedFrames = 0;
        QTimer source;
        source.setInterval(5);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&] {
            while (fedFrames + kFrames <= clock.nsecsElapsed() * 48 / 1'000'000) {
                h.feedMixedTone();
                fedFrames += kFrames;
            }
        });
        QTimer speaker;
        speaker.setInterval(5);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&] {
            while (renderedFrames + kFrames <= clock.nsecsElapsed() * 48 / 1'000'000) {
                h.remoteBus->render(kFrames);
                renderedFrames += kFrames;
            }
        });
        source.start();
        speaker.start();

        h.connectSession();
        QVERIFY(remoteMedia.audioProfileNegotiated());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                          == RemoteAudioStatus::State::Playing
                                      && remoteMedia.audioStatus().losslessEncoder.has_value(),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        {
            QList<QJsonObject> starts;
            QStringList profiles;
            for (const auto& call : coreControls) {
                const QJsonObject control = call.at(0).toJsonObject();
                if (control.value(QStringLiteral("op")) == QLatin1String("start")) {
                    starts << control;
                } else if (control.value(QStringLiteral("op")) == QLatin1String("audio")) {
                    profiles << control.value(QStringLiteral("profile")).toString();
                }
            }
            QCOMPARE(starts.size(), 1);
            QCOMPARE(starts.constFirst().value(QStringLiteral("audioProfileVersion")).toInteger(),
                     qint64{1});
            QVERIFY(!profiles.isEmpty());
            for (const QString& profile : profiles) { QCOMPARE(profile, QStringLiteral("lossless")); }
        }
        RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(*status.losslessEncoder, l16EncoderProfile());
        QCOMPARE(remoteAudioCodecText(status),
                 QStringLiteral("Lossless stereo, 16-bit, 1536 kbit/s, 4 ms packets"));
        QCOMPARE(remoteAudioQualityText(status), QStringLiteral("Lossless"));
        QVERIFY(!status.qualityReason.has_value());

        // Past the trial window, at the full rate, with nothing missing.
        const RemoteAudioReceiverTelemetry before = remoteMedia.audioTelemetry();
        QElapsedTimer measured;
        measured.start();
        QTest::qWait(int(RemoteAudioLinkTrial::kWindowMs) + 1500);
        const RemoteAudioReceiverTelemetry after = remoteMedia.audioTelemetry();
        const double seconds = measured.elapsed() / 1000.0;
        QCOMPARE(after.generation, before.generation); // no restart
        const double kbps = double(after.receivedAudioPayloadBytes
                                   - before.receivedAudioPayloadBytes) * 8.0 / 1000.0 / seconds;
        const quint64 decoded = after.decodedPackets - before.decodedPackets;
        qInfo() << "lossless session:" << decoded << "packets decoded in" << seconds
                << "s," << kbps << "kbit/s of audio, missing" << after.missingPackets
                << "of" << after.expectedPackets << ", filled" << after.concealedPackets;
        QVERIFY2(kbps > 1400.0 && kbps < 1700.0, qPrintable(QString::number(kbps)));
        QVERIFY(decoded >= quint64(seconds * 250 * 0.9));
        QCOMPARE(after.missingPackets, quint64(0));
        QVERIFY(after.concealedPackets <= 2);
        status = remoteMedia.audioStatus();
        QCOMPARE(status.state, RemoteAudioStatus::State::Playing);
        QVERIFY(status.losslessEncoder.has_value());
        QVERIFY(!status.qualityReason.has_value());
        QCOMPARE(remoteErrors.count(), 0);
        // The tones stay where the station mixed them.
        const int from = h.remoteBus->heard.size() / 2 - 48000;
        QVERIFY(toneAmplitude(h.remoteBus->heard, 0, 617.0, from)
                > 8.0 * toneAmplitude(h.remoteBus->heard, 1, 617.0, from));
        QVERIFY(toneAmplitude(h.remoteBus->heard, 1, 1579.0, from)
                > 8.0 * toneAmplitude(h.remoteBus->heard, 0, 1579.0, from));

        // Back to Opus when chosen.
        remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Opus);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state == RemoteAudioStatus::State::Playing
                                     && remoteMedia.audioStatus().encoder.has_value()
                                     && !remoteMedia.audioStatus().losslessEncoder.has_value(),
                                 10000);
        QCOMPARE(remoteAudioQualityText(remoteMedia.audioStatus()), QStringLiteral("Opus"));

        // Lossless again, then a reconnect replays it without being asked.
        remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Lossless);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().losslessEncoder.has_value(), 10000);
        h.client.disconnectFromStation(QStringLiteral("test reconnect"));
        QTRY_VERIFY_WITH_TIMEOUT(!h.client.mediaAvailable(), 5000);
        QVERIFY(!remoteMedia.audioStatus().losslessEncoder.has_value());
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                          == RemoteAudioStatus::State::Playing
                                      && remoteMedia.audioStatus().losslessEncoder.has_value(),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QCOMPARE(remoteErrors.count(), 0);

        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-43: apps on this computer hear each receiver on its own stream,
    // with the speakers muted. The slice-B app gets slice B's 1579 Hz, not
    // slice A's 617 Hz, and the other way round; the speakers stay silent
    // and the station's mix is untouched. Each stream runs only while an
    // app listens, and releasing one leaves the other running.
    void receiverStreamsReachAppsWhileTheSpeakersAreMuted()
    {
        Harness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        Test::CollectingReceiverSink appA;
        Test::CollectingReceiverSink appB;
        Test::CollectingReceiverSink secondAppB;
        const auto releaseAll = qScopeGuard([&] {
            remoteMedia.releaseReceiverAudio(h.sliceA, &appA);
            remoteMedia.releaseReceiverAudio(h.sliceB, &appB);
            remoteMedia.releaseReceiverAudio(h.sliceB, &secondAppB);
        });
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(kFrames); });
        source.start();
        speaker.start();

        h.connectSession();
        QVERIFY(remoteMedia.receiverAudioNegotiated());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const QList<QJsonObject> starts = [&] {
            QList<QJsonObject> found;
            for (const auto& call : coreControls) {
                const QJsonObject control = call.at(0).toJsonObject();
                if (control.value(QStringLiteral("op")) == QLatin1String("start")) {
                    found << control;
                }
            }
            return found;
        }();
        QCOMPARE(starts.size(), 1);
        QCOMPARE(starts.constFirst().value(QStringLiteral("receiverAudioVersion")).toInteger(),
                 qint64{1});

        // Speakers muted first: nothing an app does may need them.
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedAudioContext()
                                     && !remoteMedia.acceptedAudioContext()->enabled, 5000);
        const double stationPanA = h.station.sliceById(h.sliceA)->audioPan();
        const double stationPanB = h.station.sliceById(h.sliceB)->audioPan();
        const bool stationMutedA = h.station.sliceById(h.sliceA)->muted();
        const bool stationMutedB = h.station.sliceById(h.sliceB)->muted();
        const int heardAtMute = h.remoteBus->heard.size() / 2;

        remoteMedia.requestReceiverAudio(h.sliceA, &appA);
        remoteMedia.requestReceiverAudio(h.sliceB, &appB);
        remoteMedia.requestReceiverAudio(h.sliceB, &secondAppB); // shares B's stream
        QTRY_VERIFY_WITH_TIMEOUT(appA.frames(h.sliceA) >= 48000 && appB.frames(h.sliceB) >= 48000
                                     && secondAppB.frames(h.sliceB) >= 48000, 15000);
        // One request per slice, however many apps listen.
        QList<QJsonObject> requests;
        for (const auto& call : coreControls) {
            const QJsonObject control = call.at(0).toJsonObject();
            if (control.value(QStringLiteral("op")) == QLatin1String("receiver-audio")) {
                requests << control;
            }
        }
        QCOMPARE(requests.size(), 2);
        for (const QJsonObject& request : requests) {
            QStringList keys = request.keys();
            keys.sort();
            QCOMPARE(keys, (QStringList{QStringLiteral("connectionId"), QStringLiteral("enabled"),
                                        QStringLiteral("op"), QStringLiteral("profile"),
                                        QStringLiteral("revision"), QStringLiteral("sliceId")}));
            QVERIFY(request.value(QStringLiteral("enabled")).toBool());
            QCOMPARE(request.value(QStringLiteral("profile")).toString(), QStringLiteral("opus"));
        }

        // Past the codec's start, each app hears its own slice.
        const QVector<float> b = appB.audio(h.sliceB);
        const QVector<float> a = appA.audio(h.sliceA);
        constexpr int kSettle = 4800;
        for (int channel : {0, 1}) {
            const double b1579 = Test::toneAmplitude(b, channel, Harness::kSliceBToneHz, kSettle);
            const double b617 = Test::toneAmplitude(b, channel, Harness::kSliceAToneHz, kSettle);
            const double a617 = Test::toneAmplitude(a, channel, Harness::kSliceAToneHz, kSettle);
            const double a1579 = Test::toneAmplitude(a, channel, Harness::kSliceBToneHz, kSettle);
            qInfo() << "receiver streams, channel" << channel << "B 1579/617" << b1579 << b617
                    << "A 617/1579" << a617 << a1579;
            QVERIFY(b1579 > 0.1);
            QVERIFY(b1579 > 8.0 * b617);
            QVERIFY(a617 > 0.1);
            QVERIFY(a617 > 8.0 * a1579);
        }
        // Both apps on B hear the whole stream, not half each.
        QVERIFY(std::abs(appB.frames(h.sliceB) - secondAppB.frames(h.sliceB)) <= 3840);
        QVERIFY(appA.stops().isEmpty());
        QVERIFY(appB.stops().isEmpty());

        // The speakers stayed muted and silent, the speakers' stream stayed
        // off, and the station's mix is as it was.
        QCOMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::MutedHere);
        QVERIFY(!remoteMedia.acceptedAudioContext()->enabled);
        QVERIFY(channelEnergy(h.remoteBus->heard, 0, heardAtMute) < 1e-9);
        QVERIFY(channelEnergy(h.remoteBus->heard, 1, heardAtMute) < 1e-9);
        QCOMPARE(h.station.sliceById(h.sliceA)->audioPan(), stationPanA);
        QCOMPARE(h.station.sliceById(h.sliceB)->audioPan(), stationPanB);
        QCOMPARE(h.station.sliceById(h.sliceA)->muted(), stationMutedA);
        QCOMPARE(h.station.sliceById(h.sliceB)->muted(), stationMutedB);
        QCOMPARE(audioContexts(controls).constLast().value(QStringLiteral("reason")).toString(),
                 QStringLiteral("client-disabled"));

        // The remote audio status names each stream, its state and health.
        const RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.receivers.size(), 2);
        for (const RemoteReceiverAudioStatus& receiver : status.receivers) {
            QCOMPARE(receiver.state, RemoteReceiverAudioStatus::State::Receiving);
            QCOMPARE(receiver.runningProfile,
                     std::optional<RemoteAudioProfile>(RemoteAudioProfile::Opus));
        }
        const QString details = formatRemoteAudioDetails(
            status, remoteMedia.audioTelemetry(), remoteMedia.audioDelay(),
            remoteMedia.receiverAudioTelemetry());
        // R-R3-43 / R-R3-23: each receiver stream reached this window at the
        // Core's 48 kbit/s receiver rate while the speakers' stream kept the
        // default 24 kbit/s; the window decoded them with no rate of its own
        // (the unchanged receiver path an older window runs), and its status
        // names the rate the Core reported.
        QList<QJsonObject> receiverEncoders;
        for (const auto& call : controls) {
            const QJsonObject message = call.at(0).toJsonObject();
            if (message.value(QStringLiteral("op")) == QLatin1String("receiver-audio-context")
                && message.value(QStringLiteral("enabled")).toBool()) {
                receiverEncoders << message.value(QStringLiteral("encoder")).toObject();
            }
        }
        QVERIFY(receiverEncoders.size() >= 2);
        QJsonObject receiverEncoderJson = defaultEncoderJson();
        receiverEncoderJson.insert(QStringLiteral("targetBitrate"), 48000);
        receiverEncoderJson.insert(QStringLiteral("audioBandwidthHz"), 20000);
        for (const QJsonObject& encoder : receiverEncoders) {
            QCOMPARE(encoder, receiverEncoderJson);
        }
        bool sawMainEncoder = false;
        for (const QJsonObject& context : audioContexts(controls)) {
            if (context.contains(QStringLiteral("encoder"))) {
                QCOMPARE(context.value(QStringLiteral("encoder")).toObject(), defaultEncoderJson());
                sawMainEncoder = true;
            }
        }
        QVERIFY(sawMainEncoder);
        for (const RemoteReceiverAudioStatus& receiver : status.receivers) {
            QVERIFY(receiver.encoder.has_value());
            QCOMPARE(receiver.encoder->targetBitrate, 48000);
        }
        QVERIFY2(details.contains(QStringLiteral("Receiver A for apps: Receiving, Opus 48\u00A0kbit/s\n"
                                                 "Receiver A: arrival jitter ")),
                 qPrintable(details));
        QVERIFY2(details.contains(QStringLiteral("Receiver B for apps: Receiving, Opus 48\u00A0kbit/s")),
                 qPrintable(details));
        QVERIFY2(details.contains(QStringLiteral(", gaps filled ")), qPrintable(details));

        // One app on B leaves: B keeps running for the other.
        remoteMedia.releaseReceiverAudio(h.sliceB, &appB);
        const int appBFrames = appB.frames(h.sliceB);
        const int secondBFrames = secondAppB.frames(h.sliceB);
        QTRY_VERIFY_WITH_TIMEOUT(secondAppB.frames(h.sliceB) >= secondBFrames + 9600, 5000);
        QCOMPARE(appB.frames(h.sliceB), appBFrames);
        // The last app on B leaves: the Core is asked to stop B, and A plays on.
        remoteMedia.releaseReceiverAudio(h.sliceB, &secondAppB);
        const auto lastRequest = [&](int sliceId) {
            QJsonObject found;
            for (const auto& call : coreControls) {
                const QJsonObject control = call.at(0).toJsonObject();
                if (control.value(QStringLiteral("op")) == QLatin1String("receiver-audio")
                    && control.value(QStringLiteral("sliceId")).toInt() == sliceId) {
                    found = control;
                }
            }
            return found;
        };
        QTRY_VERIFY_WITH_TIMEOUT(!lastRequest(h.sliceB).value(QStringLiteral("enabled")).toBool(),
                                 5000);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.activeReceiverAudioStreamCount(), 1, 5000);
        const int secondBStopped = secondAppB.frames(h.sliceB);
        const int aFrames = appA.frames(h.sliceA);
        QTRY_VERIFY_WITH_TIMEOUT(appA.frames(h.sliceA) >= aFrames + 9600, 5000);
        QCOMPARE(secondAppB.frames(h.sliceB), secondBStopped);
        QCOMPARE(remoteMedia.audioStatus().receivers.size(), 1);

        // Unmuting brings the speakers back beside the app's stream.
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state,
                                  RemoteAudioStatus::State::Playing, 10000);
        const int aBeforeUnmute = appA.frames(h.sliceA);
        QTRY_VERIFY_WITH_TIMEOUT(appA.frames(h.sliceA) >= aBeforeUnmute + 9600, 5000);
        QCOMPARE(remoteErrors.count(), 0);

        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-43: a Core from before receiver audio. No receiver request goes
    // out, an app is told in plain words, and no receiver-audio field is
    // sent. Independent negotiated features still appear on the start.
    void hiddenReceiverAudioKeepsTodaysControls()
    {
        Harness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        Test::CollectingReceiverSink app;
        const auto release = qScopeGuard([&] { remoteMedia.releaseReceiverAudio(h.sliceB, &app); });
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(kFrames); });
        source.start();
        speaker.start();

        // Asked before any connection: not ready yet.
        remoteMedia.requestReceiverAudio(h.sliceB, &app);
        QCOMPARE(app.stops().size(), 1);
        QCOMPARE(app.stops().constLast().second, QStringLiteral("media-not-ready"));

        h.hideReceiverAudio = true;
        // Desktop remote transmit: such a Core predates the microphone line
        // too (the window is sent no remoteTxVersion).
        h.declareRemoteTx = false;
        // R-R3-45: such a Core predates the headphones mix too.
        h.hideHeadphonesMix = true;
        // Parity Task 32: and the transmit monitor.
        h.hideTxMonitorAudio = true;
        h.connectSession();
        QVERIFY(remoteMedia.audioProfileNegotiated());
        QVERIFY(!remoteMedia.receiverAudioNegotiated());
        QVERIFY(!remoteMedia.headphonesMixNegotiated());
        // Receiver audio is independent of the negotiated quality choice.
        QVERIFY(remoteMedia.audioQualityNegotiated());
        QCOMPARE(h.client.capabilities().mediaRelayRoutingVersion, 1);
        QCOMPARE(h.client.capabilities().mediaTunnelVersion, 1);
        QCOMPARE(h.client.capabilities().miniDisplayVersion, 1);
        QCOMPARE(h.client.capabilities().remoteIqVersion, 1);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTRY_VERIFY(!app.stops().isEmpty()
                    && app.stops().constLast().second
                        == QLatin1String(RemoteMediaController::kReceiverAudioUnavailableReason));
        QCOMPARE(app.stops().constLast().first, h.sliceB);
        QCOMPARE(QString::fromLatin1(RemoteMediaController::kReceiverAudioUnavailableReason),
                 QStringLiteral("This Core cannot send a receiver's audio."));
        // A second app on the same receiver hears the same.
        Test::CollectingReceiverSink second;
        remoteMedia.requestReceiverAudio(h.sliceB, &second);
        QCOMPARE(second.stops().size(), 1);
        QCOMPARE(second.stops().constFirst().second,
                 QLatin1String(RemoteMediaController::kReceiverAudioUnavailableReason));
        remoteMedia.releaseReceiverAudio(h.sliceB, &second);

        // Mute and unmute send today's audio controls too.
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedAudioContext()
                                     && !remoteMedia.acceptedAudioContext()->enabled, 5000);
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state,
                                  RemoteAudioStatus::State::Playing, 10000);
        QTest::qWait(300);

        const QStringList audioKeys{QStringLiteral("connectionId"), QStringLiteral("enabled"),
                                    QStringLiteral("op"), QStringLiteral("opusBitrate"),
                                    QStringLiteral("profile"),
                                    QStringLiteral("revision")};
        int starts = 0;
        int audio = 0;
        for (const auto& call : coreControls) {
            const QJsonObject control = call.at(0).toJsonObject();
            const QString op = control.value(QStringLiteral("op")).toString();
            QVERIFY2(op != QLatin1String("receiver-audio"), "no receiver request to this Core");
            QStringList keys = control.keys();
            keys.sort();
            if (op == QLatin1String("start")) {
                ++starts;
                QVERIFY(startMatches(control, modernStartWithoutConnection(true)));
            } else if (op == QLatin1String("audio")) {
                ++audio;
                QCOMPARE(keys, audioKeys);
                QCOMPARE(control.value(QStringLiteral("opusBitrate")).toInt(), 48000);
            }
        }
        QCOMPARE(starts, 1);
        QVERIFY(audio >= 3);
        QCOMPARE(app.frames(h.sliceB), 0);
        QCOMPARE(daemonMedia.activeReceiverAudioStreamCount(), 0);
        // The status says why, in the operator's words.
        const RemoteAudioStatus status = remoteMedia.audioStatus();
        QCOMPARE(status.receivers.size(), 1);
        QCOMPARE(status.receivers.constFirst().state, RemoteReceiverAudioStatus::State::Stopped);
        QVERIFY(formatRemoteAudioDetails(status, remoteMedia.audioTelemetry())
                    .contains(QStringLiteral("Receiver B for apps: Stopped. This Core cannot send "
                                             "a receiver's audio.")));

        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-45: headphones in a remote window, over the real encrypted
    // session. Routing slice B to the headphones on this computer's flag
    // writes the Core's slice (the route is a mirrored slice property the
    // Core owns and saves), and the Core starts a second mix on its own
    // stream. The headphones play B's tone (1579 Hz) and not A's; the
    // speakers keep A (617 Hz) and lose B. Routing B back stops the second
    // stream and B returns to the speakers. The speakers' stream is never
    // restarted by any of it.
    void headphonesPlayTheirReceiverWhileTheSpeakersKeepTheOther()
    {
        Harness h;
        const auto routes = qScopeGuard([&h] { h.resetOutputRoutes(); });
        h.attachRemoteHeadphones();
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer devices;
        devices.setInterval(10);
        devices.setTimerType(Qt::PreciseTimer);
        connect(&devices, &QTimer::timeout, &devices, [&h] {
            h.remoteBus->render(kFrames);
            h.remoteHeadphonesBus->render(kFrames);
        });
        source.start();
        devices.start();

        h.connectSession();
        QVERIFY(remoteMedia.headphonesMixNegotiated());
        QVERIFY(remoteMedia.headphonesProblem().isEmpty());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        const auto startsOf = [&coreControls] {
            QList<QJsonObject> found;
            for (const auto& call : coreControls) {
                const QJsonObject control = call.at(0).toJsonObject();
                if (control.value(QStringLiteral("op")) == QLatin1String("start")) {
                    found << control;
                }
            }
            return found;
        };
        QCOMPARE(startsOf().size(), 1);
        QCOMPARE(startsOf().constFirst().value(QStringLiteral("headphonesMixVersion")).toInteger(),
                 qint64{1});
        // Asked for (this computer has headphones), off while nothing is
        // routed there.
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedHeadphonesContext().has_value(), 5000);
        QVERIFY(!remoteMedia.acceptedHeadphonesContext()->enabled);
        QCOMPARE(remoteMedia.acceptedHeadphonesContext()->offReason,
                 std::optional{RemoteAudioOffReason::NoHeadphonesReceiver});
        const int mainContexts = int(audioContexts(controls).size());

        // B to the headphones, from the remote window's flag.
        SliceModel* const remoteB = h.remote.sliceById(h.sliceB);
        QVERIFY(remoteB != nullptr);
        remoteB->setOutputRoute(SliceModel::OutputRoute::Headphones);
        QTRY_COMPARE_WITH_TIMEOUT(h.station.sliceById(h.sliceB)->outputRoute(),
                                  SliceModel::OutputRoute::Headphones, 5000);
        // The Core saved it (Core and window share one settings store in
        // this test; tst_slice_model_phase3f_properties shows a remote
        // window's slice writes none of its own).
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Slice%1/OutputRoute").arg(h.sliceB)).toString(),
                 QStringLiteral("Headphones"));
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.headphonesMixSending(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedHeadphonesContext()
                                     && remoteMedia.acceptedHeadphonesContext()->enabled, 5000);
        const int speakerFrom = int(h.remoteBus->heard.size() / 2);
        const int headphonesFrom = int(h.remoteHeadphonesBus->heard.size() / 2);
        QTRY_VERIFY_WITH_TIMEOUT(
            h.remoteHeadphonesBus->heard.size() / 2 >= headphonesFrom + 96000
                && channelEnergy(h.remoteHeadphonesBus->heard, 1, headphonesFrom + 48000) > 1.0
                && h.remoteBus->heard.size() / 2 >= speakerFrom + 96000,
            20000);
        // Past the codec's start and the route's own crossfade.
        const int hpSettle = headphonesFrom + 48000;
        const int spkSettle = speakerFrom + 48000;
        const QVector<float> headphones = h.remoteHeadphonesBus->heard;
        const QVector<float> speakers = h.remoteBus->heard;
        const double hp1579 = toneAmplitude(headphones, 1, 1579.0, hpSettle);
        const double hp617 = std::max(toneAmplitude(headphones, 0, 617.0, hpSettle),
                                      toneAmplitude(headphones, 1, 617.0, hpSettle));
        const double spk617 = toneAmplitude(speakers, 0, 617.0, spkSettle);
        const double spk1579 = std::max(toneAmplitude(speakers, 0, 1579.0, spkSettle),
                                        toneAmplitude(speakers, 1, 1579.0, spkSettle));
        qInfo() << "headphones 1579/617" << hp1579 << hp617 << "speakers 617/1579" << spk617
                << spk1579;
        QVERIFY(hp1579 > 0.05);
        QVERIFY(hp1579 > 8.0 * hp617);
        QVERIFY(spk617 > 0.05);
        QVERIFY(spk617 > 8.0 * spk1579);
        // The speakers' stream carried on: no new context for it.
        QCOMPARE(int(audioContexts(controls).size()), mainContexts);
        QCOMPARE(remoteMedia.audioStatus().state, RemoteAudioStatus::State::Playing);
        QVERIFY(remoteMedia.headphonesTelemetry().running);
        QCOMPARE(remoteErrors.count(), 0);

        // B back to the speakers: the second stream stops, B returns there.
        remoteB->setOutputRoute(SliceModel::OutputRoute::Speakers);
        QTRY_VERIFY_WITH_TIMEOUT(!daemonMedia.headphonesMixSending(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedHeadphonesContext()
                                     && !remoteMedia.acceptedHeadphonesContext()->enabled, 5000);
        QCOMPARE(remoteMedia.acceptedHeadphonesContext()->offReason,
                 std::optional{RemoteAudioOffReason::NoHeadphonesReceiver});
        QVERIFY(!remoteMedia.headphonesTelemetry().running);
        // B plays there again at its own gain (0.45 of the tone, right-
        // panned), well clear of the silence it had while on the headphones.
        // Measured over the newest half second, once the route change has
        // crossed the session and the jitter hold (its timing is not what
        // is under test).
        const auto newest1579 = [&h] {
            const int frames = int(h.remoteBus->heard.size() / 2);
            return frames < 24000 ? 0.0
                                  : toneAmplitude(h.remoteBus->heard, 1, 1579.0, frames - 24000);
        };
        QTRY_VERIFY_WITH_TIMEOUT(newest1579() > 0.03, 15000);
        const double back1579 = newest1579();
        qInfo() << "speakers 1579 after B came back" << back1579;
        QVERIFY(back1579 > 100.0 * spk1579);
        QCOMPARE(int(audioContexts(controls).size()), mainContexts);
        QCOMPARE(remoteErrors.count(), 0);

        source.stop();
        devices.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Remote-window parity Task 32 (R-IOS-13, R-R3-49): the transmit monitor
    // over the real encrypted session, measured after Opus decode at the
    // Core's setting (48 kbit/s full band). A 1031 Hz tone stands for the
    // transmitter's siphon (handed to txMonitorBlockReady as TxChannel would
    // while keyed; nothing is keyed). This window holds transmit (a take)
    // with MON on at 0.5: its main stream carries the tone at 0.5 of it
    // within 1 dB; routed to the headphones it moves to the headphones
    // stream (which runs for it though no receiver is there) and leaves the
    // main one; route none, or MON off, carries it nowhere.
    void theHolderHearsItsMonitorAtItsLevel()
    {
        Harness h;
        h.attachRemoteHeadphones();
        // This computer's speaker volume at full, so what is heard is what
        // the Core sent (the headphones carry no master volume).
        h.remote.audioEngine()->setVolume(1.0f);
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        constexpr double kMonHz = 1031.0;
        constexpr double kMonIn = 0.3;
        constexpr double kMonLevel = 0.5;
        qint64 monFrames = 0;
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h, &monFrames] {
            std::vector<float> mono(kFrames);
            for (int frame = 0; frame < kFrames; ++frame) {
                const double time = static_cast<double>(monFrames + frame) / 48000.0;
                mono[static_cast<size_t>(frame)] =
                    static_cast<float>(kMonIn * std::sin(2.0 * kPi * kMonHz * time));
            }
            monFrames += kFrames;
            h.stationAudio->txMonitorBlockReady(mono.data(), kFrames);
            h.feedMixedTone();
        });
        QTimer devices;
        devices.setInterval(10);
        devices.setTimerType(Qt::PreciseTimer);
        connect(&devices, &QTimer::timeout, &devices, [&h] {
            h.remoteBus->render(kFrames);
            h.remoteHeadphonesBus->render(kFrames);
        });
        source.start();
        devices.start();

        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QVERIFY(remoteMedia.txMonitorAudioNegotiated());
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedMonitorContext().has_value(), 5000);

        // The monitor's level in the newest second a device played: the
        // median of twenty 50 ms measurements, so a playback hiccup on a
        // loaded machine (a jitter-hold gap, a concealed packet) does not
        // count against the level.
        const auto monitorOn = [](const QVector<float>& heard, int fromFrame) {
            constexpr int kWindow = 2400;
            const int frames = int(heard.size() / 2);
            std::vector<double> levels;
            for (int start = fromFrame; start + kWindow <= frames; start += kWindow) {
                const QVector<float> part = heard.mid(qsizetype(start) * 2, kWindow * 2);
                levels.push_back(std::max(toneAmplitude(part, 0, kMonHz),
                                          toneAmplitude(part, 1, kMonHz)));
            }
            if (levels.empty()) {
                return 0.0;
            }
            std::sort(levels.begin(), levels.end());
            return levels[levels.size() / 2];
        };
        const auto newest = [](const PacedAudioBus* bus) {
            return std::max(0, int(bus->heard.size() / 2) - 48000);
        };
        const auto waitSecond = [&h](const PacedAudioBus* bus) {
            const int from = int(bus->heard.size() / 2);
            QTRY_VERIFY_WITH_TIMEOUT(bus->heard.size() / 2 >= from + 72000, 20000);
        };

        // Not yet the holder: the main stream has none.
        waitSecond(h.remoteBus);
        QVERIFY(monitorOn(h.remoteBus->heard, newest(h.remoteBus)) < 0.01);

        // Take transmit (never keys), MON on at 0.5.
        TransmitHolder::Holder self;
        self.deviceId = h.server.mediaSessionDevice(h.server.mediaSessionEpoch());
        h.server.transmitHolder()->transferTo(self, QStringLiteral("test"));
        h.station.transmitModel().setMonitorVolume(float(kMonLevel));
        h.stationAudio->setTxMonitorVolume(float(kMonLevel));
        h.station.transmitModel().setMonEnabled(true);
        h.stationAudio->setTxMonitorEnabled(true);
        QCOMPARE(daemonMedia.txMonitorRoute(), TxMonitorRoute::Speakers);
        QVERIFY(!h.stationAudio->txMonitorLocal());
        waitSecond(h.remoteBus);
        const double expected = kMonIn * kMonLevel;
        const double onMain = monitorOn(h.remoteBus->heard, newest(h.remoteBus));
        qInfo() << "monitor on the main stream" << onMain << "expected" << expected;
        QVERIFY2(std::fabs(20.0 * std::log10(onMain / expected)) < 1.0,
                 qPrintable(QStringLiteral("main %1 against %2").arg(onMain).arg(expected)));
        // The receivers still play beside it.
        QVERIFY(toneAmplitude(h.remoteBus->heard, 0, 617.0, newest(h.remoteBus)) > 0.05);

        // To the headphones.
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::Headphones);
        QTRY_VERIFY_WITH_TIMEOUT(daemonMedia.headphonesMixSending(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.acceptedHeadphonesContext()
                                     && remoteMedia.acceptedHeadphonesContext()->enabled, 5000);
        waitSecond(h.remoteHeadphonesBus);
        waitSecond(h.remoteHeadphonesBus);
        const double onPhones = monitorOn(h.remoteHeadphonesBus->heard,
                                          newest(h.remoteHeadphonesBus));
        const double leftOnMain = monitorOn(h.remoteBus->heard, newest(h.remoteBus));
        qInfo() << "monitor on the headphones" << onPhones << "left on the main" << leftOnMain;
        QVERIFY2(std::fabs(20.0 * std::log10(onPhones / expected)) < 1.0,
                 qPrintable(QStringLiteral("headphones %1 against %2").arg(onPhones).arg(expected)));
        QVERIFY(leftOnMain < 0.1 * expected);

        // Route none: nowhere, and the headphones mix has nothing to carry.
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::None);
        QTRY_VERIFY_WITH_TIMEOUT(!daemonMedia.headphonesMixSending(), 5000);
        waitSecond(h.remoteBus);
        QVERIFY(monitorOn(h.remoteBus->heard, newest(h.remoteBus)) < 0.1 * expected);

        // Back to the main stream, then MON off: nowhere.
        remoteMedia.setTxMonitorRoute(TxMonitorRoute::Speakers);
        QTRY_COMPARE_WITH_TIMEOUT(daemonMedia.txMonitorRoute(), TxMonitorRoute::Speakers, 5000);
        waitSecond(h.remoteBus);
        QVERIFY(monitorOn(h.remoteBus->heard, newest(h.remoteBus)) > 0.5 * expected);
        h.station.transmitModel().setMonEnabled(false);
        h.stationAudio->setTxMonitorEnabled(false);
        waitSecond(h.remoteBus);
        QVERIFY(monitorOn(h.remoteBus->heard, newest(h.remoteBus)) < 0.1 * expected);
        QCOMPARE(remoteErrors.count(), 0);

        h.server.transmitHolder()->transferTo(std::nullopt, QStringLiteral("test"));
        source.stop();
        devices.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // R-R3-35: measured delay over the real encrypted session, with the
    // Core's clock 5 s ahead of this computer's. The station is silent,
    // then a 200 Hz cosine begins at full height on a known frame. Truth is
    // kept on one timeline, to the sample: the station captures steadily
    // (frame f at f / 48 kHz), and this computer's speaker plays steadily
    // (heard frame k at the device clock's origin plus k / 48 kHz), so
    // neither timer's lateness enters it. The heard onset is where the
    // cosine first reaches half its height. The reported delay must match
    // within its own accuracy plus half a millisecond, so an 8 ms miss (the
    // Opus lookahead and the rate matcher's filter left out) fails. Muting
    // ends the audio context and the figure with it; unmuting brings it
    // back.
    void measuredDelayMatchesTheHeardDelay_data()
    {
        QTest::addColumn<bool>("lossless");
        QTest::newRow("opus") << false;
        QTest::newRow("lossless") << true;
    }

    void measuredDelayMatchesTheHeardDelay()
    {
        QFETCH(bool, lossless);
        constexpr qint64 kCoreAheadNs = 5'000'000'000;
        // What the model leaves: where the half-height crossing falls after
        // the codec's and the filter's reconstruction (under a frame), and
        // the frame conventions at each end.
        constexpr double kToleranceMs = 0.5;
        constexpr double kOnsetAmplitude = 0.5;
        constexpr double kOnsetHz = 200.0;
        constexpr qint64 kNsPerFrame48 = 1'000'000; // ns per 48 frames
        Harness h;
        // The one timeline every truth below is kept on.
        QElapsedTimer clock;
        clock.start();
        const auto dueNs = [](qint64 frame) { return frame * kNsPerFrame48 / 48; };
        // A steady radio delivers each 480-frame block when its last frame
        // is due. While a block is fed the Core's capture clock reads that
        // due time, so its capture stamp is the steady capture time however
        // late the feed timer woke. Every other Core clock read, the probe
        // times among them, is the real clock.
        const Qt::HANDLE feeder = QThread::currentThreadId();
        std::atomic<qint64> feedingDueNs{-1};
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station, nullptr, {},
            [&clock, &feedingDueNs, feeder] {
                if (QThread::currentThreadId() == feeder) {
                    const qint64 due = feedingDueNs.load();
                    if (due >= 0) { return kCoreAheadNs + due; }
                }
                return kCoreAheadNs + clock.nsecsElapsed();
            });
        const auto restoreChoice = qScopeGuard([&remoteMedia] {
            remoteMedia.setAudioProfileChoice(RemoteAudioProfile::Opus);
        });
        remoteMedia.setAudioProfileChoice(lossless ? RemoteAudioProfile::Lossless
                                                   : RemoteAudioProfile::Opus);
        QSignalSpy remoteErrors(&remoteMedia, &RemoteMediaController::errorOccurred);
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);
        // This computer's speaker plays on the same timeline, continuously,
        // taking its queue a 48-frame callback at a time, as the callback
        // device it reports does.
        h.remoteBus->callbackFrames = 48;
        h.remoteBus->setPlayClockForTesting([&clock] { return clock.nsecsElapsed(); });
        const qint64 playOriginNs = h.remoteBus->playClockOriginNs();

        qint64 fedFrames = 0;
        qint64 onsetFrame = -1;       // the first cosine frame, once chosen
        qint64 roughHeardFrame = -1;  // first heard frame clearly not silent
        RemoteAudioDelayReport reportAtOnset;
        QTimer source;
        source.setInterval(2);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&] {
            while (fedFrames + kFrames <= clock.nsecsElapsed() * 48 / 1'000'000) {
                QVector<float> a(kFrames * 2, 0.0f);
                const QVector<float> silence(kFrames * 2, 0.0f);
                for (int frame = 0; frame < kFrames; ++frame) {
                    const qint64 f = fedFrames + frame;
                    if (onsetFrame >= 0 && f >= onsetFrame) {
                        const double t = double(f - onsetFrame) / 48000.0;
                        a[frame * 2] = a[frame * 2 + 1] =
                            float(kOnsetAmplitude * std::cos(2.0 * Harness::kPi * kOnsetHz * t));
                    }
                }
                feedingDueNs.store(dueNs(fedFrames + kFrames));
                h.stationAudio->rxBlockReady(h.sliceA, a.constData(), kFrames);
                h.stationAudio->rxBlockReady(h.sliceB, silence.constData(), kFrames);
                feedingDueNs.store(-1);
                h.stationFrames += kFrames;
                fedFrames += kFrames;
            }
        });
        QTimer speaker;
        speaker.setInterval(2);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&] {
            const qint64 before = h.remoteBus->heard.size() / 2;
            if (h.remoteBus->renderDue() <= 0 || onsetFrame < 0 || roughHeardFrame >= 0) {
                return;
            }
            for (qint64 k = before; k < h.remoteBus->heard.size() / 2; ++k) {
                if (std::abs(h.remoteBus->heard.at(k * 2)) > 0.02f) {
                    roughHeardFrame = k;
                    reportAtOnset = remoteMedia.audioDelay();
                    break;
                }
            }
        });
        source.start();
        speaker.start();

        h.connectSession();
        QVERIFY(remoteMedia.audioClockNegotiated());
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                          == RemoteAudioStatus::State::Playing
                                      && (!lossless
                                          || remoteMedia.audioStatus().losslessEncoder.has_value()),
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        // Echoes arrive and a delay is measured while silence plays.
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioDelay().estimate.has_value(), 5000);
        QVERIFY(remoteMedia.audioDelay().measurable);
        // Silence plays until the clock probes have settled what the checks
        // below need (two probes on the wire, a bound under 8 ms), instead
        // of a fixed 3 s.
        QTRY_VERIFY2_WITH_TIMEOUT(
            clockProbesIn(coreControls) >= 2
                && remoteMedia.audioDelay().estimate.has_value()
                && remoteMedia.audioDelay().estimate->boundMs + kToleranceMs < 8.0,
            "the clock probes did not settle the delay estimate", 10000);

        // Mid-block, so a block boundary is not what is found.
        onsetFrame = ((fedFrames / kFrames) + 20) * kFrames + 177;
        QTRY_VERIFY_WITH_TIMEOUT(roughHeardFrame >= 0, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size() / 2 >= roughHeardFrame + 960, 5000);
        QVERIFY(reportAtOnset.measurable);
        QVERIFY(reportAtOnset.estimate.has_value());
        const AudioDelayEstimate& estimate = *reportAtOnset.estimate;
        // The heard onset to the sample: the first frame at half the
        // cosine's heard height.
        float peak = 0.0f;
        for (qint64 k = roughHeardFrame; k < roughHeardFrame + 960; ++k) {
            peak = std::max(peak, h.remoteBus->heard.at(k * 2));
        }
        QVERIFY(peak > 0.1f);
        qint64 heardFrame = roughHeardFrame;
        while (h.remoteBus->heard.at(heardFrame * 2) < peak / 2.0f) { ++heardFrame; }
        const double heardMs =
            double(playOriginNs + dueNs(heardFrame) - dueNs(onsetFrame)) / 1e6;
        qInfo().noquote() << QStringLiteral(
            "%1 session: heard delay %2 ms, measured %3 ms +- %4 ms (delivery %5 ms), "
            "device counted %6, speaker ran dry for %7 frames")
            .arg(lossless ? QStringLiteral("lossless") : QStringLiteral("opus"))
            .arg(heardMs, 0, 'f', 2).arg(estimate.delayMs, 0, 'f', 2)
            .arg(estimate.boundMs, 0, 'f', 2)
            .arg(estimate.deliveryMs.value_or(-1), 0, 'f', 1)
            .arg(estimate.includesDevice ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(h.remoteBus->playedDryFramesForTesting());
        QVERIFY2(std::abs(estimate.delayMs - heardMs) <= estimate.boundMs + kToleranceMs,
                 qPrintable(QStringLiteral("measured %1 +- %2, heard %3")
                                .arg(estimate.delayMs).arg(estimate.boundMs).arg(heardMs)));
        // An 8 ms miss cannot pass.
        QVERIFY2(estimate.boundMs + kToleranceMs < 8.0, qPrintable(QString::number(estimate.boundMs)));
        // Not the Core's clock offset, and not half a loopback round trip:
        // the jitter hold alone is 80 ms.
        QVERIFY(estimate.delayMs > 60.0 && estimate.delayMs < 1000.0);
        QVERIFY(!estimate.includesDevice); // the paced test speaker reports none
        QVERIFY(estimate.deliveryMs && *estimate.deliveryMs > 0.0
                && *estimate.deliveryMs < estimate.delayMs);
        QVERIFY(remoteAudioDelayText(estimate).endsWith(
            QStringLiteral(", not counting the speaker device")));

        // The probes on the wire: exactly the documented keys.
        QVERIFY(clockProbesIn(coreControls) >= 2);
        for (const auto& call : coreControls) {
            const QJsonObject control = call.at(0).toJsonObject();
            if (control.value(QStringLiteral("op")) != QLatin1String("clock-probe")) { continue; }
            QStringList keys = control.keys();
            keys.sort();
            QCOMPARE(keys, (QStringList{QStringLiteral("connectionId"), QStringLiteral("id"),
                                        QStringLiteral("op"), QStringLiteral("t0")}));
        }

        // A new audio context: nothing until the Core reports its capture.
        h.remote.audioEngine()->setMasterMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(!remoteMedia.audioDelay().estimate.has_value(), 2000);
        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioDelay().estimate.has_value(), 10000);
        QCOMPARE(remoteErrors.count(), 0);

        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        QTRY_VERIFY_WITH_TIMEOUT(!remoteMedia.audioDelay().measurable, 5000);
    }
};

QTEST_GUILESS_MAIN(TstRemoteAudioSession)
#include "tst_remote_audio_session.moc"
