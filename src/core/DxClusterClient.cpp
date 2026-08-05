// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DX cluster telnet client (implementation)
//
// Ported from AetherSDR src/core/DxClusterClient.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B3. Initial port.
//                                    See DxClusterClient.h for full
//                                    attribution notes. NereusSDR
//                                    refactor: stripTelnetIAC() is split
//                                    into a pure
//                                    stripTelnetIACBuffer(QByteArray&)
//                                    helper plus a thin instance method
//                                    that calls it on m_readBuffer, so
//                                    the public stripTelnetIACForTest
//                                    seam can exercise the algorithm
//                                    without a live socket. NereusSDR
//                                    addition: parseDxSpotLine() now
//                                    assigns spot.source = "Cluster" by
//                                    default and promotes to "RBN" when
//                                    the spotter callsign starts with
//                                    "RBN-" (case-insensitive) or ends
//                                    with "-#" (Reverse Beacon Network
//                                    skimmer marker). Upstream
//                                    AetherSDR's parser left source
//                                    unset; the calling site set it
//                                    based on which connection (cluster
//                                    vs. RBN) emitted the spot. Logic,
//                                    field mapping, regex, and reject
//                                    filter are byte-for-byte from
//                                    upstream. AI tooling: Anthropic
//                                    Claude Code.

#include "DxClusterClient.h"
#include "LogCategories.h"

#include <QRegularExpression>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace NereusSDR {

// From AetherSDR src/core/DxClusterClient.cpp:12-25 [@0cd4559]
DxClusterClient::DxClusterClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected,    this, &DxClusterClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DxClusterClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &DxClusterClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &DxClusterClient::onSocketError);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DxClusterClient::onReconnectTimer);
}

// From AetherSDR src/core/DxClusterClient.cpp:27-34 [@0cd4559]
DxClusterClient::~DxClusterClient()
{
    m_intentionalDisconnect = true;
    m_reconnectTimer->stop();
    m_logFile.close();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

// From AetherSDR src/core/DxClusterClient.cpp:36-40 [@0cd4559]
//
// NereusSDR uses Qt's AppConfigLocation (which already lands under
// "NereusSDR/" when QCoreApplication::organizationName/applicationName
// are set) instead of AetherSDR's GenericConfigLocation +
// "AetherSDR/...". This keeps the file co-located with the rest of
// NereusSDR's per-user state, mirroring the SpotCollectorClient (B1)
// and PotaClient (B2) ports.
QString DxClusterClient::logFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/" + m_logFileName;
}

// From AetherSDR src/core/DxClusterClient.cpp:42-67 [@0cd4559]
void DxClusterClient::connectToCluster(const QString& host, quint16 port, const QString& callsign)
{
    if (m_connected) {
        qCWarning(lcSpots) << "DxClusterClient: already connected";
        return;
    }

    m_host = host;
    m_port = port;
    m_callsign = callsign;
    m_loggedIn = false;
    m_intentionalDisconnect = false;
    m_readBuffer.clear();

    qCInfo(lcSpots) << "DxClusterClient: connecting to" << host << ":" << port
                    << "callsign=" << callsign;
    m_socket->connectToHost(host, port);

    // Connection timeout
    QTimer::singleShot(ConnectTimeoutMs, this, [this] {
        if (!m_connected && m_socket->state() != QAbstractSocket::ConnectedState) {
            qCWarning(lcSpots) << "DxClusterClient: connection timeout";
            m_socket->abort();
            emit connectionError(QStringLiteral("Connection timeout"));
        }
    });
}

// From AetherSDR src/core/DxClusterClient.cpp:69-78 [@0cd4559]
void DxClusterClient::disconnect()
{
    m_intentionalDisconnect = true;
    m_reconnectTimer->stop();
    if (m_connected) {
        m_socket->write("bye\r\n");
        m_socket->flush();
    }
    m_socket->disconnectFromHost();
}

// From AetherSDR src/core/DxClusterClient.cpp:80-89 [@0cd4559]
void DxClusterClient::sendCommand(const QString& cmd)
{
    if (!m_connected) {
        return;
    }
    qCDebug(lcSpots) << "DxClusterClient TX:" << cmd;
    if (m_logFile.isOpen()) {
        m_logFile.write(("> " + cmd + "\n").toUtf8());
        m_logFile.flush();
    }
    m_socket->write((cmd + "\r\n").toLatin1());
}

// ── Socket slots ────────────────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:93-112 [@0cd4559]
void DxClusterClient::onConnected()
{
    qCInfo(lcSpots) << "DxClusterClient: TCP connected to" << m_host;
    m_connected = true;
    m_reconnectAttempts = 0;

    // Open log file (truncate on each new connection)
    m_logFile.close();
    m_logFile.setFileName(logFilePath());
    QDir().mkpath(QFileInfo(m_logFile).absolutePath());
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_logFile.write(QString("--- Connected to %1:%2 at %3 ---\n")
            .arg(m_host).arg(m_port)
            .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss UTC"))
            .toUtf8());
        m_logFile.flush();
    }

    emit connected();
}

