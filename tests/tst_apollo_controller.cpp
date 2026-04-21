// no-port-check: test fixture asserts ApolloController Apollo PA/ATU/LPF present + filter + tuner booleans + persistence
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/accessories/ApolloController.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestApolloController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        AppSettings::instance().clear();
    }

    // Defaults: all off (matches Thetis private field initializers)
    void defaults_all_off() {
        ApolloController a;
        QVERIFY(!a.present());
        QVERIFY(!a.filterEnabled());
        QVERIFY(!a.tunerEnabled());
    }

    // setPresent: round-trip + signal fires
    void setPresent_signal_and_roundtrip() {
        ApolloController a;
        QSignalSpy spy(&a, &ApolloController::presentChanged);
        a.setPresent(true);
        QVERIFY(a.present());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
    }

    // setPresent: idempotent — no double-fire
    void setPresent_idempotent() {
        ApolloController a;
        QSignalSpy spy(&a, &ApolloController::presentChanged);
        a.setPresent(false);
        QCOMPARE(spy.count(), 0);  // already false, no change
    }

    // setFilterEnabled: round-trip + signal fires
    void setFilterEnabled_signal_and_roundtrip() {
        ApolloController a;
        QSignalSpy spy(&a, &ApolloController::filterEnabledChanged);
        a.setFilterEnabled(true);
        QVERIFY(a.filterEnabled());
        QCOMPARE(spy.count(), 1);
    }

    // setTunerEnabled: round-trip + signal fires
    void setTunerEnabled_signal_and_roundtrip() {
        ApolloController a;
        QSignalSpy spy(&a, &ApolloController::tunerEnabledChanged);
        a.setTunerEnabled(true);
        QVERIFY(a.tunerEnabled());
        QCOMPARE(spy.count(), 1);
    }

    // Per-MAC persistence round-trip
    void persistence_roundtrip() {
        const QString mac = QStringLiteral("11:22:33:44:55:66");
        ApolloController a1;
        a1.setMacAddress(mac);
        a1.setPresent(true);
        a1.setFilterEnabled(true);
        a1.setTunerEnabled(false);
        a1.save();

        ApolloController a2;
        a2.setMacAddress(mac);
        a2.load();
        QVERIFY(a2.present());
        QVERIFY(a2.filterEnabled());
        QVERIFY(!a2.tunerEnabled());
    }

    // Persistence: all fields survive a full cycle
    void persistence_all_fields() {
        const QString mac = QStringLiteral("aa:11:bb:22:cc:33");
        ApolloController a1;
        a1.setMacAddress(mac);
        a1.setPresent(true);
        a1.setFilterEnabled(false);
        a1.setTunerEnabled(true);
        a1.save();

        ApolloController a2;
        a2.setMacAddress(mac);
        a2.load();
        QVERIFY(a2.present());
        QVERIFY(!a2.filterEnabled());
        QVERIFY(a2.tunerEnabled());
    }

    // load() without a MAC address set is a no-op
    void load_without_mac_is_noop() {
        ApolloController a;
        a.load();  // should not crash or change defaults
        QVERIFY(!a.present());
    }
};

QTEST_APPLESS_MAIN(TestApolloController)
#include "tst_apollo_controller.moc"
