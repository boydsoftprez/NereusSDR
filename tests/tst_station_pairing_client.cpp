// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_pairing_client.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 18 (R-IOS-08): the desktop pairs with a Core.
//
// The desktop's own key first: device-identity.pem, P-256, mode 0600,
// created once and reused, a damaged file refused and left alone; the name
// the Core lists this computer by.
//
// Then refusals, each before any admit path: a code that is not a number
// and two words of the list fails at once and is never sent (so it cannot
// burn the Core's code); a wrong code fails with the Core's reason and
// burns the code; one tap on a claimed Core fails with the Core's reason;
// a Core that does not pair fails; a Core identity whose certificate
// binding does not verify for the connection's certificate (or with no
// certificate at all) is not kept.
//
// Then the admit paths against a real StationServer over the loopback:
// one tap on an unclaimed Core, and the right code on a reopened one, each
// end in paired() with the Core's identity and label, and the Core lists
// this computer as kind `computer` under its name. The desktop then signs
// in by key (StationClient) on a new connection. Both at 127.0.0.1 and at
// ::1, and the typed address forms (a name, IPv4, IPv6 with and without
// brackets and ports).
//
// Keys and codes are made at run time; a code is never printed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QFile>
#include <QFileDevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QHostAddress>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingCode.h"
#include "core/security/PairingWindow.h"
#include "core/security/SpakeExchange.h"
#include "core/security/StationIdentity.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/StationPairingClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "fakes/LoginProxy.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kDeviceName = QStringLiteral("Shack MacBook");
const QString kShortName = QStringLiteral("MacBook");

// One Core over the loopback, as tst_station_pairing stands it up.
struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;
    QList<QUrl> dialled;
    int opened = 0;

    Core()
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        settings->setValue(QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"));
        model = std::make_unique<RadioModel>();
        server = std::make_unique<StationServer>(
            model.get(), *settings, NereusSDR::Test::seedCoreIdentity(securityDir.path()));
        server->setHeartbeatIntervalMs(0);
    }

    ~Core() { server.reset(); }

    QByteArray certSha256() const
    {
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        return QByteArray::fromHex(pin.toLatin1());
    }

    // A connection the way this computer would see it: the Core's own
    // certificate unless `certificate` names another, and `address` as the
    // Core sees this computer.
    LoopbackTransport* open(const QString& address, const QByteArray& certificate)
    {
        auto* app = new LoopbackTransport(QStringLiteral("desktop"));
        auto* station = new LoopbackTransport(QStringLiteral("core"));
        station->setPeerAddress(address);
        app->setPeerCertificateSha256(certificate);
        station->linkTo(app);
        server->acceptTransport(station);
        ++opened;
        return app;
    }

    // What the pairing client dials: the loopback to this Core.
    StationPairingClient::TransportFactory factory(
        const QString& address = QStringLiteral("127.0.0.1"),
        std::optional<QByteArray> certificate = std::nullopt)
    {
        return [this, address, certificate](const QUrl& url) -> SessionTransport* {
            dialled.append(url);
            return open(address, certificate.value_or(certSha256()));
        };
    }
};

std::shared_ptr<const ClientDeviceIdentity> makeKey(const QTemporaryDir& dir)
{
    return std::make_shared<const ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(dir.path()));
}

// A code of the list that is not `code`.
QString otherCode(const QString& code)
{
    const QStringList parts = code.split(QLatin1Char('-'));
    const QStringList& words = PairingCode::wordList();
    for (const QString& word : words) {
        if (word != parts.value(1)) {
            return parts.value(0) + QLatin1Char('-') + word + QLatin1Char('-') + parts.value(2);
        }
    }
    return {};
}

} // namespace

class TstStationPairingClient : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<PairedStationRecord>();
        QVERIFY(SpakeExchange::isAvailable());
    }

    // ── This computer's key ─────────────────────────────────────────────

    void deviceKeyIsCreatedOnceOwnerOnlyAndReused()
    {
        QTemporaryDir dir;
        const ClientDeviceIdentity first = ClientDeviceIdentity::loadOrCreate(dir.path());
        QVERIFY(first.isValid());
        QVERIFY(first.wasCreatedThisRun());
        QCOMPARE(first.keyPath(), dir.filePath(QStringLiteral("device-identity.pem")));
        QCOMPARE(first.publicKeySpki().size(), StationIdentity::kSpkiBytes);
        QCOMPARE(first.fingerprint().size(), 32);
#ifndef Q_OS_WIN
        QCOMPARE(QFile::permissions(first.keyPath())
                     & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther
                        | QFileDevice::WriteOther | QFileDevice::ExeOwner),
                 QFileDevice::Permissions());
