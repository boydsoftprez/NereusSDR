// no-port-check: NereusSDR-original. The CW keyer gate: when the radio's
// keyer may run. The band plan it consults is BandPlanGuard's (ported there,
// with its cites); this file ports no upstream logic.
// =================================================================
// src/core/safety/CwKeyerGate.h  (NereusSDR)
// =================================================================
//
// R-CW-05: "may the radio's keyer run", as a bool and the refusal words.
// The inputs are plain values the caller (CwKeyerController) reads from the
// models; the gate is a pure function so its truth table is tested alone.
// The PC microphone check is not an input (no microphone in CW), and neither
// is the transmit holder (R-CW-33 applies the holder to computer keying).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the keyer gate (R-CW-05, R-CW-06, D16).
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#pragma once

#include <QString>

#include "core/NereusCoreExport.h"
#include "core/safety/BandPlanGuard.h"
#include "core/safety/TxRefusal.h"

namespace NereusSDR::safety {

using MoxCheckResult = BandPlanGuard::MoxCheckResult;

struct CwKeyerGateInputs {
    bool hasTxSlice{false};
    bool txSliceInCw{false};          // CWL or CWU
    MoxCheckResult bandPlan{true, QString()}; // checkMoxAllowed for the carrier in CW
    bool receiveOnly{false};          // setting, board, Core policy or role
    bool txInhibited{false};
    bool paTripped{false};
    bool radioLinkDown{false};
    bool radioChangeUnderway{false};
    bool stopLatched{false};          // emergency stop or TX time-out latch
    QString interlockRefusal;         // empty when TxInterlockPolicy passes
    bool interlockArmed{false};
    bool interlockGranted{false};
};

struct CwKeyerGateResult {
    bool allowed{false};
    TxRefusal refusal;
};

// The words for the refusals that have no TxRefusals entry of their own.
// Each passes OperatorWording::isPlain (tst_cw_keyer_gate).
inline constexpr char kCwGateNotCwText[] =
    "Put the transmit slice in CW to use the radio's keyer.";
inline constexpr char kCwGateStopLatchedText[] =
    "Transmit was stopped. Release the key before sending again.";
inline constexpr char kCwGateInterlockNotGrantedText[] =
    "The keyer waits until the amplifier is ready to transmit.";

// True only when every input passes. Order of refusals: no TX slice, not CW,
// receive only, inhibit, PA trip, link down, radio change, stop latch, band
// plan, interlock, interlock not granted (D16: an armed amplifier interlock
// holds the keyer until it has granted).
NEREUS_CORE_EXPORT CwKeyerGateResult evaluateCwKeyerGate(const CwKeyerGateInputs& in);

} // namespace NereusSDR::safety
