#pragma once
// =================================================================
// tests/fakes/RemoteAudioSessionHarness.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test fixture.
//
// A real Core and a real remote GUI joined over LoopbackTransport for the
// authenticated control plane. Both media peers use the default
// LibDataChannel transport, so RTP and Opus travel over real DTLS/SRTP.
// The station mixes two panned slices; the GUI plays into a PacedAudioBus
// the test drives.
//
// Moved unchanged out of tst_remote_audio_session.cpp (R-R3-23 Task 3) so
// tst_remote_media_controller can drive the same real session. Both files
// also share the readable printer for the GUI's remote audio state.
//
// R-R3-23 lossless: hideAudioProfile makes the Core look like one from
// before the lossless profile (its capabilities carry no
// audioProfileVersion), so tests can prove such a Core sees exactly the
// behaviour it did. R-R3-35: hideAudioClock likewise makes the Core look
// like one from before measured audio delay (no audioClockVersion).
// R-R3-43: hideReceiverAudio likewise makes the Core look like one from
// before receiver audio streams (no receiverAudioVersion). The station's
// two slices carry their own tones, 617 Hz on slice A and 1579 Hz on slice
// B, so a receiver stream shows which slice it carries.
// R-R3-45: hideHeadphonesMix likewise makes the Core look like one from
// before the headphones mix (no headphonesMixVersion), and
// attachRemoteHeadphones() gives the remote window a paced headphones
// device beside its speakers.
// Remote-window parity Task 32: hideTxMonitorAudio likewise makes the Core
// look like one from before the transmit monitor (no
// txMonitorAudioVersion).
// iPhone app plan Task 36 (R-IOS-13): declareRemoteTx makes the window's
// hello declare remoteTx 1, so the Core tells it remoteTxVersion and the media
// connection gets the microphone line; grantTransmit makes the Core's
// capabilities say txPermitted, as for a paired device, so VOX armed can be
// shown over the token sign-in. Desktop remote transmit: the window's own
// hello now declares remoteTx, so declareRemoteTx defaults to true and false
// takes the declaration out (a window from before remote transmit).
// pairWindow signs the window in with its own paired device key instead of
// the token, so the Core's gate permits it to transmit for real.
//
// =================================================================

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationCapabilities.h"
#include "core/session/RemoteKeying.h"
#include "core/session/StationServer.h"
#include "core/session/media/IReceiverPcmSink.h"
#include "core/settings/SettingsProxy.h"
#include "core/session/media/RemoteAudioContext.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteMediaController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "LoopbackTransport.h"
#include "PacedAudioBus.h"

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QVector>

#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include "UpgradedCoreToken.h"

namespace NereusSDR {

// Readable QCOMPARE failures for the remote audio state (found by ADL).
inline char* toString(RemoteAudioStatus::State state)
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

namespace NereusSDR::Test {

// Test-only control link, so compatibility can be exercised without any
// production test hook. With a hello minor set on both ends, a current Core
// and a current GUI each believe the other speaks that minor. A one-shot
// forge puts a forged copy of the next audio context on the wire just
// ahead of the real one.
class RewritingTransport final : public LoopbackTransport {
public:
    using Forge = std::function<QJsonObject(const QJsonObject& real)>;

    RewritingTransport(const QString& description, std::optional<quint16> helloMinor)
        : LoopbackTransport(description), m_helloMinor(helloMinor) {}

