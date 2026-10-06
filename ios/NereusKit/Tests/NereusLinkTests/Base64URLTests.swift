// NereusSDR for iOS: tests for strict base64url, as the link writes keys and signatures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusLink

@Suite struct Base64URLTests {
    /// Foundation's base64, turned into the URL alphabet without padding.
    private static func reference(_ data: Data) -> String {
        data.base64EncodedString().replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_").replacingOccurrences(of: "=", with: "")
    }

    @Test func everyLengthRoundTripsAndMatchesFoundation() {
        for length in 0...70 {
            let data = Data((0..<length).map { _ in UInt8.random(in: 0...255) })
            let written = Base64URL.encode(data)
            #expect(written == Self.reference(data), "length \(length)")
            #expect(Base64URL.decode(written) == data, "length \(length)")
        }
    }

    @Test func aKeyAndAFingerprintHaveTheLinksLengths() {
        #expect(Base64URL.encode(Data(repeating: 0xFF, count: 32)).count == 43)
        #expect(Base64URL.encode(Data(repeating: 0xFF, count: 64)).count == 86)
        #expect(Base64URL.encode(Data(repeating: 0xFF, count: 91)).count == 122)
    }

    @Test func theURLAlphabetIsUsed() {
        #expect(Base64URL.encode(Data([0xFB, 0xFF])) == "-_8")
        #expect(Base64URL.decode("-_8") == Data([0xFB, 0xFF]))
    }

    @Test(arguments: [
        "AA==",       // padding
        "AAA=",       // padding
        "+/8",        // the standard alphabet
        "AA AA",      // whitespace
        "AAAA\n",     // a newline
        "A",          // no whole byte
        "AAAAA",      // five characters give no whole number of bytes
        "AB",         // unused bits not zero (one byte)
        "AAB",        // unused bits not zero (two bytes)
        "é",          // outside ASCII
    ])
    func whatIsNotStrictBase64URLIsRefused(text: String) {
        #expect(Base64URL.decode(text) == nil)
    }

    @Test func unusedBitsZeroAreAccepted() {
        #expect(Base64URL.decode("AA") == Data([0]))
        #expect(Base64URL.decode("AAA") == Data([0, 0]))
        #expect(Base64URL.decode("") == Data())
    }
}
