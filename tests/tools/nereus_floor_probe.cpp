// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/nereus_floor_probe.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 Step 1 (R-IOS-16): the measuring end of the relay
// floor comparison (docs/architecture/2026-09-23-relay-floor-measurement.md),
// run by tests/scripts/floor-measurement.sh inside the traversal harness.
// One media connection (LibDataChannelMediaTransport, the Core's own) is
// introduced through the service exactly as the traversal peer does, and
// then reaches the far end by one of:
//
//   none  ICE as it is today (host, server reflexive, TURN over UDP)
//   c     option (C): TURN over TLS through TurnTlsShimPrototype
//   e     option (E): the service's WebSocket relay (WsRelayPrototype)
//
// Option (B), libjuice patched to speak TURN over TCP and TLS itself, puts
// the same bytes on the same wire to the same listener as (C) without the
// loopback hop, so (C)'s numbers stand for both (see the document).
//
// Traffic, both directions, while connected:
//   Core to device  Opus-shaped RTP every 20 ms (72 bytes, the SSRC the
//                   answer declares) and a display frame on the display
//                   channel 30 times a second, sized so the two make
//                   --rate kbit/s of payload;
//   device to Core  a 32-byte keyed event on the "tx" channel every 100 ms
//                   (RemoteTransmitClient::kKeepaliveIntervalMs).
// Each receiver keeps the arrival time of everything and reports the
// inter-arrival p50, p95, p99 and maximum, what arrived of what was sent,
// the cold time to the first audio packet, and its own CPU time.
//
//   nereus_floor_probe core --dir DIR --server URL --paired KEY --floor F
//                           [--relay-url URL] [--relay allow|deny]
//                           [--report-after-s S] [--ca FILE] [--id-file FILE]
//   nereus_floor_probe device --dir DIR --server URL --station-id ID
//                           --floor F [--turn-tls HOST:PORT]
//                           [--relay-url URL] [--rate KBPS] [--duration-s S]
//                           [--reset-at-s S] [--ca FILE]
//
// The device prints one JSON line with its measurements and exits 0 when
// audio arrived. The Core prints one JSON line for the keyed events of each
// connection when the device says it is done or the connection ends.
//
// Every display frame carries the text kPlaintextMarker, so a relay that
// could read what it carries would find it; the prototype relay counts
// frames that contain it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTimer>
#include <QtEndian>

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "core/security/StationIdentity.h"
#include "core/session/IceConfiguration.h"
#include "core/session/RendezvousClient.h"
#include "core/session/RendezvousWire.h"
#include "core/session/media/LibDataChannelMediaTransport.h"

#include "floor/TurnTlsShimPrototype.h"
#include "floor/WsRelayPrototype.h"

using namespace NereusSDR;
using namespace NereusSDR::FloorPrototype;

namespace {

constexpr quint32 kCoreSsrc = 0x5a5a;
constexpr quint32 kDeviceSsrc = 0xa5a5;
constexpr int kAudioIntervalMs = 20;
constexpr int kAudioPacketBytes = 72;
constexpr int kDisplayFramesPerSecond = 30;
constexpr int kKeyedIntervalMs = 100;
constexpr int kKeyedEventBytes = 32;
constexpr char kPlaintextMarker[] = "NEREUS-FLOOR-PLAINTEXT-MARKER";
const QByteArray kDoneTag("NEREUS-FLOOR-DONE");

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

// Arrival times are wall-clock ms: both ends run on one computer (the
// harness's namespaces share its clock), so the Core can time the device's
// cold start and its reset.
double epochMs()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(now).count())
           / 1000.0;
}

double cpuSeconds()
{
    rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    return static_cast<double>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec)
           + static_cast<double>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1e6;
}

// Arrival times of one stream, in ms from the probe's start.
struct Arrivals {
    std::vector<double> times;
    quint64 sent = 0;

    void add(double ms) { times.push_back(ms); }

