// NereusSDR for iOS: the Core's recent log and its logging categories, for the Support Bundle and Logs pages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror
import os

/// The Core's side of the desktop's Support dialog on the phone (link
/// document sections 7.1, 7.7 and 9.1, at `supportBundleVersion` 1): the
/// Core's recent log (the `coreLog` record stream, asked for only while a
/// page that shows it is open, as a window asks for it only while its
/// dialog is open) and the Core's logging categories (`radio`'s
/// `logCategories`, changed with `support.setLogCategories`, which turns on
/// exactly the ids it names). The categories and their labels come from a
/// ``LogCategorySource``; an id the Core has on that the list lacks is
/// listed by its id and kept on when the others change. One model serves
/// the Support Bundle page and Setup's Logs page (D95), so the two never
/// stop each other's log.
@MainActor
final class CoreLogModel: ObservableObject {
    static let minor: UInt16 = 11
    static let timeout: Duration = .seconds(10)

    /// One of the Core's logging categories, as its Support dialog lists it.
    struct Category: Equatable, Identifiable {
        let id: String
        let title: String
    }

    /// The Core's logging categories held on the phone (``PhoneHeldLogCategories``), for an older Core.
    static let known: [Category] = PhoneHeldLogCategories.list

    // MARK: Words

    static let notConnectedReason = "Connect to the Core to see its log."
    static let olderCoreReason =
        "This Core does not share its log or support bundle with this app. Updating the Core may help."
    static let readingText = "Reading the Core's log..."
    static let copiedText = "Copied"

    // MARK: State

    /// Every category the page lists: the known ones, then any other the Core has on.
    @Published private(set) var categories: [Category] = CoreLogModel.known
    /// The ids that are on, as the page shows them: a change on its way, else the Core's.
    @Published private(set) var on: Set<String> = []
    /// Why the categories and the log cannot be used now; nil when they can.
    @Published private(set) var reason: String?
    /// The Core's recent log, oldest first.
    @Published private(set) var lines: [CoreLogLine] = []
    /// The Core's words for the last change it refused, or for a log it would not send.
    @Published private(set) var note: String?

    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.corelog")

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let records: RecordStreamClient?
    private let source: LogCategorySource
    private var watch: ToolMirrorWatch?
    /// How many open pages show the log; it is asked for while any does.
    private var viewers = 0
    private var open: Bool { viewers > 0 }
    /// This model asked for the log, so it stops asking when the page closes.
    private var wanting = false
    private var mirrored: Set<String> = []
    /// The categories the operator chose, shown until the Core answers them or refuses.
    private let pending = PendingChoice<Set<String>>()
    private var queued: Set<String>?
    private var sending = false
    /// The Core's refusal of the log as last shown, so the note goes when it does.
    private var streamRefusal: String?

    /// `source` gives the categories and their labels; by default the
    /// Core's own (``CoreLogCategories``), else the phone's list.
    init(mirror: MirrorStore, commands: CommandClient?, records: RecordStreamClient?,
         categories source: LogCategorySource? = nil) {
        self.mirror = mirror
        self.commands = commands
        self.records = records
        self.source = source ?? CoreLogCategories(mirror: mirror)
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        if let records {
            watch.watch(records.$streams)
            watch.watch(records.$available)
            watch.watch(records.$refusals)
        }
        self.watch = watch
        refresh()
    }

    /// A page that shows the log opened or closed: the log is asked for
    /// while at least one does. Each page calls it once as it opens and
    /// once as it closes.
    func setOpen(_ shown: Bool) {
        viewers = shown ? viewers + 1 : max(0, viewers - 1)
        refresh()
    }

