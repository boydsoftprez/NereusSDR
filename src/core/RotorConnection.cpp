// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RotorConnection.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native; see RotorConnection.h for the cited protocol facts.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code. Rotor control plan, Task 3b.
//   2026-10-08: Final review fixes (span on the controller's reading, a
//               stop before a link closes mid-turn, arrival on the span,
//               plain fault words).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/RotorConnection.h"

#include "core/RotctldProcess.h"
#include "core/RotorHeading.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTcpSocket>

#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#endif

#include <algorithm>
#include <cmath>
#include <iterator>

namespace NereusSDR {

namespace {

Q_LOGGING_CATEGORY(lcRotor, "nereus.rotor")

// The reconnect schedule the TGXL connection uses (TgxlConnection.cpp
// kTgxlBackoffSec), in seconds.
constexpr int kBackoffSec[] = {1, 2, 5, 10, 30, 60};

// How long a TCP dial may take before it counts as failed.
constexpr int kTcpConnectTimeoutMs = 5000;

// A reply line longer than this without CR or LF is noise, not a reply.
constexpr int kMaxLineBytes = 256;

// A heading change below this is the same reading (replies are whole
// degrees on the GS-232; rotctld gives decimals).
constexpr double kSameHeadingDeg = 0.5;

// Plausible raw replies. GS-232B on a 450-degree Yaesu reads up to 450;
// some Hamlib backends report azimuth as -180 to 180.
constexpr double kMinReportedAz = -180.0;
constexpr double kMaxReportedAz = 450.0;
constexpr double kMinReportedEl = -90.0;
constexpr double kMaxReportedEl = 180.0;
constexpr double kMaxElevationTargetDeg = 90.0;

double circularGap(double a, double b)
{
    double d = std::fmod(std::abs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

// ── Fault words ──────────────────────────────────────────────────────
//
// Final review M8: a fault the operator reads is said in plain words with
// what to check; the raw Qt, OS or Hamlib text goes to the log.

QString noAnswerReason(const QString& host, quint16 port)
{
    return QStringLiteral("No answer from %1 port %2. Check the address and that the "
                          "rotor controller is on.")
        .arg(host).arg(port);
}

QString unreachableReason(const QString& host, quint16 port, QAbstractSocket::SocketError error)
{
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        return QStringLiteral("Nothing answered at %1 port %2. Check the port and that the "
                              "rotor controller is on.")
            .arg(host).arg(port);
    case QAbstractSocket::HostNotFoundError:
        return QStringLiteral("The address %1 was not found. Check the rotor's address in "
                              "Setup.")
            .arg(host);
    default:
        return QStringLiteral("Could not reach %1 port %2. Check the address and the "
                              "network.")
            .arg(host).arg(port);
    }
}

// What a controller's nonzero RPRT means to the operator; the code is
// logged where it is read.
QString controllerRefusedReason()
{
    return QStringLiteral("The rotor controller refused that command.");
}

// Hamlib's rotctld exiting, from the cause line RotctldProcess picked out
// of its stderr (empty when none named one).
QString rotctldStoppedReason(const QString& cause)
{
    const auto says = [&cause](const char* word) {
        return cause.contains(QLatin1String(word), Qt::CaseInsensitive);
    };
    if (says("timed out")) {
        return QStringLiteral("Hamlib's rotctld stopped: the rotor controller did not answer "
                              "in time. Check the rotor's port and that it is on.");
    }
    if (says("refused")) {
        return QStringLiteral("Hamlib's rotctld stopped: the rotor controller turned the "
                              "connection away. Check the rotor's port in Setup.");
    }
    if (says("no such")) {
        return QStringLiteral("Hamlib's rotctld stopped: the rotor's port was not found. "
                              "Check it is plugged in and named right in Setup.");
    }
    if (says("in use")) {
        return QStringLiteral("Hamlib's rotctld stopped: another program is using the "
                              "rotor's port.");
    }
    if (says("permission") || says("not permitted")) {
        return QStringLiteral("Hamlib's rotctld stopped: this computer's account may not "
                              "open the rotor's port.");
    }
    return QStringLiteral("Hamlib's rotctld stopped. Check the rotor's port and model in "
                          "Setup.");
}

#ifdef HAVE_SERIALPORT
QString serialOpenReason(const QString& portName, QSerialPort::SerialPortError error)
{
    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        return QStringLiteral("The serial port %1 was not found. Check it is plugged in.")
            .arg(portName);
    case QSerialPort::PermissionError:
        return QStringLiteral("The serial port %1 is in use by another program, or this "
                              "computer's account may not open it.")
            .arg(portName);
    default:
        return QStringLiteral("Could not open the serial port %1. Check it is plugged in "
                              "and not in use.")
            .arg(portName);
    }
}

QString serialLostReason(const QString& portName)
{
    return QStringLiteral("The serial port %1 went away. Check the cable.").arg(portName);
}
#endif

// ── Production transports ────────────────────────────────────────────

class TcpRotorTransport final : public RotorTransport {
public:
    TcpRotorTransport(QString host, quint16 port)
        : m_host(std::move(host)), m_port(port)
    {
        m_connectTimer.setSingleShot(true);
        connect(&m_connectTimer, &QTimer::timeout, this, [this]() {
            m_socket.abort();
            emit failed(noAnswerReason(m_host, m_port));
        });
        connect(&m_socket, &QTcpSocket::connected, this, [this]() {
            m_connectTimer.stop();
            m_open = true;
            emit opened();
        });
        connect(&m_socket, &QTcpSocket::readyRead,
                this, &RotorTransport::readyRead);
        connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
            if (!m_open) { return; }
            m_open = false;
            emit dropped(QStringLiteral("The rotor controller closed the connection."));
        });
        connect(&m_socket, &QTcpSocket::errorOccurred, this,
                [this](QAbstractSocket::SocketError error) {
            if (m_open) {
                // disconnected() follows for a link that was up.
                return;
            }
            m_connectTimer.stop();
            qCWarning(lcRotor).noquote() << "could not reach" << m_host << "port" << m_port
                                         << "-" << m_socket.errorString();
            m_socket.abort();
            emit failed(unreachableReason(m_host, m_port, error));
        });
    }

