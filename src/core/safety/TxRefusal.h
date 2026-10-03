// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/TxRefusal.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-13, R-IOS-02, D60): why the Core refused
// a key or a change while someone transmits, as one record the link
// carries: a stable code an app keys its screens on, the plain sentence the
// Core shows, and the fix an app may offer (a button that does it).
//
// The codes and their sentences are the link document's Transmit section
// (docs/architecture/2026-09-23-station-link-v1.md). Every sentence passes
// OperatorWording::isPlain (tst_tx_refusal holds them to it, with device
// names such as "Grant's iPhone" put in).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 35 (R-IOS-13): keyEnded (a copy of a
//               key the Core has already stopped) and the holder's unkey
//               refusal. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
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
//   2026-09-29: slice control plan Task 11: chooseTransmitSlice. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: TX safety: radioLinkDown, every key while the link to the
//               radio is lost. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

namespace NereusSDR {

struct TxRefusal {
    /// The refusal's stable name (notReady, otherDeviceHolds, ...). Empty
    /// for no refusal.
    QByteArray code;
    /// The sentence the operator reads.
    QString text;
    /// What an app may offer to fix it (takeTransmit, operateAmp), or empty.
    QByteArray fix;

    bool isEmpty() const { return code.isEmpty(); }
    bool operator==(const TxRefusal& other) const = default;
};

namespace TxRefusals {

// ---- Codes (the link document's Transmit section) -------------------------
inline constexpr char kNotReady[] = "notReady";
inline constexpr char kStationReceiveOnly[] = "stationReceiveOnly";
inline constexpr char kBandPlan[] = "bandPlan";
inline constexpr char kInterlock[] = "interlock";
inline constexpr char kAmpStandby[] = "ampStandby";
inline constexpr char kPaProtection[] = "paProtection";
inline constexpr char kSwr[] = "swr";
inline constexpr char kOtherDeviceHolds[] = "otherDeviceHolds";
inline constexpr char kProgramNeedsTransmit[] = "programNeedsTransmit";
inline constexpr char kMicNotReady[] = "micNotReady";
/// The design's "Transmit is changing hands." (ruling 8.2 step 1).
inline constexpr char kChangingHands[] = "changingHands";
/// The design's "The radio did not confirm it stopped transmitting."
/// (ruling 8.2 step 2).
inline constexpr char kStopNotConfirmed[] = "stopNotConfirmed";
/// Ruling 7.4 (D60): a change refused while the holder is on the air.
inline constexpr char kHolderOnAir[] = "holderOnAir";
/// The holder's own verb (tx.setTxSlice) from a device while transmit is
/// unheld (Task 34's choice, for the controller's review).
inline constexpr char kNotHolder[] = "notHolder";
/// Task 35: a copy of a key (the same command) that arrives after the Core
/// stopped the transmission it started. It never keys again.
inline constexpr char kKeyEnded[] = "keyEnded";
/// Slice control plan Task 7: a key on a Core with no slice.
inline constexpr char kNoTransmitSlice[] = "noTransmitSlice";
/// Slice control plan Task 11 (ruling Q8): a key that would land on a slice
/// the device took control of from another device and has not chosen for
/// transmit, with no other slice it may transmit on.
inline constexpr char kChooseTransmitSlice[] = "chooseTransmitSlice";

// ---- Fixes ----------------------------------------------------------------
inline constexpr char kFixTakeTransmit[] = "takeTransmit";
inline constexpr char kFixOperateAmp[] = "operateAmp";

// ---- The refusals ---------------------------------------------------------

/// The device's sign-in has not finished (its snapshot is not complete).
TxRefusal notReady();
/// The app does not declare remote transmit (code notReady).
TxRefusal appCannotTransmit();
/// The device signed in without a paired device key (code notReady).
TxRefusal deviceNotPaired();
/// The Core's remote_transmit is deny.
TxRefusal stationReceiveOnly();
/// A band-plan or mode refusal, in the band plan's own words.
TxRefusal bandPlan(const QString& reason);
/// The radio's TX inhibit input holds transmit off.
TxRefusal txInhibited();
/// TX safety (2026-09-30, code interlock): every key while the link to the
/// radio is lost and until it is back: "The link to the radio is down."
TxRefusal radioLinkDown();
/// The transmit interlock refused, for a reason other than the two below.
TxRefusal interlock();
/// The interlock refused because the amplifier is in standby.
TxRefusal ampStandby();
/// The amplifier (PA) protection has tripped.
TxRefusal paProtection();
/// The interlock refused because the SWR is over its limit.
TxRefusal swr();
/// Another device holds transmit. `holderName` as the Core sends it, or
/// "Radio" after the radio's own PTT took transmit.
TxRefusal otherDeviceHolds(const QString& holderName);
/// A program's key while its device does not hold transmit.
TxRefusal programNeedsTransmit();
/// The PC microphone is not ready.
TxRefusal micNotReady();
/// Fix wave C1 (code micNotReady): a remote device's voice or program key
/// while its media carries no microphone line.
TxRefusal micNotConnected();
/// Fix wave M4 (code micNotReady): a remote device's line carried no sound
/// within the key's wait; the device waits for its microphone.
TxRefusal remoteMicNotReady();
/// Every key during a transfer of transmit.
TxRefusal changingHands();
/// Slice control plan Task 7: every key while the Core has no slice to
/// transmit on.
TxRefusal noTransmitSlice();
/// Slice control plan Task 11: a key on a slice taken from another device
/// and not chosen for transmit (the TX button on the slice chooses it).
TxRefusal chooseTransmitSlice();
/// Every key after a transfer that ended with MOX still on, until it
/// reads off.
TxRefusal stopNotConfirmed();
/// A change from another device while the holder is on the air.
/// `radioPtt` true when the station device holds transmit (the radio's own
/// PTT, or the Core's own keys): "The radio is on the air. Try again when
/// it stops."
TxRefusal holderOnAir(const QString& holderShortName, bool radioPtt);
/// iPhone app plan Task 77 fix wave, I3 (code holderOnAir, no fix): a
/// Tuner Genius autotune asked for while the radio is on the air, by the
/// asking device's own key too: "The radio is on the air. Try again when it
/// stops."
TxRefusal radioOnAir();
/// The holder's verb while transmit is unheld.
TxRefusal notHolder();
/// Task 35 (ruling 8.5): tx.unkey, or a TUNE or two-tone stop, from a device
/// that does not hold transmit (code otherDeviceHolds): "<holder> has the
/// transmitter. Take it to stop the transmission."
TxRefusal otherDeviceHoldsStop(const QString& holderName);
/// Task 35: a copy of a key that arrives after the Core stopped it.
TxRefusal keyEnded();

} // namespace TxRefusals

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::TxRefusal)
