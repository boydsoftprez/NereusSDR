// NereusSDR for iOS: the TX panel: RF and tune power, TUNE and MOX, the amp's and tuner's OPERATE, the tuner's antennas, mic level and gain, PROC, VOX and MON
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusModels
import SwiftUI

/// The approved quick controls share their live values and writes with Modes.
/// Settings and the four key actions stay pinned above the scrolling groups.
struct TxPanel: View {
    @ObservedObject var transmit: TransmitModel
    @ObservedObject var accessories: AccessoriesModel
    /// The mic level meter's own level before keying.
    let micLevel: LiveMicLevel
    /// Mic Gain's value, range and write, shared with the Modes tab's row.
    @ObservedObject var modes: ModesTabModel
    let meters: StationCatalog.Meters?
    /// Opens the Radio tab, where an amplifier or tuner is set up.
    var openRadio: () -> Void = {}
    /// The panel's width; nil fills the width it is given (the iPad's
    /// applet column and front panel).
    var width: CGFloat? = TxPanel.width
    /// The panel scrolls itself and shows its own number pad; in the iPad's
    /// applet column the column does both.
    var scrolls = true
    /// The AM Mod Monitor, below the usual controls while the transmitting
    /// slice is in AM, SAM or DSB (D102).
    var modMonitor: ModMonitorModel?
    /// Take transmit, under the holder's line while another device holds
    /// transmit, as the desktop's TX applet offers it.
    var take: TransmitTakeModel? = nil

    /// The visible route to the full Transmit section in Modes.
    var openSettings: () -> Void = {}
    @State private var timingOpen = false
    @Environment(\.dynamicTypeSize) private var textSize

    static let width: CGFloat = 300

    var body: some View {
        Group {
            if scrolls {
                VStack(alignment: .leading, spacing: 0) {
                    pinnedControls
                    ScrollView { content }
                        .overlay { ValuePadLayer(pad: transmit.pad) }
                }
            } else {
                // The iPad column owns the ScrollView. Its TX section pins
                // the same header when the operator scrolls through TX.
                LazyVStack(alignment: .leading, spacing: 0, pinnedViews: [.sectionHeaders]) {
                    Section {
                        content
                    } header: {
                        pinnedControls
                    }
                }
            }
        }
        .frame(width: width)
        .frame(maxWidth: width == nil ? .infinity : nil, maxHeight: scrolls ? .infinity : nil, alignment: .top)
        .background(ChromeColours.panel)
        .environment(\.panelButtonHeight, 44)
        .environment(\.panelSliderHeight, 44)
        .environment(\.panelTextScale, textSize.isAccessibilitySize ? 1.5 : 1)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("TX panel")
        .accessibilityIdentifier("txPanelDrawer")
    }

    private var pinnedControls: some View {
        VStack(alignment: .leading, spacing: 0) {
            title
            keyControls.padding(10)
        }
        .background(ChromeColours.panel)
    }

