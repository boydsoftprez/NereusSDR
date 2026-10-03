#pragma once
// =================================================================
// src/core/session/SessionCommandDispatcher.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 11.
//
// Turns a decoded CommandInvoke SessionMessage into the corresponding
// RadioModel call, and reports back what ACTUALLY happened as a
// CommandResult (SessionMessages.h). This is the production half of "in-
// process dispatch" tst_session_verbs.cpp exercises over
// tests/fakes/LoopbackStationLink.h; Task 18 is what feeds a real wss
// session's inbound bytes into dispatch() and relays commandResultReady()
// back out.
//
// Five verbs. Four matching the R2 plan's Task 11 step 2 exactly
// (RadioModel.h, this tree's HEAD at commit 998e7854), plus a fifth added
// in fix round 1 after review found the plan's own justification for
// leaving it out ("active rides the ordinary delta path") was true
// OUTBOUND only -- SliceModel::active has no WRITE at all, so a remote
// peer had no path to move it (see SliceModel.cpp's applyMirroredValue,
// which now names this verb instead of the unreachable
// RadioModel::setActiveSlice()):
//
//   addSlice               -- RadioModel.h:775   int addSlice(QString)
//   removeSlice             -- RadioModel.h:779   void removeSlice(int)
//   requestSliceSampleRate  -- RadioModel.h:720   void requestSliceSampleRate(int, int)
//   addSliceOnPan           -- RadioModel.h:813   Q_INVOKABLE void addSliceOnPan(QString)
//   setActiveSliceById      -- RadioModel.h:804   bool setActiveSliceById(int)
//
// Nothing here writes a mirrored PROPERTY -- that is StateMirror::
// applyInbound()'s job (Task 8). This class exists for the five RadioModel
// entry points MirrorPolicy has no property to gate at all: creating,
// destroying or activating a slice is not a value change on an existing
// object's WRITE accessor, and requestSliceSampleRate is the one case
// (StateMirror.cpp's kVerbHints table, :69) where a mirrored property
// (SliceModel::sampleRateHz) is deliberately Outbound-only specifically
// BECAUSE this verb is how a client changes it.
//
// ── THREADING ────────────────────────────────────────────────────────────
//
// dispatch() must be called on RadioModel's own thread. Task 18's session
// read loop -- whatever decodes bytes off the wss socket into a
// SessionMessage and calls dispatch() -- therefore also runs on that same
// thread, not a dedicated I/O thread. This is not a new constraint this
// class invents: it is the same single-thread precondition StateMirror.h
// already documents for attachSession() and for the m_applying inbound-
// echo guard (StateMirror.cpp) -- both rely on Qt::AutoConnection
// resolving to a direct call, which is only true while sender and
// receiver share a thread. Splitting the session onto its own thread
// later is Task 18's call to make, but if it does, every watched model,
// StateMirror, ObjectRegistry AND this dispatcher all have to move
// together, or all three invariants break at once.
//
// ── requestSliceSampleRate IS THE ONE ASYNCHRONOUS VERB ─────────────────
//
// RadioModel::requestSliceSampleRate can reach RadioModel::setSampleRateLive
// (RadioModel.cpp), whose own doc comment measures at least 40 ms of
// QThread::msleep across three calls. Since dispatch() runs on the model
// thread (above), calling that synchronously from inside dispatch() would
// stall the session read loop -- and therefore every OTHER inbound
// message, command or otherwise -- for the duration. handleRequestSliceSampleRate
// defers the actual RadioModel call to a LATER turn of that same thread's
// event loop via QMetaObject::invokeMethod(..., Qt::QueuedConnection), so
// dispatch() itself always returns immediately. Its CommandResult is only
// emitted once that queued call has actually run -- see
// commandResultReady()'s doc comment. The other three verbs have no such
// hazard and both run AND report synchronously, within the dispatch() call
// that requested them.
//
// ── SCOPE, NOT JUST SUCCESS ──────────────────────────────────────────────
//
// requestSliceSampleRate names one slice, but RadioModel can retune more
// than that slice in response to it: co-hosted slices share one DDC
// stream, so any rate change to that stream moves every slice on it, and
// on a Protocol 1 board requestSliceSampleRate can escalate all the way to
// RadioModel::setSampleRateLive's own 12-step, radio-wide sequence
// (RadioModel.cpp, sampleRateIsRadioWide()/setStreamSampleRate) -- every
// slice's sampleRateHz moves, not only the one named in the request.
// handleRequestSliceSampleRate does not special-case either mechanism: it
// snapshots every known slice's sampleRateHz before calling RadioModel,
// calls it, and reports whichever slices' values actually differ
// afterward as the CommandResult's affectedKeys. This is deliberately
// protocol- and topology-agnostic -- it reports what happened, not why.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: startLevelCalibration and
//                                    cancelLevelCalibration. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: resetLevelCalibration
//                                    (radioHardwareVersion 12). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11: command
//                                    dispatch (addSlice / removeSlice /
//                                    requestSliceSampleRate /
//                                    addSliceOnPan). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11 fix round 1:
//                                    added setActiveSliceById (review
//                                    Important 1 -- a remote operator's
//                                    active-slice click had no path to the
//                                    daemon). AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 1: an
//                                    integer argument outside int's range
//                                    is now refused rather than truncated
//                                    (see dispatch()). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01):
//                                    verbSpecs(), the declared table of
//                                    every verb dispatch() routes, its
//                                    arguments and the capability that
//                                    advertises it. Routing is unchanged.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-47: the Tuner Genius's antenna,
//                                    operate and bypass verbs.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08):
//                                    devices.revoke, station.rename,
//                                    station.acknowledgeKeyBackup and
//                                    station.retireToken, routed to the
//                                    Core's StationDevicesFacade.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08):
//                                    pairing.open and pairing.close, routed
//                                    to the same facade.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2):
//                                    setTunePowerForTxBand.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the
//                                    txProfile verbs and rade.resetVocoder.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): session.leave and
//               sessionLeaveRequested. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 74 (R-IOS-30): confirm.proceed,
//               confirm.cancel, notice.takeBack (setConfirmAnswer).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25: iPhone app Task 72 (R-IOS-02, ruling 5.8): the owner is
//               per session (setSessionOwner before each dispatch,
//               endSessionOwner, resetSessionState). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 34 (R-IOS-02):
//                                    tx.setTxSlice and the on-air refusals
//                                    (setTransmitAccess), refusals with
//                                    refusalCode and refusalFix values.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 35 (R-IOS-13):
//                                    tx.key, tx.unkey, tx.tune and
//                                    tx.twoTone (TransmitAccess::keying);
//                                    an accepted key carries its epoch.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan Task 37 (R-IOS-13):
//                                    tx.keepalive {sequence, epoch}
//                                    (TransmitAccess::keepalive).
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: slice.selectBand.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: notch.addAtSlice.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 8): moveTgxlRelay,
//                                    scanTgxlLan and setTgxlAddress.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9): setPgxlOperate,
//                                    scanPgxlLan and setPgxlAddress.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10):
//                                    setRfKitOperate, setRfKitAntenna,
//                                    setRfKitTciMode and setRfKitAddress.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-46 (parity Task 14):
//                                    handleRequestIoBoardI2c and
//                                    handleSetIoBoardOutput.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: M2 TransmitAccess::release, a
//               two-tone stop from another device refused. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 16):
//                                    handleFilterResponse.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13): tx.take and
//                                    tx.tunerTune; TransmitAccess::take and
//                                    ::transmitter. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  R-R3-49 / R-IOS-18 (remote-window
//                                    parity Task 22, iPhone plan Task 25):
//                                    support.collect (the bundle made on a
//                                    worker thread) and
//                                    support.setLogCategories.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-IOS-13 / R-R3-49: txEq.setCurve and
//                                    txEq.resetCurve (txEqCurveVersion 2)
//                                    through TxEqCurveAccess. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  setTransmitSettingsOnAir
//                                    (transmitSettingsVersion 13).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  refusedForTheHolder (ruling 7.7).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18 (paProfileVersion 1): the paProfile
//                 verbs (handlePaProfile). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  cfc.setProfile
//                                    (transmitSettingsVersion 15) through
//                                    CfcProfileAccess. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 2: the
//                                    slice access check is the change
//                                    predicate (SliceAccessPolicy), so a
//                                    listener's verbs are refused.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 4:
//                                    slice.listen, slice.stopListening,
//                                    slice.takeControl and slice.release
//                                    (SliceAccessController);
//                                    setActiveSliceById on a listened
//                                    slice, and removeSlice from a
//                                    controller others listen with as a
//                                    release (ruling Q6).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control fix wave (Important 4):
//                                    TransmitAccess::txSliceChosen, a
//                                    device's explicit tx.setTxSlice
//                                    choice. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 6:
//                                    handleSliceListenLevel for
//                                    slice.setListenLevel. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QHash>
#include <QSet>

