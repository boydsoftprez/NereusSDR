#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/settings/SettingsBackup.h"
#include "core/settings/SettingsProxy.h"

using namespace NereusSDR;

static QByteArray bytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

class TstSettingsBackupCodec : public QObject {
    Q_OBJECT
private slots:
    void localRoundTripAndProxyIsolation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings source(dir.filePath("source.settings"));
        source.setValue("hardware/aa:bb/[0]+", "local & value");
        source.setStationName("NereusSDR");
        source.setStationValue("Station/Choice", "saved");
        SettingsProxy proxy;
        proxy.applySnapshot({{"hardware/aa:bb/[0]+", "remote"}});
        source.setRemoteBackend(&proxy);
        QCOMPARE(source.value("hardware/aa:bb/[0]+").toString(), QString("remote"));
        QString error;
        const QByteArray xml = source.exportLocalXml(&error);
        QVERIFY2(!xml.isEmpty(), qPrintable(error));
        QVERIFY(AppSettings::validateLocalXml(xml, &error));
        QVERIFY(xml.contains("local &amp; value"));
        QVERIFY(!xml.contains("remote"));

        AppSettings restored(dir.filePath("restored.settings"));
        QVERIFY2(restored.importLocalXml(xml, &error), qPrintable(error));
        AppSettings reload(restored.filePath());
        reload.load();
        QCOMPARE(reload.value("hardware/aa:bb/[0]+").toString(), QString("local & value"));
        QCOMPARE(reload.stationValue("Station/Choice").toString(), QString("saved"));
        QCOMPARE(reload.stationName(), QString("NereusSDR"));
    }

    void invalidImportPreservesStore()
    {
        QTemporaryDir dir;
        AppSettings store(dir.filePath("settings.xml"));
        store.setValue("Keep", "old");
        QVERIFY(store.save());
        const QByteArray original = bytes(store.filePath());
        const QList<QByteArray> bad = {
            {}, "<wrong/>", "<NereusSDR>",
            "<!DOCTYPE NereusSDR [<!ENTITY x 'bad'>]><NereusSDR><A>&x;</A></NereusSDR>",
            "<NereusSDR><A><B>nested</B></A></NereusSDR>",
            "<NereusSDR><A>one</A><A>two</A></NereusSDR>",
            "<NereusSDR><a__s__b>one</a__s__b><a__s__b>two</a__s__b></NereusSDR>",
            "<NereusSDR><One type='station'/><Two type='station'/></NereusSDR>"
        };
        for (const QByteArray& xml : bad) {
            QString error;
            QVERIFY(!AppSettings::validateLocalXml(xml, &error));
            QVERIFY(!error.isEmpty());
            QVERIFY(!store.importLocalXml(xml, &error));
            QCOMPARE(bytes(store.filePath()), original);
            QCOMPARE(store.value("Keep").toString(), QString("old"));
        }
        QByteArray oversized(16 * 1024 * 1024 + 1, 'x');
        QString error;
        QVERIFY(!AppSettings::validateLocalXml(oversized, &error));
        QVERIFY(!store.importLocalXml(oversized, &error));
        QCOMPARE(bytes(store.filePath()), original);
    }

    void replacementRefusesLiveOwnerAndFailedDestination()
    {
        QTemporaryDir dir;
        AppSettings source(dir.filePath("source.xml"));
        source.setValue("New", "value");
        const QByteArray xml = source.exportLocalXml();
        AppSettings store(dir.filePath("store.xml"));
        store.setValue("Keep", "old");
        QVERIFY(store.save());
        const QByteArray original = bytes(store.filePath());
        SettingsProxy proxy;
        QString error;
        store.setRemoteBackend(&proxy);
        QVERIFY(!store.importLocalXml(xml, &error));
        store.setRemoteBackend(nullptr);
        store.setChangeHook([](const QString&) {});
        QVERIFY(!store.importLocalXml(xml, &error));
        store.setChangeHook({});
        QCOMPARE(bytes(store.filePath()), original);
        QCOMPARE(store.value("Keep").toString(), QString("old"));

        AppSettings blocked(dir.filePath("missing-parent/store.xml"));
        blocked.setValue("Keep", "old");
        // A non-directory parent defeats QSaveFile without mutating maps.
        QFile obstruction(dir.filePath("missing-parent"));
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        obstruction.close();
        QVERIFY(!blocked.importLocalXml(xml, &error));
        QCOMPARE(blocked.value("Keep").toString(), QString("old"));
    }

    void bundleValidationAndAtomicFile()
    {
        QTemporaryDir dir;
        const QByteArray xml = "<?xml version=\"1.0\"?><NereusSDR/>";
        const SettingsBackup wanted{xml, xml};
        QByteArray encoded;
        QString error;
        QVERIFY(SettingsBackup::encode(wanted, &encoded, &error));
        SettingsBackup decoded;
        QVERIFY(SettingsBackup::decode(encoded, &decoded, &error));
        QCOMPARE(decoded.windowXml, xml);
        QCOMPARE(decoded.coreXml, xml);
        const QList<QByteArray> invalid = {
            "{}", "[]", "{", "{\"format\":\"nereus-settings-backup\",\"version\":2,\"windowXml\":\"x\",\"coreXml\":\"y\"}",
            "{\"format\":\"nereus-settings-backup\",\"version\":1,\"windowXml\":42,\"coreXml\":\"y\"}",
            "{\"format\":\"nereus-settings-backup\",\"version\":1,\"windowXml\":\"x\"}",
            "{\"format\":\"nereus-settings-backup\",\"version\":1,\"windowXml\":\"x\",\"coreXml\":\"y\",\"other\":0}"
        };
        for (const QByteArray& candidate : invalid) {
            SettingsBackup output{QByteArray("keep"), QByteArray("keep")};
            QVERIFY(!SettingsBackup::decode(candidate, &output, &error));
            QCOMPARE(output.windowXml, QByteArray("keep"));
            QVERIFY(!error.isEmpty());
        }
        QByteArray oversized(SettingsBackup::kMaxXmlBytes + 1, 'x');
        QVERIFY(!SettingsBackup::encode({oversized, xml}, &encoded, &error));
        const QString path = dir.filePath("backup.json");
        QVERIFY(SettingsBackup::writeFile(path, wanted, &error));
        const QByteArray original = bytes(path);
        QVERIFY(!SettingsBackup::writeFile(path, {oversized, xml}, &error));
        QCOMPARE(bytes(path), original);
        QVERIFY(SettingsBackup::readFile(path, &decoded, &error));
        QCOMPARE(decoded.coreXml, xml);
    }
};

QTEST_MAIN(TstSettingsBackupCodec)
#include "tst_settings_backup_codec.moc"
