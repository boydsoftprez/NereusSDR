#pragma once
// =================================================================
// src/core/session/SessionMessages.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 10.
//
// The wire-message shapes StateMirror::attachSession() (StateMirror.h)
// sends, and a live session (Task 18) will send and receive over the wss
// control channel. Seven kinds exist so far:
//
//   Schema            -- one mirrored class's property table (ordinal,
//                         name, wire kind; no live value), sent once per
//                         class per session, before anything referring to
//                         an instance of it.
//   ObjectCreate       -- one object appeared. Carries the FULL settled
//                         property bag (MirrorSchema.h's MirrorUpdate,
//                         which DOES carry a value).
//   ObjectDestroy      -- one object vanished. No properties: a client
//                         that already has the object needs nothing more
//                         than the key to drop it.
//   Delta              -- a SUBSET of one object's properties, changed
//                         since the last message about it.
//   SnapshotComplete   -- no fields at all. Marks the end of the
//                         connect-time burst; StateMirror::attachSession()
//                         guarantees nothing after this marker was already
//                         implied by something before it, and nothing
//                         before it is missing.
//   CommandInvoke      -- Task 11. A client asking the daemon to run one of
//                         the RadioModel entry points that is not a plain
//                         property write: create a slice, remove one,
//                         re-tune a DDC's rate. `commandVerb` names which;
//                         `arguments` carries its named, typed parameters
//                         (MirrorUpdate reused as a generic {name, kind,
//                         value} triple -- `ordinal` is meaningless here
//                         and always 0). `commandId` is caller-assigned and
//                         echoed verbatim on the matching CommandResult, so
//                         a caller with more than one command in flight can
//                         still tell results apart.
//   CommandResult      -- Task 11. What a CommandInvoke actually did.
//                         `accepted` / `reason` mirror MirrorApplyResult's
//                         own shape. `affectedKeys` names the object(s)
//                         this result is about; its exact meaning is per-
//                         verb, not a blanket "everything that changed"
//                         guarantee -- see SessionMessage::affectedKeys
//                         below for the precise contract and the one verb
//                         (requestSliceSampleRate) where it really is a
//                         full before/after diff.
//
// Task 18 appends eleven more, in three groups, to the SAME table rather
// than starting a second one:
//
//   Hello / AuthRequest / AuthResult / Capabilities / SessionEnd
//                      -- the parent design's section 7.0 connect
//                         sequence, in order: "TLS establish -> protocol
//                         hello carrying a semantic version from both
//                         ends -> authentication -> capability exchange
//                         -> state snapshot -> snapshot-complete marker".
//   PropertyWrite       -- the INBOUND half of the property mirror: a
//                         client asking the daemon to write mirrored
//                         properties (StateMirror::applyInbound). Tasks 7
//                         and 8 built both halves of that apply; until now
//                         nothing named it on the wire.
//   SettingsSnapshot / SettingsWrite / SettingsRemove / SettingsValue /
//   SettingsReject     -- SettingsProxy (client) and SettingsProxyServer
//                         (daemon) talking to each other. Deliberately
//                         separate kinds from the property mirror above:
//                         settings are a flat QString-valued key space
//                         with no object identity and no schema, so
//                         routing them through Delta would mean
//                         overloading `objectKey` with something that is
//                         not an object.
//
// Encodes as JSON text, per the R2 design addendum section 6: "R2 encodes
// as JSON text on the reliable control channel ... because the ordinal
// dictionary [MirrorSchema's dense per-class ordinals] exists from day
// one, R3 swaps the codec without touching the object model." This is a
// deliberate, documented staged simplification, not an oversight: section
// 7.2 of the parent design calls for a compact native envelope, and R2
// does not build it.
//
// JSON numbers are IEEE-754 doubles with no distinct 64-bit integer type,
// so MirrorWireKind::Int64 and ::Enum values are carried as JSON numbers,
// exact up to 2^53. Every currently mirrored integral property
// (filterLow/filterHigh in Hz, sliceIndex, chainIndex, wire ordinals, and
// so on) sits comfortably inside that range -- see MirrorSchema.h's
// section 6 note ("Type surface is small") for the full inventory. A
// future property that genuinely needs the missing bits is an R3 codec
// problem, not a reason to complicate this one early.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 10: session
//                                    message shapes and JSON codec. AI-
//                                    assisted transformation via Anthropic
//                                    Claude Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11: CommandInvoke
//                                    / CommandResult message shapes and
//                                    codec. AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: the section
//                                    7.0 handshake kinds (Hello /
//                                    AuthRequest / AuthResult /
//                                    Capabilities / SessionEnd), the
//                                    inbound PropertyWrite, and the five
//                                    settings kinds. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    settingsValueAbsent(). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01):
//                                    allKinds(), the declared list of every
//                                    kind the link surface is captured
//                                    from. AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): the
//                                    hello's `majors` and `features`.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 12 (R-IOS-08,
//                                    R-IOS-01): the Core hello's
//                                    `identity` and `challenge`,
//                                    auth.request's `device`, and the end
//                                    `code` on auth.result and
//                                    session.end. AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08): the
//                                    five pair.* kinds (pair.start,
//                                    pair.accept, pair.spake, pair.confirm,
//                                    pair.fail). AI-assisted implementation
//                                    via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the optional device shortName in
//               auth.request, stored with the device. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02):
//               SessionEndCode::kSameDevice. J.J. Boyd (KG4VCF), with AI-
//               assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): path.join and
//               path.switch. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 74 (R-IOS-30): confirm.request and notice
//               (SessionPrompt). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               confirm.request holder (takeTransmit). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 4: the
//               notice kind controlTaken documented. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QJsonArray>
#include <QJsonObject>

