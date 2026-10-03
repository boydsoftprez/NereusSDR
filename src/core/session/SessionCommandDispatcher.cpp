// =================================================================
// src/core/session/SessionCommandDispatcher.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 11.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal 2: startLevelCalibration
//                                    calibrates only a slice the device may
//                                    change, named or active. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: startLevelCalibration and
//                                    cancelLevelCalibration
//                                    (radioHardwareVersion 12). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: resetLevelCalibration
//                                    (radioHardwareVersion 12). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Parity ruling C4: setRadioSampleRate
//                                    (radioHardwareVersion 9). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Task 24: Core-owned Settings Hygiene
//                                    commands and current-radio guards.
//                                    AI-assisted implementation via Codex.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11: command
//                                    dispatch (addSlice / removeSlice /
//                                    requestSliceSampleRate /
//                                    addSliceOnPan). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11 fix round 1:
//                                    added setActiveSliceById (review
//                                    Important 1) plus same-thread-
//                                    invariant notes on the by-reference
//                                    lambda captures (Minor 7). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 1:
//                                    findIntArgument() replaces four bare
//                                    QVariant::toInt() narrows that
//                                    silently truncated an out-of-range
//                                    id to 32 bits. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-40: nnr.tryAgain (a slice ID)
//                                    clears the runtime NNR limit; the
//                                    server admits it from minor 11.
//                                    AI-assisted implementation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-21 / R-R3-09: notch.add,
//                                    notch.move, notch.setActive and
//                                    notch.delete on the Core's notch list.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46: requestIoBoardProbe, the HL2
//                                    I/O board probe for a remote window.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 fix wave (radioHardwareVersion
//                                    3): setAlexRxAntenna, one band's RX or
//                                    RX-only antenna. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-21 (radioHardwareVersion
//                                    4): setAlexBpfMode, one receive filter
//                                    chain's filter policy. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: configurePgxl,
//                                    disconnectPgxl and
//                                    setPgxlConnectionSettings for the
//                                    Core's Power Genius XL. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01):
//                                    verbSpecs(), the declared verb table.
//                                    Routing unchanged. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-48: configureRfKit,
//                                    disconnectRfKit and setRfKitEnabled
//                                    for the Core's RF-Kit RF2K-S, and
//                                    setStationTci for the station's TCI
//                                    server. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 2 (R-IOS-01): the
//                                    RF-Kit and station TCI verbs in
//                                    verbSpecs(). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: setTxInterlockPolicy,
//                                    setPgxlPowerCap and clearAccessoryFaults
//                                    (accessoryDataVersion 1). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): the
//                                    accessory record verbs in
//                                    verbSpecs(). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4b (R-IOS-01,
//                                    R-R3-21): every refusal reason in
//                                    operator words. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    a PureSignal request's arguments are
//                                    read before the transmit gate.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: the amp's and
//                                    tuner's own settings (setPgxlName,
//                                    setPgxlHardware, setPgxlNetwork,
//                                    savePgxlSettings, readPgxlSettings and
//                                    the four setTgxl* / *TgxlSettings
//                                    verbs). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47: resetRfKitError (the RF-Kit
//                                    page's Reset amp error). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Lane B takes integration (R-IOS-01,
//                                    R-R3-21): the filter policy request's
//                                    unreadable-request reason in plain
//                                    words.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-47: setTgxlAntenna,
//                                    setTgxlOperate and setTgxlBypass
//                                    (remoteTgxlControlVersion 2).
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08):
//                                    devices.revoke, station.rename,
//                                    station.acknowledgeKeyBackup and
//                                    station.retireToken.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08):
//                                    pairing.open and pairing.close.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2):
//                                    setTunePowerForTxBand
//                                    (transmitSettingsVersion 2).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3):
//                                    txProfile.select, txProfile.save,
//                                    txProfile.delete and rade.resetVocoder
//                                    (transmitSettingsVersion 3).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): session.leave
//               (sessionHolderVersion 1). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 72 (R-IOS-02, ruling 5.8): the owner per
//               session; ending one owner cancels only its DSP-asset jobs.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app Task 73 (R-IOS-02, rulings 5.9, 5.10): the
//               requesting device; removeSlice, setActiveSliceById, nnr.*
//               and notch.add refused for another device's slice; a new
//               slice is its requester's; setActiveSliceById sets the
//               requester's own active slice. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 34 (R-IOS-02):
//                                    tx.setTxSlice and the on-air refusals
//                                    (setTransmitAccess), refusals with
//                                    refusalCode and refusalFix values.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 35 (R-IOS-13):
//                                    tx.key, tx.unkey, tx.tune and
//                                    tx.twoTone, routed to the Core's
//                                    RemoteKeying; an accepted key's
//                                    epoch in the result's values.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 37 (R-IOS-13):
//                                    tx.keepalive {sequence, epoch} for the
//                                    transmit watchdog.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: slice.selectBand
//                                    (bandSelectVersion 1), the desktop's
//                                    band button on a slice.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: notch.addAtSlice
//                                    (notchControlVersion 2), the desktop's
//                                    +TNF on a slice.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Checkpoint join (R-IOS-02, R-IOS-27):
//                                    slice.selectBand and notch.addAtSlice
//                                    refused for another device's slice.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 7): ps3.single,
//                                    ps3.automatic, ps3.applyCurrent and
//                                    ps3.restoreCorrection taken off the air
//                                    from a peer offered
//                                    transmitSettingsVersion 7; ps3.twoTone
//                                    stays with remote transmit.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 8): moveTgxlRelay,
//                                    scanTgxlLan and setTgxlAddress
//                                    (remoteTgxlControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9): setPgxlOperate,
//                                    scanPgxlLan and setPgxlAddress
//                                    (remotePgxlControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10):
//                                    setRfKitOperate, setRfKitAntenna,
//                                    setRfKitTciMode and setRfKitAddress
//                                    (remoteRfKitControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-46 parity mini-round
//                                    (radioHardwareVersion 6):
//                                    setAlexTxAntenna, one band's TX
//                                    antenna. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-46 (parity Task 14,
//                                    radioHardwareVersion 7):
//                                    requestIoBoardI2c and setIoBoardOutput.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: M2 TransmitAccess::release, a
//               two-tone stop from another device refused. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 16,
//                                    dspInfoVersion 1): dsp.filterResponse,
//                                    the filter graph's curve.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): tx.take,
//                                    tx.tunerTune, ps3.twoTone on as a key,
//                                    txProfile.select held by the holder,
//                                    tx.setTxSlice for the holder's own
//                                    slices. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-IOS-25 / R-R3-49 (parity Task 19,
//                                    recordStreamVersion 1):
//                                    records.subscribe, records.unsubscribe
//                                    and the spots.* verbs.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-IOS-18 / R-R3-49 (parity Task 21,
//                                    stationRadiosVersion 1): the station
//                                    radio verbs.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  R-IOS-26 / R-R3-49 (iPhone plan Task
//                                    22, parity Task 20,
//                                    stationFreedvVersion 1): the freedv.*
//                                    verbs. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): session.pathTicket in
//               the verb table (controlSwitchVersion 1). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: remote-window parity Task 22 / iPhone app plan Task 25
//               (R-R3-49, R-IOS-18, supportBundleVersion 1): support.collect
//               and support.setLogCategories. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: Parity Task 23 (R-R3-48, R-R3-42, R-R3-49,
//               stationTciVersion 2): setStationTciOptions and
//               disconnectStationTciClient, refused while the radio is on
//               the air. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-29: setStationTciSettings (JJ's ruling of 2026-09-28,
//               stationTciSettingsVersion 1), refused while the radio is on
//               the air. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-27: R-IOS-13 / R-R3-49: txModMonitor.reset in the verb table
//               (txModMonitorVersion 1), answered by the station server
//               beside the record streams. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28: R-IOS-13 / R-R3-49: txEq.setCurve and txEq.resetCurve
//               (txEqCurveVersion 2), applied by the station server as its
//               txEqParaEqData write (TxEqCurveAccess). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: Remote parity on the air (transmitSettingsVersion 13): the
//               transmit-setting commands, PureSignal arming and
//               tx.twoTonePreset are taken on the air from a peer that may
//               change the transmit settings. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: setTunePowerForTxBand, txProfile.save / delete and
//               rade.resetVocoder are the holder's while transmit is held
//               (refusedForTheHolder, ruling 7.7), as txProfile.select was.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18 (paProfileVersion 1): the paProfile
//                 verbs (handlePaProfile). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29: transmitSettingsVersion 15: cfc.setProfile, applied by the
//               station server as its cfcParaEqData write
//               (CfcProfileAccess). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 2: the
//                                    slice access check is the change
//                                    predicate (SliceAccessPolicy), so a
//                                    listener's verbs are refused.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 4:
//                                    slice.listen, slice.stopListening,
//                                    slice.takeControl and slice.release;
//                                    setActiveSliceById on a listened
//                                    slice; removeSlice as a release from a
//                                    controller others listen with (ruling
//                                    Q6). AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control fix wave (Important 4):
//                                    TransmitAccess::txSliceChosen, a
//                                    device's explicit tx.setTxSlice
//                                    choice. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 7: a device
//                                    closing the Core's last slice with
//                                    nobody else on it releases it, and it
//                                    closes. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 6:
//                                    slice.setListenLevel, a listener's own
//                                    level and mute for a slice it hears.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/SessionCommandDispatcher.h"

#include "core/TxSliceArbiter.h"

#include "core/SliceOwnership.h"
#include "core/SpotSourceHost.h"
#include "core/LogCategories.h"
#include "core/station/StationRadios.h"
#include "core/AppSettings.h"
#include "core/SettingsHygiene.h"
#include "core/session/SettingsHygieneWire.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/SliceAccessController.h"
#include "core/dsp/DspAssetService.h"
#include "DspCommandValues.h"
#include "PureSignalSessionFacade.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/session/StationDevicesFacade.h"
#include "models/BandGrid.h"
#include "models/RadioModel.h"
#include "models/StationTciModel.h"
#include "models/SliceModel.h"

#include <QHash>
#include <QMetaObject>
#include <QMetaType>
#include <QSet>
#include <QThread>
#include <QVariant>

#include <algorithm>
#include <limits>
#include <initializer_list>
#include <cmath>

