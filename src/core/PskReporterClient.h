// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - PskReporterClient: PSK Reporter IPFIX protocol client.
//
// Ported from freedv-gui src/reporting/pskreporter.{h,cpp} [@77e793a].
//   Wire-protocol logic (rxFormatHeader / txFormatHeader template
//   payloads, 16-byte IPFIX header, sender-record encoding, RX/TX data
//   set framing, 4-byte alignment, server hostname/port) all come
//   from the freedv-gui source.
//
// License (upstream):
//   - freedv-gui carries an LGPLv2.1+ root license (`freedv-gui/COPYING`).
//     The specific `pskreporter.{h,cpp}` files carry a permissive
//     BSD-2-Clause-style file header (Copyright Mooneer Salem, no per-
//     file project copyright line); the BSD permission block is
//     reproduced verbatim below per the upstream redistribution clause.
//
// LGPL is upgrade-compatible to GPL-3 (LGPL §3 conversion clause); the
// BSD-2-Clause file-header carve-out is GPL-compatible by its own terms.
//
// --- From freedv-gui src/reporting/pskreporter.h [@77e793a] (verbatim header) ---
//
// =========================================================================
//  Name:            pskreporter.h
//  Purpose:         Implementation of PSK Reporter support.
//
//  Authors:         Mooneer Salem
//  License:
//
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//  - Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//
//  - Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// =========================================================================
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B6. Initial port.
//                                    Replaces freedv-gui's POSIX socket /
//                                    detached std::thread with Qt6's
//                                    QUdpSocket, and folds the
//                                    SenderRecord helper struct into
//                                    private static helpers on the
//                                    client. Adds a parser side
//                                    (parseDatagramForTest /
//                                    spotReceived signal) so an
//                                    incoming sender-record datagram
//                                    round-trips into DxSpot for the
//                                    SpotModel; freedv-gui only sends,
//                                    never parses incoming PSK
//                                    Reporter datagrams. AI tooling:
//                                    Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QString>
#include <QHostAddress>
#include <QVector>

#include "DxSpot.h"

class QUdpSocket;
class QTimer;

namespace NereusSDR {

// PSK Reporter IPFIX (RFC 5101) v0.1 client. Speaks the wire format
// understood by report.pskreporter.info:4739.
//
// The class supports both directions even though only the send side is
// part of the upstream freedv-gui code:
//   - **Send**: queue a decode via reportDecode() and the next send()
//     call (auto-triggered after autoSendIntervalSec, or manual) bundles
//     queued records into one IPFIX datagram and sendto()s it.
//   - **Receive**: incoming datagrams (e.g. from a local listener bound
//     on `listenPort`) are parsed back into DxSpot values via the
//     spotReceived signal. PSK Reporter's public infrastructure does
//     not push live-spot datagrams to clients (it is a write-only
//     network from the radio's perspective), so receive is gated on a
//     locally-bound listener. The shape is in place so we can drive
//     it from canned datagrams in tests and from any future relay
//     protocol that delivers PSK Reporter records to NereusSDR.
class PskReporterClient : public QObject {
    Q_OBJECT

public:
    explicit PskReporterClient(QObject* parent = nullptr);
    ~PskReporterClient() override;

    // Identity for outgoing datagrams. Mirrors the upstream PskReporter
    // constructor signature (pskreporter.cpp:148-169 [@77e793a]).
    void setIdentity(const QString& callsign, const QString& gridSquare,
                     const QString& version);
    void setServerHost(const QString& host, quint16 port = 4739);

    // Optional UDP listener for relay-style ingest. Port 0 disables.
    void startListening(quint16 port = 0);
    void stopListening();
    bool isListening() const;

    // Queue a decode our radio made; bundled into the next send().
    void reportDecode(const QString& dxCall, const QString& mode,
                      double freqMhz, int snr);

    // Force a datagram flush. Returns true if the datagram was sent.
    bool send();

    // Polling cadence. 0 disables auto-send (manual `send()` only).
    void setAutoSendIntervalSec(int sec);

    // Source-first port from freedv-gui semantics: PSK Reporter is
    // "enabled" iff the auto-send timer is running.  Callers (e.g.
    // RadioModel decode fan-outs) gate their reportDecode() calls on
    // this so queueing only happens while the operator has explicitly
    // started the reporter — matches freedv-gui main.cpp:2575-2582
    // [@77e793a] where PskReporter is only added to m_reporters[] if
    // pskReporterEnabled is true.
    bool isAutoSendActive() const;

    // From freedv-gui main.cpp:2597 [@77e793a]:
    //   m_pskReporterTimer.Start(5 * 60 * 1000);  // 5 minutes
    // PSK Reporter's IPFIX server (report.pskreporter.info:4739)
    // accepts datagrams at most every 5 minutes per reporter; more
    // frequent transmissions get silently dropped.  freedv-gui
    // hardcodes 5 minutes; NereusSDR exposes it as a named constant
    // so the Start button can use the upstream cadence verbatim.
    static constexpr int kReportingIntervalSec = 300;

    // Test seams. These mirror the upstream PskReporter::reportCommon_
    // (pskreporter.cpp:282-372 [@77e793a]) and
    // PskReporter::encodeSenderRecords_ (pskreporter.cpp:261-280
    // [@77e793a]) shapes, but build into a QByteArray instead of
    // sending over the socket. The parser-side test seam
    // (parseDatagramForTest) round-trips a canned datagram back into
    // DxSpot values via the public spotReceived signal.
    QByteArray buildDatagramForTest();
    QVector<DxSpot> parseDatagramForTest(const QByteArray& datagram);

    // Per-record helpers exposed for unit tests. Mirror the upstream
    // SenderRecord::recordSize and SenderRecord::encode
    // (pskreporter.cpp:113-146 [@77e793a]).
    static int senderRecordSizeForTest(const QString& callsign,
                                        const QString& mode);
    static QByteArray encodeSenderRecordForTest(const QString& callsign,
                                                 quint64 frequencyHz,
                                                 int snr,
                                                 const QString& mode,
                                                 quint32 flowTimeSeconds);

signals:
    // Drives SpotModel via the SpotCollector adapter.
    void spotReceived(const NereusSDR::DxSpot& spot);

    // Lifecycle / error.
    void errorOccurred(const QString& error);

private slots:
    void onSocketReadyRead();
    void onAutoSendTick();

private:
    // Queued sender record. Mirrors freedv-gui SenderRecord (pskreporter.h:41-54
    // [@77e793a]) but stored as a Qt value type so we can sit on the
    // queue without locking.
    struct PendingRecord {
        QString callsign;
        quint64 frequencyHz{0};
        qint8   snr{0};
        QString mode;
        quint8  infoSource{1};
        quint32 flowTimeSeconds{0};
    };

    // Build the next outgoing datagram. Caller holds the queue lock.
    QByteArray buildDatagramLocked();

    // Parse a single incoming datagram. Emits spotReceived for each
    // sender record found.
    QVector<DxSpot> parseDatagramLocked(const QByteArray& datagram);

    QUdpSocket*       m_socket{nullptr};
    QTimer*           m_autoSendTimer{nullptr};

    QString           m_receiverCallsign;
    QString           m_receiverGridSquare;
    QString           m_decodingSoftware;

    QString           m_serverHost{QStringLiteral("report.pskreporter.info")};
    quint16           m_serverPort{4739};

    QVector<PendingRecord> m_queue;

    quint32           m_sequenceNumber{0};
    quint32           m_randomIdentifier{0};
};

}  // namespace NereusSDR
