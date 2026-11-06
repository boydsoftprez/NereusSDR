// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12: the USB worker, gestures
//                and device routing. AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 14: displays and lit keys from
//                the state feed. AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// Owns the controller workers and turns what they report into dispatcher calls.
// Lives on the main thread with the dispatcher. MidiInput lives on the MIDI worker,
// DeviceInput on the USB worker, and each talks to this only by queued signals (design V8).
#pragma once

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/devices/DeviceParser.h"
#include "gui/hotkeys/devices/GestureEngine.h"
#include "gui/hotkeys/midi/MidiProfile.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QThread>
#include <QTimer>

#include <array>
#include <functional>
#include <memory>

namespace NereusSDR {

class HotKeyBindings;
class DeviceInput;
class HotKeyDispatcher;
class HotKeyStateFeed;
class MidiInput;

class NEREUS_GUI_EXPORT ControllerHost : public QObject {
    Q_OBJECT

public:
    // A fader on a level action moves its full range over this many steps (plan,
    // decisions): at a volume step of 5, that is the whole slider.
    static constexpr int kLevelStepsPerRange = 20;
    // A jog ring held off center sends its position this often (plan, our choice).
    static constexpr int kRingRepeatMs = 100;
    // How often a control waiting to become a hold is looked at. Costs nothing at
    // rest: it runs only while one is down.
    static constexpr int kGestureTickMs = 20;
    // A display follows a turning knob at most this often (plan, Task 14).
    static constexpr int kDisplayMinMs = 100;

    ControllerHost(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, QObject* parent = nullptr);
    ~ControllerHost() override;

    MidiInput* midi() const { return m_midi; }
    DeviceInput* usb() const { return m_usb; }
    HotKeyDeviceBindings* deviceBindings() const { return m_deviceBindings.get(); }

    // A binding a knob can't run is left out, with midiBindingRefused.
    void setMidiProfile(const MidiProfile& profile);
    MidiProfile midiProfile() const { return m_profile; }

    // Saves the choice and opens it now and at each launch. Empty closes it.
    void setMidiDevice(const QString& name);
    static QString savedMidiDevice();

    // One source per device, so an unplug drops only that device's holds.
    static HotKeySource deviceSource(DeviceId id);
    bool deviceConnected(DeviceId id) const { return m_deviceStates.value(id).connected; }
    QString deviceReason(DeviceId id) const { return m_deviceStates.value(id).reason; }

    // What the displays and lit keys show. Null-safe if the feed goes first.
    void setStateFeed(HotKeyStateFeed* feed);
    HotKeyStateFeed* stateFeed() const;

    using ClockMs = std::function<qint64()>;
    void setDeviceClockForTest(ClockMs clock) { m_deviceClock = std::move(clock); }

public slots:
    // Where MidiInput's signals land; public for tests.
    void onMidiControlEvent(const QString& controlKey, NereusSDR::MidiControlKind kind, int value, int delta);
    void onMidiPortOpened(const QString& portName);
    void onMidiConnectionChanged(bool connected, const QString& reason);

    // Where DeviceInput's signals land; public for tests.
    void onDeviceEvent(NereusSDR::DeviceId id, const NereusSDR::DeviceEvent& event);
    void onDeviceStateChanged(NereusSDR::DeviceId id, bool connected, const QString& reason);
    void onGestureTick();   // the timer's slot; public for tests
    void onRingTick();      // the timer's slot; public for tests
    void onDisplayTick();   // the timer's slot; public for tests

signals:
    void midiBindingRefused(QString controlKey, QString reason);
    void deviceStateChanged(NereusSDR::DeviceId id, bool connected, QString reason);

private:
    void forgetButtons();
    void releaseButton(const QString& controlKey);
    void routeAbsolute(const MidiBinding& binding, int delta);
    void routeTurn(const MidiBinding& binding, int steps);

    void deviceButtonDown(DeviceId id, const DeviceEvent& event);
    void deviceButtonUp(DeviceId id, const DeviceEvent& event);
    bool usesGestureEngine(DeviceId id, const QString& control) const;
    void runGesture(DeviceId id, const QString& control, ControlGesture gesture, bool canRelease);
    void runBinding(DeviceId id, const QString& control, const HotKeyDeviceBinding& binding, bool canRelease);
    void releaseDeviceControl(DeviceId id, const QString& control);
    void deviceRotate(DeviceId id, const DeviceEvent& event);
    void deviceRing(DeviceId id, const DeviceEvent& event);
    void sendRingTurn(DeviceId id, const QString& control, int position);
    void sendDeviceTurn(DeviceId id, const QString& control, int steps);
    bool toggleTxLatch(DeviceId id, const QString& control);
    void forgetDevice(DeviceId id);
    void forgetAllDevices();
    void updateGestureTimer();

    void queueOutput(DeviceId id, const QList<QByteArray>& packets);
    void pumpDisplays();
    bool sendTMateFrame();
    bool sendDeckStrip();
    void refreshDeckKeys();

    HotKeyDispatcher* m_dispatcher;
    QThread m_midiThread;
    MidiInput* m_midi;  // deleted on its own thread as that thread ends

    MidiProfile m_profile;
    QHash<QString, MidiBinding> m_routes;

    // One per device, so an unplug drops only that device's holds.
    HotKeySource m_midiSource{HotKeySourceKind::Midi, QString()};
    QHash<QString, QString> m_buttonsDown;      // control, and the action its press ran
    QHash<QString, int> m_absoluteCarry;        // part steps not yet sent, by control

    using DeviceControl = QPair<DeviceId, QString>;
    struct DeviceState {
        bool connected{false};
        QString reason;
    };

    QThread m_usbThread;
    DeviceInput* m_usb;  // deleted on its own thread as that thread ends
    std::unique_ptr<HotKeyDeviceBindings> m_deviceBindings;
    QHash<DeviceId, DeviceState> m_deviceStates;
    QHash<DeviceId, GestureEngine> m_gestures;
    QHash<DeviceControl, QString> m_deviceHolds;    // control, and the Hold action its press ran
    QHash<DeviceControl, int> m_rotateCarry;        // steps short of one speed unit
    QHash<DeviceControl, int> m_ringPositions;      // only those off center
    QSet<DeviceId> m_txLatched;                     // a stay-on hold is keying from this device
    QElapsedTimer m_deviceClockStart;
    ClockMs m_deviceClock;
    QTimer m_gestureTimer;
    QTimer m_ringTimer;

    // Displays and lit keys. All drawn here, and only what changed is drawn, and only
    // for a device that's open. The worker is handed bytes.
    struct DeckKey {
        QString label;
        bool lit{false};
        bool sent{false};
    };
    QPointer<HotKeyStateFeed> m_stateFeed;
    QByteArray m_lastTMateFrame;
    QString m_lastDeckStrip;
    std::array<DeckKey, 8> m_deckKeys;
    bool m_displayDirty{false};
    qint64 m_lastDisplayMs{-1};
    QTimer m_displayTimer;
};

} // namespace NereusSDR
