// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_data_channel_transport.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 28 (R-IOS-16): the control session over a data
// channel (DataChannelTransport), on this computer:
//
//   - the chunking and heartbeat bytes (ControlFraming) and the framing
//     conformance fixtures (tests/data/link/v1/framing/);
//   - a message both ways, a 300 KiB settings snapshot in chunks, a message
//     over each end's cap ending the connection, frames a conforming sender
//     never makes;
//   - a whole session between a StationServer and a StationClient signed in
//     by device key over the channel, the Core presenting its own
//     certificate in DTLS;
//   - a Core whose DTLS certificate is not the one its identity binds,
//     refused by the desktop before it sends anything;
//   - the heartbeat declaring the link dead after two missed pongs, at the
//     Core and at the desktop.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: Start the short heartbeat test deadline after sign-in.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: each end keeps the far end's description before its ICE
//               agent takes it (R-R3-49). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: each end sets the DTLS MTU before it takes incoming
//               records (R-R3-49). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: real transmit-watch peer, candidate and frame tests;
//               AI-assisted via OpenAI Codex for J.J. Boyd (KG4VCF).
//   2026-10-01: Control logging lane: a delivered message's wait since
//               its receipt is readable while it is delivered, and only
//               then; the link's diagnostics name the data channel's
//               buffer. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-28: watch peers through two actual RelayLegs and a local WSS
//               relay player; AI-assisted via OpenAI Codex for J.J. Boyd.
// =================================================================

#include <QtTest>

#include <QDir>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QPointer>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QHostAddress>
#include <QMutex>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include "core/AppSettings.h"
#include "core/security/CertificateStore.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/DataChannelLibraryLogTestHook.h"
#include "core/session/RelayLeg.h"
#include "core/session/RendezvousWire.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"

#include "LinkFixtures.h"
#include "fakes/DataChannelPair.h"
#include "fakes/DataChannelStartupEvidence.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LinkFixtures;
using NereusSDR::Test::DataChannelPairEvent;
using NereusSDR::Test::startDataChannelPair;
using NereusSDR::Test::waitFor;

namespace {

// Keep only fixed protocol-stage words from libdatachannel's verbose log.
// Its other lines can contain raw SDP, candidates, addresses or fingerprints.
using NereusSDR::Test::safeRtcStage;

constexpr quint64 kStationCap = StationServer::kMaxIncomingMessageBytes;
constexpr quint64 kClientCap = StationClient::kMaxIncomingMessageBytes;

QByteArray patterned(qsizetype size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    for (qsizetype index = 0; index < size; ++index) {
        bytes[index] = static_cast<char>('a' + (index * 7) % 26);
    }
    return bytes;
}

// A Core with its StationServer and its own certificate.
struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;

    Core()
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        settings->setValue(QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"));
        model = std::make_unique<RadioModel>();
        const QString security = NereusSDR::Test::seedCoreIdentity(securityDir.path());
        server = std::make_unique<StationServer>(model.get(), *settings, security);
        server->setHeartbeatIntervalMs(0);
    }

    ~Core() { server.reset(); }

    bool pairComputer(const ClientDeviceIdentity& key)
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = QStringLiteral("Shack MacBook");
        device.kind = QStringLiteral("computer");
        return server->deviceStore()->add(device);
    }
};

// Two transports on this computer, open.
struct OpenPair {
    std::unique_ptr<DataChannelTransport> offerer = std::make_unique<DataChannelTransport>();
    std::unique_ptr<DataChannelTransport> answerer = std::make_unique<DataChannelTransport>();

    bool open(const QString& certificate = QString(), const QString& key = QString())
    {
        if (!startDataChannelPair(offerer.get(), answerer.get(), kClientCap, kStationCap,
                                  certificate, key)) {
            return false;
        }
        return waitFor([this] { return offerer->isOpen() && answerer->isOpen(); }, 15000);
    }
};

class WatchSource final : public IceConfiguration::CandidateSource {
public:
    void start(std::function<void(const QString&)> add) override { m_add = std::move(add); }
    void stop() override { m_add = {}; }
    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override
    {
        if (!m_add) { return std::nullopt; }
        NetworkPathSnapshot path;
        path.kind = NetworkPathSnapshot::Kind::Relayed;
        path.carrier = NetworkPathSnapshot::Carrier::WebRelay;
        path.endpoints = NetworkPathSnapshot::Endpoints::Socket;
        path.remoteAddress = QStringLiteral("198.51.100.7");
        path.remotePort = 443;
        return path;
    }
    bool ready() const { return static_cast<bool>(m_add); }
    std::function<void(const QString&)> callback() const { return m_add; }
    void inject(const QString& candidate) { if (m_add) { m_add(candidate); } }

private:
    std::function<void(const QString&)> m_add;
};

IceConfiguration watchIce(const std::shared_ptr<WatchSource>& source)
{
    IceConfiguration ice = IceConfiguration::throughRendezvous({}, true, {}, {});
    ice.setRelay(std::nullopt, 1);
    ice.setCandidateSourceFactory(
        [source](int lane, const QString& id, bool routed)
            -> std::shared_ptr<IceConfiguration::CandidateSource> {
            return lane == IceConfiguration::kControlLane && id.isEmpty() && !routed
                ? source : nullptr;
        }, true);
    return ice;
}

QString loopbackFor(const QStringList& gathered)
{
    for (const QString& candidate : gathered) {
        const QStringList fields = candidate.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() >= 8 && fields.at(4).contains(QLatin1Char('.'))
            && fields.at(7) == QLatin1String("host")) {
            bool ok = false;
            const int port = fields.at(5).toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                return RelayLeg::candidateLine(IceConfiguration::kControlLane,
                                               static_cast<quint16>(port));
            }
        }
    }
    return {};
}

struct WatchPair {
    std::unique_ptr<DataChannelTransport> offerer = std::make_unique<DataChannelTransport>();
    std::unique_ptr<DataChannelTransport> answerer = std::make_unique<DataChannelTransport>();
    std::shared_ptr<WatchSource> offerSource = std::make_shared<WatchSource>();
    std::shared_ptr<WatchSource> answerSource = std::make_shared<WatchSource>();
    bool descriptionsAccepted = true;

    bool open(const QString& certificate, const QString& key)
    {
        QObject::connect(offerer.get(), &DataChannelTransport::localDescription,
                         answerer.get(), [this](const QString& sdp, const QString& type) {
            descriptionsAccepted &= answerer->acceptDescription(sdp, type);
        });
        QObject::connect(answerer.get(), &DataChannelTransport::localDescription,
                         offerer.get(), [this](const QString& sdp, const QString& type) {
            descriptionsAccepted &= offerer->acceptDescription(sdp, type);
        });
        DataChannelTransport::Options answer;
        answer.role = DataChannelTransport::Role::Answerer;
        answer.purpose = DataChannelTransport::Purpose::TxWatch;
        answer.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        answer.ice = watchIce(answerSource);
        answer.certificatePemPath = certificate;
        answer.privateKeyPemPath = key;
        DataChannelTransport::Options offer = answer;
        offer.role = DataChannelTransport::Role::Offerer;
        offer.ice = watchIce(offerSource);
        offer.certificatePemPath.clear();
        offer.privateKeyPemPath.clear();
        if (!answerer->start(answer) || !offerer->start(offer)) {
            return false;
        }
        if (!waitFor([this] {
                return descriptionsAccepted && offerSource->ready() && answerSource->ready()
                    && !loopbackFor(offerer->localCandidatesForTest()).isEmpty()
                    && !loopbackFor(answerer->localCandidatesForTest()).isEmpty();
            }, 10000)) {
            return false;
        }
        offerSource->inject(loopbackFor(answerer->localCandidatesForTest()));
        answerSource->inject(loopbackFor(offerer->localCandidatesForTest()));
        return waitFor([this] { return offerer->isOpen() && answerer->isOpen(); }, 15000);
    }
};

struct LoopbackTlsIdentity {
    QSslCertificate certificate;
    QSslKey key;
};

LoopbackTlsIdentity makeLoopbackTlsIdentity()
{
    using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using Cert = std::unique_ptr<X509, decltype(&X509_free)>;
    using Bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
    Key key(EVP_RSA_gen(2048), &EVP_PKEY_free);
    Cert cert(X509_new(), &X509_free);
    if (!key || !cert || X509_set_version(cert.get(), 2) != 1
        || ASN1_INTEGER_set_int64(X509_get_serialNumber(cert.get()), 1) != 1
        || !X509_gmtime_adj(X509_getm_notBefore(cert.get()), -60)
        || !X509_gmtime_adj(X509_getm_notAfter(cert.get()), 3600)
        || X509_set_pubkey(cert.get(), key.get()) != 1) {
        return {QSslCertificate(), QSslKey()};
    }
    X509_NAME* name = X509_get_subject_name(cert.get());
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>("127.0.0.1"),
                                   -1, -1, 0) != 1
        || X509_set_issuer_name(cert.get(), name) != 1) {
        return {QSslCertificate(), QSslKey()};
    }
    X509V3_CTX context;
    X509V3_set_ctx(&context, cert.get(), cert.get(), nullptr, nullptr, 0);
    for (const auto& [nid, value] : {
             std::pair<int, const char*>{NID_basic_constraints, "critical,CA:TRUE"},
             {NID_key_usage, "critical,digitalSignature,keyEncipherment,keyCertSign"},
             {NID_ext_key_usage, "serverAuth"},
             {NID_subject_alt_name, "IP:127.0.0.1"}}) {
        X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value);
        const bool added = extension && X509_add_ext(cert.get(), extension, -1) == 1;
        X509_EXTENSION_free(extension);
        if (!added) {
            return {QSslCertificate(), QSslKey()};
        }
    }
    if (X509_sign(cert.get(), key.get(), EVP_sha256()) == 0) {
        return {QSslCertificate(), QSslKey()};
    }
    Bio certBio(BIO_new(BIO_s_mem()), &BIO_free);
    Bio keyBio(BIO_new(BIO_s_mem()), &BIO_free);
    if (!certBio || !keyBio || PEM_write_bio_X509(certBio.get(), cert.get()) != 1
        || PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0,
                                    nullptr, nullptr) != 1) {
        return {QSslCertificate(), QSslKey()};
    }
    const auto contents = [](BIO* bio) {
        char* data = nullptr;
        const long size = BIO_get_mem_data(bio, &data);
        return QByteArray(data, static_cast<qsizetype>(size));
    };
    const QByteArray certPem = contents(certBio.get());
    return {QSslCertificate(certPem, QSsl::Pem),
            QSslKey(contents(keyBio.get()), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey)};
}

