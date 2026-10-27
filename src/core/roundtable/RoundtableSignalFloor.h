#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/roundtable/RoundtableSignalFloor.h  (NereusSDR)
// =================================================================
//
// One slice's signal-present decision (Roundtable wire spec 6.5, design
// C3): a reading is present when it is at least 10 dB above the slice's
// own floor. The floor is the 10th percentile of the last 60 s of readings
// taken while present was false; while present is true the floor holds
// (no reading joins the window and none ages out of it). After a retune
// present stays false for the first 10 s while a floor forms.
//
// The percentile is the nearest-rank one: the smallest reading with at
// least 10% of the window at or below it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10, R-RT-6).
// =================================================================

#include "core/NereusCoreExport.h"

#include <QtGlobal>

#include <deque>
#include <utility>

namespace NereusSDR {

class NEREUS_CORE_EXPORT RoundtableSignalFloor {
public:
    static constexpr int kWindowMs = 60000;
    static constexpr int kWarmupMs = 10000;
    static constexpr double kPresentAboveFloorDb = 10.0;
    static constexpr double kFloorPercentile = 0.10;

    /// Starts the warm-up and clears the window (a new frequency or mode).
    void retune(qint64 nowMs);
    /// Takes one reading and returns whether a signal is present. The
    /// first reading after construction starts the warm-up as retune does.
    bool update(double dbm, qint64 nowMs);

    bool present() const { return m_present; }
    /// The floor in force, or false when the window is empty.
    bool floor(double* dbm) const;

private:
    void dropOlderThan(qint64 nowMs);

    std::deque<std::pair<qint64, double>> m_window;
    qint64 m_warmupEndsMs = 0;
    bool m_started = false;
    bool m_present = false;
};

} // namespace NereusSDR
