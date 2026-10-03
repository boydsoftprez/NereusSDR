#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/DeviceSessionRegistry.h  (NereusSDR)
// =================================================================
//
// Who holds a place on the Core (iPhone app plan Task 71, R-IOS-02; the
// several-devices design, docs/architecture/2026-09-24-several-devices-on-
// one-core-design.md, sections 4.1 to 4.6, rulings 4.1 to 4.12).
//
// Up to four devices hold sessions on one Core at the same time. This class
// is the record of them and the rules that decide who is let in; it owns no
// socket and sends nothing. StationServer asks it after every accepted
// sign-in and tells it when a session ends.
//
// ---- What a device is (ruling 4.1) ----
//
//   - a paired device, by its device key's id (raw, 32 bytes);
//   - a window signed in with the older token and no key: a device for the
//     life of its session, id "token:<n>" (nextTokenDeviceId()), named
//     "Computer at <address>" (tokenWindowName()), never recognised when it
//     comes back, so it has no grace period;
//   - the station device of a desktop that hosts the Core
//     (registerHostingDevice()): that desktop's own window, which has no
//     network session and still takes one of the four places. On a Core
//     with no desktop there is none, and it takes no place.
//
// One session per device (ruling 4.2).
//
// ---- Admission (rulings 4.4, 4.5, 4.8) ----
//
// admit() decides, in this order: a device that already holds a place,
// live or away, is SameDevice (the new session replaces the older one at
// once, keeping its place); with a place free it is Admitted; otherwise
// Full. Places are admitted sessions, devices away in their grace period
// and a hosting desktop's own window (kMaxDeviceSessions). Nothing here
// ever ends another device's session: preemption is gone.
//
// ---- Away (rulings 4.10, 4.11) ----
//
// A paired device whose session ends without leaving on purpose is away
// for kGraceMs (180 s), keeping its place. expireAway() frees the places of
// devices whose time has run out and records that it ran out, kept until
// the device is next admitted, is revoked (remove()), or the Core restarts
// (this object is rebuilt). A token window, and a device that leaves on
// purpose (session.leave, ruling 4.12), frees its place at once.
//
// ---- Names and short names (ruling 4.3) ----
//
// numberNames() is the one numbering rule, used for both the `devices`
// object and `connectedDevices`: in the order given (paired devices in
// pairing order, then a hosting desktop the store does not hold, then token
// windows in the order they connected), a name already taken gets the next
// free number (" 2", " 3", ...). Short names are numbered on their own
// collisions by the same rule. A missing or unusable short name is the
// device's kind in plain words (kindWord()). Names and short names are the
// operator's words: they are validated as DeviceStore validates a name,
// never held to the Core's wording rules.
//
// ---- Clock ----
//
// Every time here is the Core's monotonic clock in milliseconds (setClock;
// the default is a steady clock started with the object). No wall-clock time
// is read, so a Core whose clock is wrong or moves still counts durations
// right (ruling 10.3).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 71 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02): graceEnded, for what
//               the end of a device's 180 s does to its slices. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 8: each away period's generation
//               (awayGeneration), carried by graceEnded and checked by
//               isCurrentAbsence, so an old expiry never acts on a device
//               that came back. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace NereusSDR {

class DeviceSessionRegistry : public QObject {
    Q_OBJECT

public:
    /// D44: devices that hold a place at once (admitted sessions, devices
    /// away in their grace period and a hosting desktop's own window).
    static constexpr int kMaxDeviceSessions = 4;
    /// D62: how long a dropped device keeps its place (`graceMs`).
    static constexpr qint64 kGraceMs = 180000;
    /// How often, at most, a device's reported last activity moves: the
    /// list is not re-sent to every other device each time one tunes.
    static constexpr qint64 kActivityResolutionMs = 60000;

    using Clock = std::function<qint64()>;

    enum class State {
        Listening,  ///< a live session
        Away,       ///< dropped without leaving, within its grace period
    };

