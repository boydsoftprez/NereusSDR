// NereusSDR for iOS: the Modes tab's front end: ATT, S-ATT or A-ATT, preamp, step attenuator, RX1 preamp, antennas and RX bypass
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Front end section (D17): ATT, S-ATT or A-ATT as the Core's
/// `stepAtt` holds it (the desktop RX applet's label, the way it is set in
/// its Setup), the preamp items in ATT and the step attenuator in S-ATT
/// and A-ATT (the other greyed with its reason), the second ADC's preamp,
/// the RX antennas and receive-only inputs, the TX antennas, and RX bypass
/// on transmit (the desktop flag's BYPS). Every list and range is the
/// Core's. A control that cannot run is greyed with its reason below; one
/// for hardware the radio lacks (the second ADC's preamp, the RX bypass
/// relay) is left out, as on the desktop.
struct FrontEndSection: View {
    @ObservedObject var model: ModesTabModel

    var body: some View {
        ModesChrome.section("Front end") {
            ModesChrome.caption("Attenuator:")
            ModesChrome.grid(columns: 3) {
                ForEach(ModesTabModel.AttenuatorWay.allCases) { way in
                    PanelButton(label: way.rawValue, lit: model.attenuatorWay == way, style: .blue,
                                disabled: model.attenuatorWayReason != nil || model.attenuatorWay == nil) {
                        model.selectAttenuatorWay(way)
                    }
                    .accessibilityHint(model.attenuatorWayReason ?? Self.wayHint(way))
                    .accessibilityIdentifier("modesAtt" + way.rawValue)
                }
            }
            if let reason = model.attenuatorWayReason {
                ModesChrome.note("Attenuator: \(reason)")
            }
            // With no preamp choices the note alone names the row, so
            // "Preamp" is read once.
            if Self.preampCaptionShown(model) {
                ModesChrome.caption("Preamp:")
                ModesChrome.grid(columns: 4) {
                    ForEach(model.preamp) { item in
                        PanelButton(label: item.label, lit: item.lit, style: .blue,
                                    disabled: model.preampReason != nil) {
                            model.selectPreamp(item.id)
                        }
                        .accessibilityHint(model.preampReason ?? "")
                    }
                }
            }
            if let reason = model.preampReason {
                ModesChrome.note(Self.preampNote(reason))
            }
            HStack(spacing: 6) {
                ModesChrome.label("Step att")
                ModesChrome.arrow(left: true, accessibility: "Less attenuation", disabled: !attenuatorLive) {
                    model.stepAttenuator(up: false)
                }
                ValueField(text: model.attenuationDb.map { "\(ValuePadModel.text($0)) dB" } ?? "",
                           accessibility: "Step attenuator", disabled: !attenuatorLive, grow: true) {
                    model.openAttenuatorPad()
                }
                .accessibilityIdentifier("modesStepAtt")
                ModesChrome.arrow(left: false, accessibility: "More attenuation", disabled: !attenuatorLive) {
                    model.stepAttenuator(up: true)
                }
            }
            if let reason = model.attenuatorReason {
                ModesChrome.note("Step att: \(reason)")
            }
            // The second ADC's preamp and RX bypass, each only on a radio
            // that has it (the Core's catalogue says which).
            if model.rx1PreampPresent || model.bypassPresent {
                HStack(spacing: 6) {
                    if model.rx1PreampPresent {
                        PanelButton(label: "RX1 preamp", lit: model.rx1Preamp == true, style: .dsp,
                                    disabled: model.rx1PreampReason != nil || model.rx1Preamp == nil) {
                            model.toggleRx1Preamp()
                        }
                        .frame(width: 110)
                        .accessibilityHint(model.rx1PreampReason ?? "The second receiver input's preamp")
                        .accessibilityIdentifier("modesRx1Preamp")
                    }
                    if model.bypassPresent {
                        PanelButton(label: "RX bypass", lit: model.bypass == true, style: .dsp,
                                    disabled: model.bypassReason != nil || model.bypass == nil) {
                            model.toggleBypass()
                        }
                        .frame(width: 110)
                        .accessibilityLabel("RX bypass on transmit")
                        .accessibilityHint(model.bypassReason ?? Self.bypassHint)
                        .accessibilityIdentifier("modesBypass")
                    }
                    Spacer(minLength: 0)
                }
            }
            if let reason = model.rx1PreampReason {
                ModesChrome.note("RX1 preamp: \(reason)")
            }
            if let reason = model.bypassReason {
                ModesChrome.note("RX bypass: \(reason)")
            }
            ModesChrome.caption("RX antenna:")
            antennaGrid(model.rxAntennas, reason: model.rxAntennaReason, style: .blue, prefix: "modesRx") {
                model.selectRxAntenna($0)
            }
            if !model.rxOnlyInputs.isEmpty {
                antennaGrid(model.rxOnlyInputs, reason: model.rxAntennaReason, style: .blue, prefix: "modesRx") {
                    model.selectRxAntenna($0)
                }
            }
            if let reason = model.rxAntennaReason {
                ModesChrome.note("RX antenna: \(reason)")
            }
            ModesChrome.caption("TX antenna:")
            antennaGrid(model.txAntennas, reason: model.txAntennaReason, style: .red, prefix: "modesTx") {
                model.selectTxAntenna($0)
            }
            if let reason = model.txAntennaReason {
                ModesChrome.note("TX antenna: \(reason)")
            }
        }
    }

    /// The "Preamp:" caption and its buttons show unless there are no
    /// choices and the note says why: the note then names the row alone.
    static func preampCaptionShown(_ model: ModesTabModel) -> Bool {
        !model.preamp.isEmpty || model.preampReason == nil
    }

    /// The preamp row's note: its label once, then the reason.
    static func preampNote(_ reason: String) -> String {
        "Preamp: \(reason)"
    }

    /// The desktop flag's words for BYPS, as a hint.
    static let bypassHint = "Routes the receive path through the bypass relay while transmitting."

    static func wayHint(_ way: ModesTabModel.AttenuatorWay) -> String {
        switch way {
        case .att:
            return "The preamp choices, with the step attenuator off"
        case .stepAtt:
            return "The step attenuator, set by hand"
        case .autoAtt:
            return "The step attenuator, set automatically on an overload"
        }
    }

    private var attenuatorLive: Bool {
        model.attenuatorReason == nil && model.attenuationDb != nil && model.attenuatorRange != nil
    }

    private func antennaGrid(_ antennas: [ModesTabModel.Antenna], reason: String?, style: PanelButton.Style,
                             prefix: String, select: @escaping (ModesTabModel.Antenna) -> Void) -> some View {
        ModesChrome.grid(columns: 3) {
            ForEach(antennas) { antenna in
                PanelButton(label: antenna.label, lit: antenna.lit, style: style, disabled: reason != nil) {
                    select(antenna)
                }
                .accessibilityHint(reason ?? "")
                .accessibilityIdentifier(prefix + antenna.label)
            }
        }
    }
}
