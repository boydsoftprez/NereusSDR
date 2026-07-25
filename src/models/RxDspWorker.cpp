// =================================================================
// src/models/RxDspWorker.cpp  (NereusSDR)
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

#include "RxDspWorker.h"

#include "core/AudioEngine.h"
#include "core/LogCategories.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/RadeChannel.h"
#include "core/Resampler.h"
#include "core/audio/RealtimeAudioPriority.h"

namespace NereusSDR {

RxDspWorker::RxDspWorker(QObject* parent)
    : QObject(parent)
{
    // Phase 3F Sub-Epic I Task 4: stream 0 is the primary DDC and carries
    // Slice A. Seed its accumulator reservation (unchanged from the old
    // single-accumulator constructor) AND its slice binding, so the
    // single-stream / single-slice RX path keeps working unchanged before
    // RadioModel starts publishing bindings via setStreamSlices.
    // ReceiverManager forwards the primary DDC as logical receiver 0
    // (ReceiverManager.cpp:327), so this seed is exactly the binding the
    // allocator republishes later.
    StreamAccum& primary = m_accums[0];
    primary.i.reserve(kDefaultInSize * 2);
    primary.q.reserve(kDefaultInSize * 2);
    m_streamSlices[0] = QVector<int>{0};
}

RxDspWorker::~RxDspWorker() = default;

void RxDspWorker::onThreadStarted()
{
    // 2026-05-25 KG4VCF bench fix: real-time audio priority for the
    // DSP feeder.  Runs on the DSP thread (signal-emitted on this
    // thread by QThread::started after Qt has spun the thread up).
    // Calling on this thread is the requirement for both
    // pthread_set_qos_class_self_np and os_workgroup_join (macOS)
    // as well as pthread_setschedparam (Linux) and
    // AvSetMmThreadCharacteristics (Windows).
    if (m_audioPrioToken != nullptr) {
        // Defensive: started() should fire only once per thread
        // lifecycle.  If it fires twice (e.g. a future requeue path),
        // release the prior token before allocating a new one.
        leaveAudioThreadPriority(m_audioPrioToken);
    }
    m_audioPrioToken = elevateAudioThreadPriority();
}

void RxDspWorker::onThreadFinished()
{
    // Released on the DSP thread before it exits.  Required for
    // os_workgroup_leave to match the join on the correct thread.
    leaveAudioThreadPriority(m_audioPrioToken);
    m_audioPrioToken = nullptr;
}

void RxDspWorker::setEngines(WdspEngine* wdsp, AudioEngine* audio)
{
    m_wdspEngine  = wdsp;
    m_audioEngine = audio;
}

void RxDspWorker::setBufferSizes(int inSize, int outSize)
{
    m_inSize.store(inSize, std::memory_order_relaxed);
    m_outSize.store(outSize, std::memory_order_relaxed);

    // Phase 3M-3a-iv: emit only on change so steady-state operation does
    // not spam TxWorkerThread::setAntiVoxBlockGeometry with no-op
    // SetAntiVOXSize/SetAntiVOXRate calls. Sentinel m_lastEmittedInSize=-1
    // guarantees the first call always fires, even if it matches the
    // kDefault* defaults that m_inSize/m_outSize started at.
    if (inSize != m_lastEmittedInSize || outSize != m_lastEmittedOutSize) {
        m_lastEmittedInSize  = inSize;
        m_lastEmittedOutSize = outSize;
        emit bufferSizesChanged(outSize, m_sampleRate);
    }
}

void RxDspWorker::setSampleRate(double rate)
{
    m_sampleRate = rate;
}

void RxDspWorker::setRadeChannel(RadeChannel* channel)
{
    // Disconnect the previous receiver before swapping the pointer.
    // Qt's queued-connection delivery is safe across QObject
    // destruction on its own (~QObject + removePostedEvents handle
    // pending calls under the connection-list lock), but an explicit
    // disconnect tightens the contract and avoids stale slot calls
    // if the previous channel outlives this swap.
    if (auto* prev = m_radeChannel.load(std::memory_order_acquire)) {
        disconnect(this, &RxDspWorker::radeIqReady,
                   prev, &RadeChannel::processIq);
    }

    m_radeChannel.store(channel, std::memory_order_release);

    if (channel != nullptr) {
        // Cross-thread queued connection: RxDspWorker lives on the
        // DSP thread, RadeChannel lives on the main thread (created
        // by WdspEngine, parent owned by RadioModel).  Replacing the
        // earlier QMetaObject::invokeMethod(raw_ptr, ...,
        // Qt::QueuedConnection) with a Qt-native signal/slot
        // connection closes the UAF gap PR #238 review P1 #3
        // flagged: queued events for a destroyed receiver are
        // dropped under Qt's connection-list lock, instead of
        // calling into freed memory.
        connect(this, &RxDspWorker::radeIqReady,
                channel, &RadeChannel::processIq,
                Qt::QueuedConnection);
    }

    // Lifecycle tracer (off by default; enable with
    // QT_LOGGING_RULES="nereus.dsp.debug=true").
    qCDebug(lcDsp).noquote() << QString("RxDspWorker::setRadeChannel(%1)")
                                    .arg(reinterpret_cast<quintptr>(channel),
                                         0, 16);
}

void RxDspWorker::processIqBatch(int receiverIndex,
                                 const QVector<float>& interleavedIQ)
{
    // Snapshot the sizing for this batch so a concurrent
    // setBufferSizes() (e.g. mid-batch reconfigure) can't split a
    // single drain across two values. The fields are std::atomic<int>
    // to avoid the C++ data race that a plain int read would hit; the
    // local snapshot then gives a stable pair for the rest of the batch.
    const int inSize  = m_inSize.load(std::memory_order_relaxed);
    const int outSize = m_outSize.load(std::memory_order_relaxed);

    // Deinterleave and append to accumulation buffers. Done regardless
    // of WDSP wiring so the chunkDrained signal can be observed in
    // tests that don't link a real WDSP build.
    const int numSamples = interleavedIQ.size() / 2;
    // Defensive bounds check (2026-05-22, G2E gateware-lockup recovery):
    // a stuck-gateware radio occasionally sends malformed I/Q packets with
    // negative or absurd payload sizes.  Without this guard the reserve()
    // below tries to allocate hundreds of MB and crashes via Apple's
    // heap detector (EXC_BREAKPOINT in libsystem_malloc).  P2 frames are
    // 238 samples typically; even 1536 kHz never exceeds a few thousand
    // per packet.  Anything wildly larger is a corrupt stream — drop it.
    constexpr int kMaxSaneSamplesPerBatch = 65536;
    if (numSamples <= 0 || numSamples > kMaxSaneSamplesPerBatch) {
        return;
    }

    // ── Phase 3F Sub-Epic I Task 4: accumulate PER STREAM ────────────────
    // receiverIndex was previously Q_UNUSED, so every DDC appended into a
    // single shared accumulator pair. The moment a second DDC started
    // streaming its samples were interleaved into Slice A's chunk boundary
    // and corrupted Slice A's audio. Each stream now carries its own
    // partial chunk. std::unordered_map references are node-stable, so
    // `acc` stays valid for the whole call.
    StreamAccum& acc = m_accums[receiverIndex];
    acc.i.reserve(acc.i.size() + numSamples);
    acc.q.reserve(acc.q.size() + numSamples);
    for (int i = 0; i < numSamples; ++i) {
        acc.i.append(interleavedIQ[i * 2]);
        acc.q.append(interleavedIQ[i * 2 + 1]);
    }

    // Slices bound to this stream. Copied rather than referenced: QVector
    // is implicitly shared so the copy is a refcount bump, and it removes
    // any chance of a directly-connected slot invalidating the map entry
    // mid-drain. An unbound stream yields an empty list: it accumulates
    // and drains, but demodulates nothing.
    const auto sliceIt = m_streamSlices.find(receiverIndex);
    const QVector<int> slices = (sliceIt != m_streamSlices.end())
                                    ? sliceIt->second
                                    : QVector<int>{};

    // RxChannel::processIq writes sampleCount floats on the inactive-channel
    // memset path and outSampleCount via fexchange2, so the reusable output
    // scratch must cover the larger of the two.
    const int scratchLen = qMax(inSize, outSize);

    // Drain whole chunks of inSize through WDSP (or skip the WDSP/audio
    // calls when engines aren't wired — chunkDrained still fires so the
    // chunking contract is observable).
    while (acc.i.size() >= inSize) {
        if (m_wdspEngine != nullptr && m_audioEngine != nullptr) {
            // ── Phase 3F Sub-Epic I Task 4: one stream, many slices ──
            // Every slice bound to this stream demodulates the SAME I/Q
            // chunk through its OWN WDSP channel, differing by shift
            // offset, mode, filter and AGC. This is ChannelMaster's
            // one-_rcvr-many-subrx topology: `struct _rcvr` holds one
            // I/Q input and one noise blanker but `double*
            // audio[cmMAXSubRcvr]` outputs (cmaster.h:74-82
            // [v2.10.3.15]).
            //
            // Note the aliasing that topology implies: RxChannel::processIq
            // runs NB1 / NB2 in place on the input legs (xanbEXTF /
            // xnobEXTF, RxChannel.cpp:1543-1545), so a slice with a noise
            // blanker enabled blanks the chunk every later slice sees.
            // Upstream agrees the blanker belongs to the receiver, not the
            // sub-receiver (ANB panb / NOB pnob live in `struct _rcvr`);
            // hoisting NB ownership from RxChannel to the stream is a
            // separate change. Single-slice behaviour is unaffected.
            for (int sliceIdx : slices) {
                // Invariant: WDSP channel id == slice index.
                RxChannel* rxCh = m_wdspEngine->rxChannel(sliceIdx);
                if (rxCh == nullptr) {
                    // Defensive: WDSP RxChannel should always exist now
                    // (RadioModel creates it unconditionally per Phase 3R
                    // K-bench restructure). If absent, skip this slice;
                    // the chunk still drains below.
                    continue;
                }

                // ── WDSP always runs ──────────────────────────────────────
                // S-meter, spectrum, AGC, ADC-overflow detector all live
                // inside WDSP's RxChannel internals. They MUST update
                // every tick regardless of audio routing, so processIq
                // runs unconditionally. The decoded audio in outI/outQ
                // is gated below depending on whether RADE owns the
                // speaker path for this slice.
                if (m_sliceOutI.size() < scratchLen) {
                    m_sliceOutI.resize(scratchLen);
                    m_sliceOutQ.resize(scratchLen);
                }
                // The scratch is reused across slices and drains, so it must
                // be zeroed exactly where the old per-drain
                // `QVector<float> outI(inSize)` was value-initialised.
                // fexchange2 returns without writing either output leg when
                // the channel's exchange bit is clear (iobuffs.c:525, the
                // whole body is inside that test), which happens across
                // flush / restart transitions. Without the zero-fill that
                // path would replay the previous chunk's audio instead of
                // emitting silence. Cheaper than the allocation it replaces:
                // the old code zero-filled the same span AND hit the heap.
                m_sliceOutI.fill(0.0f);
                m_sliceOutQ.fill(0.0f);
                QVector<float>& outI = m_sliceOutI;
                QVector<float>& outQ = m_sliceOutQ;
                rxCh->processIq(acc.i.data(), acc.q.data(),
                                outI.data(), outQ.data(), inSize, outSize);

                // ── Phase 3R K-bench (source-first reframe): RADE RX fork
                //
                // freedv-gui (RADEReceiveStep.cpp:175-310 [@77e793a]) and
                // AetherSDR (RADEEngine.cpp:200-303 [@0cd4559]) BOTH feed
                // RADE post-SSB-demodulation REAL AUDIO (not raw DDC
                // complex baseband). The codec internally builds RADE_COMP
                // by setting real=audio, imag=0 — it's an audio-domain
                // demodulator, not a baseband one.
                //
                // Earlier NereusSDR attempts fed the raw DDC I/Q directly
                // and the codec never synced because the input format was
                // wrong. This fork now uses outI (WDSP's decoded audio,
                // 48 kHz dual-mono) → downsample to 24 kHz → interleave
                // as I=audio, Q=0 for RadeChannel::processIq. RADE's
                // internal 24→8 decimator + RADE_COMP assembly then
                // matches the freedv-gui pipeline byte-for-byte.
                //
                // outI / outQ are dual-mono identical (RXA patch panel
                // SetRXAPanelBinaural(channel, 0)), so we use outI as
                // the mono audio source.
                //
                // Phase 3F Sub-Epic I Task 4: the fork is SLICE 0 ONLY.
                // RADE owns exactly one channel and one speaker path, so
                // multi-slice RADE stays a documented deferral (RADE on A
                // while SSB on B, Phase 3F future). Without this gate a
                // secondary slice would take the fork, discard its own WDSP
                // audio below, and fall silent.
                RadeChannel* radeCh =
                    (sliceIdx == 0)
                        ? m_radeChannel.load(std::memory_order_acquire)
                        : nullptr;
                // One-shot tracer (off by default; enable with
                // QT_LOGGING_RULES="nereus.dsp.debug=true") to confirm
                // the RADE RX fork is reaching the codec during bench
                // shakedown.
                static int s_rxRadeDiagCount = 0;
                if (radeCh != nullptr && s_rxRadeDiagCount < 3) {
                    qCDebug(lcDsp).noquote()
                        << QString("RxDspWorker RADE fork #%1: radeCh=%2 "
                                   "outSize=%3 (audio rate=48kHz)")
                            .arg(s_rxRadeDiagCount + 1)
                            .arg(reinterpret_cast<quintptr>(radeCh), 0, 16)
                            .arg(outSize);
                    ++s_rxRadeDiagCount;
                }
                if (radeCh != nullptr && outSize > 0) {
                    // Lazy-build 48→24 audio downsampler. Single resampler
                    // (real audio); no Q-leg needed since RADE expects
                    // imag=0.
                    if (!m_radeRxDownsamplerI
                        || m_radeRxDownsamplerSrcRate != 48000.0) {
                        m_radeRxDownsamplerI =
                            std::make_unique<Resampler>(
                                48000.0, 24000.0, 4096);
                        // Q-leg downsampler unused in this path; keep it
                        // null so any stale state from the old direct-
                        // baseband path is discarded.
                        m_radeRxDownsamplerQ.reset();
                        m_radeRxDownsamplerSrcRate = 48000.0;
                    }

                    // Downsample WDSP's outI (48 kHz mono real audio)
                    // to 24 kHz.
                    QByteArray downAudio =
                        m_radeRxDownsamplerI->process(outI.data(), outSize);
                    const int outBytes = downAudio.size();
                    const int outFrames =
                        outBytes / static_cast<int>(sizeof(float));

                    if (outFrames > 0) {
                        // Build interleaved stereo float32 with audio in
                        // the I (real) leg and zero in the Q (imag) leg
                        // at 24 kHz. This matches AetherSDR's
                        // RADEEngine.cpp:222-227 pattern (DAX 24 kHz
                        // stereo PCM → average to mono → set imag=0)
                        // and freedv-gui's RADEReceiveStep:201 pattern
                        // (input short[] → RADE_COMP{re=sample, im=0}).
                        m_radeRxIqScratch.resize(
                            outFrames * 2 * static_cast<int>(sizeof(float)));
                        float* dst = reinterpret_cast<float*>(
                            m_radeRxIqScratch.data());
                        const float* srcAudio =
                            reinterpret_cast<const float*>(
                                downAudio.constData());
                        for (int i = 0; i < outFrames; ++i) {
                            dst[2 * i + 0] = srcAudio[i];   // real = audio
                            dst[2 * i + 1] = 0.0f;          // imag = 0
                        }
                        // Post to RadeChannel on the main thread via the
                        // queued radeIqReady signal connection (set in
                        // setRadeChannel).  Using signal/slot instead of
                        // QMetaObject::invokeMethod(raw_ptr, ...) closes
                        // the use-after-free gap PR #238 review P1 #3
                        // flagged: Qt drops queued slot calls under the
                        // connection-list lock when the receiver
                        // QObject is destroyed.  radeCh is still loaded
                        // above as a cheap gate so we skip the
                        // downsample work when no channel is wired.
                        emit radeIqReady(m_radeRxIqScratch);
                    }
                }

                // ── Audio routing ───────────────────────────────────────
                // In RADE mode, WDSP audio is discarded — RADE's
                // rxSpeechReady signal (wired in J4 to AudioEngine)
                // owns the speaker path. Otherwise route WDSP's decoded
                // audio to AudioEngine as before.
                if (radeCh == nullptr) {
                    // Phase 3F Sub-Epic I Task 4: slice 0 keeps
                    // m_interleavedOut to itself because the anti-VOX fork
                    // below reads it as the cancellation reference; a
                    // secondary slice writing there would hand the DEXP
                    // detector the wrong slice's audio. rxBlockReady
                    // consumes the pointer synchronously, so one scratch
                    // per role is enough.
                    QVector<float>& scratch =
                        (sliceIdx == 0) ? m_interleavedOut : m_interleavedOutAux;
                    if (scratch.size() < outSize * 2) {
                        scratch.resize(outSize * 2);
                    }
                    float* interleaved = scratch.data();
                    for (int i = 0; i < outSize; ++i) {
                        interleaved[i * 2 + 0] = outI[i];
                        interleaved[i * 2 + 1] = outQ[i];
                    }
                    // MasterMixer sums every registered slice into the one
                    // global output, so each slice pushes under its own id.
                    m_audioEngine->rxBlockReady(sliceIdx, interleaved, outSize);
                }

                emit sliceProcessed(sliceIdx, inSize);
            }
        } else {
            // No engines wired (unit tests): still honour the fan-out
            // contract so the signal sequence stays observable, exactly
            // as chunkDrained already fires without engines.
            for (int sliceIdx : slices) {
                emit sliceProcessed(sliceIdx, inSize);
            }
        }

        acc.i.remove(0, inSize);
        acc.q.remove(0, inSize);
        emit chunkDrained(inSize);
        emit chunkDrainedForStream(receiverIndex, inSize);

        // Phase 3M-3a-iv: fork the same RX audio block to TxWorkerThread
        // for the WDSP DEXP anti-VOX detector. Slice 0 is the only consumer
        // in the single-RX path; multi-RX mux lands with 3F.
        //
        // From Thetis ChannelMaster cmaster.c:159-175 [v2.10.3.13]: this
        // is the single-RX equivalent of pavoxmix → SendAntiVOXData.
        // No aamix instance is needed because there is one input, one
        // sample rate, and no mixing — a queued signal/slot replaces the
        // aamix port. When 3F multi-pan ships, this connection is replaced
        // by a real aamix port; the TxWorkerThread side stays unchanged.
        //
        // ── Tap-point signpost (3M-3a-iv post-bench refactor) ────────────
        // The anti-VOX cancellation reference is forked here from
        // RxDspWorker's demod output before it reaches AudioEngine.  This
        // is correct as long as the audio bus stage applies no processing
        // that diverges between outputs (per-bus EQ, gain, mute beyond
        // master).  Today's single-output PC speaker path satisfies this
        // assumption.  WHEN OUTPUT DIVERGENCE LANDS (radio-speaker output
        // with independent processing, or per-bus EQ/gain), the anti-VOX
        // tap MUST move from RxDspWorker to AudioEngine's post-mixer
        // summing point so the cancellation reference matches the audio
        // actually leaving the speakers.  This is a tap-point relocation
        // only; the WDSP DEXP block and TxChannel::sendAntiVoxData wrapper
        // stay unchanged.
        //
        // Fires regardless of WDSP wiring (mirrors chunkDrained's
        // contract): when engines are wired, the buffer carries the
        // freshly decoded WDSP audio in m_interleavedOut; when not, a
        // zero-filled stereo buffer of the correct outSize*2 floats is
        // emitted so test fixtures without fake engines still observe
        // the signal shape. The QVector<float> deep-copies on the queued
        // connection, leaving m_interleavedOut owned by this thread.
        //
        // Phase 3F Sub-Epic I Task 4: this stays a SLICE 0 feed and it
        // carries SLICE 0's audio specifically. m_interleavedOut is
        // written only by the sliceIdx == 0 branch above (secondary
        // slices use m_interleavedOutAux), so a multi-slice drain cannot
        // leak whichever slice happened to run last into the DEXP
        // detector.
        QVector<float> antiVoxBuffer(outSize * 2, 0.0f);
        if (m_wdspEngine != nullptr && m_audioEngine != nullptr
            && m_interleavedOut.size() >= outSize * 2) {
            const float* src = m_interleavedOut.constData();
            for (int i = 0; i < outSize * 2; ++i) {
                antiVoxBuffer[i] = src[i];
            }
        }
        emit antiVoxSampleReady(0, antiVoxBuffer, outSize);
    }

    emit batchProcessed();
}

void RxDspWorker::setStreamSlices(int streamIndex,
                                  const QVector<int>& sliceIndices)
{
    m_streamSlices[streamIndex] = sliceIndices;
}

void RxDspWorker::resetAccumulator()
{
    // Phase 3F Sub-Epic I Task 4: clear every stream's partial chunk.
    // Clearing the vectors in place rather than dropping the map entries
    // keeps QVector's capacity, matching the old single-accumulator
    // behaviour. Both callers hit this mid-reconfigure and the DSP thread
    // would otherwise re-grow the buffers batch by batch afterwards.
    //
    // m_streamSlices is intentionally left alone: the callers
    // (RadioModel::setSampleRateLive / setActiveRxCountLive) reconnect the
    // same slices after the reconfigure, and forgetting the bindings here
    // would leave every stream demodulating nothing until something
    // republished them.
    for (auto& entry : m_accums) {
        entry.second.i.clear();
        entry.second.q.clear();
    }
}

} // namespace NereusSDR
