#pragma once
// =================================================================
// src/core/session/IStationLink.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 4.
//
// Non-owning seam RadioModel holds (attachStation / detachStation,
// RadioModel.h) for the control-plane link a remote GUI drives its
// station through. StationClient (core/session/StationClient.h) is the
// one production implementation; it turns each call below into a
// CommandInvoke on the wss session and lets SessionCommandDispatcher act
// on the DAEMON's RadioModel.
//
// ── WHY THE SEAM CARRIES COMMANDS AND NOT PROPERTIES ─────────────────────
//
// Property state already has a path in both directions: StateMirror
// pushes it out and StationClient applies it in. What had no path at all
// was the operator's CLICK. Five RadioModel entry points MUTATE the slice
// list or the active slice rather than writing a property, so no amount
// of property mirroring can carry them, and MirrorPolicy correctly
// refuses to send the daemon-authoritative properties they move
// (SliceModel::active is Outbound, so an optimistic local flip was sent
// nowhere, changed nothing on the daemon, and therefore drew no
// corrective delta back: a silent, PERMANENT divergence).
//
// ── WHY IT IS TYPED, AND NOT invoke(verb, arguments) ─────────────────────
//
// A generic verb-plus-bag signature would drag SessionMessages' wire
// types (MirrorUpdate, MirrorWireKind) into src/models, and would move
// the "did I spell the verb right" question from compile time to a
// bench. Five methods is not speculative surface: each one has exactly
// one call site, in the RadioModel entry point of the same name, and
// each maps to a verb SessionCommandDispatcher::dispatch() already
// accepts.
//
// ── ASYNCHRONOUS BY CONSTRUCTION ─────────────────────────────────────────
//
// CommandOutcome answers "did this leave the client", never "did the
// station do it". The station's answer arrives later: as a CommandResult
// (refusals reach the operator through RadioModel::
// reportStationSliceCommandRejected / reportStationRetuneRejected) and,
// for anything that worked, as ordinary mirror traffic. An implementation
// must not apply the change locally on the way out, or it rebuilds the
// divergence this seam exists to close.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal: rx2PreampModeAvailable,
//                                    RX2's own preamp mode on the Core
//                                    (radioHardwareVersion 12). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: startLevelCalibration and
//                                    cancelLevelCalibration, and the
//                                    levelCalibration feature for the run's
//                                    progress (radioHardwareVersion 12).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: resetLevelCalibration
//                                    (radioHardwareVersion 12). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Parity ruling C4: setRadioSampleRate
//                                    (radioHardwareVersion 9). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29: The Core's TCI server settings (JJ's ruling of 2026-09-28,
//               stationTciSettingsVersion 1). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-08-04  J.J. Boyd / KG4VCF  Remote daemon R2 Task 4: station-link
//                                    seam. AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Remote daemon R2: grow the seam into
//                                    the five slice command verbs, so the
//                                    GUI's slice controls reach the
//                                    daemon at all. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: whether the Core
//                                    reports its Power Genius and RF-Kit
//                                    to this app, and whether the link to
//                                    the Core is up. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-48: the RF-Kit's
//                                    configure, disconnect and switch
//                                    requests, and the station TCI
//                                    switch. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: the accessory
//                                    records and settings
//                                    (accessoryDataVersion 1): interlock
//                                    policy, output limit, fault history.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: the amp's and
//                                    tuner's own settings
//                                    (remotePgxlControlVersion 3,
//                                    remoteTgxlControlVersion 1).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-21: the filter policy
//                                    request (radioHardwareVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-47: the Tuner Genius's antenna,
//                                    operate and bypass requests
//                                    (remoteTgxlControlVersion 2).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 fix wave: tgxlOperateAppliesWhole
//                                    (remoteTgxlControlVersion 3).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan, desktop remote
//                                    transmit (R-IOS-13): remoteTransmit().
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Merge of Tasks 38 and 39:
//                                    transmitTimeOutAvailable().
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 1):
//                                    transmitSettingsUnavailableReason.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2):
//                                    requestTunePowerForTxBand.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3):
//                                    requestTxProfileSelect, Save, Delete
//                                    and requestRadeResetVocoder.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 8):
//                                    tgxlFullControlAvailable,
//                                    requestTgxlRelayMove,
//                                    requestTgxlLanScan and
//                                    requestTgxlAddress
//                                    (remoteTgxlControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9):
//                                    pgxlFullControlAvailable,
//                                    requestPgxlOperate,
//                                    requestPgxlLanScan and
//                                    requestPgxlAddress
//                                    (remotePgxlControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 10):
//                                    rfKitFullControlAvailable,
//                                    requestRfKitOperate,
//                                    requestRfKitAntenna,
//                                    requestRfKitTciMode and
//                                    requestRfKitAddress
//                                    (remoteRfKitControlVersion 4).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 13):
//                                    transmitSettingsAvailable on the link.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-46 (parity Task 14):
//                                    radioHardwareAvailable, the I/O board's
//                                    I2C and output pin requests.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 16):
//                                    requestFilterResponse (dspInfoVersion
//                                    1).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 21 (R-IOS-18): the Core's
//                                    radio (stationRadiosVersion 1),
//                                    requestStationRadio.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  HL2 clock options: hl2ClockUnavailableReason
//                                    (radioHardwareVersion 10).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25): the Core's
//                                    spot sources (recordStreamVersion 1),
//                                    requestSpotSource.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  iPhone plan Task 22 / parity Task 20
//                                    (R-IOS-26): the Core's FreeDV Reporter
//                                    (stationFreedvVersion 1),
//                                    stationFreedvAvailable, requestFreedv.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 18 (B3.1): a window's band
//                                    buttons send slice.selectBand
//                                    (bandSelectVersion 1) for a named
//                                    slice. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 (R-IOS-02,
//                                    R-IOS-03, R-IOS-13):
//                                    tgxlAutotuneAvailable. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Parity Task 23 (R-R3-48, R-R3-42):
//                                    stationTciServerAvailable,
//                                    requestStationTciOptions,
//                                    and requestDisconnectStationTciClient.
//   2026-09-27  J.J. Boyd / KG4VCF  R-IOS-13 / R-R3-49: the AM Mod
//                                    Monitor's availability, source and
//                                    RESET (txModMonitorVersion 1).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  iPhone app plan Task 25: the Core's
//                                    device verbs (deviceAdminAvailable,
//                                    pairingAvailable, requestDeviceAdmin).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Addendum G-42: transmitSettingsPermitted
//                                    and transmitPermissionReason, this
//                                    window's transmit permission for the
//                                    settings only a permitted device may
//                                    change. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11:
//                                    adcAttenuatorsAvailable(). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX (radioHardwareVersion 10). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  requestCfcProfile
//                                    (transmitSettingsVersion 15).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  R-R3-49 / R-IOS-27: holdsTransmitHere(),
//                                    for the PA Gain page's on-the-air
//                                    holder rule. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control and shared listening
//                                    plan Task 5: remoteSliceAccessAvailable
//                                    and the listen, stop listening, take
//                                    control and release requests
//                                    (sliceAccessVersion 1). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 14b:
//                                    requestListenLevel, a listened slice's
//                                    own volume and mute. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec lane:
//                                    hl2SwapAudioUnavailableReason
//                                    (radioHardwareVersion 13).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QString>
#include <QVariantMap>
#include <QtGlobal>

