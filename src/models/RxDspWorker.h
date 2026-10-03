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
//   2026-09-21 — Slice-owned, generation-guarded RADE RX routing by J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 OpenAI Codex.
//   2026-09-23 - Receive input delay bound (processStampedIqBatch,
//                 inputDelayStats, R-R3-40) by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 NereusSDR-original; no Thetis counterpart. Later the same
//                 day: the external-diversity legs are stamped and bounded
//                 too (processStampedExternalDiversityIqBatch).
//   2026-09-30 - RADE threads: every RADE slice has its own route
//                 (setRadeRxRoute) to its channel's decoder thread through
//                 lock-free rings; the DSP thread feeds each block and plays
//                 each slice's decoded block exactly radeLateBoundBlocks()
//                 later, or silence when it is not back, so a late decoder
//                 never holds the mixer. Replaces the single RADE binding and
//                 its main-thread hop (radeIqReady, routeRadeSpeech).
//                 NereusSDR-original. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-30 - RADE gaps: a slice in RADE mode that has no route on this
//                 worker yet (the blocks before the queued route lands, a
//                 new worker before the replay, a decoder that could not be
//                 made) plays silence, not its WDSP sideband. RadioModel
//                 publishes the RADE-mode slices as one atomic bit mask
//                 (setRadeModeSlices); the DSP thread only loads it.
//                 Fix round 1: the mask width is kRadeModeMaskSlices.
//                 NereusSDR-original. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-10-01 - Diversity lane: externalDiversityStreamSlices and
//                 processSliceChunk, so every slice on the diversity target's
//                 stream is fed from the mix. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
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

#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include <QObject>
#include <QVector>

namespace NereusSDR {

class WdspEngine;
class AudioEngine;
class Resampler;
class RadeRxBridge;
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
// Qt::QueuedConnection from ReceiverManager::iqDataForReceiverStamped.
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
    //
    // Phase 3F Sub-Epic I closeout, defect H1: `inSize` here is the DEFAULT
    // for streams that have not been given one of their own. A stream whose
    // DDC runs at its own rate carries its own drain size through
    // setStreamInputChunk() and ignores this value. `outSize` stays global:
    // every WDSP RX channel decimates input_rate -> 48 kHz and hands back
    // 64 samples per call regardless of its input rate, so there is nothing
    // per-stream about it.
    void setBufferSizes(int inSize, int outSize);

    // Configure the post-decimation panel sample rate (Hz, default 48 kHz).
    // Used as the rate component of the bufferSizesChanged() emission.
    // The WDSP RX channel decimates input_rate → 48 kHz internally, so
    // 48000.0 is the panel-side rate seen by AudioEngine and DEXP.
    void setSampleRate(double rate);

    // ── Receive input delay bound (R-R3-40, NereusSDR-original) ─────────
    //
    // Each queued batch carries the time it left ReceiverManager. When the
    // wait between that time and processing grows past the limit, the
    // worker skips that receiver's batches, without processing them, until
    // the wait falls below the resume level, so a DSP chain that cannot keep
    // up loses input instead of falling ever further behind. One log line
    // per episode says how much input was skipped.
    static constexpr qint64 kDspInputDelayLimitMs  = 500;
    static constexpr qint64 kDspInputDelayResumeMs = 250;
    // Receivers (logical stream indices) tracked by the bound; a batch for
    // an index outside [0, kMaxInputDelayReceivers) is processed unbounded.
    static constexpr int kMaxInputDelayReceivers = 32;

    struct InputDelayStats {
        qint64 inputDelayMs{0};    // wait of the receiver's latest batch
        qint64 droppedInputMs{0};  // input skipped since this worker started
    };

    // Safe from any thread (atomics). Zeroes for an untracked index.
    InputDelayStats inputDelayStats(int receiverIndex) const;

    // The external-diversity route's input (both hardware legs, stamped and
    // bounded the same way; skipped input is counted once, on the primary
    // leg). Safe from any thread (atomics).
    InputDelayStats externalDiversityInputDelayStats() const;

    // The input a slice actually processes: the external-diversity route's
    // when the slice is that route's target, otherwise its stream's
    // (inputDelayStats(streamIndex); zeroes for streamIndex < 0). Safe from
    // any thread (atomics).
    InputDelayStats inputDelayStatsForSlice(int sliceId, int streamIndex) const;

