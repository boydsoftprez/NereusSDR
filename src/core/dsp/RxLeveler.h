// no-port-check: NereusSDR-original receive leveler; no upstream source.
#pragma once

// =================================================================
// src/core/dsp/RxLeveler.h  (NereusSDR)
// =================================================================
//
// Receive leveler. It evens out how loud stations sound on a voice net:
// the AGC already holds every station's peaks near one level, but a
// heavily processed voice still sounds louder than a natural one, and a
// station under the AGC threshold comes out quieter. The leveler rides
// each receiver's speech toward one listening level.
//
// Where it runs: AudioEngine::rxBlockReady, per receiver, after VAX and
// the receiver taps have taken their copy and before the speakers and
// remote-listening mix. So VAX and TCI always hear the plain audio, and
// the AF slider (applied in the mix, after this) stays the volume.
//
// What it is allowed to do:
//   * lift a quiet station only while RxStationGate (fed from the
//     S-meter, the receiver's quiet level, the panadapter noise floor
//     and the ADC overload readings) says a station is above the noise,
//     and never while the ADC is near overload;
//   * turn a loud station down, while RXL is on;
//   * never add delay: each block's loudest point is read before the
//     gain is applied, the gain moves in a ramp across the block, and a
//     limiter that looks ahead within the same block keeps the boost
//     under kCeiling.
// Off is a bypass. AudioEngine passes the block on untouched, so the
// audio is bit-identical to a build without the leveler.
//
// Modification history (NereusSDR):
//   2026-11-14 — RX Leveler feature (RXL control, setup section, per
//                receiver settings) by Brian Sutton (W4KZO), AI-assisted
//                via xAI Grok.
//   2026-11-14 — Leveling engine rewritten: moved from RxChannel into
//                AudioEngine, boost gated on a station being above the
//                noise, block look-ahead limiter, new defaults and
//                ranges chosen by offline measurement
//                (tst_rx_leveler, NEREUS_RXL_MATRIX=1). J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code.
// =================================================================

#include "core/WdspTypes.h"

#include <atomic>
#include <cstdint>

namespace NereusSDR {

// What the audio thread needs for one receiver's leveler, one block at a
// time. RadioModel resolves Auto to the preset below before it publishes,
// so the engine sees numbers only.
struct RxLevelerParams {
    // RXL is on, the receiver is in a voice mode, and this computer runs
    // its DSP. False is a bypass.
    bool on{false};
    // RxStationGate's verdict: a station shows above the noise, counting
    // the hold through the pauses in speech.
    bool stationPresent{false};
    // RxStationGate's verdict: the ADC is at or near overload.
    bool nearOverload{false};
    int  targetDb{-12};
    int  maxBoostDb{20};
    int  maxCutDb{6};
    int  attackMs{1000};
    int  releaseMs{600};

    bool operator==(const RxLevelerParams&) const = default;

    // One lock-free word, so the audio thread never reads half of a
    // change. Fields are clamped to the ranges below on the way in.
    std::uint64_t pack() const noexcept;
    static RxLevelerParams unpack(std::uint64_t word) noexcept;
};

class RxLeveler {
public:
    // ── Operator settings (SliceModel, the setup page, the RXL popup) ──
    //
    // Listening target: the speech level, in dBFS of the receiver's audio
    // before the AF slider, that every station is moved toward. The Auto
    // value is the level a natural (unprocessed) voice reaches after the
    // AGC, measured with the synthetic stations in tst_rx_leveler, so the
    // most common station is left as it is and the outliers move.
    static constexpr int kAutoTargetDb   = -12;
    static constexpr int kTargetDbMin    = -24;
    static constexpr int kTargetDbMax    =  -6;
    // Most the leveler may lift a quiet station.
    static constexpr int kAutoMaxBoostDb = 20;
    static constexpr int kMaxBoostDbMin  = 0;
    static constexpr int kMaxBoostDbMax  = 30;
    // Most the leveler may turn a loud station down.
    static constexpr int kAutoMaxCutDb   = 6;
    static constexpr int kMaxCutDbMin    = 0;
    static constexpr int kMaxCutDbMax    = 12;
    // How long the gain takes to rise toward a quieter station (one time
    // constant, ms).
    static constexpr int kAutoAttackMs     = 1000;
    static constexpr int kAttackMsMin      = 300;
    static constexpr int kAttackMsMax      = 5000;
    // How long the gain takes to come down for a louder station, and back
    // to unity when the station goes away (one time constant, ms).
    static constexpr int kAutoReleaseMs     = 600;
    static constexpr int kReleaseMsMin      = 200;
    static constexpr int kReleaseMsMax      = 3000;

