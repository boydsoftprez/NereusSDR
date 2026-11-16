// no-port-check: NereusSDR-original receive leveler gate; no upstream source.
#pragma once

// =================================================================
// src/core/dsp/RxStationGate.h  (NereusSDR)
// =================================================================
//
// Decides, for one receiver, whether a station shows above the noise
// and whether the ADC is near overload. The receive leveler (RxLeveler)
// lifts audio only while the first is true and the second is false, so
// the hiss between overs is never raised, and a weak station is lifted
// only when the radio shows it above the noise. The audio level is never
// the only evidence.
//
// Fed at the S-meter's rate (SliceMeterPump, about ten times a second)
// on the thread that owns the models, from readings NereusSDR already
// takes:
//   * the passband signal (WDSP's average S-meter reading, dBm);
//   * the panadapter noise floor for the receiver's stream, when the
//     panadapter has one (dBm per bin, the Auto AGC-T reading);
//   * the receive ADC peak (dBFS of the receiver's I/Q) and the radio's
//     own ADC overload flag;
//   * the share of the panadapter bins inside the receiver's filter that
//     stand out over the band's noise (RxPassbandVoice). A weak SSB voice
//     shows there well before it moves the passband's average reading.
//
// With the panadapter running, its bins decide: a static crash or band
// noise fading up lifts the whole band, the bins' noise reference with
// it, so it never counts there, while it moves the S-meter just as a
// voice does. With no panadapter reading the S-meter decides, and only
// once it has stayed above its bar for kMeterSustainSec, longer than a
// crash holds it up. Once a station is present, lower bars keep it
// (kPresentStayMarginDb, kVoiceStayShare), so a weak station is not
// dropped in the middle of a sentence; starting one takes the higher bars
// (the bins for kPresentEnterSec), so a single noise burst does not count.
//
// The quiet level is the S-meter reading with no station: it follows the
// reading down at once and creeps up slowly, so the pauses in speech keep
// it on the noise. While the panadapter floor is known, the gap between
// it and the quiet level is learned in quiet moments, and the quiet level
// is never allowed to creep more than kAnchorSlackDb above the band's own
// noise. A carrier that sits on frequency for a long over therefore stays
// a station, and a noise rise across the band is still followed.
//
// Modification history (NereusSDR):
//   2026-11-14 — New. J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-11-15 — Weak stations: the panadapter bins in the passband as a
//                second sign of a station, and lower bars to stay present.
//                J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-11-16 — Nobody talking, no lift: with the panadapter running only
//                its bins show a station; the S-meter alone must stay up
//                kMeterSustainSec. Static and fading noise had held a
//                station present and the leveler lifted the hiss. J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code.
// =================================================================

namespace NereusSDR {

struct RxStationReading {
    // Passband S-meter average, dBm. At or below kNoReadingDbm is no
    // reading (radio down, transmitting, channel not running).
    double signalDbm{-400.0};
    // Panadapter noise floor for this receiver, dBm per bin.
    double panFloorDbm{-400.0};
    bool   panFloorValid{false};
    // Peak of the receiver's I/Q, dBFS.
    double adcPeakDbfs{-400.0};
    // The radio reports an ADC overload on this receiver's ADC.
    bool   adcOverload{false};
    // Share (0 to 1) of the passband's panadapter bins standing out over
    // the band's noise (RxPassbandVoice::take). Below 0 is no reading.
    double passbandVoice{-1.0};
};

class RxStationGate {
public:
    // A station is present when the S-meter is this far above the quiet
    // level.
    static constexpr double kPresentMarginDb = 6.0;
    // Once present, the S-meter keeps it at this margin.
    static constexpr double kPresentStayMarginDb = 3.0;
    // A station is present when this share of the passband's panadapter
    // bins stands out over the band's noise ...
    static constexpr double kVoiceEnterShare = 0.08;
    // ... and, once present, stays at this share.
    static constexpr double kVoiceStayShare = 0.05;
    // ... for this long, so a single noise crash does not count.
    static constexpr double kPresentEnterSec = 0.15;
    // With no panadapter reading, the S-meter must stay above its bar this
    // long, to bring a station in and to keep one: longer than a static
    // crash holds the meter up.
    static constexpr double kMeterSustainSec = 0.5;
    // Once present, it stays present this long after the last reading
    // above the margin: the gaps between words and breaths.
    static constexpr double kPresentHoldSec = 2.5;
    // How fast the quiet level creeps up when the reading stays above it.
    static constexpr double kQuietRiseDbPerSec = 0.5;
    // How far the quiet level may sit above the panadapter's noise, once
    // the gap between the two is learned.
    static constexpr double kAnchorSlackDb = 3.0;
    // Time constant for learning that gap, in quiet moments.
    static constexpr double kAnchorLearnSec = 4.0;
    // Seconds of quiet readings before the gap counts as learned.
    static constexpr double kAnchorReadySec = 2.0;
    // ADC peak at or above this is near overload (dBFS).
    static constexpr double kNearOverloadDbfs = -3.0;
    // Near overload lasts this long after the last sign of it.
    static constexpr double kOverloadHoldSec = 2.0;
    // Readings at or below this are no reading.
    static constexpr double kNoReadingDbm = -300.0;

    // Forget the quiet level and every hold: a retune, a mode or filter
    // change, a new band. The next readings start the learning again,
    // with no station present until one shows above them.
    void reset();

    // One reading, `dtSec` after the previous one.
    void update(const RxStationReading& reading, double dtSec);

    bool stationPresent() const noexcept { return m_present; }
    bool nearOverload() const noexcept { return m_overloadLeft > 0.0; }
    // The quiet level (dBm), or kNoReadingDbm before the first reading.
    double quietLevelDbm() const noexcept { return m_quietDbm; }
    bool anchorLearned() const noexcept { return m_anchorQuietSec >= kAnchorReadySec; }

private:
    double m_quietDbm{kNoReadingDbm};
    bool   m_present{false};
    double m_aboveSec{0.0};       // bins above their bar, unbroken
    double m_meterSec{0.0};       // S-meter above its bar, unbroken
    double m_holdLeft{0.0};
    double m_overloadLeft{0.0};
    double m_anchorDb{0.0};       // quiet level minus panadapter floor
    double m_anchorQuietSec{0.0};
};

}  // namespace NereusSDR