    /// What kind of device an entry is (ruling 4.1).
    enum class Kind {
        Paired,   ///< a paired device, by its key's id
        Token,    ///< a window signed in with the older token and no key
        Hosting,  ///< the station device: a hosting desktop's own window
    };

    struct Entry {
        /// A paired device's raw id, "token:<n>" for a token window, or the
        /// hosting desktop's own id.
        QByteArray deviceId;
        Kind kind = Kind::Paired;
        /// The device's own words, as it signed in (before numbering).
        QString name;
        QString shortName;
        /// "phone", "tablet", "computer", or "station" for the hosting
        /// desktop's window.
        QString deviceKind;
        State state = State::Listening;
        /// Admission order; a device keeps it through a same-device return.
        quint64 order = 0;
        qint64 connectedSinceMs = 0;
        qint64 awaySinceMs = 0;
        /// The last activity the list reports (at most once a minute) and
        /// the true one.
        qint64 reportedActivityMs = 0;
        qint64 lastActivityMs = 0;
        /// The live session (a connection); null while away and for the
        /// hosting desktop's window.
        const QObject* session = nullptr;
        /// Slice control plan Task 8: this away period's generation, a new
        /// one each time the device drops; 0 while listening.
        quint64 awayGeneration = 0;
    };

    enum class Admission {
        Admitted,    ///< a place was free
        SameDevice,  ///< the device held a place, live or away (ruling 4.8)
        Full,        ///< every place is taken
    };

    struct AdmitResult {
        Admission admission = Admission::Full;
        /// SameDevice: the older live session the new one replaces, which
        /// the caller ends with sameDevice; null when the device was away.
        const QObject* replacedSession = nullptr;
        /// Admitted after the device's time ran out (read by Task 74's
        /// graceEnded); the record is cleared by this admission.
        std::optional<qint64> timeRanOutAtMs;
        struct TakenPlace {
            QByteArray byId;
            QString byName;
            qint64 atMs = 0;
        };
        std::optional<TakenPlace> placeTaken;
    };

    /// How a session ended.
    enum class EndKind {
        Dropped,  ///< a lost link, the heartbeat, a closed socket: away
        Left,     ///< session.leave: no away state
    };

    explicit DeviceSessionRegistry(QObject* parent = nullptr);

    /// The Core's monotonic clock in milliseconds. Tests inject theirs.
    void setClock(Clock clock);
    qint64 now() const;

    /// "token:<n>", a new one each call (never reused while this object
    /// lives).
    QByteArray nextTokenDeviceId();

    /// Admits `device` (its deviceId, kind, name, shortName and deviceKind
    /// are read) on `session`, as the rules above say. Away devices whose
    /// time has run out are expired first. On SameDevice the device keeps
    /// its place and order and takes the new session, name and short name.
    AdmitResult admit(const Entry& device, const QObject* session);

    /// The session `session` of `deviceId` ended. Dropped leaves a paired
    /// device away (its place kept); a token window, and a device that
    /// Left, frees its place at once. Nothing when `session` is not the
    /// device's current session (it was already replaced).
    void sessionEnded(const QByteArray& deviceId, const QObject* session, EndKind kind);

    /// Frees `deviceId`'s place at once, live or away, and forgets that its
    /// time ran out (a revoke). Its live session, if any, is the caller's to
    /// end.
    void remove(const QByteArray& deviceId);
    /// A fifth device took this place. No away grace; retain the event
    /// until this device next signs in, is revoked, or the Core restarts.
    void replace(const QByteArray& deviceId, const QByteArray& byId,
                 const QString& byName);
    std::optional<AdmitResult::TakenPlace> placeTakenBy(const QByteArray& deviceId) const;
    quint32 revision() const { return m_revision; }
    /// Task 41 ordering: away longest first, then present by actual last
    /// command/write activity, with an on-air device last.
    QList<Entry> replacementCandidates(const QByteArray& transmittingId = {}) const;

