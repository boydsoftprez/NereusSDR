// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 18.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Setup > Keys and controllers > Knobs. Device states are handed to the host as the
// device layer would. Simulated; no device has been tried and no real one is opened
// here, since only main() allows that (R-HK-19).
#include <QtTest>

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QSignalSpy>
#include <QSlider>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QWheelEvent>

#include <memory>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "fakes/FakeMidiBackend.h"
#include "gui/SetupDialog.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/FlexControlInput.h"
#include "gui/setup/KnobsSetupPage.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using Platform = KnobsSetupPage::Platform;

namespace {

const QString kNotPlugged = QStringLiteral("Not plugged in");

class NoTargets : public HotKeyTargets {
public:
    SliceModel* windowSlice() const override { return nullptr; }
    SliceModel* sliceByLetter(QChar) const override { return nullptr; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel*, double) override { return true; }
    void runSliceAction(const QString&, SliceModel*, int) override {}
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return false; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool) override { return true; }
    void runTransmitAction(const QString&) override {}
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString&, bool) override {}
    void toast(const QString&) override {}
    qint64 nowMs() const override { return 1000; }
};

bool choose(QComboBox* combo, const QVariant& data)
{
    const int index = combo != nullptr ? combo->findData(data) : -1;
    if (index < 0) {
        return false;
    }
    combo->setCurrentIndex(index);
    emit combo->activated(index);
    return true;
}

bool itemEnabled(QComboBox* combo, const QString& id, QString* reason = nullptr)
{
    const auto* const model = qobject_cast<QStandardItemModel*>(combo->model());
    const QStandardItem* const item = model->item(combo->findData(id));
    if (reason != nullptr) {
        *reason = item->toolTip();
    }
    return item->isEnabled();
}

QList<QPair<QString, QString>> tableRows(QTableWidget* table)
{
    QList<QPair<QString, QString>> rows;
    for (int i = 0; i < table->rowCount(); ++i) {
        rows.append({table->item(i, 0)->text(), table->item(i, 1)->text()});
    }
    return rows;
}

} // namespace

