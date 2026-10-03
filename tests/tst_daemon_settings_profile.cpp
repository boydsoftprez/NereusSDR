// =================================================================
// tests/tst_daemon_settings_profile.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2, Task 1: nereusd used to share the GUI client's own
// settings/log directory whenever --profile was omitted entirely (R1
// Task 9's resolveDaemonProfileArgument() treated "absent" and
// "explicitly empty" identically -- both are "" out of
// QCommandLineParser::value() once profileOpt has no default). That made
// R2's state-mirroring feature (SettingsProxy, Task 15) able to pass its
// own verification while doing nothing: a "remote" GUI reading the same
// on-disk file the local daemon just wrote looks correct whether or not
// anything was actually mirrored over the wire. See
// docs/architecture/2026-08-03-remote-daemon-r2-r3-design-addendum.md
// §2.1.
//
// This file is the seam test for the fix: a bool `wasSet` parameter
// (== parser.isSet(profileOpt)) lets resolveDaemonProfileArgument() tell
// "no --profile at all" apart from "--profile with an empty value", and
// the two now resolve differently --
//   wasSet == false            -> AppSettings::kDaemonProfileName ("daemon")
//   wasSet == true,  empty     -> "" (still shares, deliberate escape hatch)
// -- proves that threading the resolved profile string through both
// AppSettings::setProfileOverride() and CoreInit::initialize(), exactly
// as src/server_main.cpp:186-197 does, moves the log directory together
// with the settings path instead of the two drifting apart, and pins the
// first-run seed marker Task 15's Setup gate will read to tell "this
// profile's settings snapshot is empty because it is a legitimately
// fresh daemon profile" apart from "something is broken".
//
// tests/tst_daemon_config.cpp keeps the resolveDaemonProfileArgument()
// accept/valid/invalid-name coverage (unchanged by this task); this file
// does not repeat it.
// =================================================================

#include <QtTest>
#include <QFileInfo>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/CoreInit.h"
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstDaemonSettingsProfile : public QObject {
    Q_OBJECT
private slots:
    // Pins the exact literal values callers (and Task 15) depend on.
    void reservedProfileConstantsHaveTheSpecifiedValues()
    {
        QCOMPARE(QString(AppSettings::kDaemonProfileName), QStringLiteral("daemon"));
        QCOMPARE(QString(AppSettings::kDaemonProfileSeededKey),
                 QStringLiteral("DaemonProfileSeeded"));
    }

    // No --profile on the command line at all: reserve nereusd's own
    // profile rather than silently sharing the GUI's directory.
    void absentProfileArgumentResolvesToReservedDaemonProfile()
    {
        QString err;
        const QString profile = resolveDaemonProfileArgument(QString(), false, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(profile, QString(AppSettings::kDaemonProfileName));
    }

    // The escape hatch: --profile explicitly given an empty value
    // (wasSet == true, value still empty) opts back into sharing the
    // GUI's own settings/log directory.
    void explicitlyEmptyProfileArgumentStillShares()
    {
        QString err;
        const QString profile = resolveDaemonProfileArgument(QString(), true, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(profile.isEmpty());
    }

    // The seam this task exists to close: threading the SAME resolved
    // profile string through AppSettings::setProfileOverride() (which the
    // AppSettings::instance() singleton reads on first construction) and
    // CoreInit::initialize() (which resolves the log directory
    // independently via AppSettings::resolveConfigDir(profile),
    // CoreInit.cpp:94) moves the settings file and the log directory
    // together, exactly the way src/server_main.cpp:186-197 does it. This
    // is the only slot in this file that touches the AppSettings::
    // instance() singleton or CoreInit::initialize() (both are
    // process-lifetime singletons/guards), so it does not depend on QTest
    // slot ordering.
    void resolvedProfileMovesTheLogDirectoryWithTheSettingsPath()
    {
        QString err;
        const QString profile = resolveDaemonProfileArgument(QString(), false, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(profile, QString(AppSettings::kDaemonProfileName));

        AppSettings::setProfileOverride(profile);
        QVERIFY(CoreInit::initialize(profile));

        const QString settingsDir =
            QFileInfo(AppSettings::instance().filePath()).absolutePath();
        const QString logDir = AppSettings::resolveConfigDir(profile);
        QCOMPARE(settingsDir, logDir);

        // And it actually moved: neither directory is the GUI's shared
        // default (the pre-Task-1 collision this task exists to close).
        const QString sharedDir = AppSettings::resolveConfigDir(QString());
        QVERIFY(logDir != sharedDir);
        QVERIFY(settingsDir != sharedDir);
    }

    // First-run seed marker (brief step 7): writes "True" under the
    // unprefixed kDaemonProfileSeededKey only when the key is absent, so
    // repeat calls (nereusd calls this on every startup) after the first
    // are a genuine no-op. Direct AppSettings(filePath) construction --
    // not the instance() singleton -- so this is independently testable
    // regardless of the singleton state the previous slot left behind.
    void seedDaemonProfileMarkerWritesOnlyOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings s(dir.filePath(QStringLiteral("NereusSDR.settings")));
        QVERIFY(!s.contains(QLatin1String(AppSettings::kDaemonProfileSeededKey)));

        s.seedDaemonProfileMarker();
        QCOMPARE(s.value(QLatin1String(AppSettings::kDaemonProfileSeededKey)).toString(),
                 QStringLiteral("True"));

        // Idempotent: a second (and third) call must leave the marker
        // exactly as the first call left it.
        s.seedDaemonProfileMarker();
        s.seedDaemonProfileMarker();
        QCOMPARE(s.value(QLatin1String(AppSettings::kDaemonProfileSeededKey)).toString(),
                 QStringLiteral("True"));
    }

    // Review fix round 1: the assertions above only ever observe the final
    // value "True", which an unconditional write would also produce --
    // they cannot tell "wrote once, guard worked" apart from "wrote every
    // call, guard doesn't exist". This slot proves the "only when absent"
    // half of the contract directly: overwrite the key to a sentinel value
    // seedDaemonProfileMarker() would never write, call it again, and
    // assert the sentinel survived. That fails under an unconditional-write
    // implementation and passes only when the second call is a genuine
    // no-op.
    void seedDaemonProfileMarkerDoesNotOverwriteAnExistingValue()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings s(dir.filePath(QStringLiteral("NereusSDR.settings")));

        s.seedDaemonProfileMarker();
        QCOMPARE(s.value(QLatin1String(AppSettings::kDaemonProfileSeededKey)).toString(),
                 QStringLiteral("True"));

        // A value seedDaemonProfileMarker() itself would never write.
        s.setValue(QLatin1String(AppSettings::kDaemonProfileSeededKey),
                   QStringLiteral("SENTINEL"));

        s.seedDaemonProfileMarker();
        QCOMPARE(s.value(QLatin1String(AppSettings::kDaemonProfileSeededKey)).toString(),
                 QStringLiteral("SENTINEL"));
    }

    // The marker must survive a process restart -- that is the entire
    // point of a first-run flag -- so seeding must reach disk, not just
    // the in-memory map.
    void seedDaemonProfileMarkerPersistsAcrossReload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings s(path);
            s.seedDaemonProfileMarker();
        }

        AppSettings reloaded(path);
        reloaded.load();
        QCOMPARE(reloaded.value(QLatin1String(AppSettings::kDaemonProfileSeededKey)).toString(),
                 QStringLiteral("True"));
    }
};

QTEST_MAIN(TstDaemonSettingsProfile)
#include "tst_daemon_settings_profile.moc"
