// NereusSDR for iOS: a typed pairing code made into the one text both ends hash, and words to finish one
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// The pairing code as the operator types it (link document section 3.6):
/// a number and two words from the Core's word list, `7-anvil-harbor`.
/// Both ends normalise a typed code the same way before they use it, and
/// the normalised text, as UTF-8, is the password. A code whose words are
/// not in the list is caught here, before anything is sent, so a typing
/// slip never burns a code.
public enum PairingCodeText {
    /// The highest number a code carries (the rendezvous nameplate's range).
    public static let maxNameplate = 999_999
    /// A number of more digits than this is not a code, whatever its value.
    public static let maxNameplateDigits = 9

    /// The word list, `resources/pairing-words-v1.txt` copied into the
    /// package: 256 lowercase words, sorted.
    public static let words: [String] = loadWords()
    private static let wordSet = Set(words)

    /// The code `text` stands for, normalised: lowercased, split on runs of
    /// anything outside ASCII letters and digits, and exactly three parts,
    /// the number (all digits, at most 9 of them, 1 to 999999, its leading
    /// zeros dropped) and two words of the list, joined by single hyphens.
    /// Nil when `text` is not a code.
    public static func normalise(_ text: String) -> String? {
        let parts = split(text.lowercased())
        guard parts.count == 3 else {
            return nil
        }
        let digits = parts[0]
        guard !digits.isEmpty, digits.count <= maxNameplateDigits,
              digits.unicodeScalars.allSatisfy({ ("0"..."9").contains($0) }),
              let number = Int(digits), (1...maxNameplate).contains(number) else {
            return nil
        }
        guard wordSet.contains(parts[1]), wordSet.contains(parts[2]) else {
            return nil
        }
        return "\(number)-\(parts[1])-\(parts[2])"
    }

    /// The words of the list that start with `prefix` (lowercased, without
    /// surrounding spaces), in the list's order; none for an empty prefix.
    public static func suggestions(forPrefix prefix: String) -> [String] {
        let wanted = prefix.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard !wanted.isEmpty else {
            return []
        }
        return words.filter { $0.hasPrefix(wanted) }
    }

    /// The runs of ASCII letters and digits in `text`.
    private static func split(_ text: String) -> [String] {
        var parts: [String] = []
        var current = String.UnicodeScalarView()
        for scalar in text.unicodeScalars {
            if ("a"..."z").contains(scalar) || ("0"..."9").contains(scalar) {
                current.append(scalar)
            } else if !current.isEmpty {
                parts.append(String(current))
                current = String.UnicodeScalarView()
            }
        }
        if !current.isEmpty {
            parts.append(String(current))
        }
        return parts
    }

    /// The name of the resource bundle SwiftPM and Xcode make for NereusLink.
    static let resourceBundleName = "NereusKit_NereusLink.bundle"

    /// Where the word list is. `Bundle.module` stops the process when its
    /// bundle is not where it looks, and a test host on the simulator keeps
    /// it inside the test bundle, so the bundle is looked for here: beside
    /// or inside the app, the image holding this code, and every loaded
    /// bundle, and the directories that hold them.
    static var wordListURL: URL? {
        final class Marker {}
        var places: [URL?] = [Bundle.main.resourceURL, Bundle.main.bundleURL,
                              Bundle(for: Marker.self).resourceURL, Bundle(for: Marker.self).bundleURL]
        for bundle in Bundle.allBundles + Bundle.allFrameworks {
            places.append(bundle.resourceURL)
            places.append(bundle.bundleURL)
            places.append(bundle.bundleURL.deletingLastPathComponent())
        }
        for place in places.compactMap({ $0 }) {
            let candidate = place.appendingPathComponent(resourceBundleName)
            if let bundle = Bundle(url: candidate),
               let url = bundle.url(forResource: "pairing-words-v1", withExtension: "txt") {
                return url
            }
        }
        return nil
    }

    private static func loadWords() -> [String] {
        guard let url = wordListURL, let text = try? String(contentsOf: url, encoding: .utf8) else {
            // The list ships inside the package; without it no code is one,
            // and every code the operator types would be refused.
            Logger(subsystem: "NereusSDR", category: "link.pairing")
                .fault("The pairing word list is missing from the app; no pairing code can be read")
            assertionFailure("the pairing word list is missing from NereusLink's resource bundle")
            return []
        }
        return text.split(separator: "\n").map(String.init)
    }
}
