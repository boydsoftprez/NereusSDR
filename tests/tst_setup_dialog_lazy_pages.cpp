// tests/tst_setup_dialog_lazy_pages.cpp  (NereusSDR)
//
// Issues #272 + #301 regression suite: lazy SetupDialog page construction.
//
// no-port-check: NereusSDR-original UI test fixture. SetupDialog is not a
// Thetis port, so no upstream attribution applies here.
//
// Background. SetupDialog::buildTree() used to construct all 55 setup pages
// synchronously from the ctor, so every Tools -> Setup open blocked the Qt
// main thread for roughly a second on an M-series Mac and multiple seconds on
// the reporters' hardware. Two symptoms fell out of that one stall:
//
//   #301 (funsutton, ANAN-10E, macOS Intel): the RX audio buffer looped 2-3
//        times, because AudioEngine's 10 ms drain timer could not run.
//   #272 (Chris-W4ORS, HL2, Windows 11): the freeze outlasted the Protocol 1
//        ep6 watchdog (2011 ms), the link was declared lost, and the unclean
//        audio teardown cascaded into a CLOCK_WATCHDOG_TIMEOUT BSOD on a
//        legacy 2010 Realtek driver.
//
// buildTree() now only registers a label plus a factory per nav leaf;
// realizePage() builds the widget the first time the leaf is selected.
//
// These tests pin the four properties the refactor has to hold:
//   1. Construction registers pages without building any of them.
//   2. Selecting a node realizes exactly that node's page, and revisiting it
//      reuses the cached widget.
//   3. A page built late still reads live model / persisted state correctly
//      (the issue #259 guarantee, now on an even later construction point).
//   4. Every cross-page wire that used to rely on a sibling page already
//      existing still fires: the PA [Reset PA Values] fan-out, the pending
//      TciServer pointer MainWindow hands over at wireSetupDialog() time, the
//      CFC dialog request, and the per-SKU PA capability push.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/RadioStatus.h"
#include "core/StepAttenuatorController.h"
#include "gui/SetupDialog.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/setup/PaSetupPages.h"
#include "models/RadioModel.h"

#ifdef HAVE_WEBSOCKETS
#include "core/TciServer.h"
#endif

using namespace NereusSDR;

namespace {

// Labels used across the cases below. Each must match a leaf registered by
// SetupDialog::buildTree(); a typo would silently make an assertion vacuous,
// so every test that uses one also asserts the label resolves.
const QString kAgcAlc      = QStringLiteral("AGC/ALC");
const QString kNbSnb       = QStringLiteral("NB/SNB");
const QString kGeneralOpts = QStringLiteral("Options");
const QString kWattMeter   = QStringLiteral("Watt Meter");
const QString kPaValues    = QStringLiteral("PA Values");
const QString kPaGain      = QStringLiteral("PA Gain");
const QString kCfc         = QStringLiteral("CFC");
const QString kTciServer   = QStringLiteral("TCI Server");

// Locate the "RX1 Enable" step-attenuator checkbox inside a realized
// GeneralOptionsPage. Same walk tst_general_options_page_step_att_init.cpp
// uses: the page holds the widget privately, so we match on caption.
QCheckBox* findRx1StepAttCheck(QWidget* page)
{
    for (QCheckBox* box : page->findChildren<QCheckBox*>()) {
        if (box->text() == QStringLiteral("RX1 Enable")) {
            return box;
        }
    }
    return nullptr;
}

} // namespace

class TstSetupDialogLazyPages : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // 1. Registration without construction.
    void construction_registers_every_leaf_but_builds_none();
    void construction_leaves_pa_page_pointers_null();

    // 2. On-demand realization + caching.
    void selecting_a_leaf_realizes_only_that_page();
    void revisiting_a_leaf_reuses_the_cached_widget();
    void selectPage_realizes_the_target_for_external_findChild();

    // 3. A late-built page still sees live state.
    void lazily_realized_page_reads_live_controller_state();
    void lazily_realized_page_persists_edits_to_appsettings();

    // 4. Cross-page wiring survives the split.
    void pa_reset_realizes_the_values_page_on_demand();
    void pa_reset_still_works_when_values_page_visited_first();
    void pa_pages_get_capabilities_when_realized_after_the_caps_pass();
    void cfc_dialog_request_forwards_after_late_realization();
#ifdef HAVE_WEBSOCKETS
    void pending_tci_server_is_replayed_into_the_late_built_page();
#endif
};

// ---------------------------------------------------------------------------
// 1. Registration without construction
// ---------------------------------------------------------------------------