    int inSize() const { return m_inSize.load(std::memory_order_relaxed); }
    int outSize() const { return m_outSize.load(std::memory_order_relaxed); }
    double sampleRate() const { return m_sampleRate; }
    static constexpr int kMaxSaneExternalDiversityChunk = 65536;

#ifdef NEREUS_BUILD_TESTS
    // Test-only: sleep this long at the start of every processed (not
    // skipped) batch, to simulate a DSP chain slower than real time.
    void setProcessingDelayUsForTest(int microseconds)
    {
        m_processingDelayUsForTest.store(microseconds, std::memory_order_relaxed);
    }

    using ExternalDiversityOutputHookForTest =
        void (*)(int targetSlice, const float* i, const float* q, int samples);
    using ExternalDiversityRouteHookForTest =
        void (*)(bool active, int targetSlice,
                 int primarySource, int secondarySource);
    void setExternalDiversityOutputHookForTest(
        ExternalDiversityOutputHookForTest hook)
    {
        m_externalDiversityOutputHookForTest = hook;
    }
    void setExternalDiversityRouteHookForTest(
        ExternalDiversityRouteHookForTest hook)
    {
        m_externalDiversityRouteHookForTest = hook;
    }
#endif

public slots:
    // Receive a batch of interleaved I/Q from ReceiverManager. Runs
    // on m_dspThread via Qt::QueuedConnection. Accumulates samples
    // into in_size chunks, hands each chunk to RxChannel::processIq,
    // and forwards the decoded audio to AudioEngine.
    void processIqBatch(int receiverIndex,
                        const QVector<float>& interleavedIQ);

    // Production entry (R-R3-40): the same batch with the
    // ReceiverManager::enqueueClockNs() time it was queued. Applies the
    // input delay bound above, then hands the batch to processIqBatch or
    // skips it. batchProcessed fires for a skipped batch too.
    void processStampedIqBatch(int receiverIndex,
                               const QVector<float>& interleavedIQ,
                               qint64 enqueuedNs);

    /// Feed one raw hardware-DDC stream into the paired diversity route.
    ///
    /// This is deliberately separate from processIqBatch(): the primary DDC
    /// is also mapped by ReceiverManager to a logical stream for ordinary
    /// co-hosted slices, while the synced partner DDC has no logical receiver.
    /// A dedicated raw path therefore supplies each diversity leg exactly
    /// once without changing the normal logical-stream fan-out.
    void processExternalDiversityIqBatch(
        int sourceStream, const QVector<float>& interleavedIQ);

    /// Production entry for the diversity legs (R-R3-40): the same batch
    /// with the ReceiverManager::enqueueClockNs() time it was queued. Applies
    /// the input delay bound (kDspInputDelayLimitMs / kDspInputDelayResumeMs)
    /// to the route as a whole: an episode clears both legs' queued samples
    /// and skips both legs' batches; on resume the leg that lost less input
    /// drops the difference, so the legs stay sample-aligned.
    void processStampedExternalDiversityIqBatch(
        int sourceStream, const QVector<float>& interleavedIQ,
        qint64 enqueuedNs);

    /// Select the two raw source streams and the stable target slice for one
    /// WdspEngine external-diversity slot. Runs on the DSP thread through a
    /// queued/blocking invocation from RadioModel.
    void setExternalDiversityRoute(int extDivId, int targetSliceId,
                                   int primaryStream, int secondaryStream);

    /// Disable the worker route and flush both unmatched source queues.
    void clearExternalDiversityRoute();

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

