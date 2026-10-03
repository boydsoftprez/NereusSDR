// =================================================================
// src/core/session/MirrorPolicy.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 7.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30 - RADE reason: SliceModel radeReason Outbound, gated on
//                 radeReason (radeReasonVersion 1). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (ruling (d)): RadioModel
//                 rxFilter0LowPassReason and rxFilter0LowPassSlice
//                 Outbound, gated on rxFilterLowPass
//                 (rxFilterLowPassVersion 1). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: RadioModel levelCalRunning, levelCalPercent,
//                 levelCalMessage and levelCalSucceeded Outbound, gated on
//                 levelCalibration (radioHardwareVersion 12). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: RadioModel alexLpfBits Outbound, gated
//                 on alexLpf (radioHardwareVersion 10). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - RADE status: SliceModel radeSynced and radeFreqOffsetHz
//                 Outbound, gated on radeStatus (radeStatusVersion 1).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 7: mirror
//                                    direction table. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-40: SliceModel nnrLimit is
//                                    Outbound. AI-assisted implementation
//                                    via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-45: SliceModel outputRoute is
//                                    Bidirectional. AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the Core's step
//                                    attenuator and preamp (`stepAtt`).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: AlexAntennaFacade directions. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46 fix wave: IoBoardHl2Facade, all Outbound. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-47 / R-R3-22: AmplifierModel and RfKitModel, all
//                 Outbound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-45: SliceModel outputRoute is Outbound until the
//                 headphones plan's remote window task makes it two-way.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-48: RfKitModel rows, bandFollow,
//                StationTciModel, all Outbound; rfKitEnabled Outbound. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - Merge: lane B's headphones Task 2 makes outputRoute
//                 two-way; the Outbound entry is removed. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: AccessoryDataModel, all Outbound. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: AccessorySettingsModel, all Outbound.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 fix wave: RadioModel transmitting Outbound. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: DspAssetService's dfnrModelStatus
//                and dfnrRunnable, Outbound. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-25 - R-R3-49, Sub-epic C-1: DspAssetService's mnrStatus and
//                mnrRunnable, Outbound. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 13 (R-IOS-08): StationDevicesFacade, all
//                 Outbound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 - iPhone app Task 14 (R-IOS-08): StationDevicesFacade's
//                 pairingWindowOpen and pairingCode, Outbound. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app Task 71 (R-IOS-02): ConnectedDevicesFacade,
//                 all Outbound. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-26 - Parity Task 19 (R-IOS-25): SpotSourceHost, all Outbound.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - iPhone plan Task 22 / parity Task 20 (R-IOS-26):
//                SpotSourceHost's FreeDV Reporter state and hidden flag,
//                Outbound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-24 - iPhone app Task 19 (R-IOS-06): StationCatalog, all
//   2026-09-24 - R-R3-49 (parity Task 2): the TX and Phone/CW applets'
//                 thirteen TransmitModel settings Bidirectional;
//                 tunePowerForTxBand and tuneDrivePowerSource Outbound.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): the seven radio microphone
//                 settings Bidirectional; activeTxProfile and txProfilesJson
//                 Outbound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 4): the TX EQ, CFC, phase rotator,
//                 CESSB, leveler and ALC settings Bidirectional. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 5): tuneDrivePowerSource becomes
//                 Bidirectional; the Power, DEXP/VOX and two-tone settings
//                 and the step attenuator's ATT on TX, its value and Force
//                 ATT Bidirectional. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 6): RadioModel txInhibited
//                 Outbound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 - iPhone app Task 73 (R-IOS-02): SliceMarker, all Outbound.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 35 (R-IOS-13): TransmitModel's mox
//                 and tune Outbound (the transmit verbs key). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan, desktop remote transmit (R-IOS-13):
//                 TransmitModel's voxEnabled Bidirectional. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 39 (D14, R-IOS-13): TransmitState,
//                 all Outbound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 10): AccessoryDataModel's five
//                 rfkit* connection counts Outbound. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-46 (parity Task 12): AlexAntennaFacade's
//                 txAntennas, blockTxAnt2, blockTxAnt3, ext1OutOnTx,
//                 ext2OutOnTx and rxOutOverride Bidirectional. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-46 (parity Task 14): IoBoardHl2Facade outputs,
//                 Outbound. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (parity Task 15): SliceModel's ADC and
//                 AGC readings (adcPeakDbfs, adcAverageDbfs, agcGainDb,
//                 agcPeakDb, agcAverageDb) Outbound. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave: I4 TransmitState's holder
//               properties Outbound. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26 - R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16): RadioModel
//                 noiseReductionMethods and dspOptionsLastApplyMs, SliceModel
//                 minNotchWidthHz, all Outbound. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 (trunk merge of parity Tasks 16 to 18):
//                 noiseReductionMethods dropped (DspAssetService is the one
//                 noise reduction source); an older Core's DFNR and MNR are
//                 shown disabled with the "does not say" reason. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Parity Task 28 (R-R3-49, A11): TransmitState highSwr and
//                 swrWindBackLatched Outbound (txDisplayVersion 1). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-R3-49 / R-IOS-18 (remote-window parity Task 22):
//                 RadioModel logCategories, Outbound. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Parity Task 33 (R-R3-49, R-R3-32): TransmitState
//                 forwardAdcRaw, reflectedAdcRaw and compressionDb Outbound
//                 (txReadingsVersion 1). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - StationTciModel's other eleven settings Outbound
//                 (stationTciSettingsVersion 1). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Parity Task 23 (R-R3-48, R-R3-42): StationTciModel's four
//                 options Outbound (stationTciVersion 2). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - A9 (iPhone app plan Task 39): TransmitState's seven stage
//                 readings (eqDb .. alcGroupDb) Outbound (txReadingsVersion
//                 3). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-28 - iPhone app plan Task 25: StationVax, the `vax` object
//                 (vaxVersion 1). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - iPhone app plan Task 40: TransmitModel micMuted
//                 Bidirectional (transmitSettingsVersion 10). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-IOS-13 / R-R3-49: TransmitModel txEqCurve Outbound
//                 (txEqCurveVersion 1). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - Merge of the phone wire batch: SliceModel diversityPattern
//                 and RadioModel logCategoryList join featureGates, so a
//                 window that did not declare them sees no schema skew.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Phone wire batch: SliceModel diversityPattern Outbound
//                 (diversityPatternVersion 1); RadioModel logCategoryList
//                 ConstantSnapshot (logCategoryListVersion 1). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The phone's direct addresses: StationDevicesFacade
//                 coreAddresses Outbound and in featureGates
//                 (coreAddressesVersion 1). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: paProfileVersion and the read-only
//                 paProfiles object (PaProfilesFacade). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: RadioModel txInhibitReason Outbound and
//                 in featureGates (txInhibitReasonVersion 1). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - transmitSettingsVersion 15: TransmitModel cfcProfile
//                 Outbound and in featureGates. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - PA on-air gate review: TransmitModel powerByBandJson and
//                 tunePowerByBandJson Outbound; the Core's RF and Tune
//                 sliders own the per-band maps and a peer's write is
//                 refused. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 - Slice control plan Task 4: SliceAccess, sliceId and
//                 incarnation ConstantSnapshot, the rest Outbound. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/MirrorPolicy.h"

// For MirrorSchema::shortClassName only: QMetaObject::className()
// reports "NereusSDR::SliceModel" while this table (and hand-written
// call sites) use the bare name, and both must resolve to one entry.
// MirrorPolicy.h stays free of the dependency.
#include "core/session/MirrorSchema.h"

#include <QHash>

#include <iterator>

