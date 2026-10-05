// NereusSDR for iOS: the six vintage S-meter faces' colours, the desktop's values
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One vintage face's colours (D86, D83: the desktop's values; the phone
/// draws them with its own code). Each colour is `0xRRGGBB`.
public struct VintageFaceTheme: Equatable, Sendable {
    public let name: String
    /// The bezel's three rings.
    public let bezelDark: UInt32
    public let bezelMid: UInt32
    public let bezelLight: UInt32
    /// The card's gradient, top to bottom.
    public let faceTop: UInt32
    public let faceBottom: UInt32
    /// The scale's and lettering's ink, and the minor ticks' softer ink.
    public let ink: UInt32
    public let inkSoft: UInt32
    /// The mirror strip under the scale, and its opacity out of 255 (0: none).
    public let mirror: UInt32
    public let mirrorOpacity: Int
    /// The pivot's cap and its screw's gradient.
    public let cap: UInt32
    public let screwLight: UInt32
    public let screwDark: UInt32
    /// The band colours.
    public let red: UInt32
    public let green: UInt32
    public let yellow: UInt32
    public let pointer: UInt32
    /// The glass's highlight across the card's upper left.
    public let glare: Bool

    public static let agedCream = VintageFaceTheme(
        name: "Aged Cream", bezelDark: 0x6E5322, bezelMid: 0xA88434, bezelLight: 0xD9B65A,
        faceTop: 0xEFE3C8, faceBottom: 0xD3C39C, ink: 0x1E1A16, inkSoft: 0x4A4238,
        mirror: 0xC9C4B8, mirrorOpacity: 180, cap: 0x1E1A16, screwLight: 0xD9B65A, screwDark: 0x6E5322,
        red: 0xB3261E, green: 0x3D7A44, yellow: 0xD9A400, pointer: 0x1A1512, glare: true)

    public static let vuAmber = VintageFaceTheme(
        name: "VU Amber", bezelDark: 0x141311, bezelMid: 0x2C2A26, bezelLight: 0x3E3B36,
        faceTop: 0xF3DF9C, faceBottom: 0xE2C674, ink: 0x14110E, inkSoft: 0x4A3F2A,
        mirror: 0x000000, mirrorOpacity: 0, cap: 0x14110E, screwLight: 0x8A8A8A, screwDark: 0x3A3A3A,
        red: 0xD0341E, green: 0x3C7A3E, yellow: 0xC98F00, pointer: 0x14110E, glare: true)

    public static let collinsWhite = VintageFaceTheme(
        name: "Collins White", bezelDark: 0x101010, bezelMid: 0x262626, bezelLight: 0x383838,
        faceTop: 0xF5F2EB, faceBottom: 0xE7E2D6, ink: 0x101010, inkSoft: 0x454545,
        mirror: 0xDADADA, mirrorOpacity: 130, cap: 0x141414, screwLight: 0xC8C8C8, screwDark: 0x505050,
        red: 0xB02020, green: 0x2F7D3A, yellow: 0xC99700, pointer: 0x101010, glare: false)

    public static let blackface = VintageFaceTheme(
        name: "Blackface", bezelDark: 0x2A2A2A, bezelMid: 0x4A4A4A, bezelLight: 0x6C6C6C,
        faceTop: 0x1F1F1F, faceBottom: 0x0C0C0C, ink: 0xF0EDE0, inkSoft: 0xB8B4A6,
        mirror: 0x000000, mirrorOpacity: 0, cap: 0x3A3A3A, screwLight: 0xD8D8D8, screwDark: 0x606060,
        red: 0xE0402A, green: 0x58C060, yellow: 0xFFD60A, pointer: 0xF2EDDC, glare: true)

    public static let carbon = VintageFaceTheme(
        name: "Carbon", bezelDark: 0x1A1D21, bezelMid: 0x3A3F46, bezelLight: 0x596069,
        faceTop: 0x1C2026, faceBottom: 0x0E1013, ink: 0xE8ECF0, inkSoft: 0x8A929C,
        mirror: 0x000000, mirrorOpacity: 0, cap: 0x2B3037, screwLight: 0xC9CED4, screwDark: 0x5A6068,
        red: 0xFF3B30, green: 0x34C759, yellow: 0xFFD60A, pointer: 0xFF9F0A, glare: false)

    public static let ice = VintageFaceTheme(
        name: "Ice", bezelDark: 0x9AA0A6, bezelMid: 0xC5CAD0, bezelLight: 0xE4E7EA,
        faceTop: 0xF9FAFB, faceBottom: 0xE6EAEF, ink: 0x1C1F24, inkSoft: 0x6B7280,
        mirror: 0x000000, mirrorOpacity: 0, cap: 0x2A2E34, screwLight: 0xE4E7EA, screwDark: 0x8B9096,
        red: 0xE5484D, green: 0x30A46C, yellow: 0xF5B301, pointer: 0x0A84FF, glare: false)
}