    void sendText(const QByteArray& wire) override
    {
        const bool mayRewrite = ((m_helloMinor || !declareRemoteTx) && wire.contains("\"hello\""))
            || (grantTransmit && wire.contains("\"capabilities\""))
            || (forgeNextAudioContext && wire.contains("\"audio-context\""))
            || ((hideAudioProfile || hideAudioClock || hideReceiverAudio || hideHeadphonesMix
                 || hideTxMonitorAudio)
                && wire.contains("\"capabilities\""));
        SessionMessage message;
        if (mayRewrite && SessionMessages::decode(wire, &message)) {
            if ((hideAudioProfile || hideAudioClock || hideReceiverAudio || hideHeadphonesMix
                 || hideTxMonitorAudio || grantTransmit)
                && message.kind == SessionMessageKind::Capabilities) {
                StationCapabilities capabilities = StationCapabilities::fromUpdates(message.updates);
                if (grantTransmit) { capabilities.txPermitted = true; }
                if (hideAudioProfile) { capabilities.audioProfileVersion = 0; }
                if (hideAudioClock) { capabilities.audioClockVersion = 0; }
                if (hideReceiverAudio) { capabilities.receiverAudioVersion = 0; }
                if (hideHeadphonesMix) { capabilities.headphonesMixVersion = 0; }
                if (hideTxMonitorAudio) { capabilities.txMonitorAudioVersion = 0; }
                ++hiddenAudioProfiles;
                LoopbackTransport::sendText(SessionMessages::encode(
                    SessionMessages::capabilities(capabilities.toUpdates())));
                return;
            }
            if (!declareRemoteTx && message.kind == SessionMessageKind::Hello) {
                // A window from before remote transmit: its hello without
                // remoteTx; everything else it said is kept.
                QHash<QByteArray, int> features = message.features;
                features.remove(QByteArrayLiteral("remoteTx"));
                QList<quint16> majors = message.supportedMajors;
                if (majors.isEmpty()) { majors.append(message.protocolMajor); }
                LoopbackTransport::sendText(SessionMessages::encode(SessionMessages::hello(
                    message.protocolMajor, m_helloMinor.value_or(message.protocolMinor),
                    message.settingsSchemaVersion, message.peerName, majors, features)));
                return;
            }
            if (m_helloMinor && message.kind == SessionMessageKind::Hello) {
                ++rewrittenHellos;
                LoopbackTransport::sendText(SessionMessages::encode(SessionMessages::hello(
                    message.protocolMajor, *m_helloMinor, message.settingsSchemaVersion,
                    message.peerName)));
                return;
            }
            if (forgeNextAudioContext && message.kind == SessionMessageKind::MediaControl
                && message.mediaPayload.value(QStringLiteral("op"))
                    == QLatin1String("audio-context")) {
                SessionMessage forged = message;
                forged.mediaPayload = std::exchange(forgeNextAudioContext, {})(
                    message.mediaPayload);
                ++forgedContexts;
                LoopbackTransport::sendText(SessionMessages::encode(forged));
            }
        }
        LoopbackTransport::sendText(wire);
    }

    int rewrittenHellos = 0;
    int forgedContexts = 0;
    int hiddenAudioProfiles = 0;
    Forge forgeNextAudioContext;
    bool hideAudioProfile = false;
    bool hideAudioClock = false;
    bool hideReceiverAudio = false;
    bool hideHeadphonesMix = false;
    bool hideTxMonitorAudio = false;
    bool declareRemoteTx = true;
    bool grantTransmit = false;

private:
    std::optional<quint16> m_helloMinor;
};

// R-R3-43: an app on the remote computer that listens to receiver
// streams: every block and every stop, per slice, safe across the receive
// worker threads that deliver them.
class CollectingReceiverSink final : public IReceiverPcmSink {
public:
    void receiverAudioBlock(int sliceId, const float* pcm, int frames) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        QVector<float>& audio = m_audio[sliceId];
        audio.append(QVector<float>(pcm, pcm + qsizetype(frames) * 2));
        ++m_blocks[sliceId];
    }
    void receiverAudioStopped(int sliceId, const QString& reason) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stops.append({sliceId, reason});
    }
    QVector<float> audio(int sliceId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_audio.value(sliceId);
    }
    int frames(int sliceId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return int(m_audio.value(sliceId).size() / 2);
    }
    int blocks(int sliceId) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_blocks.value(sliceId);
    }
    QList<std::pair<int, QString>> stops() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stops;
    }

private:
    mutable std::mutex m_mutex;
    QHash<int, QVector<float>> m_audio;
    QHash<int, int> m_blocks;
    QList<std::pair<int, QString>> m_stops;
};

