// NereusSDR for iOS: the fake Core's answers for the display controls: grid settings, slices on a pan, notches
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// What the fake Core answers for the phone's display controls, as the Core
/// answers them:
///
/// - a `settings.write` of a per-band grid key (`DisplayGridMax_<band>`,
///   `DisplayGridMin_<band>`, Station scope at src/core/session/SettingsScope.cpp:494)
///   is kept and echoed with the writer's origin (link section 8.1), or
///   refused with the reason ``refuseNextSetting(_:reason:)`` queued,
///   carrying the value the fake kept;
/// - `addSliceOnPan {panId}` adds a slice on that pan, up to the
///   `effectiveMaxSlices` the fake's capabilities sent, then refuses in the
///   Core's words (``sliceCapReason(_:)``);
/// - `notch.add {sliceId, centreHz, widthHz}` records the notch
///   (``notchesAdded``) and answers with the list's revision and the new id;
/// - a `property.write` of a slice's `panKey` moves the slice to that pan
///   (two-way at src/core/session/MirrorPolicy.cpp:281-287).
///
/// A refusal queued with ``refuseNext(_:reason:)`` under `addSliceOnPan`
/// or `notch.add` answers the next one, as for any verb.
extension FakeStation {
    /// The verbs the display controls send.
    public static let addSliceOnPanVerb = "addSliceOnPan"
    public static let addNotchVerb = "notch.add"
    /// The Core's refusal of a notch change it cannot read
    /// (src/core/session/SessionCommandDispatcher.cpp:2394). An
    /// `addSliceOnPan` without `panId` gets ``unreadableRequestReason``
    /// (SessionCommandDispatcher.cpp:2631).
    public static let unreadableNotchReason = "This notch change is not one this Core understands."

    /// The Core's refusal of a slice past the radio's limit, for a radio
    /// that does not name itself, as RadioModel::sliceCapReason words it.
    // From src/models/RadioModel.cpp:15730-15749 ("%1 supports a maximum of %2", "This radio", "1 slice" / "%1 slices")
    public static func sliceCapReason(_ cap: Int) -> String {
        let slices = cap == 1 ? "1 slice" : "\(cap) slices"
        return "This radio supports a maximum of \(slices)"
    }

    /// The display controls' state, behind its own lock.
    final class DisplayState: @unchecked Sendable {
        private let lock = NSLock()
        private var settings: [String: [LinkMessage.PropertyEntry]] = [:]
        private var pans: [Int64: String] = [:]
        private var notches: [(sliceId: String, centreHz: Double, widthHz: Double)] = []

        /// The fixture's slices and their pans.
        func seed(slices: Set<Int64>, pans seeded: [Int64: String]) {
            lock.withLock {
                for slice in slices {
                    pans[slice] = seeded[slice] ?? ""
                }
            }
        }

        func read<Value>(_ body: (inout [String: [LinkMessage.PropertyEntry]],
                                  inout [Int64: String],
                                  inout [(sliceId: String, centreHz: Double, widthHz: Double)]) -> Value) -> Value {
            lock.withLock { body(&settings, &pans, &notches) }
        }
    }

    /// The notches `notch.add` recorded, oldest first: the slice's id as
    /// the app sent it (`"0"` for `slice:0`), the centre and the width.
    public var notchesAdded: [(sliceId: String, centreHz: Double, widthHz: Double)] {
        displayState.read { _, _, notches in notches }
    }

    /// The pan the fake holds slice `sliceId` on, or nil for a slice it does not have.
    public func panKey(ofSlice sliceId: Int64) -> String? {
        displayState.read { _, pans, _ in pans[sliceId] }
    }

    /// The next `settings.write` of `key` is refused with `reason`, and the
    /// refusal carries the value the fake kept, as the Core restores its own.
    public func refuseNextSetting(_ key: String, reason: String) {
        refuseNext(key, reason: reason)
    }

    /// Whether `key` is one of the Core's per-band grid settings.
    static func isDisplayGridKey(_ key: String) -> Bool {
        key.hasPrefix("DisplayGridMax_") || key.hasPrefix("DisplayGridMin_")
    }

    /// The fake's answer to a `settings.write` of a grid key; nil leaves
    /// any other key to the rest of the fake.
    func displaySettingReplies(_ write: LinkMessage.SettingsWrite) -> [LinkMessage]? {
        guard Self.isDisplayGridKey(write.key) else {
            return nil
        }
        if let reason = takeRefusal(write.key) {
            let kept = displayState.read { settings, _, _ in settings[write.key] ?? [] }
            return [.settingsReject(LinkMessage.SettingsReject(key: write.key, properties: kept, reason: reason))]
        }
        displayState.read { settings, _, _ in settings[write.key] = write.properties }
        return [.settingsValue(LinkMessage.SettingsValue(key: write.key, origin: write.origin,
                                                         properties: write.properties))]
    }

