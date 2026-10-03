// =================================================================
// src/core/session/media/RemoteMicReceiver.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See RemoteMicReceiver.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 36 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C2: setFeedWriter, one line
//               writes the transmitter's feed at a time. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: R-IOS-13, R-R3-42: the small adaptive transmit buffer,
//               shedding a standing excess only in silence, and the
//               over's latency figures. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: Load findings 2 (R-IOS-13): the key's fill wait runs from
//               the line's first packet, with its own bound for that
//               packet. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: LINK minor 12 (TX audio): a stall's held audio past
//               kStaleAfterStallMs is trimmed to the target when the
//               buffer starts again, not sent late. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX stall lane: arrivals are timed at their receipt in the
//               transport (an arrival record per write, carrying how long
//               the packet waited at the Core), so a stalled drain of the
//               Core's event loop is not counted as network jitter. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX stall lane, fix round 1: the buffer's timing is the
//               pump's drain again (a stalled drain grows the margin, as it
//               did before); each packet's wait at the Core is measured
//               into the over's figures only. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): submit runs on the thread
//               that delivers the line (the transport's microphone thread
//               on a real connection), under the line's lock; its event
//               loop part is posted there. The feed's writer side takes a
//               lock the pump never takes. The buffer is timed at each
//               packet's receipt again. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: submit measures each packet's receipt
//               gap and its receipt less its RTP timestamp; the pump places
//               the over's first underruns in time (when, how long silent,
//               and the arrivals around them). Measurement only; nothing
//               the buffer does changes. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/media/RemoteMicReceiver.h"

#include "core/LogCategories.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "core/session/media/RemoteAudioRateMatcher.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <opus.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace NereusSDR {

namespace {

using Cfg = RemoteMicConfig;
constexpr int kFloatBytes = static_cast<int>(sizeof(float));
constexpr int kPumpBlock = Cfg::kPumpBlockFrames;
// libopus decodes at most 120 ms in one packet.
constexpr int kMaxOpusFrames = Cfg::kSampleRate / 1000 * 120;
// A gap is concealed only up to 60 ms; a longer one inserts nothing (the
// buffer has run dry meanwhile anyway).
constexpr int kMaxConcealFrames = Cfg::kMaxConcealFrames;
// The buffer's clock, in pump blocks (1.33 ms each).
constexpr int blocksFor(int ms)
{
    return (ms * Cfg::kFramesPerMs + kPumpBlock - 1) / kPumpBlock;
}
constexpr int kWindowBlocks = blocksFor(Cfg::kWindowMs);
constexpr int kShrinkIntervalBlocks = blocksFor(Cfg::kShrinkIntervalMs);
constexpr int kJitterMemoryWindows = Cfg::kJitterMemoryMs / Cfg::kWindowMs;
constexpr int kReserveFrames = Cfg::kReserveMs * Cfg::kFramesPerMs;
constexpr int kSilenceRunFrames = Cfg::kSilenceRunMs * Cfg::kFramesPerMs;

qint64 steadyMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

qint64 steadyUs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

// TX diagnostics lane: pump blocks as ms, and us as ms.
double blocksToMs(quint64 blocks)
{
    return static_cast<double>(blocks) * kPumpBlock / Cfg::kFramesPerMs;
}
double usToMs(qint64 us)
{
    return us < 0 ? -1.0 : static_cast<double>(us) / 1000.0;
}
// An RTP timestamp step past this is a new stream, not time passing.
constexpr qint64 kRtpRebaseFrames = static_cast<qint64>(Cfg::kSampleRate) * 10;

bool configureMatcher(RemoteAudioRateMatcher& matcher, int ringFrames)
{
    // R-IOS-13: the buffer feeds rmatch a block at a time, so its ring only
    // bridges one block to the next; the ratio is forced (the buffer's own
    // control sets it), so rmatch's half-full target never sets the delay.
    if (!matcher.configure(kPumpBlock, kPumpBlock, ringFrames, Cfg::kSampleRate)) {
        return false;
    }
    matcher.forceRatio(true, 1.0);
    return true;
}

} // namespace

bool opusPacketCarriesFec(const QByteArray& payload)
{
    return !payload.isEmpty()
        && opus_packet_has_lbrr(reinterpret_cast<const unsigned char*>(payload.constData()),
                                static_cast<opus_int32>(payload.size()))
            == 1;
}

// ============================================================================
// RemoteMicFeed
// ============================================================================

RemoteMicFeed::RemoteMicFeed()
    : m_matcher(std::make_unique<RemoteAudioRateMatcher>())
    , m_buffer(static_cast<size_t>(kBufferFrames))
    , m_delayMinMemory(static_cast<size_t>(kJitterMemoryWindows),
                       std::numeric_limits<qint64>::max())
    , m_delayMaxMemory(static_cast<size_t>(kJitterMemoryWindows),
                       std::numeric_limits<qint64>::min())
    , m_monoScratch(static_cast<size_t>(kPumpBlock))
    , m_stereoScratch(static_cast<size_t>(kPumpBlock) * 2)
{
    if (!configureMatcher(*m_matcher, kMatcherRingFrames)) {
        qCWarning(lcAudio) << "Remote microphone: the rate matcher could not be built;"
                           << "remote microphone audio will be silent";
    }
    resetWindow();
}

RemoteMicFeed::~RemoteMicFeed() = default;

void RemoteMicFeed::setInUse(bool inUse)
{
    // TX mic thread: a write on the line's thread never sees half a change.
    const std::lock_guard<std::mutex> lock(m_writerLock);
    if (inUse == m_inUse.load(std::memory_order_relaxed)) {
        return;
    }
    m_inUse.store(inUse, std::memory_order_release);
    m_framesSinceInUse.store(0, std::memory_order_release);
    // Everything written so far is dropped by the pump when it sees this
    // change; the buffer starts again empty, from silence.
    m_clearAtBytes.store(m_writtenBytes, std::memory_order_release);
    m_inUseForPump.store(inUse, std::memory_order_release);
    m_change.fetch_add(1, std::memory_order_acq_rel);
}

bool RemoteMicFeed::write(const float* mono, int frames, int heldFrames, ArrivalTiming timing)
{
    const std::lock_guard<std::mutex> lock(m_writerLock);
    if (!m_inUse.load(std::memory_order_relaxed) || mono == nullptr || frames <= 0) {
        return false;
    }
    const qint64 bytes = static_cast<qint64>(frames) * kFloatBytes;
    // TX stall lane: the arrival's record goes first, so the pump finds it
    // by the time it reads the audio; both fit or neither is written (this
    // side only adds, so the room it sees is the least there is).
    constexpr qint64 kArrivalBytes = static_cast<qint64>(sizeof(Arrival));
    const qint64 inputRoom =
        static_cast<qint64>(m_input.capacity() - 1 - m_input.usedBytes());
    const qint64 arrivalRoom =
        static_cast<qint64>(m_arrivals.capacity() - 1 - m_arrivals.usedBytes());
    if (bytes > inputRoom || kArrivalBytes > arrivalRoom) {
        m_droppedFrames.fetch_add(static_cast<quint64>(frames), std::memory_order_relaxed);
        return false;
    }
    const Arrival arrival{m_writtenBytes + static_cast<quint64>(bytes), frames,
                          std::max(0, heldFrames), timing.receiptGapUs, timing.rtpOffsetUs};
    m_arrivals.tryPushCopy(reinterpret_cast<const uint8_t*>(&arrival), kArrivalBytes);
    m_input.tryPushCopy(reinterpret_cast<const uint8_t*>(mono), bytes);
    m_writtenBytes += static_cast<quint64>(bytes);
    m_framesSinceInUse.fetch_add(frames, std::memory_order_acq_rel);
    // One packet a write (a concealed or recovered packet is its own write):
    // the buffer's target is one packet plus its margin.
    m_packetFrames.store(std::min(frames, Cfg::kMaxDepthFrames / 2), std::memory_order_relaxed);
    return true;
}

