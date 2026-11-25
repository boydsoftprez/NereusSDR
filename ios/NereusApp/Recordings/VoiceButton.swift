// NereusSDR for iOS: the round VOICE beside PTT and the voice keyer's M1 to M8 over the pan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 18, VOICE beside PTT and its messages over the pan.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: VoiceButtonGate, the switch on Tools > Voice Keyer that takes VOICE
//   off the main screen. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: Screen walk for PR 808, the Voice keyer panel scrolls when it
//   is taller than the screen (a phone sideways). J.J. Boyd (KG4VCF), AI-
//   assisted via Claude Code.

import NereusLink
import NereusModels
import SwiftUI

/// VOICE on the main screen while this device's switch on Tools > Voice
/// Keyer is on (``PhoneSettings/voiceButtonShown``); off, nothing is drawn.
/// `content` is told whether VOICE goes above PTT: while the thumbwheel is
/// on, its step box takes the place beside PTT.
struct VoiceButtonGate<Content: View>: View {
    @ObservedObject var settings: PhoneSettings
    @ViewBuilder let content: (_ above: Bool) -> Content

    var body: some View {
        if settings.voiceButtonShown {
            content(settings.dialKind == .thumbwheel)
        }
    }
}

/// The round VOICE beside PTT (design 2026-11-15-recording-voice-keyer-dsr
/// sections 3, 6.6 and 6.8; the phone mockup's pan). A tap opens M1 to M8
/// over the pan (``VoiceKeyerPanel``). While a message goes out it turns
/// red with the message's number and a red ring filling round it; between
/// repeats it shows the seconds left. A tap then stops it, as PTT does.
/// While another station holds transmit it dims, and the panel offers
/// Take transmit. With no Core, or one too old, it dims and the panel says why.
struct VoiceButton: View {
    let model: VoiceKeyerModel
    let open: () -> Void

    static let diameter: CGFloat = 52
    /// The space between PTT and VOICE.
    static let gap: CGFloat = 11
    /// The ring sits this far outside the button.
    static let ringOutset: CGFloat = 4

    /// Where VOICE sits beside a PTT at `ptt`: to its right, centred on it.
    static func frame(besidePtt ptt: CGRect) -> CGRect {
        CGRect(x: ptt.maxX + gap, y: ptt.midY - diameter / 2, width: diameter, height: diameter)
    }

    /// Where VOICE sits over a PTT at `ptt` when the thumbwheel is on and
    /// its step box takes the place beside PTT: above it, centred on it.
    static func frame(abovePtt ptt: CGRect) -> CGRect {
        CGRect(x: ptt.midX - diameter / 2, y: ptt.minY - gap - diameter, width: diameter, height: diameter)
    }

    var body: some View {
        TimelineView(.animation(minimumInterval: 0.2, paused: model.client.voiceKeyer?.sending == nil)) { timeline in
            face(model.progress(now: timeline.date))
        }
    }

    private func face(_ progress: VoiceKeyerModel.Progress?) -> some View {
        let air = progress.map { !$0.waiting } ?? false
        let waiting = progress?.waiting ?? false
        let dim = progress == nil && model.sendReason != nil
        let label = progress.map { $0.waiting ? "\($0.waitSeconds) s" : "M\($0.n)" } ?? "VOICE"
        return Button {
            Task {
                if await !model.voiceTapped() {
                    open()
                }
            }
        } label: {
            ZStack {
                Circle().fill(air ? ChromeColours.revoke : waiting ? ChromeColours.revoke.opacity(0.7)
                              : ChromeColours.pttGround)
                Circle().strokeBorder(air || waiting ? ChromeColours.txRed : ChromeColours.pttEdge, lineWidth: 1.5)
                VStack(spacing: 1) {
                    WaveGlyph()
                        .stroke(style: StrokeStyle(lineWidth: 2, lineCap: .round))
                        .frame(width: 22, height: 16)
                    Text(label)
                        .font(.system(size: progress == nil ? 8.5 : 9.5, weight: .bold).monospacedDigit())
                        .kerning(0.6)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)
                }
                .foregroundStyle(air || waiting ? ChromeColours.textBright : ChromeColours.pttText)
                if air, let progress {
                    let ring = Self.diameter + 2 * Self.ringOutset
                    Circle()
                        .stroke(ChromeColours.txRed.opacity(0.25), lineWidth: 3)
                        .frame(width: ring - 3, height: ring - 3)
                    Circle()
                        .trim(from: 0, to: progress.fraction)
                        .stroke(ChromeColours.txRed, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                        .rotationEffect(.degrees(-90))
                        .frame(width: ring - 3, height: ring - 3)
                }
            }
            .frame(width: Self.diameter, height: Self.diameter)
            .contentShape(Circle())
        }
        .buttonStyle(.plain)
        .opacity(dim ? 0.45 : 1)
        .accessibilityLabel(progress == nil ? "Voice keyer" : "Stop voice keyer")
        .accessibilityValue(progress.map { model.airText($0).title } ?? (model.sendReason ?? ""))
        .accessibilityHint(progress == nil ? "Opens your eight messages" : "Stops the message")
        .accessibilityIdentifier("voiceKeyer.voice")
    }
}

