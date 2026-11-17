// =================================================================
// src/core/QsoUploadQueue.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see QsoUploadQueue.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude Code, operator Martin Fischer.
// =================================================================

#include "QsoUploadQueue.h"

#include "AdifLog.h"
#include "AppSettings.h"
#include "CallsignInfo.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QTimer>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcUploadQueue, "longpath.log.upload")

constexpr int kFormatVersion = 1;

QJsonObject readStore(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    return QJsonDocument::fromJson(f.readAll()).object()
        .value(QStringLiteral("services")).toObject();
}
} // namespace

QsoUploadQueue::QsoUploadQueue(const QString& storePath, QObject* parent)
    : QObject(parent), m_storePath(storePath)
{
    load();
}

QsoUploadQueue::~QsoUploadQueue() = default;

QString QsoUploadQueue::defaultStorePath()
{
    return AppSettings::dataDir() + QStringLiteral("/qso-upload-queue.json");
}

int QsoUploadQueue::storedCount(const QString& storePath)
{
    int n = 0;
    const QJsonObject services = readStore(storePath);
    for (auto it = services.begin(); it != services.end(); ++it) {
        n += it.value().toArray().size();
    }
    return n;
}

QsoUploadQueue::Lane* QsoUploadQueue::laneFor(const QString& id, bool create)
{
    for (const auto& lane : m_lanes) {
        if (lane->id == id) { return lane.get(); }
    }
    if (!create) { return nullptr; }
    auto lane = std::make_unique<Lane>();
    lane->id = id;
    Lane* raw = lane.get();
    raw->watchdog = new QTimer(this);
    raw->watchdog->setSingleShot(true);
    connect(raw->watchdog, &QTimer::timeout, this, [this, raw]() {
        if (raw->inFlight == 0) { return; }
        const int i = indexOf(*raw, raw->inFlight);
        const QString call = i >= 0 ? raw->waiting.at(i).entry.call : QString{};
        finish(raw, call, false, false, QStringLiteral("no answer"));
    });
    m_lanes.push_back(std::move(lane));
    return raw;
}

int QsoUploadQueue::indexOf(const Lane& lane, quint64 itemId)
{
    for (int i = 0; i < lane.waiting.size(); ++i) {
        if (lane.waiting.at(i).id == itemId) { return i; }
    }
    return -1;
}

void QsoUploadQueue::setUploaders(const QVector<QsoUploader*>& uploaders)
{
    for (const auto& lane : m_lanes) {
        if (lane->connection) { disconnect(lane->connection); }
        lane->connection = {};
        lane->uploader = nullptr;
    }
    for (QsoUploader* u : uploaders) {
        if (!u) { continue; }
        Lane* lane = laneFor(u->serviceId(), true);
        lane->uploader = u;
        lane->connection = connect(u, &QsoUploader::uploadFinished, this,
            [this, lane](const QString& call, bool ok, bool duplicate,
                         const QString& message) {
                finish(lane, call, ok, duplicate, message);
            });
    }
}

int QsoUploadQueue::submit(const LogEntry& entry)
{
    if (!entry.isValid()) { return 0; }

    const quint64 batchId = ++m_nextId;
    Batch batch;
    batch.entry = entry;

    for (const auto& lanePtr : m_lanes) {
        Lane* lane = lanePtr.get();
        if (!lane->uploader || !lane->uploader->isConfigured()) { continue; }

        // The same contact already waiting (logged, upload failed,
        // logged again by a WSJT-X resend): one entry, not two.
        quint64 itemId = 0;
        for (const Item& it : lane->waiting) {
            if (AdifLog::isSameQso(it.entry, entry)) { itemId = it.id; break; }
        }
        if (itemId == 0) {
            Item it;
            it.id = ++m_nextId;
            it.entry = entry;
            lane->waiting.append(it);
            itemId = it.id;
        }
        if (lane->batchOf.contains(itemId)) { continue; }   // already on its way
        lane->batchOf.insert(itemId, batchId);
        if (lane->inFlight != itemId && !lane->work.contains(itemId)) {
            lane->work.append(itemId);
        }
        ++batch.outstanding;
        schedulePump(lane);
    }

    if (batch.outstanding == 0) { return 0; }
    m_batches.insert(batchId, batch);
    // On disk before the first byte goes out: a crash or a quit in the
    // next second must not turn "logged and being sent" into "logged
    // and never sent".
    save();
    return batch.outstanding;
}

void QsoUploadQueue::retryPending()
{
    for (const auto& lanePtr : m_lanes) {
        Lane* lane = lanePtr.get();
        lane->retryFailStreak = 0;
        queueLeftovers(lane);
    }
}