    void open() override
    {
        m_open = false;
        m_connectTimer.start(kTcpConnectTimeoutMs);
        m_socket.connectToHost(m_host, m_port);
    }

    void close() override
    {
        m_connectTimer.stop();
        m_open = false;
        QObject::disconnect(&m_socket, nullptr, this, nullptr);
        m_socket.abort();
    }

    qint64 write(const QByteArray& bytes) override { return m_socket.write(bytes); }
    QByteArray readAll() override { return m_socket.readAll(); }

    void flush(int msecs, bool awaitReply) override
    {
        if (m_socket.state() != QAbstractSocket::ConnectedState) { return; }
        m_socket.waitForBytesWritten(msecs);
        if (awaitReply) { m_socket.waitForReadyRead(msecs); }
    }

private:
    QString    m_host;
    quint16    m_port{0};
    QTcpSocket m_socket;
    QTimer     m_connectTimer;
    bool       m_open{false};
};

#ifdef HAVE_SERIALPORT
class SerialRotorTransport final : public RotorTransport {
public:
    SerialRotorTransport(QString portName, int baud)
        : m_portName(std::move(portName)), m_baud(baud)
    {
        connect(&m_port, &QSerialPort::readyRead,
                this, &RotorTransport::readyRead);
        connect(&m_port, &QSerialPort::errorOccurred, this,
                [this](QSerialPort::SerialPortError error) {
            // An unplugged USB serial adapter reports a resource error.
            if (m_open && error == QSerialPort::ResourceError) {
                m_open = false;
                qCWarning(lcRotor).noquote() << "serial port" << m_portName << "went away -"
                                             << m_port.errorString();
                m_port.close();
                emit dropped(serialLostReason(m_portName));
            }
        });
    }

    void open() override
    {
        m_port.setPortName(m_portName);
        m_port.setBaudRate(m_baud);
        // GS-232: 8N1, no handshake (Hamlib gs232a.c / gs232b.c; the ERC
        // capture ran 9600 8N1).
        m_port.setDataBits(QSerialPort::Data8);
        m_port.setParity(QSerialPort::NoParity);
        m_port.setStopBits(QSerialPort::OneStop);
        m_port.setFlowControl(QSerialPort::NoFlowControl);
        if (!m_port.open(QIODevice::ReadWrite)) {
            qCWarning(lcRotor).noquote() << "could not open serial port" << m_portName << "-"
                                         << m_port.errorString();
            emit failed(serialOpenReason(m_portName, m_port.error()));
            return;
        }
        m_open = true;
        emit opened();
    }

    void close() override
    {
        m_open = false;
        if (m_port.isOpen()) { m_port.close(); }
    }

    qint64 write(const QByteArray& bytes) override { return m_port.write(bytes); }
    QByteArray readAll() override { return m_port.readAll(); }

    void flush(int msecs, bool awaitReply) override
    {
        if (!m_port.isOpen()) { return; }
        m_port.waitForBytesWritten(msecs);
        if (awaitReply) { m_port.waitForReadyRead(msecs); }
    }

private:
    QString     m_portName;
    int         m_baud{9600};
    QSerialPort m_port;
    bool        m_open{false};
};
#else
class SerialRotorTransport final : public RotorTransport {
public:
    SerialRotorTransport(QString portName, int)
        : m_portName(std::move(portName)) {}

    void open() override
    {
        emit failed(QStringLiteral("This build cannot open serial ports, so %1 "
                                   "cannot be used.").arg(m_portName));
    }
    void close() override {}
    qint64 write(const QByteArray&) override { return -1; }
    QByteArray readAll() override { return {}; }

private:
    QString m_portName;
};
#endif

