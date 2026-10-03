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
//   2026-09-23  configuredHost()/configuredPort() for the Core's
//                 `amplifier` object (R-R3-47). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (R-R3-47, R-R3-22): the Tuner Genius connection's
//                 lifecycle (TgxlConnection.h, 2026-09-21/22 entries): an
//                 owned, generation-checked retry timer replaces the static
//                 single-shot; every physical dial gets a fresh socket; an
//                 opt-in identity admission (native `info` serial, held
//                 until the Core's controller approves it) for the Core.
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (R-R3-47, R-R3-22): replyReceived, every answer of an
//                 admitted amp by sequence, for the Core's device settings.
//   2026-09-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (iPhone app plan Task 77 fix round 2; R-IOS-02, R-IOS-03,
//                 R-IOS-13): operateCommanded, every operate=0 or operate=1
//                 written, so the Core knows the amp is changing over.
//   2026-09-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Task 77 fix round 3): operateCommanded carries the
//                 command's sequence.
//   2026-09-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Task 77 fix round 4): replyRefused.
//   2026-09-30: Fix wave RD-I11: writeSetup refuses a key or value with
//               a space or '=' (isSetupToken), so a name cannot add
//               setup fields. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: Fix round 1: asSetupToken offers a name saved with
//               spaces as one word. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
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

// R-R3-47: the amp's own answer to `info`, for the Core's identity check.
// Captured from a real PGXL (captures/flex-tgxl-direct-CONTROL.pcapng,
// C30698/C206): `R<seq>|0|serial=10-200/24-0046  version=3.8.9
// protocol=1.0 mains=240`. No model and no nickname in this reply.
struct PgxlIdentityInfo {
    quint64 socketAttemptToken{0};
    QString peerAddress;
    quint16 peerPort{0};
    QString serial;
    QString version;
};

class PgxlConnection : public QObject {
    Q_OBJECT
public:
    explicit PgxlConnection(QObject* parent = nullptr);
    ~PgxlConnection() override;

    bool    isConnected() const { return m_connected; }
    QString version()     const { return m_version; }
    QString peerAddress() const { return m_socket->peerAddress().toString(); }
    quint16 peerPort()    const { return m_socket->peerPort(); }
    quint64 socketAttemptToken() const { return m_socketAttemptGeneration; }
    bool reconnectPending() const noexcept { return m_reconnectTimer.isActive(); }
    bool identityAdmissionRequired() const { return m_identityAdmissionRequired; }
    PgxlIdentityInfo identityInfo() const { return m_identityInfo; }

    // R-R3-47: the Core's identity boundary, off by default so a local
    // window's V-banner handshake is unchanged. When on, a V banner only
    // starts identification: `info` is sent, nothing else is sent or
    // published, and the connection counts as connected only once the
    // owner calls admitIdentity() for this physical attempt.
    void setIdentityAdmissionRequired(bool required);
    bool admitIdentity(quint64 socketAttemptToken, const QString& expectedSerial);
    bool rejectIdentity(quint64 socketAttemptToken, const QString& reason);

    // R-R3-47: connection settings applied while running. Re-reads
    // PGXL_AutoReconnect (a pending retry is dropped when it is off) and
    // PGXL_KeepaliveSec (a running keepalive takes the new interval).
    void applyConnectionSettings();
    // Periodic `ping` while connected, every `seconds`; 0 turns it off.
    void setAutoPingIntervalSec(int seconds);
    int autoPingIntervalSec() const { return m_autoPingSec; }
    // R-R3-47: the address and port last dialled (connectToPgxl), for the
    // Core's `amplifier` object. Empty and 9008 before any dial.
    QString configuredHost() const { return m_lastHost; }
    quint16 configuredPort() const { return m_lastPort; }

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
    void injectLineForTesting(const QString& line) { processLine(line, 0); }

    // Test-only: simulate a disconnect without a real socket drop.
    // Used by tst_pgxl_connection_reconnect. Production code never calls this.
    void testForceDisconnect();

    // Test-only: returns true if the keepalive timer is active.
    bool testKeepaliveTimerActive() const { return m_keepaliveTimer.isActive(); }

    // Test-only: flush all pending pings as timed-out without waiting 5 s.
    void testFlushPingTimeouts();

    // Test-only lifecycle seams, as TgxlConnection's: compress the
    // 1/2/5/10/30/60-second policy and expose the owned-timer state.
    void testSetReconnectBackoffUnitMs(int ms) {
        m_reconnectBackoffUnitMs = ms > 0 ? ms : 1;
    }
    bool testReconnectPending() const { return reconnectPending(); }
    void testForceSourceBindFailureOnce() { m_testForceSourceBindFailure = true; }
    quint64 testActiveSocketAttemptGeneration() const {
        return m_socketAttemptGeneration;
    }
    QTcpSocket* testSocketForTesting() const { return m_socket; }
    void testInjectLineForSocketAttempt(const QString& line, quint64 attemptGeneration);
    void testInjectFailureForSocketAttempt(quint64 attemptGeneration);
    void testSetIdentityTimeoutMs(int ms) { m_identityTimeoutMs = ms > 0 ? ms : 1; }
    bool testAutoPingActive() const { return m_pingTimer.isActive(); }

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

