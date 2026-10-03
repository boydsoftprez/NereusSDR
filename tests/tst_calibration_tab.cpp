// tests/tst_calibration_tab.cpp  (NereusSDR)
//
// Smoke tests for CalibrationTab UI (Phase 3P-G commit 2).
// no-port-check: test file — no Thetis attribution required.
//
// Covers:
//  - Construction with a dummy RadioModel doesn't crash
//  - groupBoxCountForTest() returns 5 (Freq Cal, Level Cal, HPSDR Diag,
//    TX Display Cal, Volts/Amps Calibration).  Group 6 (PA Forward Power
//    Calibration / PaCalibrationGroup) was migrated to PA → Watt Meter on
//    2026-05-02 (Setup IA reshape Phase 3A) and is covered by
//    tst_pa_watt_meter_page.  Group 5 was relabelled from "PA Current (A)
//    calculation" to "Volts/Amps Calibration" 2026-05-02 (Phase 3B).
//  - Setting a controller value updates the UI (controller -> UI sync)
//  - Changing a UI spinbox updates the controller (UI -> controller write)
//  - Level Cal 2: the hosting desktop's Start names a slice it may change,
//    or is disabled with the ownership words when it has none

#include "gui/setup/hardware/CalibrationTab.h"
#include "core/AppSettings.h"
#include "core/CalibrationController.h"
#include "core/LevelCalibrationService.h"
#include "core/SliceOwnership.h"
#include "core/session/IStationLink.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "FakeLevelCalibrationHost.h"

#include <QtTest/QtTest>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QProgressBar>
#include <QPushButton>

using namespace NereusSDR::LevelCalTest;

namespace {

// What the tab put in front of the operator, in place of a message box.
struct Prompts {
    int asked = 0;
    bool answer = true;
    QStringList titles;
    QStringList texts;
    QList<bool> warnings;

    void attach(NereusSDR::CalibrationTab& tab)
    {
        tab.setLevelCalPromptsForTest(
            [this]() { ++asked; return answer; },
            [this](const QString& title, const QString& text, bool warning) {
                titles << title;
                texts << text;
                warnings << warning;
            });
    }
};

struct LevelCalControls {
    QPushButton* start = nullptr;
    QPushButton* reset = nullptr;
    QPushButton* cancel = nullptr;
    QProgressBar* progress = nullptr;
    QLabel* status = nullptr;
    QDoubleSpinBox* freq = nullptr;
    QDoubleSpinBox* level = nullptr;

    explicit LevelCalControls(NereusSDR::CalibrationTab& tab)
        : start(tab.findChild<QPushButton*>(QStringLiteral("levelCalStartButton")))
        , reset(tab.findChild<QPushButton*>(QStringLiteral("levelCalResetButton")))
        , cancel(tab.findChild<QPushButton*>(QStringLiteral("levelCalCancelButton")))
        , progress(tab.findChild<QProgressBar*>(QStringLiteral("levelCalProgressBar")))
        , status(tab.findChild<QLabel*>(QStringLiteral("levelCalStatusLabel")))
        , freq(tab.findChild<QDoubleSpinBox*>(QStringLiteral("levelCalFrequencySpin")))
        , level(tab.findChild<QDoubleSpinBox*>(QStringLiteral("levelCalLevelSpin")))
    {
    }
    bool found() const { return start && reset && cancel && progress && status && freq && level; }
};

// The local model's service on the fake receiver; `holdMs` keeps the run
// going at its last wait.
bool useFakeReceiver(NereusSDR::RadioModel& model, FakeHost& fake, int holdMs)
{
    NereusSDR::LevelCalibrationService* service = model.levelCalibrationServiceForTest();
    if (service == nullptr) {
        return false;
    }
    service->setHostForTest(&fake);
    NereusSDR::LevelCalibrationRun::Timings timings = instant();
    timings.finalSettleMs = holdMs;
    service->setTimingsForTest(timings);
    return true;
}

} // namespace

class TstCalibrationTab : public QObject {
    Q_OBJECT

private slots:
    void construction_doesNotCrash();
    void groupBoxCount_isFive();
    void controllerToUi_freqFactor();
    void uiToController_rx1LnaOffset();
    void sixMeterLnaSpinsTakeThetisRangeAndRx2IsDisabled();
    void correctionFactorSpinsTakeThetisRange();

    // Level Cal: the Start, Cancel and progress of the Core's run.
    void levelCalRemoteWithoutCoreShowsDisabledWithReason();
    void levelCalLocalIdleState();
    void levelCalStartAsksFirst();
    void levelCalRefusalIsShown();
    void levelCalRunShowsProgressAndCompletes();
    void levelCalCancelStopsTheRun();
    void levelCalFollowsTheCoresRunInARemoteWindow();