    private var content: some View {
        VStack(alignment: .leading, spacing: 9) {
            caption("RF")
            LinearGauge(scale: .rfPower(meters?.rfPower), value: transmit.forwardWatts)
            TxSwrGauge(transmit: transmit, meters: meters)
            // Each radio's own range and readout, from the Core's catalogue
            // (on a Hermes Lite 2, drive in dB as its desktop shows it).
            PanelSliderRow(label: "RF Power", value: transmit.rfPower,
                           range: transmit.powerControl.range, accessibility: "RF power",
                           format: transmit.powerControl.text, greyed: !transmit.settingsEditable(1), notConfirmed: transmit.isUnconfirmed("power")) { transmit.setRfPower($0) }
                .accessibilityIdentifier("txRfPower")
            PanelSliderRow(label: "Tune Pwr", value: transmit.tunePower,
                           range: transmit.tuneControl.range, accessibility: "Tune power",
                           format: transmit.tuneControl.text,
                           greyed: !(transmit.settingsEditable(2) && transmit.tunePowerAvailable), notConfirmed: transmit.isUnconfirmed("tunePowerForTxBand")) { transmit.setTunePower($0) }
                .accessibilityIdentifier("txTunePower")
            if let reason = transmit.settingsReason(1) {
                note("RF Power, Tune Pwr, TX filter and the settings below: " + reason)
                    .accessibilityIdentifier("txSettingsReason")
            }
            if let reason = transmit.twoToneReason {
                note("2-Tone: " + reason)
                    .accessibilityIdentifier("twoToneReason")
            }
            if let reason = transmit.psaReason, !transmit.psa {
                note("PS-A: " + reason)
                    .accessibilityIdentifier("txPsaReason")
            }
            if ampConfigured || tunerConfigured {
                caption("Equipment")
                if ampConfigured { ampRow }
                if tunerConfigured { tunerRow }
            }
            caption("Audio")
            MicLevelGauge(transmit: transmit, level: micLevel, liveText: LiveMicLevel.liveInTxPanelText)
            // Mic Gain directly under the mic level meter, as on the
            // desktop's Phone/CW applet (JJ, 2026-09-30).
            MicGainRow(model: modes, transmit: transmit, identifier: "txMicGain")
            // The seven transmit stage readings, directly under Mic level
            // and above PROC, VOX and MON (JJ, 2026-09-28).
            TxStageMeters(transmit: transmit)
            caption("Voice")
            voiceRows
            AntiVoxReasons(transmit: transmit)
            caption("Processing")
            grid(columns: textSize.isAccessibilitySize ? 2 : 4) {
                PanelButton(label: "PROC", lit: transmit.proc, style: .dsp, disabled: !chainEditable) {
                    transmit.toggleProc()
                }
                .accessibilityIdentifier("txProc")
                PanelButton(label: "LEV", lit: modes.leveler == true, style: .dsp,
                            disabled: !chainEditable || modes.leveler == nil) { modes.toggleLeveler() }
                    .accessibilityLabel("Leveler")
                    .accessibilityIdentifier("txLeveler")
                PanelButton(label: "EQ", lit: modes.eq == true, style: .dsp,
                            disabled: !chainEditable || modes.eq == nil) { modes.toggleEq() }
                    .accessibilityIdentifier("txEq")
                PanelButton(label: "CFC", lit: modes.cfc == true, style: .dsp,
                            disabled: !chainEditable || modes.cfc == nil) { modes.toggleCfc() }
                    .accessibilityIdentifier("txCfc")
                PanelButton(label: "DEXP", lit: transmit.dexp == true, style: .dsp,
                            disabled: !chainEditable || transmit.dexp == nil) { transmit.toggleDexp() }
                    .accessibilityLabel("Downward expander")
                    .accessibilityIdentifier("txDexp")
                PanelButton(label: "MON", lit: transmit.mon, style: .dsp, disabled: transmit.monReason != nil) {
                    transmit.toggleMon()
                }
                .accessibilityHint(transmit.monReason ?? "")
                .accessibilityIdentifier("txMon")
            }
            caption("Profile")
            profileRow
            PanelSliderRow(label: "AM carrier", value: transmit.amCarrier,
                           range: TransmitModel.amCarrierRange,
                           accessibility: "AM carrier level", format: { "\(Int($0.rounded()))%" },
                           greyed: !chainEditable, notConfirmed: transmit.isUnconfirmed("amCarrierLevel")) {
                transmit.setAmCarrier($0)
            }
            if let reason = transmit.inhibitReason {
                // Shown while the radio holds transmit off; a key already on
                // stays lit so this phone can let it go.
                note("TUNE and MOX: " + reason)
                    .accessibilityIdentifier("txInhibitReason")
            }
            if let reason = Self.voxReason(transmit) {
                note("VOX: " + reason)
                    .accessibilityIdentifier("txVoxReason")
            }
            if let reason = transmit.monReason {
                note("MON: " + reason)
                    .accessibilityIdentifier("txMonReason")
            }
            if !transmit.tunePowerAvailable {
                note("Tune Pwr: " + TransmitModel.tunePowerOlderCoreText)
            }
            if !transmit.offered {
                note(TransmitModel.noRemoteTransmitText)
                    .accessibilityIdentifier("txPanelReason")
            } else if let reason = transmit.permission?.reason, !transmit.permitted {
                note("TUNE, MOX, 2-Tone and VOX: " + reason)
                    .accessibilityIdentifier("txPanelReason")
            }
            if let take, Self.offersTake(transmit) {
                TakeTransmitButton(take: take, identifier: "txTakeTransmit")
                    .padding(.top, 6)
            }
            if let reason = transmit.note {
                note(reason)
                    .accessibilityIdentifier("txPanelNote")
            }
            if let modMonitor {
                ModMonitorSlot(model: modMonitor)
                    .padding(.horizontal, -10)
            }
        }
        .padding(10)
    }

