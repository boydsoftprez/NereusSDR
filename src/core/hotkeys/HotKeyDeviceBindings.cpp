// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "core/hotkeys/HotKeyDeviceBindings.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/hotkeys/HotKeyActionList.h"

#include <QStringList>

#include <algorithm>
#include <array>

namespace NereusSDR {

namespace {

constexpr int kDefaultSpeed = 1;

const QString& settingsRoot()
{
    static const QString kRoot = QStringLiteral("HotKeys/Device/");
    return kRoot;
}

QString settingsPrefix(DeviceId id)
{
    return settingsRoot() + HotKeyDeviceBindings::deviceKey(id) + QLatin1Char('/');
}

QString gestureKey(ControlGesture gesture)
{
    switch (gesture) {
    case ControlGesture::Press:
        return QStringLiteral("Press");
    case ControlGesture::Hold:
        return QStringLiteral("Hold");
    case ControlGesture::Tap:
        return QStringLiteral("Tap");
    case ControlGesture::DoubleTap:
        return QStringLiteral("DoubleTap");
    case ControlGesture::Turn:
        return QStringLiteral("Turn");
    }
    return {};
}

QString bindingKey(DeviceId id, const QString& control, ControlGesture gesture)
{
    return HotKeyDeviceBindings::deviceKey(id) + QLatin1Char('/') + control + QLatin1Char('/') + gestureKey(gesture);
}

QChar pinOf(const QString& text)
{
    if (text.size() != 1) {
        return {};
    }
    const QChar letter = text.at(0).toUpper();
    return letter >= QLatin1Char('A') && letter <= QLatin1Char('D') ? letter : QChar{};
}

// The design's section 5, knob table. Plan's corrections: ShuttlePro button 15 ships unbound.
QHash<QString, HotKeyDeviceBinding> buildDefaults()
{
    using G = ControlGesture;
    QHash<QString, HotKeyDeviceBinding> rows;
    const auto press = [&rows](DeviceId id, const QString& control, const char* action, G gesture = G::Press) {
        rows.insert(bindingKey(id, control, gesture), HotKeyDeviceBinding{QString::fromLatin1(action), {}, {}});
    };
    const auto turn = [&rows](DeviceId id, const QString& control, const char* up, const char* down) {
        rows.insert(bindingKey(id, control, G::Turn),
                    HotKeyDeviceBinding{QString::fromLatin1(up), QString::fromLatin1(down), {}});
    };
    const auto buttons = [&press](DeviceId id, std::initializer_list<const char*> actions) {
        int n = 0;
        for (const char* action : actions) {
            press(id, QStringLiteral("button%1").arg(++n), action);
        }
    };
    const QString wheel = QStringLiteral("wheel");
    const QString jog = QStringLiteral("jog");
    const QString ring = QStringLiteral("ring");
    const QString knobPress = QStringLiteral("press");

    turn(DeviceId::Rc28, wheel, "tune.up", "tune.down");
    press(DeviceId::Rc28, QStringLiteral("f1"), "step.larger");
    press(DeviceId::Rc28, QStringLiteral("f1"), "slice.lock", G::Hold);
    press(DeviceId::Rc28, QStringLiteral("f2"), "step.smaller");
    press(DeviceId::Rc28, QStringLiteral("f2"), "mute", G::Hold);
    press(DeviceId::Rc28, QStringLiteral("tx"), "tx.hold");

    turn(DeviceId::PowerMate, wheel, "tune.up", "tune.down");
    press(DeviceId::PowerMate, knobPress, "mute");

    for (const DeviceId id : {DeviceId::ShuttleXpress, DeviceId::ShuttlePro2}) {
        turn(id, jog, "tune.up", "tune.down");
        turn(id, ring, "tune.up10", "tune.down10");
    }
    buttons(DeviceId::ShuttleXpress, {"mute", "slice.lock", "filter.next", "filter.previous", "step.larger"});
    buttons(DeviceId::ShuttlePro2,
            {"mute", "slice.lock", "filter.next", "filter.previous", "step.larger", "step.smaller", "bandstack.next",
             "bandstack.previous", "mode.next", "mode.previous", "zoom.in", "zoom.out", "volume.up", "volume.down"});

    turn(DeviceId::FlexControl, wheel, "tune.up", "tune.down");
    for (const QString& control : {QStringLiteral("button1"), QStringLiteral("knob")}) {
        press(DeviceId::FlexControl, control, "step.larger", G::Tap);
        press(DeviceId::FlexControl, control, "step.smaller", G::DoubleTap);
    }
    press(DeviceId::FlexControl, QStringLiteral("button2"), "tx.mox", G::Tap);
    press(DeviceId::FlexControl, QStringLiteral("button2"), "tx.tune", G::DoubleTap);
    press(DeviceId::FlexControl, QStringLiteral("button3"), "mute", G::Tap);
    press(DeviceId::FlexControl, QStringLiteral("button3"), "slice.lock", G::DoubleTap);

    turn(DeviceId::TMate2, QStringLiteral("main"), "tune.up", "tune.down");
    turn(DeviceId::TMate2, QStringLiteral("e1"), "volume.up", "volume.down");
    turn(DeviceId::TMate2, QStringLiteral("e2"), "filter.next", "filter.previous");
    buttons(DeviceId::TMate2,
            {"mute", "slice.lock", "bandstack.next", "bandstack.previous", "mode.next", "mode.previous"});

    turn(DeviceId::StreamDeckPlus, QStringLiteral("dial1"), "tune.up", "tune.down");
    turn(DeviceId::StreamDeckPlus, QStringLiteral("dial2"), "volume.up", "volume.down");
    turn(DeviceId::StreamDeckPlus, QStringLiteral("dial3"), "filter.next", "filter.previous");
    turn(DeviceId::StreamDeckPlus, QStringLiteral("dial4"), "bandstack.next", "bandstack.previous");
    buttons(DeviceId::StreamDeckPlus, {"tx.hold", "tx.mox", "tx.tune", "mute", "bandstack.next", "bandstack.previous",
                                       "filter.next", "filter.previous"});

    turn(DeviceId::UlanziDial, wheel, "tune.up", "tune.down");
    press(DeviceId::UlanziDial, knobPress, "mute");
    press(DeviceId::UlanziDial, QStringLiteral("topLeft"), "tx.mox");
    press(DeviceId::UlanziDial, QStringLiteral("topMiddle"), "rit.toggle");
    press(DeviceId::UlanziDial, QStringLiteral("topRight"), "tx.tune");
    press(DeviceId::UlanziDial, QStringLiteral("rightTop"), "slice.next");

    return rows;
}

const QHash<QString, HotKeyDeviceBinding>& defaults()
{
    static const QHash<QString, HotKeyDeviceBinding> kRows = buildDefaults();
    return kRows;
}

// The same words as MidiProfile's refusal on a knob.
QString turnRefusal(const QString& id)
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        return QStringLiteral("That action no longer exists.");
    }
    if (action->gesture == HotKeyGesture::Hold || action->transmits) {
        return QStringLiteral("A knob can't run %1. Bind it to a button.").arg(action->name);
    }
    return {};
}

