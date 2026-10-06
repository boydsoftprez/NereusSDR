// NereusSDR for iOS: Setup's PTT buttons page: the buttons that toggle PTT, a locked phone, and the Core's transmit time-out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Setup, Transmit, PTT buttons (R-IOS-15, R-IOS-21, spec section 5.5 items
/// 6 and 14, pictures 15 and 17), in the board's words. The three hardware
/// buttons and "Buttons key a locked phone" are drawn in their places, but
/// the phone keys only from the PTT on the screen, so each is greyed with
/// what the phone does, and nothing is kept for them. The Transmit
/// time-out group is the Core's and works: it writes the Core's setting,
/// which every device and the desktop share.
struct PttButtonsPage: View {
    @ObservedObject var timeOut: TransmitTimeOutModel
    @ObservedObject private var settings: SettingsProxyClient

    init(timeOut: TransmitTimeOutModel) {
        self.timeOut = timeOut
        settings = timeOut.settings
    }

    static let buttonsReason = "Only the PTT on the screen keys the radio from this phone. These buttons don't key it."
    static let lockedReason = "A locked phone can't be keyed. Locking the phone ends a transmission."
    static let timeOutDetail = "Every transmission from a phone or iPad. The Core ends it and tells you why."
    static let footnote = "The time-out and the Core's watchdog apply to every transmission, however it was keyed. The desktop at the station keeps its own time-out, off by default."

    var body: some View {
        List {
            Section {
                SetupSwitchRow(title: "Headset button", detail: "AirPods or wired: press once to key, again to unkey",
                               isOn: .constant(false), enabled: false, id: "headsetButton")
                HStack(spacing: 12) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Bluetooth PTT button")
                            .font(.body.weight(.semibold))
                        Text("None paired")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    Spacer(minLength: 0)
                    Button("Pair") {}
                        .buttonStyle(.bordered)
                        .disabled(true)
                        .accessibilityIdentifier("bluetoothPttPair")
                }
                VStack(alignment: .leading, spacing: 2) {
                    Text("Action button")
                        .font(.body.weight(.semibold))
                    Text("Choose NereusSDR PTT in the iPhone's Settings, Action Button")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .foregroundStyle(.secondary)
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("actionButton")
            } header: {
                Text("Buttons that toggle PTT")
            } footer: {
                Text(Self.buttonsReason)
                    .accessibilityIdentifier("pttButtonsReason")
            }
            Section {
                SetupSwitchRow(title: "Buttons key a locked phone",
                               detail: "Through Apple's Push to Talk; locking still ends an on-screen transmission",
                               isOn: .constant(false), enabled: false, id: "keyLockedPhone")
            } header: {
                Text("With the phone locked")
            } footer: {
                Text(Self.lockedReason)
            }
            Section {
                HStack(spacing: 12) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Stop transmitting after")
                            .font(.body.weight(.semibold))
                        Text(Self.timeOutDetail)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    Spacer(minLength: 0)
                    Menu {
                        Button(TransmitTimeOutModel.text(nil)) {
                            Task { await timeOut.choose(nil) }
                        }
                        ForEach(timeOut.menu, id: \.self) { seconds in
                            Button(TransmitTimeOutModel.text(seconds)) {
                                Task { await timeOut.choose(seconds) }
                            }
                        }
                    } label: {
                        Text("\(TransmitTimeOutModel.text(timeOut.choice)) \u{25BE}")
                            .font(.footnote.weight(.bold))
                            .foregroundStyle(ChromeColours.text)
                            .padding(.horizontal, 10)
                            .frame(minHeight: 32)
                            .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4)
                                .strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                    }
                    .accessibilityLabel("Stop transmitting after")
                    .accessibilityValue(TransmitTimeOutModel.text(timeOut.choice))
                    .accessibilityIdentifier("transmitTimeOut")
                }
                if let problem = timeOut.problem {
                    Text(problem)
                        .font(.footnote)
                        .foregroundStyle(ChromeColours.revokeText)
                        .accessibilityIdentifier("transmitTimeOutProblem")
                }
            } header: {
                HStack(spacing: 6) {
                    Text("Transmit time-out")
                    SetupTagBadge(tag: .core)
                }
            } footer: {
                Text(Self.footnote)
            }
            #if PTT_PROBE
            // The button test (plan Task 65 step 1): debug copies only.
            Section {
                NavigationLink {
                    PttProbePage(probe: .shared)
                } label: {
                    Text("PTT button test")
                        .font(.body.weight(.semibold))
                }
                .accessibilityIdentifier("pttProbeEntry")
            } header: {
                Text("Debug copy only")
            } footer: {
                Text("Logs what each hardware button does with Apple's Push to Talk. It never keys the radio.")
            }
            #endif
        }
        .navigationTitle("PTT buttons")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .thisPhone)
            }
        }
    }
}
