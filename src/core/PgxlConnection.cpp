// =================================================================
// src/core/PgxlConnection.cpp  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/core/PgxlConnection.{h,cpp} [@0cd4559].
//                 processLine() stubbed; V/R/S frame parsing lands in Tasks 6+7.
// =================================================================
#include "PgxlConnection.h"
#include "AppSettings.h"
#include <QDateTime>
#include <QLoggingCategory>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcPgxl, "nereus.pgxl")

// Exponential backoff schedule for auto-reconnect, in seconds.
// From FlexRadio wiki spec + design §6.4: amp keeps connection state;
// NereusSDR must reconnect promptly on blip. Cap at 60 s.
static constexpr int kBackoffSec[] = {1, 2, 5, 10, 30, 60};

PgxlConnection::PgxlConnection(QObject* parent)
    : QObject(parent) {
    connect(&m_socket, &QTcpSocket::connected,     this, &PgxlConnection::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected,  this, &PgxlConnection::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead,     this, &PgxlConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &PgxlConnection::onError);

    m_pollTimer.setInterval(200);  // 5 Hz per AetherSDR
    connect(&m_pollTimer, &QTimer::timeout, this, &PgxlConnection::pollStatus);

    // Keepalive timer: issues a status poke at the configured interval.
    // Disabled until enableKeepalive() is called.
    m_keepaliveTimer.setSingleShot(false);
    connect(&m_keepaliveTimer, &QTimer::timeout, this, &PgxlConnection::onKeepaliveTimeout);

    // Ping timeout checker: every 5 s, evict pings older than 5 s.
    m_pingTimeoutTimer.setInterval(5000);
    m_pingTimeoutTimer.setSingleShot(false);
    connect(&m_pingTimeoutTimer, &QTimer::timeout, this, &PgxlConnection::onPingTimeoutCheck);
    m_pingTimeoutTimer.start();
}

void PgxlConnection::connectToPgxl(const QString& host, quint16 port) {
    // Idempotent guard: if the socket is in any state other than Unconnected,
    // a connect attempt is already in flight or established.  Re-issuing
    // connectToHost() on the same QTcpSocket emits "Trying to connect while
    // connection is in progress".  Auto-connect (Task 20) + a manual click
    // can race exactly during the handshake window between connectToHost()
    // and the V-frame arrival that flips m_connected = true.
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        qCDebug(lcPgxl) << "connectToPgxl: socket already in state"
                        << m_socket.state() << "- ignoring duplicate";
        return;
    }
    m_lastHost = host;
    m_lastPort = port;
    m_seq = 0;
    m_gotVersion = false;
    m_version.clear();
    m_readBuf.clear();
    qCDebug(lcPgxl) << "connecting to" << host << ":" << port;
    m_socket.connectToHost(host, port);
}

void PgxlConnection::disconnect() {
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_connected = false;
    m_socket.disconnectFromHost();
}

quint32 PgxlConnection::sendCommand(const QString& cmd) {
    quint32 seq = ++m_seq;
    QString line = QString("C%1|%2\n").arg(seq).arg(cmd);
    m_socket.write(line.toUtf8());
    qCDebug(lcPgxl) << "sent" << line.trimmed();
    // Phase 3P-II bench-diagnostic logging (remove after pairing protocol confirmed)
    qCInfo(lcPgxl) << "TX seq=" << seq << "cmd:" << cmd;
    emit testFrameWrittenForTesting(line.trimmed());  // test seam
    ++m_framesOut;
    m_bytesOut += quint64(line.size());
    return seq;
}

void PgxlConnection::onConnected() {
    qCDebug(lcPgxl) << "TCP connected, waiting for version line";
    m_connectedSinceMs = QDateTime::currentMSecsSinceEpoch();
    m_reconnectAttempts = 0;
}

void PgxlConnection::onDisconnected() {
    qCDebug(lcPgxl) << "disconnected";
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_connected = false;
    // Phase 3P-II Task 66: clear paired serial on disconnect so setBand() stays silent.
    m_pairedRadioSerial.clear();
    emit disconnected();
    scheduleReconnect();
}

void PgxlConnection::onError() {
    QString err = m_socket.errorString();
    qCWarning(lcPgxl) << "socket error:" << err;
    emit connectionFailed(err);
}

