// NereusSDR for iOS: a typed code normalises as the Core normalises it, and the word list is the Core's
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import Testing
@testable import NereusLink

/// R-IOS-16: the code text (link document section 3.6), against the Core's
/// own table (tests/tst_pairing_code.cpp, normaliseJoinsWhateverSeparatesTheParts
/// and normaliseRefusesWhatIsNotACode).
@Suite struct PairingCodeTextTests {
    @Test func theWordListIsTheCoresCopy() throws {
        // The copy NereusLink ships (its resource bundle).
        let url = try #require(PairingCodeText.wordListURL)
        let data = try Data(contentsOf: url)
        #expect(data.count == 1697)
        #expect(SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
                == "dd319f6966664521e3a1253ad98d6b548e8a7b28ccef1d864d990f91a7513054")
        let words = PairingCodeText.words
        #expect(words.count == 256)
        #expect(words == words.sorted())
        #expect(words.allSatisfy { $0.count >= 4 && $0.count <= 7 && $0.allSatisfy { ("a"..."z").contains($0) } })
        #expect(words.contains("anvil"))
        #expect(words.contains("harbor"))
        #expect(!words.contains("harbour"))
    }

    @Test(arguments: [
        ("7 Anvil  harbor", "7-anvil-harbor"),
        ("  7-ANVIL-HARBOR \n", "7-anvil-harbor"),
        ("7 - anvil -- harbor", "7-anvil-harbor"),
        ("7.anvil.harbor", "7-anvil-harbor"),
        ("007 anvil harbor", "7-anvil-harbor"),
        ("42 harbor anvil", "42-harbor-anvil"),
        // A separator before the number is only a separator.
        ("-1-anvil-harbor", "1-anvil-harbor"),
        ("007-anvil-harbor", "7-anvil-harbor"),
        ("999999-anvil-harbor", "999999-anvil-harbor"),
        ("000000001 anvil harbor", "1-anvil-harbor"),
    ])
    func joinsWhateverSeparatesTheParts(_ typed: String, _ normalised: String) {
        #expect(PairingCodeText.normalise(typed) == normalised)
    }

    @Test(arguments: [
        "", "7", "7-anvil", "7-anvil-harbor-extra",
        "7-anvil-harbour", "anvil-7-harbor", "seven-anvil-harbor",
        "0-anvil-harbor", "1234567-anvil-harbor",
        "7-anvil-harborx", "7x-anvil-harbor",
        "1000000-anvil-harbor", "0000000001-anvil-harbor",
        "\u{0667}-anvil-harbor",
    ])
    func refusesWhatIsNotACode(_ typed: String) {
        #expect(PairingCodeText.normalise(typed) == nil)
    }

    @Test func suggestsTheWordsAPrefixCouldBecome() {
        #expect(PairingCodeText.suggestions(forPrefix: "harb").contains("harbor"))
        #expect(PairingCodeText.suggestions(forPrefix: " HARB").contains("harbor"))
        #expect(PairingCodeText.suggestions(forPrefix: "harbou").isEmpty)
        #expect(PairingCodeText.suggestions(forPrefix: "").isEmpty)
        #expect(PairingCodeText.suggestions(forPrefix: "a").allSatisfy { $0.hasPrefix("a") })
    }
}
