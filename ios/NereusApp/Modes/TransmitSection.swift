// NereusSDR for iOS: the Modes tab's transmit settings: TX filter, mic level and gain, PROC, LEV, EQ, CFC, VOX, DEXP, AM carrier and MON
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusModels
import SwiftUI

/// The Transmit section: the Core's TX filter (its edges typed in, and
/// Match RX, D81), the mic level and the mic gain, PROC and its level, LEV, EQ, CFC, VOX
/// with its level and delay, DEXP, the AM carrier and MON. As on the
/// desktop (I1), a setting that keys nothing changes while the Core takes
/// it and its radio is off the air, whether or not this phone may
/// transmit; otherwise it is greyed with the reason. VOX arms a key, so it
/// follows this phone's transmit permission and microphone line. MON is
/// greyed with its reason while the Core does not send its transmit
/// monitor or the phone's sound is not in headphones (D80), and the CFC
/// bars with theirs while the Core does not send its CFC meter.
struct TransmitSection: View {
    @ObservedObject var model: ModesTabModel
    @ObservedObject var transmit: TransmitModel
    /// The mic level meter's own level before keying.
    let micLevel: LiveMicLevel
    @Environment(\.dynamicTypeSize) private var textSize

    var body: some View {
        ModesChrome.section("Transmit") {
            HStack(spacing: 6) {
                ModesChrome.label("TX filter")
                ValueField(text: ModesChrome.hertz(model.txFilterLowHz), accessibility: "TX filter low edge",
                           disabled: !filterEditable || model.txFilterLowHz == nil, minWidth: 74, minHeight: 44) {
                    model.openTxFilterPad(low: true)
                }
                .accessibilityIdentifier("modesTxFilterLow")
                Text("to")
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                ValueField(text: ModesChrome.hertz(model.txFilterHighHz), accessibility: "TX filter high edge",
                           disabled: !filterEditable || model.txFilterHighHz == nil, minWidth: 74, minHeight: 44) {
                    model.openTxFilterPad(low: false)
                }
                .accessibilityIdentifier("modesTxFilterHigh")
                Spacer(minLength: 0)
                PanelButton(label: "Match RX", lit: false, style: .dsp, disabled: !filterEditable) {
                    model.matchRx()
                }
                .frame(width: 78)
                .accessibilityHint("Sets the TX filter to the receive filter")
                .accessibilityIdentifier("modesMatchRx")
            }
            if let reason = transmit.settingsReason(1) {
                ModesChrome.note("TX filter: " + reason)
                    .accessibilityIdentifier("modesTransmitReason")
            }
            // The mic level beside Mic Gain, live from this phone's
            // microphone before keying, so Mic Gain can be set by it.
            MicLevelGauge(transmit: transmit, level: micLevel, liveText: LiveMicLevel.liveText)
            MicGainRow(model: model, transmit: transmit, identifier: "modesMicGain")
            TxStageMeters(transmit: transmit)
            HStack(spacing: 6) {
                PanelButton(label: "PROC", lit: transmit.proc, style: .dsp, disabled: !chainEditable) {
                    transmit.toggleProc()
                }
                .frame(width: 62)
                PanelSliderRow(label: nil, value: model.procLevelDb,
                               range: ModesTabModel.procLevelRange,
                               accessibility: "Processor level", greyed: !chainEditable, notConfirmed: model.isUnconfirmed(ModesTabModel.Transmit.cpdrLevelDb, transmit: true)) { model.setProcLevel($0) }
            }
            ModesChrome.grid(columns: 5) {
                PanelButton(label: "LEV", lit: model.leveler == true, style: .dsp,
                            disabled: !chainEditable || model.leveler == nil) {
                    model.toggleLeveler()
                }
                .accessibilityLabel("Leveler")
                PanelButton(label: "EQ", lit: model.eq == true, style: .dsp, disabled: !chainEditable || model.eq == nil) {
                    model.toggleEq()
                }
                PanelButton(label: "CFC", lit: model.cfc == true, style: .dsp, disabled: !chainEditable || model.cfc == nil) {
                    model.toggleCfc()
                }
                PanelButton(label: "DEXP", lit: transmit.dexp == true, style: .dsp,
                            disabled: !chainEditable || transmit.dexp == nil) {
                    transmit.toggleDexp()
                }
                .accessibilityLabel("Downward expander")
                // MON writes the Core's monEnabled and asks for the monitor in
                // this phone's headphones (D80); greyed with its reason otherwise.
                PanelButton(label: "MON", lit: transmit.mon, style: .dsp, disabled: transmit.monReason != nil) {
                    transmit.toggleMon()
                }
                .accessibilityHint(transmit.monReason ?? "")
                .accessibilityIdentifier("modesMon")
            }
            if let reason = transmit.settingsReason(ModesTabModel.chainVersion) {
                ModesChrome.note("Mic gain, PROC, LEV, EQ, CFC, DEXP, VOX level and AM carrier: " + reason)
            }
            HStack(spacing: 6) {
                PanelButton(label: "VOX", lit: transmit.vox, style: .dsp, disabled: !voxArmable) {
                    transmit.toggleVox()
                }
                .frame(width: 62)
                .accessibilityHint(voxReason ?? "")
                .accessibilityIdentifier("modesVox")
                PanelSliderRow(label: nil, value: transmit.voxThresholdDb,
                               range: TransmitModel.voxThresholdRange,
                               accessibility: "VOX level", format: { "\(Int($0.rounded())) dB" },
                               greyed: !chainEditable, notConfirmed: transmit.isUnconfirmed("voxThresholdDb")) {
                    transmit.setVoxThreshold($0)
                }
            }
            PanelSliderRow(label: "VOX delay", value: transmit.voxHangMs,
                           range: TransmitModel.voxHangRange,
                           accessibility: "VOX delay", format: { "\(Int($0.rounded())) ms" },
                           greyed: !chainEditable, notConfirmed: transmit.isUnconfirmed("voxHangTimeMs")) {
                transmit.setVoxHang($0)
            }
            HStack(spacing: 6) {
                PanelButton(label: "Anti-VOX", lit: transmit.antiVoxRun == true, style: .dsp,
                            disabled: !transmit.settingsEditable(1) || transmit.antiVoxRun == nil) {
                    transmit.toggleAntiVox()
                }
                .frame(width: 86)
                .accessibilityIdentifier("modesAntiVox")
                PanelSliderRow(label: nil, value: transmit.antiVoxGainDb, range: TransmitModel.antiVoxGainRange,
                               accessibility: "Anti-VOX gain", format: { "\(Int($0.rounded())) dB" },
                               greyed: !transmit.settingsEditable(5), notConfirmed: transmit.isUnconfirmed("antiVoxGainDb")) { transmit.setAntiVoxGain($0) }
                    .accessibilityIdentifier("modesAntiVoxGain")
            }
            PanelSliderRow(label: "Anti-VOX time", value: transmit.antiVoxTauMs, range: TransmitModel.antiVoxTimeRange,
                           accessibility: "Anti-VOX time", format: { "\(Int($0.rounded())) ms" },
                           greyed: !transmit.settingsEditable(1), notConfirmed: transmit.isUnconfirmed("antiVoxTauMs")) { transmit.setAntiVoxTime($0) }
                .accessibilityIdentifier("modesAntiVoxTime")
            AntiVoxReasons(transmit: transmit)
            PanelSliderRow(label: "MON volume", value: transmit.monitorVolume, range: TransmitModel.monitorVolumeRange,
                           accessibility: "Monitor volume", format: { "\(Int(($0 * 100).rounded()))%" },
                           greyed: !chainEditable, notConfirmed: transmit.isUnconfirmed("monitorVolume")) { transmit.setMonitorVolume($0) }
                .accessibilityIdentifier("modesMonitorVolume")
            PanelSliderRow(label: "AM carrier", value: transmit.amCarrier,
                           range: TransmitModel.amCarrierRange,
                           accessibility: "AM carrier level", format: { "\(Int($0.rounded()))%" },
                           greyed: !chainEditable, notConfirmed: transmit.isUnconfirmed("amCarrierLevel")) {
                transmit.setAmCarrier($0)
            }
            cfcBars
            if let reason = voxReason {
                ModesChrome.note("VOX: " + reason)
                    .accessibilityIdentifier("modesVoxReason")
            }
            if let reason = transmit.monReason {
                ModesChrome.note("MON: " + reason)
                    .accessibilityIdentifier("modesMonReason")
            }
            if let reason = transmit.note {
                ModesChrome.note(reason)
                    .accessibilityIdentifier("modesTransmitNote")
            }
        }
        .environment(\.panelButtonHeight, 44)
        .environment(\.panelSliderHeight, 44)
        .environment(\.panelTextScale, textSize.isAccessibilitySize ? 1.5 : 1)
    }

