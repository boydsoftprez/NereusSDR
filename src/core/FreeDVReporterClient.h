// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - FreeDVReporterClient: Engine.IO/Socket.IO WebSocket
// client for qso.freedv.org. Dual-feed: drives both FreeDVStationModel
// (rich 14-field live state) and SpotModel (one-shot spot stream).
//
// Ported from freedv-gui src/reporting/FreeDVReporter.{h,cpp} [@77e793a].
//   Wire-protocol logic, Socket.IO event names, JSON field shapes,
//   bulk_update fan-out, and the view-only auth payload all come from
//   the freedv-gui source.
//
// Structural pattern from AetherSDR src/core/FreeDvClient.{h,cpp}
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
// --- From freedv-gui src/reporting/FreeDVReporter.h [@77e793a] (verbatim header) ---
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
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B5. Initial port.
//                                    Replaces AetherSDR's "lacking"
//                                    FreeDvClient with a freedv-gui-
//                                    faithful event handler that
//                                    preserves all 14 station fields
//                                    needed by FreeDVStationModel
//                                    (Phase 3J-2 Task D3). Dual-feed
//                                    behaviour (emit BOTH stationUpdated
//                                    and spotReceived from
//                                    onFreqChange / onRxReport) is a
//                                    NereusSDR architectural decision
//                                    per design doc Section 4 Flow 2 -
//                                    freedv-gui's onFrequencyChange
//                                    updates only the station map; the
//                                    NereusSDR client also synthesizes
//                                    a DxSpot for the panadapter
//                                    overlay. Test seam wrappers
//                                    (handleEngineIOForTest /
//                                    handleSocketIOForTest /
//                                    pingIntervalMsForTest /
//                                    lastSentMessageForTest) are 1-line
//                                    forwards exposed so the unit test
//                                    can drive the wire-protocol layer
//                                    without an actual WebSocket round-
//                                    trip. AI tooling: Anthropic Claude
//                                    Code.

#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <atomic>

#ifdef HAVE_WEBSOCKETS
#include <QWebSocket>
#else
class QWebSocket;
#endif
#include <QTimer>
#include <QAbstractSocket>

#include "DxSpot.h"
#include "FreeDVStation.h"

namespace NereusSDR {

// FreeDV Reporter Engine.IO / Socket.IO client.
//
// Wire-protocol behavior is byte-for-byte from freedv-gui. The handful
// of NereusSDR-side decisions are:
//   - View-only auth always (we are a spot-consumer, not a reporter
//     pushing TX/RX state). That maps to freedv-gui's
//     `isValidForReporting() == false` connect_() branch
//     (FreeDVReporter.cpp:295-298 [@77e793a]).
//   - Dual-feed: every freq_change / rx_report event ALSO synthesizes
//     a DxSpot for the SpotModel pipeline (panadapter overlay).
//     freedv-gui never produces spots; this is NereusSDR's
//     architectural decision per the Phase 3J-2 design doc Section 4
//     Flow 2.
class FreeDVReporterClient : public QObject {
    Q_OBJECT

public:
    explicit FreeDVReporterClient(QObject* parent = nullptr);
    ~FreeDVReporterClient() override;

    void startConnection();
    void stopConnection();
    bool isConnected() const { return m_connected.load(); }

    // Identity (set from AppSettings before connecting). View-only
    // clients can leave callsign / gridSquare / message empty; only
    // `version` and `serverUrl` are used in view mode.
    void setIdentity(const QString& callsign, const QString& gridSquare,
                     const QString& message, const QString& version);
    void setServerUrl(const QString& url);

    // Send-side (no-ops in view-only mode until 3M-x flips us to
    // reporter mode). Wire shapes from freedv-gui FreeDVReporter.cpp
    // requestQSY (:104-119) and updateMessage (:122-130).
    void requestQSY(const QString& targetSid, quint64 freqHz,
                    const QString& message);
    void updateMessage(const QString& message);

