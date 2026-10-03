// =================================================================
// tests/tst_settings_schema_v7_migration.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R-R3-49 fix wave: SettingsSchemaVersion v6 -> v7 resets
// NetworkWatchdogEnabled once. Before R-R3-49 the Setup > General >
// Options checkbox saved the key but nothing read it; the key now drives
// the radio's watchdog, so a value saved while it did nothing must not
// come alive on upgrade. Every operator starts from the default (on).
// =================================================================

#include <QtTest/QtTest>
#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {
const QString kVersionKey = QStringLiteral("SettingsSchemaVersion");
const QString kWatchdogKey = QStringLiteral("NetworkWatchdogEnabled");
const QString kOtherKey = QStringLiteral("RxOnly");
const QString kOtherNestedKey = QStringLiteral("hardware/aa:bb:cc:dd:ee:ff/radioInfo/sampleRate");
} // namespace

class TestSettingsSchemaV7Migration : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().remove(kVersionKey);
        AppSettings::instance().remove(kWatchdogKey);
        AppSettings::instance().remove(kOtherKey);
        AppSettings::instance().remove(kOtherNestedKey);
    }

    void v6_to_v7_resets_a_stale_watchdog_value()
    {
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("6"));
        s.setValue(kWatchdogKey, QStringLiteral("False"));

        s.ensureSettingsAtVersion(7);

        QVERIFY(!s.contains(kWatchdogKey));
        QCOMPARE(s.value(kVersionKey, QString()).toString(), QStringLiteral("7"));
    }

    void v6_to_v7_bumps_version_with_no_watchdog_key()
    {
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("6"));

        s.ensureSettingsAtVersion(7);

        QVERIFY(!s.contains(kWatchdogKey));
        QCOMPARE(s.value(kVersionKey, QString()).toString(), QStringLiteral("7"));
    }

    void v6_to_v7_leaves_other_keys_alone()
    {
        // The step removes the watchdog key only: every other setting, flat
        // or per-radio, comes through the upgrade with its value.
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("6"));
        s.setValue(kWatchdogKey, QStringLiteral("False"));
        s.setValue(kOtherKey, QStringLiteral("True"));
        s.setValue(kOtherNestedKey, QStringLiteral("192000"));

        s.ensureSettingsAtVersion(7);

        QVERIFY(!s.contains(kWatchdogKey));
        QCOMPARE(s.value(kOtherKey, QString()).toString(), QStringLiteral("True"));
        QCOMPARE(s.value(kOtherNestedKey, QString()).toString(), QStringLiteral("192000"));
    }

    void v7_keeps_a_value_saved_after_the_reset()
    {
        // The reset runs once: an operator who turns the watchdog off on a
        // v7 settings file keeps that choice across later launches.
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("7"));
        s.setValue(kWatchdogKey, QStringLiteral("False"));

        s.ensureSettingsAtVersion(7);

        QCOMPARE(s.value(kWatchdogKey, QString()).toString(), QStringLiteral("False"));
    }
};

QTEST_MAIN(TestSettingsSchemaV7Migration)
#include "tst_settings_schema_v7_migration.moc"