#include <functional>
#include <optional>
#include <utility>

#include "core/SupportBundle.h"
#include "core/session/RemoteKeying.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationDevicesFacade.h"
#include "core/safety/TxRefusal.h"

namespace NereusSDR {

class RadioModel;
class SliceAccessController;
class SliceModel;
class StationRadios;

/// One named argument of a command verb, as dispatch() reads it: the name,
/// the wire kind it must carry, and whether it may be left out.
struct CommandArgumentSpec {
    QByteArray name;
    MirrorWireKind kind = MirrorWireKind::Unsupported;
    bool optional = false;
};

/// One verb dispatch() accepts. `capability` names the StationCapabilities
/// entry (its toUpdates() name) that advertises the verb and
/// `capabilityVersion` the lowest value of it that does; an empty
/// capability means the verb predates capability gating. `minMinor` is the
/// lowest agreed session minor a client must hold before sending it.
struct CommandVerbSpec {
    QByteArray verb;
    QList<CommandArgumentSpec> arguments;
    QByteArray capability;
    int capabilityVersion = 0;
    quint16 minMinor = 0;
};

class SessionCommandDispatcher : public QObject {
    Q_OBJECT

public:
    using Ps3DisplayAdmissionHandler = std::function<bool(bool, QString*)>;
    void setPs3DisplayAdmissionHandler(Ps3DisplayAdmissionHandler handler)
    { m_ps3DisplayAdmission = std::move(handler); }
    /// `radioModel` is watched via QPointer, not owned: this class outlives
    /// or is outlived by it depending on which a future session's lifetime
    /// (Task 18) ties to which, and neither ordering should crash.
    explicit SessionCommandDispatcher(RadioModel* radioModel, QObject* parent = nullptr);