void PgxlConnection::onReadyRead() {
    QByteArray chunk = m_socket.readAll();
    m_bytesIn += quint64(chunk.size());
    m_readBuf.append(chunk);
    while (true) {
        int idx = m_readBuf.indexOf('\n');
        if (idx < 0) { break; }
        QString line = QString::fromUtf8(m_readBuf.left(idx)).trimmed();
        m_readBuf.remove(0, idx + 1);
        if (!line.isEmpty()) {
            ++m_framesIn;
            m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
            processLine(line);
        }
    }
}

void PgxlConnection::pollStatus() {
    if (m_connected) {
        sendCommand("status");
    }
}

// -------------------------------------------------------------------------
// Tier 2 NereusSDR-native command surface.
// Wire formats from FlexRadio PowerGenius Ethernet API wiki spec (design §6.4).
// -------------------------------------------------------------------------

// From FlexRadio wiki spec: amplifier create ip=<ip> port=<port> model=<model> serial_num=<serial> ant=<map>
quint32 PgxlConnection::amplifierCreate(const QString& serial,
                                        const QString& model,
                                        const QString& antMap) {
    QString ourIp = m_socket.localAddress().toString();
    return sendCommand(QString("amplifier create ip=%1 port=%2 model=%3 serial_num=%4 ant=%5")
        .arg(ourIp)
        .arg(9008)
        .arg(model)
        .arg(serial)
        .arg(antMap));
}

// From FlexRadio wiki spec: flexradio ampslice=<A|B|C|D> serial=<radio_serial> txant=<ant> ptt=<LAN|NONE> active=<0|1>
quint32 PgxlConnection::flexradioPair(QChar ampSlice,
                                      const QString& radioSerial,
                                      const QString& txAnt,
                                      bool pttOverLan,
                                      bool active) {
    // Phase 3P-II Task 66: capture serial so setBand() can use it.
    m_pairedRadioSerial = radioSerial;
    quint32 seq = sendCommand(
        QString("flexradio ampslice=%1 serial=%2 txant=%3 ptt=%4 active=%5")
            .arg(ampSlice)
            .arg(radioSerial)
            .arg(txAnt)
            .arg(pttOverLan ? "LAN" : "NONE")
            .arg(active ? 1 : 0));
    m_pendingPairingSeq = seq;
    return seq;
}

// From FlexRadio wiki spec: keepalive enable
quint32 PgxlConnection::enableKeepalive() {
    quint32 seq = sendCommand("keepalive enable");
    auto& s = AppSettings::instance();
    int intervalSec = s.value("PGXL_KeepaliveSec", "30").toInt();
    m_keepaliveTimer.setInterval(intervalSec * 1000);
    m_keepaliveTimer.start();
    return seq;
}

// From FlexRadio wiki spec: ping (no-op roundtrip for RTT measurement)
quint32 PgxlConnection::ping(const QString& tag) {
    quint32 seq = sendCommand("ping");
    m_pendingPings.insert(seq, PendingPing{seq, QDateTime::currentMSecsSinceEpoch(), tag});
    return seq;
}

// From FlexRadio wiki spec: interlock create type=AMP valid_antennas=<list> name=<name> serial=<serial>
quint32 PgxlConnection::interlockCreate(const QString& validAntennas,
                                        const QString& name,
                                        const QString& serial) {
    return sendCommand(
        QString("interlock create type=AMP valid_antennas=%1 name=%2 serial=%3")
            .arg(validAntennas)
            .arg(name)
            .arg(serial));
}

// From FlexRadio wiki spec: interlock disable <id>
quint32 PgxlConnection::interlockDisable(int interlockId) {
    return sendCommand(QString("interlock disable %1").arg(interlockId));
}

// From FlexRadio wiki spec: setup read
quint32 PgxlConnection::readSetup() {
    return sendCommand("setup read");
}

// From FlexRadio wiki spec: setup <kv> ... (write fields as space-separated k=v pairs)
quint32 PgxlConnection::writeSetup(const QMap<QString,QString>& fields) {
    QStringList parts;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        parts << QString("%1=%2").arg(it.key(), it.value());
    }
    return sendCommand(QString("setup %1").arg(parts.join(' ')));
}

