// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_rendezvous_client.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 27 (R-IOS-08, R-IOS-16): reaching the Core through
// the remote access service (the rendezvous,
// docs/architecture/2026-09-23-rendezvous-v1.md).
//
// The conformance suite, rendezvous/conformance/v1/ (section 10):
//   - every crypto vector (strict base64url, the P-256 key check, the
//     rendezvous id, the registration and introduction signatures, the
//     TURN credentials);
//   - every control fixture the Core and the desktop receive or send, each
//     decoded (or refused) and encoded again;
//   - the Core's runner (section 10.4): this test plays the service towards
//     a RendezvousClient in the station role for every fixture whose runs
//     include "core", and the desktop's runner does the same towards one in
//     the client role for every fixture that includes "app".
//
// Then the Core against the real Python service (rendezvous/server), run
// on this computer with a small STUN and TURN fake beside it
// (tests/tools/fake_turn_server.py); nothing leaves this computer:
//   - a Core registers, a paired device's introduction arrives, and an ICE
//     connection completes with the service's STUN server in use;
//   - with direct paths blocked in the test (only relay candidates pass),
//     it completes through TURN over UDP;
//   - an introduction from a device the Core never paired, and from one it
//     revoked, is dropped without a reply and counted;
//   - pairing by code works through a mailbox: a recording relay between
//     the ends and the service sees every body forwarded unchanged and
//     never the code, and the service's log shows the mailbox;
//   - with the service stopped, a session that was running continues;
//   - IPv6 is preferred where both ends have it.
//
// And the desktop (Step 2): a saved Core's last good addresses are tried
// first, so with the service stopped a client with a cached address still
// connects; an address that does not answer, that another computer
// answers, or that never opens gives way to the next; each attempt is
// recorded path by path in plain words; the saved Core keeps its last good
// addresses, most recent first.
//
// Keys, codes, nonces and secrets are made at run time; nothing secret is
// printed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): a session through the
//               service and its relay given back, an answer with no
//               credentials and its retirement, the plain words of a
//               service connection; the Task 27 ICE tests answer
//               introductions themselves. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29: LocalService, Core and their
//               helpers moved to RendezvousTestHarness.h, shared with
//               tst_path_racer; a paired Core's addresses are raced, so the
//               cached address tests expect every address's line. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: R-R3-49 load round: the Task 28 session case waits for
//               both ends' control paths to settle on the direct pair
//               before it checks that media takes no relay. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: load finding: the relay-only case waits the product's ICE
//               connect bound (IceConfiguration::kConnectDeadlineMs) and on
//               failure prints both ends' progress, the relay's output and
//               the service's log. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: relay teardown finding: the fake relay exited on a datagram
//               it could not send, so no release reached it. The waits on
//               a connection through the fake relay and on its releases
//               stop at their bound (QTRY_* ran three times it, past
//               ctest's 300 s) and at once if the relay exits, with its
//               errors; new cases for the relay carrying on past such a
//               datagram and for both ends of a relayed connection closed
//               at once giving both allocations back. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: theServiceEndsWithTheProcessThatStartedIt: the service
//               LocalService starts ends with the process it is tied to.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-01: NEREUS_ICE_DIAG is read in initTestCase, so a CI run can
//               log its ICE checks. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QNetworkInterface>
#include <QProcess>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QtEndian>
#include <QWebSocket>
#include <QWebSocketServer>

#include <functional>
#include <memory>
#include <optional>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif

#include "core/AppSettings.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingCode.h"
#include "core/security/PairingWindow.h"
#include "core/security/SpakeExchange.h"
#include "core/security/StationIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/IceDiagnostics.h"
#include "core/session/RendezvousDialer.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/IceConfiguration.h"
#include "core/session/RendezvousClient.h"
#include "core/session/RendezvousMailboxTransport.h"
#include "core/session/RendezvousWire.h"
#include "core/session/StationPairingClient.h"
#include "core/session/StationRendezvous.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/SwitchableTransport.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/settings/SettingsProxy.h"
#include "gui/CoreTargetStore.h"
#include "models/RadioModel.h"

#include "OperatorWording.h"
#include "RendezvousTestHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "fakes/LoginProxy.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

using namespace NereusSDR;
namespace Wire = NereusSDR::RendezvousWire;
// Task 29: the service on this computer and a Core, shared with
// tst_path_racer.
using namespace NereusSDR::Test::Rendezvous;

namespace {

const QString kSuite = QStringLiteral(NEREUS_SOURCE_DIR "/rendezvous/conformance/v1");

QJsonObject readJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString b64(const QByteArray& bytes)
{
    return StationIdentity::toBase64Url(bytes);
}

QByteArray unb64(const QString& text)
{
    return StationIdentity::fromBase64Url(text);
}

std::shared_ptr<TestKey> makeKey()
{
    return std::make_shared<TestKey>();
}

QString turnPassword(const QByteArray& secret, const QString& username)
{
    return QString::fromLatin1(
        QMessageAuthenticationCode::hash(username.toUtf8(), secret, QCryptographicHash::Sha1)
            .toBase64());
}

// ── The service played by a runner (section 10.4) ──────────────────────

class ServicePlayer : public QObject {
public:
    ServicePlayer() : m_server(QStringLiteral("rendezvous-runner"), QWebSocketServer::NonSecureMode)
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&m_server, &QWebSocketServer::newConnection, this, [this] {
            while (m_server.hasPendingConnections()) {
                QWebSocket* socket = m_server.nextPendingConnection();
                socket->setParent(this);
                hostHeaders.append(QString::fromLatin1(socket->request().rawHeader("Host")));
                // How many of this player's earlier connections were still
                // open when this one arrived.
                openAtArrival.append(int(m_inbox.size() - m_closed.size()));
                auto* queue = new QStringList;
                m_inbox.insert(socket, queue);
                QObject::connect(socket, &QWebSocket::textMessageReceived, this,
                                 [queue](const QString& text) { queue->append(text); });
                QObject::connect(socket, &QWebSocket::disconnected, this,
                                 [this, socket] { m_closed.insert(socket); });
                m_pending.append(socket);
            }
        });
    }

    ~ServicePlayer() override
    {
        // The sockets are children, destroyed after the members: silence
        // them first, so a close during teardown touches nothing gone.
        for (auto it = m_inbox.cbegin(); it != m_inbox.cend(); ++it) {
            QObject::disconnect(it.key(), nullptr, this, nullptr);
        }
        qDeleteAll(m_inbox);
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(m_server.serverPort()));
    }

    QWebSocket* waitForConnection(int timeoutMs = 5000)
    {
        QDeadlineTimer deadline(timeoutMs);
        while (m_pending.isEmpty() && !deadline.hasExpired()) {
            QTest::qWait(5);
        }
        return m_pending.isEmpty() ? nullptr : m_pending.takeFirst();
    }

    std::optional<QString> waitForMessage(QWebSocket* socket, int timeoutMs = 5000)
    {
        QStringList* queue = m_inbox.value(socket);
        if (queue == nullptr) {
            return std::nullopt;
        }
        QDeadlineTimer deadline(timeoutMs);
        while (queue->isEmpty() && !deadline.hasExpired()) {
            QTest::qWait(5);
        }
        if (queue->isEmpty()) {
            return std::nullopt;
        }
        return queue->takeFirst();
    }

    /// The Host header of each connection's opening request.
    QStringList hostHeaders;
    /// For each connection, how many earlier ones were still open (the
    /// peer had not closed them) when it arrived.
    QList<int> openAtArrival;

    quint16 port() const { return m_server.serverPort(); }

    bool silentFor(QWebSocket* socket, int ms)
    {
        QTest::qWait(ms);
        QStringList* queue = m_inbox.value(socket);
        return queue == nullptr || queue->isEmpty();
    }

private:
    QWebSocketServer m_server;
    QList<QWebSocket*> m_pending;
    QHash<QWebSocket*, QStringList*> m_inbox;
    QSet<QWebSocket*> m_closed;
};

// Task 28 tail (R-IOS-16): a lossy path between the Core and a service,
// for the Core's pings. A TCP relay on this computer that passes
// everything through, except that the service's pongs can be dropped (the
// next N, or all of them) or held for a while, with everything the service
// sends after a held pong held behind it, as a stalled TCP path would.
// Only the service's frames are read (unmasked, after the upgrade answer);
// the Core's go through untouched.
class LossyLink : public QObject {
public:
    explicit LossyLink(quint16 upstreamPort) : m_upstreamPort(upstreamPort)
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (m_server.hasPendingConnections()) {
                adopt(m_server.nextPendingConnection());
            }
        });
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(m_server.serverPort()));
    }

    /// Drops the service's next `count` pongs.
    void dropNextPongs(int count) { m_dropNext = count; }
    /// Drops every pong from now on, until set false.
    void setDropAllPongs(bool drop) { m_dropAll = drop; }
    /// Holds each pong (and what follows it) this long.
    void setPongDelayMs(int ms) { m_delayMs = ms; }

    int pongsSeen() const { return m_pongsSeen; }
    int pongsDropped() const { return m_pongsDropped; }
    /// For each connection the Core opened, how many of its earlier ones
    /// were still open at this end when it arrived.
    QList<int> openAtArrival;

private:
    struct Pipe {
        QPointer<QTcpSocket> down;
        QPointer<QTcpSocket> up;
        QByteArray fromUp;
        bool upgraded = false;
        bool holding = false;
        QByteArray held;
    };

    void adopt(QTcpSocket* down)
    {
        int open = 0;
        for (const auto& pipe : m_pipes) {
            if (pipe->down && pipe->down->state() == QAbstractSocket::ConnectedState) {
                ++open;
            }
        }
        openAtArrival.append(open);
        auto owned = std::make_unique<Pipe>();
        Pipe* pipe = owned.get();
        m_pipes.push_back(std::move(owned));
        down->setParent(this);
        auto* up = new QTcpSocket(this);
        pipe->down = down;
        pipe->up = up;
        QObject::connect(down, &QTcpSocket::readyRead, this,
                         [pipe] { if (pipe->up) { pipe->up->write(pipe->down->readAll()); } });
        QObject::connect(up, &QTcpSocket::readyRead, this, [this, pipe] { fromService(pipe); });
        QObject::connect(down, &QTcpSocket::disconnected, this,
                         [pipe] { if (pipe->up) { pipe->up->abort(); } });
        QObject::connect(up, &QTcpSocket::disconnected, this,
                         [pipe] { if (pipe->down) { pipe->down->disconnectFromHost(); } });
        up->connectToHost(QHostAddress::LocalHost, m_upstreamPort);
    }

    void toCore(Pipe* pipe, const QByteArray& bytes)
    {
        if (pipe->holding) {
            pipe->held.append(bytes);
        } else if (pipe->down) {
            pipe->down->write(bytes);
        }
    }

    void fromService(Pipe* pipe)
    {
        pipe->fromUp.append(pipe->up->readAll());
        if (!pipe->upgraded) {
            const qsizetype end = pipe->fromUp.indexOf("\r\n\r\n");
            if (end < 0) {
                return;
            }
            toCore(pipe, pipe->fromUp.left(end + 4));
            pipe->fromUp.remove(0, end + 4);
            pipe->upgraded = true;
        }
        for (;;) {
            const QByteArray& b = pipe->fromUp;
            if (b.size() < 2) {
                return;
            }
            const quint8 first = static_cast<quint8>(b.at(0));
            const quint8 second = static_cast<quint8>(b.at(1));
            qint64 length = second & 0x7f;
            qsizetype header = 2;
            if (length == 126) {
                if (b.size() < 4) {
                    return;
                }
                length = (qint64(quint8(b.at(2))) << 8) | quint8(b.at(3));
                header = 4;
            } else if (length == 127) {
                if (b.size() < 10) {
                    return;
                }
                length = 0;
                for (int i = 2; i < 10; ++i) {
                    length = (length << 8) | quint8(b.at(i));
                }
                header = 10;
            }
            if ((second & 0x80) != 0) {
                header += 4;
            }
            const qsizetype total = header + qsizetype(length);
            if (b.size() < total) {
                return;
            }
            const QByteArray frame = b.left(total);
            pipe->fromUp.remove(0, total);
            if ((first & 0x0f) == 0x0a) {
                ++m_pongsSeen;
                if (m_dropAll || m_dropNext > 0) {
                    if (m_dropNext > 0) {
                        --m_dropNext;
                    }
                    ++m_pongsDropped;
                    continue;
                }
                if (m_delayMs > 0 && !pipe->holding) {
                    pipe->holding = true;
                    QTimer::singleShot(m_delayMs, this, [pipe] {
                        pipe->holding = false;
                        if (pipe->down) {
                            pipe->down->write(pipe->held);
                        }
                        pipe->held.clear();
                    });
                }
            }
            toCore(pipe, frame);
        }
    }

    QTcpServer m_server;
    quint16 m_upstreamPort = 0;
    std::vector<std::unique_ptr<Pipe>> m_pipes;
    int m_dropNext = 0;
    bool m_dropAll = false;
    int m_delayMs = 0;
    int m_pongsSeen = 0;
    int m_pongsDropped = 0;
};

// ── Placeholders (section 10.4, and link section 16.1) ─────────────────

struct Context {
    bool coreMode = true;
    QHash<QString, QJsonValue> recorded;
    // Station keys by name: runner-made (a private key), or learned from
    // the Core (its public key only).
    QHash<QString, std::shared_ptr<TestKey>> stationKeys;
    QHash<QString, QByteArray> stationSpki;
    QHash<QString, std::shared_ptr<TestKey>> devices;
    QByteArray turnSecret = randomBytes(24).toHex();
    qint64 wallClock = 1800000000;
    qint64 advancedMs = 0;
    qint64 turnTtlSeconds = 86400;
    QStringList stunUrls{QStringLiteral("stun:rv6.conformance.invalid:3478"),
                         QStringLiteral("stun:rv4.conformance.invalid:3478")};
    QStringList turnUrls{QStringLiteral("turn:rv6.conformance.invalid:3478?transport=udp"),
                         QStringLiteral("turn:rv4.conformance.invalid:3478?transport=udp")};
    QString offerSdp = readText(kSuite + QStringLiteral("/sdp/offer.sdp"));
    QString answerSdp = readText(kSuite + QStringLiteral("/sdp/answer.sdp"));

    void setup(const QJsonObject& serverSetup)
    {
        if (serverSetup.contains(QStringLiteral("stunUrls"))) {
            stunUrls.clear();
            for (const QJsonValue& url : serverSetup.value(QStringLiteral("stunUrls")).toArray()) {
                stunUrls.append(url.toString());
            }
        }
        if (serverSetup.contains(QStringLiteral("turnUrls"))) {
            turnUrls.clear();
            for (const QJsonValue& url : serverSetup.value(QStringLiteral("turnUrls")).toArray()) {
                turnUrls.append(url.toString());
            }
        }
        if (serverSetup.contains(QStringLiteral("turnTtlSeconds"))) {
            turnTtlSeconds = serverSetup.value(QStringLiteral("turnTtlSeconds")).toInteger();
        }
        if (serverSetup.contains(QStringLiteral("wallClock"))) {
            wallClock = serverSetup.value(QStringLiteral("wallClock")).toInteger();
        }
    }

    QByteArray spkiOf(const QString& name)
    {
        if (stationSpki.contains(name)) {
            return stationSpki.value(name);
        }
        auto key = makeKey();
        stationKeys.insert(name, key);
        stationSpki.insert(name, key->spki());
        return key->spki();
    }

    std::shared_ptr<TestKey> device(const QString& name)
    {
        if (!devices.contains(name)) {
            devices.insert(name, makeKey());
        }
        return devices.value(name);
    }

    QByteArray recordedBytes(const QString& name) const
    {
        return unb64(recorded.value(name).toString());
    }

    QJsonObject turnObject(const QString& station)
    {
        const qint64 expires = wallClock + advancedMs / 1000 + turnTtlSeconds;
        const QString username = QStringLiteral("%1:%2").arg(expires).arg(
            Wire::rendezvousId(spkiOf(station)));
        return QJsonObject{
            {QStringLiteral("username"), username},
            {QStringLiteral("password"), turnPassword(turnSecret, username)},
            {QStringLiteral("expires"), static_cast<double>(expires)},
            {QStringLiteral("urls"), QJsonArray::fromStringList(turnUrls)},
        };
    }

    QByteArray signatureCase(const std::shared_ptr<TestKey>& key, QByteArray transcript,
                             const QString& which, const QByteArray& otherTranscript)
    {
        if (which == QLatin1String("otherNonce")) {
            transcript = otherTranscript;
        }
        QByteArray signature = key ? key->sign(transcript) : QByteArray();
        if (which == QLatin1String("flippedBit") && !signature.isEmpty()) {
            signature[signature.size() - 1] = static_cast<char>(signature.back() ^ 0x01);
        }
        return signature;
    }

    QJsonValue fill(const QJsonValue& value)
    {
        if (value.isObject()) {
            QJsonObject object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                it.value() = fill(it.value());
            }
            return object;
        }
        if (value.isArray()) {
            QJsonArray array;
            for (const QJsonValue& entry : value.toArray()) {
                array.append(fill(entry));
            }
            return array;
        }
        if (!value.isString() || !value.toString().startsWith(QLatin1Char('$'))) {
            return value;
        }
        const QString text = value.toString();
        const QStringList parts = text.split(QLatin1Char(':'));
        const QString kind = parts.value(0);
        if (kind == QLatin1String("$b64")) {
            const QString filled = b64(randomBytes(parts.value(1).toInt()));
            recorded.insert(parts.value(2), filled);
            return filled;
        }
        if (kind == QLatin1String("$ref")) {
            return recorded.value(parts.value(1));
        }
        if (kind == QLatin1String("$key")) {
            const QByteArray spki = spkiOf(parts.value(1));
            return parts.value(2) == QLatin1String("id") ? QJsonValue(Wire::rendezvousId(spki))
                                                         : QJsonValue(b64(spki));
        }
        if (kind == QLatin1String("$device")) {
            return b64(StationIdentity::fingerprintOf(device(parts.value(1))->spki()));
        }
        if (kind == QLatin1String("$introduce")) {
            const QString id = Wire::rendezvousId(spkiOf(parts.value(2)));
            const QByteArray signature = signatureCase(
                device(parts.value(1)),
                Wire::introduceTranscript(id, recordedBytes(parts.value(3))), parts.value(4),
                Wire::introduceTranscript(id, randomBytes(32)));
            recorded.insert(text, b64(signature));
            return b64(signature);
        }
        if (kind == QLatin1String("$register")) {
            spkiOf(parts.value(1));
            const QByteArray signature = signatureCase(
                stationKeys.value(parts.value(1)),
                Wire::registerTranscript(recordedBytes(parts.value(2))), parts.value(3),
                Wire::registerTranscript(randomBytes(32)));
            recorded.insert(text, b64(signature));
            return b64(signature);
        }
        if (kind == QLatin1String("$sdp")) {
            const QString sdp = parts.value(1) == QLatin1String("offer") ? offerSdp : answerSdp;
            recorded.insert(parts.value(2), sdp);
            return sdp;
        }
        if (kind == QLatin1String("$candidate")) {
            const QString candidate =
                QStringLiteral("candidate:1 1 UDP 2122317823 127.0.0.1 50000 typ host");
            recorded.insert(parts.value(1), candidate);
            return candidate;
        }
        if (kind == QLatin1String("$turn")) {
            const QJsonObject turn = turnObject(parts.value(1));
            recorded.insert(parts.value(2), turn);
            return turn;
        }
        return value;
    }

    bool match(const QJsonValue& expected, const QJsonValue& actual, QString* why)
    {
        const auto fail = [why](const QString& text) {
            *why = text;
            return false;
        };
        if (expected.isObject()) {
            if (!actual.isObject()) {
                return fail(QStringLiteral("not an object"));
            }
            const QJsonObject want = expected.toObject();
            const QJsonObject got = actual.toObject();
            QStringList wantKeys = want.keys();
            QStringList gotKeys = got.keys();
            wantKeys.sort();
            gotKeys.sort();
            if (wantKeys != gotKeys) {
                return fail(QStringLiteral("keys %1, expected %2")
                                .arg(gotKeys.join(QLatin1Char(',')),
                                     wantKeys.join(QLatin1Char(','))));
            }
            // A public key before the id derived from it.
            std::stable_sort(wantKeys.begin(), wantKeys.end(), [&want](const QString& a,
                                                                      const QString& b) {
                const bool aKey = want.value(a).toString().endsWith(QLatin1String(":publicKey"));
                const bool bKey = want.value(b).toString().endsWith(QLatin1String(":publicKey"));
                return aKey && !bKey;
            });
            for (const QString& key : std::as_const(wantKeys)) {
                QString inner;
                if (!match(want.value(key), got.value(key), &inner)) {
                    return fail(key + QStringLiteral(": ") + inner);
                }
            }
            return true;
        }
        if (expected.isArray()) {
            const QJsonArray want = expected.toArray();
            const QJsonArray got = actual.toArray();
            if (!actual.isArray() || want.size() != got.size()) {
                return fail(QStringLiteral("array differs"));
            }
            for (qsizetype index = 0; index < want.size(); ++index) {
                if (!match(want.at(index), got.at(index), why)) {
                    return false;
                }
            }
            return true;
        }
        if (!expected.isString() || !expected.toString().startsWith(QLatin1Char('$'))) {
            return expected == actual ? true : fail(QStringLiteral("value differs"));
        }
        const QString text = expected.toString();
        const QStringList parts = text.split(QLatin1Char(':'));
        const QString kind = parts.value(0);
        if (kind == QLatin1String("$any")) {
            return true;
        }
        if (kind == QLatin1String("$string") || kind == QLatin1String("$capture")) {
            if (kind == QLatin1String("$string") && !actual.isString()) {
                return fail(QStringLiteral("not a string"));
            }
            if (parts.size() > 1) {
                recorded.insert(parts.value(1), actual);
            }
            return true;
        }
        if (kind == QLatin1String("$int")) {
            if (!actual.isDouble() || actual.toDouble() != std::floor(actual.toDouble())) {
                return fail(QStringLiteral("not a whole number"));
            }
            if (parts.size() > 1) {
                recorded.insert(parts.value(1), actual);
            }
            return true;
        }
        if (kind == QLatin1String("$b64")) {
            bool ok = false;
            const QByteArray bytes = StationIdentity::fromBase64Url(actual.toString(), &ok);
            if (!actual.isString() || !ok || bytes.size() != parts.value(1).toInt()) {
                return fail(QStringLiteral("not base64url of the right length"));
            }
            recorded.insert(parts.value(2), actual);
            return true;
        }
        if (kind == QLatin1String("$ref")) {
            return recorded.value(parts.value(1)) == actual ? true
                                                           : fail(QStringLiteral("not the recorded value"));
        }
        if (kind == QLatin1String("$key")) {
            const QString name = parts.value(1);
            if (parts.value(2) == QLatin1String("publicKey")) {
                bool ok = false;
                const QByteArray spki = StationIdentity::fromBase64Url(actual.toString(), &ok);
                if (!ok || !StationIdentity::isP256Spki(spki)) {
                    return fail(QStringLiteral("not a canonical P-256 key"));
                }
                if (coreMode && !stationSpki.contains(name)) {
                    stationSpki.insert(name, spki);
                    return true;
                }
                return spki == spkiOf(name) ? true : fail(QStringLiteral("another key"));
            }
            return actual.toString() == Wire::rendezvousId(spkiOf(name))
                       ? true
                       : fail(QStringLiteral("not the id of the key"));
        }
        if (kind == QLatin1String("$device")) {
            return actual.toString() == b64(StationIdentity::fingerprintOf(device(parts.value(1))->spki()))
                       ? true
                       : fail(QStringLiteral("not the device's id"));
        }
        if (kind == QLatin1String("$register")) {
            const QByteArray signature = unb64(actual.toString());
            return StationIdentity::verify(spkiOf(parts.value(1)),
                                           Wire::registerTranscript(recordedBytes(parts.value(2))),
                                           signature)
                       ? true
                       : fail(QStringLiteral("the registration signature does not verify"));
        }
        if (kind == QLatin1String("$introduce")) {
            const QByteArray signature = unb64(actual.toString());
            const QString id = Wire::rendezvousId(spkiOf(parts.value(2)));
            return StationIdentity::verify(device(parts.value(1))->spki(),
                                           Wire::introduceTranscript(id, recordedBytes(parts.value(3))),
                                           signature)
                       ? true
                       : fail(QStringLiteral("the introduction signature does not verify"));
        }
        if (kind == QLatin1String("$sdp")) {
            if (!actual.isString() || actual.toString().isEmpty()) {
                return fail(QStringLiteral("not a description"));
            }
            recorded.insert(parts.value(2), actual);
            return true;
        }
        if (kind == QLatin1String("$candidate")) {
            if (!actual.isString() || !Wire::isCandidate(actual.toString())) {
                return fail(QStringLiteral("not a candidate"));
            }
            recorded.insert(parts.value(1), actual);
            return true;
        }
        if (kind == QLatin1String("$turn")) {
            const QJsonObject turn = turnObject(parts.value(1));
            recorded.insert(parts.value(2), turn);
            return QJsonValue(turn) == actual ? true : fail(QStringLiteral("other credentials"));
        }
        return fail(QStringLiteral("unknown placeholder ") + kind);
    }
};

