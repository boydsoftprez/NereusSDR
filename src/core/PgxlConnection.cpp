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
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (R-R3-47, R-R3-22): TgxlConnection's lifecycle (owned
//                 retry timer, endpoint and attempt generations, a fresh
//                 socket per dial, bind-failure fallback) replaces the
//                 static single-shot retry and the reused socket; opt-in
//                 identity admission for the Core (StationPgxlController).
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//                 iPhone app Part A fix wave (R-IOS-01): the identity
//                 failures a station sends an app as the amplifier's
//                 connection error are in operator words; the detail goes
//                 to the log.
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//                 a connect-time socket failure reaches the connection
//                 error in the Core's own words, not the library's.
//   2026-09-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (R-R3-47, R-R3-22): replyReceived for every answer of a
//                 connected amp (the Core's device settings).
//   2026-09-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (iPhone app plan Task 77 fix round 2): operateCommanded.
//   2026-09-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Task 77 fix round 3): operateCommanded carries its seq.
//   2026-09-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Task 77 fix round 4): replyRefused, a reply whose code
//                 reads and is not zero.
//   2026-09-30: Fix wave RD-I11: writeSetup refuses a key or value with
//               a space or '=' (isSetupToken), so a name cannot add
//               setup fields. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: Fix round 1: asSetupToken offers a name saved with
//               spaces as one word. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================
#include "PgxlConnection.h"
#include "AppSettings.h"
#include "RouteProbe.h"
#include <QDateTime>
#include <QLoggingCategory>
#include <QPointer>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcPgxl, "nereus.pgxl")

// Exponential backoff schedule for auto-reconnect, in seconds.
// From FlexRadio wiki spec + design §6.4: amp keeps connection state;
// NereusSDR must reconnect promptly on blip. Cap at 60 s.
static constexpr int kBackoffSec[] = {1, 2, 5, 10, 30, 60};

PgxlConnection::PgxlConnection(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
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

    // R-R3-47: periodic ping, off until setAutoPingIntervalSec() (the Core).
    m_pingTimer.setSingleShot(false);
    connect(&m_pingTimer, &QTimer::timeout, this, &PgxlConnection::onAutoPing);

    // R-R3-47: TgxlConnection's owned timers (its constructor, 2026-09-21).
    // A pending retry or dial is cancellable and generation-checked, so a
    // replaced or cancelled endpoint can never be redialled.
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &PgxlConnection::onReconnectTimeout);
    m_connectTimer.setSingleShot(true);
    connect(&m_connectTimer, &QTimer::timeout,
            this, &PgxlConnection::onConnectTimeout);
    m_identityTimer.setSingleShot(true);
    connect(&m_identityTimer, &QTimer::timeout,
            this, &PgxlConnection::onIdentityTimeout);
    qRegisterMetaType<PgxlIdentityInfo>();
}

PgxlConnection::~PgxlConnection()
{
    // QAbstractSocket::~QAbstractSocket may emit disconnected(). Quiesce the
    // socket while every member is still alive so destruction cannot
    // re-enter the reconnect machinery.
    m_userInitiatedDisconnect = true;
    ++m_endpointGeneration;
    m_reconnectTimer.stop();
    m_connectTimer.stop();
    m_identityTimer.stop();
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_pingTimer.stop();
    m_pingTimeoutTimer.stop();
    retireSocketAttempt();
    m_socket->abort();
}

void PgxlConnection::setIdentityAdmissionRequired(bool required)
{
    if (m_identityAdmissionRequired == required) {
        return;
    }
    m_identityAdmissionRequired = required;
    // The owner selects the policy before dialling. An attempt that is live
    // when it changes is retired rather than reinterpreted.
    if (m_connected
        || m_socket->state() != QAbstractSocket::UnconnectedState
        || m_connectTimer.isActive()
        || m_reconnectTimer.isActive()
        || m_identityTimer.isActive()) {
        disconnect(); // emits last; a direct observer may delete this object
    } else {
        clearIdentityAttempt();
    }
}