    /// Decode `invoke`'s verb and act on it.
    ///
    /// A malformed request (`invoke.kind` is not CommandInvoke, no
    /// RadioModel attached, an unrecognised commandVerb, a missing
    /// argument, or an integer argument outside the range RadioModel's
    /// own entry point can hold) produces an immediate, synchronous
    /// rejection via commandResultReady() before this call returns. The
    /// range check applies to requestSliceSampleRate too, so an
    /// out-of-range argument there is answered synchronously and its
    /// deferral (see the class comment) is never entered. A well-formed command's result may also arrive
    /// synchronously (addSlice, removeSlice, addSliceOnPan) or on a LATER
    /// turn of RadioModel's event loop (requestSliceSampleRate -- see the
    /// class comment). Callers must not assume commandResultReady() has
    /// fired by the time dispatch() itself returns.
    void dispatch(const NereusSDR::SessionMessage& invoke);

    /// Check a radio-bound Alex row before shared-setting classification and
    /// again on the RadioModel owner thread at application.
    static QString radioAntennaRowRefusal(const NereusSDR::SessionMessage& invoke,
                                          const RadioModel* radioModel);

    /// iPhone app Task 72 (ruling 5.8): the session the dispatches that
    /// follow act for, as `station:<sessionId>`. The Core sets it before
    /// each dispatch, so a DSP-asset job belongs to the device that started
    /// it and one device leaving cancels only its own. Nothing else
    /// changes.
    void setSessionOwner(const QString& owner);
    /// R-R3-49 (parity Task 7): the session's peer was offered
    /// transmitSettingsVersion 7, so ps3.single, ps3.automatic,
    /// ps3.applyCurrent and ps3.restoreCorrection are taken from it while
    /// the radio is off the air. False for every other peer, and again
    /// whenever the session owner changes.
    void setPureSignalArmingOffered(bool offered) { m_pureSignalArmingOffered = offered; }
    /// Remote parity on the air (transmitSettingsVersion 13): the session's
    /// peer may change the transmit settings, so setTunePowerForTxBand,
    /// txProfile.*, rade.resetVocoder, the PureSignal arming verbs and
    /// tx.twoTonePreset are taken from it while the radio is on the air, as
    /// a local window takes them. False for every other peer and between
    /// dispatches.
    void setTransmitSettingsOnAir(bool taken) { m_transmitSettingsOnAir = taken; }

    /// Cancels every DSP-asset job `owner` started (its session ended).
    void endSessionOwner(const QString& owner);

    /// Fix wave I1: the session owner the result being emitted by
    /// commandResultReady() answers. A result emitted inside dispatch()
    /// belongs to that dispatch's owner; one that arrives on a later turn
    /// (requestSliceSampleRate, a PureSignal action's later phases) belongs
    /// to the owner of the dispatch that started it, even when another
    /// session's dispatch is running. Read it inside a commandResultReady()
    /// handler only. Command ids are counted per client, so the owner, not
    /// the verb and id, names who asked.
    QString resultOwner() const
    { return m_resultOwner ? *m_resultOwner : m_sessionOwner; }

    /// Forgets pending PureSignal actions and returns every slice to normal
    /// NNR audio. The Core calls it when the session media goes to starts
    /// or ends: a new session never replays a prior test signal.
    void resetSessionState();

    /// iPhone app Task 13: the Core's device administration, which the
    /// devices.* and station.* verbs act on. Not owned. Without one those
    /// verbs are refused in plain words. They need no radio.
    void setDeviceAdmin(StationDevicesFacade* devices) { m_deviceAdmin = devices; }