// From AetherSDR src/core/DxClusterClient.cpp:114-131 [@0cd4559]
void DxClusterClient::onDisconnected()
{
    qCDebug(lcSpots) << "DxClusterClient: disconnected";
    bool wasConnected = m_connected;
    m_connected = false;
    m_loggedIn = false;

    if (wasConnected) {
        emit disconnected();
    }

    if (!m_intentionalDisconnect) {
        // From AetherSDR src/core/DxClusterClient.cpp:124-128 [@0cd4559] — original
        // pattern is `1 << m_reconnectAttempts`. NereusSDR fix: clamp shift count
        // to avoid signed-int UB. The backoff saturates at MaxReconnectDelayMs
        // well before 30 attempts, so capping has no behavioral effect on a
        // healthy connection.
        const int shiftBits = std::min(m_reconnectAttempts, 30);
        int delay = std::min(InitialReconnectDelayMs * (1 << shiftBits),
                             MaxReconnectDelayMs);
        qCDebug(lcSpots) << "DxClusterClient: reconnecting in" << delay << "ms";
        m_reconnectTimer->start(delay);
        m_reconnectAttempts++;
    }
}

// From AetherSDR src/core/DxClusterClient.cpp:133-138 [@0cd4559]
void DxClusterClient::onSocketError(QAbstractSocket::SocketError /*err*/)
{
    QString msg = m_socket->errorString();
    qCWarning(lcSpots) << "DxClusterClient: socket error:" << msg;
    emit connectionError(msg);
}

// From AetherSDR src/core/DxClusterClient.cpp:140-145 [@0cd4559]
void DxClusterClient::onReconnectTimer()
{
    if (m_intentionalDisconnect) {
        return;
    }
    qCDebug(lcSpots) << "DxClusterClient: attempting reconnect";
    connectToCluster(m_host, m_port, m_callsign);
}

// ── Line-buffered read ──────────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:149-160 [@0cd4559]
//
// NereusSDR refactor: pure form takes the buffer to operate on as an
// argument so the public stripTelnetIACForTest() seam can exercise the
// algorithm without a live socket. The instance method below is a thin
// wrapper that calls this on m_readBuffer. Algorithm (skip 0xFF + 2
// command bytes per IAC sequence) is byte-for-byte from upstream.
void DxClusterClient::stripTelnetIACBuffer(QByteArray& buf)
{
    // Remove telnet IAC sequences (0xFF + command byte + option byte)
    int i = 0;
    while (i < buf.size()) {
        if (static_cast<unsigned char>(buf[i]) == 0xFF && i + 2 < buf.size()) {
            buf.remove(i, 3);
        } else {
            i++;
        }
    }
}

void DxClusterClient::stripTelnetIAC()
{
    stripTelnetIACBuffer(m_readBuffer);
}

// From AetherSDR src/core/DxClusterClient.cpp:162-198 [@0cd4559]
void DxClusterClient::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
    stripTelnetIAC();

    while (true) {
        int idx = m_readBuffer.indexOf('\n');
        if (idx < 0) {
            // No newline yet — check for login prompt (may not end with \n)
            if (!m_loggedIn) {
                QString partial = QString::fromLatin1(m_readBuffer).trimmed();
                qCInfo(lcSpots) << "DxClusterClient: partial buffer (no newline yet)"
                                << "len=" << partial.length()
                                << "tail=" << partial.right(40);
                if (isLoginPrompt(partial)) {
                    qCInfo(lcSpots) << "DxClusterClient: login prompt detected (no newline):" << partial;
                    m_readBuffer.clear();
                    m_socket->write((m_callsign + "\r\n").toLatin1());
                    m_loggedIn = true;
                    qCInfo(lcSpots) << "DxClusterClient: sent callsign" << m_callsign;
                }
            }
            break;
        }

        QString line = QString::fromLatin1(m_readBuffer.left(idx)).trimmed();
        m_readBuffer.remove(0, idx + 1);

        if (line.isEmpty()) {
            continue;
        }

        // Write to log file
        if (m_logFile.isOpen()) {
            m_logFile.write((line + "\n").toUtf8());
            m_logFile.flush();
        }

        emit rawLineReceived(line);
        handleLine(line);
    }
}

// From AetherSDR src/core/DxClusterClient.cpp:200-218 [@0cd4559]
void DxClusterClient::handleLine(const QString& line)
{
    // Login prompt detection (line-based)
    if (!m_loggedIn && isLoginPrompt(line)) {
        qCInfo(lcSpots) << "DxClusterClient: login prompt (line):" << line;
        m_socket->write((m_callsign + "\r\n").toLatin1());
        m_loggedIn = true;
        qCInfo(lcSpots) << "DxClusterClient: sent callsign" << m_callsign;
        return;
    }

    // Try to parse as a DX spot
    DxSpot spot;
    if (parseDxSpotLine(line, spot)) {
        qCDebug(lcSpots) << "DxClusterClient: spot" << spot.dxCall
                 << spot.freqMhz << "MHz de" << spot.spotterCall;
        emit spotReceived(spot);
    }
}

