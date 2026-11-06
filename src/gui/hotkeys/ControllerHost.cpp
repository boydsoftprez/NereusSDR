// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12: the USB worker, gestures
//                and device routing. AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 14: displays and lit keys from
//                the state feed. AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 15: the FlexControl on the USB
//                worker. AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 16: the Ulanzi Dial.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 17: every device's lights from
//                the state feed. AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 19: a plugin takes the Stream Deck+.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/ControllerHost.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyStateFeed.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/DeviceOutput.h"
#include "gui/hotkeys/devices/FlexControlInput.h"
#include "gui/hotkeys/devices/Rc28Parser.h"
#include "gui/hotkeys/devices/UlanziDialInput.h"
#include "gui/hotkeys/midi/MidiInput.h"

#include <QColor>
#include <QMetaObject>
#include <QStringList>

#include <algorithm>
#include <array>
#include <utility>

namespace NereusSDR {

namespace {

const QString& idTxHold()
{
    static const QString kId = QStringLiteral("tx.hold");
    return kId;
}

bool isHoldAction(const QString& id)
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    return action != nullptr && action->gesture == HotKeyGesture::Hold;
}

const QString& deviceKey()
{
    static const QString kKey = QStringLiteral("HotKeys/Midi/Device");
    return kKey;
}

// From AetherSDR src/gui/MainWindow_Controllers.cpp:1291 [@1e0718ad]
const QColor kDeckKeyIdle(20, 28, 45);
// This design's: upstream's keys don't light. Red for what transmits, green otherwise.
const QColor kDeckKeyLitTransmit(170, 30, 30);
const QColor kDeckKeyLit(20, 110, 40);
// This design's: lights turned off leave a key dark, with no name on it.
const QColor kDeckKeyOff(0, 0, 0);

// Every device with a light or a display, in the order a pass reaches them.
constexpr std::array<DeviceId, 4> kLitDevices = {DeviceId::Rc28, DeviceId::FlexControl, DeviceId::TMate2,
                                                 DeviceId::StreamDeckPlus};

constexpr int kFlexAuxButtons = 3;

QString lightsKey(DeviceId id)
{
    return QStringLiteral("HotKeys/Device/%1/Lights").arg(HotKeyDeviceBindings::deviceKey(id));
}

bool isRc28FKey(const QString& control)
{
    return control == QLatin1String("f1") || control == QLatin1String("f2");
}

bool isTransmitAction(const QString& id)
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    return action != nullptr && action->transmits;
}

bool isLevelAction(const QString& id)
{
    return id == QLatin1String("volume.up") || id == QLatin1String("volume.down");
}

} // namespace

