// NereusSDR for iOS: the tests' mailbox numbers never meet a Core's or each other
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import LinkSessionTestSupport
import NereusLink
import Testing

/// ``PairingClient`` refuses a second pairing on a mailbox number one already
/// holds, process-wide, so the tests' numbers must never meet: every number
/// ``MailboxNameplates/unshown()`` gives is above the numbers a Core shows,
/// still a number a code can carry, and never given twice, even to callers
/// running at once.
@Suite struct MailboxNameplatesTests {
    @Test func anUnshownNumberIsNeverACoresNorGivenTwice() async {
        let numbers = await withTaskGroup(of: [Int].self) { group in
            for _ in 0..<16 {
                group.addTask { (0..<64).map { _ in MailboxNameplates.unshown() } }
            }
            return await group.reduce(into: [Int]()) { $0 += $1 }
        }
        #expect(numbers.count == 16 * 64)
        #expect(Set(numbers).count == numbers.count)
        #expect(numbers.allSatisfy { $0 > MailboxNameplates.coreRange.upperBound })
        #expect(numbers.allSatisfy { $0 <= PairingCodeText.maxNameplate })
        #expect(MailboxNameplates.coreRange == 1...99)
    }
}
