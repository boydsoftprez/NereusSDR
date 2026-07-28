// =================================================================
// tests/tst_rx_dsp_worker_multi_slice.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Task 4: per-stream accumulation, per-slice fan-out.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/RxDspWorker.h"

using namespace NereusSDR;

class TestRxDspWorkerMultiSlice : public QObject {
    Q_OBJECT
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
};

QTEST_MAIN(TestRxDspWorkerMultiSlice)
#include "tst_rx_dsp_worker_multi_slice.moc"