    private var keyControls: some View {
        grid(columns: textSize.isAccessibilitySize ? 2 : 4) {
            // One key at a time: TUNE waits while PTT is keyed, MOX while TUNE is on.
            PanelButton(label: "TUNE", lit: transmit.ptt.tuning, style: .red,
                        disabled: !keys || transmit.ptt.pttKeyed || transmit.ptt.twoTone
                            || transmit.ptt.tunerTuning
                            || (transmit.inhibitReason != nil && !transmit.ptt.tuning)) {
                transmit.toggleTune()
            }
            .accessibilityIdentifier("txTune")
            PanelButton(label: "MOX", lit: transmit.ptt.pttKeyed, style: .red,
                        disabled: !keys || transmit.ptt.tuning || transmit.ptt.twoTone
                            || transmit.ptt.tunerTuning
                            || (transmit.inhibitReason != nil && !transmit.ptt.pttKeyed)) {
                transmit.tapPtt()
            }
            .accessibilityIdentifier("txMox")
            // 2-Tone keys the two-tone test, one key at a time as TUNE (M6).
            PanelButton(label: "2-Tone", lit: transmit.ptt.twoTone || transmit.twoToneOn, style: .red,
                        disabled: !keys || transmit.ptt.pttKeyed || transmit.ptt.tuning
                            || transmit.ptt.tunerTuning || transmit.twoToneReason != nil) {
                transmit.toggleTwoTone()
            }
            .accessibilityLabel("Two-tone test")
            .accessibilityHint(transmit.twoToneReason ?? "")
            .accessibilityIdentifier("txTwoTone")
            // PS-A: PureSignal's automatic calibration (M7).
            PanelButton(label: "PS-A", lit: transmit.psa, style: .dsp,
                        disabled: transmit.psa ? !transmit.psaVerbsOffered : transmit.psaReason != nil) {
                transmit.togglePsa()
            }
            .accessibilityLabel("PureSignal automatic calibration")
            .accessibilityHint(transmit.psaReason ?? "")
            .accessibilityIdentifier("txPsa")
        }
    }

    /// Take transmit shows under the holder's line: another device holds
    /// transmit, or the Core's reason this phone may not transmit offers it.
    static func offersTake(_ transmit: TransmitModel) -> Bool {
        transmit.offered && (transmit.report.heldElsewhere
            || (!transmit.permitted && transmit.permission?.fix == TxRefusalInfo.takeTransmit))
    }

    /// The version 2 settings (PROC, DEXP, VOX level and delay, AM carrier) may change.
    private var chainEditable: Bool {
        transmit.settingsEditable(2)
    }

    /// TUNE, MOX and 2-Tone may key: the Core lets this phone transmit.
    private var keys: Bool {
        transmit.permitted || transmit.ptt.transmitting
    }

    /// Why VOX cannot be armed: it keys, so it needs this phone's transmit
    /// permission, and its microphone line to the Core.
    static func voxReason(_ transmit: TransmitModel) -> String? {
        if !transmit.offered {
            return TransmitModel.noRemoteTransmitText
        }
        if !transmit.permitted {
            return transmit.permission?.reason ?? TransmitModel.noRemoteTransmitText
        }
        return transmit.microphoneLine ? nil : TransmitModel.voxNeedsMicrophone
    }

