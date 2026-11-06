// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 7.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// The keyboard router on a real offscreen window: which keys act where
// (design section 6), repeats, releases, failsafes and the off switch.
//
// Keys go in as real QKeyEvents through QApplication's notify, which is the path
// QTest::sendKeyEvent takes without its "not accepted" log line. Two rigs run:
// the window's own router, read through the slice and MOX, and a second router
// on a recording dispatcher. The second one is installed later, so Qt asks it
// first, and what it passes on still reaches the window's own router.
#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QWindow>

#include <QMenu>
#include <QMenuBar>

#include <cmath>
#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyChord.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/AmpViewWindow.h"
#include "gui/MainWindow.h"
#include "gui/PanFloatingWindow.h"
#include "gui/SetupDialog.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/CwxApplet.h"
#include "gui/containers/ArrangeHoldFilter.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/FreeCanvasSurface.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/KeyboardRouter.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

constexpr int kStepHz = 100;
constexpr double kStartHz = 14'200'000.0;

void sendKey(QWidget* to, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
             bool autoRepeat = false, quint32 nativeKey = 0)
{
    const QString text = key >= 0x20 && key < 0x7f && !modifiers.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier)
        ? QString(QChar(key)).toLower()
        : QString();
    QKeyEvent event(type, key, modifiers, 0, nativeKey, 0, text, autoRepeat);
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

// Where Qt would deliver a key right now.
QWidget* keyTarget()
{
    if (QWidget* const popup = QApplication::activePopupWidget()) {
        return popup->focusWidget() != nullptr ? popup->focusWidget() : popup;
    }
    QWidget* const focus = QApplication::focusWidget();
    return focus != nullptr ? focus : QApplication::activeWindow();
}

// QWindow::isActive is already true for a dialog over an active parent, so
// qWaitForWindowActive can't say the dialog has the keyboard. This can.
bool waitActive(QWidget* window)
{
    return QTest::qWaitFor([window] { return QApplication::activeWindow() == window; }, 5000);
}

// Counts the key presses a widget actually receives, after the application filters.
class KeySeen : public QObject {
public:
    explicit KeySeen(QWidget* widget)
    {
        widget->installEventFilter(this);
    }
    int presses{0};

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            ++presses;
        }
        return false;
    }
};

// Records what the dispatcher would run, in order, as action ids.
class RecordingTargets : public HotKeyTargets {
public:
    SliceModel* slice{nullptr};
    QStringList ran;
    QList<bool> keyHolds;
    QList<QPair<QString, bool>> cwKeys;
    mutable qint64 clock{1000};
    bool clockRuns{true};
    std::function<void()> onKeyHold;

    SliceModel* windowSlice() const override { return slice; }
    SliceModel* sliceByLetter(QChar) const override { return slice; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel* target, double hz) override
    {
        const int steps = int(std::lround((hz - target->frequency()) / target->stepHz()));
        ran.append(steps == 1 ? QStringLiteral("tune.up")
                   : steps == -1 ? QStringLiteral("tune.down")
                   : steps == 10 ? QStringLiteral("tune.up10")
                   : steps == -10 ? QStringLiteral("tune.down10")
                                  : QStringLiteral("tune %1").arg(steps));
        target->setFrequency(hz);
        return true;
    }
    void runSliceAction(const QString& id, SliceModel*, int) override { ran.append(id); }
    void runWindowAction(const QString& id) override { ran.append(id); }
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return false; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool isDown) override
    {
        keyHolds.append(isDown);
        if (isDown) {
            ran.append(QStringLiteral("tx.hold"));
            if (onKeyHold) {
                onKeyHold();
            }
        }
        return true;
    }
    void runTransmitAction(const QString& id) override { ran.append(id); }
    void runCwMessage(const QString& id) override { ran.append(id); }
    void setCwKeyDown(const QString& id, bool isDown) override
    {
        cwKeys.append({id, isDown});
        if (isDown) {
            ran.append(id);
        }
    }
    void toast(const QString&) override {}
    qint64 nowMs() const override
    {
        // Past the 20 ms limit on every look, unless a test holds the clock.
        if (clockRuns) {
            clock += 100;
        }
        return clock;
    }
};

// One real window, shown offscreen, connected, with Slice A on pan-0.
struct Bench {
    MainWindow window{RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred};
    RadioModel* model{window.radioModel()};
    MoxController* mox{model->moxController()};
    HotKeyDispatcher* keys{window.hotKeyDispatcher()};
    HotKeyBindings* bindings{window.hotKeyBindings()};
    SliceModel* slice{nullptr};

    Bench()
    {
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model->transmitModel().setMicSourceLocked(false);
        model->transmitModel().setMicSource(MicSource::Radio);
        slice = model->sliceById(model->addSlice(QStringLiteral("pan-0")));
        slice->setDspMode(DSPMode::USB);
        slice->setStepHz(kStepHz);
        slice->setFrequency(kStartHz);
        window.resize(1200, 800);
        window.show();
        window.activateWindow();
    }

    bool ready()
    {
        if (!waitActive(&window)) {
            return false;
        }
        if (QWidget* const focus = QApplication::focusWidget()) {
            focus->clearFocus();
        }
        return QApplication::focusWidget() == nullptr;
    }

    bool reactivate()
    {
        window.activateWindow();
        return waitActive(&window);
    }

    QPair<int, int> filter() const { return {slice->filterLow(), slice->filterHigh()}; }

    // Lets anything queued go out, so "did not tune" can't pass by being early.
    void settleTunes()
    {
        keys->flushTune();
        QTest::qWait(25);
        keys->flushTune();
    }
};

// A second router, on the same window, recording instead of running.
struct Rig {
    HotKeyBindings bindings;
    RecordingTargets targets;
    HotKeyDispatcher dispatcher{&bindings, &targets};
    SliceModel slice{0};
    KeyboardRouter router;

    explicit Rig(MainWindow* window)
        : router(&dispatcher, &bindings, window)
    {
        targets.slice = &slice;
        slice.setStepHz(kStepHz);
        slice.setFrequency(kStartHz);
    }
};

// Enough of a station for a Roundtable room to be live.
class RoundtableStubLink : public IStationLink {
public:
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool roundtableAvailable() const override { return true; }
};

struct ChordKey {
    int key;
    Qt::KeyboardModifiers modifiers;
};

ChordKey keyOf(const QString& chord)
{
    const QKeySequence sequence = QKeySequence::fromString(chord, QKeySequence::PortableText);
    const QKeyCombination combination = sequence[0];
    return {int(combination.key()), combination.keyboardModifiers()};
}

QStringList transmitChords()
{
    QStringList chords;
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        if (action.transmits || action.group == QLatin1String("CW") || action.group == QLatin1String("Voice")) {
            chords.append(action.defaultKeys);
        }
    }
    return chords;
}

QString defaultHolderOf(const QString& chord)
{
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        if (action.defaultKeys.contains(chord)) {
            return action.id;
        }
    }
    return {};
}

} // namespace