const QString& staysOnKey()
{
    static const QString kKey = QStringLiteral("HotKeys/Device/RC28/TxStaysOn");
    return kKey;
}

} // namespace

HotKeyDeviceBindings::HotKeyDeviceBindings(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<DeviceId>();
    qRegisterMetaType<ControlGesture>();
    load();
}

QList<DeviceId> HotKeyDeviceBindings::devices()
{
    return {DeviceId::Rc28, DeviceId::PowerMate, DeviceId::ShuttleXpress, DeviceId::ShuttlePro2,
            DeviceId::FlexControl, DeviceId::TMate2, DeviceId::StreamDeckPlus, DeviceId::UlanziDial};
}

QString HotKeyDeviceBindings::deviceName(DeviceId id)
{
    switch (id) {
    case DeviceId::Rc28:
        return QStringLiteral("RC-28");
    case DeviceId::PowerMate:
        return QStringLiteral("PowerMate");
    case DeviceId::ShuttleXpress:
        return QStringLiteral("ShuttleXpress");
    case DeviceId::ShuttlePro2:
        return QStringLiteral("ShuttlePro v2");
    case DeviceId::FlexControl:
        return QStringLiteral("FlexControl");
    case DeviceId::TMate2:
        return QStringLiteral("TMate 2");
    case DeviceId::StreamDeckPlus:
        return QStringLiteral("Stream Deck+");
    case DeviceId::UlanziDial:
        return QStringLiteral("Ulanzi Dial");
    }
    return {};
}