    /// The TX profile (I12): the Core's profiles in its order, the active
    /// one ticked; a pick asks the Core (`txProfile.select`).
    private var profileRow: some View {
        let editable = transmit.settingsEditable(3) && !transmit.profiles.isEmpty
        return VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 8) {
                Text("Profile")
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .frame(width: 62, alignment: .leading)
                Menu {
                    ForEach(transmit.profiles, id: \.self) { name in
                        Button {
                            transmit.selectProfile(name)
                        } label: {
                            if name == transmit.activeProfile {
                                Label(name, systemImage: "checkmark")
                            } else {
                                Text(name)
                            }
                        }
                    }
                } label: {
                    HStack(spacing: 4) {
                        Text(transmit.activeProfile ?? "\u{2013}")
                            .font(.system(size: textSize.isAccessibilitySize ? 18 : 12, weight: .semibold))
                            .lineLimit(1)
                        Spacer(minLength: 0)
                        Image(systemName: "chevron.up.chevron.down")
                            .font(.system(size: 10, weight: .semibold))
                    }
                    .foregroundStyle(editable ? ChromeColours.text : ChromeColours.buttonOffText)
                    .padding(.horizontal, 8)
                    .frame(maxWidth: .infinity, minHeight: 44)
                    .background(editable ? ChromeColours.button : ChromeColours.buttonOff,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(editable ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                }
                .disabled(!editable)
                .accessibilityLabel("TX profile")
                .accessibilityValue(transmit.activeProfile ?? "")
                .accessibilityIdentifier("txProfile")
            }
            if let reason = transmit.settingsReason(3), transmit.settingsReason(1) == nil {
                note("Profile: " + reason)
            }
        }
    }

    /// The amplifier's row: the Core's Power Genius or RF-Kit, or greyed as not set up.
    @ViewBuilder
    private var ampRow: some View {
        if let amp = transmit.amp {
            let device: AccessoriesModel.Device = amp.kind == .rfKit ? .rfKit : .powerGenius
            accessoryRow(amp, available: transmit.ampOperateAvailable && accessories.switchReason(device) == nil) {
                transmit.setAmpOperate(!amp.operate)
            }
            if device == .powerGenius { powerGeniusReadouts }
            if let reason = accessories.switchReason(device) { note(reason) }
            else if !transmit.ampOperateAvailable { note(TransmitModel.ampOlderCoreText) }
            // The Core's warning while the Power Genius is over its output limit.
            if let alert = accessories.powerCapAlert {
                AccessoryChrome.Alert(text: alert)
                    .accessibilityIdentifier("txPowerCapAlert")
            }
        } else if let device = configuredAmp {
            blockedAccessory(device)
            if device == .powerGenius { powerGeniusReadouts }
        }
    }

    /// The tuner's row: the Core's Tuner Genius, or greyed as not set up.
    @ViewBuilder
    private var tunerRow: some View {
        if let tuner = transmit.tuner {
            accessoryRow(tuner, available: transmit.tunerOperateAvailable && accessories.switchReason(.tunerGenius) == nil,
                         tuneButton: true) {
                transmit.setTunerOperate(!tuner.operate)
            }
            tunerGeniusReadouts
            // The tuner's TUNE keys through the PTT (tx.tunerTune); greyed, its reason.
            if let reason = transmit.tunerTuneReason {
                note("Tuner TUNE: " + reason)
                    .accessibilityIdentifier("tunerTuneReason")
            }
            if let reason = accessories.switchReason(.tunerGenius) { note(reason) }
            else if !transmit.tunerOperateAvailable { note(TransmitModel.tunerOlderCoreText) }
            // Its three antennas, as on the Tuner Genius page, and the Core's
            // words when it refuses one.
            if let tunerGenius = accessories.tunerGenius, tunerGenius.hasAntennaSwitch {
                TunerAntennaRow(model: accessories, tuner: tunerGenius, identifierPrefix: "txTunerAntenna")
                if let refusal = accessories.notes[.tunerGenius] {
                    AccessoryChrome.Refusal(text: refusal) { accessories.dismissNote(.tunerGenius) }
                        .accessibilityIdentifier("txTunerRefusal")
                }
            }
        } else {
            blockedAccessory(.tunerGenius)
            tunerGeniusReadouts
            if let tuner = accessories.tunerGenius, tuner.hasAntennaSwitch {
                TunerAntennaRow(model: accessories, tuner: tuner, identifierPrefix: "txTunerAntenna")
            }
        }
    }

