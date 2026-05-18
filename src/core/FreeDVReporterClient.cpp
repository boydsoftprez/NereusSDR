// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - FreeDVReporterClient implementation.
//
// Ported from freedv-gui src/reporting/FreeDVReporter.cpp [@77e793a].
//   Wire-protocol logic, Socket.IO event names, JSON field shapes,
//   bulk_update fan-out, and the view-only auth payload all come from
//   the freedv-gui source.
//
// Structural pattern from AetherSDR src/core/FreeDvClient.cpp
// [@0cd4559].
//   Qt6 lifecycle (QWebSocket member + slots, QTimer ping/reconnect,
//   exponential-backoff reconnect math) follow the AetherSDR client,
//   which itself targeted the same Engine.IO / Socket.IO wire format.
//
// License (upstream):
//   - freedv-gui carries an LGPLv2.1+ root license (`freedv-gui/COPYING`).
//     The specific `FreeDVReporter.{h,cpp}` files carry a permissive
//     BSD-2-Clause-style file header (Copyright Mooneer Salem, no per-
//     file project copyright line); the BSD permission block is
//     reproduced verbatim below per the upstream redistribution clause.
//   - AetherSDR is GPL-3.0-or-later
//     (https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// LGPL is upgrade-compatible to GPL-3 (LGPL §3 conversion clause); the
// BSD-2-Clause file-header carve-out is GPL-compatible by its own terms.
//
// --- From freedv-gui src/reporting/FreeDVReporter.cpp [@77e793a] (verbatim header) ---
//
// =========================================================================
//  Name:            FreeDVReporter.h
//  Purpose:         Implementation of interface to freedv-reporter
//
//  Authors:         Mooneer Salem
//  License:
//
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//  - Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//
//  - Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// =========================================================================
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B5. See
//                                    FreeDVReporterClient.h for the full
//                                    attribution block. Implementation
//                                    follows the freedv-gui dispatch
//                                    table (connect_() :361-369) for
//                                    the on(...) handlers, AetherSDR's
//                                    Qt-side parser (handleEngineIO /
//                                    handleSocketIO at FreeDvClient.cpp
//                                    :141-219 [@0cd4559]) for the
//                                    Engine.IO / Socket.IO state
//                                    machine, and the freedv-gui per-
//                                    event handler bodies (:372-651)
//                                    for the JSON field extraction.
//                                    Dual-feed onFreqChange /
//                                    onRxReport spot synthesis
//                                    (`emitSpotFromFreqChange` /
//                                    `emitSpotFromRxReport`) is a
//                                    NereusSDR architectural decision
//                                    per design doc Section 4 Flow 2;
//                                    the spot field mapping mirrors
//                                    AetherSDR FreeDvClient.cpp
//                                    :233-370 [@0cd4559]. Logging
//                                    routes through NereusSDR's
//                                    `lcSpots` category instead of
//                                    AetherSDR's `lcDxCluster`; log
//                                    file path uses Qt's
//                                    AppConfigLocation (already lands
//                                    under `NereusSDR/`) instead of
//                                    AetherSDR's GenericConfigLocation
//                                    + "AetherSDR/freedv.log". AI
//                                    tooling: Anthropic Claude Code.

#include "FreeDVReporterClient.h"
#include "LogCategories.h"
#include "AppSettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <cmath>

namespace NereusSDR {

FreeDVReporterClient::FreeDVReporterClient(QObject* parent)
    : QObject(parent)
    , m_pingTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_serverUrl(QString::fromLatin1(DefaultWsUrl))
{
#ifdef HAVE_WEBSOCKETS
    // From AetherSDR src/core/FreeDvClient.cpp:14-39 [@0cd4559]
    m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(m_ws, &QWebSocket::connected, this, &FreeDVReporterClient::onWsConnected);
    connect(m_ws, &QWebSocket::disconnected, this, &FreeDVReporterClient::onWsDisconnected);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &FreeDVReporterClient::onWsTextMessage);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(m_ws, &QWebSocket::errorOccurred, this, &FreeDVReporterClient::onWsError);
#else
    connect(m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &FreeDVReporterClient::onWsError);
#endif
#endif

    m_pingTimer->setSingleShot(false);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &FreeDVReporterClient::onReconnectTimer);

    // Engine.IO ping keepalive. Reply handled in handleEngineIO.
    // From AetherSDR src/core/FreeDvClient.cpp:34-38 [@0cd4559]
    connect(m_pingTimer, &QTimer::timeout, this, [this] {
#ifdef HAVE_WEBSOCKETS
        if (m_ws && m_ws->state() == QAbstractSocket::ConnectedState) {
            sendText(QStringLiteral("2"));  // client-side Engine.IO Ping
        }
#endif
    });
}

FreeDVReporterClient::~FreeDVReporterClient()
{
    stopConnection();
    m_logFile.close();
}

void FreeDVReporterClient::setIdentity(const QString& callsign, const QString& gridSquare,
                                       const QString& message, const QString& version)
{
    m_callsign = callsign;
    m_gridSquare = gridSquare;
    m_statusMessage = message;
    m_version = version;
}

void FreeDVReporterClient::setServerUrl(const QString& url)
{
    m_serverUrl = url;
}

