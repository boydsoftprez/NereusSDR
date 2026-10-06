// NereusSDR for iOS: the slice list the toolbar's Slice button drops: every live slice on the Core, one row each
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// The list behind the toolbar's Slice button (R-IOS-42, R-IOS-11; JJ's
/// rulings of 2026-09-30), as the board's `#slicelist-review` draws it
/// (`.sll-sheet`): a 44-point title row with the count and ✕, then one row
/// per live slice in letter order (``SliceListModel``), then New slice.
///
/// - This phone's own slice on its band: the whole row is one button with
///   its state pill, Active or Make active.
/// - A slice it is in on another pan: the row's top is one button, Show
///   its band (Active while shown).
/// - A slice it is not in: Listen and Take control. One it listens to:
///   Stop listening, Take control, and its own volume and mute. One it
///   controls elsewhere: Release.
/// - Take control is greyed only while the slice is on the air, with the
///   Core's words under it.
///
/// Everything is a plain button, 44 points tall at least.
struct SliceListSheet: View {
    @ObservedObject var model: SliceListModel
    @ObservedObject var slices: BandSlicesModel
    let close: () -> Void

    @Environment(\.dynamicTypeSize) private var typeSize

    private var large: Bool { typeSize.isAccessibilitySize }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            titleRow
            ViewThatFits(in: .vertical) {
                list
                ScrollView { list }
            }
        }
        .background(ChromeColours.sheet)
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.55), radius: 14, y: 10)
        .contentShape(RoundedRectangle(cornerRadius: 6))
        .accessibilityElement(children: .contain)
        .accessibilityLabel(SliceListModel.title)
        .accessibilityIdentifier("sliceList")
    }

    private var titleRow: some View {
        HStack(spacing: 8) {
            Text(SliceListModel.title)
                .font(.system(size: large ? 19 : 13, weight: .bold))
                .foregroundStyle(ChromeColours.textBright)
                .frame(maxWidth: .infinity, alignment: .leading)
                .accessibilityAddTraits(.isHeader)
            Text(model.liveText)
                .font(.system(size: large ? 19 : 11, weight: .semibold))
                .tracking(0.44)
                .foregroundStyle(ChromeColours.textDim)
                .accessibilityIdentifier("sliceListCount")
            Button(action: close) {
                Text("\u{2715}")
                    .font(.system(size: large ? 19 : 16))
                    .foregroundStyle(ChromeColours.text)
                    .frame(width: 44, height: 44)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Close the slice list")
            .accessibilityIdentifier("sliceListClose")
        }
        .padding(.leading, 12)
        .frame(minHeight: 44)
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1)
        }
    }

    private var list: some View {
        VStack(alignment: .leading, spacing: 0) {
            ForEach(Array(model.rows.enumerated()), id: \.element.id) { index, row in
                SliceListRow(model: model, slices: slices, row: row, large: large, close: close)
                    .overlay(alignment: .bottom) {
                        if index < model.rows.count - 1 {
                            Rectangle().fill(SliceListStyle.rowRule).frame(height: 1)
                        }
                    }
            }
            if model.older {
                Text(SliceListModel.olderCoreText)
                    .font(.system(size: large ? 19 : 13))
                    .foregroundStyle(ChromeColours.text)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 10)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .overlay(alignment: .top) {
                        Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1)
                    }
                    .accessibilityIdentifier("sliceListOlder")
            }
            foot
        }
    }

    private var foot: some View {
        VStack(alignment: .leading, spacing: 0) {
            if let note = model.note {
                Text(note)
                    .font(.system(size: large ? 19 : 12.5, weight: .semibold))
                    .foregroundStyle(ChromeColours.refusalText)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.bottom, 8)
                    .accessibilityIdentifier("sliceListNote")
            }
            SliceListButton(title: SliceListModel.newSliceTitle, kind: model.canAddSlice ? .plain : .greyed,
                            large: large) {
                model.newSlice()
            }
            .accessibilityHint(model.newSliceRefusal?.text ?? "")
            .accessibilityIdentifier("sliceListNewSlice")
            if let refusal = model.newSliceRefusal {
                refused(refusal)
            }
        }
        .padding(10)
        .overlay(alignment: .top) {
            Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1)
        }
    }

    /// New slice refused for room: the Core's words, and the slices it
    /// offers to listen in to.
    private func refused(_ refusal: SliceListModel.NewSliceRefusal) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            Text(refusal.text)
                .font(.system(size: large ? 19 : 13, weight: .bold))
                .foregroundStyle(SliceListStyle.refusedText)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("sliceListRefused")
            if !refusal.usable.isEmpty {
                Text(SliceListModel.listenInsteadText)
                    .font(.system(size: large ? 19 : 12, weight: .bold))
                    .foregroundStyle(ChromeColours.text)
                    .padding(.top, 8)
                    .padding(.bottom, 4)
                ForEach(refusal.usable) { usable in
                    choice(usable)
                        .padding(.top, 6)
                }
            }
        }
        .padding(10)
        .background(SliceListStyle.refusedGround, in: RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(SliceListStyle.refusedEdge, lineWidth: 1))
        .padding(.top, 10)
    }

    private func choice(_ usable: SliceRoster.UsableSlice) -> some View {
        let row = model.row(usable.sliceId)
        let words = (row.map { "\(model.frequencyText($0)) \(model.modeText($0)) \u{00B7} " } ?? "")
            + "controlled by " + model.holderText(usable)
        let badge = SliceLetterBadge(letter: usable.letter, colour: BandColours.slice(model.colour(usable.sliceId)),
                                     size: large ? 26 : 22, points: large ? 17 : 13)
        let text = Text(words)
            .font(.system(size: large ? 19 : 12))
            .foregroundStyle(ChromeColours.text)
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
        let listen = SliceListButton(title: SliceListModel.listenTitle, kind: .plain, large: large) {
            model.listen(usable)
        }
        .accessibilityLabel("Listen to slice \(usable.letter)")
        .accessibilityIdentifier("sliceListChoice\(usable.letter)")
        return Group {
            if large {
                VStack(alignment: .leading, spacing: 8) {
                    HStack(spacing: 8) {
                        badge
                        text
                    }
                    listen
                }
            } else {
                HStack(spacing: 8) {
                    badge
                    text
                    listen.frame(width: 96)
                }
            }
        }
    }
}