void RemoteMicFeed::discardInputTo(quint64 bytes)
{
    while (m_readBytes < bytes) {
        const qint64 want = static_cast<qint64>(
            std::min<quint64>(bytes - m_readBytes,
                              static_cast<quint64>(m_monoScratch.size()) * kFloatBytes));
        const qint64 got =
            m_input.popInto(reinterpret_cast<uint8_t*>(m_monoScratch.data()), want);
        if (got <= 0) {
            break;
        }
        m_readBytes += static_cast<quint64>(got);
    }
}

void RemoteMicFeed::discardAllInput()
{
    for (;;) {
        const qint64 got = m_input.popInto(reinterpret_cast<uint8_t*>(m_monoScratch.data()),
                                           static_cast<qint64>(m_monoScratch.size()) * kFloatBytes);
        if (got <= 0) {
            break;
        }
        m_readBytes += static_cast<quint64>(got);
    }
}

int RemoteMicFeed::drainInput()
{
    // Whole frames only, into the jitter buffer's free space (the input
    // ring keeps the rest until there is room).
    int drained = 0;
    while (m_bufferCount < kBufferFrames) {
        const int tail = (m_bufferHead + m_bufferCount) % kBufferFrames;
        const int room = std::min(kBufferFrames - m_bufferCount, kBufferFrames - tail);
        const qint64 used = static_cast<qint64>(m_input.usedBytes()) / kFloatBytes;
        const int take = static_cast<int>(std::min<qint64>(room, used));
        if (take <= 0) {
            break;
        }
        const qint64 got = m_input.popInto(
            reinterpret_cast<uint8_t*>(m_buffer.data() + tail),
            static_cast<qint64>(take) * kFloatBytes);
        if (got <= 0) {
            break;
        }
        m_readBytes += static_cast<quint64>(got);
        m_bufferCount += static_cast<int>(got / kFloatBytes);
        drained += static_cast<int>(got / kFloatBytes);
    }
    return drained;
}

void RemoteMicFeed::takeArrivals(bool note)
{
    // Every write whose audio the pump has read (or discarded) whole. What
    // one drain found counts as one arrival, timed at its first write's
    // receipt (a lost packet rebuilt from the next one's FEC arrives with
    // it and counts one packet late; a burst is timed by its oldest
    // packet). TX mic thread: with the line off the event loop, the
    // receipt is the link's timing, which fix round 1 could not yet use.
    // Each write's wait at the Core also goes into the over's figures.
    Arrival arrival{};
    int frames = 0;
    int heldFrames = 0;
    while (m_arrivals.peekInto(reinterpret_cast<uint8_t*>(&arrival), sizeof(Arrival))
           && arrival.endBytes <= m_readBytes) {
        m_arrivals.dropOldest(sizeof(Arrival));
        if (frames == 0) {
            heldFrames = arrival.heldFrames;
        }
        frames += arrival.frames;
        if (note) {
            noteArrivalTiming(arrival);
            ++m_overOwnerWaits;
            m_overOwnerWaitSumFrames += arrival.heldFrames;
            m_overOwnerWaitMaxFrames = std::max(m_overOwnerWaitMaxFrames, arrival.heldFrames);
            if (arrival.heldFrames > Cfg::kLongOwnerWaitMs * Cfg::kFramesPerMs) {
                ++m_overOwnerWaitsLong;
            }
        }
    }
    if (note && frames > 0) {
        noteArrival(frames, heldFrames);
    }
}

bool RemoteMicFeed::memoryDelayMin(qint64* min) const
{
    qint64 lowest = m_windowArrived ? m_windowDelayMin : std::numeric_limits<qint64>::max();
    for (const qint64 d : m_delayMinMemory) {
        lowest = std::min(lowest, d);
    }
    if (lowest == std::numeric_limits<qint64>::max()) {
        return false;
    }
    *min = lowest;
    return true;
}

void RemoteMicFeed::noteArrival(int frames, int heldFrames)
{
    // This arrival's delay: the pump's clock now less where its first
    // frame sits in the stream. On time it is the same every arrival; a
    // late one (a jittered packet, or one rebuilt from the next packet's
    // FEC and written with it) shows as more. One past the ceiling is a
    // stall, ridden through, not jitter the margin should cover.
    // TX stall lane: the time the packet waited at the Core after its
    // receipt in the transport is taken off, so the delay is the link's.
    const qint64 delay = static_cast<qint64>(m_block) * kPumpBlock
        - static_cast<qint64>(heldFrames) - static_cast<qint64>(m_arrivedFrames);
    m_arrivedFrames += static_cast<quint64>(frames);
    qint64 earliest = 0;
    if (memoryDelayMin(&earliest) && delay - earliest > marginCeiling()) {
        return;
    }
    if (!m_windowArrived) {
        m_windowArrived = true;
        m_windowDelayMin = delay;
        m_windowDelayMax = delay;
        return;
    }
    m_windowDelayMin = std::min(m_windowDelayMin, delay);
    m_windowDelayMax = std::max(m_windowDelayMax, delay);
}

void RemoteMicFeed::noteArrivalTiming(const Arrival& arrival)
{
    // TX diagnostics lane, measurement only. The over's first arrival's gap
    // runs back to the last over, so it is not counted.
    qint64 gapUs = m_overArrivals > 0 ? arrival.receiptGapUs : -1;
    ++m_overArrivals;
    qint64 lateUs = -1;
    if (arrival.rtpOffsetUs != ArrivalTiming::kNoOffset) {
        m_overRtpOffsetMinUs = std::min(m_overRtpOffsetMinUs, arrival.rtpOffsetUs);
        lateUs = arrival.rtpOffsetUs - m_overRtpOffsetMinUs;
    }
    m_windowGapMaxUs = std::max(m_windowGapMaxUs, gapUs);
    m_windowLateMaxUs = std::max(m_windowLateMaxUs, lateUs);
    if (m_openUnderrun >= 0) {
        Stats::Underrun& event = m_underruns[static_cast<size_t>(m_openUnderrun)];
        event.arrivalGapMs = std::max(event.arrivalGapMs, usToMs(gapUs));
        event.lateMs = std::max(event.lateMs, usToMs(lateUs));
        publishUnderrun(m_openUnderrun);
    }
}

