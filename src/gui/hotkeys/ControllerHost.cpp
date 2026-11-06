// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/ControllerHost.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/midi/MidiInput.h"

#include <QMetaObject>
#include <QStringList>

namespace NereusSDR {

namespace {

const QString& deviceKey()
{
    static const QString kKey = QStringLiteral("HotKeys/Midi/Device");
    return kKey;
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
    connect(bindings, &HotKeyBindings::enabledChanged, this, [this](bool on) {
        if (!on) {
            forgetButtons();
        }
    });

    m_midiThread.start();

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

} // namespace NereusSDR
