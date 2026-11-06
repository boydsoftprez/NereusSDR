// NereusSDR for iOS: a pan's saved band stack entries as chips at the foot of the spectrum, tap to go there
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// Where the chips sit: a dot at the entry's frequency, in two rows along the
/// foot of the spectrum, above the band plan and left of the dBm scale.
enum BandStackMarkerLayout {
    /// One entry's chip.
    struct Chip: Equatable, Identifiable {
        let entry: BandStack.Entry
        let text: String
        /// The active slice stands on this entry.
        let active: Bool
        let rect: CGRect

        var id: String { entry.id }
    }

    /// Finger size, as the approved board draws them
    /// (docs/architecture/2026-11-05-band-stack-design/phone-and-ipad.html, `.mk`).
    static let height: CGFloat = 26
    static let rowStep: CGFloat = 29
    static let footGap: CGFloat = 4
    /// How far a chip's left edge sits before its frequency.
    static let leadIn: CGFloat = 5
    static let edgeGap: CGFloat = 2

    /// Where a refusal's note starts on a band this high: past the band
    /// column, which reaches down over the waterfall's top on an iPad and a
    /// phone on its side.
    @MainActor
    static func noteLeft(bandHeight: CGFloat, leadingInset: CGFloat) -> CGFloat {
        leadingInset + BandColumnLayout.origin
            + BandColumnLayout.columnWidth(columns: BandColumnLayout.columns(bandHeight: bandHeight))
            + BandColumnLayout.clearance
    }
    static let chipGap: CGFloat = 2
    static let dot: CGFloat = 7
    static let spacing: CGFloat = 4
    static let leadingPadding: CGFloat = 6
    static let trailingPadding: CGFloat = 7
    static let lockWidth: CGFloat = 9
    static let fontSize: CGFloat = 11
    /// One character of the chip's monospaced type, with a little to spare.
    static let characterWidth: CGFloat = 6.9
    /// The chips' two rows.
    static let rows = 2

    /// The chips show: the switch is on, the Core has a stack, and the radio
    /// is not on the air. Off, or on a Core without band stacks, nothing draws.
    static func shows(switchOn: Bool, availability: BandStackModel.Availability, onAir: Bool) -> Bool {
        switchOn && availability == .ready && !onAir
    }

    static func width(text: String, locked: Bool) -> CGFloat {
        leadingPadding + dot + spacing + CGFloat(text.count) * characterWidth
            + (locked ? spacing + lockWidth : 0) + trailingPadding
    }

    /// The entries in view, low to high. Each takes the first row with room
    /// at its frequency; where neither has, the one that ends sooner.
    static func chips(entries: [BandStack.Entry], isActive: (BandStack.Entry) -> Bool, geometry: BandGeometry,
                      layout: BandLayout) -> [Chip] {
        let shown = entries
            .filter { Double($0.frequencyHz) >= geometry.lowHz && Double($0.frequencyHz) <= geometry.highHz }
            .sorted { ($0.frequencyHz, $0.id) < ($1.frequencyHz, $1.id) }
        var rowEnds = [CGFloat](repeating: -.infinity, count: rows)
        return shown.map { entry in
            let text = BandStackBlock.frequencyText(entry.frequencyHz)
            let width = width(text: text, locked: entry.locked)
            let wanted = CGFloat(geometry.x(forHz: Double(entry.frequencyHz))) - leadIn
            let left = max(edgeGap, min(layout.strip.maxX - edgeGap - width, wanted))
            let row = rowEnds.firstIndex { $0 <= left }
                ?? rowEnds.indices.min { rowEnds[$0] < rowEnds[$1] } ?? 0
            rowEnds[row] = left + width + chipGap
            let top = layout.strip.minY - footGap - height - CGFloat(row) * rowStep
            return Chip(entry: entry, text: text, active: isActive(entry),
                        rect: CGRect(x: left, y: max(0, top), width: width, height: height))
        }
    }
}

extension BandStackModel {
    /// This pan's chips in the given view, drawn or not.
    func markerChips(geometry: BandGeometry, layout: BandLayout) -> [BandStackMarkerLayout.Chip] {
        BandStackMarkerLayout.chips(entries: stack?.entries ?? [],
                                    isActive: { activeSlicePlace(band: $0.band) == $0.id },
                                    geometry: geometry, layout: layout)
    }
}

/// The chips themselves (spec section 8, V6, V18), over the band and under the
/// flags. A tap goes to the entry on the pan's active slice. A drag that
/// starts on one is the band's, so a slice's line under a chip still drags.
/// What the Core refuses shows at the top of the waterfall, since the Pan sheet
/// that shows its notes is not open here.
struct BandStackMarkerLayer: View {
    @ObservedObject var model: BandStackModel
    let geometry: BandGeometry
    let layout: BandLayout
    var leadingInset: CGFloat = 0