QString compact(const QJsonValue& value)
{
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
}

// Section 5.3's listed keys, for the control fixtures' comparison.
QStringList listedKeys(Wire::Direction direction, Wire::Kind kind)
{
    using D = Wire::Direction;
    using K = Wire::Kind;
    const bool fromStation = direction == D::StationToService;
    const bool toStation = direction == D::ServiceToStation;
    switch (kind) {
    case K::Hello: return {"type", "version", "nonce", "stun"};
    case K::Register: return {"type", "id", "publicKey"};
    case K::Challenge: return {"type", "nonce"};
    case K::Prove: return {"type", "signature"};
    case K::Registered: return {"type", "id"};
    case K::Introduce: return {"type", "id", "device", "deviceSignature", "offer"};
    case K::Introduction: return {"type", "from", "device", "deviceSignature", "offer", "nonce"};
    case K::Answer:
        return fromStation ? QStringList{"type", "to", "answer", "turn"}
                           : QStringList{"type", "answer", "turn"};
    case K::Credentials: return {"type", "from", "turn"};
    case K::Candidate:
        if (fromStation) {
            return {"type", "to", "candidate"};
        }
        return toStation ? QStringList{"type", "from", "candidate"}
                         : QStringList{"type", "candidate"};
    case K::IntroductionEnd:
        return toStation ? QStringList{"type", "from", "code"} : QStringList{"type", "code"};
    case K::Nameplate:
    case K::MailboxOpen:
    case K::MailboxOpened:
        return {"type", "nameplate"};
    case K::Mailbox: return {"type", "body"};
    case K::MailboxClosed: return {"type", "code"};
    case K::Error: return {"type", "code", "reason", "retryAfterMs"};
    case K::RelayGrant:
        return toStation ? QStringList{"type", "from", "url", "token", "expires"}
                         : QStringList{"type", "url", "token", "expires"};
    default: return {"type"};
    }
}

QJsonObject restricted(const QJsonObject& object, const QStringList& keys)
{
    QJsonObject result;
    for (const QString& key : keys) {
        if (!object.contains(key)) {
            continue;
        }
        QJsonValue value = object.value(key);
        if (key == QLatin1String("turn") && value.isObject()) {
            value = restricted(value.toObject(), {"username", "password", "expires", "urls"});
        }
        result.insert(key, value);
    }
    return result;
}

// ── The Python service and the STUN and TURN fake, on this computer ────

// A relay between the ends and the service that records every message
// both ways, so a test can see what the service was given.
class RecordingRelay : public QObject {
public:
    explicit RecordingRelay(const QUrl& service)
        : m_service(service)
        , m_server(QStringLiteral("recording-relay"), QWebSocketServer::NonSecureMode)
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&m_server, &QWebSocketServer::newConnection, this, [this] {
            while (m_server.hasPendingConnections()) {
                QWebSocket* inner = m_server.nextPendingConnection();
                inner->setParent(this);
                auto* outer = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
                auto* backlog = new QStringList;
                m_backlogs.append(backlog);
                QObject::connect(inner, &QWebSocket::textMessageReceived, this,
                                 [this, outer, backlog](const QString& text) {
                    toService.append(text);
                    if (outer->state() == QAbstractSocket::ConnectedState) {
                        outer->sendTextMessage(text);
                    } else {
                        backlog->append(text);
                    }
                });
                QObject::connect(outer, &QWebSocket::connected, this, [outer, backlog] {
                    for (const QString& text : std::as_const(*backlog)) {
                        outer->sendTextMessage(text);
                    }
                    backlog->clear();
                });
                QObject::connect(outer, &QWebSocket::textMessageReceived, this,
                                 [this, inner](const QString& text) {
                    fromService.append(text);
                    inner->sendTextMessage(text);
                });
                QObject::connect(inner, &QWebSocket::disconnected, outer, [outer] { outer->close(); });
                QObject::connect(outer, &QWebSocket::disconnected, inner, [inner] { inner->close(); });
                outer->open(m_service);
            }
        });
    }

    ~RecordingRelay() override { qDeleteAll(m_backlogs); }

    QUrl url() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(m_server.serverPort()));
    }

    QStringList toService;
    QStringList fromService;

private:
    QUrl m_service;
    QWebSocketServer m_server;
    QList<QStringList*> m_backlogs;
};

// Both ends of one connection through the service: the client's offer and
// the Core's answer each on a LibDataChannelMediaTransport started with the
// service's ICE settings, candidates trickled through the service. With
// `relayOnly`, only relay candidates are passed on in either direction:
// the test's stand-in for a network where direct UDP is blocked.
class IcePair : public QObject {
public:
    IcePair(RendezvousClient* station, RendezvousClient* client, bool relayOnly)
        : m_station(station), m_client(client), m_relayOnly(relayOnly)
    {
        QObject::connect(station, &RendezvousClient::introduced, this,
                         [this](const RendezvousIntroduction& introduction) {
            onIntroduced(introduction);
        });
        QObject::connect(station, &RendezvousClient::credentialsReceived, this,
                         [this](const QByteArray&, bool offered, const Wire::Turn& turn) {
            IceConfiguration ice = *m_stationIce;
            ice.setRelay(offered ? std::optional<Wire::Turn>(turn) : std::nullopt, 1);
            stationRelays = ice.relayServers().size();
            m_answerer->gatherCandidates(ice.relayServers());
        });
        QObject::connect(station, &RendezvousClient::candidateReceived, this,
                         [this](const QByteArray&, const QString& candidate) {
            ++stationHeard;
            if (!candidate.isEmpty() && m_answerer && passes(candidate)) {
                m_answerer->acceptCandidate(candidate, QString());
            }
        });
        QObject::connect(client, &RendezvousClient::answerReceived, this,
                         [this](const QString& sdp, bool offered, const Wire::Turn& turn) {
            QVERIFY(m_offerer->acceptDescription(sdp, QStringLiteral("answer")));
            IceConfiguration ice = *m_clientIce;
            ice.setRelay(offered ? std::optional<Wire::Turn>(turn) : std::nullopt, 1);
            clientRelays = ice.relayServers().size();
            m_offerer->gatherCandidates(ice.relayServers());
        });
        QObject::connect(client, &RendezvousClient::candidateReceived, this,
                         [this](const QByteArray&, const QString& candidate) {
            if (!candidate.isEmpty() && passes(candidate)) {
                m_offerer->acceptCandidate(candidate, QString());
            }
        });
    }

    // Connects the client to the service, then makes its offer with the
    // service's STUN server and introduces it to the Core as `device`.
    void start(const QString& stationId, std::shared_ptr<TestKey> device)
    {
        m_device = std::move(device);
        QObject::connect(m_client, &RendezvousClient::connected, this, [this, stationId] {
            if (m_offerer) {
                return;
            }
            // Nothing known about either family on the loopback: the service's
            // first entry, one relay.
            m_clientIce = IceConfiguration::throughRendezvous(m_client->stunUrls(), true,
                                                              AddressFamilies{}, HostFamilies{});
            m_offerer = std::make_unique<LibDataChannelMediaTransport>();
            QObject::connect(m_offerer.get(), &IMediaTransport::localDescription, this,
                             [this, stationId](const QString& sdp, const QString&) {
                const std::shared_ptr<TestKey> key = m_device;
                m_client->introduce(stationId, key->spki(),
                                    [key](const QByteArray& message) { return key->sign(message); },
                                    sdp);
            });
            QObject::connect(m_offerer.get(), &IMediaTransport::localCandidate, this,
                             [this](const QString& candidate, const QString&) {
                clientCandidates.append(candidate);
                m_client->sendCandidate(candidate);
            });
            QObject::connect(m_offerer.get(), &IMediaTransport::gatheringComplete, this, [this] {
                clientGathered = true;
                m_client->sendCandidate(QString());
            });
            QObject::connect(m_offerer.get(), &IMediaTransport::ready, this,
                             [this] { clientReady = true; });
            QObject::connect(m_offerer.get(), &IMediaTransport::displayReceived, this,
                             [this](const QByteArray& message) { clientReceived.append(message); });
            IMediaTransport::StartOptions options{IMediaTransport::Role::Offerer, 0x1111};
            options.ice = m_clientIce;
            QVERIFY(m_offerer->start(options));
        });
        m_client->connectToService();
    }

    LibDataChannelMediaTransport* offerer() const { return m_offerer.get(); }
    LibDataChannelMediaTransport* answerer() const { return m_answerer.get(); }

    // Both ends closed and destroyed in the same turn of the event loop:
    // each peer's close races the other end's teardown, with nothing
    // waiting for either.
    void closeBothAtOnce()
    {
        m_offerer.reset();
        m_answerer.reset();
    }

    // What each end got to, for a failure message: readiness, gathering,
    // the relay servers each was given and the candidates each produced
    // (addresses are loopback, nothing secret).
    QString describe() const
    {
        return QStringLiteral("clientReady=%1 stationReady=%2 clientGathered=%3 stationGathered=%4 "
                              "clientRelays=%5 stationRelays=%6 stationHeard=%7\n"
                              "client candidates: %8\nstation candidates: %9")
            .arg(clientReady)
            .arg(stationReady)
            .arg(clientGathered)
            .arg(stationGathered)
            .arg(clientRelays)
            .arg(stationRelays)
            .arg(stationHeard)
            .arg(clientCandidates.join(QStringLiteral(" | ")),
                 stationCandidates.join(QStringLiteral(" | ")));
    }

    bool clientReady = false;
    bool stationReady = false;
    bool clientGathered = false;
    bool stationGathered = false;
    int stationHeard = 0;
    qsizetype stationRelays = -1;
    qsizetype clientRelays = -1;
    QStringList clientCandidates;
    QStringList stationCandidates;
    QList<QByteArray> clientReceived;
    QList<QByteArray> stationReceived;

private:
    bool passes(const QString& candidate) const
    {
        return !m_relayOnly || IceConfiguration::candidateType(candidate) == QLatin1String("relay");
    }

    void onIntroduced(const RendezvousIntroduction& introduction)
    {
        const QByteArray id = introduction.id;
        m_stationIce = IceConfiguration::throughRendezvous(
            m_station->stunUrls(), m_station->relayAllowed(), AddressFamilies{}, HostFamilies{});
        m_answerer = std::make_unique<LibDataChannelMediaTransport>();
        QObject::connect(m_answerer.get(), &IMediaTransport::localDescription, this,
                         [this, id](const QString& sdp, const QString&) {
            QVERIFY(m_station->answer(id, sdp));
            // With the relay denied no credentials follow: gather now.
            if (!m_station->relayAllowed()) {
                m_answerer->gatherCandidates({});
            }
        });
        QObject::connect(m_answerer.get(), &IMediaTransport::localCandidate, this,
                         [this, id](const QString& candidate, const QString&) {
            stationCandidates.append(candidate);
            m_station->sendCandidate(id, candidate);
        });
        QObject::connect(m_answerer.get(), &IMediaTransport::gatheringComplete, this, [this, id] {
            stationGathered = true;
            m_station->sendCandidate(id, QString());
        });
        QObject::connect(m_answerer.get(), &IMediaTransport::ready, this,
                         [this] { stationReady = true; });
        QObject::connect(m_answerer.get(), &IMediaTransport::displayReceived, this,
                         [this](const QByteArray& message) { stationReceived.append(message); });
        IMediaTransport::StartOptions options{IMediaTransport::Role::Answerer, 0x2222};
        options.ice = m_stationIce;
        QVERIFY(m_answerer->start(options));
        QVERIFY(m_answerer->acceptDescription(introduction.offer, QStringLiteral("offer")));
    }

    RendezvousClient* m_station;
    RendezvousClient* m_client;
    bool m_relayOnly;
    std::shared_ptr<TestKey> m_device;
    std::optional<IceConfiguration> m_stationIce;
    std::optional<IceConfiguration> m_clientIce;
    std::unique_ptr<LibDataChannelMediaTransport> m_offerer;
    std::unique_ptr<LibDataChannelMediaTransport> m_answerer;
};

// Waits for both ends of `pair` to be ready, for `boundMs` at most, and
// stops at once if the fake relay exits. On failure `why` has the reason,
// what each end got to, the relay's own account and the service's log.
bool waitForPair(IcePair& pair, LocalService& service, int boundMs, QString* why)
{
    EndWatch ends;
    ends.watch(service);
    QString reason;
    if (waitUntil([&pair] { return pair.clientReady && pair.stationReady; }, boundMs, ends,
                  QStringLiteral("both ends of the connection ready"), &reason)) {
        return true;
    }
    *why = reason + QLatin1Char('\n') + pair.describe() + QStringLiteral("\n--- relay:\n")
        + service.turnReport() + QStringLiteral("\n--- service:\n") + service.log();
    return false;
}

// Waits until the fake relay's output has `line`, for `boundMs` at most,
// stopping at once if the relay exits; on failure `why` has its account.
bool waitForRelayLine(LocalService& service, const QString& line, int boundMs, QString* why)
{
    EndWatch ends;
    ends.watch(service);
    QString reason;
    if (waitUntil([&service, &line] { return service.turnOutput().contains(line); }, boundMs,
                  ends, QStringLiteral("\"%1\" from the relay").arg(line), &reason)) {
        return true;
    }
    *why = reason + QLatin1Char('\n') + service.turnReport();
    return false;
}

// A STUN/TURN client over UDP, just enough of RFC 8489 and RFC 8656 to
// drive the fake relay by hand: requests with the long-term credentials
// the fake takes (coturn's time-limited ones), and a Send indication.
class TurnProbe {
public:
    static constexpr quint16 kAllocate = 0x003;
    static constexpr quint16 kRefresh = 0x004;
    static constexpr quint16 kSend = 0x006;
    static constexpr quint16 kCreatePermission = 0x008;
    static constexpr quint16 kRequest = 0x000;
    static constexpr quint16 kIndication = 0x010;
    static constexpr quint16 kSuccess = 0x100;
    static constexpr quint16 kError = 0x110;

    struct Attribute {
        quint16 type;
        QByteArray value;
    };
    struct Response {
        quint16 method = 0;
        quint16 cls = 0;
        QList<Attribute> attributes;
        QByteArray value(quint16 type) const
        {
            for (const Attribute& attribute : attributes) {
                if (attribute.type == type) {
                    return attribute.value;
                }
            }
            return {};
        }
    };

    TurnProbe(quint16 port, QByteArray secret) : m_port(port), m_secret(std::move(secret))
    {
        m_socket.bind(QHostAddress(QHostAddress::LocalHost), 0);
    }

    // Allocate without credentials, take the realm and nonce from the 401,
    // then allocate with them.
    bool allocate()
    {
        const auto challenge = request(kAllocate, {{0x0019, transport()}}, false);
        if (!challenge || challenge->cls != kError) {
            return false;
        }
        m_realm = challenge->value(0x0014);
        m_nonce = challenge->value(0x0015);
        m_username = QByteArrayLiteral("4102444800:relay-probe");
        const QByteArray password = QMessageAuthenticationCode::hash(
            m_username, m_secret, QCryptographicHash::Sha1).toBase64();
        m_key = QCryptographicHash::hash(m_username + ':' + m_realm + ':' + password,
                                         QCryptographicHash::Md5);
        const auto allocated = request(kAllocate, {{0x0019, transport()}}, true);
        return allocated && allocated->cls == kSuccess;
    }

    bool permit(const QHostAddress& peer, quint16 port)
    {
        const auto permitted = request(kCreatePermission, {{0x0012, xorAddress(peer, port)}}, true);
        return permitted && permitted->cls == kSuccess;
    }

    void sendTo(const QHostAddress& peer, quint16 port, const QByteArray& data)
    {
        m_socket.writeDatagram(message(kSend, kIndication, {{0x0012, xorAddress(peer, port)},
                                                            {0x0013, data}}, false),
                               QHostAddress(QHostAddress::LocalHost), m_port);
    }

    // A Refresh of LIFETIME 0: the allocation given back.
    bool release()
    {
        QByteArray zero(4, '\0');
        const auto released = request(kRefresh, {{0x000D, zero}}, true);
        return released && released->cls == kSuccess;
    }

private:
    static QByteArray transport()
    {
        return QByteArray("\x11\x00\x00\x00", 4);  // UDP
    }

    QByteArray xorAddress(const QHostAddress& peer, quint16 port) const
    {
        QByteArray value(8, '\0');
        value[1] = 0x01;
        qToBigEndian<quint16>(port ^ quint16(kMagic >> 16), value.data() + 2);
        qToBigEndian<quint32>(peer.toIPv4Address() ^ kMagic, value.data() + 4);
        return value;
    }

    static void append(QByteArray& body, quint16 type, const QByteArray& value)
    {
        char header[4];
        qToBigEndian<quint16>(type, header);
        qToBigEndian<quint16>(quint16(value.size()), header + 2);
        body.append(header, 4);
        body.append(value);
        body.append(QByteArray((4 - value.size() % 4) % 4, '\0'));
    }

