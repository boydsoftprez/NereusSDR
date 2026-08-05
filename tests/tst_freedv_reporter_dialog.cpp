// SPDX-License-Identifier: GPL-2.0-or-later
//
// no-port-check: Test wires synthetic FreeDVStationModel fixtures into the
// dialog; no callsigns or wire payloads are involved.
//
// NereusSDR - FreeDVReporterDialog G1 shell smoke tests
//
// Phase 3J-2 Task G1. Pins the contract that FreeDVReporterDialog (the
// freedv-gui freedv_reporter.{h,cpp} port) constructs successfully when
// handed a FreeDVStationModel, that its embedded QTableView reports the
// 14-column layout drawn from upstream's `createColumn_` switch (column
// labels, alignment, exact column count), and that the per-row TX-red /
// RX-green highlight tones flip on / off in response to
// FreeDVStationModel::stationUpdated. The 6-second clear interval is
// drained via a test seam (`setHighlightClearMsForTest`) so the suite
// does not block on real wall-clock time. G2 (filter / QSY / message
// bar) and G3 (menu bar with Show / Filter / Idle menus) are out of
// scope; G1 only leaves the placeholder hooks where those wire in.
//
// Source ports cited inline next to each port.
//   freedv-gui src/gui/dialogs/freedv_reporter.cpp:47-65 (column index
//     macros + UNKNOWN_SNR_VAL) [@77e793a]
//   freedv-gui src/gui/dialogs/freedv_reporter.cpp:75-182 (createColumn_
//     column-build table) [@77e793a]
//   freedv-gui src/gui/dialogs/freedv_reporter.cpp:3580-3737
//     (onTransmitUpdateFn_ / onReceiveUpdateFn_ TX-RX state hooks) [@77e793a]
//   freedv-gui src/gui/dialogs/freedv_reporter.cpp:1289-1322
//     (TX-red / RX-green / msg-color row-tint priority chain) [@77e793a]

#include <QtTest>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QColor>
#include <QHeaderView>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTableView>
#include <QUrl>

#include "core/AppSettings.h"
#include "gui/FreeDVReporterDialog.h"
#include "models/FreeDVStationModel.h"
#include "core/FreeDVStation.h"

using namespace NereusSDR;

class TestFreeDVReporterDialog : public QObject {
    Q_OBJECT
private slots:
    void dialogConstructs();
    void tableHasFourteenColumns();
    void tableColumnHeadersMatchUpstream();
    void tableColumnAlignmentsMatchUpstream();
    void populatesFiveStationsToFiveRows();
    void snrColumnFormatsMinusNinetyNineAsDash();
    void txReportTurnsRowRed();
    void rxReportTurnsRowGreen();
    void highlightClearsAfterSixSeconds();
    void stationRemovedDeletesRow();

    // G2 - filter / QSY / message bar
    void bandFilterComboHasThirteenEntries();
    void bandFilterAllShowsAllRows();
    void bandFilterTwentyMetersHidesOtherBandRows();
    void qsyButtonDisabledWhenNoSelection();
    void qsyButtonEnabledWhenRowSelected();
    void qsyButtonEmitsRequestWithParsedFreq();
    void messageSendEmitsCurrentText();
    void messageSaveAppendsToMru();
    void messageClearEmitsEmptyAndClearsEdit();
    void openWebsiteOpensQsoFreedvOrgUrl();

    // G3 - Show / Filter / Idle longer than menus + filters + idle sweep
    void menuBarHasThreeMenus();
    void showMenuHasFourteenCheckableActions();
    void showMenuChecksMatchUpstreamDefaults();
    void togglingShowMenuActionHidesColumn();
    void visibleColumnsPersistToAppSettings();
    void filterMenuHasFourteenSubmenus();
    void filterMenuColumnSubmenuHasSixOperatorsAndClear();
    void applyingFilterReducesVisibleRows();
    void clearingFilterRestoresRows();
    void bandFilterAndColumnFilterCompose();
    void idleLongerThanMenuHasFourExclusiveActions();
    void idleSweepRemovesStationsOlderThanThreshold();
    void idleNeverDoesNotRemove();
    void rightClickOnRowOpensContextMenu();
    void qrzUrlAccessorReturnsExpectedFormat();
    void doubleClickOnRowEmitsTuneRequested();
};

// Build a FreeDVStation with sensible defaults and let callers override.
static FreeDVStation makeStation(const QString& sid, const QString& callsign) {
    FreeDVStation s;
    s.sid = sid;
    s.callsign = callsign;
    s.gridSquare = "EM00";
    s.version = "1.9.0";
    s.frequencyHz = 14236000;
    s.txMode = "700D";
    s.status = "Active";
    s.lastUpdate = QDateTime::currentDateTime();
    return s;
}

