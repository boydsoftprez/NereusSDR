// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/RadeChannel.h  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeChannel: host-side wrapper around the RADE (Radio
// Autoencoder) v1 codec for FreeDV digital voice on OpenHPSDR radios.
//
// Ported from two upstreams (hybrid port):
//
//   Structure: AetherSDR src/core/RADEEngine.{h,cpp} [@0cd4559]
//     Class layout (Q_OBJECT subclass, public start/stop/isActive/
//     isSynced lifecycle, signals/slots, std::unique_ptr-owned
//     resampler members, paired TX/RX accumulator QByteArrays,
//     opaque-pointer ownership of the rade/LPCNet/FARGAN handles),
//     dtor placement (out-of-line in the .cpp so forward-declared
//     unique_ptr types resolve), and the Qt6 conventions throughout
//     all follow the AetherSDR client.
//
//   DSP API:   freedv-gui src/pipeline/RADEReceiveStep.{h,cpp},
//              RADETransmitStep.{h,cpp}                 [@77e793a]
//     The call patterns I2/I3 will fill in (rade_open with a model
//     filename, rade_n_features_in_out, rade_nin, rade_rx, rade_tx,
//     LPCNet feature extractor lifecycle, FARGAN vocoder warm-up,
//     embedded rade_text aux channel) all come from the freedv-gui
//     pipeline steps. I1 stubs the slot bodies; I2 plugs in the RX
//     path; I3 plugs in the TX path; I4 plugs in the embedded text
//     channel.
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
//     The specific RADEReceiveStep.{h,cpp} and RADETransmitStep.{h,cpp}
//     files each carry a permissive BSD-2-Clause-style file header
//     (Copyright Mooneer Salem, no per-file project copyright line);
//     the BSD permission block is reproduced verbatim below per the
//     upstream redistribution clause. The four upstream files all
//     carry the same header text; we reproduce it once and stack
//     `--- From <path> ---` markers above so the multi-file scope is
//     unambiguous.
//
// LGPL is upgrade-compatible to GPL-3 (LGPL section 3 conversion
// clause); the BSD-2-Clause file-header carve-out is GPL-compatible
// by its own terms. NereusSDR ships under GPLv3.
//
// --- From freedv-gui/src/pipeline/RADEReceiveStep.h ---
// --- From freedv-gui/src/pipeline/RADEReceiveStep.cpp ---
// --- From freedv-gui/src/pipeline/RADETransmitStep.h ---
// --- From freedv-gui/src/pipeline/RADETransmitStep.cpp ---
//   (all four files carry the identical BSD-2-Clause-style header
//    reproduced verbatim below)
//
//=========================================================================
// Name:            RADEReceiveStep.h
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
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I1. Initial skeleton
//                 port. Hybrid sourcing: class layout / Q_OBJECT shape
//                 / signal-slot surface / member ownership / dtor
//                 placement from AetherSDR src/core/RADEEngine.{h,cpp}
//                 [@0cd4559]; DSP API surface that I2/I3/I4 will plug
//                 into (rade_open call shape, LPCNet feature extractor,
//                 FARGAN vocoder warm-up, embedded rade_text channel)
//                 from freedv-gui src/pipeline/RADE{Receive,Transmit}
//                 Step.{h,cpp} [@77e793a]. NereusSDR divergences vs
//                 AetherSDR: start() takes a model-path argument (the
//                 OpenHPSDR client must load the user-selected .f32
//                 model file rather than hard-coding "dummy" the way
//                 AetherSDR does); the processIq slot accepts I/Q
//                 samples from the receiver (RADE expects RADE_COMP
//                 baseband, not DAX audio); txEncode accepts mic
//                 samples (16 kHz mono int16) so the WdspEngine TX
//                 pump path can feed directly without a 24kHz->16kHz
//                 step the way AetherSDR's feedTxAudio does;
//                 rxTextDecoded signal carries the embedded text-channel
//                 callsign + grid (I4 wires this up). Skeleton bodies
//                 only at I1; isActive()/isSynced() pin the lifecycle
//                 contract the I2/I3/I4 implementations layer onto.
//                 AI tooling: Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I2. RX path port
//                 from AetherSDR src/core/RADEEngine.cpp:27-78 (start),
//                 :80-106 (stop), :200-303 (feedRxAudio body) [@0cd4559]
//                 cross-checked against freedv-gui
//                 src/pipeline/RADEReceiveStep.cpp:175-310 [@77e793a].
//                 Added second downsampler m_down24to8Q so I/Q input
//                 from the OpenHPSDR DDC stays complex through the
//                 RADE_COMP assembly (vs AetherSDR's stereo-PCM
//                 downmix to mono + imag=0). Added test seam
//                 radeRxCallCountForTest() + m_radeRxCallCount so
//                 the new test suite can pin when rade_rx() is
//                 invoked relative to the rade_nin() accumulator
//                 threshold. AI tooling: Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I3. TX path port
//                 from AetherSDR src/core/RADEEngine.cpp:134-198
//                 (feedTxAudio body) [@0cd4559] cross-checked against
//                 freedv-gui src/pipeline/RADETransmitStep.cpp
//                 :216-247 (restartVocoder / reset) [@77e793a].
//                 NereusSDR divergence vs AetherSDR: txEncode accepts
//                 16 kHz mono int16 speech samples directly (the
//                 WdspEngine TX pump already feeds 16 kHz mono per the
//                 plan), so AetherSDR's 24 kHz stereo float -> 16 kHz
//                 mono int16 down-conversion at feedTxAudio:139-152 is
//                 dropped; the input bytes go straight into m_txAccum
//                 as int16. The LPCNet feature extraction, the
//                 12-frame feature accumulation, the rade_tx call,
//                 and the 8 kHz RADE_COMP real-leg -> 24 kHz stereo
//                 float32 upsample all follow AetherSDR line-for-line.
//                 Added test seams radeTxCallCountForTest() and
//                 txFeatureAccumSizeForTest() + m_radeTxCallCount so
//                 the test suite can pin when rade_tx() is invoked
//                 relative to the rade_n_features_in_out() threshold
//                 and verify resetTx() actually flushes feature state.
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QByteArray>
#include <QString>
#include <memory>

