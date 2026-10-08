// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/StationRotorController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native. See StationRotorController.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code. Rotor control plan, Task 3c.
//   2026-10-08: Bench fix: the serial port list offers USB serial
//               adapters first and leaves out console ports. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/StationRotorController.h"

#include "core/AppSettings.h"
#include "core/RotctldProcess.h"
#include "core/RotorHeading.h"
#include "core/RotorModels.h"
#include "core/RotorRoute.h"
#include "core/SpotSourceHost.h"

#include <QCollator>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSignalBlocker>

#ifdef HAVE_SERIALPORT
#include <QSerialPortInfo>
#endif

#include <algorithm>
#include <cmath>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcRotorController, "nereus.rotor.controller")

namespace {

// Core-owned settings (contract, "Core-owned settings").
const QString kKeyDriver      = QStringLiteral("Rotor/Driver");
const QString kKeySerialPort  = QStringLiteral("Rotor/SerialPort");
const QString kKeyBaud        = QStringLiteral("Rotor/Baud");
const QString kKeyHost        = QStringLiteral("Rotor/Host");
const QString kKeyPort        = QStringLiteral("Rotor/Port");
const QString kKeyHamlibModel = QStringLiteral("Rotor/HamlibModel");
const QString kKeyAxes        = QStringLiteral("Rotor/Axes");
const QString kKeyEndStop     = QStringLiteral("Rotor/EndStop");
const QString kKeyRangeDeg    = QStringLiteral("Rotor/RangeDeg");
const QString kKeyOffsetDeg   = QStringLiteral("Rotor/OffsetDeg");
const QString kKeyPresets     = QStringLiteral("Rotor/Presets");

// The elevation a target may ask for (contract, "Refusals").
constexpr double kMaxElevationDeg = 90.0;

int intSetting(const QString& key, int fallback)
{
    bool ok = false;
    const int v = AppSettings::instance().value(key).toString().toInt(&ok);
    return ok ? v : fallback;
}

bool isSerialDriver(RotorDriver driver)
{
    return driver == RotorDriver::Gs232a || driver == RotorDriver::Gs232b
        || driver == RotorDriver::RotctldStarted;
}

void setReason(QString* reason, const QString& text)
{
    if (reason) { *reason = text; }
}

QStringList systemSerialPorts()
{
    QList<StationRotorController::SerialPortCandidate> ports;
#ifdef HAVE_SERIALPORT
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : infos) {
        StationRotorController::SerialPortCandidate port;
#ifdef Q_OS_WIN
        // "COM4": what QSerialPort and rotctld's -r take on Windows.
        port.name = info.portName();
#else
        // "/dev/ttyUSB0": rotctld's -r needs the full path.
        port.name = info.systemLocation();
#endif
        port.usbVendor = info.hasVendorIdentifier();
        ports.append(port);
    }
#endif
    QStringList consoles;
#ifdef Q_OS_LINUX
    // The kernel's own console (console= on its command line) is never a
    // rotor; one small read of a pseudo-file.
    QFile cmdline(QStringLiteral("/proc/cmdline"));
    if (cmdline.open(QIODevice::ReadOnly)) {
        consoles = StationRotorController::consoleDevicesFromCmdline(
            QString::fromLocal8Bit(cmdline.readAll()));
    }
#endif
    return StationRotorController::orderSerialPorts(ports, consoles);
}

QString baseName(const QString& port)
{
    const qsizetype slash = port.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? port : port.mid(slash + 1);
}

} // namespace

// ── Refusals (contract, "Refusals"; kept word for word) ─────────────

QString StationRotorController::noRotorReason()
{
    return QStringLiteral("No rotor is set up on this Core.");
}

QString StationRotorController::notConnectedReason()
{
    return QStringLiteral("The rotor is not connected.");
}

QString StationRotorController::azimuthOnlyReason()
{
    return QStringLiteral("This rotor turns in azimuth only.");
}

