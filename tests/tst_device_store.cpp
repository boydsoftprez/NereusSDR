// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_device_store.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 12 (R-IOS-08, R-IOS-02): the Core's paired devices.
//
// Refusals first: a record whose id is not its key's fingerprint, a key
// that is not P-256, an unknown kind, a bad name and a duplicate are all
// refused with nothing written; a damaged file fails closed (no device
// found, nothing overwritten, and the Core still counts as claimed). Then
// the admit path: add, find, list, touch (lastSeen and lastAddress, empty
// over the relay), remove with its signals, the 0600 file surviving a
// reload, and isClaimed() for a new Core, a paired one and one still
// claimed through its token.
//
// Device keys are generated at run time (a StationIdentity in a scratch
// directory stands in for a phone's key).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
// =================================================================

#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/security/TokenStore.h"

using namespace NereusSDR;

namespace {

// A device key made at run time.
struct TestDevice {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());

    PairedDevice record(const QString& name = QStringLiteral("Shack iPhone"),
                        const QString& kind = QStringLiteral("phone")) const
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = name;
        device.kind = kind;
        return device;
    }
};

QByteArray readAll(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class TstDeviceStore : public QObject {
    Q_OBJECT

private slots:
    // ── Refusals ─────────────────────────────────────────────────────────

    void refusesRecordsThatAreNotWellFormed()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DeviceStore store(dir.path());
        QVERIFY(store.isValid());
        QSignalSpy changed(&store, &DeviceStore::devicesChanged);
        TestDevice phone;
        TestDevice other;

        PairedDevice wrongId = phone.record();
        wrongId.id = other.key.fingerprint();
        QVERIFY(!store.add(wrongId));

        PairedDevice notAKey = phone.record();
        notAKey.publicKeySpki = QByteArray(91, 'x');
        notAKey.id = StationIdentity::fingerprintOf(notAKey.publicKeySpki);
        QVERIFY(!store.add(notAKey));

        QVERIFY(!store.add(phone.record(QStringLiteral("Watch"), QStringLiteral("watch"))));
        QVERIFY(!store.add(phone.record(QString())));
        QVERIFY(!store.add(phone.record(QStringLiteral("   "))));
        QVERIFY(!store.add(phone.record(QStringLiteral("line\nbreak"))));
        QVERIFY(!store.add(phone.record(QString(65, QLatin1Char('n')))));

        QVERIFY(store.list().isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(store.filePath()));

        QVERIFY(store.add(phone.record()));
        const QByteArray written = readAll(store.filePath());
        QVERIFY(!store.add(phone.record(QStringLiteral("Again"))));
        QCOMPARE(readAll(store.filePath()), written);
        QCOMPARE(store.list().size(), 1);
    }

    void aDamagedFileFailsClosedAndIsNeverOverwritten_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::newRow("not json") << QByteArray("{{{");
        QTest::newRow("another version") << QByteArray("{\"version\":2,\"devices\":[]}");
        QTest::newRow("a bad record")
            << QByteArray("{\"version\":1,\"devices\":[{\"id\":\"x\"}]}");
    }

    void aDamagedFileFailsClosedAndIsNeverOverwritten()
    {
        QFETCH(QByteArray, contents);
        QTemporaryDir dir;
        QFile file(dir.filePath(QString::fromLatin1(DeviceStore::kFileName)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
        file.close();

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^Paired devices unavailable")));
        DeviceStore store(dir.path());
        QVERIFY(!store.isValid());
        QVERIFY(!store.lastError().isEmpty());
        // Claimed, so no pairing window opens because a file was damaged.
        QVERIFY(store.isClaimed());
        TestDevice phone;
        QVERIFY(!store.add(phone.record()));
        QVERIFY(!store.find(phone.key.fingerprint()).has_value());
        QVERIFY(!store.remove(phone.key.fingerprint()));
        QCOMPARE(readAll(store.filePath()), contents);
    }

    void aDuplicatedRecordInTheFileFailsClosed()
    {
        QTemporaryDir dir;
        TestDevice phone;
        {
            DeviceStore store(dir.path());
            QVERIFY(store.add(phone.record()));
        }
        QByteArray json = readAll(dir.filePath(QString::fromLatin1(DeviceStore::kFileName)));
        const int open = json.indexOf('{', json.indexOf('['));
        const int close = json.lastIndexOf('}', json.lastIndexOf(']'));
        const QByteArray record = json.mid(open, close - open + 1);
        json.replace(record, record + ',' + record);
        QFile file(dir.filePath(QString::fromLatin1(DeviceStore::kFileName)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(json);
        file.close();
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^Paired devices unavailable")));
        DeviceStore store(dir.path());
        QVERIFY(!store.isValid());
    }

    void unknownDevicesAreNotFoundAndNotTouched()
    {
        QTemporaryDir dir;
        DeviceStore store(dir.path());
        QSignalSpy changed(&store, &DeviceStore::devicesChanged);
        TestDevice stranger;
        QVERIFY(!store.find(stranger.key.fingerprint()).has_value());
        store.touch(stranger.key.fingerprint(), QStringLiteral("192.0.2.7"));
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(store.filePath()));
    }

    // ── The admit path ───────────────────────────────────────────────────

    void addFindTouchAndRemove()
    {
        QTemporaryDir dir;
        DeviceStore store(dir.path());
        QDateTime clock = QDateTime(QDate(2026, 9, 24), QTime(12, 0), Qt::UTC);
        store.setClock([&clock]() { return clock; });
        QSignalSpy changed(&store, &DeviceStore::devicesChanged);
        QSignalSpy removed(&store, &DeviceStore::deviceRemoved);

        TestDevice phone;
        TestDevice laptop;
        QVERIFY(store.add(phone.record()));
        PairedDevice enrolled = laptop.record(QStringLiteral("Shack Mac"), QStringLiteral("computer"));
        enrolled.enrolledThroughToken = true;
        QVERIFY(store.add(enrolled));
        QCOMPARE(changed.count(), 2);
        QCOMPARE(store.list().size(), 2);

        auto found = store.find(phone.key.fingerprint());
        QVERIFY(found.has_value());
        QCOMPARE(found->name, QStringLiteral("Shack iPhone"));
        QCOMPARE(found->kind, QStringLiteral("phone"));
        QCOMPARE(found->pairedAt, clock);
        QCOMPARE(found->lastSeen, clock);
        QVERIFY(!found->enrolledThroughToken);
        QVERIFY(store.find(laptop.key.fingerprint())->enrolledThroughToken);

        clock = clock.addSecs(3600);
        store.touch(phone.key.fingerprint(), QStringLiteral("192.0.2.7"));
        found = store.find(phone.key.fingerprint());
        QCOMPARE(found->lastSeen, clock);
        QCOMPARE(found->lastAddress, QStringLiteral("192.0.2.7"));
        QCOMPARE(found->pairedAt, clock.addSecs(-3600));
        // Over the relay the address is empty, never the relay's.
        store.touch(phone.key.fingerprint(), QString());
        QVERIFY(store.find(phone.key.fingerprint())->lastAddress.isEmpty());
        QCOMPARE(changed.count(), 4);

        QVERIFY(store.remove(phone.key.fingerprint()));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.first().first().toByteArray(), phone.key.fingerprint());
        QCOMPARE(changed.count(), 5);
        QVERIFY(!store.find(phone.key.fingerprint()).has_value());
        QVERIFY(!store.remove(phone.key.fingerprint()));
    }

    void theShortNameIsStoredAndReplacedAtEachSignIn()
    {
        // Part C fix wave: auth.request's `shortName`, the operator's own
        // words (so "Grant's iPhone" passes), at most 32 bytes of UTF-8,
        // replaced when a sign-in carries a usable one.
        QCOMPARE(DeviceStore::kMaxShortNameBytes, 32);
        QVERIFY(DeviceStore::isValidShortName(QStringLiteral("Grant's iPhone")));
        QVERIFY(DeviceStore::isValidShortName(QString(16, QChar(0x00E9))));        // 32 bytes
        QVERIFY(!DeviceStore::isValidShortName(QString(17, QChar(0x00E9))));       // 34 bytes
        QVERIFY(DeviceStore::isValidShortName(QString(32, QLatin1Char('a'))));
        QVERIFY(!DeviceStore::isValidShortName(QString(33, QLatin1Char('a'))));
        QVERIFY(!DeviceStore::isValidShortName(QString()));
        QVERIFY(!DeviceStore::isValidShortName(QStringLiteral("   ")));
        QVERIFY(!DeviceStore::isValidShortName(QStringLiteral("a\nb")));
        QVERIFY(!DeviceStore::isValidShortName(QStringLiteral("a\u200Eb")));

        QTemporaryDir dir;
        TestDevice phone;
        {
            DeviceStore store(dir.path());
            QVERIFY(store.add(phone.record()));
            QVERIFY(store.find(phone.key.fingerprint())->shortName.isEmpty());
            store.touch(phone.key.fingerprint(), QStringLiteral("192.0.2.7"),
                        QStringLiteral("Grant's iPhone"));
            QCOMPARE(store.find(phone.key.fingerprint())->shortName,
                     QStringLiteral("Grant's iPhone"));
            // A sign-in without one, or with an unusable one, keeps it.
            store.touch(phone.key.fingerprint(), QStringLiteral("192.0.2.7"));
            store.touch(phone.key.fingerprint(), QStringLiteral("192.0.2.7"),
                        QString(33, QLatin1Char('a')));
            QCOMPARE(store.find(phone.key.fingerprint())->shortName,
                     QStringLiteral("Grant's iPhone"));
            // The next usable one replaces it.
            store.touch(phone.key.fingerprint(), QStringLiteral("192.0.2.7"),
                        QStringLiteral("iPhone"));
            QCOMPARE(store.find(phone.key.fingerprint())->shortName, QStringLiteral("iPhone"));
        }
        // It survives a restart.
        DeviceStore reloaded(dir.path());
        QVERIFY2(reloaded.isValid(), qPrintable(reloaded.lastError()));
        QCOMPARE(reloaded.find(phone.key.fingerprint())->shortName, QStringLiteral("iPhone"));

        // A record with an unusable short name is refused.
        TestDevice tablet;
        PairedDevice bad = tablet.record(QStringLiteral("Tablet"), QStringLiteral("tablet"));
        bad.shortName = QString(33, QLatin1Char('a'));
        QVERIFY(!reloaded.add(bad));
    }

    void aListFromBeforeShortNamesStillLoads()
    {
        // Additive: a list written before the short name has no key for it.
        QTemporaryDir dir;
        TestDevice phone;
        {
            DeviceStore store(dir.path());
            QVERIFY(store.add(phone.record()));
        }
        const QString path = dir.filePath(QString::fromLatin1(DeviceStore::kFileName));
        QJsonDocument doc = QJsonDocument::fromJson(readAll(path));
        QJsonObject root = doc.object();
        QJsonArray devices = root.value(QStringLiteral("devices")).toArray();
        QJsonObject first = devices.at(0).toObject();
        QVERIFY(first.contains(QStringLiteral("shortName")));
        first.remove(QStringLiteral("shortName"));
        devices.replace(0, first);
        root.insert(QStringLiteral("devices"), devices);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            file.write(QJsonDocument(root).toJson());
        }
        DeviceStore old(dir.path());
        QVERIFY2(old.isValid(), qPrintable(old.lastError()));
        QVERIFY(old.find(phone.key.fingerprint())->shortName.isEmpty());

        // One that is there but not a string, or not usable, fails closed.
        for (const QJsonValue& wrong :
             {QJsonValue(7), QJsonValue(QString(33, QLatin1Char('a')))}) {
            first.insert(QStringLiteral("shortName"), wrong);
            devices.replace(0, first);
            root.insert(QStringLiteral("devices"), devices);
            {
                QFile file(path);
                QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
                file.write(QJsonDocument(root).toJson());
            }
            DeviceStore damaged(dir.path());
            QVERIFY(!damaged.isValid());
        }
    }

    void aListRestoredAtWiderPermissionsIsMadeOwnerOnlyOnLoad()
    {
        // Part C fix wave (R1-M5).
#ifdef Q_OS_WIN
        QSKIP("Windows files carry no Unix mode; the profile directory's ACL protects them.");
#else
        QTemporaryDir dir;
        TestDevice phone;
        {
            DeviceStore store(dir.path());
            QVERIFY(store.add(phone.record()));
        }
        const QString path = dir.filePath(QString::fromLatin1(DeviceStore::kFileName));
        QVERIFY(QFile(path).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                           | QFileDevice::ReadGroup | QFileDevice::ReadOther));
        DeviceStore reloaded(dir.path());
        QVERIFY2(reloaded.isValid(), qPrintable(reloaded.lastError()));
        QVERIFY(reloaded.find(phone.key.fingerprint()).has_value());
        const QFileDevice::Permissions others =
            QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
        QCOMPARE(QFile(path).permissions() & others, QFileDevice::Permissions());
#endif
    }

    void theListSurvivesARestartInAnOwnerOnlyFile()
    {
        QTemporaryDir dir;
        TestDevice phone;
        {
            DeviceStore store(dir.path());
            QVERIFY(store.add(phone.record(QStringLiteral("Tablet"), QStringLiteral("tablet"))));
            store.touch(phone.key.fingerprint(), QStringLiteral("2001:db8::7"));
        }
        const QString path = dir.filePath(QString::fromLatin1(DeviceStore::kFileName));
#ifndef Q_OS_WIN
        const QFileDevice::Permissions others = QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                                | QFileDevice::ReadOther
                                                | QFileDevice::WriteOther;
        QCOMPARE(QFile(path).permissions() & others, QFileDevice::Permissions());
#endif
        DeviceStore reloaded(dir.path());
        QVERIFY2(reloaded.isValid(), qPrintable(reloaded.lastError()));
        const auto found = reloaded.find(phone.key.fingerprint());
        QVERIFY(found.has_value());
        QCOMPARE(found->publicKeySpki, phone.key.publicKeySpki());
        QCOMPARE(found->kind, QStringLiteral("tablet"));
        QCOMPARE(found->lastAddress, QStringLiteral("2001:db8::7"));
        QVERIFY(found->lastSeen.isValid());
    }

    void claimedMeansAPairedDeviceOrAnActiveToken()
    {
        QTemporaryDir fresh;
        DeviceStore newCore(fresh.path());
        QVERIFY(!newCore.isClaimed());
        TestDevice phone;
        QVERIFY(newCore.add(phone.record()));
        QVERIFY(newCore.isClaimed());
        QVERIFY(newCore.remove(phone.key.fingerprint()));
        QVERIFY(!newCore.isClaimed());

        // An upgraded Core: a token file from before pairing existed.
        QTemporaryDir upgraded;
        {
            QFile token(upgraded.filePath(QStringLiteral("station-token")));
            QVERIFY(token.open(QIODevice::WriteOnly));
            token.write("upgraded-core-token-not-a-real-one-0123456");
        }
        TokenStore tokens(upgraded.path());
        QVERIFY(tokens.isActive());
        DeviceStore store(upgraded.path(), &tokens);
        QVERIFY(store.list().isEmpty());
        QVERIFY(store.isClaimed());
    }
};

QTEST_GUILESS_MAIN(TstDeviceStore)
#include "tst_device_store.moc"
