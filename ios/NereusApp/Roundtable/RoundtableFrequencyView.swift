// NereusSDR for iOS: Roundtable's view of one frequency, shared by the sheet, the iPad's applet and its popover
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-11-03: Overlay redesign design D21 to D29.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-11-04: Tune is off when it would write nothing, or when no mode reaches the family.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import NereusModels
import SwiftUI

/// The words and rules of a frequency view, apart from any view (overlay design D22).
enum RoundtableFrequencyText {
    /// "Viewing, not tuned · −11.4 kHz from slice A · LSB".
    static func sub(hz: Int64, family: String, sliceHz: Int64?, sliceLetter: String) -> String {
        var text = "Viewing, not tuned"
        if let sliceHz {
            let away = Double(hz - sliceHz) / 1000
            if abs(away) >= 0.05 {
                text += " \u{00B7} \(away > 0 ? "+" : "\u{2212}")\(String(format: "%.1f", abs(away))) kHz from slice \(sliceLetter)"
            }
        }
        return text + " \u{00B7} " + RoundtableText.modeLabel(family)
    }

    /// "on freq" at zero, else "+20 Hz" or "−40 Hz".
    static func offset(_ hz: Int64) -> String {
        hz == 0 ? "on freq" : "\(hz > 0 ? "+" : "\u{2212}")\(abs(hz)) Hz"
    }

    /// "179 mi 039°", in the station's units, with the bearing if it wants one.
    static func path(km: Int64?, bearing: Int64?, units: String, showBearing: Bool) -> String {
        var parts: [String] = []
        if km != nil {
            parts.append(RoundtableText.distance(km, units: units) + (units == "km" ? " km" : " mi"))
        }
        if showBearing, let bearing, (0..<360).contains(bearing) {
            parts.append(String(format: "%03d\u{00B0}", Int(bearing)))
        }
        return parts.joined(separator: " ")
    }

    static func head(_ card: RoundtableCardData) -> String {
        switch card.kind {
        case .quiet, .lastTalker: "Nobody talking"
        case .unknown: "Unknown signal"
        case .talking, .you, .double, .lastDouble: RoundtableTagLayout.cardHead(card, narrow: false)
        }
    }

    /// The note under the head: the link's (D7) with "last known", else the card's own.
    static func link(_ card: RoundtableCardData) -> String? {
        guard let note = card.note else {
            return nil
        }
        return card.frozen ? note + " \u{00B7} last known" : note
    }

    /// D2: combined RF, and a signal with nobody named, have no path.
    static func showsPath(_ card: RoundtableCardData) -> Bool {
        card.kind != .double && card.kind != .lastDouble && card.kind != .unknown
    }

    /// D22, D23: Your pan first when someone is on the air, then the readings.
    /// It speaks only for now, so a frozen card has none (D7).
    static func rows(_ card: RoundtableCardData, panDbm: Double?, panPeak: Double?) -> [RoundtableCardData.Row] {
        let onAir = card.kind == .talking || card.kind == .you || card.kind == .double || card.kind == .unknown
        guard onAir, !card.frozen, let panDbm else {
            return card.rows
        }
        return [RoundtableCardData.Row(call: "pan", dbm: panDbm, age: 0, km: nil, bearing: nil, offsetHz: 0, you: true,
                                       old: false, peak: panPeak, label: "Your pan")] + card.rows
    }

    /// D22: Tune is off while keyed, when already there, and when the radio's
    /// own connection or the slice itself will not take it. "Already there"
    /// counts the family: a Tune from an LSB view moves a USB slice's mode too.
    /// A FreeDV view writes no mode, so from any family on the same frequency it
    /// has nothing to do. `familyReachable` is false when getting to this family
    /// needs a mode the Core's catalog does not offer (or has not sent).
    static func canTune(hz: Int64, family: String, sliceHz: Int64?, sliceFamily: String?, keyed: Bool, tunable: Bool,
                        linkUp: Bool, familyReachable: Bool = true) -> Bool {
        let writesMode = family != sliceFamily && family != "fdvu" && family != "fdvl"
        return !keyed && tunable && linkUp && familyReachable && sliceHz != nil && (hz != sliceHz || writesMode)
    }
}

