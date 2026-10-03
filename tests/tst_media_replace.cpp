// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_media_replace.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 (R-IOS-16; the pairing design, section 5.4; the
// remote media control document, "Replacing the media connection";
// mediaReplaceVersion 1): media moves to a new peer connection with no
// gap and no repeat.
//
//   - The Core (DaemonMediaController, transports fake only at the
//     network boundary): a replacement is refused unless it replaces the
//     current, ready peer and the radio is idle; once the new peer is
//     ready every audio packet goes out on both with the same sequence
//     number and timestamp and each peer's own SSRC; kReplaceOverlapMs
//     later the new peer takes over, the Core says `replace`, the old peer
//     sends nothing more and closes after kReplaceDrainMs; a new peer that
//     closes before that leaves the current one untouched.
//   - Dual receive (RtpDuplicateFilter with the window's AudioJitterBuffer,
//     on a simulated clock): a switch from a slow, lossy relay path to a
//     fast direct one plays every interval once, none concealed and none
//     twice.
//   - End to end over the real DTLS/SRTP session (the Core's and the
//     window's media controllers, real Opus): the window replaces its
//     media while listening; the audio heard has no gap over 40 ms and no
//     repeat, the session signs in and snapshots nothing new, and no new
//     audio context restarts playback.
//
// No real audio device is opened (the harness's paced bus stands in for
// the speakers).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: a refused replace keeps the move it carried: a move folded
//               into the retried fallback, or one made while the
//               fallback's replace is under way, is still followed after
//               the Core unkeys. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: load finding: a session move folded into a refused
//               fallback's wait is made only once the window has heard the
//               Core on the air, and the waits for the refusal use the
//               window's own replace deadline. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: media back on the direct pair ends a refused fallback
//               waiting to be retried (a folded move is still followed, as
//               a normal replace). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: a session move while a refused fallback waits keeps the
//               retry on the tunnel alone. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: direct media follow-up: a fallback refused while the Core
//               transmits is retried onto the tunnel alone. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: direct media fix wave: the silence fallback on the tunnel
//               alone, once per silence, then recovery; none while muted
//               or disconnected; the return to receive restarts the
//               silence clock; a direct replace after a move onto the
//               tunnel; ordering barriers in place of fixed waits.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: the direct-only replace (STUN and
//               host candidates, no tunnel or relay), the older relay-leg
//               refusal judged by the connection in use, the window's
//               direct schedule, a refused direct step, the no-packets
//               fallback and an older Core's three-field replace. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtEndian>

#include <cmath>
#include <functional>
#include <map>
#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/media/AudioJitterBuffer.h"
#include "core/session/media/DaemonAudioSource.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/media/MediaPeer.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/DualPathAudio.h"
#include "core/session/media/RtpDuplicateFilter.h"
#include "core/session/PathRacer.h"
#include "OperatorWording.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/RemoteMediaController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "RealtimeTestLoad.h"
#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

constexpr char kFirst[] = "11111111-2222-4333-8444-555555555555";
constexpr char kSecond[] = "66666666-7777-4888-9999-aaaaaaaaaaaa";
constexpr char kThird[] = "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff";
constexpr int kDspFrames = 64;
// The window's own bound for the Core's answer to a replace: startReplacement
// arms replaceDeadline with the description deadline plus the ICE connect
// deadline (src/gui/RemoteMediaController.cpp startReplacement), after which
// it gives the replace up.
constexpr int kReplaceAnswerMs =
    RemoteMediaController::kMediaDescriptionDeadlineMs + IceConfiguration::kConnectDeadlineMs;

class FakeTransport final : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}

    bool start(const StartOptions& options) override
    {
        startOptions = options;
        started = true;
        return true;
    }
    void stop() override
    {
        if (!stopped) {
            stopped = true;
            readyState = false;
        }
    }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return readyState; }
    bool sendRtp(const QByteArray& packet) override
    {
        if (!readyState) {
            return false;
        }
        rtpPackets.append(packet);
        return true;
    }
    bool isReady() const override { return readyState; }
    std::optional<MediaIcePath> selectedPath() const override { return path; }
    void becomeReady()
    {
        readyState = true;
        emit ready();
    }
    void close() { emit closed(); }

    bool started{false};
    bool stopped{false};
    bool readyState{false};
    StartOptions startOptions{Role::Answerer, 0};
    std::optional<MediaIcePath> path;
    QList<QByteArray> rtpPackets;
};

quint32 ssrcOf(const QByteArray& packet)
{
    return qFromBigEndian<quint32>(packet.constData() + 8);
}
quint32 timestampOf(const QByteArray& packet)
{
    return qFromBigEndian<quint32>(packet.constData() + 4);
}
quint16 sequenceOf(const QByteArray& packet)
{
    return qFromBigEndian<quint16>(packet.constData() + 2);
}

QVector<float> stereoBlock(float left, float right)
{
    QVector<float> block(kDspFrames * 2);
    for (int frame = 0; frame < kDspFrames; ++frame) {
        block[frame * 2] = left;
        block[frame * 2 + 1] = right;
    }
    return block;
}

QJsonObject control(const QString& op, const QString& connectionId)
{
    return {{QStringLiteral("op"), op}, {QStringLiteral("connectionId"), connectionId}};
}

QJsonObject replaceControl(const QString& connectionId, const QString& replaces)
{
    return {{QStringLiteral("op"), QStringLiteral("replace")},
            {QStringLiteral("connectionId"), connectionId},
            {QStringLiteral("replaces"), replaces}};
}

// The direct media ladder: a replace asking for the direct-only connection.
QJsonObject directReplaceControl(const QString& connectionId, const QString& replaces,
                                 int version = 1)
{
    QJsonObject replace = replaceControl(connectionId, replaces);
    replace.insert(QStringLiteral("mediaDirectVersion"), version);
    return replace;
}

MediaIcePath tunnelShimPath()
{
    MediaIcePath path;
    path.remoteAddress = QStringLiteral("127.0.0.1");
    path.ownedLoopbackShim = true;
    return path;
}

QList<QJsonObject> controlsNamed(const QSignalSpy& spy, const QString& op)
{
    QList<QJsonObject> out;
    for (const QList<QVariant>& call : spy) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == op) {
            out.append(message);
        }
    }
    return out;
}

// The Core's media controller with its transports fake at the network, as
// tst_daemon_audio_session stands it up; each peer it makes is kept.
struct CoreHarness {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    QList<QPointer<FakeTransport>> transports;
    DaemonMediaController controller;
    AudioEngine* engine{nullptr};
    int slice{-1};
    // The Core's end of the session link, and whether it carries binary
    // messages (false: a data channel, where the media tunnel cannot run).
    Test::LoopbackTransport* stationLink = nullptr;
    bool stationCarriesBinary = true;

    CoreHarness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
        , controller(&server, &radio, nullptr,
                     [this](QObject* parent) -> IMediaTransport* {
                         auto* transport = new FakeTransport(parent);
                         transports.append(transport);
                         return transport;
                     })
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        engine = radio.audioEngine();
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        slice = radio.addSlice();
        engine->setSliceStreaming(slice, true);
        server.setMediaEnabled(true);
    }

    bool establishWithAudio(const QJsonObject& startExtra = {})
    {
        stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        stationLink->setCarriesBinary(stationCarriesBinary);
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        if (!QTest::qWaitFor([this] { return server.mediaAvailable() && client.mediaAvailable(); },
                             10000)) {
            qWarning() << "media never available";
            return false;
        }
        QJsonObject start = control(QStringLiteral("start"), QLatin1String(kFirst));
        for (auto it = startExtra.begin(); it != startExtra.end(); ++it) {
            start.insert(it.key(), it.value());
        }
        const bool sent = client.sendMediaControl(start, client.sessionEpoch());
        if (!QTest::qWaitFor([this] { return !transports.isEmpty(); }, 5000)) {
            qWarning() << "no transport; start sent" << sent;
            return false;
        }
        transports.first()->becomeReady();
        client.sendMediaControl({{QStringLiteral("op"), QStringLiteral("audio")},
                                 {QStringLiteral("connectionId"), QLatin1String(kFirst)},
                                 {QStringLiteral("revision"), 1},
                                 {QStringLiteral("enabled"), true}},
                                client.sessionEpoch());
        return QTest::qWaitFor([this] { return controller.audioDiagnostics().activeContext; },
                               5000);
    }

    void feed(int blocks)
    {
        const QVector<float> tone = stereoBlock(0.25f, 0.25f);
        for (int b = 0; b < blocks; ++b) {
            for (int delivered = 0; delivered < DaemonAudioSource::kBlockFrames;
                 delivered += kDspFrames) {
                engine->rxBlockReady(slice, tone.constData(), kDspFrames);
            }
        }
    }
};

// The direct media ladder: the window's media controller against the Core
// above, both with transports fake at the network and the window's clock
// injected, so the schedule and the silence window run without waiting.
struct GuiHarness {
    CoreHarness core;
    QList<QPointer<FakeTransport>> guiTransports;
    qint64 now = 0;
    Test::LoopbackTransport* stationLink = nullptr;
    QList<QByteArray> toCore;
    std::unique_ptr<RemoteMediaController> gui;
    // Audio keeps arriving on the first connection (every 100 ms) while
    // `feeding`, so the tunnel's own stall rule never starts media over
    // underneath a test; a test on a direct path turns it off to go silent.
    QTimer feeder;
    bool feeding = true;
    QTemporaryDir windowKeyDir;
    std::shared_ptr<const ClientDeviceIdentity> windowKey;

    GuiHarness()
    {
        feeder.setInterval(100);
        QObject::connect(&feeder, &QTimer::timeout, &feeder, [this] {
            if (feeding && !guiTransports.isEmpty() && guiTransports.first()) { audioOn(0); }
        });
        core.server.setRemoteTransmitAllowed(true);
        gui = std::make_unique<RemoteMediaController>(
            &core.client, &core.remote, nullptr, nullptr,
            [this](QObject* parent) -> IMediaTransport* {
                auto* transport = new FakeTransport(parent);
                guiTransports.append(transport);
                return transport;
            },
            [this] { return now; });
    }