QString HotKeyDeviceBindings::deviceKey(DeviceId id)
{
    switch (id) {
    case DeviceId::Rc28:
        return QStringLiteral("RC28");
    case DeviceId::PowerMate:
        return QStringLiteral("PowerMate");
    case DeviceId::ShuttleXpress:
        return QStringLiteral("ShuttleXpress");
    case DeviceId::ShuttlePro2:
        return QStringLiteral("ShuttlePro2");
    case DeviceId::FlexControl:
        return QStringLiteral("FlexControl");
    case DeviceId::TMate2:
        return QStringLiteral("TMate2");
    case DeviceId::StreamDeckPlus:
        return QStringLiteral("StreamDeckPlus");
    case DeviceId::UlanziDial:
        return QStringLiteral("UlanziDial");
    }
    return {};
}

bool HotKeyDeviceBindings::testedOnRealDevice(DeviceId /*id*/)
{
    return false;
}

QString HotKeyDeviceBindings::tmate2EncoderControl(int index)
{
    // From AetherSDR src/gui/MainWindow_Controllers.cpp:855-860 [@1e0718ad]: the LCD
    // labels the two auxiliaries in the opposite physical order. Index 2 is printed
    // E1, index 1 is printed E2.
    switch (index) {
    case 0:
        return QStringLiteral("main");
    case 1:
        return QStringLiteral("e2");
    case 2:
        return QStringLiteral("e1");
    default:
        return {};
    }
}

QString HotKeyDeviceBindings::refusalFor(ControlGesture gesture, const QString& actionId, const QString& actionIdDown)
{
    if (actionId.isEmpty()) {
        return {};      // clearing is always allowed
    }
    if (gesture == ControlGesture::Turn) {
        const QString upRefusal = turnRefusal(actionId);
        if (!upRefusal.isEmpty() || actionIdDown.isEmpty()) {
            return upRefusal;
        }
        return turnRefusal(actionIdDown);
    }
    const HotKeyAction* const action = HotKeyActionList::find(actionId);
    if (action == nullptr) {
        return QStringLiteral("That action no longer exists.");
    }
    if ((gesture == ControlGesture::Tap || gesture == ControlGesture::DoubleTap)
        && action->gesture == HotKeyGesture::Hold) {
        return QStringLiteral("A tap can't run %1, since it needs a button you can hold.").arg(action->name);
    }
    return {};
}

HotKeyDeviceBinding HotKeyDeviceBindings::binding(DeviceId id, const QString& control, ControlGesture gesture) const
{
    const QString key = bindingKey(id, control, gesture);
    const auto changed = m_changed.constFind(key);
    if (changed != m_changed.constEnd()) {
        return *changed;
    }
    return defaults().value(key);
}

void HotKeyDeviceBindings::setBinding(DeviceId id, const QString& control, ControlGesture gesture,
                                      const HotKeyDeviceBinding& binding)
{
    const QString reason = refusalFor(gesture, binding.actionId, binding.actionIdDown);
    if (!reason.isEmpty()) {
        qCWarning(lcHotKeys) << "Refused a binding on" << deviceKey(id) << control << binding.actionId
                             << binding.actionIdDown << reason;
        emit bindingRefused(id, control, gesture, reason);
        return;
    }

    HotKeyDeviceBinding stored = binding;
    stored.pin = pinOf(QString(binding.pin));

    AppSettings& settings = AppSettings::instance();
    const QString key = bindingKey(id, control, gesture);
    const QString prefix = settingsRoot() + key + QLatin1Char('/');
    if (stored == defaults().value(key)) {
        m_changed.remove(key);
        settings.remove(prefix + QStringLiteral("Action"));
        settings.remove(prefix + QStringLiteral("ActionDown"));
        settings.remove(prefix + QStringLiteral("Pin"));
    } else {
        m_changed.insert(key, stored);
        settings.setValue(prefix + QStringLiteral("Action"), stored.actionId);
        settings.setValue(prefix + QStringLiteral("ActionDown"), stored.actionIdDown);
        settings.setValue(prefix + QStringLiteral("Pin"), stored.pin.isNull() ? QString() : QString(stored.pin));
    }
    settings.save();
    emit bindingsChanged();
}

