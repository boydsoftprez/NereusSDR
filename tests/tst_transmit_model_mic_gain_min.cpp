// no-port-check: NereusSDR-original unit-test file.  The "setup.cs" and
// "setup.designer.cs" references below are cite comments documenting which
// Thetis lines each assertion verifies; no Thetis logic is ported in this
// test file.
// =================================================================
// tests/tst_transmit_model_mic_gain_min.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::micGainMinDb property.
//
// Floor for the MicGain slider; bound in Thetis to udMicGainMin spinbox.
// Per-MAC persisted under key MicGainMin.
//
// Source references (cited per assertion; no logic ported in this file):
//   setup.cs:9678-9683 [v2.10.3.13+501e3f51] — udMicGainMin_ValueChanged:
//     console.MicGainMin = (int)udMicGainMin.Value.
//   setup.designer.cs:46808-46835 [v2.10.3.13+501e3f51] — udMicGainMin
//     spinbox: Minimum=-96, Maximum=0, Value=-40 (default).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("DE:AD:BE:EF:00:11");
}

class TstTransmitModelMicGainMin : public QObject {
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

    void defaultIsMinusForty() {
        // From Thetis setup.designer.cs:46830-46834 [v2.10.3.13+501e3f51]:
        //   udMicGainMin.Value = new decimal(new int[] { 40, 0, 0, -2147483648 });
        //   (sign-bit set in high word -> -40)
        TransmitModel tm;
        QCOMPARE(tm.micGainMinDb(), -40);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // CLAMPING (range -96..0 from setup.designer.cs:46821-46825)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterClampsBelow() {
        TransmitModel tm;
        tm.setMicGainMinDb(-200);
        QCOMPARE(tm.micGainMinDb(), -96);
    }

    void setterClampsAbove() {
        TransmitModel tm;
        tm.setMicGainMinDb(50);
        QCOMPARE(tm.micGainMinDb(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SETTER + SIGNAL
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterEmitsAndUpdates() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::micGainMinDbChanged);
        tm.setMicGainMinDb(-20);
        QCOMPARE(tm.micGainMinDb(), -20);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), -20);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // PER-MAC PERSISTENCE
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void persistsPerMac() {
        TransmitModel tm;
        tm.loadFromSettings(kMac);
        tm.setMicGainMinDb(-30);

        TransmitModel reload;
        reload.loadFromSettings(kMac);
        QCOMPARE(reload.micGainMinDb(), -30);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelMicGainMin)
#include "tst_transmit_model_mic_gain_min.moc"