// The amplitude of one tone in one channel of interleaved stereo, from
// firstFrame on.
inline double toneAmplitude(const QVector<float>& samples, int channel, double hz,
                            int firstFrame = 0)
{
    constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
    double cosine = 0.0;
    double sine = 0.0;
    int frames = 0;
    for (int frame = firstFrame; frame * 2 + channel < samples.size(); ++frame) {
        const double phase = kTwoPi * hz * static_cast<double>(frame) / 48000.0;
        const double sample = samples.at(frame * 2 + channel);
        cosine += sample * std::cos(phase);
        sine += sample * std::sin(phase);
        ++frames;
    }
    return frames > 0 ? 2.0 * std::hypot(cosine, sine) / frames : 0.0;
}

struct RemoteAudioSessionHarness {
    static constexpr int kFrames = 480;
    static constexpr double kPi = 3.14159265358979323846;

    QTemporaryDir directory;
    AppSettings settings;
    RadioModel station;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    AudioEngine* stationAudio{nullptr};
    PacedAudioBus* remoteBus{nullptr};
    int sliceA{-1};
    int sliceB{-1};
    qint64 stationFrames{0};

    RemoteAudioSessionHarness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
    {
        Q_ASSERT(directory.isValid());
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                    /*defaultRateHz=*/192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        stationAudio = station.audioEngine();
        Q_ASSERT(stationAudio != nullptr);
        stationAudio->masterMixForTest().setRampFrames(1);
        stationAudio->masterMixForTest().setSlewUpFrames(0);
        sliceA = station.addSlice();
        sliceB = station.addSlice();
        Q_ASSERT(sliceA >= 0 && sliceB >= 0);
        // AF is the mixer level now; these tests measure unity gain.
        station.sliceById(sliceA)->setAfGain(100);
        station.sliceById(sliceB)->setAfGain(100);
        stationAudio->setSliceStreaming(sliceA, true);
        stationAudio->setSliceStreaming(sliceB, true);
        stationAudio->masterMixForTest().setSliceGain(sliceA, 0.60f, -0.95f);
        stationAudio->masterMixForTest().setSliceGain(sliceB, 0.45f, 0.95f);
        station.sliceById(sliceA)->setAudioPan(-0.95);
        station.sliceById(sliceB)->setAudioPan(0.95);
        server.setMediaEnabled(true);

        remote.setConnectionStateForTest(ConnectionState::Connected);
        auto bus = std::make_unique<PacedAudioBus>();
        remoteBus = bus.get();
        remote.audioEngine()->setSpeakersBusForTest(std::move(bus));
    }

    // helloMinor, when set, is what both ends announce, so both negotiate
    // down to it. forgeFirstContext, when set, puts one forged copy on the
    // wire ahead of Core's first audio context.
    void connectSession(std::optional<quint16> helloMinor = std::nullopt,
                        RewritingTransport::Forge forgeFirstContext = {})
    {
        auto* station = new RewritingTransport(QStringLiteral("station"), helloMinor);
        auto* clientEnd = new RewritingTransport(QStringLiteral("client"), helloMinor);
        station->forgeNextAudioContext = std::move(forgeFirstContext);
        station->hideAudioProfile = hideAudioProfile;
        station->hideAudioClock = hideAudioClock;
        station->hideReceiverAudio = hideReceiverAudio;
        station->hideHeadphonesMix = hideHeadphonesMix;
        station->hideTxMonitorAudio = hideTxMonitorAudio;
        station->grantTransmit = grantTransmit;
        clientEnd->declareRemoteTx = declareRemoteTx;
        stationLink = station;
        station->linkTo(clientEnd);
        if (pairWindow) {
            // Desktop remote transmit: the window signs in by its own key,
            // which the Core has paired, so the gate may permit it.
            if (!windowKey) {
                windowKey = std::make_shared<const ClientDeviceIdentity>(
                    ClientDeviceIdentity::loadOrCreate(windowKeyDir.path()));
                client.setDeviceIdentity(windowKey, QStringLiteral("Shack MacBook"),
                                         QStringLiteral("MacBook"));
                PairedDevice device;
                device.id = windowKey->fingerprint();
                device.publicKeySpki = windowKey->publicKeySpki();
                device.name = QStringLiteral("Shack MacBook");
                device.kind = QStringLiteral("computer");
                QVERIFY(server.deviceStore()->add(device));
            }
            QString pin = server.certificateFingerprint();
            pin.remove(QLatin1Char(':'));
            clientEnd->setPeerCertificateSha256(QByteArray::fromHex(pin.toLatin1()));
            client.startSession(clientEnd, QString(), QString(),
                                server.stationIdentity().fingerprint());
        } else {
            client.startSession(clientEnd, server.token());
        }
        server.acceptTransport(station);
        QTRY_VERIFY(server.mediaAvailable());
        // R-R3-49: the Core's side is ready once it has sent
        // snapshot.complete; this window's is once it has read it, and with
        // it the capabilities every *Negotiated() reads. The two are one
        // queued delivery apart, and a wait that stops between them (its
        // slice of the event loop ran out on a busy computer) left
        // receiverAudioNegotiated() false.
        QTRY_VERIFY(client.isHandshakeComplete());
        if (helloMinor) {
            QCOMPARE(station->rewrittenHellos, 1);
            QCOMPARE(clientEnd->rewrittenHellos, 1);
            QCOMPARE(client.agreedMinor(), *helloMinor);
        }
        if (hideAudioProfile || hideAudioClock || hideReceiverAudio || hideHeadphonesMix
            || hideTxMonitorAudio) {
            // On a reconnect the server can still report media from the
            // session being replaced; wait for this link's capabilities.
            QTRY_VERIFY(station->hiddenAudioProfiles >= 1 && client.isHandshakeComplete());
            if (hideAudioProfile) { QCOMPARE(client.capabilities().audioProfileVersion, 0); }
            if (hideAudioClock) { QCOMPARE(client.capabilities().audioClockVersion, 0); }
            if (hideReceiverAudio) { QCOMPARE(client.capabilities().receiverAudioVersion, 0); }
            if (hideHeadphonesMix) { QCOMPARE(client.capabilities().headphonesMixVersion, 0); }
            if (hideTxMonitorAudio) { QCOMPARE(client.capabilities().txMonitorAudioVersion, 0); }
        }
    }

