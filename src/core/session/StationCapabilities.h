// 2026-09-27: activate the validated Core transmit-region control.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
// =================================================================
// src/core/session/StationCapabilities.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
//
// The capability descriptor the daemon advertises immediately after
// authentication, and the client applies to its own RadioModel. Parent
// design section 7.0:
//
//   "Capability descriptor, advertised by the daemon: SKU, maxSlices,
//   userDdcCount, available modes, PureSignal present, wideband present,
//   TX permitted, supported audio codecs, supported display-codec
//   versions, max pixels, max frame rate, AppSettings schema version. The
//   client's UI gates on these."
//
// ---- EFFECTIVE, not board (parent section 4.5) ----
//
// "Capacity is a runtime property, not a SKU property. Section 7.0's
// capability descriptor currently advertises maxSlices and userDdcCount
// straight from BoardCapabilities, which describes what the RADIO
// supports. On the floor the DAEMON may not sustain that. The descriptor
// therefore advertises EFFECTIVE limits ... The client gates its UI on
// the effective values, never the board values."
//
// So `effectiveMaxSlices` is the number the client gates on, and
// `boardMaxSlices` travels alongside it for diagnostics ONLY -- so an
// operator looking at a station that will only give them two slices on a
// five-slice radio can see that it is a daemon decision and not a
// misidentified SKU. Nothing on the client may gate on boardMaxSlices;
// StationClient writes only the effective value into RadioModel.
//
// R2 has no PerfMonitor and no degradation ladder, so the effective value
// is whatever StationServer::setSustainableSliceLimit() was told, which
// defaults to the board value. Section 4.5's ladder (step 4, "Refuse
// additional slices beyond the sustainable count") is what eventually
// makes this number move at runtime; the field exists now so the client
// is already reading the right one when it does, rather than needing a
// protocol change on the day the ladder lands.
//
// ---- What R2 deliberately does NOT advertise ----
//
// Six of section 7.0's listed entries have no honest source in R2 and are
// therefore absent rather than present-and-fabricated: available modes,
// supported audio codecs, supported display-codec versions, max pixels,
// max frame rate, wideband present. R2's demo is explicitly "no spectrum
// trace, no waterfall, no sound" (design addendum section 2), so every one
// of those describes a subsystem this release does not carry over the
// link at all. A descriptor entry advertising a capability nobody
// implements is worse than a missing one: fromUpdates() below treats an
// absent entry as "the peer did not say", which is recoverable, whereas a
// zero or an empty list read as authoritative is not.
//
// Adding one later is a MINOR protocol bump plus a capability entry, which
// is exactly what section 7.0's version policy is for.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: RADE reason: radeReasonVersion, after
//               rxFilterLowPassVersion and before coreBuildInfo. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Shared-input filters (ruling (d)): rxFilterLowPassVersion,
//               after radioMicVersion and before coreBuildInfo. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Radio codec lane: radioMicVersion, after
//               rx2AttenuatorVersion and before coreBuildInfo. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Level Cal 2: rx2AttenuatorVersion, after the direct media
//               ladder and before coreBuildInfo. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: mediaDirectVersion and
//               mediaStunUrls, before coreBuildInfo. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: The Core's TCI server settings (JJ's ruling of 2026-09-28,
//               stationTciSettingsVersion 1). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1):
//               a device's own Opus bitrate. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29 - RADE status: radeStatusVersion. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: capability
//                                    descriptor. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46: the Core's radio model,
//                                    protocol and address. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22:
//                                    remotePgxlControlVersion and
//                                    remoteRfKitControlVersion, in the same
//                                    minor-11 block. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24 - R-R3-48: stationTciVersion, last in the
//                minor-11 block. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: accessoryDataVersion, last in the
//                minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: remotePgxlControlVersion 3 and
//                remoteTgxlControlVersion 1 (the amp's and tuner's own
//                settings), the latter last in the minor-11 block. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 12 (R-IOS-08): stationIdentityVersion,
//                last in the minor-11 block. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 13 (R-IOS-08): deviceAdminVersion, last
//                in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 14 (R-IOS-08): pairingVersion, last in
//                the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 19 (R-IOS-06): stationCatalogVersion,
//                last in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 20 (R-IOS-27): displayExtrasVersion,
//                last in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 1): transmitSettingsVersion, last
//                in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): sessionHolderVersion,
//               sent only to a peer that declared sessionHolder. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): remoteTxVersion, last,
//               sent only to a peer whose hello declared remoteTx; txPermitted
//               now the station transmit gate's answer. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan, desktop remote transmit (R-IOS-13,
//               R-R3-42): txRefusalCode, txRefusalReason and txRefusalFix
//               after remoteTxVersion. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): txStateVersion,
//               after remoteTxVersion and only with it (the `txState`
//               object). J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-25 - R-IOS-27, R-IOS-06: bandSelectVersion, last in the
//                minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-IOS-27, R-IOS-06: notchControlVersion 2 documented
//                (notch.addAtSlice). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25 - R-IOS-27, R-IOS-06: displayExtrasVersion 2 documented
//                (clarity-retune). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-46 / R-R3-32 (parity Task 14): radioHardwareVersion
//                7 and stationTelemetryVersion 5 documented. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (parity Task 15): meterReadingsVersion.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16):
//                dspInfoVersion. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-IOS-25 / R-R3-49 (parity Task 19): recordStreamVersion.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-IOS-18 / R-R3-49 (parity Task 21): stationRadiosVersion.
//   2026-09-26 - R-R3-49 / A11 (parity Task 28): txDisplayVersion.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-21 / R-R3-08: displayClockVersion. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-IOS-16 (iPhone app plan Task 28 fix wave):
//                controlChannelVersion. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27 - R-IOS-13 / R-R3-49 (parity Task 32): txMonitorAudioVersion.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20):
//                stationFreedvVersion. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27 - R-IOS-16 (iPhone app plan Task 29): mediaReplaceVersion,
//                controlSwitchVersion and relayAllowed. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-R3-49 / R-IOS-18 (remote-window parity Task 22, iPhone
//                app plan Task 25): supportBundleVersion. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-R3-49 / R-R3-32 (parity Task 33): txReadingsVersion.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-13 / R-R3-49: txModMonitorVersion, the AM Mod
//                Monitor's readings. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - R-IOS-13 / R-R3-49: txEqCurveVersion, the read-only TX
//                EQ curve on `transmit`. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - R-IOS-26 / R-R3-49: band2mVersion, 2 m as its own band.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Phone wire batch: diversityPatternVersion,
//                logCategoryListVersion and radioModelsVersion. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-IOS-13 / R-R3-49: txEqCurveVersion 2 (the curve verbs).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - kTransmitSettingsOnAirVersion (transmitSettingsVersion
//                13). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-09-29 - The phone's direct addresses: coreAddressesVersion.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-11: adcAttenuatorVersion, the other ADC's
//                own attenuator on `stepAtt`. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: paProfileVersion and the read-only
//                 paProfiles object (PaProfilesFacade). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: txInhibitReasonVersion. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX (radioHardwareVersion 10). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - kTransmitSettingsCfcProfileVersion (15): the CFC band
//                editor published and cfc.setProfile. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Slice control plan Task 4: sliceAccessVersion, appended
//                after radioAntennaRowsVersion, only for a peer that
//                declared sliceAccess with sessionHolder. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QList>
#include <optional>
#include <QString>
#include <QStringList>