void RemoteMicFeed::openUnderrun()
{
    // TX diagnostics lane: the first few of an over, placed. The arrivals
    // of this window and the one before count toward it, and those until
    // playing resumes.
    if (m_underrunsPlaced >= Stats::kMaxUnderrunsPlaced) {
        m_openUnderrun = -1;
        return;
    }
    const int index = m_underrunsPlaced++;
    Stats::Underrun& event = m_underruns[static_cast<size_t>(index)];
    event.atLineMs = blocksToMs(m_block - m_changeBlock);
    event.atSteadyUs = steadyUs();
    event.silentMs = -1.0;
    event.arrivalGapMs = usToMs(std::max(m_windowGapMaxUs, m_prevWindowGapMaxUs));
    event.lateMs = usToMs(std::max(m_windowLateMaxUs, m_prevWindowLateMaxUs));
    m_openUnderrun = index;
    m_openUnderrunBlock = m_block;
    publishUnderrun(index);
    m_statsUnderrunsPlaced.store(m_underrunsPlaced, std::memory_order_release);
}

void RemoteMicFeed::closeUnderrun()
{
    if (m_openUnderrun < 0) {
        return;
    }
    m_underruns[static_cast<size_t>(m_openUnderrun)].silentMs =
        blocksToMs(m_block - m_openUnderrunBlock);
    publishUnderrun(m_openUnderrun);
    m_openUnderrun = -1;
}

void RemoteMicFeed::publishUnderrun(int index)
{
    const auto i = static_cast<size_t>(index);
    const Stats::Underrun& event = m_underruns[i];
    m_statsUnderrunAtMs[i].store(event.atLineMs, std::memory_order_relaxed);
    m_statsUnderrunSteadyUs[i].store(event.atSteadyUs, std::memory_order_relaxed);
    m_statsUnderrunSilentMs[i].store(event.silentMs, std::memory_order_relaxed);
    m_statsUnderrunGapMs[i].store(event.arrivalGapMs, std::memory_order_relaxed);
    m_statsUnderrunLateMs[i].store(event.lateMs, std::memory_order_relaxed);
}

bool RemoteMicFeed::headIsSilent() const
{
    for (int i = 0; i < kPumpBlock; ++i) {
        const float x = m_buffer[static_cast<size_t>((m_bufferHead + i) % kBufferFrames)];
        if (std::abs(x) >= Cfg::kSilencePeak) {
            return false;
        }
    }
    return true;
}

bool RemoteMicFeed::canSplice() const
{
    // Deep in a pause: 20 ms of silence already went to rmatch (so what it
    // still holds, and the block before, are silent) and the next block is
    // silent too. A splice here steps by at most twice the silence peak.
    return m_silentRunFrames >= kSilenceRunFrames && m_bufferCount >= kPumpBlock
        && headIsSilent();
}

void RemoteMicFeed::popBlock(float* mono)
{
    bool silent = true;
    for (int i = 0; i < kPumpBlock; ++i) {
        mono[i] = m_buffer[static_cast<size_t>(m_bufferHead)];
        silent = silent && std::abs(mono[i]) < Cfg::kSilencePeak;
        m_bufferHead = (m_bufferHead + 1) % kBufferFrames;
    }
    m_bufferCount -= kPumpBlock;
    m_silentRunFrames = silent ? m_silentRunFrames + kPumpBlock : 0;
}

void RemoteMicFeed::dropBlock()
{
    // Only ever a silent block (canSplice), so the run goes on.
    m_bufferHead = (m_bufferHead + kPumpBlock) % kBufferFrames;
    m_bufferCount -= kPumpBlock;
    m_silentRunFrames += kPumpBlock;
}

void RemoteMicFeed::trimStaleToTarget()
{
    // The oldest whole blocks go until the buffer holds its target; what a
    // full buffer left in the input ring is drained and trimmed the same
    // way. Nothing has played since the underrun (rmatch faded out), so
    // this splices nothing: playback starts again from silence with the
    // newest audio.
    for (;;) {
        while (m_bufferCount >= kPumpBlock
               && m_bufferCount + m_matcherFill - kPumpBlock >= currentTarget()) {
            m_bufferHead = (m_bufferHead + kPumpBlock) % kBufferFrames;
            m_bufferCount -= kPumpBlock;
        }
        const int arrived = drainInput();
        if (arrived <= 0) {
            break;
        }
        takeArrivals(true);
    }
    m_silentRunFrames = 0;
}

int RemoteMicFeed::currentPacketFrames() const
{
    return std::max(kPumpBlock, m_packetFrames.load(std::memory_order_relaxed));
}

int RemoteMicFeed::currentTarget() const
{
    return m_marginFrames + currentPacketFrames();
}

int RemoteMicFeed::marginCeiling() const
{
    return std::max(Cfg::kMinMarginFrames, Cfg::kMaxDepthFrames - currentPacketFrames());
}

void RemoteMicFeed::resetWindow()
{
    // TX diagnostics lane: this window's arrival figures become the last
    // window's.
    m_prevWindowGapMaxUs = m_windowGapMaxUs;
    m_prevWindowLateMaxUs = m_windowLateMaxUs;
    m_windowGapMaxUs = -1;
    m_windowLateMaxUs = -1;
    m_windowStart = m_block;
    m_windowFloor = std::numeric_limits<int>::max();
    m_windowPlayed = false;
    m_windowUnderrun = false;
    m_windowArrived = false;
    m_windowRingFloorMs = -1.0;
}

