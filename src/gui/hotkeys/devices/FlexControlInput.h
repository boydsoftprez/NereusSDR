#pragma once

// =================================================================
// src/gui/hotkeys/devices/FlexControlInput.h  (NereusSDR)
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
//   2026-11-06 - FlexControl serial input built on AetherSDR
//                 src/core/FlexControlManager.cpp and .h [@1e0718ad] for NereusSDR
//                 by J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 15. The port sits behind a seam so a test never opens one;
//                 it looks for the device from launch rather than only after
//                 a first connection; one timer shot per attempt so a test can
//                 drive the back-off; open failures say how to fix them.
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/devices/DeviceParser.h"
#include "gui/hotkeys/devices/FlexControlParser.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>

class QTimer;

namespace NereusSDR {

// One open serial port. The real one wraps a QSerialPort; a test supplies its own.
// bytesArrived may come from inside the port's own signal, so closePort must be safe
// there. failed must never come from inside a call into the handle.
class NEREUS_GUI_EXPORT FlexSerialHandle : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    virtual bool write(const QByteArray& bytes) = 0;    // false when not all of it went
    virtual void finishWrites() = 0;                    // a short wait, for a last write
    virtual void closePort() = 0;

signals:
    void bytesArrived(QByteArray bytes);
    void failed(QString detail);
};

enum class FlexOpenFailure { Gone, Denied, Other };

// What FlexControlInput needs from the serial ports. All of it runs on the USB worker.
class FlexSerialLayer {
public:
    virtual ~FlexSerialLayer() = default;
    // Lists ports and opens none. Empty when the device isn't there.
    virtual QString findPort(quint16 vendorId, quint16 productId) = 0;
    virtual std::unique_ptr<FlexSerialHandle> open(const QString& portName, FlexOpenFailure* failure,
                                                   QString* detail) = 0;
};

// The fix for an open failure, in plain words.
NEREUS_GUI_EXPORT QString flexOpenFailureReason(FlexOpenFailure failure);

class NEREUS_GUI_EXPORT FlexControlInput : public QObject {   // lives on the USB worker
    Q_OBJECT

public:
    using LayerFactory = std::function<std::unique_ptr<FlexSerialLayer>()>;
    using Scheduler = std::function<void(int delayMs)>;

    // From AetherSDR src/core/FlexControlManager.h:16,57-58 [@1e0718ad]
    static constexpr quint16 kVendorId = 0x2192;
    static constexpr quint16 kProductId = 0x0010;
    // From AetherSDR src/core/FlexControlManager.h:64,70 [@1e0718ad]
    static constexpr int kRetryMinMs = 2000;
    static constexpr int kRetryMaxMs = 30000;

    explicit FlexControlInput(QObject* parent = nullptr);
    ~FlexControlInput() override;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    // Which of aux 1 to 3 is lit. Anything else lights none. Sent now and at each open
    // and device reset.
    Q_INVOKABLE void setLit(int lit);

    static bool supported();    // false in a build without Qt SerialPort

    // Null goes back to the real serial ports, or the real timer. A factory may return
    // null, which is what a build without Qt SerialPort has.
    static void setLayerFactoryForTest(LayerFactory factory);
    static void setSchedulerForTest(Scheduler scheduler);

public slots:
    void retryNow();    // the retry timer's slot; public for tests

signals:
    void deviceEvent(NereusSDR::DeviceId id, NereusSDR::DeviceEvent event);
    // reason is shown to the user. Empty when connected, and after stop.
    void deviceStateChanged(NereusSDR::DeviceId id, bool connected, QString reason);

private:
    void tryOpen();
    void armRetry();
    void writeLights();
    void onBytes(const QByteArray& bytes);
    void onPortFailed(const QString& detail);
    void retirePort();
    void closeWithLightsOff();
    void notify(bool connected, const QString& reason);

    std::unique_ptr<FlexSerialLayer> m_layer;
    Scheduler m_scheduler;
    QTimer* m_retryTimer;
    FlexSerialHandle* m_port{nullptr};  // a Qt child while open
    FlexControlParser m_parser;
    int m_lit{0};

    bool m_wanted{false};
    bool m_retryArmed{false};
    int m_retryMs{kRetryMinMs};
    bool m_failureLogged{false};    // warn once per outage, not per tick

    bool m_stateKnown{false};
    bool m_connected{false};
    QString m_reason;
};

} // namespace NereusSDR
