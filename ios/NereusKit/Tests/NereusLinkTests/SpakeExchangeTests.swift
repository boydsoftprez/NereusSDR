// NereusSDR for iOS: SPAKE2+EE between the device's side and the Core's, with the link's fixed values
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CSodium
import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// R-IOS-08, D37: the exchange (link document section 3.6), codes made at
/// run time.
@Suite struct SpakeExchangeTests {
    private static func code() -> String {
        let words = PairingCodeText.words
        return "\(Int.random(in: 1...99))-\(words.randomElement()!)-\(words.randomElement()!)"
    }

    @Test func theFixedValuesAreTheLinksOwn() {
        #expect(SpakeExchange.isAvailable)
        #expect(SpakeExchange.clientId.utf8.count == 19)
        #expect(SpakeExchange.serverId.utf8.count == 20)
        #expect(crypto_pwhash_alg_default() == 2)
        #expect(crypto_pwhash_opslimit_interactive() == 2)
        #expect(crypto_pwhash_memlimit_interactive() == 67_108_864)
    }

    @Test func sameCodeAgreesTheKeysAndEachSideOpensTheOthersBox() async throws {
        let code = Self.code()
        let storedResult = await TestFixtureCrypto.run { SpakeExchange.storedData(code: code) }
        let stored = try #require(storedResult)
        #expect(stored.count == 164)
        let core = SpakeExchange(role: .station)
        let device = SpakeExchange(role: .device)
        let step0 = try #require(core.stationStep0(stored: stored))
        #expect(step0.count == 36)
        // Version 1, Argon2id, the interactive limits, then a 16-byte salt.
        #expect(Array(step0.prefix(20)) == [1, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0])
        #expect(SpakeExchange.validatesPublicData(step0))
        let step1Result = await TestFixtureCrypto.run { device.deviceStep1(publicData: step0, code: code) }
        let step1 = try #require(step1Result)
        #expect(step1.count == 32)
        let step2 = try #require(core.stationStep2(stored: stored, response1: step1))
        #expect(step2.count == 64)
        let step3 = try #require(device.deviceStep3(response2: step2))
        #expect(step3.count == 32)
        #expect(core.stationStep4(response3: step3))
        #expect(device.isComplete && core.isComplete)

        let fromDevice = try #require(device.seal(Data("device".utf8)))
        #expect(core.open(fromDevice) == Data("device".utf8))
        #expect(device.open(fromDevice) == nil)
        let fromCore = try #require(core.seal(Data("core".utf8)))
        #expect(device.open(fromCore) == Data("core".utf8))
        #expect(core.open(fromCore) == nil)
    }

    @Test func anotherCodeFailsAtTheDevicesStep3() async throws {
        let code = Self.code()
        var other = Self.code()
        while other == code {
            other = Self.code()
        }
        let storedResult = await TestFixtureCrypto.run { SpakeExchange.storedData(code: code) }
        let stored = try #require(storedResult)
        let core = SpakeExchange(role: .station)
        let device = SpakeExchange(role: .device)
        let step0 = try #require(core.stationStep0(stored: stored))
        let otherCode = other
        let step1Result = await TestFixtureCrypto.run { device.deviceStep1(publicData: step0, code: otherCode) }
        let step1 = try #require(step1Result)
        let step2 = try #require(core.stationStep2(stored: stored, response1: step1))
        #expect(device.deviceStep3(response2: step2) == nil)
        #expect(!device.isComplete)
        #expect(device.seal(Data("x".utf8)) == nil)
    }

    @Test func otherHashSettingsAreRefusedBeforeHashing() async throws {
        let code = Self.code()
        let storedResult = await TestFixtureCrypto.run { SpakeExchange.storedData(code: code) }
        let stored = try #require(storedResult)
        let step0 = try #require(SpakeExchange(role: .station).stationStep0(stored: stored))
        for (range, bytes) in [(0..<2, [2, 0]), (2..<4, [1, 0]), (4..<12, [1, 0, 0, 0, 0, 0, 0, 0]),
                               (12..<20, [0, 0, 0, 1, 0, 0, 0, 0])] as [(Range<Int>, [UInt8])] {
            var weaker = step0
            weaker.replaceSubrange(range, with: bytes)
            #expect(!SpakeExchange.validatesPublicData(weaker))
            let refused = SpakeExchange(role: .device).deviceStep1(publicData: weaker, code: Self.code()) == nil
            #expect(refused)
        }
        #expect(!SpakeExchange.validatesPublicData(step0.prefix(35)))
    }

    @Test func theCoreRefusesAStep1ThatIsNotAPointAndTakesNoSecondShare() async throws {
        let code = Self.code()
        let storedResult = await TestFixtureCrypto.run { SpakeExchange.storedData(code: code) }
        let stored = try #require(storedResult)
        let core = SpakeExchange(role: .station)
        let step0 = try #require(core.stationStep0(stored: stored))
        // All ones is not a canonical point.
        #expect(core.stationStep2(stored: stored, response1: Data(repeating: 0xFF, count: 32)) == nil)
        let step1Result = await TestFixtureCrypto.run {
            SpakeExchange(role: .device).deviceStep1(publicData: step0, code: code)
        }
        let step1 = try #require(step1Result)
        #expect(core.stationStep2(stored: stored, response1: step1) == nil)
    }

    @Test func eachSideTakesOnlyItsOwnSteps() async throws {
        let code = Self.code()
        let storedResult = await TestFixtureCrypto.run { SpakeExchange.storedData(code: code) }
        let stored = try #require(storedResult)
        #expect(SpakeExchange(role: .device).stationStep0(stored: stored) == nil)
        let step0 = try #require(SpakeExchange(role: .station).stationStep0(stored: stored))
        let stationRefusesStep1 = SpakeExchange(role: .station).deviceStep1(publicData: step0, code: Self.code()) == nil
        #expect(stationRefusesStep1)
        let device = SpakeExchange(role: .device)
        #expect(device.deviceStep3(response2: Data(count: 64)) == nil)
    }
}