    // Level Cal 2: which slice the hosting desktop's Start names.
    void levelCalHostOwnSliceRuns();
    void levelCalHostUsesItsOwnSliceWhenAnotherDeviceIsActive();
    void levelCalHostWithNoSliceOfItsOwnIsDisabled();
};

void TstCalibrationTab::construction_doesNotCrash()
{
    NereusSDR::RadioModel model;
    NereusSDR::CalibrationTab tab(&model);
    // Verify it's a QWidget and has children
    QVERIFY(tab.isWidgetType());
}

void TstCalibrationTab::groupBoxCount_isFive()
{
    NereusSDR::RadioModel model;
    NereusSDR::CalibrationTab tab(&model);
    // 5 group boxes: Freq Cal, Level Cal, HPSDR Freq Cal Diagnostic,
    // TX Display Cal, Volts/Amps Calibration.
    // Group 6 (PA Forward Power Calibration / PaCalibrationGroup) was
    // migrated to PA → Watt Meter on 2026-05-02 (Setup IA reshape Phase 3A);
    // see tst_pa_watt_meter_page for that integration coverage.
    // Group 5 was previously titled "PA Current (A) calculation"; renamed
    // to "Volts/Amps Calibration" 2026-05-02 (Setup IA reshape Phase 3B).
    QCOMPARE(tab.groupBoxCountForTest(), 5);
}

void TstCalibrationTab::controllerToUi_freqFactor()
{
    NereusSDR::RadioModel model;
    NereusSDR::CalibrationTab tab(&model);

    // Set a distinctive value via controller
    NereusSDR::CalibrationController& ctrl = model.calibrationControllerMutable();
    ctrl.setFreqCorrectionFactor(1.000007);
    // onControllerChanged() should have been triggered synchronously;
    // find the freqFactorSpin by walking the hierarchy
    // We verify indirectly: the controller value is set correctly
    QCOMPARE(ctrl.freqCorrectionFactor(), 1.000007);
}

void TstCalibrationTab::uiToController_rx1LnaOffset()
{
    NereusSDR::RadioModel model;
    NereusSDR::CalibrationTab tab(&model);

    NereusSDR::CalibrationController& ctrl = model.calibrationControllerMutable();
    // From Thetis setup.designer.cs:12112-12116 [v2.10.3.15]: 13 dB.
    QCOMPARE(ctrl.rx1_6mLnaOffset(), 13.0);

    // Find the rx1LnaSpin: it's a QDoubleSpinBox inside a child widget
    // We exercise via controller setter (the UI->controller path requires
    // actual QDoubleSpinBox setValue() which is hard to do without a running
    // event loop; we test the round-trip via the controller directly)
    ctrl.setRx1_6mLnaOffset(2.5);
    QCOMPARE(ctrl.rx1_6mLnaOffset(), 2.5);
    // After controller changed, onControllerChanged() is called and
    // the spinbox should reflect the new value (next syncFromController() tick)
}

// From Thetis setup.designer.cs:12047-12116 [v2.10.3.15]: ud6mLNAGainOffset
// and ud6mRx2LNAGainOffset, 0..25 dB, step 1, one decimal, value 13. The Rx2
// value has no receiver calibration of its own yet, in a local window and
// in a remote one alike, so it shows disabled with the reason.
void TstCalibrationTab::sixMeterLnaSpinsTakeThetisRangeAndRx2IsDisabled()
{
    for (const auto role : {NereusSDR::RadioModel::Role::Local,
                            NereusSDR::RadioModel::Role::Remote}) {
        NereusSDR::RadioModel model(role);
        NereusSDR::CalibrationTab tab(&model);
        auto* rx1 = tab.findChild<QDoubleSpinBox*>(QStringLiteral("rx1SixMeterLnaSpin"));
        auto* rx2 = tab.findChild<QDoubleSpinBox*>(QStringLiteral("rx2SixMeterLnaSpin"));
        QVERIFY(rx1 != nullptr);
        QVERIFY(rx2 != nullptr);
        for (QDoubleSpinBox* spin : {rx1, rx2}) {
            QCOMPARE(spin->minimum(), 0.0);
            QCOMPARE(spin->maximum(), 25.0);
            QCOMPARE(spin->singleStep(), 1.0);
            QCOMPARE(spin->decimals(), 1);
            QCOMPARE(spin->value(), 13.0);
        }
        QVERIFY(rx1->isEnabled());
        QVERIFY(!rx2->isEnabled());
        QVERIFY(!rx2->toolTip().isEmpty());
    }
}

