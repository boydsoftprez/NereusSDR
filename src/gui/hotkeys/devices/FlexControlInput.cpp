// =================================================================
// src/gui/hotkeys/devices/FlexControlInput.cpp  (NereusSDR)
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

#include "gui/hotkeys/devices/FlexControlInput.h"

#include "core/LogCategories.h"
#include "gui/hotkeys/devices/DeviceInput.h"

#include <QTimer>

#include <utility>

#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

namespace NereusSDR {

namespace {

const QString& notPluggedInReason()
{
    static const QString kReason = QStringLiteral("Not plugged in");
    return kReason;
}

const QString& noSupportReason()
{
    static const QString kReason = QStringLiteral("This build has no serial port support.");
    return kReason;
}

FlexControlInput::LayerFactory& testLayerFactory()
{
    static FlexControlInput::LayerFactory kFactory;
    return kFactory;
}

FlexControlInput::Scheduler& testScheduler()
{
    static FlexControlInput::Scheduler kScheduler;
    return kScheduler;
}

#ifdef HAVE_SERIALPORT

// A bus with nothing on it, for a process that hasn't allowed real devices. It never
// calls Qt's serial port code, so it neither lists nor opens a port.
class NoPortLayer : public FlexSerialLayer {
public:
    QString findPort(quint16 /*vendorId*/, quint16 /*productId*/) override
    {
        return {};
    }

    std::unique_ptr<FlexSerialHandle> open(const QString& /*portName*/, FlexOpenFailure* /*failure*/,
                                           QString* /*detail*/) override
    {
        return nullptr;
    }
};

class RealSerialHandle : public FlexSerialHandle {
public:
    RealSerialHandle()
    {
        // Set this as parent so moveToThread() moves m_port with us.
        // [original inline comment from src/core/FlexControlManager.cpp:14]
        m_port.setParent(this);
    }

    bool open(const QString& portName, FlexOpenFailure* failure, QString* detail)
    {
        // A previous error can leave the port latched; clear it or Qt may refuse
        // to reopen the same object.
        // [original inline comment from src/core/FlexControlManager.cpp:100-101]
        m_port.clearError();

        // From AetherSDR src/core/FlexControlManager.cpp:104-111 [@1e0718ad]
        m_port.setPortName(portName);
        m_port.setBaudRate(9600);
        m_port.setDataBits(QSerialPort::Data8);
        m_port.setParity(QSerialPort::NoParity);
        m_port.setStopBits(QSerialPort::OneStop);
        m_port.setFlowControl(QSerialPort::NoFlowControl);

        if (!m_port.open(QIODevice::ReadWrite)) {
            *detail = m_port.errorString();
            if (m_port.error() == QSerialPort::PermissionError) {
                *failure = FlexOpenFailure::Denied;
            } else if (m_port.error() == QSerialPort::DeviceNotFoundError) {
                *failure = FlexOpenFailure::Gone;
            } else {
                *failure = FlexOpenFailure::Other;
            }
            return false;
        }

        connect(&m_port, &QSerialPort::readyRead, this, [this] { emit bytesArrived(m_port.readAll()); });
        // Without this the driver had NO error path at all: a port that dropped
        // stayed "open" from Qt's point of view, readyRead simply never fired
        // again, and nothing retried — the knob was dead until the process (or on
        // Windows, the machine) restarted. Every other device driver in the tree
        // handles this; see AcomConnection's serial branch and MidiControlManager's
        // hotplug timer. (#4574)
        // [original inline comment from src/core/FlexControlManager.cpp:19-24]
        // Queued here, since this signal can fire from inside any call into the port.
        connect(&m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
            if (error == QSerialPort::NoError) {
                return;
            }
            const QString text = m_port.errorString();
            QMetaObject::invokeMethod(this, [this, text] { emit failed(text); }, Qt::QueuedConnection);
        });
        return true;
    }

    bool write(const QByteArray& bytes) override
    {
        // From AetherSDR src/core/FlexControlManager.cpp:328-343 [@1e0718ad]
        if (m_port.write(bytes) != bytes.size()) {
            return false;
        }
        m_port.flush();
        return true;
    }

    void finishWrites() override
    {
        // From AetherSDR src/core/FlexControlManager.cpp:228 [@1e0718ad]
        m_port.waitForBytesWritten(50);
    }

    void closePort() override
    {
        // clearError() first: on an errored port Qt can refuse further operations
        // until the error state is reset.
        // [original inline comment from src/core/FlexControlManager.cpp:170-171]
        m_port.clearError();
        m_port.close();
        m_port.clearError();
    }

private:
    QSerialPort m_port;
};

class RealSerialLayer : public FlexSerialLayer {
public:
    QString findPort(quint16 vendorId, quint16 productId) override
    {
        // From AetherSDR src/core/FlexControlManager.cpp:83-91 [@1e0718ad]
        const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        for (const QSerialPortInfo& info : ports) {
            if (info.vendorIdentifier() == vendorId && info.productIdentifier() == productId) {
                return info.portName();
            }
        }
        return {};
    }

