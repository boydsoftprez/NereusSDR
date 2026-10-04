// NereusSDR for iOS: strict coordinated Diversity summary and exact participant action regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// Existing-model causal RED observed before adding these typed regressions.
/// Removing any required-kind/range/state validation must fail one of these tests.
@Suite struct DiversityControlTests {
    static let live = """
    {"sliceId":1,"incarnation":101,"controlRevision":4,"controllerDeviceId":"phone","letter":"B",
     "band":5,"frequencyHz":14200000,"phaseDeg":123.4,"gainDb":-2,"fineNullEnabled":false,"pattern":null}
    """
    static let state = """
    {"version":1,"revision":12,"requested":true,"running":true,"paused":false,"reasonCode":"","reason":"",
     "live":\(live),"targets":[
        {"sliceId":1,"incarnation":101,"controlRevision":4,"controllerDeviceId":"phone","eligible":true,"reasonCode":"","reason":""},
        {"sliceId":2,"incarnation":102,"controlRevision":5,"controllerDeviceId":"phone","eligible":true,"reasonCode":"","reason":""}]}
    """

    @Test func requiredFieldsAndKinds() throws {
        guard case .object(let good) = try LinkJSON.parse(Self.state) else { return }
        for key in good.keys {
            var value = good
            value.removeValue(forKey: key)
            #expect(DiversityState(json: LinkJSON.object(value).compactText) == nil, "missing \(key)")
        }
        for (key, wrong) in ["version": LinkJSON.bool(true), "revision": .string("12"),
                             "requested": .number(1), "running": .string("true"), "paused": .number(0),
                             "reasonCode": .null, "reason": .bool(false), "live": .array([]), "targets": .object([:])] {
            var value = good
            value[key] = wrong
            #expect(DiversityState(json: LinkJSON.object(value).compactText) == nil, "kind \(key)")
        }
        #expect(DiversityState(json: Self.state) != nil)
    }

    @Test func nestedRequiredFieldsAndKinds() throws {
        guard case .object(var top) = try LinkJSON.parse(Self.state),
              case .object(let live)? = top["live"], case .array(let targets)? = top["targets"],
              case .object(let target) = targets[0] else { Issue.record("fixture shape"); return }
        for key in live.keys {
            var edited = live; edited.removeValue(forKey: key); top["live"] = .object(edited)
            #expect(DiversityState(json: LinkJSON.object(top).compactText) == nil, "live missing \(key)")
        }
        top["live"] = .object(live)
        for key in target.keys {
            var edited = target; edited.removeValue(forKey: key); top["targets"] = .array([.object(edited), targets[1]])
            #expect(DiversityState(json: LinkJSON.object(top).compactText) == nil, "target missing \(key)")
        }
        for (key, wrong) in ["sliceId": LinkJSON.bool(true), "incarnation": .string("101"),
                             "controlRevision": .null, "controllerDeviceId": .number(1), "eligible": .number(1),
                             "reasonCode": .null, "reason": .number(1)] {
            var edited = target; edited[key] = wrong; top["targets"] = .array([.object(edited), targets[1]])
            #expect(DiversityState(json: LinkJSON.object(top).compactText) == nil, "target kind \(key)")
        }
    }