void TestFreeDVReporterDialog::dialogConstructs() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    QVERIFY(dlg != nullptr);
    QVERIFY(dlg->findChild<QTableView*>() != nullptr);
    QVERIFY(dlg->findChild<QMenuBar*>() != nullptr);
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::tableHasFourteenColumns() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->model()->columnCount(), 14);
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::tableColumnHeadersMatchUpstream() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);
    auto* tm = table->model();

    // Headers per freedv-gui createColumn_ switch, column indices 0..13
    // [freedv_reporter.cpp:75-153 @77e793a]. G1 hard-codes MHz for col 5;
    // the unit toggle (kHz) lands in G3.
    QCOMPARE(tm->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Callsign"));
    QCOMPARE(tm->headerData(1, Qt::Horizontal).toString(), QStringLiteral("Locator"));
    QCOMPARE(tm->headerData(2, Qt::Horizontal).toString(), QStringLiteral("km"));
    QCOMPARE(tm->headerData(3, Qt::Horizontal).toString(), QStringLiteral("Hdg"));
    QCOMPARE(tm->headerData(4, Qt::Horizontal).toString(), QStringLiteral("Version"));
    QCOMPARE(tm->headerData(5, Qt::Horizontal).toString(), QStringLiteral("MHz"));
    QCOMPARE(tm->headerData(6, Qt::Horizontal).toString(), QStringLiteral("Mode"));
    QCOMPARE(tm->headerData(7, Qt::Horizontal).toString(), QStringLiteral("Status"));
    QCOMPARE(tm->headerData(8, Qt::Horizontal).toString(), QStringLiteral("Msg"));
    QCOMPARE(tm->headerData(9, Qt::Horizontal).toString(), QStringLiteral("Last TX"));
    QCOMPARE(tm->headerData(10, Qt::Horizontal).toString(), QStringLiteral("RX Call"));
    QCOMPARE(tm->headerData(11, Qt::Horizontal).toString(), QStringLiteral("Mode (RX)"));
    QCOMPARE(tm->headerData(12, Qt::Horizontal).toString(), QStringLiteral("SNR"));
    QCOMPARE(tm->headerData(13, Qt::Horizontal).toString(), QStringLiteral("Last Update"));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::tableColumnAlignmentsMatchUpstream() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);
    auto* tm = table->model();

    // CALLSIGN_COL renderer SetAlignment(wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL)
    //   [freedv-gui freedv_reporter.cpp:79+86-88 @77e793a]
    auto callsignAlign = tm->headerData(0, Qt::Horizontal, Qt::TextAlignmentRole)
                            .toInt() & Qt::AlignHorizontal_Mask;
    QCOMPARE(callsignAlign, int(Qt::AlignLeft));

    // DISTANCE_COL renderer SetAlignment(wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL)
    //   [freedv-gui freedv_reporter.cpp:93-97 @77e793a]
    auto kmAlign = tm->headerData(2, Qt::Horizontal, Qt::TextAlignmentRole)
                      .toInt() & Qt::AlignHorizontal_Mask;
    QCOMPARE(kmAlign, int(Qt::AlignRight));

    // USER_MESSAGE_COL builds the column with wxALIGN_CENTER
    //   [freedv-gui freedv_reporter.cpp:120-133 @77e793a]
    auto msgAlign = tm->headerData(8, Qt::Horizontal, Qt::TextAlignmentRole)
                       .toInt() & Qt::AlignHorizontal_Mask;
    QCOMPARE(msgAlign, int(Qt::AlignHCenter));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::populatesFiveStationsToFiveRows() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    for (int i = 0; i < 5; ++i) {
        QString sid = QStringLiteral("sid-%1").arg(i);
        QString call = QStringLiteral("CALL%1").arg(i);
        model->onStationAdded(sid, makeStation(sid, call));
    }
    QCOMPARE(table->model()->rowCount(), 5);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::snrColumnFormatsMinusNinetyNineAsDash() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    auto sUnknown = makeStation("sid-unknown", "AA0AA");
    sUnknown.snrVal = -99;
    model->onStationAdded("sid-unknown", sUnknown);

    auto sPos = makeStation("sid-pos", "BB0BB");
    sPos.snrVal = 12;
    model->onStationAdded("sid-pos", sPos);

    // SNR is column 12 per freedv_reporter.cpp:59 @77e793a
    auto tm = table->model();
    QString unknownText, posText;
    for (int r = 0; r < tm->rowCount(); ++r) {
        QString call = tm->index(r, 0).data(Qt::DisplayRole).toString();
        QString snr = tm->index(r, 12).data(Qt::DisplayRole).toString();
        if (call == "AA0AA") unknownText = snr;
        if (call == "BB0BB") posText = snr;
    }
    QCOMPARE(unknownText, QStringLiteral(" - "));
    QCOMPARE(posText, QStringLiteral("+12"));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::txReportTurnsRowRed() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    auto s = makeStation("sid-tx", "K1TX");
    model->onStationAdded("sid-tx", s);
    s.transmitting = true;
    model->onStationUpdated("sid-tx", s);

    QColor bg = dlg->rowHighlightColorForTest("sid-tx");
    // Red-dominant tone per freedv_reporter.cpp:1313-1317 @77e793a
    // (`backgroundColor = parent_->txRowBackgroundColor`).
    QVERIFY(bg.isValid());
    QVERIFY(bg.red() > bg.green());
    QVERIFY(bg.red() > bg.blue());

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::rxReportTurnsRowGreen() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    auto s = makeStation("sid-rx", "K1RX");
    model->onStationAdded("sid-rx", s);
    s.lastRxCallsign = "K1NEAR";
    s.lastRxMode = "700D";
    s.snrVal = 5;
    s.lastRxDate = QDateTime::currentDateTime();
    model->onStationUpdated("sid-rx", s);

    QColor bg = dlg->rowHighlightColorForTest("sid-rx");
    // RX tint is the slate-teal "dim cool" tone (#2e4750 = 46/71/80
    // RGB).  Updated 2026-05-13 per PR #238 bench feedback to be a
    // muted blue-dominant tint instead of the original neon green
    // (#3faf55) — upstream uses cyan-leaning #379baf (also blue
    // dominant) at freedv_reporter.cpp:91-95 [@77e793a].  Assertion
    // pins "this is a cool / blue-dominant cell, not warm or
    // green-dominant" so any future palette tweak that swings the
    // hue back to red/green will trip this regression.
    QVERIFY(bg.isValid());
    QVERIFY(bg.blue() > bg.red());
    QVERIFY(bg.blue() >= bg.green());

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::highlightClearsAfterSixSeconds() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    // Drop the clear interval to keep the test fast. Production wires
    // 6000 ms per task spec; G3 will surface this as a per-radio setting
    // matching upstream's freedv_reporter_tx_rx_highlight_time.
    dlg->setHighlightClearMsForTest(50);

    auto s = makeStation("sid-fade", "K1FADE");
    model->onStationAdded("sid-fade", s);
    s.transmitting = true;
    model->onStationUpdated("sid-fade", s);
    QVERIFY(dlg->rowHighlightColorForTest("sid-fade").isValid());

    QTest::qWait(120);
    QColor bg = dlg->rowHighlightColorForTest("sid-fade");
    QVERIFY(!bg.isValid());

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::stationRemovedDeletesRow() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    model->onStationAdded("sid-go", makeStation("sid-go", "K1GO"));
    QCOMPARE(table->model()->rowCount(), 1);
    model->onStationRemoved("sid-go");
    QCOMPARE(table->model()->rowCount(), 0);

    delete dlg;
    delete model;
}

