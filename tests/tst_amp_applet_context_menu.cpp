// tests/tst_amp_applet_context_menu.cpp
//
// Verifies AmpApplet right-click context menu structure and signal emission
// via the buildContextMenuForTesting() test seam.
//
// Test seam: buildContextMenuForTesting() calls buildContextMenu(this) and
// returns the heap-allocated QMenu* without exec()-ing it. This lets tests
// inspect actions and trigger them without a live event loop or real mouse
// events. Same pattern as SMeterWidget::buildContextMenuForTesting() (Task 38,
// commit 067d2d5b).
//
// Menu order (per design doc ss5.9):
//   0 - "Open PGXL Advanced..." action -> navigationRequested("pgxlAdvanced")
//   1 - separator
//   2 - "Disconnect" / "Reconnect"     -> connectionToggleRequested()
//   3 - "Copy diagnostics to clipboard" -> diagnosticsCopyRequested()
//
// Three test slots:
//   menuOpensPgxlAdvanced          - triggering action 0 emits navigationRequested("pgxlAdvanced")
//   disconnectActionEmitsToggle    - triggering the Disconnect action emits connectionToggleRequested()
//   copyDiagnosticsActionEmitsSignal - triggering the copy action emits diagnosticsCopyRequested()
//
// NereusSDR-native test; Phase 3P-II Phase 4 Task 88.

#include <QtTest>
#include <QMenu>
#include <QAction>

#include "gui/applets/AmpApplet.h"

using namespace NereusSDR;

class AmpAppletContextMenuTest : public QObject {
    Q_OBJECT
private slots:
    void menuOpensPgxlAdvanced();
    void disconnectActionEmitsToggle();
    void copyDiagnosticsActionEmitsSignal();
};

// Triggering the first action ("Open PGXL Advanced...") must emit
// navigationRequested with "pgxlAdvanced" as the page key.
void AmpAppletContextMenuTest::menuOpensPgxlAdvanced()
{
    NereusSDR::AmpApplet a(nullptr);
    QSignalSpy spy(&a, &NereusSDR::AmpApplet::navigationRequested);

    QMenu* menu = a.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);

    // First non-separator action is "Open PGXL Advanced..."
    const auto actions = menu->actions();
    QVERIFY2(!actions.isEmpty(), "Context menu must have at least one action");
    actions.first()->trigger();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("pgxlAdvanced"));

    menu->deleteLater();
}

// Triggering "Disconnect" (when pgxlConnected is true) must emit
// connectionToggleRequested().
void AmpAppletContextMenuTest::disconnectActionEmitsToggle()
{
    NereusSDR::AmpApplet a(nullptr);
    a.setPgxlConnected(true);  // causes the label to read "Disconnect"

    QSignalSpy spy(&a, &NereusSDR::AmpApplet::connectionToggleRequested);

    QMenu* menu = a.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);

    // Find the Disconnect action by text
    QAction* toggleAction = nullptr;
    for (QAction* act : menu->actions()) {
        if (act->text() == QStringLiteral("Disconnect")) {
            toggleAction = act;
            break;
        }
    }
    QVERIFY2(toggleAction != nullptr,
             "Context menu must contain a 'Disconnect' action when pgxlConnected==true");
    toggleAction->trigger();

    QCOMPARE(spy.count(), 1);

    menu->deleteLater();
}

// Triggering "Copy diagnostics to clipboard" must emit diagnosticsCopyRequested().
void AmpAppletContextMenuTest::copyDiagnosticsActionEmitsSignal()
{
    NereusSDR::AmpApplet a(nullptr);
    QSignalSpy spy(&a, &NereusSDR::AmpApplet::diagnosticsCopyRequested);

    QMenu* menu = a.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);

    QAction* copyAction = nullptr;
    for (QAction* act : menu->actions()) {
        if (act->text() == QStringLiteral("Copy diagnostics to clipboard")) {
            copyAction = act;
            break;
        }
    }
    QVERIFY2(copyAction != nullptr,
             "Context menu must contain a 'Copy diagnostics to clipboard' action");
    copyAction->trigger();

    QCOMPARE(spy.count(), 1);

    menu->deleteLater();
}

QTEST_MAIN(AmpAppletContextMenuTest)
#include "tst_amp_applet_context_menu.moc"
