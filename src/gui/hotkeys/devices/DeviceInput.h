#pragma once

// =================================================================
// src/gui/hotkeys/devices/DeviceInput.h  (NereusSDR)
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
//   2026-11-06 - USB knob and controller reader built on AetherSDR
//                 src/core/HidEncoderManager.cpp [@1e0718ad] (the hidapi open
//                 sequence, the read loop, the hotplug check) for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 12. One device per kind, all at once, where upstream has one
//                 in all; its own thread; nothing opened until the device is
//                 seen on the USB bus; open failures say how to fix them.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <map>
#include <memory>

class QTimer;

namespace NereusSDR {

class PowerMateLinuxInput;

// One open device. The real one wraps a hid_device; a test supplies its own.
class HidDeviceHandle {
public:
    virtual ~HidDeviceHandle() = default;
    // Bytes read, 0 for nothing waiting, below 0 when the device has gone.
    virtual int read(char* buffer, int size) = 0;
    virtual int write(const QByteArray& report) = 0;
};

// What DeviceInput needs from the device layer. All of it runs on the USB worker.
class HidLayer {
public:
    virtual ~HidLayer() = default;
    virtual QSet<quint32> present() = 0;    // never opens a device (UsbPresence::key)
    // On failure, error gets the detail deviceOpenFailureReason reads.
    virtual std::unique_ptr<HidDeviceHandle> open(quint16 vendorId, quint16 productId, QString* error) = 0;
};

// One way a device shows up on USB. A device can have more than one.
struct DeviceSpec {
    DeviceId id;
    quint16 vendorId;
    quint16 productId;
    int reportSize;
    // Null until a parser exists: such a device is never opened.
    std::function<std::unique_ptr<DeviceParser>()> makeParser;
};

// The fix for an open failure, in plain words. detail is hidapi's error text.
NEREUS_GUI_EXPORT QString deviceOpenFailureReason(const QString& detail);

class NEREUS_GUI_EXPORT DeviceInput : public QObject {   // lives on the USB worker
    Q_OBJECT

public:
    using LayerFactory = std::function<std::unique_ptr<HidLayer>()>;

    // From AetherSDR src/core/HidEncoderManager.h:189-190 [@1e0718ad]
    static constexpr int kPollMs = 5;
    static constexpr int kPresenceMs = 3000;

    explicit DeviceInput(QObject* parent = nullptr);
    ~DeviceInput() override;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void sendOutput(NereusSDR::DeviceId id, QByteArray report);

    static bool supported();    // false in a build without hidapi

    // The application calls this once at startup, before it builds a window. Until
    // then the real device library is never touched, so a test process, which never
    // calls it, can't open a device on this computer or raise the macOS prompt.
    static void allowRealDevices();
    static bool realDevicesAllowed();

    // Null goes back to the real device layer, or the real table.
    static void setLayerFactoryForTest(LayerFactory factory);
    static void setDeviceTableForTest(const QList<DeviceSpec>& table);
    static void clearDeviceTableForTest();

public slots:
    void poll();            // the 5 ms timer's slot; public for tests
    void checkPresence();   // the 3 s timer's slot; public for tests

signals:
    void deviceEvent(NereusSDR::DeviceId id, NereusSDR::DeviceEvent event);
    // reason is shown to the user. Empty when connected, and after stop.
    void deviceStateChanged(NereusSDR::DeviceId id, bool connected, QString reason);

private:
    struct OpenDevice {
        std::unique_ptr<HidDeviceHandle> handle;
        DeviceParser* parser;
        QByteArray buffer;
    };

    void tryOpen(const DeviceSpec& spec);
    void closeDevice(DeviceId id);
    void notify(DeviceId id, bool connected, const QString& reason);
    QList<DeviceId> tableDevices() const;

    std::unique_ptr<HidLayer> m_layer;
    QList<DeviceSpec> m_table;
    QTimer* m_pollTimer;
    QTimer* m_presenceTimer;
    PowerMateLinuxInput* m_powerMateLinux{nullptr};     // Linux only; a Qt child
    bool m_started{false};

    std::map<DeviceId, OpenDevice> m_open;
    std::map<DeviceId, std::unique_ptr<DeviceParser>> m_parsers;    // kept across an unplug
    struct State {
        bool connected{false};
        QString reason;
    };
    QHash<DeviceId, State> m_states;
};

} // namespace NereusSDR
