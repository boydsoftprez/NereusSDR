// NereusSDR for iOS: what the Core told this phone about another device, over the band, with Take it back where the Core offers it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import NereusMirror
import SwiftUI

/// The Core's notices over the band (spec section 5.8 items 9 and 12,
/// section 5.9 items 8 to 11, pictures 22 and 23): the Core's words as it
/// sends them, the first sentence bold, and, for something another device
/// did, when it happened by this phone's clock. A notice the Core lets be
/// taken back carries Take it back, which asks the Core the same question
/// the other way; a tap anywhere else on it closes it. The newest one
/// shows. Taking transmit's notices are the transmit screens' own. The
/// Core's refusal of one of this phone's own slice writes shows the same
/// way, over any notice, until closed or until the Core takes the next.
///
/// Another device taking control of one of this phone's slices
/// (`controlTaken`, R-IOS-42) shows the Core's words and Take back, which
/// the Core runs as the take the other way. Where the Core cannot give it
/// back, Take back stays greyed with the Core's reason under it. A Take
/// back refused for good takes the card down and shows the Core's words
/// as a refusal.
///
/// A refusal that offers taking transmit (`takeTransmit`) carries Take
/// transmit, which runs the same take as the slice's TX button (`take`).
struct NoticeBanner: View {
    @ObservedObject var devices: SeveralDevicesClient
    /// The notices another layer shows (the RECEIVER TAKEN cover's).
    var excluded: Set<Int64> = []
    /// The Core's words for this phone's last refused slice write.
    var refusal: BandSlicesModel.Refusal? = nil
    var dismissRefusal: () -> Void = {}
    /// Take transmit, for a refusal that offers it.
    var take: TransmitTakeModel? = nil

    var body: some View {
        if let refusal {
            refusalCard(refusal)
                .transition(.opacity)
                .id("refusal-\(refusal.id)")
        } else if let ended = devices.endedTakeBack {
            refusalCard(BandSlicesModel.Refusal(id: -ended.id, text: ended.text, takeOver: true),
                        close: devices.dismissEndedTakeBack)
                .transition(.opacity)
                .id("ended-\(ended.id)")
        } else if let shown = devices.notices.last(where: { Self.shows($0.notice.kind) && !excluded.contains($0.id) }) {
            card(shown)
                .transition(.opacity)
                .id(shown.id)
        }
    }

    /// The kinds this banner tells; transmit's are the transmit screens'.
    static func shows(_ kind: SeveralDevices.NoticeKind) -> Bool {
        kind != .transmitTaken
    }

    /// Something that took from this phone is a warning; the rest inform.
    static func warns(_ kind: SeveralDevices.NoticeKind) -> Bool {
        switch kind {
        case .receiverTaken, .sliceTaken, .sliceClosed, .placeTaken, .slicesNotRestored, .transmitTaken:
            return true
        default:
            return false
        }
    }

    /// The Core's words, then when it happened for something another device did.
    static func text(_ received: SeveralDevicesClient.ReceivedNotice) -> String {
        let notice = received.notice
        // Taking control's notice is the Core's words alone, as the board draws it.
        guard notice.by != nil, notice.kind != .antennaKept, notice.kind != .controlTaken else {
            return notice.reason
        }
        return notice.reason + " At " + SeveralDevicesWords.timeOfDay(received.happened) + "."
    }

