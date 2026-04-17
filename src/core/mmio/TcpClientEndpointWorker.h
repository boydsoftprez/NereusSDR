#pragma once
// =================================================================
// src/core/mmio/TcpClientEndpointWorker.h  (NereusSDR)
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

// Phase 3G-6 block 5 — TCP client transport worker.
// Dials an outbound QTcpSocket to the endpoint's (host, port), reads
// newline-delimited messages from the remote server, dispatches each through
// the format parser (JSON/XML/RAW), and emits batches via valueBatchReceived().
// Auto-reconnects on drop with a fixed backoff timer.
//
// Porting intent from Thetis MeterManager.cs:40160-40403 (TcpClientHandler
// inner class), but uses Qt signal/slot (QTcpSocket::connected,
// QTcpSocket::readyRead, QTcpSocket::disconnected, QTcpSocket::errorOccurred)
// instead of Thetis's polling loop.

#include "ITransportWorker.h"
#include <QByteArray>

class QTcpSocket;
class QTimer;

namespace NereusSDR {

class MmioEndpoint;

class TcpClientEndpointWorker : public ITransportWorker {
    Q_OBJECT

public:
    explicit TcpClientEndpointWorker(MmioEndpoint* endpoint, QObject* parent = nullptr);
    ~TcpClientEndpointWorker() override;

public slots:
    void start() override;
    void stop() override;

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError();
    void attemptReconnect();

private:
    MmioEndpoint* m_endpoint;
    QTcpSocket*   m_socket{nullptr};
    QTimer*       m_reconnectTimer{nullptr};
    QByteArray    m_lineBuffer;
    bool          m_stopping{false};

    // From Thetis MeterManager.cs:40180 — reconnect delay is 3 seconds.
    static constexpr int kReconnectDelayMs = 3000;
};

} // namespace NereusSDR