    // Signs in; media starts as the window starts it (with the tunnel), and
    // one audio packet arrives on `path`.
    bool connect(const MediaIcePath& path)
    {
        stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        QObject::connect(stationLink, &SessionTransport::textReceived, stationLink,
                         [this](const QByteArray& wire) { toCore.append(wire); });
        // The window signs in by its own paired key, as a desktop remote
        // that may transmit does, so VOX armed counts.
        windowKey = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(windowKeyDir.path()));
        core.client.setDeviceIdentity(windowKey, QStringLiteral("Shack MacBook"),
                                      QStringLiteral("MacBook"));
        PairedDevice device;
        device.id = windowKey->fingerprint();
        device.publicKeySpki = windowKey->publicKeySpki();
        device.name = QStringLiteral("Shack MacBook");
        device.kind = QStringLiteral("computer");
        if (!core.server.deviceStore()->add(device)) {
            qWarning() << "the window could not be paired";
            return false;
        }
        QString pin = core.server.certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        clientLink->setPeerCertificateSha256(QByteArray::fromHex(pin.toLatin1()));
        core.client.startSession(clientLink, QString(), QString(),
                                 core.server.stationIdentity().fingerprint());
        core.server.acceptTransport(stationLink);
        if (!QTest::qWaitFor([this] {
                return !guiTransports.isEmpty() && !core.transports.isEmpty();
            }, 10000)) {
            qWarning() << "media never started";
            return false;
        }
        guiTransports.first()->path = path;
        core.transports.first()->path = path;
        guiTransports.first()->becomeReady();
        core.transports.first()->becomeReady();
        audioOn(0);
        feeder.start();
        return true;
    }

    // One audio packet on connection `index`, under its own main SSRC.
    void audioOn(int index)
    {
        FakeTransport* transport = guiTransports.at(index);
        QByteArray packet(12 + 3, '\0');
        packet[0] = char(0x80);
        packet[1] = char(111);
        qToBigEndian<quint32>(transport->startOptions.localAudioSsrc, packet.data() + 8);
        emit transport->rtpReceived(packet);
    }

    // The replaces this window sent, as the Core read them.
    QList<QJsonObject> replacesSent() const
    {
        QList<QJsonObject> out;
        for (const QByteArray& wire : toCore) {
            if (!wire.contains("\"replace\"")) { continue; }
            const QJsonObject envelope = QJsonDocument::fromJson(wire).object();
            // The media control rides inside the control envelope.
            std::function<void(const QJsonValue&)> find = [&](const QJsonValue& value) {
                if (value.isObject()) {
                    const QJsonObject object = value.toObject();
                    if (object.value(QStringLiteral("op")) == QStringLiteral("replace")) {
                        out.append(object);
                        return;
                    }
                    for (auto it = object.begin(); it != object.end(); ++it) { find(*it); }
                } else if (value.isArray()) {
                    for (const QJsonValue& item : value.toArray()) { find(item); }
                }
            };
            find(envelope);
        }
        return out;
    }
};

} // namespace