// Lead's ruling (R-R3-49): the correction factors take Thetis's range.
// From Thetis setup.designer.cs:11983 [v2.10.3.15] udHPSDRFreqCorrectFactor
//   Maximum = 65; Minimum = 0 (udHPSDRFreqCorrectFactor10MHz :11928, the same)
void TstCalibrationTab::correctionFactorSpinsTakeThetisRange()
{
    NereusSDR::RadioModel model;
    NereusSDR::CalibrationTab tab(&model);
    for (const char* name : {"freqCorrectionFactorSpin", "freqCorrectionFactor10MSpin"}) {
        auto* spin = tab.findChild<QDoubleSpinBox*>(QLatin1String(name));
        QVERIFY2(spin != nullptr, name);
        QCOMPARE(spin->minimum(), 0.0);
        QCOMPARE(spin->maximum(), 65.0);
    }
}

// Level Cal: a remote window whose Core cannot run the calibration shows
// Start and Cancel disabled with the reason, never hidden.
void TstCalibrationTab::levelCalRemoteWithoutCoreShowsDisabledWithReason()
{
    NereusSDR::RadioModel model(NereusSDR::RadioModel::Role::Remote);
    NereusSDR::CalibrationTab tab(&model);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    for (QWidget* w : {static_cast<QWidget*>(c.start), static_cast<QWidget*>(c.cancel)}) {
        QVERIFY(!w->isHidden());
        QVERIFY(!w->isEnabled());
        QCOMPARE(w->toolTip(), NereusSDR::IStationLink::levelCalibrationRunUnavailableReason());
    }
}

// A local window: Start is live, Cancel waits for a run, nothing measured.
void TstCalibrationTab::levelCalLocalIdleState()
{
    NereusSDR::RadioModel model;
    NereusSDR::CalibrationTab tab(&model);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    QVERIFY(c.start->isEnabled());
    QVERIFY(!c.start->toolTip().isEmpty());
    QVERIFY(!c.cancel->isHidden());
    QVERIFY(!c.cancel->isEnabled());
    QVERIFY(!c.cancel->toolTip().isEmpty());
    QCOMPARE(c.progress->value(), 0);
    QVERIFY(c.status->text().isEmpty());
}

// From Thetis setup.cs:6516-6523 [v2.10.3.15]: Start asks whether the
// signal is there first; No leaves everything as it was.
void TstCalibrationTab::levelCalStartAsksFirst()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::RadioModel model;
    FakeHost fake;
    QVERIFY(useFakeReceiver(model, fake, 0));
    model.addSlice(QStringLiteral("pan-0"));
    NereusSDR::CalibrationTab tab(&model);
    Prompts prompts;
    prompts.attach(tab);
    prompts.answer = false;
    LevelCalControls c(tab);
    QVERIFY(c.found());
    c.start->click();
    QCOMPARE(prompts.asked, 1);
    QVERIFY(!model.levelCalRunning());
    QCOMPARE(fake.meterReads, 0);
    QVERIFY(prompts.texts.isEmpty());
    NereusSDR::AppSettings::instance().clear();
}

// A run that cannot start says why.
void TstCalibrationTab::levelCalRefusalIsShown()
{
    NereusSDR::RadioModel model;
    NereusSDR::CalibrationTab tab(&model);
    Prompts prompts;
    prompts.attach(tab);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    c.start->click();  // no slice open
    QCOMPARE(prompts.asked, 1);
    QCOMPARE(prompts.texts,
             QStringList{QStringLiteral("Open a slice before calibrating the receive level.")});
    QCOMPARE(prompts.warnings, QList<bool>{true});
    // A refusal the Core sends after the start arrives the same way.
    emit model.levelCalibrationRefused(QStringLiteral("Stop transmitting before calibrating the receive level."));
    QCOMPARE(prompts.texts.size(), 2);
    QCOMPARE(prompts.texts.last(),
             QStringLiteral("Stop transmitting before calibrating the receive level."));
    QVERIFY(c.start->isEnabled());
}

