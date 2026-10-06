// NereusSDR for iOS: the RX panel: AF gain, AGC, filter presets, the noise buttons and squelch
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusModels
import SwiftUI

/// The RX panel (spec section 5.1 item 5, picture 01's "RX panel"): an
/// applet in NereusSDR's style that slides in over the band from the left,
/// with the active slice's AF gain, AGC, filter presets, noise buttons and
/// squelch. Everything it shows is the Core's (``RxPanelModel``).
struct RxPanel: View {
    @ObservedObject var model: RxPanelModel
    /// The active slice's colour, for the title's letter.
    let sliceColour: String?
    /// The panel's width; nil fills the width it is given (the iPad's
    /// applet column and front panel).
    var width: CGFloat? = RxPanel.width
    /// The panel scrolls itself and shows its own number pad; in the iPad's
    /// applet column the column does both.
    var scrolls = true

    static let width: CGFloat = 300

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            title
            if scrolls {
                ScrollView {
                    content
                }
                .overlay {
                    ValuePadLayer(pad: model.pad)
                }
            } else {
                content
            }
        }
        .frame(width: width)
        .frame(maxWidth: width == nil ? .infinity : nil, maxHeight: scrolls ? .infinity : nil, alignment: .top)
        .background(ChromeColours.panel)
        .accessibilityElement(children: .contain)
        .accessibilityLabel("RX panel")
        .accessibilityIdentifier("rxPanelDrawer")
    }

    private var content: some View {
        VStack(alignment: .leading, spacing: 9) {
            RxGainRow(model: model, slices: model.slices)
            VStack(alignment: .leading, spacing: 9) {
                caption("AGC:")
                grid(columns: 5) {
                    ForEach(model.agc) { choice in
                        PanelButton(label: choice.label, lit: choice.lit, style: .blue) {
                            model.selectAgc(choice.id)
                        }
                    }
                }
                caption("Filter:")
                grid(columns: 5) {
                    ForEach(model.presets) { preset in
                        PanelButton(label: preset.text, lit: preset.lit, style: .blue) {
                            model.selectPreset(preset)
                        }
                        .accessibilityLabel("\(preset.name), \(preset.text)")
                    }
                }
                edges
                caption("Noise:")
                grid(columns: 3) {
                    ForEach(model.noise) { button in
                        PanelButton(label: button.label, lit: button.lit, style: .dsp, disabled: button.reason != nil,
                                    warning: button.warning != nil) {
                            model.tap(button)
                        }
                        .accessibilityHint(button.reason ?? button.warning ?? "")
                    }
                }
                ForEach(model.noise.filter { $0.reason != nil }) { button in
                    note("\(button.label): \(button.reason ?? "")")
                }
                NnrStepBack(rx: model, identifier: "rxPanelNnr")
                squelch
                if let reason = model.note {
                    note(reason)
                        .accessibilityIdentifier("rxPanelNote")
                }
            }
            .modifier(RxListenGrey(slices: model.slices))
        }
        .padding(10)
    }

    private var title: some View {
        HStack(spacing: 6) {
            Text("RX")
            if let letter = model.sliceLetter {
                SliceLetterBadge(letter: letter, colour: BandColours.slice(sliceColour ?? BandSlice.colourUnknown))
                    .scaleEffect(15.0 / 18.0)
                    .frame(width: 15, height: 15)
            }
        }
        .font(.system(size: 11, weight: .bold))
        .foregroundStyle(ChromeColours.icon)
        .padding(.horizontal, 8)
        .frame(maxWidth: .infinity, minHeight: 22, maxHeight: 22, alignment: .leading)
        .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                           .init(color: ChromeColours.titleMiddle, location: 0.5),
                                           .init(color: ChromeColours.titleBottom, location: 1)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
        }
    }

    /// The slice's filter edges, each typed in (C2), as the desktop's typed
    /// and dragged edges set them.
    private var edges: some View {
        HStack(spacing: 6) {
            Text("Low")
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.textDim)
            ValueField(text: ModesChrome.hertz(model.filterLowHz), accessibility: "Filter low edge",
                       disabled: model.filterLowHz == nil, grow: true) {
                model.openFilterEdgePad(low: true)
            }
            .accessibilityIdentifier("rxFilterLow")
            Text("High")
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.textDim)
            ValueField(text: ModesChrome.hertz(model.filterHighHz), accessibility: "Filter high edge",
                       disabled: model.filterHighHz == nil, grow: true) {
                model.openFilterEdgePad(low: false)
            }
            .accessibilityIdentifier("rxFilterHigh")
        }
    }

    private var squelch: some View {
        HStack(spacing: 8) {
            // The desktop RX applet's SQL switch, in the label's place.
            Button {
                model.toggleSquelch()
            } label: {
                Text("Squelch")
                    .font(.system(size: 11, weight: .bold))
                    .foregroundStyle(model.squelchOn ? ChromeColours.buttonOnGreenText : ChromeColours.textDim)
                    .frame(width: 62, height: 28)
                    .background(model.squelchOn ? ChromeColours.buttonOnGreen : ChromeColours.button,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(model.squelchOn ? ChromeColours.buttonOnGreenBorder : ChromeColours.buttonBorder,
                                      lineWidth: 1))
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Squelch")
            .accessibilityValue(model.squelchOn ? "On" : "Off")
            .accessibilityAddTraits(model.squelchOn ? .isSelected : [])
            PanelSliderRow(label: nil, value: model.squelch, range: model.squelchRange,
                           accessibility: "Squelch level", notConfirmed: model.isUnconfirmed("ssqlThresh")) { model.setSquelch($0) }
        }
    }

    private func caption(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.caption)
            .padding(.bottom, -4)
    }

    private func note(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.textFaint)
            .fixedSize(horizontal: false, vertical: true)
    }

    private func grid(columns: Int, @ViewBuilder content: () -> some View) -> some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible(minimum: 0), spacing: 4), count: columns), spacing: 4) {
            content()
        }
    }
}

