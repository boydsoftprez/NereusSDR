// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test file. SpotHubDialog itself is
// an AetherSDR port (DxClusterDialog [@0cd4559]), but the central
// "Settings" tab pinned here is a NereusSDR addition. Upstream has no
// equivalent: AetherSDR scatters callsign + grid identity fields across
// each per-source tab.
//
// NereusSDR - SpotHubDialog Settings tab (operator identity).
//
// User-reported UX bug (2026-05-11): each spot client needed callsign +
// grid, but the inputs were scattered across the Cluster / RBN / PSK
// Reporter tabs, and the FreeDV Reporter tab had NO callsign / grid
// input at all. Result: "FreeDV Reporter is not connecting" because the
// auto-start fired with an empty identity. Fix is a new Settings tab
// (first position) where the operator enters callsign + grid + FreeDV
// status message once, and Save propagates to every per-source key
// (DxClusterCallsign / RbnCallsign / PskReporterCallsign /
// FreeDvReporter/Callsign / FreeDvReporter/GridSquare) plus the
// canonical User/Callsign + User/GridSquare keys for new code to read.
//
// What is verified:
//   * The dialog now has 10 tabs (was 9) and Settings is the first one.
//   * Save button writes to the canonical User/* + FreeDvReporter/Message
//     keys and ALSO propagates to the per-source legacy keys.
//   * Empty-callsign + invalid-grid validation blocks the write.
//   * FreeDV tab shows a status label that reads "Identity: <call> /
//     <grid>" when set or warns "No identity set" when missing.
//   * Per-source tabs (Cluster / RBN / PSK Reporter) fall back to
//     User/Callsign when their per-source key is empty.
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Initial commit. AI tooling:
//                                    Anthropic Claude Code.

#include <QtTest>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/FreeDVReporterClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/WsjtxClient.h"
#include "core/DxccColorProvider.h"
#include "gui/SpotHubDialog.h"
#include "models/SpotModel.h"
#include "models/SpotTableModel.h"

using namespace NereusSDR;

namespace {

AppSettings& testSettings()
{
    return AppSettings::instance();
}

// Construct a SpotHubDialog wired to fresh fixture clients. The caller
// owns the returned pointer.
SpotHubDialog* makeDialog()
{
    auto* cluster = new DxClusterClient;
    auto* rbn = new DxClusterClient;
    auto* wsjtx = new WsjtxClient;
    auto* spotCollector = new SpotCollectorClient;
    auto* pota = new PotaClient;
    auto* freedv = new FreeDVReporterClient;
    auto* psk = new PskReporterClient;
    auto* spots = new SpotModel;
    auto* spotTable = new SpotTableModel;
    auto* dxcc = new DxccColorProvider;

    return new SpotHubDialog(cluster, rbn, wsjtx, spotCollector, pota,
                             freedv, psk, spots, spotTable, dxcc, nullptr);
}

} // namespace