#include <optional>

#include "core/session/MirrorSchema.h"
#include "core/session/RecordStream.h"
#include "core/session/StationTelemetry.h"

namespace NereusSDR {

/// Which kind of session message a SessionMessage carries. Task 11 appends
/// CommandInvoke / CommandResult here; the codec lives in this file, the
/// dispatch onto RadioModel lives in SessionCommandDispatcher.
enum class SessionMessageKind {
    Schema,
    ObjectCreate,
    ObjectDestroy,
    Delta,
    SnapshotComplete,
    CommandInvoke,
    CommandResult,

    // ── Task 18: the section 7.0 connect sequence ───────────────────────
    Hello,
    AuthRequest,
    AuthResult,
    Capabilities,
    SessionEnd,
    SessionHeld,
    SessionTakeover,

    // ── Task 18: the inbound half of the property mirror ────────────────
    PropertyWrite,

    // ── Task 18: SettingsProxy / SettingsProxyServer over the wire ──────
    SettingsSnapshot,
    SettingsWrite,
    SettingsRemove,
    SettingsValue,
    SettingsReject,
    // R3: bounded control/signalling for the separate encrypted media peer.
    MediaControl,
    // R3: bounded, capability-gated station observations. Never a command.
    StationTelemetry,
    PropertyResult,
    // iPhone app Task 14 (R-IOS-08): pairing a device, on a connection
    // that sends pair.start after its hello instead of auth.request, and
    // only to a Core whose hello declares features.pairing (the link
    // document's Pairing section). The connection ends after pair.accept,
    // the Core's pair.confirm, or pair.fail.
    PairStart,    // device -> Core: mode "lan" or "code", and the device
    PairAccept,   // Core -> device: one tap succeeded; the Core's identity
    PairSpake,    // both: one SPAKE2+EE step, 0 to 3
    PairConfirm,  // both: a confirmation box under the agreed keys
    PairFail,     // Core -> device: why not, and when to try again
    // iPhone app Task 74 (R-IOS-30; the several-devices design, sections
    // 7.3 and 7.4): Core -> device, only to a device with
    // sessionHolderVersion 1. A change that reaches another device, or a
    // take, asks first (confirm.request); what another device did is told
    // afterwards (notice).
    ConfirmRequest,
    Notice,
    // Parity Task 19 (R-IOS-25; the link's "Record streams" section): Core
    // -> peer, only to a peer that subscribed with records.subscribe
    // (recordStreamVersion 1). Upserts and removes for one stream.
    RecordBatch,
    // iPhone app plan Task 29 (R-IOS-16; the link document, section 21.2):
    // moving a session to another connection (controlSwitchVersion 1).
    // path.join: device -> Core on the new connection, after its hello and
    // in place of auth.request, carrying the ticket session.pathTicket
    // gave. path.switch: both ways on the old connection, the last message
    // each end sends there.
    PathJoin,
    PathSwitch,
};

/// The session protocol's own semantic version, advertised by BOTH ends in
/// the Hello message and governed by the parent design's section 7.0
/// version policy: "refuse on major mismatch, negotiate down on minor".
///
/// Major is incremented ONLY for a breaking change to framing, identity, or
/// the snapshot contract (section 7.0's own wording). Everything else is a
/// minor bump plus a capability entry, because a desktop GUI several
/// releases ahead of a Pi still running this one is the EXPECTED case, not
/// an error case, and it has to degrade rather than refuse.
///
/// R2 ships 1.0. It is deliberately not 0.x: the JSON control encoding is a
/// documented staged simplification (see this file's header), but the
/// message SHAPES -- object identity, the snapshot contract, the schema
/// table -- are the thing this number governs, and those are not
/// provisional. R3 swapping the codec under the same object model is a
/// minor bump at most.
inline constexpr quint16 kSessionProtocolMajor = 1;
inline constexpr quint16 kSessionProtocolMinor = 11;
inline constexpr quint16 kMediaSessionProtocolMinor = 1;
inline constexpr quint16 kRemoteCtunSessionProtocolMinor = 2;
inline constexpr quint16 kStationTelemetrySessionProtocolMinor = 3;
inline constexpr quint16 kRemoteTgxlConfigSessionProtocolMinor = 4;
// Both R3 accessory controls belong to minor 4. Separate capabilities let
// intermediate development builds expose TGXL configuration before the
// master/listener controls are available.
inline constexpr quint16 kRemoteFourO3AControlSessionProtocolMinor = 4;
inline constexpr quint16 kDspControlSessionProtocolMinor = 5;
inline constexpr quint16 kRemoteWidebandSessionProtocolMinor = 6;
inline constexpr quint16 kRemoteDisplayBudgetSessionProtocolMinor = 7;
// The audio context carries the accepted encoder profile when audio is on and
// the reason it is off otherwise. Minor-7 peers keep the eight-key context.
inline constexpr quint16 kRemoteAudioStatusSessionProtocolMinor = 8;
// The spectrum context reports what Core granted the endpoint: FFT size and
// tier, requested and granted points, and what limited them. Minor-8 peers
// keep the 19-key (20 with wideband) context.
inline constexpr quint16 kRemoteSpectrumGrantSessionProtocolMinor = 9;
// Station telemetry carries the Core computer's CPU, memory and temperature
// in an optional host section (stationTelemetryVersion 2). Minor-9 peers
// receive exactly the radio and audio sections.
inline constexpr quint16 kCoreHostTelemetrySessionProtocolMinor = 10;
// Station telemetry carries each receiver's processing load, input wait and
// skipped input in an optional receivers section (stationTelemetryVersion 3).
// Minor-10 peers receive exactly the radio, audio and host sections. The
// radio's PA readings and link quality (stationTelemetryVersion 4) also need
// minor 11; below it the radio section carries none of them.
inline constexpr quint16 kReceiverLoadSessionProtocolMinor = 11;
// A receiver that cannot keep up with neural noise reduction is stepped back
// at runtime: SliceModel's nnrLimit property and the nnr.tryAgain command.
// Same unreleased step as receiver load. Minor-10 peers never see the
// property and cannot send the command.
inline constexpr quint16 kNnrLimitSessionProtocolMinor = 11;
// The capability descriptor names why the display budget is below the
// Core's ceiling (displayBudgetReason: the Core computer is busy). Same
// unreleased step. Minor-10 peers receive exactly the five budget fields.
inline constexpr quint16 kDisplayBudgetReasonSessionProtocolMinor = 11;
// R-R3-46: the capability descriptor names the Core's radio model
// (hpsdrModel), how it talks to the radio (radioProtocol) and the radio's
// LAN address (radioAddress). Same unreleased step. Minor-10 peers receive
// exactly the descriptor they were built for.
inline constexpr quint16 kRadioIdentitySessionProtocolMinor = 11;
inline constexpr qsizetype kMaxMediaControlBytes = 128 * 1024;

// R-R3-16/17: how long either end waits for the connect sequence to finish
// once a link exists. The GUI runs it from attaching a transport until the
// snapshot-complete marker (StationClient::setHandshakeDeadlineMs); Core runs
// it from accepting a peer until that peer's snapshot has been sent
// (StationServer::setAuthDeadlineMs). One value on purpose: a Core whose event
// loop stalls mid-connect must be abandoned by the GUI in the same bound the
// Core uses to clear a peer that stalls, so neither end holds a half-made
// session the other has already given up on for long.
inline constexpr int kStationHandshakeDeadlineMs = 30000;
inline constexpr qsizetype kMaxStationTelemetryBytes = 16 * 1024;
// iPhone app plan Task 29 (R-IOS-16): the longest ticket path.join may
// carry. A Core's own tickets are 43 characters (32 bytes, base64url);
// the bound keeps a hostile one from being compared at any length.
inline constexpr qsizetype kMaxPathTicketChars = 128;
/// Link section 21.2: session.pathTicket's refusal while the radio is on the
/// air (MOX not idle), which a device waits out rather than counting as a
/// failed look.
inline constexpr const char* kPathTransmittingReason = "Not while the radio is transmitting.";

// iPhone app Task 12 (R-IOS-08): the machine-readable end codes an
// auth.result refusal or a session.end may carry in `code` (the link
// document, section 12.4). A client reads the code where it is present and
// falls back to the reason text for an older Core, which sends none. A
// code is a stable token, never shown; the reason beside it is what the
// operator reads.
namespace SessionEndCode {
/// Another app signed in and took the session.
inline constexpr const char* kTakenOver = "takenOver";
inline constexpr const char* kCoreFull = "coreFull";
/// The two ends share no link major.
inline constexpr const char* kLinkVersion = "linkVersion";
/// A sign-in with the pairing token on a Core that has none (a new Core,
/// or one whose token was retired): "This Core uses paired devices. Pair
/// this device first."
inline constexpr const char* kPairingRequired = "pairingRequired";
/// The pairing token was wrong.
inline constexpr const char* kWrongToken = "wrongToken";
/// A well-formed device sign-in from a key the Core has not paired.
inline constexpr const char* kDeviceNotPaired = "deviceNotPaired";
/// A device sign-in that did not prove itself: a bad signature, one over
/// another connection's challenge or another certificate, or a malformed
/// device block.
inline constexpr const char* kDeviceProofFailed = "deviceProofFailed";
/// The device was removed from the Core (Task 13's revoke).
inline constexpr const char* kDeviceRemoved = "deviceRemoved";
/// iPhone app Task 71 (ruling 4.8): the same device signed in on a newer
/// connection, which replaced this one: "This device connected again."
inline constexpr const char* kSameDevice = "sameDevice";
/// The client's own reason, never sent by a Core: the Core's certificate
/// or identity key is not the one this device paired with.
inline constexpr const char* kIdentityChanged = "identityChanged";
/// The app broke the connect sequence or sent a message the Core cannot
/// read.
inline constexpr const char* kProtocolError = "protocolError";
/// Fix wave after parity Tasks 19 and 21 (the operator's ruling of
/// 2026-09-26): the Core is restarting its run on another radio. Retryable:
/// the app reconnects by itself.
inline constexpr const char* kRadioChanging = "radioChanging";
} // namespace SessionEndCode

/// iPhone app Task 12: the Core hello's `identity`, base64url text as on
/// the wire (the link document, section 3.4).
struct SessionStationIdentity {
    QString publicKey;    // the Core's identity key, SPKI DER
    QString certBinding;  // raw r || s over "NereusSDR cert-binding v1\n" || SHA-256(cert)
};

/// iPhone app Task 12: auth.request's `device`, base64url text as on the
/// wire (the link document, section 3.5).
/// iPhone app Task 74 (R-IOS-30): the body of a confirm.request or a
/// notice (the several-devices design, sections 7.3 and 7.4; the link
/// document, section 7.3). Its `reason` rides in SessionMessage::reason.
/// The nested values (affected, change, choices, slices) are JSON the Core
/// builds and the device reads as the link document describes them.
struct SessionPrompt {
    /// Both: the question's or the notice's id, unique on this Core.
    qint64 id = 0;
    /// Both: confirm.request panMove, takeReceiver, takeSlice (Tasks 75 and
    /// 77 add sharedSetting and takeTransmit); notice sliceMoved,
    /// sliceClosed, receiverTaken, sliceTaken, transmitTaken (Task 77),
    /// graceEnded, slicesNotRestored; controlTaken (slice control plan
    /// Task 4).
    QString kind;

