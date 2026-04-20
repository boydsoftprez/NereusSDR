// =================================================================
// tests/tst_dialog_add_container.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 14 — + Add container button.
//
// Creates a new floating container, auto-names it "Meter N" with
// the smallest unused positive integer, and switches the dialog's
// focus to the new container. This test asserts both slots from
// the spec:
//   1. The click creates the container and the dropdown is updated.
//   2. nextAutoName() picks the smallest unused Meter-N integer
//      even when existing containers have non-contiguous names.
//
// no-port-check: test scaffolding; dialog API is NereusSDR-original.
// =================================================================

#include <QtTest/QtTest>
#include <QSplitter>
#include <QWidget>

#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

class TstDialogAddContainer : public QObject
{
    Q_OBJECT

private slots:
    void clickAdd_createsNewContainerAndSwitches();
    void autoName_pickSmallestUnusedInteger();
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
        container->setNotes(QStringLiteral("Meter 1"));
        meter = new MeterWidget();
        container->setContent(meter);
    }
};

} // namespace

void TstDialogAddContainer::clickAdd_createsNewContainerAndSwitches()
{
    Harness h;
    ContainerSettingsDialog dlg(h.container, nullptr, &h.mgr);
    QCOMPARE(h.mgr.allContainers().size(), 1);

    dlg.triggerAddContainerForTest();

    QCOMPARE(h.mgr.allContainers().size(), 2);
    // The dropdown should now show the new container's name — the
    // auto-name picker returns "Meter 2" since "Meter 1" is taken.
    QCOMPARE(dlg.currentContainerDropdownText(),
             QStringLiteral("Meter 2"));
}

void TstDialogAddContainer::autoName_pickSmallestUnusedInteger()
{
    QWidget dockParent;
    QSplitter splitter;
    ContainerManager mgr(&dockParent, &splitter);

    // Seed the manager with three containers: Meter 1 / Meter 2 /
    // Meter 3, then destroy Meter 1 so "1" becomes free.
    ContainerWidget* c1 = mgr.createContainer(1, DockMode::Floating);
    c1->setNotes(QStringLiteral("Meter 1"));
    c1->setContent(new MeterWidget());
    ContainerWidget* c2 = mgr.createContainer(1, DockMode::Floating);
    c2->setNotes(QStringLiteral("Meter 2"));
    c2->setContent(new MeterWidget());
    ContainerWidget* c3 = mgr.createContainer(1, DockMode::Floating);
    c3->setNotes(QStringLiteral("Meter 3"));
    c3->setContent(new MeterWidget());
    const QString c1Id = c1->id();
    mgr.destroyContainer(c1Id);
    QCOMPARE(mgr.allContainers().size(), 2);

    ContainerSettingsDialog dlg(c2, nullptr, &mgr);
    dlg.triggerAddContainerForTest();

    QCOMPARE(mgr.allContainers().size(), 3);
    // Smallest unused = 1 (since Meter 2 and Meter 3 remain).
    QCOMPARE(dlg.currentContainerDropdownText(),
             QStringLiteral("Meter 1"));
}

QTEST_MAIN(TstDialogAddContainer)
#include "tst_dialog_add_container.moc"