ControllerHost::ControllerHost(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, QObject* parent)
    : QObject(parent)
    , m_dispatcher(dispatcher)
    , m_midi(new MidiInput)     // no parent: it moves to its own thread, finished deletes it
    , m_usb(new DeviceInput)    // the same
    , m_flex(new FlexControlInput)  // the same
    , m_ulanzi(UlanziDialInput::create(UlanziDialInput::staysOnMainThread() ? this : nullptr))
    , m_deviceBindings(std::make_unique<HotKeyDeviceBindings>())
{
    m_midiThread.setObjectName(QStringLiteral("NereusMidi"));
    m_midi->moveToThread(&m_midiThread);
    connect(&m_midiThread, &QThread::finished, m_midi, &QObject::deleteLater);

    connect(m_midi, &MidiInput::controlEvent, this, &ControllerHost::onMidiControlEvent, Qt::QueuedConnection);
    connect(m_midi, &MidiInput::portOpened, this, &ControllerHost::onMidiPortOpened, Qt::QueuedConnection);
    connect(m_midi, &MidiInput::connectionChanged, this, &ControllerHost::onMidiConnectionChanged,
            Qt::QueuedConnection);

    // The dispatcher has already let go of every source. A release after this finds
    // no entry, and one lost in sleep can't eat the next press.
    connect(m_dispatcher, &HotKeyDispatcher::holdsDroppedBySleep, this, &ControllerHost::forgetButtons);
    connect(m_dispatcher, &HotKeyDispatcher::holdsDroppedBySleep, this, &ControllerHost::forgetAllDevices);
    connect(bindings, &HotKeyBindings::enabledChanged, this, [this](bool on) {
        if (!on) {
            forgetButtons();
            forgetAllDevices();
        }
    });
    // A stay-on hold whose key went off (a time-out, say) no longer keys. The next
    // press keys again, rather than only letting go of a hold nobody can hear.
    connect(m_dispatcher, &HotKeyDispatcher::transmitKeyWentOff, this, [this] {
        const QSet<DeviceId> latched = std::exchange(m_txLatched, {});
        for (const DeviceId id : latched) {
            m_dispatcher->release(idTxHold(), deviceSource(id));
        }
    });

    m_usbThread.setObjectName(QStringLiteral("NereusUsb"));
    if (!UlanziDialInput::staysOnMainThread()) {
        m_ulanzi->moveToThread(&m_usbThread);
        // Connected ahead of m_usb's: its handles close before its layer calls hid_exit.
        connect(&m_usbThread, &QThread::finished, m_ulanzi, &QObject::deleteLater);
    }
    m_usb->moveToThread(&m_usbThread);
    connect(&m_usbThread, &QThread::finished, m_usb, &QObject::deleteLater);
    connect(m_usb, &DeviceInput::deviceEvent, this, &ControllerHost::onDeviceEvent, Qt::QueuedConnection);
    connect(m_usb, &DeviceInput::deviceStateChanged, this, &ControllerHost::onDeviceStateChanged, Qt::QueuedConnection);
    m_flex->moveToThread(&m_usbThread);
    connect(&m_usbThread, &QThread::finished, m_flex, &QObject::deleteLater);
    connect(m_flex, &FlexControlInput::deviceEvent, this, &ControllerHost::onDeviceEvent, Qt::QueuedConnection);
    connect(m_flex, &FlexControlInput::deviceStateChanged, this, &ControllerHost::onDeviceStateChanged,
            Qt::QueuedConnection);
    connect(m_ulanzi, &UlanziDialInput::deviceEvent, this, &ControllerHost::onDeviceEvent, Qt::QueuedConnection);
    connect(m_ulanzi, &UlanziDialInput::deviceStateChanged, this, &ControllerHost::onDeviceStateChanged,
            Qt::QueuedConnection);

    m_deviceClockStart.start();
    m_deviceClock = [this] { return m_deviceClockStart.elapsed(); };
    m_gestureTimer.setInterval(kGestureTickMs);
    connect(&m_gestureTimer, &QTimer::timeout, this, &ControllerHost::onGestureTick);
    m_ringTimer.setInterval(kRingRepeatMs);
    connect(&m_ringTimer, &QTimer::timeout, this, &ControllerHost::onRingTick);
    m_displayTimer.setSingleShot(true);
    connect(&m_displayTimer, &QTimer::timeout, this, &ControllerHost::onDisplayTick);
    connect(m_deviceBindings.get(), &HotKeyDeviceBindings::bindingsChanged, this, [this] {
        refreshDeckKeys();
        markOutputDirty(DeviceId::Rc28);
        markOutputDirty(DeviceId::FlexControl);
        pumpOutputs();
    });
    const AppSettings& settings = AppSettings::instance();
    for (const DeviceId id : kLitDevices) {
        if (settings.value(lightsKey(id)).toString() == QLatin1String("False")) {
            m_lightsOff.insert(id);
        }
    }

    m_midiThread.start();
    m_usbThread.start();
    QMetaObject::invokeMethod(m_usb, &DeviceInput::start, Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_flex, &FlexControlInput::start, Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_ulanzi, &UlanziDialInput::start, Qt::QueuedConnection);

    const QString active = MidiProfile::activeName();
    if (!active.isEmpty()) {
        MidiProfile profile(active);
        profile.load();
        setMidiProfile(profile);
    }
    const QString device = savedMidiDevice();
    if (!device.isEmpty()) {
        MidiInput* const midi = m_midi;
        QMetaObject::invokeMethod(midi, [midi, device] { midi->openPort(device); }, Qt::QueuedConnection);
    }
}

ControllerHost::~ControllerHost()
{
    m_midiThread.quit();
    m_midiThread.wait();
    m_usbThread.quit();
    m_usbThread.wait();
}

void ControllerHost::setMidiProfile(const MidiProfile& profile)
{
    m_profile = profile;
    m_routes.clear();
    m_absoluteCarry.clear();

    QStringList relative;
    const QList<MidiBinding>& bindings = profile.bindings();
    for (const MidiBinding& binding : bindings) {
        const QString reason = MidiProfile::refusalFor(binding.kind, binding.actionId, binding.actionIdDown);
        if (!reason.isEmpty()) {
            qCWarning(lcHotKeys) << "Refused a MIDI binding:" << binding.controlKey << binding.actionId
                                 << binding.actionIdDown << reason;
            emit midiBindingRefused(binding.controlKey, reason);
            continue;
        }
        m_routes.insert(binding.controlKey, binding);
        if (binding.kind == MidiControlKind::RelativeKnob) {
            relative.append(binding.controlKey);
        }
    }
    MidiInput* const midi = m_midi;
    QMetaObject::invokeMethod(midi, [midi, relative] { midi->setRelativeControls(relative); }, Qt::QueuedConnection);
}