class TestHotKeyRouter : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("hot-key-router")));
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
    }

    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    // ---- Chords ----

    void chordsAreCanonical_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
        QTest::addColumn<QString>("chord");
        const auto row = [](const char* name, int key, Qt::KeyboardModifiers modifiers, const char* chord) {
            QTest::newRow(name) << key << modifiers << QString::fromLatin1(chord);
        };
        row("mac right arrow", Qt::Key_Right, Qt::KeypadModifier, "Right");
        row("mac shift left arrow", Qt::Key_Left, Qt::KeypadModifier | Qt::ShiftModifier, "Shift+Left");
        row("mac up arrow", Qt::Key_Up, Qt::KeypadModifier, "Up");
        row("mac down arrow", Qt::Key_Down, Qt::KeypadModifier, "Down");
        row("keypad plus", Qt::Key_Plus, Qt::KeypadModifier, "Num++");
        row("keypad minus", Qt::Key_Minus, Qt::KeypadModifier, "Num+-");
        row("keypad star", Qt::Key_Asterisk, Qt::KeypadModifier, "Num+*");
        row("ctrl shift t", Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier, "Ctrl+Shift+T");
        row("space", Qt::Key_Space, Qt::NoModifier, "Space");
        row("letter", Qt::Key_B, Qt::NoModifier, "B");
        row("comma", Qt::Key_Comma, Qt::NoModifier, ",");
        row("bracket", Qt::Key_BracketLeft, Qt::NoModifier, "[");
        row("backquote", Qt::Key_QuoteLeft, Qt::NoModifier, "`");
        row("function key", Qt::Key_F10, Qt::ControlModifier | Qt::ShiftModifier, "Ctrl+Shift+F10");
        row("bare shift", Qt::Key_Shift, Qt::ShiftModifier, "");
        row("bare control", Qt::Key_Control, Qt::ControlModifier, "");
        row("bare alt", Qt::Key_Alt, Qt::AltModifier, "");
        row("bare meta", Qt::Key_Meta, Qt::MetaModifier, "");
        row("unknown key", Qt::Key_unknown, Qt::NoModifier, "");
        row("no portable name", Qt::Key_VolumeUp, Qt::NoModifier, "");
    }

    void chordsAreCanonical()
    {
        QFETCH(int, key);
        QFETCH(Qt::KeyboardModifiers, modifiers);
        QFETCH(QString, chord);
        const QKeyEvent event(QEvent::KeyPress, key, modifiers);
        QCOMPARE(KeyboardRouter::chordFromEvent(&event), chord);
    }

    void everyDefaultChordComesBackFromItsEvent()
    {
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            for (const QString& chord : action.defaultKeys) {
                const ChordKey key = keyOf(chord);
                const QKeyEvent event(QEvent::KeyPress, key.key, key.modifiers);
                QCOMPARE(KeyboardRouter::chordFromEvent(&event), chord);
            }
        }
    }

    // ---- Filter order ----

    void arrangeHoldSeesEachKeyBeforeTheRouter()
    {
        WorkspacePreview::setEnabledForTest(true);
        const auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });
        Bench w;
        QVERIFY(w.ready());
        auto* const filter = w.window.findChild<ArrangeHoldFilter*>(QStringLiteral("workspaceArrangeHold"));
        QVERIFY(filter != nullptr);
        ContainerManager* const manager = w.window.containerManager();
        QVERIFY(manager != nullptr);

        // Right inside the delay: the filter hears it and gives up on the hold.
        filter->setHoldDelayForTest(60000);
        down(&w.window, Qt::Key_Shift, Qt::ShiftModifier);
        QVERIFY(filter->holdPendingForTest());
        tap(&w.window, Qt::Key_Right, Qt::ShiftModifier);
        QVERIFY(!filter->holdPendingForTest());
        QVERIFY(!manager->arrangeHeld());
        QTRY_COMPARE(w.slice->frequency(), kStartHz + 10 * kStepHz);
        up(&w.window, Qt::Key_Shift);
        w.settleTunes();

        // Past the delay, with no handle focused: still ten steps.
        filter->setHoldDelayForTest(1);
        down(&w.window, Qt::Key_Shift, Qt::ShiftModifier);
        QTRY_VERIFY(manager->arrangeHeld());
        tap(&w.window, Qt::Key_Right, Qt::ShiftModifier);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + 20 * kStepHz);
        QVERIFY(manager->arrangeHeld());
        up(&w.window, Qt::Key_Shift);
        QTRY_VERIFY(!manager->arrangeHeld());
    }

    // ---- Who wins inside the window ----

    void textWidgetsKeepEveryKey_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("text box") << 0;
        QTest::newRow("multi-line text") << 1;
        QTest::newRow("plain multi-line text") << 2;
        QTest::newRow("number box") << 3;
        QTest::newRow("decimal number box") << 4;
        QTest::newRow("editable dropdown") << 5;
    }

    void textWidgetsKeepEveryKey()
    {
        QFETCH(int, kind);
        Bench w;
        QVERIFY(w.ready());
        QWidget* widget = nullptr;
        switch (kind) {
        case 0: widget = new QLineEdit(&w.window); break;
        case 1: widget = new QTextEdit(&w.window); break;
        case 2: widget = new QPlainTextEdit(&w.window); break;
        case 3: widget = new QSpinBox(&w.window); break;
        case 4: widget = new QDoubleSpinBox(&w.window); break;
        default: {
            auto* const combo = new QComboBox(&w.window);
            combo->setEditable(true);
            widget = combo;
            break;
        }
        }
        widget->show();
        // A clicked text box: the reason makes no difference to it.
        widget->setFocus(Qt::MouseFocusReason);
        QWidget* const target = QApplication::focusWidget();
        QVERIFY(target != nullptr);
        KeySeen seen(target);
        const QPair<int, int> filter = w.filter();

        tap(target, Qt::Key_Right);
        QCOMPARE(seen.presses, 1);
        tap(target, Qt::Key_Space);
        QCOMPARE(seen.presses, 2);
        tap(target, Qt::Key_B);
        QCOMPARE(seen.presses, 3);

        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);
        QCOMPARE(w.filter(), filter);
        QVERIFY(!w.mox->isMox());
    }

    void anOpenDropdownOrMenuKeepsEveryKeyUntilItCloses()
    {
        Bench w;
        QVERIFY(w.ready());
        const auto keepsKeys = [&w] {
            QVERIFY(QApplication::activePopupWidget() != nullptr);
            const QPair<int, int> filter = w.filter();
            for (const int key : {int(Qt::Key_Right), int(Qt::Key_Space), int(Qt::Key_B)}) {
                tap(keyTarget(), key);
                QVERIFY(QApplication::activePopupWidget() != nullptr);
            }
            w.settleTunes();
            QCOMPARE(w.slice->frequency(), kStartHz);
            QCOMPARE(w.filter(), filter);
            QVERIFY(!w.mox->isMox());
        };

        auto* const combo = new QComboBox(&w.window);
        combo->addItems({QStringLiteral("one"), QStringLiteral("two")});
        combo->show();
        combo->setFocus(Qt::MouseFocusReason);
        combo->showPopup();
        keepsKeys();
        if (QTest::currentTestFailed()) {
            return;
        }
        combo->hidePopup();
        QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
        // The offscreen plugin does not hand activation back when a popup closes.
        QVERIFY(w.reactivate());
        QPair<int, int> before = w.filter();
        tap(keyTarget(), Qt::Key_B);
        QVERIFY(w.filter() != before);

        QMenu menu(&w.window);
        menu.addAction(QStringLiteral("one"));
        menu.popup(w.window.mapToGlobal(QPoint(50, 50)));
        QTRY_VERIFY(QApplication::activePopupWidget() == &menu);
        const QPair<int, int> filterAtMenu = w.filter();
        for (const int key : {int(Qt::Key_Right), int(Qt::Key_Space), int(Qt::Key_B)}) {
            tap(keyTarget(), key);
            if (QApplication::activePopupWidget() == nullptr) {
                break;  // the menu took the key for itself and closed
            }
        }
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);
        QCOMPARE(w.filter(), filterAtMenu);
        QVERIFY(!w.mox->isMox());
        menu.close();
        QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
        QVERIFY(w.reactivate());
        before = w.filter();
        tap(keyTarget(), Qt::Key_B);
        QVERIFY(w.filter() != before);
    }

    void arrangeModeKeepsTheArrowsOnly()
    {
        Bench w;
        QVERIFY(w.ready());
        ContainerManager* const manager = w.window.containerManager();
        manager->setArrangeMode(true);
        QVERIFY(manager->arrangeMode());
        const QPair<int, int> filter = w.filter();

        for (const int key : {int(Qt::Key_Left), int(Qt::Key_Right)}) {
            tap(&w.window, key);
            tap(&w.window, key, Qt::ShiftModifier);
        }
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);

        tap(&w.window, Qt::Key_B);
        QVERIFY(w.filter() != filter);

        manager->setArrangeMode(false);
        tap(&w.window, Qt::Key_Right);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + kStepHz);
    }

    void aFocusedHandleKeepsTheArrowsDuringAHold()
    {
        Bench w;
        QVERIFY(w.ready());
        ContainerManager* const manager = w.window.containerManager();
        auto* const surface = new FreeCanvasSurface(&w.window);
        // Stands in for the file-local CanvasHandle: a focusable child of the surface.
        auto* const handle = new QWidget(surface);
        handle->setFocusPolicy(Qt::StrongFocus);
        surface->show();
        handle->show();
        handle->setFocus(Qt::MouseFocusReason);
        QCOMPARE(QApplication::focusWidget(), handle);
        KeySeen seen(handle);

        manager->setArrangeHeld(true);
        tap(handle, Qt::Key_Right);
        QCOMPARE(seen.presses, 1);
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);

        manager->setArrangeHeld(false);
        tap(handle, Qt::Key_Right);
        QCOMPARE(seen.presses, 1);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + kStepHz);
    }

    void aControlReachedWithTabKeepsArrowsAndSpace_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<Qt::FocusReason>("reason");
        for (const auto& [name, kind] : {std::pair<const char*, int>{"slider", 0}, {"button", 1},
                                         {"tab bar", 2}, {"closed dropdown", 3}}) {
            QTest::addRow("%s, tab", name) << kind << Qt::TabFocusReason;
            QTest::addRow("%s, shift tab", name) << kind << Qt::BacktabFocusReason;
        }
    }

    void aControlReachedWithTabKeepsArrowsAndSpace()
    {
        QFETCH(int, kind);
        QFETCH(Qt::FocusReason, reason);
        Bench w;
        QVERIFY(w.ready());
        QSlider* slider = nullptr;
        QWidget* const control = makeControl(kind, &w.window, slider);
        auto* const elsewhere = new QPushButton(QStringLiteral("elsewhere"), &w.window);
        elsewhere->show();
        KeySeen seen(control);

        control->setFocus(reason);
        QCOMPARE(QApplication::focusWidget(), control);

        tap(control, Qt::Key_Right);
        QCOMPARE(seen.presses, 1);
        if (slider != nullptr) {
            QCOMPARE(slider->value(), 51);
        }
        // A button's arrow moved focus to the next button, its own use of the key.
        control->setFocus(reason);
        tap(control, Qt::Key_Space);
        QCOMPARE(seen.presses, 2);
        QVERIFY(!w.mox->isMox());
        // Space opens a dropdown, its own use of the key.
        if (auto* const combo = qobject_cast<QComboBox*>(control)) {
            QTRY_VERIFY(QApplication::activePopupWidget() != nullptr);
            combo->hidePopup();
            QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
            QVERIFY(w.reactivate());
            QTRY_COMPARE(QApplication::focusWidget(), control);
        }
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);

        // Letters still act.
        const QPair<int, int> filter = w.filter();
        tap(control, Qt::Key_B);
        QCOMPARE(seen.presses, 2);
        QVERIFY(w.filter() != filter);

        // Focus moves on: the arrows tune again, even coming back by mouse.
        elsewhere->setFocus(Qt::MouseFocusReason);
        tap(elsewhere, Qt::Key_Right);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + kStepHz);
        control->setFocus(Qt::MouseFocusReason);
        tap(control, Qt::Key_Right);
        QCOMPARE(seen.presses, 2);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + 2 * kStepHz);
    }

    void aTabbedControlStaysSoAcrossAWindowSwitch()
    {
        Bench w;
        QVERIFY(w.ready());
        auto* const slider = new QSlider(Qt::Horizontal, &w.window);
        slider->setRange(0, 100);
        slider->setValue(50);
        slider->show();
        slider->setFocus(Qt::TabFocusReason);

        QWidget other;
        other.show();
        other.activateWindow();
        QVERIFY(waitActive(&other));
        other.close();
        QVERIFY(w.reactivate());
        QTRY_COMPARE(QApplication::focusWidget(), slider);

        tap(slider, Qt::Key_Right);
        QCOMPARE(slider->value(), 51);
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);
    }

    void aClickedControlKeepsNothing_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("slider") << 0;
        QTest::newRow("button") << 1;
        QTest::newRow("tab bar") << 2;
        QTest::newRow("closed dropdown") << 3;
    }

    void aClickedControlKeepsNothing()
    {
        QFETCH(int, kind);
        Bench w;
        QVERIFY(w.ready());
        QSlider* slider = nullptr;
        QWidget* const control = makeControl(kind, &w.window, slider);
        KeySeen seen(control);
        control->setFocus(Qt::MouseFocusReason);
        QCOMPARE(QApplication::focusWidget(), control);

        tap(control, Qt::Key_Right);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + kStepHz);
        if (slider != nullptr) {
            QCOMPARE(slider->value(), 50);
        }
        down(control, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        up(control, Qt::Key_Space);
        QTRY_VERIFY(!w.mox->isMox());
        QCOMPARE(seen.presses, 0);
    }

    // ---- Windows ----

    void mainAndFloatingWindowsAct()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);

        tap(&w.window, Qt::Key_B);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("filter.next")});
        rig.targets.ran.clear();

        PanFloatingWindow pan(&w.window);
        pan.show();
        pan.activateWindow();
        QVERIFY(waitActive(&pan));
        tap(&pan, Qt::Key_B);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("filter.next")});
        rig.targets.ran.clear();
        pan.hide();

        FloatingContainer container(0);
        container.show();
        container.activateWindow();
        QVERIFY(waitActive(&container));
        tap(&container, Qt::Key_B);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("filter.next")});
        container.hide();
    }

    void otherWindowsKeepTheirKeys_data()
    {
        QTest::addColumn<int>("kind");
        QTest::newRow("Setup") << 0;
        QTest::newRow("a dialog") << 1;
        QTest::newRow("Roundtable") << 2;
        QTest::newRow("amplifier chart") << 3;
        QTest::newRow("any other window") << 4;
    }

    void otherWindowsKeepTheirKeys()
    {
        QFETCH(int, kind);
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        RoundtableModel roundtable;
        std::unique_ptr<QWidget> other;
        switch (kind) {
        case 0: other = std::make_unique<SetupDialog>(w.model, &w.window); break;
        case 1: other = std::make_unique<QDialog>(&w.window); break;
        case 2: other = std::make_unique<RoundtableWindow>(&roundtable, &w.window); break;
        case 3: other = std::make_unique<AmpViewWindow>(w.model, nullptr, &w.window); break;
        default: other = std::make_unique<QWidget>(); break;
        }
        // The kinds of control PR 698 took keys from, in each window.
        auto* const tree = new QTreeWidget(other.get());
        auto* const table = new QTableWidget(2, 2, other.get());
        auto* const tabs = new QTabBar(other.get());
        tabs->addTab(QStringLiteral("one"));
        tabs->addTab(QStringLiteral("two"));
        auto* const slider = new QSlider(Qt::Horizontal, other.get());
        other->show();
        other->activateWindow();
        QVERIFY(waitActive(other.get()));

        QList<QWidget*> controls{other.get(), tree, table, tabs, slider};
        if (kind == 0) {
            controls.append(other->findChild<QTreeWidget*>(QString(), Qt::FindDirectChildrenOnly));
        }
        for (QWidget* const control : std::as_const(controls)) {
            if (control == nullptr) {
                continue;
            }
            control->setFocus(Qt::MouseFocusReason);
            for (const int key : {int(Qt::Key_Right), int(Qt::Key_Space), int(Qt::Key_B)}) {
                tap(control, key);
            }
        }
        QCOMPARE(rig.targets.ran, QStringList{});
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);
        QVERIFY(!w.mox->isMox());
    }

    void noKeyActsUnderAModalDialogOrWithNoActiveWindow()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);

        QDialog dialog(&w.window);
        dialog.setModal(true);
        dialog.show();
        QTRY_VERIFY(QApplication::activeModalWidget() == &dialog);
        for (QWidget* const to : {static_cast<QWidget*>(&dialog), static_cast<QWidget*>(&w.window)}) {
            tap(to, Qt::Key_B);
            tap(to, Qt::Key_Right);
        }
        QCOMPARE(rig.targets.ran, QStringList{});
        dialog.close();
        QTRY_VERIFY(QApplication::activeModalWidget() == nullptr);
        QVERIFY(w.reactivate());

        w.window.hide();
        QTRY_VERIFY(QApplication::activeWindow() == nullptr);
        tap(&w.window, Qt::Key_B);
        QCOMPARE(rig.targets.ran, QStringList{});
    }

    // ---- Focus: R-HK-05 ----

    void aSpectrumClickLeavesTheBoxesTheirFocusAndKeys_data()
    {
        QTest::addColumn<bool>("roundtableBox");
        QTest::newRow("CWX text box") << false;
        QTest::newRow("Roundtable message box") << true;
    }

    void aSpectrumClickLeavesTheBoxesTheirFocusAndKeys()
    {
        QFETCH(bool, roundtableBox);
        Bench w;
        QVERIFY(w.ready());
        auto* const spectrum = w.window.findChild<SpectrumWidget*>();
        QVERIFY(spectrum != nullptr);
        QVERIFY(spectrum->isVisible());

        RoundtableStubLink roundtableLink;
        RoundtableState roundtableState;
        RoundtableModel roundtable;
        const auto detach = qScopeGuard([&roundtable] { roundtable.detach(); });
        QWidget* holder = nullptr;
        QWidget* box = nullptr;
        if (roundtableBox) {
            // A live room on Slice A, posed as tst_roundtable_panel poses one.
            roundtable.attachStation(&roundtableLink, &roundtableState);
            roundtableState.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
            roundtableState.applyStationValue("sliceRooms", QStringLiteral("{\"0\":\"room-1\"}"));
            roundtable.applyRooms(QJsonObject{{QStringLiteral("rooms"),
                                               QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("room-1")},
                                                                      {QStringLiteral("hz"), 7185000},
                                                                      {QStringLiteral("members"), 5}}}}});
            auto* const panel = new RoundtablePanel(&roundtable, &w.window);
            panel->followSlice(0);
            holder = panel;
            box = holder->findChild<QPlainTextEdit*>(QStringLiteral("roundtableBox"));
            QVERIFY(box != nullptr);
        } else {
            holder = new CwxApplet(w.model, &w.window);
            box = holder->findChild<QTextEdit*>(QStringLiteral("cwxText"));
            QVERIFY(box != nullptr);
            box->setEnabled(true);
        }
        holder->resize(400, 400);
        holder->show();
        QVERIFY(box->isVisible());
        box->setFocus(Qt::MouseFocusReason);
        QCOMPARE(QApplication::focusWidget(), box);

        QTest::mouseClick(spectrum, Qt::LeftButton, Qt::NoModifier, spectrum->rect().center());
        QCoreApplication::processEvents();
        QCOMPARE(QApplication::focusWidget(), box);

        KeySeen seen(box);
        const double frequency = w.slice->frequency();
        const QPair<int, int> filter = w.filter();
        tap(QApplication::focusWidget(), Qt::Key_Right);
        tap(QApplication::focusWidget(), Qt::Key_B);
        QCOMPARE(seen.presses, 2);
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), frequency);
        QCOMPARE(w.filter(), filter);
        QCOMPARE(QApplication::focusWidget(), box);
    }

    // ---- Repeat ----

    void repeatAddsToTunesAndLevelsAndNothingElse()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        const auto repeatPress = [&w](int key) {
            sendKey(&w.window, QEvent::KeyPress, key, Qt::NoModifier, true);
        };

        down(&w.window, Qt::Key_B);
        repeatPress(Qt::Key_B);
        repeatPress(Qt::Key_B);
        up(&w.window, Qt::Key_B);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("filter.next")});
        rig.targets.ran.clear();

        down(&w.window, Qt::Key_Right);
        repeatPress(Qt::Key_Right);
        repeatPress(Qt::Key_Right);
        up(&w.window, Qt::Key_Right);
        QCOMPARE(rig.targets.ran, (QStringList{QStringLiteral("tune.up"), QStringLiteral("tune.up"),
                                               QStringLiteral("tune.up")}));
        rig.targets.ran.clear();

        down(&w.window, Qt::Key_Up);
        repeatPress(Qt::Key_Up);
        up(&w.window, Qt::Key_Up);
        QCOMPARE(rig.targets.ran, (QStringList{QStringLiteral("volume.up"), QStringLiteral("volume.up")}));
        rig.targets.ran.clear();

        down(&w.window, Qt::Key_Space);
        repeatPress(Qt::Key_Space);
        // Some platforms send a release with each repeat: it lets go of nothing.
        sendKey(&w.window, QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, true);
        repeatPress(Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, QList<bool>{true});
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("tx.hold")});
        up(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
    }

    void aRepeatArrivingAfterADropDoesNotKeyAgain()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        down(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, QList<bool>{true});
        QApplicationStateChangeEvent inactive(Qt::ApplicationInactive);
        QCoreApplication::sendEvent(qApp, &inactive);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));

        sendKey(&w.window, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, true);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
        up(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
    }

    void separateFastPressesGoThroughTheSameLimitWithNoStepLost()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        rig.targets.clockRuns = false;

        for (int i = 0; i < 8; ++i) {
            tap(&w.window, Qt::Key_Right);
        }
        // The first press goes out. The other seven wait out the 20 ms.
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("tune.up")});
        QCOMPARE(rig.slice.frequency(), kStartHz + kStepHz);
        rig.dispatcher.flushTune();
        QCOMPARE(rig.targets.ran.size(), 1);

        rig.targets.clock += TuneCoalescer::kMinIntervalMs;
        rig.dispatcher.flushTune();
        QCOMPARE(rig.targets.ran.size(), 2);
        QCOMPARE(rig.slice.frequency(), kStartHz + 8 * kStepHz);
    }

    // ---- Release ----

    void aReleaseWithTheModifierLiftedFirstStillReleases_data()
    {
        QTest::addColumn<QString>("chord");
        QTest::addColumn<int>("pressKey");
        QTest::addColumn<Qt::KeyboardModifiers>("pressModifiers");
        QTest::addColumn<int>("releaseKey");
        QTest::addColumn<quint32>("nativeKey");
        const Qt::KeyboardModifiers ctrlShift = Qt::ControlModifier | Qt::ShiftModifier;
        QTest::newRow("Ctrl+Shift+T, by key code")
            << QStringLiteral("Ctrl+Shift+T") << int(Qt::Key_T) << ctrlShift << int(Qt::Key_T) << quint32(0);
        QTest::newRow("Ctrl+Shift+T, by native key")
            << QStringLiteral("Ctrl+Shift+T") << int(Qt::Key_T) << ctrlShift << int(Qt::Key_T) << quint32(17);
        // Less going down, Comma coming up: only the native key pairs them.
        QTest::newRow("Shift+comma")
            << QStringLiteral("Shift+<") << int(Qt::Key_Less) << Qt::KeyboardModifiers(Qt::ShiftModifier)
            << int(Qt::Key_Comma) << quint32(43);
    }

    void aReleaseWithTheModifierLiftedFirstStillReleases()
    {
        QFETCH(QString, chord);
        QFETCH(int, pressKey);
        QFETCH(Qt::KeyboardModifiers, pressModifiers);
        QFETCH(int, releaseKey);
        QFETCH(quint32, nativeKey);
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);

        QCOMPARE(rig.bindings.bind(QStringLiteral("tx.hold"), chord, true).result, HotKeyBindResult::Bound);
        sendKey(&w.window, QEvent::KeyPress, pressKey, pressModifiers, false, nativeKey);
        QCOMPARE(rig.targets.keyHolds, QList<bool>{true});
        sendKey(&w.window, QEvent::KeyRelease, releaseKey, Qt::NoModifier, false, nativeKey);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));

        rig.bindings.resetToDefault(QStringLiteral("tx.hold"));
        QCOMPARE(rig.bindings.bind(QStringLiteral("cw.dot"), chord, true).result, HotKeyBindResult::Bound);
        sendKey(&w.window, QEvent::KeyPress, pressKey, pressModifiers, false, nativeKey);
        sendKey(&w.window, QEvent::KeyRelease, releaseKey, Qt::NoModifier, false, nativeKey);
        const QList<QPair<QString, bool>> expected{{QStringLiteral("cw.dot"), true}, {QStringLiteral("cw.dot"), false}};
        QCOMPARE(rig.targets.cwKeys, expected);
    }

    void aReleaseWorksAfterFocusMovedIntoATextBox()
    {
        Bench w;
        QVERIFY(w.ready());
        down(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        auto* const edit = new QLineEdit(&w.window);
        edit->show();
        edit->setFocus(Qt::MouseFocusReason);
        KeySeen seen(edit);
        sendKey(edit, QEvent::KeyRelease, Qt::Key_Space);
        QTRY_VERIFY(!w.mox->isMox());
        QCOMPARE(seen.presses, 0);
    }

    // ---- Failsafes: R-HK-08 ----

    void eachFailsafeDropsAHoldOnceAndLeavesMoxOn_data()
    {
        QTest::addColumn<int>("failsafe");
        QTest::newRow("active window changes") << 0;
        QTest::newRow("application goes inactive") << 1;
        QTest::newRow("a modal dialog opens") << 2;
    }

    void eachFailsafeDropsAHoldOnceAndLeavesMoxOn()
    {
        QFETCH(int, failsafe);
        Bench w;
        QVERIFY(w.ready());

        const auto runFailsafe = [failsafe, &w](auto&& whileItHolds) {
            QDialog dialog(&w.window);
            QWidget other;
            switch (failsafe) {
            case 0:
                other.show();
                other.activateWindow();
                QVERIFY(waitActive(&other));
                break;
            case 1: {
                QApplicationStateChangeEvent inactive(Qt::ApplicationInactive);
                QCoreApplication::sendEvent(qApp, &inactive);
                break;
            }
            default:
                dialog.setModal(true);
                dialog.show();
                QTRY_VERIFY(QApplication::activeModalWidget() == &dialog);
                break;
            }
            whileItHolds();
            other.close();
            dialog.close();
            QTRY_VERIFY(QApplication::activeModalWidget() == nullptr);
            QVERIFY(w.reactivate());
        };

        // The real window: Space keys, the failsafe unkeys.
        down(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        runFailsafe([&w] { QTRY_VERIFY(!w.mox->isMox()); });
        if (QTest::currentTestFailed()) {
            return;
        }
        // The release arrives late, wherever it goes: nothing changes.
        up(&w.window, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());

        // A latched MOX stays on, with Space held over it.
        tap(&w.window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(w.mox->isMox());
        down(&w.window, Qt::Key_Space);
        runFailsafe([&w] { QVERIFY(w.mox->isMox()); });
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY(w.mox->isMox());
        up(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        tap(&w.window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(!w.mox->isMox());

        // Once, through the recording rig, for Space and for a CW key.
        Rig rig(&w.window);
        down(&w.window, Qt::Key_Space);
        down(&w.window, Qt::Key_Comma);
        QCOMPARE(rig.targets.keyHolds, QList<bool>{true});
        runFailsafe([&rig] {
            QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
            const QList<QPair<QString, bool>> expected{{QStringLiteral("cw.dot"), true},
                                                       {QStringLiteral("cw.dot"), false}};
            QCOMPARE(rig.targets.cwKeys, expected);
        });
        if (QTest::currentTestFailed()) {
            return;
        }
        // Coming back and the late releases add nothing.
        up(&w.window, Qt::Key_Space);
        up(&w.window, Qt::Key_Comma);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
        QCOMPARE(rig.targets.cwKeys.size(), 2);
    }

    void aDialogOpenedByThePressItselfStillDropsTheHold()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        QDialog dialog(&w.window);
        dialog.setModal(true);
        rig.targets.onKeyHold = [&dialog] { dialog.show(); };

        down(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));

        // Not stuck: with the dialog gone, Space keys again.
        rig.targets.onKeyHold = nullptr;
        up(&dialog, Qt::Key_Space);
        dialog.close();
        QTRY_VERIFY(QApplication::activeModalWidget() == nullptr);
        QVERIFY(w.reactivate());
        down(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false, true}));
        up(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false, true, false}));
    }

    void aToastShowingDoesNotDropAHold()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        down(&w.window, Qt::Key_Space);

        // The offscreen plugin activates it, whatever it asks.
        auto* const toast = new StatusToast(QStringLiteral("A toast"), ToastSeverity::Info, 5000, &w.window);
        toast->show();
        QVERIFY(waitActive(toast));
        QCOMPARE(rig.targets.keyHolds, QList<bool>{true});

        up(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
    }

    // ---- A key already down (first-look fixes, items 1, 4, 5, 13) ----

    // Whatever later events on a key still down say, its hold ends with its
    // release, and nothing else runs.
    void aRepeatWithAChangedModifierKeepsTheHoldItsKeyStarted()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        QCOMPARE(rig.bindings.bind(QStringLiteral("tx.hold"), QStringLiteral("F9"), true).result, HotKeyBindResult::Bound);
        QCOMPARE(rig.bindings.bind(QStringLiteral("tx.mox"), QStringLiteral("Shift+F9"), true).result, HotKeyBindResult::Bound);
        constexpr quint32 kNative = 101;

        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F9, Qt::NoModifier, false, kNative);
        QCOMPARE(rig.targets.keyHolds, QList<bool>{true});
        // Shift goes down while F9 repeats.
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F9, Qt::ShiftModifier, true, kNative);
        // F9 comes up.
        sendKey(&w.window, QEvent::KeyRelease, Qt::Key_F9, Qt::ShiftModifier, false, kNative);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("tx.hold")});
    }

    // Windows can send a repeat with changed modifiers as a fresh press.
    void aFreshPressOnAKeyAlreadyDownRunsNothing()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        QCOMPARE(rig.bindings.bind(QStringLiteral("cw.straight"), QStringLiteral("F9"), true).result, HotKeyBindResult::Bound);
        QCOMPARE(rig.bindings.bind(QStringLiteral("tx.mox"), QStringLiteral("Shift+F9"), true).result, HotKeyBindResult::Bound);
        QCOMPARE(rig.bindings.bind(QStringLiteral("filter.next"), QStringLiteral("F10"), true).result, HotKeyBindResult::Bound);
        constexpr quint32 kNative = 101;

        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F9, Qt::NoModifier, false, kNative);
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F9, Qt::ShiftModifier, false, kNative);
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F9, Qt::NoModifier, false, kNative);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("cw.straight")});
        sendKey(&w.window, QEvent::KeyRelease, Qt::Key_F9, Qt::NoModifier, false, kNative);
        QCOMPARE(rig.targets.cwKeys, (QList<QPair<QString, bool>>{{QStringLiteral("cw.straight"), true},
                                                                  {QStringLiteral("cw.straight"), false}}));
        // Up means up: the next press is a press.
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F9, Qt::ShiftModifier, false, kNative);
        QCOMPARE(rig.targets.ran, (QStringList{QStringLiteral("cw.straight"), QStringLiteral("tx.mox")}));
        sendKey(&w.window, QEvent::KeyRelease, Qt::Key_F9, Qt::ShiftModifier, false, kNative);
        rig.targets.ran.clear();

        // A Press action is one press too.
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F10, Qt::NoModifier, false, 102);
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_F10, Qt::NoModifier, false, 102);
        sendKey(&w.window, QEvent::KeyRelease, Qt::Key_F10, Qt::NoModifier, false, 102);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("filter.next")});
    }

    // Holding Right and adding Shift goes on tuning, in tens. A tune never
    // turns into anything else.
    void aHeldTuneKeyMayChangeItsStepAndNothingElse()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        QCOMPARE(rig.bindings.bind(QStringLiteral("tx.mox"), QStringLiteral("Ctrl+Right"), true).result, HotKeyBindResult::Bound);
        constexpr quint32 kNative = 124;

        sendKey(&w.window, QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, false, kNative);
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier, true, kNative);
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_Right, Qt::ControlModifier, true, kNative);
        sendKey(&w.window, QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier, false, kNative);
        QCOMPARE(rig.targets.ran, (QStringList{QStringLiteral("tune.up"), QStringLiteral("tune.up10")}));
    }

    void onLinuxAPressAndItsReleasePairByTheScanCode()
    {
        // X11 and Wayland report a Shift-dependent key sym as the virtual key
        // and the same keycode as the scan code whatever the level.
        constexpr quint32 kCommaKeycode = 59;
        constexpr quint32 kSymLess = 0x3c;
        constexpr quint32 kSymComma = 0x2c;
        const qint64 pressed = KeyboardRouter::physicalKeyOf(kCommaKeycode, kSymLess, Qt::Key_Less, true);
        const qint64 released = KeyboardRouter::physicalKeyOf(kCommaKeycode, kSymComma, Qt::Key_Comma, true);
        QCOMPARE(pressed, released);
        // Another key is another key, and no scan code falls back in order.
        QVERIFY(KeyboardRouter::physicalKeyOf(60, kSymComma, Qt::Key_Comma, true) != released);
        QCOMPARE(KeyboardRouter::physicalKeyOf(0, kSymComma, Qt::Key_Comma, true),
                 KeyboardRouter::physicalKeyOf(0, kSymComma, Qt::Key_Less, true));
        QVERIFY(KeyboardRouter::physicalKeyOf(0, 0, Qt::Key_Comma, true)
                != KeyboardRouter::physicalKeyOf(0, 0, Qt::Key_Less, true));
        // A scan code and a virtual key of the same number are different keys.
        QVERIFY(KeyboardRouter::physicalKeyOf(kSymComma, 0, Qt::Key_A, true)
                != KeyboardRouter::physicalKeyOf(0, kSymComma, Qt::Key_A, true));
        // macOS and Windows go on pairing by the virtual key.
        QVERIFY(KeyboardRouter::physicalKeyOf(kCommaKeycode, kSymLess, Qt::Key_Less, false)
                != KeyboardRouter::physicalKeyOf(kCommaKeycode, kSymComma, Qt::Key_Comma, false));
        QCOMPARE(KeyboardRouter::physicalKeyOf(1, kSymComma, Qt::Key_Less, false),
                 KeyboardRouter::physicalKeyOf(2, kSymComma, Qt::Key_Comma, false));
    }