/// One frequency: the head, how far it is from your slice, who is on it and
/// what each listener reads, then Tune. Your own frequency reads the same with
/// `viewing` false, and has no Tune (overlay design D22, D28; overlay.js peekBody).
struct RoundtableFrequencyView: View {
    let data: RoundtableCardData
    let viewing: Bool
    let canTune: Bool
    var sliceHz: Int64? = nil
    var sliceLetter = "A"
    var format: RoundtableSignalFormat = .dbm
    var units = "mi"
    var showBearing = true
    var panDbm: Double? = nil
    var panPeak: Double? = nil
    var reconnecting = false
    /// Draws Back to my slice at the top; off where a header holds it.
    var showsBack = true
    /// Sits between Back and the head: the applet's frequency line, live line and rail.
    var banner: AnyView? = nil
    /// Off where the Tune is pinned outside the scrolling head.
    var showsTune = true
    let onTune: () -> Void
    let onBack: () -> Void

    private static let subText = Color(red: 0x79 / 255, green: 0xA8 / 255, blue: 0xD2 / 255)
    private static let personText = Color(red: 0xC8 / 255, green: 0xC8 / 255, blue: 0xCE / 255)
    private var isDouble: Bool { data.kind == .double }

    private var glyph: RoundtableFlagLine.Glyph {
        switch data.kind {
        case .talking: .talking
        case .you: .you
        case .unknown: .unknown
        case .double: .doubled
        case .lastDouble, .lastTalker, .quiet: .quiet
        }
    }

    private var whoColour: Color {
        if data.frozen { return RoundtableCardView.lastText }
        switch data.kind {
        case .double: return RoundtableCardView.doubleText
        case .you: return RoundtableColours.you
        case .unknown: return RoundtableColours.unknown
        case .lastTalker, .lastDouble, .quiet: return RoundtableCardView.lastText
        case .talking: return RoundtableTagLayer.labelColour
        }
    }

    private var isLive: Bool {
        data.kind == .talking || data.kind == .you || data.kind == .double || data.kind == .unknown
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if viewing && showsBack {
                RoundtableBackButton(action: onBack)
            }
            if let banner {
                banner
            }
            VStack(alignment: .leading, spacing: 5) {
                head
                if viewing {
                    Text(RoundtableFrequencyText.sub(hz: data.hz, family: data.family, sliceHz: sliceHz,
                                                     sliceLetter: sliceLetter))
                        .font(.system(size: 10, weight: .medium))
                        .foregroundStyle(Self.subText)
                        .lineLimit(1)
                        .truncationMode(.tail)
                        .accessibilityIdentifier("roundtableViewSub")
                }
                if let link = RoundtableFrequencyText.link(data) {
                    Text(link)
                        .font(.system(size: 10, weight: .medium))
                        .foregroundStyle(reconnecting ? RoundtableColours.unknown : RoundtableColours.dim)
                        .lineLimit(1)
                }
                caption("On frequency \u{00B7} \(data.onFrequency.count)", trailing: "Tuned", colour: RoundtableColours.dim)
                people
                readings
                if viewing && showsTune {
                    tuneButton
                }
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 7)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableFrequencyView")
    }

    private var head: some View {
        HStack(spacing: 5) {
            RoundtableGlyphView(glyph: glyph, size: RoundtableTagLayout.glyphSize)
                .saturation(data.frozen || data.kind == .lastDouble ? 0.15 : 1)
            Text(RoundtableFrequencyText.head(data))
                .font(.system(size: 12, weight: .bold))
                .foregroundStyle(whoColour)
                .lineLimit(1)
                .truncationMode(.tail)
            // A dropped link has no live clock (D7).
            if isLive && !data.frozen, let clock = RoundtableTagLayout.cardClockText(data) {
                Text(clock)
                    .font(.system(size: 10, weight: .semibold, design: .monospaced))
                    .foregroundStyle(RoundtableColours.dim)
                    .fixedSize()
                    .accessibilityIdentifier("roundtableViewClock")
            }
            Spacer(minLength: 0)
            Text(RoundtableTagLayout.cardFrequency(data.hz))
                .font(.system(size: 9.5, weight: .semibold, design: .monospaced))
                .foregroundStyle(RoundtableColours.accentText)
                .fixedSize()
        }
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("roundtableViewHead")
    }

