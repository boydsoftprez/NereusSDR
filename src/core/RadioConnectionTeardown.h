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

#include <QDebug>
#include <QMetaObject>
#include <QThread>

namespace NereusSDR {

inline void teardownWorkerThreadedConnection(RadioConnection*& conn,
                                             QThread*& thread)
{
    if (!conn || !thread) {
        qInfo() << "issue-83: teardownWorker — null conn/thread, fast path";
        conn = nullptr;
        if (thread) {
            delete thread;
            thread = nullptr;
        }
        return;
    }

    if (thread->isRunning()) {
        RadioConnection* c = conn;
        qInfo() << "issue-83: teardownWorker — posting disconnect+deleteLater lambda to worker";
        QMetaObject::invokeMethod(c, [c]() {
            qInfo() << "issue-83: teardownWorker lambda — entered (worker thread)"
                    << "thread=" << QThread::currentThread();
            c->disconnect();
            qInfo() << "issue-83: teardownWorker lambda — disconnect() returned; scheduling deleteLater";
            c->deleteLater();
            qInfo() << "issue-83: teardownWorker lambda — deleteLater posted; exiting lambda";
        });
        qInfo() << "issue-83: teardownWorker — calling thread->quit()";
        thread->quit();
        qInfo() << "issue-83: teardownWorker — waiting for thread to finish (3s timeout)";
        if (!thread->wait(3000)) {
            qWarning() << "issue-83: teardownWorker — wait timed out; calling terminate()";
            thread->terminate();
            thread->wait(1000);
        } else {
            qInfo() << "issue-83: teardownWorker — thread finished cleanly";
        }
    }

    // conn was destroyed on the worker thread by the DeferredDelete
    // that Qt processes during quit(). Just null out the caller's
    // pointer.
    conn = nullptr;

    qInfo() << "issue-83: teardownWorker — deleting QThread container";
    delete thread;
    thread = nullptr;
    qInfo() << "issue-83: teardownWorker — complete";
}

} // namespace NereusSDR