// Forward-declare the lcRade logging category so other translation
// units (notably RadioModel.cpp's wireRadeChannel extension at K4)
// can qCInfo/qCWarning into "nereus.rade" without duplicating the
// Q_LOGGING_CATEGORY definition.  The definition lives in
// RadeChannel.cpp; this declaration just publishes the symbol.
Q_DECLARE_LOGGING_CATEGORY(lcRade)

// Forward declarations for the upstream RADE / LPCNet / FARGAN handles.
// We keep them as `struct ...*` and `void*` here per the AetherSDR
// pattern (see RADEEngine.h:8-12 [@0cd4559]) so the freedv-gui /
// opus headers do not bleed into every translation unit that
// includes RadeChannel.h. The .cpp resolves them via the
// third_party/rade headers at instantiation time.
struct rade;
struct LPCNetEncState;

namespace NereusSDR {

// Forward declarations for the NereusSDR-native helpers RadeChannel
// owns by unique_ptr. The full type definitions live in Resampler.h
// and RadeText.h; RadeChannel.cpp includes them so the unique_ptr
// destructors resolve.
class Resampler;
class RadeText;

// Host-side wrapper for the RADE v1 codec. Lifecycle is:
//
//   1. Construct                            !isActive(), !isSynced()
//   2. start(modelPath)                     loads the .f32 model file,
//                                           opens rade/LPCNet/FARGAN,
//                                           builds the resamplers;
//                                           flips isActive() to true.
//                                           I1 ships path-exists only;
//                                           I2 wires up the real load.
//   3. processIq(iq)        (RX path)       feeds I/Q from the
//                                           receiver. Body lands at I2.
//   4. txEncode(speech)     (TX path)       feeds mic samples to the
//                                           LPCNet encoder + rade_tx.
//                                           Body lands at I3.
//   5. resetTx()                            flushes TX state on MOX
//                                           release. Body lands at I3.
//   6. stop()                               unwinds in reverse; flips
//                                           isActive() back to false.
//
// Signals follow the AetherSDR surface (syncChanged / snrChanged /
// freqOffsetChanged) plus the NereusSDR-specific rxTextDecoded
// signal that I4 will hook up to the embedded rade_text channel.
class RadeChannel : public QObject {
    Q_OBJECT

public:
    explicit RadeChannel(QObject* parent = nullptr);
    ~RadeChannel() override;

