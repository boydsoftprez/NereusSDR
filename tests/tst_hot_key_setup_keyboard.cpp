// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-07 - J.J. Boyd (KG4VCF), hot keys: VOX on or off (vox.toggle).
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-08 - J.J. Boyd (KG4VCF), hot keys: band stack rows can be set now
//                that the window has band stacking.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// Setup > Keys and controllers > Keyboard: the map, the list, Set and its
// refusals, Acts on, disabled rows with their reasons, import and export, and
// the off switch in the page and the View menu, local and remote.
#include <QtTest/QtTest>

#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "HotKeyWindowActivation.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include <QScopeGuard>
#include <QScrollArea>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "HotKeyWindowActivation.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/SetupDialog.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/KeyboardRouter.h"
#include "gui/hotkeys/MainWindowHotKeyTargets.h"
#include "gui/setup/HotKeySetupPages.h"
#include "gui/setup/KeyboardMapWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

void sendKey(QWidget* to, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(type, key, modifiers);
    QCoreApplication::sendEvent(to, &event);
}

void down(QWidget* to, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendKey(to, QEvent::KeyPress, key, modifiers);
}

void up(QWidget* to, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendKey(to, QEvent::KeyRelease, key, modifiers);
}

void tap(QWidget* to, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    down(to, key, modifiers);
    up(to, key, modifiers);
}

template <typename T>
T* named(QWidget* root, const QString& name)
{
    return root->findChild<T*>(name);
}

QPushButton* button(QWidget* root, const QString& prefix, const QString& id)
{
    return named<QPushButton>(root, prefix + id);
}

QAbstractButton* keyCap(QWidget* root, const QString& chord)
{
    auto* const map = root->findChild<KeyboardMapWidget*>();
    return map == nullptr ? nullptr : map->findChild<QAbstractButton*>(chord);
}