QString StationRotorController::notANumberReason()
{
    return QStringLiteral("That heading is not a number.");
}

QString StationRotorController::outOfRangeReason()
{
    return QStringLiteral("That heading is outside the rotor's range.");
}

QString StationRotorController::noGridReason()
{
    return QStringLiteral("Set your grid square in Setup to turn the beam to spots.");
}

QString StationRotorController::callNotPlacedReason()
{
    return QStringLiteral("That callsign could not be placed.");
}

QString StationRotorController::rotctldMissingReason()
{
    return RotctldProcess::notInstalledReason();
}

QString StationRotorController::unknownSerialPortReason()
{
    return QStringLiteral("That serial port is not on the Core's computer.");
}

// ── Life ─────────────────────────────────────────────────────────────

StationRotorController::StationRotorController(QObject* parent)
    : QObject(parent)
    , m_connection(new RotorConnection(this))
    , m_portLister(&systemSerialPorts)
{
    m_holdTimer.setSingleShot(true);
    // A coarse timer may fire 5% early; the lapse is 750 ms, not 712.
    m_holdTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_holdTimer, &QTimer::timeout, this, &StationRotorController::onHoldLapsed);

    connect(m_connection, &RotorConnection::connected, this, [this]() {
        m_fault.clear();
        setPhase(RotorConnectionPhase::Connected);
    });
    connect(m_connection, &RotorConnection::disconnected, this, [this]() {
        endHold();
        setPhase(m_wantConnected ? RotorConnectionPhase::Retrying
                                 : RotorConnectionPhase::Disconnected,
                 m_connectionError);
        emit positionChanged();
    });
    connect(m_connection, &RotorConnection::connectionFailed, this,
            [this](const QString& reason) {
        m_fault = reason;
        setPhase(RotorConnectionPhase::Error, reason);
    });
    connect(m_connection, &RotorConnection::reconnectScheduled, this, [this](int, int) {
        setPhase(RotorConnectionPhase::Retrying, m_connectionError);
    });
    connect(m_connection, &RotorConnection::positionUpdated, this, [this]() {
        emit positionChanged();
        emit stateChanged();
    });
    connect(m_connection, &RotorConnection::positionFreshChanged, this, [this](bool) {
        emit positionChanged();
        emit stateChanged();
    });
    connect(m_connection, &RotorConnection::turningChanged, this, [this](bool) {
        emit stateChanged();
    });
    connect(m_connection, &RotorConnection::rotorError, this,
            [this](int, const QString& reason) {
        m_fault = reason;
        emit stateChanged();
    });
}

StationRotorController::~StationRotorController()
{
    // No signals to an owner that is itself being torn down.
    const QSignalBlocker block(this);
    m_holdTimer.stop();
}

void StationRotorController::start()
{
    loadSettings();
    connectNow();
}

void StationRotorController::setCallsignLocator(CallsignLocator locator)
{
    m_locator = std::move(locator);
}

void StationRotorController::setSerialPortListerForTesting(SerialPortLister lister)
{
    m_portLister = std::move(lister);
}

// ── Settings ─────────────────────────────────────────────────────────

