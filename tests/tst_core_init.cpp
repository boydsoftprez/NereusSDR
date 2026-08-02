// =================================================================
// tests/tst_core_init.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R1 Task 8: CoreInit bundles the settings migration sequence and the
// file-logging setup that used to live inline in main(), each with exactly
// one call site, so a daemon-first install (src/server_main.cpp, R1 Task 9)
// runs against a migrated settings store instead of a raw one.
//
// Slot order matters here. CoreInit::initialize() is guarded by a
// file-static flag that lives for this test binary's whole process, so
// only the FIRST slot's call to initialize() actually runs the body;
// every later call in this file is a guaranteed no-op. Declaration order
// below puts the profile-argument check first, because it is the only
// slot that can observe the real run.
// =================================================================

#include <QtTest>
#include <QDir>
#include <QStringList>
#include "core/CoreInit.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstCoreInit : public QObject {
    Q_OBJECT
private slots:
    // Must run first (see class comment): the only call in this binary
    // that reaches CoreInit::initialize()'s real body, so it is the only
    // one that can prove the profile argument drives the log directory
    // independently of AppSettings::profileOverride() (coordinator concern:
    // a daemon must be able to pass its own profile without replaying
    // main()'s pre-QApplication argv scan).
    void initializeUsesGivenProfileForLogDirectory()
    {
        const QString profile = QStringLiteral("r1task8test");
        QVERIFY(CoreInit::initialize(profile));
        QCOMPARE(CoreInit::initializeRunCount(), 1);

        const QString logDir = AppSettings::resolveConfigDir(profile);
        QDir dir(logDir);
        const QStringList logs = dir.entryList({QStringLiteral("nereussdr-*.log")},
                                                QDir::Files, QDir::Name);
        QVERIFY2(!logs.isEmpty(),
                 qPrintable(QStringLiteral("expected a log file under %1").arg(logDir)));
    }

    void migratesSettingsToCurrentVersion()
    {
        QVERIFY(NereusSDR::CoreInit::initialize());
        const QString v = NereusSDR::AppSettings::instance()
                              .value("SettingsSchemaVersion", "0").toString();
        QVERIFY(v.toInt() >= 6);
    }

    void isIdempotent()
    {
        QVERIFY(NereusSDR::CoreInit::initialize());
        QVERIFY(NereusSDR::CoreInit::initialize());
        // Both calls above land after the guard already tripped in the
        // first slot, so a call count still pinned at 1 is the genuine-
        // no-op claim, not just "every migration happens to be idempotent
        // on replay".
        QCOMPARE(NereusSDR::CoreInit::initializeRunCount(), 1);
    }
};

QTEST_MAIN(TstCoreInit)
#include "tst_core_init.moc"