    std::unique_ptr<FlexSerialHandle> open(const QString& portName, FlexOpenFailure* failure, QString* detail) override
    {
        auto handle = std::make_unique<RealSerialHandle>();
        if (!handle->open(portName, failure, detail)) {
            return nullptr;
        }
        return handle;
    }
};

#endif // HAVE_SERIALPORT

std::unique_ptr<FlexSerialLayer> makeLayer()
{
    if (testLayerFactory()) {
        return testLayerFactory()();
    }
#ifdef HAVE_SERIALPORT
    // Only the application turns this on. A test process links the same library and
    // never does, so no test can list or open a serial port on this computer.
    if (!DeviceInput::realDevicesAllowed()) {
        return std::make_unique<NoPortLayer>();
    }
    return std::make_unique<RealSerialLayer>();
#else
    return nullptr;
#endif
}

} // namespace

QString flexOpenFailureReason(FlexOpenFailure failure)
{
    switch (failure) {
    case FlexOpenFailure::Gone:
        return notPluggedInReason();
    case FlexOpenFailure::Denied:
        // Qt gives one error for a port another app holds and for a port this user
        // may not open.
#if defined(Q_OS_LINUX)
        return QStringLiteral("Another app is using it, or this computer's access rules block it. Close that app, "
                              "or copy 60-nereussdr-controllers.rules to /etc/udev/rules.d and plug it in again.");
#else
        return QStringLiteral("Another app is using it. Close that app and it connects on its own.");
#endif
    case FlexOpenFailure::Other:
        break;
    }
    return QStringLiteral("It couldn't be opened. Unplug it and plug it back in.");
}

FlexControlInput::FlexControlInput(QObject* parent)
    : QObject(parent)
    , m_layer(makeLayer())
    , m_scheduler(testScheduler())
    , m_retryTimer(new QTimer(this))
{
    qRegisterMetaType<DeviceId>();
    qRegisterMetaType<DeviceEvent>();
    // Parent the timer for exactly the reason m_port is parented above, and it
    // matters more here: MainWindow does
    //     m_flexControl = new FlexControlManager;
    //     m_flexControl->moveToThread(m_extCtrlThread);
    // (MainWindow_Controllers.cpp). An unparented QTimer member is not a child,
    // so moveToThread() leaves it on the GUI thread — and QTimer::start() from
    // the worker thread is then silently refused, so the whole recovery below
    // never runs. MidiControlManager avoids this by allocating its hotplug
    // timer as `new QTimer(this)`. (#4574)
    // [original inline comment from src/core/FlexControlManager.cpp:28-36]
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &FlexControlInput::retryNow);
}

FlexControlInput::~FlexControlInput()
{
    closeWithLightsOff();
}

bool FlexControlInput::supported()
{
#ifdef HAVE_SERIALPORT
    return true;
#else
    return false;
#endif
}

void FlexControlInput::setLayerFactoryForTest(LayerFactory factory)
{
    testLayerFactory() = std::move(factory);
}

void FlexControlInput::setSchedulerForTest(Scheduler scheduler)
{
    testScheduler() = std::move(scheduler);
}

void FlexControlInput::start()
{
    if (m_wanted) {
        return;
    }
    if (!m_layer) {
        notify(false, noSupportReason());
        return;
    }
    m_wanted = true;
    notify(false, notPluggedInReason());
    tryOpen();
}

void FlexControlInput::stop()
{
    // Operator-initiated: stop wanting a connection, so the retry timer does
    // not immediately undo this.
    // [original inline comment from src/core/FlexControlManager.cpp:205-206]
    m_wanted = false;
    m_retryTimer->stop();
    m_retryArmed = false;
    m_retryMs = kRetryMinMs;
    m_failureLogged = false;
    const bool wasOpen = m_port != nullptr;
    closeWithLightsOff();
    if (wasOpen) {
        notify(false, QString());
    }
    m_stateKnown = false;
}

void FlexControlInput::setLit(int lit)
{
    // From AetherSDR src/core/FlexControlManager.cpp:237-247 [@1e0718ad]
    if (lit < 1 || lit > 3) {
        lit = 0;
    }
    if (m_lit == lit) {
        return;
    }
    m_lit = lit;
    writeLights();
}

void FlexControlInput::retryNow()
{
    m_retryArmed = false;
    if (!m_wanted || m_port != nullptr) {
        return;
    }
    tryOpen();
}

