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

    // Invariant this test depends on: no other test persists
    // SettingsSchemaVersion to the shared QStandardPaths test sandbox
    // before this one runs. `tests/tst_settings_schema_v6_migration.cpp`
    // shares this exact key, sets it to "5" and to "6" via the SAME
    // default-profile AppSettings::instance() singleton this test reads
    // (not a local direct-constructed instance), and registers earlier in
    // tests/CMakeLists.txt, so it runs first under a plain sequential
    // `ctest` (no -j, no random scheduling).
    //
    // That is currently harmless only because nothing in the chain calls
    // .save(): AppSettings::setValue()/remove() are pure in-memory QMap
    // operations, AppSettings::load() does not write on the file-missing
    // path, ensureSettingsAtVersion() never calls save() at all, and the
    // other three migration helpers (migrateVaxSchemaV1ToV2,
    // migrateLegacyN2adrFilter, removeOrphanOcN2adrFilter) each early-return
    // before their own conditional save() when the store is fresh. So
    // tst_settings_schema_v6_migration's in-memory writes die with its
    // process and never reach the on-disk sandbox file this test's
    // CoreInit::initialize() -> AppSettings::instance().load() call reads.
    //
    // If a future edit adds a .save() to tst_settings_schema_v6_migration.cpp
    // (reasonable, to check a real round-trip), that mechanism breaks
    // silently: this test would load an already-migrated store and never
    // exercise CoreInit's actual migration path, while still reporting
    // PASS on the >= 6 assertion below. Anyone adding that save() should
    // also give it its own isolated AppSettings(tempPath) instance, the way
    // tst_settings_migration_v0_3_0.cpp already does.
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