    private func card(_ received: SeveralDevicesClient.ReceivedNotice) -> some View {
        let warns = Self.warns(received.notice.kind)
        return VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .top, spacing: 10) {
                Self.dot(warns: warns)
                Self.words(Self.text(received))
                if received.notice.takeBack {
                    takeBackButton(received)
                } else if let why = devices.takeBackUnavailableReason(received) {
                    takeBackButton(received, unavailable: why)
                }
            }
            if let why = devices.takeBackUnavailableReason(received) {
                Text(why)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, 20)
                    .accessibilityIdentifier("takeBackUnavailable")
            }
            if let refusal = received.takeBackRefusal {
                ConfirmationSheetChrome.Refusal(text: refusal)
                    .padding(.leading, 20)
            }
        }
        .modifier(Chrome(identifier: "noticeBanner") { devices.dismiss(received.id) })
    }

    /// The Core's refusal of a slice write: its words as it sent them,
    /// the first sentence bold, closed by a tap.
    private func refusalCard(_ refusal: BandSlicesModel.Refusal, close: (() -> Void)? = nil) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .top, spacing: 10) {
                if refusal.takeOver {
                    // The board's take-over notice: amber dot, the Core's words plain.
                    Self.dot(warns: true)
                    Self.plainWords(refusal.text)
                } else {
                    Self.dot(warns: false)
                    Self.words(refusal.text)
                }
            }
            if refusal.fix == TxRefusalInfo.takeTransmit, let take {
                TakeTransmitButton(take: take, sliceId: refusal.sliceId, identifier: "sliceRefusalTakeTransmit",
                                   then: close ?? dismissRefusal)
                    .padding(.leading, 20)
            }
        }
        .modifier(Chrome(identifier: "sliceRefusal", close: close ?? dismissRefusal))
    }

    private static func dot(warns: Bool) -> some View {
        let dot = warns ? ChromeColours.noticeWarn : ChromeColours.noticeInfo
        return Circle()
            .fill(dot)
            .frame(width: 10, height: 10)
            .shadow(color: warns ? dot.opacity(0.6) : .clear, radius: 3)
            .padding(.top, 4)
    }

    private static func words(_ text: String) -> some View {
        styled(text)
            .font(.system(size: 13))
            .foregroundStyle(ChromeColours.text)
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// The Core's words as sent, no sentence bold (the board's take-over notices).
    static func plainWords(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 13))
            .foregroundStyle(ChromeColours.text)
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// The card's frame, and its close on a tap anywhere on it.
    private struct Chrome: ViewModifier {
        let identifier: String
        let close: () -> Void

        func body(content: Content) -> some View {
            content
                .padding(.vertical, 10)
                .padding(.horizontal, 12)
                .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                .contentShape(Rectangle())
                .onTapGesture { close() }
                .accessibilityElement(children: .contain)
                .accessibilityIdentifier(identifier)
                .accessibilityAction(named: "Close") { close() }
        }
    }

    private func takeBackButton(_ received: SeveralDevicesClient.ReceivedNotice,
                                unavailable: String? = nil) -> some View {
        let control = received.notice.kind == .controlTaken
        let letter = received.notice.slices.first?.letter ?? ""
        return Button {
            Task { await devices.takeBack(received.id) }
        } label: {
            Text(Self.takeBackTitle(control: control, taking: received.takingBack))
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(unavailable == nil ? .white : ChromeColours.textDim)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 14)
                .padding(.vertical, 4)
                .frame(minHeight: control ? 44 : 36)
                .background(unavailable == nil ? ChromeColours.buttonOnBlue : Self.greyedFill,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(unavailable == nil ? ChromeColours.buttonOnBlueBorder : Self.greyedEdge, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .disabled(received.takingBack || unavailable != nil)
        .accessibilityLabel(control && !letter.isEmpty ? "Take back slice \(letter)"
                                                         : Self.takeBackTitle(control: control, taking: false))
        .accessibilityHint(unavailable ?? "")
        .accessibilityIdentifier("takeBack")
    }

    /// Take it back's words: for control of a slice the board's Take back.
    static func takeBackTitle(control: Bool, taking: Bool) -> String {
        if control {
            return taking ? "Taking back\u{2026}" : "Take back"
        }
        return taking ? "Asking first\u{2026}" : "Take it back"
    }

    /// A greyed button, as the board's disabled Take control.
    static let greyedFill = Color(red: 0x1A / 255, green: 0x1A / 255, blue: 0x2A / 255)
    static let greyedEdge = Color(red: 0x2A / 255, green: 0x30 / 255, blue: 0x40 / 255)

    /// The first sentence in bold, as the board sets a notice.
    static func styled(_ text: String) -> Text {
        var styled = AttributedString(text)
        let first = text.range(of: ". ").map { String(text[..<$0.lowerBound]) + "." } ?? text
        if let range = styled.range(of: first) {
            styled[range].font = .system(size: 13, weight: .bold)
            styled[range].foregroundColor = ChromeColours.textBright
        }
        return Text(styled)
    }
}