    // True when `text` can go out as one key or value of a `setup` line:
    // no whitespace, no '=', no control characters. writeSetup refuses
    // a field that fails this, and the device name is held to it.
    static bool isSetupToken(const QString& text);
    // Fix round 1: a name saved before isSetupToken, offered as one word:
    // trimmed, each run of spaces, '=' or control characters made one '_'.
    static QString asSetupToken(const QString& text);
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
    /// R-R3-47 / R-R3-22: every R-frame answer once the amp is connected
    /// (admitted, on the Core): its sequence, whether its code was 0, and
    /// its body. The Core's device settings match their requests by it.
    void replyReceived(quint32 seq, bool accepted, const QString& body);
    /// iPhone app plan Task 77 fix round 4: a reply whose code parses and
    /// is not zero (the amp refused command `seq`), after replyReceived.
    void replyRefused(quint32 seq);

    // R-R3-47 identity admission (only with setIdentityAdmissionRequired).
    void identityProtocolProgress(quint64 socketAttemptToken,
                                  const QString& peerAddress,
                                  quint16 peerPort,
                                  const QString& version);
    void nativeInfoReceived(const NereusSDR::PgxlIdentityInfo& info);
    void identityAdmissionFailed(quint64 socketAttemptToken, const QString& reason);

    /// iPhone app plan Task 77 fix round 2: an operate=1 (`operate` true)
    /// or operate=0 was written to the amp, by any sender. The amp is
    /// changing over until its status reports the commanded state.
    /// Task 77 fix round 3: `seq` is the command's sequence, so an error
    /// reply to it can end the changeover.
    void operateCommanded(bool operate, quint32 seq);

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
    void onAutoPing();

private:
    enum class SourceBindResult { NotRequested, Bound, Failed };

    void onConnected(quint64 attemptGeneration);
    void onDisconnected(quint64 attemptGeneration);
    void onReadyRead(quint64 attemptGeneration);
    void onError(quint64 attemptGeneration);
    void processLine(const QString& line, quint64 attemptGeneration = 0);
    void queueDial(const QString& host, quint16 port, quint64 generation);
    bool requestIsCurrent(const QString& host, quint16 port, quint64 generation) const;
    bool socketAttemptIsCurrent(quint64 attemptGeneration) const;
    void beginSocketAttempt(quint64 endpointGeneration);
    void retireSocketAttempt();
    void clearIdentityAttempt();
    void clearPairing();
    void failIdentityAdmission(quint64 socketAttemptToken, const QString& reason);
    quint32 writeProtocolCommand(const QString& cmd);

    // 2026-05-26 KG4VCF: probe the kernel for the local source IP it
    // would use to reach `host` and explicitly bind m_socket to that
    // address.  Called for every physical dial (initial and retry) so a
    // topology change between drops is picked up.  No-op if the probe
    // fails -- falls through to OS default routing.
    SourceBindResult bindSourceForHost(const QString& host);

    // R-R3-47: one QTcpSocket per physical dial (TgxlConnection's fix for
    // the invalid-descriptor errors a reused socket gave after a timeout).
    QTcpSocket* m_socket{nullptr};
    QTimer     m_pollTimer;
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    // PR #279 review #3 (2026-05-23): set true by disconnect() (user-
    // initiated path), checked by onDisconnected() before calling
    // scheduleReconnect().  Cleared by connectToPgxl() on the next
    // intentional connect so a manual reconnect re-arms auto-reconnect.
    // Without this flag, the Peripherals Disconnect button could not
    // keep PGXL disconnected because PGXL_AutoReconnect defaults true
    // and scheduleReconnect() fires unconditionally from onDisconnected.
    bool       m_userInitiatedDisconnect{false};
    QString    m_version;

    // Tier 2 state.
    QTimer  m_keepaliveTimer;
    QTimer  m_pingTimer;         // periodic auto-ping (PGXL_PingSec); wired in Task 67.
    QTimer  m_pingTimeoutTimer;
    QTimer  m_reconnectTimer;
    QTimer  m_connectTimer;
    QTimer  m_identityTimer;
    int     m_reconnectBackoffUnitMs{1000};
    int     m_reconnectAttempts{0};
    // R-R3-47: endpoint and physical-attempt generations (TgxlConnection's
    // pattern). A retry, a queued dial or a socket callback belonging to a
    // replaced or cancelled endpoint can never act.
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
    int     m_autoPingSec{0};
    bool    m_testForceSourceBindFailure{false};
    bool    m_identityAdmissionRequired{false};
    int     m_identityTimeoutMs{5000};
    PgxlIdentityInfo m_identityInfo;
    QMap<QString, QString> m_identityStatus;
    quint32 m_pendingIdentityInfoSeq{0};
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::PgxlIdentityInfo)
