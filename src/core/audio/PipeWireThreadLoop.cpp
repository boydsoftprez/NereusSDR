// =================================================================
// src/core/audio/PipeWireThreadLoop.cpp  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE
#include "core/audio/PipeWireThreadLoop.h"

#include <QLoggingCategory>
#include <pipewire/pipewire.h>

Q_LOGGING_CATEGORY(lcPw, "nereussdr.pipewire")

namespace NereusSDR {

PipeWireThreadLoop::PipeWireThreadLoop()
{
    pw_init(nullptr, nullptr);
}

PipeWireThreadLoop::~PipeWireThreadLoop()
{
    // ORDERING CONTRACT (Task 6 reviewer — forward-looking for Task 8+):
    // When PipeWireStream objects exist (landing in Task 8), the owning
    // AudioEngine MUST stop the DSP producer thread BEFORE destroying
    // this loop. AudioRingSpsc::pushCopy yields in a spin when full;
    // if the pw_stream drain thread is torn down while a producer is
    // mid-push, the producer spins forever. AudioEngine's dtor therefore
    // tears down DSP-facing state (stops feeding rxBlockReady) BEFORE
    // destroying the PipeWireThreadLoop. Do not reorder.
    // Stop the loop thread BEFORE disconnect/destroy so no pw callback
    // races with teardown. pw_thread_loop_destroy() also stops internally,
    // but that runs AFTER we've disconnected core/destroyed context — too
    // late. The duplicate-stop is intentional, not accidental.
    if (m_loop) {
        pw_thread_loop_stop(m_loop);
    }
    if (m_core)    { pw_core_disconnect(m_core); }
    if (m_context) { pw_context_destroy(m_context); }
    if (m_loop)    { pw_thread_loop_destroy(m_loop); }
    pw_deinit();
}

bool PipeWireThreadLoop::connect()
{
    m_loop = pw_thread_loop_new("nereussdr.pw", nullptr);
    if (!m_loop) {
        qCWarning(lcPw) << "pw_thread_loop_new failed";
        return false;
    }
    if (pw_thread_loop_start(m_loop) < 0) {
        qCWarning(lcPw) << "pw_thread_loop_start failed";
        return false;
    }

    lock();
    m_context = pw_context_new(pw_thread_loop_get_loop(m_loop),
                               nullptr, 0);
    if (!m_context) {
        unlock();
        qCWarning(lcPw) << "pw_context_new failed";
        return false;
    }
    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        unlock();
        qCWarning(lcPw) << "pw_context_connect failed (socket unreachable?)";
        return false;
    }

    // Record server version for the Setup backend strip.
    m_serverVersion = QString::fromUtf8(pw_get_library_version());

    unlock();
    qCInfo(lcPw) << "PipeWire thread loop connected — server version"
                 << m_serverVersion;
    return true;
}

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
