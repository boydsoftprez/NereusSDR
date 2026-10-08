//=================================================================
// SDRSerialPort.cs
//=================================================================
// Copyright (C) 2005  Bill Tracey
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//================================================================= 
// Serial port support for PowerSDR support of CAT and serial port control  
//=================================================================

// Ported from Thetis Project Files/Source/Console/CAT/SDRSerialPortII.cs
// Modification history (NereusSDR):
// 2026-10-04 - Qt serial transport and sampled input pins by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.
// 2026-10-04 - Native Linux HUP observation by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex; NereusSDR-original mechanics.
#include "CatSerialTransport.h"
#include "CatSettings.h"
#include "core/LogCategories.h"
#include <QPointer>
#include <QScopeGuard>
#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#if defined(Q_OS_LINUX)
#include <poll.h>
#include <cerrno>
#include <cstring>
#endif
#endif
namespace NereusSDR {
namespace {
// Nereus event-loop scheduling and output bounds, not DSP parameters.
constexpr int kPinSamplingIntervalMs = 20;
#if defined(Q_OS_LINUX) && defined(HAVE_SERIALPORT)
constexpr int kSerialDisconnectObservationMs = 20;
#endif
constexpr qsizetype kMaximumPendingBytes = 256 * 1024;
constexpr qsizetype kMaximumOutputBytes = 64 * 1024;
#ifdef HAVE_SERIALPORT
class NativeSerialDevice final : public CatSerialDevice {
public:
    NativeSerialDevice() : m_port(std::make_unique<QSerialPort>().release(), [](QSerialPort* port) { port->deleteLater(); }) {
#if defined(Q_OS_LINUX)
        // Our Linux/Qt 6.8 PTY regression gets HUP without a Qt device error.
        // Observe actual HUP/ERR without consuming input
        // or mistaking an ordinary empty read for a disappearance.
        m_disconnectTimer.setInterval(kSerialDisconnectObservationMs);
        connect(&m_disconnectTimer, &QTimer::timeout, this, [this] {
            const auto port = m_port;
            if (m_opening) { return; }
            QString error;
            pollfd descriptor{static_cast<int>(port->handle()), 0, 0};
            if (!port->isOpen() || descriptor.fd < 0) {
                error = "Serial device no longer open";
            } else {
                const int result = ::poll(&descriptor, 1, 0);
                if (result < 0 && errno != EINTR) {
                    error = "Serial descriptor poll: " + QString::fromLocal8Bit(std::strerror(errno));
                } else if (result > 0 && (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL))) {
                    error = "Serial device disconnected";
                }
            }
            if (!error.isEmpty()) {
                m_disconnectTimer.stop();
                emit errorOccurred(error);
                // The callback may close, restart or delete this device.
            }
        });
#endif
        connect(m_port.get(), &QSerialPort::readyRead, this, &CatSerialDevice::readyRead);
        connect(m_port.get(), &QSerialPort::bytesWritten, this, &CatSerialDevice::bytesWritten);
        connect(m_port.get(), &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError code) {
            if (code != QSerialPort::NoError && !m_opening && m_port->isOpen()) {
                emit errorOccurred(m_port->errorString());
            }
        });
    }
    bool open(const CatEndpointConfig& config, QString& error) override {
        m_opening = true;
        const auto opening = qScopeGuard([this] { m_opening = false; });
        const auto port = m_port;
        QSerialPort::Parity parity;
        if (config.serialParity == "None") { parity = QSerialPort::NoParity; }
        else if (config.serialParity == "Odd") { parity = QSerialPort::OddParity; }
        else if (config.serialParity == "Even") { parity = QSerialPort::EvenParity; }
        else if (config.serialParity == "Space") { parity = QSerialPort::SpaceParity; }
        else if (config.serialParity == "Mark") { parity = QSerialPort::MarkParity; }
        else { error = "Unsupported serial parity"; return false; }
        QSerialPort::DataBits bits;
        switch (config.serialDataBits) {
        case 5: bits = QSerialPort::Data5; break;
        case 6: bits = QSerialPort::Data6; break;
        case 7: bits = QSerialPort::Data7; break;
        case 8: bits = QSerialPort::Data8; break;
        default: error = "Unsupported serial data bits"; return false;
        }
        QSerialPort::StopBits stops;
        if (config.serialStopBits == "1") { stops = QSerialPort::OneStop; }
        else if (config.serialStopBits == "1.5") { stops = QSerialPort::OneAndHalfStop; }
        else if (config.serialStopBits == "2") { stops = QSerialPort::TwoStop; }
        else { error = "Unsupported serial stop bits"; return false; }
        port->setPortName(config.serialDevice);
        // From Thetis CAT/SDRSerialPortII.cs:91 [v2.10.3.15].
        //[2.10.3.9]MW0LGE
        // [original inline comment from SDRSerialPortII.cs:91]
        if (!port->setBaudRate(config.serialBaud) || !port->setParity(parity)
            || !port->setDataBits(bits) || !port->setStopBits(stops)
            || !port->setFlowControl(QSerialPort::NoFlowControl) || !port->open(QIODevice::ReadWrite)) {
            error = port->errorString(); port->close(); return false;
        }
        // Verify accepted settings through QSerialPort's actual getters.
        if (port->baudRate() != config.serialBaud || port->parity() != parity
            || port->dataBits() != bits || port->stopBits() != stops
            || port->flowControl() != QSerialPort::NoFlowControl) {
            error = "Serial driver did not accept the configured format"; port->close(); return false;
        }
        port->setReadBufferSize(64 * 1024);
#if defined(Q_OS_LINUX)
        m_disconnectTimer.start();
#endif
        return true;
    }
    void close() override {
#if defined(Q_OS_LINUX)
        m_disconnectTimer.stop();
#endif
        // Native handle has no QObject parent and stays alive across synchronous Qt callbacks.
        const auto port = m_port;
        port->close();
    }
    bool isOpen() const override { return m_port->isOpen(); }
    qint64 pendingBytes() const override { return m_port->bytesToWrite(); }
    qint64 write(const QByteArray& bytes) override { const auto port = m_port; return port->write(bytes); }
    QByteArray read() override { const auto port = m_port; return port->read(4096); }
    bool pins(bool& cts, bool& dsr, QString& error) override {
        const auto port = m_port;
        port->clearError();
        const QSerialPort::PinoutSignals pinFlags = port->pinoutSignals();
        if (port->error() != QSerialPort::NoError) { error = port->errorString(); return false; }
        // From Thetis CAT/SDRSerialPortII.cs:234-241,265-268 [v2.10.3.15].
            //refactored for MAX performance [2.10.3.9]MW0LGE
            // added else's to match switch's from old code
        // [original inline comment from SDRSerialPortII.cs:236-237]
        // Legacy PTTOnRTS samples CTS; PTTOnDTR samples DSR, never output lines.
        cts = pinFlags.testFlag(QSerialPort::ClearToSendSignal);
        dsr = pinFlags.testFlag(QSerialPort::DataSetReadySignal);
        return true;
    }
private:
    std::shared_ptr<QSerialPort> m_port;
    bool m_opening{false};
#if defined(Q_OS_LINUX)
    QTimer m_disconnectTimer;
#endif
};
#endif
}
CatSerialTransport::CatSerialTransport(std::shared_ptr<CatSerialDevice> device, QObject* parent)
    : QObject(parent), m_device(std::move(device)) {
#ifdef HAVE_SERIALPORT
    if (!m_device) { m_device = std::make_shared<NativeSerialDevice>(); }
#endif
    m_pinTimer.setInterval(kPinSamplingIntervalMs);
    connect(&m_pinTimer, &QTimer::timeout, this, &CatSerialTransport::samplePins);
    if (m_device) {
        connect(m_device.get(), &CatSerialDevice::readyRead, this, [this] {
            const auto device = m_device;
            const QPointer<CatSerialTransport> self(this);
            const quint64 generation = m_generation;
            while (self && m_active && generation == m_generation) {
                const QByteArray bytes = device->read();
                if (!self || generation != m_generation || !m_active || bytes.isEmpty()) { return; }
                emit bytesReceived(bytes);
            }
        });
        connect(m_device.get(), &CatSerialDevice::bytesWritten, this, [this](qint64) { drain(); });
        connect(m_device.get(), &CatSerialDevice::errorOccurred, this, [this](const QString& error) { if (m_active) { fail(error); } });
    }
}
CatSerialTransport::~CatSerialTransport() { stop(); }
bool CatSerialTransport::start(const CatEndpointConfig& config) {
    const QPointer<CatSerialTransport> lifetime(this);
    const quint64 expected = m_generation + 1;
    stop();
    if (!lifetime || m_generation != expected) { return false; }
    QString error;
    if (!CatSettings::validate(config, &error)) { fail(error); return false; }
    if (!m_device) { fail("Qt SerialPort dependency unavailable"); return false; }
    const auto device = m_device;
    const QPointer<CatSerialTransport> self(this);
    const quint64 generation = m_generation;
    m_error.clear();
    const bool opened = device->open(config, error);
    if (!self || generation != m_generation) { return false; }
    if (!opened || !device->isOpen()) { fail(error.isEmpty() ? QString("Serial device did not open") : error); return false; }
    m_active = true;
    return true;
}
void CatSerialTransport::stop() {
    ++m_generation; m_active = false; m_pending.clear(); m_pinTimer.stop();
    const auto device = m_device;
    if (device && device->isOpen()) { device->close(); }
}
void CatSerialTransport::fail(const QString& error) {
    // Detach runtime before the service cancels claims and any failure callback closes/restarts.
    ++m_generation; m_active = false; m_pinTimer.stop(); m_pending.clear(); m_error = error;
    qCWarning(lcCat) << "Serial CAT:" << error;
    const QPointer<CatSerialTransport> self(this);
    const quint64 generation = m_generation;
    const auto device = m_device;
    emit failed(error);
    if (self && generation == m_generation && device && device->isOpen()) { device->close(); }
}
void CatSerialTransport::writeBytes(const QByteArray& bytes) {
    if (!isOpen() || bytes.isEmpty()) { return; }
    if (bytes.size() > kMaximumOutputBytes || queuedBytes() + bytes.size() > kMaximumPendingBytes) {
        fail("Serial output queue limit exceeded"); return;
    }
    m_pending += bytes; drain();
}
void CatSerialTransport::drain() {
    if (!isOpen() || m_pending.isEmpty() || m_draining) { return; }
    m_draining = true;
    const QPointer<CatSerialTransport> self(this);
    const auto draining = qScopeGuard([self] { if (self) { self->m_draining = false; } });
    const auto device = m_device;
    const quint64 generation = m_generation;
    const qint64 written = device->write(m_pending);
    if (!self || generation != m_generation || !m_active) { return; }
    if (written < 0 || written > m_pending.size()) { fail("Serial write failed"); return; }
    m_pending.remove(0, written);
    // One write per readiness event; a partial/zero write retains the ordered tail.
}
bool CatSerialTransport::setPinSampling(bool enabled) {
    m_pinTimer.stop();
    if (!enabled) { return true; }
    if (!isOpen()) { fail("PTT input pins require an open serial endpoint"); return false; }
    const QPointer<CatSerialTransport> self(this);
    const quint64 generation = m_generation;
    samplePins();
    if (!self || generation != m_generation || !isOpen()) { return false; }
    m_pinTimer.start(); return true;
}
void CatSerialTransport::samplePins() {
    if (!isOpen()) { return; }
    bool cts = false; bool dsr = false; QString error;
    const auto device = m_device;
    const QPointer<CatSerialTransport> self(this);
    const quint64 generation = m_generation;
    const bool available = device->pins(cts, dsr, error);
    if (!self || generation != m_generation || !m_active) { return; }
    if (!available) { fail(error.isEmpty() ? QString("Serial input pins unavailable") : error); return; }
    emit pttSampled(cts, dsr);
}
} // namespace NereusSDR
