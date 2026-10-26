// tst_radio_model_band_click.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here.
//
// Unit tests for RadioModel::onBandButtonClicked(Band) — the central
// band-button handler for issue #118. Exercises the three branches:
// same-band no-op, first-visit seed, second-visit restore. Also covers
// null-active-slice, XVTR, and locked-slice edge cases from the spec.
//
// The tests drive RadioModel directly (no live radio, no WDSP) and
// observe SliceModel state and AppSettings writes.
//
// Container item parity Task 35 (2026-10-26, J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code): a band grid click through the
// band groups reaches its own slice through ContainerButtonDispatcher.
// Fix round 1: the SWL bands tune with their Thetis seeds.

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/BandGroups.h"
#include "models/Band.h"
#include "models/BandDefaults.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRadioModelBandClick : public QObject {
    Q_OBJECT

private:
    static void clearAllBandKeys() {
        auto& s = AppSettings::instance();
        for (const QString& band : {
                 QStringLiteral("160m"), QStringLiteral("80m"),
                 QStringLiteral("60m"),  QStringLiteral("40m"),
                 QStringLiteral("30m"),  QStringLiteral("20m"),
                 QStringLiteral("17m"),  QStringLiteral("15m"),
                 QStringLiteral("12m"),  QStringLiteral("10m"),
                 QStringLiteral("6m"),   QStringLiteral("WWV"),
                 QStringLiteral("GEN"),  QStringLiteral("XVTR"),
                 QStringLiteral("2m"),   QStringLiteral("31m"),
                 QStringLiteral("22m") }) {
            const QString bp = QStringLiteral("Slice0/Band") + band + QStringLiteral("/");
            for (const QString& field : {
                     QStringLiteral("Frequency"),  QStringLiteral("DspMode"),
                     QStringLiteral("FilterLow"),  QStringLiteral("FilterHigh"),
                     QStringLiteral("AgcMode"),    QStringLiteral("StepHz") }) {
                s.remove(bp + field);
            }
        }
    }