    // ── Fixed engine values ──
    //
    // Peak ceiling for the leveler's own gain, linear full scale: -1 dBFS.
    // WDSP's AGC aims its peaks at out_target 1.0 (rxa.c create_wcpagc),
    // so this keeps a lifted station's peaks a little under the AGC's own.
    // A block that arrives above it keeps its own peak as the ceiling: the
    // limiter only holds back what the leveler adds.
    static constexpr double kCeiling = 0.8912509381337456;
    // A block counts as speech when it is within this much of the recent
    // speech peaks, and at least kOverHissDb over the hiss heard between
    // overs. Quieter blocks are the gaps between words: the gain holds
    // through them instead of chasing the hiss.
    static constexpr double kSpeechGateDb = 20.0;
    static constexpr double kOverHissDb = 3.0;
    // After this long with no speech block the gain goes back toward
    // unity at the release rate: the over has ended, or a long pause.
    // The hiss after an over is never held up for longer than this.
    static constexpr double kPauseReleaseSec = 1.0;
    // Up to this long after a speech block the gain keeps moving toward
    // what the station needs (the gaps between words); after it, until
    // kPauseReleaseSec, it may come down but not rise.
    static constexpr double kWordGapSec = 0.3;
    // How fast the recent-peaks reference falls (dB per second).
    static constexpr double kPeakFallDbPerSec = 4.0;
    // The hiss level is learned while no station is present and taken
    // out of the loudness reading, so a weak station is judged by its
    // voice, not its voice plus hiss. It falls with the first time
    // constant, rises with the second, and on a block more than
    // kHissSkipDb above it rises no faster than kHissCreepDbPerSec: the
    // first syllables of a weak station, a few dB over the hiss, arrive
    // before the station check has said a station is there, and must not
    // be learned as hiss. A band that really gets noisier is still
    // followed, slowly.
    static constexpr double kHissFallSec = 0.2;
    static constexpr double kHissRiseSec = 3.0;
    static constexpr double kHissSkipDb = 2.0;
    static constexpr double kHissCreepDbPerSec = 0.3;
    // Below this a block is silence whatever the reference says (dBFS).
    static constexpr double kSilenceDbfs = -70.0;
    // The loudness reading: an average of the speech blocks' power, hiss
    // taken out, over about this long.
    static constexpr double kLoudnessSec = 1.0;
    // A speech block may not be lifted past target + this. A loud station
    // that starts while the gain is still up for a quiet one loses that
    // boost at once instead of at the fall rate.
    static constexpr double kBoostHeadroomDb = 9.0;
    // Limiter: how fast it may pull the gain down ahead of a peak inside
    // the block, and how fast it lets go.
    static constexpr double kLimiterAttackMs  = 2.0;
    static constexpr double kLimiterReleaseMs = 150.0;

    // Voice modes only. CW, FM and the digital modes are left alone: they
    // feed decoders or carry no speech level to even out.
    static bool modeAllows(DSPMode mode);

    // Back to unity, nothing learned. The audio thread calls this when the
    // leveler is turned off, so turning it on again starts from scratch.
    void reset();

    // In place, interleaved stereo float32, `frames` frames. With
    // params.on false the block is left untouched and the state resets.
    void process(float* interleaved, int frames, double sampleRate,
                 const RxLevelerParams& params);

    // The leveling gain now (dB, + lift, - cut; the limiter not included).
    // Any thread.
    double gainDb() const;
    // The limiter's pull now (dB, 0 or less). Any thread.
    double limiterDb() const;
    // The station's speech level as the leveler reads it (dBFS), or
    // kSilenceDbfs before it has heard any. Audio thread (and tests).
    double loudnessDb() const;

private:
    double m_gainDb{0.0};
    double m_limiter{1.0};
    double m_loudPow{-1.0};     // < 0: no speech heard yet
    double m_loudSec{0.0};
    double m_quietSec{0.0};
    double m_needDb{0.0};
    double m_shortPow{0.0};
    double m_hissPow{-1.0};     // < 0: not learned yet
    double m_peakRefDb{-200.0};
    bool   m_active{false};
    std::atomic<float> m_publishedGain{0.0f};
    std::atomic<float> m_publishedLimiter{0.0f};
};

}  // namespace NereusSDR