std::unique_ptr<RotorTransport> makeTransport(const RotorTransportTarget& t)
{
    if (t.serial) {
        return std::make_unique<SerialRotorTransport>(t.serialPort, t.baud);
    }
    return std::make_unique<TcpRotorTransport>(t.host, t.port);
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────

RotorConnection::RotorConnection(QObject* parent)
    : QObject(parent)
    , m_rotctld(std::make_unique<RotctldProcess>())
{
    m_tracker.configure(m_config.endStop, m_config.rangeDeg);

    m_pollTimer.setInterval(m_timing.pollStillMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &RotorConnection::onPollTick);

    m_replyTimer.setSingleShot(true);
    connect(&m_replyTimer, &QTimer::timeout, this, &RotorConnection::onReplyTimeout);

    m_staleTimer.setSingleShot(true);
    // A coarse timer may fire 5% early; stale means 1500 ms, not 1425.
    m_staleTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_staleTimer, &QTimer::timeout, this, &RotorConnection::onStale);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &RotorConnection::onReconnectTimeout);

    m_answerTimer.setSingleShot(true);
    connect(&m_answerTimer, &QTimer::timeout, this, &RotorConnection::onAnswerDeadline);

    m_dialTimer.setSingleShot(true);
    connect(&m_dialTimer, &QTimer::timeout, this, [this]() {
        if (m_wantConnected) { openTransport(); }
    });

    connect(m_rotctld.get(), &RotctldProcess::exited, this,
            [this](int, const QString& stderrText) {
        if (m_config.driver != RotorDriver::RotctldStarted || !m_wantConnected) {
            return;
        }
        m_dialTimer.stop();
        const bool wasConnected = m_connected;
        closeTransport();
        if (wasConnected) { emit disconnected(); }
        const QString why = RotctldProcess::reasonFromStderr(stderrText);
        if (!why.isEmpty()) {
            qCWarning(lcRotor).noquote() << "rotctld stopped:" << why;
        }
        fail(rotctldStoppedReason(why));
        scheduleReconnect();
    });

    // A quitting Core (nereusd's shutdown, a window closing) stops a turn
    // before anything closes. RotctldProcess stops rotctld on aboutToQuit
    // by itself; that would take rotctld away before the stop could pass
    // through it, so this hook replaces it: the stop first, then rotctld.
    if (QCoreApplication* app = QCoreApplication::instance()) {
        QObject::disconnect(app, &QCoreApplication::aboutToQuit,
                            m_rotctld.get(), &RotctldProcess::stop);
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() {
            stopBeforeClose();
            m_rotctld->stop();
        });
    }
}

RotorConnection::~RotorConnection()
{
    // No signals to an owner that is itself being torn down.
    const QSignalBlocker block(this);
    m_wantConnected = false;
    stopBeforeClose();
    closeTransport();
    if (m_rotctld) { m_rotctld->stop(); }
}

void RotorConnection::configure(const RotorConfig& config)
{
    m_config = config;
    m_config.rangeDeg = RotorRoute::clampRangeDeg(config.rangeDeg);
    m_tracker.configure(m_config.endStop, m_config.rangeDeg);
}

void RotorConnection::setTransportFactoryForTesting(TransportFactory factory)
{
    m_factory = std::move(factory);
}

void RotorConnection::setTimingForTesting(const Timing& timing)
{
    m_timing = timing;
    updatePollInterval();
}

// ── Drivers ──────────────────────────────────────────────────────────

bool RotorConnection::isSerialDriver() const
{
    return m_config.driver == RotorDriver::Gs232a
        || m_config.driver == RotorDriver::Gs232b;
}

bool RotorConnection::isRotctldDriver() const
{
    return m_config.driver == RotorDriver::Rotctld
        || m_config.driver == RotorDriver::RotctldStarted;
}

RotorDriver RotorConnection::wireDriver() const
{
    // Driver 4 runs rotctld on the Core's loopback and talks to it as
    // driver 3 (remote rotor control v1, driver table).
    return m_config.driver == RotorDriver::RotctldStarted
        ? RotorDriver::Rotctld : m_config.driver;
}

QByteArray RotorConnection::pollCommand(RotorDriver driver)
{
    switch (driver) {
    case RotorDriver::Gs232a:
    case RotorDriver::Gs232b:
        // `C2`: azimuth and elevation (gs232a.c, gs232b.c). The ERC
        // answers `AZ=302  EL=000` even on an azimuth rotor.
        return QByteArrayLiteral("C2\r");
    case RotorDriver::Rotctld:
    case RotorDriver::RotctldStarted:
        return QByteArrayLiteral("p\n");
    case RotorDriver::None:
        break;
    }
    return {};
}

