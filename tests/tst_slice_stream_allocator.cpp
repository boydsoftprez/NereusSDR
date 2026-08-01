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

    // ── preferOwnStream: what "+PAN" means ──────────────────────────────
    //
    // Bench-caught 2026-08-01 (J.J. Boyd, KG4VCF): adding a second pan while
    // the first sat on 20m produced two pans that tuned together. The new
    // slice is seeded at the active slice's frequency, so it landed inside
    // that window and the sharing preference absorbed it onto the same DDC.
    // Both pans then rendered one stream, because every pan is recentred on
    // its stream (MainWindow::applyStreamWindowToPan).
    //
    // Sharing is correct for a slice: two signals in one passband cost one
    // DDC and no extra bus bandwidth. It is wrong for a PAN, which exists to
    // be an independent window. preferOwnStream is how the caller says which
    // it is asking for.
    void a_pan_wanting_its_own_window_skips_the_sharing_preference()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 5, /*maxSlices*/ 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // Same frequency the existing stream is centred on: the sharing
        // scan would otherwise take it every time.
        const auto r = alloc.placeSlice(14200000.0, /*preferOwnStream=*/true);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(r.streamIndex, 1);
        QCOMPARE(r.shiftOffsetHz, 0.0);
        QCOMPARE(r.newStreamCentreHz, 14200000.0);
    }

    // The default is unchanged, so a slice added to an existing pan still
    // shares. This is the same frequency as the case above, and it must give
    // the opposite answer.
    void a_slice_at_the_same_frequency_still_shares_by_default()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.placeSlice(14200000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
        QCOMPARE(r.streamIndex, 0);
    }

    // A pan cannot be conjured out of a radio with no DDC left. Refusing is
    // the honest answer: silently sharing would recreate the coupled-pans
    // defect, which is the thing preferOwnStream exists to prevent.
    void a_pan_wanting_its_own_window_is_refused_when_none_is_free()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 2, /*maxSlices*/ 5);
        alloc.activateStream(0, 14200000.0, 192000);
        alloc.activateStream(1, 7150000.0, 192000);

        const auto r = alloc.placeSlice(14200000.0, /*preferOwnStream=*/true);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::Rejected);
        QVERIFY(!r.reason.isEmpty());
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
                                         /*soleOccupant*/ false,
                                         /*ddcPinned*/ false,
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
        const auto r = alloc.retuneSlice(0, /*soleOccupant*/ true,
                                         /*ddcPinned*/ false, 7150000.0);

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
        const auto r = alloc.retuneSlice(0, /*soleOccupant*/ false,
                                         /*ddcPinned*/ false, 7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(r.streamIndex, 1);
    }

    // ── Bench defect, ANAN-G2E 2026-07-26 ────────────────────────────────
    //
    // Two pans, Slice A on 40 m (stream 0) and Slice B on 20 m (stream 1) at
    // 48 kHz, so B's window is only +-24 kHz. CTUN was on, which pinned the
    // DDC. Tuning B a little way inside 20 m put it outside that narrow
    // window, and because the pin withheld permission to re-centre, B
    // MIGRATED to stream 2 -- while stream 1, whose only occupant had just
    // left, went idle. The enable mask became DDC0 + DDC2 with a hole at
    // DDC1, the radio was still streaming the DDC the client had stopped
    // listening to, and the second pan went dead.
    //
    // The pin is meant to stop the panadapter sliding under the operator
    // while the VFO moves INSIDE the window. Once the slice leaves, the pan
    // has to jump regardless -- MainWindow's band-jump handler drops the
    // lock and re-centres for exactly this case -- so honouring the pin here
    // buys nothing and costs a DDC.
    void a_pinned_sole_occupant_leaving_its_window_keeps_its_own_stream()
    {
        SliceStreamAllocator alloc;
        alloc.configure(4, 5);
        alloc.activateStream(0, 7245000.0, 48000);   // Slice A, 40 m
        alloc.activateStream(1, 14200000.0, 48000);  // Slice B, 20 m

        // B tunes 40 kHz up: still 20 m, but outside a +-24 kHz window.
        const auto r = alloc.retuneSlice(/*currentStream*/ 1,
                                         /*soleOccupant*/ true,
                                         /*ddcPinned*/ true,
                                         14240000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::RetunedStream);
        QCOMPARE(r.streamIndex, 1);       // NOT 2 -- no migration, no hole
        QCOMPARE(r.shiftOffsetHz, 0.0);
        QCOMPARE(r.newStreamCentreHz, 14240000.0);
    }

    // The pin still does its job for the case it exists for.
    void a_pinned_sole_occupant_inside_its_window_still_only_shifts()
    {
        SliceStreamAllocator alloc;
        alloc.configure(4, 5);
        alloc.activateStream(1, 14200000.0, 48000);

        const auto r = alloc.retuneSlice(1, /*soleOccupant*/ true,
                                         /*ddcPinned*/ true, 14210000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
        QCOMPARE(r.streamIndex, 1);
        QCOMPARE(r.shiftOffsetHz, 10000.0);
    }

    // A slice someone else is sharing a window with still has to leave.
    void a_pinned_co_hosted_slice_leaving_its_window_still_migrates()
    {
        SliceStreamAllocator alloc;
        alloc.configure(4, 5);
        alloc.activateStream(0, 14200000.0, 48000);

        const auto r = alloc.retuneSlice(0, /*soleOccupant*/ false,
                                         /*ddcPinned*/ true, 7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(r.streamIndex, 1);
    }
};

QTEST_MAIN(TestSliceStreamAllocator)
#include "tst_slice_stream_allocator.moc"
