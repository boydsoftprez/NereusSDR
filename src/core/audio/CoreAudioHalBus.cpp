// =================================================================
// src/core/audio/CoreAudioHalBus.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR source:
//   src/core/VirtualAudioBridge.cpp
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. NereusSDR is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per AetherSDR convention.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Ported/adapted in C++20 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Adapted to NereusSDR IAudioBus contract:
//                 monolithic VirtualAudioBridge decomposed into per-endpoint
//                 CoreAudioHalBus instances (Role enum for Vax1..4 / TxInput),
//                 QObject/signals dropped in favor of atomic metering,
//                 silence-fill + TX poll timers deferred to Phase 3M.
//                 Shm paths: /aethersdr-dax-* → /nereussdr-vax-*.
//                 Sample rate: 24 kHz → 48 kHz (spec §8.1).
// =================================================================

#include "CoreAudioHalBus.h"

#include "core/LogCategories.h"
#include "core/PerfMonitor.h"

#include <QLoggingCategory>

#ifdef Q_OS_MAC
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

namespace NereusSDR {

namespace {

// Agreed shm-name table (Task 5.1 D7). Stored as string literals so the .h
// can hand out a `const char*` without owning a QString or QByteArray.
constexpr const char* kShmNameVax1   = "/nereussdr-vax-1";
constexpr const char* kShmNameVax2   = "/nereussdr-vax-2";
constexpr const char* kShmNameVax3   = "/nereussdr-vax-3";
constexpr const char* kShmNameVax4   = "/nereussdr-vax-4";
constexpr const char* kShmNameTxIn   = "/nereussdr-vax-tx";

const char* shmNameForRole(CoreAudioHalBus::Role role) {
    switch (role) {
        case CoreAudioHalBus::Role::Vax1:    return kShmNameVax1;
        case CoreAudioHalBus::Role::Vax2:    return kShmNameVax2;
        case CoreAudioHalBus::Role::Vax3:    return kShmNameVax3;
        case CoreAudioHalBus::Role::Vax4:    return kShmNameVax4;
        case CoreAudioHalBus::Role::TxInput: return kShmNameTxIn;
    }
    return kShmNameVax1;
}

// Backlog guards for the TX drain path — ported verbatim from
// VirtualAudioBridge::readTxAudio so the TX consumer keeps near-real-time
// latency when the HAL plugin has written more than we can drain in a tick.
constexpr uint32_t kTargetBacklogSamples = 256 * 2;  // 256 frames ≈ 5.3 ms @ 48 kHz stereo
constexpr uint32_t kMaxBacklogSamples    = 768 * 2;  // 768 frames ≈ 16 ms @ 48 kHz stereo

} // namespace

CoreAudioHalBus::CoreAudioHalBus(Role role)
    : m_role(role)
    , m_shmName(shmNameForRole(role))
    , m_backendName(QStringLiteral("CoreAudio HAL (VAX)"))
{}

CoreAudioHalBus::~CoreAudioHalBus() {
    close();
}

bool CoreAudioHalBus::open(const AudioFormat& format) {
#ifndef Q_OS_MAC
    (void)format;
    m_err = QStringLiteral("CoreAudioHalBus is macOS-only");
    return false;
#else
    if (m_open) {
        return true;
    }

    // Fixed format — the HAL plugin hard-codes 48 kHz stereo float32, and the
    // shm ring sizes itself accordingly. Reject anything else cleanly rather
    // than silently mis-routing samples.
    if (format.sampleRate != 48000
     || format.channels   != 2
     || format.sample     != AudioFormat::Sample::Float32) {
        m_err = QStringLiteral("CoreAudioHalBus requires 48000 Hz stereo float32 "
                               "(got %1 Hz, %2 ch, sample=%3)")
                    .arg(format.sampleRate)
                    .arg(format.channels)
                    .arg(static_cast<int>(format.sample));
        return false;
    }

    // Try to open an existing segment first — the HAL plugin (coreaudiod) may
    // already have it mapped. If that fails, create it at the canonical size.
    int fd = ::shm_open(m_shmName, O_RDWR, 0666);
    bool created = false;
    if (fd < 0) {
        fd = ::shm_open(m_shmName, O_CREAT | O_RDWR, 0666);
        if (fd < 0) {
            m_err = QStringLiteral("shm_open(%1) failed: errno=%2")
                        .arg(QString::fromUtf8(m_shmName)).arg(errno);
            qCWarning(lcAudio) << "CoreAudioHalBus:" << m_err;
            return false;
        }
        if (::ftruncate(fd, sizeof(VaxShmBlock)) != 0) {
            m_err = QStringLiteral("ftruncate(%1) failed: errno=%2")
                        .arg(QString::fromUtf8(m_shmName)).arg(errno);
            qCWarning(lcAudio) << "CoreAudioHalBus:" << m_err;
            ::close(fd);
            return false;
        }
        created = true;
    }

    // fstat guard — mirrors the Task 5.1 plugin-side fix (I3). Refuse to mmap
    // a stale/undersized segment, otherwise reading past the end raises SIGBUS.
    struct stat st{};
    if (::fstat(fd, &st) != 0
     || static_cast<size_t>(st.st_size) < sizeof(VaxShmBlock)) {
        m_err = QStringLiteral("shm segment %1 undersized (%2 bytes, need %3)")
                    .arg(QString::fromUtf8(m_shmName))
                    .arg(static_cast<qint64>(st.st_size))
                    .arg(static_cast<qint64>(sizeof(VaxShmBlock)));
        qCWarning(lcAudio) << "CoreAudioHalBus:" << m_err;
        ::close(fd);
        return false;
    }

    void* ptr = ::mmap(nullptr, sizeof(VaxShmBlock),
                       PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        m_err = QStringLiteral("mmap(%1) failed: errno=%2")
                    .arg(QString::fromUtf8(m_shmName)).arg(errno);
        qCWarning(lcAudio) << "CoreAudioHalBus:" << m_err;
        ::close(fd);
        return false;
    }

    m_shmFd = fd;
    m_block = static_cast<VaxShmBlock*>(ptr);

    // Re-initialize header fields. We own the segment's header while we're
    // open — the plugin only reads writePos/readPos/active.
    m_block->writePos.store(0, std::memory_order_relaxed);
    m_block->readPos.store(0, std::memory_order_relaxed);
    m_block->sampleRate = 48000;
    m_block->channels   = 2;
    std::memset(&m_block->reserved[0], 0, sizeof(m_block->reserved));
    m_block->active.store(1, std::memory_order_release);

    m_negFormat = format;
    m_open = true;
    m_err.clear();

    qCInfo(lcAudio) << "CoreAudioHalBus: opened" << m_shmName
                    << (created ? "(created)" : "(attached)")
                    << "role=" << static_cast<int>(m_role);
    return true;
#endif
}

void CoreAudioHalBus::close() {
#ifdef Q_OS_MAC
    if (m_block) {
        // Tell the plugin we're gone so it stops draining stale data.
        m_block->active.store(0, std::memory_order_release);
        ::munmap(m_block, sizeof(VaxShmBlock));
        m_block = nullptr;
    }
    if (m_shmFd >= 0) {
        ::close(m_shmFd);
        m_shmFd = -1;
    }
    // NOTE: intentionally no shm_unlink here. The HAL plugin (coreaudiod)
    // may still have the segment mapped; unlinking now would orphan its
    // file descriptor. Matches the AetherSDR VirtualAudioBridge pattern.
#endif
    m_open = false;
    m_rxLevel.store(0.0f, std::memory_order_release);
    m_txLevel.store(0.0f, std::memory_order_release);
    m_meterCounter = 0;
}

qint64 CoreAudioHalBus::push(const char* data, qint64 bytes) {
    // TODO(phase3m): Re-add TX-driven silence fill so the HAL plugin's ring
    // keeps advancing during TX state. See AetherSDR VirtualAudioBridge::
    // setTransmitting() / feedSilenceToAllChannels() for the pattern — a
    // future TX state source will drive a timer that writes zeros into every
    // active Vax1..4 segment so WSJT-X/VARA don't see a stalled input.
    if (!isProducer()) {
        return -1;
    }
    if (!m_open || !m_block || data == nullptr || bytes <= 0) {
        return 0;
    }

#ifdef Q_OS_MAC
    const auto* samples = reinterpret_cast<const float*>(data);
    const int numSamples = static_cast<int>(bytes / sizeof(float));

    // SPSC invariant: this thread is the sole writer, so a relaxed load of
    // our own previously-released store is sound (program order provides
    // the necessary dependency). The release store below is what the plugin's
    // acquire load synchronizes against.
    uint32_t wp = m_block->writePos.load(std::memory_order_relaxed);

    // 2026-05-26 KG4VCF perf instrumentation: report the ring fill
    // level BEFORE this push so the perf overlay can show how much
    // unread audio the kernel-side plugin has queued.  Healthy
    // steady-state is tens of ms; if the min over the window drops
    // toward 0 we are about to underrun even before the OS reports it.
    //
    // Ring is 48 kHz stereo float (96 samples per ms).  When the
    // plugin lags or our DSP thread is slow, fill grows; when our
    // DSP is preempted, fill shrinks because the plugin keeps draining.
    {
        const uint32_t rp = m_block->readPos.load(std::memory_order_acquire);
        const uint32_t fillSamples = wp - rp;  // SPSC uint wrap-safe
        constexpr double kSamplesPerMs = 48.0 * 2.0;  // 48 kHz stereo
        const double fillMs =
            static_cast<double>(fillSamples) / kSamplesPerMs;
        NereusSDR::PerfMonitor::instance().recordAudioFillMs(fillMs);
    }

    for (int i = 0; i < numSamples; ++i) {
        m_block->ringBuffer[wp % VaxShmBlock::RING_SIZE] = samples[i];
        ++wp;
    }
    m_block->writePos.store(wp, std::memory_order_release);

    // RMS meter on the left channel (indices 0, 2, 4, …). Strided so the hot
    // path doesn't walk the full block every call.
    if (++m_meterCounter % kMeterStride == 0 && numSamples > 0) {
        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < numSamples; i += 2) {
            sum += samples[i] * samples[i];
            ++count;
        }
        const float rms = (count > 0) ? std::sqrt(sum / static_cast<float>(count)) : 0.0f;
        m_rxLevel.store(rms, std::memory_order_release);
    }

