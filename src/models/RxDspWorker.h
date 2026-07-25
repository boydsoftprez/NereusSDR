#pragma once

// =================================================================
// src/models/RxDspWorker.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include <atomic>
#include <memory>
#include <unordered_map>

#include <QObject>
#include <QVector>

namespace NereusSDR {

class WdspEngine;
class AudioEngine;
class RadeChannel;
class Resampler;
struct AudioPriorityToken;   // src/core/audio/RealtimeAudioPriority.h

// RxDspWorker runs the per-receiver I/Q → WDSP → audio processing step
// on a dedicated DSP thread, off the GUI main thread.
//
// Why: WDSP's fexchange2() (called via RxChannel::processIq) is opened
// with bfo=1, which makes it block on Sem_OutReady whenever the WDSP
// channel's internal DSP loop hasn't replenished its output ring. When
// that call ran on the GUI main thread it produced a deterministic
// two-way deadlock: fexchange2 waited on Sem_OutReady, the WDSP worker
// (wdspmain) waited on Sem_BuffReady, and because the main thread was
// blocked the Qt event loop stopped delivering more I/Q events to feed
// fexchange2 — so wdspmain never received another input batch and never
// signalled Sem_OutReady. Moving fexchange2 to its own thread keeps the
// blocking semantics local to that thread and lets the GUI event loop
// keep dispatching I/Q events.
//
// The worker is owned by RadioModel. It is constructed on the main
// thread, given non-owning WdspEngine/AudioEngine pointers via
// setEngines(), moved to RadioModel::m_dspThread, then driven by a
// Qt::QueuedConnection from ReceiverManager::iqDataForReceiver.
class RxDspWorker : public QObject {
    Q_OBJECT

public:
    // Default buffer sizes match the historical hardcoded values that
    // were correct for a 768 kHz wire rate (Thetis formula
    // in_size = 64 * rate / 48000 → 1024 at 768 kHz, out_size always 64
    // because WDSP decimates input_rate → 48000 internally). Real call
    // sites override these via setBufferSizes() once the wire rate is
    // known; the defaults exist so test fixtures and code paths that
    // predate setBufferSizes() keep working.
    static constexpr int kDefaultInSize  = 1024;
    static constexpr int kDefaultOutSize = 64;

    explicit RxDspWorker(QObject* parent = nullptr);
    ~RxDspWorker() override;

    // Set non-owning engine pointers. Must be called on the main
    // thread before moveToThread(). The engines must outlive this
    // worker — RadioModel guarantees this by tearing the worker down
    // before destroying the engines.
    void setEngines(WdspEngine* wdsp, AudioEngine* audio);

    // Configure the per-rate accumulator drain size. Must match the
    // in_size and out_size that RadioModel passed to
    // WdspEngine::createRxChannel for the same connect cycle, otherwise
    // fexchange2 receives the wrong number of samples per call and
    // produces glitchy / jittery audio output. The Thetis formula is
    // in_size = 64 * rate / 48000; out_size is always 64 in the current
    // RX path (input_rate → 48000 decimation). Safe to call from any
    // thread — m_inSize/m_outSize are std::atomic<int>, and
    // processIqBatch snapshots both at batch start so a concurrent
    // setBufferSizes() takes effect no earlier than the next batch.
    //
    // Phase 3M-3a-iv: also fires bufferSizesChanged(outSize, sampleRate)
    // when the (in, out) pair actually changes (idempotent re-calls are
    // suppressed). Consumed by TxWorkerThread::setAntiVoxBlockGeometry
    // to align WDSP DEXP detector dimensions with RX block geometry.
    void setBufferSizes(int inSize, int outSize);

    // Configure the post-decimation panel sample rate (Hz, default 48 kHz).
    // Used as the rate component of the bufferSizesChanged() emission.
    // The WDSP RX channel decimates input_rate → 48 kHz internally, so
    // 48000.0 is the panel-side rate seen by AudioEngine and DEXP.
    void setSampleRate(double rate);

    int inSize() const { return m_inSize.load(std::memory_order_relaxed); }
    int outSize() const { return m_outSize.load(std::memory_order_relaxed); }
    double sampleRate() const { return m_sampleRate; }

public slots:
    // Receive a batch of interleaved I/Q from ReceiverManager. Runs
    // on m_dspThread via Qt::QueuedConnection. Accumulates samples
    // into in_size chunks, hands each chunk to RxChannel::processIq,
    // and forwards the decoded audio to AudioEngine.
    void processIqBatch(int receiverIndex,
                        const QVector<float>& interleavedIQ);

