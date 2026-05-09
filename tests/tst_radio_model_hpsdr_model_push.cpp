// no-port-check: NereusSDR-original unit-test file.  No upstream logic
// is ported in this file.  The Thetis cite below documents the
// behavioural contract under test.
// =================================================================
// tests/tst_radio_model_hpsdr_model_push.cpp  (NereusSDR)
// =================================================================
//
// v0.4.1 hotfix regression test — RadioModel must push the connected
// hardware HPSDRModel into BOTH TransmitModel AND ReceiverManager when
// applying hardware info.  The ReceiverManager push was missing in
// v0.4.0, causing P1CodecStandard::applyPureSignalDdcConfig to fall
// through the model-switch's default branch and emit an empty
// PsDdcConfig — which kept PsccPump inactive and PureSignal correction
// from ever landing on Hermes / ANAN-10 / ANAN-10E / ANAN-100 /
// ANAN-100B / AnvelinaPro3-on-P1.  HL2 / G2 / Saturn were unaffected
// because their codecs ignore the model parameter.
//
// Source: ReceiverManager::updateDdcAssignment reads m_hpsdrModel and
// passes it to the codec; without a push from RadioModel the field
// stays at HPSDRModel::HPSDR (the safe Atlas-bus default initialised at
// ReceiverManager.cpp:142).  Thetis equivalent is implicit — the C#
// code reads HardwareSpecific.Model directly off the singleton, so no
// "push" exists upstream.
//
// Coverage:
//   §1 ReceiverManager defaults to HPSDR pre-push.
//   §2 setHpsdrModelForTest(ANAN10E) propagates to ReceiverManager.
//   §3 setHpsdrModelForTest(HERMES) propagates to ReceiverManager.
//   §4 setHpsdrModelForTest also keeps TransmitModel in sync.
//   §5 Repeated push with same model is idempotent.
// =================================================================

#include <QtTest/QtTest>
#include <QObject>

#include "core/HpsdrModel.h"
#include "core/ReceiverManager.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestRadioModelHpsdrModelPush : public QObject {
    Q_OBJECT

private slots:
    // ── §1 ReceiverManager defaults to HPSDR pre-push ────────────────────
    void receiverManager_defaultsToHpsdrAtlas() {
        RadioModel model;
        QCOMPARE(model.receiverManager()->hpsdrModel(), HPSDRModel::HPSDR);
    }

    // ── §2 setHpsdrModelForTest(ANAN10E) propagates to ReceiverManager ───
    // This is the hot-fix regression: prior to v0.4.1, only TransmitModel
    // got the push; ReceiverManager stayed at HPSDR, breaking
    // P1CodecStandard's model dispatch for HermesII boards.
    void setHpsdrModelForTest_anan10e_propagatesToReceiverManager() {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN10E);
        QCOMPARE(model.receiverManager()->hpsdrModel(), HPSDRModel::ANAN10E);
    }

    // ── §3 setHpsdrModelForTest(HERMES) propagates to ReceiverManager ────
    // Hermes-class (ANAN-10/100) also goes through P1CodecStandard's
    // model-switch and was equally broken in v0.4.0.
    void setHpsdrModelForTest_hermes_propagatesToReceiverManager() {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMES);
        QCOMPARE(model.receiverManager()->hpsdrModel(), HPSDRModel::HERMES);
    }

    // ── §4 TransmitModel still gets the push (unchanged behaviour) ───────
    // The pre-existing TransmitModel push (issue #175 fix at RadioModel.cpp:
    // 1398) must continue to fire after the refactor that adds the
    // ReceiverManager push.
    void setHpsdrModelForTest_alsoPushesToTransmitModel() {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN10E);
        QCOMPARE(model.transmitModel().hpsdrModel(), HPSDRModel::ANAN10E);
    }

    // ── §5 Repeated push with same model is idempotent ───────────────────
    // ReceiverManager::setHpsdrModel has an idempotent guard
    // (ReceiverManager.cpp:390) — re-setting the same model is a no-op.
    void setHpsdrModelForTest_idempotentRepeat() {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN10E);
        QCOMPARE(model.receiverManager()->hpsdrModel(), HPSDRModel::ANAN10E);

        // Re-push same model — should remain ANAN10E without any error
        // from the idempotent guard.
        model.setHpsdrModelForTest(HPSDRModel::ANAN10E);
        QCOMPARE(model.receiverManager()->hpsdrModel(), HPSDRModel::ANAN10E);
    }
};

QTEST_MAIN(TestRadioModelHpsdrModelPush)
#include "tst_radio_model_hpsdr_model_push.moc"