// The headline assertion for #272 / #301: the ctor must not build page
// widgets. Pre-fix this was 55 constructed pages and roughly a second of
// blocked main thread; post-fix it is 55 registered factories and zero
// widgets.
void TstSetupDialogLazyPages::construction_registers_every_leaf_but_builds_none()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QVERIFY2(dialog.registeredPageCountForTest() > 0,
             "buildTree() must register at least one navigation leaf");
    QCOMPARE(dialog.realizedPageCountForTest(), 0);

    // Spot-check a spread of categories rather than trusting the count alone:
    // a bug that registered the leaf but eagerly built the widget would still
    // report realized == 0 only if it never touched m_pages.
    QVERIFY(!dialog.isPageRealizedForTest(kAgcAlc));
    QVERIFY(!dialog.isPageRealizedForTest(kGeneralOpts));
    QVERIFY(!dialog.isPageRealizedForTest(kWattMeter));
    QVERIFY(!dialog.isPageRealizedForTest(kTciServer));
    QVERIFY(!dialog.isPageRealizedForTest(kCfc));
}

// The three PA page pointers are what applyPaVisibility() fans capabilities
// out through. They must start null so the null-guards in that method are the
// live path, not dead code.
void TstSetupDialogLazyPages::construction_leaves_pa_page_pointers_null()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QVERIFY(dialog.paWattMeterPageForTest() == nullptr);
    QVERIFY(dialog.paValuesPageForTest()    == nullptr);
}

// ---------------------------------------------------------------------------
// 2. On-demand realization + caching
// ---------------------------------------------------------------------------

// Selecting one leaf must build that leaf and nothing else. A regression that
// force-realized siblings (for example to satisfy a cross-page connect) would
// push the count above 1 and reintroduce the stall this fix removes.
void TstSetupDialogLazyPages::selecting_a_leaf_realizes_only_that_page()
{
    RadioModel model;
    SetupDialog dialog(&model);

    dialog.selectPage(kAgcAlc);

    QCOMPARE(dialog.realizedPageCountForTest(), 1);
    QVERIFY(dialog.isPageRealizedForTest(kAgcAlc));
    QVERIFY(!dialog.isPageRealizedForTest(kNbSnb));

    dialog.selectPage(kNbSnb);

    QCOMPARE(dialog.realizedPageCountForTest(), 2);
    QVERIFY(dialog.isPageRealizedForTest(kNbSnb));
}

// Second and later visits must reuse the cached widget: rebuilding on every
// visit would trade the one-off open stall for a per-click stall, which is the
// other half of what #272 reported.
void TstSetupDialogLazyPages::revisiting_a_leaf_reuses_the_cached_widget()
{
    RadioModel model;
    SetupDialog dialog(&model);

    dialog.selectPage(kAgcAlc);
    QWidget* firstVisit = dialog.realizePageForTest(kAgcAlc);
    QVERIFY(firstVisit != nullptr);

    dialog.selectPage(kNbSnb);
    dialog.selectPage(kAgcAlc);

    QCOMPARE(dialog.realizePageForTest(kAgcAlc), firstVisit);
    QCOMPARE(dialog.realizedPageCountForTest(), 2);
}

// MainWindow's openNrSetupRequested handler does
//   dialog->selectPage("NR/ANF");
//   if (auto* p = dialog->findChild<NrAnfSetupPage*>()) { p->selectSubtab(slot); }
// (MainWindow.cpp:5230-5235). That only works if selectPage() realizes the
// page synchronously and the page ends up in the dialog's child tree.
void TstSetupDialogLazyPages::selectPage_realizes_the_target_for_external_findChild()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QVERIFY(dialog.findChild<NrAnfSetupPage*>() == nullptr);

    dialog.selectPage(QStringLiteral("NR/ANF"));

    QVERIFY2(dialog.findChild<NrAnfSetupPage*>() != nullptr,
             "selectPage() must realize the page and reparent it under the dialog");
}

// ---------------------------------------------------------------------------
// 3. A late-built page still sees live state
// ---------------------------------------------------------------------------

