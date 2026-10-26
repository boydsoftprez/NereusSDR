// no-port-check: NereusSDR-original. The CW keyer state both protocols put on
// the wire, and the helpers that derive it from the CW settings. Field
// layouts, limits and formulas are facts cited at each declaration; no
// upstream logic is ported here.
// =================================================================
// src/core/codec/CwWireState.h  (NereusSDR)
// =================================================================
//
// One protocol-neutral description of the radio keyer's settings
// (R-CW-11, R-CW-13, R-CW-16, R-CW-38). The Protocol 1 codecs read it from
// CodecContext::cw for banks 12, 13 and 14; the Protocol 2 connection fills
// its CmdTx CW fields from the same struct. The struct defaults are what a
// default CodecContext encodes: Thetis's keyer defaults with CW enable off.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW wire state and its helpers (R-CW-11,
//               R-CW-13, R-CW-16, R-CW-38). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#pragma once

#include <QtGlobal>

#include "core/CwTypes.h"
#include "core/NereusCoreExport.h"

namespace NereusSDR {

struct CwWireState {
    bool enable{false};          // radio keyer may run (R-CW-07); P1 bank 13 C1, P2 byte 5 bit 1
    bool breakIn{false};         // P2 byte 5 bit 7
    int  keyerMode{2};           // CwKeyerMode
    int  speedWpm{25};
    int  weight{50};
    bool strictSpacing{false};
    bool reversePaddles{false};
    int  sidetoneLevel{0};       // 0..127; P2 byte 5 bit 4 set when > 0
    int  sidetoneFreqHz{600};
    int  rfDelayMs{7};
    int  hangMs{310};
    int  edgeLengthMs{9};        // P2 byte 17; From Thetis setup.cs:29327 [v2.10.3.15]
    bool qskTxAttenuation{false};// P2 CmdTx bytes 57-59 = 31 (R-CW-16)
    bool operator==(const CwWireState&) const = default;
};

// Wire limits (R-CW-13). Speed: gateware iambic.v:150 (0 divides by zero,
// iambic.v:189) [@8e86a61]. Weight: iambic.v:153 [@8e86a61]. RF delay: one
// byte (Thetis networkproto1.c:637 [v2.10.3.15]). Hang: 10 bits (Thetis
// networkproto1.c:643-644 [v2.10.3.15]). Sidetone level: 0..127
// (Thetis console.cs:13046-13089 [v2.10.3.15], x1.27 of a 0..100 level).
inline constexpr int kCwWireSpeedMinWpm = 1;
inline constexpr int kCwWireSpeedMaxWpm = 60;
inline constexpr int kCwWireWeightMin = 33;
inline constexpr int kCwWireWeightMax = 66;
inline constexpr int kCwWireSidetoneLevelMax = 127;
inline constexpr int kCwWireRfDelayMaxMs = 255;
inline constexpr int kCwWireHangMaxMs = 1023;

// Speed 1..60, weight 33..66, level 0..127, rfDelay 0..255, hang 0..1023.
NEREUS_CORE_EXPORT void applyCwWireClamps(CwWireState& s);

// The radio's PTT hang (R-CW-13): Semi min(d + u, 1023); Qsk min(u, 1023)
// (the break-in delay is 0 in QSK, R-CW-21); Off 0.
// From Thetis console.cs:18523-18533 [v2.10.3.15] (break-in delay plus
// key-up delay while break-in is on, else 0).
NEREUS_CORE_EXPORT int cwHangMs(CwBreakInMode mode, int breakInDelayMs, int keyUpDelayMs);

// min(keyDownMs, 255, 900 / max(speedWpm, 1)), never below 0.
// From deskHPSDR old_protocol.c:2823-2831 [@f3d857c]
NEREUS_CORE_EXPORT int cwRfDelayMs(int keyDownMs, int speedWpm);

// 0 unless sidetone, radioSidetone and !radioMuted all allow;
// round(v*100*1.27) Semi/Qsk, round(v*100*0.73) Off, v clamped to 0..1.
// From Thetis console.cs:13046-13089 [v2.10.3.15]
// Upstream comment preserved verbatim (console.cs:13048):
//   //[2.10.3.6]MW0LGE changed to one location as code was everywhere previously
NEREUS_CORE_EXPORT int cwRadioSidetoneLevel(bool sidetone, bool radioSidetone, bool radioMuted,
                                            CwBreakInMode mode, float monitorVolume);

// P2 CmdTx byte 5: bit0 eer, 1 enable, 2 reverse, 3 iambic (mode != Straight),
// 4 level > 0, 5 mode B, 6 strict spacing, 7 breakIn.
// From Thetis network.h:151-166 [v2.10.3.15]
NEREUS_CORE_EXPORT quint8 composeP2CwModeControl(const CwWireState& s, bool eer);

} // namespace NereusSDR
