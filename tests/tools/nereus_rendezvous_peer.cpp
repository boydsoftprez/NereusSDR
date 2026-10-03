// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/nereus_rendezvous_peer.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 27 (R-IOS-16): one end of a connection through the
// remote access service, for the traversal harness
// (tests/scripts/traversal-harness.sh), which runs it in network namespaces
// behind different NATs and firewalls. It uses the Core's own pieces:
// RendezvousClient, IceConfiguration and LibDataChannelMediaTransport.
//
//   nereus_rendezvous_peer key --dir DIR
//       Prints the public key (base64url SPKI) of the key in DIR, making
//       it on first use.
//   nereus_rendezvous_peer station --dir DIR --server URL --paired KEY
//                          [--relay deny] [--id-file FILE] [--ca FILE]
//       Registers with the service under DIR's key and answers
//       introductions from the device whose public key is KEY (the only
//       paired device); echoes every message back. Writes its rendezvous id
//       to FILE once registered. Runs until killed.
//   nereus_rendezvous_peer client --dir DIR --server URL --station-id ID
//                          [--send-bytes N] [--timeout-ms T] [--ca FILE]
//                          [--require-ipv6]
//       Introduces itself with DIR's key, connects, sends one message of N
//       bytes (default 60000, many 1000-byte datagrams) and waits for it to
//       come back. Prints one JSON line
//       {"connected":bool,"echoed":bool,"relayed":bool,"localType",
//        "remoteType","localAddress","remoteAddress","ms","reason"} and
//       exits 0 when the message came back, 1 otherwise. With
//       --require-ipv6, also waits up to 10 seconds after the echo for
//       both selected-path addresses to be numeric IPv6 addresses.
//
//   nereus_rendezvous_peer device-key --dir DIR
//       Prints the public key (base64url SPKI) of a desktop's device key in
//       DIR (ClientDeviceIdentity), making it on first use.
//   nereus_rendezvous_peer core --dir DIR --server URL --paired KEY
//                          [--relay deny] [--id-file FILE] [--ca FILE]
//       Plan Task 28: a Core, as nereusd runs it (a StationServer with no
//       radio, and StationRendezvous), with the desktop whose device key is
//       KEY paired. It answers introductions with a control connection and
//       runs the session over it. Writes "<rendezvous id> <identity
//       fingerprint, base64url>" to FILE once registered. Runs until
//       killed.
//   nereus_rendezvous_peer session --dir DIR --server URL --core FILE
//                          [--timeout-ms T] [--ca FILE]
//       Plan Task 28: a desktop (StationClient::connectThroughService) with
//       DIR's device key reaches the Core named in FILE through the service
//       and runs the whole connect sequence. Prints one JSON line
//       {"connected":bool,"relayed":bool,"localType","remoteType",
//        "localAddress","remoteAddress","ms","reason"} and
//       exits 0 when the session was established, 1 otherwise.
//
// Plan Task 29 (R-IOS-16; the link document, section 21):
//   core ... [--listen PORT [--listen-loopback]] [--media]
//       --listen: the Core's WebSocket on every address at PORT, for the
//       direct path. --listen-loopback: bind that listener only to IPv4
//       loopback (127.0.0.1); requires --listen. --media: media on
//       (a DaemonMediaController), a tone in
//       the Core's audio and synthetic I/Q in its display source, so a
//       session's media carries real Opus audio and real display frames.
//   session ... [--direct URL] [--upgrade-schedule-ms "A,B"]
//               [--wait-upgrade-ms T] [--media-ms M]
//       --direct: the race (StationClient::connectToStation for a paired
//       Core) with URL and the service at once, instead of the service
//       alone. Prints {"event":"connected",...} as soon as the session is
//       up. --wait-upgrade-ms: then waits up to T for the session to move
//       to a better path (--upgrade-schedule-ms sets the looks).
//       --media-ms: then runs media for M ms (a MediaPeer answering the
//       Core's offer, audio on, one display) and counts what decoded. The
//       last line adds "rank", "path", "attempt", "moved", "rankAfter",
//       "handshakes", "switches", and with media "audioPackets",
//       "audioDecoded", "displayMessages", "displayDecoded".
//
// --ca adds a certificate authority the harness made at run time, so the
// service's TLS (a test certificate for its test name) verifies. Nothing
// secret is printed; keys stay in DIR.
//
// It is not linked with the test sandbox: it talks to a service in another
// namespace, which a test-mode RendezvousClient refuses to reach.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Task 27 follow-up (new Minor 2): the station answers an
//               introduction only once its STUN names are resolved. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): the device-key, core and
//               session modes, a whole session over an introduced control
//               connection. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the race, a move to a
//               better path, and real media through the service (core
//               --listen and --media; session --direct, --wait-upgrade-ms
//               and --media-ms). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 fix wave (R-IOS-16, review
//               Important 1): session --follow-media-ms, the desktop's own
//               media (RemoteMediaController) played into a paced bus, to
//               show media following a relay-to-direct move with no gap.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: opt-in IPv6 preference observation for the traversal test.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#include <QCoreApplication>
#include <QApplication>
#include <QElapsedTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QUuid>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTextStream>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/IceConfiguration.h"
#include "core/session/RendezvousClient.h"
#include "core/session/RendezvousWire.h"
#include "core/session/StationClient.h"
#include "core/session/TransmitStateFacade.h"
#include "core/session/StationRendezvous.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/media/MediaPeer.h"
#include "core/session/media/OpusAudioCodec.h"
#include "models/SliceModel.h"
#include "core/settings/SettingsProxy.h"
#include "gui/RemoteMediaController.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "core/MoxController.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/PathRacer.h"
#include "core/session/RemoteTransmitClient.h"
#include "models/TransmitModel.h"
#include "../fakes/PacedAudioBus.h"
#include "models/RadioModel.h"

#include "StunLookupGate.h"

using namespace NereusSDR;

