// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/TxRefusal.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-13). See TxRefusal.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 35 (R-IOS-13): otherDeviceHoldsStop
//               and keyEnded. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-26: Transmit group fix wave C1: micNotConnected, a remote
//               voice key with no microphone line. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: Transmit group fix wave: M4 remoteMicNotReady. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 77 fix wave, I3: radioOnAir. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 7: noTransmitSlice. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 11: chooseTransmitSlice. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-30: TX safety: radioLinkDown. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/safety/TxRefusal.h"

#include <QRegularExpression>

namespace NereusSDR::TxRefusals {

namespace {

TxRefusal make(const char* code, QString text, const char* fix = nullptr)
{
    return {QByteArray(code), std::move(text), fix != nullptr ? QByteArray(fix) : QByteArray()};
}

// The band plan's own reasons are plain sentences. One that ever names an
// internal term (OperatorWording's list, kept here as the same few words so
// NereusCore needs no GUI code) is replaced by the general sentence.
bool bandPlanReasonIsPlain(const QString& reason)
{
    static const QRegularExpression internal(
        QStringLiteral("\\b(DSP|WDSP|slot|session|protocol|peer|snapshot|capabilit|budget|grant)"),
        QRegularExpression::CaseInsensitiveOption);
    return !reason.trimmed().isEmpty() && !internal.match(reason).hasMatch();
}

} // namespace

TxRefusal notReady()
{
    return make(kNotReady,
                QStringLiteral("This device is still connecting to the Core. Try again in a moment."));
}

TxRefusal appCannotTransmit()
{
    return make(kNotReady, QStringLiteral("Update this app to transmit through this Core."));
}

TxRefusal deviceNotPaired()
{
    return make(kNotReady, QStringLiteral("Pair this device with the Core to transmit through it."));
}

TxRefusal stationReceiveOnly()
{
    return make(kStationReceiveOnly, QStringLiteral("This Core is set to receive only."));
}

TxRefusal bandPlan(const QString& reason)
{
    return make(kBandPlan, bandPlanReasonIsPlain(reason)
                               ? reason
                               : QStringLiteral("The band plan does not allow transmitting here."));
}

TxRefusal txInhibited()
{
    return make(kInterlock,
                QStringLiteral("The radio's transmit inhibit input is holding transmit off."));
}

TxRefusal radioLinkDown()
{
    return make(kInterlock, QStringLiteral("The link to the radio is down."));
}

TxRefusal interlock()
{
    return make(kInterlock,
                QStringLiteral("The transmit interlock is holding transmit off. Check it in Setup."));
}

TxRefusal ampStandby()
{
    return make(kAmpStandby,
                QStringLiteral("The amplifier is in standby. Operate it, or change the interlock in Setup."),
                kFixOperateAmp);
}

TxRefusal paProtection()
{
    return make(kPaProtection,
                QStringLiteral("The amplifier has tripped. Reset it before transmitting."));
}

TxRefusal swr()
{
    return make(kSwr, QStringLiteral("The SWR is over the interlock's limit. Check the antenna, "
                                     "or change the interlock in Setup."));
}

TxRefusal otherDeviceHolds(const QString& holderName)
{
    return make(kOtherDeviceHolds, QStringLiteral("%1 has the transmitter.").arg(holderName),
                kFixTakeTransmit);
}

TxRefusal programNeedsTransmit()
{
    return make(kProgramNeedsTransmit,
                QStringLiteral("A program can transmit only while this device has transmit. "
                               "Take transmit here first."),
                kFixTakeTransmit);
}

TxRefusal micNotReady()
{
    // The same sentence RadioModel's microphone check gives (R-R3-36).
    return make(kMicNotReady,
                QStringLiteral("Microphone is not ready. Check Audio settings and retry."));
}

TxRefusal micNotConnected()
{
    // Fix wave C1: the Core never keys a remote device's voice on its own
    // microphone; the device waits for its line.
    return make(kMicNotReady,
                QStringLiteral("This device's microphone is not connected to the Core. "
                               "Wait a moment and try again."));
}

TxRefusal remoteMicNotReady()
{
    // Fix wave M4: the device's own microphone, not the Core's Audio
    // settings, is what to wait for.
    return make(kMicNotReady,
                QStringLiteral("No sound has reached the Core from this device's microphone. "
                               "Wait a moment and try again."));
}

TxRefusal changingHands()
{
    return make(kChangingHands,
                QStringLiteral("Transmit is changing hands. Try again in a moment."));
}

TxRefusal noTransmitSlice()
{
    return make(kNoTransmitSlice,
                QStringLiteral("There is no slice to transmit on. Add a slice first."));
}

TxRefusal chooseTransmitSlice()
{
    return make(kChooseTransmitSlice,
                QStringLiteral("You took this slice from another device. Choose it for "
                               "transmit first with its TX button."));
}

TxRefusal stopNotConfirmed()
{
    return make(kStopNotConfirmed,
                QStringLiteral("The radio did not confirm it stopped transmitting."));
}

TxRefusal holderOnAir(const QString& holderShortName, bool radioPtt)
{
    // D60's words, the holder's short name in place of "The iPhone";
    // "The radio is on the air." while the radio's own PTT holds transmit
    // (the several-devices design, ruling 7.4), with the Core's own words
    // for the rest (RadioModel's refusal of a tuner switch while it
    // transmits, and TunerApplet::onAirReason).
    return make(kHolderOnAir,
                radioPtt ? QStringLiteral("The radio is on the air. Try again when it stops.")
                         : QStringLiteral("%1 is on the air. Try again when they stop.")
                               .arg(holderShortName),
                kFixTakeTransmit);
}

TxRefusal radioOnAir()
{
    // holderOnAir's words for the radio, without its fix: the device asking
    // may be the one on the air, and taking transmit would not help.
    return make(kHolderOnAir, QStringLiteral("The radio is on the air. Try again when it stops."));
}

TxRefusal notHolder()
{
    return make(kNotHolder, QStringLiteral("Take transmit on this device first."),
                kFixTakeTransmit);
}

TxRefusal otherDeviceHoldsStop(const QString& holderName)
{
    // The several-devices design, ruling 8.5: a device that does not hold
    // transmit stops a transmission only by taking transmit.
    return make(kOtherDeviceHolds,
                QStringLiteral("%1 has the transmitter. Take it to stop the transmission.")
                    .arg(holderName),
                kFixTakeTransmit);
}

TxRefusal keyEnded()
{
    return make(kKeyEnded,
                QStringLiteral("The Core already stopped this transmission. Key again to transmit."));
}

} // namespace NereusSDR::TxRefusals
