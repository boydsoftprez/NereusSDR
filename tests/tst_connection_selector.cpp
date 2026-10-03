// =================================================================
// tests/tst_connection_selector.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R3 Core session presentation and actions.
// =================================================================

#include "gui/ConnectionSelector.h"
#include "gui/CoreTargetEditor.h"
#include "gui/styles/AppTheme.h"

#include <QAccessible>
#include <QCheckBox>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QStyleFactory>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QTest>
#include <QTreeWidget>

#include <algorithm>
#include <functional>

using namespace NereusSDR;

namespace {

ConnectionTargetRow savedRow(const QString& key = QStringLiteral("saved-core"))
{
    return {key, ConnectionTargetKind::SavedCore, QStringLiteral("Shack"),
            QStringLiteral("Saturn G2"), QStringLiteral("shack.example"),
            QStringLiteral("Disconnected"), true, true, true};
}

SavedCoreTarget initialTarget()
{
    SavedCoreTarget target;
    target.id = QStringLiteral("saved-core");
    target.label = QStringLiteral("Shack");
    target.connection.url = QStringLiteral("wss://shack.example:8443");
    target.connection.token = QStringLiteral("token");
    target.connection.fingerprint = QStringLiteral("fingerprint");
    target.lastRadioName = QStringLiteral("Saturn G2");
    target.lastRadioMac = QStringLiteral("00:11:22:33:44:55");
    return target;
}

} // namespace

class ConnectionSelectorTest final : public QObject {
    Q_OBJECT

private slots:
    void selectionRefreshIsStableAndDoesNotConnect();
    void refreshPreservesSelectedRowModelIdentity();
    void explicitActionsUseTheSelectedKey();
    void disconnectRemainsIndependentOfSelectedTarget();
    void editorValidatesAndPreservesSecrets();
    void editorCancelHasNoAcceptance();
    void editorForgetsLastAddressesWhenTheCoreChanges();
    void editorOffersReachingTheCoreFromAnywhere();
    void controlsRemainReadableAndReachable();
    void rowSelectionNeverResizesTheWindow();
    void theCodeDialogNamesPlacesThatShowTheCode();
    void codeAloneIsTheNormalPairingDialogPath();
    void serviceOnlyEditorKeepsIdentityAndRouteWithoutAnAddress();
};

void ConnectionSelectorTest::selectionRefreshIsStableAndDoesNotConnect()
{
    ConnectionSelector selector;
    QSignalSpy connectSpy(&selector, &ConnectionSelector::connectRequested);
    QList<ConnectionTargetRow> targets{
        {QStringLiteral("local"), ConnectionTargetKind::LocalRadio, QStringLiteral("ANAN"),
         QStringLiteral("Local DSP"), QStringLiteral("192.168.1.10"),
         QStringLiteral("Available")},
        savedRow(),
    };
    selector.setTargets(targets);
    auto* tree = selector.findChild<QTreeWidget*>(QStringLiteral("connectionSelectorTargets"));
    QVERIFY(tree != nullptr);
    QTreeWidgetItem* savedRowItem = tree->topLevelItem(2)->child(0);
    QVERIFY(savedRowItem != nullptr);
    tree->setCurrentItem(savedRowItem);
    QCOMPARE(selector.selectedKey(), QStringLiteral("saved-core"));

    std::reverse(targets.begin(), targets.end());
    selector.setTargets(targets);
    QCOMPARE(selector.selectedKey(), QStringLiteral("saved-core"));
    QCOMPARE(connectSpy.count(), 0);
}