    // Drop any partial accumulator state. Called from RadioModel
    // teardown via Qt::BlockingQueuedConnection so it executes on
    // the worker thread before the WDSP channel is destroyed.
    //
    // Phase 3F Sub-Epic I Task 4: clears every stream's partial I/Q.
    // Slice bindings (m_streamSlices) deliberately survive: both live
    // callers (RadioModel::setSampleRateLive / setActiveRxCountLive) are
    // mid-flight reconfigures that resume feeding the same slices, and
    // dropping the bindings here would silence every slice until
    // something republished them.
    void resetAccumulator();

    /// Declare which slice indices are bound to a DDC stream. Called from
    /// the main thread on every slice bind / unbind / migration; queued,
    /// so the map is only ever touched on the DSP thread.
    ///
    /// Phase 3F Sub-Epic I Task 4. A stream with no declared slices
    /// accumulates and drains but demodulates nothing.
    void setStreamSlices(int streamIndex, const QVector<int>& sliceIndices);

    // Phase 3R K-bench: set the active RadeChannel for I/Q routing.
    // When non-null AND WDSP rxChannel(0) returns null (slice is in
    // RADE mode), processIqBatch decimates each chunk to 24 kHz I/Q
    // and posts it to radeCh->processIq via Qt::QueuedConnection
    // (RadeChannel lives on the main thread). RadioModel pushes this
    // pointer from wireRadeChannel (set) and the channel's destroyed
    // signal (clear).
    //
    // Cross-thread queued slot. Atomic raw pointer write; ownership
    // remains with WdspEngine::m_radeChannels.
    void setRadeChannel(RadeChannel* channel);

    // 2026-05-25 KG4VCF bench fix: real-time scheduling priority for
    // audio DSP work.  Connected by RadioModel to m_dspThread's
    // started() / finished() signals so the elevation runs ON the
    // DSP thread (a prerequisite of pthread_set_qos_class_self_np
    // and os_workgroup_join, which both act on the calling thread).
    //
    // Without these, the DSP feeder runs at default Qt priority and
    // gets preempted during heavy system load (parallel compiles
    // etc), draining the audio ring buffer and producing severe
    // jitter on the listening end.  PortAudio's own callback thread
    // is unaffected (it runs SCHED_FIFO via PortAudio's internal
    // priority setup); this slot fixes the FEEDER thread.
    void onThreadStarted();
    void onThreadFinished();

signals:
    // Emitted at the end of every processIqBatch invocation, on the
    // DSP thread. Used by tests to observe that work happens off the
    // main thread without requiring a real WDSP build.
    void batchProcessed();

    // Emitted once per drained chunk, regardless of whether the WDSP
    // engine is wired. Carries the chunk's sample count so tests can
    // verify the drain size matches setBufferSizes(). Production code
    // does not need to listen to this — it exists for the regression
    // test that pins the per-rate accumulator-drain contract.
    void chunkDrained(int sampleCount);

    // Phase 3F Sub-Epic I Task 4: per-stream companion to chunkDrained,
    // carrying the originating DDC stream. chunkDrained is kept unchanged
    // for existing single-slice subscribers.
    void chunkDrainedForStream(int streamIndex, int samples);

    // Phase 3F Sub-Epic I Task 4: emitted once per slice per drained
    // chunk, after that slice's WDSP channel has run. Test seam for the
    // fan-out; fires even without engines wired, mirroring chunkDrained.
    void sliceProcessed(int sliceIndex, int samples);

    /// Emitted once per drained chunk when the stream's noise blanker pass
    /// runs. The blanker belongs to the DDC stream, not the slice
    /// (ChannelMaster cmaster.h:79-81 [v2.10.3.15]), so exactly one pass
    /// happens per chunk no matter how many slices share the stream.
    void streamNoiseBlankerApplied(int streamIndex);

