// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/media/DaemonAudioSource.cpp  (NereusSDR)
// =================================================================

#include "DaemonAudioSource.h"

#include "core/AudioEngine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>

namespace NereusSDR {

class DaemonAudioSource::Bridge final : public MasterMixAudioTap, public SliceAudioTap {
public:
    Bridge()
        : m_captureClock([] {
            return static_cast<qint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        })
    {
    }

    // Control thread, only while stopped: consume() reads the clock.
    bool setCaptureClock(DaemonAudioSource::CaptureClock clock)
    {
        if (m_running.load(std::memory_order_acquire)) {
            return false;
        }
        if (clock) {
            m_captureClock = std::move(clock);
        }
        return true;
    }

    // Threading contract. consume() runs on the one DSP thread that drains
    // MasterMixer (its producers are serialised on that thread), so the
    // assembly state below is producer-owned and needs no lock. The control
    // thread touches it only in start() and stop(), which run while
    // AudioEngine's tap gate keeps consume() out; the gate's seq_cst
    // admission counter orders those writes against the next callback.
    // m_mutex guards only the ready ring shared with the consumer, and the
    // producer only ever try-locks it.
    void start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        resetLocked();
        resetProducerState();
        m_contentionRetries.store(0, std::memory_order_relaxed);
        m_contentionLosses.store(0, std::memory_order_relaxed);
        m_ringFullDrops.store(0, std::memory_order_relaxed);
        m_invalidIngressDrops.store(0, std::memory_order_relaxed);
        m_nextFramePosition.store(0, std::memory_order_relaxed);
        m_running.store(true, std::memory_order_release);
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(false, std::memory_order_release);
        resetLocked();
        resetProducerState();
    }

