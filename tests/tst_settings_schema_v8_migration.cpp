// =================================================================
// tests/tst_settings_schema_v8_migration.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Receiver and transmit gaps plan, Task 10 (R-R3-49):
// SettingsSchemaVersion v7 -> v8 drops TciRateLimitMsgsPerSec once. The
// TCI rate limit was a messages-per-second box nothing read; it is now
// Thetis's gap in ms between frequency updates to each app, saved under
// TciRateLimitMs. A number saved in the old unit means nothing in the
// new one, so every operator starts from the default (100 ms).
// =================================================================

#include <QtTest/QtTest>
#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {
const QString kVersionKey = QStringLiteral("SettingsSchemaVersion");
const QString kOldKey = QStringLiteral("TciRateLimitMsgsPerSec");
const QString kNewKey = QStringLiteral("TciRateLimitMs");
const QString kOtherKey = QStringLiteral("TciServerPort");
} // namespace

class TestSettingsSchemaV8Migration : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        auto& s = AppSettings::instance();
        s.remove(kVersionKey);
        s.remove(kOldKey);
        s.remove(kNewKey);
        s.remove(kOtherKey);
    }

    void v7_to_v8_drops_the_old_unit()
    {
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("7"));
        s.setValue(kOldKey, QStringLiteral("60"));
        s.setValue(kOtherKey, QStringLiteral("50002"));

        s.ensureSettingsAtVersion(8);

        QVERIFY(!s.contains(kOldKey));
        QVERIFY(!s.contains(kNewKey));  // the default applies, nothing seeded
        QCOMPARE(s.value(kOtherKey, QString()).toString(), QStringLiteral("50002"));
        QCOMPARE(s.value(kVersionKey, QString()).toString(), QStringLiteral("8"));
    }

    void v8_keeps_a_value_saved_in_ms()
    {
        // The drop runs once: a gap chosen on a v8 settings file stays.
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("8"));
        s.setValue(kNewKey, QStringLiteral("250"));

        s.ensureSettingsAtVersion(8);

        QCOMPARE(s.value(kNewKey, QString()).toString(), QStringLiteral("250"));
    }
};

QTEST_MAIN(TestSettingsSchemaV8Migration)
#include "tst_settings_schema_v8_migration.moc"