namespace NereusSDR {

class RemoteTransmitClient;

/// Non-owning control-plane seam. RadioModel holds a pointer to one via
/// attachStation()/detachStation() without owning or including anything
/// about the transport underneath it; the concrete implementation lives
/// in core/session/StationClient.h.
class IStationLink {
public:
    virtual ~IStationLink() = default;

    /// Whether the request left this process, and why not when it did
    /// not. Deliberately NOT the station's verdict: see the header
    /// comment. `reason` is plain English, ready for a status bar, and is
    /// meaningful only when `sent` is false.
    struct CommandOutcome {
        bool sent = false;
        QString reason;
        /// The id the command went out under, when sent and the link
        /// numbers its commands (0 otherwise). Its result arrives as
        /// RadioModel::stationCommandFinished with the same id, so a sender
        /// can tell its own command's result apart (the amp applets, the
        /// TCI switch), and a page that sent it can claim the Core's
        /// refusal (RadioModel::noteAccessoryRequestShownOnPage).
        quint32 commandId = 0;
    };

    /// SessionCommandDispatcher verb "addSlice", argument initialPanId.
    virtual CommandOutcome requestAddSlice(const QString& initialPanId) = 0;

    /// Verb "addSliceOnPan", argument panId. Distinct from requestAddSlice
    /// because the daemon derives its stream placement from whether the
    /// named pan already holds slices (R2 design addendum section 6.1:
    /// "routes pan-affecting creation through addSliceOnPan"), so the two
    /// produce different placements for the same end state.
    virtual CommandOutcome requestAddSliceOnPan(const QString& panId) = 0;

    /// Verb "removeSlice", argument sliceId.
    virtual CommandOutcome requestRemoveSlice(int sliceId) = 0;

    /// Verb "setActiveSliceById", argument sliceId.
    virtual CommandOutcome requestActiveSlice(int sliceId) = 0;

    /// Verb "requestSliceSampleRate", arguments sliceId and rateHz.
    virtual CommandOutcome requestSliceSampleRate(int sliceId, int rateHz) = 0;

    /// Parity Task 18 (B3.1, R-IOS-27): verb "slice.selectBand"
    /// (bandSelectVersion 1), arguments sliceId and band (the Band value).
    /// The Core runs its own band change on that slice, with its own band
    /// memory, exactly as a band button at the Core does. The defaults
    /// refuse, for links that did not negotiate it.
    virtual bool bandSelectAvailable() const { return false; }
    /// R-IOS-26 / R-R3-49: the Core knows 2 m as its own band. Empty when
    /// it does; otherwise the plain reason a 2 m control cannot reach it.
    virtual QString band2mUnavailableReason() const { return {}; }
    virtual CommandOutcome requestSelectBand(int /*sliceId*/, int /*band*/)
    { return { false, QStringLiteral("This Core cannot change bands for this app. Updating the Core may help.") }; }

    /// Slice control plan Task 5 (sliceAccessVersion 1 at minor 11): the
    /// Core shares slices between devices with this window. Verbs
    /// "slice.listen" and "slice.stopListening" (sliceId, incarnation) and
    /// "slice.takeControl" and "slice.release" (sliceId, incarnation,
    /// controlRevision), with the values the window was shown in the
    /// slice's `access:<id>` object. The defaults refuse, for links that
    /// did not negotiate it.
    virtual bool remoteSliceAccessAvailable() const { return false; }
    static QString sliceAccessUnavailableReason()
    { return QStringLiteral("This Core cannot share slices with this app. Updating the Core may help."); }
    virtual CommandOutcome requestListen(int /*sliceId*/, quint64 /*incarnation*/)
    { return { false, sliceAccessUnavailableReason() }; }
    virtual CommandOutcome requestStopListening(int /*sliceId*/, quint64 /*incarnation*/)
    { return { false, sliceAccessUnavailableReason() }; }
    virtual CommandOutcome requestTakeControl(int /*sliceId*/, quint64 /*incarnation*/,
                                              quint64 /*controlRevision*/)
    { return { false, sliceAccessUnavailableReason() }; }
    virtual CommandOutcome requestRelease(int /*sliceId*/, quint64 /*incarnation*/,
                                          quint64 /*controlRevision*/)
    { return { false, sliceAccessUnavailableReason() }; }
    /// Slice control plan Task 14b: "slice.setListenLevel" (sliceId,
    /// incarnation, level 0..1, muted), this window's own volume and mute
    /// for a slice it listens to (the flag's "Your volume").
    virtual CommandOutcome requestListenLevel(int /*sliceId*/, quint64 /*incarnation*/,
                                              double /*level*/, bool /*muted*/)
    { return { false, sliceAccessUnavailableReason() }; }