/// Five bars, the voice keyer's mark.
struct WaveGlyph: Shape {
    func path(in rect: CGRect) -> Path {
        // The mockup's 22 by 16 bars: x, top, bottom.
        let bars: [(CGFloat, CGFloat, CGFloat)] = [(2, 6, 10), (6, 3, 13), (10, 1, 15), (14, 4, 12), (18, 6.5, 9.5)]
        let sx = rect.width / 22
        let sy = rect.height / 16
        var path = Path()
        for (x, top, bottom) in bars {
            path.move(to: CGPoint(x: rect.minX + x * sx, y: rect.minY + top * sy))
            path.addLine(to: CGPoint(x: rect.minX + x * sx, y: rect.minY + bottom * sy))
        }
        return path
    }
}

/// M1 to M8 over the pan, opened by VOICE: a tap sends one and closes the
/// panel; an empty one opens Tools > Voice Keyer to record or import it.
/// Stop while one goes out, Repeat, and All settings › to the page. While
/// another station holds transmit the messages dim, with Take transmit.
struct VoiceKeyerPanel: View {
    let model: VoiceKeyerModel
    var take: TransmitTakeModel?
    let close: () -> Void
    let openPage: () -> Void

    var body: some View {
        TimelineView(.animation(minimumInterval: 0.2, paused: model.client.voiceKeyer?.sending == nil)) { timeline in
            let progress = model.progress(now: timeline.date)
            // Whole when it fits; on a short screen (a phone sideways) it
            // scrolls, where its title and Repeat ran off the screen.
            ViewThatFits(in: .vertical) {
                panel(progress)
                ScrollView {
                    panel(progress)
                }
                .scrollBounceBehavior(.basedOnSize)
                .modifier(PanelTopScrollEdge())
            }
        }
    }