void RemoteMicFeed::endWindow()
{
    // The jitter: the spread of the arrival delays over the last
    // kJitterMemoryMs, this window included.
    qint64 earliest = 0;
    const bool haveEarliest = memoryDelayMin(&earliest);
    m_delayMinMemory[static_cast<size_t>(m_memoryIndex)] =
        m_windowArrived ? m_windowDelayMin : std::numeric_limits<qint64>::max();
    m_delayMaxMemory[static_cast<size_t>(m_memoryIndex)] =
        m_windowArrived ? m_windowDelayMax : std::numeric_limits<qint64>::min();
    m_memoryIndex = (m_memoryIndex + 1) % kJitterMemoryWindows;
    qint64 latest = std::numeric_limits<qint64>::min();
    for (const qint64 d : m_delayMaxMemory) {
        latest = std::max(latest, d);
    }
    const qint64 jitter = haveEarliest && latest != std::numeric_limits<qint64>::min()
        ? std::max<qint64>(0, latest - earliest)
        : 0;
    const int wanted = static_cast<int>(std::clamp<qint64>(
        Cfg::kMinMarginFrames + jitter, Cfg::kMinMarginFrames, marginCeiling()));
    if (wanted > m_marginFrames) {
        // Jitter the margin did not cover: grow at once. The deficit is
        // filled in silence (or at once, after an underrun).
        m_marginFrames = wanted;
        m_lastShrink = m_block;
        ++m_overGrows;
    } else if (wanted < m_marginFrames
               && m_block - m_lastShrink >= static_cast<quint64>(kShrinkIntervalBlocks)) {
        // A steadier link: ease back a step.
        m_marginFrames = std::max(wanted,
                                  m_marginFrames - Cfg::kShrinkStepMs * Cfg::kFramesPerMs);
        m_lastShrink = m_block;
    }
    if (!m_windowPlayed || m_windowUnderrun || !m_windowArrived || !haveEarliest) {
        m_shedBudget = 0;
        m_insertBudget = 0;
        m_ringShedBudget = 0;
        resetWindow();
        return;
    }
    // The buffer's level with this window's jitter added back: the lowest
    // fill came just before its latest arrival.
    const int level = m_windowFloor
        + static_cast<int>(std::max<qint64>(0, m_windowDelayMax - earliest));
    // What stood past the margin (or short of it) for the whole window.
    m_shedBudget = std::max(0, level - m_marginFrames - kReserveFrames);
    m_insertBudget = std::max(0, m_marginFrames - kReserveFrames - level);
    m_ringShedBudget = m_windowRingFloorMs < 0.0
        ? 0
        : std::max(0, static_cast<int>((m_windowRingFloorMs - Cfg::kRingSlackMs)
                                       * Cfg::kFramesPerMs));
    // Clock matching: a small ratio moves the buffer toward its margin.
    const double deviationMs = static_cast<double>(level - m_marginFrames) / Cfg::kFramesPerMs;
    const double ppm = std::clamp(-Cfg::kRatioPpmPerMs * deviationMs, -Cfg::kMaxRatioPpm,
                                  Cfg::kMaxRatioPpm);
    const double ratio = 1.0 + ppm * 1e-6;
    if (ratio != m_ratio) {
        m_ratio = ratio;
        m_matcher->forceRatio(true, m_ratio);
    }
    resetWindow();
}

void RemoteMicFeed::publishOver()
{
    m_statsBlocks.store(m_overBlocks, std::memory_order_relaxed);
    m_statsAddedMeanMs.store(m_overBlocks > 0 ? m_overAddedSumMs / m_overBlocks : 0.0,
                             std::memory_order_relaxed);
    m_statsAddedMaxMs.store(m_overAddedMaxMs, std::memory_order_relaxed);
    m_statsRingMeanMs.store(m_overRingBlocks > 0 ? m_overRingSumMs / m_overRingBlocks : -1.0,
                            std::memory_order_relaxed);
    m_statsRingMaxMs.store(m_overRingMaxMs, std::memory_order_relaxed);
    m_statsShed.store(m_overShed, std::memory_order_relaxed);
    m_statsShedForRing.store(m_overShedForRing, std::memory_order_relaxed);
    m_statsInserted.store(m_overInserted, std::memory_order_relaxed);
    m_statsGrows.store(m_overGrows, std::memory_order_relaxed);
    m_statsHeld.store(m_overHeld, std::memory_order_relaxed);
    m_statsOwnerWaitMeanMs.store(
        m_overOwnerWaits > 0 ? static_cast<double>(m_overOwnerWaitSumFrames)
                / m_overOwnerWaits / Cfg::kFramesPerMs
                             : 0.0,
        std::memory_order_relaxed);
    m_statsOwnerWaitMaxMs.store(static_cast<double>(m_overOwnerWaitMaxFrames) / Cfg::kFramesPerMs,
                                std::memory_order_relaxed);
    m_statsOwnerWaitsLong.store(m_overOwnerWaitsLong, std::memory_order_relaxed);
}

void RemoteMicFeed::endBlock()
{
    ++m_block;
    if (m_block - m_windowStart >= static_cast<quint64>(kWindowBlocks)) {
        endWindow();
    }
    m_targetFrames.store(currentTarget(), std::memory_order_relaxed);
    m_statsMargin.store(m_marginFrames, std::memory_order_relaxed);
    m_statsFill.store(m_bufferCount + m_matcherFill, std::memory_order_relaxed);
    m_statsRatio.store(m_ratio, std::memory_order_relaxed);
    m_statsUnderflows.store(m_underflows, std::memory_order_relaxed);
    publishOver();
}

