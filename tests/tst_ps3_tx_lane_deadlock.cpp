// no-port-check: NereusSDR-original regression for the observed PS3 lock cycle.
// A real doPSCorrChange calculation installs BEGIN/SWAP with TXA quiescent.
// The actual transmit lane must still read status, process Off, and start
// a subsequent nonzero stream. No radio, network, or audio device is opened.

#include <QtTest>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include "RealtimeTestLoad.h"

#include <array>
#include <atomic>
#include <cmath>
#include <numbers>
#include <limits>
#include <thread>
#include <vector>

#include "core/DspControlThread.h"
#include "core/PureSignal.h"
#include "core/MoxController.h"
#include "core/StepAttenuatorController.h"
#include "core/RadioConnection.h"
#include "core/TxChannel.h"
#include "core/TxAnalyzer.h"
#include "core/WdspEngine.h"
#include "core/wdsp_api.h"

using namespace NereusSDR;

#ifdef HAVE_WDSP
extern "C" {
void pscc(int channel, int size, double* tx, double* rx);
int nereus_ps_iqc_state(int channel);
void nereus_ps_release_parked_ramp(int channel);
int nereus_ps_stop_and_rearm(int channel);
int nereus_ps_native_quiescent(int channel);
int GetPSCalculationStateForTest(int channel, int* running, int* done,
                                int* inProgress, int* ownsCurves);
}

namespace {
constexpr int kChannel = WdspEngine::kTxChannelId;
constexpr int kInputFrames = 64;
constexpr int kRate = 48000;
constexpr int kProgressBoundMs = 2000;

class TxSink final : public RadioConnection {
public:
    void init() override {}
    void connectToRadio(const RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
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
    void sendTxIq(const float* iq, int frames) override
    {
        ++blocks;
        for (int i = 0; i < 2 * frames; ++i) {
            peak = std::max(peak, std::abs(iq[i]));
        }
    }
    int blocks{0};
    float peak{0.0f};
};

struct CalibrationSamples {
    std::vector<double> tx = std::vector<double>(8192);
    std::vector<double> rx = std::vector<double>(8192);

