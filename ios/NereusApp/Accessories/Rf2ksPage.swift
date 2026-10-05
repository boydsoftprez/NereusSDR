// NereusSDR for iOS: the RF-Kit RF2K-S's page: OPERATE or STANDBY, its readings, its antennas and its greyed tuner
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The RF-Kit RF2K-S (spec section 5.4 item 4, picture 11): reached
/// through the Core; OPERATE or STANDBY; power, SWR and heat; volts and amps; its
/// antennas with the operator's names; and its tuner, whose buttons stay
/// greyed with the note to use the amp's front panel until its firmware
/// accepts tuner commands. RF-Kit settings opens what the Core keeps of it.
struct Rf2ksPage: View {
    @ObservedObject var model: AccessoriesModel
    let open: (AccessoriesSection.Route) -> Void

    static let notReportedText =
        "This Core does not report its RF-Kit amplifier to this app. Updating the Core may help."

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let note = model.notes[.rfKit] {
                AccessoryChrome.Refusal(text: note) { model.dismissNote(.rfKit) }
            }
            if let rfKit = model.rfKit {
                AccessorySetupCard(model: model, device: .rfKit, open: open)
                AccessoryChrome.Header(name: rfKit.link.model.isEmpty ? "RF2K-S" : rfKit.link.model,
                                       status: AccessoryStatusLine.rfKitHeader(rfKit),
                                       dot: rfKit.link.connected ? ChromeColours.linkUp : ChromeColours.linkOffDot,
                                       button: rfKit.operate ? "OPERATE" : "STANDBY", lit: rfKit.operate,
                                       reason: model.switchReason(.rfKit)) {
                    model.setRfKitOperate(!rfKit.operate)
                }
                if !rfKit.link.error.isEmpty {
                    AccessoryChrome.Note(text: rfKit.link.error)
                }
                if let words = AccessoryStatusLine.rfKitBandFollow(rfKit) {
                    AccessoryChrome.Note(text: words)
                        .accessibilityIdentifier("rfkitBandFollow")
                }
                AccessoryChrome.Card {
                    LinearGauge(scale: .ampPower, value: rfKit.forwardW)
                    LinearGauge(scale: .ampSwr, value: rfKit.swr)
                    LinearGauge(scale: .ampTemperature, value: rfKit.temperatureC)
                }
                Text(Self.readings(rfKit))
                    .font(.system(size: 12, weight: .semibold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .frame(maxWidth: .infinity)
                    .accessibilityIdentifier("rfkitReadings")
                antennas(rfKit)
                AccessoryChrome.Card {
                    Text("Tuner: \(Self.tunerWord(rfKit.tunerMode))")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundStyle(ChromeColours.text)
                    HStack(spacing: 4) {
                        AccessoryChrome.GreyedButton(label: "TUNE")
                            .accessibilityHint(AccessoriesModel.rfKitTunerNote)
                        AccessoryChrome.GreyedButton(label: "BYPASS")
                            .accessibilityHint(AccessoriesModel.rfKitTunerNote)
                    }
                    AccessoryChrome.Note(text: AccessoriesModel.rfKitTunerNote)
                        .accessibilityIdentifier("rfkitTunerNote")
                }
                AccessoryChrome.Rows {
                    AccessoryChrome.PageRow(title: "RF-Kit settings",
                                            detail: "Connection \u{00B7} Antenna labels \u{00B7} Live diagnostics",
                                            identifier: "rfkitSettings") {
                        open(.advanced(.rfKit))
                    }
                }
            } else {
                AccessoryChrome.Card {
                    AccessoryChrome.Note(text: Self.notReportedText)
                }
            }
        }
        .accessibilityIdentifier("rf2ksPage")
    }

    private func antennas(_ rfKit: AccessoriesModel.RfKit) -> some View {
        let labels = model.records?.rfKitLabels ?? ["", "", "", ""]
        let reasons = (1...4).map { model.rfKitAntennaReason(Int64($0)) }
        // Each reason once, in the antennas' order.
        var shown: [String] = []
        for reason in reasons.compactMap({ $0 }) where !shown.contains(reason) {
            shown.append(reason)
        }
        return VStack(alignment: .leading, spacing: 6) {
            AccessoryChrome.Caption(text: "Antenna:")
            HStack(spacing: 4) {
                ForEach(1...4, id: \.self) { (port: Int) in
                    AccessoryChrome.ChoiceButton(label: "ANT \(port)", detail: labels[port - 1],
                                                 lit: Self.antennaLit(rfKit, port: port),
                                                 enabled: reasons[port - 1] == nil) {
                        model.setRfKitAntenna(Int64(port))
                    }
                    .accessibilityHint(reasons[port - 1] ?? "")
                    .accessibilityIdentifier("rfkitAntenna\(port)")
                }
            }
            ForEach(shown, id: \.self) { reason in
                AccessoryChrome.Note(text: reason)
            }
        }
    }

    /// Whether the internal ANT `port` button (1 to 4) is lit. The Core
    /// reports the amp's own antenna number, which counts from 1 (0 is none).
    static func antennaLit(_ rfKit: AccessoriesModel.RfKit, port: Int) -> Bool {
        !rfKit.activeAntennaExternal && rfKit.activeAntenna == Int64(port)
    }

    /// "Fwd 0 W · SWR 1.00 · 53.2 V · 0.0 A".
    static func readings(_ rfKit: AccessoriesModel.RfKit) -> String {
        [String(format: "Fwd %.0f W", rfKit.forwardW), String(format: "SWR %.2f", rfKit.swr),
         String(format: "%.1f V", rfKit.volts), String(format: "%.1f A", rfKit.amps)]
            .joined(separator: " \u{00B7} ")
    }

    /// The amp's tuner mode, in its own word.
    static func tunerWord(_ mode: AccessoriesModel.TunerMode) -> String {
        switch mode {
        case .bypass:
            return "BYPASS"
        case .manual:
            return "MANUAL"
        case .autoTuning:
            return "AUTO TUNING"
        case .auto:
            return "AUTO"
        case .unknown:
            return "-"
        }
    }
}
