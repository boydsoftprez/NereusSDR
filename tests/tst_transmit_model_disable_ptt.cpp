// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// reference below is a cite comment documenting which Thetis line each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_disable_ptt.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::disablePTT property.
//
// Property gates the entire PollPTT loop (radio-mic-input Thetis parity
// epic, Task 5).  Per-MAC persisted under key Disable_PTT.
//
// Source references (cited per assertion; no logic ported in this file):
//   console.cs:19750-19755 [v2.10.3.13+501e3f51] — _disable_ptt = false
//     property declaration (default value).
//   setup.cs:6535-6539 [v2.10.3.13+501e3f51] —
//     chkGeneralDisablePTT_CheckedChanged: console.DisablePTT = checkbox.Checked
//     (the Setup checkbox the property binds to in Thetis).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:FF");
}

class TstTransmitModelDisablePtt : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() {
        AppSettings::instance().clear();
    }
    void init() {
        // Clear before each test for persistence isolation.
        AppSettings::instance().clear();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // DEFAULT
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void defaultIsFalse() {
        // From Thetis console.cs:19750 [v2.10.3.13+501e3f51]:
        //   private bool _disable_ptt = false;
        TransmitModel tm;
        QCOMPARE(tm.disablePTT(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SETTER + SIGNAL
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterEmitsSignalAndUpdates() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::disablePTTChanged);
        tm.setDisablePTT(true);
        QCOMPARE(tm.disablePTT(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // IDEMPOTENT GUARD
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void idempotentSetterDoesNotEmit() {
        // setDisablePTT(false) on a fresh model (default = false) must NOT emit.
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::disablePTTChanged);
        tm.setDisablePTT(false);
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // PER-MAC PERSISTENCE
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void persistsPerMac() {
        TransmitModel tm;
        tm.loadFromSettings(kMac);
        tm.setDisablePTT(true);

        TransmitModel reload;
        reload.loadFromSettings(kMac);
        QCOMPARE(reload.disablePTT(), true);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelDisablePtt)
#include "tst_transmit_model_disable_ptt.moc"
