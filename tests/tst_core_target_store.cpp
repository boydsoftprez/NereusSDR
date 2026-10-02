// =================================================================
// tests/tst_core_target_store.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original unit-test file. Remote-daemon R3
// Task 4g saved Core address-book persistence.
//
// iPhone app Task 18 (R-IOS-08): the list lives under ConnectionTargets/V2
// with each Core's identity fingerprint; a V1 list migrates once, every
// record's trust details exactly, and is never read again.
// 2026-10-02: Manual listener retention, canonicalization and target leases.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "core/security/StationIdentity.h"
#include "gui/CoreTargetStore.h"

#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

constexpr auto kTargetKey = "ConnectionTargets/V3";
constexpr auto kV2Key = "ConnectionTargets/V2";
constexpr auto kV1Key = "ConnectionTargets/V1";

SavedCoreTarget makeTarget(const QString& id, const QString& token = QStringLiteral("token"))
{
    SavedCoreTarget target;
    target.id = id;
    target.label = QStringLiteral("Bench Core");
    target.connection.url = QStringLiteral("wss://core.example.test:4433");
    target.connection.token = token;
    target.connection.fingerprint = QStringLiteral("fingerprint");
    target.connection.allowUnpinned = false;
    target.lastRadioName = QStringLiteral("Radio One");
    target.lastRadioMac = QStringLiteral("00:11:22:33:44:55");
    return target;
}

QString documentFor(const QJsonArray& cores, const QString& selectedId = QStringLiteral("local"),
                    int version = 3)
{
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("version"), version},
        {QStringLiteral("selectedId"), selectedId},
        {QStringLiteral("cores"), cores},
    }).toJson(QJsonDocument::Compact));
}

// A V1 record: no identity key.
QJsonObject jsonV1Target(const QString& id)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), QStringLiteral("Core")},
        {QStringLiteral("url"), QStringLiteral("wss://core.example.test")},
        {QStringLiteral("token"), QStringLiteral("secret")},
        {QStringLiteral("fingerprint"), QStringLiteral("pin")},
        {QStringLiteral("allowUnpinned"), false},
        {QStringLiteral("lastRadioName"), QStringLiteral("Radio")},
        {QStringLiteral("lastRadioMac"), QStringLiteral("aa:bb")},
    };
}

// A V2 record: the V1 fields and the identity (empty: not paired).
QJsonObject jsonTarget(const QString& id, const QString& identity = QString())
{
    QJsonObject object = jsonV1Target(id);
    object.insert(QStringLiteral("identity"), identity);
    return object;
}

// 32 bytes made at run time, never a real Core's.
QByteArray someIdentity()
{
    QByteArray bytes(32, '\0');
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return bytes;
}

} // namespace

