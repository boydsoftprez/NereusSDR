#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/RoundtableModel.h  (NereusSDR)
// =================================================================
//
// What the Roundtable screens bind to, the same in every placement
// (design 10.3):
//
//   Everything (a desktop running its own radio, and the Core itself):
//     attachLocal() feeds it from the Core's RoundtableClient (the band
//     list and room views as the server sends them) and sends its actions
//     to the Core's RoundtableState.
//   A window of a Core: attachStation() sends its actions as the
//     roundtable.* verbs; StationClient feeds it the `roundtableRooms` and
//     `roundtableRoom:<id>` record streams (applyRecordBatch) and the
//     verbs' answers.
//
// Either way the `roundtable` object (RoundtableState: the link state,
// first-run answer, Hide my station, mention counts and slice rooms) is
// read from the RoundtableState it is given.
//
// rooms() is the band list's summaries in ascending order of center
// frequency. room() is one room in view: {id, band, family, hz, members
// (joining order), now (open turns, oldest first), earlier (ended turns,
// newest first), messages (the last 50, oldest first)}; each turn's names
// are listed by count, highest first, then by name. The Core builds the
// streams from the same two readers (roomsRecords, roomRecords), so both
// placements hold equal rooms.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 11).
//   2026-10-24: Roundtable plan Task 12: available(), muteList(), the
//               Core's Roundtable settings (stationSetting,
//               setStationSetting) and settingChanged for the Spot Hub
//               tab. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-24: Roundtable plan Task 13: earlier turns of the last 30
//               minutes only (wire spec 5.4.6: receivers drop the rest
//               themselves), with a clock tests can set. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-24: Roundtable plan Task 13: recentNames(), the names used on
//               a room's frequency in the last 24 hours (design 8.6), for
//               naming a turn. J.J. Boyd (KG4VCF), AI-assisted via Claude
//               Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/roundtable/RoundtableClient.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <functional>

namespace NereusSDR {

class IStationLink;
class RoundtableState;
struct RecordBatch;

class NEREUS_CORE_EXPORT RoundtableModel : public QObject {
    Q_OBJECT

public:
    using LinkState = RoundtableClient::LinkState;

    /// The link's streams (design 10.3).
    static constexpr const char* kRoomsStream = "roundtableRooms";
    static constexpr const char* kRoomStreamPrefix = "roundtableRoom:";
    static constexpr int kRoomsCapacity = 500;
    static constexpr int kRoomCapacity = 400;
    /// Wire spec 5.4.5: a room view holds the last 50 messages.
    static constexpr int kRoomMessages = 50;
    /// Wire spec 5.4.6: a room view's earlier turns are those that ended in
    /// the last 30 minutes; a turn that leaves the window is not announced,
    /// receivers drop it themselves.
    static constexpr qint64 kEarlierWindowMs = 30LL * 60LL * 1000LL;
    /// Design 8.6: names to name a turn with are picked from those used on
    /// that frequency in the last 24 hours.
    static constexpr qint64 kRecentNamesWindowMs = 24LL * 60LL * 60LL * 1000LL;

    /// The Unix time in ms the model reads (the wall clock unless a test
    /// set its own).
    static qint64 nowMs();
    /// A test's clock; an empty function restores the wall clock.
    static void setClockForTest(std::function<qint64()> clock);

    static QString roomStreamName(const QString& roomId);
    /// The room id of a `roundtableRoom:<id>` stream, else empty.
    static QString roomIdOfStream(const QString& stream);
    /// The `roundtableRooms` records for rooms(): id the room id, fields
    /// {kind: "room", ...summary}.
    static QHash<QString, QJsonObject> roomsRecords(const QJsonArray& rooms);
    /// The `roundtableRoom:<id>` records for room(): member:<ref>,
    /// reading:<ref>, turn:<id>, name:<turn id>:<name> and message:<id>.
    static QHash<QString, QJsonObject> roomRecords(const QJsonObject& room);

