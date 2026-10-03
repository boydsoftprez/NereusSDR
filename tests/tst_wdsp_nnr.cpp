// no-port-check: NereusSDR-original linked-WDSP integration tests. Synthetic
// samples prove processing/lifecycle contracts, not listening or RF quality.
#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <thread>

#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/NnrAdapter.h"
#include "core/wdsp_api.h"
#include "nnr_compat.h"

extern "C" {
extern const unsigned char nnr_model_1_data[];
extern const unsigned int nnr_model_1_size;
}

using namespace NereusSDR;

class TestWdspNnr : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        m_engine = std::make_unique<WdspEngine>();
        QString reason;
        QVERIFY(!m_engine->setNnrModelPaths({QString(512, 'a'), QString{}}, &reason));
        m_engine->setSynchronousInitForTest(true);
        QVERIFY(m_engine->initialize(m_directory.path() + '/'));
        m_a = m_engine->createRxChannel(0, 256, 1024, 48000, 48000, 48000);
        m_b = m_engine->createRxChannel(2, 256, 1024, 48000, 48000, 48000);
        QVERIFY(m_a);
        QVERIFY(m_b);
    }

    void completeTuningReachesBothRealModelsWhileDisabled()
    {
        const auto state = m_a->nnrDiagnostics();
        QVERIFY(state.available);
        QVERIFY(state.ready);
        QVERIFY(state.modelAvailable[0]);
        QVERIFY(state.modelAvailable[1]);
        QCOMPARE(state.modelSources[0], NnrModelSource::Bundled);
        QCOMPARE(state.modelSources[1], NnrModelSource::Bundled);
        QVERIFY(!state.running);
        QVERIFY(state.delaySamples > 0);

        NnrSettings wanted{1, -31.25, NrPosition::PreAgc, 1.75, 12.5, 2.75, 13.5, 17.25, 83.5};
        QVERIFY(m_a->setNnrTuning(wanted));
        QVERIFY(m_a->nnrTuning() == wanted);
        QCOMPARE(m_a->nnrDiagnostics().actualModelSlot, 1);
        QVERIFY(!m_a->nnrDiagnostics().running);
        QVERIFY(m_b->nnrTuning() == NnrSettings{});

        wanted.modelSlot = 0;
        QVERIFY(m_a->setNnrTuning(wanted));
        QVERIFY(m_a->nnrTuning() == wanted);
        QVERIFY(m_a->setActiveNr(NrSlot::NNR));
        QVERIFY(m_a->nnrDiagnostics().running);
        QVERIFY(m_a->setActiveNr(NrSlot::NR2));
        QVERIFY(!m_a->nnrDiagnostics().running);
        QVERIFY(m_a->emnrEnabled());
    }

    void invalidModelAndTuningLeavePreviousStateIntact()
    {
        const auto before = m_a->nnrTuning();
        auto invalid = before;
        invalid.alpha = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!m_a->setNnrTuning(invalid));
        invalid = before;
        invalid.modelSlot = 7;
        QVERIFY(!m_a->setNnrTuning(invalid));
        QVERIFY(m_a->nnrTuning() == before);
        QCOMPARE(m_a->activeNr(), NrSlot::NR2);
    }

    void internalSizeAndRateRebuildRetainTuningAndDiagnosticSession()
    {
        QVERIFY(m_a->setActiveNr(NrSlot::NNR));
        QVERIFY(m_a->setNnrDiagnostics(1, 0));
        const auto before = m_a->nnrTuning();
        SetDSPBuffsize(0, 2048);
        QVERIFY(m_a->nnrTuning() == before);
        QCOMPARE(m_a->nnrDiagnostics().testMode, 1);
        QCOMPARE(m_a->nnrDiagnostics().outputMode, 0);
        QVERIFY(m_a->nnrDiagnostics().running);

        // 24 kHz is a legal 2:1 WDSP rate relative to 48 kHz I/O, but is
        // not an integer multiple of this NNR network's 16 kHz rate.
        SetDSPSamplerate(0, 24000);
        QVERIFY(!m_a->nnrDiagnostics().rateSupported);
        QVERIFY(!m_a->nnrDiagnostics().running);
        QVERIFY(m_a->nnrTuning() == before);
        SetDSPSamplerate(0, 48000);
        QVERIFY(m_a->nnrDiagnostics().rateSupported);
        QVERIFY(m_a->nnrDiagnostics().running);
        QVERIFY(m_a->nnrTuning() == before);
        QVERIFY(m_a->setNnrDiagnostics(0, 1));
    }

    void realNetworkProducesFiniteAudioForBothModels()
    {
        std::array<float, 256> inI{}, inQ{}, outI{}, outQ{};
        for (int model = 0; model < 2; ++model) {
            auto settings = m_b->nnrTuning();
            settings.modelSlot = model;
            QVERIFY(m_b->setNnrTuning(settings));
            QVERIFY(m_b->setActiveNr(NrSlot::NNR));
            m_b->setActive(true);
            double maximum = 0.0;
            for (int block = 0; block < 64; ++block) {
                for (int i = 0; i < 256; ++i) {
                    const double phase = 2.0 * std::numbers::pi * 1000.0 * (block * 256 + i) / 48000.0;
                    inI[i] = static_cast<float>(0.01 * std::cos(phase));
                    inQ[i] = static_cast<float>(-0.01 * std::sin(phase));
                }
                m_b->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), 256, 256);
                for (int i = 0; i < 256; ++i) {
                    QVERIFY(std::isfinite(outI[i]));
                    QVERIFY(std::isfinite(outQ[i]));
                    maximum = std::max(maximum, std::abs(static_cast<double>(outI[i])));
                }
            }
            QVERIFY(maximum > 1e-8);
            m_b->setActive(false);
        }
    }

    void recreatedChannelRetainsNormalSettingsAndResetsDiagnostics()
    {
        QVERIFY(m_a->setNnrDiagnostics(2, 0));
        const auto saved = m_a->nnrTuning();
        ChannelConfig config;
        config.sampleRate = 96000;
        config.bufferSize = 256;
        config.filterSize = 2048;
        QVERIFY(m_engine->rebuildRxChannel(0, config) >= 0);
        m_a = m_engine->rxChannel(0);
        QVERIFY(m_a);
        QVERIFY(m_a->nnrTuning() == saved);
        QCOMPARE(m_a->activeNr(), NrSlot::NNR);
        QVERIFY(m_a->nnrDiagnostics().running);
        QCOMPARE(m_a->nnrDiagnostics().testMode, 0);
        QCOMPARE(m_a->nnrDiagnostics().outputMode, 1);
    }

    void partiallyLoadedModelCannotBeSelectedOrDereferenced()
    {
        // Keep the real file grammar and tensors but remove one required
        // first-layer tensor name. The native loader reaches its partial
        // model path; user imports are rejected earlier by host preflight.
        QByteArray broken(reinterpret_cast<const char*>(nnr_model_1_data), nnr_model_1_size);
        const auto count = qFromLittleEndian<quint32>(broken.constData() + 12);
        bool changed = false;
        for (quint32 i = 0; i < count; ++i) {
            const int offset = 32 + static_cast<int>(i) * 72;
            if (broken.mid(offset, 6) == "enc1_w") {
                broken[offset] = 'x';
                changed = true;
                break;
            }
        }
        QVERIFY(changed);
        const QString path = m_directory.filePath("missing-required-tensor.bin");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(broken), broken.size());
        file.close();
        const auto encoded = QFile::encodeName(path);
        SetNNRModelPathSlot(1, encoded.constData());
        auto* partial = m_engine->createRxChannel(4, 256, 1024, 48000, 48000, 48000);
        SetNNRModelPathSlot(1, "");
        QVERIFY(partial);
        QVERIFY(partial->nnrDiagnostics().modelAvailable[0]);
        QVERIFY(!partial->nnrDiagnostics().modelAvailable[1]);
        QVERIFY(partial->setActiveNr(NrSlot::NR2));
        const auto before = partial->nnrTuning();
        auto requested = before;
        requested.modelSlot = 1;
        requested.alpha = 2.5;
        QVERIFY(!partial->setNnrTuning(requested));
        QVERIFY(partial->nnrTuning() == before);
        QCOMPARE(partial->activeNr(), NrSlot::NR2);
        requested.modelSlot = 0;
        QVERIFY(partial->setNnrTuning(requested));
        QCOMPARE(partial->nnrTuning().alpha, 2.5);
        m_engine->destroyRxChannel(4);
    }

    // R-R3-40: a runtime limit is requested without the channel's DSP lock,
    // so the request returns while the worker is inside a long block; the
    // worker applies it at a later block. The accepted configuration stays
    // the caller's choice throughout, and clearing the limit restores it.
    void limitRequestReturnsAtOnceAndTheWorkerAppliesIt()
    {
        using Clock = std::chrono::steady_clock;
        // Outside WdspEngine's reserved ids (slices 0-4, TX 5, PS feedback 6).
        constexpr int kChannel = 12;
        constexpr int kInSize = 1024;
        constexpr int kDspSize = 4096;
        constexpr int kRate = 48000;
        // Busy-wait inside every worker block (block period 85.3 ms), so the
        // worker holds the DSP lock most of the time.
        constexpr int kBlockDelayUs = 60000;
        // A lock-free store. Anything near a block's length would mean the
        // call waited for the lock; a quarter block leaves room for a busy
        // machine descheduling the calling thread.
        constexpr double kMaxRequestMs = kBlockDelayUs / 1000.0 / 4.0;

        OpenChannel(kChannel, kInSize, kDspSize, kRate, kRate, kRate,
                    0, 1, 0.010, 0.025, 0.000, 0.010,
                    0);   // bfo off: the feeder never waits for output
        const auto closeChannel = qScopeGuard([&] { CloseChannel(kChannel); });
        NNRConfiguration premium{1, 0, -25.0, 1.0, 10.0, 2.0, 12.0, 0.0, 0.0};
        NNRRuntimeStatus status{};
        QVERIFY(ConfigureRXANNR(kChannel, &premium, &status));
        SetRXANNRRun(kChannel, 1);
        QVERIFY(GetRXANNRStatus(kChannel, &status));
        QVERIFY(status.running);
        QCOMPARE(status.active_model_slot, 1);
        QCOMPARE(status.limit, 0);

        std::atomic<bool> stop{false};
        std::thread feeder([&] {
            std::array<float, kInSize> inI{}, inQ{}, outI{}, outQ{};
            const auto period = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(double(kInSize) / kRate / 2.0));
            auto next = Clock::now();
            double phase = 0.0;
            while (!stop.load()) {
                for (int i = 0; i < kInSize; ++i) {
                    inI[i] = static_cast<float>(0.01 * std::cos(phase));
                    inQ[i] = static_cast<float>(0.01 * std::sin(phase));
                    phase = std::fmod(phase + 2.0 * std::numbers::pi * 1000.0 / kRate,
                                      2.0 * std::numbers::pi);
                }
                int error = 0;
                fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
                next += period;
                std::this_thread::sleep_until(next);
            }
        });
        const auto stopFeeder = qScopeGuard([&] {
            WDSPSetTestBlockDelayUs(kChannel, 0);
            stop.store(true);
            feeder.join();
            // Let the worker drain its backlog before the channel closes.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        });
        WDSPSetTestBlockDelayUs(kChannel, kBlockDelayUs);

        const auto blocks = [&] {
            WdspChannelLoad load{};
            GetChannelDspLoad(kChannel, &load);
            return load;
        };
        // Waits, without the DSP lock, until the worker finished two more
        // blocks: the next one to start runs xnnr with the request pending.
        const auto waitTwoBlocks = [&] {
            const long long start = blocks().blocks;
            const auto deadline = Clock::now() + std::chrono::seconds(5);
            while (blocks().blocks < start + 2 && Clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            return blocks().blocks >= start + 2;
        };
        // Requests while the worker is inside a block, and proves it stayed
        // inside that same block for the whole call.
        const auto requestInsideABlock = [&](int limit, double* elapsedMs) {
            const auto deadline = Clock::now() + std::chrono::seconds(5);
            while (Clock::now() < deadline) {
                const WdspChannelLoad before = blocks();
                if (before.currentBlockNs == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(200));
                    continue;
                }
                const auto t0 = Clock::now();
                RequestRXANNRLimit(kChannel, limit);
                *elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
                const WdspChannelLoad after = blocks();
                if (after.blocks == before.blocks && after.currentBlockNs != 0)
                    return true;
                // The block ended during the call; the request stands, so
                // prove it with the next block instead.
            }
            return false;
        };

        QVERIFY(waitTwoBlocks());
        double elapsedMs = 0.0;
        QVERIFY2(requestInsideABlock(1, &elapsedMs), "no request landed inside a worker block");
        qInfo("standard-only request returned in %.1f us while the worker was inside a "
              "%d ms block", elapsedMs * 1000.0, kBlockDelayUs / 1000);
        QVERIFY2(elapsedMs < kMaxRequestMs, "the limit request waited for the DSP lock");
        QVERIFY(waitTwoBlocks());
        QVERIFY(GetRXANNRStatus(kChannel, &status));
        QCOMPARE(status.limit, 1);
        QCOMPARE(status.active_model_slot, 0);
        QCOMPARE(status.configuration.model_slot, 1);   // the choice is kept
        QVERIFY(status.running);

        // A later configuration of Premium is clamped by the limit too.
        QVERIFY(ConfigureRXANNR(kChannel, &premium, &status));
        QCOMPARE(status.configuration.model_slot, 1);
        QCOMPARE(status.active_model_slot, 0);

        QVERIFY(requestInsideABlock(2, &elapsedMs));
        QVERIFY2(elapsedMs < kMaxRequestMs, "the limit request waited for the DSP lock");
        QVERIFY(waitTwoBlocks());
        QVERIFY(GetRXANNRStatus(kChannel, &status));
        QCOMPARE(status.limit, 2);
        QVERIFY(!status.running);
        QVERIFY(status.requested_run);
        QCOMPARE(status.configuration.model_slot, 1);

        QVERIFY(requestInsideABlock(0, &elapsedMs));
        QVERIFY(waitTwoBlocks());
        QVERIFY(GetRXANNRStatus(kChannel, &status));
        QCOMPARE(status.limit, 0);
        QVERIFY(status.running);
        QCOMPARE(status.active_model_slot, 1);
    }

    // R-R3-40: the worker's "off" step (applied in xnnr, with the channel
    // found through the owner the request recorded) updates the receive
    // bandpass as the locked NNR setters do. NNR running raises that
    // bandpass's gain to 2 (RXAbp1Check); a step that left it there would
    // play about 6 dB louder than NNR switched off by the operator. The
    // level is read on the AGC meter, which follows the bandpass; the AGC
    // runs at a fixed gain so it cannot even the difference out.
    void workerOffStepUpdatesTheBandpassLikeTheOperatorsOff()
    {
        using Clock = std::chrono::steady_clock;
        constexpr int kChannel = 13;   // outside WdspEngine's reserved ids
        constexpr int kInSize = 1024;
        constexpr int kDspSize = 4096;
        constexpr int kRate = 48000;
        constexpr int kAgcAverageMeter = 6;   // RXA_AGC_AV (RXA.h rxaMeterType)
        // Blocks for the meter's average to settle after a change.
        constexpr int kSettleBlocks = 16;

        OpenChannel(kChannel, kInSize, kDspSize, kRate, kRate, kRate,
                    0, 1, 0.010, 0.025, 0.000, 0.010,
                    0);   // bfo off: the feeder never waits for output
        const auto closeChannel = qScopeGuard([&] { CloseChannel(kChannel); });
        NNRConfiguration standard{0, 0, -25.0, 1.0, 10.0, 2.0, 12.0, 0.0, 0.0};
        NNRRuntimeStatus status{};
        QVERIFY(ConfigureRXANNR(kChannel, &standard, &status));
        SetRXAAGCMode(kChannel, 0);   // fixed gain: the AGC would even out the bandpass gain
        SetRXANNRRun(kChannel, 1);

        std::atomic<bool> stop{false};
        std::thread feeder([&] {
            std::array<float, kInSize> inI{}, inQ{}, outI{}, outQ{};
            const auto period = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(double(kInSize) / kRate / 2.0));
            auto next = Clock::now();
            double phase = 0.0;
            while (!stop.load()) {
                for (int i = 0; i < kInSize; ++i) {
                    inI[i] = static_cast<float>(0.01 * std::cos(phase));
                    inQ[i] = static_cast<float>(0.01 * std::sin(phase));
                    phase = std::fmod(phase + 2.0 * std::numbers::pi * 1000.0 / kRate,
                                      2.0 * std::numbers::pi);
                }
                int error = 0;
                fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
                next += period;
                std::this_thread::sleep_until(next);
            }
        });
        const auto stopFeeder = qScopeGuard([&] {
            stop.store(true);
            feeder.join();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        });
        // Waits, without the DSP lock, for the worker to finish more blocks.
        const auto waitBlocks = [&](int count) {
            WdspChannelLoad load{};
            GetChannelDspLoad(kChannel, &load);
            const long long target = load.blocks + count;
            const auto deadline = Clock::now() + std::chrono::seconds(10);
            while (load.blocks < target && Clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                GetChannelDspLoad(kChannel, &load);
            }
            return load.blocks >= target;
        };

        QVERIFY(waitBlocks(kSettleBlocks));
        const double running = GetRXAMeter(kChannel, kAgcAverageMeter);

        RequestRXANNRLimit(kChannel, 2);   // no locked NNR call until it applies
        QVERIFY(waitBlocks(kSettleBlocks));
        QVERIFY(GetRXANNRStatus(kChannel, &status));   // reads, never applies
        QCOMPARE(status.limit, 2);
        QVERIFY(!status.running);
        const double workerOff = GetRXAMeter(kChannel, kAgcAverageMeter);

        RequestRXANNRLimit(kChannel, 0);
        QVERIFY(waitBlocks(kSettleBlocks));
        QVERIFY(GetRXANNRStatus(kChannel, &status));
        QCOMPARE(status.limit, 0);
        QVERIFY(status.running);
        SetRXANNRRun(kChannel, 0);         // the operator's off, locked
        QVERIFY(waitBlocks(kSettleBlocks));
        const double operatorOff = GetRXAMeter(kChannel, kAgcAverageMeter);

        qInfo("AGC meter: NNR running %.2f dB, worker off step %.2f dB, operator off %.2f dB",
              running, workerOff, operatorOff);
        QVERIFY2(std::abs(workerOff - operatorOff) < 1.5,
                 "the worker's off step left the NNR bandpass gain in place");
    }

    // R-R3-40: RxChannel carries the limit to WDSP and across a rebuild, and
    // an "off" limit accepts NNR as selected while holding it off.
    void rxChannelLimitClampsAndSurvivesARebuild()
    {
        auto settings = m_a->nnrTuning();
        settings.modelSlot = 1;
        QVERIFY(m_a->setNnrTuning(settings));
        QVERIFY(m_a->setActiveNr(NrSlot::NNR));
        QVERIFY(!m_a->requestNnrLimit(3));
        QVERIFY(m_a->requestNnrLimit(1));
        QCOMPARE(m_a->nnrLimit(), 1);
        // The locked tuning call applies the pending limit before it reads.
        QVERIFY(m_a->setNnrTuning(settings));
        QCOMPARE(m_a->nnrTuning().modelSlot, 1);
        QCOMPARE(m_a->nnrDiagnostics().actualModelSlot, 0);
        QCOMPARE(m_a->nnrDiagnostics().appliedLimit, 1);

        ChannelConfig config;
        config.sampleRate = 48000;
        config.bufferSize = 256;
        config.filterSize = 2048;
        QVERIFY(m_engine->rebuildRxChannel(0, config) >= 0);
        m_a = m_engine->rxChannel(0);
        QVERIFY(m_a);
        QCOMPARE(m_a->nnrLimit(), 1);
        QCOMPARE(m_a->nnrTuning().modelSlot, 1);
        QCOMPARE(m_a->nnrDiagnostics().actualModelSlot, 0);
        QVERIFY(m_a->nnrDiagnostics().running);

        QVERIFY(m_a->requestNnrLimit(2));
        QVERIFY(m_a->setActiveNr(NrSlot::NR2));
        QVERIFY(m_a->setActiveNr(NrSlot::NNR));   // selected, held off
        QCOMPARE(m_a->activeNr(), NrSlot::NNR);
        QVERIFY(!m_a->nnrDiagnostics().running);
        QVERIFY(m_a->requestNnrLimit(0));
        QVERIFY(m_a->setNnrTuning(settings));
        QVERIFY(m_a->nnrDiagnostics().running);
        QCOMPARE(m_a->nnrDiagnostics().actualModelSlot, 1);
    }

    // R-R3-40: the legacy model setter (no NereusSDR caller) requests its
    // model through the runtime limit instead of switching past it.
    void legacyModelSetterGoesThroughTheLimit()
    {
        const int channel = m_a->channelId();
        auto settings = m_a->nnrTuning();
        settings.modelSlot = 0;
        QVERIFY(m_a->setNnrTuning(settings));
        QVERIFY(m_a->requestNnrLimit(1));
        QCOMPARE(SetRXANNRModel(channel, 1), 0);   // applies the pending limit first
        NNRRuntimeStatus status{};
        QVERIFY(GetRXANNRStatus(channel, &status));
        QCOMPARE(status.limit, 1);
        QCOMPARE(status.configuration.model_slot, 1);
        QCOMPARE(status.active_model_slot, 0);
        QVERIFY(m_a->requestNnrLimit(0));
        QCOMPARE(SetRXANNRModel(channel, 1), 1);
        QVERIFY(m_a->setNnrTuning(settings));   // back to the fixture's choice
        QCOMPARE(m_a->nnrDiagnostics().actualModelSlot, 0);
    }

    void modelPathsStayFixedUntilReconnect()
    {
        QString reason;
        QVERIFY(!m_engine->setNnrModelPaths({}, &reason));
        QVERIFY(reason.contains("disconnected and connected again"));
    }

    void cleanupTestCase()
    {
        m_a = nullptr;
        m_b = nullptr;
        m_engine.reset();
    }

private:
    QTemporaryDir m_directory;
    std::unique_ptr<WdspEngine> m_engine;
    RxChannel* m_a{nullptr};
    RxChannel* m_b{nullptr};
};

QTEST_APPLESS_MAIN(TestWdspNnr)
#include "tst_wdsp_nnr.moc"