void ConnectionSelectorTest::refreshPreservesSelectedRowModelIdentity()
{
    ConnectionSelector selector;
    ConnectionTargetRow first = savedRow(QStringLiteral("saved-first"));
    first.name = QStringLiteral("First");
    ConnectionTargetRow selected = savedRow(QStringLiteral("saved-selected"));
    selected.name = QStringLiteral("Selected");
    selector.setTargets({first, selected});

    auto* tree = selector.findChild<QTreeWidget*>(QStringLiteral("connectionSelectorTargets"));
    QVERIFY(tree != nullptr);
    selector.setSelectedKey(selected.key);
    QTreeWidgetItem* selectedItem = tree->currentItem();
    QVERIFY(selectedItem != nullptr);
    selector.show();
    QCoreApplication::processEvents();
    const QPersistentModelIndex selectedIndex = tree->indexFromItem(selectedItem, 0);
    QVERIFY(selectedIndex.isValid());
    QAccessibleInterface* treeInterface = QAccessible::queryAccessibleInterface(tree);
    QVERIFY(treeInterface != nullptr);
    QAccessibleSelectionInterface* accessibleSelection = treeInterface->selectionInterface();
    QVERIFY(accessibleSelection != nullptr);
    const auto currentSelectedAccessibleId = [accessibleSelection] {
        const QList<QAccessibleInterface*> selected = accessibleSelection->selectedItems();
        return selected.isEmpty() || selected.first() == nullptr
            ? QAccessible::Id{} : QAccessible::uniqueId(selected.first());
    };
    const QList<QAccessibleInterface*> selectedInterfaces = accessibleSelection->selectedItems();
    QVERIFY(!selectedInterfaces.isEmpty());
    QVERIFY(selectedInterfaces.first() != nullptr);
    const QAccessible::Id selectedAccessibleId = QAccessible::uniqueId(selectedInterfaces.first());
    QSignalSpy resetSpy(tree->model(), &QAbstractItemModel::modelReset);

    selected.state = QStringLiteral("Connected");
    selector.setTargets({first, selected});

    QCOMPARE(resetSpy.count(), 0);
    QVERIFY(selectedIndex.isValid());
    QCOMPARE(tree->itemFromIndex(selectedIndex), selectedItem);
    QCOMPARE(tree->currentItem(), selectedItem);
    QCOMPARE(selectedItem->text(3), QStringLiteral("Connected"));
    QCOMPARE(currentSelectedAccessibleId(), selectedAccessibleId);

    selector.setTargets({selected});

    QCOMPARE(resetSpy.count(), 0);
    QVERIFY(selectedIndex.isValid());
    QCOMPARE(tree->itemFromIndex(selectedIndex), selectedItem);
    QCOMPARE(tree->currentItem(), selectedItem);
    QCOMPARE(selectedItem->text(3), QStringLiteral("Connected"));
    QCOMPARE(selector.selectedKey(), selected.key);
    const QList<QAccessibleInterface*> shiftedInterfaces = accessibleSelection->selectedItems();
    QVERIFY(!shiftedInterfaces.isEmpty());
    for (QAccessibleInterface* interface : shiftedInterfaces) {
        QVERIFY(interface != nullptr);
        QVERIFY(interface->isValid());
    }

    selector.setTargets({first});

    QCOMPARE(resetSpy.count(), 0);
    QVERIFY(!selectedIndex.isValid());
    QVERIFY(accessibleSelection->selectedItems().isEmpty());
    QVERIFY(tree->currentItem() == nullptr);
    QVERIFY(selector.selectedKey().isEmpty());
    auto* details = selector.findChild<QPushButton*>(QStringLiteral("connectionSelectorDetails"));
    QVERIFY(details != nullptr);
    QVERIFY(!details->isEnabled());
}

void ConnectionSelectorTest::explicitActionsUseTheSelectedKey()
{
    ConnectionSelector selector;
    selector.setTargets({savedRow()});
    selector.setSelectedKey(QStringLiteral("saved-core"));

    QSignalSpy connectSpy(&selector, &ConnectionSelector::connectRequested);
    QSignalSpy editSpy(&selector, &ConnectionSelector::editRequested);
    QSignalSpy forgetSpy(&selector, &ConnectionSelector::forgetRequested);
    QSignalSpy detailsSpy(&selector, &ConnectionSelector::detailsRequested);
    for (const char* buttonName : {"connectionSelectorConnect", "connectionSelectorEdit",
                                   "connectionSelectorForget", "connectionSelectorDetails"}) {
        auto* button = selector.findChild<QPushButton*>(QString::fromLatin1(buttonName));
        QVERIFY(button != nullptr);
        QVERIFY(button->isEnabled());
        button->click();
    }
    QCOMPARE(connectSpy.takeFirst().at(0).toString(), QStringLiteral("saved-core"));
    QCOMPARE(editSpy.takeFirst().at(0).toString(), QStringLiteral("saved-core"));
    QCOMPARE(forgetSpy.takeFirst().at(0).toString(), QStringLiteral("saved-core"));
    QCOMPARE(detailsSpy.takeFirst().at(0).toString(), QStringLiteral("saved-core"));
}

