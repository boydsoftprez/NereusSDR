// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_device_auth.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 12 (R-IOS-08, R-IOS-02): device sign-in.
//
// Refusals first, then the admit path:
//
//   - a wrong signature, a device not in the store, a signature over
//     another connection's challenge, one binding another certificate, an
//     id that is not its key's fingerprint and a malformed block are each
//     refused with auth.result accepted:false, a plain reason and an end
//     code, and the connection closes;
//   - the rate limits: 10 failures in 60 s refuse that address, or that
//     id, for 60 s (an injected clock, no sleeps); over the relay (no
//     address) the limit is per id and per introduction; a failed proof
//     never counts against the id it names; a refusal while limited is
//     not counted again;
//   - the two limiters never meet: wrong tokens do not lock out a device's
//     key, and failed device sign-ins do not lock out the token;
//   - a new Core (no token) refuses token sign-in with "This Core uses
//     paired devices. Pair this device first." and creates no token;
//   - an upgraded Core is claimed through its token; a window signing in
//     with the token and its device block is enrolled (kind computer,
//     enrolledThroughToken) and signs in by key from then on with nothing
//     typed; a window that has not enrolled keeps using the token until it
//     is retired, and not after;
//   - a paired device is admitted, and lastSeen and lastAddress follow
//     each authenticated connection (empty over the relay);
//   - the hello carries the Core's key, a binding that verifies against
//     the certificate's SHA-256, and a fresh 32-byte challenge per
//     connection, and declares deviceAuth 1.
//
// Every device key is made at run time in a scratch directory.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 fix wave (R-IOS-16, review Minor
//               15): an introduced join must name its device. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/security/TokenStore.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kPairFirst = QStringLiteral("This Core uses paired devices. Pair this device first.");
const QString kNotPaired = QStringLiteral("This device is not paired with this Core. Pair it first.");
const QString kProofFailed =
    QStringLiteral("This device could not prove it is paired with this Core.");
const QString kDeviceLimited = QStringLiteral(
    "The Core is refusing sign-ins from this device for a while after too many failed ones. "
    "Try again later.");

// A device key made at run time.
struct Device {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());

    PairedDevice record(const QString& kind = QStringLiteral("phone")) const
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = QStringLiteral("Shack iPhone");
        device.kind = kind;
        return device;
    }

    // The block this device sends, signing the transcript of `challenge`
    // and `certSha256` for the Core whose key is `stationSpki`.
    DeviceAuthRequest request(const QByteArray& challenge, const QByteArray& certSha256,
                              const QByteArray& stationSpki,
                              const QString& address = QString()) const
    {
        DeviceAuthRequest r;
        r.id = StationIdentity::toBase64Url(key.fingerprint());
        r.publicKey = StationIdentity::toBase64Url(key.publicKeySpki());
        r.name = QStringLiteral("Shack iPhone");
        r.kind = QStringLiteral("phone");
        r.signature = StationIdentity::toBase64Url(key.sign(
            DeviceAuthenticator::transcript(challenge, certSha256, stationSpki,
                                            key.publicKeySpki())));
        r.sourceAddress = address;
        return r;
    }

    SessionDeviceBlock block(const QByteArray& challenge, const QByteArray& certSha256,
                             const QByteArray& stationSpki) const
    {
        const DeviceAuthRequest r = request(challenge, certSha256, stationSpki);
        return SessionDeviceBlock{r.id, r.publicKey, r.name, r.kind, r.signature};
    }
};

QByteArray sha256(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

// A challenge of the test's own: another connection's, as far as the Core
// can tell.
QByteArray randomChallenge()
{
    quint32 words[DeviceAuthenticator::kChallengeBytes / sizeof(quint32)]{};
    QRandomGenerator::system()->fillRange(words);
    return QByteArray(reinterpret_cast<const char*>(words), sizeof(words));
}

std::unique_ptr<RadioModel> makeStationModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

QJsonObject firstOfType(const QList<QByteArray>& received, const QString& type)
{
    for (const QByteArray& wire : received) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == type) {
            return o;
        }
    }
    return {};
}

