// =================================================================
// src/core/audio/RemoteVaxFeeder.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-44; see RemoteVaxFeeder.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-44 fix wave: slices sharing the channel are mixed.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/RemoteVaxFeeder.h"

#include "core/AudioEngine.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>

namespace NereusSDR {

namespace {

constexpr std::size_t kFrameBytes = 2 * sizeof(float);

qint64 steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

VaxOutputPort VaxOutputPort::forEngine(AudioEngine* engine, int channel)
{
    VaxOutputPort port;
    port.pacing = [engine, channel]() -> std::optional<IAudioBus::OutputPacing> {
        return engine ? engine->vaxOutputPacing(channel) : std::nullopt;
    };
    port.write = [engine, channel](const float* stereo, int frames) {
        return engine && engine->writeVaxOutput(channel, stereo, frames);
    };
    return port;
}

RemoteVaxFeeder::RemoteVaxFeeder(int channel, VaxOutputPort port, Clock clock)
    : m_channel(channel)
    , m_port(std::move(port))
    , m_clock(clock ? std::move(clock) : Clock(&steadyNowNs))
{
    for (Source& source : m_sources) {
        source.handoff = std::make_unique<AudioRingSpsc<kHandoffBytes>>();
    }
    m_chunk.resize(kOutputBlockFrames * 2);
    m_part.resize(kOutputBlockFrames * 2);
    m_matcherChunk.resize(kInputChunkFrames * 2);
}

RemoteVaxFeeder::~RemoteVaxFeeder()
{
    stopWorker();
}

void RemoteVaxFeeder::setSourceSlices(const QList<int>& sliceIds)
{
    QList<int> wanted;
    for (int id : sliceIds) {
        if (id >= 0 && !wanted.contains(id) && wanted.size() < kMaxSources) {
            wanted << id;
        }
    }
    // A slice that stays keeps its ring; the others' slots are freed, then
    // the new slices take free slots. The pump drops what a freed slot's
    // slice left (up to here; nothing of it follows once its stream is
    // released).
    for (Source& source : m_sources) {
        const int id = source.sliceId.load(std::memory_order_acquire);
        if (id >= 0 && !wanted.contains(id)) {
            source.dropUntilBytes.store(source.pushedBytes.load(std::memory_order_acquire),
                                        std::memory_order_release);
            source.sliceId.store(-1, std::memory_order_release);
            source.stopped.store(false, std::memory_order_release);
            source.stopReason.clear();
            source.generation.fetch_add(1, std::memory_order_acq_rel);
        }
    }
    for (int id : wanted) {
        bool present = false;
        for (const Source& source : m_sources) {
            present = present || source.sliceId.load(std::memory_order_acquire) == id;
        }
        if (present) { continue; }
        for (Source& source : m_sources) {
            if (source.sliceId.load(std::memory_order_acquire) >= 0) { continue; }
            source.dropUntilBytes.store(source.pushedBytes.load(std::memory_order_acquire),
                                        std::memory_order_release);
            source.stopped.store(false, std::memory_order_release);
            source.stopReason.clear();
            // Its previous slice was released before this: no block of it
            // follows, so nothing races this reset.
            source.receivedFrames.store(0, std::memory_order_release);
            source.sliceId.store(id, std::memory_order_release);
            source.generation.fetch_add(1, std::memory_order_acq_rel);
            break;
        }
    }
    // A slice that stays keeps its stop: it is still news until its audio
    // flows (fix wave follow-up; clearing every stop here, or letting
    // another slice's audio clear it, hid a refusal for the slice joining).
    m_generation.fetch_add(1, std::memory_order_acq_rel);
}

void RemoteVaxFeeder::setSourceSlice(int sliceId)
{
    setSourceSlices(sliceId < 0 ? QList<int>{} : QList<int>{sliceId});
}

QList<int> RemoteVaxFeeder::sourceSlices() const
{
    QList<int> ids;
    for (const Source& source : m_sources) {
        const int id = source.sliceId.load(std::memory_order_acquire);
        if (id >= 0) { ids << id; }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

int RemoteVaxFeeder::sourceSlice() const
{
    const QList<int> ids = sourceSlices();
    return ids.isEmpty() ? -1 : ids.constFirst();
}

RemoteVaxFeeder::Source* RemoteVaxFeeder::sourceFor(int sliceId)
{
    if (sliceId < 0) { return nullptr; }
    for (Source& source : m_sources) {
        if (source.sliceId.load(std::memory_order_acquire) == sliceId) {
            return &source;
        }
    }
    return nullptr;
}

void RemoteVaxFeeder::receiverAudioBlock(int sliceId, const float* interleavedStereo, int frames)
{
    // Receive worker, under the stream's lock: copy and return.
    Source* source = sourceFor(sliceId);
    if (interleavedStereo == nullptr || frames <= 0 || source == nullptr) {
        return;
    }
    const std::size_t bytes = std::size_t(frames) * kFrameBytes;
    AudioRingSpsc<kHandoffBytes>& handoff = *source->handoff;
    // All or nothing, so the ring stays whole stereo frames. This thread is
    // the only writer, so the free space only grows until the push.
    if (handoff.usedBytes() + bytes > handoff.capacity() - 1) {
        m_droppedFrames.fetch_add(quint64(frames), std::memory_order_relaxed);
        return;
    }
    handoff.tryPushCopy(reinterpret_cast<const uint8_t*>(interleavedStereo), qint64(bytes));
    source->pushedBytes.fetch_add(quint64(bytes), std::memory_order_release);
    source->stopped.store(false, std::memory_order_release);
    source->receivedFrames.fetch_add(quint64(frames), std::memory_order_release);
    m_receivedFrames.fetch_add(quint64(frames), std::memory_order_relaxed);
}

void RemoteVaxFeeder::receiverAudioStopped(int sliceId, const QString& reason)
{
    Source* source = sourceFor(sliceId);
    if (source == nullptr) {
        return;
    }
    source->stopped.store(true, std::memory_order_release);
    source->stopReason = reason;
    source->stopAtFrames = source->receivedFrames.load(std::memory_order_acquire);
    source->stopOrder = ++m_stopCount;
}

QList<RemoteVaxFeeder::Stop> RemoteVaxFeeder::stops() const
{
    // Per slice: another slice's audio on the channel says nothing about
    // this one's stop.
    QList<std::pair<quint64, Stop>> ordered;
    for (const Source& source : m_sources) {
        const int id = source.sliceId.load(std::memory_order_acquire);
        if (id < 0 || source.stopReason.isEmpty()
            || source.receivedFrames.load(std::memory_order_acquire) > source.stopAtFrames) {
            continue;
        }
        ordered.append({source.stopOrder, Stop{id, source.stopReason}});
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    QList<Stop> result;
    for (const auto& entry : ordered) {
        result.append(entry.second);
    }
    return result;
}

QString RemoteVaxFeeder::lastStopReason() const
{
    const QList<Stop> current = stops();
    return current.isEmpty() ? QString() : current.constLast().reason;
}

void RemoteVaxFeeder::startWorker()
{
    if (m_worker.joinable()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_workerMutex);
        m_workerStop = false;
    }
    m_worker = std::thread([this] {
        std::unique_lock<std::mutex> lock(m_workerMutex);
        while (!m_workerStop) {
            lock.unlock();
            pump();
            lock.lock();
            m_workerWake.wait_for(lock, std::chrono::nanoseconds(kPumpIntervalNs),
                                  [this] { return m_workerStop; });
        }
    });
}

void RemoteVaxFeeder::stopWorker()
{
    {
        std::lock_guard<std::mutex> lock(m_workerMutex);
        m_workerStop = true;
    }
    m_workerWake.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

bool RemoteVaxFeeder::workerRunning() const
{
    return m_worker.joinable();
}

void RemoteVaxFeeder::dropHandoff(Source& source)
{
    const std::size_t used = source.handoff->usedBytes();
    source.handoff->dropOldest(used);
    source.poppedBytes += used;
}

void RemoteVaxFeeder::dropHandoff()
{
    for (Source& source : m_sources) {
        dropHandoff(source);
    }
}

void RemoteVaxFeeder::popHandoff(Source& source, void* into, std::size_t bytes)
{
    source.handoff->popInto(static_cast<uint8_t*>(into), qint64(bytes));
    source.poppedBytes += bytes;
}

void RemoteVaxFeeder::trimHandoff(Source& source, std::size_t frames)
{
    const std::size_t bytes = std::min(frames * kFrameBytes, source.handoff->usedBytes());
    source.handoff->dropOldest(bytes);
    source.poppedBytes += bytes;
    m_trimmedFrames += bytes / kFrameBytes;
}

bool RemoteVaxFeeder::isLive(const Source& source, qint64 now) const
{
    return source.sliceId.load(std::memory_order_acquire) >= 0 && source.heard
        && !source.stopped.load(std::memory_order_acquire)
        && now - source.lastArrivalNs <= kQuietNs;
}

qint64 RemoteVaxFeeder::pausedNs() const
{
    // Twice the largest read step the output has shown, at 48 kHz.
    const qint64 step = qint64(m_largestStep) * 1'000'000'000 / 48'000;
    return std::max(kPausedNs, 2 * step);
}

void RemoteVaxFeeder::restartMatcher()
{
    if (m_matcherReady) {
        m_matcher.reset();
    }
}

int RemoteVaxFeeder::drainHandoff(bool paced, qint64 now)
{
    if (paced) {
        if (!m_matcherTried) {
            m_matcherTried = true;
            m_matcherReady = m_matcher.configure(kInputChunkFrames, kOutputBlockFrames,
                                                 kMatcherRingFrames);
        }
        if (!m_matcherReady) {
            // No rate matcher in this build: fall back to writing as the
            // audio arrives.
            return drainHandoff(false, now);
        }
    }
    // A slice that is not live counts as silence: what it left is dropped,
    // so it cannot play late, out of step with the others.
    int live = 0;
    bool wentQuiet = false;
    for (Source& source : m_sources) {
        const bool isNowLive = isLive(source, now);
        if (isNowLive) {
            ++live;
        } else {
            dropHandoff(source);
            // Went quiet: still on the channel, not stopped by the Core,
            // and silent past kQuietNs. A slice that left the channel (its
            // slot freed) or that the Core stopped did not hold anyone up.
            wentQuiet = wentQuiet
                || (source.wasLive && source.sliceId.load(std::memory_order_acquire) >= 0
                    && !source.stopped.load(std::memory_order_acquire)
                    && now - source.lastArrivalNs > kQuietNs);
        }
        source.wasLive = isNowLive;
    }
    if (live == 0) {
        return 0;
    }
    if (wentQuiet) {
        // The others waited kQuietNs for the slice that went quiet: that
        // much has piled up behind it. Keep a quarter of the matcher's ring
        // (45 ms) and drop the rest, rather than overrun the matcher with
        // it; the matcher ran dry during the wait anyway.
        constexpr std::size_t kKeepFrames = std::size_t(kMatcherRingFrames / 4);
        for (Source& source : m_sources) {
            if (!source.wasLive) { continue; }
            const std::size_t used = source.handoff->usedBytes() / kFrameBytes;
            if (used > kKeepFrames) {
                trimHandoff(source, used - kKeepFrames);
            }
        }
    }
    if (live > 1) {
        // A slice that stalled and then burst would otherwise keep its
        // backlog for good: every chunk takes equal frames from each slice,
        // so it would play that much late from then on. Keep it at most
        // kMaxLagFrames ahead of the least-queued slice, dropping the
        // oldest, as the local VAX mix's per-slice ring does.
        std::size_t least = std::numeric_limits<std::size_t>::max();
        for (const Source& source : m_sources) {
            if (source.wasLive) {
                least = std::min(least, source.handoff->usedBytes() / kFrameBytes);
            }
        }
        for (Source& source : m_sources) {
            if (!source.wasLive) { continue; }
            const std::size_t used = source.handoff->usedBytes() / kFrameBytes;
            if (used > least + std::size_t(kMaxLagFrames)) {
                trimHandoff(source, used - least - std::size_t(kMaxLagFrames));
            }
        }
    }
    int moved = 0;
    for (;;) {
        // The chunk every live slice can give: the matcher takes fixed
        // 192-frame chunks; unpaced, up to one output block of whatever
        // all of them have.
        int ready = std::numeric_limits<int>::max();
        for (const Source& source : m_sources) {
            if (isLive(source, now)) {
                ready = std::min(ready, int(source.handoff->usedBytes() / kFrameBytes));
            }
        }
        const int frames = paced ? (ready >= kInputChunkFrames ? kInputChunkFrames : 0)
                                 : std::min(ready, kOutputBlockFrames);
        if (frames <= 0) {
            break;
        }
        const std::size_t bytes = std::size_t(frames) * kFrameBytes;
        const std::size_t floats = std::size_t(frames) * 2;
        std::fill(m_chunk.begin(), m_chunk.begin() + qsizetype(floats), 0.0f);
        for (Source& source : m_sources) {
            if (!isLive(source, now)) { continue; }
            popHandoff(source, m_part.data(), bytes);
            for (std::size_t i = 0; i < floats; ++i) {
                m_chunk[qsizetype(i)] += m_part[qsizetype(i)];
            }
        }
        if (paced) {
            // The matcher takes exactly one input chunk.
            std::copy(m_chunk.constBegin(), m_chunk.constBegin() + qsizetype(floats),
                      m_matcherChunk.begin());
            if (!m_matcher.push(m_matcherChunk)) {
                ++m_restarts;
                restartMatcher();
                break;
            }
        } else if (m_port.write && m_port.write(m_chunk.constData(), frames)) {
            m_writtenFrames += quint64(frames);
        }
        moved += frames;
    }
    return moved;
}

void RemoteVaxFeeder::pump()
{
    const qint64 now = m_clock();
    const quint32 generation = m_generation.load(std::memory_order_acquire);
    if (generation != m_seenGeneration) {
        m_seenGeneration = generation;
        // A slice that stayed in the set keeps the output running; with
        // none, it starts afresh.
        bool continuing = false;
        for (Source& source : m_sources) {
            const quint32 slotGeneration = source.generation.load(std::memory_order_acquire);
            if (slotGeneration == source.seenGeneration) {
                continuing = continuing || source.sliceId.load(std::memory_order_acquire) >= 0;
                continue;
            }
            source.seenGeneration = slotGeneration;
            // Only what the slot's previous slice left.
            const quint64 until = source.dropUntilBytes.load(std::memory_order_acquire);
            if (until > source.poppedBytes) {
                const std::size_t stale = std::size_t(until - source.poppedBytes);
                source.handoff->dropOldest(stale);
                source.poppedBytes += stale;
            }
            source.heard = false;
            source.wasLive = false;
        }
        if (!continuing) {
            restartMatcher();
            m_state = State::WaitingForAudio;
            m_haveConsumed = false;
            m_lastInputNs = now;
        }
    }
    bool assigned = false;
    for (Source& source : m_sources) {
        if (source.sliceId.load(std::memory_order_acquire) < 0) { continue; }
        assigned = true;
        // A slice is heard when its stream delivered since the last pump.
        const quint64 pushed = source.pushedBytes.load(std::memory_order_acquire);
        if (pushed != source.seenPushedBytes) {
            source.seenPushedBytes = pushed;
            source.lastArrivalNs = now;
            source.heard = true;
        }
    }
    if (!assigned) {
        dropHandoff();
        m_state = State::Idle;
        publish(m_state, 0);
        return;
    }

    const std::optional<IAudioBus::OutputPacing> pacing =
        m_port.pacing ? m_port.pacing() : std::nullopt;
    if (!pacing) {
        // No playback timing to pace by (or the output is closed): write
        // what arrived as it arrives.
        const int moved = drainHandoff(false, now);
        if (moved > 0) {
            m_lastInputNs = now;
            m_state = State::Playing;
        } else if (m_state == State::Playing && now - m_lastInputNs > kQuietNs) {
            m_state = State::WaitingForAudio;
        }
        publish(m_state, 0);
        return;
    }

    // The output's clock: frames an app has taken from it.
    if (!m_haveConsumed || pacing->consumedFrames != m_lastConsumed) {
        if (m_haveConsumed) {
            // The largest step seen between two pumps: an app reading big
            // blocks needs that much queued, whatever the output reports.
            const quint64 step = pacing->consumedFrames - m_lastConsumed;
            const quint64 cap = quint64(std::max(0, pacing->capacityFrames / 4));
            m_largestStep = std::max(m_largestStep, int(std::min(step, cap)));
        }
        m_haveConsumed = true;
        m_lastConsumed = pacing->consumedFrames;
        m_lastProgressNs = now;
        if (m_state == State::NoReader) {
            // Something reads it again.
            restartMatcher();
            m_state = State::WaitingForAudio;
            m_lastInputNs = now;
        }
    } else if (m_state != State::NoReader && pacing->queuedFrames > 0
               && now - m_lastProgressNs > kNoReaderNs) {
        // Nothing has taken audio for half a second with audio waiting:
        // no app is reading this VAX output. Not a fault; stop writing.
        restartMatcher();
        m_state = State::NoReader;
    }
    if (m_state == State::NoReader) {
        dropHandoff();
        publish(m_state, pacing->queuedFrames);
        return;
    }
    const bool outputPaused = pacing->queuedFrames > 0 && now - m_lastProgressNs > pausedNs();
    if (outputPaused) {
        // The output stopped taking audio (an app closing it, or pausing).
        // Feeding the rate matcher now would only run it over; drop what
        // arrives and start afresh when the output moves again.
        dropHandoff();
        if (m_state == State::Playing) {
            restartMatcher();
            m_state = State::WaitingForAudio;
        }
        publish(m_state, pacing->queuedFrames);
        return;
    }

    const int moved = drainHandoff(true, now);
    if (!m_matcherReady) {
        // drainHandoff() fell back to direct writes.
        if (moved > 0) {
            m_lastInputNs = now;
            m_state = State::Playing;
        }
        publish(m_state, pacing->queuedFrames);
        return;
    }
    if (moved > 0) {
        m_lastInputNs = now;
        m_state = State::Playing;
    } else if (m_state == State::Playing && now - m_lastInputNs > kQuietNs) {
        // The Core went quiet: let the output run out rather than play the
        // rate matcher dry, and start afresh when audio returns.
        restartMatcher();
        m_state = State::WaitingForAudio;
    }
    if (m_state != State::Playing) {
        publish(m_state, pacing->queuedFrames);
        return;
    }

    // Keep one output block beyond the largest step the output takes at
    // once, and never less than 20 ms, as the remote speaker does.
    const int step = std::max(pacing->callbackFrames, m_largestStep);
    int target = std::max(2 * kOutputBlockFrames, step + kOutputBlockFrames);
    target = std::min(target, pacing->capacityFrames - kOutputBlockFrames);
    int queued = pacing->queuedFrames;
    const int maxBlocks = std::max(0, pacing->capacityFrames / kOutputBlockFrames);
    for (int i = 0; i < maxBlocks && queued < target; ++i) {
        const QVector<float> pcm = m_matcher.take();
        if (pcm.size() != kOutputBlockFrames * 2 || !m_port.write
            || !m_port.write(pcm.constData(), kOutputBlockFrames)) {
            break;
        }
        queued += kOutputBlockFrames;
        m_writtenFrames += kOutputBlockFrames;
    }
    const RemoteAudioRateMatcherStats matcherStats = m_matcher.stats();
    if (matcherStats.underflows > 0 || matcherStats.overflows > 0) {
        ++m_restarts;
        restartMatcher();
        m_state = State::WaitingForAudio;
    }
    publish(m_state, queued);
}

void RemoteVaxFeeder::publish(State state, int queuedFrames)
{
    RemoteVaxFeederStats stats;
    stats.state = state;
    stats.sourceSliceIds = sourceSlices();
    stats.sourceSliceId = stats.sourceSliceIds.isEmpty() ? -1 : stats.sourceSliceIds.constFirst();
    stats.receivedFrames = m_receivedFrames.load(std::memory_order_relaxed);
    stats.droppedFrames = m_droppedFrames.load(std::memory_order_relaxed);
    stats.trimmedFrames = m_trimmedFrames;
    stats.writtenFrames = m_writtenFrames;
    stats.restarts = m_restarts;
    stats.queuedFrames = std::max(0, queuedFrames);
    std::size_t handoffBytes = 0;
    for (const Source& source : m_sources) {
        handoffBytes = std::max(handoffBytes, source.handoff->usedBytes());
    }
    stats.handoffFrames = int(handoffBytes / kFrameBytes);
    stats.paced = m_haveConsumed || state == State::NoReader;
    if (m_matcherReady && state == State::Playing) {
        const RemoteAudioRateMatcherStats matcherStats = m_matcher.stats();
        stats.matcherFillFrames = std::max(0, matcherStats.ringFillFrames);
        if (matcherStats.controlActive) {
            stats.ratio = matcherStats.currentRatio;
        }
    }
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = stats;
}

RemoteVaxFeederStats RemoteVaxFeeder::stats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

} // namespace NereusSDR