bool PgxlConnection::admitIdentity(quint64 socketAttemptToken,
                                   const QString& expectedSerial)
{
    if (!m_identityAdmissionRequired
        || !socketAttemptIsCurrent(socketAttemptToken)
        || m_identityInfo.socketAttemptToken != socketAttemptToken
        || m_identityInfo.serial.isEmpty()
        || expectedSerial.isEmpty()) {
        return false;
    }
    if (m_identityInfo.serial != expectedSerial) {
        // The station shows this reason to an app as sent, so it is in
        // operator words (iPhone app Part A fix wave, R-IOS-01); the serials
        // go to the log.
        qCInfo(lcPgxl) << "PGXL identity serial mismatch: expected" << expectedSerial
                           << "observed" << m_identityInfo.serial;
        failIdentityAdmission(socketAttemptToken,
            QStringLiteral("The Power Genius at this address is not the one the Core found on its "
                           "network. Check the amplifier's address and port."));
        return false;
    }

    m_identityTimer.stop();
    m_pendingIdentityInfoSeq = 0;
    m_reconnectTimer.stop();
    m_connectTimer.stop();
    m_reconnectAttempts = 0;
    m_retryHost.clear();
    m_retryPort = 0;
    m_connected = true;

    // The info reply reaches statusUpdated only now, as a local window's
    // reply always has, after the Core approved this exact socket.
    const QMap<QString, QString> admittedStatus = m_identityStatus;
    QPointer<PgxlConnection> self(this);
    if (!admittedStatus.isEmpty()) {
        emit statusUpdated(admittedStatus);
        if (!self || !socketAttemptIsCurrent(socketAttemptToken) || !m_connected) {
            return false;
        }
    }
    writeProtocolCommand(QStringLiteral("status"));
    if (!self || !socketAttemptIsCurrent(socketAttemptToken) || !m_connected) {
        return false;
    }
    m_pollTimer.start();
    if (m_autoPingSec > 0) {
        m_pingTimer.start(m_autoPingSec * 1000);
    }
    emit connected();
    return true;
}

bool PgxlConnection::rejectIdentity(quint64 socketAttemptToken, const QString& reason)
{
    if (!m_identityAdmissionRequired || !socketAttemptIsCurrent(socketAttemptToken)) {
        return false;
    }
    failIdentityAdmission(socketAttemptToken,
        reason.isEmpty() ? QStringLiteral("The Core could not confirm that the device at this "
                                          "address is a Power Genius.")
                         : reason);
    return true;
}

void PgxlConnection::applyConnectionSettings()
{
    auto& s = AppSettings::instance();
    if (s.value(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("True")).toString()
            != QStringLiteral("True")
        && m_reconnectTimer.isActive()) {
        qCInfo(lcPgxl) << "applyConnectionSettings: auto-reconnect off; pending retry dropped";
        m_reconnectTimer.stop();
        m_retryHost.clear();
        m_retryPort = 0;
    }
    if (m_keepaliveTimer.isActive()) {
        const int intervalSec = s.value(QStringLiteral("PGXL_KeepaliveSec"),
                                        QStringLiteral("30")).toInt();
        m_keepaliveTimer.setInterval(qMax(1, intervalSec) * 1000);
    }
}

void PgxlConnection::setAutoPingIntervalSec(int seconds)
{
    m_autoPingSec = qMax(0, seconds);
    if (m_autoPingSec == 0) {
        m_pingTimer.stop();
    } else if (m_connected) {
        m_pingTimer.start(m_autoPingSec * 1000);
    }
}

void PgxlConnection::onAutoPing()
{
    if (m_connected) {
        ping(QStringLiteral("auto"));
    }
}

// R-R3-47: the endpoint-replacement semantics of TgxlConnection::
// connectToTgxl (2026-09-21). The same endpoint while in flight is a
// duplicate and ignored; a different one replaces it, cancelling any
// socket, queued dial and captured retry.
void PgxlConnection::connectToPgxl(const QString& host, quint16 port) {
    const bool sameEndpoint = (host == m_lastHost && port == m_lastPort);
    // Idempotent guard: if the socket is in any state other than Unconnected,
    // a connect attempt is already in flight or established.  Re-issuing
    // connectToHost() on the same QTcpSocket emits "Trying to connect while
    // connection is in progress".  Auto-connect (Task 20) + a manual click
    // can race exactly during the handshake window between connectToHost()
    // and the V-frame arrival that flips m_connected = true.
    if (sameEndpoint
        && (m_socket->state() != QAbstractSocket::UnconnectedState
            || m_connectTimer.isActive())) {
        qCDebug(lcPgxl) << "connectToPgxl: socket already in state"
                        << m_socket->state() << "- ignoring duplicate";
        return;
    }

    const bool wasConnected = m_connected;
    ++m_endpointGeneration;
    m_reconnectTimer.stop();
    m_connectTimer.stop();
    retireSocketAttempt();
    m_retryHost.clear();
    m_retryPort = 0;

    m_lastHost = host;
    m_lastPort = port;
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_pingTimer.stop();
    m_connected = false;
    clearPairing();
    m_seq = 0;
    m_gotVersion = false;
    m_version.clear();
    m_readBuf.clear();
    // PR #279 review #3 (2026-05-23): clear the user-initiated flag on
    // every intentional connect so a manual reconnect re-arms
    // auto-reconnect for subsequent network drops.
    m_userInitiatedDisconnect = false;

    // Always cross one event-loop boundary: a socket may not be ready for
    // another connect while errorOccurred is unwinding, and a consumer of
    // connectionFailed or reconnectAttempt may replace the endpoint.
    queueDial(host, port, m_endpointGeneration);
    if (wasConnected) {
        // A's callbacks are retired, so its disconnected() never arrives.
        // Publish the transition; emit last.
        emit disconnected();
    }
}

