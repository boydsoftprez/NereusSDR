// =================================================================
// tests/tst_new_pan_is_its_own_receiver.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Multi-pan slice-to-DDC placement is
// a Phase 3F concept; Thetis has a fixed RX1/RX2 pair and no allocator to
// port from.
//
// Bench report 2026-07-30 (JJ, KG4VCF): "creating a second pan, then
// retuning the first or second pan causes both to tune... dragging and
// panning up and down the band from the waterfall controls both, they
// should be independent."
//
// Not a wiring fault. Every spectrum handler already resolves its own
// pan's slice through sliceForPan(panId), and that was verified. The
// coupling was one layer down: RadioModel::addSlice seeds a new slice on
// the ACTIVE slice's frequency so the pan opens on the band being worked,
// which put it inside the active stream's window, so the allocator shared
// the DDC. A DDC has one centre and feeds one FFT stream, so the two pans
// were two views of a single receiver. They moved together because they
// were one thing.
//
// Operator decision, 2026-07-30: a new pan is a new receiver, matching
// AetherSDR and Thetis RX1/RX2. A slice added to a pan that already has
// slices still shares that pan's receiver, which is what makes co-hosted
// slices on one panadapter free.
//
// These cases pin the placement, which is where the coupling lived. The
// operator-visible half (drag pan A, pan B stays put) is bench row 26.
//
// Modification history (NereusSDR):
//   2026-10-22: a lone slice that tunes well inside another receiver of its
//               pan goes back onto it (JJ's ruling 2026-10-22). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: review fixes: only the highest receiver is freed (both
//               ways round, and none from the middle of three), the new
//               band's antenna label, Diversity, the anchoring device and a
//               required receiver. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-22: second review round: the fixture has a codec (a rejoin
//               needs the ADC each DDC reads); Protocol 1 2-ADC routing,
//               the moving slices' blanker state and device, and the
//               receivers' own C-Tune pins under the station-wide lock.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: third review round: Protocol 1 Diversity's remapped ADCs,
//               one-ADC boards under a two-ADC word, the G2E's codec, no
//               codec, receivers at different rates, and the routing word
//               cleared by a scope guard. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include <QScopeGuard>
#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "core/ReceiverManager.h"
#include "core/SliceOwnership.h"
#include "core/SliceStreamAllocator.h"
#include "core/WdspTypes.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecHermes.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
// Clears a test MAC's settings when the test leaves, pass or fail, so a
// routing word it persisted cannot reach the next test.
auto clearP1Word(const QString& mac)
{
    return qScopeGuard([mac] { AppSettings::instance().clearHardwareValues(mac); });
}
} // namespace

class TestNewPanIsItsOwnReceiver : public QObject
{
    Q_OBJECT

private:
    static void seed(RadioModel& model, int ddcs = 5)
    {
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ ddcs, /*maxSlices*/ 5, 192000);
        // The G2's Protocol 2 codec, as a connection installs it: a rejoin
        // compares the ADC each receiver's DDC reads, which only a codec's
        // assignment says. Stateless, so one serves every model.
        static P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
    }

