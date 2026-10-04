// Regression: a synthetic Qt Cocoa table cell must not delete its borrowed
// parent ID, and incompatible/intercepted methods must never be overwritten.
// JJ Boyd (KG4VCF), 2026-10-03, with OpenAI Codex assistance.
#include "gui/QtCocoaAccessibilityOwnershipGuard.h"
#include "gui/ConnectionSelector.h"
#include "gui/QtCocoaAccessibilityOwnershipGuard_p.h"
#include "gui/applets/RxApplet.h"
#include "core/BoardCapabilities.h"
#include "core/StepAttenuatorFacade.h"
#include "models/RadioModel.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QAccessible>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QtTest>
#include <memory>
#include <thread>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

@interface NSObject (NereusOwnershipRegression)
+ (id)elementWithId:(unsigned int)identifier;
+ (void)removeElementsFromCache:(NSArray*)elements;
- (id)initWithId:(unsigned int)identifier;
- (id)initWithId:(unsigned int)identifier role:(NSAccessibilityRole)role;
- (NSArray*)accessibilitySelectedChildren;
- (NSArray*)accessibilityRows;
- (NSArray*)accessibilityChildren;
- (QAccessibleInterface*)qtInterface;
- (void)updateTableModel;
@end

namespace {
void foreignRemove(id, SEL, NSArray*) {}
}

class QtCocoaOwnershipTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
#if !defined(Q_PROCESSOR_ARM_64)
        QSKIP("Native ownership regression requires the pinned Qt6.11.0 arm64 Cocoa image.");
