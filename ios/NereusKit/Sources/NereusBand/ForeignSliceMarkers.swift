// NereusSDR for iOS: another device's slices on this band: dashed, hollow and out of reach
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics

/// Another device's slices on this band (D46, R-IOS-17, spec section 5.8
/// item 1, picture 22): each drawn read-only, so nothing on it invites a
/// drag. Its centre line is dashed in its letter's colour, its triangle is
/// hollow (the band's background inside a ring of that colour), its
/// passband's two edges are dashed grey, and the passband is not shaded.
/// It has no flag: a label at the foot of the spectrum names it
/// (``labelX(centreX:labelWidth:bandWidth:)`` keeps the label inside the
/// band, centred on the line where it fits).
///
/// The colour is the Core's catalogue's for the slice's letter, the same
/// on every device, so no colour table lives in the app (D4).
public enum ForeignSliceMarkers {
    /// One slice another device owns, as the Core's marker describes it.
    public struct Slice: Equatable, Sendable, Identifiable {
        /// The slice's number on the Core; its letter is 'A' + id.
        public var id: Int
        public var frequencyHz: Double
        /// The passband's edges from the centre, signed as the slice's.
        public var filterLowHz: Double
        public var filterHighHz: Double
        /// `#RRGGBB`, the catalogue's colour for the slice's letter.
        public var colour: String
        /// The owner's name and short name, as the Core sends them.
        public var ownerName: String
        public var ownerShortName: String
        /// The owner's kind, as the Core sends it: `station` for a slice
        /// the Core itself holds or no device owns, else the device's
        /// (`phone`, `tablet`, `computer`).
        public var ownerKind: String
        /// Its owner has transmit and transmits on it.
        public var txSlice: Bool
        /// Its owner is away (its link dropped), or the slice is held for it.
        public var away: Bool

        public init(id: Int, frequencyHz: Double, filterLowHz: Double, filterHighHz: Double, colour: String,
                    ownerName: String, ownerShortName: String, ownerKind: String = "", txSlice: Bool = false,
                    away: Bool = false) {
            self.id = id
            self.frequencyHz = frequencyHz
            self.filterLowHz = filterLowHz
            self.filterHighHz = filterHighHz
            self.colour = colour
            self.ownerName = ownerName
            self.ownerShortName = ownerShortName
            self.ownerKind = ownerKind
            self.txSlice = txSlice
            self.away = away
        }

        public var letter: String { BandSlice.letter(forIndex: id) }

        /// The passband's low and high frequencies, low first.
        public var passbandHz: ClosedRange<Double> {
            let a = frequencyHz + filterLowHz
            let b = frequencyHz + filterHighHz
            return min(a, b)...max(a, b)
        }

        /// The name the label shows: the device's short name, or its name
        /// when the Core sent no short name, each as sent. For an owner the
        /// Core sent no name for, who holds it in the Core's words
        /// (``holderWords``): "the Core", "a phone", "another device".
        public var labelName: String {
            if ownerNamed {
                return ownerShortName.isEmpty ? ownerName : ownerShortName
            }
            return holderWords
        }

        /// Who holds the slice, as the Core names a slice's holder in its
        /// refusals (NereusSDR src/core/session/StationServer.cpp:9059-9083
        /// `sliceHolderWords`, and the desktop's own copy in
        /// src/core/session/SliceAccessMirror.cpp:180-198, at c13abe564):
        /// the device's own name, as sent; "the Core" for a slice the Core
        /// holds itself or no device holds (the marker's kind `station`);
        /// "a phone", "a tablet" or "a computer" for a device sent with no
        /// name; "another device" when neither is known. Never empty.
        public var holderWords: String {
            if ownerIsCore {
                return Self.coreWords
            }
            if !ownerName.isEmpty {
                return ownerName
            }
            if !ownerShortName.isEmpty {
                return ownerShortName
            }
            if !ownerKind.isEmpty {
                return "a " + Self.kindWord(ownerKind).lowercased()
            }
            return Self.anotherDevice
        }

