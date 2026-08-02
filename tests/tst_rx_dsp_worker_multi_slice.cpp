// =================================================================
// tests/tst_rx_dsp_worker_multi_slice.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Task 4: per-stream accumulation, per-slice fan-out.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/P2RadioConnection.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/RxDspWorker.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRxDspWorkerMultiSlice : public QObject {
    Q_OBJECT

    struct DetachConnection {
        RadioModel* model{nullptr};
        ~DetachConnection()
        {
            if (model) {
                model->injectConnectionForTest(nullptr);
            }
        }
    };

    struct DiversityRecorder {
        int processCalls{0};
        int runStops{0};
        int destroys{0};
        QStringList lifecycle;
        QVector<double> primary;
        QVector<double> secondary;
        int targetSlice{-1};
        QVector<float> targetI;
        QVector<float> targetQ;
    };

    static inline DiversityRecorder* s_diversity = nullptr;

    static void divCreate(int, int, int, int)
    {
        s_diversity->lifecycle.append(QStringLiteral("create"));
    }
    static void divDestroy(int)
    {
        ++s_diversity->destroys;
        s_diversity->lifecycle.append(QStringLiteral("destroy"));
    }
    static void divProcess(int, int samples, double** inputs, double* output)
    {
        ++s_diversity->processCalls;
        s_diversity->primary =
            QVector<double>(inputs[0], inputs[0] + 2 * samples);
        s_diversity->secondary =
            QVector<double>(inputs[1], inputs[1] + 2 * samples);
        for (int i = 0; i < 2 * samples; ++i) {
            output[i] = inputs[0][i] + inputs[1][i];
        }
    }
    static void divRun(int, int run)
    {
        if (run == 0) {
            ++s_diversity->runStops;
            s_diversity->lifecycle.append(QStringLiteral("run0"));
        } else {
            s_diversity->lifecycle.append(QStringLiteral("run1"));
        }
    }
    static void divNr(int, int)
    {
        s_diversity->lifecycle.append(QStringLiteral("nr"));
    }
    static void divOutput(int, int)
    {
        s_diversity->lifecycle.append(QStringLiteral("output"));
    }
    static void divRotate(int, int, double*, double*)
    {
        s_diversity->lifecycle.append(QStringLiteral("rotate"));
    }

    static WdspEngine::ExternalDiversityApiForTest diversityApi()
    {
        return {
            &divCreate, &divDestroy, &divProcess, &divRun,
            &divNr, &divOutput, &divRotate,
        };
    }

    static void captureDiversityTarget(int targetSlice,
                                       const float* i,
                                       const float* q,
                                       int samples)
    {
        s_diversity->targetSlice = targetSlice;
        s_diversity->targetI = QVector<float>(i, i + samples);
        s_diversity->targetQ = QVector<float>(q, q + samples);
    }

    static void captureDiversityRoute(bool active, int targetSlice,
                                      int primarySource, int secondarySource)
    {
        s_diversity->lifecycle.append(
            active ? QStringLiteral("route") : QStringLiteral("clear"));
        if (active) {
            QCOMPARE(targetSlice, 0);
            QCOMPARE(primarySource, 0);
            QCOMPARE(secondarySource, 1);
        }
    }

    static void armDiversity(WdspEngine& engine, int samples)
    {
        engine.setExternalDiversityApiForTest(diversityApi());
        QVERIFY(engine.createExternalDiversity(0, 2, samples));
        double iRotate[2] = {1.0, 1.0};
        double qRotate[2] = {0.0, 0.0};
        engine.configureExternalDiversity(0, 2, iRotate, qRotate, 2);
        engine.setExternalDiversityRunning(0, true);
    }

