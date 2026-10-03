// no-port-check: NereusSDR-original. AetherSDR and freedv-gui are cited for
// structure only; no upstream code is ported.
// =================================================================
// src/core/RadeRxWorker.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Each RADE receive decoder runs on its own thread,
// fed by the DSP thread and read back by it, with no main-thread hop.
//
// Structure followed (no code ported, so no upstream header applies):
//   - AetherSDR runs its RADEEngine on a dedicated worker QThread named
//     after the engine (src/gui/MainWindow_DigitalModes.cpp:375-380
//     [@1e0718ad]).
//   - freedv-gui feeds rade_rx from its own TX/RX pipeline thread through
//     FIFOs: a full input FIFO drops the block and counts it
//     (src/main.cpp:4084-4087 [@a4ae053]), the pipeline thread writes its
//     speech to the output FIFO and drops what does not fit
//     (src/pipeline/TxRxThread.cpp:914-938 [@a4ae053]), and the sound
//     card callback plays whatever the output FIFO holds and zeros for the
//     rest, never waiting (src/main.cpp:3565-3580 [@a4ae053]).
//
// NereusSDR divergence from freedv-gui: the output is not "whatever the
// FIFO holds". Every block carries the DSP thread's block sequence number,
// and the DSP thread plays block n exactly radeLateBoundBlocks() blocks
// after it fed it. A block that is not back by then is silence for that
// slot, and when it does arrive it is dropped, so a late decoder can never
// build up latency or hold the mixer (JJ's ruling 2, 2026-09-30).
//
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd (KG4VCF)  Created for the RADE threads lane.
//   2026-09-30  J.J. Boyd (KG4VCF)  Fix wave: the input push scratch is
//                                    sized for the largest record when the
//                                    bridge is made. AI-assisted via
//                                    Anthropic Claude Code.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioRingSpsc.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class QThread;

namespace NereusSDR {

// From third_party/rade/src/rade_rx.c:123-125 and
// third_party/rade/src/rade_dsp.h:64-70 [b289102]:
//   int rade_rx_nin_max(const rade_rx_state *rx) { return RADE_NMF + RADE_M; }
//   #define RADE_M    160   /* Samples per OFDM symbol (Fs/Rs') */
//   #define RADE_NMF  ((RADE_NS+1)*(RADE_M+RADE_NCP))  /* Samples per modem frame = 960 */
// The most modem samples one rade_rx call consumes, at
// RADE_MODEM_SAMPLE_RATE 8000 (rade_api.h:84): 140 ms.
constexpr int kRadeNinMax8k = 1120;
constexpr int kRadeModemRate = 8000;
// The mixer's own rate (MasterMixer, kBufferBaseRate).
constexpr int kRadeMixerRate = 48000;

// How many of the DSP thread's blocks a RADE slice's decoded audio may
// trail the block it was decoded from before that slot plays silence.
//
// The bound is one rade_rx call's worth of input, rade_nin_max(): a
// decoder that keeps real time finishes each call before the next call's
// input has arrived, so no block it decodes is ever later than that. Any
// smaller bound would silence a decoder that is keeping up; anything larger
// only adds delay. Counted in the mixer's own blocks, never wall time:
// 1120 modem samples = 6720 frames at 48 kHz = 105 blocks of 64.
inline int radeLateBoundBlocks(int blockFrames48k)
{
    const int frames = kRadeNinMax8k * (kRadeMixerRate / kRadeModemRate);
    const int block = blockFrames48k > 0 ? blockFrames48k : 64;
    return (frames + block - 1) / block;
}

// The two lock-free rings between the DSP thread and one decoder thread,
// shared (std::shared_ptr) by both so neither side can outlive the memory.
//
// Records in both rings are a Header and then `frames` interleaved stereo
// float pairs at 24 kHz. Input: the (audio, 0) pairs RadeChannel::processIq
// takes. Output: the stereo speech it returns.
class RadeRxBridge {
public:
    struct Header {
        quint32 epoch{0};
        quint32 seq{0};
        qint32 frames{0};
    };

    // About 120 input blocks (one rade_nin_max of 24 kHz audio is 105 of
    // them); a decoder further behind than this loses input, not time.
    static constexpr size_t kInputRingBytes = 32768;
    // The output waits here for its slot, up to radeLateBoundBlocks().
    static constexpr size_t kOutputRingBytes = 65536;
    static constexpr int kMaxRecordFrames = 2048;
    // One record: its header and kMaxRecordFrames stereo frames, in floats.
    static constexpr size_t kMaxRecordFloats =
        (sizeof(Header) + size_t(kMaxRecordFrames) * 2 * sizeof(float) + sizeof(float) - 1)
        / sizeof(float);

    // ── DSP thread (the only producer of input, only consumer of output) ──
    //
    // Queue one input block. Never waits: a full ring, or a second producer
    // racing this one, drops the block and counts it.
    bool pushInput(quint32 epoch, quint32 seq, const float* iq, int frames);

    // The record for block `due` of `epoch`, copied into `out` (frames * 2
    // floats); returns its frame count. Records of an older block or
    // another epoch are dropped on the way (a late block). Returns 0 when
    // the due record is not there, which is silence for that slot.
    int takeDue(quint32 epoch, quint32 due, std::vector<float>& out);