void ConnectionSelectorTest::disconnectRemainsIndependentOfSelectedTarget()
{
    ConnectionSelector selector;
    ConnectionTargetRow unavailable = savedRow();
    unavailable.connectable = false;
    selector.setTargets({unavailable});
    selector.setSelectedKey(unavailable.key);
    auto* connectButton = selector.findChild<QPushButton*>(QStringLiteral("connectionSelectorConnect"));
    auto* disconnectButton = selector.findChild<QPushButton*>(QStringLiteral("connectionSelectorDisconnect"));
    QVERIFY(connectButton != nullptr);
    QVERIFY(disconnectButton != nullptr);
    QVERIFY(!connectButton->isEnabled());

    selector.setCurrentConnection(QStringLiteral("Retrying Shack"), QStringLiteral("Waiting"),
                                  false, true);
    QCOMPARE(disconnectButton->text(), QStringLiteral("Cancel retry"));
    QVERIFY(disconnectButton->isEnabled());
    QSignalSpy disconnectSpy(&selector, &ConnectionSelector::disconnectRequested);
    disconnectButton->click();
    QCOMPARE(disconnectSpy.count(), 1);
}

void ConnectionSelectorTest::editorValidatesAndPreservesSecrets()
{
    CoreTargetEditor editor(initialTarget());
    auto* address = editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorAddress"));
    auto* token = editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorToken"));
    auto* fingerprint = editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorFingerprint"));
    auto* error = editor.findChild<QLabel*>(QStringLiteral("coreTargetEditorError"));
    auto* save = editor.findChild<QPushButton*>(QStringLiteral("coreTargetEditorSave"));
    QVERIFY(address != nullptr);
    QVERIFY(token != nullptr);
    QVERIFY(fingerprint != nullptr);
    QVERIFY(error != nullptr);
    QVERIFY(save != nullptr);
    QCOMPARE(token->echoMode(), QLineEdit::Password);

    address->setText(QStringLiteral("https://not-a-station"));
    save->click();
    QCOMPARE(editor.result(), int(QDialog::Rejected));
    QCOMPARE(error->text(), QStringLiteral("Enter a valid Core address beginning with ws:// or wss://."));

    address->setText(QStringLiteral(" wss://other.example:8443 "));
    token->setText(QStringLiteral(" token with spaces "));
    fingerprint->setText(QStringLiteral(" fingerprint with spaces "));
    save->click();
    QCOMPARE(editor.result(), int(QDialog::Accepted));
    const SavedCoreTarget saved = editor.target();
    QCOMPARE(saved.id, QStringLiteral("saved-core"));
    QCOMPARE(saved.connection.url, QStringLiteral("wss://other.example:8443"));
    QCOMPARE(saved.connection.token, QStringLiteral(" token with spaces "));
    QCOMPARE(saved.connection.fingerprint, QStringLiteral(" fingerprint with spaces "));
    QVERIFY(saved.lastRadioName.isEmpty());
    QVERIFY(saved.lastRadioMac.isEmpty());
}

void ConnectionSelectorTest::editorCancelHasNoAcceptance()
{
    CoreTargetEditor editor(initialTarget());
    auto* cancel = editor.findChild<QPushButton*>(QStringLiteral("coreTargetEditorCancel"));
    QVERIFY(cancel != nullptr);
    cancel->click();
    QCOMPARE(editor.result(), int(QDialog::Rejected));
}