    /// iPhone app Task 73 (rulings 5.9, 5.10): the device the dispatches
    /// that follow act for, by the id its slices are owned under (the Core
    /// sets it with the session owner and clears it after). Empty acts for
    /// nobody in particular, as before several devices: no slice is refused,
    /// a new slice has no owner and setActiveSliceById moves the one active
    /// slice.
    void setRequester(const QByteArray& device) { m_requester = device; }
    /// Slice control plan Task 4: the listen, stop listening, take control
    /// and release checks (StationServer's). Not owned. Without one
    /// slice.listen, slice.stopListening, slice.takeControl and
    /// slice.release are refused in plain words.
    void setSliceAccessController(SliceAccessController* controller);
    /// Slice control plan Task 4: whether the requester of the dispatches
    /// that follow shares slices (sliceAccessVersion 1 reached it): its
    /// setActiveSliceById may name any slice it listens to. The Core sets
    /// it with the requester and clears it after.
    void setRequesterSharesSlices(bool shares) { m_requesterSharesSlices = shares; }
    /// Fix wave after the several-devices group review: the next
    /// requestSliceSampleRate dispatched (a confirmed rate change) closes
    /// `closing` through `close`, and only once the change is certain
    /// (RadioModel::setStreamSampleRateClosing). Used by that one dispatch
    /// and cleared, whether it runs or not.
    ///
    /// Fix wave 2 (Important 4): `closing` maps each slice id to its owner
    /// (SliceOwnership::Mark::subject()) at the proceed. The change runs on
    /// a later turn; if any of those slices is gone or has another owner
    /// then (closed, and its id reused), the whole change is refused with
    /// `changedReason` and nothing closes.
    ///
    /// Fix wave 3 (Important 1): the owner alone cannot tell a reused id
    /// that stayed with the same device (or with nobody), so each entry
    /// also carries the slice itself; the change runs only when the id
    /// still names that same slice object.
    struct ClosingSlice {
        QPointer<SliceModel> slice;
        QByteArray owner;
    };
    void setRateClosing(QHash<int, ClosingSlice> closing, std::function<void(int)> close,
                        QString changedReason = {})
    {
        m_rateClosing = std::move(closing);
        m_rateClose = std::move(close);
        m_rateChangedReason = std::move(changedReason);
    }
    /// The plain refusal for `requester` naming `sliceId`, or empty when it
    /// may: "That slice belongs to <owner>. It can be changed only there."
    /// Slice control plan Task 2: the change predicate
    /// (SliceAccessPolicy::mayChange through StationServer::changeRefusal),
    /// so a device that only listens to a slice is refused too.
    using SliceAccess = std::function<QString(const QByteArray& requester, int sliceId)>;
    void setSliceAccess(SliceAccess access) { m_sliceAccess = std::move(access); }