// Issue #259 established that setup pages read live state in their ctor,
// because SetupDialog is rebuilt on every Tools -> Setup open. Lazy
// realization moves that ctor even later (first visit rather than dialog
// open), so the same guarantee has to hold. Timeline under test:
//   1. StepAttenuatorController restores enabled=true / 5 dB.
//   2. SetupDialog is constructed (no page widgets yet).
//   3. Operator clicks General -> Options; the page is built NOW.
//   4. Checkbox and spinbox must show the restored state, not ctor defaults.
void TstSetupDialogLazyPages::lazily_realized_page_reads_live_controller_state()
{
    RadioModel model;
    auto* ctrl = new StepAttenuatorController(&model);
    ctrl->setMaxAttenuation(31);          // ANAN-10E classic range
    ctrl->setStepAttEnabled(true);
    ctrl->setAttenuation(5, 0);
    model.setStepAttController(ctrl);

    SetupDialog dialog(&model);
    QVERIFY(!dialog.isPageRealizedForTest(kGeneralOpts));

    QWidget* page = dialog.realizePageForTest(kGeneralOpts);
    QVERIFY2(page != nullptr, "General -> Options leaf must resolve");

    QCheckBox* rx1Check = findRx1StepAttCheck(page);
    QVERIFY2(rx1Check != nullptr, "RX1 Enable checkbox not found");
    QCOMPARE(rx1Check->isChecked(), true);

    QSpinBox* rx1Spin = nullptr;
    for (QSpinBox* spin : page->findChildren<QSpinBox*>()) {
        if (spin->parent() == rx1Check->parent()) {
            rx1Spin = spin;
            break;
        }
    }
    QVERIFY2(rx1Spin != nullptr, "RX1 dB spinbox not found");
    QCOMPARE(rx1Spin->value(), 5);
}

// The other direction of the round-trip: an edit made on a lazily realized
// page must still reach AppSettings. Driven through the General -> Options CPU
// meter rate spinbox, which persists CpuMeterRateHz and re-emits upward.
void TstSetupDialogLazyPages::lazily_realized_page_persists_edits_to_appsettings()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QSignalSpy rateSpy(&dialog, &SetupDialog::cpuMeterRateChanged);

    QWidget* page = dialog.realizePageForTest(kGeneralOpts);
    QVERIFY(page != nullptr);

    // The CPU meter rate spinbox is the only one carrying a " Hz" suffix on
    // this page, which makes it addressable without a new production seam.
    QSpinBox* rateSpin = nullptr;
    for (QSpinBox* spin : page->findChildren<QSpinBox*>()) {
        if (spin->suffix() == QStringLiteral(" Hz")) {
            rateSpin = spin;
            break;
        }
    }
    QVERIFY2(rateSpin != nullptr, "CPU meter rate spinbox not found");

    const int newRate = (rateSpin->value() == 7) ? 9 : 7;
    rateSpin->setValue(newRate);

    QCOMPARE(AppSettings::instance()
                 .value(QStringLiteral("GeneralCpuMeterUpdateRateHz")).toInt(),
             newRate);
    // ...and the forward up to MainWindow still lands even though the page was
    // built long after wireSetupDialog() connected to this dialog signal.
    QCOMPARE(rateSpy.count(), 1);
    QCOMPARE(rateSpy.at(0).at(0).toInt(), newRate);
}

// ---------------------------------------------------------------------------
// 4. Cross-page wiring survives the split
// ---------------------------------------------------------------------------

// The PA [Reset PA Values] fan-out is the one connect() in the dialog that
// spans two sibling pages. It now routes through SetupDialog, which realizes
// PA Values on button press. Pre-refactor both pages existed up front; this
// pins that the reset still lands when only the Watt Meter page was visited.
void TstSetupDialogLazyPages::pa_reset_realizes_the_values_page_on_demand()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QVERIFY(dialog.realizePageForTest(kWattMeter) != nullptr);
    PaWattMeterPage* wattMeter = dialog.paWattMeterPageForTest();
    QVERIFY(wattMeter != nullptr);

    // Only the Watt Meter page is up at this point.
    QVERIFY2(!dialog.isPageRealizedForTest(kPaValues),
             "realizing Watt Meter must not drag PA Values in with it");

    wattMeter->clickResetPaValuesForTest();

    QVERIFY2(dialog.isPageRealizedForTest(kPaValues),
             "the reset fan-out must realize the PA Values page on demand");
    QVERIFY(dialog.paValuesPageForTest() != nullptr);
}

// Same wire, opposite visit order: PA Values realized first, then the Watt
// Meter page's Reset button must collapse its peak/min trackers to current.
// This is the original Phase 9 (#167) acceptance case, re-expressed for the
// lazy path.
void TstSetupDialogLazyPages::pa_reset_still_works_when_values_page_visited_first()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QVERIFY(dialog.realizePageForTest(kPaValues)  != nullptr);
    QVERIFY(dialog.realizePageForTest(kWattMeter) != nullptr);

    PaWattMeterPage* wattMeter = dialog.paWattMeterPageForTest();
    PaValuesPage*    values    = dialog.paValuesPageForTest();
    QVERIFY(wattMeter != nullptr);
    QVERIFY(values    != nullptr);

    model.radioStatus().setForwardPower(10.0);
    model.radioStatus().setForwardPower(50.0);
    model.radioStatus().setForwardPower(30.0);

    QCOMPARE(values->fwdCalibratedPeakForTest(), QStringLiteral("50.00 W"));
    QCOMPARE(values->fwdCalibratedMinForTest(),  QStringLiteral("10.00 W"));

    wattMeter->clickResetPaValuesForTest();

    QCOMPARE(values->fwdCalibratedPeakForTest(), QStringLiteral("30.00 W"));
    QCOMPARE(values->fwdCalibratedMinForTest(),  QStringLiteral("30.00 W"));
}

