#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ConfirmStep.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-30; the several-devices design, sections
// 7.3 and 7.4, rulings 7.4a and 7.5): the Core's record of the questions it
// has asked and the notices it has sent or is keeping.
//
// The confirm step (7.3): a change that would disturb another device is
// not applied. The Core keeps it here as a Question, with what the operator
// was shown (each disturbed device's slices and effects, or the take's
// choices), and asks. On confirm.proceed StationServer computes the set
// again; a set that names a device or an effect the operator was not shown
// is asked again, anything else is applied exactly as the original request
// would have been. A device has at most one open question: a new one
// replaces it, and its session ending drops it (ruling 7.5). A question
// expires 60 s after it is sent (expired(): Task 75 refuses a later
// proceed).
//
// Task 75 adds the setting that affects every device (kind sharedSetting,
// section 7.1): the question also remembers what the change acts on and
// that target's value when it was asked (ruling 7.6), so a proceed after
// the value moved, whoever moved it, is refused, and a new write from the
// requester to the same target cancels the question.
//
// Notices (7.4): each has an id; a notice with Take it back is kept, with
// the slices it closed and their settings, so notice.takeBack can recreate
// them. An away device's notices wait here and are sent right after its
// snapshot.complete; after its 3 minutes they still arrive, after
// graceEnded and without Take it back. The Core keeps them until the device
// returns, is revoked, or the Core restarts (nothing here is saved).
//
// Slice control plan Task 1: a slice id is reused once its slice closes,
// so every slice a question names, offers or shows closing is also kept
// with its incarnation (SliceOwnership::SliceRef). A proceed whose slice
// has a different incarnation by then acts on nothing and is answered as
// changed since asked.
//
// Times are the session registry's monotonic milliseconds (the one clock
// convention, ruling 10.3): secondsAgo is measured when a notice is sent.
//
// Single thread: StationServer's.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 75 (R-IOS-30): the settings write,
//               the target and its value, expiry. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               takeTransmit question's holder epoch and keyed state. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: slice control and shared listening plan Task 1: each named,
//               offered and shown slice's incarnation. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 9: askedListeners, each slice's
//               listeners when a device that shares slices was asked. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: take-over parity: forgetTakeBacks(), the records of one
//               device that a test picks. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>

#include <functional>
#include <optional>

#include "core/DeviceLayoutStore.h"
#include "core/SliceOwnership.h"
#include "core/session/SessionMessages.h"

namespace NereusSDR {

class ConfirmStep {
public:
    /// Ruling 7.5: a question expires 60 s after it is sent.
    static constexpr qint64 kExpiryMs = 60000;

    /// What a question holds back.
    enum class Held {
        Command,        ///< a command.invoke, run again on proceed
        PropertyWrite,  ///< a property.write, applied on proceed
        TakeBack,       ///< notice.takeBack: recreate the closed slices
        SettingsWrite,  ///< a settings.write (Task 75), applied on proceed
    };

    struct Question {
        qint64 id = 0;
        QByteArray device;
        /// panMove, takeReceiver, takeSlice, sharedSetting or (Task 77)
        /// takeTransmit.
        QString kind;
        Held held = Held::Command;
        SessionMessage original;
        /// TakeBack: the notice it answers.
        qint64 noticeId = 0;
        /// panMove: the receiver, its new centre and the slice whose own
        /// retune moves it (-1 for a C-Tune move).
        int stream = -1;
        double centreHz = 0.0;
        int exemptSliceId = -1;
        /// What the operator was shown: "<device>|<slice>|<effect>" for
        /// each disturbed slice (panMove), or per choice the slices that
        /// close (a take).
        QSet<QString> shown;
        QList<QSet<int>> shownChoices;
        /// A take: each choice's receiver (takeReceiver) or slice
        /// (takeSlice), and whether it was offered as takeable.
        QList<int> choiceTargets;
        QList<bool> choiceTakeable;
        /// takeSlice: whose each choice's slice was when asked.
        QList<QByteArray> shownOwners;
        /// A take: what needs a receiver (ReceiverPlanner::Need), and the
        /// requester's slices that go with the change.
        int need = -1;
        QList<int> moving;
        /// TakeBack: a choice of -1 in choiceTargets is a free slice.
        qint64 askedAtMs = 0;
        /// sharedSetting (Task 75, ruling 7.6): what the change acts on,
        /// its value when asked, and the {label, from, to} shown.
        QString target;
        QString targetValue;
        QJsonObject change;
        /// Fix wave I2: the requester's slices the question names (the
        /// written object, a `sliceId` argument, the slices in `moving`).
        /// Each must still be the requester's at proceed.
        QList<int> namedSlices;
        /// Slice control plan Task 1: namedSlices with the incarnation each
        /// had when asked, in the same order.
        QList<SliceOwnership::SliceRef> namedRefs;
        /// Slice control plan Task 1: per entry of choiceTargets, the
        /// offered slice with its incarnation when asked (takeSlice); a
        /// receiver choice or a free slice has sliceId -1.
        QList<SliceOwnership::SliceRef> choiceRefs;
        /// Slice control plan Task 1: every slice in `shown` (a pan move's
        /// disturbed slices, each choice's closing slices) with its
        /// incarnation when asked.
        QList<SliceOwnership::SliceRef> shownRefs;
        /// Slice control fix wave: the control revision of every slice in
        /// namedRefs, choiceRefs and shownRefs when asked, by slice id. A
        /// slice whose control changed hands meanwhile (even back again)
        /// has listeners the operator was not shown.
        QHash<int, quint64> askedRevisions;
        /// Slice control plan Task 9: for a device that shares slices, the
        /// listeners of the same slices when asked, by slice id, controller
        /// first. A slice that gained a listener meanwhile is asked again;
        /// one that lost a listener is not.
        QHash<int, QList<QByteArray>> askedListeners;
        bool listenersShown = false;
        /// takeTransmit (Task 77, ruling 8.7): the holder epoch and whether
        /// the holder was on the air when asked.
        quint64 holderEpoch = 0;
        bool holderKeyed = false;
    };

