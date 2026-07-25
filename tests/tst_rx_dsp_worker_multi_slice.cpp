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
};

QTEST_MAIN(TestRxDspWorkerMultiSlice)
#include "tst_rx_dsp_worker_multi_slice.moc"
