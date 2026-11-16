// NereusSDR for iOS: the fake Core's band stack: its document on the bandStack object, and the bandStack verbs taken
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink

/// The fake Core's band stack: with ``Additions/bandStack`` it advertises
/// `bandStackVersion` 1, and its snapshot carries the `bandStack` object with
/// the suite's sample document at revision 1. It holds no stack of its own,
/// since the stack's rules are the Core's. A `bandStack.*` verb whose
/// arguments fit `surface.json` is accepted and noted in
/// ``bandStackCommands``, unless ``refuseNext(_:reason:)`` queued a refusal;
/// a test then sends the outcome with ``deliverBandStack(_:)``. Without the
/// addition the verbs are unknown, as on a Core before band stacks.
extension FakeStation {
    /// The mirrored class of the `bandStack` object.
    static let bandStackClass = "BandStackFacade"
    /// The Core's refusal of arguments the verb does not take.
    public static let bandStackNotUnderstoodReason = "The request for the band stack was not understood."

    static func bandStackCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        additions.contains(.bandStack) ? [("bandStackVersion", 1)] : []
    }

    /// The band stack commands the fake accepted, oldest first.
    public var bandStackCommands: [LinkMessage.CommandInvoke] {
        lock.withLock { bandStackCommandLog }
    }

    /// The document the next snapshot carries; its revision moves by one.
    public func setBandStack(_ json: String) {
        lock.withLock { bandStackDocument = (json, bandStackDocument.revision + 1) }
    }

    /// A new document, sent to the newest connection as the Core sends a change.
    public func deliverBandStack(_ json: String) async {
        setBandStack(json)
        let revision = lock.withLock { bandStackDocument.revision }
        await deliver(.delta(LinkMessage.Delta(key: "bandStack", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(revision)),
        ])))
    }

    /// The `bandStack` object as it stands now, encoded.
    func bandStackObject() -> String {
        let document = lock.withLock { bandStackDocument }
        return LinkCodec.encode(.objectCreate(LinkMessage.ObjectCreate(key: "bandStack", className: Self.bandStackClass,
                                                                       properties: [
            .init(ordinal: 0, name: "json", value: .utf8(document.json)),
            .init(ordinal: 1, name: "revision", value: .i64(document.revision)),
        ])))
    }

    /// The fake's answer to a `bandStack.*` verb; nil leaves any other to the rest of the fake.
    func bandStackReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        guard invoke.verb.hasPrefix("bandStack.") else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "") -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted, reason: reason,
                                                     affected: accepted ? ["bandStack"] : [], values: nil))
        }
        guard additions.contains(.bandStack), let arguments = Self.bandStackVerbs[invoke.verb] else {
            return [result(false, Self.unknownVerbReason)]
        }
        guard Self.fits(invoke, arguments) else {
            return [result(false, Self.bandStackNotUnderstoodReason)]
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        lock.withLock { bandStackCommandLog.append(invoke) }
        return [result(true)]
    }

    private struct VerbArgument: Sendable {
        let name: String
        let kind: String
        let optional: Bool
    }

    /// The `bandStack.*` verbs and their arguments, as `surface.json` declares them.
    private static let bandStackVerbs: [String: [VerbArgument]] = {
        let surface = try? LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        var verbs: [String: [VerbArgument]] = [:]
        for command in surface?["commands"] as? [[String: Any]] ?? [] {
            guard let verb = command["verb"] as? String, verb.hasPrefix("bandStack."),
                  let arguments = command["arguments"] as? [[String: Any]] else {
                continue
            }
            verbs[verb] = arguments.compactMap { argument in
                guard let name = argument["name"] as? String, let kind = argument["kind"] as? String else {
                    return nil
                }
                return VerbArgument(name: name, kind: kind, optional: argument["optional"] as? Bool ?? false)
            }
        }
        return verbs
    }()

    /// Exactly the verb's arguments at their kinds. `ids` goes as JSON text,
    /// since the link has no list kind.
    private static func fits(_ invoke: LinkMessage.CommandInvoke, _ arguments: [VerbArgument]) -> Bool {
        guard Set(invoke.args.map(\.name)).count == invoke.args.count,
              invoke.args.allSatisfy({ sent in
                  arguments.contains { $0.name == sent.name && $0.kind == sent.kind.rawValue }
              }),
              arguments.allSatisfy({ argument in argument.optional || invoke.args.contains { $0.name == argument.name } })
        else {
            return false
        }
        if case .utf8(let ids)? = invoke.args.first(where: { $0.name == "ids" })?.value {
            return (try? JSONDecoder().decode([String].self, from: Data(ids.utf8))) != nil
        }
        return true
    }
}

extension FakeStation.Additions {
    /// `bandStackVersion` 1, the `bandStack` object and the `bandStack.*` verbs. Not in ``all``.
    public static let bandStack = FakeStation.Additions(rawValue: 1 << 28)
}