    CalibrationSamples()
    {
        // CALCC's real collection has 16 buckets of 256 samples
        // (calcc.c:438-449). Span each bucket, including [0.04, 0.0625),
        // with a smooth deterministic PA response, rather than repeated
        // points or injected fitted curves.
        for (int bucket = 0; bucket < 16; ++bucket) {
            const double bottom = std::max(0.04, static_cast<double>(bucket) / 16.0);
            const double top = static_cast<double>(bucket + 1) / 16.0;
            for (int sample = 0; sample < 256; ++sample) {
                const int index = 2 * (256 * bucket + sample);
                const double a = bottom + (top - bottom) * (sample + 0.5) / 256.0;
                const double gain = 0.6 * (1.0 - 0.08 * a * a);
                const double phase = 0.02 * a * a;
                tx[index] = a;
                rx[index] = a * gain * std::cos(phase);
                rx[index + 1] = a * gain * std::sin(phase);
            }
        }
    }
    void collect(int calls)
    {
        for (int i = 0; i < calls; ++i) {
            ::pscc(kChannel, 4096, tx.data(), rx.data());
        }
    }
};

bool correctionBusy()
{
    int run = 0;
    int busy = 0;
    return ::GetPSCorrectionState(kChannel, &run, &busy) != 0 && run && busy;
}

// Save is serviced by the same real correction worker after calculation.
// Its generation acknowledges worker completion without consuming LCALC's
// calcdone handshake or adding a production test hook.
bool nativeWorkerBarrier(const QString& path)
{
    ::PSFileOperationStatus before{};
    if (!::GetPSFileOperationStatus(kChannel, 0, &before)) return false;
    auto bytes = path.toLocal8Bit();
    ::PSSaveCorr(kChannel, bytes.data());
    return QTest::qWaitFor([&] {
        ::PSFileOperationStatus after{};
        return ::GetPSFileOperationStatus(kChannel, 0, &after)
            && !after.pending && after.generation == before.generation + 1
            && after.result == 0;
    }, kProgressBoundMs);
}

bool finishNativeRamp()
{
    ::SetChannelState(kChannel, 1, 0);
    std::array<double, 2 * kInputFrames> input{};
    std::array<double, 2 * kInputFrames> output{};
    const bool completed = QTest::qWaitFor([&] {
        int error = 0;
        for (int block = 0; block < 8; ++block)
            ::fexchange0(kChannel, input.data(), output.data(), &error);
        return error == 0 && !correctionBusy();
    }, kProgressBoundMs);
    // Normal unkey waits for down-slew and flush while the audio worker
    // keeps delivering blocks. dmode=0 would leave an old downflag that
    // clears exchange during the next key, invalidating this fixture.
    std::atomic<bool> drained{false};
    QTimer pump;
    pump.setTimerType(Qt::PreciseTimer);
    pump.setInterval(1);
    QObject::connect(&pump, &QTimer::timeout, &pump, [&] {
        int error = 0;
        ::fexchange0(kChannel, input.data(), output.data(), &error);
    });
    pump.start();
    std::thread drainer([&] {
        ::SetChannelState(kChannel, 0, 1);
        drained.store(true, std::memory_order_release);
    });
    const bool acknowledged = QTest::qWaitFor([&] {
        return drained.load(std::memory_order_acquire);
    }, kProgressBoundMs);
    pump.stop();
    // The native call has its own 100 ms bound, including no-sample cases.
    // Always join before either arrays or the WDSP channel can be destroyed.
    drainer.join();
    return completed && acknowledged && nereus_ps_native_quiescent(kChannel);
}
} // namespace
#endif

class TestPs3TxLaneDeadlock final : public QObject {
    Q_OBJECT
private slots:
    void resetReadbackDistinguishesQueuedAcceptanceFromNativeCompletion()
    {
#ifndef HAVE_WDSP
        QSKIP("requires real WDSP");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        WdspEngine engine;
        engine.setSynchronousInitForTest(true);
        QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
        TxAnalyzer analyzer;
        analyzer.setFftSize(4096);
        analyzer.setBlockSize(WdspEngine::kTxDspBufferSize);
        analyzer.start();
        analyzer.stop();
        TxChannel* tx = engine.createTxChannel(kChannel, kInputFrames,
            WdspEngine::kTxDspBufferSize, kRate,
            WdspEngine::kTxDspSampleRate, kRate);
        QVERIFY(tx);
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        std::atomic<bool> releaseLane{false};
        const auto cleanup = qScopeGuard([&] {
            releaseLane.store(true);
            nereus_ps_release_parked_ramp(kChannel);
            (void)lane.waitIdleForTest(kProgressBoundMs);
            engine.setTransmitLane(nullptr);
            engine.destroyTxChannel(kChannel);
            lane.stop();
        });
        tx->setPSHWPeak(1.0);
        tx->setPSMoxDelay(0.0);
        tx->setPSLoopDelay(0.0);
        tx->setPSTXDelay(0.0);
        tx->setPSRunCal(1);
        tx->setPSMox(true);
        tx->setPSControl(1, 0, 1, 0);
        CalibrationSamples samples;
        samples.collect(7);
        QVERIFY(QTest::qWaitFor(correctionBusy, kProgressBoundMs));
        engine.setTransmitLane(&lane);
        int info[16] = {};
        std::uint64_t observedSerial = 0;
        (void)tx->getPSInfo(info, &observedSerial);
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        QVERIFY(tx->getPSInfo(info, &observedSerial));
        QCOMPARE(info[15], 6);
        const std::uint64_t beforeReset = observedSerial;
        std::atomic<bool> laneEntered{false};
        lane.postBarrier([&] {
            laneEntered.store(true);
            while (!releaseLane.load()) {
                std::this_thread::yield();
            }
        });
        QVERIFY(QTest::qWaitFor([&] { return laneEntered.load(); }, kProgressBoundMs));
        const std::uint64_t resetSerial = tx->setPSControl(1, 0, 0, 0);
        QVERIFY(resetSerial > beforeReset);
        QVERIFY(tx->getPSInfo(info, &observedSerial));
        QCOMPARE(observedSerial, beforeReset); // Request is still queued.
        releaseLane.store(true);
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        QVERIFY(tx->getPSInfo(info, &observedSerial));
        QVERIFY(observedSerial >= resetSerial);
        QCOMPARE(info[15], 6); // Applied reset request has not drained LCALC.
        samples.collect(20);
        QVERIFY(tx->getPSInfo(info, &observedSerial));
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        QVERIFY(tx->getPSInfo(info, &observedSerial));
        QCOMPARE(info[15], 6);
        QVERIFY(finishNativeRamp());
        const QString savedCorrection = directory.path() + QStringLiteral("/reset-drain.psc");
        QVERIFY(nativeWorkerBarrier(savedCorrection));
        QVERIFY(QTest::qWaitFor([&] {
            samples.collect(1);
            int nativeInfo[16] = {};
            ::GetPSInfo(kChannel, nativeInfo);
            return nativeInfo[15] == 0;
        }, kProgressBoundMs));
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        (void)tx->getPSInfo(info, &observedSerial);
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        QVERIFY(tx->getPSInfo(info, &observedSerial));
        QVERIFY(observedSerial >= resetSerial);
        QCOMPARE(info[15], 0);
        QCOMPARE(info[7], 1); // The drained collection is already published.

        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal coordinator(&engine, tx, nullptr, &mox, &stepAtt, nullptr);
        coordinator.setTimersEnabled(false);
        coordinator.setAutoAttenuate(true);
        coordinator.setAutoCalEnabled(true);
        mox.setMox(true);
        stepAtt.setAttOnTxValue(31);
        int hostInfo[16] = {};
        coordinator.processNewInfo(hostInfo);
        coordinator.processNewInfo(hostInfo);
        hostInfo[4] = 29;
        hostInfo[5] = 1;
        hostInfo[7] = 1;
        hostInfo[15] = 6;
        coordinator.processNewInfo(hostInfo);
        coordinator.autoAttentionTick();
        QVERIFY(!coordinator.beginRestoreCorrections(savedCorrection));
        hostInfo[15] = 0;
        coordinator.processNewInfo(hostInfo, std::numeric_limits<std::uint64_t>::max());
        coordinator.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 17);
        QVERIFY(!coordinator.beginRestoreCorrections(savedCorrection));
        QSignalSpy hardwareFlipped(&mox, &MoxController::hardwareFlipped);
        mox.setMox(false);
        QVERIFY(QTest::qWaitFor([&] {
            return !hardwareFlipped.isEmpty()
                && !hardwareFlipped.last().first().toBool();
        }, kProgressBoundMs));
        QCOMPARE(coordinator.autoAttenuateState(),
                 PureSignal::AutoAttenuateState::Monitor);
        QVERIFY(!coordinator.beginRestoreCorrections(savedCorrection));
        mox.setMox(true);
        coordinator.processNewInfo(hostInfo, std::numeric_limits<std::uint64_t>::max());
        QVERIFY(coordinator.beginRestoreCorrections(savedCorrection));
        coordinator.reset();
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        coordinator.setTxChannel(nullptr);
#endif
    }