void QsoUploadQueue::queueLeftovers(Lane* lane)
{
    if (!lane->uploader || !lane->uploader->isConfigured()) { return; }
    bool any = false;
    for (const Item& it : lane->waiting) {
        if (it.id == lane->inFlight || lane->work.contains(it.id)) { continue; }
        lane->work.append(it.id);
        any = true;
    }
    if (any) {
        lane->roundHadRetry = true;
        schedulePump(lane);
    }
}

int QsoUploadQueue::waitingCount() const
{
    int n = 0;
    for (const auto& lane : m_lanes) { n += lane->waiting.size(); }
    return n;
}

int QsoUploadQueue::waitingCount(const QString& serviceId) const
{
    for (const auto& lane : m_lanes) {
        if (lane->id == serviceId) { return lane->waiting.size(); }
    }
    return 0;
}

void QsoUploadQueue::schedulePump(Lane* lane)
{
    if (lane->pumpQueued) { return; }
    lane->pumpQueued = true;
    // Queued, never direct: AdifNetworkUploader (and every uploader's
    // "not configured" path) answers from INSIDE upload(). Sending from
    // the caller's stack would report a contact before submit() had
    // returned, and a long waiting list would recurse once per entry.
    QMetaObject::invokeMethod(this, [this, lane]() {
        lane->pumpQueued = false;
        pump(lane);
    }, Qt::QueuedConnection);
}

void QsoUploadQueue::pump(Lane* lane)
{
    if (lane->inFlight != 0) { return; }
    if (!lane->uploader || !lane->uploader->isConfigured()) {
        // The key was taken away while contacts were waiting. They stay
        // on the list; whoever logged them is told why nothing went.
        const QList<quint64> stranded = lane->work;
        lane->work.clear();
        for (quint64 id : stranded) {
            const auto b = lane->batchOf.constFind(id);
            if (b == lane->batchOf.constEnd()) { continue; }
            const quint64 batchId = b.value();
            lane->batchOf.remove(id);
            Outcome o;
            o.service = lane->uploader ? lane->uploader->shortName() : lane->id;
            o.message = QStringLiteral("not set up");
            record(batchId, lane, o);
        }
        endRoundIfIdle(lane);
        return;
    }

    while (!lane->work.isEmpty()) {
        const quint64 id = lane->work.takeFirst();
        const int i = indexOf(*lane, id);
        if (i < 0) { continue; }
        const LogEntry entry = lane->waiting.at(i).entry;
        lane->inFlight = id;
        lane->watchdog->start(m_answerTimeoutMs);
        lane->uploader->upload(entry);   // may answer before it returns
        return;
    }
    endRoundIfIdle(lane);
}

void QsoUploadQueue::finish(Lane* lane, const QString& call, bool ok,
                            bool duplicate, const QString& message)
{
    // The uploader objects are shared with the logbook window's Upload
    // menu. An answer while nothing of ours is in flight, or for a
    // different callsign, is that window's business.
    if (lane->inFlight == 0) { return; }
    const int i = indexOf(*lane, lane->inFlight);
    if (i >= 0 && Callsigns::normalized(lane->waiting.at(i).entry.call)
                      != Callsigns::normalized(call)) {
        return;
    }

    const quint64 id = lane->inFlight;
    lane->inFlight = 0;
    lane->watchdog->stop();
    if (i < 0) { schedulePump(lane); return; }

    const LogEntry entry = lane->waiting.at(i).entry;
    const QString shortName =
        lane->uploader ? lane->uploader->shortName() : lane->id;
    const auto batchIt = lane->batchOf.constFind(id);
    const bool fresh = batchIt != lane->batchOf.constEnd();
    const quint64 batchId = fresh ? batchIt.value() : 0;
    lane->batchOf.remove(id);

    if (ok) {
        lane->waiting.removeAt(i);
        lane->retryFailStreak = 0;
        if (!fresh) { ++lane->roundSent; }
        save();
        emit uploaded(lane->id, entry, duplicate);
    } else {
        // To the back of the list: a contact the service refuses for
        // its own sake must not stand in front of the others at every
        // attempt.
        const Item kept = lane->waiting.takeAt(i);
        lane->waiting.append(kept);
        save();
        qCWarning(lcUploadQueue).noquote()
            << shortName << "upload failed for" << entry.call << "-" << message
            << "- kept," << lane->waiting.size() << "waiting";
        if (!fresh) {
            lane->roundLastError = message;
            if (++lane->retryFailStreak >= kMaxRetryFailuresInARow) {
                // Give up this round. Freshly logged contacts stay in
                // line; they are owed an answer of their own.
                QList<quint64> keep;
                for (quint64 w : lane->work) {
                    if (lane->batchOf.contains(w)) { keep.append(w); }
                }
                lane->work = keep;
            }
        }
    }

    if (fresh) {
        Outcome o;
        o.service = shortName;
        o.ok = ok;
        o.duplicate = duplicate;
        o.message = message;
        record(batchId, lane, o);
        // A fresh contact got through, so the service is reachable and
        // the key is good: now is the moment for what was left over.
        if (ok) {
            lane->retryFailStreak = 0;
            queueLeftovers(lane);
        }
    }

    schedulePump(lane);
}