// From FlexRadio wiki spec: ifconf read
quint32 PgxlConnection::readIfconf() {
    return sendCommand("ifconf read");
}

// From FlexRadio wiki spec: ifconf address=<ip> netmask=<mask> gateway=<gw> dhcp=<true|false>
quint32 PgxlConnection::writeIfconf(const QString& ip,
                                    const QString& netmask,
                                    const QString& gateway,
                                    bool dhcp) {
    return sendCommand(
        QString("ifconf address=%1 netmask=%2 gateway=%3 dhcp=%4")
            .arg(ip)
            .arg(netmask)
            .arg(gateway)
            .arg(dhcp ? "true" : "false"));
}

// From FlexRadio wiki spec: save (persists config; amp will reboot after ack)
quint32 PgxlConnection::save() {
    return sendCommand("save");
}

// setBand: sends band via the paired flexradio path if paired, else no-op.
// From FlexRadio wiki spec: flexradio ampslice=<A|B|C|D> serial=<radio_serial> band=<hz>
// Phase 3P-II Task 66: full implementation.
// Returns 0 (no-op) if pairing is still in flight or serial is not set.
quint32 PgxlConnection::setBand(int bandHz) {
    // m_pendingPairingSeq is non-zero while the R-frame ack has not arrived yet.
    // m_pairedRadioSerial is empty until flexradioPair() captures it.
    if (m_pendingPairingSeq != 0 || m_pairedRadioSerial.isEmpty()) {
        return 0;
    }
    auto& s = AppSettings::instance();
    QChar slice = s.value(QStringLiteral("PGXL_FlexAmpSlice"),
                          QStringLiteral("A")).toString().at(0);
    // From FlexRadio PowerGenius Ethernet API wiki spec (design §6.4).
    return sendCommand(
        QString("flexradio ampslice=%1 serial=%2 band=%3")
            .arg(slice)
            .arg(m_pairedRadioSerial)
            .arg(bandHz));
}

void PgxlConnection::onKeepaliveTimeout() {
    if (m_connected) {
        sendCommand("status");
    }
}

void PgxlConnection::onPingTimeoutCheck() {
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QList<quint32> stale;
    for (auto it = m_pendingPings.cbegin(); it != m_pendingPings.cend(); ++it) {
        if (nowMs - it.value().sentMs > 5000) {
            stale << it.key();
        }
    }
    for (quint32 seq : stale) {
        emit pingTimedOut(seq);
        m_pendingPings.remove(seq);
    }
}

void PgxlConnection::scheduleReconnect() {
    auto& s = AppSettings::instance();
    if (s.value("PGXL_AutoReconnect", "True").toString() != "True") { return; }
    if (m_lastHost.isEmpty()) { return; }

    int idx = std::min(m_reconnectAttempts, int(std::size(kBackoffSec)) - 1);
    int delayMs = kBackoffSec[idx] * 1000;
    // Increment attempt counter synchronously so the next call to
    // scheduleReconnect() (or testForceDisconnect()) uses the updated index.
    // The singleShot only fires the actual socket reconnect.
    ++m_reconnectAttempts;
    emit reconnectAttempt(m_reconnectAttempts, delayMs);
    QString host = m_lastHost;
    quint16 port = m_lastPort;
    QTimer::singleShot(delayMs, this, [this, host, port] {
        m_socket.connectToHost(host, port);
    });
}

void PgxlConnection::testForceDisconnect() {
    m_connected = false;
    scheduleReconnect();
}

void PgxlConnection::testFlushPingTimeouts() {
    QList<quint32> allSeqs = m_pendingPings.keys();
    for (quint32 seq : allSeqs) {
        emit pingTimedOut(seq);
        m_pendingPings.remove(seq);
    }
}