void ControllerHost::setMidiDevice(const QString& name)
{
    AppSettings& settings = AppSettings::instance();
    settings.setValue(deviceKey(), name);
    settings.save();
    MidiInput* const midi = m_midi;
    QMetaObject::invokeMethod(
        midi,
        [midi, name] {
            if (name.isEmpty()) {
                midi->closePort();
            } else {
                midi->openPort(name);
            }
        },
        Qt::QueuedConnection);
}

QString ControllerHost::savedMidiDevice()
{
    return AppSettings::instance().value(deviceKey()).toString();
}

void ControllerHost::onMidiPortOpened(const QString& portName)
{
    m_midiSource.instance = portName;
}

void ControllerHost::onMidiConnectionChanged(bool connected, const QString& /*reason*/)
{
    if (connected || m_midiSource.instance.isEmpty()) {
        return;
    }
    // The release can't arrive now. Only this device's holds drop.
    m_dispatcher->releaseAll(m_midiSource);
    forgetButtons();
    m_absoluteCarry.clear();
    m_midiSource.instance.clear();
}

void ControllerHost::onMidiControlEvent(const QString& controlKey, MidiControlKind /*kind*/, int value, int delta)
{
    if (m_midiSource.instance.isEmpty()) {
        return;     // no device: a press here could never be released
    }
    // Before the profile: a press ends what it started, even if the profile has
    // changed since.
    if (value == 0 && m_buttonsDown.contains(controlKey)) {
        releaseButton(controlKey);
        return;
    }
    const auto route = m_routes.constFind(controlKey);
    if (route == m_routes.constEnd()) {
        return;
    }
    switch (route->kind) {
    case MidiControlKind::Button:
        if (value > 0 && !m_buttonsDown.contains(controlKey)) {
            m_buttonsDown.insert(controlKey, route->actionId);
            m_dispatcher->press(route->actionId, m_midiSource, route->pin);
        }
        break;
    case MidiControlKind::RelativeKnob:
        routeTurn(*route, delta);
        break;
    case MidiControlKind::AbsoluteKnob:
        routeAbsolute(*route, delta);
        break;
    }
}

void ControllerHost::releaseButton(const QString& controlKey)
{
    const QString id = m_buttonsDown.take(controlKey);
    const HotKeyAction* const action = HotKeyActionList::find(id);
    // Two buttons on one hold: it ends with the last.
    if (action != nullptr && action->gesture == HotKeyGesture::Hold
        && !m_buttonsDown.values().contains(id)) {
        m_dispatcher->release(id, m_midiSource);
    }
}

void ControllerHost::forgetButtons()
{
    m_buttonsDown.clear();
}

void ControllerHost::routeAbsolute(const MidiBinding& binding, int delta)
{
    if (delta == 0) {
        return;
    }
    // The part of a step left over stays for the next message, so a slow fader
    // loses nothing and comes back to where it started. A pitch bend counts in the
    // same units as a CC.
    const bool level = isLevelAction(binding.actionId) || isLevelAction(binding.actionIdDown);
    const int stepsPerRange = level ? kLevelStepsPerRange : kMidiValueMax;
    const int range = midiControlRange(binding.controlKey);
    int& carry = m_absoluteCarry[binding.controlKey];
    carry += delta * stepsPerRange;
    const int steps = carry / range;
    carry -= steps * range;
    routeTurn(binding, steps);
}

void ControllerHost::routeTurn(const MidiBinding& binding, int steps)
{
    if (steps == 0 || (steps > 0 ? binding.actionId : binding.actionIdDown).isEmpty()) {
        return;
    }
    m_dispatcher->turn(binding.actionId, binding.actionIdDown, steps, m_midiSource, binding.pin);
}

HotKeySource ControllerHost::deviceSource(DeviceId id)
{
    return HotKeySource{HotKeySourceKind::Knob, HotKeyDeviceBindings::deviceKey(id)};
}