RemoteMicFeed::Pull RemoteMicFeed::pullBlock(float* dst, int frames, double downstreamQueuedMs,
                                             bool holdSplices)
{
    const quint64 change = m_change.load(std::memory_order_acquire);
    if (change != m_seenChange) {
        // A change of use: drop what was written before it and start the
        // buffer again. Rebuilding the matcher is the one allocation on
        // this thread, once per key or VOX change, never per block.
        discardInputTo(m_clearAtBytes.load(std::memory_order_acquire));
        takeArrivals(false);
        m_matcher->reset();
        // WDSP rmatch starts with its ring half full of silence
        // (calc_rmatch: n_ring = rsize / 2). Read it out, so the buffer
        // counts only the microphone's audio.
        while (m_matcher->stats().ringFillFrames >= kPumpBlock) {
            if (!m_matcher->takeInto(m_stereoScratch.data(), kPumpBlock)) {
                break;
            }
        }
        m_matcherFill = m_matcher->stats().ringFillFrames;
        m_bufferHead = 0;
        m_bufferCount = 0;
        m_started = false;
        m_resumingAfterUnderrun = false;
        m_silentRunFrames = 0;
        m_underflows = 0;
        m_shedBudget = 0;
        m_insertBudget = 0;
        m_ringShedBudget = 0;
        m_ratio = 1.0;
        m_matcher->forceRatio(true, m_ratio);
        // The arrival clock starts again with the stream; the margin is the
        // link's and is kept (it eases back if the link is steady).
        m_arrivedFrames = 0;
        std::fill(m_delayMinMemory.begin(), m_delayMinMemory.end(),
                  std::numeric_limits<qint64>::max());
        std::fill(m_delayMaxMemory.begin(), m_delayMaxMemory.end(),
                  std::numeric_limits<qint64>::min());
        resetWindow();
        // TX diagnostics lane: the over's arrival figures start again.
        m_changeBlock = m_block;
        m_overArrivals = 0;
        m_overRtpOffsetMinUs = std::numeric_limits<qint64>::max();
        m_prevWindowGapMaxUs = -1;
        m_prevWindowLateMaxUs = -1;
        m_openUnderrun = -1;
        m_seenChange = change;
        const bool inUse = m_inUseForPump.load(std::memory_order_acquire);
        if (inUse) {
            // A new over: its figures start here (the last over's stay
            // readable until then).
            m_overBlocks = 0;
            m_overAddedSumMs = 0.0;
            m_overAddedMaxMs = 0.0;
            m_overRingBlocks = 0;
            m_overRingSumMs = 0.0;
            m_overRingMaxMs = -1.0;
            m_overShed = 0;
            m_overShedForRing = 0;
            m_overInserted = 0;
            m_overGrows = 0;
            m_overHeld = 0;
            m_overOwnerWaits = 0;
            m_overOwnerWaitSumFrames = 0;
            m_overOwnerWaitMaxFrames = 0;
            m_overOwnerWaitsLong = 0;
            m_underrunsPlaced = 0;
            m_statsUnderrunsPlaced.store(0, std::memory_order_release);
            publishOver();
        }
        m_statsStarted.store(false, std::memory_order_relaxed);
        m_statsFill.store(0, std::memory_order_relaxed);
        m_statsRatio.store(1.0, std::memory_order_relaxed);
        m_statsUnderflows.store(0, std::memory_order_relaxed);
        m_statsOverflows.store(0, std::memory_order_relaxed);
        m_statsChanges.store(change, std::memory_order_relaxed);
    }
    if (!m_inUseForPump.load(std::memory_order_acquire)) {
        discardAllInput();
        takeArrivals(false);
        m_bufferCount = 0;
        return Pull::NotInUse;
    }
    if (dst == nullptr || frames != kPumpBlock) {
        return Pull::NotInUse;
    }

    drainInput();
    takeArrivals(true);
    RemoteAudioRateMatcherStats matcher = m_matcher->stats();
    m_matcherFill = matcher.ringFillFrames;
    m_statsOverflows.store(matcher.overflows, std::memory_order_relaxed);
    int fill = m_bufferCount + m_matcherFill;

    if (!m_started) {
        if (fill < currentTarget()) {
            std::fill(dst, dst + frames, 0.0f);
            endBlock();
            return Pull::Audio;
        }
        // LINK minor 12 (TX audio): a stall's held audio past
        // kStaleAfterStallMs is not sent late; start again from the target.
        if (m_resumingAfterUnderrun && fill > Cfg::kStaleAfterStallFrames) {
            trimStaleToTarget();
            fill = m_bufferCount + m_matcherFill;
        }
        m_resumingAfterUnderrun = false;
        m_started = true;
        m_statsStarted.store(true, std::memory_order_relaxed);
        closeUnderrun();
    }

    // The over's figures and the window's floors, before this block plays.
    m_windowPlayed = true;
    m_windowFloor = std::min(m_windowFloor, fill);
    double addedMs = static_cast<double>(fill) / Cfg::kFramesPerMs;
    if (downstreamQueuedMs >= 0.0) {
        addedMs += downstreamQueuedMs;
        m_windowRingFloorMs = m_windowRingFloorMs < 0.0
            ? downstreamQueuedMs
            : std::min(m_windowRingFloorMs, downstreamQueuedMs);
        ++m_overRingBlocks;
        m_overRingSumMs += downstreamQueuedMs;
        m_overRingMaxMs = std::max(m_overRingMaxMs, downstreamQueuedMs);
    }
    ++m_overBlocks;
    m_overAddedSumMs += addedMs;
    m_overAddedMaxMs = std::max(m_overAddedMaxMs, addedMs);

    // DEXP counts its hold, decay and VOX turn-off in the samples it
    // processes (Thetis wdsp/dexp.c:142-144, 312-381 [v2.10.3.15]), and
    // WDSP has no call to move those counts; so while one runs nothing is
    // spliced and DEXP sees the pause as it was spoken.
    if (holdSplices) {
        ++m_overHeld;
    }
    const bool mayShed = !holdSplices;

    // Excess past the send ring: shed a silent block and skip this pump
    // block, so TX DSP never runs on it and the ring drains by one block.
    if (mayShed && downstreamQueuedMs >= 0.0 && m_ringShedBudget >= kPumpBlock
        && canSplice()) {
        dropBlock();
        m_ringShedBudget -= kPumpBlock;
        m_overShedForRing += kPumpBlock;
        endBlock();
        return Pull::Shed;
    }

    // Feed rmatch only what this block needs; the buffer keeps the rest.
    // At most one splice (a silent block shed, or one inserted) a block.
    bool spliced = false;
    while (m_matcherFill < kPumpBlock) {
        if (mayShed && !spliced && m_insertBudget >= kPumpBlock && canSplice()) {
            std::fill(m_stereoScratch.begin(), m_stereoScratch.end(), 0.0f);
            m_matcher->push(m_stereoScratch.data(), kPumpBlock);
            m_insertBudget -= kPumpBlock;
            m_overInserted += kPumpBlock;
            m_silentRunFrames += kPumpBlock;
            spliced = true;
        } else if (m_bufferCount < kPumpBlock) {
            break;
        } else if (mayShed && !spliced && m_shedBudget >= kPumpBlock && canSplice()) {
            dropBlock();
            m_shedBudget -= kPumpBlock;
            m_overShed += kPumpBlock;
            spliced = true;
            continue;
        } else {
            popBlock(m_monoScratch.data());
            for (int i = 0; i < kPumpBlock; ++i) {
                const float sample = m_monoScratch[static_cast<size_t>(i)];
                m_stereoScratch[static_cast<size_t>(2 * i)] = sample;
                m_stereoScratch[static_cast<size_t>(2 * i + 1)] = sample;
            }
            m_matcher->push(m_stereoScratch.data(), kPumpBlock);
        }
        m_matcherFill = m_matcher->stats().ringFillFrames;
    }

    if (m_matcherFill < kPumpBlock) {
        // Nothing left to play: rmatch fades what it holds out (its dslew)
        // and the buffer fills again to its target, grown by how late the
        // audio turns out to be when it comes back (noteArrival).
        ++m_underflows;
        m_windowUnderrun = true;
        m_started = false;
        m_resumingAfterUnderrun = true;
        m_statsStarted.store(false, std::memory_order_relaxed);
        openUnderrun();
    }
    if (!m_matcher->takeInto(m_stereoScratch.data(), kPumpBlock)) {
        std::fill(dst, dst + frames, 0.0f);
    } else {
        for (int i = 0; i < kPumpBlock; ++i) {
            dst[i] = m_stereoScratch[static_cast<size_t>(2 * i)];
        }
    }
    m_matcherFill = m_matcher->stats().ringFillFrames;
    endBlock();
    return Pull::Audio;
}