// =====================================================================
// G2: filter / QSY / message bar
// =====================================================================

// Build a station at a specific frequency for band filter testing.
static FreeDVStation makeStationOnFreq(const QString& sid, const QString& callsign,
                                       quint64 freqHz) {
    FreeDVStation s = makeStation(sid, callsign);
    s.frequencyHz = freqHz;
    return s;
}

void TestFreeDVReporterDialog::bandFilterComboHasThirteenEntries() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* combo = dlg->findChild<QComboBox*>(QStringLiteral("bandFilterCombo"));
    QVERIFY(combo != nullptr);
    // Task spec: All + 12 bands (160 / 80 / 60 / 40 / 30 / 20 / 17 / 15 /
    //   12 / 10 / 6 / 2) = 13 entries. Cite at construction site.
    QCOMPARE(combo->count(), 13);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::bandFilterAllShowsAllRows() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* combo = dlg->findChild<QComboBox*>(QStringLiteral("bandFilterCombo"));
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(combo != nullptr);
    QVERIFY(table != nullptr);

    // 3 stations across 3 different bands: 80m / 20m / 6m.
    model->onStationAdded("sid-80",
        makeStationOnFreq("sid-80", "K1A", 3573000));    // 80m FT8 freq
    model->onStationAdded("sid-20",
        makeStationOnFreq("sid-20", "K1B", 14236000));   // 20m FreeDV freq
    model->onStationAdded("sid-6",
        makeStationOnFreq("sid-6", "K1C", 50313000));    // 6m FT8 freq

    // Combo is on "All" by default.
    QCOMPARE(combo->currentIndex(), 0);
    QCOMPARE(table->model()->rowCount(), 3);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::bandFilterTwentyMetersHidesOtherBandRows() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* combo = dlg->findChild<QComboBox*>(QStringLiteral("bandFilterCombo"));
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(combo != nullptr);
    QVERIFY(table != nullptr);

    model->onStationAdded("sid-80",
        makeStationOnFreq("sid-80", "K1A", 3573000));
    model->onStationAdded("sid-20",
        makeStationOnFreq("sid-20", "K1B", 14236000));
    model->onStationAdded("sid-6",
        makeStationOnFreq("sid-6", "K1C", 50313000));

    // Find the "20m" item and select it.
    const int idx = combo->findText(QStringLiteral("20m"));
    QVERIFY(idx > 0);
    combo->setCurrentIndex(idx);

    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(table->model()->index(0, 0).data().toString(), QStringLiteral("K1B"));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::qsyButtonDisabledWhenNoSelection() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* qsyBtn = dlg->findChild<QPushButton*>(QStringLiteral("qsySendButton"));
    QVERIFY(qsyBtn != nullptr);
    // From freedv-gui freedv_reporter.cpp:312 [@77e793a] -
    //   m_buttonSendQSY->Enable(false) is the constructor default.
    QVERIFY(!qsyBtn->isEnabled());

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::qsyButtonEnabledWhenRowSelected() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* qsyBtn = dlg->findChild<QPushButton*>(QStringLiteral("qsySendButton"));
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(qsyBtn != nullptr);
    QVERIFY(table != nullptr);

    model->onStationAdded("sid-go",
        makeStationOnFreq("sid-go", "K1XX", 14236000));

    // Programmatically select row 0.
    table->selectRow(0);

    QVERIFY(qsyBtn->isEnabled());

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::qsyButtonEmitsRequestWithParsedFreq() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* qsyBtn = dlg->findChild<QPushButton*>(QStringLiteral("qsySendButton"));
    auto* qsyFreq = dlg->findChild<QLineEdit*>(QStringLiteral("qsyFreqEdit"));
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(qsyBtn != nullptr);
    QVERIFY(qsyFreq != nullptr);
    QVERIFY(table != nullptr);

    model->onStationAdded("sid-target",
        makeStationOnFreq("sid-target", "K1TGT", 14236000));
    table->selectRow(0);

    qsyFreq->setText(QStringLiteral("14.230"));

    QSignalSpy spy(dlg, &FreeDVReporterDialog::qsyRequested);
    qsyBtn->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("sid-target"));
    QCOMPARE(spy.first().at(1).toULongLong(), quint64(14230000));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::messageSendEmitsCurrentText() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* msgEdit = dlg->findChild<QLineEdit*>(QStringLiteral("msgEdit"));
    auto* sendBtn = dlg->findChild<QPushButton*>(QStringLiteral("msgSendButton"));
    QVERIFY(msgEdit != nullptr);
    QVERIFY(sendBtn != nullptr);

    msgEdit->setText(QStringLiteral("CQ TEST"));

    QSignalSpy spy(dlg, &FreeDVReporterDialog::messageSendRequested);
    sendBtn->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("CQ TEST"));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::messageSaveAppendsToMru() {
    // Clear AppSettings entry first so this test starts clean.
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/SavedMessages"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* msgEdit = dlg->findChild<QLineEdit*>(QStringLiteral("msgEdit"));
    auto* saveBtn = dlg->findChild<QPushButton*>(QStringLiteral("msgSaveButton"));
    auto* msgDropdown = dlg->findChild<QComboBox*>(QStringLiteral("msgDropdown"));
    QVERIFY(msgEdit != nullptr);
    QVERIFY(saveBtn != nullptr);
    QVERIFY(msgDropdown != nullptr);

    msgEdit->setText(QStringLiteral("CQ DX"));
    saveBtn->click();

    bool found = false;
    for (int i = 0; i < msgDropdown->count(); ++i) {
        if (msgDropdown->itemText(i) == QStringLiteral("CQ DX")) {
            found = true;
            break;
        }
    }
    QVERIFY(found);

    // Round-trips through AppSettings.
    const QString stored = AppSettings::instance()
        .value(QStringLiteral("FreeDvReporter/SavedMessages")).toString();
    QVERIFY(stored.contains(QStringLiteral("CQ DX")));

    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/SavedMessages"));
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::messageClearEmitsEmptyAndClearsEdit() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* msgEdit = dlg->findChild<QLineEdit*>(QStringLiteral("msgEdit"));
    auto* clearBtn = dlg->findChild<QPushButton*>(QStringLiteral("msgClearButton"));
    QVERIFY(msgEdit != nullptr);
    QVERIFY(clearBtn != nullptr);

    msgEdit->setText(QStringLiteral("some old message"));

    QSignalSpy spy(dlg, &FreeDVReporterDialog::messageSendRequested);
    clearBtn->click();

    QVERIFY(msgEdit->text().isEmpty());
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().at(0).toString().isEmpty());

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::openWebsiteOpensQsoFreedvOrgUrl() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);

    // Test seam: read the URL the Open Website button targets without
    // actually invoking QDesktopServices::openUrl in the unit test.
    const QUrl url = dlg->websiteUrl();
    QCOMPARE(url, QUrl(QStringLiteral("https://qso.freedv.org")));

    delete dlg;
    delete model;
}

