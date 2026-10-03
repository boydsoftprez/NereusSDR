// no-port-check: NereusSDR-original struct; the doc comment below names Thetis
// source files only to explain the default-value provenance, not to port code.
#pragma once

#include "models/SliceModel.h"   // for Mode enum (alias of DSPMode)
#include "NnrSettings.h"

namespace NereusSDR {

/// Snapshot of all per-channel DSP state that survives a rebuild.
/// Captured before WDSP channel destroy; reapplied after recreate.
///
/// **Default values:** These are the Thetis-visible "first-launch UI defaults"
/// from `console.cs` and `setup.cs` (e.g. `mode = USB`, `agcMode = 0` for the
/// initial UI state Thetis shows on fresh installation). They do NOT match
/// `RxChannel`'s constructor defaults (`mode = LSB`, `agcMode = Med`), which
/// reflect the WDSP wire-level defaults from `cmaster.c`. Always populate this
/// struct via `RxChannel::captureState()` before calling `applyState()` —
/// default-constructing an `RxChannelState` and applying it will push the UI
/// defaults to WDSP, which is rarely what you want.
struct RxChannelState {
    SliceModel::Mode mode               = SliceModel::Mode::USB;
    int    filterLowHz              = 200;
    int    filterHighHz             = 2700;

    // AGC
    int    agcMode                  = 0;
    int    agcAttackMs              = 5;
    int    agcDecayMs               = 500;
    int    agcHangMs                = 100;
    int    agcSlope                 = 5;
    int    agcMaxGainDb             = 90;
    int    agcFixedGainDb           = 60;
    int    agcHangThresholdPct      = 0;

    // Noise blanker / noise reduction / ANF
    bool   nbEnabled                = false;
    int    nbMode                   = 0;
    bool   nrEnabled                = false;
    int    nrMode                   = 0;
    NrSlot activeNr                 = NrSlot::Off;
    NnrSettings nnrTuning;
    // R-R3-40 (2026-09-23): the runtime NNR limit (NnrLimit), so a rebuilt
    // channel keeps the step the Core took. Never persisted.
    int    nnrLimit                 = 0;
    bool   anfEnabled               = false;

    // EQ
    bool   eqEnabled                = false;
    int    eqPreampDb               = 0;
    int    eqBandsDb[10]            = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Squelch
    bool   squelchEnabled           = false;
    int    squelchThresholdDb       = -150;

    // RIT, antenna, shift offset
    int    ritOffsetHz              = 0;
    int    antennaIndex             = 0;
    double shiftOffsetHz            = 0.0;
};

}  // namespace NereusSDR