RemoteMicFeed::Stats RemoteMicFeed::stats() const
{
    Stats stats;
    stats.started = m_statsStarted.load(std::memory_order_relaxed);
    stats.fillFrames = m_statsFill.load(std::memory_order_relaxed);
    stats.ratio = m_statsRatio.load(std::memory_order_relaxed);
    stats.underflows = m_statsUnderflows.load(std::memory_order_relaxed);
    stats.overflows = m_statsOverflows.load(std::memory_order_relaxed);
    stats.droppedFrames = m_droppedFrames.load(std::memory_order_relaxed);
    stats.changes = m_statsChanges.load(std::memory_order_relaxed);
    stats.targetFrames = m_targetFrames.load(std::memory_order_relaxed);
    stats.marginFrames = m_statsMargin.load(std::memory_order_relaxed);
    stats.blocks = m_statsBlocks.load(std::memory_order_relaxed);
    stats.addedMeanMs = m_statsAddedMeanMs.load(std::memory_order_relaxed);
    stats.addedMaxMs = m_statsAddedMaxMs.load(std::memory_order_relaxed);
    stats.ringMeanMs = m_statsRingMeanMs.load(std::memory_order_relaxed);
    stats.ringMaxMs = m_statsRingMaxMs.load(std::memory_order_relaxed);
    stats.shedFrames = m_statsShed.load(std::memory_order_relaxed);
    stats.shedForRingFrames = m_statsShedForRing.load(std::memory_order_relaxed);
    stats.insertedFrames = m_statsInserted.load(std::memory_order_relaxed);
    stats.grows = m_statsGrows.load(std::memory_order_relaxed);
    stats.heldBlocks = m_statsHeld.load(std::memory_order_relaxed);
    stats.ownerWaitMeanMs = m_statsOwnerWaitMeanMs.load(std::memory_order_relaxed);
    stats.ownerWaitMaxMs = m_statsOwnerWaitMaxMs.load(std::memory_order_relaxed);
    stats.ownerWaitsLong = m_statsOwnerWaitsLong.load(std::memory_order_relaxed);
    stats.underrunsPlacedCount = std::clamp(m_statsUnderrunsPlaced.load(std::memory_order_acquire),
                                            0, Stats::kMaxUnderrunsPlaced);
    for (int k = 0; k < stats.underrunsPlacedCount; ++k) {
        const auto i = static_cast<size_t>(k);
        Stats::Underrun& event = stats.underrunsPlaced[i];
        event.atLineMs = m_statsUnderrunAtMs[i].load(std::memory_order_relaxed);
        event.atSteadyUs = m_statsUnderrunSteadyUs[i].load(std::memory_order_relaxed);
        event.silentMs = m_statsUnderrunSilentMs[i].load(std::memory_order_relaxed);
        event.arrivalGapMs = m_statsUnderrunGapMs[i].load(std::memory_order_relaxed);
        event.lateMs = m_statsUnderrunLateMs[i].load(std::memory_order_relaxed);
    }
    return stats;
}

// ============================================================================
// RemoteMicEncoder
// ============================================================================

struct RemoteMicEncoder::State {
    OpusEncoder* encoder = nullptr;
    std::vector<unsigned char> payload =
        std::vector<unsigned char>(static_cast<size_t>(OpusAudioCodecConfig::kMaxPayloadBytes));
    ~State()
    {
        if (encoder != nullptr) {
            opus_encoder_destroy(encoder);
        }
    }
};

RemoteMicEncoder::RemoteMicEncoder()
    : m_state(std::make_unique<State>())
{
    reset();
}

RemoteMicEncoder::~RemoteMicEncoder() = default;

void RemoteMicEncoder::reset()
{
    if (m_state->encoder != nullptr) {
        opus_encoder_destroy(m_state->encoder);
        m_state->encoder = nullptr;
    }
    int error = OPUS_OK;
    // Speech from a microphone: the VOIP application, whose SILK and hybrid
    // modes carry in-band FEC (LBRR). FEC is sent only when the encoder
    // expects loss, so it is told to expect some.
    OpusEncoder* encoder = opus_encoder_create(RemoteMicConfig::kSampleRate, 1,
                                               OPUS_APPLICATION_VOIP, &error);
    if (encoder == nullptr || error != OPUS_OK) {
        qCWarning(lcAudio) << "Remote microphone: the Opus encoder could not start:" << error;
        return;
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(RemoteMicConfig::kOpusBitrate));
    opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(10));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    m_state->encoder = encoder;
}

bool RemoteMicEncoder::isReady() const
{
    return m_state->encoder != nullptr;
}

QByteArray RemoteMicEncoder::encode(const float* mono, quint16 sequence, quint32 timestamp,
                                    quint32 ssrc)
{
    if (m_state->encoder == nullptr || mono == nullptr) {
        return {};
    }
    const opus_int32 bytes =
        opus_encode_float(m_state->encoder, mono, RemoteMicConfig::kOpusFrameSamples,
                          m_state->payload.data(),
                          static_cast<opus_int32>(m_state->payload.size()));
    if (bytes <= 0) {
        return {};
    }
    const QByteArray payload(reinterpret_cast<const char*>(m_state->payload.data()), bytes);
    return buildAudioRtp(RemoteMicConfig::kOpusPayloadType, sequence, timestamp, ssrc, payload);
}

// ============================================================================
// RemoteMicReceiver
// ============================================================================

struct RemoteMicReceiver::Decoder {
    OpusDecoder* opus = nullptr;
    ~Decoder()
    {
        if (opus != nullptr) {
            opus_decoder_destroy(opus);
        }
    }
};

RemoteMicReceiver::RemoteMicReceiver(RemoteMicFeed* feed, QObject* parent, Clock clock,
                                     Scheduler scheduler)
    : QObject(parent)
    , m_feed(feed)
    , m_clock(std::move(clock))
    , m_scheduler(std::move(scheduler))
    , m_decoder(std::make_unique<Decoder>())
    , m_pcm(static_cast<size_t>(kMaxOpusFrames))
{
    if (!m_clock) {
        m_clock = steadyMs;
    }
    if (!m_scheduler) {
        m_scheduler = [this](int ms, std::function<void()> fire) {
            QTimer::singleShot(ms, this, std::move(fire));
        };
    }
}

RemoteMicReceiver::~RemoteMicReceiver() = default;

qint64 RemoteMicReceiver::now() const
{
    return m_clock();
}

bool RemoteMicReceiver::isRunning() const
{
    const std::lock_guard<std::mutex> lock(m_lineLock);
    return m_running;
}

quint32 RemoteMicReceiver::ssrc() const
{
    const std::lock_guard<std::mutex> lock(m_lineLock);
    return m_ssrc;
}

void RemoteMicReceiver::setLosslessNegotiated(bool negotiated)
{
    const std::lock_guard<std::mutex> lock(m_lineLock);
    m_lossless = negotiated;
}

RemoteMicReceiver::Stats RemoteMicReceiver::stats() const
{
    const std::lock_guard<std::mutex> lock(m_lineLock);
    return m_stats;
}

bool RemoteMicReceiver::start(quint32 ssrc, bool losslessNegotiated)
{
    stop();
    int error = OPUS_OK;
    OpusDecoder* opus = opus_decoder_create(RemoteMicConfig::kSampleRate, 1, &error);
    if (opus == nullptr || error != OPUS_OK) {
        qCWarning(lcAudio) << "Remote microphone: the Opus decoder could not start:" << error;
        return false;
    }
    {
        const std::lock_guard<std::mutex> lock(m_lineLock);
        m_decoder->opus = opus;
        m_ssrc = ssrc;
        m_lossless = losslessNegotiated;
        m_haveSequence = false;
        m_lastOpusFrames = RemoteMicConfig::kOpusFrameSamples;
        m_stats = {};
        m_lastReceiptUs = -1;
        m_haveRtpTimestamp = false;
        m_rtpFrames = 0;
        m_lastRtpOffsetUs = 0;
        m_running = true;
    }
    m_lastAudioMs.store(now(), std::memory_order_release);
    return true;
}