#include "core/HpsdrModel.h"
#include "core/session/MirrorSchema.h"
#include "core/session/media/DisplayBudget.h"

namespace NereusSDR {

/// Remote parity on the air: from this transmitSettingsVersion the Core
/// takes the transmit settings while its radio is on the air, as a local
/// window does (the OC transmit pins and Region still wait). The DSP >
/// Options TX and PA keys reach the TX channel and PA profiles once the
/// radio is back on receive; the SWR protection keys apply at once, as the
/// local page and Thetis apply them. A window against an older Core keeps
/// them disabled on the air with the reason.
inline constexpr int kTransmitSettingsOnAirVersion = 13;

/// From this transmitSettingsVersion the Core owns General Options'
/// "Prevent transmitting on a different band" (PreventTxOnDifferentBandToRx):
/// a window shows the Core's value and changes it with transmit permission,
/// off the air. Against an older Core the box is disabled with the reason.
inline constexpr int kTransmitSettingsDifferentBandVersion = 14;

/// From this transmitSettingsVersion the Core publishes the CFC dialog's
/// band editor (transmit's cfcProfile, to a peer that declared cfcProfile 1)
/// and takes cfc.setProfile: every band's frequency, compression, post-EQ
/// gain and Q, the range, the pre-compression and the post-EQ gain, applied
/// at once against the revision the app last saw. Against an older Core a
/// window keeps writing the CFC properties one by one.
inline constexpr int kTransmitSettingsCfcProfileVersion = 15;

/// Optional identity of the Core executable, never radio firmware identity.
struct CoreBuildInfo {
    QString productVersion;
    QString sourceTag;

    QByteArray toJson() const;
    static std::optional<CoreBuildInfo> fromJson(const QByteArray& json);
};

struct StationCapabilities {
    // ---- Station identity ----
    // RadioModel only ever learns these from a connected radio, or from
    // this descriptor: name/model/version are mirrored Q_PROPERTYs with no
    // WRITE, and RadioModel::applyMirroredValue refuses all three
    // deliberately (see its own doc comment, which names this task).
    QString stationName;      ///< RadioModel::name(), e.g. "ANAN-G2E"
    QString radioModelName;   ///< RadioModel::model()
    QString firmwareVersion;  ///< RadioModel::version()
    QString macAddress;       ///< The connected radio's MAC, per-MAC settings scope
    HPSDRHW board = HPSDRHW::Unknown;