    // ── confirm.request ─────────────────────────────────────────────────
    /// One entry per disturbed device.
    QJsonArray affected;
    qint64 expiresInMs = 0;
    /// {label, from, to}; absent for a take.
    std::optional<QJsonObject> change;
    /// A take's list, one entry per receiver or slice.
    std::optional<QJsonArray> choices;
    std::optional<qint64> forCommandId;
    std::optional<qint64> forWriteId;
    /// Sent when not empty.
    QString forSettingsKey;
    /// iPhone app plan Task 77: takeTransmit's holder, the entry of the
    /// device (or the radio) transmit would be taken from.
    std::optional<QJsonObject> holder;

    // ── notice ──────────────────────────────────────────────────────────
    /// Whole seconds since it happened, measured when it is sent.
    qint64 secondsAgo = 0;
    bool takeBack = false;
    /// Who did it; each sent when not empty. None on a notice about the
    /// device's own state.
    QString byDeviceId;
    QString byName;
    QString byShortName;
    QString byKind;
    QString bySource;
    /// [{sliceId, letter, frequencyHz, mode, band}], closed ones included.
    std::optional<QJsonArray> slices;
};

/// iPhone app Task 14: the device pair.start names (its key as base64url of
/// SPKI DER, a name and a kind). In code mode the name and kind inside the
/// device's confirmation box win over these.
struct SessionPairDevice {
    QString publicKey;
    QString name;
    QString kind;       // "phone", "tablet", "computer"
};

struct SessionDeviceBlock {
    QString id;         // SHA-256 of the device key's SPKI DER
    QString publicKey;  // the device key, SPKI DER
    QString name;
    QString kind;       // "phone", "tablet", "computer"
    QString signature;  // raw r || s over the device-auth transcript
    /// Optional (Part C fix wave, settled with the phone session): the
    /// device's own short name, at most DeviceStore::kMaxShortNameBytes of
    /// UTF-8; "" when absent, and then not encoded. Outside the signed
    /// transcript, as `name` is.
    QString shortName;
};

/// One property's WIRE DECLARATION: name, ordinal and kind, carrying no
/// live value. This is what a Schema message announces once per class per
/// session. MirrorUpdate (MirrorSchema.h) is the analogous VALUE-carrying
/// shape ObjectCreate and Delta use.
struct SessionSchemaField {
    quint16 ordinal = 0;
    QByteArray name;
    MirrorWireKind kind = MirrorWireKind::Unsupported;
};

/// One message on the session's reliable control channel (R2 design
/// addendum section 7.3's "Control" envelope: state deltas, lifecycle
/// events, snapshot, commands). A flat, kind-tagged struct rather than a
/// class hierarchy, matching StateMirror.h's MirrorApplyResult: which
/// fields are meaningful depends on `kind`, and callers are expected to
/// go through the builder functions below rather than populate fields by
/// hand.
///
///   Schema            -- className, fields
///   ObjectCreate       -- objectKey, className, updates (the FULL bag)
///   ObjectDestroy       -- objectKey, className
///   Delta               -- objectKey, updates (the CHANGED subset)
///   SnapshotComplete    -- nothing but kind
///   CommandInvoke       -- commandVerb, commandId, arguments
///   CommandResult       -- commandVerb, commandId, accepted, reason,
///                          affectedKeys
struct SessionPropertyResult {
    QByteArray property;
    bool accepted = false;
    QString reason;
    bool hasValue = false;
    MirrorUpdate value;
};

struct SessionMessage {
    SessionMessageKind kind = SessionMessageKind::Delta;

