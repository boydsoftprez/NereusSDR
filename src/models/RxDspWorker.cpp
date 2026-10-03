// =================================================================
// src/models/RxDspWorker.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/sync.c, original licence from Thetis source is included below
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
//                 NereusSDR-original. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-30 - Fix wave (DSP-thread allocations): a RADE route's
//                 resamplers and the RADE scratch are made when the route
//                 is set, and each block resamples into them
//                 (Resampler::processInto), so the DSP thread's per-block
//                 RADE path allocates nothing. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-10-01 - Diversity lane: every slice on the diversity target's
//                 stream is fed from the mixed buffer, as every sub-receiver
//                 of Thetis's stream 0 reads it (cmaster.c xcmaster, sync.c
//                 InboundBlock); the per-slice drain body moves into
//                 processSliceChunk so both paths share it. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
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

// --- From cmaster.c ---
/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

// --- From sync.c ---
/*  sync.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

#include "RxDspWorker.h"

#include "core/AudioEngine.h"
#include "core/LogCategories.h"
#include "core/ReceiverManager.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/Resampler.h"
#include "core/RadeRxWorker.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/platform/ThreadPlacement.h"

#include <algorithm>
#include <chrono>
#include <thread>

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
    //
    // R-R3-41: in nereusd on Linux, thread placement owns this thread's
    // core and priority (nice, never a real-time policy); see
    // ThreadPlacement.h.
    if (ThreadPlacement::managesThreadPriority()) {
        ThreadPlacement::instance().registerCurrentThread(ThreadRole::DspThread);
        return;
    }
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
    if (ThreadPlacement::managesThreadPriority()) {
        ThreadPlacement::instance().deregisterCurrentThread();
    }
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

int RxDspWorker::externalDiversityChunkSize() const
{
    if (m_wdspEngine && m_externalDiversityRoute.targetSliceId >= 0) {
        if (RxChannel* target =
                m_wdspEngine->rxChannel(
                    m_externalDiversityRoute.targetSliceId)) {
            if (target->bufferSize() > 0) {
                return target->bufferSize();
            }
        }
    }
    return m_inSize.load(std::memory_order_relaxed);
}

void RxDspWorker::prepareExternalDiversityBuffers(int chunkSize)
{
    if (chunkSize <= 0 || chunkSize > kMaxSaneSamplesPerBatch) {
        return;
    }

    m_externalDiversityChunkSize = chunkSize;
    m_externalDiversityMaxQueuedSamples =
        chunkSize + kMaxSaneSamplesPerBatch;

    m_externalDiversityPrimary.i.reserve(
        m_externalDiversityMaxQueuedSamples);
    m_externalDiversityPrimary.q.reserve(
        m_externalDiversityMaxQueuedSamples);
    m_externalDiversitySecondary.i.reserve(
        m_externalDiversityMaxQueuedSamples);
    m_externalDiversitySecondary.q.reserve(
        m_externalDiversityMaxQueuedSamples);

    const int complexDoubles = 2 * chunkSize;
    m_externalDiversityPrimaryInterleaved.resize(complexDoubles);
    m_externalDiversitySecondaryInterleaved.resize(complexDoubles);
    m_externalDiversityOutputInterleaved.resize(complexDoubles);
    m_externalDiversityOutputI.resize(chunkSize);
    m_externalDiversityOutputQ.resize(chunkSize);

    // The combined stream still passes through the target RxChannel and audio
    // interleave path. Size those shared scratch buffers here as part of route
    // publication as well: feedExternalDiversityTarget then performs no growth
    // on the steady-state DSP path.
    const int outSize = m_outSize.load(std::memory_order_relaxed);
    const int scratchLen = qMax(chunkSize, outSize);
    m_sliceOutI.resize(scratchLen);
    m_sliceOutQ.resize(scratchLen);
    m_interleavedOut.resize(2 * outSize);
    m_interleavedOutAux.resize(2 * outSize);
}

void RxDspWorker::setExternalDiversityRoute(
    int extDivId, int targetSliceId, int primaryStream, int secondaryStream)
{
    clearExternalDiversityRoute();

    ExternalDiversityRoute route{
        extDivId, targetSliceId, primaryStream, secondaryStream,
    };
    if (!route.active()) {
        return;
    }

    m_externalDiversityRoute = route;
    prepareExternalDiversityBuffers(externalDiversityChunkSize());
    if (m_externalDiversityChunkSize <= 0) {
        m_externalDiversityRoute = {};
        return;
    }
    m_externalDiversityStatsSlice.store(route.targetSliceId,
                                        std::memory_order_relaxed);

#ifdef NEREUS_BUILD_TESTS
    if (m_externalDiversityRouteHookForTest) {
        m_externalDiversityRouteHookForTest(
            true, route.targetSliceId,
            route.primaryStream, route.secondaryStream);
    }
#endif
}

void RxDspWorker::clearExternalDiversityRoute()
{
#ifdef NEREUS_BUILD_TESTS
    // Only the test hook below reads the route being cleared.
    const ExternalDiversityRoute oldRoute = m_externalDiversityRoute;
#endif
    m_externalDiversityPrimary.i.clear();
    m_externalDiversityPrimary.q.clear();
    m_externalDiversitySecondary.i.clear();
    m_externalDiversitySecondary.q.clear();
    m_externalDiversityRoute = {};
    m_externalDiversityChunkSize = 0;
    m_externalDiversityMaxQueuedSamples = 0;
    m_externalDiversityStatsSlice.store(-1, std::memory_order_relaxed);
    // A new route starts aligned; an episode in progress ends with the
    // route (its log line is not written: nothing resumed).
    m_externalDiversityInputDelay.skipping = false;
    m_externalDiversityInputDelay.episodeSkippedUs = 0;
    m_externalDiversityEpisodeAdvance[0] = m_externalDiversityEpisodeAdvance[1] = 0;
    m_externalDiversityDropPending[0] = m_externalDiversityDropPending[1] = 0;

#ifdef NEREUS_BUILD_TESTS
    if (oldRoute.active() && m_externalDiversityRouteHookForTest) {
        m_externalDiversityRouteHookForTest(
            false, oldRoute.targetSliceId,
            oldRoute.primaryStream, oldRoute.secondaryStream);
    }
#endif
}

bool RxDspWorker::isExternalDiversityTarget(int sliceId) const noexcept
{
    return m_externalDiversityRoute.active()
        && sliceId == m_externalDiversityRoute.targetSliceId;
}

void RxDspWorker::appendExternalDiversitySamples(
    StreamAccum& destination, const QVector<float>& interleavedIQ,
    int skipLeading)
{
    const int batchSamples = interleavedIQ.size() / 2;
    if (batchSamples <= 0 || batchSamples > kMaxSaneSamplesPerBatch
        || m_externalDiversityMaxQueuedSamples <= 0) {
        return;
    }
    const int first = qBound(0, skipLeading, batchSamples);
    const int samples = batchSamples - first;
    if (samples <= 0) {
        return;
    }

    // Bound a missing-leg backlog. Route setup reserves exactly this maximum,
    // so after warm-up the append and drain path cannot grow either vector.
    const int overflow =
        destination.i.size() + samples - m_externalDiversityMaxQueuedSamples;
    if (overflow > 0) {
        const int drop = qMin(overflow, destination.i.size());
        destination.i.remove(0, drop);
        destination.q.remove(0, drop);
    }

    for (int sample = first; sample < batchSamples; ++sample) {
        destination.i.append(interleavedIQ[2 * sample]);
        destination.q.append(interleavedIQ[2 * sample + 1]);
    }
}

void RxDspWorker::processExternalDiversityIqBatch(
    int sourceStream, const QVector<float>& interleavedIQ)
{
    if (!m_externalDiversityRoute.active()) {
        return;
    }

    if (sourceStream == m_externalDiversityRoute.primaryStream) {
        appendExternalDiversitySamples(
            m_externalDiversityPrimary, interleavedIQ);
    } else if (sourceStream == m_externalDiversityRoute.secondaryStream) {
        appendExternalDiversitySamples(
            m_externalDiversitySecondary, interleavedIQ);
    } else {
        return;
    }

    drainExternalDiversity();
}

// R-R3-40. Runs on the DSP thread. Per batch: one clock read and one relaxed
// atomic store, as on the ordinary stream path; the only log line is written
// once per episode, when it ends.
// The primary leg's lost input is the route's skipped input. The episode
// total is kept in samples and converted as a whole, so rounding each batch
// never loses time.
void RxDspWorker::countDiversityPrimaryLoss(qint64 drainSize, qint64 samples)
{
    InputDelayState& state = m_externalDiversityInputDelay;
    const qint64 beforeUs =
        spanUsForDrainSize(drainSize, m_externalDiversityEpisodeAdvance[0]);
    m_externalDiversityEpisodeAdvance[0] += samples;
    const qint64 afterUs =
        spanUsForDrainSize(drainSize, m_externalDiversityEpisodeAdvance[0]);
    state.episodeSkippedUs = afterUs;
    state.droppedUs.fetch_add(afterUs - beforeUs, std::memory_order_relaxed);
}

void RxDspWorker::processStampedExternalDiversityIqBatch(
    int sourceStream, const QVector<float>& interleavedIQ, qint64 enqueuedNs)
{
    if (!m_externalDiversityRoute.active()) {
        return;
    }
    int leg = -1;
    if (sourceStream == m_externalDiversityRoute.primaryStream) {
        leg = 0;
    } else if (sourceStream == m_externalDiversityRoute.secondaryStream) {
        leg = 1;
    } else {
        return;
    }
    StreamAccum& primary = m_externalDiversityPrimary;
    StreamAccum& secondary = m_externalDiversitySecondary;

    InputDelayState& state = m_externalDiversityInputDelay;
    const qint64 delayUs =
        std::max<qint64>(0, (ReceiverManager::enqueueClockNs() - enqueuedNs) / 1000);
    state.delayUs.store(delayUs, std::memory_order_relaxed);

    const qint64 drainSize = m_externalDiversityChunkSize;
    if (!state.skipping && delayUs > kDspInputDelayLimitMs * 1000) {
        // Episode start: the queued samples of both legs are older than this
        // batch and go with the skipped input. The legs' heads were aligned,
        // so each leg's input position now moves on by what it lost.
        state.skipping = true;
        state.episodeSkippedUs = 0;
        m_externalDiversityEpisodeAdvance[0] = 0;
        m_externalDiversityEpisodeAdvance[1] = secondary.i.size();
        m_externalDiversityDropPending[0] = m_externalDiversityDropPending[1] = 0;
        countDiversityPrimaryLoss(drainSize, primary.i.size());
        primary.i.clear();
        primary.q.clear();
        secondary.i.clear();
        secondary.q.clear();
    }

    const qint64 batchSamples = interleavedIQ.size() / 2;
    if (state.skipping) {
        if (delayUs >= kDspInputDelayResumeMs * 1000) {
            if (leg == 0) {
                countDiversityPrimaryLoss(drainSize, batchSamples);
            } else {
                m_externalDiversityEpisodeAdvance[1] += batchSamples;
            }
            return;
        }
        // Resume: the leg that lost less input is behind the other by the
        // difference; it drops that many leading samples from what arrives
        // next, so both accumulators start at the same input position.
        state.skipping = false;
        const qint64 lead =
            m_externalDiversityEpisodeAdvance[0] - m_externalDiversityEpisodeAdvance[1];
        m_externalDiversityDropPending[0] = lead < 0 ? -lead : 0;
        m_externalDiversityDropPending[1] = lead > 0 ? lead : 0;
        qCWarning(lcDsp).noquote()
            << QStringLiteral("Receive processing fell behind; skipped %1 ms "
                              "of input to catch up.")
                   .arg(state.episodeSkippedUs / 1000);
        state.episodeSkippedUs = 0;
    }

    const qint64 drop = std::min(m_externalDiversityDropPending[leg], batchSamples);
    m_externalDiversityDropPending[leg] -= drop;
    appendExternalDiversitySamples(leg == 0 ? primary : secondary, interleavedIQ,
                                   static_cast<int>(drop));
    drainExternalDiversity();
}

void RxDspWorker::feedExternalDiversityTarget(int samples)
{
    for (int sample = 0; sample < samples; ++sample) {
        m_externalDiversityOutputI[sample] =
            static_cast<float>(
                m_externalDiversityOutputInterleaved[2 * sample]);
        m_externalDiversityOutputQ[sample] =
            static_cast<float>(
                m_externalDiversityOutputInterleaved[2 * sample + 1]);
    }

#ifdef NEREUS_BUILD_TESTS
    if (m_externalDiversityOutputHookForTest) {
        m_externalDiversityOutputHookForTest(
            m_externalDiversityRoute.targetSliceId,
            m_externalDiversityOutputI.constData(),
            m_externalDiversityOutputQ.constData(),
            samples);
    }
#endif

    RxChannel* target =
        m_wdspEngine
            ? m_wdspEngine->rxChannel(
                  m_externalDiversityRoute.targetSliceId)
            : nullptr;
    const int outSize = m_outSize.load(std::memory_order_relaxed);

    if (target && m_audioEngine) {
        const int scratchLen = qMax(samples, outSize);
        if (m_sliceOutI.size() < scratchLen) {
            // Route setup occurs after the target channel exists in
            // production, so this is a one-time control-path growth.
            m_sliceOutI.resize(scratchLen);
            m_sliceOutQ.resize(scratchLen);
        }
        m_sliceOutI.fill(0.0f);
        m_sliceOutQ.fill(0.0f);

        // The target is excluded from normal source-stream fan-out. It owns
        // the single blanker pass on the already-combined stream instead.
        target->setNoiseBlankerBypassed(false);
        target->processIq(
            m_externalDiversityOutputI.data(),
            m_externalDiversityOutputQ.data(),
            m_sliceOutI.data(), m_sliceOutQ.data(),
            samples, outSize);

        QVector<float>& scratch =
            (m_externalDiversityRoute.targetSliceId == 0)
                ? m_interleavedOut
                : m_interleavedOutAux;
        if (scratch.size() < 2 * outSize) {
            scratch.resize(2 * outSize);
        }
        for (int sample = 0; sample < outSize; ++sample) {
            scratch[2 * sample] = m_sliceOutI[sample];
            scratch[2 * sample + 1] = m_sliceOutQ[sample];
        }
        m_audioEngine->rxBlockReady(
            m_externalDiversityRoute.targetSliceId,
            scratch.data(), outSize);
    }

    // The signal is the no-real-channel unit-test seam and the same
    // post-process observation point used by ordinary slice fan-out.
    if (target || !m_audioEngine) {
        emit sliceProcessed(
            m_externalDiversityRoute.targetSliceId, samples);
    }

    // The other slices on the target's stream are sub-receivers of the same
    // receiver, and they read the same mixed buffer. ChannelMaster runs every
    // sub-receiver of a stream on that stream's one input, which for stream 0
    // is the diversity output (sync.c:49-51, quoted in processIqBatch):
    //   From Thetis ChannelMaster/cmaster.c:365-366 [v2.10.3.15]
    //     for (j = 0; j < pcm->cmSubRCVR; j++)
    //         fexchange0 (chid (stream, j), pcm->in[stream], pcm->rcvr[rx].audio[j], &error);		// dsp
    // One blanking pass per stream (cmaster.c:351-352 runs xanb / xnob once,
    // before that loop): the target above blanked the mixed legs in place,
    // so the slices after it are bypassed.
    const QVector<int> streamSlices = externalDiversityStreamSlices();
    bool blankerClaimed = target && m_audioEngine;
    for (int sliceIdx : streamSlices) {
        if (sliceIdx == m_externalDiversityRoute.targetSliceId) {
            continue;
        }
        if (m_audioEngine) {
            if (processSliceChunk(sliceIdx,
                                  m_externalDiversityOutputI.data(),
                                  m_externalDiversityOutputQ.data(),
                                  samples, outSize, blankerClaimed)) {
                blankerClaimed = true;
            }
        } else {
            emit sliceProcessed(sliceIdx, samples);
        }
    }
}

void RxDspWorker::drainExternalDiversity()
{
    if (!m_externalDiversityRoute.active() || !m_wdspEngine) {
        return;
    }

    const int chunkSize = externalDiversityChunkSize();
    if (chunkSize <= 0) {
        return;
    }
    if (chunkSize != m_externalDiversityChunkSize) {
        // A geometry change invalidates unmatched samples. Buffer growth is
        // deliberately forbidden here: RadioModel republishes the route from
        // its control thread after changing the target channel's rate, which
        // is where prepareExternalDiversityBuffers may allocate.
        m_externalDiversityPrimary.i.clear();
        m_externalDiversityPrimary.q.clear();
        m_externalDiversitySecondary.i.clear();
        m_externalDiversitySecondary.q.clear();
        return;
    }

    while (m_externalDiversityPrimary.i.size() >= chunkSize
           && m_externalDiversitySecondary.i.size() >= chunkSize) {
        for (int sample = 0; sample < chunkSize; ++sample) {
            m_externalDiversityPrimaryInterleaved[2 * sample] =
                m_externalDiversityPrimary.i[sample];
            m_externalDiversityPrimaryInterleaved[2 * sample + 1] =
                m_externalDiversityPrimary.q[sample];
            m_externalDiversitySecondaryInterleaved[2 * sample] =
                m_externalDiversitySecondary.i[sample];
            m_externalDiversitySecondaryInterleaved[2 * sample + 1] =
                m_externalDiversitySecondary.q[sample];
        }

        double* inputs[2] = {
            m_externalDiversityPrimaryInterleaved.data(),
            m_externalDiversitySecondaryInterleaved.data(),
        };
        const bool processed = m_wdspEngine->processExternalDiversity(
            m_externalDiversityRoute.extDivId, chunkSize,
            inputs, m_externalDiversityOutputInterleaved.data());

        m_externalDiversityPrimary.i.remove(0, chunkSize);
        m_externalDiversityPrimary.q.remove(0, chunkSize);
        m_externalDiversitySecondary.i.remove(0, chunkSize);
        m_externalDiversitySecondary.q.remove(0, chunkSize);

        if (!processed) {
            // A stop racing queued input must not leave stale paired samples
            // to replay if the same route is enabled later.
            m_externalDiversityPrimary.i.clear();
            m_externalDiversityPrimary.q.clear();
            m_externalDiversitySecondary.i.clear();
            m_externalDiversitySecondary.q.clear();
            return;
        }
        feedExternalDiversityTarget(chunkSize);
    }
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

// RADE threads (2026-09-30), NereusSDR-original.
void RxDspWorker::setRadeRxRoute(int sliceId,
                                 std::shared_ptr<RadeRxBridge> bridge)
{
    if (!bridge) {
        m_radeRxRoutes.erase(sliceId);
    } else {
        auto it = m_radeRxRoutes.find(sliceId);
        if (it == m_radeRxRoutes.end() || it->second.bridge != bridge) {
            RadeRxRoute route;
            route.bridge = std::move(bridge);
            route.epoch = route.bridge->nextEpoch();
            // Made here, once per route, not on the block path.
            route.down = std::make_unique<Resampler>(48000.0, 24000.0, kRadeRxMaxBlockFrames);
            route.upL = std::make_unique<Resampler>(24000.0, 48000.0, kRadeRxMaxBlockFrames);
            route.upR = std::make_unique<Resampler>(24000.0, 48000.0, kRadeRxMaxBlockFrames);
            route.play.reserve(kRadeRxPlayReserveFloats);
            m_radeRxRoutes[sliceId] = std::move(route);
            reserveRadeRxScratch();
        }
    }
    m_radeRxRouteCount.store(int(m_radeRxRoutes.size()),
                             std::memory_order_release);
    qCDebug(lcDsp).noquote()
        << QString("RxDspWorker::setRadeRxRoute(slice=%1, routed=%2, routes=%3)")
               .arg(sliceId)
               .arg(m_radeRxRoutes.count(sliceId) != 0)
               .arg(m_radeRxRoutes.size());
}

void RxDspWorker::reserveRadeRxScratch()
{
    // Sized once: the block path only resizes within these capacities.
    const auto atLeast = [](std::vector<float>& v, size_t floats) {
        if (v.size() < floats) {
            v.resize(floats);
        }
    };
    m_radeRxIqScratch.reserve(size_t(kRadeRxMaxBlockFrames) * 2);
    m_radeRxDueScratch.reserve(size_t(RadeRxBridge::kMaxRecordFrames) * 2);
    m_radeRxLegL.reserve(size_t(RadeRxBridge::kMaxRecordFrames));
    m_radeRxLegR.reserve(size_t(RadeRxBridge::kMaxRecordFrames));
    m_radeRxOutScratch.reserve(size_t(kRadeRxMaxBlockFrames) * 2);
    atLeast(m_radeRxDown, size_t(kRadeRxMaxBlockFrames));
    // 24 -> 48 kHz doubles a record of up to kMaxRecordFrames, with room
    // for r8brain's uneven output per call.
    atLeast(m_radeRxUpL, size_t(kRadeRxMaxBlockFrames) * 2);
    atLeast(m_radeRxUpR, size_t(kRadeRxMaxBlockFrames) * 2);
}

void RxDspWorker::clearRadeRxRoutes()
{
    m_radeRxRoutes.clear();
    m_radeRxRouteCount.store(0, std::memory_order_release);
}

bool RxDspWorker::processRadeRxBlock(int sliceIdx, const float* audio48k,
                                     int outSize)
{
    auto it = m_radeRxRoutes.find(sliceIdx);
    if (it == m_radeRxRoutes.end() || outSize <= 0) {
        return it != m_radeRxRoutes.end();
    }
    RadeRxRoute& route = it->second;
    RadeRxBridge& bridge = *route.bridge;

    // ── Feed ────────────────────────────────────────────────────────────
    // [original comments from the single-binding fork, kept with it:]
    // Lazy-build 48→24 audio downsampler. Single resampler
    // (real audio); no Q-leg needed since RADE expects
    // imag=0.
    // (Fix wave: built in setRadeRxRoute now, with the route.)
    // Downsample WDSP's outI (48 kHz mono real audio)
    // to 24 kHz.
    const int inFrames = route.down->processInto(audio48k, outSize, m_radeRxDown.data(),
                                                 int(m_radeRxDown.size()));
    const quint32 seq = route.seq++;
    if (inFrames > 0) {
        // Build interleaved stereo float32 with audio in
        // the I (real) leg and zero in the Q (imag) leg
        // at 24 kHz. This matches AetherSDR's
        // RADEEngine.cpp:222-227 pattern (DAX 24 kHz
        // stereo PCM → average to mono → set imag=0)
        // and freedv-gui's RADEReceiveStep:201 pattern
        // (input short[] → RADE_COMP{re=sample, im=0}).
        m_radeRxIqScratch.resize(size_t(inFrames) * 2);
        const float* srcAudio = m_radeRxDown.data();
        for (int i = 0; i < inFrames; ++i) {
            m_radeRxIqScratch[size_t(2 * i) + 0] = srcAudio[i];  // real = audio
            m_radeRxIqScratch[size_t(2 * i) + 1] = 0.0f;         // imag = 0
        }
        if (bridge.pushInput(route.epoch, seq, m_radeRxIqScratch.data(), inFrames)) {
            RadeRxWorker::wake(bridge);
        }
    }

    // ── Play ────────────────────────────────────────────────────────────
    // The block fed radeLateBoundBlocks() ago is due now. If its speech is
    // back it plays; if not, this slot is silence and the speech is dropped
    // when it does arrive. Never waits (JJ's ruling 2).
    const int lateBound = radeLateBoundBlocks(outSize);
    int dueFrames = 0;
    if (route.fedBlocks >= lateBound) {
        // Sequence numbers wrap; the bridge compares them by distance.
        const quint32 dueSeq = seq - quint32(lateBound);
        dueFrames = bridge.takeDue(route.epoch, dueSeq, m_radeRxDueScratch);
        bridge.countSlot(dueFrames > 0);
    } else {
        ++route.fedBlocks;  // the first lateBound slots of a route are silence
    }
    if (dueFrames > 0) {
        // The speech is 24 kHz stereo; bring each leg to 48 kHz as
        // RadioModel::onRadeSpeechReady did before this moved here.
        m_radeRxLegL.resize(size_t(dueFrames));
        m_radeRxLegR.resize(size_t(dueFrames));
        for (int i = 0; i < dueFrames; ++i) {
            m_radeRxLegL[size_t(i)] = m_radeRxDueScratch[size_t(2 * i)];
            m_radeRxLegR[size_t(i)] = m_radeRxDueScratch[size_t(2 * i) + 1];
        }
        const int upL = route.upL->processInto(m_radeRxLegL.data(), dueFrames,
                                               m_radeRxUpL.data(), int(m_radeRxUpL.size()));
        const int upR = route.upR->processInto(m_radeRxLegR.data(), dueFrames,
                                               m_radeRxUpR.data(), int(m_radeRxUpR.size()));
        const int frames48k = std::min(upL, upR);
        const float* left = m_radeRxUpL.data();
        const float* right = m_radeRxUpR.data();
        const size_t base = route.play.size();
        route.play.resize(base + size_t(frames48k) * 2);
        for (int i = 0; i < frames48k; ++i) {
            route.play[base + size_t(2 * i)] = left[i];
            route.play[base + size_t(2 * i) + 1] = right[i];
        }
    }

    // Exactly outSize frames every block, like every other slice, so this
    // slice keeps the mixer's pace whatever its decoder does. Short: the
    // rest is silence. Long (resampler jitter, a slot of silence ahead of
    // speech that then arrived whole): keep at most four blocks queued.
    const size_t want = size_t(outSize) * 2;
    const size_t keepMax = want * 4;
    if (route.play.size() > keepMax) {
        route.play.erase(route.play.begin(),
                         route.play.begin() + std::ptrdiff_t(route.play.size() - keepMax));
    }
    m_radeRxOutScratch.assign(want, 0.0f);
    const size_t take = std::min(want, route.play.size());
    std::copy(route.play.begin(), route.play.begin() + std::ptrdiff_t(take),
              m_radeRxOutScratch.begin());
    route.play.erase(route.play.begin(), route.play.begin() + std::ptrdiff_t(take));
    if (m_audioEngine != nullptr) {
        m_audioEngine->rxBlockReady(sliceIdx, m_radeRxOutScratch.data(), outSize);
    }
    return true;
}

RxDspWorker::InputDelayStats RxDspWorker::inputDelayStats(int receiverIndex) const
{
    InputDelayStats stats;
    if (receiverIndex < 0 || receiverIndex >= kMaxInputDelayReceivers) {
        return stats;
    }
    const InputDelayState& state = m_inputDelay[receiverIndex];
    stats.inputDelayMs   = state.delayUs.load(std::memory_order_relaxed) / 1000;
    stats.droppedInputMs = state.droppedUs.load(std::memory_order_relaxed) / 1000;
    return stats;
}

RxDspWorker::InputDelayStats RxDspWorker::externalDiversityInputDelayStats() const
{
    InputDelayStats stats;
    stats.inputDelayMs =
        m_externalDiversityInputDelay.delayUs.load(std::memory_order_relaxed) / 1000;
    stats.droppedInputMs =
        m_externalDiversityInputDelay.droppedUs.load(std::memory_order_relaxed) / 1000;
    return stats;
}

RxDspWorker::InputDelayStats RxDspWorker::inputDelayStatsForSlice(int sliceId,
                                                                  int streamIndex) const
{
    if (sliceId >= 0
        && m_externalDiversityStatsSlice.load(std::memory_order_relaxed) == sliceId) {
        return externalDiversityInputDelayStats();
    }
    return streamIndex >= 0 ? inputDelayStats(streamIndex) : InputDelayStats{};
}

qint64 RxDspWorker::spanUsForDrainSize(qint64 inSize, qint64 samples)
{
    if (inSize <= 0 || samples <= 0) {
        return 0;
    }
    // rate = inSize * 48000 / 64, inverting the drain-size rule documented
    // on setStreamInputChunk; span = samples / rate.
    return samples * 64 * 1000000 / (inSize * 48000);
}

qint64 RxDspWorker::inputSpanUs(int receiverIndex, qint64 samples) const
{
    const auto sizeIt = m_streamInSize.find(receiverIndex);
    const qint64 inSize = (sizeIt != m_streamInSize.end() && sizeIt->second > 0)
                              ? sizeIt->second
                              : m_inSize.load(std::memory_order_relaxed);
    return spanUsForDrainSize(inSize, samples);
}

// R-R3-40. Runs on the DSP thread. Per batch this adds one clock read and one
// relaxed atomic store; a skipped batch also adds one relaxed fetch_add. The
// only log line is written once per episode, when it ends.
void RxDspWorker::processStampedIqBatch(int receiverIndex,
                                        const QVector<float>& interleavedIQ,
                                        qint64 enqueuedNs)
{
    if (receiverIndex < 0 || receiverIndex >= kMaxInputDelayReceivers) {
        processIqBatch(receiverIndex, interleavedIQ);
        return;
    }

    InputDelayState& state = m_inputDelay[receiverIndex];
    const qint64 delayUs =
        std::max<qint64>(0, (ReceiverManager::enqueueClockNs() - enqueuedNs) / 1000);
    state.delayUs.store(delayUs, std::memory_order_relaxed);

    if (!state.skipping && delayUs > kDspInputDelayLimitMs * 1000) {
        state.skipping = true;
        state.episodeSkippedUs = 0;
        // The stream's partial chunk is older than this batch; it goes with
        // the skipped input rather than being joined to newer samples.
        auto accIt = m_accums.find(receiverIndex);
        if (accIt != m_accums.end() && !accIt->second.i.isEmpty()) {
            const qint64 partialUs =
                inputSpanUs(receiverIndex, accIt->second.i.size());
            state.episodeSkippedUs += partialUs;
            state.droppedUs.fetch_add(partialUs, std::memory_order_relaxed);
            accIt->second.i.clear();
            accIt->second.q.clear();
        }
    }

    if (state.skipping) {
        if (delayUs >= kDspInputDelayResumeMs * 1000) {
            const qint64 spanUs =
                inputSpanUs(receiverIndex, interleavedIQ.size() / 2);
            state.episodeSkippedUs += spanUs;
            state.droppedUs.fetch_add(spanUs, std::memory_order_relaxed);
            emit batchProcessed();
            return;
        }
        state.skipping = false;
        qCWarning(lcDsp).noquote()
            << QStringLiteral("Receive processing fell behind; skipped %1 ms "
                              "of input to catch up.")
                   .arg(state.episodeSkippedUs / 1000);
        state.episodeSkippedUs = 0;
    }

    processIqBatch(receiverIndex, interleavedIQ);
}

void RxDspWorker::processIqBatch(int receiverIndex,
                                 const QVector<float>& interleavedIQ)
{
#ifdef NEREUS_BUILD_TESTS
    if (const int delayUs =
            m_processingDelayUsForTest.load(std::memory_order_relaxed);
        delayUs > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(delayUs));
    }
#endif

    // Snapshot the sizing for this batch so a concurrent
    // setBufferSizes() (e.g. mid-batch reconfigure) can't split a
    // single drain across two values. The fields are std::atomic<int>
    // to avoid the C++ data race that a plain int read would hit; the
    // local snapshot then gives a stable pair for the rest of the batch.
    const int defaultInSize = m_inSize.load(std::memory_order_relaxed);
    const int outSize       = m_outSize.load(std::memory_order_relaxed);

    // ── Phase 3F Sub-Epic I closeout, defect H1 ──────────────────────────
    // The drain threshold belongs to the DDC STREAM, not to the radio.
    // Before this, one geometry served every stream: RadioModel called
    // setBufferSizes() once with the connection-wide rate's in_size, and this
    // loop used that single value for every accumulator. The moment a second
    // stream ran at a rate the first does not share, the threshold was wrong
    // for whichever stream it was not computed from, and fexchange2 was
    // handed the wrong number of samples.
    //
    // That is a chunk-geometry error, not a cadence one. fexchange2 copies
    // exactly ch[channel].in_size samples out of the caller's legs
    // (iobuffs.c:532-536 [WDSP v1.29]) without consulting any count we pass,
    // so a threshold BELOW the channel's configured in_size reads past the
    // end of the accumulator. RadioModel keeps the two in step; see
    // RadioModel::applyStreamDspGeometry for the ordering that guarantees it.
    //
    // Upstream keys the size per input stream for the same reason:
    //   From Thetis cmaster.c:461 [v2.10.3.15]
    //     pcm->xcm_insize[in_id] = getbuffsize (rate);
    const auto sizeIt = m_streamInSize.find(receiverIndex);
    const int inSize  = (sizeIt != m_streamInSize.end() && sizeIt->second > 0)
                            ? sizeIt->second
                            : defaultInSize;

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

    // External diversity: the stream hosting the diversity target is the
    // one receiver whose input is the mix. Every slice on it, the target
    // and any slice sharing its stream alike, is fed from the mixed buffer
    // (feedExternalDiversityTarget), so none takes this raw primary-leg
    // chunk. Thetis's sub-receivers of stream 0 all read Inbound(0), which
    // is the xdivEXT output while diversity runs:
    //   From Thetis ChannelMaster/sync.c:49-51 [v2.10.3.15]
    //     case 0: // diversity receivers
    //         xdivEXT (0, nsamples, data, psyn->divbuff);
    //         Inbound (0, nsamples, psyn->divbuff);
    const bool diversityStream = m_externalDiversityRoute.active()
        && slices.contains(m_externalDiversityRoute.targetSliceId);
    const auto fedFromDiversityMix = [this, diversityStream](int sliceId) {
        return diversityStream || isExternalDiversityTarget(sliceId);
    };

    // (Phase 3F Sub-Epic I closeout defect G1 computed a hostsSliceZero flag
    //  here to elect one stream to raise the anti-VOX fork. Sub-Epic J Task 9
    //  retired that fork; see the note at the bottom of the drain loop.)

    // Drain whole chunks of inSize through WDSP (or skip the WDSP/audio
    // calls when engines aren't wired — chunkDrained still fires so the
    // chunking contract is observable).
    while (acc.i.size() >= inSize) {
        // ── Phase 3F Sub-Epic I Task 4b: one blanking pass per chunk ─────
        // The noise blanker belongs to the DDC stream, not the slice
        // (ANB panb / NOB pnob live in `struct _rcvr` alongside
        // `double* audio[cmMAXSubRcvr]`, cmaster.h:74-82 [v2.10.3.15]).
        // This announces that the chunk about to be fanned out gets
        // exactly one blanking pass, owned by this stream. It reports the
        // topology, not whether NB happens to be switched on, and fires
        // without engines wired so the contract stays observable in unit
        // tests (same rule as chunkDrained / sliceProcessed).
        const bool hasOrdinarySlice =
            std::any_of(slices.cbegin(), slices.cend(),
                        [&fedFromDiversityMix](int sliceId) {
                            return !fedFromDiversityMix(sliceId);
                        });
        if (hasOrdinarySlice) {
            emit streamNoiseBlankerApplied(receiverIndex);
        }

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
            // That topology dictates who blanks: RxChannel::processIq runs
            // NB1 / NB2 in place on the input legs (xanbEXTF / xnobEXTF,
            // RxChannel.cpp:1557-1562), so with every slice handed the same
            // chunk, only ONE of them may blank it. The first slice to reach
            // processIq owns the stream's blanker and runs with its own NB
            // settings; the rest are bypassed via setNoiseBlankerBypassed so
            // they cannot re-blank an already-blanked buffer. Matches
            // upstream's one ANB / NOB per receiver. Single-slice behaviour
            // is byte-identical to before (the sole slice is always first,
            // so it is never bypassed).
            bool streamBlankerClaimed = false;
            for (int sliceIdx : slices) {
                if (fedFromDiversityMix(sliceIdx)) {
                    continue;
                }

                if (processSliceChunk(sliceIdx, acc.i.data(), acc.q.data(),
                                      inSize, outSize, streamBlankerClaimed)) {
                    streamBlankerClaimed = true;
                }
            }
        } else {
            // No engines wired (unit tests): still honour the fan-out
            // contract so the signal sequence stays observable, exactly
            // as chunkDrained already fires without engines.
            for (int sliceIdx : slices) {
                if (fedFromDiversityMix(sliceIdx)) {
                    continue;
                }
                emit sliceProcessed(sliceIdx, inSize);
            }
        }

        acc.i.remove(0, inSize);
        acc.q.remove(0, inSize);
        emit chunkDrained(inSize);
        emit chunkDrainedForStream(receiverIndex, inSize);

        // ── The anti-VOX fork used to live here ──────────────────────────
        //
        // Phase 3M-3a-iv forked the RX audio block from this spot to
        // TxWorkerThread for the WDSP DEXP anti-VOX detector, gated on the
        // stream hosting slice 0. Phase 3F Sub-Epic J Task 9 retired it:
        // the reference is now AudioEngine::m_antiVoxMix, a second
        // MasterMixer summing every audible slice, which is what Thetis
        // does (every sub-receiver is pushed into the transmitter's
        // anti-VOX mixer, cmaster.c:371-372 [v2.10.3.15]). Slice A's audio
        // alone was the wrong reference the moment a second receiver
        // became audible.
        //
        // The reasoning is kept rather than deleted because it is the only
        // place two constraints on that feed are written down, and both
        // still bind, they are just satisfied somewhere else now.
        //
        // ── Tap-point signpost (3M-3a-iv post-bench refactor), RESOLVED ──
        // The signpost that stood here said: forking the cancellation
        // reference from RxDspWorker's demod output is correct only while
        // the audio bus stage applies no processing that diverges between
        // outputs (per-bus EQ, gain, mute beyond master), and WHEN OUTPUT
        // DIVERGENCE LANDS the tap MUST move to AudioEngine's post-mixer
        // summing point so the reference matches the audio actually
        // leaving the speakers. Per-slice mute, gain and pan are exactly
        // that divergence, and Task 9 made exactly that move. The tap is
        // now AudioEngine's anti-VOX mixer drain in rxBlockReady; the WDSP
        // DEXP block and the TxChannel::sendAntiVoxData wrapper were left
        // unchanged, as the signpost predicted.
        //
        // ── Cadence: Sub-Epic I closeout defects G1 and H1 ───────────────
        // DEXP is configured with exactly one block geometry:
        // TxWorkerThread::setAntiVoxBlockGeometry pushes SetAntiVOXSize
        // (outSize) and SetAntiVOXRate (48 kHz panel rate) once, from
        // RxDspWorker::bufferSizesChanged, which is still emitted from this
        // class and still the authority on the detector's dimensions.
        //
        // The detector integrates one block per delivery and no faster.
        // From Thetis wdsp/dexp.c:288-297 [v2.10.3.15]: on each xdexp() pass
        // it walks antivox_size samples through a single-pole IIR whose
        // coefficient is antivox_mult = exp(-1/(antivox_rate * antivox_tau)),
        // then clears antivox_new. The IIR is therefore calibrated in
        // SAMPLE steps at antivox_rate, and one block must represent
        // antivox_size / antivox_rate seconds of wall clock.
        //
        // Delivery is destructive, not accumulating. From Thetis
        // wdsp/dexp.c:708-715 [v2.10.3.15], SendAntiVOXData memcpys over
        // antivox_data and re-raises antivox_new, so a second block arriving
        // before the TXA pump consumes the first silently replaces it.
        // Deliver too fast and antivox_level integrates faster than
        // antivox_rate says it should; too slow and it goes stale. Either
        // way `asig = avsig - antivox_gain * antivox_level` (dexp.c:313-316
        // [v2.10.3.15]) comes out wrong, which is a false VOX trigger
        // (unintended transmit) or a failure to cancel.
        //
        // The slice-0 gate held that cadence because this drain loop runs
        // once per DDC STREAM, and an ungated emit would have handed the
        // detector one block per draining stream instead of one per period.
        // One stream was elected to raise it, and the arithmetic worked out
        // exactly: the drain interval is inSize / inputRate seconds, Thetis
        // sizes inSize = 64 * inputRate / 48000 with outSize 64
        // (RadioModel.cpp:6866 passes literal 64), so
        // inSize / inputRate == outSize / outRate. Defect H1 kept that
        // exact once streams carried their own rates, since each stream's
        // inSize is derived from its own rate through the same formula.
        //
        // THAT ARGUMENT NOW LIVES IN AudioEngine's DRAIN. The anti-VOX
        // mixer's readiness barrier releases at most one summed block per
        // audio period no matter how many streams or slices feed it, which
        // is the same one-block-per-outSize/outRate-seconds contract
        // arrived at by a different route: membership, not election.
        // Pinned by tst_audio_engine_antivox_mix's
        // theAntiVoxMixDrainsOneBlockPerPeriod.
    }

    emit batchProcessed();
}

bool RxDspWorker::processSliceChunk(int sliceIdx, float* inI, float* inQ,
                                    int inSize, int outSize,
                                    bool blankerClaimed)
{
    // RxChannel::processIq writes sampleCount floats on the inactive-channel
    // memset path and outSampleCount via fexchange2, so the reusable output
    // scratch must cover the larger of the two.
    const int scratchLen = qMax(inSize, outSize);

    // Invariant: WDSP channel id == slice index.
    RxChannel* rxCh = m_wdspEngine->rxChannel(sliceIdx);
    if (rxCh == nullptr) {
        // Defensive: WDSP RxChannel should always exist now
        // (RadioModel creates it unconditionally per Phase 3R
        // K-bench restructure). If absent, skip this slice;
        // the chunk still drains below.
        return false;
    }

    // Claim the stream's single blanking pass for the first
    // slice that actually reaches processIq. Anchored on the
    // processIq call rather than on position in `slices` so a
    // skipped slice above cannot consume the pass and leave the
    // chunk unblanked.
    // (The caller marks the pass claimed when this returns true.)
    rxCh->setNoiseBlankerBypassed(blankerClaimed);

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
    rxCh->processIq(inI, inQ,
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
    // RADE threads (2026-09-30): any number of slices route
    // to RADE, each to its own decoder thread
    // (processRadeRxBlock), which also delivers the slice's
    // audio for this block.
    const bool routesToRade =
        processRadeRxBlock(sliceIdx, outI.data(), outSize);
    // One-shot tracer (off by default; enable with
    // QT_LOGGING_RULES="nereus.dsp.debug=true") to confirm
    // the RADE RX fork is reaching the codec during bench
    // shakedown.
    static int s_rxRadeDiagCount = 0;
    if (routesToRade && s_rxRadeDiagCount < 3) {
        qCDebug(lcDsp).noquote()
            << QString("RxDspWorker RADE fork #%1: "
                       "slice=%2 outSize=%3 (audio rate=48kHz)")
                .arg(s_rxRadeDiagCount + 1)
                .arg(sliceIdx)
                .arg(outSize);
        ++s_rxRadeDiagCount;
    }

    // ── Audio routing ───────────────────────────────────────
    // In RADE mode, WDSP audio is discarded — RADE's
    // rxSpeechReady signal (wired in J4 to AudioEngine)
    // owns the speaker path. Otherwise route WDSP's decoded
    // audio to AudioEngine as before.
    // [RADE threads: the decoded speech now reaches AudioEngine
    // from processRadeRxBlock above, on this thread.]
    if (!routesToRade) {
        // RADE gaps (2026-09-30): a slice in RADE mode whose
        // route is not here (yet) keeps its place in the mix
        // with silence. Its WDSP audio is the sideband, which
        // a RADE slice never plays (setRadeModeSlices).
        static_assert(WdspEngine::kMaxSliceChannels <= kRadeModeMaskSlices,
                      "every slice id needs a RADE mode bit");
        const bool radeWithoutRoute = sliceIdx >= 0
            && sliceIdx < kRadeModeMaskSlices
            && ((m_radeModeSlices.load(std::memory_order_acquire)
                 >> sliceIdx) & 1u) != 0;
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
        if (radeWithoutRoute) {
            std::fill(interleaved, interleaved + outSize * 2, 0.0f);
        } else {
            for (int i = 0; i < outSize; ++i) {
                interleaved[i * 2 + 0] = outI[i];
                interleaved[i * 2 + 1] = outQ[i];
            }
        }
        // MasterMixer sums every registered slice into the one
        // global output, so each slice pushes under its own id.
        m_audioEngine->rxBlockReady(sliceIdx, interleaved, outSize);
    }

    emit sliceProcessed(sliceIdx, inSize);
    return true;
}

QVector<int> RxDspWorker::externalDiversityStreamSlices() const
{
    if (!m_externalDiversityRoute.active()) {
        return {};
    }
    for (const auto& [stream, slices] : m_streamSlices) {
        Q_UNUSED(stream);
        if (slices.contains(m_externalDiversityRoute.targetSliceId)) {
            return slices;
        }
    }
    return {};
}

void RxDspWorker::setStreamSlices(int streamIndex,
                                  const QVector<int>& sliceIndices)
{
    m_streamSlices[streamIndex] = sliceIndices;
}

// ── Phase 3F Sub-Epic I closeout, defect H1 ─────────────────────────────────
//
// One stream's drain size. Runs on the DSP thread (queued from RadioModel),
// which is the whole point: the size and that stream's accumulator have to
// change together, and doing both here means no drain can observe one without
// the other.
void RxDspWorker::setStreamInputChunk(int streamIndex, int inSize)
{
    const int defaultInSize = m_inSize.load(std::memory_order_relaxed);

    // Resolve both sides through the same "absent or <= 0 means default" rule
    // the drain loop uses, so "set stream 1 to the value it already resolves
    // to" is correctly seen as a no-op and does not punch a hole in the audio.
    const auto it = m_streamInSize.find(streamIndex);
    const int  previous = (it != m_streamInSize.end() && it->second > 0)
                              ? it->second
                              : defaultInSize;
    const int  resolved = (inSize > 0) ? inSize : defaultInSize;

    if (inSize > 0) {
        m_streamInSize[streamIndex] = inSize;
    } else if (it != m_streamInSize.end()) {
        m_streamInSize.erase(it);
    }

    if (resolved == previous) {
        return;
    }

    // The partial chunk was captured by the DDC at the OLD rate. A WDSP
    // channel carries exactly ONE input rate (SetInputSamplerate,
    // channel.c:197-208 [WDSP v1.29]), so a chunk assembled from both sides
    // of the change cannot be described to it: the carried prefix would be
    // demodulated against the wrong timebase, corrupting the whole chunk
    // rather than clicking at the seam. Thetis reaches the same place from
    // the other direction, draining the channel with SetChannelState(id,0,1)
    // before SetXcmInrate (setup.cs:7010 / 7081 [v2.10.3.15]), and
    // RadioModel::setSampleRateLive already drops every accumulator through
    // resetAccumulator() for the radio-wide case.
    //
    // The cost is bounded by one drain interval, which the Thetis buffer
    // formula pins at inSize / rate = 64 / 48000 s at EVERY rate, so under
    // 1.4 ms of audio on a deliberate operator action.
    auto accIt = m_accums.find(streamIndex);
    if (accIt != m_accums.end()) {
        accIt->second.i.clear();
        accIt->second.q.clear();
    }
}

void RxDspWorker::clearStreamInputChunks()
{
    m_streamInSize.clear();
    // Same reasoning as setStreamInputChunk: every stream's partial chunk was
    // captured against the geometry that just went away.
    for (auto& entry : m_accums) {
        entry.second.i.clear();
        entry.second.q.clear();
    }
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
