// no-port-check: NereusSDR-original CAT accepted-activation ownership tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/cat/CatTxCoordinator.h"
#include "core/cat/CatService.h"
#include "core/RadioConnection.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/TxSliceArbiter.h"
#include "core/TwoToneController.h"
#include "core/TxInterlockPolicy.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
class CatMockConnection : public RadioConnection {
    Q_OBJECT
public:
    // Ordered log of setTxDrive() argument values.
    QList<int> txDriveLog;
    // Ordered log of setTxFrequency() argument values (TUNE VFO offset).
    QList<quint64> txFreqLog;

    explicit CatMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    // ── Pure-virtual overrides ────────────────────────────────────────────────
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64 hz) override { txFreqLog.append(hz); }
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int level) override { txDriveLog.append(level); }
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};


static void pumpCat() { for (int i = 0; i < 5; ++i) { QCoreApplication::processEvents(); } }
class TstCatTxOwnership : public QObject {
    Q_OBJECT
    void setup(RadioModel& model, CatMockConnection& conn)
    {
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice(); model.addSlice();
        model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        model.sliceOwnership()->setOwner(1, SliceOwnership::stationDevice());
        model.sliceById(0)->setFrequency(14'200'000);
        model.sliceById(1)->setFrequency(14'250'000);
        // PTT admission is exercised through real MOX/gate below; avoid unavailable mic fixture.
        model.moxController()->setMoxCheck([] { return safety::BandPlanGuard::MoxCheckResult{true, QString()}; });
    }
private slots:
    void newerGateCallbackIntentAbortsOlderCatAdmission_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("PTT") << static_cast<int>(CatTransmitKind::Ptt);
        QTest::newRow("tune") << static_cast<int>(CatTransmitKind::Tune);
        QTest::newRow("two-tone") << static_cast<int>(CatTransmitKind::TwoTone);
    }
    void newerGateCallbackIntentAbortsOlderCatAdmission()
    {
        QFETCH(int, kind);
        CatMockConnection conn; TxChannel channel(1); RadioModel model; setup(model, conn);
        model.injectTxChannelForTest(&channel);
        model.twoToneController()->setTxChannel(&channel); model.twoToneController()->setPowerOn(true);
        CatTxCoordinator tx(model);
        MoxController* mox = model.moxController();
        mox->setKeyingGate([mox](PttMode, const KeyerIdentity& keyer) {
            if (keyer.requestTag) { mox->setMox(true); }
            return KeyingAnswer{};
        });
        QVERIFY(!tx.requestTransmit(1, 0, static_cast<CatTransmitKind>(kind))); pumpCat();
        QVERIFY(mox->isMox()); QCOMPARE(mox->pttMode(), PttMode::None);
        QVERIFY(!model.isTune()); QVERIFY(!model.twoToneController()->isActive());
        mox->onVoxActive(false); tx.releasePtt(1); pumpCat(); QVERIFY(mox->isMox());
    }

    void newerIdleGateIntentAbortsToneAdmission_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("tune") << static_cast<int>(CatTransmitKind::Tune);
        QTest::newRow("two-tone") << static_cast<int>(CatTransmitKind::TwoTone);
    }
    void newerIdleGateIntentAbortsToneAdmission()
    {
        QFETCH(int, kind);
        CatMockConnection conn; TxChannel channel(1); RadioModel model; setup(model, conn);
        model.injectTxChannelForTest(&channel);
        model.twoToneController()->setTxChannel(&channel); model.twoToneController()->setPowerOn(true);
        CatTxCoordinator tx(model);
        MoxController* mox = model.moxController();
        mox->setKeyingGate([mox](PttMode, const KeyerIdentity& keyer) {
            if (keyer.requestTag) { mox->setMox(false); }
            return KeyingAnswer{};
        });
        QVERIFY(!tx.requestTransmit(1, 0, static_cast<CatTransmitKind>(kind))); pumpCat();
        QVERIFY(!mox->isMox()); QVERIFY(!model.isTune());
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!model.twoToneController()->isActivationInFlight());
        QVERIFY(!mox->isManualKey());
    }

    void cancellationDuringAdmission_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("PTT") << static_cast<int>(CatTransmitKind::Ptt);
        QTest::newRow("tune") << static_cast<int>(CatTransmitKind::Tune);
        QTest::newRow("two-tone") << static_cast<int>(CatTransmitKind::TwoTone);
    }
    void cancellationDuringAdmission()
    {
        QFETCH(int, kind);
        CatMockConnection conn; TxChannel channel(1); RadioModel model; setup(model, conn);
        model.injectTxChannelForTest(&channel);
        model.twoToneController()->setTxChannel(&channel); model.twoToneController()->setPowerOn(true);
        CatTxCoordinator tx(model);
        MoxController* mox = model.moxController();
        const quint64 stamp = mox->acceptedRequestGeneration();
        mox->setKeyingGate([&tx](PttMode, const KeyerIdentity&) { tx.cancelAll(); return KeyingAnswer{}; });
        QVERIFY(!tx.requestTransmit(1, 0, static_cast<CatTransmitKind>(kind))); pumpCat();
        QCOMPARE(mox->acceptedRequestGeneration(), stamp);
        QVERIFY(!mox->isMox()); QVERIFY(!model.isTune()); QVERIFY(!mox->isManualKey());
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!model.twoToneController()->isActivationInFlight());
    }

    void legacyExplicitCatRepeatSupersedesTaggedClaim()
    {
        CatMockConnection conn; RadioModel model; setup(model, conn); CatTxCoordinator tx(model);
        QVERIFY(tx.requestPtt(1, 0)); pumpCat();
        const quint64 stamp = model.moxController()->acceptedRequestGeneration();
        model.moxController()->onCatPtt(true);
        QVERIFY(model.moxController()->acceptedRequestGeneration() > stamp);
        tx.releasePtt(1); model.moxController()->onVoxActive(false); pumpCat();
        QVERIFY(model.moxController()->isMox());
        model.moxController()->onCatPtt(false); pumpCat(); QVERIFY(!model.moxController()->isMox());
    }

    void tuneAndTwoToneSupersession_data()
    {
        QTest::addColumn<bool>("twoTone"); QTest::addColumn<bool>("tci");
        QTest::newRow("tune/operator") << false << false;
        QTest::newRow("tune/TCI") << false << true;
        QTest::newRow("two-tone/operator") << true << false;
        QTest::newRow("two-tone/TCI") << true << true;
    }
    void tuneAndTwoToneSupersession()
    {
        QFETCH(bool, twoTone); QFETCH(bool, tci);
        CatMockConnection conn; TxChannel tc(1); RadioModel model; setup(model, conn);
        model.injectTxChannelForTest(&tc);
        model.twoToneController()->setTxChannel(&tc); model.twoToneController()->setPowerOn(true);
        model.twoToneController()->setSettleDelaysMs(0, 0);
        CatTxCoordinator tx(model);
        const CatTransmitKind kind = twoTone ? CatTransmitKind::TwoTone : CatTransmitKind::Tune;
        QVERIFY(tx.requestTransmit(1, 0, kind)); pumpCat();
        QVERIFY(!tx.requestTransmit(2, 0, kind)); QVERIFY(!tx.requestPtt(2, 0));
        if (tci) { model.setMox(true); }
        else if (twoTone) { model.twoToneController()->setActive(true); }
        else { model.setTune(true); }
        const int driveCount = conn.txDriveLog.size();
        tx.releaseTransmit(1); pumpCat();
        QVERIFY(model.moxController()->isMox());
        QCOMPARE(conn.txDriveLog.size(), driveCount);
    }
    void cancellationBeforeTonePreparation_data()
    {
        QTest::addColumn<bool>("twoTone"); QTest::newRow("tune") << false; QTest::newRow("two-tone") << true;
    }
    void cancellationBeforeTonePreparation()
    {
        QFETCH(bool, twoTone);
        CatMockConnection conn; TxChannel tc(1); RadioModel model; setup(model, conn);
        model.injectTxChannelForTest(&tc); model.twoToneController()->setTxChannel(&tc);
        model.twoToneController()->setPowerOn(true);
        CatTxCoordinator tx(model);
        connect(model.moxController(), &MoxController::requestAccepted, &tx,
            [&tx](const KeyerIdentity& keyer, quint64, bool on) { if (on && keyer.requestTag) { tx.cancelAll(); } });
        QVERIFY(!tx.requestTransmit(1, 0, twoTone ? CatTransmitKind::TwoTone : CatTransmitKind::Tune));
        pumpCat(); QVERIFY(!model.moxController()->isMox());
        QVERIFY(!model.isTune()); QVERIFY(!model.twoToneController()->isActive());
    }
    void existingAdmissionGatesRefuseWithoutTaking()
    {
        CatMockConnection conn; RadioModel model; setup(model, conn); CatTxCoordinator tx(model);
        model.moxController()->setKeyingGate([](PttMode mode, const KeyerIdentity& keyer) {
            Q_ASSERT(mode == PttMode::Cat && keyer.isStation() && keyer.program && keyer.session.isEmpty());
            return KeyingAnswer{KeyingVerdict::Refuse, TxRefusals::changingHands()};
        });
        const quint64 before = model.moxController()->acceptedRequestGeneration();
        QVERIFY(!tx.requestPtt(1, 0));
        QCOMPARE(model.moxController()->acceptedRequestGeneration(), before);
        // Preserve that real gate; it still gets the next request.
        QVERIFY(!tx.requestPtt(2, 0));
        model.moxController()->setKeyingGate({});
        TxInterlockPolicy policy;
        policy.applyMirrored(TxInterlockPolicy::Mode::Block, 0, false, 3.0f);
        model.moxController()->setInterlockPolicy(&policy);
        model.moxController()->onAmpStateChanged(true, false);
        QVERIFY(!tx.requestPtt(1, 0));
        QCOMPARE(model.moxController()->acceptedRequestGeneration(), before);
        model.moxController()->setInterlockPolicy(nullptr);
        model.moxController()->setRxOnly(true);
        QVERIFY(!tx.requestPtt(1, 0));
        QCOMPARE(model.moxController()->acceptedRequestGeneration(), before);
    }
    void externalHandoffNeverQueuesCatKey()
    {
        CatMockConnection conn; RadioModel model; setup(model, conn); CatTxCoordinator tx(model);
        QVERIFY(tx.requestTxSelection(1, 1));
        QVERIFY(tx.requestPtt(1, 1)); pumpCat();
        QVERIFY(!tx.requestTxSelection(2, 0));
        QVERIFY(model.txSliceArbiter()->requestHandoff(0, SliceOwnership::stationDevice()));
        pumpCat(); QVERIFY(!model.moxController()->isMox());
        tx.releasePtt(1); model.moxController()->onVoxActive(false); pumpCat();
        QVERIFY(!model.moxController()->isMox());
    }
    void startupDoesNotKeyOrCreateSlice()
    {
        RadioModel model; CatTxCoordinator tx(model);
        const int count = model.slices().size();
        QVERIFY(!tx.requestPtt(1, 0)); model.catService()->startConfigured();
        QCOMPARE(model.slices().size(), count); QVERIFY(!model.moxController()->isMox());
    }

    void sharedTargetAndConflictingClaims()
    {
        CatMockConnection conn; RadioModel model; setup(model, conn);
        CatTxCoordinator tx(model);
        QVERIFY(tx.requestPtt(1, 0)); QVERIFY(tx.requestPtt(2, 0));
        QVERIFY(!tx.requestPtt(3, 1)); QVERIFY(!tx.requestTxSelection(3, 1));
        tx.cancelSession(1); QVERIFY(model.moxController()->isMox());
        tx.releasePtt(2); pumpCat(); QVERIFY(!model.moxController()->isMox());
        QVERIFY(tx.requestPtt(3, 1)); tx.cancelAll(); pumpCat();
        QVERIFY(!model.moxController()->isMox());
    }
    void operatorAndTciSupersede_data()
    {
        QTest::addColumn<bool>("tci"); QTest::newRow("operator") << false; QTest::newRow("TCI") << true;
    }
    void operatorAndTciSupersede()
    {
        QFETCH(bool, tci);
        CatMockConnection conn; RadioModel model; setup(model, conn);
        CatTxCoordinator tx(model);
        QVERIFY(tx.requestPtt(1, 0)); pumpCat();
        if (tci) { model.setMox(true); } else { model.moxController()->setMox(true); }
        tx.releasePtt(1);
        model.moxController()->onVoxActive(false); model.moxController()->onMicPttFromRadio(false);
        pumpCat(); QVERIFY(model.moxController()->isMox());
        QVERIFY(!tx.requestPtt(2, 0));
    }
    void rejectionRetainsClaimsAndForeignOwnershipRefuses()
    {
        CatMockConnection conn; RadioModel model; setup(model, conn);
        CatTxCoordinator tx(model);
        model.moxController()->setKeyingGate([](PttMode, const KeyerIdentity& keyer) {
            if (keyer.requestTag == 0) { return KeyingAnswer{KeyingVerdict::Refuse, TxRefusals::changingHands()}; }
            return KeyingAnswer{};
        });
        QVERIFY(tx.requestPtt(1, 0));
        const quint64 stamp = model.moxController()->acceptedRequestGeneration();
        KeyerIdentity foreign; foreign.deviceId = "foreign";
        model.moxController()->setMox(true, foreign);
        QCOMPARE(model.moxController()->acceptedRequestGeneration(), stamp);
        tx.releasePtt(1); pumpCat(); QVERIFY(!model.moxController()->isMox());
        model.sliceOwnership()->setOwner(0, "foreign");
        QVERIFY(!tx.requestPtt(1, 0));
    }
    void lifecycleAndIncarnation()
    {
        CatMockConnection conn; RadioModel model; setup(model, conn);
        CatTxCoordinator tx(model);
        QVERIFY(tx.requestPtt(1, 0));
        model.removeSlice(0); pumpCat();
        QCOMPARE(model.addSlice(), 0);
        QVERIFY(!model.moxController()->isMox());
        tx.releasePtt(1); QVERIFY(!model.moxController()->isMox());
        model.injectConnectionForTest(nullptr);
        QVERIFY(!tx.requestPtt(1, 0));
    }
    void observationCancellationBeforeActivation()
    {
        CatMockConnection conn; RadioModel model; setup(model, conn);
        CatTxCoordinator tx(model);
        connect(model.moxController(), &MoxController::requestAccepted, &tx,
            [&tx](const KeyerIdentity& keyer, quint64, bool on) { if (on && keyer.requestTag) { tx.cancelAll(); } });
        QVERIFY(!tx.requestPtt(1, 0)); pumpCat(); QVERIFY(!model.moxController()->isMox());
    }
};
QTEST_MAIN(TstCatTxOwnership)
#include "tst_cat_tx_ownership.moc"
