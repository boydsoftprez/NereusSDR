// =================================================================
// tests/tst_tx_interlock_policy.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent; TxInterlockPolicy is a
// NereusSDR-native class per design doc §4.9.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: disabledAlwaysAllows, blockDeniesWhenAmpStandby,
//                 warnAllowsButEmits.
// =================================================================

#include <QtTest/QtTest>
#include "core/TxInterlockPolicy.h"

class TxInterlockPolicyTest : public QObject {
    Q_OBJECT
private slots:
    void disabledAlwaysAllows();
    void blockDeniesWhenAmpStandby();
    void warnAllowsButEmits();
};

// Disabled mode must return true for any combination of inputs, including
// worst-case (ampPresent=false, ampInOperate=false, SWR=99).
void TxInterlockPolicyTest::disabledAlwaysAllows()
{
    NereusSDR::TxInterlockPolicy p;
    p.setMode(NereusSDR::TxInterlockPolicy::Disabled);
    QVERIFY(p.evaluateTxRequest(false, false, 99.0f));
    // Also verify with an amp present but not in operate -- still allowed.
    QVERIFY(p.evaluateTxRequest(true, false, 1.5f));
}

// Block mode must deny TX and emit denied() when the amp is present but
// not yet in OPERATE (standby state).
void TxInterlockPolicyTest::blockDeniesWhenAmpStandby()
{
    NereusSDR::TxInterlockPolicy p;
    p.setMode(NereusSDR::TxInterlockPolicy::Block);
    QSignalSpy spy(&p, &NereusSDR::TxInterlockPolicy::denied);
    QVERIFY(!p.evaluateTxRequest(true, false, 1.5f));
    QCOMPARE(spy.count(), 1);
}

// Warn mode must allow TX (return true) but emit warned() so the UI can
// toast the operator.
void TxInterlockPolicyTest::warnAllowsButEmits()
{
    NereusSDR::TxInterlockPolicy p;
    p.setMode(NereusSDR::TxInterlockPolicy::Warn);
    QSignalSpy spy(&p, &NereusSDR::TxInterlockPolicy::warned);
    QVERIFY(p.evaluateTxRequest(true, false, 1.5f));
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(TxInterlockPolicyTest)
#include "tst_tx_interlock_policy.moc"
