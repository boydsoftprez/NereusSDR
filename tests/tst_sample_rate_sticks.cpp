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
#include "core/P1RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/session/ReceiverPlanner.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kStartRateHz = 192000;
constexpr int kChosenRateHz = 1536000;
const QString kRadioMac = QStringLiteral("AA:BB:CC:DD:EE:51");
const QString kOtherRadioMac = QStringLiteral("AA:BB:CC:DD:EE:52");

QString rateKey(const QString& mac, int sliceId)
{
    return QStringLiteral("hardware/%1/slices/%2/SampleRate")
        .arg(AppSettings::normalizedRadioMac(mac))
        .arg(sliceId);
}

QString radioWideRateKey(const QString& mac)
{
    return QStringLiteral("hardware/%1/radioInfo/sampleRate")
        .arg(AppSettings::normalizedRadioMac(mac));
}

// Leaves the model with no connection before the connection on the stack goes.
struct DetachConnection {
    RadioModel* model;
    ~DetachConnection() { model->injectConnectionForTest(nullptr); }
};

} // namespace

class TestSampleRateSticks : public QObject
{
    Q_OBJECT

private:
    static void identify(RadioModel& model, const QString& mac = kRadioMac)
    {
        RadioInfo info;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
    }

    static void seed(RadioModel& model)
    {
        identify(model);
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, kStartRateHz);
    }

    // A Protocol 1 radio whose rate change can really happen: WDSP counts as up,
    // so nothing but the guard under test stops a change reaching the wire.
    static void seedOnProtocol1(RadioModel& model, P1RadioConnection& connection)
    {
        identify(model);
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        model.injectConnectionForTest(&connection);
        model.wdspEngine()->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        connection.restartStreamWithRate(kStartRateHz);
        model.setConnectionRateForTest(kStartRateHz, 1);
        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 2, kStartRateHz);
    }

    static void settleRateChange(RadioModel& model)
    {
        QVERIFY(model.waitForReceiveLaneForTest());
        // The wire write is queued to the connection, which is on this thread.
        QCoreApplication::processEvents();
    }

    // Makes slice `id` again from its saved keys, as a Core does for a device
    // coming back. Returns the restored slice.
    static SliceModel* makeAgain(RadioModel& model, int id, const QString& panKey,
                                 double frequencyHz)
    {
        ReceiveSliceState saved;
        saved.id = id;
        saved.panKey = panKey;
        saved.frequencyHz = frequencyHz;
        saved.dspMode = DSPMode::USB;
        QString reason;
        const int restored = model.restoreSliceFor(QByteArrayLiteral("phone"), id, saved, &reason);
        if (restored != id) {
            qWarning() << "restoreSliceFor refused:" << reason;
            return nullptr;
        }
        return model.sliceById(restored);
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

        // A value saved under the radio's own key wins from then on.
        settings.setValue(rateKey(kRadioMac, 0), 384000);
        QCOMPARE(model.sliceById(a)->savedSampleRateHz(), 384000);
    }
    // Protocol 1 has one rate for the whole radio. A slice made again must not
    // move it for every other receiver, nor rewrite the radio's saved rate, and
    // it shows the rate it is really running.
    void on_protocol1_a_slice_made_again_does_not_move_the_radio()
    {
        RadioModel model;
        P1RadioConnection connection;
        seedOnProtocol1(model, connection);
        DetachConnection detach{&model};
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        const int streamA = model.sliceById(a)->streamIndex();
        const quint8 wireBefore = static_cast<quint8>(connection.captureBank0ForTest().at(1));

        auto& settings = AppSettings::instance();
        settings.setValue(rateKey(kRadioMac, 1), 384000);
        settings.setValue(radioWideRateKey(kRadioMac), kStartRateHz);
        settings.setValue(QStringLiteral("Slice1/LastBand"), bandKeyName(Band::Band40m));

        QSignalSpy rejected(&model, &RadioModel::sliceRetuneRejected);
        SliceModel* again = makeAgain(model, 1, QStringLiteral("pan-1"), 7100000.0);
        QVERIFY(again != nullptr);
        QVERIFY2(again->streamIndex() != streamA,
                 "precondition: the slice made again claimed a receiver of its own");
        settleRateChange(model);

        QCOMPARE(model.streamSampleRateHzForTest(streamA), kStartRateHz);
        QCOMPARE(model.streamSampleRateHzForTest(again->streamIndex()), kStartRateHz);
        QCOMPARE(again->sampleRateHz(), kStartRateHz);
        QCOMPARE(static_cast<quint8>(connection.captureBank0ForTest().at(1)), wireBefore);
        QCOMPARE(settings.value(radioWideRateKey(kRadioMac)).toInt(), kStartRateHz);
        QCOMPARE(rejected.count(), 0);
    }

    // The same for a new slice seeded from another: the radio stays where it is
    // and the slice shows the rate of the receiver it got.
    void on_protocol1_a_new_slice_does_not_move_the_radio()
    {
        RadioModel model;
        P1RadioConnection connection;
        seedOnProtocol1(model, connection);
        DetachConnection detach{&model};
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        const quint8 wireBefore = static_cast<quint8>(connection.captureBank0ForTest().at(1));
        // The next receiver claimed opens at a rate unlike the seed's.
        model.setConnectionRateForTest(96000, 1);

        QSignalSpy rejected(&model, &RadioModel::sliceRetuneRejected);
        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(b >= 0);
        SliceModel* sliceB = model.sliceById(b);
        settleRateChange(model);

        QCOMPARE(model.streamSampleRateHzForTest(model.sliceById(a)->streamIndex()), kStartRateHz);
        QCOMPARE(model.streamSampleRateHzForTest(sliceB->streamIndex()), 96000);
        QCOMPARE(sliceB->sampleRateHz(), 96000);
        QCOMPARE(static_cast<quint8>(connection.captureBank0ForTest().at(1)), wireBefore);
        QCOMPARE(rejected.count(), 0);
    }

    // A rate the radio does not have is not asked for: the receiver stays, and
    // the slice shows what the receiver runs. 200 kHz is on no board's ladder.
    void a_slice_made_again_at_a_rate_the_radio_lacks_stays_put()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);

        auto& settings = AppSettings::instance();
        settings.setValue(rateKey(kRadioMac, 1), 200000);
        settings.setValue(QStringLiteral("Slice1/LastBand"), bandKeyName(Band::Band40m));

        SliceModel* again = makeAgain(model, 1, QStringLiteral("pan-1"), 7100000.0);
        QVERIFY(again != nullptr);
        QVERIFY(again->streamIndex() >= 0);
        QCOMPARE(model.streamSampleRateHzForTest(again->streamIndex()), kStartRateHz);
        QCOMPARE(again->sampleRateHz(), kStartRateHz);
    }

    void a_new_slice_seeded_with_a_rate_the_radio_lacks_stays_put()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        model.requestSliceSampleRate(a, 200000);
        QCOMPARE(model.streamSampleRateHzForTest(model.sliceById(a)->streamIndex()), 200000);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(b >= 0);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB->streamIndex() != model.sliceById(a)->streamIndex());
        QCOMPARE(model.streamSampleRateHzForTest(sliceB->streamIndex()), kStartRateHz);
        QCOMPARE(sliceB->sampleRateHz(), kStartRateHz);
    }
    // One computer, several radios: each keeps its own rate on the same letter.
    void two_radios_keep_separate_rates_on_one_letter()
    {
        SliceModel onFirst;
        onFirst.setSliceIndex(0);
        onFirst.setSettingsRadioIdentity(kRadioMac);
        onFirst.setSampleRateHz(kChosenRateHz);
        onFirst.saveToSettings(Band::Band20m);

        SliceModel onSecond;
        onSecond.setSliceIndex(0);
        onSecond.setSettingsRadioIdentity(kOtherRadioMac);
        onSecond.setSampleRateHz(kStartRateHz);
        onSecond.saveToSettings(Band::Band20m);

        QCOMPARE(onFirst.savedSampleRateHz(), kChosenRateHz);
        QCOMPARE(onSecond.savedSampleRateHz(), kStartRateHz);
        auto& settings = AppSettings::instance();
        QCOMPARE(settings.value(rateKey(kRadioMac, 0)).toInt(), kChosenRateHz);
        QCOMPARE(settings.value(rateKey(kOtherRadioMac, 0)).toInt(), kStartRateHz);
        QVERIFY(!settings.contains(QStringLiteral("Slice0/SampleRate")));
    }

    // With no radio known the rate has nowhere to go, so it is not saved, and
    // only a rate from an older build can be read.
    void with_no_radio_known_the_rate_is_not_saved()
    {
        SliceModel slice;
        slice.setSliceIndex(0);
        slice.setSampleRateHz(kChosenRateHz);
        slice.saveToSettings(Band::Band20m);

        auto& settings = AppSettings::instance();
        QVERIFY(!settings.contains(QStringLiteral("Slice0/SampleRate")));
        QVERIFY(!settings.contains(QStringLiteral("Slice0/Band20m/SampleRate")));
        QCOMPARE(slice.savedSampleRateHz(), 0);

        settings.setValue(QStringLiteral("Slice0/Band20m/SampleRate"), 384000);
        QCOMPARE(slice.savedSampleRateHz(), 384000);
        settings.setValue(rateKey(kRadioMac, 0), kChosenRateHz);
        QCOMPARE(slice.savedSampleRateHz(), 384000);
    }
    // Two slices on one receiver have one rate between them. The highest saved
    // wins: the rate the operator chose must not lose to an old default.
    void slices_sharing_a_receiver_come_back_at_the_highest_saved_rate_data()
    {
        QTest::addColumn<int>("rateA");
        QTest::addColumn<int>("rateB");
        QTest::newRow("A chose it") << kChosenRateHz << kStartRateHz;
        QTest::newRow("B chose it") << kStartRateHz << kChosenRateHz;
    }

    void slices_sharing_a_receiver_come_back_at_the_highest_saved_rate()
    {
        QFETCH(int, rateA);
        QFETCH(int, rateB);
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        QVERIFY(b >= 0);
        model.sliceById(b)->setFrequency(14210000.0);
        const int shared = model.sliceById(a)->streamIndex();
        QCOMPARE(model.sliceById(b)->streamIndex(), shared);
        model.flushPendingSettingsSave();

        auto& settings = AppSettings::instance();
        settings.setValue(rateKey(kRadioMac, a), rateA);
        settings.setValue(rateKey(kRadioMac, b), rateB);

        model.applySavedSliceSampleRates(model.savedSliceSampleRates(),
                                         ProtocolVersion::Protocol2);
        QVERIFY(model.waitForReceiveLaneForTest());

        QCOMPARE(model.streamSampleRateHzForTest(shared), kChosenRateHz);
        QCOMPARE(model.sliceById(a)->sampleRateHz(), kChosenRateHz);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), kChosenRateHz);
        model.flushPendingSettingsSave();
        QCOMPARE(model.sliceById(a)->savedSampleRateHz(), kChosenRateHz);
        QCOMPARE(model.sliceById(b)->savedSampleRateHz(), kChosenRateHz);
    }

    // Receivers of their own keep their own rates: one rate per receiver, not
    // one for the radio.
    void slices_on_their_own_receivers_come_back_at_their_own_rates()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice(QStringLiteral("pan-1"));
        QVERIFY(b >= 0);
        model.sliceById(b)->setFrequency(7100000.0);
        QVERIFY(model.sliceById(a)->streamIndex() != model.sliceById(b)->streamIndex());

        auto& settings = AppSettings::instance();
        settings.setValue(rateKey(kRadioMac, a), kChosenRateHz);
        settings.setValue(rateKey(kRadioMac, b), 384000);
        model.applySavedSliceSampleRates(model.savedSliceSampleRates(),
                                         ProtocolVersion::Protocol2);
        QVERIFY(model.waitForReceiveLaneForTest());

        QCOMPARE(model.sliceById(a)->sampleRateHz(), kChosenRateHz);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), 384000);
    }

    // A slice pushed off a receiver the connect is narrowing still gets its
    // saved rate on the receiver it lands on.
    void a_slice_moved_by_a_saved_rate_gets_its_own_saved_rate_too()
    {
        RadioModel model;
        seed(model);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        QVERIFY(b >= 0);
        model.requestSliceSampleRate(a, kChosenRateHz);
        model.sliceById(b)->setFrequency(14500000.0);
        QCOMPARE(model.sliceById(b)->streamIndex(), model.sliceById(a)->streamIndex());

        // Both saved lower than the receiver runs: the higher applies, and the
        // slice it no longer covers moves, then takes its own.
        QHash<int, int> saved;
        saved.insert(a, 384000);
        saved.insert(b, 96000);
        model.applySavedSliceSampleRates(saved, ProtocolVersion::Protocol2);
        QVERIFY(model.waitForReceiveLaneForTest());

        QVERIFY(model.sliceById(a)->streamIndex() != model.sliceById(b)->streamIndex());
        QCOMPARE(model.sliceById(a)->sampleRateHz(), 384000);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), 96000);
    }
};

QTEST_MAIN(TestSampleRateSticks)
#include "tst_sample_rate_sticks.moc"
