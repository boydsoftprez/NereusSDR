// =================================================================
// tests/tst_peripherals_settings.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No upstream source file ported.
//
// Verifies the per-radio peripherals settings API introduced by the
// 2026-05-26 refactor:
//
//   1. peripheralValue() returns the caller's default when no radio is
//      connected (no MAC to scope under).
//   2. peripheralValue() returns the per-MAC value when connected and
//      the key has been written.
//   3. setPeripheralValue() round-trips via AppSettings::hardwareValue
//      under the hardware/<mac>/peripherals/<key> path.
//   4. setPeripheralValue() is a no-op when not connected (no MAC).
//   5. Global-key migration: legacy RfKit_* / FourO3A_Enabled / PGXL_*
//      / TGXL_* keys present at the first Connected event get folded
//      into the connected MAC's per-MAC scope and the global keys are
//      removed.  The PeripheralsMigrationDone sentinel is set so a
//      second Connected event is a no-op.
//   6. MAC-switch isolation: writing to radio A's peripheral scope
//      doesn't bleed into radio B's.
//
// Modification history (NereusSDR):
//   2026-05-26 -- Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"   // RadioInfo
#include "core/RadioConnection.h"  // ConnectionState

using namespace NereusSDR;

class PeripheralsSettingsTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();

    void peripheralValueReturnsDefaultWhenNotConnected();
    void peripheralValueReturnsStoredValueWhenConnected();
    void setPeripheralValueRoundTrips();
    void setPeripheralValueIsNoOpWhenNotConnected();
    void migrationFoldsGlobalsToPerMacAndSetsSentinel();
    void migrationRunsOnlyOnce();
    void macSwitchIsolatesScope();

private:
    static void connectMac(RadioModel& m, const QString& mac);
    static void disconnectModel(RadioModel& m);
};

void PeripheralsSettingsTest::connectMac(RadioModel& m, const QString& mac)
{
    RadioInfo info;
    info.macAddress = mac;
    m.setLastRadioInfoForTest(info);
    m.setConnectionStateForTest(ConnectionState::Connected);
    // Drive the peripherals lifecycle directly so tests don't need the
    // full onConnectionStateChanged side-effects (settings-hygiene
    // validate, currentRadioChanged emit, etc.).  Production routes via
    // the signal-driven onConnectionStateChanged handler in RadioModel.cpp.
    m.applyPeripheralsForTest();
}

void PeripheralsSettingsTest::disconnectModel(RadioModel& m)
{
    m.teardownPeripheralsForTest();
    m.setConnectionStateForTest(ConnectionState::Disconnected);
    m.setLastRadioInfoForTest(RadioInfo{});
}

void PeripheralsSettingsTest::initTestCase()
{
    AppSettings::instance().clear();
}

void PeripheralsSettingsTest::cleanup()
{
    AppSettings::instance().clear();
}

// 1. peripheralValue() returns default when not connected.
void PeripheralsSettingsTest::peripheralValueReturnsDefaultWhenNotConnected()
{
    RadioModel m;
    QCOMPARE(m.currentRadioMac(), QString{});
    QCOMPARE(m.peripheralValue(QStringLiteral("RfKit_ManualIp"),
                               QStringLiteral("fallback.example")),
             QStringLiteral("fallback.example"));
    QCOMPARE(m.peripheralValue(QStringLiteral("PGXL_ManualPort"),
                               QStringLiteral("9008")),
             QStringLiteral("9008"));
}

// 2. peripheralValue() returns stored value when connected and key set.
void PeripheralsSettingsTest::peripheralValueReturnsStoredValueWhenConnected()
{
    RadioModel m;
    const QString mac = QStringLiteral("aa:bb:cc:dd:ee:f0");
    connectMac(m, mac);

    AppSettings::instance().setHardwareValue(
        mac,
        QStringLiteral("peripherals/RfKit_ManualIp"),
        QStringLiteral("10.0.0.5"));

    QCOMPARE(m.peripheralValue(QStringLiteral("RfKit_ManualIp")),
             QStringLiteral("10.0.0.5"));
    // A key NOT written still falls back to the caller's default.
    QCOMPARE(m.peripheralValue(QStringLiteral("PGXL_ManualPort"),
                               QStringLiteral("9008")),
             QStringLiteral("9008"));
}