// One Core over the loopback, in a scratch security directory.
struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;
    QList<LoopbackTransport*> clients;

    explicit Core(bool upgradedWithToken)
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        model = makeStationModel();
        const QString dir = upgradedWithToken
                                ? NereusSDR::Test::seedUpgradedCoreToken(securityDir.path())
                                : NereusSDR::Test::seedCoreIdentity(securityDir.path());
        server = std::make_unique<StationServer>(model.get(), *settings, dir);
        server->setHeartbeatIntervalMs(0);
    }

    ~Core()
    {
        server.reset();
        qDeleteAll(clients);
    }

    QByteArray certSha256() const
    {
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        return QByteArray::fromHex(pin.toLatin1());
    }

    QByteArray stationSpki() const { return server->stationIdentity().publicKeySpki(); }

    // Opens a connection from `address` (empty: through the relay) and
    // returns the app's end once the Core's hello has arrived.
    LoopbackTransport* open(const QString& address = QStringLiteral("192.0.2.7"))
    {
        auto* app = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("station"));
        station->setPeerAddress(address);
        station->linkTo(app);
        clients.append(app);
        server->acceptTransport(station);
        const bool greeted = QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000);
        Q_UNUSED(greeted);  // challengeOf() then reads nothing, and the check fails
        return app;
    }

    // A connection the remote access service introduced, as
    // StationRendezvous hands one over once its channel is open, reporting
    // `address` (empty when relayed).
    LoopbackTransport* openIntroduced(const QString& introductionId,
                                      const QString& address = QString(), bool waitForHello = true)
    {
        auto* app = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("introduced"));
        station->setPeerAddress(address);
        station->linkTo(app);
        clients.append(app);
        server->acceptIntroducedTransport(station, introductionId);
        if (waitForHello) {
            const bool greeted =
                QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000);
            Q_UNUSED(greeted);
        }
        return app;
    }

    static QByteArray challengeOf(LoopbackTransport* app)
    {
        const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
        return StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
    }

    // Sends hello and `auth`, and returns the auth.result.
    static QJsonObject signIn(LoopbackTransport* app, const SessionMessage& auth)
    {
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("NereusSDR iPhone"),
            {kSessionProtocolMajor}, {{"deviceAuth", 1}})));
        app->sendText(SessionMessages::encode(auth));
        const bool answered = QTest::qWaitFor(
            [app]() { return !firstOfType(app->received(), QStringLiteral("auth.result")).isEmpty(); },
            5000);
        Q_UNUSED(answered);  // an empty result fails verifyAdmitted/verifyRefusal
        return firstOfType(app->received(), QStringLiteral("auth.result"));
    }

    // A device sign-in on a fresh connection from `address`.
    QJsonObject deviceSignIn(const Device& device, const QString& address = QStringLiteral("192.0.2.7"))
    {
        LoopbackTransport* app = open(address);
        return signIn(app, SessionMessages::authRequest(
                               QString(), device.block(challengeOf(app), certSha256(), stationSpki())));
    }

    QJsonObject tokenSignIn(const QString& token, const QString& address = QStringLiteral("192.0.2.7"))
    {
        return signIn(open(address), SessionMessages::authRequest(token));
    }
};

void verifyRefusal(const QJsonObject& result, const QString& reason, const QString& code,
                   bool retryable = false)
{
    QVERIFY2(!result.isEmpty(), "no auth.result arrived");
    QCOMPARE(result.value(QStringLiteral("accepted")).toBool(true), false);
    QCOMPARE(result.value(QStringLiteral("reason")).toString(), reason);
    QCOMPARE(result.value(QStringLiteral("code")).toString(), code);
    QCOMPARE(result.value(QStringLiteral("retryable")).toBool(!retryable), retryable);
    QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
}

void verifyAdmitted(const QJsonObject& result)
{
    QVERIFY2(!result.isEmpty(), "no auth.result arrived");
    QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
             qPrintable(result.value(QStringLiteral("reason")).toString()));
}

} // namespace

class TstDeviceAuth : public QObject {
    Q_OBJECT

private slots:
    // ── The authenticator's refusals ─────────────────────────────────────