private slots:
    void init() { clearAllBandKeys(); }
    void cleanup() { clearAllBandKeys(); }

    void first_visit_seeds_freq_and_mode() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        QVERIFY(slice);
        // Start on 40m so the 20m click is a genuine cross-band event.
        slice->setFrequency(7100000.0);
        slice->setDspMode(DSPMode::LSB);

        radio.onBandButtonClicked(Band::Band20m);

        QCOMPARE(slice->frequency(), 14155000.0);   // seed freq
        QCOMPARE(slice->dspMode(),   DSPMode::USB); // seed mode
    }

    void second_visit_restores_last_used() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);

        // Start on 80m — picked specifically so the snapshot-on-exit
        // during the first click does NOT prewrite the 40m slot we
        // later want to exercise as a first-visit seed. If we started
        // on 40m instead, step (3) below would restore the starting
        // state rather than apply the seed.
        slice->setFrequency(3700000.0);
        slice->setDspMode(DSPMode::LSB);

        // (1) First visit 20m: seeds 14.155 USB.
        radio.onBandButtonClicked(Band::Band20m);

        // (2) User tunes to 14.100 and switches to CWU.
        slice->setFrequency(14100000.0);
        slice->setDspMode(DSPMode::CWU);

        // (3) Jump to 40m — first visit, applies seed.
        radio.onBandButtonClicked(Band::Band40m);
        QCOMPARE(slice->frequency(), 7152000.0);   // 40m seed
        QCOMPARE(slice->dspMode(),   DSPMode::LSB);

        // (4) Jump back to 20m — should restore, NOT re-seed.
        radio.onBandButtonClicked(Band::Band20m);
        QCOMPARE(slice->frequency(), 14100000.0);
        QCOMPARE(slice->dspMode(),   DSPMode::CWU);
    }

    // Step persistence lane (2026-10-15), case (c)/(d): pins today's model.
    // The tune step is kept per slice per band (Slice<N>/Band<key>/StepHz):
    // a first visit keeps the step it arrived with, and each later visit
    // brings back that band's own step. Thetis keeps one step for the whole
    // console (wheel_tune_index, console.cs:3205 and 4016-4017
    // [v2.10.3.15]); whether NereusSDR should is the maintainer's call.
    void step_is_kept_per_band() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        QVERIFY(slice);
        slice->setFrequency(3700000.0);   // 80m, so 20m and 40m are first visits
        slice->setDspMode(DSPMode::LSB);

        radio.onBandButtonClicked(Band::Band20m);
        slice->setStepHz(1000);

        radio.onBandButtonClicked(Band::Band40m);
        QCOMPARE(slice->stepHz(), 1000);  // first visit keeps the step
        slice->setStepHz(10);

        radio.onBandButtonClicked(Band::Band20m);
        QCOMPARE(slice->stepHz(), 1000);
        radio.onBandButtonClicked(Band::Band40m);
        QCOMPARE(slice->stepHz(), 10);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Slice0/Band20m/StepHz")).toInt(), 1000);
    }

    void same_band_is_no_op() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        slice->setFrequency(14100000.0);
        slice->setDspMode(DSPMode::USB);

        const double  freqBefore = slice->frequency();
        const DSPMode modeBefore = slice->dspMode();

        // 14.100 is inside 20m; clicking 20m again is the reproducer
        // for the same-band no-op branch.
        radio.onBandButtonClicked(Band::Band20m);

        QCOMPARE(slice->frequency(), freqBefore);
        QCOMPARE(slice->dspMode(),   modeBefore);
    }

    void xvtr_emits_ignored_and_no_change() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        slice->setFrequency(14100000.0);   // on 20m
        slice->setDspMode(DSPMode::USB);

        QSignalSpy spy(&radio, &RadioModel::bandClickIgnored);
        radio.onBandButtonClicked(Band::XVTR);

        // Neither freq nor mode should change.
        QCOMPARE(slice->frequency(), 14100000.0);
        QCOMPARE(slice->dspMode(),   DSPMode::USB);

        // Signal fired so the user sees the transverter-config message.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).value<Band>(), Band::XVTR);
        const QString reason = spy.first().at(1).toString();
        QVERIFY2(reason.contains(QStringLiteral("transverter"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("reason should mention transverter; got: %1").arg(reason)));
    }

    void successful_seed_does_not_emit_ignored() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        slice->setFrequency(7100000.0);
        slice->setDspMode(DSPMode::LSB);

        QSignalSpy spy(&radio, &RadioModel::bandClickIgnored);
        radio.onBandButtonClicked(Band::Band20m);

        // Normal first-visit seed path — no ignored signal.
        QCOMPARE(spy.count(), 0);
    }

    void same_band_click_does_not_emit_ignored() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        slice->setFrequency(14100000.0);   // on 20m
        slice->setDspMode(DSPMode::USB);

        QSignalSpy spy(&radio, &RadioModel::bandClickIgnored);
        radio.onBandButtonClicked(Band::Band20m);

        // Same-band is silent-expected, not ignored-unexpected.
        QCOMPARE(spy.count(), 0);
    }

    void null_active_slice_is_safe() {
        RadioModel radio;
        // No slice added — activeSlice() returns nullptr.
        // Must not crash, must not emit any signals we can observe.
        radio.onBandButtonClicked(Band::Band20m);
        QVERIFY(radio.activeSlice() == nullptr);
    }

    void locked_slice_emits_ignored_and_leaves_state_intact() {
        // Updated for #118 review: earlier design let mode change on a
        // locked click but that corrupted the new band's persistence slot
        // (blocked setFrequency left stale freq in memory, tail save then
        // baked it in). Now lock short-circuits the handler entirely and
        // emits bandClickIgnored for user-visible feedback.
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        slice->setFrequency(7100000.0);   // on 40m
        slice->setDspMode(DSPMode::LSB);
        slice->setLocked(true);

        QSignalSpy spy(&radio, &RadioModel::bandClickIgnored);
        radio.onBandButtonClicked(Band::Band20m);

        // Nothing moved — freq AND mode both frozen.
        QCOMPARE(slice->frequency(), 7100000.0);
        QCOMPARE(slice->dspMode(),   DSPMode::LSB);

        // Signal fired so UI can surface the reason.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).value<Band>(), Band::Band20m);
        const QString reason = spy.first().at(1).toString();
        QVERIFY2(reason.contains(QStringLiteral("locked"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("reason should mention lock; got: %1").arg(reason)));
    }

    void save_on_exit_persists_current_band_state() {
        RadioModel radio;
        radio.addSlice();
        radio.setActiveSlice(0);
        SliceModel* slice = radio.sliceById(0);
        slice->setFrequency(14100000.0);   // 20m
        slice->setDspMode(DSPMode::CWU);

        radio.onBandButtonClicked(Band::Band40m);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("Slice0/Band20m/Frequency")).toDouble(),
                 14100000.0);
        QCOMPARE(s.value(QStringLiteral("Slice0/Band20m/DspMode")).toInt(),
                 static_cast<int>(DSPMode::CWU));
    }

    // Task 35: a band grid click, through the Thetis band groups, changes
    // the band of the grid's own slice with onBandButtonClicked(slice,
    // band). Each slice keeps its own group. Nothing keys.
    void a_band_grid_click_reaches_its_slice_through_the_groups() {
        RadioModel radio;
        radio.addSlice();
        radio.addSlice();
        SliceModel* a = radio.sliceById(0);
        SliceModel* b = radio.sliceById(1);
        QVERIFY(a && b);
        a->setFrequency(7100000.0);   // 40m
        a->setDspMode(DSPMode::LSB);
        b->setFrequency(3700000.0);   // 80m
        b->setDspMode(DSPMode::LSB);
        constexpr int kSliceA = 1;    // ContainerWidget::rxSource() for slice A
        constexpr int kSliceB = 2;

        ContainerButtonDispatcher dispatcher(&radio, ContainerButtonDispatcher::Hooks{});
        BandButtonItem grid;
        // The string form: BandButtonItem is not exported from the GUI DLL.
        QSignalSpy clicks(&grid, SIGNAL(bandClicked(int)));
        QVERIFY(clicks.isValid());
        // What MainWindow does with a container's band grid.
        const auto click = [&](int index, int rxSource) {
            dispatcher.applyBand(&grid, rxSource);
            const int before = clicks.count();
            grid.buttonClicked(index, Qt::LeftButton);
            if (clicks.count() > before) {
                QVERIFY(dispatcher.clickBand(clicks.last().at(0).toInt(), rxSource).isEmpty());
            }
            dispatcher.applyBand(&grid, rxSource);
        };

        click(5, kSliceA);   // HF: 20m
        QCOMPARE(a->frequency(), 14155000.0);
        QCOMPARE(b->frequency(), 3700000.0);
        QVERIFY(grid.button(5).on);

        click(12, kSliceA);  // the VHF jump: no band change
        QCOMPARE(grid.shownGroup(), BandGroup::Vhf);
        QCOMPARE(a->frequency(), 14155000.0);
        click(0, kSliceA);   // VHF: 2m
        QCOMPARE(bandFromFrequency(a->frequency()), Band::Band2m);
        QCOMPARE(grid.shownGroup(), BandGroup::Vhf);
        QVERIFY(grid.button(0).on);

        click(14, kSliceA);  // HF
        click(13, kSliceA);  // WWV
        QCOMPARE(bandFromFrequency(a->frequency()), Band::WWV);
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);

        // Slice A on GEN, Slice B on HF: each keeps its own group.
        click(14, kSliceA);  // SWL
        QCOMPARE(grid.group(0), BandGroup::Gen);
        dispatcher.applyBand(&grid, kSliceB);
        QCOMPARE(grid.shownGroup(), BandGroup::Hf);
        QVERIFY(grid.button(1).on);  // 80m
        click(3, kSliceB);   // 40m, on slice B only
        QCOMPARE(bandFromFrequency(b->frequency()), Band::Band40m);
        QCOMPARE(bandFromFrequency(a->frequency()), Band::WWV);
        dispatcher.applyBand(&grid, kSliceA);
        QCOMPARE(grid.shownGroup(), BandGroup::Gen);
        QCOMPARE(grid.group(1), BandGroup::Hf);

        // The GEN group's 31m tunes Slice A into 31m.
        click(bandGroupPosition(BandGroup::Gen, Band::Band31m), kSliceA);
        QCOMPARE(a->frequency(), 9100000.0);
        QCOMPARE(a->dspMode(), DSPMode::SAM);
        QVERIFY(grid.button(bandGroupPosition(BandGroup::Gen, Band::Band31m)).on);

        QVERIFY(!radio.mox());
    }

    // Fix round 1: an SWL band tunes its slice with the Thetis seed
    // (AddBandStackSWL), saves as itself, and leaves the other slice alone.
    // Only XVTR gives the transverter reason.
    void swl_band_click_tunes_its_own_slice() {
        RadioModel radio;
        radio.addSlice();
        radio.addSlice();
        SliceModel* a = radio.sliceById(0);
        SliceModel* b = radio.sliceById(1);
        QVERIFY(a && b);
        a->setFrequency(14100000.0);   // 20m
        a->setDspMode(DSPMode::USB);
        b->setFrequency(3700000.0);    // 80m
        b->setDspMode(DSPMode::LSB);

        QSignalSpy ignored(&radio, &RadioModel::bandClickIgnored);
        radio.onBandButtonClicked(a, Band::Band31m);
        QCOMPARE(ignored.count(), 0);
        QCOMPARE(a->frequency(), 9100000.0);
        QCOMPARE(a->dspMode(), DSPMode::SAM);
        QCOMPARE(BandDefaults::bandGridBandFromFrequency(a->frequency()), Band::Band31m);
        QCOMPARE(b->frequency(), 3700000.0);
        QCOMPARE(b->dspMode(), DSPMode::LSB);

        // Tuned within 31m, then GEN: 31m keeps its own state and GEN's
        // slot is not written with the 31m frequency.
        a->setFrequency(9500000.0);
        radio.onBandButtonClicked(a, Band::GEN);
        QCOMPARE(a->frequency(), 13845000.0);
        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("Slice0/Band31m/Frequency")).toDouble(), 9500000.0);
        radio.onBandButtonClicked(a, Band::Band31m);
        QCOMPARE(a->frequency(), 9500000.0);
        QCOMPARE(s.value(QStringLiteral("Slice0/BandGEN/Frequency")).toDouble(), 13845000.0);
        // Clicking 31m on 31m is the same band.
        radio.onBandButtonClicked(a, Band::Band31m);
        QCOMPARE(a->frequency(), 9500000.0);
        QCOMPARE(ignored.count(), 0);

        // XVTR still refuses with its own reason.
        radio.onBandButtonClicked(a, Band::XVTR);
        QCOMPARE(ignored.count(), 1);
        QVERIFY(ignored.last().at(1).toString().contains(QStringLiteral("transverter")));
        QCOMPARE(a->frequency(), 9500000.0);
        QCOMPARE(b->frequency(), 3700000.0);
        QVERIFY(!radio.mox());
    }
};

QTEST_MAIN(TestRadioModelBandClick)
#include "tst_radio_model_band_click.moc"