    QByteArray message(quint16 method, quint16 cls, const QList<Attribute>& attributes,
                       bool signed_)
    {
        m_transaction = randomBytes(12);
        QByteArray body;
        for (const Attribute& attribute : attributes) {
            append(body, attribute.type, attribute.value);
        }
        if (signed_) {
            append(body, 0x0006, m_username);
            append(body, 0x0014, m_realm);
            append(body, 0x0015, m_nonce);
        }
        const quint16 type = quint16(((method & 0x0F80) << 2) | ((method & 0x0070) << 1)
                                     | (method & 0x000F) | cls);
        const auto header = [&](int length) {
            QByteArray bytes(8, '\0');
            qToBigEndian<quint16>(type, bytes.data());
            qToBigEndian<quint16>(quint16(length), bytes.data() + 2);
            qToBigEndian<quint32>(kMagic, bytes.data() + 4);
            return bytes + m_transaction;
        };
        if (signed_) {
            const QByteArray mac = QMessageAuthenticationCode::hash(
                header(body.size() + 24) + body, m_key, QCryptographicHash::Sha1);
            append(body, 0x0008, mac);
        }
        return header(body.size()) + body;
    }

    std::optional<Response> request(quint16 method, const QList<Attribute>& attributes,
                                    bool signed_)
    {
        m_socket.writeDatagram(message(method, kRequest, attributes, signed_),
                               QHostAddress(QHostAddress::LocalHost), m_port);
        const QDeadlineTimer deadline(kReplyBoundMs);
        while (!deadline.hasExpired()) {
            if (!m_socket.hasPendingDatagrams()
                && !m_socket.waitForReadyRead(int(qMax<qint64>(1, deadline.remainingTime())))) {
                continue;
            }
            const QByteArray data = m_socket.receiveDatagram().data();
            if (data.size() < 20 || data.mid(8, 12) != m_transaction) {
                continue;
            }
            Response response;
            const quint16 type = qFromBigEndian<quint16>(data.constData());
            response.cls = type & 0x0110;
            response.method = quint16((type & 0x000F) | ((type & 0x00E0) >> 1)
                                      | ((type & 0x3E00) >> 2));
            int offset = 20;
            while (offset + 4 <= data.size()) {
                const quint16 attributeType = qFromBigEndian<quint16>(data.constData() + offset);
                const quint16 length = qFromBigEndian<quint16>(data.constData() + offset + 2);
                response.attributes.append({attributeType, data.mid(offset + 4, length)});
                offset += 4 + length + (4 - length % 4) % 4;
            }
            return response;
        }
        return std::nullopt;
    }

    static constexpr quint32 kMagic = 0x2112A442;
    // How long one request waits for the fake relay on this computer.
    static constexpr int kReplyBoundMs = 5000;
    QUdpSocket m_socket;
    quint16 m_port;
    QByteArray m_secret;
    QByteArray m_realm;
    QByteArray m_nonce;
    QByteArray m_username;
    QByteArray m_key;
    QByteArray m_transaction;
};

// Task 28 tail (R-IOS-16): whether this computer has IPv6 the product would
// use, by the product's own rule (IceConfiguration::localAddressFamilies():
// global unicast, 2000::/3). A unique local address (fc00::/7, as ZeroTier
// or a VPN puts on a utun interface) or a link-local one reaches no server,
// so the product never chooses IPv6 for it, and neither may this test's
// precondition: counting one made the test run, and fail, on a computer
// whose only IPv6 was a unique local address.
bool hasUsableIpv6()
{
    return IceConfiguration::localAddressFamilies().ipv6;
}

// A certificate for 127.0.0.1 (in its subjectAltName) and its key, made at
// run time and written where a Core's CertificateStore loads them, so a
// test can add the certificate to the trust store and have a handshake to
// that Core raise no TLS error at all. RSA, as CertificateStore uses.
// Task 29 step 2b: `altName` another name, for a certificate a sign-in
// page would present.
QSslCertificate writeLoopbackCertificate(const QString& directory,
                                         const char* altName = "IP:127.0.0.1")
{
    using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using CertPtr = std::unique_ptr<X509, decltype(&X509_free)>;
    using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
    KeyPtr key(EVP_RSA_gen(2048), &EVP_PKEY_free);
    CertPtr cert(X509_new(), &X509_free);
    if (!key || !cert || X509_set_version(cert.get(), 2) != 1
        || ASN1_INTEGER_set_int64(X509_get_serialNumber(cert.get()),
                                  QRandomGenerator::system()->bounded(1, 1 << 30))
               != 1
        || !X509_gmtime_adj(X509_getm_notBefore(cert.get()), -60)
        || !X509_gmtime_adj(X509_getm_notAfter(cert.get()), 24 * 60 * 60)
        || X509_set_pubkey(cert.get(), key.get()) != 1) {
        return QSslCertificate();
    }
    // Keep the subject consistent with the SAN. On some TLS backends a
    // loopback CN masks a deliberately mismatched DNS SAN.
    const char* commonName = QByteArrayView(altName).startsWith("DNS:")
        ? altName + 4 : "127.0.0.1";
    X509_NAME* name = X509_get_subject_name(cert.get());
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>(commonName), -1, -1, 0)
            != 1
        || X509_set_issuer_name(cert.get(), name) != 1) {
        return QSslCertificate();
    }
    X509V3_CTX context;
    X509V3_set_ctx(&context, cert.get(), cert.get(), nullptr, nullptr, 0);
    const std::pair<int, const char*> extensions[] = {
        {NID_basic_constraints, "critical,CA:TRUE"},
        {NID_key_usage, "critical,digitalSignature,keyEncipherment,keyCertSign"},
        {NID_ext_key_usage, "serverAuth"},
        {NID_subject_alt_name, altName},
    };
    for (const auto& [nid, value] : extensions) {
        X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value);
        const bool added = extension && X509_add_ext(cert.get(), extension, -1) == 1;
        X509_EXTENSION_free(extension);
        if (!added) {
            return QSslCertificate();
        }
    }
    if (X509_sign(cert.get(), key.get(), EVP_sha256()) == 0) {
        return QSslCertificate();
    }
    BioPtr certBio(BIO_new(BIO_s_mem()), &BIO_free);
    BioPtr keyBio(BIO_new(BIO_s_mem()), &BIO_free);
    if (!certBio || !keyBio || PEM_write_bio_X509(certBio.get(), cert.get()) != 1
        || PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr,
                                    nullptr)
               != 1) {
        return QSslCertificate();
    }
    const auto contents = [](BIO* bio) {
        char* data = nullptr;
        const long length = BIO_get_mem_data(bio, &data);
        return QByteArray(data, static_cast<qsizetype>(length));
    };
    const QByteArray certPem = contents(certBio.get());
    const std::pair<QString, QByteArray> files[] = {
        {QStringLiteral("tls-cert.pem"), certPem},
        {QStringLiteral("tls-key.pem"), contents(keyBio.get())},
    };
    for (const auto& [file, pem] : files) {
        QFile out(QDir(directory).filePath(file));
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(pem) != pem.size()
            || !out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            return QSslCertificate();
        }
    }
    return QSslCertificate(certPem, QSsl::Pem);
}

// Adds a certificate authority to the trust store for as long as it lives.
class TrustedAuthority {
public:
    explicit TrustedAuthority(const QSslCertificate& authority)
        : m_saved(QSslConfiguration::defaultConfiguration())
    {
        QSslConfiguration configuration = m_saved;
        configuration.addCaCertificate(authority);
        QSslConfiguration::setDefaultConfiguration(configuration);
    }
    ~TrustedAuthority() { QSslConfiguration::setDefaultConfiguration(m_saved); }
    TrustedAuthority(const TrustedAuthority&) = delete;
    TrustedAuthority& operator=(const TrustedAuthority&) = delete;

private:
    QSslConfiguration m_saved;
};

} // namespace

// Plays the service's side of a Core's registration on `socket`: hello,
// challenge, registered.
void registerOnPlayer(ServicePlayer& player, QWebSocket* socket, const QString& id)
{
    socket->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
                                                {"nonce", b64(randomBytes(32))},
                                                {"stun", QJsonArray()}}));
    QVERIFY(player.waitForMessage(socket).has_value());  // register
    socket->sendTextMessage(
        compact(QJsonObject{{"type", "challenge"}, {"nonce", b64(randomBytes(32))}}));
    QVERIFY(player.waitForMessage(socket).has_value());  // prove
    socket->sendTextMessage(compact(QJsonObject{{"type", "registered"}, {"id", id}}));
}

class TstRendezvousClient : public QObject {
    Q_OBJECT

private:
    void runCoreFixture(const QString& file);
    void runAppFixture(const QString& file);

private slots:
    void initTestCase()
    {
        // NEREUS_ICE_DIAG=1 logs this run's ICE checks, as it does for the
        // Core; installed before any peer is made.
        IceDiagnostics::installFromEnvironment();
        qRegisterMetaType<RendezvousIntroduction>();
        qRegisterMetaType<Wire::Turn>();
        QVERIFY2(QFile::exists(kSuite + QStringLiteral("/manifest.json")), qPrintable(kSuite));
    }

    void watchRelayNegotiation_data()
    {
        QTest::addColumn<bool>("stationRole");
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<int>("serviceVersion");
        QTest::addColumn<int>("watchVersion");
        QTest::addColumn<bool>("expected");
        for (bool station : {false, true}) {
            const QByteArray prefix = station ? "station-" : "client-";
            QTest::newRow((prefix + "default-off").constData()) << station << false << 2 << 1 << false;
            QTest::newRow((prefix + "old-service").constData()) << station << true << 1 << 0 << false;
            QTest::newRow((prefix + "wrong-envelope").constData()) << station << true << 1 << 1 << false;
            QTest::newRow((prefix + "future-watch").constData()) << station << true << 2 << 2 << false;
            QTest::newRow((prefix + "negotiated").constData()) << station << true << 2 << 1 << true;
        }
    }

