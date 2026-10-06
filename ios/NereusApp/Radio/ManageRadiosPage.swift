// NereusSDR for iOS: Manage Radios: the radios the Core can see, with Choose, Forget and Scan again
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Manage Radios (spec section 5.2 item 5, the board's "Which radio the
/// Core connects to"): the radios the Core can see, the one it runs marked
/// In use, each with its name, model, protocol, address and MAC. Choose
/// makes another radio the Core's, after a question here; Forget removes one
/// the Core does not run, after a question; Scan again looks again; Change
/// model, on each radio, offers the models the Core lists for it. While
/// they cannot run the buttons stay, greyed, with the reason once under the
/// list; the Core's refusal shows in its own words. The Radio tab opens and
/// closes its model as the page comes on and leaves the screen.
struct ManageRadiosPage: View {
    @ObservedObject var model: ManageRadiosModel
    @State private var choosing: StationRadio?
    @State private var forgetting: StationRadio?

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let waiting = model.waiting {
                AccessoryChrome.Alert(text: waiting)
                    .accessibilityIdentifier("manageRadios.waiting")
            }
            if let refusal = model.refusal {
                AccessoryChrome.Refusal(text: refusal) { model.clearRefusal() }
                    .accessibilityIdentifier("manageRadios.refusal")
            }
            if let note = model.note {
                AccessoryChrome.Note(text: note)
                    .accessibilityIdentifier("manageRadios.note")
            }
            AccessoryChrome.Caption(text: "Radios the Core can see")
            if model.radios.isEmpty {
                AccessoryChrome.Card {
                    AccessoryChrome.Note(text: model.listNote ?? "The Core cannot see a radio on its network.")
                        .accessibilityIdentifier("manageRadios.listNote")
                }
            } else {
                ForEach(model.radios) { radio in
                    card(radio)
                }
            }
            button("Scan again", identifier: "manageRadios.rescan") { model.rescan() }
            if model.actionReason == nil, !model.namesModels, !model.radios.isEmpty {
                // Said once for every radio: the Core does not list the models.
                AccessoryChrome.Note(text: "Change model: \(ManageRadiosModel.olderModelsReason)")
                    .accessibilityIdentifier("manageRadios.changeModelReason")
            }
            if let reason = model.actionReason {
                AccessoryChrome.Note(text: reason)
                    .accessibilityIdentifier("manageRadios.reason")
            }
        }
        .confirmationDialog(choosing.map { "Choose \(Self.name($0))?" } ?? "", isPresented: Binding(
            get: { choosing != nil }, set: { if !$0 { choosing = nil } }), titleVisibility: .visible) {
            Button("Choose") {
                if let radio = choosing { model.choose(radio) }
                choosing = nil
            }
            Button("Cancel", role: .cancel) { choosing = nil }
        } message: {
            Text("The Core switches to this radio. Every device on the Core reconnects, this phone too.")
        }
        .confirmationDialog(forgetting.map { "Forget \(Self.name($0))?" } ?? "", isPresented: Binding(
            get: { forgetting != nil }, set: { if !$0 { forgetting = nil } }), titleVisibility: .visible) {
            Button("Forget", role: .destructive) {
                if let radio = forgetting { model.forget(radio) }
                forgetting = nil
            }
            Button("Cancel", role: .cancel) { forgetting = nil }
        } message: {
            Text("The Core forgets this radio and the model saved for it. A scan lists it again while it is on the network.")
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("manageRadiosPage")
    }

    /// The radio's own name, else its MAC.
    static func name(_ radio: StationRadio) -> String {
        radio.name.isEmpty ? radio.mac : radio.name
    }

    private func card(_ radio: StationRadio) -> some View {
        AccessoryChrome.Card {
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Text(Self.name(radio))
                    .font(.system(size: 15, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.textBright)
                    .lineLimit(1)
                    .truncationMode(.middle)
                Spacer(minLength: 8)
                if radio.inUse {
                    Text("In use")
                        .font(.system(size: 11, weight: .bold))
                        .foregroundStyle(ChromeColours.buttonOnGreenText)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 3)
                        .background(ChromeColours.buttonOnGreen, in: RoundedRectangle(cornerRadius: 3))
                        .accessibilityIdentifier("manageRadios.\(radio.mac).inUse")
                }
            }
            AccessoryChrome.ValueRow(label: "Model", value: model.modelText(radio))
            AccessoryChrome.ValueRow(label: "Protocol", value: radio.protocolVersion.map(String.init)
                ?? RadioAtAGlance.unavailableText)
            AccessoryChrome.ValueRow(label: "IP address", value: radio.address.isEmpty
                ? RadioAtAGlance.unavailableText : radio.address)
            AccessoryChrome.ValueRow(label: "MAC", value: radio.mac)
            HStack(spacing: 8) {
                if !radio.inUse {
                    button("Choose", identifier: "manageRadios.\(radio.mac).choose") { choosing = radio }
                    button("Forget", identifier: "manageRadios.\(radio.mac).forget") { forgetting = radio }
                }
                changeModel(radio)
            }
            if model.actionReason == nil, model.namesModels, let reason = model.changeModelReason(radio) {
                // This radio's own reason; the reasons every radio shares show once under the list.
                AccessoryChrome.Note(text: "Change model: \(reason)")
                    .accessibilityIdentifier("manageRadios.\(radio.mac).changeModelReason")
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("manageRadios.\(radio.mac)")
    }

    /// Change model: the models the Core lists for this radio, the one it
    /// runs as ticked; greyed, with the reason, when it cannot change.
    private func changeModel(_ radio: StationRadio) -> some View {
        let reason = model.changeModelReason(radio)
        let enabled = reason == nil && !model.busy
        return Menu {
            ForEach(model.modelChoices(radio), id: \.self) { choice in
                Button {
                    model.setModel(radio, choice)
                } label: {
                    if choice.model == radio.model {
                        Label(choice.label, systemImage: "checkmark")
                    } else {
                        Text(choice.label)
                    }
                }
            }
        } label: {
            let look = AccessoryChrome.ChoiceButton.look(lit: false, enabled: enabled)
            Text("Change model")
                .font(.system(size: 12, weight: .bold))
                .foregroundStyle(look.text)
                .frame(maxWidth: .infinity, minHeight: 40)
                .background(look.fill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(look.border, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .disabled(!enabled)
        .accessibilityLabel("Change model")
        .accessibilityValue(model.modelText(radio))
        .accessibilityHint(reason ?? "")
        .accessibilityIdentifier("manageRadios.\(radio.mac).changeModel")
    }

    private func button(_ label: String, identifier: String, action: @escaping () -> Void) -> some View {
        let enabled = model.actionReason == nil && !model.busy
        return AccessoryChrome.ChoiceButton(label: label, lit: false, enabled: enabled, action: action)
            .accessibilityHint(model.actionReason ?? "")
            .accessibilityIdentifier(identifier)
    }
}
