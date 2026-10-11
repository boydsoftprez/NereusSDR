// =================================================================
// src/core/audio/CaptureShm.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The shared memory and the wake
// signal of the PC mic hand-off between the window and its
// nereus-audio-capture helper (R-AUD-17, D22); no Thetis logic.
//
// The window creates a region and a wake signal under fresh names, sends
// the names to the helper (CaptureProtocol AttachRing), and unlinks the
// names once the helper has attached (RingAttached) or the generation
// ends.  The helper builds its clock matcher's ring in the region; its
// input callback writes the ring and posts the wake, the only system call
// it makes.  The window's waiter thread blocks in waitWake().
//
// POSIX: shm_open / mmap and a named semaphore (sem_open); Windows: a
// named file mapping and an auto-reset event in the session's Local
// namespace.
//
// Design: docs/architecture/2026-10-08-native-audio-engines-design.md
// ("The PC mic hand-off").
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 13 (R-AUD-17). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"

#include <QString>
#include <QtGlobal>

#include <atomic>
#include <cstddef>
#include <memory>

namespace NereusSDR {

// The longest name makeCaptureShmNames may make on POSIX, under macOS's
// 31-character limit for shared memory and semaphore names.
inline constexpr int kCaptureShmNameMaxChars = 30;

struct CaptureShmNames {
    QString memory;
    QString wake;
};

// POSIX "/nrsc-<pid>-<8hex>m" and "/nrsc-<pid>-<8hex>w"; Windows
// "Local\nrsc-<pid>-<8hex>m" and "Local\nrsc-<pid>-<8hex>w".  The hex is
// the random value as eight lower-case digits.
NEREUS_CORE_EXPORT CaptureShmNames makeCaptureShmNames(qint64 pid, quint32 random);

class NEREUS_CORE_EXPORT CaptureShmRegion {
public:
    // Owner side (the window): creates both names, failing if either
    // exists.  The memory is zeroed.  nullptr on any failure (logged).
    static std::unique_ptr<CaptureShmRegion> create(const CaptureShmNames& names,
                                                    std::size_t bytes);
    // Attach side (the helper): opens both names; the memory must be at
    // least bytes long.  nullptr on any failure (logged).
    static std::unique_ptr<CaptureShmRegion> attach(const CaptureShmNames& names,
                                                    std::size_t bytes);

    ~CaptureShmRegion();
    CaptureShmRegion(const CaptureShmRegion&) = delete;
    CaptureShmRegion& operator=(const CaptureShmRegion&) = delete;

    void* data() const;
    std::size_t size() const;

    // POSIX: shm_unlink and sem_unlink, once; Windows: nothing (the
    // objects go with their last handle).  The mapping and the wake stay
    // usable.  The owner also unlinks on destruction.
    void unlinkNames();

    // sem_post / SetEvent.  No lock, no allocation; safe on a device
    // callback.
    void postWake();
    // sem_wait / WaitForSingleObject(INFINITE).  True for a wake, false
    // once shutdownWake() was called (and on a wait error).
    bool waitWake();
    // Sets the stop flag and posts once, so a waitWake() returns false.
    void shutdownWake();

    // Opaque; only create() and attach() can make one.
    struct Impl;
    explicit CaptureShmRegion(std::unique_ptr<Impl> impl);

private:
    std::unique_ptr<Impl> m_impl;
    std::atomic<bool> m_stop{false};
};

} // namespace NereusSDR
