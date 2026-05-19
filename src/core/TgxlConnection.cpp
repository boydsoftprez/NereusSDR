// =================================================================
// src/core/TgxlConnection.cpp  (NereusSDR)
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
//                 Layout from AetherSDR src/core/TgxlConnection.{h,cpp} [@0cd4559].
//   2026-05-19  Tier 2 additions (keepalive/ping/setup r/w/ifconf r/w/save +
//                 auto-reconnect). NereusSDR-native; design §4.2.1 + §6.4.
// =================================================================
#include "TgxlConnection.h"
#include "AppSettings.h"
#include <QDateTime>
#include <QLoggingCategory>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcTgxl, "nereus.tgxl")

// Exponential backoff schedule for auto-reconnect, in seconds.
// Parallel to PgxlConnection (design §6.4); cap at 60 s.
static constexpr int kTgxlBackoffSec[] = {1, 2, 5, 10, 30, 60};

// From AetherSDR src/core/TgxlConnection.cpp:6 [@0cd4559]
TgxlConnection::TgxlConnection(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected,     this, &TgxlConnection::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected,  this, &TgxlConnection::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead,     this, &TgxlConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &TgxlConnection::onError);

    m_pollTimer.setInterval(1000);  // 1 Hz per AetherSDR [@0cd4559]
    connect(&m_pollTimer, &QTimer::timeout, this, &TgxlConnection::pollStatus);

    // Keepalive timer: issues a status poke at the configured interval.
    // Disabled until enableKeepalive() is called.
    m_keepaliveTimer.setSingleShot(false);
    connect(&m_keepaliveTimer, &QTimer::timeout, this, &TgxlConnection::onKeepaliveTimeout);

    // Ping timeout checker: every 5 s, evict pings older than 5 s.
    m_pingTimeoutTimer.setInterval(5000);
    m_pingTimeoutTimer.setSingleShot(false);
    connect(&m_pingTimeoutTimer, &QTimer::timeout, this, &TgxlConnection::onPingTimeoutCheck);
    m_pingTimeoutTimer.start();
}

// From AetherSDR src/core/TgxlConnection.cpp:18 [@0cd4559]
void TgxlConnection::connectToTgxl(const QString& host, quint16 port)
{
    // Idempotent guard: if the socket is in any state other than Unconnected,
    // a connect attempt is already in flight or established.  Re-issuing
    // connectToHost() emits "Trying to connect while connection is in progress".
    // Auto-connect (Task 20) + a manual click can race during the TCP handshake
    // window between connectToHost() and the V-frame arrival that sets m_connected.
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        qCDebug(lcTgxl) << "connectToTgxl: socket already in state"
                        << m_socket.state() << "- ignoring duplicate";
        return;
    }

    // Stash host/port for auto-reconnect.
    m_lastHost = host;
    m_lastPort = port;

    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_connected = false;
    m_gotVersion = false;
    m_version.clear();
    m_readBuf.clear();
    m_seq = 0;
    qCDebug(lcTgxl) << "TgxlConnection: connecting to" << host << ":" << port;
    m_socket.connectToHost(host, port);
}

// From AetherSDR src/core/TgxlConnection.cpp:32 [@0cd4559]
void TgxlConnection::disconnect()
{
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_connected = false;
    m_socket.disconnectFromHost();
}

// From AetherSDR src/core/TgxlConnection.cpp:39 [@0cd4559]
void TgxlConnection::onConnected()
{
    qCDebug(lcTgxl) << "TgxlConnection: TCP connected, waiting for version line";
    m_connectedSinceMs = QDateTime::currentMSecsSinceEpoch();
    m_reconnectAttempts = 0;
    // TGXL sends V<version>\n first, then we send our init commands
}

// From AetherSDR src/core/TgxlConnection.cpp:45 [@0cd4559]
void TgxlConnection::onDisconnected()
{
    qCDebug(lcTgxl) << "TgxlConnection: disconnected";
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_connected = false;
    emit disconnected();
    scheduleReconnect();
}

// From AetherSDR src/core/TgxlConnection.cpp:53 [@0cd4559]
void TgxlConnection::onError()
{
    qCWarning(lcTgxl) << "TgxlConnection: socket error" << m_socket.errorString();
    emit connectionFailed(m_socket.errorString());
}