    bool start(const QString& modelPath);
    void stop();
    bool isActive() const;
    bool isSynced() const;

    // Hook for sideband selection (RADE_U vs RADE_L).  Stored only at
    // the v0.5.0 fix-up that split the original single RADE entry into
    // upper/lower variants; not yet consumed by the I/Q routing layer.
    // K-bench
    // follow-up will wire the stored value into any future spectral
    // mirroring at the TX modulator stage.  Default is upper.
    bool sidebandUpper() const;

    // Test seam. Returns the number of times rade_rx() has been invoked
    // since start(). Used by tst_rade_channel to verify the RX
    // accumulator pumps the codec only when a full rade_nin()-sized
    // chunk is buffered.
    int radeRxCallCountForTest() const;

    // Test seam. Returns the number of times rade_tx() has been invoked
    // since start(). Used by tst_rade_channel to verify the TX
    // accumulator pumps the codec only once rade_n_features_in_out()
    // features are buffered.
    int radeTxCallCountForTest() const;

    // Test seam. Returns the byte size of the TX feature accumulator.
    // Used by tst_rade_channel to verify resetTx() actually flushes
    // state on MOX release.
    int txFeatureAccumSizeForTest() const;

public slots:
    // Sideband selection hook.  Set true for RADE_U (upper) and false
    // for RADE_L (lower).  Stored on the channel; not yet consumed by
    // the I/Q routing layer.  Default state (no caller) is upper.
    void setSideband(bool upper);

    // RX path: feed I/Q from the receiver. RADE expects baseband
    // RADE_COMP at the codec's sample rate; the conversion path
    // lands at I2.
    void processIq(const QByteArray& iqSamples);

    // TX path: feed mic samples (16 kHz mono int16) for LPCNet
    // feature extraction and rade_tx encoding. Conversion + encoder
    // path lands at I3.
    void txEncode(const QByteArray& speechSamples);

    // Flush TX encoder state on MOX release to prevent stale audio
    // from carrying across PTT transitions. Body lands at I3.
    void resetTx();

signals:
    // Decoded speech, 24 kHz stereo int16, ready for the speaker bus.
    void rxSpeechReady(const QByteArray& pcm);

    // Encoded modem signal, 24 kHz stereo int16, ready for the TX
    // I/Q output stage.
    void txModemReady(const QByteArray& iq);

    // RADE decoder sync indication. Emitted on every state transition;
    // I2's syncFn will drive this.
    void syncChanged(bool synced);

    // Signal-to-noise ratio in dB as reported by the RADE decoder.
    void snrChanged(float snrDb);

    // Carrier-frequency offset in Hz as reported by the RADE decoder
    // (used by the panadapter overlay to draw a sync mark).
    void freqOffsetChanged(float hz);

