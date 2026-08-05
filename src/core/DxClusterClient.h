// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DX cluster telnet client (DX Spider / AR-Cluster / CC-Cluster)
//
// Ported from AetherSDR src/core/DxClusterClient.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B3. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". DxSpot include
//                                    moved to the extracted DxSpot.h
//                                    (Phase 3J-2 Task B1) instead of
//                                    redefining DxSpot inline (upstream
//                                    DxClusterClient.h:13-23 defined it).
//                                    Logging routes through lcSpots
//                                    ("nereus.spots") instead of upstream
//                                    AetherSDR's lcDxCluster. Log file
//                                    path uses Qt's AppConfigLocation
//                                    (already lands under NereusSDR/)
//                                    instead of upstream's
//                                    GenericConfigLocation +
//                                    "AetherSDR/dxcluster.log". Added
//                                    public test seams
//                                    parseDxSpotLineForTest(),
//                                    isLoginPromptForTest(),
//                                    stripTelnetIACForTest() so unit
//                                    tests can validate the parser, login
//                                    prompt detector, and telnet IAC
//                                    stripper without instantiating a
//                                    QTcpSocket or simulating a telnet
//                                    server. NereusSDR addition: source-
//                                    label assignment in parseDxSpotLine
//                                    defaults to "Cluster" and promotes
//                                    to "RBN" when the spotter callsign
//                                    starts with "RBN-" (case-insensitive)
//                                    or ends with "-#". This lets a
//                                    single class instance serve both the
//                                    DX cluster and RBN connections
//                                    (RadioModel instantiates two
//                                    DxClusterClient objects for the two
//                                    sources). AI tooling: Anthropic
//                                    Claude Code.

#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QString>
#include <QTime>
#include <QByteArray>
#include <atomic>

#include "DxSpot.h"

namespace NereusSDR {

// From AetherSDR src/core/DxClusterClient.h:25-83 [@0cd4559]
//
// Telnet client for DX cluster nodes (DX Spider, AR-Cluster, CC Cluster).
// Connects, logs in with callsign, parses "DX de" spot lines, and emits
// spotReceived() for each parsed spot.
class DxClusterClient : public QObject {
    Q_OBJECT

public:
    explicit DxClusterClient(QObject* parent = nullptr);
    ~DxClusterClient() override;

    void connectToCluster(const QString& host, quint16 port, const QString& callsign);
    void disconnect();
    bool isConnected() const { return m_connected; }

    void sendCommand(const QString& cmd);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    QString logFilePath() const;
    void setLogFileName(const QString& name) { m_logFileName = name; }

    // Public test seams. Same body as the private impls; exists so unit
    // tests can validate the regex, login-prompt detector, and telnet IAC
    // stripper without instantiating a QTcpSocket or simulating a telnet
    // server.
    bool parseDxSpotLineForTest(const QString& line, DxSpot& spot) const {
        return parseDxSpotLine(line, spot);
    }
    bool isLoginPromptForTest(const QString& line) const {
        return isLoginPrompt(line);
    }
    void stripTelnetIACForTest(QByteArray& buf) const {
        stripTelnetIACBuffer(buf);
    }

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void spotReceived(const DxSpot& spot);
    void rawLineReceived(const QString& line);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onReconnectTimer();

private:
    bool parseDxSpotLine(const QString& line, DxSpot& spot) const;
    bool isLoginPrompt(const QString& line) const;
    void handleLine(const QString& line);
    void stripTelnetIAC();

    // NereusSDR addition: pure form of stripTelnetIAC() that operates on
    // an externally-supplied buffer. The instance method stripTelnetIAC()
    // is a thin wrapper that calls this on m_readBuffer. Existing as a
    // standalone helper makes the algorithm unit-testable without a live
    // socket.
    static void stripTelnetIACBuffer(QByteArray& buf);

    QTcpSocket* m_socket;
    QByteArray  m_readBuffer;
    QTimer*     m_reconnectTimer;
    QFile       m_logFile;

    QString m_logFileName{"dxcluster.log"};
    QString m_host;
    quint16 m_port{7300};
    QString m_callsign;
    std::atomic<bool> m_connected{false};
    bool    m_loggedIn{false};
    bool    m_intentionalDisconnect{false};
    int     m_reconnectAttempts{0};

    static constexpr int MaxReconnectDelayMs    = 60000;
    static constexpr int InitialReconnectDelayMs = 5000;
    static constexpr int ConnectTimeoutMs        = 10000;
};

} // namespace NereusSDR