    StationTelemetrySnapshot telemetry;

    QByteArray className;
    QList<SessionSchemaField> fields;
    QByteArray objectKey;
    QList<MirrorUpdate> updates;
    quint32 writeId = 0;
    QList<SessionPropertyResult> propertyResults;

    // ── Task 11: CommandInvoke / CommandResult only ─────────────────────

    /// Which RadioModel entry point (CommandInvoke), or which one this is
    /// the answer to (CommandResult). One of SessionCommandDispatcher's
    /// four known verb names; an unrecognised one is a dispatch-time
    /// rejection, not a decode-time one -- see that class's doc comment.
    QByteArray commandVerb;

    /// Caller-assigned, echoed back verbatim on the CommandResult. Lets a
    /// caller with more than one command in flight -- guaranteed for
    /// requestSliceSampleRate, which SessionCommandDispatcher defers to a
    /// LATER event-loop turn -- match a result to the invoke that produced
    /// it.
    quint32 commandId = 0;

    /// CommandInvoke only: named, typed parameters. Reuses MirrorUpdate as
    /// a generic {name, kind, value} triple rather than inventing a
    /// parallel encode/decode path -- `ordinal` carries no meaning here and
    /// is always 0. Per-verb argument contracts (SessionCommandDispatcher.h):
    ///   addSlice               -- {"initialPanId": Utf8}
    ///   removeSlice            -- {"sliceId": Int64}
    ///   requestSliceSampleRate -- {"sliceId": Int64, "rateHz": Int64}
    ///   addSliceOnPan          -- {"panId": Utf8}
    ///   requestStreamCtunPinned -- {"sliceId": Int64, "pinned": Bool}
    ///   configureTgxl          -- {"host": Utf8, "port": Int64}
    ///   disconnectTgxl         -- {}
    ///   configurePgxl          -- {"host": Utf8, "port": Int64}
    ///   disconnectPgxl         -- {}
    ///   setPgxlConnectionSettings -- {"autoReconnect": Bool,
    ///                               "keepaliveSec": Int64, "pingSec": Int64}
    ///   configureRfKit         -- {"host": Utf8, "port": Int64}
    ///   disconnectRfKit        -- {}
    ///   setRfKitEnabled        -- {"enabled": Bool}
    ///   resetRfKitError        -- {}
    ///   setStationTci          -- {"enabled": Bool, "port": Int64}
    ///   setStationTciOptions   -- {"emulateExpertSdr3": Bool,
    ///                               "emulateSunSdr2Pro": Bool,
    ///                               "cwluBecomesCw": Bool,
    ///                               "sendInitialState": Bool}
    ///   disconnectStationTciClient -- {"id": Utf8}
    ///   setStationTciSettings  -- one or more of {"rateLimitMs": Int64,
    ///                               "cwBecomesCwuAbove10mhz": Bool,
    ///                               "iqSwap": Bool, "alwaysStreamIq": Bool,
    ///                               "audioBlockSamples": Int64,
    ///                               "txChannel": Int64 (0 Left, 1 Right,
    ///                               2 Both), "rxSensorIntervalMs": Int64,
    ///                               "txSensorIntervalMs": Int64,
    ///                               "forgetRx2VfoBOnDisconnect": Bool,
    ///                               "useRx1VfoaForRx2Vfoa": Bool,
    ///                               "copyRx2VfobToVfoa": Bool}
    ///   setTxInterlockPolicy   -- {"mode": Int64, "graceMs": Int64,
    ///                               "swrGateEnabled": Bool, "swrGateMax": Double}
    ///   setPgxlPowerCap        -- {"enabled": Bool, "watts": Int64}
    ///   clearAccessoryFaults   -- {"device": Utf8}
    ///   setPgxlName, setTgxlName -- {"name": Utf8}
    ///   setPgxlHardware        -- exactly one of {"biasMode": Utf8},
    ///                               {"fanMode": Utf8}, {"ledIntensity": Int64}
    ///   setPgxlNetwork, setTgxlNetwork -- {"dhcp": Bool, "address": Utf8,
    ///                               "netmask": Utf8, "gateway": Utf8}
    ///   savePgxlSettings, saveTgxlSettings, readPgxlSettings,
    ///   readTgxlSettings       -- {}
    ///   requestStreamCentre     -- {"sliceId": Int64, "centreHz": Double}
    QList<MirrorUpdate> arguments;