    private var powerGeniusReadouts: some View {
        let readings = accessories.powerGeniusReadings
        // Existing PGXL page meters: PowerGeniusPage.swift:42-45; desktop AmpApplet.cpp:95-145.
        return VStack(alignment: .leading, spacing: 4) {
            accessoryGauge(.ampPower, value: readings.forwardW, text: TxAccessoryReadoutText.power(readings.forwardW), device: "PGXL", id: "txPgxlPower")
            accessoryGauge(.ampSwr, value: readings.swr, text: TxAccessoryReadoutText.swr(readings.swr), device: "PGXL", id: "txPgxlSwr")
            accessoryGauge(.ampTemperature, value: readings.temperatureC, text: TxAccessoryReadoutText.temperature(readings.temperatureC), device: "PGXL", id: "txPgxlTemperature")
        }
    }

    private var tunerGeniusReadouts: some View {
        let readings = accessories.tunerGeniusReadings
        let amp = accessories.powerGenius
        // MainWindow.cpp:9586-9610 scales for the present PGXL in OPERATE.
        let amplifying = amp?.present == true && amp?.link.connected == true && amp?.operate == true
        // The control may be normalized or in dB; only its catalogue W readout is watts.
        let shown = transmit.powerControl.shown
        let radioWatts = shown?.unit == "W" ? shown?.max ?? 0 : 0
        return VStack(alignment: .leading, spacing: 4) {
            accessoryGauge(TxAccessoryMeterScale.tunerPower(maxWatts: radioWatts, amplifying: amplifying),
                           value: readings.forwardW, text: TxAccessoryReadoutText.power(readings.forwardW), device: "TGXL", id: "txTgxlPower")
            accessoryGauge(TxAccessoryMeterScale.tunerSwr, value: readings.swr,
                           text: TxAccessoryReadoutText.swr(readings.swr), device: "TGXL", id: "txTgxlSwr")
            // The page's read-only meter; its +/- command buttons are separate (TunerGeniusPage.swift:97-111).
            // Desktop stacks C1/L/C2: TunerApplet.cpp:204-219; RelayBar.cpp:86-130.
            ForEach(0..<3, id: \.self) { index in
                AccessoryChrome.RelayBar(label: ["C1", "L", "C2"][index], value: readings.relays[index])
                    .accessibilityIdentifier("txTgxlRelay\(index)")
            }
        }
    }

