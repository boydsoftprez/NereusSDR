// NereusSDR for iOS: the question before taking transmit, and the Take transmit button refusals and the TX panel offer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The question before this phone takes transmit (R-IOS-02, R-IOS-03), in
/// the several-devices sheet: who holds it, what it is doing, and what
/// taking it does, in the desktop's words (TakeTransmitDialog), with PTT
/// where the desktop says MOX. While the holder is on the air the button
/// is the desktop's red Unkey and take over.
///
/// It asks either the phone's own question (``TransmitTakeModel/asked``),
/// answered with `tx.take`, or the Core's `takeTransmit` question, answered
/// with `confirm.proceed` or `confirm.cancel`. A refusal shows the Core's
/// words as sent above one Close.
struct TakeTransmitSheet: View {
    @ObservedObject var take: TransmitTakeModel
    @ObservedObject var devices: SeveralDevicesClient
    /// The Core's question; nil asks the phone's own.
    let question: SeveralDevices.Question?

    var body: some View {
        let holder = question?.holder ?? take.asked?.holder
        ConfirmationSheetChrome.Sheet(kicker: Self.kicker(holder)) {
            if let holder {
                ConfirmationSheetChrome.Who(kind: holder.kind, name: Self.name(holder), detail: Self.detail(holder))
                ConfirmationSheetChrome.Note(text: Self.note(holder))
            } else if let question {
                ConfirmationSheetChrome.Note(text: question.reason)
            }
            answers(onAir: holder?.onAir ?? false)
        }
        .accessibilityIdentifier("takeTransmitSheet")
    }

    @ViewBuilder
    private func answers(onAir: Bool) -> some View {
        VStack(spacing: 8) {
            if let refusal {
                ConfirmationSheetChrome.Refusal(text: refusal)
                    .frame(maxWidth: .infinity, alignment: .leading)
                ConnectChrome.WideButton(title: "Close") { close() }
                    .accessibilityIdentifier("takeTransmitClose")
            } else {
                ConnectChrome.WideButton(title: busy ? Self.busyTitle(onAir: onAir) : Self.goTitle(onAir: onAir),
                                         look: onAir ? .stop : .go, busy: busy) {
                    go()
                }
                .accessibilityIdentifier("takeTransmitGo")
                ConnectChrome.WideButton(title: "Cancel") { cancel() }
                    .disabled(busy)
                    .accessibilityIdentifier("takeTransmitCancel")
            }
        }
    }

    private var busy: Bool {
        question == nil ? take.answering == .taking : devices.answering == .proceeding
    }

    private var refusal: String? {
        if question == nil {
            if case .refused(let reason) = take.answering {
                return reason
            }
        } else if case .refused(let reason) = devices.answering {
            return reason
        }
        return nil
    }

    private func go() {
        if question == nil {
            Task { await take.confirm() }
        } else {
            Task { await take.proceedCore() }
        }
    }

    /// Cancel: nothing changes.
    func cancel() {
        if question == nil {
            take.cancel()
        } else {
            take.cancelCore()
        }
    }

    private func close() {
        if question == nil {
            take.close()
        } else {
            take.closeCore()
        }
    }

    // MARK: The words

    /// "Take transmit from the MacBook?"
    static func kicker(_ holder: SeveralDevices.Holder?) -> String {
        guard let holder else {
            return "Take transmit?"
        }
        return "Take transmit from \(shortName(holder))?"
    }

    /// Unkey and take over while the holder is on the air, as the desktop's.
    static func goTitle(onAir: Bool) -> String {
        onAir ? "Unkey and take over" : "Take transmit"
    }

    static func busyTitle(onAir: Bool) -> String {
        onAir ? "Taking over\u{2026}" : "Taking transmit\u{2026}"
    }

    /// The holder's name in full: "MacBook Pro", or the radio's own PTT.
    static func name(_ holder: SeveralDevices.Holder) -> String {
        if holder.radioPtt {
            return "The radio\u{2019}s own PTT"
        }
        if !holder.name.isEmpty {
            return holder.name
        }
        return holder.shortName.isEmpty ? "Another device" : holder.shortName
    }

    /// The holder inside a sentence: "the MacBook", "the radio".
    static func shortName(_ holder: SeveralDevices.Holder) -> String {
        if holder.radioPtt {
            return "the radio"
        }
        let short = SeveralDevicesWords.shortName(holder.shortName, holder.name)
        return short.isEmpty ? "another device" : SeveralDevicesWords.the(short)
    }