class TestHotKeySetupKnobs : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<NoTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<FakeMidiDeviceLayer> m_layer;
    std::unique_ptr<ControllerHost> m_host;
    std::unique_ptr<KnobsSetupPage> m_page;

    KnobsSetupPage& open()
    {
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_page = std::make_unique<KnobsSetupPage>(m_host.get(), m_bindings.get());
        return *m_page;
    }

    template <typename T>
    T* named(const QString& name)
    {
        return m_page->findChild<T*>(name);
    }

    QAbstractButton* deviceRow(DeviceId id)
    {
        return named<QAbstractButton>(QStringLiteral("knobDevice:") + HotKeyDeviceBindings::deviceKey(id));
    }

    QString stateLine(DeviceId id) { return deviceRow(id)->property("stateLine").toString(); }
    QTableWidget* list() { return named<QTableWidget>(QStringLiteral("knobsList")); }
    QString state() { return named<QLabel>(QStringLiteral("knobsState"))->text(); }
    QString note() { return named<QLabel>(QStringLiteral("knobsNote"))->text(); }
    QComboBox* action(const char* suffix) { return named<QComboBox>(QStringLiteral("knobAction:") + QLatin1String(suffix)); }
    QComboBox* down(const char* suffix) { return named<QComboBox>(QStringLiteral("knobDown:") + QLatin1String(suffix)); }
    QComboBox* actsOn(const char* suffix) { return named<QComboBox>(QStringLiteral("knobActsOn:") + QLatin1String(suffix)); }
    HotKeyDeviceBindings* store() { return m_host->deviceBindings(); }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<NoTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_layer = std::make_unique<FakeMidiDeviceLayer>();
    }

    void cleanup()
    {
        m_page.reset();
        m_host.reset();
        m_layer.reset();
        m_dispatcher.reset();
        m_targets.reset();
        m_bindings.reset();
        AppSettings::instance().clear();
    }

    void aTestProcessCantOpenARealDevice()
    {
        QVERIFY(!DeviceInput::realDevicesAllowed());
    }

    // ---- The list ----

    void theEightDevicesAreListedAndUntested()
    {
        open();
        const QList<DeviceId> devices = HotKeyDeviceBindings::devices();
        QCOMPARE(devices.size(), 8);
        for (const DeviceId id : devices) {
            QAbstractButton* const row = deviceRow(id);
            QVERIFY2(row != nullptr, qPrintable(HotKeyDeviceBindings::deviceName(id)));
            QCOMPARE(row->text(), HotKeyDeviceBindings::deviceName(id));
            QCOMPARE(stateLine(id), QStringLiteral("Not plugged in. Untested."));
            QVERIFY(row->isEnabled());
        }
    }

    // R-HK-19: whatever a device reports, the word stays.
    void noStateCanChangeUntested()
    {
        open();
        const QStringList reasons{kNotPlugged, deviceOpenFailureReason(QStringLiteral("exclusive access")),
                                  deviceOpenFailureReason(QStringLiteral("not permitted")),
                                  QStringLiteral("A deck plugin is using it."), QStringLiteral("anything")};
        for (const DeviceId id : HotKeyDeviceBindings::devices()) {
            QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(id));
            m_host->onDeviceStateChanged(id, true, {});
            QCOMPARE(stateLine(id), QStringLiteral("Connected. Untested."));
            for (const QString& reason : reasons) {
                m_host->onDeviceStateChanged(id, false, reason);
                QVERIFY2(stateLine(id).endsWith(QLatin1String(". Untested.")), qPrintable(stateLine(id)));
            }
        }
        m_page->setSupportedForTest(false, false);
        for (const DeviceId id : HotKeyDeviceBindings::devices()) {
            QVERIFY2(stateLine(id).endsWith(QLatin1String(". Untested.")), qPrintable(stateLine(id)));
        }
    }

    void theStatesReadPlainly()
    {
        open();
        m_host->onDeviceStateChanged(DeviceId::Rc28, true, {});
        QCOMPARE(stateLine(DeviceId::Rc28), QStringLiteral("Connected. Untested."));
        QCOMPARE(state(), QString());
        m_host->onDeviceStateChanged(DeviceId::Rc28, false, kNotPlugged);
        QCOMPARE(stateLine(DeviceId::Rc28), QStringLiteral("Not plugged in. Untested."));
        QCOMPARE(state(), QString());

        // The words the device layers really give.
        m_host->onDeviceStateChanged(DeviceId::Rc28, false, deviceOpenFailureReason(QStringLiteral("resource busy")));
        QCOMPARE(stateLine(DeviceId::Rc28), QStringLiteral("Another app has it. Untested."));
        QCOMPARE(state(), QStringLiteral("Another app has this device. Quit the other app using it, and it connects on its own."));
        QCOMPARE(deviceRow(DeviceId::Rc28)->toolTip(), state());

        m_page->selectDevice(DeviceId::FlexControl);
        m_host->onDeviceStateChanged(DeviceId::FlexControl, false, flexOpenFailureReason(FlexOpenFailure::Denied));
        if (KnobsSetupPage::thisPlatform() != Platform::Linux) {
            QVERIFY2(state().startsWith(QLatin1String("Another app has this device.")), qPrintable(state()));
        }

        const QString blocked = deviceOpenFailureReason(QStringLiteral("not permitted"));
        m_page->selectDevice(DeviceId::PowerMate);
        m_host->onDeviceStateChanged(DeviceId::PowerMate, false, blocked);
        QCOMPARE(stateLine(DeviceId::PowerMate), QStringLiteral("Can't open it. Untested."));
        QCOMPARE(state(), blocked);

        m_page->selectDevice(DeviceId::StreamDeckPlus);
        m_host->onDeviceStateChanged(DeviceId::StreamDeckPlus, false, QStringLiteral("A deck plugin is using it."));
        QCOMPARE(stateLine(DeviceId::StreamDeckPlus), QStringLiteral("A deck plugin has it. Untested."));
        QCOMPARE(state(), QStringLiteral("A deck plugin is using it."));
    }

    void theFixNamesTheAppToQuitWhereThereIsASource()
    {
        const QString busy = deviceOpenFailureReason(QStringLiteral("exclusive access"));
        QCOMPARE(KnobsSetupPage::stateFix(DeviceId::StreamDeckPlus, false, busy, Platform::Mac),
                 QStringLiteral("Another app has this device. Quit the Stream Deck app, and it connects on its own."));
        QCOMPARE(KnobsSetupPage::stateFix(DeviceId::StreamDeckPlus, false, busy, Platform::Linux),
                 QStringLiteral("Another app has this device. Quit StreamController, and it connects on its own."));
        QCOMPARE(KnobsSetupPage::stateFix(DeviceId::UlanziDial, false, busy, Platform::Windows),
                 QStringLiteral("Another app has this device. Quit Ulanzi Studio, and it connects on its own."));
        QCOMPARE(KnobsSetupPage::appToQuit(DeviceId::UlanziDial, Platform::Linux), QString());
        QCOMPARE(KnobsSetupPage::appToQuit(DeviceId::Rc28, Platform::Windows), QString());
    }

    // ---- What each system says ----

    void theUlanziRowSaysWhatItsSystemMakesTrue_data()
    {
        QTest::addColumn<int>("platform");
        QTest::addColumn<bool>("connected");
        QTest::addColumn<QString>("reason");
        QTest::addColumn<QString>("has");

        const QString windows = QStringLiteral("focused window");
        const QString linux_ = QStringLiteral("60-nereussdr-controllers.rules");
        const QString mac = QStringLiteral("Input Monitoring");
        QTest::newRow("windows, not plugged in") << int(Platform::Windows) << false << kNotPlugged << windows;
        QTest::newRow("linux, not plugged in") << int(Platform::Linux) << false << kNotPlugged << linux_;
        QTest::newRow("mac, not plugged in") << int(Platform::Mac) << false << kNotPlugged << QString();
        QTest::newRow("mac, nothing heard yet") << int(Platform::Mac) << false << QString() << QString();
        QTest::newRow("mac, connected") << int(Platform::Mac) << true << QString() << mac;
        QTest::newRow("mac, another app") << int(Platform::Mac) << false
                                          << deviceOpenFailureReason(QStringLiteral("exclusive access")) << mac;
    }

    void theUlanziRowSaysWhatItsSystemMakesTrue()
    {
        QFETCH(int, platform);
        QFETCH(bool, connected);
        QFETCH(QString, reason);
        QFETCH(QString, has);
        const QString text = KnobsSetupPage::deviceNote(DeviceId::UlanziDial, connected, reason, Platform(platform));
        if (has.isEmpty()) {
            QCOMPARE(text, QString());
        } else {
            QVERIFY2(text.contains(has), qPrintable(text));
        }
        if (Platform(platform) == Platform::Windows) {
            QVERIFY(text.contains(QStringLiteral("Ctrl chords may not arrive")));
        }
    }

    void onlyTheirOwnDevicesCarryANote()
    {
        for (const DeviceId id : HotKeyDeviceBindings::devices()) {
            for (const Platform platform : {Platform::Mac, Platform::Windows, Platform::Linux}) {
                const QString text = KnobsSetupPage::deviceNote(id, true, QString(), platform);
                const bool expected = id == DeviceId::UlanziDial || (id == DeviceId::FlexControl && platform == Platform::Linux);
                QCOMPARE(!text.isEmpty(), expected);
                if (text.contains(QLatin1String("Input Monitoring"))) {
                    QCOMPARE(id, DeviceId::UlanziDial);
                }
            }
        }
        QVERIFY(KnobsSetupPage::deviceNote(DeviceId::FlexControl, false, kNotPlugged, Platform::Linux)
                    .contains(QStringLiteral("dialout")));
    }

    void theMacNoteDoesntRepeatWhatTheFixSays()
    {
        const QString blocked = QStringLiteral("macOS is blocking it. Turn on NereusSDR in System Settings, "
                                               "under Privacy & Security, Input Monitoring.");
        QCOMPARE(KnobsSetupPage::deviceNote(DeviceId::UlanziDial, false, blocked, Platform::Mac), QString());
    }

    void theNoteShowsOnThePageForTheChosenDevice()
    {
        open();
        m_page->setPlatformForTest(Platform::Windows);
        m_page->selectDevice(DeviceId::UlanziDial);
        QVERIFY2(note().contains(QStringLiteral("focused window")), qPrintable(note()));
        m_page->setPlatformForTest(Platform::Mac);
        QCOMPARE(note(), QString());
        m_host->onDeviceStateChanged(DeviceId::UlanziDial, true, {});
        QVERIFY2(note().contains(QStringLiteral("Input Monitoring")), qPrintable(note()));
        m_page->selectDevice(DeviceId::Rc28);
        QCOMPARE(note(), QString());
    }

    // ---- A build without a library ----

    void aBuildWithoutHidapiKeepsDevicesListedAndDisabled()
    {
        open();
        m_page->setPlatformForTest(Platform::Mac);
        m_page->setSupportedForTest(false, true);
        for (const DeviceId id : {DeviceId::Rc28, DeviceId::PowerMate, DeviceId::ShuttleXpress, DeviceId::ShuttlePro2,
                                  DeviceId::TMate2, DeviceId::StreamDeckPlus}) {
            QAbstractButton* const row = deviceRow(id);
            QVERIFY(row != nullptr);
            QVERIFY(!row->isEnabled());
            QCOMPARE(row->toolTip(), DeviceInput::unsupportedReason());
            QCOMPARE(stateLine(id), QStringLiteral("Not in this build. Untested."));
            QVERIFY(!m_page->selectDevice(id));
        }
        QVERIFY(deviceRow(DeviceId::FlexControl)->isEnabled());
        QVERIFY(deviceRow(DeviceId::UlanziDial)->isEnabled());
        QCOMPARE(m_page->selectedDevice(), DeviceId::FlexControl);
        QVERIFY(list()->isEnabled());

        // On Windows the dial needs hidapi too.
        m_page->setPlatformForTest(Platform::Windows);
        QVERIFY(!deviceRow(DeviceId::UlanziDial)->isEnabled());
    }

    void aBuildWithoutSerialPortKeepsTheFlexControlListedAndDisabled()
    {
        open();
        m_page->selectDevice(DeviceId::FlexControl);
        m_page->setSupportedForTest(true, false);
        QVERIFY(!deviceRow(DeviceId::FlexControl)->isEnabled());
        QCOMPARE(deviceRow(DeviceId::FlexControl)->toolTip(), FlexControlInput::unsupportedReason());
        QCOMPARE(m_page->selectedDevice(), DeviceId::Rc28);
    }

    void aBuildWithNeitherDisablesTheRowsWithTheReason()
    {
        open();
        m_page->setPlatformForTest(Platform::Windows);
        m_page->setSupportedForTest(false, false);
        QVERIFY(!list()->isEnabled());
        QCOMPARE(list()->toolTip(), DeviceInput::unsupportedReason());
        QCOMPARE(state(), DeviceInput::unsupportedReason());
        QVERIFY(!named<QGroupBox>(QStringLiteral("knobsBehavior"))->isEnabled());
    }

    // ---- The rows ----

    void eachDeviceHasARowPerControlAndGesture_data()
    {
        QTest::addColumn<int>("device");
        QTest::addColumn<int>("rows");
        QTest::addColumn<int>("caps");
        QTest::newRow("RC-28") << int(DeviceId::Rc28) << 6 << 4;
        QTest::newRow("PowerMate") << int(DeviceId::PowerMate) << 2 << 1;
        QTest::newRow("ShuttleXpress") << int(DeviceId::ShuttleXpress) << 7 << 7;
        QTest::newRow("ShuttlePro v2") << int(DeviceId::ShuttlePro2) << 17 << 17;
        QTest::newRow("FlexControl") << int(DeviceId::FlexControl) << 13 << 5;
        QTest::newRow("TMate 2") << int(DeviceId::TMate2) << 9 << 9;
        QTest::newRow("Stream Deck+") << int(DeviceId::StreamDeckPlus) << 16 << 12;
        QTest::newRow("Ulanzi Dial") << int(DeviceId::UlanziDial) << 9 << 8;
    }

    void eachDeviceHasARowPerControlAndGesture()
    {
        QFETCH(int, device);
        QFETCH(int, rows);
        QFETCH(int, caps);
        open();
        QVERIFY(m_page->selectDevice(DeviceId(device)));
        QCOMPARE(list()->rowCount(), rows);
        const QList<QAbstractButton*> buttons = named<QWidget>(QStringLiteral("knobsDrawing"))->findChildren<QAbstractButton*>();
        QCOMPARE(int(buttons.size()), caps);
        // Every cap stands on the stage, at the mockup's places.
        for (const QAbstractButton* const button : buttons) {
            QVERIFY2(QRect(0, 0, 396, 190).contains(button->geometry()), qPrintable(button->objectName()));
        }
    }

    // The controls the parsers name, so a row can't be for a control that never reports.
    void theRowsMatchTheDefaults()
    {
        open();
        const auto check = [this](DeviceId id, const QString& control, ControlGesture gesture, const char* what) {
            m_page->selectDevice(id);
            const QString suffix = QStringLiteral("%1:%2").arg(control, QLatin1String(what));
            auto* const combo = named<QComboBox>(QStringLiteral("knobAction:") + suffix);
            QVERIFY2(combo != nullptr, qPrintable(suffix));
            QCOMPARE(combo->currentData().toString(), store()->binding(id, control, gesture).actionId);
        };
        check(DeviceId::Rc28, QStringLiteral("wheel"), ControlGesture::Turn, "Turn");
        check(DeviceId::Rc28, QStringLiteral("f1"), ControlGesture::Hold, "Hold");
        check(DeviceId::Rc28, QStringLiteral("tx"), ControlGesture::Press, "Press");
        check(DeviceId::PowerMate, QStringLiteral("press"), ControlGesture::Press, "Press");
        check(DeviceId::ShuttlePro2, QStringLiteral("button15"), ControlGesture::Press, "Press");
        check(DeviceId::ShuttlePro2, QStringLiteral("ring"), ControlGesture::Turn, "Turn");
        check(DeviceId::FlexControl, QStringLiteral("knob"), ControlGesture::DoubleTap, "DoubleTap");
        check(DeviceId::FlexControl, QStringLiteral("button3"), ControlGesture::Tap, "Tap");
        check(DeviceId::TMate2, QStringLiteral("e2"), ControlGesture::Turn, "Turn");
        check(DeviceId::StreamDeckPlus, QStringLiteral("button8"), ControlGesture::Press, "Press");
        check(DeviceId::StreamDeckPlus, QStringLiteral("dial4"), ControlGesture::Press, "Press");
        check(DeviceId::UlanziDial, QStringLiteral("rightBottom"), ControlGesture::Press, "Press");
        QCOMPARE(tableRows(list()).first(), (QPair<QString, QString>{QStringLiteral("Dial"), QStringLiteral("Turn")}));
    }

    void changingAnActionWritesThroughTheBindings()
    {
        open();
        QVERIFY(choose(action("f1:Press"), QStringLiteral("mute")));
        QCOMPARE(store()->binding(DeviceId::Rc28, QStringLiteral("f1"), ControlGesture::Press).actionId, QStringLiteral("mute"));

        // Saved, so a new object reads it.
        HotKeyDeviceBindings saved;
        QCOMPARE(saved.binding(DeviceId::Rc28, QStringLiteral("f1"), ControlGesture::Press).actionId, QStringLiteral("mute"));

        // None clears the control.
        QVERIFY(choose(action("f1:Press"), QString()));
        QCOMPARE(store()->binding(DeviceId::Rc28, QStringLiteral("f1"), ControlGesture::Press).actionId, QString());
        QVERIFY(!actsOn("f1:Press")->isEnabled());
        QCOMPARE(actsOn("f1:Press")->toolTip(), QStringLiteral("Choose an action first."));
    }

    void anOutsideChangeShowsOnTheRow()
    {
        open();
        store()->setBinding(DeviceId::Rc28, QStringLiteral("f2"), ControlGesture::Press,
                            HotKeyDeviceBinding{QStringLiteral("mute"), {}, {}});
        QCOMPARE(action("f2:Press")->currentData().toString(), QStringLiteral("mute"));
    }

    void theOtherDirectionFollowsOnlyItsOwn()
    {
        open();
        QCOMPARE(down("wheel:Turn")->currentData().toString(), QStringLiteral("tune.down"));
        QVERIFY(choose(action("wheel:Turn"), QStringLiteral("volume.up")));
        const HotKeyDeviceBinding turned = store()->binding(DeviceId::Rc28, QStringLiteral("wheel"), ControlGesture::Turn);
        QCOMPARE(turned.actionId, QStringLiteral("volume.up"));
        QCOMPARE(turned.actionIdDown, QStringLiteral("volume.down"));
        QCOMPARE(down("wheel:Turn")->currentData().toString(), QStringLiteral("volume.down"));

        // A choice of its own stays.
        QVERIFY(choose(down("wheel:Turn"), QStringLiteral("filter.previous")));
        QVERIFY(choose(action("wheel:Turn"), QStringLiteral("tune.up")));
        QCOMPARE(store()->binding(DeviceId::Rc28, QStringLiteral("wheel"), ControlGesture::Turn).actionIdDown,
                 QStringLiteral("filter.previous"));
    }

    void onlyATurnedControlHasAnOtherDirection()
    {
        open();
        QVERIFY(down("f1:Press") == nullptr);
        QCOMPARE(list()->item(1, 3)->text(), QStringLiteral("None"));
    }

    void whatARowCantRunIsDisabledWithWhy()
    {
        open();
        QString reason;
        QVERIFY(!itemEnabled(action("wheel:Turn"), QStringLiteral("tx.hold"), &reason));
        QCOMPARE(reason, QStringLiteral("A knob can't run Hold to talk. Bind it to a button."));
        QVERIFY(!itemEnabled(down("wheel:Turn"), QStringLiteral("tx.mox"), &reason));
        QVERIFY(itemEnabled(action("f1:Press"), QStringLiteral("tx.hold")));

        m_page->selectDevice(DeviceId::FlexControl);
        QVERIFY(!itemEnabled(action("button1:Tap"), QStringLiteral("tx.hold"), &reason));
        QVERIFY2(reason.contains(QStringLiteral("hold")), qPrintable(reason));
        QVERIFY(!itemEnabled(action("button1:DoubleTap"), QStringLiteral("tx.hold")));
        // Its hold is told once it's over, so no release could end one.
        QVERIFY(!itemEnabled(action("button1:Hold"), QStringLiteral("tx.hold"), &reason));
        QVERIFY2(reason.contains(QStringLiteral("FlexControl")), qPrintable(reason));
        QVERIFY(itemEnabled(action("button1:Hold"), QStringLiteral("mute")));
        QVERIFY(itemEnabled(action("button2:Tap"), QStringLiteral("tx.mox")));
    }

    void aRefusalPutsTheRowBackAndSaysWhy()
    {
        open();
        QSignalSpy refused(store(), &HotKeyDeviceBindings::bindingRefused);
        QVERIFY(choose(action("wheel:Turn"), QStringLiteral("tx.mox")));
        QCOMPARE(refused.size(), 1);
        QCOMPARE(action("wheel:Turn")->currentData().toString(), QStringLiteral("tune.up"));
        QCOMPARE(named<QLabel>(QStringLiteral("knobsStatus"))->text(),
                 QStringLiteral("A knob can't run MOX. Bind it to a button."));
    }

    void anActionTheWindowCantUseIsDisabledWithWhy()
    {
        open();
        m_page->setUnavailableReasonProvider([](const QString& id) {
            return id == QLatin1String("mute") ? QStringLiteral("Not here.") : QString();
        });
        QString reason;
        QVERIFY(!itemEnabled(action("f1:Press"), QStringLiteral("mute"), &reason));
        QCOMPARE(reason, QStringLiteral("Not here."));
        QVERIFY(itemEnabled(action("f1:Press"), QStringLiteral("filter.next")));
    }

    void actsOnPinsASliceWhereTheActionHasOne()
    {
        open();
        QComboBox* const combo = actsOn("f2:Hold");     // mute, a slice action
        QCOMPARE(combo->itemText(0), QStringLiteral("Selected slice"));
        QCOMPARE(combo->count(), 5);
        QCOMPARE(combo->itemText(4), QStringLiteral("Slice D"));
        QVERIFY(combo->isEnabled());
        QVERIFY(choose(combo, QStringLiteral("B")));
        QCOMPARE(store()->binding(DeviceId::Rc28, QStringLiteral("f2"), ControlGesture::Hold).pin, QLatin1Char('B'));
        QCOMPARE(actsOn("f2:Hold")->currentData().toString(), QStringLiteral("B"));

        // Transmit always uses the transmit slice, and a pin doesn't outlive that.
        QVERIFY(choose(action("f2:Hold"), QStringLiteral("tx.mox")));
        QCOMPARE(store()->binding(DeviceId::Rc28, QStringLiteral("f2"), ControlGesture::Hold).pin, QChar());
        QVERIFY(!actsOn("f2:Hold")->isEnabled());
        QCOMPARE(actsOn("f2:Hold")->currentText(), QStringLiteral("Transmit slice"));
        QCOMPARE(actsOn("tx:Press")->currentText(), QStringLiteral("Transmit slice"));
    }

    // ---- The RC-28's stay-on ----

    void theStayOnSettingIsOnTheTxBarRow()
    {
        open();
        auto* const stays = named<QCheckBox>(QStringLiteral("knobStaysOn"));
        QVERIFY(stays != nullptr);
        // On the row itself: the cell beside its action.
        int txRow = -1;
        for (int i = 0; i < list()->rowCount(); ++i) {
            if (list()->item(i, 0)->text() == QLatin1String("TX bar")) {
                txRow = i;
            }
        }
        QVERIFY(txRow >= 0);
        QVERIFY(list()->cellWidget(txRow, 3)->isAncestorOf(stays));

        QVERIFY(!stays->isChecked());
        stays->click();
        QVERIFY(store()->rc28TxStaysOn());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Device/RC28/TxStaysOn")).toString(),
                 QStringLiteral("True"));

        // Only a hold can stay on, and it reads back after a change of device.
        QVERIFY(choose(action("tx:Press"), QStringLiteral("mute")));
        QVERIFY(!stays->isEnabled());
        QVERIFY2(stays->toolTip().contains(QStringLiteral("Hold to talk")), qPrintable(stays->toolTip()));
        QVERIFY(choose(action("tx:Press"), QStringLiteral("tx.hold")));
        QVERIFY(stays->isEnabled());
        m_page->selectDevice(DeviceId::PowerMate);
        QVERIFY(named<QCheckBox>(QStringLiteral("knobStaysOn")) == nullptr);
        m_page->selectDevice(DeviceId::Rc28);
        QVERIFY(named<QCheckBox>(QStringLiteral("knobStaysOn"))->isChecked());
    }

    // ---- Behavior ----

    void speedWritesThroughAndRightIsFaster()
    {
        open();
        auto* const slider = named<QSlider>(QStringLiteral("knobSpeed"));
        auto* const value = named<QLabel>(QStringLiteral("knobSpeedValue"));
        QCOMPARE(store()->speed(DeviceId::Rc28), 1);
        QCOMPARE(slider->value(), slider->maximum());
        QCOMPARE(value->text(), QStringLiteral("Every click"));

        slider->setValue(slider->maximum() - 2);
        QCOMPARE(store()->speed(DeviceId::Rc28), 3);
        QCOMPARE(value->text(), QStringLiteral("One step per 3 clicks"));
        slider->setValue(slider->minimum());
        QCOMPARE(store()->speed(DeviceId::Rc28), KnobsSetupPage::kSpeedSlowest);

        // Per device.
        m_page->selectDevice(DeviceId::PowerMate);
        QCOMPARE(slider->value(), slider->maximum());
        QCOMPARE(store()->speed(DeviceId::PowerMate), 1);
        m_page->selectDevice(DeviceId::Rc28);
        QCOMPARE(slider->value(), slider->minimum());
    }

    void reverseWritesThroughPerDevice()
    {
        open();
        auto* const reverse = named<QCheckBox>(QStringLiteral("knobReverse"));
        reverse->click();
        QVERIFY(store()->reversed(DeviceId::Rc28));
        QVERIFY(!store()->reversed(DeviceId::PowerMate));
        m_page->selectDevice(DeviceId::PowerMate);
        QVERIFY(!reverse->isChecked());
        m_page->selectDevice(DeviceId::Rc28);
        QVERIFY(reverse->isChecked());
    }

    void lightsReachTheHostWhereTheDeviceHasThem_data()
    {
        QTest::addColumn<int>("device");
        QTest::addColumn<QString>("text");
        QTest::newRow("RC-28") << int(DeviceId::Rc28) << QStringLiteral("Lights follow the radio");
        QTest::newRow("FlexControl") << int(DeviceId::FlexControl) << QStringLiteral("Aux lights follow the radio");
        QTest::newRow("TMate 2") << int(DeviceId::TMate2) << QStringLiteral("Show the radio on the display");
        QTest::newRow("Stream Deck+") << int(DeviceId::StreamDeckPlus) << QStringLiteral("Show the radio on keys and strip");
    }

    void lightsReachTheHostWhereTheDeviceHasThem()
    {
        QFETCH(int, device);
        QFETCH(QString, text);
        open();
        const DeviceId id = DeviceId(device);
        m_page->selectDevice(id);
        auto* const lights = named<QCheckBox>(QStringLiteral("knobLights"));
        QCOMPARE(lights->text(), text);
        QVERIFY(lights->isEnabled());
        QVERIFY(lights->isChecked());
        lights->click();
        QVERIFY(!m_host->lightsEnabled(id));
        lights->click();
        QVERIFY(m_host->lightsEnabled(id));
    }

    void aDeviceWithNoLightsDisablesTheSettingWithWhy()
    {
        open();
        for (const DeviceId id : {DeviceId::PowerMate, DeviceId::ShuttleXpress, DeviceId::ShuttlePro2, DeviceId::UlanziDial}) {
            m_page->selectDevice(id);
            auto* const lights = named<QCheckBox>(QStringLiteral("knobLights"));
            QVERIFY(lights->isVisibleTo(m_page.get()));
            QVERIFY(!lights->isEnabled());
            QVERIFY(!lights->isChecked());
            QVERIFY(!lights->toolTip().isEmpty());
        }
    }

    // ---- The drawing and the rows go together, by mouse or by Tab ----

    void aControlOnTheDrawingPicksItsRows()
    {
        open();
        const auto selectedRows = [this] {
            QList<int> rows;
            const QModelIndexList picked = list()->selectionModel()->selectedRows();
            for (const QModelIndex& index : picked) {
                rows.append(index.row());
            }
            std::sort(rows.begin(), rows.end());
            return rows;
        };
        QCOMPARE(selectedRows(), QList<int>{0});
        auto* const f1 = named<QAbstractButton>(QStringLiteral("knobCap:F1"));
        QVERIFY(f1 != nullptr);
        QCOMPARE(f1->focusPolicy() & Qt::TabFocus, Qt::TabFocus);
        f1->click();
        QVERIFY(f1->isChecked());
        QCOMPARE(selectedRows(), (QList<int>{1, 2}));

        // And the other way.
        emit list()->cellClicked(5, 0);
        QVERIFY(named<QAbstractButton>(QStringLiteral("knobCap:TX bar"))->isChecked());
        QVERIFY(!f1->isChecked());
        QCOMPARE(selectedRows(), QList<int>{5});

        // Changing a row picks it too.
        QVERIFY(choose(action("f2:Press"), QStringLiteral("mute")));
        QVERIFY(named<QAbstractButton>(QStringLiteral("knobCap:F2"))->isChecked());
    }

    void clickingADeviceShowsIt()
    {
        open();
        QAbstractButton* const row = deviceRow(DeviceId::TMate2);
        QCOMPARE(row->focusPolicy() & Qt::TabFocus, Qt::TabFocus);
        row->click();
        QCOMPARE(m_page->selectedDevice(), DeviceId::TMate2);
        QVERIFY(row->isChecked());
        QVERIFY(!deviceRow(DeviceId::Rc28)->isChecked());
        QCOMPARE(list()->rowCount(), 9);
    }

    void scrollingOverARowChangesNothing()
    {
        open();
        QComboBox* const combo = action("f1:Press");
        const QString before = combo->currentData().toString();
        QWheelEvent wheel(QPointF(5, 5), QPointF(5, 5), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(combo, &wheel);
        QCOMPARE(combo->currentData().toString(), before);
        auto* const slider = named<QSlider>(QStringLiteral("knobSpeed"));
        const int at = slider->value();
        QApplication::sendEvent(slider, &wheel);
        QCOMPARE(slider->value(), at);
    }

    // ---- The page ----

    void thePageSaysKnobsPlugIntoThisComputer()
    {
        open();
        const QString text = named<QLabel>(QStringLiteral("knobsThisComputer"))->text();
        QVERIFY2(text.contains(QStringLiteral("plug into this computer")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("remote window")), qPrintable(text));
    }

    void useShortcutsOffDisablesWithTheReason()
    {
        open();
        m_bindings->setEnabled(false);
        QVERIFY(!list()->isEnabled());
        QCOMPARE(list()->toolTip(), QStringLiteral("Use shortcuts is off."));
        QCOMPARE(state(), QStringLiteral("Use shortcuts is off."));
        QVERIFY(!named<QGroupBox>(QStringLiteral("knobsBehavior"))->isEnabled());
        // The list still shows what each device does.
        QVERIFY(deviceRow(DeviceId::PowerMate)->isEnabled());

        m_bindings->setEnabled(true);
        QVERIFY(list()->isEnabled());
        QCOMPARE(state(), QString());
    }

    void aPageWithNoWindowSaysSo()
    {
        m_page = std::make_unique<KnobsSetupPage>(nullptr, nullptr);
        QVERIFY(!list()->isEnabled());
        QVERIFY2(list()->toolTip().contains(QStringLiteral("radio window")), qPrintable(list()->toolTip()));
        QCOMPARE(stateLine(DeviceId::Rc28), QStringLiteral("Not plugged in. Untested."));
        QVERIFY(!named<QCheckBox>(QStringLiteral("knobLights"))->isChecked());
    }

    void theSetupDialogBuildsThePageWithItsHost()
    {
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        RadioModel model;
        SetupDialog dialog(&model);
        dialog.setHotKeyContext(m_bindings.get(), [](const QString&) { return QString(); });
        dialog.setControllerHost(m_host.get());
        dialog.selectPage(QStringLiteral("Knobs"));
        auto* const page = dialog.findChild<KnobsSetupPage*>();
        QVERIFY(page != nullptr);
        QCOMPARE(page->controllerHost(), m_host.get());
    }
};

QTEST_MAIN(TestHotKeySetupKnobs)
#include "tst_hot_key_setup_knobs.moc"
