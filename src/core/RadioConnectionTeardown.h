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
// The correct pattern, captured here: queue a lambda on the worker
// that runs the protocol disconnect() and then schedules self-
// deletion via deleteLater(). When quit() fires, the worker's event
// loop processes the pending DeferredDelete as part of its shutdown
// sequence, so the destructor runs on the worker thread — where the
// timers live — and no cross-thread warnings fire.
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
        QMetaObject::invokeMethod(c, [c]() {
            c->disconnect();
            c->deleteLater();
        });
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