PgxlConnection::SourceBindResult PgxlConnection::bindSourceForHost(const QString& host)
{
    // 2026-05-26 KG4VCF multi-homed-host source-IP pick.  On a host
    // with overlapping subnets across more than one local interface
    // (e.g. macOS en0 AND a ZeroTier feth/utun overlay both advertising
    // 192.168.x.x), the OS routing picker may bind the socket to an
    // interface that can not actually reach this peer.  Re-probe per
    // connect attempt and source-bind the TCP socket to the kernel's
    // ground-truth source for *this* target.  Independent from TGXL's
    // probe so a host can route to PGXL via one NIC and TGXL via
    // another.
    const QHostAddress targetAddr(host);
    if (targetAddr.isNull()) {
        return SourceBindResult::NotRequested;
    }
    const bool forceBindFailure = m_testForceSourceBindFailure;
    m_testForceSourceBindFailure = false;
    const QHostAddress src = forceBindFailure
        ? QHostAddress(QStringLiteral("192.0.2.123"))
        : probeLocalAddressFor(targetAddr);
    if (src.isNull()) {
        return SourceBindResult::NotRequested;  // no kernel hint; OS default routing
    }
    QPointer<PgxlConnection> self(this);
    const bool bound = m_socket->bind(src, /*port=*/0);
    if (!self) {
        return SourceBindResult::Failed;
    }
    if (bound) {
        qCInfo(lcPgxl) << "source-bound to" << src.toString()
                       << "for target" << host;
        return SourceBindResult::Bound;
    }
    qCWarning(lcPgxl) << "source bind to" << src.toString()
                      << "failed:" << m_socket->errorString()
                      << "-- resetting before OS default routing";
    return SourceBindResult::Failed;
}

bool PgxlConnection::requestIsCurrent(const QString& host, quint16 port,
                                      quint64 generation) const
{
    return generation == m_endpointGeneration
        && host == m_lastHost
        && port == m_lastPort
        && !m_userInitiatedDisconnect;
}

bool PgxlConnection::socketAttemptIsCurrent(quint64 attemptGeneration) const
{
    // Zero is the offline parser seam (injectLineForTesting); never current.
    return attemptGeneration != 0
        && attemptGeneration == m_socketAttemptGeneration
        && m_socketAttemptEndpointGeneration == m_endpointGeneration
        && !m_userInitiatedDisconnect;
}

void PgxlConnection::retireSocketAttempt()
{
    QObject::disconnect(m_socketConnectedConnection);
    QObject::disconnect(m_socketDisconnectedConnection);
    QObject::disconnect(m_socketReadyReadConnection);
    QObject::disconnect(m_socketErrorConnection);
    m_socketConnectedConnection = {};
    m_socketDisconnectedConnection = {};
    m_socketReadyReadConnection = {};
    m_socketErrorConnection = {};
    m_socketAttemptGeneration = 0;
    m_socketAttemptEndpointGeneration = 0;
    clearIdentityAttempt();
}

void PgxlConnection::clearIdentityAttempt()
{
    m_identityTimer.stop();
    m_pendingIdentityInfoSeq = 0;
    m_identityInfo = {};
    m_identityStatus.clear();
}

void PgxlConnection::clearPairing()
{
    // Phase 3P-II Task 66: clear paired serial so setBand() stays silent.
    // R-R3-47: and an unanswered pairing, whose sequence belongs to the old
    // connection, so it cannot block band follow on the next one.
    m_pairedRadioSerial.clear();
    m_pendingPairingSeq = 0;
}

void PgxlConnection::failIdentityAdmission(quint64 socketAttemptToken,
                                           const QString& reason)
{
    if (!m_identityAdmissionRequired || !socketAttemptIsCurrent(socketAttemptToken)) {
        return;
    }
    const QString host = m_lastHost;
    const quint16 port = m_lastPort;
    const quint64 endpointGeneration = m_endpointGeneration;
    m_identityTimer.stop();
    m_pendingIdentityInfoSeq = 0;
    m_connected = false;

    QPointer<PgxlConnection> self(this);
    emit identityAdmissionFailed(socketAttemptToken, reason);
    if (!self || !socketAttemptIsCurrent(socketAttemptToken)) {
        return;
    }
    emit connectionFailed(reason);
    if (!self || !socketAttemptIsCurrent(socketAttemptToken)
        || !requestIsCurrent(host, port, endpointGeneration)) {
        return;
    }

    // Never admitted: retire its callbacks before aborting and let the
    // endpoint-generation backoff own the redial.
    m_suppressSocketReconnect = true;
    retireSocketAttempt();
    m_socket->abort();
    if (!self || !requestIsCurrent(host, port, endpointGeneration)) {
        return;
    }
    m_suppressSocketReconnect = false;
    scheduleReconnect();
}

