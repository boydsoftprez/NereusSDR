// NereusSDR for iOS: the trouble screens: the Core isn't answering, the Core needs updating, and an older Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit

/// The five trouble screens (spec section 5.3 item 14, picture 10), each
/// naming its cause, so the operator knows whether to walk to the radio,
/// the Core or the phone. Two rise as sheets over Your Cores
/// (``TroubleSheet``): the Core isn't answering, and the Core needs
/// updating. One sits over Tools (``OlderCoreNotice``): an older Core. The
/// radio off and this phone offline sit on the band (``LinkLostBanner``).
struct TroubleSheet: View {
    @ObservedObject var flow: ConnectionFlow
    let trouble: ConnectionFlow.Trouble

    /// Where the update guide is.
    static let updateGuide = URL(string: "https://nereussdr.com")

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Capsule()
                .fill(ConnectChrome.grab)
                .frame(width: 40, height: 5)
                .frame(maxWidth: .infinity)
            switch trouble {
            case .notAnswering(let core, let tried, let denied, let note):
                notAnswering(core: core, tried: tried, localNetworkDenied: denied, note: note)
            case .needsUpdating(let core, let coreSpeaks, let appSpeaks, let reason, let coreIsOlder, let gap):
                needsUpdating(core: core, coreSpeaks: coreSpeaks, appSpeaks: appSpeaks, reason: reason,
                              coreIsOlder: coreIsOlder, gap: gap)
            }
        }
        .padding(.horizontal, 18)
        .padding(.top, 8)
        .padding(.bottom, 20)
        .frame(maxWidth: .infinity)
        .background(ConnectChrome.sheetFill, in: UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14))
        .overlay(alignment: .top) {
            UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14)
                .stroke(ConnectChrome.sheetEdge, lineWidth: 1)
        }
        .shadow(color: .black.opacity(0.5), radius: 15, y: -10)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("troubleSheet")
    }

    // MARK: The Core isn't answering

    /// What the phone tried, place by place (this Wi-Fi, direct over the
    /// internet, by relay; spec section 5.3 item 14), and what to check.
    @ViewBuilder
    private func notAnswering(core: String, tried: [ConnectionFlow.TriedRow], localNetworkDenied: Bool,
                              note: String?) -> some View {
        kicker("\(core) isn\u{2019}t answering")
        triedBox(rows: tried.map { ($0.place, $0.result) })
        if let note {
            paragraph(note)
                .accessibilityIdentifier("troubleNote")
        }
        if localNetworkDenied {
            paragraph("NereusSDR isn\u{2019}t allowed to reach devices on this Wi-Fi, so it couldn\u{2019}t try your Core here. Turn on Local Network for NereusSDR in Settings, Privacy & Security.")
            ConnectChrome.WideButton(title: "Open Settings", look: .go) {
                if let settings = URL(string: UIApplication.openSettingsURLString) {
                    UIApplication.shared.open(settings)
                }
            }
            .accessibilityIdentifier("openSettings")
        } else {
            paragraph("Worth checking at home: is the Core\u{2019}s computer on and awake, is NereusSDR\u{2019}s Core switch on, and is the home internet up?")
            ConnectChrome.WideButton(title: "Try again", look: .go) {
                Task { await flow.tryAgain() }
            }
            .accessibilityIdentifier("troubleTryAgain")
        }
        ConnectChrome.WideButton(title: "Cancel") { flow.dismissTrouble() }
            .accessibilityIdentifier("troubleCancel")
    }

    // MARK: The Core needs updating

    @ViewBuilder
    private func needsUpdating(core: String, coreSpeaks: String, appSpeaks: String, reason: String?,
                               coreIsOlder: Bool, gap: Int?) -> some View {
        if coreIsOlder {
            kicker("\(core) needs updating")
            paragraph(Self.behindText(gap: gap))
        } else {
            kicker("This app needs updating")
            paragraph("This app is too far behind the Core\u{2019}s NereusSDR to talk. Nothing has changed on the Core.")
        }
        triedBox(rows: [("The Core speaks", coreSpeaks), ("This app speaks", appSpeaks)], dots: false)
        if let reason {
            paragraph(reason)
        }
        if coreIsOlder {
            paragraph("Update NereusSDR on the Core\u{2019}s computer, or on the small box, then connect again. Your settings stay on the Core.")
            ConnectChrome.WideButton(title: "How to update the Core", look: .go) {
                if let guide = Self.updateGuide {
                    UIApplication.shared.open(guide)
                }
            }
            .accessibilityIdentifier("howToUpdate")
        } else {
            paragraph("Update NereusSDR on this phone from the App Store, then connect again. Your settings stay on the Core.")
        }
        ConnectChrome.WideButton(title: "OK") { flow.dismissTrouble() }
            .accessibilityIdentifier("troubleOK")
    }

    /// "The Core's NereusSDR is two versions behind this app", when the gap is known.
    static func behindText(gap: Int?) -> String {
        let words = ["two", "three", "four", "five", "six", "seven", "eight", "nine"]
        guard let gap, gap >= 2 else {
            return "The Core\u{2019}s NereusSDR is too far behind this app to talk. Nothing has changed on the Core."
        }
        let count = gap <= 9 ? words[gap - 2] : String(gap)
        return "The Core\u{2019}s NereusSDR is \(count) versions behind this app, too far apart to talk. Nothing has changed on the Core."
    }

    // MARK: Parts

    private func kicker(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 19, weight: .heavy))
            .foregroundStyle(ChromeColours.textBright)
            .padding(.top, 4)
            .accessibilityAddTraits(.isHeader)
            .accessibilityIdentifier("troubleTitle")
    }

    private func paragraph(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 12))
            .foregroundStyle(ChromeColours.textDim)
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
    }

    private func triedBox(rows: [(String, String)], dots: Bool = true) -> some View {
        VStack(spacing: 8) {
            ForEach(Array(rows.enumerated()), id: \.offset) { _, row in
                HStack(alignment: .top) {
                    if dots {
                        Circle().fill(ConnectChrome.pillOff).frame(width: 7, height: 7).padding(.top, 4)
                    }
                    Text(row.0)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.text)
                        .fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 8)
                    Text(row.1)
                        .font(.system(size: 11, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                        .multilineTextAlignment(.trailing)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .accessibilityElement(children: .combine)
            }
        }
        .padding(12)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
    }
}

/// An older Core (D23, picture 10): connected, one link version behind
/// this app. Everything the Core can do works; each item it can't do yet
/// is greyed "Needs a newer Core" where it sits.
struct OlderCoreNotice: View {
    let core: String

    var body: some View {
        ConnectChrome.NoticeBox(title: "\(core) runs an older NereusSDR.",
                                text: "Everything works except the items marked here. Update the Core to get them.")
            .accessibilityIdentifier("olderCoreNotice")
    }
}
