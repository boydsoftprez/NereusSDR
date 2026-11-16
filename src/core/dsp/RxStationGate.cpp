// no-port-check: NereusSDR-original receive leveler gate; no upstream source.
// =================================================================
// src/core/dsp/RxStationGate.cpp  (NereusSDR)
// =================================================================
//
// See RxStationGate.h.
// =================================================================

#include "RxStationGate.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

void RxStationGate::reset()
{
    m_quietDbm = kNoReadingDbm;
    m_present = false;
    m_aboveSec = 0.0;
    m_meterSec = 0.0;
    m_holdLeft = 0.0;
    m_anchorDb = 0.0;
    m_anchorQuietSec = 0.0;
    // Near overload is about the radio, not the frequency: a retune keeps it.
}

void RxStationGate::update(const RxStationReading& reading, double dtSec)
{
    const double dt = std::isfinite(dtSec) ? std::max(dtSec, 0.0) : 0.0;

    const bool adcNear = reading.adcOverload
        || (std::isfinite(reading.adcPeakDbfs) && reading.adcPeakDbfs > kNoReadingDbm
            && reading.adcPeakDbfs >= kNearOverloadDbfs);
    if (adcNear) {
        m_overloadLeft = kOverloadHoldSec;
    } else {
        m_overloadLeft = std::max(0.0, m_overloadLeft - dt);
    }

    const double s = reading.signalDbm;
    if (!std::isfinite(s) || s <= kNoReadingDbm) {
        // No reading: nothing to judge by, so no station.
        m_present = false;
        m_aboveSec = 0.0;
        m_meterSec = 0.0;
        m_holdLeft = 0.0;
        return;
    }
    if (m_quietDbm <= kNoReadingDbm) {
        m_quietDbm = s;
        return;
    }

    const bool panValid = reading.panFloorValid && std::isfinite(reading.panFloorDbm)
        && reading.panFloorDbm > kNoReadingDbm;

    // The quiet level follows the reading down at once and up slowly.
    if (s < m_quietDbm) {
        m_quietDbm = s;
    } else {
        m_quietDbm = std::min(s, m_quietDbm + kQuietRiseDbPerSec * dt);
    }
    // ... and, once the gap to the band's noise is known, no higher than
    // that noise allows.
    if (panValid && anchorLearned()) {
        m_quietDbm = std::min(m_quietDbm,
                              reading.panFloorDbm + m_anchorDb + kAnchorSlackDb);
    }

    // Above the noise by the panadapter bins in the passband, or, with no
    // panadapter reading, by the S-meter; a station already present is
    // kept at the lower bars.
    //
    // With the panadapter running, its bins decide alone. A static crash,
    // or band noise fading up a few dB, lifts the whole band and the S-meter
    // with it: the S-meter took that for a station, and the leveler then
    // lifted the hiss. In the bins it lifts the band's noise reference too
    // and does not count, while any station strong enough to move the
    // S-meter stands out there.
    //
    // With no panadapter, the S-meter counts once it has stayed above its
    // bar for kMeterSustainSec, both to bring a station in and to keep one:
    // a crash holds the meter up for less, so crash after crash cannot hold
    // a station present on an empty frequency.
    const double voice = std::isfinite(reading.passbandVoice) ? reading.passbandVoice : -1.0;
    const bool binsKnown = voice >= 0.0;
    const bool meterAbove = s >= m_quietDbm + (m_present ? kPresentStayMarginDb : kPresentMarginDb);
    m_meterSec = (meterAbove && !binsKnown) ? m_meterSec + dt : 0.0;
    const bool meterHeld = m_meterSec >= kMeterSustainSec - 1e-9;
    const bool binsAbove = voice >= (m_present ? kVoiceStayShare : kVoiceEnterShare);
    m_aboveSec = binsAbove ? m_aboveSec + dt : 0.0;
    const bool binsHeld = binsAbove && (m_present || m_aboveSec >= kPresentEnterSec - 1e-9);
    const bool above = meterAbove || binsAbove;
    if (meterHeld || binsHeld) {
        m_present = true;
        m_holdLeft = kPresentHoldSec;
    } else if (m_present) {
        m_holdLeft -= dt;
        if (m_holdLeft <= 0.0) {
            m_present = false;
            m_holdLeft = 0.0;
        }
    }

    // Learn the gap between the quiet level and the panadapter's noise in
    // quiet moments only.
    if (panValid && !m_present && !above) {
        const double gap = s - reading.panFloorDbm;
        if (m_anchorQuietSec <= 0.0) {
            m_anchorDb = gap;
        } else {
            m_anchorDb += (1.0 - std::exp(-dt / kAnchorLearnSec)) * (gap - m_anchorDb);
        }
        m_anchorQuietSec += dt;
    }
}

}  // namespace NereusSDR