void PgxlConnection::beginSocketAttempt(quint64 endpointGeneration)
{
    QTcpSocket* retiredSocket = m_socket;
    retireSocketAttempt();
    // TgxlConnection.cpp (2026-09-22): Qt's asynchronous connect timeout
    // leaves a socket Unconnected without resetting its engine, so binding
    // it again can fail with an invalid descriptor. Every physical attempt
    // gets a new socket; the retired one may still be the sender of the
    // signal whose consumer asked for this attempt, so delete it later.
    retiredSocket->deleteLater();
    m_socket = new QTcpSocket(this);
    ++m_nextSocketAttemptGeneration;
    if (m_nextSocketAttemptGeneration == 0) {
        ++m_nextSocketAttemptGeneration; // zero is the offline-parser seam
    }
    const quint64 attempt = m_nextSocketAttemptGeneration;
    m_socketAttemptGeneration = attempt;
    m_socketAttemptEndpointGeneration = endpointGeneration;

    m_socketConnectedConnection = connect(
        m_socket, &QTcpSocket::connected, this,
        [this, attempt] { onConnected(attempt); });
    m_socketDisconnectedConnection = connect(
        m_socket, &QTcpSocket::disconnected, this,
        [this, attempt] { onDisconnected(attempt); });
    m_socketReadyReadConnection = connect(
        m_socket, &QTcpSocket::readyRead, this,
        [this, attempt] { onReadyRead(attempt); });
    m_socketErrorConnection = connect(
        m_socket, &QTcpSocket::errorOccurred, this,
        [this, attempt](QAbstractSocket::SocketError) { onError(attempt); });
}

void PgxlConnection::queueDial(const QString& host, quint16 port, quint64 generation)
{
    if (!requestIsCurrent(host, port, generation)) {
        return;
    }
    retireSocketAttempt();
    m_connectHost = host;
    m_connectPort = port;
    m_connectGeneration = generation;
    m_connectTimer.start(0);
}

void PgxlConnection::onConnectTimeout()
{
    const QString host = m_connectHost;
    const quint16 port = m_connectPort;
    const quint64 generation = m_connectGeneration;
    if (!requestIsCurrent(host, port, generation)) {
        return;
    }
    retireSocketAttempt();
    // abort()/bind() can emit socket signals synchronously; this one
    // controlled dial decides whether a retry is needed.
    m_suppressSocketReconnect = true;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        QPointer<PgxlConnection> self(this);
        m_socket->abort();
        if (!self) {
            return;
        }
    }

    beginSocketAttempt(generation);
    m_readBuf.clear();
    m_gotVersion = false;
    if (!requestIsCurrent(host, port, generation)) {
        m_suppressSocketReconnect = false;
        return;
    }

    QPointer<PgxlConnection> self(this);
    const SourceBindResult bindResult = bindSourceForHost(host);
    if (!self) {
        return;
    }
    if (!requestIsCurrent(host, port, generation)) {
        m_suppressSocketReconnect = false;
        return;
    }
    if (bindResult == SourceBindResult::Failed) {
        // A failed bind can leave the native descriptor unusable: reset,
        // check, then make exactly one OS-default-routing dial.
        m_socket->abort();
        if (!self) {
            return;
        }
        if (!requestIsCurrent(host, port, generation)) {
            m_suppressSocketReconnect = false;
            return;
        }
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_suppressSocketReconnect = false;
            qCWarning(lcPgxl) << "failed source bind did not reset; retrying later";
            scheduleReconnect();
            return;
        }
        qCInfo(lcPgxl) << "source bind unavailable; using OS default route";
    }
    m_suppressSocketReconnect = false;
    qCDebug(lcPgxl) << "connecting to" << host << ":" << port;
    m_socket->connectToHost(host, port);
}

void PgxlConnection::disconnect() {
    // PR #279 review #3 (2026-05-23): mark this disconnect as
    // user-initiated so onDisconnected() does not re-schedule a reconnect.
    // Without this, the Peripherals Disconnect button could not keep PGXL
    // disconnected because PGXL_AutoReconnect defaults true and
    // scheduleReconnect() fires unconditionally from onDisconnected.
    // The flag is cleared in connectToPgxl() so a fresh manual connect
    // re-arms auto-reconnect on subsequent network drops.
    // R-R3-47: it also cancels a queued dial and a pending retry, in every
    // phase, so nothing redials the old address.
    m_userInitiatedDisconnect = true;
    ++m_endpointGeneration;
    m_reconnectTimer.stop();
    m_connectTimer.stop();
    m_retryHost.clear();
    m_retryPort = 0;
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_pingTimer.stop();
    m_connected = false;
    clearPairing();
    const bool notifyDisconnected =
        m_socket->state() != QAbstractSocket::UnconnectedState;
    retireSocketAttempt();
    m_socket->abort();
    if (notifyDisconnected) {
        qCInfo(lcPgxl) << "PGXL user-initiated disconnect; auto-reconnect suppressed";
        // The socket's own callback was retired first. Emit last: a direct
        // consumer may delete this object.
        emit disconnected();
    }
}

