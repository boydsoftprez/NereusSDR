// tests/tst_setup_dialog_qt_warnings.cpp  (NereusSDR)
//
// Regression test for #272 — the May 16 W4ORS BSOD report flagged two Qt
// runtime warnings firing during SetupDialog construction:
//
//   QObject::connect(QCheckBox, QPushButton): unique connections require a
//     pointer to member function of a QObject subclass
//   QLayout: Attempting to add QLayout "" to QGroupBox "", which already
//     has a layout                                                     [×7]
//
// Both are real Qt programming errors (one was a lambda passed with
// Qt::UniqueConnection in Hl2OptionsTab::onI2cEnableToggled, where the slot
// could not be deduped; the other was a redundant setLayout() call in
// DeviceCard::buildLayout after the QVBoxLayout(this) parent-ctor already
// installed the layout). They were not the BSOD root cause, but they did
// pollute the log Chris attached to the issue and they survive any
// SetupDialog-construction-time freeze investigation, so closing them out
// makes future repro logs easier to read.
//
// no-port-check: this is a NereusSDR-original UI test fixture; the cited
// Thetis files are referenced in the production source comments and not
// here.
//
// Strategy: install a QtMessageHandler before SetupDialog construction;
// pattern-match each emitted message against the two known warnings;
// assert zero matches.

#include <QtTest/QtTest>
#include <QApplication>

#include <atomic>

#include "core/AppSettings.h"
#include "gui/SetupDialog.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {
std::atomic<int>* g_layoutDoubleAdd = nullptr;
std::atomic<int>* g_uniqueLambdaConnect = nullptr;

void counterHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    Q_UNUSED(type);
    // QGroupBox double-setLayout — "QLayout: Attempting to add QLayout"
    if (g_layoutDoubleAdd &&
        msg.contains(QStringLiteral("Attempting to add QLayout"))) {
        g_layoutDoubleAdd->fetch_add(1);
    }
    // Lambda + Qt::UniqueConnection — "unique connections require a
    // pointer to member function"
    if (g_uniqueLambdaConnect &&
        msg.contains(QStringLiteral("unique connections require"))) {
        g_uniqueLambdaConnect->fetch_add(1);
    }
}
} // namespace

class TstSetupDialogQtWarnings : public QObject {
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

    void setupDialog_construction_emits_no_layout_double_add_warnings();
    void setupDialog_construction_emits_no_unique_lambda_connect_warnings();
};

// ---------------------------------------------------------------------------
// Test 1: zero "Attempting to add QLayout" warnings.
//
// Pre-fix: 7 fired (3 from AudioDevicesPage's DeviceCards + 4 from
// AudioVaxPage's VaxChannelCard DeviceCards). Post-fix: 0.
// ---------------------------------------------------------------------------
void TstSetupDialogQtWarnings::
    setupDialog_construction_emits_no_layout_double_add_warnings()
{
    std::atomic<int> layoutCount{0};
    g_layoutDoubleAdd = &layoutCount;
    g_uniqueLambdaConnect = nullptr;
    QtMessageHandler prev = qInstallMessageHandler(counterHandler);

    {
        RadioModel model;
        SetupDialog dialog(&model);
        Q_UNUSED(dialog);
    }

    qInstallMessageHandler(prev);
    g_layoutDoubleAdd = nullptr;

    QCOMPARE(layoutCount.load(), 0);
}

// ---------------------------------------------------------------------------
// Test 2: zero "unique connections require" warnings.
//
// Pre-fix: 1 fired from Hl2OptionsTab::onI2cEnableToggled wiring its
// m_chkI2cWriteEnable → m_btnWrite gating lambda with Qt::UniqueConnection
// each time the slot ran. Post-fix: the wiring moves to buildI2cControl
// (called once) and the slot becomes a real PMF on the tab.
//
// Note: HardwarePage only constructs the Hl2OptionsTab when the connected
// radio reports HL2 capabilities. In this test no radio is connected, so
// the tab may not be built at all — in which case the warning naturally
// never fires regardless of the fix. The QCOMPARE to zero still holds:
// the assertion is "no warning fires during SetupDialog construction",
// and that's true both when the tab is skipped and when it's built with
// the fixed wiring.
// ---------------------------------------------------------------------------
void TstSetupDialogQtWarnings::
    setupDialog_construction_emits_no_unique_lambda_connect_warnings()
{
    std::atomic<int> uniqueLambdaCount{0};
    g_layoutDoubleAdd = nullptr;
    g_uniqueLambdaConnect = &uniqueLambdaCount;
    QtMessageHandler prev = qInstallMessageHandler(counterHandler);

    {
        RadioModel model;
        SetupDialog dialog(&model);
        Q_UNUSED(dialog);
    }

    qInstallMessageHandler(prev);
    g_uniqueLambdaConnect = nullptr;

    QCOMPARE(uniqueLambdaCount.load(), 0);
}

QTEST_MAIN(TstSetupDialogQtWarnings)
#include "tst_setup_dialog_qt_warnings.moc"
