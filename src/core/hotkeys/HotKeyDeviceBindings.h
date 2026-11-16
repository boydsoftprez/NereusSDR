// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// What each control on a knob or controller runs: the defaults of the design's
// section 5, and the user's changes in AppSettings. Holds no device code, so the
// Core can link it (R-HK-18).
#pragma once

#include "core/NereusCoreExport.h"

#include <QChar>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>

namespace NereusSDR {

enum class DeviceId { Rc28, PowerMate, ShuttleXpress, ShuttlePro2, FlexControl, TMate2, StreamDeckPlus, UlanziDial };
enum class ControlGesture { Press, Hold, Tap, DoubleTap, Turn };

// For a Turn control, actionId runs for steps up and actionIdDown for steps down.
// An empty actionId means nothing is bound.
struct HotKeyDeviceBinding {
    QString actionId;
    QString actionIdDown;
    QChar pin;

    bool operator==(const HotKeyDeviceBinding& other) const
    {
        return actionId == other.actionId && actionIdDown == other.actionIdDown && pin == other.pin;
    }
};

class NEREUS_CORE_EXPORT HotKeyDeviceBindings : public QObject {
    Q_OBJECT

public:
    // Reads the saved changes. A change is saved as it is made.
    explicit HotKeyDeviceBindings(QObject* parent = nullptr);

    static QList<DeviceId> devices();
    static QString deviceName(DeviceId id);
    static QString deviceKey(DeviceId id);              // stable text, for settings and HotKeySource
    static bool testedOnRealDevice(DeviceId id);        // false for every device; no simulator may change this (R-HK-19)
    // The TMate 2's encoders by index: main, printed E2, printed E1.
    static QString tmate2EncoderControl(int index);

    // Empty when the binding is allowed. A Turn control refuses what MidiProfile
    // refuses on a knob. Tap and double tap refuse a Hold action, since no release
    // comes after one.
    static QString refusalFor(ControlGesture gesture, const QString& actionId, const QString& actionIdDown);

    HotKeyDeviceBinding binding(DeviceId id, const QString& control, ControlGesture gesture) const;
    // Refused bindings change nothing and emit bindingRefused. An empty actionId
    // clears the control; the default's own value removes the change.
    void setBinding(DeviceId id, const QString& control, ControlGesture gesture, const HotKeyDeviceBinding& binding);

    int speed(DeviceId id) const;           // 1 = every detent, N = one step per N
    void setSpeed(DeviceId id, int speed);
    bool reversed(DeviceId id) const;
    void setReversed(DeviceId id, bool reversed);
    bool rc28TxStaysOn() const;
    void setRc28TxStaysOn(bool on);

signals:
    void bindingsChanged();
    void bindingRefused(NereusSDR::DeviceId id, QString control, NereusSDR::ControlGesture gesture, QString reason);

private:
    void load();

    QHash<QString, HotKeyDeviceBinding> m_changed;      // by device/control/gesture
    QHash<DeviceId, int> m_speed;
    QHash<DeviceId, bool> m_reversed;
    bool m_rc28TxStaysOn{false};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::DeviceId)
Q_DECLARE_METATYPE(NereusSDR::ControlGesture)
