// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_relay_session.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08; the rendezvous
// document, section 12; the options survey's section D, rung 3): a whole
// session over the web relay on this computer. A window reaches a Core
// through the local rendezvous service, which mints relay grants; a stand-in
// relay (below) pairs the two legs by their grant's session and forwards
// their frames as section 12.3 says. With every candidate but the web
// relay's refused (IceConfiguration's test seam, since host pairs always
// work on one computer) the session runs over it: ranked as the floor, the
// attempt record says so, and nothing the relay carries holds the control
// channel's JSON in the clear. Without the seam, a direct pair wins and the
// leg is only a low-priority candidate that ICE never chooses.
//
// The Docker traversal harness proves the same against the real relay
// behind TLS with only TCP 443 open (tests/scripts/traversal-harness.sh).
//
// Loopback only.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: every wait ends as soon as what it waits on closes or
//               fails, and says why (EndWatch, waitUntil). A QTRY_* wait
//               runs on for twice its timeout after it expires, so a session
//               the handshake deadline had closed held the test to QTest's
//               300 s function timeout with no reason given.
//   2026-10-15: the stand-in relay can hold its datagrams back for a
//               while (holdDatagramsMs), so in aDirectPairStillWins the
//               web relay's pair cannot succeed before the direct pair
//               and win libjuice's 2 s nomination timer on a loaded
//               runner. The test also checks that the shim's candidate
//               ranks below every direct one and that its checks reached
//               the relay, since the hold would hide either fault.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: a Core slow to see its own end open still gets the
//               session, over the web relay and over a direct pair. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: and with an older window, which leaves the service at its
//               own open; an answer kept past its introduction still goes
//               at the Core's answer deadline. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-15: this window's rows check that no introduction ends before
//               the session is up, so they guard the window's own fix; the
//               deadline is measured from the Core's answer. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: the slow Core over the web relay with the window racing
//               every path (PathRacer), as JJ's window dials. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: the slow Core over a direct pair races, as a real window
//               does, and runs both orders on every pass: direct first
//               (rank ServiceDirect at connect) and relay first (the web
//               relay at connect, moved to a direct pair within the first
//               step of the better-path search). ICE nominating the web
//               relay first on a loaded runner failed the old check.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: NSDC v2 interop (display codec v2, Task 6): the media
//               controllers' own connection over the web relay agrees
//               display codec v2 at its start, and the display frames it
//               delivers are version 2 (DisplayVersionTap). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSignalSpy>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>

#include "RendezvousTestHarness.h"
#include "core/AudioEngine.h"
#include "core/HpsdrModel.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/StationIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/IceConfiguration.h"
#include "core/session/PathRacer.h"
#include "core/session/RelayLeg.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/StationClient.h"
#include "core/session/StationRendezvous.h"
#include "core/session/StationServer.h"
#include "core/session/SwitchableTransport.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/DisplayVersionTap.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteMediaController.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using namespace NereusSDR::Test::Rendezvous;

namespace {

// A stand-in for the relay (rendezvous section 12): pairs legs by the
// session named in their token (section 12.2's payload, bytes 2 to 17),
// answers READY and PEER, forwards tagged datagrams to the other leg, and
// keeps every forwarded payload for the test to read.
class StandInRelay {
public:
    StandInRelay()
    {
        server = std::make_unique<QWebSocketServer>(QStringLiteral("relay"),
                                                    QWebSocketServer::NonSecureMode);
        server->listen(QHostAddress::LocalHost, 0);
        QObject::connect(server.get(), &QWebSocketServer::newConnection, server.get(), [this] {
            while (QWebSocket* socket = server->nextPendingConnection()) {
                QObject::connect(socket, &QWebSocket::binaryMessageReceived, socket,
                                 [this, socket](const QByteArray& message) {
                                     onMessage(socket, message);
                                 });
                QObject::connect(socket, &QWebSocket::disconnected, socket, [this, socket] {
                    for (auto it = legs.begin(); it != legs.end(); ++it) {
                        if (it.value() == socket) {
                            legs.erase(it);
                            break;
                        }
                    }
                    socket->deleteLater();
                });
            }
        });
    }

