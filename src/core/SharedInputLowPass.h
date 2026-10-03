#pragma once

// no-port-check: NereusSDR-original selection helper. It generalises the
// two-receiver rules ported in P1RadioConnection.cpp and
// P2RadioConnection.cpp (which carry the Thetis console.cs and mi0bot
// Penny.cs headers) to every slice counted on one receiver input. The
// Thetis and mi0bot file names below point at where those rules are
// quoted; no upstream code is reproduced here.

// =================================================================
// src/core/SharedInputLowPass.h  (NereusSDR)
// =================================================================
//
// Shared-input filters, ruling (c) 2026-09-30: which receiver the receive
// low-pass on a shared input follows. The connections call this to pick the
// filter, and RadioModel calls it to name the slice in the low-pass reason
// (AlexAdcState::lowPassSlice), so the reason always names the receiver the
// filter follows.
//
// One frequency source per filter, the one Thetis uses for it:
//
//  - The Alex receive low-pass (every Alex board, Protocol 1 and 2) is
//    chosen from the receiver's DDS frequency, Thetis's rx1_dds_freq_mhz /
//    rx2_dds_freq_mhz (console.cs:15487-15498 UpdateAlexTXFilter
//    [v2.10.3.15]). Both protocols go through the same setAlexLPF. Under
//    click-tune that is the DDC centre, not the VFO: RX1's DDS frequency
//    takes the VFO only with click-tune off, and the centre frequency
//    otherwise (console.cs:31894-31910 [v2.10.3.15]). Here: the receiver's
//    DDC centre.
//  - The HL2's receive filter is the N2ADR board, a bank of low-pass
//    filters chosen by the OC pins of a band, and that band is the VFO's
//    (console.cs:29101-29106 [v2.10.3.15] BandByFreq(VFOAFreq) into
//    UpdateExtCtrl). Which receiver's band: the one with the highest
//    frequency, read as for the Alex low-pass (the DDC centre). A low-pass
//    set for the highest counted receiver passes every lower one.
//    Divergence (maintainer ruling, 2026-09-30): NereusSDR orders by
//    frequency; mi0bot compares band enum values (Penny.cs:158-159 and
//    183-189 [@c26a8a4], idxb > idx over enums.cs:280-322), which rank WWV
//    and the SWL bands above 10 m, so enum order can pick a lower filter.
//    A receiver whose band is outside mi0bot's range (GEN, XVTR;
//    OcMatrix::extCtrlBandIndex -1, where mi0bot sends no pins,
//    Penny.cs:162-165) ranks below every receiver inside it.
//
// One ordering frequency serves both filters (the DDC centre); they differ
// only in what the chosen receiver sets: the Alex row for its centre, or the
// N2ADR pins for its VFO's band (ruleHz).
//
// Candidates are one per hardware receiver slot, in slot order, RX1 first.
// On a frequency tie the earlier candidate keeps it, as mi0bot's strict
// compare keeps RX1.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 - Review fix: the notes describe the upstream rules and
//                cite them, with no upstream text quoted. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - HL2 pins ordered by frequency, not mi0bot's band enum
//                (maintainer ruling on review I-2). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - hl2ReceivePins: the HL2's unkeyed receive pins, with the
//                N2ADR broadcast-band high-pass (bit 6) cleared when a
//                counted slice's own pins lack it (JJ's ruling). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Review fix: a top receiver whose own mask lacks bit 6 is
//                named in highPassOff; hl2PinsReceiver is the one place the
//                pins' receiver is chosen. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
// =================================================================

#include <QList>
#include <QtGlobal>

#include "core/OcMatrix.h"
#include "models/Band.h"

