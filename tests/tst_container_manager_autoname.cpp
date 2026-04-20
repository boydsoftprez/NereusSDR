// =================================================================
// tests/tst_container_manager_autoname.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 17 — ContainerManager::nextMeterAutoName
// canonical auto-name helper. Verifies it picks the smallest unused
// positive integer across existing container notes.
//
// no-port-check: test scaffolding; helper is NereusSDR-original.
// =================================================================

#include <QtTest/QtTest>
#include <QSplitter>
#include <QWidget>

#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"

using namespace NereusSDR;

class TstContainerManagerAutoname : public QObject
{
    Q_OBJECT

private slots:
    void nextMeterAutoName_returnsMeter1_whenEmpty();
    void nextMeterAutoName_picksMeter1_whenNoMatchingNames();
    void nextMeterAutoName_skipsUsedIntegers();
    void nextMeterAutoName_ignoresNonMatchingFormats();
};

void TstContainerManagerAutoname::nextMeterAutoName_returnsMeter1_whenEmpty()
{
    QWidget dockParent;
    QSplitter splitter;
    ContainerManager mgr(&dockParent, &splitter);
    QCOMPARE(mgr.nextMeterAutoName(), QStringLiteral("Meter 1"));
}

void TstContainerManagerAutoname::nextMeterAutoName_picksMeter1_whenNoMatchingNames()
{
    QWidget dockParent;
    QSplitter splitter;
    ContainerManager mgr(&dockParent, &splitter);

    ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
    c->setNotes(QStringLiteral("Power / SWR"));

    QCOMPARE(mgr.nextMeterAutoName(), QStringLiteral("Meter 1"));
}

void TstContainerManagerAutoname::nextMeterAutoName_skipsUsedIntegers()
{
    QWidget dockParent;
    QSplitter splitter;
    ContainerManager mgr(&dockParent, &splitter);

    ContainerWidget* c1 = mgr.createContainer(1, DockMode::Floating);
    c1->setNotes(QStringLiteral("Meter 1"));
    ContainerWidget* c3 = mgr.createContainer(1, DockMode::Floating);
    c3->setNotes(QStringLiteral("Meter 3"));

    // 2 is the smallest unused positive integer.
    QCOMPARE(mgr.nextMeterAutoName(), QStringLiteral("Meter 2"));

    ContainerWidget* c2 = mgr.createContainer(1, DockMode::Floating);
    c2->setNotes(QStringLiteral("Meter 2"));

    // With 1/2/3 taken, next is 4.
    QCOMPARE(mgr.nextMeterAutoName(), QStringLiteral("Meter 4"));
}

void TstContainerManagerAutoname::nextMeterAutoName_ignoresNonMatchingFormats()
{
    QWidget dockParent;
    QSplitter splitter;
    ContainerManager mgr(&dockParent, &splitter);

    // Regex is anchored: "Meter Foo", "Meter 1a", "meter 1" all fail.
    ContainerWidget* c1 = mgr.createContainer(1, DockMode::Floating);
    c1->setNotes(QStringLiteral("Meter Foo"));
    ContainerWidget* c2 = mgr.createContainer(1, DockMode::Floating);
    c2->setNotes(QStringLiteral("Meter 1a"));
    ContainerWidget* c3 = mgr.createContainer(1, DockMode::Floating);
    c3->setNotes(QStringLiteral("meter 1"));

    QCOMPARE(mgr.nextMeterAutoName(), QStringLiteral("Meter 1"));
}

QTEST_MAIN(TstContainerManagerAutoname)
#include "tst_container_manager_autoname.moc"
