// Issue #100 — multi-instance support via --profile CLI flag.
//
// Two instances of NereusSDR connected to different radios need isolated
// settings + log directories. AppSettings exposes static path resolvers
// so main.cpp can thread a profile name in from QCommandLineParser
// without coupling the singleton's constructor to CLI parsing.

#include <QtTest/QtTest>
#include "core/AppSettings.h"

#include <QDir>
#include <QStandardPaths>

using namespace NereusSDR;

class TstAppSettingsProfile : public QObject {
    Q_OBJECT
private slots:
    void emptyProfileResolvesToLegacyPath() {
        // PR #238 unified the path resolver: both macOS and the other
        // platforms route through QStandardPaths::writableLocation(
        // GenericConfigLocation) + "/NereusSDR" so TestSandboxInit's
        // setTestModeEnabled(true) redirect actually takes effect on
        // every platform. The pre-#238 macOS branch hardcoded
        // ~/Library/Preferences/NereusSDR which bypassed the test
        // sandbox; this assertion now mirrors production for all
        // platforms (and therefore picks up the .qttest/ prefix during
        // test runs without needing a #ifdef shim).
        const QString path = AppSettings::resolveSettingsPath(QString());
        const QString expected = QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation) +
            "/NereusSDR/NereusSDR.settings";
        QCOMPARE(path, expected);
    }

    void profileScopesUnderProfilesSubdir() {
        const QString path = AppSettings::resolveSettingsPath("hf");
        QVERIFY2(path.contains("/profiles/hf/"),
                 qPrintable("got: " + path));
        QVERIFY(path.endsWith("/NereusSDR.settings"));
    }

    void distinctProfilesDoNotCollide() {
        QCOMPARE(AppSettings::resolveSettingsPath("hf")
                 == AppSettings::resolveSettingsPath("vhf"), false);
    }

    void configDirIsSettingsFileParent() {
        const QString dir  = AppSettings::resolveConfigDir("hf");
        const QString path = AppSettings::resolveSettingsPath("hf");
        QVERIFY(path.startsWith(dir + "/"));
        QCOMPARE(path, dir + "/NereusSDR.settings");
    }

    void unsafeProfileNameFallsBackToDefault() {
        // Path-traversal or whitespace names must not escape the
        // profiles/ sandbox.
        const QString legacy = AppSettings::resolveSettingsPath(QString());
        QCOMPARE(AppSettings::resolveSettingsPath("../escape"), legacy);
        QCOMPARE(AppSettings::resolveSettingsPath("with space"), legacy);
        QCOMPARE(AppSettings::resolveSettingsPath("a/b"),       legacy);
    }

    void profileNameIsValidatedToAlnumDashUnderscore() {
        QVERIFY(AppSettings::isValidProfileName("hf"));
        QVERIFY(AppSettings::isValidProfileName("anan-8000dle"));
        QVERIFY(AppSettings::isValidProfileName("vhf_uhf"));
        QVERIFY(!AppSettings::isValidProfileName(""));
        QVERIFY(!AppSettings::isValidProfileName("../escape"));
        QVERIFY(!AppSettings::isValidProfileName("with space"));
        QVERIFY(!AppSettings::isValidProfileName("a/b"));
    }
};

QTEST_APPLESS_MAIN(TstAppSettingsProfile)
#include "tst_app_settings_profile.moc"
