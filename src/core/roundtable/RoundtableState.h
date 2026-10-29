#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/roundtable/RoundtableState.h  (NereusSDR)
// =================================================================
//
// The station link's `roundtable` object (design 10.3): what every device
// on the station reads about the Core's Roundtable connection, the same on
// each. Read-only on the link; the roundtable.* verbs change it.
//
//   linkState         RoundtableClient's link state
//   linkSince         when it last changed (Unix ms; 0 before the first)
//   firstRunAnswered  someone answered the first-run question (D25, U5)
//   hidden            Hide my station (D2, D21)
//   mentionCounts     a compact JSON object, room id to the number of
//                     messages naming this station posted after the
//                     mention read mark (U6); rooms with none are left out
//   sliceRooms        a compact JSON object, slice id to the room id the
//                     slice is in, or null while it is in no listed room
//
// On the Core it is also the Roundtable controller the verbs reach: it
// keeps the Roundtable/* answers in AppSettings (FirstRunAnswer, Hidden,
// MuteList, MentionsSeenMs) and feeds them to RoundtableClient when bound
// and whenever they change, so the Core stays read-only until the
// first-run answer (D25). In a window it holds the Core's values
// (applyStationValue) and is never bound.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 11: the `roundtable`
//               object and the Core's Roundtable answers).
//   2026-10-24: Task 11 fix round: isClientId() public. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: readsSettings(), so only the station server whose
//               settings the state reads hands it back. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableWire.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <utility>

namespace NereusSDR {

class AppSettings;

class NEREUS_CORE_EXPORT RoundtableState : public QObject {
    Q_OBJECT
    Q_PROPERTY(NereusSDR::RoundtableClient::LinkState linkState READ linkState NOTIFY stateChanged)
    Q_PROPERTY(qint64 linkSince READ linkSince NOTIFY stateChanged)
    Q_PROPERTY(bool firstRunAnswered READ firstRunAnswered NOTIFY stateChanged)
    Q_PROPERTY(bool hidden READ hidden NOTIFY stateChanged)
    Q_PROPERTY(QString mentionCounts READ mentionCounts NOTIFY stateChanged)
    Q_PROPERTY(QString sliceRooms READ sliceRooms NOTIFY stateChanged)

public:
    using LinkState = RoundtableClient::LinkState;
    using Clock = std::function<qint64()>;

    /// The Roundtable/* keys kept on the Core (design 10.1).
    static constexpr const char* kFirstRunAnswerKey = "Roundtable/FirstRunAnswer";
    static constexpr const char* kHiddenKey = "Roundtable/Hidden";
    static constexpr const char* kMuteListKey = "Roundtable/MuteList";
    static constexpr const char* kMentionsSeenKey = "Roundtable/MentionsSeenMs";

    explicit RoundtableState(QObject* parent = nullptr);
    ~RoundtableState() override;

    /// A property write's refusal: the object changes only by its verbs.
    static QString readOnlyReason();
    /// The answer to a verb that needs the station connection while there
    /// is none (not started, connecting, hidden or read only).
    static QString notConnectedReason();
    /// The answer to a request the Core could not read.
    static QString unreadableReason();
    /// Wire spec 5.2: a post's clientId, 1 to 64 bytes of printable ASCII.
    static bool isClientId(const QString& id);

    LinkState linkState() const { return m_linkState; }
    qint64 linkSince() const { return m_linkSince; }
    bool firstRunAnswered() const { return m_firstRunAnswered; }
    bool hidden() const { return m_hidden; }
    QString mentionCounts() const;
    QString sliceRooms() const;
    QJsonObject mentionCountsObject() const { return m_mentionCounts; }
    QJsonObject sliceRoomsObject() const { return m_sliceRooms; }
    int mentionCount(const QString& roomId) const;
    /// The room the slice is in, or empty.
    QString sliceRoomId(int sliceId) const;
    /// The base callsigns muted on the Core (Roundtable/MuteList), read
    /// from the settings, which a window shares with its Core.
    QStringList muteList() const;
    bool muted(const QString& callsign) const;

    // ---- The Core.

    /// Binds the Core's client and the settings its answers live in (not
    /// owned), and feeds the client the first-run answer, Hide my station
    /// and the mention read mark at once.
    void bindClient(RoundtableClient* client, AppSettings* settings);
    bool isBound() const { return !m_client.isNull(); }
    RoundtableClient* client() const { return m_client.data(); }
    /// Whether the state reads its answers from `settings` (the one
    /// bindClient was given): a station server going away hands the state
    /// back only when it is that server's.
    bool readsSettings(const AppSettings* settings) const { return m_settings == settings; }
    /// The Core's slices, active first, as RoundtableClient gets them.
    void setSlices(const QList<RoundtableSliceState>& slices);
    /// The band list's summaries (the rooms that are listed now).
    void setRooms(const QJsonArray& summaries);
    /// Whether `roomId` is a room in the band list.
    bool isListedRoom(const QString& roomId) const;

    // The verbs (each answers true, or false with plain words in *reason).
    bool post(const QString& roomId, const QString& text, const QString& clientId,
              QString* reason);
    bool deleteMessage(const QString& messageId, QString* reason);
    bool nameTurn(const QString& turnId, const QString& name, QString* reason);
    bool report(const QString& kind, const QString& target, const QString& reason,
                QString* refusal);
    bool readRoom(const QString& roomId, QString* reason);
    void unreadRoom(const QString& roomId);
    bool setOperatorHidden(bool hidden, QString* reason);
    bool answerFirstRun(bool live, QString* reason);
    bool mute(const QString& callsign, QString* reason);
    bool unmute(const QString& callsign, QString* reason);
    bool markMentionRead(qint64 upToMs, QString* reason);
    bool requestWebLink(QString* reason);
    bool deleteArchive(const QString& confirm, QString* reason);

    /// Wall clock (Unix ms) in place of the system's.
    void setClockForTest(Clock clock) { m_clock = std::move(clock); }

    // ---- A window.

    /// The Core's value of one property (StationClient).
    bool applyStationValue(const QByteArray& propertyName, const QVariant& value);
    /// Back to nothing known (a window whose link ended).
    void clearStationValues();

signals:
    void stateChanged();

private:
    struct Mention {
        QString room;
        qint64 t = 0;
        QString sender;
    };

    qint64 now() const;
    AppSettings* settings() const;
    void reloadSettings();
    void onLinkState(LinkState state);
    void onMention(const QJsonObject& mention);
    void recount();
    void replace();
    bool requireClient(QString* reason) const;
    bool requireLive(QString* reason) const;

    QPointer<RoundtableClient> m_client;
    AppSettings* m_settings = nullptr;
    int m_settingsObserver = -1;
    Clock m_clock;

    LinkState m_linkState = LinkState::Off;
    qint64 m_linkSince = 0;
    bool m_firstRunAnswered = false;
    bool m_hidden = false;
    QJsonObject m_mentionCounts;
    QJsonObject m_sliceRooms;

    // The Core's own inputs.
    QList<RoundtableSliceState> m_slices;
    QHash<QString, RoundtableWire::ListedRoom> m_rooms;
    QHash<qint64, Mention> m_mentions;
    qint64 m_mentionsSeenMs = 0;
    QStringList m_muteList;
};

} // namespace NereusSDR