QString FreeDVReporterClient::logFilePath() const
{
    // Qt's AppConfigLocation already lands under "NereusSDR/" when
    // QCoreApplication::organizationName / applicationName are set
    // (mirrors SpotCollectorClient / PotaClient / DxClusterClient /
    // WsjtxClient B1-B4 ports).
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/freedv.log";
}

void FreeDVReporterClient::startConnection()
{
    if (m_connected.load()) return;

    qCDebug(lcSpots) << "FreeDVReporterClient: connecting to" << m_serverUrl;

    // Open log file (truncate on each start; same pattern as the other
    // spot-ingest clients).
    m_logFile.close();
    m_logFile.setFileName(logFilePath());
    QDir().mkpath(QFileInfo(m_logFile).absolutePath());
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_logFile.write(QString("--- FreeDV Reporter connected at %1 ---\n")
            .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss UTC")).toUtf8());
        m_logFile.flush();
    }

    m_stations.clear();
    m_intentionalDisconnect = false;
    m_reconnectAttempts = 0;

#ifdef HAVE_WEBSOCKETS
    if (m_ws) {
        m_ws->open(QUrl(m_serverUrl));
    }
#else
    qCWarning(lcSpots) << "FreeDVReporterClient: built without Qt6::WebSockets;"
                       << "live connections are disabled.";
    emit connectionError(QStringLiteral("Qt6::WebSockets not available"));
#endif
}

void FreeDVReporterClient::stopConnection()
{
    m_intentionalDisconnect = true;
    m_pingTimer->stop();
    m_reconnectTimer->stop();

#ifdef HAVE_WEBSOCKETS
    if (m_ws && m_ws->state() != QAbstractSocket::UnconnectedState) {
        m_ws->close();
    }
#endif

    m_connected.store(false);
    m_stations.clear();
    emit disconnected();
}