    /// As the board's `.mk` and `.mk.act`.
    private static let face = Color(red: 12 / 255, green: 12 / 255, blue: 14 / 255).opacity(0.86)
    private static let rim = Color(red: 0x33 / 255, green: 0x50 / 255, blue: 0x6d / 255)
    private static let words = Color(red: 0xa9 / 255, green: 0xc7 / 255, blue: 0xe4 / 255)
    private static let dot = Color(red: 0x6e / 255, green: 0xa5 / 255, blue: 0xd8 / 255)
    private static let activeDot = Color(red: 0xd8 / 255, green: 0xa5 / 255, blue: 0x5f / 255)
    private static let activeRim = Color(red: 0x6b / 255, green: 0x54 / 255, blue: 0x26 / 255)
    private static let activeWords = Color(red: 0xec / 255, green: 0xd3 / 255, blue: 0xa9 / 255)
    private static let lock = Color(red: 0xc9 / 255, green: 0xa2 / 255, blue: 0x4a / 255)

    /// The note's widest.
    private static let noteMaxWidth: CGFloat = 240

    var body: some View {
        let chips = model.markerChips(geometry: geometry, layout: layout)
        ZStack(alignment: .topLeading) {
            ForEach(chips) { chip in
                button(chip)
                    .zIndex(chip.active ? 1 : 0)
            }
            if let note = model.note {
                noteView(note)
            }
        }
    }

    private func button(_ chip: BandStackMarkerLayout.Chip) -> some View {
        Button {
            model.goTo(chip.entry.id)
        } label: {
            HStack(spacing: BandStackMarkerLayout.spacing) {
                Circle()
                    .fill(chip.active ? Self.activeDot : Self.dot)
                    .frame(width: BandStackMarkerLayout.dot, height: BandStackMarkerLayout.dot)
                Text(chip.text)
                    .font(.system(size: BandStackMarkerLayout.fontSize, weight: .semibold, design: .monospaced))
                    .foregroundStyle(chip.active ? Self.activeWords : Self.words)
                    .lineLimit(1)
                if chip.entry.locked {
                    Image(systemName: "lock.fill")
                        .font(.system(size: 9))
                        .foregroundStyle(Self.lock)
                        .frame(width: BandStackMarkerLayout.lockWidth)
                }
            }
            .padding(.leading, BandStackMarkerLayout.leadingPadding)
            .padding(.trailing, BandStackMarkerLayout.trailingPadding)
            .frame(width: chip.rect.width, height: chip.rect.height, alignment: .leading)
            .background(Self.face, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
            .overlay {
                RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius)
                    .strokeBorder(chip.active ? Self.activeRim : Self.rim, lineWidth: 1)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Self.words(chip))
        .accessibilityAddTraits(chip.active ? .isSelected : [])
        .accessibilityIdentifier("bandStackMarker.\(chip.id)")
        .offset(x: chip.rect.minX, y: chip.rect.minY)
    }

    /// What VoiceOver reads for a chip.
    static func words(_ chip: BandStackMarkerLayout.Chip) -> String {
        var parts = ["Saved spot", chip.text, chip.entry.mode]
        if !chip.entry.name.isEmpty {
            parts.append(chip.entry.name)
        }
        if chip.entry.locked {
            parts.append("locked")
        }
        if chip.active {
            parts.append("active")
        }
        return parts.joined(separator: ", ")
    }

    private func noteView(_ note: String) -> some View {
        let left = BandStackMarkerLayout.noteLeft(bandHeight: layout.size.height, leadingInset: leadingInset)
        return HStack(alignment: .top, spacing: 6) {
            Text(note)
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.refusalText)
                .fixedSize(horizontal: false, vertical: true)
                .frame(maxWidth: .infinity, alignment: .leading)
                .accessibilityIdentifier("bandStackMarkerNote")
            Button {
                model.note = nil
            } label: {
                Image(systemName: "xmark")
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(ChromeColours.textDim)
                    .frame(width: 28, height: 28)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Close this note")
            .accessibilityIdentifier("bandStackMarkerNoteClose")
        }
        .padding(.leading, 8)
        .background(Self.face, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
        .frame(width: min(Self.noteMaxWidth, max(0, layout.strip.maxX - left - BandStackMarkerLayout.edgeGap)))
        // At the top of the waterfall: a flag hanging down the spectrum would cover it there.
        .offset(x: left, y: layout.waterfall.minY + BandStackMarkerLayout.footGap)
    }
}

/// The per-device Band stack button in the older Display sheet's "On the band"
/// group, which a Core without the pill shows (V18).
struct BandStackMarkerSheetButton: View {
    @ObservedObject var model: BandStackModel
    @ObservedObject var settings: PhoneSettings

    var body: some View {
        PanelButton(label: "Band stack", lit: settings.showBandStackMarkers && model.availability == .ready,
                    style: .dsp, disabled: model.unavailableReason != nil) {
            settings.showBandStackMarkers.toggle()
        }
        .accessibilityValue(settings.showBandStackMarkers ? DescribedControl.onText : DescribedControl.offText)
        .accessibilityHint(model.unavailableReason ?? "")
        .accessibilityIdentifier("bandStackMarkersSwitch")
    }
}

/// The line under the sheet's group: what the button does, or why it is grey.
struct BandStackMarkerSheetNote: View {
    static let onBandNote = "Band stack marks your saved spots on the band. Tap one to go there."

    @ObservedObject var model: BandStackModel

    var body: some View {
        SheetNote(model.unavailableReason.map { "Band stack: \($0)" } ?? Self.onBandNote)
            .accessibilityIdentifier("bandStackMarkersNote")
    }
}