void ControllerHost::onDeviceStateChanged(DeviceId id, bool connected, const QString& reason)
{
    m_deviceStates.insert(id, DeviceState{connected, reason});
    if (!connected) {
        // The release can't arrive now. Only this device's holds drop.
        m_dispatcher->releaseAll(deviceSource(id));
        forgetDevice(id);
    }
    emit deviceStateChanged(id, connected, reason);
    if (!connected) {
        pumpOutputs();  // lets go of whatever was waiting for it
        return;
    }
    // A device just opened shows nothing of ours, so it gets everything.
    if (id == DeviceId::TMate2) {
        m_lastTMateFrame.clear();
    } else if (id == DeviceId::StreamDeckPlus) {
        m_deckKeys = {};
        m_lastDeckStrip.clear();
        refreshDeckKeys();
    } else if (id == DeviceId::Rc28) {
        // Written at each open whatever it last said, as upstream
        // (AetherSDR src/gui/MainWindow_Controllers.cpp:2683 [@1e0718ad]).
        m_lastRc28Report.clear();
    }
    if (hasLights(id)) {
        markOutputDirty(id);
        pumpOutputs();
    }
}

void ControllerHost::setStreamDeckClaimedByPlugin(bool claimed)
{
    DeviceInput* const usb = m_usb;
    QMetaObject::invokeMethod(
        usb, [usb, claimed] { usb->setDeviceClaimedElsewhere(DeviceId::StreamDeckPlus, claimed); },
        Qt::QueuedConnection);
}

void ControllerHost::onDeviceEvent(DeviceId id, const DeviceEvent& event)
{
    // Ahead of the off switch: this is about the button, not what it runs.
    if (id == DeviceId::Rc28) {
        noteRc28Button(event);
    }
    // The off switch has already let go of everything and forgotten it, so an up
    // here has nothing to end. A down here must leave nothing to end.
    if (!m_dispatcher->enabled()) {
        return;
    }
    switch (event.kind) {
    case DeviceEvent::Kind::ButtonDown:
        deviceButtonDown(id, event);
        break;
    case DeviceEvent::Kind::ButtonUp:
        deviceButtonUp(id, event);
        break;
    case DeviceEvent::Kind::Rotate:
        deviceRotate(id, event);
        break;
    case DeviceEvent::Kind::RingPosition:
        deviceRing(id, event);
        break;
    case DeviceEvent::Kind::TokenGesture:
        // The device decided the gesture itself, so no release follows it.
        runGesture(id, event.control, event.gesture, false);
        break;
    }
}

bool ControllerHost::usesGestureEngine(DeviceId id, const QString& control) const
{
    // A Hold action on the press row keys while the button is down, so it can't
    // wait to see what the press becomes. It would otherwise key and let go at once.
    return !m_deviceBindings->binding(id, control, ControlGesture::Hold).actionId.isEmpty()
        && !isHoldAction(m_deviceBindings->binding(id, control, ControlGesture::Press).actionId);
}

void ControllerHost::deviceButtonDown(DeviceId id, const DeviceEvent& event)
{
    if (toggleTxLatch(id, event.control)) {
        return;
    }
    if (usesGestureEngine(id, event.control)) {
        m_gestures[id].onEvent(event, m_deviceClock());
        updateGestureTimer();
        return;
    }
    runGesture(id, event.control, ControlGesture::Press, true);
}

void ControllerHost::deviceButtonUp(DeviceId id, const DeviceEvent& event)
{
    const auto engine = m_gestures.find(id);
    if (engine != m_gestures.end()) {
        const QList<DeviceEvent> gestures = engine->onEvent(event, m_deviceClock());
        for (const DeviceEvent& gesture : gestures) {
            runGesture(id, gesture.control, gesture.gesture, true);
        }
    }
    updateGestureTimer();
    // After the gestures: a hold first seen at the up still has its key lifted.
    releaseDeviceControl(id, event.control);
}

void ControllerHost::onGestureTick()
{
    const qint64 now = m_deviceClock();
    const QList<DeviceId> ids = m_gestures.keys();
    for (const DeviceId id : ids) {
        const QList<DeviceEvent> gestures = m_gestures[id].onTick(now);
        for (const DeviceEvent& gesture : gestures) {
            runGesture(id, gesture.control, gesture.gesture, true);
        }
    }
    updateGestureTimer();
}

void ControllerHost::updateGestureTimer()
{
    bool waiting = false;
    for (const GestureEngine& engine : std::as_const(m_gestures)) {
        waiting = waiting || engine.waiting();
    }
    if (waiting && !m_gestureTimer.isActive()) {
        m_gestureTimer.start();
    } else if (!waiting) {
        m_gestureTimer.stop();
    }
}

void ControllerHost::runGesture(DeviceId id, const QString& control, ControlGesture gesture, bool canRelease)
{
    runBinding(id, control, m_deviceBindings->binding(id, control, gesture), canRelease);
}