/// The list's colours, the board's.
enum SliceListStyle {
    static let rowRule = rgb(0x1A, 0x2A, 0x3A)
    static let whereEdge = rgb(0x30, 0x40, 0x50)
    static let whereText = rgb(0x8A, 0xA8, 0xC0)
    static let hereEdge = rgb(0x20, 0x50, 0x70)
    static let radeMode = rgb(0xB0, 0x70, 0xFF)
    static let refusedGround = rgb(0x1A, 0x14, 0x10)
    static let refusedEdge = rgb(0x5A, 0x40, 0x20)
    static let refusedText = rgb(0xFF, 0xCC, 0x66)
    static let greyed = rgb(0x1A, 0x1A, 0x2A)
    static let greyedEdge = rgb(0x2A, 0x30, 0x40)
    static let greyedText = rgb(0x6A, 0x78, 0x88)
    static let muted = rgb(0xCC, 0x22, 0x22)
    static let mutedEdge = rgb(0xFF, 0x44, 0x44)
    static let valueEdge = rgb(0x20, 0x30, 0x40)
    static let modesBlock = rgb(0x0E, 0x16, 0x24)
    static let nudge = rgb(0xFF, 0xD8, 0x4A)

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}

/// One of the list's buttons (`.sll-btn`): 44 points tall at least, the
/// blue go colour for Take control, greyed (never hidden) when it cannot
/// be pressed.
struct SliceListButton: View {
    enum Kind {
        case plain
        case go
        case greyed
    }