    // From freedv-gui src/reporting/FreeDVReporter.cpp:653-668 + 670-686 [@77e793a]
    // emit freq_change / tx_report Socket.IO events so the
    // reporter server lists our station's current freq and TX state.
    // Without these, NereusSDR shows as connected but never broadcasts
    // its operating frequency or transmit indicator — user bench-
    // reported 2026-05-11.
    void setFrequency(quint64 freqHz);
    void setTransmitting(bool tx, const QString& mode = QString());

    // From freedv-gui src/reporting/FreeDVReporter.cpp:167-185 [@77e793a]
    // (hideFromView / showOurselves) + :704-729 (hideFromViewImpl_ /
    // showOurselvesImpl_). Toggles the server-side "hidden" flag for our
    // station. When hidden, our row does not appear on the public
    // dashboard but our connection stays alive (we still receive the
    // station list). Emits Socket.IO "hide_self" or "show_self" events
    // with no payload. On show-after-hide, re-emits freq_change /
    // tx_report / message_update so the server knows our current state.
    // The hidden state is also re-asserted in onConnectionSuccessful
    // after a reconnect, matching freedv-gui FreeDVReporter.cpp:424-433
    // [@77e793a].
    void setHiddenFromView(bool hidden);

    // From freedv-gui src/reporting/FreeDVReporter.cpp:addReceiveRecord
    // [@77e793a]: emit a Socket.IO "rx_report" event so other operators
    // see that we're decoding their station. Carries the source
    // station's callsign, the mode we're decoding ("RADE"), and the
    // SNR we measured. Called whenever RadeChannel decodes a new
    // callsign from the EOO text channel + updates SNR.
    void sendRxReport(const QString& callsign,
                      const QString& mode, int snrDb);

    // Read-side query for FreeDVStationModel + tests.
    QHash<QString, FreeDVStation> stations() const { return m_stations; }

    QString logFilePath() const;

    // Test seams: NEREUS_TESTING-gated wrappers around private wire-
    // protocol methods. Production callers go through the QWebSocket
    // signal pipeline (`onWsTextMessage` -> `handleEngineIO` ->
    // `handleSocketIO`); tests bypass the socket and drive the parsers
    // directly so a real WebSocket round-trip is not required.
    int pingIntervalMsForTest() const { return m_pingIntervalMs; }
    QString lastSentMessageForTest() const { return m_lastSentForTest; }
    void clearLastSentForTest() { m_lastSentForTest.clear(); }
    void handleEngineIOForTest(const QString& msg) { handleEngineIO(msg); }
    void handleSocketIOForTest(const QString& msg) { handleSocketIO(msg); }

    // Identity accessors for the auto-start identity-aware tests. The
    // SpotHub Settings tab plus RadioModel::restoreSpotClientAutoStartState
    // resolve the operator's callsign / grid from User/* fallback chains
    // and call setIdentity() before startConnection(); tst_spot_auto_start
    // pins that round-trip via these accessors.
    QString callsignForTest() const { return m_callsign; }
    QString gridSquareForTest() const { return m_gridSquare; }

signals:
    // Connection lifecycle
    void connected();
    void disconnected();
    void connectionError(const QString& error);

    // Station model signals (drive FreeDVStationModel)
    void stationAdded(const QString& sid, const NereusSDR::FreeDVStation& info);
    void stationUpdated(const QString& sid, const NereusSDR::FreeDVStation& info);
    void stationRemoved(const QString& sid);

    // Spot signal (drives SpotModel via adapter; NereusSDR dual-feed)
    void spotReceived(const NereusSDR::DxSpot& spot);

