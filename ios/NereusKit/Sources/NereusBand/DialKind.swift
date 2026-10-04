// NereusSDR for iOS: which tuning dial this phone shows, if any
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The tuning dial chosen in Setup, General, Navigation on this phone
/// (D12, R-IOS-12): off until someone picks one of the three.
public enum DialKind: String, Equatable, Sendable, CaseIterable {
    /// No dial: tune by dragging or tapping the band.
    case off
    /// A knob at the bottom right of the waterfall, opposite PTT.
    case waterfallKnob
    /// A big knob in a sheet, raised by tapping the frequency.
    case sheetKnob
    /// A flat wheel along the bottom of the waterfall.
    case thumbwheel

    /// A new install starts with no dial.
    public static let standard = DialKind.off
}
