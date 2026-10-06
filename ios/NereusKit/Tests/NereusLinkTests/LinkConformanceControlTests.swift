// NereusSDR for iOS: the link's control fixtures, decoded and written again by the app's codec
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
@testable import NereusLink

/// Link document section 16.2: each end decodes `wire`; when `decodes` is
/// true it encodes the result again and the two compare equal after
/// parsing, key order ignored. Every control fixture applies to both ends.
@Suite struct LinkConformanceControlTests {
    private struct ControlFixture {
        let id: String
        let from: String
        let wire: LinkJSON
        let decodes: Bool
    }

    /// Every control fixture of the suite. The several-devices kinds,
    /// `confirm.request` and `notice`, are the app's from Task 56c, which
    /// declares `sessionHolder`.
    private static func controlFixtures() throws -> [ControlFixture] {
        try LinkFixtureLoader.manifest().filter { $0.kind == "control" }.map { fixture in
            let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(fixture.file))
            let unknown = Set(object.keys).subtracting(["from", "wire", "decodes"])
            if let key = unknown.sorted().first {
                throw LinkFixtureLoader.Malformed(description: "\(fixture.id): unknown field \"\(key)\"")
            }
            guard let from = object["from"] as? String, from == "station" || from == "client",
                  let wire = object["wire"], let decodes = object["decodes"] as? Bool else {
                throw LinkFixtureLoader.Malformed(description: "\(fixture.id): from, wire or decodes is missing")
            }
            return ControlFixture(id: fixture.id, from: from, wire: try LinkJSON(foundation: wire), decodes: decodes)
        }
    }

    private static func surface() throws -> [String: Any] {
        try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
    }

    @Test func everyControlFixtureDecodesAndEncodesAgain() throws {
        let fixtures = try Self.controlFixtures()
        #expect(fixtures.count == 71)
        #expect(Set(fixtures.map(\.id)).count == fixtures.count)
        #expect(fixtures.contains { $0.id == "control-client-diversity-set-target" })
        // A spot on 2 m, band 27 (band2mVersion 1).
        #expect(fixtures.contains { $0.id == "control-station-record-batch-band-2m" })
        // A record stream's batch, and one whose upsert has no id (refused);
        // the spots with the Core's resolved mode, and FreeDV Reporter's stations with their bands.
        for id in ["control-station-record-batch", "control-record-batch-upsert-without-id",
                   "control-station-record-batch-resolved-mode", "control-station-freedv-stations-band"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        for id in ["control-station-confirm-request", "control-station-notice", "control-notice-take-back-not-bool"] {
            #expect(fixtures.contains { $0.id == id }, "\(id)")
        }
        for fixture in fixtures {
            let text = fixture.wire.compactText
            if fixture.decodes {
                let message: LinkMessage
                do {
                    message = try LinkCodec.decode(text)
                } catch {
                    Issue.record("\(fixture.id): did not decode: \(error)")
                    continue
                }
                let again = try LinkJSON.parse(LinkCodec.encode(message))
                #expect(again == fixture.wire, "\(fixture.id): encoded again as \(again.compactText)")
            } else {
                #expect(throws: LinkCodecError.self, "\(fixture.id) must be refused") {
                    try LinkCodec.decode(text)
                }
            }
        }
    }


    /// New verbs retain their exact minor, capability and typed argument contracts.
    @Test func integratedCommandsKeepTheirTypedMetadata() throws {
        let commands = try #require(try Self.surface()["commands"] as? [[String: Any]])
        #expect(commands.count == 160)
        #expect(Set(commands.compactMap { $0["verb"] as? String }).count == commands.count)
        let expected: [(verb: String, capability: String, version: Int, names: [String], kinds: [String])] = [
            ("diversity.setTarget", "diversityControlVersion", 1,
             ["enabled", "stateRevision", "sourceSliceId", "sourceIncarnation", "sourceControlRevision",
              "targetSliceId", "targetIncarnation", "targetControlRevision"],
             ["bool", "i64", "i64", "i64", "i64", "i64", "i64", "i64"]),
            ("tx.setMicSource", "radioMicVersion", 2, ["source"], ["utf8"]),
        ]
        for entry in expected {
            let command = try #require(commands.first { $0["verb"] as? String == entry.verb })
            #expect(Set(command.keys) == Set(["verb", "capability", "capabilityVersion", "minMinor", "arguments"]))
            #expect(command["capability"] as? String == entry.capability)
            #expect(command["capabilityVersion"] as? Int == entry.version)
            #expect(command["minMinor"] as? Int == 11)
            let arguments = try #require(command["arguments"] as? [[String: Any]])
            #expect(arguments.count == entry.names.count)
            #expect(arguments.compactMap { $0["name"] as? String } == entry.names)
            #expect(arguments.compactMap { $0["kind"] as? String } == entry.kinds)
            for argument in arguments {
                #expect(Set(argument.keys) == Set(["name", "kind", "optional"]))
                #expect(argument["optional"] as? Bool == false)
            }
        }
    }

    /// The client codec carries an unrecognised verb; the station owns refusal.
    @Test func anUnknownVerbKeepsItsTypedArguments() throws {
        let message = LinkMessage.commandInvoke(LinkMessage.CommandInvoke(verb: "future.fixtureVerb", id: 0,
            args: [.init(ordinal: 0, name: "enabled", value: .bool(true))]))
        let text = LinkCodec.encode(message)
        let decoded = try LinkCodec.decode(text)
        #expect(try LinkJSON.parse(LinkCodec.encode(decoded)) == LinkJSON.parse(text))
    }

    @Test func fixturesCoverEveryKindInItsDirection() throws {
        var fromClient: Set<String> = []
        var fromStation: Set<String> = []
        for fixture in try Self.controlFixtures() where fixture.decodes {
            guard case .object(let object) = fixture.wire, case .string(let type)? = object["type"] else {
                continue
            }
            if fixture.from == "client" {
                fromClient.insert(type)
            } else {
                fromStation.insert(type)
            }
        }
        let clientKinds = Set(LinkMessage.Kind.allCases.filter(\.sentByClient).map(\.rawValue))
        let stationKinds = Set(LinkMessage.Kind.allCases.filter(\.sentByStation).map(\.rawValue))
        #expect(clientKinds.count == 14)
        #expect(stationKinds.count == 25)
        #expect(fromClient == clientKinds)
        #expect(fromStation == stationKinds)
    }

    /// LinkMessage has a case for every kind in the link's surface, and no other.
    @Test func messageKindsMatchTheSurface() throws {
        guard let kinds = try Self.surface()["messageKinds"] as? [String: Any] else {
            throw LinkFixtureLoader.Malformed(description: "surface.json: messageKinds is missing")
        }
        #expect(Set(kinds.keys) == Set(LinkMessage.Kind.allCases.map(\.rawValue)))
    }

    /// Removing any key the surface marks required makes the message refused.
    @Test func everyRequiredKeyIsRequired() throws {
        guard let kinds = try Self.surface()["messageKinds"] as? [String: [String: Any]] else {
            throw LinkFixtureLoader.Malformed(description: "surface.json: messageKinds is missing")
        }
        for fixture in try Self.controlFixtures() where fixture.decodes {
            guard case .object(let object) = fixture.wire, case .string(let type)? = object["type"],
                  let required = kinds[type]?["required"] as? [String] else {
                Issue.record("\(fixture.id): no surface entry")
                continue
            }
            for key in required where key != "type" {
                var trimmed = object
                trimmed[key] = nil
                #expect(throws: LinkCodecError.self, "\(fixture.id) without \(key) must be refused") {
                    try LinkCodec.decode(LinkJSON.object(trimmed).compactText)
                }
            }
        }
    }

    /// The limits the app keeps are the ones the surface records.
    @Test func theAppKeepsTheSurfacesLimits() throws {
        guard let limits = try Self.surface()["limits"] as? [String: [String: Any]] else {
            throw LinkFixtureLoader.Malformed(description: "surface.json: limits is missing")
        }
        #expect(limits.count == 30)
        #expect(limits["shortNameMaxBytes"]?["value"] as? Int == DeviceName.shortNameMaxBytes)
        #expect(limits["shortNameMaxBytes"]?["value"] as? Int == 32)
        // The app never holds more than one connection that is still
        // connecting, well inside the Core's two per address.
        #expect(limits["maxHandshakesPerAddress"]?["value"] as? Int == 2)
        #expect(limits["clientInboundMessageBytes"]?["value"] as? Int == WebSocketLinkTransport.maxInboundMessageBytes)
        #expect(limits["stationInboundMessageBytes"]?["value"] as? Int == StationSession.maxOutboundMessageBytes)
        #expect(limits["maxDeviceSessions"]?["value"] as? Int == FoundStation.maximumDevices)
        #expect(limits["connectDeadlineMs"]?["value"] as? Int == 30_000)
        #expect(StationSession.connectDeadline == .milliseconds(30_000))
        #expect(limits["takeoverAnswerMs"]?["value"] as? Int == 60_000)
        #expect(StationSession.takeoverAnswerDeadline == .milliseconds(60_000))
        #expect(limits["telemetryBytes"]?["value"] as? Int == LinkCodec.telemetryCapBytes)
    }

    @Test func notFiniteNumbersTravelAsStrings() throws {
        let delta = LinkMessage.delta(LinkMessage.Delta(key: "slice:0", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "a", value: .f64(.nan)),
            LinkMessage.PropertyEntry(ordinal: 2, name: "b", value: .f64(.infinity)),
            LinkMessage.PropertyEntry(ordinal: 3, name: "c", value: .f64(-.infinity)),
        ]))
        let text = LinkCodec.encode(delta)
        #expect(text.contains("\"nan\"") && text.contains("\"inf\"") && text.contains("\"-inf\""))
        guard case .delta(let back) = try LinkCodec.decode(text) else {
            Issue.record("not a delta")
            return
        }
        guard case .f64(let nan) = back.properties[0].value, case .f64(let inf) = back.properties[1].value,
              case .f64(let negative) = back.properties[2].value else {
            Issue.record("not f64 values")
            return
        }
        #expect(nan.isNaN)
        #expect(inf == .infinity)
        #expect(negative == -.infinity)
    }

    @Test func messagesAreCompact() {
        let text = LinkCodec.encode(.authRequest(LinkMessage.AuthRequest(token: "a \"quoted\" token")))
        #expect(text == #"{"token":"a \"quoted\" token","type":"auth.request"}"#)
    }

    @Test func anEmptyShortNameIsNotWritten() throws {
        let block = LinkMessage.DeviceBlock(id: "i", publicKey: "k", name: "n", kind: "phone", signature: "s",
                                            shortName: "")
        let text = LinkCodec.encode(.authRequest(LinkMessage.AuthRequest(token: "", device: block)))
        #expect(!text.contains("shortName"))
    }

    @Test func fifthDeviceControlFixturesKeepDirectionAndSurfaceShape() throws {
        let fixtures = try Self.controlFixtures()
        for (id, expectedDirection, decodes) in [
            ("control-station-session-held", "station", true),
            ("control-client-session-takeover", "client", true),
            ("control-client-session-takeover-bad-revision", "client", false),
        ] {
            let fixture = try #require(fixtures.first { $0.id == id }, "Missing Core fixture \(id)")
            #expect(fixture.from == expectedDirection)
            #expect(fixture.decodes == decodes)
            if decodes {
                #expect(try LinkJSON.parse(LinkCodec.encode(LinkCodec.decode(fixture.wire.compactText))) == fixture.wire)
            } else {
                #expect(throws: LinkCodecError.self) { try LinkCodec.decode(fixture.wire.compactText) }
            }
        }
        #expect(LinkMessage.Kind.sessionHeld.sentByStation)
        #expect(!LinkMessage.Kind.sessionHeld.sentByClient)
        #expect(LinkMessage.Kind.sessionTakeover.sentByClient)
        #expect(!LinkMessage.Kind.sessionTakeover.sentByStation)
        guard let kinds = try Self.surface()["messageKinds"] as? [String: [String: Any]] else {
            throw LinkFixtureLoader.Malformed(description: "surface.json: messageKinds is missing")
        }
        #expect(kinds["session.held"]?["required"] as? [String] == ["devices", "revision", "type"])
        #expect(kinds["session.takeover"]?["required"] as? [String] == ["deviceId", "revision", "type"])
    }

    /// Truly unknown keys from a later Core are ignored; takeover metadata is retained.
    @Test func unknownKeysInEndsAndSignInsAreIgnored() throws {
        let end = #"{"type":"session.end","reason":"Gone.","retryable":false,"code":"takenOver","takenOverById":"x","secondsAgo":3,"later":1}"#
        #expect(try LinkCodec.decode(end) == .sessionEnd(LinkMessage.SessionEnd(reason: "Gone.", retryable: false,
                                                                                code: "takenOver", takenOverById: "x",
                                                                                secondsAgo: 3)))
        let result = #"{"type":"auth.result","accepted":true,"reason":"","later":1}"#
        #expect(try LinkCodec.decode(result) == .authResult(LinkMessage.AuthResult(accepted: true, reason: "")))
    }

    @Test(arguments: [
        #"{"type":"hello","major":1,"minor":11,"settingsSchema":0,"peer":"nereusd","challenge":""}"#,
        #"{"type":"hello","major":1,"minor":11,"settingsSchema":0,"peer":"nereusd","identity":{"publicKey":"k"}}"#,
        #"{"type":"auth.result","accepted":false,"reason":"No.","code":7}"#,
        #"{"type":"pair.start","mode":"code","device":{"publicKey":"k","name":"n"}}"#,
        #"{"type":"pair.accept","identity":{"publicKey":"k","certBinding":"b"}}"#,
        #"{"type":"pair.spake","step":-1,"data":"d"}"#,
        #"{"type":"pair.spake","step":1,"data":""}"#,
        #"{"type":"pair.confirm","box":""}"#,
        #"{"type":"pair.fail","reason":"No.","retryAfterMs":2147483648}"#,
    ])
    func shapesTheStationRefusesAreRefused(text: String) {
        #expect(throws: LinkCodecError.self) { try LinkCodec.decode(text) }
    }

    @Test func numberedFamiliesNeedAnIdFromOne() {
        let zero = #"{"type":"command.invoke","verb":"nnr.resetTuning","id":0,"args":[]}"#
        #expect(throws: LinkCodecError.self) { try LinkCodec.decode(zero) }
        let other = #"{"type":"command.invoke","verb":"notch.add","id":0,"args":[]}"#
        #expect((try? LinkCodec.decode(other)) != nil)
    }

    @Test func telemetryOverItsCapIsRefused() {
        let padding = String(repeating: "x", count: LinkCodec.telemetryCapBytes)
        let text = #"{"type":"station.metrics.v1","payload":{"pad":""# + padding + #""}}"#
        #expect(throws: LinkCodecError.self) { try LinkCodec.decode(text) }
    }
}