    // R3 remote C-Tune. Default refusals retain source compatibility for
    // older test links and transports that do not negotiate this capability.
    virtual CommandOutcome requestStreamCtunPinned(int, bool)
    { return { false, QStringLiteral("Remote C-Tune is not supported by this link to the Core.") }; }
    virtual CommandOutcome requestStreamCentre(int, double)
    { return { false, QStringLiteral("Remote C-Tune is not supported by this link to the Core.") }; }

    // Task 4d remote TGXL configuration.  Defaults preserve existing test
    // links and transports which have not negotiated the accessory feature.
    virtual CommandOutcome requestConfigureTgxl(const QString&, quint16)
    { return { false, QStringLiteral("Remote TGXL configuration is not supported by this link to the Core.") }; }
    virtual CommandOutcome requestDisconnectTgxl()
    { return { false, QStringLiteral("Remote TGXL configuration is not supported by this link to the Core.") }; }

    // Task 4d remote 4O3A master control.  The station owns both the
    // persisted per-MAC intent and the listener; a remote GUI only asks it
    // to change that intent and waits for its mirrored state to return.
    virtual CommandOutcome requestFourO3AEnabled(bool)
    { return { false, QStringLiteral("Remote 4O3A control is not supported by this link to the Core.") }; }

    /// Feature gate for accessory controls.  The default keeps every
    /// existing link inert until it explicitly implements the negotiated
    /// station capability.
    virtual bool remoteTgxlConfigAvailable() const { return false; }
    virtual bool remoteFourO3AControlAvailable() const { return false; }
    /// R-R3-47 / R-R3-22: the link to the Core is up and its first state has
    /// arrived. False while connecting and after the Core is lost, when a
    /// window's copy of the Core's readings is stale.
    virtual bool stationLinkReady() const { return false; }
    /// Merge of Tasks 38 and 39 (R-IOS-04): the Core has the transmit
    /// time-out (iPhone app plan Task 38) and uses its seven settings. A Core
    /// that sends `txStateVersion` 1 has it (txState carries the time left);
    /// an older Core stores the settings and ignores them.
    virtual bool transmitTimeOutAvailable() const { return false; }
    /// R-R3-46 / R-R3-11: the Core sends the other receive ADC's own
    /// attenuator (`stepAtt` rx2AttenuationDb, adcAttenuatorVersion 1).
    virtual bool adcAttenuatorsAvailable() const { return false; }
    /// R-R3-47 / R-R3-22: the Core reports its Power Genius XL (the
    /// `amplifier` object) and its RF-Kit RF2K-S (the `rfkit` object) to
    /// this app. Both false on an older Core or link.
    virtual bool remoteAmplifierStatusAvailable() const { return false; }
    virtual bool remoteRfKitStatusAvailable() const { return false; }

    /// R-R3-47 / R-R3-22 (remotePgxlControlVersion 2): the Core's Power
    /// Genius XL is set up through the Core. Acceptance means the Core
    /// took the request; `amplifier`.connectionPhase says what happened.
    virtual bool remotePgxlControlAvailable() const { return false; }
    virtual CommandOutcome requestConfigurePgxl(const QString&, quint16)
    { return { false, QStringLiteral("The station does not support remote PGXL configuration.") }; }
    virtual CommandOutcome requestDisconnectPgxl()
    { return { false, QStringLiteral("The station does not support remote PGXL configuration.") }; }
    virtual CommandOutcome requestPgxlConnectionSettings(bool, int, int)
    { return { false, QStringLiteral("The station does not support remote PGXL configuration.") }; }