    QString url() const
    {
        return QStringLiteral("ws://127.0.0.1:%1/v1/relay").arg(server->serverPort());
    }

    // Leg keys: session bytes and the leg byte (1 Core, 2 device).
    QHash<QByteArray, QPointer<QWebSocket>> legs;
    QHash<QWebSocket*, QByteArray> keyOf;
    QList<QByteArray> forwarded[3];
    int joins = 0;
    std::unique_ptr<QWebSocketServer> server;
    // Off by default. When set, datagrams (control and media) are kept
    // this long from the first one, then forwarded in order.
    int holdDatagramsMs = 0;

private:
    QElapsedTimer m_holdClock;
    QList<QPair<QPointer<QWebSocket>, QByteArray>> m_held;

    void forward(QWebSocket* socket, const QByteArray& message)
    {
        const QPointer<QWebSocket> other = legs.value(otherKey(keyOf.value(socket)));
        if (other) {
            other->sendBinaryMessage(message);
        }
    }

    void releaseHeld()
    {
        const auto held = std::exchange(m_held, {});
        for (const auto& [socket, message] : held) {
            if (socket) {
                forward(socket, message);
            }
        }
    }

    static QByteArray otherKey(const QByteArray& key)
    {
        QByteArray other = key;
        other[other.size() - 1] = key.back() == 1 ? 2 : 1;
        return other;
    }

    void onMessage(QWebSocket* socket, const QByteArray& message)
    {
        if (message.isEmpty()) {
            return;
        }
        const auto tag = static_cast<quint8>(message.at(0));
        if (tag == RelayLeg::kTagJoin) {
            bool ok = false;
            const QByteArray token =
                StationIdentity::fromBase64Url(QString::fromLatin1(message.mid(1)), &ok);
            if (!ok || token.size() < 18) {
                socket->close();
                return;
            }
            const QByteArray key = token.mid(2, 16) + token.mid(1, 1);
            legs.insert(key, socket);
            keyOf.insert(socket, key);
            ++joins;
            const QPointer<QWebSocket> other = legs.value(otherKey(key));
            QByteArray ready("\x81\x01", 2);
            ready.append(other ? '\x01' : '\x00');
            socket->sendBinaryMessage(ready);
            if (other) {
                other->sendBinaryMessage(QByteArray("\x82\x01", 2));
            }
            return;
        }
        if (tag == RelayLeg::kTagControl || tag == RelayLeg::kTagMedia) {
            forwarded[tag].append(message.mid(1));
            if (holdDatagramsMs > 0) {
                if (!m_holdClock.isValid()) {
                    m_holdClock.start();
                }
                const qint64 left = holdDatagramsMs - m_holdClock.elapsed();
                if (left > 0 || !m_held.isEmpty()) {
                    if (m_held.isEmpty()) {
                        QTimer::singleShot(static_cast<int>(left), server.get(),
                                           [this] { releaseHeld(); });
                    }
                    m_held.append(qMakePair(QPointer<QWebSocket>(socket), message));
                    return;
                }
            }
            forward(socket, message);
        }
    }
};

// A Core registered with the local service, with the web relay allowed.
struct CoreThroughService {
    Core core;
    std::unique_ptr<StationRendezvous> rendezvous;
    QString failure;

    bool start(LocalService& service)
    {
        rendezvous = std::make_unique<StationRendezvous>(core.server.get(),
                                                         QList<QUrl>{service.url()},
                                                         /*relayAllowed=*/true);
        bool registered = false;
        QObject::connect(rendezvous->client(), &RendezvousClient::registered,
                         rendezvous->client(), [&registered] { registered = true; });
        EndWatch ends;
        ends.watch(rendezvous->client());
        if (!rendezvous->start()) {
            failure = QStringLiteral("the Core's rendezvous did not start");
            return false;
        }
        // The bound this fixture always had for registering.
        return waitUntil([&registered] { return registered; }, 10000, ends,
                         QStringLiteral("the Core to register with the service"), &failure);
    }
};

// Connects `window` to `station` through `service` and waits for the
// handshake, ending early if the session closes or fails. The watch is
// set before the connect call, so a failure inside it is seen too.
bool connectWindow(StationClient& window, LocalService& service, CoreThroughService& station,
                   QString* why, qint64* elapsedMs = nullptr)
{
    EndWatch ends;
    ends.watch(&window);
    window.connectThroughService({service.url()}, station.rendezvous->client()->stationId(),
                                 station.core.server->stationIdentity().fingerprint());
    return waitForHandshake(window, kServiceConnectBudgetMs, why, elapsedMs, &ends);
}

} // namespace