namespace {

QString option(const QStringList& args, const QString& name, const QString& fallback = QString())
{
    const qsizetype index = args.indexOf(name);
    return index >= 0 && index + 1 < args.size() ? args.at(index + 1) : fallback;
}

void printLine(const QJsonObject& object)
{
    std::fputs(QJsonDocument(object).toJson(QJsonDocument::Compact).constData(), stdout);
    std::fputs("\n", stdout);
    std::fflush(stdout);
}

bool addAuthority(const QString& path)
{
    if (path.isEmpty()) {
        return true;
    }
    const QList<QSslCertificate> authorities = QSslCertificate::fromPath(path);
    if (authorities.isEmpty()) {
        return false;
    }
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.addCaCertificates(authorities);
    QSslConfiguration::setDefaultConfiguration(configuration);
    return true;
}

QJsonObject pathObject(const IMediaTransport& transport)
{
    QJsonObject object;
    if (const auto path = transport.selectedPath()) {
        object.insert(QStringLiteral("relayed"), path->relayed());
        object.insert(QStringLiteral("localType"), path->localType);
        object.insert(QStringLiteral("remoteType"), path->remoteType);
        object.insert(QStringLiteral("localAddress"), path->localAddress);
        object.insert(QStringLiteral("remoteAddress"), path->remoteAddress);
    }
    return object;
}

bool hasIpv6Pair(const QJsonObject& path)
{
    QHostAddress local;
    QHostAddress remote;
    return local.setAddress(path.value(QStringLiteral("localAddress")).toString())
           && remote.setAddress(path.value(QStringLiteral("remoteAddress")).toString())
           && local.protocol() == QAbstractSocket::IPv6Protocol
           && remote.protocol() == QAbstractSocket::IPv6Protocol;
}

int runStation(const QStringList& args)
{
    const StationIdentity key = StationIdentity::loadOrCreate(option(args, QStringLiteral("--dir")));
    bool ok = false;
    const QByteArray paired = StationIdentity::fromBase64Url(option(args, QStringLiteral("--paired")), &ok);
    if (!key.isValid() || !ok || !StationIdentity::isP256Spki(paired)) {
        std::fputs("station: a key is not usable\n", stderr);
        return 2;
    }
    const bool relayAllowed = option(args, QStringLiteral("--relay"), QStringLiteral("allow"))
                              != QLatin1String("deny");
    auto* client = new RendezvousClient(QCoreApplication::instance());
    client->setServers(RendezvousClient::serverUrls({option(args, QStringLiteral("--server"))}));
    client->setRelayAllowed(relayAllowed);
    const QString idFile = option(args, QStringLiteral("--id-file"));
    QObject::connect(client, &RendezvousClient::registered, client, [client, idFile] {
        if (!idFile.isEmpty()) {
            QFile file(idFile + QStringLiteral(".part"));
            if (file.open(QIODevice::WriteOnly)) {
                file.write(client->stationId().toLatin1());
                file.close();
                QFile::remove(idFile);
                file.rename(idFile);
            }
        }
        printLine({{QStringLiteral("event"), QStringLiteral("registered")}});
    });
    // Each introduction is answered once the STUN names the hello lists
    // are resolved, so it chooses its STUN server by this end's address
    // families (IceConfiguration, fix wave I2). RendezvousClient registers
    // as the hello arrives, so an introduction that comes during the lookup
    // waits for it (StunLookupGate, the follow-up to the re-review, new
    // Minor 2) rather than choosing from families not yet known.
    auto gate = std::make_shared<Test::StunLookupGate>(
        [client, relayAllowed](const RendezvousIntroduction& introduction,
                               const HostFamilies& stunFamilies) {
        const QByteArray id = introduction.id;
        auto ice = std::make_shared<IceConfiguration>(IceConfiguration::throughRendezvous(
            client->stunUrls(), relayAllowed, IceConfiguration::localAddressFamilies(),
            stunFamilies));
        auto* answerer = new LibDataChannelMediaTransport(client);
        QObject::connect(answerer, &IMediaTransport::localDescription, client,
                         [client, answerer, id, relayAllowed](const QString& sdp, const QString&) {
            client->answer(id, sdp);
            if (!relayAllowed) {
                answerer->gatherCandidates({});
            }
        });
        QObject::connect(answerer, &IMediaTransport::localCandidate, client,
                         [client, id](const QString& candidate, const QString&) {
            client->sendCandidate(id, candidate);
        });
        QObject::connect(answerer, &IMediaTransport::gatheringComplete, client,
                         [client, id] { client->sendCandidate(id, QString()); });
        QObject::connect(client, &RendezvousClient::credentialsReceived, answerer,
                         [answerer, ice, id](const QByteArray& from, bool offered,
                                             const RendezvousWire::Turn& turn) {
            if (from != id) {
                return;
            }
            const std::optional<RendezvousWire::Turn> relay =
                offered ? std::optional<RendezvousWire::Turn>(turn) : std::nullopt;
            IceConfiguration::resolveHostFamilies(
                relay ? IceConfiguration::hostNames(relay->urls) : QStringList(), answerer,
                [answerer, ice, relay](const HostFamilies& families) {
                    ice->addHostFamilies(families);
                    ice->setRelay(relay, 1);
                    answerer->gatherCandidates(ice->relayServers());
                });
        });
        QObject::connect(client, &RendezvousClient::candidateReceived, answerer,
                         [answerer, id](const QByteArray& from, const QString& candidate) {
            if (from == id && !candidate.isEmpty()) {
                answerer->acceptCandidate(candidate, QString());
            }
        });
        QObject::connect(answerer, &IMediaTransport::ready, client, [answerer] {
            QJsonObject event = pathObject(*answerer);
            event.insert(QStringLiteral("event"), QStringLiteral("connected"));
            printLine(event);
        });
        QObject::connect(answerer, &IMediaTransport::displayReceived, answerer,
                         [answerer](const QByteArray& message) { answerer->sendDisplay(message); });
        IMediaTransport::StartOptions options{IMediaTransport::Role::Answerer, 0x5a5a};
        options.ice = *ice;
        if (!answerer->start(options)
            || !answerer->acceptDescription(introduction.offer, QStringLiteral("offer"))) {
            printLine({{QStringLiteral("event"), QStringLiteral("refused")}});
        }
    });
    QObject::connect(client, &RendezvousClient::connected, client, [client, gate] {
        const quint64 lookup = gate->lookupStarted();
        IceConfiguration::resolveHostFamilies(
            IceConfiguration::hostNames(client->stunUrls()), client,
            [gate, lookup](const HostFamilies& families) {
                gate->lookupFinished(lookup, families);
            });
    });
    QObject::connect(client, &RendezvousClient::introduced, client,
                     [gate](const RendezvousIntroduction& introduction) {
        gate->introduce(introduction);
    });
    client->registerStation(key.publicKeySpki(),
                            [key](const QByteArray& message) { return key.sign(message); },
                            [paired](const QByteArray& deviceId) {
                                return StationIdentity::fingerprintOf(paired) == deviceId
                                           ? paired
                                           : QByteArray();
                            });
    return QCoreApplication::exec();
}

int runClient(const QStringList& args)
{
    const StationIdentity key = StationIdentity::loadOrCreate(option(args, QStringLiteral("--dir")));
    const QString stationId = option(args, QStringLiteral("--station-id"));
    const int sendBytes = option(args, QStringLiteral("--send-bytes"), QStringLiteral("60000")).toInt();
    const int timeoutMs = option(args, QStringLiteral("--timeout-ms"), QStringLiteral("90000")).toInt();
    const bool requireIpv6 = args.contains(QStringLiteral("--require-ipv6"));
    if (!key.isValid() || !RendezvousWire::isRendezvousId(stationId) || sendBytes < 1
        || sendBytes > IMediaTransport::kMaxDisplayMessageBytes) {
        std::fputs("client: an argument is not usable\n", stderr);
        return 2;
    }
    QElapsedTimer clock;
    clock.start();
    auto* client = new RendezvousClient(QCoreApplication::instance());
    client->setServers(RendezvousClient::serverUrls({option(args, QStringLiteral("--server"))}));
    auto ice = std::make_shared<IceConfiguration>();
    auto* offerer = new LibDataChannelMediaTransport(client);
    QByteArray message(sendBytes, Qt::Uninitialized);
    for (int index = 0; index < sendBytes; ++index) {
        message[index] = static_cast<char>(index * 31 + 7);
    }
    auto done = std::make_shared<bool>(false);
    auto firstEchoMs = std::make_shared<qint64>(-1);
    auto firstEchoPath = std::make_shared<QJsonObject>();
    const auto finish = [offerer, &clock, done, firstEchoMs, firstEchoPath,
                         requireIpv6](bool connected, bool echoed, const QString& reason) {
        if (*done) {
            return;
        }
        *done = true;
        QJsonObject result = pathObject(*offerer);
        result.insert(QStringLiteral("connected"), connected);
        result.insert(QStringLiteral("echoed"), echoed);
        result.insert(QStringLiteral("ms"), static_cast<double>(clock.elapsed()));
        if (requireIpv6 && *firstEchoMs >= 0) {
            result.insert(QStringLiteral("firstEchoMs"), static_cast<double>(*firstEchoMs));
            result.insert(QStringLiteral("firstEchoPath"), *firstEchoPath);
        }
        if (!reason.isEmpty()) {
            result.insert(QStringLiteral("reason"), reason);
        }
        if (!result.contains(QStringLiteral("relayed"))) {
            result.insert(QStringLiteral("relayed"), false);
        }
        printLine(result);
        QCoreApplication::exit(echoed && reason.isEmpty()
                                       && (!requireIpv6 || hasIpv6Pair(result))
                                   ? 0
                                   : 1);
    };
    QObject::connect(client, &RendezvousClient::connected, client, [client, offerer, ice, &key,
                                                                      stationId] {
        // The STUN server chosen by this end's address families, once the
        // names are resolved (fix wave I2).
        IceConfiguration::resolveHostFamilies(
            IceConfiguration::hostNames(client->stunUrls()), client,
            [client, offerer, ice, &key, stationId](const HostFamilies& families) {
                *ice = IceConfiguration::throughRendezvous(
                    client->stunUrls(), true, IceConfiguration::localAddressFamilies(), families);
                QObject::connect(offerer, &IMediaTransport::localDescription, client,
                                 [client, &key, stationId](const QString& sdp, const QString&) {
                    client->introduce(stationId, key.publicKeySpki(),
                                      [&key](const QByteArray& m) { return key.sign(m); }, sdp);
                });
                IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, 0xa5a5};
                options.ice = *ice;
                offerer->start(options);
            });
    });
    QObject::connect(client, &RendezvousClient::answerReceived, offerer,
                     [offerer, ice](const QString& sdp, bool offered, const RendezvousWire::Turn& turn) {
        offerer->acceptDescription(sdp, QStringLiteral("answer"));
        const std::optional<RendezvousWire::Turn> relay =
            offered ? std::optional<RendezvousWire::Turn>(turn) : std::nullopt;
        IceConfiguration::resolveHostFamilies(
            relay ? IceConfiguration::hostNames(relay->urls) : QStringList(), offerer,
            [offerer, ice, relay](const HostFamilies& families) {
                ice->addHostFamilies(families);
                ice->setRelay(relay, 1);
                offerer->gatherCandidates(ice->relayServers());
            });
    });
    QObject::connect(offerer, &IMediaTransport::localCandidate, client,
                     [client](const QString& candidate, const QString&) {
        client->sendCandidate(candidate);
    });
    QObject::connect(offerer, &IMediaTransport::gatheringComplete, client,
                     [client] { client->sendCandidate(QString()); });
    QObject::connect(client, &RendezvousClient::candidateReceived, offerer,
                     [offerer](const QByteArray&, const QString& candidate) {
        if (!candidate.isEmpty()) {
            offerer->acceptCandidate(candidate, QString());
        }
    });
    QObject::connect(client, &RendezvousClient::unreachable, offerer,
                     [finish](const QString& reason) { finish(false, false, reason); });
    QObject::connect(offerer, &IMediaTransport::connectionFailed, offerer,
                     [finish, firstEchoMs](const QString&) {
        finish(false, *firstEchoMs >= 0, QStringLiteral("The connection could not be made."));
    });
    if (requireIpv6) {
        QObject::connect(offerer, &IMediaTransport::closed, offerer, [finish, firstEchoMs] {
            finish(false, *firstEchoMs >= 0, QStringLiteral("The connection closed."));
        });
    }
    QObject::connect(offerer, &IMediaTransport::ready, offerer, [offerer, message] {
        offerer->sendDisplay(message);
    });
    QObject::connect(offerer, &IMediaTransport::displayReceived, offerer,
                     [finish, message, offerer, &clock, done, firstEchoMs, firstEchoPath,
                      requireIpv6, timeoutMs](const QByteArray& echoed) {
        if (*done || *firstEchoMs >= 0) {
            return;
        }
        if (echoed != message) {
            finish(true, false, QStringLiteral("The echo differed."));
            return;
        }
        if (!requireIpv6) {
            finish(true, true, QString());
            return;
        }
        *firstEchoMs = clock.elapsed();
        *firstEchoPath = pathObject(*offerer);
        if (hasIpv6Pair(*firstEchoPath)) {
            finish(true, true, QString());
            return;
        }
        auto* poll = new QTimer(offerer);
        poll->setInterval(25);
        QObject::connect(poll, &QTimer::timeout, offerer,
                         [finish, offerer, done, &clock, firstEchoMs, timeoutMs] {
            if (*done) {
                return;
            }
            if (clock.elapsed() > timeoutMs || clock.elapsed() - *firstEchoMs > 10000) {
                finish(true, true, QStringLiteral("IPv6 preference was not observed in time."));
            } else if (hasIpv6Pair(pathObject(*offerer))) {
                finish(true, true, QString());
            }
        });
        poll->start();
        QTimer::singleShot(10000, Qt::PreciseTimer, offerer, [finish, done] {
            if (!*done) {
                finish(true, true, QStringLiteral("IPv6 preference was not observed in time."));
            }
        });
    });
    QTimer::singleShot(timeoutMs, Qt::PreciseTimer, offerer, [finish, offerer, firstEchoMs] {
        finish(offerer->isReady(), *firstEchoMs >= 0,
               *firstEchoMs >= 0 ? QStringLiteral("IPv6 preference was not observed in time.")
                                 : QStringLiteral("No echo in time."));
    });
    client->connectToService();
    return QCoreApplication::exec();
}