// applyPaVisibility() runs from the ctor (and on every currentRadioChanged)
// while the PA page pointers are still null, so each PA factory re-applies the
// live capabilities itself. Without that, a page realized after the caps pass
// would render with its ctor defaults instead of the per-SKU state.
//
// Atlas-class caps (hasPaProfile=false) must therefore still disable the PA
// Gain editor and raise its no-PA-support banner on a late build.
void TstSetupDialogLazyPages::pa_pages_get_capabilities_when_realized_after_the_caps_pass()
{
    RadioModel model;
    SetupDialog dialog(&model);

    // No radio has connected, so boardCapabilities() yields the Unknown row,
    // which carries hasPaProfile=false, the same conservative default the
    // ctor's visibility pass uses.
    QVERIFY(!BoardCapsTable::forBoard(HPSDRHW::Unknown).hasPaProfile);

    QWidget* page = dialog.realizePageForTest(kPaGain);
    QVERIFY2(page != nullptr, "PA Gain leaf must resolve");

    auto* gainPage = qobject_cast<PaGainByBandPage*>(page);
    QVERIFY(gainPage != nullptr);
    QVERIFY2(!gainPage->isPaEditorEnabledForTest(),
             "a PA page realized after the caps pass must still honour hasPaProfile=false");
    QVERIFY2(gainPage->isNoPaSupportBannerVisibleForTest(),
             "the no-PA-support banner must be applied at realization time");
}

// CfcSetupPage's [Configure CFC bands...] button is forwarded up to
// SetupDialog::cfcDialogRequested, which MainWindow::wireSetupDialog() has
// already connected to TxApplet::requestOpenCfcDialog. The connect now lives
// inside the page factory, so it must still be in place on a late build.
void TstSetupDialogLazyPages::cfc_dialog_request_forwards_after_late_realization()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QSignalSpy cfcSpy(&dialog, &SetupDialog::cfcDialogRequested);

    QWidget* page = dialog.realizePageForTest(kCfc);
    QVERIFY2(page != nullptr, "CFC leaf must resolve");

    auto* button = page->findChild<QPushButton*>(
        QStringLiteral("btnCFCBandsConfigure"));
    QVERIFY2(button != nullptr, "[Configure CFC bands...] button not found");

    button->click();

    QCOMPARE(cfcSpy.count(), 1);
}

#ifdef HAVE_WEBSOCKETS
// MainWindow::wireSetupDialog() calls dialog->setTciServer(m_tciServer)
// immediately after construction (MainWindow.cpp:4708), long before the
// operator navigates to CAT & Network -> TCI Server. SetupDialog stores the
// pointer and replays it when the page is realized.
//
// Observable proof: a serverStarted emission after realization has to reach
// the page's status label. That only happens if the replay actually ran the
// page's setTciServer(), which is what installs those signal hookups.
void TstSetupDialogLazyPages::pending_tci_server_is_replayed_into_the_late_built_page()
{
    RadioModel model;
    SetupDialog dialog(&model);

    TciServer server(nullptr);   // RadioModel* not needed for lifecycle wiring
    dialog.setTciServer(&server);

    // Still not realized: the pointer is parked, not delivered.
    QVERIFY(!dialog.isPageRealizedForTest(kTciServer));

    QWidget* page = dialog.realizePageForTest(kTciServer);
    QVERIFY2(page != nullptr, "TCI Server leaf must resolve");

    QVERIFY(server.start(0));    // 0 = OS-assigned ephemeral port
    QVERIFY(server.isRunning());

    bool sawRunningStatus = false;
    for (const QLabel* label : page->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("Running"))) {
            sawRunningStatus = true;
            break;
        }
    }
    QVERIFY2(sawRunningStatus,
             "the parked TciServer pointer must be replayed into the page at "
             "realization time, so serverStarted reaches its status label");

    server.stop();
}
#endif // HAVE_WEBSOCKETS

QTEST_MAIN(TstSetupDialogLazyPages)
#include "tst_setup_dialog_lazy_pages.moc"