    private func panel(_ progress: VoiceKeyerModel.Progress?) -> some View {
        let enabled = model.reason == nil
        return VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Voice keyer")
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundStyle(ChromeColours.textBright)
                    .accessibilityAddTraits(.isHeader)
                Spacer()
                Button("All settings ›") {
                    close()
                    openPage()
                }
                .font(.system(size: 15))
                .foregroundStyle(ChromeColours.accentText)
                .accessibilityIdentifier("voiceKeyer.allSettings")
            }
            if let reason = model.reason {
                ConnectChrome.NoticeBox(tone: .warn, title: nil, text: reason)
                    .accessibilityIdentifier("voiceKeyer.panelReason")
            }
            LazyVGrid(columns: [GridItem(.flexible(), spacing: 8), GridItem(.flexible(), spacing: 8)], spacing: 8) {
                ForEach(VoiceKeyerModel.slotNumbers, id: \.self) { n in
                    tile(n, progress: progress, enabled: enabled)
                }
            }
            .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
            if enabled, let held = model.held {
                VStack(alignment: .leading, spacing: 8) {
                    Text(held.reason)
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.buttonOnAmberText)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("voiceKeyer.panelHeld")
                    if let take, model.offersTake {
                        TakeTransmitButton(take: take, identifier: "voiceKeyer.panelTakeTransmit")
                    }
                }
                .padding(10)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(ChromeColours.buttonOnAmber, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
                .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
                    .strokeBorder(ChromeColours.buttonOnAmberBorder, lineWidth: 1))
            }
            if let note = model.note, note.refusal {
                ToolPageParts.Refusal(text: note.text, identifier: "voiceKeyer.panelNote")
            }
            if progress != nil {
                Button {
                    Task { await model.stop() }
                } label: {
                    Text("Stop")
                        .font(.system(size: 17, weight: .bold))
                        .frame(maxWidth: .infinity, minHeight: 46)
                }
                .buttonStyle(SetupButtonStyle(.keyed, fill: true))
                .accessibilityIdentifier("voiceKeyer.panelStop")
            }
            Rectangle().fill(ChromeColours.panelEdge).frame(height: 1)
            HStack {
                VStack(alignment: .leading, spacing: 1) {
                    Text("Repeat the message")
                        .font(.system(size: 15))
                        .foregroundStyle(ChromeColours.text)
                    Text(model.repeatSummary)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                }
                Spacer()
                SpotHubPage.OnOff(isOn: model.repeatOn, enabled: enabled, identifier: "voiceKeyer.panelRepeat") {
                    Task { await model.setRepeat(!model.repeatOn) }
                }
            }
        }
        .padding(.horizontal, 16)
        .padding(.top, 14)
        .padding(.bottom, 20)
        .background(ChromeColours.sheet,
                    in: UnevenRoundedRectangle(topLeadingRadius: WorkspacePalette.panelRadius,
                                               topTrailingRadius: WorkspacePalette.panelRadius))
        .overlay(alignment: .top) {
            Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1)
        }
        .accessibilityIdentifier("voiceKeyer.panel")
    }

    private func tile(_ n: Int, progress: VoiceKeyerModel.Progress?, enabled: Bool) -> some View {
        let slot = model.slot(n)
        let sending = progress?.n == n
        let dim = enabled && !slot.empty && model.held != nil && !sending
        let time: String = {
            if sending, let progress {
                return progress.waiting ? "again \(progress.waitSeconds) s"
                    : RecordingFlagModel.lengthText(progress.positionMs)
            }
            return slot.empty ? "" : VoiceKeyerModel.lengthText(slot.lengthMs)
        }()
        return Button {
            Task {
                let result = await model.tapMessage(n)
                if result == .sent {
                    close()
                } else if result == .empty {
                    close()
                    openPage()
                }
            }
        } label: {
            VStack(alignment: .leading, spacing: 2) {
                HStack {
                    Text("M\(n)")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundStyle(sending ? ChromeColours.revokeText : ChromeColours.textDim)
                    Spacer()
                    Text(time)
                        .font(.system(size: 11.5, design: .monospaced))
                        .foregroundStyle(sending ? ChromeColours.revokeText : ChromeColours.textDim)
                }
                Text(slot.empty ? "Empty" : model.label(n))
                    .font(.system(size: 15, weight: slot.empty ? .regular : .semibold))
                    .foregroundStyle(slot.empty ? ChromeColours.textFaint : ChromeColours.textBright)
                    .lineLimit(1)
            }
            .padding(.horizontal, 10)
            .frame(maxWidth: .infinity, minHeight: 56, alignment: .leading)
            .background(sending ? ChromeColours.revoke : slot.empty ? ChromeColours.inset : ChromeColours.button,
                        in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
            .overlay {
                RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
                    .strokeBorder(sending ? ChromeColours.txRed : ChromeColours.buttonBorder,
                                  style: StrokeStyle(lineWidth: 1, dash: slot.empty ? [4, 3] : []))
            }
            .overlay(alignment: .bottomLeading) {
                if sending, let progress, !progress.waiting {
                    GeometryReader { proxy in
                        Rectangle().fill(ChromeColours.txRed)
                            .frame(width: proxy.size.width * progress.fraction, height: 3)
                            .frame(maxHeight: .infinity, alignment: .bottom)
                    }
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
            .opacity(dim ? WorkspacePalette.disabledOpacity : 1)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityLabel(slot.empty ? "M\(n), empty" : "M\(n), " + model.label(n))
        .accessibilityHint(sending ? "Stops it" : slot.empty ? "Opens the voice keyer page to record it"
                           : model.held?.reason ?? "Sends it on the air")
        .accessibilityIdentifier("voiceKeyer.tile\(n)")
    }
}

/// The Voice keyer panel's title sits at the top of its scroll, at the very
/// top of a phone held sideways: the system's soft scroll edge would blur it.
private struct PanelTopScrollEdge: ViewModifier {
    func body(content: Content) -> some View {
        if #available(iOS 26.0, *) {
            content.scrollEdgeEffectHidden(true, for: .top)
        } else {
            content
        }
    }
}
