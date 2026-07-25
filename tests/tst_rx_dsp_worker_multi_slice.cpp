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

    // ── Phase 3F Sub-Epic I closeout, defect G1 ─────────────────────────────
    // The anti-VOX cancellation feed is a per-RADIO feed, not a per-stream
    // one. WDSP DEXP is told a single block geometry via SetAntiVOXSize /
    // SetAntiVOXRate, and dexp.c:288-297 [v2.10.3.15] integrates exactly one
    // antivox_size block per antivox_new flag. Emitting once per draining
    // stream would hand DEXP twice the block rate it was configured for, and
    // half of those blocks would be a stale repeat of slice 0's previous
    // chunk (m_interleavedOut is only ever written by the slice-0 branch).
    void anti_vox_fires_once_per_drain_interval_not_once_per_stream()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        // Slice 0 lives on stream 0 (the constructor seed); stream 1 hosts a
        // second slice on its own DDC.
        worker.setStreamSlices(0, QVector<int>{0});
        worker.setStreamSlices(1, QVector<int>{1});

        QSignalSpy spy(&worker, &RxDspWorker::antiVoxSampleReady);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(0, four);   // stream 0 drains once
        worker.processIqBatch(1, four);   // stream 1 drains once

        // Two streams drained, but only the one hosting slice 0 may feed the
        // detector: one block per drain interval, matching what
        // setAntiVoxBlockGeometry told WDSP.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);    // sliceId
        QCOMPARE(spy.at(0).at(2).toInt(), 64);   // sampleCount == outSize
    }

    void anti_vox_does_not_fire_for_a_stream_without_slice_zero()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        // Slice 0 has migrated off stream 0; stream 2 carries slices 1 and 3.
        worker.setStreamSlices(0, QVector<int>{});
        worker.setStreamSlices(2, QVector<int>{1, 3});

        QSignalSpy spy(&worker, &RxDspWorker::antiVoxSampleReady);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(2, four);
        worker.processIqBatch(0, four);   // bound to nothing at all

        QCOMPARE(spy.count(), 0);
    }

    // Slice 0 is not pinned to stream 0: a migration must take the anti-VOX
    // feed with it, because the feed's identity is "the audio slice 0 puts on
    // the speakers", not "whatever stream 0 happens to be doing".
    void anti_vox_follows_slice_zero_when_it_migrates_streams()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        worker.setStreamSlices(0, QVector<int>{2});
        worker.setStreamSlices(1, QVector<int>{0});

        QSignalSpy spy(&worker, &RxDspWorker::antiVoxSampleReady);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(0, four);
        worker.processIqBatch(1, four);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
    }
};

QTEST_MAIN(TestRxDspWorkerMultiSlice)
#include "tst_rx_dsp_worker_multi_slice.moc"