void StationRotorController::loadSettings()
{
    RotorConfig c;   // the contract's defaults
    auto& s = AppSettings::instance();

    const int driver = intSetting(kKeyDriver, static_cast<int>(RotorDriver::None));
    c.driver = (driver >= static_cast<int>(RotorDriver::None)
                && driver <= static_cast<int>(RotorDriver::RotctldStarted))
                   ? static_cast<RotorDriver>(driver)
                   : RotorDriver::None;
    c.serialPort = s.value(kKeySerialPort).toString();
    const int baud = intSetting(kKeyBaud, c.baud);
    c.baud = baud > 0 ? baud : c.baud;
    c.host = s.value(kKeyHost).toString();
    const int port = intSetting(kKeyPort, c.port);
    c.port = (port > 0 && port <= 65535) ? static_cast<quint16>(port) : c.port;
    c.hamlibModel = intSetting(kKeyHamlibModel, c.hamlibModel);
    const int axes = intSetting(kKeyAxes, static_cast<int>(c.axes));
    c.axes = axes == static_cast<int>(RotorAxes::AzimuthElevation)
                 ? RotorAxes::AzimuthElevation
                 : RotorAxes::Azimuth;
    const int endStop = intSetting(kKeyEndStop, static_cast<int>(c.endStop));
    c.endStop = (endStop >= static_cast<int>(RotorRoute::EndStop::None)
                 && endStop <= static_cast<int>(RotorRoute::EndStop::South))
                    ? static_cast<RotorRoute::EndStop>(endStop)
                    : c.endStop;
    c.rangeDeg = RotorRoute::clampRangeDeg(
        intSetting(kKeyRangeDeg, static_cast<int>(c.rangeDeg)));
    bool ok = false;
    const double offset = s.value(kKeyOffsetDeg).toString().toDouble(&ok);
    c.offsetDeg = (ok && std::isfinite(offset)) ? offset : 0.0;

    m_config = c;
    m_presets = s.value(kKeyPresets).toString();
}

void StationRotorController::saveSettings() const
{
    auto& s = AppSettings::instance();
    s.setValue(kKeyDriver, QString::number(static_cast<int>(m_config.driver)));
    s.setValue(kKeySerialPort, m_config.serialPort);
    s.setValue(kKeyBaud, QString::number(m_config.baud));
    s.setValue(kKeyHost, m_config.host);
    s.setValue(kKeyPort, QString::number(m_config.port));
    s.setValue(kKeyHamlibModel, QString::number(m_config.hamlibModel));
    s.setValue(kKeyAxes, QString::number(static_cast<int>(m_config.axes)));
    s.setValue(kKeyEndStop, QString::number(static_cast<int>(m_config.endStop)));
    s.setValue(kKeyRangeDeg, QString::number(static_cast<int>(m_config.rangeDeg)));
    s.setValue(kKeyOffsetDeg, QString::number(m_config.offsetDeg, 'g', 10));
}

// ── Connection ───────────────────────────────────────────────────────

void StationRotorController::connectNow()
{
    endHold();
    m_wantConnected = false;
    m_connection->disconnectFromRotor();
    m_connection->configure(m_config);
    m_config = m_connection->config();   // the range as the connection holds it
    m_connectionError.clear();

    if (m_config.driver == RotorDriver::None) {
        setPhase(RotorConnectionPhase::Disabled);
        return;
    }
    m_wantConnected = true;
    setPhase(RotorConnectionPhase::Connecting);
    if (!m_connection->connectToRotor()) {
        // Driver 4 with no rotctld: no retry (connectionFailed has the
        // reason already).
        m_wantConnected = false;
        setPhase(RotorConnectionPhase::Error, m_connection->lastError());
    }
}

void StationRotorController::setPhase(RotorConnectionPhase phase, const QString& error)
{
    m_phase = phase;
    m_connectionError = phase == RotorConnectionPhase::Connected ? QString() : error;
    emit stateChanged();
}

// ── Properties ───────────────────────────────────────────────────────

QString StationRotorController::label() const
{
    const QString port = m_config.serialPort;
    switch (m_config.driver) {
    case RotorDriver::None:
        return {};
    case RotorDriver::Gs232a:
        return QStringLiteral("Yaesu GS-232A on %1").arg(port);
    case RotorDriver::Gs232b:
        return QStringLiteral("Yaesu GS-232B on %1").arg(port);
    case RotorDriver::Rotctld:
        return QStringLiteral("Hamlib's rotctld at %1 port %2")
            .arg(m_config.host)
            .arg(m_config.port);
    case RotorDriver::RotctldStarted: {
        const auto models = commonRotorModels();
        for (const RotorModel& m : models) {
            if (m.hamlibId == m_config.hamlibModel) {
                return QStringLiteral("%1 on %2").arg(m.name, port);
            }
        }
        return QStringLiteral("Hamlib rotor model %1 on %2")
            .arg(m_config.hamlibModel)
            .arg(port);
    }
    }
    return {};
}

