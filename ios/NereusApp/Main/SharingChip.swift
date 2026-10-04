// NereusSDR for iOS: the Sharing chip on the band while the Core slows this phone's display, and its note
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import NereusMedia
import SwiftUI

/// While the Core holds this phone's band below the frames it asks for
/// because the Core is shared or busy (D54), the band's top left shows
/// "Sharing · <n> fps"; a tap opens a note saying why, in the words kept
/// on board v51, and who keeps their full band.
struct SharingChip: View {
    @ObservedObject var subscriber: BandSubscriber
    @ObservedObject var transmit: TransmitModel

    @State private var open = false

    var body: some View {
        if let fps = subscriber.sharingFps {
            VStack(alignment: .leading, spacing: 6) {
                Button {
                    open.toggle()
                } label: {
                    Text("Sharing \u{00B7} \(fps) fps")
                        .font(.system(size: 11, weight: .bold, design: .monospaced))
                        .foregroundStyle(ChromeColours.noticeWarn)
                        .padding(.horizontal, 8)
                        .frame(height: 22)
                        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 3))
                        .overlay(RoundedRectangle(cornerRadius: 3)
                            .strokeBorder(ChromeColours.noticeWarn.opacity(0.5), lineWidth: 1))
                }
                .buttonStyle(.plain)
                .accessibilityIdentifier("sharingChip")
                if open {
                    Text(Self.note(reason: subscriber.budget?.reason, holder: holder))
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.text)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(10)
                        .frame(maxWidth: 300, alignment: .leading)
                        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                        .onTapGesture { open = false }
                        .accessibilityIdentifier("sharingNote")
                }
            }
        }
    }

    /// The device that holds transmit, when it is not this phone.
    private var holder: String? {
        let report = transmit.report
        guard report.heldElsewhere else {
            return nil
        }
        return report.holderLabel
    }

    /// The note: why the band is slowed, then who keeps a full band.
    static func note(reason: DisplayQualityAllocator.Budget.Reason?, holder: String?) -> String {
        let why: String
        switch reason {
        case .sharedConnection?:
            why = "The Core's connection is full."
        default:
            why = "The Core is busy."
        }
        if let holder {
            return why + " " + holder + " has transmit, so it keeps its full band and sound."
        }
        return why + " The devices share it."
    }
}
