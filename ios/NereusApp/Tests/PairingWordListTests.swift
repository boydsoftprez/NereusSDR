// NereusSDR for iOS: the pairing word list is found where the app runs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
@testable import NereusSDR
import Testing

/// R-IOS-16: the app finds NereusLink's copy of the Core's word list in its
/// own bundle, so a code the Core shows is read as one on the device.
@Suite("Pairing word list")
struct PairingWordListTests {
    @Test("the app carries all 256 words")
    func theAppCarriesTheWholeList() {
        #expect(PairingCodeText.words.count == 256)
        // A code made from the list the app carries, typed loosely, reads as one.
        let first = PairingCodeText.words[0]
        let last = PairingCodeText.words[PairingCodeText.words.count - 1]
        #expect(PairingCodeText.normalise("7 \(first.capitalized)  \(last.uppercased())") == "7-\(first)-\(last)")
    }
}