// =====================================================================
// G3: Show / Filter / Idle longer than menus, per-column filters,
//     idle auto-delete, row context menu, double-click QSY
// =====================================================================

void TestFreeDVReporterDialog::menuBarHasThreeMenus() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* mb = dlg->findChild<QMenuBar*>();
    QVERIFY(mb != nullptr);

    // Three top-level menus: Show / Filter / Idle longer than. Mirrors
    // freedv-gui freedv_reporter.cpp:395-431 [@77e793a] where the same
    // three logical groupings are appended (the upstream wxMenuBar puts
    // Show + Filter at the top, with "Idle more than" as a submenu of
    // Filter; the task spec promotes Idle to a top-level menu).
    QList<QMenu*> topLevel;
    for (QAction* a : mb->actions()) {
        if (a->menu() != nullptr) {
            topLevel.append(a->menu());
        }
    }
    QCOMPARE(topLevel.size(), 3);
    QStringList titles;
    for (QMenu* m : topLevel) {
        titles.append(m->title());
    }
    QVERIFY(titles.contains(QStringLiteral("Show")));
    QVERIFY(titles.contains(QStringLiteral("Filter")));
    QVERIFY(titles.contains(QStringLiteral("Idle longer than")));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::showMenuHasFourteenCheckableActions() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* mb = dlg->findChild<QMenuBar*>();
    QVERIFY(mb != nullptr);

    QMenu* showMenu = nullptr;
    for (QAction* a : mb->actions()) {
        if (a->menu() != nullptr && a->menu()->title() == QStringLiteral("Show")) {
            showMenu = a->menu();
            break;
        }
    }
    QVERIFY(showMenu != nullptr);

    // 14 actions, one per column, each checkable. Mirrors
    // freedv-gui freedv_reporter.cpp:398-413 [@77e793a] (vector of 14
    // {col_id, label} pairs, each Append'd with wxITEM_CHECK).
    QList<QAction*> actions = showMenu->actions();
    int checkableCount = 0;
    for (QAction* a : actions) {
        if (a->isCheckable()) {
            checkableCount++;
        }
    }
    QCOMPARE(checkableCount, 14);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::showMenuChecksMatchUpstreamDefaults() {
    // Clear any persisted state so this test starts from upstream defaults.
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/VisibleColumns"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* mb = dlg->findChild<QMenuBar*>();
    QVERIFY(mb != nullptr);

    QMenu* showMenu = nullptr;
    for (QAction* a : mb->actions()) {
        if (a->menu() != nullptr && a->menu()->title() == QStringLiteral("Show")) {
            showMenu = a->menu();
            break;
        }
    }
    QVERIFY(showMenu != nullptr);

    // Upstream default has all 14 columns visible (the column-visibility
    // vector is filled with `true` for every missing slot, see
    // freedv_reporter.cpp:251-255 [@77e793a]). NereusSDR mirrors that.
    int checkedCount = 0;
    for (QAction* a : showMenu->actions()) {
        if (a->isCheckable() && a->isChecked()) {
            checkedCount++;
        }
    }
    QCOMPARE(checkedCount, 14);

    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/VisibleColumns"));
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::togglingShowMenuActionHidesColumn() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/VisibleColumns"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* mb = dlg->findChild<QMenuBar*>();
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(mb != nullptr);
    QVERIFY(table != nullptr);

    QMenu* showMenu = nullptr;
    for (QAction* a : mb->actions()) {
        if (a->menu() != nullptr && a->menu()->title() == QStringLiteral("Show")) {
            showMenu = a->menu();
            break;
        }
    }
    QVERIFY(showMenu != nullptr);

    // Uncheck SNR (column index 12). Expect setColumnHidden(12, true).
    // From freedv-gui freedv_reporter.cpp:843-859 [@77e793a]
    //   (OnShowColumn flips visibility and calls col->SetHidden).
    QList<QAction*> actions = showMenu->actions();
    QAction* snrAction = nullptr;
    for (QAction* a : actions) {
        if (a->data().toInt() == 12) {
            snrAction = a;
            break;
        }
    }
    QVERIFY(snrAction != nullptr);
    QVERIFY(snrAction->isChecked());
    // trigger() on a checkable action toggles isChecked() and emits
    // triggered() with the new state. Default = true, so trigger()
    // takes it to false and the QActionGroup::triggered slot fires
    // onShowColumnToggled with the action now at isChecked() == false.
    snrAction->trigger();

    QVERIFY(!snrAction->isChecked());
    QVERIFY(table->isColumnHidden(12));

    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/VisibleColumns"));
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::visibleColumnsPersistToAppSettings() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/VisibleColumns"));

    // First dialog: hide SNR column (index 12).
    {
        auto* model = new FreeDVStationModel;
        auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
        auto* mb = dlg->findChild<QMenuBar*>();
        QMenu* showMenu = nullptr;
        for (QAction* a : mb->actions()) {
            if (a->menu() && a->menu()->title() == QStringLiteral("Show")) {
                showMenu = a->menu();
                break;
            }
        }
        QVERIFY(showMenu != nullptr);
        for (QAction* a : showMenu->actions()) {
            if (a->data().toInt() == 12) {
                // trigger() flips isChecked() from default true to false.
                a->trigger();
                break;
            }
        }
        delete dlg;
        delete model;
    }

    // Second dialog: expect SNR column to come up hidden.
    {
        auto* model = new FreeDVStationModel;
        auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
        auto* table = dlg->findChild<QTableView*>();
        QVERIFY(table != nullptr);
        QVERIFY(table->isColumnHidden(12));

        auto* mb = dlg->findChild<QMenuBar*>();
        QMenu* showMenu = nullptr;
        for (QAction* a : mb->actions()) {
            if (a->menu() && a->menu()->title() == QStringLiteral("Show")) {
                showMenu = a->menu();
                break;
            }
        }
        QAction* snrAction = nullptr;
        for (QAction* a : showMenu->actions()) {
            if (a->data().toInt() == 12) {
                snrAction = a;
                break;
            }
        }
        QVERIFY(snrAction != nullptr);
        QVERIFY(!snrAction->isChecked());

        delete dlg;
        delete model;
    }

    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/VisibleColumns"));
}

