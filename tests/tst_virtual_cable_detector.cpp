// tests/tst_virtual_cable_detector.cpp (NereusSDR)
// Unit tests for VirtualCableDetector::matchProduct() pure-function hook.
// NereusSDR-original; no Thetis/AetherSDR port.
#include <QtTest/QtTest>
#include "core/audio/VirtualCableDetector.h"

using namespace NereusSDR;

class TstVirtualCableDetector : public QObject {
    Q_OBJECT
private slots:
    void detectsVbCableBase() {
        QVERIFY(VirtualCableDetector::matchProduct(
            "CABLE Input (VB-Audio Virtual Cable)") == VirtualCableProduct::VbCable);
    }
    void detectsVbCableAB() {
        QVERIFY(VirtualCableDetector::matchProduct("CABLE-A Input") == VirtualCableProduct::VbCableA);
        QVERIFY(VirtualCableDetector::matchProduct("CABLE-B Input") == VirtualCableProduct::VbCableB);
    }
    void detectsVac() {
        QVERIFY(VirtualCableDetector::matchProduct("Line 1 (Virtual Audio Cable)")
                == VirtualCableProduct::MuzychenkoVac);
    }
    void detectsVoicemeeter() {
        QVERIFY(VirtualCableDetector::matchProduct("VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)")
                == VirtualCableProduct::Voicemeeter);
    }
    void detectsDante() {
        QVERIFY(VirtualCableDetector::matchProduct("Dante Tx 01-02")
                == VirtualCableProduct::Dante);
    }
    void detectsFlexDax() {
        QVERIFY(VirtualCableDetector::matchProduct("DAX Audio RX 1")
                == VirtualCableProduct::FlexRadioDax);
    }
    void detectsReservedNereusVax() {
        QVERIFY(VirtualCableDetector::matchProduct("NereusSDR VAX 1")
                == VirtualCableProduct::NereusSdrVax);
    }
    void unknownDeviceIsNone() {
        QVERIFY(VirtualCableDetector::matchProduct("Realtek HD Audio Output")
                == VirtualCableProduct::None);
    }
    // Extra: verify CABLE-A prefix doesn't shadow CABLE-AB (shouldn't match VbCableA)
    void cableAbDoesNotMatchCableA() {
        // "CABLE-AB Input" doesn't start with "^CABLE-A " (note trailing space in rule),
        // so it falls through to "^CABLE " and matches VbCable base, NOT VbCableA.
        const auto p = VirtualCableDetector::matchProduct("CABLE-AB Input");
        QVERIFY(p != VirtualCableProduct::VbCableA);
    }
};

QTEST_APPLESS_MAIN(TstVirtualCableDetector)
#include "tst_virtual_cable_detector.moc"