QString StationRotorController::serialPort() const
{
    return isSerialDriver(m_config.driver) ? m_config.serialPort : QString();
}

QString StationRotorController::host() const
{
    return m_config.driver == RotorDriver::Rotctld ? m_config.host : QString();
}

QStringList StationRotorController::serialPorts() const
{
    // The lister's order is kept: the system's is orderSerialPorts().
    QStringList ports = m_portLister ? m_portLister() : QStringList{};
    ports.removeDuplicates();
    return ports;
}

QStringList StationRotorController::orderSerialPorts(const QList<SerialPortCandidate>& ports,
                                                     const QStringList& consoleDevices)
{
    static const QStringList kUsbPrefixes = {
        QStringLiteral("ttyUSB"),      QStringLiteral("ttyACM"),
        QStringLiteral("cu.usbserial"), QStringLiteral("cu.usbmodem"),
        QStringLiteral("tty.usbserial"), QStringLiteral("tty.usbmodem")};
    QStringList usb;
    QStringList other;
    for (const SerialPortCandidate& port : ports) {
        const QString base = baseName(port.name);
        if (base.isEmpty() || base.startsWith(QLatin1String("ttyFIQ"))
            || consoleDevices.contains(base)) {
            continue;
        }
        const bool isUsb = port.usbVendor
            || std::any_of(kUsbPrefixes.cbegin(), kUsbPrefixes.cend(),
                           [&base](const QString& prefix) { return base.startsWith(prefix); });
        QStringList& group = isUsb ? usb : other;
        if (!group.contains(port.name)) { group.append(port.name); }
    }
    QCollator collator;
    collator.setNumericMode(true);
    const auto byName = [&collator](const QString& a, const QString& b) {
        return collator.compare(a, b) < 0;
    };
    std::sort(usb.begin(), usb.end(), byName);
    std::sort(other.begin(), other.end(), byName);
    return usb + other;
}

QStringList StationRotorController::consoleDevicesFromCmdline(const QString& cmdline)
{
    QStringList devices;
    const QStringList words = cmdline.split(QRegularExpression(QStringLiteral("\\s+")),
                                            Qt::SkipEmptyParts);
    for (const QString& word : words) {
        if (!word.startsWith(QLatin1String("console="))) { continue; }
        const QString device = word.mid(8).section(QLatin1Char(','), 0, 0);
        if (!device.isEmpty() && !devices.contains(device)) { devices.append(device); }
    }
    return devices;
}

bool StationRotorController::rotctldAvailable() const
{
    return !RotctldProcess::findBinary().isEmpty();
}

bool StationRotorController::positionFresh() const
{
    return m_connection->positionFresh();
}

double StationRotorController::azimuthDeg() const
{
    return m_connection->azimuthDeg();
}

double StationRotorController::elevationDeg() const
{
    return m_connection->elevationDeg();
}

double StationRotorController::spanPositionDeg() const
{
    if (!m_connection->isConnected()) { return RotorRoute::kUnknownSpanDeg; }
    return m_connection->spanPositionDeg();
}

double StationRotorController::targetAzimuthDeg() const
{
    return m_connection->targetAzimuthDeg();
}

double StationRotorController::targetElevationDeg() const
{
    return m_connection->targetElevationDeg();
}

double StationRotorController::travelDeg() const
{
    const double target = targetAzimuthDeg();
    if (target < 0.0) { return 0.0; }
    const RotorRoute::Move move = m_connection->tracker().planTo(target);
    return move.routeKnown ? move.travelDeg : 0.0;
}

bool StationRotorController::routeKnown() const
{
    const double target = targetAzimuthDeg();
    if (target < 0.0) { return true; }
    return m_connection->tracker().planTo(target).routeKnown;
}

