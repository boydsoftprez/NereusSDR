// no-port-check: NereusSDR-original regression test.
//
// Phase 3J-1 closeout Item 4 (2026-05-12): pins per-(band, mode)
// filter persistence on SliceModel.  When the user adjusts the filter
// for a specific band+mode pair and then switches mode, switching back
// to the original mode must restore the operator's previously-set
// cutoffs, NOT slam to defaultFilterForMode.
//
// Mirrors Thetis preset[m].LastFilter machinery (console.cs:14653-14671
// [v2.10.3.13]).

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestSliceFilterPerBandPerModePersistence : public QObject {
    Q_OBJECT

private slots:
    void mode_change_remembers_per_mode_filter() {
        // Use the singleton with an explicit test-mode reset so each test
        // starts from a clean slate.  AppSettings is process-wide, so we
        // can't easily inject a QTemporaryDir without redirecting the
        // singleton -- the TestSandboxInit harness already handles that.
        AppSettings::instance().setValue(QStringLiteral("__resetMarker"),
                                          QStringLiteral("Item4Test1"));

        SliceModel slice(/*sliceIndex=*/0, nullptr);
        slice.setFrequency(14200000.0);  // 20m
        slice.setDspMode(DSPMode::USB);

        // Operator widens the USB filter on 20m
        slice.setFilter(200, 2900);

        // Switch to DIGU on the same band -- filter should land on the
        // default for DIGU (NOT the USB cutoffs we just set)
        slice.setDspMode(DSPMode::DIGU);
        QVERIFY(slice.filterLow()  != 200);
        QVERIFY(slice.filterHigh() != 2900);

        // Operator narrows the DIGU filter
        slice.setFilter(1400, 1700);

        // Switch back to USB -- the previously-set 200/2900 must restore
        slice.setDspMode(DSPMode::USB);
        QCOMPARE(slice.filterLow(),  200);
        QCOMPARE(slice.filterHigh(), 2900);

        // Switch back to DIGU -- the 1400/1700 must restore (not default)
        slice.setDspMode(DSPMode::DIGU);
        QCOMPARE(slice.filterLow(),  1400);
        QCOMPARE(slice.filterHigh(), 1700);
    }

    void unset_mode_falls_back_to_default() {
        AppSettings::instance().setValue(QStringLiteral("__resetMarker"),
                                          QStringLiteral("Item4Test2"));

        // Use a band+mode pair that has no persisted value yet, on a
        // different slice index from test 1 so its writes don't bleed in.
        SliceModel slice(/*sliceIndex=*/3, nullptr);
        slice.setFrequency(7100000.0);  // 40m
        slice.setDspMode(DSPMode::LSB);
        // Verify the LSB defaults are applied (not stale state)
        auto def = SliceModel::defaultFilterForMode(DSPMode::LSB);
        QCOMPARE(slice.filterLow(),  def.first);
        QCOMPARE(slice.filterHigh(), def.second);
    }

    void band_change_keyspace_isolated_per_band() {
        AppSettings::instance().setValue(QStringLiteral("__resetMarker"),
                                          QStringLiteral("Item4Test3"));

        // Same mode on two different bands should keep separate filter
        // memories.  Use slice 2 so we don't collide with tests 1 / 2.
        SliceModel slice(/*sliceIndex=*/2, nullptr);
        slice.setFrequency(14200000.0);  // 20m
        slice.setDspMode(DSPMode::USB);
        slice.setFilter(100, 3000);

        // Drop to 40m USB -- because the band-crossing handler is in
        // RadioModel (not SliceModel), changing m_frequency alone won't
        // trigger a saveToSettings/restoreFromSettings cycle here.  Use
        // saveToSettings(20m) to persist 20m USB then move to 40m.
        slice.saveToSettings(Band::Band20m);

        slice.setFrequency(7100000.0);
        slice.setFilter(300, 2700);
        slice.saveToSettings(Band::Band40m);

        // Back to 20m
        slice.setFrequency(14200000.0);
        slice.restoreFromSettings(Band::Band20m);
        QCOMPARE(slice.filterLow(),  100);
        QCOMPARE(slice.filterHigh(), 3000);

        // Back to 40m
        slice.setFrequency(7100000.0);
        slice.restoreFromSettings(Band::Band40m);
        QCOMPARE(slice.filterLow(),  300);
        QCOMPARE(slice.filterHigh(), 2700);
    }
};

QTEST_MAIN(TestSliceFilterPerBandPerModePersistence)
#include "tst_slice_filter_per_band_per_mode_persistence.moc"
