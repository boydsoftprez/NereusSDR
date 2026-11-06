// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 16.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
//
// What the three Ulanzi Dial backends share. Each turns its system's input into Linux
// key codes and hands them to feedKey. create() makes the one for this system; the
// other two build and stay idle, so a test on any system can run their logic.
//
// The gate: every backend reaches its system's device code only through a real layer,
// and each makes that layer only when DeviceInput::realDevicesAllowed() is on, which
// only the application turns on. A test process gets a layer with nothing on it.
#pragma once

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"
#include "gui/hotkeys/devices/UlanziChordDecoder.h"

#include <QObject>
#include <QString>

namespace NereusSDR {

class NEREUS_GUI_EXPORT UlanziDialInput : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void start() = 0;
    virtual void stop() = 0;

    // True on the system whose backend must stay on the main thread.
    static bool staysOnMainThread();
    // The backend for this system, with no parent when it is to move to a worker.
    static UlanziDialInput* create(QObject* parent);

    static const QString& notPluggedInReason();
    static const QString& claimedReason();

    // A plugin in Ulanzi Studio has the dial: let go of it, so one press can't arrive
    // twice, and take it back when that plugin goes. Same thread as start and stop.
    void setClaimedElsewhere(bool claimed);

signals:
    void deviceEvent(NereusSDR::DeviceId id, NereusSDR::DeviceEvent event);
    // reason is shown to the user. Empty when connected, and after stop.
    void deviceStateChanged(NereusSDR::DeviceId id, bool connected, QString reason);

protected:
    void feedKey(int linuxKeyCode, int value);
    void notify(bool connected, const QString& reason);
    // Say it's gone first: the host then drops every hold and forgets every press, so
    // these releases can't turn into a tap and run something as the device goes away.
    void keysGone(const QString& reason);
    void forgetState() { m_stateKnown = false; }
    const QString& lastReason() const { return m_reason; }

private:
    UlanziChordDecoder m_decoder;
    bool m_stateKnown{false};
    bool m_connected{false};
    bool m_claimedElsewhere{false};
    QString m_reason;
};

} // namespace NereusSDR