    void watchRelayNegotiation()
    {
        QFETCH(bool, stationRole);
        QFETCH(bool, enabled);
        QFETCH(int, serviceVersion);
        QFETCH(int, watchVersion);
        QFETCH(bool, expected);
        ServicePlayer player;
        auto key = makeKey();
        RendezvousClient endpoint;
        endpoint.setServers({player.url()});
        endpoint.setWatchRelayEnabled(enabled);
        if (stationRole) {
            endpoint.registerStation(key->spki(),
                [key](const QByteArray& value) { return key->sign(value); },
                [](const QByteArray&) { return QByteArray(); });
        } else {
            endpoint.introduce(Wire::rendezvousId(makeKey()->spki()), key->spki(),
                [key](const QByteArray& value) { return key->sign(value); }, "offer");
        }
        QWebSocket* service = player.waitForConnection();
        QVERIFY(service);
        QJsonObject hello{{"type", "hello"}, {"version", serviceVersion},
                          {"nonce", b64(randomBytes(32))}, {"stun", QJsonArray()}};
        if (watchVersion) { hello.insert("watchRelayVersion", watchVersion); }
        service->sendTextMessage(compact(hello));
        const auto request = player.waitForMessage(service);
        QVERIFY(request.has_value());
        const auto sent = QJsonDocument::fromJson(request->toUtf8()).object();
        QCOMPARE(sent.contains("watchRelayVersion"), expected);
        if (expected) { QCOMPARE(sent["watchRelayVersion"].toInt(), 1); }
        if (!stationRole) {
            QSignalSpy granted(&endpoint, &RendezvousClient::relayGrantReceived);
            service->sendTextMessage(compact(QJsonObject{
                {"type", "relay.grant"}, {"url", "wss://example.invalid/relay"},
                {"token", "primary"}, {"watchToken", "watch"}, {"expires", 1800000120}}));
            QTRY_COMPARE(granted.size(), 1);
            QVERIFY(endpoint.relayGrant());
            QCOMPARE(endpoint.relayGrant()->token, QStringLiteral("primary"));
            QCOMPARE(endpoint.relayGrant()->watchToken, expected ? QStringLiteral("watch") : QString());
        }
        endpoint.stop();
        QVERIFY(!endpoint.relayGrant());
        // A subsequent old-service connection must not inherit negotiation.
        if (stationRole) {
            endpoint.registerStation(key->spki(),
                [key](const QByteArray& value) { return key->sign(value); },
                [](const QByteArray&) { return QByteArray(); });
        } else {
            endpoint.connectToService();
        }
        QWebSocket* next = player.waitForConnection();
        QVERIFY(next);
        next->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
            {"nonce", b64(randomBytes(32))}, {"stun", QJsonArray()}}));
        if (!stationRole) {
            QTRY_VERIFY(endpoint.isConnected());
            endpoint.introduce(Wire::rendezvousId(makeKey()->spki()), key->spki(),
                [key](const QByteArray& value) { return key->sign(value); }, "offer");
        }
        const auto nextRequest = player.waitForMessage(next);
        QVERIFY(nextRequest.has_value());
        QVERIFY(!QJsonDocument::fromJson(nextRequest->toUtf8()).object().contains("watchRelayVersion"));
        endpoint.stop();
    }

    void watchGrantOptionalWireFields()
    {
        const auto roundTrip = [](Wire::Direction direction, const QJsonObject& input) {
            Wire::Message decoded;
            if (!Wire::decode(direction, compact(input).toUtf8(), &decoded)) {
                return QJsonObject{};
            }
            return QJsonDocument::fromJson(Wire::encode(direction, decoded)).object();
        };
        const auto serviceClient = Wire::Direction::ServiceToClient;
        QJsonObject hello{{"type", "hello"}, {"version", 2},
                          {"nonce", b64(randomBytes(32))}, {"stun", QJsonArray()},
                          {"watchRelayVersion", 1}};
        QCOMPARE(roundTrip(serviceClient, hello), hello);
        hello.remove("watchRelayVersion");
        QCOMPARE(roundTrip(serviceClient, hello), hello);
        const auto key = makeKey();
        QJsonObject registration{{"type", "register"},
                                 {"id", Wire::rendezvousId(key->spki())},
                                 {"publicKey", b64(key->spki())}, {"watchRelayVersion", 1}};
        QCOMPARE(roundTrip(Wire::Direction::StationToService, registration), registration);
        QJsonObject introduction{{"type", "introduce"}, {"id", registration["id"]},
                                 {"device", b64(randomBytes(32))},
                                 {"deviceSignature", b64(randomBytes(64))},
                                 {"offer", "offer"}, {"watchRelayVersion", 1}};
        QCOMPARE(roundTrip(Wire::Direction::ClientToService, introduction), introduction);
        QJsonObject grant{{"type", "relay.grant"}, {"url", "wss://example.invalid/relay"},
                          {"token", "primary"}, {"expires", 1800000120},
                          {"watchToken", "watch"}};
        QCOMPARE(roundTrip(serviceClient, grant), grant);
        grant.insert("from", b64(randomBytes(16)));
        QCOMPARE(roundTrip(Wire::Direction::ServiceToStation, grant), grant);
        for (const QJsonValue& bad : {QJsonValue(true), QJsonValue(0), QJsonValue(65536),
                                     QJsonValue(1.5), QJsonValue("1"), QJsonValue()}) {
            hello.insert("watchRelayVersion", bad);
            Wire::Message decoded;
            QVERIFY(!Wire::decode(serviceClient, compact(hello).toUtf8(), &decoded));
        }
        grant.remove("from");
        for (const QJsonValue& bad : {QJsonValue(true), QJsonValue(""),
                                     QJsonValue("not+base64"), QJsonValue()}) {
            grant.insert("watchToken", bad);
            Wire::Message decoded;
            QVERIFY(!Wire::decode(serviceClient, compact(grant).toUtf8(), &decoded));
        }
    }

    // ── Crypto vectors (section 10.2) ───────────────────────────────────

    void base64urlVectors()
    {
        const QJsonArray cases =
            readJson(kSuite + QStringLiteral("/crypto/base64url.json")).value(QStringLiteral("cases")).toArray();
        QVERIFY(!cases.isEmpty());
        for (const QJsonValue& value : cases) {
            const QJsonObject c = value.toObject();
            bool ok = false;
            const QByteArray bytes =
                StationIdentity::fromBase64Url(c.value(QStringLiteral("text")).toString(), &ok);
            QCOMPARE(ok, c.value(QStringLiteral("valid")).toBool());
            if (ok) {
                QCOMPARE(bytes.toHex(), c.value(QStringLiteral("bytesHex")).toString().toLatin1());
            }
        }
    }

    void p256KeyVectors()
    {
        const QJsonArray cases =
            readJson(kSuite + QStringLiteral("/crypto/p256-spki.json")).value(QStringLiteral("cases")).toArray();
        QVERIFY(!cases.isEmpty());
        for (const QJsonValue& value : cases) {
            const QJsonObject c = value.toObject();
            const QByteArray spki =
                QByteArray::fromHex(c.value(QStringLiteral("spkiHex")).toString().toLatin1());
            QVERIFY2(StationIdentity::isP256Spki(spki) == c.value(QStringLiteral("valid")).toBool(),
                     qPrintable(c.value(QStringLiteral("name")).toString()));
        }
    }

    void rendezvousIdVectors()
    {
        const QJsonObject file = readJson(kSuite + QStringLiteral("/crypto/rendezvous-id.json"));
        QCOMPARE(QByteArray("NereusSDR rendezvous id v1\n").toHex(),
                 file.value(QStringLiteral("prefixHex")).toString().toLatin1());
        const QJsonArray cases = file.value(QStringLiteral("cases")).toArray();
        QVERIFY(!cases.isEmpty());
        for (const QJsonValue& value : cases) {
            const QJsonObject c = value.toObject();
            const QByteArray spki = unb64(c.value(QStringLiteral("publicKey")).toString());
            QCOMPARE(QCryptographicHash::hash(QByteArray("NereusSDR rendezvous id v1\n") + spki,
                                              QCryptographicHash::Sha256)
                         .toHex(),
                     c.value(QStringLiteral("digestHex")).toString().toLatin1());
            const QString id = Wire::rendezvousId(spki);
            QCOMPARE(id, c.value(QStringLiteral("id")).toString());
            QVERIFY(Wire::isRendezvousId(id));
        }
        QVERIFY(!Wire::isRendezvousId(QStringLiteral("LWHU2KYJNRFDWVPXVDCKAO3LB7")));
        QVERIFY(!Wire::isRendezvousId(QStringLiteral("lwhu2kyjnrfdwvpxvdckao3lb")));
        QVERIFY(Wire::rendezvousId(QByteArray(90, 'x')).isEmpty());
    }

    void registerProofVectors()
    {
        const QJsonObject file = readJson(kSuite + QStringLiteral("/crypto/register-proof.json"));
        const QJsonObject key = file.value(QStringLiteral("key")).toObject();
        const QByteArray spki = unb64(key.value(QStringLiteral("publicKey")).toString());
        QCOMPARE(Wire::rendezvousId(spki), key.value(QStringLiteral("id")).toString());
        const QByteArray transcript =
            Wire::registerTranscript(unb64(file.value(QStringLiteral("nonce")).toString()));
        QCOMPARE(transcript.size(), 65);
        QCOMPARE(transcript.toHex(), file.value(QStringLiteral("transcriptHex")).toString().toLatin1());
        const QJsonArray cases = file.value(QStringLiteral("cases")).toArray();
        QVERIFY(!cases.isEmpty());
        for (const QJsonValue& value : cases) {
            const QJsonObject c = value.toObject();
            bool keyOk = false;
            bool signatureOk = false;
            const QByteArray caseKey =
                StationIdentity::fromBase64Url(c.value(QStringLiteral("publicKey")).toString(), &keyOk);
            const QByteArray signature =
                StationIdentity::fromBase64Url(c.value(QStringLiteral("signature")).toString(), &signatureOk);
            const bool verified = keyOk && signatureOk && StationIdentity::isP256Spki(caseKey)
                                  && StationIdentity::verify(caseKey, transcript, signature);
            QVERIFY2(verified == c.value(QStringLiteral("valid")).toBool(),
                     qPrintable(c.value(QStringLiteral("name")).toString()));
        }
    }

    void introduceSignatureVectors()
    {
        const QJsonObject file = readJson(kSuite + QStringLiteral("/crypto/introduce-signature.json"));
        const QJsonObject device = file.value(QStringLiteral("device")).toObject();
        const QByteArray spki = unb64(device.value(QStringLiteral("publicKey")).toString());
        QCOMPARE(b64(StationIdentity::fingerprintOf(spki)), device.value(QStringLiteral("id")).toString());
        const QByteArray transcript =
            Wire::introduceTranscript(file.value(QStringLiteral("stationId")).toString(),
                                      unb64(file.value(QStringLiteral("nonce")).toString()));
        QCOMPARE(transcript.size(), 81);
        QCOMPARE(transcript.toHex(), file.value(QStringLiteral("transcriptHex")).toString().toLatin1());
        const QJsonArray cases = file.value(QStringLiteral("cases")).toArray();
        QVERIFY(!cases.isEmpty());
        for (const QJsonValue& value : cases) {
            const QJsonObject c = value.toObject();
            bool ok = false;
            const QByteArray signature =
                StationIdentity::fromBase64Url(c.value(QStringLiteral("signature")).toString(), &ok);
            const bool verified = ok && StationIdentity::verify(spki, transcript, signature);
            QVERIFY2(verified == c.value(QStringLiteral("valid")).toBool(),
                     qPrintable(c.value(QStringLiteral("name")).toString()));
        }
    }

    void turnCredentialVectors()
    {
        const QJsonArray cases =
            readJson(kSuite + QStringLiteral("/crypto/turn-credentials.json")).value(QStringLiteral("cases")).toArray();
        QVERIFY(!cases.isEmpty());
        for (const QJsonValue& value : cases) {
            const QJsonObject c = value.toObject();
            const QString username = QStringLiteral("%1:%2")
                                         .arg(c.value(QStringLiteral("expires")).toInteger())
                                         .arg(c.value(QStringLiteral("stationId")).toString());
            QCOMPARE(username, c.value(QStringLiteral("username")).toString());
            QCOMPARE(turnPassword(c.value(QStringLiteral("secret")).toString().toUtf8(), username),
                     c.value(QStringLiteral("password")).toString());
        }
    }

    // ── Control fixtures (section 10.3) ─────────────────────────────────

    void controlFixtures()
    {
        QDir dir(kSuite + QStringLiteral("/control"));
        const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        QVERIFY(files.size() > 40);
        int station = 0;
        int client = 0;
        for (const QString& name : files) {
            const QJsonObject fixture = readJson(dir.filePath(name));
            const QString from = fixture.value(QStringLiteral("from")).toString();
            const QString to = fixture.value(QStringLiteral("to")).toString();
            Wire::Direction direction;
            if (from == QLatin1String("station")) {
                direction = Wire::Direction::StationToService;
                ++station;
            } else if (from == QLatin1String("client")) {
                direction = Wire::Direction::ClientToService;
                ++client;
            } else if (to == QLatin1String("station")) {
                direction = Wire::Direction::ServiceToStation;
                ++station;
            } else {
                direction = Wire::Direction::ServiceToClient;
                ++client;
            }
            const QJsonObject wire = fixture.value(QStringLiteral("wire")).toObject();
            const QByteArray text = QJsonDocument(wire).toJson(QJsonDocument::Compact);
            Wire::Message message;
            QString why;
            const bool decoded = Wire::decode(direction, text, &message, &why);
            QVERIFY2(decoded == fixture.value(QStringLiteral("decodes")).toBool(),
                     qPrintable(name + QStringLiteral(": ") + why));
            if (!decoded) {
                continue;
            }
            const QByteArray again = Wire::encode(direction, message);
            QVERIFY2(!again.isEmpty(), qPrintable(name));
            const QStringList keys = listedKeys(direction, message.kind);
            const QJsonObject ours = QJsonDocument::fromJson(again).object();
            QStringList ourKeys = ours.keys();
            QStringList wantKeys = keys;
            ourKeys.sort();
            wantKeys.sort();
            QVERIFY2(ourKeys == wantKeys, qPrintable(name));
            QVERIFY2(restricted(ours, keys) == restricted(wire, keys), qPrintable(name));
        }
        QVERIFY(station > 0);
        QVERIFY(client > 0);
    }

    // A service that replays an accepted introduction under new ids gets
    // at most kMaxLiveIntroductions of them reported; the rest are dropped
    // and counted.
    void theCoreHoldsABoundedNumberOfIntroductions()
    {
        ServicePlayer player;
        auto coreKey = makeKey();
        auto device = makeKey();
        const QByteArray deviceId = StationIdentity::fingerprintOf(device->spki());
        RendezvousClient core;
        core.setServers({player.url()});
        QSignalSpy registered(&core, &RendezvousClient::registered);
        QSignalSpy introduced(&core, &RendezvousClient::introduced);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [device, deviceId](const QByteArray& id) {
                                 return id == deviceId ? device->spki() : QByteArray();
                             });
        QWebSocket* station = player.waitForConnection();
        QVERIFY(station != nullptr);
        const auto send = [station](const QJsonObject& message) {
            station->sendTextMessage(compact(message));
        };
        send({{"type", "hello"}, {"version", 1}, {"nonce", b64(randomBytes(32))},
              {"stun", QJsonArray()}});
        QVERIFY(player.waitForMessage(station).has_value());
        const QByteArray challenge = randomBytes(32);
        send({{"type", "challenge"}, {"nonce", b64(challenge)}});
        QVERIFY(player.waitForMessage(station).has_value());
        const QString id = Wire::rendezvousId(coreKey->spki());
        send({{"type", "registered"}, {"id", id}});
        QTRY_COMPARE(registered.size(), 1);
        const QByteArray nonce = randomBytes(32);
        const QString signature = b64(device->sign(Wire::introduceTranscript(id, nonce)));
        const QString offer = readText(kSuite + QStringLiteral("/sdp/offer.sdp"));
        const int sent = RendezvousClient::kMaxLiveIntroductions + 4;
        for (int index = 0; index < sent; ++index) {
            send({{"type", "introduction"}, {"from", b64(randomBytes(16))},
                  {"device", b64(deviceId)}, {"deviceSignature", signature}, {"offer", offer},
                  {"nonce", b64(nonce)}});
        }
        QTRY_COMPARE(introduced.size() + static_cast<qsizetype>(core.droppedIntroductions()),
                     static_cast<qsizetype>(sent));
        QCOMPARE(introduced.size(), static_cast<qsizetype>(RendezvousClient::kMaxLiveIntroductions));
        QCOMPARE(core.droppedIntroductions(), quint64(4));
        core.stop();
    }

    // Task 29 fix wave (rendezvous section 12.1): a relay grant for an
    // introduction this Core answered is kept for the relay leg; one for an
    // introduction it never answered is not, and it goes when the
    // introduction ends.
    void theCoreKeepsTheRelayGrantOfAnAnsweredIntroduction()
    {
        ServicePlayer player;
        auto coreKey = makeKey();
        auto device = makeKey();
        const QByteArray deviceId = StationIdentity::fingerprintOf(device->spki());
        RendezvousClient core;
        core.setServers({player.url()});
        QSignalSpy registered(&core, &RendezvousClient::registered);
        QSignalSpy introduced(&core, &RendezvousClient::introduced);
        QSignalSpy granted(&core, &RendezvousClient::relayGrantReceived);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [device, deviceId](const QByteArray& id) {
                                 return id == deviceId ? device->spki() : QByteArray();
                             });
        QWebSocket* station = player.waitForConnection();
        QVERIFY(station != nullptr);
        const auto send = [station](const QJsonObject& message) {
            station->sendTextMessage(compact(message));
        };
        send({{"type", "hello"}, {"version", 1}, {"nonce", b64(randomBytes(32))},
              {"stun", QJsonArray()}});
        QVERIFY(player.waitForMessage(station).has_value());
        send({{"type", "challenge"}, {"nonce", b64(randomBytes(32))}});
        QVERIFY(player.waitForMessage(station).has_value());
        const QString id = Wire::rendezvousId(coreKey->spki());
        send({{"type", "registered"}, {"id", id}});
        QTRY_COMPARE(registered.size(), 1);
        const QByteArray nonce = randomBytes(32);
        const QString signature = b64(device->sign(Wire::introduceTranscript(id, nonce)));
        const QString offer = readText(kSuite + QStringLiteral("/sdp/offer.sdp"));
        const QByteArray answered = randomBytes(16);
        const QByteArray unanswered = randomBytes(16);
        for (const QByteArray& intro : {answered, unanswered}) {
            send({{"type", "introduction"}, {"from", b64(intro)}, {"device", b64(deviceId)},
                  {"deviceSignature", signature}, {"offer", offer}, {"nonce", b64(nonce)}});
        }
        QTRY_COMPARE(introduced.size(), 2);
        QVERIFY(core.answer(answered, readText(kSuite + QStringLiteral("/sdp/answer.sdp"))));
        QVERIFY(player.waitForMessage(station).has_value());
        const QString token = QStringLiteral(
            "AQEAAQIDBAUGBwgJCgsMDQ4P8pvPCvK38JprSdJ4pqn2imoBltp4JM7Y4jlm8z_CRueEVquUo2unUc8QLhE");
        for (const QByteArray& intro : {unanswered, answered}) {
            send({{"type", "relay.grant"}, {"from", b64(intro)},
                  {"url", "wss://rv.conformance.invalid/v1/relay"}, {"token", token},
                  {"expires", 1800000120}});
        }
        QTRY_COMPARE(granted.size(), 1);
        QCOMPARE(granted.at(0).at(0).toByteArray(), answered);
        QVERIFY(!core.relayGrant(unanswered).has_value());
        const std::optional<Wire::RelayGrant> grant = core.relayGrant(answered);
        QVERIFY(grant.has_value());
        QCOMPARE(grant->url, QStringLiteral("wss://rv.conformance.invalid/v1/relay"));
        QCOMPARE(grant->token, token);
        QCOMPARE(grant->expires, qint64(1800000120));
        send({{"type", "introduction.end"}, {"from", b64(answered)}, {"code", "clientLeft"}});
        QTRY_VERIFY(!core.relayGrant(answered).has_value());
        core.stop();
    }

    // Task 29 step 2a re-review: a client keeps its introduction's relay
    // grant (the relay leg reads it) and lets it go when the introduction
    // ends.
    void theClientKeepsItsRelayGrantWhileItsIntroductionLives()
    {
        ServicePlayer player;
        auto phone = makeKey();
        RendezvousClient client;
        client.setServers({player.url()});
        QSignalSpy granted(&client, &RendezvousClient::relayGrantReceived);
        QSignalSpy ended(&client, &RendezvousClient::introductionEnded);
        client.introduce(Wire::rendezvousId(makeKey()->spki()), phone->spki(),
                         [phone](const QByteArray& message) { return phone->sign(message); },
                         readText(kSuite + QStringLiteral("/sdp/offer.sdp")));
        QWebSocket* service = player.waitForConnection();
        QVERIFY(service != nullptr);
        const auto send = [service](const QJsonObject& message) {
            service->sendTextMessage(compact(message));
        };
        send({{"type", "hello"}, {"version", 1}, {"nonce", b64(randomBytes(32))},
              {"stun", QJsonArray()}});
        const std::optional<QString> introduce = player.waitForMessage(service);
        QVERIFY(introduce.has_value() && introduce->contains(QLatin1String("\"introduce\"")));
        QVERIFY(!client.relayGrant().has_value());
        send({{"type", "answer"}, {"answer", readText(kSuite + QStringLiteral("/sdp/answer.sdp"))},
              {"turn", QJsonValue()}});
        const QString token = QStringLiteral(
            "AQIAAQIDBAUGBwgJCgsMDQ4P8pvPCvK38JprSdJ4owlJqdQdFfk6rhCxyyJSkGlChqsG7n4s48iq69Pw1eU");
        send({{"type", "relay.grant"}, {"url", "wss://rv.conformance.invalid/v1/relay"},
              {"token", token}, {"expires", 1800000120}});
        QTRY_COMPARE(granted.size(), 1);
        QVERIFY(granted.at(0).at(0).toByteArray().isEmpty());
        QVERIFY(client.relayGrant().has_value());
        QCOMPARE(client.relayGrant()->token, token);
        send({{"type", "introduction.end"}, {"code", "stationLeft"}});
        QTRY_COMPARE(ended.size(), 1);
        QVERIFY(!client.relayGrant().has_value());
        client.stop();
    }

    // Fix wave I3: a service that sends its hello and then never registers
    // the Core is left after the hello time, and the reconnect runs; the
    // time is shortened through the injectable value.
    void aServiceThatNeverRegistersTheCoreIsLeft()
    {
        ServicePlayer player;
        auto coreKey = makeKey();
        RendezvousClient core;
        core.setServers({player.url()});
        QCOMPARE(core.helloTimeoutMs(), RendezvousClient::kHelloTimeoutMs);
        core.setHelloTimeoutMs(200);
        core.setReconnectDelaysMs({50});
        QSignalSpy lost(&core, &RendezvousClient::connectionLost);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [](const QByteArray&) { return QByteArray(); });
        QWebSocket* first = player.waitForConnection();
        QVERIFY(first != nullptr);
        first->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
                                        {"nonce", b64(randomBytes(32))}, {"stun", QJsonArray()}}));
        const std::optional<QString> registration = player.waitForMessage(first);
        QVERIFY(registration.has_value());
        QVERIFY(registration->contains(QLatin1String("\"register\"")));
        // Then nothing: no challenge, no registered.
        QTRY_COMPARE_WITH_TIMEOUT(lost.size(), 1, 5000);
        QVERIFY(!core.isRegistered());
        QWebSocket* second = player.waitForConnection();
        QVERIFY(second != nullptr);
        core.stop();
    }

    // Task 28 fix wave (the reconnect seen on the live service): whatever
    // makes the Core reconnect (the service closing its connection, a
    // registration that never finishes, and, in
    // aCoreThatHearsNoPongsLeavesThatConnectionFirst below, a Core that
    // stopped hearing its pongs), it closes the connection it had before it
    // opens the next, so the service never sees two of the Core's
    // connections at once from the Core's side. ("registered again; closing
    // the older connection" on the service then means the older one's close
    // never reached it.)
    void theCoreClosesItsConnectionBeforeItOpensTheNext()
    {
        ServicePlayer player;
        auto coreKey = makeKey();
        RendezvousClient core;
        core.setServers({player.url()});
        core.setHelloTimeoutMs(300);
        core.setReconnectDelaysMs({50});
        QSignalSpy registered(&core, &RendezvousClient::registered);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [](const QByteArray&) { return QByteArray(); });
        const QString id = Wire::rendezvousId(coreKey->spki());
        const auto registerOn = [&](QWebSocket* socket) {
            socket->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
                                             {"nonce", b64(randomBytes(32))},
                                             {"stun", QJsonArray()}}));
            QVERIFY(player.waitForMessage(socket).has_value());
            socket->sendTextMessage(compact(QJsonObject{{"type", "challenge"},
                                                        {"nonce", b64(randomBytes(32))}}));
            QVERIFY(player.waitForMessage(socket).has_value());
            socket->sendTextMessage(compact(QJsonObject{{"type", "registered"}, {"id", id}}));
        };
        // Registered, then the service closes the connection.
        QWebSocket* first = player.waitForConnection();
        QVERIFY(first != nullptr);
        registerOn(first);
        QTRY_COMPARE(registered.size(), 1);
        first->close();
        // Registered again, then the service never finishes the next
        // registration: the Core leaves at its registration timeout.
        QWebSocket* second = player.waitForConnection();
        QVERIFY(second != nullptr);
        second->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
                                         {"nonce", b64(randomBytes(32))},
                                         {"stun", QJsonArray()}}));
        QVERIFY(player.waitForMessage(second).has_value());  // register; nothing follows
        QWebSocket* third = player.waitForConnection(5000);
        QVERIFY(third != nullptr);
        registerOn(third);
        QTRY_COMPARE(registered.size(), 2);
        // Each new connection arrived with none of the Core's older ones
        // open.
        QCOMPARE(player.openAtArrival, QList<int>({0, 0, 0}));
        core.stop();
    }

    // Task 28 tail (R-IOS-16), the Rock's registrations: a registered Core
    // pings every kPingIntervalMs and leaves the connection when a ping is
    // still unanswered at the next tick, the service's own rule (G-08). On
    // a lossy path (LossyLink: shortened interval, the same rule) pongs
    // held for 5% or 15% of the interval (1 s and 3 s of the real 20 s)
    // keep the registration and the one connection.
    void aLostOrLatePongKeepsTheRegistration_data()
    {
        QTest::addColumn<int>("dropped");
        QTest::addColumn<int>("delayPercent");
        QTest::newRow("every pong 5% late") << 0 << 5;
        QTest::newRow("every pong 15% late") << 0 << 15;
    }

    void aLostOrLatePongKeepsTheRegistration()
    {
        QFETCH(int, dropped);
        QFETCH(int, delayPercent);
        constexpr int kIntervalMs = 400;
        ServicePlayer player;
        LossyLink link(player.port());
        link.dropNextPongs(dropped);
        link.setPongDelayMs(kIntervalMs * delayPercent / 100);
        auto coreKey = makeKey();
        RendezvousClient core;
        core.setServers({link.url()});
        QCOMPARE(core.pingIntervalMs(), RendezvousClient::kPingIntervalMs);
        core.setPingIntervalMs(kIntervalMs);
        core.setReconnectDelaysMs({50});
        QSignalSpy registered(&core, &RendezvousClient::registered);
        QSignalSpy lost(&core, &RendezvousClient::connectionLost);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [](const QByteArray&) { return QByteArray(); });
        QWebSocket* socket = player.waitForConnection();
        QVERIFY(socket != nullptr);
        registerOnPlayer(player, socket, Wire::rendezvousId(coreKey->spki()));
        QTRY_COMPARE(registered.size(), 1);
        // Eight pings.
        QTRY_VERIFY_WITH_TIMEOUT(link.pongsSeen() >= 8, 20 * kIntervalMs);
        QCOMPARE(link.pongsDropped(), dropped);
        QCOMPARE(lost.size(), 0);
        QVERIFY(core.isRegistered());
        QCOMPARE(link.openAtArrival.size(), 1);
        core.stop();
    }

    // Task 28 tail (R-IOS-16), and the ping case the comment above names: a
    // path that carries no pong is found at the second tick after the last
    // pong (one ping unanswered for a whole interval, the service's own
    // 20 s ping and 20 s timeout, G-08), the Core leaves that connection
    // before it opens the next, and it registers again once pongs come
    // back.
    void aCoreThatHearsNoPongsLeavesThatConnectionFirst()
    {
        constexpr int kIntervalMs = 400;
        ServicePlayer player;
        LossyLink link(player.port());
        auto coreKey = makeKey();
        RendezvousClient core;
        core.setServers({link.url()});
        core.setPingIntervalMs(kIntervalMs);
        core.setReconnectDelaysMs({50});
        QSignalSpy registered(&core, &RendezvousClient::registered);
        QSignalSpy lost(&core, &RendezvousClient::connectionLost);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [](const QByteArray&) { return QByteArray(); });
        const QString id = Wire::rendezvousId(coreKey->spki());
        QWebSocket* first = player.waitForConnection();
        QVERIFY(first != nullptr);
        registerOnPlayer(player, first, id);
        QTRY_COMPARE(registered.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(link.pongsSeen() >= 1, 10 * kIntervalMs);
        QCOMPARE(lost.size(), 0);

        // From here the service's pongs never reach the Core.
        link.setDropAllPongs(true);
        QElapsedTimer silent;
        silent.start();
        QTRY_COMPARE_WITH_TIMEOUT(lost.size(), 1, 20 * kIntervalMs);
        // One tick sends a ping, the next finds it unanswered and ends the
        // connection: never before one interval of silence, and after only
        // one lost pong (it took two before G-08, 20 s later than the
        // service gives up on the same path).
        QVERIFY2(silent.elapsed() >= kIntervalMs - kIntervalMs / 10,
                 qPrintable(QStringLiteral("left after %1 ms").arg(silent.elapsed())));
        QCOMPARE(link.pongsDropped(), 1);

        link.setDropAllPongs(false);
        QWebSocket* second = player.waitForConnection();
        QVERIFY(second != nullptr);
        registerOnPlayer(player, second, id);
        QTRY_COMPARE(registered.size(), 2);
        QVERIFY(core.isRegistered());
        // The first connection was closed at the Core's end before the
        // second opened.
        QCOMPARE(link.openAtArrival, QList<int>({0, 0}));
        core.stop();
    }

    // Fix wave: `replaced` keeps the backoff climbing across the next
    // registration, so two Cores that share a key do not flap at the first
    // rung; an ordinary loss still starts again from the first rung once
    // the Core is registered.
    void aReplacedCoreKeepsBackingOff()
    {
        ServicePlayer player;
        auto coreKey = makeKey();
        RendezvousClient core;
        core.setServers({player.url()});
        core.setReconnectDelaysMs({20, 40, 60, 80});
        QSignalSpy registered(&core, &RendezvousClient::registered);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [](const QByteArray&) { return QByteArray(); });
        const QString id = Wire::rendezvousId(coreKey->spki());
        const auto registerOn = [&](QWebSocket* socket) {
            socket->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
                                             {"nonce", b64(randomBytes(32))},
                                             {"stun", QJsonArray()}}));
            QVERIFY(player.waitForMessage(socket).has_value());
            socket->sendTextMessage(compact(QJsonObject{{"type", "challenge"}, {"nonce", b64(randomBytes(32))}}));
            QVERIFY(player.waitForMessage(socket).has_value());
            socket->sendTextMessage(compact(QJsonObject{{"type", "registered"}, {"id", id}}));
        };
        const auto replace = [](QWebSocket* socket) {
            socket->sendTextMessage(compact(QJsonObject{{"type", "error"}, {"code", "replaced"},
                                             {"reason", "The Core registered again on another "
                                                        "connection, so this one was closed."},
                                             {"retryAfterMs", 0}}));
            socket->close();
        };

        QWebSocket* socket = player.waitForConnection();
        QVERIFY(socket != nullptr);
        registerOn(socket);
        QTRY_COMPARE(registered.size(), 1);
        QCOMPARE(core.reconnectAttempts(), 0);
        for (int round = 1; round <= 2; ++round) {
            replace(socket);
            socket = player.waitForConnection();
            QVERIFY(socket != nullptr);
            registerOn(socket);
            QTRY_COMPARE(registered.size(), 1 + round);
            QCOMPARE(core.reconnectAttempts(), round);
        }
        // An ordinary loss: registered again, back to the first rung.
        socket->close();
        socket = player.waitForConnection();
        QVERIFY(socket != nullptr);
        registerOn(socket);
        QTRY_COMPARE(registered.size(), 4);
        QCOMPARE(core.reconnectAttempts(), 0);
        core.stop();
    }

    // Fix wave: a `nameplate` the Core did not claim, and a second
    // `mailbox.opened` while one is open, are ignored.
    void unaskedNameplatesAndSecondMailboxesAreIgnored()
    {
        ServicePlayer player;
        auto coreKey = makeKey();
        RendezvousClient core;
        core.setServers({player.url()});
        QSignalSpy registered(&core, &RendezvousClient::registered);
        QSignalSpy claimed(&core, &RendezvousClient::nameplateClaimed);
        QSignalSpy opened(&core, &RendezvousClient::mailboxOpened);
        QSignalSpy received(&core, &RendezvousClient::mailboxReceived);
        core.registerStation(coreKey->spki(),
                             [coreKey](const QByteArray& m) { return coreKey->sign(m); },
                             [](const QByteArray&) { return QByteArray(); });
        QWebSocket* socket = player.waitForConnection();
        QVERIFY(socket != nullptr);
        const auto send = [socket](const QJsonObject& message) {
            socket->sendTextMessage(compact(message));
        };
        send({{"type", "hello"}, {"version", 1}, {"nonce", b64(randomBytes(32))},
              {"stun", QJsonArray()}});
        QVERIFY(player.waitForMessage(socket).has_value());
        send({{"type", "challenge"}, {"nonce", b64(randomBytes(32))}});
        QVERIFY(player.waitForMessage(socket).has_value());
        send({{"type", "registered"}, {"id", Wire::rendezvousId(coreKey->spki())}});
        QTRY_COMPARE(registered.size(), 1);

        // Messages are handled in order, so once the mailbox opens the
        // nameplate before it has been dealt with.
        send({{"type", "nameplate"}, {"nameplate", 7}});
        send({{"type", "mailbox.opened"}, {"nameplate", 7}});
        QTRY_COMPARE(opened.size(), 1);
        QCOMPARE(claimed.size(), 0);
        send({{"type", "mailbox.opened"}, {"nameplate", 8}});
        send({{"type", "mailbox"}, {"body", "one"}});
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(opened.size(), 1);

        // A claim this Core made is answered as before.
        core.claimNameplate();
        const std::optional<QString> claim = player.waitForMessage(socket);
        QVERIFY(claim.has_value());
        QVERIFY(claim->contains(QLatin1String("nameplate.claim")));
        send({{"type", "nameplate"}, {"nameplate", 9}});
        QTRY_COMPARE(claimed.size(), 1);
        QCOMPARE(claimed.first().first().toInt(), 9);
        core.stop();
    }

    void theSenderKeepsToTheWire()
    {
        // Section 2: a message over the cap is not sent, whatever its kind.
        Wire::Message mailbox;
        mailbox.kind = Wire::Kind::Mailbox;
        mailbox.body = QString(Wire::kMaxBodyBytes, QLatin1Char('b'));
        QVERIFY(!Wire::encode(Wire::Direction::ClientToService, mailbox).isEmpty());
        mailbox.body.append(QLatin1Char('b'));
        QVERIFY(Wire::encode(Wire::Direction::ClientToService, mailbox).isEmpty());
        // Control characters escape to six bytes each: the field fits, the
        // message does not.
        mailbox.body = QString(Wire::kMaxBodyBytes, QChar(0x01));
        QVERIFY(Wire::encode(Wire::Direction::ClientToService, mailbox).size() == 0);
        // A lone surrogate is never sent.
        mailbox.body = QString(QChar(0xD800));
        QVERIFY(Wire::encode(Wire::Direction::ClientToService, mailbox).isEmpty());
        // An `a=` is taken off a candidate, and one that still does not
        // start `candidate:` is refused.
        QCOMPARE(Wire::wireCandidate(QStringLiteral("a=candidate:1 1 UDP 1 ::1 5 typ host")),
                 QStringLiteral("candidate:1 1 UDP 1 ::1 5 typ host"));
        Wire::Message candidate;
        candidate.kind = Wire::Kind::Candidate;
        candidate.candidate = QStringLiteral("a=candidate:1 1 UDP 1 ::1 5 typ host");
        QVERIFY(Wire::encode(Wire::Direction::ClientToService, candidate).isEmpty());
        // A station never sends a client's kinds, nor a client a station's.
        Wire::Message claim;
        claim.kind = Wire::Kind::NameplateClaim;
        QVERIFY(Wire::encode(Wire::Direction::ClientToService, claim).isEmpty());
        QVERIFY(!Wire::encode(Wire::Direction::StationToService, claim).isEmpty());
    }

    void serverAddressesAreRead()
    {
        QStringList rejected;
        const QList<QUrl> urls = RendezvousClient::serverUrls(
            {QStringLiteral("rv.nereussdr.com"), QStringLiteral("rv.example.net:8443"),
             QStringLiteral("[2001:db8::5]:443"), QStringLiteral("wss://rv.example.org/path"),
             QStringLiteral("ws://127.0.0.1:8710"), QStringLiteral("ws://rv.example.net"),
             QStringLiteral("http://rv.example.net"), QStringLiteral("rv.nereussdr.com")},
            &rejected);
        QCOMPARE(urls,
                 QList<QUrl>({QUrl(QStringLiteral("wss://rv.nereussdr.com/")),
                              QUrl(QStringLiteral("wss://rv.example.net:8443/")),
                              QUrl(QStringLiteral("wss://[2001:db8::5]:443/")),
                              QUrl(QStringLiteral("wss://rv.example.org/path")),
                              QUrl(QStringLiteral("ws://127.0.0.1:8710/"))}));
        QCOMPARE(rejected, QStringList({QStringLiteral("ws://rv.example.net"),
                                        QStringLiteral("http://rv.example.net")}));
    }

    // Every test binary runs in test mode (tests/TestSandboxInit.cpp): a
    // Core started with the default server list, as tst_daemon_app starts
    // one, never reaches rv.nereussdr.com from a test.
    // Task 29 step 2b (options survey B.6, B.7): a network that breaks
    // the secure connection to the service is named in plain words. The
    // service is not pinned: these are the system's own trust errors.
    void aNetworkThatBreaksTheSecureConnectionIsNamed_data()
    {
        QTest::addColumn<QByteArray>("altName");
        QTest::addColumn<bool>("trusted");
        QTest::addColumn<QString>("words");
        // The right name, an authority this computer does not trust.
        QTest::newRow("inspecting") << QByteArray("IP:127.0.0.1") << false
            << QStringLiteral("This computer does not trust the certificate for the secure "
                              "connection. Network inspection is one possible cause. Check "
                              "the network's certificate policy or try another network.");
        // A trusted certificate for another name: a sign-in page answering
        // for every address with its own (TLS stacks stop at an untrusted
        // authority before they check the name).
        QTest::newRow("sign-in page") << QByteArray("DNS:portal.example.net") << true
            << QStringLiteral("The secure connection answered with a certificate for another "
                              "name. A Wi-Fi sign-in page is one possible cause. Check whether "
                              "this network needs browser sign-in, then try again.");
    }

    void aNetworkThatBreaksTheSecureConnectionIsNamed()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend on this machine");
        }
        QFETCH(QByteArray, altName);
        QFETCH(bool, trusted);
        QFETCH(QString, words);
        QTemporaryDir dir;
        const QSslCertificate certificate
            = writeLoopbackCertificate(dir.path(), altName.constData());
        QVERIFY(!certificate.isNull());
        std::optional<TrustedAuthority> authority;
        if (trusted) {
            authority.emplace(certificate);
        }
        QFile keyFile(QDir(dir.path()).filePath(QStringLiteral("tls-key.pem")));
        QVERIFY(keyFile.open(QIODevice::ReadOnly));
        QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
        tls.setLocalCertificate(certificate);
        tls.setPrivateKey(QSslKey(keyFile.readAll(), QSsl::Rsa));
        QWebSocketServer network(QStringLiteral("network"), QWebSocketServer::SecureMode);
        network.setSslConfiguration(tls);
        QVERIFY(network.listen(QHostAddress::LocalHost, 0));
        RendezvousClient client;
        client.setServers({QUrl(QStringLiteral("wss://127.0.0.1:%1/").arg(network.serverPort()))});
        QSignalSpy unreachable(&client, &RendezvousClient::unreachable);
        client.connectToService();
        QTRY_COMPARE_WITH_TIMEOUT(unreachable.size(), 1, 15000);
        QCOMPARE(unreachable.at(0).at(0).toString(), words);
    }

    // A network whose proxy demands a login: the service is unreachable,
    // and the reason says why in plain words.
    void aProxyThatNeedsALoginIsSaidPlainly()
    {
        NereusSDR::Test::LoginProxy proxy;
        QVERIFY(proxy.listen());
        proxy.useAsSystemProxy();
        RendezvousClient client;
        client.setServers({QUrl(QStringLiteral("wss://127.0.0.1:9/"))});
        QSignalSpy unreachable(&client, &RendezvousClient::unreachable);
        client.connectToService();
        QTRY_COMPARE_WITH_TIMEOUT(unreachable.size(), 1, 15000);
        QVERIFY(proxy.requests() >= 1);
        QCOMPARE(unreachable.at(0).at(0).toString(),
                 QStringLiteral("This network's proxy needs a login, which NereusSDR can't provide."));
    }

    void aTestRunNeverLeavesThisComputer()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        RendezvousClient client;
        client.setServers(RendezvousClient::serverUrls({QString::fromLatin1(RendezvousClient::kDefaultServer)}));
        QCOMPARE(client.servers(), QList<QUrl>({QUrl(QStringLiteral("wss://rv.nereussdr.com/"))}));
        QSignalSpy unreachable(&client, &RendezvousClient::unreachable);
        QSignalSpy connected(&client, &RendezvousClient::connected);
        client.connectToService();
        QTRY_COMPARE(unreachable.size(), 1);
        QCOMPARE(connected.size(), 0);
        QCOMPARE(client.findChildren<QWebSocket*>().size(), 0);
    }

    // The service is reached by host name through a web server (Caddy on
    // rv.nereussdr.com), so the opening request is an HTTP/1.1 Upgrade
    // (QWebSocket's only kind; never HTTP/2's extended CONNECT) carrying the
    // service's name, not an address it resolved to.
    void theServiceIsAskedForByName()
    {
        ServicePlayer player;
        RendezvousClient client;
        client.setServers(RendezvousClient::serverUrls(
            {QStringLiteral("ws://localhost:%1").arg(player.port())}));
        client.connectToService();
        QVERIFY(player.waitForConnection() != nullptr);
        QCOMPARE(player.hostHeaders,
                 QStringList({QStringLiteral("localhost:%1").arg(player.port())}));
        client.stop();
    }

    // ── The Core's and the desktop's runners (section 10.4) ────────────

    void coreRunner_data()
    {
        QTest::addColumn<QString>("file");
        const QJsonArray fixtures =
            readJson(kSuite + QStringLiteral("/manifest.json")).value(QStringLiteral("fixtures")).toArray();
        int count = 0;
        for (const QJsonValue& value : fixtures) {
            const QString file = value.toObject().value(QStringLiteral("file")).toString();
            if (value.toObject().value(QStringLiteral("kind")).toString() != QLatin1String("session")) {
                continue;
            }
            const QJsonArray runs = readJson(kSuite + QLatin1Char('/') + file).value(QStringLiteral("runs")).toArray();
            if (runs.contains(QJsonValue(QStringLiteral("core")))) {
                QTest::newRow(qPrintable(file)) << file;
                ++count;
            }
        }
        // Every session fixture the manifest lists for the Core runs here
        // (Task 29 fix wave: the relay grant's added two); never fewer than
        // version 1's ten.
        QVERIFY2(count >= 10, qPrintable(QString::number(count)));
    }

    void coreRunner()
    {
        QFETCH(QString, file);
        runCoreFixture(file);
    }

    void appRunner_data()
    {
        QTest::addColumn<QString>("file");
        const QJsonArray fixtures =
            readJson(kSuite + QStringLiteral("/manifest.json")).value(QStringLiteral("fixtures")).toArray();
        for (const QJsonValue& value : fixtures) {
            const QString file = value.toObject().value(QStringLiteral("file")).toString();
            if (value.toObject().value(QStringLiteral("kind")).toString() != QLatin1String("session")) {
                continue;
            }
            const QJsonArray runs = readJson(kSuite + QLatin1Char('/') + file).value(QStringLiteral("runs")).toArray();
            if (runs.contains(QJsonValue(QStringLiteral("app")))) {
                QTest::newRow(qPrintable(file)) << file;
            }
        }
    }

    void appRunner()
    {
        QFETCH(QString, file);
        runAppFixture(file);
    }

    // ── Against the Python service on this computer ────────────────────

    void aPairedDeviceConnectsOverIceUsingStun()
    {
        // STUN only: the service has no relay, so the connection is direct.
        LocalService service(/*stun=*/true, /*relay=*/false);
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        // The id comes from the station identity key, never the TLS
        // certificate's.
        QCOMPARE(rendezvous.client()->stationId(),
                 Wire::rendezvousId(core.server->stationIdentity().publicKeySpki()));

        RendezvousClient client;
        client.setServers({service.url()});
        IcePair pair(rendezvous.client(), &client, /*relayOnly=*/false);
        pair.start(rendezvous.client()->stationId(), phone);
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, 30000, &why), qPrintable(why));
        }
        const auto path = pair.offerer()->selectedPath();
        QVERIFY(path.has_value());
        QVERIFY(!path->relayed());
        // The service's STUN server was used: both ends gathered a
        // server-reflexive candidate from it.
        const auto gatheredThroughServer = [](const QStringList& candidates) {
            for (const QString& candidate : candidates) {
                if (IceConfiguration::candidateType(candidate) == QLatin1String("srflx")) {
                    return true;
                }
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT(gatheredThroughServer(pair.clientCandidates), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(gatheredThroughServer(pair.stationCandidates), 10000);
        QCOMPARE(rendezvous.client()->droppedIntroductions(), quint64(0));
    }

    // With the relay on offer too, a working direct path is the one used.
    void aDirectPathIsPreferredOverTheRelay()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        RendezvousClient client;
        client.setServers({service.url()});
        IcePair pair(rendezvous.client(), &client, /*relayOnly=*/false);
        pair.start(rendezvous.client()->stationId(), phone);
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, 30000, &why), qPrintable(why));
        }
        QCOMPARE(pair.stationRelays, 1);
        QCOMPARE(pair.clientRelays, 1);
        QTRY_VERIFY_WITH_TIMEOUT(pair.offerer()->selectedPath().has_value()
                                     && !pair.offerer()->selectedPath()->relayed(),
                                 15000);
    }

    // A full relay (TURN 486, Allocation Quota Reached) is an ordinary
    // outcome: gathering finishes without it on both ends and the
    // connection goes on directly.
    void aFullRelayIsNotFatal()
    {
        LocalService service;
        service.setRelayFull(true);
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        RendezvousClient client;
        client.setServers({service.url()});
        IcePair pair(rendezvous.client(), &client, false);
        pair.start(rendezvous.client()->stationId(), phone);
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, 30000, &why), qPrintable(why));
        }
        QTRY_VERIFY_WITH_TIMEOUT(pair.clientGathered && pair.stationGathered, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(service.turnOutput().contains(QLatin1String("QUOTA 486")), 5000);
        QVERIFY(!service.turnOutput().contains(QLatin1String("ALLOCATED")));
        for (const QString& candidate : pair.clientCandidates + pair.stationCandidates) {
            QVERIFY(IceConfiguration::candidateType(candidate) != QLatin1String("relay"));
        }
        QVERIFY(!pair.offerer()->selectedPath()->relayed());
    }

    // Only relay candidates are passed on, both ways, and the connection
    // works through the relay. This asserts what a loopback test can prove
    // (the follow-up to the Task 27 re-review): each end allocated on the
    // relay, the relay carried checks both ways (at least one relay pair's
    // checks crossed it), and the message arrived. It does not assert the
    // relayed pair was the one chosen: on the loopback nothing blocks a
    // direct packet, and libjuice pairs a peer-reflexive candidate learned
    // from a relayed check with the host socket, so a direct pair can form
    // and win. "Direct blocked means relayed" is the traversal harness's
    // udp-direct-blocked scenario, where packets really are dropped.
    void withOnlyRelayCandidatesTheRelayCarriesTheConnection()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);

        RendezvousClient client;
        client.setServers({service.url()});
        IcePair pair(rendezvous.client(), &client, /*relayOnly=*/true);
        pair.start(rendezvous.client()->stationId(), phone);
        // The product's bound for an ICE connection through the service:
        // gathering, then the connectivity checks, after which failure is
        // certain (IceConfiguration::kConnectDeadlineMs). The service's own
        // client idle close starts only when the introduction ends (its
        // 120 s lifetime), so it cannot cut this wait short. On failure the
        // relay's and the service's output say which leg stalled.
        // The wait stops at that bound (a QTRY_* ran on to three times it,
        // past ctest's 300 s for the whole binary) or as soon as the relay
        // exits.
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, IceConfiguration::kConnectDeadlineMs, &why),
                     qPrintable(why));
        }
        QCOMPARE(pair.stationRelays, 1);
        QCOMPARE(pair.clientRelays, 1);
        QVERIFY(pair.offerer()->selectedPath().has_value());
        // Both ends allocated, and the relay carried checks both ways.
        QTRY_VERIFY_WITH_TIMEOUT(service.turnOutput().contains(QLatin1String("ALLOCATED 2")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(service.turnOutput().contains(QLatin1String("RELAYED OUT 1")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(service.turnOutput().contains(QLatin1String("RELAYED IN 1")), 5000);
        // The message arrives.
        QTRY_VERIFY_WITH_TIMEOUT(pair.offerer()->sendDisplay(QByteArrayLiteral("through the relay")),
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(pair.stationReceived.contains(QByteArrayLiteral("through the relay")),
                                 5000);
    }

    // Both ends of a connection carried by the relay closed and destroyed
    // in the same turn of the event loop, each close racing the other
    // end's teardown: both allocations are still given back, not held for
    // their lifetime (the libjuice agent's bounded release outlives the
    // transports, cmake/patches/libdatachannel-0003).
    void aRelayIsGivenBackWhenBothEndsCloseAtOnce()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);

        RendezvousClient client;
        client.setServers({service.url()});
        IcePair pair(rendezvous.client(), &client, /*relayOnly=*/true);
        pair.start(rendezvous.client()->stationId(), phone);
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, IceConfiguration::kConnectDeadlineMs, &why),
                     qPrintable(why));
        }
        QVERIFY(pair.offerer()->selectedPath().has_value());
        {
            QString why;
            QVERIFY2(waitForRelayLine(service, QStringLiteral("ALLOCATED 2"), 5000, &why),
                     qPrintable(why));
        }
        QVERIFY(!service.turnOutput().contains(QLatin1String("RELEASED")));

        pair.closeBothAtOnce();
        // The agents' release window is 5 s (libjuice-0002's
        // TURN_CLOSE_DEADLINE); this waits three times that at most.
        {
            QString why;
            QVERIFY2(waitForRelayLine(service, QStringLiteral("RELEASED 2"), 15000, &why),
                     qPrintable(why));
        }
    }

    // The fake relay itself: a datagram it cannot send (here to a
    // documentation address, RFC 5737, which its loopback-bound relay
    // socket cannot reach, as it could not reach this computer's VPN
    // address) is dropped and counted, and the relay carries on and takes
    // the release after it. Before, the send's error ended the relay, and
    // no release that followed was ever seen.
    void theFakeRelayCarriesOnPastADatagramItCannotSend()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        const QHostAddress unreachable(QStringLiteral("192.0.2.1"));
        constexpr quint16 kDiscardPort = 9;
        TurnProbe probe(service.turnPort(), service.turnSecret());
        QVERIFY2(probe.allocate(), qPrintable(service.turnReport()));
        QVERIFY2(probe.permit(unreachable, kDiscardPort), qPrintable(service.turnReport()));
        probe.sendTo(unreachable, kDiscardPort, QByteArrayLiteral("nowhere to go"));
        // The relay reads its one socket in order: the release is answered
        // after the Send indication was handled.
        QVERIFY2(probe.release(), qPrintable(service.turnReport()));
        QString why;
        QVERIFY2(waitForRelayLine(service, QStringLiteral("RELEASED 1"), 5000, &why),
                 qPrintable(why));
        QVERIFY2(service.turnOutput().contains(QLatin1String("UNSENT 1")),
                 qPrintable(service.turnReport()));
        QVERIFY2(service.turnProcess()->state() == QProcess::Running,
                 qPrintable(service.turnReport()));
    }

    // The service a test starts ends with the process it was tied to, even
    // when no destructor runs (a test killed at its ctest timeout): here
    // tied to a stand-in process that is then killed.
    void theServiceEndsWithTheProcessThatStartedIt()
    {
        QProcess standIn;
        standIn.start(QStringLiteral("/bin/sleep"), {QStringLiteral("600")});
        QVERIFY(standIn.waitForStarted(10000));
        LocalService service;
        service.setParentPidForTest(standIn.processId());
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        QProcess* running = service.serviceProcess();
        QVERIFY(running != nullptr);
        QCOMPARE(running->state(), QProcess::Running);
        standIn.kill();
        QVERIFY(standIn.waitForFinished(10000));
        // The launcher looks every 0.5 s (PARENT_POLL_S); ten times that.
        constexpr int kEndBoundMs = 5000;
        QVERIFY2(running->waitForFinished(kEndBoundMs),
                 qPrintable(QStringLiteral("the service was still running %1 ms after the "
                                           "process it was tied to ended\n%2")
                                .arg(kEndBoundMs)
                                .arg(service.log())));
    }

    void relayDeniedAsksForNoCredentials()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/false);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QSignalSpy credentials(rendezvous.client(), &RendezvousClient::credentialsReceived);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        RendezvousClient client;
        client.setServers({service.url()});
        QSignalSpy answers(&client, &RendezvousClient::answerReceived);
        IcePair pair(rendezvous.client(), &client, false);
        pair.start(rendezvous.client()->stationId(), phone);
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, 30000, &why), qPrintable(why));
        }
        QCOMPARE(credentials.size(), 0);
        QCOMPARE(answers.size(), 1);
        QCOMPARE(answers.at(0).at(1).toBool(), false);
        QVERIFY(!pair.offerer()->selectedPath()->relayed());
    }

    void unpairedAndRevokedDevicesGetNoAnswerAndAreCounted()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto stranger = makeKey();
        auto revoked = makeKey();
        QVERIFY(core.pair(*revoked));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QSignalSpy introduced(rendezvous.client(), &RendezvousClient::introduced);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        QVERIFY(core.server->deviceStore()->remove(StationIdentity::fingerprintOf(revoked->spki())));

        const QString offer = readText(kSuite + QStringLiteral("/sdp/offer.sdp"));
        for (const auto& key : {stranger, revoked}) {
            RendezvousClient client;
            client.setServers({service.url()});
            QSignalSpy answers(&client, &RendezvousClient::answerReceived);
            QSignalSpy errors(&client, &RendezvousClient::serviceError);
            const quint64 droppedBefore = rendezvous.client()->droppedIntroductions();
            client.introduce(rendezvous.client()->stationId(), key->spki(),
                             [key](const QByteArray& message) { return key->sign(message); },
                             offer);
            // The Core has decided once it counts the drop; an answer would
            // have been sent before that, and the service forwards in order.
            QTRY_COMPARE_WITH_TIMEOUT(rendezvous.client()->droppedIntroductions(),
                                      droppedBefore + 1, 10000);
            QCOMPARE(answers.size(), 0);
            // Silence, not `offline`: the service cannot tell whether a
            // device is paired (section 6.4).
            QCOMPARE(errors.size(), 0);
        }
        QCOMPARE(introduced.size(), 0);
        QCOMPARE(rendezvous.client()->droppedIntroductions(), quint64(2));
    }

    void pairingByCodeGoesThroughTheMailboxWithoutTheCode()
    {
        QVERIFY(SpakeExchange::isAvailable());
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        RecordingRelay relay(service.url());
        Core core;
        QVERIFY(core.server->pairingWindow()->isOpen());
        StationRendezvous rendezvous(core.server.get(), {relay.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy nameplates(rendezvous.client(), &RendezvousClient::nameplateClaimed);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(nameplates.size(), 1, 10000);
        // The code the Core shows carries the nameplate the service gave it.
        const int nameplate = nameplates.at(0).at(0).toInt();
        QTRY_VERIFY(core.server->pairingWindow()->currentCode().startsWith(
            QString::number(nameplate) + QLatin1Char('-')));
        const QString code = core.server->pairingWindow()->currentCode();

        QTemporaryDir keyDir;
        auto identity = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        StationPairingClient pairing(identity, QStringLiteral("Shack MacBook"));
        QSignalSpy paired(&pairing, &StationPairingClient::paired);
        QSignalSpy failed(&pairing, &StationPairingClient::failed);
        pairing.pairByCodeFromAnywhere(code, {relay.url()});
        QTRY_VERIFY_WITH_TIMEOUT(!paired.isEmpty() || !failed.isEmpty(), 60000);
        QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.at(0).at(0).toString()));
        const PairedStationRecord record = paired.at(0).at(0).value<PairedStationRecord>();
        QCOMPARE(record.identityKey, core.server->stationIdentity().publicKeySpki());
        QVERIFY(core.server->deviceStore()->find(identity->fingerprint()).has_value());
        // The first pairing claims the Core and closes its window; the
        // nameplate goes back.
        QTRY_VERIFY(!core.server->pairingWindow()->isOpen());

        // What the service was given: pairing messages, forwarded
        // unchanged, and never the code.
        QStringList sent;
        QStringList delivered;
        for (const QString& text : std::as_const(relay.toService)) {
            const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
            if (message.value(QStringLiteral("type")).toString() == QLatin1String("mailbox")) {
                sent.append(message.value(QStringLiteral("body")).toString());
            }
        }
        for (const QString& text : std::as_const(relay.fromService)) {
            const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
            if (message.value(QStringLiteral("type")).toString() == QLatin1String("mailbox")) {
                delivered.append(message.value(QStringLiteral("body")).toString());
            }
        }
        QVERIFY(sent.size() >= 5);
        QCOMPARE(delivered, sent);
        for (const QString& body : std::as_const(sent)) {
            QVERIFY(QJsonDocument::fromJson(body.toUtf8())
                        .object()
                        .value(QStringLiteral("type"))
                        .toString()
                        .startsWith(QLatin1String("pair.")));
        }
        const QStringList words = code.split(QLatin1Char('-')).mid(1);
        QCOMPARE(words.size(), 2);
        for (const QString& text : relay.toService + relay.fromService) {
            QVERIFY(!text.contains(code));
            for (const QString& word : words) {
                QVERIFY(!text.contains(word, Qt::CaseInsensitive));
            }
        }
        // The service's own log records the mailbox (never its bodies).
        QTRY_VERIFY_WITH_TIMEOUT(service.log().contains(QLatin1String("mailbox opened")), 5000);
        QVERIFY(!service.log().contains(code));

        // Task 28 fix wave (privacy): no device name and no Core label in
        // the clear. The plain pair.start names the computer only
        // "Computer"; its own name went sealed, and that is the name the
        // Core recorded. The Core's label goes only in its sealed box.
        const QString label = core.server->devicesFacade()->stationLabel();
        QStringList secrets{QStringLiteral("Shack MacBook"), QStringLiteral("KG4VCF")};
        if (!label.isEmpty()) {
            secrets.append(label);
        }
        for (const QString& text : relay.toService + relay.fromService) {
            for (const QString& secret : std::as_const(secrets)) {
                QVERIFY2(!text.contains(secret, Qt::CaseInsensitive), qPrintable(secret));
            }
        }
        bool sawStart = false;
        for (const QString& body : std::as_const(sent)) {
            const QJsonObject message = QJsonDocument::fromJson(body.toUtf8()).object();
            if (message.value(QStringLiteral("type")).toString() == QLatin1String("pair.start")) {
                sawStart = true;
                QCOMPARE(message.value(QStringLiteral("device")).toObject()
                             .value(QStringLiteral("name")).toString(),
                         QStringLiteral("Computer"));
            }
            QVERIFY(message.value(QStringLiteral("type")).toString()
                    != QLatin1String("pair.accept"));
        }
        QVERIFY(sawStart);
        QCOMPARE(core.server->deviceStore()->find(identity->fingerprint())->name,
                 QStringLiteral("Shack MacBook"));
    }

    void freshMailboxPairingPersistsAndConnectsWithoutDirectUrl()
    {
        QVERIFY(SpakeExchange::isAvailable());
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy nameplates(rendezvous.client(), &RendezvousClient::nameplateClaimed);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(nameplates.size(), 1, 10000);
        const QString code = core.server->pairingWindow()->currentCode();
        QVERIFY(!code.isEmpty());

        QTemporaryDir keyDir;
        auto identity = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        StationPairingClient pairing(identity, QStringLiteral("Shack MacBook"));
        QSignalSpy paired(&pairing, &StationPairingClient::paired);
        QSignalSpy failed(&pairing, &StationPairingClient::failed);
        pairing.pairByCodeFromAnywhere(code, {service.url()});
        QTRY_VERIFY_WITH_TIMEOUT(!paired.isEmpty() || !failed.isEmpty(), 60000);
        QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.at(0).at(0).toString()));
        const auto record = paired.at(0).at(0).value<PairedStationRecord>();
        QVERIFY(record.host.isEmpty());
        QCOMPARE(record.port, quint16(0));
        QCOMPARE(record.identityKey, core.server->stationIdentity().publicKeySpki());
        QCOMPARE(StationIdentity::fingerprintOf(record.identityKey), record.identityFingerprint);

        QTemporaryDir savedDir;
        AppSettings settings(savedDir.filePath(QStringLiteral("targets.settings")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = CoreTargetStore::createId();
        target.label = record.label;
        target.connection.identityFingerprint = record.identityFingerprint;
        target.connection.rendezvousId = Wire::rendezvousId(record.identityKey);
        QVERIFY(store.upsert(target));
        QVERIFY(store.select(target.id));
        CoreTargetStore reloaded(settings);
        QVERIFY(reloaded.load());
        const auto saved = reloaded.target(target.id);
        QVERIFY(saved.has_value());
        QVERIFY(saved->connection.url.isEmpty());
        QCOMPARE(saved->connection.identityFingerprint, record.identityFingerprint);

        rendezvous.setAnswersIntroductionsForTest(true);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(identity, QStringLiteral("Shack MacBook"));
        StationClient::ServiceRoute route;
        route.servers = {service.url()};
        route.rendezvousId = saved->connection.rendezvousId;
        window.setServiceRoute(route);
        window.connectToStation(QUrl(), QString(), QString(), false,
                                saved->connection.identityFingerprint);
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(window.connectionAttempt().tries.size(), 1);
        QVERIFY(window.connectionAttempt().tries.first().path
                == StationConnectionAttempt::Path::Service
            || window.connectionAttempt().tries.first().path
                == StationConnectionAttempt::Path::Relay
            || window.connectionAttempt().tries.first().path
                == StationConnectionAttempt::Path::WebRelay);
        QVERIFY(core.server->hasAuthenticatedSession());
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    void aSessionOutlivesTheService()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        rendezvous.client()->setReconnectDelaysMs({200});
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QSignalSpy lost(rendezvous.client(), &RendezvousClient::connectionLost);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        RendezvousClient client;
        client.setServers({service.url()});
        IcePair pair(rendezvous.client(), &client, false);
        pair.start(rendezvous.client()->stationId(), phone);
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, 30000, &why), qPrintable(why));
        }

        service.stop();
        QTRY_VERIFY_WITH_TIMEOUT(lost.size() >= 1, 10000);
        // The connection the service introduced carries on both ways.
        QTRY_VERIFY_WITH_TIMEOUT(pair.offerer()->sendDisplay(QByteArrayLiteral("after the stop")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(pair.stationReceived.contains(QByteArrayLiteral("after the stop")),
                                 5000);
        QVERIFY(pair.answerer()->sendDisplay(QByteArrayLiteral("and back")));
        QTRY_VERIFY_WITH_TIMEOUT(pair.clientReceived.contains(QByteArrayLiteral("and back")), 5000);
        QVERIFY(pair.offerer()->isReady());
        QVERIFY(pair.answerer()->isReady());

        // The Core registers again once the service is back.
        QVERIFY2(service.launch(), qPrintable(service.startFailure()));
        QTRY_VERIFY_WITH_TIMEOUT(registered.size() >= 2, 20000);
        QVERIFY(pair.offerer()->isReady());
    }

    void ipv6IsPreferredWhenBothEndsHaveIt()
    {
        if (!hasUsableIpv6()) {
            QSKIP("This computer has no global IPv6 address (a unique local, link-local or "
                  "loopback one reaches no server).");
        }
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        RendezvousClient client;
        client.setServers({service.url()});
        IcePair pair(rendezvous.client(), &client, false);
        pair.start(rendezvous.client()->stationId(), phone);
        {
            QString why;
            QVERIFY2(waitForPair(pair, service, 30000, &why), qPrintable(why));
        }
        const auto hasFamily = [](const QStringList& candidates, QAbstractSocket::NetworkLayerProtocol family) {
            for (const QString& candidate : candidates) {
                const QStringList fields = candidate.split(QLatin1Char(' '));
                if (QHostAddress(fields.value(4)).protocol() == family) {
                    return true;
                }
            }
            return false;
        };
        // Both ends gathered IPv6 host candidates (and IPv4 ones).
        QVERIFY(hasFamily(pair.clientCandidates, QAbstractSocket::IPv6Protocol));
        QVERIFY(hasFamily(pair.stationCandidates, QAbstractSocket::IPv6Protocol));
        QVERIFY(hasFamily(pair.clientCandidates, QAbstractSocket::IPv4Protocol));
        // Nomination settles on the highest priority pair; wait for it.
        QTRY_VERIFY_WITH_TIMEOUT(pair.offerer()->selectedPath().has_value()
                                     && QHostAddress(pair.offerer()->selectedPath()->localAddress)
                                            .protocol() == QAbstractSocket::IPv6Protocol,
                                 10000);
        QCOMPARE(QHostAddress(pair.offerer()->selectedPath()->remoteAddress).protocol(),
                 QAbstractSocket::IPv6Protocol);
    }

    // ── The desktop: cached addresses and the attempt record ───────────

    // The pairing design, section 5.3: the cached address first, so a
    // reconnect never needs the remote access service; none is running
    // here at all.
    void aCachedAddressConnectsWithoutTheService()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("No TLS backend");
        }
        Core core;
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        window.setCachedAddresses({core.url()});
        // The saved address is one nothing answers at: it is never needed.
        const QUrl saved(QStringLiteral("wss://127.0.0.1:%1").arg(freeTcpPort()));
        window.connectToStation(saved, QString(), QString(), false,
                                core.server->stationIdentity().fingerprint());
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, 20000, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(window.connectedUrl(), core.url());
        // Task 29 (the link document, section 21.1): a paired Core's
        // addresses are raced, so the saved one is tried too, beside the
        // cached one, and its line says what it met.
        const StationConnectionAttempt attempt = window.connectionAttempt();
        QCOMPARE(attempt.tries.size(), 2);
        QCOMPARE(attempt.tries.at(0).path, StationConnectionAttempt::Path::ThisNetwork);
        QCOMPARE(attempt.tries.at(0).outcome, StationConnectionAttempt::Outcome::Connected);
        QVERIFY(attempt.tries.at(1).outcome == StationConnectionAttempt::Outcome::NoAnswer
                || attempt.tries.at(1).outcome
                    == StationConnectionAttempt::Outcome::AnotherPathFirst);
        QVERIFY(attempt.connected());
        QVERIFY(attempt.summary().startsWith(
            QStringLiteral("Tried this network (127.0.0.1:%1): connected; ")
                .arg(core.server->serverPort())));
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // A cached address nothing answers at, and one another computer
    // answers at, each give way to the next address at once.
    void aDeadOrForeignCachedAddressGivesWay()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("No TLS backend");
        }
        Core core;
        Core other;
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        QVERIFY(other.server->listen(QHostAddress::LocalHost, 0));
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        const QUrl dead(QStringLiteral("wss://127.0.0.1:%1").arg(freeTcpPort()));
        window.setCachedAddresses({dead, other.url()});
        window.connectToStation(core.url(), QString(), QString(), false,
                                core.server->stationIdentity().fingerprint());
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, 20000, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(window.connectedUrl(), core.url());
        const StationConnectionAttempt attempt = window.connectionAttempt();
        QCOMPARE(attempt.tries.size(), 3);
        QCOMPARE(attempt.tries.at(0).outcome, StationConnectionAttempt::Outcome::NoAnswer);
        QCOMPARE(attempt.tries.at(1).outcome, StationConnectionAttempt::Outcome::NotThisCore);
        QCOMPARE(attempt.tries.at(2).outcome, StationConnectionAttempt::Outcome::Connected);
        QVERIFY(attempt.summary().contains(QLatin1String("no answer")));
        QVERIFY(attempt.summary().contains(QLatin1String("another computer answered")));
        // The other Core was told nothing: this computer sent no sign-in.
        QVERIFY(!other.server->hasAuthenticatedSession());
        QCOMPARE(window.lastEndReport().kind, StationEndReport::Kind::None);
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // A cached address that takes the connection and then never opens does
    // not hold a paired Core's race up (Task 29: the addresses are raced,
    // not tried one after another): the Core's own address wins, and the
    // silent one is recorded as not needed.
    void aCachedAddressThatNeverOpensDoesNotHoldTheRace()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("No TLS backend");
        }
        Core core;
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost, 0));
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        window.setCachedAddresses({QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(silent.serverPort()))});
        QCOMPARE(window.cachedAddressOpenTimeoutMs(), StationClient::kCachedAddressOpenTimeoutMs);
        // The race waits for no open time (that bound is the serial plan's,
        // for a Core trusted by its pin).
        window.setCachedAddressOpenTimeoutMs(60000);
        window.connectToStation(core.url(), QString(), QString(), false,
                                core.server->stationIdentity().fingerprint());
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, 20000, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        const StationConnectionAttempt attempt = window.connectionAttempt();
        QCOMPARE(attempt.tries.size(), 2);
        QCOMPARE(attempt.tries.at(0).outcome, StationConnectionAttempt::Outcome::AnotherPathFirst);
        QCOMPARE(attempt.tries.at(1).outcome, StationConnectionAttempt::Outcome::Connected);
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // Fix wave I1 (b): a Core saved with "connect without a certificate
    // fingerprint" has nothing that proves who answers, so its token never
    // goes to an address the operator did not type: no cached address is
    // dialled at all.
    void anUnpinnedCoreNeverDialsACachedAddress()
    {
        QTcpServer trap;
        QVERIFY(trap.listen(QHostAddress::LocalHost, 0));
        QSignalSpy trapped(&trap, &QTcpServer::newConnection);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setCachedAddresses({QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(trap.serverPort()))});
        QSignalSpy ended(&window, &StationClient::sessionEnded);
        const QUrl typed(QStringLiteral("wss://127.0.0.1:%1").arg(freeTcpPort()));
        window.connectToStation(typed, QStringLiteral("secret-token"), QString(),
                                /*allowUnpinned=*/true);
        QCOMPARE(window.connectionAttempt().tries.size(), 1);
        QCOMPARE(window.connectionAttempt().tries.at(0).address,
                 QStringLiteral("127.0.0.1:%1").arg(typed.port()));
        QTRY_VERIFY_WITH_TIMEOUT(!ended.isEmpty(), 10000);
        QCOMPARE(window.connectionAttempt().tries.size(), 1);
        QCOMPARE(trapped.size(), 0);
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // Fix wave I1 (c): a pinned Core whose cached address now reaches
    // another computer (another certificate) gives way to the next address
    // with the token unsent, as a paired Core does on identityChanged; the
    // other computer shares the token here, so a sent token would have
    // signed it in.
    void aPinMismatchAtACachedAddressGivesWayWithTheTokenUnsent()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("No TLS backend");
        }
        Core core(/*upgraded=*/true);
        Core other(/*upgraded=*/true, core.securityDir.path());
        QVERIFY(!core.server->token().isEmpty());
        QCOMPARE(other.server->token(), core.server->token());
        QVERIFY(core.server->certificateFingerprint() != other.server->certificateFingerprint());
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        QVERIFY(other.server->listen(QHostAddress::LocalHost, 0));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setCachedAddresses({other.url()});
        QSignalSpy endedEarly(&window, &StationClient::sessionEnded);
        window.connectToStation(core.url(), core.server->token(),
                                core.server->certificateFingerprint(), false);
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, 20000, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(window.connectedUrl(), core.url());
        const StationConnectionAttempt attempt = window.connectionAttempt();
        QCOMPARE(attempt.tries.size(), 2);
        QCOMPARE(attempt.tries.at(0).outcome, StationConnectionAttempt::Outcome::NotThisCore);
        QCOMPARE(attempt.tries.at(1).outcome, StationConnectionAttempt::Outcome::Connected);
        QVERIFY(!other.server->hasAuthenticatedSession());
        QCOMPARE(window.lastEndReport().kind, StationEndReport::Kind::None);
        // Giving way is not an end: nothing told the window it stopped.
        QCOMPARE(endedEarly.size(), 0);
        window.disconnectFromStation(QStringLiteral("test done"));

        // At the last address the plan has, a mismatch still ends the
        // attempt, with no retry, as before.
        RadioModel remoteAlone(RadioModel::Role::Remote);
        SettingsProxy proxyAlone;
        StationClient alone(&remoteAlone, &proxyAlone);
        QSignalSpy ended(&alone, &StationClient::sessionEnded);
        alone.connectToStation(other.url(), core.server->token(),
                               core.server->certificateFingerprint(), false);
        QTRY_VERIFY_WITH_TIMEOUT(!ended.isEmpty(), 10000);
        QCOMPARE(ended.first().first().toString(),
                 QStringLiteral("Station certificate fingerprint does not match the saved pin."));
        QVERIFY(!alone.isReconnectPending());
        QVERIFY(!other.server->hasAuthenticatedSession());
    }

    // The follow-up to the Task 27 re-review (new Minor 3): the other path
    // to the same refusal. The other computer's certificate is one the
    // trust store accepts (a certificate for 127.0.0.1 from an authority
    // added to it), so the handshake raises no TLS error and the mismatch
    // is caught when the socket connects (StationClient's connected()
    // handler), not in the sslErrors handler. It still gives way to the
    // next address with the token unsent.
    void aPinMismatchOnATrustedCertificateGivesWayWithTheTokenUnsent()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("No TLS backend");
        }
        Core core(/*upgraded=*/true);
        Core other(/*upgraded=*/true, core.securityDir.path());
        other.server.reset();
        const QSslCertificate trusted = writeLoopbackCertificate(other.securityDir.path());
        QVERIFY(!trusted.isNull());
        other.server = std::make_unique<StationServer>(other.model.get(), *other.settings,
                                                       other.securityDir.path());
        other.server->setHeartbeatIntervalMs(0);
        QCOMPARE(other.server->token(), core.server->token());
        QVERIFY(core.server->certificateFingerprint() != other.server->certificateFingerprint());
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        QVERIFY(other.server->listen(QHostAddress::LocalHost, 0));
        const TrustedAuthority authority(trusted);

        // The trust store accepts the other computer: a handshake to it
        // raises no TLS error.
        {
            QWebSocket probe;
            QSignalSpy errors(&probe, &QWebSocket::sslErrors);
            QSignalSpy opened(&probe, &QWebSocket::connected);
            probe.open(other.url());
            QTRY_VERIFY_WITH_TIMEOUT(!opened.isEmpty(), 10000);
            QCOMPARE(errors.size(), 0);
            probe.abort();
        }

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setCachedAddresses({other.url()});
        QSignalSpy endedEarly(&window, &StationClient::sessionEnded);
        window.connectToStation(core.url(), core.server->token(),
                                core.server->certificateFingerprint(), false);
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, 20000, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(window.connectedUrl(), core.url());
        const StationConnectionAttempt attempt = window.connectionAttempt();
        QCOMPARE(attempt.tries.size(), 2);
        QCOMPARE(attempt.tries.at(0).outcome, StationConnectionAttempt::Outcome::NotThisCore);
        QCOMPARE(attempt.tries.at(1).outcome, StationConnectionAttempt::Outcome::Connected);
        QVERIFY(!other.server->hasAuthenticatedSession());
        QCOMPARE(endedEarly.size(), 0);
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    void pathsAreNamedForTheOperator()
    {
        QCOMPARE(StationConnectionAttempt::pathFor(QUrl(QStringLiteral("wss://127.0.0.1:47910"))),
                 StationConnectionAttempt::Path::ThisNetwork);
        QCOMPARE(StationConnectionAttempt::pathFor(QUrl(QStringLiteral("wss://shack.local:47910"))),
                 StationConnectionAttempt::Path::ThisNetwork);
        QCOMPARE(StationConnectionAttempt::pathFor(QUrl(QStringLiteral("wss://shack.example.net:47910"))),
                 StationConnectionAttempt::Path::Direct);
        QCOMPARE(StationConnectionAttempt::pathFor(QUrl(QStringLiteral("wss://[2001:db8::7]:47910"))),
                 StationConnectionAttempt::Path::Direct);
        StationConnectionAttempt attempt;
        QCOMPARE(attempt.summary(), QString());
        attempt.tries.append({StationConnectionAttempt::Path::ThisNetwork,
                              QStringLiteral("192.168.1.20:47910"),
                              StationConnectionAttempt::Outcome::NoAnswer});
        attempt.tries.append({StationConnectionAttempt::Path::Direct,
                              QStringLiteral("shack.example.net:47910"),
                              StationConnectionAttempt::Outcome::TimedOut});
        attempt.tries.append({StationConnectionAttempt::Path::Relay, QStringLiteral("rv.nereussdr.com"),
                              StationConnectionAttempt::Outcome::Connected});
        QCOMPARE(attempt.summary(),
                 QStringLiteral("Tried this network (192.168.1.20:47910): no answer; direct "
                                "(shack.example.net:47910): no answer in time; relay "
                                "(rv.nereussdr.com): connected."));
    }

    // The saved Core keeps where it was reached, most recent first, at
    // most four, and a record from before them still loads.
    void theSavedCoreKeepsItsLastGoodAddresses()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = CoreTargetStore::createId();
        target.label = QStringLiteral("KG4VCF/shack");
        target.connection.url = QStringLiteral("wss://shack.example.net:47910");
        target.connection.identityFingerprint = QByteArray(32, '\x07');
        QVERIFY(store.upsert(target));
        QVERIFY(!settings.value(QStringLiteral("ConnectionTargets/V2")).toString().contains(
            QLatin1String("lastAddresses")));

        for (int port = 1; port <= 5; ++port) {
            QVERIFY(store.rememberAddress(target.id,
                                          QStringLiteral("wss://192.168.1.%1:47910").arg(port)));
        }
        QVERIFY(store.rememberAddress(target.id, QStringLiteral("wss://192.168.1.3:47910")));
        QVERIFY(!store.rememberAddress(target.id, QStringLiteral("http://192.168.1.9")));
        const QStringList expected{QStringLiteral("wss://192.168.1.3:47910"),
                                   QStringLiteral("wss://192.168.1.5:47910"),
                                   QStringLiteral("wss://192.168.1.4:47910"),
                                   QStringLiteral("wss://192.168.1.2:47910")};
        QCOMPARE(store.target(target.id)->connection.cachedAddresses, expected);

        CoreTargetStore reloaded(settings);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.target(target.id)->connection.cachedAddresses, expected);
        QCOMPARE(reloaded.target(target.id)->connection.url, target.connection.url);
    }

    // ── Task 28: the control session through the service ─────────────

    // A paired computer reaches the Core through the service: the Core
    // answers the introduction with a control connection presenting its own
    // certificate, the whole session runs over it, the introduction is
    // retired as the connection opens, and when the session ends both ends
    // give their relay allocations back.
    void aSessionRunsThroughTheServiceAndGivesTheRelayBack()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QSignalSpy introduced(rendezvous.client(), &RendezvousClient::introduced);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QSignalSpy ended(&window, &StationClient::sessionEnded);
        window.connectThroughService({service.url()}, rendezvous.client()->stationId(),
                                     core.server->stationIdentity().fingerprint());
        QVERIFY(window.isConnectionActive());
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(ended.size(), 0);
        QCOMPARE(introduced.size(), 1);
        QVERIFY(core.server->hasAuthenticatedSession());
        // Retired here, not at the service's 120 s.
        QCOMPARE(rendezvous.client()->liveIntroductions(), 0);
        QCOMPARE(rendezvous.pendingAnswers(), 0);
        // What the attempt record shows, and the settings media reuses.
        const StationConnectionAttempt attempt = window.connectionAttempt();
        QCOMPARE(attempt.tries.size(), 1);
        QCOMPARE(attempt.tries.at(0).outcome, StationConnectionAttempt::Outcome::Connected);
        // Media's settings: the control connection found a path on this
        // computer without the relay, so media takes no relay allocation at
        // either end (the Task 28 fix wave, Important 4); both control
        // allocations were made (ALLOCATED 2 below).
        // R-R3-49 load round: ICE can nominate the relay pair first and move
        // to the direct pair once its check succeeds (as
        // aDirectPathIsPreferredOverTheRelay waits for); on a loaded
        // computer the session opened on the relay in 2 of about 90 runs,
        // and the relay kept for media then was the rule working. Wait for
        // each end's control path to be the direct one the rule is about.
        const quint64 epoch = core.server->mediaSessionEpoch();
        QVERIFY(epoch != 0);
        // This fixture has one authenticated session. Read its current inner
        // channel, not the first DataChannelTransport child (an introduction
        // can leave another transport alive during retirement).
        const auto sessions = core.server->findChildren<SwitchableTransport*>(
            QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(sessions.size(), 1);
        const QPointer<SwitchableTransport> carrying = sessions.first();
        QVERIFY(qobject_cast<const DataChannelTransport*>(window.transport()) != nullptr);
        std::optional<MediaIcePath> windowPath;
        std::optional<MediaIcePath> corePath;
        std::optional<IceConfiguration> ice;
        std::optional<IceConfiguration> coreIce;
        const auto directWithoutOwnShim = [&] {
            const auto* windowChannel =
                qobject_cast<const DataChannelTransport*>(window.transport());
            const auto* coreChannel = carrying
                ? qobject_cast<const DataChannelTransport*>(carrying->inner()) : nullptr;
            windowPath = windowChannel ? windowChannel->selectedPath() : std::nullopt;
            corePath = coreChannel ? coreChannel->selectedPath() : std::nullopt;
            ice = window.sessionIceConfiguration();
            coreIce = core.server->sessionIceConfiguration(epoch);
            return core.server->mediaSessionEpoch() == epoch
                && windowPath && corePath && !windowPath->relayed() && !corePath->relayed()
                && !windowPath->viaLoopbackShim() && !corePath->viaLoopbackShim()
                && ice && coreIce && ice->relayServers().isEmpty()
                && coreIce->relayServers().isEmpty();
        };
        const auto pathState = [](const std::optional<MediaIcePath>& path) {
            return path ? QStringLiteral("%1/%2 remote=%3:%4 relay=%5 ownShim=%6")
                              .arg(path->localType, path->remoteType, path->remoteAddress)
                              .arg(path->remotePort).arg(path->relayed())
                              .arg(path->viaLoopbackShim())
                        : QStringLiteral("none");
        };
        QTRY_VERIFY2_WITH_TIMEOUT(directWithoutOwnShim(),
            qPrintable(QStringLiteral("window=%1 Core=%2 mediaTURN=%3/%4")
                .arg(pathState(windowPath), pathState(corePath))
                .arg(ice ? ice->relayServers().size() : -1)
                .arg(coreIce ? coreIce->relayServers().size() : -1)), 15000);
        QVERIFY(ice->relayKnown());
        QVERIFY(ice->relayAllowed());
        QVERIFY(ice->stunServer().has_value());
        QTRY_VERIFY_WITH_TIMEOUT(service.turnOutput().contains(QLatin1String("ALLOCATED 2")),
                                 10000);

        window.disconnectFromStation(QStringLiteral("test done"));
        QTRY_VERIFY_WITH_TIMEOUT(!core.server->hasAuthenticatedSession(), 10000);
        // Both allocations, the Core's and the computer's, given back at
        // once rather than held for their lifetime. On a failure, what the
        // relay saw: which allocations were made and which came back.
        {
            QString why;
            QVERIFY2(waitForRelayLine(service, QStringLiteral("RELEASED 2"), 15000, &why),
                     qPrintable(why));
        }
    }

    // Task 29 (R-IOS-16): a Core with `relay = deny` settles its relay
    // (none) when it gathers, so the session's media connection, which
    // starts from the same settings, gathers too. Before, the settings
    // stayed waiting for relay credentials that never come and media never
    // connected (found by the traversal harness's session-media).
    void aRelayDeniedCoresMediaSettingsAreSettled()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        window.connectThroughService({service.url()}, rendezvous.client()->stationId(),
                                     core.server->stationIdentity().fingerprint());
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        const quint64 epoch = core.server->mediaSessionEpoch();
        const std::optional<IceConfiguration> coreIce = core.server->sessionIceConfiguration(epoch);
        QVERIFY(coreIce.has_value());
        QVERIFY(coreIce->relayKnown());
        QVERIFY(!coreIce->relayAllowed());
        QCOMPARE(coreIce->relayServers().size(), 0);
        window.disconnectFromStation(QStringLiteral("test done"));
        QTRY_VERIFY_WITH_TIMEOUT(!core.server->hasAuthenticatedSession(), 10000);
    }

    // Task 28 tail (R-IOS-16), what a device sees while the Core registers
    // again: a session already open through the service rides its own
    // connection, so the Core leaving the service (its pongs stopped, as on
    // the Rock) and registering again leaves it signed in and answering.
    void aSessionOutlivesTheCoresRegisteringAgain()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        LossyLink link(service.port());
        Core core;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(core.pairComputer(*key));
        // The Core reaches the service through the lossy path; the device
        // straight.
        StationRendezvous rendezvous(core.server.get(), {link.url()}, /*relayAllowed=*/true);
        rendezvous.client()->setPingIntervalMs(400);
        rendezvous.client()->setReconnectDelaysMs({200});
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QSignalSpy lost(rendezvous.client(), &RendezvousClient::connectionLost);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QSignalSpy ended(&window, &StationClient::sessionEnded);
        QSignalSpy results(&window, &StationClient::commandResult);
        window.connectThroughService({service.url()}, rendezvous.client()->stationId(),
                                     core.server->stationIdentity().fingerprint());
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(window, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }

        // The Core's pongs stop: it leaves the service and registers again.
        link.setDropAllPongs(true);
        QTRY_VERIFY_WITH_TIMEOUT(lost.size() >= 1, 10000);
        link.setDropAllPongs(false);
        QTRY_VERIFY_WITH_TIMEOUT(registered.size() >= 2, 20000);

        // The session never noticed, and still answers.
        QCOMPARE(ended.size(), 0);
        QVERIFY(window.isHandshakeComplete());
        QVERIFY(core.server->hasAuthenticatedSession());
        const quint32 id = window.invokeCommand(QByteArrayLiteral("conformanceUnknownVerb"), {});
        QTRY_VERIFY_WITH_TIMEOUT(!results.isEmpty(), 10000);
        QCOMPARE(results.last().at(0).toUInt(), id);
        QCOMPARE(results.last().at(1).toBool(), false);
        window.disconnectFromStation(QStringLiteral("test done"));
        QTRY_VERIFY_WITH_TIMEOUT(!core.server->hasAuthenticatedSession(), 10000);
    }

    // The Core answered with the relay allowed and the credentials never
    // came: after the bound it gathers without the relay, and an answer
    // whose connection never opens is retired at its deadline.
    void anAnswerWithoutCredentialsGathersAndIsRetired()
    {
        ServicePlayer player;
        Core core;
        auto device = makeKey();
        QVERIFY(core.pair(*device));
        StationRendezvous rendezvous(core.server.get(), {player.url()}, /*relayAllowed=*/true);
        rendezvous.setCredentialsTimeoutMs(200);
        rendezvous.setAnswerDeadlineMs(1500);
        QVERIFY(rendezvous.start());
        QWebSocket* station = player.waitForConnection();
        QVERIFY(station != nullptr);
        const auto send = [station](const QJsonObject& message) {
            station->sendTextMessage(compact(message));
        };
        send({{"type", "hello"}, {"version", 1}, {"nonce", b64(randomBytes(32))},
              {"stun", QJsonArray()}});
        QVERIFY(player.waitForMessage(station).has_value());  // register
        send({{"type", "challenge"}, {"nonce", b64(randomBytes(32))}});
        QVERIFY(player.waitForMessage(station).has_value());  // prove
        const QString id = Wire::rendezvousId(core.server->stationIdentity().publicKeySpki());
        send({{"type", "registered"}, {"id", id}});
        QTRY_VERIFY(rendezvous.client()->isRegistered());

        // A real control offer, from a device's end.
        DataChannelTransport offerer;
        QSignalSpy offered(&offerer, &DataChannelTransport::localDescription);
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Offerer;
        options.maxIncomingBytes = StationClient::kMaxIncomingMessageBytes;
        options.ice = IceConfiguration::throughRendezvous({}, true, AddressFamilies{},
                                                          HostFamilies{});
        QVERIFY(offerer.start(options));
        QTRY_COMPARE(offered.size(), 1);
        const QByteArray nonce = randomBytes(32);
        const QByteArray intro = randomBytes(16);
        send({{"type", "introduction"}, {"from", b64(intro)},
              {"device", b64(StationIdentity::fingerprintOf(device->spki()))},
              {"deviceSignature", b64(device->sign(Wire::introduceTranscript(id, nonce)))},
              {"offer", offered.at(0).at(0).toString()}, {"nonce", b64(nonce)}});

        const std::optional<QString> answer = player.waitForMessage(station);
        QVERIFY(answer.has_value());
        const QJsonObject answerObject = QJsonDocument::fromJson(answer->toUtf8()).object();
        QCOMPARE(answerObject.value("type").toString(), QStringLiteral("answer"));
        QCOMPARE(answerObject.value("turn").toBool(), true);
        QCOMPARE(rendezvous.pendingAnswers(), 1);
        // No credentials: the Core gathers anyway and ends its candidates.
        bool ended = false;
        QDeadlineTimer deadline(5000);
        while (!ended && !deadline.hasExpired()) {
            const std::optional<QString> next = player.waitForMessage(station, 200);
            if (next) {
                const QJsonObject message = QJsonDocument::fromJson(next->toUtf8()).object();
                ended = message.value("type").toString() == QStringLiteral("candidate")
                    && message.value("candidate").toString().isEmpty();
            }
        }
        QVERIFY2(ended, "the Core never ended its candidates without the credentials");
        // Nothing connects (the device's end never hears the answer): the
        // introduction is retired at the deadline and its place is free.
        QTRY_COMPARE_WITH_TIMEOUT(rendezvous.pendingAnswers(), 0, 5000);
        QCOMPARE(rendezvous.client()->liveIntroductions(), 0);
        // The same introduction handed back is dropped.
        QSignalSpy introducedAgain(rendezvous.client(), &RendezvousClient::introduced);
        send({{"type", "introduction"}, {"from", b64(intro)},
              {"device", b64(StationIdentity::fingerprintOf(device->spki()))},
              {"deviceSignature", b64(device->sign(Wire::introduceTranscript(id, nonce)))},
              {"offer", offered.at(0).at(0).toString()}, {"nonce", b64(nonce)}});
        QVERIFY(player.silentFor(station, 300));
        QCOMPARE(introducedAgain.size(), 0);
        rendezvous.client()->stop();
    }

    // Task 28 fix wave (review Minor 1): the Core leaving the service, or
    // this computer losing its connection to it, ends a dial at once
    // rather than at kDialDeadlineMs (79 s).
    void aDialEndsAtOnceWhenItsIntroductionEnds_data()
    {
        QTest::addColumn<bool>("connectionLost");
        QTest::newRow("stationLeft") << false;
        QTest::newRow("connectionLost") << true;
    }

    void aDialEndsAtOnceWhenItsIntroductionEnds()
    {
        QFETCH(bool, connectionLost);
        ServicePlayer player;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        RendezvousDialer dialer;
        QSignalSpy failed(&dialer, &RendezvousDialer::failed);
        dialer.dial({player.url()}, QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaa"), key);
        QWebSocket* service = player.waitForConnection();
        QVERIFY(service != nullptr);
        service->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
                                                     {"nonce", b64(randomBytes(32))},
                                                     {"stun", QJsonArray()}}));
        const std::optional<QString> introduce = player.waitForMessage(service);
        QVERIFY(introduce.has_value());
        QCOMPARE(QJsonDocument::fromJson(introduce->toUtf8()).object().value("type").toString(),
                 QStringLiteral("introduce"));
        QElapsedTimer elapsed;
        elapsed.start();
        if (connectionLost) {
            service->close();
        } else {
            service->sendTextMessage(
                compact(QJsonObject{{"type", "introduction.end"}, {"code", "stationLeft"}}));
        }
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
        QVERIFY(elapsed.elapsed() < 5000);
        QCOMPARE(failed.at(0).at(0).toString(),
                 QStringLiteral("The Core could not be reached from here."));
        QVERIFY(NereusSDR::OperatorWording::isPlain(failed.at(0).at(0).toString()));
    }

    // Task 28 tail (re-review Minor): the service going (the Core left it,
    // or this computer lost its connection) fails a dial at once only while
    // the Core's answer or the end of its candidates is missing. Once both
    // have come, this computer holds everything the service would carry
    // from the Core; the Core learns this computer's address from its
    // checks, so ICE finishes without the service and the channel opens.
    // The fake service plays the Core with a DataChannelTransport of its
    // own, so the order of what reaches the dialer is the test's.
    void aDialAfterTheCoresAnswerAndCandidatesOutlivesTheService_data()
    {
        QTest::addColumn<bool>("connectionLost");
        QTest::addColumn<bool>("candidatesEnded");
        QTest::newRow("stationLeft, answer and candidates") << false << true;
        QTest::newRow("connectionLost, answer and candidates") << true << true;
        QTest::newRow("stationLeft, answer only") << false << false;
        QTest::newRow("connectionLost, answer only") << true << false;
    }

    void aDialAfterTheCoresAnswerAndCandidatesOutlivesTheService()
    {
        QFETCH(bool, connectionLost);
        QFETCH(bool, candidatesEnded);
        ServicePlayer player;
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        RendezvousDialer dialer;
        QSignalSpy failed(&dialer, &RendezvousDialer::failed);
        QSignalSpy ready(&dialer, &RendezvousDialer::ready);
        dialer.dial({player.url()}, QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaa"), key);
        QWebSocket* service = player.waitForConnection();
        QVERIFY(service != nullptr);
        service->sendTextMessage(compact(QJsonObject{{"type", "hello"}, {"version", 1},
                                                     {"nonce", b64(randomBytes(32))},
                                                     {"stun", QJsonArray()}}));
        const std::optional<QString> introduce = player.waitForMessage(service);
        QVERIFY(introduce.has_value());
        const QJsonObject introduceObject = QJsonDocument::fromJson(introduce->toUtf8()).object();
        QCOMPARE(introduceObject.value("type").toString(), QStringLiteral("introduce"));

        // The Core: its answer and every candidate, gathered first.
        DataChannelTransport core;
        QSignalSpy answered(&core, &DataChannelTransport::localDescription);
        QSignalSpy coreCandidates(&core, &DataChannelTransport::localCandidate);
        QSignalSpy gathered(&core, &DataChannelTransport::gatheringComplete);
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Answerer;
        options.maxIncomingBytes = StationServer::kMaxIncomingMessageBytes;
        options.ice = IceConfiguration::throughRendezvous(
            {}, false, IceConfiguration::localAddressFamilies(), HostFamilies{});
        QVERIFY(core.start(options));
        QVERIFY(core.acceptDescription(introduceObject.value("offer").toString(),
                                       QStringLiteral("offer")));
        QTRY_COMPARE(answered.size(), 1);
        QVERIFY(core.gatherCandidates(*options.ice));
        QTRY_COMPARE_WITH_TIMEOUT(gathered.size(), 1, 10000);
        QVERIFY(!coreCandidates.isEmpty());

        // In one burst: the answer, then (in one row) every candidate and
        // their end, then the service goes.
        Wire::Message answer;
        answer.kind = Wire::Kind::Answer;
        answer.sdp = answered.at(0).at(0).toString();
        service->sendTextMessage(QString::fromUtf8(
            Wire::encode(Wire::Direction::ServiceToClient, answer)));
        if (candidatesEnded) {
            QStringList candidates;
            for (const QList<QVariant>& emitted : std::as_const(coreCandidates)) {
                candidates.append(emitted.at(0).toString());
            }
            candidates.append(QString());
            for (const QString& candidate : std::as_const(candidates)) {
                Wire::Message message;
                message.kind = Wire::Kind::Candidate;
                message.candidate = candidate;
                const QByteArray wire = Wire::encode(Wire::Direction::ServiceToClient, message);
                QVERIFY(!wire.isEmpty());
                service->sendTextMessage(QString::fromUtf8(wire));
            }
        }
        QElapsedTimer elapsed;
        elapsed.start();
        if (connectionLost) {
            service->close();
        } else {
            service->sendTextMessage(
                compact(QJsonObject{{"type", "introduction.end"}, {"code", "stationLeft"}}));
        }
        if (!candidatesEnded) {
            QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
            QVERIFY(elapsed.elapsed() < 5000);
            QCOMPARE(ready.size(), 0);
            return;
        }
        // Nothing more goes through the service; the connection opens.
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 20000);
        QCOMPARE(failed.size(), 0);
        QVERIFY(core.isOpen());
        std::unique_ptr<DataChannelTransport> opened(
            qvariant_cast<DataChannelTransport*>(ready.at(0).at(0)));
        QVERIFY(opened != nullptr);
        QVERIFY(opened->isOpen());
    }

    // Task 28 fix wave (review Minor 1): the device leaving before its
    // connection opened frees the Core's answer at once (its peer
    // connection and any relay allocation), not at kAnswerDeadlineMs.
    void anUnopenedAnswerIsFreedWhenTheDeviceLeaves()
    {
        ServicePlayer player;
        Core core;
        auto device = makeKey();
        QVERIFY(core.pair(*device));
        StationRendezvous rendezvous(core.server.get(), {player.url()}, /*relayAllowed=*/false);
        QVERIFY(rendezvous.start());
        QWebSocket* station = player.waitForConnection();
        QVERIFY(station != nullptr);
        const auto send = [station](const QJsonObject& message) {
            station->sendTextMessage(compact(message));
        };
        send({{"type", "hello"}, {"version", 1}, {"nonce", b64(randomBytes(32))},
              {"stun", QJsonArray()}});
        QVERIFY(player.waitForMessage(station).has_value());  // register
        send({{"type", "challenge"}, {"nonce", b64(randomBytes(32))}});
        QVERIFY(player.waitForMessage(station).has_value());  // prove
        const QString id = Wire::rendezvousId(core.server->stationIdentity().publicKeySpki());
        send({{"type", "registered"}, {"id", id}});
        QTRY_VERIFY(rendezvous.client()->isRegistered());

        DataChannelTransport offerer;
        QSignalSpy offered(&offerer, &DataChannelTransport::localDescription);
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Offerer;
        options.maxIncomingBytes = StationClient::kMaxIncomingMessageBytes;
        options.ice = IceConfiguration::throughRendezvous({}, false, AddressFamilies{},
                                                          HostFamilies{});
        QVERIFY(offerer.start(options));
        QTRY_COMPARE(offered.size(), 1);
        const QByteArray nonce = randomBytes(32);
        const QByteArray intro = randomBytes(16);
        send({{"type", "introduction"}, {"from", b64(intro)},
              {"device", b64(StationIdentity::fingerprintOf(device->spki()))},
              {"deviceSignature", b64(device->sign(Wire::introduceTranscript(id, nonce)))},
              {"offer", offered.at(0).at(0).toString()}, {"nonce", b64(nonce)}});
        const std::optional<QString> answer = player.waitForMessage(station);
        QVERIFY(answer.has_value());
        QCOMPARE(QJsonDocument::fromJson(answer->toUtf8()).object().value("type").toString(),
                 QStringLiteral("answer"));
        QCOMPARE(rendezvous.pendingAnswers(), 1);

        send({{"type", "introduction.end"}, {"from", b64(intro)}, {"code", "clientLeft"}});
        QTRY_COMPARE_WITH_TIMEOUT(rendezvous.pendingAnswers(), 0, 2000);
        QCOMPARE(rendezvous.client()->liveIntroductions(), 0);
        rendezvous.client()->stop();
    }

    // A client with no paired Core, or no key, is told plainly and nothing
    // is dialled.
    void connectingThroughTheServiceNeedsAPairedCore()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        QSignalSpy ended(&window, &StationClient::sessionEnded);
        window.connectThroughService({QUrl(QStringLiteral("ws://127.0.0.1:9/"))},
                                     QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaa"),
                                     QByteArray(32, 'x'));
        QCOMPARE(ended.size(), 1);
        QVERIFY(!window.isConnectionActive());
        QVERIFY(NereusSDR::OperatorWording::isPlain(ended.at(0).at(0).toString()));
        // The dialer's own words, and a service that is not there.
        RendezvousDialer noKey;
        QSignalSpy noKeyFailed(&noKey, &RendezvousDialer::failed);
        noKey.dial({QUrl(QStringLiteral("ws://127.0.0.1:9/"))},
                   QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaa"), nullptr);
        QTRY_COMPARE(noKeyFailed.size(), 1);
        QVERIFY(NereusSDR::OperatorWording::isPlain(noKeyFailed.at(0).at(0).toString()));
        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        RendezvousDialer nowhere;
        QSignalSpy nowhereFailed(&nowhere, &RendezvousDialer::failed);
        nowhere.dial({QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(freeTcpPort()))},
                     QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaa"), key);
        QTRY_COMPARE_WITH_TIMEOUT(nowhereFailed.size(), 1, 15000);
        QVERIFY(NereusSDR::OperatorWording::isPlain(nowhereFailed.at(0).at(0).toString()));
    }

    void aClientTriesTheNextServerWhenTheCoreIsNotOnTheFirst()
    {
        LocalService first(/*stun=*/false, /*relay=*/false);
        LocalService second(/*stun=*/false, /*relay=*/false);
        QVERIFY2(first.start(), qPrintable(first.startFailure()));
        QVERIFY2(second.start(), qPrintable(second.startFailure()));
        Core core;
        auto phone = makeKey();
        QVERIFY(core.pair(*phone));
        // The Core is registered only with the second server; the first
        // answers `offline`, and the client moves on down its list.
        StationRendezvous rendezvous(core.server.get(), {second.url()}, true);
        // Task 27's ICE tests answer the introduction themselves (IcePair).
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QSignalSpy introduced(rendezvous.client(), &RendezvousClient::introduced);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        RendezvousClient client;
        // A server that is not running at all comes first of all.
        client.setServers({QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(freeTcpPort())),
                           first.url(), second.url()});
        client.introduce(rendezvous.client()->stationId(), phone->spki(),
                         [phone](const QByteArray& message) { return phone->sign(message); },
                         readText(kSuite + QStringLiteral("/sdp/offer.sdp")));
        QTRY_COMPARE_WITH_TIMEOUT(introduced.size(), 1, 20000);
        QCOMPARE(client.currentServer(), second.url());

        // Nowhere to be found: the client says so in plain words.
        RendezvousClient lonely;
        lonely.setServers({first.url()});
        QSignalSpy unreachable(&lonely, &RendezvousClient::unreachable);
        lonely.introduce(Wire::rendezvousId(makeKey()->spki()), phone->spki(),
                         [phone](const QByteArray& message) { return phone->sign(message); },
                         readText(kSuite + QStringLiteral("/sdp/offer.sdp")));
        QTRY_COMPARE_WITH_TIMEOUT(unreachable.size(), 1, 10000);
        QCOMPARE(unreachable.at(0).at(0).toString(),
                 QStringLiteral("The Core is not reachable right now. Check that it is running and "
                                "connected to the internet."));
    }
};