    // Debug stream
    void rawLineReceived(const QString& line);

private slots:
    void onWsConnected();
    void onWsDisconnected();
    void onWsTextMessage(const QString& message);
    void onWsError(QAbstractSocket::SocketError err);
    void onReconnectTimer();

private:
    // Engine.IO / Socket.IO framing. See FreeDVReporter.cpp parser
    // body in freedv-gui [@77e793a] for the wire format authority;
    // FreeDvClient.cpp [@0cd4559] for the Qt-side Socket.IO state
    // machine these methods follow.
    void handleEngineIO(const QString& raw);
    void handleSocketIO(const QString& payload);
    void handleEvent(const QString& eventName, const QJsonObject& data);

    // Per-event handlers (one per Socket.IO event name freedv-gui
    // dispatches in connect_(). FreeDVReporter.cpp:361-369 [@77e793a]).
    void onNewConnection(const QJsonObject& data);
    void onFreqChange(const QJsonObject& data);
    void onRxReport(const QJsonObject& data);
    void onTxReport(const QJsonObject& data);
    void onRemoveConnection(const QJsonObject& data);
    void onMessageUpdate(const QJsonObject& data);
    void onConnectionSuccessful(const QJsonObject& data);
    void onBulkUpdate(const QJsonArray& pairs);

    // Spot synthesis (NereusSDR dual-feed; not present upstream).
    void emitSpotFromFreqChange(const QString& sid);
    void emitSpotFromRxReport(const QJsonObject& data);

    // Send helper (records the last message for the test seam, then
    // forwards to QWebSocket if a real socket is wired in HAVE_
    // WEBSOCKETS builds).
    void sendText(const QString& msg);

    QWebSocket*  m_ws{nullptr};
    QTimer*      m_pingTimer{nullptr};
    QTimer*      m_reconnectTimer{nullptr};
    QFile        m_logFile;

    QHash<QString, FreeDVStation> m_stations;  // sid -> station

    // 2026-05-12 (PR #238 bench follow-up): rx_report spot dedup.
    // Source-first from AetherSDR MainWindow.cpp:635-660 [@0cd4559]
    // (isDuplicateSpot lambda).  qso.freedv.org fans every TX out
    // to every receiver that decodes it, so a single VA3WTB
    // transmission to 20 receivers generated 20 rx_report events
    // -> 20 separate panadapter spots, all at the same freq.
    // Dedup by (dxCall, freqMhz within 1 Hz) within a 60 s window
    // so each TX produces at most one spot per minute regardless
    // of how many stations heard it.
    struct DedupEntry {
        double  freqMhz;
        qint64  addedMs;
    };
    QHash<QString, DedupEntry>    m_spotDedup;  // dxCall -> entry

    std::atomic<bool> m_connected{false};
    bool    m_intentionalDisconnect{false};
    int     m_reconnectAttempts{0};
    int     m_pingIntervalMs{25000};

    // Last-sent text for the test seam. We mirror the message here on
    // every sendText(); production callers ignore this field.
    QString m_lastSentForTest;

    // Identity (view-only mode leaves these mostly empty).
    QString m_callsign;
    QString m_gridSquare;
    QString m_statusMessage;
    QString m_version;
    QString m_serverUrl;

    // Cached last-emitted state. freedv-gui FreeDVReporter.cpp:667 / :684-685
    // / :701 [@77e793a] saves these in freqChangeImpl_ / transmitImpl_ /
    // sendMessageImpl_ so onFreeDVReporterConnectionSuccessful_ at :430-432
    // can re-push them after a reconnect (or after show_self toggles us
    // back from hidden).
    quint64 m_lastFrequency{0};
    QString m_lastTxMode;
    bool    m_lastTxState{false};
    bool    m_hiddenFromView{false};

    // Defaults from freedv-gui (FreeDVReporter.h:51 [@77e793a]) and
    // AetherSDR (FreeDvClient.h:81-84 [@0cd4559]).
    static constexpr const char* DefaultWsUrl =
        "wss://qso.freedv.org/socket.io/?EIO=4&transport=websocket";
    static constexpr int MaxReconnectDelayMs     = 60000;
    static constexpr int InitialReconnectDelayMs = 5000;
};

} // namespace NereusSDR
