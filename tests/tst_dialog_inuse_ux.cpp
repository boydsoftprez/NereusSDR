// =================================================================
// tests/tst_dialog_inuse_ux.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 13 — in-use list UX.
//
// Each in-use row carries a ContainerInUseRow wrapper: a stable
// rowId (survives drag-reorder), a user-editable displayName, and
// the MeterItem pointer. This test locks the Rename / Duplicate /
// Delete context-menu behaviours, including the hybrid-rule guard
// that blocks Duplicate on presets.
//
// no-port-check: test scaffolding; exercises NereusSDR-original
// dialog API only.
// =================================================================

#include <QtTest/QtTest>
#include <QSplitter>
#include <QWidget>

#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"

using namespace NereusSDR;

class TstDialogInUseUx : public QObject
{
    Q_OBJECT

private slots:
    void renameItem_persistsInDisplayList();
    void duplicatePrimitive_addsSecondEntry();
    void duplicatePreset_isBlocked();
    void deleteItem_removesFromWorking();
};

namespace {

struct Harness {
    QWidget dockParent;
    QSplitter splitter;
    ContainerManager mgr{&dockParent, &splitter};
    ContainerWidget* container{nullptr};
    MeterWidget* meter{nullptr};

    Harness()
    {
        container = mgr.createContainer(1, DockMode::Floating);
        Q_ASSERT(container);
        meter = new MeterWidget();
        // Pre-populate with one BarItem primitive so the dialog's
        // populateItemList captures something to start with.
        meter->addItem(new BarItem());
        container->setContent(meter);
    }
};

} // namespace

void TstDialogInUseUx::renameItem_persistsInDisplayList()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    QCOMPARE(dlg.workingItems().size(), 1);

    const QUuid id = dlg.rowIdAtIndex(0);
    QVERIFY(!id.isNull());

    dlg.triggerRenameForTest(id, QStringLiteral("Main Bar"));
    QCOMPARE(dlg.displayNameForRowId(id), QStringLiteral("Main Bar"));
}

void TstDialogInUseUx::duplicatePrimitive_addsSecondEntry()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    QCOMPARE(dlg.workingItems().size(), 1);

    const QUuid id = dlg.rowIdAtIndex(0);
    dlg.triggerDuplicateForTest(id);

    QCOMPARE(dlg.workingItems().size(), 2);
    // Neither entry is lost, and the duplicate's displayName is the
    // original's name + " (copy)".
    const QUuid newId = dlg.rowIdAtIndex(1);
    QVERIFY(newId != id);
    QVERIFY(dlg.displayNameForRowId(newId).endsWith(QStringLiteral(" (copy)")));
}

void TstDialogInUseUx::duplicatePreset_isBlocked()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    // Clear out the primitive BarItem that came along via the harness
    // so we can start from a clean slate and add a preset.
    const QUuid primId = dlg.rowIdAtIndex(0);
    dlg.triggerDeleteForTest(primId);
    QCOMPARE(dlg.workingItems().size(), 0);

    dlg.appendPresetRowForTest(QStringLiteral("AnanMM"));
    QCOMPARE(dlg.workingItems().size(), 1);
    const QUuid presetId = dlg.rowIdAtIndex(0);

    // Duplicate is a no-op for presets (hybrid rule).
    dlg.triggerDuplicateForTest(presetId);
    QCOMPARE(dlg.workingItems().size(), 1);
}

void TstDialogInUseUx::deleteItem_removesFromWorking()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    QCOMPARE(dlg.workingItems().size(), 1);

    const QUuid id = dlg.rowIdAtIndex(0);
    dlg.triggerDeleteForTest(id);

    QCOMPARE(dlg.workingItems().size(), 0);
    QVERIFY(dlg.displayNameForRowId(id).isEmpty());
}

QTEST_MAIN(TstDialogInUseUx)
#include "tst_dialog_inuse_ux.moc"