    /// iPhone app plan Task 34 (R-IOS-02, R-IOS-13; ruling 7.4, D60): the
    /// Core's transmit rules for the requester. onAir: the on-air refusal
    /// for a change from `requester` while another device's holder is on
    /// the air, or empty. txSlice: the refusal for tx.setTxSlice from
    /// `requester` (the holder's verb, ruling 8.10), or empty. Unset, no
    /// verb is refused by either rule.
    /// Task 35 (R-IOS-13): keying: the Core's answer to tx.key, tx.unkey,
    /// tx.tune and tx.twoTone (RemoteKeying::handle). Unset, those verbs
    /// are refused stationReceiveOnly and nothing keys.
    /// Task 36: the answer comes through the reply, at once or later (a key
    /// waiting for its microphone buffer); a later one is emitted then, and
    /// the Core routes it by verb and id to the session that asked.
    struct TransmitAccess {
        std::function<TxRefusal(const QByteArray& requester)> onAir;
        std::function<TxRefusal(const QByteArray& requester)> txSlice;
        /// Slice control fix wave (Important 4): tx.setTxSlice from
        /// `requester` was accepted for `sliceId`, its explicit choice.
        std::function<void(const QByteArray& requester, int sliceId)> txSliceChosen;
        std::function<void(const RemoteKeying::Command& command, RemoteKeying::Reply reply)>
            keying;
        /// Task 37 (R-IOS-13): tx.keepalive {sequence, epoch} from
        /// `requester`, for the transmit watchdog. Unset, the verb is
        /// refused stationReceiveOnly.
        std::function<void(const QByteArray& requester, quint64 sequence, quint32 epoch)>
            keepalive;
        /// Fix wave M2 (ruling 8.5): a release of the transmission (the
        /// PureSignal two-tone test off) from `requester`: empty when it may
        /// (it holds transmit, or nobody does), or the refusal naming the
        /// holder. Unset, every release is allowed.
        std::function<TxRefusal(const QByteArray& requester)> release;
        /// iPhone app plan Task 77 (rulings 8.4, 8.7): tx.take from the
        /// device being dispatched, its optional holderEpoch and
        /// shownKeyed read. `reply` runs once with the command.result, now
        /// or when the transfer ends. Unset, tx.take is refused
        /// stationReceiveOnly.
        std::function<void(const SessionMessage& invoke, std::optional<quint64> holderEpoch,
                           std::optional<bool> shownKeyed,
                           std::function<void(const SessionMessage& result)> reply)>
            take;
        /// iPhone app plan Task 77 (ruling 7.7): a change to the
        /// transmitter's own settings (txProfile.select) from `requester`:
        /// empty when it may (it holds transmit, or nobody does), or the
        /// refusal naming the holder. Unset, every change is allowed.
        std::function<TxRefusal(const QByteArray& requester)> transmitter;
        /// Task 42: session-only transmit admission for accessory changes;
        /// the idle holder is handled by the shared-setting question.
        std::function<TxRefusal(const QByteArray& requester)> accessory;
        /// R-R3-49 / R-IOS-27: whether `requester` holds transmit, for the
        /// PA profile verbs on the air (RadioModel::paOnAirEditRefusal).
        /// Unset, no requester holds it.
        std::function<bool(const QByteArray& requester)> holdsTransmit;
    };
    void setTransmitAccess(TransmitAccess access) { m_transmitAccess = std::move(access); }
    /// iPhone app Task 74 (R-IOS-30): the Core's confirm step, which
    /// answers confirm.proceed (`choice` -1 when the kind has none),
    /// confirm.cancel and notice.takeBack with the command.result to send.
    using ConfirmAnswer =
        std::function<SessionMessage(const SessionMessage& invoke, int id, int choice)>;
    void setConfirmAnswer(ConfirmAnswer answer) { m_confirmAnswer = std::move(answer); }

    /// R-IOS-01: every verb dispatch() routes, declared beside the routing
    /// rather than derived from it. A family routed by prefix ("ps3.",
    /// "dspAssets.", "notch.") lists each concrete verb it accepts.
    /// tst_link_surface_manifest scans this file's routing and each
    /// family's handler and fails when the two disagree.
    /// Parity Task 19 (R-IOS-25): records.subscribe and records.unsubscribe
    /// belong to the connection that sent them. The Core's station server
    /// answers them (and sends the backlog) through this; true when it did.
    using RecordAccess = std::function<bool(const NereusSDR::SessionMessage& invoke)>;
    void setRecordAccess(RecordAccess access) { m_recordAccess = std::move(access); }
    /// R-IOS-13 / R-R3-49 (txEqCurveVersion 2): txEq.setCurve and
    /// txEq.resetCurve are the asking connection's txEqParaEqData write,
    /// so the station server applies and answers them; true when it did.
    using TxEqCurveAccess = std::function<bool(const NereusSDR::SessionMessage& invoke)>;
    void setTxEqCurveAccess(TxEqCurveAccess access) { m_txEqCurveAccess = std::move(access); }
    /// transmitSettingsVersion 15: cfc.setProfile is the asking
    /// connection's cfcParaEqData write, so the station server applies and
    /// answers it; true when it did.
    using CfcProfileAccess = std::function<bool(const NereusSDR::SessionMessage& invoke)>;
    void setCfcProfileAccess(CfcProfileAccess access) { m_cfcProfileAccess = std::move(access); }
    /// Parity Task 21 (R-IOS-18): the Core's radios (nereusd), for the
    /// station.selectRadio, station.rescanRadios, station.setRadioModel and
    /// station.forgetRadio verbs.
    void setStationRadios(StationRadios* radios);
    /// Parity Task 22 / the iPhone app plan's Task 25 (R-R3-49, R-IOS-18):
    /// what support.collect's bundle is made from, read on the main thread
    /// (the station server adds its telemetry and nereusd's configuration
    /// file). Without one the dispatcher reads the model alone.
    using SupportInputs = std::function<SupportBundle::Inputs()>;
    void setSupportInputs(SupportInputs inputs) { m_supportInputs = std::move(inputs); }