    private func accessoryGauge(_ scale: LinearGauge.Scale, value: Double?, text: String, device: String, id: String) -> some View {
        HStack(spacing: 8) {
            LinearGauge(scale: scale, value: value).accessibilityHidden(true)
            Text(text)
                .font(.system(size: textSize.isAccessibilitySize ? 17 : 11, weight: .semibold, design: .monospaced))
                .foregroundStyle(ChromeColours.text)
                .frame(width: textSize.isAccessibilitySize ? 110 : 74, alignment: .trailing)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(device) \(scale.title)")
        .accessibilityValue(text)
        .accessibilityIdentifier(id)
    }

    private var configuredAmp: AccessoriesModel.Device? {
        if accessories.standing(.powerGenius) == .setUp { return .powerGenius }
        if accessories.standing(.rfKit) == .setUp { return .rfKit }
        return nil
    }

    private var ampConfigured: Bool { configuredAmp != nil }
    private var tunerConfigured: Bool { accessories.standing(.tunerGenius) == .setUp }

    private func blockedAccessory(_ device: AccessoriesModel.Device) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(device.title).font(.system(size: 12)).foregroundStyle(ChromeColours.textDim)
                Spacer(minLength: 4)
                if device == .tunerGenius {
                    PanelButton(label: "TUNE", lit: false, style: .red, disabled: true) {}
                        .frame(width: 60)
                        .accessibilityIdentifier("tunerTune")
                }
                PanelButton(label: "OPERATE", lit: false, style: .dsp, disabled: true) {}
                    .frame(width: 96)
                    .accessibilityIdentifier(device == .tunerGenius ? "txTunerOperate" : "txAmpOperate")
            }
            if let reason = accessories.switchReason(device) { note(reason) }
        }
    }

    private var voiceRows: some View {
        VStack(alignment: .leading, spacing: 9) {
            HStack(spacing: 6) {
                PanelButton(label: "VOX", lit: transmit.vox, style: .dsp,
                            disabled: Self.voxReason(transmit) != nil && !transmit.vox) { transmit.toggleVox() }
                    .frame(width: textSize.isAccessibilitySize ? 90 : 76)
                    .accessibilityHint(Self.voxReason(transmit) ?? "")
                    .accessibilityIdentifier("txVox")
                PanelSliderRow(label: nil, value: transmit.voxThresholdDb, range: TransmitModel.voxThresholdRange,
                               accessibility: "VOX level", format: { "\(Int($0.rounded())) dB" },
                               greyed: !chainEditable, notConfirmed: transmit.isUnconfirmed("voxThresholdDb")) { transmit.setVoxThreshold($0) }
                    .accessibilityIdentifier("txVoxLevel")
            }
            HStack(spacing: 6) {
                PanelButton(label: "Anti-VOX", lit: transmit.antiVoxRun == true, style: .dsp,
                            disabled: !transmit.settingsEditable(1) || transmit.antiVoxRun == nil) { transmit.toggleAntiVox() }
                    .frame(width: textSize.isAccessibilitySize ? 90 : 76)
                    .accessibilityIdentifier("txAntiVox")
                PanelSliderRow(label: nil, value: transmit.antiVoxGainDb, range: TransmitModel.antiVoxGainRange,
                               accessibility: "Anti-VOX gain", format: { "\(Int($0.rounded())) dB" },
                               greyed: !transmit.settingsEditable(5), notConfirmed: transmit.isUnconfirmed("antiVoxGainDb")) { transmit.setAntiVoxGain($0) }
                    .accessibilityIdentifier("txAntiVoxGain")
            }
            Button { timingOpen.toggle() } label: {
                Label("Timing", systemImage: timingOpen ? "chevron.down" : "chevron.right")
                    .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .foregroundStyle(ChromeColours.text)
            .accessibilityValue(timingOpen ? "Expanded" : "Collapsed")
            .accessibilityIdentifier("txVoiceTiming")
            if timingOpen {
                PanelSliderRow(label: "VOX delay", value: transmit.voxHangMs, range: TransmitModel.voxHangRange,
                               accessibility: "VOX delay", format: { "\(Int($0.rounded())) ms" },
                               greyed: !chainEditable, notConfirmed: transmit.isUnconfirmed("voxHangTimeMs")) { transmit.setVoxHang($0) }
                    .accessibilityIdentifier("txVoxDelay")
                PanelSliderRow(label: "Anti-VOX time", value: transmit.antiVoxTauMs, range: TransmitModel.antiVoxTimeRange,
                               accessibility: "Anti-VOX time", format: { "\(Int($0.rounded())) ms" },
                               greyed: !transmit.settingsEditable(1), notConfirmed: transmit.isUnconfirmed("antiVoxTauMs")) { transmit.setAntiVoxTime($0) }
                    .accessibilityIdentifier("txAntiVoxTime")
            }
        }
    }

    /// "On at the Core, limit 2.0." and the like.
    static func swrProtectionText(_ protection: TransmitModel.SwrProtection?) -> String {
        guard let protection else {
            return "SWR protection: not known."
        }
        guard protection.on else {
            return "SWR protection: off at the Core."
        }
        guard let limit = protection.limit else {
            return "SWR protection: on at the Core."
        }
        return "SWR protection: on at the Core, limit " + String(format: "%.1f", limit) + "."
    }

    private var title: some View {
        HStack {
            Text("TX")
                .font(.system(size: textSize.isAccessibilitySize ? 18 : 11, weight: .bold))
                .foregroundStyle(ChromeColours.icon)
            Spacer(minLength: 8)
            Button(action: openSettings) {
                Text("Settings")
                    .font(.system(size: textSize.isAccessibilitySize ? 18 : 12, weight: .semibold))
                    .frame(minWidth: 80, minHeight: 44)
                    .contentShape(Rectangle())
            }
                .buttonStyle(.plain)
                .foregroundStyle(ChromeColours.text)
                .accessibilityLabel("Transmit settings in Modes")
                .accessibilityIdentifier("txSettings")
        }
        .padding(.horizontal, 8)
        .background(ChromeColours.bar)
        .overlay(alignment: .bottom) { Rectangle().fill(ChromeColours.titleBorder).frame(height: 1) }
    }

    private func accessoryRow(_ accessory: TransmitModel.Accessory, available: Bool, tuneButton: Bool = false,
                              toggle: @escaping () -> Void) -> some View {
        HStack(spacing: 8) {
            Text(accessory.name)
                .font(.system(size: textSize.isAccessibilitySize ? 18 : 12))
                .foregroundStyle(ChromeColours.text)
                .lineLimit(1)
                .frame(maxWidth: .infinity, alignment: .leading)
            if tuneButton {
                // The Tuner Genius's autotune, a key of this phone's: lit
                // while it is on, pressable to end it (desktop TunerApplet.cpp:620-651).
                PanelButton(label: "TUNE", lit: transmit.ptt.tunerTuning, style: .red,
                            disabled: transmit.tunerTuneReason != nil) {
                    transmit.toggleTunerTune()
                }
                .frame(width: 60)
                .accessibilityLabel("Tuner TUNE")
                .accessibilityHint(transmit.tunerTuneReason ?? "")
                .accessibilityIdentifier("tunerTune")
            }
            Button(action: toggle) {
                Text(accessory.operate ? "OPERATE" : "STANDBY")
                    .font(.system(size: textSize.isAccessibilitySize ? 18 : 12, weight: .bold))
                    .foregroundStyle(accessory.operate ? .white : ChromeColours.text)
                    .frame(minWidth: 96, minHeight: 44)
                    .background(accessory.operate ? ChromeColours.operateOn : ChromeColours.operateOff,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(accessory.operate ? ChromeColours.operateOnBorder : ChromeColours.operateOffBorder,
                                      lineWidth: 1))
            }
            .buttonStyle(.plain)
            // OPERATE follows the Core's verb for the device, not transmit
            // permission; the Core refuses it on the air with its words (D60).
            .disabled(!available)
            .opacity(available ? 1 : 0.5)
            .accessibilityLabel(accessory.name)
            .accessibilityValue(accessory.operate ? "Operate" : "Standby")
            .accessibilityIdentifier(tuneButton ? "txTunerOperate" : "txAmpOperate")
        }
    }

    private func caption(_ text: String) -> some View {
        Text(text)
            .font(.system(size: textSize.isAccessibilitySize ? 18 : 11))
            .foregroundStyle(ChromeColours.caption)
            .padding(.bottom, -4)
    }

    private func note(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.textFaint)
            .fixedSize(horizontal: false, vertical: true)
    }

    private func grid(columns: Int, @ViewBuilder content: () -> some View) -> some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible(minimum: 0), spacing: 4), count: columns), spacing: 4) {
            content()
        }
    }
}

