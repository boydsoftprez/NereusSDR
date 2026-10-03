// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/RadeChannel.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeChannel implementation. I1 shipped lifecycle
// skeleton; I2 fills in the RX path (24 kHz I/Q -> 8 kHz RADE_COMP ->
// rade_rx -> features -> FARGAN -> 16 kHz mono speech -> 24 kHz stereo
// output bus + syncChanged / snrChanged emission). I3 will fill in
// the TX path; I4 the embedded rade_text aux channel.
//
// Ported from sources (hybrid):
//   Structure: AetherSDR src/core/RADEEngine.cpp [@0cd4559]
//     Ctor / dtor pair, idempotent start()/stop() guards on the
//     active flag, isActive() / isSynced() accessors. See
//     RadeChannel.h for the full AetherSDR-block attribution.
//     I2 adds: rade_open + lpcnet_encoder_create + fargan_init +
//     resampler-chain construction (start, AetherSDR :27-78); the
//     teardown in stop (AetherSDR :80-106); and the RX-path body
//     of processIq (AetherSDR feedRxAudio :200-303).
//   DSP API:   freedv-gui src/pipeline/RADEReceiveStep.cpp,
//              src/pipeline/RADETransmitStep.cpp        [@77e793a]
//     The slot-body call sequences that Tasks I2 / I3 follow
//     (rade_rx with inputBufCplx_ + featuresOut_ + rade_nin frame
//     readiness check; rade_tx with featureList_ + 12-frame
//     accumulation; LPCNet feature extractor lifecycle; FARGAN
//     vocoder warm-up; embedded rade_text aux channel) all come
//     from these two freedv-gui pipeline-step files. The I2 RX path
//     specifically cross-checks against RADEReceiveStep.cpp:175-310.
//
// License (upstream):
//   - AetherSDR has no per-file copyright header, so per
//     docs/attribution/HOW-TO-PORT.md rule 6 we cite the project URL
//     and primary author at NereusSDR block level rather than copying
//     a verbatim header that does not exist:
//       Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//         - per https://github.com/ten9876/AetherSDR (GPLv3; see
//           LICENSE and About dialog for the live contributor list)
//   - freedv-gui carries an LGPLv2.1+ root license (`freedv-gui/COPYING`).
//     The specific RADEReceiveStep.cpp and RADETransmitStep.cpp
//     files each carry a permissive BSD-2-Clause-style file header
//     (Copyright Mooneer Salem, no per-file project copyright line);
//     the BSD permission block is reproduced verbatim below per the
//     upstream redistribution clause. Both files carry the same
//     header text; we reproduce it once.
//
// --- From freedv-gui/src/pipeline/RADEReceiveStep.cpp ---
// --- From freedv-gui/src/pipeline/RADETransmitStep.cpp ---
//
//=========================================================================
// Name:            RADEReceiveStep.cpp
// Purpose:         Describes a demodulation step in the audio pipeline.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave (RADE EOO): queueEndOfOver
//                 encodes held speech first, never waits for the decoder.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE reason: test seam setStartFailsForTest, read at the top of start().
//                                    AI-assisted via Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I1. See
//                 RadeChannel.h for the full attribution block.
//                 Skeleton implementation: lifecycle bodies
//                 (ctor/dtor pair, start() path-exists check +
//                 active-flag flip, stop() active-flag unflip, the
//                 isActive() / isSynced() accessors) ported from
//                 AetherSDR src/core/RADEEngine.cpp:18-124
//                 [@0cd4559]. DSP-bearing slot bodies (processIq /
//                 txEncode / resetTx) are TODO-marked for Phase 3R
//                 Tasks I2 / I3 with inline cites pointing at the
//                 freedv-gui RADEReceiveStep::execute /
//                 RADETransmitStep::execute / RADETransmitStep::reset
//                 line ranges they will follow. AI tooling: Anthropic
//                 Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I2. RX path body
//                 lands. start() now calls rade_initialize +
//                 rade_open + lpcnet_encoder_create + fargan_init +
//                 builds the five-resampler chain (24<->8 with the
//                 second m_down24to8Q for the Q leg, 24<->16);
//                 ported from AetherSDR src/core/RADEEngine.cpp:27-78
//                 [@0cd4559]. stop() unwinds in reverse, ported from
//                 RADEEngine.cpp:80-106. processIq() ports the
//                 feedRxAudio body at RADEEngine.cpp:200-303 with the
//                 NereusSDR-architectural divergence noted in
//                 RadeChannel.h's mod-history: AetherSDR's
//                 processStereoToMono(L,R)+imag=0 path is replaced
//                 with parallel processing of the I leg through
//                 m_down24to8 and the Q leg through m_down24to8Q,
//                 because NereusSDR's input is already complex
//                 baseband from the OpenHPSDR DDC. Cross-checked
//                 against freedv-gui RADEReceiveStep::execute
//                 (src/pipeline/RADEReceiveStep.cpp:175-310
//                 [@77e793a]); freedv-gui's freq_shift_coh step is
//                 not needed because NereusSDR's DDC delivers
//                 baseband directly. txEncode / resetTx slot bodies
//                 remain TODO-marked for I3.
//                 AI tooling: Anthropic Claude Code.
//   2026-09-21  J.J. Boyd / KG4VCF  Restored AetherSDR's input-cadenced
//                 quiet padding for no-timeout multi-slice mixing, with
//                 AI-assisted implementation via OpenAI Codex.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I3. TX path body
//                 lands. txEncode() ports the feedTxAudio body at
//                 AetherSDR src/core/RADEEngine.cpp:134-198 [@0cd4559]
//                 with one NereusSDR-architectural divergence: the
//                 input is already 16 kHz mono int16 (the WdspEngine
//                 TX pump feeds mic samples at that rate per the
//                 plan), so AetherSDR's 24 kHz stereo float -> 16 kHz
//                 mono int16 conversion at :139-152 is dropped and
//                 the input bytes append straight into m_txAccum.
//                 The LPCNet feature extraction
//                 (lpcnet_compute_single_frame_features over
//                 LPCNET_FRAME_SIZE chunks), the NB_TOTAL_FEATURES
//                 feature accumulator, the rade_n_features_in_out-
//                 bounded drain to rade_tx, the RADE_COMP real-leg
//                 take, and the 8 kHz mono -> 24 kHz stereo upsample
//                 via m_up8to24 all follow AetherSDR line-for-line.
//                 resetTx() flushes m_txAccum + m_txFeatAccum +
//                 m_radeTxCallCount per AetherSDR :126-132 [@0cd4559]
//                 cross-checked against freedv-gui
//                 RADETransmitStep::reset (:242-247 [@77e793a]).
//                 Test seams radeTxCallCountForTest /
//                 txFeatureAccumSizeForTest expose m_radeTxCallCount
//                 and m_txFeatAccum.size() so the I3 test suite can
//                 pin when rade_tx() is invoked relative to the
//                 rade_n_features_in_out() threshold and verify
//                 resetTx() actually flushes the feature accumulator.
//                 AI tooling: Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): resetTx()
//                 counts its runs for the resetTxCountForTest() seam.
//                 NereusSDR-original. AI tooling: Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE threads: processIq's body is
//                 decodeRxBlock, run on the channel's own decoder thread
//                 (RadeRxWorker) under m_codecMutex; TX, start and stop
//                 take the same mutex on the main thread, and their
//                 signals leave after it is released. The tick log is
//                 per channel ("channel=<id> tick=<n>"). txEncode encodes
//                 only on the selected (TX slice's) channel.
//                 NereusSDR-original. AI tooling: Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE threads review: txEncode takes the
//                 codec with try_lock and holds a block that finds it busy
//                 for the next call, so the main thread never waits for a
//                 decode; the SNR and offset go out when they change (and
//                 every kRadeMetricsRefreshTicks blocks); the tick log adds
//                 the bridge's silentSlots, playedSlots, lateDrops,
//                 inputDrops and outputDrops. NereusSDR-original. AI
//                 tooling: Anthropic Claude Code.
// =================================================================

