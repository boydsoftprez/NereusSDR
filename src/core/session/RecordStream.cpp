// no-port-check: NereusSDR-original. The link's record streams.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/session/RecordStream.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See RecordStream.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Created (parity Task 19, R-IOS-25,
//                                    R-R3-49). AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "core/session/RecordStream.h"

#include <algorithm>

namespace NereusSDR {

RecordStream::RecordStream(const QString& name, int capacity)
    : m_name(name)
    , m_capacity(std::max(1, capacity))
{
}

void RecordStream::upsert(const QString& id, const QJsonObject& fields)
{
    if (id.isEmpty()) {
        return;
    }
    if (m_records.contains(id)) {
        m_order.removeOne(id);
    }
    m_records.insert(id, fields);
    m_order.append(id);
    noteUpsert(id, fields);
    while (m_order.size() > m_capacity) {
        const QString oldest = m_order.takeFirst();
        m_records.remove(oldest);
        noteRemove(oldest);
    }
}

void RecordStream::remove(const QString& id)
{
    if (!m_records.contains(id)) {
        return;
    }
    m_records.remove(id);
    m_order.removeOne(id);
    noteRemove(id);
}

void RecordStream::reset()
{
    m_records.clear();
    m_order.clear();
    ++m_generation;
    for (Pending& pending : m_subscribers) {
        pending.clearChanges();
        pending.reset = true;
    }
}

QList<RecordUpsert> RecordStream::newest(int count) const
{
    QList<RecordUpsert> out;
    const int n = std::clamp(count, 0, static_cast<int>(m_order.size()));
    out.reserve(n);
    for (int i = static_cast<int>(m_order.size()) - n; i < m_order.size(); ++i) {
        const QString& id = m_order.at(i);
        out.append({id, m_records.value(id)});
    }
    return out;
}

RecordBatch RecordStream::subscribe(Subscriber peer, int backlog)
{
    Pending pending;
    pending.backlog = std::clamp(backlog, 0, m_capacity);
    m_subscribers.insert(peer, pending);
    RecordBatch batch;
    batch.stream = m_name;
    batch.generation = m_generation;
    batch.reset = true;
    batch.upserts = newest(pending.backlog);
    return batch;
}

void RecordStream::unsubscribe(Subscriber peer)
{
    m_subscribers.remove(peer);
}

QList<std::pair<RecordStream::Subscriber, RecordBatch>> RecordStream::takePending()
{
    QList<std::pair<Subscriber, RecordBatch>> out;
    for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
        Pending& pending = it.value();
        if (!pending.reset && pending.count() == 0) {
            continue;
        }
        RecordBatch batch;
        batch.stream = m_name;
        batch.generation = m_generation;
        if (pending.reset) {
            // The newest records as they are now: what the peer would have
            // after every change it missed, up to its backlog.
            batch.reset = true;
            batch.upserts = newest(pending.backlog);
        } else {
            for (const QString& id : std::as_const(pending.upsertOrder)) {
                batch.upserts.append({id, pending.upserts.value(id)});
            }
            batch.removes = pending.removes;
        }
        pending.reset = false;
        pending.clearChanges();
        out.append({it.key(), batch});
    }
    return out;
}

int RecordStream::pendingCountForTest(Subscriber peer) const
{
    const auto it = m_subscribers.constFind(peer);
    return it == m_subscribers.cend() ? 0 : it->count();
}

bool RecordStream::pendingResetForTest(Subscriber peer) const
{
    const auto it = m_subscribers.constFind(peer);
    return it != m_subscribers.cend() && it->reset;
}

void RecordStream::noteUpsert(const QString& id, const QJsonObject& fields)
{
    for (Pending& pending : m_subscribers) {
        if (pending.reset) {
            continue; // the reset sends the records as they are then
        }
        pending.removes.removeOne(id);
        if (pending.upserts.contains(id)) {
            pending.upsertOrder.removeOne(id);
        }
        pending.upserts.insert(id, fields);
        pending.upsertOrder.append(id);
        boundPending(pending);
    }
}

void RecordStream::noteRemove(const QString& id)
{
    for (Pending& pending : m_subscribers) {
        if (pending.reset) {
            continue;
        }
        // A waiting upsert may be an update of a record the peer already
        // holds, so the remove is always queued (fix wave, I4); a window
        // ignores a remove for an id it never held.
        if (pending.upserts.remove(id) > 0) {
            pending.upsertOrder.removeOne(id);
        }
        if (!pending.removes.contains(id)) {
            pending.removes.append(id);
        }
        boundPending(pending);
    }
}

void RecordStream::boundPending(Pending& pending) const
{
    // A slow peer never holds more than a stream's worth of changes: past
    // that, the changes are dropped for a reset, which carries only the
    // newest records up to its backlog.
    if (pending.count() > m_capacity) {
        pending.clearChanges();
        pending.reset = true;
    }
}

} // namespace NereusSDR
