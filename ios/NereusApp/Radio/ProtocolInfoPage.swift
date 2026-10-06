// NereusSDR for iOS: Protocol Info: the Core's radio, its model, protocol, firmware, MAC and network address
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Protocol Info (the desktop's Radio > Protocol Info, as a remote window
/// shows it): the radio the Core runs, by the name it reports, its model,
/// its protocol, its firmware, its MAC and its IP address, each as the
/// Core describes it, or unavailable with why. The values can be selected
/// and copied. It reads again as the Core changes them.
struct ProtocolInfoPage: View {
    @ObservedObject var model: RadioTabModel
    let now: () -> Int64

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            AccessoryChrome.Caption(text: "The Core's radio")
            let _ = model.revision
            RadioAtAGlanceSection.Lines(rows: model.protocolInfo(nowMilliseconds: now()), identifier: "protocolInfo")
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("protocolInfoPage")
    }
}