void FreeDVReporterClient::requestQSY(const QString& targetSid, quint64 freqHz,
                                      const QString& message)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:104-119 [@77e793a]
    if (!m_connected.load()) return;

    QJsonObject obj;
    obj["dest_sid"] = targetSid;
    obj["message"] = message;
    obj["frequency"] = qint64(freqHz);

    QJsonArray arr;
    arr.append(QStringLiteral("qsy_request"));
    arr.append(obj);
    sendText(QStringLiteral("42")
             + QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void FreeDVReporterClient::updateMessage(const QString& message)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:122-130 [@77e793a]
    // (the inner sendMessageImpl_ at :688-702).
    m_statusMessage = message;
    qCInfo(lcSpots) << "FreeDVReporterClient::updateMessage:"
                    << message
                    << "(connected=" << m_connected.load() << ")";
    if (!m_connected.load()) return;

    QJsonObject obj;
    obj["message"] = message;

    QJsonArray arr;
    arr.append(QStringLiteral("message_update"));
    arr.append(obj);
    sendText(QStringLiteral("42")
             + QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void FreeDVReporterClient::setFrequency(quint64 freqHz)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:653-668 [@77e793a]
    // (freqChangeImpl_). Emit a Socket.IO "freq_change" event with our
    // current operating frequency so the reporter server lists it next
    // to our station.
    //
    // Cache the value regardless of connected state so a reconnect or a
    // show_self toggle can re-push it. Matches freedv-gui's
    // `lastFrequency_ = frequency` at :667 [@77e793a].
    m_lastFrequency = freqHz;
    if (!m_connected.load()) return;

    QJsonObject obj;
    // qint64 cast: QJSON has no native uint64; freqHz fits safely in
    // qint64 for any HF/VHF frequency we'd encounter.
    obj["freq"] = qint64(freqHz);

    QJsonArray arr;
    arr.append(QStringLiteral("freq_change"));
    arr.append(obj);
    sendText(QStringLiteral("42")
             + QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void FreeDVReporterClient::sendRxReport(const QString& callsign,
                                        const QString& mode, int snrDb)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:187-203 [@77e793a]
    // (addReceiveRecord). Emits a Socket.IO "rx_report" event so
    // qso.freedv.org marks our row as decoding the source station.
    //
    // Two upstream callers (freedv-gui main.cpp:1959-1996 [@77e793a]):
    //   - Path A: callsign decoded from RADE EOO text channel ->
    //     `(callsign, "RADEV1", snr)`. NereusSDR drives this from
    //     RadioModel::onRadeTextDecoded.
    //   - Path B: RADE has sync but no callsign yet -> empty callsign
    //     `("", "RADEV1", snr)` every ~1 s while synced. NereusSDR
    //     drives this from FreeDVRadeReporterBridge.
    //
    // Upstream addReceiveRecord does NOT reject empty callsigns
    // (FreeDVReporter.cpp:187-203): the only gate is `isFullyConnected_`.
    // An earlier port added a `callsign.isEmpty()` short-circuit that
    // blocked Path B silently; re-porting to match upstream verbatim
    // semantics so both paths emit identically.
    if (!m_connected.load()) {
        return;
    }

    QJsonObject obj;
    obj["callsign"] = callsign;
    obj["mode"] = mode;
    obj["snr"] = snrDb;

    QJsonArray arr;
    arr.append(QStringLiteral("rx_report"));
    arr.append(obj);
    sendText(QStringLiteral("42")
             + QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void FreeDVReporterClient::setTransmitting(bool tx, const QString& mode)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:670-686 [@77e793a]
    // (transmitImpl_). Emit a Socket.IO "tx_report" event so the
    // reporter server marks our station as TXing (red row in the
    // reporter dialog) or RXing.
    //
    // Cache regardless of connected state so a reconnect or show_self
    // toggle can re-push it. Matches freedv-gui's `mode_ = mode; tx_ = tx`
    // at :684-685 [@77e793a].
    m_lastTxMode = mode;
    m_lastTxState = tx;
    if (!m_connected.load()) return;

    QJsonObject obj;
    obj["mode"] = mode;
    obj["transmitting"] = tx;

    QJsonArray arr;
    arr.append(QStringLiteral("tx_report"));
    arr.append(obj);
    sendText(QStringLiteral("42")
             + QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void FreeDVReporterClient::setHiddenFromView(bool hidden)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:167-185 [@77e793a]
    // (hideFromView / showOurselves) and the inner :704-729
    // (hideFromViewImpl_ / showOurselvesImpl_). Toggles the server-side
    // "hidden" flag for our station via the Socket.IO "hide_self" /
    // "show_self" events (no payload). On the hide-to-show transition,
    // the server has forgotten our state, so we re-emit freq_change /
    // tx_report / message_update -- matching upstream :726-728.
    m_hiddenFromView = hidden;
    if (!m_connected.load()) return;

    QJsonArray arr;
    arr.append(hidden ? QStringLiteral("hide_self")
                      : QStringLiteral("show_self"));
    sendText(QStringLiteral("42")
             + QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));

    if (!hidden) {
        // Re-emit cached state after show_self. From freedv-gui
        // FreeDVReporter.cpp:726-728 [@77e793a].
        if (m_lastFrequency != 0) {
            setFrequency(m_lastFrequency);
        }
        if (!m_lastTxMode.isEmpty()) {
            setTransmitting(m_lastTxState, m_lastTxMode);
        }
        if (!m_statusMessage.isEmpty()) {
            updateMessage(m_statusMessage);
        }
    }
}

// ── WebSocket slots ─────────────────────────────────────────────────────

void FreeDVReporterClient::onWsConnected()
{
    // Engine.IO open packet will arrive as the first text message;
    // we send the Socket.IO Connect from handleEngineIO type '0'
    // (handshake completes on receipt, not on TCP up).
    // From AetherSDR src/core/FreeDvClient.cpp:92-96 [@0cd4559]
    qCDebug(lcSpots) << "FreeDVReporterClient: WebSocket connected";
}

void FreeDVReporterClient::onWsDisconnected()
{
    // From AetherSDR src/core/FreeDvClient.cpp:98-115 [@0cd4559]
    m_connected.store(false);
    m_pingTimer->stop();
    m_stations.clear();

    if (m_intentionalDisconnect) {
        qCDebug(lcSpots) << "FreeDVReporterClient: disconnected (intentional)";
        return;
    }

    // Auto-reconnect with exponential backoff.
    // From AetherSDR src/core/FreeDvClient.cpp:107-113 [@0cd4559] — original
    // pattern is `1 << m_reconnectAttempts`. NereusSDR fix: clamp shift count
    // to avoid signed-int UB. The backoff saturates at MaxReconnectDelayMs
    // well before 30 attempts, so capping has no behavioral effect on a
    // healthy connection.
    const int shiftBits = std::min(m_reconnectAttempts, 30);
    int delay = std::min(InitialReconnectDelayMs * (1 << shiftBits),
                         MaxReconnectDelayMs);
    qCDebug(lcSpots) << "FreeDVReporterClient: disconnected, reconnecting in" << delay << "ms";
    emit rawLineReceived(QString("--- Disconnected, reconnecting in %1s ---").arg(delay / 1000));
    m_reconnectTimer->start(delay);
}

void FreeDVReporterClient::onWsError(QAbstractSocket::SocketError err)
{
    // From AetherSDR src/core/FreeDvClient.cpp:117-123 [@0cd4559]
    Q_UNUSED(err);
#ifdef HAVE_WEBSOCKETS
    QString msg = m_ws ? m_ws->errorString() : QStringLiteral("(unknown)");
#else
    QString msg = QStringLiteral("(WebSockets disabled)");
#endif
    qCWarning(lcSpots) << "FreeDVReporterClient: WebSocket error:" << msg;
    emit connectionError(msg);
}

void FreeDVReporterClient::onReconnectTimer()
{
    // From AetherSDR src/core/FreeDvClient.cpp:125-131 [@0cd4559]
    if (m_intentionalDisconnect) return;
    m_reconnectAttempts++;
    qCDebug(lcSpots) << "FreeDVReporterClient: reconnect attempt" << m_reconnectAttempts;
#ifdef HAVE_WEBSOCKETS
    if (m_ws) {
        m_ws->open(QUrl(m_serverUrl));
    }
#endif
}

void FreeDVReporterClient::onWsTextMessage(const QString& message)
{
    // From AetherSDR src/core/FreeDvClient.cpp:135-139 [@0cd4559]
    if (message.isEmpty()) return;
    handleEngineIO(message);
}

void FreeDVReporterClient::sendText(const QString& msg)
{
    m_lastSentForTest = msg;
#ifdef HAVE_WEBSOCKETS
    if (m_ws && m_ws->state() == QAbstractSocket::ConnectedState) {
        m_ws->sendTextMessage(msg);
    }
#endif
}

// ── Engine.IO / Socket.IO framing ───────────────────────────────────────
//
// Wire protocol authority: freedv-gui ships its own SocketIoClient
// (src/util/SocketIoClient.cpp [@77e793a]) which we do not port (it
// uses the websocketpp 3rd-party library and its own thread). Qt
// structure: AetherSDR src/core/FreeDvClient.cpp:141-219 [@0cd4559]
// already implemented the same Engine.IO v4 / Socket.IO v4 state
// machine on top of QWebSocket. We follow the AetherSDR shape with
// freedv-gui-faithful event dispatch.

void FreeDVReporterClient::handleEngineIO(const QString& raw)
{
    // From AetherSDR src/core/FreeDvClient.cpp:141-179 [@0cd4559]
    if (raw.isEmpty()) return;
    QChar type = raw.at(0);

    if (type == '0') {
        // Engine.IO Open: server sends JSON with sid, pingInterval,
        // pingTimeout. Parse pingInterval and start the keepalive
        // timer, then respond with Socket.IO Connect.
        QJsonDocument doc = QJsonDocument::fromJson(raw.mid(1).toUtf8());
        QJsonObject obj = doc.object();
        m_pingIntervalMs = obj.value(QStringLiteral("pingInterval")).toInt(25000);
        m_pingTimer->start(m_pingIntervalMs);

        // Send Socket.IO Connect with view OR report auth depending on
        // whether we have a callsign + grid identity to publish.
        //
        // Wire payload from freedv-gui FreeDVReporter.cpp:295-309
        // [@77e793a]:
        //   isValidForReporting() == false -> {"role":"view", proto}
        //   isValidForReporting() == true  -> {"role":"report",
        //                                      "callsign":..., "grid_square":...,
        //                                      "version":..., "rx_only":bool,
        //                                      "os":..., "protocol_version":2}
        //
        // Phase 3R K-bench (bench feedback): the NereusSDR FreeDV
        // Reporter client originally shipped view-only with the
        // comment "reporter mode is a 3M-x follow-up". That meant our
        // station never appeared on qso.freedv.org. This commit
        // upgrades view -> report when setIdentity() has been called
        // with a non-empty callsign + grid pair.
        QJsonObject auth;
        const bool canReport =
            !m_callsign.isEmpty() && !m_gridSquare.isEmpty();
        if (canReport) {
            auth["role"] = QStringLiteral("report");
            auth["callsign"] = m_callsign;
            auth["grid_square"] = m_gridSquare;
            auth["version"] = m_version.isEmpty()
                                  ? QStringLiteral("NereusSDR")
                                  : m_version;
            // rx_only: NereusSDR can TX RADE per Phase 3R K-bench, so
            // default to false. Future enhancement: expose this as a
            // user setting (e.g. for SWL-only ops).
            auth["rx_only"] = false;
            // os: NereusSDR is cross-platform; report the host OS so
            // other operators can see what we're running.
#if defined(Q_OS_MACOS)
            auth["os"] = QStringLiteral("macOS");
#elif defined(Q_OS_WIN)
            auth["os"] = QStringLiteral("Windows");
#elif defined(Q_OS_LINUX)
            auth["os"] = QStringLiteral("Linux");
#else
            auth["os"] = QStringLiteral("Unknown");
#endif
        } else {
            auth["role"] = QStringLiteral("view");
        }
        auth["protocol_version"] = 2;
        qCInfo(lcSpots).noquote()
            << "FreeDVReporterClient: auth payload role="
            << auth["role"].toString()
            << "callsign=" << m_callsign
            << "grid=" << m_gridSquare;
        sendText(QStringLiteral("40") + QString::fromUtf8(
            QJsonDocument(auth).toJson(QJsonDocument::Compact)));
        return;
    }

    if (type == '2') {
        // Engine.IO Ping -> reply with Pong
        sendText(QStringLiteral("3"));
        return;
    }

    if (type == '3') {
        // Engine.IO Pong -- response to our keepalive ping
        return;
    }

    if (type == '4') {
        // Engine.IO Message -- contains Socket.IO payload
        handleSocketIO(raw.mid(1));
        return;
    }
    // type '6' = noop, others = ignore
}

void FreeDVReporterClient::handleSocketIO(const QString& payload)
{
    // From AetherSDR src/core/FreeDvClient.cpp:181-219 [@0cd4559]
    if (payload.isEmpty()) return;

    QChar sioType = payload.at(0);

    if (sioType == '0') {
        // Socket.IO Connect ACK -- "0{"sid":"..."}"
        m_connected.store(true);
        m_reconnectAttempts = 0;
        qCInfo(lcSpots) << "FreeDVReporterClient: Socket.IO connected, "
                           "msg=" << m_statusMessage
                        << "hidden=" << m_hiddenFromView;
        emit rawLineReceived(QStringLiteral("Connected to ") + m_serverUrl);
        emit connected();

        // Phase 3R K-bench (2026-05-11 bench): upstream relies on the
        // server emitting `connection_successful` and then pushes
        // cached freq/tx/message inside that handler
        // (FreeDVReporter.cpp:424-433 [@77e793a]). The qso.freedv.org
        // server has been observed to skip that event for role=report
        // sessions on the bench, leaving the cached message unsent.
        // Push it here too, on the Socket.IO Connect ACK, so the
        // dashboard always sees our message. Idempotent if the
        // connection_successful event does later fire and re-pushes.
        if (m_hiddenFromView) {
            QJsonArray arr;
            arr.append(QStringLiteral("hide_self"));
            sendText(QStringLiteral("42")
                     + QString::fromUtf8(
                         QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        } else if (!m_statusMessage.isEmpty()) {
            QJsonObject obj;
            obj["message"] = m_statusMessage;
            QJsonArray arr;
            arr.append(QStringLiteral("message_update"));
            arr.append(obj);
            sendText(QStringLiteral("42")
                     + QString::fromUtf8(
                         QJsonDocument(arr).toJson(QJsonDocument::Compact)));
            qCInfo(lcSpots) << "FreeDVReporterClient: pushed cached message"
                            << m_statusMessage << "on Socket.IO ACK";
        }
        return;
    }

    if (sioType == '2') {
        // Socket.IO Event -- "2["event_name",{...}]"
        QString json = payload.mid(1);
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (!doc.isArray()) return;
        QJsonArray arr = doc.array();
        if (arr.size() < 2) return;

        QString eventName = arr[0].toString();
        if (eventName == QStringLiteral("bulk_update")) {
            onBulkUpdate(arr[1].toArray());
        } else {
            handleEvent(eventName, arr[1].toObject());
        }
        return;
    }

    if (sioType == '1') {
        // Socket.IO Disconnect
        qCDebug(lcSpots) << "FreeDVReporterClient: server disconnected us";
        m_connected.store(false);
        return;
    }
}

// ── Event dispatch ──────────────────────────────────────────────────────
//
// Dispatch table mirrors freedv-gui src/reporting/FreeDVReporter.cpp
// :361-369 [@77e793a] (the on() registrations inside connect_()).

void FreeDVReporterClient::handleEvent(const QString& eventName, const QJsonObject& data)
{
    if      (eventName == QStringLiteral("new_connection"))         onNewConnection(data);
    else if (eventName == QStringLiteral("freq_change"))            onFreqChange(data);
    else if (eventName == QStringLiteral("rx_report"))              onRxReport(data);
    else if (eventName == QStringLiteral("tx_report"))              onTxReport(data);
    else if (eventName == QStringLiteral("remove_connection"))      onRemoveConnection(data);
    else if (eventName == QStringLiteral("message_update"))         onMessageUpdate(data);
    else if (eventName == QStringLiteral("connection_successful"))  onConnectionSuccessful(data);
    // Silently ignore: chat_*, qsy_request (we are view-only), etc.
}

void FreeDVReporterClient::onNewConnection(const QJsonObject& data)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:372-406 [@77e793a]
    // (onFreeDVReporterNewConnection_).
    QString sid = data.value(QStringLiteral("sid")).toString();
    if (sid.isEmpty()) return;

    FreeDVStation info;
    info.sid          = sid;
    info.callsign     = data.value(QStringLiteral("callsign")).toString();
    info.gridSquare   = data.value(QStringLiteral("grid_square")).toString();
    info.version      = data.value(QStringLiteral("version")).toString();
    info.rxOnly       = data.value(QStringLiteral("rx_only")).toBool();
    info.status       = info.rxOnly ? QStringLiteral("RX Only") : QStringLiteral("Active");
    info.connectTime  = QDateTime::fromString(
        data.value(QStringLiteral("connect_time")).toString(), Qt::ISODate);
    info.lastUpdate   = QDateTime::fromString(
        data.value(QStringLiteral("last_update")).toString(), Qt::ISODate);

    m_stations[sid] = info;
    emit stationAdded(sid, info);
}

void FreeDVReporterClient::onFreqChange(const QJsonObject& data)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:555-583 [@77e793a]
    // (onFreeDVReporterFrequencyChange_).
    QString sid = data.value(QStringLiteral("sid")).toString();
    if (sid.isEmpty()) return;

    // Create the entry if new_connection hasn't arrived yet (bulk_update
    // can re-fire freq_change before the connect packet is replayed).
    // From AetherSDR src/core/FreeDvClient.cpp:243-262 [@0cd4559]
    if (!m_stations.contains(sid)) {
        FreeDVStation info;
        info.sid       = sid;
        info.callsign  = data.value(QStringLiteral("callsign")).toString();
        info.gridSquare = data.value(QStringLiteral("grid_square")).toString();
        m_stations.insert(sid, info);
    }

    auto& info = m_stations[sid];
    info.frequencyHz = data.value(QStringLiteral("freq")).toVariant().toULongLong();

    // Pick up callsign / grid if present (bulk_update freq_change
    // includes them).
    if (info.callsign.isEmpty()) {
        info.callsign = data.value(QStringLiteral("callsign")).toString();
    }
    if (info.gridSquare.isEmpty()) {
        info.gridSquare = data.value(QStringLiteral("grid_square")).toString();
    }
    info.lastUpdate = QDateTime::fromString(
        data.value(QStringLiteral("last_update")).toString(), Qt::ISODate);

    emit stationUpdated(sid, info);

    // NereusSDR dual-feed: also synthesize a DxSpot.
    emitSpotFromFreqChange(sid);
}

void FreeDVReporterClient::onRxReport(const QJsonObject& data)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:505 [@77e793a]
    // + src/gui/dialogs/freedv_reporter.cpp:3651-3723 [@77e793a].
    //
    // Upstream rx_report payload keys (from FreeDVReporter.cpp:505-514):
    //   sid, last_update, receiver_callsign, receiver_grid_square,
    //   callsign (= received callsign), snr, mode.
    //
    // Upstream onReceiveUpdateFn_ (freedv_reporter.cpp:3693-3723)
    // treats `callsign == "" && rxMode == ""` as a FREQUENCY CHANGE
    // sentinel: clear lastRxDate, lastRxCallsign, lastRxMode, snr.
    // Otherwise stamp lastRxDate = now and write the (possibly empty)
    // callsign + mode + snr.  This empty-callsign-but-mode-set case
    // is what feeds the row-coloring SHORT_TIMEOUT branch
    // (freedv_reporter.cpp:1296-1299): a station that just changed
    // mode briefly highlights cyan even before it decodes a peer
    // callsign.  Without this distinction, our coloring never lights
    // up when qso.freedv.org delivers a mode-only update.
    QString sid = data.value(QStringLiteral("sid")).toString();
    if (sid.isEmpty()) return;

    if (m_stations.contains(sid)) {
        auto& info = m_stations[sid];
        const QString recvCall = data.value(QStringLiteral("callsign")).toString();
        const QString recvMode = data.value(QStringLiteral("mode")).toString();

        // Per-event tracer (off by default; enable via
        // QT_LOGGING_RULES="nereus.spots.debug=true").  Captures
        // wire-shape data for bench triage of "Last RX Callsign"
        // column update timing — freedv_reporter.cpp:3651-3723
        // upstream stores receivedCallsign verbatim, so empty
        // strings here mean the band is quiet, not a parser bug.
        qCDebug(lcSpots).noquote()
            << QStringLiteral("FreeDV rx_report sid=%1 stationCall=%2 "
                              "decodedCall='%3' mode='%4' snr=%5")
                   .arg(sid.left(8),
                        info.callsign,
                        recvCall,
                        recvMode)
                   .arg(data.value(QStringLiteral("snr")).toDouble(), 0, 'f', 1);

        if (recvCall.isEmpty() && recvMode.isEmpty()) {
            // Frequency-change sentinel.  Clear RX state so the row
            // stops painting cyan and the table column blanks out.
            info.lastRxCallsign.clear();
            info.lastRxMode.clear();
            info.snrVal  = 0;
            info.snrText.clear();
            info.lastRxDate = QDateTime();  // invalid -> kills RX highlight
        } else {
            // 2026-05-12 bench-fix (PR #238): only overwrite the
            // callsign cell when the wire actually carries one.
            // qso.freedv.org sends mode-only rx_report keep-alives
            // (callsign="" mode="<mode>") during ongoing decode;
            // upstream stores those as empty + lets the freq-change
            // sentinel be the only clear path, but that flickers
            // the "Last RX Callsign" column to empty every few
            // seconds during a transmission so the user can't read
            // who's being decoded until the EOO arrives.  Keep the
            // last-known callsign sticky here so the column stays
            // populated for the full TX.  Sentinel-cleared above
            // still wipes it cleanly on freq change.
            if (!recvCall.isEmpty()) {
                info.lastRxCallsign = recvCall;
            }
            info.lastRxMode     = recvMode;
            // SNR may be int or real (freedv-gui :518-530);
            // QJsonValue::toDouble covers both.
            info.snrVal         = static_cast<int>(std::round(
                                      data.value(QStringLiteral("snr")).toDouble()));
            info.snrText        = QString::number(info.snrVal);
            info.lastRxDate     = QDateTime::currentDateTimeUtc();
        }

        info.lastUpdate     = QDateTime::fromString(
            data.value(QStringLiteral("last_update")).toString(), Qt::ISODate);
        emit stationUpdated(sid, info);
    }

    // NereusSDR dual-feed: synthesize a DxSpot for the reported
    // transmitter.
    emitSpotFromRxReport(data);
}

void FreeDVReporterClient::onTxReport(const QJsonObject& data)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:470-503 [@77e793a]
    // + src/gui/dialogs/freedv_reporter.cpp:3580-3645 [@77e793a]
    // (onFreeDVReporterTransmitReport_ + onTransmitUpdateFn_).
    QString sid = data.value(QStringLiteral("sid")).toString();
    if (sid.isEmpty()) return;

    if (!m_stations.contains(sid)) return;
    auto& info = m_stations[sid];
    info.txMode       = data.value(QStringLiteral("mode")).toString();
    info.transmitting = data.value(QStringLiteral("transmitting")).toBool();

    // 2026-05-12 (PR #238 bench follow-up): match upstream
    // freedv_reporter.cpp:3610-3617 — the Status column flips
    // "Active" <-> "TX" on each tx_report so the user sees who's
    // currently keyed even before the row coloring fires.  "RX Only"
    // stations are left as-is (upstream guards with status !=
    // _(RX_ONLY_STATUS) at line 3619).
    if (info.status != QStringLiteral("RX Only")) {
        info.status = info.transmitting
            ? QStringLiteral("TX")
            : QStringLiteral("Active");
    }

    if (info.transmitting) {
        info.lastTxDate = QDateTime::currentDateTimeUtc();
    }
    info.lastUpdate = QDateTime::fromString(
        data.value(QStringLiteral("last_update")).toString(), Qt::ISODate);

    // Per-event tracer (off by default; enable via
    // QT_LOGGING_RULES="nereus.spots.debug=true").  Captures
    // tx_report state-transition events for bench triage of "row
    // not turning red" reports.  No keepalive: upstream fires
    // tx_report only on transmitting=true<->false transitions
    // (FreeDVReporter.cpp:470-503).
    qCDebug(lcSpots).noquote()
        << QStringLiteral("FreeDV tx_report sid=%1 call=%2 "
                          "transmitting=%3 mode='%4'")
               .arg(sid.left(8),
                    info.callsign,
                    info.transmitting ? QStringLiteral("TRUE")
                                      : QStringLiteral("FALSE"),
                    info.txMode);

    emit stationUpdated(sid, info);
}

void FreeDVReporterClient::onRemoveConnection(const QJsonObject& data)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:436-468 [@77e793a]
    // (onFreeDVReporterRemoveConnection_).
    QString sid = data.value(QStringLiteral("sid")).toString();
    if (sid.isEmpty()) return;
    m_stations.remove(sid);
    emit stationRemoved(sid);
}