    // ---- The Core's radio (R-R3-46) ----
    // Sent only to a peer that negotiated kRadioIdentitySessionProtocolMinor
    // (radioIdentityEntries true); an older app receives exactly the
    // descriptor it was built for. Absent (an older Core) reads as the
    // defaults below, which a window treats as "the Core did not say".
    //
    // The three travel together: fromUpdates() sets radioIdentityEntries
    // when it sees any of them.
    bool radioIdentityEntries = false;
    /// The Core's own HardwareProfile model (an operator's model choice
    /// included), so a window resolves ANAN-8000DLE and ANAN-G2 1K rather
    /// than the first model on their board. FIRST: not reported, or a
    /// value this build does not know.
    HPSDRModel hpsdrModel = HPSDRModel::FIRST;
    /// ProtocolVersion as an integer: 1 or 2. 0: not reported.
    int radioProtocol = 0;
    /// The radio's LAN address as the Core sees it. Empty: not reported.
    QString radioAddress;
    /// R-R3-46 / R-R3-11: 1 means the Core mirrors its step attenuator and
    /// preamp as the `stepAtt` object and applies a window's edits to it
    /// through its own controller. 2 adds the Alex antenna settings
    /// (`alexAntennas`), the hardware apply step and the I/O board probe; 3
    /// the read-only `ioBoard` object and the setAlexRxAntenna command; 4
    /// the setAlexBpfMode command (a receive filter chain's filter policy,
    /// R-R3-46 / R-R3-21); 5 `rxOutOnTx` two-way (group B fix wave); 6 the
    /// rest of the transmit antennas and relays two-way (`txAntennas`,
    /// `blockTxAnt2`, `blockTxAnt3`, `ext1OutOnTx`, `ext2OutOnTx`,
    /// `rxOutOverride`; parity Task 12) and the setAlexTxAntenna command
    /// (one band's TX antenna; parity mini-round); 7 the I/O board's
    /// requestIoBoardI2c and setIoBoardOutput commands, `ioBoard` outputs
    /// and the Alex tab's three transmit high-pass switches taken from a
    /// window (parity Task 14); 8 the Alex Filters tabs' receive filter
    /// rows (each row's bypass and edges, and Alex-2's master bypass),
    /// applied to the Core's radio at once; 9 the radio's sample rate
    /// (setRadioSampleRate); 10 the Alex-1 Filters tab's low-pass rows and
    /// 6m/ByPass on RX, and radio's alexLpfBits to a peer that declared
    /// alexLpf 1. Sent last in the same block as the three above,
    /// so only at minor 11. 0: a window keeps today's behaviour and does
    /// not write `stepAtt`.
    int radioHardwareVersion = 0;
    /// One-band Alex edits bound to the connected radio's canonical MAC.
    /// Optional at minor 11 for a peer declaring radioAntennaRows 1.
    int radioAntennaRowsVersion = 0;
    /// iPhone app plan Task 25 (R-IOS-18): 1 means the Core sends the `vax`
    /// object (the station computer's VAX channels) and keeps the
    /// `vaxLevels` record stream. Sent last in the minor-11 block, only to a
    /// peer whose hello declared vax 1 (vaxEntry), so every other peer's
    /// capabilities are today's.
    bool vaxEntry = false;
    int vaxVersion = 0;
    /// R-IOS-13 / R-R3-49: 1 means `transmit` carries txEqCurve, the TX EQ
    /// parametric curve the Core derives from txEqParaEqData as documented,
    /// read-only JSON. Sent at minor 11 only to a peer whose hello declared
    /// txEqCurve 1, after radioAntennaRowsVersion; that peer alone gets the
    /// property. 0 (absent): the peer sees today's wire. 2, to a peer that
    /// declared txEqCurve 2: also the txEq.setCurve and txEq.resetCurve
    /// verbs, applied as that peer's txEqParaEqData write.
    int txEqCurveVersion = 0;
    /// R-IOS-26 / R-R3-49: 1 means the Core knows 2 m as its own band
    /// (Band 27, BandLinkFit.h) and sends it to this peer. Optional and
    /// last at minor 11, for a peer that declared band2m 1. 0: the Core
    /// sends 2 m as GEN and the per-band lists without it, and a window
    /// sends neither band 27 nor a 2 m list entry.
    int band2mVersion = 0;
    /// Phone wire batch: 1 means every slice carries `diversityPattern`,
    /// the Diversity dialog's sensitivity pattern. Sent after vaxVersion,
    /// only to a peer at minor 11 whose hello declared diversityPattern 1,
    /// on a Core with a radio model; any other peer's capabilities and
    /// slices are today's.
    int diversityPatternVersion = 0;
    int diversityControlVersion = 0;
    /// Phone wire batch: 1 means `radio` carries `logCategoryList`, every
    /// logging category with its label. Sent after diversityPatternVersion,
    /// only to a peer at minor 11 whose hello declared logCategoryList 1,
    /// on a Core with a radio model; any other peer's are today's.
    int logCategoryListVersion = 0;
    /// Phone wire batch: 1 means each `stationRadios` record carries
    /// `modelLabel` and `models` (the models that radio can run as). Sent
    /// after logCategoryListVersion, only to a peer at minor 11 whose hello
    /// declared radioModels 1 (radioModelsEntry), 0 on a Core that keeps no
    /// radio list (stationRadiosVersion 0); any other peer's capabilities
    /// and records are today's.
    bool radioModelsEntry = false;
    int radioModelsVersion = 0;
    /// The phone's direct addresses: 1 means `devices` carries
    /// `coreAddresses`, where a device can dial this Core. Sent after
    /// radioModelsVersion, only to a peer at minor 11 whose hello declared
    /// coreAddresses 1 and deviceAuth 1 and that signed in with a paired
    /// device's own key, on a Core with a radio model and the devices
    /// object; any other peer's capabilities and devices object are today's.
    int coreAddressesVersion = 0;
    /// iPhone app plan Task 23 (R-IOS-09): 1 means the device may ask for
    /// its own Opus bitrate (`opusBitrate` in the audio control, one of the
    /// catalogue's `audio.opusProfiles`). Optional and last at minor 11,
    /// for a peer that declared audioQuality 1 while the Core offers media.
    /// 0 (absent): the device gets the Core's audio_bitrate, as before.
    int audioQualityVersion = 0;
    /// JJ's ruling of 2026-09-28: 1 means the Core's `stationTci` object
    /// carries the rest of its TCI server's settings (the eleven
    /// StationTciModel::settingsTable() properties) and takes
    /// setStationTciSettings. Optional and last at minor 11, for a peer
    /// that declared stationTciSettings 1 on a Core at stationTciVersion 2.
    /// 0 (absent): the peer sees today's `stationTci`.
    int stationTciSettingsVersion = 0;
    /// R-R3-46 / R-R3-11: 1 means `stepAtt` carries rx2AttenuationDb (the
    /// attenuator of the ADC slice A is not on, two-way) and rx2SliceMask
    /// (the slices on that ADC). Sent at minor 11 only to a peer whose hello
    /// declared adcAttenuators 1, after stationTciSettingsVersion, and only
    /// by a Core that offers `stepAtt` (radioHardwareVersion 1 or more); that
    /// peer alone gets the two properties. 0 (absent): the peer sees today's
    /// wire, and every slice reads attenuationDb.
    int adcAttenuatorVersion = 0;
    /// R-R3-49 / R-IOS-18: 1 means the Core sends its PA Gain profiles on
    /// the read-only `paProfiles` object and takes the paProfile verbs
    /// (select, new, copy, delete, reset, setGain, setAdjust, setMaxPower,
    /// setUseMax), each gated as the Core gates the desktop's own PA profile
    /// writes. Only for a peer at minor 11 that declared paProfiles 1, after
    /// adcAttenuatorVersion.
    int paProfileVersion = 0;
    /// RADE status: 1 means every slice carries `radeSynced` and
    /// `radeFreqOffsetHz`, the RADE decoder's sync and frequency offset the
    /// VFO flag shows. Sent after radioModelsVersion, only to a peer at
    /// minor 11 whose hello declared radeStatus 1, on a Core with a radio
    /// model; any other peer's capabilities and slices are today's.
    int radeStatusVersion = 0;
    /// HL2 port part 2: 1 means `radio` carries `txInhibitReason`, why the
    /// Core's transmit is held off in plain words (the HL2 I/O board's
    /// "I/O Board: Fault Code N"), empty when the reason is the plain TX
    /// inhibit or none. Only for a peer at minor 11 that declared
    /// txInhibitReason 1, after radeStatusVersion.
    int txInhibitReasonVersion = 0;
    /// PA on-air gate re-review, Important C: 1 means `radio` carries
    /// `paTransmitBand`, the PA row the Core holds on the air. Sent after
    /// radeStatusVersion, only to a peer at minor 11 whose hello declared
    /// paTransmitBand 1, on a Core with a radio model; any other peer's
    /// capabilities and radio object are today's.
    int paTransmitBandVersion = 0;
    /// Slice control plan Task 4: 1 means the Core sends a `SliceAccess`
    /// object per slice (`access:<id>`), the joined slices as `slice:<id>`
    /// and every other as `marker:<id>`, and takes slice.listen,
    /// slice.stopListening, slice.takeControl and slice.release. Appended
    /// after paTransmitBandVersion in the minor-11 block, and only to a
    /// peer whose hello declared `sliceAccess` 1 with sessionHolder and
    /// deviceAuth (sliceAccessEntry); any other peer is sent no entry and
    /// reads 0, so its descriptor is exactly today's.
    bool sliceAccessEntry = false;
    int sliceAccessVersion = 0;
    /// R-R3-47 / R-R3-22: 1 means the Core mirrors its Power Genius XL
    /// status as the read-only `amplifier` object. Sent after
    /// radioHardwareVersion in the same minor-11 block. 0: a window shows
    /// no Power Genius readings from this Core. 2 adds configurePgxl,
    /// disconnectPgxl and setPgxlConnectionSettings; 3 adds the amp's own
    /// settings (the pgxl* properties of the read-only `accessorySettings`
    /// object and the setPgxlName, setPgxlHardware, setPgxlNetwork,
    /// savePgxlSettings and readPgxlSettings commands).
    int remotePgxlControlVersion = 0;
    /// R-R3-47 / R-R3-22: 1 means the Core mirrors its RF-Kit RF2K-S status
    /// as the read-only `rfkit` object; 2 adds the interface, antenna and
    /// tuner rows, band follow, and the configureRfKit, disconnectRfKit and
    /// setRfKitEnabled commands. Sent in the same block.
    int remoteRfKitControlVersion = 0;
    /// R-R3-48: 1 means the Core runs its own TCI server on the station
    /// network, mirrored as the read-only `stationTci` object and switched
    /// by the setStationTci command. Sent in the same block. 0: a
    /// window's TCI switch changes only its own server. 2 (remote-window
    /// parity Task 23, with recordStreamVersion 1) adds the `tciClients`
    /// record stream (the apps on that server), disconnectStationTciClient
    /// and setStationTciOptions, and the object's four option properties.
    int stationTciVersion = 0;
    /// R-R3-47 / R-R3-22: 1 means the Core mirrors its accessory records
    /// and settings as the read-only `accessoryData` object (fault
    /// history, connection counters, interlock policy, output limit and
    /// its alert, tune memory, antenna names) and takes the
    /// setTxInterlockPolicy, setPgxlPowerCap and clearAccessoryFaults
    /// commands. Sent last in the same block.
    int accessoryDataVersion = 0;
    /// Task 42: transmit-coupled accessory command family, minor 11.
    int accessoryTxVersion = 0;
    /// R-R3-47 / R-R3-22: 1 means the Core sends its Tuner Genius's own
    /// settings (the tgxl* properties of `accessorySettings`) and takes the
    /// setTgxlName, setTgxlNetwork, saveTgxlSettings and readTgxlSettings
    /// commands. Sent in the same minor-11 block. 0: a window cannot
    /// change the tuner's own settings on this Core and says so.
    int remoteTgxlControlVersion = 0;
    /// iPhone app Task 12 (R-IOS-08): 1 means the Core has its own identity
    /// key and signs in paired devices by key (the hello's `identity` and
    /// `challenge`, auth.request's `device`). Sent last in the same
    /// minor-11 block. A client learns the same from the hello's
    /// `features.deviceAuth`, which it needs before capabilities arrive;
    /// this entry is what a signed-in window reads afterwards.
    int stationIdentityVersion = 0;
    /// iPhone app Task 13 (R-IOS-08): 1 means the Core sends the `devices`
    /// object (its paired devices, label, claim, token and key backup) to a
    /// device whose hello declares `deviceAuth` 1, and takes devices.revoke,
    /// station.rename, station.acknowledgeKeyBackup and station.retireToken.
    /// Sent in the same minor-11 block, after stationIdentityVersion.
    int deviceAdminVersion = 0;
    /// iPhone app Task 14 (R-IOS-08): 1 means the Core pairs devices (the
    /// `pair.*` messages, which a client learns before capabilities from the
    /// hello's `features.pairing`), keeps `pairingWindowOpen` and
    /// `pairingCode` on the `devices` object, and takes `pairing.open` and
    /// `pairing.close`. Sent in the same minor-11 block, after
    /// deviceAdminVersion.
    int pairingVersion = 0;
    /// iPhone app Task 19 (R-IOS-06): 1 means the Core sends the read-only
    /// `catalog` object (the modes, filter presets, tune steps, AGC and
    /// gauge ranges, board, band plans, palettes, slice colours and tools
    /// an app draws its controls from). Sent last in the same minor-11
    /// block.
    int stationCatalogVersion = 0;
    /// Task 43: a peer declaring setupDescription receives the read-only
    /// `setup` object; zero omits the capability from an older peer's wire.
    int setupDescriptionVersion = 0;
    /// iPhone app Task 20 (R-IOS-27): 1 means a spectrum subscription may
    /// ask the Core for display extras (peak blobs, the active peak hold
    /// row, the noise floor, the waterfall's levels) and for normalise,
    /// calibration and averaging applied at the Core; the Core then sends
    /// an NSDX datagram beside each NSDC frame (display extras v1). Sent
    /// last in the same minor-11 block, after stationCatalogVersion.
    /// 2 (R-IOS-27, R-IOS-06) adds the media control operation
    /// clarity-retune, Clarity's Re-tune for one endpoint.
    int displayExtrasVersion = 0;
    /// R-R3-49 (parity Task 1): 1 means a receive-only Core takes a
    /// `transmit` write of any property but the keying set (mox, tune,
    /// voxEnabled, twoToneActive) and a DspOptions<Setting><Mode>Tx settings
    /// write or remove while its radio is off the air, and applies it at
    /// once; each is refused while the radio is on the air. Sent in the
    /// same minor-11 block, after displayExtrasVersion. 0: a window's
    /// transmit settings stay greyed and say the Core cannot take them.
    /// 9 also offers validated BandPlanRegion edits and the TX passband guard.
    /// 10 (iPhone app plan Task 40) adds `transmit.micMuted`, the mic mute.
    /// 11 offers Transmit > Power's "Disable HF PA" (DisableHfPa),
    /// taken on and off the air and applied to the radio at once.
    /// 12 (addendum G-42) adds the Core's Extended transmit setting,
    /// `ExtendedTransmit`, taken with transmit permission and off the air.
    /// 13 (kTransmitSettingsOnAirVersion): the transmit settings a local
    /// window changes while transmitting are taken on the air too; the
    /// on-air refusal above applies only below 13.
    int transmitSettingsVersion = 0;
    /// R-IOS-27, R-IOS-06: 1 means the Core takes `slice.selectBand`, which
    /// runs the desktop's band button on a slice (its saved frequency, mode
    /// and filter for that band come back), for a band the catalogue's
    /// `bands` lists. Sent in the same minor-11 block, after
    /// transmitSettingsVersion. 0: an app's band buttons stay greyed.
    int bandSelectVersion = 0;
    /// R-R3-13 / R-R3-49 (remote-window parity Task 15): 1 means the Core's
    /// slices carry its ADC and AGC readings (adcPeakDbfs, adcAverageDbfs,
    /// agcGainDb, agcPeakDb, agcAverageDb), refreshed at its meter pump's
    /// rate, and a window's Multimeter polling delay sets that rate at once.
    /// Sent in the same minor-11 block, after bandSelectVersion. 0 (no radio
    /// model, or no meter pump): a window's ADC and AGC meters show no
    /// reading.
    int meterReadingsVersion = 0;
    /// R-R3-49 / R-R3-21 / R-R3-40 (remote-window parity Task 16): 1 means
    /// the Core says how long its last DSP Options apply took (`radio`
    /// dspOptionsLastApplyMs), each slice's minimum notch width
    /// (minNotchWidthHz), and takes dsp.filterResponse for the filter
    /// graph's curve. Sent in the same minor-11 block, after
    /// meterReadingsVersion. 0 (no local radio model): a window shows its
    /// filter curve unavailable. Which noise reduction the Core runs is
    /// DspAssetService's (dspAssetVersion 3 and 4).
    int dspInfoVersion = 0;
    /// R-IOS-25 / R-R3-49 (remote-window parity Task 19): 1 means the Core
    /// takes records.subscribe and records.unsubscribe and sends
    /// record.batch for its `spots` stream (the newest 500) and each station
    /// source's spotConsole:<source> stream (the last 200 lines), sends the
    /// read-only `spotSources` object, and takes spots.connect,
    /// spots.disconnect, spots.sendCommand and spots.clearAll. Sent in the
    /// same minor-11 block, after dspInfoVersion. 0 (no local radio model):
    /// a window shows the station's spot sources disabled with a reason.
    int recordStreamVersion = 0;
    /// R-IOS-18 / R-R3-49 (remote-window parity Task 21): 1 means the Core
    /// chooses its radio from an app: it sends the `stationRadios` record
    /// stream ({id, name, model, mac, address, protocol, inUse}) and takes
    /// station.selectRadio, station.rescanRadios, station.setRadioModel and
    /// station.forgetRadio. Sent in the same minor-11 block, after
    /// recordStreamVersion. 0 (a Core that is not nereusd, or older): a
    /// window shows This Core's Change radio disabled with a reason.
    int stationRadiosVersion = 0;
    // Task 24: Core-owned settings validation and per-MAC hygiene commands.
    int settingsHygieneVersion = 0;
    std::optional<CoreBuildInfo> coreBuildInfo;
    /// The direct media ladder (link section "Direct media"): 1 means the
    /// Core takes a media `replace` carrying `mediaDirectVersion` 1 and
    /// makes that replacement direct only (host and STUN candidates, no
    /// relay, no tunnel). Sent before coreBuildInfo, only to a peer with
    /// media whose hello declared `mediaDirect` 1; 0 otherwise.
    int mediaDirectVersion = 0;
    /// With mediaDirectVersion: the Core's STUN servers, `stun:` and
    /// `stuns:` URLs only, as a JSON array (utf8). A device uses them for
    /// its media connections and never stores them.
    QStringList mediaStunUrls;
    /// Level Cal 2: 1 means the catalogue's board carries rx2Attenuator,
    /// rx2PreampItems and rx2AttenuatorReason (RX2's own input control).
    /// Sent after the direct media ladder and before coreBuildInfo, only
    /// to a peer whose hello declared `rx2Attenuator` 1; 0 otherwise.
    int rx2AttenuatorVersion = 0;
    /// Radio codec lane: 1 means the catalogue's board carries radioMic
    /// and radioMicNote (whether the radio's own mic can be chosen, and the
    /// note that goes with it). Sent after rx2AttenuatorVersion and before
    /// coreBuildInfo, only to a peer whose hello declared `radioMic` 1; 0
    /// otherwise. Version 2 additionally offers authenticated, session-scoped
    /// tx.setMicSource to peers declaring radioMic 2 and remoteTx 1. Catalogue
    /// version 1 alone never offers microphone selection.
    int radioMicVersion = 0;
    /// Shared-input filters, ruling (d): 1 means radio carries
    /// rxFilter0LowPassReason (why the receive low-pass on chain 0's input
    /// is held for one slice) and rxFilter0LowPassSlice (that slice's id, -1
    /// for none). Sent after radioMicVersion and before coreBuildInfo, only
    /// to a peer whose hello declared `rxFilterLowPass` 1; 0 otherwise.
    int rxFilterLowPassVersion = 0;
    /// RADE reason: 1 means every slice carries `radeReason`, why the slice
    /// is in RADE with no working decoder in plain words (empty while it
    /// decodes). Sent after rxFilterLowPassVersion and before coreBuildInfo,
    /// only to a peer whose hello declared `radeReason` 1; 0 otherwise.
    int radeReasonVersion = 0;
    /// At most this many URLs are read, each at most kMaxMediaStunUrlBytes.
    static constexpr int kMaxMediaStunUrls = 8;
    static constexpr int kMaxMediaStunUrlBytes = 512;
    /// Version 1 offers read-only paired Core settings XML export.
    int settingsBackupVersion = 0;
    /// R-R3-49 / A11 (remote-window parity Task 28): 1 means the Core sends
    /// the transmit analyzer's display, not the receiver's, for a pan on the
    /// transmitting slice while it is keyed, to a media peer that declared
    /// txDisplayVersion in its start (the media document's "Transmit
    /// display"), and txState carries highSwr and swrWindBackLatched. Sent in
    /// the same minor-11 block, after stationRadiosVersion. 0 without media
    /// or on a station with no TX analyzer. 2 (remote-window parity Task
    /// 30, A12) adds: a window's write or removal of one of Setup > Display
    /// > TX Display's nine analyzer keys reaches the Core's TX analyzer at
    /// once, on and off the air; below 2 a window shows those nine controls
    /// disabled. 3 (remote-window parity Task 31, A11) adds: a peer that
    /// declares 3 in its start may add `duplex` (display duplex) to a
    /// subscribe, and that display keeps the receiver while keyed; below 3
    /// a window shows its DUP controls disabled.
    int txDisplayVersion = 0;
    /// R-R3-21 / R-R3-08: 1 means the Core stamps every display frame's
    /// producerTimestamp and its clock-echo times (t1, t2, capturedNs) from
    /// one clock, so a window can present its spectrum and waterfall on its
    /// audio's playout clock. No message changes shape. Sent in the same
    /// minor-11 block, after txDisplayVersion; 1 whenever media is on.
    /// 0 (an older Core): a window draws each display frame on arrival.
    int displayClockVersion = 0;
    /// iPhone app plan Task 28 fix wave (R-IOS-16; the safety review's
    /// Important 5): 1 means the Core answers an introduction through the
    /// remote access service with the control session over a data channel
    /// (link section 20), presenting its bound certificate in DTLS. A
    /// device records it with the paired Core and offers connecting from
    /// anywhere only to a Core that declared it. Sent in the same minor-11
    /// block, after displayClockVersion. 0 (an older Core, or one with no
    /// identity key): connecting from anywhere is shown disabled with a
    /// reason.
    int controlChannelVersion = 0;
    /// R-IOS-13 / R-R3-49 (remote-window parity Task 32): 1 means the Core
    /// sends the transmit monitor (MON) to the device that holds transmit,
    /// in its own media audio, while its radio is on the air and MON is on:
    /// a media peer that declares txMonitorAudioVersion in its start may
    /// send monitor-audio (route speakers, headphones or none) and is
    /// answered with monitor-audio-context (the media document's "Transmit
    /// monitor (monitor-audio)"). Sent in the same minor-11 block, after
    /// controlChannelVersion; 1 whenever media is on with the Core's own
    /// radio model, 0 otherwise. 0 (an older Core): a window shows its MON
    /// output pair disabled with a reason.
    int txMonitorAudioVersion = 0;
    /// Task 23: one dedicated ordered raw-I/Q media stream for a window's
    /// TCI apps, only after the media start declares this version.
    int remoteIqVersion = 0;
    /// A media peer may declare version 1 and subscribe to a receiver-scoped
    /// mini display with displayRole="mini". Absent means a normal pan.
    int miniDisplayVersion = 0;
    /// R-IOS-26 / R-R3-49 (iPhone app plan Task 22, remote-window parity
    /// Task 20): 1 means the Core runs FreeDV Reporter itself, registered
    /// with its own callsign, grid square and message and listing its own
    /// RADE slice: it sends the `freedvStations` record stream (the newest
    /// 1000 stations), the FreeDV Reporter console as
    /// spotConsole:freedvReporter and its state in `spotSources`
    /// (freedvReporterState, freedvReporterText, freedvReporterHidden), and
    /// takes spots.connect / spots.disconnect for source freedvReporter and
    /// freedv.setMessage, freedv.sendQsy and freedv.setHidden. Sent in the
    /// same minor-11 block, after txMonitorAudioVersion; 1 whenever
    /// recordStreamVersion is 1. 0 (an older Core): a window shows its
    /// FreeDV Reporter controls disabled with a reason.
    int stationFreedvVersion = 0;
    /// iPhone app plan Task 29 (R-IOS-16; link section 21.3): 1 means the
    /// Core takes the media `replace` operation (the remote media control
    /// document, "Replacing the media connection"). Sent in the same
    /// minor-11 block, after stationFreedvVersion; 1 whenever media is on
    /// for this peer.
    /// 0 (an older Core, or media off): a device keeps its media connection
    /// when its session moves.
    int mediaReplaceVersion = 0;
    /// iPhone app plan Task 29 (link section 21.2): 1 means the Core takes
    /// session.pathTicket and path.join and moves a session to another
    /// connection without ending it. Sent in the same block, after
    /// mediaReplaceVersion. 0: a device keeps its session on the path its
    /// race chose.
    int controlSwitchVersion = 0;
    /// iPhone app plan Task 29 (link section 21.1): false when the Core's
    /// nereusd.conf has `relay = deny`, true otherwise. Sent in the same
    /// block, after controlSwitchVersion. A device records it and leaves
    /// the relay out of its races while it is false. An older Core sends
    /// none: relayAllowedEntry false, and the relay is tried.
    bool relayAllowedEntry = false;
    bool relayAllowed = true;
    /// Remote-window parity Task 22 / iPhone app plan Task 25 (R-R3-49,
    /// R-IOS-18): 1 means the Core takes support.collect and
    /// support.setLogCategories, sends the `coreLog` record stream and
    /// `radio`'s `logCategories`. Sent in the same block, after
    /// relayAllowed. 0 (an older Core): a window shows its Core-side
    /// support controls disabled with the reason.
    int supportBundleVersion = 0;
    /// iPhone app plan Task 29 step 2b (R-IOS-16; link section 21, "The
    /// media tunnel"): 1 means the Core carries a media connection's
    /// datagrams inside a direct WebSocket session, as binary messages,
    /// when the window's media start declares it. Sent in the same minor-11
    /// block, after all previously emitted minor-11 entries; 1 whenever
    /// media is on for the peer. 0
    /// (an older Core): media on a direct session runs over UDP only.
    int mediaTunnelVersion = 0;
    /// Version 1 prefixes relay tag-2 payloads with the media connection's
    /// 16-byte UUID, preserving independent generations during replacement.
    int mediaRelayRoutingVersion = 0;
    /// R-IOS-13 / R-R3-49 (iPhone plan Task 39 row A10): 1 means the Core
    /// sends its AM Mod Monitor readings on the txAmModulation and
    /// txAmModulationFeedback record streams (to a subscribing peer, while
    /// its radio is keyed in AM, SAM or DSB), takes txModMonitor.reset and
    /// applies a window's ModMon/FbStream. Sent in the minor-11 block,
    /// after remoteIqVersion and all earlier optional entries. 0: a window shows its
    /// Mod Monitor disabled with the reason.
    int txModMonitorVersion = 0;
    /// iPhone app Task 71 (R-IOS-02; the several-devices design, ruling
    /// 10.1): 1 means the Core admits up to four devices at once, sends the
    /// `connectedDevices` object and takes session.leave. Sent before the
    /// trailing media-floor versions in the same minor-11 block, and only
    /// to a peer whose hello declared the
    /// feature `sessionHolder` 1 with `deviceAuth` 1 (sessionHolderEntry);
    /// any other peer is sent no entry and reads 0, so its capabilities
    /// are exactly today's.
    bool sessionHolderEntry = false;
    int sessionHolderVersion = 0;
    /// iPhone app plan Task 34: remote transmit (txPermitted per session,
    /// tx.setTxSlice, the on-air refusals). Sent before the trailing
    /// media-floor versions, only to a peer whose
    /// hello declared remoteTx 1 (remoteTxEntry); 0 otherwise.
    bool remoteTxEntry = false;
    int remoteTxVersion = 0;
    /// Direct WSS transmit-watch attachment for a paired, permitted peer
    /// that declared txWatchPath 1. Absent for every other peer.
    int txWatchPathVersion = 0;
    /// Desktop remote transmit (R-IOS-13, R-R3-42): why txPermitted is
    /// false, as the Core's refusal (link section 18.3): its code, its
    /// sentence and its fix. Sent right after remoteTxVersion and only with
    /// it; all three empty while permitted.
    QString txRefusalCode;
    QString txRefusalReason;
    QString txRefusalFix;
    /// iPhone app plan Task 39 (D14, R-IOS-13): 1 means the Core sends the
    /// read-only `txState` object (TransmitState: keyed, who keyed, the
    /// time left, the transmit meters and why the Core last stopped a
    /// transmission). Sent after remoteTxVersion and its three txRefusal*
    /// entries, and only with them.
    int txStateVersion = 0;
    /// Remote-window parity Task 33 (R-R3-49, R-R3-32): 1 means `txState`
    /// also carries forwardAdcRaw and reflectedAdcRaw (the radio's raw
    /// forward and reflected power readings) and the Core keeps the
    /// `txCfcCompression` record stream (the CFC bar chart). Sent right
    /// after txStateVersion and only with it.
    /// 2 also carries Core-scaled PA raw forward watts and forward/reverse
    /// voltage (outbound Float64), following the same raw sample cadence.
    /// 3 also carries the seven stage readings a local window's container
    /// meters show (eqDb, levelerDb, levelerGainDb, cfcDb, cfcGainDb,
    /// alcGainDb, alcGroupDb; A9), read with the other meters.
    int txReadingsVersion = 0;

