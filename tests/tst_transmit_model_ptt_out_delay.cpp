// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// and "setup.designer.cs" references below are cite comments documenting
// which Thetis lines each assertion verifies; no Thetis logic is ported
// in this test file.
// =================================================================
// tests/tst_transmit_model_ptt_out_delay.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::pttOutDelayMs property.
//
// MoxController slot that consumes this property is added in a later task
// (radio-mic-input Task 7 lands the model + persistence only).  Per-MAC
// persisted under key PTT_Out_Delay.
//
// Source references (cited per assertion; no logic ported in this file):
//   console.cs:19694 [v2.10.3.13+501e3f51] — ptt_out_delay = 20 (default).
//   setup.designer.cs:9176-9204 [v2.10.3.13+501e3f51] — udGenPTTOutDelay
//     spinbox: Minimum=0, Maximum=500.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("11:22:33:44:55:66");
}

class TstTransmitModelPttOutDelay : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() {
        AppSettings::instance().clear();
    }
    void init() {
        AppSettings::instance().clear();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // DEFAULT
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void defaultIsTwenty() {
        // From Thetis console.cs:19694 [v2.10.3.13+501e3f51]:
        //   private int ptt_out_delay = 20;
        TransmitModel tm;
        QCOMPARE(tm.pttOutDelayMs(), 20);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // CLAMPING (range 0..500 from setup.designer.cs:9176-9204)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterClampsBelow() {
        TransmitModel tm;
        tm.setPttOutDelayMs(-5);
        QCOMPARE(tm.pttOutDelayMs(), 0);
    }

    void setterClampsAbove() {
        TransmitModel tm;
        tm.setPttOutDelayMs(800);
        QCOMPARE(tm.pttOutDelayMs(), 500);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SETTER + SIGNAL
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterEmitsAndUpdates() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::pttOutDelayMsChanged);
        tm.setPttOutDelayMs(50);
        QCOMPARE(tm.pttOutDelayMs(), 50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 50);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // PER-MAC PERSISTENCE
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void persistsPerMac() {
        TransmitModel tm;
        tm.loadFromSettings(kMac);
        tm.setPttOutDelayMs(75);

        TransmitModel reload;
        reload.loadFromSettings(kMac);
        QCOMPARE(reload.pttOutDelayMs(), 75);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelPttOutDelay)
#include "tst_transmit_model_ptt_out_delay.moc"