    return bytes;
#else
    (void)data;
    return 0;
#endif
}

qint64 CoreAudioHalBus::pull(char* data, qint64 maxBytes) {
    if (isProducer()) {
        return -1;
    }
    if (!m_open || !m_block || data == nullptr || maxBytes <= 0) {
        return 0;
    }

#ifdef Q_OS_MAC
    // Drain TX audio written by the HAL plugin. Ported verbatim from
    // VirtualAudioBridge::readTxAudio — acquire loads, wrap-skip if the writer
    // has lapped the reader, backlog guards to keep near-real-time latency.
    uint32_t rp = m_block->readPos.load(std::memory_order_acquire);
    uint32_t wp = m_block->writePos.load(std::memory_order_acquire);

    uint32_t available = wp - rp;
    if (available == 0) {
        return 0;
    }

    // Defense against 32-bit counter wraparound: wp/rp are uint32 and wrap
    // after ~12 hours of 48 kHz stereo float pushes. If only one side wraps,
    // the unsigned `wp - rp` subtraction can produce a bogus huge "available"
    // value. Skip ahead to a recent window. The MAX/TARGET backlog guard
    // below usually fires first under any realistic timing; this branch is
    // the belt-and-suspenders against the pathological wrap case.
    if (available > VaxShmBlock::RING_SIZE) {
        rp = wp - VaxShmBlock::RING_SIZE / 2;  // jump to recent data
        available = wp - rp;
    }

    // Low-latency guard: if backlog grows, skip stale audio and keep only
    // a small recent window near the writer head.
    if (available > kMaxBacklogSamples) {
        rp = wp - kTargetBacklogSamples;
        available = wp - rp;
    }

    uint32_t maxFloats = static_cast<uint32_t>(maxBytes / sizeof(float));
    uint32_t totalSamples = std::min(available, maxFloats);

    auto* dst = reinterpret_cast<float*>(data);
    for (uint32_t i = 0; i < totalSamples; ++i) {
        dst[i] = m_block->ringBuffer[rp % VaxShmBlock::RING_SIZE];
        ++rp;
    }

    m_block->readPos.store(rp, std::memory_order_release);

    // TX level meter (strided).
    if (++m_meterCounter % kMeterStride == 0 && totalSamples > 0) {
        float sum = 0.0f;
        uint32_t count = 0;
        for (uint32_t i = 0; i < totalSamples; i += 2) {
            sum += dst[i] * dst[i];
            ++count;
        }
        const float rms = (count > 0) ? std::sqrt(sum / static_cast<float>(count)) : 0.0f;
        m_txLevel.store(rms, std::memory_order_release);
    }

    return static_cast<qint64>(totalSamples) * static_cast<qint64>(sizeof(float));
#else
    (void)data;
    (void)maxBytes;
    return 0;
#endif
}

} // namespace NereusSDR