// A bounded local WSS player for the relay protocol. JOIN tokens select
// primary/watch and Core/client; watch tag-3 datagrams alone are forwarded.
// Admission here is a fixture, not the Python production relay's verifier.
class LocalWatchRelay {
public:
    LocalWatchRelay()
        : m_savedTls(QSslConfiguration::defaultConfiguration())
        , m_identity(makeLoopbackTlsIdentity())
        , m_server(QStringLiteral("watch-relay"), QWebSocketServer::SecureMode)
    {
        if (m_identity.certificate.isNull() || m_identity.key.isNull()) {
            return;
        }
        QSslConfiguration trusted = m_savedTls;
        trusted.addCaCertificate(m_identity.certificate);
        QSslConfiguration::setDefaultConfiguration(trusted);
        QSslConfiguration serverTls = trusted;
        serverTls.setLocalCertificate(m_identity.certificate);
        serverTls.setPrivateKey(m_identity.key);
        m_server.setSslConfiguration(serverTls);
        if (!m_server.listen(QHostAddress::LocalHost, 0)) {
            return;
        }
        QObject::connect(&m_server, &QWebSocketServer::newConnection, &m_server, [this] {
            while (QWebSocket* socket = m_server.nextPendingConnection()) {
                socket->setMaxAllowedIncomingMessageSize(RelayLeg::kMaxDatagramBytes + 65);
                QObject::connect(socket, &QWebSocket::binaryMessageReceived, &m_server,
                                 [this, socket](const QByteArray& frame) {
                    onFrame(socket, frame);
                });
            }
        });
    }

    ~LocalWatchRelay() { QSslConfiguration::setDefaultConfiguration(m_savedTls); }
    bool listening() const { return m_server.isListening(); }
    QUrl url() const
    {
        return QUrl(QStringLiteral("wss://127.0.0.1:%1/v1/relay").arg(m_server.serverPort()));
    }
    QSslCertificate certificate() const { return m_identity.certificate; }
    bool hasAllLegs() const { return m_joined.size() == 4; }
    bool fourSockets() const
    {
        QSet<QWebSocket*> sockets;
        for (const auto& socket : m_joined) {
            if (!socket) {
                return false;
            }
            sockets.insert(socket.data());
        }
        return sockets.size() == 4;
    }
    const QList<QByteArray>& forwardedWatchFrames() const { return m_forwardedWatch; }
    const QList<QByteArray>& primaryFrames() const { return m_primaryFrames; }
    int rejected() const { return m_rejected; }

private:
    void onFrame(QWebSocket* socket, const QByteArray& frame)
    {
        if (frame.isEmpty()) {
            ++m_rejected;
            socket->close();
            return;
        }
        const quint8 tag = static_cast<quint8>(frame.at(0));
        if (tag == RelayLeg::kTagJoin) {
            const QString token = QString::fromLatin1(frame.mid(1));
            if (!QStringList{QStringLiteral("core-primary"), QStringLiteral("client-primary"),
                             QStringLiteral("core-watch"), QStringLiteral("client-watch")}
                     .contains(token)
                || m_joined.contains(token)) {
                ++m_rejected;
                socket->close();
                return;
            }
            m_joined.insert(token, socket);
            m_role.insert(socket, token);
            const bool watch = token.endsWith(QLatin1String("watch"));
            const QString other = watch
                ? (token == QLatin1String("core-watch")
                       ? QStringLiteral("client-watch") : QStringLiteral("core-watch"))
                : (token == QLatin1String("core-primary")
                       ? QStringLiteral("client-primary") : QStringLiteral("core-primary"));
            if (m_joined.contains(other)) {
                const QByteArray ready("\x81\x01\x01", 3);
                socket->sendBinaryMessage(ready);
                m_joined.value(other)->sendBinaryMessage(ready);
            }
            return;
        }
        const QString token = m_role.value(socket);
        if (token.isEmpty()) {
            ++m_rejected;
            socket->close();
            return;
        }
        if (token.endsWith(QLatin1String("primary"))) {
            m_primaryFrames.append(frame);
            return;
        }
        if (tag != RelayLeg::kTagWatch || frame.size() < 2
            || frame.size() > RelayLeg::kMaxDatagramBytes + 1) {
            ++m_rejected;
            socket->close();
            return;
        }
        const QString other = token == QLatin1String("core-watch")
            ? QStringLiteral("client-watch") : QStringLiteral("core-watch");
        if (QWebSocket* peer = m_joined.value(other).data()) {
            m_forwardedWatch.append(frame);
            peer->sendBinaryMessage(frame);
        }
    }

    QSslConfiguration m_savedTls;
    LoopbackTlsIdentity m_identity;
    QWebSocketServer m_server;
    QHash<QString, QPointer<QWebSocket>> m_joined;
    QHash<QWebSocket*, QString> m_role;
    QList<QByteArray> m_forwardedWatch;
    QList<QByteArray> m_primaryFrames;
    int m_rejected = 0;
};

} // namespace

