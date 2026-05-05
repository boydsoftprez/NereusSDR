// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// reference below is a cite comment documenting which Thetis line each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_all_mode_mic_ptt.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::allModeMicPTT property.
//
// MoxController gate that consumes this property is added in a later task
// (radio-mic-input Task 6 lands the model + persistence only).  Per-MAC
// persisted under key All_Mode_Mic_PTT.
//
// Source references (cited per assertion; no logic ported in this file):
//   console.cs:12022 [v2.10.3.13+501e3f51] — _all_mode_mic_ptt = false
//     property declaration (default value).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:FF");
}

class TstTransmitModelAllModeMicPtt : public QObject {
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
        // From Thetis console.cs:12022 [v2.10.3.13+501e3f51]:
        //   private bool _all_mode_mic_ptt = false;
        TransmitModel tm;
        QCOMPARE(tm.allModeMicPTT(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // SETTER + SIGNAL
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void setterEmitsSignalAndUpdates() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::allModeMicPTTChanged);
        tm.setAllModeMicPTT(true);
        QCOMPARE(tm.allModeMicPTT(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // IDEMPOTENT GUARD
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void idempotentSetterDoesNotEmit() {
        // setAllModeMicPTT(false) on a fresh model (default = false) must NOT emit.
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::allModeMicPTTChanged);
        tm.setAllModeMicPTT(false);
        QCOMPARE(spy.count(), 0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // PER-MAC PERSISTENCE
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    void persistsPerMac() {
        TransmitModel tm;
        tm.loadFromSettings(kMac);
        tm.setAllModeMicPTT(true);

        TransmitModel reload;
        reload.loadFromSettings(kMac);
        QCOMPARE(reload.allModeMicPTT(), true);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelAllModeMicPtt)
#include "tst_transmit_model_all_mode_mic_ptt.moc"
