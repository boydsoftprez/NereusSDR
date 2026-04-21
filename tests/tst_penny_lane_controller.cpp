// no-port-check: test fixture asserts PennyLaneController ext-ctrl master toggle + persistence
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/accessories/PennyLaneController.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestPennyLaneController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        AppSettings::instance().clear();
    }

    // Default: extCtrlEnabled = true (matches Thetis penny_ext_ctrl_enabled = true)
    void default_extctrl_enabled() {
        PennyLaneController p;
        QVERIFY(p.extCtrlEnabled());
    }

    // setExtCtrlEnabled: toggle off, signal fires
    void setExtCtrlEnabled_off_signal_fires() {
        PennyLaneController p;
        QSignalSpy spy(&p, &PennyLaneController::extCtrlEnabledChanged);
        p.setExtCtrlEnabled(false);
        QVERIFY(!p.extCtrlEnabled());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), false);
    }

    // Idempotent: setting the same value doesn't fire the signal
    void setExtCtrlEnabled_idempotent_no_signal() {
        PennyLaneController p;
        QSignalSpy spy(&p, &PennyLaneController::extCtrlEnabledChanged);
        p.setExtCtrlEnabled(true);  // already true
        QCOMPARE(spy.count(), 0);
    }

    // setExtCtrlEnabled: toggle on then off
    void setExtCtrlEnabled_toggle_on_off() {
        PennyLaneController p;
        p.setExtCtrlEnabled(false);
        QVERIFY(!p.extCtrlEnabled());
        p.setExtCtrlEnabled(true);
        QVERIFY(p.extCtrlEnabled());
    }

    // Per-MAC persistence round-trip (extCtrlEnabled = false)
    void persistence_roundtrip_false() {
        const QString mac = QStringLiteral("de:ad:be:ef:00:01");
        PennyLaneController p1;
        p1.setMacAddress(mac);
        p1.setExtCtrlEnabled(false);
        p1.save();

        PennyLaneController p2;
        p2.setMacAddress(mac);
        p2.load();
        QVERIFY(!p2.extCtrlEnabled());
    }

    // Per-MAC persistence round-trip (extCtrlEnabled = true)
    void persistence_roundtrip_true() {
        const QString mac = QStringLiteral("de:ad:be:ef:00:02");
        PennyLaneController p1;
        p1.setMacAddress(mac);
        p1.setExtCtrlEnabled(true);
        p1.save();

        PennyLaneController p2;
        p2.setMacAddress(mac);
        p2.load();
        QVERIFY(p2.extCtrlEnabled());
    }

    // load() without MAC is a no-op; default remains true
    void load_without_mac_is_noop() {
        PennyLaneController p;
        p.setExtCtrlEnabled(false);
        p.load();  // no MAC — should not change state or crash
        QVERIFY(!p.extCtrlEnabled());  // state from before, not reset by failed load
    }
};

QTEST_APPLESS_MAIN(TestPennyLaneController)
#include "tst_penny_lane_controller.moc"