    @Test func offSummaryAndEndpointBounds() throws {
        var json = Self.state.replacingOccurrences(of: "\"requested\":true", with: "\"requested\":false")
            .replacingOccurrences(of: "\"running\":true", with: "\"running\":false")
            .replacingOccurrences(of: "\"live\":\(Self.live)", with: "\"live\":null")
        let off = try #require(DiversityState(json: json))
        #expect(off.live == nil && !off.requested && off.targets.count == 2)
        #expect(DiversityTargetAction(state: off, enabled: true, targetSliceId: 2)?.arguments.prefix(5) == [
            .init(name: "enabled", value: .bool(true)), .init(name: "stateRevision", value: .int(12)),
            .init(name: "sourceSliceId", value: .int(-1)), .init(name: "sourceIncarnation", value: .int(0)),
            .init(name: "sourceControlRevision", value: .int(0)),
        ])
        for (old, new) in [("123.4", "0"), ("123.4", "360"), ("\"gainDb\":-2", "\"gainDb\":-20"),
                           ("\"gainDb\":-2", "\"gainDb\":20"), ("14200000", "0"),
                           ("\"revision\":12", "\"revision\":9007199254740991")] {
            #expect(DiversityState(json: Self.state.replacingOccurrences(of: old, with: new)) != nil)
        }
        json = Self.state.replacingOccurrences(of: "14200000", with: "1e309")
        #expect(DiversityState(json: json) == nil)
        #expect(LinkFeatures.app["diversityControl"] == 1)
        #expect(DiversityState.ordinal == 37 && DiversityState.minimumMinor == 11)
    }

    @Test func unknownVersionsAndInconsistentStates() {
        for (old, new) in [("\"version\":1", "\"version\":2"),
                           ("\"revision\":12", "\"revision\":-1"),
                           ("\"revision\":12", "\"revision\":9007199254740992"),
                           ("\"requested\":true", "\"requested\":false"),
                           ("\"paused\":false", "\"paused\":true"),
                           ("\"live\":\(Self.live)", "\"live\":null")] {
            #expect(DiversityState(json: Self.state.replacingOccurrences(of: old, with: new)) == nil)
        }
    }

    @Test func participantAndBlendBounds() {
        for (old, new) in [("\"sliceId\":1", "\"sliceId\":5"),
                           ("\"incarnation\":101", "\"incarnation\":9007199254740992"),
                           ("\"controlRevision\":4", "\"controlRevision\":4.5"),
                           ("\"band\":5", "\"band\":28"),
                           ("\"frequencyHz\":14200000", "\"frequencyHz\":-1"),
                           ("\"phaseDeg\":123.4", "\"phaseDeg\":360.1"),
                           ("\"gainDb\":-2", "\"gainDb\":20.1"),
                           ("\"fineNullEnabled\":false", "\"fineNullEnabled\":0"),
                           ("\"pattern\":null", "\"pattern\":[]")] {
            #expect(DiversityState(json: Self.state.replacingOccurrences(of: old, with: new)) == nil)
        }
        #expect(DiversityState(json: Self.state.replacingOccurrences(of: "14200000", with: "144200000"))?.live?.frequencyHz == 144200000)
        #expect(DiversityState(json: Self.state.replacingOccurrences(of: "14200000", with: "1e300"))?.live?.frequencyHz == 1e300)
    }

    @Test func completeRosterIsBoundedAndCannotDuplicateAnIdentity() throws {
        guard case .object(var object) = try LinkJSON.parse(Self.state),
              case .array(let targets)? = object["targets"] else { return }
        object["targets"] = .array(Array(repeating: targets[0], count: 6))
        #expect(DiversityState(json: LinkJSON.object(object).compactText) == nil)
        object["targets"] = .array([targets[0], targets[0]])
        #expect(DiversityState(json: LinkJSON.object(object).compactText) == nil)
        object["targets"] = .array(targets)
        #expect(DiversityState(json: LinkJSON.object(object).compactText) != nil)
    }

    @Test func pausedKeepsTheExactOwnerAndUnknownReasonWords() throws {
        let json = Self.state.replacingOccurrences(of: "\"running\":true", with: "\"running\":false")
            .replacingOccurrences(of: "\"paused\":false", with: "\"paused\":true")
            .replacingOccurrences(of: "\"reasonCode\":\"\",\"reason\":\"\",\n", with: "\"reasonCode\":\"futureReason\",\"reason\":\"Core resource words.\",\n")
        let state = try #require(DiversityState(json: json))
        #expect(state.live?.identity.sliceId == 1)
        #expect(state.requested && !state.running && state.paused)
        #expect(state.reason == "Core resource words.")
    }

    @Test func decodedZeroIdentityIsNeverAnAdmittedParticipantAction() throws {
        // JSON bounds permit zero; Core command admission requires positive incarnation and control revision.
        for (old, new) in [("\"incarnation\":101", "\"incarnation\":0"),
                           ("\"controlRevision\":4", "\"controlRevision\":0")] {
            let state = try #require(DiversityState(json: Self.state.replacingOccurrences(of: old, with: new)))
            #expect(DiversityTargetAction(state: state, enabled: false, targetSliceId: nil) == nil)
            #expect(DiversityTargetAction(state: state, enabled: true, targetSliceId: 2) == nil)
        }
    }

    @Test func exactMoveArgumentsCarryBothParticipantsWithoutCallerIdentity() throws {
        let state = try #require(DiversityState(json: Self.state))
        let action = try #require(DiversityTargetAction(state: state, enabled: true, targetSliceId: 2))
        #expect(action.arguments == [
            .init(name: "enabled", value: .bool(true)), .init(name: "stateRevision", value: .int(12)),
            .init(name: "sourceSliceId", value: .int(1)), .init(name: "sourceIncarnation", value: .int(101)),
            .init(name: "sourceControlRevision", value: .int(4)), .init(name: "targetSliceId", value: .int(2)),
            .init(name: "targetIncarnation", value: .int(102)), .init(name: "targetControlRevision", value: .int(5)),
        ])
        let off = try #require(DiversityTargetAction(state: state, enabled: false, targetSliceId: nil))
        #expect(off.arguments.suffix(3) == [
            .init(name: "targetSliceId", value: .int(-1)), .init(name: "targetIncarnation", value: .int(0)),
            .init(name: "targetControlRevision", value: .int(0)),
        ])
    }
}