void PgxlConnection::processLine(const QString& line) {
    // Version: V3.8.9
    if (!m_gotVersion && line.startsWith('V')) {
        m_version = line.mid(1);
        m_gotVersion = true;
        qCInfo(lcPgxl) << "PGXL version" << m_version;
        // Phase 3P-II bench-diagnostic logging (remove after pairing protocol confirmed)
        qCInfo(lcPgxl) << "RX V-frame version=" << m_version;
        sendCommand("info");
        sendCommand("status");
        m_connected = true;
        m_pollTimer.start();
        emit connected();
        return;
    }
    // Response: R<seq>|<hex>|<body>
    if (line.startsWith('R')) {
        int pipe1 = line.indexOf('|');
        int pipe2 = (pipe1 >= 0) ? line.indexOf('|', pipe1 + 1) : -1;
        if (pipe2 >= 0) {
            // Parse the sequence number from R<seq>.
            quint32 rseq = line.mid(1, pipe1 - 1).toUInt();
            // Parse the hex status code.
            QString hexStr = line.mid(pipe1 + 1, pipe2 - pipe1 - 1).trimmed();
            bool hexOk = false;
            uint hexCode = hexStr.toUInt(&hexOk, 16);

            QString body = line.mid(pipe2 + 1).trimmed();

            // Phase 3P-II bench-diagnostic logging (remove after pairing protocol confirmed)
            if (hexOk && hexCode != 0) {
                qCWarning(lcPgxl) << "RX R-frame seq=" << rseq << "hex=" << QString::number(hexCode, 16) << "body:" << body;
            } else {
                qCInfo(lcPgxl) << "RX R-frame seq=" << rseq << "hex=" << (hexOk ? QString::number(hexCode, 16) : "PARSE_ERROR") << "body:" << body;
            }

            // Check for pairing result correlation.
            if (m_pendingPairingSeq != 0 && rseq == m_pendingPairingSeq) {
                bool succeeded = (hexOk && hexCode == 0);
                // Phase 3P-II Task 66: if pairing failed, clear the serial so
                // setBand() stays a no-op until a new successful pair arrives.
                if (!succeeded) {
                    m_pairedRadioSerial.clear();
                }
                emit pairingResult(succeeded, body);
                m_pendingPairingSeq = 0;
            }

            // Check for pong correlation (ping response: R<seq>|0|).
            if (m_pendingPings.contains(rseq)) {
                PendingPing pp = m_pendingPings.take(rseq);
                qint64 rttMs = QDateTime::currentMSecsSinceEpoch() - pp.sentMs;
                emit pongReceived(rseq, rttMs, pp.tag);
            }

            // Check for save ack (R<seq>|0|saving).
            if (hexOk && hexCode == 0 && body == "saving") {
                emit saveAcknowledged();
            }

            // Emit general status for kv body (setup/ifconf responses land here too).
            if (!body.isEmpty()) {
                QMap<QString,QString> kvs;
                const auto parts = body.split(' ', Qt::SkipEmptyParts);
                for (const auto& p : parts) {
                    int eq = p.indexOf('=');
                    if (eq > 0) { kvs.insert(p.left(eq), p.mid(eq + 1)); }
                }
                if (!kvs.isEmpty()) { emit statusUpdated(kvs); }
            }
        }
        return;
    }
    // Status push: S0|<object> <kv> ... (PGXL may push unsolicited status).
    // Frame format per FlexRadio PowerGenius Ethernet API + design §6.1:
    // the <object> prefix is required; drop frames that lack it.
    if (line.startsWith('S')) {
        int pipe = line.indexOf('|');
        if (pipe < 0) return;

        QString rest = line.mid(pipe + 1);
        int firstEq = rest.indexOf('=');
        if (firstEq < 0) return;
        int lastSpaceBeforeEq = rest.lastIndexOf(' ', firstEq);
        if (lastSpaceBeforeEq < 0) return;

        QString kvString = rest.mid(lastSpaceBeforeEq + 1);
        QMap<QString, QString> kvs;
        const auto parts = kvString.split(' ', Qt::SkipEmptyParts);
        for (const auto& part : parts) {
            int eq = part.indexOf('=');
            if (eq > 0)
                kvs.insert(part.left(eq), part.mid(eq + 1));
        }
        // Phase 3P-II bench-diagnostic logging (remove after pairing protocol confirmed)
        if (!kvs.isEmpty()) {
            qCInfo(lcPgxl) << "RX S-frame state=" << kvs.value("state", "unknown");
        }
        if (!kvs.isEmpty())
            emit statusUpdated(kvs);
        return;
    }
}

}  // namespace NereusSDR
