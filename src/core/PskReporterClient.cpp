// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - PskReporterClient implementation.
//
// Ported from freedv-gui src/reporting/pskreporter.cpp [@77e793a].
//   The IPFIX wire format (16-byte header, rxFormatHeader,
//   txFormatHeader, sender-record layout, 4-byte alignment, server
//   address) follows the freedv-gui source byte-for-byte.
//
// License (upstream):
//   - freedv-gui carries an LGPLv2.1+ root license (`freedv-gui/COPYING`).
//     The specific `pskreporter.cpp` file carries a permissive
//     BSD-2-Clause-style file header (Copyright Mooneer Salem, no per-
//     file project copyright line); the BSD permission block is
//     reproduced verbatim below per the upstream redistribution clause.
//
// LGPL is upgrade-compatible to GPL-3 (LGPL §3 conversion clause); the
// BSD-2-Clause file-header carve-out is GPL-compatible by its own terms.
//
// --- From freedv-gui src/reporting/pskreporter.cpp [@77e793a] (verbatim header) ---
//
// =========================================================================
//  Name:            pskreporter.cpp
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
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B6. See
//                                    PskReporterClient.h for the full
//                                    attribution block. Implementation
//                                    is a faithful translation of
//                                    upstream's reportCommon_,
//                                    encodeReceiverRecord_,
//                                    encodeSenderRecords_,
//                                    SenderRecord::encode, and
//                                    getRx/TxDataSize_ helpers
//                                    (pskreporter.cpp:103-372
//                                    [@77e793a]) into Qt6 idioms:
//                                    QByteArray byte buffers instead of
//                                    raw `char*` + `new[]`, QUdpSocket
//                                    instead of POSIX socket +
//                                    detached thread, Qt event-loop
//                                    QTimer instead of a manual loop.
//                                    Parser side (parseDatagramLocked
//                                    /spotReceived) is a NereusSDR
//                                    addition; freedv-gui is send-only.
//                                    AI tooling: Anthropic Claude Code.

#include "PskReporterClient.h"

#include <QDateTime>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QTimer>
#include <QUdpSocket>