int HotKeyDeviceBindings::speed(DeviceId id) const
{
    return m_speed.value(id, kDefaultSpeed);
}

void HotKeyDeviceBindings::setSpeed(DeviceId id, int speed)
{
    const int kept = std::max(speed, 1);
    m_speed.insert(id, kept);
    AppSettings& settings = AppSettings::instance();
    settings.setValue(settingsPrefix(id) + QStringLiteral("Speed"), kept);
    settings.save();
    emit bindingsChanged();
}

bool HotKeyDeviceBindings::reversed(DeviceId id) const
{
    return m_reversed.value(id, false);
}

void HotKeyDeviceBindings::setReversed(DeviceId id, bool reversed)
{
    m_reversed.insert(id, reversed);
    AppSettings& settings = AppSettings::instance();
    settings.setValue(settingsPrefix(id) + QStringLiteral("Reversed"),
                      reversed ? QStringLiteral("True") : QStringLiteral("False"));
    settings.save();
    emit bindingsChanged();
}

bool HotKeyDeviceBindings::rc28TxStaysOn() const
{
    return m_rc28TxStaysOn;
}

void HotKeyDeviceBindings::setRc28TxStaysOn(bool on)
{
    m_rc28TxStaysOn = on;
    AppSettings& settings = AppSettings::instance();
    settings.setValue(staysOnKey(), on ? QStringLiteral("True") : QStringLiteral("False"));
    settings.save();
    emit bindingsChanged();
}

void HotKeyDeviceBindings::load()
{
    const AppSettings& settings = AppSettings::instance();
    for (const DeviceId id : devices()) {
        const QString prefix = settingsPrefix(id);
        const int savedSpeed = settings.value(prefix + QStringLiteral("Speed"), kDefaultSpeed).toInt();
        m_speed.insert(id, std::max(savedSpeed, 1));
        m_reversed.insert(id, settings.value(prefix + QStringLiteral("Reversed")).toString() == QLatin1String("True"));
    }
    m_rc28TxStaysOn = settings.value(staysOnKey()).toString() == QLatin1String("True");

    // HotKeys/Device/<device>/<control>/<gesture>/Action
    constexpr std::array<ControlGesture, 5> kGestures = {ControlGesture::Press, ControlGesture::Hold,
                                                         ControlGesture::Tap, ControlGesture::DoubleTap,
                                                         ControlGesture::Turn};
    const QString actionSuffix = QStringLiteral("/Action");
    const QStringList keys = settings.allKeys();
    for (const QString& full : keys) {
        if (!full.startsWith(settingsRoot()) || !full.endsWith(actionSuffix)) {
            continue;
        }
        const QString rowPath = full.mid(settingsRoot().size());
        const QString key = rowPath.chopped(actionSuffix.size());
        const QStringList parts = key.split(QLatin1Char('/'));
        if (parts.size() != 3) {
            continue;
        }
        const QList<DeviceId> known = devices();
        const auto device = std::find_if(known.cbegin(), known.cend(), [&parts](DeviceId id) {
            return deviceKey(id) == parts.at(0);
        });
        const auto gesture = std::find_if(kGestures.cbegin(), kGestures.cend(), [&parts](ControlGesture g) {
            return gestureKey(g) == parts.at(2);
        });
        const QString prefix = settingsRoot() + key + QLatin1Char('/');
        HotKeyDeviceBinding saved;
        saved.actionId = settings.value(full).toString();
        saved.actionIdDown = settings.value(prefix + QStringLiteral("ActionDown")).toString();
        saved.pin = pinOf(settings.value(prefix + QStringLiteral("Pin")).toString());
        // Design 10: a binding for an action that no longer exists is dropped.
        const QString reason = device == known.cend() || gesture == kGestures.cend()
            ? QStringLiteral("No such control.")
            : refusalFor(*gesture, saved.actionId, saved.actionIdDown);
        if (!reason.isEmpty()) {
            qCWarning(lcHotKeys) << "Dropped a saved binding:" << key << saved.actionId << saved.actionIdDown << reason;
            continue;
        }
        m_changed.insert(key, saved);
    }
}

} // namespace NereusSDR
