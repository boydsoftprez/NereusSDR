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
// The correct pattern, captured here: post the protocol disconnect()
// onto the worker via QueuedConnection, then wait on a semaphore that
// the lambda releases when it finishes. main waits up to a timeout
// for that release before calling quit() / wait() / terminate(). The
// lambda also schedules the connection's self-deletion via
// deleteLater(), which Qt processes on the worker as part of the
// event-loop shutdown sequence — so the destructor (and its child
// QTimer destructors) runs on the worker thread, where the timers
// live, and no cross-thread warnings fire.
//
// Issue #83: an earlier version used a plain QueuedConnection
// invokeMethod for both disconnect() AND deleteLater(), relying on
// "Qt will drain the posted events during quit()". On Windows the
// event dispatcher's interrupt path races the queued MetaCall: quit()
// exited the loop before the disconnect lambda was dispatched. On
// HL2 close that left the UDP socket open, metis-stop unsent, three
// timers alive, and the RadioConnection object orphaned with its
// thread gone — the process then crashed during later static-dtor
// cleanup and left the Winsock endpoint in a state that prevented
// Thetis from binding port 51188 on the next session until a reboot
// / `netsh winsock reset`.
//
// PR #108 review (Codex) flagged a follow-on concern with the first
// fix attempt (which used Qt::BlockingQueuedConnection): if the
// worker is stuck inside its own onReadyRead loop (which drains UDP
// datagrams in an unbounded `while (hasPendingDatagrams())`), the
// blocking call would never return and the UI thread would hang
// indefinitely on shutdown — there'd be no way to fall through to
// the existing wait(timeout) + terminate fallback. The semaphore
// pattern keeps the synchronization (we still know the lambda ran
// before we quit) while bounding the worst-case shutdown latency at
// kDispatchTimeoutMs + kPostQuitWaitMs.
//
// The semaphore is held via std::shared_ptr so a late-firing lambda
// after the dispatch timeout doesn't touch a destroyed object.
//
// Both pointer references are set to nullptr on return; the QThread
// container itself is main-thread-owned and safe to delete from the
// caller, which this helper does.

#pragma once

#include "core/RadioConnection.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QSemaphore>
#include <QThread>

#include <memory>

namespace NereusSDR {

// Bounds: dispatch wait covers the time between posting the lambda
// and the worker servicing it; post-quit wait covers the event-loop
// drain of the lambda's deleteLater(). Worst-case shutdown latency is
// kDispatchTimeoutMs + kPostQuitWaitMs + kTerminateWaitMs (3000 + 1000
// + 1000 = 5 s). Empirically the dispatch happens in <1 ms on real
// HL2 traffic; the timeouts are pure safety nets.
inline constexpr int kDispatchTimeoutMs = 3000;
inline constexpr int kPostQuitWaitMs    = 1000;
inline constexpr int kTerminateWaitMs   = 1000;

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
        // Semaphore is held via shared_ptr so a late-firing lambda
        // after the dispatch timeout does not write to a destroyed
        // QSemaphore on the test/UI stack frame. See file header.
        auto dispatched = std::make_shared<QSemaphore>();
        QMetaObject::invokeMethod(c, [c, dispatched]() {
            c->disconnect();
            c->deleteLater();
            dispatched->release();
        });
        if (!dispatched->tryAcquire(1, kDispatchTimeoutMs)) {
            // Worker did not service the queued lambda within the
            // budget — most likely stuck in onReadyRead under heavy
            // UDP ingress. Fall through to the original quit/wait/
            // terminate fallback. The lambda may still fire later
            // before the loop exits; the shared_ptr keeps the
            // semaphore alive until then.
            QLoggingCategory cat("nereus.connection");
            qCWarning(cat).noquote()
                << "teardownWorker: disconnect dispatch timed out after"
                << kDispatchTimeoutMs << "ms — forcing thread quit";
        }
        thread->quit();
        if (!thread->wait(kPostQuitWaitMs)) {
            thread->terminate();
            thread->wait(kTerminateWaitMs);
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