    /// Give one DDC stream its own accumulator drain size, overriding the
    /// global setBufferSizes() value for that stream alone. `inSize <= 0`
    /// drops the override and returns the stream to the global default.
    ///
    /// Phase 3F Sub-Epic I closeout, defect H1. The drain size is a property
    /// of the DDC stream's sample rate, not of the radio: ChannelMaster keys
    /// it per input stream too.
    ///
    ///   From Thetis cmaster.c:461 [v2.10.3.15] (SetXcmInrate):
    ///     pcm->xcm_insize[in_id] = getbuffsize (rate);
    ///
    /// with getbuffsize(rate) = 64 * rate / 48000 (cmsetup.c:106-111
    /// [v2.10.3.15]). Callers must compute `inSize` with
    /// SampleRateCatalog's bufferSizeForRate(), which is that formula and
    /// the only copy of it in the tree. RxDspWorker stores what it is told
    /// and never recomputes.
    ///
    /// Changing a stream's size DROPS that stream's partial accumulator. The
    /// samples already in it were captured by the DDC at the old rate, and a
    /// WDSP channel carries exactly one input rate (SetInputSamplerate,
    /// channel.c:197-208), so a chunk straddling the change is demodulated
    /// against the wrong timebase end to end rather than merely clicking at
    /// the seam. The loss is bounded by one drain interval, which the Thetis
    /// formula fixes at inSize / rate = 64 / 48000 seconds at every rate.
    /// An idempotent re-push of the same size keeps the partial.
    ///
    /// Cross-thread queued slot, same contract as setStreamSlices: written
    /// from the main thread, only ever touched on the DSP thread.
    void setStreamInputChunk(int streamIndex, int inSize);

    /// Drop every per-stream drain-size override, returning all streams to
    /// the global setBufferSizes() value, and clear their accumulators.
    ///
    /// Phase 3F Sub-Epic I closeout, defect H1: paired with setBufferSizes()
    /// whenever the connection-wide geometry moves (RadioModel::
    /// setSampleRateLive), because overrides published against the previous
    /// wire rate would otherwise outlive it and leave a stream draining a
    /// chunk size no channel is configured for.
    void clearStreamInputChunks();

public:
    // RADE threads (2026-09-30). Route one slice's decoded WDSP audio to its
    // RADE decoder thread, or (bridge null) back to the speakers. DSP thread
    // only: RadioModel queues it there. Any number of slices may be routed
    // at once. Each install starts a new epoch on the bridge, so a record
    // left from an earlier route never fills one of this route's slots.
    void setRadeRxRoute(int sliceId, std::shared_ptr<RadeRxBridge> bridge);
    // DSP thread: drop every RADE route (the worker is being detached).
    void clearRadeRxRoutes();
    // Any thread: how many slices are routed to RADE.
    int radeRxRouteCount() const { return m_radeRxRouteCount.load(std::memory_order_acquire); }
    // RADE gaps (2026-09-30). Any thread: the slices in RADE mode, bit n for
    // slice id n (RadioModel::publishRadeModeSlices). A RADE-mode slice with
    // no route here plays silence, never its sideband: freedv-gui plays only
    // decoded speech in RADE (RADEReceiveStep.cpp:196-270 [@77e793a]) and
    // the demodulated audio only when the operator picks Analog
    // (TxRxThread.cpp:483-495 [@77e793a]). Lock-free: one atomic store here,
    // one atomic load per slice block on the DSP thread.
    // Slice ids the mask can carry: one bit each of a quint32.
    static constexpr int kRadeModeMaskSlices = 32;
    void setRadeModeSlices(quint32 mask)
    {
        m_radeModeSlices.store(mask, std::memory_order_release);
    }
    quint32 radeModeSlices() const { return m_radeModeSlices.load(std::memory_order_acquire); }

public slots:

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

    // (Phase 3M-3a-iv added antiVoxSampleReady here: one slice-0 audio block
    //  per drain interval, forked to TxWorkerThread::onAntiVoxSamplesReady to
    //  feed WDSP DEXP. Phase 3F Sub-Epic J Task 9 retired it. The anti-VOX
    //  reference is now AudioEngine::m_antiVoxMix, a second MasterMixer
    //  summing every audible slice, matching Thetis cmaster.c:371-372
    //  [v2.10.3.15]; AudioEngine::antiVoxBlockReady carries its drained
    //  block. bufferSizesChanged above is still this class's contribution to
    //  the chain, and still sets DEXP's block geometry. The retired feed's
    //  cadence argument is preserved at the bottom of the drain loop in
    //  RxDspWorker.cpp.)

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

