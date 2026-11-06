// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Owns the controller workers and turns what they report into dispatcher calls.
// Lives on the main thread with the dispatcher. MidiInput lives on the MIDI worker,
// and the two talk only by queued signals (design V8).
#pragma once

#include "core/hotkeys/HotKeyAction.h"
#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/midi/MidiProfile.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QThread>

namespace NereusSDR {

class HotKeyBindings;
class HotKeyDispatcher;
class MidiInput;

class NEREUS_GUI_EXPORT ControllerHost : public QObject {
    Q_OBJECT

public:
    // A fader on a level action moves its full range over this many steps (plan,
    // decisions): at a volume step of 5, that is the whole slider.
    static constexpr int kLevelStepsPerRange = 20;

    ControllerHost(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, QObject* parent = nullptr);
    ~ControllerHost() override;

    MidiInput* midi() const { return m_midi; }

    // A binding a knob can't run is left out, with midiBindingRefused.
    void setMidiProfile(const MidiProfile& profile);
    MidiProfile midiProfile() const { return m_profile; }

    // Saves the choice and opens it now and at each launch. Empty closes it.
    void setMidiDevice(const QString& name);
    static QString savedMidiDevice();

public slots:
    // Where MidiInput's signals land; public for tests.
    void onMidiControlEvent(const QString& controlKey, NereusSDR::MidiControlKind kind, int value, int delta);
    void onMidiPortOpened(const QString& portName);
    void onMidiConnectionChanged(bool connected, const QString& reason);

signals:
    void midiBindingRefused(QString controlKey, QString reason);

private:
    void forgetButtons();
    void releaseButton(const QString& controlKey);
    void routeAbsolute(const MidiBinding& binding, int delta);
    void routeTurn(const MidiBinding& binding, int steps);

    HotKeyDispatcher* m_dispatcher;
    QThread m_midiThread;
    MidiInput* m_midi;  // deleted on its own thread as that thread ends

    MidiProfile m_profile;
    QHash<QString, MidiBinding> m_routes;

    // One per device, so an unplug drops only that device's holds.
    HotKeySource m_midiSource{HotKeySourceKind::Midi, QString()};
    QHash<QString, QString> m_buttonsDown;      // control, and the action its press ran
    QHash<QString, int> m_absoluteCarry;        // part steps not yet sent, by control
};

} // namespace NereusSDR
