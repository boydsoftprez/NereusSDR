// =================================================================
// tests/tst_sample_rate_sticks.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. A receiver with its own rate on Protocol 2
// and a device's slices made again on a Core have no Thetis equivalent: Thetis
// has one console, and only Setup changes its rate (setup.cs:7066 [v2.10.3.15]).
//
// J.J. Boyd (KG4VCF) keeps having to set the sample rate back to 1536 kHz when
// he opens the application or opens a new slice. These are the ways the rate
// was being lost, each from the operator's side of it.
//
// Modification history (NereusSDR):
//   2026-11-05 - Written by J.J. Boyd (KG4VCF), with AI assistance from
//                Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kStartRateHz = 192000;
constexpr int kChosenRateHz = 1536000;

} // namespace

class TestSampleRateSticks : public QObject
{
    Q_OBJECT

private:
    static void seed(RadioModel& model)
    {
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, kStartRateHz);
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
    }

    // Set 1536 kHz on the receiver you are using, then open a new pan. It gets
    // a receiver of its own, and that receiver has to open at the rate in use.
    void a_new_slice_on_its_own_receiver_opens_at_the_rate_in_use()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.requestSliceSampleRate(a, kChosenRateHz);
        const int streamA = model.sliceById(a)->streamIndex();
        QCOMPARE(model.streamSampleRateHzForTest(streamA), kChosenRateHz);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(b >= 0);
        const int streamB = model.sliceById(b)->streamIndex();
        QVERIFY2(streamB >= 0 && streamB != streamA,
                 "precondition: a new pan claims a receiver of its own");

        QCOMPARE(model.streamSampleRateHzForTest(streamB), kChosenRateHz);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), kChosenRateHz);
    }

    // A window or phone comes back to a running Core, which makes its saved
    // slice again (StationServer's admission, RadioModel::restoreSliceFor).
    void a_devices_slice_made_again_comes_back_at_its_saved_rate()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(7100000.0);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(b >= 0);
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(14200000.0);
        model.requestSliceSampleRate(b, kChosenRateHz);
        sliceB->saveToSettings(bandFromFrequency(sliceB->frequency()));
        QCOMPARE(sliceB->savedSampleRateHz(), kChosenRateHz);

        ReceiveSliceState saved;
        saved.id = b;
        saved.panKey = sliceB->panKey();
        saved.frequencyHz = sliceB->frequency();
        saved.dspMode = sliceB->dspMode();
        model.removeSlice(b);
        QCoreApplication::processEvents();

        QString reason;
        const int restored =
            model.restoreSliceFor(QByteArrayLiteral("phone"), b, saved, &reason);
        QVERIFY2(restored == b, qPrintable(reason));
        SliceModel* again = model.sliceById(restored);
        QVERIFY(again != nullptr);

        QCOMPARE(model.streamSampleRateHzForTest(again->streamIndex()), kChosenRateHz);
        QCOMPARE(again->sampleRateHz(), kChosenRateHz);

        // And the Core's next save keeps the rate rather than writing the
        // receiver's opening rate over the one the operator chose.
        model.flushPendingSettingsSave();
        QCOMPARE(again->savedSampleRateHz(), kChosenRateHz);
    }

    // A slice made again onto a receiver another device is using takes that
    // receiver's rate. It must not move the other device's receiver.
    void a_slice_made_again_does_not_move_a_receiver_someone_else_is_on()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        SliceModel* sliceA = model.sliceById(a);
        sliceA->setFrequency(14200000.0);
        const int streamA = sliceA->streamIndex();

        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(b >= 0);
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(14210000.0);
        model.requestSliceSampleRate(b, kChosenRateHz);
        sliceB->saveToSettings(bandFromFrequency(sliceB->frequency()));

        ReceiveSliceState saved;
        saved.id = b;
        saved.panKey = sliceB->panKey();
        saved.frequencyHz = sliceB->frequency();
        saved.dspMode = sliceB->dspMode();
        model.removeSlice(b);
        QCoreApplication::processEvents();

        QString reason;
        const int restored =
            model.restoreSliceFor(QByteArrayLiteral("phone"), b, saved, &reason);
        QVERIFY2(restored == b, qPrintable(reason));
        SliceModel* again = model.sliceById(restored);
        QVERIFY(again != nullptr);
        QVERIFY2(again->streamIndex() == streamA,
                 "precondition: the restored slice joined the covering receiver");

        QCOMPARE(model.streamSampleRateHzForTest(streamA), kStartRateHz);
        QCOMPARE(again->sampleRateHz(), kStartRateHz);
    }

    // A slice with nothing saved for the band has no preference to apply: its
    // constructor's 192 kHz must not count as one.
    void a_slice_made_again_with_no_saved_rate_keeps_the_receiver_rate()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        model.requestSliceSampleRate(a, kChosenRateHz);
        const int streamA = model.sliceById(a)->streamIndex();

        ReceiveSliceState saved;
        saved.id = 3;
        saved.panKey = QStringLiteral("pan-0");
        saved.frequencyHz = 14210000.0;
        saved.dspMode = DSPMode::USB;

        QString reason;
        const int restored =
            model.restoreSliceFor(QByteArrayLiteral("phone"), 3, saved, &reason);
        QVERIFY2(restored == 3, qPrintable(reason));
        QVERIFY2(model.sliceById(3)->streamIndex() == streamA,
                 "precondition: the restored slice joined the covering receiver");
        QCOMPARE(model.streamSampleRateHzForTest(streamA), kChosenRateHz);
    }

    // JJ's ruling, 2026-11-05: a band change does not touch the rate, in either
    // direction, on a band visited before or not.
    void a_band_change_leaves_the_rate_alone()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        SliceModel* slice = model.sliceById(a);
        model.onBandButtonClicked(Band::Band40m);   // 40m has settings from here on
        model.onBandButtonClicked(Band::Band20m);
        model.requestSliceSampleRate(a, kChosenRateHz);

        model.onBandButtonClicked(Band::Band40m);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->band(), Band::Band40m);
        QCOMPARE(model.streamSampleRateHzForTest(slice->streamIndex()), kChosenRateHz);
        QCOMPARE(slice->sampleRateHz(), kChosenRateHz);

        model.flushPendingSettingsSave();
        QCOMPARE(slice->savedSampleRateHz(), kChosenRateHz);
    }

    // A slice sharing a receiver tunes outside it. It gets a receiver of its
    // own, at its own rate rather than the radio-wide connect rate.
    void a_slice_leaving_a_shared_receiver_keeps_its_rate()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        QVERIFY(b >= 0);
        SliceModel* sliceB = model.sliceById(b);
        const int shared = sliceB->streamIndex();
        QCOMPARE(model.sliceById(a)->streamIndex(), shared);
        model.requestSliceSampleRate(a, kChosenRateHz);
        QCOMPARE(sliceB->sampleRateHz(), kChosenRateHz);

        sliceB->setFrequency(7100000.0);

        QVERIFY2(sliceB->streamIndex() != shared,
                 "precondition: tuning outside the window claims another receiver");
        QCOMPARE(model.streamSampleRateHzForTest(sliceB->streamIndex()), kChosenRateHz);
        QCOMPARE(sliceB->sampleRateHz(), kChosenRateHz);
    }

    // Settings from before a receiver had one rate: it comes back at the rate
    // it had on the band it was last on.
    void a_rate_saved_per_band_by_an_older_build_is_still_read()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Slice0/LastBand"), bandKeyName(Band::Band40m));
        settings.setValue(QStringLiteral("Slice0/Band40m/SampleRate"), kChosenRateHz);
        settings.setValue(QStringLiteral("Slice0/Band20m/SampleRate"), kStartRateHz);

        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        QCOMPARE(model.sliceById(a)->savedSampleRateHz(), kChosenRateHz);

        settings.setValue(QStringLiteral("Slice0/SampleRate"), 384000);
        QCOMPARE(model.sliceById(a)->savedSampleRateHz(), 384000);
    }
};

QTEST_MAIN(TestSampleRateSticks)
#include "tst_sample_rate_sticks.moc"
