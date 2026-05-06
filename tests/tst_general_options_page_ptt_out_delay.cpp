// tests/tst_general_options_page_ptt_out_delay.cpp  (NereusSDR)
//
// radio-mic-input Task 23 — PTT Out Delay spinbox on
// Setup -> General -> Options.
//
// no-port-check: test fixture; no Thetis logic ported here.
//
// Verifies:
//   1. Spinbox reflects TransmitModel::pttOutDelayMs at construction time.
//   2. Editing the spinbox pushes the new value to TransmitModel.
//   3. External TransmitModel::setPttOutDelayMs() updates the spinbox UI
//      (covers persistence-load and cross-page sync).
//   4. Spinbox honors the Thetis 0..500 ms range.
//
// Thetis source for the parity:
//   setup.cs:14302-14305 [v2.10.3.13+501e3f51] — udGenPTTOutDelay_ValueChanged.
//   setup.designer.cs:9176-9204 [v2.10.3.13+501e3f51] — designer Min=0 Max=500.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestGeneralOptionsPagePttOutDelay : public QObject
{
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

    // ── 1. spinbox reflects model at construction ────────────────────────────

    void spinboxReflectsModel()
    {
        RadioModel rm;
        rm.transmitModel().setPttOutDelayMs(150);
        GeneralOptionsPage page(&rm);
        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("pttOutDelaySpin"));
        QVERIFY2(spin != nullptr, "pttOutDelaySpin must exist");
        QCOMPARE(spin->value(), 150);
    }

    // ── 2. spinbox edit pushes to model ──────────────────────────────────────

    void spinboxEditPushesToModel()
    {
        RadioModel rm;
        GeneralOptionsPage page(&rm);
        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("pttOutDelaySpin"));
        QVERIFY2(spin != nullptr, "pttOutDelaySpin must exist");
        spin->setValue(200);
        QCOMPARE(rm.transmitModel().pttOutDelayMs(), 200);
    }

    // ── 3. external model change updates the spinbox ─────────────────────────

    void modelChangeUpdatesSpinbox()
    {
        RadioModel rm;
        GeneralOptionsPage page(&rm);
        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("pttOutDelaySpin"));
        QVERIFY2(spin != nullptr, "pttOutDelaySpin must exist");
        rm.transmitModel().setPttOutDelayMs(75);
        QCOMPARE(spin->value(), 75);
    }

    // ── 4. spinbox range matches Thetis designer (0..500 ms) ─────────────────

    void spinboxRangeMatchesThetis()
    {
        RadioModel rm;
        GeneralOptionsPage page(&rm);
        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("pttOutDelaySpin"));
        QVERIFY2(spin != nullptr, "pttOutDelaySpin must exist");
        QCOMPARE(spin->minimum(), 0);
        QCOMPARE(spin->maximum(), 500);
    }
};

QTEST_MAIN(TestGeneralOptionsPagePttOutDelay)
#include "tst_general_options_page_ptt_out_delay.moc"