QByteArray RotorConnection::setCommand(RotorDriver driver, double azimuthDeg,
                                       double elevationDeg)
{
    switch (driver) {
    case RotorDriver::Gs232a:
    case RotorDriver::Gs232b: {
        // `Waaa eee`, three digits each, as Hamlib sends and as the ERC
        // accepted (`W292 000`); elevation 000 on an azimuth rotor.
        const int az = qRound(RotorRoute::wrap360(azimuthDeg)) % 360;
        const int el = std::clamp(qRound(elevationDeg), 0, 180);
        return QStringLiteral("W%1 %2\r")
            .arg(az, 3, 10, QLatin1Char('0'))
            .arg(el, 3, 10, QLatin1Char('0'))
            .toLatin1();
    }
    case RotorDriver::Rotctld:
    case RotorDriver::RotctldStarted:
        // `P az el` with two decimals. QString::arg(double) formats in the
        // C locale whatever the user's, so a decimal comma never reaches
        // rotctld (after Longpath RotctldClient::moveCommand).
        return QStringLiteral("P %1 %2\n")
            .arg(RotorRoute::wrap360(azimuthDeg), 0, 'f', 2)
            .arg(elevationDeg, 0, 'f', 2)
            .toLatin1();
    case RotorDriver::None:
        break;
    }
    return {};
}

QByteArray RotorConnection::stopCommand(RotorDriver driver)
{
    switch (driver) {
    case RotorDriver::Gs232a:
    case RotorDriver::Gs232b:
        return QByteArrayLiteral("S\r");
    case RotorDriver::Rotctld:
    case RotorDriver::RotctldStarted:
        return QByteArrayLiteral("S\n");
    case RotorDriver::None:
        break;
    }
    return {};
}

QByteArray RotorConnection::moveCommand(RotorDriver driver, RotorDirection direction)
{
    switch (driver) {
    case RotorDriver::Gs232a:
    case RotorDriver::Gs232b:
        switch (direction) {
        case RotorDirection::Ccw:  return QByteArrayLiteral("L\r");
        case RotorDirection::Cw:   return QByteArrayLiteral("R\r");
        case RotorDirection::Down: return QByteArrayLiteral("D\r");
        case RotorDirection::Up:   return QByteArrayLiteral("U\r");
        }
        break;
    case RotorDriver::Rotctld:
    case RotorDriver::RotctldStarted: {
        // rotctld(1) `M dir speed`: 2 up, 4 down, 8 left, 16 right.
        int dir = 0;
        switch (direction) {
        case RotorDirection::Ccw:  dir = 8;  break;
        case RotorDirection::Cw:   dir = 16; break;
        case RotorDirection::Down: dir = 4;  break;
        case RotorDirection::Up:   dir = 2;  break;
        }
        return QStringLiteral("M %1 %2\n").arg(dir).arg(kRotctldMoveSpeed).toLatin1();
    }
    case RotorDriver::None:
        break;
    }
    return {};
}

// ── Connecting ───────────────────────────────────────────────────────

bool RotorConnection::connectToRotor()
{
    m_reconnectTimer.stop();
    m_dialTimer.stop();
    // A reconnect (a new setup) mid-turn stops the old turn first.
    stopBeforeClose();
    closeTransport();

    if (m_config.driver == RotorDriver::None) {
        m_wantConnected = false;
        fail(QStringLiteral("No rotor is set up on this Core."));
        return false;
    }

    m_wantConnected = true;
    m_reconnectAttempts = 0;

    if (m_config.driver == RotorDriver::RotctldStarted) {
        QString error;
        if (!startRotctld(&error)) {
            m_wantConnected = false;
            fail(error);
            return false;
        }
        m_dialTimer.start(m_timing.rotctldStartDelayMs);
        return true;
    }
    m_rotctld->stop();
    openTransport();
    return true;
}

bool RotorConnection::startRotctld(QString* error)
{
    if (m_rotctld->isRunning()) { return true; }
    const quint16 preferred = m_config.port != 0 ? m_config.port : kRotctldDefaultPort;
    return m_rotctld->start(m_config.hamlibModel, m_config.serialPort,
                            m_config.baud, preferred, error);
}

void RotorConnection::disconnectFromRotor()
{
    m_wantConnected = false;
    m_reconnectTimer.stop();
    m_dialTimer.stop();
    const bool wasConnected = m_connected;
    stopBeforeClose();
    closeTransport();
    m_rotctld->stop();
    if (wasConnected) { emit disconnected(); }
}

quint16 RotorConnection::rotctldPort() const
{
    if (m_config.driver != RotorDriver::RotctldStarted || !m_rotctld->isRunning()) {
        return 0;
    }
    return m_rotctld->listenPort();
}

