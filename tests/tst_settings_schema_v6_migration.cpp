// =================================================================
// tests/tst_settings_schema_v6_migration.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Task 12: verify SettingsSchemaVersion v5 -> v6
// migration. v6 is additive only (no key renames), so the migration is
// effectively a version bump that lets the app know per-slice per-band
// sample-rate keys are part of the schema going forward.
// =================================================================

#include <QtTest/QtTest>
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestSettingsSchemaV6Migration : public QObject {
    Q_OBJECT
private slots:
    void v5_to_v6_bumps_version_key()
    {
        // Pre-populate AppSettings as if it was v5 (current latest before 3F).
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("5"));

        // Run migration with currentVersion = 6.
        s.ensureSettingsAtVersion(6);

        QCOMPARE(s.value(QStringLiteral("SettingsSchemaVersion"), QString()).toString(),
                 QStringLiteral("6"));
    }

    void v6_to_v6_is_noop()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("6"));
        s.ensureSettingsAtVersion(6);
        QCOMPARE(s.value(QStringLiteral("SettingsSchemaVersion"), QString()).toString(),
                 QStringLiteral("6"));
    }
};

QTEST_MAIN(TestSettingsSchemaV6Migration)
#include "tst_settings_schema_v6_migration.moc"