    /// CommandResult only. True iff the command ran; false leaves every
    /// bit of state exactly as it was (same contract as
    /// MirrorApplyResult::accepted).
    bool accepted = false;

    /// CommandResult only. Empty iff accepted.
    QString reason;

    /// AuthResult and SessionEnd only: is the condition that produced this
    /// refusal TRANSIENT, so a client may sensibly back off and try again?
    ///
    /// A free-text `reason` is for the operator; this is for the client's
    /// retry policy, which must not be built on matching English prose.
    /// The distinction is load-bearing in both directions. "Station is at
    /// its concurrent-connection limit" and "too many failed
    /// authentication attempts" both clear on their own, and treating them
    /// as permanent is what let a bad actor lock an operator out with no
    /// automatic recovery. A wrong token and a protocol-major mismatch
    /// never clear, and retrying either forever is how the rate limiter
    /// gets fed.
    ///
    /// Defaults to FALSE, and decode() reads it leniently rather than
    /// requiring it: a peer built before this field existed sends no
    /// "retryable" key, and absent must mean "assume permanent", which is
    /// both the safe direction and exactly the behaviour that peer's own
    /// client half had.
    bool retryable = false;

    /// CommandResult only: which object(s) this result is about. The exact
    /// meaning is PER-VERB, not a blanket mutation-diff guarantee:
    ///
    ///   - requestSliceSampleRate: a full before/after diff -- EVERY slice
    ///     whose sampleRateHz actually changed, which is not always the
    ///     one slice the invoke named (co-hosted slices on the same DDC
    ///     stream, or a Protocol 1 board's radio-wide RadioModel::
    ///     setSampleRateLive escalation -- SessionCommandDispatcher::
    ///     handleRequestSliceSampleRate, SessionCommandDispatcher.cpp).
    ///   - addSlice / addSliceOnPan: the one newly-created slice.
    ///   - removeSlice: the one removed slice. NOT a mutation diff: removal
    ///     can leave RadioModel::requestDdcAssignment() moving a SURVIVING
    ///     slice's ddcIndex/streamIndex, which this list does not name.
    ///     Nothing is lost -- that survivor's change still reaches the
    ///     client normally, as an ordinary Delta through StateMirror's
    ///     existing outbound path -- this field is the command's own
    ///     object-level outcome, not a substitute for that path.
    ///   - setActiveSliceById: the newly-active slice, plus the previously-
    ///     active one when it differs.
    QList<QByteArray> affectedKeys;

