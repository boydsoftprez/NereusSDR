// NereusSDR for iOS: RECEIVER TAKEN over the band: who took it and when, the slice kept, and Take it back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Your receiver taken (D49, spec section 5.8 item 9, picture 22): another
/// device took the receiver this band's slices were on, and they closed.
/// The Core's words say who, the time is this phone's own, and the slices'
/// frequencies and settings wait on the Core. The connection stays up.
/// Take it back sends `notice.takeBack`, and the Core asks the same
/// question the other way; the cover goes when a slice is back.
struct ReceiverTakenOverlay: View {
    @ObservedObject var devices: SeveralDevicesClient
    let notice: SeveralDevicesClient.ReceivedNotice

    var body: some View {
        VStack(spacing: 12) {
            Text("RECEIVER TAKEN")
                .font(.system(size: 24, weight: .heavy))
                .kerning(4)
                .foregroundStyle(ChromeColours.noticeInfo)
                .accessibilityAddTraits(.isHeader)
            Text(notice.notice.reason)
                .font(.system(size: 17, weight: .bold))
                .foregroundStyle(ChromeColours.textBright)
                .fixedSize(horizontal: false, vertical: true)
            Text(Self.body(notice))
                .font(.system(size: 14))
                .foregroundStyle(ChromeColours.textDim)
                .lineSpacing(3)
                .frame(maxWidth: 320)
                .fixedSize(horizontal: false, vertical: true)
            if notice.notice.takeBack {
                Button {
                    Task { await devices.takeBack(notice.id) }
                } label: {
                    Text(notice.takingBack ? "Asking first\u{2026}" : "Take it back")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 22)
                        .frame(height: 44)
                        .background(ChromeColours.buttonOnBlue, in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4)
                            .strokeBorder(ChromeColours.buttonOnBlueBorder, lineWidth: 1))
                }
                .buttonStyle(.plain)
                .disabled(notice.takingBack)
                .padding(.top, 6)
                .accessibilityIdentifier("takeBack")
            }
            if let refusal = notice.takeBackRefusal {
                ConfirmationSheetChrome.Refusal(text: refusal)
            }
        }
        .multilineTextAlignment(.center)
        .padding(.horizontal, 26)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color(red: 10 / 255, green: 12 / 255, blue: 20 / 255).opacity(0.784))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("receiverTakenCover")
    }

    /// "Slice A closed on this phone at 19:54. Its frequency and settings
    /// are kept, so taking the receiver back puts it where it was."
    static func body(_ received: SeveralDevicesClient.ReceivedNotice) -> String {
        let letters = received.notice.slices.map(\.letter)
        let time = SeveralDevicesWords.timeOfDay(received.happened)
        guard letters.count > 1 else {
            let letter = letters.first.map { "Slice \($0)" } ?? "Your slice"
            return "\(letter) closed on this phone at \(time). Its frequency and settings are kept, "
                + "so taking the receiver back puts it where it was."
        }
        return "Slices \(SeveralDevicesWords.letters(letters)) closed on this phone at \(time). Their frequencies and "
            + "settings are kept, so taking the receiver back puts them where they were."
    }
}