class TstMediaReplace final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        OpusAudioEncoder encoder;
        if (!encoder.isReady()) {
            QSKIP("Opus encoder is unavailable in this build");
        }
    }
    // The load when a real-time case failed (R-R3-21, R-R3-40).
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }

    // ── The Core ──────────────────────────────────────────────────────

    // Every audio packet goes out on both peers once the new one is ready,
    // with the same sequence number and timestamp and each peer's own
    // SSRC; the new peer then takes over and the Core says so.
    void theCoreSendsOnBothThenTheNewPeerTakesOver()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio());
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        FakeTransport* first = h.transports.first();
        h.feed(2);
        QTRY_VERIFY(first->rtpPackets.size() >= 2);
        const quint32 firstSsrc = first->startOptions.localAudioSsrc;
        QCOMPARE(ssrcOf(first->rtpPackets.constLast()), firstSsrc);

        QVERIFY(h.client.sendMediaControl(replaceControl(QLatin1String(kSecond),
                                                         QLatin1String(kFirst)),
                                          h.client.sessionEpoch()));
        QTRY_COMPARE(h.transports.size(), 2);
        FakeTransport* second = h.transports.at(1);
        QVERIFY(second->started);
        QCOMPARE(second->startOptions.role, IMediaTransport::Role::Offerer);
        const quint32 secondSsrc = second->startOptions.localAudioSsrc;
        QVERIFY(secondSsrc != 0 && secondSsrc != firstSsrc);
        // Not yet ready: audio goes on the current peer alone.
        const int before = first->rtpPackets.size();
        h.feed(1);
        QTRY_COMPARE(first->rtpPackets.size(), before + 1);
        QVERIFY(second->rtpPackets.isEmpty());

        second->becomeReady();
        h.feed(3);
        QTRY_VERIFY(second->rtpPackets.size() >= 3);
        // The same packets on both: sequence and timestamp alike, SSRC each
        // peer's own.
        const QByteArray onFirst = first->rtpPackets.constLast();
        const QByteArray onSecond = second->rtpPackets.constLast();
        QCOMPARE(sequenceOf(onSecond), sequenceOf(onFirst));
        QCOMPARE(timestampOf(onSecond), timestampOf(onFirst));
        QCOMPARE(ssrcOf(onFirst), firstSsrc);
        QCOMPARE(ssrcOf(onSecond), secondSsrc);
        QCOMPARE(onSecond.mid(12), onFirst.mid(12));

        // kReplaceOverlapMs later: the new peer takes over, `replace` says so.
        QTRY_COMPARE_WITH_TIMEOUT(controlsNamed(controls, QStringLiteral("replace")).size(), 1,
                                  DaemonMediaController::kReplaceOverlapMs + 3000);
        const QJsonObject done = controlsNamed(controls, QStringLiteral("replace")).first();
        QCOMPARE(done.value(QStringLiteral("connectionId")).toString(), QLatin1String(kSecond));
        QCOMPARE(done.value(QStringLiteral("replaces")).toString(), QLatin1String(kFirst));
        QCOMPARE(done.size(), 3);
        // Nothing more on the old peer; the new one carries on, under its
        // own SSRC, the timeline unbroken.
        const int firstCount = first->rtpPackets.size();
        const QByteArray lastOnSecond = second->rtpPackets.constLast();
        const int secondBefore = second->rtpPackets.size();
        h.feed(3);
        QTRY_COMPARE(second->rtpPackets.size(), secondBefore + 3);
        QTest::qWait(50);
        QCOMPARE(first->rtpPackets.size(), firstCount);
        QCOMPARE(ssrcOf(second->rtpPackets.constLast()), secondSsrc);
        QCOMPARE(sequenceOf(second->rtpPackets.constLast()),
                 static_cast<quint16>(sequenceOf(lastOnSecond) + 3));
        // No whole-peer refusal went out for either peer, and no new audio
        // context restarted playback.
        QVERIFY(controlsNamed(controls, QStringLiteral("rejected")).isEmpty());
        // (The first context, generation 1, may still have been on its way
        // when the spy began.)
        for (const QJsonObject& context : controlsNamed(controls, QStringLiteral("audio-context"))) {
            QCOMPARE(context.value(QStringLiteral("generation")).toInt(), 1);
            QCOMPARE(context.value(QStringLiteral("connectionId")).toString(),
                     QLatin1String(kFirst));
        }
        // Operations now name the new connection.
        const qsizetype contextsBefore =
            controlsNamed(controls, QStringLiteral("audio-context")).size();
        h.client.sendMediaControl({{QStringLiteral("op"), QStringLiteral("audio")},
                                   {QStringLiteral("connectionId"), QLatin1String(kSecond)},
                                   {QStringLiteral("revision"), 2},
                                   {QStringLiteral("enabled"), false}},
                                  h.client.sessionEpoch());
        QTRY_VERIFY(controlsNamed(controls, QStringLiteral("audio-context")).size() > contextsBefore);
        QCOMPARE(controlsNamed(controls, QStringLiteral("audio-context")).constLast()
                     .value(QStringLiteral("connectionId")).toString(),
                 QLatin1String(kSecond));
        // The old peer closes after draining.
        const QPointer<FakeTransport> firstLeft = h.transports.first();
        QTRY_VERIFY_WITH_TIMEOUT(firstLeft.isNull() || firstLeft->stopped,
                                 DaemonMediaController::kReplaceDrainMs + 3000);
    }

    // Refused, and the current peer untouched: a replacement naming a peer
    // that is not the current one, the current id itself, a second one
    // while one is under way.
    void aReplacementOfAnotherPeerIsRefused()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio());
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        const QString other = QStringLiteral("99999999-8888-4777-8666-555555555555");
        h.client.sendMediaControl(replaceControl(QLatin1String(kSecond), other),
                                  h.client.sessionEpoch());
        h.client.sendMediaControl(replaceControl(QLatin1String(kFirst), QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 2);
        for (const QJsonObject& refusal : controlsNamed(controls, QStringLiteral("rejected"))) {
            QCOMPARE(refusal.value(QStringLiteral("endpointId")).toInt(), 0);
            QCOMPARE(refusal.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("The Core did not move audio and display: that connection "
                                    "is not the current one."));
        }
        QCOMPARE(h.transports.size(), 1);
        QVERIFY(!h.transports.first()->stopped);
    }

    void legacyRelayMediaRefusesOverlapWithoutDisturbingAudio()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio());
        FakeTransport* const first = h.transports.first();
        MediaIcePath relayPath;
        relayPath.remoteAddress = QStringLiteral("127.0.0.1");
        relayPath.ownedLoopbackShim = true;
        first->path = relayPath;
        h.feed(4);
        const qsizetype sentBefore = first->rtpPackets.size();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.client.sendMediaControl(replaceControl(QLatin1String(kSecond), QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
        QCOMPARE(h.transports.size(), 1);
        QVERIFY(!first->stopped);
        h.feed(4);
        QVERIFY(first->rtpPackets.size() > sentBefore);
    }

    // ── The direct media ladder (the Core) ────────────────────────────

    // A window that declared `mediaDirect` is told mediaDirectVersion and
    // the Core's STUN (never a TURN server); its four-key replace starts a
    // connection with STUN and host candidates only (no tunnel, no relay),
    // and the Core's echo keeps its three fields.
    void aDirectReplaceStartsWithStunAndNoTunnel()
    {
        CoreHarness h;
        h.server.setMediaStun({QStringLiteral("stun:stun.example.test:3478"),
                               QStringLiteral("turn:relay.example.test:3478")});
        QVERIFY(h.establishWithAudio());
        QCOMPARE(h.client.capabilities().mediaDirectVersion, 1);
        QCOMPARE(h.client.capabilities().mediaStunUrls,
                 QStringList{QStringLiteral("stun:stun.example.test:3478")});
        QVERIFY(h.client.mediaDirectAvailable());
        const IceConfiguration clientDirect = h.client.mediaDirectIceConfiguration();
        QVERIFY(clientDirect.stunServer().has_value());
        QCOMPARE(clientDirect.stunServer()->host, QStringLiteral("stun.example.test"));
        QVERIFY(!clientDirect.hasCandidateSourceFactory());

        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QVERIFY(h.client.sendMediaControl(directReplaceControl(QLatin1String(kSecond),
                                                               QLatin1String(kFirst)),
                                          h.client.sessionEpoch()));
        QTRY_COMPARE(h.transports.size(), 2);
        FakeTransport* second = h.transports.at(1);
        QVERIFY(second->startOptions.ice.has_value());
        const IceConfiguration& ice = *second->startOptions.ice;
        QVERIFY(ice.stunServer().has_value());
        QCOMPARE(ice.stunServer()->host, QStringLiteral("stun.example.test"));
        QCOMPARE(ice.stunServer()->port, quint16(3478));
        QVERIFY(!ice.hasCandidateSourceFactory());
        QVERIFY(!ice.mediaRouting());
        QVERIFY(!ice.relayAllowed());
        second->becomeReady();
        h.feed(3);
        QTRY_COMPARE_WITH_TIMEOUT(controlsNamed(controls, QStringLiteral("replace")).size(), 1,
                                  DaemonMediaController::kReplaceOverlapMs + 3000);
        const QJsonObject done = controlsNamed(controls, QStringLiteral("replace")).first();
        QCOMPARE(done.size(), 3);
        QCOMPARE(done.value(QStringLiteral("connectionId")).toString(), QLatin1String(kSecond));
        QVERIFY(controlsNamed(controls, QStringLiteral("rejected")).isEmpty());
    }

    // Any other version, and a window that never declared `mediaDirect`,
    // start nothing; the undeclared window hears of neither capability.
    void aDirectReplaceNeedsVersionOneAndTheDeclaration()
    {
        {
            CoreHarness h;
            QVERIFY(h.establishWithAudio());
            QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
            h.client.sendMediaControl(directReplaceControl(QLatin1String(kSecond),
                                                           QLatin1String(kFirst), 2),
                                      h.client.sessionEpoch());
            // An ordering barrier, not a wait: a replace naming a connection
            // that is not the current one is refused, and that reply comes
            // after the Core handled the control before it.
            h.client.sendMediaControl(replaceControl(QLatin1String(kThird),
                                                     QLatin1String(kSecond)),
                                      h.client.sessionEpoch());
            QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
            QCOMPARE(h.transports.size(), 1);
            QVERIFY(!h.transports.first()->stopped);
        }
        {
            CoreHarness h;
            h.server.setMediaStun({QStringLiteral("stun:stun.example.test:3478")});
            h.client.withholdFeatureForTest(QByteArrayLiteral("mediaDirect"));
            QVERIFY(h.establishWithAudio());
            QCOMPARE(h.client.capabilities().mediaDirectVersion, 0);
            QVERIFY(h.client.capabilities().mediaStunUrls.isEmpty());
            QVERIFY(!h.client.mediaDirectAvailable());
            QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
            // Nor does a later change of the Core's STUN reach it.
            h.server.setMediaStun({QStringLiteral("stun:other.example.test:3478")});
            h.client.sendMediaControl(directReplaceControl(QLatin1String(kSecond),
                                                           QLatin1String(kFirst)),
                                      h.client.sessionEpoch());
            // The same barrier: the refused replace's reply comes after the
            // STUN change and the direct replace were handled.
            h.client.sendMediaControl(replaceControl(QLatin1String(kThird),
                                                     QLatin1String(kSecond)),
                                      h.client.sessionEpoch());
            QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
            QVERIFY(h.client.capabilities().mediaStunUrls.isEmpty());
            QCOMPARE(h.transports.size(), 1);
            // The three-field replace still works for it.
            h.client.sendMediaControl(replaceControl(QLatin1String(kSecond),
                                                     QLatin1String(kFirst)),
                                      h.client.sessionEpoch());
            QTRY_COMPARE(h.transports.size(), 2);
        }
    }

    // A STUN change reaches a declared window while it is connected.
    void theCoreRepublishesItsStun()
    {
        CoreHarness h;
        h.server.setMediaStun({QStringLiteral("stun:stun.example.test:3478")});
        QVERIFY(h.establishWithAudio());
        h.server.setMediaStun({QStringLiteral("stuns:secure.example.test:5349"),
                               QStringLiteral("turn:relay.example.test:3478"),
                               QStringLiteral("stun:user@stun.example.test:3478"),
                               QStringLiteral("stun:stun.example.test:3478?transport=udp")});
        QTRY_COMPARE(h.client.capabilities().mediaStunUrls,
                     QStringList{QStringLiteral("stuns:secure.example.test:5349")});
    }

    // An older relay leg in use (a plain start on the relay's raw tag-2
    // pair) still refuses a direct replace; the audio carries on.
    void aDirectReplaceOnTheOlderRelayLegIsRefused()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio());
        FakeTransport* const first = h.transports.first();
        first->path = tunnelShimPath();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.client.sendMediaControl(directReplaceControl(QLatin1String(kSecond),
                                                       QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
        QCOMPARE(controlsNamed(controls, QStringLiteral("rejected")).first()
                     .value(QStringLiteral("reason")).toString(),
                 QStringLiteral("This older relay media path cannot move while it is still "
                                "in use."));
        QCOMPARE(h.transports.size(), 1);
        QVERIFY(!first->stopped);
        const qsizetype before = first->rtpPackets.size();
        h.feed(2);
        QTRY_VERIFY(first->rtpPackets.size() > before);
    }

    // A media start that declared the tunnel (without the relay's routing)
    // runs over the tunnel's own framing: a direct replace is taken, and
    // the tunnel keeps carrying media until the new connection is ready.
    void aDirectReplaceFromTheTunnelIsTaken()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio({{QStringLiteral("mediaTunnelVersion"), 1}}));
        FakeTransport* const first = h.transports.first();
        QVERIFY(first->startOptions.ice.has_value());
        QVERIFY(first->startOptions.ice->mediaRouting());
        QVERIFY(first->startOptions.ice->hasCandidateSourceFactory());
        first->path = tunnelShimPath();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.client.sendMediaControl(directReplaceControl(QLatin1String(kSecond),
                                                       QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(h.transports.size(), 2);
        QVERIFY(controlsNamed(controls, QStringLiteral("rejected")).isEmpty());
        FakeTransport* const second = h.transports.at(1);
        QVERIFY(!second->startOptions.ice->hasCandidateSourceFactory());
        // The new connection fails: the tunnel still carries the audio.
        second->close();
        QTRY_VERIFY(second->stopped || h.transports.at(1).isNull());
        QVERIFY(!first->stopped);
        const qsizetype before = first->rtpPackets.size();
        h.feed(2);
        QTRY_VERIFY(first->rtpPackets.size() > before);
    }

    // What the older relay-leg refusal judges is the connection in use: a
    // start made without routing (the tunnel declared, but the session link
    // carried no binary then) that a plain replace moved onto the tunnel
    // takes a direct replace. (The start-time flag refused it.)
    void aDirectReplaceAfterAMoveOntoTheTunnelIsTaken()
    {
        CoreHarness h;
        h.server.setMediaStun({QStringLiteral("stun:stun.example.test:3478")});
        h.stationCarriesBinary = false;
        QVERIFY(h.establishWithAudio({{QStringLiteral("mediaTunnelVersion"), 1}}));
        FakeTransport* const first = h.transports.first();
        QVERIFY(!first->startOptions.ice || !first->startOptions.ice->mediaRouting());
        h.stationLink->setCarriesBinary(true);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QVERIFY(h.client.sendMediaControl(replaceControl(QLatin1String(kSecond),
                                                         QLatin1String(kFirst)),
                                          h.client.sessionEpoch()));
        QTRY_COMPARE(h.transports.size(), 2);
        FakeTransport* const second = h.transports.at(1);
        QVERIFY(second->startOptions.ice.has_value());
        QVERIFY(second->startOptions.ice->mediaRouting());
        QVERIFY(second->startOptions.ice->hasCandidateSourceFactory());
        second->path = tunnelShimPath();
        second->becomeReady();
        h.feed(3);
        QTRY_COMPARE_WITH_TIMEOUT(controlsNamed(controls, QStringLiteral("replace")).size(), 1,
                                  DaemonMediaController::kReplaceOverlapMs + 3000);
        QVERIFY(h.client.sendMediaControl(directReplaceControl(QLatin1String(kThird),
                                                               QLatin1String(kSecond)),
                                          h.client.sessionEpoch()));
        QTRY_COMPARE(h.transports.size(), 3);
        QVERIFY(controlsNamed(controls, QStringLiteral("rejected")).isEmpty());
        QVERIFY(!h.transports.at(2)->startOptions.ice->mediaRouting());
    }

    // ── The direct media ladder (the window) ─────────────────────────

    // Media that starts over the tunnel names the Core's STUN beside the
    // tunnel; a direct-only replace is tried at 5, 30, 120 and 300 s (the
    // last repeating), never while keyed or with VOX armed; one that fails
    // is dropped and the tunnel keeps carrying media.
    void theWindowTriesDirectOnItsScheduleAndKeepsTheTunnel()
    {
        GuiHarness g;
        g.core.server.setMediaStun({QStringLiteral("stun:stun.example.test:3478")});
        QVERIFY(g.connect(tunnelShimPath()));
        QVERIFY(g.core.client.mediaDirectAvailable());
        FakeTransport* const first = g.guiTransports.first();
        QVERIFY(first->startOptions.ice.has_value());
        QVERIFY(first->startOptions.ice->hasCandidateSourceFactory());
        QVERIFY(first->startOptions.ice->stunServer().has_value());
        QCOMPARE(first->startOptions.ice->stunServer()->host, QStringLiteral("stun.example.test"));
        const QString firstId = g.gui->mediaConnectionId();
        // The stall rule's tick (every 500 ms) arms the first step.
        QTRY_COMPARE_WITH_TIMEOUT(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[0],
                                  3000);

        g.gui->setMicKeyDown(true);
        g.gui->runDirectUpgradeStep();
        QCOMPARE(g.guiTransports.size(), 1);
        QCOMPARE(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[1]);
        g.gui->setMicKeyDown(false);

        QVERIFY(g.core.client.capabilities().txPermitted);
        g.gui->setVoxArmed(true);
        g.gui->runDirectUpgradeStep();
        QCOMPARE(g.guiTransports.size(), 1);
        QCOMPARE(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[2]);
        g.gui->setVoxArmed(false);

        g.gui->runDirectUpgradeStep();
        QCOMPARE(g.guiTransports.size(), 2);
        QVERIFY(g.gui->replacingConnection());
        const IceConfiguration& direct = *g.guiTransports.at(1)->startOptions.ice;
        QVERIFY(!direct.hasCandidateSourceFactory());
        QVERIFY(!direct.mediaRouting());
        QVERIFY(!direct.relayAllowed());
        QCOMPARE(direct.stunServer()->host, QStringLiteral("stun.example.test"));
        QTRY_COMPARE(g.core.transports.size(), 2);
        QVERIFY(!g.core.transports.at(1)->startOptions.ice->hasCandidateSourceFactory());
        QTRY_COMPARE(g.replacesSent().size(), 1);
        QCOMPARE(g.replacesSent().first().value(QStringLiteral("mediaDirectVersion")).toInt(), 1);
        QCOMPARE(g.replacesSent().first().size(), 4);

        // The direct connection fails: dropped, nothing pending, the tunnel
        // carries on, and the next step is the last one's.
        g.guiTransports.at(1)->close();
        QVERIFY(!g.gui->replacingConnection());
        QVERIFY(!g.gui->replacePending());
        QCOMPARE(g.gui->mediaConnectionId(), firstId);
        QVERIFY(!first->stopped);
        QTRY_COMPARE_WITH_TIMEOUT(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[3],
                                  3000);
        g.gui->runDirectUpgradeStep();
        QCOMPARE(g.guiTransports.size(), 3);
        g.guiTransports.at(2)->close();
        QTRY_COMPARE_WITH_TIMEOUT(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[3],
                                  3000);
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A Core that refused a direct-only replace leaves nothing pending: the
    // next try is the next step, not the move-follower's retry.
    void aRefusedDirectReplaceWaitsForTheNextStep()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        QTRY_VERIFY_WITH_TIMEOUT(g.gui->directUpgradeDelayMs() > 0, 3000);
        // The Core goes on the air where this window cannot hear it.
        MoxController* mox = g.core.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        g.core.radio.transmitModel().setMicSourceLocked(false);
        g.core.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = g.core.radio.sliceById(g.core.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        g.stationLink->setDropsOutgoing(true);
        mox->setMox(true);
        QTRY_VERIFY(mox->state() != MoxState::Rx);
        g.stationLink->setDropsOutgoing(false);
        QSignalSpy controls(&g.core.client, &StationClient::mediaControlReceived);
        g.gui->runDirectUpgradeStep();
        QCOMPARE(g.guiTransports.size(), 2);
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
        QVERIFY(!g.gui->replacingConnection());
        QVERIFY(!g.gui->replacePending());
        QVERIFY(!g.guiTransports.first()->stopped);
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A direct path silent for kDirectMediaSilenceFallbackMs (not a moment
    // less) goes back to the tunnel alone (its candidate only: no STUN, no
    // host candidates), by the three-field replace, and the direct schedule
    // starts over; never on the tunnel.
    void aSilentDirectPathFallsBackToTheTunnel()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        QTRY_COMPARE_WITH_TIMEOUT(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[0],
                                  3000);
        // Two refused steps (keyed) move the schedule on.
        g.gui->setMicKeyDown(true);
        g.gui->runDirectUpgradeStep();
        g.gui->runDirectUpgradeStep();
        g.gui->setMicKeyDown(false);
        QCOMPARE(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[2]);
        // On the tunnel, silence is the stall rule's, not this one's.
        g.now += 10 * RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);

        // Now on a direct pair.
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        const qint64 lastPacket = g.now;
        g.now = lastPacket + RemoteMediaController::kDirectMediaSilenceFallbackMs - 1;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);
        g.now = lastPacket + RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        const IceConfiguration& fallback = *g.guiTransports.at(1)->startOptions.ice;
        QVERIFY(fallback.hasCandidateSourceFactory());
        QVERIFY(fallback.onlySourceCandidates());
        QVERIFY(!fallback.stunServer().has_value());
        QVERIFY(!fallback.relayAllowed());
        QVERIFY(fallback.mediaRouting());
        QTRY_COMPARE(g.replacesSent().size(), 1);
        QCOMPARE(g.replacesSent().first().size(), 3);
        QVERIFY(!g.replacesSent().first().contains(QStringLiteral("mediaDirectVersion")));
        QTRY_COMPARE(g.core.transports.size(), 2);
        QVERIFY(g.core.transports.at(1)->startOptions.ice->hasCandidateSourceFactory());

        // The schedule starts over at its first step once media is back on
        // the tunnel.
        g.guiTransports.at(1)->close();
        g.guiTransports.first()->path = tunnelShimPath();
        g.feeding = true;
        QTRY_COMPARE_WITH_TIMEOUT(g.gui->directUpgradeDelayMs(), PathRacer::kUpgradeRetryMs[0],
                                  3000);
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The fallback runs once per silence: a Core still silent after it
    // (here the fallback's connection failed, so media stays on the direct
    // pair) is not replaced again; a window after the fallback finished the
    // window asks for recovery instead. Media coming back re-arms it.
    void theSilenceFallbackRunsOnceThenRecovers()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        QSignalSpy recovery(g.gui.get(), &RemoteMediaController::recoveryRequested);
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        g.guiTransports.at(1)->close();
        QVERIFY(!g.gui->replacingConnection());
        const qint64 finished = g.now;
        // Two more windows of silence: no second replace.
        g.now = finished + RemoteMediaController::kDirectMediaSilenceFallbackMs - 1;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        QVERIFY(recovery.isEmpty());
        g.now = finished + RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(recovery.size(), 1);
        g.now += 2 * RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        QCOMPARE(recovery.size(), 1);
        QTRY_COMPARE(g.replacesSent().size(), 1);
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A media packet arms the fallback again: the next silence falls back
    // once more, and no recovery is asked for.
    void aMediaPacketArmsTheFallbackAgain()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        QSignalSpy recovery(g.gui.get(), &RemoteMediaController::recoveryRequested);
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        g.guiTransports.at(1)->close();
        QVERIFY(!g.gui->replacingConnection());
        g.audioOn(0);
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 3);
        QVERIFY(recovery.isEmpty());
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A fallback that moved media onto the tunnel but brought no media
    // back asks for recovery one window after the move.
    void aFallbackThatBringsNoMediaAsksForRecovery()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        QSignalSpy recovery(g.gui.get(), &RemoteMediaController::recoveryRequested);
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        // The Core moves media onto the fallback's connection.
        QTRY_COMPARE(g.core.transports.size(), 2);
        g.guiTransports.at(1)->path = tunnelShimPath();
        g.core.transports.at(1)->path = tunnelShimPath();
        g.guiTransports.at(1)->becomeReady();
        g.core.transports.at(1)->becomeReady();
        const QString fallbackId = g.guiTransports.at(1)->startOptions.connectionId;
        g.core.feed(3);
        QTRY_COMPARE_WITH_TIMEOUT(g.gui->mediaConnectionId(), fallbackId,
                                  DaemonMediaController::kReplaceOverlapMs + 3000);
        // The old connection's late packets are taken for a while; the
        // check waits until that connection is let go.
        QTRY_VERIFY_WITH_TIMEOUT(!g.guiTransports.at(0) || g.guiTransports.at(0)->stopped,
                                 DaemonMediaController::kReplaceOverlapMs + 3000);
        const qint64 moved = g.now;
        g.now = moved + RemoteMediaController::kDirectMediaSilenceFallbackMs - 1;
        g.gui->checkMediaSilence();
        QVERIFY(recovery.isEmpty());
        g.now = moved + RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(recovery.size(), 1);
        QCOMPARE(g.guiTransports.size(), 2);
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Silence the window does not ask for (muted, or its radio not
    // connected) never falls back.
    void noFallbackWhileTheWindowWantsNoAudio()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        QVERIFY(g.core.remote.isConnected());
        g.core.remote.audioEngine()->setMasterMuted(true);
        g.now += 2 * RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);
        g.core.remote.audioEngine()->setMasterMuted(false);
        g.core.remote.setConnectionStateForTest(ConnectionState::Disconnected);
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);
        g.core.remote.setConnectionStateForTest(ConnectionState::Connected);
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Silence while the Core is keyed is expected: back on receive, the
    // silence clock starts again rather than counting the keyed time.
    void theReturnToReceiveRestartsTheSilenceClock()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        MoxController* mox = g.core.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        g.core.radio.transmitModel().setMicSourceLocked(false);
        g.core.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = g.core.radio.sliceById(g.core.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        const TransmitState* tx = g.core.client.transmitState();
        QVERIFY(tx != nullptr);
        const auto onAir = [tx] { return tx->keyed() || tx->tuning() || tx->txEnding(); };
        mox->setMox(true);
        QTRY_VERIFY(onAir());
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs + 1000;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);
        mox->setMox(false);
        QTRY_VERIFY(!onAir());
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs - 1;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);
        g.now += 1;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A fallback the Core refuses because it went on the air as the replace
    // arrived is tried again once it is back on receive, and the retry is
    // still a fallback onto the tunnel alone, never a normal replace with
    // STUN and host candidates.
    void aFallbackRefusedWhileTransmittingStaysOnTheTunnelAlone()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        MoxController* mox = g.core.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        g.core.radio.transmitModel().setMicSourceLocked(false);
        g.core.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = g.core.radio.sliceById(g.core.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        const TransmitState* tx = g.core.client.transmitState();
        QVERIFY(tx != nullptr);
        const auto onAir = [tx] { return tx->keyed() || tx->tuning() || tx->txEnding(); };
        // The Core keys the moment the window's replace leaves it, so the
        // Core reads that replace while it transmits.
        Test::LoopbackTransport* windowLink = g.stationLink->peerForTest();
        QVERIFY(windowLink != nullptr);
        bool keyed = false;
        const QMetaObject::Connection keyOnReplace = QObject::connect(
            windowLink, &Test::LoopbackTransport::outboundText, windowLink,
            [&keyed, mox](const QByteArray& wire) {
                if (!keyed && wire.contains("\"replace\"")) {
                    keyed = true;
                    mox->setMox(true);
                }
            });
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        QVERIFY(g.guiTransports.at(1)->startOptions.ice->onlySourceCandidates());
        QVERIFY(keyed);
        QObject::disconnect(keyOnReplace);
        // Refused while transmitting: the move waits, and the Core started
        // no connection for it (within the window's replace deadline).
        QTRY_VERIFY_WITH_TIMEOUT(g.gui->replacePending(), kReplaceAnswerMs);
        QCOMPARE(g.core.transports.size(), 1);
        mox->setMox(false);
        QTRY_VERIFY(!onAir());
        QTRY_COMPARE_WITH_TIMEOUT(g.core.transports.size(), 2,
                                  RemoteMediaController::kReplaceRetryMs + 5000);
        QVERIFY(!g.gui->replacePending());
        // Every connection the window started after the first, the retry
        // included, has the tunnel's candidate alone (a refused one is
        // already gone; the retry is live).
        QVERIFY(g.guiTransports.size() >= 3);
        QVERIFY(g.guiTransports.last());
        for (int i = 1; i < g.guiTransports.size(); ++i) {
            if (!g.guiTransports.at(i)) { continue; }
            const IceConfiguration& ice = *g.guiTransports.at(i)->startOptions.ice;
            QVERIFY2(ice.onlySourceCandidates(), qPrintable(QString::number(i)));
            QVERIFY(!ice.stunServer().has_value());
        }
        QVERIFY(g.core.transports.at(1)->startOptions.ice->hasCandidateSourceFactory());
        for (const QJsonObject& replace : g.replacesSent()) {
            QCOMPARE(replace.size(), 3);
        }
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The session moves while a refused fallback waits for the Core to be
    // back on receive: the move is followed, and the replace is still a
    // fallback onto the tunnel alone, so it cannot pick the silent direct
    // pair again.
    void aSessionMoveKeepsAWaitingFallbackOnTheTunnelAlone()
    {
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        MoxController* mox = g.core.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        g.core.radio.transmitModel().setMicSourceLocked(false);
        g.core.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = g.core.radio.sliceById(g.core.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        const TransmitState* tx = g.core.client.transmitState();
        QVERIFY(tx != nullptr);
        const auto onAir = [tx] { return tx->keyed() || tx->tuning() || tx->txEnding(); };
        Test::LoopbackTransport* windowLink = g.stationLink->peerForTest();
        QVERIFY(windowLink != nullptr);
        bool keyed = false;
        const QMetaObject::Connection keyOnReplace = QObject::connect(
            windowLink, &Test::LoopbackTransport::outboundText, windowLink,
            [&keyed, mox](const QByteArray& wire) {
                if (!keyed && wire.contains("\"replace\"")) {
                    keyed = true;
                    mox->setMox(true);
                }
            });
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        QVERIFY(g.guiTransports.at(1)->startOptions.ice->onlySourceCandidates());
        QVERIFY(keyed);
        QObject::disconnect(keyOnReplace);
        // The Core refuses the replace the moment it leaves receive, but
        // the window hears the Core on the air only on the Core's next delta
        // flush (StationServer::kDefaultDeltaFlushMs, a rate limit with no
        // delivery bound of its own). Until then the window takes the Core
        // for idle and would start the move at once; so the move is made
        // once the window has heard it, and the refusal is waited for again
        // (a retry in between is refused the same way). Both come over the
        // same link, within the window's bound for a replace answer.
        QTRY_VERIFY_WITH_TIMEOUT(g.core.remote.isTransmitting(), kReplaceAnswerMs);
        QTRY_VERIFY_WITH_TIMEOUT(g.gui->replacePending(), kReplaceAnswerMs);
        QCOMPARE(g.core.transports.size(), 1);
        // The session moves while the refused fallback waits.
        emit g.core.client.pathChanged();
        QVERIFY(g.gui->replacePending());
        mox->setMox(false);
        QTRY_VERIFY(!onAir());
        QTRY_COMPARE_WITH_TIMEOUT(g.core.transports.size(), 2,
                                  RemoteMediaController::kReplaceRetryMs + 5000);
        QVERIFY(!g.gui->replacePending());
        // Every connection the window started after the first has the
        // tunnel's candidate alone, the one that followed the move included.
        QVERIFY(g.guiTransports.size() >= 3);
        QVERIFY(g.guiTransports.last());
        for (int i = 1; i < g.guiTransports.size(); ++i) {
            if (!g.guiTransports.at(i)) { continue; }
            const IceConfiguration& ice = *g.guiTransports.at(i)->startOptions.ice;
            QVERIFY2(ice.onlySourceCandidates(), qPrintable(QString::number(i)));
            QVERIFY(!ice.stunServer().has_value());
        }
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Media comes back on the direct pair while a refused fallback waits
    // for the Core to be back on receive: the silence that fallback
    // answered is over, so it is dropped, and nothing moves to the tunnel
    // when the Core unkeys. With a session move folded into the wait, the
    // move is still followed, as a normal replace (the direct pair is no
    // longer silent, so nothing keeps it to the tunnel alone).
    void mediaBackOnTheDirectPairEndsAWaitingFallback_data()
    {
        QTest::addColumn<bool>("moved");
        QTest::newRow("no move") << false;
        QTest::newRow("a move folded in") << true;
    }
    void mediaBackOnTheDirectPairEndsAWaitingFallback()
    {
        QFETCH(bool, moved);
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        MoxController* mox = g.core.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        g.core.radio.transmitModel().setMicSourceLocked(false);
        g.core.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = g.core.radio.sliceById(g.core.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        const TransmitState* tx = g.core.client.transmitState();
        QVERIFY(tx != nullptr);
        const auto onAir = [tx] { return tx->keyed() || tx->tuning() || tx->txEnding(); };
        Test::LoopbackTransport* windowLink = g.stationLink->peerForTest();
        QVERIFY(windowLink != nullptr);
        bool keyed = false;
        const QMetaObject::Connection keyOnReplace = QObject::connect(
            windowLink, &Test::LoopbackTransport::outboundText, windowLink,
            [&keyed, mox](const QByteArray& wire) {
                if (!keyed && wire.contains("\"replace\"")) {
                    keyed = true;
                    mox->setMox(true);
                }
            });
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        QVERIFY(g.guiTransports.at(1)->startOptions.ice->onlySourceCandidates());
        QVERIFY(keyed);
        QObject::disconnect(keyOnReplace);
        // As in aSessionMoveKeepsAWaitingFallbackOnTheTunnelAlone: the move
        // is made once the window has heard the Core on the air, and the
        // refusal is waited for within the window's replace deadline.
        QTRY_VERIFY_WITH_TIMEOUT(g.core.remote.isTransmitting(), kReplaceAnswerMs);
        QTRY_VERIFY_WITH_TIMEOUT(g.gui->replacePending(), kReplaceAnswerMs);
        QCOMPARE(g.core.transports.size(), 1);
        if (moved) {
            emit g.core.client.pathChanged();
            QVERIFY(g.gui->replacePending());
        }
        // Audio arrives again on the direct pair while the retry waits.
        g.audioOn(0);
        QCOMPARE(g.gui->replacePending(), moved);
        mox->setMox(false);
        QTRY_VERIFY(!onAir());
        if (!moved) {
            // Past the retry: no replace, and media stays on the direct pair.
            QTest::qWait(RemoteMediaController::kReplaceRetryMs + 500);
            QVERIFY(!g.gui->replacePending());
            QCOMPARE(g.core.transports.size(), 1);
            QCOMPARE(g.guiTransports.size(), 2);
        } else {
            // The move is followed, by a normal replace.
            QTRY_COMPARE_WITH_TIMEOUT(g.core.transports.size(), 2,
                                      RemoteMediaController::kReplaceRetryMs + 5000);
            QVERIFY(!g.gui->replacePending());
            QVERIFY(g.guiTransports.size() >= 3);
            QVERIFY(g.guiTransports.last());
            QVERIFY(!g.guiTransports.last()->startOptions.ice->onlySourceCandidates());
        }
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A session move folded into a waiting fallback rides with the replace
    // that retries it; if the Core refuses that replace too (it keyed again
    // as it arrived, or the window had not yet heard it keyed), the move
    // stays pending. The same holds for a move that comes while the
    // fallback's own replace is under way. Media then comes back on the
    // direct pair, which ends the fallback but not the move: after the Core
    // unkeys, the move is followed by a normal replace.
    void aRefusedReplaceKeepsTheMoveItCarried_data()
    {
        QTest::addColumn<bool>("duringFlight");
        QTest::newRow("folded into the retried fallback") << false;
        QTest::newRow("a move while the fallback is under way") << true;
    }
    void aRefusedReplaceKeepsTheMoveItCarried()
    {
        QFETCH(bool, duringFlight);
        GuiHarness g;
        QVERIFY(g.connect(tunnelShimPath()));
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        MoxController* mox = g.core.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        g.core.radio.transmitModel().setMicSourceLocked(false);
        g.core.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = g.core.radio.sliceById(g.core.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        const TransmitState* tx = g.core.client.transmitState();
        QVERIFY(tx != nullptr);
        const auto onAir = [tx] { return tx->keyed() || tx->tuning() || tx->txEnding(); };
        Test::LoopbackTransport* windowLink = g.stationLink->peerForTest();
        QVERIFY(windowLink != nullptr);
        // The Core keys the moment a replace leaves the window, so it reads
        // that replace while it transmits and refuses it.
        int replacesSeen = 0;
        int keyOnReplace = 1;
        const QMetaObject::Connection keyer = QObject::connect(
            windowLink, &Test::LoopbackTransport::outboundText, windowLink,
            [&replacesSeen, &keyOnReplace, mox](const QByteArray& wire) {
                if (!wire.contains("\"replace\"")) { return; }
                if (++replacesSeen == keyOnReplace) { mox->setMox(true); }
            });
        const auto refused = [&g](int index) {
            return !g.guiTransports.at(index) || g.guiTransports.at(index)->stopped;
        };
        g.now += RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 2);
        QVERIFY(g.guiTransports.at(1)->startOptions.ice->onlySourceCandidates());
        QCOMPARE(replacesSeen, 1);
        int carrier = 1;
        if (duringFlight) {
            // The fallback's replace is on its way and not yet answered: the
            // session moves now, and the move waits for it.
            QVERIFY(!refused(1));
            QVERIFY(!g.gui->replacePending());
            emit g.core.client.pathChanged();
            QVERIFY(g.gui->replacePending());
        } else {
            // The fallback is refused; the session moves once the window has
            // heard the Core on the air, so the move folds into the wait.
            QTRY_VERIFY_WITH_TIMEOUT(g.core.remote.isTransmitting(), kReplaceAnswerMs);
            QTRY_VERIFY_WITH_TIMEOUT(g.gui->replacePending(), kReplaceAnswerMs);
            QCOMPARE(g.core.transports.size(), 1);
            emit g.core.client.pathChanged();
            QVERIFY(g.gui->replacePending());
            // The retry, carrying the move, is refused as well: the Core
            // keys again as it arrives.
            keyOnReplace = 2;
            mox->setMox(false);
            QTRY_VERIFY_WITH_TIMEOUT(replacesSeen == 2,
                                     RemoteMediaController::kReplaceRetryMs + 5000);
            QCOMPARE(g.guiTransports.size(), 3);
            carrier = 2;
        }
        // The refusal of the replace carrying the move, within the window's
        // bound for a replace answer; the window then hears the Core on the
        // air (so no retry starts while this test looks).
        QTRY_VERIFY_WITH_TIMEOUT(refused(carrier), kReplaceAnswerMs);
        QTRY_VERIFY_WITH_TIMEOUT(g.core.remote.isTransmitting() && g.gui->replacePending(),
                                 kReplaceAnswerMs);
        QCOMPARE(g.core.transports.size(), 1);
        QObject::disconnect(keyer);
        // Audio arrives again on the direct pair: the fallback is over, the
        // move is not.
        g.audioOn(0);
        QVERIFY(g.gui->replacePending());
        mox->setMox(false);
        QTRY_VERIFY(!onAir());
        QTRY_COMPARE_WITH_TIMEOUT(g.core.transports.size(), 2,
                                  RemoteMediaController::kReplaceRetryMs + 5000);
        QVERIFY(!g.gui->replacePending());
        QVERIFY(g.guiTransports.last());
        QVERIFY(!g.guiTransports.last()->startOptions.ice->onlySourceCandidates());
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A Core that never took `mediaDirect` (an older one, as the window
    // sees it) gets the three-field replace only: no direct schedule, no
    // silence fallback, no new field on the wire.
    void anOlderCoreKeepsTheThreeFieldReplace()
    {
        GuiHarness g;
        g.core.client.withholdFeatureForTest(QByteArrayLiteral("mediaDirect"));
        QVERIFY(g.connect(tunnelShimPath()));
        QVERIFY(!g.core.client.mediaDirectAvailable());
        // The stall tick that marks the tunnel in use runs the direct
        // schedule's update right after, in the same tick; wait for a
        // second mark so that update has run at least once.
        QTRY_VERIFY_WITH_TIMEOUT(g.core.client.mediaTunnelInUse(), 5000);
        g.core.client.setMediaTunnelInUse(false);
        QTRY_VERIFY_WITH_TIMEOUT(g.core.client.mediaTunnelInUse(), 5000);
        QCOMPARE(g.gui->directUpgradeDelayMs(), -1);
        QVERIFY(!g.gui->upgradeToDirectConnection());
        MediaIcePath hostPath;
        hostPath.remoteAddress = QStringLiteral("127.0.0.1");
        g.feeding = false;
        g.guiTransports.first()->path = hostPath;
        g.audioOn(0);
        g.now += 10 * RemoteMediaController::kDirectMediaSilenceFallbackMs;
        g.gui->checkMediaSilence();
        QCOMPARE(g.guiTransports.size(), 1);
        QVERIFY(g.gui->replaceConnection());
        QTRY_COMPARE(g.replacesSent().size(), 1);
        QCOMPARE(g.replacesSent().first().size(), 3);
        for (const QByteArray& wire : g.toCore) {
            QVERIFY(!wire.contains("mediaDirect"));
        }
        g.core.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // Nothing moves while the radio is on the air (the media document's
    // "Transmit"): refused in plain words, and the key untouched.
    void noReplacementWhileKeyed()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio());
        MoxController* mox = h.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        h.radio.transmitModel().setMicSourceLocked(false);
        h.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = h.radio.sliceById(h.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        h.server.setRemoteTransmitAllowed(true);
        mox->setMox(true);
        QTRY_VERIFY(mox->state() != MoxState::Rx);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.client.sendMediaControl(replaceControl(QLatin1String(kSecond), QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
        QCOMPARE(controlsNamed(controls, QStringLiteral("rejected")).first()
                     .value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core did not move audio and display: the radio is "
                                "transmitting."));
        QCOMPARE(h.transports.size(), 1);
        // The direct media ladder: a direct-only replace meets the same gate.
        h.client.sendMediaControl(directReplaceControl(QLatin1String(kSecond),
                                                       QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 2);
        QCOMPARE(controlsNamed(controls, QStringLiteral("rejected")).at(1)
                     .value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core did not move audio and display: the radio is "
                                "transmitting."));
        QCOMPARE(h.transports.size(), 1);
        QVERIFY(mox->state() != MoxState::Rx);
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Task 29 fix wave (review Important 2): MOX released but its unkey
    // delay still running (MOX off, the state not yet receive): a
    // replacement is refused as while keyed.
    void noReplacementDuringTheUnkeyDelay()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio());
        MoxController* mox = h.radio.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        h.radio.transmitModel().setMicSourceLocked(false);
        h.radio.transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* slice = h.radio.sliceById(h.slice)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        h.server.setRemoteTransmitAllowed(true);
        mox->setMox(true);
        QTRY_VERIFY(mox->state() != MoxState::Rx);
        QTRY_VERIFY(mox->isMox());
        mox->setTimerIntervals(3000, 3000, 3000, 3000, 3000, 3000);
        mox->setMox(false);
        QTRY_VERIFY(!mox->isMox());
        QVERIFY(mox->state() != MoxState::Rx);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.client.sendMediaControl(replaceControl(QLatin1String(kSecond), QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
        QCOMPARE(controlsNamed(controls, QStringLiteral("rejected")).first()
                     .value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core did not move audio and display: the radio is "
                                "transmitting."));
        QCOMPARE(h.transports.size(), 1);
        QVERIFY(mox->state() != MoxState::Rx);
        QTRY_COMPARE_WITH_TIMEOUT(mox->state(), MoxState::Rx, 20000);
    }

    // A new peer that closes before it takes over is dropped with the
    // whole-peer refusal for its own id; the current peer carries on.
    void aNewPeerThatClosesLeavesTheCurrentOne()
    {
        CoreHarness h;
        QVERIFY(h.establishWithAudio());
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        h.client.sendMediaControl(replaceControl(QLatin1String(kSecond), QLatin1String(kFirst)),
                                  h.client.sessionEpoch());
        QTRY_COMPARE(h.transports.size(), 2);
        h.transports.at(1)->close();
        QTRY_COMPARE(controlsNamed(controls, QStringLiteral("rejected")).size(), 1);
        const QJsonObject refusal = controlsNamed(controls, QStringLiteral("rejected")).first();
        QCOMPARE(refusal.value(QStringLiteral("connectionId")).toString(), QLatin1String(kSecond));
        QCOMPARE(refusal.value(QStringLiteral("reason")).toString(),
                 QString::fromLatin1(kMediaPeerClosedReason));
        FakeTransport* first = h.transports.first();
        QVERIFY(!first->stopped);
        const int before = first->rtpPackets.size();
        h.feed(1);
        QTRY_COMPARE(first->rtpPackets.size(), before + 1);
        QVERIFY(controlsNamed(controls, QStringLiteral("replace")).isEmpty());
    }

    // ── Dual receive ──────────────────────────────────────────────────

    // The window's jitter queue fed from two paths across a switch, on a
    // simulated clock: a relay path of 150 ms with 2 % loss and jitter up
    // to 60 ms, then from the replacement's ready a direct path of 20 ms
    // with 1 % loss, both carrying the same packets for the overlap, then
    // the direct path alone, merged by DualPathAudio as the window merges
    // them. Every interval plays once or is skipped once: none concealed
    // that one path carried, none twice, and never two in a row skipped.
    void twoPathsAcrossASwitchPlayEachIntervalOnce()
    {
        constexpr qint64 kMs = 1'000'000;
        constexpr qint64 kPacketNs = AudioJitterBuffer::kDefaultPacketDurationNs;
        constexpr int kFramesPerPacket = AudioJitterBuffer::kDefaultPacketFrames;
        constexpr int kPackets = 400;         // 16 s of audio
        constexpr int kReadyAt = 60;          // the replacement is ready
        constexpr quint32 kSsrc = 0xAAAA;
        const int doneAt = kReadyAt + DaemonMediaController::kReplaceOverlapMs / 40;
        AudioJitterBuffer jitter;
        jitter.reset(0);
        qint64 now = 0;
        DualPathAudio dual([&jitter, &now](const QByteArray& packet) {
            jitter.insert(packet, qFromBigEndian<quint32>(packet.constData() + 4), now);
        });
        struct Arrival {
            quint32 timestamp;
            bool fromNew;
        };
        std::multimap<qint64, Arrival> arrivals;
        quint32 lcg = 12345;
        const auto random = [&lcg] {
            lcg = lcg * 1103515245u + 12345u;
            return (lcg >> 8) % 10000;
        };
        for (int i = 0; i < kPackets; ++i) {
            const qint64 sent = i * kPacketNs;
            const quint32 timestamp = static_cast<quint32>(i * kFramesPerPacket);
            const bool onRelay = i < doneAt;
            const bool onDirect = i >= kReadyAt;
            if (onRelay && random() >= 200) {
                arrivals.emplace(sent + 150 * kMs + static_cast<qint64>(random() % 60) * kMs,
                                 Arrival{timestamp, false});
            }
            if (onDirect && random() >= 100) {
                arrivals.emplace(sent + 20 * kMs, Arrival{timestamp, true});
            }
        }
        QSet<quint32> arrived;
        for (const auto& [at, arrival] : arrivals) {
            arrived.insert(arrival.timestamp);
        }
        // The Core's `replace` reaches the window over the direct control
        // path just after it stops sending on the relay.
        const qint64 doneNs = doneAt * kPacketNs + 20 * kMs;
        bool doneSeen = false;
        QList<quint32> played;
        QList<quint32> concealed;
        auto next = arrivals.cbegin();
        const qint64 end = kPackets * kPacketNs + 800 * kMs;
        const auto packetFor = [](quint32 timestamp) {
            QByteArray packet(20, 'x');
            packet[0] = char(0x80);
            qToBigEndian<quint32>(timestamp, packet.data() + 4);
            qToBigEndian<quint32>(kSsrc, packet.data() + 8);
            return packet;
        };
        for (now = 0; now < end; now += kMs) {
            const qint64 nowMs = now / kMs;
            if (!doneSeen && now >= kReadyAt * kPacketNs + 20 * kMs && !dual.active()) {
                dual.start(nowMs);
            }
            if (!doneSeen && now >= doneNs) {
                dual.oldPathDone(nowMs);
                doneSeen = true;
            }
            for (; next != arrivals.cend() && next->first <= now; ++next) {
                dual.submit(packetFor(next->second.timestamp), next->second.fromNew, nowMs);
            }
            dual.tick(nowMs);
            while (const std::optional<AudioJitterBuffer::Playout> out = jitter.takeReady(now)) {
                (out->concealed() ? concealed : played).append(out->timestamp);
            }
            for (const QByteArray& shed : jitter.takeShedPackets()) {
                Q_UNUSED(shed);
            }
        }
        QVERIFY(!dual.active());
        QVERIFY(dual.duplicatesDropped() > 0);
        // Nothing twice, in order.
        const QSet<quint32> unique(played.cbegin(), played.cend());
        QCOMPARE(unique.size(), played.size());
        for (qsizetype i = 1; i < played.size(); ++i) {
            QVERIFY(played.at(i) > played.at(i - 1));
        }
        // Nothing one path carried was concealed.
        for (const quint32 timestamp : std::as_const(concealed)) {
            QVERIFY2(!arrived.contains(timestamp),
                     qPrintable(QStringLiteral("interval %1 concealed though it arrived")
                                    .arg(timestamp / kFramesPerPacket)));
        }
        // The lower delay reached one skipped interval at a time: never two
        // in a row that arrived.
        const QSet<quint32> heard = unique + QSet<quint32>(concealed.cbegin(), concealed.cend());
        int run = 0;
        int longest = 0;
        for (int i = 1; i < kPackets; ++i) {
            const quint32 timestamp = static_cast<quint32>(i * kFramesPerPacket);
            run = !heard.contains(timestamp) && arrived.contains(timestamp) ? run + 1 : 0;
            longest = std::max(longest, run);
        }
        QVERIFY2(longest <= 1, qPrintable(QStringLiteral("%1 intervals in a row skipped")
                                               .arg(longest)));
        // The lead eased to nothing: the direct path's own delay again.
        QCOMPARE(dual.leadMs(), qint64{0});
    }

    // DualPathAudio alone: the new path's packet waits for the old copy,
    // which sets the schedule; a packet the old path never brings goes on
    // at its arrival plus the lead; the lead eases once the old path is
    // done.
    // Task 29 fix wave (review Minor 11): packets the new path holds go
    // on in the order they came, across the RTP timestamp's wrap.
    void heldPacketsGoOnInArrivalOrderAcrossTheWrap()
    {
        QList<quint32> delivered;
        DualPathAudio dual([&delivered](const QByteArray& packet) {
            delivered.append(qFromBigEndian<quint32>(packet.constData() + 4));
        });
        const auto packet = [](quint32 timestamp) {
            QByteArray bytes(20, 'x');
            qToBigEndian<quint32>(timestamp, bytes.data() + 4);
            qToBigEndian<quint32>(7, bytes.data() + 8);
            return bytes;
        };
        dual.start(0);
        const QList<quint32> sent{0xFFFFF880u, 0xFFFFFC40u, 0x00000000u, 0x000003C0u};
        for (const quint32 timestamp : sent) {
            dual.submit(packet(timestamp), true, 0);
        }
        QVERIFY(delivered.isEmpty());
        // No old copy and no lead known: all due at once, in arrival order.
        dual.tick(DualPathAudio::kMaxWaitMs);
        QCOMPARE(delivered, sent);
    }

    void theNewPathWaitsForTheOldAndThenLeadsNothing()
    {
        QList<QPair<quint32, qint64>> delivered;
        qint64 now = 0;
        DualPathAudio dual([&delivered, &now](const QByteArray& packet) {
            delivered.append({qFromBigEndian<quint32>(packet.constData() + 4), now});
        });
        const auto packet = [](quint32 timestamp) {
            QByteArray bytes(20, 'x');
            qToBigEndian<quint32>(timestamp, bytes.data() + 4);
            qToBigEndian<quint32>(7, bytes.data() + 8);
            return bytes;
        };
        dual.start(0);
        // New first at 0, old at 130: delivered at 130, from the old.
        dual.submit(packet(1920), true, now);
        QVERIFY(delivered.isEmpty());
        now = 130;
        dual.submit(packet(1920), false, now);
        QCOMPARE(delivered.size(), 1);
        QCOMPARE(delivered.first().second, qint64{130});
        QCOMPARE(dual.leadMs(), qint64{130});
        // A packet the old path lost: at its arrival plus the lead.
        now = 200;
        dual.submit(packet(3840), true, now);
        now = 329;
        dual.tick(now);
        QCOMPARE(delivered.size(), 1);
        now = 330;
        dual.tick(now);
        QCOMPARE(delivered.size(), 2);
        // Its late old copy is dropped.
        now = 360;
        dual.submit(packet(3840), false, now);
        QCOMPARE(delivered.size(), 2);
        QCOMPARE(dual.duplicatesDropped(), quint64{2});
        // Done: the lead eases one interval every kEaseIntervalMs.
        dual.oldPathDone(now);
        now += DualPathAudio::kEaseIntervalMs;
        dual.tick(now);
        QCOMPARE(dual.leadMs(), qint64{130 - DualPathAudio::kEaseStepMs});
        for (int i = 0; i < 4; ++i) {
            now += DualPathAudio::kEaseIntervalMs;
            dual.tick(now);
        }
        QCOMPARE(dual.leadMs(), qint64{0});
        QVERIFY(!dual.active());
        // A new path that is gone drops what it held.
        dual.start(now);
        dual.submit(packet(99 * 1920), true, now);
        QCOMPARE(dual.held(), 1);
        dual.newPathGone();
        QCOMPARE(dual.held(), 0);
        QVERIFY(!dual.active());
    }

    // The Core's refusals of a replacement are in plain words.
    void theRefusalsAreInPlainWords()
    {
        for (const char* text :
             {"The Core did not move audio and display: that connection is not the current one.",
              "The Core did not move audio and display: the radio is transmitting."}) {
            QVERIFY2(OperatorWording::isPlain(QLatin1String(text)), text);
        }
    }

    // RtpDuplicateFilter alone: a timestamp is taken once per stream, the
    // streams apart, and it forgets beyond its window.
    void theFilterTakesEachTimestampOncePerStream()
    {
        RtpDuplicateFilter filter;
        QVERIFY(filter.admit(1, 1920));
        QVERIFY(!filter.admit(1, 1920));
        QVERIFY(filter.admit(2, 1920));
        for (int i = 2; i < 2 + RtpDuplicateFilter::kWindow; ++i) {
            QVERIFY(filter.admit(1, static_cast<quint32>(i * 1920)));
        }
        // 1920 has left the window.
        QVERIFY(filter.admit(1, 1920));
        QCOMPARE(filter.dropped(), quint64{1});
        QByteArray packet(12, '\0');
        qToBigEndian<quint32>(7, packet.data() + 4);
        qToBigEndian<quint32>(9, packet.data() + 8);
        QVERIFY(filter.admit(packet));
        QVERIFY(!filter.admit(packet));
        QVERIFY(filter.admit(QByteArray(4, 'x')));
    }

    // ── End to end ────────────────────────────────────────────────────

    // Over the real DTLS/SRTP session with real Opus: the window replaces
    // its media connection while listening. The session signs in and
    // snapshots nothing new, no new audio context restarts playback, the
    // copies are dropped before the jitter queue, and the tone heard has
    // no gap over 40 ms.
    void aReplacementWhileListeningHasNoGapAndNoRepeat()
    {
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QSignalSpy handshakes(&h.client, &StationClient::handshakeComplete);
        QSignalSpy authenticated(&h.server, &StationServer::clientAuthenticated);
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        h.connectSession();
        QVERIFY(h.client.capabilities().mediaReplaceVersion >= 1);
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        // A second of steady audio first.
        const int steadyFrom = h.remoteBus->heard.size() / 2;
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size() / 2 >= steadyFrom + 48000, 10000);

        const QString firstId = remoteMedia.mediaConnectionId();
        const int handshakesBefore = handshakes.size();
        const int authBefore = authenticated.size();
        const int contextsBefore = [&controls] {
            int n = 0;
            for (const QList<QVariant>& call : controls) {
                if (call.at(0).toJsonObject().value(QStringLiteral("op"))
                    == QLatin1String("audio-context")) {
                    ++n;
                }
            }
            return n;
        }();
        const RemoteAudioReceiverTelemetry before = remoteMedia.audioTelemetry();
        const int switchFrom = h.remoteBus->heard.size() / 2;
        QVERIFY(remoteMedia.replaceConnection());
        QVERIFY(remoteMedia.replacingConnection());
        QTRY_VERIFY_WITH_TIMEOUT(!remoteMedia.replacingConnection()
                                     && remoteMedia.mediaConnectionId() != firstId, 20000);
        // Past the old connection's drain, with audio still coming.
        const int doneAt = h.remoteBus->heard.size() / 2;
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size() / 2
                                     >= doneAt + 48 * (DaemonMediaController::kReplaceDrainMs
                                                       + 1000),
                                 15000);
        const RemoteAudioReceiverTelemetry after = remoteMedia.audioTelemetry();
        source.stop();
        speaker.stop();

        // Copies came on both connections and were dropped before the
        // queue: no duplicate and no late copy reached it.
        QVERIFY(remoteMedia.duplicateAudioDropped() > 0);
        QCOMPARE(after.duplicatePackets, before.duplicatePackets);
        QCOMPARE(after.latePackets, before.latePackets);
        // At most one interval (40 ms) concealed across the move.
        QVERIFY2(after.concealedPackets - before.concealedPackets <= 1,
                 qPrintable(QStringLiteral("concealed %1")
                                .arg(after.concealedPackets - before.concealedPackets)));
        // The tone heard has no silent run over 40 ms (4 blocks of 10 ms).
        const QVector<float>& heard = h.remoteBus->heard;
        int silentRun = 0;
        int longest = 0;
        for (int frame = switchFrom; (frame + 480) * 2 <= heard.size(); frame += 480) {
            double energy = 0.0;
            for (int i = 0; i < 480; ++i) {
                const double l = heard.at((frame + i) * 2);
                energy += l * l;
            }
            silentRun = energy / 480.0 < 1e-6 ? silentRun + 1 : 0;
            longest = std::max(longest, silentRun);
        }
        QVERIFY2(longest <= 4, qPrintable(QStringLiteral("silent for %1 ms").arg(longest * 10)));
        // No sign-in, no snapshot, no new audio context.
        QCOMPARE(handshakes.size(), handshakesBefore);
        QCOMPARE(authenticated.size(), authBefore);
        int contextsAfter = 0;
        for (const QList<QVariant>& call : controls) {
            if (call.at(0).toJsonObject().value(QStringLiteral("op"))
                == QLatin1String("audio-context")) {
                ++contextsAfter;
            }
        }
        QCOMPARE(contextsAfter, contextsBefore);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A delayed packet from the old connection after promotion must still
    // meet the overlap filter, even when its scheduling phase has ended.
    void lateOldRtpAfterPromotionNeverReachesTheJitterQueue()
    {
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        QTimer source;
        source.setInterval(10);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(media.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(media).constData(),
                                  h.kMediaConnectionWaitMs);
        MediaPeer* const old = media.findChild<MediaPeer*>();
        QVERIFY(old);
        QPointer<MediaPeer> retired(old);
        const quint32 oldSsrc = old->audioSsrc();
        QVERIFY(media.replaceConnection());
        MediaPeer* next = nullptr;
        for (MediaPeer* peer : media.findChildren<MediaPeer*>()) {
            if (peer != old) { next = peer; break; }
        }
        QVERIFY(next);
        QSignalSpy nextRtp(next, &MediaPeer::rtpReceived);
        QTRY_VERIFY_WITH_TIMEOUT(!media.replacingConnection(), 20000);
        nextRtp.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!nextRtp.isEmpty(), 5000);
        QByteArray late;
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            for (const auto& call : nextRtp) {
                const QByteArray packet = call.at(0).toByteArray();
                if (packet.size() >= 12 && ssrcOf(packet) == next->audioSsrc()) {
                    late = packet;
                    return true;
                }
            }
            return false;
        }(), 5000);
        QTest::qWait(120); // any held new-path packet has entered the filter
        QVERIFY(late.size() >= 12);
        qToBigEndian<quint32>(oldSsrc, late.data() + 8);
        const quint64 filteredBefore = media.duplicateAudioDropped();
        const quint64 jitterBefore = media.audioTelemetry().duplicatePackets;
        emit old->rtpReceived(late);
        QCOMPARE(media.duplicateAudioDropped(), filteredBefore + 1);
        QTest::qWait(80);
        QCOMPARE(media.audioTelemetry().duplicatePackets, jitterBefore);
        QTRY_VERIFY_WITH_TIMEOUT(retired.isNull(),
                                 DaemonMediaController::kReplaceDrainMs + 3000);
        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A second path move is remembered while the first old peer drains.
    // It must start only after that owner is released, then use the current
    // session path rather than the earlier request's ICE snapshot.
    void anotherMoveWaitsForRetirementAndReleasesTheOldPeer()
    {
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController media(&h.client, &h.remote, nullptr);
        DaemonMediaController daemon(&h.server, &h.station);
        QTimer source;
        source.setInterval(10);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(media.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(media).constData(),
                                  h.kMediaConnectionWaitMs);
        QPointer<MediaPeer> first(media.findChild<MediaPeer*>());
        QVERIFY(first);
        const QString firstId = media.mediaConnectionId();
        QVERIFY(media.replaceConnection());
        QTRY_VERIFY_WITH_TIMEOUT(!media.replacingConnection()
                                     && media.mediaConnectionId() != firstId, 20000);
        const QString secondId = media.mediaConnectionId();
        QVERIFY(first);
        emit h.client.pathChanged();
        QVERIFY(media.replacePending());
        QVERIFY(!media.replaceConnection());
        QVERIFY(!media.replacingConnection());
        QCOMPARE(media.findChildren<MediaPeer*>().size(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(first.isNull(),
                                 DaemonMediaController::kReplaceDrainMs + 3000);
        QTRY_VERIFY_WITH_TIMEOUT(media.replacingConnection(), 5000);
        // The replacement and current peer are the only GUI owners now.
        QCOMPARE(media.findChildren<MediaPeer*>().size(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!media.replacingConnection()
                                     && media.mediaConnectionId() != secondId, 20000);
        QVERIFY(!media.replacePending());
        // A fourth request during the next drain is discarded on stop.
        emit h.client.pathChanged();
        QVERIFY(media.replacePending());
        source.stop();
        speaker.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
        QVERIFY(!media.replacePending());
        QTest::qWait(DaemonMediaController::kReplaceDrainMs + 100);
        QVERIFY(media.mediaConnectionId().isEmpty());
        QVERIFY(!media.replacingConnection());
        QVERIFY(media.findChildren<MediaPeer*>().isEmpty());
    }

    // Task 29 fix wave (review Important 1): the session moves to a better
    // path right after it signs in, before its media connection is ready,
    // as the race's standby does at snapshot.complete. Media follows the
    // move by itself once it is ready (a replacement kept pending), while
    // listening, with no silent run over 40 ms and no repeat.
    void mediaFollowsAMoveThatCameBeforeItWasReady()
    {
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy moved(&h.client, &StationClient::pathChanged);
        QSignalSpy handshakes(&h.client, &StationClient::handshakeComplete);
        QTimer source;
        source.setInterval(10);
        source.setTimerType(Qt::PreciseTimer);
        connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        speaker.setTimerType(Qt::PreciseTimer);
        connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        // The better path, ready before the session signs in: a second
        // connection to the same Core with its hello read. The session
        // moves there the moment it signs in, before media is ready (the
        // media controller starts media from the same signal, first).
        auto* stationB = new Test::LoopbackTransport(QStringLiteral("station B"));
        auto* clientB = new Test::LoopbackTransport(QStringLiteral("client B"));
        stationB->linkTo(clientB);
        QList<QByteArray> onB;
        const QMetaObject::Connection helloSpy =
            connect(clientB, &SessionTransport::textReceived, clientB,
                    [&onB](const QByteArray& wire) { onB.append(wire); });
        h.server.acceptTransport(stationB);
        QTRY_VERIFY(!onB.isEmpty());
        disconnect(helloSpy);
        QString firstId;
        bool movedBeforeReady = false;
        connect(&h.client, &StationClient::handshakeComplete, &h.client, [&] {
            firstId = remoteMedia.mediaConnectionId();
            movedBeforeReady = h.client.moveSessionForTest(clientB, PathRacer::ThisNetwork);
        });
        connect(&h.client, &StationClient::pathChanged, &h.client, [&] {
            movedBeforeReady = movedBeforeReady
                && remoteMedia.audioStatus().state != RemoteAudioStatus::State::Playing;
        });
        h.connectSession();
        QVERIFY(h.client.capabilities().mediaReplaceVersion >= 1);
        QVERIFY(!firstId.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(moved.size(), 1, 10000);
        QVERIFY(movedBeforeReady);

        // Media follows: a new connection takes over once the first was
        // ready, and nothing stays pending.
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                      == RemoteAudioStatus::State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.mediaConnectionId() != firstId
                                     && !remoteMedia.replacingConnection(), 20000);
        QVERIFY(!remoteMedia.replacePending());
        // Past the old connection's drain, with audio still coming.
        const int doneAt = h.remoteBus->heard.size() / 2;
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size() / 2
                                     >= doneAt + 48 * (DaemonMediaController::kReplaceDrainMs
                                                       + 1000),
                                 15000);
        const RemoteAudioReceiverTelemetry after = remoteMedia.audioTelemetry();
        source.stop();
        speaker.stop();
        QVERIFY(remoteMedia.duplicateAudioDropped() > 0);
        QCOMPARE(after.duplicatePackets, quint64(0));
        // From the first tone heard on, no silent run over 40 ms.
        const QVector<float>& heard = h.remoteBus->heard;
        int first = -1;
        int silentRun = 0;
        int longest = 0;
        for (int frame = 0; (frame + 480) * 2 <= heard.size(); frame += 480) {
            double energy = 0.0;
            for (int i = 0; i < 480; ++i) {
                const double l = heard.at((frame + i) * 2);
                energy += l * l;
            }
            const bool silent = energy / 480.0 < 1e-6;
            if (first < 0) {
                if (!silent) {
                    first = frame;
                }
                continue;
            }
            silentRun = silent ? silentRun + 1 : 0;
            longest = std::max(longest, silentRun);
        }
        QVERIFY(first >= 0);
        QVERIFY2(longest <= 4, qPrintable(QStringLiteral("silent for %1 ms").arg(longest * 10)));
        // One sign-in, the session moved once.
        QCOMPARE(handshakes.size(), 1);
        QCOMPARE(h.client.pathSwitches(), 1);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }
};

QTEST_MAIN(TstMediaReplace)
#include "tst_media_replace.moc"