void TestFreeDVReporterDialog::filterMenuHasFourteenSubmenus() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* mb = dlg->findChild<QMenuBar*>();
    QVERIFY(mb != nullptr);

    QMenu* filterMenu = nullptr;
    for (QAction* a : mb->actions()) {
        if (a->menu() && a->menu()->title() == QStringLiteral("Filter")) {
            filterMenu = a->menu();
            break;
        }
    }
    QVERIFY(filterMenu != nullptr);

    // One submenu per column = 14.
    int submenuCount = 0;
    for (QAction* a : filterMenu->actions()) {
        if (a->menu() != nullptr) {
            submenuCount++;
        }
    }
    QCOMPARE(submenuCount, 14);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::filterMenuColumnSubmenuHasSixOperatorsAndClear() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* mb = dlg->findChild<QMenuBar*>();
    QVERIFY(mb != nullptr);

    QMenu* filterMenu = nullptr;
    for (QAction* a : mb->actions()) {
        if (a->menu() && a->menu()->title() == QStringLiteral("Filter")) {
            filterMenu = a->menu();
            break;
        }
    }
    QVERIFY(filterMenu != nullptr);

    // Pick the SNR submenu (col 12); same shape applies to every column.
    QMenu* snrSubmenu = nullptr;
    for (QAction* a : filterMenu->actions()) {
        if (a->menu() && a->data().toInt() == 12) {
            snrSubmenu = a->menu();
            break;
        }
    }
    QVERIFY(snrSubmenu != nullptr);

    // 6 operators + Clear filter = 7 actions. From
    // freedv-gui freedv_reporter.cpp:1726-1771 [@77e793a]
    //   (FILTER_GTE / FILTER_GT / FILTER_EQ / FILTER_NEQ / FILTER_LT /
    //   FILTER_LTE + Clear Column Filter).
    int actionCount = 0;
    for (QAction* a : snrSubmenu->actions()) {
        if (!a->isSeparator()) {
            actionCount++;
        }
    }
    QCOMPARE(actionCount, 7);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::applyingFilterReducesVisibleRows() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/VisibleColumns"));
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    // 5 stations with different SNR values: -10, -3, 0, 5, 12.
    int snrs[] = {-10, -3, 0, 5, 12};
    for (int i = 0; i < 5; ++i) {
        QString sid = QStringLiteral("sid-%1").arg(i);
        FreeDVStation s = makeStation(sid, QStringLiteral("K%1").arg(i));
        s.snrVal = snrs[i];
        model->onStationAdded(sid, s);
    }
    QCOMPARE(table->model()->rowCount(), 5);

    // Apply SNR >= 5. Should leave 2 rows (snr 5 and 12).
    dlg->setColumnFilterForTest(12, 0 /* FILTER_GTE */, QStringLiteral("5"));
    QCOMPARE(table->model()->rowCount(), 2);

    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::clearingFilterRestoresRows() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    int snrs[] = {-10, -3, 0, 5, 12};
    for (int i = 0; i < 5; ++i) {
        QString sid = QStringLiteral("sid-%1").arg(i);
        FreeDVStation s = makeStation(sid, QStringLiteral("K%1").arg(i));
        s.snrVal = snrs[i];
        model->onStationAdded(sid, s);
    }
    dlg->setColumnFilterForTest(12, 0, QStringLiteral("5"));
    QCOMPARE(table->model()->rowCount(), 2);

    // Clear filter. Should restore all 5 rows.
    dlg->clearColumnFilterForTest(12);
    QCOMPARE(table->model()->rowCount(), 5);

    // Clean up persisted filter so it does not leak into later tests.
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::bandFilterAndColumnFilterCompose() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* combo = dlg->findChild<QComboBox*>(QStringLiteral("bandFilterCombo"));
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(combo != nullptr);
    QVERIFY(table != nullptr);

    // 4 stations:
    //   sid-a 20m SNR 8     (passes both filters)
    //   sid-b 20m SNR -5    (band ok, SNR filter rejects)
    //   sid-c 40m SNR 8     (SNR ok, band filter rejects)
    //   sid-d 40m SNR -5    (rejected by both)
    FreeDVStation a = makeStationOnFreq("sid-a", "K1A", 14236000);
    a.snrVal = 8;
    model->onStationAdded("sid-a", a);

    FreeDVStation b = makeStationOnFreq("sid-b", "K1B", 14236000);
    b.snrVal = -5;
    model->onStationAdded("sid-b", b);

    FreeDVStation c = makeStationOnFreq("sid-c", "K1C", 7074000);
    c.snrVal = 8;
    model->onStationAdded("sid-c", c);

    FreeDVStation d = makeStationOnFreq("sid-d", "K1D", 7074000);
    d.snrVal = -5;
    model->onStationAdded("sid-d", d);

    // Apply 20m band + SNR >= 5. Only sid-a should remain.
    const int idx = combo->findText(QStringLiteral("20m"));
    QVERIFY(idx > 0);
    combo->setCurrentIndex(idx);
    dlg->setColumnFilterForTest(12, 0, QStringLiteral("5"));

    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(table->model()->index(0, 0).data().toString(), QStringLiteral("K1A"));

    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::idleLongerThanMenuHasFourExclusiveActions() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* mb = dlg->findChild<QMenuBar*>();
    QVERIFY(mb != nullptr);

    QMenu* idleMenu = nullptr;
    for (QAction* a : mb->actions()) {
        if (a->menu() && a->menu()->title() == QStringLiteral("Idle longer than")) {
            idleMenu = a->menu();
            break;
        }
    }
    QVERIFY(idleMenu != nullptr);

    // 4 actions: 30 minutes / 1 hour / 2 hours / Never. From task spec
    // (upstream offers Disabled / 30 / 60 / 90 / 120 / Custom...; the
    // NereusSDR menu is the trimmed 4-item set with "Never" as the
    // explicit disabled option).
    QList<QAction*> actions;
    for (QAction* a : idleMenu->actions()) {
        if (!a->isSeparator()) {
            actions.append(a);
        }
    }
    QCOMPARE(actions.size(), 4);

    // All four checkable; exactly one checked at a time (QActionGroup
    // setExclusive(true)).
    int checkableCount = 0;
    int checkedCount = 0;
    for (QAction* a : actions) {
        if (a->isCheckable()) {
            checkableCount++;
        }
        if (a->isChecked()) {
            checkedCount++;
        }
    }
    QCOMPARE(checkableCount, 4);
    QCOMPARE(checkedCount, 1);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::idleSweepRemovesStationsOlderThanThreshold() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    // Set the idle threshold to 30 minutes via the test seam.
    dlg->setIdleTimeoutMinutesForTest(30);
    // Crank the sweep interval down to 10 ms.
    dlg->setIdleSweepIntervalMsForTest(10);

    // 2 stations:
    //   sid-fresh - lastUpdate = now. Should NOT be removed.
    //   sid-stale - lastUpdate = 35 minutes ago. Should be removed.
    FreeDVStation fresh = makeStation("sid-fresh", "K1FRESH");
    fresh.lastUpdate = QDateTime::currentDateTime();
    model->onStationAdded("sid-fresh", fresh);

    FreeDVStation stale = makeStation("sid-stale", "K1STALE");
    stale.lastUpdate = QDateTime::currentDateTime().addSecs(-35 * 60);
    model->onStationAdded("sid-stale", stale);

    QCOMPARE(table->model()->rowCount(), 2);

    // Let the sweep timer fire at least once.
    QTest::qWait(50);

    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(table->model()->index(0, 0).data().toString(), QStringLiteral("K1FRESH"));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::idleNeverDoesNotRemove() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    // "Never" = 0 minutes via the test seam.
    dlg->setIdleTimeoutMinutesForTest(0);
    dlg->setIdleSweepIntervalMsForTest(10);

    FreeDVStation stale = makeStation("sid-stale", "K1STALE");
    stale.lastUpdate = QDateTime::currentDateTime().addSecs(-24 * 60 * 60);  // 24h ago
    model->onStationAdded("sid-stale", stale);

    QCOMPARE(table->model()->rowCount(), 1);

    QTest::qWait(50);

    // Still 1 row - "Never" disables the sweep regardless of station age.
    QCOMPARE(table->model()->rowCount(), 1);

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::rightClickOnRowOpensContextMenu() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);

    model->onStationAdded("sid-x",
        makeStationOnFreq("sid-x", "K1XX", 14236000));

    // Build the context menu via the test seam without actually showing
    // it (popup needs a screen pointer + main event loop, which is more
    // hassle than the menu's structural surface).
    QMenu* menu = dlg->buildRowContextMenuForTest(0);
    QVERIFY(menu != nullptr);

    // 5 visible actions + 2 separators expected: QRZ / HamQTH / HamCall /
    // Copy callsign / Copy locator / Send QSY to this station. From task
    // spec.
    QStringList labels;
    for (QAction* a : menu->actions()) {
        if (!a->isSeparator()) {
            labels.append(a->text());
        }
    }
    QVERIFY(labels.contains(QStringLiteral("Look up on QRZ.com")));
    QVERIFY(labels.contains(QStringLiteral("Look up on HamQTH")));
    QVERIFY(labels.contains(QStringLiteral("Look up on HamCall")));
    QVERIFY(labels.contains(QStringLiteral("Copy callsign")));
    QVERIFY(labels.contains(QStringLiteral("Copy locator")));
    QVERIFY(labels.contains(QStringLiteral("Send QSY to this station")));

    delete menu;
    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::qrzUrlAccessorReturnsExpectedFormat() {
    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);

    // From freedv-gui freedv_reporter.cpp:1846-1851 [@77e793a]
    //   (`https://www.qrz.com/db/%s` template).
    QCOMPARE(dlg->qrzUrlForCallsignForTest("KG4VCF"),
             QStringLiteral("https://www.qrz.com/db/KG4VCF"));

    delete dlg;
    delete model;
}

void TestFreeDVReporterDialog::doubleClickOnRowEmitsTuneRequested() {
    AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));

    auto* model = new FreeDVStationModel;
    auto* dlg = new FreeDVReporterDialog(model, nullptr, nullptr);
    auto* table = dlg->findChild<QTableView*>();
    QVERIFY(table != nullptr);

    model->onStationAdded("sid-d",
        makeStationOnFreq("sid-d", "K1DBL", 14236000));

    QSignalSpy spy(dlg, &FreeDVReporterDialog::tuneRequested);
    // Programmatically emit the doubleClicked signal on row 0.
    const QModelIndex idx = table->model()->index(0, 0);
    emit table->doubleClicked(idx);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toULongLong(), quint64(14236000));

    delete dlg;
    delete model;
}

QTEST_MAIN(TestFreeDVReporterDialog)
#include "tst_freedv_reporter_dialog.moc"
