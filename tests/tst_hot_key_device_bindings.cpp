// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// The knob defaults of the design's section 5, the changes a user makes, and what
// a Turn control refuses. Linked to the Core alone: this holds no device code.
#include <QtTest>

#include <QRegularExpression>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyDeviceBindings.h"

using namespace NereusSDR;

namespace {

using G = ControlGesture;

struct Row {
    DeviceId device;
    const char* control;
    G gesture;
    const char* action;
    const char* actionDown;
};

// Section 5, line for line.
const QList<Row>& sectionFive()
{
    static const QList<Row> kRows = {
        {DeviceId::Rc28, "wheel", G::Turn, "tune.up", "tune.down"},
        {DeviceId::Rc28, "f1", G::Press, "step.larger", ""},
        {DeviceId::Rc28, "f1", G::Hold, "slice.lock", ""},
        {DeviceId::Rc28, "f2", G::Press, "step.smaller", ""},
        {DeviceId::Rc28, "f2", G::Hold, "mute", ""},
        {DeviceId::Rc28, "tx", G::Press, "tx.hold", ""},

        {DeviceId::PowerMate, "wheel", G::Turn, "tune.up", "tune.down"},
        {DeviceId::PowerMate, "press", G::Press, "mute", ""},

        {DeviceId::ShuttleXpress, "jog", G::Turn, "tune.up", "tune.down"},
        {DeviceId::ShuttleXpress, "ring", G::Turn, "tune.up10", "tune.down10"},
        {DeviceId::ShuttleXpress, "button1", G::Press, "mute", ""},
        {DeviceId::ShuttleXpress, "button2", G::Press, "slice.lock", ""},
        {DeviceId::ShuttleXpress, "button3", G::Press, "filter.next", ""},
        {DeviceId::ShuttleXpress, "button4", G::Press, "filter.previous", ""},
        {DeviceId::ShuttleXpress, "button5", G::Press, "step.larger", ""},

        {DeviceId::ShuttlePro2, "jog", G::Turn, "tune.up", "tune.down"},
        {DeviceId::ShuttlePro2, "ring", G::Turn, "tune.up10", "tune.down10"},
        {DeviceId::ShuttlePro2, "button1", G::Press, "mute", ""},
        {DeviceId::ShuttlePro2, "button2", G::Press, "slice.lock", ""},
        {DeviceId::ShuttlePro2, "button3", G::Press, "filter.next", ""},
        {DeviceId::ShuttlePro2, "button4", G::Press, "filter.previous", ""},
        {DeviceId::ShuttlePro2, "button5", G::Press, "step.larger", ""},
        {DeviceId::ShuttlePro2, "button6", G::Press, "step.smaller", ""},
        {DeviceId::ShuttlePro2, "button7", G::Press, "bandstack.next", ""},
        {DeviceId::ShuttlePro2, "button8", G::Press, "bandstack.previous", ""},
        {DeviceId::ShuttlePro2, "button9", G::Press, "mode.next", ""},
        {DeviceId::ShuttlePro2, "button10", G::Press, "mode.previous", ""},
        {DeviceId::ShuttlePro2, "button11", G::Press, "zoom.in", ""},
        {DeviceId::ShuttlePro2, "button12", G::Press, "zoom.out", ""},
        {DeviceId::ShuttlePro2, "button13", G::Press, "volume.up", ""},
        {DeviceId::ShuttlePro2, "button14", G::Press, "volume.down", ""},

        {DeviceId::FlexControl, "wheel", G::Turn, "tune.up", "tune.down"},
        {DeviceId::FlexControl, "button1", G::Tap, "step.larger", ""},
        {DeviceId::FlexControl, "button1", G::DoubleTap, "step.smaller", ""},
        {DeviceId::FlexControl, "knob", G::Tap, "step.larger", ""},
        {DeviceId::FlexControl, "knob", G::DoubleTap, "step.smaller", ""},
        {DeviceId::FlexControl, "button2", G::Tap, "tx.mox", ""},
        {DeviceId::FlexControl, "button2", G::DoubleTap, "tx.tune", ""},
        {DeviceId::FlexControl, "button3", G::Tap, "mute", ""},
        {DeviceId::FlexControl, "button3", G::DoubleTap, "slice.lock", ""},

        {DeviceId::TMate2, "main", G::Turn, "tune.up", "tune.down"},
        {DeviceId::TMate2, "e1", G::Turn, "volume.up", "volume.down"},
        {DeviceId::TMate2, "e2", G::Turn, "filter.next", "filter.previous"},
        {DeviceId::TMate2, "button1", G::Press, "mute", ""},
        {DeviceId::TMate2, "button2", G::Press, "slice.lock", ""},
        {DeviceId::TMate2, "button3", G::Press, "bandstack.next", ""},
        {DeviceId::TMate2, "button4", G::Press, "bandstack.previous", ""},
        {DeviceId::TMate2, "button5", G::Press, "mode.next", ""},
        {DeviceId::TMate2, "button6", G::Press, "mode.previous", ""},

        {DeviceId::StreamDeckPlus, "dial1", G::Turn, "tune.up", "tune.down"},
        {DeviceId::StreamDeckPlus, "dial2", G::Turn, "volume.up", "volume.down"},
        {DeviceId::StreamDeckPlus, "dial3", G::Turn, "filter.next", "filter.previous"},
        {DeviceId::StreamDeckPlus, "dial4", G::Turn, "bandstack.next", "bandstack.previous"},
        {DeviceId::StreamDeckPlus, "button1", G::Press, "tx.hold", ""},
        {DeviceId::StreamDeckPlus, "button2", G::Press, "tx.mox", ""},
        {DeviceId::StreamDeckPlus, "button3", G::Press, "tx.tune", ""},
        {DeviceId::StreamDeckPlus, "button4", G::Press, "mute", ""},
        {DeviceId::StreamDeckPlus, "button5", G::Press, "bandstack.next", ""},
        {DeviceId::StreamDeckPlus, "button6", G::Press, "bandstack.previous", ""},
        {DeviceId::StreamDeckPlus, "button7", G::Press, "filter.next", ""},
        {DeviceId::StreamDeckPlus, "button8", G::Press, "filter.previous", ""},

        {DeviceId::UlanziDial, "wheel", G::Turn, "tune.up", "tune.down"},
        {DeviceId::UlanziDial, "press", G::Press, "mute", ""},
        {DeviceId::UlanziDial, "topLeft", G::Press, "tx.mox", ""},
        {DeviceId::UlanziDial, "topMiddle", G::Press, "rit.toggle", ""},
        {DeviceId::UlanziDial, "topRight", G::Press, "tx.tune", ""},
        {DeviceId::UlanziDial, "rightTop", G::Press, "slice.next", ""},
    };
    return kRows;
}

const HotKeyDeviceBinding kVolume{QStringLiteral("volume.up"), QStringLiteral("volume.down"), {}};

} // namespace

