// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_media_tunnel.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16; the options survey's B.3; link
// section 21, "The media tunnel"): media inside a direct WebSocket session.
// A real Core and a real window joined over an in-process link, with real
// DTLS/SRTP media: when no UDP pair can work (IceConfiguration's test seam
// refuses every candidate but a loopback shim's, since on one computer
// host pairs always work) the media connection runs through the tunnel,
// its datagrams binary messages on the session's link, and audio plays;
// when UDP works, a host pair wins and the tunnel carries nothing. The
// window declares the tunnel only to a Core that carries it.
//
// The Docker traversal harness proves the same with UDP blocked between a
// device and a Core it reaches by its forwarded wss port.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: load finding: the tunnel-only replacement case waits the
//               product's ICE connect bound (IceConfiguration::
//               kConnectDeadlineMs) for both pairs and on failure prints
//               what each end got to. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: direct media follow-up: the tunnel-only transport's
//               refusal checked with a remote description set, and a real
//               tunnel-only replacement carrying media after the direct
//               path it replaces is cut. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: direct media fix wave: the stall test's bound anchored on
//               the last message before the silence, without slack; an
//               ordering barrier in place of a fixed wait. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: STUN in the first and direct-only
//               configurations, media moving from the tunnel to a direct
//               path, and one stall tick of slack in the stall test's lower
//               bound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QUuid>
#include <QWebSocket>
#include <QWebSocketServer>

#include <algorithm>
#include <utility>
#include <thread>

#include "RealtimeTestLoad.h"
#include "core/session/IceConfiguration.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "fakes/LoopbackTransport.h"
#include "core/session/MediaTunnel.h"
#include "core/session/PathRacer.h"
#include "core/session/CandidateSourceLease.h"
#include "core/session/SessionTransport.h"
#include "core/session/SwitchableTransport.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/RemoteConnectionController.h"
#include "gui/RemoteMediaController.h"

using namespace NereusSDR;

namespace {

// Counts the binary messages (tunnel datagrams) a link end sends.
struct BinaryCount {
    int messages = 0;
    QMetaObject::Connection watch;
    explicit BinaryCount(SessionTransport* link)
    {
        watch = QObject::connect(link, &SessionTransport::binaryReceived, link,
                                 [this](const QByteArray&) { ++messages; });
    }
    ~BinaryCount() { QObject::disconnect(watch); }
};


// An RTP packet under `ssrc` (payload type 111, as the transport's audio).
QByteArray tunnelRtpPacket(quint16 sequence, quint32 ssrc)
{
    QByteArray packet(15, char(0xa5));
    packet[0] = char(0x80);
    packet[1] = char(111);
    packet[2] = char(sequence >> 8);
    packet[3] = char(sequence & 0xff);
    packet[8] = char(ssrc >> 24);
    packet[9] = char(ssrc >> 16);
    packet[10] = char(ssrc >> 8);
    packet[11] = char(ssrc);
    return packet;
}

// A direct UDP path the test can cut. Each end's first IPv4 host candidate
// is rewritten to one of two loopback ports, so each end knows the other
// only through the forwarder: datagrams on the Core-facing port go on to
// the window, those on the window-facing port go on to the Core. cut()
// drops everything from then on.
class DirectPathForwarder final {
public:
    DirectPathForwarder() = default;
    DirectPathForwarder(const DirectPathForwarder&) = delete;
    DirectPathForwarder& operator=(const DirectPathForwarder&) = delete;

    bool bind()
    {
        if (!m_coreFacing.bind(QHostAddress::LocalHost, 0)
            || !m_windowFacing.bind(QHostAddress::LocalHost, 0)) {
            return false;
        }
        QObject::connect(&m_coreFacing, &QUdpSocket::readyRead, &m_coreFacing, [this] {
            pump(m_coreFacing, m_windowFacing, m_core, m_window);
        });
        QObject::connect(&m_windowFacing, &QUdpSocket::readyRead, &m_windowFacing, [this] {
            pump(m_windowFacing, m_coreFacing, m_window, m_core);
        });
        return true;
    }

    quint16 windowFacingPort() const { return m_windowFacing.localPort(); }
    void cut() { m_cut = true; }
    quint64 dropped() const { return m_dropped; }

    // The Core's candidate as the window is to see it (empty: not sent).
    QString coreCandidate(const QString& candidate)
    {
        return rewrite(candidate, m_core, m_windowFacing.localPort());
    }
    // The window's candidate as the Core is to see it (empty: not sent).
    QString windowCandidate(const QString& candidate)
    {
        return rewrite(candidate, m_window, m_coreFacing.localPort());
    }

private:
    struct End {
        bool advertised = false;
        quint16 port = 0;
        QHostAddress address;
    };

    static QString rewrite(const QString& candidate, End& end, quint16 through)
    {
        if (end.advertised) { return QString(); }
        const QStringList fields = candidate.split(QLatin1Char(' '));
        if (fields.size() < 8 || fields.at(7) != QLatin1String("host")) { return QString(); }
        bool ipv4 = false;
        QHostAddress(fields.at(4)).toIPv4Address(&ipv4);
        if (!ipv4) { return QString(); }
        end.advertised = true;
        if (end.port == 0) {
            end.port = fields.at(5).toUShort();
            end.address = QHostAddress(QHostAddress::LocalHost);
        }
        return QStringLiteral("candidate:1 1 UDP 2122317823 127.0.0.1 %1 typ host").arg(through);
    }

    void pump(QUdpSocket& in, QUdpSocket& out, End& from, const End& to)
    {
        while (in.hasPendingDatagrams()) {
            const QNetworkDatagram datagram = in.receiveDatagram();
            if (m_cut || to.port == 0) {
                ++m_dropped;
                continue;
            }
            // Replies go back to where the end actually sends from.
            from.address = datagram.senderAddress();
            from.port = quint16(datagram.senderPort());
            out.writeDatagram(datagram.data(), to.address, to.port);
        }
    }