// Plan Task 28: a Core as nereusd runs it, for a session over an introduced
// control connection.
int runCore(const QStringList& args)
{
    const bool listenRequested = args.contains(QStringLiteral("--listen"));
    const bool listenLoopback = args.contains(QStringLiteral("--listen-loopback"));
    bool listenPortOk = false;
    const uint loopbackPort = option(args, QStringLiteral("--listen")).toUInt(&listenPortOk);
    if (listenLoopback
        && (!listenRequested || !listenPortOk || loopbackPort == 0 || loopbackPort > 65535)) {
        std::fputs("core: --listen-loopback requires --listen PORT (1-65535)\n", stderr);
        return 2;
    }
    const QString dir = option(args, QStringLiteral("--dir"));
    bool ok = false;
    const QByteArray paired = StationIdentity::fromBase64Url(option(args, QStringLiteral("--paired")), &ok);
    if (dir.isEmpty() || !ok || !StationIdentity::isP256Spki(paired)) {
        std::fputs("core: an argument is not usable\n", stderr);
        return 2;
    }
    const bool relayAllowed = option(args, QStringLiteral("--relay"), QStringLiteral("allow"))
                              != QLatin1String("deny");
    auto* settings = new AppSettings(dir + QStringLiteral("/NereusSDR.settings"));
    settings->setValue(QStringLiteral("StationCallsign"), QStringLiteral("N0CALL"));
    auto* model = new RadioModel();
    auto* server = new StationServer(model, *settings, dir + QStringLiteral("/security"),
                                     QCoreApplication::instance());
    PairedDevice device;
    device.id = StationIdentity::fingerprintOf(paired);
    device.publicKeySpki = paired;
    device.name = QStringLiteral("Harness desktop");
    device.kind = QStringLiteral("computer");
    if (server->deviceStore() == nullptr
        || (!server->deviceStore()->find(device.id) && !server->deviceStore()->add(device))) {
        std::fputs("core: the desktop could not be paired\n", stderr);
        return 2;
    }
    auto* rendezvous = new StationRendezvous(
        server, RendezvousClient::serverUrls({option(args, QStringLiteral("--server"))}),
        relayAllowed, QCoreApplication::instance());
    QObject::connect(rendezvous->client(), &RendezvousClient::relayGrantReceived, server,
                     [rendezvous](const QByteArray& id) {
        const auto grant = rendezvous->client()->relayGrant(id);
        printLine({{QStringLiteral("event"), QStringLiteral("coreRendezvousGrant")},
                   {QStringLiteral("present"), grant.has_value()},
                   {QStringLiteral("watchGranted"), grant && !grant->watchToken.isEmpty()}});
    });
    const QString idFile = option(args, QStringLiteral("--id-file"));
    QObject::connect(rendezvous->client(), &RendezvousClient::registered, server,
                     [rendezvous, server, idFile] {
        if (!idFile.isEmpty()) {
            QFile file(idFile + QStringLiteral(".part"));
            if (file.open(QIODevice::WriteOnly)) {
                file.write(rendezvous->client()->stationId().toLatin1() + ' '
                           + StationIdentity::toBase64Url(server->stationIdentity().fingerprint())
                                 .toLatin1());
                file.close();
                QFile::remove(idFile);
                file.rename(idFile);
            }
        }
        printLine({{QStringLiteral("event"), QStringLiteral("registered")}});
    });
    QObject::connect(server, &StationServer::clientAuthenticated, server,
                     [](const QString& peer) {
        printLine({{QStringLiteral("event"), QStringLiteral("session")},
                   {QStringLiteral("peer"), peer}});
    });
    if (!rendezvous->start()) {
        std::fputs("core: the remote access service could not be used\n", stderr);
        return 2;
    }
    // Plan Task 29: the direct path, and real media.
    server->setRelayAllowed(relayAllowed);
    const QString listen = option(args, QStringLiteral("--listen"));
    if (!listen.isEmpty()
        && !server->listen(listenLoopback ? QHostAddress::LocalHost : QHostAddress::Any,
                           static_cast<quint16>(listen.toUInt()))) {
        std::fputs("core: could not listen\n", stderr);
        return 2;
    }
    if (listen.isEmpty() && args.contains(QStringLiteral("--keyable"))
        && !server->listen(QHostAddress::LocalHost, 0)) {
        // nereusd keeps its persistent TLS listener active when a web-relay
        // introduction wins. Mirror that authority here without exposing a
        // direct WAN path in the isolated namespace.
        std::fputs("core: could not open the loopback listener\n", stderr);
        return 2;
    }
    if (args.contains(QStringLiteral("--media"))) {
        const bool secondRx = args.contains(QStringLiteral("--second-rx"));
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                   /*defaultRateHz=*/192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int slice = model->addSlice();
        AudioEngine* engine = model->audioEngine();
        engine->setSliceStreaming(slice, true);
        int listeningSlice = -1;
        if (secondRx) {
            listeningSlice = model->addSlice(QStringLiteral("harness-second-rx"));
            const SliceModel* tx = model->sliceById(slice);
            const SliceModel* rx = model->sliceById(listeningSlice);
            if (!tx || !rx || tx->streamIndex() < 0 || rx->streamIndex() < 0
                || tx->streamIndex() == rx->streamIndex()
                || model->txBoundSlice() != tx) {
                std::fputs("core: second RX did not get a distinct non-TX receiver\n", stderr);
                return 2;
            }
            engine->setSliceStreaming(listeningSlice, true);
            printLine({{QStringLiteral("event"), QStringLiteral("secondRxReady")},
                       {QStringLiteral("txSlice"), slice},
                       {QStringLiteral("txStream"), tx->streamIndex()},
                       {QStringLiteral("listeningSlice"), listeningSlice},
                       {QStringLiteral("listeningStream"), rx->streamIndex()}});
        }
        server->setMediaEnabled(true);
        new DaemonMediaController(server, model, QCoreApplication::instance());
        // A 700 Hz tone in the Core's audio, 480 frames every 10 ms.
        auto* tone = new QTimer(QCoreApplication::instance());
        tone->setTimerType(Qt::PreciseTimer);
        auto frames = std::make_shared<qint64>(0);
        QObject::connect(tone, &QTimer::timeout, engine, [engine, slice, listeningSlice, frames] {
            constexpr int kFrames = 480;
            QVector<float> block(kFrames * 2);
            for (int frame = 0; frame < kFrames; ++frame) {
                const double time = static_cast<double>(*frames + frame) / 48000.0;
                const float value = static_cast<float>(0.25 * std::sin(2.0 * M_PI * 700.0 * time));
                block[frame * 2] = block[frame * 2 + 1] = value;
            }
            *frames += kFrames;
            engine->rxBlockReady(slice, block.constData(), kFrames);
            if (listeningSlice >= 0) {
                // The TX-bound slice is withdrawn during MOX. A distinct
                // receiver supplies the actual remote-audio mix throughout.
                for (int frame = 0; frame < kFrames; ++frame) {
                    const double time = static_cast<double>(*frames - kFrames + frame) / 48000.0;
                    const float value = static_cast<float>(0.25 * std::sin(2.0 * M_PI * 1100.0 * time));
                    block[frame * 2] = block[frame * 2 + 1] = value;
                }
                engine->rxBlockReady(listeningSlice, block.constData(), kFrames);
            }
        });
        tone->start(10);
        // Synthetic I/Q for the display, through RadioModel's own tap.
        auto* iq = new QTimer(QCoreApplication::instance());
        QObject::connect(iq, &QTimer::timeout, model, [model, slice] {
            const SliceModel* s = model->sliceById(slice);
            if (s == nullptr || s->streamIndex() < 0) {
                return;
            }
            QVector<float> samples;
            samples.reserve(1026 * 2);
            for (int n = 0; n < 1026; ++n) {
                const double phase = 2.0 * M_PI * 0.125 * n;
                samples.append(static_cast<float>(std::cos(phase)));
                samples.append(static_cast<float>(std::sin(phase)));
            }
            QMetaObject::invokeMethod(model, "rawIqDataForStream", Qt::DirectConnection,
                                      Q_ARG(int, s->streamIndex()),
                                      Q_ARG(QVector<float>, samples));
        });
        iq->start(5);
    }
    // Plan Task 29 step 2b: a Core whose radio a paired device may key (the
    // tests' makeTransmitReady), and the watchdog's view of each key: every
    // keepalive's gap since the one before and every trip, written once a
    // second while they change (the transmit deadline's measurement).
    if (args.contains(QStringLiteral("--keyable"))) {
        if (!args.contains(QStringLiteral("--media"))) {
            model->setBoardForTest(HPSDRHW::Saturn);
            model->setConnectionStateForTest(ConnectionState::Connected);
            model->addSlice();
        }
        server->setRemoteTransmitAllowed(true);
        model->transmitModel().setMicSourceLocked(false);
        model->transmitModel().setMicSource(MicSource::Radio);
        if (SliceModel* first = model->slices().value(0)) {
            first->setDspMode(DSPMode::USB);
            first->setFrequency(14200000.0);
        }
        // Record the Core's actual logical key transitions. The client-side
        // TUNE request alone cannot establish how long the Core stayed keyed.
        auto keyed = std::make_shared<bool>(false);
        auto* keyPoll = new QTimer(server);
        keyPoll->setInterval(10);
        keyPoll->setTimerType(Qt::PreciseTimer);
        QObject::connect(keyPoll, &QTimer::timeout, server, [model, keyed] {
            const bool now = !model->keyedBy().isEmpty() && model->transmitModel().isTune();
            if (now == *keyed) { return; }
            *keyed = now;
            printLine({{QStringLiteral("event"), QStringLiteral("coreKeyed")},
                       {QStringLiteral("on"), now},
                       {QStringLiteral("atEpochMs"), static_cast<double>(
                            QDateTime::currentMSecsSinceEpoch())}});
        });
        keyPoll->start();
        auto gaps = std::make_shared<QList<qint64>>();
        auto trips = std::make_shared<int>(0);
        auto changed = std::make_shared<bool>(false);
        QObject::connect(server->txWatchdog(), &RemoteTxWatchdog::keepaliveHeard, server,
                         [gaps, changed](const QByteArray&, qint64 sinceLastMs) {
                             if (sinceLastMs >= 0) {
                                 gaps->append(sinceLastMs);
                                 *changed = true;
                             }
                             printLine({{QStringLiteral("event"), QStringLiteral("keepaliveReceived")},
                                        {QStringLiteral("atEpochMs"), static_cast<double>(
                                             QDateTime::currentMSecsSinceEpoch())},
                                        {QStringLiteral("gapMs"), static_cast<double>(sinceLastMs)}});
                         });
        QObject::connect(server->txWatchdog(), &RemoteTxWatchdog::tripped, server,
                         [trips, changed](const QByteArray&, bool linkClosed, qint64 silentMs) {
                             ++*trips;
                             *changed = true;
                             printLine({{QStringLiteral("event"), QStringLiteral("tripped")},
                                        {QStringLiteral("linkClosed"), linkClosed},
                                        {QStringLiteral("atEpochMs"), static_cast<double>(
                                             QDateTime::currentMSecsSinceEpoch())},
                                        {QStringLiteral("silentMs"), static_cast<double>(silentMs)}});
                         });
        auto* stats = new QTimer(QCoreApplication::instance());
        QObject::connect(stats, &QTimer::timeout, server, [gaps, trips, changed] {
            if (!*changed) {
                return;
            }
            *changed = false;
            QList<qint64> sorted = *gaps;
            std::sort(sorted.begin(), sorted.end());
            const auto at = [&sorted](double q) {
                return sorted.isEmpty() ? 0.0
                                        : static_cast<double>(sorted.at(std::min<qsizetype>(
                                              sorted.size() - 1,
                                              static_cast<qsizetype>(q * sorted.size()))));
            };
            int over = 0;
            for (const qint64 gap : sorted) {
                if (gap > RemoteTxWatchdog::kLinkLossDeadlineMs) {
                    ++over;
                }
            }
            printLine({{QStringLiteral("event"), QStringLiteral("keepalives")},
                       {QStringLiteral("count"), static_cast<double>(sorted.size())},
                       {QStringLiteral("p50"), at(0.50)},
                       {QStringLiteral("p95"), at(0.95)},
                       {QStringLiteral("p99"), at(0.99)},
                       {QStringLiteral("max"), sorted.isEmpty() ? 0.0 : static_cast<double>(sorted.last())},
                       {QStringLiteral("overDeadline"), over},
                       {QStringLiteral("trips"), *trips}});
        });
        stats->start(1000);
    }
    return QCoreApplication::exec();
}