class TestHotKeyDeviceBindings : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
    }

    void everyRowOfTheKnobTable()
    {
        const HotKeyDeviceBindings bindings;
        for (const Row& row : sectionFive()) {
            const HotKeyDeviceBinding got = bindings.binding(row.device, QLatin1String(row.control), row.gesture);
            const QByteArray where = HotKeyDeviceBindings::deviceKey(row.device).toLatin1() + ' ' + row.control;
            QVERIFY2(got.actionId == QLatin1String(row.action), where.constData());
            QVERIFY2(got.actionIdDown == QLatin1String(row.actionDown), where.constData());
            QVERIFY2(got.pin.isNull(), where.constData());
            QVERIFY2(HotKeyActionList::find(got.actionId) != nullptr, where.constData());
            // Every default is one its control is allowed.
            QVERIFY2(HotKeyDeviceBindings::refusalFor(row.gesture, got.actionId, got.actionIdDown).isEmpty(),
                     where.constData());
        }
    }

    void shuttleProButton15ShipsUnbound()
    {
        const HotKeyDeviceBindings bindings;
        QVERIFY(bindings.binding(DeviceId::ShuttlePro2, QStringLiteral("button15"), G::Press).actionId.isEmpty());
    }

    void tmate2LabelsFollowPrintedNames()
    {
        QCOMPARE(HotKeyDeviceBindings::tmate2EncoderControl(0), QStringLiteral("main"));
        QCOMPARE(HotKeyDeviceBindings::tmate2EncoderControl(2), QStringLiteral("e1"));
        QCOMPARE(HotKeyDeviceBindings::tmate2EncoderControl(1), QStringLiteral("e2"));
    }

    void noDeviceIsMarkedTested()
    {
        const QList<DeviceId> devices = HotKeyDeviceBindings::devices();
        QCOMPARE(devices.size(), 8);
        for (const DeviceId id : devices) {
            QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(id));
            QVERIFY(!HotKeyDeviceBindings::deviceName(id).isEmpty());
        }
        QCOMPARE(HotKeyDeviceBindings::deviceName(DeviceId::ShuttlePro2), QStringLiteral("ShuttlePro v2"));
    }

    void overridesRoundTrip()
    {
        {
            HotKeyDeviceBindings bindings;
            bindings.setBinding(DeviceId::PowerMate, QStringLiteral("wheel"), G::Turn,
                                HotKeyDeviceBinding{QStringLiteral("volume.up"), QStringLiteral("volume.down"),
                                                    QLatin1Char('B')});
            bindings.setBinding(DeviceId::Rc28, QStringLiteral("f1"), G::Hold,
                                HotKeyDeviceBinding{QStringLiteral("tx.hold"), {}, {}});
            bindings.setSpeed(DeviceId::PowerMate, 4);
            bindings.setReversed(DeviceId::PowerMate, true);
            bindings.setRc28TxStaysOn(true);
        }
        const HotKeyDeviceBindings again;
        const HotKeyDeviceBinding wheel = again.binding(DeviceId::PowerMate, QStringLiteral("wheel"), G::Turn);
        QCOMPARE(wheel.actionId, QStringLiteral("volume.up"));
        QCOMPARE(wheel.actionIdDown, QStringLiteral("volume.down"));
        QCOMPARE(wheel.pin, QLatin1Char('B'));
        QCOMPARE(again.binding(DeviceId::Rc28, QStringLiteral("f1"), G::Hold).actionId, QStringLiteral("tx.hold"));
        QCOMPARE(again.speed(DeviceId::PowerMate), 4);
        QVERIFY(again.reversed(DeviceId::PowerMate));
        QVERIFY(again.rc28TxStaysOn());
        // Others untouched.
        QCOMPARE(again.speed(DeviceId::Rc28), 1);
        QVERIFY(!again.reversed(DeviceId::Rc28));
    }

    void settingsHoldOnlyWhatDiffers()
    {
        HotKeyDeviceBindings bindings;
        const QString control = QStringLiteral("button1");
        bindings.setBinding(DeviceId::TMate2, control, G::Press, HotKeyDeviceBinding{QStringLiteral("zoom.in"), {}, {}});
        const QString key = QStringLiteral("HotKeys/Device/TMate2/button1/Press/Action");
        QCOMPARE(AppSettings::instance().value(key).toString(), QStringLiteral("zoom.in"));
        bindings.setBinding(DeviceId::TMate2, control, G::Press, HotKeyDeviceBinding{QStringLiteral("mute"), {}, {}});
        QVERIFY(!AppSettings::instance().contains(key));
    }

    void clearingKeepsAcrossALoad()
    {
        {
            HotKeyDeviceBindings bindings;
            bindings.setBinding(DeviceId::PowerMate, QStringLiteral("press"), G::Press, HotKeyDeviceBinding{});
        }
        const HotKeyDeviceBindings again;
        QVERIFY(again.binding(DeviceId::PowerMate, QStringLiteral("press"), G::Press).actionId.isEmpty());
    }

    void aTurnControlRefusesHoldAndTransmit_data()
    {
        QTest::addColumn<QString>("up");
        QTest::addColumn<QString>("down");
        QTest::newRow("hold") << QStringLiteral("tx.hold") << QString();
        QTest::newRow("mox") << QStringLiteral("tx.mox") << QStringLiteral("tune.down");
        QTest::newRow("tune") << QStringLiteral("tune.up") << QStringLiteral("tx.tune");
        QTest::newRow("cw key") << QStringLiteral("tune.up") << QStringLiteral("cw.dot");
        QTest::newRow("cw message") << QStringLiteral("cw.msg1") << QString();
    }

    void aTurnControlRefusesHoldAndTransmit()
    {
        QFETCH(QString, up);
        QFETCH(QString, down);
        HotKeyDeviceBindings bindings;
        QSignalSpy refused(&bindings, &HotKeyDeviceBindings::bindingRefused);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Refused a binding")));
        bindings.setBinding(DeviceId::PowerMate, QStringLiteral("wheel"), G::Turn, HotKeyDeviceBinding{up, down, {}});
        QCOMPARE(refused.size(), 1);
        QVERIFY(refused.at(0).at(3).toString().contains(QStringLiteral("can't run")));
        QCOMPARE(bindings.binding(DeviceId::PowerMate, QStringLiteral("wheel"), G::Turn).actionId,
                 QStringLiteral("tune.up"));
    }

    void aTurnControlTakesWhatAKnobCan()
    {
        HotKeyDeviceBindings bindings;
        QSignalSpy refused(&bindings, &HotKeyDeviceBindings::bindingRefused);
        bindings.setBinding(DeviceId::PowerMate, QStringLiteral("wheel"), G::Turn, kVolume);
        QCOMPARE(refused.size(), 0);
        QCOMPARE(bindings.binding(DeviceId::PowerMate, QStringLiteral("wheel"), G::Turn), kVolume);
    }

    void aButtonTakesAHold()
    {
        QVERIFY(HotKeyDeviceBindings::refusalFor(G::Press, QStringLiteral("tx.hold"), {}).isEmpty());
        QVERIFY(HotKeyDeviceBindings::refusalFor(G::Hold, QStringLiteral("cw.straight"), {}).isEmpty());
    }

    void aTapRefusesAHoldSinceNoReleaseFollows()
    {
        QVERIFY(!HotKeyDeviceBindings::refusalFor(G::Tap, QStringLiteral("tx.hold"), {}).isEmpty());
        QVERIFY(!HotKeyDeviceBindings::refusalFor(G::DoubleTap, QStringLiteral("cw.dot"), {}).isEmpty());
        QVERIFY(HotKeyDeviceBindings::refusalFor(G::Tap, QStringLiteral("tx.mox"), {}).isEmpty());
    }

    void aBindingForAnActionThatIsGoneDrops()
    {
        AppSettings& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("HotKeys/Device/PowerMate/press/Press/Action"), QStringLiteral("no.such"));
        settings.setValue(QStringLiteral("HotKeys/Device/PowerMate/wheel/Turn/Action"), QStringLiteral("tx.hold"));
        settings.setValue(QStringLiteral("HotKeys/Device/Nothing/press/Press/Action"), QStringLiteral("mute"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Dropped a saved binding")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Dropped a saved binding")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Dropped a saved binding")));
        const HotKeyDeviceBindings bindings;
        QCOMPARE(bindings.binding(DeviceId::PowerMate, QStringLiteral("press"), G::Press).actionId, QStringLiteral("mute"));
        QCOMPARE(bindings.binding(DeviceId::PowerMate, QStringLiteral("wheel"), G::Turn).actionId, QStringLiteral("tune.up"));
    }

    void speedStaysAtLeastOne()
    {
        HotKeyDeviceBindings bindings;
        bindings.setSpeed(DeviceId::Rc28, 0);
        QCOMPARE(bindings.speed(DeviceId::Rc28), 1);
    }
};

QTEST_GUILESS_MAIN(TestHotKeyDeviceBindings)
#include "tst_hot_key_device_bindings.moc"
