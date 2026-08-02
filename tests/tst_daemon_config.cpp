// =================================================================
// tests/tst_daemon_config.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R1 Task 9: DaemonConfig parses nereusd's own "key = value" config file
// (default /etc/nereusd.conf, overridable with --config). Test bodies are
// the brief's own (task-9-brief.md Step 1) verbatim.
//
// R1 Task 9 fix round 1: three more slots pin resolveDaemonProfileArgument
// (--profile support, closing the gap described in DaemonConfig.h's own
// comment on that function and task-9-report.md section 5). This is new
// glue logic this task wrote, not a re-test of AppSettings::
// isValidProfileName()'s own accept/reject rules -- those already have
// dedicated coverage in tests/tst_app_settings_profile.cpp.
// =================================================================

#include <QtTest>
#include <QTemporaryFile>
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstDaemonConfig : public QObject {
    Q_OBJECT
private slots:
    void defaultsAreValid()
    {
        QString err;
        QVERIFY(DaemonConfig::defaults().validate(&err));
        QVERIFY2(err.isEmpty(), qPrintable(err));
    }

    void parsesAWellFormedFile()
    {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("radio_mac = 00:1C:2D:05:37:2A\n"
                "sample_rate_hz = 384000\n"
                "slice_count = 3\n"
                "log_level = debug\n");
        f.flush();
        QString err;
        DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.radioMac, QStringLiteral("00:1C:2D:05:37:2A"));
        QCOMPARE(c.sampleRateHz, 384000);
        QCOMPARE(c.sliceCount, 3);
        QCOMPARE(c.logLevel, QStringLiteral("debug"));
    }

    void rejectsSliceCountBelowOne()
    {
        DaemonConfig c = DaemonConfig::defaults();
        c.sliceCount = 0;
        QString err;
        QVERIFY(!c.validate(&err));
        QVERIFY(!err.isEmpty());
    }

    void missingFileYieldsDefaultsAndAnError()
    {
        QString err;
        DaemonConfig c = DaemonConfig::fromFile("/nonexistent/nereusd.conf", &err);
        QVERIFY(!err.isEmpty());
        QCOMPARE(c.sliceCount, DaemonConfig::defaults().sliceCount);
    }

    void emptyProfileArgumentMeansNoProfile()
    {
        QString err;
        const QString profile = resolveDaemonProfileArgument(QString(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(profile.isEmpty());
    }

    void validProfileNameIsAccepted()
    {
        QString err;
        const QString profile = resolveDaemonProfileArgument(QStringLiteral("hf"), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(profile, QStringLiteral("hf"));
    }

    void invalidProfileNameIsRejected()
    {
        QString err;
        const QString profile =
            resolveDaemonProfileArgument(QStringLiteral("with space"), &err);
        QVERIFY(!err.isEmpty());
        QVERIFY(profile.isEmpty());
    }
};

QTEST_MAIN(TstDaemonConfig)
#include "tst_daemon_config.moc"