    // A wait on this window's media connection allows what the window
    // itself allows before it gives up and recovers: the Core's media
    // description, then the connection (RemoteMediaController's
    // kMediaDescriptionDeadlineMs + kMediaConnectDeadlineMs; this harness
    // connects directly, never through the remote access service, so no
    // gathering bound is added). A shorter wait failed on a loaded
    // computer while the product was still inside its own limits.
    static constexpr int kMediaConnectionWaitMs =
        RemoteMediaController::kMediaDescriptionDeadlineMs
        + RemoteMediaController::kMediaConnectDeadlineMs;

    // How far the media connection got, for the message of a wait on it
    // that ran out: control handshake, the Core's media, the window's media
    // peer (started once the Core's description arrived), bytes over it,
    // the audio status and the last accepted audio context.
    QByteArray mediaStage(const RemoteMediaController& media) const
    {
        const auto yesNo = [](bool value) {
            return value ? QStringLiteral("yes") : QStringLiteral("no");
        };
        const std::optional<MediaPeerTelemetry> traffic = media.trafficTelemetry();
        const std::unique_ptr<char[]> state(toString(media.audioStatus().state));
        QString context = QStringLiteral("none");
        if (const std::optional<RemoteAudioContextMessage> accepted
            = media.acceptedAudioContext()) {
            context = QStringLiteral("generation %1, %2")
                          .arg(accepted->generation)
                          .arg(accepted->enabled
                                   ? QStringLiteral("enabled")
                                   : QStringLiteral("off (%1)").arg(
                                         accepted->offReason
                                             ? remoteAudioOffReasonToWire(*accepted->offReason)
                                             : QStringLiteral("no reason")));
        }
        return QStringLiteral(
                   "media connection stage: handshake complete %1; Core media %2; "
                   "window media %3; media peer started %4; RTP bytes received %5, "
                   "sent %6; audio status %7; accepted audio context %8")
            .arg(yesNo(client.isHandshakeComplete()), yesNo(server.mediaAvailable()),
                 yesNo(client.mediaAvailable()), yesNo(traffic.has_value()))
            .arg(traffic ? traffic->traffic.receivedRtpBytes : 0)
            .arg(traffic ? traffic->traffic.submittedRtpBytes : 0)
            .arg(QString::fromLatin1(state.get()), context)
            .toUtf8();
    }