/// The panel's AF Gain: the slice's own, or, on a slice this phone only
/// listens to (R-IOS-42), this phone's own volume, which changes only
/// what this phone hears (`slice.setListenLevel`).
private struct RxGainRow: View {
    @ObservedObject var model: RxPanelModel
    @ObservedObject var slices: BandSlicesModel

    var body: some View {
        if let entry = slices.active, entry.listening {
            PanelSliderRow(label: "AF Gain", value: (slices.listenLevel(entry.id).level * 100).rounded(),
                           range: ListenerAudio.range,
                           accessibility: "Volume for slice \(entry.slice.letter), on this phone only") { gain in
                slices.setListenLevel(entry.id, level: gain / 100, muted: slices.listenLevel(entry.id).muted)
            }
            .accessibilityIdentifier("rxListenerGain")
        } else {
            PanelSliderRow(label: "AF Gain", value: model.afGain, range: model.afRange,
                           accessibility: "AF gain", notConfirmed: model.isUnconfirmed("afGain")) { model.setAfGain($0) }
        }
    }
}

/// The rest of the panel on a slice this phone only listens to: dimmed,
/// and a tap gives the Core's words (``ListenerDim``).
private struct RxListenGrey: ViewModifier {
    @ObservedObject var slices: BandSlicesModel

    func body(content: Content) -> some View {
        content.listenerDim(slices.active?.ownerLine) { slices.showReason($0) }
    }
}

/// How tall a ``PanelButton`` is at least: the RX panel's 36 points, or a
/// finger's 44 where a flag's tab panel sets it.
private struct PanelButtonHeightKey: EnvironmentKey {
    static let defaultValue: CGFloat = 36
}

private struct PanelTextScaleKey: EnvironmentKey {
    static let defaultValue: CGFloat = 1
}

private struct PanelSliderHeightKey: EnvironmentKey {
    static let defaultValue: CGFloat = 30
}