    func refresh() {
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        let shares = connected && (mirror.agreedMinor ?? 0) >= Self.minor
            && mirror.capabilityVersion(CoreLogLine.capabilityName) >= 1
        let radio = watch?.object("radio")
        func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<CoreLogModel, Value>, _ value: Value) {
            if self[keyPath: path] != value {
                self[keyPath: path] = value
            }
        }
        set(\.reason, !connected ? Self.notConnectedReason : shares ? nil : Self.olderCoreReason)
        mirrored = shares ? Set(CoreLogLine.categories(ToolValue.text(radio?[CoreLogLine.categoriesProperty]) ?? ""))
                          : []
        pending.follow(radio) { values in
            Set(CoreLogLine.categories(ToolValue.text(values[CoreLogLine.categoriesProperty]) ?? ""))
        }
        if !shares {
            pending.drop()
            queued = nil
        } else {
            pending.settle(mirrored: mirrored, sending: sending)
        }
        let shown = pending.value ?? mirrored
        set(\.on, shown)
        set(\.categories, Self.listed(offered: source.categories(radio: radio), on: shown))

        let follows = shares && records?.available == true
        if follows && open {
            wanting = true
            records?.want(CoreLogLine.streamName, backlog: CoreLogLine.capacity, by: self)
        } else if wanting {
            wanting = false
            records?.unwant(CoreLogLine.streamName, by: self)
        }
        set(\.lines, follows ? (records?.records(CoreLogLine.streamName) ?? []).map(CoreLogLine.init(record:)) : [])
        // The Core's refusal of the log shows while it stands, and goes when
        // the Core sends the log again or the page stops asking.
        let refused = (follows ? records?.refusals[CoreLogLine.streamName] : nil)
            .map { $0.isEmpty ? BandSlicesModel.refusedText : $0 }
        if refused != streamRefusal {
            if let refused {
                set(\.note, refused)
            } else if note == streamRefusal {
                set(\.note, nil)
            }
            streamRefusal = refused
        }
    }

    /// The log as the page shows it and as Copy takes it.
    var text: String {
        if let reason {
            return reason
        }
        return lines.isEmpty ? Self.readingText : lines.map(\.line).joined(separator: "\n")
    }

    // MARK: Changing

    /// The list the page shows: the source's list (the phone-held
    /// ``known`` one when nil), then any other id that is on, listed by its id.
    static func listed(offered: [Category]?, on: Set<String>) -> [Category] {
        let base = offered ?? known
        let others = on.subtracting(base.map(\.id)).sorted().map { Category(id: $0, title: $0) }
        return base + others
    }

    func setCategory(_ id: String, _ isOn: Bool) {
        var next = on
        if isOn {
            next.insert(id)
        } else {
            next.remove(id)
        }
        send(next)
    }

    /// Turns every category the page lists on, or every one off.
    func setAll(_ isOn: Bool) {
        send(isOn ? Set(categories.map(\.id)) : [])
    }

    /// Reads the Core's log again from its newest lines.
    func reload() {
        guard reason == nil, wanting else {
            return
        }
        records?.resubscribe(CoreLogLine.streamName)
    }

    private func send(_ next: Set<String>) {
        guard reason == nil, next != on else {
            return
        }
        pending.choose(next)
        on = next
        if sending {
            queued = next
            return
        }
        guard let commands else {
            pending.drop()
            refresh()
            return
        }
        sending = true
        let order = categories
        Task { [weak self] in
            var current: Set<String>? = next
            while let sending = current {
                var accepted = false
                do {
                    let result = try await commands.invoke(CoreLogLine.setCategoriesVerb, arguments: [
                        CommandArgument(name: "categories", value: .text(Self.joined(sending, order: order))),
                    ], timeout: Self.timeout)
                    accepted = result.accepted
                    self?.noteResult(result.accepted, result.reason)
                } catch {
                    Self.logger.info("A logging change had no answer from the Core")
                }
                guard let self else {
                    return
                }
                current = self.queued
                self.queued = nil
                if !accepted && current == nil {
                    self.pending.drop()
                }
            }
            self?.sending = false
            self?.refresh()
        }
    }

    /// The ids joined by commas, the listed ones in their order first.
    static func joined(_ ids: Set<String>, order: [Category] = known) -> String {
        let ordered = order.map(\.id).filter(ids.contains) + ids.subtracting(order.map(\.id)).sorted()
        return ordered.joined(separator: ",")
    }

    private func noteResult(_ accepted: Bool, _ reason: String) {
        if accepted {
            if note != nil {
                note = nil
            }
        } else {
            // A refusal with no words still says it was refused.
            note = reason.isEmpty ? BandSlicesModel.refusedText : reason
        }
    }
}