quint32 PgxlConnection::sendCommand(const QString& cmd) {
    // R-R3-47: on the Core, nothing but the identity `info` is sent to an
    // amp the Core has not admitted (no pairing, no band, no operate).
    // A local window keeps its established behaviour.
    if (m_identityAdmissionRequired && !m_connected) {
        return 0;
    }
    return writeProtocolCommand(cmd);
}

quint32 PgxlConnection::writeProtocolCommand(const QString& cmd)
{
    quint32 seq = ++m_seq;
    QString line = QString("C%1|%2\n").arg(seq).arg(cmd);
    m_socket->write(line.toUtf8());
    qCDebug(lcPgxl) << "sent" << line.trimmed();
    // Phase 3P-II bench-diagnostic logging (remove after pairing protocol confirmed)
    qCInfo(lcPgxl) << "TX seq=" << seq << "cmd:" << cmd;
    ++m_framesOut;
    m_bytesOut += quint64(line.size());
    emit testFrameWrittenForTesting(line.trimmed());  // test seam
    // iPhone app plan Task 77 fix round 2: the amp starts changing over.
    if (cmd == QLatin1String("operate=1") || cmd == QLatin1String("operate=0")) {
        emit operateCommanded(cmd == QLatin1String("operate=1"), seq);
    }
    return seq;
}

void PgxlConnection::onConnected(quint64 attemptGeneration) {
    if (!socketAttemptIsCurrent(attemptGeneration)) {
        return;
    }
    qCDebug(lcPgxl) << "TCP connected, waiting for version line";
    m_reconnectTimer.stop();
    m_connectTimer.stop();
    m_connectedSinceMs = QDateTime::currentMSecsSinceEpoch();
    // Bench-fix 2026-05-20: reset the version-received latch so the next
    // V-frame arrival re-arms the handshake and re-sets m_connected=true.
    // Without this, m_gotVersion stays sticky from the previous session,
    // the V-frame from the new connection is silently dropped by the
    // `if (!m_gotVersion ...)` guard in processLine(), and the Peripherals
    // dialog reports "disconnected" forever despite an ESTABLISHED TCP
    // socket. Confirmed by lsof showing TCP ESTABLISHED to 9008 while
    // m_connected=false. Same bug pattern existed in TgxlConnection.
    m_gotVersion = false;
    clearIdentityAttempt();
    if (m_identityAdmissionRequired) {
        // Bound TCP, V, info and the Core's approval together: a peer that
        // accepts TCP but never answers must not stay provisional forever.
        m_identityTimer.start(m_identityTimeoutMs);
    }
}

void PgxlConnection::onDisconnected(quint64 attemptGeneration) {
    if (!socketAttemptIsCurrent(attemptGeneration)) {
        return;
    }
    const quint64 generation = m_endpointGeneration;
    // Phase 3P-II bench-diagnostic logging: record why PGXL dropped so the
    // bench audit trail shows the root cause. errorString() is populated by
    // Qt when the disconnect was caused by a network error; empty string means
    // a clean (operator-initiated) close.
    const QString err = m_socket->errorString();
    if (err.isEmpty()) {
        qCInfo(lcPgxl) << "PGXL disconnected cleanly";
    } else {
        qCWarning(lcPgxl) << "PGXL disconnected with error:" << err;
    }
    m_pollTimer.stop();
    m_keepaliveTimer.stop();
    m_pingTimer.stop();
    m_connected = false;
    clearPairing();
    clearIdentityAttempt();
    QPointer<PgxlConnection> self(this);
    emit disconnected();
    if (!self) {
        return;
    }
    // PR #279 review #3 (2026-05-23): only auto-reconnect on network
    // drops, not user-initiated disconnects.
    if (m_userInitiatedDisconnect) {
        qCInfo(lcPgxl) << "PGXL user-initiated disconnect; auto-reconnect suppressed";
        return;
    }
    if (generation != m_endpointGeneration
        || !socketAttemptIsCurrent(attemptGeneration)
        || m_suppressSocketReconnect) {
        return;
    }
    scheduleReconnect();
}