private slots:
    void streams_do_not_share_an_accumulator()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        QSignalSpy spy(&worker, &RxDspWorker::chunkDrained);

        const QVector<float> two{0.1f, 0.1f, 0.2f, 0.2f};   // 2 samples
        worker.processIqBatch(0, two);
        worker.processIqBatch(1, two);

        // Shared accumulator would total 4 and drain. Per-stream: neither
        // reaches 4, so nothing drains.
        QCOMPARE(spy.count(), 0);
    }

    void a_stream_drains_at_its_own_threshold()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(3, four);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
        QCOMPARE(spy.at(0).at(1).toInt(), 4);
    }

    void every_slice_bound_to_a_stream_is_offered_the_drain()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        // Slices 0, 2 and 3 all live on stream 1 (they share a DDC).
        worker.setStreamSlices(1, QVector<int>{0, 2, 3});

        QSignalSpy spy(&worker, &RxDspWorker::sliceProcessed);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(1, four);

        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(1).at(0).toInt(), 2);
        QCOMPARE(spy.at(2).at(0).toInt(), 3);
    }

    void noise_blanker_runs_once_per_stream_not_once_per_slice()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        worker.setStreamSlices(1, QVector<int>{0, 2, 3});

        QSignalSpy spy(&worker, &RxDspWorker::streamNoiseBlankerApplied);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(1, four);

        // Three slices share the chunk, but the blanker is a property of
        // the DDC stream, so it must be applied exactly once.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);   // stream index
    }

    void a_stream_with_no_slices_processes_nothing()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        QSignalSpy spy(&worker, &RxDspWorker::sliceProcessed);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(2, four);   // no setStreamSlices for 2

        QCOMPARE(spy.count(), 0);
    }

    // ── Phase 3F Sub-Epic I closeout defect G1: three cases retired ───────
    //
    // Three cases stood here pinning the anti-VOX fork's per-stream
    // behaviour: that it fired once per drain interval rather than once per
    // draining stream, that a stream without slice 0 never raised it, and
    // that it followed slice 0 across a stream migration.
    //
    // Phase 3F Sub-Epic J Task 9 retired the fork. The anti-VOX reference is
    // now AudioEngine::m_antiVoxMix, a second MasterMixer summing every
    // audible slice, which is what Thetis does (every sub-receiver goes to
    // the transmitter's anti-VOX mixer, cmaster.c:371-372 [v2.10.3.15]).
    // Slice A's audio alone was the wrong reference the moment a second
    // receiver became audible, so all three properties above described a
    // topology that no longer exists: the feed has no per-slice and no
    // per-stream identity left to assert on.
    //
    // The one property that survives the move is the CADENCE those cases
    // existed to protect, one block per outSize/outRate seconds, which the
    // mixer's readiness barrier now supplies. It is pinned by
    // tst_audio_engine_antivox_mix's theAntiVoxMixDrainsOneBlockPerPeriod.

    // ── Phase 3F Sub-Epic I closeout, defect H1 ─────────────────────────────
    //
    // One drain geometry served every stream: setBufferSizes(inSize, outSize)
    // was called once from RadioModel with the connection-wide rate's inSize,
    // and the drain loop used that single threshold for every stream's
    // accumulator. The moment setStreamSampleRate let stream 1 run at a rate
    // stream 0 does not share, the threshold was wrong for whichever stream it
    // was not computed from, and fexchange2 received the wrong sample count.
    //
    // Per-stream sizing mirrors ChannelMaster, which stores the buffer size
    // per input stream rather than per radio:
    //   From Thetis cmaster.c:461 [v2.10.3.15]
    //     pcm->xcm_insize[in_id] = getbuffsize (rate);
    // with getbuffsize(rate) = 64 * rate / 48000 (cmsetup.c:106-111
    // [v2.10.3.15]), which is bufferSizeForRate() here.

    void streams_drain_at_their_own_chunk_sizes()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);          // global default
        worker.setStreamInputChunk(1, 8);      // stream 1 runs wider

        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};

        // Stream 0 keeps the global threshold: four samples is a whole chunk.
        worker.processIqBatch(0, four);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(0).at(1).toInt(), 4);

        // Stream 1 needs eight. Four must NOT drain it: the shared-threshold
        // bug drained here and handed fexchange2 half a chunk.
        worker.processIqBatch(1, four);
        QCOMPARE(spy.count(), 1);

        // Four more completes stream 1's own chunk, at ITS size.
        worker.processIqBatch(1, four);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toInt(), 1);
        QCOMPARE(spy.at(1).at(1).toInt(), 8);
    }

    // Neither stream may starve or over-drain the other: one wide stream and
    // one narrow stream fed the same number of samples produce drain counts
    // that follow their own thresholds, not each other's.
    void a_wide_stream_does_not_starve_a_narrow_one()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        worker.setStreamInputChunk(1, 8);

        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        // 16 samples into each stream: stream 0 must drain 4 chunks of 4,
        // stream 1 must drain 2 chunks of 8.
        QVector<float> sixteen;
        for (int i = 0; i < 16; ++i) {
            sixteen.append(0.1f);
            sixteen.append(0.2f);
        }
        worker.processIqBatch(0, sixteen);
        worker.processIqBatch(1, sixteen);

        int chunksOnZero = 0;
        int chunksOnOne  = 0;
        for (int i = 0; i < spy.count(); ++i) {
            const int st = spy.at(i).at(0).toInt();
            const int n  = spy.at(i).at(1).toInt();
            if (st == 0) { ++chunksOnZero; QCOMPARE(n, 4); }
            if (st == 1) { ++chunksOnOne;  QCOMPARE(n, 8); }
        }
        QCOMPARE(chunksOnZero, 4);
        QCOMPARE(chunksOnOne, 2);
    }

    // The single-rate path must be untouched: a stream nobody sized follows
    // the global default exactly as before.
    void a_stream_without_an_override_uses_the_global_default()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        worker.setStreamInputChunk(1, 8);      // only stream 1 is overridden

        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(3, four);        // never given a size

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
        QCOMPARE(spy.at(0).at(1).toInt(), 4);
    }

    void clearing_an_override_returns_the_stream_to_the_global_default()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        worker.setStreamInputChunk(1, 8);
        worker.setStreamInputChunk(1, 0);      // 0 = drop the override

        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(1, four);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 4);
    }

    // A partial chunk captured at the old rate cannot be carried into a chunk
    // that WDSP will interpret at the new rate: fexchange2 reads in_size
    // samples and the channel carries ONE input rate (channel.c:197-208
    // [WDSP v1.29]), so a mixed-timebase chunk is demodulated wrong end to
    // end, not merely clicked at the seam. Drop it, exactly as
    // setSampleRateLive already drops every accumulator through
    // resetAccumulator() before reconfiguring.
    void changing_a_streams_chunk_size_drops_its_partial_accumulator()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);

        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        const QVector<float> two{0.1f, 0.1f, 0.2f, 0.2f};   // 2 samples
        worker.processIqBatch(1, two);                      // partial, at 4
        QCOMPARE(spy.count(), 0);

        worker.setStreamInputChunk(1, 8);

        // Six more. Carried over, 2 + 6 would be a full 8-chunk and drain.
        QVector<float> six;
        for (int i = 0; i < 6; ++i) {
            six.append(0.3f);
            six.append(0.4f);
        }
        worker.processIqBatch(1, six);
        QCOMPARE(spy.count(), 0);

        // Two more brings the post-change total to 8 and drains once.
        worker.processIqBatch(1, two);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 8);
    }

    // ...but an idempotent re-push must not punch a hole in the audio.
    void re_pushing_the_same_chunk_size_keeps_the_partial()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        worker.setStreamInputChunk(1, 4);

        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        const QVector<float> two{0.1f, 0.1f, 0.2f, 0.2f};
        worker.processIqBatch(1, two);
        worker.setStreamInputChunk(1, 4);   // same size, so no drop
        worker.processIqBatch(1, two);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 4);
    }

    // Every slice on the stream is offered the stream's chunk, not the
    // global one: the fan-out count has to follow the same threshold.
    void the_fan_out_carries_the_streams_own_chunk_size()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        worker.setStreamInputChunk(1, 8);
        worker.setStreamSlices(1, QVector<int>{0, 2});

        QSignalSpy spy(&worker, &RxDspWorker::sliceProcessed);

        QVector<float> eight;
        for (int i = 0; i < 8; ++i) {
            eight.append(0.1f);
            eight.append(0.2f);
        }
        worker.processIqBatch(1, eight);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(1).toInt(), 8);
        QCOMPARE(spy.at(1).at(1).toInt(), 8);
    }

    void one_diversity_source_alone_produces_no_output()
    {
        DiversityRecorder record;
        s_diversity = &record;
        WdspEngine engine;
        armDiversity(engine, 4);

        RxDspWorker worker;
        worker.setEngines(&engine, nullptr);
        worker.setBufferSizes(4, 64);
        worker.setExternalDiversityOutputHookForTest(&captureDiversityTarget);
        worker.setExternalDiversityRoute(0, 7, 10, 11);

        const QVector<float> primary{
            1, 101, 2, 102, 3, 103, 4, 104,
        };
        worker.processExternalDiversityIqBatch(10, primary);

        QCOMPARE(record.processCalls, 0);
        QCOMPARE(record.targetSlice, -1);
    }

    void paired_sources_keep_order_and_feed_only_the_designated_target()
    {
        DiversityRecorder record;
        s_diversity = &record;
        WdspEngine engine;
        armDiversity(engine, 4);

        RxDspWorker worker;
        worker.setEngines(&engine, nullptr);
        worker.setBufferSizes(4, 64);
        worker.setExternalDiversityOutputHookForTest(&captureDiversityTarget);
        worker.setExternalDiversityRoute(0, 7, 10, 11);
        QSignalSpy sliceSpy(&worker, &RxDspWorker::sliceProcessed);

        const QVector<float> primary{
            1, 101, 2, 102, 3, 103, 4, 104,
        };
        const QVector<float> secondary{
            10, 110, 20, 120, 30, 130, 40, 140,
        };
        worker.processExternalDiversityIqBatch(10, primary);
        worker.processExternalDiversityIqBatch(11, secondary);

        QCOMPARE(record.processCalls, 1);
        QCOMPARE(record.primary, QVector<double>({
            1, 101, 2, 102, 3, 103, 4, 104,
        }));
        QCOMPARE(record.secondary, QVector<double>({
            10, 110, 20, 120, 30, 130, 40, 140,
        }));
        QCOMPARE(record.targetSlice, 7);
        QCOMPARE(record.targetI, QVector<float>({11, 22, 33, 44}));
        QCOMPARE(record.targetQ, QVector<float>({211, 222, 233, 244}));
        QCOMPARE(sliceSpy.count(), 1);
        QCOMPARE(sliceSpy.at(0).at(0).toInt(), 7);
    }

    void ordinary_cohosted_slices_continue_while_target_skips_normal_fanout()
    {
        DiversityRecorder record;
        s_diversity = &record;
        WdspEngine engine;
        armDiversity(engine, 4);

        RxDspWorker worker;
        worker.setEngines(&engine, nullptr);
        worker.setBufferSizes(4, 64);
        worker.setStreamSlices(10, QVector<int>{7, 8});
        worker.setStreamSlices(11, QVector<int>{9});
        worker.setExternalDiversityRoute(0, 7, 10, 11);
        QSignalSpy sliceSpy(&worker, &RxDspWorker::sliceProcessed);

        const QVector<float> chunk{
            1, 2, 3, 4, 5, 6, 7, 8,
        };
        worker.processIqBatch(10, chunk);
        worker.processIqBatch(11, chunk);

        QCOMPARE(sliceSpy.count(), 2);
        QCOMPARE(sliceSpy.at(0).at(0).toInt(), 8);
        QCOMPARE(sliceSpy.at(1).at(0).toInt(), 9);
        QCOMPARE(record.processCalls, 0);
    }

    void differently_chunked_sources_wait_for_equal_target_chunks()
    {
        DiversityRecorder record;
        s_diversity = &record;
        WdspEngine engine;
        armDiversity(engine, 4);

        RxDspWorker worker;
        worker.setEngines(&engine, nullptr);
        worker.setBufferSizes(4, 64);
        worker.setExternalDiversityRoute(0, 7, 10, 11);

        worker.processExternalDiversityIqBatch(
            10, QVector<float>{1, 1, 2, 2, 3, 3});
        worker.processExternalDiversityIqBatch(
            11, QVector<float>{10, 10, 20, 20});
        worker.processExternalDiversityIqBatch(
            11, QVector<float>{30, 30, 40, 40});
        QCOMPARE(record.processCalls, 0);

        worker.processExternalDiversityIqBatch(
            10, QVector<float>{4, 4});
        QCOMPARE(record.processCalls, 1);
    }

    void route_clear_flushes_unmatched_samples_and_never_promotes_a_slice()
    {
        DiversityRecorder record;
        s_diversity = &record;
        WdspEngine engine;
        armDiversity(engine, 4);

        RxDspWorker worker;
        worker.setEngines(&engine, nullptr);
        worker.setBufferSizes(4, 64);
        worker.setExternalDiversityOutputHookForTest(&captureDiversityTarget);
        worker.setStreamSlices(10, QVector<int>{3, 7});
        worker.setExternalDiversityRoute(0, 7, 10, 11);

        worker.processExternalDiversityIqBatch(
            10, QVector<float>{1, 1, 2, 2});
        worker.clearExternalDiversityRoute();
        engine.setExternalDiversityRunning(0, false);
        engine.destroyExternalDiversity(0);

        // Re-arm the same stable target after the source lifecycle changes.
        armDiversity(engine, 4);
        worker.setExternalDiversityRoute(0, 7, 10, 11);
        worker.processExternalDiversityIqBatch(
            11, QVector<float>{10, 10, 20, 20, 30, 30, 40, 40});
        worker.processExternalDiversityIqBatch(
            10, QVector<float>{3, 3, 4, 4});
        QCOMPARE(record.processCalls, 0); // old primary samples were flushed

        worker.processExternalDiversityIqBatch(
            10, QVector<float>{5, 5, 6, 6});
        QCOMPARE(record.processCalls, 1);
        QCOMPARE(record.targetSlice, 7); // never positional first slice 3
        QCOMPARE(record.runStops, 1);
        QCOMPARE(record.destroys, 1);
    }

    // Mutation caught: removing clearExternalDiversityRoute() from
    // RadioModel's disable sequence leaves the worker route live after the
    // WDSP slot is destroyed. Recreating slot 0 later then lets packets from
    // the old source pair reach the old target.
    void radio_model_disable_flushes_the_worker_route_before_slot_teardown()
    {
        DiversityRecorder record;
        s_diversity = &record;
        RxDspWorker worker;
        RadioModel model;
        P2RadioConnection connection;
        connection.setBoardForTest(HPSDRHW::Saturn);
        model.injectConnectionForTest(&connection);
        DetachConnection detach{&model};
        model.configureStreamPool(
            /*userDdcCount=*/2, /*maxSlices=*/2, /*defaultRateHz=*/48000);
        const int targetId = model.addSlice();
        QCOMPARE(targetId, 0);
        SliceModel* target = model.sliceById(targetId);
        QVERIFY(target != nullptr);

        model.attachDspWorkerForTest(&worker);
        worker.setEngines(model.wdspEngine(), nullptr);
        worker.setBufferSizes(64, 64);

        model.wdspEngine()->setExternalDiversityApiForTest(diversityApi());
        target->setDiversityEnabled(true);

        target->setDiversityEnabled(false);
        QCOMPARE(record.runStops, 1);
        QCOMPARE(record.destroys, 1);

        // Reusing slot 0 must not resurrect the disabled route.
        armDiversity(*model.wdspEngine(), 4);
        worker.processExternalDiversityIqBatch(
            0, QVector<float>{1, 1, 2, 2, 3, 3, 4, 4});
        worker.processExternalDiversityIqBatch(
            1, QVector<float>{10, 10, 20, 20, 30, 30, 40, 40});
        QCOMPARE(record.processCalls, 0);
    }

    // Mutation caught: starting Run before publishing the worker route creates
    // a live pdiv slot with no paired source owner. Likewise, configuring
    // before Create is silently ignored by WdspEngine's lifecycle guard.
    void radio_model_enable_orders_create_configure_route_then_run_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("Saturn-ddcEnable-includes-partner")
            << static_cast<int>(HPSDRHW::Saturn);
        QTest::newRow("Hermes-syncEnable-owns-partner")
            << static_cast<int>(HPSDRHW::Hermes);
    }

    void radio_model_enable_orders_create_configure_route_then_run()
    {
        QFETCH(int, board);
        DiversityRecorder record;
        s_diversity = &record;
        RxDspWorker worker;
        RadioModel model;
        P2RadioConnection connection;
        connection.setBoardForTest(static_cast<HPSDRHW>(board));
        model.injectConnectionForTest(&connection);
        DetachConnection detach{&model};
        model.configureStreamPool(
            /*userDdcCount=*/2, /*maxSlices=*/2, /*defaultRateHz=*/48000);
        const int targetId = model.addSlice();
        QCOMPARE(targetId, 0);
        SliceModel* target = model.sliceById(targetId);
        QVERIFY(target != nullptr);

        model.attachDspWorkerForTest(&worker);
        worker.setEngines(model.wdspEngine(), nullptr);
        worker.setBufferSizes(64, 64);
        worker.setExternalDiversityRouteHookForTest(&captureDiversityRoute);
        model.wdspEngine()->setExternalDiversityApiForTest(diversityApi());

        record.lifecycle.clear();
        target->setDiversityEnabled(true);

        QCOMPARE(record.lifecycle, QStringList({
            QStringLiteral("create"),
            QStringLiteral("nr"),
            QStringLiteral("output"),
            QStringLiteral("rotate"),
            QStringLiteral("route"),
            QStringLiteral("run1"),
        }));
        QCOMPARE(target->ddcIndex(), 0);

        QVector<float> primary(2 * 64, 1.0f);
        QVector<float> secondary(2 * 64, 2.0f);
        worker.processExternalDiversityIqBatch(0, primary);
        worker.processExternalDiversityIqBatch(1, secondary);
        QCOMPARE(record.processCalls, 1);
    }

    // Mutation caught: list-position lookup promotes Slice B after Slice A is
    // removed. The stable target id must stop exactly once and remain absent.
    void removing_stable_diversity_target_stops_once_without_promotion()
    {
        DiversityRecorder record;
        s_diversity = &record;
        RxDspWorker worker;
        RadioModel model;
        P2RadioConnection connection;
        connection.setBoardForTest(HPSDRHW::Saturn);
        model.injectConnectionForTest(&connection);
        DetachConnection detach{&model};
        model.configureStreamPool(
            /*userDdcCount=*/2, /*maxSlices=*/2, /*defaultRateHz=*/48000);
        const int targetId = model.addSlice();
        const int remainingId = model.addSlice();
        QCOMPARE(targetId, 0);
        QCOMPARE(remainingId, 1);
        SliceModel* target = model.sliceById(targetId);
        SliceModel* remaining = model.sliceById(remainingId);
        QVERIFY(target != nullptr);
        QVERIFY(remaining != nullptr);

        model.attachDspWorkerForTest(&worker);
        worker.setEngines(model.wdspEngine(), nullptr);
        worker.setBufferSizes(64, 64);
        worker.setExternalDiversityRouteHookForTest(&captureDiversityRoute);
        model.wdspEngine()->setExternalDiversityApiForTest(diversityApi());
        target->setDiversityEnabled(true);

        record.lifecycle.clear();
        model.removeSlice(targetId);
        QCOMPARE(record.lifecycle, QStringList({
            QStringLiteral("clear"),
            QStringLiteral("run0"),
            QStringLiteral("destroy"),
        }));
        QCOMPARE(record.runStops, 1);
        QCOMPARE(record.destroys, 1);

        record.lifecycle.clear();
        remaining->setDiversityEnabled(true);
        QVERIFY(record.lifecycle.isEmpty());
        QCOMPARE(record.runStops, 1);
        QCOMPARE(record.destroys, 1);
    }
};

QTEST_MAIN(TestRxDspWorkerMultiSlice)
#include "tst_rx_dsp_worker_multi_slice.moc"
