// =================================================================
// tests/tst_slice_stream_allocator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. The window-fit rule ports Thetis
// console.cs:31920 [v2.10.3.15]; the promote-instead-of-disable
// behaviour is a documented divergence (design doc §3).
//
// Phase 3F Sub-Epic I Task 2.
// =================================================================
#include <QtTest/QtTest>
#include "core/SliceStreamAllocator.h"

using namespace NereusSDR;

class TestSliceStreamAllocator : public QObject {
    Q_OBJECT
private slots:
    void slice_joins_a_stream_whose_window_contains_it()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 5, /*maxSlices*/ 5);
        // Stream 0 active, centred 14.200 MHz, 192 kHz wide: +-96 kHz.
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.placeSlice(14225000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
        QCOMPARE(r.streamIndex, 0);
        QCOMPARE(r.shiftOffsetHz, 25000.0);
    }

    void slice_outside_every_window_claims_a_free_stream()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.placeSlice(7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(r.streamIndex, 1);
        QCOMPARE(r.shiftOffsetHz, 0.0);   // new stream centres on the slice
    }

    void four_slices_share_one_stream_when_all_fit()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // A through D, all inside +-96 kHz of 14.200.
        for (double f : {14150000.0, 14180000.0, 14225000.0, 14260000.0}) {
            const auto r = alloc.placeSlice(f);
            QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
            QCOMPARE(r.streamIndex, 0);
        }
        QCOMPARE(alloc.activeStreamCount(), 1);
    }

    void edge_of_window_is_excluded()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // Exactly +96 kHz is the Nyquist edge. Thetis uses a strict
        // inequality (console.cs:31920), so this must NOT join.
        const auto r = alloc.placeSlice(14200000.0 + 96000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
    }

    void exhausted_streams_reject_with_a_reason()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 1, /*maxSlices*/ 5);
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.placeSlice(7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::Rejected);
        QVERIFY(!r.reason.isEmpty());
    }

    void retune_inside_the_window_only_moves_the_shift()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);
        alloc.placeSlice(14225000.0);

        const auto r = alloc.retuneSlice(/*currentStream*/ 0,
                                         /*mayRetuneStream*/ false,
                                         14180000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
        QCOMPARE(r.streamIndex, 0);
        QCOMPARE(r.shiftOffsetHz, -20000.0);
    }

    void sole_occupant_leaving_its_window_retunes_the_stream()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // Only slice on stream 0: cheaper to move the DDC than to burn
        // another one.
        const auto r = alloc.retuneSlice(0, /*mayRetuneStream*/ true, 7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::RetunedStream);
        QCOMPARE(r.streamIndex, 0);
        QCOMPARE(r.shiftOffsetHz, 0.0);
    }

    void co_hosted_slice_leaving_its_window_migrates_to_a_free_stream()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // Another slice still needs stream 0 where it is, so this one moves.
        const auto r = alloc.retuneSlice(0, /*mayRetuneStream*/ false, 7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(r.streamIndex, 1);
    }
};

QTEST_MAIN(TestSliceStreamAllocator)
#include "tst_slice_stream_allocator.moc"
