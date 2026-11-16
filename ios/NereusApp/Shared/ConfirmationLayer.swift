// NereusSDR for iOS: the Core's open question over the screen, in the sheet its kind asks for
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The Core's open question (`confirm.request`) over the screen, with the
/// screen dimmed behind it: a take in ``TakeReceiverSheet``, a pan move in
/// ``MoveSharedReceiverSheet``, a shared change in ``SharedChangeSheet``,
/// and taking transmit in ``TakeTransmitSheet``, which also asks the
/// phone's own question before it takes transmit (`take`).
/// A tap on the dimmed screen cancels, as Cancel does, unless an answer is
/// on its way. Without `take` taking transmit is not asked here.
/// ``ConfirmationWindowAnchor`` shows it over whatever screen is showing.
struct ConfirmationLayer: View {
    @ObservedObject var devices: SeveralDevicesClient
    let modeLabel: (Int) -> String
    var kindOf: (String) -> String = { _ in "" }
    /// The phone's own question after a region edit, once the Core has none
    /// open. Observed here, not only by the window's content: the window hides
    /// when it closes, and with nothing else changed this body would not run
    /// again, so the next question would redraw the last one's words.
    @ObservedObject var bandStackRegion: BandStackRegionPrompt
    var take: TransmitTakeModel? = nil

    var body: some View {
        if let take, let asked = take.asked {
            dimmed {
                if take.answering != .taking {
                    take.cancel()
                }
            } sheet: {
                TakeTransmitSheet(take: take, devices: devices, question: nil)
                    .id("take-\(asked.id)")
            }
        } else if let question = devices.question, Self.shows(question.kind, takesTransmit: take != nil) {
            dimmed {
                if devices.answering == .idle {
                    if let take, question.kind == .takeTransmit {
                        take.cancelCore()
                    } else {
                        devices.cancel()
                    }
                }
            } sheet: {
                sheet(question)
                    .id(question.id)
            }
        } else if let asked = bandStackRegion.asked {
            dimmed {
                bandStackRegion.keep()
            } sheet: {
                BandStackRegionSheet(prompt: bandStackRegion, region: asked.region)
                    .id("bandStackRegion-\(asked.id)")
            }
        }
    }

    private func dimmed<Sheet: View>(cancel: @escaping () -> Void,
                                     @ViewBuilder sheet: () -> Sheet) -> some View {
        ZStack(alignment: .bottom) {
            Color.black.opacity(0.45)
                .contentShape(Rectangle())
                .onTapGesture { cancel() }
                .accessibilityLabel("Cancel")
                .accessibilityAddTraits(.isButton)
            sheet()
        }
        .transition(.opacity)
    }

    /// The kinds this layer asks; taking transmit only where the phone
    /// takes transmit (``TransmitTakeModel``).
    static func shows(_ kind: SeveralDevices.QuestionKind, takesTransmit: Bool) -> Bool {
        switch kind {
        case .takeReceiver, .takeSlice, .panMove, .sharedSetting:
            return true
        case .takeTransmit:
            return takesTransmit
        case .other:
            return false
        }
    }

    @ViewBuilder
    private func sheet(_ question: SeveralDevices.Question) -> some View {
        switch question.kind {
        case .takeReceiver, .takeSlice:
            TakeReceiverSheet(devices: devices, question: question, modeLabel: modeLabel)
        case .panMove:
            MoveSharedReceiverSheet(devices: devices, question: question, modeLabel: modeLabel, kindOf: kindOf)
        case .takeTransmit:
            if let take {
                TakeTransmitSheet(take: take, devices: devices, question: question)
            }
        default:
            SharedChangeSheet(devices: devices, question: question, modeLabel: modeLabel, kindOf: kindOf)
        }
    }
}