// From AetherSDR src/core/TgxlConnection.cpp:60 [@0cd4559]
void TgxlConnection::onReadyRead()
{
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

// From AetherSDR src/core/TgxlConnection.cpp:76 [@0cd4559]
void TgxlConnection::processLine(const QString& line)
{
    // Version line: V1.2.17
    if (!m_gotVersion && line.startsWith('V')) {
        m_version = line.mid(1);
        m_gotVersion = true;
        qCDebug(lcTgxl) << "TgxlConnection: TGXL version" << m_version;

        // Send init commands
        sendCommand("info");
        sendCommand("status");

        m_connected = true;
        m_pollTimer.start();
        emit connected();
        return;
    }

    // Response: R<seq>|<code>|<body>
    // Status poll responses contain fwd/swr meter data as KV pairs.
    // Format: R<seq>|0|key=val key=val ...
    if (line.startsWith('R')) {
        // Extract body after second pipe: R<seq>|<code>|<body>
        int pipe1 = line.indexOf('|');
        int pipe2 = (pipe1 >= 0) ? line.indexOf('|', pipe1 + 1) : -1;
        if (pipe2 >= 0) {
            // Parse sequence number and hex status code.
            quint32 rseq = line.mid(1, pipe1 - 1).toUInt();
            QString hexStr = line.mid(pipe1 + 1, pipe2 - pipe1 - 1).trimmed();
            bool hexOk = false;
            uint hexCode = hexStr.toUInt(&hexOk, 16);

            QString body = line.mid(pipe2 + 1).trimmed();

            // Pong correlation: any R-frame matching a pending ping seq is a pong.
            if (m_pendingPings.contains(rseq)) {
                PendingPing pp = m_pendingPings.take(rseq);
                qint64 rttMs = QDateTime::currentMSecsSinceEpoch() - pp.sentMs;
                emit pongReceived(rseq, rttMs, pp.tag);
            }

            // Save acknowledgement: R<seq>|0|saving
            if (hexOk && hexCode == 0 && body == "saving") {
                emit saveAcknowledged();
            }

            // Setup read response correlation.
            if (m_pendingSetupSeq != 0 && rseq == m_pendingSetupSeq) {
                m_pendingSetupSeq = 0;
                if (!body.isEmpty()) {
                    QMap<QString, QString> kvs;
                    const auto parts = body.split(' ', Qt::SkipEmptyParts);
                    for (const auto& part : parts) {
                        int eq = part.indexOf('=');
                        if (eq > 0) { kvs.insert(part.left(eq), part.mid(eq + 1)); }
                    }
                    if (!kvs.isEmpty()) { emit setupResponse(kvs); }
                }
                return;
            }

            // Ifconf read response correlation.
            if (m_pendingIfconfSeq != 0 && rseq == m_pendingIfconfSeq) {
                m_pendingIfconfSeq = 0;
                if (!body.isEmpty()) {
                    QMap<QString, QString> kvs;
                    const auto parts = body.split(' ', Qt::SkipEmptyParts);
                    for (const auto& part : parts) {
                        int eq = part.indexOf('=');
                        if (eq > 0) { kvs.insert(part.left(eq), part.mid(eq + 1)); }
                    }
                    if (!kvs.isEmpty()) { emit ifconfResponse(kvs); }
                }
                return;
            }

            // General KV body: emit statusUpdated (covers poll responses, etc.).
            if (!body.isEmpty()) {
                QMap<QString, QString> kvs;
                const auto parts = body.split(' ', Qt::SkipEmptyParts);
                for (const auto& part : parts) {
                    int eq = part.indexOf('=');
                    if (eq > 0)
                        kvs.insert(part.left(eq), part.mid(eq + 1));
                }
                if (!kvs.isEmpty())
                    emit statusUpdated(kvs);
            }
        }
        return;
    }

    // State push: S0|state key=val key=val ...
    // Status poll response: S<seq>|status key=val key=val ...
    // Frame format per 4O3A TGXL API + design §6.1:
    // the <object> prefix is required; drop frames that lack it.
    if (line.startsWith('S')) {
        int pipe = line.indexOf('|');
        if (pipe < 0) { return; }

        QString rest = line.mid(pipe + 1);

        // Find object name -- everything before first key=val
        // "state bypassA=0 ..." or "status fwd=0.0000 ..."
        int firstEq = rest.indexOf('=');
        if (firstEq < 0) { return; }
        int lastSpaceBeforeEq = rest.lastIndexOf(' ', firstEq);
        if (lastSpaceBeforeEq < 0) { return; }  // <object> prefix required; strict per design §6.1

        QString object   = rest.left(lastSpaceBeforeEq).trimmed();
        QString kvString = rest.mid(lastSpaceBeforeEq + 1);

        // Parse key=value pairs
        QMap<QString, QString> kvs;
        const auto parts = kvString.split(' ', Qt::SkipEmptyParts);
        for (const auto& part : parts) {
            int eq = part.indexOf('=');
            if (eq > 0)
                kvs.insert(part.left(eq), part.mid(eq + 1));
        }

        if (object == "state") {
            emit stateUpdated(kvs);
        } else if (object == "status") {
            emit statusUpdated(kvs);
        }
        return;
    }
}

// From AetherSDR src/core/TgxlConnection.cpp:154 [@0cd4559]
quint32 TgxlConnection::sendCommand(const QString& cmd)
{
    quint32 seq = ++m_seq;
    QString line = QString("C%1|%2\n").arg(seq).arg(cmd);
    m_socket.write(line.toUtf8());
    qCDebug(lcTgxl) << "TgxlConnection: sent" << line.trimmed();
    emit testFrameWrittenForTesting(line.trimmed());  // test seam
    ++m_framesOut;
    m_bytesOut += quint64(line.size());
    return seq;
}

// From AetherSDR src/core/TgxlConnection.cpp:163 [@0cd4559]
void TgxlConnection::adjustRelay(int relay, int direction)
{
    if (!m_connected) { return; }
    if (relay < 0 || relay > 2) { return; }
    int move = (direction > 0) ? 1 : -1;
    sendCommand(QString("tune relay=%1 move=%2").arg(relay).arg(move));
}

// From AetherSDR src/core/TgxlConnection.cpp:171 [@0cd4559]
void TgxlConnection::pollStatus()
{
    if (m_connected) {
        sendCommand("status");
    }
}

// -------------------------------------------------------------------------
// Tier 2 NereusSDR-native command surface.
// Wire formats from design §4.2.1 + §6.4 (4O3A TGXL Ethernet API).
// -------------------------------------------------------------------------

// keepalive enable: instructs TGXL to expect periodic status pokes.
// AppSettings key TGXL_KeepaliveSec (default "30").
quint32 TgxlConnection::enableKeepalive()
{
    quint32 seq = sendCommand("keepalive enable");
    auto& s = AppSettings::instance();
    int intervalSec = s.value("TGXL_KeepaliveSec", "30").toInt();
    m_keepaliveTimer.setInterval(intervalSec * 1000);
    m_keepaliveTimer.start();
    return seq;
}

// ping: no-op roundtrip for RTT measurement. Correlates via R-frame seq.
quint32 TgxlConnection::ping(const QString& tag)
{
    quint32 seq = sendCommand("ping");
    m_pendingPings.insert(seq, PendingPing{seq, QDateTime::currentMSecsSinceEpoch(), tag});
    return seq;
}

// setup read: request current device configuration as KV pairs.
quint32 TgxlConnection::readSetup()
{
    quint32 seq = sendCommand("setup read");
    m_pendingSetupSeq = seq;
    return seq;
}

// setup <k=v ...>: write device configuration fields.
quint32 TgxlConnection::writeSetup(const QMap<QString,QString>& fields)
{
    QStringList parts;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        parts << QString("%1=%2").arg(it.key(), it.value());
    }
    return sendCommand(QString("setup %1").arg(parts.join(' ')));
}