#include "core/RadeChannel.h"

#include <QFile>
#include <QLoggingCategory>

#include <algorithm>
#include <cstring>
#include <vector>

// Pull in the NereusSDR-native helper definitions so the
// std::unique_ptr<Resampler> and std::unique_ptr<RadeText> members
// can be destroyed. They are forward-declared in RadeChannel.h to
// keep the freedv-gui / opus include surface out of every callsite.
#include "core/Resampler.h"
#include "core/RadeText.h"
#include "core/RadeRxWorker.h"

extern "C" {
#include "rade_api.h"
#include "lpcnet.h"
#include "fargan.h"
}

Q_LOGGING_CATEGORY(lcRade, "nereus.rade")

namespace NereusSDR {

// Custom deleter for the opaque FARGANState handle held by
// std::unique_ptr<void, FarganDeleter> on RadeChannel. Resolves the
// FARGANState type at the cpp-side include scope so the opus header
// does not bleed into RadeChannel.h. NereusSDR-only refactor of
// AetherSDR's raw-void* m_fargan pattern at RADEEngine.h:8-12 [@0cd4559]
// to comply with the project's "no raw new/delete" rule (CLAUDE.md).
void RadeChannel::FarganDeleter::operator()(void* p) const noexcept {
    delete static_cast<FARGANState*>(p);
}

namespace {

// AetherSDR treats "dummy" as the librade convention for "ignore the
// model_file argument and use the built-in weights"; the radae_nopy
// implementation at rade_api_nopy.c:58-76 [@b289102] confirms model_file
// is logged then discarded. We honor the same sentinel for two reasons:
// (1) it lets the unit tests bypass the path-exists check without a
// fixture file, (2) it matches the call shape we will use in production
// when AppSettings has not yet pointed at a user-selected model.
constexpr const char* kDummyModelSentinel = "dummy";

// r8brain CDSPResampler24's per-process() input-size ceiling. The
// resampler pre-allocates internal buffers at construction; a single
// process() call with more samples than this overruns those buffers
// and triggers glibc heap-corruption on Linux (Ubuntu 24.04 with
// _FORTIFY_SOURCE).  See RadeChannel::start() for the full history,
// and RadeChannel::processIq() for the chunking guard that splits
// oversized inputs into ≤ kRadeResamplerMaxBlock pieces.
constexpr int kRadeResamplerMaxBlock = 16384;

// From freedv-backend src/pipeline/RADETransmitStep.cpp:65-69 [@f02e7e9]
// Additional silence added at the end of the EOO block to ensure that it actually gets
// transmitted out over the air. This was determined experimentally using the FlexRadio
// waveform and OTA testing to be 200ms. Other radios (especially ones directly connected
// to a PC) may not need as long.
constexpr int NUM_SAMPLES_SILENCE = 200 * RADE_MODEM_SAMPLE_RATE / 1000;

// RADE threads review (NereusSDR-original): the most microphone audio
// txEncode holds while the codec is busy decoding, 1 s at 16 kHz int16. A
// decode takes tens of milliseconds, so this is only reached if the codec
// stays busy far longer than one decode; past it a block is dropped and
// counted (txHeldDrops).
constexpr qsizetype kTxHeldMaxBytes = 16000 * qsizetype(sizeof(int16_t));

// RADE threads review (NereusSDR-original): snrChanged and freqOffsetChanged
// go out when either value changes, and otherwise once in this many blocks,
// the tick log's cadence, so a view that starts listening (the applet on a
// newly active slice) still hears the current values. rade_rx runs once per
// modem frame, about every hundred blocks, so the values rarely change
// between two blocks.
constexpr int kRadeMetricsRefreshTicks = 100;

}  // namespace

// From AetherSDR src/core/RADEEngine.cpp:18-25 [@0cd4559]
//   Trivial QObject ctor; dtor calls stop() so a destruction
//   without an explicit stop() still unwinds the RADE handles.
//   The dtor is defined out-of-line here so the forward-declared
//   std::unique_ptr<Resampler>/<RadeText> members can resolve
//   their destructors.
RadeChannel::RadeChannel(QObject* parent)
    : QObject(parent)
    , m_textChannel(std::make_unique<RadeText>())
{
    // RADE end-of-over callsigns: FreeDV's end-of-over frame carries a
    // callsign and no grid square.
    //
    // RADE threads: this connection must stay queued. textDecoded is
    // emitted on the decoder thread inside decodeRxBlock while it holds
    // m_codecMutex (processRxEooBits); `this` lives on the main thread, so
    // AutoConnection queues it and rxTextDecoded's receivers never run
    // under the codec lock. A direct connection would run them there.
    connect(m_textChannel.get(), &RadeText::textDecoded, this,
            [this](const QString& callsign) { emit rxTextDecoded(callsign, QString()); });
}

RadeChannel::~RadeChannel()
{
    // RADE threads: join the decoder thread before the codec it runs goes.
    stopRxWorker();
    stop();
}

// From AetherSDR src/core/RADEEngine.cpp:27-78 [@0cd4559]
//   Wires up the RADE / LPCNet / FARGAN handles and builds the
//   resampler chain. Cleanup-on-failure unwinds in reverse so a
//   partially-initialised wrapper does not leak.
//
//   NereusSDR divergences vs AetherSDR:
//     1. The model_file argument is honored. AetherSDR hard-codes
//        rade_open("dummy", ...). We pass through the caller's path
//        unless it matches kDummyModelSentinel, in which case we
//        bypass the path-exists check.
//     2. Five resamplers instead of four: m_down24to8Q is added so
//        the Q leg of the OpenHPSDR DDC I/Q stream stays separate
//        through the RADE_COMP assembly. AetherSDR averages L+R
//        stereo PCM to mono and sets imag=0.
//     3. m_active is set explicitly to true rather than implied by
//        m_rade being non-null (the I1 skeleton already established
//        the flag-driven contract).
bool RadeChannel::start(const QString& modelPath)
{
    std::lock_guard<std::mutex> lock(m_codecMutex);  // RADE threads
    if (m_active) {
        // Idempotent: a second start() with the same channel already
        // running is a no-op success (matches AetherSDR's `if (m_rade)
        // return true` guard at RADEEngine.cpp:30 [@0cd4559]).
        return true;
    }
#ifdef NEREUS_BUILD_TESTS
    if (m_startFailsForTest) {
        return false;
    }
#endif
    if (modelPath.isEmpty()) {
        return false;
    }
    if (modelPath != QLatin1String(kDummyModelSentinel) && !QFile::exists(modelPath)) {
        return false;
    }

    // From AetherSDR src/core/RADEEngine.cpp:32-49 [@0cd4559]
    rade_initialize();

    const QByteArray modelPathUtf8 = modelPath.toUtf8();
    m_rade = rade_open(const_cast<char*>(modelPathUtf8.constData()),
                       RADE_USE_C_ENCODER | RADE_USE_C_DECODER | RADE_VERBOSE_0);
    if (!m_rade) {
        qCWarning(lcRade) << "RadeChannel: rade_open() failed for" << modelPath;
        rade_finalize();
        return false;
    }

    // TX: LPCNet feature extractor (speech -> features). Constructed
    // at start() time even though the I3 TX path will not be wired
    // until that task lands; mirrors the AetherSDR pattern so
    // start() is the one place that allocates and stop() is the one
    // place that releases.
    m_lpcnetEnc = lpcnet_encoder_create();
    if (!m_lpcnetEnc) {
        qCWarning(lcRade) << "RadeChannel: lpcnet_encoder_create() failed";
        rade_close(m_rade);
        m_rade = nullptr;
        rade_finalize();
        return false;
    }

    // RX: FARGAN vocoder (features -> speech). AetherSDR keeps the
    // FARGAN state as a void* opaque pointer in the header to keep
    // the opus headers out of the include surface; we follow, but
    // wrap in unique_ptr<void, FarganDeleter> per CLAUDE.md's
    // "no raw new/delete" rule. RAII handles teardown in stop().
    auto* fargan = new FARGANState;
    fargan_init(fargan);
    m_fargan.reset(fargan);
    m_farganWarmedUp = false;

    // From AetherSDR src/core/RADEEngine.cpp:58-61 [@0cd4559] plus
    // NereusSDR-architectural addition m_down24to8Q.
    //
    // 2026-05-12 (PR #238 follow-up): maxBlockSamples bumped from the
    // 4096 default to 16384 because r8brain's CDSPResampler24 pre-
    // allocates internal buffers sized for the value at construction;
    // a process() call with > maxBlockSamples in one shot overruns
    // those buffers and trips glibc's heap-corruption sentinel on
    // Linux CI (Ubuntu 24.04 glibc 2.39 with _FORTIFY_SOURCE).
    // RadeChannel accumulates 24 kHz I/Q until rade_nin() worth of
    // 8 kHz samples are ready (typically ~960 samples at the
    // RADE-v1 default — 2880 input @ 24 kHz, well under 16384), so
    // the bumped headroom is purely defensive. Same root cause as
    // the tst_resampler heap corruption fixed in the same commit.
    //
    // 2026-05-13 (Linux CI #238): processIq below now chunks inputs
    // against kRadeResamplerMaxBlock so an oversized input (e.g.
    // tst_rade_channel's 24000-sample stress chunk) no longer
    // overruns the r8brain internal buffer.  Production callers
    // (RxDspWorker) never produce chunks larger than the DSP block
    // size (≤ 2048), but the chunked path is purely defensive and
    // keeps the wrapper safe against any future caller.
    m_down24to8  = std::make_unique<Resampler>(24000, 8000,  kRadeResamplerMaxBlock);
    m_down24to8Q = std::make_unique<Resampler>(24000, 8000,  kRadeResamplerMaxBlock);
    m_up8to24    = std::make_unique<Resampler>(8000,  24000, kRadeResamplerMaxBlock);
    m_down24to16 = std::make_unique<Resampler>(24000, 16000, kRadeResamplerMaxBlock);
    m_up16to24   = std::make_unique<Resampler>(16000, 24000, kRadeResamplerMaxBlock);

    // From AetherSDR src/core/RADEEngine.cpp:63-67 [@0cd4559]
    m_txAccum.clear();
    m_txFeatAccum.clear();
    m_rxAccum.clear();
    m_rxFeatAccum.clear();
    m_rxOutAccum.clear();
    m_synced = false;
    m_radeRxCallCount = 0;
    m_radeTxCallCount = 0;
    m_endOfOverQueued = false;
    m_metricsSent = false;  // RADE threads review

    // From AetherSDR src/core/RADEEngine.cpp:68-72 [@0cd4559]
    const int n_features = rade_n_features_in_out(m_rade);
    const int n_tx_out   = rade_n_tx_out(m_rade);
    const int nin        = rade_nin(m_rade);
    qCInfo(lcRade) << "RadeChannel: started"
                   << "n_features=" << n_features
                   << "n_tx_out=" << n_tx_out
                   << "nin=" << nin;

    m_active = true;
    return true;
}

// From AetherSDR src/core/RADEEngine.cpp:80-106 [@0cd4559]
//   Unwinds start() in reverse. Idempotent on the m_active flag so
//   double-stop is safe. The dtor calls stop() so any RadeChannel
//   that was started but not stopped still tears down before the
//   wrapper goes out of scope.
void RadeChannel::stop()
{
    std::lock_guard<std::mutex> lock(m_codecMutex);  // RADE threads
    if (!m_active) {
        return;
    }

    if (m_lpcnetEnc) {
        lpcnet_encoder_destroy(m_lpcnetEnc);
        m_lpcnetEnc = nullptr;
    }
    // RAII: unique_ptr<void, FarganDeleter> handles deletion of the
    // FARGANState through the cpp-scope deleter.
    m_fargan.reset();
    if (m_rade) {
        rade_close(m_rade);
        m_rade = nullptr;
        rade_finalize();
    }

    m_down24to8.reset();
    m_down24to8Q.reset();
    m_up8to24.reset();
    m_down24to16.reset();
    m_up16to24.reset();

    m_txAccum.clear();
    m_txFeatAccum.clear();
    m_rxAccum.clear();
    m_rxFeatAccum.clear();
    m_rxOutAccum.clear();

    m_txHeld.clear();  // RADE threads review
    m_metricsSent = false;
    m_active = false;
    m_synced = false;
    m_farganWarmedUp = false;
    m_radeRxCallCount = 0;
    m_radeTxCallCount = 0;
    m_endOfOverQueued = false;
    m_endOfOverPending = false;  // fix wave (RADE EOO)
    m_pendingEndOfOverSent = {};

    qCInfo(lcRade) << "RadeChannel: stopped";
}

// From AetherSDR src/core/RADEEngine.cpp:108-115 [@0cd4559]
bool RadeChannel::isActive() const
{
    return m_active;
}

// From AetherSDR src/core/RADEEngine.cpp:117-124 [@0cd4559]
bool RadeChannel::isSynced() const
{
    return m_synced;
}

// NereusSDR-native hook: sideband selection for RADE_U / RADE_L.
// Stored on the channel at the v0.5.0 sideband-split fix-up; not yet
// consumed by the I/Q routing layer.  K-bench follow-up will wire the
// stored value into any future spectral mirroring at the TX modulator
// stage.
void RadeChannel::setSideband(bool upper)
{
    m_sidebandUpper = upper;
}

bool RadeChannel::sidebandUpper() const
{
    return m_sidebandUpper;
}

int RadeChannel::radeRxCallCountForTest() const
{
    return m_radeRxCallCount;
}

int RadeChannel::radeTxCallCountForTest() const
{
    return m_radeTxCallCount;
}

int RadeChannel::txFeatureAccumSizeForTest() const
{
    std::lock_guard<std::mutex> lock(m_codecMutex);  // RADE threads
    return static_cast<int>(m_txFeatAccum.size());
}

// ── RADE threads (2026-09-30), NereusSDR-original ───────────────────────────

void RadeChannel::startRxWorker()
{
    if (!m_rxWorker) {
        m_rxWorker = std::make_unique<RadeRxWorker>(
            [this](const QByteArray& iq) { return decodeRxBlock(iq); });
    }
    m_rxWorker->setGated(m_rxGated.load(std::memory_order_acquire));
    m_rxWorker->start(QStringLiteral("RadeRx%1").arg(channelId()), channelId());
}

void RadeChannel::stopRxWorker()
{
    if (m_rxWorker) {
        m_rxWorker->stop();
        m_rxWorker.reset();
    }
}

bool RadeChannel::rxWorkerRunning() const
{
    return m_rxWorker && m_rxWorker->isRunning();
}

std::shared_ptr<RadeRxBridge> RadeChannel::rxBridge() const
{
    return rxWorkerRunning() ? m_rxWorker->bridge() : nullptr;
}

void RadeChannel::setRxGated(bool gated)
{
    m_rxGated.store(gated, std::memory_order_release);
    if (m_rxWorker) {
        m_rxWorker->setGated(gated);
    }
}

#ifdef NEREUS_BUILD_TESTS
bool RadeChannel::waitRxIdleForTest(int timeoutMs)
{
    return m_rxWorker ? m_rxWorker->waitIdleForTest(timeoutMs) : true;
}

void RadeChannel::setRxStallHookForTest(std::function<void()> hook)
{
    if (m_rxWorker) {
        m_rxWorker->setBeforeDecodeHookForTest(std::move(hook));
    }
}

void RadeChannel::setRxDecodeLockedHookForTest(std::function<void()> hook)
{
    std::lock_guard<std::mutex> lock(m_rxLockedHookMutex);
    m_rxLockedHook = std::move(hook);
}

Qt::HANDLE RadeChannel::rxThreadIdForTest() const
{
    return m_rxWorker ? m_rxWorker->threadIdForTest() : nullptr;
}

QString RadeChannel::rxThreadNameForTest() const
{
    return m_rxWorker ? m_rxWorker->threadName() : QString();
}
#endif

// From AetherSDR src/core/RADEEngine.cpp:200-303 (feedRxAudio body)
// [@0cd4559], cross-checked against freedv-gui
// src/pipeline/RADEReceiveStep.cpp:175-310 [@77e793a].
//
// Pipeline:
//   1. Deinterleave 24 kHz interleaved I/Q float input.
//   2. Downsample I leg via m_down24to8 and Q leg via m_down24to8Q
//      in parallel (NereusSDR divergence; AetherSDR averages L+R
//      stereo PCM to mono and sets imag=0).
//   3. Interleave I and Q outputs into RADE_COMP samples and append
//      to m_rxAccum.
//   4. While m_rxAccum has >= rade_nin() RADE_COMP samples, drain a
//      chunk and call rade_rx; append decoded features to
//      m_rxFeatAccum.
//   5. While m_rxFeatAccum has >= NB_TOTAL_FEATURES worth of features,
//      warm up FARGAN on first run, then call fargan_synthesize for
//      LPCNET_FRAME_SIZE samples of 16 kHz mono speech; append to
//      a local speech16k QByteArray.
//   6. Upsample speech16k via m_up16to24 to 24 kHz stereo and append
//      to m_rxOutAccum.
//   7. If m_rxOutAccum has accumulated at least the input chunk's
//      byte size, emit rxSpeechReady with that prefix and remove it
//      from the accumulator; otherwise emit a silence chunk of the
//      same size so the speaker bus stays paced.
//   8. Sample rade_sync() / rade_snrdB_3k_est() / rade_freq_offset()
//      and emit syncChanged on a state transition + snrChanged +
//      freqOffsetChanged when synced.
void RadeChannel::processIq(const QByteArray& iqSamples)
{
    // RADE threads: the body is decodeRxBlock; this slot keeps the direct
    // call surface (tests, and any caller on one thread).
    decodeRxBlock(iqSamples);
}

QByteArray RadeChannel::decodeRxBlock(const QByteArray& iqSamples)
{
    // Fix wave (RADE EOO): an end-of-over that found the codec busy is sent
    // on this object's thread once this decode has let go of the codec, on
    // every return path. Constructed before the lock, so it runs after the
    // lock is released.
    struct EndOfOverWake {
        RadeChannel* self;
        ~EndOfOverWake()
        {
            if (self->m_endOfOverPending.load(std::memory_order_seq_cst)) {
                QMetaObject::invokeMethod(self, [s = self]() { s->runPendingEndOfOver(); },
                                          Qt::QueuedConnection);
            }
        }
    } endOfOverWake{this};
    // RADE threads: what to emit once m_codecMutex is released.
    QByteArray speechOut;
    bool syncEdge = false;
    bool syncedNow = false;
    bool haveMetrics = false;
    float snrOut = 0.0f;
    float foffOut = 0.0f;
    {
    std::lock_guard<std::mutex> lock(m_codecMutex);

    // BENCH DEBUG: one-shot logs to confirm RX I/Q reaches the codec
    // through the queued-invocation path from RxDspWorker.
    // RADE threads: atomic, as several decoder threads may pass here.
    static std::atomic<int> s_rxProcessIqCount{0};
    if (s_rxProcessIqCount.load(std::memory_order_relaxed) < 3) {
        qCInfo(lcRade).noquote()
            << QString("RadeChannel::processIq #%1 bytes=%2 active=%3 "
                       "rade=%4 fargan=%5")
                .arg(s_rxProcessIqCount.load() + 1)
                .arg(iqSamples.size())
                .arg(m_active.load())
                .arg(m_rade != nullptr)
                .arg(m_fargan != nullptr);
        ++s_rxProcessIqCount;
    }
    if (!m_active || !m_rade || !m_fargan) {
        return QByteArray();
    }
#ifdef NEREUS_BUILD_TESTS
    {
        std::function<void()> hook;
        {
            std::lock_guard<std::mutex> hookLock(m_rxLockedHookMutex);
            hook = m_rxLockedHook;
        }
        if (hook) {
            hook();  // the codec is held busy while this runs
        }
    }
#endif

    auto* fargan = static_cast<FARGANState*>(m_fargan.get());

    // Step 1: deinterleave 24 kHz interleaved I/Q float input.
    // The QByteArray holds an integer number of (I, Q) float pairs.
    const int kStereoFrameBytes = 2 * static_cast<int>(sizeof(float));
    const int nFrames = iqSamples.size() / kStereoFrameBytes;
    if (nFrames <= 0) {
        return QByteArray();
    }

    std::vector<float> iLeg(nFrames);
    std::vector<float> qLeg(nFrames);
    {
        const auto* src = reinterpret_cast<const float*>(iqSamples.constData());
        for (int i = 0; i < nFrames; ++i) {
            iLeg[i] = src[2 * i];
            qLeg[i] = src[2 * i + 1];
        }
    }

    // Step 2: downsample I and Q legs in parallel to 8 kHz.
    //
    // 2026-05-13 (Linux CI #238 bug 3): chunk against the resampler's
    // pre-allocated input buffer ceiling (kRadeResamplerMaxBlock).
    // r8brain's CDSPResampler24 overruns its internal buffers if a
    // single process() call exceeds the maxBlockSamples it was
    // constructed with -- harmless garbage read on macOS arm64,
    // glibc heap-corruption / segfault on Linux x64 (Ubuntu 24.04
    // _FORTIFY_SOURCE).  Production callers (RxDspWorker emits
    // ≤ 2048-sample chunks) never hit this, but the test fixture
    // tst_rade_channel::processIqAccumulatesAcrossMultipleChunks
    // intentionally pushes a 24 000-sample chunk to verify the
    // accumulator drain path.  Splitting the input keeps the
    // resampler's state continuous (r8brain handles per-call edges
    // via its delay-line) so output is identical to a single call.
    QByteArray iOut8k;
    QByteArray qOut8k;
    for (int offset = 0; offset < nFrames; offset += kRadeResamplerMaxBlock) {
        const int chunk = std::min(kRadeResamplerMaxBlock, nFrames - offset);
        iOut8k.append(m_down24to8->process(iLeg.data() + offset, chunk));
        qOut8k.append(m_down24to8Q->process(qLeg.data() + offset, chunk));
    }
    const int nI = iOut8k.size() / static_cast<int>(sizeof(float));
    const int nQ = qOut8k.size() / static_cast<int>(sizeof(float));
    const int nComp = std::min(nI, nQ);

    // Step 3: assemble RADE_COMP samples (I leg -> real, Q leg ->
    // imag) and append to the RX accumulator. From AetherSDR
    // src/core/RADEEngine.cpp:217-223 [@0cd4559] with the
    // NereusSDR-architectural Q-from-imag-leg divergence.
    {
        const auto* iSamples = reinterpret_cast<const float*>(iOut8k.constData());
        const auto* qSamples = reinterpret_cast<const float*>(qOut8k.constData());
        for (int i = 0; i < nComp; ++i) {
            RADE_COMP c;
            c.real = iSamples[i];
            c.imag = qSamples[i];
            m_rxAccum.append(reinterpret_cast<const char*>(&c), sizeof(RADE_COMP));
        }
    }

    // Step 4: drain rade_nin()-sized chunks through rade_rx.
    // From AetherSDR src/core/RADEEngine.cpp:226-270 [@0cd4559].
    QByteArray speech16k;
    int nin = rade_nin(m_rade);
    while (m_rxAccum.size() >= static_cast<int>(nin * sizeof(RADE_COMP))) {
        const int n_features_out = rade_n_features_in_out(m_rade);
        std::vector<float> features_out(n_features_out);
        int has_eoo = 0;
        const int n_eoo_bits = rade_n_eoo_bits(m_rade);
        std::vector<float> eoo_out(n_eoo_bits);

        auto* rx_in = reinterpret_cast<RADE_COMP*>(m_rxAccum.data());
        const int n_out = rade_rx(m_rade, features_out.data(), &has_eoo,
                                  eoo_out.data(), rx_in);
        ++m_radeRxCallCount;

        // Remove consumed samples.
        m_rxAccum.remove(0, nin * sizeof(RADE_COMP));

        // From freedv-backend src/pipeline/RADEReceiveStep.cpp:233-242
        // [@f02e7e9]: an EOO frame carries no speech features; its data
        // goes to the text channel (rade_text_rx there, RadeText here).
        if (has_eoo) {
            // Handle RX of bits from EOO.  [original inline comment from RADEReceiveStep.cpp:238]
            m_textChannel->processRxEooBits(eoo_out.data(), n_eoo_bits);
            nin = rade_nin(m_rade);
            continue;
        }

        // Step 5: feed features to FARGAN.
        // From AetherSDR src/core/RADEEngine.cpp:240-267 [@0cd4559].
        if (n_out > 0) {
            m_rxFeatAccum.append(reinterpret_cast<const char*>(features_out.data()),
                                 sizeof(float) * n_out);
        }

        while (m_rxFeatAccum.size() >=
               qsizetype(sizeof(float) * NB_TOTAL_FEATURES)) {
            // FARGAN warmup: feed zeros once on first frame so the
            // recurrent state initialises. From AetherSDR
            // src/core/RADEEngine.cpp:247-254 [@0cd4559].
            if (!m_farganWarmedUp) {
                float zeros[320] = {0};
                float warmup_features[5 * NB_TOTAL_FEATURES] = {0};
                fargan_cont(fargan, zeros, warmup_features);
                m_farganWarmedUp = true;
            }

            const float* feat =
                reinterpret_cast<const float*>(m_rxFeatAccum.constData());
            float fpcm[LPCNET_FRAME_SIZE];
            fargan_synthesize(fargan, fpcm, feat);

            speech16k.append(reinterpret_cast<const char*>(fpcm),
                             LPCNET_FRAME_SIZE * sizeof(float));

            m_rxFeatAccum.remove(0, sizeof(float) * NB_TOTAL_FEATURES);
        }

        nin = rade_nin(m_rade);
    }

    // Step 6: upsample 16 kHz mono speech to 24 kHz stereo.
    // From AetherSDR src/core/RADEEngine.cpp:272-277 [@0cd4559].
    if (!speech16k.isEmpty()) {
        QByteArray tmp = m_up16to24->processMonoToStereo(
            reinterpret_cast<const float*>(speech16k.constData()),
            speech16k.size() / static_cast<int>(sizeof(float)));
        m_rxOutAccum.append(tmp);
    }

    // Step 7: emit a rxSpeechReady chunk that matches the input
    // chunk's byte size so the downstream speaker bus stays paced.
    // From AetherSDR src/core/RADEEngine.cpp:279-288 [@0cd4559].
    //
    // NereusSDR divergence: AetherSDR uses the input PCM byte count
    // (int16 stereo); our input is float32 I/Q. To preserve the
    // pacing semantics we compute the equivalent float32 stereo
    // output byte count for the same frame count.
    const int outBytesPerFrame = 2 * static_cast<int>(sizeof(float));
    const int outChunkBytes = nFrames * outBytesPerFrame;
    if (m_rxOutAccum.size() >= outChunkBytes) {
        speechOut = m_rxOutAccum.left(outChunkBytes);  // RADE threads: emitted below
        m_rxOutAccum.remove(0, outChunkBytes);
    } else {
        // Preserve one output event per accepted input block while the neural
        // decoder is warming or unsynchronised. MasterMixer is a readiness
        // barrier with no timeout, so omitting this block after the RADE
        // slice has joined would stall every ordinary co-hosted slice.
        // AetherSDR RADEEngine.cpp:496-504 [@0dea0dd7] uses the same-sized
        // zero pad for this exact accumulator-short case.
        speechOut = QByteArray(outChunkBytes, '\0');  // RADE threads: emitted below
    }

    // Step 8: sample sync / SNR / freq-offset.
    // From AetherSDR src/core/RADEEngine.cpp:290-298 [@0cd4559].
    const bool synced = rade_sync(m_rade) != 0;

    // BENCH DEBUG: log SNR + sync state every 100 rade_rx calls so we
    // can see whether the codec's internal detector is seeing anything
    // on the air even when sync isn't held. Helps distinguish:
    //   (a) signal present but at wrong sideband / freq (SNR fluctuates)
    //   (b) no signal at all (SNR stays at floor / NaN)
    //
    // RADE threads (2026-09-30): the count was one static shared by every
    // channel, so with two RADE slices the rate doubled and no line said
    // whose it was. Now each channel counts its own and names itself.
    const int tick = m_rxTickCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (tick % 100 == 1) {
        const float snr = static_cast<float>(rade_snrdB_3k_est(m_rade));
        const float foff = static_cast<float>(rade_freq_offset(m_rade));
        // RADE threads review: the decoder's slot and drop counters, to
        // watch a slow decoder on the Rock. The worker runs this, so its
        // bridge is there; a direct processIq caller has none.
        const std::shared_ptr<RadeRxBridge> bridge =
            m_rxWorker ? m_rxWorker->bridge() : nullptr;
        qCInfo(lcRade).noquote()
            << QString("RadeChannel: channel=%1 tick=%2 silentSlots=%3 "
                       "playedSlots=%4 lateDrops=%5 inputDrops=%6 "
                       "outputDrops=%7 synced=%8 SNR=%9 dB")
                .arg(channelId())
                .arg(tick)
                .arg(bridge ? bridge->silentSlots() : 0)
                .arg(bridge ? bridge->playedSlots() : 0)
                .arg(bridge ? bridge->lateDrops() : 0)
                .arg(bridge ? bridge->inputDrops() : 0)
                .arg(bridge ? bridge->outputDrops() : 0)
                .arg(synced ? "YES" : "no")
                .arg(snr, 0, 'f', 1)
            << QString("freqOff=%1 Hz sideband=%2")
                .arg(foff, 0, 'f', 0)
                .arg(m_sidebandUpper ? "USB" : "LSB");
    }

    if (synced != m_synced) {
        m_synced = synced;
        qCInfo(lcRade) << "RadeChannel: rade_sync transition ->"
                       << (synced ? "SYNCED" : "UNSYNCED");
        syncEdge = true;
        syncedNow = synced;
    }
    if (synced) {
        snrOut = static_cast<float>(rade_snrdB_3k_est(m_rade));
        foffOut = static_cast<float>(rade_freq_offset(m_rade));
        // RADE threads review: only a changed pair, a new lock, or the
        // refresh cadence sends them; an unsent pair equals the last sent.
        haveMetrics = !m_metricsSent || syncEdge
            || snrOut != m_lastSnrSent || foffOut != m_lastFoffSent
            || tick - m_lastMetricsTick >= kRadeMetricsRefreshTicks;
        if (haveMetrics) {
            m_metricsSent = true;
            m_lastSnrSent = snrOut;
            m_lastFoffSent = foffOut;
            m_lastMetricsTick = tick;
        }
    }
    }  // RADE threads: m_codecMutex released

    emit rxSpeechReady(speechOut);
    if (syncEdge) {
        emit syncChanged(syncedNow);
    }
    if (haveMetrics) {
        m_snrEmitCount.fetch_add(1, std::memory_order_relaxed);
        emit snrChanged(snrOut);
        emit freqOffsetChanged(foffOut);
    }
    return speechOut;
}

// From AetherSDR src/core/RADEEngine.cpp:134-198 (feedTxAudio body)
// [@0cd4559], cross-checked against freedv-gui
// src/pipeline/RADETransmitStep.cpp:150-260 [@77e793a].
//
// Pipeline:
//   1. NereusSDR divergence vs AetherSDR: input is already 16 kHz mono
//      int16 per the wrapper's contract (the WdspEngine TX pump feeds
//      mic samples at 16 kHz mono). AetherSDR's :139-152 step that
//      downmixes 24 kHz stereo float -> 16 kHz mono int16 is therefore
//      dropped; the input bytes are appended directly to m_txAccum.
//   2. Drain m_txAccum in LPCNET_FRAME_SIZE-sample chunks. For each
//      chunk, call lpcnet_compute_single_frame_features to produce
//      NB_TOTAL_FEATURES floats. Append to m_txFeatAccum.
//   3. While m_txFeatAccum holds >= rade_n_features_in_out() floats,
//      drain a chunk and call rade_tx, producing rade_n_tx_out()
//      RADE_COMP samples at 8 kHz.
//   4. Convert RADE_COMP -> 8 kHz mono float32 by taking the .real
//      component (AetherSDR :184-187).
//   5. Upsample 8 kHz mono float32 -> 24 kHz stereo float32 via
//      m_up8to24->processMonoToStereo.
//   6. Emit txModemReady with the stereo 24 kHz float32 chunk.
void RadeChannel::txEncode(const QByteArray& speechSamples)
{
    // RADE threads: only the TX slice's channel encodes the microphone.
    if (!m_txSelected.load(std::memory_order_acquire)) {
        return;
    }
    // RADE end-of-over callsigns: once the end-of-over frame is queued the
    // over is ending; FreeDV takes no more microphone audio then
    // (freedv-gui src/main.cpp:3986 [@a4ae053], `if (!endingTx...)`).
    // RADE threads review: read before the codec is taken (it is atomic).
    if (m_endOfOverQueued.load(std::memory_order_acquire) || speechSamples.isEmpty()) {
        return;
    }
    // RADE threads: modem chunks leave after m_codecMutex is released, in
    // order, before this call returns (as before).
    std::vector<QByteArray> modemOut;
    {
    // RADE threads review: never wait for the decoder thread. The same
    // rade handle serves rade_rx and rade_tx, so the two never run at once;
    // a block that finds the codec busy is held, in order, for the next
    // call rather than waiting out the decode (tens of milliseconds on the
    // Rock).
    std::unique_lock<std::mutex> lock(m_codecMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        m_txCodecBusyCount.fetch_add(1, std::memory_order_relaxed);
        if (m_txHeld.size() + speechSamples.size() <= kTxHeldMaxBytes) {
            m_txHeld.append(speechSamples);
        } else {
            m_txHeldDrops.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
    // BENCH DEBUG: one-shot first-receive log + gate-rejection trace so
    // we can see (a) whether the queued radeMicBlockReady is reaching
    // txEncode at all, and (b) why it might bail out (m_active /
    // m_rade / m_lpcnetEnc null after start()).
    static int s_txEncodeFirstLogged = 0;
    if (s_txEncodeFirstLogged < 3) {
        qCInfo(lcRade)
            << "txEncode call #" << (s_txEncodeFirstLogged + 1)
            << "bytes=" << speechSamples.size()
            << "active=" << m_active.load()
            << "rade=" << (m_rade != nullptr)
            << "lpcnet=" << (m_lpcnetEnc != nullptr);
        ++s_txEncodeFirstLogged;
    }

    if (!m_active || !m_rade || !m_lpcnetEnc) {
        m_txHeld.clear();
        return;
    }

    // Step 1: append the int16 mono 16 kHz input straight into the
    // TX accumulator. NereusSDR divergence vs AetherSDR (which
    // converts 24 kHz stereo float -> 16 kHz mono int16 at :139-152);
    // our input is already in the LPCNet-ready format.
    // RADE threads review: blocks held while the codec was busy go first.
    if (!m_txHeld.isEmpty()) {
        m_txAccum.append(m_txHeld);
        m_txHeld.clear();
    }
    m_txAccum.append(speechSamples);

    // Steps 2-6 (fix wave, RADE EOO: shared with queueEndOfOver).
    encodeSpeechLocked(modemOut);
    }  // RADE threads: m_codecMutex released
    for (const QByteArray& chunk : modemOut) {
        emit txModemReady(chunk);
    }
}

// Fix wave (RADE EOO): txEncode's steps 2-6, unchanged, so queueEndOfOver
// can encode the speech recorded before the release ahead of the EOO.
// m_codecMutex is held by the caller; blocks held while the codec was busy
// go first.
void RadeChannel::encodeSpeechLocked(std::vector<QByteArray>& modemOut)
{
    if (!m_rade || !m_lpcnetEnc || !m_up8to24) {
        m_txHeld.clear();
        return;
    }
    if (!m_txHeld.isEmpty()) {
        m_txAccum.append(m_txHeld);
        m_txHeld.clear();
    }

    // Step 2: process LPCNet 10 ms frames (LPCNET_FRAME_SIZE = 160
    // samples at 16 kHz). From AetherSDR src/core/RADEEngine.cpp
    // :154-170 [@0cd4559].
    while (static_cast<qsizetype>(m_txAccum.size() / sizeof(int16_t)) >=
           qsizetype(LPCNET_FRAME_SIZE)) {
        QByteArray sampleArray =
            m_txAccum.left(LPCNET_FRAME_SIZE * sizeof(int16_t));
        const int16_t* samples =
            reinterpret_cast<const int16_t*>(sampleArray.constData());
        m_txAccum.remove(0, sampleArray.size());

        // Extract features for one 10 ms frame. The opus header
        // declares the pcm argument as const opus_int16*, but
        // AetherSDR casts away const for historical reasons; we
        // mirror that cast to stay byte-for-byte compatible.
        float features[NB_TOTAL_FEATURES];
        lpcnet_compute_single_frame_features(
            m_lpcnetEnc,
            const_cast<int16_t*>(samples),
            features,
            0 /*arch=auto*/);

        // Accumulate NB_TOTAL_FEATURES floats per frame.
        m_txFeatAccum.append(reinterpret_cast<const char*>(features),
                             NB_TOTAL_FEATURES * sizeof(float));

        // Step 3: drain rade_n_features_in_out-sized chunks.
        // From AetherSDR src/core/RADEEngine.cpp:172-193 [@0cd4559].
        const int n_features_in = rade_n_features_in_out(m_rade);
        const int n_tx_out      = rade_n_tx_out(m_rade);
        while (static_cast<qsizetype>(m_txFeatAccum.size() / sizeof(float)) >=
               qsizetype(n_features_in)) {
            std::vector<RADE_COMP> tx_out(n_tx_out);

            rade_tx(m_rade,
                    tx_out.data(),
                    reinterpret_cast<float*>(m_txFeatAccum.data()));
            ++m_radeTxCallCount;

            m_txFeatAccum.remove(0, n_features_in * sizeof(float));

            // Step 4: convert RADE_COMP -> 8 kHz mono float32 by
            // taking the real component. From AetherSDR
            // src/core/RADEEngine.cpp:183-187 [@0cd4559].
            QByteArray modem8k(n_tx_out * static_cast<int>(sizeof(float)),
                               Qt::Uninitialized);
            auto* out = reinterpret_cast<float*>(modem8k.data());
            for (int i = 0; i < n_tx_out; ++i) {
                out[i] = tx_out[i].real;
            }

            // Step 5: upsample 8 kHz mono -> 24 kHz stereo float32.
            // From AetherSDR src/core/RADEEngine.cpp:189-190 [@0cd4559].
            QByteArray stereo24k = m_up8to24->processMonoToStereo(
                out, n_tx_out);

            // Step 6: emit the encoded modem chunk. From AetherSDR
            // src/core/RADEEngine.cpp:192 [@0cd4559].
            modemOut.push_back(stereo24k);  // RADE threads: emitted below
        }
    }
}

// From AetherSDR src/core/RADEEngine.cpp:126-132 (resetTx body)
// [@0cd4559], cross-checked against freedv-gui
// src/pipeline/RADETransmitStep.cpp:242-247 (RADETransmitStep::reset)
// [@77e793a]. Clears the speech and feature accumulators on MOX
// release so a partial frame from the previous over does not bleed
// into the next over. AetherSDR clears m_txAccum + m_txFeatAccum;
// freedv-gui additionally resets its input/output FIFOs and zeros
// featureListIdx_. Our QByteArray accumulators are the equivalent
// surface; the LPCNet encoder itself is stateful but does not have
// a documented reset entry-point in the librade build, so we leave
// the encoder state alone (matching AetherSDR; the next start()
// fully reallocates the encoder anyway).
void RadeChannel::resetTx()
{
    dropTxAudio();
    ++m_resetTxCountForTest;  // R-R3-49 (parity Task 3): test seam only
}

// NereusSDR: the flush resetTx does, without counting as the Reset vocoder
// action; RadioModel runs it at every unkey and after an end-of-over tail.
void RadeChannel::dropTxAudio()
{
    std::lock_guard<std::mutex> lock(m_codecMutex);  // RADE threads
    m_txAccum.clear();
    m_txHeld.clear();  // RADE threads review: same thread as txEncode
    m_txFeatAccum.clear();
    m_radeTxCallCount = 0;
    m_endOfOverQueued = false;  // RADE end-of-over callsigns: a new over
    // Fix wave (RADE EOO): a deferred end-of-over goes with the over.
    m_endOfOverPending = false;
    m_pendingEndOfOverSent = {};
    // NereusSDR: the 8 -> 24 kHz stage holds about 300 ms of this over's
    // modem audio; drop it so none of it starts the next over.
    if (m_up8to24) {
        m_up8to24->clear();
    }
}

int RadeChannel::endOfOverSamples8k() const
{
    std::lock_guard<std::mutex> lock(m_codecMutex);  // RADE threads
    if (!m_active || !m_rade) {
        return 0;
    }
    return rade_n_tx_eoo_out(m_rade) + NUM_SAMPLES_SILENCE
           + (m_up8to24 ? m_up8to24->latencyInputSamples() : 0);
}

// From freedv-backend src/pipeline/RADETransmitStep.cpp:248-271 [@f02e7e9]
// (RADETransmitStep::restartVocoder), with the callsign set first as
// freedv-gui src/freedv_interface.cpp:697-711 [@a4ae053] (setReliableText)
// does:
//   // Queues up EOO for return on the next call to this pipeline step.
//   rade_tx_eoo(dv_, eooOut_);
//   memset(eooOutShort_, 0, sizeof(short) * (numEOOSamples + NUM_SAMPLES_SILENCE));
//   for (int index = 0; index < numEOOSamples; index++)
//       eooOutShort_[index] = eooOut_[index].real * RADE_SCALING_FACTOR;
//   outputSampleFifo_.write(eooOutShort_, numEOOSamples + NUM_SAMPLES_SILENCE)
// NereusSDR follows txEncode's path for the samples (the real leg as
// float, 8 -> 24 kHz stereo, txModemReady) instead of scaling to int16 for
// a sound card.
bool RadeChannel::queueEndOfOver(const QString& callsign, std::function<void()> onSent)
{
    if (!m_active.load(std::memory_order_acquire)) {
        return false;
    }
    m_pendingEndOfOverCallsign = callsign;
    m_pendingEndOfOverSent = std::move(onSent);
    // Nothing more is encoded in this over (txEncode checks this flag).
    m_endOfOverQueued = true;
    // Fix wave (RADE EOO): set before the codec is tried, so a decode that
    // holds it now sees the flag once it lets go and queues the send.
    m_endOfOverPending.store(true, std::memory_order_seq_cst);
    switch (runPendingEndOfOver()) {
    case EndOfOverRun::Failed:
        m_endOfOverQueued = false;
        return false;
    case EndOfOverRun::Busy:
        qCInfo(lcRade) << "RadeChannel: end-of-over waits for the decoder";
        return true;
    case EndOfOverRun::Sent:
    case EndOfOverRun::NothingPending:
        return true;
    }
    return true;
}

RadeChannel::EndOfOverRun RadeChannel::runPendingEndOfOver()
{
    if (!m_endOfOverPending.load(std::memory_order_seq_cst)) {
        return EndOfOverRun::NothingPending;
    }
    // RADE threads: the modem audio leaves after m_codecMutex is released.
    std::vector<QByteArray> modemOut;
    QString callsign;
    {
        // Fix wave (RADE EOO): never wait for the decoder on this thread.
        // A busy codec leaves the send pending; decodeRxBlock queues
        // another try once it releases the codec.
        std::unique_lock<std::mutex> lock(m_codecMutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            return EndOfOverRun::Busy;
        }
        if (!m_endOfOverPending.exchange(false, std::memory_order_seq_cst)) {
            return EndOfOverRun::NothingPending;
        }
        callsign = m_pendingEndOfOverCallsign;
        if (!m_active || !m_rade || !m_up8to24) {
            m_pendingEndOfOverSent = {};
            return EndOfOverRun::Failed;
        }

        // Fix wave (RADE EOO): the speech recorded before the release goes
        // out ahead of the EOO, whole LPCNet frames only.
        // From freedv-gui src/pipeline/TxRxThread.cpp:808-811 [@a4ae053]:
        //   // There may be recorded audio left to encode while ending TX. To handle this,
        //   // we keep reading from the FIFO until we have less than nsam_in_48 samples available.
        // A partial frame left over is dropped with the over (resetTx), as
        // FreeDV leaves it in its FIFO.
        encodeSpeechLocked(modemOut);

        m_textChannel->setOurCallsign(callsign);
        m_textChannel->pushTxCallsign(m_rade);

        const int numEOOSamples = rade_n_tx_eoo_out(m_rade);
        if (numEOOSamples <= 0) {
            m_pendingEndOfOverSent = {};
            return EndOfOverRun::Failed;
        }
        std::vector<RADE_COMP> eooOut(static_cast<size_t>(numEOOSamples));
        rade_tx_eoo(m_rade, eooOut.data());

        // NereusSDR: the 8 -> 24 kHz resampler holds back its latency
        // (about 300 ms of input), more than the 200 ms of silence, so that
        // many more zeros follow to bring the EOO and all of the silence
        // out. FreeDV has no resampler at this point.
        const int total = numEOOSamples + NUM_SAMPLES_SILENCE + m_up8to24->latencyInputSamples();
        std::vector<float> modem8k(static_cast<size_t>(total), 0.0f);
        for (int index = 0; index < numEOOSamples; index++) {
            modem8k[static_cast<size_t>(index)] = eooOut[static_cast<size_t>(index)].real;
        }

        QByteArray stereo24k;
        for (int offset = 0; offset < total; offset += kRadeResamplerMaxBlock) {
            const int chunk = std::min(kRadeResamplerMaxBlock, total - offset);
            stereo24k.append(m_up8to24->processMonoToStereo(modem8k.data() + offset, chunk));
        }
        if (!stereo24k.isEmpty()) {
            modemOut.push_back(stereo24k);
        }
        qCInfo(lcRade) << "RadeChannel: end-of-over frame queued"
                       << "speechChunks=" << (modemOut.size() - (stereo24k.isEmpty() ? 0 : 1))
                       << "eooSamples=" << numEOOSamples
                       << "silenceSamples=" << NUM_SAMPLES_SILENCE
                       << "callsign=" << (callsign.isEmpty() ? QStringLiteral("(none)") : callsign);
    }  // RADE threads: m_codecMutex released
    for (const QByteArray& chunk : modemOut) {
        emit txModemReady(chunk);
    }
    std::function<void()> sent = std::move(m_pendingEndOfOverSent);
    m_pendingEndOfOverSent = {};
    if (sent) {
        sent();
    }
    return EndOfOverRun::Sent;
}

}  // namespace NereusSDR