void PgxlConnection::onError(quint64 attemptGeneration) {
    if (!socketAttemptIsCurrent(attemptGeneration)) {
        return;
    }
    const quint64 generation = m_endpointGeneration;
    const QString err = m_socket->errorString();
    if (m_connected) {
        // Transient socket noise on an established connection (e.g., brief
        // network blip during status polling). Log it; don't overwrite the
        // UI status label via connectionFailed. The connection will either
        // recover silently or onDisconnected will fire and update status
        // cleanly.
        qCWarning(lcPgxl) << "transient socket error while connected:" << err;
        return;
    }
    qCWarning(lcPgxl) << "connect-time socket error:" << err;
    clearIdentityAttempt();
    QPointer<PgxlConnection> self(this);
    // In the Core's own words, not the socket library's: a station sends
    // this to an app as the connection error (iPhone app Part A fix wave,
    // R-IOS-01). The library's text is in the log line above.
    emit connectionFailed(QStringLiteral("The Core could not reach the Power Genius at this address. "
                                         "Check the amplifier's address and port, and that it is on."));
    if (!self) {
        return;
    }
    // 2026-05-20 bench fix (mirror of TgxlConnection): connect-time
    // failures do NOT trigger onDisconnected, so without this
    // scheduleReconnect() the exponential backoff stops dead after a
    // single failed retry. Schedule another attempt so the connection
    // keeps trying until PGXL accepts.
    if (generation == m_endpointGeneration
        && socketAttemptIsCurrent(attemptGeneration)
        && !m_userInitiatedDisconnect
        && !m_suppressSocketReconnect) {
        scheduleReconnect();
    }
}