// iPhone app plan Task 27 fix wave (I1): the Core's last good addresses
// belong to the address, token, pin and identity they were reached with.
// Changing any of those forgets them, so the next connect does not dial an
// address the operator never approved for the new details.
void ConnectionSelectorTest::editorForgetsLastAddressesWhenTheCoreChanges()
{
    const QStringList cached{QStringLiteral("wss://192.168.1.20:8443"),
                             QStringLiteral("wss://10.0.0.7:8443")};
    SavedCoreTarget initial = initialTarget();
    initial.connection.cachedAddresses = cached;

    {
        // Nothing but the label changed: the addresses stay.
        CoreTargetEditor editor(initial);
        editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorLabel"))
            ->setText(QStringLiteral("Shack upstairs"));
        QCOMPARE(editor.target().connection.cachedAddresses, cached);
    }
    const auto forgets = [&](const std::function<void(CoreTargetEditor&)>& change) {
        CoreTargetEditor editor(initial);
        change(editor);
        return editor.target().connection.cachedAddresses.isEmpty();
    };
    QVERIFY(forgets([](CoreTargetEditor& editor) {
        editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorAddress"))
            ->setText(QStringLiteral("wss://other.example:8443"));
    }));
    QVERIFY(forgets([](CoreTargetEditor& editor) {
        editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorToken"))
            ->setText(QStringLiteral("another token"));
    }));
    QVERIFY(forgets([](CoreTargetEditor& editor) {
        editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorFingerprint"))
            ->setText(QStringLiteral("another fingerprint"));
    }));
    QVERIFY(forgets([](CoreTargetEditor& editor) {
        editor.findChild<QCheckBox*>(QStringLiteral("coreTargetEditorAllowUnpinned"))
            ->setChecked(true);
    }));
    // Surrounding spaces on the address are not a change.
    {
        CoreTargetEditor editor(initial);
        editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorAddress"))
            ->setText(QStringLiteral(" wss://shack.example:8443 "));
        QCOMPARE(editor.target().connection.cachedAddresses, cached);
    }
}

// iPhone app plan Task 29 (R-IOS-16): reaching the Core through the
// internet service beside its addresses is on for a paired Core and can be
// turned off; for a Core it cannot reach that way the choice is shown,
// disabled, with the reason beside it.
void ConnectionSelectorTest::editorOffersReachingTheCoreFromAnywhere()
{
    {
        CoreTargetEditor editor(initialTarget());
        auto* check = editor.findChild<QCheckBox*>(QStringLiteral("coreTargetEditorReachAnywhere"));
        auto* reason =
            editor.findChild<QLabel*>(QStringLiteral("coreTargetEditorReachAnywhereReason"));
        QVERIFY(check != nullptr && reason != nullptr);
        QVERIFY(!check->isEnabled());
        QCOMPARE(reason->text(), QStringLiteral("Pair with the Core to reach it from anywhere."));
        QVERIFY(!reason->isHidden());
    }
    SavedCoreTarget paired = initialTarget();
    paired.connection.identityFingerprint = QByteArray(32, '\x07');
    {
        // Task 29 fix wave (Minor 4): paired, but its service name is not
        // known until a sign-in: shown disabled with the reason.
        CoreTargetEditor editor(paired);
        auto* check = editor.findChild<QCheckBox*>(QStringLiteral("coreTargetEditorReachAnywhere"));
        auto* reason =
            editor.findChild<QLabel*>(QStringLiteral("coreTargetEditorReachAnywhereReason"));
        QVERIFY(!check->isEnabled());
        QCOMPARE(reason->text(), QStringLiteral("Connect to the Core once to reach it from "
                                                "anywhere."));
        QVERIFY(!reason->isHidden());
    }
    paired.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    paired.connection.relayAllowed = 1;
    {
        CoreTargetEditor editor(paired);
        auto* check = editor.findChild<QCheckBox*>(QStringLiteral("coreTargetEditorReachAnywhere"));
        auto* reason =
            editor.findChild<QLabel*>(QStringLiteral("coreTargetEditorReachAnywhereReason"));
        QVERIFY(check->isEnabled());
        QVERIFY(check->isChecked());
        QVERIFY(reason->isHidden());
        check->setChecked(false);
        const SavedCoreTarget edited = editor.target();
        QVERIFY(!edited.connection.reachFromAnywhere);
        // What the Core said of the service is kept across an address edit.
        QCOMPARE(edited.connection.rendezvousId, paired.connection.rendezvousId);
        QCOMPARE(edited.connection.relayAllowed, 1);
    }
    paired.connection.controlChannelVersion = 0;
    paired.connection.negativeControlObservedMs = QDateTime::currentMSecsSinceEpoch();
    {
        CoreTargetEditor editor(paired);
        auto* check = editor.findChild<QCheckBox*>(QStringLiteral("coreTargetEditorReachAnywhere"));
        auto* reason =
            editor.findChild<QLabel*>(QStringLiteral("coreTargetEditorReachAnywhereReason"));
        QVERIFY(!check->isEnabled());
        QCOMPARE(reason->text(), QStringLiteral("Update the Core to reach it from anywhere."));
        RemoteStationOptions current = paired.connection;
        editor.setCurrentOptionsSource([&current] { return current; });
        current.negativeControlObservedMs = -1; // changed network generation
        editor.refreshServiceAvailability();
        QVERIFY(check->isEnabled());
        QVERIFY(reason->isHidden());
    }
}

