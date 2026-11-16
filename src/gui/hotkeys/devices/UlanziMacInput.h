#pragma once

// =================================================================
// src/gui/hotkeys/devices/UlanziMacInput.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - Ulanzi Dial on macOS built on AetherSDR
//                 src/core/UlanziDialMacOSManager.cpp [@1e0718ad] for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. Nothing is created until the IOKit registry shows the dial
//                 and the application allows real devices, where upstream opens
//                 at start. Checks the open result and closes when it fails. Counts
//                 interfaces, so one going away does not read as the device
//                 gone. Closes the manager when the last goes away. Schedules in
//                 the common modes, so a release arrives while a menu is open.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/UlanziDialInput.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>

class QTimer;

namespace NereusSDR {

// Where the device layer reports. The real one calls these from its run loop callbacks.
class UlanziMacSink {
public:
    virtual ~UlanziMacSink() = default;
    virtual void dialMatched(const void* device) = 0;
    virtual void dialRemoved(const void* device) = 0;
    virtual void dialValue(quint32 usagePage, quint32 usage, qint64 value) = 0;
};

// An open, seized manager. Destroying it closes it, which lets go of the seize.
class UlanziMacSession {
public:
    virtual ~UlanziMacSession() = default;
};

class UlanziMacLayer {
public:
    virtual ~UlanziMacLayer() = default;
    // Reads the IOKit registry and never touches IOHIDManager, so it raises no prompt.
    virtual bool dialPresent() = 0;
    // Raises the Input Monitoring prompt, so it runs only once dialPresent says yes.
    // On failure, detail gets the words deviceOpenFailureReason reads.
    virtual std::unique_ptr<UlanziMacSession> open(UlanziMacSink* sink, QString* detail) = 0;
};

// Stays on the main thread, because IOHIDManager needs the main CFRunLoop (upstream's
// src/gui/MainWindow_Controllers.cpp:2737-2743 [@1e0718ad]). The rule that puts devices
// on a worker exists because device reads block. These callbacks don't: the system
// calls them with the value in hand, and they only decode and post an event. The
// registry read runs only while no dial is open.
class NEREUS_GUI_EXPORT UlanziMacInput : public UlanziDialInput, private UlanziMacSink {
    Q_OBJECT

public:
    using LayerFactory = std::function<std::unique_ptr<UlanziMacLayer>()>;

    // How often the registry is read while no dial is open. Upstream's hotplug check
    // on the other systems, src/core/UlanziDialWindowsManager.h:81 [@1e0718ad]
    static constexpr int kPresenceMs = 3000;
    // After a refused open, this many presence checks pass before the next try. Our
    // choice: an open talks to the system's privacy service on the main thread.
    static constexpr int kChecksBetweenTries = 10;

    explicit UlanziMacInput(QObject* parent = nullptr);
    ~UlanziMacInput() override;

    void start() override;
    void stop() override;

    // The real manager, behind the application's switch. Null off macOS, and null with
    // nothing touched while the switch is off. Public so a test can show the second.
    static std::unique_ptr<UlanziMacSession> openRealSession(UlanziMacSink* sink, QString* detail);

    // Null goes back to the real device.
    static void setLayerFactoryForTest(LayerFactory factory);

    // What the layer's callbacks land on; public for tests.
    void dialMatched(const void* device) override;
    void dialRemoved(const void* device) override;
    void dialValue(quint32 usagePage, quint32 usage, qint64 value) override;

public slots:
    void checkPresence();   // the timer's slot; public for tests

private:
    void closeIfNoDevices();

    std::unique_ptr<UlanziMacLayer> m_layer;
    std::unique_ptr<UlanziMacSession> m_session;
    QTimer* m_presenceTimer;
    QSet<const void*> m_devices;    // the dial shows one per HID interface
    bool m_wanted{false};
    bool m_opened{false};           // the state says connected
    int m_checksToWait{0};
};

} // namespace NereusSDR
