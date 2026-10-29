// no-port-check: NereusSDR-original. See CwKeyerGate.h.
// =================================================================
// src/core/safety/CwKeyerGate.cpp  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the keyer gate (R-CW-05, R-CW-06, D16).
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-26: CW transmit early fix wave M5: the not-CW refusal carries
//               notReady, not the band plan's code. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include "core/safety/CwKeyerGate.h"

#include "core/station/StationRadios.h"

namespace NereusSDR::safety {

namespace {

CwKeyerGateResult refused(TxRefusal refusal)
{
    return CwKeyerGateResult{false, std::move(refusal)};
}

TxRefusal withCode(const char* code, const QString& text)
{
    return TxRefusal{QByteArray(code), text, QByteArray()};
}

// The interlock's words keep the TxRefusal they came from (MoxController's
// interlockRefusal), so a remote app still sees ampStandby with its fix.
TxRefusal interlockRefusalFor(const QString& text)
{
    for (const TxRefusal& known : {TxRefusals::ampStandby(), TxRefusals::swr(),
                                   TxRefusals::interlock()}) {
        if (known.text == text) {
            return known;
        }
    }
    return withCode(TxRefusals::kInterlock, text);
}

} // namespace

CwKeyerGateResult evaluateCwKeyerGate(const CwKeyerGateInputs& in)
{
    if (!in.hasTxSlice) {
        return refused(TxRefusals::noTransmitSlice());
    }
    if (!in.txSliceInCw) {
        // Early fix wave M5: not the band plan's refusal. notReady is the
        // closest existing code (the keyer is not ready in this mode); the
        // codes are strings on the link, so none is renumbered.
        return refused(withCode(TxRefusals::kNotReady, QString::fromLatin1(kCwGateNotCwText)));
    }
    if (in.receiveOnly) {
        return refused(TxRefusals::stationReceiveOnly());
    }
    if (in.txInhibited) {
        return refused(TxRefusals::txInhibited());
    }
    if (in.paTripped) {
        return refused(TxRefusals::paProtection());
    }
    if (in.radioLinkDown) {
        return refused(TxRefusals::radioLinkDown());
    }
    if (in.radioChangeUnderway) {
        return refused(withCode(TxRefusals::kNotReady, StationRadios::switchingReason()));
    }
    if (in.stopLatched) {
        return refused(withCode(TxRefusals::kInterlock,
                                QString::fromLatin1(kCwGateStopLatchedText)));
    }
    if (!in.bandPlan.ok) {
        // A band-plan check that names its own code (the Core's receive-only
        // or not-ready refusals) keeps it; an empty code is the band plan's.
        if (!in.bandPlan.refusalCode.isEmpty()) {
            return refused(TxRefusal{in.bandPlan.refusalCode, in.bandPlan.reason, QByteArray()});
        }
        return refused(TxRefusals::bandPlan(in.bandPlan.reason));
    }
    if (!in.interlockRefusal.isEmpty()) {
        return refused(interlockRefusalFor(in.interlockRefusal));
    }
    if (in.interlockArmed && !in.interlockGranted) {
        return refused(withCode(TxRefusals::kInterlock,
                                QString::fromLatin1(kCwGateInterlockNotGrantedText)));
    }
    return CwKeyerGateResult{true, TxRefusal{}};
}

} // namespace NereusSDR::safety