    private func caption(_ text: String, trailing: String, colour: Color, bold: Bool = false) -> some View {
        HStack(spacing: 6) {
            Text(text)
                .font(.system(size: 9.5, weight: bold ? .semibold : .medium))
                .foregroundStyle(colour)
                .lineLimit(1)
            Spacer(minLength: 0)
            Text(trailing)
                .font(.system(size: 9.5, weight: .medium))
                .foregroundStyle(RoundtableColours.dim)
                .fixedSize()
        }
        .padding(.top, 4)
        .overlay(alignment: .top) {
            Rectangle().fill(RoundtableTagLayer.listRule).frame(height: 1)
        }
    }

    @ViewBuilder
    private var people: some View {
        if data.onFrequency.isEmpty {
            note("No stations named here")
        } else {
            VStack(alignment: .leading, spacing: 2) {
                ForEach(Array(data.onFrequency.enumerated()), id: \.offset) { _, person in
                    HStack(spacing: 5) {
                        RoundtableGlyphView(glyph: person.keyed ? (isDouble ? .doubled : .talking) : .quiet,
                                            size: RoundtableTagLayout.glyphSize)
                            .saturation(data.frozen ? 0.15 : 1)
                            .frame(width: 11)
                        // A call that does not fit is cut, never overlapped.
                        Text(person.you ? "You" : person.name)
                            .font(.system(size: 11, weight: .semibold))
                            .foregroundStyle(person.keyed && !data.frozen ? RoundtableColours.text : Self.personText)
                            .lineLimit(1)
                            .fixedSize()
                            .frame(minWidth: 0, maxWidth: .infinity, alignment: .leading)
                            .clipped()
                        if person.keyed {
                            Text("keyed")
                                .font(.system(size: 10, weight: .medium))
                                .foregroundStyle(data.frozen ? RoundtableColours.dim
                                                 : isDouble ? RoundtableCardView.doubleText : RoundtableColours.talking)
                                .fixedSize()
                        }
                        Text(RoundtableFrequencyText.offset(person.offsetHz))
                            .font(.system(size: 10, weight: .medium, design: .monospaced))
                            .foregroundStyle(RoundtableColours.dim)
                            .frame(minWidth: 46, alignment: .trailing)
                            .fixedSize()
                    }
                    .frame(height: 14)
                    .accessibilityElement(children: .combine)
                    .accessibilityIdentifier("roundtableViewPerson")
                }
            }
        }
    }

    @ViewBuilder
    private var readings: some View {
        let live = isDouble && !data.frozen
        caption(RoundtableTagLayout.cardCaption(data, narrow: false),
                trailing: RoundtableTagLayout.cardFormatTitle(format),
                colour: live ? RoundtableCardView.doubleText : RoundtableColours.dim, bold: live)
        let rows = RoundtableFrequencyText.rows(data, panDbm: panDbm, panPeak: panPeak)
        if rows.isEmpty {
            note(data.note?.hasPrefix("Double ended") == true ? "Waiting for readings" : "No readings yet")
        } else {
            let showsPath = RoundtableFrequencyText.showsPath(data)
            VStack(spacing: RoundtableCardPlan.gap) {
                ForEach(Array(rows.enumerated()), id: \.offset) { _, row in
                    RoundtableCardRow(row: row, kind: data.kind, frozen: data.frozen, format: format,
                                      narrow: false, tight: false,
                                      path: showsPath && row.label == nil
                                          ? RoundtableFrequencyText.path(km: row.km, bearing: row.bearing, units: units,
                                                                         showBearing: showBearing)
                                          : nil)
                        .accessibilityElement(children: .combine)
                        .accessibilityIdentifier("roundtableViewRow")
                }
            }
        }
    }

    private func note(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 10, weight: .medium))
            .foregroundStyle(RoundtableColours.dim)
            .lineLimit(1)
    }

    private var tuneButton: some View {
        RoundtableFrequencyTuneButton(sliceLetter: sliceLetter, canTune: canTune, action: onTune)
    }
}

/// "Tune slice A here", in the view or pinned under it.
struct RoundtableFrequencyTuneButton: View {
    let sliceLetter: String
    let canTune: Bool
    let action: () -> Void

    private static let tuneFill = Color(red: 0x2F / 255, green: 0x5F / 255, blue: 0x92 / 255)
    private static let tuneEdge = Color(red: 0x5E / 255, green: 0x8D / 255, blue: 0xB8 / 255)

