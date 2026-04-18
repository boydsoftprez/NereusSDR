// =================================================================
// src/core/mmio/TcpClientEndpointWorker.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "TcpClientEndpointWorker.h"
#include "MmioEndpoint.h"
#include "FormatParser.h"
#include "../LogCategories.h"

#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QUuid>
#include <QAbstractSocket>

namespace NereusSDR {

TcpClientEndpointWorker::TcpClientEndpointWorker(MmioEndpoint* endpoint, QObject* parent)
    : ITransportWorker(parent)
    , m_endpoint(endpoint)
{
}

TcpClientEndpointWorker::~TcpClientEndpointWorker()
{
    stop();
}

void TcpClientEndpointWorker::start()
{
    // Tear down any previously existing socket cleanly before creating a new one.
    if (m_socket) {
        m_socket->abort();
        delete m_socket;
        m_socket = nullptr;
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
        delete m_reconnectTimer;
        m_reconnectTimer = nullptr;
    }

    m_stopping = false;

    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::connected,
            this,     &TcpClientEndpointWorker::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this,     &TcpClientEndpointWorker::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this,     &TcpClientEndpointWorker::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this,     &TcpClientEndpointWorker::onSocketError);

    connect(m_reconnectTimer, &QTimer::timeout,
            this,             &TcpClientEndpointWorker::attemptReconnect);

    const QString host = m_endpoint->host();
    const quint16 port = m_endpoint->port();

    // Log a warning for hosts that don't make sense for outbound dials, but
    // still pass the value through to connectToHost — it will fail and we'll
    // retry via the reconnect timer. Mirrors Thetis's TcpClientHandler which
    // does not special-case the host string (MeterManager.cs:40200).
    if (host.isEmpty() || host == QStringLiteral("0.0.0.0")) {
        qCWarning(lcMmio) << "TCP client: host is" << host
                          << "— outbound connection will likely fail and retry";
    }

    setState(State::Connecting);
    qCInfo(lcMmio) << "TCP client connecting to" << host << ":" << port
                   << "for endpoint" << m_endpoint->name();

    m_socket->connectToHost(host, port);
}

void TcpClientEndpointWorker::stop()
{
    m_stopping = true;

    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
        delete m_reconnectTimer;
        m_reconnectTimer = nullptr;
    }

    if (m_socket) {
        m_socket->abort();
        delete m_socket;
        m_socket = nullptr;
    }

    setState(State::Disconnected);
}

void TcpClientEndpointWorker::onConnected()
{
    setState(State::Connected);
    m_lineBuffer.clear();
    qCInfo(lcMmio) << "TCP client connected to" << m_endpoint->host()
                   << ":" << m_endpoint->port()
                   << "for endpoint" << m_endpoint->name();
}

void TcpClientEndpointWorker::onDisconnected()
{
    qCInfo(lcMmio) << "TCP client disconnected from endpoint" << m_endpoint->name();

    if (m_stopping) {
        setState(State::Disconnected);
        return;
    }

    // Schedule a reconnect attempt. State stays Connecting to indicate we are
    // actively trying to re-establish the connection.
    setState(State::Connecting);
    m_reconnectTimer->start(kReconnectDelayMs);
}

void TcpClientEndpointWorker::onReadyRead()
{
    // Append all available data to the line buffer, then drain complete lines.
    // From Thetis MeterManager.cs:40320-40353 — messages are newline-delimited;
    // strip any trailing carriage return for cross-platform robustness.
    m_lineBuffer.append(m_socket->readAll());

    while (true) {
        const int nlIndex = m_lineBuffer.indexOf('\n');
        if (nlIndex < 0) {
            break;
        }

        QByteArray line = m_lineBuffer.left(nlIndex);
        m_lineBuffer.remove(0, nlIndex + 1);

        // Strip trailing \r (CRLF line endings from Windows servers).
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

        if (batch.isEmpty()) {
            qCWarning(lcMmio) << "TCP client: parse yielded empty batch for endpoint"
                              << m_endpoint->name()
                              << "(format:" << static_cast<int>(fmt) << ")";
            continue;
        }

        emit valueBatchReceived(batch);
    }
}

void TcpClientEndpointWorker::onSocketError()
{
    qCWarning(lcMmio) << "TCP client socket error for endpoint" << m_endpoint->name()
                      << ":" << m_socket->errorString();

    if (state() == State::Error || m_stopping) {
        return;
    }

    // Schedule reconnect. Transition to Connecting so the UI reflects that we
    // are retrying rather than in a permanent error state.
    setState(State::Connecting);
    m_reconnectTimer->start(kReconnectDelayMs);
}

void TcpClientEndpointWorker::attemptReconnect()
{
    if (m_stopping) {
        return;
    }

    const QString host = m_endpoint->host();
    const quint16 port = m_endpoint->port();

    qCInfo(lcMmio) << "TCP client reconnect attempt to" << host << ":" << port
                   << "for endpoint" << m_endpoint->name();

    m_socket->abort();
    m_socket->connectToHost(host, port);
}

} // namespace NereusSDR