class TstCoreTargetStore : public QObject {
    Q_OBJECT

private slots:
    void manualEndpointNormalization_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<int>("port");
        QTest::addColumn<QString>("expected");
        QTest::newRow("hostname") << QStringLiteral("CoRe.Example.Test.") << 47910
            << QStringLiteral("wss://core.example.test:47910");
        QTest::newRow("single-label") << QStringLiteral("Core") << 1
            << QStringLiteral("wss://core:1");
        QTest::newRow("decimal-ipv4") << QStringLiteral("192.168.001.042") << 65535
            << QStringLiteral("wss://192.168.1.42:65535");
        QTest::newRow("pasted-port") << QStringLiteral("CORE.Example.Test.:047911") << 47910
            << QStringLiteral("wss://core.example.test:47911");
        QTest::newRow("pasted-ipv4") << QStringLiteral("192.168.001.042:47912") << 47910
            << QStringLiteral("wss://192.168.1.42:47912");
        QTest::newRow("ipv6") << QStringLiteral("2001:0DB8:0:0:0:0:0:5") << 47910
            << QStringLiteral("wss://[2001:db8::5]:47910");
        QTest::newRow("bracketed-ipv6") << QStringLiteral("[2001:DB8::5]") << 47910
            << QStringLiteral("wss://[2001:db8::5]:47910");
        QTest::newRow("pasted-ipv6") << QStringLiteral("[2001:DB8::5]:47912") << 47910
            << QStringLiteral("wss://[2001:db8::5]:47912");
        QTest::newRow("ipv6-scope") << QStringLiteral("fe80::1%en0") << 47910
            << QStringLiteral("wss://[fe80::1%25en0]:47910");
        QTest::newRow("numeric-scope") << QStringLiteral("fe80::1%25") << 47910
            << QStringLiteral("wss://[fe80::1%2525]:47910");
    }

    void manualEndpointNormalization()
    {
        QFETCH(QString, input);
        QFETCH(int, port);
        QFETCH(QString, expected);
        QString error = QStringLiteral("old error");
        QCOMPARE(CoreTargetStore::normalizeManualAddress(input, port, &error), expected);
        QVERIFY(error.isEmpty());
    }

    void manualEndpointRejectsNonListenerInput_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<int>("port");
        for (const QString& input : {
            QString(), QStringLiteral("wss://core.test"), QStringLiteral("https://core.test"),
            QStringLiteral("turn:core.test"), QStringLiteral("stun:core.test"),
            QStringLiteral("user@core.test"), QStringLiteral("core.test/path"),
            QStringLiteral("core.test?token=secret"), QStringLiteral("core.test#fragment"),
            QStringLiteral(" core.test"), QStringLiteral("core.test "), QStringLiteral("core\ttest"),
            QStringLiteral("256.1.2.3"), QStringLiteral("127.1"), QStringLiteral("1234"),
            QStringLiteral("1.2.3.4.5"), QStringLiteral("core..test"), QStringLiteral("-core.test"),
            QStringLiteral("core-.test"), QStringLiteral("core_test"),
            QStringLiteral("[2001:db8::1"), QStringLiteral("2001:db8::1]"),
            QStringLiteral("[2001:db8::1]]:47910"), QStringLiteral("[192.168.1.42]:47910"),
            QStringLiteral("[2001:db8::1]:0"), QStringLiteral("[2001:db8::1]:65536"),
            QStringLiteral("core.test:bad"), QStringLiteral("core.test:"),
            QStringLiteral("fe80::1%"), QStringLiteral("fe80::1%bad%scope"),
            QString(64, QLatin1Char('a')) + QStringLiteral(".test")}) {
            QTest::newRow(qPrintable(input.isEmpty() ? QStringLiteral("empty") : input)) << input << 47910;
        }
        QTest::newRow("port-zero") << QStringLiteral("core.test") << 0;
        QTest::newRow("port-overflow") << QStringLiteral("core.test") << 65536;
        QTest::newRow("port-negative") << QStringLiteral("core.test") << -1;
    }

    void manualEndpointRejectsNonListenerInput()
    {
        QFETCH(QString, input);
        QFETCH(int, port);
        QString error;
        QVERIFY(CoreTargetStore::normalizeManualAddress(input, port, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void manualMutationsDeduplicateAndReplaceAtCapacityWithoutChangingEvidence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget saved = makeTarget(QStringLiteral("paired"));
        saved.connection.identityFingerprint = someIdentity();
        saved.connection.cachedAddresses = {QStringLiteral("wss://192.0.2.9:47912")};
        saved.connection.coreAddresses = {QStringLiteral("wss://[2001:db8::5]:47913")};
        saved.autoConnect = false;
        QVERIFY(store.upsert(saved));
        QVERIFY(store.select(saved.id));
        const quint64 lease = store.targetIncarnation(saved.id);
        const QString before = settings.value(QLatin1String(kTargetKey)).toString();
        const QStringList expected{QStringLiteral("wss://core.test:47910"),
            QStringLiteral("wss://192.168.1.42:47911"), QStringLiteral("wss://[2001:db8::5]:47912"),
            QStringLiteral("wss://192.0.2.9:47912")};
        QVERIFY(store.addManualAddress(saved.id, QStringLiteral("wss://CORE.Test.:47910")));
        QVERIFY(store.addManualAddress(saved.id, QStringLiteral("wss://192.168.001.042:47911")));
        QVERIFY(store.addManualAddress(saved.id, QStringLiteral("wss://[2001:0DB8::5]:47912")));
        QVERIFY(store.addManualAddress(saved.id, expected.last()));
        QCOMPARE(store.target(saved.id)->manualAddresses, expected);
        const QString full = settings.value(QLatin1String(kTargetKey)).toString();
        QString error;
        for (const QString& duplicate : {QStringLiteral("wss://CORE.TEST:47910"),
                 QStringLiteral("wss://192.168.001.042:47911"),
                 QStringLiteral("wss://[2001:0db8:0:0:0:0:0:5]:47912")}) {
            QVERIFY(!store.addManualAddress(saved.id, duplicate, &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), full);
        }
        QVERIFY(!store.addManualAddress(saved.id, QStringLiteral("wss://fifth.test:47910"), &error));
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), full);
        QVERIFY(!store.addManualAddress(saved.id, QStringLiteral("ws://unsafe.test:47910"), &error));
        QVERIFY(!store.updateManualAddress(saved.id, QStringLiteral("wss://CORE.TEST:47910"),
                                          QStringLiteral("wss://new.test:47910"), &error));
        QVERIFY(!store.updateManualAddress(saved.id, expected.first(), expected.at(1), &error));
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), full);
        QVERIFY(store.updateManualAddress(saved.id, expected.first(), QStringLiteral("wss://NEW.Test.:47914"), &error));
        QStringList replaced = expected;
        replaced[0] = QStringLiteral("wss://new.test:47914");
        QCOMPARE(store.target(saved.id)->manualAddresses, replaced);
        QVERIFY(store.updateManualAddress(saved.id, replaced.first(), QStringLiteral("wss://NEW.Test.:47914"), &error));
        QVERIFY(error.isEmpty());
        QVERIFY(!store.removeManualAddress(saved.id, QStringLiteral("wss://NEW.Test:47914"), &error));
        for (const QString& address : replaced) { QVERIFY(store.removeManualAddress(saved.id, address)); }
        QVERIFY(store.target(saved.id)->manualAddresses.isEmpty());
        // Removing even an address that also worked affects retention only.
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), before);
        QCOMPARE(store.selectedId(), saved.id);
        QCOMPARE(store.targetIncarnation(saved.id), lease);
    }

    void manualScopeRoundTripsWithoutChangingItsInterface()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const SavedCoreTarget saved = makeTarget(QStringLiteral("scope"));
        QVERIFY(store.upsert(saved));
        QVERIFY(store.addManualAddress(saved.id, QStringLiteral("wss://[fe80::1%25en0]:47910")));
        QVERIFY(store.addManualAddress(saved.id, QStringLiteral("wss://[fe80::1%2525]:47910")));
        QCOMPARE(store.target(saved.id)->manualAddresses,
                 (QStringList{QStringLiteral("wss://[fe80::1%25en0]:47910"),
                              QStringLiteral("wss://[fe80::1%2525]:47910")}));
        AppSettings loadedSettings(path);
        loadedSettings.load();
        CoreTargetStore loaded(loadedSettings);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.target(saved.id)->manualAddresses, store.target(saved.id)->manualAddresses);
    }

    void malformedManualDocumentsCannotReplaceLoadedTargets()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget saved = makeTarget(QStringLiteral("one"));
        saved.manualAddresses = {QStringLiteral("wss://core.test:47910")};
        QVERIFY(store.upsert(saved));
        QVERIFY(store.select(saved.id));
        const QString good = settings.value(QLatin1String(kTargetKey)).toString();
        const QJsonObject record = QJsonDocument::fromJson(good.toUtf8()).object()
            .value(QStringLiteral("cores")).toArray().first().toObject();
        for (const QJsonValue& bad : {QJsonValue(QStringLiteral("not-array")), QJsonValue(QJsonArray{7}),
                 QJsonValue(QJsonArray{QString()}),
                 QJsonValue(QJsonArray{QStringLiteral("wss://CORE.test:47910")}),
                 QJsonValue(QJsonArray{QStringLiteral("ws://core.test:47910")}),
                 QJsonValue(QJsonArray{QStringLiteral("wss://core.test:47910/path")}),
                 QJsonValue(QJsonArray{QStringLiteral("wss://core.test")}),
                 QJsonValue(QJsonArray{QStringLiteral("wss://core.test:47910"), QStringLiteral("wss://core.test:47910")}),
                 QJsonValue(QJsonArray{QStringLiteral("wss://a.test:1"), QStringLiteral("wss://b.test:2"),
                     QStringLiteral("wss://c.test:3"), QStringLiteral("wss://d.test:4"), QStringLiteral("wss://e.test:5")})}) {
            QJsonObject invalid = record;
            invalid.insert(QStringLiteral("manualAddresses"), bad);
            const QString document = documentFor(QJsonArray{invalid}, saved.id);
            settings.setValue(QLatin1String(kTargetKey), document);
            QString error;
            QVERIFY(!store.load(&error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(store.target(saved.id)->manualAddresses, saved.manualAddresses);
            QCOMPARE(store.selectedId(), saved.id);
            QCOMPARE(store.targetIncarnation(saved.id), quint64(0));
            QVERIFY(!store.addManualAddress(saved.id, QStringLiteral("wss://new.test:47910"), &error));
            QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), document);
            settings.setValue(QLatin1String(kTargetKey), good);
            QVERIFY(store.load());
        }
    }

    void manualPersistenceFailureRollsBackAllDocumentsAndLease()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        const QString id = QStringLiteral("one");
        settings.setValue(QLatin1String(kV1Key), documentFor(QJsonArray{jsonV1Target(id)}, id, 1));
        settings.setValue(QLatin1String(kV2Key), documentFor(QJsonArray{jsonTarget(id)}, id, 2));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(store.addManualAddress(id, QStringLiteral("wss://core.test:47910")));
        const quint64 lease = store.targetIncarnation(id);
        QStringList documents;
        for (const char* key : {kTargetKey, kV2Key, kV1Key}) {
            documents.append(settings.value(QLatin1String(key)).toString());
        }
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path));
        QString error;
        QVERIFY(!store.addManualAddress(id, QStringLiteral("wss://new.test:47911"), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!store.updateManualAddress(id, QStringLiteral("wss://core.test:47910"),
                                          QStringLiteral("wss://new.test:47911"), &error));
        QVERIFY(!store.removeManualAddress(id, QStringLiteral("wss://core.test:47910"), &error));
        QVERIFY(!store.remove(id, &error));
        SavedCoreTarget changedIdentity = *store.target(id);
        changedIdentity.connection.identityFingerprint = QByteArray(32, 'x');
        QVERIFY(!store.upsert(changedIdentity, &error));
        QCOMPARE(store.targetIncarnation(id), lease);
        QCOMPARE(store.target(id)->manualAddresses, QStringList{QStringLiteral("wss://core.test:47910")});
        QCOMPARE(store.selectedId(), id);
        int index = 0;
        for (const char* key : {kTargetKey, kV2Key, kV1Key}) {
            QCOMPARE(settings.value(QLatin1String(key)).toString(), documents.at(index++));
        }
    }

    void failedReloadRepairCannotAuthorizeFromPreservedPresentation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        CoreTargetStore store(settings, [] { return qint64(900); });
        QVERIFY(store.load());
        SavedCoreTarget saved = makeTarget(QStringLiteral("one"));
        saved.connection.controlChannelVersion = 0;
        saved.connection.negativeControlObservedMs = 1000;
        QVERIFY(store.upsert(saved));
        const QString before = settings.value(QLatin1String(kTargetKey)).toString();
        QVERIFY(store.targetIncarnation(saved.id) != 0);
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path));
        QString error;
        QVERIFY(!store.load(&error)); // Future-observation repair cannot be saved.
        QVERIFY(!error.isEmpty());
        QCOMPARE(store.target(saved.id)->connection.negativeControlObservedMs, qint64(1000));
        QCOMPARE(store.targetIncarnation(saved.id), quint64(0));
        QVERIFY(!store.addManualAddress(saved.id, QStringLiteral("wss://new.test:47910"), &error));
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), before);
    }

    void manualRetentionDoesNotChangeOlderRollbackDocuments()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        const QString id = QStringLiteral("one");
        const QString v1 = documentFor(QJsonArray{jsonV1Target(id)}, id, 1);
        const QString v2 = documentFor(QJsonArray{jsonTarget(id)}, id, 2);
        settings.setValue(QLatin1String(kV1Key), v1);
        settings.setValue(QLatin1String(kV2Key), v2);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(store.target(id)->manualAddresses.isEmpty());
        QVERIFY(store.addManualAddress(id, QStringLiteral("wss://core.test:47910")));
        QCOMPARE(settings.value(QLatin1String(kV1Key)).toString(), v1);
        QCOMPARE(settings.value(QLatin1String(kV2Key)).toString(), v2);
        QVERIFY(store.removeManualAddress(id, QStringLiteral("wss://core.test:47910")));
        QCOMPARE(settings.value(QLatin1String(kV1Key)).toString(), v1);
        QCOMPARE(settings.value(QLatin1String(kV2Key)).toString(), v2);
    }

    void targetLeaseRetiresOnRecreationIdentityChangeAndReload()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        const QString id = QStringLiteral("one");
        QCOMPARE(store.targetIncarnation(id), quint64(0));
        QVERIFY(store.load());
        SavedCoreTarget saved = makeTarget(id);
        saved.connection.identityFingerprint = QByteArray(32, 'a');
        QVERIFY(store.upsert(saved));
        const quint64 first = store.targetIncarnation(id);
        QVERIFY(first != 0);
        saved.label = QStringLiteral("Edited legacy label");
        QVERIFY(store.upsert(saved));
        QVERIFY(store.rememberAddress(id, QStringLiteral("wss://192.0.2.9:47910")));
        QCOMPARE(store.targetIncarnation(id), first);
        const SavedCoreTarget beforeForget = *store.target(id);
        QVERIFY(store.remove(id));
        QCOMPARE(store.targetIncarnation(id), quint64(0));
        QVERIFY(store.upsert(beforeForget));
        const quint64 recreated = store.targetIncarnation(id);
        QVERIFY(recreated > first);
        SavedCoreTarget replacement = *store.target(id);
        replacement.connection.identityFingerprint = QByteArray(32, 'b');
        QVERIFY(store.upsert(replacement));
        const quint64 identityChanged = store.targetIncarnation(id);
        QVERIFY(identityChanged > recreated);
        replacement.connection.fingerprint = QStringLiteral("new-pin");
        QVERIFY(store.upsert(replacement));
        const quint64 trustChanged = store.targetIncarnation(id);
        QVERIFY(trustChanged > identityChanged);
        replacement.connection.allowUnpinned = true;
        QVERIFY(store.upsert(replacement));
        const quint64 trustPolicyChanged = store.targetIncarnation(id);
        QVERIFY(trustPolicyChanged > trustChanged);
        QVERIFY(store.load());
        const quint64 reloaded = store.targetIncarnation(id);
        QVERIFY(reloaded > trustPolicyChanged);
        CoreTargetStore anotherStore(settings);
        QVERIFY(anotherStore.load());
        QVERIFY(anotherStore.targetIncarnation(id) > reloaded);
        settings.setValue(QLatin1String(kTargetKey), QStringLiteral("broken"));
        QVERIFY(!store.load());
        QVERIFY(store.target(id).has_value()); // Presentation survives, authority does not.
        QCOMPARE(store.targetIncarnation(id), quint64(0));
    }

    void manualAddressesSurviveV3RoundTripWithoutChangingCoreEvidence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        const QString id = QStringLiteral("paired");
        QJsonObject record = jsonTarget(id, StationIdentity::toBase64Url(someIdentity()));
        const QJsonArray manual{QStringLiteral("wss://192.168.1.42:47910"),
                                QStringLiteral("wss://core.example.test:47911")};
        record.insert(QStringLiteral("manualAddresses"), manual);
        record.insert(QStringLiteral("lastAddresses"),
                      QJsonArray{QStringLiteral("wss://192.0.2.9:47912")});
        record.insert(QStringLiteral("coreAddresses"),
                      QJsonArray{QStringLiteral("wss://[2001:db8::5]:47913")});
        record.insert(QStringLiteral("autoConnect"), false);
        settings.setValue(QLatin1String(kTargetKey), documentFor(QJsonArray{record}, id));

        CoreTargetStore store(settings);
        QVERIFY(store.load());
        const auto loaded = store.target(id);
        QVERIFY(loaded.has_value());
        QVERIFY(store.upsert(*loaded));
        QCOMPARE(store.selectedId(), id);
        QJsonObject persisted = QJsonDocument::fromJson(
            settings.value(QLatin1String(kTargetKey)).toString().toUtf8()).object()
            .value(QStringLiteral("cores")).toArray().first().toObject();
        QJsonObject originalEvidence = record;
        originalEvidence.remove(QStringLiteral("manualAddresses"));
        QJsonObject persistedEvidence = persisted;
        persistedEvidence.remove(QStringLiteral("manualAddresses"));
        QCOMPARE(persistedEvidence, originalEvidence);
        // These were entered by the operator, not proved by a connection.
        // Dropping them during an ordinary save loses the address book.
        QCOMPARE(persisted.value(QStringLiteral("manualAddresses")).toArray(), manual);

        AppSettings reloadedSettings(path);
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.selectedId(), id);
        QVERIFY(reloaded.target(id).has_value());
        QVERIFY(reloaded.upsert(*reloaded.target(id)));
        const QJsonObject afterReload = QJsonDocument::fromJson(
            reloadedSettings.value(QLatin1String(kTargetKey)).toString().toUtf8()).object()
            .value(QStringLiteral("cores")).toArray().first().toObject();
        QCOMPARE(afterReload, record);
    }

    void coreListedAddressesUseGlobalLiteralsExactPortsAndSeparateIdentity()
    {
        const QString wire = QString::fromUtf8("{\"addresses\":[\"[2001:0DB8::5]:47912\",\"[2001:db8::5]:47912\",\"8.8.8.8:47913\",\"192.168.1.2:47910\",\"100.64.0.2:47910\",\"[fe80::1]:47910\",\"[fd00::1]:47910\",\"[::ffff:8.8.8.8]:47910\",\"core.test:47910\",\"8.8.4.4:0\",\"8.8.4.4:65536\",\"wss://8.8.4.4:47910\",7]}");
        const QStringList expected{QStringLiteral("wss://[2001:db8::5]:47912"), QStringLiteral("wss://8.8.8.8:47913")};
        QCOMPARE(CoreTargetStore::parseCoreAddresses(wire), expected);
        QCOMPARE(CoreTargetStore::parseCoreAddresses(QStringLiteral("broken")), QStringList{});
        QTemporaryDir directory;
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget saved = makeTarget(QStringLiteral("paired"));
        saved.connection.identityFingerprint = someIdentity();
        saved.connection.cachedAddresses = {QStringLiteral("wss://192.0.2.9:47910")};
        QVERIFY(store.upsert(saved));
        QVERIFY(!store.rememberCoreAddresses(saved.id, QByteArray(32, 'x'), wire));
        QVERIFY(store.target(saved.id)->connection.coreAddresses.isEmpty());
        QVERIFY(store.rememberCoreAddresses(saved.id, saved.connection.identityFingerprint, wire));
        QCOMPARE(store.target(saved.id)->connection.coreAddresses, expected);
        QCOMPARE(store.target(saved.id)->connection.cachedAddresses, saved.connection.cachedAddresses);
        QCOMPARE(store.target(saved.id)->connection.url, saved.connection.url);
        for (const QString& empty : {QStringLiteral("broken"), QStringLiteral("{}"), QString::fromUtf8("{\"addresses\":[]}")}) {
            QVERIFY(store.rememberCoreAddresses(saved.id, saved.connection.identityFingerprint, empty));
            QCOMPARE(store.target(saved.id)->connection.coreAddresses, expected);
        }
        QStringList many;
        for (int i = 1; i <= 12; ++i) { many.append(QStringLiteral("[2001:db8::%1]:47910").arg(i)); }
        const QString manyWire = QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("addresses"), QJsonArray::fromStringList(many)}}).toJson());
        QCOMPARE(CoreTargetStore::parseCoreAddresses(manyWire).size(), 8);
        QVERIFY(store.rememberCoreAddresses(saved.id, saved.connection.identityFingerprint, manyWire));
        QCOMPARE(store.target(saved.id)->connection.coreAddresses.size(), 8);
        QVERIFY(store.remove(saved.id));
        QVERIFY(!store.rememberCoreAddresses(saved.id, saved.connection.identityFingerprint, wire));
    }

    void failedLearnedAddressSaveKeepsPriorIdentityHistoryAndList()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget saved = makeTarget(QStringLiteral("paired"));
        saved.connection.identityFingerprint = someIdentity();
        saved.connection.cachedAddresses = {saved.connection.url};
        QVERIFY(store.upsert(saved));
        const QString first = QString::fromUtf8("{\"addresses\":[\"[2001:db8::5]:47912\"]}");
        QVERIFY(store.rememberCoreAddresses(saved.id, saved.connection.identityFingerprint, first));
        const auto before = *store.target(saved.id);
        const QString document = settings.value(QLatin1String(kTargetKey)).toString();
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path));
        QString error;
        QVERIFY(!store.rememberCoreAddresses(saved.id, saved.connection.identityFingerprint,
            QString::fromUtf8("{\"addresses\":[\"[2001:db8::6]:47913\"]}"), &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(store.target(saved.id)->connection.coreAddresses, before.connection.coreAddresses);
        QCOMPARE(store.target(saved.id)->connection.cachedAddresses, before.connection.cachedAddresses);
        QCOMPARE(store.target(saved.id)->connection.identityFingerprint, before.connection.identityFingerprint);
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), document);
    }

    void learnedCoreAddressesPersistSeparatelyWithIdentity()
    {
        QTemporaryDir directory;
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        QJsonObject record = jsonTarget(QStringLiteral("paired"), StationIdentity::toBase64Url(someIdentity()));
        const QJsonArray learned{QStringLiteral("wss://[2001:db8::5]:47912"), QStringLiteral("wss://8.8.8.8:47913")};
        record.insert(QStringLiteral("coreAddresses"), learned);
        record.insert(QStringLiteral("lastAddresses"), QJsonArray{QStringLiteral("wss://192.0.2.9:47910")});
        settings.setValue(QLatin1String(kTargetKey), documentFor(QJsonArray{record}));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(store.upsert(*store.target(QStringLiteral("paired"))));
        const auto persisted = QJsonDocument::fromJson(settings.value(QLatin1String(kTargetKey)).toString().toUtf8()).object()
            .value(QStringLiteral("cores")).toArray().first().toObject();
        QCOMPARE(persisted.value(QStringLiteral("coreAddresses")).toArray(), learned);
        QCOMPARE(persisted.value(QStringLiteral("lastAddresses")).toArray(), record.value(QStringLiteral("lastAddresses")).toArray());
    }
    void pairedServiceOnlyTargetIsRemoteAndPersistsWithoutChangingV2()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        const QString oldDocument = documentFor(QJsonArray{jsonTarget(QStringLiteral("existing"))},
                                                QStringLiteral("existing"), 2);
        settings.setValue(QLatin1String(kV2Key), oldDocument);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget paired = makeTarget(QStringLiteral("service-only"), QString());
        paired.connection.url.clear();
        paired.connection.fingerprint.clear();
        paired.connection.identityFingerprint = someIdentity();
        paired.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY(paired.connection.isRemote());
        QVERIFY(store.upsert(paired));
        QVERIFY(store.select(paired.id));
        QCOMPARE(settings.value(QLatin1String(kV2Key)).toString(), oldDocument);
        QVERIFY(settings.contains(QLatin1String(kTargetKey)));

        AppSettings reloadedSettings(path);
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.selectedId(), paired.id);
        QCOMPARE(reloaded.target(paired.id)->connection.url, QString());
        QCOMPARE(reloaded.target(paired.id)->connection.identityFingerprint,
                 paired.connection.identityFingerprint);
        QCOMPARE(reloaded.target(paired.id)->connection.rendezvousId,
                 paired.connection.rendezvousId);
    }

    void invalidServiceOnlyTargetAndAuthoritativeV3FailClosed()
    {
        QTemporaryDir directory;
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        settings.setValue(QLatin1String(kV2Key),
                          documentFor(QJsonArray{jsonTarget(QStringLiteral("old"))},
                                      QStringLiteral("old"), 2));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target = makeTarget(QStringLiteral("service"));
        target.connection.url.clear();
        target.connection.token.clear();
        target.connection.fingerprint.clear();
        target.connection.identityFingerprint = someIdentity();
        target.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY(store.upsert(target));
        for (void (*mutate)(SavedCoreTarget&) : {
            +[](SavedCoreTarget& candidate) { candidate.connection.identityFingerprint.clear(); },
            +[](SavedCoreTarget& candidate) { candidate.connection.rendezvousId = QStringLiteral("bad"); },
            +[](SavedCoreTarget& candidate) { candidate.connection.token = QStringLiteral("secret"); },
            +[](SavedCoreTarget& candidate) { candidate.connection.allowUnpinned = true; },
        }) {
            SavedCoreTarget invalid = target;
            mutate(invalid);
            QVERIFY(!store.upsert(invalid));
            QCOMPARE(store.target(target.id)->connection.identityFingerprint,
                     target.connection.identityFingerprint);
        }
        const QString previousV2 = settings.value(QLatin1String(kV2Key)).toString();
        settings.setValue(QLatin1String(kTargetKey), QStringLiteral("{ malformed"));
        CoreTargetStore reloaded(settings);
        QString error;
        QVERIFY(!reloaded.load(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(reloaded.targets().isEmpty());
        QCOMPARE(settings.value(QLatin1String(kV2Key)).toString(), previousV2);
    }

    void autoConnectIsPerCoreAndDefaultsOnForExistingRecords()
    {
        QTemporaryDir directory;
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        auto first = makeTarget(QStringLiteral("first"));
        auto second = makeTarget(QStringLiteral("second"));
        first.autoConnect = false;
        QVERIFY(store.upsert(first));
        QVERIFY(store.upsert(second));
        QVERIFY(!store.target(first.id)->autoConnect);
        QVERIFY(store.target(second.id)->autoConnect);
        AppSettings reloadedSettings(directory.filePath(QStringLiteral("settings.xml")));
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings);
        QVERIFY(reloaded.load());
        QVERIFY(!reloaded.target(first.id)->autoConnect);
        QVERIFY(reloaded.target(second.id)->autoConnect);

        const auto document = QJsonDocument::fromJson(
            reloadedSettings.value(QLatin1String(kTargetKey)).toString().toUtf8()).object();
        const QJsonArray records = document.value(QStringLiteral("cores")).toArray();
        QCOMPARE(records.at(0).toObject().value(QStringLiteral("autoConnect")), QJsonValue(false));
        QVERIFY(!records.at(1).toObject().contains(QStringLiteral("autoConnect")));
    }

    void newKeyIsOperatorLocal()
    {
        for (const char* key : {kV1Key, kV2Key, kTargetKey}) {
            QCOMPARE(classifySettingsKey(QString::fromLatin1(key)), SettingsScope::OperatorLocal);
            SettingsProxy proxy;
            QVERIFY(!proxy.handlesKey(QString::fromLatin1(key)));
        }
    }

    void persistsAndReloadsMultipleIndependentCredentials()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        CoreTargetStore store(settings);
        QVERIFY(store.load());

        SavedCoreTarget first = makeTarget(QStringLiteral("first"), QStringLiteral("first-token"));
        SavedCoreTarget second = makeTarget(QStringLiteral("second"), QStringLiteral("second-token"));
        second.connection.url = QStringLiteral("ws://second.example.test:9000");
        second.connection.allowUnpinned = true;
        second.lastRadioName = QStringLiteral("Radio Two");

        QVERIFY(store.upsert(first));
        QVERIFY(store.upsert(second));
        QVERIFY(store.select(second.id));

        AppSettings reloadedSettings(path);
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.selectedId(), QStringLiteral("second"));
        QCOMPARE(reloaded.targets().size(), 2);
        const auto restoredFirst = reloaded.target(QStringLiteral("first"));
        const auto restoredSecond = reloaded.target(QStringLiteral("second"));
        QVERIFY(restoredFirst.has_value());
        QVERIFY(restoredSecond.has_value());
        QCOMPARE(restoredFirst->connection.token, QStringLiteral("first-token"));
        QCOMPARE(restoredSecond->connection.token, QStringLiteral("second-token"));
        QCOMPARE(restoredSecond->connection.url, QStringLiteral("ws://second.example.test:9000"));
        QVERIFY(restoredSecond->connection.allowUnpinned);
        QCOMPARE(restoredSecond->lastRadioName, QStringLiteral("Radio Two"));
    }

    void migratesExactLegacyTrustTupleOnce()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        settings.setValue(QStringLiteral("RemoteStationUrl"),
                          QStringLiteral("wss://Legacy.Example.Test:4433/path"));
        settings.setValue(QStringLiteral("RemoteStationToken"), QStringLiteral(" legacy token "));
        settings.setValue(QStringLiteral("RemoteStationFingerprint"), QStringLiteral(" legacy pin "));
        settings.setValue(QStringLiteral("RemoteStationAllowUnpinned"), QStringLiteral("True"));

        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QCOMPARE(store.selectedId(), QStringLiteral("legacy-core"));
        const auto migrated = store.target(QStringLiteral("legacy-core"));
        QVERIFY(migrated.has_value());
        QCOMPARE(migrated->connection.url, QStringLiteral("wss://Legacy.Example.Test:4433/path"));
        QCOMPARE(migrated->connection.token, QStringLiteral(" legacy token "));
        QCOMPARE(migrated->connection.fingerprint, QStringLiteral(" legacy pin "));
        QVERIFY(migrated->connection.allowUnpinned);
        QCOMPARE(migrated->label, QStringLiteral("legacy.example.test"));
        QVERIFY(settings.contains(QLatin1String(kTargetKey)));

        QVERIFY(store.select(QStringLiteral("local")));
        QVERIFY(store.remove(QStringLiteral("legacy-core")));
        CoreTargetStore secondLoad(settings);
        QVERIFY(secondLoad.load());
        QCOMPARE(secondLoad.selectedId(), QStringLiteral("local"));
        QVERIFY(secondLoad.targets().isEmpty());
    }

    void invalidLegacyAddressDoesNotOverwrite()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        settings.setValue(QStringLiteral("RemoteStationUrl"), QStringLiteral("https://wrong.example.test"));
        CoreTargetStore store(settings);
        QString error;

        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!settings.contains(QLatin1String(kTargetKey)));
        QCOMPARE(store.selectedId(), QStringLiteral("local"));
        QVERIFY(store.targets().isEmpty());
    }

    void malformedDocumentDoesNotReplaceLoadedState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(store.upsert(makeTarget(QStringLiteral("known"))));
        QVERIFY(store.select(QStringLiteral("known")));
        const QString malformed = documentFor(QJsonArray{jsonTarget(QStringLiteral("known"))},
                                              QStringLiteral("missing"));
        settings.setValue(QLatin1String(kTargetKey), malformed);
        QString error;

        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(store.selectedId(), QStringLiteral("known"));
        QCOMPARE(store.targets().size(), 1);
        QCOMPARE(store.targets().first().id, QStringLiteral("known"));
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), malformed);
    }

    void rejectsTypedMalformedAndBoundedDocuments()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QString error;

        QJsonObject badType = jsonTarget(QStringLiteral("one"));
        badType.insert(QStringLiteral("allowUnpinned"), QStringLiteral("False"));
        settings.setValue(QLatin1String(kTargetKey),
                          documentFor(QJsonArray{badType}, QStringLiteral("one")));
        QVERIFY(!store.load(&error));

        settings.setValue(QLatin1String(kTargetKey),
                          documentFor(QJsonArray{jsonTarget(QStringLiteral("one")),
                                                 jsonTarget(QStringLiteral("one"))},
                                      QStringLiteral("one")));
        QVERIFY(!store.load(&error));

        QJsonArray tooMany;
        for (int i = 0; i < 129; ++i) {
            tooMany.append(jsonTarget(QStringLiteral("id%1").arg(i)));
        }
        settings.setValue(QLatin1String(kTargetKey), documentFor(tooMany));
        QVERIFY(!store.load(&error));

        QJsonObject tooLong = jsonTarget(QStringLiteral("one"));
        tooLong.insert(QStringLiteral("token"), QString(8193, QLatin1Char('x')));
        settings.setValue(QLatin1String(kTargetKey),
                          documentFor(QJsonArray{tooLong}, QStringLiteral("one")));
        QVERIFY(!store.load(&error));
    }

    void mutationsRequireSuccessfulLoadAndRecoverAfterReload()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        const QString valid = documentFor(QJsonArray{jsonTarget(QStringLiteral("one"))},
                                          QStringLiteral("one"));
        settings.setValue(QLatin1String(kTargetKey), valid);
        QVERIFY(settings.save());

        CoreTargetStore store(settings);
        QString error;
        QVERIFY(!store.upsert(makeTarget(QStringLiteral("two")), &error));
        QVERIFY(!store.remove(QStringLiteral("one"), &error));
        QVERIFY(!store.select(QStringLiteral("local"), &error));
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), valid);
        AppSettings unchangedOnDisk(path);
        unchangedOnDisk.load();
        QCOMPARE(unchangedOnDisk.value(QLatin1String(kTargetKey)).toString(), valid);

        QVERIFY(store.load());
        QCOMPARE(store.selectedId(), QStringLiteral("one"));
        QCOMPARE(store.targets().size(), 1);
        const QString malformed = QStringLiteral("{bad json");
        settings.setValue(QLatin1String(kTargetKey), malformed);
        QVERIFY(!store.load(&error));
        QCOMPARE(store.selectedId(), QStringLiteral("one"));
        QCOMPARE(store.targets().size(), 1);
        QVERIFY(!store.upsert(makeTarget(QStringLiteral("two")), &error));
        QVERIFY(!store.remove(QStringLiteral("one"), &error));
        QVERIFY(!store.select(QStringLiteral("local"), &error));
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), malformed);

        settings.setValue(QLatin1String(kTargetKey), valid);
        QVERIFY(store.load());
        QVERIFY(store.upsert(makeTarget(QStringLiteral("two"))));
        QVERIFY(store.remove(QStringLiteral("two")));
        QVERIFY(store.select(QStringLiteral("local")));
    }

    void serializedDocumentBoundPreventsAggregateEscapedGrowth()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());

        bool rejected = false;
        for (int i = 0; i < 128; ++i) {
            SavedCoreTarget target = makeTarget(QStringLiteral("id%1").arg(i));
            // The field is individually legal, but JSON escaping doubles
            // every byte and must not create a document load() will reject.
            target.connection.token = QString(8192, QLatin1Char('\\'));
            const QString before = settings.value(QLatin1String(kTargetKey)).toString();
            const int countBefore = store.targets().size();
            QString error;
            if (!store.upsert(target, &error)) {
                QVERIFY(!error.isEmpty());
                QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), before);
                QCOMPARE(store.targets().size(), countBefore);
                rejected = true;
                break;
            }
        }
        QVERIFY(rejected);
    }

    void mutationRejectsUnsafeIdAndPreservesOrdering()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(store.upsert(makeTarget(QStringLiteral("first"))));
        QVERIFY(store.upsert(makeTarget(QStringLiteral("second"))));

        SavedCoreTarget updated = makeTarget(QStringLiteral("first"), QStringLiteral("new-token"));
        updated.lastRadioMac = QStringLiteral("new-mac");
        QVERIFY(store.upsert(updated));
        QCOMPARE(store.targets().at(0).id, QStringLiteral("first"));
        QCOMPARE(store.targets().at(1).id, QStringLiteral("second"));
        QCOMPARE(store.targets().at(0).connection.token, QStringLiteral("new-token"));
        QCOMPARE(store.targets().at(0).lastRadioMac, QStringLiteral("new-mac"));

        SavedCoreTarget unsafe = makeTarget(QStringLiteral("local"));
        QString error;
        QVERIFY(!store.upsert(unsafe, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!store.remove(QStringLiteral("missing"), &error));
        QVERIFY(!store.select(QStringLiteral("missing"), &error));
    }

    void saveFailureRollsBackExistingKeyAndAbsentMigration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString blocker = directory.filePath(QStringLiteral("not-a-file"));
        QVERIFY(QDir().mkpath(blocker));
        AppSettings settings(blocker);
        const QString previous = documentFor(QJsonArray{jsonTarget(QStringLiteral("first"))},
                                             QStringLiteral("first"));
        settings.setValue(QLatin1String(kTargetKey), previous);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QString error;

        QVERIFY(!store.upsert(makeTarget(QStringLiteral("second")), &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), previous);
        QCOMPARE(store.targets().size(), 1);
        QCOMPARE(store.selectedId(), QStringLiteral("first"));

        AppSettings missingSettings(blocker);
        CoreTargetStore missingStore(missingSettings);
        QVERIFY(!missingStore.load(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!missingSettings.contains(QLatin1String(kTargetKey)));
        QVERIFY(missingStore.targets().isEmpty());
        QCOMPARE(missingStore.selectedId(), QStringLiteral("local"));
    }

    // iPhone app Task 18: the V1 list moves to V2 once. Every record's
    // trust details (address, token, pin, bench flag) and its label, last
    // radio and the selection come over exactly, with no identity; after
    // that V2 is what is read, whatever V1 holds.
    void v1RecordsMigrateOnceWithTheirTrustExactly()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        QJsonObject first = jsonV1Target(QStringLiteral("first"));
        QJsonObject second = jsonV1Target(QStringLiteral("second"));
        second.insert(QStringLiteral("url"), QStringLiteral("ws://[2001:db8::7]:9000"));
        second.insert(QStringLiteral("token"), QStringLiteral(" spaced token "));
        second.insert(QStringLiteral("fingerprint"), QString());
        second.insert(QStringLiteral("allowUnpinned"), true);
        second.insert(QStringLiteral("label"), QStringLiteral("Bench"));
        second.insert(QStringLiteral("lastRadioName"), QStringLiteral("Saturn"));
        second.insert(QStringLiteral("lastRadioMac"), QStringLiteral("AA:BB:CC:DD:EE:01"));
        const QString v1 = documentFor(QJsonArray{first, second}, QStringLiteral("second"), 1);
        settings.setValue(QLatin1String(kV1Key), v1);
        QVERIFY(settings.save());

        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QCOMPARE(store.selectedId(), QStringLiteral("second"));
        QCOMPARE(store.targets().size(), 2);
        const auto migrated = store.target(QStringLiteral("second"));
        QVERIFY(migrated.has_value());
        QCOMPARE(migrated->label, QStringLiteral("Bench"));
        QCOMPARE(migrated->connection.url, QStringLiteral("ws://[2001:db8::7]:9000"));
        QCOMPARE(migrated->connection.token, QStringLiteral(" spaced token "));
        QVERIFY(migrated->connection.fingerprint.isEmpty());
        QVERIFY(migrated->connection.allowUnpinned);
        QCOMPARE(migrated->lastRadioName, QStringLiteral("Saturn"));
        QCOMPARE(migrated->lastRadioMac, QStringLiteral("AA:BB:CC:DD:EE:01"));
        QVERIFY(migrated->connection.identityFingerprint.isEmpty());
        const auto plain = store.target(QStringLiteral("first"));
        QVERIFY(plain.has_value());
        QCOMPARE(plain->connection.token, QStringLiteral("secret"));
        QCOMPARE(plain->connection.fingerprint, QStringLiteral("pin"));
        QVERIFY(!plain->connection.allowUnpinned);

        // V3 is authoritative; V2 is a readable rollback copy and V1 is
        // left exactly as it was during migration.
        AppSettings onDisk(path);
        onDisk.load();
        QVERIFY(onDisk.contains(QLatin1String(kTargetKey)));
        QCOMPARE(onDisk.value(QLatin1String(kV1Key)).toString(), v1);
        const QJsonObject v3 = QJsonDocument::fromJson(
            onDisk.value(QLatin1String(kTargetKey)).toString().toUtf8()).object();
        QCOMPARE(v3.value(QStringLiteral("version")).toInt(), 3);
        const QJsonObject v2 = QJsonDocument::fromJson(
            onDisk.value(QLatin1String(kV2Key)).toString().toUtf8()).object();
        QCOMPARE(v2.value(QStringLiteral("version")).toInt(), 2);
        for (const QJsonValue& core : v2.value(QStringLiteral("cores")).toArray()) {
            QCOMPARE(core.toObject().value(QStringLiteral("identity")).toString(), QString());
        }

        // Never read again: a change to V1 now has no effect.
        settings.setValue(QLatin1String(kV1Key),
                          documentFor(QJsonArray{jsonV1Target(QStringLiteral("other"))},
                                      QStringLiteral("other"), 1));
        CoreTargetStore again(settings);
        QVERIFY(again.load());
        QCOMPARE(again.targets().size(), 2);
        QVERIFY(!again.target(QStringLiteral("other")).has_value());
        QCOMPARE(again.selectedId(), QStringLiteral("second"));
    }

    // Part C fix wave (R2-M3): V1 stays for a build from before V2, but a
    // forgotten Core leaves it, token and all, and an edit reaches it.
    void v1FollowsForgetsAndEditsButGainsNothing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        QJsonObject first = jsonV1Target(QStringLiteral("first"));
        first.insert(QStringLiteral("token"), QStringLiteral("first-secret"));
        QJsonObject second = jsonV1Target(QStringLiteral("second"));
        second.insert(QStringLiteral("token"), QStringLiteral("second-secret"));
        settings.setValue(QLatin1String(kV1Key),
                          documentFor(QJsonArray{first, second}, QStringLiteral("first"), 1));
        QVERIFY(settings.save());
        CoreTargetStore store(settings);
        QVERIFY(store.load());

        const auto v1 = [&path]() {
            AppSettings onDisk(path);
            onDisk.load();
            return QJsonDocument::fromJson(
                       onDisk.value(QLatin1String(kV1Key)).toString().toUtf8())
                .object();
        };

        // Forget "first": gone from V1, its token with it, and V1's
        // selection falls back to this computer's own Core.
        QVERIFY(store.remove(QStringLiteral("first")));
        QJsonObject doc = v1();
        QCOMPARE(doc.value(QStringLiteral("version")).toInt(), 1);
        QCOMPARE(doc.value(QStringLiteral("selectedId")).toString(), QStringLiteral("local"));
        QJsonArray cores = doc.value(QStringLiteral("cores")).toArray();
        QCOMPARE(cores.size(), 1);
        QCOMPARE(cores.at(0).toObject().value(QStringLiteral("id")).toString(),
                 QStringLiteral("second"));
        QVERIFY(!QJsonDocument(doc).toJson().contains("first-secret"));

        // Edit "second": its new token and address reach V1, with no
        // identity key in a V1 record.
        SavedCoreTarget edited = *store.target(QStringLiteral("second"));
        edited.connection.token = QStringLiteral("replaced-secret");
        edited.connection.url = QStringLiteral("wss://moved.example.test:4433");
        edited.connection.identityFingerprint = someIdentity();
        QVERIFY(store.upsert(edited));
        cores = v1().value(QStringLiteral("cores")).toArray();
        QCOMPARE(cores.size(), 1);
        const QJsonObject carried = cores.at(0).toObject();
        QCOMPARE(carried.value(QStringLiteral("token")).toString(),
                 QStringLiteral("replaced-secret"));
        QCOMPARE(carried.value(QStringLiteral("url")).toString(),
                 QStringLiteral("wss://moved.example.test:4433"));
        QVERIFY(!carried.contains(QStringLiteral("identity")));
        QVERIFY(!QJsonDocument(v1()).toJson().contains("second-secret"));

        // A Core added after V2 does not appear in V1.
        QVERIFY(store.upsert(makeTarget(QStringLiteral("third"))));
        QCOMPARE(v1().value(QStringLiteral("cores")).toArray().size(), 1);

        // Its last Core forgotten, V1 is an empty list, still there.
        QVERIFY(store.remove(QStringLiteral("second")));
        AppSettings onDisk(path);
        onDisk.load();
        QVERIFY(onDisk.contains(QLatin1String(kV1Key)));
        QVERIFY(v1().value(QStringLiteral("cores")).toArray().isEmpty());
    }

    void v2FollowsExistingRecordsButKeepsNewServiceTargetsOut()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        QJsonObject first = jsonTarget(QStringLiteral("first"));
        first.insert(QStringLiteral("token"), QStringLiteral("old-secret"));
        const QString original = documentFor(QJsonArray{first}, QStringLiteral("first"), 2);
        settings.setValue(QLatin1String(kV2Key), original);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QCOMPARE(settings.value(QLatin1String(kV2Key)).toString(), original);

        SavedCoreTarget newTarget = makeTarget(QStringLiteral("service"), QString());
        newTarget.connection.url.clear();
        newTarget.connection.fingerprint.clear();
        newTarget.connection.identityFingerprint = someIdentity();
        newTarget.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY(store.upsert(newTarget));
        QCOMPARE(settings.value(QLatin1String(kV2Key)).toString(), original);

        SavedCoreTarget edited = *store.target(QStringLiteral("first"));
        edited.connection.token = QStringLiteral("new-secret");
        QVERIFY(store.upsert(edited));
        QJsonObject older = QJsonDocument::fromJson(
            settings.value(QLatin1String(kV2Key)).toString().toUtf8()).object();
        QCOMPARE(older.value(QStringLiteral("cores")).toArray().size(), 1);
        QCOMPARE(older.value(QStringLiteral("cores")).toArray().first().toObject()
                     .value(QStringLiteral("token")), QJsonValue("new-secret"));
        QVERIFY(!QJsonDocument(older).toJson().contains("old-secret"));

        edited.connection.url.clear();
        edited.connection.token.clear();
        edited.connection.fingerprint.clear();
        edited.connection.identityFingerprint = someIdentity();
        edited.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY(store.upsert(edited));
        older = QJsonDocument::fromJson(
            settings.value(QLatin1String(kV2Key)).toString().toUtf8()).object();
        QVERIFY(older.value(QStringLiteral("cores")).toArray().isEmpty());
        QCOMPARE(older.value(QStringLiteral("selectedId")), QJsonValue("local"));
        QVERIFY(store.remove(QStringLiteral("service")));
        QVERIFY(store.remove(QStringLiteral("first")));
        QVERIFY(!settings.value(QLatin1String(kV2Key)).toString().contains(QStringLiteral("new-secret")));
    }

    void failedV3MutationRestoresBothOlderDocuments()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString blocker = directory.filePath(QStringLiteral("not-a-file"));
        QVERIFY(QDir().mkpath(blocker));
        AppSettings settings(blocker);
        const QString v2 = documentFor(QJsonArray{jsonTarget(QStringLiteral("one"))},
                                       QStringLiteral("one"), 2);
        const QString v3 = documentFor(QJsonArray{jsonTarget(QStringLiteral("one"))},
                                       QStringLiteral("one"), 3);
        settings.setValue(QLatin1String(kV2Key), v2);
        const QString v1 = documentFor(QJsonArray{jsonV1Target(QStringLiteral("one"))},
                                       QStringLiteral("one"), 1);
        settings.setValue(QLatin1String(kV1Key), v1);
        settings.setValue(QLatin1String(kTargetKey), v3);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget changed = *store.target(QStringLiteral("one"));
        changed.connection.token = QStringLiteral("new-secret");
        QString error;
        QVERIFY(!store.upsert(changed, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(settings.value(QLatin1String(kV2Key)).toString(), v2);
        QCOMPARE(settings.value(QLatin1String(kV1Key)).toString(), v1);
        QCOMPARE(settings.value(QLatin1String(kTargetKey)).toString(), v3);
        QCOMPARE(store.target(QStringLiteral("one"))->connection.token, QStringLiteral("secret"));
    }

    // A V1 that cannot be read, beside a V2, is removed at the next change:
    // nothing could keep a forgotten Core's token out of it.
    void anUnreadableV1BesideV2IsRemovedAtTheNextChange()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        settings.setValue(QLatin1String(kV2Key),
                          documentFor(QJsonArray{jsonTarget(QStringLiteral("one"))},
                                      QStringLiteral("one"), 2));
        settings.setValue(QLatin1String(kV1Key), QStringLiteral("{ not a list"));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(settings.contains(QLatin1String(kV1Key)));
        QVERIFY(store.remove(QStringLiteral("one")));
        QVERIFY(!settings.contains(QLatin1String(kV1Key)));
    }

    void anUnreadableV2BesideV3IsRemovedAtTheNextChange()
    {
        QTemporaryDir directory;
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        settings.setValue(QLatin1String(kTargetKey),
                          documentFor(QJsonArray{jsonTarget(QStringLiteral("one"))},
                                      QStringLiteral("one")));
        settings.setValue(QLatin1String(kV2Key), QStringLiteral("{ not a list"));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        QVERIFY(settings.contains(QLatin1String(kV2Key)));
        QVERIFY(store.remove(QStringLiteral("one")));
        QVERIFY(!settings.contains(QLatin1String(kV2Key)));
    }

    // A V1 list that cannot be read writes nothing and is left as it is.
    void unreadableV1ListIsNotMigrated()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        const QString malformed = documentFor(QJsonArray{jsonV1Target(QStringLiteral("one"))},
                                              QStringLiteral("missing"), 1);
        settings.setValue(QLatin1String(kV1Key), malformed);
        CoreTargetStore store(settings);
        QString error;
        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!settings.contains(QLatin1String(kTargetKey)));
        QCOMPARE(settings.value(QLatin1String(kV1Key)).toString(), malformed);
        // A V2 document is not accepted as V1, nor a V1 document as V2.
        settings.setValue(QLatin1String(kV1Key),
                          documentFor(QJsonArray{jsonTarget(QStringLiteral("one"))},
                                      QStringLiteral("one"), 2));
        QVERIFY(!store.load(&error));
        settings.remove(QLatin1String(kV1Key));
        settings.setValue(QLatin1String(kTargetKey),
                          documentFor(QJsonArray{jsonV1Target(QStringLiteral("one"))},
                                      QStringLiteral("one"), 1));
        QVERIFY(!store.load(&error));
    }

    // A V1 migration that cannot be saved fails the load and writes nothing.
    void v1MigrationThatCannotBeSavedFails()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString blocker = directory.filePath(QStringLiteral("not-a-file"));
        QVERIFY(QDir().mkpath(blocker));
        AppSettings settings(blocker);
        settings.setValue(QLatin1String(kV1Key),
                          documentFor(QJsonArray{jsonV1Target(QStringLiteral("one"))},
                                      QStringLiteral("one"), 1));
        CoreTargetStore store(settings);
        QString error;
        QVERIFY(!store.load(&error));
        QVERIFY(!settings.contains(QLatin1String(kTargetKey)));
        QVERIFY(store.targets().isEmpty());
    }

    // The identity round-trips; a Core with none stays a valid entry; a
    // wrong length or unreadable identity is refused, on write and on load.
    void identityRoundTripsAndIsChecked()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        CoreTargetStore store(settings);
        QVERIFY(store.load());

        SavedCoreTarget paired = makeTarget(QStringLiteral("paired"), QString());
        paired.connection.fingerprint.clear();
        paired.connection.identityFingerprint = someIdentity();
        const SavedCoreTarget tokenOnly = makeTarget(QStringLiteral("token-only"));
        QVERIFY(store.upsert(paired));
        QVERIFY(store.upsert(tokenOnly));

        AppSettings reloadedSettings(path);
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.target(QStringLiteral("paired"))->connection.identityFingerprint,
                 paired.connection.identityFingerprint);
        QVERIFY(reloaded.target(QStringLiteral("token-only"))->connection.identityFingerprint
                    .isEmpty());
        QCOMPARE(reloaded.target(QStringLiteral("token-only"))->connection.token,
                 QStringLiteral("token"));

        SavedCoreTarget shortIdentity = paired;
        shortIdentity.connection.identityFingerprint.chop(1);
        QString error;
        QVERIFY(!store.upsert(shortIdentity, &error));
        QVERIFY(!error.isEmpty());

        const QString goodIdentity = StationIdentity::toBase64Url(someIdentity());
        settings.setValue(QLatin1String(kTargetKey),
                          documentFor(QJsonArray{jsonTarget(QStringLiteral("one"), goodIdentity)},
                                      QStringLiteral("one")));
        QVERIFY(store.load());
        QCOMPARE(store.target(QStringLiteral("one"))->connection.identityFingerprint.size(), 32);

        for (const QString& bad : {QStringLiteral("not base64!"),
                                   StationIdentity::toBase64Url(QByteArray(31, 'x')),
                                   goodIdentity + QStringLiteral("=")}) {
            settings.setValue(QLatin1String(kTargetKey),
                              documentFor(QJsonArray{jsonTarget(QStringLiteral("one"), bad)},
                                          QStringLiteral("one")));
            QVERIFY2(!store.load(&error), qPrintable(bad));
        }
        QJsonObject noIdentityKey = jsonTarget(QStringLiteral("one"));
        noIdentityKey.remove(QStringLiteral("identity"));
        settings.setValue(QLatin1String(kTargetKey),
                          documentFor(QJsonArray{noIdentityKey}, QStringLiteral("one")));
        QVERIFY(!store.load(&error));
    }

    // iPhone app plan Task 28 fix wave (review Important 5): the Core's
    // controlChannelVersion is kept with it: absent until a sign-in
    // records it, round-tripped, refused when it is not a whole number.
    void controlChannelVersionIsRecordedWithTheCore()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        qint64 now = 1000000;
        CoreTargetStore store(settings, [&now] { return now; });
        QVERIFY(store.load());
        SavedCoreTarget paired = makeTarget(QStringLiteral("paired"), QString());
        paired.connection.fingerprint.clear();
        paired.connection.identityFingerprint = someIdentity();
        QVERIFY(store.upsert(paired));
        QCOMPARE(store.target(QStringLiteral("paired"))->connection.controlChannelVersion, -1);
        QVERIFY(!settings.value(QLatin1String(kTargetKey)).toString()
                     .contains(QLatin1String("controlChannelVersion")));

        QVERIFY(store.rememberControlChannelVersion(QStringLiteral("paired"), 1));
        AppSettings reloadedSettings(path);
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.target(QStringLiteral("paired"))->connection.controlChannelVersion, 1);
        QVERIFY(store.rememberControlChannelVersion(QStringLiteral("paired"), 0));
        QCOMPARE(store.target(QStringLiteral("paired"))->connection.controlChannelVersion, 0);
        QCOMPARE(store.target(QStringLiteral("paired"))->connection.negativeControlObservedMs, now);
        QVERIFY(!store.rememberControlChannelVersion(QStringLiteral("nobody"), 1));

        QString error;
        for (const QJsonValue& bad : {QJsonValue(QStringLiteral("1")), QJsonValue(-1),
                                      QJsonValue(1.5), QJsonValue(true)}) {
            QJsonObject record = jsonTarget(QStringLiteral("one"),
                                            StationIdentity::toBase64Url(someIdentity()));
            record.insert(QStringLiteral("controlChannelVersion"), bad);
            settings.setValue(QLatin1String(kTargetKey),
                              documentFor(QJsonArray{record}, QStringLiteral("one")));
            QVERIFY(!store.load(&error));
        }
    }

    // Connecting from anywhere is offered to a paired Core that declared
    // the control channel or has had no session yet, and refused in plain
    // words otherwise.
    void connectingFromAnywhereIsOfferedOnlyToACoreThatTakesIt()
    {
        RemoteStationOptions options;
        options.url = QStringLiteral("wss://192.0.2.5:47910");
        QCOMPARE(options.serviceConnectRefusal(),
                 QStringLiteral("Pair with the Core to reach it from anywhere."));
        options.identityFingerprint = someIdentity();
        // Task 29 fix wave (Minor 4): not before the Core's service name is
        // known from a sign-in.
        QCOMPARE(options.serviceConnectRefusal(),
                 QStringLiteral("Connect to the Core once to reach it from anywhere."));
        QVERIFY(NereusSDR::OperatorWording::isPlain(options.serviceConnectRefusal()));
        options.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY(options.serviceConnectRefusal().isEmpty());  // nothing recorded yet
        options.controlChannelVersion = 1;
        QVERIFY(options.serviceConnectRefusal().isEmpty());
        options.controlChannelVersion = 0;
        options.negativeControlObservedMs = 1000000;
        QCOMPARE(options.serviceConnectRefusal(1000000),
                 QStringLiteral("Update the Core to reach it from anywhere."));
        QVERIFY(NereusSDR::OperatorWording::isPlain(options.serviceConnectRefusal(1000000)));
        QVERIFY(options.serviceConnectRefusal(1300000).isEmpty());
        QVERIFY(options.serviceConnectRefusal(999999).isEmpty());
    }

    void authenticatedNegativeObservationAgesAndNetworkChangeInvalidatesOnce()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        qint64 now = 1000000;
        CoreTargetStore store(settings, [&now] { return now; });
        QVERIFY(store.load());
        SavedCoreTarget paired = makeTarget(QStringLiteral("paired"), QString());
        paired.connection.identityFingerprint = someIdentity();
        paired.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        paired.connection.relayAllowed = 0;
        QVERIFY(store.upsert(paired));
        QVERIFY(store.observeNetworkFingerprint(QStringLiteral("network-a")));
        QVERIFY(store.rememberControlChannelVersion(paired.id, 0));
        QCOMPARE(store.target(paired.id)->connection.effectiveControlChannelVersion(now), 0);
        AppSettings freshSettings(path);
        freshSettings.load();
        CoreTargetStore fresh(freshSettings, [&now] { return now; });
        QVERIFY(fresh.load());
        QCOMPARE(fresh.target(paired.id)->connection.effectiveControlChannelVersion(now), 0);
        now += 299999;
        QCOMPARE(store.target(paired.id)->connection.effectiveControlChannelVersion(now), 0);
        QVERIFY(store.observeNetworkFingerprint(QStringLiteral("network-a")));
        QCOMPARE(store.target(paired.id)->connection.negativeControlObservedMs, 1000000);
        now += 1;
        QCOMPARE(store.target(paired.id)->connection.effectiveControlChannelVersion(now), -1);
        QVERIFY(store.rememberControlChannelVersion(paired.id, 0));
        QCOMPARE(store.target(paired.id)->connection.negativeControlObservedMs, now);
        now += 1000;
        QVERIFY(store.rememberControlChannelVersion(paired.id, 0));
        QCOMPARE(store.target(paired.id)->connection.negativeControlObservedMs, now);
        QVERIFY(store.observeNetworkFingerprint(QStringLiteral("network-b")));
        QCOMPARE(store.target(paired.id)->connection.effectiveControlChannelVersion(now), -1);
        QVERIFY(store.observeNetworkFingerprint(QStringLiteral("network-b")));
        QCOMPARE(store.target(paired.id)->connection.negativeControlObservedMs, -1);
        QCOMPARE(store.target(paired.id)->connection.relayAllowed, 0);

        AppSettings reloadedSettings(path);
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings, [&now] { return now; });
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.target(paired.id)->connection.controlChannelVersion, 0);
        QCOMPARE(reloaded.target(paired.id)->connection.effectiveControlChannelVersion(now), -1);
        QVERIFY(reloaded.rememberControlChannelVersion(paired.id, 1));
        QCOMPARE(reloaded.target(paired.id)->connection.negativeControlObservedMs, -1);
        QCOMPARE(reloaded.target(paired.id)->connection.effectiveControlChannelVersion(now), 1);
    }

    void legacyMalformedAndFutureObservationsNeverBlockDiscovery()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        const qint64 now = 1000000;
        for (const QJsonValue& stamp : {QJsonValue(), QJsonValue(QStringLiteral("bad")),
                                        QJsonValue(1.5), QJsonValue(-1), QJsonValue(1000001)}) {
            QJsonObject record = jsonTarget(QStringLiteral("paired"),
                                            StationIdentity::toBase64Url(someIdentity()));
            record.insert(QStringLiteral("controlChannelVersion"), 0);
            if (!stamp.isUndefined()) {
                record.insert(QStringLiteral("negativeControlObservedMs"), stamp);
            }
            record.insert(QStringLiteral("relayAllowed"), false);
            settings.setValue(QLatin1String(kTargetKey),
                              documentFor(QJsonArray{record}, QStringLiteral("paired")));
            CoreTargetStore store(settings, [now] { return now; });
            QVERIFY(store.load());
            const auto target = store.target(QStringLiteral("paired"));
            QVERIFY(target.has_value());
            QCOMPARE(target->connection.effectiveControlChannelVersion(now), -1);
            QCOMPARE(target->connection.relayAllowed, 0);
            if (stamp == QJsonValue(1000001)) {
                QCOMPARE(target->connection.negativeControlObservedMs, -1);
            }
        }
    }

    void clockRollbackDiscardsAnAlreadyRecordedNegative()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        qint64 now = 1000000;
        CoreTargetStore store(settings, [&now] { return now; });
        QVERIFY(store.load());
        SavedCoreTarget paired = makeTarget(QStringLiteral("paired"), QString());
        paired.connection.identityFingerprint = someIdentity();
        QVERIFY(store.upsert(paired));
        QVERIFY(store.rememberControlChannelVersion(paired.id, 0));
        now -= 1000;
        QVERIFY(store.invalidateFutureNegativeObservations());
        QCOMPARE(store.target(paired.id)->connection.negativeControlObservedMs, -1);
        now += 1000;
        QCOMPARE(store.target(paired.id)->connection.effectiveControlChannelVersion(now), -1);
    }

    // iPhone app plan Task 29 (R-IOS-16): the Core's rendezvous id and
    // relay setting are kept with it once a sign-in told them, the
    // operator's choice to reach it through the service only when it is
    // off; each round-trips and a bad value is refused.
    void theServiceRouteIsRecordedWithTheCore()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(path);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget paired = makeTarget(QStringLiteral("paired"), QString());
        paired.connection.fingerprint.clear();
        paired.connection.identityFingerprint = someIdentity();
        QVERIFY(store.upsert(paired));
        QVERIFY(store.target(QStringLiteral("paired"))->connection.rendezvousId.isEmpty());
        QCOMPARE(store.target(QStringLiteral("paired"))->connection.relayAllowed, -1);
        QVERIFY(store.target(QStringLiteral("paired"))->connection.reachFromAnywhere);
        const QString record = settings.value(QLatin1String(kTargetKey)).toString();
        QVERIFY(!record.contains(QLatin1String("rendezvousId")));
        QVERIFY(!record.contains(QLatin1String("relayAllowed")));
        QVERIFY(!record.contains(QLatin1String("reachFromAnywhere")));

        const QString id = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        QVERIFY(store.rememberServiceRoute(QStringLiteral("paired"), id, 0));
        // Not a rendezvous id: ignored, the recorded one kept.
        QVERIFY(store.rememberServiceRoute(QStringLiteral("paired"), QStringLiteral("nope"), -1));
        SavedCoreTarget off = *store.target(QStringLiteral("paired"));
        off.connection.reachFromAnywhere = false;
        QVERIFY(store.upsert(off));
        AppSettings reloadedSettings(path);
        reloadedSettings.load();
        CoreTargetStore reloaded(reloadedSettings);
        QVERIFY(reloaded.load());
        const RemoteStationOptions options = reloaded.target(QStringLiteral("paired"))->connection;
        QCOMPARE(options.rendezvousId, id);
        QCOMPARE(options.relayAllowed, 0);
        QVERIFY(!options.reachFromAnywhere);
        QVERIFY(!store.rememberServiceRoute(QStringLiteral("nobody"), id, 1));

        QString error;
        for (const auto& [key, bad] :
             {std::pair{QStringLiteral("rendezvousId"), QJsonValue(QStringLiteral("short"))},
              std::pair{QStringLiteral("rendezvousId"), QJsonValue(7)},
              std::pair{QStringLiteral("relayAllowed"), QJsonValue(1)},
              std::pair{QStringLiteral("reachFromAnywhere"), QJsonValue(QStringLiteral("no"))}}) {
            QJsonObject badRecord = jsonTarget(QStringLiteral("one"),
                                                StationIdentity::toBase64Url(someIdentity()));
            badRecord.insert(key, bad);
            settings.setValue(QLatin1String(kTargetKey),
                              documentFor(QJsonArray{badRecord}, QStringLiteral("one")));
            QVERIFY2(!store.load(&error), qPrintable(key));
        }
    }
};

QTEST_APPLESS_MAIN(TstCoreTargetStore)

#include "tst_core_target_store.moc"