    /// The holder starting a sentence: "The MacBook", "The radio".
    static func subject(_ holder: SeveralDevices.Holder) -> String {
        if holder.radioPtt {
            return "The radio"
        }
        let short = SeveralDevicesWords.shortName(holder.shortName, holder.name)
        return short.isEmpty ? "The other device" : SeveralDevicesWords.the(short, capitalised: true)
    }

    /// What the holder is doing: on the air, away, or holding transmit.
    static func detail(_ holder: SeveralDevices.Holder) -> String {
        if holder.onAir {
            return holder.transmittingForSeconds > 0
                ? "On the air for \(SeveralDevicesWords.duration(seconds: holder.transmittingForSeconds))"
                : "On the air"
        }
        if holder.state == .away {
            return holder.awayForSeconds > 0
                ? "Away for \(SeveralDevicesWords.duration(seconds: holder.awayForSeconds))" : "Away"
        }
        guard holder.lastActivitySeconds > 0 else {
            return "Has transmit"
        }
        return "Has transmit, last active \(SeveralDevicesWords.ago(seconds: holder.lastActivitySeconds))"
    }

    /// What taking it does, as the desktop's dialog says it.
    static func note(_ holder: SeveralDevices.Holder) -> String {
        let who = subject(holder)
        if holder.onAir {
            return "\(who) is on the air now. Taking over unkeys it first. Press PTT here when you want to transmit."
        }
        if holder.state == .away {
            return holder.awayForSeconds > 0
                ? "\(who) is away (for \(SeveralDevicesWords.duration(seconds: holder.awayForSeconds))). "
                    + "Nothing is on the air."
                : "\(who) is away. Nothing is on the air."
        }
        return "\(who) has the transmitter and is not on the air. Press PTT here when you want to transmit."
    }
}

/// Take transmit, where the Core's refusal offers it (`takeTransmit`) and
/// on the TX panel's holder line: the same take as a slice's TX button,
/// asked first while another device holds transmit. Red while the holder
/// is on the air, as the desktop's TX applet draws it. Where the Core
/// takes no `tx.take` from this phone it is still drawn, greyed, with the
/// reason under it in the phone's words.
struct TakeTransmitButton: View {
    @ObservedObject var take: TransmitTakeModel
    /// The slice transmit moves onto once taken, for a TX button's refusal.
    var sliceId: Int? = nil
    var identifier = "takeTransmit"
    /// Runs once the take has started: the refusal it answers closes. A
    /// take that did not start (transmit changing hands) leaves it up,
    /// with the reason it shows.
    var then: () -> Void = {}

    static let title = "Take transmit"

    /// The button takes a tap: the take is offered, none is on its way
    /// and no question about it is up.
    static func enabled(_ take: TransmitTakeModel) -> Bool {
        take.available && !take.inFlight && !take.questionUp
    }

    /// Why the button is greyed, shown under it; nil while offered.
    static func reason(_ take: TransmitTakeModel) -> String? {
        take.available ? nil : take.unavailableReason
    }

    var body: some View {
        // Greyed while a question about the take is up: it is answered there.
        let offered = take.available && !take.questionUp
        let red = offered && take.holderOnAir
        VStack(alignment: .leading, spacing: 4) {
            Button {
                if take.begin(sliceId: sliceId, offered: true) {
                    then()
                }
            } label: {
                HStack(spacing: 6) {
                    if take.inFlight {
                        ProgressView().tint(.white).controlSize(.small)
                    }
                    Text(Self.title)
                }
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(offered ? .white : ChromeColours.textDim)
                .padding(.horizontal, 12)
                .frame(minHeight: 32)
                .background(red ? ConnectChrome.WideButton.stopFill
                                : offered ? ChromeColours.buttonOnBlue : NoticeBanner.greyedFill,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(red ? ConnectChrome.WideButton.stopBorder
                                      : offered ? ChromeColours.buttonOnBlueBorder : NoticeBanner.greyedEdge,
                                  lineWidth: 1))
            }
            .buttonStyle(.plain)
            .disabled(!Self.enabled(take))
            .accessibilityHint(Self.reason(take) ?? "")
            .accessibilityIdentifier(identifier)
            if let why = Self.reason(take) {
                Text(why)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier(identifier + "Unavailable")
            }
        }
    }
}
