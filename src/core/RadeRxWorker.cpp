// no-port-check: NereusSDR-original. AetherSDR and freedv-gui are cited for
// structure only; no upstream code is ported.
// =================================================================
// src/core/RadeRxWorker.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See RadeRxWorker.h for the structure it follows
// (AetherSDR's RADEEngine worker thread, freedv-gui's FIFOs) and for the
// late bound.
//
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd (KG4VCF)  Created for the RADE threads lane.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd (KG4VCF)  The decoder thread registers with
//                 ThreadPlacement as a DspThread, so nereusd places it
//                 with the DSP thread. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-30  J.J. Boyd (KG4VCF)  Registers as a RadeDecoder for its
//                 channel instead, so nereusd puts it on the least busy
//                 fast core, not the DSP thread's (JJ's ruling of
//                 2026-09-30). AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#include "core/RadeRxWorker.h"
#include "core/platform/ThreadPlacement.h"

#include <QThread>

#include <chrono>
#include <cstring>

namespace NereusSDR {

namespace {

constexpr size_t kHeaderBytes = sizeof(RadeRxBridge::Header);

// A never-waiting owner guard: whoever does not get it drops the call.
class TryGuard {
public:
    explicit TryGuard(std::atomic_flag& flag)
        : m_flag(flag), m_owned(!flag.test_and_set(std::memory_order_acquire)) {}
    ~TryGuard()
    {
        if (m_owned) {
            m_flag.clear(std::memory_order_release);
        }
    }
    TryGuard(const TryGuard&) = delete;
    TryGuard& operator=(const TryGuard&) = delete;
    bool owned() const { return m_owned; }

private:
    std::atomic_flag& m_flag;
    bool m_owned;
};

// Serial record write: header and payload in one all-or-nothing push, so a
// reader that sees any of a record sees all of it.
template <size_t Capacity>
bool pushRecord(AudioRingSpsc<Capacity>& ring, std::vector<float>& scratch,
                const RadeRxBridge::Header& header, const float* data, int frames)
{
    const size_t payload = size_t(frames) * 2 * sizeof(float);
    const size_t total = kHeaderBytes + payload;
    const size_t floats = (total + sizeof(float) - 1) / sizeof(float);
    if (scratch.size() < floats) {
        scratch.resize(floats);
    }
    auto* bytes = reinterpret_cast<uint8_t*>(scratch.data());
    std::memcpy(bytes, &header, kHeaderBytes);
    if (payload > 0) {
        std::memcpy(bytes + kHeaderBytes, data, payload);
    }
    return ring.tryPushCopy(bytes, qint64(total)) == qint64(total);
}

template <size_t Capacity>
bool peekHeader(const AudioRingSpsc<Capacity>& ring, RadeRxBridge::Header& header)
{
    return ring.peekInto(reinterpret_cast<uint8_t*>(&header), kHeaderBytes);
}

template <size_t Capacity>
bool popRecord(AudioRingSpsc<Capacity>& ring, RadeRxBridge::Header& header,
               std::vector<float>& data)
{
    if (!peekHeader(ring, header)) {
        return false;
    }
    ring.dropOldest(kHeaderBytes);
    const int frames = header.frames > 0 ? header.frames : 0;
    data.resize(size_t(frames) * 2);
    const qint64 payload = qint64(frames) * 2 * qint64(sizeof(float));
    if (payload > 0) {
        ring.popInto(reinterpret_cast<uint8_t*>(data.data()), payload);
    }
    return true;
}

template <size_t Capacity>
void dropRecord(AudioRingSpsc<Capacity>& ring, const RadeRxBridge::Header& header)
{
    const int frames = header.frames > 0 ? header.frames : 0;
    ring.dropOldest(kHeaderBytes + size_t(frames) * 2 * sizeof(float));
}

}  // namespace

// ── RadeRxBridge ────────────────────────────────────────────────────────────

bool RadeRxBridge::pushInput(quint32 epoch, quint32 seq, const float* iq, int frames)
{
    if (frames <= 0 || frames > kMaxRecordFrames) {
        return false;
    }
    TryGuard guard(m_inputProducer);
    if (!guard.owned()) {
        m_inputDrops.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const Header header{epoch, seq, frames};
    if (!pushRecord(m_input, m_pushScratch, header, iq, frames)) {
        m_inputDrops.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    m_pushed.fetch_add(1, std::memory_order_release);
    return true;
}

int RadeRxBridge::takeDue(quint32 epoch, quint32 due, std::vector<float>& out)
{
    TryGuard guard(m_outputConsumer);
    if (!guard.owned()) {
        return 0;
    }
    Header header;
    while (peekHeader(m_output, header)) {
        const bool sameEpoch = header.epoch == epoch;
        // Sequence numbers wrap; compare by signed distance.
        const qint32 age = qint32(due - header.seq);
        if (sameEpoch && age == 0) {
            popRecord(m_output, header, out);
            return header.frames;
        }
        if (sameEpoch && age < 0) {
            // Early: this record belongs to a later slot. Leave it.
            return 0;
        }
        // Older than the due slot, or from a route that has been replaced:
        // its slot has already played silence, so it is dropped, never
        // played late.
        dropRecord(m_output, header);
        m_lateDrops.fetch_add(1, std::memory_order_relaxed);
    }
    return 0;
}

bool RadeRxBridge::popInput(Header& header, std::vector<float>& iq)
{
    return popRecord(m_input, header, iq);
}

bool RadeRxBridge::pushOutput(const Header& header, const float* speech, int frames)
{
    // Only the decoder thread writes the output ring, so a per-thread
    // scratch is its alone.
    thread_local std::vector<float> scratch;
    Header out = header;
    out.frames = frames;
    if (frames < 0 || frames > kMaxRecordFrames
        || !pushRecord(m_output, scratch, out, speech, frames)) {
        m_outputDrops.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

// ── RadeRxWorker ────────────────────────────────────────────────────────────

RadeRxWorker::RadeRxWorker(DecodeFn decode)
    : m_decode(std::move(decode)), m_bridge(std::make_shared<RadeRxBridge>())
{
}

RadeRxWorker::~RadeRxWorker()
{
    stop();
}

void RadeRxWorker::start(const QString& name, int placementChannel)
{
    if (m_thread) {
        return;
    }
    m_stopping.store(false, std::memory_order_release);
    m_threadName = name;
    m_placementChannel = placementChannel;
    m_thread.reset(QThread::create([this] { run(); }));
    m_thread->setObjectName(name);
    m_thread->start();
}

void RadeRxWorker::stop()
{
    if (!m_thread) {
        return;
    }
    m_stopping.store(true, std::memory_order_release);
    m_bridge->m_wake.fetch_add(1, std::memory_order_release);
    m_bridge->m_wake.notify_all();
    m_thread->wait();
    m_thread.reset();
    m_threadName.clear();
    m_threadId.store(nullptr, std::memory_order_release);
    {
        // Wake any test waiter so it re-reads the counters.
        std::lock_guard<std::mutex> lock(m_idleMutex);
    }
    m_idle.notify_all();
}

bool RadeRxWorker::isRunning() const
{
    return m_thread != nullptr;
}

void RadeRxWorker::wake(RadeRxBridge& bridge)
{
    bridge.m_wake.fetch_add(1, std::memory_order_release);
    bridge.m_wake.notify_one();
}

void RadeRxWorker::run()
{
    m_threadId.store(QThread::currentThreadId(), std::memory_order_release);
    // In nereusd on Linux the decoder runs on the least busy fast core
    // (raised priority), not on the housekeeping cores with spectrum and
    // networking. Elsewhere this does nothing.
    const bool placed = ThreadPlacement::managesThreadPriority();
    if (placed) {
        ThreadPlacement::instance().registerCurrentThread(ThreadRole::RadeDecoder,
                                                          m_placementChannel);
    }
    RadeRxBridge& bridge = *m_bridge;
    RadeRxBridge::Header header;
    std::vector<float> iq;
    while (!m_stopping.load(std::memory_order_acquire)) {
        const quint32 seen = bridge.m_wake.load(std::memory_order_acquire);
        bool worked = false;
        while (!m_stopping.load(std::memory_order_acquire)
               && bridge.popInput(header, iq)) {
            worked = true;
            if (!m_gated.load(std::memory_order_acquire)) {
#ifdef NEREUS_BUILD_TESTS
                std::function<void()> hook;
                {
                    std::lock_guard<std::mutex> lock(m_hookMutex);
                    hook = m_beforeDecodeHook;
                }
                if (hook) {
                    hook();
                }
#endif
                const QByteArray in = QByteArray::fromRawData(
                    reinterpret_cast<const char*>(iq.data()),
                    int(iq.size() * sizeof(float)));
                const QByteArray speech = m_decode ? m_decode(in) : QByteArray();
                const int frames =
                    int(speech.size() / qsizetype(2 * sizeof(float)));
                if (frames > 0) {
                    bridge.pushOutput(header,
                                      reinterpret_cast<const float*>(speech.constData()),
                                      frames);
                }
            }
            bridge.m_processed.fetch_add(1, std::memory_order_release);
        }
        if (worked) {
            {
                std::lock_guard<std::mutex> lock(m_idleMutex);
            }
            m_idle.notify_all();
        }
        if (m_stopping.load(std::memory_order_acquire)) {
            break;
        }
        bridge.m_wake.wait(seen, std::memory_order_acquire);
    }
    if (placed) {
        ThreadPlacement::instance().deregisterCurrentThread();
    }
}

#ifdef NEREUS_BUILD_TESTS
void RadeRxWorker::setBeforeDecodeHookForTest(std::function<void()> hook)
{
    std::lock_guard<std::mutex> lock(m_hookMutex);
    m_beforeDecodeHook = std::move(hook);
}

bool RadeRxWorker::waitIdleForTest(int timeoutMs)
{
    const quint64 target = m_bridge->m_pushed.load(std::memory_order_acquire);
    std::unique_lock<std::mutex> lock(m_idleMutex);
    return m_idle.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
        return m_bridge->m_processed.load(std::memory_order_acquire) >= target;
    });
}
#endif

}  // namespace NereusSDR