void PgxlConnection::onReadyRead(quint64 attemptGeneration) {
    if (!socketAttemptIsCurrent(attemptGeneration)) {
        return;
    }
    QByteArray chunk = m_socket->readAll();
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
            QPointer<PgxlConnection> self(this);
            processLine(line, attemptGeneration);
            if (!self || !socketAttemptIsCurrent(attemptGeneration)) {
                return;
            }
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
    QString ourIp = m_socket->localAddress().toString();
    return sendCommand(QString("amplifier create ip=%1 port=%2 model=%3 serial_num=%4 ant=%5")
        .arg(ourIp)
        .arg(4992)  // SmartSDR API port (real FlexRadios advertise this; PGXL validates)
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
    if (seq == 0) {
        // R-R3-47: not sent (the Core has not admitted this amp).
        m_pairedRadioSerial.clear();
    }
    m_pendingPairingSeq = seq;
    return seq;
}

// From FlexRadio wiki spec: keepalive enable
quint32 PgxlConnection::enableKeepalive() {
    quint32 seq = sendCommand("keepalive enable");
    if (seq == 0) { return 0; }
    auto& s = AppSettings::instance();
    int intervalSec = s.value("PGXL_KeepaliveSec", "30").toInt();
    m_keepaliveTimer.setInterval(intervalSec * 1000);
    m_keepaliveTimer.start();
    return seq;
}

// From FlexRadio wiki spec: ping (no-op roundtrip for RTT measurement)
quint32 PgxlConnection::ping(const QString& tag) {
    quint32 seq = sendCommand("ping");
    if (seq == 0) { return 0; }
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

QString PgxlConnection::asSetupToken(const QString& text)
{
    QString out;
    bool gap = false;
    for (const QChar c : text.trimmed()) {
        if (c.isSpace() || c == QLatin1Char('=') || c.category() == QChar::Other_Control) {
            gap = true;
            continue;
        }
        if (gap && !out.isEmpty()) {
            out += QLatin1Char('_');
        }
        gap = false;
        out += c;
    }
    return out;
}

bool PgxlConnection::isSetupToken(const QString& text)
{
    for (const QChar c : text) {
        if (c.isSpace() || c == QLatin1Char('=') || c.category() == QChar::Other_Control) {
            return false;
        }
    }
    return true;
}

// From FlexRadio wiki spec: setup <kv> ... (write fields as space-separated k=v pairs)
quint32 PgxlConnection::writeSetup(const QMap<QString,QString>& fields) {
    QStringList parts;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        // RD-I11: a key or value with a space or '=' would split into
        // fields of its own; nothing is sent.
        if (!isSetupToken(it.key()) || !isSetupToken(it.value())) {
            qCWarning(lcPgxl) << "setup field refused (space or '=' in it):" << it.key();
            return 0;
        }
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
    const QString cmd = QString("flexradio ampslice=%1 serial=%2 band=%3")
            .arg(slice)
            .arg(m_pairedRadioSerial)
            .arg(bandHz);
    // Bench-fix 2026-05-19: log before send so /tmp/nereus-pgxl.log shows the
    // exact wire command, confirming both that the push fires and what PGXL sees.
    qCInfo(lcPgxl) << "setBand sending:" << cmd;
    return sendCommand(cmd);
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

void PgxlConnection::onIdentityTimeout()
{
    const quint64 attempt = m_socketAttemptGeneration;
    if (!m_identityAdmissionRequired || !socketAttemptIsCurrent(attempt)) {
        return;
    }
    qCInfo(lcPgxl) << "PGXL identity timed out; serial" << m_identityInfo.serial;
    failIdentityAdmission(attempt,
        m_identityInfo.serial.isEmpty()
            ? QStringLiteral("The device at this address did not answer as a Power Genius in time.")
            : QStringLiteral("The Core did not see this Power Genius on its network in time."));
}

void PgxlConnection::scheduleReconnect() {
    auto& s = AppSettings::instance();
    if (s.value("PGXL_AutoReconnect", "True").toString() != "True") {
        qCInfo(lcPgxl) << "scheduleReconnect: PGXL_AutoReconnect=False, NOT retrying";
        return;
    }
    if (m_lastHost.isEmpty()) {
        qCInfo(lcPgxl) << "scheduleReconnect: m_lastHost empty, NOT retrying"
                          " (never had a successful initial connect)";
        return;
    }
    if (m_userInitiatedDisconnect) {
        qCInfo(lcPgxl) << "scheduleReconnect: operator disconnected, NOT retrying";
        return;
    }
    // R-R3-47: the owned timer is the deduplication (TgxlConnection's
    // pattern, replacing the 500 ms time window): Qt's paired errorOccurred
    // and disconnected for one failure schedule one retry, and a queued
    // dial already is the endpoint's next attempt.
    if (m_reconnectTimer.isActive() || m_connectTimer.isActive()) {
        qCDebug(lcPgxl) << "scheduleReconnect: retry or dial already pending";
        return;
    }

    int idx = std::min(m_reconnectAttempts, int(std::size(kBackoffSec)) - 1);
    int delayMs = kBackoffSec[idx] * m_reconnectBackoffUnitMs;
    ++m_reconnectAttempts;
    const int attempt = m_reconnectAttempts;
    m_retryHost = m_lastHost;
    m_retryPort = m_lastPort;
    m_retryGeneration = m_endpointGeneration;
    qCInfo(lcPgxl) << "scheduleReconnect: attempt #" << m_reconnectAttempts
                   << "scheduled in" << delayMs << "ms to" << m_retryHost << ":" << m_retryPort;
    m_reconnectTimer.start(delayMs);
    // Start before notifying: a direct observer may replace the endpoint,
    // disconnect or delete this object. No member is touched afterwards.
    emit reconnectAttempt(attempt, delayMs);
}

void PgxlConnection::onReconnectTimeout()
{
    if (AppSettings::instance().value("PGXL_AutoReconnect", "True").toString() != "True") {
        qCInfo(lcPgxl) << "reconnect disabled before timeout fired";
        return;
    }
    const QString host = m_retryHost;
    const quint16 port = m_retryPort;
    const quint64 generation = m_retryGeneration;
    if (!requestIsCurrent(host, port, generation)) {
        qCDebug(lcPgxl) << "discarded stale reconnect timeout";
        return;
    }
    qCInfo(lcPgxl) << "reconnect timeout: dialing" << host << ":" << port;
    queueDial(host, port, generation);
}

void PgxlConnection::testForceDisconnect() {
    m_connected = false;
    // Each call is a distinct synthetic drop: cancel the previous synthetic
    // attempt so back-to-back calls exercise the whole backoff schedule
    // (tst_pgxl_connection_reconnect). Production code never calls this.
    m_reconnectTimer.stop();
    m_connectTimer.stop();
    scheduleReconnect();
}

void PgxlConnection::testInjectLineForSocketAttempt(const QString& line,
                                                    quint64 attemptGeneration)
{
    processLine(line, attemptGeneration);
}

void PgxlConnection::testInjectFailureForSocketAttempt(quint64 attemptGeneration)
{
    QPointer<PgxlConnection> self(this);
    onError(attemptGeneration);
    if (!self) {
        return;
    }
    onDisconnected(attemptGeneration);
}

void PgxlConnection::testFlushPingTimeouts() {
    QList<quint32> allSeqs = m_pendingPings.keys();
    for (quint32 seq : allSeqs) {
        emit pingTimedOut(seq);
        m_pendingPings.remove(seq);
    }
}

void PgxlConnection::processLine(const QString& line, quint64 attemptGeneration) {
    const bool offlineTest = (attemptGeneration == 0);
    if (!offlineTest && !socketAttemptIsCurrent(attemptGeneration)) {
        return;
    }
    // Version: V3.8.9
    if (!m_gotVersion && line.startsWith('V')) {
        m_version = line.mid(1);
        m_gotVersion = true;
        qCInfo(lcPgxl) << "PGXL version" << m_version;
        // Phase 3P-II bench-diagnostic logging (remove after pairing protocol confirmed)
        qCInfo(lcPgxl) << "RX V-frame version=" << m_version;
        QPointer<PgxlConnection> self(this);
        if (m_identityAdmissionRequired && !offlineTest) {
            // R-R3-47: V says only that this peer speaks the Genius
            // protocol (a Tuner Genius sends one too). Ask which unit it
            // is; the Core's controller matches that against the LAN
            // discovery announcement before anything else is sent.
            m_pendingIdentityInfoSeq = writeProtocolCommand(QStringLiteral("info"));
            if (!self || !socketAttemptIsCurrent(attemptGeneration)) {
                return;
            }
            const QString address = m_socket->peerAddress().toString();
            const quint16 port = m_socket->peerPort();
            emit identityProtocolProgress(attemptGeneration, address, port, m_version);
            return;
        }
        // Local window: V completes the handshake.
        m_reconnectTimer.stop();
        m_connectTimer.stop();
        m_reconnectAttempts = 0;
        m_retryHost.clear();
        m_retryPort = 0;
        sendCommand("info");
        if (!self || (!offlineTest && !socketAttemptIsCurrent(attemptGeneration))) {
            return;
        }
        sendCommand("status");
        if (!self || (!offlineTest && !socketAttemptIsCurrent(attemptGeneration))) {
            return;
        }
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

            // R-R3-47: the identity `info` reply on the Core. Captured from
            // a real PGXL (captures/flex-tgxl-direct-CONTROL.pcapng, C30698
            // and C206): `R<seq>|0|serial=10-200/24-0046  version=3.8.9
            // protocol=1.0 mains=240`, key=value pairs with no leading
            // word, two spaces after the serial.
            if (m_identityAdmissionRequired && !m_connected
                && m_pendingIdentityInfoSeq != 0 && rseq == m_pendingIdentityInfoSeq) {
                const quint64 identityAttempt = attemptGeneration;
                if (!hexOk || hexCode != 0) {
                    qCInfo(lcPgxl) << "PGXL native info failed with code"
                                       << (hexOk ? QString::number(hexCode, 16)
                                                 : QStringLiteral("parse-error"));
                    failIdentityAdmission(identityAttempt,
                        QStringLiteral("The Power Genius at this address did not say which unit it is."));
                    return;
                }
                QMap<QString, QString> fields;
                for (const QString& part : body.split(' ', Qt::SkipEmptyParts)) {
                    const int eq = part.indexOf('=');
                    if (eq > 0) { fields.insert(part.left(eq), part.mid(eq + 1)); }
                }
                const QString serial = fields.value(QStringLiteral("serial"));
                if (serial.isEmpty()) {
                    qCInfo(lcPgxl) << "PGXL info reply named no serial";
                    failIdentityAdmission(identityAttempt,
                        QStringLiteral("The Power Genius at this address did not say which unit it is."));
                    return;
                }
                m_pendingIdentityInfoSeq = 0;
                m_identityStatus = fields;
                m_identityInfo = {
                    identityAttempt,
                    m_socket->peerAddress().toString(),
                    m_socket->peerPort(),
                    serial,
                    fields.value(QStringLiteral("version")),
                };
                // Emit last: the Core's controller may admit, reject,
                // replace the address, disconnect or delete this object.
                const PgxlIdentityInfo result = m_identityInfo;
                emit nativeInfoReceived(result);
                return;
            }

            // Until the Core admits this attempt, no other reply may
            // publish presence, readings or pairing.
            if (m_identityAdmissionRequired && !m_connected) {
                return;
            }

            // R-R3-47 / R-R3-22: every answer, by sequence, for the Core's
            // device settings. Emitted first; a consumer may delete this.
            {
                QPointer<PgxlConnection> self(this);
                emit replyReceived(rseq, hexOk && hexCode == 0, body);
                if (!self || (!offlineTest && !socketAttemptIsCurrent(attemptGeneration))) {
                    return;
                }
                // Task 77 fix round 4: a refusal only when the code reads
                // and is not zero (an unreadable code refuses nothing).
                if (hexOk && hexCode != 0) {
                    emit replyRefused(rseq);
                    if (!self || (!offlineTest && !socketAttemptIsCurrent(attemptGeneration))) {
                        return;
                    }
                }
            }

            // Check for pairing result correlation.
            if (m_pendingPairingSeq != 0 && rseq == m_pendingPairingSeq) {
                bool succeeded = (hexOk && hexCode == 0);
                // Phase 3P-II Task 66: if pairing failed, clear the serial so
                // setBand() stays a no-op until a new successful pair arrives.
                if (!succeeded) {
                    m_pairedRadioSerial.clear();
                }
                m_pendingPairingSeq = 0;
                QPointer<PgxlConnection> self(this);
                emit pairingResult(succeeded, body);
                if (!self || (!offlineTest && !socketAttemptIsCurrent(attemptGeneration))) {
                    return;
                }
            }

            // Check for pong correlation (ping response: R<seq>|0|).
            if (m_pendingPings.contains(rseq)) {
                PendingPing pp = m_pendingPings.take(rseq);
                qint64 rttMs = QDateTime::currentMSecsSinceEpoch() - pp.sentMs;
                QPointer<PgxlConnection> self(this);
                emit pongReceived(rseq, rttMs, pp.tag);
                if (!self || (!offlineTest && !socketAttemptIsCurrent(attemptGeneration))) {
                    return;
                }
            }

            // Check for save ack (R<seq>|0|saving).
            if (hexOk && hexCode == 0 && body == "saving") {
                QPointer<PgxlConnection> self(this);
                emit saveAcknowledged();
                if (!self || (!offlineTest && !socketAttemptIsCurrent(attemptGeneration))) {
                    return;
                }
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
        if (m_identityAdmissionRequired && !m_connected) {
            return;
        }
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
