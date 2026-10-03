// tests/tst_level_cal_grid_follow_guard.cpp  (NereusSDR)
// no-port-check: test file, no Thetis attribution required.
//
// Level Cal: while a level calibration runs, the window's grid does not
// follow the noise floor, and afterwards it is put back as it was.
// From Thetis console.cs:9872-9876 and 10230-10231 [v2.10.3.15]
// (CalibrateLevel saves GridMinFollowsNFRX1/RX2, turns them off, and
// restores them at the end).

#include "core/AppSettings.h"
#include "gui/LevelCalGridFollowGuard.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"

#include <QtTest/QtTest>

using NereusSDR::LevelCalGridFollowGuard;
using NereusSDR::RadioModel;
using NereusSDR::SpectrumWidget;
using NereusSDR::AppSettings;

namespace {

struct Grid {
    bool follow = true;
    QList<bool> writes;
};

void attach(LevelCalGridFollowGuard& guard, Grid& grid)
{
    guard.setAccess([&grid]() { return grid.follow; },
                    [&grid](bool on) { grid.follow = on; grid.writes << on; });
}

} // namespace

class TstLevelCalGridFollowGuard : public QObject {
    Q_OBJECT

private slots:
    void offWhileRunningThenRestored()
    {
        RadioModel model(RadioModel::Role::Remote);
        LevelCalGridFollowGuard guard(&model);
        Grid grid;
        attach(guard, grid);
        QVERIFY(model.applyStationLevelCalValue("levelCalRunning", true));
        QCOMPARE(grid.follow, false);
        // Progress while it runs changes nothing more.
        QVERIFY(model.applyStationLevelCalValue("levelCalPercent", 50));
        QCOMPARE(grid.writes, QList<bool>{false});
        QVERIFY(model.applyStationLevelCalValue("levelCalRunning", false));
        QCOMPARE(grid.follow, true);
        QCOMPARE(grid.writes, (QList<bool>{false, true}));
    }

    void offStaysOff()
    {
        RadioModel model(RadioModel::Role::Remote);
        LevelCalGridFollowGuard guard(&model);
        Grid grid;
        grid.follow = false;
        attach(guard, grid);
        QVERIFY(model.applyStationLevelCalValue("levelCalRunning", true));
        QVERIFY(model.applyStationLevelCalValue("levelCalRunning", false));
        QCOMPARE(grid.follow, false);
    }

    void nothingRunsNothingChanges()
    {
        RadioModel model(RadioModel::Role::Remote);
        LevelCalGridFollowGuard guard(&model);
        Grid grid;
        attach(guard, grid);
        QVERIFY(model.applyStationLevelCalValue("levelCalMessage", QStringLiteral("x")));
        QVERIFY(grid.writes.isEmpty());
    }

    // The session ends in the middle of a run: the grid comes back.
    void sessionEndRestores()
    {
        RadioModel model(RadioModel::Role::Remote);
        LevelCalGridFollowGuard guard(&model);
        Grid grid;
        attach(guard, grid);
        QVERIFY(model.applyStationLevelCalValue("levelCalRunning", true));
        model.clearStationLevelCal();
        QCOMPARE(grid.follow, true);
    }

    // Level Cal fix wave: the window quits (its closing save) or crashes
    // while a run holds the follow off. The saved setting is still the
    // user's, so the next launch follows the noise floor again.
    void quitMidRunKeepsTheUsersSetting()
    {
        const QString key = QStringLiteral("DisplayAdjustGridMinToNoiseFloor");
        RadioModel model(RadioModel::Role::Remote);
        SpectrumWidget pan;
        pan.setAdjustGridMinToNoiseFloor(true);
        pan.saveSettingsForTest();
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("True"));

        LevelCalGridFollowGuard guard(&model);
        guard.setAccess([&pan]() { return pan.adjustGridMinToNoiseFloor(); },
                        [&pan](bool on) { pan.setAdjustGridMinToNoiseFloor(on); },
                        [](std::optional<bool> saved) {
                            SpectrumWidget::setGridFollowSaveHold(saved);
                        });
        QVERIFY(model.applyStationLevelCalValue("levelCalRunning", true));
        QVERIFY(!pan.adjustGridMinToNoiseFloor());

        // Any save during the run (the closing one on a quit, or the
        // debounced one a crash leaves on disk) stores the user's value.
        pan.saveSettingsForTest();
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("True"));

        // The run ends: the follow is back and the hold is gone.
        QVERIFY(model.applyStationLevelCalValue("levelCalRunning", false));
        QVERIFY(pan.adjustGridMinToNoiseFloor());
        QVERIFY(!SpectrumWidget::gridFollowSaveHold().has_value());
        pan.setAdjustGridMinToNoiseFloor(false);
        pan.saveSettingsForTest();
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("False"));
    }
};

QTEST_MAIN(TstLevelCalGridFollowGuard)
#include "tst_level_cal_grid_follow_guard.moc"
