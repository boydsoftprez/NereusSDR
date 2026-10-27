// no-port-check: NereusSDR-original. Helpers for the CW wire state; the
// limits and formulas are facts cited in CwWireState.h and below.
// =================================================================
// src/core/codec/CwWireState.cpp  (NereusSDR)
// =================================================================
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW wire state and its helpers (R-CW-11,
//               R-CW-13, R-CW-16, R-CW-38). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include "core/codec/CwWireState.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// From Thetis console.cs:13065-13069 [v2.10.3.15]
//   vol = (int)(txaf * 0.73);   // BreakIn.Manual
//   vol = (int)(txaf * 1.27);   // otherwise
constexpr double kSidetoneScaleBreakIn = 1.27;
constexpr double kSidetoneScaleManual = 0.73;

// From deskHPSDR old_protocol.c:2828 [@f3d857c]
//   uint8_t rfmax = 900 / cw_keyer_speed;
constexpr int kRfDelayDotBudget = 900;

} // namespace

void applyCwWireClamps(CwWireState& s)
{
    s.speedWpm = std::clamp(s.speedWpm, kCwWireSpeedMinWpm, kCwWireSpeedMaxWpm);
    s.weight = std::clamp(s.weight, kCwWireWeightMin, kCwWireWeightMax);
    s.sidetoneLevel = std::clamp(s.sidetoneLevel, 0, kCwWireSidetoneLevelMax);
    s.rfDelayMs = std::clamp(s.rfDelayMs, 0, kCwWireRfDelayMaxMs);
    s.hangMs = std::clamp(s.hangMs, 0, kCwWireHangMaxMs);
}

int cwHangMs(CwBreakInMode mode, int breakInDelayMs, int keyUpDelayMs)
{
    const int delay = std::max(breakInDelayMs, 0);
    const int keyUp = std::max(keyUpDelayMs, 0);
    switch (mode) {
        case CwBreakInMode::Semi:
            return std::min(delay + keyUp, kCwWireHangMaxMs);
        case CwBreakInMode::Qsk:
            return std::min(keyUp, kCwWireHangMaxMs);
        case CwBreakInMode::Off:
            break;
    }
    return 0;
}

int cwRfDelayMs(int keyDownMs, int speedWpm)
{
    const int dotBudget = kRfDelayDotBudget / std::max(speedWpm, 1);
    const int delay = std::min({keyDownMs, kCwWireRfDelayMaxMs, dotBudget});
    return std::max(delay, 0);
}

int cwRadioSidetoneLevel(bool sidetone, bool radioSidetone, bool radioMuted,
                         CwBreakInMode mode, float monitorVolume)
{
    if (!sidetone || !radioSidetone || radioMuted) {
        return 0;
    }
    // Thetis's txaf is the integer 0..100 TX AF level; NereusSDR's monitor
    // volume is 0..1, so it is rounded to that integer first. The product is
    // then truncated, as Thetis's (int) cast does (R-CW-38).
    const int txaf = static_cast<int>(std::lround(std::clamp(monitorVolume, 0.0f, 1.0f) * 100.0f));
    const double scale = (mode == CwBreakInMode::Off) ? kSidetoneScaleManual
                                                      : kSidetoneScaleBreakIn;
    const int level = static_cast<int>(txaf * scale);
    return std::clamp(level, 0, kCwWireSidetoneLevelMax);
}

quint8 composeP2CwModeControl(const CwWireState& s, bool eer)
{
    // From Thetis network.h:156-163 [v2.10.3.15]
    //   eer : 1, // bit 00   cw_enable : 1, // bit 01   rev_paddle : 1, // bit 02
    //   iambic : 1, // bit 03   sidetone : 1, // bit 04   mode_b : 1, // bit 05
    //   strict_spacing : 1, // bit 06   break_in : 1; // bit 07
    const auto mode = static_cast<CwKeyerMode>(s.keyerMode);
    quint8 b = 0;
    if (eer) {
        b |= 0x01;
    }
    if (s.enable) {
        b |= 0x02;
    }
    if (s.reversePaddles) {
        b |= 0x04;
    }
    if (mode != CwKeyerMode::Straight) {
        b |= 0x08;
    }
    if (s.sidetoneLevel > 0) {
        b |= 0x10;
    }
    if (mode == CwKeyerMode::IambicB) {
        b |= 0x20;
    }
    if (s.strictSpacing) {
        b |= 0x40;
    }
    if (s.breakIn) {
        b |= 0x80;
    }
    return b;
}

} // namespace NereusSDR
