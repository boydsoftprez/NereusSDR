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

}  // namespace

// From AetherSDR src/core/RADEEngine.cpp:18-25 [@0cd4559]
//   Trivial QObject ctor; dtor calls stop() so a destruction
//   without an explicit stop() still unwinds the RADE handles.
//   The dtor is defined out-of-line here so the forward-declared
//   std::unique_ptr<Resampler>/<RadeText> members can resolve
//   their destructors.
RadeChannel::RadeChannel(QObject* parent)
    : QObject(parent)
{
}

RadeChannel::~RadeChannel()
{
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
    if (m_active) {
        // Idempotent: a second start() with the same channel already
        // running is a no-op success (matches AetherSDR's `if (m_rade)
        // return true` guard at RADEEngine.cpp:30 [@0cd4559]).
        return true;
    }
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

    m_active = false;
    m_synced = false;
    m_farganWarmedUp = false;
    m_radeRxCallCount = 0;
    m_radeTxCallCount = 0;

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
    return static_cast<int>(m_txFeatAccum.size());
}

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
    // BENCH DEBUG: one-shot logs to confirm RX I/Q reaches the codec
    // through the queued-invocation path from RxDspWorker.
    static int s_rxProcessIqCount = 0;
    if (s_rxProcessIqCount < 3) {
        qCInfo(lcRade).noquote()
            << QString("RadeChannel::processIq #%1 bytes=%2 active=%3 "
                       "rade=%4 fargan=%5")
                .arg(s_rxProcessIqCount + 1)
                .arg(iqSamples.size())
                .arg(m_active)
                .arg(m_rade != nullptr)
                .arg(m_fargan != nullptr);
        ++s_rxProcessIqCount;
    }
    if (!m_active || !m_rade || !m_fargan) {
        return;
    }

    auto* fargan = static_cast<FARGANState*>(m_fargan.get());

    // Step 1: deinterleave 24 kHz interleaved I/Q float input.
    // The QByteArray holds an integer number of (I, Q) float pairs.
    const int kStereoFrameBytes = 2 * static_cast<int>(sizeof(float));
    const int nFrames = iqSamples.size() / kStereoFrameBytes;
    if (nFrames <= 0) {
        return;
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

        // EOO (end-of-over) handling lands at I4 with the embedded
        // text channel. For I2 we drop the EOO frame; AetherSDR
        // does likewise.
        if (has_eoo) {
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
        emit rxSpeechReady(m_rxOutAccum.left(outChunkBytes));
        m_rxOutAccum.remove(0, outChunkBytes);
    }
    // Note: AetherSDR emits a silence pad when the output accumulator
    // is short. We choose NOT to emit a silence pad on the no-sync
    // case because NereusSDR's audio engine is timer-driven and a
    // missing chunk does not stall the speaker bus; emitting silence
    // would just clobber whatever non-RADE audio is also feeding the
    // bus. If a future caller needs deterministic pacing, expose a
    // setting on the wrapper instead of forcing it here.

    // Step 8: sample sync / SNR / freq-offset.
    // From AetherSDR src/core/RADEEngine.cpp:290-298 [@0cd4559].
    const bool synced = rade_sync(m_rade) != 0;

    // BENCH DEBUG: log SNR + sync state every 100 rade_rx calls so we
    // can see whether the codec's internal detector is seeing anything
    // on the air even when sync isn't held. Helps distinguish:
    //   (a) signal present but at wrong sideband / freq (SNR fluctuates)
    //   (b) no signal at all (SNR stays at floor / NaN)
    static int s_radeRxTickCount = 0;
    if (++s_radeRxTickCount % 100 == 1) {
        const float snr = static_cast<float>(rade_snrdB_3k_est(m_rade));
        const float foff = static_cast<float>(rade_freq_offset(m_rade));
        qCInfo(lcRade).noquote()
            << QString("RadeChannel: tick=%1 synced=%2 SNR=%3 dB "
                       "freqOff=%4 Hz sideband=%5")
                .arg(s_radeRxTickCount)
                .arg(synced ? "YES" : "no")
                .arg(snr, 0, 'f', 1)
                .arg(foff, 0, 'f', 0)
                .arg(m_sidebandUpper ? "USB" : "LSB");
    }

    if (synced != m_synced) {
        m_synced = synced;
        qCInfo(lcRade) << "RadeChannel: rade_sync transition ->"
                       << (synced ? "SYNCED" : "UNSYNCED");
        emit syncChanged(synced);
    }
    if (synced) {
        const float snr = static_cast<float>(rade_snrdB_3k_est(m_rade));
        const float foff = static_cast<float>(rade_freq_offset(m_rade));
        emit snrChanged(snr);
        emit freqOffsetChanged(foff);
    }
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
    // BENCH DEBUG: one-shot first-receive log + gate-rejection trace so
    // we can see (a) whether the queued radeMicBlockReady is reaching
    // txEncode at all, and (b) why it might bail out (m_active /
    // m_rade / m_lpcnetEnc null after start()).
    static int s_txEncodeFirstLogged = 0;
    if (s_txEncodeFirstLogged < 3) {
        qCInfo(lcRade)
            << "txEncode call #" << (s_txEncodeFirstLogged + 1)
            << "bytes=" << speechSamples.size()
            << "active=" << m_active
            << "rade=" << (m_rade != nullptr)
            << "lpcnet=" << (m_lpcnetEnc != nullptr);
        ++s_txEncodeFirstLogged;
    }

    if (!m_active || !m_rade || !m_lpcnetEnc) {
        return;
    }
    if (speechSamples.isEmpty()) {
        return;
    }

    // Step 1: append the int16 mono 16 kHz input straight into the
    // TX accumulator. NereusSDR divergence vs AetherSDR (which
    // converts 24 kHz stereo float -> 16 kHz mono int16 at :139-152);
    // our input is already in the LPCNet-ready format.
    m_txAccum.append(speechSamples);

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
            emit txModemReady(stereo24k);
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
    m_txAccum.clear();
    m_txFeatAccum.clear();
    m_radeTxCallCount = 0;
}

}  // namespace NereusSDR
