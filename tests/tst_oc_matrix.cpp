// no-port-check: test fixture asserts OcMatrix model behavior + persistence round-trip
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/OcMatrix.h"
#include "models/Band.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestOcMatrix : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Use a clean isolated AppSettings for each test run.
        AppSettings::instance().clear();
    }

    // Default state — every band × pin × {RX,TX} starts clear (0)
    void default_state_all_clear() {
        OcMatrix m;
        for (int b = int(Band::Band160m); b < int(Band::Count); ++b) {
            for (int pin = 0; pin < 7; ++pin) {
                QVERIFY(!m.pinEnabled(Band(b), pin, false));
                QVERIFY(!m.pinEnabled(Band(b), pin, true));
            }
        }
        QCOMPARE(int(m.maskFor(Band::Band20m, false)), 0);
        QCOMPARE(int(m.maskFor(Band::Band20m, true)),  0);
    }

    // Setting individual pins flips the right bits
    void setPin_flips_bit() {
        OcMatrix m;
        m.setPin(Band::Band20m, /*pin=*/2, /*tx=*/false, true);
        QCOMPARE(int(m.maskFor(Band::Band20m, false)) & 0x04, 0x04);  // pin 2 → bit 2
        QCOMPARE(int(m.maskFor(Band::Band20m, true)),  0);
        QCOMPARE(int(m.maskFor(Band::Band80m, false)), 0);
    }

    // Mask format: bit N = pin N
    void mask_bit_layout() {
        OcMatrix m;
        m.setPin(Band::Band40m, 0, true, true);
        m.setPin(Band::Band40m, 6, true, true);
        const quint8 expected = 0x01 | 0x40;  // pins 0 and 6
        QCOMPARE(int(m.maskFor(Band::Band40m, true)), int(expected));
    }

    // TX pin action mapping — default clear
    void txPinAction_default_clear() {
        OcMatrix m;
        for (int pin = 0; pin < 7; ++pin) {
            for (int act = 0; act < int(OcMatrix::TXPinAction::Count); ++act) {
                QCOMPARE(m.pinAction(pin), OcMatrix::TXPinAction::MoxTuneTwoTone);
            }
        }
    }

    void txPinAction_set_get() {
        OcMatrix m;
        m.setPinAction(/*pin=*/2, OcMatrix::TXPinAction::Mox);
        m.setPinAction(/*pin=*/3, OcMatrix::TXPinAction::Tune);
        QCOMPARE(m.pinAction(2), OcMatrix::TXPinAction::Mox);
        QCOMPARE(m.pinAction(3), OcMatrix::TXPinAction::Tune);
        // Untouched pins still have default
        QCOMPARE(m.pinAction(0), OcMatrix::TXPinAction::MoxTuneTwoTone);
    }

    // Per-MAC persistence round-trip
    void persistence_roundtrip() {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:ff");

        OcMatrix m1;
        m1.setMacAddress(mac);
        m1.setPin(Band::Band20m, 2, false, true);
        m1.setPin(Band::Band40m, 5, true, true);
        m1.setPinAction(3, OcMatrix::TXPinAction::Tune);
        m1.save();

        // Fresh instance — should hydrate from AppSettings
        OcMatrix m2;
        m2.setMacAddress(mac);
        m2.load();
        QVERIFY(m2.pinEnabled(Band::Band20m, 2, false));
        QVERIFY(!m2.pinEnabled(Band::Band20m, 2, true));
        QVERIFY(m2.pinEnabled(Band::Band40m, 5, true));
        QCOMPARE(m2.pinAction(3), OcMatrix::TXPinAction::Tune);
    }

    // resetDefaults() clears every cell back to Thetis init state
    void resetDefaults_restores_init_state() {
        OcMatrix m;
        m.setPin(Band::Band20m, 2, false, true);
        m.setPinAction(3, OcMatrix::TXPinAction::Mox);
        m.resetDefaults();
        QCOMPARE(int(m.maskFor(Band::Band20m, false)), 0);
        // After reset, pin actions restore to Thetis init: MOX_TUNE_TWOTONE
        QCOMPARE(m.pinAction(3), OcMatrix::TXPinAction::MoxTuneTwoTone);
    }

    // changed() signal fires on setPin / setPinAction
    void changed_signal_fires_on_setPin() {
        OcMatrix m;
        QSignalSpy spy(&m, &OcMatrix::changed);
        m.setPin(Band::Band20m, 2, false, true);
        QCOMPARE(spy.count(), 1);
    }

    void changed_signal_fires_on_setPinAction() {
        OcMatrix m;
        QSignalSpy spy(&m, &OcMatrix::changed);
        m.setPinAction(0, OcMatrix::TXPinAction::Mox);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_APPLESS_MAIN(TestOcMatrix)
#include "tst_oc_matrix.moc"