class TstRelaySession final : public QObject {
    Q_OBJECT

    // This window, and an older one that leaves the service at its own
    // open.
    static void windowRows()
    {
        QTest::addColumn<bool>("olderWindow");
        QTest::newRow("this window") << false;
        QTest::newRow("older window") << true;
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void cleanup()
    {
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(false);
        RelayLeg::setRelayUrlForTest(QUrl());
        DataChannelTransport::setAnswererOpenDelayForTest(0);
        RendezvousDialer::setLeavesServiceAtOpenForTest(false);
        DataChannelTransport::setDirectCandidateDelayForTest(0);
    }

    // Only the web relay works: the session runs over it, both ends' legs
    // carry the control connection's datagrams, and none holds its JSON in
    // the clear (DTLS end to end).
    void aSessionRunsOverTheWebRelay()
    {
        StandInRelay relay;
        QVERIFY(relay.server->isListening());
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QString why;
        QVERIFY2(connectWindow(window, service, station, &why), qPrintable(why));
        QCOMPARE(relay.joins, 2);
        QCOMPARE(window.pathRank(), int(PathRacer::Floor));
        QVERIFY2(window.connectionAttempt().summary().contains(QLatin1String("web relay")),
                 qPrintable(window.connectionAttempt().summary()));
        const auto* channel = qobject_cast<const DataChannelTransport*>(window.transport());
        QVERIFY(channel != nullptr);
        QVERIFY(channel->selectedPath().has_value() && channel->selectedPath()->viaLoopbackShim());
        // The control lane carried the whole sign-in; its JSON never
        // crossed the relay in the clear.
        QVERIFY(relay.forwarded[RelayLeg::kTagControl].size() > 10);
        for (const QByteArray& datagram : relay.forwarded[RelayLeg::kTagControl]) {
            QVERIFY(!datagram.contains("snapshot.complete"));
            QVERIFY(!datagram.contains("capabilities"));
            QVERIFY(!datagram.contains("\"type\""));
        }

        // Both agents on one leg: a media connection made with each end's
        // session settings (what the media controllers use) runs on the
        // media lane of the same two legs, and the relay never sees its
        // SRTP payload's marker in the clear.
        const std::optional<IceConfiguration> coreIce =
            station.core.server->sessionIceConfiguration(station.core.server->mediaSessionEpoch());
        const std::optional<IceConfiguration> windowIce = window.sessionIceConfiguration();
        QVERIFY(coreIce && coreIce->hasCandidateSourceFactory());
        QVERIFY(windowIce && windowIce->hasCandidateSourceFactory());
        LibDataChannelMediaTransport offerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
        LibDataChannelMediaTransport answerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
        QObject::connect(&offerer, &IMediaTransport::localDescription, &answerer,
                         [&answerer](const QString& sdp, const QString& type) {
                             answerer.acceptDescription(sdp, type);
                         });
        QObject::connect(&answerer, &IMediaTransport::localDescription, &offerer,
                         [&offerer](const QString& sdp, const QString& type) {
                             offerer.acceptDescription(sdp, type);
                         });
        QObject::connect(&offerer, &IMediaTransport::localCandidate, &answerer,
                         [&answerer](const QString& candidate, const QString& mid) {
                             answerer.acceptCandidate(candidate, mid);
                         });
        QObject::connect(&answerer, &IMediaTransport::localCandidate, &offerer,
                         [&offerer](const QString& candidate, const QString& mid) {
                             offerer.acceptCandidate(candidate, mid);
                         });
        QSignalSpy received(&answerer, &IMediaTransport::rtpReceived);
        EndWatch mediaEnds;
        mediaEnds.watch(&offerer, QStringLiteral("offering"));
        mediaEnds.watch(&answerer, QStringLiteral("answering"));
        mediaEnds.watch(&window);
        IMediaTransport::StartOptions core;
        core.role = IMediaTransport::Role::Offerer;
        core.localAudioSsrc = 0x11223344u;
        core.ice = coreIce;
        IMediaTransport::StartOptions device;
        device.role = IMediaTransport::Role::Answerer;
        device.localAudioSsrc = 0x55667788u;
        device.ice = windowIce;
        QVERIFY(answerer.start(device));
        QVERIFY(offerer.start(core));
        QVERIFY2(waitUntil([&] { return offerer.isReady() && answerer.isReady(); }, 30000,
                           mediaEnds, QStringLiteral("both media connections to be ready"), &why),
                 qPrintable(why));
        QVERIFY(offerer.selectedPath() && offerer.selectedPath()->viaLoopbackShim());
        QVERIFY(answerer.selectedPath() && answerer.selectedPath()->viaLoopbackShim());
        QByteArray rtp(12 + 60, '\0');
        rtp[0] = static_cast<char>(0x80);
        rtp[1] = static_cast<char>(111);
        qToBigEndian<quint32>(core.localAudioSsrc, rtp.data() + 8);
        rtp.replace(12, 16, QByteArrayLiteral("NEREUS-PLAINTEXT"));
        for (int i = 0; i < 20 && received.isEmpty() && !mediaEnds.ended(); ++i) {
            qToBigEndian<quint16>(static_cast<quint16>(i + 1), rtp.data() + 2);
            offerer.sendRtp(rtp);
            QTest::qWait(50);
        }
        // QTRY_VERIFY's default timeout, without its doubled overrun.
        QVERIFY2(waitUntil([&received] { return !received.isEmpty(); }, 5000, mediaEnds,
                           QStringLiteral("the media packet to arrive"), &why),
                 qPrintable(why));
        QVERIFY(received.first().first().toByteArray().contains("NEREUS-PLAINTEXT"));
        QVERIFY(!relay.forwarded[RelayLeg::kTagMedia].isEmpty());
        for (const QByteArray& datagram : relay.forwarded[RelayLeg::kTagMedia]) {
            QVERIFY(!datagram.contains("NEREUS-PLAINTEXT"));
        }
        QCOMPARE(relay.joins, 2); // one leg per end carried both
        offerer.stop();
        answerer.stop();
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // NSDC v2 interop (display codec v2, Task 6): with only the web relay
    // working, the media controllers' own connection runs on the relay's
    // media lane; its start agrees display codec v2, and every display
    // frame it delivers to the window is version 2.
    void relayedMediaAgreesDisplayCodecV2()
    {
        StandInRelay relay;
        QVERIFY(relay.server->isListening());
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        // A radio with one slice whose stream the test feeds, and media on.
        RadioModel& radio = *station.core.model;
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = radio.addSlice();
        QVERIFY(sliceId >= 0);
        const int stream = radio.sliceById(sliceId)->streamIndex();
        QVERIFY(stream >= 0);
        station.core.server->setMediaEnabled(true);
        DaemonMediaController daemonMedia(station.core.server.get(), &radio);
        QVERIFY2(station.start(service), qPrintable(station.failure));
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        remote.setConnectionStateForTest(ConnectionState::Connected);
        remote.audioEngine()->setMasterMuted(true); // Display fixture opens no speaker.
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        window.withholdFeatureForTest(QByteArrayLiteral("remoteTx")); // Display-only fixture.
        PanadapterStack stack;
        auto* pan = stack.addPanadapter(QStringLiteral("relay"));
        pan->setActiveSliceIndex(sliceId);
        pan->spectrumWidget()->setDisplayWindowPreservingHistory(
            radio.streamCentreHz(stream), 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        RemoteMediaController remoteMedia(&window, &remote, &stack);
        Test::DisplayVersionTap tap(&remoteMedia);
        QSignalSpy controls(station.core.server.get(), &StationServer::mediaControlReceived);
        QSignalSpy frames(&remoteMedia, &RemoteMediaController::displayFrameReceived);
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * static_cast<float>(std::cos(double(i) * 0.17));
            iq[i + 1] = 0.01f * static_cast<float>(std::sin(double(i) * 0.17));
        }
        QTimer display;
        display.setInterval(20);
        QObject::connect(&display, &QTimer::timeout, &display, [&radio, stream, &iq] {
            QMetaObject::invokeMethod(&radio, "rawIqDataForStream", Qt::DirectConnection,
                                      Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
        });
        display.start();

        QString why;
        QVERIFY2(connectWindow(window, service, station, &why), qPrintable(why));
        QCOMPARE(window.pathRank(), int(PathRacer::Floor));
        QCOMPARE(window.capabilities().displayCodecVersion, 2);
        EndWatch ends;
        ends.watch(&window);
        QVERIFY2(waitUntil([&] {
                     tap.follow();
                     return !frames.isEmpty() && tap.allVersions().size() >= 4;
                 }, 60000, ends, QStringLiteral("display frames over the web relay"), &why),
                 qPrintable(why));
        display.stop();
        // The media connection ran on the relay's media lane.
        QVERIFY(!relay.forwarded[RelayLeg::kTagMedia].isEmpty());
        int starts = 0;
        for (const QList<QVariant>& call : controls) {
            const QJsonObject control = call.at(0).toJsonObject();
            if (control.value(QStringLiteral("op")).toString() == QLatin1String("start")) {
                ++starts;
                QCOMPARE(control.value(QStringLiteral("displayCodecVersion")).toInt(), 2);
            }
        }
        QCOMPARE(starts, 1);
        QCOMPARE(daemonMedia.displayCodecVersion(), quint32{2});
        for (int version : tap.allVersions()) {
            QCOMPARE(version, 2);
        }
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // 2026-10-15: a Core that sees its own end open a second after the
    // window's (a busy Core, or one whose open came the long way round).
    // The window keeps its connection to the service until the Core speaks
    // on the new connection, so the Core's introduction does not end
    // (clientLeft) before its end opens, and the session comes up over the
    // web relay. Before, the window left the service at its own open and
    // the Core closed the connection it was about to accept. An older
    // window still leaves then; the Core keeps a connection under way.
    void aSlowCoreStillOpensOverTheWebRelay_data() { windowRows(); }
    void aSlowCoreStillOpensOverTheWebRelay()
    {
        QFETCH(bool, olderWindow);
        RendezvousDialer::setLeavesServiceAtOpenForTest(olderWindow);
        StandInRelay relay;
        QVERIFY(relay.server->isListening());
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);
        DataChannelTransport::setAnswererOpenDelayForTest(1000);

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QSignalSpy ended(station.rendezvous->client(), &RendezvousClient::introductionEnded);
        QString why;
        QVERIFY2(connectWindow(window, service, station, &why), qPrintable(why));
        QCOMPARE(window.pathRank(), int(PathRacer::Floor));
        if (!olderWindow) {
            // This window's own fix, with or without the Core's: it stays
            // with the service until the Core speaks, so no introduction
            // ends before the session is up.
            const QString code = ended.isEmpty() ? QString() : ended.first().at(1).toString();
            QVERIFY2(ended.isEmpty(),
                     qPrintable(QStringLiteral("the introduction ended early: %1").arg(code)));
        }
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // 2026-10-15: the same with the window racing every path, as a saved
    // Core is dialed (connectToStation with its identity key): the service's
    // web relay is the only path that works. PathRacer takes the connection
    // when it opens (and can hold it on standby); the service client the
    // dialer handed it still stays until the Core speaks, then leaves.
    void aSlowCoreStillOpensOverTheWebRelayWhenTheWindowRaces()
    {
        StandInRelay relay;
        QVERIFY(relay.server->isListening());
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);
        DataChannelTransport::setAnswererOpenDelayForTest(1000);

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        StationClient::ServiceRoute route;
        route.servers = {service.url()};
        route.rendezvousId = station.rendezvous->client()->stationId();
        route.relayAllowed = true;
        route.controlChannelVersion = 1;
        window.setServiceRoute(route);
        QSignalSpy ended(station.rendezvous->client(), &RendezvousClient::introductionEnded);
        QTest::ignoreMessage(QtInfoMsg,
                             "Leaving the remote access service: the Core answered on the new connection");
        EndWatch ends;
        ends.watch(&window);
        // A paired Core's identity key: the window races (link 21.1).
        window.connectToStation(QUrl(), QString(), QString(), false,
                                StationIdentity::fingerprintOf(
                                    station.core.server->stationIdentity().publicKeySpki()));
        QString why;
        QVERIFY2(waitForHandshake(window, kServiceConnectBudgetMs, &why, nullptr, &ends),
                 qPrintable(why));
        QCOMPARE(window.pathRank(), int(PathRacer::Floor));
        QVERIFY2(window.connectionAttempt().summary().contains(QLatin1String("web relay")),
                 qPrintable(window.connectionAttempt().summary()));
        {
            const QString code = ended.isEmpty() ? QString() : ended.first().at(1).toString();
            QVERIFY2(ended.isEmpty(),
                     qPrintable(QStringLiteral("the introduction ended early: %1").arg(code)));
        }
        // The Core's hello came on the raced connection, and the kept
        // service client left then (the expected line, checked when this
        // function ends); the session stays up.
        QTest::qWait(500);
        QVERIFY(window.isHandshakeComplete());
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // 2026-10-15: the same over a direct pair, every path open: the window
    // leaving the service at its own open ended every introduction the
    // service brokered, not only the relayed ones.
    // PR 616: the window races, as a real window dials a paired Core, and
    // both orders ICE can settle in are run on every pass. Direct first
    // (the web relay's datagrams held back): the session opens on the
    // direct pair. Relay first (the far end's signalled candidates held
    // back on each connection that has the web relay's leg): the session
    // opens on the web relay, and the window's better-path search moves it
    // to a direct pair at the first step of its schedule.
    void aSlowCoreStillOpensOverADirectPair_data()
    {
        QTest::addColumn<bool>("olderWindow");
        QTest::addColumn<bool>("relayFirst");
        QTest::newRow("this window, direct first") << false << false;
        QTest::newRow("this window, relay first") << false << true;
        QTest::newRow("older window, direct first") << true << false;
        QTest::newRow("older window, relay first") << true << true;
    }
    void aSlowCoreStillOpensOverADirectPair()
    {
        QFETCH(bool, olderWindow);
        QFETCH(bool, relayFirst);
        // Direct first: as aDirectPairStillWins, the relay's pair cannot
        // succeed before the direct one. Relay first: the direct pairs are
        // not formed until the web relay's pair has been nominated.
        constexpr int kRelayHoldMs = 3000;
        constexpr int kDirectDelayMs = 3000;
        // The first step of the better-path search, then a whole new
        // connection through the service, then the move.
        constexpr int kUpgradeWaitMs = PathRacer::kUpgradeRetryMs.front() + kDirectDelayMs
            + kServiceConnectBudgetMs + SwitchableTransport::kSwitchDeadlineMs;
        RendezvousDialer::setLeavesServiceAtOpenForTest(olderWindow);
        StandInRelay relay;
        QVERIFY(relay.server->isListening());
        if (!relayFirst) {
            relay.holdDatagramsMs = kRelayHoldMs;
        }
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));
        DataChannelTransport::setAnswererOpenDelayForTest(1000);
        if (relayFirst) {
            DataChannelTransport::setDirectCandidateDelayForTest(kDirectDelayMs);
        }

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        StationClient::ServiceRoute route;
        route.servers = {service.url()};
        route.rendezvousId = station.rendezvous->client()->stationId();
        route.relayAllowed = true;
        route.controlChannelVersion = 1;
        window.setServiceRoute(route);
        QSignalSpy ended(station.rendezvous->client(), &RendezvousClient::introductionEnded);
        EndWatch ends;
        ends.watch(&window);
        // A paired Core's identity key: the window races (link 21.1).
        window.connectToStation(QUrl(), QString(), QString(), false,
                                StationIdentity::fingerprintOf(
                                    station.core.server->stationIdentity().publicKeySpki()));
        QString why;
        QVERIFY2(waitForHandshake(window, kServiceConnectBudgetMs, &why, nullptr, &ends),
                 qPrintable(why));
        QCOMPARE(window.pathRank(), int(relayFirst ? PathRacer::Floor : PathRacer::ServiceDirect));
        if (!olderWindow) {
            // This window's own fix, with or without the Core's: it stays
            // with the service until the Core speaks, so no introduction
            // ends before the session is up.
            const QString code = ended.isEmpty() ? QString() : ended.first().at(1).toString();
            QVERIFY2(ended.isEmpty(),
                     qPrintable(QStringLiteral("the introduction ended early: %1").arg(code)));
        }
        if (relayFirst) {
            // waitUntil, not QTRY_*: it ends at its bound (or the session's
            // end) and says why, where a QTRY_* wait runs on for twice its
            // timeout, past QTest's function timeout at this bound.
            QVERIFY2(waitUntil([&window] { return window.pathRank() == PathRacer::ServiceDirect; },
                               kUpgradeWaitMs, ends,
                               QStringLiteral("the session to move to a direct pair"), &why),
                     qPrintable(QStringLiteral("%1 (rank %2)").arg(why).arg(window.pathRank())));
            QVERIFY(window.isHandshakeComplete());
        }
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // 2026-10-15: an answer the Core keeps past its introduction (an older
    // window left the service at its own open) and that never opens still
    // goes at the Core's answer deadline, not before and not never.
    void anAnswerKeptPastItsIntroductionGoesAtTheDeadline()
    {
        StandInRelay relay;
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));
        constexpr int kDeadlineMs = 3000;
        station.rendezvous->setAnswerDeadlineMs(kDeadlineMs);
        RendezvousDialer::setLeavesServiceAtOpenForTest(true);
        // Never within the test: the Core's end does not open.
        DataChannelTransport::setAnswererOpenDelayForTest(600000);

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QSignalSpy ended(station.rendezvous->client(), &RendezvousClient::introductionEnded);
        // From the Core's answer: the introduction reaches the Core's own
        // handler (which starts the deadline) first, then this one.
        QElapsedTimer sinceAnswer;
        QObject::connect(station.rendezvous->client(), &RendezvousClient::introduced,
                         station.rendezvous->client(), [&sinceAnswer] {
                             if (!sinceAnswer.isValid()) {
                                 sinceAnswer.start();
                             }
                         });
        window.connectThroughService({service.url()}, station.rendezvous->client()->stationId(),
                                     station.core.server->stationIdentity().fingerprint());
        EndWatch ends;
        QString why;
        QVERIFY2(waitUntil([&ended] { return !ended.isEmpty(); }, 15000, ends,
                           QStringLiteral("the introduction to end (clientLeft)"), &why),
                 qPrintable(why));
        QCOMPARE(ended.first().at(1).toString(), QStringLiteral("clientLeft"));
        // Kept: its connection was under way.
        QCOMPARE(station.rendezvous->pendingAnswers(), 1);
        QVERIFY2(waitUntil([&station] { return station.rendezvous->pendingAnswers() == 0; },
                           kDeadlineMs + 10000, ends,
                           QStringLiteral("the kept answer to go"), &why),
                 qPrintable(why));
        // The deadline is a coarse timer, which may fire up to 5% early
        // (Qt::CoarseTimer).
        QVERIFY(sinceAnswer.isValid());
        QVERIFY2(sinceAnswer.elapsed() >= kDeadlineMs * 95 / 100,
                 qPrintable(QStringLiteral("went %1 ms after the answer")
                                .arg(sinceAnswer.elapsed())));
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // Every path open: the direct pair wins; the web relay is offered as
    // the lowest-priority candidate and never chosen.
    void aDirectPairStillWins()
    {
        StandInRelay relay;
        // The web relay's shim is a host candidate on the same libjuice
        // agent. The controlling side nominates its best succeeded pair
        // 2 s after selecting it (NOMINATION_TIMEOUT), so a loaded runner
        // whose direct pair is slower than that ends on the relay. Holding
        // the relay's datagrams 3 s moves that window rather than closing
        // it: the direct remote candidates get the hold time to arrive and
        // succeed. Once the shim's pair succeeds after the release, libjuice
        // nominates it at once if no higher pair is pending, so a direct
        // path slower than the hold still loses. That is libjuice working
        // as designed, not a product fault this test covers.
        relay.holdDatagramsMs = 3000;

        // With the hold, the direct pair would win even if the shim's
        // candidate outranked it, so the priority is checked here. The
        // line the agent is given carries kCandidatePriority, and an agent's
        // own candidate is never below 255 (RFC 8445 5.1.2.1: type and
        // local preference 0, component 1 gives 256 - 1), so the shim must
        // stay under that to rank below every direct candidate.
        constexpr quint32 kLowestAgentPriority = 255;
        QCOMPARE(RelayLeg::kCandidatePriority, 1u);
        const QStringList shimLine =
            RelayLeg::candidateLine(IceConfiguration::kControlLane, 40000).split(QLatin1Char(' '));
        QVERIFY(shimLine.size() > 3);
        bool priorityOk = false;
        const quint32 shimPriority = shimLine.at(3).toUInt(&priorityOk);
        QVERIFY(priorityOk);
        QCOMPARE(shimPriority, RelayLeg::kCandidatePriority);
        QVERIFY2(shimPriority < kLowestAgentPriority,
                 qPrintable(QStringLiteral("shim priority %1").arg(shimPriority)));
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QString why;
        QVERIFY2(connectWindow(window, service, station, &why), qPrintable(why));
        QCOMPARE(window.pathRank(), int(PathRacer::ServiceDirect));
        const auto* channel = qobject_cast<const DataChannelTransport*>(window.transport());
        QVERIFY(channel != nullptr && channel->selectedPath().has_value());
        QVERIFY(!channel->selectedPath()->viaLoopbackShim());
        // Both legs joined at the introduction all the same.
        EndWatch ends;
        ends.watch(&window);
        QVERIFY2(waitUntil([&relay] { return relay.joins == 2; }, 5000, ends,
                           QStringLiteral("both relay legs to join"), &why),
                 qPrintable(QStringLiteral("%1 (joins: %2)").arg(why).arg(relay.joins)));
        // The shim's pair was offered and checked: its connectivity checks
        // reached the relay on the control lane.
        QVERIFY2(waitUntil([&relay] { return !relay.forwarded[RelayLeg::kTagControl].isEmpty(); },
                           5000, ends, QStringLiteral("the shim's checks to reach the relay"),
                           &why),
                 qPrintable(why));
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // The waits above end when the session does: a session the handshake
    // deadline closes (the close seen under load) ends the handshake wait
    // at once with that reason, not at the connect budget.
    void aClosedSessionEndsTheWaitWithItsReason()
    {
        StandInRelay relay;
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        // Forces the close: no sign-in finishes in a millisecond.
        window.setHandshakeDeadlineMs(1);
        QString why;
        qint64 elapsedMs = -1;
        QVERIFY(!connectWindow(window, service, station, &why, &elapsedMs));
        QVERIFY2(why.contains(StationClient::handshakeDeadlineReason()), qPrintable(why));
        QVERIFY2(elapsedMs < kServiceConnectBudgetMs,
                 qPrintable(QStringLiteral("ended after %1 ms: %2").arg(elapsedMs).arg(why)));
        window.disconnectFromStation(QStringLiteral("test done"));
    }
};

QTEST_MAIN(TstRelaySession)
#include "tst_relay_session.moc"
