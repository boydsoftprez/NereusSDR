#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/roundtable/RoundtableClient.h  (NereusSDR)
// =================================================================
//
// The Core's connection to Roundtable (docs/architecture/2026-10-23-
// roundtable-v1.md; design sections 7.1, 7.2, 8.3, 10.1, 10.2, C3, C22,
// C24, C25 and D25). It lives on the main thread with the models and never
// touches the audio thread.
//
// Two ways in, one at a time:
//
//   Station (`/v1/station`): signs in with the Core's station identity key
//     over "NereusSDR roundtable hello v1\n" || host || "\n" || nonce
//     (4.3), then sends presence on every change (at least 1100 ms apart)
//     and every 10 s, and a reading once a second for each listed slice
//     that is not keyed, only when that slice's meter has a new reading.
//
//   Live reader (`/v1/live`): read only. Used until the first-run question
//     is answered (setFirstRunAnswered), while hidden, and while nobody is
//     at the radio when the operator chose to stop then (setStopWhenAway).
//     It sends `watchBand` `all` and `watch` for the rooms read, and
//     nothing about the station (D25). Going hidden from live sends
//     `hidden` last, closes, and reopens as a live reader.
//
// The archive while reading live (requestWebLink, deleteArchive) opens a
// brief station connection beside it that sends `hello` and then only
// `weblink` or `deleteArchive`, never `presence`, and closes once answered
// (7.8).
//
// A lost connection is retried after 1, 2, 5, 10 and 30 s, then every
// 30 s; no frame for 45 s counts as lost (2.3). A sign-in refused (4001),
// a protocol error (4002), a newer connection of this station (4003) and a
// revoked key (4004) stop the retries, with the reason in statusText(),
// until the callsign, the key, the server or start() changes (2.4).
//
// Every message is checked against the wire tables before it is sent, so
// the Core never sends one the server would close the connection for. A
// test run (QStandardPaths test mode) never contacts a server that is not
// on this computer. Nothing secret is logged: no key, nonce, signature or
// message text, and no station id beyond its first 6 characters.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10: R-RT-6, R-RT-12,
//               R-RT-13, R-RT-14 Core side).
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/roundtable/RoundtableSignalFloor.h"
#include "core/roundtable/RoundtableWire.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QTimer;
class QWebSocket;

namespace NereusSDR {

class StationIdentity;

class NEREUS_CORE_EXPORT RoundtableClient : public QObject {
    Q_OBJECT

public:
    enum class LinkState { Off, Connecting, Live, Reconnecting, Offline, Hidden };
    Q_ENUM(LinkState)

    using Clock = std::function<qint64()>;
    using IdentityProvider = std::function<const StationIdentity*()>;

    /// Design 8.3 and wire spec 6.5.
    static constexpr int kPresenceSpacingMs = 1100;
    static constexpr int kPresenceRepeatMs = 10000;
    static constexpr int kReadingIntervalMs = 1000;
    /// Wire spec 2.3: no frame for this long and the connection is lost.
    static constexpr int kSilenceMs = 45000;
    /// The server pings every 20 s, but a WebSocket stack does not report
    /// the pings it answers, so the Core pings too and counts the pongs.
    static constexpr int kPingIntervalMs = 15000;
    /// How long a brief archive connection may take to be answered.
    static constexpr int kArchiveTimeoutMs = 30000;
    static constexpr int kTickMs = 100;
    /// The retry ladder (2.3, design 10.1); the last step repeats.
    static const QList<int>& reconnectDelaysMs();
    static QUrl defaultServerUrl();

    /// The profile directory a desktop's station key lives in: the one the
    /// settings file is in, as Remote Access uses it, so both find one key.
    static QString profileDirectoryFor(const QString& settingsFilePath);

    explicit RoundtableClient(QObject* parent = nullptr);
    ~RoundtableClient() override;

    /// wss://, or ws:// to this computer only. Default
    /// wss://roundtable.nereussdr.com/v1/station.
    void setServerUrl(const QUrl& url);
    QUrl serverUrl() const { return m_serverUrl; }
    /// The live reader's path on the same server.
    QUrl liveUrl() const;

    /// Not owned. setIdentityProvider() asks for the key only when a
    /// station connection opens, so a key file is made only when needed.
    void setIdentity(const StationIdentity* identity);
    void setIdentityProvider(IdentityProvider provider);

    /// The callsign must pass RoundtableWire::isCallsign for a station
    /// connection; the grid is sent as RoundtableWire::gridToSend gives it.
    void setStationInfo(const QString& callsign, const QString& grid);
    /// Active slice first. Slices in a mode with no family are left out of
    /// presence; at most 4 are listed.
    void setSlices(const QList<RoundtableSliceState>& slices);
    /// One meter reading for `sliceId`, taken now (the wire time is this
    /// computer's clock at this call).
    void pushMeter(int sliceId, double dbm, qint64 meterReadingNs);
    void setAtRadio(bool atRadio);
    void setHidden(bool hidden);
    void setFirstRunAnswered(bool answered);
    /// `Roundtable/AwayChoice` "Stop": nobody at the radio reads live only.
    void setStopWhenAway(bool stop);

    void start();
    void stop();
    LinkState linkState() const { return m_state; }
    /// The connection line in operator words (empty when Off).
    QString statusText() const;
    /// This station's Roundtable id, from the last welcome.
    QString stationId() const { return m_stationId; }

    // ---- Station actions.
    void post(const QString& roomId, const QString& text, const QString& clientId);
    void deleteMessage(const QString& messageId);
    void nameTurn(const QString& turnId, const QString& name);
    /// `kind` is message, turn or station; `target` is the message or turn
    /// id, or "<room id>/<row ref>" for a station; an empty `reason` is
    /// sent as null.
    void report(const QString& kind, const QString& target, const QString& reason);
    void readRoom(const QString& roomId);
    void unreadRoom(const QString& roomId);
    void markMentionsSeen(qint64 upToMs);
    void requestWebLink();
    void deleteArchive();

