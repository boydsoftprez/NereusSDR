// =================================================================
// tests/tst_slice_stream_allocator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. The window-fit rule ports Thetis
// console.cs:31920 [v2.10.3.15]; the promote-instead-of-disable
// behaviour is a documented divergence (design doc §3).
//
// Phase 3F Sub-Epic I Task 2.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 74 (R-IOS-30): a copy of the policy
//               plans a pan move without touching the live one. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
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
        QCOMPARE(r.reason,
                 QStringLiteral("All 2 of the radio's receivers are in use, so a new "
                                "panadapter cannot have its own. Close a panadapter, or "
                                "add this receiver to an existing panadapter instead."));
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
        // Plain English (R-R3-34); U+00A0 keeps "7.1500" and "MHz" together.
        QCOMPARE(r.reason,
                 QStringLiteral("The radio's only receiver is in use and it does not "
                                "cover 7.1500\u00A0MHz. Retune or close another "
                                "receiver, or choose a higher sample rate so each one "
                                "covers more."));
        QVERIFY(!r.reason.contains(QStringLiteral("DDC")));
    }

    // R-R3-34: joining a panadapter's receiver that does not cover the
    // frequency is refused in plain words; U+00A0 keeps "7.1500" and "MHz"
    // together.
    void joining_a_receiver_that_does_not_cover_the_frequency_is_refused_plainly()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.joinStream(0, 7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::Rejected);
        QCOMPARE(r.reason,
                 QStringLiteral("The radio receiver this panadapter uses does not "
                                "cover 7.1500\u00A0MHz. Retune into its range, or give "
                                "this receiver a panadapter of its own."));
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

    // iPhone app Task 74 (the several-devices design, rulings 6.4 and 6.5):
    // the Core asks before a pan move by planning it on a copy of the
    // policy (ReceiverPlanner::planWindowMove), so what the operator is
    // shown is exactly what the move then does. The copy must never touch
    // the live policy, and its answers are the move's: a slice the moved
    // window leaves goes to a free receiver (moves) or, with none, is
    // refused (closes).
    void a_copy_plans_a_window_move_without_touching_the_live_policy()
    {
        SliceStreamAllocator live;
        live.configure(2, 5);
        live.activateStream(0, 7074000.0, 192000);

        SliceStreamAllocator copy = live;
        copy.activateStream(0, 7000000.0, 192000);
        const auto moved = copy.placeSlice(7150000.0);
        QCOMPARE(moved.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(moved.streamIndex, 1);
        copy.activateStream(moved.streamIndex, moved.newStreamCentreHz, 192000);
        // A second slice the move leaves finds no receiver: it would close.
        QCOMPARE(copy.placeSlice(7400000.0).outcome, SliceStreamAllocator::Outcome::Rejected);

        QCOMPARE(live.streamCentreHz(0), 7074000.0);
        QVERIFY(!live.isStreamActive(1));
        QCOMPARE(live.placeSlice(7150000.0).outcome,
                 SliceStreamAllocator::Outcome::JoinedExisting);
    }
};

QTEST_MAIN(TestSliceStreamAllocator)
#include "tst_slice_stream_allocator.moc"