    // Phase 3M-3a-iv: fires whenever setBufferSizes() actually changes
    // the (inSize, outSize) pair. Consumed by
    // TxWorkerThread::setAntiVoxBlockGeometry to align WDSP DEXP's
    // anti-VOX detector dimensions with the post-decimation RX block.
    //
    // Payload: (outSize_complexSamples, outRate_Hz).
    //
    // From Thetis ChannelMaster cmaster.c:159-175 [v2.10.3.13]: aamix is
    // configured with audio_outsize / audio_outrate, which in NereusSDR's
    // single-RX path correspond to RxDspWorker::outSize and the post-
    // decimation panel rate.
    void bufferSizesChanged(int outSize, double outRate);

    // Phase 3M-3a-iv: fires per drained chunk after the existing
    // chunkDrained / rxBlockReady delivery. Consumed by
    // TxWorkerThread::onAntiVoxSamplesReady to feed WDSP DEXP.
    //
    // Payload: (sliceId, interleaved L/R float buffer, sampleCount).
    //
    // From Thetis ChannelMaster cmaster.c:171 [v2.10.3.13]: aamix's
    // SendAntiVOXData callback delivers exactly one audio_outsize block
    // at audio_outrate. Single-RX equivalent here — no resample, no mix,
    // one source — replaces the aamix port with a direct queued signal.
    //
    // Fires regardless of WDSP/AudioEngine wiring, mirroring chunkDrained,
    // so tests without fake engines still observe the contract. When
    // engines are wired the buffer carries the WDSP-decoded interleaved
    // audio; when they are not, a zero-filled stereo buffer of the
    // correct size is emitted instead.
    //
    // Phase 3F Sub-Epic I closeout, defect G1: this is a per-RADIO feed,
    // NOT a per-stream one. Exactly one emission happens per drain
    // interval no matter how many DDC streams are draining, raised only by
    // the stream that hosts slice 0 (whose audio the buffer carries). Any
    // higher rate would overrun the single block geometry DEXP was given
    // through setAntiVoxBlockGeometry. Rationale and the dexp.c cites are
    // at the emit site in RxDspWorker.cpp.
    void antiVoxSampleReady(int sliceId, const QVector<float>& interleaved, int sampleCount);

    // Phase 3R K-bench: per-batch RADE feed.  Emitted from the DSP
    // thread with a 24 kHz interleaved-float32 I/Q buffer (real=audio,
    // imag=0) that mirrors the freedv-gui / AetherSDR RADE input
    // shape.  Connected to RadeChannel::processIq via
    // Qt::QueuedConnection inside setRadeChannel().
    //
    // Why a signal instead of QMetaObject::invokeMethod on a raw
    // pointer (the original K-bench shape):  invokeMethod(raw_ptr,
    // ..., Qt::QueuedConnection) packs the raw pointer into a
    // QMetaCallEvent posted to the target's thread; Qt does not
    // dis-arm those events when the target QObject is destroyed
    // out from under us, so a teardown that races the DSP thread
    // can deliver a queued slot call to a freed RadeChannel
    // (use-after-free).  Replacing the invoke with a connected
    // signal moves the lifetime contract into Qt's metaobject
    // system: ~QObject auto-disconnects and removePostedEvents
    // drops in-flight slot calls under a connection-list lock, so
    // a worker that emits during teardown is safe.
    //
    // (review finding 2026-05-12, PR #238 — P1 #3).
    void radeIqReady(QByteArray iq);

private:
    WdspEngine*      m_wdspEngine{nullptr};
    AudioEngine*     m_audioEngine{nullptr};

    // 2026-05-25 KG4VCF bench fix: opaque token returned by
    // elevateAudioThreadPriority().  Allocated on the DSP thread
    // from onThreadStarted() and released from onThreadFinished()
    // (also on the DSP thread, before the thread exits).  See
    // src/core/audio/RealtimeAudioPriority.h.
    AudioPriorityToken* m_audioPrioToken{nullptr};

    // ── Phase 3F Sub-Epic I Task 4: per-stream accumulation ─────────────
    //
    // Before this, one shared accumulator pair served every caller and
    // processIqBatch's receiverIndex argument was ignored, so a second
    // DDC's samples were appended into Slice A's stream and corrupted its
    // audio.
    //
    // Keyed by DDC stream index. Each stream's drained chunk is then fed
    // to every slice bound to that stream, mirroring ChannelMaster's
    // `struct _rcvr` which holds one I/Q input and one noise blanker but
    // `double* audio[cmMAXSubRcvr]` outputs (cmaster.h:74-82
    // [v2.10.3.15]).
    //
    // Touched only on the DSP thread, so no lock is needed. m_streamSlices
    // is written via a queued setStreamSlices call, which lands on the DSP
    // thread's event loop, so it is also DSP-thread-only at point of use.
    struct StreamAccum {
        QVector<float> i;
        QVector<float> q;
    };
    std::unordered_map<int, StreamAccum>  m_accums;
    std::unordered_map<int, QVector<int>> m_streamSlices;

