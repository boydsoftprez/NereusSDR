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
//   2026-09-21  J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 OpenAI Codex: adopted AetherSDR's owned reconnect-timer
//                 lifecycle [@1e0718ad]. Endpoint/attempt generations,
//                 exponential backoff, and explicit source-bind fallback
//                 are NereusSDR-native.
//   2026-09-21  J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 OpenAI Codex: added opt-in, sequence-correlated native-info
//                 identity admission for Core-owned station connections.
//   2026-09-22  J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 OpenAI Codex: retire each failed Qt socket before retry so
//                 asynchronous timeout state cannot leak into source binding.
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (R-R3-47, R-R3-22): replyReceived, every answer of an
//                 admitted tuner by sequence, for the Core's device settings.
//   2026-09-30  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (TGXL tune lane): messageReceived for the tuner's `M|`
//                 lines, and its tuning flag in the state log line.
//   2026-10-01  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (TGXL tune lane fix round): autotuneSent, every `autotune`
//                 this computer sends the tuner, whoever asked for it.
// =================================================================
#pragma once

#include "core/NereusCoreExport.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QString>

namespace NereusSDR {

struct TgxlIdentityInfo {
    quint64 socketAttemptToken{0};
    QString peerAddress;
    quint16 peerPort{0};
    QString serial;
    QString version;
    QString nickname;
};

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
class NEREUS_CORE_EXPORT TgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit TgxlConnection(QObject* parent = nullptr);
    ~TgxlConnection() override;

    bool    isConnected() const { return m_connected; }
    QString version()     const { return m_version; }
    QString peerAddress() const { return m_socket->peerAddress().toString(); }
    quint16 peerPort()    const { return m_socket->peerPort(); }
    quint64 socketAttemptToken() const { return m_socketAttemptGeneration; }
    bool identityAdmissionRequired() const { return m_identityAdmissionRequired; }
    TgxlIdentityInfo identityInfo() const { return m_identityInfo; }
    bool reconnectPending() const noexcept { return m_reconnectTimer.isActive(); }

    // Opt-in station-owned identity boundary. The default is deliberately
    // false so the established local-direct V-banner handshake is unchanged.
    void setIdentityAdmissionRequired(bool required);
    bool admitIdentity(quint64 socketAttemptToken,
                       const QString& expectedSerial);
    bool rejectIdentity(quint64 socketAttemptToken, const QString& reason);

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
    void injectLineForTesting(const QString& line) { processLine(line, 0); }

    // Test-only: simulate a disconnect without a real socket drop.
    // Used by future TgxlConnection reconnect tests. Production code never
    // calls this.
    void testForceDisconnect();

    // Test-only: returns true if the keepalive timer is active.
    bool testKeepaliveTimerActive() const { return m_keepaliveTimer.isActive(); }

    // Test-only: flush all pending pings as timed-out without waiting 5 s.
    void testFlushPingTimeouts();

    // Test-only reconnect lifecycle seams. They compress the existing
    // 1/2/5/10/30/60-second policy and expose owned-timer state without
    // changing production defaults.
    void testSetReconnectBackoffUnitMs(int ms) {
        m_reconnectBackoffUnitMs = ms > 0 ? ms : 1;
    }
    bool testReconnectPending() const { return reconnectPending(); }
    void testForceSourceBindFailureOnce() { m_testForceSourceBindFailure = true; }
    quint64 testActiveSocketAttemptGeneration() const {
        return m_socketAttemptGeneration;
    }
    QTcpSocket* testSocketForTesting() const { return m_socket; }
    void testInjectLineForSocketAttempt(const QString& line,
                                        quint64 attemptGeneration);
    void testInjectFailureForSocketAttempt(quint64 attemptGeneration);
    void testSetIdentityTimeoutMs(int ms) { m_identityTimeoutMs = ms > 0 ? ms : 1; }

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
    /// R-R3-47 / R-R3-22: every R-frame answer once the tuner is connected
    /// (admitted, on the Core): its sequence, whether its code was 0, and
    /// its body. The Core's device settings match their requests by it.
    void replyReceived(quint32 seq, bool accepted, const QString& body);
    /// TGXL tune lane (bench 2026-09-30): an `M|<text>` message line from
    /// the tuner. captures/flex-tgxl-direct-CONTROL.pcapng shows the tuner
    /// sending `M|Tuned SWR: 1.05:1` at the end of a sweep (T+174.551) and
    /// `M|LOW RF POWER` when it gives a tune up (T+237.750).
    void messageReceived(const QString& text);
    /// TGXL tune lane fix round (2026-10-01): an `autotune` command was
    /// written to the tuner (sequence `seq`), whoever asked for it (the
    /// Tuner page, a device's tune, the band-change recall). The tuner
    /// answers one with its own `transmit tune on` (pcap T+172.199), which
    /// is then never its front-panel TUNE.
    void autotuneSent(quint32 seq);
    void identityProtocolProgress(quint64 socketAttemptToken,
                                  const QString& peerAddress,
                                  quint16 peerPort,
                                  const QString& version);
    void nativeInfoReceived(const NereusSDR::TgxlIdentityInfo& info);
    void identityAdmissionFailed(quint64 socketAttemptToken,
                                 const QString& reason);

