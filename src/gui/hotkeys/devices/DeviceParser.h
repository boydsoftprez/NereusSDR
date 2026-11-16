// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 12.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// What a knob or controller reports. A parser turns one raw report into these. It
// knows no device library and no Qt GUI, so a test can feed it bytes.
#pragma once

#include "core/hotkeys/HotKeyDeviceBindings.h"

#include <QByteArrayView>
#include <QList>
#include <QMetaType>
#include <QString>

namespace NereusSDR {

struct DeviceEvent {
    enum class Kind { ButtonDown, ButtonUp, Rotate, RingPosition, TokenGesture } kind{Kind::ButtonDown};
    QString control;   // "wheel", "f1", "tx", "button3", "e1", "dial2", "topLeft", ...
    int value{0};      // steps for Rotate, -7..7 for RingPosition; 0 otherwise
    ControlGesture gesture{ControlGesture::Press};   // for TokenGesture only
};

class DeviceParser {
public:
    virtual ~DeviceParser() = default;
    virtual QList<DeviceEvent> parse(QByteArrayView report) = 0;   // all changes in one report
    virtual void reset() = 0;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::DeviceEvent)