void TstRendezvousClient::runCoreFixture(const QString& file)
{
    const QJsonObject fixture = readJson(kSuite + QLatin1Char('/') + file);
    Context ctx;
    ctx.coreMode = true;
    ctx.setup(fixture.value(QStringLiteral("serverSetup")).toObject());
    ServicePlayer player;

    // The Core: its station identity key and the devices it paired before
    // the fixture starts, all made now.
    auto coreKey = makeKey();
    QHash<QByteArray, QByteArray> paired;
    for (const QJsonValue& name : fixture.value(QStringLiteral("pairedDevices")).toArray()) {
        const QByteArray spki = ctx.device(name.toString())->spki();
        paired.insert(StationIdentity::fingerprintOf(spki), spki);
    }
    RendezvousClient core;
    core.setServers({player.url()});
    QList<RendezvousIntroduction> introductions;
    connect(&core, &RendezvousClient::introduced, this,
            [&introductions](const RendezvousIntroduction& introduction) {
        introductions.append(introduction);
    });
    QWebSocket* station = nullptr;

    for (const QJsonValue& value : fixture.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject step = value.toObject();
        const QString where = file + QStringLiteral(": ") + compact(step).left(120);
        if (step.contains(QStringLiteral("connect"))) {
            if (step.value(QStringLiteral("connect")).toString() == QLatin1String("station")) {
                core.registerStation(coreKey->spki(),
                                     [coreKey](const QByteArray& message) { return coreKey->sign(message); },
                                     [&paired](const QByteArray& id) { return paired.value(id); });
                station = player.waitForConnection();
                QVERIFY2(station != nullptr, qPrintable(where));
            }
            continue;
        }
        if (step.contains(QStringLiteral("advanceMs"))) {
            ctx.advancedMs += step.value(QStringLiteral("advanceMs")).toInteger();
            continue;
        }
        if (step.contains(QStringLiteral("disconnect"))) {
            if (step.value(QStringLiteral("disconnect")).toString() == QLatin1String("station")) {
                core.stop();
            }
            continue;
        }
        if (step.contains(QStringLiteral("expectSilent"))) {
            QVERIFY2(station == nullptr || player.silentFor(station, 1000), qPrintable(where));
            continue;
        }
        if (step.contains(QStringLiteral("expectClosed"))) {
            if (step.value(QStringLiteral("expectClosed")).toString() == QLatin1String("station")
                && station != nullptr) {
                station->close(static_cast<QWebSocketProtocol::CloseCode>(
                    step.value(QStringLiteral("code")).toInt()));
                QVERIFY2(player.silentFor(station, 300), qPrintable(where));
            }
            continue;
        }
        const QString from = step.value(QStringLiteral("from")).toString();
        const QJsonObject message = step.value(QStringLiteral("message")).toObject();
        if (from == QLatin1String("server")) {
            const QJsonValue filled = ctx.fill(message);
            if (step.value(QStringLiteral("to")).toString() == QLatin1String("station")) {
                QVERIFY2(station != nullptr, qPrintable(where));
                station->sendTextMessage(compact(filled));
            }
            continue;
        }
        if (from != QLatin1String("station")
            || step.value(QStringLiteral("role")).toString() != QLatin1String("behaviour")) {
            // Another connection's message, or one a conformant Core never
            // sends: its placeholders are recorded only.
            ctx.fill(message);
            continue;
        }
        // Drive the Core to this behaviour, then match what it sends.
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("answer")) {
            QTRY_VERIFY2(!introductions.isEmpty(), qPrintable(where));
            QVERIFY2(core.answer(introductions.last().id, ctx.answerSdp), qPrintable(where));
        } else if (type == QLatin1String("candidate")) {
            QVERIFY2(!introductions.isEmpty(), qPrintable(where));
            QVERIFY2(core.sendCandidate(introductions.last().id,
                                        ctx.fill(message.value(QStringLiteral("candidate"))).toString()),
                     qPrintable(where));
        } else if (type == QLatin1String("nameplate.claim")) {
            core.claimNameplate();
        } else if (type == QLatin1String("nameplate.release")) {
            core.releaseNameplate();
        } else if (type == QLatin1String("mailbox")) {
            QTRY_VERIFY2(core.isMailboxOpen(), qPrintable(where));
            QVERIFY2(core.sendMailbox(message.value(QStringLiteral("body")).toString()), qPrintable(where));
        } else if (type == QLatin1String("mailbox.close")) {
            core.closeMailbox();
        }
        const std::optional<QString> sent = player.waitForMessage(station);
        QVERIFY2(sent.has_value(), qPrintable(where + QStringLiteral(": nothing sent")));
        QString why;
        const QJsonObject actual = QJsonDocument::fromJson(sent->toUtf8()).object();
        QVERIFY2(ctx.match(message, actual, &why), qPrintable(where + QStringLiteral(": ") + why));
    }
    // Nothing more from the Core.
    if (station != nullptr) {
        QVERIFY2(player.silentFor(station, 200), qPrintable(file));
    }
    core.stop();
}