    // Phase 3F Sub-Epic I closeout, defect H1: per-stream drain size, keyed
    // by DDC stream index. Absent = that stream follows the global m_inSize,
    // which is what every stream does on a single-rate radio, so the
    // single-rate path never consults this map at all.
    //
    // Mirrors ChannelMaster's `pcm->xcm_insize[in_id]` (cmaster.c:461
    // [v2.10.3.15]): one buffer size per input stream, derived from that
    // stream's own rate. Same threading contract as m_streamSlices: written
    // through the queued setStreamInputChunk slot, so DSP-thread-only at
    // point of use and no lock is needed. Deliberately NOT std::atomic like
    // m_inSize: an atomic would only protect one scalar, and what has to
    // stay coherent here is the (size, accumulator) pair, which only the DSP
    // thread ever touches.
    std::unordered_map<int, int>          m_streamInSize;

    // ── Phase 3F Sub-Epic A: paired external-diversity route ──────────────
    //
    // WDSP external diversity consumes two equally sized interleaved-double
    // buffers. Radio packets can arrive at different times and in different
    // chunk sizes, so each raw DDC leg has a separate bounded accumulator.
    // Route changes clear the queues and pre-size every scratch vector on the
    // DSP control path; steady-state drains then perform no heap allocation.
    struct ExternalDiversityRoute {
        int extDivId{-1};
        int targetSliceId{-1};
        int primaryStream{-1};
        int secondaryStream{-1};

        bool active() const noexcept
        {
            return extDivId >= 0 && targetSliceId >= 0
                && primaryStream >= 0 && secondaryStream >= 0
                && primaryStream != secondaryStream;
        }
    };

    static constexpr int kMaxSaneSamplesPerBatch =
        kMaxSaneExternalDiversityChunk;

    ExternalDiversityRoute m_externalDiversityRoute;
    StreamAccum m_externalDiversityPrimary;
    StreamAccum m_externalDiversitySecondary;
    int m_externalDiversityChunkSize{0};
    int m_externalDiversityMaxQueuedSamples{0};
    QVector<double> m_externalDiversityPrimaryInterleaved;
    QVector<double> m_externalDiversitySecondaryInterleaved;
    QVector<double> m_externalDiversityOutputInterleaved;
    QVector<float> m_externalDiversityOutputI;
    QVector<float> m_externalDiversityOutputQ;

#ifdef NEREUS_BUILD_TESTS
    ExternalDiversityOutputHookForTest
        m_externalDiversityOutputHookForTest{nullptr};
    ExternalDiversityRouteHookForTest
        m_externalDiversityRouteHookForTest{nullptr};
#endif

    // ── Receive input delay bound (R-R3-40) ────────────────────────────
    // delayUs / droppedUs are written on the DSP thread and read from any
    // thread; skipping / episodeSkippedUs are DSP-thread only.
    struct InputDelayState {
        std::atomic<qint64> delayUs{0};
        std::atomic<qint64> droppedUs{0};
        bool   skipping{false};
        qint64 episodeSkippedUs{0};
    };
    std::array<InputDelayState, kMaxInputDelayReceivers> m_inputDelay;

    // The external-diversity route's bound (R-R3-40): the same state, plus
    // how far each leg's input advanced (samples cleared or skipped) during
    // the current episode, and how many leading samples each leg still has
    // to drop after it to stay aligned with the other. DSP thread only,
    // except the InputDelayState atomics and the stats slice.
    InputDelayState m_externalDiversityInputDelay;
    qint64 m_externalDiversityEpisodeAdvance[2]{0, 0};
    qint64 m_externalDiversityDropPending[2]{0, 0};
    // Target slice of the active route for inputDelayStatsForSlice, -1 when
    // no route is active. Written on the DSP thread, read from any thread.
    std::atomic<int> m_externalDiversityStatsSlice{-1};

    // Wall-clock span of `samples` input samples on this stream, from its
    // drain size (inSize = 64 * rate / 48000, see setStreamInputChunk).
    qint64 inputSpanUs(int receiverIndex, qint64 samples) const;
    // The same rule for a known drain size.
    static qint64 spanUsForDrainSize(qint64 inSize, qint64 samples);
    // Adds primary-leg samples lost in the current diversity episode.
    void countDiversityPrimaryLoss(qint64 drainSize, qint64 samples);

#ifdef NEREUS_BUILD_TESTS
    std::atomic<int> m_processingDelayUsForTest{0};
#endif