    struct Notice {
        qint64 id = 0;
        QByteArray device;
        QString reason;
        SessionPrompt prompt;
        qint64 happenedAtMs = 0;
        /// Take it back: who took, what (the receiver, or the taker's new
        /// slice), and the slices it closed with their settings.
        QByteArray taker;
        int takenStream = -1;
        int takerSlice = -1;
        QList<SavedSlice> closed;
    };

    qint64 nextId() { return ++m_lastId; }

    /// Keeps `question` as its device's one open question, replacing any
    /// other.
    void ask(const Question& question);
    /// The device's open question with this id, removed; nullopt when none.
    std::optional<Question> answer(const QByteArray& device, qint64 id);
    const Question* openQuestion(const QByteArray& device) const;
    void dropQuestion(const QByteArray& device);
    /// Fix wave I2: drops every open question that names `sliceId` (it
    /// closed or changed owner), remembering each so its answer is told
    /// what it reaches changed. Returns the devices whose question went.
    QList<QByteArray> dropQuestionsNaming(int sliceId);
    /// The kind of `device`'s question `id` dropped by dropQuestionsNaming,
    /// forgotten on read; nullopt when it was not.
    std::optional<QString> takeDroppedAsChanged(const QByteArray& device, qint64 id);
    /// Ruling 7.5: whether `question`, asked at askedAtMs, has expired at
    /// `nowMs` (60 s after it was sent, that instant included).
    static bool expired(const Question& question, qint64 nowMs)
    {
        return nowMs - question.askedAtMs >= kExpiryMs;
    }

    /// Keeps a notice that offers Take it back, for notice.takeBack.
    void keepTakeBack(const Notice& notice);
    std::optional<Notice> takeBackRecord(const QByteArray& device, qint64 id) const;
    void forgetTakeBack(const QByteArray& device, qint64 id);
    /// Take-over parity: every Take it back record of `device` that
    /// `which` picks goes.
    void forgetTakeBacks(const QByteArray& device,
                         const std::function<bool(const Notice&)>& which);

    /// Keeps a notice for a device that is away.
    void keepPending(const Notice& notice);
    /// The device's waiting notices, oldest first, removed.
    QList<Notice> takePending(const QByteArray& device);
    int pendingCount(const QByteArray& device) const;
    /// Fix wave 2 (the away device's taken slice): the device's waiting
    /// notices that still offer Take it back no longer do, and are returned
    /// as they were, so the slices they closed can be saved for the device.
    /// The notices stay waiting.
    ///
    /// Fix wave 3 (the re-review's second out-of-scope item): a Take it
    /// back already delivered ends here too (ruling 7.4: at the end of the
    /// device's next away period), so every Take it back record the device
    /// has goes and is returned, once each.
    QList<Notice> endTakeBacks(const QByteArray& device);

    /// A revoked device: every question, record and waiting notice goes.
    void forgetDevice(const QByteArray& device);

private:
    qint64 m_lastId = 0;
    QHash<QByteArray, Question> m_questions;
    /// Per device, the question dropped because a slice it named went:
    /// its id and kind.
    QHash<QByteArray, QPair<qint64, QString>> m_droppedAsChanged;
    QHash<QByteArray, QList<Notice>> m_takeBack;
    QHash<QByteArray, QList<Notice>> m_pending;
};

} // namespace NereusSDR