void ConnectionSelectorTest::controlsRemainReadableAndReachable()
{
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    applyDarkPalette(*qApp);
    applyAppBaselineQss(*qApp);
    ConnectionSelector selector;
    selector.setTargets({
        {QStringLiteral("local"), ConnectionTargetKind::LocalRadio, QStringLiteral("This computer's Core"),
         QStringLiteral("Choose a local radio"), QStringLiteral("This computer"), QStringLiteral("Available")},
        {QStringLiteral("g2e"), ConnectionTargetKind::LocalRadio, QStringLiteral("ANAN G2E"),
         QStringLiteral("This computer's Core"), QStringLiteral("192.168.109.108"), QStringLiteral("Available"), true, true, true},
        {QStringLiteral("lan"), ConnectionTargetKind::LanCore, QStringLiteral("Rock 5C (advertised)"),
         QStringLiteral("Saturn (advertised online)"), QStringLiteral("192.168.109.106:4433"), QStringLiteral("Saved, ready to connect")},
        savedRow()
    });
    selector.setDiscoveryStatus(QStringLiteral("LAN discovery is active."));
    selector.setSelectedKey(QStringLiteral("saved-core"));
    selector.setCurrentConnection(QStringLiteral("Core connected"),
        QStringLiteral("Core: 192.168.109.106:4433\nRadio: Saturn G2"), true, false);
    selector.show();
    QCoreApplication::processEvents();
    QList<QRect> rectangles;
    for (QPushButton* button : selector.findChildren<QPushButton*>()) {
        if (!button->isVisible()) { continue; }
        const QRect bounds(button->mapTo(&selector, QPoint()), button->size());
        QVERIFY2(selector.rect().contains(bounds), qPrintable(button->text()));
        QVERIFY2(button->width() >= button->sizeHint().width(), qPrintable(button->text()));
        for (const QRect& previous : rectangles) { QVERIFY(!previous.intersects(bounds)); }
        rectangles.append(bounds);
    }
    // iPhone app Task 18: Add a Core by code joins the row.
    QCOMPARE(rectangles.size(), 10);
    const QString captures = qEnvironmentVariable("NEREUS_SELECTOR_CAPTURE_DIR");
    if (!captures.isEmpty()) {
        QVERIFY(QDir().mkpath(captures));
        QVERIFY(selector.grab().save(captures + QStringLiteral("/connections.png")));
        selector.setCurrentConnection(QStringLiteral("Retrying Core (attempt 3)"),
            QStringLiteral("Core: 192.168.109.106:4433\nRadio state unavailable\nRetry delay: 4 s. Disconnect cancels automatic retries."), true, true);
        selector.setNotice(QStringLiteral("The selected Core could not be reached. Saved and manual addresses remain available."));
        QCoreApplication::processEvents();
        QVERIFY(selector.grab().save(captures + QStringLiteral("/connections-retry.png")));
        CoreTargetEditor editor(initialTarget());
        editor.show();
        QCoreApplication::processEvents();
        QVERIFY(editor.grab().save(captures + QStringLiteral("/core-setup.png")));
    }
}