    void refusesEveryProofButTheRightOne()
    {
        QTemporaryDir coreDir;
        const StationIdentity core = StationIdentity::loadOrCreate(coreDir.path());
        DeviceStore store(coreDir.path());
        qint64 now = 0;
        DeviceAuthenticator auth(store, core, [&now]() { return now; });
        Device phone;
        Device stranger;
        QVERIFY(store.add(phone.record()));
        const QByteArray challenge = auth.newChallenge();
        QCOMPARE(challenge.size(), 32);
        const QByteArray cert = sha256(QByteArrayLiteral("certificate"));
        const QByteArray spki = core.publicKeySpki();

        // A signature over another connection's challenge.
        QCOMPARE(auth.verify(phone.request(randomChallenge(), cert, spki), challenge, cert).result,
                 AuthOutcome::Result::ProofFailed);
        // One binding another certificate.
        QCOMPARE(auth.verify(phone.request(challenge, sha256("another"), spki), challenge, cert)
                     .result,
                 AuthOutcome::Result::ProofFailed);
        // One for another Core.
        QTemporaryDir otherCoreDir;
        const StationIdentity otherCore = StationIdentity::loadOrCreate(otherCoreDir.path());
        QCOMPARE(auth.verify(phone.request(challenge, cert, otherCore.publicKeySpki()), challenge,
                             cert)
                     .result,
                 AuthOutcome::Result::ProofFailed);
        // A wrong signature.
        DeviceAuthRequest wrong = phone.request(challenge, cert, spki);
        wrong.signature = stranger.request(challenge, cert, spki).signature;
        QCOMPARE(auth.verify(wrong, challenge, cert).result, AuthOutcome::Result::ProofFailed);
        // The paired device's id carrying a stranger's key and signature.
        DeviceAuthRequest swapped = stranger.request(challenge, cert, spki);
        swapped.id = StationIdentity::toBase64Url(phone.key.fingerprint());
        QCOMPARE(auth.verify(swapped, challenge, cert).result, AuthOutcome::Result::ProofFailed);
        // Malformed text.
        DeviceAuthRequest malformed = phone.request(challenge, cert, spki);
        malformed.publicKey += QLatin1Char('=');
        QCOMPARE(auth.verify(malformed, challenge, cert).result, AuthOutcome::Result::ProofFailed);
        // A well-formed proof from a key the Core has not paired.
        QCOMPARE(auth.verify(stranger.request(challenge, cert, spki), challenge, cert).result,
                 AuthOutcome::Result::NotPaired);

        // And the one right answer.
        const AuthOutcome admitted = auth.verify(phone.request(challenge, cert, spki), challenge, cert);
        QVERIFY(admitted.admitted());
        QCOMPARE(admitted.deviceId, phone.key.fingerprint());
    }

    void possessionAloneIsWhatTokenEnrolmentAsks()
    {
        QTemporaryDir coreDir;
        const StationIdentity core = StationIdentity::loadOrCreate(coreDir.path());
        DeviceStore store(coreDir.path());
        DeviceAuthenticator auth(store, core);
        Device window;
        const QByteArray challenge = auth.newChallenge();
        const QByteArray cert = sha256(QByteArrayLiteral("certificate"));
        QCOMPARE(auth.verifyPossession(window.request(challenge, cert, core.publicKeySpki()),
                                       challenge, cert)
                     .result,
                 AuthOutcome::Result::Proved);
        QCOMPARE(auth.verifyPossession(window.request(randomChallenge(), cert, core.publicKeySpki()),
                                       challenge, cert)
                     .result,
                 AuthOutcome::Result::ProofFailed);
    }

    // ── Rate limits ──────────────────────────────────────────────────────

    void tenFailuresFromOneAddressRefuseItForSixtySeconds()
    {
        QTemporaryDir coreDir;
        const StationIdentity core = StationIdentity::loadOrCreate(coreDir.path());
        DeviceStore store(coreDir.path());
        qint64 now = 1000;
        DeviceAuthenticator auth(store, core, [&now]() { return now; });
        Device phone;
        QVERIFY(store.add(phone.record()));
        const QByteArray cert = sha256(QByteArrayLiteral("certificate"));
        const QByteArray challenge = auth.newChallenge();
        const QString address = QStringLiteral("192.0.2.66");

        // Ten strangers (ten ids) from one address.
        for (int i = 0; i < DeviceAuthenticator::kMaxFailures; ++i) {
            Device stranger;
            QCOMPARE(auth.verify(stranger.request(challenge, cert, core.publicKeySpki(), address),
                                 challenge, cert)
                         .result,
                     AuthOutcome::Result::NotPaired);
            now += 100;
        }
        // The paired device from that address is refused for now ...
        QCOMPARE(auth.verify(phone.request(challenge, cert, core.publicKeySpki(), address),
                             challenge, cert)
                     .result,
                 AuthOutcome::Result::RateLimited);
        // ... and not from another address.
        QVERIFY(auth.verify(phone.request(challenge, cert, core.publicKeySpki(),
                                          QStringLiteral("192.0.2.7")),
                            challenge, cert)
                    .admitted());
        // Refusals while limited are not counted: 59.9 s later still
        // refused, 60 s after the tenth failure admitted again.
        now += DeviceAuthenticator::kLockoutMs - 200;
        QCOMPARE(auth.verify(phone.request(challenge, cert, core.publicKeySpki(), address),
                             challenge, cert)
                     .result,
                 AuthOutcome::Result::RateLimited);
        now += 200;
        QVERIFY(auth.verify(phone.request(challenge, cert, core.publicKeySpki(), address),
                            challenge, cert)
                    .admitted());
    }

