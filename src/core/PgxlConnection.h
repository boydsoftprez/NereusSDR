// =================================================================
// src/core/PgxlConnection.h  (NereusSDR)
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
// =================================================================
#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QString>

namespace NereusSDR {

class PgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit PgxlConnection(QObject* parent = nullptr);

    bool    isConnected() const { return m_connected; }
    QString version()     const { return m_version; }
    QString peerAddress() const { return m_socket.peerAddress().toString(); }
    quint16 peerPort()    const { return m_socket.peerPort(); }

    // Read-only metric accessors for ConnectionDiagnostics polling (1 Hz).
    quint64 framesIn()         const noexcept { return m_framesIn; }
    quint64 framesOut()        const noexcept { return m_framesOut; }
    quint64 bytesIn()          const noexcept { return m_bytesIn; }
    quint64 bytesOut()         const noexcept { return m_bytesOut; }
    qint64  connectedSinceMs() const noexcept { return m_connectedSinceMs; }
    qint64  lastFrameMs()      const noexcept { return m_lastFrameMs; }
    int     keepaliveMissed()  const noexcept { return m_keepaliveMissed; }
    int     reconnectCount()   const noexcept { return m_reconnectAttempts; }

    // Test-only: feed a single line into processLine (no newline needed).
    // Used by tst_pgxl_connection_parse. Production code never calls this.
    void injectLineForTesting(const QString& line) { processLine(line); }

    // Test-only: simulate a disconnect without a real socket drop.
    // Used by tst_pgxl_connection_reconnect. Production code never calls this.
    void testForceDisconnect();

    // Test-only: returns true if the keepalive timer is active.
    bool testKeepaliveTimerActive() const { return m_keepaliveTimer.isActive(); }

    // Test-only: flush all pending pings as timed-out without waiting 5 s.
    void testFlushPingTimeouts();

public slots:
    void connectToPgxl(const QString& host, quint16 port = 9008);
    void disconnect();
    quint32 sendCommand(const QString& cmd);

    // Tier 2 NereusSDR-native command surface.
    // From FlexRadio PowerGenius Ethernet API wiki spec (see design §6.4).
    quint32 amplifierCreate(const QString& ourSerial,
                            const QString& ourModel = "NereusSDR",
                            const QString& antMap = "ANT1:PORTA,ANT2:PORTB");
    quint32 flexradioPair(QChar ampSlice,
                          const QString& radioSerial,
                          const QString& txAnt,
                          bool pttOverLan = true,
                          bool active = true);
    quint32 enableKeepalive();
    quint32 ping(const QString& tag = "");
    quint32 interlockCreate(const QString& validAntennas,
                            const QString& name,
                            const QString& serial);
    quint32 interlockDisable(int interlockId);
    quint32 readSetup();
    quint32 writeSetup(const QMap<QString,QString>& fields);
    quint32 readIfconf();
    quint32 writeIfconf(const QString& ip, const QString& netmask,
                        const QString& gateway, bool dhcp);
    quint32 save();
    quint32 setBand(int bandHz);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    void statusUpdated(const QMap<QString, QString>& kvs);

    // Tier 2 response signals.
    void pongReceived(quint32 seq, qint64 rttMs, const QString& tag);
    void pingTimedOut(quint32 seq);
    void setupResponse(const QMap<QString,QString>& fields);
    void ifconfResponse(const QMap<QString,QString>& fields);
    void pairingResult(bool succeeded, const QString& detail);
    void saveAcknowledged();
    void reconnectAttempt(int attemptNumber, int backoffMs);

    // Test seam: emitted from sendCommand so tests can assert frame format.
    void testFrameWrittenForTesting(const QString& frame);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError();
    void pollStatus();
    void onKeepaliveTimeout();
    void onPingTimeoutCheck();
    void scheduleReconnect();

private:
    void processLine(const QString& line);

    QTcpSocket m_socket;
    QTimer     m_pollTimer;
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    QString    m_version;

    // Tier 2 state.
    QTimer  m_keepaliveTimer;
    QTimer  m_pingTimer;         // periodic auto-ping (PGXL_PingSec); wired in Task 67.
    QTimer  m_pingTimeoutTimer;
    QTimer  m_reconnectTimer;
    int     m_reconnectAttempts{0};
    int     m_keepaliveMissed{0};
    quint32 m_pendingPairingSeq{0};
    // Phase 3P-II Task 66: serial captured from flexradioPair() so setBand()
    // can send "flexradio ampslice=<x> serial=<serial> band=<hz>".
    // Cleared on disconnect or pairing failure.
    QString m_pairedRadioSerial;
    struct PendingPing { quint32 seq; qint64 sentMs; QString tag; };
    QHash<quint32, PendingPing> m_pendingPings;
    qint64  m_lastFrameMs{0};
    qint64  m_connectedSinceMs{0};
    quint64 m_framesIn{0}, m_framesOut{0}, m_bytesIn{0}, m_bytesOut{0};
    QString m_lastHost;
    quint16 m_lastPort{9008};
};

}  // namespace NereusSDR