RotorMotion StationRotorController::motion() const
{
    if (!m_connection->isConnected()) { return RotorMotion::Stopped; }
    if (m_holdActive) { return RotorMotion::Nudging; }
    // The connection clears the target on arrival (within
    // RotorConnection::kArrivedDeg) or when the heading stops changing
    // (RotorConnection::kSettleMs), and on stop.
    if (targetAzimuthDeg() >= 0.0) { return RotorMotion::Turning; }
    return RotorMotion::Stopped;
}

// ── Commands ─────────────────────────────────────────────────────────

bool StationRotorController::turnable(QString* reason) const
{
    if (m_config.driver == RotorDriver::None) {
        setReason(reason, noRotorReason());
        return false;
    }
    if (!m_connection->isConnected()) {
        setReason(reason, notConnectedReason());
        return false;
    }
    return true;
}

bool StationRotorController::acceptTarget(double azimuthDeg, double elevationDeg,
                                          QString* reason)
{
    if (!std::isfinite(azimuthDeg) || !std::isfinite(elevationDeg)) {
        setReason(reason, notANumberReason());
        return false;
    }
    double az = 0.0;
    if (!RotorHeading::accept(azimuthDeg, &az)) {
        setReason(reason, outOfRangeReason());
        return false;
    }
    if (elevationDeg != -1.0) {
        if (m_config.axes != RotorAxes::AzimuthElevation) {
            setReason(reason, azimuthOnlyReason());
            return false;
        }
        if (elevationDeg < 0.0 || elevationDeg > kMaxElevationDeg) {
            setReason(reason, outOfRangeReason());
            return false;
        }
    }
    endHold();
    if (!m_connection->setTarget(az, elevationDeg)) {
        setReason(reason, notConnectedReason());
        emit stateChanged();
        return false;
    }
    emit stateChanged();
    return true;
}

bool StationRotorController::setRotorTarget(double azimuthDeg, double elevationDeg,
                                            QString* reason)
{
    if (!turnable(reason)) { return false; }
    return acceptTarget(azimuthDeg, elevationDeg, reason);
}

bool StationRotorController::turnRotorToCall(const QString& call, bool longPath,
                                             QString* reason)
{
    if (!turnable(reason)) { return false; }
    // The station's grid square, from the same keys the spot reporters
    // read (FreeDvReporter/GridSquare, else User/GridSquare).
    const QString grid = SpotSourceHost::freedvGridSquare();
    if (grid.isEmpty()) {
        setReason(reason, noGridReason());
        return false;
    }
    const QString callsign = call.trimmed().toUpper();
    const std::optional<GeoPosition> where =
        (m_locator && !callsign.isEmpty()) ? m_locator(callsign) : std::nullopt;
    if (!where) {
        setReason(reason, callNotPlacedReason());
        return false;
    }
    const std::optional<double> bearing = GreatCircle::bearingFromGrid(grid, *where);
    if (!bearing) {
        // A grid square that is set but cannot be read places nothing.
        setReason(reason, noGridReason());
        return false;
    }
    const double heading = longPath ? GreatCircle::longPathBearingDeg(*bearing) : *bearing;
    return acceptTarget(heading, -1.0, reason);
}

bool StationRotorController::stopRotor(QString* reason)
{
    if (!turnable(reason)) { return false; }
    endHold();
    m_connection->stop();
    emit stateChanged();
    return true;
}

bool StationRotorController::nudgeRotor(RotorDirection direction, bool active,
                                        quint64 sessionId, QString* reason)
{
    if (!turnable(reason)) { return false; }
    const bool elevation = direction == RotorDirection::Up
                        || direction == RotorDirection::Down;
    if (elevation && m_config.axes != RotorAxes::AzimuthElevation) {
        setReason(reason, azimuthOnlyReason());
        return false;
    }

    if (!active) {
        // The window let go: its hold stops now.
        if (m_holdActive && m_holdSession == sessionId) {
            endHold();
            m_connection->stop();
            emit stateChanged();
        }
        return true;
    }

    if (!m_holdActive || m_holdDirection != direction) {
        if (!m_connection->startMove(direction)) {
            setReason(reason, notConnectedReason());
            return false;
        }
        m_holdActive = true;
        m_holdDirection = direction;
    }
    // The latest window to press owns the hold.
    m_holdSession = sessionId;
    m_holdTimer.start(kHoldLapseMs);
    emit stateChanged();
    return true;
}

