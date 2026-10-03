// tests/tst_general_options_page_rx_only.cpp  (NereusSDR)
//
// Phase 3M-1a G.2: Receive Only checkbox wired from
// BoardCapabilities::isRxOnlySku (NereusSDR-original).
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Task 16 (receiver and transmit gaps plan, 2026-09-25) replaced the
// contract. The box used to be hidden except on the HL2 receive-only kit;
// it is now shown on every radio (the operator, 2026-09-25: a control that
// cannot run is shown disabled with its reason, never hidden), and on the
// kit it is checked and disabled with the reason. tst_receive_only covers
// the gate behind it.
//
// Verifies:
//   1. Default (null model): checkbox is shown and enabled.
//   2. Standard board (isRxOnlySku=false): checkbox is shown and enabled.
//   3. RX-only board (isRxOnlySku=true): checkbox is shown, checked and
//      disabled with the reason.
//   4. A reconnect to a different board updates it without reopening Setup.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>

#include "core/AppSettings.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestGeneralOptionsPageRxOnly : public QObject
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

    // ── 1. null model: checkbox shown ────────────────────────────────────────

    void nullModel_rxOnlyCheckbox_isShown()
    {
        GeneralOptionsPage page(/*model=*/nullptr);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        // isHidden() checks the explicit hidden flag regardless of parent
        // widget show state (isVisible() is always false for unshown pages).
        QVERIFY2(!chk->isHidden(), "checkbox must be shown when model is null");
        QVERIFY(chk->isEnabled());
    }

    // ── 2. isRxOnlySku=false: checkbox shown and enabled ─────────────────────

    void standardCaps_rxOnlyCheckbox_isShownAndEnabled()
    {
        RadioModel model;
        GeneralOptionsPage page(&model);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        QVERIFY2(!chk->isHidden(), "checkbox must be shown when isRxOnlySku is false");
        QVERIFY(chk->isEnabled());
        QVERIFY(!chk->isChecked());
    }

    // ── 3. isRxOnlySku=true: checked and disabled with the reason ────────────

    void rxOnlyCaps_rxOnlyCheckbox_isCheckedAndDisabled()
    {
        // Inject isRxOnlySku=true via setCapsRxOnlyForTest (3M-1a G.2 test hook).
        // Cite: BoardCapabilities::isRxOnlySku (NereusSDR-original, Phase 3M-0 Task 1).
        RadioModel model;
        model.setCapsRxOnlyForTest(true);
        GeneralOptionsPage page(&model);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        QVERIFY2(!chk->isHidden(), "checkbox must be shown when isRxOnlySku is true");
        QVERIFY(chk->isChecked());
        QVERIFY(!chk->isEnabled());
        QCOMPARE(chk->toolTip(), RadioModel::rxOnlyForcedReason());
    }

    // ── 4. reconnect simulation: currentRadioChanged drives the box ──────────

    void reconnectChange_rxOnlyCheckbox_follows()
    {
        RadioModel model;
        model.setCapsRxOnlyForTest(false);
        GeneralOptionsPage page(&model);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        QVERIFY(!chk->isHidden());
        QVERIFY(chk->isEnabled());

        // Reconnect to an RX-only board.
        model.setCapsRxOnlyForTest(true);
        model.emitCurrentRadioChangedForTest();
        QApplication::processEvents();
        QVERIFY(!chk->isHidden());
        QVERIFY(chk->isChecked());
        QVERIFY(!chk->isEnabled());

        // Back to a full-TX board.
        model.setCapsRxOnlyForTest(false);
        model.emitCurrentRadioChangedForTest();
        QApplication::processEvents();
        QVERIFY(!chk->isHidden());
        QVERIFY(!chk->isChecked());
        QVERIFY(chk->isEnabled());
    }
};

QTEST_MAIN(TestGeneralOptionsPageRxOnly)
#include "tst_general_options_page_rx_only.moc"
