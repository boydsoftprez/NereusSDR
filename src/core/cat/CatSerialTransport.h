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
#pragma once
#include "CatConfiguration.h"
#include <QObject>
#include <QTimer>
#include <memory>
namespace NereusSDR {
// Narrow device boundary: production uses QSerialPort; tests inject byte/pin/error events.
class CatSerialDevice : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool open(const CatEndpointConfig&, QString& error) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 pendingBytes() const { return 0; }
    virtual qint64 write(const QByteArray&) = 0;
    virtual QByteArray read() = 0;
    virtual bool pins(bool& cts, bool& dsr, QString& error) = 0;
signals:
    void readyRead();
    void bytesWritten(qint64);
    void errorOccurred(QString);
};
class CatSerialTransport : public QObject {
    Q_OBJECT
public:
    explicit CatSerialTransport(std::shared_ptr<CatSerialDevice> device = {}, QObject* parent = nullptr);
    ~CatSerialTransport() override;
    bool start(const CatEndpointConfig&);
    void stop();
    void writeBytes(const QByteArray&);
    bool setPinSampling(bool enabled);
    bool isOpen() const { return m_active && m_device && m_device->isOpen(); }
    QString errorString() const { return m_error; }
    qint64 queuedBytes() const { return m_pending.size() + (m_device ? m_device->pendingBytes() : 0); }
signals:
    void bytesReceived(QByteArray);
    void failed(QString);
    void pttSampled(bool cts, bool dsr);
private:
    void drain();
    void samplePins();
    void fail(const QString&);
    std::shared_ptr<CatSerialDevice> m_device;
    QTimer m_pinTimer;
    QByteArray m_pending;
    QString m_error;
    quint64 m_generation{0};
    bool m_active{false};
    bool m_draining{false};
};
} // namespace NereusSDR
