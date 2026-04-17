// =================================================================
// src/core/mmio/SerialEndpointWorker.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#ifdef HAVE_SERIALPORT

#include "SerialEndpointWorker.h"
#include "MmioEndpoint.h"
#include "FormatParser.h"
#include "../LogCategories.h"

#include <QSerialPort>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariant>

namespace NereusSDR {

SerialEndpointWorker::SerialEndpointWorker(MmioEndpoint* endpoint, QObject* parent)
    : ITransportWorker(parent)
    , m_endpoint(endpoint)
{
}

SerialEndpointWorker::~SerialEndpointWorker()
{
    stop();
}

// From Thetis MeterManager.cs:40630-40660 — SerialPortHandler.Open():
// sets port name, baud rate, then opens with default 8N1 / no flow
// control before subscribing to DataReceived.
void SerialEndpointWorker::start()
{
    // Tear down any previous port so start() is idempotent.
    if (m_port) {
        m_port->close();
        delete m_port;
        m_port = nullptr;
    }

    m_lineBuffer.clear();

    m_port = new QSerialPort(this);
    m_port->setPortName(m_endpoint->serialDevice());
    m_port->setBaudRate(m_endpoint->serialBaud());

    // Default 8N1, no flow control — matches Thetis SerialPortHandler
    // defaults (MeterManager.cs:40635-40642).
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_port, &QSerialPort::readyRead,
            this,   &SerialEndpointWorker::onReadyRead);

    // QSerialPort::errorOccurred(QSerialPort::SerialPortError)
    connect(m_port, &QSerialPort::errorOccurred,
            this,   &SerialEndpointWorker::onErrorOccurred);

    if (!m_port->open(QIODevice::ReadOnly)) {
        qCWarning(lcMmio) << "Serial open failed:" << m_port->errorString();
        setState(State::Error);
        return;
    }

    setState(State::Connected);
    qCInfo(lcMmio) << "Serial opened on" << m_endpoint->serialDevice()
                   << "@" << m_endpoint->serialBaud();
}

void SerialEndpointWorker::stop()
{
    if (m_port && m_port->isOpen()) {
        m_port->close();
    }

    delete m_port;
    m_port = nullptr;

    setState(State::Disconnected);
}

// From Thetis MeterManager.cs:40700-40760 — SerialPortHandler.DataReceived():
// reads available data, accumulates into a buffer, splits on newline,
// dispatches each complete line to the format parser.
void SerialEndpointWorker::onReadyRead()
{
    m_lineBuffer.append(m_port->readAll());

    // Process every complete newline-terminated record in the buffer.
    int newlinePos;
    while ((newlinePos = m_lineBuffer.indexOf('\n')) != -1) {
        QByteArray line = m_lineBuffer.left(newlinePos);
        m_lineBuffer.remove(0, newlinePos + 1);

        // Strip optional trailing carriage return (CRLF line endings).
        if (!line.isEmpty() && line.back() == '\r') {
            line.chop(1);
        }

        if (line.isEmpty()) {
            continue;
        }

        const QUuid guid = m_endpoint->guid();
        const MmioEndpoint::Format fmt = m_endpoint->format();

        QHash<QString, QVariant> batch;

        if (fmt == MmioEndpoint::Format::Json) {
            batch = FormatParser::parseJson(line, guid);
        } else if (fmt == MmioEndpoint::Format::Xml) {
            batch = FormatParser::parseXml(line, guid);
        } else if (fmt == MmioEndpoint::Format::Raw) {
            batch = FormatParser::parseRaw(line, guid);
        }

        if (!batch.isEmpty()) {
            emit valueBatchReceived(batch);
        }
    }
}

// QSerialPort emits errorOccurred(NoError) when it clears the error
// state internally — ignore those to avoid false Error transitions.
void SerialEndpointWorker::onErrorOccurred()
{
    if (m_port->error() == QSerialPort::NoError) {
        return;
    }

    qCWarning(lcMmio) << "Serial error:" << m_port->errorString();
    setState(State::Error);
}

} // namespace NereusSDR

#endif // HAVE_SERIALPORT