    // Embedded text-channel decode (callsign + Maidenhead grid). I4
    // wires this up to the rade_text aux channel.
    void rxTextDecoded(const QString& callsign, const QString& grid);

private:
    // Custom deleter for the opaque FARGANState handle so we can hold it
    // in unique_ptr<void, FarganDeleter> without dragging the opus
    // FARGAN header into the include surface. The operator() resolves
    // to `delete static_cast<FARGANState*>(p)` in RadeChannel.cpp where
    // fargan.h is included. NereusSDR-only refactor of AetherSDR's
    // raw-void* pattern at RADEEngine.h:8-12 [@0cd4559] to comply with
    // the project's "no raw new/delete" rule (CLAUDE.md).
    struct FarganDeleter {
        void operator()(void* p) const noexcept;
    };

    // Upstream RADE / LPCNet / FARGAN handles. Opaque here; the .cpp
    // resolves struct rade / LPCNetEncState / FARGANState* (held as
    // void* per AetherSDR's pattern so the opus FARGAN header does
    // not need to be visible at the include site).
    struct rade*         m_rade{nullptr};
    LPCNetEncState*      m_lpcnetEnc{nullptr};
    std::unique_ptr<void, FarganDeleter> m_fargan;

    bool                 m_active{false};
    bool                 m_synced{false};
    bool                 m_farganWarmedUp{false};

    // RADE-U / RADE-L sideband flag.  True for upper (RADE_U, default),
    // false for lower (RADE_L).  Set via setSideband() in SliceModel's
    // mode-swap path; not yet consumed by the I/Q routing layer at
    // v0.5.0.  K-bench follow-up will wire this into any future
    // spectral mirroring at the TX modulator stage.
    bool                 m_sidebandUpper{true};

    // Test seam counter: incremented every time rade_rx() runs in
    // processIq(). Cleared on start().
    int                  m_radeRxCallCount{0};

    // Test seam counter: incremented every time rade_tx() runs in
    // txEncode(). Cleared on start() and on resetTx().
    int                  m_radeTxCallCount{0};

    // Resampler chain. The AetherSDR client owns four resamplers
    // (24kHz<->8kHz for the modem leg and 24kHz<->16kHz for the
    // LPCNet/FARGAN leg). NereusSDR's TX-side input is 16 kHz mono
    // (per txEncode's contract), but the RX-side output is still
    // 24 kHz stereo for the speaker bus, so we keep the full set.
    //
    // NereusSDR divergence: AetherSDR takes stereo DAX audio and
    // averages L+R into a single mono leg via processStereoToMono
    // before feeding RADE_COMP with imag=0. Our processIq receives
    // I/Q from the OpenHPSDR DDC, which is already complex baseband,
    // so we run two parallel downsamplers (m_down24to8 for the I leg,
    // m_down24to8Q for the Q leg) and interleave the outputs back
    // into RADE_COMP pairs. The TX-side modem-up leg and the LPCNet
    // leg remain single-resampler because LPCNet operates on real
    // mono speech and the modem TX output is also real mono.
    std::unique_ptr<Resampler> m_down24to8;
    std::unique_ptr<Resampler> m_down24to8Q;
    std::unique_ptr<Resampler> m_up8to24;
    std::unique_ptr<Resampler> m_down24to16;
    std::unique_ptr<Resampler> m_up16to24;

    // TX accumulation buffers: speech samples queue up until LPCNet
    // has a frame worth, then feature vectors queue up until RADE
    // has 12 frames (NB_TOTAL_FEATURES = 432 floats) for rade_tx.
    // RX accumulation buffers: RADE_COMP samples queue up until
    // rade_nin() worth are available for rade_rx, then decoded
    // features queue up for FARGAN.
    QByteArray m_txAccum;
    QByteArray m_txFeatAccum;
    QByteArray m_rxAccum;
    QByteArray m_rxFeatAccum;
    QByteArray m_rxOutAccum;

    // Embedded aux text channel. I4 wires this up to rade_text_set_tx /
    // rade_text_get_data and emits rxTextDecoded on a complete payload.
    std::unique_ptr<RadeText> m_textChannel;
};

}  // namespace NereusSDR