private slots:

    // ── 1. The reported case ─────────────────────────────────────────────
    //
    // Two pans, same band, because the new pan is seeded on the band being
    // worked. They must still be two receivers.
    void a_second_pan_on_the_same_band_gets_its_own_ddc()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);

        // Seeded onto A's band, which is the behaviour that caused the
        // sharing and which is deliberately kept.
        QCOMPARE(sliceB->frequency(), sliceA->frequency());

        QVERIFY2(sliceA->streamIndex() >= 0 && sliceB->streamIndex() >= 0,
                 "both slices must be bound");
        QVERIFY2(sliceA->streamIndex() != sliceB->streamIndex(),
            "a new pan is a new receiver: sharing the DDC is what made "
            "panning either pan move both");
    }

    // ── 1b. A refused pan leaves nothing behind ──────────────────────────
    //
    // Codex review P1, PR #311. addSlice discarded the bind result, so a
    // refused placement still produced a wired, emitted slice with
    // streamIndex() == -1: a pan that looks populated, receives no I/Q, and
    // permanently burns one of the five slice slots. Reachable since the
    // allocator started refusing instead of falling back to sharing.
    //
    // Two DDCs, both claimed by pans, then a third pan asked for.
    void a_refused_pan_does_not_leave_a_dead_slice()
    {
        RadioModel model;
        seed(model, /*ddcs*/ 2);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(model.sliceById(a));
        model.sliceById(a)->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(model.sliceById(b));

        const int before = model.slices().size();
        QSignalSpy rejected(&model, &RadioModel::sliceAddRejected);

        const int c = model.addSlice(QStringLiteral("pan-2"));

        QCOMPARE(c, -1);
        QCOMPARE(model.slices().size(), before);
        QVERIFY2(model.sliceById(c) == nullptr,
                 "a refused slice must not remain addressable");
        QCOMPARE(rejected.count(), 1);

        // The slot it would have burned is still available: a slice that
        // CAN be placed still succeeds afterwards.
        const int d = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY2(model.sliceById(d) != nullptr,
                 "the refused slice must not have consumed a slice slot");
        QVERIFY(model.sliceById(d)->streamIndex() >= 0);
    }

    // ── 1c. Slice A survives being created before the pool exists ────────
    //
    // The rollback keys on the pool being sized, not on the bare bool.
    // bindSliceToStream also returns false when there is no pool yet, which
    // is exactly where connectToRadio creates Slice A (configureStreamPool
    // runs later, then bindUnboundSlices binds it). Rolling back on the bool
    // alone would delete Slice A on every cold start.
    void a_slice_created_before_the_pool_is_kept()
    {
        RadioModel model;                     // deliberately NOT seeded

        const int a = model.addSlice(QStringLiteral("pan-0"));

        QVERIFY2(a >= 0, "a slice created before connect must still be made");
        QVERIFY(model.sliceById(a) != nullptr);
        QCOMPARE(model.sliceById(a)->streamIndex(), -1);

        // And it binds once the pool arrives, which is the whole reason it
        // had to survive.
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 5, 192000);
        model.bindUnboundSlices();
        QVERIFY2(model.sliceById(a)->streamIndex() >= 0,
                 "bindUnboundSlices must adopt the pre-pool slice");
    }

    // ── 2. +RX on an existing pan still shares ───────────────────────────
    //
    // The other half of the rule. If this also claimed a DDC, co-hosted
    // slices would stop being free and a 5-DDC radio would run out after
    // five slices however they were arranged.
    void a_second_slice_on_the_same_pan_shares_its_receiver()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);

        QCOMPARE(sliceB->streamIndex(), sliceA->streamIndex());
    }

    // ── 3. Four pans, four receivers ─────────────────────────────────────
    //
    // The 2x2 layout, which is where an operator would most expect four
    // independent views.
    void four_pans_claim_four_distinct_ddcs()
    {
        RadioModel model;
        seed(model);

        QSet<int> streams;
        for (int i = 0; i < 4; ++i) {
            const int id = model.addSlice(QStringLiteral("pan-%1").arg(i));
            SliceModel* s = model.sliceById(id);
            QVERIFY(s);
            QVERIFY(s->streamIndex() >= 0);
            streams.insert(s->streamIndex());
        }

        QCOMPARE(streams.size(), 4);
    }

    // ── 4. A full DDC pool refuses rather than silently coupling ─────────
    //
    // This assertion was inverted on 2026-08-01. The original (PR #293) held
    // that a coupled pan beats no pan, so a full pool fell back to sharing.
    // The HL2 bench behind PR #311 showed that fallback is the same defect
    // this flag exists to prevent, arriving silently: the third pan opens,
    // looks independent, and tunes in lockstep with whichever pan it landed
    // on. On a 2-DDC radio "no third independent window" is the truth, and
    // saying so is better than a pan that lies about what it is.
    //
    // The slice is still created; only the stream bind is refused, and the
    // caller surfaces placement.reason.
    void a_new_pan_is_refused_when_no_ddc_is_free()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 2, /*maxSlices*/ 5);
        alloc.setDefaultSampleRateHz(192000);
        alloc.activateStream(0, 14200000.0, 192000);
        alloc.activateStream(1, 7100000.0, 192000);

        using Outcome = SliceStreamAllocator::Outcome;

        // A third pan on a frequency stream 0 already covers. Sharing would
        // succeed and is exactly what must not happen here.
        const auto p = alloc.placeSlice(14200000.0, /*preferOwnStream*/ true);
        QCOMPARE(p.outcome, Outcome::Rejected);
        QVERIFY2(!p.reason.isEmpty(),
                 "a refusal must carry a reason for the operator");
        QVERIFY2(!p.reason.contains(QStringLiteral("none covers")),
            "the pool is exhausted, not mistuned: telling the operator to "
            "retune sends them chasing a fix that cannot work");

        // The same frequency without the flag still shares, which is what
        // keeps co-hosted slices free.
        const auto shared = alloc.placeSlice(14200000.0, /*preferOwnStream*/ false);
        QCOMPARE(shared.outcome, Outcome::JoinedExisting);
        QCOMPARE(shared.streamIndex, 0);
    }

    // ── 5. The allocator honours the preference directly ─────────────────
    //
    // Below RadioModel, so a future caller reading the flag gets the same
    // answer without standing up a model.
    void the_allocator_prefers_a_free_ddc_when_asked()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 4, /*maxSlices*/ 4);
        alloc.setDefaultSampleRateHz(192000);
        alloc.activateStream(0, 14200000.0, 192000);

        using Outcome = SliceStreamAllocator::Outcome;

        // Same frequency, inside stream 0's window.
        const auto shared = alloc.placeSlice(14200000.0, /*preferOwnStream*/ false);
        QCOMPARE(shared.outcome, Outcome::JoinedExisting);
        QCOMPARE(shared.streamIndex, 0);

        const auto dedicated = alloc.placeSlice(14200000.0, /*preferOwnStream*/ true);
        QCOMPARE(dedicated.outcome, Outcome::NewStream);
        QVERIFY2(dedicated.streamIndex != 0,
                 "a dedicated request must not land on the occupied stream");
        QCOMPARE(dedicated.newStreamCentreHz, 14200000.0);
        QCOMPARE(dedicated.shiftOffsetHz, 0.0);
    }

    // ── 6. The preference does not change a retune ───────────────────────
    //
    // Only the first bind may claim. A retune has co-hosts that depend on
    // its window, and pulling a slice onto a fresh DDC there would strand
    // them or leave a hole in the enable mask.
    void retuning_inside_a_shared_window_still_shares()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-0"));   // co-host
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);
        const int shared = sliceA->streamIndex();
        QCOMPARE(sliceB->streamIndex(), shared);

        // Nudge B a little, still inside the 192 kHz window.
        sliceB->setFrequency(14210000.0);

        QCOMPARE(sliceB->streamIndex(), shared);
    }

    // ── 7. A lone slice rejoins its pan's other receiver ─────────────────
    //
    // JJ's ruling 2026-10-22. STRIX Core, 2026-10-22: A restored on
    // 3.87 MHz and B on 7.262 MHz, one pan, two receivers. A was tuned to
    // 7.268 MHz, 6 kHz from B, and kept a receiver of its own, so the
    // window's pan cleared on every A/B click.
    //
    // The receiver freed is always the highest active one, so the DDC enable
    // mask keeps no gap (the 2026-07-26 G2E bench).
    //
    // Two slices on pan-0, A on 80 m on the LOWER receiver and B on 40 m on
    // the higher one (JJ's case). Returns false (with a test failure) when
    // the setup did not land.
    static bool twoReceiversOnOnePan(RadioModel& model, SliceModel*& a, SliceModel*& b)
    {
        a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        if (!a) { return false; }
        a->setFrequency(3870000.0);
        b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        if (!b) { return false; }
        // Seeded on A's frequency, so B shares A's receiver; tuning it to
        // 40 m takes it out of that window onto a receiver of its own.
        b->setFrequency(7262000.0);
        return a->streamIndex() == 0 && b->streamIndex() == 1;
    }

    // The other way round: B on 40 m on the lower receiver, A on 80 m on the
    // higher one.
    static bool loneSliceOnTheHigherReceiver(RadioModel& model, SliceModel*& a, SliceModel*& b)
    {
        b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        if (!b) { return false; }
        b->setFrequency(7262000.0);
        a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        if (!a) { return false; }
        a->setFrequency(3870000.0);
        return b->streamIndex() == 0 && a->streamIndex() == 1;
    }

    // JJ's case: A, alone on the lower receiver, retunes it next to B. B
    // comes down onto A's receiver and the higher one is freed.
    void a_lone_slice_tuned_into_its_pans_other_receiver_rejoins_it()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        QCOMPARE(model.streamAllocator().streamCentreHz(1), 7262000.0);

        a->setFrequency(7268000.0);

        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(model.streamAllocator().streamCentreHz(0), 7268000.0);
        QCOMPARE(a->shiftOffsetHz(), 0.0);
        QCOMPARE(b->shiftOffsetHz(), -6000.0);
        QVERIFY2(!model.streamAllocator().isStreamActive(1),
                 "the higher receiver must be released");
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // A, alone on the higher receiver, joins B's at a shift; B's receiver
    // does not move.
    void a_lone_slice_on_the_higher_receiver_joins_the_lower_one()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(loneSliceOnTheHigherReceiver(model, a, b));

        a->setFrequency(7268000.0);

        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(a->shiftOffsetHz(), 6000.0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(b->shiftOffsetHz(), 0.0);
        QCOMPARE(model.streamAllocator().streamCentreHz(0), 7262000.0);
        QVERIFY(!model.streamAllocator().isStreamActive(1));
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // Three receivers: freeing the middle one would leave a gap, so nothing
    // moves, either way round.
    void a_rejoin_that_would_free_the_middle_receiver_does_not_happen()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(loneSliceOnTheHigherReceiver(model, a, b));
        SliceModel* c = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(c);
        c->setFrequency(14200000.0);
        QCOMPARE(c->streamIndex(), 2);

        // A on the middle receiver tunes next to B on the lowest.
        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().streamCentreHz(1), 7268000.0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 3);

        // B on the lowest tunes next to A on the middle one.
        b->setFrequency(7270000.0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(a->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 3);

        // With C gone the same retune rejoins: the middle is now the top.
        model.removeSlice(c->sliceIndex());
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);
        b->setFrequency(7266000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // Outside the other receiver's window, a lone slice moves its own
    // receiver as before. Then, tuned into it, it rejoins.
    void a_lone_slice_outside_the_other_window_keeps_its_own_receiver()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));

        a->setFrequency(7400000.0);   // 138 kHz from B's centre; window +-96 kHz
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(model.streamAllocator().streamCentreHz(0), 7400000.0);
        QCOMPARE(b->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);

        a->setFrequency(7270000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    // Anti-flap: a slice leaves a shared receiver at the window edge and
    // rejoins only within the inner 80% of it (76.8 kHz of a 96 kHz half
    // window), so stepping across the edge does not bounce it.
    void a_slice_near_the_window_edge_does_not_flap_between_receivers()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(loneSliceOnTheHigherReceiver(model, a, b));
        const double bCentre = 7262000.0;

        a->setFrequency(bCentre + 85000.0);   // in B's window, not well inside
        QCOMPARE(a->streamIndex(), 1);

        a->setFrequency(bCentre + 70000.0);   // well inside: rejoins
        QCOMPARE(a->streamIndex(), 0);

        a->setFrequency(bCentre + 97000.0);   // out of the window: leaves
        QCOMPARE(a->streamIndex(), 1);
        a->setFrequency(bCentre + 95000.0);   // back a step: stays on its own
        QCOMPARE(a->streamIndex(), 1);
        a->setFrequency(bCentre + 97000.0);
        QCOMPARE(a->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().streamCentreHz(0), bCentre);

        // The fold has the same margin: B stays put until A's retuned
        // window holds it well inside.
        RadioModel model2;
        seed(model2);
        QVERIFY(twoReceiversOnOnePan(model2, a, b));
        a->setFrequency(bCentre + 85000.0);
        QCOMPARE(b->streamIndex(), 1);
        a->setFrequency(bCentre + 70000.0);
        QCOMPARE(b->streamIndex(), 0);
    }

    // The receive antenna: joining a receiver whose slices are on another
    // antenna would change that receiver's routing, so it keeps today's
    // behaviour. The slice is compared on the label it will have on its new
    // band (the Alex grid's), not the one it had on the old band.
    void a_slice_on_another_antenna_keeps_its_own_receiver()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        QCOMPARE(model.alexController().rxAnt(Band::Band40m), 1);
        b->setRxAntenna(QStringLiteral("ANT2"));   // B's receiver on ANT2

        a->setFrequency(7268000.0);                // A's 40 m label is ANT1
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().streamCentreHz(0), 7268000.0);

        b->setRxAntenna(QStringLiteral("ANT1"));
        a->setFrequency(7269000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    // Review finding (Critical 1): A on 80 m is labelled ANT2, the 80 m
    // antenna; B on 40 m is labelled ANT1, the 40 m antenna. A tuned to 40 m
    // will be on ANT1, so it shares B's receiver and takes the ANT1 label,
    // and B's receiver stays on ANT1. Both receivers C-Tune pinned. Data:
    // whether the station's band is already 40 m when A crosses (then band
    // tracking does not refresh A's labels at all).
    void a_band_jump_compares_the_new_bands_antenna_data()
    {
        QTest::addColumn<bool>("stationOn40m");
        QTest::newRow("station band 80 m") << false;
        QTest::newRow("station band already 40 m") << true;
    }
    void a_band_jump_compares_the_new_bands_antenna()
    {
        QFETCH(bool, stationOn40m);
        RadioModel model;
        seed(model);
        model.setCapsForTest(/*hasAlex=*/true);
        model.alexControllerMutable().setRxAnt(Band::Band80m, 2);
        model.alexControllerMutable().setRxAnt(Band::Band40m, 1);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        model.setLastBandForTest(stationOn40m ? Band::Band40m : Band::Band80m);
        a->setRxAntenna(QStringLiteral("ANT2"));
        b->setRxAntenna(QStringLiteral("ANT1"));
        model.enableBandTrackingForTest();
        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), true));
        QVERIFY(model.requestStreamCtunPinned(b->sliceIndex(), true));

        a->setFrequency(7268000.0);

        QCOMPARE(a->streamIndex(), b->streamIndex());
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
        QCOMPARE(a->rxAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(b->rxAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(model.alexController().rxAnt(Band::Band40m), 1);
        QVERIFY(a->streamCtunPinned());
    }

    // Another pan's receiver is never joined: that would couple the pans.
    void a_slice_does_not_rejoin_another_pans_receiver()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setFrequency(3870000.0);
        SliceModel* b = model.sliceById(model.addSlice(QStringLiteral("pan-1")));
        QVERIFY(b);
        b->setFrequency(7262000.0);
        const int aStream = a->streamIndex();
        const int bStream = b->streamIndex();
        QVERIFY(aStream != bStream);

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), aStream);
        QCOMPARE(b->streamIndex(), bStream);

        // On B's pan, the same retune shares.
        a->setPanKey(QStringLiteral("pan-1"));
        a->setFrequency(7269000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    // Diversity changes which DDC pair carries a receiver: a slice in it,
    // or a receiver with one on it, is never joined.
    void a_slice_in_diversity_does_not_rejoin()
    {
        // A is slice 0, the one a legacy Diversity request may enable, on
        // the lower receiver (the fold way round).
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        a->setDiversityEnabled(true);
        QVERIFY(a->diversityEnabled());

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);

        a->setDiversityEnabled(false);
        a->setFrequency(7269000.0);
        QCOMPARE(b->streamIndex(), 0);
    }

    void a_receiver_with_a_slice_in_diversity_is_not_joined()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(loneSliceOnTheHigherReceiver(model, a, b));
        b->setDiversityEnabled(true);
        QVERIFY(b->diversityEnabled());

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), 1);

        b->setDiversityEnabled(false);
        a->setFrequency(7269000.0);
        QCOMPARE(a->streamIndex(), 0);
    }

    // Several devices on one Core: a receiver anchored by another device
    // stays that device's.
    void a_receiver_another_device_anchors_is_not_joined()
    {
        RadioModel model;
        seed(model);
        SliceOwnership* owners = model.sliceOwnership();
        QVERIFY(owners);
        const QByteArray phone("phone-device");
        const QByteArray tablet("tablet-device");
        SliceModel* b = nullptr;
        {
            SliceOwnership::CreatorScope creator(owners, phone);
            b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        }
        QVERIFY(b);
        b->setFrequency(7262000.0);
        SliceModel* a = nullptr;
        {
            SliceOwnership::CreatorScope creator(owners, tablet);
            a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        }
        QVERIFY(a);
        a->setFrequency(3870000.0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(a->streamIndex(), 1);
        QCOMPARE(owners->anchorOf(0), phone);

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), 1);

        // The same device's slice joins.
        owners->setOwner(a->sliceIndex(), phone);
        a->setFrequency(7269000.0);
        QCOMPARE(a->streamIndex(), 0);
    }

    // A caller that requires a receiver gets that receiver: a bind asked to
    // stay on its own is never turned into a rejoin.
    void a_required_receiver_is_never_rejoined()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(loneSliceOnTheHigherReceiver(model, a, b));

        QVERIFY(model.bindSliceToStream(a, 7268000.0, false, /*requiredStream=*/1));
        QCOMPARE(a->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().streamCentreHz(1), 7268000.0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);
    }

    // C-Tune: a pinned slice still inside its own window is left there, as
    // C-Tune holds the pan still. A pinned slice that leaves its window joins
    // a receiver with the same pin; one with a different pin is not joined.
    void ctune_holds_a_pinned_slice_inside_its_own_window()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const int aStream = a->streamIndex();
        a->setFrequency(7350000.0);          // 88 kHz from B: not well inside
        QCOMPARE(a->streamIndex(), aStream);
        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), true));
        QVERIFY(model.requestStreamCtunPinned(b->sliceIndex(), true));

        a->setFrequency(7300000.0);          // own window, and well inside B's
        QCOMPARE(a->streamIndex(), aStream);
        QCOMPARE(model.streamAllocator().streamCentreHz(aStream), 7350000.0);

        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), false));
        QVERIFY(model.requestStreamCtunPinned(b->sliceIndex(), false));
        a->setFrequency(7301000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
    }

    void a_pinned_slice_leaving_its_window_joins_only_a_receiver_pinned_alike()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), true));

        a->setFrequency(7268000.0);          // B's receiver is not pinned
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);

        a->setFrequency(3870000.0);
        QVERIFY(model.requestStreamCtunPinned(b->sliceIndex(), true));
        a->setFrequency(7268000.0);          // both pinned: the JJ sequence
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 0);
        QVERIFY(a->streamCtunPinned());
        QVERIFY(b->streamCtunPinned());
        QVERIFY(!model.streamAllocator().isStreamActive(1));
    }

    // A Protocol 1 station: `hpsdr`'s board, a P1 codec, a pool of `pool`
    // receivers, connected under `mac` (so the per-MAC routing word
    // `hardware/<mac>/p1AdcCntrl` applies) with `word` persisted there, or
    // none when `word` is empty. The caller clears the MAC's settings with
    // clearP1Word first, so a failing check leaves nothing behind.
    static void p1Station(RadioModel& model, HPSDRModel hpsdr, int pool, IP1Codec* codec,
                          const QString& mac, const QString& word)
    {
        model.setHpsdrModelForTest(hpsdr);
        model.configureStreamPool(/*userDdcCount*/ pool, /*maxSlices*/ 5, 192000);
        model.receiverManager()->setP1Codec(codec);
        RadioInfo info;
        info.protocol = ProtocolVersion::Protocol1;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        AppSettings::instance().clearHardwareValues(mac);
        if (!word.isEmpty()) {
            AppSettings::instance().setHardwareValue(mac, QStringLiteral("p1AdcCntrl"), word);
        }
    }

    // Second review round. The ADC: on Protocol 1 the bank-4 word routes
    // each frame slot to an ADC whatever the antenna label says. On a 7000D
    // (Orion class, two ADCs) stream 0 reads slot 0 and stream 1 slot 2. The
    // default word (4) puts both on ADC0; 0x10 puts slot 2 on ADC1, the RX2
    // input. Then neither way round may move a slice between the two
    // receivers: it would change the input it hears.
    void a_receiver_on_another_adc_is_not_joined_data()
    {
        QTest::addColumn<bool>("fold");
        QTest::addColumn<bool>("split");
        QTest::newRow("P1 2-ADC, join, one ADC") << false << false;
        QTest::newRow("P1 2-ADC, join, slot 2 on ADC1") << false << true;
        QTest::newRow("P1 2-ADC, fold, one ADC") << true << false;
        QTest::newRow("P1 2-ADC, fold, slot 2 on ADC1") << true << true;
    }
    void a_receiver_on_another_adc_is_not_joined()
    {
        QFETCH(bool, fold);
        QFETCH(bool, split);
        static P1CodecStandard codec;
        const QString mac = QStringLiteral("00:1c:c0:a2:61:0a");
        const auto cleanup = clearP1Word(mac);
        RadioModel model;
        p1Station(model, HPSDRModel::ANAN7000D, 4, &codec, mac,
                  split ? QStringLiteral("0x10") : QString());
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        // fold: A alone on the lower receiver retunes next to B (B would come
        // down); join: A alone on the higher one tunes next to B.
        QVERIFY(fold ? twoReceiversOnOnePan(model, a, b)
                     : loneSliceOnTheHigherReceiver(model, a, b));
        QCOMPARE(model.ddcForStream(0), 0);
        QCOMPARE(model.ddcForStream(1), 2);
        QCOMPARE(model.receiveAdcForStream(0), 0);
        QCOMPARE(model.receiveAdcForStream(1), split ? 1 : 0);

        a->setFrequency(7268000.0);

        if (split) {
            QCOMPARE(a->streamIndex(), fold ? 0 : 1);
            QCOMPARE(b->streamIndex(), fold ? 1 : 0);
            QCOMPARE(model.streamAllocator().activeStreamCount(), 2);
        } else {
            QCOMPARE(a->streamIndex(), 0);
            QCOMPARE(b->streamIndex(), 0);
            QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
        }
    }

    // Third review round (N1). Protocol 1 Diversity moves the receivers onto
    // other frame slots, and the codec remaps the routing word with them so
    // each receiver keeps its ADC (P1CodecStandard::ddcAssignmentOrionClass);
    // the remapped word is what the radio is sent. A 7000D with slot 2 on
    // ADC1: C on stream 0, B on stream 1 (ADC1), A on stream 2 in Diversity,
    // D alone on stream 3. With A's stream on slots 0 and 1, B moves to slot
    // 3 and D to slot 4. Read off the un-remapped word, slots 3 and 4 were
    // both ADC0, so D joined B and moved to the RX2 input. With the default
    // word every receiver is on ADC0 and D joins.
    void a_diversity_remap_keeps_each_receivers_adc_data()
    {
        QTest::addColumn<bool>("split");
        QTest::newRow("P1 2-ADC Diversity, slot 2 on ADC1") << true;
        QTest::newRow("P1 2-ADC Diversity, one ADC") << false;
    }
    void a_diversity_remap_keeps_each_receivers_adc()
    {
        QFETCH(bool, split);
        static P1CodecStandard codec;
        const QString mac = QStringLiteral("00:1c:c0:a2:61:0b");
        const auto cleanup = clearP1Word(mac);
        RadioModel model;
        p1Station(model, HPSDRModel::ANAN7000D, 4, &codec, mac,
                  split ? QStringLiteral("0x10") : QString());
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        QCOMPARE(a->sliceIndex(), 0);   // the slice a legacy Diversity request may enable
        a->setFrequency(14200000.0);
        SliceModel* b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(b);
        b->setFrequency(7262000.0);
        SliceModel* c = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(c);
        c->setFrequency(14210000.0);
        QCOMPARE(c->streamIndex(), a->streamIndex());
        a->setFrequency(21200000.0);     // leaves C: a receiver of its own
        SliceModel* d = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(d);
        d->setFrequency(3870000.0);
        QCOMPARE(c->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);
        QCOMPARE(a->streamIndex(), 2);
        QCOMPARE(d->streamIndex(), 3);
        a->setDiversityEnabled(true);
        QVERIFY(a->diversityEnabled());
        QCOMPARE(model.ddcForStream(2), 0);
        QCOMPARE(model.ddcForStream(1), 3);
        QCOMPARE(model.ddcForStream(3), 4);
        QCOMPARE(model.receiveAdcForStream(1), split ? 1 : 0);
        QCOMPARE(model.receiveAdcForStream(3), 0);

        d->setFrequency(7268000.0);

        QCOMPARE(d->streamIndex(), split ? 3 : 1);
        QCOMPARE(model.streamAllocator().activeStreamCount(), split ? 4 : 3);
    }

    // Third review round (N2). One-ADC boards: the 10E (Hermes II class) and
    // the HL2 have one ADC whatever the routing word says, and their codecs
    // publish ADC0. A two-ADC word persisted for the MAC (4: slot 1 on ADC1)
    // must not turn the rejoin off.
    void a_one_adc_board_ignores_a_two_adc_word_data()
    {
        QTest::addColumn<int>("hpsdr");
        QTest::addColumn<bool>("fold");
        QTest::newRow("ANAN-10E, join") << int(HPSDRModel::ANAN10E) << false;
        QTest::newRow("ANAN-10E, fold") << int(HPSDRModel::ANAN10E) << true;
        QTest::newRow("HL2, join") << int(HPSDRModel::HERMESLITE) << false;
        QTest::newRow("HL2, fold") << int(HPSDRModel::HERMESLITE) << true;
    }
    void a_one_adc_board_ignores_a_two_adc_word()
    {
        QFETCH(int, hpsdr);
        QFETCH(bool, fold);
        static P1CodecStandard standard;
        static P1CodecHl2 hl2;
        const bool isHl2 = hpsdr == int(HPSDRModel::HERMESLITE);
        const QString mac = QStringLiteral("00:1c:c0:a2:61:0c");
        const auto cleanup = clearP1Word(mac);
        RadioModel model;
        p1Station(model, HPSDRModel(hpsdr), 2,
                  isHl2 ? static_cast<IP1Codec*>(&hl2) : static_cast<IP1Codec*>(&standard),
                  mac, QStringLiteral("4"));
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(fold ? twoReceiversOnOnePan(model, a, b)
                     : loneSliceOnTheHigherReceiver(model, a, b));
        QCOMPARE(model.ddcForStream(1), 1);
        QCOMPARE(model.receiveAdcForStream(0), 0);
        QCOMPARE(model.receiveAdcForStream(1), 0);

        a->setFrequency(7268000.0);

        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // Third review round (N3). JJ's ANAN-G2E on Protocol 2 runs
    // P2CodecHermes (P2RadioConnection selects it for HermesC10); the rejoin
    // works there both ways round.
    void the_g2e_codec_rejoins_and_folds_data()
    {
        QTest::addColumn<bool>("fold");
        QTest::newRow("G2E, join") << false;
        QTest::newRow("G2E, fold") << true;
    }
    void the_g2e_codec_rejoins_and_folds()
    {
        QFETCH(bool, fold);
        static P2CodecHermes codec;
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 5, 192000);
        model.receiverManager()->setP2Codec(&codec);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(fold ? twoReceiversOnOnePan(model, a, b)
                     : loneSliceOnTheHigherReceiver(model, a, b));
        QVERIFY(model.ddcForStream(0) >= 0);
        QVERIFY(model.ddcForStream(1) >= 0);
        QCOMPARE(model.receiveAdcForStream(0), 0);
        QCOMPARE(model.receiveAdcForStream(1), 0);

        a->setFrequency(7268000.0);

        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // Third review round (N3). No codec has answered, so no receiver has a
    // known DDC or ADC: nothing rejoins or folds, either way round.
    void no_codec_means_no_rejoin_data()
    {
        QTest::addColumn<bool>("fold");
        QTest::newRow("no codec, join") << false;
        QTest::newRow("no codec, fold") << true;
    }
    void no_codec_means_no_rejoin()
    {
        QFETCH(bool, fold);
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(fold ? twoReceiversOnOnePan(model, a, b)
                     : loneSliceOnTheHigherReceiver(model, a, b));
        QCOMPARE(model.receiveAdcForStream(0), -1);
        QCOMPARE(model.receiveAdcForStream(1), -1);

        a->setFrequency(7268000.0);

        QCOMPARE(a->streamIndex(), fold ? 0 : 1);
        QCOMPARE(b->streamIndex(), fold ? 1 : 0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);
    }

    // Third review round, controller ruling: the slices that move take the
    // receiver's rate, so neither a join nor a fold happens between two
    // receivers at different rates. Once the rates match, it does.
    void a_receiver_at_another_rate_is_not_joined_data()
    {
        QTest::addColumn<bool>("fold");
        QTest::newRow("join") << false;
        QTest::newRow("fold") << true;
    }
    void a_receiver_at_another_rate_is_not_joined()
    {
        QFETCH(bool, fold);
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(fold ? twoReceiversOnOnePan(model, a, b)
                     : loneSliceOnTheHigherReceiver(model, a, b));
        model.requestSliceSampleRate(b->sliceIndex(), 96000);
        QCOMPARE(model.streamAllocator().streamSampleRateHz(b->streamIndex()), 96000);
        QCOMPARE(model.streamAllocator().streamSampleRateHz(a->streamIndex()), 192000);
        const int aStream = a->streamIndex();
        const int bStream = b->streamIndex();

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), aStream);
        QCOMPARE(b->streamIndex(), bStream);
        QCOMPARE(b->sampleRateHz(), 96000);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);

        model.requestSliceSampleRate(b->sliceIndex(), 192000);
        a->setFrequency(7269000.0);
        QCOMPARE(a->streamIndex(), b->streamIndex());
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // A fold moves the higher receiver's slices onto the lone slice's, where
    // they would adopt its noise-blanker state. A slice whose blanker
    // differs therefore stays where it is; once it matches, it folds.
    void a_fold_needs_the_moving_slices_blanker_state_data()
    {
        QTest::addColumn<QString>("field");
        QTest::newRow("NB mode") << QStringLiteral("nbMode");
        QTest::newRow("NB1 threshold") << QStringLiteral("threshold");
        QTest::newRow("NB1 transition") << QStringLiteral("transition");
        QTest::newRow("NB1 lead") << QStringLiteral("lead");
        QTest::newRow("NB1 lag") << QStringLiteral("lag");
        QTest::newRow("NB2 mode") << QStringLiteral("nb2Mode");
    }
    void a_fold_needs_the_moving_slices_blanker_state()
    {
        QFETCH(QString, field);
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        const auto setOn = [&field](SliceModel* s, bool changed) {
            if (field == QLatin1String("nbMode")) {
                s->setNbMode(changed ? NbMode::NB2 : NbMode::Off);
            } else if (field == QLatin1String("threshold")) {
                s->setNb1Threshold(changed ? 45 : 30);
            } else if (field == QLatin1String("transition")) {
                s->setNb1TransitionMs(changed ? 0.05 : 0.01);
            } else if (field == QLatin1String("lead")) {
                s->setNb1LeadMs(changed ? 0.05 : 0.01);
            } else if (field == QLatin1String("lag")) {
                s->setNb1LagMs(changed ? 0.05 : 0.01);
            } else {
                s->setNb2Mode(changed ? 1 : 0);
            }
        };
        setOn(b, true);
        const NbMode bMode = b->nbMode();
        const int bThreshold = b->nb1Threshold();
        const double bTransition = b->nb1TransitionMs();
        const double bLead = b->nb1LeadMs();
        const double bLag = b->nb1LagMs();
        const int bNb2 = b->nb2Mode();

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);
        // B was not touched.
        QCOMPARE(b->nbMode(), bMode);
        QCOMPARE(b->nb1Threshold(), bThreshold);
        QCOMPARE(b->nb1TransitionMs(), bTransition);
        QCOMPARE(b->nb1LeadMs(), bLead);
        QCOMPARE(b->nb1LagMs(), bLag);
        QCOMPARE(b->nb2Mode(), bNb2);

        // Once A's blanker matches B's, the same retune folds.
        setOn(a, true);
        a->setFrequency(7269000.0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // A fold moves every slice on the higher receiver, so each must be the
    // lone slice's device's, not only the one anchoring that receiver.
    void a_fold_does_not_move_another_devices_slice()
    {
        RadioModel model;
        seed(model);
        SliceOwnership* owners = model.sliceOwnership();
        QVERIFY(owners);
        const QByteArray phone("phone-device");
        const QByteArray tablet("tablet-device");
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        SliceModel* c = nullptr;
        {
            SliceOwnership::CreatorScope creator(owners, tablet);
            a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
            QVERIFY(a);
            a->setFrequency(3870000.0);
            b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
            QVERIFY(b);
            b->setFrequency(7262000.0);
            c = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
            QVERIFY(c);
            c->setFrequency(7264000.0);
        }
        // A new slice of another device gets a receiver of its own, so a
        // receiver carries two devices' slices only after a hand-over.
        owners->setOwner(c->sliceIndex(), phone);
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);
        QCOMPARE(c->streamIndex(), 1);
        QCOMPARE(owners->anchorOf(1), tablet);

        a->setFrequency(7268000.0);
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);
        QCOMPARE(c->streamIndex(), 1);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);

        // With every slice there the tablet's, the same retune folds.
        owners->setOwner(c->sliceIndex(), tablet);
        a->setFrequency(7269000.0);
        QCOMPARE(b->streamIndex(), 0);
        QCOMPARE(c->streamIndex(), 0);
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }

    // The station-wide C-Tune lock applies to every receiver alike, so it
    // cannot make a pinned receiver and an unpinned one match: the slices
    // that would move would change pin.
    void the_station_wide_lock_does_not_make_two_pins_match()
    {
        RadioModel model;
        seed(model);
        SliceModel* a = nullptr;
        SliceModel* b = nullptr;
        QVERIFY(twoReceiversOnOnePan(model, a, b));
        QVERIFY(model.requestStreamCtunPinned(a->sliceIndex(), true));
        model.receiverManager()->setDdcFrequencyLocked(true);

        a->setFrequency(7268000.0);          // out of A's window; B unpinned
        QCOMPARE(a->streamIndex(), 0);
        QCOMPARE(b->streamIndex(), 1);
        QVERIFY(!b->streamCtunPinned());
        QCOMPARE(model.streamAllocator().activeStreamCount(), 2);
    }

    // Restore: two slices of one pan inside one window share one receiver
    // (bindUnboundSlices places a later slice on a covering window). A guard
    // on existing behaviour: this already holds without the rejoin.
    void restored_slices_inside_one_window_share_one_receiver()
    {
        RadioModel model;                     // pool comes later, as at start
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setFrequency(7262000.0);
        SliceModel* b = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(b);
        b->setFrequency(7268000.0);

        seed(model);
        model.bindUnboundSlices();

        QVERIFY(a->streamIndex() >= 0);
        QCOMPARE(b->streamIndex(), a->streamIndex());
        QCOMPARE(model.streamAllocator().activeStreamCount(), 1);
    }
};

QTEST_MAIN(TestNewPanIsItsOwnReceiver)
#include "tst_new_pan_is_its_own_receiver.moc"
