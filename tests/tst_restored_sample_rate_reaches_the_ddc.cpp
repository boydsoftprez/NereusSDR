// =================================================================
// tests/tst_restored_sample_rate_reaches_the_ddc.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Per-slice per-band sample rate is a
// Phase 3F concept with no Thetis equivalent: Thetis carries one rate for
// the whole radio in C&C bank 0.
//
// Codex review round 7, PR #293.
//
// SliceModel::restoreFromSettings reads the persisted per-band SampleRate
// and calls setSampleRateHz, which is a plain property setter:
//
//     void SliceModel::setSampleRateHz(int hz)
//     {
//         if (m_sampleRateHz != hz) { m_sampleRateHz = hz;
//                                     emit sampleRateHzChanged(hz); }
//     }
//
// It moves the number the VFO menu shows and nothing else. The rate the
// receiver, codec and wire actually run at changes only through
// RadioModel::requestSliceSampleRate, which plans and commits a
// stream-wide transaction. So leaving 40 m at 384 kHz, working 20 m at
// 192 kHz, and coming back to 40 m displayed 384 while the DDC stayed at
// 192: the saved preference was never actually restored, and the display
// asserted otherwise.
//
// Another instance of the branch's recurring shape, this one inverted:
// here the SETTER was the thing that looked authoritative and was not.
// =================================================================

#include <QtTest/QtTest>

#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRestoredSampleRateReachesTheDdc : public QObject
{
    Q_OBJECT

private:
    /// A model with a real stream pool, so streamSampleRateHz is meaningful
    /// rather than reporting an unbound -1 for everything.
    static void seed(RadioModel& model)
    {
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
    }

    // No settings isolation needed: these cases drive setSampleRateHz
    // directly, which is exactly the restore step under test, rather than
    // round-tripping through a settings file. What is being asserted is what
    // happens AFTER the value is restored.

private slots:

    // ── 1. The restore reaches the stream ────────────────────────────────
    //
    // The finding. Save a band at a rate the stream is not on, restore it,
    // and the stream has to follow.
    void a_restored_rate_moves_the_stream_not_just_the_property()
    {
        RadioModel model;
        seed(model);

        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY2(stream >= 0, "precondition: the slice is bound to a stream");

        // Park the stream somewhere else, then hand the slice a different
        // restored preference.
        QCOMPARE(model.streamSampleRateHzForTest(stream), 192000);

        slice->setSampleRateHz(384000);
        model.applyRestoredSampleRate(slice);

        QVERIFY2(model.streamSampleRateHzForTest(stream) == 384000,
            "the restored rate must reach the DDC, not only the menu");
    }

    // ── 2. A no-op restore stays a no-op ─────────────────────────────────
    //
    // Most band changes do not change the rate. Running the whole rebind
    // transaction anyway would churn the stream and could emit a rejection
    // toast for a rate the radio is already on.
    void restoring_the_rate_the_stream_is_already_on_changes_nothing()
    {
        RadioModel model;
        seed(model);

        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);

        QSignalSpy rejected(&model, &RadioModel::sliceRetuneRejected);

        slice->setSampleRateHz(model.streamSampleRateHzForTest(stream));
        model.applyRestoredSampleRate(slice);

        QCOMPARE(model.streamSampleRateHzForTest(stream), 192000);
        QVERIFY2(rejected.count() == 0,
            "a restore to the rate already in force must not warn");
    }

    // ── 3. An unbound slice is left alone ────────────────────────────────
    //
    // A slice with no stream has no DDC whose width to change. It adopts
    // its stream's rate when it binds, so touching anything here would be
    // acting on a receiver that does not exist yet.
    void an_unbound_slice_is_not_pushed_anywhere()
    {
        RadioModel model;
        seed(model);

        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);

        slice->setStreamIndex(-1);
        slice->setSampleRateHz(768000);

        QSignalSpy rejected(&model, &RadioModel::sliceRetuneRejected);
        model.applyRestoredSampleRate(slice);   // must not crash or warn

        QCOMPARE(rejected.count(), 0);
    }

    // ── 4. Nonsense is ignored rather than pushed ────────────────────────
    //
    // A settings file predating the per-band SampleRate key, or one that
    // was hand-edited, can hand back 0. Forwarding that into the rate
    // transaction would ask the allocator for a zero-width DDC.
    void a_zero_or_negative_restored_rate_is_ignored()
    {
        RadioModel model;
        seed(model);

        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);
        const int before = model.streamSampleRateHzForTest(stream);

        slice->setSampleRateHz(0);
        model.applyRestoredSampleRate(slice);

        QCOMPARE(model.streamSampleRateHzForTest(stream), before);
    }

    // ── 5. A null slice is survivable ────────────────────────────────────
    //
    // Both call sites resolve the slice from a container first, but they
    // are two of them and a third will appear.
    void a_null_slice_is_survivable()
    {
        RadioModel model;
        seed(model);
        model.applyRestoredSampleRate(nullptr);   // must not crash
        QVERIFY(true);
    }
};

QTEST_MAIN(TestRestoredSampleRateReachesTheDdc)
#include "tst_restored_sample_rate_reaches_the_ddc.moc"
