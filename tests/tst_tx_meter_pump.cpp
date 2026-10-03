// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_tx_meter_pump.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 39 (D14, R-IOS-13): the Core's transmit meter pump.
//
//   1. It reads ten times a second while running, and not at all stopped:
//      its timer is 100 ms, a child of the pump (so the conformance
//      runner's virtual clock drives it), and each tick hands out one
//      reading from its source.
//   2. The power meters are the radio's readings (RadioStatus): forward
//      and reflected power and the SWR worked from them. With no transmit
//      channel there is no ALC or MIC reading.
//   3. The ALC and MIC readings come from the transmit lane's cache: a
//      TxChannel with a lane that has not run answers at once, on this
//      thread, with the cache's idle readings as Thetis shows them (MIC
//      -195 dB, ALC -30 dB), so a poll never waits for the lane or calls
//      WDSP on the event loop.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 39 (D14, R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/DspControlThread.h"
#include "core/RadioStatus.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/meters/TxMeterPump.h"

using namespace NereusSDR;

namespace {

QTimer* timerOf(TxMeterPump& pump)
{
    return pump.findChild<QTimer*>();
}

// Fires the pump's timer once, as its interval passing would.
void tick(TxMeterPump& pump)
{
    QTimer* timer = timerOf(pump);
    QVERIFY(timer != nullptr);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
}

} // namespace

class TstTxMeterPump : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<NereusSDR::TxMeterReadings>(); }

    void itReadsTenTimesASecondWhileRunning()
    {
        TxMeterPump pump(nullptr);
        int reads = 0;
        pump.setSource([&reads]() {
            ++reads;
            TxMeterReadings r;
            r.forwardPowerWatts = 10.0 * reads;
            return r;
        });
        QSignalSpy taken(&pump, &TxMeterPump::readingsTaken);

        QTimer* timer = timerOf(pump);
        QVERIFY2(timer != nullptr, "the pump's timer is not its child");
        QCOMPARE(TxMeterPump::kIntervalMs, 100);
        QCOMPARE(timer->interval(), 100);
        QVERIFY(!pump.isRunning());

        pump.start();
        QVERIFY(pump.isRunning());
        for (int i = 0; i < 10; ++i) {
            tick(pump);
        }
        QCOMPARE(reads, 10);
        QCOMPARE(taken.count(), 10);
        QCOMPARE(taken.last().at(0).value<TxMeterReadings>().forwardPowerWatts, 100.0);

        pump.stop();
        QVERIFY(!pump.isRunning());
        QVERIFY(!timer->isActive());
    }

    void itReadsTheRadiosPowerReadings()
    {
        RadioStatus status;
        status.setForwardPower(100.0);
        status.setReflectedPower(4.0);
        const TxMeterReadings r = TxMeterPump::read(status, nullptr);
        QCOMPARE(r.forwardPowerWatts, 100.0);
        QCOMPARE(r.reflectedPowerWatts, 4.0);
        QCOMPARE(r.swr, status.swrRatio());
        // SWR from 100 W forward and 4 W reflected: (1 + 0.2) / (1 - 0.2).
        QVERIFY(qAbs(r.swr - 1.5) < 1e-9);
        // No transmit channel: no ALC or MIC reading.
        QCOMPARE(r.alcDb, TxMeterReadings::kNoReadingDb);
        QCOMPARE(r.micLevelDb, TxMeterReadings::kNoReadingDb);
    }

    void anIdleRadioReadsNoPowerAndSwrOne()
    {
        RadioStatus status;
        const TxMeterReadings r = TxMeterPump::read(status, nullptr);
        QCOMPARE(r.forwardPowerWatts, 0.0);
        QCOMPARE(r.reflectedPowerWatts, 0.0);
        QCOMPARE(r.swr, 1.0);
        QCOMPARE(r, TxMeterReadings{});
    }

    void theAlcAndMicReadingsComeFromTheLanesCacheWithoutWaiting()
    {
        // A transmit lane that never runs: a read that waited for it, or
        // that it had to answer, would never come back.
        DspControlThread lane(DspLane::Transmit);
        TxChannel tx(WdspEngine::kTxChannelId, 64, 64, &lane);
        RadioStatus status;

        QElapsedTimer took;
        took.start();
        const TxMeterReadings r = TxMeterPump::read(status, &tx);
        QVERIFY2(took.elapsed() < 1000, "the read waited for the lane");
        // The cache's idle value (-400, WDSP's meter.c idle) as Thetis shows
        // it: MIC max(-195, -400), ALC max(-30, -400).
        QCOMPARE(r.micLevelDb, -195.0);
        QCOMPARE(r.alcDb, -30.0);
    }

    void aPumpOnAModelReadsItsRadioStatus()
    {
        // readNow() with no source and no model reads the idle values.
        TxMeterPump pump(nullptr);
        QCOMPARE(pump.readNow(), TxMeterReadings{});
    }
};

QTEST_GUILESS_MAIN(TstTxMeterPump)
#include "tst_tx_meter_pump.moc"
