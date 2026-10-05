// Regression: a synthetic Qt Cocoa table cell must not delete its borrowed
// parent ID, and incompatible/intercepted methods must never be overwritten.
// JJ Boyd (KG4VCF), 2026-10-03, with OpenAI Codex assistance.
#include "gui/QtCocoaAccessibilityOwnershipGuard.h"
#include "gui/ConnectionSelector.h"
#include "gui/QtCocoaAccessibilityOwnershipGuard_p.h"
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/meters/OtherButtonItem.h"
#include <QLineEdit>
#include <QSplitter>
#include <QTemporaryDir>
#include "gui/applets/RxApplet.h"
#include "gui/applets/RadeApplet.h"
#include "core/MicProfileManager.h"
#include "core/BoardCapabilities.h"
#include "core/StepAttenuatorFacade.h"
#include "models/RadioModel.h"
#include "gui/setup/DeviceCard.h"
#include "gui/setup/AudioTxInputPage.h"
#include "core/AudioDeviceConfig.h"
#include "core/audio/PortAudioBus.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QAccessible>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QPersistentModelIndex>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardItemModel>
#include <QSignalSpy>
#include <QSignalBlocker>
#include <QStandardPaths>
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

// Exercise the real Cocoa lifecycle while keeping the card and popup hidden.
// Qt's native row rebuild deletes the promoted cell, but leaves its ID in
// QAccessibleTable's separate child cache. A refresh must tolerate that state.
QAccessible::Id expireNativePopupCell(QComboBox* combo)
{
    QAbstractItemView* view = combo->view();
    view->resize(300, 200);
    view->doItemsLayout();
    view->setCurrentIndex(combo->model()->index(combo->currentIndex(), 0));
    QAccessibleInterface* table = QAccessible::queryAccessibleInterface(view);
    if (!table || !table->tableInterface() || !table->selectionInterface()) {
        return 0;
    }
    const QAccessible::Id tableId = QAccessible::uniqueId(table);
    const QList<QAccessibleInterface*> selected = table->selectionInterface()->selectedItems();
    if (selected.size() != 1) {
        return 0;
    }
    const QAccessible::Id cellId = QAccessible::uniqueId(selected.first());
    Class cls = NSClassFromString(@"QMacAccessibilityElement");
    id element = [cls elementWithId:tableId];
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [element updateTableModel];
    NSArray* rows = [element accessibilityRows];
    bool promoted = false;
    if (rows.count == NSUInteger(combo->count())) {
        NSArray* cells = [[rows objectAtIndex:combo->currentIndex()] accessibilityChildren];
        if (cells.count == 1) {
            QAccessibleInterface* cell = [[cells firstObject] qtInterface];
            promoted = cell && QAccessible::uniqueId(cell) == cellId;
        }
    }
    [element updateTableModel];
    [pool drain];
    return promoted && !QAccessible::accessibleInterface(cellId)
        && QAccessible::accessibleInterface(tableId) == table ? cellId : 0;
}
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

    void audioDeviceCardRefreshAfterNativePopupRebuild_data()
    {
        QTest::addColumn<bool>("input");
        QTest::addColumn<QString>("refresh");
        QTest::newRow("input-driver") << true << QStringLiteral("driver");
        QTest::newRow("output-driver") << false << QStringLiteral("driver");
        QTest::newRow("input-device-reload") << true << QStringLiteral("device");
        QTest::newRow("output-device-reload") << false << QStringLiteral("device");
        QTest::newRow("input-buffer-reload") << true << QStringLiteral("buffer");
        QTest::newRow("output-buffer-reload") << false << QStringLiteral("buffer");
    }

    void audioDeviceCardRefreshAfterNativePopupRebuild()
    {
        QFETCH(bool, input);
        QFETCH(QString, refresh);
        QString reason;
        QVERIFY2(installQtCocoaAccessibilityOwnershipGuard(&reason), qPrintable(reason));
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(NereusSDR::PortAudioBus::portAudioBarredForTestRun());
        QAccessible::setActive(true);
        const QString prefix = input ? QStringLiteral("audio/TxInput")
                                     : QStringLiteral("audio/Speakers");
        NereusSDR::AudioDeviceConfig config;
        config.deviceName = QStringLiteral("Absent test audio device");
        config.bufferSamples = 3000;
        config.saveToSettings(prefix);
        NereusSDR::DeviceCard card(prefix, input ? NereusSDR::DeviceCard::Role::Input
                                               : NereusSDR::DeviceCard::Role::Output, false);
        const QList<QComboBox*> combos = card.findChildren<QComboBox*>();
        QComboBox* driver = combos.first();
        QComboBox* device = nullptr;
        QComboBox* buffer = nullptr;
        for (QComboBox* combo : combos) {
            if (combo->findText(QStringLiteral("(platform default)")) >= 0) {
                device = combo;
            }
            if (combo->findText(QStringLiteral("256 samples")) >= 0) {
                buffer = combo;
            }
        }
        QVERIFY(device && buffer);
        {
            QSignalBlocker blocker(driver);
            driver->addItem(QStringLiteral("Test audio API"), 0);
        }
        QComboBox* refreshed = refresh == QStringLiteral("buffer") ? buffer : device;
        QSignalSpy changes(&card, &NereusSDR::DeviceCard::configChanged);
        QSignalSpy modelResets(refreshed->model(), &QAbstractItemModel::modelReset);
        QVERIFY(!card.isVisible());
        QVERIFY(!refreshed->view()->isVisible());
        for (int iteration = 0; iteration < 3; ++iteration) {
            const QAccessible::Id expired = expireNativePopupCell(refreshed);
            QVERIFY2(expired != 0, "Actual Cocoa row rebuild must expire the queried popup cell");
            qInfo() << "Expired audio card popup cell before" << refresh << "refresh:" << expired;
            if (refresh == QStringLiteral("driver")) {
                driver->setCurrentIndex(driver->currentIndex() == 0 ? 1 : 0);
                QCOMPARE(changes.count(), iteration + 1);
                const auto saved = NereusSDR::AudioDeviceConfig::loadFromSettings(prefix);
                QCOMPARE(saved.deviceName, config.deviceName);
                QCOMPARE(saved.bufferSamples, 3000);
            } else {
                if (refresh == QStringLiteral("device")) {
                    config.deviceName = QStringLiteral("Absent test audio device %1").arg(iteration);
                } else {
                    config.bufferSamples = iteration % 2 == 0 ? 5000 : 3000;
                }
                config.saveToSettings(prefix);
                card.loadFromSettings();
                QCOMPARE(changes.count(), 0);
            }
            QCOMPARE(card.currentConfig().deviceName, config.deviceName);
            QCOMPARE(card.currentConfig().bufferSamples, config.bufferSamples);
            QCOMPARE(device->currentText(), config.deviceName + QStringLiteral(" (not available)"));
            QCOMPARE(device->count(), 2);
            // The accepted DeviceCard model reset runs twice for driver
            // rebuild + retained device selection, once for a retained-entry reload.
            const int resetsPerRefresh = refresh == QStringLiteral("driver") ? 2 : 1;
            QCOMPARE(modelResets.count(), (iteration + 1) * resetsPerRefresh);
            QAccessibleInterface* table = QAccessible::queryAccessibleInterface(refreshed->view());
            QVERIFY(table && table->tableInterface());
            QAccessibleInterface* selected = table->tableInterface()->cellAt(refreshed->currentIndex(), 0);
            QVERIFY(selected && selected->isValid());
            QCOMPARE(selected->text(QAccessible::Name), refreshed->currentText());
        }
    }

    void radeProfileRefreshAfterNativePopupRebuild_data()
    {
        QTest::addColumn<bool>("permitted");
        QTest::newRow("profile-permitted") << true;
        QTest::newRow("profile-denied") << false;
    }

    void radeProfileRefreshAfterNativePopupRebuild()
    {
        QFETCH(bool, permitted);
        QString reason;
        QVERIFY2(installQtCocoaAccessibilityOwnershipGuard(&reason), qPrintable(reason));
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(NereusSDR::PortAudioBus::portAudioBarredForTestRun());
        QAccessible::setActive(true);
        NereusSDR::RadioModel remote{NereusSDR::RadioModel::Role::Remote};
        QVERIFY(!remote.ownsLocalDsp());
        QVERIFY(remote.stationLink() == nullptr);
        auto& tx = remote.transmitModel();
        QVERIFY(tx.applyStationValue("txProfilesJson", QStringLiteral("[\"Default\",\"RADE\",\"Secondary\"]")));
        QVERIFY(tx.applyStationValue("activeTxProfile", QStringLiteral("Default")));
        NereusSDR::RadeApplet applet(&remote);
        const QString deniedReason = QStringLiteral("Profile changes are unavailable during this test");
        applet.setTxProfilePermitted(permitted, deniedReason);
        QComboBox* const combo = applet.profileComboForTest();
        QPushButton* const reset = applet.resetVocoderButtonForTest();
        auto* const model = qobject_cast<QStandardItemModel*>(combo->model());
        QVERIFY(model && reset);
        QObject* const modelParent = model->parent();
        QWidget* const comboParent = combo->parentWidget();
        QCOMPARE(combo->currentText(), QStringLiteral("RADE"));
        QCOMPARE(combo->isEnabled(), permitted);
        QCOMPARE(reset->isEnabled(), permitted);
        const QString tooltip = combo->toolTip();
        const QString description = combo->accessibleDescription();
        const QString resetTooltip = reset->toolTip();
        const QString resetDescription = reset->accessibleDescription();
        if (!permitted) {
            QCOMPARE(tooltip, deniedReason);
            QCOMPARE(description, deniedReason);
            QCOMPARE(resetTooltip, deniedReason);
            QCOMPARE(resetDescription, deniedReason);
        }
        QSignalSpy resets(model, &QAbstractItemModel::modelReset);
        QSignalSpy activated(combo, &QComboBox::textActivated);
        QSignalSpy indexChanges(combo, &QComboBox::currentIndexChanged);
        QSignalSpy activeChanges(remote.micProfileManager(), &NereusSDR::MicProfileManager::activeProfileChanged);
        QSignalSpy lists(remote.micProfileManager(), &NereusSDR::MicProfileManager::profileListChanged);
        QSignalSpy catalogs(&tx, &NereusSDR::TransmitModel::txProfilesJsonChanged);
        QSignalSpy rejected(&remote, &NereusSDR::RadioModel::sliceAddRejected);
        QSignalSpy commands(&remote, &NereusSDR::RadioModel::stationCommandFinished);
        // A refresh must tolerate expired native cells without activating a
        // profile. Exercise the real remote catalog -> manager -> applet path.
        const QList<QStringList> catalogsToApply = {
            {QStringLiteral("Secondary A"), QStringLiteral("Default")},
            {QStringLiteral("Secondary B"), QStringLiteral("RADE"), QStringLiteral("Default")},
            {QStringLiteral("Default"), QStringLiteral("Secondary C")}
        };
        const QStringList selections = {QStringLiteral("Default"), QStringLiteral("RADE"), QStringLiteral("Default")};
        QVERIFY(!applet.isVisible());
        QVERIFY(!combo->view()->isVisible());
        for (int iteration = 0; iteration < 3; ++iteration) {
            const QPersistentModelIndex oldIndex(model->index(combo->currentIndex(), 0));
            QVERIFY(oldIndex.isValid());
            QAccessibleInterface* const table = QAccessible::queryAccessibleInterface(combo->view());
            QVERIFY(table && table->tableInterface());
            const QAccessible::Id tableId = QAccessible::uniqueId(table);
            const QAccessible::Id expired = expireNativePopupCell(combo);
            QVERIFY2(expired != 0, "Actual Cocoa row rebuild must expire the queried RADE popup cell");
            QVERIFY(!QAccessible::accessibleInterface(expired));
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
            qInfo() << "Expired RADE popup cell before mirrored catalog refresh:" << expired << permitted << iteration;

            const QStringList& names = catalogsToApply.at(iteration);
            const QString json = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(names)).toJson(QJsonDocument::Compact));
            QVERIFY(tx.applyStationValue("txProfilesJson", json));

            QCOMPARE(catalogs.count(), iteration + 1);
            QCOMPARE(lists.count(), iteration + 1);
            QCOMPARE(combo->model(), model);
            QCOMPARE(model->parent(), modelParent);
            QCOMPARE(combo->parentWidget(), comboParent);
            QCOMPARE(model->columnCount(), 1);
            QCOMPARE(combo->count(), names.size());
            for (int row = 0; row < names.size(); ++row) {
                QCOMPARE(combo->itemText(row), names.at(row));
            }
            QCOMPARE(combo->currentText(), selections.at(iteration));
            QCOMPARE(remote.micProfileManager()->profileNames(), names);
            QCOMPARE(remote.micProfileManager()->activeProfileName(), QStringLiteral("Default"));
            QCOMPARE(tx.activeTxProfile(), QStringLiteral("Default"));
            QCOMPARE(resets.count(), iteration + 1);
            QVERIFY(!oldIndex.isValid());
            QCOMPARE(activated.count(), 0);
            QCOMPARE(indexChanges.count(), 0);
            QCOMPARE(activeChanges.count(), 0);
            QCOMPARE(rejected.count(), 0);
            QCOMPARE(commands.count(), 0);
            QVERIFY(!tx.isMox());
            QVERIFY(!tx.isTune());
            QCOMPARE(combo->isEnabled(), permitted);
            QCOMPARE(reset->isEnabled(), permitted);
            QCOMPARE(combo->toolTip(), tooltip);
            QCOMPARE(combo->accessibleDescription(), description);
            QCOMPARE(reset->toolTip(), resetTooltip);
            QCOMPARE(reset->accessibleDescription(), resetDescription);
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
            QAccessibleInterface* const selected = table->tableInterface()->cellAt(combo->currentIndex(), 0);
            QVERIFY(selected && selected->isValid());
            QCOMPARE(selected->text(QAccessible::Name), selections.at(iteration));
            qInfo() << "Verified RADE mirrored catalog refresh:" << permitted << iteration;
        }
    }

    void audioTxInputRefreshAfterNativePopupRebuild()
    {
        QString reason;
        QVERIFY2(installQtCocoaAccessibilityOwnershipGuard(&reason), qPrintable(reason));
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(NereusSDR::PortAudioBus::portAudioBarredForTestRun());
        QAccessible::setActive(true);
        NereusSDR::AudioTxInputPage page(nullptr);
        const QList<QComboBox*> combos = page.findChildren<QComboBox*>();
        QVERIFY(combos.size() >= 2);
        QComboBox* backend = combos.at(0);
        QComboBox* device = combos.at(1);
        QCOMPARE(backend->currentData().toInt(), -1);
        QCOMPARE(device->currentText(), QStringLiteral("(default)"));
        {
            QSignalBlocker blocker(backend);
            backend->addItem(QStringLiteral("Test capture API"), 0);
        }
        QSignalSpy deviceChanges(device, &QComboBox::currentIndexChanged);
        QVERIFY(!page.isVisible());
        QVERIFY(!device->view()->isVisible());
        for (int iteration = 0; iteration < 3; ++iteration) {
            const QAccessible::Id expired = expireNativePopupCell(device);
            QVERIFY2(expired != 0, "Actual Cocoa row rebuild must expire the queried TX input cell");
            qInfo() << "Expired TX input popup cell before backend refresh:" << expired;
            backend->setCurrentIndex(backend->currentIndex() == 0 ? 1 : 0);
            QCOMPARE(deviceChanges.count(), 0);
            QCOMPARE(device->count(), 1);
            QCOMPARE(device->currentText(), iteration % 2 == 0
                ? QStringLiteral("(no input devices)") : QStringLiteral("(default)"));
            QAccessibleInterface* table = QAccessible::queryAccessibleInterface(device->view());
            QVERIFY(table && table->tableInterface());
            QAccessibleInterface* selected = table->tableInterface()->cellAt(0, 0);
            QVERIFY(selected && selected->isValid());
            QCOMPARE(selected->text(QAccessible::Name), device->currentText());
        }
    }

    void containerDropdownSelectionAfterNativeTableRebuild()
    {
        QString reason;
        QVERIFY2(installQtCocoaAccessibilityOwnershipGuard(&reason), qPrintable(reason));
        QAccessible::setActive(true);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        NereusSDR::AppSettings settings(dir.filePath(QStringLiteral("settings")));
        NereusSDR::ContainerWorkspaceStore store(settings);
        NereusSDR::ContainerContentRegistry registry;
        QWidget root;
        QSplitter splitter(&root);
        NereusSDR::ContainerManager manager(&root, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        NereusSDR::WorkspaceDocument document;
        document.mainContainerId = QStringLiteral("A");
        NereusSDR::ContainerDocument first;
        first.id = QStringLiteral("A");
        first.name = QStringLiteral("First");
        first.layout = NereusSDR::ContentLayout::VerticalStack;
        first.contents = {registry.makeEntry(QStringLiteral("control.mox"))};
        NereusSDR::ContainerDocument second;
        second.id = QStringLiteral("B");
        second.name = QStringLiteral("Second");
        second.layout = NereusSDR::ContentLayout::VerticalStack;
        second.contents = {registry.makeEntry(QStringLiteral("meter.clock"))};
        document.containers = {first, second};
        QCOMPARE(manager.commitWorkspace(document, 0).status, NereusSDR::CommitStatus::Saved);
        const NereusSDR::WorkspaceDocument original = store.snapshot();
        const QVariant saved = settings.value(QStringLiteral("ContainerWorkspace"));
        auto* control = qobject_cast<NereusSDR::OtherButtonItem*>(
            manager.contentHost(first.id)->entryRows().first().item.data());
        QVERIFY(control);
        QSignalSpy commands(control, &NereusSDR::OtherButtonItem::otherButtonClicked);
        QSignalSpy commits(&store, &NereusSDR::ContainerWorkspaceStore::committed);
        QSignalSpy reconciles(&manager, &NereusSDR::ContainerManager::workspaceReconciled);
        NereusSDR::ContainerSettingsDialog dialog(manager.container(first.id), nullptr, &manager);
        auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("containerDraftSelection"));
        auto* title = dialog.findChild<QLineEdit*>(QStringLiteral("containerDraftTitle"));
        QVERIFY(combo && title && dialog.editSession());
        QCOMPARE(combo->currentData().toString(), first.id);
        title->setText(QStringLiteral("Draft first"));
        QSignalSpy comboChanges(combo, &QComboBox::currentIndexChanged);
        QSignalSpy modelResets(combo->model(), &QAbstractItemModel::modelReset);
        QAbstractItemView* view = combo->view();
        view->resize(300, 200);
        view->doItemsLayout();
        QAccessibleInterface* table = QAccessible::queryAccessibleInterface(view);
        QVERIFY(table && table->tableInterface() && table->selectionInterface());
        const QAccessible::Id tableId = QAccessible::uniqueId(table);
        Class cls = NSClassFromString(@"QMacAccessibilityElement");
        id element = [cls elementWithId:tableId];
        QVERIFY(element);
        QVERIFY(!dialog.isVisible());
        QVERIFY(!view->isVisible());

        int switches = 0;
        for (const QString& selectedId : {second.id, first.id, second.id}) {
            view->setCurrentIndex(combo->model()->index(combo->currentIndex(), 0));
            const QList<QAccessibleInterface*> selected = table->selectionInterface()->selectedItems();
            QCOMPARE(selected.size(), 1);
            const QAccessible::Id oldId = QAccessible::uniqueId(selected.first());
            NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
            [element updateTableModel];
            NSArray* rows = [element accessibilityRows];
            QCOMPARE(rows.count, NSUInteger(2));
            NSArray* native = [[rows objectAtIndex:combo->currentIndex()] accessibilityChildren];
            QCOMPARE(native.count, NSUInteger(1));
            QAccessibleInterface* promoted = [[native firstObject] qtInterface];
            QVERIFY(promoted);
            QCOMPARE(QAccessible::uniqueId(promoted), oldId);
            [element updateTableModel];
            [pool drain];
            // Expire an actual Cocoa popup cell, retaining the independent
            // QAccessibleTable cache that RowsRemoved visits during clear.
            QVERIFY(!QAccessible::accessibleInterface(oldId));
            QCOMPARE(QAccessible::accessibleInterface(tableId), table);
            qInfo() << "Expired container dropdown cell ID before selection:" << oldId;

            // Exercise onContainerDropdownChanged through its real signal;
            // selecting directly through the dialog would miss this path.
            combo->setCurrentIndex(combo->findData(selectedId));

            ++switches;
            QCOMPARE(comboChanges.count(), switches);
            QCOMPARE(combo->currentData().toString(), selectedId);
            QCOMPARE(combo->count(), 2);
            QVERIFY(combo->isEnabled());
            QCOMPARE(combo->itemText(combo->findData(first.id)), QStringLiteral("Draft first"));
            QCOMPARE(combo->itemText(combo->findData(second.id)), QStringLiteral("Second"));
            QCOMPARE(title->text(), selectedId == first.id ? QStringLiteral("Draft first") : QStringLiteral("Second"));
            NereusSDR::WorkspaceDocument expectedDraft = original;
            expectedDraft.containers[0].name = QStringLiteral("Draft first");
            QCOMPARE(dialog.editSession()->draft(), expectedDraft);
            QVERIFY(dialog.editSession()->hasPendingChanges());
            QCOMPARE(store.snapshot(), original);
            QCOMPARE(settings.value(QStringLiteral("ContainerWorkspace")), saved);
            QCOMPARE(commits.count(), 0);
            QCOMPARE(reconciles.count(), 0);
            QCOMPARE(commands.count(), 0);
            QCOMPARE(modelResets.count(), 0);
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
        dialog.reject();
        QCOMPARE(store.snapshot(), original);
        QCOMPARE(settings.value(QStringLiteral("ContainerWorkspace")), saved);
        QCOMPARE(commits.count(), 0);
        QCOMPARE(reconciles.count(), 0);
        QCOMPARE(commands.count(), 0);
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