void RotorConnection::openTransport()
{
    closeTransport();

    RotorTransportTarget target;
    if (isSerialDriver()) {
        target.serial = true;
        target.serialPort = m_config.serialPort;
        target.baud = m_config.baud;
    } else if (m_config.driver == RotorDriver::RotctldStarted) {
        target.host = QStringLiteral("127.0.0.1");
        target.port = m_rotctld->listenPort();
    } else {
        target.host = m_config.host;
        target.port = m_config.port;
    }

    m_transport = m_factory ? m_factory(target) : makeTransport(target);
    if (!m_transport) {
        onTransportFailed(QStringLiteral("The rotor connection could not be made."));
        return;
    }
    RotorTransport* t = m_transport.get();
    connect(t, &RotorTransport::opened, this, &RotorConnection::onOpened);
    connect(t, &RotorTransport::failed, this, &RotorConnection::onTransportFailed);
    connect(t, &RotorTransport::dropped, this, &RotorConnection::onDropped);
    connect(t, &RotorTransport::readyRead, this, &RotorConnection::onReadyRead);
    t->open();
}

// Only for a close the Core chose (disconnect, a new setup, teardown):
// after a failure or a drop the link is gone and there is nothing to
// write to. A queued move that never left is stopped too, harmlessly.
void RotorConnection::stopBeforeClose()
{
    if (!m_transport || !m_connected || !(m_hasTarget || m_moveActive)) { return; }
    qCInfo(lcRotor) << "closing the link during a turn; sending a stop first";
    // Nothing more is read from this link: the stop's answer is not parsed.
    QObject::disconnect(m_transport.get(), &RotorTransport::readyRead,
                        this, &RotorConnection::onReadyRead);
    m_transport->write(stopCommand(wireDriver()));
    m_transport->flush(kStopFlushMs, isRotctldDriver());
    m_hasTarget = false;
    m_moveActive = false;
}

// Closes the transport and forgets the link: queue, replies awaited,
// position, target. Emits nothing but positionFreshChanged and
// turningChanged.
void RotorConnection::closeTransport()
{
    if (m_transport) {
        RotorTransport* t = m_transport.release();
        QObject::disconnect(t, nullptr, this, nullptr);
        t->close();
        // Possibly inside one of its own signals: delete it later.
        t->deleteLater();
    }
    m_connected = false;
    m_linkOpen = false;
    m_answerTimer.stop();
    m_pollTimer.stop();
    m_replyTimer.stop();
    m_staleTimer.stop();
    m_rx.clear();
    m_queue.clear();
    m_inFlight.clear();
    m_hasTarget = false;
    m_moveActive = false;
    setTurning(false);
    resetPosition();
}

void RotorConnection::resetPosition()
{
    m_haveAzimuth = false;
    m_azimuthDeg = -1.0;
    m_elevationDeg = -1.0;
    m_lastChangeHeading = -1.0;
    m_tracker.markStale();
    if (m_positionFresh) {
        m_positionFresh = false;
        emit positionFreshChanged(false);
    }
}

void RotorConnection::onOpened()
{
    m_linkOpen = true;
    m_rx.clear();
    m_queue.clear();
    m_inFlight.clear();
    if (isSerialDriver()) {
        // Bench fix: an open serial port is not a rotor. JJ's Core said
        // "connected" on /dev/ttyFIQ0 (the board's debug console), where
        // no heading ever came. Poll, and call it connected on the first
        // position reply (acceptPosition()); none within the deadline is a
        // fault.
        qCInfo(lcRotor) << "port open, waiting for the controller to answer";
        m_answerTimer.start(m_timing.answerDeadlineMs);
    } else {
        m_connected = true;
        m_reconnectAttempts = 0;
        m_lastError.clear();
        qCInfo(lcRotor) << "connected, driver" << static_cast<int>(m_config.driver);
        emit connected();
        if (!m_connected) { return; }   // a slot disconnected us
    }
    updatePollInterval();
    m_pollTimer.start();
    onPollTick();
}

QString RotorConnection::notAnsweringReason(const QString& serialPort)
{
    return QStringLiteral("The rotor controller on %1 is not answering. "
                          "Check the serial port and the baud rate.")
        .arg(serialPort);
}

void RotorConnection::onAnswerDeadline()
{
    if (m_connected || !m_linkOpen) { return; }
    closeTransport();
    fail(notAnsweringReason(m_config.serialPort));
    scheduleReconnect();
}

void RotorConnection::onTransportFailed(const QString& reason)
{
    closeTransport();
    fail(reason);
    scheduleReconnect();
}

void RotorConnection::onDropped(const QString& reason)
{
    const bool wasConnected = m_connected;
    closeTransport();
    if (wasConnected) { emit disconnected(); }
    fail(reason);
    scheduleReconnect();
}

void RotorConnection::fail(const QString& reason)
{
    m_lastError = reason;
    qCWarning(lcRotor) << reason;
    emit connectionFailed(reason);
}