// ifconf read: request current network interface configuration.
quint32 TgxlConnection::readIfconf()
{
    quint32 seq = sendCommand("ifconf read");
    m_pendingIfconfSeq = seq;
    return seq;
}

// ifconf address=<ip> netmask=<mask> gateway=<gw> dhcp=<true|false>
quint32 TgxlConnection::writeIfconf(const QString& ip,
                                    const QString& netmask,
                                    const QString& gateway,
                                    bool dhcp)
{
    return sendCommand(
        QString("ifconf address=%1 netmask=%2 gateway=%3 dhcp=%4")
            .arg(ip)
            .arg(netmask)
            .arg(gateway)
            .arg(dhcp ? "true" : "false"));
}

// save: persist configuration; TGXL will reboot after ack.
quint32 TgxlConnection::save()
{
    return sendCommand("save");
}

void TgxlConnection::onKeepaliveTimeout()
{
    if (m_connected) {
        sendCommand("status");
    }
}

void TgxlConnection::onPingTimeoutCheck()
{
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

void TgxlConnection::scheduleReconnect()
{
    auto& s = AppSettings::instance();
    if (s.value("TGXL_AutoReconnect", "True").toString() != "True") { return; }
    if (m_lastHost.isEmpty()) { return; }

    int idx = std::min(m_reconnectAttempts, int(std::size(kTgxlBackoffSec)) - 1);
    int delayMs = kTgxlBackoffSec[idx] * 1000;
    // Increment attempt counter synchronously so the next call uses the
    // updated index. The singleShot only fires the actual socket reconnect.
    ++m_reconnectAttempts;
    emit reconnectAttempt(m_reconnectAttempts, delayMs);
    QString host = m_lastHost;
    quint16 port = m_lastPort;
    QTimer::singleShot(delayMs, this, [this, host, port] {
        m_socket.connectToHost(host, port);
    });
}

void TgxlConnection::testForceDisconnect()
{
    m_connected = false;
    scheduleReconnect();
}

void TgxlConnection::testFlushPingTimeouts()
{
    QList<quint32> allSeqs = m_pendingPings.keys();
    for (quint32 seq : allSeqs) {
        emit pingTimedOut(seq);
        m_pendingPings.remove(seq);
    }
}

}  // namespace NereusSDR