    let title: String
    let kind: Kind
    let large: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: large ? 19 : 13, weight: .bold))
                .foregroundStyle(foreground)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .frame(maxWidth: .infinity, minHeight: 44)
                .background(background, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(border, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityValue(kind == .greyed ? "Not available" : "")
    }

    private var foreground: Color {
        switch kind {
        case .plain:
            return ChromeColours.text
        case .go:
            return .white
        case .greyed:
            return SliceListStyle.greyedText
        }
    }

    private var background: Color {
        switch kind {
        case .plain:
            return ChromeColours.button
        case .go:
            return ChromeColours.buttonOnBlue
        case .greyed:
            return SliceListStyle.greyed
        }
    }

    private var border: Color {
        switch kind {
        case .plain:
            return ChromeColours.buttonBorder
        case .go:
            return ChromeColours.buttonOnBlueBorder
        case .greyed:
            return SliceListStyle.greyedEdge
        }
    }
}

/// This phone's own volume for a slice it listens to (`.sll-vol`): Your
/// volume, only what this phone hears; mute; the slider; the value.
struct YourVolumeRow: View {
    let letter: String
    let colour: Color
    let level: BandSlicesModel.ListenLevel
    let large: Bool
    let setLevel: (Double, Bool) -> Void
    /// Where the row is, for its identifiers: `Row`, `Modes`.
    let place: String

    @State private var dragging: Double?