#endif
        // The Core's own key file is not this one.
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("station-identity.pem"))));

        const ClientDeviceIdentity again = ClientDeviceIdentity::loadOrCreate(dir.path());
        QVERIFY(again.isValid());
        QVERIFY(!again.wasCreatedThisRun());
        QCOMPARE(again.publicKeySpki(), first.publicKeySpki());
        const QByteArray message("sign me");
        QVERIFY(StationIdentity::verify(first.publicKeySpki(), message, again.sign(message)));
    }

    void damagedDeviceKeyIsRefusedAndLeftAlone()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("device-identity.pem"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a key");
        file.close();
        const ClientDeviceIdentity damaged = ClientDeviceIdentity::loadOrCreate(dir.path());
        QVERIFY(!damaged.isValid());
        QVERIFY(!damaged.lastError().isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("not a key"));

        // A pairing without a usable key says so and dials nothing.
        Core core;
        StationPairingClient client(std::make_shared<const ClientDeviceIdentity>(damaged),
                                    kDeviceName);
        client.setTransportFactory(core.factory());
        QSignalSpy failed(&client, &StationPairingClient::failed);
        client.pairOnThisNetwork(QStringLiteral("127.0.0.1"), 47910);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(core.opened, 0);
    }

    void theCoreListsThisComputerByTheMachinesName()
    {
        QCOMPARE(ClientDeviceIdentity::deviceNameFrom(QStringLiteral("Shack-MacBook.local")),
                 QStringLiteral("Shack-MacBook"));
        QCOMPARE(ClientDeviceIdentity::deviceNameFrom(QStringLiteral("  bench\x01pc  ")),
                 QStringLiteral("benchpc"));
        QCOMPARE(ClientDeviceIdentity::deviceNameFrom(QString()), QStringLiteral("Computer"));
        QCOMPARE(ClientDeviceIdentity::deviceNameFrom(QStringLiteral(".local")),
                 QStringLiteral("Computer"));
        const QString longName = ClientDeviceIdentity::deviceNameFrom(QString(100, QChar(0x00E9)));
        QVERIFY(longName.toUtf8().size() <= ClientDeviceIdentity::kMaxNameBytes);
        QVERIFY(DeviceStore::isValidName(longName));
        QVERIFY(DeviceStore::isValidName(ClientDeviceIdentity::machineName()));
    }

    void theShortNameIsTheShortHostName()
    {
        // Part C fix wave: the computer's short host name, trimmed to the
        // Core's 32-byte cap.
        QCOMPARE(ClientDeviceIdentity::kMaxShortNameBytes, DeviceStore::kMaxShortNameBytes);
        QCOMPARE(ClientDeviceIdentity::kMaxNameBytes, DeviceStore::kMaxNameBytes);
        QCOMPARE(ClientDeviceIdentity::shortNameFrom(QStringLiteral("Shack-MacBook.local")),
                 QStringLiteral("Shack-MacBook"));
        QCOMPARE(ClientDeviceIdentity::shortNameFrom(QStringLiteral("bench.example.org")),
                 QStringLiteral("bench"));
        QCOMPARE(ClientDeviceIdentity::shortNameFrom(QStringLiteral("  bench\x01pc  ")),
                 QStringLiteral("benchpc"));
        QCOMPARE(ClientDeviceIdentity::shortNameFrom(QString()), QStringLiteral("Computer"));
        QCOMPARE(ClientDeviceIdentity::shortNameFrom(QStringLiteral(".local")),
                 QStringLiteral("Computer"));
        const QString longName =
            ClientDeviceIdentity::shortNameFrom(QString(40, QChar(0x00E9)) + QStringLiteral(".lan"));
        QCOMPARE(longName.toUtf8().size(), 32);
        QVERIFY(DeviceStore::isValidShortName(longName));
        QVERIFY(DeviceStore::isValidShortName(ClientDeviceIdentity::machineShortName()));
    }

    // Every sentence this computer's pairing and sign-in can show is in
    // plain operator words and calls the NereusSDR computer the Core.
    void everySentenceShownIsPlain()
    {
        const QRegularExpression piece(QStringLiteral("\"([^\"]*)\""));
        int checked = 0;
        for (const char* file : {"src/core/session/StationPairingClient.cpp",
                                 "src/gui/RemoteConnectionController.cpp",
                                 "src/gui/GuiConnectionController.cpp",
                                 "src/gui/ConnectionSelector.cpp"}) {
            QFile source(QStringLiteral(NEREUS_SOURCE_DIR "/") + QLatin1String(file));
            QVERIFY2(source.open(QIODevice::ReadOnly), file);
            const QString code = QString::fromUtf8(source.readAll());
            // tr("...") and QStringLiteral("..."), adjacent literals joined.
            const QRegularExpression call(QStringLiteral(
                "\\b(?:tr|QStringLiteral)\\(\\s*((?:\"(?:[^\"\\\\]|\\\\.)*\"\\s*)+)"));
            for (auto it = call.globalMatch(code); it.hasNext();) {
                QString sentence;
                for (auto parts = piece.globalMatch(it.next().captured(1)); parts.hasNext();) {
                    sentence += parts.next().captured(1);
                }
                if (!sentence.contains(QLatin1Char(' ')) || !sentence.endsWith(QLatin1Char('.'))) {
                    continue;
                }
                sentence.replace(QRegularExpression(QStringLiteral("%[0-9]")), QStringLiteral("x"));
                ++checked;
                QVERIFY2(OperatorWording::isPlain(sentence), qPrintable(sentence));
                QVERIFY2(OperatorWording::coreCalledStationIn(sentence).isEmpty(),
                         qPrintable(sentence));
                QVERIFY2(!sentence.contains(QChar(0x2014)), qPrintable(sentence));
            }
        }
        QVERIFY2(checked >= 30, qPrintable(QString::number(checked)));
    }

    // ── Addresses ───────────────────────────────────────────────────────

    void typedAddressesTakeBothFamiliesAndNames()
    {
        const struct {
            const char* typed;
            const char* host;
            quint16 port;
        } good[] = {
            {"127.0.0.1", "127.0.0.1", 47910},
            {"192.168.1.20:5000", "192.168.1.20", 5000},
            {"::1", "::1", 47910},
            {"[::1]", "::1", 47910},
            {"[::1]:5000", "::1", 5000},
            {"2001:db8::20", "2001:db8::20", 47910},
            {"shack-core.local", "shack-core.local", 47910},
            {"shack-core.local:47911", "shack-core.local", 47911},
            {"wss://[2001:db8::20]:4433", "2001:db8::20", 4433},
            {" 10.0.0.5 ", "10.0.0.5", 47910},
        };
        for (const auto& entry : good) {
            QString host;
            quint16 port = 0;
            QVERIFY2(StationPairingClient::parseAddress(QString::fromLatin1(entry.typed), &host,
                                                        &port),
                     entry.typed);
            QCOMPARE(host, QString::fromLatin1(entry.host));
            QCOMPARE(port, entry.port);
        }
        for (const char* bad : {"", "   ", "[::1", "[::1]x", "[not-v6]:1", "10.0.0.5:0",
                                "10.0.0.5:99999", "https://core.example", "a b"}) {
            QString host;
            quint16 port = 0;
            QVERIFY2(!StationPairingClient::parseAddress(QString::fromLatin1(bad), &host, &port),
                     bad);
        }
        QCOMPARE(StationPairingClient::coreUrl(QStringLiteral("::1"), 47910).toString(),
                 QStringLiteral("wss://[::1]:47910"));
        QCOMPARE(StationPairingClient::coreUrl(QStringLiteral("127.0.0.1"), 47910).toString(),
                 QStringLiteral("wss://127.0.0.1:47910"));
    }

    // ── Refusals ────────────────────────────────────────────────────────

    // A mistyped word never leaves this computer, so it cannot burn the
    // Core's code.
    void aCodeNotOfTheListIsNeverSent()
    {
        QTemporaryDir keyDir;
        Core core;
        const quint64 serial = core.server->pairingWindow()->codeSerial();
        StationPairingClient client(makeKey(keyDir), kDeviceName);
        client.setTransportFactory(core.factory());
        QSignalSpy failed(&client, &StationPairingClient::failed);
        QSignalSpy paired(&client, &StationPairingClient::paired);
        for (const QString& typed : {QStringLiteral("7-anvil-harbour"), QStringLiteral("anvil"),
                                     QStringLiteral("0-anvil-harbor"), QString()}) {
            client.pairByCode(typed, QStringLiteral("127.0.0.1"), 47910);
        }
        QCOMPARE(failed.size(), 4);
        QVERIFY(OperatorWording::isPlain(failed.first().first().toString()));
        QCOMPARE(core.opened, 0);
        QCOMPARE(paired.size(), 0);
        QCOMPARE(core.server->pairingWindow()->codeSerial(), serial);
    }

    void aWrongCodeFailsWithTheCoresReasonAndBurnsTheCode()
    {
        QTemporaryDir keyDir;
        Core core;
        PairingWindow* window = core.server->pairingWindow();
        const QString code = window->currentCode();
        QVERIFY(!code.isEmpty());
        StationPairingClient client(makeKey(keyDir), kDeviceName);
        client.setTransportFactory(core.factory());
        QSignalSpy failed(&client, &StationPairingClient::failed);
        QSignalSpy paired(&client, &StationPairingClient::paired);
        client.pairByCode(otherCode(code), QStringLiteral("127.0.0.1"), 47910);
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 20000);
        const QString reason = failed.first().first().toString();
        QVERIFY2(reason.startsWith(QLatin1String("The pairing code was not right.")),
                 qPrintable(reason));
        QVERIFY(OperatorWording::isPlain(reason));
        QCOMPARE(paired.size(), 0);
        QVERIFY(core.server->deviceStore()->list().isEmpty());
        // Burned: the code is gone, and the next appears after the wait.
        QVERIFY(window->currentCode() != code);
        QVERIFY(window->retryAfterMs() > 0);
        QVERIFY(!client.isPairing());
    }

    void oneTapOnAClaimedCoreFailsWithTheCoresReason()
    {
        QTemporaryDir keyDir;
        QTemporaryDir otherDir;
        Core core;
        const ClientDeviceIdentity other = ClientDeviceIdentity::loadOrCreate(otherDir.path());
        PairedDevice device;
        device.id = other.fingerprint();
        device.publicKeySpki = other.publicKeySpki();
        device.name = QStringLiteral("Phone");
        device.kind = QStringLiteral("phone");
        QVERIFY(core.server->deviceStore()->add(device));
        StationPairingClient client(makeKey(keyDir), kDeviceName);
        client.setTransportFactory(core.factory());
        QSignalSpy failed(&client, &StationPairingClient::failed);
        QSignalSpy paired(&client, &StationPairingClient::paired);
        client.pairOnThisNetwork(QStringLiteral("127.0.0.1"), 47910);
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(),
                 QStringLiteral("This Core is not taking new devices. Open pairing on the Core "
                                "or on a paired device first."));
        QCOMPARE(paired.size(), 0);
        QCOMPARE(core.server->deviceStore()->list().size(), 1);
    }

    // A hello that does not offer pairing (a Core from before it).
    void aCoreThatDoesNotPairIsTold()
    {
        QTemporaryDir keyDir;
        auto* core = new LoopbackTransport(QStringLiteral("old core"), this);
        StationPairingClient client(makeKey(keyDir), kDeviceName);
        client.setTransportFactory([core](const QUrl&) -> SessionTransport* {
            auto* app = new LoopbackTransport(QStringLiteral("desktop"));
            core->linkTo(app);
            return app;
        });
        QSignalSpy failed(&client, &StationPairingClient::failed);
        client.pairOnThisNetwork(QStringLiteral("127.0.0.1"), 47910);
        core->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("old core"))));
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().first().toString().startsWith(
            QLatin1String("This Core cannot pair new devices.")));
        // Nothing but its hello... and not even that: it left before pair.start.
        for (const QByteArray& kind : core->receivedKinds()) {
            QVERIFY(kind != QByteArrayLiteral("pair.start"));
        }
    }

    // The Core's identity must vouch for the certificate the connection
    // presented: a pairing over another certificate, or none, is not kept.
    void anIdentityThatDoesNotBindTheCertificateIsNotKept_data()
    {
        QTest::addColumn<QByteArray>("certificate");
        QTest::newRow("another certificate") << QByteArray(32, '\x42');
        QTest::newRow("no certificate") << QByteArray();
    }

    void anIdentityThatDoesNotBindTheCertificateIsNotKept()
    {
        QFETCH(QByteArray, certificate);
        QTemporaryDir keyDir;
        Core core;
        StationPairingClient client(makeKey(keyDir), kDeviceName);
        client.setTransportFactory(core.factory(QStringLiteral("127.0.0.1"), certificate));
        QSignalSpy failed(&client, &StationPairingClient::failed);
        QSignalSpy paired(&client, &StationPairingClient::paired);
        client.pairOnThisNetwork(QStringLiteral("127.0.0.1"), 47910);
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(),
                 QStringLiteral("The Core's certificate is not signed by the Core that paired, "
                                "so this computer did not keep the pairing."));
        QCOMPARE(paired.size(), 0);
    }

    // ── Admit paths ─────────────────────────────────────────────────────

    void oneTapPairsThenSignsInByKey_data()
    {
        QTest::addColumn<QString>("host");
        QTest::newRow("IPv4") << QStringLiteral("127.0.0.1");
        QTest::newRow("IPv6") << QStringLiteral("::1");
    }

    void oneTapPairsThenSignsInByKey()
    {
        QFETCH(QString, host);
        QTemporaryDir keyDir;
        Core core;
        const auto key = makeKey(keyDir);
        StationPairingClient client(key, kDeviceName);
        client.setTransportFactory(core.factory(host));
        QSignalSpy failed(&client, &StationPairingClient::failed);
        QSignalSpy paired(&client, &StationPairingClient::paired);
        client.pairOnThisNetwork(host, 47910);
        QTRY_COMPARE(paired.size(), 1);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(core.dialled.size(), 1);
        QCOMPARE(core.dialled.first(), StationPairingClient::coreUrl(host, 47910));

        const auto record = paired.first().first().value<PairedStationRecord>();
        QCOMPARE(record.identityKey, core.server->stationIdentity().publicKeySpki());
        QCOMPARE(record.identityFingerprint, core.server->stationIdentity().fingerprint());
        QCOMPARE(record.label, core.server->devicesFacade()->stationLabel());
        QCOMPARE(record.host, host);
        QCOMPARE(record.port, quint16(47910));

        const auto listed = core.server->deviceStore()->find(key->fingerprint());
        QVERIFY(listed.has_value());
        QCOMPARE(listed->kind, QStringLiteral("computer"));
        QCOMPARE(listed->name, kDeviceName);

        signInByKey(core, key, record.identityFingerprint, host);
    }

    void theRightCodePairsThenSignsInByKey()
    {
        QTemporaryDir keyDir;
        QTemporaryDir otherDir;
        Core core;
        // A Core already paired with a phone, reopened for this computer:
        // only the code pairs.
        const ClientDeviceIdentity phone = ClientDeviceIdentity::loadOrCreate(otherDir.path());
        PairedDevice device;
        device.id = phone.fingerprint();
        device.publicKeySpki = phone.publicKeySpki();
        device.name = QStringLiteral("Phone");
        device.kind = QStringLiteral("phone");
        QVERIFY(core.server->deviceStore()->add(device));
        PairingWindow* window = core.server->pairingWindow();
        window->reopen();
        QCOMPARE(window->state(), PairingWindow::State::OpenReopened);
        const QString code = window->currentCode();
        QVERIFY(!code.isEmpty());

        const auto key = makeKey(keyDir);
        StationPairingClient client(key, kDeviceName);
        client.setTransportFactory(core.factory(QStringLiteral("::1")));
        QSignalSpy failed(&client, &StationPairingClient::failed);
        QSignalSpy paired(&client, &StationPairingClient::paired);
        // Typed loosely, as an operator reads it out.
        QString typed = code.toUpper();
        typed.replace(QLatin1Char('-'), QLatin1Char(' '));
        client.pairByCode(typed, QStringLiteral("::1"), 47910);
        QVERIFY(client.isPairing());
        QTRY_COMPARE_WITH_TIMEOUT(paired.size(), 1, 20000);
        QCOMPARE(failed.size(), 0);
        const auto record = paired.first().first().value<PairedStationRecord>();
        QCOMPARE(record.identityFingerprint, core.server->stationIdentity().fingerprint());
        QCOMPARE(record.label, core.server->devicesFacade()->stationLabel());
        const auto listed = core.server->deviceStore()->find(key->fingerprint());
        QVERIFY(listed.has_value());
        QCOMPARE(listed->kind, QStringLiteral("computer"));
        QCOMPARE(listed->name, kDeviceName);
        QCOMPARE(window->state(), PairingWindow::State::ClosedClaimed);

        signInByKey(core, key, record.identityFingerprint, QStringLiteral("::1"));
    }

    // The whole path over real TLS, on each address family: one tap over
    // wss:// to a listening Core, then a sign-in by key over wss:// with
    // no pin and no token, as a saved paired Core is dialled.
    void overRealTlsOnBothFamilies_data()
    {
        QTest::addColumn<QString>("host");
        QTest::newRow("IPv4") << QStringLiteral("127.0.0.1");
        QTest::newRow("IPv6") << QStringLiteral("::1");
    }

    void overRealTlsOnBothFamilies()
    {
        QFETCH(QString, host);
        if (!QSslSocket::supportsSsl()) {
            QSKIP("No TLS backend");
        }
        QTemporaryDir keyDir;
        Core core;
        if (!core.server->listen(QHostAddress(host), 0)) {
            QSKIP("This computer has no loopback address of this family");
        }
        const auto key = makeKey(keyDir);
        StationPairingClient client(key, kDeviceName);
        QSignalSpy failed(&client, &StationPairingClient::failed);
        QSignalSpy paired(&client, &StationPairingClient::paired);
        client.pairOnThisNetwork(host, core.server->serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(paired.size() == 1 || failed.size() == 1, 20000);
        QVERIFY2(failed.isEmpty(), qPrintable(failed.value(0).value(0).toString()));
        const auto record = paired.first().first().value<PairedStationRecord>();
        QCOMPARE(record.identityFingerprint, core.server->stationIdentity().fingerprint());
        QVERIFY(core.server->deviceStore()->find(key->fingerprint()).has_value());

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, kDeviceName);
        window.connectToStation(StationPairingClient::coreUrl(record.host, record.port), QString(),
                                QString(), false, record.identityFingerprint);
        QTRY_VERIFY_WITH_TIMEOUT(window.isHandshakeComplete() || !window.isConnectionActive(),
                                 20000);
        QVERIFY2(window.isHandshakeComplete(), qPrintable(window.lastError()));
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // A network whose proxy demands a login: pairing fails with plain words
    // saying so (NereusSDR has no login to give it).
    void aProxyThatNeedsALoginIsSaidPlainly()
    {
        QTemporaryDir keyDir;
        NereusSDR::Test::LoginProxy proxy;
        QVERIFY(proxy.listen());
        proxy.useAsSystemProxy();
        StationPairingClient client(makeKey(keyDir), kDeviceName);
        QSignalSpy failed(&client, &StationPairingClient::failed);
        client.pairOnThisNetwork(QStringLiteral("127.0.0.1"), 9);
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 10000);
        QVERIFY(proxy.requests() >= 1);
        QCOMPARE(failed.first().first().toString(),
                 QStringLiteral("This network's proxy needs a login, which NereusSDR can't provide."));
    }

private:
    // The window's own client, signing in by key on a new connection.
    void signInByKey(Core& core, const std::shared_ptr<const ClientDeviceIdentity>& key,
                     const QByteArray& identity, const QString& address)
    {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, kDeviceName, kShortName);
        LoopbackTransport* app = core.open(address, core.certSha256());
        window.startSession(app, QString(), QString(), identity);
        QTRY_VERIFY(window.isHandshakeComplete());
        QVERIFY(core.server->hasAuthenticatedSession());
        // Part C fix wave: the device block carried the short name, and the
        // Core stored it.
        const std::optional<PairedDevice> stored =
            core.server->deviceStore()->find(key->fingerprint());
        QVERIFY(stored.has_value());
        QCOMPARE(stored->shortName, kShortName);
        QCOMPARE(window.lastEndReport().kind, StationEndReport::Kind::None);
        window.disconnectFromStation(QStringLiteral("test done"));
    }

};

QTEST_MAIN(TstStationPairingClient)

#include "tst_station_pairing_client.moc"
