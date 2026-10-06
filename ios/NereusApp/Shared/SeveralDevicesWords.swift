// NereusSDR for iOS: the words the several-devices screens write themselves: durations, times of day, slices and states
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusMirror

/// The words the phone writes itself on the several-devices screens (spec
/// sections 5.8 and 5.9, pictures 22 and 23), in the board's wording.
/// Everything the Core says (its reasons, a take's `why`, a change's label
/// and values, device names) is shown as it sends it, not rewritten here.
enum SeveralDevicesWords {
    /// "just now", "4 minutes ago", "2 hours ago".
    static func ago(seconds: Int64) -> String {
        if seconds < 60 {
            return "just now"
        }
        return duration(seconds: seconds) + " ago"
    }

    /// "40 seconds", "1 minute", "12 minutes", "2 hours".
    static func duration(seconds: Int64) -> String {
        let seconds = max(0, seconds)
        if seconds < 60 {
            return seconds == 1 ? "1 second" : "\(seconds) seconds"
        }
        let minutes = seconds / 60
        if minutes < 60 {
            return minutes == 1 ? "1 minute" : "\(minutes) minutes"
        }
        let hours = minutes / 60
        return hours == 1 ? "1 hour" : "\(hours) hours"
    }

    /// A time of day by this phone's own clock, zone and settings ("19:55").
    static func timeOfDay(_ date: Date) -> String {
        date.formatted(date: .omitted, time: .shortened)
    }

    /// "listening", "transmitting" or "away".
    static func state(_ state: SeveralDevices.DeviceState) -> String {
        switch state {
        case .listening:
            return "listening"
        case .transmitting:
            return "transmitting"
        case .away:
            return "away"
        case .other(let name):
            return name
        }
    }

    /// "Slice C on 14.230.000 USB".
    static func slice(_ letter: String, hz: Double, mode: String) -> String {
        let where_ = mode.isEmpty ? BandSlice.frequencyText(hz: hz) : BandSlice.frequencyText(hz: hz) + " " + mode
        return "Slice \(letter) on \(where_)"
    }

    /// A device as the board names it inside a sentence: "the MacBook", or
    /// the name alone when it already says whose it is ("Grant\u{2019}s
    /// iPhone", "Chris\u{2019} iPad"), which "the" would not read right
    /// before. `capitalised` starts a sentence with it: "The MacBook".
    static func the(_ shortName: String, capitalised: Bool = false) -> String {
        if isPossessive(shortName) {
            return shortName
        }
        return (capitalised ? "The " : "the ") + shortName
    }

    /// "the MacBook\u{2019}s", "Grant\u{2019}s iPhone\u{2019}s".
    static func theDevices(_ shortName: String, capitalised: Bool = false) -> String {
        the(shortName, capitalised: capitalised) + "\u{2019}s"
    }

    /// Whether a device's name says whose it is: a word in it ends in
    /// "\u{2019}s" or "s\u{2019}" (or with a straight apostrophe), as in
    /// "Grant's iPhone" and "Chris' iPad".
    static func isPossessive(_ name: String) -> Bool {
        let endings = ["'s", "\u{2019}s", "s'", "s\u{2019}"]
        return name.split(whereSeparator: \.isWhitespace).contains { word in
            let lower = word.lowercased()
            return lower.count > 2 && endings.contains { lower.hasSuffix($0) }
        }
    }

    /// "B", "B and D", "B, C and D".
    static func letters(_ letters: [String]) -> String {
        switch letters.count {
        case 0:
            return ""
        case 1:
            return letters[0]
        default:
            return letters.dropLast().joined(separator: ", ") + " and " + letters[letters.count - 1]
        }
    }

    /// Who holds another device's slice, in the Core's words for a
    /// slice's holder (`ForeignSliceMarkers.Slice.holderWords`): the
    /// device's own name as sent ("MacBook Pro", "Grant\u{2019}s iPhone"),
    /// "the Core", "a phone", "a tablet", "a computer" or "another device".
    /// `capitalised` starts a sentence with the Core's words ("The Core",
    /// "A phone"); a device's own name is never changed. Never empty.
    static func sliceOwner(_ slice: ForeignSliceMarkers.Slice, capitalised: Bool = false) -> String {
        let words = slice.holderWords
        guard capitalised, !slice.ownerNamed else {
            return words
        }
        return words.prefix(1).uppercased() + words.dropFirst()
    }

    /// The name a sentence uses for a device: its short name, else its name.
    static func shortName(_ shortName: String, _ name: String) -> String {
        shortName.isEmpty ? name : shortName
    }
}
