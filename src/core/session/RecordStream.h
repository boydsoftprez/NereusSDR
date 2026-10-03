#pragma once
// no-port-check: NereusSDR-original. The link's record streams: a keyed,
// bounded list of records the Core sends to the peers that ask for it.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/RecordStream.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. R-IOS-25, R-R3-49 (parity Task 19,
// the iPhone app plan's Task 21 station half; remote design section 6.1a).
//
// The third way the Core shares state (after mirrored objects and the
// settings proxy): records that come and go, such as spots and console
// lines, which no property bag can carry. A stream keeps at most
// `capacity` records, newest last. A peer subscribes with a backlog (how
// many of the newest records it wants at once) and is then sent only what
// changes: upserts (a new record, or a changed one) and removes, merged by
// record id between sends.
//
// Bounded for a slow peer: what waits for one peer never grows past the
// stream's capacity. When it would, the waiting changes are dropped and
// the peer is sent a reset instead (a new generation carrying only the
// newest records, up to its backlog), so the oldest records are the ones
// lost, never queued without limit.
//
// The wire form is the `record.batch` message (the link document, section
// "Record streams"): {stream, generation, reset, upserts: [{id, fields}],
// removes: [id]}.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 19, R-IOS-25,
//                                    R-R3-49). AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

namespace NereusSDR {

/// One record: its id (unique in its stream) and its fields.
struct RecordUpsert {
    QString id;
    QJsonObject fields;
    bool operator==(const RecordUpsert& other) const
    {
        return id == other.id && fields == other.fields;
    }
};

/// What a peer is sent, and what a window applies: the `record.batch`
/// message's body. `reset` true means the peer's copy is replaced by the
/// upserts (removes is then empty).
struct RecordBatch {
    QString stream;
    quint64 generation = 0;
    bool reset = false;
    QList<RecordUpsert> upserts;
    QStringList removes;

    bool isEmpty() const { return !reset && upserts.isEmpty() && removes.isEmpty(); }
};

class RecordStream {
public:
    /// A subscriber, as the Core names it (the peer's connection).
    using Subscriber = const void*;

    RecordStream(const QString& name, int capacity);

    const QString& name() const { return m_name; }
    int capacity() const { return m_capacity; }
    /// Starts at 1; each reset() raises it.
    quint64 generation() const { return m_generation; }
    int size() const { return static_cast<int>(m_order.size()); }
    bool contains(const QString& id) const { return m_records.contains(id); }

    /// A new record, or a changed one; it becomes the newest. Past the
    /// capacity the oldest record is removed (and subscribers told).
    void upsert(const QString& id, const QJsonObject& fields);
    /// Removes one record (no change when it is not held).
    void remove(const QString& id);
    /// Removes every record and starts a new generation; each subscriber
    /// is sent a reset.
    void reset();

    /// The newest `count` records (all when count is larger), oldest first.
    QList<RecordUpsert> newest(int count) const;

    /// Subscribes (or subscribes again): returns the reset batch the peer
    /// is sent at once, carrying the newest min(backlog, capacity) records.
    RecordBatch subscribe(Subscriber peer, int backlog);
    void unsubscribe(Subscriber peer);
    bool isSubscribed(Subscriber peer) const { return m_subscribers.contains(peer); }
    int subscriberCount() const { return static_cast<int>(m_subscribers.size()); }

    /// What each subscriber is owed since the last call, one batch each
    /// (only subscribers with something to send), and clears it.
    QList<std::pair<Subscriber, RecordBatch>> takePending();

    /// For a test: how many changes wait for one subscriber (upserts plus
    /// removes), and whether they turned into a reset.
    int pendingCountForTest(Subscriber peer) const;
    bool pendingResetForTest(Subscriber peer) const;

private:
    struct Pending {
        int backlog = 0;
        bool reset = false;
        QStringList upsertOrder;
        QHash<QString, QJsonObject> upserts;
        QStringList removes;
        int count() const { return static_cast<int>(upsertOrder.size() + removes.size()); }
        void clearChanges()
        {
            upsertOrder.clear();
            upserts.clear();
            removes.clear();
        }
    };

    void noteUpsert(const QString& id, const QJsonObject& fields);
    void noteRemove(const QString& id);
    void boundPending(Pending& pending) const;

    QString m_name;
    int m_capacity = 1;
    quint64 m_generation = 1;
    QStringList m_order; // oldest first
    QHash<QString, QJsonObject> m_records;
    QHash<Subscriber, Pending> m_subscribers;
};

} // namespace NereusSDR
