// src/core/RadioConnectionTeardown.h
//
// Shared helper for tearing down a RadioConnection that lives on its
// own worker QThread. Used by RadioModel during disconnect / shutdown
// and by the regression test tst_cross_thread_teardown.
//
// Why this exists: P1RadioConnection and P2RadioConnection create
// QTimers inside init(), which runs on the worker thread after
// moveToThread(). Those timers are thread-affined to the worker.
// If the main thread destroys the RadioConnection directly after
// joining the worker, each child QTimer's destructor calls killTimer
// on the wrong thread — Qt prints "Timers cannot be stopped from
// another thread" and on Windows the process can terminate abnormally
// during cleanup.
//
// The correct pattern, captured here: run the protocol disconnect()
// on the worker thread via a BlockingQueuedConnection so main waits
// until the worker has actually executed it. THEN post deleteLater()
// on the worker, call quit(), and wait. The deleteLater is already in
// the worker's queue, so Qt's event-loop shutdown processes the
// DeferredDelete event on the worker thread — where the timers live
// — and no cross-thread warnings fire.
//
// Issue #83: an earlier version of this helper used a plain
// QueuedConnection invokeMethod for both disconnect() AND
// deleteLater(), relying on "Qt will drain the posted events during
// quit()". On Windows the event dispatcher's interrupt path races the
// queued MetaCall: quit() exited the loop before the disconnect
// lambda was dispatched. On HL2 close that left the UDP socket open,
// metis-stop unsent, three timers alive, and the RadioConnection
// object orphaned with its thread gone — the process then crashed
// during later static-dtor cleanup and left the Winsock endpoint in a
// state that prevented Thetis from binding port 51188 on the next
// session until a reboot / `netsh winsock reset`.
//
// Both pointer references are set to nullptr on return; the QThread
// container itself is main-thread-owned and safe to delete from the
// caller, which this helper does.

#pragma once

#include "core/RadioConnection.h"

#include <QMetaObject>
#include <QThread>

namespace NereusSDR {

inline void teardownWorkerThreadedConnection(RadioConnection*& conn,
                                             QThread*& thread)
{
    if (!conn || !thread) {
        conn = nullptr;
        if (thread) {
            delete thread;
            thread = nullptr;
        }
        return;
    }

    if (thread->isRunning()) {
        RadioConnection* c = conn;
        // Blocking: main waits here until disconnect() has actually
        // executed on the worker. After this returns the timers are
        // stopped, metis-stop has been sent, and the socket is closed
        // (see P1/P2 RadioConnection::disconnect). Issue #83.
        QMetaObject::invokeMethod(c, [c]() {
            c->disconnect();
            c->deleteLater();
        }, Qt::BlockingQueuedConnection);
        thread->quit();
        if (!thread->wait(3000)) {
            thread->terminate();
            thread->wait(1000);
        }
    }

    // conn was destroyed on the worker thread by the DeferredDelete
    // that Qt processes during quit(). Just null out the caller's
    // pointer.
    conn = nullptr;

    delete thread;
    thread = nullptr;
}

} // namespace NereusSDR
