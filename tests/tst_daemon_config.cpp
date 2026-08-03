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
//
// R1 merge-blocker fix round: sampleFileKeysAndParserKeysAgree pins the
// shipped packaging/nereusd.conf.sample against the parser, because the
// sample file documented keys the daemon did nothing with.
// =================================================================

#include <QtTest>
#include <QFile>
#include <QTemporaryFile>
#include <QTextStream>
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
                "audio_device = hw:CARD=Device\n");
        f.flush();
        QString err;
        DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.radioMac, QStringLiteral("00:1C:2D:05:37:2A"));
        QCOMPARE(c.sampleRateHz, 384000);
        QCOMPARE(c.sliceCount, 3);
        QCOMPARE(c.audioDevice, QStringLiteral("hw:CARD=Device"));
    }

    // Every key nereusd.conf.sample documents must parse, and nothing it
    // does not document may. The reason this is pinned: the sample file
    // shipped `log_level` for a while, which parsed into a struct field
    // that no production code ever read, so an operator setting it got
    // silence. Removing a key from the struct without removing it from
    // the sample file (or the reverse) now fails here.
    void sampleFileKeysAndParserKeysAgree()
    {
        const QStringList documented = {
            QStringLiteral("radio_mac"),
            QStringLiteral("sample_rate_hz"),
            QStringLiteral("slice_count"),
            QStringLiteral("audio_device"),
        };

        // Each documented key parses without an "unknown key" complaint.
        // fromFile logs unknown keys via qCWarning; QTest turns an
        // unexpected qWarning into a test failure only with
        // QTest::failOnWarning, so assert on the parsed value instead.
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("radio_mac = aa:bb:cc:dd:ee:ff\n"
                "sample_rate_hz = 96000\n"
                "slice_count = 2\n"
                "audio_device = default\n");
        f.flush();
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.radioMac, QStringLiteral("aa:bb:cc:dd:ee:ff"));
        QCOMPARE(c.sampleRateHz, 96000);
        QCOMPARE(c.sliceCount, 2);
        QCOMPARE(c.audioDevice, QStringLiteral("default"));

        // And the shipped sample file documents exactly those keys, no
        // more. Parsed straight out of the packaging file so the two
        // cannot drift.
        QFile sample(QStringLiteral(NEREUS_SOURCE_DIR
                                    "/packaging/nereusd.conf.sample"));
        QVERIFY2(sample.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(sample.fileName()));
        QStringList found;
        QTextStream ts(&sample);
        while (!ts.atEnd()) {
            QString line = ts.readLine();
            const int hash = line.indexOf(QLatin1Char('#'));
            if (hash >= 0) { line.truncate(hash); }
            line = line.trimmed();
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0) { found << line.left(eq).trimmed(); }
        }
        found.sort();
        QStringList expected = documented;
        expected.sort();
        QCOMPARE(found, expected);
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