int QsoUploadQueue::placeOf(const Lane* lane) const
{
    for (std::size_t i = 0; i < m_lanes.size(); ++i) {
        if (m_lanes[i].get() == lane) { return static_cast<int>(i); }
    }
    return -1;
}

void QsoUploadQueue::record(quint64 batchId, const Lane* lane,
                            const Outcome& outcome)
{
    auto it = m_batches.find(batchId);
    if (it == m_batches.end()) { return; }
    it->outcomes.insert(placeOf(lane), outcome);
    if (--it->outstanding > 0) { return; }
    const Batch done = it.value();
    m_batches.erase(it);
    emit qsoFinished(done.entry, done.outcomes.values().toVector());
}

void QsoUploadQueue::endRoundIfIdle(Lane* lane)
{
    if (lane->inFlight != 0 || !lane->work.isEmpty()) { return; }
    if (!lane->roundHadRetry) { return; }
    const int sent = lane->roundSent;
    const QString err = lane->roundLastError;
    lane->roundHadRetry = false;
    lane->roundSent = 0;
    lane->roundLastError.clear();
    emit backlogSent(lane->uploader ? lane->uploader->shortName() : lane->id,
                     sent, lane->waiting.size(), err);
}

QString QsoUploadQueue::statusLine(const QString& call,
                                   const QVector<Outcome>& outcomes)
{
    QStringList sent;
    QStringList already;
    QStringList failed;
    for (const Outcome& o : outcomes) {
        if (!o.ok) {
            failed << QStringLiteral("%1 upload failed: %2")
                          .arg(o.service, o.message.isEmpty()
                                              ? QStringLiteral("no reason given")
                                              : o.message);
        } else if (o.duplicate) {
            already << o.service;
        } else {
            sent << o.service;
        }
    }

    QStringList good;
    if (!sent.isEmpty()) {
        good << QStringLiteral("uploaded to %1").arg(sent.join(QStringLiteral(", ")));
    }
    if (!already.isEmpty()) {
        good << QStringLiteral("already in %1").arg(already.join(QStringLiteral(", ")));
    }

    if (failed.isEmpty()) {
        return QStringLiteral("%1 — %2").arg(call, good.join(QStringLiteral(", ")));
    }
    // The contact is in the local file regardless; say so, or a failed
    // upload reads as a lost QSO.
    const QString bad = QStringLiteral("logged locally, %1 — will retry")
                            .arg(failed.join(QStringLiteral("; ")));
    if (good.isEmpty()) {
        return QStringLiteral("%1 %2").arg(call, bad);
    }
    return QStringLiteral("%1 — %2; %3")
        .arg(call, good.join(QStringLiteral(", ")), bad);
}

// ── The file ─────────────────────────────────────────────────────────
//
// {"version":1,"services":{"qrz":["<CALL:5>…<EOR>", …],"cloudlog":[…]}}
//
// ADIF records, because that is what goes out and what AdifLog already
// reads back losslessly (unmodelled fields included). No credentials:
// the keys stay in the keychain.

void QsoUploadQueue::load()
{
    const QJsonObject services = readStore(m_storePath);
    for (auto it = services.begin(); it != services.end(); ++it) {
        Lane* lane = laneFor(it.key(), true);
        const QJsonArray records = it.value().toArray();
        for (const QJsonValue& v : records) {
            const QVector<LogEntry> parsed = AdifLog::parse(v.toString());
            if (parsed.isEmpty() || !parsed.first().isValid()) { continue; }
            Item item;
            item.id = ++m_nextId;
            item.entry = parsed.first();
            lane->waiting.append(item);
        }
    }
}

void QsoUploadQueue::save() const
{
    QJsonObject services;
    for (const auto& lane : m_lanes) {
        if (lane->waiting.isEmpty()) { continue; }
        QJsonArray records;
        for (const Item& it : lane->waiting) {
            records.append(it.entry.toAdifRecord());
        }
        services.insert(lane->id, records);
    }
    if (services.isEmpty()) {
        // Nothing waiting is the normal state; no file says so as well
        // as an empty one and leaves the folder as it was.
        QFile::remove(m_storePath);
        return;
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("services"), services);

    // Temporary file and rename, like the logbook itself: a half
    // written list would lose the contacts that were waiting on it.
    QSaveFile f(m_storePath);
    if (!f.open(QIODevice::WriteOnly)
        || f.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !f.commit()) {
        qCWarning(lcUploadQueue).noquote()
            << "couldn't write the upload waiting list:" << f.errorString();
    }
}

} // namespace Longpath