    // Reusable interleaved stereo scratch handed to AudioEngine::rxBlockReady.
    // Sized to outSize*2 on first use and reused in-place per batch so the
    // DSP thread never allocates in the hot path after warmup.
    //
    // Phase 3F Sub-Epic I Task 4: m_interleavedOut is reserved for SLICE 0.
    // It doubles as the anti-VOX cancellation reference read after the
    // drain, so secondary slices must not clobber it; they interleave
    // through m_interleavedOutAux instead. AudioEngine::rxBlockReady
    // consumes the pointer synchronously (AudioEngine.cpp:928), so one
    // shared scratch per role is safe.
    QVector<float>   m_interleavedOut;
    QVector<float>   m_interleavedOutAux;

    // Phase 3F Sub-Epic I Task 4: reusable WDSP output scratch. Previously
    // two QVectors were constructed per drained chunk; with N slices per
    // chunk that would be 2N allocations on the audio path. Grown to
    // max(inSize, outSize) on demand and reused. RxChannel::processIq
    // writes sampleCount floats on the inactive-channel memset path
    // (RxChannel.cpp:1532) and outSampleCount via fexchange2, so the
    // buffer must cover the larger of the two.
    QVector<float>   m_sliceOutI;
    QVector<float>   m_sliceOutQ;
    // Written by setBufferSizes() (typically on the main thread when the
    // wire rate changes) and read by processIqBatch() on the DSP thread.
    // std::atomic<int> prevents the C++ data race that plain int reads
    // would exhibit. Relaxed ordering is sufficient: no other state is
    // published alongside these values.
    std::atomic<int> m_inSize{kDefaultInSize};
    std::atomic<int> m_outSize{kDefaultOutSize};

    // Phase 3M-3a-iv: post-decimation panel sample rate emitted as the
    // rate component of bufferSizesChanged(). 48 kHz matches the WDSP
    // RX channel's internal decimation target (input_rate → 48000).
    // Plain double, not atomic: only setBufferSizes() reads it, and
    // setSampleRate() / setBufferSizes() are both expected to run on
    // the configuring thread (typically main).
    double m_sampleRate{48000.0};

    // Phase 3M-3a-iv: tracks the last (in, out) pair emitted via
    // bufferSizesChanged so identical re-calls don't spam
    // TxWorkerThread::setAntiVoxBlockGeometry. -1 sentinel means "no
    // emission yet" — guarantees the first setBufferSizes() call always
    // fires regardless of whether it matches the kDefault* defaults.
    int m_lastEmittedInSize{-1};
    int m_lastEmittedOutSize{-1};

    // Phase 3R K-bench: RADE RX path. m_radeChannel is the active
    // RadeChannel for slice 0; when non-null AND m_wdspEngine has no
    // RxChannel for slice 0, processIqBatch routes I/Q through the
    // decimators below to RadeChannel::processIq instead of WDSP.
    //
    // The decimators run at the configured radio rate (m_sampleRate,
    // typically 48 / 96 / 192 kHz) and produce 24 kHz I/Q matching
    // RadeChannel's processIq expectation. Built lazily on first use
    // and rebuilt if m_sampleRate changes. Two parallel resamplers
    // (one per leg) so the I and Q channels stay aligned.
    std::atomic<RadeChannel*>   m_radeChannel{nullptr};
    std::unique_ptr<Resampler>  m_radeRxDownsamplerI;
    std::unique_ptr<Resampler>  m_radeRxDownsamplerQ;
    double                      m_radeRxDownsamplerSrcRate{0.0};
    // Scratch for the float-mono I/Q presented to RadeChannel::processIq
    // as interleaved stereo float32 at 24 kHz (matching RadeChannel's
    // input convention from RadeChannel::processIq).
    QByteArray                  m_radeRxIqScratch;
};

} // namespace NereusSDR