void RemoteMicReceiver::stop()
{
    // The line ends: a key waiting on it is answered not ready.
    if (m_waitDone) {
        std::function<void(bool)> done = std::move(m_waitDone);
        m_waitDone = nullptr;
        ++m_waitGeneration;
        done(false);
    }
    if (m_starved) {
        m_starved = false;
        emit starved(false);
    }
    m_watching = false;
    ++m_starvationGeneration;
    m_starvationCheckPending = false;
    const std::lock_guard<std::mutex> lock(m_lineLock);
    if (m_decoder->opus != nullptr) {
        opus_decoder_destroy(m_decoder->opus);
        m_decoder->opus = nullptr;
    }
    m_running = false;
    m_ssrc = 0;
}

void RemoteMicReceiver::submit(const QByteArray& packet, qint64 heldUs)
{
    const qint64 clampedHeldUs = std::clamp<qint64>(heldUs, 0, 10'000'000);
    bool wroteAudio = false;
    {
        // TX mic thread: the line's state, on whichever thread delivers it.
        const std::lock_guard<std::mutex> lock(m_lineLock);
        if (!m_running) {
            return;
        }
        // TX stall lane: the packet's wait at the Core, for its timing in
        // the buffer and the over's figures.
        m_heldFrames = static_cast<int>(clampedHeldUs * Cfg::kFramesPerMs / 1000);
        const int payloadType = audioRtpPayloadType(packet);
        const bool opus = payloadType == RemoteMicConfig::kOpusPayloadType;
        const bool l16 = payloadType == PcmAudioCodecConfig::kPayloadType && m_lossless;
        AudioRtpPacket parsed;
        if ((!opus && !l16)
            || parseAudioRtp(packet, payloadType, parsed) != OpusAudioCodecStatus::Accepted
            || parsed.ssrc != m_ssrc) {
            ++m_stats.rejectedPackets;
            return;
        }
        // Sequence order: a packet behind the stream (reordered too late, or
        // a repeat) is dropped; a gap ahead is concealed before this packet.
        int missing = 0;
        if (m_haveSequence) {
            const qint16 delta = static_cast<qint16>(parsed.sequence - m_expectedSequence);
            if (delta < 0) {
                ++m_stats.latePackets;
                return;
            }
            missing = delta;
        }
        ++m_stats.accepted;
        // TX diagnostics lane (measurement only): the gap since the line's
        // last packet was received, and its receipt less its RTP
        // timestamp's time, on the line's own scale.
        {
            const qint64 receiptUs = now() * 1000 - clampedHeldUs;
            m_timing.receiptGapUs = m_lastReceiptUs >= 0 ? receiptUs - m_lastReceiptUs : -1;
            m_lastReceiptUs = receiptUs;
            if (!m_haveRtpTimestamp) {
                m_haveRtpTimestamp = true;
                m_rtpFrames = 0;
            } else {
                const qint64 step = static_cast<qint32>(parsed.timestamp - m_lastRtpTimestamp);
                if (step > kRtpRebaseFrames || step < -kRtpRebaseFrames) {
                    // A new stream on the line: it carries on from the
                    // last packet's offset.
                    m_rtpFrames = (receiptUs - m_lastRtpOffsetUs) * Cfg::kFramesPerMs / 1000;
                } else {
                    m_rtpFrames += step;
                }
            }
            m_lastRtpTimestamp = parsed.timestamp;
            m_lastRtpOffsetUs = receiptUs - m_rtpFrames * 1000 / Cfg::kFramesPerMs;
            m_timing.rtpOffsetUs = m_lastRtpOffsetUs;
        }
        const quint64 writtenBefore = m_stats.framesWritten;
        if (opus) {
            decodeOpus(parsed.payload, missing);
        } else {
            decodeL16(packet, missing);
        }
        m_haveSequence = true;
        m_expectedSequence = static_cast<quint16>(parsed.sequence + 1);
        wroteAudio = m_stats.framesWritten != writtenBefore;
    }

    // Audio arrived (TX mic thread: when it reached the transport): a
    // starvation ends, and the next check runs from then. Fix round 2: it
    // only moves forward (as in setWatching), so a packet received before a
    // watch began but handled after it cannot move it back.
    {
        const qint64 receivedAt = now() - clampedHeldUs / 1000;
        qint64 last = m_lastAudioMs.load(std::memory_order_acquire);
        while (last < receivedAt
               && !m_lastAudioMs.compare_exchange_weak(last, receivedAt,
                                                       std::memory_order_acq_rel)) {
        }
    }
    if (thread() == QThread::currentThread()) {
        afterPacket(wroteAudio);
    } else {
        QMetaObject::invokeMethod(
            this, [this, wroteAudio]() { afterPacket(wroteAudio); }, Qt::QueuedConnection);
    }
}

void RemoteMicReceiver::afterPacket(bool wroteAudio)
{
    if (!isRunning()) {
        return;
    }
    if (m_starved) {
        m_starved = false;
        emit starved(false);
    }
    if (m_watching && !m_starvationCheckPending) {
        scheduleStarvationCheck(RemoteMicConfig::kStarvationMs);
    }
    if (wroteAudio) {
        checkReady();
    }
    // Load findings 2 (R-IOS-13): the waiting key's line has started; the
    // buffer now has kReadyDeadlineMs to fill. A trailing packet of the
    // previous over, still in flight at a quick re-press, starts this clock
    // too; the cost is at most the old cold-start refusal, never a key
    // without audio (review of the line-start wait).
    if (m_waitDone && !m_waitLineStarted) {
        m_waitLineStarted = true;
        qCInfo(lcAudio) << "Remote microphone: the line's first packet came"
                        << now() - m_waitStartedMs << "ms into the key's wait";
        refuseWaitAfter(RemoteMicConfig::kReadyDeadlineMs, false);
    }
}