    // Desktop remote transmit: set before connectSession(): the window
    // signs in with its own paired key (the Core permits it to transmit
    // when makeTransmitReady() allowed remote transmit).
    bool pairWindow = false;
    QTemporaryDir windowKeyDir;
    std::shared_ptr<const ClientDeviceIdentity> windowKey;

    // Desktop remote transmit: the Core allows remote transmit and its
    // radio keys at once (no MOX delays, the radio's own microphone as its
    // configured source, slice A on 20 m USB).
    void makeTransmitReady()
    {
        server.setRemoteTransmitAllowed(true);
        station.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        station.transmitModel().setMicSourceLocked(false);
        station.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = station.sliceById(sliceA)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
    }

    // Fix wave C1: a voice key needs the window's microphone line, which a
    // test without media has not got. The line reads open and its buffer
    // full at once. A DaemonMediaController made after this installs the
    // real line instead.
    void openFakeMicrophoneLine()
    {
        RemoteKeying* keying = server.remoteKeying();
        if (keying == nullptr) {
            return;
        }
        RemoteKeying::MicUplink uplink;
        uplink.carriesMic = [](const QByteArray&) { return true; };
        uplink.prime = [](const QByteArray&, std::function<void(bool)> done) { done(true); };
        uplink.endPriming = [](const QByteArray&) {};
        keying->setMicUplink(uplink);
    }

    // Set before connectSession(): the Core appears to predate lossless.
    bool hideAudioProfile = false;
    // Set before connectSession(): the Core appears to predate measured
    // audio delay (R-R3-35).
    bool hideAudioClock = false;
    // Set before connectSession(): the Core appears to predate receiver
    // audio streams (R-R3-43).
    bool hideReceiverAudio = false;
    // Set before connectSession(): the Core appears to predate the
    // headphones mix (R-R3-45).
    bool hideHeadphonesMix = false;
    // Set before connectSession(): the Core appears to predate the transmit
    // monitor (remote-window parity Task 32).
    bool hideTxMonitorAudio = false;
    // Task 36: set before connectSession(): the window's hello declares
    // remoteTx 1 (as the desktop's own does; false takes it out); the
    // Core's capabilities say txPermitted.
    bool declareRemoteTx = true;
    bool grantTransmit = false;

    // R-R3-45: the remote window's headphones, a paced fake device the test
    // renders like the speakers. Open, so the window can play the Core's
    // headphones mix on it.
    PacedAudioBus* remoteHeadphonesBus{nullptr};
    void attachRemoteHeadphones()
    {
        auto bus = std::make_unique<PacedAudioBus>();
        remoteHeadphonesBus = bus.get();
        remote.audioEngine()->setHeadphonesBusForTest(std::move(bus));
    }

    // R-R3-45: both station slices back on the speakers, here and in the
    // settings the station saved, so later tests start from the default.
    void resetOutputRoutes()
    {
        for (int sliceId : {sliceA, sliceB}) {
            if (SliceModel* slice = station.sliceById(sliceId)) {
                slice->setOutputRoute(SliceModel::OutputRoute::Speakers);
            }
            AppSettings::instance().remove(QStringLiteral("Slice%1/OutputRoute").arg(sliceId));
        }
    }

    // The station's two slice tones.
    static constexpr double kSliceAToneHz = 617.0;
    static constexpr double kSliceBToneHz = 1579.0;

    QPointer<RewritingTransport> stationLink;

    void feedMixedTone()
    {
        QVector<float> a(kFrames * 2);
        QVector<float> b(kFrames * 2);
        for (int frame = 0; frame < kFrames; ++frame) {
            const double time = static_cast<double>(stationFrames + frame) / 48000.0;
            const float first = static_cast<float>(0.22 * std::sin(2.0 * kPi * kSliceAToneHz * time));
            const float second = static_cast<float>(0.19 * std::sin(2.0 * kPi * kSliceBToneHz * time));
            a[frame * 2] = a[frame * 2 + 1] = first;
            b[frame * 2] = b[frame * 2 + 1] = second;
        }
        stationFrames += kFrames;
        stationAudio->rxBlockReady(sliceA, a.constData(), kFrames);
        stationAudio->rxBlockReady(sliceB, b.constData(), kFrames);
    }
};

} // namespace NereusSDR::Test