void ControllerHost::runBinding(DeviceId id, const QString& control, const HotKeyDeviceBinding& binding, bool canRelease)
{
    if (binding.actionId.isEmpty()) {
        return;
    }
    if (isHoldAction(binding.actionId)) {
        if (!canRelease) {
            qCWarning(lcHotKeys) << "No release can end this hold, so it stays off:" << control << binding.actionId;
            return;
        }
        m_deviceHolds.insert(DeviceControl{id, control}, binding.actionId);
    }
    m_dispatcher->press(binding.actionId, deviceSource(id), binding.pin);
}

void ControllerHost::releaseDeviceControl(DeviceId id, const QString& control)
{
    const QString held = m_deviceHolds.take(DeviceControl{id, control});
    if (held.isEmpty()) {
        return;
    }
    // Two buttons on one hold: it ends with the last.
    for (auto it = m_deviceHolds.constBegin(); it != m_deviceHolds.constEnd(); ++it) {
        if (it.key().first == id && it.value() == held) {
            return;
        }
    }
    m_dispatcher->release(held, deviceSource(id));
}

bool ControllerHost::toggleTxLatch(DeviceId id, const QString& control)
{
    if (id != DeviceId::Rc28 || control != QLatin1String("tx")) {
        return false;
    }
    // First, so that turning the setting off can't strand a key already latched.
    if (m_txLatched.remove(id)) {
        m_dispatcher->release(idTxHold(), deviceSource(id));
        return true;
    }
    if (!m_deviceBindings->rc28TxStaysOn()
        || m_deviceBindings->binding(id, control, ControlGesture::Press).actionId != idTxHold()) {
        return false;
    }
    const HotKeySource source = deviceSource(id);
    m_dispatcher->press(idTxHold(), source, m_deviceBindings->binding(id, control, ControlGesture::Press).pin);
    // A refused press isn't in the ledger, and the next press tries again.
    if (m_dispatcher->holdsTransmit(source)) {
        m_txLatched.insert(id);
    }
    return true;
}

void ControllerHost::deviceRotate(DeviceId id, const DeviceEvent& event)
{
    const int steps = m_deviceBindings->reversed(id) ? -event.value : event.value;
    if (steps == 0) {
        return;
    }
    const int speed = m_deviceBindings->speed(id);
    int& carry = m_rotateCarry[DeviceControl{id, event.control}];
    // A direction change starts over, so a part step can't eat the first detents back.
    if ((carry > 0 && steps < 0) || (carry < 0 && steps > 0)) {
        carry = 0;
    }
    carry += steps;
    const int whole = carry / speed;
    carry -= whole * speed;
    sendDeviceTurn(id, event.control, whole);
}

void ControllerHost::deviceRing(DeviceId id, const DeviceEvent& event)
{
    const DeviceControl key{id, event.control};
    if (event.value == 0) {
        m_ringPositions.remove(key);
    } else {
        const bool wasCentered = !m_ringPositions.contains(key);
        m_ringPositions.insert(key, event.value);
        if (wasCentered) {
            sendRingTurn(id, event.control, event.value);
        }
    }
    if (m_ringPositions.isEmpty()) {
        m_ringTimer.stop();
    } else if (!m_ringTimer.isActive()) {
        m_ringTimer.start();
    }
}

void ControllerHost::onRingTick()
{
    const QList<DeviceControl> rings = m_ringPositions.keys();
    for (const DeviceControl& ring : rings) {
        sendRingTurn(ring.first, ring.second, m_ringPositions.value(ring));
    }
}

void ControllerHost::sendRingTurn(DeviceId id, const QString& control, int position)
{
    sendDeviceTurn(id, control, m_deviceBindings->reversed(id) ? -position : position);
}

void ControllerHost::sendDeviceTurn(DeviceId id, const QString& control, int steps)
{
    const HotKeyDeviceBinding binding = m_deviceBindings->binding(id, control, ControlGesture::Turn);
    if (steps == 0 || (steps > 0 ? binding.actionId : binding.actionIdDown).isEmpty()) {
        return;
    }
    m_dispatcher->turn(binding.actionId, binding.actionIdDown, steps, deviceSource(id), binding.pin);
}

