// NereusSDR for iOS: what covers the band when the link is lost, the radio is off or the phone is offline, and back on the air
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import NereusMirror
import SwiftUI

/// The band's cover when something between the phone and the radio is
/// down, the desktop's DISCONNECTED overlay at phone size (spec section
/// 5.3 items 8, 9 and 14, section 7, pictures 07 and 10):
/// - the link lost while listening: LINK LOST, the try and its countdown,
///   and Cancel; after Cancel, Reconnect and Back to Cores;
/// - the radio off: DISCONNECTED, the Core can't reach the radio, the last
///   frame when the phone saw it go, and the Core's five-second retry;
/// - this phone offline: NO NETWORK, not the Core's fault, waiting for Wi-Fi
///   or cellular.
/// Back on the air, a short notice says so over the band.
struct LinkLostBanner: View {
    @ObservedObject var flow: ConnectionFlow
    /// The Core's state, to know whether it shares itself with other
    /// devices; nil where no mirror is at hand (the flow's own pictures).
    var mirror: MirrorStore?
    /// The Core's notices: back after 3 minutes says so itself.
    var devices: SeveralDevicesClient?

    /// How bright the band draws under a cover. The board fades the pan,
    /// its zoom and its flags to 0.4 under the cover's backing
    /// (`connecting-v5.html` `.is-lost .pan`), but that leaves about 9% of
    /// the band showing, enough for the scale's digits and the flags to
    /// read between the cover's words. The phone fades it much further:
    /// under the words the band's brightest parts keep about 1% of their
    /// contrast (the shot tests hold it under 2%), and elsewhere the band
    /// stays faintly there.
    static let bandOpacityUnderCover = 0.04

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .top) {
                if flow.offline {
                    cover(word: "NO NETWORK", quiet: true, lead: "This phone is offline.",
                          text: "Nothing is wrong at the Core. The phone reconnects as soon as Wi-Fi or cellular is back. If you were transmitting, the Core has already unkeyed.") {
                        strip(pill: ChromeColours.linkOffDot, text: Text("Waiting for Wi-Fi or cellular"))
                    }
                    .accessibilityIdentifier("offlineCover")
                } else if let lost = flow.linkLost {
                    cover(word: "LINK LOST", quiet: false, lead: Self.lostLead(lost), text: Self.lostText(lost)) {
                        lostStrip(lost)
                    }
                    .accessibilityIdentifier("linkLostCover")
                } else if let radio = flow.radioOff {
                    TimelineView(.periodic(from: .now, by: 1)) { context in
                        cover(word: "DISCONNECTED", quiet: false, lead: "The Core can\u{2019}t reach the radio.",
                              text: Self.radioText(radio, now: context.date)) {
                            strip(pill: ChromeColours.linkLost,
                                  text: Text(radio.waiting == nil ? Self.radioRetryText(radio, now: context.date)
                                                 : "Waiting for the Core\u{2019}s radio"))
                        }
                    }
                    .accessibilityIdentifier("radioOffCover")
                }
                if flow.backOnAir {
                    if let devices {
                        // Back after its 3 minutes, the Core's own notice says so instead.
                        UnlessBackAfterItsTime(devices: devices) { placedBackOnAir(proxy) }
                    } else {
                        placedBackOnAir(proxy)
                    }
                }
            }
            .frame(width: proxy.size.width, height: proxy.size.height, alignment: .top)
        }
        .animation(.easeOut(duration: 0.2), value: flow.backOnAir)
    }

    // MARK: The cover

    private func cover<Strip: View>(word: String, quiet: Bool, lead: String, text: String,
                                    @ViewBuilder strip: () -> Strip) -> some View {
        VStack(spacing: 12) {
            Text(word)
                .font(.system(size: 28, weight: .heavy))
                .kerning(6)
                .foregroundStyle(quiet ? ChromeColours.textDim : ChromeColours.linkLost)
                .accessibilityAddTraits(.isHeader)
            Text(lead)
                .font(.system(size: 15, weight: .bold))
                .foregroundStyle(ChromeColours.textBright)
            Text(text)
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.textDim)
                .lineSpacing(3)
                .frame(maxWidth: 300)
                .fixedSize(horizontal: false, vertical: true)
            strip()
                .padding(.top, 8)
        }
        .multilineTextAlignment(.center)
        .padding(.horizontal, 20)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color(red: 10 / 255, green: 12 / 255, blue: 20 / 255).opacity(0.784))
        .accessibilityElement(children: .contain)
    }

    private func strip<Buttons: View>(pill: Color, text: Text,
                                      @ViewBuilder buttons: () -> Buttons = { EmptyView() }) -> some View {
        HStack(spacing: 8) {
            Circle().fill(pill).frame(width: 12, height: 12)
            text
                .font(.system(size: 13).monospacedDigit())
                .foregroundStyle(ChromeColours.text)
                .frame(maxWidth: .infinity, alignment: .leading)
                .accessibilityIdentifier("lostStripText")
            buttons()
        }
        .padding(.leading, 12)
        .padding(.trailing, 6)
        .frame(height: 50)
        .background(ConnectChrome.rgb(0x0D, 0x1B, 0x28), in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.barBorder, lineWidth: 1))
        .frame(maxWidth: 360)
    }

    private func lostStrip(_ lost: ConnectionFlow.LinkLost) -> some View {
        TimelineView(.periodic(from: .now, by: 1)) { context in
            if lost.stopped {
                VStack(spacing: 8) {
                    strip(pill: ChromeColours.linkLost, text: Text("Stopped reconnecting")) {
                        stripButton("Reconnect", go: true) { Task { await flow.reconnect() } }
                            .accessibilityIdentifier("reconnect")
                    }
                    ConnectChrome.WideButton(title: "Back to Cores") { Task { await flow.leaveBand() } }
                        .frame(maxWidth: 360)
                        .accessibilityIdentifier("leaveBand")
                }
            } else {
                strip(pill: ChromeColours.linkLost, text: Text(Self.retryText(lost, now: context.date))) {
                    stripButton("Cancel", go: false) { Task { await flow.cancelReconnecting() } }
                        .accessibilityIdentifier("cancelReconnecting")
                }
            }
        }
    }

    private func stripButton(_ title: String, go: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 13, weight: .bold))
                .foregroundStyle(go ? Color.white : ConnectChrome.rgb(0xC8, 0xA8, 0xA8))
                .padding(.horizontal, 14)
                .frame(height: 38)
                .background(go ? ChromeColours.buttonOnBlue : ConnectChrome.rgb(0x4A, 0x20, 0x20),
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(go ? ChromeColours.buttonOnBlueBorder : ConnectChrome.rgb(0x6A, 0x30, 0x30),
                                  lineWidth: 1))
        }
        .buttonStyle(.plain)
    }

    private var backOnAir: some View {
        HStack(alignment: .top, spacing: 10) {
            Circle()
                .fill(ChromeColours.linkUp)
                .frame(width: 10, height: 10)
                .padding(.top, 4)
            backOnAirText
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 12)
        .background(ChromeColours.notice, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("backOnAir")
    }

    // MARK: Link lost while keyed (spec section 5.3 item 8, picture 07)

    static let keyedLead = "You were transmitting when the link dropped."
    /// The Core's promise, since the phone can't see it happen.
    static let keyedText = PttController.linkLostText + " Your frequency and settings stay on the Core."

    /// The cover's first line: the Core's own words when it ended the
    /// session, that this phone was transmitting, or that the link dropped.
    static func lostLead(_ lost: ConnectionFlow.LinkLost) -> String {
        if let words = lost.words {
            return words
        }
        return lost.keyed ? keyedLead : "The link to the Core dropped."
    }

    static func lostText(_ lost: ConnectionFlow.LinkLost) -> String {
        if lost.words != nil {
            return lost.keyed ? keyedLead + " " + keyedText
                : "The phone reconnects by itself when the Core is back."
        }
        return lost.keyed ? keyedText : "The phone keeps trying. Your frequency and settings stay on the Core."
    }

    /// Back on a Core that shares itself with other devices, the phone
    /// took its own session back without asking (spec section 5.9 item 6).
    static let ownSessionText = "It was this phone\u{2019}s own session, so it didn\u{2019}t ask."

    /// Back through the remote access service's relay, it says so (picture 07).
    static let backByRelayText = "Back on the air by relay."
    static let transmitStaysOffText = "Transmit stays off until you tap PTT."

    private var backOnAirText: Text {
        let relayed = flow.app.linkRelayed
        let back = Text(relayed ? Self.backByRelayText : "Back on the air.").bold()
            .foregroundColor(ChromeColours.textBright)
        let shared = mirror.map { SeveralDevices.available(in: $0) } ?? false
        if shared {
            return back + Text(" " + Self.ownSessionText)
        }
        return relayed ? back + Text(" " + Self.transmitStaysOffText) : back
    }

    private func placedBackOnAir(_ proxy: GeometryProxy) -> some View {
        backOnAir
            .padding(.horizontal, 10)
            .offset(y: proxy.size.height * 0.55)
            .transition(.opacity)
    }

    /// Shows its content unless the Core has said this phone came back
    /// after its 3 minutes.
    private struct UnlessBackAfterItsTime<Content: View>: View {
        @ObservedObject var devices: SeveralDevicesClient
        @ViewBuilder let content: () -> Content

        var body: some View {
            if !devices.notices.contains(where: { $0.notice.kind == .graceEnded }) {
                content()
            }
        }
    }

    // MARK: Words

    /// "Reconnecting, try 4: next in 8 s", or "trying now" while a try is under way.
    static func retryText(_ lost: ConnectionFlow.LinkLost, now: Date) -> String {
        guard let due = lost.retryAt else {
            return "Reconnecting, try \(lost.attempt): trying now"
        }
        let left = Int(due.timeIntervalSince(now).rounded(.up))
        return left <= 0 ? "Reconnecting, try \(lost.attempt): trying now"
            : "Reconnecting, try \(lost.attempt): next in \(left) s"
    }

    /// The Core's five-second retry at the radio, counted from when the screen showed it.
    static func radioRetryText(_ radio: ConnectionFlow.RadioOff, now: Date) -> String {
        let elapsed = max(0, Int(now.timeIntervalSince(radio.shownAt)))
        let step = elapsed % 5
        return elapsed > 0 && step == 0 ? "Radio link lost: trying now" : "Radio link lost: next try in \(5 - step) s"
    }

    /// What the Core can't reach, and since when when the phone saw it go.
    static func radioText(_ radio: ConnectionFlow.RadioOff, now: Date) -> String {
        if let waiting = radio.waiting {
            // The Core says why it has no radio (link 7.1): its words, as sent.
            return waiting
        }
        let name = radio.radio.map { "the \($0)" } ?? "the radio"
        let place = radio.address.map { "\(name) at \($0)" } ?? name
        let retry = "The Core tries again every 5 seconds. Is the radio on?"
        guard let since = radio.since else {
            return "The Core isn\u{2019}t getting frames from \(place). \(retry)"
        }
        return "The last frame from \(place) came \(ago(since, now: now)). \(retry)"
    }

    private static func ago(_ since: Date, now: Date) -> String {
        let seconds = max(0, Int(now.timeIntervalSince(since)))
        if seconds < 60 {
            return seconds <= 1 ? "just now" : "\(seconds) seconds ago"
        }
        let minutes = seconds / 60
        return minutes == 1 ? "a minute ago" : "\(minutes) minutes ago"
    }
}

/// Fades the band under a cover (the board's `.is-lost .pan`, faded
/// further), and tells
/// the screen when a cover comes up so an open drop sheet closes rather
/// than draw over the cover's words.
struct BandUnderCover: ViewModifier {
    @ObservedObject var flow: ConnectionFlow
    let covered: () -> Void

    func body(content: Content) -> some View {
        content
            .opacity(flow.coversBand ? LinkLostBanner.bandOpacityUnderCover : 1)
            .onChange(of: flow.coversBand) { _, now in
                if now {
                    covered()
                }
            }
    }
}
