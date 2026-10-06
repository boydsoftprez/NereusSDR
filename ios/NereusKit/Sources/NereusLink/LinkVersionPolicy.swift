// NereusSDR for iOS: which link versions the app speaks and how it agrees one with a Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Link versions both ways (link document section 6.1). Each end supports
/// its own major and the one before it, and the two agree the highest major
/// both support. Ends two or more majors apart share none, and the side
/// with the older newest version is the one to update.
public enum LinkVersionPolicy {
    /// The majors this build speaks, oldest first.
    public static let defaultMajors: [UInt16] = [1]

    /// The minor both ends send; it stays at 11 (section 6.1).
    public static let minor: UInt16 = 11

    /// The launch argument that replaces the majors in a debug build, for
    /// trying the version screens against a Core started with
    /// `--test-link-majors`: `-NereusLinkMajors 1,2`.
    public static let majorsArgument = "-NereusLinkMajors"

    /// The majors this app supports: `defaultMajors`, or in a debug build
    /// the list given with `-NereusLinkMajors`. A release build ignores it.
    public static let supportedMajors: [UInt16] = majors(fromArguments: ProcessInfo.processInfo.arguments,
                                                          overrideAllowed: isDebugBuild)

    /// True in a debug build.
    public static var isDebugBuild: Bool {
        #if DEBUG
        return true
        #else
        return false
        #endif
    }

    /// The majors `arguments` ask for, when an override is allowed and the
    /// list parses; `defaultMajors` otherwise.
    public static func majors(fromArguments arguments: [String], overrideAllowed: Bool) -> [UInt16] {
        guard overrideAllowed, let index = arguments.firstIndex(of: majorsArgument),
              index + 1 < arguments.count, let majors = parseMajors(arguments[index + 1]) else {
            return defaultMajors
        }
        return majors
    }

    /// Whole numbers from 1 to 65535 separated by commas, sorted oldest
    /// first without repeats; nil when the text is not such a list.
    public static func parseMajors(_ text: String) -> [UInt16]? {
        var majors: Set<UInt16> = []
        for part in text.split(separator: ",", omittingEmptySubsequences: false) {
            let trimmed = part.trimmingCharacters(in: .whitespaces)
            guard !trimmed.isEmpty, trimmed.allSatisfy({ $0.isASCII && $0.isNumber }),
                  let value = Int(trimmed), value >= 1, value <= 65_535 else {
                return nil
            }
            majors.insert(UInt16(value))
        }
        return majors.sorted()
    }

    /// The highest major in both lists, or nil when they share none.
    public static func agree(ours: [UInt16], theirs: [UInt16]) -> UInt16? {
        ours.filter { theirs.contains($0) }.max()
    }

    /// Which side must update when the lists share no major: the one whose
    /// newest major is older. Equal newest majors cannot share none, so the
    /// tie goes to the app, as the Core's own wording does.
    public static func refusal(station: [UInt16], app: [UInt16]) -> Refusal.Reason {
        let stationNewest = station.max() ?? 0
        let appNewest = app.max() ?? 0
        if stationNewest < appNewest {
            return .stationTooOld(station: station, app: app)
        }
        return .appTooOld(station: station, app: app)
    }
}
