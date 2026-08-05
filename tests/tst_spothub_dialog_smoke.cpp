// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Test constructs fixture clients and asserts the
// tab-strip contract; no callsigns or wire payloads are involved.
//
// NereusSDR - SpotHubDialog tab-strip smoke tests
//
// Phase 3J-2 Task F1 + F2 + F3 + F4. Pins the contract that
// SpotHubDialog constructs successfully when handed all 6
// spot-ingest clients + the SpotModel + the DxccColorProvider,
// that its top-level QTabWidget exposes exactly 10 tabs, that the
// tab labels appear in NereusSDR's adjusted order (Settings as the
// first tab; then Cluster, RBN, WSJT-X, SpotCollector, POTA, FreeDV,
// PSK Reporter, Spot List, Display from upstream), and (F2) that each
// per-source tab carries the uniform template: connection-control
// widgets, auto-start toggle, start/stop button, status line, and a
// raw-event console. WSJT-X carries the three filter checkboxes and
// four color picker buttons. PSK Reporter is NereusSDR-native and uses
// the same uniform shape. The Settings tab (first position) is a
// NereusSDR addition for central operator identity and is verified by
// `tst_spothub_settings_tab.cpp` rather than the per-source uniform
// template test.
//
// F3 extends the contract with the Spot List tab: a QTableView
// bound to BandFilterProxy(SpotTableModel), a row of 12 band-filter
// pill buttons (160m..2m), a row of 7 source-filter pill buttons
// (DX / RBN / JT / COL / POT / FDR / PSK), and a Clear button.
// Row double-click on a populated table emits tuneRequested(double).
//
// F4 extends the contract with the Display tab: a left column of
// 8 stat blocks (Total Spots / Unique Callsigns / Active Sources /
// cty.dat entries / ADIF QSOs / DXCC entities / New DXCC / New
// bands), a red "Clear All Spots" button (emits spotsClearedAll),
// and a right column of knobs ported verbatim from AetherSDR's
// standalone SpotSettingsDialog (Spots toggle / Memories toggle /
// Levels slider / Position slider / Font Size slider / Spot
// Lifetime slider / Override Colors toggle + swatch / Override BG
// + Auto toggles + swatch / BG Opacity slider). Every knob change
// writes to AppSettings and emits settingsChanged().

#include <QtTest>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QSlider>
#include <QTableView>
#include <QSignalSpy>

#include "gui/SpotHubDialog.h"
#include "core/DxClusterClient.h"
#include "core/DxSpot.h"
#include "core/WsjtxClient.h"
#include "core/SpotCollectorClient.h"
#include "core/PotaClient.h"
#include "core/FreeDVReporterClient.h"
#include "core/PskReporterClient.h"
#include "core/DxccColorProvider.h"
#include "models/BandFilterProxy.h"
#include "models/SpotModel.h"
#include "models/SpotTableModel.h"

using namespace NereusSDR;

class TestSpotHubDialogSmoke : public QObject {
    Q_OBJECT
private slots:
    void dialogConstructs();
    void hasTenTabs();
    void tabOrderMatchesAetherSdr();

    // F2: per-source tab content (uniform template).
    void clusterTabHasHostPortCall();
    void rbnTabHasHostPortCallRate();
    void wsjtxTabHasAddrPort();
    void wsjtxTabHasFilterCheckboxes();
    void wsjtxTabHasFourColorPickers();
    void spotCollectorTabHasPortSpin();
    void potaTabHasIntervalSpin();
    void freedvTabHasAutoStartAndConsole();
    void pskTabHasCallsignField();

    // F2 cross-cutting: every per-source tab carries the uniform
    // skeleton (auto-start toggle, start/stop button, status label,
    // raw-event console).
    void everySourceTabHasUniformTemplate();

    // F3: Spot List tab (QTableView + band pills + source pills +
    // Clear button + double-click-to-tune).
    void spotListTabHasTableView();
    void spotListTabHasTwelveBandPills();
    void spotListTabHasSevenSourcePills();
    void spotListBandPillTogglesProxyFilter();
    void spotListSourcePillTogglesProxyFilter();
    void doubleClickOnSpotRowEmitsTuneRequested();

