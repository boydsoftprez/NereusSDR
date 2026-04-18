// =================================================================
// src/core/mmio/TcpListenerEndpointWorker.cpp  (NereusSDR)
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

#include "TcpListenerEndpointWorker.h"
#include "MmioEndpoint.h"
#include "FormatParser.h"
#include "../LogCategories.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

namespace NereusSDR {

TcpListenerEndpointWorker::TcpListenerEndpointWorker(MmioEndpoint* endpoint, QObject* parent)
    : ITransportWorker(parent)
    , m_endpoint(endpoint)
{
}

TcpListenerEndpointWorker::~TcpListenerEndpointWorker()
{
    stop();
}

void TcpListenerEndpointWorker::start()
{
    // Tear down any previous server or client before re-binding.
    if (m_client) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_lineBuffer.clear();

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &TcpListenerEndpointWorker::onNewConnection);

    // Resolve bind address: empty or "0.0.0.0" → AnyIPv4, else parse.
    const QString hostStr = m_endpoint->host();
    QHostAddress bindAddr;
    if (hostStr.isEmpty() || hostStr == QStringLiteral("0.0.0.0")) {
        bindAddr = QHostAddress::AnyIPv4;
    } else {
        bindAddr = QHostAddress(hostStr);
    }

    const quint16 port = m_endpoint->port();

    if (!m_server->listen(bindAddr, port)) {
        qCWarning(lcMmio) << "TcpListenerEndpointWorker: failed to bind"
                          << bindAddr.toString() << ":" << port
                          << "—" << m_server->errorString();
        setState(State::Error);
        return;
    }

    setState(State::Listening);
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: listening on"
                   << m_server->serverAddress().toString()
                   << ":" << m_server->serverPort();
}

void TcpListenerEndpointWorker::stop()
{
    if (m_client) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_lineBuffer.clear();
    setState(State::Disconnected);
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: stopped";
}

void TcpListenerEndpointWorker::onNewConnection()
{
    QTcpSocket* incoming = m_server->nextPendingConnection();
    if (!incoming) {
        return;
    }

    if (m_client != nullptr) {
        // Already have a client — reject the new one.
        qCInfo(lcMmio) << "TcpListenerEndpointWorker: rejecting additional"
                          " TCP client — only one connection supported";
        incoming->disconnectFromHost();
        incoming->deleteLater();
        return;
    }

    m_client = incoming;
    connect(m_client, &QTcpSocket::readyRead,
            this, &TcpListenerEndpointWorker::onClientReadyRead);
    connect(m_client, &QTcpSocket::disconnected,
            this, &TcpListenerEndpointWorker::onClientDisconnected);

    m_lineBuffer.clear();
    setState(State::Connected);
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: client connected from"
                   << m_client->peerAddress().toString()
                   << ":" << m_client->peerPort();
}

void TcpListenerEndpointWorker::onClientReadyRead()
{
    m_lineBuffer.append(m_client->readAll());

    // Process all complete newline-delimited messages in the buffer.
    while (true) {
        const int newlinePos = m_lineBuffer.indexOf('\n');
        if (newlinePos < 0) {
            break;
        }

        // Extract the line up to (not including) the newline.
        QByteArray line = m_lineBuffer.left(newlinePos);
        // Remove the consumed bytes (including the newline) from the buffer.
        m_lineBuffer.remove(0, newlinePos + 1);

        // Strip a trailing carriage return to support CR+LF line endings.
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }

        if (line.isEmpty()) {
            continue;
        }

        // Dispatch to the appropriate format parser.
        QHash<QString, QVariant> batch;
        const QUuid guid = m_endpoint->guid();

        switch (m_endpoint->format()) {
        case MmioEndpoint::Format::Json:
            batch = FormatParser::parseJson(line, guid);
            break;
        case MmioEndpoint::Format::Xml:
            batch = FormatParser::parseXml(line, guid);
            break;
        case MmioEndpoint::Format::Raw:
            batch = FormatParser::parseRaw(line, guid);
            break;
        }

        if (!batch.isEmpty()) {
            emit valueBatchReceived(batch);
        }
    }
}

void TcpListenerEndpointWorker::onClientDisconnected()
{
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: TCP client disconnected";
    m_client->deleteLater();
    m_client = nullptr;
    m_lineBuffer.clear();
    // Keep the server alive — return to listening for the next client.
    setState(State::Listening);
}

} // namespace NereusSDR