namespace NereusSDR {

namespace {

// From freedv-gui src/reporting/pskreporter.cpp:82-88 [@77e793a].
// RX record template: receiverCallsign, receiverLocator,
// decodingSoftware (each variable-length, enterprise PEN 0x0000768F).
// Template ID 0x9992 (decimal 39314).
static const unsigned char rxFormatHeader[] = {
    0x00, 0x03, 0x00, 0x24, 0x99, 0x92, 0x00, 0x03, 0x00, 0x00,
    0x80, 0x02, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x04, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x08, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x00, 0x00
};

// From freedv-gui src/reporting/pskreporter.cpp:93-101 [@77e793a].
// TX record template: senderCallsign (variable), frequency (5 bytes
// for 10+ GHz support), SNR (1 byte signed), mode (variable),
// informationSource (1 byte), flowStartSeconds (4 bytes). Template ID
// 0x9993 (decimal 39315).
static const unsigned char txFormatHeader[] = {
    0x00, 0x02, 0x00, 0x34, 0x99, 0x93, 0x00, 0x06,
    0x80, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x05, 0x00, 0x05, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x0A, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x0B, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
    0x00, 0x96, 0x00, 0x04
};

// Big-endian writers. PSK Reporter IPFIX is network byte order on every
// numeric field. We write directly into the QByteArray byte buffer at
// caller-controlled offsets, matching upstream's pointer-arithmetic
// style.
inline void writeU16(QByteArray& buf, int off, quint16 v) {
    buf[off + 0] = char((v >> 8) & 0xff);
    buf[off + 1] = char(v & 0xff);
}
inline void writeU32(QByteArray& buf, int off, quint32 v) {
    buf[off + 0] = char((v >> 24) & 0xff);
    buf[off + 1] = char((v >> 16) & 0xff);
    buf[off + 2] = char((v >> 8) & 0xff);
    buf[off + 3] = char(v & 0xff);
}

// Big-endian readers, mirror.
inline quint16 readU16(const QByteArray& buf, int off) {
    return (quint16(quint8(buf[off + 0])) << 8) |
           quint16(quint8(buf[off + 1]));
}
inline quint32 readU32(const QByteArray& buf, int off) {
    return (quint32(quint8(buf[off + 0])) << 24) |
           (quint32(quint8(buf[off + 1])) << 16) |
           (quint32(quint8(buf[off + 2])) << 8) |
           quint32(quint8(buf[off + 3]));
}

// RX data set size per freedv-gui pskreporter.cpp:204-213 [@77e793a]:
//   4 (set header) + (1 + cs.size) + (1 + locator.size) + (1 + sw.size),
//   padded to 4-byte boundary.
int rxDataSize(const QByteArray& cs, const QByteArray& loc,
               const QByteArray& sw) {
    int size = 4 + (1 + cs.size()) + (1 + loc.size()) + (1 + sw.size());
    if (size % 4) {
        size += 4 - (size % 4);
    }
    return size;
}

}  // namespace

// From freedv-gui SenderRecord::recordSize (pskreporter.cpp:113-116
// [@77e793a]):
//   (1 + callsign.size()) + 5 + 1 + (1 + mode.size()) + 1 + 4
int PskReporterClient::senderRecordSizeForTest(const QString& callsign,
                                                const QString& mode) {
    const QByteArray cs = callsign.toUtf8();
    const QByteArray md = mode.toUtf8();
    return (1 + cs.size()) + 5 + 1 + (1 + md.size()) + 1 + 4;
}

// Ports freedv-gui SenderRecord::encode (pskreporter.cpp:118-146
// [@77e793a]). Byte layout:
//   [csLen][callsign bytes][5-byte BE freq][SNR signed]
//   [modeLen][mode bytes][infoSource][4-byte BE flowTime]
QByteArray PskReporterClient::encodeSenderRecordForTest(
    const QString& callsign, quint64 frequencyHz, int snr,
    const QString& mode, quint32 flowTimeSeconds) {
    const QByteArray cs = callsign.toUtf8();
    const QByteArray md = mode.toUtf8();
    const int size = (1 + cs.size()) + 5 + 1 + (1 + md.size()) + 1 + 4;

    QByteArray out(size, char(0));
    int off = 0;

    // Encode callsign (length byte + chars). pskreporter.cpp:121-123
    out[off] = char(cs.size());
    for (int i = 0; i < cs.size(); ++i) {
        out[off + 1 + i] = cs[i];
    }
    off += 1 + cs.size();

    // Encode frequency. pskreporter.cpp:127-131
    // 5-byte big-endian, shifts 32/24/16/8/0.
    out[off + 0] = char((frequencyHz >> 32) & 0xff);
    out[off + 1] = char((frequencyHz >> 24) & 0xff);
    out[off + 2] = char((frequencyHz >> 16) & 0xff);
    out[off + 3] = char((frequencyHz >> 8) & 0xff);
    out[off + 4] = char(frequencyHz & 0xff);
    off += 5;

    // Encode SNR. pskreporter.cpp:134 (signed char)
    out[off] = char(qint8(snr));
    off += 1;

    // Encode mode. pskreporter.cpp:137-138
    out[off] = char(md.size());
    for (int i = 0; i < md.size(); ++i) {
        out[off + 1 + i] = md[i];
    }
    off += 1 + md.size();

    // Encode infoSource. pskreporter.cpp:142 (constant 1 in upstream,
    // ported here as the third helper argument default).
    out[off] = char(1);
    off += 1;

    // Encode flow start time. pskreporter.cpp:145 (htonl)
    writeU32(out, off, flowTimeSeconds);

    return out;
}

PskReporterClient::PskReporterClient(QObject* parent)
    : QObject(parent) {
    // From freedv-gui pskreporter.cpp:154-157 [@77e793a]:
    // randomIdentifier_ is a per-instance random uint used as the
    // IPFIX observationDomain. Use QRandomGenerator instead of
    // std::random_device for Qt parity with the rest of NereusSDR.
    m_randomIdentifier = QRandomGenerator::system()->generate();

    m_autoSendTimer = new QTimer(this);
    m_autoSendTimer->setSingleShot(false);
    connect(m_autoSendTimer, &QTimer::timeout,
            this, &PskReporterClient::onAutoSendTick);
}

PskReporterClient::~PskReporterClient() {
    // From freedv-gui pskreporter.cpp:171-181 [@77e793a]:
    //   if (recordList_.size() > 0) { reportCommon_(); }
    // Flush any queued records on shutdown.
    if (!m_queue.isEmpty()) {
        send();
    }
}

void PskReporterClient::setIdentity(const QString& callsign,
                                     const QString& gridSquare,
                                     const QString& version) {
    m_receiverCallsign = callsign;
    m_receiverGridSquare = gridSquare;
    m_decodingSoftware = version;
}

void PskReporterClient::setServerHost(const QString& host, quint16 port) {
    m_serverHost = host;
    m_serverPort = port;
}

void PskReporterClient::startListening(quint16 port) {
    if (port == 0) {
        stopListening();
        return;
    }
    if (!m_socket) {
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead,
                this, &PskReporterClient::onSocketReadyRead);
    }
    if (m_socket->state() != QUdpSocket::BoundState) {
        if (!m_socket->bind(QHostAddress::Any, port)) {
            emit errorOccurred(
                QStringLiteral("PSK Reporter listener bind failed: %1")
                    .arg(m_socket->errorString()));
        }
    }
}

