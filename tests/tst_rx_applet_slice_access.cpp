// =================================================================
// tests/tst_rx_applet_slice_access.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Slice control and shared listening plan, Task 15: the RX applet's tabs
// list the slices this window controls or listens to. On a listened slice
// (another device controls it) every shared tuning and DSP control in the
// applet is disabled with the reason naming that device, and the applet
// never writes the slice. Take control stays reachable from the tab and
// badge menus; the tabs themselves stay usable, so a listened tab can be
// selected. The applet has no volume or mute (ruling U5: the flag's
// "Your volume" is that surface).
//
// Modification history (NereusSDR):
//   2026-09-29 - Written for slice control plan Task 15. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-30 - TX rulings (item 3, JJ): the attenuator and preamp
//                 controls are held on a listened slice with the same
//                 reason, write nothing, and come back on a controlled one,
//                 local and remote. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: Take control of the Core's own
//               slice is disabled with the Core's words below
//               sliceAccessVersion 3. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================
#include <QtTest/QtTest>
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>

#include "core/AppSettings.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/accessories/AlexController.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const QString kReason = QStringLiteral("Shack iPad controls this slice");

VfoWidget::SliceAccess listened()
{
    VfoWidget::SliceAccess access;
    access.state = VfoWidget::SliceAccess::State::Listening;
    access.line = QStringLiteral("Listening · controlled by Shack iPad");
    access.heldReason = kReason;
    return access;
}

VfoWidget::SliceAccess controlled()
{
    VfoWidget::SliceAccess access;
    access.state = VfoWidget::SliceAccess::State::Controlled;
    access.line = QStringLiteral("You control");
    return access;
}