    // A proved key the Core has not paired, failing ten times from ten
    // addresses: that id is refused everywhere, and another is not.
    void tenFailuresForOneIdRefuseThatIdEverywhere()
    {
        QTemporaryDir coreDir;
        const StationIdentity core = StationIdentity::loadOrCreate(coreDir.path());
        DeviceStore store(coreDir.path());
        qint64 now = 0;
        DeviceAuthenticator auth(store, core, [&now]() { return now; });
        Device stranger;
        Device other;
        QVERIFY(store.add(other.record()));
        const QByteArray cert = sha256(QByteArrayLiteral("certificate"));
        const QByteArray challenge = auth.newChallenge();

        for (int i = 0; i < DeviceAuthenticator::kMaxFailures; ++i) {
            const DeviceAuthRequest unpaired =
                stranger.request(challenge, cert, core.publicKeySpki(),
                                 QStringLiteral("198.51.100.%1").arg(i + 1));
            QCOMPARE(auth.verify(unpaired, challenge, cert).result,
                     AuthOutcome::Result::NotPaired);
        }
        QCOMPARE(auth.verify(stranger.request(challenge, cert, core.publicKeySpki(),
                                              QStringLiteral("192.0.2.7")),
                             challenge, cert)
                     .result,
                 AuthOutcome::Result::RateLimited);
        // Another device is not affected.
        QVERIFY(auth.verify(other.request(challenge, cert, core.publicKeySpki(),
                                          QStringLiteral("192.0.2.7")),
                            challenge, cert)
                    .admitted());
    }

    // LINK minor 4: failed proofs that name a paired device's id, from many
    // addresses, never lock that device out. They did not come from its
    // key; anyone who knows the id could send them.
    void failedProofsNamingAnIdNeverLockItOut()
    {
        QTemporaryDir coreDir;
        const StationIdentity core = StationIdentity::loadOrCreate(coreDir.path());
        DeviceStore store(coreDir.path());
        qint64 now = 0;
        DeviceAuthenticator auth(store, core, [&now]() { return now; });
        Device phone;
        QVERIFY(store.add(phone.record()));
        const QByteArray cert = sha256(QByteArrayLiteral("certificate"));
        const QByteArray challenge = auth.newChallenge();

        for (int i = 0; i < 3 * DeviceAuthenticator::kMaxFailures; ++i) {
            DeviceAuthRequest bad =
                phone.request(randomChallenge(), cert, core.publicKeySpki(),
                              QStringLiteral("198.51.100.%1").arg(i + 1));
            QCOMPARE(auth.verify(bad, challenge, cert).result, AuthOutcome::Result::ProofFailed);
        }
        QVERIFY(auth.verify(phone.request(challenge, cert, core.publicKeySpki(),
                                          QStringLiteral("192.0.2.7")),
                            challenge, cert)
                    .admitted());
    }

    void failuresOlderThanTheWindowDoNotCount()
    {
        QTemporaryDir coreDir;
        const StationIdentity core = StationIdentity::loadOrCreate(coreDir.path());
        DeviceStore store(coreDir.path());
        qint64 now = 0;
        DeviceAuthenticator auth(store, core, [&now]() { return now; });
        Device phone;
        QVERIFY(store.add(phone.record()));
        const QByteArray cert = sha256(QByteArrayLiteral("certificate"));
        const QByteArray challenge = auth.newChallenge();
        const QString address = QStringLiteral("192.0.2.66");
        for (int i = 0; i < DeviceAuthenticator::kMaxFailures - 1; ++i) {
            Device stranger;
            auth.verify(stranger.request(challenge, cert, core.publicKeySpki(), address), challenge,
                        cert);
        }
        now += DeviceAuthenticator::kWindowMs;
        Device stranger;
        QCOMPARE(auth.verify(stranger.request(challenge, cert, core.publicKeySpki(), address),
                             challenge, cert)
                     .result,
                 AuthOutcome::Result::NotPaired);
        QVERIFY(auth.verify(phone.request(challenge, cert, core.publicKeySpki(), address), challenge,
                            cert)
                    .admitted());
    }

