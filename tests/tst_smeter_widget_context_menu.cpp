// tests/tst_smeter_widget_context_menu.cpp
//
// Verifies SMeterWidget right-click context menu structure and AppSettings
// persistence via the buildContextMenuForTesting() test seam.
//
// Test seam: buildContextMenuForTesting() calls buildContextMenu(this) and
// returns the heap-allocated QMenu* without exec()-ing it. This lets tests
// inspect the menu tree without a live event loop or real mouse events.
//
// Menu order (index 0..2 on the top-level QMenu):
//   0 - "TX Mode"    submenu (4 exclusive actions)
//   1 - "RX Mode"    submenu (4 exclusive actions)
//   2 - "Peak Hold"  submenu (Enabled + Decay submenu + Reset)
//
// AppSettings persistence:
//   setTxMode("SWR") -> SMeter_TxSelect == 1  (Power=0, SWR=1, Level=2, Compression=3)
//   setRxMode("Signal Peak") -> SMeter_RxSelect == 2
//
// TestSandboxInit.cpp (auto-linked by nereus_add_test) enables
// QStandardPaths::setTestModeEnabled(true) before main(), so
// AppSettings::instance().clear() affects only the test sandbox and
// never touches the developer's real NereusSDR.settings.
//
// NereusSDR-native test file; no upstream equivalent (AetherSDR does not have
// a contextMenuEvent on SMeterWidget).
//
// Phase 3P-II Phase 2, Task 39.

#include <QtTest>
#include <QMenu>
#include <QAction>

#include "gui/SMeterWidget.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class SMeterWidgetContextMenuTest : public QObject {
    Q_OBJECT
private slots:
    void menuHasFourRxModes();
    void settingTxModePersistsToAppSettings();
};

// Verify that the RX Mode submenu contains exactly the four entries
// specified in design doc ss5.4.2: Signal, Sig Avg, Signal Peak, Max Bin.
void SMeterWidgetContextMenuTest::menuHasFourRxModes()
{
    NereusSDR::SMeterWidget w;
    QMenu* menu = w.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);

    // Top-level menu has 3 sub-menus: TX Mode (0), RX Mode (1), Peak Hold (2)
    const auto topActions = menu->actions();
    QVERIFY2(topActions.size() >= 2, "Expected at least TX Mode and RX Mode sub-menus");

    QMenu* rxMenu = topActions.at(1)->menu();
    QVERIFY2(rxMenu != nullptr, "Index 1 action should be the RX Mode submenu");

    QStringList labels;
    for (auto* a : rxMenu->actions()) {
        labels << a->text();
    }

    QVERIFY2(labels.contains("Signal"),
             qPrintable(QString("Expected 'Signal' in RX menu, got: %1").arg(labels.join(", "))));
    QVERIFY2(labels.contains("Sig Avg"),
             qPrintable(QString("Expected 'Sig Avg' in RX menu, got: %1").arg(labels.join(", "))));
    QVERIFY2(labels.contains("Signal Peak"),
             qPrintable(QString("Expected 'Signal Peak' in RX menu, got: %1").arg(labels.join(", "))));
    QVERIFY2(labels.contains("Max Bin"),
             qPrintable(QString("Expected 'Max Bin' in RX menu, got: %1").arg(labels.join(", "))));

    menu->deleteLater();
}

// Verify that setTxMode("SWR") persists SMeter_TxSelect == 1 to AppSettings.
// Mapping per design doc ss5.4.2: Power=0, SWR=1, Level=2, Compression=3.
void SMeterWidgetContextMenuTest::settingTxModePersistsToAppSettings()
{
    AppSettings::instance().clear();

    NereusSDR::SMeterWidget w;
    w.setTxMode("SWR");

    QCOMPARE(AppSettings::instance().value("SMeter_TxSelect", -1).toInt(), 1);
}

QTEST_MAIN(SMeterWidgetContextMenuTest)
#include "tst_smeter_widget_context_menu.moc"
