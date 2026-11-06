// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12: the USB worker, gestures
//                and device routing. AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 14: displays and lit keys from
//                the state feed. AI-assisted implementation via Anthropic Claude Code.
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
#include "gui/hotkeys/midi/MidiInput.h"

#include <QColor>
#include <QMetaObject>
#include <QStringList>

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
    m_usb->moveToThread(&m_usbThread);
    connect(&m_usbThread, &QThread::finished, m_usb, &QObject::deleteLater);
    connect(m_usb, &DeviceInput::deviceEvent, this, &ControllerHost::onDeviceEvent, Qt::QueuedConnection);
    connect(m_usb, &DeviceInput::deviceStateChanged, this, &ControllerHost::onDeviceStateChanged, Qt::QueuedConnection);

    m_deviceClockStart.start();
    m_deviceClock = [this] { return m_deviceClockStart.elapsed(); };
    m_gestureTimer.setInterval(kGestureTickMs);
    connect(&m_gestureTimer, &QTimer::timeout, this, &ControllerHost::onGestureTick);
    m_ringTimer.setInterval(kRingRepeatMs);
    connect(&m_ringTimer, &QTimer::timeout, this, &ControllerHost::onRingTick);
    m_displayTimer.setSingleShot(true);
    connect(&m_displayTimer, &QTimer::timeout, this, &ControllerHost::onDisplayTick);
    connect(m_deviceBindings.get(), &HotKeyDeviceBindings::bindingsChanged, this, &ControllerHost::refreshDeckKeys);

    m_midiThread.start();
    m_usbThread.start();
    QMetaObject::invokeMethod(m_usb, &DeviceInput::start, Qt::QueuedConnection);

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
    // A device just opened shows nothing of ours, so it gets everything.
    if (connected && id == DeviceId::TMate2) {
        m_lastTMateFrame.clear();
        m_displayDirty = true;
        pumpDisplays();
    } else if (connected && id == DeviceId::StreamDeckPlus) {
        m_deckKeys = {};
        m_lastDeckStrip.clear();
        refreshDeckKeys();
        m_displayDirty = true;
        pumpDisplays();
    }
}

void ControllerHost::onDeviceEvent(DeviceId id, const DeviceEvent& event)
{
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
    connect(feed, &HotKeyStateFeed::displayChanged, this, [this] {
        if (deviceConnected(DeviceId::TMate2) || deviceConnected(DeviceId::StreamDeckPlus)) {
            m_displayDirty = true;
            pumpDisplays();
        }
    });
    connect(feed, &HotKeyStateFeed::stateChanged, this, [this](const QString& id, bool /*on*/) {
        refreshDeckKeys();
        // The feed says this before it updates the display, and a change of slice can
        // be both. On the next pass the frame shows them together.
        if (id == QLatin1String("slice.lock") && deviceConnected(DeviceId::TMate2)) {
            m_displayDirty = true;
            if (!m_displayTimer.isActive()) {
                m_displayTimer.start(0);
            }
        }
    });
    refreshDeckKeys();
    if (deviceConnected(DeviceId::TMate2) || deviceConnected(DeviceId::StreamDeckPlus)) {
        m_displayDirty = true;
        pumpDisplays();
    }
}

HotKeyStateFeed* ControllerHost::stateFeed() const
{
    return m_stateFeed.data();
}

void ControllerHost::queueOutput(DeviceId id, const QList<QByteArray>& packets)
{
    DeviceInput* const usb = m_usb;
    for (const QByteArray& packet : packets) {
        QMetaObject::invokeMethod(usb, [usb, id, packet] { usb->sendOutput(id, packet); }, Qt::QueuedConnection);
    }
}

void ControllerHost::onDisplayTick()
{
    pumpDisplays();
}

void ControllerHost::pumpDisplays()
{
    if (!m_displayDirty) {
        return;
    }
    const qint64 now = m_deviceClock();
    const qint64 wait = m_lastDisplayMs < 0 ? 0 : kDisplayMinMs - (now - m_lastDisplayMs);
    if (wait > 0) {
        // The newest state goes out when the wait is over.
        m_displayTimer.start(static_cast<int>(wait));
        return;
    }
    m_displayDirty = false;
    m_displayTimer.stop();
    const bool sentFrame = sendTMateFrame();
    const bool sentStrip = sendDeckStrip();
    if (sentFrame || sentStrip) {
        m_lastDisplayMs = now;
    }
}

bool ControllerHost::sendTMateFrame()
{
    if (!deviceConnected(DeviceId::TMate2)) {
        return false;
    }
    const HotKeyStateFeed::Display display = m_stateFeed ? m_stateFeed->display() : HotKeyStateFeed::Display{};
    const bool locked = m_stateFeed && m_stateFeed->isOn(QStringLiteral("slice.lock"));
    const QByteArray frame = DeviceOutput::tmate2Frame(display.frequency, display.connected, locked);
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
    const HotKeyStateFeed::Display display = m_stateFeed ? m_stateFeed->display() : HotKeyStateFeed::Display{};
    const QString slice = display.slice.isNull() ? QString() : QString(display.slice);
    const QString shown = QStringList{slice, display.frequency, display.mode, display.filter}.join(QLatin1Char('|'));
    if (shown == m_lastDeckStrip) {
        return false;
    }
    m_lastDeckStrip = shown;
    queueOutput(DeviceId::StreamDeckPlus,
                DeviceOutput::streamDeckStrip(
                    DeviceOutput::renderStreamDeckStrip(slice, display.frequency, display.mode, display.filter)));
    return true;
}

void ControllerHost::refreshDeckKeys()
{
    if (!deviceConnected(DeviceId::StreamDeckPlus)) {
        return;
    }
    for (int key = 0; key < DeviceOutput::kStreamDeckKeyCount; ++key) {
        const QString control = QStringLiteral("button%1").arg(key + 1);
        const QString actionId = m_deviceBindings->binding(DeviceId::StreamDeckPlus, control, ControlGesture::Press).actionId;
        const HotKeyAction* const action = HotKeyActionList::find(actionId);
        const QString label = action != nullptr ? action->name : QString();
        const bool lit = m_stateFeed && m_stateFeed->isOn(actionId);
        DeckKey& shown = m_deckKeys[static_cast<size_t>(key)];
        if (shown.sent && shown.label == label && shown.lit == lit) {
            continue;
        }
        shown = DeckKey{label, lit, true};
        const bool transmits = action != nullptr && action->transmits;
        const QColor& background = !lit ? kDeckKeyIdle : (transmits ? kDeckKeyLitTransmit : kDeckKeyLit);
        queueOutput(DeviceId::StreamDeckPlus,
                    DeviceOutput::streamDeckKeyImage(key, DeviceOutput::renderStreamDeckKey(label, background)));
    }
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
}

void ControllerHost::forgetAllDevices()
{
    for (const DeviceId id : HotKeyDeviceBindings::devices()) {
        forgetDevice(id);
    }
}

} // namespace NereusSDR