#ifdef Q_OS_LINUX
    void onLinuxLiftingShiftFirstStillReleases()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        QCOMPARE(rig.bindings.bind(QStringLiteral("cw.dot"), QStringLiteral("Shift+<"), true).result, HotKeyBindResult::Bound);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Less, Qt::ShiftModifier, 59, 0x3c, 0, QStringLiteral("<"));
        QCoreApplication::sendEvent(&w.window, &press);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Comma, Qt::NoModifier, 59, 0x2c, 0, QStringLiteral(","));
        QCoreApplication::sendEvent(&w.window, &release);
        QCOMPARE(rig.targets.cwKeys, (QList<QPair<QString, bool>>{{QStringLiteral("cw.dot"), true},
                                                                  {QStringLiteral("cw.dot"), false}}));
    }
#endif

    void sleepDropsAHoldAndTheKeyBehavesAfterWake()
    {
        const HotKeySource keyboard{HotKeySourceKind::Keyboard, QString()};
        Bench w;
        QVERIFY(w.ready());
        qint64 wall = 1000000;
        w.keys->setSleepClocksForTest([&wall] { return wall; }, [] { return qint64(5000); });
        const auto sleep = [&] {
            wall += 3600000;
            w.keys->checkForSleep();
        };

        // Let go during the sleep, with the release lost: the next press keys.
        down(&w.window, Qt::Key_Space);
        QTRY_VERIFY(w.mox->isMox());
        sleep();
        QTRY_VERIFY(!w.mox->isMox());
        down(&w.window, Qt::Key_Space);
        QTRY_VERIFY(w.mox->isMox());
        up(&w.window, Qt::Key_Space);
        QTRY_VERIFY(!w.mox->isMox());

        // Still down on wake: its repeats key nothing.
        down(&w.window, Qt::Key_Space);
        QTRY_VERIFY(w.mox->isMox());
        sleep();
        QTRY_VERIFY(!w.mox->isMox());
        sendKey(&w.window, QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, true);
        QCoreApplication::processEvents();
        QVERIFY(!w.mox->isMox());
        up(&w.window, Qt::Key_Space);

        // The real release after wake does nothing further, to a latched MOX either.
        down(&w.window, Qt::Key_Space);
        QTRY_VERIFY(w.mox->isMox());
        sleep();
        QTRY_VERIFY(!w.mox->isMox());
        w.keys->press(QStringLiteral("tx.mox"), keyboard);
        QTRY_VERIFY(w.mox->isMox());
        up(&w.window, Qt::Key_Space);
        QCoreApplication::processEvents();
        QVERIFY(w.mox->isMox());
        w.keys->press(QStringLiteral("tx.mox"), keyboard);
        QTRY_VERIFY(!w.mox->isMox());
    }

    void aMenuBarMenuOpeningDropsTheHoldAndLeavesMox()
    {
        const HotKeySource keyboard{HotKeySourceKind::Keyboard, QString()};
        Bench w;
        QVERIFY(w.ready());

        // A hold's key drops. No key-up has to arrive for it.
        // The title bar holds the window's menu bar, so ask for it by type.
        QMenuBar* bar = nullptr;
        QMenu* opened = nullptr;
        for (QMenuBar* const candidate : w.window.findChildren<QMenuBar*>()) {
            for (QAction* const action : candidate->actions()) {
                if (opened == nullptr && action->menu() != nullptr) {
                    bar = candidate;
                    opened = action->menu();
                }
            }
        }
        QVERIFY(opened != nullptr);
        down(&w.window, Qt::Key_Space);
        QTRY_VERIFY(w.mox->isMox());
        emit opened->aboutToShow();
        QTRY_VERIFY(!w.mox->isMox());
        up(&w.window, Qt::Key_Space);

        // A menu added later counts.
        QMenu* const later = bar->addMenu(QStringLiteral("Later"));
        down(&w.window, Qt::Key_Space);
        QTRY_VERIFY(w.mox->isMox());
        emit later->aboutToShow();
        QTRY_VERIFY(!w.mox->isMox());
        up(&w.window, Qt::Key_Space);

        // A latched MOX stays, with a hold down over it and a menu opening.
        w.keys->press(QStringLiteral("tx.mox"), keyboard);
        QTRY_VERIFY(w.mox->isMox());
        down(&w.window, Qt::Key_Space);
        emit opened->aboutToShow();
        up(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        w.keys->press(QStringLiteral("tx.mox"), keyboard);
        QTRY_VERIFY(!w.mox->isMox());
    }

    // Only a real status toast is excused. Asking not to activate does not
    // excuse a window that has taken the keyboard.
    void anotherWindowAskingNotToActivateStillDropsAHold()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        down(&w.window, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, QList<bool>{true});

        QWidget other(&w.window, Qt::FramelessWindowHint | Qt::Tool);
        other.setAttribute(Qt::WA_ShowWithoutActivating);
        other.show();
        QVERIFY(waitActive(&other));
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
        up(&w.window, Qt::Key_Space);
    }

    // ---- The off switch ----

    void offEatsNoKeyAndActsOnNothing()
    {
        Bench w;
        QVERIFY(w.ready());
        auto* const button = new QPushButton(QStringLiteral("button"), &w.window);
        button->show();
        button->setFocus(Qt::MouseFocusReason);
        KeySeen seen(button);
        QSignalSpy clicked(button, &QPushButton::clicked);

        AppSettings::instance().setValue(QStringLiteral("HotKeys/Enabled"), QStringLiteral("False"));
        w.bindings->load();
        QVERIFY(!w.bindings->enabled());

        const QPair<int, int> filter = w.filter();
        tap(button, Qt::Key_Space);
        QCOMPARE(seen.presses, 1);
        QCOMPARE(clicked.count(), 1);
        QVERIFY(!w.mox->isMox());
        tap(button, Qt::Key_B);
        tap(button, Qt::Key_Right);
        QCOMPARE(seen.presses, 3);
        w.settleTunes();
        QCOMPARE(w.slice->frequency(), kStartHz);
        QCOMPARE(w.filter(), filter);

        AppSettings::instance().setValue(QStringLiteral("HotKeys/Enabled"), QStringLiteral("True"));
        w.bindings->load();
        tap(button, Qt::Key_B);
        QCOMPARE(seen.presses, 3);
        QVERIFY(w.filter() != filter);
    }

    void goingOffWhileHeldForgetsTheKey()
    {
        Bench w;
        QVERIFY(w.ready());
        auto* const button = new QPushButton(QStringLiteral("button"), &w.window);
        button->show();
        button->setFocus(Qt::MouseFocusReason);
        down(button, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        w.bindings->setEnabled(false);
        QTRY_VERIFY(!w.mox->isMox());

        // The release is no longer the router's: it reaches the button.
        KeySeen seen(button);
        up(button, Qt::Key_Space);
        QVERIFY(!w.mox->isMox());
        w.bindings->setEnabled(true);
        down(button, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        up(button, Qt::Key_Space);
        QTRY_VERIFY(!w.mox->isMox());
    }

    // ---- R-HK-09 ----

    void noTransmitChordKeysFromATextFieldADialogOrAnInactiveWindow_data()
    {
        QTest::addColumn<QString>("chord");
        for (const QString& chord : transmitChords()) {
            QTest::newRow(qPrintable(chord)) << chord;
        }
    }

    void noTransmitChordKeysFromATextFieldADialogOrAnInactiveWindow()
    {
        QFETCH(QString, chord);
        const ChordKey key = keyOf(chord);
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        const auto nothingKeyed = [&](QWidget* to) {
            tap(to, key.key, key.modifiers);
            QCOMPARE(rig.targets.ran, QStringList{});
            QCOMPARE(rig.targets.keyHolds, QList<bool>{});
            QVERIFY(rig.targets.cwKeys.isEmpty());
            QVERIFY(!w.mox->isMox());
        };

        auto* const edit = new QLineEdit(&w.window);
        edit->show();
        edit->setFocus(Qt::MouseFocusReason);
        nothingKeyed(edit);
        if (QTest::currentTestFailed()) {
            return;
        }
        edit->clearFocus();

        {
            QDialog dialog(&w.window);
            dialog.show();
            dialog.activateWindow();
            QVERIFY(waitActive(&dialog));
            nothingKeyed(&dialog);
            if (QTest::currentTestFailed()) {
                return;
            }
        }
        QVERIFY(w.reactivate());

        QWidget other;
        other.show();
        other.activateWindow();
        QVERIFY(waitActive(&other));
        nothingKeyed(&w.window);
        nothingKeyed(&other);
        if (QTest::currentTestFailed()) {
            return;
        }
        other.close();
        QVERIFY(w.reactivate());

        // The control: the same chord from the clicked background reaches the rig.
        tap(&w.window, key.key, key.modifiers);
        QCOMPARE(rig.targets.ran.size(), 1);
    }

    // ---- R-HK-01 ----

    void everyDefaultChordRunsItsAction()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            for (const QString& chord : action.defaultKeys) {
                rig.targets.ran.clear();
                const ChordKey key = keyOf(chord);
                tap(&w.window, key.key, key.modifiers);
                QVERIFY2(rig.targets.ran == QStringList{action.id},
                         qPrintable(QStringLiteral("%1 ran [%2], wanted %3")
                                        .arg(chord, rig.targets.ran.join(QLatin1Char(' ')), action.id)));
            }
        }
    }

    void theWindowsOwnRouterRunsTheRealThing()
    {
        Bench w;
        QVERIFY(w.ready());

        tap(&w.window, Qt::Key_Right);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + kStepHz);
        tap(&w.window, Qt::Key_Left, Qt::KeypadModifier);
        QTRY_COMPARE(w.slice->frequency(), kStartHz);
        tap(&w.window, Qt::Key_Right, Qt::ShiftModifier);
        QTRY_COMPARE(w.slice->frequency(), kStartHz + 10 * kStepHz);

        const QPair<int, int> filter = w.filter();
        tap(&w.window, Qt::Key_B);
        QVERIFY(w.filter() != filter);

        down(&w.window, Qt::Key_Space);
        QVERIFY(w.mox->isMox());
        up(&w.window, Qt::Key_Space);
        QTRY_VERIFY(!w.mox->isMox());

        tap(&w.window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(w.mox->isMox());
        QVERIFY(w.mox->isManualKey());
        tap(&w.window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(!w.mox->isMox());
    }

    void aKeyComingThroughTheWindowRunsOnce()
    {
        Bench w;
        QVERIFY(w.ready());
        Rig rig(&w.window);
        QWindow* const handle = w.window.windowHandle();
        QVERIFY(handle != nullptr);
        const auto throughWindow = [handle](QEvent::Type type, int key) {
            QKeyEvent event(type, key, Qt::NoModifier);
            QCoreApplication::sendEvent(handle, &event);
        };

        // Qt's own route: the window, then the focused widget and its parents.
        auto* const button = new QPushButton(QStringLiteral("button"), &w.window);
        button->show();
        button->setFocus(Qt::MouseFocusReason);
        KeySeen seen(button);
        throughWindow(QEvent::KeyPress, Qt::Key_B);
        throughWindow(QEvent::KeyRelease, Qt::Key_B);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("filter.next")});
        QCOMPARE(seen.presses, 0);
        rig.targets.ran.clear();

        // A hold released through that route lets go.
        throughWindow(QEvent::KeyPress, Qt::Key_Space);
        throughWindow(QEvent::KeyRelease, Qt::Key_Space);
        QCOMPARE(rig.targets.keyHolds, (QList<bool>{true, false}));
        rig.targets.ran.clear();

        // A text box passes the key up its parents, and none of them runs it.
        auto* const edit = new QLineEdit(&w.window);
        edit->show();
        edit->setFocus(Qt::MouseFocusReason);
        throughWindow(QEvent::KeyPress, Qt::Key_Up);
        throughWindow(QEvent::KeyRelease, Qt::Key_Up);
        QCOMPARE(rig.targets.ran, QStringList{});

        // QTest's own sender reaches the same place.
        edit->clearFocus();
        QTest::sendKeyEvent(QTest::Click, &w.window, Qt::Key_B, QStringLiteral("b"), Qt::NoModifier);
        QCOMPARE(rig.targets.ran, QStringList{QStringLiteral("filter.next")});
    }

    // ---- Menus ----

    void menuShortcutsStillWork()
    {
        Bench w;
        QVERIFY(w.ready());
        const QKeySequence ctrlM(QStringLiteral("Ctrl+M"), QKeySequence::PortableText);
        QAction* minimal = nullptr;
        for (QAction* const action : w.window.findChildren<QAction*>()) {
            if (action->shortcuts().contains(ctrlM)) {
                minimal = action;
            }
        }
        QVERIFY(minimal != nullptr);
        QSignalSpy triggered(minimal, &QAction::triggered);

        // How a real press starts: Qt offers it to the shortcut map first.
        QTest::sendKeyEvent(QTest::Shortcut, &w.window, Qt::Key_M, QString(), Qt::ControlModifier);
#ifndef Q_OS_MAC
        QCOMPARE(triggered.count(), 1);
#else
        // On a Mac the native menu takes its shortcuts before any window hears the
        // key, and Qt's shortcut map skips menu items, so only hardware can show it.
        QCOMPARE(triggered.count(), 0);
#endif
        QVERIFY(!w.mox->isMox());

        // And the router lets the key itself by.
        KeySeen seen(&w.window);
        tap(&w.window, Qt::Key_M, Qt::ControlModifier);
        QCOMPARE(seen.presses, 1);
        QVERIFY(!w.mox->isMox());
    }

    void noShortcutSitsOnADefaultKeyUnlessReserved_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::newRow("classic look") << false;
        QTest::newRow("workspace look") << true;
    }

    void noShortcutSitsOnADefaultKeyUnlessReserved()
    {
        QFETCH(bool, workspaceLook);
        WorkspacePreview::setEnabledForTest(workspaceLook);
        const auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });
        Bench w;
        QVERIFY(w.ready());
        QStringList checked;
        const auto check = [&checked](const QKeySequence& sequence) {
            if (sequence.isEmpty()) {
                return;
            }
            const auto chord = HotKeyChord::parse(sequence.toString(QKeySequence::PortableText));
            if (!chord.has_value()) {
                return;
            }
            const QString text = chord->toString();
            checked.append(text);
            const QString holder = defaultHolderOf(text);
            QVERIFY2(holder.isEmpty() || !HotKeyBindings::reservedBy(text).isEmpty(),
                     qPrintable(QStringLiteral("%1 is a menu shortcut and the default key of %2, "
                                               "but HotKeyBindings::reservedBy does not list it.")
                                    .arg(text, holder)));
        };
        for (QAction* const action : w.window.findChildren<QAction*>()) {
            for (const QKeySequence& sequence : action->shortcuts()) {
                check(sequence);
            }
        }
        for (QShortcut* const shortcut : w.window.findChildren<QShortcut*>()) {
            check(shortcut->key());
        }
        // The walk found the menus.
        QVERIFY(checked.contains(QStringLiteral("Ctrl+M")));
        // The profile keys come with the workspace look's rail, where it finds one.
        if (workspaceLook) {
            QVERIFY(checked.contains(QStringLiteral("Ctrl+Shift+A")));
        }
    }

private:
    // Kinds: 0 slider at 50, 1 button, 2 tab bar, 3 closed dropdown.
    QWidget* makeControl(int kind, QWidget* parent, QSlider*& slider)
    {
        QWidget* control = nullptr;
        switch (kind) {
        case 0:
            slider = new QSlider(Qt::Horizontal, parent);
            slider->setRange(0, 100);
            slider->setValue(50);
            control = slider;
            break;
        case 1:
            control = new QPushButton(QStringLiteral("button"), parent);
            break;
        case 2: {
            auto* const tabs = new QTabBar(parent);
            tabs->addTab(QStringLiteral("one"));
            tabs->addTab(QStringLiteral("two"));
            control = tabs;
            break;
        }
        default: {
            auto* const combo = new QComboBox(parent);
            combo->addItems({QStringLiteral("one"), QStringLiteral("two")});
            control = combo;
            break;
        }
        }
        control->setFocusPolicy(Qt::StrongFocus);
        control->show();
        return control;
    }
};

QTEST_MAIN(TestHotKeyRouter)
#include "tst_hot_key_router.moc"
