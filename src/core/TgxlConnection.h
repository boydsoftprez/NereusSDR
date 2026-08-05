// =================================================================
// src/core/TgxlConnection.h  (NereusSDR)
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
#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QString>

namespace NereusSDR {

// Direct TCP connection to a 4O3A Tuner Genius XL on port 9010.
// Provides manual relay control (C1/L/C2) via the TGXL's native protocol,
// which is independent of the FlexRadio on port 4992.
//
// Protocol format (same style as SmartSDR):
//   C<seq>|<command>\n          -- client command
//   R<seq>|<code>|<body>\n      -- TGXL response
//   S0|state key=val ...\n      -- unsolicited state push
//   V<version>\n                -- version line on connect
//
// Reverse-engineered from 4O3A TGXL management app pcap (#469).
class TgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit TgxlConnection(QObject* parent = nullptr);

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
    // Used by tst_tgxl_connection_parse. Production code never calls this.
    void injectLineForTesting(const QString& line) { processLine(line); }

    // Test-only: simulate a disconnect without a real socket drop.
    // Used by future TgxlConnection reconnect tests. Production code never
    // calls this.
    void testForceDisconnect();

    // Test-only: returns true if the keepalive timer is active.
    bool testKeepaliveTimerActive() const { return m_keepaliveTimer.isActive(); }

    // Test-only: flush all pending pings as timed-out without waiting 5 s.
    void testFlushPingTimeouts();

public slots:
    void connectToTgxl(const QString& host, quint16 port = 9010);
    void disconnect();
    void adjustRelay(int relay, int direction);
    quint32 sendCommand(const QString& cmd);

    // Tier 2 NereusSDR-native command surface.
    // Wire formats from design §4.2.1 + §6.4 (4O3A TGXL Ethernet API).
    // No amplifierCreate / flexradioPair: TGXL is a tuner, not an amplifier.
    quint32 enableKeepalive();
    quint32 ping(const QString& tag = "");
    quint32 readSetup();
    quint32 writeSetup(const QMap<QString,QString>& fields);
    quint32 readIfconf();
    quint32 writeIfconf(const QString& ip, const QString& netmask,
                        const QString& gateway, bool dhcp);
    quint32 save();

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    void stateUpdated(const QMap<QString, QString>& kvs);
    void statusUpdated(const QMap<QString, QString>& kvs);

    // Tier 2 response signals.
    void pongReceived(quint32 seq, qint64 rttMs, const QString& tag);
    void pingTimedOut(quint32 seq);
    void setupResponse(const QMap<QString,QString>& fields);
    void ifconfResponse(const QMap<QString,QString>& fields);
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

    // 2026-05-26 KG4VCF: probe the kernel for the local source IP
    // it would use to reach `host` and explicitly bind m_socket to
    // that address.  Called both from connectToTgxl() (initial) and
    // from scheduleReconnect() (auto-reconnect) so a topology change
    // between drops and the next attempt is picked up.  No-op if
    // host is invalid or the probe fails -- falls through to the
    // OS default route in that case.
    void bindSourceForHost(const QString& host);

    QTcpSocket m_socket;
    QTimer     m_pollTimer;       // 1/sec status poll
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    // PR #279 review #3 (2026-05-23): set true by disconnect() so
    // onDisconnected() suppresses the reconnect.  Cleared by
    // connectToTgxl() so a fresh manual connect re-arms auto-reconnect.
    bool       m_userInitiatedDisconnect{false};
    QString    m_version;

    // Tier 2 state.
    QTimer  m_keepaliveTimer;
    QTimer  m_pingTimer;          // periodic auto-ping (TGXL_PingSec); wired in Task 67.
    QTimer  m_pingTimeoutTimer;
    QTimer  m_reconnectTimer;
    int     m_reconnectAttempts{0};
    // Dedup window for scheduleReconnect: when Qt fires BOTH errorOccurred
    // and disconnected for the same socket failure (happens on most
    // connect-time errors), we'd otherwise schedule two retries back-to-
    // back which race and waste an attempt. This timestamp is set every
    // time scheduleReconnect is invoked; if called again within
    // kReconnectDedupMs ms, the second call is treated as a duplicate
    // and ignored. Bench-confirmed 2026-05-20 18:19:25 (two scheduleR-
    // econnect calls in the same millisecond from onError + onDisc).
    qint64  m_lastReconnectScheduleMs{0};
    int     m_keepaliveMissed{0};
    quint32 m_pendingSetupSeq{0};
    quint32 m_pendingIfconfSeq{0};
    struct PendingPing { quint32 seq; qint64 sentMs; QString tag; };
    QHash<quint32, PendingPing> m_pendingPings;
    qint64  m_lastFrameMs{0};
    qint64  m_connectedSinceMs{0};
    quint64 m_framesIn{0}, m_framesOut{0}, m_bytesIn{0}, m_bytesOut{0};
    QString m_lastHost;
    quint16 m_lastPort{9010};
};

}  // namespace NereusSDR
