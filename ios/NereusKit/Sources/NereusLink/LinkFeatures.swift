// NereusSDR for iOS: the features the app declares in its hello
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The `features` object of the app's `hello` (link document section 6.1):
/// what the Core must know about this app before it sends capabilities.
/// Each feature the Core gates on a declaration adds its entry here, name
/// to version, when the app gains it.
///
/// `deviceAuth` 1: the app signs in with its device key (section 3.5), and
/// the Core may send it the `devices` object. The app never declares
/// `pairing`, which only a Core declares (section 3.6).
///
/// `sessionHolder` 1: the app shares a Core with other devices (the
/// several-devices design, ruling 10.1): it draws other devices' slices as
/// markers, and answers the Core's confirmations and notices. A Core reads
/// it only beside `deviceAuth`, which the app always declares.
///
/// `remoteTx` 1: the app transmits through the Core (section 18): it keys
/// with `tx.key`, `tx.unkey`, `tx.tune` and `tx.twoTone`, sends
/// `tx.keepalive` every 100 ms while its key is on or its VOX is armed, and
/// reads `txState`. A Core that is sent it at minor 11 watches the app's
/// keepalives and stops a key of the app's that goes quiet.
///
/// `setupDescription` 24: the app draws every Setup page the Core
/// describes, through description version 24 (Display's Grid & Scales,
/// 3D View and the rest of Setup > Display, Appearance's Reset all
/// colors, then the pages versions 13 to 24 add: 17 the Alex-1 low-pass
/// rows, 18 the HL2 clock rows, 19 the CFC band editor, 20 the PA
/// profile rows' on-air state, 21 the TCI Forget row's dependency, 22
/// the RX buffer rows' on-air state, 23 Calibration's Rx1 6m LNA row and
/// 24 Audio > TX Input's Line In Gain in 1.5 dB steps and the Saturn G2's
/// Mic Tip-Ring row), and sends each control to its owner
/// (the Setup description contract); the Core sends the `setup` object
/// and projects each category to this version. A Core that offers less
/// is read at its own version; rows it cannot run on a phone stay greyed
/// with a plain reason.
///
/// `cfcProfile` 1: the app's CFC band editor reads `transmit.cfcProfile`
/// and sends `cfc.setProfile` with the whole profile and the revision it
/// was edited from.
///
/// `paProfiles` 1: the app's PA Gain page reads the Core's profile bank
/// through `paProfiles` and sends the nine `paProfile` lifecycle and cell
/// commands. The Core then sends `paProfileVersion` and, at 1, the
/// `PaProfilesFacade` object. Values and row availability come from the Core.
///
/// `settingsHygiene` 2: the app's Settings Validation panel sends
/// `station.validateSettings`, `station.repairSettings` and
/// `station.forgetSettings` with the connected radio's address and reads
/// their bounded answer; the Core then sends `settingsHygieneVersion`. At
/// 1 the Core has no repair, and its greyed Reset is shown as it words it.
///
/// `radioAntennaRows` 1: the app's per-band antenna tables send
/// `setAlexTxAntennaForRadio` and `setAlexRxAntennaForRadio` with the
/// connected radio's address; the Core then sends `radioAntennaRowsVersion`.
///
/// `vax` 1: the app's VAX Audio page shows the VAX channels of the
/// computer the Core runs on; the Core then sends `vaxVersion` and, at 1,
/// the `vax` object and the `vaxLevels` record stream.
///
/// `txEqCurve` 2: the app reads the TX EQ curve as the link document's
/// section 7.1 gives it and changes it through `txEq.setCurve` and
/// `txEq.resetCurve`; the Core then sends `txEqCurveVersion` and
/// `transmit`'s `txEqCurve`. A Core that knows only 1 answers 1, and the
/// curve stays read-only.
///
/// `diversityPattern` 1: the app draws the Diversity page's sensitivity
/// pattern from the Core's samples; the Core then sends
/// `diversityPatternVersion` and each slice's `diversityPattern`.
///
/// `logCategoryList` 1: the app names the Core's logging categories by the
/// Core's own labels; the Core then sends `logCategoryListVersion` and
/// `radio`'s `logCategoryList`.
///
/// `radioModels` 1: the app names each radio the Core can see by its model
/// and offers the models it can run as; the Core then sends
/// `radioModelsVersion` and, at 1, `modelLabel` and `models` on each
/// `stationRadios` record.
///
/// `band2m` 1: the app knows 2 m as a band of its own, band 27 (section
/// 6.1): its band grid, spot and FreeDV Reporter filters take the band the
/// Core's catalogue and records name, its antenna tables take the Core's
/// fifteenth row and list entry, and its RF Power write carries the `2m`
/// key. The Core then sends `band2mVersion`; a Core that sends none keeps
/// 2 m as GEN (11) and the 14-band lists.
///
/// `radeStatus` 1: the app's RADE row on the VFO flag reads whether the
/// receiver is locked on and the frequency offset. The Core then sends
/// `radeStatusVersion` and, at 1, `radeSynced` and `radeFreqOffsetHz` on
/// each slice; a Core that sends none leaves the row greyed with its reason.
///
/// `coreAddresses` 1: the app dials the Core at the addresses the Core
/// itself reports (``CoreAddressList``), so a phone away from home reaches
/// the Core's global IPv6 address directly. A Core sends
/// `coreAddressesVersion` and `devices`' `coreAddresses` only to a device
/// signed in with its own key, as this app always signs in.
///
/// `stationTciSettings` 1: the app's described Setup draws the Core TCI
/// server settings rows (rate limit, IQ swap, audio block size, TX
/// channel, sensor intervals and the VFO quirks) and sends each through
/// `setStationTciSettings`. The Core then sends
/// `stationTciSettingsVersion` and the settings on `stationTci`; a Core
/// that sends none leaves those rows greyed with their reason.
///
/// `txInhibitReason` 1: the app reads why the radio's transmit inhibit
/// input holds transmit off. The Core then sends `txInhibitReasonVersion`
/// and `radio`'s `txInhibitReason`; a Core that sends none leaves the
/// phone's own plain words under greyed MOX and TUNE.
///
/// `alexLpf` 1: the app marks the Alex-1 low-pass filter the radio is
/// using beside its rows on Setup > Hardware > Alex-1 Filters. The Core
/// then sends `radio`'s `alexLpfBits` (read only, never written); a Core
/// that sends none, or sends -1, leaves the rows unmarked.
///
/// `mediaDirect` 1: the app tries a direct-only media connection while
/// its media rides the Core's tunnel, and moves back to the tunnel when a
/// direct path goes quiet (the direct media ladder, section 6.3). The Core
/// then sends `mediaDirectVersion` and `mediaStunUrls`, and takes a
/// `replace` carrying `"mediaDirectVersion": 1`.
///
/// `sliceAccess` 3: the app shares slices with the other devices on the
/// Core (the slice access contract note): it reads each joined slice's
/// `access:<id>`, names who controls a slice it only listens to, offers
/// Take control (`slice.takeControl`), offers Take it back on the
/// `controlTaken` notice (2; 1 is the wire before Take it back), and
/// offers Take control on the Core's own slice too (3; 2 is the wire
/// before it). A Core reads it only beside `sessionHolder` and
/// `deviceAuth`, which the app always declares; the Core then sends
/// `sliceAccessVersion`, the lower of its version and 3.
///
/// `levelCalibration` 1: the app's Setup > Hardware > Calibration shows
/// the Core's level calibration run; the Core then sends `radio`'s
/// `levelCalRunning`, `levelCalPercent`, `levelCalMessage` and
/// `levelCalSucceeded`.
///
/// `adcAttenuators` 1: the app shows each slice the step attenuator of the
/// receiver input that slice is on; the Core then sends
/// `adcAttenuatorVersion` and `stepAtt`'s `rx2AttenuationDb`,
/// `rx2SliceMask`, RX2's own enable and auto-attenuate settings and, at
/// `radioHardwareVersion` 12, `rx2PreampMode`.
///
/// `rx2Attenuator` 1: the app reads RX2's own input control from the
/// catalogue's `board` (`rx2Attenuator`, `rx2PreampItems`,
/// `rx2AttenuatorReason`); the Core then sends `rx2AttenuatorVersion`.
///
/// `radioMic` 1: the app reads whether the radio's own mic input can be
/// chosen, and the note that goes with it, from the catalogue's `board`
/// (`radioMic`, `radioMicNote`); the Core then sends `radioMicVersion`.
///
/// `rxFilterLowPass` 1: the app shows why the receive low-pass on the
/// first receiver input is set for another slice, beside the Core's words
/// for that input's filters, as the desktop's Filter Policy dialog shows
/// them; the Core then sends `rxFilterLowPassVersion` and `radio`'s
/// `rxFilter0LowPassReason` and `rxFilter0LowPassSlice`. A Core that sends
/// neither shows the input's own words alone.
///
/// `radeReason` 1: the app shows why a slice in RADE has no working RADE
/// decoder on the flag's RADE row (`RADE ○ off` and the Core's sentence),
/// as the desktop's flag shows it. The Core then sends `radeReasonVersion`
/// and each slice's `radeReason`; a Core that sends none leaves the row as
/// it was.
///
/// `audioQuality` 1: the app chooses the Opus bitrate of its own main
/// audio from the catalogue's `audio.opusProfiles` and sends it as the
/// `audio` control's `opusBitrate`, beside `profile` (the media control
/// document, "Per-device audio quality"). The Core then sends
/// `audioQualityVersion` 1 while it offers media; a Core that sends none
/// plays its own bitrate and the phone's choice of quality stays greyed.
///
/// The PA readings need no declaration: the Core sends its telemetry to
/// every peer at `stationTelemetryVersion` 4 or later.
public enum LinkFeatures {
    public static let app: [String: Int] = ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                            "setupDescription": 24, "settingsHygiene": 2,
                                            "radioAntennaRows": 1,
                                            "vax": 1, "txEqCurve": 2,
                                            "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1,
                                            CoreAddressList.featureName: 1,
                                            "radeStatus": 1, "stationTciSettings": 1,
                                            "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1,
                                            "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                            "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1,
                                            "radioMic": 1, "rxFilterLowPass": 1, "radeReason": 1,
                                            "audioQuality": 1]
}
