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
//   6 - "Disconnect" / "Connect"    -> connectionToggleRequested()
//   7 - "Copy diagnostics to clipboard" -> diagnosticsCopyRequested()
//
// Three test slots:
//   saveCurrentMemoryStoresSlot  - Save action stores relay values in TuneMemoryStore
//   clearActionRemovesEntry      - Clear action removes the stored entry
//   menuOpensTgxlAdvanced        - action 0 emits navigationRequested("tgxlAdvanced")
//
// NereusSDR-native test; Phase 3P-II Phase 4 Task 89.
// 2026-09-25: R-R3-49 (parity Task 8): in a remote window Recall tune
// memory is offered (it copies the stored values into the bars and sends
// nothing) and Open TGXL Advanced opens the Tuner Genius tab. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QMenu>
#include <QAction>

#include "gui/applets/TunerApplet.h"
#include "core/TgxlConnection.h"
#include "core/TuneMemoryStore.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/TunerModel.h"

using namespace NereusSDR;

class TunerAppletContextMenuTest : public QObject {
    Q_OBJECT
private slots:
    void saveCurrentMemoryStoresSlot();
    void clearActionRemovesEntry();
    void menuOpensTgxlAdvanced();
    void remoteTuningTelemetryUpdatesVisualsWithoutCarrierOrchestration();
    void receiveOnlyPermissionKeepsAccessoryCommandsDisabled();
    void remoteDisconnectMarksCachedTelemetryStale();
    void remoteConnectionActionNavigatesToPeripheralsAndLocalActionRemains();
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

void TunerAppletContextMenuTest::remoteTuningTelemetryUpdatesVisualsWithoutCarrierOrchestration()
{
    // R-R3-25: an isTuning=true snapshot or delta describes what Core sees
    // at the station. It must paint the remote applet, but it is not a local
    // hardware-TUNE event and must not call RadioModel::startTgxlAutotune().
    RadioModel model(RadioModel::Role::Remote);
    TunerModel* const tuner = model.tunerModel();
    QVERIFY(tuner != nullptr);
    TunerApplet applet(&model, tuner);

    // Make the old orchestration path observable without opening a socket:
    // a V frame marks the GUI-local test connection connected, after which
    // startTgxlAutotune() reaches setTune(true) and emits tuneRefused on a
    // Role::Remote model. The corrected telemetry path never gets there.
    model.tgxlConnection()->injectLineForTesting(QStringLiteral("V1.2.17"));
    QSignalSpy refused(&model, &RadioModel::tuneRefused);
    QSignalSpy localFrames(model.tgxlConnection(),
                           &TgxlConnection::testFrameWrittenForTesting);
    localFrames.clear();

    QVERIFY(tuner->applyStationValue(QByteArrayLiteral("isTuning"), true));

    QVERIFY(tuner->isTuning());
    QCOMPARE(applet.tuneButtonTextForTesting(), QStringLiteral("TUNING..."));
    QVERIFY(!applet.carrierEngagedForTgxlTuneForTesting());
    QCOMPARE(refused.count(), 0);
    QCOMPARE(localFrames.count(), 0);
}

void TunerAppletContextMenuTest::receiveOnlyPermissionKeepsAccessoryCommandsDisabled()
{
    RadioModel model(RadioModel::Role::Remote);
    TunerModel* const tuner = model.tunerModel();
    QVERIFY(tuner != nullptr);
    model.tgxlConnection()->injectLineForTesting(QStringLiteral("V1.2.17"));

    TuneMemoryStore store;
    store.store({1, Band::Band20m, 4, 5, 6, 1});
    TunerApplet applet(&model, tuner, nullptr, &store);
    applet.testSetCurrentBandAndAntenna(Band::Band20m, 1);
    applet.setTransmitPermitted(
        false, QStringLiteral("TX is unavailable on this receive-only station"));

    QVERIFY(!applet.actuatingControlsEnabledForTesting());

    QSignalSpy localFrames(model.tgxlConnection(),
                           &TgxlConnection::testFrameWrittenForTesting);
    QVERIFY(QMetaObject::invokeMethod(&applet, "cycleOperateState",
                                      Qt::DirectConnection));
    QCOMPARE(localFrames.count(), 0);

    QMenu* menu = applet.buildContextMenuForTesting();
    QVERIFY(menu != nullptr);
    QAction* recallAction = nullptr;
    for (QAction* action : menu->actions()) {
        if (action->text() == QStringLiteral("Recall tune memory")) {
            recallAction = action;
            break;
        }
    }
    QVERIFY(recallAction != nullptr);
    // R-R3-49 (parity Task 8): recall copies the stored values into the
    // bars and sends nothing, so a remote window offers it.
    QVERIFY(recallAction->isEnabled());
    recallAction->trigger();
    QCOMPARE(localFrames.count(), 0);

    // A telemetry refresh changes labels and readouts, but cannot undo the
    // negotiated permission that owns command availability.
    QVERIFY(tuner->applyStationValue(QByteArrayLiteral("isOperate"), true));
    QVERIFY(!applet.actuatingControlsEnabledForTesting());
    QCOMPARE(localFrames.count(), 0);
    menu->deleteLater();
}

void TunerAppletContextMenuTest::remoteDisconnectMarksCachedTelemetryStale()
{
    RadioModel model(RadioModel::Role::Remote);
    TunerModel* const tuner = model.tunerModel();
    QVERIFY(tuner != nullptr);
    TunerApplet applet(&model, tuner);

    QVERIFY(tuner->applyStationValue(QByteArrayLiteral("relayC1"), 42));
    QVERIFY(tuner->applyStationValue(QByteArrayLiteral("isOperate"), true));
    QCOMPARE(tuner->relayC1(), 42);
    QVERIFY(tuner->isOperate());
    QVERIFY(applet.staleIndicatorVisibleForTesting());

    applet.setStationConnected(true);
    QVERIFY(!applet.staleIndicatorVisibleForTesting());
    applet.setStationConnected(false);
    QVERIFY(applet.staleIndicatorVisibleForTesting());

    // Disconnect presentation must not erase last-known station state.
    QCOMPARE(tuner->relayC1(), 42);
    QVERIFY(tuner->isOperate());

    RadioModel localModel;
    TunerApplet localApplet(&localModel, localModel.tunerModel());
    localApplet.setStationConnected(false);
    QVERIFY(!localApplet.staleIndicatorVisibleForTesting());
}

void TunerAppletContextMenuTest::remoteConnectionActionNavigatesToPeripheralsAndLocalActionRemains()
{
    const auto findConnectionAction = [](QMenu* menu) -> QAction* {
        for (QAction* action : menu->actions()) {
            if (action->text() == QStringLiteral("Disconnect")
                || action->text() == QStringLiteral("Connect")
                || action->text() == QStringLiteral("Configure remote TGXL...")) {
                return action;
            }
        }
        return nullptr;
    };

    RadioModel remoteModel(RadioModel::Role::Remote);
    TunerApplet remoteApplet(&remoteModel, remoteModel.tunerModel());
    QSignalSpy remoteNavigation(&remoteApplet, &TunerApplet::navigationRequested);
    QMenu* remoteMenu = remoteApplet.buildContextMenuForTesting();
    QAction* remoteAdvancedAction = nullptr;
    for (QAction* action : remoteMenu->actions()) {
        if (action->text() == QStringLiteral("Open TGXL Advanced...")) {
            remoteAdvancedAction = action;
            break;
        }
    }
    QVERIFY(remoteAdvancedAction != nullptr);
    // R-R3-49 (parity Task 8): the remote Tuner Genius tab shows the Core's.
    QVERIFY(remoteAdvancedAction->isEnabled());
    remoteAdvancedAction->trigger();
    QCOMPARE(remoteNavigation.count(), 1);
    QCOMPARE(remoteNavigation.takeFirst().at(0).toString(), QStringLiteral("tgxlAdvanced"));

    QAction* const remoteAction = findConnectionAction(remoteMenu);
    QVERIFY(remoteAction != nullptr);
    QCOMPARE(remoteAction->text(), QStringLiteral("Configure remote TGXL..."));
    QVERIFY(remoteAction->isEnabled());
    remoteAction->trigger();
    QCOMPARE(remoteNavigation.count(), 1);
    QCOMPARE(remoteNavigation.takeFirst().at(0).toString(), QStringLiteral("peripherals"));
    remoteMenu->deleteLater();

    RadioModel localModel;
    TunerApplet localApplet(&localModel, localModel.tunerModel());
    QSignalSpy localToggle(&localApplet, &TunerApplet::connectionToggleRequested);
    QMenu* localMenu = localApplet.buildContextMenuForTesting();
    QAction* const localAction = findConnectionAction(localMenu);
    QVERIFY(localAction != nullptr);
    QVERIFY(localAction->isEnabled());
    localAction->trigger();
    QCOMPARE(localToggle.count(), 1);
    localMenu->deleteLater();
}

QTEST_MAIN(TunerAppletContextMenuTest)
#include "tst_tuner_applet_context_menu.moc"