    void overTheRelayTheLimitIsPerIdAndPerIntroduction()
    {
        QTemporaryDir coreDir;
        const StationIdentity core = StationIdentity::loadOrCreate(coreDir.path());
        DeviceStore store(coreDir.path());
        qint64 now = 0;
        DeviceAuthenticator auth(store, core, [&now]() { return now; });
        Device phone;
        QVERIFY(store.add(phone.record()));
        const QByteArray cert = sha256(QByteArrayLiteral("certificate"));
        const QByteArray challenge = auth.newChallenge();

        // Ten strangers through one introduction, all with no address of
        // their own (the relay's is never used).
        for (int i = 0; i < DeviceAuthenticator::kMaxFailures; ++i) {
            Device stranger;
            DeviceAuthRequest r = stranger.request(challenge, cert, core.publicKeySpki());
            r.introduction = QStringLiteral("introduction-a");
            auth.verify(r, challenge, cert);
        }
        DeviceAuthRequest viaA = phone.request(challenge, cert, core.publicKeySpki());
        viaA.introduction = QStringLiteral("introduction-a");
        QCOMPARE(auth.verify(viaA, challenge, cert).result, AuthOutcome::Result::RateLimited);
        // Another introduction through the same relay is not refused: the
        // relay's shared address is not a key.
        DeviceAuthRequest viaB = phone.request(challenge, cert, core.publicKeySpki());
        viaB.introduction = QStringLiteral("introduction-b");
        QVERIFY(auth.verify(viaB, challenge, cert).admitted());
    }

    // ── On the link: refusals ────────────────────────────────────────────

    void theLinkRefusesEachBadSignIn()
    {
        Core core(false);
        Device phone;
        Device stranger;
        QVERIFY(core.server->deviceStore()->add(phone.record()));

        // Not paired.
        verifyRefusal(core.deviceSignIn(stranger), kNotPaired, QStringLiteral("deviceNotPaired"));

        // Signed over another connection's challenge.
        LoopbackTransport* first = core.open();
        LoopbackTransport* second = core.open();
        const QByteArray firstChallenge = Core::challengeOf(first);
        QVERIFY(firstChallenge != Core::challengeOf(second));
        verifyRefusal(Core::signIn(second, SessionMessages::authRequest(
                                               QString(), phone.block(firstChallenge, core.certSha256(),
                                                                      core.stationSpki()))),
                      kProofFailed, QStringLiteral("deviceProofFailed"));

        // Binding another certificate (a device shown someone else's).
        LoopbackTransport* third = core.open();
        verifyRefusal(Core::signIn(third, SessionMessages::authRequest(
                                              QString(), phone.block(Core::challengeOf(third),
                                                                     sha256("another certificate"),
                                                                     core.stationSpki()))),
                      kProofFailed, QStringLiteral("deviceProofFailed"));

        // A wrong signature.
        LoopbackTransport* fourth = core.open();
        SessionDeviceBlock wrong =
            phone.block(Core::challengeOf(fourth), core.certSha256(), core.stationSpki());
        wrong.signature = stranger.block(Core::challengeOf(fourth), core.certSha256(),
                                         core.stationSpki())
                              .signature;
        verifyRefusal(Core::signIn(fourth, SessionMessages::authRequest(QString(), wrong)),
                      kProofFailed, QStringLiteral("deviceProofFailed"));

        // Every refused connection closed without a session.
        QTRY_VERIFY(!second->isOpen() && !third->isOpen() && !fourth->isOpen());
        QVERIFY(!core.server->hasAuthenticatedSession());
    }

    void aNewCoreRefusesTokenSignInAndCreatesNoToken()
    {
        Core core(false);
        QVERIFY(core.server->token().isEmpty());
        QVERIFY(!core.server->deviceStore()->isClaimed());
        verifyRefusal(core.tokenSignIn(QStringLiteral("a-token-from-somewhere")), kPairFirst,
                      QStringLiteral("pairingRequired"));
        verifyRefusal(core.tokenSignIn(QString()), kPairFirst, QStringLiteral("pairingRequired"));
        QVERIFY(!QFile::exists(QDir(core.securityDir.path()).filePath(QStringLiteral("station-token"))));
    }

    void wrongTokensNeverLockOutADeviceKey()
    {
        Core core(true);
        core.server->setAuthRateLimit(5, 60000);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        for (int i = 0; i < 5; ++i) {
            verifyRefusal(core.tokenSignIn(QStringLiteral("wrong")),
                          QStringLiteral("The Core did not accept this app's pairing token. Check the token saved for this Core."),
                          QStringLiteral("wrongToken"));
        }
        // The token is locked out now, even the right one ...
        const QJsonObject locked = core.tokenSignIn(core.server->token());
        QCOMPARE(locked.value(QStringLiteral("accepted")).toBool(true), false);
        QCOMPARE(locked.value(QStringLiteral("retryable")).toBool(false), true);
        // ... and the device's key signs in from the same address.
        verifyAdmitted(core.deviceSignIn(phone));
    }

    void failedDeviceSignInsNeverLockOutTheToken()
    {
        Core core(true);
        for (int i = 0; i < DeviceAuthenticator::kMaxFailures + 2; ++i) {
            Device stranger;
            core.deviceSignIn(stranger);
        }
        Device stranger;
        verifyRefusal(core.deviceSignIn(stranger), kDeviceLimited, QString(), /*retryable=*/true);
        verifyAdmitted(core.tokenSignIn(core.server->token()));
    }