    // F4: Display tab (stat blocks + Clear All Spots + knobs from
    // AetherSDR's standalone SpotSettingsDialog folded inline).
    void displayTabHasStatBlocks();
    void displayTabHasLevelsSlider();
    void displayTabHasLifetimeSlider();
    void displayTabHasOverrideColorButton();
    void displayTabHasClearAllSpotsButton();
    void clearAllButtonEmitsSignal();
    void knobChangeEmitsSettingsChanged();
};

static SpotHubDialog* makeDialog() {
    auto cluster = new DxClusterClient;
    auto rbn = new DxClusterClient;
    auto wsjtx = new WsjtxClient;
    auto spotCollector = new SpotCollectorClient;
    auto pota = new PotaClient;
    auto freedv = new FreeDVReporterClient;
    auto psk = new PskReporterClient;
    auto spots = new SpotModel;
    auto spotTable = new SpotTableModel;
    auto dxcc = new DxccColorProvider;

    return new SpotHubDialog(cluster, rbn, wsjtx, spotCollector, pota,
                             freedv, psk, spots, spotTable, dxcc, nullptr);
}

void TestSpotHubDialogSmoke::dialogConstructs() {
    auto* dlg = makeDialog();
    QVERIFY(dlg != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::hasTenTabs() {
    auto* dlg = makeDialog();
    auto* tabs = dlg->findChild<QTabWidget*>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 10);
    delete dlg;
}

void TestSpotHubDialogSmoke::tabOrderMatchesAetherSdr() {
    auto* dlg = makeDialog();
    auto* tabs = dlg->findChild<QTabWidget*>();
    QVERIFY(tabs != nullptr);
    // NereusSDR-native Settings tab is in position 0; the AetherSDR
    // upstream order (Cluster, RBN, WSJT, SpotCollector, POTA, FreeDV,
    // PSK, Spot List, Display) is preserved starting at index 1.
    QVERIFY(tabs->tabText(0).contains("Settings", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(1).contains("Cluster", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(2).contains("RBN", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(3).contains("WSJT", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(4).contains("SpotCollector", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(5).contains("POTA", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(6).contains("FreeDV", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(7).contains("PSK", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(8).contains("Spot List", Qt::CaseInsensitive));
    QVERIFY(tabs->tabText(9).contains("Display", Qt::CaseInsensitive));
    delete dlg;
}

void TestSpotHubDialogSmoke::clusterTabHasHostPortCall() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QLineEdit*>("clusterHostEdit") != nullptr);
    QVERIFY(dlg->findChild<QSpinBox*>("clusterPortSpin") != nullptr);
    QVERIFY(dlg->findChild<QLineEdit*>("clusterCallEdit") != nullptr);
    QVERIFY(dlg->findChild<QLineEdit*>("clusterCmdEdit") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("clusterSendBtn") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::rbnTabHasHostPortCallRate() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QLineEdit*>("rbnHostEdit") != nullptr);
    QVERIFY(dlg->findChild<QSpinBox*>("rbnPortSpin") != nullptr);
    QVERIFY(dlg->findChild<QLineEdit*>("rbnCallEdit") != nullptr);
    QVERIFY(dlg->findChild<QSpinBox*>("rbnRateSpin") != nullptr);
    QVERIFY(dlg->findChild<QLineEdit*>("rbnCmdEdit") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("rbnSendBtn") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::wsjtxTabHasAddrPort() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QLineEdit*>("wsjtxAddrEdit") != nullptr);
    QVERIFY(dlg->findChild<QSpinBox*>("wsjtxPortSpin") != nullptr);
    QVERIFY(dlg->findChild<QSlider*>("wsjtxLifeSlider") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::wsjtxTabHasFilterCheckboxes() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QCheckBox*>("wsjtxFilterCQ") != nullptr);
    QVERIFY(dlg->findChild<QCheckBox*>("wsjtxFilterPOTA") != nullptr);
    QVERIFY(dlg->findChild<QCheckBox*>("wsjtxFilterCallingMe") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::wsjtxTabHasFourColorPickers() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QPushButton*>("wsjtxColorCQ") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("wsjtxColorPOTA") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("wsjtxColorCallingMe") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("wsjtxColorDefault") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::spotCollectorTabHasPortSpin() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QSpinBox*>("scPortSpin") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::potaTabHasIntervalSpin() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QSpinBox*>("potaIntervalSpin") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("potaColorBtn") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::freedvTabHasAutoStartAndConsole() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QPushButton*>("freedvAutoStartBtn") != nullptr);
    QVERIFY(dlg->findChild<QPlainTextEdit*>("freedvConsole") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("freedvColorBtn") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::pskTabHasCallsignField() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QLineEdit*>("pskCallEdit") != nullptr);
    QVERIFY(dlg->findChild<QLineEdit*>("pskGridEdit") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("pskAutoStartBtn") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("pskStartBtn") != nullptr);
    QVERIFY(dlg->findChild<QLabel*>("pskStatusLabel") != nullptr);
    QVERIFY(dlg->findChild<QPlainTextEdit*>("pskConsole") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::everySourceTabHasUniformTemplate() {
    // For each per-source tab the uniform template requires:
    //   * an auto-start toggle button
    //   * a start/stop button
    //   * a status label
    //   * a raw-event console (QPlainTextEdit)
    static constexpr struct {
        const char* prefix;
    } sources[] = {
        {"cluster"},
        {"rbn"},
        {"wsjtx"},
        {"sc"},
        {"pota"},
        {"freedv"},
        {"psk"},
    };

    auto* dlg = makeDialog();
    for (const auto& src : sources) {
        QString autoBtn  = QString("%1AutoStartBtn").arg(src.prefix);
        QString autoBtn2 = QString("%1AutoConnectBtn").arg(src.prefix);
        // Cluster and RBN use "AutoConnect"; UDP / WebSocket sources use
        // "AutoStart". Accept either name to keep the uniform-template
        // assertion source-agnostic.
        QPushButton* autoButton =
            dlg->findChild<QPushButton*>(autoBtn);
        if (!autoButton)
            autoButton = dlg->findChild<QPushButton*>(autoBtn2);
        QVERIFY2(autoButton != nullptr,
                 qPrintable(QString("missing auto-toggle for %1").arg(src.prefix)));

        // Start/Stop button: name is `<prefix>StartBtn` for UDP/WS or
        // `<prefix>ConnectBtn` for telnet (cluster/rbn).
        QString startName = QString("%1StartBtn").arg(src.prefix);
        QString connName = QString("%1ConnectBtn").arg(src.prefix);
        QPushButton* startBtn = dlg->findChild<QPushButton*>(startName);
        if (!startBtn) {
            startBtn = dlg->findChild<QPushButton*>(connName);
        }
        QVERIFY2(startBtn != nullptr,
                 qPrintable(QString("missing start/connect button for %1").arg(src.prefix)));

        QString statusName = QString("%1StatusLabel").arg(src.prefix);
        QVERIFY2(dlg->findChild<QLabel*>(statusName) != nullptr,
                 qPrintable(QString("missing status label for %1").arg(src.prefix)));

        QString consoleName = QString("%1Console").arg(src.prefix);
        QVERIFY2(dlg->findChild<QPlainTextEdit*>(consoleName) != nullptr,
                 qPrintable(QString("missing console for %1").arg(src.prefix)));
    }
    delete dlg;
}

void TestSpotHubDialogSmoke::spotListTabHasTableView() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QTableView*>("spotListTable") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("spotListClearBtn") != nullptr);
    QVERIFY(dlg->findChild<QLabel*>("spotListCountLabel") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::spotListTabHasTwelveBandPills() {
    auto* dlg = makeDialog();
    static const char* bands[] = {
        "160m", "80m", "60m", "40m", "30m", "20m",
        "17m", "15m", "12m", "10m", "6m", "2m"
    };
    for (const char* b : bands) {
        QString name = QString("spotListBandPill_%1").arg(b);
        QVERIFY2(dlg->findChild<QPushButton*>(name) != nullptr,
                 qPrintable(QString("missing band pill: %1").arg(name)));
    }
    delete dlg;
}

void TestSpotHubDialogSmoke::spotListTabHasSevenSourcePills() {
    auto* dlg = makeDialog();
    static const char* sources[] = {
        "DX", "RBN", "JT", "COL", "POT", "FDR", "PSK"
    };
    for (const char* s : sources) {
        QString name = QString("spotListSourcePill_%1").arg(s);
        QVERIFY2(dlg->findChild<QPushButton*>(name) != nullptr,
                 qPrintable(QString("missing source pill: %1").arg(name)));
    }
    delete dlg;
}

void TestSpotHubDialogSmoke::spotListBandPillTogglesProxyFilter() {
    auto* dlg = makeDialog();
    auto* proxy = dlg->findChild<BandFilterProxy*>("spotListProxyModel");
    QVERIFY(proxy != nullptr);
    auto* pill20 = dlg->findChild<QPushButton*>("spotListBandPill_20m");
    QVERIFY(pill20 != nullptr);
    QVERIFY(pill20->isCheckable());
    // Default state - all bands visible
    QVERIFY(proxy->isBandVisible("20m"));
    // Toggle off
    pill20->setChecked(false);
    emit pill20->toggled(false);
    QVERIFY(!proxy->isBandVisible("20m"));
    // Toggle back on
    pill20->setChecked(true);
    emit pill20->toggled(true);
    QVERIFY(proxy->isBandVisible("20m"));
    delete dlg;
}

void TestSpotHubDialogSmoke::spotListSourcePillTogglesProxyFilter() {
    auto* dlg = makeDialog();
    auto* proxy = dlg->findChild<BandFilterProxy*>("spotListProxyModel");
    QVERIFY(proxy != nullptr);
    auto* pillDx = dlg->findChild<QPushButton*>("spotListSourcePill_DX");
    QVERIFY(pillDx != nullptr);
    QVERIFY(pillDx->isCheckable());
    // The DX pill maps to the "Cluster" source string emitted by
    // DxClusterClient. Default state - all sources visible.
    QVERIFY(proxy->isSourceVisible("Cluster"));
    pillDx->setChecked(false);
    emit pillDx->toggled(false);
    QVERIFY(!proxy->isSourceVisible("Cluster"));
    pillDx->setChecked(true);
    emit pillDx->toggled(true);
    QVERIFY(proxy->isSourceVisible("Cluster"));
    delete dlg;
}

void TestSpotHubDialogSmoke::doubleClickOnSpotRowEmitsTuneRequested() {
    auto* dlg = makeDialog();
    auto* proxy = dlg->findChild<BandFilterProxy*>("spotListProxyModel");
    auto* table = dlg->findChild<QTableView*>("spotListTable");
    QVERIFY(proxy != nullptr);
    QVERIFY(table != nullptr);
    // 2026-05-12 (PR #238 follow-up): SpotTableModel was moved out of
    // SpotHubDialog ownership and into RadioModel (so auto-connect
    // spot clients can feed the table before the dialog is opened
    // for the first time).  findChild<> on dlg therefore can't see
    // the model — pull it through the proxy's sourceModel() instead.
    auto* model = qobject_cast<SpotTableModel*>(proxy->sourceModel());
    QVERIFY(model != nullptr);
    // Add a known spot.
    DxSpot s;
    s.dxCall = "K1ABC";
    s.spotterCall = "W2XYZ";
    s.comment = "CW";
    s.freqMhz = 14.025;
    s.utcTime = QTime(12, 34);
    s.source = "Cluster";
    model->addSpot(s);
    QVERIFY(model->rowCount() == 1);
    QVERIFY(proxy->rowCount() == 1);
    // doubleClicked is the upstream trigger. Emit it via the proxy
    // index to exercise the mapToSource + freqAtRow path.
    QSignalSpy spy(dlg, &SpotHubDialog::tuneRequested);
    QModelIndex idx = proxy->index(0, SpotTableModel::ColFreq);
    QVERIFY(idx.isValid());
    emit table->doubleClicked(idx);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toDouble(), 14.025);
    delete dlg;
}

// ── F4: Display tab ────────────────────────────────────────────────

void TestSpotHubDialogSmoke::displayTabHasStatBlocks() {
    auto* dlg = makeDialog();
    // Eight stat blocks per the F4 design (Section 7 of design doc).
    // Each is a QLabel whose objectName is fixed for the test harness.
    static const char* names[] = {
        "displayStatTotalSpots",
        "displayStatUniqueCallsigns",
        "displayStatActiveSources",
        "displayStatCtyDatEntries",
        "displayStatAdifQsos",
        "displayStatDxccEntities",
        "displayStatNewDxcc",
        "displayStatNewBands",
    };
    for (const char* n : names) {
        QVERIFY2(dlg->findChild<QLabel*>(n) != nullptr,
                 qPrintable(QString("missing stat block: %1").arg(n)));
    }
    delete dlg;
}

void TestSpotHubDialogSmoke::displayTabHasLevelsSlider() {
    auto* dlg = makeDialog();
    auto* levels = dlg->findChild<QSlider*>("displayLevelsSlider");
    QVERIFY(levels != nullptr);
    QCOMPARE(levels->minimum(), 1);
    QCOMPARE(levels->maximum(), 10);
    auto* position = dlg->findChild<QSlider*>("displayPositionSlider");
    QVERIFY(position != nullptr);
    QCOMPARE(position->minimum(), 0);
    QCOMPARE(position->maximum(), 100);
    auto* fontSize = dlg->findChild<QSlider*>("displayFontSizeSlider");
    QVERIFY(fontSize != nullptr);
    QCOMPARE(fontSize->minimum(), 8);
    QCOMPARE(fontSize->maximum(), 32);
    auto* bgOpacity = dlg->findChild<QSlider*>("displayBgOpacitySlider");
    QVERIFY(bgOpacity != nullptr);
    QCOMPARE(bgOpacity->minimum(), 0);
    QCOMPARE(bgOpacity->maximum(), 100);
    delete dlg;
}

void TestSpotHubDialogSmoke::displayTabHasLifetimeSlider() {
    auto* dlg = makeDialog();
    auto* life = dlg->findChild<QSlider*>("displayLifetimeSlider");
    QVERIFY(life != nullptr);
    // Non-linear lifetime steps: 10s..55s (5s steps) = 10 entries,
    // 5min..55min (5min steps) = 11 entries, 1hr..24hr (1hr steps)
    // = 24 entries. Total = 45. Range minimum is 0 (index-based).
    QCOMPARE(life->minimum(), 0);
    QCOMPARE(life->maximum(), 10 + 11 + 24 - 1);
    delete dlg;
}

void TestSpotHubDialogSmoke::displayTabHasOverrideColorButton() {
    auto* dlg = makeDialog();
    QVERIFY(dlg->findChild<QPushButton*>("displaySpotsToggle") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("displayMemoriesToggle") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("displayOverrideColorsToggle") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("displayColorSwatch") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("displayOverrideBgToggle") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("displayOverrideBgAutoToggle") != nullptr);
    QVERIFY(dlg->findChild<QPushButton*>("displayBgColorSwatch") != nullptr);
    delete dlg;
}

void TestSpotHubDialogSmoke::displayTabHasClearAllSpotsButton() {
    auto* dlg = makeDialog();
    auto* clearBtn = dlg->findChild<QPushButton*>("displayClearAllSpotsBtn");
    QVERIFY(clearBtn != nullptr);
    QVERIFY(clearBtn->text().contains("Clear", Qt::CaseInsensitive));
    delete dlg;
}

void TestSpotHubDialogSmoke::clearAllButtonEmitsSignal() {
    auto* dlg = makeDialog();
    auto* clearBtn = dlg->findChild<QPushButton*>("displayClearAllSpotsBtn");
    QVERIFY(clearBtn != nullptr);
    QSignalSpy spy(dlg, &SpotHubDialog::spotsClearedAll);
    clearBtn->click();
    QCOMPARE(spy.count(), 1);
    delete dlg;
}

void TestSpotHubDialogSmoke::knobChangeEmitsSettingsChanged() {
    auto* dlg = makeDialog();
    auto* levels = dlg->findChild<QSlider*>("displayLevelsSlider");
    QVERIFY(levels != nullptr);
    QSignalSpy spy(dlg, &SpotHubDialog::settingsChanged);
    // Toggle the levels slider away from its current value to trigger
    // a value change. Choose a known-good value within the 1..10 range.
    int newVal = (levels->value() == 5) ? 4 : 5;
    levels->setValue(newVal);
    QVERIFY(spy.count() >= 1);
    delete dlg;
}

QTEST_MAIN(TestSpotHubDialogSmoke)
#include "tst_spothub_dialog_smoke.moc"