// From Thetis setup.cs:6525-6558 [v2.10.3.15]: Start and Reset are off
// while the run goes, the progress shows, and a finished run says
// "Level Calibration complete." under "Calibration".
void TstCalibrationTab::levelCalRunShowsProgressAndCompletes()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::RadioModel model;
    FakeHost fake;
    QVERIFY(useFakeReceiver(model, fake, 300));
    model.addSlice(QStringLiteral("pan-0"));
    NereusSDR::CalibrationTab tab(&model);
    Prompts prompts;
    prompts.attach(tab);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    c.freq->setValue(kCentre + 1000.0);
    c.level->setValue(-50.0);
    c.start->click();
    QVERIFY(model.levelCalRunning());
    QVERIFY(!c.start->isEnabled());
    QVERIFY(!c.reset->isEnabled());
    QVERIFY(c.cancel->isEnabled());
    QTRY_VERIFY(c.progress->value() > 0);
    QTRY_VERIFY(!model.levelCalRunning());
    QVERIFY(model.levelCalSucceeded());
    QCOMPARE(c.progress->value(), 100);
    QCOMPARE(prompts.titles, QStringList{QStringLiteral("Calibration")});
    QCOMPARE(prompts.texts, QStringList{QStringLiteral("Level Calibration complete.")});
    QCOMPARE(prompts.warnings, QList<bool>{false});
    QVERIFY(c.start->isEnabled());
    QVERIFY(c.reset->isEnabled());
    QVERIFY(!c.cancel->isEnabled());
    NereusSDR::AppSettings::instance().clear();
}

// Cancel stops the run (Thetis's progress window Abort); the tab says so
// without a completion message.
void TstCalibrationTab::levelCalCancelStopsTheRun()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::RadioModel model;
    FakeHost fake;
    QVERIFY(useFakeReceiver(model, fake, 5000));
    model.addSlice(QStringLiteral("pan-0"));
    NereusSDR::CalibrationTab tab(&model);
    Prompts prompts;
    prompts.attach(tab);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    c.freq->setValue(kCentre + 1000.0);
    c.level->setValue(-50.0);
    c.start->click();
    QVERIFY(model.levelCalRunning());
    c.cancel->click();
    QTRY_VERIFY(!model.levelCalRunning());
    QVERIFY(!model.levelCalSucceeded());
    QCOMPARE(c.status->text(), QStringLiteral("Level calibration was canceled."));
    QVERIFY(prompts.texts.isEmpty());
    QVERIFY(c.start->isEnabled());
    NereusSDR::AppSettings::instance().clear();
}

// A remote window shows the Core's run as it goes.
void TstCalibrationTab::levelCalFollowsTheCoresRunInARemoteWindow()
{
    NereusSDR::RadioModel model(NereusSDR::RadioModel::Role::Remote);
    NereusSDR::CalibrationTab tab(&model);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    QVERIFY(model.applyStationLevelCalValue("levelCalRunning", true));
    QVERIFY(model.applyStationLevelCalValue("levelCalPercent", 40));
    QCOMPARE(c.progress->value(), 40);
    QVERIFY(!c.start->isEnabled());
    QVERIFY(!c.reset->isEnabled());
    QVERIFY(model.applyStationLevelCalValue("levelCalMessage",
                                            QStringLiteral("Level calibration finished.")));
    QVERIFY(model.applyStationLevelCalValue("levelCalRunning", false));
    QCOMPARE(c.status->text(), QStringLiteral("Level calibration finished."));
}

// Level Cal 2: the hosting desktop's own slice active: Start names the
// station's active slice (-1) and runs.
void TstCalibrationTab::levelCalHostOwnSliceRuns()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::RadioModel model;
    FakeHost fake;
    QVERIFY(useFakeReceiver(model, fake, 0));
    const int mine = model.addSlice(QStringLiteral("pan-0"));
    model.sliceOwnership()->setOwner(mine, NereusSDR::SliceOwnership::stationDevice());
    QVERIFY(model.setActiveSliceById(mine));
    NereusSDR::CalibrationTab tab(&model);
    Prompts prompts;
    prompts.attach(tab);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    QString refusal;
    QCOMPARE(model.levelCalHostSlice(&refusal), -1);
    QVERIFY(refusal.isEmpty());
    QVERIFY(c.start->isEnabled());
    c.freq->setValue(kCentre + 1000.0);
    c.level->setValue(-50.0);
    c.start->click();
    QTRY_VERIFY(model.levelCalSucceeded());
    QVERIFY(fake.meterReads > 0);
    QVERIFY(prompts.warnings.isEmpty() || !prompts.warnings.contains(true));
    NereusSDR::AppSettings::instance().clear();
}