void PskReporterClient::stopListening() {
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

bool PskReporterClient::isListening() const {
    return m_socket && m_socket->state() == QUdpSocket::BoundState;
}

// From freedv-gui PskReporter::addReceiveRecord (pskreporter.cpp:183-194
// [@77e793a]). Upstream auto-flushes at 50 queued records to avoid
// overflowing a UDP datagram; we preserve that ceiling.
void PskReporterClient::reportDecode(const QString& dxCall,
                                      const QString& mode,
                                      double freqMhz, int snr) {
    PendingRecord r;
    r.callsign = dxCall;
    r.frequencyHz = quint64(freqMhz * 1e6 + 0.5);
    r.snr = qint8(qBound(-128, snr, 127));
    r.mode = mode;
    r.infoSource = 1;
    r.flowTimeSeconds = quint32(QDateTime::currentSecsSinceEpoch());
    m_queue.append(r);

    // pskreporter.cpp:188-193 [@77e793a]: 50-record auto-flush.
    if (m_queue.size() >= 50) {
        send();
    }
}

bool PskReporterClient::send() {
    if (m_queue.isEmpty() && m_receiverCallsign.isEmpty()) {
        // Nothing to report and no identity to register. Skip.
        return false;
    }

    QByteArray datagram = buildDatagramLocked();
    if (datagram.isEmpty()) {
        return false;
    }

    // Lazy-init the send-side socket.
    if (!m_socket) {
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead,
                this, &PskReporterClient::onSocketReadyRead);
    }

    // From freedv-gui pskreporter.cpp:335-365 [@77e793a]: resolve the
    // server hostname and sendto(). QUdpSocket::writeDatagram() handles
    // DNS resolution + sendto in one call.
    qint64 written = m_socket->writeDatagram(datagram, QHostAddress(m_serverHost),
                                              m_serverPort);
    if (written < 0) {
        // Hostname not pre-resolved? Try the lookup path.
        // QUdpSocket::writeDatagram supports QString hostname overload.
        // Most platforms accept the QHostAddress(QString) ctor with a
        // numeric address only; fall back to writing via QHostAddress
        // after lookupHost for hostnames like "report.pskreporter.info".
        QHostInfo info = QHostInfo::fromName(m_serverHost);
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            emit errorOccurred(QStringLiteral("cannot resolve %1: %2")
                                   .arg(m_serverHost, info.errorString()));
            return false;
        }
        written = m_socket->writeDatagram(datagram, info.addresses().first(),
                                           m_serverPort);
    }
    if (written < 0) {
        emit errorOccurred(QStringLiteral("cannot send to %1:%2 - %3")
                               .arg(m_serverHost)
                               .arg(m_serverPort)
                               .arg(m_socket->errorString()));
        return false;
    }

    // pskreporter.cpp:332 [@77e793a]: clear queue after successful send.
    m_queue.clear();
    return true;
}

void PskReporterClient::setAutoSendIntervalSec(int sec) {
    if (sec <= 0) {
        m_autoSendTimer->stop();
        return;
    }
    m_autoSendTimer->start(sec * 1000);
}

bool PskReporterClient::isAutoSendActive() const {
    return m_autoSendTimer != nullptr && m_autoSendTimer->isActive();
}

QByteArray PskReporterClient::buildDatagramForTest() {
    return buildDatagramLocked();
}

QVector<DxSpot> PskReporterClient::parseDatagramForTest(
    const QByteArray& datagram) {
    return parseDatagramLocked(datagram);
}