namespace NereusSDR {

namespace {

// Looks up one named argument out of a CommandInvoke's arguments list.
// `arguments` reuses MirrorUpdate as a generic {name, kind, value} triple
// (SessionMessage::arguments' own doc comment) -- `kind` and `ordinal` are
// not consulted here, only `name` and `value`. Every INTEGER argument goes
// through findIntArgument() below rather than narrowing the QVariant at
// the call site; only the string arguments (initialPanId, panId) still
// read this directly, and QVariant::toString() has no range to fall off.
bool findArgument(const QList<MirrorUpdate>& arguments, const QByteArray& name, QVariant* out)
{
    for (const MirrorUpdate& arg : arguments) {
        if (arg.name == name) {
            *out = arg.value;
            return true;
        }
    }
    return false;
}

bool findUtf8Argument(const QList<MirrorUpdate>& arguments, const QByteArray& name,
                      QString* out)
{
    for (const MirrorUpdate& arg : arguments) {
        if (arg.name == name) {
            if (arg.kind != MirrorWireKind::Utf8 || arg.value.typeId() != QMetaType::QString) {
                return false;
            }
            *out = arg.value.toString();
            return true;
        }
    }
    return false;
}

bool hasWireKind(const QList<MirrorUpdate>& arguments, const QByteArray& name,
                 MirrorWireKind kind)
{
    for (const MirrorUpdate& arg : arguments) {
        if (arg.name == name) {
            return arg.kind == kind;
        }
    }
    return false;
}

// What findIntArgument() found. Three states rather than a bool, because
// "you did not send sliceId" and "the sliceId you sent is not a number
// this station can act on" are different things: the peer is told the
// Core could not read the request, or could not use one of its values.
// Both are in operator words (R-IOS-01).
enum class ArgumentStatus {
    Ok,
    Missing,
    NotRepresentable,
};

// Every id and rate argument in this file is an `int` on RadioModel's
// side, and every one of them arrives from the far side of a socket.
//
// Fix round 5 review finding (Important 1): each of these used to be a
// bare QVariant::toInt() with the `ok` flag discarded. Measured on this
// tree's Qt, QVariant(qlonglong 4294967296).toInt() returns 0 with
// ok == true and 4294967297 returns 1, so a peer asking to remove slice
// 4294967296 removed slice 0 and got back accepted with affected
// ["slice:0"] -- a request naming an object that does not exist
// destroying a DIFFERENT object that does. One helper rather than four
// checked narrows at four call sites: the four sites want identical
// semantics, the next verb added to this file gets the safe behaviour by
// construction, and the wording a peer sees stays in one place.
//
// The runtime type is checked, not the declared MirrorWireKind. The two
// carry the same information -- SessionMessages' decoder collapses each
// wire kind onto exactly one QVariant runtime type (fromJsonValue:
// bool / qlonglong for Int64 and Enum / double / QString) -- but the
// runtime type is the stronger statement, since it also holds for an
// in-process caller that built the MirrorUpdate directly. Refusing a
// double outright also keeps this code out of QVariant's own
// floating-to-integral conversion, which is not a narrowing this layer
// should be performing on untrusted input.
ArgumentStatus findIntArgument(const QList<MirrorUpdate>& arguments,
                               const QByteArray& name, int* out)
{
    QVariant raw;
    if (!findArgument(arguments, name, &raw)) {
        return ArgumentStatus::Missing;
    }
    switch (raw.typeId()) {
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        break;
    default:
        return ArgumentStatus::NotRepresentable;
    }
    bool ok = false;
    const qlonglong wide = raw.toLongLong(&ok);
    if (!ok || wide < static_cast<qlonglong>(std::numeric_limits<int>::min())
        || wide > static_cast<qlonglong>(std::numeric_limits<int>::max())) {
        return ArgumentStatus::NotRepresentable;
    }
    *out = static_cast<int>(wide);
    return ArgumentStatus::Ok;
}

bool hasExactlyArguments(const QList<MirrorUpdate>& arguments,
                         std::initializer_list<QByteArray> expected)
{
    if (arguments.size() != static_cast<qsizetype>(expected.size())) {
        return false;
    }
    QSet<QByteArray> expectedNames(expected.begin(), expected.end());
    QSet<QByteArray> seen;
    for (const MirrorUpdate& argument : arguments) {
        if (!expectedNames.contains(argument.name) || seen.contains(argument.name)) {
            return false;
        }
        seen.insert(argument.name);
    }
    return true;
}

bool findFiniteDoubleArgument(const QList<MirrorUpdate>& arguments,
                              const QByteArray& name, double* out)
{
    QVariant raw;
    if (!findArgument(arguments, name, &raw) || raw.typeId() != QMetaType::Double) {
        return false;
    }
    const double value = raw.toDouble();
    if (!std::isfinite(value)) {
        return false;
    }
    *out = value;
    return true;
}

QString notRepresentableReason()
{
    return QStringLiteral("The Core could not use one of the values in this request.");
}

} // namespace

// ── The declared verb table (R-IOS-01) ───────────────────────────────────
//
// Each row is what dispatch() and the handler behind it accept: argument
// names and wire kinds as the handler checks them (every handler requires
// every argument it names, so none is optional today), and the gate a
// client applies before sending the verb, as StationClient applies it:
//
//   slice verbs            none: they predate capability gating
//   requestStreamCtun*     remoteCtunAvailable()          (StationClient.cpp)
//   configure/disconnectTgxl remoteTgxlConfigAvailable()
//   setTgxlAntenna/Operate/Bypass tgxlControlAvailable() (version 2)
//   moveTgxlRelay, scanTgxlLan, setTgxlAddress
//                          tgxlFullControlAvailable() (version 4)
//   setFourO3AEnabled      remoteFourO3AControlAvailable()
//   *Pgxl*                 remotePgxlControlAvailable() (version 2)
//   setPgxlOperate, scanPgxlLan, setPgxlAddress
//                          pgxlFullControlAvailable() (version 4)
//   *RfKit*                remoteRfKitControlAvailable() (version 2)
//   setRfKitOperate, setRfKitAntenna, setRfKitTciMode, setRfKitAddress
//                          rfKitFullControlAvailable() (version 4)
//   setStationTci          stationTciAvailable() (version 1)
//   setStationTciOptions, disconnectStationTciClient
//                          stationTciVersion 2 (requestStationTciOptions,
//                          requestDisconnectStationTciClient)
//   setStationTciSettings  stationTciSettingsVersion 1
//                          (requestStationTciSetting)
//   setTxInterlockPolicy, setPgxlPowerCap, clearAccessoryFaults
//                          accessoryDataAvailable() (version 1)
//   requestIoBoardProbe    remoteHardwareConfigAvailable() (version 2)
//   setAlexRxAntenna       radioHardwareVersion 3 (requestAlexRxAntenna)
//   setAlexTxAntenna       radioHardwareVersion 6 (requestAlexTxAntenna)
//   requestIoBoardI2c, setIoBoardOutput
//                          radioHardwareVersion 7 (requestIoBoardI2c,
//                          requestIoBoardOutput)
//   setRadioSampleRate     radioHardwareVersion 9 (requestRadioSampleRate)
//   resetLevelCalibration  radioHardwareVersion 12
//                          (requestResetLevelCalibration)
//   startLevelCalibration, cancelLevelCalibration
//                          radioHardwareVersion 12
//                          (requestStartLevelCalibration,
//                          requestCancelLevelCalibration)
//   dsp.filterResponse     dspInfoVersion 1 (requestFilterResponse)
//   records.subscribe, records.unsubscribe, spots.connect, spots.disconnect,
//   spots.sendCommand, spots.clearAll
//                          recordStreamVersion 1 (the window subscribes after
//                          its snapshot; requestSpotSource)
//   txEq.setCurve, txEq.resetCurve
//                          txEqCurveVersion 2, to a peer whose hello declared
//                          txEqCurve 2 (StationServer applies them as its
//                          txEqParaEqData write)
//   txModMonitor.reset     txModMonitorVersion 1 (the Mod Monitor's RESET in
//                          a remote window; StationServer answers it)
//   station.selectRadio, station.rescanRadios, station.setRadioModel,
//   station.forgetRadio    stationRadiosVersion 1 (requestStationRadio)
//   freedv.setMessage, freedv.sendQsy, freedv.setHidden
//                          stationFreedvVersion 1 (requestFreedv)
//   nnr.*                  nnrControlAvailable(); nnr.tryAgain adds
//                          kNnrLimitSessionProtocolMinor
//   nnr.applyModelSelection dspAssetVersion 1 (requestApplyNnrModels)
//   dspAssets.*            dspAssetVersion 1; selectNr3Model version 2
//   ps3.subscribeDisplay   psDisplayVersion 1 with media
//   ps3.<action>           psAlgorithmVersion 3
//   notch.*                remoteNotchControlAvailable()
//   devices.revoke, station.rename, station.acknowledgeKeyBackup,
//   station.retireToken    deviceAdminVersion 1, to a device whose hello
//                          declares deviceAuth (the `devices` object)
//   pairing.open,
//   pairing.close          pairingVersion 1, to the same peers
//   session.pathTicket     controlSwitchVersion 1 (StationServer answers it,
//                          link section 21.2; StationClient asks it only to
//                          move its session)
//   support.collect,
//   support.setLogCategories supportBundleVersion 1 (the bundle is written
//                          on a worker thread; its answer comes later)
//   slice.listen, slice.stopListening, slice.takeControl, slice.release,
//   slice.setListenLevel   sliceAccessVersion 1, to a device whose hello
//                          declares sliceAccess with sessionHolder
//                          (SliceAccessController answers them)
//
// tst_link_surface_manifest keeps this table and the routing in step: a
// source scan of dispatch() and of each prefix family's handler, and a
// dispatch of every row on a live RadioModel.
const QList<CommandVerbSpec>& SessionCommandDispatcher::verbSpecs()
{
    constexpr MirrorWireKind kInt = MirrorWireKind::Int64;
    constexpr MirrorWireKind kUtf8 = MirrorWireKind::Utf8;
    constexpr MirrorWireKind kBool = MirrorWireKind::Bool;
    constexpr MirrorWireKind kDouble = MirrorWireKind::Float64;
    const auto arg = [](const char* name, MirrorWireKind kind) {
        return CommandArgumentSpec{QByteArray(name), kind, false};
    };
    const auto optionalArg = [](const char* name, MirrorWireKind kind) {
        return CommandArgumentSpec{QByteArray(name), kind, true};
    };
    static const QList<CommandVerbSpec> specs{
        // Slices (R2 Task 11).
        {"addSlice", {arg("initialPanId", kUtf8)}, {}, 0, 0},
        {"removeSlice", {arg("sliceId", kInt)}, {}, 0, 0},
        {"requestSliceSampleRate", {arg("sliceId", kInt), arg("rateHz", kInt)}, {}, 0, 0},
        {"addSliceOnPan", {arg("panId", kUtf8)}, {}, 0, 0},
        {"setActiveSliceById", {arg("sliceId", kInt)}, {}, 0, 0},
        // A slice's band buttons (R-IOS-27, R-IOS-06): the desktop's per-pan
        // BAND grid, for a band the catalogue's `bands` lists.
        {"slice.selectBand", {arg("sliceId", kInt), arg("band", kInt)}, "bandSelectVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // C-Tune.
        {"requestStreamCtunPinned", {arg("sliceId", kInt), arg("pinned", kBool)},
         "remoteCtunVersion", 1, kRemoteCtunSessionProtocolMinor},
        {"requestStreamCentre", {arg("sliceId", kInt), arg("centreHz", kDouble)},
         "remoteCtunVersion", 1, kRemoteCtunSessionProtocolMinor},
        // 4O3A accessories.
        {"configureTgxl", {arg("host", kUtf8), arg("port", kInt)},
         "remoteTgxlConfigVersion", 1, kRemoteTgxlConfigSessionProtocolMinor},
        {"disconnectTgxl", {}, "remoteTgxlConfigVersion", 1,
         kRemoteTgxlConfigSessionProtocolMinor},
        {"setFourO3AEnabled", {arg("enabled", kBool)}, "remoteFourO3AControlVersion", 1,
         kRemoteFourO3AControlSessionProtocolMinor},
        {"configurePgxl", {arg("host", kUtf8), arg("port", kInt)},
         "remotePgxlControlVersion", 2, kRadioIdentitySessionProtocolMinor},
        {"disconnectPgxl", {}, "remotePgxlControlVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"setPgxlConnectionSettings",
         {arg("autoReconnect", kBool), arg("keepaliveSec", kInt), arg("pingSec", kInt)},
         "remotePgxlControlVersion", 2, kRadioIdentitySessionProtocolMinor},
        // The amp's and the tuner's own settings (R-R3-47, R-R3-22).
        // setPgxlHardware takes exactly one of its three arguments.
        {"setPgxlName", {arg("name", kUtf8)}, "remotePgxlControlVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        {"setPgxlHardware",
         {optionalArg("biasMode", kUtf8), optionalArg("fanMode", kUtf8),
          optionalArg("ledIntensity", kInt)},
         "remotePgxlControlVersion", 3, kRadioIdentitySessionProtocolMinor},
        {"setPgxlNetwork",
         {arg("dhcp", kBool), arg("address", kUtf8), arg("netmask", kUtf8),
          arg("gateway", kUtf8)},
         "remotePgxlControlVersion", 3, kRadioIdentitySessionProtocolMinor},
        {"savePgxlSettings", {}, "remotePgxlControlVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        {"readPgxlSettings", {}, "remotePgxlControlVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        // The Power Genius's OPERATE and STANDBY, LAN scan and saved address
        // (R-R3-49, parity Task 9).
        {"setPgxlOperate", {arg("on", kBool)}, "remotePgxlControlVersion", 4,
         kRadioIdentitySessionProtocolMinor},
        {"scanPgxlLan", {}, "remotePgxlControlVersion", 4, kRadioIdentitySessionProtocolMinor},
        {"setPgxlAddress", {arg("host", kUtf8), arg("port", kInt)},
         "remotePgxlControlVersion", 4, kRadioIdentitySessionProtocolMinor},
        {"setTgxlName", {arg("name", kUtf8)}, "remoteTgxlControlVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"setTgxlNetwork",
         {arg("dhcp", kBool), arg("address", kUtf8), arg("netmask", kUtf8),
          arg("gateway", kUtf8)},
         "remoteTgxlControlVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"saveTgxlSettings", {}, "remoteTgxlControlVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"readTgxlSettings", {}, "remoteTgxlControlVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // The Tuner Genius's antenna, operate and bypass (R-R3-49, R-R3-47).
        {"setTgxlAntenna", {arg("port", kInt)}, "remoteTgxlControlVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"setTgxlOperate", {arg("on", kBool)}, "remoteTgxlControlVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"setTgxlBypass", {arg("on", kBool)}, "remoteTgxlControlVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        // The Tuner Genius's relay nudge, LAN scan and saved address
        // (R-R3-49, parity Task 8).
        {"moveTgxlRelay", {arg("relay", kInt), arg("direction", kInt)},
         "remoteTgxlControlVersion", 4, kRadioIdentitySessionProtocolMinor},
        {"scanTgxlLan", {}, "remoteTgxlControlVersion", 4, kRadioIdentitySessionProtocolMinor},
        {"setTgxlAddress", {arg("host", kUtf8), arg("port", kInt)},
         "remoteTgxlControlVersion", 4, kRadioIdentitySessionProtocolMinor},
        // The TX applet's Tune Power slider (R-R3-49, parity Task 2).
        {"setTunePowerForTxBand", {arg("watts", kInt)}, "transmitSettingsVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        // The TX profile combos, Setup > Audio > TX Profile and the RADE
        // applet's Reset vocoder (R-R3-49, parity Task 3).
        {"txProfile.select", {arg("name", kUtf8)}, "transmitSettingsVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        {"txProfile.save", {arg("name", kUtf8)}, "transmitSettingsVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        {"txProfile.delete", {arg("name", kUtf8)}, "transmitSettingsVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        {"rade.resetVocoder", {}, "transmitSettingsVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        // The TX EQ panel's curve from an app (R-IOS-13, R-R3-49,
        // txEqCurveVersion 2).
        {"txEq.setCurve", {arg("curveJson", kUtf8)}, "txEqCurveVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"txEq.resetCurve", {}, "txEqCurveVersion", 2, kRadioIdentitySessionProtocolMinor},
        // The CFC dialog's band editor from an app, applied at once against
        // the revision it last saw (transmitSettingsVersion 15).
        {"cfc.setProfile", {arg("profileJson", kUtf8), arg("expectedRevision", kUtf8)},
         "transmitSettingsVersion", 15, kRadioIdentitySessionProtocolMinor},
        // Setup > PA > PA Gain's profiles and table, as the local page
        // changes them (R-R3-49, R-IOS-18; paProfileVersion 1).
        {"paProfile.select", {arg("name", kUtf8)}, "paProfileVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"paProfile.new", {arg("name", kUtf8)}, "paProfileVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"paProfile.copy", {arg("name", kUtf8)}, "paProfileVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"paProfile.delete", {arg("name", kUtf8)}, "paProfileVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"paProfile.reset", {}, "paProfileVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"paProfile.setGain", {arg("band", kInt), arg("value", kDouble)}, "paProfileVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"paProfile.setAdjust", {arg("band", kInt), arg("step", kInt), arg("value", kDouble)},
         "paProfileVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"paProfile.setMaxPower", {arg("band", kInt), arg("value", kDouble)},
         "paProfileVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"paProfile.setUseMax", {arg("band", kInt), arg("on", kBool)}, "paProfileVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // The Core's RF-Kit RF2K-S and the station TCI server (R-R3-47,
        // R-R3-48).
        {"configureRfKit", {arg("host", kUtf8), arg("port", kInt)},
         "remoteRfKitControlVersion", 2, kRadioIdentitySessionProtocolMinor},
        {"disconnectRfKit", {}, "remoteRfKitControlVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"setRfKitEnabled", {arg("enabled", kBool)}, "remoteRfKitControlVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"resetRfKitError", {}, "remoteRfKitControlVersion", 3,
         kRadioIdentitySessionProtocolMinor},
        // The RF-Kit's OPERATE and STANDBY, antenna, TCI mode and saved
        // address (R-R3-49, parity Task 10).
        {"setRfKitOperate", {arg("on", kBool)}, "remoteRfKitControlVersion", 4,
         kRadioIdentitySessionProtocolMinor},
        {"setRfKitAntenna", {arg("port", kInt)}, "remoteRfKitControlVersion", 4,
         kRadioIdentitySessionProtocolMinor},
        {"setRfKitTciMode", {}, "remoteRfKitControlVersion", 4,
         kRadioIdentitySessionProtocolMinor},
        {"setRfKitAddress", {arg("host", kUtf8), arg("port", kInt)},
         "remoteRfKitControlVersion", 4, kRadioIdentitySessionProtocolMinor},
        // Task 42: station-owned transmit-coupled accessory actions.
        {"amp.operate", {}, "accessoryTxVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"amp.standby", {}, "accessoryTxVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"tuner.tune", {}, "accessoryTxVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"tuner.operate", {arg("on", kBool)}, "accessoryTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"tuner.bypass", {arg("on", kBool)}, "accessoryTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"tuner.antenna", {arg("port", kInt)}, "accessoryTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"rfkit.operate", {}, "accessoryTxVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"rfkit.standby", {}, "accessoryTxVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"rfkit.antenna", {arg("port", kInt)}, "accessoryTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"setStationTci", {arg("enabled", kBool), arg("port", kInt)}, "stationTciVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // The Core's station TCI server's options and apps (R-R3-48,
        // R-R3-42, parity Task 23).
        {"setStationTciOptions",
         {arg("emulateExpertSdr3", kBool), arg("emulateSunSdr2Pro", kBool),
          arg("cwluBecomesCw", kBool), arg("sendInitialState", kBool)},
         "stationTciVersion", 2, kRadioIdentitySessionProtocolMinor},
        {"disconnectStationTciClient", {arg("id", kUtf8)}, "stationTciVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        // JJ's ruling of 2026-09-28: the rest of the Core's TCI server
        // settings, any of them at once (StationTciModel::settingsTable()).
        {"setStationTciSettings",
         {optionalArg("rateLimitMs", kInt), optionalArg("cwBecomesCwuAbove10mhz", kBool),
          optionalArg("iqSwap", kBool), optionalArg("alwaysStreamIq", kBool),
          optionalArg("audioBlockSamples", kInt), optionalArg("txChannel", kInt),
          optionalArg("rxSensorIntervalMs", kInt), optionalArg("txSensorIntervalMs", kInt),
          optionalArg("forgetRx2VfoBOnDisconnect", kBool),
          optionalArg("useRx1VfoaForRx2Vfoa", kBool), optionalArg("copyRx2VfobToVfoa", kBool)},
         "stationTciSettingsVersion", 1, kRadioIdentitySessionProtocolMinor},
        // The Core's accessory records and settings (R-R3-47, R-R3-22).
        {"setTxInterlockPolicy",
         {arg("mode", kInt), arg("graceMs", kInt), arg("swrGateEnabled", kBool),
          arg("swrGateMax", kDouble)},
         "accessoryDataVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"setPgxlPowerCap", {arg("enabled", kBool), arg("watts", kInt)},
         "accessoryDataVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"clearAccessoryFaults", {arg("device", kUtf8)}, "accessoryDataVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // The Core's radio hardware (R-R3-46).
        {"requestIoBoardProbe", {}, "radioHardwareVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"setAlexRxAntenna", {arg("band", kInt), arg("antenna", kInt), arg("rxOnly", kBool)},
         "radioHardwareVersion", 3, kRadioIdentitySessionProtocolMinor},
        {"setAlexRxAntennaForRadio",
         {arg("mac", kUtf8), arg("band", kInt), arg("antenna", kInt), arg("rxOnly", kBool)},
         "radioAntennaRowsVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"setAlexBpfMode", {arg("chain", kInt), arg("mode", kInt)}, "radioHardwareVersion", 4,
         kRadioIdentitySessionProtocolMinor},
        // iPhone app plan Task 34 (R-IOS-02, ruling 8.10): move the TX flag
        // to one of the holder's slices, by its id (never a list position).
        {"tx.setTxSlice", {arg("sliceId", kInt)}, "remoteTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // iPhone app plan Task 35 (R-IOS-13): keying from a device. Each is
        // sent three times as the same command; the Core acts once.
        {"tx.setMicSource", {arg("source", kUtf8)}, "radioMicVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"tx.key", {arg("trigger", kUtf8)}, "remoteTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"tx.unkey", {arg("epoch", kInt)}, "remoteTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"tx.tune", {arg("on", kBool)}, "remoteTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"tx.twoTone", {arg("on", kBool)}, "remoteTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // Setup > Test's two Thetis frequency presets change both mirrored
        // frequencies as one request, so a remote renderer needs no local
        // assumptions about either preset.
        {"tx.twoTonePreset", {arg("name", kUtf8)}, "setupDescriptionVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // iPhone app plan Task 37 (R-IOS-13): the transmit watchdog's
        // keepalive, every 100 ms while the device is keyed or has VOX
        // armed. Sent once each (a lost one is overtaken by the next).
        {"tx.keepalive", {arg("sequence", kInt), arg("epoch", kInt)}, "remoteTxVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // iPhone app plan Task 77 (R-IOS-02, R-IOS-03; rulings 8.4, 8.6,
        // 8.7): take transmit without keying; and the Tuner Genius
        // autotune from a device, a key under the holder rules.
        {"tx.take",
         {{"holderEpoch", MirrorWireKind::Int64, true}, {"shownKeyed", MirrorWireKind::Bool, true}},
         "remoteTxVersion", 2, kRadioIdentitySessionProtocolMinor},
        {"tx.tunerTune", {arg("on", kBool)}, "remoteTxVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        {"setAlexTxAntenna", {arg("band", kInt), arg("antenna", kInt)},
         "radioHardwareVersion", 6, kRadioIdentitySessionProtocolMinor},
        {"setAlexTxAntennaForRadio", {arg("mac", kUtf8), arg("band", kInt), arg("antenna", kInt)},
         "radioAntennaRowsVersion", 1, kRadioIdentitySessionProtocolMinor},
        // HL2 Options' I2C tool and Pin Control (R-R3-46, parity Task 14).
        {"requestIoBoardI2c",
         {arg("bus", kInt), arg("address", kInt), arg("register", kInt), arg("write", kBool),
          arg("value", kInt)},
         "radioHardwareVersion", 7, kRadioIdentitySessionProtocolMinor},
        {"setIoBoardOutput", {arg("pin", kInt), arg("on", kBool)}, "radioHardwareVersion", 7,
         kRadioIdentitySessionProtocolMinor},
        // Parity ruling C4: the radio's sample rate, every receiver and the
        // radio's own rate, as a local window's Radio Info change.
        {"setRadioSampleRate", {arg("rateHz", kInt)}, "radioHardwareVersion", 9,
         kRadioIdentitySessionProtocolMinor},
        // Level Cal: Setup's Reset, the meter and display calibration back
        // to the radio's defaults.
        {"resetLevelCalibration", {}, "radioHardwareVersion", 12,
         kRadioIdentitySessionProtocolMinor},
        // Level Cal: the Core's calibration run on one slice, and its stop.
        {"startLevelCalibration",
         {arg("levelDbm", kDouble), arg("frequencyHz", kDouble), arg("sliceId", kInt)},
         "radioHardwareVersion", 12, kRadioIdentitySessionProtocolMinor},
        {"cancelLevelCalibration", {}, "radioHardwareVersion", 12,
         kRadioIdentitySessionProtocolMinor},
        // The filter graph's curve (R-R3-49, parity Task 16).
        {"dsp.filterResponse", {arg("sliceId", kInt), arg("highResolution", kBool)},
         "dspInfoVersion", 1, kRadioIdentitySessionProtocolMinor},
        // Record streams and the Core's spot sources (R-IOS-25, R-R3-49,
        // parity Task 19).
        {"records.subscribe", {arg("stream", kUtf8), arg("backlog", kInt)},
         "recordStreamVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"records.unsubscribe", {arg("stream", kUtf8)}, "recordStreamVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"spots.connect", {arg("source", kUtf8)}, "recordStreamVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"spots.disconnect", {arg("source", kUtf8)}, "recordStreamVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"spots.sendCommand", {arg("source", kUtf8), arg("text", kUtf8)},
         "recordStreamVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"spots.clearAll", {}, "recordStreamVersion", 1, kRadioIdentitySessionProtocolMinor},
        // The AM Mod Monitor's RESET (R-IOS-13, R-R3-49): the Core's
        // analyzer for a source, 0 TX I/Q or 1 PA feedback.
        {"txModMonitor.reset", {arg("source", kInt)}, "txModMonitorVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // The Core's radio (R-IOS-18, R-R3-49, parity Task 21).
        {"station.selectRadio", {arg("mac", kUtf8)}, "stationRadiosVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"station.rescanRadios", {}, "stationRadiosVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"station.setRadioModel", {arg("mac", kUtf8), arg("model", kInt)},
         "stationRadiosVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"station.forgetRadio", {arg("mac", kUtf8)}, "stationRadiosVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"station.validateSettings", {arg("mac", kUtf8)}, "settingsHygieneVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"station.forgetSettings", {arg("mac", kUtf8)}, "settingsHygieneVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // G-38: Diagnostics' Repair invalid settings from a remote window.
        {"station.repairSettings", {arg("mac", kUtf8)}, "settingsHygieneVersion", 2,
         kRadioIdentitySessionProtocolMinor},
        // The Core's FreeDV Reporter (R-IOS-26, R-R3-49, iPhone plan Task
        // 22, parity Task 20).
        {"freedv.setMessage", {arg("text", kUtf8)}, "stationFreedvVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"freedv.sendQsy", {arg("callsign", kUtf8), arg("frequencyHz", kInt)},
         "stationFreedvVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"freedv.setHidden", {arg("on", kBool)}, "stationFreedvVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // The Core's support bundle and logging categories (R-R3-49,
        // R-IOS-18, parity Task 22, iPhone plan Task 25).
        {"support.collect", {}, "supportBundleVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"support.setLogCategories", {arg("categories", kUtf8)}, "supportBundleVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // Neural noise reduction.
        {"nnr.setDiagnostics",
         {arg("sliceId", kInt), arg("testMode", kInt), arg("outputMode", kInt)},
         "nnrVersion", 1, kDspControlSessionProtocolMinor},
        {"nnr.resetTuning", {arg("sliceId", kInt)}, "nnrVersion", 1,
         kDspControlSessionProtocolMinor},
        {"nnr.tryAgain", {arg("sliceId", kInt)}, "nnrVersion", 1,
         kNnrLimitSessionProtocolMinor},
        {"nnr.applyModelSelection", {arg("revision", kInt)}, "dspAssetVersion", 1,
         kDspControlSessionProtocolMinor},
        // DSP assets (DspAssetService::execute).
        {"dspAssets.list", {}, "dspAssetVersion", 1, kDspControlSessionProtocolMinor},
        {"dspAssets.beginImport",
         {arg("kind", kInt), arg("label", kUtf8), arg("size", kInt), arg("hash", kUtf8),
          arg("radioIdentity", kUtf8)},
         "dspAssetVersion", 1, kDspControlSessionProtocolMinor},
        {"dspAssets.chunk",
         {arg("transferId", kUtf8), arg("offset", kInt), arg("data", kUtf8)},
         "dspAssetVersion", 1, kDspControlSessionProtocolMinor},
        {"dspAssets.finishImport", {arg("transferId", kUtf8)}, "dspAssetVersion", 1,
         kDspControlSessionProtocolMinor},
        {"dspAssets.cancelImport", {arg("transferId", kUtf8)}, "dspAssetVersion", 1,
         kDspControlSessionProtocolMinor},
        {"dspAssets.export", {arg("id", kUtf8), arg("offset", kInt)}, "dspAssetVersion", 1,
         kDspControlSessionProtocolMinor},
        {"dspAssets.selectNnrModel", {arg("slot", kInt), arg("id", kUtf8)},
         "dspAssetVersion", 1, kDspControlSessionProtocolMinor},
        {"dspAssets.selectNr3Model", {arg("id", kUtf8)}, "dspAssetVersion", 2,
         kDspControlSessionProtocolMinor},
        // PureSignal (PureSignalSessionFacade::actionVerb and the display
        // subscription handled here).
        {"ps3.subscribeDisplay", {arg("enabled", kBool)}, "psDisplayVersion", 1,
         kMediaSessionProtocolMinor},
        {"ps3.off", {}, "psAlgorithmVersion", 3, kDspControlSessionProtocolMinor},
        {"ps3.single", {}, "psAlgorithmVersion", 3, kDspControlSessionProtocolMinor},
        {"ps3.automatic", {}, "psAlgorithmVersion", 3, kDspControlSessionProtocolMinor},
        {"ps3.applyCurrent", {}, "psAlgorithmVersion", 3, kDspControlSessionProtocolMinor},
        {"ps3.twoTone", {arg("enabled", kBool)}, "psAlgorithmVersion", 3,
         kDspControlSessionProtocolMinor},
        {"ps3.saveCorrection", {arg("label", kUtf8)}, "psAlgorithmVersion", 3,
         kDspControlSessionProtocolMinor},
        {"ps3.restoreCorrection", {arg("assetId", kUtf8)}, "psAlgorithmVersion", 3,
         kDspControlSessionProtocolMinor},
        // Notches (R-R3-21 / R-R3-09).
        {"notch.add", {arg("sliceId", kInt), arg("centreHz", kDouble), arg("widthHz", kDouble)},
         "notchControlVersion", 1, kDspControlSessionProtocolMinor},
        {"notch.move", {arg("id", kInt), arg("centreHz", kDouble), arg("widthHz", kDouble)},
         "notchControlVersion", 1, kDspControlSessionProtocolMinor},
        {"notch.setActive", {arg("id", kInt), arg("active", kBool)}, "notchControlVersion", 1,
         kDspControlSessionProtocolMinor},
        {"notch.delete", {arg("id", kInt)}, "notchControlVersion", 1,
         kDspControlSessionProtocolMinor},
        // R-IOS-27, R-IOS-06: the desktop's +TNF on a slice.
        {"notch.addAtSlice", {arg("sliceId", kInt)}, "notchControlVersion", 2,
         kDspControlSessionProtocolMinor},
        // The Core's paired devices, its name, its key backup and its old
        // pairing token (iPhone app Task 13, R-IOS-08).
        {"devices.revoke", {arg("id", kUtf8)}, "deviceAdminVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"station.rename", {arg("label", kUtf8)}, "deviceAdminVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"station.acknowledgeKeyBackup", {}, "deviceAdminVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"station.retireToken", {}, "deviceAdminVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // The Core's pairing window (iPhone app Task 14, R-IOS-08).
        {"pairing.open", {}, "pairingVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"pairing.close", {}, "pairingVersion", 1, kRadioIdentitySessionProtocolMinor},
        // Leaving the Core on purpose (iPhone app Task 71, R-IOS-02).
        {"session.leave", {}, "sessionHolderVersion", 1, kRadioIdentitySessionProtocolMinor},
        // Answering the Core's questions and taking back (iPhone app Task
        // 74, R-IOS-30; the several-devices design, section 10.4).
        {"confirm.proceed",
         {{"id", MirrorWireKind::Int64, false}, {"choice", MirrorWireKind::Int64, false}},
         "sessionHolderVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"confirm.cancel", {{"id", MirrorWireKind::Int64, false}}, "sessionHolderVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        {"notice.takeBack", {{"id", MirrorWireKind::Int64, false}}, "sessionHolderVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // Moving a session to another connection (iPhone app plan Task 29,
        // R-IOS-16; the link document, section 21.2). StationServer answers
        // it itself; the dispatcher never routes it.
        {"session.pathTicket", {}, "controlSwitchVersion", 1,
         kRadioIdentitySessionProtocolMinor},
        // Slice control plan Task 4: listening to another device's slice
        // and taking or releasing control of one, each naming the slice by
        // its id and incarnation (`access:<id>`), take and release with the
        // control revision the device saw.
        {"slice.listen", {arg("sliceId", kInt), arg("incarnation", kInt)}, "sliceAccessVersion",
         1, kRadioIdentitySessionProtocolMinor},
        {"slice.stopListening", {arg("sliceId", kInt), arg("incarnation", kInt)},
         "sliceAccessVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"slice.takeControl",
         {arg("sliceId", kInt), arg("incarnation", kInt), arg("controlRevision", kInt)},
         "sliceAccessVersion", 1, kRadioIdentitySessionProtocolMinor},
        {"slice.release",
         {arg("sliceId", kInt), arg("incarnation", kInt), arg("controlRevision", kInt)},
         "sliceAccessVersion", 1, kRadioIdentitySessionProtocolMinor},
        // Slice control plan Task 6: a listener's own level (0..1) and mute
        // for a slice it hears; the controller's AF is not touched.
        {"slice.setListenLevel",
         {arg("sliceId", kInt), arg("incarnation", kInt), arg("level", kDouble),
          arg("muted", kBool)},
         "sliceAccessVersion", 1, kRadioIdentitySessionProtocolMinor},
    };
    return specs;
}

QString SessionCommandDispatcher::radioAntennaRowRefusal(const SessionMessage& invoke,
                                                          const RadioModel* radioModel)
{
    const bool rx = invoke.commandVerb == "setAlexRxAntennaForRadio";
    const bool tx = invoke.commandVerb == "setAlexTxAntennaForRadio";
    if (!rx && !tx) {
        return {};
    }

    QString mac;
    int band = 0;
    int antenna = 0;
    QVariant rxOnly;
    const auto exactInt64 = [&invoke](const QByteArray& name, int* value) {
        for (const MirrorUpdate& argument : invoke.arguments) {
            if (argument.name != name) {
                continue;
            }
            return argument.kind == MirrorWireKind::Int64
                && argument.value.typeId() == QMetaType::LongLong
                && findIntArgument(invoke.arguments, name, value) == ArgumentStatus::Ok;
        }
        return false;
    };
    if (!hasExactlyArguments(invoke.arguments, rx
            ? std::initializer_list<QByteArray>{"mac", "band", "antenna", "rxOnly"}
            : std::initializer_list<QByteArray>{"mac", "band", "antenna"})
        || !findUtf8Argument(invoke.arguments, "mac", &mac)
        || !exactInt64("band", &band) || !exactInt64("antenna", &antenna)
        || (rx && (!hasWireKind(invoke.arguments, "rxOnly", MirrorWireKind::Bool)
                   || !findArgument(invoke.arguments, "rxOnly", &rxOnly)
                   || rxOnly.typeId() != QMetaType::Bool))) {
        return QStringLiteral("The Core could not read this request.");
    }
    // A band number: 160m .. XVTR (0-13) or 2 m (27, R-IOS-26).
    if (band < 0 || band >= static_cast<int>(Band::Count)
        || !hasPerBandState(static_cast<Band>(band))) {
        return QStringLiteral("The Core keeps antennas for 160 m to 6 m, 2 m, GEN, WWV and XVTR.");
    }
    // The shared-setting classifier reads this row before the facade's
    // setter. Reject an unusable port here so no other device is asked to
    // confirm a change the existing one-band setter would refuse.
    if (antenna < (rx && rxOnly.toBool() ? 0 : 1) || antenna > 3) {
        return rx && rxOnly.toBool()
            ? QStringLiteral("The receive-only input is none or 1 to 3.")
            : QStringLiteral("Antennas are numbered 1 to 3.");
    }
    if (radioModel == nullptr || radioModel->thread() != QThread::currentThread()
        || radioModel->role() != RadioModel::Role::Local
        || !radioModel->boardCapabilities().hasAlexFilters
        || radioModel->alexAntennaFacade() == nullptr
        || !radioModel->alexAntennaFacade()->isBound()) {
        return QStringLiteral("The Core has no antenna settings ready.");
    }
    const QString current = radioModel->currentRadioMac();
    if (mac.isEmpty() || AppSettings::normalizedRadioMac(mac) != mac
        || current.isEmpty() || AppSettings::normalizedRadioMac(current) != current
        || mac != current) {
        return QStringLiteral("This antenna change is for another radio or a disconnected radio.");
    }
    return {};
}

SessionCommandDispatcher::SessionCommandDispatcher(RadioModel* radioModel, QObject* parent)
    : QObject(parent)
    , m_radioModel(radioModel)
{
    if (radioModel) {
        connect(radioModel->pureSignalFacade(), &PureSignalSessionFacade::actionResult,
                this, [this](quint32 id, Ps3ActionPhase phase, const QString& reason,
                             QVariantMap values) {
            const auto found = m_pureSignalCommands.constFind(id);
            if (found == m_pureSignalCommands.cend()) {
                return;
            }
            const PendingPureSignalCommand command = *found;
            QString state;
            switch (phase) {
            case Ps3ActionPhase::Accepted: state = "accepted"; break;
            case Ps3ActionPhase::Pending: state = "pending"; break;
            case Ps3ActionPhase::Completed: state = "completed"; break;
            case Ps3ActionPhase::Failed: state = "failed"; break;
            }
            if (phase == Ps3ActionPhase::Completed || phase == Ps3ActionPhase::Failed) {
                m_pureSignalCommands.remove(id);
            }
            values.insert("phase", state);
            emitResultAs(command.owner, SessionMessages::commandResult(command.verb, command.commandId,
                phase != Ps3ActionPhase::Failed, reason, {"pureSignal"},
                dspCommandValues(values).value_or(QList<MirrorUpdate>{})));
        });
    }
}

void SessionCommandDispatcher::setSessionOwner(const QString& owner)
{
    // iPhone app Task 72: before, this also cancelled the previous owner's
    // jobs and reset the session state, because there was one session. The
    // owner now changes with every dispatch; those two are their own calls.
    m_sessionOwner = owner;
}

SessionCommandDispatcher::DispatchContext
SessionCommandDispatcher::exchangeDispatchContext(DispatchContext context)
{
    DispatchContext previous{m_sessionOwner, m_requester, m_requesterSharesSlices,
                             m_pureSignalArmingOffered, m_transmitSettingsOnAir, m_resultOwner};
    setSessionOwner(context.owner);
    setRequester(context.requester);
    setRequesterSharesSlices(context.requesterSharesSlices);
    setPureSignalArmingOffered(context.pureSignalArmingOffered);
    setTransmitSettingsOnAir(context.transmitSettingsOnAir);
    m_resultOwner = std::move(context.resultOwner);
    return previous;
}

void SessionCommandDispatcher::endSessionOwner(const QString& owner)
{
    if (m_radioModel && !owner.isEmpty()) {
        m_radioModel->dspAssets()->cancelOwner(owner);
    }
}

void SessionCommandDispatcher::resetSessionState()
{
    m_pureSignalCommands.clear();
    if (m_radioModel) {
        for (SliceModel* slice : m_radioModel->slices()) {
            // Diagnostic modes are operator actions. A new session always
            // starts on normal audio and never replays a prior test signal.
            m_radioModel->setNnrDiagnosticMode(slice->sliceIndex(), 0, 1);
        }
    }
    // R-R3-49 (parity Task 7): the arming offer is set before each dispatch
    // (setPureSignalArmingOffered); a Core with no session offers none. A
    // Tuner Genius or Power Genius scan still due is dropped.
    m_pureSignalArmingOffered = false;
    m_transmitSettingsOnAir = false;
    ++m_sessionGeneration;
}

void SessionCommandDispatcher::dispatch(const SessionMessage& invoke)
{
    if (invoke.kind != SessionMessageKind::CommandInvoke) {
        // Not this class's concern -- a caller routing error, not a
        // command failure worth reporting back.
        return;
    }
    // iPhone app Task 13: the Core's device administration needs no radio.
    if (invoke.commandVerb == "devices.revoke" || invoke.commandVerb == "station.rename"
        || invoke.commandVerb == "station.acknowledgeKeyBackup"
        || invoke.commandVerb == "station.retireToken") {
        handleDeviceAdmin(invoke);
        return;
    }
    // iPhone app Task 14: the pairing window needs no radio either.
    if (invoke.commandVerb == "pairing.open" || invoke.commandVerb == "pairing.close") {
        handlePairingWindow(invoke);
        return;
    }
    // iPhone app Task 71: nor does leaving the Core.
    if (invoke.commandVerb == "session.leave") {
        handleSessionLeave(invoke);
        return;
    }
    // iPhone app plan Task 29: StationServer answers session.pathTicket for
    // a signed-in session before the dispatcher sees it (link section
    // 21.2). Reaching here, there is no session whose connection could
    // move.
    if (invoke.commandVerb == "session.pathTicket") {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not move this connection."), {});
        return;
    }
    // iPhone app Task 74: an answer to the Core's question, or Take it
    // back; the Core's confirm step decides.
    if (invoke.commandVerb == "confirm.proceed" || invoke.commandVerb == "confirm.cancel"
        || invoke.commandVerb == "notice.takeBack") {
        handleConfirmAnswer(invoke);
        return;
    }
    if (m_radioModel.isNull()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core has no radio ready."), {});
        return;
    }

    if (invoke.commandVerb.startsWith("ps3.")) {
        // Task 34 (ruling 7.4): PureSignal waits while the holder is on the
        // air, two-tone and the display subscription aside.
        if (refusedWhileOnAir(invoke)) {
            return;
        }
        handlePureSignalAction(invoke);
        return;
    }

    if (invoke.commandVerb.startsWith("dspAssets.")) {
        const auto arguments = dspCommandValues(invoke.arguments);
        const auto result = arguments && !m_sessionOwner.isEmpty()
            ? m_radioModel->dspAssets()->execute(invoke.commandVerb, *arguments, m_sessionOwner)
            : DspAssetServiceResult{false, QStringLiteral("The Core could not read this request."), {}};
        const auto values = dspCommandValues(result.values);
        emit commandResultReady(SessionMessages::commandResult(invoke.commandVerb,
            invoke.commandId, result.accepted, result.reason, {}, values.value_or(QList<MirrorUpdate>{})));
        return;
    }
    if (invoke.commandVerb == "nnr.applyModelSelection") {
        quint32 revision = 0;
        QString reason;
        const bool shape = hasExactlyArguments(invoke.arguments, {"revision"});
        const QVariant value = shape ? invoke.arguments.first().value : QVariant();
        const bool integer = value.typeId() == QMetaType::Int || value.typeId() == QMetaType::UInt
            || value.typeId() == QMetaType::LongLong || value.typeId() == QMetaType::ULongLong;
        bool converted = false;
        const qlonglong wide = value.toLongLong(&converted);
        const bool valid = shape && integer && converted
            && invoke.arguments.first().kind == MirrorWireKind::Int64
            && wide > 0 && wide <= std::numeric_limits<quint32>::max();
        if (valid) {
            revision = static_cast<quint32>(wide);
        }
        const bool accepted = valid && m_radioModel->applyNnrModelSelection(revision, &reason);
        if (!valid) {
            reason = QStringLiteral("The model choice changed on the Core before this request arrived. Try again.");
        }
        emitResult(invoke.commandVerb, invoke.commandId, accepted, reason, {"dspAssets"});
        return;
    }

    // Slice control plan Task 4: listening and control, answered by the
    // Core's SliceAccessController, which checks the slice itself.
    if (invoke.commandVerb == "slice.listen" || invoke.commandVerb == "slice.stopListening"
        || invoke.commandVerb == "slice.takeControl" || invoke.commandVerb == "slice.release") {
        handleSliceAccessVerb(invoke);
        return;
    }
    if (invoke.commandVerb == "slice.setListenLevel") {
        handleSliceListenLevel(invoke);
        return;
    }
    // Slice control plan Task 4: a device that shares slices may make any
    // slice it listens to its active receive slice
    // (RadioModel::setActiveRxFor); an older window keeps today's rule.
    if (invoke.commandVerb == "setActiveSliceById" && m_requesterSharesSlices
        && !m_requester.isEmpty() && !m_sliceAccessController.isNull()) {
        int sliceId = -1;
        if (findIntArgument(invoke.arguments, "sliceId", &sliceId) == ArgumentStatus::Ok) {
            const int previous = m_radioModel->sliceOwnership()->activeRxFor(m_requester);
            const SliceAccessController::Result result =
                m_sliceAccessController->selectRx(m_requester, sliceId);
            QList<QByteArray> affected = result.affected;
            if (result.accepted && previous >= 0 && previous != sliceId) {
                affected.append(ObjectRegistry::keyForSlice(previous));
            }
            emitResult(invoke.commandVerb, invoke.commandId, result.accepted, result.reason,
                       result.accepted ? affected : QList<QByteArray>{});
            return;
        }
    }

    // iPhone app Task 73 (ruling 5.9): a device addresses only its own
    // slices. Refused before anything is looked at or changed.
    if (refusedForAnotherDevice(invoke)) {
        return;
    }
    // iPhone app plan Task 34 (ruling 7.4, D60): a change to the transmit
    // path, a Protocol 1 rate change or a move of the holder's transmit
    // slice waits while another device's holder is on the air.
    if (refusedWhileOnAir(invoke)) {
        return;
    }
    if (invoke.commandVerb == "tx.twoTonePreset") {
        QString onAir;
        // Version 13: taken on the air from a peer that may change the
        // transmit settings, as the local Two-Tone page's presets are.
        if (!m_transmitSettingsOnAir && m_radioModel->stationOnAirRefusal(&onAir)) {
            emitResult(invoke.commandVerb, invoke.commandId, false, onAir, {});
            return;
        }
        if (m_transmitAccess.transmitter) {
            if (const TxRefusal refusal = m_transmitAccess.transmitter(m_requester);
                !refusal.isEmpty()) {
                emitRefusal(invoke.commandVerb, invoke.commandId, refusal);
                return;
            }
        }
        QString name;
        if (!hasExactlyArguments(invoke.arguments, {"name"})
            || !findUtf8Argument(invoke.arguments, "name", &name)
            || (name != QLatin1String("defaults") && name != QLatin1String("stealth"))) {
            emitResult(invoke.commandVerb, invoke.commandId, false,
                       QStringLiteral("Choose a two-tone frequency preset."), {});
            return;
        }
        auto& transmit = m_radioModel->transmitModel();
        transmit.setTwoToneFrequencies(name == QLatin1String("defaults") ? 700 : 70,
                                       name == QLatin1String("defaults") ? 1900 : 190);
        emitResult(invoke.commandVerb, invoke.commandId, true, {}, {"transmit"});
        return;
    }
    if (invoke.commandVerb == "tx.setTxSlice") {
        handleSetTxSlice(invoke);
        return;
    }
    if (invoke.commandVerb == "amp.operate" || invoke.commandVerb == "amp.standby"
        || invoke.commandVerb == "tuner.tune" || invoke.commandVerb == "tuner.operate"
        || invoke.commandVerb == "tuner.bypass" || invoke.commandVerb == "tuner.antenna"
        || invoke.commandVerb == "rfkit.operate" || invoke.commandVerb == "rfkit.standby"
        || invoke.commandVerb == "rfkit.antenna") {
        handleAccessoryTx(invoke);
        return;
    }
    if (invoke.commandVerb == "tx.setMicSource") {
        handleTxMicSource(invoke);
        return;
    }
    if (invoke.commandVerb == "tx.key" || invoke.commandVerb == "tx.unkey"
        || invoke.commandVerb == "tx.tune" || invoke.commandVerb == "tx.twoTone"
        || invoke.commandVerb == "tx.tunerTune") {
        handleTxKeying(invoke);
        return;
    }
    if (invoke.commandVerb == "tx.keepalive") {
        handleTxKeepalive(invoke);
        return;
    }
    if (invoke.commandVerb == "tx.take") {
        handleTxTake(invoke);
        return;
    }

    if (invoke.commandVerb.startsWith("notch.")) {
        handleNotchAction(invoke);
        return;
    }

    if (invoke.commandVerb == "addSlice") {
        handleAddSlice(invoke);
    } else if (invoke.commandVerb == "removeSlice") {
        handleRemoveSlice(invoke);
    } else if (invoke.commandVerb == "requestSliceSampleRate") {
        handleRequestSliceSampleRate(invoke);
    } else if (invoke.commandVerb == "addSliceOnPan") {
        handleAddSliceOnPan(invoke);
    } else if (invoke.commandVerb == "setActiveSliceById") {
        handleSetActiveSliceById(invoke);
    } else if (invoke.commandVerb == "slice.selectBand") {
        handleSelectBand(invoke);
    } else if (invoke.commandVerb == "requestStreamCtunPinned") {
        handleRequestStreamCtunPinned(invoke);
    } else if (invoke.commandVerb == "requestStreamCentre") {
        handleRequestStreamCentre(invoke);
    } else if (invoke.commandVerb == "configureTgxl") {
        handleConfigureTgxl(invoke);
    } else if (invoke.commandVerb == "disconnectTgxl") {
        handleDisconnectTgxl(invoke);
    } else if (invoke.commandVerb == "setFourO3AEnabled") {
        handleSetFourO3AEnabled(invoke);
    } else if (invoke.commandVerb == "configurePgxl") {
        handleConfigurePgxl(invoke);
    } else if (invoke.commandVerb == "disconnectPgxl") {
        handleDisconnectPgxl(invoke);
    } else if (invoke.commandVerb == "setPgxlConnectionSettings") {
        handleSetPgxlConnectionSettings(invoke);
    } else if (invoke.commandVerb == "configureRfKit") {
        handleConfigureRfKit(invoke);
    } else if (invoke.commandVerb == "disconnectRfKit") {
        handleDisconnectRfKit(invoke);
    } else if (invoke.commandVerb == "setRfKitEnabled") {
        handleSetRfKitEnabled(invoke);
    } else if (invoke.commandVerb == "resetRfKitError") {
        handleResetRfKitError(invoke);
    } else if (invoke.commandVerb == "setRfKitOperate") {
        handleSetRfKitOperate(invoke);
    } else if (invoke.commandVerb == "setRfKitAntenna") {
        handleSetRfKitAntenna(invoke);
    } else if (invoke.commandVerb == "setRfKitTciMode") {
        handleSetRfKitTciMode(invoke);
    } else if (invoke.commandVerb == "setRfKitAddress") {
        handleSetRfKitAddress(invoke);
    } else if (invoke.commandVerb == "setStationTci") {
        handleSetStationTci(invoke);
    } else if (invoke.commandVerb == "setStationTciOptions"
               || invoke.commandVerb == "disconnectStationTciClient") {
        handleStationTciServer(invoke);
    } else if (invoke.commandVerb == "setStationTciSettings") {
        handleSetStationTciSettings(invoke);
    } else if (invoke.commandVerb == "setTxInterlockPolicy") {
        handleSetTxInterlockPolicy(invoke);
    } else if (invoke.commandVerb == "setPgxlPowerCap") {
        handleSetPgxlPowerCap(invoke);
    } else if (invoke.commandVerb == "clearAccessoryFaults") {
        handleClearAccessoryFaults(invoke);
    } else if (invoke.commandVerb == "setPgxlName" || invoke.commandVerb == "setPgxlHardware"
               || invoke.commandVerb == "setPgxlNetwork"
               || invoke.commandVerb == "savePgxlSettings"
               || invoke.commandVerb == "readPgxlSettings"
               || invoke.commandVerb == "setTgxlName" || invoke.commandVerb == "setTgxlNetwork"
               || invoke.commandVerb == "saveTgxlSettings"
               || invoke.commandVerb == "readTgxlSettings") {
        handleAccessoryDeviceSettings(invoke);
    } else if (invoke.commandVerb == "setTgxlAntenna" || invoke.commandVerb == "setTgxlOperate"
               || invoke.commandVerb == "setTgxlBypass") {
        handleTgxlControl(invoke);
    } else if (invoke.commandVerb == "moveTgxlRelay") {
        handleMoveTgxlRelay(invoke);
    } else if (invoke.commandVerb == "scanTgxlLan") {
        handleScanTgxlLan(invoke);
    } else if (invoke.commandVerb == "setTgxlAddress") {
        handleSetTgxlAddress(invoke);
    } else if (invoke.commandVerb == "setPgxlOperate") {
        handleSetPgxlOperate(invoke);
    } else if (invoke.commandVerb == "scanPgxlLan") {
        handleScanPgxlLan(invoke);
    } else if (invoke.commandVerb == "setPgxlAddress") {
        handleSetPgxlAddress(invoke);
    } else if (invoke.commandVerb == "setTunePowerForTxBand") {
        handleTunePowerForTxBand(invoke);
    } else if (invoke.commandVerb == "txProfile.select" || invoke.commandVerb == "txProfile.save"
               || invoke.commandVerb == "txProfile.delete") {
        handleTxProfile(invoke);
    } else if (invoke.commandVerb == "rade.resetVocoder") {
        handleRadeResetVocoder(invoke);
    } else if (invoke.commandVerb == "txEq.setCurve" || invoke.commandVerb == "txEq.resetCurve") {
        handleTxEqCurve(invoke);
    } else if (invoke.commandVerb == "cfc.setProfile") {
        handleCfcProfile(invoke);
    } else if (invoke.commandVerb == "paProfile.select" || invoke.commandVerb == "paProfile.new"
               || invoke.commandVerb == "paProfile.copy" || invoke.commandVerb == "paProfile.delete"
               || invoke.commandVerb == "paProfile.reset"
               || invoke.commandVerb == "paProfile.setGain"
               || invoke.commandVerb == "paProfile.setAdjust"
               || invoke.commandVerb == "paProfile.setMaxPower"
               || invoke.commandVerb == "paProfile.setUseMax") {
        handlePaProfile(invoke);
    } else if (invoke.commandVerb == "requestIoBoardProbe") {
        handleRequestIoBoardProbe(invoke);
    } else if (invoke.commandVerb == "setAlexRxAntenna") {
        handleSetAlexRxAntenna(invoke);
    } else if (invoke.commandVerb == "setAlexRxAntennaForRadio") {
        const QString refusal = radioAntennaRowRefusal(invoke, m_radioModel);
        if (!refusal.isEmpty()) {
            emitResult(invoke.commandVerb, invoke.commandId, false, refusal, {});
        } else {
            SessionMessage row = invoke;
            row.arguments.removeIf([](const MirrorUpdate& arg) { return arg.name == "mac"; });
            handleSetAlexRxAntenna(row);
        }
    } else if (invoke.commandVerb == "setAlexBpfMode") {
        handleSetAlexBpfMode(invoke);
    } else if (invoke.commandVerb == "setAlexTxAntenna") {
        handleSetAlexTxAntenna(invoke);
    } else if (invoke.commandVerb == "setAlexTxAntennaForRadio") {
        const QString refusal = radioAntennaRowRefusal(invoke, m_radioModel);
        if (!refusal.isEmpty()) {
            emitResult(invoke.commandVerb, invoke.commandId, false, refusal, {});
        } else {
            SessionMessage row = invoke;
            row.arguments.removeIf([](const MirrorUpdate& arg) { return arg.name == "mac"; });
            handleSetAlexTxAntenna(row);
        }
    } else if (invoke.commandVerb == "requestIoBoardI2c") {
        handleRequestIoBoardI2c(invoke);
    } else if (invoke.commandVerb == "setIoBoardOutput") {
        handleSetIoBoardOutput(invoke);
    } else if (invoke.commandVerb == "setRadioSampleRate") {
        handleSetRadioSampleRate(invoke);
    } else if (invoke.commandVerb == "resetLevelCalibration") {
        handleResetLevelCalibration(invoke);
    } else if (invoke.commandVerb == "startLevelCalibration") {
        handleStartLevelCalibration(invoke);
    } else if (invoke.commandVerb == "cancelLevelCalibration") {
        handleCancelLevelCalibration(invoke);
    } else if (invoke.commandVerb == "dsp.filterResponse") {
        handleFilterResponse(invoke);
    } else if (invoke.commandVerb == "records.subscribe"
               || invoke.commandVerb == "records.unsubscribe"
               || invoke.commandVerb == "txModMonitor.reset") {
        handleRecords(invoke);
    } else if (invoke.commandVerb == "station.selectRadio"
               || invoke.commandVerb == "station.rescanRadios"
               || invoke.commandVerb == "station.setRadioModel"
               || invoke.commandVerb == "station.forgetRadio") {
        handleStationRadios(invoke);
    } else if (invoke.commandVerb == "station.validateSettings"
               || invoke.commandVerb == "station.forgetSettings"
               || invoke.commandVerb == "station.repairSettings") {
        handleSettingsHygiene(invoke);
    } else if (invoke.commandVerb == "spots.connect" || invoke.commandVerb == "spots.disconnect"
               || invoke.commandVerb == "spots.sendCommand"
               || invoke.commandVerb == "spots.clearAll") {
        handleSpotSources(invoke);
    } else if (invoke.commandVerb == "freedv.setMessage" || invoke.commandVerb == "freedv.sendQsy"
               || invoke.commandVerb == "freedv.setHidden") {
        handleFreedv(invoke);
    } else if (invoke.commandVerb == "support.collect"
               || invoke.commandVerb == "support.setLogCategories") {
        handleSupport(invoke);
    } else if (invoke.commandVerb == "nnr.setDiagnostics" || invoke.commandVerb == "nnr.resetTuning"
               || invoke.commandVerb == "nnr.tryAgain") {
        handleNnrAction(invoke);
    } else {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core does not know this request. Updating the Core may help."), {});
    }
}

void SessionCommandDispatcher::emitRefusal(const QByteArray& verb, quint32 commandId,
                                           const TxRefusal& refusal)
{
    emit commandResultReady(SessionMessages::commandResult(
        verb, commandId, false, refusal.text, {},
        {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(refusal.code)},
         {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(refusal.fix)}}));
}

bool SessionCommandDispatcher::refusedForTheHolder(const QByteArray& verb, quint32 commandId)
{
    // Ruling 7.7 (iPhone app plan Task 77): a change to the transmitter's
    // own settings from a device that does not hold transmit, while another
    // does, is refused with the holder's name (the Core's rule,
    // TransmitAccess::transmitter). True when it answered.
    if (!m_transmitAccess.transmitter) {
        return false;
    }
    const TxRefusal refusal = m_transmitAccess.transmitter(m_requester);
    if (refusal.isEmpty()) {
        return false;
    }
    emitRefusal(verb, commandId, refusal);
    return true;
}

bool SessionCommandDispatcher::refusedWhileOnAir(const SessionMessage& invoke)
{
    if (m_requester.isEmpty() || !m_transmitAccess.onAir || m_radioModel.isNull()) {
        return false;
    }
    // Ruling 7.4's list, as it exists when Task 34 lands. The transmit
    // path: the amplifier and its settings verbs, the tuner, a receive or
    // transmit antenna (setAlexRxAntenna; the slices' and alexAntennas'
    // antennas are property writes, refused in StationServer), PureSignal
    // (ps3.* but two-tone), the interlock and the power cap. Task 42 adds
    // its own verbs.
    static const QSet<QByteArray> kTransmitPath{
        QByteArrayLiteral("configurePgxl"), QByteArrayLiteral("disconnectPgxl"),
        QByteArrayLiteral("setPgxlConnectionSettings"), QByteArrayLiteral("setPgxlName"),
        QByteArrayLiteral("setPgxlHardware"), QByteArrayLiteral("setPgxlNetwork"),
        QByteArrayLiteral("savePgxlSettings"), QByteArrayLiteral("configureTgxl"),
        QByteArrayLiteral("disconnectTgxl"), QByteArrayLiteral("setTgxlName"),
        QByteArrayLiteral("setTgxlNetwork"), QByteArrayLiteral("saveTgxlSettings"),
        QByteArrayLiteral("setTgxlAntenna"), QByteArrayLiteral("setTgxlOperate"),
        QByteArrayLiteral("setTgxlBypass"), QByteArrayLiteral("setAlexRxAntenna"),
        QByteArrayLiteral("setAlexRxAntennaForRadio"),
        QByteArrayLiteral("amp.operate"), QByteArrayLiteral("amp.standby"),
        QByteArrayLiteral("tuner.operate"), QByteArrayLiteral("tuner.bypass"),
        QByteArrayLiteral("tuner.antenna"), QByteArrayLiteral("rfkit.operate"),
        QByteArrayLiteral("rfkit.standby"), QByteArrayLiteral("rfkit.antenna"),
        QByteArrayLiteral("setTxInterlockPolicy"), QByteArrayLiteral("setPgxlPowerCap")};
    bool waits = kTransmitPath.contains(invoke.commandVerb);
    if (invoke.commandVerb.startsWith("ps3.")) {
        waits = invoke.commandVerb != "ps3.twoTone" && invoke.commandVerb != "ps3.subscribeDisplay";
    }
    if (invoke.commandVerb == "requestSliceSampleRate") {
        // A Protocol 1 rate change stops the radio's data flow. The
        // Protocol 2 rate changes are Task 75's to route.
        waits = m_radioModel->currentRadioInfo().protocol == ProtocolVersion::Protocol1;
    }
    if (invoke.commandVerb == "requestStreamCentre") {
        // A C-Tune centre change on the receiver of the holder's transmit
        // slice moves that slice's receiver.
        int sliceId = -1;
        const SliceModel* txSlice = m_radioModel->txBoundSlice();
        if (findIntArgument(invoke.arguments, "sliceId", &sliceId) == ArgumentStatus::Ok
            && txSlice != nullptr) {
            const SliceModel* slice = m_radioModel->sliceById(sliceId);
            waits = slice != nullptr
                && (slice == txSlice
                    || (slice->streamIndex() >= 0 && slice->streamIndex() == txSlice->streamIndex()));
        }
    }
    if (!waits) {
        return false;
    }
    const TxRefusal refusal = m_transmitAccess.onAir(m_requester);
    if (refusal.isEmpty()) {
        return false;
    }
    emitRefusal(invoke.commandVerb, invoke.commandId, refusal);
    return true;
}

void SessionCommandDispatcher::handleAccessoryTx(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    // The Core checks this again on confirm.proceed. The holder is omitted
    // from this gate: an idle holder is asked by handleSharedSetting.
    if (!m_transmitAccess.accessory) {
        emitRefusal(verb, invoke.commandId, TxRefusals::stationReceiveOnly());
        return;
    }
    if (const TxRefusal refusal = m_transmitAccess.accessory(m_requester); !refusal.isEmpty()) {
        emitRefusal(verb, invoke.commandId, refusal);
        return;
    }
    const bool portVerb = verb == "tuner.antenna" || verb == "rfkit.antenna";
    const bool boolVerb = verb == "tuner.operate" || verb == "tuner.bypass";
    int port = 0;
    QVariant on;
    const bool readable = portVerb
        ? hasExactlyArguments(invoke.arguments, {"port"})
              && hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
              && findIntArgument(invoke.arguments, "port", &port) == ArgumentStatus::Ok
        : boolVerb
            ? hasExactlyArguments(invoke.arguments, {"on"})
                  && findArgument(invoke.arguments, "on", &on)
                  && on.typeId() == QMetaType::Bool
            : hasExactlyArguments(invoke.arguments, {});
    if (!readable) {
        emitRefusal(verb, invoke.commandId,
                    {"invalidRequest", QStringLiteral("The Core could not read this request."), {}});
        return;
    }
    if (verb == "tuner.tune") {
        // RemoteKeying::TunerTune takes unheld transmit before starting the
        // Core's PGXL/TGXL cycle, and refuses another holder or an on-air
        // cycle before either accessory receives a command.
        handleTxKeying(invoke);
        return;
    }
    QString reason;
    const bool sent = verb == "amp.operate" ? m_radioModel->setPgxlOperateForStation(true, &reason)
        : verb == "amp.standby" ? m_radioModel->setPgxlOperateForStation(false, &reason)
        : verb == "tuner.operate" ? m_radioModel->setTgxlOperateForStation(on.toBool(), &reason)
        : verb == "tuner.bypass" ? m_radioModel->setTgxlBypassForStation(on.toBool(), &reason)
        : verb == "tuner.antenna" ? m_radioModel->setTgxlAntennaForStation(port, &reason)
        : verb == "rfkit.operate" ? m_radioModel->setRfKitOperateForStation(true, &reason)
        : verb == "rfkit.standby" ? m_radioModel->setRfKitOperateForStation(false, &reason)
                                     : m_radioModel->setRfKitAntennaForStation(port, &reason);
    if (!sent) {
        emitRefusal(verb, invoke.commandId,
                    {"accessoryUnavailable",
                     reason.isEmpty() ? QStringLiteral("The Core could not switch this accessory.")
                                      : reason, {}});
        return;
    }
    emitResult(verb, invoke.commandId, true, {}, {});
}

void SessionCommandDispatcher::handleTxMicSource(const SessionMessage& invoke)
{
    QString name;
    const bool readable = hasExactlyArguments(invoke.arguments, {"source"})
        && invoke.arguments.first().ordinal == 0
        && findUtf8Argument(invoke.arguments, "source", &name);
    const auto source = readable ? remoteMicSourceFromName(name) : std::nullopt;
    if (!m_transmitAccess.micSource) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   source ? remoteMicLegacyReason() : QStringLiteral("The Core could not read this request."), {});
        return;
    }
    const QString owner = m_sessionOwner;
    const auto result = m_transmitAccess.micSource(invoke, owner, m_requester, source);
    emitResultAs(owner, result);
}

void SessionCommandDispatcher::handleTxKeying(const SessionMessage& invoke)
{
    // iPhone app plan Task 35 (R-IOS-13): keying from a device. The rules
    // are RemoteKeying's; this reads the arguments and answers.
    RemoteKeying::Command command;
    command.deviceId = m_requester;
    command.session = m_sessionOwner;
    command.commandId = invoke.commandId;
    bool readable = false;
    if (invoke.commandVerb == "tx.key") {
        command.verb = RemoteKeying::Verb::Key;
        QString trigger;
        readable = hasExactlyArguments(invoke.arguments, {"trigger"})
            && findUtf8Argument(invoke.arguments, "trigger", &trigger)
            && RemoteKeying::isTrigger(trigger.toUtf8());
        command.trigger = trigger.toUtf8();
    } else if (invoke.commandVerb == "tx.unkey") {
        command.verb = RemoteKeying::Verb::Unkey;
        QVariant raw;
        bool converted = false;
        readable = hasExactlyArguments(invoke.arguments, {"epoch"})
            && hasWireKind(invoke.arguments, "epoch", MirrorWireKind::Int64)
            && findArgument(invoke.arguments, "epoch", &raw);
        const qlonglong epoch = readable ? raw.toLongLong(&converted) : 0;
        // An epoch the Core issued: 1 to 4294967295.
        readable = readable && converted && epoch >= 1
            && epoch <= static_cast<qlonglong>(std::numeric_limits<quint32>::max());
        command.epoch = readable ? static_cast<quint32>(epoch) : 0;
    } else {
        command.verb = invoke.commandVerb == "tx.tune"        ? RemoteKeying::Verb::Tune
                     : invoke.commandVerb == "tx.tunerTune"
                           || invoke.commandVerb == "tuner.tune" ? RemoteKeying::Verb::TunerTune
                                                            : RemoteKeying::Verb::TwoTone;
        QVariant on;
        readable = invoke.commandVerb == "tuner.tune"
            ? hasExactlyArguments(invoke.arguments, {})
            : hasExactlyArguments(invoke.arguments, {"on"})
            && findArgument(invoke.arguments, "on", &on) && on.typeId() == QMetaType::Bool;
        command.on = readable && (invoke.commandVerb == "tuner.tune" || on.toBool());
    }
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (!m_transmitAccess.keying) {
        // No Core transmit rules behind this dispatcher: nothing keys.
        emitRefusal(invoke.commandVerb, invoke.commandId, TxRefusals::stationReceiveOnly());
        return;
    }
    // Task 36: answered through the reply, now or once a key's microphone
    // buffer has filled or timed out.
    //
    // Merge of the trunk into the transmit lane (the multi-client fix wave
    // I1): a result is named by the session it answers, so every answer,
    // at once or later, is emitted as this dispatch's session owner's.
    const QPointer<SessionCommandDispatcher> self(this);
    const QByteArray verb = invoke.commandVerb;
    const quint32 id = invoke.commandId;
    const QString owner = m_sessionOwner;
    m_transmitAccess.keying(command, [self, verb, id, owner](const RemoteKeying::Result& result) {
        if (self.isNull()) {
            return;
        }
        QList<MirrorUpdate> values;
        QString reason;
        if (result.accepted) {
            if (result.epoch != 0) {
                values.append({0, "epoch", MirrorWireKind::Int64,
                               QVariant(static_cast<qlonglong>(result.epoch))});
            }
        } else if (!result.refusal.isEmpty()) {
            reason = result.refusal.text;
            values = {{0, "refusalCode", MirrorWireKind::Utf8, QString::fromUtf8(result.refusal.code)},
                      {0, "refusalFix", MirrorWireKind::Utf8, QString::fromUtf8(result.refusal.fix)}};
        } else if (verb == "tuner.tune") {
            reason = result.reason;
            values = {{0, "refusalCode", MirrorWireKind::Utf8,
                       QStringLiteral("accessoryUnavailable")},
                      {0, "refusalFix", MirrorWireKind::Utf8, QString()}};
        } else {
            reason = result.reason;
        }
        self->emitResultAs(owner, SessionMessages::commandResult(verb, id, result.accepted,
                                                                 reason, {}, values));
    });
}

void SessionCommandDispatcher::handleTxTake(const SessionMessage& invoke)
{
    // iPhone app plan Task 77 (R-IOS-02, R-IOS-03; rulings 8.4, 8.6,
    // 8.7): the rules are the Core's (TransmitAccess::take); this reads the
    // arguments. Both are optional, and sent together or not at all.
    std::optional<quint64> holderEpoch;
    std::optional<bool> shownKeyed;
    bool readable = true;
    for (const MirrorUpdate& argument : invoke.arguments) {
        if (argument.name == "holderEpoch" && argument.kind == MirrorWireKind::Int64
            && !holderEpoch.has_value()) {
            bool converted = false;
            const qlonglong value = argument.value.toLongLong(&converted);
            readable = readable && converted && value >= 0;
            holderEpoch = static_cast<quint64>(std::max<qlonglong>(0, value));
        } else if (argument.name == "shownKeyed" && argument.kind == MirrorWireKind::Bool
                   && argument.value.typeId() == QMetaType::Bool && !shownKeyed.has_value()) {
            shownKeyed = argument.value.toBool();
        } else {
            readable = false;
        }
    }
    if (holderEpoch.has_value() != shownKeyed.has_value()) {
        readable = false;
    }
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (!m_transmitAccess.take) {
        emitRefusal(invoke.commandVerb, invoke.commandId, TxRefusals::stationReceiveOnly());
        return;
    }
    const QPointer<SessionCommandDispatcher> self(this);
    const QString owner = m_sessionOwner;
    m_transmitAccess.take(invoke, holderEpoch, shownKeyed,
                          [self, owner](const SessionMessage& result) {
                              if (!self.isNull()) {
                                  self->emitResultAs(owner, result);
                              }
                          });
}

void SessionCommandDispatcher::handleTxKeepalive(const SessionMessage& invoke)
{
    // iPhone app plan Task 37 (R-IOS-13): the watchdog's rules are
    // RemoteTxWatchdog's; this reads the arguments and answers. sequence
    // is 1 or more (a whole number a JSON number carries exactly); epoch is
    // the device's key's, 0 to 4294967295.
    constexpr qlonglong kMaxExactInteger = 9007199254740991LL;
    QVariant rawSequence;
    QVariant rawEpoch;
    bool sequenceOk = false;
    bool epochOk = false;
    bool readable = hasExactlyArguments(invoke.arguments, {"sequence", "epoch"})
        && hasWireKind(invoke.arguments, "sequence", MirrorWireKind::Int64)
        && hasWireKind(invoke.arguments, "epoch", MirrorWireKind::Int64)
        && findArgument(invoke.arguments, "sequence", &rawSequence)
        && findArgument(invoke.arguments, "epoch", &rawEpoch);
    const qlonglong sequence = readable ? rawSequence.toLongLong(&sequenceOk) : 0;
    const qlonglong epoch = readable ? rawEpoch.toLongLong(&epochOk) : -1;
    readable = readable && sequenceOk && epochOk && sequence >= 1 && sequence <= kMaxExactInteger
        && epoch >= 0 && epoch <= static_cast<qlonglong>(std::numeric_limits<quint32>::max());
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (!m_transmitAccess.keepalive) {
        emitRefusal(invoke.commandVerb, invoke.commandId, TxRefusals::stationReceiveOnly());
        return;
    }
    m_transmitAccess.keepalive(m_requester, static_cast<quint64>(sequence),
                               static_cast<quint32>(epoch));
    // Answered accepted whether or not it counted (a keepalive while
    // nothing is watched, a copy, an older epoch): it changes nothing then.
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

void SessionCommandDispatcher::handleSetTxSlice(const SessionMessage& invoke)
{
    // iPhone app plan Task 34 (R-IOS-02, ruling 8.10): the holder's verb.
    // The argument is a slice's id (SliceModel::sliceIndex), never a list
    // position (remote design section 7.1).
    int sliceId = -1;
    if (!hasExactlyArguments(invoke.arguments, {"sliceId"})
        || !hasWireKind(invoke.arguments, "sliceId", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "sliceId", &sliceId) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (m_transmitAccess.txSlice) {
        const TxRefusal refusal = m_transmitAccess.txSlice(m_requester);
        if (!refusal.isEmpty()) {
            emitRefusal(invoke.commandVerb, invoke.commandId, refusal);
            return;
        }
    }
    if (m_radioModel->sliceById(sliceId) == nullptr) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That slice is no longer on the Core."), {});
        return;
    }
    // iPhone app plan Task 77 (ruling 8.10): the holder's own slices only.
    if (m_sliceAccess && !m_requester.isEmpty()) {
        const QString reason = m_sliceAccess(m_requester, sliceId);
        if (!reason.isEmpty()) {
            emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
            return;
        }
    }
    // While keyed the transmitter unkeys through the unkey gate first and
    // the flag moves once it is confirmed (TxSliceArbiter); unkeyed it
    // moves at once. Either way the slices' txSlice deltas carry it.
    TxSliceArbiter* arbiter = m_radioModel->txSliceArbiter();
    const bool moved = arbiter != nullptr && !m_requester.isEmpty()
        ? arbiter->requestHandoff(sliceId, m_requester)
        : m_radioModel->requestTxHandoffToSlice(sliceId);
    if (!moved) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That slice is no longer on the Core."), {});
        return;
    }
    // Slice control fix wave (Important 4): the device's explicit choice.
    // Without a requester the Core's own window chose, and RadioModel's
    // txSliceSelected records it.
    if (!m_requester.isEmpty() && m_transmitAccess.txSliceChosen) {
        m_transmitAccess.txSliceChosen(m_requester, sliceId);
    }
    QList<QByteArray> affected;
    for (const SliceModel* slice : m_radioModel->slices()) {
        if (slice != nullptr) {
            affected.append(ObjectRegistry::keyForSlice(slice->sliceIndex()));
        }
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), affected);
}

bool SessionCommandDispatcher::refusedForAnotherDevice(const SessionMessage& invoke)
{
    // Every verb that names a slice by `sliceId` (ruling 5.9): a device
    // addresses only its own slices. requestSliceSampleRate,
    // requestStreamCtunPinned and requestStreamCentre are routes on the
    // requester's own slice (D53, the anchor rules; Tasks 74 and 75, which
    // run before this), and refusals on anyone else's, a slice nobody owns
    // or one held for a device, whatever receivers are in use (fix wave
    // C1). slice.selectBand and notch.addAtSlice (R-IOS-27) joined at the
    // checkpoint merge: a band button or +TNF acts on its own slice only.
    // Slice control plan Task 2: "its own" is the change predicate, so a
    // listener's verbs on a slice it hears are refused as well.
    // startLevelCalibration (Level Cal 2): a run retunes the slice and
    // switches its preamp, so a device calibrates only a slice it may change.
    static const QSet<QByteArray> kSliceVerbs{
        QByteArrayLiteral("removeSlice"), QByteArrayLiteral("setActiveSliceById"),
        QByteArrayLiteral("nnr.setDiagnostics"), QByteArrayLiteral("nnr.resetTuning"),
        QByteArrayLiteral("nnr.tryAgain"), QByteArrayLiteral("notch.add"),
        QByteArrayLiteral("requestSliceSampleRate"), QByteArrayLiteral("requestStreamCentre"),
        QByteArrayLiteral("requestStreamCtunPinned"), QByteArrayLiteral("slice.selectBand"),
        QByteArrayLiteral("notch.addAtSlice"), QByteArrayLiteral("startLevelCalibration")};
    if (m_requester.isEmpty() || !m_sliceAccess || !kSliceVerbs.contains(invoke.commandVerb)) {
        return false;
    }
    int sliceId = -1;
    if (findIntArgument(invoke.arguments, "sliceId", &sliceId) != ArgumentStatus::Ok) {
        return false;
    }
    const QString reason = m_sliceAccess(m_requester, sliceId);
    if (reason.isEmpty()) {
        return false;
    }
    emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
    return true;
}

void SessionCommandDispatcher::handlePureSignalAction(const SessionMessage& invoke)
{
    PureSignalSessionFacade* facade = m_radioModel->pureSignalFacade();
    const auto arguments = dspCommandValues(invoke.arguments);
    if (m_sessionOwner.isEmpty() || invoke.commandId == 0 || !arguments) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not use this PureSignal request."), {});
        return;
    }
    if (invoke.commandVerb == "ps3.subscribeDisplay") {
        if (arguments->size() != 1 || !arguments->contains("enabled")
            || arguments->value("enabled").typeId() != QMetaType::Bool) {
            emitResult(invoke.commandVerb, invoke.commandId, false,
                       QStringLiteral("The Core could not read the PureSignal display request."), {});
            return;
        }
        const bool enabled = arguments->value("enabled").toBool();
        QString refusal;
        const QPointer<SessionCommandDispatcher> self(this);
        const QPointer<PureSignalSessionFacade> currentFacade(facade);
        const QString owner = m_sessionOwner;
        const Ps3DisplayAdmissionHandler admission = m_ps3DisplayAdmission;
        if (admission && !admission(enabled, &refusal)) {
            if (self && owner == m_sessionOwner) {
                emitResult(invoke.commandVerb, invoke.commandId, false, refusal, {});
            }
            return;
        }
        if (!self || !currentFacade || owner != m_sessionOwner) { return; }
        facade->setRemoteAmpViewSubscribed(enabled);
        if (!self || owner != m_sessionOwner) { return; }
        emitResult(invoke.commandVerb, invoke.commandId, true, {}, {"pureSignal"});
        return;
    }
    const auto action = PureSignalSessionFacade::actionForVerb(invoke.commandVerb);
    if (!action) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core does not know this PureSignal action."), {});
        return;
    }
    // Read the request before deciding on it: a request that is not one
    // this action takes is refused as unreadable, not with the transmit
    // gate's reason below, so the app learns what it sent was wrong.
    if (!PureSignalSessionFacade::argumentsFit(*action, *arguments)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this PureSignal request."), {});
        return;
    }
    const bool stop = *action == Ps3Action::OffReset
        || (*action == Ps3Action::SetTwoTone && arguments->size() == 1
            && arguments->value("enabled").typeId() == QMetaType::Bool
            && !arguments->value("enabled").toBool());
    // The session currently advertises txPermitted=false. Keep the same hard
    // gate here even when a client bypasses its disabled controls. R4 owns
    // replacing this gate with negotiated, station-authorized transmit.
    // R-R3-49 (parity Task 7): arming keys nothing (Single Cal, Automatic,
    // Apply current correction, Restore a saved correction), so a peer
    // offered transmitSettingsVersion 7 may ask for it while the radio is
    // off the air. The two-tone test keys the radio and stays here.
    const bool arming = *action == Ps3Action::Single || *action == Ps3Action::StartAutomatic
        || *action == Ps3Action::ApplyCurrentCorrection
        || *action == Ps3Action::RestoreCorrection;
    // iPhone app plan Task 77 (rulings 7.7, 8.3): the two-tone test puts a
    // carrier on the air, so starting it is a key: the same key as
    // tx.twoTone {on:true}, under every holder rule (a person's start on
    // unheld transmit takes it and keys; another device's while held is
    // refused otherDeviceHolds) and watched as the device's key.
    if (*action == Ps3Action::SetTwoTone && !stop && m_transmitAccess.keying
        && !m_requester.isEmpty()) {
        RemoteKeying::Command command;
        command.verb = RemoteKeying::Verb::TwoTone;
        command.deviceId = m_requester;
        command.session = m_sessionOwner;
        command.commandId = invoke.commandId;
        command.on = true;
        const QPointer<SessionCommandDispatcher> self(this);
        const QByteArray verb = invoke.commandVerb;
        const quint32 commandId = invoke.commandId;
        const QString owner = m_sessionOwner;
        m_transmitAccess.keying(command, [self, verb, commandId, owner](
                                             const RemoteKeying::Result& result) {
            if (self.isNull()) {
                return;
            }
            QList<MirrorUpdate> values;
            QString reason;
            if (result.accepted) {
                values.append({0, "phase", MirrorWireKind::Utf8, QStringLiteral("completed")});
                if (result.epoch != 0) {
                    values.append({0, "epoch", MirrorWireKind::Int64,
                                   QVariant(static_cast<qlonglong>(result.epoch))});
                }
            } else if (!result.refusal.isEmpty()) {
                reason = result.refusal.text;
                values = {{0, "refusalCode", MirrorWireKind::Utf8,
                           QString::fromUtf8(result.refusal.code)},
                          {0, "refusalFix", MirrorWireKind::Utf8,
                           QString::fromUtf8(result.refusal.fix)}};
            } else {
                reason = result.reason;
            }
            self->emitResultAs(owner, SessionMessages::commandResult(
                                          verb, commandId, result.accepted, reason,
                                          result.accepted ? QList<QByteArray>{"pureSignal"}
                                                          : QList<QByteArray>{},
                                          values));
        });
        return;
    }
    if (!stop && *action != Ps3Action::SaveCorrection
        && !(arming && m_pureSignalArmingOffered)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("PureSignal cannot be run from a remote window."), {});
        return;
    }
    // Fix wave M2 (ruling 8.5): stopping the two-tone test is a release of
    // the transmission, the holder's; another device stops it only by
    // taking transmit.
    if (*action == Ps3Action::SetTwoTone && stop && m_transmitAccess.release) {
        if (const TxRefusal refusal = m_transmitAccess.release(m_requester); !refusal.isEmpty()) {
            emitRefusal(invoke.commandVerb, invoke.commandId, refusal);
            return;
        }
    }
    // Version 13: arming is taken on the air from a peer that may change
    // the transmit settings, as the local PureSignal dialog arms keyed.
    if (arming && !m_transmitSettingsOnAir) {
        QString onAir;
        if (m_radioModel->stationOnAirRefusal(&onAir)) {
            emitResult(invoke.commandVerb, invoke.commandId, false, onAir, {});
            return;
        }
    }
    for (const PendingPureSignalCommand& command : std::as_const(m_pureSignalCommands)) {
        // Fix wave I1: ids are counted per client, so another session's
        // action with the same id is not this one.
        if (command.commandId == invoke.commandId && command.owner == m_sessionOwner) {
            emitResult(invoke.commandVerb, invoke.commandId, false,
                       QStringLiteral("This PureSignal request is already in progress."), {});
            return;
        }
    }
    const quint32 id = facade->requestAction(*action, *arguments);
    if (!id) {
        emitResult(invoke.commandVerb, invoke.commandId, false, facade->lastActionError(), {});
        return;
    }
    m_pureSignalCommands.insert(id, {invoke.commandId, invoke.commandVerb, m_sessionOwner});
    emit commandResultReady(SessionMessages::commandResult(invoke.commandVerb, invoke.commandId,
        true, {}, {}, {{0, "phase", MirrorWireKind::Utf8, QStringLiteral("accepted")}}));
}

void SessionCommandDispatcher::handleNnrAction(const SessionMessage& invoke)
{
    const bool diagnostics = invoke.commandVerb == "nnr.setDiagnostics";
    const bool shapeValid = diagnostics
        ? hasExactlyArguments(invoke.arguments, {"sliceId", "testMode", "outputMode"})
        : hasExactlyArguments(invoke.arguments, {"sliceId"});
    int sliceId = -1;
    int testMode = 0;
    int outputMode = 1;
    bool typesValid = shapeValid && findIntArgument(invoke.arguments, "sliceId", &sliceId) == ArgumentStatus::Ok;
    for (const auto& argument : invoke.arguments) {
        typesValid = typesValid && argument.kind == MirrorWireKind::Int64;
    }
    if (diagnostics) {
        typesValid = typesValid && findIntArgument(invoke.arguments, "testMode", &testMode) == ArgumentStatus::Ok
            && findIntArgument(invoke.arguments, "outputMode", &outputMode) == ArgumentStatus::Ok;
    }
    SliceModel* slice = typesValid ? m_radioModel->sliceById(sliceId) : nullptr;
    if (!slice) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not apply this NNR change to that receiver."), {});
        return;
    }
    QString reason;
    bool accepted = false;
    if (diagnostics) {
        accepted = m_radioModel->setNnrDiagnosticMode(sliceId, testMode, outputMode, &reason);
    } else if (invoke.commandVerb == "nnr.tryAgain") {
        // R-R3-40: the operator asks for the saved NNR choice back. The
        // Core's RadioModel clears the runtime limit synchronously.
        slice->requestNnrRetry();
        accepted = slice->nnrLimit() == static_cast<int>(NnrLimit::None);
        if (!accepted) {
            reason = QStringLiteral("Noise reduction could not try again.");
        }
    } else {
        slice->resetNnrTuning();
        reason = slice->nnrLastError();
        accepted = reason.isEmpty();
    }
    emitResult(invoke.commandVerb, invoke.commandId, accepted, reason,
               accepted ? QList<QByteArray>{ObjectRegistry::keyForSlice(sliceId)} : QList<QByteArray>{});
}

void SessionCommandDispatcher::handleNotchAction(const SessionMessage& invoke)
{
    // Exact shapes, exact wire kinds: a remote window's notch edit either
    // names one notch (or one receiver, for an add) in the expected form or
    // changes nothing.
    const QByteArray& verb = invoke.commandVerb;
    const auto kindIs = [&invoke](const QByteArray& name, MirrorWireKind kind) {
        return hasWireKind(invoke.arguments, name, kind);
    };
    int id = -1;
    int sliceId = -1;
    double centreHz = 0.0;
    double widthHz = 0.0;
    bool active = false;
    bool valid = false;
    if (verb == "notch.add") {
        valid = hasExactlyArguments(invoke.arguments, {"sliceId", "centreHz", "widthHz"})
            && kindIs("sliceId", MirrorWireKind::Int64)
            && findIntArgument(invoke.arguments, "sliceId", &sliceId) == ArgumentStatus::Ok
            && kindIs("centreHz", MirrorWireKind::Float64)
            && kindIs("widthHz", MirrorWireKind::Float64)
            && findFiniteDoubleArgument(invoke.arguments, "centreHz", &centreHz)
            && findFiniteDoubleArgument(invoke.arguments, "widthHz", &widthHz);
    } else if (verb == "notch.move") {
        valid = hasExactlyArguments(invoke.arguments, {"id", "centreHz", "widthHz"})
            && kindIs("id", MirrorWireKind::Int64)
            && findIntArgument(invoke.arguments, "id", &id) == ArgumentStatus::Ok
            && kindIs("centreHz", MirrorWireKind::Float64)
            && kindIs("widthHz", MirrorWireKind::Float64)
            && findFiniteDoubleArgument(invoke.arguments, "centreHz", &centreHz)
            && findFiniteDoubleArgument(invoke.arguments, "widthHz", &widthHz);
    } else if (verb == "notch.setActive") {
        QVariant raw;
        valid = hasExactlyArguments(invoke.arguments, {"id", "active"})
            && kindIs("id", MirrorWireKind::Int64)
            && findIntArgument(invoke.arguments, "id", &id) == ArgumentStatus::Ok
            && kindIs("active", MirrorWireKind::Bool)
            && findArgument(invoke.arguments, "active", &raw)
            && raw.typeId() == QMetaType::Bool;
        active = raw.toBool();
    } else if (verb == "notch.delete") {
        valid = hasExactlyArguments(invoke.arguments, {"id"})
            && kindIs("id", MirrorWireKind::Int64)
            && findIntArgument(invoke.arguments, "id", &id) == ArgumentStatus::Ok;
    } else if (verb == "notch.addAtSlice") {
        // R-IOS-27, R-IOS-06 (notchControlVersion 2): the desktop's +TNF.
        // The Core composes the centre and width from its own slice.
        valid = hasExactlyArguments(invoke.arguments, {"sliceId"})
            && kindIs("sliceId", MirrorWireKind::Int64)
            && findIntArgument(invoke.arguments, "sliceId", &sliceId) == ArgumentStatus::Ok;
    } else {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The Core does not know this request. Updating the Core may help."), {});
        return;
    }
    if (!valid) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("This notch change is not one this Core understands."), {});
        return;
    }

    QString reason;
    bool accepted = false;
    int addedId = -1;
    if (verb == "notch.add") {
        accepted = m_radioModel->addNotchFromStation(sliceId, centreHz, widthHz, &addedId, &reason);
    } else if (verb == "notch.addAtSlice") {
        accepted = m_radioModel->addTnfFromStation(sliceId, &addedId, &reason);
    } else if (verb == "notch.move") {
        accepted = m_radioModel->moveNotchFromStation(id, centreHz, widthHz, &reason);
    } else if (verb == "notch.setActive") {
        accepted = m_radioModel->setNotchActiveFromStation(id, active, &reason);
    } else {
        accepted = m_radioModel->deleteNotchFromStation(id, &reason);
    }

    QList<MirrorUpdate> values;
    if (accepted) {
        // The list revision after this change, so the window can hold its
        // own view of the edit until the mirror has caught up with it.
        values.append({0, "revision", MirrorWireKind::Int64,
                       static_cast<qlonglong>(m_radioModel->notchListRevision())});
        if (verb == "notch.add" || verb == "notch.addAtSlice") {
            values.append({0, "id", MirrorWireKind::Int64, static_cast<qlonglong>(addedId)});
        }
    }
    emit commandResultReady(SessionMessages::commandResult(
        verb, invoke.commandId, accepted, accepted ? QString() : reason,
        accepted ? QList<QByteArray>{"notches"} : QList<QByteArray>{}, values));
}

void SessionCommandDispatcher::emitResult(const QByteArray& verb, quint32 commandId,
                                          bool accepted, const QString& reason,
                                          const QList<QByteArray>& affectedKeys)
{
    emit commandResultReady(
        SessionMessages::commandResult(verb, commandId, accepted, reason, affectedKeys));
}

void SessionCommandDispatcher::emitResultAs(const QString& owner, const SessionMessage& result)
{
    // Fix wave I1: saved and restored, so a later result emitted inside
    // another session's dispatch leaves that dispatch's owner in place.
    const std::optional<QString> previous = m_resultOwner;
    const QPointer<SessionCommandDispatcher> self(this);
    m_resultOwner = owner;
    emit commandResultReady(result);
    if (self) { m_resultOwner = previous; }
}

// ── addSlice ─────────────────────────────────────────────────────────────

void SessionCommandDispatcher::handleAddSlice(const SessionMessage& invoke)
{
    QVariant panIdArg;
    if (!findArgument(invoke.arguments, "initialPanId", &panIdArg)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }

    // RadioModel::addSlice() (RadioModel.cpp) can reject a placement after
    // partial construction, rolling back and returning -1 -- but only
    // after already emitting sliceAddRejected with the real, human-
    // readable reason (design addendum: "Rejected creation is first-class
    // ... The command result carries the reason"). A temporary connection
    // captures it; the call is synchronous, so the emission (if any)
    // happens before addSlice() returns and before this connection is torn
    // down. The by-reference capture below is correct only under the
    // class-level same-thread invariant: it relies on the connected signal
    // firing synchronously, inside this call, before `rejectionReason`
    // goes out of scope. A future thread split that made this connection
    // cross-thread would auto-queue it and turn this into a dangling read.
    QString rejectionReason;
    const QMetaObject::Connection conn = connect(
        m_radioModel, &RadioModel::sliceAddRejected, this,
        [&rejectionReason](const QString& reason) { rejectionReason = reason; });
    int id = -1;
    {
        // iPhone app Task 73: the new slice is the requesting device's.
        const SliceOwnership::CreatorScope creator(m_radioModel->sliceOwnership(), m_requester);
        id = m_radioModel->addSlice(panIdArg.toString());
    }
    QObject::disconnect(conn);

    if (id < 0) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   rejectionReason.isEmpty()
                       ? QStringLiteral("The Core could not add another receiver.")
                       : rejectionReason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(),
               { ObjectRegistry::keyForSlice(id) });
}

// ── removeSlice ──────────────────────────────────────────────────────────

void SessionCommandDispatcher::handleRemoveSlice(const SessionMessage& invoke)
{
    int sliceId = 0;
    switch (findIntArgument(invoke.arguments, "sliceId", &sliceId)) {
    case ArgumentStatus::Missing:
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    case ArgumentStatus::NotRepresentable:
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   notRepresentableReason(), {});
        return;
    case ArgumentStatus::Ok:
        break;
    }

    // Fix wave 3 (Important 1): the slice asked about, by identity, so a
    // slice closed and its id reused before the change runs (by any
    // device, the asker included) is never changed in its place.
    const QPointer<SliceModel> askedSlice(m_radioModel->sliceById(sliceId));
    if (askedSlice.isNull()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That receiver is no longer on the Core."), {});
        return;
    }
    // Slice control plan Task 4 (ruling Q6): closing a slice other devices
    // still listen to, from its controller, releases it instead: it stays
    // for them. An older window's close follows the same rule.
    if (!m_requester.isEmpty() && !m_sliceAccessController.isNull()
        && m_sliceAccessController->closeIsRelease(m_requester, sliceId)) {
        const SliceOwnership* ownership = m_radioModel->sliceOwnership();
        const SliceAccessController::Result result = m_sliceAccessController->release(
            m_requester, ownership->refOf(sliceId), ownership->controlRevision(sliceId));
        emitResult(invoke.commandVerb, invoke.commandId, result.accepted, result.reason,
                   result.accepted ? QList<QByteArray>{ObjectRegistry::keyForSlice(sliceId)}
                                   : QList<QByteArray>{});
        return;
    }
    // Slice control plan Task 7: the Core's last slice closes by the claims
    // rule when its controller closes it and nobody else is on it (a
    // release that leaves nobody on it); the Core then has no slice.
    if (m_radioModel->slices().size() <= 1 && !m_requester.isEmpty()
        && !m_sliceAccessController.isNull()) {
        const SliceOwnership* ownership = m_radioModel->sliceOwnership();
        const SliceAccessController::Result result = m_sliceAccessController->release(
            m_requester, ownership->refOf(sliceId), ownership->controlRevision(sliceId));
        emitResult(invoke.commandVerb, invoke.commandId, result.accepted, result.reason,
                   result.accepted ? QList<QByteArray>{ObjectRegistry::keyForSlice(sliceId)}
                                   : QList<QByteArray>{});
        return;
    }
    if (m_radioModel->slices().size() <= 1) {
        // RadioModel::removeSlice() (RadioModel.cpp) silently no-ops rather
        // than remove the last remaining slice -- no signal marks this
        // rejection (there is nothing wrong with the request itself, only
        // with the station's state), so it has to be caught here, before
        // the call, or the result would wrongly claim success.
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The last receiver cannot be removed."), {});
        return;
    }

    const QByteArray key = ObjectRegistry::keyForSlice(sliceId);
    m_radioModel->removeSlice(sliceId);
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), { key });
}

// ── slice.selectBand ─────────────────────────────────────────────────────

// R-IOS-27, R-IOS-06 (bandSelectVersion 1): a band button of the desktop's
// per-pan BAND grid, for one slice. The Core runs the desktop's own band
// change on that slice (RadioModel::onBandButtonClicked(SliceModel*, Band),
// as ContainerButtonDispatcher does for a container's slice), so the band's
// saved frequency, mode and filter come back, or its seed on a first visit.
// The desktop offers every grid band on every radio and does not hold a
// band change while the radio is on the air, so neither is refused here.
// Its own refusal (a locked slice) comes back through bandClickIgnored with
// the desktop's words. A band the slice is already on changes nothing and
// is accepted, as the desktop's click is silently a no-op.
void SessionCommandDispatcher::handleSelectBand(const SessionMessage& invoke)
{
    int sliceId = -1;
    int bandId = -1;
    if (!hasExactlyArguments(invoke.arguments, { "sliceId", "band" })
        || !hasWireKind(invoke.arguments, "sliceId", MirrorWireKind::Int64)
        || !hasWireKind(invoke.arguments, "band", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "sliceId", &sliceId) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "band", &bandId) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to change band was not understood."), {});
        return;
    }
    SliceModel* const slice = m_radioModel->sliceById(sliceId);
    if (slice == nullptr) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That receiver is no longer on the Core."), {});
        return;
    }
    const BandGridEntry* entry = nullptr;
    for (const BandGridEntry& candidate : kBandGrid) {
        if (static_cast<int>(candidate.band) == bandId) {
            entry = &candidate;
        }
    }
    if (entry == nullptr) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core has no band button for that band."), {});
        return;
    }

    // onBandButtonClicked returns nothing; a refusal is its bandClickIgnored
    // signal, emitted synchronously inside the call (the same-thread
    // invariant handleAddSlice relies on).
    QString ignoredReason;
    const QMetaObject::Connection conn = connect(
        m_radioModel, &RadioModel::bandClickIgnored, this,
        [&ignoredReason](Band, const QString& reason) { ignoredReason = reason; });
    m_radioModel->onBandButtonClicked(slice, entry->band);
    QObject::disconnect(conn);

    if (!ignoredReason.isEmpty()) {
        emitResult(invoke.commandVerb, invoke.commandId, false, ignoredReason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(),
               { ObjectRegistry::keyForSlice(sliceId) });
}

// ── addSliceOnPan ────────────────────────────────────────────────────────

void SessionCommandDispatcher::handleAddSliceOnPan(const SessionMessage& invoke)
{
    QVariant panIdArg;
    if (!findArgument(invoke.arguments, "panId", &panIdArg)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }

    // addSliceOnPan() returns void (RadioModel.h), unlike addSlice(), so
    // the outcome has to be read off the two signals its own cap check and
    // its addSlice() delegate can each produce: sliceAdded(id) on success,
    // sliceAddRejected(reason) either from the cap check itself or from
    // addSlice()'s own allocator rollback. Both connections are torn down
    // immediately after the synchronous call returns. As in handleAddSlice
    // above, the by-reference captures below depend on the class-level
    // same-thread invariant -- a cross-thread connection would auto-queue
    // and read `newId`/`rejectionReason` after they are gone.
    int newId = -1;
    QString rejectionReason;
    const QMetaObject::Connection addedConn = connect(
        m_radioModel, &RadioModel::sliceAdded, this,
        [&newId](int id) { newId = id; });
    const QMetaObject::Connection rejectedConn = connect(
        m_radioModel, &RadioModel::sliceAddRejected, this,
        [&rejectionReason](const QString& reason) { rejectionReason = reason; });

    {
        // iPhone app Task 73: the new slice is the requesting device's.
        const SliceOwnership::CreatorScope creator(m_radioModel->sliceOwnership(), m_requester);
        m_radioModel->addSliceOnPan(panIdArg.toString());
    }

    QObject::disconnect(addedConn);
    QObject::disconnect(rejectedConn);

    if (newId < 0) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   rejectionReason.isEmpty()
                       ? QStringLiteral("The Core could not add another receiver.")
                       : rejectionReason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(),
               { ObjectRegistry::keyForSlice(newId) });
}

// ── requestSliceSampleRate ───────────────────────────────────────────────

void SessionCommandDispatcher::handleRequestSliceSampleRate(const SessionMessage& invoke)
{
    // Fix wave: a confirmed rate change's closes, for this dispatch only.
    const QHash<int, ClosingSlice> closingOwners = std::exchange(m_rateClosing, {});
    const std::function<void(int)> close = std::exchange(m_rateClose, {});
    const QString changedReason = std::exchange(m_rateChangedReason, {});
    int sliceId = 0;
    int rateHz = 0;
    const ArgumentStatus sliceIdStatus =
        findIntArgument(invoke.arguments, "sliceId", &sliceId);
    const ArgumentStatus rateHzStatus = findIntArgument(invoke.arguments, "rateHz", &rateHz);
    // One combined message for the missing case, as before -- naming both
    // is what tells a peer this verb needs the pair. The out-of-range
    // case names the offending argument specifically, because there the
    // peer sent something and needs to know WHICH one was refused.
    if (sliceIdStatus == ArgumentStatus::Missing || rateHzStatus == ArgumentStatus::Missing) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (sliceIdStatus == ArgumentStatus::NotRepresentable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   notRepresentableReason(), {});
        return;
    }
    if (rateHzStatus == ArgumentStatus::NotRepresentable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   notRepresentableReason(), {});
        return;
    }

    // Fix wave 3 (Important 1): the slice asked about, by identity, so a
    // slice closed and its id reused before the change runs (by any
    // device, the asker included) is never changed in its place.
    const QPointer<SliceModel> askedSlice(m_radioModel->sliceById(sliceId));
    if (askedSlice.isNull()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That receiver is no longer on the Core."), {});
        return;
    }

    // Deferred to a LATER turn of RadioModel's own event loop -- see the
    // class comment for why this is the one verb that cannot run inline.
    // self/radioModel are QPointer copies so the daemon tearing either one
    // down before this queued call runs leaves nothing dangling; the
    // by-value capture of verb/commandId/sliceId/rateHz keeps this
    // self-contained once dispatch() (and `invoke`, which is a reference to
    // a caller-owned temporary) has returned.
    const QByteArray verb = invoke.commandVerb;
    const quint32 commandId = invoke.commandId;
    // Fix wave I1: the result is this session's, whichever dispatch is
    // running when it arrives.
    const QString owner = m_sessionOwner;
    // Fix wave 2 (Important 4): the device this change acts for, and the
    // check that refused another device's slice here, run again when the
    // change is applied: the slice may have closed and its id gone to
    // another device's new slice in between.
    const QByteArray requester = m_requester;
    const SliceAccess access = m_sliceAccess;
    const QPointer<SessionCommandDispatcher> self(this);
    const QPointer<RadioModel> radioModel(m_radioModel);

    QMetaObject::invokeMethod(
        m_radioModel,
        [self, radioModel, verb, commandId, sliceId, rateHz, owner, closingOwners, close,
         changedReason, requester, access, askedSlice]() {
            if (self.isNull() || radioModel.isNull()) {
                return;
            }
            if (!requester.isEmpty() && access) {
                const QString refusal = access(requester, sliceId);
                if (!refusal.isEmpty()) {
                    self->emitResultAs(owner, SessionMessages::commandResult(
                        verb, commandId, false, refusal, {}));
                    return;
                }
            }
            // Fix wave 3 (Important 1): the id must still name the slice
            // asked about; a reuse by another device is refused above as
            // that device's, a reuse by the asker (or by nobody) here.
            if (askedSlice.isNull() || radioModel->sliceById(sliceId) != askedSlice.data()) {
                self->emitResultAs(owner, SessionMessages::commandResult(
                    verb, commandId, false,
                    QStringLiteral("That receiver is no longer on the Core."), {}));
                return;
            }
            // A confirmed change closes exactly the slices it was confirmed
            // for: each id must still name the same slice, with the owner
            // it had at the proceed, or the whole change is refused and
            // nothing closes (fix wave 3: identity, so an id reused by the
            // same owner, or by nobody, is caught too).
            QSet<int> closing;
            for (auto it = closingOwners.cbegin(); it != closingOwners.cend(); ++it) {
                SliceModel* const now = radioModel->sliceById(it.key());
                if (now == nullptr || it.value().slice.isNull() || now != it.value().slice.data()
                    || radioModel->sliceOwnership()->mark(it.key()).subject()
                           != it.value().owner) {
                    self->emitResultAs(owner, SessionMessages::commandResult(
                        verb, commandId, false,
                        changedReason.isEmpty()
                            ? QStringLiteral("That setting changed since you asked. "
                                             "Make the change again.")
                            : changedReason,
                        {}));
                    return;
                }
                closing.insert(it.key());
            }

            // Actual scope, not requested scope (see the class comment):
            // snapshot every slice's rate before, act, then report
            // whichever slices' rates actually differ afterward. Correct
            // regardless of WHY more than one moved -- co-hosted slices
            // sharing requestSliceSampleRate's target DDC stream, or (on a
            // Protocol 1 board) the request escalating all the way to
            // RadioModel::setSampleRateLive's radio-wide sequence.
            QHash<int, int> before;
            for (SliceModel* slice : radioModel->slices()) {
                if (slice != nullptr) {
                    before.insert(slice->sliceIndex(), slice->sampleRateHz());
                }
            }

            // setStreamSampleRate (RadioModel.cpp) can refuse the retune
            // outright -- every slice stays exactly where it was -- and
            // reports that through sliceRetuneRejected with a human-
            // readable reason, the same pattern handleAddSlice's
            // sliceAddRejected capture uses. requestSliceSampleRate() is
            // synchronous, so the emission (if any) lands before it
            // returns and before this connection is torn down. This
            // by-reference capture is ALREADY running inside a queued
            // lambda on RadioModel's thread (see this method's own
            // deferral above), so it depends on the same same-thread
            // invariant as handleAddSlice's capture, one level further in.
            QString rejectionReason;
            const QMetaObject::Connection conn = connect(
                radioModel, &RadioModel::sliceRetuneRejected, self,
                [&rejectionReason](int, const QString& reason) { rejectionReason = reason; });
            radioModel->requestSliceSampleRateClosing(sliceId, rateHz, closing, close);
            QObject::disconnect(conn);

            if (!rejectionReason.isEmpty()) {
                self->emitResultAs(owner, SessionMessages::commandResult(
                                              verb, commandId, false, rejectionReason, {}));
                return;
            }

            QList<QByteArray> affected;
            for (SliceModel* slice : radioModel->slices()) {
                if (slice == nullptr) {
                    continue;
                }
                const int id = slice->sliceIndex();
                if (before.value(id, -1) != slice->sampleRateHz()) {
                    affected.append(ObjectRegistry::keyForSlice(id));
                }
            }
            // An empty `affected` here is a legitimate no-op (the slice was
            // not yet bound to a stream, or was already at this rate --
            // requestSliceSampleRate()'s own idempotent-check paths,
            // RadioModel.cpp), not a failure: RadioModel raised no
            // rejection, so nothing here second-guesses that.
            self->emitResultAs(owner, SessionMessages::commandResult(verb, commandId, true,
                                                                     QString(), affected));
        },
        Qt::QueuedConnection);
}

// ── setActiveSliceById ───────────────────────────────────────────────────

// Fix round 1 review finding (Important 1): before this verb existed, a
// remote operator's active-slice click had no path to the daemon at all.
// SliceModel::active carries no WRITE (SliceModel.h), so StateMirror::
// applyInbound() always fell through to applyMirroredValue(), which
// refused it outright (SliceModel.cpp) -- both inbound doors were shut.
// This verb is the one that was missing.
//
// Mechanically identical to handleRemoveSlice above: resolve the id,
// check the RadioModel entry point's own success/failure signal (here a
// bool return rather than an existence probe plus a separate guard), and
// report the resulting scope. RadioModel::setActiveSliceById() already
// does its own "no such slice" check internally (sliceById(sliceId) ==
// nullptr) and returns false, so this handler does not duplicate it --
// unlike handleRemoveSlice, which has a SECOND rejection RadioModel
// signals nothing about (the last-slice guard) and therefore does have to
// duplicate.
void SessionCommandDispatcher::handleSetActiveSliceById(const SessionMessage& invoke)
{
    int sliceId = 0;
    switch (findIntArgument(invoke.arguments, "sliceId", &sliceId)) {
    case ArgumentStatus::Missing:
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    case ArgumentStatus::NotRepresentable:
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   notRepresentableReason(), {});
        return;
    case ArgumentStatus::Ok:
        break;
    }

    // Captured BEFORE the call: this is the slice that is ABOUT to stop
    // being active, and setActiveSliceById() (RadioModel.cpp) reassigns
    // m_activeSlice as its very first side effect on success, so reading
    // this afterward would already show the NEW slice.
    //
    // iPhone app Task 73 (ruling 5.10): with a requesting device, its own
    // active slice among its own, which is the one that stops being active.
    SliceModel* const previouslyActive = m_radioModel->activeSlice();
    const int previouslyActiveId = !m_requester.isEmpty()
        ? m_radioModel->sliceOwnership()->activeFor(m_requester)
        : ((previouslyActive != nullptr) ? previouslyActive->sliceIndex() : -1);

    const bool activated = !m_requester.isEmpty()
        ? m_radioModel->setActiveSliceByIdFor(m_requester, sliceId)
        : m_radioModel->setActiveSliceById(sliceId);
    if (!activated) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That receiver is no longer on the Core."), {});
        return;
    }

    // The newly-active key, plus the previously-active one when it is a
    // DIFFERENT slice -- requesting the slice that was already active is a
    // legitimate no-op accept (RadioModel::setActiveSlice()'s own
    // change-guard makes it one), and reporting the same key twice would
    // not describe two objects moving, just one.
    QList<QByteArray> affected{ ObjectRegistry::keyForSlice(sliceId) };
    if (previouslyActiveId >= 0 && previouslyActiveId != sliceId) {
        affected.append(ObjectRegistry::keyForSlice(previouslyActiveId));
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), affected);
}

void SessionCommandDispatcher::handleRequestStreamCtunPinned(const SessionMessage& invoke)
{
    int sliceId = 0;
    QVariant pinned;
    if (!hasExactlyArguments(invoke.arguments, { "sliceId", "pinned" })
        || findIntArgument(invoke.arguments, "sliceId", &sliceId) != ArgumentStatus::Ok
        || !findArgument(invoke.arguments, "pinned", &pinned)
        || pinned.typeId() != QMetaType::Bool) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (!m_radioModel->requestStreamCtunPinned(sliceId, pinned.toBool())) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That receiver is not running on the Core."), {});
        return;
    }
    QList<QByteArray> affected;
    if (SliceModel* slice = m_radioModel->sliceById(sliceId)) {
        for (int id : m_radioModel->slicesOnStream(slice->streamIndex())) {
            affected.append(ObjectRegistry::keyForSlice(id));
        }
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), affected);
}

void SessionCommandDispatcher::handleRequestStreamCentre(const SessionMessage& invoke)
{
    int sliceId = 0;
    double centreHz = 0.0;
    if (!hasExactlyArguments(invoke.arguments, { "sliceId", "centreHz" })
        || findIntArgument(invoke.arguments, "sliceId", &sliceId) != ArgumentStatus::Ok
        || !findFiniteDoubleArgument(invoke.arguments, "centreHz", &centreHz)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    SliceModel* const slice = m_radioModel->sliceById(sliceId);
    const int stream = slice ? slice->streamIndex() : -1;
    if (!m_radioModel->requestStreamCentre(sliceId, centreHz)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("C-Tune cannot centre there while other receivers share this spectrum."), {});
        return;
    }
    QList<QByteArray> affected;
    for (int id : m_radioModel->slicesOnStream(stream)) {
        affected.append(ObjectRegistry::keyForSlice(id));
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), affected);
}

void SessionCommandDispatcher::handleConfigureTgxl(const SessionMessage& invoke)
{
    QString host;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "host", "port" })
        || !findUtf8Argument(invoke.arguments, "host", &host)
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok
        || port < 1 || port > 65535) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }

    QString reason;
    if (!m_radioModel->configureTgxlForStation(host, static_cast<quint16>(port), &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not set up the Tuner Genius XL.") : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

void SessionCommandDispatcher::handleDisconnectTgxl(const SessionMessage& invoke)
{
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }

    QString reason;
    if (!m_radioModel->disconnectTgxlForStation(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not disconnect the Tuner Genius XL.") : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-47 / R-R3-22: the Power Genius's address, as configureTgxl's.
// Accepted means saved and identifying; `amplifier`.connectionPhase says
// whether it connected.
void SessionCommandDispatcher::handleConfigurePgxl(const SessionMessage& invoke)
{
    QString host;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "host", "port" })
        || !findUtf8Argument(invoke.arguments, "host", &host)
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok
        || port < 1 || port > 65535) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->configurePgxlForStation(host, static_cast<quint16>(port), &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not set up the Power Genius.") : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-47 / R-R3-22: the RF-Kit's configure rule, as the Power Genius's.
void SessionCommandDispatcher::handleConfigureRfKit(const SessionMessage& invoke)
{
    QString host;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "host", "port" })
        || !findUtf8Argument(invoke.arguments, "host", &host)
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok
        || port < 1 || port > 65535) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->configureRfKitForStation(host, static_cast<quint16>(port), &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not set up the RF-Kit amplifier.")
                                    : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

void SessionCommandDispatcher::handleDisconnectRfKit(const SessionMessage& invoke)
{
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The disconnect request for the RF-Kit amplifier was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->disconnectRfKitForStation(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not disconnect the RF-Kit "
                                                     "amplifier.")
                                    : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// I4 (R-R3-47, remoteRfKitControlVersion 3): the local page's Reset amp
// error, sent by the Core to its admitted amp.
void SessionCommandDispatcher::handleResetRfKitError(const SessionMessage& invoke)
{
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to reset the RF-Kit amplifier's error was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->resetRfKitErrorForStation(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not reset the RF-Kit "
                                                     "amplifier's error.")
                                    : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 10, remoteRfKitControlVersion 4): the RF-Kit's
// OPERATE or STANDBY, sent to the Core's admitted amp as the local applet's
// request. Refused while the radio is on the air or the Core is not
// connected to the amp; nothing is sent then.
void SessionCommandDispatcher::handleSetRfKitOperate(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    QVariant on;
    if (!hasExactlyArguments(invoke.arguments, { "on" })
        || !findArgument(invoke.arguments, "on", &on) || on.typeId() != QMetaType::Bool) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to put the RF-Kit amplifier in operate or "
                                  "standby was not understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setRfKitOperateForStation(on.toBool(), &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not switch the RF-Kit "
                                                     "amplifier.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 10): the applet's ANT 1 to 4, an internal antenna.
void SessionCommandDispatcher::handleSetRfKitAntenna(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "port" })
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to switch the RF-Kit amplifier's antenna was "
                                  "not understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setRfKitAntennaForStation(port, &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not switch the RF-Kit "
                                                     "amplifier's antenna.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 10): the RF-Kit page's "Set amp to TCI mode".
void SessionCommandDispatcher::handleSetRfKitTciMode(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to put the RF-Kit amplifier in TCI mode was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setRfKitTciModeForStation(&reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not put the RF-Kit amplifier "
                                                     "in TCI mode.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 10): the RF-Kit page's Host and Port, saved on the
// Core for its radio without dialling.
void SessionCommandDispatcher::handleSetRfKitAddress(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    QString host;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "host", "port" })
        || !findUtf8Argument(invoke.arguments, "host", &host)
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to save the RF-Kit amplifier address was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setRfKitAddressForStation(host, port, &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not save the RF-Kit "
                                                     "amplifier address.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

void SessionCommandDispatcher::handleSetRfKitEnabled(const SessionMessage& invoke)
{
    QVariant enabled;
    if (!hasExactlyArguments(invoke.arguments, { "enabled" })
        || !findArgument(invoke.arguments, "enabled", &enabled)
        || enabled.typeId() != QMetaType::Bool) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to turn the RF-Kit amplifier on or off was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setRfKitEnabledForStation(enabled.toBool(), &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change its RF-Kit "
                                                     "amplifier switch.")
                                    : reason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// iPhone app Task 71 (R-IOS-02, ruling 4.12, sessionHolderVersion 1): the
// device leaves the Core on purpose. StationServer answers a peer without
// sessionHolderVersion 1 before this runs; here the request is checked, the
// result goes out, and StationServer frees the place and ends the
// connection.
void SessionCommandDispatcher::handleSessionLeave(const SessionMessage& invoke)
{
    if (!invoke.arguments.isEmpty()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to leave the Core was not understood."), {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
    emit sessionLeaveRequested();
}

// iPhone app Task 74 (R-IOS-30): confirm.proceed {id, choice},
// confirm.cancel {id}, notice.takeBack {id}. The arguments are read here;
// what they do is the Core's confirm step (StationServer).
void SessionCommandDispatcher::setSliceAccessController(SliceAccessController* controller)
{
    m_sliceAccessController = controller;
}

// Slice control plan Task 4 (sliceAccessVersion 1): the slice by its id and
// incarnation, take and release with the control revision the device saw.
// StationServer refuses these to a device that did not declare
// sliceAccess before they reach here.
void SessionCommandDispatcher::handleSliceAccessVerb(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    const bool withRevision = verb == "slice.takeControl" || verb == "slice.release";
    const auto exactUnsigned = [&invoke](const QByteArray& name, quint64* value) {
        for (const MirrorUpdate& argument : invoke.arguments) {
            if (argument.name != name) {
                continue;
            }
            if (argument.kind != MirrorWireKind::Int64
                || argument.value.typeId() != QMetaType::LongLong
                || argument.value.toLongLong() < 0) {
                return false;
            }
            *value = static_cast<quint64>(argument.value.toLongLong());
            return true;
        }
        return false;
    };
    quint64 sliceId = 0;
    SliceOwnership::SliceRef ref;
    quint64 revision = 0;
    const bool shape = withRevision
        ? hasExactlyArguments(invoke.arguments, {"sliceId", "incarnation", "controlRevision"})
        : hasExactlyArguments(invoke.arguments, {"sliceId", "incarnation"});
    if (!shape || !exactUnsigned("sliceId", &sliceId) || sliceId > 63
        || !exactUnsigned("incarnation", &ref.incarnation)
        || (withRevision && !exactUnsigned("controlRevision", &revision))) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    ref.sliceId = static_cast<int>(sliceId);
    if (m_sliceAccessController.isNull() || m_requester.isEmpty()) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("This Core cannot share slices between devices."), {});
        return;
    }
    SliceAccessController::Result result;
    if (verb == "slice.listen") {
        result = m_sliceAccessController->listen(m_requester, ref);
    } else if (verb == "slice.stopListening") {
        result = m_sliceAccessController->stopListening(m_requester, ref);
    } else if (verb == "slice.takeControl") {
        result = m_sliceAccessController->takeControl(m_requester, ref, revision);
    } else {
        result = m_sliceAccessController->release(m_requester, ref, revision);
    }
    QList<MirrorUpdate> values;
    if (result.accepted && (verb == "slice.listen" || verb == "slice.takeControl")) {
        values.append(MirrorUpdate{0, QByteArrayLiteral("controlRevision"), MirrorWireKind::Int64,
                                   QVariant(static_cast<qlonglong>(result.controlRevision))});
    }
    emit commandResultReady(SessionMessages::commandResult(
        verb, invoke.commandId, result.accepted, result.reason,
        result.accepted ? result.affected : QList<QByteArray>{}, values));
}

// Slice control plan Task 6 (sliceAccessVersion 1): a listener's own level
// and mute for one slice. The level is 0..1 and is applied in the Core's
// mixer to this device's audio only (SliceAccessController::setListenLevel).
void SessionCommandDispatcher::handleSliceListenLevel(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    const auto refuseUnread = [this, &invoke, &verb] {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
    };
    if (!hasExactlyArguments(invoke.arguments, {"sliceId", "incarnation", "level", "muted"})) {
        refuseUnread();
        return;
    }
    qint64 sliceId = -1;
    qint64 incarnation = -1;
    double level = -1.0;
    bool muted = false;
    bool haveLevel = false;
    bool haveMuted = false;
    for (const MirrorUpdate& argument : invoke.arguments) {
        if (argument.name == "sliceId" || argument.name == "incarnation") {
            if (argument.kind != MirrorWireKind::Int64
                || argument.value.typeId() != QMetaType::LongLong) {
                refuseUnread();
                return;
            }
            (argument.name == "sliceId" ? sliceId : incarnation) = argument.value.toLongLong();
        } else if (argument.name == "level") {
            if (argument.kind != MirrorWireKind::Float64
                || argument.value.typeId() != QMetaType::Double) {
                refuseUnread();
                return;
            }
            level = argument.value.toDouble();
            haveLevel = true;
        } else if (argument.name == "muted") {
            if (argument.kind != MirrorWireKind::Bool
                || argument.value.typeId() != QMetaType::Bool) {
                refuseUnread();
                return;
            }
            muted = argument.value.toBool();
            haveMuted = true;
        }
    }
    if (!haveLevel || !haveMuted || sliceId < 0 || sliceId > 63 || incarnation < 0
        || !std::isfinite(level) || level < 0.0 || level > 1.0) {
        refuseUnread();
        return;
    }
    if (m_sliceAccessController.isNull() || m_requester.isEmpty()) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("This Core cannot share slices between devices."), {});
        return;
    }
    SliceOwnership::SliceRef ref;
    ref.sliceId = static_cast<int>(sliceId);
    ref.incarnation = static_cast<quint64>(incarnation);
    const SliceAccessController::Result result =
        m_sliceAccessController->setListenLevel(m_requester, ref, level, muted);
    emit commandResultReady(SessionMessages::commandResult(
        verb, invoke.commandId, result.accepted, result.reason,
        result.accepted ? result.affected : QList<QByteArray>{}, {}));
}

void SessionCommandDispatcher::handleConfirmAnswer(const SessionMessage& invoke)
{
    const bool proceed = invoke.commandVerb == "confirm.proceed";
    int id = 0;
    int choice = -1;
    const bool shape = proceed ? hasExactlyArguments(invoke.arguments, {"id", "choice"})
                               : hasExactlyArguments(invoke.arguments, {"id"});
    if (!shape || findIntArgument(invoke.arguments, "id", &id) != ArgumentStatus::Ok
        || (proceed && findIntArgument(invoke.arguments, "choice", &choice) != ArgumentStatus::Ok)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (!m_confirmAnswer) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("That question is no longer open. Make the change again."), {});
        return;
    }
    emit commandResultReady(m_confirmAnswer(invoke, id, choice));
}

// R-R3-48: the one TCI switch and port, kept by the Core.
// iPhone app Task 13 (R-IOS-08, deviceAdminVersion 1). The facade answers in
// plain words; the connection a revoke or a token retirement ends is
// StationServer's to end, after this result has gone out.
void SessionCommandDispatcher::handleDeviceAdmin(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    if (m_deviceAdmin.isNull()) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("This Core cannot manage its paired devices."), {});
        return;
    }
    DeviceAdminResult result;
    if (verb == "devices.revoke" || verb == "station.rename") {
        const QByteArray name = verb == "devices.revoke" ? QByteArrayLiteral("id")
                                                          : QByteArrayLiteral("label");
        QVariant value;
        if (!hasExactlyArguments(invoke.arguments, {name})
            || !hasWireKind(invoke.arguments, name, MirrorWireKind::Utf8)
            || !findArgument(invoke.arguments, name, &value)
            || value.typeId() != QMetaType::QString) {
            if (verb == "devices.revoke") {
                emitResult(verb, invoke.commandId, false,
                           QStringLiteral("The request to remove a device was not understood."),
                           {});
            } else {
                emitResult(verb, invoke.commandId, false,
                           QStringLiteral("The request to rename the Core was not understood."),
                           {});
            }
            return;
        }
        result = verb == "devices.revoke" ? m_deviceAdmin->revoke(value.toString())
                                          : m_deviceAdmin->rename(value.toString());
    } else {
        if (!invoke.arguments.isEmpty()) {
            if (verb == "station.retireToken") {
                emitResult(verb, invoke.commandId, false,
                           QStringLiteral("The request to stop accepting the pairing token "
                                          "was not understood."),
                           {});
            } else {
                emitResult(verb, invoke.commandId, false,
                           QStringLiteral("The request to confirm the key backup was not "
                                          "understood."),
                           {});
            }
            return;
        }
        result = verb == "station.retireToken" ? m_deviceAdmin->retireToken()
                                               : m_deviceAdmin->acknowledgeKeyBackup();
    }
    emitResult(verb, invoke.commandId, result.accepted, result.reason,
               result.accepted ? QList<QByteArray>{"devices"} : QList<QByteArray>{});
}