    int externalDiversityChunkSize() const;
    void prepareExternalDiversityBuffers(int chunkSize);
    // Appends the batch after dropping its first skipLeading samples.
    void appendExternalDiversitySamples(StreamAccum& destination,
                                        const QVector<float>& interleavedIQ,
                                        int skipLeading = 0);
    void drainExternalDiversity();
    void feedExternalDiversityTarget(int samples);
    bool isExternalDiversityTarget(int sliceId) const noexcept;
    // The slices bound to the stream that hosts the diversity target (the
    // target included), or empty when no route is active or the target is
    // bound to no stream. Every one of them reads the mixed buffer, as every
    // sub-receiver of Thetis's stream 0 does.
    QVector<int> externalDiversityStreamSlices() const;

    // One slice's share of a drained chunk: its WDSP channel, the RADE fork
    // and the audio push, then sliceProcessed. The input legs are blanked in
    // place unless blankerClaimed (the stream's single blanking pass already
    // ran on them). Returns whether the slice reached processIq, which is
    // what claims the pass. Engines must be wired.
    bool processSliceChunk(int sliceIdx, float* inI, float* inQ,
                           int inSize, int outSize, bool blankerClaimed);

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

    // Phase 3R K-bench: RADE RX path, worker-thread-owned. A routed slice
    // goes through the decimator to its decoder, and only that slice's
    // ordinary audio is replaced. All other slices continue through
    // AudioEngine.
    //
    // RADE threads (2026-09-30): one route per RADE slice, each with its
    // own resamplers so no slice's filter state reaches another's.
    struct RadeRxRoute {
        std::shared_ptr<RadeRxBridge> bridge;
        quint32 epoch{0};
        quint32 seq{0};  // this route's block counter
        int fedBlocks{0};  // blocks fed so far, up to the late bound
        // 48 -> 24 kHz for the codec's input (was m_radeRxDownsamplerI), and
        // 24 -> 48 kHz for its speech, per leg (was RadioModel's
        // m_radeRxSpeechL / R).
        std::unique_ptr<Resampler> down;
        std::unique_ptr<Resampler> upL;
        std::unique_ptr<Resampler> upR;
        // 48 kHz stereo speech waiting to play; each block takes exactly
        // outSize frames from it, zero-padded when short.
        std::vector<float> play;
    };
    // Routes one block of a RADE slice and delivers that slice's audio for
    // the block. Returns false when the slice has no route.
    bool processRadeRxBlock(int sliceIdx, const float* audio48k, int outSize);
    // Sizes the RADE scratch below once, with a route, so the block path
    // never grows it (fix wave: no allocation on the DSP thread's blocks).
    void reserveRadeRxScratch();
    // The most 48 kHz frames one RADE block's resamplers take or give (the
    // resamplers' own maxBlockSamples, as before), and the 48 kHz stereo
    // speech a route holds: at most four blocks kept plus one arriving.
    static constexpr int kRadeRxMaxBlockFrames = 4096;
    static constexpr size_t kRadeRxPlayReserveFloats = size_t(6) * kRadeRxMaxBlockFrames * 2;

    std::unordered_map<int, RadeRxRoute> m_radeRxRoutes;
    std::atomic<int>            m_radeRxRouteCount{0};
    // RADE gaps: see setRadeModeSlices.
    std::atomic<quint32>        m_radeModeSlices{0};
    // Scratch for the (audio, 0) pairs presented to the codec as
    // interleaved stereo float32 at 24 kHz (RadeChannel::processIq's input
    // convention), and for the speech coming back.
    std::vector<float>          m_radeRxIqScratch;
    std::vector<float>          m_radeRxDueScratch;
    std::vector<float>          m_radeRxLegL;
    std::vector<float>          m_radeRxLegR;
    std::vector<float>          m_radeRxOutScratch;
    // Resampler output: 24 kHz codec input, and each 48 kHz speech leg.
    std::vector<float>          m_radeRxDown;
    std::vector<float>          m_radeRxUpL;
    std::vector<float>          m_radeRxUpR;
};

} // namespace NereusSDR