extension EnvironmentValues {
    var panelTextScale: CGFloat {
        get { self[PanelTextScaleKey.self] }
        set { self[PanelTextScaleKey.self] = newValue }
    }

    var panelSliderHeight: CGFloat {
        get { self[PanelSliderHeightKey.self] }
        set { self[PanelSliderHeightKey.self] = newValue }
    }

    var panelButtonHeight: CGFloat {
        get { self[PanelButtonHeightKey.self] }
        set { self[PanelButtonHeightKey.self] = newValue }
    }
}

/// One of the panel's square buttons, in the desktop's colours: blue for a
/// choice, green for a DSP switch, greyed when the Core cannot run it.
struct PanelButton: View {
    enum Style {
        case blue
        case dsp
        /// TUNE and MOX, lit red while on the air; the TX antenna, lit red.
        case red
        /// RIT and XIT, lit amber (the Modes tab).
        case amber
    }

    let label: String
    let lit: Bool
    let style: Style
    var disabled = false
    /// The desktop's amber mark in the corner: the Core holds this back
    /// (NNR stepped back), with its reason beside the button.
    var warning = false
    let action: () -> Void
    /// 36 points tall, and 44 in a flag's tab panel (``FlagPanel``).
    @Environment(\.panelButtonHeight) private var height
    @Environment(\.panelTextScale) private var textScale

    var body: some View {
        Button(action: action) {
            Text(label)
                .font(.system(size: 12 * textScale, weight: .bold))
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .foregroundStyle(foreground)
                .frame(maxWidth: .infinity, minHeight: height)
                .background(background, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(warning ? ChromeColours.buttonOnAmberBorder : border, lineWidth: 1))
                .overlay(alignment: .topTrailing) {
                    if warning {
                        Circle().fill(ChromeColours.buttonOnAmber).frame(width: 7, height: 7).padding(4)
                    }
                }
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(disabled)
        .accessibilityLabel(label)
        .accessibilityAddTraits(lit ? .isSelected : [])
    }

    private var foreground: Color {
        if disabled {
            return ChromeColours.buttonOffText
        }
        guard lit else {
            return ChromeColours.text
        }
        switch style {
        case .dsp:
            return ChromeColours.buttonOnGreenText
        case .amber:
            return ChromeColours.buttonOnAmberText
        case .blue, .red:
            return .white
        }
    }

    private var background: Color {
        if disabled {
            return ChromeColours.buttonOff
        }
        guard lit else {
            return ChromeColours.button
        }
        switch style {
        case .blue:
            return ChromeColours.buttonOnBlue
        case .dsp:
            return ChromeColours.buttonOnGreen
        case .red:
            return ChromeColours.txRed
        case .amber:
            return ChromeColours.buttonOnAmber
        }
    }

    private var border: Color {
        if disabled {
            return ChromeColours.buttonOffBorder
        }
        guard lit else {
            return ChromeColours.buttonBorder
        }
        switch style {
        case .blue:
            return ChromeColours.buttonOnBlueBorder
        case .dsp:
            return ChromeColours.buttonOnGreenBorder
        case .red:
            return ChromeColours.pttEdge
        case .amber:
            return ChromeColours.buttonOnAmberBorder
        }
    }
}

/// A label, the board's slider and the value in its inset box. The slider
/// follows the finger while it moves and shows the mirror's value
/// otherwise, which holds the operator's value until the Core answers; it
/// sends at most one value every 50 ms while dragged and the final value
/// on release (``SliderSendPacer``). Its range is the catalogue's, and
/// without one it is greyed. A row that
/// cannot change now (`greyed`) keeps its range, so the thumb stays where
/// the value is.
struct PanelSliderRow: View {
    let label: String?
    let value: Double?
    let range: StationCatalog.Range?
    let accessibility: String
    /// How the value reads beside the slider; whole numbers, or one
    /// decimal for a step below one, by default.
    var format: ((Double) -> String)?
    /// Greyed, with the thumb still placed on `range`: it takes no finger
    /// and no VoiceOver step.
    var greyed = false
    var notConfirmed = false
    let onChange: (Double) -> Void

