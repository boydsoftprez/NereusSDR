// no-port-check: NereusSDR-original unit-test file.  The mi0bot-Thetis
// references below are cite comments documenting which upstream lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_tx_post_gen_tone_mag.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::m_txPostGenToneMag property.
//
// Spec §5.4.3; Plan Task 3; Issue #175.
//
// Source references (cited per assertion; logic ported in TransmitModel.cpp):
//   mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]
//       -- SetTXAPostGenToneMag call site driving HL2 sub-step DSP
//          audio-gain modulation.  Range 0.4..0.9999 on HL2 sub-step
//          path; 1.0 means "no modulation" (default, non-HL2 path).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTransmitModelTxPostGenToneMag : public QObject {
    Q_OBJECT
private slots:

    void defaultIs1_0() {
        // mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]:
        // 1.0 is the non-HL2 / no-modulation sentinel value.
        TransmitModel m;
        QCOMPARE(m.txPostGenToneMag(), 1.0);
    }

    void setterRoundTrip() {
        TransmitModel m;
        m.setTxPostGenToneMag(0.5);
        QCOMPARE(m.txPostGenToneMag(), 0.5);
    }

    void setterEmitsSignal() {
        TransmitModel m;
        QSignalSpy spy(&m, &TransmitModel::txPostGenToneMagChanged);
        m.setTxPostGenToneMag(0.42);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toDouble(), 0.42);
    }

    void setterDeduplicates() {
        // Matches NereusSDR setter convention: dedupe equal values.
        TransmitModel m;
        m.setTxPostGenToneMag(0.5);
        QSignalSpy spy(&m, &TransmitModel::txPostGenToneMagChanged);
        m.setTxPostGenToneMag(0.5);  // same value
        QCOMPARE(spy.count(), 0);    // no re-emit
    }
};

QTEST_MAIN(TestTransmitModelTxPostGenToneMag)
#include "tst_transmit_model_tx_post_gen_tone_mag.moc"