void ControllerHost::setStateFeed(HotKeyStateFeed* feed)
{
    if (m_stateFeed) {
        disconnect(m_stateFeed, nullptr, this, nullptr);
    }
    m_stateFeed = feed;
    if (feed == nullptr) {
        return;
    }
    m_lastTransmitting = feed->display().transmitting;
    connect(feed, &HotKeyStateFeed::displayChanged, this, [this] {
        markOutputDirty(DeviceId::TMate2);
        markOutputDirty(DeviceId::StreamDeckPlus);
        markOutputDirty(DeviceId::Rc28);
        const bool transmitting = m_stateFeed && m_stateFeed->display().transmitting;
        if (transmitting != m_lastTransmitting) {
            m_lastTransmitting = transmitting;
            sendAtOnce(DeviceId::Rc28);
        }
        pumpOutputs();
    });
    connect(feed, &HotKeyStateFeed::stateChanged, this, [this](const QString& id, bool /*on*/) {
        markOutputDirty(DeviceId::Rc28);
        markOutputDirty(DeviceId::FlexControl);
        markOutputDirty(DeviceId::StreamDeckPlus);
        if (id == QLatin1String("slice.lock")) {
            markOutputDirty(DeviceId::TMate2);
        }
        if (isTransmitAction(id)) {
            sendAtOnce(DeviceId::StreamDeckPlus);
            sendAtOnce(DeviceId::FlexControl);
        }
        // The feed says this before it updates the display, and one change of slice can
        // be several of these. On the next pass the output shows them together.
        if (anyOutputDirty()) {
            m_displayTimer.start(0);
        }
    });
    refreshDeckKeys();
    for (const DeviceId id : kLitDevices) {
        markOutputDirty(id);
    }
    pumpOutputs();
}

HotKeyStateFeed* ControllerHost::stateFeed() const
{
    return m_stateFeed.data();
}

bool ControllerHost::hasLights(DeviceId id)
{
    return std::find(kLitDevices.cbegin(), kLitDevices.cend(), id) != kLitDevices.cend();
}

bool ControllerHost::lightsEnabled(DeviceId id) const
{
    return hasLights(id) && !m_lightsOff.contains(id);
}

void ControllerHost::setLightsEnabled(DeviceId id, bool enabled)
{
    if (!hasLights(id) || enabled == lightsEnabled(id)) {
        return;
    }
    if (enabled) {
        m_lightsOff.remove(id);
    } else {
        m_lightsOff.insert(id);
    }
    AppSettings& settings = AppSettings::instance();
    settings.setValue(lightsKey(id), enabled ? QStringLiteral("True") : QStringLiteral("False"));
    settings.save();
    // What each device is sent already depends on this, so the off state goes once,
    // and after it there's nothing new to send. Not held at the gate: this is one change
    // the user just asked for. An RC-28 with an F key down still waits for the release.
    m_outputGates[id].lastMs = -1;
    markOutputDirty(id);
    pumpOutputs();
}

void ControllerHost::queueOutput(DeviceId id, const QList<QByteArray>& packets)
{
    DeviceInput* const usb = m_usb;
    for (const QByteArray& packet : packets) {
        QMetaObject::invokeMethod(usb, [usb, id, packet] { usb->sendOutput(id, packet); }, Qt::QueuedConnection);
    }
    m_outputsQueued += static_cast<int>(packets.size());
}

void ControllerHost::onDisplayTick()
{
    pumpOutputs();
}

void ControllerHost::markOutputDirty(DeviceId id)
{
    // Nothing is kept or worked out for a device that isn't there.
    if (deviceConnected(id)) {
        m_outputGates[id].dirty = true;
    }
}

bool ControllerHost::anyOutputDirty() const
{
    return std::any_of(m_outputGates.cbegin(), m_outputGates.cend(), [](const OutputGate& gate) { return gate.dirty; });
}

void ControllerHost::pumpOutputs()
{
    const qint64 now = m_deviceClock();
    qint64 soonest = -1;
    for (const DeviceId id : kLitDevices) {
        const auto gate = m_outputGates.find(id);
        if (gate == m_outputGates.end() || !gate->dirty) {
            continue;
        }
        if (!deviceConnected(id)) {
            gate->dirty = false;
            continue;
        }
        qint64 wait = gate->lastMs < 0 ? 0 : kDisplayMinMs - (now - gate->lastMs);
        if (id == DeviceId::Rc28) {
            const qint64 lightWait = rc28LightWaitMs(now);
            if (lightWait < 0) {
                continue;   // the release brings it back here
            }
            wait = std::max(wait, lightWait);
        }
        if (wait > 0) {
            // The newest state goes out when the wait is over.
            soonest = soonest < 0 ? wait : std::min(soonest, wait);
            continue;
        }
        gate->dirty = false;
        if (sendOutputFor(id)) {
            gate->lastMs = now;
        }
    }
    if (soonest >= 0) {
        m_displayTimer.start(static_cast<int>(soonest));
    } else {
        m_displayTimer.stop();
    }
}