void RotorConnection::scheduleReconnect()
{
    if (!m_wantConnected || m_reconnectTimer.isActive()) { return; }
    constexpr int kSteps = static_cast<int>(std::size(kBackoffSec));
    const int step = std::min(m_reconnectAttempts, kSteps - 1);
    const int delayMs = kBackoffSec[step] * m_timing.reconnectUnitMs;
    ++m_reconnectAttempts;
    m_reconnectTimer.start(delayMs);
    emit reconnectScheduled(m_reconnectAttempts, delayMs);
}

void RotorConnection::onReconnectTimeout()
{
    if (!m_wantConnected) { return; }
    if (m_config.driver == RotorDriver::RotctldStarted) {
        if (!m_rotctld->isRunning()) {
            QString error;
            if (!startRotctld(&error)) {
                fail(error);
                scheduleReconnect();
                return;
            }
            m_dialTimer.start(m_timing.rotctldStartDelayMs);
            return;
        }
    }
    openTransport();
}

// ── Commands ─────────────────────────────────────────────────────────

void RotorConnection::writeNow(const Outgoing& out)
{
    if (!m_transport) { return; }
    m_transport->write(out.bytes);
    if (out.expect != Expect::Nothing) {
        m_inFlight.push_back(out.expect);
        if (!m_replyTimer.isActive()) { m_replyTimer.start(m_timing.replyTimeoutMs); }
    }
}

void RotorConnection::pump()
{
    // One reply awaited at a time: a command waits behind an outstanding
    // poll (at most about 210 ms on the ERC) rather than interleave.
    while (m_linkOpen && !m_queue.empty() && m_inFlight.empty()) {
        const Outgoing out = m_queue.front();
        m_queue.pop_front();
        writeNow(out);
    }
}

void RotorConnection::enqueue(const Outgoing& out)
{
    m_queue.push_back(out);
    pump();
}

bool RotorConnection::setTarget(double azimuthDeg, double elevationDeg)
{
    if (!m_connected) { return false; }

    double az = 0.0;
    if (!RotorHeading::accept(azimuthDeg, &az)) { return false; }

    const bool azEl = m_config.axes == RotorAxes::AzimuthElevation;
    const bool leaveElevation = (elevationDeg == -1.0);
    if (!leaveElevation) {
        if (!azEl) { return false; }
        if (!std::isfinite(elevationDeg) || elevationDeg < 0.0
            || elevationDeg > kMaxElevationTargetDeg) {
            return false;
        }
    }

    // `P az el` and `Waaa eee` set both axes: leaving elevation sends the
    // one last read, never 0, so an az/el rotor's elevation stays put
    // (after Longpath RotctldClient::moveTo). An azimuth rotor sends 0.
    double sendEl = 0.0;
    if (!leaveElevation) {
        sendEl = elevationDeg;
    } else if (azEl && m_elevationDeg >= 0.0) {
        sendEl = m_elevationDeg;
    }

    const RotorDriver wire = wireDriver();
    const double sendAz = RotorRoute::removeOffset(az, m_config.offsetDeg);
    enqueue(Outgoing{setCommand(wire, sendAz, sendEl),
                     isRotctldDriver() ? Expect::Report : Expect::Nothing,
                     true});

    m_hasTarget = true;
    m_targetAz = az;
    m_targetEl = leaveElevation ? -1.0 : elevationDeg;
    m_moveActive = false;
    m_lastChangeHeading = m_azimuthDeg;
    m_lastChangeMs = QDateTime::currentMSecsSinceEpoch();
    setTurning(true);
    return true;
}

void RotorConnection::stop()
{
    if (!m_connected) { return; }

    // Stop wins: queued turns are dropped, and the stop is written now,
    // ahead of anything queued and without waiting for a reply.
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [](const Outgoing& o) { return o.motion; }),
                  m_queue.end());
    writeNow(Outgoing{stopCommand(wireDriver()),
                      isRotctldDriver() ? Expect::Report : Expect::Nothing,
                      false});

    m_hasTarget = false;
    m_moveActive = false;
    // The ERC coasts 2 to 4 degrees after `S` during a move: keep the
    // fast polls until the heading settles.
    m_lastChangeHeading = m_azimuthDeg;
    m_lastChangeMs = QDateTime::currentMSecsSinceEpoch();
    setTurning(true);
}

bool RotorConnection::startMove(RotorDirection direction)
{
    if (!m_connected) { return false; }
    const bool elevation = direction == RotorDirection::Up
                        || direction == RotorDirection::Down;
    if (elevation && m_config.axes != RotorAxes::AzimuthElevation) { return false; }

    enqueue(Outgoing{moveCommand(wireDriver(), direction),
                     isRotctldDriver() ? Expect::Report : Expect::Nothing,
                     true});
    m_hasTarget = false;
    m_moveActive = true;
    setTurning(true);
    return true;
}

// ── Polling ──────────────────────────────────────────────────────────

void RotorConnection::setTurning(bool turning)
{
    if (m_turning == turning) { return; }
    m_turning = turning;
    updatePollInterval();
    emit turningChanged(turning);
}