    QUdpSocket m_coreFacing;
    QUdpSocket m_windowFacing;
    End m_core;
    End m_window;
    bool m_cut = false;
    quint64 m_dropped = 0;
};

// LINK-I3: a control link whose socket stays full, as during a stall:
// it carries binary and reports a backlog over the tunnel's write limit.
class StalledLink final : public SessionTransport {
public:
    void sendText(const QByteArray&) override {}
    void ping() override {}
    void closeLink(const QString&) override {}
    bool isOpen() const override { return true; }
    QString peerDescription() const override { return QStringLiteral("stalled"); }
    bool sendBinary(const QByteArray&) override { return false; }
    bool carriesBinary() const override { return true; }
    qint64 backlogBytes() const override { return MediaTunnel::kWriteLimitBytes; }
};
} // namespace

class TstMediaTunnel final : public QObject {
    Q_OBJECT

private slots:
    void directWebSocketTunnelReportsItsCurrentControlSocket()
    {
        QWebSocketServer server(QStringLiteral("local tunnel"), QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        auto* clientSocket = new QWebSocket;
        WebSocketTransport control(clientSocket, 4096);
        auto tunnel = MediaTunnel::create(&control);
        QVERIFY(tunnel);
        IceConfiguration ice = MediaTunnel::iceFor(tunnel, std::nullopt);
        const QString id = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        auto source = ice.makeCandidateSource(IceConfiguration::kMediaLane, id);
        QVERIFY(source);
        source->start([](const QString&) {});
        QVERIFY(!source->networkPathSnapshot());
        clientSocket->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort())));
        QTRY_VERIFY(clientSocket->state() == QAbstractSocket::ConnectedState);
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QWebSocket> accepted(server.nextPendingConnection());
        const auto path = source->networkPathSnapshot();
        QVERIFY(path);
        QCOMPARE(path->kind, NetworkPathSnapshot::Kind::Direct);
        QCOMPARE(path->carrier, NetworkPathSnapshot::Carrier::WebSocket);
        QCOMPARE(path->endpoints, NetworkPathSnapshot::Endpoints::Socket);
        QVERIFY(path->mediaRidesControl);
        QCOMPARE(path->remoteAddress, QStringLiteral("127.0.0.1"));
        QCOMPARE(path->remotePort, server.serverPort());
        QVERIFY(path->localPort != 0);
        control.closeLink(QStringLiteral("test done"));
        QVERIFY(!source->networkPathSnapshot());
        source->stop();
    }

    // LINK-I3: while the control link is stalled, every datagram that
    // arrives flushes and finds the link full. One retry timer runs, not one
    // chain per datagram: over a window the tunnel makes at most one pass
    // per retry interval, plus the passes the datagrams themselves make.
    void aStalledLinkIsRetriedByOneTimer()
    {
        StalledLink link;
        auto tunnel = MediaTunnel::create(&link);
        QVERIFY(tunnel);
        IceConfiguration ice = MediaTunnel::iceFor(tunnel, std::nullopt);
        auto source = ice.makeCandidateSource(
            IceConfiguration::kMediaLane, QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"));
        QVERIFY(source);
        quint16 port = 0;
        source->start([&](const QString& candidate) {
            port = candidate.split(' ').at(5).toUShort();
        });
        QVERIFY(port != 0);
        QUdpSocket agent;
        QVERIFY(agent.bind(QHostAddress::LocalHost, 0));
        constexpr int kDatagrams = 40;
        for (int i = 0; i < kDatagrams; ++i) {
            agent.writeDatagram(QByteArray("media"), QHostAddress::LocalHost, port);
            // Each datagram its own read, so each one flushes.
            const quint64 before = tunnel->flushPassesForTest();
            QTRY_VERIFY(tunnel->flushPassesForTest() > before);
        }
        const quint64 start = tunnel->flushPassesForTest();
        QElapsedTimer window;
        window.start();
        QTest::qWait(200);
        const qint64 elapsedMs = window.elapsed();
        const quint64 passes = tunnel->flushPassesForTest() - start;
        // One 5 ms retry chain: at most elapsed / 5 passes, plus the one
        // already armed when the window opened.
        QVERIFY2(passes <= quint64(elapsedMs / 5 + 1),
                 qPrintable(QStringLiteral("%1 flush passes in %2 ms").arg(passes).arg(elapsedMs)));
        QVERIFY(passes >= 1);  // the retry still runs while data waits
        source->stop();
    }

    void tunnelRoutesConcurrentGenerationsAndRejectsOversize()
    {
        Test::LoopbackTransport local(QStringLiteral("local"));
        Test::LoopbackTransport remote(QStringLiteral("remote"));
        local.linkTo(&remote);
        auto tunnel = MediaTunnel::create(&local);
        QVERIFY(tunnel);
        IceConfiguration ice = MediaTunnel::iceFor(tunnel, std::nullopt);
        QVERIFY(!ice.makeCandidateSource(IceConfiguration::kMediaLane,
                                         QStringLiteral("00000000-0000-0000-0000-000000000000")));
        QVERIFY(!ice.makeCandidateSource(IceConfiguration::kMediaLane,
                                         QStringLiteral("AAAAAAAA-AAAA-4AAA-8AAA-AAAAAAAAAAAA")));
        const QString a = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        const QString b = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
        auto firstLease = CandidateSourceLease::create(ice);
        std::shared_ptr<void> delayedIceLifetime = firstLease;
        auto second = ice.makeCandidateSource(IceConfiguration::kMediaLane, b);
        QVERIFY(second);
        quint16 firstPort = 0;
        quint16 secondPort = 0;
        firstLease->start(IceConfiguration::kMediaLane, a, [&](const QString& candidate) {
            firstPort = candidate.split(' ').at(5).toUShort();
        });
        second->start([&](const QString& candidate) {
            secondPort = candidate.split(' ').at(5).toUShort();
        });
        QVERIFY(firstPort != 0 && secondPort != 0 && firstPort != secondPort);
        QUdpSocket firstAgent;
        QUdpSocket secondAgent;
        QVERIFY(firstAgent.bind(QHostAddress::LocalHost, 0));
        QVERIFY(secondAgent.bind(QHostAddress::LocalHost, 0));
        QSignalSpy outgoing(&remote, &SessionTransport::binaryReceived);
        firstAgent.writeDatagram("one", QHostAddress::LocalHost, firstPort);
        secondAgent.writeDatagram("two", QHostAddress::LocalHost, secondPort);
        QTRY_VERIFY(outgoing.size() >= 2);
        const QByteArray firstUuid = QUuid::fromString(a).toRfc4122();
        const QByteArray secondUuid = QUuid::fromString(b).toRfc4122();
        const QByteArray one = QByteArray(1, '\x02') + firstUuid + "one";
        const QByteArray two = QByteArray(1, '\x02') + secondUuid + "two";
        QVERIFY(outgoing.at(0).at(0).toByteArray() == one
                || outgoing.at(1).at(0).toByteArray() == one);
        QVERIFY(outgoing.at(0).at(0).toByteArray() == two
                || outgoing.at(1).at(0).toByteArray() == two);
        QUdpSocket otherSender;
        QVERIFY(otherSender.bind(QHostAddress::LocalHost, 0));
        otherSender.writeDatagram("wrong-one", QHostAddress::LocalHost, firstPort);
        otherSender.writeDatagram("wrong-two", QHostAddress::LocalHost, secondPort);
        QTRY_COMPARE(tunnel->droppedWrongSender(), quint64(2));
        QCOMPARE(outgoing.size(), 2); // neither generation retargeted
        firstAgent.writeDatagram(QByteArray(1484, 'x'), QHostAddress::LocalHost, firstPort);
        QTRY_VERIFY(outgoing.size() >= 3);
        QCOMPARE(outgoing.last().at(0).toByteArray().size(), 1501);
        firstAgent.writeDatagram(QByteArray(1485, 'x'), QHostAddress::LocalHost, firstPort);
        QTRY_COMPARE(tunnel->droppedOversize(), quint64(1));
        remote.sendBinary(QByteArray(1, '\x02') + firstUuid + QByteArray(1485, 'x'));
        QTRY_COMPARE(tunnel->droppedOversize(), quint64(2));
        remote.sendBinary(QByteArray(1, '\x02') + firstUuid + "to-one");
        remote.sendBinary(QByteArray(1, '\x02') + secondUuid + "to-two");
        QTRY_VERIFY(firstAgent.hasPendingDatagrams());
        QTRY_VERIFY(secondAgent.hasPendingDatagrams());
        QCOMPARE(firstAgent.receiveDatagram().data(), QByteArray("to-one"));
        QCOMPARE(secondAgent.receiveDatagram().data(), QByteArray("to-two"));
        QVERIFY(!otherSender.hasPendingDatagrams());
        // The wrapper can disappear while ICE still holds this lease. Even
        // after the old fixed hold interval, the old socket must remain
        // bound and must never become another generation's route.
        firstLease.reset();
        QTest::qWait(2100);
        remote.sendBinary(QByteArray(1, '\x02') + firstUuid + "still-old");
        remote.sendBinary(QByteArray(1, '\x02') + secondUuid + "live");
        remote.sendBinary(QByteArray(1, '\x02')); // malformed routed frame
        QTRY_VERIFY(firstAgent.hasPendingDatagrams());
        QCOMPARE(firstAgent.receiveDatagram().data(), QByteArray("still-old"));
        QTRY_VERIFY(secondAgent.hasPendingDatagrams());
        QCOMPARE(secondAgent.receiveDatagram().data(), QByteArray("live"));
        QTRY_COMPARE(tunnel->droppedNoRoute(), quint64(1));
        auto third = ice.makeCandidateSource(
            IceConfiguration::kMediaLane,
            QStringLiteral("cccccccc-cccc-4ccc-8ccc-cccccccccccc"));
        auto fourth = ice.makeCandidateSource(
            IceConfiguration::kMediaLane,
            QStringLiteral("dddddddd-dddd-4ddd-8ddd-dddddddddddd"));
        QVERIFY(third && fourth);
        quint16 thirdPort = 0;
        third->start([&](const QString& candidate) {
            thirdPort = candidate.split(' ').at(5).toUShort();
        });
        QVERIFY(thirdPort != 0 && thirdPort != firstPort);
        bool fourthOffered = false;
        fourth->start([&](const QString&) { fourthOffered = true; });
        QVERIFY(!fourthOffered); // current, new and retiring exhaust the bound
        // libdatachannel drops this last holder on its teardown worker.
        // The lease must post socket release back to the Qt thread.
        std::thread teardown([held = std::move(delayedIceLifetime)]() mutable {
            held.reset();
        });
        teardown.join();
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            fourth->start([&](const QString&) { fourthOffered = true; });
            return fourthOffered;
        }(), 3000);
        remote.sendBinary(QByteArray(1, '\x02') + firstUuid + "retired");
        QTRY_COMPARE(tunnel->droppedNoRoute(), quint64(2));
        third->stop();
        fourth->stop();
        second->stop();
    }

    // The direct media ladder (rendezvous section "Direct media"): the
    // first media connection gathers from the Core's STUN server as well as
    // its host addresses, with the tunnel as its lowest-priority candidate
    // and no relay.
    void theFirstMediaConfigurationHasStunAndTheTunnel()
    {
        Test::LoopbackTransport local(QStringLiteral("local"));
        Test::LoopbackTransport remote(QStringLiteral("remote"));
        local.linkTo(&remote);
        auto tunnel = MediaTunnel::create(&local);
        QVERIFY(tunnel);
        const IceServerAddress stun{QStringLiteral("stun.example.test"), 3478};
        const IceConfiguration ice = MediaTunnel::iceFor(tunnel, stun);
        QCOMPARE(ice.stunServer(), std::optional<IceServerAddress>(stun));
        QVERIFY(ice.hasCandidateSourceFactory());
        QVERIFY(ice.makeCandidateSource(IceConfiguration::kMediaLane,
                                        QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")));
        QVERIFY(ice.mediaRouting());
        QVERIFY(!ice.relayAllowed());
        QVERIFY(ice.relayKnown());
        QVERIFY(ice.relayServers().isEmpty());
        // With no STUN server known, as before: host candidates and the
        // tunnel.
        const IceConfiguration bare = MediaTunnel::iceFor(tunnel, std::nullopt);
        QVERIFY(!bare.stunServer());
        QVERIFY(bare.hasCandidateSourceFactory());
    }

    // The silence fallback's configuration: the tunnel's candidate alone.
    // The transport offers the far end no candidate of its own and takes
    // none the far end signals, so ICE can only nominate the tunnel. The
    // refusal is checked with the far end's description in place, where a
    // transport without the rule takes the same candidates.
    void theTunnelOnlyTransportTakesNoOtherCandidate()
    {
        Test::LoopbackTransport local(QStringLiteral("local"));
        Test::LoopbackTransport remote(QStringLiteral("remote"));
        local.linkTo(&remote);
        auto tunnel = MediaTunnel::create(&local);
        QVERIFY(tunnel);
        const IceConfiguration ice = MediaTunnel::tunnelIceFor(tunnel);
        QVERIFY(ice.onlySourceCandidates());
        QVERIFY(!ice.stunServer());
        QVERIFY(!ice.relayAllowed());
        QVERIFY(ice.hasCandidateSourceFactory());
        QVERIFY(!MediaTunnel::iceFor(tunnel, std::nullopt).onlySourceCandidates());

        LibDataChannelMediaTransport transport;
        LibDataChannelMediaTransport farEnd;
        QSignalSpy candidates(&transport, &IMediaTransport::localCandidate);
        QSignalSpy complete(&transport, &IMediaTransport::gatheringComplete);
        QSignalSpy errors(&transport, &IMediaTransport::errorOccurred);
        bool offerTaken = false;
        bool answerTaken = false;
        QList<QPair<QString, QString>> farCandidates;
        QObject::connect(&transport, &IMediaTransport::localDescription, &farEnd,
                         [&farEnd, &offerTaken](const QString& sdp, const QString& type) {
                             offerTaken = farEnd.acceptDescription(sdp, type);
                         });
        QObject::connect(&farEnd, &IMediaTransport::localDescription, &transport,
                         [&transport, &answerTaken](const QString& sdp, const QString& type) {
                             answerTaken = transport.acceptDescription(sdp, type);
                         });
        QObject::connect(&farEnd, &IMediaTransport::localCandidate, &transport,
                         [&farCandidates](const QString& candidate, const QString& mid) {
                             farCandidates.append(qMakePair(candidate, mid));
                         });
        QVERIFY(farEnd.start({IMediaTransport::Role::Answerer, 0x5678}));
        IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, 0x1234};
        options.connectionId = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        options.ice = ice;
        QVERIFY(transport.start(options));
        QTRY_VERIFY_WITH_TIMEOUT(complete.size() == 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(offerTaken && answerTaken && !farCandidates.isEmpty(), 10000);
        QCOMPARE(candidates.size(), 0);
        // The far end's own host candidates, and a loopback one, with the
        // answer in place: every one refused by the rule itself, not by an
        // error from a transport that could not take a candidate yet.
        for (const auto& far : std::as_const(farCandidates)) {
            QVERIFY2(!transport.acceptCandidate(far.first, far.second), qPrintable(far.first));
        }
        QVERIFY(!transport.acceptCandidate(
            QStringLiteral("candidate:1 1 UDP 2122317823 127.0.0.1 50123 typ host"),
            farCandidates.first().second));
        QCOMPARE(errors.size(), 0);
        transport.stop();
        farEnd.stop();
    }

    // The silence fallback end to end on real transports: media on a
    // direct UDP pair, the pair cut so nothing more crosses it, then a
    // tunnel-only connection on the same session link reaches connected
    // and carries a media packet. The window answers, as in production.
    void aTunnelOnlyReplacementCarriesMediaAfterTheDirectPathIsCut()
    {
        // The direct path: each end knows the other only as one of the
        // forwarder's two loopback ports, so every datagram of the pair
        // crosses the forwarder and cutting it cuts the pair.
        DirectPathForwarder forwarder;
        QVERIFY(forwarder.bind());
        constexpr quint32 kCoreSsrc = 0x4e523401U;
        constexpr quint32 kWindowSsrc = 0x4e523402U;
        LibDataChannelMediaTransport directCore;
        LibDataChannelMediaTransport directWindow;
        QObject::connect(&directCore, &IMediaTransport::localDescription, &directWindow,
                         [&directWindow](const QString& sdp, const QString& type) {
                             QVERIFY(directWindow.acceptDescription(sdp, type));
                         });
        QObject::connect(&directWindow, &IMediaTransport::localDescription, &directCore,
                         [&directCore](const QString& sdp, const QString& type) {
                             QVERIFY(directCore.acceptDescription(sdp, type));
                         });
        QObject::connect(&directCore, &IMediaTransport::localCandidate, &directWindow,
                         [&directWindow, &forwarder](const QString& candidate, const QString& mid) {
                             const QString through = forwarder.coreCandidate(candidate);
                             if (!through.isEmpty()) {
                                 QVERIFY(directWindow.acceptCandidate(through, mid));
                             }
                         });
        QObject::connect(&directWindow, &IMediaTransport::localCandidate, &directCore,
                         [&directCore, &forwarder](const QString& candidate, const QString& mid) {
                             const QString through = forwarder.windowCandidate(candidate);
                             if (!through.isEmpty()) {
                                 QVERIFY(directCore.acceptCandidate(through, mid));
                             }
                         });
        QSignalSpy directCoreReady(&directCore, &IMediaTransport::ready);
        QSignalSpy directWindowReady(&directWindow, &IMediaTransport::ready);
        QSignalSpy directRtp(&directWindow, &IMediaTransport::rtpReceived);
        QVERIFY(directWindow.start({IMediaTransport::Role::Answerer, kWindowSsrc}));
        QVERIFY(directCore.start({IMediaTransport::Role::Offerer, kCoreSsrc}));
        // The product's bound for an ICE connection: gathering, then the
        // connectivity checks, after which failure is certain
        // (IceConfiguration::kConnectDeadlineMs; RemoteMediaController gives
        // a replacement the same bound after its description).
        QTRY_VERIFY2_WITH_TIMEOUT(
            directCoreReady.size() == 1 && directWindowReady.size() == 1,
            qPrintable(QStringLiteral("direct pair: core ready %1, window ready %2; forwarder "
                                      "dropped %3")
                           .arg(directCoreReady.size())
                           .arg(directWindowReady.size())
                           .arg(forwarder.dropped())),
            IceConfiguration::kConnectDeadlineMs);
        const auto directPath = directWindow.selectedPath();
        QVERIFY(directPath);
        QVERIFY(!directPath->viaLoopbackShim());
        QCOMPARE(directPath->remotePort, forwarder.windowFacingPort());
        QVERIFY(directCore.sendRtp(tunnelRtpPacket(1, kCoreSsrc)));
        QTRY_COMPARE_WITH_TIMEOUT(directRtp.size(), 1, 10000);

        // Cut: the next packet goes into the forwarder and no further.
        forwarder.cut();
        const quint64 droppedBefore = forwarder.dropped();
        QVERIFY(directCore.sendRtp(tunnelRtpPacket(2, kCoreSsrc)));
        QTRY_VERIFY_WITH_TIMEOUT(forwarder.dropped() > droppedBefore, 10000);

        // The replacement over the session link: the Core with its host
        // candidates and the tunnel, the window with the tunnel's
        // candidate alone, under one media generation.
        Test::LoopbackTransport coreLink(QStringLiteral("core"));
        Test::LoopbackTransport windowLink(QStringLiteral("window"));
        coreLink.linkTo(&windowLink);
        auto coreTunnel = MediaTunnel::create(&coreLink);
        auto windowTunnel = MediaTunnel::create(&windowLink);
        QVERIFY(coreTunnel && windowTunnel);
        BinaryCount atWindow(&windowLink);
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        LibDataChannelMediaTransport core;
        LibDataChannelMediaTransport window;
        int coreCandidates = 0;
        int refused = 0;
        QObject::connect(&core, &IMediaTransport::localDescription, &window,
                         [&window](const QString& sdp, const QString& type) {
                             QVERIFY(window.acceptDescription(sdp, type));
                         });
        QObject::connect(&window, &IMediaTransport::localDescription, &core,
                         [&core](const QString& sdp, const QString& type) {
                             QVERIFY(core.acceptDescription(sdp, type));
                         });
        QObject::connect(&core, &IMediaTransport::localCandidate, &window,
                         [&window, &coreCandidates, &refused](const QString& candidate,
                                                              const QString& mid) {
                             ++coreCandidates;
                             if (!window.acceptCandidate(candidate, mid)) { ++refused; }
                         });
        QSignalSpy windowCandidates(&window, &IMediaTransport::localCandidate);
        QSignalSpy windowErrors(&window, &IMediaTransport::errorOccurred);
        QSignalSpy coreGathered(&core, &IMediaTransport::gatheringComplete);
        QSignalSpy coreReady(&core, &IMediaTransport::ready);
        QSignalSpy windowReady(&window, &IMediaTransport::ready);
        QSignalSpy tunnelRtp(&window, &IMediaTransport::rtpReceived);
        IMediaTransport::StartOptions windowOptions{IMediaTransport::Role::Answerer, kWindowSsrc};
        windowOptions.connectionId = id;
        windowOptions.ice = MediaTunnel::tunnelIceFor(windowTunnel);
        IMediaTransport::StartOptions coreOptions{IMediaTransport::Role::Offerer, kCoreSsrc};
        coreOptions.connectionId = id;
        coreOptions.ice = MediaTunnel::iceFor(coreTunnel, std::nullopt);
        QVERIFY(window.start(windowOptions));
        QVERIFY(core.start(coreOptions));
        // What each end of the tunnel-only pair got to, for a failure.
        const auto describeTunnelPair = [&] {
            QStringList errors;
            for (const QList<QVariant>& error : std::as_const(windowErrors)) {
                errors.append(error.value(0).toString());
            }
            return QStringLiteral("core ready %1, window ready %2, core gathered %3; core "
                                  "candidates %4 (window refused %5), window candidates %6; "
                                  "tunnel messages at the window %7; window errors: %8")
                .arg(coreReady.size())
                .arg(windowReady.size())
                .arg(coreGathered.size())
                .arg(coreCandidates)
                .arg(refused)
                .arg(windowCandidates.size())
                .arg(atWindow.messages)
                .arg(errors.join(QStringLiteral(" | ")));
        };
        // The product's ICE connect bound, as for the direct pair above.
        QTRY_VERIFY2_WITH_TIMEOUT(coreReady.size() == 1 && windowReady.size() == 1,
                                  qPrintable(describeTunnelPair()),
                                  IceConfiguration::kConnectDeadlineMs);
        const auto tunnelPath = window.selectedPath();
        QVERIFY(tunnelPath);
        QVERIFY(tunnelPath->viaLoopbackShim());
        QVERIFY(atWindow.messages > 0);
        QVERIFY(core.sendRtp(tunnelRtpPacket(3, kCoreSsrc)));
        QTRY_COMPARE_WITH_TIMEOUT(tunnelRtp.size(), 1, 10000);
        QCOMPARE(tunnelRtp.at(0).at(0).toByteArray(), tunnelRtpPacket(3, kCoreSsrc));
        // The window offered nothing of its own and took none of the Core's
        // host candidates, each refused by the rule rather than an error.
        QTRY_VERIFY_WITH_TIMEOUT(coreGathered.size() == 1, 10000);
        QVERIFY(coreCandidates > 0);
        QCOMPARE(refused, coreCandidates);
        QCOMPARE(windowCandidates.size(), 0);
        QCOMPARE(windowErrors.size(), 0);
        // Nothing crossed the cut pair.
        QCOMPARE(directRtp.size(), 1);
        core.stop();
        window.stop();
        directCore.stop();
        directWindow.stop();
    }

    // A direct-only replacement: STUN and host candidates, no tunnel and no
    // relay, so ICE can only nominate a direct pair.
    void theDirectOnlyConfigurationHasStunAndNoCandidateSource()
    {
        const IceServerAddress stun{QStringLiteral("stun.example.test"), 19302};
        const IceConfiguration ice = MediaTunnel::directIceFor(stun);
        QCOMPARE(ice.stunServer(), std::optional<IceServerAddress>(stun));
        QVERIFY(!ice.hasCandidateSourceFactory());
        QVERIFY(!ice.makeCandidateSource(IceConfiguration::kMediaLane,
                                         QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")));
        QVERIFY(!ice.mediaRouting());
        QVERIFY(!ice.relayAllowed());
        QVERIFY(ice.relayKnown());
        QVERIFY(ice.relayServers().isEmpty());
        QVERIFY(!MediaTunnel::directIceFor(std::nullopt).stunServer());
    }

    void cleanup()
    {
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(false);
        RealtimeTestLoad::printLoadAverageIfFailed();
    }

    // No UDP pair works: audio plays through the tunnel on the session's
    // own link.
    void mediaRunsThroughTheTunnelWhenNothingElseWorks()
    {
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QTimer source;
        source.setInterval(10);
        QObject::connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        QObject::connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        h.connectSession();
        QCOMPARE(h.client.capabilities().mediaTunnelVersion, 1);
        QVERIFY(h.client.mediaTunnelAvailable());
        BinaryCount atCore(h.stationLink);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 20000);
        QVERIFY(atCore.messages > 0);
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void realDtlsMediaReplacesThroughIndependentTunnelSockets()
    {
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QTimer source;
        source.setInterval(10);
        QObject::connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        QObject::connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 20000);
        const QString oldId = remoteMedia.mediaConnectionId();
        QVERIFY(!oldId.isEmpty());
        const int switchFrom = h.remoteBus->heard.size() / 2;
        QVERIFY(remoteMedia.replaceConnection());
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.mediaConnectionId() != oldId, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(h.remoteBus->heard.size() / 2 >= switchFrom + 48000, 10000);
        int silentRun = 0;
        int longestSilentRun = 0;
        const QVector<float>& heard = h.remoteBus->heard;
        for (int frame = switchFrom; (frame + 480) * 2 <= heard.size(); frame += 480) {
            double energy = 0.0;
            for (int i = 0; i < 480; ++i) {
                const double sample = heard.at((frame + i) * 2);
                energy += sample * sample;
            }
            silentRun = energy / 480.0 < 1e-6 ? silentRun + 1 : 0;
            longestSilentRun = std::max(longestSilentRun, silentRun);
        }
        QVERIFY2(longestSilentRun <= 4, qPrintable(QString::number(longestSilentRun * 10)));
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // The direct media ladder: media that settled on the tunnel moves to a
    // direct-only connection (host pairs, no tunnel) at the schedule's
    // step once UDP works, and audio keeps playing across the move.
    void mediaOnTheTunnelMovesToADirectPath()
    {
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QTimer source;
        source.setInterval(10);
        QObject::connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        QObject::connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        h.connectSession();
        QVERIFY(h.client.mediaDirectAvailable());
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 20000);
        // On the tunnel, the first step is armed.
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.directUpgradeDelayMs(),
                                  PathRacer::kUpgradeRetryMs[0], 5000);
        const QString oldId = remoteMedia.mediaConnectionId();
        // UDP works from here on; the step runs now instead of in 5 s.
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(false);
        remoteMedia.runDirectUpgradeStep();
        QVERIFY(remoteMedia.replacingConnection());
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.mediaConnectionId() != oldId, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 5000);
        // Off the tunnel: nothing more is scheduled, and audio stops
        // riding the session's link.
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.directUpgradeDelayMs(), -1, 5000);
        // Counted against the Core's own audio rather than a clock: after
        // the Core takes 200 more blocks (about 2 s of audio), fewer than 10
        // messages rode the session's link.
        BinaryCount atCore(h.stationLink);
        int blocks = 0;
        const QMetaObject::Connection fed = QObject::connect(
            &source, &QTimer::timeout, &source, [&blocks] { ++blocks; });
        QTRY_VERIFY_WITH_TIMEOUT(blocks >= 200, 20000);
        QObject::disconnect(fed);
        QVERIFY2(atCore.messages < 10, qPrintable(QString::number(atCore.messages)));
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // A silent tunnel requests session recovery within a few seconds. The
    // in-process link has no address to redial, so the fixture supplies the
    // next link after the production recovery controller closes the first.
    // Both decoded speaker output and a presented display frame must return.
    void aStalledTunnelIsStartedAgainWithinSeconds()
    {
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);
        Test::RemoteAudioSessionHarness h;
        const int stream = h.station.sliceById(h.sliceA)->streamIndex();
        QVERIFY(stream >= 0);
        PanadapterStack stack;
        auto* pan = stack.addPanadapter(QStringLiteral("recovery"));
        pan->setActiveSliceIndex(h.sliceA);
        pan->spectrumWidget()->setDisplayWindowPreservingHistory(
            h.station.streamCentreHz(stream), 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        RemoteMediaController remoteMedia(&h.client, &h.remote, &stack);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        RemoteConnectionController connection(&h.client, &h.remote, RemoteStationOptions{});
        QObject::connect(&remoteMedia, &RemoteMediaController::recoveryRequested,
                         &connection, &RemoteConnectionController::recoverMediaSession,
                         Qt::QueuedConnection);
        QSignalSpy recovery(&remoteMedia, &RemoteMediaController::recoveryRequested);
        QSignalSpy ended(&h.client, &StationClient::sessionEnded);
        QSignalSpy frames(&remoteMedia, &RemoteMediaController::displayFrameReceived);
        QTimer source;
        source.setInterval(10);
        QObject::connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        QTimer display;
        display.setInterval(20);
        QObject::connect(&display, &QTimer::timeout, &display, [&h, stream, &iq] {
            QMetaObject::invokeMethod(&h.station, "rawIqDataForStream", Qt::DirectConnection,
                                      Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
        });
        QTimer speaker;
        speaker.setInterval(10);
        QObject::connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        display.start();
        speaker.start();
        h.connectSession();
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty(), 10000);
        const QString oldId = remoteMedia.mediaConnectionId();
        QVERIFY(!oldId.isEmpty());
        // The window's stall clock starts at the last audio packet it took,
        // which can come before the link goes silent (on a loaded computer
        // it was 20 ms before, load average 34). So the bound is anchored on
        // the last message the window's session link delivered, and the
        // display stops first so that message is audio: every tunnel message
        // after a quiet 300 ms with no display frame is. The window takes an
        // audio packet after its tunnel message arrives, so the rule (never
        // before kMediaStallMs of silence) holds from that anchor exactly.
        QElapsedTimer silence;
        silence.start();
        qint64 lastMessageMs = -1;
        qint64 lastFrameMs = -1;
        qint64 recoveredMs = -1;
        const QMetaObject::Connection messages = QObject::connect(
            h.client.sessionTransport(), &SessionTransport::binaryReceived, &remoteMedia,
            [&silence, &lastMessageMs](const QByteArray&) { lastMessageMs = silence.elapsed(); });
        const QMetaObject::Connection frameTimes = QObject::connect(
            &remoteMedia, &RemoteMediaController::displayFrameReceived, &remoteMedia,
            [&silence, &lastFrameMs] { lastFrameMs = silence.elapsed(); });
        const QMetaObject::Connection recoveredAt = QObject::connect(
            &remoteMedia, &RemoteMediaController::recoveryRequested, &remoteMedia,
            [&silence, &recoveredMs] {
                if (recoveredMs < 0) { recoveredMs = silence.elapsed(); }
            });
        display.stop();
        const qint64 displayStoppedMs = silence.elapsed();
        QTRY_VERIFY_WITH_TIMEOUT(
            lastMessageMs - std::max(displayStoppedMs, lastFrameMs) > 300, 10000);
        h.stationLink->setDropsOutgoing(true);
        QTRY_VERIFY_WITH_TIMEOUT(!recovery.isEmpty(), 10000);
        QObject::disconnect(messages);
        QObject::disconnect(frameTimes);
        QObject::disconnect(recoveredAt);
        display.start();
        const qint64 found = recoveredMs - lastMessageMs;
        qInfo("A stalled tunnel found %lld ms after its last message",
              static_cast<long long>(found));
        QVERIFY2(found >= RemoteMediaController::kMediaStallMs && found <= 5000,
                 qPrintable(QString::number(found)));
        QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 1, 5000);
        const int framesBeforeReconnect = frames.size();
        h.connectSession(); // the fixture's replacement for an address-based redial
        const int audioStart = h.remoteBus->heard.size() / 2;
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.mediaConnectionId() != oldId
                                     && !remoteMedia.mediaConnectionId().isEmpty(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() > framesBeforeReconnect, 10000);
        const qint64 displayResumeMs = silence.elapsed();
        const auto heardAgain = [&h, audioStart] {
            const auto& heard = h.remoteBus->heard;
            for (int frame = audioStart; (frame + 480) * 2 <= heard.size(); frame += 480) {
                double energy = 0.0;
                for (int i = 0; i < 480; ++i) {
                    const double sample = heard.at((frame + i) * 2);
                    energy += sample * sample;
                }
                if (energy / 480.0 >= 1e-6) { return true; }
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT(heardAgain(), 10000);
        const qint64 audioResumeMs = silence.elapsed();
        qInfo("Tunnel recovery decoded audio after %lld ms and displayed a frame after %lld ms",
              static_cast<long long>(audioResumeMs), static_cast<long long>(displayResumeMs));
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void tuneSilenceDoesNotDiscardTheTunnel_data()
    {
        QTest::addColumn<bool>("resumeAudio");
        QTest::newRow("rx-audio-resumes") << true;
        QTest::newRow("rx-audio-still-missing") << false;
    }

    void tuneSilenceDoesNotDiscardTheTunnel()
    {
        QFETCH(bool, resumeAudio);
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);
        Test::RemoteAudioSessionHarness h;
        const int stream = h.station.sliceById(h.sliceA)->streamIndex();
        QVERIFY(stream >= 0);
        PanadapterStack stack;
        auto* pan = stack.addPanadapter(QStringLiteral("tune"));
        pan->setActiveSliceIndex(h.sliceA);
        pan->spectrumWidget()->setDisplayWindowPreservingHistory(
            h.station.streamCentreHz(stream), 48000);
        stack.resize(600, 400);
        stack.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stack));
        RemoteMediaController remoteMedia(&h.client, &h.remote, &stack);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QSignalSpy recovery(&remoteMedia, &RemoteMediaController::recoveryRequested);
        QSignalSpy frames(&remoteMedia, &RemoteMediaController::displayFrameReceived);
        QTimer source;
        source.setInterval(10);
        QObject::connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QVector<float> iq(2048);
        for (int i = 0; i < iq.size(); i += 2) {
            iq[i] = 0.01f * std::cos(double(i) * 0.17);
            iq[i + 1] = 0.01f * std::sin(double(i) * 0.17);
        }
        QTimer display;
        display.setInterval(20);
        QObject::connect(&display, &QTimer::timeout, &display, [&h, stream, &iq] {
            QMetaObject::invokeMethod(&h.station, "rawIqDataForStream", Qt::DirectConnection,
                                      Q_ARG(int, stream), Q_ARG(QVector<float>, iq));
        });
        QTimer speaker;
        speaker.setInterval(10);
        QObject::connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        display.start();
        speaker.start();
        h.connectSession();
        QVERIFY(h.client.capabilities().txStateVersion >= 1);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty(), 10000);
        const QString mediaId = remoteMedia.mediaConnectionId();
        QVERIFY(!mediaId.isEmpty());
        const auto heardTone = [&h](int firstFrame) {
            const auto& heard = h.remoteBus->heard;
            for (int frame = firstFrame; (frame + 480) * 2 <= heard.size(); frame += 480) {
                double energy = 0.0;
                for (int i = 0; i < 480; ++i) {
                    const double sample = heard.at((frame + i) * 2);
                    energy += sample * sample;
                }
                if (energy / 480.0 >= 1e-6) { return true; }
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT(heardTone(0), 5000);

        h.station.moxController()->setMoxCheck({}); // fake Core, no radio
        h.station.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        // The fake Core has no physical transmitter. Drive its existing
        // MOX/TUNE state machine and mirrored transmit model directly.
        h.station.transmitModel().setTune(true);
        h.station.moxController()->setTune(true);
        QTRY_VERIFY_WITH_TIMEOUT(h.client.transmitState()->keyed()
                                     && h.client.transmitState()->tuning(), 5000);
        source.stop(); // half-duplex RX silence, while display remains active
        // A tunnel selected after TX began must still shorten the control
        // heartbeat. Reset the selection to model that path-settling edge.
        h.client.setMediaTunnelInUse(false);
        QCOMPARE(h.client.effectiveHeartbeatIntervalMs(),
                 StationClient::kDefaultHeartbeatIntervalMs);
        QTRY_COMPARE_WITH_TIMEOUT(h.client.effectiveHeartbeatIntervalMs(),
                                  StationClient::kRelayedHeartbeatIntervalMs, 1500);
        QTest::qWait(RemoteMediaController::kMediaStallMs + 600);
        QCOMPARE(recovery.size(), 0);
        QCOMPARE(remoteMedia.mediaConnectionId(), mediaId);

        // The RX stall clock restarts on the notification that the Core
        // stopped transmitting, which the check below can see up to a poll
        // later (2983 ms found at load 3.8). So the silence is timed from
        // that notification: this slot runs after the window's own.
        QElapsedTimer rxSilence;
        const TransmitState* txState = h.client.transmitState();
        const QMetaObject::Connection rxEdge = QObject::connect(
            txState, &TransmitState::stateChanged, &remoteMedia, [&rxSilence, txState] {
                if (!rxSilence.isValid() && !txState->keyed() && !txState->tuning()
                    && !txState->txEnding()) {
                    rxSilence.start();
                }
            });
        h.station.moxController()->setTune(false);
        h.station.transmitModel().setTune(false);
        QTRY_VERIFY_WITH_TIMEOUT(!h.client.transmitState()->keyed()
                                     && !h.client.transmitState()->tuning()
                                     && !h.client.transmitState()->txEnding(), 5000);
        QObject::disconnect(rxEdge);
        QVERIFY(rxSilence.isValid());
        const int framesBeforeRx = frames.size();
        if (resumeAudio) {
            const int audioStart = h.remoteBus->heard.size() / 2;
            source.start();
            QTRY_VERIFY_WITH_TIMEOUT(heardTone(audioStart), 5000);
            QTRY_VERIFY_WITH_TIMEOUT(frames.size() > framesBeforeRx, 5000);
            QCOMPARE(recovery.size(), 0);
            QCOMPARE(remoteMedia.mediaConnectionId(), mediaId);
        } else {
            // Unrelated stateChanged notifications while idle must not
            // postpone an already armed RX stall clock.
            QSignalSpy unrelatedUpdates(h.client.transmitState(),
                                        &TransmitState::stateChanged);
            QTimer unrelated;
            unrelated.setInterval(100);
            int counter = 0;
            QObject::connect(&unrelated, &QTimer::timeout, &unrelated, [&] {
                const int sliceId = (++counter & 1) ? h.sliceB : h.sliceA;
                h.client.transmitState()->applyStationValue("txSliceId", sliceId);
            });
            unrelated.start();
            QTRY_VERIFY_WITH_TIMEOUT(!recovery.isEmpty(), 6000);
            QVERIFY(unrelatedUpdates.size() >= 10);
            QVERIFY2(rxSilence.elapsed() >= RemoteMediaController::kMediaStallMs
                         && rxSilence.elapsed() <= 5000,
                     qPrintable(QString::number(rxSilence.elapsed())));
        }
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    // UDP works: a host pair wins, and the tunnel is only a candidate.
    void aHostPairStillWins()
    {
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        QTimer source;
        source.setInterval(10);
        QObject::connect(&source, &QTimer::timeout, &source, [&h] { h.feedMixedTone(); });
        QTimer speaker;
        speaker.setInterval(10);
        QObject::connect(&speaker, &QTimer::timeout, &speaker, [&h] { h.remoteBus->render(480); });
        source.start();
        speaker.start();
        h.connectSession();
        BinaryCount atCore(h.stationLink);
        QTRY_VERIFY_WITH_TIMEOUT(remoteMedia.audioStatus().state
                                     == RemoteAudioStatus::State::Playing, 20000);
        // Two seconds of audio (about 50 packets): none of it came through
        // the tunnel, which carries at most the agent's checks of its pair.
        const int before = atCore.messages;
        QTest::qWait(2000);
        QVERIFY2(atCore.messages - before < 10,
                 qPrintable(QString::number(atCore.messages - before)));
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }
};

QTEST_MAIN(TstMediaTunnel)
#include "tst_media_tunnel.moc"