// iPhone app Task 14 (R-IOS-08, pairingVersion 1). pairing.open answers
// with the window's code in `code` ("" while no code is shown);
// StationServer blanks it for any connection not signed in with a paired
// device's key before the result leaves the Core.
void SessionCommandDispatcher::handlePairingWindow(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    const bool open = verb == "pairing.open";
    if (m_deviceAdmin.isNull()) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("This Core cannot pair new devices."), {});
        return;
    }
    if (!invoke.arguments.isEmpty()) {
        emitResult(verb, invoke.commandId, false,
                   open ? QStringLiteral("The request to open pairing was not understood.")
                        : QStringLiteral("The request to close pairing was not understood."),
                   {});
        return;
    }
    const DeviceAdminResult result =
        open ? m_deviceAdmin->openPairing() : m_deviceAdmin->closePairing();
    if (!result.accepted || !open) {
        emitResult(verb, invoke.commandId, result.accepted, result.reason,
                   result.accepted ? QList<QByteArray>{"devices"} : QList<QByteArray>{});
        return;
    }
    emit commandResultReady(SessionMessages::commandResult(
        verb, invoke.commandId, true, QString(), {"devices"},
        {MirrorUpdate{0, QByteArrayLiteral("code"), MirrorWireKind::Utf8,
                      QVariant(m_deviceAdmin->pairingCode())}}));
}