    var body: some View {
        Button(action: action) {
            Text("Tune slice \(sliceLetter) here")
                .font(.system(size: 11, weight: .semibold))
                .foregroundStyle(Color.white)
                .frame(maxWidth: .infinity, minHeight: 28)
                .background(Self.tuneFill, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(Self.tuneEdge, lineWidth: 1))
                .frame(minHeight: 44)
                .contentShape(Rectangle())
                .opacity(canTune ? 1 : 0.45)
        }
        .buttonStyle(.plain)
        .disabled(!canTune)
        .accessibilityIdentifier("roundtableViewTune")
    }
}

/// Tune pinned above the composer, so a half-height sheet always reaches it while
/// the view scrolls behind.
struct RoundtableFrequencyTuneBar: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var slices: BandSlicesModel
    let contextId: String
    let keyed: Bool
    let linkUp: Bool
    let onTune: (Int64, String) -> Void

    var body: some View {
        let data = roundtable.cardData(context: contextId)
        let active = slices.active
        RoundtableFrequencyTuneButton(
            sliceLetter: active?.slice.letter ?? "A",
            canTune: RoundtableFrequencyHost.canTune(data, active: active, keyed: keyed,
                                                     tunable: active.map(slices.isTunable) ?? false, linkUp: linkUp,
                                                     modes: slices.catalog?.offeredModes ?? []),
            action: { onTune(data.hz, data.family) })
            .padding(.horizontal, 10)
            .background(RoundtableColours.ground)
            .overlay(alignment: .top) {
                Rectangle().fill(RoundtableTagLayer.listRule).frame(height: 1)
            }
            // A container, so the room's own identifier does not replace the button's.
            .accessibilityElement(children: .contain)
    }
}

/// "‹ Back to my slice": a bar over a column's room, or a chip in a sheet's header.
struct RoundtableBackButton: View {
    var chip = false
    let action: () -> Void

    private static let fill = Color(red: 0x1E / 255, green: 0x3D / 255, blue: 0x5F / 255)
    private static let edge = Color(red: 0x5E / 255, green: 0x8D / 255, blue: 0xB8 / 255)
    private static let text = Color(red: 0xDC / 255, green: 0xDC / 255, blue: 0xE1 / 255)