// Plan Task 29: the path the session runs on, in the harness's words.
QString pathName(int rank)
{
    switch (rank) {
    case PathRacer::ThisNetwork: return QStringLiteral("thisNetwork");
    case PathRacer::Direct: return QStringLiteral("direct");
    case PathRacer::ServiceDirect: return QStringLiteral("service");
    case PathRacer::ServiceRelayed: return QStringLiteral("relay");
    case PathRacer::Floor: return QStringLiteral("webRelay");
    default: return QStringLiteral("none");
    }
}

QJsonObject watchProbe(const StationClient* window)
{
    QJsonObject probe{{QStringLiteral("event"), QStringLiteral("watchProbe")},
                      {QStringLiteral("remoteTxVersion"), window->capabilities().remoteTxVersion},
                      {QStringLiteral("txPermitted"), window->capabilities().txPermitted},
                      {QStringLiteral("txWatchPathVersion"),
                       window->capabilities().txWatchPathVersion},
                      {QStringLiteral("watchReady"), window->transmitWatchReady()}};
    if (const auto* relay = qobject_cast<const DataChannelTransport*>(window->transport())) {
        probe.insert(QStringLiteral("watchGrantPresent"), relay->watchRelayGrant().has_value());
        probe.insert(QStringLiteral("watchRoute"), relay->hasWatchRelayRoute());
        probe.insert(QStringLiteral("watchAdmission"), relay->canOpenWatchRelay());
        probe.insert(QStringLiteral("primaryPinned"), relay->peerCertificateSha256().size() == 32);
    }
    return probe;
}

// The fake speaker advances on the session thread. Read each 10 ms block
// once, so media readiness and post-unkey recovery do not scan the whole
// recording on every timer tick.
bool heardAudibleBlock(PacedAudioBus* bus, int& nextSample)
{
    if (bus == nullptr) {
        return false;
    }
    const QVector<float>& heard = bus->heard;
    while (nextSample + 960 <= heard.size()) {
        double energy = 0.0;
        for (int i = 0; i < 480; ++i) {
            const double sample = heard.at(nextSample + i * 2);
            energy += sample * sample;
        }
        nextSample += 960;
        if (energy / 480.0 >= 1e-6) {
            return true;
        }
    }
    return false;
}

struct KeyedAudibility {
    int fullSeconds = 0;
    int audibleSeconds = 0;
    int audibleBlocks = 0;
    int longestSilentMs = 0;
};

KeyedAudibility keyedAudibility(PacedAudioBus* bus, int startSample, int endSample)
{
    KeyedAudibility result;
    if (!bus || endSample <= startSample) { return result; }
    const QVector<float>& heard = bus->heard;
    const int end = std::min(endSample, int(heard.size()));
    int silentBlocks = 0;
    int blocks = 0;
    bool audibleThisSecond = false;
    for (int sample = startSample; sample + 960 <= end; sample += 960) {
        double energy = 0.0;
        for (int frame = 0; frame < 480; ++frame) {
            const double value = heard.at(sample + frame * 2);
            energy += value * value;
        }
        const bool audible = energy / 480.0 >= 1e-6;
        result.audibleBlocks += audible ? 1 : 0;
        audibleThisSecond |= audible;
        silentBlocks = audible ? 0 : silentBlocks + 1;
        result.longestSilentMs = std::max(result.longestSilentMs, silentBlocks * 10);
        if (++blocks % 100 == 0) {
            ++result.fullSeconds;
            result.audibleSeconds += audibleThisSecond ? 1 : 0;
            audibleThisSecond = false;
        }
    }
    return result;
}

