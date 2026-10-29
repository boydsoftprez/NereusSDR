// NereusSDR for iOS: exact remote CW kind2 and unchanged kind1 wire bytes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Foundation
import Testing
@testable import NereusMedia

@Suite struct RemoteCwHeartbeatCodecTests {
    @Test func kindTwoHasExactlySeventeenBigEndianBytes() {
        let message = MediaUplink.keepaliveMessage(sequence: 0x0102030405060708, epoch: 0x09101112, cwEpoch: 0x13141516)
        #expect(message == Data([2,1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16]))
        #expect(message.count == MediaUplink.cwKeepaliveBytes)
    }

    @Test func zeroCwEpochPreservesKindOneByteForByte() {
        let legacy = MediaUplink.keepaliveMessage(sequence: 0x0102030405060708, epoch: Int64(UInt32.max))
        let pending = MediaUplink.keepaliveMessage(sequence: 0x0102030405060708, epoch: Int64(UInt32.max), cwEpoch: 0)
        #expect(legacy == pending)
        #expect(pending == Data([1,1,2,3,4,5,6,7,8,255,255,255,255]))
        #expect(pending.count == MediaUplink.keepaliveBytes)
    }

    @Test func cwOnlyAndMaximumEpochHaveIndependentFields() {
        let message = MediaUplink.keepaliveMessage(sequence: 1, epoch: 0, cwEpoch: Int64(UInt32.max))
        #expect(message == Data([2,0,0,0,0,0,0,0,1,0,0,0,0,255,255,255,255]))
    }
}