    /// The Core's Roundtable settings a Spot Hub tab writes as settings
    /// (design 10.3): AutoStart, AwayChoice, HighlightMentions, Callsign
    /// and GridSquare, each under `Roundtable/`. FirstRunAnswer, Hidden,
    /// MuteList and MentionsSeenMs change only through the actions below.
    static QStringList stationSettingNames();
    /// The default each reads as while unset: AutoStart "True",
    /// AwayChoice "Report", HighlightMentions "True", the overrides "".
    static QString stationSettingDefault(const QString& name);

    explicit RoundtableModel(QObject* parent = nullptr);
    ~RoundtableModel() override;

    /// Everything placement: fed by `client`, actions to `state` (the
    /// Core's, bound to the same client). Neither is owned.
    void attachLocal(RoundtableClient* client, RoundtableState* state);
    /// Window placement: actions as the roundtable.* verbs over `link`,
    /// the `roundtable` object in `state`. Neither is owned.
    void attachStation(IStationLink* link, RoundtableState* state);
    /// Neither: no rooms, actions refused.
    void detach();

    // ---- The server's views (Everything placement; public for tests).
    void applyRooms(const QJsonObject& full);
    void applyRoomsDelta(const QJsonObject& delta);
    void applyRoom(const QJsonObject& full);
    void applyRoomDelta(const QJsonObject& delta);

    // ---- The Core's streams and answers (window placement).
    /// One `roundtableRooms` or `roundtableRoom:<id>` batch; other streams
    /// are ignored (false).
    bool applyRecordBatch(const RecordBatch& batch);
    void applyPostResult(const QString& clientId, bool ok, const QString& messageIdOrReason);
    void applyWebLink(const QUrl& url);
    /// A verb the Core refused (other than a post, which answers through
    /// postResult).
    void applyRefusal(const QString& verb, const QString& reason);
    /// The window's link ended: no rooms in view.
    void clearStationRooms();

    // ---- Readers.
    QJsonArray rooms() const;
    /// Empty when the room is not in view.
    QJsonObject room(const QString& roomId) const;
    QStringList viewedRoomIds() const;
    QString sliceRoomId(int sliceId) const;
    /// The room this window reads (readRoom), or empty.
    QString readingRoomId() const { return m_readingRoomId; }
    LinkState linkState() const;
    qint64 linkSince() const;
    bool firstRunAnswered() const;
    bool hidden() const;
    int mentionCount(const QString& roomId) const;
    bool muted(const QString& callsign) const;
    /// The base callsigns muted on the Core, in the order they were muted.
    QStringList muteList() const;
    /// Whether the actions reach a Roundtable: the Core's own client, or a
    /// window's link to a Core that offers Roundtable.
    bool available() const;
    /// `Roundtable/<name>` as the Core holds it (a window reads the Core's
    /// value through the settings it shares), or its default while unset.
    QString stationSetting(const QString& name) const;
    /// Writes `Roundtable/<name>`, one of stationSettingNames(), through
    /// the settings path (a window's write goes to the Core). False for
    /// any other name.
    bool setStationSetting(const QString& name, const QString& value);
    /// A setting changed elsewhere (a window hears the Core's changes
    /// through RadioModel::reportStationSettingChanged). Emits
    /// settingChanged for a `Roundtable/` key, or for an empty key (a
    /// whole snapshot).
    void noteSettingChanged(const QString& key);
    /// What this window has typed in a room's box (not kept).
    QString draft(const QString& roomId) const { return m_drafts.value(roomId); }
    void setDraft(const QString& roomId, const QString& text);