        /// The owner is the Core itself (its kind `station`), not a device.
        public var ownerIsCore: Bool { ownerKind == Self.stationKind }

        /// The Core sent a name for the owner, so the label shows the
        /// owner's own name rather than the Core's words for a holder.
        public var ownerNamed: Bool { !ownerIsCore && !(ownerName.isEmpty && ownerShortName.isEmpty) }

        /// The kind the Core gives a slice it holds itself or no device owns
        /// (NereusSDR src/core/session/StationServer.cpp:3295-3297 at c13abe564).
        public static let stationKind = "station"
        /// The Core's words for itself as a slice's holder.
        public static let coreWords = "the Core"
        /// The Core's words for a holder whose name and kind it does not know.
        public static let anotherDevice = "another device"

        /// A device's kind as the Core words it (DeviceSessionRegistry::kindWord,
        /// NereusSDR src/core/session/DeviceSessionRegistry.cpp:411-420 at
        /// c13abe564): "Phone", "Tablet", and "Computer" for any other kind.
        public static func kindWord(_ kind: String) -> String {
            switch kind {
            case "phone":
                return "Phone"
            case "tablet":
                return "Tablet"
            default:
                return "Computer"
            }
        }
    }

    /// The centre line's opacity, the board's 0.85.
    public static let lineAlpha: Float = 0.85
    /// The edges: the band's secondary grey at the board's 0.55.
    public static let edgeColour = "#8090A0"
    public static let edgeAlpha: Float = 0.55
    /// The inside of the hollow triangle: the band's background.
    public static let triangleInside = SIMD4<Float>(10 / 255, 10 / 255, 20 / 255, 1)
    /// The triangle's ring, in points.
    public static let triangleRingPoints: CGFloat = 1.5
    /// The centre line's dashes, in points: 2 wide, 6 drawn, 4 left out.
    public static let lineDashPoints: CGFloat = 6
    public static let lineGapPoints: CGFloat = 4
    /// The edges' dashes, in points: 1 wide, 3 drawn, 3 left out.
    public static let edgeDashPoints: CGFloat = 3
    public static let edgeGapPoints: CGFloat = 3
    /// How far the label keeps from the band's sides, in points.
    public static let labelInsetPoints: CGFloat = 4

    /// The style of another device's slice drawn in `colour` (`#RRGGBB`).
    public static func style(for colour: String) -> SliceMarkers.Style {
        let own = BandPalette.rgba(colour) ?? BandPalette.rgba(BandSlice.colourUnknown) ?? SIMD4(0.5, 0.5, 0.5, 1)
        let grey = BandPalette.rgba(edgeColour) ?? SIMD4(0.5, 0.56, 0.63, 1)
        return SliceMarkers.Style(line: SliceMarkers.withAlpha(own, lineAlpha), triangle: SliceMarkers.withAlpha(own, 1),
                                  edge: SliceMarkers.withAlpha(grey, edgeAlpha), selected: false)
    }

    /// The markers for other devices' slices, their triangles at the top
    /// of the band (they have no flag), each unshaded. The renderer draws
    /// them under this device's own markers.
    public static func markers(_ slices: [Slice]) -> [SliceMarkers.Marker] {
        slices.sorted { $0.id < $1.id }.map { slice in
            SliceMarkers.Marker(sliceId: slice.id, centerHz: slice.frequencyHz, passbandHz: slice.passbandHz,
                                style: style(for: slice.colour), triangleTopPoints: 0, showsPassband: false,
                                foreign: true)
        }
    }

    /// Where a label `labelWidth` wide starts, in points, so it is centred
    /// on its line at `centreX` and kept inside a band `bandWidth` wide.
    public static func labelX(centreX: CGFloat, labelWidth: CGFloat, bandWidth: CGFloat) -> CGFloat {
        let highest = max(labelInsetPoints, bandWidth - labelWidth - labelInsetPoints)
        return min(max(centreX - labelWidth / 2, labelInsetPoints), highest)
    }
}