class TestSpotHubSettingsTab : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        testSettings().clear();
    }

    void cleanup()
    {
        testSettings().clear();
    }

    // ── Tab layout ─────────────────────────────────────────────────────

    void dialogNowHasTenTabs()
    {
        auto* dlg = makeDialog();
        auto* tabs = dlg->findChild<QTabWidget*>();
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 10);
        delete dlg;
    }

    void settingsTabFirstInOrder()
    {
        auto* dlg = makeDialog();
        auto* tabs = dlg->findChild<QTabWidget*>();
        QVERIFY(tabs != nullptr);
        QVERIFY2(tabs->tabText(0).contains("Settings", Qt::CaseInsensitive),
                 qPrintable(QString("expected Settings tab at index 0, got '%1'")
                                .arg(tabs->tabText(0))));
        delete dlg;
    }

    // ── Save round-trip ─────────────────────────────────────────────────

    void saveButtonPersistsToCanonicalKeys()
    {
        auto* dlg = makeDialog();
        auto* callEdit = dlg->findChild<QLineEdit*>("settingsCallEdit");
        auto* gridEdit = dlg->findChild<QLineEdit*>("settingsGridEdit");
        auto* msgEdit  = dlg->findChild<QLineEdit*>("settingsFreedvMsgEdit");
        auto* saveBtn  = dlg->findChild<QPushButton*>("settingsSaveBtn");
        QVERIFY(callEdit != nullptr);
        QVERIFY(gridEdit != nullptr);
        QVERIFY(msgEdit  != nullptr);
        QVERIFY(saveBtn  != nullptr);

        callEdit->setText("KG4VCF");
        gridEdit->setText("EM73");
        msgEdit->setText("73 from NereusSDR bench");
        saveBtn->click();

        QCOMPARE(testSettings().value("User/Callsign").toString(),
                 QString("KG4VCF"));
        QCOMPARE(testSettings().value("User/GridSquare").toString(),
                 QString("EM73"));
        QCOMPARE(testSettings().value("FreeDvReporter/Message").toString(),
                 QString("73 from NereusSDR bench"));
        delete dlg;
    }

    void saveButtonPropagatesToPerSourceKeys()
    {
        auto* dlg = makeDialog();
        auto* callEdit = dlg->findChild<QLineEdit*>("settingsCallEdit");
        auto* gridEdit = dlg->findChild<QLineEdit*>("settingsGridEdit");
        auto* saveBtn  = dlg->findChild<QPushButton*>("settingsSaveBtn");
        QVERIFY(callEdit != nullptr);
        QVERIFY(gridEdit != nullptr);
        QVERIFY(saveBtn  != nullptr);

        callEdit->setText("NOCALL");
        gridEdit->setText("AA00AA");
        saveBtn->click();

        // Canonical keys
        QCOMPARE(testSettings().value("User/Callsign").toString(),
                 QString("NOCALL"));
        QCOMPARE(testSettings().value("User/GridSquare").toString(),
                 QString("AA00AA"));
        // Per-source propagation
        QCOMPARE(testSettings().value("DxClusterCallsign").toString(),
                 QString("NOCALL"));
        QCOMPARE(testSettings().value("RbnCallsign").toString(),
                 QString("NOCALL"));
        // 2026-05-12 (PR #238 P2 PSK fix): PSK Reporter Save & Propagate
        // now writes the slash-key family RadioModel reads on
        // construction + session restore (PskReporter/Callsign,
        // PskReporter/GridSquare).  Old flat keys
        // PskReporterCallsign / PskReporterGrid were write-only
        // orphans nothing read.
        QCOMPARE(testSettings().value("PskReporter/Callsign").toString(),
                 QString("NOCALL"));
        QCOMPARE(testSettings().value("PskReporter/GridSquare").toString(),
                 QString("AA00AA"));
        QCOMPARE(testSettings().value("FreeDvReporter/Callsign").toString(),
                 QString("NOCALL"));
        QCOMPARE(testSettings().value("FreeDvReporter/GridSquare").toString(),
                 QString("AA00AA"));
        delete dlg;
    }

    // ── Validation ─────────────────────────────────────────────────────

    void saveButtonRejectsEmptyCallsign()
    {
        auto* dlg = makeDialog();
        auto* callEdit = dlg->findChild<QLineEdit*>("settingsCallEdit");
        auto* gridEdit = dlg->findChild<QLineEdit*>("settingsGridEdit");
        auto* saveBtn  = dlg->findChild<QPushButton*>("settingsSaveBtn");
        auto* errLabel = dlg->findChild<QLabel*>("settingsErrorLabel");
        QVERIFY(callEdit != nullptr);
        QVERIFY(gridEdit != nullptr);
        QVERIFY(saveBtn  != nullptr);
        QVERIFY(errLabel != nullptr);

        callEdit->setText("");
        gridEdit->setText("EM73");
        saveBtn->click();

        // No write should have happened.
        QVERIFY(!testSettings().contains("User/Callsign")
                || testSettings().value("User/Callsign").toString().isEmpty());
        // Error message must surface to the user.
        QVERIFY(!errLabel->text().isEmpty());
        delete dlg;
    }

    void saveButtonRejectsInvalidGrid()
    {
        auto* dlg = makeDialog();
        auto* callEdit = dlg->findChild<QLineEdit*>("settingsCallEdit");
        auto* gridEdit = dlg->findChild<QLineEdit*>("settingsGridEdit");
        auto* saveBtn  = dlg->findChild<QPushButton*>("settingsSaveBtn");
        auto* errLabel = dlg->findChild<QLabel*>("settingsErrorLabel");
        QVERIFY(callEdit != nullptr);
        QVERIFY(gridEdit != nullptr);
        QVERIFY(saveBtn  != nullptr);
        QVERIFY(errLabel != nullptr);

        callEdit->setText("KG4VCF");
        // 5-char grid is invalid (Maidenhead is 4 or 6).
        gridEdit->setText("EM73A");
        saveBtn->click();

        QVERIFY(!testSettings().contains("User/Callsign")
                || testSettings().value("User/Callsign").toString().isEmpty());
        QVERIFY(!errLabel->text().isEmpty());
        delete dlg;
    }

    // ── FreeDV tab identity status ─────────────────────────────────────

    void freeDvTabShowsIdentityFromSettings()
    {
        testSettings().setValue("User/Callsign", QStringLiteral("KG4VCF"));
        testSettings().setValue("User/GridSquare", QStringLiteral("EM73"));

        auto* dlg = makeDialog();
        auto* idLabel = dlg->findChild<QLabel*>("freedvIdentityLabel");
        QVERIFY(idLabel != nullptr);
        // Label text mentions both callsign and grid.
        QVERIFY2(idLabel->text().contains("KG4VCF"),
                 qPrintable(QString("expected KG4VCF in label, got: %1")
                                .arg(idLabel->text())));
        QVERIFY2(idLabel->text().contains("EM73"),
                 qPrintable(QString("expected EM73 in label, got: %1")
                                .arg(idLabel->text())));
        delete dlg;
    }

    void freeDvTabShowsWarningWhenIdentityEmpty()
    {
        // No User/Callsign or FreeDvReporter/Callsign set.
        auto* dlg = makeDialog();
        auto* idLabel = dlg->findChild<QLabel*>("freedvIdentityLabel");
        QVERIFY(idLabel != nullptr);
        // The warning text mentions "No identity" or "Settings tab".
        const QString text = idLabel->text();
        QVERIFY2(text.contains("No identity", Qt::CaseInsensitive)
                 || text.contains("Settings tab", Qt::CaseInsensitive),
                 qPrintable(QString("expected warning text, got: %1").arg(text)));
        delete dlg;
    }

    // ── Per-source tabs read User/Callsign as fallback ─────────────────

    void perSourceTabsFallBackToUserCallsign()
    {
        // User/Callsign is set; per-source keys are empty.
        testSettings().setValue("User/Callsign", QStringLiteral("N1ABC"));

        auto* dlg = makeDialog();
        // Cluster tab's callsign field should pre-populate with N1ABC.
        auto* clusterCall = dlg->findChild<QLineEdit*>("clusterCallEdit");
        QVERIFY(clusterCall != nullptr);
        QCOMPARE(clusterCall->text(), QString("N1ABC"));
        // RBN tab's callsign field already had its own fall-back to
        // DxClusterCallsign; with User/Callsign in play it should pick
        // up N1ABC as well.
        auto* rbnCall = dlg->findChild<QLineEdit*>("rbnCallEdit");
        QVERIFY(rbnCall != nullptr);
        QCOMPARE(rbnCall->text(), QString("N1ABC"));
        // PSK Reporter tab.
        auto* pskCall = dlg->findChild<QLineEdit*>("pskCallEdit");
        QVERIFY(pskCall != nullptr);
        QCOMPARE(pskCall->text(), QString("N1ABC"));
        delete dlg;
    }
};

QTEST_MAIN(TestSpotHubSettingsTab)
#include "tst_spothub_settings_tab.moc"