/// Only an actual fresh transmit sample above a known configured limit qualifies.
struct TxSwrWarning: Equatable {
    let reading: Double
    let limit: Double
    static func evaluate(_ reading: Double?, limit: Double?) -> Self? {
        guard let reading, let limit, reading.isFinite, limit.isFinite,
              reading >= 1, limit >= 1, reading > limit else { return nil }
        return Self(reading: reading, limit: limit)
    }

    var explanation: String {
        String(format: "Fresh SWR %.1f is above the configured limit %.1f. The Core does not report a power-cut event.",
               reading, limit)
    }
}

/// Onset and toast lifetime use an injected monotonic time, without warning on absence.
struct TxSwrWarningState {
    private var active = false
    private var onset: Int64?
    mutating func update(_ warning: TxSwrWarning?, nowMilliseconds: Int64) {
        if warning == nil { active = false; onset = nil }
        else if !active { active = true; onset = nowMilliseconds }
    }
    func toastVisible(nowMilliseconds: Int64) -> Bool {
        guard active, let onset else { return false }
        return nowMilliseconds >= onset && nowMilliseconds - onset < 3_000
    }
}

/// The expiry tick rereads actual receipts; it never makes a telemetry sample.
private struct TxSwrGauge: View {
    @ObservedObject var transmit: TransmitModel
    let meters: StationCatalog.Meters?
    @State private var warningState = TxSwrWarningState()
    @State private var details = false
    @Environment(\.dynamicTypeSize) private var textSize