    /// R-R3-47 / R-R3-22 (remoteRfKitControlVersion 2): the Core's RF-Kit
    /// RF2K-S is set up and switched through the Core. Acceptance means the
    /// Core took the request; `rfkit`.connectionPhase and the `radio`
    /// object's rfKitEnabled say what happened.
    virtual bool remoteRfKitControlAvailable() const { return false; }
    virtual CommandOutcome requestConfigureRfKit(const QString&, quint16)
    { return { false, QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.") }; }
    virtual CommandOutcome requestDisconnectRfKit()
    { return { false, QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.") }; }
    virtual CommandOutcome requestRfKitEnabled(bool)
    { return { false, QStringLiteral("This Core does not offer RF-Kit amplifier setup to this app.") }; }
    /// I4 (R-R3-47, remoteRfKitControlVersion 3): the RF-Kit page's
    /// connection settings, antenna names and Reset amp error work from a
    /// remote window. The settings and names travel as station settings,
    /// which the Core applies at once; Reset amp error is its own request.
    virtual bool rfKitSettingsAvailable() const { return false; }
    virtual CommandOutcome requestResetRfKitError()
    { return { false, QStringLiteral("This Core does not let this app reset the RF-Kit amplifier's error. Updating the Core may help.") }; }
    /// R-R3-49 (parity Task 10, remoteRfKitControlVersion 4): the Core puts
    /// its RF-Kit amplifier in operate or standby, switches it to internal
    /// antenna 1 to 4 and puts it in TCI mode (the local applet's and
    /// page's own requests; the amp's report returns on the mirrored `rfkit`
    /// object), and saves a Host and Port typed on the RF-Kit page without
    /// dialling. The Core refuses each while the radio is on the air.
    virtual bool rfKitFullControlAvailable() const { return false; }
    /// R-R3-49 (parity Task 10, accessoryDataVersion 2): the Core's RF-Kit
    /// connection counts arrive on `accessoryData` (rfkit*).
    virtual bool rfKitCountersAvailable() const { return false; }
    /// Group B fix wave (M7, accessoryDataVersion 3): the Core's RF-Kit
    /// average response time arrives on `accessoryData` (rfkitRttAvgMs).
    virtual bool rfKitResponseTimeAvailable() const { return false; }
    virtual CommandOutcome requestRfKitOperate(bool /*on*/)
    { return { false, rfKitFullControlUnavailableReason() }; }
    virtual CommandOutcome requestRfKitAntenna(int /*port*/)
    { return { false, rfKitFullControlUnavailableReason() }; }
    virtual CommandOutcome requestRfKitTciMode()
    { return { false, rfKitFullControlUnavailableReason() }; }
    virtual CommandOutcome requestRfKitAddress(const QString& /*host*/, int /*port*/)
    { return { false, rfKitFullControlUnavailableReason() }; }
    static QString rfKitFullControlUnavailableReason()
    { return QStringLiteral("This Core does not let this app put the RF-Kit amplifier in operate or standby, switch its antenna or TCI mode, or save its address. Updating the Core may help."); }

    /// R-R3-48 (stationTciVersion 1): the Core runs its own TCI server on
    /// the station network, switched by this app's one TCI switch and port.
    virtual bool stationTciAvailable() const { return false; }
    virtual CommandOutcome requestStationTci(bool, quint16)
    { return { false, QStringLiteral("This Core has no TCI server.") }; }
    /// R-R3-48: the Core this window uses runs on this computer and serves
    /// TCI here, so the window runs no TCI server of its own. Kept while the
    /// link is down (the Core keeps its server), false for another Core.
    virtual bool coreServesTciOnThisComputer() const { return false; }
    /// Rework part 2 (R-R3-48): whether the Core keeps a station TCI switch
    /// of its own yet (1), not yet (0: this window's switch seeds it), or
    /// its settings have not arrived on this link (-1).
    virtual int coreStationTciStored() const { return -1; }
    /// Parity Task 23 (stationTciVersion 2): the Core lists the apps on
    /// its station TCI server (the `tciClients` stream), closes one on
    /// request, and takes its four options from this window.
    virtual bool stationTciServerAvailable() const { return false; }
    virtual CommandOutcome requestStationTciOptions(bool /*emulateExpertSdr3*/,
                                                    bool /*emulateSunSdr2Pro*/,
                                                    bool /*cwluBecomesCw*/,
                                                    bool /*sendInitialState*/)
    { return { false, stationTciServerUnavailableReason() }; }
    virtual CommandOutcome requestDisconnectStationTciClient(const QString& /*id*/)
    { return { false, stationTciServerUnavailableReason() }; }
    /// JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the Core
    /// shares the rest of its TCI server's settings on `stationTci` and
    /// takes one from this window (`name` a StationTciModel::settingsTable()
    /// property: a bool, or an int; txChannel 0 Left, 1 Right, 2 Both).
    virtual bool stationTciSettingsAvailable() const { return false; }
    virtual CommandOutcome requestStationTciSetting(const QByteArray& /*name*/,
                                                    const QVariant& /*value*/)
    { return { false, stationTciServerUnavailableReason() }; }
    static QString stationTciServerUnavailableReason()
    { return QStringLiteral("This Core does not let this app change its TCI server's settings or see its apps. Updating the Core may help."); }

    /// R-R3-47 / R-R3-22 (accessoryDataVersion 1): the Core shares its
    /// accessory records and settings (`accessoryData`) and takes these
    /// three changes. Acceptance means the Core took the request; the
    /// `accessoryData` object says what it now holds.
    virtual bool accessoryDataAvailable() const { return false; }
    virtual CommandOutcome requestTxInterlockPolicy(int, int, bool, double)
    { return { false, QStringLiteral("This Core does not share its amplifier and tuner settings with this app.") }; }
    virtual CommandOutcome requestPgxlPowerCap(bool, int)
    { return { false, QStringLiteral("This Core does not share its amplifier and tuner settings with this app.") }; }
    virtual CommandOutcome requestClearAccessoryFaults(const QString&)
    { return { false, QStringLiteral("This Core does not share its amplifier and tuner settings with this app.") }; }

    /// R-R3-47 / R-R3-22 (remotePgxlControlVersion 3): the Core sends the
    /// Power Genius's own settings (name, hardware, network, Save & Reboot,
    /// Revert) to the amp as the local Advanced page does. Acceptance means
    /// the request left for the amp; the amp's answer and values come back
    /// on `accessorySettings`.
    virtual bool pgxlDeviceSettingsAvailable() const { return false; }
    virtual CommandOutcome requestPgxlName(const QString&)
    { return { false, pgxlDeviceSettingsUnavailableReason() }; }
    /// `setting` is "biasMode" (ClassA or ClassAB), "fanMode" (Auto, Quiet
    /// or Continuous) or "ledIntensity" (0 to 100, as a number).
    virtual CommandOutcome requestPgxlHardware(const QString&, const QString&)
    { return { false, pgxlDeviceSettingsUnavailableReason() }; }
    virtual CommandOutcome requestPgxlNetwork(bool, const QString&, const QString&, const QString&)
    { return { false, pgxlDeviceSettingsUnavailableReason() }; }
    virtual CommandOutcome requestPgxlSaveAndRestart()
    { return { false, pgxlDeviceSettingsUnavailableReason() }; }
    virtual CommandOutcome requestPgxlReadSettings()
    { return { false, pgxlDeviceSettingsUnavailableReason() }; }
    /// R-R3-47 / R-R3-22 (remoteTgxlControlVersion 1): the same for the
    /// Tuner Genius's own settings (name, network, Save & Reboot, Revert).
    virtual bool tgxlDeviceSettingsAvailable() const { return false; }
    virtual CommandOutcome requestTgxlName(const QString&)
    { return { false, tgxlDeviceSettingsUnavailableReason() }; }
    virtual CommandOutcome requestTgxlNetwork(bool, const QString&, const QString&, const QString&)
    { return { false, tgxlDeviceSettingsUnavailableReason() }; }
    virtual CommandOutcome requestTgxlSaveAndRestart()
    { return { false, tgxlDeviceSettingsUnavailableReason() }; }
    virtual CommandOutcome requestTgxlReadSettings()
    { return { false, tgxlDeviceSettingsUnavailableReason() }; }
    /// R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): the Core switches
    /// its Tuner Genius's antenna (port 1 to 3), operate and bypass for this
    /// window. Acceptance means the command left for the tuner; the tuner's
    /// report comes back on the mirrored `tuner` object. The Core refuses
    /// each while the radio is on the air.
    /// iPhone app plan Task 77 (remoteTxVersion 2): the Core runs its Tuner
    /// Genius autotune for this window (tx.tunerTune), keyed as this
    /// device under the holder rules; requested through remoteTransmit().
    virtual bool tgxlAutotuneAvailable() const { return false; }
    virtual bool tgxlControlAvailable() const { return false; }
    virtual CommandOutcome requestTgxlAntenna(int)
    { return { false, tgxlControlUnavailableReason() }; }
    virtual CommandOutcome requestTgxlOperate(bool)
    { return { false, tgxlControlUnavailableReason() }; }
    virtual CommandOutcome requestTgxlBypass(bool)
    { return { false, tgxlControlUnavailableReason() }; }
    /// R-R3-49 fix wave (remoteTgxlControlVersion 3): requestTgxlOperate(true)
    /// puts the tuner in OPERATE whole (bypass off and operate on, applied
    /// by the Core as one command), so STANDBY to OPERATE is one request.
    /// False below 3: the window sends bypass off, then operate on.
    virtual bool tgxlOperateAppliesWhole() const { return false; }
    /// R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): the Core nudges
    /// one matching relay (`relay` 0 C1, 1 L, 2 C2; `direction` -1 or 1)
    /// as the local applet's mouse wheel does; listens for Tuner Genius
    /// announcements for this window's Scan LAN (the answer arrives as
    /// RadioModel::stationTgxlLanScanFinished); and saves a Host and Port
    /// typed without Connect, without dialling. The relay nudge keeps the
    /// tuner's report on the mirrored `tuner` object; the Core refuses each
    /// while the radio is on the air.
    virtual bool tgxlFullControlAvailable() const { return false; }
    virtual CommandOutcome requestTgxlRelayMove(int /*relay*/, int /*direction*/)
    { return { false, tgxlFullControlUnavailableReason() }; }
    virtual CommandOutcome requestTgxlLanScan()
    { return { false, tgxlFullControlUnavailableReason() }; }
    virtual CommandOutcome requestTgxlAddress(const QString& /*host*/, int /*port*/)
    { return { false, tgxlFullControlUnavailableReason() }; }
    /// R-R3-49 (parity Task 9, remotePgxlControlVersion 4): the Core puts
    /// its Power Genius in operate or standby (the local applet's line;
    /// the amp's report returns on the mirrored `amplifier` object);
    /// listens for Power Genius announcements for this window's Scan LAN
    /// (the answer arrives as RadioModel::stationPgxlLanScanFinished); and
    /// saves a Host and Port typed without Connect, without dialling. The
    /// Core refuses each while the radio is on the air.
    virtual bool pgxlFullControlAvailable() const { return false; }
    virtual CommandOutcome requestPgxlOperate(bool /*on*/)
    { return { false, pgxlFullControlUnavailableReason() }; }
    virtual CommandOutcome requestPgxlLanScan()
    { return { false, pgxlFullControlUnavailableReason() }; }
    virtual CommandOutcome requestPgxlAddress(const QString& /*host*/, int /*port*/)
    { return { false, pgxlFullControlUnavailableReason() }; }
    static QString pgxlFullControlUnavailableReason()
    { return QStringLiteral("This Core does not let this app put the Power Genius in operate or standby, scan for it or save its address. Updating the Core may help."); }
    static QString tgxlFullControlUnavailableReason()
    { return QStringLiteral("This Core does not let this app move the Tuner Genius relays, scan for it or save its address. Updating the Core may help."); }
    static QString tgxlControlUnavailableReason()
    { return QStringLiteral("This Core does not let this app switch the Tuner Genius. Updating the Core may help."); }
    static QString pgxlDeviceSettingsUnavailableReason()
    { return QStringLiteral("This Core does not let this app change the Power Genius's own settings. Updating the Core may help."); }
    static QString tgxlDeviceSettingsUnavailableReason()
    { return QStringLiteral("This Core does not let this app change the Tuner Genius's own settings. Updating the Core may help."); }
    /// R-R3-49 (parity Task 1): a window's reason for its transmit settings
    /// on a Core without transmitSettingsVersion.
    static QString transmitSettingsUnavailableReason()
    { return QStringLiteral("This Core does not let this app change transmit settings. Updating the Core may help."); }
    /// R-R3-49 (parity Task 13): the link is ready and the Core offers
    /// transmitSettingsVersion at least `minVersion`. A page that holds
    /// settings the Core takes on the air too (Hardware Config's OC pin
    /// actions and transmit calibration) asks this directly, since the
    /// dialog's version gates also close on the air. The default refuses,
    /// for links that did not negotiate it.
    virtual bool transmitSettingsAvailable(int /*minVersion*/ = 1) const { return false; }
    /// Addendum G-42: whether the Core lets this device change the
    /// transmit settings it takes only from a device it permits to
    /// transmit (Extended transmit): the Core's txPermitted for this
    /// session. The default refuses.
    virtual bool transmitSettingsPermitted() const { return false; }
    /// Why not, in the Core's words when it gave them (txRefusalReason);
    /// empty while permitted.
    virtual QString transmitPermissionReason() const
    { return QStringLiteral("This device cannot transmit through this Core."); }
    /// R-R3-49 / R-IOS-27 (JJ's ruling): whether the Core names this
    /// window's device as the one holding transmit. On the air the PA
    /// Gain page opens the transmitting band only then. The default: no.
    virtual bool holdsTransmitHere() const { return false; }
    /// R-R3-49 (parity Task 2, transmitSettingsVersion 2): the TX applet's
    /// Tune Power slider. The Core sets the tune power for the band it
    /// transmits on and the tune drive source to the tune slider, as the
    /// local slider does; both come back on the mirrored `transmit` object
    /// (tunePowerForTxBand, tuneDrivePowerSource). Refused on the air.
    virtual CommandOutcome requestTunePowerForTxBand(int)
    { return { false, transmitSettingsUnavailableReason() }; }
    /// R-R3-49 (parity Task 3, transmitSettingsVersion 3): the TX profile
    /// combos and Setup > Audio > TX Profile. The Core selects, saves (its
    /// current transmit settings) or deletes the named profile, as the
    /// local controls do; its active profile and list come back on the
    /// mirrored `transmit` object. Refused on the air.
    virtual CommandOutcome requestTxProfileSelect(const QString&)
    { return { false, transmitSettingsUnavailableReason() }; }
    virtual CommandOutcome requestTxProfileSave(const QString&)
    { return { false, transmitSettingsUnavailableReason() }; }
    virtual CommandOutcome requestTxProfileDelete(const QString&)
    { return { false, transmitSettingsUnavailableReason() }; }
    /// R-R3-49 (parity Task 3): the RADE applet's Reset vocoder; the Core
    /// clears its RADE transmit vocoder. Keys nothing. Refused on the air.
    virtual CommandOutcome requestRadeResetVocoder()
    { return { false, transmitSettingsUnavailableReason() }; }
    /// transmitSettingsVersion 15: the CFC dialog's band editor. The Core
    /// applies `profileJson` (the published cfcProfile form) at once when
    /// `expectedRevision` is still its profile's revision; the saved values
    /// come back on the mirrored `transmit` object.
    virtual CommandOutcome requestCfcProfile(const QString&, const QString&)
    { return { false, transmitSettingsUnavailableReason() }; }

    virtual CommandOutcome requestApplyNnrModels(quint32)
    { return { false, QStringLiteral("NNR model application is not supported by this link to the Core.") }; }
    virtual bool nnrControlAvailable() const { return false; }
    virtual CommandOutcome requestNnrDiagnostics(int, int, int)
    { return { false, QStringLiteral("NNR diagnostics are not supported by this link to the Core.") }; }

    // R-R3-46 (radioHardwareVersion 2): ask the Core to probe its radio's
    // HL2 I/O board. The default refuses, for links that did not negotiate
    // the Core's hardware settings.
    virtual CommandOutcome requestIoBoardProbe()
    { return { false, QStringLiteral("This Core cannot probe its radio's I/O board for this app.") }; }

    // R-R3-46 (parity Task 14): the link is ready and the Core offers
    // radioHardwareVersion at least `minVersion`. At 7 a window uses HL2
    // Options' I2C tool and Pin Control through the Core, and changes the
    // Alex tab's three transmit high-pass switches. The default refuses.
    virtual bool radioHardwareAvailable(int /*minVersion*/) const { return false; }
    static QString ioBoardI2cUnavailableReason()
    { return QStringLiteral("This Core cannot reach its radio's I2C bus for this app. Updating the Core may help."); }
    static QString alexHpfSwitchesUnavailableReason()
    { return QStringLiteral("This Core cannot change these high-pass switches for this app. Updating the Core may help."); }
    // radioHardwareVersion 8: the Alex Filters tabs' receive filter rows.
    static QString alexHpfRowsUnavailableReason()
    { return QStringLiteral("This Core cannot change these filter rows for this app. Updating the Core may help."); }
    // radioHardwareVersion 10: the Alex-1 Filters tab's low-pass rows and
    // 6m/ByPass on RX.
    static QString alexLpfRowsUnavailableReason()
    { return QStringLiteral("This Core cannot change the low-pass filter rows for this app. Updating the Core may help."); }
    // radioHardwareVersion 11: HL2 Options' Enable CL2, CL2 frequency and
    // External 10 MHz, which the Core sends to its radio.
    static QString hl2ClockUnavailableReason()
    { return QStringLiteral("This Core cannot change its radio's clock settings for this app. Updating the Core may help."); }
    // radioHardwareVersion 13: HL2 Options' Swap audio channels, which the
    // Core applies to the receive audio it sends its radio.
    static QString hl2SwapAudioUnavailableReason()
    { return QStringLiteral("This Core cannot swap its radio's audio channels for this app. Updating the Core may help."); }
    // Verb "requestIoBoardI2c" (radioHardwareVersion 7): one I2C read or
    // write on the Core's radio. The answer (a read's bytes in `value`)
    // arrives as RadioModel::reportStationIoBoardResult.
    virtual CommandOutcome requestIoBoardI2c(int /*bus*/, int /*address*/, int /*reg*/,
                                             bool /*write*/, int /*value*/)
    { return { false, ioBoardI2cUnavailableReason() }; }
    // Verb "setIoBoardOutput" (radioHardwareVersion 7): one of the Core's
    // I/O board outputs on or off.
    virtual CommandOutcome requestIoBoardOutput(int /*pin*/, bool /*on*/)
    { return { false, ioBoardI2cUnavailableReason() }; }

    // Parity ruling C4, verb "setRadioSampleRate" (radioHardwareVersion 9):
    // the radio's sample rate, as a local window's Radio Info change makes
    // it (every receiver and the radio's own rate). The default refuses; a
    // window then asks each of its receivers instead.
    virtual bool radioSampleRateAvailable() const { return false; }
    static QString radioSampleRateUnavailableReason()
    { return QStringLiteral("This Core changes the sample rate of this window's receivers only. Updating the Core may help."); }
    virtual CommandOutcome requestRadioSampleRate(int /*rateHz*/)
    { return { false, radioSampleRateUnavailableReason() }; }

    // Level Cal, verb "resetLevelCalibration" (radioHardwareVersion 12):
    // Setup's Reset, the meter and display calibration back to the radio's
    // defaults on the Core (RadioModel::resetLevelCalibration). The default
    // refuses, and the window shows Reset disabled with the reason.
    virtual bool levelCalibrationResetAvailable() const { return false; }
    static QString levelCalibrationResetUnavailableReason()
    { return QStringLiteral("This Core cannot reset the level calibration for this app. Updating the Core may help."); }
    virtual CommandOutcome requestResetLevelCalibration()
    { return { false, levelCalibrationResetUnavailableReason() }; }

    // Level Cal, verbs "startLevelCalibration" and "cancelLevelCalibration"
    // (radioHardwareVersion 12): Setup's Start and Cancel, the calibration
    // run on the Core (RadioModel::requestStartLevelCalibration). Its
    // progress comes back as the radio's levelCal* properties (feature
    // "levelCalibration"). The default refuses, and the window shows Start
    // disabled with the reason.
    virtual bool levelCalibrationRunAvailable() const { return false; }
    static QString levelCalibrationRunUnavailableReason()
    { return QStringLiteral("This Core cannot run the level calibration for this app. Updating the Core may help."); }
    virtual CommandOutcome requestStartLevelCalibration(float /*levelDbm*/, double /*frequencyHz*/,
                                                        int /*sliceId*/)
    { return { false, levelCalibrationRunUnavailableReason() }; }
    virtual CommandOutcome requestCancelLevelCalibration()
    { return { false, levelCalibrationRunUnavailableReason() }; }
    // Level Cal (radioHardwareVersion 12): the Core's stepAtt carries
    // rx2PreampMode, RX2's own preamp mode, which a slice on the other ADC
    // uses. The default says no, and the window shows that slice's preamp
    // choice disabled with the reason.
    virtual bool rx2PreampModeAvailable() const { return false; }
    static QString rx2PreampModeUnavailableReason()
    { return QStringLiteral("This Core cannot change the preamp of this slice's receiver input for this app. Updating the Core may help."); }

    // R-R3-49 (parity Task 16): verb "dsp.filterResponse" (dspInfoVersion
    // 1), the filter graph's curve for a slice's receiver on the Core. The
    // answer arrives as RadioModel::reportStationFilterResponse.
    static QString filterResponseUnavailableReason()
    { return QStringLiteral("This Core does not send its filter curve. Updating the Core may help."); }
    virtual CommandOutcome requestFilterResponse(int /*sliceId*/, bool /*highResolution*/)
    { return { false, filterResponseUnavailableReason() }; }

    // R-IOS-25 / R-R3-49 (parity Task 19, recordStreamVersion 1): the Core
    // runs the station's spot sources (DX cluster, RBN, POTA, PSK
    // Reporter). `verb` is spots.connect, spots.disconnect,
    // spots.sendCommand (with `text`) or spots.clearAll (no source).
    static QString spotSourcesUnavailableReason()
    { return QStringLiteral("This Core does not run its spot sources for this app. Updating the Core may help."); }
    virtual bool spotSourcesAvailable() const { return false; }

    // R-IOS-18 / R-R3-49 (parity Task 21, stationRadiosVersion 1): This
    // Core's Change radio. `verb` is station.selectRadio (mac),
    // station.rescanRadios, station.setRadioModel (mac, model) or
    // station.forgetRadio (mac); a refusal comes back as
    // RadioModel::stationRadioRefused.
    static QString stationRadiosUnavailableReason()
    { return QStringLiteral("This Core does not let this app change its radio. Updating the Core may help."); }
    virtual bool stationRadiosAvailable() const { return false; }
    // iPhone app plan Task 25 (the This Core page in a remote window;
    // deviceAdminVersion 1, pairingVersion 1): the Core's paired devices.
    // `verb` is devices.revoke (`id`, the device's wire id),
    // station.acknowledgeKeyBackup, pairing.open or pairing.close; the
    // answer comes back as RadioModel::stationCommandFinished. The Core
    // takes them from a window signed in with this computer's own key.
    static QString deviceAdminUnavailableReason()
    { return QStringLiteral("This Core does not let this app manage its devices. Updating the Core may help."); }
    static QString pairedDeviceAdminReason()
    { return QStringLiteral("Manage the Core's devices from a paired device."); }
    virtual bool deviceAdminAvailable() const { return false; }
    virtual bool pairingAvailable() const { return false; }
    virtual CommandOutcome requestDeviceAdmin(const QByteArray& /*verb*/, const QString& /*id*/)
    { return { false, deviceAdminUnavailableReason() }; }
    static QString settingsHygieneUnavailableReason()
    { return QStringLiteral("This Core does not offer Settings Validation to this app. Updating the Core may help."); }
    virtual bool settingsHygieneAvailable() const { return false; }
    /// G-38: Repair invalid settings (station.repairSettings) needs
    /// settingsHygieneVersion 2; an older Core keeps the button disabled.
    static QString settingsRepairUnavailableReason()
    { return QStringLiteral("Repair invalid settings is not available on this Core. Updating the Core may help."); }
    virtual bool settingsRepairAvailable() const { return false; }
    virtual CommandOutcome requestSettingsHygiene(const QByteArray& /*verb*/, const QString& /*mac*/)
    { return {false, settingsHygieneUnavailableReason()}; }
    /// Export only. Connect to RadioModel::stationSettingsBackupExportFinished
    /// before requesting; its operationId equals the returned commandId.
    /// Cancel when the consuming page closes. Only completed, validated Core
    /// XML is reported, and a failure carries empty bytes.
    virtual bool settingsBackupExportAvailable() const { return false; }
    virtual CommandOutcome requestSettingsBackupExport()
    { return {false, QStringLiteral("This Core does not offer settings export to this app.")}; }
    /// A nonzero operationId cancels only that owner's request. Zero is the
    /// explicit client-wide cancel used by session/UI teardown.
    virtual void cancelSettingsBackupExport(quint32 /*operationId*/ = 0) {}
    /// Fix wave (I5): whether this session signed in with this computer's
    /// own device key. The Core takes the four radio requests only from
    /// such a session (StationRadios::pairedDeviceReason otherwise).
    virtual bool signedInWithDeviceKey() const { return false; }
    /// Follow-up N1: this session signed in with the pairing token and
    /// enrolled this computer's key in the same sign-in. Its next sign-in
    /// is by key, so reconnecting is all the radio requests need.
    virtual bool enrolledDeviceKeyThisSession() const { return false; }
    virtual CommandOutcome requestStationRadio(const QByteArray& /*verb*/, const QString& /*mac*/,
                                               int /*model*/)
    { return { false, stationRadiosUnavailableReason() }; }
    virtual CommandOutcome requestSpotSource(const QByteArray& /*verb*/, const QString& /*source*/,
                                             const QString& /*text*/)
    { return { false, spotSourcesUnavailableReason() }; }

    // iPhone plan Task 22 / parity Task 20 (R-IOS-26, stationFreedvVersion
    // 1): the Core runs FreeDV Reporter. `verb` is freedv.setMessage
    // (`text`), freedv.sendQsy (`callsign`, `frequencyHz`) or
    // freedv.setHidden (`on`); spots.connect / spots.disconnect with source
    // freedvReporter start and stop it.
    static QString stationFreedvUnavailableReason()
    { return QStringLiteral("This Core does not run FreeDV Reporter for this app. Updating the Core may help."); }
    virtual bool stationFreedvAvailable() const { return false; }
    virtual CommandOutcome requestFreedv(const QByteArray& /*verb*/, const QVariantMap& /*args*/)
    { return { false, stationFreedvUnavailableReason() }; }

    // Remote-window parity Task 22 / the iPhone app plan's Task 25 (R-R3-49,
    // R-IOS-18, supportBundleVersion 1): the Core's support bundle, its log
    // and its logging categories. requestSupportBundle sends
    // support.collect (the answer arrives as
    // RadioModel::reportStationSupportBundle); requestLogCategories sends
    // support.setLogCategories with the whole list to turn on;
    // requestCoreLog subscribes to (true) or leaves (false) the `coreLog`
    // record stream, and subscribes again after each new session while it
    // is wanted.
    static QString supportBundleUnavailableReason()
    { return QStringLiteral("This Core does not share its log or support bundle with this app. Updating the Core may help."); }
    virtual bool supportBundleAvailable() const { return false; }
    virtual CommandOutcome requestSupportBundle()
    { return { false, supportBundleUnavailableReason() }; }
    virtual CommandOutcome requestLogCategories(const QString& /*categories*/)
    { return { false, supportBundleUnavailableReason() }; }
    virtual void requestCoreLog(bool /*follow*/) {}
    // R-IOS-13 / R-R3-49 (txModMonitorVersion 1): the AM Mod Monitor in a
    // remote window. Whether the Core sends its readings; which source this
    // window watches (0 TX I/Q, 1 PA feedback, -1 none: the applet hidden),
    // kept across reconnects and subscribed again after each snapshot; and
    // RESET, which clears the Core's analyzer for a source. The readings
    // arrive in RadioModel::stationModMonitorSnapshot.
    static QString modMonitorUnavailableReason()
    { return QStringLiteral("This Core does not send the modulation monitor. Updating the Core may help."); }
    virtual bool txModMonitorAvailable() const { return false; }
    virtual void setModMonitorSource(int /*source*/) {}
    virtual CommandOutcome requestModMonitorReset(int /*source*/)
    { return { false, modMonitorUnavailableReason() }; }

    // R-R3-46 / R-R3-21 (radioHardwareVersion 4): the filter policy dialog
    // in a remote window. Whether the Core takes a filter policy change from
    // this app now, why not in plain words, and the request itself (chain
    // 0 or 1; mode 0 Auto, 1 Force filter, 2 Force bypass). The defaults
    // refuse, for links that did not negotiate it.
    virtual bool filterPolicyEditAvailable() const { return false; }
    virtual QString filterPolicyUnavailableReason() const
    { return QStringLiteral("This Core cannot change its filter policy for this app. Updating the Core may help."); }
    virtual CommandOutcome requestFilterPolicy(int /*chain*/, int /*mode*/)
    { return { false, filterPolicyUnavailableReason() }; }

    /// iPhone app plan, desktop remote transmit (R-IOS-13): the window's
    /// side of the transmit verbs, or null for a link without them. A
    /// remote window keys through it and never through its own
    /// MoxController; RadioModel routes MOX, TUNE and two-tone here while
    /// it is available().
    virtual RemoteTransmitClient* remoteTransmit() { return nullptr; }
};

} // namespace NereusSDR