    // ── Decoder thread ────────────────────────────────────────────────────
    bool popInput(Header& header, std::vector<float>& iq);
    bool pushOutput(const Header& header, const float* speech, int frames);

    // The epoch the DSP thread currently tags its records with. A new route
    // (a new DSP worker, or the same one re-bound) takes the next one, so a
    // record queued under the old one can never fill a new slot.
    quint32 nextEpoch() { return m_epoch.fetch_add(1, std::memory_order_acq_rel) + 1; }

    // Counters, readable from any thread.
    quint64 inputDrops() const { return m_inputDrops.load(std::memory_order_relaxed); }
    quint64 outputDrops() const { return m_outputDrops.load(std::memory_order_relaxed); }
    quint64 lateDrops() const { return m_lateDrops.load(std::memory_order_relaxed); }
    quint64 silentSlots() const { return m_silentSlots.load(std::memory_order_relaxed); }
    quint64 playedSlots() const { return m_playedSlots.load(std::memory_order_relaxed); }
    void countSlot(bool played)
    {
        (played ? m_playedSlots : m_silentSlots).fetch_add(1, std::memory_order_relaxed);
    }

private:
    friend class RadeRxWorker;

    bool readRecord(AudioRingSpsc<kOutputRingBytes>& ring, Header& header,
                    std::vector<float>& data);

    AudioRingSpsc<kInputRingBytes> m_input;
    AudioRingSpsc<kOutputRingBytes> m_output;
    // Never-waiting single-owner guards: a second DSP worker that still has
    // this bridge (a worker being replaced) drops instead of racing the ring.
    std::atomic_flag m_inputProducer = ATOMIC_FLAG_INIT;
    std::atomic_flag m_outputConsumer = ATOMIC_FLAG_INIT;
    // Guarded by m_inputProducer. Sized for the largest record at
    // construction, so pushInput on the DSP thread never grows it.
    std::vector<float> m_pushScratch = std::vector<float>(kMaxRecordFloats);

    std::atomic<quint32> m_epoch{0};
    std::atomic<quint32> m_wake{0};
    std::atomic<quint64> m_pushed{0};
    std::atomic<quint64> m_processed{0};
    std::atomic<quint64> m_inputDrops{0};
    std::atomic<quint64> m_outputDrops{0};
    std::atomic<quint64> m_lateDrops{0};
    std::atomic<quint64> m_silentSlots{0};
    std::atomic<quint64> m_playedSlots{0};
};

// One decoder thread. Owned by its RadeChannel, which stops it before any
// of the codec state it runs is torn down.
class RadeRxWorker {
public:
    // Runs one input block through the codec on the decoder thread and
    // returns the speech (24 kHz stereo float), or an empty array.
    using DecodeFn = std::function<QByteArray(const QByteArray& iq)>;

    explicit RadeRxWorker(DecodeFn decode);
    ~RadeRxWorker();

    RadeRxWorker(const RadeRxWorker&) = delete;
    RadeRxWorker& operator=(const RadeRxWorker&) = delete;

    std::shared_ptr<RadeRxBridge> bridge() const { return m_bridge; }

    // Start the thread (idempotent). `name` is the thread's name as the OS
    // shows it (top -H), for example "RadeRx1". `placementChannel` is the
    // channel it registers under with ThreadPlacement (RadeDecoder role).
    void start(const QString& name, int placementChannel = -1);
    // Stop and join. Waits for the block being decoded, if any.
    void stop();
    bool isRunning() const;
    // The thread's name, empty when not running.
    QString threadName() const { return m_threadName; }

    // Input is taken but not decoded while gated (the transmitting slice's
    // decoder during MOX). Any thread.
    void setGated(bool gated) { m_gated.store(gated, std::memory_order_release); }
    bool gated() const { return m_gated.load(std::memory_order_acquire); }

    // Wake the thread: the DSP thread calls this after pushInput.
    static void wake(RadeRxBridge& bridge);

#ifdef NEREUS_BUILD_TESTS
    // Runs on the decoder thread before each block is decoded. A test that
    // blocks here stalls the decoder.
    void setBeforeDecodeHookForTest(std::function<void()> hook);
    // Until every block pushed so far has been taken off the input ring.
    bool waitIdleForTest(int timeoutMs);
    Qt::HANDLE threadIdForTest() const { return m_threadId.load(std::memory_order_acquire); }
#endif

private:
    void run();

    DecodeFn m_decode;
    std::shared_ptr<RadeRxBridge> m_bridge;
    std::unique_ptr<QThread> m_thread;
    QString m_threadName;
    int m_placementChannel{-1};
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_gated{false};
    std::atomic<Qt::HANDLE> m_threadId{nullptr};
    std::mutex m_idleMutex;  // decoder thread notifies test waiters only
    std::condition_variable m_idle;
    // Test seam state (setBeforeDecodeHookForTest). Present in every build
    // so the class has one layout whatever defines a unit sees.
    std::mutex m_hookMutex;
    std::function<void()> m_beforeDecodeHook;
};

}  // namespace NereusSDR