void SessionCommandDispatcher::handleSetStationTci(const SessionMessage& invoke)
{
    QVariant enabled;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "enabled", "port" })
        || !findArgument(invoke.arguments, "enabled", &enabled)
        || enabled.typeId() != QMetaType::Bool
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to turn the Core's TCI server on or off was "
                                  "not understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setStationTciForStation(enabled.toBool(), port, &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change its TCI server.")
                                    : reason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// Parity Task 23 (R-R3-48, R-R3-42, stationTciVersion 2): the station TCI
// server's four options and closing one of its apps. Each is refused while
// the Core's radio is on the air (the parity plan's rule); nothing changes
// then.
void SessionCommandDispatcher::handleStationTciServer(const SessionMessage& invoke)
{
    const bool options = invoke.commandVerb == "setStationTciOptions";
    QVariant expert;
    QVariant sunSdr;
    QVariant cwlu;
    QVariant initial;
    QVariant id;
    const bool readable = options
        ? (hasExactlyArguments(invoke.arguments, {"emulateExpertSdr3", "emulateSunSdr2Pro",
                                                  "cwluBecomesCw", "sendInitialState"})
           && findArgument(invoke.arguments, "emulateExpertSdr3", &expert)
           && expert.typeId() == QMetaType::Bool
           && findArgument(invoke.arguments, "emulateSunSdr2Pro", &sunSdr)
           && sunSdr.typeId() == QMetaType::Bool
           && findArgument(invoke.arguments, "cwluBecomesCw", &cwlu)
           && cwlu.typeId() == QMetaType::Bool
           && findArgument(invoke.arguments, "sendInitialState", &initial)
           && initial.typeId() == QMetaType::Bool)
        : (hasExactlyArguments(invoke.arguments, {"id"})
           && findArgument(invoke.arguments, "id", &id) && id.typeId() == QMetaType::QString);
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   options ? QStringLiteral("The request to change the Core's TCI server "
                                            "settings was not understood.")
                           : QStringLiteral("The request to disconnect an app from the "
                                            "Core's TCI server was not understood."),
                   {});
        return;
    }
    QString reason;
    if (m_radioModel->stationOnAirRefusal(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
        return;
    }
    const bool accepted = options
        ? m_radioModel->setStationTciOptionsForStation(expert.toBool(), sunSdr.toBool(),
                                                       cwlu.toBool(), initial.toBool(), &reason)
        : m_radioModel->disconnectStationTciClientForStation(id.toString(), &reason);
    emitResult(invoke.commandVerb, invoke.commandId, accepted, accepted ? QString() : reason, {});
}

// JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the rest of the
// Core's TCI server settings, one or more at once, each of its own kind
// (a whole number or on/off) and held to its range by the controller.
// Refused while the Core's radio is on the air; nothing changes then.
void SessionCommandDispatcher::handleSetStationTciSettings(const SessionMessage& invoke)
{
    QVariantMap changes;
    bool readable = !invoke.arguments.isEmpty();
    for (const MirrorUpdate& argument : invoke.arguments) {
        const StationTciModel::Setting* setting = StationTciModel::setting(argument.name);
        const bool boolKind = setting != nullptr
            && setting->kind == StationTciModel::Setting::Kind::Bool;
        if (setting == nullptr || changes.contains(QString::fromUtf8(argument.name))
            || (boolKind ? argument.value.typeId() != QMetaType::Bool
                         : argument.kind != MirrorWireKind::Int64)) {
            readable = false;
            break;
        }
        changes.insert(QString::fromUtf8(argument.name),
                       boolKind ? QVariant(argument.value.toBool())
                                : QVariant(int(std::clamp<qlonglong>(
                                      argument.value.toLongLong(),
                                      std::numeric_limits<int>::min(),
                                      std::numeric_limits<int>::max()))));
    }
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to change the Core's TCI server settings was "
                                  "not understood."), {});
        return;
    }
    QString reason;
    if (m_radioModel->stationOnAirRefusal(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
        return;
    }
    const bool accepted = m_radioModel->setStationTciSettingsForStation(changes, &reason);
    emitResult(invoke.commandVerb, invoke.commandId, accepted, accepted ? QString() : reason, {});
}