    // Inter-arrival statistics over [fromMs, toMs).
    QJsonObject summary(double fromMs = -1.0, double toMs = 1e18) const
    {
        std::vector<double> gaps;
        double previous = -1.0;
        quint64 count = 0;
        for (double t : times) {
            if (t < fromMs || t >= toMs) {
                continue;
            }
            ++count;
            if (previous >= 0.0) {
                gaps.push_back(t - previous);
            }
            previous = t;
        }
        QJsonObject object;
        object.insert(QStringLiteral("received"), static_cast<double>(count));
        if (sent != 0) {
            object.insert(QStringLiteral("sent"), static_cast<double>(sent));
        }
        if (gaps.empty()) {
            return object;
        }
        std::sort(gaps.begin(), gaps.end());
        const auto at = [&gaps](double q) {
            const std::size_t index = std::min(gaps.size() - 1,
                                               static_cast<std::size_t>(q * static_cast<double>(gaps.size())));
            return gaps[index];
        };
        object.insert(QStringLiteral("p50"), at(0.50));
        object.insert(QStringLiteral("p95"), at(0.95));
        object.insert(QStringLiteral("p99"), at(0.99));
        object.insert(QStringLiteral("max"), gaps.back());
        return object;
    }

    // The longest gap that starts at or after fromMs.
    double largestGapAfter(double fromMs) const
    {
        double previous = -1.0;
        double largest = 0.0;
        for (double t : times) {
            if (previous >= 0.0 && t >= fromMs) {
                largest = std::max(largest, t - previous);
            }
            previous = t;
        }
        return largest;
    }
};

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