class TstDataChannelTransport : public QObject {
    Q_OBJECT

private slots:
    void selectedCandidateSnapshotKeepsEndpointMeaning()
    {
        MediaIcePath direct;
        direct.localType = QStringLiteral("host");
        direct.remoteType = QStringLiteral("host");
        direct.localAddress = QStringLiteral("::1");
        direct.localPort = 41000;
        direct.remoteAddress = QStringLiteral("::1");
        direct.remotePort = 42000;
        const auto directRoute = direct.networkPathSnapshot();
        QVERIFY(directRoute);
        QCOMPARE(directRoute->kind, NetworkPathSnapshot::Kind::Direct);
        QCOMPARE(directRoute->carrier, NetworkPathSnapshot::Carrier::Ice);
        QCOMPARE(directRoute->endpoints, NetworkPathSnapshot::Endpoints::IceCandidates);
        QCOMPARE(directRoute->localAddress, QStringLiteral("::1"));
        QCOMPARE(directRoute->localPort, quint16(41000));
        QCOMPARE(directRoute->remotePort, quint16(42000));
        MediaIcePath namedPeer = direct;
        namedPeer.remoteAddress = QStringLiteral("peer.local");
        const auto numericOnly = namedPeer.networkPathSnapshot();
        QVERIFY(numericOnly);
        QVERIFY(numericOnly->remoteAddress.isEmpty());
        QCOMPARE(numericOnly->remotePort, quint16(42000));

        MediaIcePath turn = direct;
        turn.localAddress = QStringLiteral("198.51.100.4");
        turn.remoteAddress = QStringLiteral("203.0.113.8");
        turn.localType = QStringLiteral("relay");
        const auto relayed = turn.networkPathSnapshot();
        QVERIFY(relayed);
        QCOMPARE(relayed->kind, NetworkPathSnapshot::Kind::Relayed);
        QCOMPARE(relayed->endpoints, NetworkPathSnapshot::Endpoints::IceCandidates);
        QCOMPARE(relayed->localAddress, QStringLiteral("198.51.100.4"));
        QCOMPARE(relayed->localCandidateType, QStringLiteral("relay"));

        direct.ownedLoopbackShim = true;
        QVERIFY(!direct.networkPathSnapshot()); // Hide internal loopback without current source facts.
        NetworkPathSnapshot source;
        source.kind = NetworkPathSnapshot::Kind::Relayed;
        source.carrier = NetworkPathSnapshot::Carrier::WebRelay;
        source.endpoints = NetworkPathSnapshot::Endpoints::Socket;
        source.remoteAddress = QStringLiteral("198.51.100.3");
        source.remotePort = 443;
        direct.ownedSourcePath = source;
        const auto routed = direct.networkPathSnapshot();
        QVERIFY(routed);
        QCOMPARE(routed->remoteAddress, QStringLiteral("198.51.100.3"));
        QCOMPARE(routed->remotePort, quint16(443));
        QVERIFY(routed->localAddress.isEmpty()); // Never leak the selected internal loopback.
    }
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void ordinaryControlBacklogIsObservable()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QCOMPARE(pair.answerer->backlogBytes(), 0);
        // Stay within the desktop's existing 8 MiB message cap while
        // queuing enough chunks to expose libdatachannel's send buffer.
        pair.answerer->sendText(patterned(6 * 1024 * 1024));
        QVERIFY2(pair.answerer->backlogBytes() > 0,
                 "ordinary control must report the data channel's queued bytes");
    }

    void watchRequiresDedicatedIceAndRefusesOrdinaryCandidates()
    {
        DataChannelTransport::Options options;
        options.purpose = DataChannelTransport::Purpose::TxWatch;
        options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        DataChannelTransport transport;
        QVERIFY(!transport.start(options));
        auto source = std::make_shared<WatchSource>();
        IceConfiguration ice = watchIce(source);
        options.ice = ice;
        options.maxIncomingBytes = 34;
        QVERIFY(!transport.start(options));
        options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        options.ice = IceConfiguration::throughRendezvous(
            {QStringLiteral("stun:127.0.0.1:3478")}, true, {}, {});
        QVERIFY(!transport.start(options));
        options.ice = ice;
        QVERIFY(transport.start(options));
        QVERIFY(!transport.acceptCandidate(RelayLeg::candidateLine(1, 12345)));
        QVERIFY(!transport.acceptCandidate(
            QStringLiteral("candidate:host 1 UDP 1 127.0.0.1 12345 typ host")));
        QVERIFY(!transport.gatherCandidates(ice));
    }

    void watchRejectsCandidatesEmbeddedInSdp()
    {
        auto offerSource = std::make_shared<WatchSource>();
        auto answerSource = std::make_shared<WatchSource>();
        DataChannelTransport offerer;
        DataChannelTransport answerer;
        QSignalSpy descriptions(&offerer, &DataChannelTransport::localDescription);
        DataChannelTransport::Options options;
        options.purpose = DataChannelTransport::Purpose::TxWatch;
        options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        options.ice = watchIce(offerSource);
        QVERIFY(offerer.start(options));
        options.role = DataChannelTransport::Role::Answerer;
        options.ice = watchIce(answerSource);
        QVERIFY(answerer.start(options));
        QTRY_COMPARE(descriptions.count(), 1);
        QString sdp = descriptions.first().first().toString();
        QVERIFY(!sdp.contains(QLatin1String("a=candidate:")));
        sdp.append(QStringLiteral(
            "a=candidate:ordinary 1 UDP 1 127.0.0.1 12345 typ host\r\n"));
        QVERIFY(!answerer.acceptDescription(sdp, QStringLiteral("offer")));
    }

    void watchUsesRealDtlsAndRawBoundedBinary()
    {
        Core core;
        WatchPair pair;
        QSignalSpy localOfferCandidates(pair.offerer.get(), &DataChannelTransport::localCandidate);
        QSignalSpy localAnswerCandidates(pair.answerer.get(), &DataChannelTransport::localCandidate);
        QSignalSpy binaryAtCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        QSignalSpy binaryAtDevice(pair.offerer.get(), &SessionTransport::binaryReceived);
        QSignalSpy textAtCore(pair.answerer.get(), &SessionTransport::textReceived);
        QVERIFY(pair.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        QVERIFY(pair.descriptionsAccepted);
        QCOMPARE(localOfferCandidates.count(), 0);
        QCOMPARE(localAnswerCandidates.count(), 0);
        QVERIFY(pair.offerer->selectedPath().has_value());
        QVERIFY(pair.answerer->selectedPath().has_value());
        QCOMPARE(pair.offerer->selectedPath()->remoteAddress, QStringLiteral("127.0.0.1"));
        QCOMPARE(pair.answerer->selectedPath()->remoteAddress, QStringLiteral("127.0.0.1"));
        QVERIFY(pair.offerer->selectedPath()->viaLoopbackShim());
        QVERIFY(pair.answerer->selectedPath()->viaLoopbackShim());
        QVERIFY(pair.offerer->carriesBinary());
        QVERIFY(pair.answerer->carriesBinary());
        QVERIFY(!pair.offerer->mediaIceConfiguration().has_value());
        QCOMPARE(pair.offerer->peerCertificateSha256(),
                 QByteArray::fromHex(core.server->certificateFingerprint()
                                         .remove(QLatin1Char(':')).toLatin1()));
        QCOMPARE(pair.answerer->peerCertificateSha256(), QByteArray());

        const QByteArray attach = QByteArray(1, char(1)) + patterned(32);
        const QByteArray ack = QByteArray::fromHex("0100");
        const QByteArray heartbeat = QByteArray::fromHex("01000000010000000100000001");
        QVERIFY(pair.offerer->sendBinary(attach));
        QVERIFY(pair.answerer->sendBinary(ack));
        QVERIFY(pair.offerer->sendBinary(heartbeat));
        QTRY_COMPARE(binaryAtCore.count(), 2);
        QTRY_COMPARE(binaryAtDevice.count(), 1);
        QCOMPARE(binaryAtCore.at(0).at(0).toByteArray(), attach);
        QCOMPARE(binaryAtCore.at(1).at(0).toByteArray(), heartbeat);
        QCOMPARE(binaryAtDevice.at(0).at(0).toByteArray(), ack);
        QCOMPARE(textAtCore.count(), 0);
        pair.offerer->sendText(QByteArrayLiteral("{}"));
        pair.offerer->ping();
        QCOMPARE(pair.offerer->countsForTest().pingsSent, quint64(0));
        QVERIFY(!pair.offerer->acceptCandidate(RelayLeg::candidateLine(1, 12345)));
        QVERIFY(!pair.offerer->sendBinary(QByteArray(34, 'x')));
        QTRY_VERIFY(!pair.offerer->isOpen());
    }

    void watchRejectsTextAndEmptyOnRealChannel()
    {
        Core core;
        WatchPair pair;
        QVERIFY(pair.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        QVERIFY(pair.offerer->sendRawTextForTest(QByteArrayLiteral("not binary")));
        QTRY_VERIFY(!pair.answerer->isOpen());

        WatchPair emptyPair;
        QVERIFY(emptyPair.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        QVERIFY(emptyPair.offerer->sendRawFrameForTest(QByteArray()));
        QTRY_VERIFY(!emptyPair.answerer->isOpen());

        WatchPair oversizedPair;
        QVERIFY(oversizedPair.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        QVERIFY(oversizedPair.offerer->sendRawFrameForTest(QByteArray(34, 'x')));
        QTRY_VERIFY(!oversizedPair.answerer->isOpen());
    }

    void watchRejectsExtraChannelsAndBoundedHeldFrames()
    {
        Core core;
        WatchPair wrongLabel;
        QVERIFY(wrongLabel.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        QVERIFY(wrongLabel.offerer->openUnexpectedChannelForTest(QStringLiteral("control"), false));
        QTRY_VERIFY(!wrongLabel.answerer->isOpen());

        WatchPair wrongReliability;
        QVERIFY(wrongReliability.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        QVERIFY(wrongReliability.offerer->openUnexpectedChannelForTest(
            QString::fromLatin1(DataChannelTransport::kTxWatchLabel), true));
        QTRY_VERIFY(!wrongReliability.answerer->isOpen());

        WatchPair held;
        QVERIFY(held.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        // No binary listener: the adapter may hold sixteen frames, then
        // closes its own peer rather than retaining an unbounded backlog.
        for (int i = 0; i < 17; ++i) {
            if (!held.offerer->isOpen()) {
                break;
            }
            held.offerer->sendBinary(QByteArray(1, char(i)));
        }
        QTRY_VERIFY(!held.answerer->isOpen());

        WatchPair invalidSource;
        QVERIFY(invalidSource.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        invalidSource.offerSource->inject(
            QStringLiteral("candidate:ordinary 1 UDP 1 127.0.0.1 12345 typ host"));
        QTRY_VERIFY(!invalidSource.offerer->isOpen());
    }

    void watchOwnerDeletionCancelsCandidateSource()
    {
        Core core;
        WatchPair pair;
        QVERIFY(pair.open(core.server->certificatePemPath(), core.server->privateKeyPemPath()));
        QPointer<DataChannelTransport> gone = pair.answerer.get();
        QVERIFY(pair.offerer->sendBinary(QByteArray::fromHex("0100")));
        pair.answerer.reset();
        QVERIFY(gone.isNull());
        QTRY_VERIFY_WITH_TIMEOUT(!pair.answerSource->ready(), 5000);
    }

    void watchPeersUseSeparateRealRelayLegsOverLocalWss()
    {
        Core core;
        LocalWatchRelay relay;
        QVERIFY(relay.listening());
        auto corePrimary = RelayLeg::create();
        auto clientPrimary = RelayLeg::create();
        auto coreWatch = RelayLeg::createWatch();
        auto clientWatch = RelayLeg::createWatch();
        QVERIFY(corePrimary && clientPrimary && coreWatch && clientWatch);
        QVERIFY(corePrimary->lanePort(1) != coreWatch->lanePort(1));
        QVERIFY(clientPrimary->lanePort(1) != clientWatch->lanePort(1));
        QCOMPARE(coreWatch->lanePort(2), quint16(0));
        QCOMPARE(clientWatch->lanePort(2), quint16(0));
        corePrimary->open(relay.url(), QStringLiteral("core-primary"));
        clientPrimary->open(relay.url(), QStringLiteral("client-primary"));
        coreWatch->open(relay.url(), QStringLiteral("core-watch"));
        clientWatch->open(relay.url(), QStringLiteral("client-watch"));
        QVERIFY(waitFor([&] {
            return relay.hasAllLegs() && relay.fourSockets()
                && corePrimary->state() == RelayLeg::State::Joined
                && clientPrimary->state() == RelayLeg::State::Joined
                && coreWatch->state() == RelayLeg::State::Joined
                && clientWatch->state() == RelayLeg::State::Joined;
        }, 10000));

        const auto iceForLeg = [](const std::shared_ptr<RelayLeg>& leg) {
            IceConfiguration ice = IceConfiguration::throughRendezvous(
                {}, true, IceConfiguration::localAddressFamilies(), {});
            ice.setRelay(std::nullopt, 1);
            ice.setCandidateSourceFactory(RelayLeg::factoryFor(leg), true);
            return ice;
        };
        auto offerer = std::make_unique<DataChannelTransport>();
        auto answerer = std::make_unique<DataChannelTransport>();
        bool descriptionsAccepted = true;
        QObject::connect(offerer.get(), &DataChannelTransport::localDescription,
                         answerer.get(), [&](const QString& sdp, const QString& type) {
            descriptionsAccepted &= answerer->acceptDescription(sdp, type);
        });
        QObject::connect(answerer.get(), &DataChannelTransport::localDescription,
                         offerer.get(), [&](const QString& sdp, const QString& type) {
            descriptionsAccepted &= offerer->acceptDescription(sdp, type);
        });
        QSignalSpy offerCandidates(offerer.get(), &DataChannelTransport::localCandidate);
        QSignalSpy answerCandidates(answerer.get(), &DataChannelTransport::localCandidate);
        QSignalSpy atCore(answerer.get(), &SessionTransport::binaryReceived);
        QSignalSpy atClient(offerer.get(), &SessionTransport::binaryReceived);
        DataChannelTransport::Options answer;
        answer.purpose = DataChannelTransport::Purpose::TxWatch;
        answer.role = DataChannelTransport::Role::Answerer;
        answer.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        answer.certificatePemPath = core.server->certificatePemPath();
        answer.privateKeyPemPath = core.server->privateKeyPemPath();
        answer.ice = iceForLeg(coreWatch);
        DataChannelTransport::Options offer = answer;
        offer.role = DataChannelTransport::Role::Offerer;
        offer.certificatePemPath.clear();
        offer.privateKeyPemPath.clear();
        offer.ice = iceForLeg(clientWatch);
        QVERIFY(answerer->start(answer));
        QVERIFY(offerer->start(offer));
        QVERIFY(waitFor([&] {
            return descriptionsAccepted && answerer->isOpen() && offerer->isOpen();
        }, 20000));
        QCOMPARE(offerCandidates.size(), 0);
        QCOMPARE(answerCandidates.size(), 0);
        QVERIFY(!offerer->acceptCandidate(RelayLeg::candidateLine(1, coreWatch->lanePort(1))));
        QVERIFY(!answerer->acceptCandidate(RelayLeg::candidateLine(1, clientWatch->lanePort(1))));
        QVERIFY(offerer->selectedPath().has_value());
        QVERIFY(answerer->selectedPath().has_value());
        QCOMPARE(offerer->selectedPath()->remoteAddress, QStringLiteral("127.0.0.1"));
        QCOMPARE(answerer->selectedPath()->remoteAddress, QStringLiteral("127.0.0.1"));
        const auto clientRoute = offerer->networkPathSnapshot();
        const auto coreRoute = answerer->networkPathSnapshot();
        QVERIFY(clientRoute && coreRoute);
        for (const auto& route : {clientRoute, coreRoute}) {
            QCOMPARE(route->kind, NetworkPathSnapshot::Kind::Relayed);
            QCOMPARE(route->carrier, NetworkPathSnapshot::Carrier::WebRelay);
            QCOMPARE(route->endpoints, NetworkPathSnapshot::Endpoints::Socket);
            QCOMPARE(route->remoteAddress, QStringLiteral("127.0.0.1"));
            QCOMPARE(route->remotePort, static_cast<quint16>(relay.url().port()));
            QVERIFY(route->localPort != 0);
        }
        QCOMPARE(offerer->peerCertificateSha256(),
                 QByteArray::fromHex(core.server->certificateFingerprint()
                                         .remove(QLatin1Char(':')).toLatin1()));
        QVERIFY(offerer->peerCertificateSha256()
                != relay.certificate().digest(QCryptographicHash::Sha256));

        const QByteArray attach = QByteArray(1, char(1)) + patterned(32);
        const QByteArray ack = QByteArray::fromHex("0100");
        const QByteArray heartbeat = QByteArray::fromHex("01000000010000000100000001");
        QVERIFY(offerer->sendBinary(attach));
        QTRY_COMPARE(atCore.size(), 1);
        QCOMPARE(atCore.first().first().toByteArray(), attach);
        QVERIFY(answerer->sendBinary(ack));
        QTRY_COMPARE(atClient.size(), 1);
        QCOMPARE(atClient.first().first().toByteArray(), ack);
        QVERIFY(offerer->sendBinary(heartbeat));
        QTRY_COMPARE(atCore.size(), 2);
        QCOMPARE(atCore.last().first().toByteArray(), heartbeat);
        QVERIFY(!relay.forwardedWatchFrames().isEmpty());
        for (const QByteArray& frame : relay.forwardedWatchFrames()) {
            QCOMPARE(static_cast<quint8>(frame.at(0)), RelayLeg::kTagWatch);
        }
        QCOMPARE(relay.primaryFrames().size(), 0);
        QCOMPARE(relay.rejected(), 0);
        QCOMPARE(corePrimary->state(), RelayLeg::State::Joined);
        QCOMPARE(clientPrimary->state(), RelayLeg::State::Joined);

        offerer->closeLink(QStringLiteral("watch fixture done"));
        answerer->closeLink(QStringLiteral("watch fixture done"));
        offerer.reset();
        answerer.reset();
        const auto sourceAvailable = [](const std::shared_ptr<RelayLeg>& leg) {
            auto probe = leg->sourceFor(IceConfiguration::kControlLane);
            if (!probe) {
                return false;
            }
            QString candidate;
            probe->start([&](const QString& line) { candidate = line; });
            probe->stop();
            return !candidate.isEmpty();
        };
        // CandidateSourceLease releases the RelayLaneSource through
        // deleteLater() after ICE teardown. QTest::qWaitFor advances deferred
        // deletion; the generic waitFor() pump does not.
        QVERIFY2(QTest::qWaitFor([&] { return sourceAvailable(coreWatch); }, 5000),
                 "Core watch source remained claimed after peer teardown");
        QVERIFY2(QTest::qWaitFor([&] { return sourceAvailable(clientWatch); }, 5000),
                 "Client watch source remained claimed after peer teardown");
        QCOMPARE(corePrimary->state(), RelayLeg::State::Joined);
        QCOMPARE(clientPrimary->state(), RelayLeg::State::Joined);
        coreWatch->close();
        clientWatch->close();
        corePrimary->close();
        clientPrimary->close();
    }

    // ── The bytes ─────────────────────────────────────────────────────

    void aMessageIsCutIntoFullChunksThenTheRest()
    {
        const QByteArray message = patterned(ControlFraming::kMaxChunkPayloadBytes * 2 + 5);
        const QList<QByteArray> chunks = ControlFraming::chunk(message);
        QCOMPARE(chunks.size(), 3);
        QCOMPARE(chunks.at(0).size(), ControlFraming::kMaxChunkBytes);
        QCOMPARE(chunks.at(1).size(), ControlFraming::kMaxChunkBytes);
        QCOMPARE(chunks.at(2).size(), qsizetype(6));
        QCOMPARE(quint8(chunks.at(0).at(0)), ControlFraming::kChunkMore);
        QCOMPARE(quint8(chunks.at(1).at(0)), ControlFraming::kChunkMore);
        QCOMPARE(quint8(chunks.at(2).at(0)), ControlFraming::kChunkLast);
        // Exactly a chunk's worth is one chunk, the last.
        const QList<QByteArray> one =
            ControlFraming::chunk(patterned(ControlFraming::kMaxChunkPayloadBytes));
        QCOMPARE(one.size(), 1);
        QCOMPARE(quint8(one.at(0).at(0)), ControlFraming::kChunkLast);
        QCOMPARE(ControlFraming::ping(0x01020304), QByteArray::fromHex("1001020304"));
        QCOMPARE(ControlFraming::pong(0xa0b0c0d0), QByteArray::fromHex("11a0b0c0d0"));

        ControlFraming::Reassembler joiner(kStationCap);
        QCOMPARE(joiner.feed(chunks.at(0)), ControlFraming::Reassembler::Result::Pending);
        // A ping between two chunks leaves the message being joined alone.
        QCOMPARE(joiner.feed(ControlFraming::ping(7)), ControlFraming::Reassembler::Result::Ping);
        QCOMPARE(joiner.id(), quint32(7));
        QCOMPARE(joiner.feed(chunks.at(1)), ControlFraming::Reassembler::Result::Pending);
        QCOMPARE(joiner.feed(ControlFraming::pong(9)), ControlFraming::Reassembler::Result::Pong);
        QCOMPARE(joiner.feed(chunks.at(2)), ControlFraming::Reassembler::Result::Message);
        QCOMPARE(joiner.message(), message);
        QCOMPARE(joiner.pendingBytes(), qsizetype(0));
    }

    void framesAConformingSenderNeverMakesEndTheConnection_data()
    {
        QTest::addColumn<QByteArray>("frame");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("a chunk with nothing in it") << QByteArray::fromHex("02");
        QTest::newRow("an unknown kind") << QByteArray::fromHex("0361");
        QTest::newRow("a short ping") << QByteArray::fromHex("10010203");
        QTest::newRow("a long pong") << QByteArray::fromHex("110102030405");
        QTest::newRow("a chunk past the size")
            << (QByteArray(1, char(ControlFraming::kChunkLast))
                + patterned(ControlFraming::kMaxChunkBytes));
    }

    void framesAConformingSenderNeverMakesEndTheConnection()
    {
        QFETCH(QByteArray, frame);
        ControlFraming::Reassembler joiner(kStationCap);
        QCOMPARE(joiner.feed(frame), ControlFraming::Reassembler::Result::Refused);
        QVERIFY(!joiner.reason().isEmpty());
        // Refused for good.
        QCOMPARE(joiner.feed(ControlFraming::chunk("{}").first()),
                 ControlFraming::Reassembler::Result::Refused);
    }

    void aMessageOverTheCapIsRefusedTheMomentItPassesIt()
    {
        ControlFraming::Reassembler joiner(100);
        QCOMPARE(joiner.feed(QByteArray(1, char(ControlFraming::kChunkMore)) + patterned(60)),
                 ControlFraming::Reassembler::Result::Pending);
        // 60 + 40 is the cap exactly, still more to come: fine.
        QCOMPARE(joiner.feed(QByteArray(1, char(ControlFraming::kChunkMore)) + patterned(40)),
                 ControlFraming::Reassembler::Result::Pending);
        // One more byte passes it, before the last chunk arrives.
        QCOMPARE(joiner.feed(QByteArray(1, char(ControlFraming::kChunkMore)) + patterned(1)),
                 ControlFraming::Reassembler::Result::Refused);
        ControlFraming::Reassembler exact(100);
        QCOMPARE(exact.feed(QByteArray(1, char(ControlFraming::kChunkLast)) + patterned(100)),
                 ControlFraming::Reassembler::Result::Message);
    }

    // The framing fixtures (the link document, section 16.1): each one's
    // chunks, as a sender makes them, and what a receiver with that end's
    // cap does with its frames.
    void framingFixtures_data()
    {
        QTest::addColumn<QString>("file");
        QString error;
        const QJsonObject manifest = LinkFixtures::readObject(
            QDir(LinkFixtures::dataDirectory()).filePath(QStringLiteral("manifest.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QList<LinkFixtures::Entry> entries =
            LinkFixtures::entries(manifest, QStringLiteral("framing"));
        QVERIFY(entries.size() >= 8);
        for (const LinkFixtures::Entry& entry : entries) {
            QTest::newRow(qPrintable(entry.id)) << entry.file;
        }
    }

    void framingFixtures()
    {
        QFETCH(QString, file);
        QString error;
        const QJsonObject fixture = LinkFixtures::readObject(
            QDir(LinkFixtures::dataDirectory()).filePath(file), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QString failure = LinkFixtures::runFraming(fixture);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
    }

    // ── Over a real connection ────────────────────────────────────────

    void aMessageCrossesBothWays()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QVERIFY(!pair.offerer->carriesBinary());
        QVERIFY(!pair.offerer->sendBinary(QByteArray::fromHex("0100")));
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::textReceived);
        QSignalSpy atDevice(pair.offerer.get(), &SessionTransport::textReceived);
        pair.offerer->sendText(QByteArrayLiteral("{\"type\":\"hello\"}"));
        pair.answerer->sendText(QByteArrayLiteral("{\"type\":\"auth.result\"}"));
        QTRY_COMPARE(atCore.count(), 1);
        QTRY_COMPARE(atDevice.count(), 1);
        QCOMPARE(atCore.first().first().toByteArray(), QByteArrayLiteral("{\"type\":\"hello\"}"));
        QCOMPARE(atDevice.first().first().toByteArray(),
                 QByteArrayLiteral("{\"type\":\"auth.result\"}"));
        // On one computer the far end has an address of its own.
        QVERIFY(!pair.answerer->peerAddress().isEmpty());
        QVERIFY(pair.offerer->telemetry().has_value());
    }

    // Control logging lane: while a message is delivered the transport
    // says how long it waited since the library's thread received it; the
    // wait is gone once the delivery returns. Measurement only.
    void aDeliveredMessageCarriesItsWaitSinceReceipt()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QVERIFY(!pair.answerer->deliveringMessageWaitUs().has_value());
        std::optional<qint64> seen;
        bool delivered = false;
        DataChannelTransport* const core = pair.answerer.get();
        connect(core, &SessionTransport::textReceived, this, [&](const QByteArray&) {
            seen = core->deliveringMessageWaitUs();
            delivered = true;
        });
        pair.offerer->sendText(QByteArrayLiteral("{\"type\":\"hello\"}"));
        QTRY_VERIFY(delivered);
        QVERIFY(seen.has_value());
        QVERIFY(*seen >= 0);
        QVERIFY(!core->deliveringMessageWaitUs().has_value());
        const SessionLinkDiagnostics link = core->linkDiagnostics();
        QCOMPARE(link.buffer, SessionLinkDiagnostics::Buffer::DataChannel);
        QVERIFY(!link.noDelay.has_value());
    }

    // R-R3-49: on a busy computer the connection failed with "DTLS alert:
    // unknown CA" at both ends. libdatachannel v0.24.5 gave the ICE agent
    // the far end's description before keeping it for the DTLS fingerprint
    // check, so a handshake that ran in between failed that check (the
    // Offerer taking its answer while the Answerer's checks and DTLS were
    // already under way). NereusSDR compiles it with the two in the other
    // order (cmake/patches/libdatachannel-keep-remote-description-first.cpp).
    // What each end logs shows the order: the description kept, then the
    // ICE agent adding the description's candidates, on the same thread.
    void eachEndKeepsTheFarEndsDescriptionBeforeIceTakesIt()
    {
        QMutex mutex;
        QList<QPair<quintptr, QString>> lines;
        testhooks::setDataChannelLibraryLog([&](quintptr thread, const QString& line) {
            const QMutexLocker lock(&mutex);
            lines.append({thread, line});
        });
        const auto logOff = qScopeGuard([] { testhooks::setDataChannelLibraryLog({}); });
        OpenPair pair;
        QVERIFY(pair.open());
        testhooks::setDataChannelLibraryLog({});

        const QMutexLocker lock(&mutex);
        int kept = 0;
        int taken = 0;
        QHash<quintptr, int> keptOnThread;
        for (const auto& [thread, line] : std::as_const(lines)) {
            if (line.contains(QLatin1String("Remote description kept before the ICE agent takes it"))) {
                ++kept;
                ++keptOnThread[thread];
            } else if (line.contains(QLatin1String("candidates from remote description"))) {
                ++taken;
                // The agent takes a description only after it was kept.
                QVERIFY2(keptOnThread.value(thread) > 0, qPrintable(line));
                --keptOnThread[thread];
            }
        }
        // One description each way: the offer, then the answer.
        QCOMPARE(kept, 2);
        QCOMPARE(taken, 2);
    }

    // R-R3-49: "the data channel did not open", still now and then after
    // the change above. libdatachannel v0.24.5's DtlsTransport::start()
    // took incoming records before it set the DTLS MTU, and a ClientHello
    // arriving in between ran the handshake with none; OpenSSL could not
    // write the server's flight and the answering end failed with "DTLS
    // recv: Handshake failed: fatal I/O error". NereusSDR compiles it with
    // the MTU set first
    // (cmake/patches/libdatachannel-set-dtls-mtu-before-incoming.cpp). What
    // each end logs shows the order: every DTLS start sets the MTU, then
    // registers for incoming records, on the same thread.
    void eachEndSetsTheDtlsMtuBeforeItTakesIncomingRecords()
    {
        QMutex mutex;
        QList<QPair<quintptr, QString>> lines;
        testhooks::setDataChannelLibraryLog([&](quintptr thread, const QString& line) {
            const QMutexLocker lock(&mutex);
            lines.append({thread, line});
        });
        const auto logOff = qScopeGuard([] { testhooks::setDataChannelLibraryLog({}); });
        OpenPair pair;
        QVERIFY(pair.open());
        testhooks::setDataChannelLibraryLog({});

        const QMutexLocker lock(&mutex);
        // A DTLS start on a thread, waiting for its MTU line.
        QHash<quintptr, bool> starting;
        QHash<quintptr, bool> mtuSet;
        int starts = 0;
        int inOrder = 0;
        for (const auto& [thread, line] : std::as_const(lines)) {
            if (line.contains(QLatin1String("DtlsTransport::start"))
                && line.contains(QLatin1String("Starting DTLS transport"))) {
                ++starts;
                starting[thread] = true;
                mtuSet[thread] = false;
            } else if (starting.value(thread)
                       && line.contains(QLatin1String("before incoming records are taken"))) {
                mtuSet[thread] = true;
            } else if (starting.value(thread)
                       && line.contains(QLatin1String("Registering incoming callback"))) {
                QVERIFY2(mtuSet.value(thread), qPrintable(line));
                starting[thread] = false;
                ++inOrder;
            }
        }
        // One DTLS transport at each end.
        QCOMPARE(starts, 2);
        QCOMPARE(inOrder, 2);
    }

    void a300KiBSettingsSnapshotCrossesInChunks()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QJsonObject settings;
        int index = 0;
        QByteArray wire;
        while (wire.size() < 300 * 1024) {
            for (int batch = 0; batch < 200; ++batch, ++index) {
                settings.insert(QStringLiteral("StationSetting%1").arg(index, 5, 10, QLatin1Char('0')),
                                QString::fromLatin1(patterned(64)));
            }
            wire = QJsonDocument(QJsonObject{{QStringLiteral("type"),
                                              QStringLiteral("settings.snapshot")},
                                             {QStringLiteral("settings"), settings}})
                       .toJson(QJsonDocument::Compact);
        }
        QVERIFY(wire.size() >= 300 * 1024);
        QSignalSpy atDevice(pair.offerer.get(), &SessionTransport::textReceived);
        const quint64 chunksBefore = pair.answerer->countsForTest().chunksSent;
        pair.answerer->sendText(wire);
        QTRY_COMPARE_WITH_TIMEOUT(atDevice.count(), 1, 15000);
        QCOMPARE(atDevice.first().first().toByteArray(), wire);
        const quint64 expected = static_cast<quint64>(
            (wire.size() + ControlFraming::kMaxChunkPayloadBytes - 1)
            / ControlFraming::kMaxChunkPayloadBytes);
        QCOMPARE(pair.answerer->countsForTest().chunksSent - chunksBefore, expected);
        QVERIFY(expected >= 5);
        QVERIFY(QJsonDocument::fromJson(atDevice.first().first().toByteArray()).isObject());
    }

    void aMessageOverTheCoresCapEndsTheConnection()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::textReceived);
        QSignalSpy coreClosed(pair.answerer.get(), &SessionTransport::closed);
        QSignalSpy deviceClosed(pair.offerer.get(), &SessionTransport::closed);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("larger than")));
        pair.offerer->sendText(patterned(static_cast<qsizetype>(kStationCap) + 1));
        QTRY_COMPARE_WITH_TIMEOUT(coreClosed.count(), 1, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(deviceClosed.count(), 1, 15000);
        QCOMPARE(atCore.count(), 0);
        QVERIFY(!pair.answerer->isOpen());
    }

    void aMessageOverTheDesktopsCapEndsTheConnection()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QSignalSpy atDevice(pair.offerer.get(), &SessionTransport::textReceived);
        QSignalSpy deviceClosed(pair.offerer.get(), &SessionTransport::closed);
        QSignalSpy coreClosed(pair.answerer.get(), &SessionTransport::closed);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("larger than")));
        // The Core may send what the Core may not receive: the desktop's cap
        // is eight times the Core's.
        pair.answerer->sendText(patterned(static_cast<qsizetype>(kClientCap) + 1));
        QTRY_COMPARE_WITH_TIMEOUT(deviceClosed.count(), 1, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(coreClosed.count(), 1, 15000);
        QCOMPARE(atDevice.count(), 0);
    }

    // Review Minor 3: whole messages waiting for a thread that has stopped
    // are bounded by bytes (kMaxQueuedCaps times the cap), not only by how
    // many there are: the connection ends past it.
    void messagesWaitingForABusyThreadAreBoundedByBytes()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::textReceived);
        QSignalSpy coreClosed(pair.answerer.get(), &SessionTransport::closed);
        const QByteArray message = patterned(static_cast<qsizetype>(kStationCap) - 64);
        const int count = static_cast<int>(DataChannelTransport::kMaxQueuedCaps) + 2;
        for (int i = 0; i < count; ++i) {
            pair.offerer->sendText(message);
        }
        // This thread stays busy (no events run) while the library's
        // threads carry the messages in; they wait for it, up to the bound.
        QElapsedTimer busy;
        busy.start();
        quint64 highest = 0;
        while (busy.elapsed() < 8000) {
            highest = std::max(highest, pair.answerer->pendingBytesForTest());
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        QVERIFY(highest <= kStationCap * DataChannelTransport::kMaxQueuedCaps);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("fell behind")));
        QTRY_COMPARE_WITH_TIMEOUT(coreClosed.count(), 1, 15000);
        QCOMPARE(atCore.count(), 0);
    }

    // Review Minor 3: messages held until something listens are bounded
    // by bytes too.
    void messagesHeldForNoListenerAreBoundedByBytes()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        // Nothing listens to the Core's end.
        QSignalSpy coreClosed(pair.answerer.get(), &SessionTransport::closed);
        const QByteArray message = patterned(static_cast<qsizetype>(kStationCap) - 64);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("past the bound")));
        const int count = static_cast<int>(DataChannelTransport::kMaxQueuedCaps) + 2;
        for (int i = 0; i < count; ++i) {
            pair.offerer->sendText(message);
            QTest::qWait(50);
        }
        QTRY_COMPARE_WITH_TIMEOUT(coreClosed.count(), 1, 20000);
    }

    // Review Minor 7: what was sent before closeLink() arrives, even behind
    // a backlog and with the transport deleted at once, as
    // StationServer::dropPeer does after its session.end.
    void theLastMessagesArriveWhenTheTransportIsDeletedAtItsClose()
    {
        auto offerer = std::make_unique<DataChannelTransport>();
        auto* answerer = new DataChannelTransport();
        QVERIFY(startDataChannelPair(offerer.get(), answerer, kClientCap, kStationCap, QString(),
                                     QString()));
        QVERIFY(waitFor([&] { return offerer->isOpen() && answerer->isOpen(); }, 15000));
        QSignalSpy atDevice(offerer.get(), &SessionTransport::textReceived);
        const QByteArray backlog = patterned(static_cast<qsizetype>(kStationCap) - 64);
        const QByteArray last = QByteArrayLiteral("{\"type\":\"session.end\"}");
        for (int i = 0; i < 3; ++i) {
            answerer->sendText(backlog);
        }
        answerer->sendText(last);
        answerer->closeLink(QStringLiteral("test"));
        delete answerer;
        QTRY_COMPARE_WITH_TIMEOUT(atDevice.count(), 4, 20000);
        QCOMPARE(atDevice.last().first().toByteArray(), last);
    }

    void anUnknownFrameEndsTheConnection()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QSignalSpy coreClosed(pair.answerer.get(), &SessionTransport::closed);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown kind")));
        QVERIFY(pair.offerer->sendRawFrameForTest(QByteArray::fromHex("0361")));
        QTRY_COMPARE_WITH_TIMEOUT(coreClosed.count(), 1, 15000);
    }

    void aPingIsAnsweredWithItsId()
    {
        OpenPair pair;
        QVERIFY(pair.open());
        QSignalSpy pongs(pair.offerer.get(), &SessionTransport::pongReceived);
        pair.offerer->ping();
        QTRY_COMPARE(pongs.count(), 1);
        // A pong for a ping never sent is not evidence of anything.
        QVERIFY(pair.answerer->sendRawFrameForTest(ControlFraming::pong(4000)));
        pair.offerer->ping();
        QTRY_COMPARE(pongs.count(), 2);
        QTest::qWait(50);
        QCOMPARE(pongs.count(), 2);
        pair.answerer->setAnswersPingsForTest(false);
        pair.offerer->ping();
        QVERIFY(waitFor([&pair] { return pair.answerer->countsForTest().pingsReceived == 3; },
                        5000));
        QCOMPARE(pongs.count(), 2);
    }

    // Loading the Core's certificate leaves nothing in this thread's
    // OpenSSL error queue, where Qt's OpenSSL TLS backend would read it as
    // its own error and end a wss:// connection on this thread.
    void presentingTheCoresCertificateLeavesNoOpenSslError()
    {
        Core core;
        ERR_clear_error();
        DataChannelTransport answerer;
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Answerer;
        options.maxIncomingBytes = kStationCap;
        options.certificatePemPath = core.server->certificatePemPath();
        options.privateKeyPemPath = core.server->privateKeyPemPath();
        QVERIFY(answerer.start(options));
        QCOMPARE(ERR_peek_error(), 0UL);
    }

    // A certificate file that cannot be read makes the peer throw while
    // the error is queued; the transport leaves the queue empty on that
    // path too.
    void aCertificateThatCannotBeReadLeavesNoOpenSslError()
    {
        QTemporaryDir dir;
        const QString bad = dir.filePath(QStringLiteral("not-a-certificate.pem"));
        QFile file(bad);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a certificate\n");
        file.close();
        ERR_clear_error();
        DataChannelTransport answerer;
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Answerer;
        options.maxIncomingBytes = kStationCap;
        options.certificatePemPath = bad;
        options.privateKeyPemPath = bad;
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("The control connection could not "
                                                               "start.*")));
        QVERIFY(!answerer.start(options));
        QCOMPARE(ERR_peek_error(), 0UL);
    }

    // A device's key whose point is not on the curve, and a signature
    // with r = 0, reach StationIdentity before anyone has signed in (a
    // pair.start, an auth.request, an introduction through the service).
    // Each makes OpenSSL queue an error; none is left behind, and a
    // wss:// session on this same thread goes on working after them.
    void aBadDeviceKeyOrSignatureLeavesNoOpenSslErrorAndAWssSessionSurvives()
    {
        Core core;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        QVERIFY2(core.server->listen(QHostAddress::LocalHost, 0),
                 qPrintable(core.server->lastError()));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        window.connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(core.server->serverPort())),
            QString(), QString(), false, core.server->stationIdentity().fingerprint());
        QTRY_VERIFY_WITH_TIMEOUT(window.isHandshakeComplete(), 20000);
        qInfo() << "TLS backend:" << QSslSocket::activeBackend();

        const QByteArray good = key->publicKeySpki();
        QCOMPARE(good.size(), StationIdentity::kSpkiBytes);
        QVERIFY(StationIdentity::isP256Spki(good));
        // The last byte of y changed: the point is off the curve.
        QByteArray offCurve = good;
        offCurve[offCurve.size() - 1] = static_cast<char>(offCurve.at(offCurve.size() - 1) ^ 0x01);
        // r = 0, s = 1.
        QByteArray zeroR(StationIdentity::kSignatureBytes, '\0');
        zeroR[zeroR.size() - 1] = 1;
        const QByteArray message = QByteArrayLiteral("a device's transcript");

        ERR_clear_error();
        QVERIFY(!StationIdentity::isP256Spki(offCurve));
        QCOMPARE(ERR_peek_error(), 0UL);
        QVERIFY(!StationIdentity::verify(offCurve, message, zeroR));
        QCOMPARE(ERR_peek_error(), 0UL);
        QVERIFY(!StationIdentity::verify(good, message, zeroR));
        QCOMPARE(ERR_peek_error(), 0UL);

        // The session on this thread still carries traffic both ways: a
        // heartbeat from the Core, answered.
        core.server->setHeartbeatIntervalMs(200);
        QTest::qWait(1500);
        QVERIFY(window.isHandshakeComplete());
        QVERIFY(core.server->hasAuthenticatedSession());
        window.disconnectFromStation(QStringLiteral("test done"));
        QTRY_VERIFY_WITH_TIMEOUT(!core.server->hasAuthenticatedSession(), 10000);
    }

    // Task 28 fix wave (review Important 4): media keeps this end's relay
    // only when the control connection's path is relayed.
    void mediaUsesTheRelayOnlyWhenTheControlPathIsRelayed()
    {
        IceConfiguration control = IceConfiguration::throughRendezvous(
            {QStringLiteral("stun:192.0.2.1:3478")}, true, AddressFamilies{true, false},
            HostFamilies{});
        RendezvousWire::Turn turn;
        turn.urls = {QStringLiteral("turn:192.0.2.1:3478?transport=udp")};
        turn.username = QStringLiteral("1790000000:abcdefghijklmnopqrstuvwxyz");
        turn.password = QStringLiteral("secret");
        QCOMPARE(control.setRelay(turn, 1), 1);

        const auto path = [](const QString& local, const QString& remote) {
            MediaIcePath p;
            p.localType = local;
            p.remoteType = remote;
            p.localAddress = QStringLiteral("198.51.100.2");
            p.remoteAddress = QStringLiteral("203.0.113.9");
            p.remotePort = 50000;
            return std::optional<MediaIcePath>(p);
        };
        // Direct (host or server-reflexive): no relay of this end's own.
        for (const auto& direct : {path(QStringLiteral("host"), QStringLiteral("host")),
                                   path(QStringLiteral("srflx"), QStringLiteral("prflx"))}) {
            const std::optional<IceConfiguration> media =
                DataChannelTransport::mediaIceFor(control, direct);
            QVERIFY(media.has_value());
            QCOMPARE(media->relayServers().size(), 0);
            QCOMPARE(media->stunServer(), control.stunServer());
            QVERIFY(media->relayAllowed());
            QVERIFY(media->relayKnown());
        }
        // A genuine same-computer ICE peer is still direct: its numeric
        // loopback address alone does not identify a locally owned shim.
        MediaIcePath localPeer = *path(QStringLiteral("host"), QStringLiteral("host"));
        localPeer.remoteAddress = QStringLiteral("127.0.0.1");
        QVERIFY(!localPeer.viaLoopbackShim());
        QCOMPARE(DataChannelTransport::mediaIceFor(control, localPeer)->relayServers().size(), 0);
        localPeer.remoteAddress = QStringLiteral("::1");
        QVERIFY(!localPeer.viaLoopbackShim());
        QCOMPARE(DataChannelTransport::mediaIceFor(control, localPeer)->relayServers().size(), 0);
        localPeer.remoteAddress = QStringLiteral("::ffff:127.0.0.1");
        QVERIFY(!localPeer.viaLoopbackShim());
        QCOMPARE(DataChannelTransport::mediaIceFor(control, localPeer)->relayServers().size(), 0);
        QCOMPARE(MediaIcePath::loopbackEndpoint(QStringLiteral("::ffff:127.0.0.1"), 4567),
                 MediaIcePath::loopbackEndpoint(QStringLiteral("127.0.0.1"), 4567));
        QVERIFY(MediaIcePath::loopbackEndpoint(QStringLiteral("::1"), 4567));
        QVERIFY(!MediaIcePath::loopbackEndpoint(QStringLiteral("192.0.2.1"), 4567));
        QVERIFY(!MediaIcePath::loopbackEndpoint(QStringLiteral("127.0.0.1"), 0));
        localPeer.ownedLoopbackShim = true;
        QVERIFY(localPeer.viaLoopbackShim());
        QCOMPARE(DataChannelTransport::mediaIceFor(control, localPeer)->relayServers(),
                 control.relayServers());
        // Relayed at either end, or through the far end's relay: kept.
        std::optional<MediaIcePath> viaFarRelay = path(QStringLiteral("srflx"), QStringLiteral("prflx"));
        viaFarRelay->farEndRelays.append(qMakePair(QStringLiteral("203.0.113.9"), quint16(50000)));
        for (const auto& relayed : {path(QStringLiteral("relay"), QStringLiteral("host")),
                                    path(QStringLiteral("host"), QStringLiteral("relay")),
                                    viaFarRelay}) {
            const std::optional<IceConfiguration> media =
                DataChannelTransport::mediaIceFor(control, relayed);
            QVERIFY(media.has_value());
            QCOMPARE(media->relayServers(), control.relayServers());
        }
        // No path yet: kept. Not through the service: none.
        QCOMPARE(DataChannelTransport::mediaIceFor(control, std::nullopt)->relayServers().size(), 1);
        QVERIFY(!DataChannelTransport::mediaIceFor(std::nullopt, path(QStringLiteral("host"),
                                                                     QStringLiteral("host")))
                     .has_value());
    }

    void onlyAnOwnedSourceCandidateMakesTheSelectedLoopbackAShim()
    {
        Core core;
        for (const bool selectedFromSource : {false, true}) {
            DataChannelTransport offerer;
            DataChannelTransport answerer;
            const auto source = std::make_shared<WatchSource>();
            bool descriptionsAccepted = true;
            QString pendingAnswer;
            QStringList offeredCandidates;
            QStringList answeredCandidates;
            QObject::connect(&offerer, &DataChannelTransport::localCandidate, &offerer,
                             [&](const QString& candidate) { offeredCandidates.append(candidate); });
            QObject::connect(&answerer, &DataChannelTransport::localCandidate, &answerer,
                             [&](const QString& candidate) { answeredCandidates.append(candidate); });
            QObject::connect(&offerer, &DataChannelTransport::localDescription, &answerer,
                             [&](const QString& sdp, const QString& type) {
                descriptionsAccepted &= answerer.acceptDescription(sdp, type);
            });
            QObject::connect(&answerer, &DataChannelTransport::localDescription, &offerer,
                             [&](const QString& sdp, const QString& type) {
                if (selectedFromSource) {
                    QCOMPARE(type, QStringLiteral("answer"));
                    pendingAnswer = sdp;
                } else {
                    descriptionsAccepted &= offerer.acceptDescription(sdp, type);
                }
            });
            DataChannelTransport::Options answer;
            answer.role = DataChannelTransport::Role::Answerer;
            answer.maxIncomingBytes = kStationCap;
            answer.ice = IceConfiguration::throughRendezvous({}, true, {}, {});
            answer.ice->setRelay(std::nullopt, 1);
            answer.certificatePemPath = core.server->certificatePemPath();
            answer.privateKeyPemPath = core.server->privateKeyPemPath();
            DataChannelTransport::Options offer;
            offer.role = DataChannelTransport::Role::Offerer;
            offer.maxIncomingBytes = kClientCap;
            offer.ice = watchIce(source);
            QVERIFY(answerer.start(answer));
            QVERIFY(offerer.start(offer));
            QVERIFY(!offerer.networkPathSnapshot());
            QTRY_VERIFY_WITH_TIMEOUT(descriptionsAccepted, 10000);
            QTRY_VERIFY_WITH_TIMEOUT(source->ready(), 10000);
            QTRY_VERIFY_WITH_TIMEOUT(!loopbackFor(offeredCandidates).isEmpty(), 10000);
            QTRY_VERIFY_WITH_TIMEOUT(!loopbackFor(answeredCandidates).isEmpty(), 10000);
            const QString actual = loopbackFor(answeredCandidates);
            const QStringList fields = actual.split(QLatin1Char(' '));
            QVERIFY(fields.size() >= 8);
            bool parsed = false;
            const int port = fields.at(5).toInt(&parsed);
            QVERIFY(parsed && port > 0 && port <= 65535);
            if (selectedFromSource) {
                QVERIFY(!pendingAnswer.isEmpty());
                // Exercise a source candidate queued before the remote
                // description: the eventual admission must retain origin.
                source->inject(actual);
                QVERIFY(offerer.acceptDescription(pendingAnswer, QStringLiteral("answer")));
            } else {
                // Same address and a forged wsrelay foundation, but the
                // owned source supplied a different port from the selected
                // ordinary candidate. Neither text nor IP confers trust.
                source->inject(RelayLeg::candidateLine(1,
                    static_cast<quint16>(port == 65535 ? port - 1 : port + 1)));
                QVERIFY(offerer.acceptCandidate(actual));
            }
            QVERIFY(answerer.acceptCandidate(loopbackFor(offeredCandidates)));
            QTRY_VERIFY_WITH_TIMEOUT(offerer.isOpen() && answerer.isOpen(), 15000);
            const auto selected = offerer.selectedPath();
            QVERIFY(selected);
            QCOMPARE(MediaIcePath::loopbackEndpoint(selected->remoteAddress, selected->remotePort),
                     MediaIcePath::loopbackEndpoint(QStringLiteral("127.0.0.1"),
                                                    static_cast<quint16>(port)));
            QCOMPARE(selected->viaLoopbackShim(), selectedFromSource);
            QVERIFY(!answerer.selectedPath()->viaLoopbackShim());
            const auto route = offerer.networkPathSnapshot();
            QVERIFY(route);
            if (selectedFromSource) {
                QCOMPARE(route->kind, NetworkPathSnapshot::Kind::Relayed);
                QCOMPARE(route->carrier, NetworkPathSnapshot::Carrier::WebRelay);
                QCOMPARE(route->remoteAddress, QStringLiteral("198.51.100.7"));
                QCOMPARE(route->remotePort, quint16(443));
                QVERIFY(route->localAddress.isEmpty());
            } else {
                QCOMPARE(route->kind, NetworkPathSnapshot::Kind::Direct);
                QCOMPARE(route->carrier, NetworkPathSnapshot::Carrier::Ice);
                QCOMPARE(route->remotePort, static_cast<quint16>(port));
                QVERIFY(route->localPort != 0);
            }
            offerer.closeLink(QStringLiteral("test done"));
            QVERIFY(!offerer.networkPathSnapshot());
        }
    }

    void closedControlSourceCannotQueueAnotherCandidate()
    {
        DataChannelTransport offerer;
        const auto source = std::make_shared<WatchSource>();
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Offerer;
        options.maxIncomingBytes = kClientCap;
        options.ice = watchIce(source);
        QVERIFY(offerer.start(options));
        QTRY_VERIFY_WITH_TIMEOUT(source->ready(), 10000);
        const auto staleCallback = source->callback();
        QVERIFY(staleCallback);
        offerer.closeLink(QStringLiteral("test done"));
        QCOMPARE(offerer.pendingSourceCandidateCountForTest(), 0);
        QCOMPARE(offerer.ownedShimEndpointCountForTest(), 0);
        staleCallback(RelayLeg::candidateLine(IceConfiguration::kControlLane, 65000));
        QCOMPARE(offerer.pendingSourceCandidateCountForTest(), 0);
        QCOMPARE(offerer.ownedShimEndpointCountForTest(), 0);
        QVERIFY(!offerer.selectedPath());
        QVERIFY(!offerer.networkPathSnapshot());
    }

    // ── A session over the channel ─────────────────────────────────────

    void aWholeSessionRunsOverTheChannel()
    {
        // Keep bounded startup evidence for the intermittent opening failure.
        // This does not extend the existing 15-second connection deadline.
        struct StartupEvidence {
            QElapsedTimer elapsed;
            QMutex mutex;
            QStringList libraryStages;
            QStringList pairEvents;
            int descriptionsEmitted[2]{};
            int candidatesEmitted[2]{};
        };
        const auto evidence = std::make_shared<StartupEvidence>();
        evidence->elapsed.start();
        testhooks::setDataChannelLibraryLog(
            [evidence](quintptr thread, const QString& line) {
                const QString stage = safeRtcStage(line);
                if (stage.isEmpty()) return;
                const QMutexLocker lock(&evidence->mutex);
                if (evidence->libraryStages.size() == 2000) evidence->libraryStages.removeFirst();
                evidence->libraryStages.append(QStringLiteral("%1 ms thread %2 %3")
                    .arg(evidence->elapsed.elapsed()).arg(thread).arg(stage));
            });
        const auto restoreLogger = qScopeGuard([evidence]() {
            testhooks::setDataChannelLibraryLog({});
            if (QTest::currentTestFailed()) {
                const QMutexLocker lock(&evidence->mutex);
                for (const QString& line : std::as_const(evidence->pairEvents)) {
                    qWarning().noquote() << line;
                }
                for (const QString& line : std::as_const(evidence->libraryStages)) {
                    qWarning().noquote() << line;
                }
            }
        });
        Core core;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        auto* offerer = new DataChannelTransport();
        auto* answerer = new DataChannelTransport();
        const QPointer<DataChannelTransport> offererLifetime(offerer);
        const QPointer<DataChannelTransport> answererLifetime(answerer);
        const auto releaseUnadopted = qScopeGuard([&]() {
            if (offererLifetime && !offererLifetime->parent()) delete offererLifetime.data();
            if (answererLifetime && !answererLifetime->parent()) delete answererLifetime.data();
        });
        QString offererFailure, answererFailure;
        QObject startupObserver;
        QObject::connect(offerer, &DataChannelTransport::failed, &startupObserver,
                         [&](const QString& reason) { offererFailure = reason; });
        QObject::connect(answerer, &DataChannelTransport::failed, &startupObserver,
                         [&](const QString& reason) { answererFailure = reason; });
        QObject::connect(answerer, &DataChannelTransport::opened, core.server.get(),
                         [&core, answerer] { core.server->acceptTransport(answerer); });
        const auto observePair = [evidence](DataChannelPairEvent event) {
            const QMutexLocker lock(&evidence->mutex);
            const int sideIndex = event.side == DataChannelPairEvent::Side::Offerer ? 0 : 1;
            const QString side = sideIndex == 0 ? QStringLiteral("offerer")
                                                : QStringLiteral("answerer");
            QString detail;
            switch (event.kind) {
            case DataChannelPairEvent::Kind::DescriptionEmitted:
                detail = QStringLiteral("description emitted count=%1")
                    .arg(++evidence->descriptionsEmitted[sideIndex]);
                break;
            case DataChannelPairEvent::Kind::DescriptionAccepted:
                detail = QStringLiteral("description accepted=%1").arg(int(event.accepted));
                break;
            case DataChannelPairEvent::Kind::CandidateEmitted:
                detail = QStringLiteral("candidate emitted count=%1")
                    .arg(++evidence->candidatesEmitted[sideIndex]);
                break;
            case DataChannelPairEvent::Kind::CandidateAccepted:
                detail = QStringLiteral("candidate accepted=%1").arg(int(event.accepted));
                break;
            case DataChannelPairEvent::Kind::GatheringComplete:
                detail = QStringLiteral("gathering complete");
                break;
            case DataChannelPairEvent::Kind::Opened:
                detail = QStringLiteral("opened");
                break;
            case DataChannelPairEvent::Kind::Failed:
                detail = QStringLiteral("failed");
                break;
            }
            if (evidence->pairEvents.size() == 256) evidence->pairEvents.removeFirst();
            evidence->pairEvents.append(QStringLiteral("%1 ms %2 %3")
                .arg(evidence->elapsed.elapsed()).arg(side, detail));
        };
        QVERIFY(startDataChannelPair(offerer, answerer, kClientCap, kStationCap,
                                     core.server->certificatePemPath(),
                                     core.server->privateKeyPemPath(), nullptr, observePair));
        const qint64 openingStarted = evidence->elapsed.elapsed();
        const bool opened = waitFor([offerer] { return offerer->isOpen(); }, 15000);
        const QString openState = QStringLiteral("wait=%1ms offererOpen=%2 answererOpen=%3 "
                                                "offererFailure=%4 answererFailure=%5")
            .arg(evidence->elapsed.elapsed() - openingStarted).arg(offerer->isOpen())
            .arg(answererLifetime && answererLifetime->isOpen())
            .arg(offererFailure, answererFailure);
        QVERIFY2(opened, qPrintable(openState));
        // The certificate the Core presented in DTLS is its own.
        QString hex = core.server->certificateFingerprint();
        hex.remove(QLatin1Char(':'));
        QCOMPARE(offerer->peerCertificateSha256(), QByteArray::fromHex(hex.toLatin1()));
        QVERIFY(answerer->peerCertificateSha256().isEmpty());

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        window.startSession(offerer, QString(), QString(),
                            core.server->stationIdentity().fingerprint());
        QTRY_VERIFY_WITH_TIMEOUT(window.isHandshakeComplete(), 20000);
        QVERIFY(core.server->hasAuthenticatedSession());
        window.disconnectFromStation(QStringLiteral("test done"));
        QTRY_VERIFY_WITH_TIMEOUT(!core.server->hasAuthenticatedSession(), 10000);
    }

    void aCoreThatIsNotTheBoundOneIsRefusedBeforeAnythingIsSent()
    {
        Core core;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        // Another certificate: the hello's binding does not cover it.
        QTemporaryDir otherDir;
        CertificateStore other(otherDir.path());
        QVERIFY(other.isValid());
        QVERIFY(other.fingerprintSha256() != core.server->certificateFingerprint());

        auto* offerer = new DataChannelTransport();
        auto* answerer = new DataChannelTransport();
        QObject::connect(answerer, &DataChannelTransport::opened, core.server.get(),
                         [&core, answerer] { core.server->acceptTransport(answerer); });
        QVERIFY(startDataChannelPair(offerer, answerer, kClientCap, kStationCap,
                                     other.certificatePath(), other.privateKeyPath()));
        QVERIFY(waitFor([offerer] { return offerer->isOpen(); }, 15000));
        const QPointer<DataChannelTransport> coreEnd(answerer);

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QSignalSpy ended(&window, &StationClient::sessionEnded);
        // What the desktop's end had sent when the session ended (read
        // then: the transport may go once it has).
        std::optional<quint64> sentByDevice;
        const QPointer<DataChannelTransport> deviceEnd(offerer);
        QObject::connect(&window, &StationClient::sessionEnded, &window, [&sentByDevice, deviceEnd] {
            if (deviceEnd) {
                sentByDevice = deviceEnd->countsForTest().messagesSent;
            }
        });
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral(".*")));
        window.startSession(offerer, QString(), QString(),
                            core.server->stationIdentity().fingerprint());
        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 20000);
        QCOMPARE(window.lastEndReport().kind, StationEndReport::Kind::IdentityChanged);
        QVERIFY(!window.isHandshakeComplete());
        // Nothing reached the Core from the desktop: not its hello, not its
        // sign-in.
        QVERIFY(sentByDevice.has_value());
        QCOMPARE(*sentByDevice, quint64(0));
        QVERIFY(coreEnd.isNull() || coreEnd->countsForTest().messagesDelivered == 0);
        QVERIFY(!core.server->hasAuthenticatedSession());
    }

    void theCoreDeclaresTheLinkDeadAfterTwoMissedPongs()
    {
        Core core;
        core.server->setHeartbeatIntervalMs(100);
        auto offerer = std::make_unique<DataChannelTransport>();
        auto* answerer = new DataChannelTransport();
        QObject::connect(answerer, &DataChannelTransport::opened, core.server.get(),
                         [&core, answerer] { core.server->acceptTransport(answerer); });
        QVERIFY(startDataChannelPair(offerer.get(), answerer, kClientCap, kStationCap,
                                     core.server->certificatePemPath(),
                                     core.server->privateKeyPemPath()));
        QSignalSpy received(offerer.get(), &SessionTransport::textReceived);
        QSignalSpy closed(offerer.get(), &SessionTransport::closed);
        QVERIFY(waitFor([&offerer] { return offerer->isOpen(); }, 15000));
        // A device that has gone silent without closing.
        offerer->setAnswersPingsForTest(false);
        QTRY_COMPARE_WITH_TIMEOUT(closed.count(), 1, 10000);
        QString last;
        for (const QList<QVariant>& message : std::as_const(received)) {
            last = QString::fromUtf8(message.first().toByteArray());
        }
        QVERIFY2(last.contains(QStringLiteral("session.end"))
                     && last.contains(QStringLiteral("stopped answering")),
                 qPrintable(last));
        // It was the third ping's tick: two went unanswered.
        QVERIFY(offerer->countsForTest().pingsReceived >= 2);
    }

    void theDesktopDeclaresTheLinkDeadAfterTwoMissedPongs()
    {
        Core core;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        auto* offerer = new DataChannelTransport();
        auto* answerer = new DataChannelTransport();
        QObject::connect(answerer, &DataChannelTransport::opened, core.server.get(),
                         [&core, answerer] { core.server->acceptTransport(answerer); });
        QVERIFY(startDataChannelPair(offerer, answerer, kClientCap, kStationCap,
                                     core.server->certificatePemPath(),
                                     core.server->privateKeyPemPath()));
        QVERIFY(waitFor([offerer] { return offerer->isOpen(); }, 15000));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QSignalSpy ended(&window, &StationClient::sessionEnded);
        window.startSession(offerer, QString(), QString(),
                            core.server->stationIdentity().fingerprint());
        QTRY_VERIFY_WITH_TIMEOUT(window.isHandshakeComplete(), 20000);
        // Allow normal sign-in before testing the deliberately short liveness
        // deadline. A busy test host must not time out during the snapshot.
        window.setHeartbeatIntervalMs(100);
        // The Core goes silent without closing.
        answerer->setAnswersPingsForTest(false);
        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 10000);
        QCOMPARE(ended.first().first().toString(), QStringLiteral("heartbeat timeout"));
    }
};

QTEST_MAIN(TstDataChannelTransport)
#include "tst_data_channel_transport.moc"
