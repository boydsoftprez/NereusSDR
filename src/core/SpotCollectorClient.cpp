// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DXLab SpotCollector UDP listener (implementation)
//
// Ported from AetherSDR src/core/SpotCollectorClient.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B1. Initial port.
//                                    See SpotCollectorClient.h for full
//                                    attribution notes. NereusSDR
//                                    addition: source-label promotion to
//                                    "RBN" when the spotter callsign has
//                                    a "-#" suffix or "RBN-" prefix
//                                    (Reverse Beacon Network spots
//                                    pushed via SpotCollector). The
//                                    upstream AetherSDR code unconditionally
//                                    set source="SpotCollector".
//                                    AI tooling: Anthropic Claude Code.

#include "SpotCollectorClient.h"
#include "LogCategories.h"

#include <QRegularExpression>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace NereusSDR {

// From AetherSDR SpotCollectorClient.cpp:13-19 [@0cd4559]
SpotCollectorClient::SpotCollectorClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &SpotCollectorClient::onReadyRead);
}

// From AetherSDR SpotCollectorClient.cpp:21-25 [@0cd4559]
SpotCollectorClient::~SpotCollectorClient()
{
    stopListening();
    m_logFile.close();
}

// From AetherSDR SpotCollectorClient.cpp:27-31 [@0cd4559]
// NereusSDR uses Qt's AppConfigLocation (which already lands under
// "NereusSDR/" when QCoreApplication::organizationName/applicationName
// are set) instead of AetherSDR's GenericConfigLocation +
// "AetherSDR/...". This keeps the file co-located with the rest of
// NereusSDR's per-user state.
QString SpotCollectorClient::logFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/spotcollector.log";
}

// From AetherSDR SpotCollectorClient.cpp:33-61 [@0cd4559]
void SpotCollectorClient::startListening(quint16 port)
{
    if (m_listening) {
        return;
    }

    m_port = port;

    qCDebug(lcSpots) << "SpotCollectorClient: binding UDP port" << port;

    if (!m_socket->bind(QHostAddress::AnyIPv4, port,
                        QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        qCWarning(lcSpots) << "SpotCollectorClient: bind failed:" << m_socket->errorString();
        return;
    }

    // Open log file (truncate on each start)
    m_logFile.close();
    m_logFile.setFileName(logFilePath());
    QDir().mkpath(QFileInfo(m_logFile).absolutePath());
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_logFile.write(QString("--- SpotCollector listener started at %1 on port %2 ---\n")
            .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss UTC"))
            .arg(port).toUtf8());
        m_logFile.flush();
    }

    m_listening = true;
    qCDebug(lcSpots) << "SpotCollectorClient: listening on port" << port;
    emit listening();
}

// From AetherSDR SpotCollectorClient.cpp:63-69 [@0cd4559]
void SpotCollectorClient::stopListening()
{
    if (!m_listening) {
        return;
    }
    m_socket->close();
    m_listening = false;
    emit stopped();
}

// From AetherSDR SpotCollectorClient.cpp:73-100 [@0cd4559]
void SpotCollectorClient::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(data.data(), data.size());

        // SpotCollector sends one or more "DX de" lines per datagram
        QString payload = QString::fromLatin1(data);
        const QStringList lines = payload.split('\n', Qt::SkipEmptyParts);
        for (const QString& raw : lines) {
            QString line = raw.trimmed();
            if (line.isEmpty()) {
                continue;
            }

            if (m_logFile.isOpen()) {
                m_logFile.write((line + "\n").toUtf8());
                m_logFile.flush();
            }
            emit rawLineReceived(line);

            DxSpot spot;
            if (parseDxSpotLine(line, spot)) {
                qCDebug(lcSpots) << "SpotCollectorClient: spot" << spot.dxCall
                         << spot.freqMhz << "MHz de" << spot.spotterCall;
                emit spotReceived(spot);
            }
        }
    }
}

// From AetherSDR SpotCollectorClient.cpp:104-126 [@0cd4559]
//
// Standard format: DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z
//
// NereusSDR divergence (source-label assignment moved into the parser):
// upstream AetherSDR sets spot.source = "SpotCollector" in
// onReadyRead() after the parser returns. NereusSDR moves the source
// assignment into parseDxSpotLine() so unit tests that exercise the
// parser directly (without a live UDP socket) see a fully-populated
// DxSpot. The default label is still "SpotCollector"; the parser also
// promotes to "RBN" when the spotter callsign carries a Reverse Beacon
// Network marker (either the "-#" suffix Skimmer servers append or the
// "RBN-" prefix some clusters use).
bool SpotCollectorClient::parseDxSpotLine(const QString& line, DxSpot& spot) const
{
    static const QRegularExpression rx(
        R"(^DX\s+de\s+(\S+?):\s+(\d+\.?\d*)\s+(\S+)\s+(.*?)\s+(\d{4})Z)",
        QRegularExpression::CaseInsensitiveOption);

    auto match = rx.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    spot.spotterCall = match.captured(1);
    double freqKhz   = match.captured(2).toDouble();
    spot.freqMhz     = freqKhz / 1000.0;
    spot.dxCall      = match.captured(3);
    spot.comment     = match.captured(4).trimmed();

    QString timeStr = match.captured(5);
    int hh = timeStr.left(2).toInt();
    int mm = timeStr.mid(2, 2).toInt();
    spot.utcTime = QTime(hh, mm);

    if (!(spot.freqMhz > 0.0 && !spot.dxCall.isEmpty())) {
        return false;
    }

    // NereusSDR addition: source-label assignment.
    spot.source = QStringLiteral("SpotCollector");
    if (spot.spotterCall.endsWith(QStringLiteral("-#"))
        || spot.spotterCall.startsWith(QStringLiteral("RBN-"))) {
        spot.source = QStringLiteral("RBN");
    }

    return true;
}

} // namespace NereusSDR