    /// The fake's answer to `addSliceOnPan` or `notch.add`; nil leaves any
    /// other verb to the rest of the fake.
    func displayReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        guard invoke.verb == Self.addSliceOnPanVerb || invoke.verb == Self.addNotchVerb else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "", affected: [String] = [],
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: affected, values: values))
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        if invoke.verb == Self.addSliceOnPanVerb {
            return addSliceReplies(invoke, result: result)
        }
        return addNotchReplies(invoke, result: result)
    }

    private func addSliceReplies(_ invoke: LinkMessage.CommandInvoke,
                                 result: (Bool, String, [String], [LinkMessage.PropertyEntry]?) -> LinkMessage)
        -> [LinkMessage] {
        // The Core reads `panId` alone (SessionCommandDispatcher.cpp:2628-2632).
        guard case .utf8(let panId)? = invoke.args.first(where: { $0.name == "panId" })?.value else {
            return [result(false, Self.unreadableRequestReason, [], nil)]
        }
        let cap = effectiveMaxSlices ?? Int.max
        let added = displayState.read { _, pans, _ -> Int64? in
            guard pans.count < cap else {
                return nil
            }
            // The fake gives the lowest free index; the new slice carries
            // only its index and pan, the rest at their stand-ins.
            let index = (Int64(0)...).first { pans[$0] == nil } ?? 0
            pans[index] = panId
            return index
        }
        guard let added else {
            return [result(false, Self.sliceCapReason(cap), [], nil)]
        }
        let key = "slice:\(added)"
        var replies = [result(true, "", [key], nil)]
        if let create = try? Self.objectCreate(key: key, className: "SliceModel",
                                               values: ["sliceIndex": .i64(added), "panKey": .utf8(panId)]) {
            replies.append(.objectCreate(create))
        }
        return replies
    }

    private func addNotchReplies(_ invoke: LinkMessage.CommandInvoke,
                                 result: (Bool, String, [String], [LinkMessage.PropertyEntry]?) -> LinkMessage)
        -> [LinkMessage] {
        func argument(_ name: String) -> LinkMessage.PropertyValue? {
            invoke.args.first { $0.name == name }?.value
        }
        // The Core reads exactly these three, of these kinds (SessionCommandDispatcher.cpp:2352-2359).
        guard invoke.args.count == 3, case .i64(let sliceId)? = argument("sliceId"),
              case .f64(let centreHz)? = argument("centreHz"), case .f64(let widthHz)? = argument("widthHz"),
              centreHz.isFinite, widthHz.isFinite else {
            return [result(false, Self.unreadableNotchReason, [], nil)]
        }
        guard panKey(ofSlice: sliceId) != nil else {
            return [result(false, Self.unknownSliceReason, [], nil)]
        }
        displayState.read { _, _, notches in notches.append((String(sliceId), centreHz, widthHz)) }
        if additions.contains(.setupPanels) {
            // The Setup panels' notch list grows, and its delta follows.
            let (id, revision) = withSetupPanels { state -> (Int64, Int64) in
                let id = state.nextNotchId
                state.nextNotchId += 1
                state.notches.append(SetupNotch(id: id, centreHz: centreHz, widthHz: widthHz, active: true))
                state.notchRevision += 1
                return (id, state.notchRevision)
            }
            return [result(true, "", ["notches"], [.init(name: "revision", value: .i64(revision)),
                                                   .init(name: "id", value: .i64(id))]),
                    notchesDelta()]
        }
        let count = lock.withLock { () -> Int64 in
            notchCount += 1
            return notchCount
        }
        return [result(true, "", ["notches"], [.init(name: "revision", value: .i64(count)),
                                               .init(name: "id", value: .i64(count))])]
    }

    /// The key ``refuseNext(_:reason:)`` takes to refuse the next `panKey`
    /// write to `slice:<sliceId>`, with the reason given (empty for a Core
    /// that gives none).
    public static func panKeyRefusalKey(sliceId: Int) -> String {
        "panKey:slice:\(sliceId)"
    }

    /// The fake's answer to a `property.write` of a slice's `panKey` alone:
    /// the slice moves, a result when the write asks for one, then the
    /// delta; or, refused (``panKeyRefusalKey(sliceId:)``), only the
    /// refusing result. Nil leaves any other write to the rest of the fake.
    func slicePanReplies(_ write: LinkMessage.PropertyWrite) -> [LinkMessage]? {
        let prefix = "slice:"
        guard write.key.hasPrefix(prefix), let sliceId = Int64(write.key.dropFirst(prefix.count)),
              !write.properties.isEmpty, write.properties.allSatisfy({ $0.name == "panKey" }),
              case .utf8(let pan)? = write.properties.last?.value else {
            return nil
        }
        if let reason = takeRefusal(Self.panKeyRefusalKey(sliceId: Int(sliceId))) {
            guard let writeId = write.writeId, writeId != 0 else {
                return []
            }
            return [.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: writeId, results: [
                .init(property: "panKey", accepted: false, reason: reason, value: nil),
            ]))]
        }
        let moved = displayState.read { _, pans, _ -> Bool in
            guard pans[sliceId] != nil else {
                return false
            }
            pans[sliceId] = pan
            return true
        }
        guard moved else {
            return nil
        }
        let ordinal = (try? Self.schema(ofClass: "SliceModel"))?.fields.first { $0.name == "panKey" }?.ordinal
            ?? write.properties.last?.ordinal ?? 0
        let entry = LinkMessage.PropertyEntry(ordinal: ordinal, name: "panKey", value: .utf8(pan))
        var replies: [LinkMessage] = []
        if let writeId = write.writeId, writeId != 0 {
            replies.append(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: writeId, results: [
                .init(property: "panKey", accepted: true, reason: "", value: entry),
            ])))
        }
        replies.append(.delta(LinkMessage.Delta(key: write.key, properties: [entry])))
        return replies
    }
}
