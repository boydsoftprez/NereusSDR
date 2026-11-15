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

    // Above the noise by the S-meter, or by the panadapter bins in the
    // passband; a station already present is kept at the lower bars.
    const bool meterAbove = s >= m_quietDbm + (m_present ? kPresentStayMarginDb : kPresentMarginDb);
    const double voice = std::isfinite(reading.passbandVoice) ? reading.passbandVoice : -1.0;
    const bool binsAbove = voice >= (m_present ? kVoiceStayShare : kVoiceEnterShare);
    const bool above = meterAbove || binsAbove;
    m_aboveSec = above ? m_aboveSec + dt : 0.0;
    if (above && (m_present || m_aboveSec >= kPresentEnterSec)) {
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
