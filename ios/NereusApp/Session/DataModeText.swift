// NereusSDR for iOS: the words for each data mode: its name, what it asks for, and its estimated cost
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMedia

/// Each data mode's words, as spec section 5.4 item 11's table and the
/// board's Data use page give them. The costs are the spec's estimates
/// (spec section 4.7, from the remote design's reference measurements)
/// and are shown as estimates until the bench measures the app's own.
/// Each includes the sound at High's 48 kbit/s (R-IOS-09): the spec's
/// figures, which carried it at 24 kbit/s, plus the other 24 kbit/s
/// (24000 x 3600 / 8 = 10.8 MB an hour), rounded as the spec rounds:
/// the band's figures to 5 MB, the sound alone to 1 MB.
enum DataModeText {
    static func title(_ mode: SessionPolicy.Mode) -> String {
        switch mode {
        case .full:
            return "Full"
        case .balanced:
            return "Balanced"
        case .saver:
            return "Saver"
        case .audioOnly:
            return "Audio only"
        }
    }

    /// What the mode asks for.
    static func asks(_ mode: SessionPolicy.Mode) -> String {
        switch mode {
        case .full:
            return "30 frames a second, full detail"
        case .balanced:
            return "15 frames a second"
        case .saver:
            return "5 frames a second, half the detail"
        case .audioOnly:
            return "No band, just the sound"
        }
    }

    /// The estimated cost for an hour.
    static func estimate(_ mode: SessionPolicy.Mode) -> String {
        switch mode {
        case .full:
            return "about 70 MB an hour"
        case .balanced:
            return "about 45 MB an hour"
        case .saver:
            return "about 30 MB an hour"
        case .audioOnly:
            return "about 24 MB an hour"
        }
    }

    /// The first time on cellular (spec section 5.4 item 12, picture 12):
    /// "On cellular: Balanced. 15 frames a second, about 45 MB an hour.
    /// Change it in Setup, Data use."
    static func cellularNote(_ mode: SessionPolicy.Mode) -> String {
        "On cellular: \(title(mode)). \(asks(mode)), \(estimate(mode)). Change it in Setup, Data use."
    }

    /// The chip on the band while off Wi-Fi: "Balanced · 15 fps", or
    /// "Audio only".
    static func chip(_ mode: SessionPolicy.Mode, fps: Int?) -> String {
        guard let fps else {
            return title(.audioOnly)
        }
        return "\(title(mode)) \u{00B7} \(fps) fps"
    }

    /// Past 5 GB in a month on cellular, once.
    static func monthWarning(_ used: String) -> String {
        "Past 5 GB on cellular this month. \(used) so far. Change it in Setup, Data use."
    }
}