    // ── Through the remote access service (Task 28 fix wave) ─────────────

    // Review Important 1: a connection the service introduced signs in by
    // a paired device's own key only (the link document, section 20).
    void aConnectionThroughTheServiceRefusesTheToken()
    {
        Core core(true);
        const QString token = core.server->token();
        const QString reason = QStringLiteral(
            "Through the remote access service, a device signs in with its own key. Pair this "
            "device with the Core first.");
        verifyRefusal(Core::signIn(core.openIntroduced(QStringLiteral("intro-1")),
                                   SessionMessages::authRequest(token)),
                      reason, QStringLiteral("protocolError"));
        // Nor may a token enrol a key there.
        Device window;
        LoopbackTransport* app = core.openIntroduced(QStringLiteral("intro-2"), QStringLiteral("192.0.2.9"));
        verifyRefusal(Core::signIn(app, SessionMessages::authRequest(
                                            token, window.block(Core::challengeOf(app),
                                                                core.certSha256(),
                                                                core.stationSpki()))),
                      reason, QStringLiteral("protocolError"));
        QVERIFY(core.server->deviceStore()->list().isEmpty());
        // The same token on a direct connection still signs in.
        verifyAdmitted(core.tokenSignIn(token));
    }

    // Task 29 fix wave (review Minor 15): a connection the service
    // introduced joins a session only when it names the paired device it
    // was introduced for; one that names none (never in production) joins
    // nothing, the named one moves the session.
    void anIntroducedJoinMustNameItsDevice()
    {
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.open(QStringLiteral("192.168.1.20"));
        verifyAdmitted(Core::signIn(
            app, SessionMessages::authRequest(
                     QString(), phone.block(Core::challengeOf(app), core.certSha256(),
                                            core.stationSpki()))));
        QTRY_VERIFY(app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
        const auto ticketFor = [app](quint32 id) {
            app->sendText(SessionMessages::encode(
                SessionMessages::commandInvoke(QByteArrayLiteral("session.pathTicket"), id, {})));
            QString ticket;
            const bool arrived = QTest::qWaitFor([app, id, &ticket] {
                for (const QByteArray& wire : app->received()) {
                    const QJsonObject o = QJsonDocument::fromJson(wire).object();
                    if (o.value(QStringLiteral("type")).toString() == QLatin1String("command.result")
                        && o.value(QStringLiteral("id")).toInteger() == id) {
                        for (const QJsonValue& v : o.value(QStringLiteral("values")).toArray()) {
                            if (v.toObject().value(QStringLiteral("name")).toString()
                                == QLatin1String("ticket")) {
                                ticket = v.toObject().value(QStringLiteral("value")).toString();
                            }
                        }
                        return true;
                    }
                }
                return false;
            }, 5000);
            Q_UNUSED(arrived);
            return ticket;
        };
        const auto join = [](LoopbackTransport* link, const QString& ticket) {
            link->sendText(SessionMessages::encode(SessionMessages::hello(
                kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("NereusSDR iPhone"),
                {kSessionProtocolMajor}, {{"deviceAuth", 1}})));
            link->sendText(SessionMessages::encode(SessionMessages::pathJoin(ticket)));
        };

        const QString first = ticketFor(1);
        QVERIFY(!first.isEmpty());
        LoopbackTransport* unnamed = core.openIntroduced(QStringLiteral("intro-a"));
        join(unnamed, first);
        QTRY_VERIFY(!firstOfType(unnamed->received(), QStringLiteral("session.end")).isEmpty());
        QCOMPARE(core.server->sessionsMoved(), 0);
        QVERIFY(app->isOpen());

        const QString second = ticketFor(2);
        QVERIFY(!second.isEmpty());
        auto* named = new LoopbackTransport(QStringLiteral("app named"));
        auto* station = new LoopbackTransport(QStringLiteral("introduced named"));
        station->linkTo(named);
        core.clients.append(named);
        core.server->acceptIntroducedTransport(station, QStringLiteral("intro-b"),
                                               phone.key.fingerprint());
        QTRY_VERIFY(!named->received().isEmpty());
        join(named, second);
        QTRY_COMPARE(core.server->sessionsMoved(), 1);
    }

    void aPairedDeviceSignsInThroughTheService()
    {
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        LoopbackTransport* app = core.openIntroduced(QStringLiteral("intro-1"));
        verifyAdmitted(Core::signIn(
            app, SessionMessages::authRequest(
                     QString(), phone.block(Core::challengeOf(app), core.certSha256(),
                                            core.stationSpki()))));
        QTRY_VERIFY(core.server->hasAuthenticatedSession());
    }

    // Review Important 2: every connection the service introduced that is
    // still connecting is one source, capped as one address is, whatever
    // address each reports; home-network devices still get in.
    void theServiceHoldsNoMoreConnectingPlacesThanOneAddress()
    {
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        QList<LoopbackTransport*> held;
        for (int i = 0; i < StationServer::kMaxHandshakesPerAddress; ++i) {
            held.append(core.openIntroduced(QStringLiteral("replayed-%1").arg(i),
                                            i == 0 ? QString() : QStringLiteral("198.51.100.%1").arg(i)));
            QVERIFY(held.last()->isOpen());
        }
        // The service replays more: each is turned away at once, retryably.
        for (int i = 0; i < 6; ++i) {
            LoopbackTransport* refused = core.openIntroduced(
                QStringLiteral("replayed-more-%1").arg(i),
                i % 2 == 0 ? QString() : QStringLiteral("203.0.113.%1").arg(i), false);
            QTRY_VERIFY(!refused->isOpen());
            const QJsonObject end = firstOfType(refused->received(), QStringLiteral("session.end"));
            QCOMPARE(end.value(QStringLiteral("retryable")).toBool(false), true);
            QCOMPARE(end.value(QStringLiteral("reason")).toString(),
                     QStringLiteral("The Core already has as many connections as it allows. Try "
                                    "again shortly."));
        }
        // The held ones are still there, and a device on the home network
        // signs in beside them.
        for (LoopbackTransport* app : std::as_const(held)) {
            QVERIFY(app->isOpen());
        }
        verifyAdmitted(core.deviceSignIn(phone, QStringLiteral("192.168.1.20")));
        // Once one of the service's connections signs in, it no longer
        // holds a connecting place, and the next introduction is let in.
        LoopbackTransport* first = held.first();
        verifyAdmitted(Core::signIn(
            first, SessionMessages::authRequest(
                       QString(), phone.block(Core::challengeOf(first), core.certSha256(),
                                              core.stationSpki()))));
        QTRY_VERIFY(first->receivedKinds().contains(QByteArrayLiteral("snapshot.complete")));
        LoopbackTransport* next = core.openIntroduced(QStringLiteral("after"));
        QVERIFY(next->isOpen());
        QVERIFY(firstOfType(next->received(), QStringLiteral("session.end")).isEmpty());
    }

    // Review Important 2: the sign-in limits key on the introduction, so
    // failures through one introduction refuse that introduction, and
    // another is not refused for them.
    void failuresThroughOneIntroductionLimitThatIntroduction()
    {
        Core core(false);
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        const auto signInThrough = [&core](const Device& device, const QString& introduction) {
            LoopbackTransport* app = core.openIntroduced(introduction);
            return Core::signIn(app, SessionMessages::authRequest(
                                         QString(), device.block(Core::challengeOf(app),
                                                                 core.certSha256(),
                                                                 core.stationSpki())));
        };
        for (int i = 0; i < DeviceAuthenticator::kMaxFailures; ++i) {
            Device stranger;
            verifyRefusal(signInThrough(stranger, QStringLiteral("introduction-a")), kNotPaired,
                          QStringLiteral("deviceNotPaired"));
        }
        verifyRefusal(signInThrough(phone, QStringLiteral("introduction-a")), kDeviceLimited,
                      QString(), /*retryable=*/true);
        verifyAdmitted(signInThrough(phone, QStringLiteral("introduction-b")));
    }

    // ── On the link: the admit path ──────────────────────────────────────

    void theHelloCarriesTheIdentityItsBindingAndAChallenge()
    {
        Core core(false);
        LoopbackTransport* app = core.open();
        const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
        // With pairing (iPhone app Task 14) beside device sign-in.
        QCOMPARE(hello.value(QStringLiteral("features")).toObject(),
                 (QJsonObject{{QStringLiteral("deviceAuth"), 1},
                              {QStringLiteral("pairing"), 1},
                              {QStringLiteral("radioMic"), 2},
                              // iPhone app Task 71: several devices at once.
                              {QStringLiteral("sessionHolder"), 1}}));
        const QJsonObject identity = hello.value(QStringLiteral("identity")).toObject();
        const QByteArray spki =
            StationIdentity::fromBase64Url(identity.value(QStringLiteral("publicKey")).toString());
        QCOMPARE(spki, core.stationSpki());
        const QByteArray binding =
            StationIdentity::fromBase64Url(identity.value(QStringLiteral("certBinding")).toString());
        QVERIFY(StationIdentity::verify(spki, StationIdentity::certBindingMessage(core.certSha256()),
                                        binding));
        QCOMPARE(Core::challengeOf(app).size(), 32);
    }

    void aPairedDeviceSignsInAndIsSeen()
    {
        Core core(false);
        QDateTime clock(QDate(2026, 9, 24), QTime(12, 0), QTimeZone::UTC);
        core.server->deviceStore()->setClock([&clock]() { return clock; });
        Device phone;
        QVERIFY(core.server->deviceStore()->add(phone.record()));
        QVERIFY(core.server->deviceStore()->isClaimed());

        clock = clock.addSecs(60);
        verifyAdmitted(core.deviceSignIn(phone, QStringLiteral("192.0.2.7")));
        QTRY_VERIFY(core.server->hasAuthenticatedSession());
        auto seen = core.server->deviceStore()->find(phone.key.fingerprint());
        QCOMPARE(seen->lastSeen, clock);
        QCOMPARE(seen->lastAddress, QStringLiteral("192.0.2.7"));

        // Again, over the relay: no address of its own.
        clock = clock.addSecs(60);
        verifyAdmitted(core.deviceSignIn(phone, QString()));
        seen = core.server->deviceStore()->find(phone.key.fingerprint());
        QCOMPARE(seen->lastSeen, clock);
        QVERIFY(seen->lastAddress.isEmpty());
    }

    void anUpgradedCoreEnrolsEachWindowThroughItsToken()
    {
        Core core(true);
        DeviceStore* devices = core.server->deviceStore();
        QVERIFY(devices->list().isEmpty());
        // Claimed through the token, so no stranger can claim it.
        QVERIFY(devices->isClaimed());
        const QString token = core.server->token();
        QCOMPARE(token.size(), 43);

        // A window from before paired devices: token only, nothing enrolled.
        verifyAdmitted(core.tokenSignIn(token));
        QVERIFY(devices->list().isEmpty());

        // A window that sends its device block with the token is enrolled
        // in the same step.
        Device window;
        LoopbackTransport* app = core.open(QStringLiteral("192.0.2.9"));
        SessionDeviceBlock block =
            window.block(Core::challengeOf(app), core.certSha256(), core.stationSpki());
        block.name = QStringLiteral("Shack Mac");
        block.kind = QStringLiteral("phone");  // enrolled as a computer whatever it says
        verifyAdmitted(Core::signIn(app, SessionMessages::authRequest(token, block)));
        const auto enrolled = devices->find(window.key.fingerprint());
        QVERIFY(enrolled.has_value());
        QCOMPARE(enrolled->kind, QStringLiteral("computer"));
        QCOMPARE(enrolled->name, QStringLiteral("Shack Mac"));
        QVERIFY(enrolled->enrolledThroughToken);
        QCOMPARE(enrolled->lastAddress, QStringLiteral("192.0.2.9"));

        // From then on it signs in by key, typing nothing.
        verifyAdmitted(core.deviceSignIn(window));

        // A token sign-in whose device block does not prove its key is
        // refused, and enrols nothing.
        Device impostor;
        LoopbackTransport* bad = core.open();
        verifyRefusal(Core::signIn(bad, SessionMessages::authRequest(
                                            token, impostor.block(randomChallenge(), core.certSha256(),
                                                                  core.stationSpki()))),
                      kProofFailed, QStringLiteral("deviceProofFailed"));
        QVERIFY(!devices->find(impostor.key.fingerprint()).has_value());

        // The token works until it is retired, and not after.
        verifyAdmitted(core.tokenSignIn(token));
        // Retired (the console, or Task 13's Remote Access page), then the
        // Core's next start.
        core.server.reset();
        TokenStore tokens(core.securityDir.path());
        QVERIFY(tokens.retire());
        StationServer restarted(core.model.get(), *core.settings, core.securityDir.path());
        restarted.setHeartbeatIntervalMs(0);
        QVERIFY(restarted.token().isEmpty());
        QVERIFY(restarted.deviceStore()->isClaimed());  // its enrolled window
        auto* station = new LoopbackTransport(QStringLiteral("station"));
        auto* retiredApp = new LoopbackTransport(QStringLiteral("app"));
        station->setPeerAddress(QStringLiteral("192.0.2.7"));
        station->linkTo(retiredApp);
        restarted.acceptTransport(station);
        QVERIFY(QTest::qWaitFor([retiredApp]() { return !retiredApp->received().isEmpty(); }, 5000));
        verifyRefusal(Core::signIn(retiredApp, SessionMessages::authRequest(token)), kPairFirst,
                      QStringLiteral("pairingRequired"));
        delete retiredApp;
    }
};

QTEST_GUILESS_MAIN(TstDeviceAuth)
#include "tst_device_auth.moc"