    /// RF power and the TX filter (version 1) may change.
    private var filterEditable: Bool {
        transmit.settingsEditable(1)
    }

    /// The version 2 settings may change.
    private var chainEditable: Bool {
        transmit.settingsEditable(ModesTabModel.chainVersion)
    }

    /// VOX arms a key: this phone may transmit and its microphone line is open.
    private var voxArmable: Bool {
        voxReason == nil
    }

    private var voxReason: String? {
        TxPanel.voxReason(transmit)
    }

    /// The CFC bars' place, greyed, with the reason beneath: this Core does
    /// not send them to a phone.
    private var cfcBars: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 6) {
                ModesChrome.label("CFC bars")
                RoundedRectangle(cornerRadius: 3)
                    .fill(ChromeColours.buttonOff)
                    .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.buttonOffBorder, lineWidth: 1))
                    .frame(maxWidth: .infinity)
                    .frame(height: 22)
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("CFC bars")
            .accessibilityValue("Not available")
            .accessibilityHint(ModesTabModel.cfcBarsNotOnLinkText)
            ModesChrome.note("CFC bars: " + ModesTabModel.cfcBarsNotOnLinkText)
        }
    }
}

/// Mic Gain: the Core's mic gain (`micGainDb`) on its catalogue range,
/// greyed while the chain may not change or the Core's mic is muted. The
/// Modes tab and the TX panel both show this row on the one value in
/// ``ModesTabModel``, so they move together and send the same write.
///
/// Before the Core has sent its mic gain the row shows -6 dB, the thumb
/// there and "-6" in its box, as the desktop's Phone/CW applet does before
/// it knows the value: its slider and label start from TransmitModel's
/// own -6 dB (src/models/TransmitModel.h:2789, src/gui/applets/
/// PhoneCwApplet.cpp:913-917) and follow micGainDbChanged after. Greying
/// follows the same rule as with the Core's value.
struct MicGainRow: View {
    @ObservedObject var model: ModesTabModel
    @ObservedObject var transmit: TransmitModel
    let identifier: String

    /// The desktop's mic gain before it knows the Core's, in dB.
    static let beforeCoreDb: Double = -6

    /// The value the row shows: the Core's, else the desktop's starting value.
    static func shown(_ model: ModesTabModel) -> Double {
        model.micGainDb ?? beforeCoreDb
    }

    var body: some View {
        PanelSliderRow(label: "Mic Gain", value: Self.shown(model),
                       range: model.micGainRange,
                       accessibility: "Microphone gain", greyed: Self.range(model, transmit) == nil, notConfirmed: model.isUnconfirmed(ModesTabModel.Transmit.micGainDb, transmit: true)) { model.setMicGain($0) }
            .accessibilityHint(transmit.micMuted ? TransmitModel.micMutedText : "")
            .accessibilityIdentifier(identifier)
    }

    /// The slider's range, or nil while it is greyed (the row still places
    /// its thumb on the catalogue's range).
    static func range(_ model: ModesTabModel, _ transmit: TransmitModel) -> StationCatalog.Range? {
        transmit.settingsEditable(ModesTabModel.chainVersion) && !transmit.micMuted ? model.micGainRange : nil
    }
}
