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
//
// Modification history (NereusSDR):
//   2026-10-15 - The band-restore case flushes the setup's held per-band
//                save before seeding its 20m fixture. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QStandardPaths>

#include "core/AppSettings.h"
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

    // The direct-setter cases drive the property change performed by restore.
    // The band-button integration case also uses AppSettings in the normal
    // TestSandboxInit/test-home sandbox, checked before its first write.

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

    void a_band_change_leaves_the_rate_alone()
    {
        // Exercise the real settings/restore ordering with a full pool. The
        // 20m window hosts two slices 200 kHz apart.
#ifdef Q_OS_WIN
        // Windows tests get no NEREUS_TEST_HOME (its known folders ignore a
        // changed home), so check Qt's test-mode location instead.
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(AppSettings::instance().filePath().startsWith(
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QLatin1Char('/')));
#else
        QVERIFY(!qEnvironmentVariableIsEmpty("NEREUS_TEST_HOME"));
        QVERIFY(AppSettings::instance().filePath().startsWith(
            qEnvironmentVariable("NEREUS_TEST_HOME") + QLatin1Char('/')));
#endif
        auto& settings = AppSettings::instance();
        const QString prefix = QStringLiteral("Slice0/Band20m/");
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 3, 192000);
        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);
        slice->setFrequency(14200000.0);
        QVERIFY(model.setStreamSampleRate(0, 768000));
        QVERIFY(model.waitForReceiveLaneForTest());
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14400000.0);
        const int c = model.addSlice();
        model.sliceById(c)->setFrequency(7100000.0);
        slice->setFrequency(7150000.0);
        QCOMPARE(slice->streamIndex(), 1);
        model.setActiveSlice(a);
        // Setup tuned slice a through 20m at 768 kHz. RadioModel holds that
        // band's values until its coalesced save and writes them at the next
        // band click, so flush the setup save before seeding: otherwise the
        // held 20m capture replaces the fixture below.
        model.flushPendingSettingsSave();
        // DspMode is hasSettingsFor's visited-band sentinel. Seed after
        // setup so any rate-change persistence cannot replace this fixture.
        settings.setValue(prefix + QStringLiteral("DspMode"), static_cast<int>(DSPMode::USB));
        settings.setValue(prefix + QStringLiteral("Frequency"), 14200000.0);
        settings.setValue(prefix + QStringLiteral("SampleRate"), 192000);
        QVERIFY(slice->hasSettingsFor(Band::Band20m));

        // JJ's ruling, 2026-11-05: the rate stays what was set, on every band.
        // A value an older build saved for this band does not move anything.
        QSignalSpy rejected(&model, &RadioModel::sliceRetuneRejected);
        model.onBandButtonClicked(Band::Band20m);
        QVERIFY(model.waitForReceiveLaneForTest());

        QCOMPARE(rejected.count(), 0);
        QCOMPARE(slice->frequency(), 14200000.0);
        QCOMPARE(slice->streamIndex(), 0);
        QCOMPARE(slice->sampleRateHz(), 768000);
        QCOMPARE(model.sliceById(b)->sampleRateHz(), 768000);
        // Slice c left slice a's shared receiver on its way to 40m and kept its rate.
        QCOMPARE(model.sliceById(c)->sampleRateHz(), 768000);
        QCOMPARE(model.streamSampleRateHzForTest(0), 768000);
        QCOMPARE(model.streamSampleRateHzForTest(1), 768000);
        QCOMPARE(model.activeStreamCount(), 2);
    }

    void a_reset_observer_cannot_apply_the_preference_to_a_replacement_slice()
    {
        RadioModel model;
        seed(model);
        const int id = model.addSlice();
        QVERIFY(model.addSlice() >= 0); // Keep a survivor so the original can close.
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setSampleRateHz(384000);
        bool replaced = false;
        connect(slice, &SliceModel::sampleRateHzChanged, &model, [&](int rate) {
            if (rate == 192000 && !replaced) {
                replaced = true;
                model.removeSlice(id);
                QCOMPARE(model.addSlice(), id);
            }
        });

        model.applyRestoredSampleRate(slice);
        QVERIFY(model.waitForReceiveLaneForTest());

        QVERIFY(replaced);
        SliceModel* replacement = model.sliceById(id);
        QVERIFY(replacement);
        QVERIFY(replacement != slice);
        QCOMPARE(replacement->sampleRateHz(), 192000);
        QCOMPARE(model.streamSampleRateHzForTest(replacement->streamIndex()), 192000);
    }

    void a_reset_observer_cannot_apply_the_preference_to_a_new_binding()
    {
        RadioModel model;
        seed(model);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14200000.0);
        const int cohost = model.addSlice();
        model.sliceById(cohost)->setFrequency(14250000.0);
        const int other = model.addSlice();
        model.sliceById(other)->setFrequency(7100000.0);
        const int originalStream = slice->streamIndex();
        const int destinationStream = model.sliceById(other)->streamIndex();
        QVERIFY(originalStream != destinationStream);
        QCOMPARE(model.sliceById(cohost)->streamIndex(), originalStream);
        slice->setSampleRateHz(384000);
        bool rebound = false;
        connect(slice, &SliceModel::sampleRateHzChanged, &model, [&](int rate) {
            if (rate == 192000 && !rebound) {
                rebound = true;
                slice->setFrequency(7150000.0);
            }
        });

        model.applyRestoredSampleRate(slice);
        QVERIFY(model.waitForReceiveLaneForTest());

        QVERIFY(rebound);
        QCOMPARE(slice->frequency(), 7150000.0);
        QCOMPARE(slice->streamIndex(), destinationStream);
        QCOMPARE(slice->sampleRateHz(), 192000);
        QCOMPARE(model.streamSampleRateHzForTest(destinationStream), 192000);
        QCOMPARE(model.sliceById(other)->sampleRateHz(), 192000);
        QCOMPARE(model.sliceById(cohost)->sampleRateHz(), 192000);
    }

    void a_reset_observers_same_stream_tune_supersedes_the_restored_preference_data()
    {
        QTest::addColumn<double>("fromHz");
        QTest::addColumn<double>("toHz");
        QTest::addColumn<Band>("destinationBand");
        QTest::newRow("within-band") << 14200000.0 << 14201000.0 << Band::Band20m;
        QTest::newRow("20m-to-GEN") << 14349000.0 << 14351000.0 << Band::GEN;
    }

    void a_reset_observers_same_stream_tune_supersedes_the_restored_preference()
    {
        QFETCH(double, fromHz);
        QFETCH(double, toHz);
        QFETCH(Band, destinationBand);
        RadioModel model;
        seed(model);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(fromHz);
        const int stream = slice->streamIndex();
        slice->setSampleRateHz(384000);
        bool retuned = false;
        connect(slice, &SliceModel::sampleRateHzChanged, &model, [&](int rate) {
            if (rate == 192000 && !retuned) {
                retuned = true;
                slice->setFrequency(toHz);
            }
        });
        QSignalSpy rejected(&model, &RadioModel::sliceRetuneRejected);

        model.applyRestoredSampleRate(slice);
        QVERIFY(model.waitForReceiveLaneForTest());

        QVERIFY(retuned);
        QCOMPARE(slice->frequency(), toHz);
        QCOMPARE(slice->band(), destinationBand);
        QCOMPARE(slice->streamIndex(), stream);
        QCOMPARE(model.streamSampleRateHzForTest(stream), 192000);
        QCOMPARE(slice->sampleRateHz(), 192000);
        QCOMPARE(rejected.count(), 0);
    }

    void a_reset_observers_new_rate_request_supersedes_the_restored_preference()
    {
        RadioModel model;
        seed(model);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setSampleRateHz(384000);
        bool superseded = false;
        connect(slice, &SliceModel::sampleRateHzChanged, &model, [&](int rate) {
            if (rate == 192000 && !superseded) {
                superseded = true;
                model.requestSliceSampleRate(id, 768000);
            }
        });

        model.applyRestoredSampleRate(slice);
        QVERIFY(model.waitForReceiveLaneForTest());

        QVERIFY(superseded);
        QCOMPARE(slice->sampleRateHz(), 768000);
        QCOMPARE(model.streamSampleRateHzForTest(slice->streamIndex()), 768000);
    }

    void a_remote_restore_uses_the_station_request_with_no_local_allocator()
    {
        RadioModel model(RadioModel::Role::Remote);
        const int id = model.addSliceWithStationId(7);
        QCOMPARE(id, 7);
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setStreamIndex(0); // Mirrored station binding; local pool stays empty.
        slice->setSampleRateHz(384000);
        QVERIFY(model.streamSampleRateHzForTest(0) <= 0);
        QSignalSpy rejected(&model, &RadioModel::sliceRetuneRejected);

        model.applyRestoredSampleRate(slice);

        // No station is attached, so the normal remote forwarding path
        // reports its refusal instead of treating the empty pool as a rate.
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.first().at(0).toInt(), id);
        QCOMPARE(slice->sampleRateHz(), 384000);
        QVERIFY(model.streamSampleRateHzForTest(0) <= 0);
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