// Ports freedv-gui PskReporter::reportCommon_ (pskreporter.cpp:282-372
// [@77e793a]) for the datagram-construction half. The send-side socket
// path is in send() above.
QByteArray PskReporterClient::buildDatagramLocked() {
    const QByteArray cs = m_receiverCallsign.toUtf8();
    const QByteArray loc = m_receiverGridSquare.toUtf8();
    const QByteArray sw = m_decodingSoftware.toUtf8();

    const int rxSize = rxDataSize(cs, loc, sw);

    // TX data set: 4-byte header + per-record bytes, padded to 4-byte
    // boundary. pskreporter.cpp:215-233 [@77e793a].
    int txSize = 0;
    if (!m_queue.isEmpty()) {
        txSize = 4;
        for (const auto& r : m_queue) {
            txSize += senderRecordSizeForTest(r.callsign, r.mode);
        }
        if (txSize % 4) {
            txSize += 4 - (txSize % 4);
        }
    }

    // Datagram size. pskreporter.cpp:290-291 [@77e793a]:
    //   16 (header) + sizeof(rxFormatHeader) + sizeof(txFormatHeader) +
    //   rxDataSize + txDataSize, minus sizeof(txFormatHeader) when no
    //   sender records are queued.
    int dgSize = 16 + int(sizeof(rxFormatHeader)) + int(sizeof(txFormatHeader)) +
                 rxSize + txSize;
    if (txSize == 0) {
        dgSize -= int(sizeof(txFormatHeader));
    }

    QByteArray dg(dgSize, char(0));

    // Encode packet header. pskreporter.cpp:297-314 [@77e793a].
    // Version field: 0x000A (IPFIX v10).
    dg[0] = char(0x00);
    dg[1] = char(0x0A);
    writeU16(dg, 2, quint16(dgSize));
    writeU32(dg, 4, quint32(QDateTime::currentSecsSinceEpoch()));
    writeU32(dg, 8, m_sequenceNumber++);
    writeU32(dg, 12, m_randomIdentifier);

    int off = 16;

    // Copy RX format header verbatim. pskreporter.cpp:318-319 [@77e793a].
    for (size_t i = 0; i < sizeof(rxFormatHeader); ++i) {
        dg[off + int(i)] = char(rxFormatHeader[i]);
    }
    off += int(sizeof(rxFormatHeader));

    // Conditionally copy TX format header. pskreporter.cpp:321-325 [@77e793a].
    if (txSize > 0) {
        for (size_t i = 0; i < sizeof(txFormatHeader); ++i) {
            dg[off + int(i)] = char(txFormatHeader[i]);
        }
        off += int(sizeof(txFormatHeader));
    }

    // Encode receiver record. pskreporter.cpp:235-259 [@77e793a].
    // 0x9992 + size + length-prefixed strings, padded.
    dg[off + 0] = char(0x99);
    dg[off + 1] = char(0x92);
    writeU16(dg, off + 2, quint16(rxSize));
    int rxFieldOff = off + 4;

    dg[rxFieldOff] = char(cs.size());
    for (int i = 0; i < cs.size(); ++i) {
        dg[rxFieldOff + 1 + i] = cs[i];
    }
    rxFieldOff += 1 + cs.size();

    dg[rxFieldOff] = char(loc.size());
    for (int i = 0; i < loc.size(); ++i) {
        dg[rxFieldOff + 1 + i] = loc[i];
    }
    rxFieldOff += 1 + loc.size();

    dg[rxFieldOff] = char(sw.size());
    for (int i = 0; i < sw.size(); ++i) {
        dg[rxFieldOff + 1 + i] = sw[i];
    }
    // Padding bytes (if any) are already zero from QByteArray(size, 0).
    off += rxSize;

    // Encode sender records, if any. pskreporter.cpp:261-280 [@77e793a].
    if (txSize > 0) {
        dg[off + 0] = char(0x99);
        dg[off + 1] = char(0x93);
        writeU16(dg, off + 2, quint16(txSize));
        int txFieldOff = off + 4;
        for (const auto& r : m_queue) {
            const QByteArray rec = encodeSenderRecordForTest(
                r.callsign, r.frequencyHz, r.snr, r.mode, r.flowTimeSeconds);
            // The infoSource byte the helper emits is hard-coded to 1
            // (matching upstream pskreporter.cpp:109 [@77e793a]).
            for (int i = 0; i < rec.size(); ++i) {
                dg[txFieldOff + i] = rec[i];
            }
            txFieldOff += rec.size();
        }
        // Padding bytes (if any) are already zero from QByteArray ctor.
    }

    return dg;
}