    /// Whether the DAEMON currently holds a live radio connection. A
    /// client that authenticated against a daemon whose radio is powered
    /// off must not present itself as Connected: every slice control it
    /// offers would reach a RadioModel that cannot act on it. See
    /// StationClient::applyCapabilities().
    bool radioConnected = false;

    // ---- Limits (see the header comment: EFFECTIVE, not board) ----
    int effectiveMaxSlices = 1;
    int boardMaxSlices = 1;   ///< diagnostics only; never gate on this
    int userDdcCount = 0;

    // ---- Feature bits ----
    bool pureSignalPresent = false;

    /// Always false in R2. TX is R4 in its entirety (design addendum
    /// section 2: "no MOX"), and section 12.2 requires TX stay disabled
    /// until the snapshot-complete marker has arrived regardless.
    bool txPermitted = false;

    /// Zero means control-only. Nonzero is advertised only when a daemon
    /// media controller is installed; negotiated session minor still gates it.
    int remoteMediaVersion = 0;
    int remoteWidebandDisplayVersion = 0;
    /// Audio contexts carry the accepted encoder profile or the reason audio
    /// is off. Nonzero only with media; negotiated minor still gates it.
    int remoteAudioStatusVersion = 0;
    /// Spectrum contexts report the grant Core made for the endpoint.
    /// Nonzero only with media; negotiated minor still gates it.
    int spectrumGrantVersion = 0;
    int remoteDisplayBudgetVersion = 0;
    std::optional<DisplayBudgetLimits> displayBudget;
    bool remotePs3DisplaySubscribed = false;
    /// Why the advertised display budget is below the Core's ceiling
    /// (R-R3-08, R-R3-37). A separate entry from the five budget fields,
    /// which older apps require to be exactly five: sent only with a budget
    /// and only to a peer that negotiated
    /// kDisplayBudgetReasonSessionProtocolMinor, parsed on its own, and
    /// dropped when the budget is not usable. nullopt: not sent, or a
    /// reason this build does not know.
    std::optional<DisplayBudgetReason> displayBudgetReason;
    int remoteCtunVersion = 0;
    /// 1: radio and audio telemetry. 2: adds the Core host section (CPU,
    /// memory, temperature). 3: adds the receivers section (each receiver's
    /// processing load and input wait). 4 (remote-window parity Task 6,
    /// minor 11): adds the radio's PA readings and link quality to the
    /// radio section. 5 (parity Task 14, minor 11): adds the Core's HL2 link
    /// (hl2*). Negotiated minor still gates each version.
    int stationTelemetryVersion = 0;
    int remoteTgxlConfigVersion = 0;
    int remoteFourO3AControlVersion = 0;
    int wdspVersion = 0;
    int wdspCompatibilityVersion = 0;
    int nnrVersion = 0;
    int psAlgorithmVersion = 0;
    int propertyResultVersion = 0;
    int dspAssetVersion = 0;
    int psDisplayVersion = 0;
    /// R-R3-21 / R-R3-09: the Core owns the notch list, mirrors it as the
    /// `notches` object and takes notch.add / notch.move / notch.setActive
    /// / notch.delete. 0 means a window keeps today's settings-based notches.
    /// 2 (R-IOS-27, R-IOS-06) adds notch.addAtSlice, the desktop's +TNF on
    /// a slice, with the Core composing the notch.
    int notchControlVersion = 0;
    /// R-R3-23: 1 means the Core can send lossless audio (uncompressed
    /// 16-bit stereo, L16) beside Opus. A GUI that sees it may add
    /// audioProfileVersion to its media start (the offer then carries the
    /// L16 format) and `profile` to its audio control; the audio context
    /// then reports the profile running and any refusal. Nonzero only with
    /// media; the Core's audio_lossless setting may still refuse.
    int audioProfileVersion = 0;
    /// R-R3-35: 1 means the Core answers the media control
    /// {op:"clock-probe", id, t0} with {op:"clock-echo", id, t0, t1, t2,
    /// generation, rtpTimestamp, capturedNs}, so a GUI can measure how far
    /// behind real time its audio plays. Nonzero only with media. A GUI that
    /// does not see it sends no probe and shows no measured delay.
    int audioClockVersion = 0;
    /// R-R3-43: 1 means the Core can send a receiver's own audio on its own
    /// stream beside the speakers' mix. A GUI that sees it may add
    /// receiverAudioVersion to its media start (the offer then declares the
    /// receiver stream ids) and send {op:"receiver-audio", connectionId,
    /// sliceId, revision, enabled, profile}; the Core answers each with a
    /// receiver-audio-context. Nonzero only with media. A GUI that does not
    /// see it sends no receiver request and gets no receiver stream.
    int receiverAudioVersion = 0;
    /// R-R3-45: 1 means the Core can send the headphones mix (the receivers
    /// routed to the headphones) on its own stream beside the speakers'
    /// mix. A GUI that sees it may add headphonesMixVersion to its media
    /// start (the offer then declares the headphones stream id and the main
    /// stream carries the speakers' mix alone) and send {op:
    /// "headphones-audio", connectionId, revision, enabled, profile}; the
    /// Core answers with a headphones-audio-context. Nonzero only with
    /// media. A GUI that does not see it gets today's wire.
    int headphonesMixVersion = 0;

    /// The daemon's own AppSettings SettingsSchemaVersion, read by that
    /// key name from its own store. See StationClient's schema-skew check.
    qint32 settingsSchemaVersion = 0;

    /// The wire form: MirrorUpdate reused as a generic {name, kind, value}
    /// triple, exactly as CommandInvoke reuses it for arguments. `ordinal`
    /// is meaningless here and is always 0.
    QList<MirrorUpdate> toUpdates() const;

    /// Inverse of toUpdates(). Unknown entry names are IGNORED, not
    /// rejected: a newer daemon advertising a capability this client has
    /// never heard of is the expected forward-compatible case under
    /// section 7.0's "negotiate down on minor" policy, not a protocol
    /// error. Absent entries keep this struct's own defaults.
    static StationCapabilities fromUpdates(const QList<MirrorUpdate>& updates);
};

} // namespace NereusSDR