    void correctionSummaryInvalidationRejectsRetainedAndQueuedCurves()
    {
#ifndef HAVE_WDSP
        QSKIP("requires real WDSP");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        WdspEngine engine;
        engine.setSynchronousInitForTest(true);
        QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
        TxAnalyzer analyzer;
        analyzer.setFftSize(4096);
        analyzer.setBlockSize(WdspEngine::kTxDspBufferSize);
        analyzer.start();
        analyzer.stop();
        TxChannel* tx = engine.createTxChannel(kChannel, kInputFrames,
            WdspEngine::kTxDspBufferSize, kRate,
            WdspEngine::kTxDspSampleRate, kRate);
        QVERIFY(tx);
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        std::atomic<bool> releaseLane{false};
        const auto cleanup = qScopeGuard([&] {
            releaseLane.store(true, std::memory_order_release);
            nereus_ps_release_parked_ramp(kChannel);
            (void)lane.waitIdleForTest(kProgressBoundMs);
            engine.setTransmitLane(nullptr);
            engine.destroyTxChannel(kChannel);
            lane.stop();
        });
        tx->setPSHWPeak(1.0);
        tx->setPSMoxDelay(0.0);
        tx->setPSLoopDelay(0.0);
        tx->setPSTXDelay(0.0);
        tx->setPSRunCal(1);
        tx->setPSMox(true);
        tx->setPSControl(1, 0, 1, 0);
        CalibrationSamples samples;
        samples.collect(7);
        QVERIFY(QTest::qWaitFor(correctionBusy, kProgressBoundMs));
        QVERIFY(finishNativeRamp());
        int info[16] = {};
        QVERIFY(QTest::qWaitFor([&] {
            samples.collect(1);
            ::GetPSInfo(kChannel, info);
            return info[5] == 1;
        }, kProgressBoundMs));

        // The real vendor display retains completed calibration curves.
        // Availability alone must not make them valid in a new session.
        QVERIFY(tx->getPs3DisplaySnapshot(77, 19, 100));
        QVERIFY(!tx->psCorrectionSummary());
        tx->markPsCorrectionSummaryCalibrationValid();
        QVERIFY(tx->psCorrectionSummary());
        tx->invalidatePsCorrectionSummary();
        QVERIFY(tx->getPs3DisplaySnapshot(77, 19, 100));
        QVERIFY(!tx->psCorrectionSummary());
        tx->markPsCorrectionSummaryCalibrationValid();
        tx->setPSReset(true);
        QVERIFY(!tx->psCorrectionSummary());
        tx->setPSReset(false);
        tx->markPsCorrectionSummaryCalibrationValid();
        tx->setPSControl(1, 0, 1, 0);
        QVERIFY(!tx->psCorrectionSummary());

        engine.setTransmitLane(&lane);
        tx->markPsCorrectionSummaryCalibrationValid();
        (void)tx->getPs3DisplaySnapshot(77, 19, 100);
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        std::atomic<bool> laneEntered{false};
        lane.postBarrier([&] {
            laneEntered.store(true, std::memory_order_release);
            while (!releaseLane.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        });
        QVERIFY(QTest::qWaitFor([&] { return laneEntered.load(std::memory_order_acquire); },
                               kProgressBoundMs));
        (void)tx->psCorrectionSummary(); // queued capture from the old epoch
        tx->invalidatePsCorrectionSummary();
        tx->markPsCorrectionSummaryCalibrationValid();
        releaseLane.store(true, std::memory_order_release);
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        QVERIFY(!tx->psCorrectionSummary()); // stale queued capture was discarded
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        QVERIFY(tx->psCorrectionSummary());
        const auto display = tx->getPs3DisplaySnapshot(77, 20, 101);
        QVERIFY(display);
        QCOMPARE(display->sequence, std::uint64_t{19});
        QCOMPARE(display->capturedAtUnixMilliseconds, std::int64_t{100});
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        const QString saved = directory.filePath("summary.ps3");
        QVERIFY(nativeWorkerBarrier(saved));
        (void)tx->psFileOperationStatus(Ps3FileOperationKind::Save);
        QVERIFY(lane.waitIdleForTest(kProgressBoundMs));
        QVERIFY(tx->psRestoreCorr(saved));
        QVERIFY(!tx->psCorrectionSummary()); // accepted restore invalidates at once
#endif
    }

    void cleanup()
    {
        RealtimeTestLoad::printLoadAverageIfFailed();
    }

    void offAndNextKeyProgressWhileCalibrationRampHasNoAudio_data()
    {
        QTest::addColumn<bool>("swap");
        QTest::newRow("BEGIN") << false;
        QTest::newRow("SWAP") << true;
    }

    void offAndNextKeyProgressWhileCalibrationRampHasNoAudio()
    {
#ifndef HAVE_WDSP
        QSKIP("requires real WDSP");
#else
        QFETCH(bool, swap);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        WdspEngine engine;
        engine.setSynchronousInitForTest(true);
        QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
        // WdspEngine arms TXA's analyzer siphon for display 5. Its actual
        // analyzer must exist before any native TX block is pumped.
        TxAnalyzer analyzer;
        analyzer.setFftSize(4096);
        analyzer.setBlockSize(WdspEngine::kTxDspBufferSize);
        analyzer.start();
        analyzer.stop();
        TxChannel* tx = engine.createTxChannel(kChannel, kInputFrames,
            WdspEngine::kTxDspBufferSize, kRate,
            WdspEngine::kTxDspSampleRate, kRate);
        QVERIFY(tx);
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        TxSink sink;
        tx->setConnection(&sink);
        PureSignal coordinator(&engine, tx, nullptr, nullptr, nullptr, nullptr);
        coordinator.setTimersEnabled(false);
        const auto cleanup = qScopeGuard([&] {
            // This works even on the broken revision: the IQC lock is not
            // the blocked CALCC lock. Never strand the test's lane on RED.
            nereus_ps_release_parked_ramp(kChannel);
            lane.waitIdleForTest(kProgressBoundMs);
            coordinator.setTxChannel(nullptr);
            engine.setTransmitLane(nullptr);
            tx->setConnection(nullptr);
            engine.destroyTxChannel(kChannel);
            lane.stop();
        });

        tx->setPSHWPeak(1.0);
        tx->setPSMoxDelay(0.0);
        tx->setPSLoopDelay(0.0);
        tx->setPSTXDelay(0.0);
        tx->setPSRunCal(1);
        tx->setPSMox(true);
        tx->setPSControl(1, 0, 1, 0);
        CalibrationSamples samples;
        // LRESET -> LWAIT -> LMOXDELAY -> LSETUP -> LCOLLECT ->
        // MOXCHECK -> LCALC; the last call starts the real calc worker.
        samples.collect(7);
        QVERIFY2(QTest::qWaitFor(correctionBusy, kProgressBoundMs),
                 "real calibration never installed its BEGIN ramp");
        QCOMPARE(nereus_ps_iqc_state(kChannel), 1); // native BEGIN
        if (swap) {
            QVERIFY2(finishNativeRamp(), "the initial BEGIN ramp did not finish");
            // The ramp can finish before the worker publishes calcdone.
            // Consume LCALC only once that publication is visible, then
            // LDELAY -> LSETUP -> LCOLLECT -> MOXCHECK -> LCALC.
            int info[16] = {};
            QVERIFY2(QTest::qWaitFor([&] {
                samples.collect(1);
                ::GetPSInfo(kChannel, info);
                return info[5] == 1 && info[15] != 6;
            }, kProgressBoundMs), "first calibration did not complete");
            samples.collect(4);
            QVERIFY2(QTest::qWaitFor(correctionBusy, kProgressBoundMs),
                     "real calibration never installed its SWAP ramp");
            QCOMPARE(nereus_ps_iqc_state(kChannel), 2); // native SWAP
        }

        QVERIFY(!tx->isRunning());
        engine.setTransmitLane(&lane);
        int status[16] = {};
        tx->getPSInfo(status); // posts the exact refresh seen in the live stack
        const bool statusProgressed = lane.waitIdleForTest(kProgressBoundMs);
        coordinator.reset();
        const bool offProgressed = lane.waitIdleForTest(kProgressBoundMs);
        // Capture progress before independent cleanup, so RED is a causal
        // assertion failure, not a hung destructor or a fixture failure.
        if (!statusProgressed || !offProgressed) {
            nereus_ps_release_parked_ramp(kChannel);
            QVERIFY2(lane.waitIdleForTest(kProgressBoundMs), "RED cleanup could not release the lane");
        }
        QVERIFY2(statusProgressed,
                 "CALCC holds its update lock while IQC awaits audio: TX status lane cannot progress");
        QVERIFY2(offProgressed, "Off was stranded behind the CALCC/IQC ramp wait");
        const auto correction = tx->psCorrectionState();
        QVERIFY(correction && !correction->run && !correction->busy);

        tx->setRunningAsync(true);
        QVERIFY2(lane.waitIdleForTest(kProgressBoundMs), "the next key never started TXA");
        QVERIFY(tx->isRunning());
        std::array<double, 2 * kInputFrames> audio{};
        int sample = 0;
        QVERIFY2(QTest::qWaitFor([&] {
            for (int block = 0; block < 8; ++block) {
                for (int i = 0; i < kInputFrames; ++i, ++sample) {
                    audio[2 * i] = 0.1 * std::sin(2.0 * std::numbers::pi * 1000.0 * sample / kRate);
                }
                tx->driveOneTxBlockFromInterleaved(audio.data());
            }
            return sink.blocks > 0 && sink.peak > 0.0001f;
        }, kProgressBoundMs), "the next key produced no nonzero DSP I/Q block");
        tx->closeRfGate();
#endif
    }
    void cancellationCannotCompleteOldEpoch_data()
    {
        QTest::addColumn<bool>("completedBeforeOff");
        QTest::newRow("parked-ramp-Off-rearm") << false;
        QTest::newRow("done-before-LCALC-Off-rearm") << true;
    }

    void cancellationCannotCompleteOldEpoch()
    {
#ifndef HAVE_WDSP
        QSKIP("requires real WDSP");
#else
        QFETCH(bool, completedBeforeOff);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        WdspEngine engine;
        engine.setSynchronousInitForTest(true);
        QVERIFY(engine.initialize(directory.path() + QLatin1Char('/')));
        TxAnalyzer analyzer;
        analyzer.setFftSize(4096);
        analyzer.setBlockSize(WdspEngine::kTxDspBufferSize);
        analyzer.start();
        analyzer.stop();
        TxChannel* tx = engine.createTxChannel(kChannel, kInputFrames,
            WdspEngine::kTxDspBufferSize, kRate,
            WdspEngine::kTxDspSampleRate, kRate);
        QVERIFY(tx);
        const auto cleanup = qScopeGuard([&] {
            if (tx) {
                nereus_ps_release_parked_ramp(kChannel);
                engine.destroyTxChannel(kChannel);
            }
        });
        tx->setPSHWPeak(1.0);
        tx->setPSMoxDelay(0.0);
        tx->setPSLoopDelay(0.0);
        tx->setPSTXDelay(0.0);
        tx->setPSRunCal(1);
        tx->setPSMox(true);
        tx->setPSControl(1, 0, 1, 0);
        CalibrationSamples samples;
        samples.collect(7);
        QVERIFY2(QTest::qWaitFor(correctionBusy, kProgressBoundMs),
                 "initial real calculation did not install BEGIN");
        QCOMPARE(nereus_ps_iqc_state(kChannel), 1);
        for (int cycle = 0; cycle < 3; ++cycle) {
            int running = 0, done = 0, inProgress = 0, ownsCurves = 1;
            QVERIFY(::GetPSCalculationStateForTest(kChannel, &running, &done,
                                                  &inProgress, &ownsCurves));
            QCOMPARE(running, 0);
            QCOMPARE(done, 0);
            QCOMPARE(inProgress, 1);
            QCOMPARE(ownsCurves, 0); // accepted installation transferred all curves
            if (completedBeforeOff) {
                QVERIFY(finishNativeRamp());
                // Worker finished, but no pscc has consumed calcdone.
                QVERIFY2(nativeWorkerBarrier(directory.filePath("before-off.ps3")),
                         "worker did not publish completion before Off");
                QVERIFY(::GetPSCalculationStateForTest(kChannel, &running, &done,
                                                      &inProgress, &ownsCurves));
                QCOMPARE(running, 1);
                QCOMPARE(done, 1);
                QCOMPARE(inProgress, 1);
                QCOMPARE(ownsCurves, 0);
            }
            QVERIFY(nereus_ps_stop_and_rearm(kChannel));
            QVERIFY2(nativeWorkerBarrier(directory.filePath("after-off.ps3")),
                     "cancelled calculation never retired after immediate rearm");
            // Assert before pscc's LRESET can hide stale worker publication.
            QVERIFY(::GetPSCalculationStateForTest(kChannel, &running, &done,
                                                  &inProgress, &ownsCurves));
            QCOMPARE(running, 0);
            QCOMPARE(done, 0);
            QCOMPARE(inProgress, 0);
            QCOMPARE(ownsCurves, 0);
            int run = 1, busy = 1;
            QVERIFY(::GetPSCorrectionState(kChannel, &run, &busy));
            QCOMPARE(run, 0);
            QCOMPARE(busy, 0);
            // A fresh epoch must calculate and install BEGIN. A stale done
            // consumed by LCALC skips this calculation; a stale running bit
            // triggers END/SWAP instead, potentially parking the worker again.
            samples.collect(7);
            QVERIFY2(QTest::qWaitFor(correctionBusy, kProgressBoundMs),
                     "old completion prevented the next real calculation");
            QCOMPARE(nereus_ps_iqc_state(kChannel), 1);
        }
        // Accepted curves belong exclusively to IQC even though the calc
        // worker is still awaiting BEGIN with TXA quiescent. Destruction must
        // cancel/join it and free each retained curve exactly once.
        engine.destroyTxChannel(kChannel);
        tx = nullptr;
#endif
    }

};

QTEST_GUILESS_MAIN(TestPs3TxLaneDeadlock)
#include "tst_ps3_tx_lane_deadlock.moc"
