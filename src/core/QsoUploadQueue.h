#pragma once

// =================================================================
// src/core/QsoUploadQueue.h  (Longpath)
// =================================================================
//
// Longpath-original. No Thetis or AetherSDR equivalent — Thetis hands
// QSOs to an external logger, and AetherSDR's QRZ code is lookup-only.
//
// Every logged contact goes to every logging service that is set up —
// QRZ, Wavelog/Cloudlog, a local logger — at the same time and
// independently of one another, and what did not get through waits
// here until it does.
//
// Why this is its own class and not three more lines in the panel:
//
//   1. Until 2026-11-17 live logging went to QRZ alone
//      (RotorLogbookPanel held ONE QrzLogbookUploader). The other
//      services existed, but only behind the logbook window's Upload
//      menu. Operator, 2026-11-17: "alle qso die im digi modus erledigt
//      werden sollen sofort auf qrz und wavelog".
//
//   2. A failed upload was reported once in the status line and then
//      forgotten. With one FT8 contact a minute nobody reads that line,
//      so "the network was away for ten minutes" silently became ten
//      contacts missing at the far end.
//
//   3. QsoUploader::uploadFinished names the contact by CALLSIGN only.
//      Two uploads in flight on one service cannot be told apart by
//      that — and FT8 produces exactly that. So each service gets a
//      lane that sends ONE contact at a time; the answer then belongs
//      to the contact that is in flight, whatever its callsign.
//
// The waiting list is a small JSON file beside the logbook
// (qso-upload-queue.json), one list of ADIF records per service. It is
// NOT the logbook and never touches it: the contact is in logbook.adi
// before it ever comes here ("log nie verlieren"), and a contact that
// waits here is a contact that is safely logged and merely not yet
// sent.
//
// A contact is written to the list BEFORE it is sent and taken off
// only when the service said yes (a duplicate counts as yes — the
// service has it). Quitting mid-upload therefore sends it again at the
// next start, which the services answer with "duplicate".
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude Code, operator Martin Fischer.
// =================================================================

#include "QsoUploader.h"
#include "models/LogEntry.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

class QTimer;

namespace Longpath {

class QsoUploadQueue : public QObject {
    Q_OBJECT
public:
    // What one service said about one freshly logged contact.
    struct Outcome {
        QString service;      // QsoUploader::shortName()
        bool    ok{false};
        bool    duplicate{false};
        QString message;
    };

    explicit QsoUploadQueue(const QString& storePath,
                            QObject* parent = nullptr);
    ~QsoUploadQueue() override;

    // <dataDir>/qso-upload-queue.json — beside logbook.adi, and like it
    // redirected by LONGPATH_CONFIG_DIR.
    static QString defaultStorePath();

    // How many contacts the file at `storePath` holds, over all
    // services, without building a queue. For the start-up question
    // "is there anything to catch up on?" — answering it must not cost
    // a keychain prompt, which constructing the uploaders does.
    static int storedCount(const QString& storePath);

    // The services. Not owned. May be called again; lanes are matched
    // by serviceId(), so the waiting lists survive.
    void setUploaders(const QVector<QsoUploader*>& uploaders);

    // A contact that has just been written to the local log. Goes to
    // every service that isConfigured(). Returns how many that is; 0
    // means nothing is set up and nothing will be reported.
    //
    // Never reports from inside this call — the first send happens on
    // the next turn of the event loop, so the caller can finish its own
    // status line first.
    int submit(const LogEntry& entry);

    // Send what is still waiting. Called once after start-up, and by
    // the queue itself whenever a fresh contact got through (the
    // service evidently works again).
    void retryPending();

    int waitingCount() const;
    int waitingCount(const QString& serviceId) const;

    // A service that never answers must not block its lane for ever.
    void setAnswerTimeoutMs(int ms) { m_answerTimeoutMs = ms; }

    // "OE1ABC — uploaded to QRZ, Wavelog" /
    // "OE1ABC logged locally, Wavelog upload failed: … — will retry".
    // Here rather than in the panel so the wording is testable without
    // a window.
    static QString statusLine(const QString& call,
                              const QVector<Outcome>& outcomes);

    // After this many refusals in a row a catch-up round is abandoned:
    // a service that is down, or a key that is wrong, should cost three
    // requests per attempt, not one per waiting contact.
    static constexpr int kMaxRetryFailuresInARow = 3;

signals:
    // Every service has answered for this freshly logged contact.
    void qsoFinished(const Longpath::LogEntry& entry,
                     const QVector<Longpath::QsoUploadQueue::Outcome>& outcomes);

    // One service accepted one contact — fresh or caught up.
    void uploaded(const QString& serviceId, const Longpath::LogEntry& entry,
                  bool duplicate);

    // A catch-up round on one service is over.
    void backlogSent(const QString& shortName, int sent, int stillWaiting,
                     const QString& lastError);

private:
    struct Item {
        quint64  id{0};
        LogEntry entry;
    };
    struct Lane {
        QString               id;
        QPointer<QsoUploader> uploader;
        QMetaObject::Connection connection;
        QVector<Item>         waiting;      // persisted, oldest first
        QList<quint64>        work;         // to send in this round
        quint64               inFlight{0};
        QHash<quint64, quint64> batchOf;    // item -> fresh batch
        int                   retryFailStreak{0};
        int                   roundSent{0};
        bool                  roundHadRetry{false};
        QString               roundLastError;
        bool                  pumpQueued{false};
        QTimer*               watchdog{nullptr};
    };
    struct Batch {
        LogEntry         entry;
        int              outstanding{0};
        // Keyed by the service's place in setUploaders(), NOT in the
        // order the answers came in: "uploaded to QRZ, Wavelog" must
        // not read "Wavelog, QRZ" whenever Wavelog happens to answer
        // first (it did, on the first CI run of this class).
        QMap<int, Outcome> outcomes;
    };
    int placeOf(const Lane* lane) const;
    void record(quint64 batchId, const Lane* lane, const Outcome& outcome);

    Lane* laneFor(const QString& id, bool create);
    static int indexOf(const Lane& lane, quint64 itemId);
    void schedulePump(Lane* lane);
    void pump(Lane* lane);
    void finish(Lane* lane, const QString& call, bool ok, bool duplicate,
                const QString& message);
    void queueLeftovers(Lane* lane);
    void endRoundIfIdle(Lane* lane);
    void load();
    void save() const;

    QString m_storePath;
    std::vector<std::unique_ptr<Lane>> m_lanes;
    QHash<quint64, Batch> m_batches;
    quint64 m_nextId{0};
    int m_answerTimeoutMs{60000};
};

} // namespace Longpath