void ControllerHost::sendAtOnce(DeviceId id)
{
    if (!deviceConnected(id)) {
        return;
    }
    const qint64 now = m_deviceClock();
    if (id == DeviceId::Rc28 && rc28LightWaitMs(now) != 0) {
        markOutputDirty(id);
        pumpOutputs();
        return;
    }
    OutputGate& gate = m_outputGates[id];
    bool sent = false;
    if (id == DeviceId::StreamDeckPlus) {
        // The keys only: a strip waiting at the gate stays there.
        sent = refreshDeckKeys();
    } else {
        gate.dirty = false;
        sent = sendOutputFor(id);
    }
    if (sent) {
        gate.lastMs = now;
    }
}

bool ControllerHost::sendOutputFor(DeviceId id)
{
    switch (id) {
    case DeviceId::Rc28:
        return sendRc28Lights();
    case DeviceId::FlexControl:
        return sendFlexLights();
    case DeviceId::TMate2:
        return sendTMateFrame();
    case DeviceId::StreamDeckPlus: {
        const bool sentKeys = refreshDeckKeys();
        const bool sentStrip = sendDeckStrip();
        return sentKeys || sentStrip;
    }
    default:
        return false;
    }
}

qint64 ControllerHost::rc28LightWaitMs(qint64 now) const
{
    if (!m_rc28FKeysDown.isEmpty()) {
        return -1;
    }
    return m_rc28LightsSettleAtMs < 0 ? 0 : std::max<qint64>(0, m_rc28LightsSettleAtMs - now);
}

void ControllerHost::noteRc28Button(const DeviceEvent& event)
{
    if (!isRc28FKey(event.control)) {
        return;
    }
    if (event.kind == DeviceEvent::Kind::ButtonDown) {
        m_rc28FKeysDown.insert(event.control);
    } else if (event.kind == DeviceEvent::Kind::ButtonUp && m_rc28FKeysDown.remove(event.control)
               && m_rc28FKeysDown.isEmpty()) {
        // Upstream found that writing these lights with a button held broke the F1
        // light, so it writes once after the release, a little later so the release
        // report settles (AetherSDR src/gui/MainWindow_Controllers.cpp:2596-2600 [@1e0718ad]).
        m_rc28LightsSettleAtMs = m_deviceClock() + kRc28LightSettleMs;
        markOutputDirty(DeviceId::Rc28);
        pumpOutputs();
    }
}

bool ControllerHost::actionOn(const QString& actionId) const
{
    return m_stateFeed && m_stateFeed->isOn(actionId);
}

bool ControllerHost::sendRc28Lights()
{
    QByteArray report;
    if (lightsEnabled(DeviceId::Rc28)) {
        const HotKeyStateFeed::Display display = m_stateFeed ? m_stateFeed->display() : HotKeyStateFeed::Display{};
        // An F light shows the action on that key's hold row, as upstream
        // (AetherSDR src/gui/MainWindow_Controllers.cpp:583-589 [@1e0718ad]).
        const auto holdOn = [this](const char* control) {
            const QString name = QString::fromLatin1(control);
            return actionOn(m_deviceBindings->binding(DeviceId::Rc28, name, ControlGesture::Hold).actionId);
        };
        // TX as the radio has it, whatever keyed it, and not MOX
        // (AetherSDR CHANGELOG.md:1523 [@1e0718ad]).
        report = rc28LightReport(display.transmitting, holdOn("f1"), holdOn("f2"), display.connected);
    } else {
        report = rc28LightReport(false, false, false, false);
    }
    if (report == m_lastRc28Report) {
        return false;
    }
    m_lastRc28Report = report;
    queueOutput(DeviceId::Rc28, {report});
    return true;
}

int ControllerHost::flexLitButton() const
{
    if (!lightsEnabled(DeviceId::FlexControl)) {
        return 0;
    }
    // One lit at a time. With two on, the lower button shows.
    for (int button = 1; button <= kFlexAuxButtons; ++button) {
        const QString control = QStringLiteral("button%1").arg(button);
        if (actionOn(m_deviceBindings->binding(DeviceId::FlexControl, control, ControlGesture::Tap).actionId)) {
            return button;
        }
    }
    return 0;
}

bool ControllerHost::sendFlexLights()
{
    const int lit = flexLitButton();
    if (lit == m_lastFlexLit) {
        return false;
    }
    m_lastFlexLit = lit;
    FlexControlInput* const flex = m_flex;
    QMetaObject::invokeMethod(flex, [flex, lit] { flex->setLit(lit); }, Qt::QueuedConnection);
    ++m_outputsQueued;
    return true;
}