    // ── Task 18: Hello only ─────────────────────────────────────────────

    /// The sender's own kSessionProtocolMajor / kSessionProtocolMinor.
    /// Both ends send these; see kSessionProtocolMajor's doc comment for
    /// the policy the receiver applies to them.
    quint16 protocolMajor = 0;
    quint16 protocolMinor = 0;

    /// The sender's AppSettings schema version (AppSettings::
    /// currentSchemaVersion()). Parent design section 7.0 lists it among
    /// the capability descriptor's contents but assigns the comparison to
    /// nothing; StationClient does it at handshake time. Carried on Hello
    /// rather than only in the Capabilities descriptor so BOTH ends learn
    /// it, and so it is available before authentication decides whether a
    /// descriptor is ever sent at all.
    qint32 settingsSchemaVersion = 0;

    /// Free-form identification of the sending process ("nereusd",
    /// "NereusSDR 0.5.2"). Diagnostics only: nothing gates on it.
    QString peerName;

    // ── iPhone app Task 4 (R-IOS-01): Hello only ────────────────────────

    /// The link majors the sender supports (LinkVersion.h). On the wire as
    /// `majors`; a hello without it (every peer built before Task 4)
    /// decodes as [protocolMajor]. A station's `protocolMajor` is its
    /// newest; a client's is the one it chose from the station's list.
    QList<quint16> supportedMajors;

    /// What the sender declares before capabilities are sent: feature name
    /// to whole-number version. On the wire as `features`; absent decodes
    /// as none. A name the receiver does not know is kept and never
    /// consulted.
    QHash<QByteArray, int> features;

    /// Whether `majors` and `features` travel. Set by the six-argument
    /// hello() builder and by decode() when the peer sent them, so a hello
    /// from an older peer encodes again exactly as it arrived.
    bool majorsOnWire = false;
    bool featuresOnWire = false;

    // ── iPhone app Task 12 (R-IOS-08): the Core's Hello only ────────────

    /// The Core's identity key and its binding to the TLS certificate. On
    /// the wire as `identity`; a hello without it (a client's, or an
    /// older Core's) decodes as nullopt.
    std::optional<SessionStationIdentity> stationIdentity;
    /// This connection's device sign-in challenge, base64url of 32 bytes.
    /// On the wire as `challenge` when not empty.
    QString challenge;

    // ── Task 18: AuthRequest only ───────────────────────────────────────

    /// The pre-shared token (TokenStore). NEVER logged: StationServer logs
    /// the OUTCOME of a verify, never the candidate. Empty in a device
    /// sign-in.
    QString token;

    /// iPhone app Task 12: the device sign-in block. On the wire as
    /// `device`; absent decodes as nullopt.
    std::optional<SessionDeviceBlock> device;

    // ── iPhone app plan Task 29: PathJoin only ──────────────────────────

    /// The ticket session.pathTicket gave (base64url of 32 bytes). A
    /// secret like the token: NEVER logged.
    QString pathTicket;

    // ── iPhone app Task 12: AuthResult and SessionEnd only ──────────────

    /// A SessionEndCode token, on the wire as `code` when not empty. A
    /// client that does not know the code reads `reason`.
    QString endCode;
    /// Task 41: the authenticated fifth-device question and its answer.
    QJsonArray heldDevices;
    quint32 heldRevision = 0;
    QString takeoverDeviceId;
    std::optional<QJsonObject> placeTaken;
    std::optional<QJsonObject> placeFreed;
    /// Enriched takenOver session.end.
    QString takenOverBy;
    QString takenOverById;
    std::optional<qint64> secondsAgo;

    // ── Task 18: SettingsWrite / SettingsValue only ─────────────────────

    /// SettingsProxy::localOriginTag() -- a per-SESSION identifier, not a
    /// per-write sequence number. The daemon echoes it back verbatim on the
    /// resulting SettingsValue broadcast so a client can tell its own echo
    /// from a third party's change. See SettingsProxy.h's origin-tag
    /// paragraph.
    QString originTag;

    // ── iPhone app Task 14: the pair.* kinds ───────────────────────────

    /// PairStart: "lan" (one tap) or "code".
    QString pairMode;
    /// PairStart: the device asking to pair.
    std::optional<SessionPairDevice> pairDevice;
    /// PairAccept: the Core's label (its identity rides in stationIdentity).
    QString pairLabel;
    /// PairSpake: the step, 0 to 3, and its bytes as base64url.
    int pairStep = 0;
    QString pairData;
    /// PairConfirm: the box as base64url (a 24-byte nonce, then the
    /// ciphertext).
    QString pairBox;
    /// PairFail: milliseconds until trying again makes sense (0: now, or
    /// never with this Core as it stands). Its reason rides in `reason`.
    qint64 retryAfterMs = 0;

