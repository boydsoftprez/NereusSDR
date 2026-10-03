#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/MirrorView.h  (NereusSDR)
// =================================================================
//
// One device's view of the Core's mirrored state (iPhone app plan Task 72,
// R-IOS-02; the several-devices design, section 5.5, rulings 5.6 to 5.8).
//
// StateMirror keeps the one set of watches: one watcher per model object
// and one re-read of every property a notifier announces. Up to four
// devices are admitted to a Core at once, and each gets a MirrorView of
// its own, which holds what a shared mirror could not:
//
//   - its own outbound coalescer. A shared one is cleared by every attach
//     (a fresh burst carries every current value), which would lose the
//     deltas another device still had pending;
//   - its own attach burst: a schema per class, an object.create per
//     watched object, then snapshot.complete, to this device alone;
//   - its own sink, which is where the Core fits each message to the
//     device (StationServer::sendToPeer: its agreed minor, what its
//     capabilities offer, the pairing code only to a paired device's key).
//     Task 73 adds ownership to that filter: a device's own slice:
//     objects, a marker: for every other slice (never to an older
//     window).
//
// Echo is per writer (ruling 5.7). StateMirror tells every view about
// every change it sees, EXCEPT the view whose device's write it is
// applying: a change made while applying device A's write (the property
// written, and any same-thread side effect on another object, such as a
// co-hosted slice's shared noise blanker) is withheld from A's view, as
// the one shared mirror withheld it before, and reaches every other view.
// A's own write is answered by property.result and the readback of its
// side effects on the written object (StationServer::handlePropertyWrite).
//
// A view starts collecting changes when it first attaches, and a delta
// is decided only when it flushes: each pending (object, property) is read
// again from the live object then, so a value changed again under a
// suppressed notify is still sent as the model holds it, and a pending
// property of an object no longer watched is dropped rather than sent.
//
// Single thread: a view, its StateMirror and every watched object live on
// the RadioModel's thread (StateMirror.h's attachSession() precondition).
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

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>

#include <functional>

#include "core/session/SessionMessages.h"
#include "core/session/StateMirror.h"

namespace NereusSDR {

class MirrorView : public QObject {
    Q_OBJECT

public:
    /// Where this view's messages go, already in wire order. The Core's
    /// sink fits each one to the device and sends it.
    using Sink = std::function<void(const SessionMessage&)>;

    /// Joins `mirror`'s views. Collects nothing until attach().
    MirrorView(StateMirror* mirror, Sink sink, QObject* parent = nullptr);
    ~MirrorView() override;

    MirrorView(const MirrorView&) = delete;
    MirrorView& operator=(const MirrorView&) = delete;

    /// The connect-time burst, to this view alone: one schema per distinct
    /// class among the watched objects (first-watched order), one
    /// object.create per watched object with its full property set (watch
    /// order), snapshot.complete, then whatever changed during the burst.
    /// From the first call on, the view collects every change it is told
    /// of. Calling it again (a layout restored, a reconnect) drops what was
    /// pending, since the new burst already carries every current value,
    /// and sends the whole burst again. Another view's pending changes are
    /// never touched.
    void attach();

    /// Sends one delta per object with anything pending, read again from
    /// the live object, and returns how many it sent.
    int flush();

    /// Sends `message` (an object.create or object.destroy from the slice
    /// lifecycle, or a settings.value) through the sink, once this view has
    /// attached. False, sending nothing, before then or after close().
    /// iPhone app Task 73: an object.create of a class this view has not
    /// yet declared (a device's first marker after its burst) is preceded
    /// by that class's schema.
    bool deliver(const SessionMessage& message);

    /// Leaves the mirror, drops everything pending and sends nothing more.
    /// A burst in progress stops at its next message. The Core calls this
    /// when the device's session ends, before the view is deleted.
    void close();

    bool isAttached() const { return m_attached; }
    bool isClosed() const { return m_closed; }

    int pendingObjectCount() const { return m_coalescer.pendingObjectCount(); }
    int pendingPropertyCount() const { return m_coalescer.pendingPropertyCount(); }

private:
    friend class StateMirror;

    /// StateMirror's call for every change this view is not the writer of.
    void noteChange(const QByteArray& objectKey, const QList<MirrorUpdate>& updates);

    void send(const SessionMessage& message);

    QPointer<StateMirror> m_mirror;
    Sink m_sink;
    MirrorCoalescer m_coalescer;
    /// The classes whose schema this view has sent since its last attach.
    QSet<QByteArray> m_announced;
    bool m_attached = false;
    bool m_closed = false;
};

} // namespace NereusSDR
