// NereusSDR for iOS: the fifth-device question and answer on the link
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusLink

@Suite struct FifthDeviceCodecTests {
    private static let device: LinkJSON = .object([
        "deviceId": .string("mac-id"), "name": .string("MacBook Pro"),
        "shortName": .string("MacBook"), "kind": .string("computer"),
        "state": .string("transmitting"), "from": .string("relay"),
        "replaceable": .bool(true), "holdsTransmit": .bool(true),
        "lastActivitySeconds": .number(0), "connectedForSeconds": .number(3600),
        "awayForSeconds": .number(0), "transmittingForSeconds": .number(42),
        "listeningOn": .array([.object([
            "sliceId": .number(1), "letter": .string("B"), "frequencyHz": .number(7_240_000),
            "mode": .number(1), "band": .number(3),
        ])]),
        "transmittingOn": .object([
            "sliceId": .number(1), "letter": .string("B"), "frequencyHz": .number(7_240_000),
            "mode": .number(1), "band": .number(3),
        ]),
    ])

    private static func held(_ devices: [LinkJSON] = [device], revision: LinkJSON = .number(7),
                             extra: [String: LinkJSON] = [:]) -> String {
        LinkJSON.object(["type": .string("session.held"), "devices": .array(devices),
                         "revision": revision].merging(extra) { _, new in new }).compactText
    }

    @Test func typedHeldAndTakeoverRoundTrip() throws {
        guard case .sessionHeld(let held) = try LinkCodec.decode(Self.held(extra: [
            "placeTaken": .object(["byName": .string("iPad"), "byId": .string("ipad-id"),
                                   "secondsAgo": .number(15)]),
            "placeFreed": .object(["secondsAgo": .number(20)]),
        ])) else {
            Issue.record("expected held question")
            return
        }
        #expect(held.revision == 7)
        #expect(held.devices.first?.state == .transmitting)
        #expect(held.devices.first?.transmittingOn?.frequencyHz == 7_240_000)
        #expect(held.placeTaken?.byId == "ipad-id")
        #expect(held.placeFreed?.secondsAgo == 20)
        #expect(try LinkCodec.decode(LinkCodec.encode(.sessionHeld(held))) == .sessionHeld(held))
        let answer = LinkMessage.SessionTakeover(deviceId: "mac-id", revision: held.revision)
        #expect(try LinkCodec.decode(LinkCodec.encode(.sessionTakeover(answer))) == .sessionTakeover(answer))
        #expect(try LinkCodec.decode(LinkCodec.encode(.sessionTakeover(.init(deviceId: "", revision: 7))))
                == .sessionTakeover(.init(deviceId: "", revision: 7)))
    }

    @Test func malformedHeldIsRejected() throws {
        let broken: [String] = [
            Self.held(Array(repeating: Self.device, count: 5)),
            Self.held(revision: .number(-1)),
            Self.held(revision: .number(4_294_967_296)),
            Self.held([.object([:])]),
            Self.held([Self.device.replacing("state", with: .string("idle"))]),
            Self.held([Self.device.replacing("awayForSeconds", with: .number(-1))]),
            Self.held([Self.device.replacing("transmittingOn", with: .object(["letter": .string("B")]))]),
            Self.held(extra: ["placeFreed": .object(["secondsAgo": .number(-1)])]),
        ]
        for text in broken {
            #expect(throws: LinkCodecError.self) { try LinkCodec.decode(text) }
        }
    }

    @Test func takeoverEndKeepsWhoAndWhen() throws {
        let end = LinkMessage.SessionEnd(reason: "iPad took this device's place on the Core.",
                                         retryable: false, code: "takenOver", takenOverBy: "iPad",
                                         takenOverById: "ipad-id", secondsAgo: 12)
        #expect(try LinkCodec.decode(LinkCodec.encode(.sessionEnd(end))) == .sessionEnd(end))
    }
}

private extension LinkJSON {
    func replacing(_ key: String, with value: LinkJSON) -> LinkJSON {
        guard case .object(var fields) = self else { return self }
        fields[key] = value
        return .object(fields)
    }
}