#endif
        if (qVersion() != QStringLiteral("6.11.0") || QApplication::platformName() != QStringLiteral("cocoa")) {
            QSKIP("Native ownership regression requires the pinned Qt6.11.0 Cocoa image.");
        }
    }

    void unsupportedClassAbi()
    {
        // Real Objective-C metadata from legal disposable classes; the same
        // validator gates installation. No Qt class layout is mutated.
        for (int variant = 0; variant < 4; ++variant) {
            const QByteArray name = QByteArray("NereusAbiControl") + QByteArray::number(variant);
            Class cls = objc_allocateClassPair([NSObject class], name.constData(), 0);
            QVERIFY(cls);
            QVERIFY(class_addProtocol(cls, @protocol(NSAccessibilityElement)));
            QVERIFY(class_addIvar(cls, "axid", variant == 1 ? 8 : 4, variant == 1 ? 3 : 2, variant == 1 ? "Q" : "I"));
            QVERIFY(class_addIvar(cls, "m_rowIndex", 4, 2, "i"));
            QVERIFY(class_addIvar(cls, "m_columnIndex", 4, 2, "i"));
            QVERIFY(class_addIvar(cls, "rows", sizeof(id), 3, "@\"NSMutableArray\""));
            QVERIFY(class_addIvar(cls, "columns", sizeof(id), 3, "@\"NSMutableArray\""));
            if (variant != 2) {
                QVERIFY(class_addIvar(cls, "synthesizedRole", sizeof(id), 3, variant == 3 ? "@\"NSNumber\"" : "@\"NSString\""));
            }
            objc_registerClassPair(cls);
            QCOMPARE(QtCocoaOwnershipDetail::classAbiMatches(cls), variant == 0);
            objc_disposeClassPair(cls);
        }
    }

    void rejectedThreadAndInterceptorDoNotInstall()
    {
        QString reason;
        bool workerAccepted = true;
        std::thread worker([&] { workerAccepted = installQtCocoaAccessibilityOwnershipGuard(&reason); });
        worker.join();
        QVERIFY(!workerAccepted);
        QVERIFY(!reason.isEmpty());
        {
            QPushButton earlyWidget("Not shown");
            QVERIFY(!installQtCocoaAccessibilityOwnershipGuard(&reason));
            QVERIFY(!reason.isEmpty());
        }
        Class cls = NSClassFromString(@"QMacAccessibilityElement");
        QVERIFY(cls);
        Method dealloc = class_getInstanceMethod(cls, sel_registerName("dealloc"));
        Method remove = class_getClassMethod(cls, sel_registerName("removeElementsFromCache:"));
        IMP savedDealloc = method_getImplementation(dealloc);
        IMP savedRemove = method_setImplementation(remove, reinterpret_cast<IMP>(foreignRemove));
        const bool accepted = installQtCocoaAccessibilityOwnershipGuard(&reason);
        const bool deallocUnchanged = method_getImplementation(dealloc) == savedDealloc;
        const bool foreignUnchanged = method_getImplementation(remove) == reinterpret_cast<IMP>(foreignRemove);
        method_setImplementation(remove, savedRemove);
        QVERIFY(!accepted);
        QVERIFY(deallocUnchanged);
        QVERIFY(foreignUnchanged);
        QVERIFY(!reason.isEmpty());
    }

    void connectionRefreshAfterNativeTableRebuild()
    {
        QString reason;
        QVERIFY2(installQtCocoaAccessibilityOwnershipGuard(&reason), qPrintable(reason));
        QAccessible::setActive(true);
        NereusSDR::ConnectionSelector selector;
        NereusSDR::ConnectionTargetRow first{QStringLiteral("first"),
            NereusSDR::ConnectionTargetKind::SavedCore, QStringLiteral("First"),
            QStringLiteral("Test"), QStringLiteral("private"), QStringLiteral("Idle")};
        NereusSDR::ConnectionTargetRow selected = first;
        selected.key = QStringLiteral("selected");
        selected.name = QStringLiteral("Selected");
        selector.setTargets({first, selected});
        selector.setSelectedKey(selected.key);
        auto* tree = selector.findChild<QTreeWidget*>(QStringLiteral("connectionSelectorTargets"));
        QVERIFY(tree);
        tree->resize(820, 400);
        tree->doItemsLayout();
        QTreeWidgetItem* selectedItem = tree->currentItem();
        const QPersistentModelIndex selectedIndex = tree->indexFromItem(selectedItem);
        QSignalSpy connectSpy(&selector, &NereusSDR::ConnectionSelector::connectRequested);
        QSignalSpy resetSpy(tree->model(), &QAbstractItemModel::modelReset);
        QAccessibleInterface* table = QAccessible::queryAccessibleInterface(tree);
        const QAccessible::Id tableId = QAccessible::uniqueId(table);
        Class cls = NSClassFromString(@"QMacAccessibilityElement");
        id element = [cls elementWithId:tableId];
        const QList<QAccessibleInterface*> cells = table->selectionInterface()->selectedItems();
        QCOMPARE(cells.size(), 4);
        QList<QAccessible::Id> oldIds;
        for (QAccessibleInterface* cell : cells) {
            oldIds.append(QAccessible::uniqueId(cell));
        }
        tree->doItemsLayout();
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        [element updateTableModel];
        NSArray* native = [element accessibilitySelectedChildren];
        QCOMPARE(native.count, NSUInteger(4));
        for (id child in native) {
            QVERIFY([child qtInterface]);
        }
        [element updateTableModel];
        [pool drain];
        // Qt 6.11 Cocoa frees real cell interfaces with its old native rows,
        // leaving their IDs in QAccessibleTable's independent child cache.
        for (QAccessible::Id identifier : oldIds) {
            QVERIFY(!QAccessible::accessibleInterface(identifier));
        }
        QCOMPARE(QAccessible::accessibleInterface(tableId), table);
        QVERIFY(!selector.isVisible());

        selector.setTargets({selected});

        QCOMPARE(selector.selectedKey(), selected.key);
        QCOMPARE(tree->currentItem(), selectedItem);
        QVERIFY(selectedIndex.isValid());
        QCOMPARE(tree->itemFromIndex(selectedIndex), selectedItem);
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(connectSpy.count(), 0);
        tree->doItemsLayout();
        NSAutoreleasePool* freshPool = [[NSAutoreleasePool alloc] init];
        NSArray* fresh = [element accessibilitySelectedChildren];
        QCOMPARE(fresh.count, NSUInteger(4));
        for (id child in fresh) {
            QAccessibleInterface* cell = [child qtInterface];
            QVERIFY(cell && cell->isValid());
        }
        [freshPool drain];
        selector.setTargets({});
        QVERIFY(selector.selectedKey().isEmpty());
        QVERIFY(table->selectionInterface()->selectedItems().isEmpty());
    }

    void preampRefreshAfterNativePopupRebuild()
    {
        QString reason;
        QVERIFY2(installQtCocoaAccessibilityOwnershipGuard(&reason), qPrintable(reason));
        QAccessible::setActive(true);
        NereusSDR::RadioModel remote(NereusSDR::RadioModel::Role::Remote);
        NereusSDR::RxApplet applet(nullptr, &remote);
        applet.setBoardCapabilities(NereusSDR::BoardCapsTable::forBoard(NereusSDR::HPSDRHW::Saturn));
        auto* stack = applet.findChild<QStackedWidget*>(QStringLiteral("RxAttenuatorStack"));
        QVERIFY(stack);
        auto* combo = stack->findChild<QComboBox*>();
        QVERIFY(combo);
        QCOMPARE(combo->count(), 4);
        NereusSDR::StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        stepAtt->setWindowAvailability(true, QString());
        const int selectedMode = combo->itemData(2).toInt();
        stepAtt->setPreampMode(selectedMode);
        QCOMPARE(combo->currentData().toInt(), selectedMode);
        QSignalSpy modeChanges(stepAtt, &NereusSDR::StepAttenuatorFacade::preampModeChanged);
        QSignalSpy comboChanges(combo, &QComboBox::currentIndexChanged);
        QAbstractItemView* view = combo->view();
        view->resize(300, 200);
        view->doItemsLayout();
        QAccessibleInterface* table = QAccessible::queryAccessibleInterface(view);
        QVERIFY(table && table->tableInterface() && table->selectionInterface());
        const QAccessible::Id tableId = QAccessible::uniqueId(table);
        Class cls = NSClassFromString(@"QMacAccessibilityElement");
        id element = [cls elementWithId:tableId];
        QVERIFY(element);
        QVERIFY(!applet.isVisible());
        QVERIFY(!view->isVisible());

        for (NereusSDR::HPSDRHW board : {NereusSDR::HPSDRHW::Saturn,
                                       NereusSDR::HPSDRHW::Hermes,
                                       NereusSDR::HPSDRHW::Saturn}) {
            view->setCurrentIndex(combo->model()->index(combo->currentIndex(), 0));
            const QList<QAccessibleInterface*> selected = table->selectionInterface()->selectedItems();
            QCOMPARE(selected.size(), 1);
            const QAccessible::Id oldId = QAccessible::uniqueId(selected.first());
            NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
            [element updateTableModel];
            NSArray* rows = [element accessibilityRows];
            QCOMPARE(rows.count, NSUInteger(combo->count()));
            NSArray* native = [[rows objectAtIndex:combo->currentIndex()] accessibilityChildren];
            QCOMPARE(native.count, NSUInteger(1));
            QAccessibleInterface* promoted = [[native firstObject] qtInterface];
            QVERIFY(promoted);
            QCOMPARE(QAccessible::uniqueId(promoted), oldId);
            [element updateTableModel];
            [pool drain];
            // Actual Cocoa row cleanup expires a promoted popup cell while
            // QAccessibleTable still holds its ID. The next clear must not
            // dereference that absent cell in RowsRemoved.
            QVERIFY(!QAccessible::accessibleInterface(oldId));
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
            qInfo() << "Expired preamp popup cell ID before capability refresh:" << oldId;

            const auto& caps = NereusSDR::BoardCapsTable::forBoard(board);
            applet.setBoardCapabilities(caps);

            const auto expected = NereusSDR::BoardCapsTable::preampItemsForBoard(board, caps.hasAlexFilters);
            QCOMPARE(combo->count(), int(expected.size()));
            for (int row = 0; row < combo->count(); ++row) {
                QCOMPARE(combo->itemText(row), QString::fromLatin1(expected[row].label));
                QCOMPARE(combo->itemData(row).toInt(), expected[row].modeInt);
            }
            QCOMPARE(combo->currentData().toInt(), selectedMode);
            QCOMPARE(stepAtt->preampMode(), selectedMode);
            QCOMPARE(comboChanges.count(), 0);
            QCOMPARE(modeChanges.count(), 0);
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
            view->doItemsLayout();
            view->setCurrentIndex(combo->model()->index(combo->currentIndex(), 0));
            NSAutoreleasePool* freshPool = [[NSAutoreleasePool alloc] init];
            NSArray* fresh = [element accessibilitySelectedChildren];
            QCOMPARE(fresh.count, NSUInteger(1));
            QAccessibleInterface* cell = [[fresh firstObject] qtInterface];
            QVERIFY(cell && cell->isValid());
            [freshPool drain];
        }
    }

    void selectedChildrenLifecycle()
    {
        QString reason;
        QVERIFY2(installQtCocoaAccessibilityOwnershipGuard(&reason), qPrintable(reason));
        Class cls = NSClassFromString(@"QMacAccessibilityElement");
        Method dealloc = class_getInstanceMethod(cls, sel_registerName("dealloc"));
        Method remove = class_getClassMethod(cls, sel_registerName("removeElementsFromCache:"));
        IMP installedDealloc = method_getImplementation(dealloc);
        IMP installedRemove = method_getImplementation(remove);
        QVERIFY(installQtCocoaAccessibilityOwnershipGuard(&reason));
        QVERIFY(method_getImplementation(dealloc) == installedDealloc);
        QVERIFY(method_getImplementation(remove) == installedRemove);

        auto tree = std::make_unique<QTreeWidget>();
        tree->setColumnCount(4);
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        tree->setSelectionBehavior(QAbstractItemView::SelectRows);
        for (int n = 0; n < 3; ++n) {
            QTreeWidgetItem* group = new QTreeWidgetItem(tree.get(), {QString::number(n)});
            group->setFlags(Qt::ItemIsEnabled);
            QTreeWidgetItem* child = new QTreeWidgetItem(group, {"Name", "Radio", "Address", "Idle"});
            Q_UNUSED(child);
            group->setExpanded(true);
        }
        tree->resize(820, 400);
        tree->doItemsLayout();
        tree->setCurrentItem(tree->topLevelItem(2)->child(0));
        QVERIFY(!tree->isVisible());
        QAccessibleInterface* table = QAccessible::queryAccessibleInterface(tree.get());
        const QAccessible::Id tableId = QAccessible::uniqueId(table);
        id element = [cls elementWithId:tableId];
        QList<QAccessible::Id> lastIds;
        for (int wave = 0; wave < 3; ++wave) {
            NSAutoreleasePool* wavePool = [[NSAutoreleasePool alloc] init];
            const QList<QAccessibleInterface*> selected = table->selectionInterface()->selectedItems();
            QCOMPARE(selected.size(), wave == 0 ? 4 : 5);
            QList<QAccessible::Id> expected;
            for (QAccessibleInterface* child : selected) {
                expected.append(QAccessible::uniqueId(child));
            }
            NSArray* native = [element accessibilitySelectedChildren];
            QCOMPARE(native.count, NSUInteger(expected.size()));
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
            QList<QAccessible::Id> observed;
            for (id child in native) {
                QAccessibleInterface* accessible = [child qtInterface];
                QVERIFY(accessible && accessible->isValid());
                observed.append(QAccessible::uniqueId(accessible));
            }
            QCOMPARE(observed, expected);
            for (qsizetype n = 0; n < expected.size(); ++n) {
                QCOMPARE(QAccessible::accessibleInterface(expected[n]), selected[n]);
            }
            lastIds = expected;
            if (wave == 0) {
                tree->setColumnCount(5);
                tree->selectionModel()->select(tree->currentIndex(),
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            } else if (wave == 1) {
                tree->clear();
                QTreeWidgetItem* fresh = new QTreeWidgetItem(tree.get(), {"Fresh", "Radio", "Address", "Idle", "Extra"});
                tree->setCurrentItem(fresh);
            }
            if (wave < 2) {
                QAccessibleTableModelChangeEvent reset(tree.get(), QAccessibleTableModelChangeEvent::ModelReset);
                table->tableInterface()->modelChange(&reset);
                tree->doItemsLayout();
                [element updateTableModel];
            }
            [wavePool drain];
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
        }

        for (NSAccessibilityRole role : {NSAccessibilityRowRole, NSAccessibilityColumnRole, NSAccessibilityCellRole}) {
            NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
            id synthetic = [[cls alloc] initWithId:tableId role:role];
            QPushButton button("Real ownership control");
            const QAccessible::Id realId = QAccessible::uniqueId(QAccessible::queryAccessibleInterface(&button));
            id real = [cls elementWithId:realId];
            [cls removeElementsFromCache:@[synthetic, real]];
            const bool realDeleted = !QAccessible::accessibleInterface(realId);
            [synthetic release];
            [pool drain];
            QVERIFY(realDeleted);
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
            for (QAccessible::Id identifier : lastIds) {
                QAccessibleInterface* child = QAccessible::accessibleInterface(identifier);
                QVERIFY(child && child->isValid());
            }
        }
        QPushButton button("Real dealloc control");
        const QAccessible::Id realId = QAccessible::uniqueId(QAccessible::queryAccessibleInterface(&button));
        id owned = [[cls alloc] initWithId:realId];
        [owned release];
        QVERIFY(!QAccessible::accessibleInterface(realId));
        NSAutoreleasePool* teardownPool = [[NSAutoreleasePool alloc] init];
        tree.reset();
        [teardownPool drain];
        QVERIFY(!QAccessible::accessibleInterface(tableId));
        for (QAccessible::Id identifier : lastIds) {
            QVERIFY(!QAccessible::accessibleInterface(identifier));
        }

        IMP prior = method_setImplementation(remove, reinterpret_cast<IMP>(foreignRemove));
        const bool accepted = installQtCocoaAccessibilityOwnershipGuard(&reason);
        const bool foreignUnchanged = method_getImplementation(remove) == reinterpret_cast<IMP>(foreignRemove);
        method_setImplementation(remove, prior);
        QVERIFY(!accepted);
        QVERIFY(foreignUnchanged);
        QVERIFY(!reason.isEmpty());
    }
};

QTEST_MAIN(QtCocoaOwnershipTest)
#include "tst_qt_cocoa_accessibility_ownership.moc"
