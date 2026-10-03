// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/MirrorView.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 72 (R-IOS-02): one device's view of the mirror.
// See MirrorView.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 72 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 73 (R-IOS-02): a class's schema before
//               its first object.create after the burst. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/MirrorView.h"

#include "core/session/MirrorSchema.h"

#include <QSet>

namespace NereusSDR {

namespace {

// The property TABLE for a Schema message (moved here from StateMirror.cpp
// with the burst): every property the schema walked, minus anything with
// no wire representation (MirrorWireKind::Unsupported -- kept in
// MirrorSchema's own table so tst_mirror_schema's membership guard can
// name it, but useless to declare to a client that will never receive a
// value for it). No currently mirrored property falls in this bucket, so
// the filter is defensive for a future property type.
QList<SessionSchemaField> schemaFieldsFor(const MirrorSchema& schema)
{
    QList<SessionSchemaField> fields;
    fields.reserve(schema.size());
    for (const MirrorProperty& prop : schema.properties()) {
        if (prop.kind == MirrorWireKind::Unsupported) {
            continue;
        }
        fields.append(SessionSchemaField{ prop.ordinal, prop.name, prop.kind });
    }
    return fields;
}

} // namespace

MirrorView::MirrorView(StateMirror* mirror, Sink sink, QObject* parent)
    : QObject(parent)
    , m_mirror(mirror)
    , m_sink(std::move(sink))
{
    if (m_mirror) {
        m_mirror->addView(this);
    }
}

MirrorView::~MirrorView()
{
    if (m_mirror) {
        m_mirror->removeView(this);
    }
}

void MirrorView::close()
{
    if (m_closed) {
        return;
    }
    m_closed = true;
    if (m_mirror) {
        m_mirror->removeView(this);
    }
    m_coalescer.clear();
    m_sink = nullptr;
}

void MirrorView::send(const SessionMessage& message)
{
    if (!m_closed && m_sink) {
        m_sink(message);
    }
}

bool MirrorView::deliver(const SessionMessage& message)
{
    if (!m_attached || m_closed || !m_sink) {
        return false;
    }
    // Task 73: a class first seen after the burst (the first marker a
    // device receives when a second device arrives) is declared first,
    // since a client drops an object whose class it was never told of.
    if (message.kind == SessionMessageKind::ObjectCreate
        && !m_announced.contains(message.className) && !m_mirror.isNull()) {
        if (const QObject* object = m_mirror->watchedObject(message.objectKey)) {
            const MirrorSchema& schema = MirrorSchema::forObject(object);
            if (schema.size() > 0
                && MirrorSchema::shortClassName(schema.className()) == message.className) {
                m_announced.insert(message.className);
                m_sink(SessionMessages::schema(message.className, schemaFieldsFor(schema)));
                if (m_closed || !m_sink) {
                    return false;
                }
            }
        }
    }
    m_sink(message);
    return true;
}

void MirrorView::noteChange(const QByteArray& objectKey, const QList<MirrorUpdate>& updates)
{
    if (!m_attached || m_closed) {
        return;
    }
    for (const MirrorUpdate& update : updates) {
        m_coalescer.update(objectKey, update);
    }
}

void MirrorView::attach()
{
    if (m_closed || m_mirror.isNull()) {
        return;
    }
    // Collecting from here on: a change a receiver makes synchronously in
    // reaction to one of this burst's own messages lands in THIS view's
    // coalescer and is sent only after the marker, by the flush below.
    m_attached = true;

    // "clear the dirty set": what was pending for this view is superseded,
    // since the burst reads every watched object's CURRENT state. Other
    // views keep theirs.
    m_coalescer.clear();

    // The keys are copied before anything is sent (a receiver may watch or
    // unwatch in response), and each object is looked up again by key as
    // the burst reaches it, so an object that went away meanwhile is
    // skipped rather than read.
    const QList<QByteArray> keys = m_mirror->watchedKeys();

    // One schema per DISTINCT class among what is watched, first-watched
    // order, under the short wire name ("SliceModel"), as ObjectRegistry
    // names it.
    m_announced.clear();
    QSet<QByteArray>& announced = m_announced;
    for (const QByteArray& key : keys) {
        if (m_closed || m_mirror.isNull()) {
            return;
        }
        const QObject* object = m_mirror->watchedObject(key);
        if (object == nullptr) {
            continue;
        }
        const MirrorSchema& schema = MirrorSchema::forObject(object);
        if (schema.size() == 0) {
            continue;
        }
        const QByteArray shortName = MirrorSchema::shortClassName(schema.className());
        if (announced.contains(shortName)) {
            continue;
        }
        announced.insert(shortName);
        send(SessionMessages::schema(shortName, schemaFieldsFor(schema)));
    }

    // One object.create per watched object, watch order, each with the FULL
    // settled property set, CONSTANT properties included (snapshot()).
    for (const QByteArray& key : keys) {
        if (m_closed || m_mirror.isNull()) {
            return;
        }
        const QObject* object = m_mirror->watchedObject(key);
        if (object == nullptr) {
            continue;
        }
        const MirrorSchema& schema = MirrorSchema::forObject(object);
        if (schema.size() == 0) {
            continue;
        }
        send(SessionMessages::objectCreate(key, MirrorSchema::shortClassName(schema.className()),
                                           m_mirror->snapshot(key)));
    }

    if (m_closed) {
        return;
    }
    send(SessionMessages::snapshotComplete());

    // Anything the burst itself caused to change, after the marker.
    flush();
}

int MirrorView::flush()
{
    if (m_closed || m_mirror.isNull()) {
        return 0;
    }
    const QList<QPair<QByteArray, QList<MirrorUpdate>>> pending = m_coalescer.flush();
    int sent = 0;
    for (const auto& batch : pending) {
        if (m_closed || m_mirror.isNull()) {
            break;
        }
        // Decided now, from the live object: the stored values are only
        // provisional (MirrorCoalescer's class comment). A key no longer
        // watched reads nothing and the batch is dropped.
        QList<quint16> ordinals;
        ordinals.reserve(batch.second.size());
        for (const MirrorUpdate& update : batch.second) {
            ordinals.append(update.ordinal);
        }
        const QList<MirrorUpdate> fresh = m_mirror->currentValues(batch.first, ordinals);
        if (fresh.isEmpty()) {
            continue;
        }
        send(SessionMessages::delta(batch.first, fresh));
        ++sent;
    }
    return sent;
}

} // namespace NereusSDR
