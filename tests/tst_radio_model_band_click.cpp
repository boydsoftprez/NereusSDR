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

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
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
                 QStringLiteral("GEN"),  QStringLiteral("XVTR") }) {
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
};

QTEST_MAIN(TestRadioModelBandClick)
#include "tst_radio_model_band_click.moc"