    // Test seam: emitted from sendCommand so tests can assert frame format.
    void testFrameWrittenForTesting(const QString& frame);

private slots:
    void pollStatus();
    void onKeepaliveTimeout();
    void onPingTimeoutCheck();
    void scheduleReconnect();
    void onReconnectTimeout();
    void onConnectTimeout();
    void onIdentityTimeout();

private:
    enum class SourceBindResult { NotRequested, Bound, Failed };

    void onConnected(quint64 attemptGeneration);
    void onDisconnected(quint64 attemptGeneration);
    void onReadyRead(quint64 attemptGeneration);
    void onError(quint64 attemptGeneration);
    void processLine(const QString& line, quint64 attemptGeneration);
    void queueDial(const QString& host, quint16 port, quint64 generation);
    bool requestIsCurrent(const QString& host, quint16 port,
                          quint64 generation) const;
    bool socketAttemptIsCurrent(quint64 attemptGeneration) const;
    void beginSocketAttempt(quint64 endpointGeneration);
    void retireSocketAttempt();
    void clearIdentityAttempt();
    void failIdentityAdmission(quint64 socketAttemptToken,
                               const QString& reason);
    quint32 writeProtocolCommand(const QString& cmd);

    // 2026-05-26 KG4VCF: probe the kernel for the local source IP
    // it would use to reach `host` and explicitly bind m_socket to
    // that address.  Called both from connectToTgxl() (initial) and
    // from scheduleReconnect() (auto-reconnect) so a topology change
    // between drops and the next attempt is picked up.  No-op if
    // host is invalid or the probe fails -- falls through to the
    // OS default route in that case.
    SourceBindResult bindSourceForHost(const QString& host);

    QTcpSocket* m_socket{nullptr};
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
    QTimer  m_connectTimer;
    QTimer  m_identityTimer;
    int     m_reconnectBackoffUnitMs{1000};
    int     m_reconnectAttempts{0};
    quint64 m_endpointGeneration{0};
    quint64 m_socketAttemptGeneration{0};
    quint64 m_nextSocketAttemptGeneration{0};
    quint64 m_socketAttemptEndpointGeneration{0};
    quint64 m_retryGeneration{0};
    quint64 m_connectGeneration{0};
    QString m_retryHost;
    quint16 m_retryPort{0};
    QString m_connectHost;
    quint16 m_connectPort{0};
    bool    m_suppressSocketReconnect{false};
    QMetaObject::Connection m_socketConnectedConnection;
    QMetaObject::Connection m_socketDisconnectedConnection;
    QMetaObject::Connection m_socketReadyReadConnection;
    QMetaObject::Connection m_socketErrorConnection;
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
    bool    m_testForceSourceBindFailure{false};
    bool    m_identityAdmissionRequired{false};
    int     m_identityTimeoutMs{5000};
    TgxlIdentityInfo m_identityInfo;
    QMap<QString, QString> m_identityStatus;
    quint32 m_pendingIdentityInfoSeq{0};
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::TgxlIdentityInfo)
