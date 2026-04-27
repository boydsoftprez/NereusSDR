// tests/tst_general_options_page_rx_only.cpp  (NereusSDR)
//
// Phase 3M-1a G.2 — Receive Only checkbox visibility wired from
// BoardCapabilities::isRxOnlySku (NereusSDR-original).
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies:
//   1. Default (null model): checkbox is hidden.
//   2. Standard board (HermesLite, isRxOnlySku=false): checkbox is hidden.
//   3. RX-only board (HermesLiteRxOnly, isRxOnlySku=true): checkbox is visible.
//   4. setReceiveOnlyVisible(true/false) round-trip works independently of caps.
//
// Setup note: HermesLiteRxOnly caps were added in Phase 3M-0 Task 1 and are
// the only board in the caps table with isRxOnlySku=true.

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

    // ── 1. null model: checkbox hidden ───────────────────────────────────────

    void nullModel_rxOnlyCheckbox_isHiddenByDefault()
    {
        // From Thetis setup.designer.cs:8535-8544 [v2.10.3.13] — Visible=false.
        // Even without a RadioModel the checkbox must be hidden.
        GeneralOptionsPage page(/*model=*/nullptr);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        // Use isHidden() — checks the explicit hidden flag regardless of parent
        // widget show state.  isVisible() is always false for unshown top-levels.
        QVERIFY2(chk->isHidden(), "checkbox must be hidden when model is null");
    }

    // ── 2. isRxOnlySku=false: checkbox hidden ────────────────────────────────

    void standardCaps_rxOnlyCheckbox_isHidden()
    {
        // boardCapabilities().isRxOnlySku == false → checkbox stays hidden.
        RadioModel model;
        // Default RadioModel has no board set → boardCapabilities() returns
        // Unknown caps which have isRxOnlySku=false.  No override needed.
        GeneralOptionsPage page(&model);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        QVERIFY2(chk->isHidden(),
                 "checkbox must be hidden when isRxOnlySku is false");
    }

    // ── 3. isRxOnlySku=true: checkbox visible ────────────────────────────────

    void rxOnlyCaps_rxOnlyCheckbox_isVisible()
    {
        // Inject isRxOnlySku=true via setCapsRxOnlyForTest (3M-1a G.2 test hook).
        // HermesLiteRxOnly has no HPSDRModel entry so setBoardForTest cannot
        // reach its caps; this hook is the correct seam.
        // Cite: BoardCapabilities::isRxOnlySku (NereusSDR-original, Phase 3M-0 Task 1).
        RadioModel model;
        model.setCapsRxOnlyForTest(true);
        GeneralOptionsPage page(&model);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        QVERIFY2(!chk->isHidden(),
                 "checkbox must not be hidden when isRxOnlySku is true");
    }

    // ── 4. setReceiveOnlyVisible round-trip ───────────────────────────────────

    void setReceiveOnlyVisible_roundTrip()
    {
        GeneralOptionsPage page(/*model=*/nullptr);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");

        // Initially hidden (explicit setVisible(false) in buildHardwareConfigGroup).
        QVERIFY(chk->isHidden());

        // Show it.
        page.setReceiveOnlyVisible(true);
        QVERIFY2(!chk->isHidden(), "setReceiveOnlyVisible(true) must un-hide checkbox");

        // Hide it again.
        page.setReceiveOnlyVisible(false);
        QVERIFY2(chk->isHidden(), "setReceiveOnlyVisible(false) must re-hide checkbox");
    }

    // ── 5. reconnect simulation: currentRadioChanged drives visibility ────────
    // Spec reviewer gap (G.2 fixup): the named-slot connection must propagate
    // live cap changes — i.e. a reconnect to a different board type (e.g.
    // standard board → RX-only board) updates the checkbox without reopening Setup.

    void reconnectChange_rxOnlyCheckbox_visibilityUpdates()
    {
        // Start with a non-RX-only board: checkbox must be hidden on construction.
        RadioModel model;
        model.setCapsRxOnlyForTest(false);
        GeneralOptionsPage page(&model);

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
        QVERIFY2(chk, "chkGeneralRXOnly not found");
        QVERIFY2(chk->isHidden(),
                 "checkbox must be hidden on construction when isRxOnlySku=false");

        // Simulate reconnect to an RX-only board: update caps then emit the signal.
        model.setCapsRxOnlyForTest(true);
        model.emitCurrentRadioChangedForTest();
        QApplication::processEvents();

        QVERIFY2(!chk->isHidden(),
                 "checkbox must be visible after currentRadioChanged fires with isRxOnlySku=true");

        // Reverse: simulate reconnect back to a full-TX board.
        model.setCapsRxOnlyForTest(false);
        model.emitCurrentRadioChangedForTest();
        QApplication::processEvents();

        QVERIFY2(chk->isHidden(),
                 "checkbox must be hidden after currentRadioChanged fires with isRxOnlySku=false");
    }
};

QTEST_MAIN(TestGeneralOptionsPageRxOnly)
#include "tst_general_options_page_rx_only.moc"