// Plan Task 29: real media over a session, counting what decodes: a
// MediaPeer answering the Core's offer, audio on, one display endpoint.
class MediaCounter : public QObject {
public:
    MediaCounter(StationClient* window, RadioModel* model, QObject* parent)
        : QObject(parent), m_window(window), m_model(model)
    {
        m_connectionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_peer = new MediaPeer(this);
        QObject::connect(m_peer, &MediaPeer::controlReady, this, [this](const QJsonObject& c) {
            m_window->sendMediaControl(c, m_window->sessionEpoch());
        });
        QObject::connect(m_window, &StationClient::mediaControlReceived, this,
                         [this](const QJsonObject& payload, quint32) { receive(payload); });
        QObject::connect(m_peer, &MediaPeer::ready, this, [this] { onReady(); });
        QObject::connect(m_peer, &MediaPeer::rtpReceived, this, [this](const QByteArray& packet) {
            ++audioPackets;
            if (m_audioSsrc != 0
                && m_decoder.decodeRtp(packet, m_audioSsrc).status
                    == OpusAudioCodecStatus::Accepted) {
                ++audioDecoded;
            }
        });
        QObject::connect(m_peer, &MediaPeer::displayReceived, this,
                         [this](const QByteArray& packet) {
            ++displayMessages;
            if (packet.startsWith("NSDC")
                && m_display.decode(packet).disposition == DisplayCodecDisposition::Accepted) {
                ++displayDecoded;
            }
        });
    }

    bool start()
    {
        // Plan Task 29 step 2b: on a direct WebSocket session to a Core that
        // carries it, the media tunnel too (as RemoteMediaController).
        const bool tunnel = m_window->mediaTunnelAvailable();
        const bool routed = m_window->capabilities().mediaRelayRoutingVersion >= 1;
        auto ice = tunnel ? m_window->mediaTunnelIceConfiguration() : std::nullopt;
        if (!ice) {
            ice = m_window->sessionIceConfiguration();
            if (ice) { ice->setMediaRouting(routed); }
        }
        m_peer->setIceConfiguration(ice);
        if (!m_peer->start(IMediaTransport::Role::Answerer, m_connectionId)) {
            return false;
        }
        QJsonObject start{{QStringLiteral("op"), QStringLiteral("start")},
                          {QStringLiteral("connectionId"), m_connectionId}};
        if (tunnel) {
            start.insert(QStringLiteral("mediaTunnelVersion"), 1);
        }
        if (routed) {
            start.insert(QStringLiteral("mediaRelayRoutingVersion"), 1);
        }
        return m_window->sendMediaControl(start, m_window->sessionEpoch());
    }
    MediaPeer* peer() const { return m_peer; }

    quint64 audioPackets = 0;
    quint64 audioDecoded = 0;
    quint64 displayMessages = 0;
    quint64 displayDecoded = 0;

private:
    void receive(const QJsonObject& payload)
    {
        if (payload.value(QStringLiteral("connectionId")).toString() != m_connectionId) {
            return;
        }
        const QString op = payload.value(QStringLiteral("op")).toString();
        if (op == QLatin1String("description") || op == QLatin1String("candidate")) {
            m_peer->acceptControl(payload);
        } else if (op == QLatin1String("audio-context")
                   && payload.value(QStringLiteral("enabled")).toBool()) {
            m_audioSsrc = static_cast<quint32>(payload.value(QStringLiteral("ssrc")).toDouble());
        }
    }

    void onReady()
    {
        // Plan Task 29 step 2b: a known plaintext on the media connection's
        // "tx" channel, which the harness's front looks for in every
        // datagram the web relay carries (it must never find it: DTLS).
        auto* marker = new QTimer(this);
        auto count = std::make_shared<int>(0);
        QObject::connect(marker, &QTimer::timeout, this, [this, marker, count] {
            m_peer->sendTx(QByteArrayLiteral("NEREUS-PLAINTEXT-MARKER-") + QByteArray::number(*count));
            if (++*count >= 10) {
                marker->stop();
            }
        });
        marker->start(200);
        const auto send = [this](QJsonObject control) {
            control.insert(QStringLiteral("connectionId"), m_connectionId);
            m_window->sendMediaControl(control, m_window->sessionEpoch());
        };
        send({{QStringLiteral("op"), QStringLiteral("audio")},
              {QStringLiteral("revision"), 1},
              {QStringLiteral("enabled"), true}});
        const QList<SliceModel*> slices = m_model->slices();
        if (slices.isEmpty()) {
            return;
        }
        const QJsonObject plane{{QStringLiteral("detector"), 0},
                                {QStringLiteral("averageMode"), -1},
                                {QStringLiteral("averageAlpha"), 0.0}};
        send({{QStringLiteral("op"), QStringLiteral("subscribe")},
              {QStringLiteral("endpointId"), 1},
              {QStringLiteral("revision"), 1},
              {QStringLiteral("sliceId"), slices.first()->sliceIndex()},
              {QStringLiteral("tier"), QStringLiteral("wide")},
              {QStringLiteral("fftSize"), 1024},
              {QStringLiteral("windowType"), 0},
              {QStringLiteral("centreHz"), slices.first()->frequency()},
              {QStringLiteral("spanHz"), 48000.0},
              {QStringLiteral("pixels"), 128},
              {QStringLiteral("fps"), 30},
              {QStringLiteral("framesPerLine"), 1},
              {QStringLiteral("trace"), plane},
              {QStringLiteral("waterfall"), plane},
              {QStringLiteral("minDbm"), -180.0},
              {QStringLiteral("maxDbm"), 0.0},
              {QStringLiteral("wideSpanFactor"), 0.0}});
    }

    StationClient* m_window;
    RadioModel* m_model;
    MediaPeer* m_peer = nullptr;
    QString m_connectionId;
    quint32 m_audioSsrc = 0;
    OpusAudioDecoder m_decoder;
    DisplayCodecDecoder m_display;
};