// R-R3-17: seen natively as 820x572 growing to 1156x710 when a row with
// more action buttons was selected. Showing a hidden button raised the
// dialog's minimum width and Qt enlarged the window to fit. The window must
// open wide enough for the full row of ten and then keep its size whichever
// row is selected, with hidden buttons still hidden rather than disabled.
void ConnectionSelectorTest::rowSelectionNeverResizesTheWindow()
{
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    applyDarkPalette(*qApp);
    applyAppBaselineQss(*qApp);
    ConnectionSelector selector;
    // "Details" only; then Connect + Edit + Forget + Details.
    const ConnectionTargetRow detailsOnly{QStringLiteral("local"), ConnectionTargetKind::LocalRadio,
        QStringLiteral("This computer's Core"), QStringLiteral("Choose a local radio"),
        QStringLiteral("This computer"), QStringLiteral("Available"), false, false, false};
    selector.setTargets({detailsOnly, savedRow()});
    selector.setCurrentConnection(QStringLiteral("Not connected"), QString(), false, false);
    selector.show();
    QCoreApplication::processEvents();
    const QSize opened = selector.size();
    // A size that only holds because this platform's buttons happen to fit
    // 820 is not enough: the minimum itself must not move with the row.
    const QSize openedMinimum = selector.minimumSize();
    QVERIFY(opened.width() >= openedMinimum.width());

    const auto visibleButtons = [&selector] {
        int count = 0;
        for (QPushButton* button : selector.findChildren<QPushButton*>()) {
            if (button->isVisible()) { ++count; }
        }
        return count;
    };
    const auto assertRowFits = [&selector] {
        QList<QRect> rectangles;
        for (QPushButton* button : selector.findChildren<QPushButton*>()) {
            if (!button->isVisible()) { continue; }
            const QRect bounds(button->mapTo(&selector, QPoint()), button->size());
            QVERIFY2(selector.rect().contains(bounds), qPrintable(button->text()));
            QVERIFY2(button->width() >= button->sizeHint().width(), qPrintable(button->text()));
            for (const QRect& previous : rectangles) { QVERIFY(!previous.intersects(bounds)); }
            rectangles.append(bounds);
        }
    };
    // Nothing selected: the five row actions are hidden, not disabled.
    QCOMPARE(visibleButtons(), 5);

    selector.setSelectedKey(detailsOnly.key);
    QCoreApplication::processEvents();
    QCOMPARE(visibleButtons(), 6);
    QCOMPARE(selector.size(), opened);
    QCOMPARE(selector.minimumSize(), openedMinimum);

    selector.setSelectedKey(savedRow().key);
    QCoreApplication::processEvents();
    QCOMPARE(visibleButtons(), 9);
    QCOMPARE(selector.size(), opened);
    QCOMPARE(selector.minimumSize(), openedMinimum);

    // All ten, with the wider "Cancel retry" caption, still fit the
    // window as it opened.
    selector.setCurrentConnection(QStringLiteral("Retrying Core (attempt 3)"),
        QStringLiteral("Core: 192.168.109.106:4433\nRadio state unavailable\n"
                       "Retry delay: 4 s. Disconnect cancels automatic retries."), true, true);
    QCoreApplication::processEvents();
    QCOMPARE(visibleButtons(), 10);
    QCOMPARE(selector.size(), opened);
    QCOMPARE(selector.minimumSize(), openedMinimum);
    assertRowFits();

    // Detail text longer than the area scrolls or wraps inside it.
    QStringList longDetails;
    for (int line = 1; line <= 12; ++line) {
        longDetails.append(QStringLiteral("Detail line %1 describing the Core connection at length "
                                          "so that it cannot fit on one line of the window.").arg(line));
    }
    selector.setCurrentConnection(QStringLiteral("Core connected"), longDetails.join(QLatin1Char('\n')),
                                  true, false);
    QCoreApplication::processEvents();
    QCOMPARE(selector.size(), opened);
    QCOMPARE(selector.minimumSize(), openedMinimum);

    selector.setSelectedKey(detailsOnly.key);
    QCoreApplication::processEvents();
    QCOMPARE(selector.size(), opened);
    QCOMPARE(selector.minimumSize(), openedMinimum);
    selector.setSelectedKey(QString());
    QCoreApplication::processEvents();
    QCOMPARE(selector.size(), opened);
    QCOMPARE(selector.minimumSize(), openedMinimum);
    assertRowFits();
}