// 3. setPeripheralValue() round-trips via AppSettings::hardwareValue.
void PeripheralsSettingsTest::setPeripheralValueRoundTrips()
{
    RadioModel m;
    const QString mac = QStringLiteral("aa:bb:cc:dd:ee:f1");
    connectMac(m, mac);

    m.setPeripheralValue(QStringLiteral("PGXL_ManualIp"),
                         QStringLiteral("192.168.1.42"));
    m.setPeripheralValue(QStringLiteral("PGXL_ManualPort"),
                         QStringLiteral("9008"));

    // Read back via the AppSettings raw API to prove the storage path.
    QCOMPARE(AppSettings::instance()
                 .hardwareValue(mac, QStringLiteral("peripherals/PGXL_ManualIp"))
                 .toString(),
             QStringLiteral("192.168.1.42"));
    QCOMPARE(AppSettings::instance()
                 .hardwareValue(mac, QStringLiteral("peripherals/PGXL_ManualPort"))
                 .toString(),
             QStringLiteral("9008"));

    // And via the helper for symmetry.
    QCOMPARE(m.peripheralValue(QStringLiteral("PGXL_ManualIp")),
             QStringLiteral("192.168.1.42"));
}

// 4. setPeripheralValue() is a no-op when not connected.
void PeripheralsSettingsTest::setPeripheralValueIsNoOpWhenNotConnected()
{
    RadioModel m;
    QVERIFY(m.currentRadioMac().isEmpty());

    m.setPeripheralValue(QStringLiteral("RfKit_ManualIp"),
                         QStringLiteral("should-not-persist"));

    // Nothing should have been written to any MAC scope.  We can't
    // grep all hardwareValue entries cheaply, but we CAN prove
    // peripheralValue() returns the default (no persisted value to
    // read back, even after we connect a MAC).
    connectMac(m, QStringLiteral("aa:bb:cc:dd:ee:f2"));
    QCOMPARE(m.peripheralValue(QStringLiteral("RfKit_ManualIp"),
                               QStringLiteral("default")),
             QStringLiteral("default"));
}

// 5. Migration folds globals into per-MAC and sets the sentinel.
void PeripheralsSettingsTest::migrationFoldsGlobalsToPerMacAndSetsSentinel()
{
    // Seed legacy globals as if from a v0.5.1 install with single radio.
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("RfKit_Enabled"),  QStringLiteral("True"));
    s.setValue(QStringLiteral("RfKit_ManualIp"), QStringLiteral("10.0.0.5"));
    s.setValue(QStringLiteral("RfKit_ManualPort"), QStringLiteral("8080"));
    s.setValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
    s.setValue(QStringLiteral("PGXL_ManualIp"),  QStringLiteral("192.168.1.42"));
    s.setValue(QStringLiteral("PGXL_ManualPort"), QStringLiteral("9008"));
    s.setValue(QStringLiteral("TGXL_ManualIp"),  QStringLiteral("192.168.1.43"));
    s.setValue(QStringLiteral("TGXL_ManualPort"), QStringLiteral("9010"));
    // Sentinel absent so migration will run.
    QVERIFY(s.value(QStringLiteral("PeripheralsMigrationDone"))
                .toString().isEmpty());

    RadioModel m;
    const QString mac = QStringLiteral("aa:bb:cc:dd:ee:f3");
    connectMac(m, mac);

    // Each legacy global should now live under the per-MAC scope.
    QCOMPARE(s.hardwareValue(mac, QStringLiteral("peripherals/RfKit_Enabled"))
                 .toString(),
             QStringLiteral("True"));
    QCOMPARE(s.hardwareValue(mac, QStringLiteral("peripherals/RfKit_ManualIp"))
                 .toString(),
             QStringLiteral("10.0.0.5"));
    QCOMPARE(s.hardwareValue(mac, QStringLiteral("peripherals/PGXL_ManualPort"))
                 .toString(),
             QStringLiteral("9008"));
    QCOMPARE(s.hardwareValue(mac, QStringLiteral("peripherals/TGXL_ManualIp"))
                 .toString(),
             QStringLiteral("192.168.1.43"));
    QCOMPARE(s.hardwareValue(mac, QStringLiteral("peripherals/FourO3A_Enabled"))
                 .toString(),
             QStringLiteral("True"));

    // Globals must be erased after the fold so they don't re-bleed on
    // the next read in case a stale reader exists somewhere.
    QVERIFY(!s.contains(QStringLiteral("RfKit_Enabled")));
    QVERIFY(!s.contains(QStringLiteral("PGXL_ManualIp")));
    QVERIFY(!s.contains(QStringLiteral("TGXL_ManualPort")));

    // Sentinel flipped to True so subsequent launches skip.
    QCOMPARE(s.value(QStringLiteral("PeripheralsMigrationDone"))
                 .toString(),
             QStringLiteral("True"));
}