// Plan Task 28: a desktop reaching the Core through the service.
int runSession(const QStringList& args)
{
    const QString dir = option(args, QStringLiteral("--dir"));
    const int timeoutMs = option(args, QStringLiteral("--timeout-ms"), QStringLiteral("120000")).toInt();
    QFile coreFile(option(args, QStringLiteral("--core")));
    const QStringList core = coreFile.open(QIODevice::ReadOnly)
        ? QString::fromLatin1(coreFile.readAll()).split(QLatin1Char(' '))
        : QStringList();
    bool ok = false;
    const QByteArray fingerprint =
        core.size() == 2 ? StationIdentity::fromBase64Url(core.at(1).trimmed(), &ok) : QByteArray();
    auto key = std::make_shared<const ClientDeviceIdentity>(ClientDeviceIdentity::loadOrCreate(dir));
    if (!key->isValid() || !ok || fingerprint.size() != 32
        || !RendezvousWire::isRendezvousId(core.value(0))) {
        std::fputs("session: an argument is not usable\n", stderr);
        return 2;
    }
    QElapsedTimer clock;
    clock.start();
    auto* model = new RadioModel(RadioModel::Role::Remote);
    auto* proxy = new SettingsProxy();
    auto* window = new StationClient(model, proxy, QCoreApplication::instance());
    window->setDeviceIdentity(key, QStringLiteral("Harness desktop"));
    // One attempt: a failure is reported, not retried.
    window->setReconnectBackoffUnitMs(3600 * 1000);
    auto done = std::make_shared<bool>(false);
    // Plan Task 29: the race, a move, media.
    const QString direct = option(args, QStringLiteral("--direct"));
    const int waitUpgradeMs = option(args, QStringLiteral("--wait-upgrade-ms"), QStringLiteral("0")).toInt();
    const int mediaMs = option(args, QStringLiteral("--media-ms"), QStringLiteral("0")).toInt();
    const QString schedule = option(args, QStringLiteral("--upgrade-schedule-ms"));
    // Plan Task 29 step 2b: TUNE keyed for this long after bounded fake
    // media readiness (the transmit deadline's measurement).
    const int tuneMs = option(args, QStringLiteral("--tune-ms"), QStringLiteral("0")).toInt();
    const bool requireWatch = args.contains(QStringLiteral("--require-watch"));
    const bool requireKeyedAudio = args.contains(QStringLiteral("--require-keyed-audio"));
    const int readyTimeoutMs = option(args, QStringLiteral("--ready-timeout-ms"),
                                      QStringLiteral("8000")).toInt();
    if (!schedule.isEmpty()) {
        QList<int> delays;
        for (const QString& part : schedule.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            delays.append(part.toInt());
        }
        window->setUpgradeScheduleForTest(delays);
    }
    auto handshakes = std::make_shared<int>(0);
    auto rankBefore = std::make_shared<int>(-1);
    auto moved = std::make_shared<bool>(false);
    auto media = std::make_shared<QPointer<MediaCounter>>();
    // Task 29 fix wave: the desktop's own media, played into a paced bus,
    // and every media connection id it used.
    const int followMs = option(args, QStringLiteral("--follow-media-ms"), QStringLiteral("0")).toInt();
    PacedAudioBus* bus = nullptr;
    QPointer<RemoteMediaController> follower;
    auto followIds = std::make_shared<QStringList>();
    struct FollowRecovery {
        QString beforeMoveId;
        QString replacementId;
        int audioStartFrame = -1;
        int audioResumeMs = -1;
        int displayResumeMs = -1;
        int displayAfterReplacement = 0;
        qint64 moveAtMs = -1;
        qint64 replacementReadyAtMs = -1;
    };
    auto followRecovery = std::make_shared<FollowRecovery>();
    auto displayPresented = std::make_shared<int>(0);
    struct TuneWindow {
        bool ready = false;
        bool watchReady = false;
        bool watchReadyAtOff = false;
        bool heardPre = false;
        bool postAudio = false;
        bool postDisplay = false;
        bool postHeard = false;
        qint64 readyAtMs = -1;
        qint64 watchReadyAtMs = -1;
        qint64 onAtMs = -1;
        qint64 coreObservedOnAtMs = -1;
        qint64 offAtMs = -1;
        qint64 postAtMs = -1;
        quint64 audioBefore = 0;
        quint64 audioAtOff = 0;
        quint64 audioAfter = 0;
        int displayBefore = 0;
        int displayAtOff = 0;
        int displayAfter = 0;
        int heardScanSample = 0;
        int keyedStartSample = -1;
        int keyedEndSample = -1;
        quint64 channelBefore = 0;
        quint64 channelAtOff = 0;
        quint64 sessionBefore = 0;
        quint64 sessionAtOff = 0;
        quint64 auxiliaryBefore = 0;
        quint64 auxiliaryAtOff = 0;
    };
    auto tuneWindow = std::make_shared<TuneWindow>();
    if (followMs > 0) {
        auto owned = std::make_unique<PacedAudioBus>();
        bus = owned.get();
        model->audioEngine()->setSpeakersBusForTest(std::move(owned));
        // The desktop's real receiver subscribes and presents one pan; the
        // Core's synthetic IQ can then prove display resumes on replacement.
        auto* stack = new PanadapterStack;
        auto* pan = stack->addPanadapter(QStringLiteral("harness"));
        stack->resize(600, 400);
        stack->show();
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                         stack, &QObject::deleteLater);
        QObject::connect(window, &StationClient::handshakeComplete, window,
                         [model, pan] {
            if (SliceModel* first = model->slices().value(0)) {
                pan->setActiveSliceIndex(first->sliceIndex());
                pan->spectrumWidget()->setDisplayWindowPreservingHistory(
                    first->frequency(), 48000);
            }
        });
        follower = new RemoteMediaController(window, model, stack, window);
        QObject::connect(follower, &RemoteMediaController::displayFrameReceived, window,
                         [follower, moved, followRecovery, displayPresented, &clock](quint32) {
            ++*displayPresented;
            if (!*moved || !follower || follower->replacePending()
                || followRecovery->replacementId.isEmpty()
                || follower->mediaConnectionId() != followRecovery->replacementId) {
                return;
            }
            ++followRecovery->displayAfterReplacement;
            if (followRecovery->displayResumeMs < 0) {
                followRecovery->displayResumeMs = int(clock.elapsed() - followRecovery->moveAtMs);
            }
        });
        auto* render = new QTimer(window);
        render->setInterval(10);
        render->setTimerType(Qt::PreciseTimer);
        QObject::connect(render, &QTimer::timeout, window, [bus] { bus->render(480); });
        render->start();
        auto* watch = new QTimer(window);
        watch->setInterval(20);
        QObject::connect(watch, &QTimer::timeout, window,
                         [follower, followIds, moved, followRecovery, bus, &clock] {
            if (follower) {
                const QString id = follower->mediaConnectionId();
                if (!id.isEmpty() && !followIds->contains(id)) {
                    followIds->append(id);
                }
                if (*moved && followRecovery->replacementId.isEmpty()
                    && !follower->replacePending() && !id.isEmpty()
                    && id != followRecovery->beforeMoveId) {
                    followRecovery->replacementId = id;
                    followRecovery->replacementReadyAtMs = clock.elapsed();
                }
                if (!followRecovery->replacementId.isEmpty()
                    && followRecovery->audioStartFrame < 0
                    && clock.elapsed() - followRecovery->replacementReadyAtMs >= 200) {
                    // Drain at most one 100 ms device ring from the old peer.
                    followRecovery->audioStartFrame = bus->heard.size() / 2;
                }
                if (followRecovery->audioStartFrame >= 0
                    && followRecovery->audioResumeMs < 0
                    && id == followRecovery->replacementId) {
                    const auto& heard = bus->heard;
                    for (int frame = followRecovery->audioStartFrame;
                         (frame + 480) * 2 <= heard.size(); frame += 480) {
                        double energy = 0.0;
                        for (int i = 0; i < 480; ++i) {
                            const double sample = heard.at((frame + i) * 2);
                            energy += sample * sample;
                        }
                        if (energy / 480.0 >= 1e-6) {
                            followRecovery->audioResumeMs =
                                int(clock.elapsed() - followRecovery->moveAtMs);
                            break;
                        }
                    }
                }
            }
        });
        watch->start();
    }
    const auto finish = [window, &clock, done, handshakes, rankBefore, moved, media, bus,
                         follower, followIds, followRecovery, displayPresented, tuneWindow,
                         requireKeyedAudio](
                            bool connected, const QString& reason) {
        if (*done) {
            return;
        }
        *done = true;
        QJsonObject result;
        result.insert(QStringLiteral("rank"), *rankBefore >= 0 ? *rankBefore : window->pathRank());
        result.insert(QStringLiteral("path"),
                      pathName(*rankBefore >= 0 ? *rankBefore : window->pathRank()));
        result.insert(QStringLiteral("attempt"), window->connectionAttempt().summary());
        result.insert(QStringLiteral("moved"), *moved);
        result.insert(QStringLiteral("rankAfter"), window->pathRank());
        result.insert(QStringLiteral("handshakes"), *handshakes);
        result.insert(QStringLiteral("switches"), window->pathSwitches());
        if (RemoteTransmitClient* transmit = window->remoteTransmit()) {
            result.insert(QStringLiteral("channelKeepalives"),
                          static_cast<double>(transmit->channelKeepalivesSent()));
            result.insert(QStringLiteral("sessionKeepalives"),
                          static_cast<double>(transmit->sessionKeepalivesSent()));
            result.insert(QStringLiteral("auxiliaryKeepalives"),
                          static_cast<double>(transmit->auxiliaryKeepalivesSent()));
        }
        if (*media) {
            result.insert(QStringLiteral("audioPackets"), static_cast<double>((*media)->audioPackets));
            result.insert(QStringLiteral("audioDecoded"), static_cast<double>((*media)->audioDecoded));
            result.insert(QStringLiteral("displayMessages"),
                          static_cast<double>((*media)->displayMessages));
            result.insert(QStringLiteral("displayDecoded"),
                          static_cast<double>((*media)->displayDecoded));
            if (const auto path = (*media)->peer()->selectedPath()) {
                result.insert(QStringLiteral("mediaRemoteAddress"), path->remoteAddress);
                result.insert(QStringLiteral("mediaViaShim"), path->viaLoopbackShim());
            }
        }
        if (bus != nullptr) {
            // From the first tone heard, the longest silent run in 10 ms
            // blocks, and how long was heard.
            const QVector<float>& heard = bus->heard;
            int first = -1;
            int run = 0;
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
                run = silent ? run + 1 : 0;
                longest = std::max(longest, run);
            }
            result.insert(QStringLiteral("mediaConnections"), static_cast<int>(followIds->size()));
            result.insert(QStringLiteral("audioAfterMoveMs"), followRecovery->audioResumeMs);
            result.insert(QStringLiteral("displayAfterMoveMs"), followRecovery->displayResumeMs);
            result.insert(QStringLiteral("displayAfterReplacement"),
                          followRecovery->displayAfterReplacement);
            result.insert(QStringLiteral("heardMs"),
                          first < 0 ? 0.0 : static_cast<double>((heard.size() / 2 - first) / 48));
            result.insert(QStringLiteral("longestSilentMs"), longest * 10);
            if (follower) {
                result.insert(QStringLiteral("audioDecoded"),
                              static_cast<double>(follower->audioTelemetry().decodedPackets));
                result.insert(QStringLiteral("displayPresented"), *displayPresented);
                result.insert(QStringLiteral("duplicatesDropped"),
                              static_cast<double>(follower->duplicateAudioDropped()));
                result.insert(QStringLiteral("replacePending"), follower->replacePending());
            }
        }
        if (tuneWindow->ready) {
            const bool unkeyed = tuneWindow->offAtMs >= 0;
            const bool postDone = tuneWindow->postAtMs >= 0;
            result.insert(QStringLiteral("tuneReadyAtMs"), static_cast<double>(tuneWindow->readyAtMs));
            result.insert(QStringLiteral("watchReadyBeforeTune"), tuneWindow->watchReady);
            result.insert(QStringLiteral("watchReadyAtOff"), tuneWindow->watchReadyAtOff);
            result.insert(QStringLiteral("watchReadyAtMs"), static_cast<double>(
                tuneWindow->watchReadyAtMs));
            result.insert(QStringLiteral("tuneKeyedMs"),
                          unkeyed ? static_cast<double>(tuneWindow->offAtMs - tuneWindow->onAtMs) : -1.0);
            if (requireKeyedAudio) {
                result.insert(QStringLiteral("tuneCoreObservedOnAtMs"),
                              static_cast<double>(tuneWindow->coreObservedOnAtMs));
                result.insert(QStringLiteral("tuneHeldAfterCoreObservedMs"),
                              unkeyed && tuneWindow->coreObservedOnAtMs >= 0
                                  ? static_cast<double>(tuneWindow->offAtMs
                                                        - tuneWindow->coreObservedOnAtMs)
                                  : -1.0);
            }
            result.insert(QStringLiteral("tunePostWindowMs"),
                          postDone ? static_cast<double>(tuneWindow->postAtMs - tuneWindow->offAtMs) : -1.0);
            result.insert(QStringLiteral("tunePreAudioDecoded"),
                          static_cast<double>(tuneWindow->audioBefore));
            result.insert(QStringLiteral("tuneKeyedAudioDecoded"),
                          unkeyed && tuneWindow->audioAtOff >= tuneWindow->audioBefore
                              ? static_cast<double>(tuneWindow->audioAtOff - tuneWindow->audioBefore) : 0.0);
            result.insert(QStringLiteral("tunePostAudioDecoded"),
                          postDone && tuneWindow->audioAfter >= tuneWindow->audioAtOff
                              ? static_cast<double>(tuneWindow->audioAfter - tuneWindow->audioAtOff) : 0.0);
            result.insert(QStringLiteral("tuneAudioCounterReset"),
                          unkeyed && tuneWindow->audioAtOff < tuneWindow->audioBefore);
            result.insert(QStringLiteral("tunePreDisplay"), tuneWindow->displayBefore);
            result.insert(QStringLiteral("tuneKeyedDisplay"),
                          unkeyed ? tuneWindow->displayAtOff - tuneWindow->displayBefore : 0);
            result.insert(QStringLiteral("tunePostDisplay"),
                          postDone ? tuneWindow->displayAfter - tuneWindow->displayAtOff : 0);
            result.insert(QStringLiteral("tunePostAudio"), tuneWindow->postAudio);
            result.insert(QStringLiteral("tunePostDisplayReady"), tuneWindow->postDisplay);
            result.insert(QStringLiteral("tunePostHeard"), tuneWindow->postHeard);
            if (requireKeyedAudio) {
                const KeyedAudibility audible = keyedAudibility(
                    bus, tuneWindow->keyedStartSample, tuneWindow->keyedEndSample);
                result.insert(QStringLiteral("tuneKeyedAudioFullSeconds"), audible.fullSeconds);
                result.insert(QStringLiteral("tuneKeyedAudioAudibleSeconds"), audible.audibleSeconds);
                result.insert(QStringLiteral("tuneKeyedAudioAudibleBlocks"), audible.audibleBlocks);
                result.insert(QStringLiteral("tuneKeyedAudioLongestSilentMs"),
                              audible.longestSilentMs);
            }
            result.insert(QStringLiteral("tuneKeyedChannelKeepalives"),
                          unkeyed ? static_cast<double>(tuneWindow->channelAtOff - tuneWindow->channelBefore) : 0.0);
            result.insert(QStringLiteral("tuneKeyedSessionKeepalives"),
                          unkeyed ? static_cast<double>(tuneWindow->sessionAtOff - tuneWindow->sessionBefore) : 0.0);
            result.insert(QStringLiteral("tuneKeyedAuxiliaryKeepalives"),
                          unkeyed ? static_cast<double>(tuneWindow->auxiliaryAtOff
                                                      - tuneWindow->auxiliaryBefore) : 0.0);
        } else {
            result.insert(QStringLiteral("tuneReadyAtMs"), -1);
        }
        result.insert(QStringLiteral("relayed"), false);
        if (const auto* transport = qobject_cast<const DataChannelTransport*>(window->transport())) {
            if (const auto path = transport->selectedPath()) {
                result.insert(QStringLiteral("relayed"), path->relayed());
                result.insert(QStringLiteral("localType"), path->localType);
                result.insert(QStringLiteral("remoteType"), path->remoteType);
                result.insert(QStringLiteral("localAddress"), path->localAddress);
                result.insert(QStringLiteral("remoteAddress"), path->remoteAddress);
            }
        }
        result.insert(QStringLiteral("connected"), connected);
        result.insert(QStringLiteral("ms"), static_cast<double>(clock.elapsed()));
        if (!reason.isEmpty()) {
            result.insert(QStringLiteral("reason"), reason);
        }
        printLine(result);
        window->disconnectFromStation(QStringLiteral("harness done"));
        // Let the close (and the relay's release) go out before leaving.
        QTimer::singleShot(1500, QCoreApplication::instance(),
                           [connected] { QCoreApplication::exit(connected ? 0 : 1); });
    };
    QObject::connect(window, &StationClient::handshakeComplete, window,
                     [window, model, finish, handshakes, rankBefore, moved, media, waitUpgradeMs,
                      mediaMs, followMs, tuneMs, follower, followRecovery, bus,
                      displayPresented, tuneWindow, done, requireWatch, requireKeyedAudio,
                      readyTimeoutMs, &clock] {
        ++*handshakes;
        if (*handshakes > 1) {
            return;
        }
        *rankBefore = window->pathRank();
        printLine(watchProbe(window));
        if (waitUpgradeMs <= 0 && mediaMs <= 0 && tuneMs <= 0) {
            finish(true, QString());
            return;
        }
        if (tuneMs > 0) {
            auto* sentTrace = new QTimer(window);
            sentTrace->setInterval(10);
            auto lastChannel = std::make_shared<quint64>(0);
            auto lastSession = std::make_shared<quint64>(0);
            QObject::connect(sentTrace, &QTimer::timeout, window,
                             [window, lastChannel, lastSession] {
                RemoteTransmitClient* transmit = window->remoteTransmit();
                if (!transmit || !transmit->keepaliveRunning()) { return; }
                const quint64 channel = transmit->channelKeepalivesSent();
                const quint64 session = transmit->sessionKeepalivesSent();
                if (channel == *lastChannel && session == *lastSession) { return; }
                *lastChannel = channel;
                *lastSession = session;
                printLine({{QStringLiteral("event"), QStringLiteral("keepaliveSent")},
                           {QStringLiteral("atEpochMs"), static_cast<double>(
                                QDateTime::currentMSecsSinceEpoch())},
                           {QStringLiteral("channel"), static_cast<double>(channel)},
                           {QStringLiteral("session"), static_cast<double>(session)}});
            });
            sentTrace->start();
            const auto decodedAudio = [follower, media]() -> quint64 {
                if (follower) { return follower->audioTelemetry().decodedPackets; }
                return *media ? (*media)->audioDecoded : 0;
            };
            const auto displayed = [follower, media, displayPresented]() -> int {
                if (follower) { return *displayPresented; }
                return *media ? int((*media)->displayDecoded) : 0;
            };
            // The sole fake RX slice is half-duplex: TUNE removes it from
            // the speaker mix. Prove reception before keying, then test
            // recovery separately after the requested keyed window.
            auto* readiness = new QTimer(window);
            readiness->setInterval(20);
            readiness->setTimerType(Qt::PreciseTimer);
            const qint64 readyDeadlineMs = clock.elapsed() + readyTimeoutMs;
            QObject::connect(readiness, &QTimer::timeout, window,
                             [window, model, readiness, readyDeadlineMs, tuneMs, finish, done,
                              tuneWindow, bus, decodedAudio, displayed, requireWatch,
                              requireKeyedAudio, &clock] {
                if (*done) { readiness->stop(); return; }
                const quint64 audio = decodedAudio();
                const int display = displayed();
                if (bus != nullptr && !tuneWindow->heardPre) {
                    tuneWindow->heardPre = heardAudibleBlock(bus, tuneWindow->heardScanSample);
                }
                if (audio > 0 && display > 0 && (bus == nullptr || tuneWindow->heardPre)
                    && (!requireWatch || window->transmitWatchReady())) {
                    readiness->stop();
                    RemoteTransmitClient* transmit = window->remoteTransmit();
                    if (transmit == nullptr) {
                        finish(true, QStringLiteral("TUNE transmitter unavailable after media readiness."));
                        return;
                    }
                    tuneWindow->ready = true;
                    tuneWindow->readyAtMs = clock.elapsed();
                    tuneWindow->watchReady = window->transmitWatchReady();
                    if (tuneWindow->watchReady) {
                        tuneWindow->watchReadyAtMs = tuneWindow->readyAtMs;
                    }
                    tuneWindow->audioBefore = audio;
                    tuneWindow->displayBefore = display;
                    tuneWindow->channelBefore = transmit->channelKeepalivesSent();
                    tuneWindow->sessionBefore = transmit->sessionKeepalivesSent();
                    tuneWindow->auxiliaryBefore = transmit->auxiliaryKeepalivesSent();
                    printLine({{QStringLiteral("event"), QStringLiteral("tuneReady")},
                               {QStringLiteral("atEpochMs"), static_cast<double>(
                                    QDateTime::currentMSecsSinceEpoch())},
                               {QStringLiteral("audioDecoded"), static_cast<double>(audio)},
                               {QStringLiteral("displayPresented"), display},
                               {QStringLiteral("watchReady"), tuneWindow->watchReady}});
                    tuneWindow->onAtMs = clock.elapsed();
                    if (bus != nullptr && !requireKeyedAudio) {
                        tuneWindow->keyedStartSample = bus->heard.size();
                    }
                    printLine({{QStringLiteral("event"), QStringLiteral("tune")},
                               {QStringLiteral("atEpochMs"), static_cast<double>(
                                    QDateTime::currentMSecsSinceEpoch())},
                               {QStringLiteral("on"), true}});
                    auto released = std::make_shared<bool>(false);
                    const auto releaseTune = [window, finish, done, released, tuneWindow, bus,
                                              decodedAudio, displayed, &clock](const QString& fault) {
                        if (*done || *released) { return; }
                        *released = true;
                        tuneWindow->offAtMs = clock.elapsed();
                        if (bus != nullptr) { tuneWindow->keyedEndSample = bus->heard.size(); }
                        tuneWindow->watchReadyAtOff = window->transmitWatchReady();
                        tuneWindow->audioAtOff = decodedAudio();
                        tuneWindow->displayAtOff = displayed();
                        if (bus != nullptr) {
                            tuneWindow->heardScanSample = bus->heard.size();
                        }
                        if (RemoteTransmitClient* again = window->remoteTransmit()) {
                            tuneWindow->channelAtOff = again->channelKeepalivesSent();
                            tuneWindow->sessionAtOff = again->sessionKeepalivesSent();
                            tuneWindow->auxiliaryAtOff = again->auxiliaryKeepalivesSent();
                            again->setTune(false);
                        }
                        printLine({{QStringLiteral("event"), QStringLiteral("tune")},
                                   {QStringLiteral("atEpochMs"), static_cast<double>(
                                        QDateTime::currentMSecsSinceEpoch())},
                                   {QStringLiteral("on"), false}});
                        QTimer::singleShot(5000, Qt::PreciseTimer, window,
                                           [finish, done, tuneWindow, bus,
                                            decodedAudio, displayed, fault, &clock] {
                            if (*done) { return; }
                            tuneWindow->postAtMs = clock.elapsed();
                            tuneWindow->audioAfter = decodedAudio();
                            tuneWindow->displayAfter = displayed();
                            tuneWindow->postAudio = tuneWindow->audioAfter > tuneWindow->audioAtOff;
                            tuneWindow->postDisplay = tuneWindow->displayAfter > tuneWindow->displayAtOff;
                            tuneWindow->postHeard = bus == nullptr
                                || heardAudibleBlock(bus, tuneWindow->heardScanSample);
                            const bool recovered = tuneWindow->postAudio
                                && tuneWindow->postDisplay && tuneWindow->postHeard;
                            finish(true, !fault.isEmpty() ? fault
                                          : recovered ? QString()
                                                      : QStringLiteral("Post-unkey media did not recover."));
                        });
                    };
                    if (requireKeyedAudio) {
                        // txState is the authenticated Core-owned state mirror.
                        // A command answer can precede its actual keyed state;
                        // begin the hold only after both keyed and tuning rise.
                        TransmitState* mirror = model->stationTransmitState();
                        if (!mirror || window->capabilities().txStateVersion < 1
                            || mirror->keyed()) {
                            finish(true, QStringLiteral("Core transmit state unavailable before TUNE."));
                            return;
                        }
                        auto observed = std::make_shared<bool>(false);
                        const auto observeOn = [window, mirror, observed, tuneWindow,
                                                releaseTune, bus, tuneMs, &clock] {
                            if (*observed || !mirror->keyed() || !mirror->tuning()) { return; }
                            *observed = true;
                            tuneWindow->coreObservedOnAtMs = clock.elapsed();
                            if (bus != nullptr) {
                                tuneWindow->keyedStartSample = bus->heard.size();
                            }
                            printLine({{QStringLiteral("event"), QStringLiteral("tuneCoreObservedOn")},
                                       {QStringLiteral("atEpochMs"), static_cast<double>(
                                            QDateTime::currentMSecsSinceEpoch())}});
                            QTimer::singleShot(tuneMs, Qt::PreciseTimer, window,
                                               [releaseTune] { releaseTune({}); });
                        };
                        QObject::connect(mirror, &TransmitState::stateChanged, window, observeOn);
                        // Even a lost state observation must end the logical
                        // key. This bound is fixture cleanup, not a deadline
                        // or production heartbeat change.
                        QTimer::singleShot(tuneMs + 5000, Qt::PreciseTimer, window,
                                           [released, releaseTune] {
                            if (!*released) {
                                releaseTune(QStringLiteral("Core TX-on state was not observed in time."));
                            }
                        });
                        transmit->setTune(true);
                        observeOn();
                    } else {
                        transmit->setTune(true);
                        QTimer::singleShot(tuneMs, Qt::PreciseTimer, window,
                                           [releaseTune] { releaseTune({}); });
                    }
                    return;
                }
                if (clock.elapsed() >= readyDeadlineMs) {
                    readiness->stop();
                    printLine(watchProbe(window));
                    finish(true, QStringLiteral("Media or independent transmit watch was not ready before synthetic TUNE."));
                }
            });
            readiness->start();
        }
        // Tell the harness the session is up, then carry on.
        printLine({{QStringLiteral("event"), QStringLiteral("connected")},
                   {QStringLiteral("rank"), window->pathRank()},
                   {QStringLiteral("path"), pathName(window->pathRank())}});
        if (waitUpgradeMs > 0) {
            QObject::connect(window, &StationClient::pathChanged, window,
                             [window, finish, moved, mediaMs, followMs,
                              follower, followRecovery, &clock] {
                if (!*moved && follower) {
                    followRecovery->beforeMoveId = follower->mediaConnectionId();
                    followRecovery->moveAtMs = clock.elapsed();
                }
                *moved = true;
                printLine({{QStringLiteral("event"), QStringLiteral("moved")},
                           {QStringLiteral("rank"), window->pathRank()}});
                if (followMs > 0) {
                    // Listen on after the move: media follows it.
                    QTimer::singleShot(followMs, window, [finish] { finish(true, QString()); });
                } else if (mediaMs <= 0) {
                    finish(true, QString());
                }
            });
            QTimer::singleShot(waitUpgradeMs, window, [finish, mediaMs, moved, followMs] {
                if (mediaMs <= 0 && (followMs <= 0 || !*moved)) {
                    finish(true, QString());
                }
            });
        }
        if (mediaMs > 0) {
            *media = new MediaCounter(window, model, window);
            if (!(*media)->start()) {
                finish(true, QStringLiteral("Media could not start."));
                return;
            }
            if (tuneMs <= 0) {
                QTimer::singleShot(mediaMs + std::max(0, waitUpgradeMs), window,
                                   [finish] { finish(true, QString()); });
            }
        }
    });
    QObject::connect(window, &StationClient::sessionEnded, window,
                     [finish](const QString& reason) { finish(false, reason); });
    QTimer::singleShot(timeoutMs, window, [finish] {
        finish(false, QStringLiteral("No session in time."));
    });
    const QList<QUrl> servers =
        RendezvousClient::serverUrls({option(args, QStringLiteral("--server"))});
    if (direct.isEmpty()) {
        window->connectThroughService(servers, core.value(0), fingerprint);
    } else {
        // Plan Task 29: the race, the Core's address and the service at
        // once.
        StationClient::ServiceRoute route;
        route.servers = servers;
        route.rendezvousId = core.value(0);
        route.relayAllowed = true;
        route.controlChannelVersion = 1;
        window->setServiceRoute(route);
        window->connectToStation(QUrl(direct), QString(), QString(), false, fingerprint);
    }
    return QCoreApplication::exec();
}

} // namespace