// R-R3-47 / R-R3-22 (accessoryDataVersion 1): the transmit interlock policy.
// The Core applies it and mirrors it back on `accessoryData`; its
// enforcement stays on the Core and a change keys nothing.
void SessionCommandDispatcher::handleSetTxInterlockPolicy(const SessionMessage& invoke)
{
    int mode = -1;
    int graceMs = -1;
    QVariant gate;
    double gateMax = 0.0;
    if (!hasExactlyArguments(invoke.arguments,
                             { "mode", "graceMs", "swrGateEnabled", "swrGateMax" })
        || !hasWireKind(invoke.arguments, "mode", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "mode", &mode) != ArgumentStatus::Ok
        || !hasWireKind(invoke.arguments, "graceMs", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "graceMs", &graceMs) != ArgumentStatus::Ok
        || !findArgument(invoke.arguments, "swrGateEnabled", &gate)
        || gate.typeId() != QMetaType::Bool
        || !hasWireKind(invoke.arguments, "swrGateMax", MirrorWireKind::Float64)
        || !findFiniteDoubleArgument(invoke.arguments, "swrGateMax", &gateMax)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to change the transmit interlock was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setTxInterlockPolicyForStation(mode, graceMs, gate.toBool(), gateMax,
                                                      &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change the transmit "
                                                     "interlock.")
                                    : reason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-47 / R-R3-22: the Power Genius output limit that raises the alert.
void SessionCommandDispatcher::handleSetPgxlPowerCap(const SessionMessage& invoke)
{
    QVariant enabled;
    int watts = 0;
    if (!hasExactlyArguments(invoke.arguments, { "enabled", "watts" })
        || !findArgument(invoke.arguments, "enabled", &enabled)
        || enabled.typeId() != QMetaType::Bool
        || !hasWireKind(invoke.arguments, "watts", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "watts", &watts) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to change the Power Genius output limit was "
                                  "not understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setPgxlPowerCapForStation(enabled.toBool(), watts, &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change the Power Genius "
                                                     "output limit.")
                                    : reason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-47 / R-R3-22: one device's fault history, cleared on the Core.
void SessionCommandDispatcher::handleClearAccessoryFaults(const SessionMessage& invoke)
{
    QString device;
    if (!hasExactlyArguments(invoke.arguments, { "device" })
        || !findUtf8Argument(invoke.arguments, "device", &device)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to clear the fault history was not understood."),
                   {});
        return;
    }
    QString reason;
    if (!m_radioModel->clearAccessoryFaultsForStation(device, &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not clear the fault history.")
                                    : reason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-47 / R-R3-22 (remotePgxlControlVersion 3, remoteTgxlControlVersion
// 1): the amp's and the tuner's own settings. The Core sends each to the
// device as the local Advanced page's own command (StationDeviceSettings);
// accepted means it left for the device, and the device's answer comes
// back on `accessorySettings`. None keys a transmitter or operates the amp.
void SessionCommandDispatcher::handleAccessoryDeviceSettings(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    const bool pgxl = verb.contains("Pgxl");
    const QString device = pgxl ? QStringLiteral("Power Genius") : QStringLiteral("Tuner Genius");
    const auto notUnderstood = [&](const QString& what) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to %1 was not understood.").arg(what), {});
    };
    QString reason;
    bool sent = false;
    if (verb == "setPgxlName" || verb == "setTgxlName") {
        QString name;
        if (!hasExactlyArguments(invoke.arguments, { "name" })
            || !findUtf8Argument(invoke.arguments, "name", &name)) {
            notUnderstood(QStringLiteral("rename the %1").arg(device));
            return;
        }
        sent = pgxl ? m_radioModel->setPgxlNameForStation(name, &reason)
                    : m_radioModel->setTgxlNameForStation(name, &reason);
    } else if (verb == "setPgxlHardware") {
        // Exactly one of biasMode (utf8), fanMode (utf8), ledIntensity (i64).
        const MirrorUpdate* only = invoke.arguments.size() == 1 ? &invoke.arguments.first()
                                                                : nullptr;
        QVariant value;
        bool shape = false;
        if (only && (only->name == "biasMode" || only->name == "fanMode")) {
            QString text;
            shape = findUtf8Argument(invoke.arguments, only->name, &text);
            value = text;
        } else if (only && only->name == "ledIntensity") {
            int led = 0;
            shape = hasWireKind(invoke.arguments, "ledIntensity", MirrorWireKind::Int64)
                && findIntArgument(invoke.arguments, "ledIntensity", &led) == ArgumentStatus::Ok;
            value = led;
        }
        if (!shape) {
            notUnderstood(QStringLiteral("change the Power Genius hardware"));
            return;
        }
        sent = m_radioModel->setPgxlHardwareForStation(QString::fromUtf8(only->name), value,
                                                       &reason);
    } else if (verb == "setPgxlNetwork" || verb == "setTgxlNetwork") {
        QVariant dhcp;
        QString address;
        QString netmask;
        QString gateway;
        if (!hasExactlyArguments(invoke.arguments, { "dhcp", "address", "netmask", "gateway" })
            || !findArgument(invoke.arguments, "dhcp", &dhcp)
            || dhcp.typeId() != QMetaType::Bool
            || !findUtf8Argument(invoke.arguments, "address", &address)
            || !findUtf8Argument(invoke.arguments, "netmask", &netmask)
            || !findUtf8Argument(invoke.arguments, "gateway", &gateway)) {
            notUnderstood(QStringLiteral("change the %1 network settings").arg(device));
            return;
        }
        sent = pgxl ? m_radioModel->setPgxlNetworkForStation(dhcp.toBool(), address, netmask,
                                                             gateway, &reason)
                    : m_radioModel->setTgxlNetworkForStation(dhcp.toBool(), address, netmask,
                                                             gateway, &reason);
    } else if (verb == "savePgxlSettings" || verb == "saveTgxlSettings") {
        if (!hasExactlyArguments(invoke.arguments, {})) {
            notUnderstood(QStringLiteral("save and restart the %1").arg(device));
            return;
        }
        sent = pgxl ? m_radioModel->savePgxlSettingsForStation(&reason)
                    : m_radioModel->saveTgxlSettingsForStation(&reason);
    } else {
        if (!hasExactlyArguments(invoke.arguments, {})) {
            notUnderstood(QStringLiteral("read the %1 settings").arg(device));
            return;
        }
        sent = pgxl ? m_radioModel->readPgxlSettingsForStation(&reason)
                    : m_radioModel->readTgxlSettingsForStation(&reason);
    }
    if (!sent) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty()
                       ? QStringLiteral("The Core did not send the request to the %1.").arg(device)
                       : reason,
                   {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): the Tuner Genius's
// antenna (port 1 to 3), operate and bypass, applied through the Core's own
// TunerModel. Accepted means the command left for the tuner; the tuner's
// report comes back on the `tuner` object. Refused while the radio is on
// the air, with no tuner admitted, and (antenna) with no antenna switch or
// a port outside 1 to 3; nothing is sent then.
void SessionCommandDispatcher::handleTgxlControl(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    QString reason;
    bool sent = false;
    if (verb == "setTgxlAntenna") {
        int port = 0;
        if (!hasExactlyArguments(invoke.arguments, { "port" })
            || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
            || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok) {
            emitResult(verb, invoke.commandId, false,
                       QStringLiteral("The request to switch the Tuner Genius antenna was not "
                                      "understood."), {});
            return;
        }
        sent = m_radioModel->setTgxlAntennaForStation(port, &reason);
    } else {
        const bool operate = verb == "setTgxlOperate";
        QVariant on;
        if (!hasExactlyArguments(invoke.arguments, { "on" })
            || !findArgument(invoke.arguments, "on", &on) || on.typeId() != QMetaType::Bool) {
            emitResult(verb, invoke.commandId, false,
                       operate ? QStringLiteral("The request to put the Tuner Genius in operate "
                                                "or standby was not understood.")
                               : QStringLiteral("The request to bypass the Tuner Genius was not "
                                                "understood."), {});
            return;
        }
        sent = operate ? m_radioModel->setTgxlOperateForStation(on.toBool(), &reason)
                       : m_radioModel->setTgxlBypassForStation(on.toBool(), &reason);
    }
    if (!sent) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not switch the Tuner Genius.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): a relay nudge
// (`relay` 0 C1, 1 L, 2 C2; `direction` -1 or 1), through the Core's own
// TunerModel. Refused as the switches are; nothing is sent then.
void SessionCommandDispatcher::handleMoveTgxlRelay(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    int relay = 0;
    int direction = 0;
    if (!hasExactlyArguments(invoke.arguments, { "relay", "direction" })
        || !hasWireKind(invoke.arguments, "relay", MirrorWireKind::Int64)
        || !hasWireKind(invoke.arguments, "direction", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "relay", &relay) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "direction", &direction) != ArgumentStatus::Ok) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to move a Tuner Genius relay was not understood."),
                   {});
        return;
    }
    QString reason;
    if (!m_radioModel->moveTgxlRelayForStation(relay, direction, &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not switch the Tuner Genius.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 8): the Core listens for Tuner Genius announcements
// and answers once its window ends, with `values` devicesJson (utf8, a JSON
// array of {"address","port","model","serial","nickname"}). An answer due
// to an earlier session is dropped.
void SessionCommandDispatcher::handleScanTgxlLan(const SessionMessage& invoke)
{
    const QByteArray verb = invoke.commandVerb;
    const quint32 commandId = invoke.commandId;
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(verb, commandId, false,
                   QStringLiteral("The request to scan for a Tuner Genius was not understood."),
                   {});
        return;
    }
    QString reason;
    const QPointer<SessionCommandDispatcher> self(this);
    const quint64 generation = m_sessionGeneration;
    // Checkpoint join (R-IOS-02): the answer comes on a later turn, so it
    // is named for the session that asked (emitResultAs), not whichever
    // session is being dispatched then.
    const QString owner = m_sessionOwner;
    const bool started = m_radioModel->scanTgxlLanForStation(
        [self, generation, owner, verb, commandId](const QString& devicesJson) {
            if (!self || self->m_sessionGeneration != generation) { return; }
            self->emitResultAs(owner, SessionMessages::commandResult(
                verb, commandId, true, QString(), {},
                {{0, "devicesJson", MirrorWireKind::Utf8, devicesJson}}));
        },
        &reason);
    if (!started) {
        emitResult(verb, commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not scan for a Tuner Genius.")
                                    : reason, {});
    }
}

// R-R3-49 (parity Task 8): the Peripherals row's Host and Port, saved on
// the Core for its radio without dialling (configureTgxl's address rules).
void SessionCommandDispatcher::handleSetTgxlAddress(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    QString host;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "host", "port" })
        || !findUtf8Argument(invoke.arguments, "host", &host)
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to save the Tuner Genius address was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setTgxlAddressForStation(host, port, &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty()
                       ? QStringLiteral("The Core did not save the Tuner Genius address.")
                       : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 9, remotePgxlControlVersion 4): the Power Genius's
// OPERATE or STANDBY, sent through the Core's own PgxlConnection as the
// local applet's line. Refused while the radio is on the air or the Core is
// not connected to the amp; nothing is sent then.
void SessionCommandDispatcher::handleSetPgxlOperate(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    QVariant on;
    if (!hasExactlyArguments(invoke.arguments, { "on" })
        || !findArgument(invoke.arguments, "on", &on) || on.typeId() != QMetaType::Bool) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to put the Power Genius in operate or standby "
                                  "was not understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setPgxlOperateForStation(on.toBool(), &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not switch the Power Genius.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 9): the Core listens for Power Genius announcements
// and answers once its window ends, as scanTgxlLan does.
void SessionCommandDispatcher::handleScanPgxlLan(const SessionMessage& invoke)
{
    const QByteArray verb = invoke.commandVerb;
    const quint32 commandId = invoke.commandId;
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(verb, commandId, false,
                   QStringLiteral("The request to scan for a Power Genius was not understood."),
                   {});
        return;
    }
    QString reason;
    const QPointer<SessionCommandDispatcher> self(this);
    const quint64 generation = m_sessionGeneration;
    // Checkpoint join (R-IOS-02): the answer comes on a later turn, so it
    // is named for the session that asked (emitResultAs), not whichever
    // session is being dispatched then.
    const QString owner = m_sessionOwner;
    const bool started = m_radioModel->scanPgxlLanForStation(
        [self, generation, owner, verb, commandId](const QString& devicesJson) {
            if (!self || self->m_sessionGeneration != generation) { return; }
            self->emitResultAs(owner, SessionMessages::commandResult(
                verb, commandId, true, QString(), {},
                {{0, "devicesJson", MirrorWireKind::Utf8, devicesJson}}));
        },
        &reason);
    if (!started) {
        emitResult(verb, commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not scan for a Power Genius.")
                                    : reason, {});
    }
}

// R-R3-49 (parity Task 9): the Peripherals row's Power Genius Host and
// Port, saved on the Core for its radio without dialling.
void SessionCommandDispatcher::handleSetPgxlAddress(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    QString host;
    int port = 0;
    if (!hasExactlyArguments(invoke.arguments, { "host", "port" })
        || !findUtf8Argument(invoke.arguments, "host", &host)
        || !hasWireKind(invoke.arguments, "port", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "port", &port) != ArgumentStatus::Ok) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to save the Power Genius address was not "
                                  "understood."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setPgxlAddressForStation(host, port, &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty()
                       ? QStringLiteral("The Core did not save the Power Genius address.")
                       : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 2, transmitSettingsVersion 2): the TX applet's Tune
// Power slider. The Core sets its transmit band's tune power and the tune
// drive source to the tune slider, as the local slider does; the window
// sees both on the `transmit` object. Refused while the radio is on the air
// and outside the tune power range; nothing changes then. Keys nothing.
void SessionCommandDispatcher::handleTunePowerForTxBand(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    int watts = 0;
    if (!hasExactlyArguments(invoke.arguments, { "watts" })
        || !hasWireKind(invoke.arguments, "watts", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "watts", &watts) != ArgumentStatus::Ok) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request to change the tune power was not understood."), {});
        return;
    }
    // Ruling 7.7: the transmitter's own settings are the holder's while
    // transmit is held (as txProfile.select), on the air or not.
    if (refusedForTheHolder(verb, invoke.commandId)) {
        return;
    }
    QString reason;
    if (!m_radioModel->setTunePowerForTxBandForStation(watts, &reason, m_transmitSettingsOnAir)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change the tune power.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 3, transmitSettingsVersion 3): the TX profile
// combos and Setup > Audio > TX Profile, through the Core's own
// MicProfileManager as the local controls use it. The Core's active profile
// and list come back on `transmit`. Refused while the radio is on the air
// unless the peer may change the transmit settings (version 13); nothing
// changes then. Keys nothing.
void SessionCommandDispatcher::handleTxProfile(const SessionMessage& invoke)
{
    const QByteArray& verb = invoke.commandVerb;
    QString name;
    if (!hasExactlyArguments(invoke.arguments, { "name" })
        || !findUtf8Argument(invoke.arguments, "name", &name)) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request for the transmit profile was not understood."), {});
        return;
    }
    // iPhone app plan Task 77 (ruling 7.7): selecting, saving or deleting a
    // transmit profile is the holder's while transmit is held.
    if (refusedForTheHolder(verb, invoke.commandId)) {
        return;
    }
    QString reason;
    bool done = false;
    if (verb == "txProfile.select") {
        done = m_radioModel->selectTxProfileForStation(name, &reason, m_transmitSettingsOnAir);
    } else if (verb == "txProfile.save") {
        done = m_radioModel->saveTxProfileForStation(name, &reason, m_transmitSettingsOnAir);
    } else {
        done = m_radioModel->deleteTxProfileForStation(name, &reason, m_transmitSettingsOnAir);
    }
    if (!done) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change the transmit profile.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-IOS-13 / R-R3-49 (txEqCurveVersion 2): the TX EQ panel's curve and its
// Reset from an app. The write is the asking connection's, under every
// rule a txEqParaEqData write from it meets, so the station server applies
// it (TxEqCurveAccess). Without one (a dispatcher on its own) there is
// nothing to apply it through.
void SessionCommandDispatcher::handleTxEqCurve(const SessionMessage& invoke)
{
    if (m_txEqCurveAccess && m_txEqCurveAccess(invoke)) {
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, false,
               QStringLiteral("This Core cannot change the TX EQ curve from here. "
                              "Updating the Core may help."),
               {});
}

// transmitSettingsVersion 15: the CFC dialog's band editor from an app. The
// write is the asking connection's, under every rule a cfcParaEqData write
// from it meets, so the station server applies it (CfcProfileAccess).
// Without one (a dispatcher on its own) there is nothing to apply it
// through.
void SessionCommandDispatcher::handleCfcProfile(const SessionMessage& invoke)
{
    if (m_cfcProfileAccess && m_cfcProfileAccess(invoke)) {
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, false,
               QStringLiteral("This Core cannot change the CFC settings from here. "
                              "Updating the Core may help."),
               {});
}

// R-R3-49 / R-IOS-18 (paProfileVersion 1): PA Gain's profiles and table,
// through the Core's own PaProfileManager as the local page uses it
// (RadioModel::paProfileActionForStation). StationServer has already
// applied the gates the desktop's own PA profile writes meet. Keys nothing.
void SessionCommandDispatcher::handlePaProfile(const SessionMessage& invoke)
{
    using Action = RadioModel::PaProfileAction;
    const QByteArray& verb = invoke.commandVerb;
    RadioModel::PaProfileRequest request;
    bool understood = false;
    int band = -1;
    int step = -1;
    const auto bandOk = [&]() {
        return hasWireKind(invoke.arguments, "band", MirrorWireKind::Int64)
            && findIntArgument(invoke.arguments, "band", &band) == ArgumentStatus::Ok;
    };
    const auto valueOk = [&]() {
        return findFiniteDoubleArgument(invoke.arguments, "value", &request.value);
    };
    if (verb == "paProfile.select" || verb == "paProfile.new" || verb == "paProfile.copy"
        || verb == "paProfile.delete") {
        request.action = verb == "paProfile.select" ? Action::Select
            : verb == "paProfile.new" ? Action::New
            : verb == "paProfile.copy" ? Action::Copy : Action::Delete;
        understood = hasExactlyArguments(invoke.arguments, { "name" })
            && findUtf8Argument(invoke.arguments, "name", &request.name);
    } else if (verb == "paProfile.reset") {
        request.action = Action::Reset;
        understood = hasExactlyArguments(invoke.arguments, {});
    } else if (verb == "paProfile.setGain" || verb == "paProfile.setMaxPower") {
        request.action = verb == "paProfile.setGain" ? Action::SetGain : Action::SetMaxPower;
        understood = hasExactlyArguments(invoke.arguments, { "band", "value" }) && bandOk()
            && valueOk();
    } else if (verb == "paProfile.setAdjust") {
        request.action = Action::SetAdjust;
        understood = hasExactlyArguments(invoke.arguments, { "band", "step", "value" })
            && bandOk() && hasWireKind(invoke.arguments, "step", MirrorWireKind::Int64)
            && findIntArgument(invoke.arguments, "step", &step) == ArgumentStatus::Ok
            && valueOk();
    } else if (verb == "paProfile.setUseMax") {
        request.action = Action::SetUseMax;
        QVariant on;
        understood = hasExactlyArguments(invoke.arguments, { "band", "on" }) && bandOk()
            && findArgument(invoke.arguments, "on", &on) && on.typeId() == QMetaType::Bool;
        request.on = on.toBool();
    }
    if (!understood) {
        emitResult(verb, invoke.commandId, false,
                   QStringLiteral("The request for the PA profile was not understood."), {});
        return;
    }
    request.band = band;
    request.step = step;
    request.requesterHoldsTransmit = m_transmitAccess.holdsTransmit
        && m_transmitAccess.holdsTransmit(m_requester);
    QString reason;
    if (!m_radioModel->paProfileActionForStation(request, &reason)) {
        emitResult(verb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change the PA profile.")
                                    : reason, {});
        return;
    }
    emitResult(verb, invoke.commandId, true, QString(), {});
}

// R-R3-49 (parity Task 3): the RADE applet's Reset vocoder, on the Core's
// RADE channel as the local button does. Keys nothing.
void SessionCommandDispatcher::handleRadeResetVocoder(const SessionMessage& invoke)
{
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The request to reset the RADE vocoder was not understood."), {});
        return;
    }
    // Ruling 7.7: the holder's while transmit is held.
    if (refusedForTheHolder(invoke.commandVerb, invoke.commandId)) {
        return;
    }
    QString reason;
    if (!m_radioModel->resetRadeVocoderForStation(&reason, m_transmitSettingsOnAir)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not reset the RADE vocoder.")
                                    : reason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

void SessionCommandDispatcher::handleDisconnectPgxl(const SessionMessage& invoke)
{
    if (!hasExactlyArguments(invoke.arguments, {})) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->disconnectPgxlForStation(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not disconnect the Power Genius.") : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-47: autoReconnect (bool), keepaliveSec (i64, 1 to 3600), pingSec
// (i64, 0 to 3600), saved together on the Core and applied at once.
void SessionCommandDispatcher::handleSetPgxlConnectionSettings(const SessionMessage& invoke)
{
    QVariant autoReconnect;
    int keepaliveSec = 0;
    int pingSec = 0;
    if (!hasExactlyArguments(invoke.arguments, { "autoReconnect", "keepaliveSec", "pingSec" })
        || !findArgument(invoke.arguments, "autoReconnect", &autoReconnect)
        || autoReconnect.typeId() != QMetaType::Bool
        || !hasWireKind(invoke.arguments, "keepaliveSec", MirrorWireKind::Int64)
        || !hasWireKind(invoke.arguments, "pingSec", MirrorWireKind::Int64)
        || findIntArgument(invoke.arguments, "keepaliveSec", &keepaliveSec) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "pingSec", &pingSec) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    QString reason;
    if (!m_radioModel->setPgxlConnectionSettingsForStation(autoReconnect.toBool(), keepaliveSec,
                                                           pingSec, &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not change the Power Genius connection settings.") : reason,
                   {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// R-R3-46 (radioHardwareVersion 2): Setup's Probe button on the HL2 I/O
// board tab, for a remote window. The Core makes the same call a local
// window's button makes (RadioModel::requestIoBoardProbe).
void SessionCommandDispatcher::handleRequestIoBoardProbe(const SessionMessage& invoke)
{
    if (!invoke.arguments.isEmpty()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    const RadioModel::IoBoardProbeOutcome outcome = m_radioModel->requestIoBoardProbe();
    emitResult(invoke.commandVerb, invoke.commandId, outcome.sent, outcome.reason, {});
}

// R-R3-46 fix wave (radioHardwareVersion 3): one band's receive antenna
// from a remote window. The window used to send the whole 14-band list, so
// a list built before a change the Core made to another band (the VFO
// flag, another window) put that band back. The Core changes only the band
// named, through its own AlexAntennaFacade and AlexController, and every
// window follows the `alexAntennas` delta.
void SessionCommandDispatcher::handleSetAlexRxAntenna(const SessionMessage& invoke)
{
    int band = 0;
    int antenna = 0;
    QVariant rxOnly;
    if (!hasExactlyArguments(invoke.arguments, { "band", "antenna", "rxOnly" })
        || findIntArgument(invoke.arguments, "band", &band) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "antenna", &antenna) != ArgumentStatus::Ok
        || !findArgument(invoke.arguments, "rxOnly", &rxOnly)
        || rxOnly.typeId() != QMetaType::Bool) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    AlexAntennaFacade* const alex = m_radioModel->alexAntennaFacade();
    if (alex == nullptr || !alex->isBound()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core has no antenna settings ready."), {});
        return;
    }
    // A band number: 160m .. XVTR (0-13) or 2 m (27, R-IOS-26).
    if (band < 0 || band >= static_cast<int>(Band::Count)
        || !hasPerBandState(static_cast<Band>(band))) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core keeps antennas for 160 m to 6 m, 2 m, GEN, WWV and XVTR."), {});
        return;
    }
    const bool receiveOnly = rxOnly.toBool();
    const QString reason = receiveOnly ? alex->setRxOnlyAntForBand(Band(band), antenna)
                                       : alex->setRxAntForBand(Band(band), antenna);
    emitResult(invoke.commandVerb, invoke.commandId, reason.isEmpty(), reason, {});
}

// Parity mini-round (radioHardwareVersion 6): one band's TX antenna from a
// remote window's Antenna Control grid. Parity Task 12 sent the whole
// 14-band txAntennas list, so a list built before a change the Core made to
// another band put that band back; the Core now changes only the band
// named, through its own AlexAntennaFacade and AlexController (the local
// grid's setTxAnt), and every window follows the `alexAntennas` delta. A
// port blocked for transmit is kept off the band, with its reason. Like the
// whole list, it keys nothing and is taken on the air and on a receive-only
// Core, as Thetis's own TX antenna grid is (parity Task 12's reading of
// setup.cs ProcessAlexAntRadioButton).
void SessionCommandDispatcher::handleSetAlexTxAntenna(const SessionMessage& invoke)
{
    int band = 0;
    int antenna = 0;
    if (!hasExactlyArguments(invoke.arguments, { "band", "antenna" })
        || findIntArgument(invoke.arguments, "band", &band) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "antenna", &antenna) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    AlexAntennaFacade* const alex = m_radioModel->alexAntennaFacade();
    if (alex == nullptr || !alex->isBound()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core has no antenna settings ready."), {});
        return;
    }
    // A band number: 160m .. XVTR (0-13) or 2 m (27, R-IOS-26).
    if (band < 0 || band >= static_cast<int>(Band::Count)
        || !hasPerBandState(static_cast<Band>(band))) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core keeps antennas for 160 m to 6 m, 2 m, GEN, WWV and XVTR."), {});
        return;
    }
    const QString reason = alex->setTxAntForBand(Band(band), antenna);
    emitResult(invoke.commandVerb, invoke.commandId, reason.isEmpty(), reason, {});
}

// R-R3-46 (parity Task 14, radioHardwareVersion 7): HL2 Options' I2C tool
// from a remote window. The Core runs the read or write as its own tool
// does (RadioModel::requestIoBoardI2c): a read is answered when the radio
// answers, with `values` value (i64: C1 << 24 | C2 << 16 | C3 << 8 | C4),
// or refused "The radio did not answer the I2C request." once the local
// tool's 21 ms pass; a write is answered once queued and refused while the
// radio is on the air. `value` is ignored on a read. An answer due to an
// earlier session is dropped.
void SessionCommandDispatcher::handleRequestIoBoardI2c(const SessionMessage& invoke)
{
    const QByteArray verb = invoke.commandVerb;
    const quint32 commandId = invoke.commandId;
    RadioModel::IoBoardI2cRequest request;
    QVariant write;
    if (!hasExactlyArguments(invoke.arguments, { "bus", "address", "register", "write", "value" })
        || findIntArgument(invoke.arguments, "bus", &request.bus) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "address", &request.address) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "register", &request.reg) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "value", &request.value) != ArgumentStatus::Ok
        || !findArgument(invoke.arguments, "write", &write)
        || write.typeId() != QMetaType::Bool) {
        emitResult(verb, commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    request.write = write.toBool();
    const QPointer<SessionCommandDispatcher> self(this);
    const quint64 generation = m_sessionGeneration;
    // Checkpoint join (R-IOS-02): a read's answer comes on a later turn,
    // named for the session that asked.
    const QString owner = m_sessionOwner;
    const bool isRead = !request.write;
    m_radioModel->requestIoBoardI2c(
        request, [self, generation, owner, verb, commandId, isRead](
                     bool ok, qint64 value, const QString& reason) {
            if (!self || self->m_sessionGeneration != generation) { return; }
            QList<MirrorUpdate> values;
            if (ok && isRead) {
                values.append({0, "value", MirrorWireKind::Int64, value});
            }
            self->emitResultAs(owner, SessionMessages::commandResult(
                verb, commandId, ok, ok ? QString() : reason, {}, values));
        });
}

// R-R3-49 (parity Task 16, dspInfoVersion 1): Setup > DSP > Options >
// High-resolution filter characteristics in a remote window. The Core
// computes its slice's receiver's curve as the local filter graph does
// (RadioModel::filterResponseForStation) and answers with `values`
// startHz and stepHz (f64) and magnitudesDbJson (utf8, a JSON array of dB,
// 0 at the peak). With `highResolution` false no curve is wanted and the
// array is empty. A read: it reaches no radio, so it is answered on and off
// the air.
void SessionCommandDispatcher::handleFilterResponse(const SessionMessage& invoke)
{
    int sliceId = -1;
    QVariant highResolution;
    if (!hasExactlyArguments(invoke.arguments, { "sliceId", "highResolution" })
        || findIntArgument(invoke.arguments, "sliceId", &sliceId) != ArgumentStatus::Ok
        || !findArgument(invoke.arguments, "highResolution", &highResolution)
        || highResolution.typeId() != QMetaType::Bool) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    RadioModel::FilterResponse response;
    QString reason;
    if (!m_radioModel->filterResponseForStation(sliceId, highResolution.toBool(), &response,
                                                &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
        return;
    }
    emit commandResultReady(SessionMessages::commandResult(
        invoke.commandVerb, invoke.commandId, true, QString(), {},
        {{0, "startHz", MirrorWireKind::Float64, response.startHz},
         {0, "stepHz", MirrorWireKind::Float64, response.stepHz},
         {0, "magnitudesDbJson", MirrorWireKind::Utf8,
          RadioModel::filterResponseToJson(response.magnitudesDb)}}));
}

// R-R3-49 / R-IOS-18 (remote-window parity Task 22, the iPhone app plan's
// Task 25, supportBundleVersion 1). support.collect answers with `bundle`,
// the Core's support bundle (SupportBundle::buildCoreBundle: a ZIP of at
// most 2 MiB, secrets removed) in base64; the bundle is written on a worker
// thread and the answer follows on a later turn, so the radio never waits
// for it. support.setLogCategories turns on exactly the listed categories
// (ids this Core does not keep are ignored) and radio's logCategories
// follows. A window at the Core does both while the radio is on the air,
// so neither waits for it here.
void SessionCommandDispatcher::handleSupport(const SessionMessage& invoke)
{
    if (invoke.commandVerb == "support.setLogCategories") {
        QVariant categories;
        if (!hasExactlyArguments(invoke.arguments, {"categories"})
            || !findArgument(invoke.arguments, "categories", &categories)
            || categories.typeId() != QMetaType::QString) {
            emitResult(invoke.commandVerb, invoke.commandId, false,
                       QStringLiteral("The Core could not read this request."), {});
            return;
        }
        QStringList ids;
        for (const QString& id : categories.toString().split(QLatin1Char(','))) {
            if (!id.trimmed().isEmpty()) {
                ids.append(id.trimmed());
            }
        }
        LogManager::instance().setEnabledList(ids);
        emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
        return;
    }
    if (!invoke.arguments.isEmpty()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (m_supportBundleRunning) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core is already making a support bundle. Try again in a "
                                  "moment."),
                   {});
        return;
    }
    SupportBundle::Inputs inputs = m_supportInputs
        ? m_supportInputs()
        : SupportBundle::gatherInputs(m_radioModel.data());
    m_supportBundleRunning = true;
    const QByteArray verb = invoke.commandVerb;
    const quint32 commandId = invoke.commandId;
    // The answer comes on a later turn: it goes to the session that asked.
    const QString owner = m_sessionOwner;
    SupportBundle::buildCoreBundleAsync(this, std::move(inputs),
                                        [this, verb, commandId, owner](QByteArray bundle) {
        m_supportBundleRunning = false;
        if (bundle.isEmpty()) {
            emitResultAs(owner, SessionMessages::commandResult(
                verb, commandId, false,
                QStringLiteral("The Core could not make its support bundle."), {}));
            return;
        }
        emitResultAs(owner, SessionMessages::commandResult(
            verb, commandId, true, QString(), {},
            {{0, "bundle", MirrorWireKind::Utf8, QString::fromLatin1(bundle.toBase64())}}));
    });
}

// R-IOS-25 / R-R3-49 (parity Task 19, recordStreamVersion 1): a record
// stream subscription is the asking connection's, so the station server
// answers it (RecordAccess). Without one (a dispatcher on its own) there is
// no stream to follow.
void SessionCommandDispatcher::handleRecords(const SessionMessage& invoke)
{
    if (m_recordAccess && m_recordAccess(invoke)) {
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, false,
               invoke.commandVerb == "txModMonitor.reset"
                   ? QStringLiteral("This Core does not send the modulation monitor. "
                                    "Updating the Core may help.")
                   : QStringLiteral("This Core does not send its spots or console lines."),
               {});
}

// R-IOS-18 / R-R3-49 (parity Task 21, stationRadiosVersion 1): This Core's
// Change radio, Scan again, Edit radio and Forget radio. Each is refused
// while the Core's radio is on the air (the parity plan's rule), and
// nothing is saved or switched then.
void SessionCommandDispatcher::setStationRadios(StationRadios* radios)
{
    m_stationRadios = radios;
}

void SessionCommandDispatcher::handleStationRadios(const SessionMessage& invoke)
{
    const bool rescan = invoke.commandVerb == "station.rescanRadios";
    const bool setModel = invoke.commandVerb == "station.setRadioModel";
    QVariant mac;
    int model = 0;
    const bool readable = rescan
        ? invoke.arguments.isEmpty()
        : (hasExactlyArguments(invoke.arguments,
                               setModel ? std::initializer_list<QByteArray>{"mac", "model"}
                                        : std::initializer_list<QByteArray>{"mac"})
           && findArgument(invoke.arguments, "mac", &mac) && mac.typeId() == QMetaType::QString
           && (!setModel
               || findIntArgument(invoke.arguments, "model", &model) == ArgumentStatus::Ok));
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (m_stationRadios.isNull()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("This Core does not change its radio from this app."), {});
        return;
    }
    QString reason;
    if (m_radioModel->stationOnAirRefusal(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
        return;
    }
    bool accepted = false;
    if (rescan) {
        accepted = m_stationRadios->rescan(&reason);
    } else if (setModel) {
        accepted = m_stationRadios->setModel(mac.toString(), model, &reason);
    } else if (invoke.commandVerb == "station.forgetRadio") {
        accepted = m_stationRadios->forget(mac.toString(), &reason);
    } else {
        accepted = m_stationRadios->select(mac.toString(), &reason);
    }
    emitResult(invoke.commandVerb, invoke.commandId, accepted, reason, {});
}

void SessionCommandDispatcher::handleSettingsHygiene(const SessionMessage& invoke)
{
    QVariant supplied;
    if (!hasExactlyArguments(invoke.arguments, {"mac"})
        || !findArgument(invoke.arguments, "mac", &supplied)
        || supplied.typeId() != QMetaType::QString) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    const QString current = AppSettings::normalizedRadioMac(m_radioModel->currentRadioMac());
    const QString target = supplied.toString();
    if (current.isEmpty() || target != current) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("This radio changed. Open Settings Validation for the current radio."), {});
        return;
    }
    const bool mutation = invoke.commandVerb != "station.validateSettings";
    QString reason;
    if (mutation && m_radioModel->stationOnAirRefusal(&reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
        return;
    }
    SettingsHygiene& hygiene = m_radioModel->settingsHygiene();
    if (mutation) {
        hygiene.validate(current, m_radioModel->boardCapabilities());
        if (!SettingsHygieneWire::encode({current, hygiene.issues()})) {
            emitResult(invoke.commandVerb, invoke.commandId, false,
                       QStringLiteral("Settings Validation has too many details to show."), {});
            return;
        }
    }
    if (invoke.commandVerb == "station.forgetSettings") {
        hygiene.forgetRadio(current);
    } else if (invoke.commandVerb == "station.repairSettings") {
        // G-38: the same repair a local window runs (Diagnostics' Repair
        // invalid settings); it re-validates on its own.
        hygiene.resetSettingsToDefaults(current, m_radioModel->boardCapabilities());
    } else {
        hygiene.validate(current, m_radioModel->boardCapabilities());
    }
    const auto values = SettingsHygieneWire::encode({current, hygiene.issues()});
    if (!values) {
        // A mutation already ran; accepted must reflect that fact even if
        // its post-operation details cannot be encoded. The client treats
        // the missing details as unavailable and can re-validate.
        emit commandResultReady(SessionMessages::commandResult(
            invoke.commandVerb, invoke.commandId, mutation, mutation ? QString() :
                QStringLiteral("Settings Validation has too many details to show."), {}, {}));
        return;
    }
    emit commandResultReady(SessionMessages::commandResult(
        invoke.commandVerb, invoke.commandId, true, {}, {}, *values));
}

// R-IOS-25 / R-R3-49 (parity Task 19, recordStreamVersion 1): the Spot Hub's
// Connect, Start, Stop, typed cluster commands and Clear All Spots for the
// Core's own spot sources (SpotSourceHost). None touches the radio, so none
// waits while it is on the air.
void SessionCommandDispatcher::handleSpotSources(const SessionMessage& invoke)
{
    SpotSourceHost* host = m_radioModel->spotSourceHost();
    const bool clearAll = invoke.commandVerb == "spots.clearAll";
    const bool sendCommand = invoke.commandVerb == "spots.sendCommand";
    QVariant source;
    QVariant text;
    const bool readable = clearAll
        ? invoke.arguments.isEmpty()
        : (hasExactlyArguments(invoke.arguments, sendCommand
                                   ? std::initializer_list<QByteArray>{"source", "text"}
                                   : std::initializer_list<QByteArray>{"source"})
           && findArgument(invoke.arguments, "source", &source)
           && source.typeId() == QMetaType::QString
           && (!sendCommand
               || (findArgument(invoke.arguments, "text", &text)
                   && text.typeId() == QMetaType::QString)));
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (host == nullptr) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("This Core does not run spot sources."), {});
        return;
    }
    QString reason;
    bool accepted = true;
    if (clearAll) {
        host->clearAll();
    } else if (invoke.commandVerb == "spots.connect") {
        accepted = host->connectSource(source.toString(), &reason);
    } else if (invoke.commandVerb == "spots.disconnect") {
        accepted = host->disconnectSource(source.toString(), &reason);
    } else {
        accepted = host->sendCommand(source.toString(), text.toString(), &reason);
    }
    emitResult(invoke.commandVerb, invoke.commandId, accepted, reason, {});
}

// R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20,
// stationFreedvVersion 1): the FreeDV Reporter dialog's Send and Clear, its
// Send QSY and the FreeDV tab's "Hide my station", for the Core's own
// FreeDV Reporter (SpotSourceHost). None touches the radio, so none waits
// while it is on the air.
void SessionCommandDispatcher::handleFreedv(const SessionMessage& invoke)
{
    SpotSourceHost* host = m_radioModel->spotSourceHost();
    QVariant text;
    QVariant callsign;
    QVariant on;
    qint64 frequencyHz = 0;
    bool readable = false;
    if (invoke.commandVerb == "freedv.setMessage") {
        readable = hasExactlyArguments(invoke.arguments, {"text"})
            && findArgument(invoke.arguments, "text", &text)
            && text.typeId() == QMetaType::QString;
    } else if (invoke.commandVerb == "freedv.sendQsy") {
        QVariant hz;
        readable = hasExactlyArguments(invoke.arguments, {"callsign", "frequencyHz"})
            && findArgument(invoke.arguments, "callsign", &callsign)
            && callsign.typeId() == QMetaType::QString
            && findArgument(invoke.arguments, "frequencyHz", &hz)
            && hz.typeId() == QMetaType::LongLong;
        frequencyHz = hz.toLongLong();
    } else {
        readable = hasExactlyArguments(invoke.arguments, {"on"})
            && findArgument(invoke.arguments, "on", &on) && on.typeId() == QMetaType::Bool;
    }
    if (!readable) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    if (host == nullptr || m_radioModel->freeDvReporter() == nullptr) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core does not run FreeDV Reporter."), {});
        return;
    }
    QString reason;
    bool accepted = false;
    if (invoke.commandVerb == "freedv.setMessage") {
        accepted = host->setFreedvMessage(text.toString(), &reason);
    } else if (invoke.commandVerb == "freedv.sendQsy") {
        accepted = host->sendFreedvQsy(callsign.toString(), frequencyHz, &reason);
    } else {
        accepted = host->setFreedvHidden(on.toBool(), &reason);
    }
    emitResult(invoke.commandVerb, invoke.commandId, accepted, reason, {});
}

// R-R3-46 (parity Task 14, radioHardwareVersion 7): Pin Control on HL2
// Options from a remote window: one of the Core's I/O board outputs on or
// off (RadioModel::setIoBoardOutput), read back into `ioBoard` outputs.
// Refused while the radio is on the air.
void SessionCommandDispatcher::handleSetIoBoardOutput(const SessionMessage& invoke)
{
    int pin = 0;
    QVariant on;
    if (!hasExactlyArguments(invoke.arguments, { "pin", "on" })
        || findIntArgument(invoke.arguments, "pin", &pin) != ArgumentStatus::Ok
        || !findArgument(invoke.arguments, "on", &on) || on.typeId() != QMetaType::Bool) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    bool accepted = false;
    QString refusal;
    m_radioModel->setIoBoardOutput(pin, on.toBool(),
                                   [&accepted, &refusal](bool ok, qint64, const QString& reason) {
                                       accepted = ok;
                                       refusal = reason;
                                   });
    emitResult(invoke.commandVerb, invoke.commandId, accepted, refusal, {});
}

// Level Cal (radioHardwareVersion 12): a remote window's Setup > Hardware >
// Calibration Reset, the call a local window's Reset makes
// (RadioModel::resetLevelCalibration, Thetis ResetLevelCalibration,
// console.cs:46868-46886 [v2.10.3.15]). Thetis has no MOX check there, so
// it is taken on the air too. The removed keys reach every window as
// settings.value with no entry.
void SessionCommandDispatcher::handleResetLevelCalibration(const SessionMessage& invoke)
{
    if (!invoke.arguments.isEmpty()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    m_radioModel->resetLevelCalibration();
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// Level Cal (radioHardwareVersion 12): a remote window's Setup > Hardware >
// Calibration Start, the call a local window makes
// (RadioModel::requestStartLevelCalibration): the Core runs Thetis
// CalibrateLevel (console.cs:9856-10232 [v2.10.3.15]) on the slice named.
// StationServer has already refused a window signed in with the pairing
// token and a radio on the air. The run's own refusals come back as the
// result; its progress reaches the window as the levelCal* properties.
void SessionCommandDispatcher::handleStartLevelCalibration(const SessionMessage& invoke)
{
    double levelDbm = 0.0;
    double frequencyHz = 0.0;
    int sliceId = -1;
    if (!hasExactlyArguments(invoke.arguments, {"levelDbm", "frequencyHz", "sliceId"})
        || !findFiniteDoubleArgument(invoke.arguments, "levelDbm", &levelDbm)
        || !findFiniteDoubleArgument(invoke.arguments, "frequencyHz", &frequencyHz)
        || findIntArgument(invoke.arguments, "sliceId", &sliceId) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    // A named slice was checked in refusedForAnotherDevice. No slice named
    // (-1) means the Core's active slice, which may be another device's:
    // a device calibrates it only when it may change it.
    if (sliceId < 0 && !m_requester.isEmpty() && m_sliceAccess) {
        const SliceModel* active = m_radioModel->activeSlice();
        const QString reason =
            active != nullptr ? m_sliceAccess(m_requester, active->sliceIndex()) : QString();
        if (!reason.isEmpty()) {
            emitResult(invoke.commandVerb, invoke.commandId, false, reason, {});
            return;
        }
    }
    const QString refusal = m_radioModel->requestStartLevelCalibration(
        static_cast<float>(levelDbm), frequencyHz, sliceId);
    emitResult(invoke.commandVerb, invoke.commandId, refusal.isEmpty(), refusal, {});
}

// Level Cal: Cancel, Thetis closing the progress window. It only stops a
// run, so any window may send it; with nothing running it does nothing.
void SessionCommandDispatcher::handleCancelLevelCalibration(const SessionMessage& invoke)
{
    if (!invoke.arguments.isEmpty()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    m_radioModel->requestCancelLevelCalibration();
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

// Parity ruling C4 (radioHardwareVersion 8): a remote window's Setup >
// Hardware > Radio Info sample rate, the change a local window makes
// (RadioModel::setSampleRateLiveAsync): every receiver and the radio's own
// rate, which new receivers take. StationServer has already refused a
// window signed in with the pairing token and a radio on the air, and the
// confirm step has asked the other devices. Like requestSliceSampleRate it
// runs on a later turn, and answers when the change has finished, naming
// the slices whose rate moved.
void SessionCommandDispatcher::handleSetRadioSampleRate(const SessionMessage& invoke)
{
    int rateHz = 0;
    if (!hasExactlyArguments(invoke.arguments, { "rateHz" })
        || findIntArgument(invoke.arguments, "rateHz", &rateHz) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    const QVector<int> allowed = m_radioModel->allowedStreamSampleRates();
    if (allowed.isEmpty()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The radio is not connected, so its sample rate cannot change."),
                   {});
        return;
    }
    if (!allowed.contains(rateHz)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("This radio cannot run at that sample rate."), {});
        return;
    }
    const QByteArray verb = invoke.commandVerb;
    const quint32 commandId = invoke.commandId;
    const QString owner = m_sessionOwner;
    const QPointer<SessionCommandDispatcher> self(this);
    const QPointer<RadioModel> radioModel(m_radioModel);
    QMetaObject::invokeMethod(
        m_radioModel,
        [self, radioModel, verb, commandId, rateHz, owner]() {
            if (self.isNull() || radioModel.isNull()) {
                return;
            }
            QHash<int, int> before;
            for (SliceModel* slice : radioModel->slices()) {
                if (slice != nullptr) {
                    before.insert(slice->sliceIndex(), slice->sampleRateHz());
                }
            }
            radioModel->changeRadioSampleRate(
                rateHz, [self, radioModel, verb, commandId, owner, before](bool ok) {
                    if (self.isNull() || radioModel.isNull()) {
                        return;
                    }
                    if (!ok) {
                        self->emitResultAs(owner, SessionMessages::commandResult(
                            verb, commandId, false,
                            QStringLiteral("The sample rate did not change. Try again."), {}));
                        return;
                    }
                    QList<QByteArray> affected;
                    for (SliceModel* slice : radioModel->slices()) {
                        if (slice != nullptr
                            && before.value(slice->sliceIndex(), -1) != slice->sampleRateHz()) {
                            affected.append(ObjectRegistry::keyForSlice(slice->sliceIndex()));
                        }
                    }
                    self->emitResultAs(owner, SessionMessages::commandResult(
                        verb, commandId, true, QString(), affected));
                });
        },
        Qt::QueuedConnection);
}

// R-R3-46 / R-R3-21 (radioHardwareVersion 4): one receive filter chain's
// filter policy from a remote window's filter policy dialog. The Core makes
// the call its own dialog's Apply makes (AlexController::setBpfMode, through
// its AlexAntennaFacade), saves it for its radio and publishes the chain's
// state (rxFilter<N>Mode) to every window. The policy picks the receive
// band-pass filter only, so a receive-only Core applies it too.
void SessionCommandDispatcher::handleSetAlexBpfMode(const SessionMessage& invoke)
{
    int chain = 0;
    int mode = 0;
    if (!hasExactlyArguments(invoke.arguments, { "chain", "mode" })
        || findIntArgument(invoke.arguments, "chain", &chain) != ArgumentStatus::Ok
        || findIntArgument(invoke.arguments, "mode", &mode) != ArgumentStatus::Ok) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }
    AlexAntennaFacade* const alex = m_radioModel->alexAntennaFacade();
    if (alex == nullptr || !alex->isBound()) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core has no filter settings ready."), {});
        return;
    }
    const QString reason = alex->setBpfModeForChain(chain, mode);
    emitResult(invoke.commandVerb, invoke.commandId, reason.isEmpty(), reason, {});
}

void SessionCommandDispatcher::handleSetFourO3AEnabled(const SessionMessage& invoke)
{
    QVariant enabled;
    if (!hasExactlyArguments(invoke.arguments, { "enabled" })
        || !findArgument(invoke.arguments, "enabled", &enabled)
        || enabled.typeId() != QMetaType::Bool) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   QStringLiteral("The Core could not read this request."), {});
        return;
    }

    QString reason;
    if (!m_radioModel->setFourO3AEnabledForStation(enabled.toBool(), &reason)) {
        emitResult(invoke.commandVerb, invoke.commandId, false,
                   reason.isEmpty() ? QStringLiteral("The Core did not turn 4O3A on or off.") : reason, {});
        return;
    }
    emitResult(invoke.commandVerb, invoke.commandId, true, QString(), {});
}

} // namespace NereusSDR
