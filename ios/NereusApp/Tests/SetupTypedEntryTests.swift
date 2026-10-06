// NereusSDR for iOS: typed values on Setup's number rows: the number pad a row opens, what it sends, and pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18 (JJ 2026-09-30): every Setup number row also takes a typed
/// value. Tapping the value, or VoiceOver's "Enter value" on the row, opens
/// the number pad with the row's unit, range and decimal places from the
/// Core's description; a whole-number row has no decimal point. The typed
/// value goes the way minus and plus send theirs; the Core's refusal shows
/// on the pad as sent. A greyed row opens no pad and is dimmed as a whole.
/// With `NEREUS_MAIN_SHOTS` set, the pictures are written there.
@Suite("Setup typed entry", .serialized)
@MainActor
struct SetupTypedEntryTests {
    typealias Pages = SetupDescribedPagesTests

    @Test("a submitted Setup pad restores latest Core at timeout; a newer draft retains its digits and note owner", arguments: [false, true])
    func timeoutRestoresOnlySubmittedSetupEntry(newerEntry: Bool) async throws {
        let clock = TestLinkClock()
        let rig = try DisplaySheetTests.Rig(clock: clock)
        let proxy = rig.settings
        let key = "CWPitch"
        proxy.apply(.settingsSnapshot(.init(properties: [.init(name: key, value: .utf8("600"))])))
        var notes: [PropertyWriteOutcome] = []
        var closed = 0
        let pad = SetupNumberRow.pad(title: "CW pitch", value: 600, range: 200...1_200, unit: "Hz", decimals: 0,
                                     send: { await proxy.write(key, String(Int($0))).propertyOutcome },
                                     sendWithLate: { value, late in
            await proxy.write(key, String(Int(value)), onLateOutcome: { late($0.propertyOutcome) }).propertyOutcome
        }, onOutcome: { notes.append($0) }, readCurrent: { proxy.value(key).flatMap(Double.init) }, close: { closed += 1 })
        Self.type("650", on: pad)
        let entered = Task { await pad.enter() }
        #expect(await Pages.settle { rig.recorded.settingsSent.contains { message in
            if case .settingsWrite(let write) = message { return write.key == key }
            return false
        } })
        proxy.apply(.settingsValue(.init(key: key, origin: "another-phone", properties: [.init(name: key, value: .utf8("720"))])))
        if newerEntry { Self.type("655", on: pad) }
        await clock.advance(by: 5_000)
        #expect(await entered.value == false)
        #expect(proxy.value(key) == "720")
        #expect(pad.value == (newerEntry ? 655 : 720))
        #expect(pad.refusal == (newerEntry ? nil : PropertyWriteOutcome.notConfirmed.reason))
        #expect(notes == (newerEntry ? [] : [.notConfirmed]))
        proxy.apply(.settingsReject(.init(key: key, properties: [.init(name: key, value: .utf8("720"))], reason: "Current CW pitch refusal.")))
        #expect(pad.value == (newerEntry ? 655 : 720))
        #expect(pad.refusal == (newerEntry ? nil : "Current CW pitch refusal."))
        #expect(notes.count == (newerEntry ? 0 : 2) && closed == 0)
        #expect(rig.recorded.settingsSent.filter { message in
            if case .settingsWrite(let write) = message { return write.key == key }
            return false
        }.count == 1)
    }

    @Test("a submitted ordinary text field restores at five seconds while newer or replacement drafts keep values and notes", arguments: [0, 1, 2])
    func timeoutRestoresOnlySubmittedTextEntry(owner: Int) async throws {
        let clock = TestLinkClock()
        let rig = try DisplaySheetTests.Rig(clock: clock)
        let proxy = rig.settings
        let key = "SliceSampleName"
        proxy.apply(.settingsSnapshot(.init(properties: [.init(name: key, value: .utf8("Core before"))])))
        let row = SetupRowOutcomeOwner()
        var entry = SetupTextEntryDraft()
        entry.modelChanged(proxy.value(key) ?? "")
        entry.type("Submitted")
        let submitted = entry.submit(owner: row)
        let entered = Task {
            let result = await proxy.write(key, "Submitted", onLateOutcome: { outcome in
                entry.receive(Self.textOutcome(outcome), edit: submitted, owner: row)
            })
            entry.receive(Self.textOutcome(result), edit: submitted, owner: row)
        }
        #expect(await Pages.settle { proxy.value(key) == "Submitted" })
        entry.modelChanged(proxy.value(key) ?? "")
        proxy.apply(.settingsValue(.init(key: key, origin: "another-phone", properties: [.init(name: key, value: .utf8("Latest Core"))])))
        if owner == 1 { entry.type("New unsent text") }
        if owner == 2 { entry = SetupTextEntryDraft(); entry.type("Replacement unsent text") }
        await clock.advance(by: 5_000)
        await entered.value
        entry.modelChanged(proxy.value(key) ?? "")
        let expected = owner == 0 ? "Latest Core" : owner == 1 ? "New unsent text" : "Replacement unsent text"
        #expect(proxy.value(key) == "Latest Core" && entry.value == expected)
        #expect(row.problem == (owner == 0 ? PropertyWriteOutcome.notConfirmed.reason : nil))
        proxy.apply(.settingsReject(.init(key: key, properties: [.init(name: key, value: .utf8("Latest Core"))], reason: "Current text refusal.")))
        entry.modelChanged(proxy.value(key) ?? "")
        #expect(entry.value == expected)
        #expect(row.problem == (owner == 0 ? "Current text refusal." : nil))
        #expect(rig.recorded.settingsSent.filter { if case .settingsWrite(let sent) = $0 { return sent.key == key }; return false }.count == 1)
    }

    @Test("the native described TextField restores its current submitted value and protects newer or replacement unsent text", .timeLimit(.minutes(1)), arguments: [0, 1, 2])
    func nativeTextFieldTimeoutOwnership(owner: Int) async throws {
        let rig = TextFieldRig()
        try await rig.connect()
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
        var identity = UUID()
        var row = SetupRowOutcomeOwner()
        let control = try rig.control()
        func root() -> AnyView {
            AnyView(DescribedControl(control: control, category: "general", dispatcher: rig.dispatcher, outcomeOwner: row).id(identity))
        }
        let host = UIHostingController(rootView: root())
        host.view.frame = window.bounds
        window.rootViewController = host
        window.makeKeyAndVisible()
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        var field = try #require(Self.nativeTextField(in: host.view), "the existing described TextField must be hosted")
        #expect(field.text == "Core before" && field.isEnabled)

        // Idle authoritative refreshes must continue to reach the real field.
        rig.core("Idle Core")
        await ShotWait.laidOut(window)
        #expect(field.text == "Idle Core")
        #expect(field.becomeFirstResponder())
        Self.typeNative("Submitted", on: field)
        await ShotWait.laidOut(window)
        #expect(field.text == "Submitted" && rig.sent.messages.isEmpty)
        // This is UIKit's actual return-key delegate, not a draft helper or
        // direct dispatcher.edit call: require its resulting wire write.
        let shouldReturn = try #require(field.delegate?.textFieldShouldReturn?(field), "the native return-key delegate")
        if shouldReturn { field.sendActions(for: .editingDidEndOnExit) }
        await rig.sent.waitForCount(1)
        let write = try #require(rig.sent.messages.compactMap { message -> LinkMessage.SettingsWrite? in
            if case .settingsWrite(let write) = message { return write }; return nil
        }.first)
        #expect(write.key == TextFieldRig.key && write.properties.first?.value == .utf8("Submitted"))
        rig.core("Latest Core")
        await ShotWait.laidOut(window)
        #expect(field.text == "Submitted", "the current submission is held before the deadline")
        if owner == 1 {
            Self.typeNative("New unsent text", on: field)
        } else if owner == 2 {
            identity = UUID()
            row = SetupRowOutcomeOwner()
            host.rootView = root()
            await ShotWait.laidOut(window)
            field = try #require(Self.nativeTextField(in: host.view))
            Self.typeNative("Replacement unsent text", on: field)
        }
        await ShotWait.laidOut(window)
        let expected = owner == 0 ? "Latest Core" : owner == 1 ? "New unsent text" : "Replacement unsent text"
        await rig.clock.advance(by: 4_999)
        await ShotWait.laidOut(window)
        #expect(field.text == (owner == 0 ? "Submitted" : expected))
        #expect(row.problem == nil)
        await rig.clock.advance(by: 1)
        #expect(rig.settings.value(TextFieldRig.key) == "Latest Core")
        await ShotWait.laidOut(window)
        #expect(field.text == expected, "the real TextField must protect a newer unsent edit")
        #expect(row.problem
                == (owner == 0 ? PropertyWriteOutcome.notConfirmed.reason : nil))
        rig.settings.apply(.settingsReject(.init(key: TextFieldRig.key,
                                                properties: [.init(name: TextFieldRig.key, value: .utf8("Late Core"))],
                                                reason: "Current text refusal.")))
        await ShotWait.laidOut(window)
        #expect(field.text == (owner == 0 ? "Late Core" : expected))
        #expect(row.problem
                == (owner == 0 ? "Current text refusal." : nil))
        rig.core("Following Core")
        await ShotWait.laidOut(window)
        #expect(field.text == (owner == 0 ? "Following Core" : expected))
        await rig.clock.advance(by: 20_000)
        await ShotWait.laidOut(window)
        #expect(rig.sent.messages.count == 1, "expired text must never replay")
    }

    @Test("snapshot and session retirement cannot deliver an old text result into a new native draft", .timeLimit(.minutes(1)), arguments: [false, true])
    func nativeTextFieldRetiredSubmission(newSession: Bool) async throws {
        let rig = TextFieldRig()
        try await rig.connect()
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
        let control = try rig.control()
        let row = SetupRowOutcomeOwner()
        let host = UIHostingController(rootView: DescribedControl(control: control, category: "general", dispatcher: rig.dispatcher, outcomeOwner: row))
        host.view.frame = window.bounds
        window.rootViewController = host
        window.makeKeyAndVisible()
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let field = try #require(Self.nativeTextField(in: host.view))
        #expect(field.becomeFirstResponder())
        Self.typeNative("Old submission", on: field)
        await ShotWait.laidOut(window)
        let shouldReturn = try #require(field.delegate?.textFieldShouldReturn?(field))
        if shouldReturn { field.sendActions(for: .editingDidEndOnExit) }
        await rig.sent.waitForCount(1)
        if newSession {
            rig.mirror.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            rig.settings.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            await rig.commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        }
        // Both reauthentication and a replacement snapshot retire the
        // actual pending write and advance the row's outcome identity.
        try await rig.connect(coreText: "New snapshot Core")
        await ShotWait.laidOut(window)
        #expect(rig.settings.value(TextFieldRig.key) == "New snapshot Core")
        #expect(rig.dispatcher.state(of: control, in: "general").value == .text("New snapshot Core"))
        // Draining queued work does not fence a later SwiftUI/native refresh.
        // Keep the original field so replacement cannot hide stale ownership.
        #expect(Self.nativeTextField(in: host.view) === field)
        let nativeReady = await ShotWait.until {
            window.layoutIfNeeded()
            return Self.nativeTextField(in: host.view) === field
                && field.window === window
                && field.text == "New snapshot Core"
        }
        #expect(nativeReady)
        #expect(field.text == "New snapshot Core")
        #expect(row.problem == nil)
        Self.typeNative("New session unsent text", on: field)
        rig.core("New session latest Core")
        await rig.clock.advance(by: 25_000)
        await ShotWait.laidOut(window)
        #expect(field.text == "New session unsent text")
        #expect(row.problem == nil)
        #expect(rig.sent.messages.count == 1, "old-session text must never replay")
    }

    @Test("a queued native text submission cannot acquire a replacement settings snapshot's permission", .timeLimit(.minutes(1)), arguments: [false, true])
    func nativeTextSubmissionKeepsGestureAdmission(linkLostFirst: Bool) async throws {
        let rig = TextFieldRig()
        try await rig.connect()
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
        let control = try rig.control()
        let host = UIHostingController(rootView: DescribedControl(control: control, category: "general", dispatcher: rig.dispatcher))
        host.view.frame = window.bounds
        window.rootViewController = host
        window.makeKeyAndVisible()
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let field = try #require(Self.nativeTextField(in: host.view))
        #expect(field.becomeFirstResponder())
        Self.typeNative("Old queued submission", on: field)
        await ShotWait.laidOut(window)
        let originalSettingsIdentity = try #require(rig.settings.currentSnapshotIdentity)
        let mirrorIdentity = rig.mirror.snapshotIdentity
        #expect(rig.sent.messages.isEmpty)
        let shouldReturn = try #require(field.delegate?.textFieldShouldReturn?(field))
        if shouldReturn { field.sendActions(for: .editingDidEndOnExit) }
        // No await between the actual Return action and replacement readiness:
        // its MainActor submission task cannot start in this interval. Keep
        // the mirror/description ready to isolate the settings admission
        // boundary, rather than obtaining a false pass from an offline gate.
        #expect(rig.sent.messages.isEmpty)
        if linkLostFirst {
            rig.settings.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            rig.settings.handle(.stateChanged(.receivingSnapshot))
        }
        rig.settings.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
        rig.settings.apply(.settingsSnapshot(.init(properties: [
            .init(name: TextFieldRig.key, value: .utf8("Replacement settings Core")),
        ])))
        let replacementSettingsIdentity = try #require(rig.settings.currentSnapshotIdentity)
        #expect(replacementSettingsIdentity != originalSettingsIdentity)
        #expect(rig.mirror.snapshotIdentity == mirrorIdentity && rig.mirror.isSnapshotComplete && !rig.mirror.isStale)
        #expect(rig.dispatcher.state(of: control, in: "general").editable)
        #expect(rig.dispatcher.state(of: control, in: "general").value == .text("Replacement settings Core"))
        await ShotWait.laidOut(window)
        await rig.clock.advance(by: 25_000)
        await ShotWait.laidOut(window)
        #expect(rig.sent.messages.isEmpty, "a retired gesture must not acquire fresh settings authority or replay")
        #expect(rig.settings.value(TextFieldRig.key) == "Replacement settings Core")
        #expect(Self.nativeTextField(in: host.view) === field && field.window === window)
        // A new gesture in the replacement snapshot must still work through
        // the same native field and Return-key delegate.
        Self.typeNative("Current submission", on: field)
        await ShotWait.laidOut(window)
        let currentReturn = try #require(field.delegate?.textFieldShouldReturn?(field))
        if currentReturn { field.sendActions(for: .editingDidEndOnExit) }
        #expect(await Pages.settle {
            rig.sent.messages.contains { message in
                if case .settingsWrite(let write) = message {
                    return write.key == TextFieldRig.key && write.properties.first?.value == .utf8("Current submission")
                }
                return false
            }
        })
        rig.core("Current submission")
        await rig.clock.advance(by: 25_000)
        await ShotWait.laidOut(window)
        #expect(rig.sent.messages.count == 1)
        #expect(field.text == "Current submission")
    }

    // Remaining current-use text coverage; additive to prepared-v1.

    @Test("each real staged CAT text row hands applied ownership to its staged source without sending", arguments: RemainingStagedTextRig.ids)
    func sourceStagedTextAppliedHandoff(id: String) async throws {
        let rig = RemainingStagedTextRig()
        try await rig.connect()
        let control = try rig.control(id)
        let row = SetupRowOutcomeOwner()
        var entry = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: "catNetwork").value?.text ?? "" })
        #expect(entry.displayedValue == "192.0.2.1")
        entry.type("192.0.2.2")
        let edit = entry.submit(owner: row)
        let admission: SetupAdmission
        switch rig.dispatcher.admit(control, in: "catNetwork") {
        case .success(let admitted): admission = admitted
        case .failure(let refusal): Issue.record("Unexpected refusal: \(refusal.reason)"); return
        }
        // Submission owns display before perform can establish its staged value.
        #expect(entry.displayedValue == "192.0.2.2")
        #expect(rig.dispatcher.state(of: control, in: "catNetwork").value == .text("192.0.2.1"))
        let outcome = await rig.dispatcher.perform(admission, value: .text("192.0.2.2"))
        #expect(outcome == .applied)
        entry.receive(outcome, edit: edit, owner: row)
        #expect(entry.displayedValue == "192.0.2.2" && row.problem == nil)
        let equalDisplayEdited = entry.type("192.0.2.2")
        #expect(!equalDisplayEdited, "equal staged display cannot become unsent input")
        guard case .property(let reference)? = control.binding else {
            Issue.record("the pinned CAT row must have a staged property binding"); return
        }
        #expect(rig.mirror.object(reference.object)?[reference.name] == .text("192.0.2.1"), "staging does not write the mirror")
        rig.mirror.apply(.delta(.init(key: reference.object, properties: [.init(name: reference.name, value: .utf8("192.0.2.3"))])))
        #expect(entry.displayedValue == "192.0.2.2", "the dispatcher keeps the staged value until its separate apply action")
        // A source reset distinguishes a handed-off draft from a stale local hold.
        rig.mirror.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(rig.dispatcher.state(of: control, in: "catNetwork").value == .text("192.0.2.3"))
        #expect(entry.displayedValue == "192.0.2.3" && row.problem == nil)
        await rig.clock.advance(by: 25_000)
        #expect(rig.sent.messages.isEmpty, "Return stages only; no setting, property or command send or replay")
    }

    @Test("settings text follows missing or disabled source while unsent input stays local", arguments: [false, true])
    func sourceSettingsTextUnavailablePresentation(missing: Bool) async throws {
        let rig = TextFieldRig()
        try await rig.connect(coreText: "Cached Core")
        let control = try rig.control()
        var following = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: "general").value?.text ?? "" })
        var unsent = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: "general").value?.text ?? "" })
        #expect(following.displayedValue == "Cached Core")
        unsent.type("Unsent")
        if missing {
            rig.settings.apply(.settingsValue(.init(key: TextFieldRig.key, origin: "another-phone", properties: [])))
        } else {
            rig.mirror.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            rig.settings.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            await rig.commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        }
        let state = rig.dispatcher.state(of: control, in: "general")
        let reason = missing ? SetupControlDispatcher.valueMissingReason : SetupControlDispatcher.notConnectedReason
        #expect(state.value == (missing ? nil : .text("Cached Core")))
        #expect(!state.editable && state.reason == reason)
        #expect(following.displayedValue == (missing ? "" : "Cached Core"))
        #expect(unsent.displayedValue == "Unsent")
        switch rig.dispatcher.admit(control, in: "general") {
        case .failure(let refusal): #expect(refusal.reason == reason)
        case .success: Issue.record("an unavailable text source must not admit a Return")
        }
        following.retirePresentation()
        unsent.retirePresentation()
        #expect(following.displayedValue == (missing ? "" : "Cached Core"))
        #expect(unsent.displayedValue == "Unsent")
        await rig.clock.advance(by: 25_000)
        #expect(rig.sent.messages.isEmpty)
    }

    @Test("command text follows missing or disabled valueProperty while unsent input stays local", arguments: [false, true])
    func sourceCommandTextUnavailablePresentation(missing: Bool) async throws {
        let rig = OwnershipCommandTextRig()
        try await rig.connect(coreText: "Cached Core")
        let control = try rig.control()
        var following = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: OwnershipCommandTextRig.category).value?.text ?? "" })
        var unsent = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: OwnershipCommandTextRig.category).value?.text ?? "" })
        #expect(following.displayedValue == "Cached Core")
        unsent.type("Unsent")
        if missing {
            rig.mirror.apply(.objectDestroy(.init(key: OwnershipCommandTextRig.object, className: "OperatorText")))
        } else {
            rig.mirror.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            rig.settings.handle(.stateChanged(.waitingToRetry(seconds: 1)))
            await rig.commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        }
        let state = rig.dispatcher.state(of: control, in: OwnershipCommandTextRig.category)
        let reason = missing ? SetupControlDispatcher.valueMissingReason : SetupControlDispatcher.notConnectedReason
        #expect(state.value == (missing ? nil : .text("Cached Core")))
        #expect(!state.editable && state.reason == reason)
        #expect(following.displayedValue == (missing ? "" : "Cached Core"))
        #expect(unsent.displayedValue == "Unsent")
        switch rig.dispatcher.admit(control, in: OwnershipCommandTextRig.category) {
        case .failure(let refusal): #expect(refusal.reason == reason)
        case .success: Issue.record("an unavailable command text source must not admit a Return")
        }
        following.retirePresentation()
        unsent.retirePresentation()
        #expect(following.displayedValue == (missing ? "" : "Cached Core"))
        #expect(unsent.displayedValue == "Unsent")
        await rig.clock.advance(by: 25_000)
        #expect(rig.sent.messages.isEmpty)
    }

    @MainActor
    private final class RemainingStagedTextRig {
        // These six controls are exact JSON objects from the pinned CAT
        // resource. Only unrelated controls/pages are omitted from the feed.
        nonisolated static let ids = ["catNetwork.powerGenius.address", "catNetwork.powerGenius.netmask", "catNetwork.powerGenius.gateway",
                          "catNetwork.tunerGenius.address", "catNetwork.tunerGenius.netmask", "catNetwork.tunerGenius.gateway"]
        static let fields = ["pgxlAddress", "pgxlNetmask", "pgxlGateway", "tgxlAddress", "tgxlNetmask", "tgxlGateway"]
        static let description = #"{"version":21,"category":{"id":"catNetwork","title":"CAT & Network","where":"mixed","coverage":"partial"},"pages":[{"id":"catNetwork.powerGenius","title":"PowerGenius XL","where":"station","coverage":"partial: Bias Mode, TX Antenna, Follows slice and the fault history table are not described","sections":[{"title":"Network","controls":[{"id":"catNetwork.powerGenius.address","label":"IP Address:","tooltip":"","kind":"text","binding":{"property":{"object":"accessorySettings","name":"pgxlAddress"}},"applies":"staged","requiresDescriptionVersion":15,"gate":{"capability":"remotePgxlControlVersion","min":3}},{"id":"catNetwork.powerGenius.netmask","label":"Netmask:","tooltip":"","kind":"text","binding":{"property":{"object":"accessorySettings","name":"pgxlNetmask"}},"applies":"staged","requiresDescriptionVersion":15,"gate":{"capability":"remotePgxlControlVersion","min":3}},{"id":"catNetwork.powerGenius.gateway","label":"Gateway:","tooltip":"","kind":"text","binding":{"property":{"object":"accessorySettings","name":"pgxlGateway"}},"applies":"staged","requiresDescriptionVersion":15,"gate":{"capability":"remotePgxlControlVersion","min":3}}]}]},{"id":"catNetwork.tunerGenius","title":"Tuner Genius XL","where":"station","coverage":"partial: the tune memory table and the fault history are not described","sections":[{"title":"Network","controls":[{"id":"catNetwork.tunerGenius.address","label":"IP Address:","tooltip":"","kind":"text","binding":{"property":{"object":"accessorySettings","name":"tgxlAddress"}},"applies":"staged","requiresDescriptionVersion":15,"gate":{"capability":"remoteTgxlControlVersion","min":1}},{"id":"catNetwork.tunerGenius.netmask","label":"Netmask:","tooltip":"","kind":"text","binding":{"property":{"object":"accessorySettings","name":"tgxlNetmask"}},"applies":"staged","requiresDescriptionVersion":15,"gate":{"capability":"remoteTgxlControlVersion","min":1}},{"id":"catNetwork.tunerGenius.gateway","label":"Gateway:","tooltip":"","kind":"text","binding":{"property":{"object":"accessorySettings","name":"tgxlGateway"}},"applies":"staged","requiresDescriptionVersion":15,"gate":{"capability":"remoteTgxlControlVersion","min":1}}]}]}]}"#
        let clock = TestLinkClock()
        let sent = TextFieldRig.Sent()
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher
        let selection = CurrentValueSubject<Int?, Never>(0)

        init() {
            let sent = sent
            let clock = clock
            let sender: MirrorStore.BoundSender = { message, permit in
                guard permit.handoff({ sent.record(message); return true }) else { throw LinkSendError.notConnected }
            }
            mirror = MirrorStore(send: { sent.record($0) }, clock: clock)
            settings = SettingsProxyClient(send: { sent.record($0) }, captureSender: { sender }, clock: clock)
            commands = CommandClient(clock: clock, send: { sent.record($0) }, captureSender: { sender })
            feed = SetupDescriptionFeed(store: mirror)
            let selection = selection
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                                                captureSender: { sender }, selectedSlice: { selection.value },
                                                selectionChanges: selection.eraseToAnyPublisher(), phone: nil)
        }

        func connect() async throws {
            mirror.handle(.stateChanged(.receivingSnapshot))
            settings.handle(.stateChanged(.receivingSnapshot))
            await commands.handle(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                .hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")),
                .authResult(.init(accepted: true, reason: "", retryable: false)),
                .capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(21)),
                                                 .init(name: "remotePgxlControlVersion", value: .i64(3)),
                                                 .init(name: "remoteTgxlControlVersion", value: .i64(3))])),
                .schema(.init(className: "SetupDescription", fields: [.init(ordinal: 0, name: "revision", kind: .i64),
                                                                    .init(ordinal: 1, name: "catNetwork", kind: .utf8)])),
                .schema(.init(className: "AccessorySettings", fields: Self.fields.enumerated().map {
                    .init(ordinal: UInt16($0.offset), name: $0.element, kind: .utf8)
                })),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "catNetwork", value: .utf8(Self.description)),
                ])),
                .objectCreate(.init(key: "accessorySettings", className: "AccessorySettings", properties: Self.fields.enumerated().map {
                    .init(ordinal: UInt16($0.offset), name: $0.element, value: .utf8("192.0.2.1"))
                })),
                .settingsSnapshot(.init(properties: [])),
                .snapshotComplete,
            ]
            for message in messages {
                mirror.apply(message)
                settings.apply(message)
                await commands.receive(message)
            }
            mirror.handle(.stateChanged(.ready))
            settings.handle(.stateChanged(.ready))
            await commands.handle(.stateChanged(.ready))
            await MainQueue.drained()
            try #require(feed.description(for: "catNetwork"), "the real staged text metadata must be current")
            for id in Self.ids {
                let control = try control(id)
                #expect(control.kind == .text && control.applies == .staged && control.metadataIssue == nil)
                #expect(dispatcher.state(of: control, in: "catNetwork").editable)
            }
        }

        func control(_ id: String) throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "catNetwork"))
            return try #require(description.pages.flatMap { $0.sections }.flatMap { $0.controls }.first { $0.id == id })
        }
    }

    // Proposed ownership coverage. This block is additive; original tests
    // and their native field/deadline assertions remain unchanged.

    @Test("a source-bearing text draft follows the dispatcher and owns a historical value typed again")
    func sourceTextHistoricalInputOwnership() async throws {
        let rig = TextFieldRig()
        try await rig.connect(coreText: "A")
        let control = try rig.control()
        var entry = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: "general").value?.text ?? "" })
        #expect(entry.displayedValue == "A")
        rig.core("B")
        #expect(entry.displayedValue == "B")
        let equalDisplayWasEdited = entry.type("B")
        #expect(!equalDisplayWasEdited, "an equal-display publication is not a local edit")
        rig.core("C")
        #expect(entry.displayedValue == "C")
        let historicalValueWasEdited = entry.type("A")
        #expect(historicalValueWasEdited, "historical stored A differs from the currently displayed C")
        rig.core("D")
        #expect(entry.displayedValue == "A", "a real unsent revision protects historical A")
    }

    @Test("source-bearing submission covers pre-perform and restores unchanged Core at exactly five seconds")
    func sourceTextPrePerformAndEqualCoreRollback() async throws {
        let rig = TextFieldRig()
        try await rig.connect(coreText: "A")
        let control = try rig.control()
        let row = SetupRowOutcomeOwner()
        var entry = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: "general").value?.text ?? "" })
        entry.type("Submitted")
        let edit = entry.submit(owner: row)
        let admission: SetupAdmission
        switch rig.dispatcher.admit(control, in: "general") {
        case .success(let admitted): admission = admitted
        case .failure(let refusal): Issue.record("Unexpected refusal: \(refusal.reason)"); return
        }
        let task = Task {
            let outcome = await rig.dispatcher.perform(admission, value: .text("Submitted"), onLateOutcome: {
                entry.receive($0, edit: edit, owner: row)
            })
            entry.receive(outcome, edit: edit, owner: row)
            return outcome
        }
        // No await: the MainActor perform task has not established a proxy hold.
        #expect(rig.sent.messages.isEmpty)
        #expect(rig.dispatcher.state(of: control, in: "general").value == .text("A"))
        #expect(entry.displayedValue == "Submitted")
        try #require(await Pages.settle { rig.sent.messages.count == 1 })
        #expect(rig.clock.pendingDueTimes.contains(5_000))
        await rig.clock.advance(by: 4_999)
        #expect(entry.displayedValue == "Submitted" && row.problem == nil)
        await rig.clock.advance(by: 1)
        #expect(await task.value == .notSent(PropertyWriteOutcome.notConfirmed.reason))
        #expect(entry.displayedValue == "A", "no changed Core string is needed to release the local hold")
        #expect(row.problem == PropertyWriteOutcome.notConfirmed.reason)
        let write = try #require(rig.sent.messages.compactMap { message -> LinkMessage.SettingsWrite? in
            if case .settingsWrite(let value) = message { return value }; return nil
        }.first)
        rig.settings.apply(.settingsReject(.init(key: write.key, properties: [.init(name: write.key, value: .utf8("Late Core"))],
                                                reason: "Current late refusal.")))
        #expect(entry.displayedValue == "Late Core" && row.problem == "Current late refusal.")
        entry.type("Unsent")
        rig.core("Following Core")
        #expect(entry.displayedValue == "Unsent")
        await rig.clock.advance(by: 20_000)
        #expect(rig.sent.messages.count == 1)
    }

    @Test("old binding getters use the replacement source while old setters cannot adopt it")
    func sourceTextRetiredBindingReadsReplacementSource() async throws {
        let oldRig = TextFieldRig()
        try await oldRig.connect(coreText: "Old setting")
        let oldControl = try oldRig.control()
        let row = SetupRowOutcomeOwner()
        var entry = SetupTextEntryDraft(readCurrent: { oldRig.dispatcher.state(of: oldControl, in: "general").value?.text ?? "" })
        entry.type("Old submission")
        let oldEdit = entry.submit(owner: row)
        let oldBinding = Binding<String>(get: { entry.displayedValue }, set: { entry.type($0, identity: oldEdit.identity) })
        let replacement = OwnershipCommandTextRig()
        try await replacement.connect(coreText: "New command source")
        let currentControl = try replacement.control()
        row.retire()
        entry = SetupTextEntryDraft(readCurrent: { replacement.dispatcher.state(of: currentControl, in: OwnershipCommandTextRig.category).value?.text ?? "" })
        #expect(oldBinding.wrappedValue == "New command source")
        replacement.core("Following replacement")
        #expect(oldBinding.wrappedValue == "Following replacement", "the old getter must not read oldControl")
        oldBinding.wrappedValue = "Old setting"
        #expect(entry.displayedValue == "Following replacement")
        entry.type("New unsent")
        oldBinding.wrappedValue = "Old publication"
        entry.receive(.refused("Old refusal."), edit: oldEdit, owner: row)
        #expect(oldBinding.wrappedValue == "New unsent" && row.problem == nil)
    }

    @Test("Yes retains the asked text but cannot adopt a changed source or newer unsent draft", arguments: [false, true])
    func sourceTextAskedValueCannotAdoptNewDisplay(newerUnsent: Bool) async throws {
        let rig = OwnershipCommandTextRig(asks: true)
        try await rig.connect(coreText: "Asked A")
        let control = try rig.control()
        let row = SetupRowOutcomeOwner()
        var entry = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: OwnershipCommandTextRig.category).value?.text ?? "" })
        let askedValue = SetupValue.text(entry.displayedValue)
        let asked: SetupQuestion
        switch rig.dispatcher.question(for: control, in: OwnershipCommandTextRig.category, value: askedValue) {
        case .success(let question)?: asked = question
        case .failure(let refusal)?: Issue.record("Unexpected refusal: \(refusal.reason)"); return
        case nil: Issue.record("the synthetic confirmed text row must ask"); return
        }
        #expect(asked.text == "Send Asked A?")
        rig.core("Core B")
        if newerUnsent { entry.type("New unsent") }
        let edit = entry.submit("Asked A", owner: row)
        #expect(edit == nil, "commit-time displayed baseline cannot grant asked A a new owner")
        let expected = newerUnsent ? "New unsent" : "Core B"
        #expect(entry.displayedValue == expected)
        // This is the existing Yes dispatcher path, with the captured question/value.
        let task = Task { await rig.dispatcher.edit(control, in: OwnershipCommandTextRig.category, to: askedValue, asked: asked) }
        try #require(await Pages.settle { rig.invocations.count == 1 })
        let invoke = try #require(rig.invocations.first)
        #expect(invoke.args == [.init(name: "name", value: .utf8("Asked A"))])
        await rig.answer(invoke, accepted: false, reason: "Asked A refused.")
        #expect(await task.value == .refused("Asked A refused."))
        #expect(entry.displayedValue == expected && row.problem == nil)
    }

    @Test("retiring a retained source-bearing draft releases submitted presentation and keeps unsent input", arguments: [false, true])
    func sourceTextRetainedDraftRetirement(newerUnsent: Bool) async throws {
        let rig = OwnershipCommandTextRig()
        try await rig.connect(coreText: "A")
        let control = try rig.control()
        let row = SetupRowOutcomeOwner()
        var entry = SetupTextEntryDraft(readCurrent: { rig.dispatcher.state(of: control, in: OwnershipCommandTextRig.category).value?.text ?? "" })
        entry.type("Submitted")
        let edit = entry.submit(owner: row)
        if newerUnsent { entry.type("Unsent") }
        row.retire()
        entry.retirePresentation()
        rig.core("B")
        entry.receive(.refused("Retired refusal."), edit: edit, owner: row)
        #expect(entry.displayedValue == (newerUnsent ? "Unsent" : "B") && row.problem == nil)
        let fresh = entry.submit(owner: row)
        #expect(row.owns(fresh.rowEdit))
    }

    @Test("a native command text submission holds through Core changes and hands back on its real answer",
          .timeLimit(.minutes(1)), arguments: [0, 1, 2, 3])
    func nativeCommandTextReturnedOutcomePresentation(answer: Int) async throws {
        let rig = OwnershipCommandTextRig()
        try await rig.connect()
        let row = SetupRowOutcomeOwner()
        let screen = try OwnershipTextScreen(control: rig.control(), dispatcher: rig.dispatcher, row: row,
                                             category: OwnershipCommandTextRig.category)
        defer { screen.close() }
        await ShotWait.laidOut(screen.window)
        let field = try screen.field()
        #expect(field.text == "Core before" && field.isEnabled)
        #expect(field.becomeFirstResponder())
        Self.typeNative("Submitted", on: field)
        await ShotWait.laidOut(screen.window)
        try Self.returnOwnershipText(field)
        #expect(rig.invocations.isEmpty && field.text == "Submitted")
        try #require(await Pages.settle { rig.invocations.count == 1 })
        let invoke = try #require(rig.invocations.first)
        #expect(invoke.args == [.init(name: "name", value: .utf8("Submitted"))])
        // Case 3 acks without a source delta: the current valueProperty is
        // still old Core. Returning an ack alone must not invent a Core value.
        if answer != 3 { rig.core("Latest Core") }
        let currentCore = answer == 3 ? "Core before" : "Latest Core"
        await ShotWait.laidOut(screen.window)
        #expect(rig.dispatcher.state(of: try rig.control(), in: OwnershipCommandTextRig.category).value == .text(currentCore))
        #expect(field.text == "Submitted", "command text has no model hold; the local pending phase owns this")
        let reason = answer == 1 ? "Core command refusal." : answer == 2 ? SeveralDevices.waitingReason : ""
        await rig.answer(invoke, accepted: answer == 0 || answer == 3, reason: reason)
        try #require(await Pages.settle { rig.dispatcher.openAdmissionCount == 0 })
        await ShotWait.laidOut(screen.window)
        #expect(try screen.field() === field && field.window === screen.window)
        // Proposed policy: all returned command outcomes hand display to valueProperty.
        #expect(field.text == currentCore, "a command ack hands back even when valueProperty is still old Core")
        #expect(row.problem == (answer == 1 ? "Core command refusal." : nil))
        rig.core("Following Core")
        await ShotWait.laidOut(screen.window)
        #expect(field.text == "Following Core")
        await rig.answer(invoke, accepted: false, reason: "Duplicate late answer.")
        await ShotWait.laidOut(screen.window)
        #expect(field.text == "Following Core" && row.problem == (answer == 1 ? "Core command refusal." : nil))
        #expect(rig.invocations.count == 1)
    }

    @Test("native command text snaps back at 4999 plus 1 ms and ignores its actual late answer",
          .timeLimit(.minutes(1)), arguments: [false, true])
    func nativeCommandTextDeadlineAndLateAnswer(newerUnsent: Bool) async throws {
        let rig = OwnershipCommandTextRig()
        try await rig.connect()
        let row = SetupRowOutcomeOwner()
        let screen = try OwnershipTextScreen(control: rig.control(), dispatcher: rig.dispatcher, row: row,
                                             category: OwnershipCommandTextRig.category)
        defer { screen.close() }
        await ShotWait.laidOut(screen.window)
        let field = try screen.field()
        #expect(field.becomeFirstResponder())
        Self.typeNative("Submitted", on: field)
        await ShotWait.laidOut(screen.window)
        try Self.returnOwnershipText(field)
        try #require(await Pages.settle { rig.invocations.count == 1 })
        let invoke = try #require(rig.invocations.first)
        #expect(rig.clock.pendingDueTimes.contains(5_000))
        // Core still equals its pre-submission value: no intervening source delta.
        #expect(rig.dispatcher.state(of: try rig.control(), in: OwnershipCommandTextRig.category).value == .text("Core before"))
        if newerUnsent { Self.typeNative("Unsent", on: field) }
        await ShotWait.laidOut(screen.window)
        await rig.clock.advance(by: 4_999)
        await ShotWait.laidOut(screen.window)
        #expect(field.text == (newerUnsent ? "Unsent" : "Submitted") && row.problem == nil)
        #expect(await rig.commands.waitingCount == 1)
        await rig.clock.advance(by: 1)
        try #require(await Pages.settle { rig.dispatcher.openAdmissionCount == 0 })
        #expect(await rig.commands.waitingCount == 0)
        await ShotWait.laidOut(screen.window)
        #expect(field.text == (newerUnsent ? "Unsent" : "Core before"))
        #expect(row.problem == (newerUnsent ? nil : PropertyWriteOutcome.notConfirmed.reason))
        await rig.answer(invoke, accepted: false, reason: "Late command refusal.")
        rig.core("Following Core")
        await ShotWait.laidOut(screen.window)
        #expect(field.text == (newerUnsent ? "Unsent" : "Following Core"))
        #expect(row.problem == (newerUnsent ? nil : PropertyWriteOutcome.notConfirmed.reason))
        await rig.clock.advance(by: 20_000)
        #expect(rig.invocations.count == 1)
        #expect(try screen.field() === field)
    }

    @Test("two native Returns of one revision give only the newest command result presentation ownership", .timeLimit(.minutes(1)))
    func nativeCommandTextSameRevisionResultOwnership() async throws {
        let rig = OwnershipCommandTextRig()
        try await rig.connect()
        let row = SetupRowOutcomeOwner()
        let screen = try OwnershipTextScreen(control: rig.control(), dispatcher: rig.dispatcher, row: row,
                                             category: OwnershipCommandTextRig.category)
        defer { screen.close() }
        await ShotWait.laidOut(screen.window)
        let field = try screen.field()
        #expect(field.becomeFirstResponder())
        Self.typeNative("Submitted", on: field)
        await ShotWait.laidOut(screen.window)
        try Self.returnOwnershipText(field)
        try #require(await Pages.settle { rig.invocations.count == 1 })
        #expect(field.becomeFirstResponder())
        try Self.returnOwnershipText(field) // No input between Returns: same revision.
        try #require(await Pages.settle { rig.invocations.count == 2 })
        let first = rig.invocations[0]
        let second = rig.invocations[1]
        #expect(first.id != second.id && first.args == second.args)
        rig.core("Current Core")
        await rig.answer(first, accepted: false, reason: "Old same-revision refusal.")
        try #require(await Pages.settle { rig.dispatcher.openAdmissionCount == 1 })
        await ShotWait.laidOut(screen.window)
        #expect(field.text == "Submitted" && row.problem == nil)
        await rig.answer(second, accepted: true)
        try #require(await Pages.settle { rig.dispatcher.openAdmissionCount == 0 })
        await ShotWait.laidOut(screen.window)
        #expect(field.text == "Current Core" && row.problem == nil)
        #expect(try screen.field() === field)
        await rig.clock.advance(by: 25_000)
        #expect(rig.invocations.count == 2)
    }

    @Test("a native settings question restores submitted Core text and preserves newer unsent text",
          .timeLimit(.minutes(1)), arguments: [false, true])
    func nativeSettingsTextAwaitingConfirmationHandoff(newerUnsent: Bool) async throws {
        let rig = TextFieldRig()
        try await rig.connect()
        let row = SetupRowOutcomeOwner()
        let screen = try OwnershipTextScreen(control: rig.control(), dispatcher: rig.dispatcher, row: row)
        defer { screen.close() }
        await ShotWait.laidOut(screen.window)
        let field = try screen.field()
        #expect(field.becomeFirstResponder())
        Self.typeNative("Submitted", on: field)
        await ShotWait.laidOut(screen.window)
        try Self.returnOwnershipText(field)
        try #require(await Pages.settle { rig.sent.messages.count == 1 })
        if newerUnsent {
            Self.typeNative("New unsent", on: field)
            await ShotWait.laidOut(screen.window)
        }
        rig.settings.apply(.settingsReject(.init(key: TextFieldRig.key, properties: [
            .init(name: TextFieldRig.key, value: .utf8("Core before")),
        ], reason: SeveralDevices.waitingReason)))
        try #require(await Pages.settle { rig.dispatcher.openAdmissionCount == 0 })
        await ShotWait.laidOut(screen.window)
        #expect(rig.settings.value(TextFieldRig.key) == "Core before")
        #expect(field.text == (newerUnsent ? "New unsent" : "Core before") && row.problem == nil)
        rig.core("Question Core")
        await ShotWait.laidOut(screen.window)
        #expect(rig.settings.value(TextFieldRig.key) == "Question Core")
        #expect(field.text == (newerUnsent ? "New unsent" : "Question Core"),
                "submitted text follows the Core while a newer unsent edit keeps its own display")
        #expect(try screen.field() === field && row.problem == nil)
        await rig.clock.advance(by: 25_000)
        #expect(rig.sent.messages.count == 1)
    }

    @Test("a retained native field releases retired pending presentation across disappearance and keeps unsent text",
          .timeLimit(.minutes(1)), arguments: [false, true])
    func nativeTextRetainedStateDisappearance(newerUnsent: Bool) async throws {
        let rig = TextFieldRig()
        try await rig.connect()
        let row = SetupRowOutcomeOwner()
        let screen = try OwnershipTextScreen(control: rig.control(), dispatcher: rig.dispatcher, row: row,
                                             retainsHostInNavigation: true)
        defer { screen.close() }
        await ShotWait.laidOut(screen.window)
        let field = try screen.field()
        #expect(field.becomeFirstResponder())
        Self.typeNative("Submitted", on: field)
        await ShotWait.laidOut(screen.window)
        try Self.returnOwnershipText(field)
        try #require(await Pages.settle { rig.sent.messages.count == 1 })
        #expect(row.owns(1), "this fresh row has exactly one submission")
        if newerUnsent { Self.typeNative("Retained unsent", on: field) }
        await ShotWait.laidOut(screen.window)
        // A navigation push actually removes the retained host from display.
        // The same host/root/State/window/field stays alive beneath the cover.
        let navigation = try #require(screen.navigation)
        let cover = UIViewController()
        navigation.pushViewController(cover, animated: false)
        try #require(await Pages.settle { !row.owns(1) }, "the real onDisappear must retire the existing owner")
        rig.core("Hidden Core")
        await rig.clock.advance(by: 5_000)
        try #require(await Pages.settle { rig.dispatcher.openAdmissionCount == 0 })
        let popped = navigation.popViewController(animated: false)
        #expect(popped === cover && navigation.topViewController === screen.host)
        await ShotWait.laidOut(screen.window)
        #expect(try screen.field() === field && field.window === screen.window)
        #expect(field.text == (newerUnsent ? "Retained unsent" : "Hidden Core"))
        #expect(row.problem == nil, "the disappeared owner's outcome cannot become a new note")
        rig.core("Following Core")
        await ShotWait.laidOut(screen.window)
        #expect(field.text == (newerUnsent ? "Retained unsent" : "Following Core"))
        await rig.clock.advance(by: 20_000)
        #expect(rig.sent.messages.count == 1)
    }

    private static func returnOwnershipText(_ field: UITextField) throws {
        let shouldReturn = try #require(field.delegate?.textFieldShouldReturn?(field), "the actual native Return delegate")
        if shouldReturn { field.sendActions(for: .editingDidEndOnExit) }
    }

    @MainActor
    private final class OwnershipTextScreen {
        let window: UIWindow
        let host: UIHostingController<AnyView>
        let navigation: UINavigationController?

        init(control: SetupDescription.Control, dispatcher: SetupControlDispatcher, row: SetupRowOutcomeOwner,
             category: String = "general", retainsHostInNavigation: Bool = false) throws {
            let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
            let window = UIWindow(windowScene: scene)
            let host = UIHostingController(rootView: AnyView(DescribedControl(control: control, category: category,
                                                                            dispatcher: dispatcher, outcomeOwner: row)))
            self.window = window
            self.host = host
            let navigation = retainsHostInNavigation ? UINavigationController(rootViewController: host) : nil
            navigation?.setNavigationBarHidden(true, animated: false)
            self.navigation = navigation
            window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
            host.view.frame = window.bounds
            if let navigation {
                window.rootViewController = navigation
            } else {
                window.rootViewController = host
            }
            window.makeKeyAndVisible()
        }

        func field() throws -> UITextField {
            try #require(SetupTypedEntryTests.nativeTextField(in: host.view), "the existing native text field")
        }

        func close() {
            window.isHidden = true
            window.rootViewController = nil
        }
    }

    /// Synthetic Core description and messages through the actual dispatcher
    /// and command client, using the existing app clock and sent-message fixture.
    @MainActor
    private final class OwnershipCommandTextRig {
        static let category = "catNetwork"
        static let object = "operatorText"
        static let verb = "setOperatorText"
        static func description(asks: Bool) -> String {
            let question = asks ? #","confirm":"Send %1?","confirmWhen":"Asked A""# : ""
            return #"{"version":15,"category":{"id":"catNetwork","title":"CAT & Network","where":"mixed"},"pages":[{"id":"catNetwork.main","title":"CAT & Network","where":"mixed","sections":[{"title":"Text","controls":[{"id":"catNetwork.main.commandName","label":"Name","tooltip":"","kind":"text","applies":"live","requiresDescriptionVersion":15,"binding":{"command":{"verb":"setOperatorText","valueProperty":{"object":"operatorText","name":"name"},"arguments":{"name":{"$controlValue":true}}}}\#(question)}]}]}]}"#
        }

        let clock = TestLinkClock()
        let sent = TextFieldRig.Sent()
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher
        let selection = CurrentValueSubject<Int?, Never>(0)
        let asks: Bool

        var invocations: [LinkMessage.CommandInvoke] {
            sent.messages.compactMap { message in
                if case .commandInvoke(let invoke) = message { return invoke }; return nil
            }
        }

        init(asks: Bool = false) {
            self.asks = asks
            let sent = sent
            let clock = clock
            let sender: MirrorStore.BoundSender = { message, permit in
                guard permit.handoff({ sent.record(message); return true }) else { throw LinkSendError.notConnected }
            }
            mirror = MirrorStore(send: { sent.record($0) }, clock: clock)
            settings = SettingsProxyClient(send: { sent.record($0) }, captureSender: { sender }, clock: clock)
            commands = CommandClient(clock: clock, send: { sent.record($0) }, captureSender: { sender })
            feed = SetupDescriptionFeed(store: mirror)
            let selection = selection
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                                                captureSender: { sender }, selectedSlice: { selection.value },
                                                selectionChanges: selection.eraseToAnyPublisher(), phone: nil)
        }

        func connect(coreText: String = "Core before") async throws {
            mirror.handle(.stateChanged(.receivingSnapshot))
            settings.handle(.stateChanged(.receivingSnapshot))
            await commands.handle(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                .hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")),
                .authResult(.init(accepted: true, reason: "", retryable: false)),
                .capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(15))])),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: Self.category, kind: .utf8),
                ])),
                .schema(.init(className: "OperatorText", fields: [.init(ordinal: 0, name: "name", kind: .utf8)])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: Self.category, value: .utf8(Self.description(asks: asks))),
                ])),
                .objectCreate(.init(key: Self.object, className: "OperatorText",
                                    properties: [.init(ordinal: 0, name: "name", value: .utf8(coreText))])),
                .objectCreate(.init(key: "slice:0", className: "SliceModel", properties: [.init(name: "sliceIndex", value: .i64(0))])),
                .settingsSnapshot(.init(properties: [.init(name: TextFieldRig.key, value: .utf8(coreText))])),
                .snapshotComplete,
            ]
            for message in messages {
                mirror.apply(message)
                settings.apply(message)
                await commands.receive(message)
            }
            mirror.handle(.stateChanged(.ready))
            settings.handle(.stateChanged(.ready))
            await commands.handle(.stateChanged(.ready))
            await MainQueue.drained()
            try #require(feed.description(for: Self.category), "the command text description must be published")
            let control = try control()
            #expect(control.kind == .text && control.metadataIssue == nil)
            #expect(dispatcher.state(of: control, in: Self.category).editable)
        }

        func control() throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: Self.category))
            return try #require(description.pages.first?.sections.first?.controls.first)
        }

        func core(_ text: String) {
            mirror.apply(.delta(.init(key: Self.object, properties: [.init(name: "name", value: .utf8(text))])))
        }

        func answer(_ invoke: LinkMessage.CommandInvoke, accepted: Bool, reason: String = "") async {
            await commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                        reason: reason, affected: [Self.object])))
        }
    }

    private static func nativeTextField(in view: UIView) -> UITextField? {
        if let field = view as? UITextField { return field }
        for child in view.subviews {
            if let field = nativeTextField(in: child) { return field }
        }
        return nil
    }

    private static func typeNative(_ text: String, on field: UITextField) {
        field.text = text
        field.sendActions(for: .editingChanged)
    }

    @MainActor
    private final class TextFieldRig {
        static let key = "SliceSampleName"
        // A synthetic, valid Core description of an ordinary settings text
        // row; the row is rendered and submitted by existing production UI.
        static let description = #"{"version":3,"category":{"id":"general","title":"General","where":"mixed"},"pages":[{"id":"general.main","title":"General","where":"mixed","sections":[{"title":"Text","controls":[{"id":"general.main.name","label":"Name","tooltip":"","kind":"text","binding":{"setting":"SliceSampleName"},"applies":"live"}]}]}]}"#
        final class Sent: @unchecked Sendable {
            private let lock = NSLock()
            private var stored: [LinkMessage] = []
            var messages: [LinkMessage] { lock.withLock { stored } }
            private var waiters: [(Int, CheckedContinuation<Void, Never>)] = []
            func record(_ message: LinkMessage) {
                let ready = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
                    stored.append(message)
                    let ready = waiters.filter { $0.0 <= stored.count }.map(\.1)
                    waiters.removeAll { $0.0 <= stored.count }
                    return ready
                }
                for waiter in ready { waiter.resume() }
            }
            func waitForCount(_ count: Int) async {
                await withCheckedContinuation { continuation in
                    let ready = lock.withLock { () -> Bool in
                        if stored.count >= count { return true }
                        waiters.append((count, continuation))
                        return false
                    }
                    if ready { continuation.resume() }
                }
            }
        }
        let clock = TestLinkClock()
        let sent = Sent()
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher
        let selection = CurrentValueSubject<Int?, Never>(0)

        init() {
            let sent = sent
            let clock = clock
            let sender: MirrorStore.BoundSender = { message, permit in
                guard permit.handoff({ sent.record(message); return true }) else { throw LinkSendError.notConnected }
            }
            mirror = MirrorStore(send: { sent.record($0) }, clock: clock)
            settings = SettingsProxyClient(send: { sent.record($0) }, captureSender: { sender }, clock: clock)
            commands = CommandClient(clock: clock, send: { sent.record($0) }, captureSender: { sender })
            feed = SetupDescriptionFeed(store: mirror)
            let selection = selection
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                                                captureSender: { sender }, selectedSlice: { selection.value },
                                                selectionChanges: selection.eraseToAnyPublisher(), phone: nil)
        }

        func connect(coreText: String = "Core before") async throws {
            mirror.handle(.stateChanged(.receivingSnapshot))
            settings.handle(.stateChanged(.receivingSnapshot))
            await commands.handle(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                .hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")),
                .authResult(.init(accepted: true, reason: "", retryable: false)),
                .capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(10))])),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "general", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "general", value: .utf8(Self.description)),
                ])),
                .objectCreate(.init(key: "slice:0", className: "SliceModel", properties: [.init(name: "sliceIndex", value: .i64(0))])),
                .settingsSnapshot(.init(properties: [.init(name: Self.key, value: .utf8(coreText))])),
                .snapshotComplete,
            ]
            for message in messages {
                mirror.apply(message)
                settings.apply(message)
                await commands.receive(message)
            }
            mirror.handle(.stateChanged(.ready))
            settings.handle(.stateChanged(.ready))
            await commands.handle(.stateChanged(.ready))
            // Observe the real feed's publication rather than waiting a
            // guessed duration or polling for its queued parse.
            await MainQueue.drained()
            try #require(feed.description(for: "general"), "the synthetic Core description must be current before hosting")
            #expect(dispatcher.state(of: try control(), in: "general").editable)
        }

        func control() throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "general"))
            return try #require(description.pages.first?.sections.first?.controls.first)
        }

        func core(_ text: String) {
            settings.apply(.settingsValue(.init(key: Self.key, origin: "another-phone",
                                               properties: [.init(name: Self.key, value: .utf8(text))])))
        }
    }

    @Test("callbacks captured by a prior rendered text entry cannot edit or refresh its replacement")
    func retiredTextBindingCannotAdoptOldCallbacks() {
        let row = SetupRowOutcomeOwner()
        var entry = SetupTextEntryDraft()
        entry.modelChanged("Old Core")
        entry.type("Old submission")
        let old = entry.submit(owner: row)
        row.retire()
        entry = SetupTextEntryDraft()
        entry.modelChanged("New snapshot Core")
        // These are the same identity-bearing setters and refresh methods
        // wired to the actual hosted TextField, held across its source reset.
        entry.type("Old Core", identity: old.identity)
        #expect(entry.value == "New snapshot Core")
        entry.modelChanged("Older queued Core", identity: old.identity)
        #expect(entry.value == "New snapshot Core")
        entry.type("New session unsent text")
        entry.modelChanged("New session latest Core")
        entry.receive(.refused("Old refusal."), edit: old, owner: row)
        #expect(entry.value == "New session unsent text" && row.problem == nil)
    }

    private static func textOutcome(_ outcome: SettingsWriteOutcome) -> SetupEditOutcome {
        switch outcome {
        case .accepted, .keptOnThisDevice: return .applied
        case .rejected(let reason): return reason == SeveralDevices.waitingReason ? .awaitingConfirmation : .refused(reason)
        case .notConfirmed: return .notSent(PropertyWriteOutcome.notConfirmed.reason)
        case .linkLost: return .notSent(PropertyWriteOutcome.linkLost.reason)
        case .notSent: return .notSent(PropertyWriteOutcome.notSent.reason)
        }
    }

    // MARK: The pad a row opens

    @Test("a row's pad: decimals only where the row has them, its unit and range, and wide ranges fit")
    func padShape() {
        let decimal = SetupNumberRow.pad(title: "CL2 frequency", value: 116, range: 1...200, unit: "MHz", decimals: 3,
                                         send: { _ in nil }, close: {})
        #expect(decimal.takesDecimals && decimal.decimals == 3 && decimal.unit == "MHz")
        #expect(decimal.numberRange == 1...200 && !decimal.signed)
        #expect(decimal.rangeText == "From 1 to 200 MHz. Now 116.000 MHz.")
        let whole = SetupNumberRow.pad(title: "Freq", value: 500, range: 0...20_000, unit: "Hz", decimals: 0,
                                       send: { _ in nil }, close: {})
        #expect(!whole.takesDecimals && whole.decimals == 0 && whole.unit == "Hz")
        #expect(whole.range == 0...20_000 && whole.value == 500)
        whole.press(.decimal)
        #expect(whole.entry == "500")
        // An Alex filter edge: 200 MHz to six places takes nine digits.
        let edge = SetupNumberRow.pad(title: "Start", value: 1.8, range: 0...200, unit: "MHz", decimals: 6,
                                      send: { _ in nil }, close: {})
        #expect(edge.longest == 9 && edge.entry == "1.8")
        for key in [ValuePadModel.Key.digit(2), .digit(3), .digit(4), .digit(5), .digit(6), .digit(7)] {
            edge.press(key)
        }
        #expect(edge.entry == "1.823456")
        // A notch at 61440000 Hz takes eight; short ranges keep seven.
        #expect(ValuePadModel.longest(100_000...61_440_000, decimals: 0) == 8)
        #expect(ValuePadModel.longest(-24...24, decimals: 1) == ValuePadModel.longestEntry)
    }

    @Test("an edit's end as the pad reads it: kept closes, a refusal or why nothing was sent stays in its words")
    func padAnswers() {
        #expect(DescribedControl.padAnswer(.applied).accepted)
        let refused = DescribedControl.padAnswer(.refused(Pages.clockRefusal))
        #expect(!refused.accepted && refused.reason == Pages.clockRefusal && refused.answeredByCore)
        let unsent = DescribedControl.padAnswer(.notSent("Not connected."))
        #expect(!unsent.accepted && unsent.reason == "Not connected." && !unsent.answeredByCore)
        #expect(DescribedControl.problem(.applied) == nil)
        #expect(DescribedControl.problem(.refused("No.")) == "No.")
    }

    @Test("a phone-only row's typed value is set at once and closes the pad")
    func phoneRowKeepsAtOnce() async throws {
        let host = ValuePadHost()
        var set: [Double] = []
        host.open { close in
            SetupNumberRow.pad(title: "Grid step", value: 10, range: 1...40, unit: "dB", decimals: 0,
                               send: { typed in
                                   set.append(typed)
                                   return SetupNumberRow.kept
                               }, close: close)
        }
        let pad = try #require(host.pad)
        pad.press(.delete)
        pad.press(.delete)
        pad.press(.digit(5))
        #expect(await pad.enter())
        #expect(set == [5] && host.pad == nil)
    }

    // MARK: HL2 CL2 frequency, typed

    @Test("CL2 frequency: greyed opens no pad; live, Enter value opens the MHz pad to 3 places, 24.576 is sent and a refusal shows as sent")
    func cl2Typed() async throws {
        let rig = try await Pages.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Pages.asV12Core(app, setupDescription: 22)
        Pages.withCapabilities(app, ["transmitSettingsVersion": .i64(15), "txPermitted": .bool(true)])
        await rig.station.deliverSetup(try Pages.coreCategories(peer: 22))
        #expect(await Pages.settle { app.setupPages.isCurrent && app.setupPages.categories["hardware"]?.version == 18 })
        let page = try #require(app.setupPages.categories["hardware"]?.pages.first { $0.id == Pages.clockPage })
        func control(_ id: String) throws -> SetupDescription.Control {
            try #require(page.sections.flatMap(\.controls).first { $0.id == "hardware.hl2Io.\(id)" })
        }
        let controls = app.setupControls
        let enable = try control("cl2Enable")
        let frequency = try control("cl2Freq")
        let path: [SetupTree.Route] = [.category("Hardware"), .described(category: "hardware", page: Pages.clockPage)]

        // Greyed (Enable CL2 off): no Enter value, nothing opens, the reason stays.
        #expect(!controls.state(of: frequency, in: "hardware").editable)
        try await Self.onScreen(rig, path: path) { window in
            let row = try #require(Self.element(frequency.id, in: window))
            #expect(row.accessibilityTraits.contains(.adjustable))
            #expect(!Self.actions(row).contains(SetupNumberRow.enterValueAction))
            #expect(Self.element("\(frequency.id).reason", in: window) != nil)
        }
        #expect(rig.router.pads.pad == nil)
        try await Self.shootBoth("typed-entry-cl2-greyed", rig: rig, path: path)

        let mac = FakeStation.setupPanelsRadioMac
        let turning = Task { await controls.edit(enable, in: "hardware", to: .bool(true)) }
        let on = try await Pages.settingsWrite("hardware/\(mac)/hl2/cl2Enable", rig.station)
        await rig.station.deliver(.settingsValue(.init(key: on.key, origin: on.origin, properties: on.properties)))
        #expect(await turning.value == .applied)
        #expect(await Pages.settle { controls.state(of: frequency, in: "hardware").editable })
        try await Self.shootBoth("typed-entry-cl2-live", rig: rig, path: path)

        // Live: VoiceOver's Enter value opens the pad with the Core's unit and places.
        try await Self.onScreen(rig, path: path) { window in
            let row = try #require(Self.element(frequency.id, in: window))
            #expect(row.accessibilityTraits.contains(.adjustable))
            #expect(Self.actions(row).contains(SetupNumberRow.enterValueAction))
            #expect(Self.perform(SetupNumberRow.enterValueAction, on: row))
        }
        let pad = try #require(rig.router.pads.pad)
        #expect(pad.title == "CL2 frequency" && pad.unit == "MHz" && pad.decimals == 3 && pad.takesDecimals)
        #expect(pad.numberRange == 1...200)
        Self.type("24.576", on: pad)
        #expect(pad.enterLabel == "Set to 24.576 MHz" && pad.canEnter)
        try await Self.shootAll("typed-entry-cl2-pad-24576", rig: rig, path: path)

        let key = "hardware/\(mac)/hl2/cl2FreqMHz"
        let entering = Task { await pad.enter() }
        let write = try await Pages.settingsWrite(key, rig.station)
        #expect(write.properties.first?.value == .utf8("24.576"))
        await rig.station.deliver(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        #expect(await entering.value)
        #expect(rig.router.pads.pad == nil)
        #expect(await Pages.settle { controls.state(of: frequency, in: "hardware").value == .decimal(24.576) })
        try await Self.shootBoth("typed-entry-cl2-set-24576", rig: rig, path: path)

        // Outside the Core's range: the pad turns it down and sends nothing.
        try await Self.onScreen(rig, path: path) { window in
            let row = try #require(Self.element(frequency.id, in: window))
            #expect(Self.perform(SetupNumberRow.enterValueAction, on: row))
        }
        let again = try #require(rig.router.pads.pad)
        #expect(again.entry == "24.576")
        Self.type("250", on: again)
        #expect(again.problem == "Choose a value from 1 to 200 MHz." && !again.canEnter)

        // Within it, not on a step: sent as typed, and the Core's refusal shows as sent.
        Self.type("150.55", on: again)
        #expect(again.canEnter)
        let refusing = Task { await again.enter() }
        let rejected = try await Pages.settingsWrite(key, rig.station) { $0.properties.first?.value == .utf8("150.55") }
        await rig.station.deliver(.settingsReject(LinkMessage.SettingsReject(
            key: rejected.key, properties: [.init(name: "value", value: .utf8("24.576"))], reason: Pages.clockRefusal)))
        #expect(await refusing.value == false)
        #expect(again.refusal == Pages.clockRefusal && rig.router.pads.pad === again)
        #expect(controls.state(of: frequency, in: "hardware").value == .decimal(24.576))
        try await Self.shootBoth("typed-entry-cl2-refused", rig: rig, path: path)
        again.cancel()
        #expect(rig.router.pads.pad == nil)
        await rig.app.disconnect()
    }

    // MARK: CFC band frequency, typed

    @Test("CFC band frequency: Enter value opens a whole-number Hz pad; the typed value goes whole through cfc.setProfile")
    func cfcTyped() async throws {
        let rig = try await Pages.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Pages.asV12Core(app, setupDescription: 22)
        Pages.withCapabilities(app, ["transmitSettingsVersion": .i64(15), "txPermitted": .bool(true)])
        await rig.station.deliverSetup(try Pages.coreCategories(peer: 22))
        #expect(await Pages.settle { app.setupPages.isCurrent && app.setupPages.categories["dsp"]?.version == 22 })
        let bands = try #require(app.setupPages.categories["dsp"]?.pages.flatMap(\.sections).flatMap(\.controls)
            .first { $0.id == "dsp.cfc.bands" })
        let controls = app.setupControls
        if app.mirror.object("transmit") == nil {
            await rig.station.deliver(.objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
                .init(name: "cfcProfile", value: .utf8(Pages.cfcProfile)),
            ])))
        } else {
            await rig.station.deliver(.delta(.init(key: "transmit", properties: [
                .init(name: "cfcProfile", value: .utf8(Pages.cfcProfile)),
            ])))
        }
        #expect(await Pages.settle { controls.cfcPanel(bands, in: "dsp").editable })
        let path: [SetupTree.Route] = [.category("DSP"), .described(category: "dsp", page: "dsp.cfc"),
                                       .cfcBands(category: "dsp", control: bands.id)]
        let id = "\(bands.id).band2.frequencyHz"
        try await Self.onScreen(rig, path: path) { window in
            let row = try #require(Self.element(id, in: window))
            #expect(Self.actions(row).contains(SetupNumberRow.enterValueAction))
            #expect(Self.perform(SetupNumberRow.enterValueAction, on: row))
        }
        let pad = try #require(rig.router.pads.pad)
        #expect(pad.title == "Band 2 Freq" && pad.unit == "Hz" && !pad.takesDecimals && pad.range == 0...20_000)
        #expect(pad.value == 500)
        Self.type("750", on: pad)
        #expect(pad.enterLabel == "Set to 750 Hz")
        try await Self.shootAll("typed-entry-cfc-band2-pad-750", rig: rig, path: path)

        controls.cfcSendPause = .milliseconds(40)
        #expect(await pad.enter())
        #expect(rig.router.pads.pad == nil)
        #expect(controls.cfcPanel(bands, in: "dsp").profile?.bands[1].frequencyHz == 750)
        let sent = await rig.station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == CfcProfile.setProfileVerb }
            return false
        }
        guard case .commandInvoke(let invoke)? = sent, case .utf8(let json)? = invoke.args.first?.value else {
            Issue.record("no cfc.setProfile")
            return
        }
        #expect(json.contains(#""frequencyHz":750"#))
        try await Self.shootBoth("typed-entry-cfc-band2-set-750", rig: rig, path: path)
        await rig.app.disconnect()
    }

    @Test("CFC band fields: each pad names its band, the editor's group title then the Core's label")
    func cfcPadNamesBand() async throws {
        let rig = try await Pages.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Pages.asV12Core(app, setupDescription: 22)
        Pages.withCapabilities(app, ["transmitSettingsVersion": .i64(15), "txPermitted": .bool(true)])
        await rig.station.deliverSetup(try Pages.coreCategories(peer: 22))
        #expect(await Pages.settle { app.setupPages.isCurrent && app.setupPages.categories["dsp"]?.version == 22 })
        let bands = try #require(app.setupPages.categories["dsp"]?.pages.flatMap(\.sections).flatMap(\.controls)
            .first { $0.id == "dsp.cfc.bands" })
        guard case .cfcProfile(let editor)? = bands.binding else {
            Issue.record("dsp.cfc.bands is not the CFC editor")
            return
        }
        let controls = app.setupControls
        if app.mirror.object("transmit") == nil {
            await rig.station.deliver(.objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
                .init(name: "cfcProfile", value: .utf8(Pages.cfcProfile)),
            ])))
        } else {
            await rig.station.deliver(.delta(.init(key: "transmit", properties: [
                .init(name: "cfcProfile", value: .utf8(Pages.cfcProfile)),
            ])))
        }
        #expect(await Pages.settle { controls.cfcPanel(bands, in: "dsp").editable })
        let count = try #require(controls.cfcPanel(bands, in: "dsp").profile?.bands.count)
        #expect(count == 5 && editor.columns.count == 5)
        let path: [SetupTree.Route] = [.category("DSP"), .described(category: "dsp", page: "dsp.cfc"),
                                       .cfcBands(category: "dsp", control: bands.id)]
        #expect(CfcBandsEditor.bandFieldTitle(2, "Freq") == "Band 2 Freq")
        // Every band field that opens a pad; the end bands' frequencies
        // sit on Low and High and open none.
        var expected: [String: String] = [:]
        for number in 1...count {
            for column in editor.columns where !(column.id == "frequencyHz" && (number == 1 || number == count)) {
                expected["\(bands.id).band\(number).\(column.id)"] = "\(CfcBandsEditor.bandTitle(number)) \(column.label)"
            }
        }
        var titles: [String: String] = [:]
        try await Self.onScreen(rig, path: path) { window in
            for number in 1...count {
                for column in editor.columns {
                    let id = "\(bands.id).band\(number).\(column.id)"
                    let row = try #require(Self.element(id, in: window), "\(id)")
                    guard Self.perform(SetupNumberRow.enterValueAction, on: row) else {
                        continue
                    }
                    let pad = try #require(rig.router.pads.pad, "\(id)")
                    titles[id] = pad.title
                    pad.cancel()
                    #expect(rig.router.pads.pad == nil)
                }
            }
        }
        #expect(titles.count == 23)
        #expect(titles == expected)
        #expect(titles["\(bands.id).band2.frequencyHz"] == "Band 2 Freq")

        try await Self.onScreen(rig, path: path) { window in
            let row = try #require(Self.element("\(bands.id).band2.frequencyHz", in: window))
            #expect(Self.perform(SetupNumberRow.enterValueAction, on: row))
        }
        #expect(rig.router.pads.pad?.title == "Band 2 Freq")
        try await Self.shootBoth("cleanup-0930-cfc-band2-freq-pad", rig: rig, path: path)
        rig.router.pads.pad?.cancel()
        await rig.app.disconnect()
    }

    @Test("CFC with Use Q Factors off: each band's Comp Q and EQ Q stay on screen, greyed, with the reason under the band")
    func cfcQRowsGreyedWithoutQFactors() async throws {
        let rig = try await Pages.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Pages.asV12Core(app, setupDescription: 22)
        Pages.withCapabilities(app, ["transmitSettingsVersion": .i64(15), "txPermitted": .bool(true)])
        await rig.station.deliverSetup(try Pages.coreCategories(peer: 22))
        #expect(await Pages.settle { app.setupPages.isCurrent && app.setupPages.categories["dsp"]?.version == 22 })
        let bands = try #require(app.setupPages.categories["dsp"]?.pages.flatMap(\.sections).flatMap(\.controls)
            .first { $0.id == "dsp.cfc.bands" })
        let fixed = Pages.cfcProfile.replacingOccurrences(of: #""parametric":true"#, with: #""parametric":false"#)
        #expect(fixed != Pages.cfcProfile)
        let controls = app.setupControls
        if app.mirror.object("transmit") == nil {
            await rig.station.deliver(.objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
                .init(name: "cfcProfile", value: .utf8(fixed)),
            ])))
        } else {
            await rig.station.deliver(.delta(.init(key: "transmit", properties: [
                .init(name: "cfcProfile", value: .utf8(fixed)),
            ])))
        }
        #expect(await Pages.settle { controls.cfcPanel(bands, in: "dsp").editable })
        let count = try #require(controls.cfcPanel(bands, in: "dsp").profile?.bands.count)
        #expect(controls.cfcPanel(bands, in: "dsp").profile?.parametric == false)
        let path: [SetupTree.Route] = [.category("DSP"), .described(category: "dsp", page: "dsp.cfc"),
                                       .cfcBands(category: "dsp", control: bands.id)]
        try await Self.onScreen(rig, path: path) { window in
            for number in 1...count {
                for column in ["compressionQ", "postEqQ"] {
                    let id = "\(bands.id).band\(number).\(column)"
                    let row = try #require(Self.element(id, in: window), "\(id)")
                    #expect(!Self.perform(SetupNumberRow.enterValueAction, on: row), "\(id)")
                }
                // The band's other values still change.
                let gain = try #require(Self.element("\(bands.id).band\(number).postEqGainDb", in: window))
                #expect(Self.perform(SetupNumberRow.enterValueAction, on: gain))
                rig.router.pads.pad?.cancel()
                let reason = try #require(Self.element("\(bands.id).band\(number).reason", in: window),
                                          "band \(number) reason")
                #expect(reason.accessibilityLabel == "Turn on Use Q Factors to change the Qs.")
                #expect(CfcBandsEditor.qFactorsOffReason == "Turn on Use Q Factors to change the Qs.")
            }
        }
        for scheme in [ColorScheme.light, .dark] {
            try await Pages.shoot("fix-setup-cfc-q-greyed-portrait-\(scheme == .dark ? "dark" : "light")", rig: rig,
                                  path: path, parts: 3, scheme: scheme)
        }
        await rig.app.disconnect()
    }

    // MARK: Helpers

    /// Types `text` on `pad` over whatever it held.
    static func type(_ text: String, on pad: ValuePadModel) {
        while !pad.entry.isEmpty {
            pad.press(.delete)
        }
        for character in text {
            if character == "." {
                pad.press(.decimal)
            } else if let digit = character.wholeNumberValue {
                pad.press(.digit(digit))
            }
        }
    }

    /// The Setup tab at `path` in a window while `check` runs.
    static func onScreen(_ rig: Pages.Rig, path: [SetupTree.Route],
                         _ check: (UIWindow) throws -> Void) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 2_400)
        rig.router.path = path
        let host = UIHostingController(rootView: SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil))
        host.view.frame = window.bounds
        let wasOn = Self.applicationAccessibility()
        Self.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
            Self.setApplicationAccessibility(wasOn)
        }
        await ShotWait.laidOut(window)
        try check(window)
    }

    /// SwiftUI builds its accessibility elements only while an assistive
    /// technology is on, and nothing public turns that on inside a unit
    /// test's process: XCTest's UI testing does it only for an app it
    /// launches, and that app has no Core, so no described Setup page or
    /// CFC editor to walk. So ``onScreen(_:path:_:)`` (and the TX panel's
    /// Mic Gain tests, the same way) turns
    /// application accessibility on through libAccessibility (the way
    /// accessibility snapshot tests do) and puts it back as it found it,
    /// because the setting outlives the test process on the simulator.
    private static func accessibilitySymbol(_ name: String) -> UnsafeMutableRawPointer? {
        guard let library = dlopen("/usr/lib/libAccessibility.dylib", RTLD_NOW) else {
            return nil
        }
        return dlsym(library, name)
    }

    static func applicationAccessibility() -> Bool {
        guard let symbol = accessibilitySymbol("_AXSApplicationAccessibilityEnabled") else {
            return false
        }
        typealias Enabled = @convention(c) () -> Bool
        return unsafeBitCast(symbol, to: Enabled.self)()
    }

    static func setApplicationAccessibility(_ on: Bool) {
        guard let symbol = accessibilitySymbol("_AXSApplicationAccessibilitySetEnabled") else {
            return
        }
        typealias SetEnabled = @convention(c) (Bool) -> Void
        unsafeBitCast(symbol, to: SetEnabled.self)(on)
    }

    /// The accessibility element with `id` under `root`.
    static func element(_ id: String, in root: NSObject) -> NSObject? {
        var queue: [NSObject] = [root]
        var visited = 0
        while !queue.isEmpty, visited < 20_000 {
            let node = queue.removeFirst()
            visited += 1
            if Self.identifier(node) == id {
                return node
            }
            if let elements = node.accessibilityElements as? [NSObject] {
                queue += elements
            } else {
                let count = node.accessibilityElementCount()
                if count != NSNotFound, count > 0 {
                    queue += (0..<count).compactMap { node.accessibilityElement(at: $0) as? NSObject }
                }
            }
            if let view = node as? UIView {
                queue += view.subviews
            }
        }
        return nil
    }

    /// An element's accessibility identifier, SwiftUI's own nodes included.
    static func identifier(_ node: NSObject) -> String? {
        if let identified = node as? UIAccessibilityIdentification, let id = identified.accessibilityIdentifier {
            return id
        }
        guard node.responds(to: NSSelectorFromString("accessibilityIdentifier")) else {
            return nil
        }
        return node.value(forKey: "accessibilityIdentifier") as? String
    }

    static func actions(_ element: NSObject) -> [String] {
        (element.accessibilityCustomActions ?? []).map(\.name)
    }

    /// Runs the element's action named `name`, as VoiceOver's rotor would.
    static func perform(_ name: String, on element: NSObject) -> Bool {
        guard let action = element.accessibilityCustomActions?.first(where: { $0.name == name }) else {
            return false
        }
        if let handler = action.actionHandler {
            return handler(action)
        }
        if let target = action.target as? NSObject {
            _ = target.perform(action.selector, with: action)
            return true
        }
        return false
    }

    /// Upright, light and dark.
    static func shootBoth(_ name: String, rig: Pages.Rig, path: [SetupTree.Route]) async throws {
        for scheme in [ColorScheme.light, .dark] {
            try await Pages.shoot("\(name)-portrait-\(scheme == .dark ? "dark" : "light")", rig: rig, path: path,
                                  scheme: scheme)
        }
    }

    /// Upright and sideways, light and dark, and upright in large type.
    static func shootAll(_ name: String, rig: Pages.Rig, path: [SetupTree.Route]) async throws {
        for scheme in [ColorScheme.light, .dark] {
            let look = scheme == .dark ? "dark" : "light"
            try await Pages.shoot("\(name)-portrait-\(look)", rig: rig, path: path, scheme: scheme)
            try await Pages.shoot("\(name)-landscape-\(look)", rig: rig, path: path, height: 402, width: 874,
                                  sideways: true, scheme: scheme)
        }
        try await Pages.shoot("\(name)-large-type", rig: rig, path: path, dynamicType: .accessibility2)
    }
    @Test("the real described-row result owner retires superseded and dismissed operations")
    func describedRowOwnsOnlyItsCurrentEdit() {
        let owner = SetupRowOutcomeOwner()
        let first = owner.begin()
        let second = owner.begin()
        owner.receive(.refused("Newest refusal."), edit: second)
        owner.receive(.refused("Old refusal."), edit: first)
        #expect(owner.problem == "Newest refusal.")
        owner.receive(.awaitingConfirmation, edit: second)
        #expect(owner.problem == "Newest refusal.")
        owner.retire()
        owner.receive(.refused("Dismissed refusal."), edit: second)
        #expect(owner.problem == "Newest refusal.")
    }

}