QString iceUfrag(const QString& sdp)
{
    static const QRegularExpression pattern(QStringLiteral("a=ice-ufrag:([^\\r\\n]+)"));
    const QRegularExpressionMatch match = pattern.match(sdp);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

// The port of a host candidate line, 0 for any other.
quint16 hostCandidatePort(const QString& candidate)
{
    const QStringList parts = candidate.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const qsizetype typ = parts.indexOf(QStringLiteral("typ"));
    if (typ < 2 || typ + 1 >= parts.size() || parts.at(typ + 1) != QLatin1String("host")) {
        return 0;
    }
    return static_cast<quint16>(parts.at(typ - 1).toUInt());
}

QByteArray rtpPacket(quint16 sequence, quint32 timestamp, quint32 ssrc)
{
    QByteArray packet(kAudioPacketBytes, '\0');
    auto* bytes = reinterpret_cast<uchar*>(packet.data());
    bytes[0] = 0x80;
    bytes[1] = 111;
    qToBigEndian<quint16>(sequence, bytes + 2);
    qToBigEndian<quint32>(timestamp, bytes + 4);
    qToBigEndian<quint32>(ssrc, bytes + 8);
    for (int index = 12; index < kAudioPacketBytes; ++index) {
        bytes[index] = static_cast<uchar>(index * 13 + sequence);
    }
    return packet;
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

// ── The Core ─────────────────────────────────────────────────────────

// What the device's done message carries (kDoneTag then five big-endian
// fields): when it started, when it reset its floor connection (0: never),
// and how many audio packets, display frames and keyed events it sent.
struct DoneMessage {
    qint64 startEpochMs = 0;
    qint64 resetEpochMs = 0;
    quint32 audioSent = 0;
    quint32 displaySent = 0;
    quint32 keyedSent = 0;

    QByteArray encode() const
    {
        QByteArray message(kDoneTag);
        message.resize(kDoneTag.size() + 28);
        auto* bytes = reinterpret_cast<uchar*>(message.data()) + kDoneTag.size();
        qToBigEndian<qint64>(startEpochMs, bytes);
        qToBigEndian<qint64>(resetEpochMs, bytes + 8);
        qToBigEndian<quint32>(audioSent, bytes + 16);
        qToBigEndian<quint32>(displaySent, bytes + 20);
        qToBigEndian<quint32>(keyedSent, bytes + 24);
        return message;
    }

    static std::optional<DoneMessage> decode(const QByteArray& message)
    {
        if (!message.startsWith(kDoneTag) || message.size() != kDoneTag.size() + 28) {
            return std::nullopt;
        }
        const auto* bytes = reinterpret_cast<const uchar*>(message.constData()) + kDoneTag.size();
        DoneMessage done;
        done.startEpochMs = qFromBigEndian<qint64>(bytes);
        done.resetEpochMs = qFromBigEndian<qint64>(bytes + 8);
        done.audioSent = qFromBigEndian<quint32>(bytes + 16);
        done.displaySent = qFromBigEndian<quint32>(bytes + 20);
        done.keyedSent = qFromBigEndian<quint32>(bytes + 24);
        return done;
    }
};

int runCore(const QStringList& args)
{
    const StationIdentity key = StationIdentity::loadOrCreate(option(args, QStringLiteral("--dir")));
    bool ok = false;
    const QByteArray paired = StationIdentity::fromBase64Url(option(args, QStringLiteral("--paired")), &ok);
    const QString floor = option(args, QStringLiteral("--floor"), QStringLiteral("none"));
    const QUrl relayUrl(option(args, QStringLiteral("--relay-url")));
    const bool relayAllowed = option(args, QStringLiteral("--relay"), QStringLiteral("allow"))
                              != QLatin1String("deny");
    // A report this long after connecting when the device's done message
    // has not come (a floor connection that did not survive a reset).
    const int reportAfterMs = option(args, QStringLiteral("--report-after-s"), QStringLiteral("0")).toInt() * 1000;
    if (!key.isValid() || !ok || !StationIdentity::isP256Spki(paired)) {
        std::fputs("core: an argument is not usable\n", stderr);
        return 2;
    }
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
    auto stunFamilies = std::make_shared<HostFamilies>();
    QObject::connect(client, &RendezvousClient::connected, client, [client, stunFamilies] {
        IceConfiguration::resolveHostFamilies(
            IceConfiguration::hostNames(client->stunUrls()), client,
            [stunFamilies](const HostFamilies& families) { *stunFamilies = families; });
    });
    const double cpuAtStart = cpuSeconds();
    QObject::connect(client, &RendezvousClient::introduced, client,
                     [client, stunFamilies, floor, relayUrl, relayAllowed, reportAfterMs,
                      cpuAtStart](const RendezvousIntroduction& introduction) {
        const QByteArray id = introduction.id;
        const double introducedAt = epochMs();
        const double cpuAtIntroduction = cpuSeconds();
        auto ice = std::make_shared<IceConfiguration>(IceConfiguration::throughRendezvous(
            client->stunUrls(), relayAllowed, IceConfiguration::localAddressFamilies(),
            *stunFamilies));
        auto* answerer = new LibDataChannelMediaTransport(client);
        WsRelayPrototype* relay = nullptr;
        if (floor == QLatin1String("e")) {
            relay = new WsRelayPrototype(relayUrl, iceUfrag(introduction.offer),
                                         WsRelayPrototype::Role::Core, answerer);
            relay->start();
        }
        QObject::connect(answerer, &IMediaTransport::localDescription, client,
                         [client, id, answerer, relayAllowed](const QString& sdp, const QString&) {
            client->answer(id, sdp);
            if (!relayAllowed) {
                answerer->gatherCandidates({});
            }
        });
        QObject::connect(answerer, &IMediaTransport::localCandidate, client,
                         [client, id, relay](const QString& candidate, const QString&) {
            if (relay != nullptr) {
                if (const quint16 port = hostCandidatePort(candidate)) {
                    relay->setAgentPort(port);
                }
            }
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
            const std::optional<RendezvousWire::Turn> relayTurn =
                offered ? std::optional<RendezvousWire::Turn>(turn) : std::nullopt;
            IceConfiguration::resolveHostFamilies(
                relayTurn ? IceConfiguration::hostNames(relayTurn->urls) : QStringList(), answerer,
                [answerer, ice, relayTurn](const HostFamilies& families) {
                    ice->addHostFamilies(families);
                    ice->setRelay(relayTurn, 1);
                    answerer->gatherCandidates(ice->relayServers());
                });
        });
        QObject::connect(client, &RendezvousClient::candidateReceived, answerer,
                         [answerer, id](const QByteArray& from, const QString& candidate) {
            if (from == id && !candidate.isEmpty()) {
                answerer->acceptCandidate(candidate, QString());
            }
        });

        auto audio = std::make_shared<Arrivals>();
        auto display = std::make_shared<Arrivals>();
        auto keyed = std::make_shared<Arrivals>();
        auto reported = std::make_shared<bool>(false);
        const auto report = [=](const QString& why, const std::optional<DoneMessage>& done) {
            if (*reported) {
                return;
            }
            *reported = true;
            QJsonObject line = pathObject(*answerer);
            line.insert(QStringLiteral("event"), QStringLiteral("report"));
            line.insert(QStringLiteral("floor"), floor);
            line.insert(QStringLiteral("why"), why);
            const double from = audio->times.empty() ? 0.0 : audio->times.front();
            if (done) {
                audio->sent = done->audioSent;
                display->sent = done->displaySent;
                keyed->sent = done->keyedSent;
                line.insert(QStringLiteral("coldFirstAudioMs"),
                            audio->times.empty() ? -1.0
                                                 : audio->times.front()
                                                       - static_cast<double>(done->startEpochMs));
                if (done->resetEpochMs != 0) {
                    const double resetAt = static_cast<double>(done->resetEpochMs);
                    QJsonObject reset;
                    reset.insert(QStringLiteral("largestAudioGapMs"), audio->largestGapAfter(resetAt));
                    reset.insert(QStringLiteral("largestDisplayGapMs"),
                                 display->largestGapAfter(resetAt));
                    reset.insert(QStringLiteral("largestKeyedGapMs"), keyed->largestGapAfter(resetAt));
                    double firstAfter = -1.0;
                    for (double t : audio->times) {
                        if (t > resetAt) {
                            firstAfter = t - resetAt;
                            break;
                        }
                    }
                    reset.insert(QStringLiteral("firstAudioAfterMs"), firstAfter);
                    reset.insert(QStringLiteral("audioAfter"), audio->summary(resetAt + 5000.0));
                    line.insert(QStringLiteral("reset"), reset);
                }
            }
            if (!audio->times.empty()) {
                line.insert(QStringLiteral("firstAudioEpochMs"), audio->times.front());
                line.insert(QStringLiteral("lastAudioEpochMs"), audio->times.back());
            }
            line.insert(QStringLiteral("audio"), audio->summary(from));
            line.insert(QStringLiteral("display"), display->summary(from));
            line.insert(QStringLiteral("keyed"), keyed->summary(from));
            line.insert(QStringLiteral("cpuSecondsSession"), cpuSeconds() - cpuAtIntroduction);
            line.insert(QStringLiteral("cpuSecondsTotal"), cpuSeconds() - cpuAtStart);
            line.insert(QStringLiteral("sessionSeconds"), (epochMs() - introducedAt) / 1000.0);
            printLine(line);
        };
        QObject::connect(answerer, &IMediaTransport::rtpReceived, answerer,
                         [audio](const QByteArray&) { audio->add(epochMs()); });
        QObject::connect(answerer, &IMediaTransport::displayReceived, answerer,
                         [display](const QByteArray&) { display->add(epochMs()); });
        QObject::connect(answerer, &IMediaTransport::txReceived, answerer,
                         [keyed, report](const QByteArray& message) {
            if (const std::optional<DoneMessage> done = DoneMessage::decode(message)) {
                report(QStringLiteral("done"), done);
                return;
            }
            keyed->add(epochMs());
        });
        QObject::connect(answerer, &IMediaTransport::closed, answerer,
                         [report] { report(QStringLiteral("closed"), std::nullopt); });
        QObject::connect(answerer, &IMediaTransport::connectionFailed, answerer,
                         [report](const QString&) { report(QStringLiteral("failed"), std::nullopt); });
        QObject::connect(answerer, &IMediaTransport::ready, answerer, [answerer, report, reportAfterMs] {
            QJsonObject event = pathObject(*answerer);
            event.insert(QStringLiteral("event"), QStringLiteral("connected"));
            printLine(event);
            if (reportAfterMs > 0) {
                QTimer::singleShot(reportAfterMs, answerer,
                                   [report] { report(QStringLiteral("timer"), std::nullopt); });
            }
        });
        IMediaTransport::StartOptions options{IMediaTransport::Role::Answerer, kCoreSsrc};
        options.ice = *ice;
        options.txChannel = true;
        if (!answerer->start(options)
            || !answerer->acceptDescription(introduction.offer, QStringLiteral("offer"))) {
            printLine({{QStringLiteral("event"), QStringLiteral("refused")}});
        }
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

// ── The device ───────────────────────────────────────────────────────

int runDevice(const QStringList& args)
{
    const StationIdentity key = StationIdentity::loadOrCreate(option(args, QStringLiteral("--dir")));
    const QString stationId = option(args, QStringLiteral("--station-id"));
    const QString floor = option(args, QStringLiteral("--floor"), QStringLiteral("none"));
    const QUrl relayUrl(option(args, QStringLiteral("--relay-url")));
    const QString turnTls = option(args, QStringLiteral("--turn-tls"));
    const int rateKbps = option(args, QStringLiteral("--rate"), QStringLiteral("145")).toInt();
    const int durationMs = option(args, QStringLiteral("--duration-s"), QStringLiteral("60")).toInt() * 1000;
    const int resetAtMs = option(args, QStringLiteral("--reset-at-s"), QStringLiteral("0")).toInt() * 1000;
    const int connectTimeoutMs = 90000;
    if (!key.isValid() || !RendezvousWire::isRendezvousId(stationId) || durationMs < 1000
        || rateKbps < 40 || (floor == QLatin1String("c") && !turnTls.contains(QLatin1Char(':')))
        || (floor == QLatin1String("e") && !relayUrl.isValid())) {
        std::fputs("device: an argument is not usable\n", stderr);
        return 2;
    }
    // Payload bytes of one display frame: what the rate leaves after audio.
    const int audioBitsPerSecond = kAudioPacketBytes * 8 * (1000 / kAudioIntervalMs);
    const int displayFrameBytes = std::max(
        64, (rateKbps * 1000 - audioBitsPerSecond) / 8 / kDisplayFramesPerSecond);

    const double startEpoch = epochMs();
    const double cpuAtStart = cpuSeconds();
    auto* client = new RendezvousClient(QCoreApplication::instance());
    client->setServers(RendezvousClient::serverUrls({option(args, QStringLiteral("--server"))}));
    auto ice = std::make_shared<IceConfiguration>();
    auto* offerer = new LibDataChannelMediaTransport(client);

    TurnTlsShimPrototype* shim = nullptr;
    if (floor == QLatin1String("c")) {
        shim = new TurnTlsShimPrototype(
            turnTls.section(QLatin1Char(':'), 0, 0),
            static_cast<quint16>(turnTls.section(QLatin1Char(':'), 1).toUInt()), offerer);
    }
    auto relay = std::make_shared<WsRelayPrototype*>(nullptr);

    auto done = std::make_shared<DoneMessage>();
    done->startEpochMs = static_cast<qint64>(startEpoch);
    auto readyAt = std::make_shared<double>(-1.0);
    auto failedAt = std::make_shared<double>(-1.0);
    auto finished = std::make_shared<bool>(false);
    auto* audioTimer = new QTimer(offerer);
    audioTimer->setTimerType(Qt::PreciseTimer);
    audioTimer->setInterval(kAudioIntervalMs);
    auto* displayTimer = new QTimer(offerer);
    displayTimer->setTimerType(Qt::PreciseTimer);
    displayTimer->setInterval(1000 / kDisplayFramesPerSecond);
    auto* keyedTimer = new QTimer(offerer);
    keyedTimer->setTimerType(Qt::PreciseTimer);
    keyedTimer->setInterval(kKeyedIntervalMs);

    const auto finish = [=](const QString& reason) {
        if (*finished) {
            return;
        }
        *finished = true;
        audioTimer->stop();
        displayTimer->stop();
        keyedTimer->stop();
        // The tx channel never retransmits: say it a few times.
        const QByteArray message = done->encode();
        for (int index = 0; index < 5; ++index) {
            QTimer::singleShot(index * 30, offerer, [offerer, message] { offerer->sendTx(message); });
        }
        QJsonObject result = pathObject(*offerer);
        result.insert(QStringLiteral("event"), QStringLiteral("device"));
        result.insert(QStringLiteral("floor"), floor);
        result.insert(QStringLiteral("connected"), *readyAt >= 0.0);
        result.insert(QStringLiteral("readyMs"), *readyAt);
        result.insert(QStringLiteral("failedMs"), *failedAt);
        result.insert(QStringLiteral("resetEpochMs"), static_cast<double>(done->resetEpochMs));
        result.insert(QStringLiteral("rateKbps"), rateKbps);
        result.insert(QStringLiteral("displayFrameBytes"), displayFrameBytes);
        result.insert(QStringLiteral("audioSent"), static_cast<double>(done->audioSent));
        result.insert(QStringLiteral("displaySent"), static_cast<double>(done->displaySent));
        result.insert(QStringLiteral("keyedSent"), static_cast<double>(done->keyedSent));
        result.insert(QStringLiteral("cpuSeconds"), cpuSeconds() - cpuAtStart);
        result.insert(QStringLiteral("wallSeconds"), (epochMs() - startEpoch) / 1000.0);
        if (shim != nullptr) {
            result.insert(QStringLiteral("floorConnections"), shim->connections());
        }
        if (*relay != nullptr) {
            result.insert(QStringLiteral("floorConnections"), (*relay)->connections());
        }
        if (!reason.isEmpty()) {
            result.insert(QStringLiteral("reason"), reason);
        }
        printLine(result);
        // Let the done messages and the close go out.
        QTimer::singleShot(1500, QCoreApplication::instance(), [readyAt] {
            QCoreApplication::exit(*readyAt >= 0.0 ? 0 : 1);
        });
    };

    QObject::connect(client, &RendezvousClient::connected, client,
                     [client, offerer, ice, &key, stationId, floor, relayUrl, relay] {
        IceConfiguration::resolveHostFamilies(
            IceConfiguration::hostNames(client->stunUrls()), client,
            [client, offerer, ice, &key, stationId, floor, relayUrl, relay](const HostFamilies& families) {
                *ice = IceConfiguration::throughRendezvous(
                    client->stunUrls(), true, IceConfiguration::localAddressFamilies(), families);
                QObject::connect(offerer, &IMediaTransport::localDescription, client,
                                 [client, &key, stationId, floor, relayUrl, offerer, relay](
                                     const QString& sdp, const QString&) {
                    if (floor == QLatin1String("e") && *relay == nullptr) {
                        *relay = new WsRelayPrototype(relayUrl, iceUfrag(sdp),
                                                      WsRelayPrototype::Role::Device, offerer);
                    }
                    client->introduce(stationId, key.publicKeySpki(),
                                      [&key](const QByteArray& m) { return key.sign(m); }, sdp);
                });
                IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, kDeviceSsrc};
                options.ice = *ice;
                options.txChannel = true;
                offerer->start(options);
            });
    });
    QObject::connect(client, &RendezvousClient::answerReceived, offerer,
                     [offerer, ice, floor, shim, relay](const QString& sdp, bool offered,
                                                       const RendezvousWire::Turn& turn) {
        offerer->acceptDescription(sdp, QStringLiteral("answer"));
        if (floor == QLatin1String("e")) {
            const quint16 port = *relay != nullptr ? (*relay)->start() : 0;
            offerer->gatherCandidates({});
            if (port != 0) {
                offerer->acceptCandidate(
                    QStringLiteral("candidate:1 1 UDP 2130706431 127.0.0.1 %1 typ host").arg(port),
                    QString());
            }
            return;
        }
        if (floor == QLatin1String("c")) {
            const quint16 port = shim->start();
            QList<IceRelayServer> relays;
            if (offered && port != 0) {
                relays.append({QStringLiteral("127.0.0.1"), port, turn.username, turn.password});
            }
            offerer->gatherCandidates(relays);
            return;
        }
        const std::optional<RendezvousWire::Turn> relayTurn =
            offered ? std::optional<RendezvousWire::Turn>(turn) : std::nullopt;
        IceConfiguration::resolveHostFamilies(
            relayTurn ? IceConfiguration::hostNames(relayTurn->urls) : QStringList(), offerer,
            [offerer, ice, relayTurn](const HostFamilies& families) {
                ice->addHostFamilies(families);
                ice->setRelay(relayTurn, 1);
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
                     [finish](const QString& reason) { finish(reason); });
    QObject::connect(offerer, &IMediaTransport::connectionFailed, offerer, [=](const QString&) {
        *failedAt = epochMs() - startEpoch;
        if (*readyAt < 0.0) {
            finish(QStringLiteral("The connection could not be made."));
        }
    });
    QObject::connect(audioTimer, &QTimer::timeout, offerer, [offerer, done] {
        const quint32 sequence = done->audioSent + 1;
        if (offerer->sendRtp(rtpPacket(static_cast<quint16>(sequence), sequence * 960U, kDeviceSsrc))) {
            done->audioSent = sequence;
        }
    });
    QObject::connect(displayTimer, &QTimer::timeout, offerer, [offerer, done, displayFrameBytes] {
        QByteArray frame(displayFrameBytes, '\0');
        const quint32 number = done->displaySent + 1;
        qToBigEndian<quint32>(number, reinterpret_cast<uchar*>(frame.data()));
        const QByteArray marker(kPlaintextMarker);
        frame.replace(4, marker.size(), marker);
        for (qsizetype index = 4 + marker.size(); index < frame.size(); ++index) {
            frame[index] = static_cast<char>(index * 7 + static_cast<int>(number));
        }
        const IMediaTransport::DisplaySendResult sent = offerer->submitDisplay(frame);
        if (sent == IMediaTransport::DisplaySendResult::Sent
            || sent == IMediaTransport::DisplaySendResult::Queued) {
            done->displaySent = number;
        }
    });
    QObject::connect(keyedTimer, &QTimer::timeout, offerer, [offerer, done] {
        QByteArray event(kKeyedEventBytes, 'K');
        qToBigEndian<quint32>(done->keyedSent + 1, reinterpret_cast<uchar*>(event.data()) + 1);
        if (offerer->sendTx(event)) {
            ++done->keyedSent;
        }
    });
    QObject::connect(offerer, &IMediaTransport::ready, offerer, [=] {
        *readyAt = epochMs() - startEpoch;
        audioTimer->start();
        displayTimer->start();
        keyedTimer->start();
        QTimer::singleShot(durationMs, offerer, [finish] { finish(QString()); });
        if (resetAtMs > 0) {
            QTimer::singleShot(resetAtMs, offerer, [=] {
                done->resetEpochMs = static_cast<qint64>(epochMs());
                if (shim != nullptr) {
                    shim->resetConnection();
                }
                if (*relay != nullptr) {
                    (*relay)->resetConnection();
                }
            });
        }
    });
    QTimer::singleShot(connectTimeoutMs, offerer, [finish, readyAt] {
        if (*readyAt < 0.0) {
            finish(QStringLiteral("No connection in time."));
        }
    });
    client->connectToService();
    return QCoreApplication::exec();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
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
        std::fputs((StationIdentity::toBase64Url(key.publicKeySpki()) + QStringLiteral("\n")).toLatin1().constData(),
                   stdout);
        return 0;
    }
    if (mode == QLatin1String("core")) {
        return runCore(args);
    }
    if (mode == QLatin1String("device")) {
        return runDevice(args);
    }
    std::fputs("usage: nereus_floor_probe key|core|device ...\n", stderr);
    return 2;
}