bool ControllerHost::sendTMateFrame()
{
    if (!deviceConnected(DeviceId::TMate2)) {
        return false;
    }
    QByteArray frame;
    if (lightsEnabled(DeviceId::TMate2)) {
        const HotKeyStateFeed::Display display = m_stateFeed ? m_stateFeed->display() : HotKeyStateFeed::Display{};
        frame = DeviceOutput::tmate2Frame(display.frequency, display.connected, actionOn(QStringLiteral("slice.lock")));
    } else {
        frame = DeviceOutput::tmate2OffFrame();
    }
    if (frame == m_lastTMateFrame) {
        return false;   // a change of mode or filter, which this display doesn't show
    }
    m_lastTMateFrame = frame;
    queueOutput(DeviceId::TMate2, {frame});
    return true;
}

bool ControllerHost::sendDeckStrip()
{
    if (!deviceConnected(DeviceId::StreamDeckPlus)) {
        return false;
    }
    const bool lightsOn = lightsEnabled(DeviceId::StreamDeckPlus);
    const HotKeyStateFeed::Display display = m_stateFeed ? m_stateFeed->display() : HotKeyStateFeed::Display{};
    const QString slice = display.slice.isNull() ? QString() : QString(display.slice);
    // With no separator in it, this can't be the same as anything shown.
    const QString shown = lightsOn ? QStringList{slice, display.frequency, display.mode, display.filter}.join(QLatin1Char('|'))
                                   : QStringLiteral("off");
    if (shown == m_lastDeckStrip) {
        return false;
    }
    m_lastDeckStrip = shown;
    ++m_imagesDrawn;
    queueOutput(DeviceId::StreamDeckPlus,
                DeviceOutput::streamDeckStrip(
                    lightsOn ? DeviceOutput::renderStreamDeckStrip(slice, display.frequency, display.mode, display.filter)
                             : DeviceOutput::renderStreamDeckBlankStrip()));
    return true;
}

bool ControllerHost::refreshDeckKeys()
{
    if (!deviceConnected(DeviceId::StreamDeckPlus)) {
        return false;
    }
    const bool lightsOn = lightsEnabled(DeviceId::StreamDeckPlus);
    bool sent = false;
    for (int key = 0; key < DeviceOutput::kStreamDeckKeyCount; ++key) {
        const QString control = QStringLiteral("button%1").arg(key + 1);
        const QString actionId = lightsOn
            ? m_deviceBindings->binding(DeviceId::StreamDeckPlus, control, ControlGesture::Press).actionId
            : QString();
        const HotKeyAction* const action = HotKeyActionList::find(actionId);
        const QString label = action != nullptr ? action->name : QString();
        const bool lit = actionOn(actionId);
        DeckKey& shown = m_deckKeys[static_cast<size_t>(key)];
        if (shown.sent && shown.label == label && shown.lit == lit && shown.off == !lightsOn) {
            continue;
        }
        shown = DeckKey{label, lit, !lightsOn, true};
        const bool transmits = action != nullptr && action->transmits;
        const QColor& background = !lightsOn ? kDeckKeyOff : (!lit ? kDeckKeyIdle : (transmits ? kDeckKeyLitTransmit : kDeckKeyLit));
        ++m_imagesDrawn;
        queueOutput(DeviceId::StreamDeckPlus,
                    DeviceOutput::streamDeckKeyImage(key, DeviceOutput::renderStreamDeckKey(label, background)));
        sent = true;
    }
    return sent;
}

void ControllerHost::forgetDevice(DeviceId id)
{
    const auto engine = m_gestures.find(id);
    if (engine != m_gestures.end()) {
        engine->reset();
    }
    const auto ofDevice = [id](auto it) { return it.key().first == id; };
    m_deviceHolds.removeIf(ofDevice);
    m_rotateCarry.removeIf(ofDevice);
    m_ringPositions.removeIf(ofDevice);
    m_txLatched.remove(id);
    if (m_ringPositions.isEmpty()) {
        m_ringTimer.stop();
    }
    updateGestureTimer();
    // A release lost with the device or in sleep can't leave its lights waiting.
    if (id == DeviceId::Rc28 && !m_rc28FKeysDown.isEmpty()) {
        m_rc28FKeysDown.clear();
        pumpOutputs();
    }
}

void ControllerHost::forgetAllDevices()
{
    for (const DeviceId id : HotKeyDeviceBindings::devices()) {
        forgetDevice(id);
    }
}

} // namespace NereusSDR