    // ---- Tests.
    /// Wall clock (Unix ms) and a monotonic clock (ms) in place of the
    /// system's. Timers run from serviceTimers(), which a 100 ms timer
    /// calls; a test that moves its clock calls it directly.
    void setClocksForTest(Clock wallMs, Clock monotonicMs);
    void serviceTimers();
    /// The monotonic time the next retry is due, or -1 for none.
    qint64 reconnectDueMsForTest() const { return m_reconnectDueMs; }

signals:
    void linkStateChanged(NereusSDR::RoundtableClient::LinkState state);
    void roomsReceived(const QJsonObject& full);
    void roomsDeltaReceived(const QJsonObject& delta);
    void roomReceived(const QJsonObject& full);
    void roomDeltaReceived(const QJsonObject& delta);
    void postResult(const QString& clientId, bool ok, const QString& messageIdOrReason);
    /// {"type":"mention","room":...,"message":{...}}.
    void mentionReceived(const QJsonObject& message);
    void webLinkReady(const QUrl& url);
    void noticeReceived(const QString& text);
    void errorReceived(const QString& code, const QString& reason);
    /// statusText() may have changed.
    void statusTextChanged();

private:
    enum class Leg { Station, Live, Archive };
    enum class Want { None, Station, Live };

    struct Link {
        QPointer<QWebSocket> socket;
        Leg leg = Leg::Station;
        QUrl url;
        quint64 generation = 0;
        bool open = false;
        bool signedIn = false;
        qint64 openedMs = 0;
        qint64 lastFrameMs = 0;
        qint64 lastPingMs = 0;
        QString errorCode;
        QString errorReason;
        qint64 retryAfterMs = 0;
    };

    struct Meter {
        RoundtableSignalFloor floor;
        double dbm = 0.0;
        qint64 stampNs = -1;
        qint64 wallMs = 0;
        bool present = false;
        qint64 sentStampNs = -1;
        qint64 lastSentMs = 0;
        bool sentOnce = false;
    };

    enum class ArchiveAction { WebLink, DeleteArchive };

    qint64 wallNow() const;
    qint64 monoNow() const;
    Want wanted() const;
    void reconcile();
    void openLink(Link& link, Leg leg);
    void closeLink(Link& link, bool polite);
    void onConnected(Leg leg, quint64 generation);
    void onDisconnected(Leg leg, quint64 generation);
    void onText(Leg leg, quint64 generation, const QString& text);
    void onMainClosed(int closeCode);
    void onArchiveClosed();
    void handleStation(const QJsonObject& message);
    void handleLive(const QJsonObject& message);
    void handleArchive(const QJsonObject& message);
    void handleViews(const QString& type, const QJsonObject& message);
    void handleError(Link& link, const QJsonObject& message);
    void answerChallenge(Link& link, const QJsonObject& message);
    bool send(Link& link, const QJsonObject& message);
    bool stationLive() const;
    void sendPresenceIfDue(qint64 now);
    void sendReadingsIfDue(qint64 now);
    QList<RoundtableSliceState> presenceSlices() const;
    void scanForMentions(const QString& roomId, const QJsonArray& messages);
    void emitMention(const QString& roomId, const QJsonObject& message);
    void runArchive(ArchiveAction action);
    void flushQueuedArchive(Link& link, QList<ArchiveAction>& queue);
    void sendArchiveAction(Link& link, ArchiveAction action);
    void finishArchiveIfAnswered();
    void scheduleReconnect(qint64 atLeastMs);
    void goOffline(const QString& reason);
    void clearOffline();
    void setState(LinkState state);
    const StationIdentity* identity() const;

    QUrl m_serverUrl;
    const StationIdentity* m_identity = nullptr;
    IdentityProvider m_identityProvider;
    QString m_software;
    QString m_softwareVersion;

    QString m_callsign;
    QString m_grid;
    QList<RoundtableSliceState> m_slices;
    QHash<int, Meter> m_meters;
    bool m_atRadio = true;
    bool m_hidden = false;
    bool m_answered = false;
    bool m_stopWhenAway = false;
    bool m_started = false;

    Link m_main;
    Link m_archive;
    quint64 m_nextGeneration = 0;
    LinkState m_state = LinkState::Off;
    QString m_offlineReason;
    bool m_offline = false;
    int m_attempt = 0;
    qint64 m_reconnectDueMs = -1;
    bool m_openQueued = false;

    // Presence (station connection).
    bool m_presenceDirty = true;
    bool m_presenceSent = false;
    qint64 m_lastPresenceMs = 0;
    qint64 m_presenceNotBeforeMs = 0;
    // The slices the server last heard, id to keyed.
    QHash<int, bool> m_sentSlices;

    // Views.
    qint64 m_roomsGen = 0;
    bool m_awaitingRooms = false;
    QHash<QString, qint64> m_roomGens;
    QSet<QString> m_awaitingRoom;
    QStringList m_reads;

    // Mentions.
    QSet<qint64> m_mentionIds;
    qint64 m_mentionsSeenMs = -1;

    // Archive.
    QList<ArchiveAction> m_mainQueue;      // waiting for the station sign-in
    QList<ArchiveAction> m_archiveQueue;   // waiting for the archive sign-in
    bool m_archiveErrored = false;
    int m_archiveUnanswered = 0;           // sent on the archive link

    QString m_stationId;
    QTimer* m_tick = nullptr;
    Clock m_wallClock;
    Clock m_monoClock;
    qint64 m_monoBase = 0;
};

} // namespace NereusSDR