    static const QList<CommandVerbSpec>& verbSpecs();

signals:
    /// Every CommandResult this dispatcher produces, in answer to some
    /// earlier dispatch() call. Task 18 encodes and relays each one back
    /// out over the wire, the same relationship StateMirror::
    /// sessionMessageReady() already has to its own outbound stream.
    void commandResultReady(const NereusSDR::SessionMessage& result);
    /// iPhone app Task 71 (ruling 4.12, sessionHolderVersion 1): the
    /// session being dispatched asked to leave; its accepted result has just
    /// been emitted. StationServer frees the device's place at once, with no
    /// away state, and ends the connection.
    void sessionLeaveRequested();

private:
    Ps3DisplayAdmissionHandler m_ps3DisplayAdmission;
    bool m_pureSignalArmingOffered = false;
    bool m_transmitSettingsOnAir = false;
    void handleAddSlice(const NereusSDR::SessionMessage& invoke);
    void handleRemoveSlice(const NereusSDR::SessionMessage& invoke);
    void handleRequestSliceSampleRate(const NereusSDR::SessionMessage& invoke);
    void handleAddSliceOnPan(const NereusSDR::SessionMessage& invoke);
    void handleSetActiveSliceById(const NereusSDR::SessionMessage& invoke);
    void handleRequestStreamCtunPinned(const NereusSDR::SessionMessage& invoke);
    void handleRequestStreamCentre(const NereusSDR::SessionMessage& invoke);
    void handleConfigureTgxl(const NereusSDR::SessionMessage& invoke);
    void handleDisconnectTgxl(const NereusSDR::SessionMessage& invoke);
    void handleSetFourO3AEnabled(const NereusSDR::SessionMessage& invoke);
    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 2): the Core's Power
    // Genius XL, as configureTgxl / disconnectTgxl, plus its connection
    // settings.
    void handleConfigurePgxl(const NereusSDR::SessionMessage& invoke);
    void handleDisconnectPgxl(const NereusSDR::SessionMessage& invoke);
    void handleSetPgxlConnectionSettings(const NereusSDR::SessionMessage& invoke);
    // R-R3-47 / R-R3-22 (remoteRfKitControlVersion 2): the Core's RF-Kit
    // RF2K-S, and its switch.
    void handleConfigureRfKit(const NereusSDR::SessionMessage& invoke);
    void handleDisconnectRfKit(const NereusSDR::SessionMessage& invoke);
    void handleSetRfKitEnabled(const NereusSDR::SessionMessage& invoke);
    // I4 (R-R3-47, remoteRfKitControlVersion 3): Reset amp error.
    void handleResetRfKitError(const NereusSDR::SessionMessage& invoke);
    // R-R3-48 (stationTciVersion 1): the station's TCI switch and port.
    void handleSetStationTci(const NereusSDR::SessionMessage& invoke);
    // Parity Task 23 (stationTciVersion 2): setStationTciOptions and
    // disconnectStationTciClient.
    void handleStationTciServer(const NereusSDR::SessionMessage& invoke);
    // JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1).
    void handleSetStationTciSettings(const NereusSDR::SessionMessage& invoke);
    // R-R3-47 / R-R3-22 (accessoryDataVersion 1): the Core's accessory
    // records and settings.
    void handleSetTxInterlockPolicy(const NereusSDR::SessionMessage& invoke);
    void handleSetPgxlPowerCap(const NereusSDR::SessionMessage& invoke);
    void handleClearAccessoryFaults(const NereusSDR::SessionMessage& invoke);
    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 3, remoteTgxlControlVersion
    // 1): the amp's and tuner's own settings, sent by the Core to the device
    // as the local Advanced page's own commands.
    void handleAccessoryDeviceSettings(const NereusSDR::SessionMessage& invoke);
    // R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): the Tuner Genius's
    // antenna, operate and bypass, through the Core's own TunerModel.
    void handleTgxlControl(const NereusSDR::SessionMessage& invoke);
    // R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): a relay nudge,
    // the Core's own Scan LAN (answered when its listening window ends) and
    // the Peripherals row's address saved without dialling.
    void handleMoveTgxlRelay(const NereusSDR::SessionMessage& invoke);
    void handleScanTgxlLan(const NereusSDR::SessionMessage& invoke);
    void handleSetTgxlAddress(const NereusSDR::SessionMessage& invoke);
    // R-R3-49 (parity Task 9, remotePgxlControlVersion 4): the Power
    // Genius's OPERATE or STANDBY, the Core's own Scan LAN for it and the
    // Peripherals row's address saved without dialling.
    void handleSetPgxlOperate(const NereusSDR::SessionMessage& invoke);
    void handleScanPgxlLan(const NereusSDR::SessionMessage& invoke);
    void handleSetPgxlAddress(const NereusSDR::SessionMessage& invoke);
    // R-R3-49 (parity Task 10, remoteRfKitControlVersion 4): the RF-Kit's
    // OPERATE or STANDBY, antenna and TCI mode, as the local applet and page
    // send them, and the RF-Kit page's address saved without dialling.
    void handleSetRfKitOperate(const NereusSDR::SessionMessage& invoke);
    void handleSetRfKitAntenna(const NereusSDR::SessionMessage& invoke);
    void handleAccessoryTx(const NereusSDR::SessionMessage& invoke);
    void handleSetRfKitTciMode(const NereusSDR::SessionMessage& invoke);
    void handleSetRfKitAddress(const NereusSDR::SessionMessage& invoke);
    // R-R3-49 (parity Task 2, transmitSettingsVersion 2): the TX applet's
    // Tune Power slider, through the Core's own TransmitModel.
    void handleTunePowerForTxBand(const NereusSDR::SessionMessage& invoke);
    // R-IOS-27, R-IOS-06 (bandSelectVersion 1): the desktop's band button on
    // one slice, RadioModel::onBandButtonClicked(SliceModel*, Band).
    void handleSelectBand(const NereusSDR::SessionMessage& invoke);
    // R-R3-49 (parity Task 3, transmitSettingsVersion 3): txProfile.select,
    // save and delete through the Core's MicProfileManager, and
    // rade.resetVocoder on the Core's RADE channel.
    void handleTxProfile(const NereusSDR::SessionMessage& invoke);
    // Ruling 7.7: refuses `verb` with the holder's name when the requester
    // does not hold transmit and another device does. True when it answered.
    bool refusedForTheHolder(const QByteArray& verb, quint32 commandId);
    void handlePaProfile(const NereusSDR::SessionMessage& invoke);
    void handleRadeResetVocoder(const NereusSDR::SessionMessage& invoke);
    // R-IOS-13 / R-R3-49 (txEqCurveVersion 2): txEq.setCurve and
    // txEq.resetCurve, through TxEqCurveAccess.
    void handleTxEqCurve(const NereusSDR::SessionMessage& invoke);
    // transmitSettingsVersion 15: cfc.setProfile, through CfcProfileAccess.
    void handleCfcProfile(const NereusSDR::SessionMessage& invoke);
    void handleRequestIoBoardProbe(const NereusSDR::SessionMessage& invoke);
    // R-R3-46 fix wave (radioHardwareVersion 3): one band's RX or RX-only
    // antenna, applied through the Core's AlexAntennaFacade.
    void handleSetAlexRxAntenna(const NereusSDR::SessionMessage& invoke);
    // Parity mini-round (radioHardwareVersion 6): one band's TX antenna,
    // applied through the Core's AlexAntennaFacade.
    void handleSetAlexTxAntenna(const NereusSDR::SessionMessage& invoke);
    // Parity Task 14 (radioHardwareVersion 7): HL2 Options' I2C tool and
    // Pin Control, run by the Core's RadioModel.
    void handleRequestIoBoardI2c(const NereusSDR::SessionMessage& invoke);
    void handleSetIoBoardOutput(const NereusSDR::SessionMessage& invoke);
    // Parity ruling C4 (radioHardwareVersion 8): the radio's sample rate,
    // as a local window's Radio Info change makes it.
    void handleSetRadioSampleRate(const NereusSDR::SessionMessage& invoke);
    // Level Cal (radioHardwareVersion 12): Setup's Reset
    // (RadioModel::resetLevelCalibration).
    void handleResetLevelCalibration(const NereusSDR::SessionMessage& invoke);
    // Level Cal: start the Core's calibration run on a slice, and stop it
    // (RadioModel::requestStartLevelCalibration /
    // requestCancelLevelCalibration).
    void handleStartLevelCalibration(const NereusSDR::SessionMessage& invoke);
    void handleCancelLevelCalibration(const NereusSDR::SessionMessage& invoke);
    // Parity Task 16 (dspInfoVersion 1): the filter graph's curve for a
    // slice's receiver (RadioModel::filterResponseForStation).
    void handleFilterResponse(const NereusSDR::SessionMessage& invoke);
    // R-R3-46 / R-R3-21 (radioHardwareVersion 4): one receive filter
    // chain's filter policy, applied through the Core's AlexAntennaFacade.
    void handleSetAlexBpfMode(const NereusSDR::SessionMessage& invoke);
    // nnr.setDiagnostics, nnr.resetTuning and (R-R3-40, minor 11)
    // nnr.tryAgain, each addressed to one slice ID.
    void handleNnrAction(const NereusSDR::SessionMessage& invoke);
    // R-R3-21 / R-R3-09: notch.add / notch.move / notch.setActive /
    // notch.delete against the Core's NotchModel.
    void handleNotchAction(const NereusSDR::SessionMessage& invoke);
    void handlePureSignalAction(const NereusSDR::SessionMessage& invoke);
    // iPhone app Task 13 (R-IOS-08, deviceAdminVersion 1): devices.revoke,
    // station.rename, station.acknowledgeKeyBackup, station.retireToken.
    void handleDeviceAdmin(const NereusSDR::SessionMessage& invoke);
    // iPhone app Task 14 (R-IOS-08, pairingVersion 1): pairing.open and
    // pairing.close.
    void handlePairingWindow(const NereusSDR::SessionMessage& invoke);
    // iPhone app Task 71 (R-IOS-02, sessionHolderVersion 1): session.leave.
    void handleSessionLeave(const NereusSDR::SessionMessage& invoke);
    // iPhone app Task 74 (R-IOS-30, sessionHolderVersion 1).
    void handleConfirmAnswer(const NereusSDR::SessionMessage& invoke);
    // Slice control plan Task 4 (sliceAccessVersion 1): slice.listen,
    // slice.stopListening, slice.takeControl and slice.release.
    void handleSliceAccessVerb(const NereusSDR::SessionMessage& invoke);
    // Slice control plan Task 6: slice.setListenLevel, a listener's own
    // level and mute (SliceAccessController::setListenLevel).
    void handleSliceListenLevel(const NereusSDR::SessionMessage& invoke);

