// NereusSDR for iOS: the noise floor's fast-attack state, display extras version 4 (NSDX section 0x10)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// `display-v12-for-phone.md`, update 4: `noiseFloor.fastAttack` asks for
/// section 0x10, one byte whose bit 0 is fast attack (any other bit
/// refuses), after the floor section in bit order; sent only to a Core that
/// sent `displayExtrasVersion` 4. Kept apart from the decoder's own suite.
@Suite struct DisplayExtrasFastAttackTests {
    static let context = DisplayExtrasDecoder.Context(endpointId: 1, contextGeneration: 1, minDbm: -140, maxDbm: -40,
                                                      traceSamples: 32)

    @Test func theNoiseFloorStateVectorDecodesToItsExpectation() throws {
        let vector = try #require(try LinkFixtureLoader.mediaVectors()["media-nsdx1-noise-floor-state"])
        #expect(vector.codec == "nsdx1")
        #expect(vector.expect["noiseFloorFastAttack"] as? Bool == true)
        let result = DisplayExtrasDecoder.decode(vector.bytes, context: Self.context)
        #expect(result.reason == .none)
        let extras = try #require(result.extras)
        #expect(extras.noiseFloorFastAttack == true)
        #expect(abs(Double(try #require(extras.noiseFloorDbm)) - (-126.75)) <= 0.01)
        #expect(extras.encoderSequence == 3 && extras.peakBlobs == nil && extras.waterfallLevels == nil)
    }

    @Test func theUnknownSectionVectorNowSetsBit0x20AndIsStillRefused() throws {
        let vector = try #require(try LinkFixtureLoader.mediaVectors()["media-nsdx1-unknown-section"])
        #expect([UInt8](vector.bytes)[5] == 0x2F)
        #expect(DisplayExtrasDecoder.decode(vector.bytes, context: Self.context).reason == .unknownSections)
    }

    /// The vector's bytes with the state byte replaced, or cut off.
    private static func state(_ byte: UInt8?) throws -> Data {
        var bytes = try #require(try LinkFixtureLoader.mediaVectors()["media-nsdx1-noise-floor-state"]).bytes
        bytes.removeLast()
        if let byte {
            bytes.append(byte)
        }
        return bytes
    }

    @Test func theStateByteIsCheckedBitByBit() throws {
        #expect(DisplayExtrasDecoder.decode(try Self.state(0x00), context: Self.context).extras?.noiseFloorFastAttack
                == false)
        #expect(DisplayExtrasDecoder.decode(try Self.state(0x01), context: Self.context).extras?.noiseFloorFastAttack
                == true)
        for bad: UInt8 in [0x02, 0x03, 0x80, 0xFF] {
            #expect(DisplayExtrasDecoder.decode(try Self.state(bad), context: Self.context).reason == .malformed,
                    "\(bad)")
        }
        #expect(DisplayExtrasDecoder.decode(try Self.state(nil), context: Self.context).reason == .truncated)
        // A datagram without the section says nothing about fast attack.
        let floorOnly = try #require(try LinkFixtureLoader.mediaVectors()["media-nsdx1-noise-floor"])
        let plain = DisplayExtrasDecoder.decode(floorOnly.bytes, context: Self.context)
        #expect(plain.accepted && plain.extras?.noiseFloorFastAttack == nil)
    }

    @Test func fastAttackAsksForTheSectionAndItsByte() {
        let asked = DisplayExtrasRequest(noiseFloor: .init(enabled: true, shiftDb: 1, fastAttack: true))
        #expect(asked.sections == DisplayExtrasRequest.noiseFloorSection | DisplayExtrasRequest.noiseFloorStateSection)
        #expect(asked.worstCaseBytesPerFrame(traceSamples: 32) == 20 + 4 + 1)
        #expect(asked.subscribeFields["noiseFloor"]
                == .object(["enabled": .bool(true), "shiftDb": .number(1), "fastAttack": .bool(true)]))
        // Off, or the floor off, asks for no state.
        let off = DisplayExtrasRequest(noiseFloor: .init(enabled: true, shiftDb: 1, fastAttack: false))
        #expect(off.sections == DisplayExtrasRequest.noiseFloorSection)
        #expect(off.subscribeFields["noiseFloor"]
                == .object(["enabled": .bool(true), "shiftDb": .number(1), "fastAttack": .bool(false)]))
        let floorOff = DisplayExtrasRequest(noiseFloor: .init(enabled: false, shiftDb: 0, fastAttack: true))
        #expect(floorOff.sections == 0)
        // Left out, the member is absent.
        #expect(DisplayExtrasRequest(noiseFloor: .init(enabled: true, shiftDb: 0)).subscribeFields["noiseFloor"]
                == .object(["enabled": .bool(true), "shiftDb": .number(0)]))
    }

    @Test func onlyACoreThatSentVersionFourIsSentFastAttack() throws {
        let three = MediaFeatureGates(agreedMinor: 11) { $0 == "displayExtrasVersion" ? 3 : 1 }
        let four = MediaFeatureGates(agreedMinor: 11) { $0 == "displayExtrasVersion" ? 4 : 1 }
        #expect(!three.noiseFloorFastAttack && four.noiseFloorFastAttack)
        var subscription = DisplaySubscription(endpointId: 1, revision: 1, sliceId: 0, tier: .fine, fftSize: 4096,
                                               windowType: 0, centreHz: 7_236_400, spanHz: 48_000, pixels: 1179,
                                               fps: 30, framesPerLine: 1,
                                               trace: .init(detector: 0, averageMode: 0, averageAlpha: 0),
                                               waterfall: .init(detector: 0, averageMode: 0, averageAlpha: 0),
                                               minDbm: -160, maxDbm: 0, wideSpanFactor: 0)
        subscription.extras = DisplayExtrasRequest(noiseFloor: .init(enabled: true, shiftDb: 0, fastAttack: true))
        #expect(DisplayEndpointRequest.subscribe(subscription, connectionId: "c", gates: four)["noiseFloor"]
                == .object(["enabled": .bool(true), "shiftDb": .number(0), "fastAttack": .bool(true)]))
        #expect(DisplayEndpointRequest.subscribe(subscription, connectionId: "c", gates: three)["noiseFloor"]
                == .object(["enabled": .bool(true), "shiftDb": .number(0)]))
    }
}
