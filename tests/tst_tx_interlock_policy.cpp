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
//   2026-09-24  J.J. Boyd (KG4VCF), R-R3-47 / R-R3-22: the Core reloads the
//                 policy when a window writes its settings, a remote window
//                 holds the Core's policy without saving it, and the
//                 setTxInterlockPolicy command is applied on the Core (ranges
//                 checked, mirrored on `accessoryData`, enforcement there).
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <tuple>
#include "core/TxInterlockPolicy.h"
#include "core/AppSettings.h"
#include "core/StationAccessoryData.h"
#include "models/AccessoryDataModel.h"
#include "OperatorWording.h"

class TxInterlockPolicyTest : public QObject {
    Q_OBJECT
private slots:
    void disabledAlwaysAllows();
    void blockDeniesWhenAmpStandby();
    void warnAllowsButEmits();
    // R-R3-47 / R-R3-22
    void coreReloadsWhenAWindowWritesTheSettings();
    void remoteWindowHoldsTheCoresPolicyWithoutSaving();
    void commandIsAppliedOnTheCoreAndMirrored();
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

// R-R3-47: a window that writes the settings (an app before the command)
// reaches the Core's live policy at once, not at the next restart.
void TxInterlockPolicyTest::coreReloadsWhenAWindowWritesTheSettings()
{
    auto& s = NereusSDR::AppSettings::instance();
    s.clear();
    NereusSDR::TxInterlockPolicy p;
    QCOMPARE(p.mode(), NereusSDR::TxInterlockPolicy::Disabled);
    QSignalSpy changed(&p, &NereusSDR::TxInterlockPolicy::changed);
    s.setValue(QStringLiteral("PGXL_TxInterlockMode"), QStringLiteral("Block"));
    s.setValue(QStringLiteral("PGXL_TxSwrGate"), QStringLiteral("True"));
    p.reloadFromSettings();
    QCOMPARE(changed.count(), 1);
    QCOMPARE(p.mode(), NereusSDR::TxInterlockPolicy::Block);
    QVERIFY(p.swrGateEnabled());
    p.reloadFromSettings();
    QCOMPARE(changed.count(), 1);   // nothing moved
    QVERIFY(!p.evaluateTxRequest(true, false, 1.0f));
    s.clear();
}

// R-R3-47: a remote window's policy is the Core's: applied, never saved.
void TxInterlockPolicyTest::remoteWindowHoldsTheCoresPolicyWithoutSaving()
{
    auto& s = NereusSDR::AppSettings::instance();
    s.clear();
    NereusSDR::TxInterlockPolicy p;
    QSignalSpy changed(&p, &NereusSDR::TxInterlockPolicy::changed);
    p.applyMirrored(NereusSDR::TxInterlockPolicy::Warn, 1500, true, 2.5f);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(p.mode(), NereusSDR::TxInterlockPolicy::Warn);
    QCOMPARE(p.graceMs(), 1500);
    QVERIFY(p.swrGateEnabled());
    QVERIFY(qFuzzyCompare(p.swrGateMax(), 2.5f));
    QVERIFY(!s.contains(QStringLiteral("PGXL_TxInterlockMode")));
    QVERIFY(!s.contains(QStringLiteral("PGXL_TxInterlockGraceMs")));
    p.applyMirrored(NereusSDR::TxInterlockPolicy::Warn, 1500, true, 2.5f);
    QCOMPARE(changed.count(), 1);
}

// R-R3-47 / R-R3-22: the command is applied by the Core's policy (so its
// enforcement, which stays there, uses it from the next request on), saved
// on the Core, and published on `accessoryData`; a request out of range
// changes nothing and says why in plain words.
void TxInterlockPolicyTest::commandIsAppliedOnTheCoreAndMirrored()
{
    using NereusSDR::AccessoryDataModel;
    auto& s = NereusSDR::AppSettings::instance();
    s.clear();
    NereusSDR::TxInterlockPolicy policy;
    AccessoryDataModel data;
    NereusSDR::StationAccessoryData::Sources sources;
    sources.interlock = &policy;
    NereusSDR::StationAccessoryData core(&data, sources);
    QCOMPARE(data.interlockMode(), AccessoryDataModel::InterlockMode::Disabled);
    QVERIFY(policy.evaluateTxRequest(true, false, 1.0f));

    QSignalSpy mirrored(&data, &AccessoryDataModel::interlockChanged);
    QString reason;
    QVERIFY(core.setInterlockPolicy(2, 2500, true, 2.0, &reason));
    QCOMPARE(policy.mode(), NereusSDR::TxInterlockPolicy::Block);
    QCOMPARE(policy.graceMs(), 2500);
    QVERIFY(policy.swrGateEnabled());
    QVERIFY(qFuzzyCompare(policy.swrGateMax(), 2.0f));
    QCOMPARE(s.value(QStringLiteral("PGXL_TxInterlockMode")).toString(), QStringLiteral("Block"));
    QVERIFY(mirrored.count() >= 1);
    QCOMPARE(data.interlockMode(), AccessoryDataModel::InterlockMode::Block);
    QCOMPARE(data.interlockGraceMs(), 2500);
    QVERIFY(data.interlockSwrGateEnabled());
    QCOMPARE(data.interlockSwrGateMax(), 2.0);
    // The refusal still happens where the policy is enforced.
    QSignalSpy denied(&policy, &NereusSDR::TxInterlockPolicy::denied);
    QVERIFY(!policy.evaluateTxRequest(true, false, 1.0f));
    QCOMPARE(denied.count(), 1);

    // Out of range: nothing changes.
    for (const auto& bad : { std::tuple{3, 1000, 2.0}, std::tuple{-1, 1000, 2.0},
                             std::tuple{1, 30001, 2.0}, std::tuple{1, -1, 2.0},
                             std::tuple{1, 1000, 0.5}, std::tuple{1, 1000, 10.5} }) {
        reason.clear();
        QVERIFY(!core.setInterlockPolicy(std::get<0>(bad), std::get<1>(bad), false,
                                         std::get<2>(bad), &reason));
        QVERIFY(NereusSDR::OperatorWording::isPlain(reason));
    }
    QCOMPARE(policy.mode(), NereusSDR::TxInterlockPolicy::Block);
    QCOMPARE(data.interlockGraceMs(), 2500);

    // A window writing the settings reaches the Core's policy too.
    s.setValue(QStringLiteral("PGXL_TxInterlockMode"), QStringLiteral("Warn"));
    core.applySetting(QStringLiteral("PGXL_TxInterlockMode"));
    QCOMPARE(policy.mode(), NereusSDR::TxInterlockPolicy::Warn);
    QCOMPARE(data.interlockMode(), AccessoryDataModel::InterlockMode::Warn);
    s.clear();
}

QTEST_GUILESS_MAIN(TxInterlockPolicyTest)
#include "tst_tx_interlock_policy.moc"