    /// R3 MediaControl only. Individual media operations validate their own
    /// fields after the authenticated, snapshot-ready session gate. This
    /// envelope carries signalling/subscriptions, never audio or FFT arrays.
    QJsonObject mediaPayload;

    /// iPhone app Task 74: ConfirmRequest and Notice only.
    SessionPrompt prompt;

    /// Parity Task 19: RecordBatch only.
    NereusSDR::RecordBatch recordBatch;
};

/// Builders plus the JSON codec. A static-method utility class with no
/// per-instance state, matching MirrorSchema's own shape.
class SessionMessages {
public:
    static SessionMessage schema(const QByteArray& className,
                                 const QList<SessionSchemaField>& fields);
    static SessionMessage objectCreate(const QByteArray& objectKey,
                                       const QByteArray& className,
                                       const QList<MirrorUpdate>& fullBag);
    static SessionMessage objectDestroy(const QByteArray& objectKey,
                                        const QByteArray& className);
    static SessionMessage delta(const QByteArray& objectKey,
                                const QList<MirrorUpdate>& changed);
    static SessionMessage snapshotComplete();

    /// Task 11. `verb` is one of SessionCommandDispatcher's known verb
    /// names (see SessionMessage::commandVerb's doc comment for the
    /// per-verb argument contract); this builder does not validate it --
    /// an unrecognised verb decodes and encodes just fine, and is rejected
    /// at dispatch time instead, matching decode()'s existing division of
    /// labour between structural validity and semantic validity.
    static SessionMessage commandInvoke(const QByteArray& verb, quint32 commandId,
                                        const QList<MirrorUpdate>& arguments);

    /// `affectedKeys` is the ACTUAL scope, not the requested one -- see
    /// SessionMessage::affectedKeys.
    static SessionMessage commandResult(const QByteArray& verb, quint32 commandId,
                                        bool accepted, const QString& reason,
                                        const QList<QByteArray>& affectedKeys,
                                        const QList<MirrorUpdate>& values = {});

    // ── Task 18 builders ────────────────────────────────────────────────

    /// First message either end sends after the TLS handshake completes.
    /// This form is today's hello, without `majors` or `features`.
    static SessionMessage hello(quint16 major, quint16 minor,
                                qint32 settingsSchemaVersion,
                                const QString& peerName);

    /// iPhone app Task 4: the hello with the sender's supported majors and
    /// declared features, both always on the wire (an empty `features` is
    /// sent as {}; an empty `supportedMajors` is sent as [major], because
    /// the wire never carries an empty `majors`).
    static SessionMessage hello(quint16 major, quint16 minor,
                                qint32 settingsSchemaVersion,
                                const QString& peerName,
                                const QList<quint16>& supportedMajors,
                                const QHash<QByteArray, int>& features);

    /// Client to daemon, once the daemon's Hello has been accepted.
    static SessionMessage authRequest(const QString& token);

    /// iPhone app Task 12: a device sign-in (`token` empty), or a window
    /// signing in with the pairing token and enrolling its device key in
    /// the same step (`token` set).
    static SessionMessage authRequest(const QString& token, const SessionDeviceBlock& device);

    /// Daemon to client. A false `accepted` is always followed by the
    /// daemon closing the socket; `reason` is what the operator sees and
    /// `retryable` is what the client's reconnect policy reads. Both are
    /// required rather than defaulted, so a refusal cannot be added
    /// without someone deciding which kind it is. See
    /// SessionMessage::retryable.
    static SessionMessage authResult(bool accepted, const QString& reason,
                                     bool retryable);
    /// iPhone app Task 12: a refusal with its SessionEndCode.
    static SessionMessage authResult(bool accepted, const QString& reason, bool retryable,
                                     const QString& endCode);

    /// Daemon to client, after a successful AuthResult. `descriptor` is
    /// StationCapabilities::toUpdates() -- MirrorUpdate reused as a generic
    /// {name, kind, value} triple, the same way CommandInvoke reuses it for
    /// arguments, rather than inventing a parallel encode path. `ordinal`
    /// carries no meaning here and is always 0.
    static SessionMessage capabilities(const QList<MirrorUpdate>& descriptor);

    /// Daemon to client: this session is over, and why. Sent for a version
    /// refusal, and for the incumbent session when a second authenticated
    /// connection preempts it (parent design section 7.1: "The displaced
    /// session is told why"). `retryable` is required for the same reason
    /// it is on authResult(); see SessionMessage::retryable.
    static SessionMessage sessionEnd(const QString& reason, bool retryable);
    /// iPhone app Task 12: an end with its SessionEndCode.
    static SessionMessage sessionEnd(const QString& reason, bool retryable,
                                     const QString& endCode);
    static SessionMessage sessionHeld(const QJsonArray& devices, quint32 revision,
                                      std::optional<QJsonObject> placeTaken = std::nullopt,
                                      std::optional<QJsonObject> placeFreed = std::nullopt);
    static SessionMessage sessionTakeover(const QString& deviceId, quint32 revision);

    /// Client to daemon: apply these property values to this object. The
    /// mirror-image of a Delta, deliberately a DISTINCT kind rather than a
    /// reused Delta so direction is explicit on the wire and a daemon can
    /// refuse a Delta outright -- a peer must never be able to tell a
    /// daemon what its own state IS, only what it should be CHANGED to,
    /// and those two readings of one message shape are exactly the
    /// ambiguity worth spending a kind name to avoid.
    static SessionMessage propertyWrite(const QByteArray& objectKey,
                                        const QList<MirrorUpdate>& updates,
                                        quint32 writeId = 0);
    static SessionMessage propertyResult(const QByteArray& objectKey,
                                         quint32 writeId,
                                         const QList<SessionPropertyResult>& results);