QAction* findAction(QMenu& menu, const QString& text)
{
    for (QAction* action : menu.actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

// Every control in the applet that changes the slice: combos, sliders,
// buttons and the passband. The radio's own hardware (the ATT/S-ATT row and
// the RX1 preamp toggle, held too since TX rulings item 3 and checked by
// their own tests below) and the slice tabs are not slice settings.
QList<QWidget*> sliceControls(RxApplet& applet)
{
    QWidget* attStack = applet.findChild<QWidget*>(QStringLiteral("RxAttenuatorStack"));
    QList<QWidget*> controls;
    const QList<QWidget*> all = applet.findChildren<QWidget*>();
    for (QWidget* w : all) {
        const bool kind = qobject_cast<QComboBox*>(w) || qobject_cast<QSlider*>(w)
            || qobject_cast<QPushButton*>(w) || qobject_cast<FilterPassbandWidget*>(w);
        if (!kind || qobject_cast<QCheckBox*>(w)) { continue; }
        if (attStack && attStack->isAncestorOf(w)) { continue; }
        controls.append(w);
    }
    return controls;
}

struct Snapshot {
    DSPMode mode;
    int filterLow;
    int filterHigh;
    AGCMode agc;
    bool locked;
    double pan;
    bool ssql;
    double ssqlThresh;
    int agcThreshold;
    bool rit;
    int ritHz;
    bool xit;
    int xitHz;
    int stepHz;
    QString rxAnt;
    QString txAnt;
    bool operator==(const Snapshot&) const = default;
};

Snapshot snapshot(const SliceModel& s)
{
    return {s.dspMode(), s.filterLow(), s.filterHigh(), s.agcMode(), s.locked(),
            s.audioPan(), s.ssqlEnabled(), s.ssqlThresh(), s.agcThreshold(),
            s.ritEnabled(), s.ritHz(), s.xitEnabled(), s.xitHz(), s.stepHz(),
            s.rxAntenna(), s.txAntenna()};
}

// Drives every slice control the way code can even when it is disabled:
// toggles, clicks, index and value changes, and the passband's edit signal.
// The antenna buttons open a menu, so they are left to the caller.
void driveEverything(RxApplet& applet)
{
    for (QWidget* w : sliceControls(applet)) {
        const QString name = w->objectName();
        if (name == QStringLiteral("m_rxAntBtn") || name == QStringLiteral("m_txAntBtn")) {
            continue;
        }
        if (auto* combo = qobject_cast<QComboBox*>(w)) {
            if (combo->count() > 1) {
                combo->setCurrentIndex((combo->currentIndex() + 1) % combo->count());
            }
        } else if (auto* slider = qobject_cast<QSlider*>(w)) {
            slider->setValue(slider->value() == slider->maximum() ? slider->minimum()
                                                                  : slider->maximum());
        } else if (auto* passband = qobject_cast<FilterPassbandWidget*>(w)) {
            emit passband->filterChanged(150, 1850);
        } else if (auto* button = qobject_cast<QPushButton*>(w)) {
            if (button->isCheckable()) {
                button->toggle();
            }
            emit button->clicked(button->isChecked());
        }
    }
}

} // namespace

class TestRxAppletSliceAccess : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    void unshared_applet_is_not_listening()
    {
        SliceModel slice(0);
        RxApplet applet(&slice, nullptr);
        QCOMPARE(applet.sliceAccess().state, VfoWidget::SliceAccess::State::Unshared);
        QVERIFY(!applet.isListening());
        for (QWidget* control : sliceControls(applet)) {
            QVERIFY(control->isEnabled());
        }
    }

    void listened_applet_holds_every_slice_control_with_the_reason()
    {
        SliceModel slice(1);
        RxApplet applet(&slice, nullptr);
        applet.setSliceAccess(listened());
        QVERIFY(applet.isListening());

        const QList<QWidget*> controls = sliceControls(applet);
        QVERIFY(controls.size() >= 20);
        const QList<QWidget*> held = applet.heldControlsForTest();
        for (QWidget* control : controls) {
            const QString what = control->objectName().isEmpty()
                ? QString::fromLatin1(control->metaObject()->className())
                : control->objectName();
            QVERIFY2(held.contains(control), qPrintable(what));
            QVERIFY2(!control->isEnabled(), qPrintable(what));
            QCOMPARE(control->toolTip(), kReason);
            QCOMPARE(control->accessibleDescription(), kReason);
        }
    }

    // TX rulings (item 3, JJ): on a listened slice the attenuator and
    // preamp controls are disabled with the reason naming the controller,
    // and the applet writes neither; on a controlled slice they work.
    void listened_applet_holds_the_attenuator_and_preamp()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        auto* ctrl = new StepAttenuatorController(&model);
        ctrl->setTickTimerEnabled(false);
        model.setStepAttController(ctrl);
        SliceModel* a = model.sliceById(model.addSlice());
        QVERIFY(a);
        ctrl->setStepAttEnabled(false);
        ctrl->setPreampMode(PreampMode::SaMinus10);
        RxApplet applet(nullptr, &model);
        applet.setSlice(a);
        auto* label = applet.findChild<QLabel*>(QStringLiteral("RxAttLabel"));
        auto* stack = applet.findChild<QWidget*>(QStringLiteral("RxAttenuatorStack"));
        auto* combo = applet.findChild<QComboBox*>(QStringLiteral("RxPreampCombo"));
        auto* spin = stack ? stack->findChild<QSpinBox*>() : nullptr;
        QVERIFY(label && stack && combo && spin);
        const QList<QWidget*> att{label, stack, combo, spin};
        QVERIFY(combo->count() > 1);

        applet.setSliceAccess(listened());
        const QList<QWidget*> held = applet.heldControlsForTest();
        for (QWidget* w : att) {
            QVERIFY2(held.contains(w), qPrintable(w->objectName()));
            QVERIFY2(!w->isEnabled(), qPrintable(w->objectName()));
            QCOMPARE(w->toolTip(), kReason);
            QCOMPARE(w->accessibleDescription(), kReason);
        }
        const PreampMode mode = ctrl->preampMode();
        const int dB = ctrl->attenuatorDb();
        combo->setCurrentIndex((combo->currentIndex() + 1) % combo->count());
        spin->setValue(spin->value() == spin->maximum() ? spin->minimum() : spin->maximum());
        QCOMPARE(ctrl->preampMode(), mode);
        QCOMPARE(ctrl->attenuatorDb(), dB);

        applet.setSliceAccess(controlled());
        for (QWidget* w : att) {
            QVERIFY2(w->isEnabled(), qPrintable(w->objectName()));
            QVERIFY(w->toolTip() != kReason);
        }
        const int next = (combo->currentIndex() + 1) % combo->count();
        combo->setCurrentIndex(next);
        QCOMPARE(static_cast<int>(ctrl->preampMode()), combo->itemData(next).toInt());
    }

    // A remote window: the Core's attenuator arriving while the slice is
    // listened keeps the controls held, and control brings them back.
    void remote_attenuator_offered_while_listening_stays_held()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(nullptr, &remote);
        SliceModel slice(1);
        applet.setSlice(&slice);
        auto* combo = applet.findChild<QComboBox*>(QStringLiteral("RxPreampCombo"));
        QVERIFY(combo);
        applet.setSliceAccess(listened());
        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        stepAtt->setWindowAvailability(true, QString());
        QVERIFY(!combo->isEnabled());
        QCOMPARE(combo->toolTip(), kReason);

        applet.setSliceAccess(controlled());
        QVERIFY(combo->isEnabled());
        QVERIFY(combo->toolTip().isEmpty());

        // The Core withdrawing it while listened comes back withdrawn.
        applet.setSliceAccess(listened());
        const QString gone = QStringLiteral("The Core has no attenuator ready.");
        stepAtt->setWindowAvailability(false, gone);
        QCOMPARE(combo->toolTip(), kReason);
        applet.setSliceAccess(controlled());
        QVERIFY(!combo->isEnabled());
        QVERIFY(combo->toolTip() != kReason);
    }

    void controlled_applet_writes_the_slice()
    {
        // The baseline: the same drive does change a slice this window
        // controls, so the listened check below is not vacuous.
        SliceModel slice(0);
        RxApplet applet(&slice, nullptr);
        applet.setSliceAccess(controlled());
        QSignalSpy autoAgc(&applet, &RxApplet::autoAgcToggled);
        const Snapshot before = snapshot(slice);
        driveEverything(applet);
        QVERIFY(!(snapshot(slice) == before));
        QCOMPARE(autoAgc.count(), 1);
    }

    void listened_applet_never_writes_the_slice()
    {
        SliceModel slice(1);
        RxApplet applet(&slice, nullptr);
        applet.setSliceAccess(listened());
        QSignalSpy autoAgc(&applet, &RxApplet::autoAgcToggled);
        QSignalSpy modeSpy(&slice, &SliceModel::dspModeChanged);
        QSignalSpy filterSpy(&slice, &SliceModel::filterChanged);
        QSignalSpy lockSpy(&slice, &SliceModel::lockedChanged);
        const Snapshot before = snapshot(slice);

        driveEverything(applet);

        // The antenna buttons must not open their menus either; a menu that
        // opened is closed and recorded.
        bool menuOpened = false;
        for (const QString& name : {QStringLiteral("m_rxAntBtn"), QStringLiteral("m_txAntBtn")}) {
            auto* button = applet.findChild<QPushButton*>(name);
            QVERIFY(button);
            QTimer::singleShot(0, [&menuOpened]() {
                if (QWidget* popup = QApplication::activePopupWidget()) {
                    menuOpened = true;
                    popup->close();
                }
            });
            emit button->clicked(false);
            QCoreApplication::processEvents();
        }

        QVERIFY(snapshot(slice) == before);
        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(filterSpy.count(), 0);
        QCOMPARE(lockSpy.count(), 0);
        QCOMPARE(autoAgc.count(), 0);
        QVERIFY(!menuOpened);
    }

    void listened_applet_reads_the_band_antenna_without_writing_it()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        model.addPanadapter();
        model.panadapters().first()->setBand(Band::Band20m);
        model.addSlice();
        model.addSlice();
        // RadioModel keeps its active slice's antenna label in step with
        // the band's (RadioModel.cpp, the antennaChanged handler); the
        // applet here shows the other slice, so any write is the applet's.
        SliceModel* slice = model.sliceById(0) == model.activeSlice()
            ? model.sliceById(1) : model.sliceById(0);
        QVERIFY(slice);
        QVERIFY(slice != model.activeSlice());

        RxApplet applet(slice, &model);
        applet.setSliceAccess(listened());
        const QString before = slice->rxAntenna();
        QSignalSpy rxAntSpy(slice, &SliceModel::rxAntennaChanged);

        const int other = before == QStringLiteral("ANT3") ? 2 : 3;
        model.alexControllerMutable().setRxAnt(Band::Band20m, other);

        QCOMPARE(rxAntSpy.count(), 0);
        QCOMPARE(slice->rxAntenna(), before);
        // The button shows the slice's antenna, which the controller sets.
        QCOMPARE(applet.findChild<QPushButton*>(QStringLiteral("m_rxAntBtn"))->text(), before);
    }

    void controlling_again_restores_every_control()
    {
        SliceModel slice(0);
        RxApplet applet(&slice, nullptr);
        QHash<QWidget*, QString> tips;
        for (QWidget* control : sliceControls(applet)) {
            tips.insert(control, control->toolTip());
        }

        applet.setSliceAccess(listened());
        applet.setSliceAccess(controlled());

        for (QWidget* control : sliceControls(applet)) {
            QVERIFY(control->isEnabled());
            QCOMPARE(control->toolTip(), tips.value(control));
            QVERIFY(control->accessibleDescription() != kReason);
        }
    }

    void filter_buttons_rebuilt_while_listening_are_held_then_restored()
    {
        SliceModel slice(1);
        RxApplet applet(&slice, nullptr);
        applet.setSliceAccess(listened());

        // The controlling device changes the mode; the applet rebuilds its
        // preset buttons for the new mode.
        slice.setDspMode(slice.dspMode() == DSPMode::AM ? DSPMode::USB : DSPMode::AM);
        QCoreApplication::processEvents();  // the old buttons are deleted later

        const QList<QWidget*> controls = sliceControls(applet);
        for (QWidget* control : controls) {
            QVERIFY(!control->isEnabled());
            QCOMPARE(control->toolTip(), kReason);
        }

        applet.setSliceAccess(controlled());
        for (QWidget* control : sliceControls(applet)) {
            QVERIFY(control->isEnabled());
            QVERIFY(control->toolTip() != kReason);
        }
    }

    void listened_menu_offers_take_control_and_stop_listening()
    {
        SliceModel a(0);
        SliceModel b(1);
        RxApplet applet(&b, nullptr);
        applet.setSliceAccess(listened());
        applet.setSliceTabAccess({{0, controlled()}, {1, listened()}});
        QSignalSpy take(&applet, &RxApplet::takeControlRequested);
        QSignalSpy stop(&applet, &RxApplet::stopListeningRequested);
        QSignalSpy release(&applet, &RxApplet::releaseRequested);

        QMenu listenedMenu;
        applet.populateSliceMenu(listenedMenu, 1);
        QAction* takeAct = findAction(listenedMenu, QStringLiteral("Take control"));
        QAction* stopAct = findAction(listenedMenu, QStringLiteral("Stop listening"));
        QVERIFY(takeAct);
        QVERIFY(stopAct);
        QVERIFY(takeAct->isEnabled());
        QVERIFY(!findAction(listenedMenu, QStringLiteral("Release")));
        takeAct->trigger();
        stopAct->trigger();
        QCOMPARE(take.count(), 1);
        QCOMPARE(take.first().at(0).toInt(), 1);
        QCOMPARE(stop.count(), 1);
        QCOMPARE(stop.first().at(0).toInt(), 1);

        QMenu controlledMenu;
        applet.populateSliceMenu(controlledMenu, 0);
        QAction* releaseAct = findAction(controlledMenu, QStringLiteral("Release"));
        QVERIFY(releaseAct);
        QVERIFY(!findAction(controlledMenu, QStringLiteral("Take control")));
        releaseAct->trigger();
        QCOMPARE(release.count(), 1);
        QCOMPARE(release.first().at(0).toInt(), 0);
    }

    // Core-slice take-over: Take control stays in the tab menu, disabled
    // with the Core's words, when the Core refuses the take.
    void listened_menu_take_control_is_disabled_with_the_cores_words()
    {
        SliceModel a(0);
        RxApplet applet(&a, nullptr);
        VfoWidget::SliceAccess access = listened();
        access.takeHeldReason = QStringLiteral("Slice A is run by the Core itself, so control of it cannot pass to this device.");
        applet.setSliceAccess(access);
        applet.setSliceTabAccess({{0, access}});
        QSignalSpy take(&applet, &RxApplet::takeControlRequested);

        QMenu menu;
        applet.populateSliceMenu(menu, 0);
        QAction* takeAct = findAction(menu, QStringLiteral("Take control"));
        QVERIFY(takeAct);
        QVERIFY(!takeAct->isEnabled());
        QCOMPARE(takeAct->toolTip(), access.takeHeldReason);
        QAction* stopAct = findAction(menu, QStringLiteral("Stop listening"));
        QVERIFY(stopAct && stopAct->isEnabled());
        takeAct->trigger();
        QCOMPARE(take.count(), 0);
    }

    void unshared_menu_has_no_access_actions()
    {
        SliceModel slice(0);
        RxApplet applet(&slice, nullptr);
        QMenu menu;
        applet.populateSliceMenu(menu, 0);
        QVERIFY(menu.actions().isEmpty());
    }

    void a_waiting_request_disables_the_access_actions()
    {
        SliceModel slice(1);
        RxApplet applet(&slice, nullptr);
        applet.setSliceAccess(listened());
        applet.setSliceTabAccess({{1, listened()}});
        applet.setSliceAccessPending(QStringLiteral("Asking the Core…"));
        QSignalSpy take(&applet, &RxApplet::takeControlRequested);

        QMenu menu;
        applet.populateSliceMenu(menu, 1);
        QAction* takeAct = findAction(menu, QStringLiteral("Take control"));
        QVERIFY(takeAct);
        QVERIFY(!takeAct->isEnabled());
        QCOMPARE(takeAct->toolTip(), QStringLiteral("Asking the Core…"));
        takeAct->trigger();
        QCOMPARE(take.count(), 0);

        applet.setSliceAccessPending(QString());
        QMenu again;
        applet.populateSliceMenu(again, 1);
        QVERIFY(findAction(again, QStringLiteral("Take control"))->isEnabled());
    }

    void tabs_say_who_controls_each_slice_and_stay_selectable()
    {
        SliceModel a(0);
        SliceModel b(1);
        RxApplet applet(&a, nullptr);
        applet.setSliceAccess(controlled());
        applet.setSliceTabAccess({{0, controlled()}, {1, listened()}});
        applet.updateSliceButtons({&a, &b}, 0);

        QCOMPARE(applet.sliceTabToolTipForTest(0), QStringLiteral("Slice A: You control"));
        QCOMPARE(applet.sliceTabToolTipForTest(1),
                 QStringLiteral("Slice B: Listening · controlled by Shack iPad"));

        QSignalSpy activate(&applet, &RxApplet::sliceActivationRequested);
        QToolButton* tabB = nullptr;
        for (QToolButton* tab : applet.findChildren<QToolButton*>()) {
            if (tab->text() == QStringLiteral("B")) { tabB = tab; }
        }
        QVERIFY(tabB);
        QVERIFY(tabB->isEnabled());
        tabB->click();
        QCOMPARE(activate.count(), 1);
        QCOMPARE(activate.first().at(0).toInt(), 1);

        // Selecting the listened slice binds it listened: the tabs stay
        // usable while the slice's controls are held.
        applet.setSlice(&b);
        applet.setSliceAccess(listened());
        applet.updateSliceButtons({&a, &b}, 1);
        for (QToolButton* tab : applet.findChildren<QToolButton*>()) {
            QVERIFY(tab->isEnabled());
        }
    }

    void unshared_tabs_keep_the_plain_tooltip()
    {
        SliceModel a(0);
        SliceModel b(1);
        RxApplet applet(&a, nullptr);
        applet.updateSliceButtons({&a, &b}, 0);
        QCOMPARE(applet.sliceTabToolTipForTest(1), QStringLiteral("Slice B"));
    }

    void applet_has_no_volume_or_mute()
    {
        // Ruling U5: the flag's "Your volume" is the listener's volume and
        // mute; the RX applet gains neither.
        SliceModel slice(1);
        RxApplet applet(&slice, nullptr);
        applet.setSliceAccess(listened());
        for (QAbstractButton* button : applet.findChildren<QAbstractButton*>()) {
            QVERIFY2(!button->text().contains(QStringLiteral("mute"), Qt::CaseInsensitive),
                     qPrintable(button->text()));
        }
        for (QLabel* label : applet.findChildren<QLabel*>()) {
            QVERIFY(label->text() != QStringLiteral("AF"));
            QVERIFY(!label->text().contains(QStringLiteral("volume"), Qt::CaseInsensitive));
        }
        QCOMPARE(applet.findChildren<QSlider*>().size(), 3);  // pan, squelch, AGC-T
    }
};

QTEST_MAIN(TestRxAppletSliceAccess)
#include "tst_rx_applet_slice_access.moc"