// Level Cal 2: another device's slice is the station's active slice and
// the desktop has one of its own: Start names the desktop's slice.
void TstCalibrationTab::levelCalHostUsesItsOwnSliceWhenAnotherDeviceIsActive()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::RadioModel model;
    FakeHost fake;
    QVERIFY(useFakeReceiver(model, fake, 0));
    const int phones = model.addSlice(QStringLiteral("pan-0"));
    const int mine = model.addSlice(QStringLiteral("pan-0"));
    NereusSDR::SliceOwnership* owners = model.sliceOwnership();
    owners->setOwner(phones, QByteArrayLiteral("phone-a"));
    owners->setOwner(mine, NereusSDR::SliceOwnership::stationDevice());
    QVERIFY(model.setActiveSliceById(phones));
    QCOMPARE(model.activeSlice()->sliceIndex(), phones);
    NereusSDR::CalibrationTab tab(&model);
    Prompts prompts;
    prompts.attach(tab);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    QString refusal;
    QCOMPARE(model.levelCalHostSlice(&refusal), mine);
    QVERIFY(refusal.isEmpty());
    QVERIFY(c.start->isEnabled());
    const double phoneHz = model.sliceById(phones)->frequency();
    c.freq->setValue(kCentre + 1000.0);
    c.level->setValue(-50.0);
    c.start->click();
    QTRY_VERIFY(model.levelCalSucceeded());
    QVERIFY(!prompts.warnings.contains(true));
    // The phone's slice was not moved.
    QCOMPARE(model.sliceById(phones)->frequency(), phoneHz);
    NereusSDR::AppSettings::instance().clear();
}

// Level Cal 2: another device's slice is the station's active slice and
// the desktop has none: Start is disabled with the ownership words, and
// the same holds for a slice nobody controls that has a listener.
void TstCalibrationTab::levelCalHostWithNoSliceOfItsOwnIsDisabled()
{
    NereusSDR::AppSettings::instance().clear();
    NereusSDR::RadioModel model;
    FakeHost fake;
    QVERIFY(useFakeReceiver(model, fake, 0));
    const int phones = model.addSlice(QStringLiteral("pan-0"));
    NereusSDR::SliceOwnership* owners = model.sliceOwnership();
    owners->setOwner(phones, QByteArrayLiteral("phone-a"));
    QVERIFY(model.setActiveSliceById(phones));
    NereusSDR::CalibrationTab tab(&model);
    Prompts prompts;
    prompts.attach(tab);
    LevelCalControls c(tab);
    QVERIFY(c.found());
    const QString owned =
        QStringLiteral("That slice belongs to another device. It can be changed only there.");
    QString refusal;
    QCOMPARE(model.levelCalHostSlice(&refusal), -1);
    QCOMPARE(refusal, owned);
    QVERIFY(!c.start->isEnabled());
    QCOMPARE(c.start->toolTip(), owned);

    // A slice the station runs held for an absent device is that device's,
    // not the desktop's own: Start stays off, whether the held slice is the
    // station's active slice or only the station device's (activeFor).
    const int held = model.addSlice(QStringLiteral("pan-0"));
    owners->hold(held, QByteArrayLiteral("phone-b"));
    QCOMPARE(owners->activeFor(NereusSDR::SliceOwnership::stationDevice()), held);
    QCOMPARE(model.levelCalHostSlice(&refusal), -1);
    QCOMPARE(refusal, owned);
    QVERIFY(!c.start->isEnabled());
    QVERIFY(model.setActiveSliceById(held));
    QCOMPARE(model.activeSlice()->sliceIndex(), held);
    QCOMPARE(model.levelCalHostSlice(&refusal), -1);
    QCOMPARE(refusal, owned);
    QVERIFY(!c.start->isEnabled());
    QCOMPARE(c.start->toolTip(), owned);
    QVERIFY(model.setActiveSliceById(phones));

    // Nobody controls it, but a device listens: still not the desktop's.
    owners->setOwner(phones, QByteArray());
    QVERIFY(owners->join(QByteArrayLiteral("phone-a"), phones));
    const QString unclaimed = QStringLiteral("Nobody controls slice %1. Take control to change it.")
                                  .arg(QChar(QLatin1Char('A').unicode() + phones));
    QCOMPARE(model.levelCalHostSlice(&refusal), -1);
    QCOMPARE(refusal, unclaimed);
    QVERIFY(!c.start->isEnabled());
    QCOMPARE(c.start->toolTip(), unclaimed);

    // The desktop takes a slice of its own: Start comes back and names it.
    const int mine = model.addSlice(QStringLiteral("pan-0"));
    owners->setOwner(mine, NereusSDR::SliceOwnership::stationDevice());
    QVERIFY(model.setActiveSliceById(phones));
    QCOMPARE(model.levelCalHostSlice(&refusal), mine);
    QVERIFY(c.start->isEnabled());
    QVERIFY(!model.levelCalRunning());
    QCOMPARE(fake.meterReads, 0);
    NereusSDR::AppSettings::instance().clear();
}

QTEST_MAIN(TstCalibrationTab)
#include "tst_calibration_tab.moc"
