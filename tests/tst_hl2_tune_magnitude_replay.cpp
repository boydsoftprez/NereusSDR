// no-port-check: NereusSDR-original integration regressions, no upstream
// code translated. References: mi0bot console.cs:47660-47673 (HL2 TUNE
// magnitude), Thetis setup.cs:18980-18996 (anti-VOX run and tau).
//
// Actual RadioModel TUNE paths against the loopback
// P1 fake. WDSP state is read through a C probe rather than a cached getter.
// No real audio device or radio is opened by ConnectableRadioModel.
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "fakes/ConnectableRadioModel.h"

#include <array>

#ifdef HAVE_WDSP
extern "C" {
double nereus_issue256_tone_magnitude(int channel);
void nereus_issue256_disable_display_siphon(int channel);
}
#endif

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;

class TestHl2TuneMagnitudeReplay : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void repeatedHl2TuneKeepsItsActualMagnitude_data()
    {
        QTest::addColumn<int>("tuneValue");
        QTest::addColumn<bool>("moveRfPower");
        QTest::addColumn<double>("expectedMagnitude");
        QTest::newRow("same-tune") << 25 << false << 0.65;
        QTest::newRow("rf-slider-between-tunes") << 25 << true << 0.65;
        QTest::newRow("upper-sub-step") << 51 << true << 0.91;
    }

    void repeatedHl2TuneKeepsItsActualMagnitude()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires the real WDSP transmit channel");
#else
        QFETCH(int, tuneValue);
        QFETCH(bool, moveRfPower);
        QFETCH(double, expectedMagnitude);
        auto harness = ConnectableRadioModel::create();
        QVERIFY(harness);
        RadioModel& model = harness->model();
        QVERIFY(model.txChannel());
        QCOMPARE(model.hardwareProfile().model, HPSDRModel::HERMESLITE);
        QVERIFY(model.activeSlice());
        model.activeSlice()->setFrequency(7'100'000.0);
        model.activeSlice()->setDspMode(DSPMode::USB);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        TransmitModel& tx = model.transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        tx.setTunePowerForBand(Band::Band40m, tuneValue);
        const int channelId = model.txChannel()->channelId();
        QVERIFY(model.waitForTransmitLaneForTest(5000));
        // MainWindow normally owns the TX analyzer. This model-only fixture
        // has no display, so don't feed Spectrum0 with an absent analyzer.
        nereus_issue256_disable_display_siphon(channelId);

        model.setTune(true);
        QTRY_VERIFY_WITH_TIMEOUT(model.mox(), 5000);
        QVERIFY(model.waitForTransmitLaneForTest(5000));
        QCOMPARE(nereus_issue256_tone_magnitude(channelId), expectedMagnitude);
        model.setTune(false);
        QTRY_VERIFY_WITH_TIMEOUT(!model.mox() && !model.tuneOffPendingForTest(), 5000);
        QVERIFY(model.waitForTransmitLaneForTest(5000));
        if (moveRfPower) {
            tx.setPower(tx.power() == 80 ? 90 : 80);
            QVERIFY(model.waitForTransmitLaneForTest(5000));
        }
        model.setTune(true);
        QTRY_VERIFY_WITH_TIMEOUT(model.mox(), 5000);
        QVERIFY(model.waitForTransmitLaneForTest(5000));
        QCOMPARE(tx.tunePowerForBand(Band::Band40m), tuneValue);
        QCOMPARE(tx.txPostGenToneMag(), expectedMagnitude);
        QCOMPARE(nereus_issue256_tone_magnitude(channelId), expectedMagnitude);
        model.setTune(false);
        QTRY_VERIFY_WITH_TIMEOUT(!model.mox() && !model.tuneOffPendingForTest(), 5000);
#endif
    }

};

QTEST_MAIN(TestHl2TuneMagnitudeReplay)
#include "tst_hl2_tune_magnitude_replay.moc"