int main(int argc, char** argv)
{
    const bool following = argc > 1 && QByteArray(argv[1]) == "session"
        && [&] {
            for (int i = 2; i < argc; ++i) {
                if (QByteArray(argv[i]) == "--follow-media-ms") { return true; }
            }
            return false;
        }();
    if (following) { qputenv("QT_QPA_PLATFORM", "offscreen"); }
    std::unique_ptr<QCoreApplication> app = following
        ? std::unique_ptr<QCoreApplication>(new QApplication(argc, argv))
        : std::unique_ptr<QCoreApplication>(new QCoreApplication(argc, argv));
    const QStringList args = app->arguments();
    const QString mode = args.value(1);
    if (!addAuthority(option(args, QStringLiteral("--ca")))) {
        std::fputs("the certificate authority could not be read\n", stderr);
        return 2;
    }
    if (mode == QLatin1String("key")) {
        const StationIdentity key = StationIdentity::loadOrCreate(option(args, QStringLiteral("--dir")));
        if (!key.isValid()) {
            return 2;
        }
        QTextStream(stdout) << StationIdentity::toBase64Url(key.publicKeySpki()) << "\n";
        return 0;
    }
    if (mode == QLatin1String("device-key")) {
        const ClientDeviceIdentity key =
            ClientDeviceIdentity::loadOrCreate(option(args, QStringLiteral("--dir")));
        if (!key.isValid()) {
            return 2;
        }
        QTextStream(stdout) << StationIdentity::toBase64Url(key.publicKeySpki()) << "\n";
        return 0;
    }
    if (mode == QLatin1String("core")) {
        return runCore(args);
    }
    if (mode == QLatin1String("session")) {
        return runSession(args);
    }
    if (mode == QLatin1String("station")) {
        return runStation(args);
    }
    if (mode == QLatin1String("client")) {
        return runClient(args);
    }
    std::fputs("usage: nereus_rendezvous_peer key|device-key|station|client|core|session ...\n",
               stderr);
    return 2;
}
