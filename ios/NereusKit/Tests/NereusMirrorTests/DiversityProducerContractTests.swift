// NereusSDR for iOS: actual Diversity v1 producer summaries/results.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// This producer-pinned corpus has its own root. The phone branch's older
/// Core corpus and ordinal/count tests continue to describe that older Core.
@Suite struct DiversityProducerContractTests {
    static var root: URL {
        Bundle.module.url(forResource: "DiversityProducerV1", withExtension: nil)!
    }

    static func object(_ path: String) throws -> [String: Any] {
        try LinkFixtureLoader.jsonObject(at: root.appendingPathComponent(path))
    }

    static func classes() throws -> [String: [[String: Any]]] {
        let all = try #require(try Self.object("surface.json")["mirrorClasses"] as? [String: [String: Any]])
        return try all.mapValues { try #require($0["properties"] as? [[String: Any]]) }
    }

    static func fixture(_ name: String) throws -> SessionFixture {
        let value = try Self.object("sessions/diversity-control-\(name).json")
        let runs = try #require(value["runs"] as? [String])
        let steps = try #require(value["steps"] as? [[String: Any]])
        return SessionFixture(id: "session-diversity-control-\(name)", runs: runs, steps: steps)
    }

    @Test func boundedManifestAndSurfaceContract() throws {
        let manifest = try Self.object("manifest.json")
        let entries = try #require(manifest["fixtures"] as? [[String: Any]])
        #expect(entries.count == 3)
        #expect(entries.filter { $0["kind"] as? String == "session" }.count == 2)
        #expect(entries.filter { $0["kind"] as? String == "control" }.count == 1)
        let pin = try Self.object("producer-identity.json")
        #expect(pin["baseCommit"] as? String == "f4077ae9acf169ad4030f0ffccd2e3b021bff6a9")
        let surface = try Self.object("surface.json")
        let capabilities = try #require(surface["capabilities"] as? [[String: Any]])
        #expect(capabilities.count == 107)
        #expect(capabilities[88]["name"] as? String == "diversityControlVersion")
        #expect((capabilities[88]["value"] as? NSNumber)?.intValue == 1)
        let version = try #require(capabilities.first { $0["name"] as? String == DiversityState.capabilityName })
        #expect(version["kind"] as? String == "i64")
        #expect((version["value"] as? NSNumber)?.intValue == 1)
        let commands = try #require(surface["commands"] as? [[String: Any]])
        let command = try #require(commands.first { $0["verb"] as? String == "diversity.setTarget" })
        #expect((command["minMinor"] as? NSNumber)?.intValue == Int(DiversityState.minimumMinor))
        #expect(command["capability"] as? String == DiversityState.capabilityName)
        #expect((command["capabilityVersion"] as? NSNumber)?.intValue == 1)
        let args = try #require(command["arguments"] as? [[String: Any]])
        #expect(args.compactMap { $0["name"] as? String } == ["enabled", "stateRevision", "sourceSliceId",
            "sourceIncarnation", "sourceControlRevision", "targetSliceId", "targetIncarnation", "targetControlRevision"])
        #expect(args.compactMap { $0["kind"] as? String } == ["bool"] + Array(repeating: "i64", count: 7))
        #expect(args.allSatisfy { $0["optional"] as? Bool == false })
        let radio = try #require(try Self.classes()["RadioModel"])
        #expect(radio.count == 38)
        let state = try #require(radio.first { $0["name"] as? String == DiversityState.propertyName })
        #expect((state["ordinal"] as? NSNumber)?.intValue == Int(DiversityState.ordinal))
        #expect(state["kind"] as? String == "utf8")
        #expect(state["direction"] as? String == "outbound")
    }

    @Test func producerControlVectorDecodesWithBothParticipants() throws {
        let vector = try Self.object("control/client-diversity-set-target.json")
        let wire = try LinkJSON(foundation: #require(vector["wire"]))
        guard case .commandInvoke(let command) = try LinkCodec.decode(wire.compactText) else {
            Issue.record("not a command.invoke"); return
        }
        #expect(command.verb == "diversity.setTarget" && command.args.count == 8)
        #expect(command.args[2].value == .i64(0))
        #expect(command.args[3].value == .i64(123))
        #expect(command.args[4].value == .i64(4))
        #expect(command.args[5].value == .i64(2))
        #expect(command.args[6].value == .i64(124))
        #expect(command.args[7].value == .i64(2))
    }

    @Test(arguments: ["pattern", "no-pattern"])
    func producerSessionsPassBothExistingTransportRunners(_ name: String) async throws {
        let fixture = try Self.fixture(name)
        #expect(fixture.runs == ["station", "app"])
        #expect(fixture.featuresTheAppLacks.isEmpty)
        let classes = try Self.classes()
        for mode in [SessionFixturePlayer.TransportMode.webSocket, .dataChannel] {
            let failures = try await SessionFixturePlayer(fixture: fixture, mirrorClasses: classes,
                                                           transportMode: mode,
                                                           producerDeviceIdentityReferences: true).play()
            #expect(failures.isEmpty, "\(name): \(failures)")
        }
    }

    @Test(arguments: ["pattern", "no-pattern"])
    func actualProducerStatesResultsAndActionsUseTheProductionConsumer(_ name: String) throws {
        let fixture = try Self.fixture(name)
        var placeholders = FixturePlaceholders(mirrorClasses: try Self.classes(), certificateSHA256: Data(count: 32))
        placeholders.record["device:self"] = .string("synthetic-fixture-device")
        var current: DiversityState?
        var results: [CommandResult] = []
        var rebuiltActions = 0
        var summaries = 0
        for (index, step) in fixture.steps.enumerated() {
            guard let raw = step["message"] else { continue }
            let json = try LinkJSON(foundation: raw)
            if step["from"] as? String == "client" {
                guard (raw as? [String: Any])?["verb"] as? String == "diversity.setTarget" else { continue }
                let filled = try placeholders.fillDiversityScriptedIdentity(json, at: "command \(index)")
                guard case .commandInvoke(let invoke) = try LinkCodec.decode(filled.compactText) else { continue }
                // The accepted source-free enable, move and off are rebuilt
                // from the producer's immediately preceding current summary.
                if [1901, 1903, 1904].contains(invoke.id) {
                    let state = try #require(current)
                    let enabled = invoke.args[0].value == .bool(true)
                    guard case .i64(let target) = invoke.args[5].value else { Issue.record("target kind"); continue }
                    let action = try #require(DiversityTargetAction(state: state, enabled: enabled,
                                                                  targetSliceId: enabled ? Int(target) : nil))
                    #expect(action.arguments == invoke.args.map { CommandArgument(name: $0.name, value: MirrorValue($0.value)) })
                    rebuiltActions += 1
                }
                continue
            }
            guard step["from"] as? String == "station" else { continue }
            let filled = try placeholders.fillStation(json, at: "station \(index)")
            let decoded = try LinkCodec.decode(filled.compactText)
            let properties: [LinkMessage.PropertyEntry]
            switch decoded {
            case .objectCreate(let object) where object.key == "radio": properties = object.properties
            case .delta(let delta) where delta.key == "radio": properties = delta.properties
            case .commandResult(let wire) where wire.verb == "diversity.setTarget":
                let result = CommandResult(wire)
                #expect(result.isFinal)
                guard case .text(let text)? = result.values["diversityState"],
                      case .text(let code)? = result.values["reasonCode"] else {
                    Issue.record("producer result values missing"); continue
                }
                let summary = try #require(DiversityState(json: text))
                if !result.accepted {
                    #expect(result.affectedKeys.isEmpty)
                    #expect(summary == current, "refused result must report current state without mutation")
                    #expect(code == (wire.id == 1902 ? "stateChanged" : "invalidRequest"))
                } else {
                    #expect(code.isEmpty)
                    #expect(result.affectedKeys == (wire.id == 1901 ? ["radio", "slice:0"]
                        : wire.id == 1903 ? ["radio", "slice:0", "slice:1"] : ["radio", "slice:1"]))
                }
                if summary.requested {
                    #expect(summary.paused && !summary.running)
                    #expect(summary.reasonCode == "resourcesUnavailable")
                    #expect(summary.reason == "Diversity is waiting for the receiver pair.")
                    #expect(summary.live?.pattern == nil ? name == "no-pattern" : name == "pattern")
                    #expect(summary.live?.identity.controllerDeviceId == "synthetic-fixture-device")
                }
                results.append(result)
                current = summary
                properties = []
            default: properties = []
            }
            for property in properties where property.name == DiversityState.propertyName {
                #expect(property.ordinal == DiversityState.ordinal && property.kind == .utf8)
                guard case .utf8(let text) = property.value else { Issue.record("summary kind"); continue }
                current = try #require(DiversityState(json: text))
                summaries += 1
            }
        }
        #expect(summaries >= 4)
        #expect(rebuiltActions == 3)
        try #require(results.count == 5)
        #expect(results.map(\.accepted) == [true, false, true, true, false])
        #expect(results[2].reason == "Diversity moved from A to B. Both slices paused briefly.")
        let final = try #require(current)
        #expect(!final.requested && !final.running && !final.paused && final.live == nil)
        #expect(final.targets.map { $0.identity.sliceId } == [0, 1])
    }
}