namespace NereusSDR {

namespace {

// Total over the mirrored surface, class by class, in declaration order.
// SliceModel::sliceLetter is absent because MirrorSchema excludes it from
// the surface outright (CONSTANT, derived from sliceIndex, and the only
// QChar anywhere in the mirror); MeterModel is absent because the whole
// class is excluded.
//
// Three groups are worth reading twice, because nothing in the
// meta-object system would produce them on its own:
//
//   1. Some SliceModel properties are Outbound DESPITE carrying WRITE.
//      The R2 plan names seven -- chainIndex, ddcIndex, streamIndex,
//      shiftOffsetHz, sampleRateHz, widebandExtensionRequested and
//      psPaused -- and every one is codec- or coordinator-owned state
//      that the daemon computes and the GUI only displays. sampleRateHz
//      is the clearest case: it is a property of the DDC stream, not of
//      the slice, and the way to change it is
//      RadioModel::requestSliceSampleRate. Writing the property directly
//      moves the display and nothing else, and the next bind overwrites
//      it.
//
//      Whole-branch review, Minor 1: that seven is the PLAN's list, not
//      the current total. snrDb and lastRadeRxCallsign joined the same
//      category later and are argued at their own entries below, which
//      already say the plan's seven is "a floor, not a cap". Reworded
//      here because reading this paragraph alone gave the wrong answer,
//      and because a StateMirror.cpp comment quoting the same stale seven
//      had drifted out of agreement with this file.
//
//   2. panKey is Bidirectional. The R2 plan's step 5 deliberately
//      corrects the design addendum's section 6.1 here, which counts
//      panKey among the outbound-only names; the addendum's own prose in
//      the same paragraph agrees with the plan, so that was a counting
//      slip rather than a design dispute. It is mirrored outbound so a
//      reconnecting client restores its layout, and applied inbound under
//      the mirror's guard, with pan-affecting CREATION routed through the
//      addSliceOnPan verb instead of through a property write.
//
//   3. sliceIndex is ConstantSnapshot. It is the mirror's object
//      identity, it carries no NOTIFY, and without an explicit snapshot
//      read every object.create on the wire would be anonymous.
//
// Everything read-only in the metaobject is Outbound here too, which is
// belt and braces: MirrorSchema::write already refuses a property with no
// WRITE. Listing them keeps the table total, so the guard can name a
// newly added property instead of silently accepting it.
const MirrorPolicy::Entry kEntries[] = {
    // ---- SliceModel (154 entries) ----
    { "SliceModel", "frequency", MirrorDirection::Bidirectional },
    { "SliceModel", "dspMode", MirrorDirection::Bidirectional },
    { "SliceModel", "filterLow", MirrorDirection::Bidirectional },
    { "SliceModel", "filterHigh", MirrorDirection::Bidirectional },
    { "SliceModel", "agcMode", MirrorDirection::Bidirectional },
    { "SliceModel", "stepHz", MirrorDirection::Bidirectional },
    { "SliceModel", "afGain", MirrorDirection::Bidirectional },
    { "SliceModel", "rfGain", MirrorDirection::Bidirectional },
    { "SliceModel", "rxAntenna", MirrorDirection::Bidirectional },
    { "SliceModel", "txAntenna", MirrorDirection::Bidirectional },
    { "SliceModel", "active", MirrorDirection::Outbound },
    { "SliceModel", "txSlice", MirrorDirection::Outbound },
    { "SliceModel", "sliceIndex", MirrorDirection::ConstantSnapshot },
    { "SliceModel", "band", MirrorDirection::Outbound },
    // Task 12: per-slice S-meter reading. Outbound -- the daemon's
    // SliceMeterPump produces the value; a remote client never writes it
    // back. No WRITE accessor at all (unlike snrDb/lastRadeRxCallsign
    // below, which carry WRITE and are refused via the writable-but-
    // Outbound path instead), so an inbound value for THIS property can
    // only ever reach the object through SliceModel::applyMirroredValue's
    // hook -- the design doc's "Inbound-only telemetry" phrasing describes
    // this same direction from the client's point of view; this table is
    // the daemon's point of view, and Outbound is the correct entry here.
    { "SliceModel", "signalStrengthDbm", MirrorDirection::Outbound },
    { "SliceModel", "signalPeakDbm", MirrorDirection::Outbound },
    { "SliceModel", "signalAverageDbm", MirrorDirection::Outbound },
    { "SliceModel", "stationAutoAgcNoiseFloorDbm", MirrorDirection::Outbound },
    { "SliceModel", "stationAutoAgcNoiseFloorValid", MirrorDirection::Outbound },
    { "SliceModel", "stationAutoAgcNoiseFloorGeneration", MirrorDirection::Outbound },
    { "SliceModel", "chainIndex", MirrorDirection::Outbound },
    { "SliceModel", "ddcIndex", MirrorDirection::Outbound },
    { "SliceModel", "streamIndex", MirrorDirection::Outbound },
    { "SliceModel", "streamCtunPinned", MirrorDirection::Outbound },
    { "SliceModel", "streamEpoch", MirrorDirection::Outbound },
    { "SliceModel", "shiftOffsetHz", MirrorDirection::Outbound },
    { "SliceModel", "panKey", MirrorDirection::Bidirectional },
    { "SliceModel", "sampleRateHz", MirrorDirection::Outbound },
    { "SliceModel", "diversityEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "diversityPhaseDeg", MirrorDirection::Bidirectional },
    { "SliceModel", "diversityGainDb", MirrorDirection::Bidirectional },
    { "SliceModel", "diversityFineNullEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "widebandExtensionRequested", MirrorDirection::Outbound },
    { "SliceModel", "psPaused", MirrorDirection::Outbound },
    { "SliceModel", "locked", MirrorDirection::Bidirectional },
    { "SliceModel", "muted", MirrorDirection::Bidirectional },
    { "SliceModel", "audioPan", MirrorDirection::Bidirectional },
    // R-R3-45: speakers or headphones (VAX design 6.2). The Core owns it:
    // the Core's mixer splits the two mixes and the Core saves the choice;
    // a remote window writes it from the flag and plays the headphones mix
    // the Core sends while any receiver is on the headphones.
    { "SliceModel", "outputRoute", MirrorDirection::Bidirectional },
    { "SliceModel", "ssqlEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "ssqlThresh", MirrorDirection::Bidirectional },
    { "SliceModel", "amsqEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "amsqThresh", MirrorDirection::Bidirectional },
    { "SliceModel", "fmsqEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "fmsqThresh", MirrorDirection::Bidirectional },
    { "SliceModel", "agcThreshold", MirrorDirection::Bidirectional },
    { "SliceModel", "agcHang", MirrorDirection::Bidirectional },
    { "SliceModel", "agcSlope", MirrorDirection::Bidirectional },
    { "SliceModel", "agcAttack", MirrorDirection::Bidirectional },
    { "SliceModel", "agcDecay", MirrorDirection::Bidirectional },
    { "SliceModel", "autoAgcEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "autoAgcOffset", MirrorDirection::Bidirectional },
    { "SliceModel", "agcFixedGain", MirrorDirection::Bidirectional },
    { "SliceModel", "agcHangThreshold", MirrorDirection::Bidirectional },
    { "SliceModel", "agcMaxGain", MirrorDirection::Bidirectional },
    { "SliceModel", "ritEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "ritHz", MirrorDirection::Bidirectional },
    { "SliceModel", "xitEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "xitHz", MirrorDirection::Bidirectional },
    { "SliceModel", "nbMode", MirrorDirection::Bidirectional },
    { "SliceModel", "activeNr", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrModelSlot", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrMaskFloorDb", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrPosition", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrAlpha", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrAlphaKneeDb", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrTauSeconds", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrMaxGainDb", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrAttackMs", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrReleaseMs", MirrorDirection::Bidirectional },
    { "SliceModel", "nnrAvailable", MirrorDirection::Outbound },
    { "SliceModel", "nnrReady", MirrorDirection::Outbound },
    { "SliceModel", "nnrRunning", MirrorDirection::Outbound },
    { "SliceModel", "nnrStandardAvailable", MirrorDirection::Outbound },
    { "SliceModel", "nnrPremiumAvailable", MirrorDirection::Outbound },
    { "SliceModel", "nnrRateSupported", MirrorDirection::Outbound },
    { "SliceModel", "nnrActualModelSlot", MirrorDirection::Outbound },
    { "SliceModel", "nnrDspRateHz", MirrorDirection::Outbound },
    { "SliceModel", "nnrNetworkRateHz", MirrorDirection::Outbound },
    { "SliceModel", "nnrDelaySamples", MirrorDirection::Outbound },
    { "SliceModel", "nnrProfilingAvailable", MirrorDirection::Outbound },
    { "SliceModel", "nnrLatencyMs", MirrorDirection::Outbound },
    { "SliceModel", "nnrTestMode", MirrorDirection::Outbound },
    { "SliceModel", "nnrOutputMode", MirrorDirection::Outbound },
    { "SliceModel", "nnrModelSource", MirrorDirection::Outbound },
    { "SliceModel", "nnrStatus", MirrorDirection::Outbound },
    { "SliceModel", "nnrLastError", MirrorDirection::Outbound },
    // R-R3-40: the Core's runtime step-back. Outbound only; the operator
    // clears it with the nnr.tryAgain command or by choosing a model, and
    // StationServer leaves it out for peers below minor 11.
    { "SliceModel", "nnrLimit", MirrorDirection::Outbound },
    { "SliceModel", "nr1Taps", MirrorDirection::Bidirectional },
    { "SliceModel", "nr1Delay", MirrorDirection::Bidirectional },
    { "SliceModel", "nr1Gain", MirrorDirection::Bidirectional },
    { "SliceModel", "nr1Leakage", MirrorDirection::Bidirectional },
    { "SliceModel", "nr1Position", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2GainMethod", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2NpeMethod", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2TrainT1", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2TrainT2", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2AeFilter", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2Position", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2Post2Run", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2Post2Level", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2Post2Factor", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2Post2Rate", MirrorDirection::Bidirectional },
    { "SliceModel", "nr2Post2Taper", MirrorDirection::Bidirectional },
    { "SliceModel", "nr3Position", MirrorDirection::Bidirectional },
    { "SliceModel", "nr3UseDefaultGain", MirrorDirection::Bidirectional },
    { "SliceModel", "nr4Reduction", MirrorDirection::Bidirectional },
    { "SliceModel", "nr4Smoothing", MirrorDirection::Bidirectional },
    { "SliceModel", "nr4Whitening", MirrorDirection::Bidirectional },
    { "SliceModel", "nr4Rescale", MirrorDirection::Bidirectional },
    { "SliceModel", "nr4PostThresh", MirrorDirection::Bidirectional },
    { "SliceModel", "nr4Algo", MirrorDirection::Bidirectional },
    { "SliceModel", "dfnrAttenLimit", MirrorDirection::Bidirectional },
    { "SliceModel", "dfnrPostFilterBeta", MirrorDirection::Bidirectional },
    { "SliceModel", "bnrStrength", MirrorDirection::Bidirectional },
    { "SliceModel", "mnrStrength", MirrorDirection::Bidirectional },
    { "SliceModel", "mnrOversub", MirrorDirection::Bidirectional },
    { "SliceModel", "mnrFloor", MirrorDirection::Bidirectional },
    { "SliceModel", "mnrAlpha", MirrorDirection::Bidirectional },
    { "SliceModel", "mnrBias", MirrorDirection::Bidirectional },
    { "SliceModel", "mnrGsmooth", MirrorDirection::Bidirectional },
    { "SliceModel", "snbEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "anfEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "nb1Threshold", MirrorDirection::Bidirectional },
    { "SliceModel", "nb1TransitionMs", MirrorDirection::Bidirectional },
    { "SliceModel", "nb1LeadMs", MirrorDirection::Bidirectional },
    { "SliceModel", "nb1LagMs", MirrorDirection::Bidirectional },
    { "SliceModel", "nb2Mode", MirrorDirection::Bidirectional },
    { "SliceModel", "snbK1", MirrorDirection::Bidirectional },
    { "SliceModel", "snbK2", MirrorDirection::Bidirectional },
    { "SliceModel", "snbOutputBandwidthHz", MirrorDirection::Bidirectional },
    { "SliceModel", "apfEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "apfTuneHz", MirrorDirection::Bidirectional },
    { "SliceModel", "binauralEnabled", MirrorDirection::Bidirectional },
    { "SliceModel", "fmCtcssMode", MirrorDirection::Bidirectional },
    { "SliceModel", "fmCtcssValueHz", MirrorDirection::Bidirectional },
    { "SliceModel", "fmOffsetHz", MirrorDirection::Bidirectional },
    { "SliceModel", "fmTxMode", MirrorDirection::Bidirectional },
    { "SliceModel", "fmReverse", MirrorDirection::Bidirectional },
    { "SliceModel", "diglOffsetHz", MirrorDirection::Bidirectional },
    { "SliceModel", "diguOffsetHz", MirrorDirection::Bidirectional },
    { "SliceModel", "rttyMarkHz", MirrorDirection::Bidirectional },
    { "SliceModel", "rttyShiftHz", MirrorDirection::Bidirectional },
    // snrDb and lastRadeRxCallsign are daemon-produced RADE telemetry that
    // carries WRITE only so the decoder can set it. Outbound, and NOT
    // because they are read-only in spirit: writing either has a real
    // effect on station behaviour.
    //
    // Both setters restart the RADE idle-clear timer, not merely store:
    // SliceModel::setSnrDb on any non-NaN write (SliceModel.cpp:2308) and
    // setLastRadeRxCallsign on any non-empty write (:2332). A client
    // writing either more often than the idle window would suppress the
    // operator's idle clear indefinitely, pinning a stale callsign and SNR
    // on the local VFO flag.
    //
    // And nothing would overwrite a fabricated value. lastRadeRxCallsign's
    // only genuine writers are EOO decodes (RadioModel.cpp:1882, :5492),
    // which arrive seconds after a remote station FINISHES transmitting,
    // plus a clearing path (:5595). With no station on air there is no
    // next decode, so a fabricated callsign simply stays on the flag --
    // and that is the field an operator reads when logging a QSO.
    //
    // The R2 plan names seven writable properties that MUST be Outbound;
    // that is a floor, not a cap. Twenty further read-only properties are
    // Outbound here too.
    { "SliceModel", "snrDb", MirrorDirection::Outbound },
    { "SliceModel", "lastRadeRxCallsign", MirrorDirection::Outbound },
    // R-R3-13 / R-R3-49 (parity Task 15, meterReadingsVersion 1): the
    // Core's ADC and AGC readings, produced by its SliceMeterPump as the
    // S-meter readings above are; no WRITE, applied in a window through
    // SliceModel::applyMirroredValue's hook.
    { "SliceModel", "adcPeakDbfs", MirrorDirection::Outbound },
    { "SliceModel", "adcAverageDbfs", MirrorDirection::Outbound },
    { "SliceModel", "agcGainDb", MirrorDirection::Outbound },
    { "SliceModel", "agcPeakDb", MirrorDirection::Outbound },
    { "SliceModel", "agcAverageDb", MirrorDirection::Outbound },
    // R-R3-49 (parity Task 16, dspInfoVersion 1): the Core's channel's
    // minimum notch width (RadioModel::refreshSliceMinNotchWidths).
    { "SliceModel", "minNotchWidthHz", MirrorDirection::Outbound },
    // Diversity pattern for the phone (diversityPatternVersion 1): the
    // Diversity dialog's sensitivity pattern, read-only; only to a peer that
    // declared diversityPattern (StationServer::fitPeerOnlyProperties).
    { "SliceModel", "diversityPattern", MirrorDirection::Outbound },
    // RADE status (radeStatusVersion 1): the RADE decoder's sync and
    // frequency offset, read-only; only to a peer that declared radeStatus
    // (StationServer::fitPeerOnlyProperties).
    { "SliceModel", "radeSynced", MirrorDirection::Outbound },
    { "SliceModel", "radeFreqOffsetHz", MirrorDirection::Outbound },
    // RADE reason (radeReasonVersion 1): why the slice is in RADE with no
    // working decoder, read-only; only to a peer that declared radeReason
    // (StationServer::fitPeerOnlyProperties).
    { "SliceModel", "radeReason", MirrorDirection::Outbound },

    // ---- TransmitModel (87 entries) ----
    // iPhone app plan Task 35 (R-IOS-13): MOX and TUNE travel from the
    // Core only. A remote device keys with the transmit verbs (tx.key,
    // tx.tune), which pass the Core's gates; a property write never keys
    // (StationServer refuses it: "Use the transmit button.").
    { "TransmitModel", "mox", MirrorDirection::Outbound },
    { "TransmitModel", "tune", MirrorDirection::Outbound },
    { "TransmitModel", "power", MirrorDirection::Bidirectional },
    { "TransmitModel", "micGain", MirrorDirection::Bidirectional },
    { "TransmitModel", "pureSig", MirrorDirection::Bidirectional },
    { "TransmitModel", "filterLow", MirrorDirection::Bidirectional },
    { "TransmitModel", "filterHigh", MirrorDirection::Bidirectional },
    { "TransmitModel", "lineInGain", MirrorDirection::Bidirectional },
    { "TransmitModel", "userDigOut", MirrorDirection::Bidirectional },
    { "TransmitModel", "forceAttwhenPSAoff", MirrorDirection::Bidirectional },
    { "TransmitModel", "forceAttwhenPowerChangesWhenPSAon", MirrorDirection::Bidirectional },
    { "TransmitModel", "forceAttwhenPowerChangesWhenPSAonAndDecreased", MirrorDirection::Bidirectional },
    { "TransmitModel", "antiVoxTauMs", MirrorDirection::Bidirectional },
    { "TransmitModel", "antiVoxRun", MirrorDirection::Bidirectional },
    { "TransmitModel", "paSettingsBypass", MirrorDirection::Bidirectional },
    // R-R3-49 (parity Task 2, transmitSettingsVersion 2): the TX and
    // Phone/CW applets' settings. None keys the radio; a receive-only Core
    // takes them off the air and refuses them on it (StationServer).
    { "TransmitModel", "tunePower", MirrorDirection::Bidirectional },
    { "TransmitModel", "voxThresholdDb", MirrorDirection::Bidirectional },
    { "TransmitModel", "voxHangTimeMs", MirrorDirection::Bidirectional },
    { "TransmitModel", "monEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "monitorVolume", MirrorDirection::Bidirectional },
    { "TransmitModel", "txLevelerOn", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "cpdrOn", MirrorDirection::Bidirectional },
    { "TransmitModel", "cpdrLevelDb", MirrorDirection::Bidirectional },
    { "TransmitModel", "amCarrierLevel", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "micGainDb", MirrorDirection::Bidirectional },
    // The Core's report; it changes only through setTunePowerForTxBand.
    { "TransmitModel", "tunePowerForTxBand", MirrorDirection::Outbound },
    // R-R3-49 (parity Task 5, transmitSettingsVersion 5): Setup > Transmit >
    // Power's Tune group writes the drive source too (Outbound before).
    { "TransmitModel", "tuneDrivePowerSource", MirrorDirection::Bidirectional },
    // R-R3-49 (parity Task 3, transmitSettingsVersion 3): Setup > Audio >
    // TX Input's radio microphone groups. None keys the radio.
    { "TransmitModel", "micBoost", MirrorDirection::Bidirectional },
    { "TransmitModel", "micXlr", MirrorDirection::Bidirectional },
    { "TransmitModel", "micTipRing", MirrorDirection::Bidirectional },
    { "TransmitModel", "micBias", MirrorDirection::Bidirectional },
    { "TransmitModel", "micPttDisabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "lineIn", MirrorDirection::Bidirectional },
    { "TransmitModel", "lineInBoost", MirrorDirection::Bidirectional },
    // The Core's TX profiles; they change only through the txProfile verbs.
    { "TransmitModel", "activeTxProfile", MirrorDirection::Outbound },
    { "TransmitModel", "txProfilesJson", MirrorDirection::Outbound },
    // R-R3-49 (parity Task 4, transmitSettingsVersion 4): the TX EQ and
    // CFC dialogs, Setup > DSP > CFC and AGC/ALC's TX Leveler and TX ALC.
    // None keys the radio.
    { "TransmitModel", "txEqUseLegacy", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqPreamp", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqBandsJson", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqFreqsJson", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqNc", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqMp", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqCtfmode", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqWintype", MirrorDirection::Bidirectional },
    { "TransmitModel", "txEqParaEqData", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcCompressionJson", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcEqFreqJson", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcPostEqBandGainJson", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcPostEqEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcPostEqGainDb", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcPrecompDb", MirrorDirection::Bidirectional },
    { "TransmitModel", "cfcParaEqData", MirrorDirection::Bidirectional },
    { "TransmitModel", "phaseRotatorEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "phaseRotatorFreqHz", MirrorDirection::Bidirectional },
    { "TransmitModel", "phaseRotatorStages", MirrorDirection::Bidirectional },
    { "TransmitModel", "phaseReverseEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "cessbOn", MirrorDirection::Bidirectional },
    { "TransmitModel", "txLevelerMaxGain", MirrorDirection::Bidirectional },
    { "TransmitModel", "txLevelerDecay", MirrorDirection::Bidirectional },
    { "TransmitModel", "txAlcMaxGain", MirrorDirection::Bidirectional },
    { "TransmitModel", "txAlcDecay", MirrorDirection::Bidirectional },
    // R-R3-49 (parity Task 5, transmitSettingsVersion 5): Setup > Transmit >
    // Power's per-band power, DEXP/VOX and Test > Two-Tone IMD. None keys
    // the radio; the two-tone test itself stays in the keying set.
    // The per-band maps are the Core's own (PA on-air gate review): its RF
    // and Tune sliders write them through `power` and tunePowerForTxBand,
    // which pass the on-air gate. A whole-map write from a peer would skip
    // that gate, so the Core refuses it. No Setup page writes the maps.
    { "TransmitModel", "powerByBandJson", MirrorDirection::Outbound },
    { "TransmitModel", "tunePowerByBandJson", MirrorDirection::Outbound },
    { "TransmitModel", "dexpAttackTimeMs", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpDetectorTauMs", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpExpansionRatioDb", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpHighCutHz", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpHysteresisRatioDb", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpLookAheadEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpLookAheadMs", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpLowCutHz", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpReleaseTimeMs", MirrorDirection::Bidirectional },
    { "TransmitModel", "dexpSideChannelFilterEnabled", MirrorDirection::Bidirectional },
    { "TransmitModel", "antiVoxGainDb", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoToneFreq1", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoToneFreq2", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoToneLevel", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoTonePower", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoTonePulsed", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoToneInvert", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoToneFreq2Delay", MirrorDirection::Bidirectional },
    { "TransmitModel", "twoToneDrivePowerSource", MirrorDirection::Bidirectional },
    // Desktop remote transmit (R-IOS-13): a remote window arms the Core's
    // VOX. A write is the permitted sessions' only (the station transmit
    // gate), and the Core turns VOX off at every change of holder.
    { "TransmitModel", "voxEnabled", MirrorDirection::Bidirectional },
    // iPhone app plan Task 40 (transmitSettingsVersion 10): the mic mute.
    { "TransmitModel", "micMuted", MirrorDirection::Bidirectional },
    // R-IOS-13 / R-R3-49 (txEqCurveVersion 1): the TX EQ curve the Core
    // derives from txEqParaEqData, read-only; only to a peer that declared
    // txEqCurve (StationServer::fitTxEqCurveToPeer).
    { "TransmitModel", "txEqCurve", MirrorDirection::Outbound },
    // transmitSettingsVersion 15: the CFC dialog's band editor the Core
    // derives from cfcParaEqData (or the ten-band values), read-only; only
    // to a peer that declared cfcProfile (StationServer::fitPeerOnlyProperties).
    // An app changes it with cfc.setProfile.
    { "TransmitModel", "cfcProfile", MirrorDirection::Outbound },

    // ---- TunerModel (21 entries) ----
    { "TunerModel", "relayC1", MirrorDirection::Outbound },
    { "TunerModel", "relayL", MirrorDirection::Outbound },
    { "TunerModel", "relayC2", MirrorDirection::Outbound },
    { "TunerModel", "isOperate", MirrorDirection::Outbound },
    { "TunerModel", "isBypass", MirrorDirection::Outbound },
    { "TunerModel", "isTuning", MirrorDirection::Outbound },
    { "TunerModel", "antennaA", MirrorDirection::Outbound },
    { "TunerModel", "hasAntennaSwitch", MirrorDirection::Outbound },
    { "TunerModel", "isPresent", MirrorDirection::Outbound },
    { "TunerModel", "hasDirectConnection", MirrorDirection::Outbound },
    { "TunerModel", "tgxlIp", MirrorDirection::Outbound },
    { "TunerModel", "fwdPower", MirrorDirection::Outbound },
    { "TunerModel", "swr", MirrorDirection::Outbound },
    { "TunerModel", "configuredHost", MirrorDirection::Outbound },
    { "TunerModel", "configuredPort", MirrorDirection::Outbound },
    { "TunerModel", "connectionPhase", MirrorDirection::Outbound },
    { "TunerModel", "connectionError", MirrorDirection::Outbound },
    { "TunerModel", "deviceModel", MirrorDirection::Outbound },
    { "TunerModel", "deviceSerial", MirrorDirection::Outbound },
    { "TunerModel", "deviceVersion", MirrorDirection::Outbound },
    { "TunerModel", "deviceNickname", MirrorDirection::Outbound },

    { "PureSignalSessionFacade", "available", MirrorDirection::Outbound },
    { "PureSignalSessionFacade", "canActuate", MirrorDirection::Outbound },
    { "PureSignalSessionFacade", "twoToneOn", MirrorDirection::Outbound },
    { "PureSignalSessionFacade", "statusJson", MirrorDirection::Outbound },
    { "PureSignalSessionFacade", "lastActionError", MirrorDirection::Outbound },
    { "PureSignalSessionFacade", "displayGeneration", MirrorDirection::Outbound },

    { "DspAssetService", "nnrStandardAsset", MirrorDirection::Outbound },
    { "DspAssetService", "nnrPremiumAsset", MirrorDirection::Outbound },
    { "DspAssetService", "nnrModelSelectionPending", MirrorDirection::Outbound },
    { "DspAssetService", "nnrModelStatus", MirrorDirection::Outbound },
    { "DspAssetService", "selectionRevision", MirrorDirection::Outbound },
    // R-R3-21 (dspAssetVersion 2): the Core-wide NR3 model. Outbound only;
    // a window changes it with the dspAssets.selectNr3Model command.
    { "DspAssetService", "nr3ModelAsset", MirrorDirection::Outbound },
    { "DspAssetService", "nr3ModelStatus", MirrorDirection::Outbound },
    // Fix wave I3: false when the Core has no usable NR3 model, so a window
    // refuses turning NR3 on with nr3ModelStatus. An older Core never sends
    // it and the window keeps its default, true.
    { "DspAssetService", "nr3Runnable", MirrorDirection::Outbound },
    // R-R3-49, Sub-epic C-1 (dspAssetVersion 3): the same pair for DFNR. A
    // window shows DFNR disabled with dfnrModelStatus while dfnrRunnable is
    // false. An older Core never sends them; the window then shows DFNR
    // disabled with "This Core does not say which noise reduction it can
    // run." (RadioModel::nrCannotRunReason, parity Task 16's rule).
    { "DspAssetService", "dfnrModelStatus", MirrorDirection::Outbound },
    { "DspAssetService", "dfnrRunnable", MirrorDirection::Outbound },
    // R-R3-49, Sub-epic C-1 (dspAssetVersion 4): the same pair for MNR,
    // which runs only on a Mac. A window shows MNR disabled with mnrStatus
    // while mnrRunnable is false; an older Core (below 4) never sends them,
    // and the window shows MNR disabled with the same "does not say" reason.
    { "DspAssetService", "mnrStatus", MirrorDirection::Outbound },
    { "DspAssetService", "mnrRunnable", MirrorDirection::Outbound },

    // R-R3-21 / R-R3-09 (notchControlVersion 1): the Core's notch list. The
    // list and its revision are Outbound; a window changes the list only
    // with the notch.* commands. The master enable and auto-increase are
    // plain two-way switches.
    { "NotchModel", "listJson", MirrorDirection::Outbound },
    { "NotchModel", "revision", MirrorDirection::Outbound },
    { "NotchModel", "globalEnabled", MirrorDirection::Bidirectional },
    { "NotchModel", "autoIncrease", MirrorDirection::Bidirectional },

    // R-R3-46 / R-R3-11 (radioHardwareVersion 1): the Core's step
    // attenuator and preamp. The operator settings are two-way; the Core
    // applies each through its own controller and answers with the value it
    // kept. The range, auto-attenuate's own state, the overload readings and
    // ADC sharing are the Core's to report.
    { "StepAttenuatorFacade", "enabled", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "attenuationDb", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "preampMode", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "rx1Preamp", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "autoAttEnabled", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "autoAttMode", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "autoAttUndo", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "autoAttUndoDelayMs", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "autoAttHoldMs", MirrorDirection::Bidirectional },
    // R-R3-49 (parity Task 5, transmitSettingsVersion 5): Setup > Transmit >
    // Power's ATT on TX, its value and Force ATT. Transmit settings: a
    // receive-only Core takes them off the air only (StationServer).
    { "StepAttenuatorFacade", "attOnTxEnabled", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "attOnTxValue", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "forceAttWhenPsOff", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "minDb", MirrorDirection::Outbound },
    { "StepAttenuatorFacade", "maxDb", MirrorDirection::Outbound },
    { "StepAttenuatorFacade", "autoAttApplied", MirrorDirection::Outbound },
    { "StepAttenuatorFacade", "overloadAdc0", MirrorDirection::Outbound },
    { "StepAttenuatorFacade", "overloadAdc1", MirrorDirection::Outbound },
    { "StepAttenuatorFacade", "adcLinked", MirrorDirection::Outbound },
    // R-R3-46 / R-R3-11 (adcAttenuatorVersion 1): the other ADC's own
    // attenuator (two-way, applied through the Core's controller) and the
    // slices on that ADC (the Core's to report). Sent only to a peer whose
    // hello declared adcAttenuators 1 (StationServer).
    { "StepAttenuatorFacade", "rx2AttenuationDb", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "rx2SliceMask", MirrorDirection::Outbound },
    // RX2's own enable and auto-attenuate settings, two-way, same gate.
    { "StepAttenuatorFacade", "rx2StepAttEnabled", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "rx2AutoAttEnabled", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "rx2AutoAttUndo", MirrorDirection::Bidirectional },
    { "StepAttenuatorFacade", "rx2AutoAttUndoDelayMs", MirrorDirection::Bidirectional },
    // Level Cal (radioHardwareVersion 12): RX2's own preamp mode, two-way,
    // same gate.
    { "StepAttenuatorFacade", "rx2PreampMode", MirrorDirection::Bidirectional },

    // R-R3-46 (radioHardwareVersion 2): the Core's Alex antenna settings.
    // The receive settings are two-way; the Core applies each through its
    // own AlexController and answers with the value it kept. The transmit
    // antennas and relays are the Core's to report until remote transmit.
    { "AlexAntennaFacade", "rxAntennas", MirrorDirection::Bidirectional },
    { "AlexAntennaFacade", "rxOnlyAntennas", MirrorDirection::Bidirectional },
    { "AlexAntennaFacade", "useTxAntennaForRx", MirrorDirection::Bidirectional },
    // Parity Task 12 (radioHardwareVersion 6): the transmit antennas and
    // relays are two-way; the Core applies each through its own
    // AlexController, as the local Antenna Control tab does.
    { "AlexAntennaFacade", "txAntennas", MirrorDirection::Bidirectional },
    { "AlexAntennaFacade", "blockTxAnt2", MirrorDirection::Bidirectional },
    { "AlexAntennaFacade", "blockTxAnt3", MirrorDirection::Bidirectional },
    // Group B fix wave (radioHardwareVersion 5): RX bypass on TX, the VFO
    // flag's BYPS, is two-way; the Core applies it through its own
    // AlexController.
    { "AlexAntennaFacade", "rxOutOnTx", MirrorDirection::Bidirectional },
    { "AlexAntennaFacade", "ext1OutOnTx", MirrorDirection::Bidirectional },
    { "AlexAntennaFacade", "ext2OutOnTx", MirrorDirection::Bidirectional },
    { "AlexAntennaFacade", "rxOutOverride", MirrorDirection::Bidirectional },

    // R-R3-46 (radioHardwareVersion 3): the Core's HL2 I/O board, as the
    // Core reads it; a window asks for a probe with a command.
    { "IoBoardHl2Facade", "detected", MirrorDirection::Outbound },
    { "IoBoardHl2Facade", "hardwareVersion", MirrorDirection::Outbound },
    { "IoBoardHl2Facade", "registers", MirrorDirection::Outbound },
    // Remote-window parity Task 14 (R-R3-46, radioHardwareVersion 7): the
    // board's output pins as the Core last read them back.
    { "IoBoardHl2Facade", "outputs", MirrorDirection::Outbound },

    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 1): the Core's Power
    // Genius XL status. Every property is the Core's to report; a window
    // connects, configures and operates the amp only through commands.
    { "AmplifierModel", "connectionPhase", MirrorDirection::Outbound },
    { "AmplifierModel", "configuredHost", MirrorDirection::Outbound },
    { "AmplifierModel", "configuredPort", MirrorDirection::Outbound },
    { "AmplifierModel", "connectionError", MirrorDirection::Outbound },
    { "AmplifierModel", "deviceModel", MirrorDirection::Outbound },
    { "AmplifierModel", "deviceSerial", MirrorDirection::Outbound },
    { "AmplifierModel", "deviceVersion", MirrorDirection::Outbound },
    { "AmplifierModel", "deviceNickname", MirrorDirection::Outbound },
    { "AmplifierModel", "present", MirrorDirection::Outbound },
    { "AmplifierModel", "state", MirrorDirection::Outbound },
    { "AmplifierModel", "deviceState", MirrorDirection::Outbound },
    { "AmplifierModel", "operate", MirrorDirection::Outbound },
    { "AmplifierModel", "transmitting", MirrorDirection::Outbound },
    { "AmplifierModel", "forwardPowerW", MirrorDirection::Outbound },
    { "AmplifierModel", "swr", MirrorDirection::Outbound },
    { "AmplifierModel", "temperatureC", MirrorDirection::Outbound },
    { "AmplifierModel", "mainsVoltageV", MirrorDirection::Outbound },
    { "AmplifierModel", "drainCurrentA", MirrorDirection::Outbound },
    { "AmplifierModel", "efficiencyText", MirrorDirection::Outbound },
    // R-R3-48 (remotePgxlControlVersion 2): band follow, read-only.
    { "AmplifierModel", "bandFollow", MirrorDirection::Outbound },

    // R-R3-47 / R-R3-22 (remoteRfKitControlVersion 1): the Core's RF-Kit
    // RF2K-S status, read-only for the same reason.
    { "RfKitModel", "connectionPhase", MirrorDirection::Outbound },
    { "RfKitModel", "configuredHost", MirrorDirection::Outbound },
    { "RfKitModel", "configuredPort", MirrorDirection::Outbound },
    { "RfKitModel", "connectionError", MirrorDirection::Outbound },
    { "RfKitModel", "deviceModel", MirrorDirection::Outbound },
    { "RfKitModel", "deviceSerial", MirrorDirection::Outbound },
    { "RfKitModel", "deviceVersion", MirrorDirection::Outbound },
    { "RfKitModel", "deviceNickname", MirrorDirection::Outbound },
    { "RfKitModel", "present", MirrorDirection::Outbound },
    { "RfKitModel", "operate", MirrorDirection::Outbound },
    { "RfKitModel", "forwardPowerW", MirrorDirection::Outbound },
    { "RfKitModel", "reflectedPowerW", MirrorDirection::Outbound },
    { "RfKitModel", "swr", MirrorDirection::Outbound },
    { "RfKitModel", "temperatureC", MirrorDirection::Outbound },
    { "RfKitModel", "voltageV", MirrorDirection::Outbound },
    { "RfKitModel", "currentA", MirrorDirection::Outbound },
    // R-R3-47 / R-R3-48 (remoteRfKitControlVersion 2): the interface,
    // antenna and tuner rows and band follow, read-only.
    { "RfKitModel", "operationalInterface", MirrorDirection::Outbound },
    { "RfKitModel", "antennaPresentMask", MirrorDirection::Outbound },
    { "RfKitModel", "antennaDisabledMask", MirrorDirection::Outbound },
    { "RfKitModel", "activeAntennaNumber", MirrorDirection::Outbound },
    { "RfKitModel", "activeAntennaExternal", MirrorDirection::Outbound },
    { "RfKitModel", "tunerMode", MirrorDirection::Outbound },
    { "RfKitModel", "tunerSetup", MirrorDirection::Outbound },
    { "RfKitModel", "tunerInductanceNh", MirrorDirection::Outbound },
    { "RfKitModel", "tunerCapacitancePf", MirrorDirection::Outbound },
    { "RfKitModel", "tunerFrequencyKhz", MirrorDirection::Outbound },
    { "RfKitModel", "tunerSegmentKhz", MirrorDirection::Outbound },
    { "RfKitModel", "bandFollow", MirrorDirection::Outbound },
    { "RfKitModel", "bandFollowAddress", MirrorDirection::Outbound },
    { "RfKitModel", "bandFollowPort", MirrorDirection::Outbound },

    // R-R3-48 (stationTciVersion 1): the Core's station TCI server,
    // read-only. Its switch changes only through setStationTci.
    { "StationTciModel", "enabled", MirrorDirection::Outbound },
    { "StationTciModel", "port", MirrorDirection::Outbound },
    { "StationTciModel", "listening", MirrorDirection::Outbound },
    { "StationTciModel", "stationAddress", MirrorDirection::Outbound },
    { "StationTciModel", "error", MirrorDirection::Outbound },
    // Parity Task 23 (stationTciVersion 2): the server's options, changed
    // only through setStationTciOptions.
    { "StationTciModel", "emulateExpertSdr3", MirrorDirection::Outbound },
    { "StationTciModel", "emulateSunSdr2Pro", MirrorDirection::Outbound },
    { "StationTciModel", "cwluBecomesCw", MirrorDirection::Outbound },
    { "StationTciModel", "sendInitialState", MirrorDirection::Outbound },
    // JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the rest of
    // the server's settings, changed only through setStationTciSettings and
    // sent only to a peer that declared stationTciSettings 1
    // (StationServer::fitStationTciSettingsToPeer).
    { "StationTciModel", "rateLimitMs", MirrorDirection::Outbound },
    { "StationTciModel", "cwBecomesCwuAbove10mhz", MirrorDirection::Outbound },
    { "StationTciModel", "iqSwap", MirrorDirection::Outbound },
    { "StationTciModel", "alwaysStreamIq", MirrorDirection::Outbound },
    { "StationTciModel", "audioBlockSamples", MirrorDirection::Outbound },
    { "StationTciModel", "txChannel", MirrorDirection::Outbound },
    { "StationTciModel", "rxSensorIntervalMs", MirrorDirection::Outbound },
    { "StationTciModel", "txSensorIntervalMs", MirrorDirection::Outbound },
    { "StationTciModel", "forgetRx2VfoBOnDisconnect", MirrorDirection::Outbound },
    { "StationTciModel", "useRx1VfoaForRx2Vfoa", MirrorDirection::Outbound },
    { "StationTciModel", "copyRx2VfobToVfoa", MirrorDirection::Outbound },

    // iPhone app plan Task 25 (vaxVersion 1): the station computer's VAX.
    // The slices, device names and transmit slice are the Core's; the
    // levels and mutes a device may change (StationServer checks them).
    { "StationVax", "ch1Slices", MirrorDirection::Outbound },
    { "StationVax", "ch2Slices", MirrorDirection::Outbound },
    { "StationVax", "ch3Slices", MirrorDirection::Outbound },
    { "StationVax", "ch4Slices", MirrorDirection::Outbound },
    { "StationVax", "ch1RxGain", MirrorDirection::Bidirectional },
    { "StationVax", "ch2RxGain", MirrorDirection::Bidirectional },
    { "StationVax", "ch3RxGain", MirrorDirection::Bidirectional },
    { "StationVax", "ch4RxGain", MirrorDirection::Bidirectional },
    { "StationVax", "ch1Muted", MirrorDirection::Bidirectional },
    { "StationVax", "ch2Muted", MirrorDirection::Bidirectional },
    { "StationVax", "ch3Muted", MirrorDirection::Bidirectional },
    { "StationVax", "ch4Muted", MirrorDirection::Bidirectional },
    { "StationVax", "ch1Device", MirrorDirection::Outbound },
    { "StationVax", "ch2Device", MirrorDirection::Outbound },
    { "StationVax", "ch3Device", MirrorDirection::Outbound },
    { "StationVax", "ch4Device", MirrorDirection::Outbound },
    { "StationVax", "txSlice", MirrorDirection::Outbound },
    { "StationVax", "txGain", MirrorDirection::Bidirectional },

    // iPhone app Task 43: the station authors every Setup description.
    { "SetupDescription", "general", MirrorDirection::Outbound },
    { "SetupDescription", "hardware", MirrorDirection::Outbound },
    { "SetupDescription", "audio", MirrorDirection::Outbound },
    { "SetupDescription", "dsp", MirrorDirection::Outbound },
    { "SetupDescription", "display", MirrorDirection::Outbound },
    { "SetupDescription", "transmit", MirrorDirection::Outbound },
    { "SetupDescription", "appearance", MirrorDirection::Outbound },
    { "SetupDescription", "catNetwork", MirrorDirection::Outbound },
    { "SetupDescription", "test", MirrorDirection::Outbound },
    { "SetupDescription", "diagnostics", MirrorDirection::Outbound },
    { "SetupDescription", "revision", MirrorDirection::Outbound },
    { "SetupDescription", "pa", MirrorDirection::Outbound },

    // R-R3-47 / R-R3-22 (accessoryDataVersion 1): the Core's accessory
    // records and settings, read-only. A window changes the interlock
    // policy, the output limit and a fault history only through
    // setTxInterlockPolicy, setPgxlPowerCap and clearAccessoryFaults.
    { "AccessoryDataModel", "faultRevision", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlFaults", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlFaults", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitFaults", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlConnectedSinceMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlLastRttMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlKeepaliveMissed", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlReconnectCount", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlFramesIn", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlFramesOut", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlBytesIn", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlBytesOut", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlLastFrameMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "pgxlFaultsSession", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlConnectedSinceMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlLastRttMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlKeepaliveMissed", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlReconnectCount", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlFramesIn", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlFramesOut", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlBytesIn", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlBytesOut", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlLastFrameMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlFaultsSession", MirrorDirection::Outbound },
    { "AccessoryDataModel", "interlockMode", MirrorDirection::Outbound },
    { "AccessoryDataModel", "interlockGraceMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "interlockSwrGateEnabled", MirrorDirection::Outbound },
    { "AccessoryDataModel", "interlockSwrGateMax", MirrorDirection::Outbound },
    { "AccessoryDataModel", "powerCapEnabled", MirrorDirection::Outbound },
    { "AccessoryDataModel", "powerCapW", MirrorDirection::Outbound },
    { "AccessoryDataModel", "powerCapExceeded", MirrorDirection::Outbound },
    { "AccessoryDataModel", "powerCapAlertText", MirrorDirection::Outbound },
    { "AccessoryDataModel", "powerCapAlertCount", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tuneMemory", MirrorDirection::Outbound },
    { "AccessoryDataModel", "autoTuneMemoryRecall", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlAntenna1Label", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlAntenna2Label", MirrorDirection::Outbound },
    { "AccessoryDataModel", "tgxlAntenna3Label", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitAntenna1Label", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitAntenna2Label", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitAntenna3Label", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitAntenna4Label", MirrorDirection::Outbound },
    // R-R3-49 (parity Task 10, accessoryDataVersion 2): the RF-Kit's
    // connection counts, read-only.
    { "AccessoryDataModel", "rfkitConnectedSinceMs", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitPollsOk", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitPollsFailed", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitReconnectCount", MirrorDirection::Outbound },
    { "AccessoryDataModel", "rfkitLastPollMs", MirrorDirection::Outbound },
    // Group B fix wave (M7, accessoryDataVersion 3): the RF-Kit's average
    // response time, read-only.
    { "AccessoryDataModel", "rfkitRttAvgMs", MirrorDirection::Outbound },

    // R-R3-47 / R-R3-22 (remotePgxlControlVersion 3, remoteTgxlControlVersion
    // 1): the amp's and tuner's own settings as the Core last heard them,
    // read-only. A window changes them only through the setPgxl* / setTgxl*,
    // save and read commands.
    { "AccessorySettingsModel", "pgxlNickname", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlBiasMode", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlFanMode", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlLedIntensity", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlNetworkKnown", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlDhcp", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlAddress", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlNetmask", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlGateway", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlAnswer", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlAnswerAccepted", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "pgxlAnswerCount", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlNickname", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlNetworkKnown", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlDhcp", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlAddress", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlNetmask", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlGateway", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlAnswer", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlAnswerAccepted", MirrorDirection::Outbound },
    { "AccessorySettingsModel", "tgxlAnswerCount", MirrorDirection::Outbound },

    // iPhone app Task 13 (R-IOS-08, deviceAdminVersion 1): the Core's paired
    // devices, label, claim, token and key backup, read-only. A device
    // changes them only through devices.revoke, station.rename,
    // station.acknowledgeKeyBackup and station.retireToken.
    { "StationDevicesFacade", "listJson", MirrorDirection::Outbound },
    { "StationDevicesFacade", "revision", MirrorDirection::Outbound },
    { "StationDevicesFacade", "stationLabel", MirrorDirection::Outbound },
    { "StationDevicesFacade", "claimed", MirrorDirection::Outbound },
    { "StationDevicesFacade", "tokenActive", MirrorDirection::Outbound },
    { "StationDevicesFacade", "keyBackupAcknowledged", MirrorDirection::Outbound },
    { "StationDevicesFacade", "keyPath", MirrorDirection::Outbound },
    // iPhone app Task 14 (R-IOS-08, pairingVersion 1): the pairing window,
    // changed only through pairing.open and pairing.close. The code reaches
    // only a connection signed in with a paired device's key
    // (StationServer::sendToSession).
    { "StationDevicesFacade", "pairingWindowOpen", MirrorDirection::Outbound },
    { "StationDevicesFacade", "pairingCode", MirrorDirection::Outbound },
    // The phone's direct addresses (coreAddressesVersion 1): where a device
    // can dial this Core, Core to device only. Only to a connection signed
    // in with a paired device's key that declared coreAddresses
    // (StationServer::fitPeerOnlyProperties).
    { "StationDevicesFacade", "coreAddresses", MirrorDirection::Outbound },

    // iPhone app Task 19 (R-IOS-06, stationCatalogVersion 1): the values the
    // Core owns and an app draws its controls from, read-only. They change
    // only with the Core's presets, band plans and radio.
    { "StationCatalog", "json", MirrorDirection::Outbound },
    { "StationCatalog", "revision", MirrorDirection::Outbound },
    // R-R3-49 / R-IOS-18 (paProfileVersion 1): the Core's PA Gain profiles,
    // read-only; the paProfile verbs change them.
    { "PaProfilesFacade", "json", MirrorDirection::Outbound },
    { "PaProfilesFacade", "revision", MirrorDirection::Outbound },

    // R-IOS-25 / R-R3-49 (parity Task 19, recordStreamVersion 1): the Core's
    // spot sources, read-only. They change only through spots.connect,
    // spots.disconnect and the sources themselves.
    { "SpotSourceHost", "dxClusterState", MirrorDirection::Outbound },
    { "SpotSourceHost", "dxClusterText", MirrorDirection::Outbound },
    { "SpotSourceHost", "rbnState", MirrorDirection::Outbound },
    { "SpotSourceHost", "rbnText", MirrorDirection::Outbound },
    { "SpotSourceHost", "potaState", MirrorDirection::Outbound },
    { "SpotSourceHost", "potaText", MirrorDirection::Outbound },
    // R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20,
    // stationFreedvVersion 1): FreeDV Reporter, the same shape, and "Hide
    // my station" (changed only through freedv.setHidden).
    { "SpotSourceHost", "freedvReporterState", MirrorDirection::Outbound },
    { "SpotSourceHost", "freedvReporterText", MirrorDirection::Outbound },
    { "SpotSourceHost", "freedvReporterHidden", MirrorDirection::Outbound },
    { "SpotSourceHost", "pskReporterState", MirrorDirection::Outbound },
    { "SpotSourceHost", "pskReporterText", MirrorDirection::Outbound },

    // iPhone app Task 71 (R-IOS-02, sessionHolderVersion 1): who is on the
    // Core, read-only. It changes only as devices come, go, go away and act.
    { "ConnectedDevicesFacade", "listJson", MirrorDirection::Outbound },
    { "ConnectedDevicesFacade", "revision", MirrorDirection::Outbound },
    { "ConnectedDevicesFacade", "deviceLimit", MirrorDirection::Outbound },

    // iPhone app Task 73 (R-IOS-02, sessionHolderVersion 1): another device's
    // slice, read-only (ruling 5.4). Only its owner changes the slice; a
    // write to a marker is refused with the owner named (ruling 5.9).
    { "SliceMarker", "sliceId", MirrorDirection::ConstantSnapshot },
    { "SliceMarker", "ownerDeviceId", MirrorDirection::Outbound },
    { "SliceMarker", "ownerName", MirrorDirection::Outbound },
    { "SliceMarker", "ownerShortName", MirrorDirection::Outbound },
    { "SliceMarker", "ownerKind", MirrorDirection::Outbound },
    { "SliceMarker", "ownerAway", MirrorDirection::Outbound },
    { "SliceMarker", "frequency", MirrorDirection::Outbound },
    { "SliceMarker", "dspMode", MirrorDirection::Outbound },
    { "SliceMarker", "filterLow", MirrorDirection::Outbound },
    { "SliceMarker", "filterHigh", MirrorDirection::Outbound },
    { "SliceMarker", "txSlice", MirrorDirection::Outbound },
    { "SliceMarker", "band", MirrorDirection::Outbound },
    { "SliceMarker", "streamIndex", MirrorDirection::Outbound },
    { "SliceMarker", "psPaused", MirrorDirection::Outbound },

    // Slice control plan Task 4 (sliceAccessVersion 1): who controls and
    // who listens to each slice, read-only. Control changes only through
    // slice.takeControl and slice.release, membership through slice.listen
    // and slice.stopListening.
    { "SliceAccess", "sliceId", MirrorDirection::ConstantSnapshot },
    { "SliceAccess", "incarnation", MirrorDirection::ConstantSnapshot },
    { "SliceAccess", "controllerDeviceId", MirrorDirection::Outbound },
    { "SliceAccess", "controlRevision", MirrorDirection::Outbound },
    { "SliceAccess", "listenerDeviceIds", MirrorDirection::Outbound },
    { "SliceAccess", "activeRxDeviceIds", MirrorDirection::Outbound },
    { "SliceAccess", "txSelected", MirrorDirection::Outbound },
    { "SliceAccess", "onAir", MirrorDirection::Outbound },

    // iPhone app plan Task 39 (D14, R-IOS-13, txStateVersion 1): the Core's
    // transmitter, read-only. It changes only as the radio keys, unkeys and
    // reads its meters, and as the Core stops a transmission.
    { "TransmitState", "keyed", MirrorDirection::Outbound },
    { "TransmitState", "tuning", MirrorDirection::Outbound },
    { "TransmitState", "twoTone", MirrorDirection::Outbound },
    { "TransmitState", "txSliceId", MirrorDirection::Outbound },
    { "TransmitState", "keyedByName", MirrorDirection::Outbound },
    { "TransmitState", "keyedByKind", MirrorDirection::Outbound },
    { "TransmitState", "keyedTrigger", MirrorDirection::Outbound },
    { "TransmitState", "keyedSinceMs", MirrorDirection::Outbound },
    { "TransmitState", "timeOutRemainingSeconds", MirrorDirection::Outbound },
    { "TransmitState", "forwardPowerWatts", MirrorDirection::Outbound },
    { "TransmitState", "reflectedPowerWatts", MirrorDirection::Outbound },
    { "TransmitState", "swr", MirrorDirection::Outbound },
    { "TransmitState", "alcDb", MirrorDirection::Outbound },
    { "TransmitState", "micLevelDb", MirrorDirection::Outbound },
    { "TransmitState", "txEnding", MirrorDirection::Outbound },
    { "TransmitState", "stopReason", MirrorDirection::Outbound },
    { "TransmitState", "stopText", MirrorDirection::Outbound },
    { "TransmitState", "stopSerial", MirrorDirection::Outbound },
    // Fix wave I4 (txStateVersion 2): who holds transmit, and how long the
    // key has been on. The Core's report.
    { "TransmitState", "holderDeviceId", MirrorDirection::Outbound },
    { "TransmitState", "holderName", MirrorDirection::Outbound },
    { "TransmitState", "holderShortName", MirrorDirection::Outbound },
    { "TransmitState", "holderKind", MirrorDirection::Outbound },
    { "TransmitState", "holderSource", MirrorDirection::Outbound },
    { "TransmitState", "holderForSeconds", MirrorDirection::Outbound },
    { "TransmitState", "holderEpoch", MirrorDirection::Outbound },
    { "TransmitState", "holderAway", MirrorDirection::Outbound },
    { "TransmitState", "holderTransferring", MirrorDirection::Outbound },
    { "TransmitState", "keyedForSeconds", MirrorDirection::Outbound },
    // Fix wave 2: the keying epoch of the key the last stop ended.
    { "TransmitState", "stopEpoch", MirrorDirection::Outbound },
    // Parity Task 28 (txDisplayVersion 1): the Core's high-SWR state, for a
    // remote window's high-SWR border on its transmitting pan.
    { "TransmitState", "highSwr", MirrorDirection::Outbound },
    { "TransmitState", "swrWindBackLatched", MirrorDirection::Outbound },
    // Parity Task 33 (txReadingsVersion 1): the radio's raw forward and
    // reflected power readings, for a remote window's PA Values page.
    { "TransmitState", "forwardAdcRaw", MirrorDirection::Outbound },
    { "TransmitState", "reflectedAdcRaw", MirrorDirection::Outbound },
    // Task 33 follow-up (txReadingsVersion 1): the COMP reading.
    { "TransmitState", "compressionDb", MirrorDirection::Outbound },
    { "TransmitState", "forwardRawPowerWatts", MirrorDirection::Outbound },
    { "TransmitState", "forwardAdcVolts", MirrorDirection::Outbound },
    { "TransmitState", "reflectedAdcVolts", MirrorDirection::Outbound },
    // A9 (txReadingsVersion 3): the container meters' stage readings.
    { "TransmitState", "eqDb", MirrorDirection::Outbound },
    { "TransmitState", "levelerDb", MirrorDirection::Outbound },
    { "TransmitState", "levelerGainDb", MirrorDirection::Outbound },
    { "TransmitState", "cfcDb", MirrorDirection::Outbound },
    { "TransmitState", "cfcGainDb", MirrorDirection::Outbound },
    { "TransmitState", "alcGainDb", MirrorDirection::Outbound },
    { "TransmitState", "alcGroupDb", MirrorDirection::Outbound },

    // Normal PS3 configuration is distinct from operational arming/actions.
    { "PureSignalSettings", "autoCalEnabled", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "runCalibrationProcessing", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "autoAttenuate", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "quickAttenuate", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "moxDelaySeconds", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "loopDelaySeconds", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "requestedTxDelayNs", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "hardwarePeakOverrideEnabled", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "hardwarePeakOverride", MirrorDirection::Bidirectional },
    { "PureSignalSettings", "lastLoadError", MirrorDirection::Outbound },

    // ---- RadioModel (35 entries) ----
    { "RadioModel", "settingsSaveError", MirrorDirection::Outbound },
    { "RadioModel", "receiveLayoutRestoreState", MirrorDirection::Outbound },
    { "RadioModel", "receiveLayoutRestoreMessage", MirrorDirection::Outbound },
    { "RadioModel", "name", MirrorDirection::Outbound },
    { "RadioModel", "model", MirrorDirection::Outbound },
    { "RadioModel", "version", MirrorDirection::Outbound },
    { "RadioModel", "connected", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter0Mode", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter0Effective", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter0Band", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter0Reason", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter1Mode", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter1Effective", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter1Band", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter1Reason", MirrorDirection::Outbound },
    // Plan Task 14 fix wave (R-R3-49): the band-output byte the Core's
    // connection composed, its band and the keyed state. Read-only: the
    // windows' OC and HL2 I/O displays show these.
    { "RadioModel", "bandOutputsByte", MirrorDirection::Outbound },
    { "RadioModel", "bandOutputsBand", MirrorDirection::Outbound },
    { "RadioModel", "bandOutputsKeyed", MirrorDirection::Outbound },
    // radioHardwareVersion 10: the Alex-1 low-pass in use, read-only; only
    // to a peer that declared alexLpf (StationServer::fitPeerOnlyProperties).
    { "RadioModel", "alexLpfBits", MirrorDirection::Outbound },
    // R-R3-47: the Core's RF-Kit switch. A window changes it with the
    // setRfKitEnabled command; a raw write is refused.
    { "RadioModel", "rfKitEnabled", MirrorDirection::Outbound },
    // The 4O3A listener and its bind error exist only at Core. A remote
    // client renders these observational values and must never write one
    // back into a listener, socket, or per-MAC settings scope.
    { "RadioModel", "fourO3AEnabled", MirrorDirection::Outbound },
    { "RadioModel", "fourO3AListening", MirrorDirection::Outbound },
    { "RadioModel", "fourO3AListenerError", MirrorDirection::Outbound },
    // R-R3-49: the Core's real transmit state (MoxController), so a window
    // can grey what waits while the radio is on the air. Never writable.
    { "RadioModel", "transmitting", MirrorDirection::Outbound },
    // R-R3-49 (parity Task 6): the Core's TX inhibit, Core to window only.
    { "RadioModel", "txInhibited", MirrorDirection::Outbound },
    // R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16, dspInfoVersion 1): the
    // Core's last DSP Options apply time, Core to window only. Which noise
    // reduction the Core runs is DspAssetService's (dfnrRunnable,
    // mnrRunnable), the one source.
    { "RadioModel", "dspOptionsLastApplyMs", MirrorDirection::Outbound },
    // Fix wave after parity Tasks 19 and 21 (M2, R-IOS-18): why the Core
    // waits for a radio, Core to window only (nereusd's StationRadios).
    { "RadioModel", "stationRadioWaiting", MirrorDirection::Outbound },
    // Remote-window parity Task 22 (R-R3-49, supportBundleVersion 1): the
    // Core's enabled logging categories, Core to window only; changed with
    // support.setLogCategories.
    { "RadioModel", "logCategories", MirrorDirection::Outbound },
    // Phone wire batch (logCategoryListVersion 1): the Support dialog's
    // categories with their labels, fixed for the process; only to a peer
    // that declared logCategoryList (StationServer::fitPeerOnlyProperties).
    { "RadioModel", "logCategoryList", MirrorDirection::ConstantSnapshot },
    // HL2 port part 2 (txInhibitReasonVersion 1): why the Core's transmit is
    // held off (the HL2 I/O board's fault code); only to a peer that
    // declared txInhibitReason (StationServer::fitPeerOnlyProperties).
    { "RadioModel", "txInhibitReason", MirrorDirection::Outbound },
    // PA on-air gate re-review, Important C (paTransmitBandVersion 1): the
    // PA row the Core holds on the air, Core to window only; only to a peer
    // that declared paTransmitBand (StationServer::fitPeerOnlyProperties).
    { "RadioModel", "paTransmitBand", MirrorDirection::Outbound },
    // Level Cal (radioHardwareVersion 12): the Core's calibration run as it
    // goes, Core to window only; only to a peer that declared
    // levelCalibration (StationServer::fitPeerOnlyProperties).
    { "RadioModel", "levelCalRunning", MirrorDirection::Outbound },
    { "RadioModel", "levelCalPercent", MirrorDirection::Outbound },
    { "RadioModel", "levelCalMessage", MirrorDirection::Outbound },
    { "RadioModel", "levelCalSucceeded", MirrorDirection::Outbound },
    // Shared-input filters, ruling (d) (rxFilterLowPassVersion 1): why the
    // receive low-pass on chain 0's input is held, and the slice holding
    // it, Core to window only; only to a peer that declared rxFilterLowPass
    // (StationServer::fitPeerOnlyProperties).
    { "RadioModel", "rxFilter0LowPassReason", MirrorDirection::Outbound },
    { "RadioModel", "rxFilter0LowPassSlice", MirrorDirection::Outbound },
    { "RadioModel", "diversityState", MirrorDirection::Outbound },

    // ---- PanadapterModel (4 entries) ----
    { "PanadapterModel", "centerFrequency", MirrorDirection::Bidirectional },
    { "PanadapterModel", "bandwidth", MirrorDirection::Bidirectional },
    { "PanadapterModel", "dBmFloor", MirrorDirection::Bidirectional },
    { "PanadapterModel", "dBmCeiling", MirrorDirection::Bidirectional },

};

QByteArray makeKey(const QByteArray& className, const QByteArray& property)
{
    // '\0' rather than "::" so a property name containing a colon could
    // never collide with a class-qualified key.
    return MirrorSchema::shortClassName(className) + '\0' + property;
}

const QHash<QByteArray, MirrorDirection>& lookupTable()
{
    static const QHash<QByteArray, MirrorDirection> table = [] {
        QHash<QByteArray, MirrorDirection> t;
        t.reserve(static_cast<int>(std::size(kEntries)));
        for (const MirrorPolicy::Entry& e : kEntries) {
            t.insert(makeKey(QByteArray(e.className), QByteArray(e.property)),
                     e.direction);
        }
        return t;
    }();
    return table;
}

} // namespace

MirrorDirection MirrorPolicy::directionFor(const QByteArray& className,
                                           const QByteArray& property)
{
    // Default deny. An unclassified property is mirrored out and never
    // written back.
    return lookupTable().value(makeKey(className, property),
                               MirrorDirection::Outbound);
}

bool MirrorPolicy::inboundAllowed(const QByteArray& className,
                                  const QByteArray& property)
{
    return directionFor(className, property) == MirrorDirection::Bidirectional;
}

bool MirrorPolicy::hasExplicitEntry(const QByteArray& className,
                                    const QByteArray& property)
{
    return lookupTable().contains(makeKey(className, property));
}

const QList<MirrorPolicy::Entry>& MirrorPolicy::entries()
{
    static const QList<Entry> all(std::begin(kEntries), std::end(kEntries));
    return all;
}

const QList<MirrorPolicy::FeatureGate>& MirrorPolicy::featureGates()
{
    static const QList<FeatureGate> gates{
        // R-IOS-13 / R-R3-49 (txEqCurveVersion 1): the read-only TX EQ
        // curve, to a peer that declared txEqCurve 1
        // (StationServer::fitTxEqCurveToPeer).
        {"TransmitModel", "txEqCurve", "txEqCurve", 1},
        // transmitSettingsVersion 15: the CFC band editor, to a peer that
        // declared cfcProfile 1 (StationServer::fitPeerOnlyProperties).
        {"TransmitModel", "cfcProfile", "cfcProfile", 1},
        // Phone wire batch (diversityPatternVersion 1): each slice's
        // Diversity dialog pattern, to a peer that declared
        // diversityPattern 1 (StationServer::fitPeerOnlyProperties).
        {"SliceModel", "diversityPattern", "diversityPattern", 1},
        {"RadioModel", "diversityState", "diversityControl", 1},
        // Phone wire batch (logCategoryListVersion 1): radio's logging
        // categories with their labels, to a peer that declared
        // logCategoryList 1 (StationServer::fitPeerOnlyProperties).
        {"RadioModel", "logCategoryList", "logCategoryList", 1},
        // HL2 port part 2: why the Core's transmit is held off, to a peer
        // that declared txInhibitReason 1.
        {"RadioModel", "txInhibitReason", "txInhibitReason", 1},
        // PA on-air gate re-review, Important C (paTransmitBandVersion 1):
        // the PA row the Core holds on the air, to a peer that declared
        // paTransmitBand 1 (StationServer::fitPeerOnlyProperties).
        {"RadioModel", "paTransmitBand", "paTransmitBand", 1},
        // The phone's direct addresses (coreAddressesVersion 1): where a
        // device can dial this Core, to a device signed in with its own key
        // that declared coreAddresses 1 (StationServer::fitPeerOnlyProperties).
        {"StationDevicesFacade", "coreAddresses", "coreAddresses", 1},
        // RADE status (radeStatusVersion 1): each slice's RADE decoder
        // sync and frequency offset, to a peer that declared radeStatus 1
        // (StationServer::fitPeerOnlyProperties).
        {"SliceModel", "radeSynced", "radeStatus", 1},
        {"SliceModel", "radeFreqOffsetHz", "radeStatus", 1},
        // RADE reason (radeReasonVersion 1): why each RADE slice has no
        // working decoder, to a peer that declared radeReason 1
        // (StationServer::fitPeerOnlyProperties).
        {"SliceModel", "radeReason", "radeReason", 1},
        // radioHardwareVersion 10: radio's Alex-1 low-pass in use, to a peer
        // that declared alexLpf 1 (StationServer::fitPeerOnlyProperties).
        {"RadioModel", "alexLpfBits", "alexLpf", 1},
        // radioHardwareVersion 12: the Core's level calibration run, to a
        // peer that declared levelCalibration 1.
        {"RadioModel", "levelCalRunning", "levelCalibration", 1},
        {"RadioModel", "levelCalPercent", "levelCalibration", 1},
        {"RadioModel", "levelCalMessage", "levelCalibration", 1},
        {"RadioModel", "levelCalSucceeded", "levelCalibration", 1},
        // Shared-input filters, ruling (d) (rxFilterLowPassVersion 1): the
        // receive low-pass reason and the slice holding it, to a peer that
        // declared rxFilterLowPass 1.
        {"RadioModel", "rxFilter0LowPassReason", "rxFilterLowPass", 1},
        {"RadioModel", "rxFilter0LowPassSlice", "rxFilterLowPass", 1},
    };
    return gates;
}

const MirrorPolicy::FeatureGate* MirrorPolicy::featureGateFor(const QByteArray& className,
                                                              const QByteArray& property)
{
    for (const FeatureGate& gate : featureGates()) {
        if (className == gate.className && property == gate.property) {
            return &gate;
        }
    }
    return nullptr;
}

} // namespace NereusSDR