void FreeDVReporterClient::onMessageUpdate(const QJsonObject& data)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:585-607 [@77e793a]
    // (onFreeDVReporterMessageUpdate_).
    QString sid = data.value(QStringLiteral("sid")).toString();
    if (sid.isEmpty() || !m_stations.contains(sid)) return;
    auto& info = m_stations[sid];
    info.userMessage = data.value(QStringLiteral("message")).toString();
    info.lastUpdate  = QDateTime::fromString(
        data.value(QStringLiteral("last_update")).toString(), Qt::ISODate);
    emit stationUpdated(sid, info);
}

void FreeDVReporterClient::onConnectionSuccessful(const QJsonObject& data)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:408-434 [@77e793a]
    // (onFreeDVReporterConnectionSuccessful_).
    Q_UNUSED(data);
    qCInfo(lcSpots) << "FreeDVReporterClient: connection_successful"
                    << "hidden=" << m_hiddenFromView;

    // freedv-gui :424-433 [@77e793a]: branch on `hidden_`. If hidden,
    // assert hide_self (no other state push -- server doesn't need our
    // freq/tx/message while we're hidden). Otherwise re-emit
    // freq_change + tx_report + message_update so the dashboard row
    // reflects our current state.
    if (m_hiddenFromView) {
        QJsonArray arr;
        arr.append(QStringLiteral("hide_self"));
        sendText(QStringLiteral("42")
                 + QString::fromUtf8(
                     QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        qCInfo(lcSpots) << "FreeDVReporterClient: asserted hide_self";
        return;
    }

    // Re-push cached state. Matches freedv-gui :430-432 [@77e793a]
    // (freqChangeImpl_ + transmitImpl_ + sendMessageImpl_).
    if (m_lastFrequency != 0) {
        QJsonObject obj;
        obj["freq"] = qint64(m_lastFrequency);
        QJsonArray arr;
        arr.append(QStringLiteral("freq_change"));
        arr.append(obj);
        sendText(QStringLiteral("42")
                 + QString::fromUtf8(
                     QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    }
    if (!m_lastTxMode.isEmpty()) {
        QJsonObject obj;
        obj["mode"] = m_lastTxMode;
        obj["transmitting"] = m_lastTxState;
        QJsonArray arr;
        arr.append(QStringLiteral("tx_report"));
        arr.append(obj);
        sendText(QStringLiteral("42")
                 + QString::fromUtf8(
                     QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    }
    if (!m_statusMessage.isEmpty()) {
        QJsonObject obj;
        obj["message"] = m_statusMessage;
        QJsonArray arr;
        arr.append(QStringLiteral("message_update"));
        arr.append(obj);
        sendText(QStringLiteral("42")
                 + QString::fromUtf8(
                     QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        qCInfo(lcSpots) << "FreeDVReporterClient: pushed message"
                        << m_statusMessage;
    }
}

void FreeDVReporterClient::onBulkUpdate(const QJsonArray& pairs)
{
    // From freedv-gui src/reporting/FreeDVReporter.cpp:633-651 [@77e793a]
    // (onFreeDVReporterBulkUpdate_) and AetherSDR FreeDvClient.cpp:313-320
    // [@0cd4559]. Each pair is [event_name, payload]; fan out through
    // handleEvent so per-event handlers run uniformly.
    for (const auto& item : pairs) {
        QJsonArray pair = item.toArray();
        if (pair.size() < 2) continue;
        handleEvent(pair[0].toString(), pair[1].toObject());
    }
}

// ── Dual-feed spot synthesis ────────────────────────────────────────────
//
// Dual-feed: NereusSDR architectural decision per design doc Section 4
// Flow 2. freedv-gui's onFrequencyChange / onRxReport update the
// station map only. AetherSDR's FreeDvClient also synthesizes a DxSpot
// for the panadapter overlay. We do both: the station map drives
// FreeDVStationModel (Reporter dialog), the synthesized spot drives
// SpotModel (panadapter overlay).

void FreeDVReporterClient::emitSpotFromFreqChange(const QString& sid)
{
    // From AetherSDR src/core/FreeDvClient.cpp:243-295 [@0cd4559]
    if (!m_stations.contains(sid)) return;
    const auto& info = m_stations[sid];

    double freqMhz = static_cast<double>(info.frequencyHz) / 1.0e6;
    if (freqMhz <= 0.0 || info.callsign.isEmpty()) return;

    // Emit a spot for every station with a known frequency. This is
    // the primary spot source; rx_report only fires when one station
    // actually decodes another, which is rare. freq_change fires on
    // connect (bulk_update) and whenever a station retunes.
    DxSpot spot;
    spot.dxCall      = info.callsign;
    spot.spotterCall = info.callsign;  // self-reported
    spot.freqMhz     = freqMhz;
    spot.source      = QStringLiteral("FreeDV");
    spot.comment     = info.txMode.isEmpty() ? QStringLiteral("FreeDV") : info.txMode;
    if (!info.gridSquare.isEmpty()) {
        spot.comment += QStringLiteral(" ") + info.gridSquare;
    }
    spot.utcTime     = QDateTime::currentDateTimeUtc().time();
    spot.lifetimeSec = 0;

    QString freedvColor = AppSettings::instance()
        .value(QStringLiteral("FreeDvSpotColor"), QStringLiteral("#FF8C00")).toString();
    if (freedvColor.length() == 7) {
        freedvColor = QStringLiteral("#FF") + freedvColor.mid(1);
    }
    spot.color = freedvColor;

    QString logLine = QString("%1  %2  %3 MHz  %4")
        .arg(spot.utcTime.toString("HH:mm"), info.callsign,
             QString::number(freqMhz, 'f', 4), spot.comment);
    if (m_logFile.isOpen()) {
        m_logFile.write((logLine + "\n").toUtf8());
        m_logFile.flush();
    }
    emit rawLineReceived(logLine);
    emit spotReceived(spot);
}

void FreeDVReporterClient::emitSpotFromRxReport(const QJsonObject& data)
{
    // From AetherSDR src/core/FreeDvClient.cpp:322-370 [@0cd4559]
    QString sid = data.value(QStringLiteral("sid")).toString();

    // Look up the receiving station's frequency from our state map
    // (freedv-gui rx_report payload doesn't carry the receiver's freq;
    // it's the station's own state, captured from its earlier
    // freq_change).
    double freqMhz = 0.0;
    if (m_stations.contains(sid)) {
        freqMhz = static_cast<double>(m_stations[sid].frequencyHz) / 1.0e6;
    }
    if (freqMhz <= 0.0) {
        return;  // cannot spot without a frequency
    }

    QString receiverCall = data.value(QStringLiteral("receiver_callsign")).toString();
    QString txCall       = data.value(QStringLiteral("callsign")).toString();
    QString mode         = data.value(QStringLiteral("mode")).toString();
    double  snr          = data.value(QStringLiteral("snr")).toDouble();
    QString grid         = data.value(QStringLiteral("receiver_grid_square")).toString();

    if (txCall.isEmpty()) return;

    // 2026-05-12 (PR #238 bench follow-up): dedup the multi-receiver
    // fan-out so a single TX seen by 20 different listening stations
    // doesn't spawn 20 panadapter labels at the same freq.
    // Source-first from AetherSDR MainWindow.cpp:635-660 [@0cd4559]
    // (isDuplicateSpot).  Window matches AetherSDR's 60 s FreeDV
    // default for spots without an explicit lifetimeSec.
    constexpr qint64 kSpotDedupWindowMs = 60'000;
    constexpr double kSpotDedupFreqTolMhz = 0.001;  // 1 Hz; same as AetherSDR
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    auto dedupIt = m_spotDedup.constFind(txCall);
    if (dedupIt != m_spotDedup.constEnd()) {
        const bool sameFreq = qAbs(dedupIt->freqMhz - freqMhz)
                                  < kSpotDedupFreqTolMhz;
        const bool fresh    = (nowMs - dedupIt->addedMs)
                                  < kSpotDedupWindowMs;
        if (sameFreq && fresh) {
            return;  // suppress duplicate
        }
    }
    m_spotDedup.insert(txCall, DedupEntry{freqMhz, nowMs});

    DxSpot spot;
    spot.dxCall      = txCall;
    spot.spotterCall = receiverCall;
    spot.freqMhz     = freqMhz;
    spot.snr         = static_cast<int>(std::round(snr));
    spot.source      = QStringLiteral("FreeDV");
    spot.comment     = mode;
    if (!grid.isEmpty()) {
        spot.comment += QStringLiteral(" ") + grid;
    }
    spot.utcTime     = QDateTime::currentDateTimeUtc().time();
    spot.lifetimeSec = 0;  // use source default from AppSettings

    QString freedvColor = AppSettings::instance()
        .value(QStringLiteral("FreeDvSpotColor"), QStringLiteral("#FF8C00")).toString();
    if (freedvColor.length() == 7) {
        freedvColor = QStringLiteral("#FF") + freedvColor.mid(1);
    }
    spot.color = freedvColor;

    QString logLine = QString("%1  %2 heard %3  %4 MHz  SNR %5 dB  %6")
        .arg(spot.utcTime.toString("HH:mm"), receiverCall, txCall,
             QString::number(freqMhz, 'f', 4),
             QString::number(snr, 'f', 1), mode);
    if (m_logFile.isOpen()) {
        m_logFile.write((logLine + "\n").toUtf8());
        m_logFile.flush();
    }
    emit rawLineReceived(logLine);
    emit spotReceived(spot);
}

} // namespace NereusSDR
