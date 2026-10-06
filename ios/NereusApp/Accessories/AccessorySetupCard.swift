// NereusSDR for iOS: the card at the top of an accessory's page while the Core has it switched off or not set up
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// What a page shows first while its accessory is switched off at the Core
/// or not set up (parity rows C4 and C5): where it stands in plain words,
/// the station's switch for it (the Power Genius and Tuner Genius switch,
/// or the RF-Kit switch), and a row to the page that sets it up (the
/// Core's address for it, Look for it, Connect), as the desktop's
/// Peripherals page offers them. A switch this Core can't change from a
/// phone is shown disabled with its reason. Nothing shows once it is set up.
struct AccessorySetupCard: View {
    @ObservedObject var model: AccessoriesModel
    let device: AccessoriesModel.Device
    let open: (AccessoriesSection.Route) -> Void

    var body: some View {
        let standing = model.standing(device)
        if standing == .switchedOff || standing == .notSetUp {
            let reason = model.stationSwitchReason(device)
            VStack(alignment: .leading, spacing: 6) {
                AccessoryChrome.Card {
                    Text(Self.headline(device, standing))
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("\(device.rawValue)SetupHeadline")
                    AccessoryFields.SwitchRow(label: Self.switchLabel(device), isOn: model.stationSwitch(device),
                                              enabled: reason == nil, identifier: "\(device.rawValue)SetupSwitch") {
                        model.setStationSwitch(device, $0)
                    }
                    if let reason {
                        AccessoryChrome.Note(text: reason)
                    }
                }
                AccessoryChrome.Rows {
                    AccessoryChrome.PageRow(title: "Set up", detail: Self.setUpDetail(device),
                                            identifier: "\(device.rawValue)SetUp") {
                        open(.advanced(device))
                    }
                }
            }
            .accessibilityElement(children: .contain)
            .accessibilityIdentifier("\(device.rawValue)SetupCard")
        }
    }

    /// "The Core has the Power Genius switched off." and the like.
    static func headline(_ device: AccessoriesModel.Device, _ standing: AccessoriesModel.Standing) -> String {
        let name: String
        switch device {
        case .powerGenius:
            name = "Power Genius"
        case .tunerGenius:
            name = "Tuner Genius"
        case .rfKit:
            name = "RF-Kit amplifier"
        }
        if standing == .switchedOff {
            return "The Core has the \(name) switched off. Switch it on here, then set it up."
        }
        return "The Core has no address for the \(name). Set it up to connect."
    }

    /// The switch's words, as the Advanced page writes them.
    static func switchLabel(_ device: AccessoriesModel.Device) -> String {
        device == .rfKit ? "RF-Kit amplifier on the Core" : "Power Genius and Tuner Genius on the Core"
    }

    /// What the Set up page holds.
    static func setUpDetail(_ device: AccessoriesModel.Device) -> String {
        device == .rfKit ? "Its address and Connect" : "Its address, Look for it and Connect"
    }
}