void RotorConnection::updatePollInterval()
{
    const int interval = m_turning ? m_timing.pollTurningMs : m_timing.pollStillMs;
    if (m_pollTimer.interval() == interval) { return; }
    if (m_pollTimer.isActive()) {
        m_pollTimer.start(interval);
    } else {
        m_pollTimer.setInterval(interval);
    }
}

void RotorConnection::onPollTick()
{
    if (!m_linkOpen) { return; }

    // Stopped changing: arrived, at a stop, or coasted to rest.
    if (m_turning && !m_moveActive
        && QDateTime::currentMSecsSinceEpoch() - m_lastChangeMs >= m_timing.settleMs) {
        m_hasTarget = false;
        setTurning(false);
    }

    // Never a poll while one is outstanding, and never ahead of a command.
    if (m_inFlight.empty() && m_queue.empty()) {
        writeNow(Outgoing{pollCommand(wireDriver()), Expect::Position, false});
    }
}

void RotorConnection::onReplyTimeout()
{
    if (m_inFlight.empty()) { return; }

    if (isRotctldDriver()) {
        // A rotctld that stops answering while its TCP side stays open
        // never recovers on its own: cut the link and dial again (after
        // Longpath's RotctldClient reply watchdog).
        onDropped(QStringLiteral("The rotor controller stopped answering."));
        return;
    }

    // Serial: give the poll up and carry on; staleness shows it.
    qCDebug(lcRotor) << "no reply within" << m_timing.replyTimeoutMs << "ms";
    m_inFlight.clear();
    m_rx.clear();
    pump();
}

void RotorConnection::onStale()
{
    m_tracker.markStale();
    if (m_positionFresh) {
        m_positionFresh = false;
        emit positionFreshChanged(false);
    }
}

// ── Replies ──────────────────────────────────────────────────────────

void RotorConnection::onReadyRead()
{
    if (!m_transport) { return; }
    m_rx += m_transport->readAll();
    if (isSerialDriver()) {
        parseGs232();
    } else {
        parseRotctld();
    }
}

void RotorConnection::parseGs232()
{
    // GS-232B: `AZ=aaa EL=eee` or `AZ=aaa` (gs232b.c); the ERC puts two
    // spaces before EL.
    static const QRegularExpression kGs232b(QStringLiteral(
        "^AZ=\\s*(\\d{1,3}(?:\\.\\d+)?)(?:\\s+EL=\\s*(\\d{1,3}(?:\\.\\d+)?))?$"));
    // GS-232A: `+0aaa+0eee` ending LF (gs232a.c). No GS-232A controller
    // has been observed on the bench; this is Hamlib's format only.
    static const QRegularExpression kGs232a(QStringLiteral(
        "^\\+(\\d{4})(?:\\+(\\d{4}))?$"));

    while (true) {
        qsizetype end = -1;
        for (qsizetype i = 0; i < m_rx.size(); ++i) {
            if (m_rx.at(i) == '\r' || m_rx.at(i) == '\n') { end = i; break; }
        }
        if (end < 0) {
            if (m_rx.size() > kMaxLineBytes) { m_rx.clear(); }
            break;
        }
        const QString token = QString::fromLatin1(m_rx.left(end)).trimmed();
        m_rx.remove(0, end + 1);

        // The bare CR acknowledging a command, the empty piece between CR
        // and LF, and a `>` prompt carry no position (Hamlib skips bare
        // CR LF and `>` as invalid replies).
        if (token.isEmpty() || token == QLatin1String(">")) { continue; }

        const QRegularExpressionMatch m =
            (m_config.driver == RotorDriver::Gs232a ? kGs232a : kGs232b).match(token);
        if (!m.hasMatch()) {
            qCDebug(lcRotor) << "not a position reply:" << token;
            continue;
        }
        if (!m_inFlight.empty() && m_inFlight.front() == Expect::Position) {
            m_inFlight.pop_front();
        }
        if (m_inFlight.empty()) { m_replyTimer.stop(); }

        const double az = m.captured(1).toDouble();
        const bool haveEl = !m.captured(2).isEmpty();
        acceptPosition(az, haveEl ? m.captured(2).toDouble() : 0.0, haveEl);
        pump();
    }
}

