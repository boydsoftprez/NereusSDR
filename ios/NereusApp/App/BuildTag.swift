// NereusSDR for iOS: the test build's name, branch and short commit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The name a test build carries so the one on screen is known by looking at
/// it: `<branch>@<short sha>`, from `ios/scripts/build-tag.sh` through the
/// `NEREUS_BUILD_TAG` build setting into the Info.plist key `NereusBuildTag`.
/// Release builds, and builds started from Xcode's own window, carry none.
enum BuildTag {
    /// The Info.plist key the build setting fills in.
    static let infoKey = "NereusBuildTag"

    /// This build's tag, or nil when it carries none.
    static var current: String? {
        tag(in: Bundle.main.infoDictionary)
    }

    /// The tag in an Info.plist dictionary: nil when the key is absent, not
    /// text, or only white space.
    static func tag(in info: [String: Any]?) -> String? {
        guard let value = info?[infoKey] as? String else {
            return nil
        }
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? nil : trimmed
    }

    /// The words Setup and About show for a tag.
    static func label(for tag: String) -> String {
        "Build \(tag)"
    }
}
