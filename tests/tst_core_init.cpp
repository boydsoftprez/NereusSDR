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
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
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
        const QString directory = AppSettings::resolveConfigDir(profile);
        QVERIFY(QDir().mkpath(directory));
        QFile legacy(QDir(directory).filePath("nereussdr-99991231-235959.log"));
        QVERIFY(legacy.open(QIODevice::WriteOnly));
        QCOMPARE(legacy.write(QByteArray(33 * 1024 * 1024, 'x') + "\nlegacy-recent-diagnostic\n"),
                 qint64(33 * 1024 * 1024 + 26));
        legacy.close();
        const auto cleanup = qScopeGuard([&]() { legacy.close(); QFile::remove(legacy.fileName()); });
        QVERIFY(CoreInit::initialize(profile));
        // The production startup path must bound legacy logs as well as
        // use the sink's rotation; a restarted profile retains its tail.
        QVERIFY(QFileInfo::exists(legacy.fileName()));
        QVERIFY(QFileInfo(legacy.fileName()).size() <= 32 * 1024 * 1024);
        QVERIFY(legacy.open(QIODevice::ReadOnly));
        QVERIFY(legacy.seek(qMax(qint64(0), legacy.size() - 100)));
        QVERIFY(legacy.readAll().endsWith("legacy-recent-diagnostic\n"));
        legacy.close();
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
    // PASS on the >= 8 assertion below. Anyone adding that save() should
    // also give it its own isolated AppSettings(tempPath) instance, the way
    // tst_settings_migration_v0_3_0.cpp already does.
    void migratesSettingsToCurrentVersion()
    {
        QVERIFY(NereusSDR::CoreInit::initialize());
        const QString v = NereusSDR::AppSettings::instance()
                              .value("SettingsSchemaVersion", "0").toString();
        QVERIFY(v.toInt() >= 8);
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

    void cleanupTestCase()
    {
        CoreInit::shutdown();
        QVERIFY(!QFileInfo::exists(QDir(AppSettings::resolveConfigDir("r1task8test"))
                                      .filePath("nereussdr-log.lock")));
    }

    // ---- redactPii (Remote Daemon R2, security fix round) ---------------
    //
    // The message handler initialize() installs passes every Qt log message
    // through redactPii() first. Its MAC rule used to be a bare six-pair
    // hex body with no boundary guard, which is a strict PREFIX of a
    // colon-separated SHA-256 certificate fingerprint
    // (CertificateStore::fingerprintSha256(), 32 pairs) -- so the rule
    // matched five times over inside one fingerprint and replaced 25 of its
    // 32 bytes with asterisks, on stderr and in the on-disk log alike.
    // These two slots pin both halves of the narrowing: a fingerprint
    // survives, and genuine MAC redaction is unchanged.

    void redactionLeavesATlsFingerprintIntact()
    {
        // Byte-for-byte the shape CertificateStore::fingerprintSha256()
        // emits: 32 colon-separated uppercase hex pairs.
        const QString fingerprint = QStringLiteral(
            "9F:2A:07:BE:41:5C:D3:88:10:6E:AB:33:74:CF:29:5D:"
            "E8:01:96:B2:47:0F:AD:5E:C1:38:72:9A:0B:E4:56:7D");
        QCOMPARE(NereusSDR::CoreInit::redactPiiForTest(fingerprint), fingerprint);

        // And on a line that carries BOTH: the fingerprint survives, the
        // MAC does not. This is the real production shape -- the daemon's
        // own diagnostics name a radio's MAC and a station fingerprint in
        // the same breath.
        const QString mac = QStringLiteral("00:1C:2D:05:37:2A");
        const QString line =
            QStringLiteral("station %1 radio %2 up").arg(fingerprint, mac);
        const QString out = NereusSDR::CoreInit::redactPiiForTest(line);
        QVERIFY2(out.contains(fingerprint),
                 qPrintable(QStringLiteral("fingerprint was mangled: %1").arg(out)));
        QVERIFY2(!out.contains(mac),
                 qPrintable(QStringLiteral("MAC survived redaction: %1").arg(out)));
        QVERIFY(out.contains(QStringLiteral("**:**:**:**:**:2A")));
    }

    void redactionStillHidesAMacInEveryShapeWeLogOne()
    {
        // Bare, colon-separated.
        QCOMPARE(NereusSDR::CoreInit::redactPiiForTest(
                     QStringLiteral("00:1C:2D:05:37:2A")),
                 QStringLiteral("**:**:**:**:**:2A"));

        // Dash-separated, the other form the rule has always accepted.
        QCOMPARE(NereusSDR::CoreInit::redactPiiForTest(
                     QStringLiteral("00-1C-2D-05-37-2A")),
                 QStringLiteral("**:**:**:**:**:2A"));

        // Inside an AppSettings key, which is how nearly every MAC in this
        // tree reaches a log line ("hardware/<mac>/..." is 92 percent of a
        // real settings file, R2 design addendum section 8).
        QCOMPARE(NereusSDR::CoreInit::redactPiiForTest(
                     QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/radioInfo/sampleRate")),
                 QStringLiteral("hardware/**:**:**:**:**:FF/radioInfo/sampleRate"));

        // After a word and a hyphen. The narrowing must not treat the
        // hyphen as evidence of a longer hex run.
        QCOMPARE(NereusSDR::CoreInit::redactPiiForTest(
                     QStringLiteral("adapter-00:1C:2D:05:37:2A")),
                 QStringLiteral("adapter-**:**:**:**:**:2A"));

        // IPv4 redaction is untouched by any of this.
        QCOMPARE(NereusSDR::CoreInit::redactPiiForTest(
                     QStringLiteral("bound 192.168.50.121")),
                 QStringLiteral("bound *.*.*. 121"));
    }
};

QTEST_MAIN(TstCoreInit)
#include "tst_core_init.moc"