void TstRendezvousClient::runAppFixture(const QString& file)
{
    const QJsonObject fixture = readJson(kSuite + QLatin1Char('/') + file);
    Context ctx;
    ctx.coreMode = false;
    ctx.setup(fixture.value(QStringLiteral("serverSetup")).toObject());
    ServicePlayer player;
    RendezvousClient client;
    client.setServers({player.url()});
    QSignalSpy connected(&client, &RendezvousClient::connected);
    QSignalSpy answers(&client, &RendezvousClient::answerReceived);
    QSignalSpy mailboxes(&client, &RendezvousClient::mailboxOpened);
    QSignalSpy closedMailboxes(&client, &RendezvousClient::mailboxClosed);
    QSignalSpy ends(&client, &RendezvousClient::introductionEnded);
    QSignalSpy errors(&client, &RendezvousClient::serviceError);
    QWebSocket* socket = nullptr;
    int expectedAnswers = 0;
    int expectedOpened = 0;
    int expectedClosed = 0;
    int expectedEnds = 0;
    int expectedErrors = 0;

    for (const QJsonValue& value : fixture.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject step = value.toObject();
        const QString where = file + QStringLiteral(": ") + compact(step).left(120);
        if (step.contains(QStringLiteral("connect"))) {
            if (step.value(QStringLiteral("connect")).toString() == QLatin1String("client")) {
                client.connectToService();
                socket = player.waitForConnection();
                QVERIFY2(socket != nullptr, qPrintable(where));
            }
            continue;
        }
        if (step.contains(QStringLiteral("advanceMs"))) {
            ctx.advancedMs += step.value(QStringLiteral("advanceMs")).toInteger();
            continue;
        }
        if (step.contains(QStringLiteral("disconnect"))) {
            if (step.value(QStringLiteral("disconnect")).toString() == QLatin1String("client")) {
                client.stop();
            }
            continue;
        }
        if (step.contains(QStringLiteral("expectSilent"))) {
            QVERIFY2(socket == nullptr || player.silentFor(socket, 1000), qPrintable(where));
            continue;
        }
        if (step.contains(QStringLiteral("expectClosed"))) {
            if (step.value(QStringLiteral("expectClosed")).toString() == QLatin1String("client")
                && socket != nullptr) {
                socket->close(static_cast<QWebSocketProtocol::CloseCode>(
                    step.value(QStringLiteral("code")).toInt()));
                QVERIFY2(player.silentFor(socket, 300), qPrintable(where));
            }
            continue;
        }
        const QString from = step.value(QStringLiteral("from")).toString();
        const QJsonObject message = step.value(QStringLiteral("message")).toObject();
        if (from == QLatin1String("server")) {
            const QJsonValue filled = ctx.fill(message);
            if (step.value(QStringLiteral("to")).toString() == QLatin1String("client")) {
                QVERIFY2(socket != nullptr, qPrintable(where));
                socket->sendTextMessage(compact(filled));
                const QString type = message.value(QStringLiteral("type")).toString();
                if (type == QLatin1String("hello")) {
                    QTRY_VERIFY2(!connected.isEmpty(), qPrintable(where));
                } else if (type == QLatin1String("answer")) {
                    ++expectedAnswers;
                } else if (type == QLatin1String("mailbox.opened")) {
                    ++expectedOpened;
                } else if (type == QLatin1String("mailbox.closed")) {
                    ++expectedClosed;
                } else if (type == QLatin1String("introduction.end")) {
                    ++expectedEnds;
                } else if (type == QLatin1String("error")) {
                    ++expectedErrors;
                }
            }
            continue;
        }
        if (from != QLatin1String("client")
            || step.value(QStringLiteral("role")).toString() != QLatin1String("behaviour")) {
            ctx.fill(message);
            continue;
        }
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("introduce")) {
            // The Core it paired with, and its own device key.
            const QString station = message.value(QStringLiteral("id")).toString().split(QLatin1Char(':')).value(1);
            const QString deviceName =
                message.value(QStringLiteral("device")).toString().split(QLatin1Char(':')).value(1);
            const std::shared_ptr<TestKey> device = ctx.device(deviceName);
            client.introduce(Wire::rendezvousId(ctx.spkiOf(station)), device->spki(),
                             [device](const QByteArray& m) { return device->sign(m); }, ctx.offerSdp);
        } else if (type == QLatin1String("mailbox.open")) {
            client.openMailbox(message.value(QStringLiteral("nameplate")).toInt());
        } else if (type == QLatin1String("mailbox")) {
            QTRY_VERIFY2(client.isMailboxOpen(), qPrintable(where));
            QVERIFY2(client.sendMailbox(message.value(QStringLiteral("body")).toString()), qPrintable(where));
        } else if (type == QLatin1String("mailbox.close")) {
            client.closeMailbox();
        } else if (type == QLatin1String("candidate")) {
            QVERIFY2(client.sendCandidate(ctx.fill(message.value(QStringLiteral("candidate"))).toString()),
                     qPrintable(where));
        }
        const std::optional<QString> sent = player.waitForMessage(socket);
        QVERIFY2(sent.has_value(), qPrintable(where + QStringLiteral(": nothing sent")));
        QString why;
        const QJsonObject actual = QJsonDocument::fromJson(sent->toUtf8()).object();
        QVERIFY2(ctx.match(message, actual, &why), qPrintable(where + QStringLiteral(": ") + why));
    }
    // The client read every message the service sent it.
    QTRY_COMPARE(answers.size(), expectedAnswers);
    QTRY_COMPARE(mailboxes.size(), expectedOpened);
    QTRY_COMPARE(closedMailboxes.size(), expectedClosed);
    QTRY_COMPARE(ends.size(), expectedEnds);
    QTRY_COMPARE(errors.size(), expectedErrors);
    if (socket != nullptr) {
        QVERIFY2(player.silentFor(socket, 200), qPrintable(file));
    }
    client.stop();
}

QTEST_GUILESS_MAIN(TstRendezvousClient)
#include "tst_rendezvous_client.moc"