    var body: some View {
        Button(action: action) {
            Group {
                if chip {
                    Text("\u{2039} Back to my slice")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(Self.text)
                        .padding(.horizontal, 10)
                        .frame(height: 30)
                        .background(Self.fill, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(Self.edge, lineWidth: 1))
                        .frame(minHeight: 44)
                } else {
                    Text("\u{2039} Back to my slice")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(Self.text)
                        .padding(.horizontal, 10)
                        .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
                        .background(Self.fill)
                }
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("roundtableBackToSlice")
    }
}

/// The 3 point rail under the applet's live line (overlay.css .rci-rail).
struct RoundtableFrequencyRail: View {
    let data: RoundtableCardData

    var body: some View {
        Group {
            if data.kind == .double && !data.frozen {
                RoundtableStripes(on: RoundtableCardView.doubleText, off: RoundtableCardView.doubleOff,
                                  onWidth: 5, offWidth: 4)
            } else {
                Rectangle().fill(RoundtableCardView.colour(data))
            }
        }
        .frame(height: RoundtableCardPlan.rail)
        .accessibilityHidden(true)
    }
}

/// A frequency's view fed from the model: its card about once a second as ever,
/// Your pan about five times a second, and only while it is open (D23).
struct RoundtableFrequencyHost: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var slices: BandSlicesModel
    let contextId: String
    let viewing: Bool
    let keyed: Bool
    let linkUp: Bool
    /// This client's pan at a frequency and family.
    var panLevel: ((Int64, String) -> Double?)? = nil
    var showsBack = true
    /// The applet's lines above the head, which read the same card.
    var banner: ((RoundtableCardData) -> AnyView)? = nil
    /// Off where the caller pins a RoundtableFrequencyTuneBar instead.
    var showsTune = true
    let onTune: (Int64, String) -> Void
    let onBack: () -> Void

    /// Whether Tune can act on this card from this slice.
    @MainActor
    static func canTune(_ data: RoundtableCardData, active: BandSlicesModel.Entry?, keyed: Bool, tunable: Bool,
                        linkUp: Bool, modes: [StationCatalog.Mode]) -> Bool {
        RoundtableFrequencyText.canTune(hz: data.hz, family: data.family,
                                        sliceHz: active.map { Int64($0.slice.frequencyHz.rounded()) },
                                        sliceFamily: active.flatMap { RoundtableModel.family(ofMode: $0.modeLabel) },
                                        keyed: keyed, tunable: tunable, linkUp: linkUp,
                                        familyReachable: active.map {
                                            BandSlicesModel.familyReachable(family: data.family, sliceMode: $0.modeLabel, modes: modes)
                                        } ?? false)
    }

    /// D23: about five times a second.
    static let panPeriodSeconds = 0.2

    /// Your pan's peak hold is on the card's rule: it restarts for each frequency.
    private func panPeak(_ dbm: Double, hz: Int64, now: Int64) -> Double {
        roundtable.peaks.update(sourceRoom: "yourPan", ref: "pan", turn: hz, dbm: dbm, now: now)
    }

    var body: some View {
        TimelineView(.periodic(from: .now, by: Self.panPeriodSeconds)) { context in
            let data = roundtable.cardData(context: contextId)
            let active = slices.active
            let sliceHz = active.map { Int64($0.slice.frequencyHz.rounded()) }
            let dbm = keyed || data.frozen ? nil : panLevel?(data.hz, data.family)
            RoundtableFrequencyView(
                data: data, viewing: viewing,
                canTune: Self.canTune(data, active: active, keyed: keyed,
                                      tunable: active.map(slices.isTunable) ?? false, linkUp: linkUp,
                                      modes: slices.catalog?.offeredModes ?? []),
                sliceHz: sliceHz, sliceLetter: active?.slice.letter ?? "A", format: roundtable.signalFormat,
                units: roundtable.distanceUnits, showBearing: roundtable.showBearing,
                panDbm: dbm,
                panPeak: dbm.map { panPeak($0, hz: data.hz, now: Int64(context.date.timeIntervalSince1970 * 1000)) },
                reconnecting: roundtable.linkState == .reconnecting, showsBack: showsBack,
                banner: banner?(data), showsTune: showsTune, onTune: { onTune(data.hz, data.family) }, onBack: onBack)
        }
    }
}

/// The room under a frequency's view: read only messages until you tune here.
/// A floating panel over the pan when an iPad's column is not showing
/// (overlay.css .scr--ipad .rc-psheet): the band beneath it still tunes, and
/// tuning closes it, so it is not a modal popover.
struct RoundtableFrequencyPopover: View {
    @ObservedObject var roundtable: RoundtableModel
    @ObservedObject var slices: BandSlicesModel
    @ObservedObject var transmit: TransmitModel
    let stationCallsign: String
    let linkUp: Bool
    var panLevel: ((Int64, String) -> Double?)? = nil
    let onTune: (Int64, String) -> Void
    let onBack: () -> Void

    static let width: CGFloat = 370

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                RoundtableBackButton(chip: true, action: onBack)
                Spacer(minLength: 0)
                Button(action: onBack) {
                    Image(systemName: "xmark")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(RoundtableColours.text)
                        .frame(width: 30, height: 30)
                        .background(Color.white.opacity(0.08), in: Circle())
                        .frame(width: 44, height: 44)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Close")
                .accessibilityIdentifier("roundtableViewClose")
            }
            .padding(.leading, 10)
            .padding(.trailing, 4)
            if let room = roundtable.room {
                RoundtableRoomView(roundtable: roundtable, room: room, full: true, keyed: transmit.ptt.transmitting,
                                   keyedSince: nil, keyedForSeconds: 0, tunedHere: roundtable.tunedToShownRoom,
                                   stationCallsign: stationCallsign, ownDbm: nil,
                                   sliceLetter: slices.active?.slice.letter ?? "A",
                                   frequencyHead: AnyView(
                                    RoundtableFrequencyHost(roundtable: roundtable, slices: slices, contextId: room.id,
                                                            viewing: true, keyed: transmit.ptt.transmitting,
                                                            linkUp: linkUp, panLevel: panLevel, showsBack: false,
                                                            showsTune: false, onTune: { _, _ in }, onBack: onBack)),
                                   frequencyFoot: AnyView(
                                    RoundtableFrequencyTuneBar(roundtable: roundtable, slices: slices, contextId: room.id,
                                                               keyed: transmit.ptt.transmitting, linkUp: linkUp,
                                                               onTune: onTune)),
                                   showsEarlier: false, viewing: true)
            }
        }
        .frame(width: Self.width)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(RoundtableColours.ground)
        .clipShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).strokeBorder(RoundtableTagLayer.listBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.5), radius: 12, y: 2)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableFrequencyPopover")
    }
}