    /// Frees the place of every away device whose kGraceMs have passed and
    /// records that its time ran out. Returns their ids.
    QList<QByteArray> expireAway();
    /// When the next away device's time runs out; nullopt with none away.
    std::optional<qint64> nextExpiryMs() const;

    /// Slice control plan Task 8: whether the absence `awayGeneration` of
    /// `deviceId` is still the current one: the device holds no place
    /// (its time ran out, or it left), or it is away in that same absence.
    /// False once it came back (or dropped again, a newer absence).
    bool isCurrentAbsence(const QByteArray& deviceId, quint64 awayGeneration) const;

    /// A command, property write or settings write from `deviceId` (never a
    /// heartbeat). The reported activity moves at most once a minute.
    void noteActivity(const QByteArray& deviceId);

    /// Task 48: the hosting desktop's own window, which takes a place and
    /// has no network session. `id` is its device id; `name` and
    /// `shortName` are the desktop's. One at a time; a second call
    /// replaces the first.
    void registerHostingDevice(const QByteArray& id, const QString& name,
                               const QString& shortName);
    void unregisterHostingDevice();

    /// Places taken, 0 to kMaxDeviceSessions.
    int placesTaken() const { return static_cast<int>(m_entries.size()); }
    bool hasPlaceFree() const { return placesTaken() < kMaxDeviceSessions; }

    /// Every device holding a place, in admission order.
    QList<Entry> entries() const;
    std::optional<Entry> entry(const QByteArray& deviceId) const;
    /// The device whose live session is `session`, if any.
    std::optional<Entry> entryForSession(const QObject* session) const;

    /// When `deviceId`'s time ran out, kept until it is next admitted, is
    /// revoked or this object is rebuilt.
    std::optional<qint64> timeRanOutAtMs(const QByteArray& deviceId) const;

    // ---- Names (ruling 4.3) ----

    struct NameInput {
        QByteArray key;
        QString name;
        /// Already resolved: a usable short name or the kind's word.
        QString shortName;
    };
    struct NumberedName {
        QString name;
        QString shortName;
    };
    /// Numbers colliding names, and colliding short names on their own, in
    /// the order given: the first keeps its name, later ones take the next
    /// free number.
    static QHash<QByteArray, NumberedName> numberNames(const QList<NameInput>& inOrder);
    /// "Phone", "Tablet", "Computer"; "Computer" for anything else.
    static QString kindWord(const QString& deviceKind);
    /// `shortName` when DeviceStore::isValidShortName holds, else the kind's
    /// word.
    static QString usableShortName(const QString& shortName, const QString& deviceKind);
    /// "Computer at <address>" (an IPv4-mapped IPv6 address as IPv4, no
    /// scope); "Computer" when the address is empty (the relay).
    static QString tokenWindowName(const QString& peerAddress);

signals:
    /// Anything in entries() changed (not merely time passing).
    void changed();
    void placesTakenChanged(int placesTaken);
    /// Task 73 (ruling 4.11): `deviceId`'s 180 s ended and its place was
    /// freed (after changed()). Slice control plan Task 8: its claims go
    /// (StationServer::releaseDeviceClaims), for the away period
    /// `awayGeneration` only.
    void graceEnded(const QByteArray& deviceId, quint64 awayGeneration);

private:
    int indexOf(const QByteArray& deviceId) const;
    void emitChanges(int placesBefore);

    Clock m_clock;
    QElapsedTimer m_monotonic;
    QList<Entry> m_entries;
    QHash<QByteArray, qint64> m_timeRanOut;
    QHash<QByteArray, AdmitResult::TakenPlace> m_placeTaken;
    quint32 m_revision = 1;
    quint64 m_nextOrder = 1;
    quint64 m_nextToken = 1;
    quint64 m_nextAwayGeneration = 1;
};

} // namespace NereusSDR