void FlexControlInput::tryOpen()
{
    // Re-detect rather than reusing the old name: a USB re-enumeration can
    // hand the device a different COM port than it had before.
    // [original inline comment from src/core/FlexControlManager.cpp:52-53]
    const QString portName = m_layer->findPort(kVendorId, kProductId);
    if (portName.isEmpty()) {
        notify(false, notPluggedInReason());
        armRetry();
        return;
    }
    FlexOpenFailure failure = FlexOpenFailure::Other;
    QString detail;
    std::unique_ptr<FlexSerialHandle> handle = m_layer->open(portName, &failure, &detail);
    if (!handle) {
        // Warn once per outage.
        // [original inline comment from src/core/FlexControlManager.cpp:112]
        if (!m_failureLogged) {
            qCWarning(lcHotKeys) << "Couldn't open FlexControl on" << portName << detail;
            m_failureLogged = true;
        }
        notify(false, flexOpenFailureReason(failure));
        armRetry();
        return;
    }

    handle->setParent(this);
    m_port = handle.release();
    connect(m_port, &FlexSerialHandle::bytesArrived, this, &FlexControlInput::onBytes);
    connect(m_port, &FlexSerialHandle::failed, this, &FlexControlInput::onPortFailed);
    m_parser.reset();
    writeLights();
    // writeLedState() -> writeLedCommand() routes a short write into
    // releasePort(), which closes the port and emits connectionChanged(false).
    // Don't then announce a connection we no longer have: a device that is
    // present but wedged would otherwise leave AetherControl showing
    // "Connected" with no open port, and hand the caller a bogus true.
    // [original inline comment from src/core/FlexControlManager.cpp:130-134]
    if (m_port == nullptr) {
        return;
    }
    m_retryMs = kRetryMinMs;
    m_failureLogged = false;
    qCInfo(lcHotKeys) << "Opened FlexControl on" << portName;
    notify(true, QString());
}

void FlexControlInput::armRetry()
{
    if (!m_wanted || m_retryArmed) {
        return;
    }
    m_retryArmed = true;
    const int delay = m_retryMs;
    // Widen the retry interval towards kMaxReconnectIntervalMs. The first retries
    // stay at 2 s because that is the case worth being fast for (a replug); a
    // device that never opens settles into a 30 s poll instead of scanning and
    // warning every 2 s for the rest of the session.
    // [original inline comment from src/core/FlexControlManager.cpp:66-69]
    m_retryMs = qMin(m_retryMs * 2, kRetryMaxMs);
    if (m_scheduler) {
        m_scheduler(delay);
    } else {
        m_retryTimer->start(delay);
    }
}

void FlexControlInput::writeLights()
{
    if (m_port == nullptr) {
        return;
    }
    if (m_port->write(FlexControlParser::auxLights(m_lit))) {
        return;
    }
    // A short write means the port is already gone. This used to only warn,
    // so a device that died during a write stayed dead. Route it into the
    // same recovery as a read error. (#4574)
    // [original inline comment from src/core/FlexControlManager.cpp:332-334]
    qCWarning(lcHotKeys) << "A write failed on FlexControl";
    retirePort();
    notify(false, m_connected ? notPluggedInReason() : flexOpenFailureReason(FlexOpenFailure::Other));
    armRetry();
}

void FlexControlInput::onBytes(const QByteArray& bytes)
{
    qCDebug(lcHotKeyDevice) << "FlexControl" << bytes;
    const QList<DeviceEvent> events = m_parser.feed(bytes);
    if (m_parser.takeDeviceReset()) {
        writeLights();
    }
    for (const DeviceEvent& event : events) {
        emit deviceEvent(DeviceId::FlexControl, event);
    }
}

void FlexControlInput::onPortFailed(const QString& detail)
{
    // ResourceError / DeviceNotFound are the USB-went-away cases; the rest
    // (permission, framing, timeout) are treated the same way because the
    // recovery is identical and re-detecting a healthy device is cheap.
    // [original inline comment from src/core/FlexControlManager.cpp:146-148]
    if (!m_failureLogged) {
        qCInfo(lcHotKeys) << "FlexControl went away:" << detail;
        m_failureLogged = true;
    }
    retirePort();
    notify(false, notPluggedInReason());
    armRetry();
}

void FlexControlInput::retirePort()
{
    if (m_port == nullptr) {
        return;
    }
    // This can run inside the port's own signal, so it can't be deleted yet.
    FlexSerialHandle* const port = std::exchange(m_port, nullptr);
    port->disconnect(this);
    port->closePort();
    port->deleteLater();
    m_parser.reset();
}

void FlexControlInput::closeWithLightsOff()
{
    if (m_port == nullptr) {
        return;
    }
    // From AetherSDR src/core/FlexControlManager.cpp:226-229 [@1e0718ad]
    if (m_port->write(FlexControlParser::auxLights(0))) {
        m_port->finishWrites();
    }
    retirePort();
}

void FlexControlInput::notify(bool connected, const QString& reason)
{
    if (m_stateKnown && m_connected == connected && m_reason == reason) {
        return;
    }
    m_stateKnown = true;
    m_connected = connected;
    m_reason = reason;
    emit deviceStateChanged(DeviceId::FlexControl, connected, reason);
}

} // namespace NereusSDR