// Parser side. Walk the IPFIX datagram and emit a DxSpot for every
// sender record we find. This is a NereusSDR addition; freedv-gui's
// pskreporter is send-only.
QVector<DxSpot> PskReporterClient::parseDatagramLocked(
    const QByteArray& datagram) {
    QVector<DxSpot> spots;
    if (datagram.size() < 16) {
        return spots;
    }
    // Header: version=0x000A, length, exportTime, sequence, observationDomain.
    if (readU16(datagram, 0) != 0x000A) {
        return spots;
    }
    const int totalLen = int(readU16(datagram, 2));
    if (totalLen != datagram.size()) {
        // Length mismatch; refuse to walk.
        return spots;
    }

    // Walk the body (offset 16 onward) one set at a time. Each set has
    // a 4-byte header: setID (2) + setLength (2). setID 0x0002/0x0003
    // are templates; 0x9992 is RX data; 0x9993 is TX data; anything
    // else gets skipped by length.
    int off = 16;
    while (off + 4 <= datagram.size()) {
        const quint16 setId = readU16(datagram, off);
        const quint16 setLen = readU16(datagram, off + 2);
        if (setLen < 4 || off + setLen > datagram.size()) {
            break;  // malformed
        }
        if (setId == 0x9993) {
            // Walk the records inside the TX data set.
            int recOff = off + 4;
            const int setEnd = off + setLen;
            while (recOff < setEnd) {
                // Callsign (length-prefixed).
                if (recOff >= setEnd) {
                    break;
                }
                const int csLen = quint8(datagram[recOff]);
                recOff += 1;
                if (recOff + csLen > setEnd) {
                    break;
                }
                const QString callsign =
                    QString::fromUtf8(datagram.mid(recOff, csLen));
                recOff += csLen;

                // Frequency (5 BE bytes).
                if (recOff + 5 > setEnd) {
                    break;
                }
                quint64 freqHz = 0;
                freqHz |= quint64(quint8(datagram[recOff + 0])) << 32;
                freqHz |= quint64(quint8(datagram[recOff + 1])) << 24;
                freqHz |= quint64(quint8(datagram[recOff + 2])) << 16;
                freqHz |= quint64(quint8(datagram[recOff + 3])) << 8;
                freqHz |= quint64(quint8(datagram[recOff + 4]));
                recOff += 5;

                // SNR (signed).
                if (recOff + 1 > setEnd) {
                    break;
                }
                const int snr = qint8(datagram[recOff]);
                recOff += 1;

                // Mode (length-prefixed).
                if (recOff >= setEnd) {
                    break;
                }
                const int modeLen = quint8(datagram[recOff]);
                recOff += 1;
                if (recOff + modeLen > setEnd) {
                    break;
                }
                const QString mode =
                    QString::fromUtf8(datagram.mid(recOff, modeLen));
                recOff += modeLen;

                // infoSource (1 byte) + flowTime (4 BE bytes). We use
                // flowTime as the spot UTC stamp.
                if (recOff + 5 > setEnd) {
                    break;
                }
                recOff += 1;  // infoSource (ignored on receive)
                const quint32 flow = readU32(datagram, recOff);
                recOff += 4;

                if (callsign.isEmpty()) {
                    continue;
                }

                DxSpot s;
                s.dxCall = callsign;
                s.freqMhz = double(freqHz) / 1e6;
                s.snr = snr;
                s.comment = mode;
                s.source = QStringLiteral("PSK");
                s.utcTime = QDateTime::fromSecsSinceEpoch(qint64(flow)).toUTC().time();
                spots.append(s);
                emit spotReceived(s);
            }
        }
        off += setLen;
    }
    return spots;
}

void PskReporterClient::onSocketReadyRead() {
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray dg;
        dg.resize(int(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(dg.data(), dg.size());
        parseDatagramLocked(dg);
    }
}

void PskReporterClient::onAutoSendTick() {
    if (!m_queue.isEmpty()) {
        send();
    }
}

}  // namespace NereusSDR