    /// Daemon to client: the connect-time settings snapshot
    /// (SettingsProxyServer::buildSnapshot()). Each entry's `name` is the
    /// AppSettings key and its `kind` is always Utf8, matching
    /// AppSettings's own flat QString-valued store.
    static SessionMessage settingsSnapshot(const QList<MirrorUpdate>& entries);

    /// Client to daemon: one Station-classified key changed here, please
    /// apply it there.
    static SessionMessage settingsWrite(const QString& key, const QString& value,
                                        const QString& originTag);

    /// Client to daemon: remove one Station-classified key.
    static SessionMessage settingsRemove(const QString& key);

    /// Daemon to client: this key's current value, broadcast to every
    /// connected client (SettingsProxyServer::outboundValueChanged).
    static SessionMessage settingsValue(const QString& key, const QString& value,
                                        const QString& originTag);

    /// Daemon to client: this key is GONE from the station's store
    /// (SettingsProxyServer::outboundValueRemoved), as distinct from
    /// holding an empty string.
    ///
    /// Whole-branch review, Important 4. The same SettingsValue kind with
    /// an EMPTY entry list, which is exactly how settingsReject() below
    /// already encodes "the daemon has nothing for this key either" --
    /// one absence convention on this wire, not two. A peer built before
    /// this existed decodes the frame fine (the codec requires the
    /// properties array to be present, and empty is present) and its
    /// handler ignores an entry-less settings.value, so it keeps its last
    /// known value rather than acquiring the empty-string ghost this
    /// replaces. Strictly better than the old behaviour, and not a
    /// version break.
    static SessionMessage settingsValueAbsent(const QString& key, const QString& originTag);

    /// Daemon to client: a SettingsWrite was refused. `hasRestoredValue`
    /// false means the daemon has nothing for this key either (proven
    /// unset, which SettingsProxy::applyRejection() distinguishes from a
    /// restored empty string) and is encoded as an EMPTY entry list rather
    /// than an entry carrying an empty value.
    // ── iPhone app Task 14: pairing ─────────────────────────────────────

    static SessionMessage pairStart(const QString& mode, const SessionPairDevice& device);
    static SessionMessage pairAccept(const SessionStationIdentity& identity,
                                     const QString& label);
    static SessionMessage pairSpake(int step, const QString& data);
    static SessionMessage pairConfirm(const QString& box);
    /// `reason` is what the operator reads; `retryAfterMs` is when trying
    /// again makes sense.
    static SessionMessage pairFail(const QString& reason, qint64 retryAfterMs);

    /// iPhone app Task 74 (R-IOS-30): Core to device, a question before a
    /// change that reaches another device, or a take.
    static SessionMessage confirmRequest(const SessionPrompt& prompt, const QString& reason);
    /// iPhone app Task 74 (R-IOS-30): Core to device, what another device
    /// did to it, or what happened to its own state.
    static SessionMessage notice(const SessionPrompt& prompt, const QString& reason);
    /// Parity Task 19 (R-IOS-25): Core to peer, one stream's upserts and
    /// removes, or its reset.
    static SessionMessage recordBatch(const NereusSDR::RecordBatch& batch);
    /// iPhone app plan Task 29 (R-IOS-16): device to Core on a new
    /// connection, in place of auth.request (the link document, section
    /// 21.2).
    static SessionMessage pathJoin(const QString& ticket);
    /// iPhone app plan Task 29: both ways on the old connection, the last
    /// message an end sends there.
    static SessionMessage pathSwitch();

    static SessionMessage settingsReject(const QString& key, bool hasRestoredValue,
                                         const QString& restoredValue,
                                         const QString& reason = {});

    /// UTF-8 JSON text. See the file header for the Int64/Enum precision
    /// note.
    static QByteArray encode(const SessionMessage& message);

    /// False (leaving *out untouched) for malformed JSON, an unrecognised
    /// "type", or a value that will not decode against the wire kind it
    /// claims. Never throws and never asserts on untrusted input -- this
    /// is the far side of a socket a remote peer controls.
    static bool decode(const QByteArray& wire, SessionMessage* out);

    /// The wire token for each SessionMessageKind ("object.create", not
    /// "ObjectCreate") and the reverse lookup. Exposed so Task 11 can
    /// extend the same table rather than inventing a second one.
    static QByteArray kindName(SessionMessageKind kind);
    static bool kindFromName(const QByteArray& name, SessionMessageKind* out);

    /// Every SessionMessageKind enumerator, in declaration order. The link
    /// surface (tests/LinkSurface.cpp, R-IOS-01) lists the wire kinds from
    /// this; tst_link_surface_manifest scans the enum's declaration so an
    /// enumerator added here without being listed fails the build's tests.
    static QList<SessionMessageKind> allKinds();

    static QByteArray wireKindName(MirrorWireKind kind);
    static bool wireKindFromName(const QByteArray& name, MirrorWireKind* out);
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::SessionMessageKind)
Q_DECLARE_METATYPE(NereusSDR::SessionSchemaField)
Q_DECLARE_METATYPE(NereusSDR::SessionMessage)
