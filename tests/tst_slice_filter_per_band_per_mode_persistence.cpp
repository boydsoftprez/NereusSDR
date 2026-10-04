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
#include <memory>

#include "core/AppSettings.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestSliceFilterPerBandPerModePersistence : public QObject {
    Q_OBJECT

private slots:
    void mode_notification_continuation_data() {
        QTest::addColumn<int>("nested");
        QTest::newRow("newer-mode") << 0;
        QTest::newRow("newer-filter-low") << 1;
        QTest::newRow("newer-filter-high") << 2;
        QTest::newRow("newer-filter-pair") << 3;
        QTest::newRow("mode-away-and-back") << 4;
        QTest::newRow("mode-no-op-echo") << 5;
        QTest::newRow("filter-low-no-op-echo") << 6;
        QTest::newRow("filter-high-no-op-echo") << 7;
        QTest::newRow("filter-pair-no-op-echo") << 8;
    }
    void mode_notification_continuation() {
        QFETCH(int, nested);
        AppSettings::instance().clear();
        SliceModel slice(4, nullptr);
        slice.setFrequency(14200000);
        slice.setFilter(100, 2800);
        QObject observer;
        QList<QPair<int,int>> filters;
        bool entered=false;
        connect(&slice, &SliceModel::filterChanged, &observer,
                [&](int low, int high) { filters.append({low,high}); });
        connect(&slice, &SliceModel::dspModeChanged, &observer, [&](DSPMode) {
            if (entered) { return; }
            entered=true;
            if (nested==0) { slice.setDspMode(DSPMode::AM); }
            else if (nested==1) { slice.setFilterLow(-2400); }
            else if (nested==2) { slice.setFilterHigh(-200); }
            else if (nested==3) { slice.setFilter(-2400,-200); }
            else if (nested==4) { slice.setDspMode(DSPMode::AM); slice.setDspMode(DSPMode::LSB); }
            else if (nested==5) { slice.setDspMode(DSPMode::LSB); }
            else if (nested==6) { slice.setFilterLow(slice.filterLow()); }
            else if (nested==7) { slice.setFilterHigh(slice.filterHigh()); }
            else { slice.setFilter(slice.filterLow(),slice.filterHigh()); }
        });
        slice.setDspMode(DSPMode::LSB);
        QVERIFY(entered);
        QCOMPARE(slice.dspMode(), nested==0 ? DSPMode::AM : DSPMode::LSB);
        QCOMPARE(filters.size(), nested==4 ? 2 : 1);
        QCOMPARE(filters.last(), qMakePair(slice.filterLow(),slice.filterHigh()));
        if (nested==4) {
            const auto expected=SliceModel::defaultFilterForMode(DSPMode::LSB);
            QCOMPARE(filters.last(),expected);
        }
    }

    void early_mode_callback_data() {
        QTest::addColumn<int>("origin"); QTest::addColumn<int>("action");
        const QList<QByteArray> origins{"callsign","sync","save-low","save-high"};
        const QList<QByteArray> actions{"delete","newer-mode","newer-filter","mode-away-back"};
        for (int origin=0;origin<origins.size();++origin) {
            for (int action=0;action<actions.size();++action) {
                QTest::newRow((origins[origin]+'-'+actions[action]).constData()) << origin << action;
            }
        }
    }
    void early_mode_callback() {
        QFETCH(int,origin); QFETCH(int,action);
        AppSettings& settings=AppSettings::instance(); settings.setChangeHook({}); settings.clear();
        auto slice=std::make_unique<SliceModel>(5,nullptr);
        slice->setFrequency(14200000); slice->setFilter(100,2800);
        if (origin<2) {
            slice->setDspMode(DSPMode::RADE_U); slice->setLastRadeRxCallsign("KG4VCF"); slice->setRadeSynced(true);
        }
        QObject observer; bool entered=false; int modes=0,filters=0;
        DSPMode expectedMode=DSPMode::LSB; QPair<int,int> expectedEdges;
        connect(slice.get(),&SliceModel::dspModeChanged,&observer,[&](DSPMode) { ++modes; });
        connect(slice.get(),&SliceModel::filterChanged,&observer,[&](int,int) { ++filters; });
        const auto callback=[&] {
            if (entered) { return; } entered=true;
            if (action==0) { slice.reset(); return; }
            if (action==1) { slice->setDspMode(DSPMode::AM); }
            else if (action==2) { slice->setFilter(-2400,-200); }
            else { slice->setDspMode(DSPMode::AM); slice->setDspMode(DSPMode::LSB); }
            expectedMode=slice->dspMode(); expectedEdges={slice->filterLow(),slice->filterHigh()};
        };
        if (origin==0) { connect(slice.get(),&SliceModel::lastRadeRxCallsignChanged,&observer,[&](const QString&) { callback(); }); }
        else if (origin==1) { connect(slice.get(),&SliceModel::radeSyncedChanged,&observer,[&](bool) { callback(); }); }
        else {
            settings.setChangeHook([&](const QString& key) {
                if (key.endsWith(origin==2 ? "/FilterLow":"/FilterHigh")) { callback(); }
            });
        }
        slice->setDspMode(DSPMode::LSB); settings.setChangeHook({});
        QVERIFY(entered);
        if (action==0) { QVERIFY(!slice); QCOMPARE(modes,0); QCOMPARE(filters,0); return; }
        QCOMPARE(slice->dspMode(),expectedMode); QCOMPARE(qMakePair(slice->filterLow(),slice->filterHigh()),expectedEdges);
        QCOMPARE(modes,action==3 ? 2 : 1); QCOMPARE(filters,action==3 ? 2 : 1);
    }

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