    var body: some View {
        let label = VStack(alignment: .leading, spacing: 1) {
            Text(SliceListModel.volumeTitle)
                .font(.system(size: large ? 19 : 12.5, weight: .bold))
                .foregroundStyle(ChromeColours.textBright)
            Text(SliceListModel.onlyThisPhoneText)
                .font(.system(size: large ? 19 : 11))
                .foregroundStyle(ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
        }
        .frame(maxWidth: large ? .infinity : nil, alignment: .leading)
        .accessibilityElement(children: .combine)
        Group {
            if large {
                VStack(alignment: .leading, spacing: 8) {
                    label
                    HStack(spacing: 8) {
                        mute
                        slider
                        value.frame(width: 56)
                    }
                }
            } else {
                HStack(spacing: 8) {
                    // The words at their own width; the slider takes what is left.
                    label.fixedSize()
                    mute
                    slider.frame(minWidth: 90)
                    value.frame(width: 40)
                }
            }
        }
        .padding(.top, 8)
    }

    private var shown: Double { dragging ?? level.level }

    private var mute: some View {
        Button {
            setLevel(level.level, !level.muted)
        } label: {
            Image(systemName: level.muted ? "speaker.slash.fill" : "speaker.wave.2.fill")
                .font(.system(size: large ? 19 : 16))
                .foregroundStyle(level.muted ? Color.white : ChromeColours.text)
                .frame(width: 44, height: 44)
                .background(level.muted ? SliceListStyle.muted : ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(level.muted ? SliceListStyle.mutedEdge : ChromeColours.buttonBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Mute slice \(letter) on this phone only")
        .accessibilityAddTraits(level.muted ? .isSelected : [])
        .accessibilityIdentifier("\(place)Mute\(letter)")
    }

    private var slider: some View {
        GeometryReader { proxy in
            let thumb: CGFloat = 22
            let usable = max(proxy.size.width - thumb, 1)
            let x = CGFloat(shown) * usable
            ZStack(alignment: .leading) {
                Capsule().fill(ChromeColours.sliderTrack).frame(height: 4).padding(.horizontal, thumb / 2)
                Capsule().fill(colour).frame(width: x + thumb / 2, height: 4).padding(.leading, thumb / 2)
                Circle()
                    .fill(colour)
                    .overlay(Circle().strokeBorder(ChromeColours.panel, lineWidth: 3))
                    .frame(width: thumb, height: thumb)
                    .offset(x: x)
            }
            .frame(maxHeight: .infinity)
            .contentShape(Rectangle())
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { gesture in
                    let next = (min(max(Double((gesture.location.x - thumb / 2) / usable), 0), 1) * 100).rounded() / 100
                    if next != dragging {
                        dragging = next
                        setLevel(next, level.muted)
                    }
                }
                .onEnded { _ in dragging = nil })
        }
        .frame(height: 44)
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Your volume for slice \(letter), on this phone only")
        .accessibilityValue("\(Int((shown * 100).rounded()))")
        .accessibilityAdjustableAction { direction in
            let step = direction == .increment ? 0.05 : -0.05
            setLevel(min(max(level.level + step, 0), 1), level.muted)
        }
        .accessibilityIdentifier("\(place)Volume\(letter)")
    }

    private var value: some View {
        Text("\(Int((shown * 100).rounded()))")
            .font(.system(size: large ? 19 : 12).monospacedDigit())
            .foregroundStyle(ChromeColours.text)
            .frame(maxWidth: .infinity, minHeight: large ? 32 : 28)
            .background(ChromeColours.bar, in: RoundedRectangle(cornerRadius: 3))
            .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(SliceListStyle.valueEdge, lineWidth: 1))
            .accessibilityHidden(true)
    }
}

/// One row of the list.
private struct SliceListRow: View {
    @ObservedObject var model: SliceListModel
    @ObservedObject var slices: BandSlicesModel
    let row: SliceRoster.Row
    let large: Bool
    let close: () -> Void

    private var colour: Color { BandColours.slice(model.colour(row.id)) }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if model.isOwnHere(row) {
                ownButton(state: model.isActive(row) ? SliceListModel.activeTitle : SliceListModel.makeActiveTitle,
                          lit: model.isActive(row), identifier: "sliceListRow\(row.letter)") {
                    close()
                    model.makeActive(row)
                }
            } else {
                if model.showsBand(row) {
                    let showing = model.isActive(row)
                    ownButton(state: showing ? SliceListModel.activeTitle : SliceListModel.showBandTitle,
                              lit: showing, identifier: "sliceListShow\(row.letter)") {
                        close()
                        model.showBand(row)
                    }
                } else {
                    details
                }
                actions
                if let why = row.takeRefusal, row.relation != .control, !model.isTaking(row) {
                    Text(why)
                        .font(.system(size: large ? 19 : 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(.top, 6)
                        .accessibilityIdentifier("sliceListWhy\(row.letter)")
                }
                if row.relation == .listening {
                    YourVolumeRow(letter: row.letter, colour: colour, level: model.level(row), large: large,
                                  setLevel: { model.setLevel(row, level: $0, muted: $1) }, place: "row")
                }
            }
        }
        .padding(.leading, 12)
        .padding(.trailing, 10)
        .padding(.top, 8)
        .padding(.bottom, 10)
        .overlay(alignment: .leading) {
            RoundedRectangle(cornerRadius: 2).fill(colour).frame(width: 3).padding(.vertical, 8)
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("sliceListSlice\(row.letter)")
    }

    /// The line, the holder and who else listens.
    private var details: some View {
        VStack(alignment: .leading, spacing: 0) {
            line
            Text(row.holderLine)
                .font(.system(size: large ? 19 : 12.5, weight: .semibold))
                .foregroundStyle(row.relation == .listening ? colour : ChromeColours.text)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.top, 5)
                .accessibilityIdentifier("sliceListHolder\(row.letter)")
            if !row.alsoListening.isEmpty {
                Text(SliceRoster.alsoListeningText + row.alsoListening.joined(separator: ", "))
                    .font(.system(size: large ? 19 : 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.top, 2)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// Letter, frequency, mode, band, and where it is.
    private var line: some View {
        let mode = model.modeText(row)
        let parts = HStack(spacing: 8) {
            SliceLetterBadge(letter: row.letter, colour: colour, size: large ? 26 : 22, points: large ? 17 : 13)
            Text(model.frequencyText(row))
                .font(.system(size: large ? 21 : 17, weight: .bold, design: .monospaced))
                .foregroundStyle(ChromeColours.textBright)
                .lineLimit(1)
                .fixedSize()
            if !mode.isEmpty {
                Text(mode)
                    .font(.system(size: large ? 19 : 12, weight: .bold))
                    .foregroundStyle(RadeReception.modeLabels.contains(mode) ? SliceListStyle.radeMode : ChromeColours.text)
            }
            Text(model.bandText(row))
                .font(.system(size: large ? 19 : 12))
                .foregroundStyle(ChromeColours.textDim)
        }
        let here = row.here
        let pill = Text(here ? SliceRoster.thisPanText : SliceRoster.anotherPanText)
            .font(.system(size: large ? 19 : 11))
            .foregroundStyle(here ? ChromeColours.accent : SliceListStyle.whereText)
            .lineLimit(1)
            .fixedSize()
            .padding(.horizontal, 6)
            .padding(.vertical, 1)
            .overlay(Capsule().strokeBorder(here ? SliceListStyle.hereEdge : SliceListStyle.whereEdge, lineWidth: 1))
            .accessibilityIdentifier("sliceListWhere\(row.letter)")
        return ViewThatFits(in: .horizontal) {
            HStack(spacing: 8) {
                parts
                Spacer(minLength: 0)
                pill
            }
            VStack(alignment: .leading, spacing: 4) {
                parts
                pill
            }
        }
    }

    /// The row's top as one button: this phone's own slice, or Show its band.
    private func ownButton(state: String, lit: Bool, identifier: String,
                           action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 0) {
                details
                Text(state)
                    .font(.system(size: large ? 19 : 12, weight: .bold))
                    .foregroundStyle(lit ? colour : ChromeColours.text)
                    .padding(.horizontal, 8)
                    .frame(minWidth: 88, minHeight: large ? 30 : 26)
                    .background(lit ? Color.clear : ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(lit ? colour : ChromeColours.buttonBorder, lineWidth: 1))
                    .padding(.top, 6)
            }
            .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityElement(children: .combine)
        .accessibilityValue(state)
        .accessibilityAddTraits(lit ? .isSelected : [])
        .accessibilityIdentifier(identifier)
    }

    /// Listen or Stop listening with Take control; Release on one it controls.
    @ViewBuilder
    private var actions: some View {
        let working = model.isWorking(row)
        if row.relation == .control {
            SliceListButton(title: SliceListModel.releaseTitle, kind: working ? .greyed : .plain, large: large) {
                model.release(row)
            }
            .padding(.top, 8)
            .accessibilityIdentifier("sliceListRelease\(row.letter)")
        } else {
            let columns = Array(repeating: GridItem(.flexible(minimum: 0), spacing: 8), count: large ? 1 : 2)
            LazyVGrid(columns: columns, spacing: 8) {
                if row.relation == .listening {
                    SliceListButton(title: SliceListModel.stopListeningTitle, kind: working ? .greyed : .plain,
                                    large: large) {
                        model.stopListening(row)
                    }
                    .accessibilityIdentifier("sliceListStop\(row.letter)")
                } else {
                    SliceListButton(title: SliceListModel.listenTitle, kind: working ? .greyed : .plain,
                                    large: large) {
                        model.listen(row)
                    }
                    .accessibilityLabel("Listen to slice \(row.letter)")
                    .accessibilityIdentifier("sliceListListen\(row.letter)")
                }
                takeButton
            }
            .padding(.top, 8)
        }
    }

    private var takeButton: some View {
        let taking = model.isTaking(row)
        let refused = row.takeRefusal != nil
        return SliceListButton(title: taking ? VfoFlagView.takingControlTitle : VfoFlagView.takeControlTitle,
                               kind: taking || refused ? .greyed : .go, large: large) {
            if let why = row.takeRefusal {
                slices.showReason(why)
            } else if !taking {
                model.takeControl(row)
            }
        }
        .accessibilityLabel(taking ? VfoFlagView.takingControlTitle : "Take control of slice \(row.letter)")
        .accessibilityHint(row.takeRefusal ?? "")
        .accessibilityIdentifier("sliceListTake\(row.letter)")
    }
}