    bool isRunning() const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }

    std::optional<DaemonAudioBlock> takeBlock()
    {
        // Lock-free fast path: the sender polls every 10 ms and usually ends
        // its drain on an empty ring, so an empty or stopped bridge answers
        // without touching the lock the producer hands packets over with.
        if (!m_running.load(std::memory_order_acquire)
            || m_readyCount.load(std::memory_order_acquire) == 0) {
            return std::nullopt;
        }

        DaemonAudioBlock block;
        block.pcmInterleaved.resize(DaemonAudioSource::kBlockSamples);
        {
            // QVector allocation is deliberately outside the shared bridge
            // mutex. The critical section is one fixed-size copy plus ring
            // bookkeeping, and the producer never waits for it: a hand-over
            // that finds the lock busy stays pending and retries later.
            std::lock_guard<std::mutex> lock(m_mutex);
#ifdef NEREUS_BUILD_TESTS
            if (m_takeBlockLockedHookForTest) {
                m_takeBlockLockedHookForTest();
            }
#endif
            const int readyCount = m_readyCount.load(std::memory_order_relaxed);
            if (!m_running.load(std::memory_order_relaxed) || readyCount == 0) {
                return std::nullopt;
            }
            block.samplePosition = m_readyPositions[m_readIndex];
            block.capturedNs = m_readyCapturedNs[m_readIndex];
            std::memcpy(block.pcmInterleaved.data(), m_ready[m_readIndex].data(),
                        sizeof(float) * DaemonAudioSource::kBlockSamples);
            m_readIndex = (m_readIndex + 1) % DaemonAudioSource::kQueueBlocks;
            m_readyCount.store(readyCount - 1, std::memory_order_release);
        }
        return block;
    }

    DaemonAudioSourceTelemetry telemetry() const noexcept
    {
        DaemonAudioSourceTelemetry snapshot;
        snapshot.capturedValidRateFrames =
            m_nextFramePosition.load(std::memory_order_relaxed);
        snapshot.contentionRetries = m_contentionRetries.load(std::memory_order_relaxed);
        snapshot.contentionLosses = m_contentionLosses.load(std::memory_order_relaxed);
        snapshot.ringFullDrops = m_ringFullDrops.load(std::memory_order_relaxed);
        snapshot.invalidIngressDrops =
            m_invalidIngressDrops.load(std::memory_order_relaxed);
        snapshot.sourceDropEvents = snapshot.contentionLosses + snapshot.ringFullDrops
            + snapshot.invalidIngressDrops;
        return snapshot;
    }

    void dropIngressForTest(int frames) noexcept
    {
        if (frames <= 0) {
            return;
        }

        // Models a valid-rate callback whose source frames were reserved but
        // whose samples never reached the bridge. The next accepted callback
        // sees the reservation gap and realigns to the packet grid.
        m_nextFramePosition.fetch_add(static_cast<quint64>(frames),
                                      std::memory_order_relaxed);
        m_invalidIngressDrops.fetch_add(1, std::memory_order_relaxed);
        m_discontinuity.store(true, std::memory_order_release);
    }

    // R-R3-43, slice source only: the MOX gate withheld `frames` of this
    // receiver's audio. Reserve them without samples, as a lost ingress is
    // reserved, so the next block keeps its true grid position and RTP shows
    // the gap. A withheld block is not a loss, so no drop is counted.
    void skip(int frames, int sampleRateHz) noexcept override
    {
        if (frames <= 0 || sampleRateHz != DaemonAudioSource::kSampleRateHz) {
            return;
        }
        m_nextFramePosition.fetch_add(static_cast<quint64>(frames),
                                      std::memory_order_relaxed);
        m_discontinuity.store(true, std::memory_order_release);
    }

    void consume(const float* samples, int frames, int sampleRateHz) noexcept override
    {
        if (samples == nullptr || frames <= 0 || sampleRateHz != DaemonAudioSource::kSampleRateHz) {
            m_invalidIngressDrops.fetch_add(1, std::memory_order_relaxed);
            m_discontinuity.store(true, std::memory_order_release);
            return;
        }
        const quint64 callbackFirstFrame =
            m_nextFramePosition.fetch_add(static_cast<quint64>(frames),
                                          std::memory_order_relaxed);
        if (!m_running.load(std::memory_order_acquire)) {
            return;
        }

        // A packet the consumer's critical section kept us from handing
        // over last time gets one more non-blocking attempt per callback.
        if (m_pendingValid) {
            tryHandOverPending();
        }

        const bool hadDiscontinuity =
            m_discontinuity.exchange(false, std::memory_order_acq_rel);
        // Defensive: reservations are made in callback order on the single
        // producer thread, so an older reservation cannot follow a newer one.
        // Should that contract ever break, reject the callback rather than
        // let packet contents run backward in time.
        if (callbackFirstFrame < m_lastAcceptedEndFrame) {
            discardPartialAndRealign(m_lastAcceptedEndFrame);
            m_invalidIngressDrops.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (hadDiscontinuity || callbackFirstFrame > m_lastAcceptedEndFrame) {
            // Reserved source frames never reached the bridge. Do not join
            // this later reservation to the old partial block. Discard
            // through the next original 40 ms boundary so the next emitted
            // RTP packet represents the loss as an integral packet gap
            // rather than an unaligned timestamp step.
            discardPartialAndRealign(callbackFirstFrame);
        }
        m_lastAcceptedEndFrame = callbackFirstFrame + static_cast<quint64>(frames);

        int consumedFrames = 0;
        while (consumedFrames < frames) {
            const quint64 currentFrame = callbackFirstFrame
                + static_cast<quint64>(consumedFrames);
            if (currentFrame < m_discardBeforeFrame) {
                const quint64 skippedFrames = std::min(
                    static_cast<quint64>(frames - consumedFrames),
                    m_discardBeforeFrame - currentFrame);
                consumedFrames += static_cast<int>(skippedFrames);
                continue;
            }
            if (m_fillSamples == 0) {
                m_assemblingFirstFrame = currentFrame;
            }
            const int freeFrames = DaemonAudioSource::kBlockFrames
                - (m_fillSamples / DaemonAudioSource::kChannels);
            const int copyFrames = std::min(freeFrames, frames - consumedFrames);
            const int copySamples = copyFrames * DaemonAudioSource::kChannels;
            std::memcpy(m_assembly[m_assemblingSlot].data() + m_fillSamples,
                        samples + static_cast<size_t>(consumedFrames)
                            * DaemonAudioSource::kChannels,
                        static_cast<size_t>(copySamples) * sizeof(float));
            m_fillSamples += copySamples;
            consumedFrames += copyFrames;

            if (m_fillSamples == DaemonAudioSource::kBlockSamples) {
                completeAssembledPacket();
                m_fillSamples = 0;
            }
        }
    }

#ifdef NEREUS_BUILD_TESTS
    std::function<void()> m_takeBlockLockedHookForTest;
#endif

private:
    static quint64 nextBlockBoundary(quint64 frame) noexcept
    {
        const quint64 remainder = frame % DaemonAudioSource::kBlockFrames;
        return remainder == 0
            ? frame
            : frame + (DaemonAudioSource::kBlockFrames - remainder);
    }

    // Producer thread only. The just-filled assembly slot holds one whole
    // packet at m_assemblingFirstFrame.
    void completeAssembledPacket() noexcept
    {
        // R-R3-35: the block is complete now; a hand-over deferred by lock
        // contention keeps this time, not the later hand-over's.
        const qint64 capturedNs = m_captureClock();
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            m_contentionRetries.fetch_add(1, std::memory_order_relaxed);
            if (m_pendingValid) {
                // A second packet completed while the first still waits for
                // the lock (the consumer held it for a whole 40 ms). Drop
                // the newest, as the full-ring rule does, so the pending
                // packet and the queued ones stay a continuous prefix. The
                // dropped packet's frames stay reserved, so the next packet
                // keeps its true grid position and RTP shows the gap.
                m_contentionLosses.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            // Keep the finished packet where it is and assemble the next
            // one into the other slot. No copy, no wait.
            m_pendingValid = true;
            m_pendingSlot = m_assemblingSlot;
            m_pendingFirstFrame = m_assemblingFirstFrame;
            m_pendingCapturedNs = capturedNs;
            m_assemblingSlot = 1 - m_assemblingSlot;
            return;
        }
        if (m_pendingValid) {
            pushLocked(m_pendingSlot, m_pendingFirstFrame, m_pendingCapturedNs);
            m_pendingValid = false;
        }
        pushLocked(m_assemblingSlot, m_assemblingFirstFrame, capturedNs);
    }

    // Producer thread only.
    void tryHandOverPending() noexcept
    {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            m_contentionRetries.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        pushLocked(m_pendingSlot, m_pendingFirstFrame, m_pendingCapturedNs);
        m_pendingValid = false;
    }

    void pushLocked(int slot, quint64 firstFrame, qint64 capturedNs) noexcept
    {
        const int readyCount = m_readyCount.load(std::memory_order_relaxed);
        if (readyCount == DaemonAudioSource::kQueueBlocks) {
            // Drop the newest completed block. Keeping queued blocks gives
            // the consumer a continuous prefix and avoids any producer-side
            // allocation or blocking.
            m_ringFullDrops.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        std::memcpy(m_ready[m_writeIndex].data(), m_assembly[slot].data(),
                    sizeof(float) * DaemonAudioSource::kBlockSamples);
        m_readyPositions[m_writeIndex] = firstFrame;
        m_readyCapturedNs[m_writeIndex] = capturedNs;
        m_writeIndex = (m_writeIndex + 1) % DaemonAudioSource::kQueueBlocks;
        m_readyCount.store(readyCount + 1, std::memory_order_release);
    }

    void discardPartialAndRealign(quint64 firstAvailableFrame) noexcept
    {
        m_fillSamples = 0;
        m_discardBeforeFrame = nextBlockBoundary(firstAvailableFrame);
    }

    void resetLocked()
    {
        m_readIndex = 0;
        m_writeIndex = 0;
        m_readyCount.store(0, std::memory_order_release);
    }

    // Control thread, only while the engine's tap gate excludes consume().
    void resetProducerState() noexcept
    {
        m_fillSamples = 0;
        m_assemblingSlot = 0;
        m_pendingValid = false;
        m_pendingSlot = 0;
        m_pendingFirstFrame = 0;
        m_pendingCapturedNs = 0;
        m_assemblingFirstFrame = 0;
        m_lastAcceptedEndFrame = 0;
        m_discardBeforeFrame = 0;
        m_discontinuity.store(false, std::memory_order_release);
    }

    // Shared with the consumer; guarded by m_mutex. m_readyCount is also
    // read without the lock by takeBlock()'s empty-ring fast path.
    mutable std::mutex m_mutex;
    std::array<std::array<float, DaemonAudioSource::kBlockSamples>,
               DaemonAudioSource::kQueueBlocks> m_ready{};
    std::array<quint64, DaemonAudioSource::kQueueBlocks> m_readyPositions{};
    std::array<qint64, DaemonAudioSource::kQueueBlocks> m_readyCapturedNs{};
    int m_readIndex = 0;
    int m_writeIndex = 0;
    std::atomic<int> m_readyCount{0};

    // Producer-owned assembly: two packet slots, one being filled and one
    // that may hold a finished packet awaiting hand-over.
    std::array<std::array<float, DaemonAudioSource::kBlockSamples>, 2> m_assembly{};
    int m_assemblingSlot = 0;
    int m_fillSamples = 0;
    bool m_pendingValid = false;
    int m_pendingSlot = 0;
    quint64 m_pendingFirstFrame = 0;
    qint64 m_pendingCapturedNs = 0;
    quint64 m_assemblingFirstFrame = 0;
    quint64 m_lastAcceptedEndFrame = 0;
    quint64 m_discardBeforeFrame = 0;

    // Written only while stopped (setCaptureClock); read by consume().
    DaemonAudioSource::CaptureClock m_captureClock;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_discontinuity{false};
    std::atomic<std::uint64_t> m_contentionRetries{0};
    std::atomic<std::uint64_t> m_contentionLosses{0};
    std::atomic<std::uint64_t> m_ringFullDrops{0};
    std::atomic<std::uint64_t> m_invalidIngressDrops{0};
    std::atomic<quint64> m_nextFramePosition{0};
};

DaemonAudioSource::DaemonAudioSource(QObject* parent)
    : QObject(parent)
    , m_bridge(std::make_unique<Bridge>())
{
}

DaemonAudioSource::~DaemonAudioSource()
{
    stop();
}

void DaemonAudioSource::setAudioEngine(AudioEngine* audioEngine)
{
    if (m_audioEngine == audioEngine) {
        return;
    }
    stop();
    m_audioEngine = audioEngine;
}

AudioEngine* DaemonAudioSource::audioEngine() const noexcept
{
    return m_audioEngine.data();
}

bool DaemonAudioSource::setSliceSource(int sliceId)
{
    if (isRunning()
        || (sliceId < 0 && sliceId != kMasterMix && sliceId != kSpeakersMix
            && sliceId != kHeadphonesMix)) {
        return false;
    }
    m_sliceId = sliceId;
    return true;
}

bool DaemonAudioSource::setOwnerMix(int slot)
{
    if (isRunning() || slot < -1 || slot >= AudioEngine::kMaxOwnerMixes) {
        return false;
    }
    m_ownerMix = slot;
    return true;
}

void DaemonAudioSource::detachFromEngine()
{
    if (m_audioEngine.isNull()) {
        return;
    }
    // The engine's clear gates wait for an admitted borrowed-pointer call to
    // return before this bridge's state is reset. Only this source's own
    // kind is cleared: clearing the master tap closes its gate for a moment,
    // and a receiver stream stopping must not cost the speakers' stream a
    // block. setSliceSource() is refused while running, so the tap this
    // bridge holds is always the current kind.
    // iPhone app Task 76: an owner mix's own taps.
    if (m_ownerMix >= 0 && (m_sliceId == kMasterMix || m_sliceId == kSpeakersMix)) {
        m_audioEngine->clearOwnerMixAudioTap(m_ownerMix, m_bridge.get());
    } else if (m_ownerMix >= 0 && m_sliceId == kHeadphonesMix) {
        m_audioEngine->clearOwnerHeadphonesMixAudioTap(m_ownerMix, m_bridge.get());
    } else if (m_sliceId == kMasterMix || m_sliceId == kSpeakersMix) {
        m_audioEngine->clearMasterMixAudioTap(m_bridge.get());
    } else if (m_sliceId == kHeadphonesMix) {
        m_audioEngine->clearHeadphonesMixAudioTap(m_bridge.get());
    } else {
        m_audioEngine->clearSliceAudioTap(m_bridge.get());
    }
}

void DaemonAudioSource::start()
{
    if (m_audioEngine.isNull()) {
        return;
    }

    // A repeated start is another capture epoch. Detach and quiesce first so
    // no prior callback can append between the reset and the new install.
    detachFromEngine();
    // Start/reset before publishing the bridge. AudioEngine's install gate
    // means the DSP thread cannot enter consume() until after this returns.
    m_bridge->start();
    if (m_ownerMix >= 0 && (m_sliceId == kMasterMix || m_sliceId == kSpeakersMix)) {
        if (!m_audioEngine->setOwnerMixAudioTap(m_ownerMix, m_bridge.get(),
                                                /*speakersOnly=*/m_sliceId == kSpeakersMix)) {
            m_bridge->stop();
        }
    } else if (m_ownerMix >= 0 && m_sliceId == kHeadphonesMix) {
        if (!m_audioEngine->setOwnerHeadphonesMixAudioTap(m_ownerMix, m_bridge.get())) {
            m_bridge->stop();
        }
    } else if (m_sliceId == kMasterMix || m_sliceId == kSpeakersMix) {
        m_audioEngine->setMasterMixAudioTap(m_bridge.get(),
                                            /*speakersOnly=*/m_sliceId == kSpeakersMix);
    } else if (m_sliceId == kHeadphonesMix) {
        m_audioEngine->setHeadphonesMixAudioTap(m_bridge.get());
    } else if (!m_audioEngine->setSliceAudioTap(m_sliceId, m_bridge.get())) {
        // Every receiver tap slot is taken: stay stopped.
        m_bridge->stop();
    }
}

void DaemonAudioSource::stop()
{
    detachFromEngine();
    m_bridge->stop();
}

bool DaemonAudioSource::setCaptureClock(CaptureClock clock)
{
    return m_bridge->setCaptureClock(std::move(clock));
}

bool DaemonAudioSource::isRunning() const noexcept
{
    return m_bridge->isRunning();
}

std::optional<DaemonAudioBlock> DaemonAudioSource::takeBlock()
{
    return m_bridge->takeBlock();
}

std::uint64_t DaemonAudioSource::dropCount() const noexcept
{
    return m_bridge->telemetry().sourceDropEvents;
}

DaemonAudioSourceTelemetry DaemonAudioSource::telemetry() const noexcept
{
    return m_bridge->telemetry();
}

void DaemonAudioSource::dropIngressForTest(int frames) noexcept
{
    m_bridge->dropIngressForTest(frames);
}

#ifdef NEREUS_BUILD_TESTS
void DaemonAudioSource::setTakeBlockLockedHookForTest(std::function<void()> hook)
{
    m_bridge->m_takeBlockLockedHookForTest = std::move(hook);
}
#endif

} // namespace NereusSDR