void RotorConnection::parseRotctld()
{
    while (true) {
        const qsizetype first = m_rx.indexOf('\n');
        if (first < 0) {
            if (m_rx.size() > kMaxLineBytes) { m_rx.clear(); }
            break;
        }
        const QByteArray line1 = m_rx.left(first).trimmed();

        if (m_inFlight.empty()) {
            qCDebug(lcRotor) << "unexpected rotctld line:" << line1;
            m_rx.remove(0, first + 1);
            continue;
        }

        const Expect expect = m_inFlight.front();

        // An RPRT line is never a position: `RPRT -1` would otherwise
        // parse as the number -1 (after Longpath RotctldClient).
        if (line1.startsWith("RPRT")) {
            m_rx.remove(0, first + 1);
            m_inFlight.pop_front();
            bool ok = false;
            const int code = line1.mid(4).trimmed().toInt(&ok);
            if (ok && code != 0) {
                qCWarning(lcRotor) << "rotctld answered" << line1;
                emit rotorError(code, controllerRefusedReason());
            }
        } else if (expect == Expect::Position) {
            // `p` answers azimuth then elevation, one per line.
            const qsizetype second = m_rx.indexOf('\n', first + 1);
            if (second < 0) { break; }
            const QByteArray line2 = m_rx.mid(first + 1, second - first - 1).trimmed();
            m_rx.remove(0, second + 1);
            m_inFlight.pop_front();
            bool okAz = false;
            bool okEl = false;
            const double az = line1.toDouble(&okAz);
            const double el = line2.toDouble(&okEl);
            if (okAz && okEl) {
                acceptPosition(az, el, true);
            } else {
                qCDebug(lcRotor) << "not a position reply:" << line1 << line2;
            }
        } else {
            qCDebug(lcRotor) << "not an RPRT reply:" << line1;
            m_rx.remove(0, first + 1);
            m_inFlight.pop_front();
        }

        if (m_inFlight.empty()) {
            m_replyTimer.stop();
        } else {
            m_replyTimer.start(m_timing.replyTimeoutMs);
        }
        pump();
    }
}

void RotorConnection::acceptPosition(double reportedAz, double reportedEl, bool haveEl)
{
    if (!std::isfinite(reportedAz) || reportedAz < kMinReportedAz
        || reportedAz > kMaxReportedAz) {
        qCDebug(lcRotor) << "azimuth out of range:" << reportedAz;
        return;
    }

    if (!m_connected && m_linkOpen) {
        // The first position reply: the controller is there.
        m_answerTimer.stop();
        m_connected = true;
        m_reconnectAttempts = 0;
        m_lastError.clear();
        qCInfo(lcRotor) << "connected, driver" << static_cast<int>(m_config.driver);
        emit connected();
        if (!m_connected) { return; }   // a slot disconnected us
    }

    // The span is the controller's: its end stops and overlap are where
    // its own reading stops, so the tracker follows the reply before the
    // offset, and the offset is applied to the heading that is shown.
    m_tracker.update(reportedAz);
    const double heading = RotorRoute::applyOffset(reportedAz, m_config.offsetDeg);
    m_azimuthDeg = heading;
    m_haveAzimuth = true;

    if (m_config.axes == RotorAxes::AzimuthElevation && haveEl
        && std::isfinite(reportedEl) && reportedEl >= kMinReportedEl
        && reportedEl <= kMaxReportedEl) {
        m_elevationDeg = reportedEl;
    } else if (m_config.axes == RotorAxes::Azimuth) {
        m_elevationDeg = -1.0;
    }

    m_staleTimer.start(m_timing.staleMs);
    if (!m_positionFresh) {
        m_positionFresh = true;
        emit positionFreshChanged(true);
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastChangeHeading < 0.0
        || circularGap(heading, m_lastChangeHeading) >= kSameHeadingDeg) {
        m_lastChangeHeading = heading;
        m_lastChangeMs = now;
    }

    if (m_turning && !m_moveActive) {
        bool arrived = false;
        // Arrival is judged on the span once the route is known (final
        // review M1): on a rotor with overlap a compass heading names two
        // span positions, and only the one the route goes to is arrival.
        // With the nearer-position route the two rules agree; the span is
        // the one that cannot be fooled. Compass otherwise.
        const RotorRoute::Move route = routeToTarget();
        const double gap = route.routeKnown && m_config.endStop != RotorRoute::EndStop::None
                               ? std::abs(route.travelDeg)
                               : circularGap(heading, m_targetAz);
        if (m_hasTarget && gap <= kArrivedDeg) {
            arrived = m_targetEl < 0.0 || m_elevationDeg < 0.0
                   || std::abs(m_elevationDeg - m_targetEl) <= kArrivedDeg;
        }
        if (arrived || now - m_lastChangeMs >= m_timing.settleMs) {
            m_hasTarget = false;
            setTurning(false);
        }
    }

    emit positionUpdated();
}

RotorRoute::Move RotorConnection::routeToTarget() const
{
    if (!m_hasTarget) { return RotorRoute::Move{}; }
    // setTarget sends the target with the offset removed; the controller
    // plans from there on its own span, and so does this.
    return m_tracker.planTo(RotorRoute::removeOffset(m_targetAz, m_config.offsetDeg));
}

double RotorConnection::azimuthDeg() const
{
    return m_haveAzimuth ? m_azimuthDeg : -1.0;
}

double RotorConnection::elevationDeg() const
{
    if (m_config.axes != RotorAxes::AzimuthElevation) { return -1.0; }
    return m_elevationDeg;
}

} // namespace NereusSDR