void StationRotorController::sessionEnded(quint64 sessionId)
{
    if (!m_holdActive || m_holdSession != sessionId) { return; }
    qCInfo(lcRotorController) << "the window holding a turn went away; stopping";
    endHold();
    m_connection->stop();
    emit stateChanged();
}

void StationRotorController::endHold()
{
    m_holdTimer.stop();
    m_holdActive = false;
    m_holdSession = 0;
}

void StationRotorController::onHoldLapsed()
{
    if (!m_holdActive) { return; }
    qCInfo(lcRotorController) << "no turn repeat for" << kHoldLapseMs << "ms; stopping";
    endHold();
    m_connection->stop();
    emit stateChanged();
}

bool StationRotorController::configureRotor(const RotorConfig& config, QString* reason)
{
    if (config.driver == RotorDriver::None) {
        // Disconnect and forget the setup; the presets stay.
        auto& s = AppSettings::instance();
        for (const QString& key : {kKeyDriver, kKeySerialPort, kKeyBaud, kKeyHost, kKeyPort,
                                   kKeyHamlibModel, kKeyAxes, kKeyEndStop, kKeyRangeDeg,
                                   kKeyOffsetDeg}) {
            s.remove(key);
        }
        s.save();
        m_config = RotorConfig{};
        m_fault.clear();
        connectNow();
        return true;
    }

    if (!std::isfinite(config.offsetDeg)) {
        setReason(reason, notANumberReason());
        return false;
    }
    if (config.driver == RotorDriver::RotctldStarted && !rotctldAvailable()) {
        setReason(reason, rotctldMissingReason());
        return false;
    }
    if (isSerialDriver(config.driver) && !serialPorts().contains(config.serialPort)) {
        setReason(reason, unknownSerialPortReason());
        return false;
    }

    m_config = config;
    m_config.rangeDeg = RotorRoute::clampRangeDeg(config.rangeDeg);
    m_fault.clear();
    saveSettings();
    AppSettings::instance().save();
    qCInfo(lcRotorController) << "rotor set up, driver" << static_cast<int>(m_config.driver);
    connectNow();
    return true;
}

bool StationRotorController::disconnectRotor(QString* /*reason*/)
{
    endHold();
    m_wantConnected = false;
    m_connection->disconnectFromRotor();
    setPhase(m_config.driver == RotorDriver::None ? RotorConnectionPhase::Disabled
                                                  : RotorConnectionPhase::Disconnected);
    return true;
}

bool StationRotorController::setRotorPresets(const QString& presets, QString* reason)
{
    QStringList out;
    const QStringList lines = presets.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line.remove(QLatin1Char('\r'));
        if (line.trimmed().isEmpty()) { continue; }
        const int tab = line.lastIndexOf(QLatin1Char('\t'));
        const QString name = tab >= 0 ? line.left(tab).trimmed() : QString();
        const QString degText = tab >= 0 ? line.mid(tab + 1) : QString();
        double deg = 0.0;
        if (!RotorHeading::parse(degText, &deg)) {
            bool ok = false;
            const double v = degText.trimmed().toDouble(&ok);
            setReason(reason, (ok && std::isfinite(v)) ? outOfRangeReason()
                                                       : notANumberReason());
            return false;
        }
        out.append(name + QLatin1Char('\t') + QString::number(deg, 'g', 10));
    }
    m_presets = out.join(QLatin1Char('\n'));
    AppSettings::instance().setValue(kKeyPresets, m_presets);
    AppSettings::instance().save();
    emit stateChanged();
    return true;
}

} // namespace NereusSDR
