// NereusSDR for iOS: TX Profile metadata, captured decisions and native phone/iPad questions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("Approved TX Profile flow", .serialized)
@MainActor
struct TxProfilesPanelTests {
    @MainActor final class Rig {
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let pages: SetupDescribedPages
        let dispatcher: SetupControlDispatcher
        let clock = TestLinkClock()
        let sent = Sent()
        var audio = ""
        var flow: SetupTxProfileFlow { dispatcher.txProfiles }
        lazy var main: MainScreenModel = {
            let model = MainScreenModel(mirror: mirror, settings: settings, commands: commands,
                operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in }))
            model.transmit.bindProfileFlow(flow)
            model.transmit.refresh()
            return model
        }()
        lazy var equalizer = TxEqualizerModel(mirror: mirror, transmit: main.transmit, commands: commands)
        func choose(_ name: String, equalizer: Bool, owner: UUID) {
            if equalizer { self.equalizer.refresh(); self.equalizer.selectProfile(name, owner: owner) }
            else { main.transmit.refresh(); main.transmit.selectProfile(name, owner: owner) }
        }
        @MainActor final class Sent {
            var messages: [LinkMessage] = []
            var invokes: [LinkMessage.CommandInvoke] {
                messages.compactMap { if case .commandInvoke(let invoke) = $0 { return invoke }; return nil }
            }
        }
        init() {
            let sent = sent
            let sender: MirrorStore.BoundSender = { message, permit in
                try await MainActor.run {
                    guard permit.handoff({ sent.messages.append(message); return true }) else { throw LinkSendError.notConnected }
                }
            }
            mirror = MirrorStore(send: { _ in }, clock: clock)
            settings = SettingsProxyClient(send: { _ in }, captureSender: { sender }, clock: clock)
            commands = CommandClient(clock: clock, send: { _ in }, captureSender: { sender })
            feed = SetupDescriptionFeed(store: mirror)
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                captureSender: { sender }, selectedSlice: { nil }, selectionChanges: Just(nil).eraseToAnyPublisher(), phone: nil)
            pages = SetupDescribedPages(feed: feed, store: mirror, hello: Just(nil).eraseToAnyPublisher())
        }
        func connect(descriptionVersion: Int = 15, omitsDescriptionCapability: Bool = false) async throws {
            audio = try #require(try SetupDescribedPagesTests.coreCategories(peer: descriptionVersion).first { $0.id == "audio" }).json
            await event(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                .hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")),
                .authResult(.init(accepted: true, reason: "", retryable: false)),
                .capabilities(.init(properties: (omitsDescriptionCapability ? [] : [.init(name: "setupDescriptionVersion", value: .i64(Int64(descriptionVersion)))]) + [
                    .init(name: "transmitSettingsVersion", value: .i64(3)), .init(name: "txPermitted", value: .bool(true))])),
                .schema(.init(className: "SetupDescription", fields: [.init(ordinal: 0, name: "revision", kind: .i64),
                    .init(ordinal: 1, name: "audio", kind: .utf8)])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(name: "revision", value: .i64(1)), .init(name: "audio", value: .utf8(audio))])),
                .objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
                    .init(name: "activeTxProfile", value: .utf8("Alpha")),
                    .init(name: "txProfilesJson", value: .utf8(#"["Alpha","Beta"]"#)),
                    .init(name: "micGainDb", value: .f64(10)), .init(name: "filterLowHz", value: .i64(100))])),
                .objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                    .init(name: "keyed", value: .bool(false)), .init(name: "tuning", value: .bool(false)),
                    .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false))])),
                .settingsSnapshot(.init(properties: [])), .snapshotComplete]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready)); await MainQueue.drained()
            if !omitsDescriptionCapability { try #require(await ShotWait.until { self.feed.description(for: "audio") != nil }) }
            pages.refresh()
            if descriptionVersion >= 15 && !omitsDescriptionCapability { flow.open(try control("activeProfile"), in: "audio") }
            else { _ = flow }
        }
        func event(_ event: StationSession.Event) async {
            mirror.handle(event); settings.handle(event); await commands.handle(event)
        }
        func control(_ suffix: String) throws -> SetupDescription.Control {
            try #require(pages.page("audio.txProfile", in: "audio")?.sections.flatMap(\.controls)
                .first { $0.id == "audio.txProfile." + suffix })
        }
        func push(_ name: String, _ value: LinkMessage.PropertyValue) {
            mirror.apply(.delta(.init(key: "transmit", properties: [.init(name: name, value: value)])))
        }
        func invoke(_ index: Int) async throws -> LinkMessage.CommandInvoke {
            try #require(await ShotWait.until { self.sent.invokes.count > index })
            return sent.invokes[index]
        }
        func answer(_ invoke: LinkMessage.CommandInvoke, accepted: Bool = true) async {
            await commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: accepted,
                reason: accepted ? "" : "Exact Core TX refusal", affected: [], values: [])))
        }
        func dirty() { push("micGainDb", .f64(11)) }
    }

    @Test("a flow retained by the real models fails closed after its dispatcher is released", arguments: ["name", "dirty", "queuedSelect"])
    func retainedFlowAfterDispatcherReleaseIsUnavailable(_ pending: String) async throws {
        var rig: Rig? = Rig()
        try await rig?.connect()
        let flow = try #require(rig?.flow)
        let main = try #require(rig?.main), equalizer = try #require(rig?.equalizer)
        let mirror = try #require(rig?.mirror), sent = try #require(rig?.sent)
        let save = try #require(try rig?.control("save")), active = try #require(try rig?.control("activeProfile"))
        let owner = UUID()
        weak var releasedDispatcher = rig?.dispatcher
        switch pending {
        case "name": flow.beginSave(save, in: "audio", owner: owner)
        case "dirty": rig?.dirty(); rig?.choose("Beta", equalizer: true, owner: owner)
        default: rig?.choose("Beta", equalizer: false, owner: owner)
        }
        // Queue the actual object-key reattachment callback before releasing
        // the owner. No await lets a queued select hand off before release.
        mirror.apply(.objectCreate(.init(key: "unrelated", className: "Unrelated", properties: [])))
        rig = nil
        try #require(releasedDispatcher == nil, "Retaining flow/model must not retain its dispatcher")
        #expect(flow.selectionReason == SetupControlDispatcher.notConnectedReason)
        #expect(!flow.usesLegacySelection && flow.question == nil && flow.questionIdentity == nil)
        #expect(main.transmit.profileSelectionReason == SetupControlDispatcher.notConnectedReason)
        equalizer.refresh()
        #expect(equalizer.profileReason != nil)
        await MainQueue.drained()
        #expect(sent.messages.isEmpty, "Queued selection must not outlive its dispatcher authority")
        flow.open(active, in: "audio")
        flow.beginSave(save, in: "audio", owner: owner)
        flow.name = "New profile"; await flow.acceptName(); await flow.confirmOverwrite()
        flow.choose("Beta", owner: owner); await flow.saveAndSwitch(); await flow.discardAndSwitch()
        flow.close(); flow.retire(owner: owner)
        // Subscriptions/queued refreshes must also be safe with their publisher
        // retained independently of the dispatcher after an old screen closes.
        mirror.apply(.delta(.init(key: "transmit", properties: [.init(name: "micGainDb", value: .f64(12))])))
        await MainQueue.drained(); equalizer.refresh()
        #expect(releasedDispatcher == nil && sent.messages.isEmpty)
        #expect(flow.question == nil && flow.questionIdentity == nil && !flow.busy)
        #expect(flow.problem(for: owner) == nil && flow.selectionReason == SetupControlDispatcher.notConnectedReason)
    }

    @Test func canonicalMetadataIsLiveAndOnlyItsWatchMakesThePageDirty() async throws {
        let rig = Rig(); try await rig.connect()
        let active = try rig.control("activeProfile"), save = try rig.control("save")
        #expect(rig.dispatcher.state(of: active, in: "audio").editable)
        #expect(rig.dispatcher.state(of: save, in: "audio").editable)
        let metadata = try #require(active.modern?.profileUnsavedChanges)
        #expect(metadata.watch.count == 60 && metadata.watch.first == "micGainDb")
        #expect(metadata.saveVerb == "txProfile.save")
        #expect(save.modern?.profilePrompt?.initial.object == "transmit")
        #expect(!rig.flow.dirty && rig.flow.currentName == "Alpha")
        rig.push("filterLowHz", .i64(200))
        #expect(!rig.flow.dirty, "The consumer must not extend Core's watch list")
        rig.dirty(); rig.push("micGainDb", .f64(10))
        #expect(rig.flow.dirty, "A watched change remains dirty after a return to the opening value")
        rig.push("activeTxProfile", .utf8("Beta"))
        #expect(!rig.flow.dirty && rig.flow.currentName == "Beta")
        #expect(rig.sent.messages.isEmpty)
    }

    @Test("an unrelated Setup descriptor refresh preserves sticky TX dirty state and asks before selecting")
    func unrelatedSetupRefreshPreservesStickyDirtyAndUnsavedQuestion() async throws {
        let rig = Rig(); try await rig.connect()
        let activeBefore = try rig.control("activeProfile")
        let generationBefore = rig.feed.generation
        let valuesBefore = try #require(rig.mirror.object("transmit")).values
        // The watched value returns to its original value. Dirty is the
        // occurrence of a change, not a comparison of current values.
        rig.dirty(); rig.push("micGainDb", .f64(10))
        try #require(rig.flow.dirty)
        #expect(rig.mirror.object("transmit")?.values == valuesBefore)
        var root = try #require(JSONSerialization.jsonObject(with: Data(rig.audio.utf8)) as? [String: Any])
        var pages = try #require(root["pages"] as? [[String: Any]])
        let pageIndex = try #require(pages.firstIndex { $0["id"] as? String == "audio.txProfile" })
        var sections = try #require(pages[pageIndex]["sections"] as? [[String: Any]])
        let sectionIndex = try #require(sections.firstIndex { $0["title"] as? String == "TX Filter" })
        // Only this unrelated section's title changes. Neither TX Profile
        // descriptor, the active name, nor any watched property changes.
        sections[sectionIndex]["title"] = "TX Filter descriptor refresh"
        pages[pageIndex]["sections"] = sections; root["pages"] = pages
        let refreshed = String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
        rig.mirror.apply(.delta(.init(key: "setup", properties: [.init(name: "audio", value: .utf8(refreshed))])))
        try #require(await ShotWait.until { rig.feed.generation > generationBefore })
        await MainQueue.drained(); rig.pages.refresh()
        #expect(try rig.control("activeProfile") == activeBefore)
        #expect(rig.mirror.object("transmit")?.values == valuesBefore)
        #expect(rig.flow.currentName == "Alpha" && rig.flow.dirty,
                "Refreshing unrelated Setup metadata cannot reset a dirty TX Profile")
        rig.flow.choose("Beta")
        try #require(await ShotWait.until { rig.flow.question != nil || !rig.sent.messages.isEmpty })
        #expect(rig.flow.question == .unsaved(title: "Unsaved Profile Changes",
            text: "The current profile (\"Alpha\") has been modified.\nSave before switching?"))
        #expect(rig.sent.messages.isEmpty, "The decision must be asked before any txProfile.select handoff")
        await rig.event(.stateChanged(.stopped))
        rig.flow.close()
    }

    @Test func descriptorABARevokesOldDecisionWithoutErasingDirtyHistory() async throws {
        let rig = Rig(); try await rig.connect(); rig.dirty()
        rig.flow.choose("Beta"); let old = try #require(rig.flow.questionIdentity)
        let generation = rig.feed.generation
        rig.mirror.apply(.delta(.init(key: "setup", properties: [.init(name: "audio", value: .utf8(rig.audio.replacingOccurrences(of: "Save...", with: "Save now...")))])))
        try #require(await ShotWait.until { rig.feed.generation > generation })
        rig.mirror.apply(.delta(.init(key: "setup", properties: [.init(name: "audio", value: .utf8(rig.audio))])))
        await MainQueue.drained(); rig.pages.refresh()
        #expect(rig.flow.dirty && rig.flow.question == nil)
        rig.flow.choose("Beta")
        #expect(rig.flow.questionIdentity != old && rig.flow.question != nil && rig.flow.dirty)
        #expect(rig.sent.messages.isEmpty)
        rig.flow.cancel()
    }

    @Test(arguments: [false, true])
    func bothExistingModelPathsGuardAfterSetupCloses(_ equalizer: Bool) async throws {
        let rig = Rig(); try await rig.connect(); _ = rig.main
        let owner = UUID()
        rig.flow.close(); rig.dirty()
        rig.choose("Beta", equalizer: equalizer, owner: owner)
        #expect(rig.flow.question != nil && rig.flow.presentationOwner == owner)
        #expect(rig.main.transmit.activeProfile == "Alpha" && rig.equalizer.activeProfile == "Alpha")
        #expect(rig.sent.messages.isEmpty)
        rig.flow.retire(owner: owner)
        #expect(rig.flow.dirty && rig.flow.question == nil && rig.sent.messages.isEmpty)
        rig.choose("Beta", equalizer: equalizer, owner: owner)
        let task = Task { await rig.flow.saveAndSwitch() }
        let save = try await rig.invoke(0)
        #expect(save.verb == "txProfile.save" && save.args == [.init(name: "name", value: .utf8("Alpha"))])
        #expect(rig.sent.messages.count == 1 && rig.main.transmit.activeProfile == "Alpha")
        await rig.answer(save)
        let select = try await rig.invoke(1)
        #expect(select.verb == "txProfile.select" && select.args == [.init(name: "name", value: .utf8("Beta"))])
        await rig.answer(select); await task.value
        #expect(rig.sent.messages.count == 2 && rig.main.transmit.activeProfile == "Alpha")
        rig.push("activeTxProfile", .utf8("Beta")); await MainQueue.drained()
        rig.main.transmit.refresh(); rig.equalizer.refresh()
        #expect(rig.main.transmit.activeProfile == "Beta" && rig.equalizer.activeProfile == "Beta")
    }

    @Test(arguments: [false, true])
    func existingModelCleanAndDiscardSelectionsFollowOnlyCoreEcho(_ equalizer: Bool) async throws {
        let rig = Rig(); try await rig.connect(); let owner = UUID()
        rig.choose("Beta", equalizer: equalizer, owner: owner)
        let clean = try await rig.invoke(0)
        #expect(clean.verb == "txProfile.select" && rig.flow.question == nil)
        #expect(rig.main.transmit.activeProfile == "Alpha" && rig.equalizer.activeProfile == "Alpha")
        await rig.answer(clean); await MainQueue.drained()
        rig.dirty(); rig.choose("Beta", equalizer: equalizer, owner: owner)
        #expect(rig.flow.question != nil && rig.sent.invokes.count == 1)
        let operation = Task { await rig.flow.discardAndSwitch() }
        let discard = try await rig.invoke(1)
        #expect(discard.verb == "txProfile.select" && discard.args == [.init(name: "name", value: .utf8("Beta"))])
        #expect(rig.main.transmit.activeProfile == "Alpha" && rig.equalizer.activeProfile == "Alpha")
        await rig.answer(discard); await operation.value
        #expect(rig.sent.invokes.count == 2)
    }

    @Test func unboundOlderModesModelKeepsCompatibilityButNeverBypassesOfferedV15() async throws {
        let rig = Rig()
        let main = MainScreenModel(mirror: rig.mirror, settings: rig.settings, commands: rig.commands,
            operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in }))
        try await rig.connect(descriptionVersion: 11, omitsDescriptionCapability: true)
        main.transmit.refresh(); main.transmit.selectProfile("Beta")
        let old = try await rig.invoke(0)
        #expect(old.verb == "txProfile.select" && main.transmit.profileFlow == nil)
        await rig.answer(old); await MainQueue.drained()
        await rig.event(.stateChanged(.stopped)); try await rig.connect()
        await rig.event(.message(.capabilities(.init(properties: [.init(name: "transmitSettingsVersion", value: .i64(3))]))))
        main.transmit.refresh(); main.transmit.selectProfile("Beta"); await MainQueue.drained()
        #expect(main.transmit.profileSelectionReason != nil && rig.sent.invokes.count == 1)
    }

    @Test(arguments: [false, true])
    func explicitlyOlderDescriptorKeepsExistingCleanSelection(_ equalizer: Bool) async throws {
        let rig = Rig(); try await rig.connect(descriptionVersion: 11)
        let owner = UUID(); rig.choose("Beta", equalizer: equalizer, owner: owner)
        #expect(rig.flow.usesLegacySelection && rig.flow.question == nil)
        let select = try await rig.invoke(0)
        #expect(select.verb == "txProfile.select" && select.args == [.init(name: "name", value: .utf8("Beta"))])
        await rig.answer(select)
    }

    @Test(arguments: [false, true])
    func completeOldCoreWithoutDescriptionCapabilityKeepsOriginalModelPath(_ equalizer: Bool) async throws {
        let rig = Rig(); try await rig.connect(descriptionVersion: 11, omitsDescriptionCapability: true)
        let owner = UUID(); rig.choose("Beta", equalizer: equalizer, owner: owner)
        #expect(rig.flow.usesLegacySelection && rig.flow.question == nil)
        let select = try await rig.invoke(0)
        #expect(select.verb == "txProfile.select" && select.args == [.init(name: "name", value: .utf8("Beta"))])
        await rig.answer(select)
    }

    @Test func incompleteUnknownAndCurrentV15CapabilityLossStayClosed() async throws {
        let rig = Rig(); _ = rig.main
        #expect(!rig.flow.usesLegacySelection && rig.main.transmit.profileSelectionReason != nil)
        await rig.event(.stateChanged(.receivingSnapshot))
        await rig.event(.message(.capabilities(.init(properties: [.init(name: "transmitSettingsVersion", value: .i64(3))]))))
        rig.main.transmit.selectProfile("Beta")
        #expect(!rig.flow.usesLegacySelection && rig.sent.messages.isEmpty)
        try await rig.connect(); rig.dirty()
        await rig.event(.message(.capabilities(.init(properties: [.init(name: "transmitSettingsVersion", value: .i64(3))]))))
        await MainQueue.drained()
        rig.choose("Beta", equalizer: false, owner: UUID())
        #expect(!rig.flow.usesLegacySelection && rig.sent.messages.isEmpty)
        await rig.event(.stateChanged(.stopped))
        try await rig.connect(descriptionVersion: 11, omitsDescriptionCapability: true)
        #expect(rig.flow.usesLegacySelection, "A genuinely old replacement session may use compatibility")
    }

    @Test(arguments: [false, true])
    func supportedMissingChangingAndDowngradedMetadataNeverBypassesGuard(_ equalizer: Bool) async throws {
        let rig = Rig(); try await rig.connect(); _ = rig.main; rig.dirty()
        let owner = UUID()
        // Raw descriptor replacement is unavailable before feed publication.
        rig.mirror.apply(.delta(.init(key: "setup", properties: [.init(name: "audio", value: .utf8("{}"))])))
        rig.choose("Beta", equalizer: equalizer, owner: owner)
        #expect(!rig.flow.usesLegacySelection && rig.sent.messages.isEmpty)
        await MainQueue.drained()
        rig.choose("Beta", equalizer: equalizer, owner: owner)
        #expect(rig.sent.messages.isEmpty)
        rig.mirror.apply(.capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(11)),
            .init(name: "transmitSettingsVersion", value: .i64(3)), .init(name: "txPermitted", value: .bool(true))])))
        await MainQueue.drained()
        rig.choose("Beta", equalizer: equalizer, owner: owner)
        #expect(!rig.flow.usesLegacySelection && rig.sent.messages.isEmpty,
                "A session that knew the canonical guard cannot downgrade into an unguarded selector")
    }

    @Test(arguments: [false, true])
    func closedOldSurfaceCannotRetireReplacementQuestionOrOwnItsResult(_ equalizer: Bool) async throws {
        let rig = Rig(); try await rig.connect(); _ = rig.main; rig.dirty()
        let old = UUID(), replacement = UUID()
        rig.choose("Beta", equalizer: equalizer, owner: old)
        let task = Task { await rig.flow.saveAndSwitch() }
        let save = try await rig.invoke(0)
        rig.flow.retire(owner: old)
        rig.choose("Beta", equalizer: !equalizer, owner: replacement)
        let replacementQuestion = try #require(rig.flow.questionIdentity)
        rig.flow.retire(owner: old); rig.flow.close()
        await rig.answer(save, accepted: false); await task.value
        #expect(rig.flow.questionIdentity == replacementQuestion && rig.flow.presentationOwner == replacement)
        #expect(rig.flow.question != nil && rig.flow.problem(for: old) == nil && rig.flow.problem(for: replacement) == nil)
        #expect(rig.sent.messages.count == 1)
        rig.flow.retire(owner: replacement)
    }

    @Test func trimmedEmptyAndExactOverwriteNoSendNothing() async throws {
        let rig = Rig(); try await rig.connect()
        let save = try rig.control("save")
        rig.flow.beginSave(save, in: "audio")
        #expect(rig.flow.name == "Alpha")
        rig.flow.name = " \n\t "; await rig.flow.acceptName()
        #expect(rig.flow.question == nil && rig.sent.messages.isEmpty)
        rig.flow.beginSave(save, in: "audio"); rig.flow.name = "  Alpha\n"; await rig.flow.acceptName()
        #expect(rig.flow.question == .overwrite(title: "Overwrite TX Profile", text: "A profile named \"Alpha\" already exists.  Overwrite?"))
        rig.flow.cancel()
        #expect(rig.sent.messages.isEmpty)
    }

    @Test(arguments: ["Alpha", "alpha", "New profile"])
    func acceptedNameUsesOneTrimmedTextArgument(_ name: String) async throws {
        let rig = Rig(); try await rig.connect(); rig.dirty()
        rig.flow.beginSave(try rig.control("save"), in: "audio"); rig.flow.name = " \(name) \n"
        let save = Task {
            await rig.flow.acceptName()
            if case .overwrite? = rig.flow.question { await rig.flow.confirmOverwrite() }
        }
        let invoke = try await rig.invoke(0)
        #expect(invoke.verb == "txProfile.save" && invoke.args == [.init(name: "name", value: .utf8(name))])
        #expect(rig.flow.busy && rig.flow.currentName == "Alpha" && rig.flow.dirty)
        await rig.answer(invoke); await save.value
        #expect(!rig.flow.busy && !rig.flow.dirty && rig.flow.problem == nil)
        await rig.answer(invoke); await MainQueue.drained()
        #expect(rig.sent.messages.count == 1)
    }

    @Test func newNameManifestEchoBeforeResultDoesNotLoseItsSaveOutcome() async throws {
        let rig = Rig(); try await rig.connect(); rig.dirty()
        rig.flow.beginSave(try rig.control("save"), in: "audio"); rig.flow.name = "New"
        let task = Task { await rig.flow.acceptName() }
        let invoke = try await rig.invoke(0)
        rig.push("txProfilesJson", .utf8(#"["Alpha","Beta","New"]"#))
        #expect(rig.flow.busy && rig.flow.dirty)
        await rig.answer(invoke); await task.value
        #expect(!rig.flow.busy && !rig.flow.dirty && rig.flow.currentName == "Alpha")
        #expect(rig.sent.messages.count == 1)
    }

    @Test func dirtyCancelAndDiscardUseOnlyTheChosenCoreName() async throws {
        let rig = Rig(); try await rig.connect(); rig.dirty()
        rig.flow.choose("Beta")
        #expect(rig.flow.question == .unsaved(title: "Unsaved Profile Changes",
            text: "The current profile (\"Alpha\") has been modified.\nSave before switching?"))
        #expect(rig.flow.currentName == "Alpha" && rig.sent.messages.isEmpty)
        rig.flow.cancel(); #expect(rig.flow.dirty && rig.sent.messages.isEmpty)
        rig.flow.choose("Beta")
        let discard = Task { await rig.flow.discardAndSwitch() }
        let invoke = try await rig.invoke(0)
        #expect(invoke.verb == "txProfile.select" && invoke.args == [.init(name: "name", value: .utf8("Beta"))])
        await rig.answer(invoke); await discard.value
        #expect(rig.flow.currentName == "Alpha", "A result alone must not fabricate the active profile echo")
        rig.push("activeTxProfile", .utf8("Beta"))
        #expect(rig.flow.currentName == "Beta" && !rig.flow.dirty && rig.sent.messages.count == 1)
    }

    @Test func saveBeforeSwitchWaitsForFinalSuccessAndSendsOneSuffix() async throws {
        let rig = Rig(); try await rig.connect(); rig.dirty(); rig.flow.choose("Beta")
        let switchTask = Task { await rig.flow.saveAndSwitch() }
        let save = try await rig.invoke(0)
        #expect(save.verb == "txProfile.save" && save.args == [.init(name: "name", value: .utf8("Alpha"))])
        #expect(rig.flow.currentName == "Alpha" && rig.sent.messages.count == 1)
        await rig.flow.saveAndSwitch(); await rig.flow.discardAndSwitch()
        #expect(rig.sent.messages.count == 1)
        await rig.answer(save)
        let select = try await rig.invoke(1)
        #expect(select.verb == "txProfile.select" && select.args == [.init(name: "name", value: .utf8("Beta"))])
        await rig.answer(save); await rig.answer(select); await switchTask.value
        #expect(rig.sent.messages.count == 2 && rig.flow.currentName == "Alpha")
        rig.push("activeTxProfile", .utf8("Beta")); #expect(!rig.flow.dirty)
    }

    @Test(arguments: [false, true])
    func refusalOrManualTimeoutRetainsCurrentAndNeverResumesSuffix(_ timeout: Bool) async throws {
        let rig = Rig(); try await rig.connect(); rig.dirty(); rig.flow.choose("Beta")
        let switchTask = Task { await rig.flow.saveAndSwitch() }
        let save = try await rig.invoke(0)
        if timeout {
            try #require(await ShotWait.until { rig.clock.pendingDueTimes.contains(5_000) })
            await rig.clock.advance(by: 5_000)
        } else { await rig.answer(save, accepted: false) }
        await switchTask.value
        #expect(rig.flow.currentName == "Alpha" && rig.flow.dirty && !rig.flow.busy)
        #expect(rig.flow.problem == (timeout ? SetupControlDispatcher.noAnswerReason : "Exact Core TX refusal"))
        await rig.answer(save); await MainQueue.drained()
        #expect(rig.sent.messages.count == 1)
    }

    @Test(arguments: ["profileABA", "watchABA", "descriptorABA", "session"])
    func capturedOperationCannotSwitchAfterAuthorityOrValueReplacement(_ change: String) async throws {
        let rig = Rig(); try await rig.connect(); rig.dirty(); rig.flow.choose("Beta")
        let old = rig.flow.questionIdentity
        let switchTask = Task { await rig.flow.saveAndSwitch() }
        let save = try await rig.invoke(0)
        switch change {
        case "profileABA": rig.push("activeTxProfile", .utf8("Beta")); rig.push("activeTxProfile", .utf8("Alpha"))
        case "watchABA": rig.push("micGainDb", .f64(12)); rig.push("micGainDb", .f64(11))
        case "descriptorABA":
            rig.mirror.apply(.delta(.init(key: "setup", properties: [.init(name: "audio", value: .utf8(rig.audio.replacingOccurrences(of: "Save...", with: "Save now...")))])))
            await MainQueue.drained()
            rig.mirror.apply(.delta(.init(key: "setup", properties: [.init(name: "audio", value: .utf8(rig.audio))])))
            await MainQueue.drained()
        default:
            await rig.event(.stateChanged(.stopped)); try await rig.connect()
        }
        await rig.answer(save); await switchTask.value; await MainQueue.drained()
        #expect(rig.flow.questionIdentity != old && rig.flow.question == nil && !rig.flow.busy)
        #expect(rig.flow.currentName == "Alpha" && rig.flow.problem == nil)
        #expect(rig.sent.messages.count == 1, "An old accepted save cannot hand its selection to a replacement")
    }

    @Test func cleanSelectionAndDeleteKeepTheirExistingCommandContracts() async throws {
        let rig = Rig(); try await rig.connect()
        rig.flow.choose("Alpha"); #expect(rig.sent.messages.isEmpty)
        rig.flow.choose("Beta"); let select = try await rig.invoke(0)
        #expect(rig.flow.question == nil && select.verb == "txProfile.select")
        await rig.answer(select); await MainQueue.drained()
        let delete = try rig.control("delete")
        let question = try #require(rig.dispatcher.question(for: delete, in: "audio", value: nil))
        switch question {
        case .success(let asked): #expect(asked.text == "Delete profile \"Alpha\"?")
        case .failure(let refusal): Issue.record("\(refusal.reason)")
        }
        #expect(rig.sent.messages.count == 1)
    }

    @Test("actual described TX page, native naming/overwrite/dirty questions on phone and iPad", arguments: [false, true])
    func nativeHostedQuestionsAndLayout(_ tablet: Bool) async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = Rig(); try await rig.connect()
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: tablet ? 1024 : 402, height: tablet ? 1366 : 874)
        window.windowLevel = .alert + 1
        let host = UIHostingController(rootView: NavigationStack {
            DescribedPage(pages: rig.pages, dispatcher: rig.dispatcher, category: "audio", pageId: "audio.txProfile", profileOwner: rig.flow.setupOwner)
        }.preferredColorScheme(.dark))
        window.rootViewController = host; host.view.frame = window.bounds; window.makeKeyAndVisible()
        defer { rig.flow.close(); window.isHidden = true; window.rootViewController = nil }
        await ShotWait.laidOut(window)
        for suffix in ["activeProfile", "save", "delete"] {
            let element = try #require(SetupTypedEntryTests.element("audio.txProfile." + suffix, in: window))
            #expect(!element.accessibilityFrame.isEmpty)
            #expect(UIAccessibility.convertToScreenCoordinates(window.bounds, in: window).intersects(element.accessibilityFrame))
        }
        try saveShot(tablet ? "ipad-page" : "phone-page", window: window)
        rig.flow.beginSave(try rig.control("save"), in: "audio")
        let naming = try await nativeAlert(host)
        #expect(naming.title == "Save TX Profile" && naming.textFields?.first?.text == "Alpha")
        #expect(naming.textFields?.first?.accessibilityLabel == "New profile name:")
        #expect(naming.actions.map(\.title) == ["Save", "Cancel"])
        try saveShot(tablet ? "ipad-name" : "phone-name", window: window)
        rig.flow.name = "Alpha"; await rig.flow.acceptName()
        try #require(await ShotWait.until { Self.alert(host)?.title == "Overwrite TX Profile" })
        let overwrite = try #require(Self.alert(host))
        #expect(overwrite.message == "A profile named \"Alpha\" already exists.  Overwrite?")
        #expect(overwrite.actions.map(\.title) == ["Yes", "No"] && overwrite.preferredAction?.title == "No")
        try saveShot(tablet ? "ipad-overwrite" : "phone-overwrite", window: window)
        rig.flow.cancel()
        try #require(await ShotWait.until { Self.alert(host) == nil })
        rig.dirty(); rig.flow.choose("Beta")
        let dirty = try await nativeAlert(host)
        #expect(dirty.title == "Unsaved Profile Changes" && dirty.actions.map(\.title) == ["Save", "Discard", "Cancel"])
        #expect(dirty.message == "The current profile (\"Alpha\") has been modified.\nSave before switching?")
        try saveShot(tablet ? "ipad-dirty" : "phone-dirty", window: window)
        rig.flow.cancel(); #expect(rig.flow.currentName == "Alpha" && rig.sent.messages.isEmpty)
        try #require(await ShotWait.until { Self.alert(host) == nil })
        // Exercise the real CommandClient deadline and its row note while
        // this actual phone/iPad page remains hosted and visible.
        for timeout in [false, true] {
            rig.flow.choose("Beta"); _ = try await nativeAlert(host)
            let operation = Task { await rig.flow.saveAndSwitch() }
            let index = rig.sent.invokes.count
            let save = try await rig.invoke(index)
            if timeout {
                try #require(await ShotWait.until { rig.clock.pendingDueTimes.contains(rig.clock.now + 5_000) })
                await rig.clock.advance(by: 5_000)
            } else { await rig.answer(save, accepted: false) }
            await operation.value; await ShotWait.laidOut(window)
            #expect(rig.flow.currentName == "Alpha" && rig.flow.dirty && !rig.flow.busy)
            let reason = timeout ? SetupControlDispatcher.noAnswerReason : "Exact Core TX refusal"
            #expect(rig.flow.problem == reason)
            let note = try #require(SetupTypedEntryTests.element("audio.txProfile.activeProfile.problem", in: window))
            #expect(note.accessibilityLabel == reason)
            #expect(SetupTypedEntryTests.element("audio.txProfile.save.problem", in: window) == nil)
            await rig.answer(save); await MainQueue.drained()
            #expect(rig.sent.messages.count == index + 1, "A late result cannot resume the native page's switch")
            try saveShot((tablet ? "ipad-" : "phone-") + (timeout ? "timeout" : "refusal"), window: window)
        }
        rig.flow.choose("Beta"); _ = try await nativeAlert(host)
        await rig.event(.stateChanged(.stopped)); try await rig.connect()
        try #require(await ShotWait.until { Self.alert(host) == nil })
        #expect(rig.flow.currentName == "Alpha" && rig.flow.question == nil && rig.flow.problem == nil)
        #expect(rig.sent.messages.count == 2, "Session replacement retires the native pending question without a command")
    }

    @Test("existing TX drawer and EQ host the approved question and retain Core profile on refusal/timeout", arguments: [false, true], [false, true])
    func nativeExistingSurfacesGuardAndOutcomes(_ equalizer: Bool, _ tablet: Bool) async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = Rig(); try await rig.connect(); _ = rig.main; rig.equalizer.refresh()
        let owner = UUID(), identifier = equalizer ? "txEq.profile" : "txProfile"
        let noteIdentifier = equalizer ? "txEq.profileNote" : "txPanelProfileNote"
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: tablet ? 1024 : 402, height: tablet ? 1366 : 874)
        window.windowLevel = .alert + 1
        let host = UIHostingController(rootView: surface(rig, equalizer: equalizer, owner: owner))
        window.rootViewController = host; host.view.frame = window.bounds; window.makeKeyAndVisible()
        defer { rig.flow.retire(owner: owner); window.isHidden = true; window.rootViewController = nil }
        await ShotWait.laidOut(window)
        try await showProfile(identifier, host: host, window: window)
        rig.flow.close(); rig.dirty()
        rig.choose("Beta", equalizer: equalizer, owner: owner)
        let question = try await nativeAlert(host)
        #expect(question.title == "Unsaved Profile Changes")
        #expect(question.actions.map(\.title) == ["Save", "Discard", "Cancel"])
        #expect(rig.sent.messages.isEmpty && rig.main.transmit.activeProfile == "Alpha" && rig.equalizer.activeProfile == "Alpha")
        let picker = try #require(SetupTypedEntryTests.element(identifier, in: window))
        #expect(picker.accessibilityValue == "Alpha")
        try saveShot((tablet ? "ipad-" : "phone-") + (equalizer ? "eq-dirty" : "drawer-dirty"), window: window)
        rig.flow.cancel(); try #require(await ShotWait.until { Self.alert(host) == nil })
        #expect(rig.sent.messages.isEmpty && rig.flow.dirty)
        for timeout in [false, true] {
            rig.choose("Beta", equalizer: equalizer, owner: owner); _ = try await nativeAlert(host)
            let index = rig.sent.invokes.count
            let operation = Task { await rig.flow.saveAndSwitch() }
            let save = try await rig.invoke(index)
            #expect(save.verb == "txProfile.save" && save.args == [.init(name: "name", value: .utf8("Alpha"))])
            if timeout {
                try #require(await ShotWait.until { rig.clock.pendingDueTimes.contains(rig.clock.now + 5_000) })
                await rig.clock.advance(by: 5_000)
            } else { await rig.answer(save, accepted: false) }
            await operation.value; await ShotWait.laidOut(window)
            let reason = timeout ? SetupControlDispatcher.noAnswerReason : "Exact Core TX refusal"
            #expect(rig.flow.problem(for: owner) == reason && rig.flow.dirty)
            #expect(rig.main.transmit.activeProfile == "Alpha" && rig.equalizer.activeProfile == "Alpha")
            scrollToBottom(host.view); await ShotWait.laidOut(window)
            let note = try #require(SetupTypedEntryTests.element(noteIdentifier, in: window))
            #expect(note.accessibilityLabel == reason)
            await rig.answer(save); await MainQueue.drained()
            #expect(rig.sent.invokes.count == index + 1, "Refusal, timeout and late success cannot select")
        }
        rig.choose("Beta", equalizer: equalizer, owner: owner); _ = try await nativeAlert(host)
        let index = rig.sent.invokes.count
        let operation = Task { await rig.flow.saveAndSwitch() }
        let save = try await rig.invoke(index); await rig.answer(save)
        let select = try await rig.invoke(index + 1)
        #expect(select.verb == "txProfile.select" && select.args == [.init(name: "name", value: .utf8("Beta"))])
        await rig.answer(select); await operation.value; await MainQueue.drained()
        #expect(rig.sent.invokes.count == index + 2 && rig.main.transmit.activeProfile == "Alpha")
        rig.push("activeTxProfile", .utf8("Beta")); await MainQueue.drained()
        rig.main.transmit.refresh(); rig.equalizer.refresh(); await ShotWait.laidOut(window)
        try await showProfile(identifier, host: host, window: window)
        let echoed = try #require(SetupTypedEntryTests.element(identifier, in: window))
        #expect(echoed.accessibilityValue == "Beta")
        rig.dirty(); rig.choose("Alpha", equalizer: equalizer, owner: owner); _ = try await nativeAlert(host)
        await rig.event(.stateChanged(.stopped)); try await rig.connect()
        try #require(await ShotWait.until { Self.alert(host) == nil })
        #expect(rig.sent.invokes.count == index + 2 && rig.flow.problem(for: owner) == nil)
    }

    @Test("retiring the old hosted surface cannot dismiss or annotate a replacement question", arguments: [false, true])
    func nativeReplacementSurfaceOwnsItsQuestion(_ equalizer: Bool) async throws {
        let rig = Rig(); try await rig.connect(); _ = rig.main; rig.equalizer.refresh(); rig.dirty()
        let oldOwner = UUID(), replacementOwner = UUID()
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene); window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
        window.windowLevel = .alert + 1
        let oldHost = UIHostingController(rootView: surface(rig, equalizer: equalizer, owner: oldOwner))
        window.rootViewController = oldHost; window.makeKeyAndVisible(); await ShotWait.laidOut(window)
        defer { rig.flow.retire(owner: replacementOwner); window.isHidden = true; window.rootViewController = nil }
        rig.choose("Beta", equalizer: equalizer, owner: oldOwner); _ = try await nativeAlert(oldHost)
        let operation = Task { await rig.flow.saveAndSwitch() }; let save = try await rig.invoke(0)
        rig.flow.retire(owner: oldOwner)
        rig.choose("Beta", equalizer: !equalizer, owner: replacementOwner)
        let identity = try #require(rig.flow.questionIdentity)
        let newHost = UIHostingController(rootView: surface(rig, equalizer: !equalizer, owner: replacementOwner))
        window.rootViewController = newHost; newHost.view.frame = window.bounds
        await ShotWait.laidOut(window)
        let question = try await nativeAlert(newHost)
        #expect(question.title == "Unsaved Profile Changes" && rig.flow.questionIdentity == identity)
        rig.flow.retire(owner: oldOwner)
        await rig.answer(save, accepted: false); await operation.value; await MainQueue.drained()
        #expect(rig.flow.questionIdentity == identity && Self.alert(newHost) === question)
        #expect(rig.flow.problem(for: replacementOwner) == nil && rig.sent.invokes.count == 1)
        rig.flow.cancel(); try #require(await ShotWait.until { Self.alert(newHost) == nil })
    }

    @Test("idle native presenters cannot dismiss a sibling's question; its owner can replace and close it")
    func nativePresentersDismissOnlyTheirOwnQuestions() async throws {
        let rig = Rig(); try await rig.connect()
        let save = try rig.control("save"), choice = try rig.control("activeProfile")
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene); window.frame = scene.coordinateSpace.bounds
        window.windowLevel = .alert + 1
        let host = UIViewController()
        let owning = TxProfileQuestionPresenter.Presenter(), idle = TxProfileQuestionPresenter.Presenter()
        for presenter in [owning, idle] {
            host.addChild(presenter); host.view.addSubview(presenter.view)
            presenter.view.frame = .zero; presenter.didMove(toParent: host)
        }
        window.rootViewController = host; host.view.frame = window.bounds; window.makeKeyAndVisible()
        defer { rig.flow.close(); window.isHidden = true; window.rootViewController = nil }
        await ShotWait.laidOut(window)

        @MainActor func requirePresented(_ alert: UIAlertController) async throws {
            try #require(await ShotWait.until {
                Self.alert(host) === alert && alert.viewIfLoaded?.window === window
                    && !alert.isBeingPresented && !alert.isBeingDismissed
                    && alert.transitionCoordinator == nil && host.transitionCoordinator == nil
            }, "The expected native alert must finish presentation in the shared host")
            await ShotWait.laidOut(window)
            try #require(Self.alert(host) === alert && alert.viewIfLoaded?.window === window && !alert.isBeingDismissed)
        }

        rig.flow.beginSave(save, in: "audio")
        let identity = try #require(rig.flow.questionIdentity)
        owning.configure(flow: rig.flow, question: rig.flow.question, controlId: save.id)
        let naming = try await nativeAlert(host); try await requirePresented(naming)
        #expect(naming.title == "Save TX Profile" && naming.textFields?.first?.text == "Alpha")
        // Exercise the UIKit ancestor alias that allowed the idle row to
        // dismiss its sibling's alert while the same flow question remained.
        try #require(idle.presentedViewController === naming)
        idle.configure(flow: rig.flow, question: nil, controlId: choice.id)
        try await requirePresented(naming)
        idle.retire(); try await requirePresented(naming)
        #expect(rig.flow.questionIdentity == identity && rig.sent.messages.isEmpty)

        await rig.flow.acceptName()
        owning.configure(flow: rig.flow, question: rig.flow.question, controlId: save.id)
        try #require(await ShotWait.until {
            guard let current = Self.alert(host) else { return false }
            return current !== naming && current.title == "Overwrite TX Profile"
        })
        let overwrite = try #require(Self.alert(host)); try await requirePresented(overwrite)
        #expect(overwrite.actions.map(\.title) == ["Yes", "No"] && overwrite.preferredAction?.title == "No")
        owning.retire()
        try #require(await ShotWait.until {
            Self.alert(host) == nil && overwrite.viewIfLoaded?.window == nil
                && !overwrite.isBeingDismissed && host.transitionCoordinator == nil
        })
        #expect(rig.flow.questionIdentity == identity && rig.sent.messages.isEmpty)
    }

    private func surface(_ rig: Rig, equalizer: Bool, owner: UUID) -> AnyView {
        if equalizer {
            return AnyView(ScrollView { TxEqualizerPage(model: rig.equalizer, profileOwner: owner).padding(12) }.preferredColorScheme(.dark))
        }
        return AnyView(TxPanel(transmit: rig.main.transmit, accessories: rig.main.accessories,
            micLevel: rig.main.micLevel, modes: rig.main.modes, meters: nil, width: nil, profileOwner: owner).preferredColorScheme(.dark))
    }
    private func showProfile(_ identifier: String, host: UIViewController, window: UIWindow) async throws {
        guard let scroll = firstScrollView(host.view) else { return }
        let screen = UIAccessibility.convertToScreenCoordinates(window.bounds, in: window)
        for step in 0...10 {
            if let element = SetupTypedEntryTests.element(identifier, in: window),
               !element.accessibilityFrame.isEmpty, screen.intersects(element.accessibilityFrame) { return }
            let extent = max(0, scroll.contentSize.height - scroll.bounds.height + scroll.adjustedContentInset.bottom)
            scroll.setContentOffset(CGPoint(x: 0, y: extent * CGFloat(step) / 10), animated: false)
            await ShotWait.laidOut(window)
        }
        let element = try #require(SetupTypedEntryTests.element(identifier, in: window))
        #expect(!element.accessibilityFrame.isEmpty && screen.intersects(element.accessibilityFrame))
    }
    private func firstScrollView(_ view: UIView) -> UIScrollView? {
        if let scroll = view as? UIScrollView { return scroll }
        for child in view.subviews { if let scroll = firstScrollView(child) { return scroll } }
        return nil
    }
    private func scrollToBottom(_ view: UIView) {
        if let scroll = view as? UIScrollView {
            scroll.setContentOffset(CGPoint(x: 0, y: max(-scroll.adjustedContentInset.top,
                scroll.contentSize.height - scroll.bounds.height + scroll.adjustedContentInset.bottom)), animated: false)
            return
        }
        for child in view.subviews { scrollToBottom(child) }
    }

    private func nativeAlert(_ host: UIViewController) async throws -> UIAlertController {
        try #require(await ShotWait.until { Self.alert(host) != nil })
        return try #require(Self.alert(host))
    }
    private static func alert(_ controller: UIViewController) -> UIAlertController? {
        if let alert = controller as? UIAlertController { return alert }
        if let presented = controller.presentedViewController, let alert = alert(presented) { return alert }
        for child in controller.children { if let alert = alert(child) { return alert } }
        return nil
    }
    private func saveShot(_ name: String, window: UIWindow) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty else { return }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in window.drawHierarchy(in: window.bounds, afterScreenUpdates: true) }
        try #require(image.pngData()).write(to: URL(fileURLWithPath: directory).appendingPathComponent("tx-profiles-" + name + ".png"))
    }
}
