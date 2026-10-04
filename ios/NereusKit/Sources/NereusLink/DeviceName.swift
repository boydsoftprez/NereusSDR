// NereusSDR for iOS: the rule a device's name and short name are held to
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The one rule for a device's name (D65) and its short name (link
/// document section 3.5): not blank, within its cap of UTF-8 bytes, and no
/// control, format, line-separator or paragraph-separator characters. Every
/// place the operator types or confirms a name checks it here, so a name
/// the app accepts on screen is never refused at sign-in.
///
/// The app checks Unicode scalars; the Core checks UTF-16 code units, so a
/// format character outside the Basic Multilingual Plane (the tag
/// characters in a subdivision flag) passes at the Core and is refused
/// here. The app's rule is the stricter one, and the one it keeps.
public enum DeviceName {
    /// A name is at most 64 bytes of UTF-8.
    public static let maxBytes = 64
    /// A short name is at most 32 bytes of UTF-8 (`shortNameMaxBytes`).
    public static let shortNameMaxBytes = 32

    /// True when `name` is a name the app will sign in with.
    public static func isUsable(_ name: String) -> Bool {
        isUsable(name, maxBytes: maxBytes)
    }

    /// True when `shortName` is a short name the app will send.
    public static func isUsableShortName(_ shortName: String) -> Bool {
        isUsable(shortName, maxBytes: shortNameMaxBytes)
    }

    private static func isUsable(_ text: String, maxBytes: Int) -> Bool {
        guard !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, text.utf8.count <= maxBytes else {
            return false
        }
        return !text.unicodeScalars.contains { scalar in
            switch scalar.properties.generalCategory {
            case .control, .format, .lineSeparator, .paragraphSeparator:
                return true
            default:
                return false
            }
        }
    }
}