// 5b. Migration runs only once: second Connected is a no-op.
void PeripheralsSettingsTest::migrationRunsOnlyOnce()
{
    auto& s = AppSettings::instance();
    // Pre-set the sentinel as if a prior launch already migrated.
    s.setValue(QStringLiteral("PeripheralsMigrationDone"),
               QStringLiteral("True"));
    // Seed legacy globals that should NOT migrate this time.
    s.setValue(QStringLiteral("RfKit_ManualIp"),
               QStringLiteral("should-not-migrate"));

    RadioModel m;
    const QString mac = QStringLiteral("aa:bb:cc:dd:ee:f4");
    connectMac(m, mac);

    // The global should still be present (untouched) and the per-MAC
    // scope should remain empty for that key.
    QCOMPARE(s.value(QStringLiteral("RfKit_ManualIp")).toString(),
             QStringLiteral("should-not-migrate"));
    QCOMPARE(s.hardwareValue(mac, QStringLiteral("peripherals/RfKit_ManualIp"))
                 .toString(),
             QString{});
}

// 6. MAC switch: writing to A's scope doesn't bleed into B's.
void PeripheralsSettingsTest::macSwitchIsolatesScope()
{
    RadioModel m;
    const QString macA = QStringLiteral("aa:bb:cc:dd:ee:01");
    const QString macB = QStringLiteral("aa:bb:cc:dd:ee:02");

    // Connect A and write a value.
    connectMac(m, macA);
    m.setPeripheralValue(QStringLiteral("RfKit_ManualIp"),
                         QStringLiteral("10.0.0.1"));

    // Switch to B (no value yet) and confirm the helper returns the
    // default rather than A's value.
    disconnectModel(m);
    connectMac(m, macB);
    QCOMPARE(m.peripheralValue(QStringLiteral("RfKit_ManualIp"),
                               QStringLiteral("default")),
             QStringLiteral("default"));

    // Write a value on B.
    m.setPeripheralValue(QStringLiteral("RfKit_ManualIp"),
                         QStringLiteral("10.0.0.2"));

    // A's stored value is untouched.
    QCOMPARE(AppSettings::instance()
                 .hardwareValue(macA,
                                QStringLiteral("peripherals/RfKit_ManualIp"))
                 .toString(),
             QStringLiteral("10.0.0.1"));
    // B's helper returns B's stored value.
    QCOMPARE(m.peripheralValue(QStringLiteral("RfKit_ManualIp")),
             QStringLiteral("10.0.0.2"));
}

QTEST_MAIN(PeripheralsSettingsTest)
#include "tst_peripherals_settings.moc"