void RemoteMicReceiver::decodeOpus(const QByteArray& payload, int missing)
{
    OpusDecoder* decoder = m_decoder->opus;
    if (decoder == nullptr) {
        return;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(payload.constData());
    const auto size = static_cast<opus_int32>(payload.size());
    if (missing > 0) {
        if (static_cast<qint64>(missing) * m_lastOpusFrames > kMaxConcealFrames) {
            // Longer than the buffer's target: nothing inserted, and the
            // decoder starts again with this packet.
            ++m_stats.longGaps;
            opus_decoder_ctl(decoder, OPUS_RESET_STATE);
        } else {
            // All but the last lost packet by loss concealment...
            for (int lost = 0; lost < missing - 1; ++lost) {
                const int frames = opus_decode_float(decoder, nullptr, 0, m_pcm.data(),
                                                     m_lastOpusFrames, 0);
                if (frames > 0) {
                    ++m_stats.concealedPackets;
                    writeAudio(m_pcm.data(), frames);
                }
            }
            // ...and the one just before this packet from this packet's
            // in-band FEC. The encoder codes FEC only for frames its voice
            // detector calls active; for any other the decoder conceals.
            const bool carriesFec = opusPacketCarriesFec(payload);
            const int frames =
                opus_decode_float(decoder, data, size, m_pcm.data(), m_lastOpusFrames, 1);
            if (frames > 0) {
                if (carriesFec) {
                    ++m_stats.recoveredPackets;
                } else {
                    ++m_stats.concealedPackets;
                }
                writeAudio(m_pcm.data(), frames);
            }
        }
    }
    const int frames = opus_decode_float(decoder, data, size, m_pcm.data(), kMaxOpusFrames, 0);
    if (frames <= 0) {
        ++m_stats.rejectedPackets;
        return;
    }
    m_lastOpusFrames = frames;
    ++m_stats.decodedPackets;
    writeAudio(m_pcm.data(), frames);
}

void RemoteMicReceiver::decodeL16(const QByteArray& packet, int missing)
{
    const PcmRtpDecodeResult decoded = decodeL16Rtp(packet, m_ssrc);
    if (decoded.status != OpusAudioCodecStatus::Accepted) {
        ++m_stats.rejectedPackets;
        return;
    }
    constexpr int kFrames = PcmAudioCodecConfig::kPacketFrames;
    if (missing > 0) {
        if (static_cast<qint64>(missing) * kFrames > kMaxConcealFrames) {
            ++m_stats.longGaps;
        } else {
            // A lost lossless packet is 4 ms of silence.
            std::fill(m_pcm.begin(), m_pcm.begin() + kFrames, 0.0f);
            for (int lost = 0; lost < missing; ++lost) {
                ++m_stats.concealedPackets;
                writeAudio(m_pcm.data(), kFrames);
            }
        }
    }
    // The window sends its microphone in both channels; mono is their mean.
    for (int i = 0; i < kFrames; ++i) {
        m_pcm[static_cast<size_t>(i)] = 0.5f
            * (decoded.pcmInterleaved.at(2 * i) + decoded.pcmInterleaved.at(2 * i + 1));
    }
    ++m_stats.decodedPackets;
    writeAudio(m_pcm.data(), kFrames);
}

void RemoteMicReceiver::writeAudio(const float* mono, int frames)
{
    // Fix wave C2: one writer at a time. Another device's line is decoded
    // (its starvation and its stream stay tracked) but never reaches the
    // transmitter's feed.
    if (!m_feedWriter.load(std::memory_order_acquire)) {
        return;
    }
    if (m_feed != nullptr && m_feed->write(mono, frames, m_heldFrames, m_timing)) {
        m_stats.framesWritten += static_cast<quint64>(frames);
    }
}

void RemoteMicReceiver::awaitReady(std::function<void(bool ready)> done)
{
    m_waitDone = std::move(done);
    m_waitLineStarted = false;
    m_waitStartedMs = now();
    const quint64 generation = ++m_waitGeneration;
    checkReady();
    if (!m_waitDone || generation != m_waitGeneration) {
        return;
    }
    // Load findings 2 (R-IOS-13): the device starts its line with the key,
    // so the fill deadline waits for the line's first packet (submit()); a
    // line that sends nothing is refused here.
    refuseWaitAfter(RemoteMicConfig::kLineStartDeadlineMs, true);
}

void RemoteMicReceiver::refuseWaitAfter(int ms, bool onlyBeforeFirstPacket)
{
    const quint64 generation = m_waitGeneration;
    QPointer<RemoteMicReceiver> self(this);
    m_scheduler(ms, [self, generation, onlyBeforeFirstPacket]() {
        if (self.isNull() || generation != self->m_waitGeneration || !self->m_waitDone
            || (onlyBeforeFirstPacket && self->m_waitLineStarted)) {
            return;
        }
        // TX mic thread: the line's audio may be in the feed with its event
        // loop part still posted behind a stall; a key whose buffer holds
        // its target is answered ready, never refused for that wait.
        self->checkReady();
        if (self.isNull() || !self->m_waitDone || generation != self->m_waitGeneration) {
            return;
        }
        if (onlyBeforeFirstPacket) {
            qCInfo(lcAudio) << "Remote microphone: no packet on the line within"
                            << RemoteMicConfig::kLineStartDeadlineMs << "ms of the key";
        }
        std::function<void(bool)> done = std::move(self->m_waitDone);
        self->m_waitDone = nullptr;
        ++self->m_waitGeneration;
        done(false);
    });
}

void RemoteMicReceiver::cancelWait()
{
    m_waitDone = nullptr;
    ++m_waitGeneration;
}

void RemoteMicReceiver::checkReady()
{
    if (!m_waitDone || !m_feedWriter.load(std::memory_order_acquire) || m_feed == nullptr
        || !m_feed->inUse()
        || m_feed->framesSinceInUse() < m_feed->targetFrames()) {
        return;
    }
    std::function<void(bool)> done = std::move(m_waitDone);
    m_waitDone = nullptr;
    ++m_waitGeneration;
    done(true);
}

void RemoteMicReceiver::setFeedWriter(bool writer)
{
    if (writer == m_feedWriter.load(std::memory_order_acquire)) {
        return;
    }
    m_feedWriter.store(writer, std::memory_order_release);
    // A key waiting on this line fills from here.
    checkReady();
}

void RemoteMicReceiver::setWatching(bool watching)
{
    if (watching == m_watching) {
        return;
    }
    m_watching = watching;
    ++m_starvationGeneration;
    m_starvationCheckPending = false;
    if (watching) {
        // Measured from the later of the last audio and the start of the
        // watch.
        const qint64 at = now();
        qint64 last = m_lastAudioMs.load(std::memory_order_acquire);
        while (last < at
               && !m_lastAudioMs.compare_exchange_weak(last, at, std::memory_order_acq_rel)) {
        }
        scheduleStarvationCheck(RemoteMicConfig::kStarvationMs);
    } else if (m_starved) {
        m_starved = false;
        emit starved(false);
    }
}

void RemoteMicReceiver::scheduleStarvationCheck(int ms)
{
    m_starvationCheckPending = true;
    const quint64 generation = m_starvationGeneration;
    QPointer<RemoteMicReceiver> self(this);
    m_scheduler(std::max(1, ms), [self, generation]() {
        if (self.isNull() || generation != self->m_starvationGeneration) {
            return;
        }
        self->m_starvationCheckPending = false;
        self->checkStarvation();
    });
}

void RemoteMicReceiver::checkStarvation()
{
    if (!m_watching || !isRunning()) {
        return;
    }
    // TX mic thread: the last audio's receipt, so a stall of the event loop
    // that delayed this check is not taken for a quiet line.
    const qint64 quiet = now() - m_lastAudioMs.load(std::memory_order_acquire);
    if (quiet >= RemoteMicConfig::kStarvationMs) {
        if (!m_starved) {
            m_starved = true;
            qCInfo(lcAudio) << "Remote microphone: no audio for" << quiet << "ms while keyed";
            emit starved(true);
        }
        return;   // the next audio schedules the next check
    }
    scheduleStarvationCheck(static_cast<int>(RemoteMicConfig::kStarvationMs - quiet));
}

} // namespace NereusSDR
