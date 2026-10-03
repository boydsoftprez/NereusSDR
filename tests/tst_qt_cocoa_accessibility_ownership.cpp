// Regression: a synthetic Qt Cocoa table cell must not delete its borrowed
// parent ID, and incompatible/intercepted methods must never be overwritten.
// JJ Boyd (KG4VCF), 2026-10-03, with OpenAI Codex assistance.
#include "gui/QtCocoaAccessibilityOwnershipGuard.h"
#include "gui/QtCocoaAccessibilityOwnershipGuard_p.h"
#include <QApplication>
#include <QAccessible>
#include <QPushButton>
#include <QTreeWidget>
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
