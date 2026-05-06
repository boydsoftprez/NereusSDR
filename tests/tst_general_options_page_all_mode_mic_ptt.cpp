// tests/tst_general_options_page_all_mode_mic_ptt.cpp  (NereusSDR)
//
// radio-mic-input Task 23 — All-Mode Mic PTT checkbox on
// Setup -> General -> Options.
//
// no-port-check: test fixture; no Thetis logic ported here.
//
// Verifies:
//   1. Checkbox reflects TransmitModel::allModeMicPTT at construction time.
//   2. Clicking the checkbox pushes the new value to TransmitModel.
//   3. External TransmitModel::setAllModeMicPTT() updates the checkbox UI
//      (covers persistence-load and cross-page sync).
//
// Thetis source for the parity:
//   setup.cs:12127-12130 [v2.10.3.13+501e3f51] — chkGenAllModeMicPTT_CheckedChanged.
//   setup.designer.cs:9217-9226 [v2.10.3.13+501e3f51] — designer (All Mode Mic PTT).

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>

#include "core/AppSettings.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestGeneralOptionsPageAllModeMicPtt : public QObject
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

    // ── 1. checkbox reflects model at construction ───────────────────────────

    void checkboxReflectsModel()
    {
        RadioModel rm;
        rm.transmitModel().setAllModeMicPTT(true);
        GeneralOptionsPage page(&rm);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("allModeMicPttCheck"));
        QVERIFY2(chk != nullptr, "allModeMicPttCheck must exist");
        QCOMPARE(chk->isChecked(), true);
    }

    // ── 2. checkbox click pushes to model ────────────────────────────────────

    void checkboxClickPushesToModel()
    {
        RadioModel rm;
        GeneralOptionsPage page(&rm);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("allModeMicPttCheck"));
        QVERIFY2(chk != nullptr, "allModeMicPttCheck must exist");
        chk->setChecked(true);
        QCOMPARE(rm.transmitModel().allModeMicPTT(), true);
    }

    // ── 3. external model change updates the checkbox ────────────────────────

    void modelChangeUpdatesCheckbox()
    {
        RadioModel rm;
        GeneralOptionsPage page(&rm);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("allModeMicPttCheck"));
        QVERIFY2(chk != nullptr, "allModeMicPttCheck must exist");
        rm.transmitModel().setAllModeMicPTT(true);
        QCOMPARE(chk->isChecked(), true);
    }
};

QTEST_MAIN(TestGeneralOptionsPageAllModeMicPtt)
#include "tst_general_options_page_all_mode_mic_ptt.moc"