namespace NereusSDR::SharedInputLowPass {

enum class Rule {
    HighestCentre,     // Alex receive low-pass: the row for the DDC centre
    HighestCentrePins, // HL2 N2ADR receive pins: the pins for the VFO's band
};

struct Candidate {
    int     slot {-1};       // hardware receiver slot (AlexRxBpf::countedSlotsAdc0 bit)
    quint64 centreHz {0};    // the DDC centre commanded to that slot
    quint64 vfoHz {0};       // the VFO of the slice the slot is known by, 0 if none
};

// The frequency the chosen receiver's filter is set from: the centre for
// the Alex row, the VFO's band for the HL2 pins (its centre when no VFO was
// told). 0 means "not tuned".
inline quint64 ruleHz(Rule rule, const Candidate& c) noexcept
{
    if (rule == Rule::HighestCentrePins) {
        return (c.vfoHz != 0) ? c.vfoHz : c.centreHz;
    }
    return c.centreHz;
}

// The frequency receivers are ordered by: the DDC centre, for both rules
// (on the HL2 the VFO when no centre was told).
inline quint64 orderHz(Rule rule, const Candidate& c) noexcept
{
    if (rule == Rule::HighestCentrePins && c.centreHz == 0) {
        return c.vfoHz;
    }
    return c.centreHz;
}

// Index into `candidates` of the receiver the low-pass follows, or -1 when
// none is tuned. Highest orderHz wins; on the HL2 a receiver whose band
// has no place in mi0bot's range ranks below every one that has. A tie
// keeps the earlier candidate.
inline int highest(Rule rule, const QList<Candidate>& candidates) noexcept
{
    int best = -1;
    quint64 bestHz = 0;
    bool bestInRange = false;
    for (int i = 0; i < candidates.size(); ++i) {
        const Candidate& c = candidates.at(i);
        const quint64 hz = orderHz(rule, c);
        if (hz == 0) { continue; }
        bool inRange = true;
        if (rule == Rule::HighestCentrePins) {
            inRange = OcMatrix::extCtrlBandIndex(
                          bandFromFrequency(static_cast<double>(ruleHz(rule, c)))) >= 0;
        }
        const bool better = (best < 0)
            || (inRange && !bestInRange)
            || (inRange == bestInRange && hz > bestHz);
        if (better) {
            best = i;
            bestHz = hz;
            bestInRange = inRange;
        }
    }
    return best;
}

// ── The HL2's receive pins on a shared input ─────────────────────────────
//
// JJ's ruling of 2026-09-30, for the HL2 unkeyed (in Auto and wherever the
// board is not bypassed): the pins are the receive mask of the counted
// receiver with the highest frequency (highest() above), with bit 6 cleared
// unless every counted receiver's own receive mask has bit 6. A difference
// between the masks never sends 0x00; only ForceBypass and WidebandLocked
// do (the hasIoBoardHl2 block in P1RadioConnection's OC byte build).
//
// From N2ADR's page for the board (https://james.ahlstrom.name/hl2filter/):
// bits 0-5 select the low-pass filters (160, 80, 60/40, 30/20, 17/15,
// 12/10 m); bit 6 (pin 7) is a 3 MHz receive high-pass that rejects AM
// broadcast and should be used on every band except 160 m; with several
// receivers the receive filter for the highest band is used. The page does
// not say what all bits zero does.
//
// So a slice on 160 m (the N2ADR preset gives 160 m no bit 6), on GEN, or
// on any band the operator set without pin 7, turns the high-pass off for
// the whole input rather than having it cut that slice's signal. The rule
// reads the operator's own pin table; no band is named here.
//
// mi0bot's HERMESLITE receive arm (Penny.cs:183-189 [@c26a8a4]) takes the
// higher band's receive mask whole, with no bit-6 handling.
inline constexpr quint8 kN2adrBroadcastHighPassBit = 0x40;  // bit 6, pin 7

struct Hl2ReceivePins {
    int         best {-1};     // index of the receiver the pins follow, -1 if none tuned
    quint8      pins {0};      // the byte to send, valid when best >= 0
    QList<int>  highPassOff;   // indices of the receivers bit 6 is off for, empty when
                               // every counted receiver's own mask has it or none does
    bool        boardOff {false};  // pins is 0x00 on a board with receive pins set
};

// Whether any band has receive pins set: an N2ADR (or other) board is
// configured. With none set the pins are 0x00 on every band, as they
// always were, and nothing is reported off.
inline bool hasReceivePins(const OcMatrix& oc)
{
    for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
        if (oc.maskFor(static_cast<Band>(b), /*tx=*/false) != 0) { return true; }
    }
    return false;
}

// The receiver the HL2's pins follow. One place, so the rule for a top
// receiver whose band has no pins set can change here alone.
inline int hl2PinsReceiver(const OcMatrix& /*oc*/, const QList<Candidate>& candidates)
{
    return highest(Rule::HighestCentrePins, candidates);
}

inline Hl2ReceivePins hl2ReceivePins(const OcMatrix& oc,
                                     const QList<Candidate>& candidates)
{
    Hl2ReceivePins result;
    result.best = hl2PinsReceiver(oc, candidates);
    if (result.best < 0) { return result; }
    const auto maskOf = [&oc](const Candidate& c) {
        return oc.maskFor(bandFromFrequency(static_cast<double>(
                              ruleHz(Rule::HighestCentrePins, c))), /*tx=*/false);
    };
    result.pins = maskOf(candidates.at(result.best));
    bool anyHas = false;
    QList<int> lacking;
    for (int i = 0; i < candidates.size(); ++i) {
        const Candidate& c = candidates.at(i);
        if (orderHz(Rule::HighestCentrePins, c) == 0) { continue; }
        if ((maskOf(c) & kN2adrBroadcastHighPassBit) != 0) {
            anyHas = true;
        } else {
            lacking.append(i);
        }
    }
    if ((result.pins & kN2adrBroadcastHighPassBit) != 0) {
        // The top receiver's mask has it: cleared for the ones that lack it.
        if (!lacking.isEmpty()) {
            result.pins = quint8(result.pins & ~kN2adrBroadcastHighPassBit);
            result.highPassOff = lacking;
        }
    } else if (anyHas) {
        // The top receiver's own mask lacks it while another's has it: the
        // high-pass is off for the top receiver, so it is the one named.
        result.highPassOff.append(result.best);
    }
    // JJ's ruling of 2026-09-30: when the pins sent come to 0x00 (the top
    // receiver's band has no pins set, WWV or a band the operator left
    // empty), the board is off, and RadioModel reports it so (WIDE, no
    // low-pass sentence). Only on a board with some receive pins set.
    if (result.pins == 0 && hasReceivePins(oc)) {
        result.boardOff = true;
        result.highPassOff.clear();
    }
    return result;
}

} // namespace NereusSDR::SharedInputLowPass