    /// The finger's value, shown until the value it sent shows.
    @State private var gestureDraft = SliderGestureDraft()
    @State private var pacer = SliderSendPacer()
    @Environment(\.panelSliderHeight) private var height
    @Environment(\.panelTextScale) private var textScale

    var body: some View {
        HStack(spacing: 8) {
            if let label {
                Text(label)
                    .font(.system(size: 11 * textScale))
                    .foregroundStyle(ChromeColours.textDim)
                    .frame(width: 62 * textScale, alignment: .leading)
            }
            GeometryReader { proxy in
                track(width: proxy.size.width)
            }
            .frame(height: height)
            Text(shown.map { formatted($0) } ?? "")
                .font(.system(size: 12 * textScale).monospacedDigit())
                .foregroundStyle(ChromeColours.text)
                .padding(.horizontal, 4)
                .padding(.vertical, 2)
                .frame(minWidth: 34)
                .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
                .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
        }
        .opacity(range == nil || value == nil || greyed ? 0.45 : 1)
        .onChange(of: value) {
            gestureDraft.modelChanged(notConfirmed: notConfirmed)
        }
        .onChange(of: notConfirmed) { gestureDraft.modelChanged(notConfirmed: notConfirmed) }
        .onChange(of: greyed) {
            if greyed {
                gestureDraft.disable()
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(accessibility)
        .accessibilityValue(shown.map { formatted($0) } ?? "")
        .accessibilityAdjustableAction { direction in
            guard let range, let current = value, !greyed else {
                return
            }
            let step = range.step > 0 ? range.step : 1
            onChange(direction == .increment ? current + step : current - step)
        }
    }

    private var shown: Double? {
        gestureDraft.value ?? value
    }

    private func fraction(_ number: Double) -> Double {
        guard let range, range.max > range.min else {
            return 0
        }
        return min(max((number - range.min) / (range.max - range.min), 0), 1)
    }

    private func track(width: CGFloat) -> some View {
        let thumb: CGFloat = 22
        let usable = max(width - thumb, 1)
        let x = CGFloat(fraction(shown ?? range?.min ?? 0)) * usable
        return ZStack(alignment: .leading) {
            Capsule().fill(ChromeColours.sliderTrack).frame(height: 4).padding(.horizontal, thumb / 2)
            Capsule().fill(ChromeColours.accent).frame(width: x + thumb / 2, height: 4).padding(.leading, thumb / 2)
            Circle()
                .fill(ChromeColours.accent)
                .overlay(Circle().strokeBorder(ChromeColours.panel, lineWidth: 3))
                .frame(width: thumb, height: thumb)
                .offset(x: x)
        }
        .frame(maxHeight: .infinity)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 0)
            .onChanged { gesture in
                guard let range, value != nil, !greyed else {
                    return
                }
                gestureDraft.begin()
                let position = min(max(Double((gesture.location.x - thumb / 2) / usable), 0), 1)
                let next = RxPanelModel.snapped(range.min + position * (range.max - range.min), to: range)
                if next != gestureDraft.value {
                    gestureDraft.move(to: next)
                    pacer.send = { next in gestureDraft.submitted(next); onChange(next) }
                    pacer.move(to: next)
                }
            }
            .onEnded { _ in
                pacer.send = { next in gestureDraft.submitted(next); onChange(next) }
                pacer.release()
                // The mirror shows the value sent from now on, held until
                // the Core answers; the finger's value goes as it arrives.
                gestureDraft.release(model: value)
            })
    }

    private func formatted(_ number: Double) -> String {
        format?(number) ?? Self.text(number, step: range?.step)
    }

    private static func text(_ number: Double, step: Double?) -> String {
        if let step, step > 0, step < 1 {
            return String(format: "%.1f", number)
        }
        return String(Int(number.rounded()))
    }
}