    var body: some View {
        TimelineView(.periodic(from: .now, by: 0.25)) { _ in
            let warning = transmit.highSwr
            let now = SystemLinkClock().nowMilliseconds
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 6) {
                    LinearGauge(scale: .swr(meters?.swr), value: transmit.swr)
                    if warning != nil {
                        Button { details = true } label: {
                            Text("High SWR")
                                .font(.system(size: textSize.isAccessibilitySize ? 18 : 12, weight: .bold))
                                .foregroundStyle(ConnectChrome.warn)
                                .padding(.horizontal, 8)
                                .frame(minHeight: 44)
                                .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 6))
                                .contentShape(Rectangle())
                        }
                            .buttonStyle(.plain)
                            .accessibilityIdentifier("txHighSwr")
                    }
                }
                if let warning, warningState.toastVisible(nowMilliseconds: now) {
                    Text(warning.explanation)
                        .font(.system(size: textSize.isAccessibilitySize ? 17 : 12))
                        .foregroundStyle(ConnectChrome.warn)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("txHighSwrToast")
                }
            }
            .onChange(of: warning, initial: true) { _, current in
                warningState.update(current, nowMilliseconds: SystemLinkClock().nowMilliseconds)
                if current == nil { details = false }
            }
            .alert("High SWR", isPresented: $details) {
                Button("Close", role: .cancel) {}
            } message: {
                Text(warning?.explanation ?? "")
            }
        }
    }
}

/// Independent minimum versions and missing fields stay visible with their reason.
struct AntiVoxReasons: View {
    @ObservedObject var transmit: TransmitModel
    @Environment(\.dynamicTypeSize) private var textSize
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if let run = transmit.antiVoxRunReason, run == transmit.antiVoxTimeReason {
                Text("Anti-VOX Run/Time: " + run)
            } else {
                if let run = transmit.antiVoxRunReason { Text("Anti-VOX Run: " + run) }
                if let time = transmit.antiVoxTimeReason { Text("Anti-VOX time: " + time) }
            }
            if let gain = transmit.antiVoxGainReason { Text("Anti-VOX gain: " + gain) }
        }
        .font(.system(size: textSize.isAccessibilitySize ? 17 : 11))
        .foregroundStyle(ChromeColours.textFaint)
        .fixedSize(horizontal: false, vertical: true)
        .accessibilityIdentifier("antiVoxReasons")
    }
}

/// Compact wire-unit text; absent readings never masquerade as measured zero.
enum TxAccessoryReadoutText {
    static func power(_ value: Double?) -> String { value.map { String(format: "%.0f W", $0) } ?? "--" }
    static func swr(_ value: Double?) -> String { value.map { String(format: "%.2f:1", $0) } ?? "--" }
    static func temperature(_ value: Double?) -> String { value.map { String(format: "%.1f °C", $0) } ?? "--" }
    static func relay(_ value: Int64?) -> String { value.map(String.init) ?? "--" }
}

/// Desktop TGXL meter limits (src/gui/applets/TunerApplet.cpp:180-197, 884-904).
/// Unlike PGXL, TGXL has no intermediate yellow power/SWR zone or explicit tick labels.
enum TxAccessoryMeterScale {
    static func tunerPower(maxWatts: Double, amplifying: Bool) -> LinearGauge.Scale {
        let maximum: Double = amplifying ? 2000 : maxWatts > 100 ? 600 : 200
        let red: Double = amplifying ? 1500 : maxWatts > 100 ? 500 : 125
        return LinearGauge.Scale(title: "Fwd Pwr", min: 0, max: maximum, yellowFrom: red, redFrom: red, ticks: [])
    }

    static let tunerSwr = LinearGauge.Scale(title: "SWR", min: 1, max: 3, yellowFrom: 2.5, redFrom: 2.5, ticks: [])
}
