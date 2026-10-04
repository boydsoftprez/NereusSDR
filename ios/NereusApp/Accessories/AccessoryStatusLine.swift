// NereusSDR for iOS: an accessory's one-line status, and its row on the Radio tab that opens its page
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// One accessory on the Radio tab (spec section 5.2 item 5, section 5.4
/// item 1): its name over a one-line status, with a chevron; a tap opens
/// its page. The status says where the device is: the Power Genius and the
/// Tuner Genius on the station's network, the RF2K-S reached through the
/// Core (the Core reads it over its web interface).
/// The same words head each page.
struct AccessoryStatusLine: View {
    let device: AccessoriesModel.Device
    let status: String
    let open: () -> Void

    static let onStationNetwork = "on the station's network"
    static let throughCore = "through the Core"

    var body: some View {
        Button(action: open) {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(device.title)
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                    Text(status)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(2)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Image(systemName: "chevron.right")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(ChromeColours.textFaint)
            }
            .padding(12)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("accessory.\(device.rawValue)")
    }

    // MARK: The words

    static let notReported = "Not reported by this Core"
    static let switchedOff = "Switched off at the Core"
    static let notSetUp = "Not set up"

    /// `device`'s status line, from the model's latest reading: where it
    /// stands when it is not set up, else its state and where it is.
    static func status(_ device: AccessoriesModel.Device, in model: AccessoriesModel) -> String {
        switch model.standing(device) {
        case .notReported:
            return notReported
        case .switchedOff:
            return switchedOff
        case .notSetUp:
            return notSetUp
        case .setUp:
            break
        }
        switch device {
        case .powerGenius:
            return model.powerGenius.map(powerGenius) ?? ""
        case .tunerGenius:
            return model.tunerGenius.map(tunerGenius) ?? ""
        case .rfKit:
            return model.rfKit.map(rfKit) ?? ""
        }
    }

    static func powerGenius(_ amp: AccessoriesModel.PowerGenius) -> String {
        "\(phaseWords(amp.link) ?? ampState(amp)) \u{00B7} \(onStationNetwork)"
    }

    static func tunerGenius(_ tuner: AccessoriesModel.TunerGenius) -> String {
        "\(phaseWords(tuner.link) ?? tunerState(tuner)) \u{00B7} \(onStationNetwork)"
    }

    static func rfKit(_ rfKit: AccessoriesModel.RfKit) -> String {
        "\(phaseWords(rfKit.link) ?? (rfKit.operate ? "Operating" : "Standby")) \u{00B7} \(throughCore)"
    }

    /// The connection's words while the Core is not connected to the
    /// device; nil once it is.
    static func phaseWords(_ link: AccessoriesModel.Link) -> String? {
        switch link.phase {
        case .connected:
            return nil
        case .disabled:
            return switchedOff
        case .disconnected, .error:
            return "Not connected"
        case .discovering:
            return "Looking for it"
        case .connecting:
            return "Connecting"
        case .identifying:
            return "Checking it"
        case .retrying:
            return "Trying again"
        }
    }

    /// The Power Genius's state in words.
    static func ampState(_ amp: AccessoriesModel.PowerGenius) -> String {
        switch amp.state {
        case .powerUp:
            return "Powering up"
        case .standby:
            return "Standby"
        case .idle, .operate:
            return "Operating"
        case .transmitA, .transmitB:
            return "Transmitting"
        case .fault:
            return "Fault"
        case .unknown:
            return amp.operate ? "Operating" : "Standby"
        }
    }

    static func tunerState(_ tuner: AccessoriesModel.TunerGenius) -> String {
        if tuner.tuning {
            return "Tuning"
        }
        if tuner.bypass {
            return "Bypass"
        }
        return tuner.operate ? "Operating" : "Standby"
    }

    /// The Power Genius's band-follow line (document, Band follow).
    static func ampBand(_ amp: AccessoriesModel.PowerGenius, bandLabel: String?) -> String {
        switch amp.bandFollow {
        case .following:
            return bandLabel.map { "\($0), from the radio" } ?? "From the radio"
        case .waiting:
            return "Waiting for the Power Genius to pair with the radio"
        case .off, .thisComputerOnly:
            return "Off while the Power Genius is not connected"
        }
    }

    /// The RF2K-S's band-follow line (document, Band follow); nil while it follows.
    static func rfKitBandFollow(_ rfKit: AccessoriesModel.RfKit) -> String? {
        switch rfKit.bandFollow {
        case .following:
            return nil
        case .waiting:
            return "Band follow: enter \(rfKit.bandFollowAddress), port \(rfKit.bandFollowPort) "
                + "as the TCI server on the amplifier."
        case .off:
            return "Band follow: off. Turn on the TCI server so the amplifier can follow the radio."
        case .thisComputerOnly:
            return "Band follow: the TCI server accepts only apps on its own computer, so the amplifier cannot reach it."
        }
    }

    /// The RF2K-S page's header line: reached through the Core, and the
    /// band from the radio while it follows.
    static func rfKitHeader(_ rfKit: AccessoriesModel.RfKit) -> String {
        var parts = ["Through the Core"]
        if let words = phaseWords(rfKit.link) {
            parts.append(words.lowercased())
        } else if rfKit.bandFollow == .following {
            parts.append("band from the radio")
        }
        return parts.joined(separator: " \u{00B7} ")
    }

    /// The transmit interlock in the board's words: its mode as the title,
    /// then what it holds back, the SWR gate and its grace time.
    static func interlockTitle(_ interlock: AccessoriesModel.Interlock) -> String {
        switch interlock.mode {
        case 2:
            return "TX interlock: Block"
        case 1:
            return "TX interlock: Warn"
        default:
            return "TX interlock: Off"
        }
    }

    static func interlockSummary(_ interlock: AccessoriesModel.Interlock) -> String {
        let first: String
        switch interlock.mode {
        case 2:
            first = "Transmit is refused while the amp is in STANDBY or FAULT."
        case 1:
            first = "Transmit goes ahead with a warning while the amp is in STANDBY or FAULT."
        default:
            return "The interlock never holds back transmit."
        }
        guard interlock.swrGateEnabled else {
            return first + " No SWR gate."
        }
        let gate = String(format: "%.1f", interlock.swrGateMax)
        guard interlock.graceMs > 0 else {
            return first + " SWR gate at \(gate)."
        }
        let seconds = Double(interlock.graceMs) / 1000
        let grace = seconds == seconds.rounded() ? String(Int(seconds)) : String(format: "%.1f", seconds)
        return first + " SWR gate at \(gate), with a \(grace) second grace after OPERATE."
    }
}
