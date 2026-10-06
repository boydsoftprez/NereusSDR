// no-port-check: NereusSDR-original asynchronous action regression.
#include <QtTest>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestPs3TwoToneCompletion : public QObject {
    Q_OBJECT
private slots:
    void stoppingWaitsForTheControllerToFinish()
    {
        AppSettings::instance().clear();
        TxChannel tx(0);
        MoxController mox;
        RadioModel radio;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TwoToneController* controller = radio.twoToneController();
        controller->setTxChannel(&tx);
        controller->setMoxController(&mox);
        controller->setSettleDelaysMs(0, 0);
        controller->setPowerOn(true);
        PureSignal coordinator(nullptr, &tx, nullptr, nullptr, nullptr, controller);
        coordinator.setTimersEnabled(false);
        PureSignalSessionFacade facade(&radio, &coordinator);
        controller->setActive(true);
        QTRY_VERIFY(controller->isActive());
        QVERIFY(facade.twoToneOn());
        QSignalSpy results(&facade, &PureSignalSessionFacade::actionResult);

        const Ps3ActionResult result = facade.executeAction(
            Ps3Action::SetTwoTone, {{"enabled", false}}, 1);
        QCOMPARE(result.phase, Ps3ActionPhase::Pending);
        QTRY_VERIFY(!controller->isActive());
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(qvariant_cast<Ps3ActionPhase>(results.first()[1]), Ps3ActionPhase::Completed);
        QVERIFY(!facade.twoToneOn());
        controller->setTxChannel(nullptr);
        controller->setMoxController(nullptr);
    }

    void startDuringStopIsRefusedWithoutReplacingTheStop()
    {
        AppSettings::instance().clear();
        TxChannel tx(0);
        MoxController mox;
        RadioModel radio;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TwoToneController* controller = radio.twoToneController();
        controller->setTxChannel(&tx);
        controller->setMoxController(&mox);
        controller->setSettleDelaysMs(0, 0);
        controller->setPowerOn(true);
        PureSignal coordinator(nullptr, &tx, nullptr, nullptr, nullptr, controller);
        coordinator.setTimersEnabled(false);
        PureSignalSessionFacade facade(&radio, &coordinator);
        controller->setActive(true);
        QTRY_VERIFY(controller->isActive());
        QSignalSpy results(&facade, &PureSignalSessionFacade::actionResult);

        const Ps3ActionResult stop = facade.executeAction(
            Ps3Action::SetTwoTone, {{"enabled", false}}, 1);
        QCOMPARE(stop.phase, Ps3ActionPhase::Pending);
        const Ps3ActionResult restart = facade.executeAction(
            Ps3Action::SetTwoTone, {{"enabled", true}}, 2);
        QCOMPARE(restart.phase, Ps3ActionPhase::Failed);
        QVERIFY(!restart.reason.isEmpty());
        QTRY_VERIFY(!controller->isActive());
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(results.first()[0].toUInt(), 1U);
        QCOMPARE(qvariant_cast<Ps3ActionPhase>(results.first()[1]), Ps3ActionPhase::Completed);
        controller->setTxChannel(nullptr);
        controller->setMoxController(nullptr);
    }

    void cancelingActivationWaitsForStopToFinish()
    {
        AppSettings::instance().clear();
        TxChannel tx(0);
        MoxController mox;
        RadioModel radio;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TwoToneController* controller = radio.twoToneController();
        controller->setTxChannel(&tx);
        controller->setMoxController(&mox);
        controller->setSettleDelaysMs(0, 0);
        controller->setPowerOn(true);
        controller->setTuneOffPendingFn([] { return true; });
        PureSignal coordinator(nullptr, &tx, nullptr, nullptr, nullptr, controller);
        coordinator.setTimersEnabled(false);
        PureSignalSessionFacade facade(&radio, &coordinator);
        QSignalSpy results(&facade, &PureSignalSessionFacade::actionResult);

        const Ps3ActionResult start = facade.executeAction(
            Ps3Action::SetTwoTone, {{"enabled", true}}, 1);
        QCOMPARE(start.phase, Ps3ActionPhase::Pending);
        QVERIFY(!controller->isActive());
        const Ps3ActionResult stop = facade.executeAction(
            Ps3Action::SetTwoTone, {{"enabled", false}}, 2);
        QCOMPARE(stop.phase, Ps3ActionPhase::Pending);
        QTRY_VERIFY(!controller->isActivationInFlight());
        coordinator.pollTimerTick();
        QTRY_COMPARE(results.size(), 2);
        QCOMPARE(results.first()[0].toUInt(), 1U);
        QCOMPARE(qvariant_cast<Ps3ActionPhase>(results.first()[1]), Ps3ActionPhase::Failed);
        QCOMPARE(results.last()[0].toUInt(), 2U);
        QCOMPARE(qvariant_cast<Ps3ActionPhase>(results.last()[1]), Ps3ActionPhase::Completed);
        controller->setTxChannel(nullptr);
        controller->setMoxController(nullptr);
    }
};

QTEST_MAIN(TestPs3TwoToneCompletion)
#include "tst_ps3_two_tone_completion.moc"