// Part C fix wave (R2-M2): a headless Core has no screen. The dialog names
// the status and now-active Remote Access pages, plus the console command.
void ConnectionSelectorTest::theCodeDialogNamesPlacesThatShowTheCode()
{
    AddCoreByCodeDialog dialog;
    auto* explanation = dialog.findChild<QLabel*>(QStringLiteral("addCoreByCodeExplanation"));
    QVERIFY(explanation);
    const QString text = explanation->text();
    QVERIFY(text.contains(QStringLiteral("status page")));
    QVERIFY(text.contains(QStringLiteral("nereusd pairing show")));
    QVERIFY(!text.contains(QStringLiteral("screen")));
    QVERIFY(text.contains(QStringLiteral("Remote Access page")));
}

void ConnectionSelectorTest::codeAloneIsTheNormalPairingDialogPath()
{
    AddCoreByCodeDialog dialog;
    auto* code = dialog.findChild<QLineEdit*>(QStringLiteral("addCoreByCodeCode"));
    auto* address = dialog.findChild<QLineEdit*>(QStringLiteral("addCoreByCodeAddress"));
    auto* pair = dialog.findChild<QPushButton*>(QStringLiteral("addCoreByCodePair"));
    QVERIFY(code && address && pair);
    dialog.show();
    QCoreApplication::processEvents();
    QVERIFY(!address->isVisibleTo(&dialog));
    code->setText(QStringLiteral("not-a-code"));
    pair->click();
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    code->setText(QStringLiteral("7-anvil-harbor"));
    pair->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(dialog.host(), QString());
    QCOMPARE(dialog.port(), quint16(0));

    AddCoreByCodeDialog direct(QStringLiteral("shack-core.local"));
    auto* directAddress = direct.findChild<QLineEdit*>(QStringLiteral("addCoreByCodeAddress"));
    QVERIFY(directAddress);
    direct.show();
    QCoreApplication::processEvents();
    QVERIFY(directAddress->isVisibleTo(&direct));
    QCOMPARE(directAddress->text(), QStringLiteral("shack-core.local"));
    auto* directCode = direct.findChild<QLineEdit*>(QStringLiteral("addCoreByCodeCode"));
    auto* directPair = direct.findChild<QPushButton*>(QStringLiteral("addCoreByCodePair"));
    QVERIFY(directCode && directPair);
    directCode->setText(QStringLiteral("7-anvil-harbor"));
    directAddress->setText(QStringLiteral("https://not-a-core"));
    directPair->click();
    QCOMPARE(direct.result(), int(QDialog::Rejected));
    directAddress->setText(QStringLiteral("shack-core.local"));
    directPair->click();
    QCOMPARE(direct.result(), int(QDialog::Accepted));
    QCOMPARE(direct.host(), QStringLiteral("shack-core.local"));
}

void ConnectionSelectorTest::serviceOnlyEditorKeepsIdentityAndRouteWithoutAnAddress()
{
    SavedCoreTarget paired;
    paired.id = QStringLiteral("paired");
    paired.label = QStringLiteral("SkyHQ");
    paired.connection.identityFingerprint = QByteArray(32, 'k');
    paired.connection.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    CoreTargetEditor editor(paired);
    auto* address = editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorAddress"));
    auto* save = editor.findChild<QPushButton*>(QStringLiteral("coreTargetEditorSave"));
    QVERIFY(address && save);
    QVERIFY(address->text().isEmpty());
    auto* label = editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorLabel"));
    QVERIFY(label);
    label->setText(QStringLiteral("SkyHQ renamed"));
    save->click();
    QCOMPARE(editor.result(), int(QDialog::Accepted));
    const SavedCoreTarget updated = editor.target();
    QCOMPARE(updated.label, QStringLiteral("SkyHQ renamed"));
    QCOMPARE(updated.connection.url, QString());
    QCOMPARE(updated.connection.identityFingerprint, paired.connection.identityFingerprint);
    QCOMPARE(updated.connection.rendezvousId, paired.connection.rendezvousId);
}

QTEST_MAIN(ConnectionSelectorTest)

#include "tst_connection_selector.moc"
