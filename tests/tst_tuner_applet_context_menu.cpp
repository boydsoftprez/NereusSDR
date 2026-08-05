// tests/tst_tuner_applet_context_menu.cpp
//
// Verifies TunerApplet right-click context menu signal emission and
// TuneMemoryStore interactions via the buildContextMenuForTesting() test seam.
//
// Test seam: buildContextMenuForTesting() calls buildContextMenu(this) and
// returns the heap-allocated QMenu* without exec()-ing it. Same pattern as
// AmpApplet (Task 88) and SMeterWidget (Task 38, commit 067d2d5b).
//
// Menu structure (per design doc ss5.9):
//   0 - "Open TGXL Advanced..."     -> navigationRequested("tgxlAdvanced")
//   1 - separator
//   2 - "Save current tune memory"  -> m_tuneStore->store(currentMem())
//   3 - "Recall tune memory"        -> apply stored relay positions
//   4 - "Clear tune memory"         -> m_tuneStore->clear(ant, band)
//   5 - separator
//   6 - "Disconnect" / "Reconnect"  -> connectionToggleRequested()
//   7 - "Copy diagnostics to clipboard" -> diagnosticsCopyRequested()
//
// Three test slots:
//   saveCurrentMemoryStoresSlot  - Save action stores relay values in TuneMemoryStore
//   clearActionRemovesEntry      - Clear action removes the stored entry
//   menuOpensTgxlAdvanced        - action 0 emits navigationRequested("tgxlAdvanced")
//
// NereusSDR-native test; Phase 3P-II Phase 4 Task 89.

#include <QtTest>
#include <QMenu>
#include <QAction>

#include "gui/applets/TunerApplet.h"
#include "core/TuneMemoryStore.h"
#include "models/Band.h"

using namespace NereusSDR;

class TunerAppletContextMenuTest : public QObject {
    Q_OBJECT
private slots:
    void saveCurrentMemoryStoresSlot();
    void clearActionRemovesEntry();
    void menuOpensTgxlAdvanced();
};

// Triggering "Save current tune memory" must store C1=42, L=199, C2=88
// for Band::Band20m / antenna 1 in the TuneMemoryStore.
void TunerAppletContextMenuTest::saveCurrentMemoryStoresSlot()
{
    NereusSDR::TuneMemoryStore store;
    // Construct with nullptr RadioModel + TunerModel; pass store directly.
    NereusSDR::TunerApplet a(nullptr, nullptr, nullptr, &store);
    a.testSetCurrentBandAndAntenna(NereusSDR::Band::Band20m, 1);
    a.testSetRelayValues(42, 199, 88);

    QMenu* menu = a.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);

    QAction* saveAction = nullptr;
    for (QAction* act : menu->actions()) {
        if (act->text() == QStringLiteral("Save current tune memory")) {
            saveAction = act;
            break;
        }
    }
    QVERIFY2(saveAction != nullptr,
             "Context menu must contain 'Save current tune memory'");
    saveAction->trigger();

    auto rec = store.recall(1, NereusSDR::Band::Band20m);
    QVERIFY2(rec.has_value(), "TuneMemoryStore must contain an entry after Save");
    QCOMPARE(rec->c1, 42);
    QCOMPARE(rec->l, 199);
    QCOMPARE(rec->c2, 88);

    menu->deleteLater();
}

// After saving, triggering "Clear tune memory" must remove the stored entry.
void TunerAppletContextMenuTest::clearActionRemovesEntry()
{
    NereusSDR::TuneMemoryStore store;
    NereusSDR::TunerApplet a(nullptr, nullptr, nullptr, &store);
    a.testSetCurrentBandAndAntenna(NereusSDR::Band::Band40m, 2);
    a.testSetRelayValues(10, 20, 30);

    // Pre-condition: save a memory slot.
    store.store({2, NereusSDR::Band::Band40m, 10, 20, 30, 0LL});

    QMenu* menu = a.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);

    QAction* clearAction = nullptr;
    for (QAction* act : menu->actions()) {
        if (act->text() == QStringLiteral("Clear tune memory")) {
            clearAction = act;
            break;
        }
    }
    QVERIFY2(clearAction != nullptr,
             "Context menu must contain 'Clear tune memory'");
    clearAction->trigger();

    auto rec = store.recall(2, NereusSDR::Band::Band40m);
    QVERIFY2(!rec.has_value(), "TuneMemoryStore must be empty after Clear");

    menu->deleteLater();
}

// Triggering the first action ("Open TGXL Advanced...") must emit
// navigationRequested with "tgxlAdvanced" as the page key.
void TunerAppletContextMenuTest::menuOpensTgxlAdvanced()
{
    NereusSDR::TunerApplet a(nullptr, nullptr, nullptr, nullptr);
    QSignalSpy spy(&a, &NereusSDR::TunerApplet::navigationRequested);

    QMenu* menu = a.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);

    const auto actions = menu->actions();
    QVERIFY2(!actions.isEmpty(), "Context menu must have at least one action");
    actions.first()->trigger();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("tgxlAdvanced"));

    menu->deleteLater();
}

QTEST_MAIN(TunerAppletContextMenuTest)
#include "tst_tuner_applet_context_menu.moc"