    // ---- Actions (the verbs). A refusal arrives as actionRefused, a
    // post's answer as postResult, the web sign-in link as webLinkReady.
    void post(const QString& roomId, const QString& text, const QString& clientId);
    void deleteMessage(const QString& messageId);
    void nameTurn(const QString& turnId, const QString& name);
    void report(const QString& kind, const QString& target, const QString& reason);
    void readRoom(const QString& roomId);
    /// Stops reading the room readRoom() opened.
    void unreadRoom();
    void setHidden(bool hidden);
    void answerFirstRun(bool live);
    void mute(const QString& callsign);
    void unmute(const QString& callsign);
    void markMentionRead(qint64 upToMs);
    void requestWebLink();
    void deleteArchive(const QString& confirm);

    /// Drops every ended turn older than kEarlierWindowMs from the rooms in
    /// view and emits roomChanged for each room that lost one. Runs on its
    /// own when the oldest earlier turn ages out.
    void pruneEarlierTurns();

    /// The names used on a room's frequency in the last 24 hours (design
    /// 8.6), newest first: the callsigns of its known talkers and the names
    /// given to its turns, as this model has seen them, upper case, one per
    /// base callsign (design 7.1). The list outlives the 30-minute earlier
    /// turns and a room that closed and reopened on the same center (the
    /// same room id). A turn still open counts as used now.
    QStringList recentNames(const QString& roomId) const;

signals:
    void roomsChanged();
    void roomChanged(const QString& roomId);
    void linkChanged();
    void firstRunChanged();
    void mentionsChanged();
    void sliceRoomsChanged();
    void readingRoomChanged();
    void postResult(const QString& clientId, bool ok, const QString& messageIdOrReason);
    void webLinkReady(const QUrl& url);
    /// `verb` is the roundtable.* verb's name after the dot ("delete").
    void actionRefused(const QString& verb, const QString& reason);
    /// A `Roundtable/` setting changed (empty: possibly all of them).
    void settingChanged(const QString& key);

private:
    struct RoomView {
        QHash<QString, QJsonObject> members;   // ref to member, with "seq"
        qint64 nextSeq = 0;
        QHash<qint64, QJsonObject> turns;      // id to turn
        QHash<qint64, QJsonObject> messages;   // id to message
    };

    void connectState(RoundtableState* state);
    void onStateChanged();
    void upsertMember(RoomView& view, const QJsonObject& member);
    void trimMessages(RoomView& view);
    void rebuildRoomsFromRecords();
    void rebuildRoomFromRecords(const QString& roomId);
    void pushRoomsToState();
    void refuse(const QString& verb, const QString& reason);
    bool stationOutcome(const QString& verb, bool sent, const QString& reason);
    static bool pruneView(RoomView& view, qint64 cutoffMs);
    void noteRecentNames(const QString& roomId, const RoomView& view);
    void schedulePrune();

    QPointer<RoundtableClient> m_client;
    QPointer<RoundtableState> m_state;
    IStationLink* m_link = nullptr;
    QList<QMetaObject::Connection> m_connections;

    QHash<QString, QJsonObject> m_summaries;
    QHash<QString, RoomView> m_views;
    // Window placement: each stream's records as the Core sent them.
    QHash<QString, QHash<QString, QJsonObject>> m_records;

    QString m_readingRoomId;
    QHash<QString, QString> m_drafts;

    // The `roundtable` object as last seen, for the change signals.
    LinkState m_seenLinkState = LinkState::Off;
    qint64 m_seenLinkSince = 0;
    bool m_seenFirstRun = false;
    bool m_seenHidden = false;
    QString m_seenMentions;
    QString m_seenSliceRooms;

    // The AppSettings write observer feeding settingChanged.
    int m_settingsObserver = -1;

    // recentNames(): room id to base callsign to {name, last used ms}.
    struct RecentName {
        QString name;
        qint64 usedMs = 0;
    };
    QHash<QString, QHash<QString, RecentName>> m_recentNames;

    // Fires when the oldest earlier turn leaves the 30-minute window.
    QTimer m_pruneTimer;
};

} // namespace NereusSDR
