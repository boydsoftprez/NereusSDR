// NereusSDR for iOS: the Radio tab's accessories, each with its one-line status, and the pages they open
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The accessories on the Radio tab (spec section 5.2 item 5, section 5.4
/// item 1): the Power Genius, the Tuner Genius and the RF-Kit, always, each
/// with a one-line status, in one card; a tap on a row opens that
/// accessory's page. One the Core has switched off or not set up says so,
/// and its page switches it on or sets it up.
struct AccessoriesSection: View {
    /// A page under the Radio tab.
    enum Route: Hashable {
        case page(AccessoriesModel.Device)
        case faults(AccessoriesModel.Device)
        case advanced(AccessoriesModel.Device)
        case tuneMemory

        /// The page's title on the bar, and the next page's way back.
        var title: String {
            switch self {
            case .page(let device):
                return device.title
            case .faults:
                return "Fault history"
            case .advanced(.rfKit):
                return "RF-Kit settings"
            case .advanced:
                return "Advanced"
            case .tuneMemory:
                return "Tune memory"
            }
        }
    }

    @ObservedObject var model: AccessoriesModel
    let open: (Route) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            AccessoryChrome.Caption(text: "Accessories")
            AccessoryChrome.Rows {
                ForEach(Array(model.listed.enumerated()), id: \.element) { index, device in
                    if index > 0 {
                        AccessoryChrome.RowDivider()
                    }
                    AccessoryStatusLine(device: device, status: AccessoryStatusLine.status(device, in: model)) {
                        open(.page(device))
                    }
                }
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("radioAccessories")
    }
}