// Which row an action is on, by its order in the list.
int rowOf(const QString& id)
{
    const QList<HotKeyAction>& actions = HotKeyActionList::all();
    for (int i = 0; i < actions.size(); ++i) {
        if (actions.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

QString keyText(QWidget* page, const QString& id)
{
    auto* const table = page->findChild<QTableWidget*>(QStringLiteral("hotKeyList"));
    return table->item(rowOf(id), 1)->text();
}

QString messageOf(QWidget* page, const QString& id)
{
    auto* const frame = named<QLabel>(page, QStringLiteral("hotKeyMessage:") + id);
    return frame != nullptr && !frame->parentWidget()->isHidden() ? frame->text() : QString();
}

struct PageBench {
    HotKeyBindings bindings;
    KeyboardPage page{&bindings};
    QTableWidget* table{page.findChild<QTableWidget*>(QStringLiteral("hotKeyList"))};

    PageBench() { page.resize(900, 650); }

    void beginSet(const QString& id) { button(&page, QStringLiteral("hotKeySet:"), id)->click(); }
};

using NereusSDR::Test::activateAndWait;
using NereusSDR::Test::waitActive;

// One real window, offscreen, connected, with Slice A, and Setup open on the page.
struct WindowBench {
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    MoxController* mox{model->moxController()};
    HotKeyBindings* bindings{window.hotKeyBindings()};
    SetupDialog* dialog{nullptr};
    KeyboardPage* page{nullptr};

    WindowBench()
    {
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model->transmitModel().setMicSourceLocked(false);
        model->transmitModel().setMicSource(MicSource::Radio);
        SliceModel* const slice = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
        slice->setDspMode(DSPMode::USB);
        window.resize(1200, 800);
        window.show();
        window.activateWindow();
    }

    bool ready()
    {
        if (!activateAndWait(&window)) {
            return false;
        }
        if (QWidget* const focus = QApplication::focusWidget()) {
            focus->clearFocus();
        }
        return true;
    }

    bool openPage()
    {
        const auto created = QObject::connect(&window, &MainWindow::setupDialogCreated,
                                              [this](SetupDialog* d) { dialog = d; });
        QMetaObject::invokeMethod(&window, "openSetupAtPage", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("Keyboard")));
        QObject::disconnect(created);
        if (dialog == nullptr) {
            return false;
        }
        page = dialog->findChild<KeyboardPage*>();
        return page != nullptr && page->isVisible();
    }

    QAction* viewAction(const QString& text)
    {
        for (QAction* const action : window.findChildren<QAction*>()) {
            if (action->text() == text) {
                return action;
            }
        }
        return nullptr;
    }
};

// As tst_hot_key_cw: the window shares slices and controls each one.
bool connectSharing(RemoteWindowHarness& h)
{
    h.startStartupConnection();
    StationClient* const client = h.client();
    if (client == nullptr
        || !QTest::qWaitFor([client] { return client->stationLinkReady(); }, 10000)
        || !client->remoteSliceAccessAvailable()) {
        return false;
    }
    return QTest::qWaitFor([&h, client] {
        const QList<int> live = h.station().sliceOwnership()->liveSlices();
        for (const int id : live) {
            const auto entry = client->sliceAccess()->entry(id);
            if (!entry || entry->controllerDeviceId != QStringLiteral("token:1")) {
                return false;
            }
        }
        return !live.isEmpty();
    }, 10000);
}

bool connectRemote(RemoteWindowHarness& h, DSPMode mode)
{
    if (!h.start()) {
        return false;
    }
    h.server().setRemoteTransmitAllowed(true);
    h.server().setTokenSessionsMayTransmitForTest(true);
    if (!connectSharing(h)) {
        return false;
    }
    StationCapabilities granted = h.server().buildCapabilities();
    granted.txPermitted = true;
    h.pushCapabilities(granted);
    StationClient* const client = h.client();
    if (!QTest::qWaitFor([client] { return client->capabilities().txPermitted; }, 10000)) {
        return false;
    }
    RadioModel* const remote = h.window()->radioModel();
    const auto slices = h.station().slices();
    if (slices.isEmpty()) {
        return false;
    }
    slices.first()->setDspMode(mode);
    return QTest::qWaitFor([remote, mode] {
        const SliceModel* const active = remote->activeSlice();
        return active != nullptr && active->dspMode() == mode;
    }, 10000);
}

} // namespace

class TestHotKeySetupKeyboard final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("hot-key-setup-keyboard")));
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
        // The first-run dialog would take the keyboard from the window under test.
        NereusSDR::Test::markAudioFirstRunDone();
    }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    // ---- The map ----

    void everyDefaultAndReservedKeyIsOnTheMap()
    {
        PageBench b;
        auto* const map = b.page.findChild<KeyboardMapWidget*>();
        QSet<QString> onMap;
        for (int layer = 0; layer < 4; ++layer) {
            map->setLayer(layer);
            for (const QAbstractButton* const cap : map->findChildren<QAbstractButton*>()) {
                onMap.insert(cap->objectName());
            }
        }
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            for (const QString& key : action.defaultKeys) {
                QVERIFY2(onMap.contains(key), qPrintable(key));
            }
        }
        for (const char* key : {"Ctrl+,", "Ctrl+Q", "Ctrl+K", "Ctrl+L", "Ctrl+R", "Ctrl+M", "Ctrl+X", "Ctrl+1",
                                "Ctrl+9", "Ctrl+Shift+A", "Ctrl+Shift+S", "Ctrl+Shift+R", "Ctrl+Shift+D",
                                "Ctrl+Shift+K", "Ctrl+Shift+N", "Ctrl+Shift+X"}) {
            QVERIFY2(onMap.contains(QLatin1String(key)), key);
        }
    }

    // The map fits the width it has, with no sideways scroll anywhere on the page.
    void theMapFitsEveryWidth_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<bool>("stacked");
        QTest::newRow("820 by 600 window") << 563 << true;
        QTest::newRow("900 by 650 window") << 643 << true;
        QTest::newRow("just under full width") << 1000 << false;
        QTest::newRow("wide") << 1400 << false;
    }

    void theMapFitsEveryWidth()
    {
        QFETCH(int, width);
        QFETCH(bool, stacked);
        HotKeyBindings bindings;
        KeyboardMapWidget map(&bindings);
        map.resize(width, map.height());
        map.show();
        QVERIFY(QTest::qWaitForWindowExposed(&map));
        QCOMPARE(map.stacked(), stacked);

        const QList<QAbstractButton*> caps = map.findChildren<QAbstractButton*>();
        QVERIFY(caps.size() > 90);
        for (qsizetype i = 0; i < caps.size(); ++i) {
            const QRect at = caps.at(i)->geometry();
            QVERIFY2(map.rect().contains(at), qPrintable(caps.at(i)->accessibleName()));
            QVERIFY2(at.width() >= 20 && at.height() >= 20, qPrintable(caps.at(i)->accessibleName()));
            for (qsizetype j = i + 1; j < caps.size(); ++j) {
                QVERIFY2(!at.intersects(caps.at(j)->geometry()),
                         qPrintable(caps.at(i)->accessibleName() + QStringLiteral(" on ")
                                    + caps.at(j)->accessibleName()));
            }
        }

        // The navigation keys and keypad go below when short, beside when not.
        const auto capAt = [&map](const char* chord) {
            return map.findChild<QAbstractButton*>(QString::fromLatin1(chord));
        };
        const QRect space = capAt("Space")->geometry();
        for (const char* chord : {"Home", "Left", "Num+7", "Num+Enter"}) {
            QAbstractButton* const cap = capAt(chord);
            QVERIFY2(cap != nullptr, chord);
            if (stacked) {
                QVERIFY2(cap->geometry().top() > space.bottom(), chord);
            } else {
                QVERIFY2(cap->geometry().left() > space.right(), chord);
            }
        }

        // Every key answers a click in this layout, the far ones included.
        QSignalSpy clicked(&map, &KeyboardMapWidget::keyClicked);
        for (const char* chord : {"Q", "Space", "Home", "Right", "Num+7", "Num+."}) {
            clicked.clear();
            QAbstractButton* const cap = capAt(chord);
            QVERIFY2(cap != nullptr && cap->isEnabled(), chord);
            QTest::mouseClick(cap, Qt::LeftButton);
            QCOMPARE(clicked.size(), 1);
            QCOMPARE(clicked.first().first().toString(), QString::fromLatin1(chord));
        }

        // The tooltip has the action's whole name, whatever the cap has room for.
        const QString spaceAction = bindings.actionForChord(QStringLiteral("Space"));
        QCOMPARE(capAt("Space")->toolTip(), HotKeyActionList::find(spaceAction)->name);
    }

    void thePageHasNoSidewaysScrollForTheMap()
    {
        PageBench b;
        auto* const map = b.page.findChild<KeyboardMapWidget*>();
        for (QWidget* parent = map->parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
            if (auto* const area = qobject_cast<QScrollArea*>(parent)) {
                QVERIFY(!area->objectName().contains(QStringLiteral("hotKeyMap")));
            }
        }
    }

    // The group colors are data, which the color sweep leaves alone, so the
    // chrome around them is checked here: one role in the look, the old color out of it.
    void aKeyWithNoActionTakesThePalette_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::addColumn<QColor>("fill");
        QTest::newRow("classic look") << false << QColor(0x13, 0x13, 0x26);
        QTest::newRow("workspace look")
            << true << Workspace::WorkspacePalette::glassAndDepth().color(QStringLiteral("button"));
    }

    void aKeyWithNoActionTakesThePalette()
    {
        QFETCH(bool, workspaceLook);
        QFETCH(QColor, fill);
        WorkspacePreview::setEnabledForTest(workspaceLook);
        const auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });
        HotKeyBindings bindings;
        KeyboardMapWidget map(&bindings);
        QAbstractButton* const cap = map.findChild<QAbstractButton*>(QStringLiteral("Esc"));
        QVERIFY(cap != nullptr);
        QVERIFY(cap->property("keyGroup").toString().isEmpty());
        const QImage image = cap->grab().toImage();
        const QColor painted = image.pixelColor(image.width() / 2, image.height() - 4);
        QCOMPARE(painted.name(), fill.name());
    }

    void layersShowTheirKeysColoredByGroup()
    {
        PageBench b;
        QAbstractButton* cap = keyCap(&b.page, QStringLiteral("Space"));
        QVERIFY(cap != nullptr);
        QCOMPARE(cap->property("keyGroup").toString(), QStringLiteral("Transmit"));
        QVERIFY(KeyboardMapWidget::groupColor(QStringLiteral("Transmit")).isValid());
        QCOMPARE(keyCap(&b.page, QStringLiteral("L"))->property("keyGroup").toString(), QStringLiteral("Tuning"));
        QVERIFY(keyCap(&b.page, QStringLiteral("Ctrl+T")) == nullptr);

        for (const int layer : {0, 1, 2, 3}) {
            QVERIFY(named<QPushButton>(&b.page, QStringLiteral("hotKeyLayer%1").arg(layer)) != nullptr);
        }
        named<QPushButton>(&b.page, QStringLiteral("hotKeyLayer2"))->click();
        cap = keyCap(&b.page, QStringLiteral("Ctrl+T"));
        QVERIFY(cap != nullptr);
        QCOMPARE(cap->property("keyGroup").toString(), QStringLiteral("Transmit"));
        // A reserved key shows, as a menu key, under the item's name.
        cap = keyCap(&b.page, QStringLiteral("Ctrl+,"));
        QVERIFY(cap != nullptr);
        QCOMPARE(cap->property("keyGroup").toString(), QStringLiteral("Menu"));
        QVERIFY2(cap->toolTip().contains(QStringLiteral("Settings")), qPrintable(cap->toolTip()));

        named<QPushButton>(&b.page, QStringLiteral("hotKeyLayer3"))->click();
        QCOMPARE(keyCap(&b.page, QStringLiteral("Ctrl+Shift+M"))->property("keyGroup").toString(),
                 QStringLiteral("Transmit"));
        named<QPushButton>(&b.page, QStringLiteral("hotKeyLayer1"))->click();
        QCOMPARE(keyCap(&b.page, QStringLiteral("Shift+M"))->property("keyGroup").toString(),
                 QStringLiteral("Band and mode"));
    }

    void onlyOneGroupChipAtATime()
    {
        PageBench b;
        auto* const tuning = named<QPushButton>(&b.page, QStringLiteral("hotKeyChip:Tuning"));
        auto* const menu = named<QPushButton>(&b.page, QStringLiteral("hotKeyChip:Menu"));
        QVERIFY(tuning != nullptr && menu != nullptr);
        tuning->click();
        QVERIFY(tuning->isChecked());
        menu->click();
        QVERIFY(menu->isChecked());
        QVERIFY(!tuning->isChecked());
    }

    // Left and Right tune, and a cap that only said "down" or "up" didn't say so.
    void theArrowCapsSayTheyTune_data()
    {
        QTest::addColumn<QString>("chord");
        QTest::addColumn<QString>("shown");
        QTest::newRow("left") << QStringLiteral("Left") << QStringLiteral("Tune down");
        QTest::newRow("right") << QStringLiteral("Right") << QStringLiteral("Tune up");
        QTest::newRow("shift left") << QStringLiteral("Shift+Left") << QStringLiteral("Tune down x10");
        QTest::newRow("shift right") << QStringLiteral("Shift+Right") << QStringLiteral("Tune up x10");
        // Short enough still to show on a cap.
        QTest::newRow("step") << QStringLiteral("[") << QStringLiteral("Step smaller");
    }

    void theArrowCapsSayTheyTune()
    {
        QFETCH(QString, chord);
        QFETCH(QString, shown);
        HotKeyBindings bindings;
        KeyboardMapWidget map(&bindings);
        if (chord.startsWith(QLatin1String("Shift+"))) {
            map.setLayer(KeyboardMapWidget::LayerShift);
        }
        QAbstractButton* const cap = map.findChild<QAbstractButton*>(chord);
        QVERIFY2(cap != nullptr, qPrintable(chord));
        QCOMPARE(cap->accessibleName(), QStringLiteral("%1: %2").arg(chord, shown));
    }

    void aMapKeysChordIsTheOneTheRouterLooksUp_data()
    {
        QTest::addColumn<int>("layer");
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("modifiers");
        const auto row = [](const char* name, int layer, int key, Qt::KeyboardModifiers modifiers) {
            QTest::newRow(name) << layer << key << int(modifiers);
        };
        row("plain letter", 0, Qt::Key_Q, Qt::NoModifier);
        row("backquote", 0, Qt::Key_QuoteLeft, Qt::NoModifier);
        row("return", 0, Qt::Key_Return, Qt::NoModifier);
        row("backspace", 0, Qt::Key_Backspace, Qt::NoModifier);
        row("page down", 0, Qt::Key_PageDown, Qt::NoModifier);
        row("insert", 0, Qt::Key_Insert, Qt::NoModifier);
        row("left", 0, Qt::Key_Left, Qt::NoModifier);
        row("left as macOS reports it", 0, Qt::Key_Left, Qt::KeypadModifier);
        row("keypad plus", 0, Qt::Key_Plus, Qt::KeypadModifier);
        row("keypad seven", 0, Qt::Key_7, Qt::KeypadModifier);
        row("keypad enter", 0, Qt::Key_Enter, Qt::KeypadModifier);
        row("shift one", 1, Qt::Key_Exclam, Qt::ShiftModifier);
        row("shift brace", 1, Qt::Key_BraceLeft, Qt::ShiftModifier);
        row("shift letter", 1, Qt::Key_M, Qt::ShiftModifier);
        row("ctrl comma", 2, Qt::Key_Comma, Qt::ControlModifier);
        row("ctrl f10", 2, Qt::Key_F10, Qt::ControlModifier);
        row("ctrl shift slash", 3, Qt::Key_Question, Qt::ControlModifier | Qt::ShiftModifier);
        row("ctrl shift f10", 3, Qt::Key_F10, Qt::ControlModifier | Qt::ShiftModifier);
    }

    void aMapKeysChordIsTheOneTheRouterLooksUp()
    {
        QFETCH(int, layer);
        QFETCH(int, key);
        QFETCH(int, modifiers);
        PageBench b;
        b.page.findChild<KeyboardMapWidget*>()->setLayer(layer);
        const QKeyEvent event(QEvent::KeyPress, key, Qt::KeyboardModifiers(modifiers));
        const QString chord = KeyboardRouter::chordFromEvent(&event);
        QVERIFY(!chord.isEmpty());
        QVERIFY2(keyCap(&b.page, chord) != nullptr, qPrintable(chord));
    }

    void aModifierCapIsDisabledWithAReason()
    {
        PageBench b;
        auto* const map = b.page.findChild<KeyboardMapWidget*>();
        int disabled = 0;
        for (const QAbstractButton* const cap : map->findChildren<QAbstractButton*>()) {
            if (cap->isEnabled()) {
                continue;
            }
            ++disabled;
            QVERIFY2(!cap->toolTip().isEmpty(), qPrintable(cap->accessibleName()));
        }
        QVERIFY(disabled > 0);
    }

    void clickingAKeyOffersWhatItCan()
    {
        PageBench b;
        b.page.show();
        auto* const text = named<QLabel>(&b.page, QStringLiteral("hotKeyPickText"));
        auto* const note = named<QLabel>(&b.page, QStringLiteral("hotKeyPickNote"));
        auto* const change = named<QPushButton>(&b.page, QStringLiteral("hotKeyPickChange"));
        auto* const clear = named<QPushButton>(&b.page, QStringLiteral("hotKeyPickClear"));
        auto* const assign = named<QComboBox>(&b.page, QStringLiteral("hotKeyPickAssign"));
        QCOMPARE(text->text(), QStringLiteral("Click a key."));

        // A key an action has: the action, Change and Clear, and its row selected.
        keyCap(&b.page, QStringLiteral("L"))->click();
        QCOMPARE(text->text(), QStringLiteral("Lock"));
        QVERIFY(change->isVisible() && clear->isVisible() && !assign->isVisible());
        QCOMPARE(b.table->selectionModel()->selectedRows().first().row(), rowOf(QStringLiteral("slice.lock")));

        // A free key: choose an action for it.
        keyCap(&b.page, QStringLiteral("Q"))->click();
        QVERIFY(assign->isVisible() && !change->isVisible());
        const int lockIndex = assign->findData(QStringLiteral("slice.lock"));
        QVERIFY(lockIndex > 0);
        assign->setCurrentIndex(lockIndex);
        emit assign->activated(lockIndex);
        QCOMPARE(b.bindings.keysFor(QStringLiteral("slice.lock")), QStringList{QStringLiteral("Q")});
        QCOMPARE(text->text(), QStringLiteral("Lock"));

        // A menu key: named, and not changeable here.
        named<QPushButton>(&b.page, QStringLiteral("hotKeyLayer2"))->click();
        QCOMPARE(text->text(), QStringLiteral("Click a key."));
        keyCap(&b.page, QStringLiteral("Ctrl+Q"))->click();
        QCOMPARE(text->text(), QStringLiteral("Quit menu shortcut"));
        QVERIFY(note->isVisible());
        QCOMPARE(note->text(), QStringLiteral("Not changeable here"));
        QVERIFY(!change->isVisible() && !clear->isVisible() && !assign->isVisible());
    }

    void theMapsClearTakesOnlyThatKey()
    {
        PageBench b;
        b.page.show();
        keyCap(&b.page, QStringLiteral("Up"))->click();
        named<QPushButton>(&b.page, QStringLiteral("hotKeyPickClear"))->click();
        QCOMPARE(b.bindings.keysFor(QStringLiteral("volume.up")), QStringList{QStringLiteral("Num++")});
        QCOMPARE(keyCap(&b.page, QStringLiteral("Up"))->property("keyGroup").toString(), QString());
    }

    void theMapsChangeListensForTheOwner()
    {
        PageBench b;
        b.page.show();
        named<QLineEdit>(&b.page, QStringLiteral("hotKeySearch"))->setText(QStringLiteral("zoom"));
        keyCap(&b.page, QStringLiteral("L"))->click();
        named<QPushButton>(&b.page, QStringLiteral("hotKeyPickChange"))->click();
        // The search went, so the row can show it is listening.
        QCOMPARE(named<QLineEdit>(&b.page, QStringLiteral("hotKeySearch"))->text(), QString());
        QVERIFY(!b.table->isRowHidden(rowOf(QStringLiteral("slice.lock"))));
        QCOMPARE(keyText(&b.page, QStringLiteral("slice.lock")), QStringLiteral("Press a key"));
        tap(&b.page, Qt::Key_F5);
        QCOMPARE(b.bindings.keysFor(QStringLiteral("slice.lock")), QStringList{QStringLiteral("F5")});
    }

    // ---- Set ----

    void setBindsThePressedKey()
    {
        PageBench b;
        const QString id = QStringLiteral("tune.up");
        QCOMPARE(keyText(&b.page, id), QStringLiteral("Right"));
        b.beginSet(id);
        QCOMPARE(keyText(&b.page, id), QStringLiteral("Press a key"));
        // A modifier alone waits for its key.
        tap(&b.page, Qt::Key_Shift, Qt::ShiftModifier);
        QCOMPARE(keyText(&b.page, id), QStringLiteral("Press a key"));
        down(&b.page, Qt::Key_F2);
        QCOMPARE(b.bindings.keysFor(id), QStringList{QStringLiteral("F2")});
        QCOMPARE(keyText(&b.page, id), QStringLiteral("F2"));
        QVERIFY(messageOf(&b.page, id).isEmpty());
    }

    void escapeCancels()
    {
        PageBench b;
        const QString id = QStringLiteral("tune.up");
        const QString before = b.bindings.exportText();
        b.beginSet(id);
        tap(&b.page, Qt::Key_Escape);
        QCOMPARE(keyText(&b.page, id), QStringLiteral("Right"));
        QCOMPARE(b.bindings.exportText(), before);
        // No longer listening: the next key binds nothing.
        tap(&b.page, Qt::Key_F2);
        QCOMPARE(b.bindings.exportText(), before);
    }

    void clickingElsewhereCancels()
    {
        PageBench b;
        const QString before = b.bindings.exportText();
        b.page.show();
        b.beginSet(QStringLiteral("tune.up"));
        QTest::mouseClick(b.table->viewport(), Qt::LeftButton);
        QCOMPARE(keyText(&b.page, QStringLiteral("tune.up")), QStringLiteral("Right"));
        tap(&b.page, Qt::Key_F2);
        QCOMPARE(b.bindings.exportText(), before);
    }

    void aConflictOffersReplaceOrCancel()
    {
        PageBench b;
        const QString id = QStringLiteral("tune.up");
        const QString owner = QStringLiteral("bandstack.next");
        const QString before = b.bindings.exportText();

        b.beginSet(id);
        down(&b.page, Qt::Key_M);
        QCOMPARE(messageOf(&b.page, id), QStringLiteral("M is already Band stack next."));
        auto* const replace = button(&b.page, QStringLiteral("hotKeyReplace:"), id);
        auto* const dismiss = button(&b.page, QStringLiteral("hotKeyDismiss:"), id);
        QVERIFY(!replace->isHidden());
        QCOMPARE(dismiss->text(), QStringLiteral("Cancel"));
        QCOMPARE(b.bindings.exportText(), before);
        dismiss->click();
        QVERIFY(messageOf(&b.page, id).isEmpty());
        QCOMPARE(b.bindings.exportText(), before);

        b.beginSet(id);
        down(&b.page, Qt::Key_M);
        replace->click();
        QVERIFY(messageOf(&b.page, id).isEmpty());
        QCOMPARE(b.bindings.keysFor(id), QStringList{QStringLiteral("M")});
        QVERIFY(b.bindings.keysFor(owner).isEmpty());
        QCOMPARE(keyText(&b.page, owner), QStringLiteral("None"));
    }

    void eachRefusalShowsItsReasonAndChangesNothing_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("modifiers");
        QTest::addColumn<QString>("reason");
        const QString transmits = QStringLiteral(
            "Q is a plain key, so a stray press would transmit. Use Space, a function key or a combination.");
        const QString stops = QStringLiteral(
            "Q is a plain key, so a stray press would stop the message. Use Space, a function key or a combination.");
        QTest::newRow("hold on a letter") << QStringLiteral("tx.hold") << int(Qt::Key_Q) << 0 << transmits;
        QTest::newRow("cw message on a letter") << QStringLiteral("cw.msg1") << int(Qt::Key_Q) << 0 << transmits;
        QTest::newRow("stop cw on a letter") << QStringLiteral("cw.stop") << int(Qt::Key_Q) << 0 << stops;
        QTest::newRow("stop voice on a letter") << QStringLiteral("voice.stop") << int(Qt::Key_Q) << 0 << stops;
        QTest::newRow("MOX on Space") << QStringLiteral("tx.mox") << int(Qt::Key_Space) << 0
                                      << QStringLiteral("MOX stays on after you let go, so it needs a combination.");
        QTest::newRow("a menu's key") << QStringLiteral("tune.up") << int(Qt::Key_Comma) << int(Qt::ControlModifier)
                                      << KeyboardMapWidget::displayText(QStringLiteral("Ctrl+, is the Settings menu shortcut."));
    }

    void eachRefusalShowsItsReasonAndChangesNothing()
    {
        QFETCH(QString, id);
        QFETCH(int, key);
        QFETCH(int, modifiers);
        QFETCH(QString, reason);
        PageBench b;
        const QString before = b.bindings.exportText();
        b.beginSet(id);
        down(&b.page, key, Qt::KeyboardModifiers(modifiers));
        QCOMPARE(messageOf(&b.page, id), reason);
        QCOMPARE(b.bindings.exportText(), before);
        auto* const dismiss = button(&b.page, QStringLiteral("hotKeyDismiss:"), id);
        QVERIFY(button(&b.page, QStringLiteral("hotKeyReplace:"), id)->isHidden());
        QCOMPARE(dismiss->text(), QStringLiteral("OK"));
        dismiss->click();
        QVERIFY(messageOf(&b.page, id).isEmpty());
    }

    void defaultCommaPeriodAndSlashStayAllowed()
    {
        PageBench b;
        const QString id = QStringLiteral("cw.straight");
        b.bindings.clear(QStringLiteral("cw.dot"));
        b.beginSet(id);
        down(&b.page, Qt::Key_Comma);
        QVERIFY(messageOf(&b.page, id).isEmpty());
        QCOMPARE(b.bindings.keysFor(id), QStringList{QStringLiteral(",")});
    }

    // ---- The list ----

    void theListSearchesNamesAndKeys()
    {
        PageBench b;
        QCOMPARE(b.table->rowCount(), HotKeyActionList::all().size() + 1);
        const int noMatch = int(HotKeyActionList::all().size());
        QVERIFY(b.table->isRowHidden(noMatch));
        const auto shown = [&b, noMatch] {
            QStringList ids;
            for (int i = 0; i < noMatch; ++i) {
                if (!b.table->isRowHidden(i)) {
                    ids.append(HotKeyActionList::all().at(i).id);
                }
            }
            return ids;
        };
        QCOMPARE(shown().size(), 51);

        auto* const search = named<QLineEdit>(&b.page, QStringLiteral("hotKeySearch"));
        search->setText(QStringLiteral("ZOOM"));
        QCOMPARE(shown(), (QStringList{QStringLiteral("zoom.in"), QStringLiteral("zoom.out")}));
        search->setText(QStringLiteral("space"));
        QCOMPARE(shown(), QStringList{QStringLiteral("tx.hold")});
        search->setText(QStringLiteral("nothing has this name"));
        QVERIFY(shown().isEmpty());
        QVERIFY(!b.table->isRowHidden(noMatch));
        search->clear();
        QVERIFY(b.table->isRowHidden(noMatch));

        auto* const group = named<QComboBox>(&b.page, QStringLiteral("hotKeyGroupFilter"));
        group->setCurrentIndex(group->findData(QStringLiteral("Display")));
        QCOMPARE(shown(), (QStringList{QStringLiteral("zoom.in"), QStringLiteral("zoom.out")}));
        search->setText(QStringLiteral("space"));
        QVERIFY(shown().isEmpty());
    }

    void clearDefaultAndResetAll()
    {
        PageBench b;
        const QString id = QStringLiteral("volume.up");
        auto* const clear = button(&b.page, QStringLiteral("hotKeyClear:"), id);
        auto* const toDefault = button(&b.page, QStringLiteral("hotKeyDefault:"), id);
        QVERIFY(clear->isEnabled());
        QVERIFY(!toDefault->isEnabled());
        QVERIFY(!toDefault->toolTip().isEmpty());

        clear->click();
        QVERIFY(b.bindings.keysFor(id).isEmpty());
        QCOMPARE(keyText(&b.page, id), QStringLiteral("None"));
        QVERIFY(!clear->isEnabled());
        QVERIFY(toDefault->isEnabled());

        toDefault->click();
        QCOMPARE(keyText(&b.page, id), KeyboardMapWidget::displayText(QStringLiteral("Up, Num++")));
        QVERIFY(!toDefault->isEnabled());

        clear->click();
        b.bindings.setPin(QStringLiteral("tune.up"), QLatin1Char('B'));
        named<QPushButton>(&b.page, QStringLiteral("hotKeyResetAll"))->click();
        QCOMPARE(keyText(&b.page, id), KeyboardMapWidget::displayText(QStringLiteral("Up, Num++")));
        QVERIFY(b.bindings.pinFor(QStringLiteral("tune.up")).isNull());
        QCOMPARE(named<QComboBox>(&b.page, QStringLiteral("hotKeyActsOn:tune.up"))->currentIndex(), 0);
    }

    void actsOnFollowsTheActionsScope()
    {
        PageBench b;
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            auto* const combo = named<QComboBox>(&b.page, QStringLiteral("hotKeyActsOn:") + action.id);
            QVERIFY2(combo != nullptr, qPrintable(action.id));
            if (action.scope == HotKeyScope::Slice) {
                QVERIFY2(combo->isEnabled(), qPrintable(action.id));
                QCOMPARE(combo->count(), 5);
                QCOMPARE(combo->itemText(0), QStringLiteral("Selected slice"));
                QCOMPARE(combo->itemText(1), QStringLiteral("Slice A"));
                QCOMPARE(combo->itemText(4), QStringLiteral("Slice D"));
                continue;
            }
            QVERIFY2(!combo->isEnabled(), qPrintable(action.id));
            QVERIFY2(!combo->toolTip().isEmpty(), qPrintable(action.id));
            QCOMPARE(combo->accessibleDescription(), combo->toolTip());
            const QString shown = combo->currentText();
            switch (action.scope) {
            case HotKeyScope::TransmitSlice:
                QCOMPARE(shown, QStringLiteral("Transmit slice"));
                break;
            case HotKeyScope::Pan:
                QCOMPARE(shown, QStringLiteral("This panadapter"));
                break;
            default:
                QCOMPARE(shown, QStringLiteral("This window"));
                break;
            }
        }
        QCOMPARE(named<QComboBox>(&b.page, QStringLiteral("hotKeyActsOn:slice.next"))->currentText(),
                 QStringLiteral("This window"));
        QCOMPARE(named<QComboBox>(&b.page, QStringLiteral("hotKeyActsOn:bandstack.next"))->currentText(),
                 QStringLiteral("This window"));
    }

    void pickingASliceSetsThePin()
    {
        PageBench b;
        const QString id = QStringLiteral("tune.up");
        auto* const combo = named<QComboBox>(&b.page, QStringLiteral("hotKeyActsOn:") + id);
        combo->setCurrentIndex(3);
        emit combo->activated(3);
        QCOMPARE(b.bindings.pinFor(id), QLatin1Char('C'));
        QVERIFY(button(&b.page, QStringLiteral("hotKeyDefault:"), id)->isEnabled());
        // Clear keeps the pin.
        button(&b.page, QStringLiteral("hotKeyClear:"), id)->click();
        QCOMPARE(b.bindings.pinFor(id), QLatin1Char('C'));
        QCOMPARE(combo->currentText(), QStringLiteral("Slice C"));

        b.bindings.setPin(id, QChar());
        QCOMPARE(combo->currentText(), QStringLiteral("Selected slice"));
        combo->setCurrentIndex(1);
        emit combo->activated(1);
        QCOMPARE(b.bindings.pinFor(id), QLatin1Char('A'));
        combo->setCurrentIndex(0);
        emit combo->activated(0);
        QVERIFY(b.bindings.pinFor(id).isNull());
    }

    // ---- Reasons ----

    void aDisabledRowStaysInTheListWithItsReason()
    {
        PageBench b;
        const QString reason = QStringLiteral("Not on this window.");
        const QString id = QStringLiteral("mute");
        bool unavailable = true;
        b.page.setUnavailableReasonProvider([&](const QString& asked) {
            return unavailable && asked == id ? reason : QString();
        });

        QVERIFY(!b.table->isRowHidden(rowOf(id)));
        for (const QString& prefix : {QStringLiteral("hotKeySet:"), QStringLiteral("hotKeyClear:"),
                                      QStringLiteral("hotKeyDefault:")}) {
            auto* const control = button(&b.page, prefix, id);
            QVERIFY(!control->isEnabled());
            QCOMPARE(control->toolTip(), reason);
            QCOMPARE(control->accessibleDescription(), reason);
        }
        auto* const combo = named<QComboBox>(&b.page, QStringLiteral("hotKeyActsOn:") + id);
        QVERIFY(!combo->isEnabled());
        QCOMPARE(combo->toolTip(), reason);
        QVERIFY(!(b.table->item(rowOf(id), 1)->flags() & Qt::ItemIsEnabled));
        QCOMPARE(b.table->item(rowOf(id), 1)->toolTip(), reason);
        QCOMPARE(keyText(&b.page, id), KeyboardMapWidget::displayText(QStringLiteral("U, Num+*")));

        // The map can't reach round it.
        b.page.show();
        keyCap(&b.page, QStringLiteral("U"))->click();
        auto* const change = named<QPushButton>(&b.page, QStringLiteral("hotKeyPickChange"));
        auto* const clear = named<QPushButton>(&b.page, QStringLiteral("hotKeyPickClear"));
        QVERIFY(change->isVisible() && !change->isEnabled());
        QCOMPARE(change->toolTip(), reason);
        QVERIFY(clear->isVisible() && !clear->isEnabled());
        keyCap(&b.page, QStringLiteral("Q"))->click();
        auto* const assign = named<QComboBox>(&b.page, QStringLiteral("hotKeyPickAssign"));
        auto* const model = qobject_cast<QStandardItemModel*>(assign->model());
        QVERIFY(model != nullptr);
        const int item = assign->findData(id);
        QVERIFY(!model->item(item)->isEnabled());
        QCOMPARE(model->item(item)->toolTip(), reason);
        QVERIFY(model->item(assign->findData(QStringLiteral("slice.lock")))->isEnabled());

        unavailable = false;
        b.page.refreshAvailability();
        QVERIFY(button(&b.page, QStringLiteral("hotKeySet:"), id)->isEnabled());
        QVERIFY(combo->isEnabled());
        QVERIFY(model->item(item)->isEnabled());
    }

    void aPageWithNoWindowIsDisabledAndSaysSo()
    {
        KeyboardPage page(nullptr);
        auto* const useShortcuts = named<QCheckBox>(&page, QStringLiteral("hotKeyUseShortcuts"));
        QVERIFY(!useShortcuts->isEnabled());
        QVERIFY(!useShortcuts->toolTip().isEmpty());
        auto* const set = button(&page, QStringLiteral("hotKeySet:"), QStringLiteral("tune.up"));
        QVERIFY(!set->isEnabled());
        QVERIFY(!set->toolTip().isEmpty());
        QVERIFY(!page.importFromFile(QStringLiteral("nothing")));
        QVERIFY(!page.exportToFile(QStringLiteral("nothing")));
    }

    // ---- Files ----

    void importAppliesGoodLinesAndListsTheOthers()
    {
        PageBench b;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("shortcuts.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# NereusSDR shortcuts 1\n"
                   "tune.up\tF5\t\n"
                   "no.such.action\tF6\t\n"
                   "cw.stop\tQ\t\n");
        file.close();

        QVERIFY(b.page.importFromFile(path));
        QCOMPARE(b.bindings.keysFor(QStringLiteral("tune.up")), QStringList{QStringLiteral("F5")});
        QCOMPARE(keyText(&b.page, QStringLiteral("tune.up")), QStringLiteral("F5"));
        QCOMPARE(b.bindings.keysFor(QStringLiteral("cw.stop")), QStringList{QStringLiteral("Ctrl+F10")});

        auto* const skipped = named<QPlainTextEdit>(&b.page, QStringLiteral("hotKeySkippedLines"));
        QVERIFY(!skipped->isHidden());
        const QStringList lines = skipped->toPlainText().split(QLatin1Char('\n'));
        QCOMPARE(lines.size(), 2);
        QVERIFY2(lines.at(0).contains(QStringLiteral("no.such.action")), qPrintable(lines.at(0)));
        QVERIFY2(lines.at(1).contains(QStringLiteral("stop the message")), qPrintable(lines.at(1)));
        QCOMPARE(named<QLabel>(&b.page, QStringLiteral("hotKeyStatus"))->text(),
                 QStringLiteral("Imported the good lines. 2 lines were skipped:"));

        // A clean import takes the old list away.
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("tune.up\tF7\t\n");
        file.close();
        QVERIFY(b.page.importFromFile(path));
        QVERIFY(skipped->isHidden());
        QCOMPARE(named<QLabel>(&b.page, QStringLiteral("hotKeyStatus"))->text(), QStringLiteral("Shortcuts imported."));
    }

    void anUnreadableImportChangesNothing()
    {
        PageBench b;
        QTemporaryDir directory;
        const QString before = b.bindings.exportText();
        const QString missing = directory.filePath(QStringLiteral("missing.txt"));
        QVERIFY(!b.page.importFromFile(missing));
        QVERIFY(named<QLabel>(&b.page, QStringLiteral("hotKeyStatus"))->text().startsWith(QStringLiteral("Could not read")));
        QCOMPARE(b.bindings.exportText(), before);

        const QString large = directory.filePath(QStringLiteral("large.txt"));
        QFile file(large);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(1024 * 1024 + 1, ' '));
        file.close();
        QVERIFY(!b.page.importFromFile(large));
        QCOMPARE(b.bindings.exportText(), before);
    }

    void exportWritesWhatImportReads()
    {
        PageBench b;
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("shortcuts.txt"));
        b.bindings.bind(QStringLiteral("tune.up"), QStringLiteral("F5"), false);
        b.bindings.setPin(QStringLiteral("tune.up"), QLatin1Char('B'));
        QVERIFY(b.page.exportToFile(path));
        QVERIFY(named<QLabel>(&b.page, QStringLiteral("hotKeyStatus"))->text().startsWith(QStringLiteral("Shortcuts saved")));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()), b.bindings.exportText());

        HotKeyBindings other;
        KeyboardPage page(&other);
        QVERIFY(page.importFromFile(path));
        QCOMPARE(other.keysFor(QStringLiteral("tune.up")), QStringList{QStringLiteral("F5")});
        QCOMPARE(other.pinFor(QStringLiteral("tune.up")), QLatin1Char('B'));

        QVERIFY(!b.page.exportToFile(directory.filePath(QStringLiteral("no/such/folder/out.txt"))));
        QVERIFY(named<QLabel>(&b.page, QStringLiteral("hotKeyStatus"))->text().startsWith(QStringLiteral("Could not write")));
    }

    // ---- The switch ----

    void theSwitchIsTheSetting()
    {
        PageBench b;
        auto* const useShortcuts = named<QCheckBox>(&b.page, QStringLiteral("hotKeyUseShortcuts"));
        auto* const mapGroup = named<QGroupBox>(&b.page, QStringLiteral("hotKeyMapGroup"));
        auto* const actions = named<QGroupBox>(&b.page, QStringLiteral("hotKeyActionsGroup"));
        QVERIFY(useShortcuts->isChecked());
        QVERIFY(mapGroup->isEnabled() && actions->isEnabled());

        b.beginSet(QStringLiteral("tune.up"));
        useShortcuts->click();
        QVERIFY(!b.bindings.enabled());
        QVERIFY(!mapGroup->isEnabled() && !actions->isEnabled());
        QCOMPARE(mapGroup->toolTip(), QStringLiteral("Use shortcuts is off."));
        // It stopped listening too.
        QCOMPARE(keyText(&b.page, QStringLiteral("tune.up")), QStringLiteral("Right"));

        b.bindings.setEnabled(true);
        QVERIFY(useShortcuts->isChecked());
        QVERIFY(mapGroup->isEnabled() && actions->isEnabled());
        QVERIFY(mapGroup->toolTip().isEmpty());
    }

    void theSwitchAndTheViewItemMoveTogetherAndStopKeys()
    {
        WindowBench w;
        QVERIFY2(w.ready(), qPrintable(NereusSDR::Test::activationState(&w.window)));
        QVERIFY(w.openPage());
        auto* const useShortcuts = named<QCheckBox>(w.page, QStringLiteral("hotKeyUseShortcuts"));
        QAction* const item = w.viewAction(QStringLiteral("Use &Shortcuts"));
        QVERIFY(item != nullptr);
        QVERIFY(item->isCheckable() && item->isChecked() && useShortcuts->isChecked());

        item->trigger();
        QVERIFY(!w.bindings->enabled());
        QVERIFY(!useShortcuts->isChecked());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Enabled")).toString(), QStringLiteral("False"));

        useShortcuts->click();
        QVERIFY(w.bindings->enabled());
        QVERIFY(item->isChecked());
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("HotKeys/Enabled")));

        // R-HK-02: off, Space in the window keys nothing.
        useShortcuts->click();
        w.dialog->close();
        w.window.activateWindow();
        QVERIFY2(waitActive(&w.window), qPrintable(NereusSDR::Test::activationState(&w.window)));
        tap(&w.window, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());
        item->trigger();
        down(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        up(&w.window, Qt::Key_Space);
        QTRY_VERIFY(!w.mox->isMox());
    }

    void turningOffFromThePageDropsAHoldAndLeavesMoxOn()
    {
        WindowBench w;
        QVERIFY2(w.ready(), qPrintable(NereusSDR::Test::activationState(&w.window)));
        QVERIFY(w.openPage());
        auto* const useShortcuts = named<QCheckBox>(w.page, QStringLiteral("hotKeyUseShortcuts"));
        w.window.activateWindow();
        QVERIFY2(waitActive(&w.window), qPrintable(NereusSDR::Test::activationState(&w.window)));

        down(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        useShortcuts->click();
        QTRY_VERIFY(!w.mox->isMox());
        up(&w.window, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());
        useShortcuts->click();

        // A latched MOX stays on, with Space held over it.
        tap(&w.window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(w.mox->isMox());
        down(&w.window, Qt::Key_Space);
        useShortcuts->click();
        QCoreApplication::processEvents();
        QVERIFY(w.mox->isMox());
        up(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
    }

    // ---- With the window ----

    void aKeyInSetGoesToSetAndKeysNothing()
    {
        WindowBench w;
        QVERIFY2(w.ready(), qPrintable(NereusSDR::Test::activationState(&w.window)));
        QVERIFY(w.openPage());
        w.dialog->activateWindow();
        QVERIFY2(waitActive(w.dialog), qPrintable(NereusSDR::Test::activationState(w.dialog)));

        // Space while listening: a conflict for this row, and no key on the radio.
        const QString id = QStringLiteral("tune.up");
        button(w.page, QStringLiteral("hotKeySet:"), id)->click();
        QVERIFY(QApplication::activeWindow() == w.dialog);
        down(w.dialog, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());
        QCOMPARE(messageOf(w.page, id), QStringLiteral("Space is already Hold to talk."));
        up(w.dialog, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());
        button(w.page, QStringLiteral("hotKeyDismiss:"), id)->click();

        // Its own key, while listening: still nothing on the radio.
        button(w.page, QStringLiteral("hotKeySet:"), QStringLiteral("tx.hold"))->click();
        down(w.dialog, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());
        up(w.dialog, Qt::Key_Space);
        QCOMPARE(w.bindings->keysFor(QStringLiteral("tx.hold")), QStringList{QStringLiteral("Space")});

        // And a transmit chord, to a focused control in the window that holds the list.
        button(w.page, QStringLiteral("hotKeySet:"), QStringLiteral("cw.msg1"))->click();
        auto* const search = named<QLineEdit>(w.page, QStringLiteral("hotKeySearch"));
        down(search, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(!w.mox->isMox());
        up(search, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(search->text(), QString());
        QVERIFY(!messageOf(w.page, QStringLiteral("cw.msg1")).isEmpty());

        // Not listening: Setup keeps its keys, as every other dialog does.
        down(w.dialog, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());
        up(w.dialog, Qt::Key_Space);
    }

    void changesAreSavedAtOnce()
    {
        WindowBench w;
        QVERIFY2(w.ready(), qPrintable(NereusSDR::Test::activationState(&w.window)));
        QVERIFY(w.openPage());
        const QString id = QStringLiteral("tune.up");
        button(w.page, QStringLiteral("hotKeySet:"), id)->click();
        down(w.dialog, Qt::Key_F5);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Binding/tune.up")).toString(), QStringLiteral("F5"));
        auto* const combo = named<QComboBox>(w.page, QStringLiteral("hotKeyActsOn:") + id);
        combo->setCurrentIndex(2);
        emit combo->activated(2);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Pin/tune.up")).toString(), QStringLiteral("B"));

        HotKeyBindings next;
        next.load();
        QCOMPARE(next.keysFor(id), QStringList{QStringLiteral("F5")});
        QCOMPARE(next.pinFor(id), QLatin1Char('B'));
    }

    void theMenuItemOpensThePageAndTheTreeHoldsIt()
    {
        WindowBench w;
        QVERIFY2(w.ready(), qPrintable(NereusSDR::Test::activationState(&w.window)));
        QAction* const item = w.viewAction(QStringLiteral("&Keyboard Shortcuts..."));
        QVERIFY(item != nullptr);
        QVERIFY(item->isEnabled());
        SetupDialog* opened = nullptr;
        const auto created = QObject::connect(&w.window, &MainWindow::setupDialogCreated,
                                              [&opened](SetupDialog* d) { opened = d; });
        item->trigger();
        QObject::disconnect(created);
        QVERIFY(opened != nullptr);
        auto* const page = opened->findChild<KeyboardPage*>();
        QVERIFY(page != nullptr);
        QVERIFY(page->isVisible());

        auto* const tree = opened->findChild<QTreeWidget*>();
        QStringList categories;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            categories.append(tree->topLevelItem(i)->text(0));
        }
        const qsizetype keys = categories.indexOf(QStringLiteral("Keys and controllers"));
        QVERIFY(keys > 0);
        QCOMPARE(categories.at(keys - 1), QStringLiteral("CAT & Network"));
        QCOMPARE(categories.at(keys + 1), QStringLiteral("Test"));
        QVERIFY(!categories.contains(QStringLiteral("Keyboard")));
        QTreeWidgetItem* const category = tree->topLevelItem(int(keys));
        QCOMPARE(category->child(0)->text(0), QStringLiteral("Keyboard"));
        QCOMPARE(tree->currentItem(), category->child(0));
        QVERIFY(!opened->pageLabelsForTest().contains(QStringLiteral("Shortcuts")));
    }

    void theWindowsReasonsReachTheRows()
    {
        WindowBench w;
        QVERIFY2(w.ready(), qPrintable(NereusSDR::Test::activationState(&w.window)));
        QVERIFY(w.openPage());
        MainWindowHotKeyTargets targets(&w.window);
        // Voice is not built.
        for (const char* id : {"voice.msg1", "voice.stop"}) {
            const QString reason = targets.unavailableReason(QLatin1String(id));
            QVERIFY2(!reason.isEmpty(), id);
            auto* const set = button(w.page, QStringLiteral("hotKeySet:"), QLatin1String(id));
            QVERIFY2(!set->isEnabled(), id);
            QCOMPARE(set->toolTip(), reason);
            auto* const table = w.page->findChild<QTableWidget*>(QStringLiteral("hotKeyList"));
            QVERIFY2(!table->isRowHidden(rowOf(QLatin1String(id))), id);
        }
        // On a local window, dot and dash are fine. The window has band stacking,
        // so its keys can be set.
        for (const char* id : {"cw.dot", "cw.dash", "tune.up", "tx.hold", "bandstack.next", "bandstack.panel"}) {
            QVERIFY2(button(w.page, QStringLiteral("hotKeySet:"), QLatin1String(id))->isEnabled(), id);
        }
    }

    void itWorksOnARemoteWindowAndDotAndDashSayWhenTheCoreIsOld()
    {
        RemoteWindowHarness::Options options;
        options.sliceAccess = true;
        RemoteWindowHarness h(options);
        QVERIFY(connectRemote(h, DSPMode::USB));
        QTRY_VERIFY(h.client()->remoteCwPaddleAvailable());

        SetupDialog* dialog = nullptr;
        const auto created = QObject::connect(h.window(), &MainWindow::setupDialogCreated,
                                              [&dialog](SetupDialog* d) { dialog = d; });
        QMetaObject::invokeMethod(h.window(), "openSetupAtPage", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("Keyboard")));
        QObject::disconnect(created);
        QVERIFY(dialog != nullptr);
        auto* const page = dialog->findChild<KeyboardPage*>();
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());
        QVERIFY(page->isVisible());

        // Local to this computer: a key binds and nothing goes to the Core.
        button(page, QStringLiteral("hotKeySet:"), QStringLiteral("tune.up"))->click();
        down(dialog, Qt::Key_F5);
        QCOMPARE(h.window()->hotKeyBindings()->keysFor(QStringLiteral("tune.up")), QStringList{QStringLiteral("F5")});
        QVERIFY(!h.stationSettings().contains(QStringLiteral("HotKeys/Binding/tune.up")));

        auto* const dot = button(page, QStringLiteral("hotKeySet:"), QStringLiteral("cw.dot"));
        auto* const dash = button(page, QStringLiteral("hotKeySet:"), QStringLiteral("cw.dash"));
        auto* const straight = button(page, QStringLiteral("hotKeySet:"), QStringLiteral("cw.straight"));
        QVERIFY(dot->isEnabled() && dash->isEnabled());

        StationCapabilities older = h.server().buildCapabilities();
        older.txPermitted = true;
        older.remoteCwVersion = 2;
        h.pushCapabilities(older);
        QTRY_VERIFY(!h.client()->remoteCwPaddleAvailable());
        // In USB too: a row says what the Core can do, whatever the mode is now.
        const QString reason = QStringLiteral("Dot and dash need a newer Core.");
        QTRY_VERIFY(!dot->isEnabled());
        QCOMPARE(dot->toolTip(), reason);
        QVERIFY(!dash->isEnabled());
        QCOMPARE(dash->toolTip(), reason);
        QVERIFY(straight->isEnabled());

        StationCapabilities newer = h.server().buildCapabilities();
        newer.txPermitted = true;
        h.pushCapabilities(newer);
        QTRY_VERIFY(h.client()->remoteCwPaddleAvailable());
        QTRY_VERIFY(dot->isEnabled());
    }
};

QTEST_MAIN(TestHotKeySetupKeyboard)
#include "tst_hot_key_setup_keyboard.moc"