    void emitResult(const QByteArray& verb, quint32 commandId, bool accepted,
                    const QString& reason, const QList<QByteArray>& affectedKeys);
    /// Emits `result` as `owner`'s (a result that arrives on a later turn).
    void emitResultAs(const QString& owner, const NereusSDR::SessionMessage& result);

    QPointer<RadioModel> m_radioModel;
    QPointer<StationDevicesFacade> m_deviceAdmin;
    QString m_sessionOwner{QStringLiteral("local")};
    // iPhone app Task 73.
    QByteArray m_requester;
    QHash<int, ClosingSlice> m_rateClosing;
    std::function<void(int)> m_rateClose;
    QString m_rateChangedReason;
    SliceAccess m_sliceAccess;
    // Slice control plan Task 4.
    QPointer<SliceAccessController> m_sliceAccessController;
    bool m_requesterSharesSlices = false;
    ConfirmAnswer m_confirmAnswer;
    RecordAccess m_recordAccess;
    TxEqCurveAccess m_txEqCurveAccess;
    CfcProfileAccess m_cfcProfileAccess;
    SupportInputs m_supportInputs;
    // Parity Task 22: one bundle is made at a time.
    bool m_supportBundleRunning = false;
    QPointer<StationRadios> m_stationRadios;
    // Parity Task 22: support.collect and support.setLogCategories.
    void handleSupport(const NereusSDR::SessionMessage& invoke);
    void handleStationRadios(const NereusSDR::SessionMessage& invoke);
    void handleSettingsHygiene(const NereusSDR::SessionMessage& invoke);
    void handleRecords(const NereusSDR::SessionMessage& invoke);
    void handleSpotSources(const NereusSDR::SessionMessage& invoke);
    // R-IOS-26 (iPhone plan Task 22, parity Task 20): freedv.setMessage,
    // freedv.sendQsy and freedv.setHidden.
    void handleFreedv(const NereusSDR::SessionMessage& invoke);
    /// Refuses (and answers) a verb whose sliceId names another device's
    /// slice. True when it did.
    bool refusedForAnotherDevice(const NereusSDR::SessionMessage& invoke);
    /// Task 34: refuses (and answers) a change ruling 7.4 makes wait while
    /// another device's holder is on the air. True when it did.
    bool refusedWhileOnAir(const NereusSDR::SessionMessage& invoke);
    /// A refusal's command.result: its sentence as the reason, and its code
    /// and fix as the values refusalCode and refusalFix.
    void emitRefusal(const QByteArray& verb, quint32 commandId, const TxRefusal& refusal);
    void handleSetTxSlice(const NereusSDR::SessionMessage& invoke);
    // Task 37: tx.keepalive.
    void handleTxKeepalive(const NereusSDR::SessionMessage& invoke);
    /// iPhone app plan Task 77: tx.take {holderEpoch, shownKeyed}.
    void handleTxTake(const NereusSDR::SessionMessage& invoke);
    // Task 35: tx.key, tx.unkey, tx.tune, tx.twoTone.
    void handleTxKeying(const NereusSDR::SessionMessage& invoke);
    TransmitAccess m_transmitAccess;
    struct PendingPureSignalCommand {
        quint32 commandId;
        QByteArray verb;
        /// The session that asked (fix wave I1): its later phases are its.
        QString owner;
    };
    /// Set while a later result is emitted (emitResultAs).
    std::optional<QString> m_resultOwner;
    QHash<quint32, PendingPureSignalCommand> m_pureSignalCommands;
    // R-R3-49 (parity Task 8): moves on each setSessionOwner, so a scan
    // answer never reaches a later session.
    quint64 m_sessionGeneration = 0;
};

} // namespace NereusSDR
