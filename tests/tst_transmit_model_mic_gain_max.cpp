// no-port-check: NereusSDR-original unit-test file.  The "setup.cs" and
// "setup.designer.cs" references below are cite comments documenting which
// Thetis lines each assertion verifies; no Thetis logic is ported in this
// test file.
// =================================================================
// tests/tst_transmit_model_mic_gain_max.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::micGainMaxDb property.
//
// Ceiling for the MicGain slider; bound in Thetis to udMicGainMax spinbox.
// Per-MAC persisted under key MicGainMax.
//
// Source references (cited per assertion; no logic ported in this file):
//   setup.cs:9685-9689 [v2.10.3.13+501e3f51] — udMicGainMax_ValueChanged:
//     console.MicGainMax = (int)udMicGainMax.Value.
//   setup.designer.cs:46838-46866 [v2.10.3.13+501e3f51] — udMicGainMax
//     spinbox: Minimum=1, Maximum=70, Value=10 (default).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("CA:FE:BA:BE:00:42");
}

class TstTransmitModelMicGainMax : public QObject {
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

    void defaultIsPlusTen() {
        // From Thetis setup.designer.cs:46861-46865 [v2.10.3.13+501e3f51]:
        //   udMicGainMax.Value = new decimal(new int[] { 10, 0, 0, 0 });
        TransmitModel tm;
        QCOMPARE(tm.micGainMaxDb(), 10);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // CLAMPING (range 1..70 from setup.designer.cs:46847-46856)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterClampsBelow() {
        TransmitModel tm;
        tm.setMicGainMaxDb(-5);
        QCOMPARE(tm.micGainMaxDb(), 1);
    }

    void setterClampsAbove() {
        TransmitModel tm;
        tm.setMicGainMaxDb(200);
        QCOMPARE(tm.micGainMaxDb(), 70);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SETTER + SIGNAL
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterEmitsAndUpdates() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::micGainMaxDbChanged);
        tm.setMicGainMaxDb(25);
        QCOMPARE(tm.micGainMaxDb(), 25);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 25);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // PER-MAC PERSISTENCE
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void persistsPerMac() {
        TransmitModel tm;
        tm.loadFromSettings(kMac);
        tm.setMicGainMaxDb(40);

        TransmitModel reload;
        reload.loadFromSettings(kMac);
        QCOMPARE(reload.micGainMaxDb(), 40);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelMicGainMax)
#include "tst_transmit_model_mic_gain_max.moc"