// ── Login prompt detection ──────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:222-232 [@0cd4559]
bool DxClusterClient::isLoginPrompt(const QString& line) const
{
    // DX cluster servers vary: "login:", "call:", "callsign:", "Please enter your call",
    // "your call>", "Enter your callsign"
    QString lower = line.toLower();
    if (lower.endsWith(QStringLiteral("login:"))
        || lower.endsWith(QStringLiteral("call:"))
        || lower.endsWith(QStringLiteral("callsign:"))) {
        return true;
    }
    if (lower.contains(QStringLiteral("enter your call"))
        || lower.contains(QStringLiteral("your call"))) {
        return true;
    }
    return false;
}

// ── DX spot line parser ─────────────────────────────────────────────────────

// From AetherSDR src/core/DxClusterClient.cpp:236-260 [@0cd4559]
//
// Standard format: DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z
// Z is the terminator — ignore any trailing chars (some nodes append BEL/NUL)
//
// NereusSDR divergence (source-label assignment moved into the parser):
// upstream AetherSDR left spot.source unset. The calling site set it
// based on which connection (DX cluster vs. RBN) emitted the spot.
// NereusSDR moves the source assignment into parseDxSpotLine() so unit
// tests that exercise the parser directly (without a live socket) see a
// fully-populated DxSpot, and so a single class instance can serve both
// the DX cluster and RBN connections (RadioModel will instantiate two
// DxClusterClient objects). Default label is "Cluster"; the parser
// promotes to "RBN" when the spotter callsign carries a Reverse Beacon
// Network marker (either the "-#" suffix Skimmer servers append or the
// "RBN-" prefix some clusters use, case-insensitive).
bool DxClusterClient::parseDxSpotLine(const QString& line, DxSpot& spot) const
{
    // Format A — classic "DX de" header (AR-Cluster, some CC-Cluster):
    //   "DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z"
    // From AetherSDR src/core/DxClusterClient.cpp:236-260 [@0cd4559].
    static const QRegularExpression rxClassic(
        R"(^DX\s+de\s+(\S+?):\s+(\d+\.?\d*)\s+(\S+)\s+(.*?)\s+(\d{4})Z)",
        QRegularExpression::CaseInsensitiveOption);

    // Format B — DXSpider standard output (NG7M-1, many other DXSpider
    // nodes; not parsed by upstream AetherSDR):
    //   "14310.0 VK2IO/P     12-May-2026 0449Z WWFF VKFF-5514     <OH0M>"
    // Tokens: FREQ_KHZ CALL DD-MMM-YYYY HHMMZ COMMENT... <SPOTTER>
    // 2026-05-12 bench: NG7M-1 (DXSpider V1.57) is the default cluster
    // host shipped in SpotHubDialog defaults; without this fallback
    // every spot from the default cluster gets dropped on the floor
    // even though the TCP + login path works.
    static const QRegularExpression rxDxspider(
        R"(^(\d+\.?\d*)\s+(\S+)\s+\d{1,2}-\w+-\d{4}\s+(\d{4})Z\s+(.*?)\s+<(\S+)>)",
        QRegularExpression::CaseInsensitiveOption);

    QString freqStr;
    QString timeStr;

    auto match = rxClassic.match(line);
    if (match.hasMatch()) {
        spot.spotterCall = match.captured(1);
        freqStr          = match.captured(2);
        spot.dxCall      = match.captured(3);
        spot.comment     = match.captured(4).trimmed();
        timeStr          = match.captured(5);
    } else {
        match = rxDxspider.match(line);
        if (!match.hasMatch()) {
            return false;
        }
        freqStr          = match.captured(1);
        spot.dxCall      = match.captured(2);
        timeStr          = match.captured(3);
        spot.comment     = match.captured(4).trimmed();
        spot.spotterCall = match.captured(5);
    }

    const double freqKhz = freqStr.toDouble();
    spot.freqMhz = freqKhz / 1000.0;

    const int hh = timeStr.left(2).toInt();
    const int mm = timeStr.mid(2, 2).toInt();
    spot.utcTime = QTime(hh, mm);

    if (!(spot.freqMhz > 0.0 && !spot.dxCall.isEmpty())) {
        return false;
    }

    // NereusSDR addition: source-label assignment.
    spot.source = QStringLiteral("Cluster");
    if (spot.spotterCall.startsWith(QStringLiteral("RBN-"), Qt::CaseInsensitive)
        || spot.spotterCall.endsWith(QStringLiteral("-#"))) {
        spot.source = QStringLiteral("RBN");
    }

    return true;
}

} // namespace NereusSDR
