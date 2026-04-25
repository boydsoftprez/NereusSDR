// =================================================================
// src/core/audio/PipeWireThreadLoop.h  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-23 — created. AI-assisted via Claude Code.
// =================================================================
#pragma once

#ifdef NEREUS_HAVE_PIPEWIRE

#include <QString>
#include <pipewire/pipewire.h>

namespace NereusSDR {

class PipeWireThreadLoop {
public:
    PipeWireThreadLoop();
    ~PipeWireThreadLoop();

    PipeWireThreadLoop(const PipeWireThreadLoop&)            = delete;
    PipeWireThreadLoop& operator=(const PipeWireThreadLoop&) = delete;

    // True when connect() succeeded. Until then the object is
    // safe to destroy but may not be used for stream creation.
    bool connect();

    pw_thread_loop* loop() const { return m_loop; }
    pw_core*        core() const { return m_core; }

    QString serverVersion() const { return m_serverVersion; }

    void lock()   { if (m_loop) { pw_thread_loop_lock(m_loop); } }
    void unlock() { if (m_loop) { pw_thread_loop_unlock(m_loop); } }

private:
    pw_thread_loop* m_loop    = nullptr;
    pw_context*     m_context = nullptr;
    pw_core*        m_core    = nullptr;

    QString m_serverVersion;
};

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
