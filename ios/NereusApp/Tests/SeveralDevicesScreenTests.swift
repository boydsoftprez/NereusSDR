// NereusSDR for iOS: several devices on one Core on the phone: another device's slice, the Core's questions and its notices
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-17, R-IOS-30, spec sections 5.8 and 5.9: the real app connected to
/// a fake Core that shares itself with other devices. The cast is the
/// board's: this phone has slice A on 40 m, the MacBook has slice B beside
/// it and has transmit, the iPad has slice C on 20 m. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each screen is written there as a PNG
/// for comparing with pictures 22 and 23.
@Suite("Several devices on screen", .serialized)
@MainActor
struct SeveralDevicesScreenTests {
    static let macBookId = "macbook-device-id"
    /// The id `connectedDevices` gives the desktop hosting the Core:
    /// base64url of `station`, never `station` itself.
    static let hostingId = "c3RhdGlvbg"
    static let iPadId = "ipad-device-id"

    // MARK: The phone and another device's slice

    @Test("the phone declares sessionHolder 1, draws the MacBook's slice read-only and never writes to it")
    func anotherDevicesSlice() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let hello = try #require(station.messages.first { if case .hello = $0 { return true } else { return false } })
        guard case .hello(let ours) = hello else {
            return
        }
        #expect(ours.features == ["deviceAuth": 1, "sessionHolder": 1, "remoteTx": 1,
                                  "setupDescription": 24, "settingsHygiene": 2, "radioAntennaRows": 1, "vax": 1, "txEqCurve": 2,
                                  "diversityPattern": 1, "diversityControl": 1, "logCategoryList": 1, "radioModels": 1, "band2m": 1, "coreAddresses": 1,
                                  "radeStatus": 1, "stationTciSettings": 1, "txInhibitReason": 1, "alexLpf": 1, "mediaDirect": 1, "sliceAccess": 3, "cfcProfile": 1, "paProfiles": 1,
                                  "levelCalibration": 1, "adcAttenuators": 1, "rx2Attenuator": 1, "radioMic": 1,
                                  "rxFilterLowPass": 1, "radeReason": 1, "audioQuality": 1])
        // The capabilities that declare it were delivered last; wait for the app to read them.
        #expect(await settle(seconds: 30) { SeveralDevices.available(in: model.mirror) })

        let foreign = model.main.foreign
        #expect(await settle(seconds: 30) { foreign.entries.count == 1 })
        let entry = try #require(foreign.entries.first)
        #expect(entry.slice.letter == "B")
        #expect(entry.slice.labelName == "MacBook")
        #expect(entry.slice.txSlice)
        #expect(entry.modeLabel == "LSB")
        #expect(ForeignSliceLabel.Note.doing(entry) == "has transmit")
        #expect(ForeignSliceLabel.Note.fine(entry) == "That slice belongs to MacBook Pro. It can be changed only there.")
        // It is not among this phone's slices, so nothing tunes it.
        #expect(!model.main.slices.entries.contains { $0.id == 1 })

        // Drawn as another device's: first, and dashed and unshaded.
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        try await shoot("mc-band", model: model) {
            foreign.openNote = 1
        }
        let markers = band.markers
        let theirs = try #require(markers.first { $0.foreign })
        #expect(theirs.sliceId == 1 && theirs.centerHz == 7_249_000 && !theirs.showsPassband)
        #expect(markers.firstIndex { $0.foreign } == 0)

        // A tap on the band tunes this phone's slice A only.
        model.main.slices.tap(to: 7_240_000)
        let write = await station.waitForMessage { message in
            if case .propertyWrite = message { return true } else { return false }
        }
        #expect(write != nil)
        #expect(!station.messages.contains { message in
            if case .propertyWrite(let write) = message { return write.key.hasPrefix("marker:") || write.key == "slice:1" }
            return false
        })

        // Away: its label greys with "away", and the note says for how long.
        await station.deliver(.delta(LinkMessage.Delta(key: "marker:1", properties: [
            .init(ordinal: 5, name: "ownerAway", value: .bool(true)),
        ])))
        await station.deliver(Self.connectedDevices(macBookAwayFor: 40))
        #expect(await settle(seconds: 30) { foreign.entries.first?.slice.away == true })
        #expect(await settle(seconds: 30) { foreign.entries.first?.awayForSeconds == 40 })
        #expect(ForeignSliceLabel.Note.doing(try #require(foreign.entries.first)) == "away for 40 seconds, holding transmit")
        try await shoot("mc-away", model: model) {
            foreign.openNote = 1
        }
        await model.disconnect()
    }

    /// The label and its note name the holder in the Core's words, and the
    /// note says what the Core says to a change from this app
    /// (StationServer::sliceHolderWords and ownedElsewhereReason,
    /// src/core/session/StationServer.cpp:9059-9093 at c13abe564). Each
    /// case is drawn in light and dark.
    @Test("another device's slice is named in the Core's words, light and dark")
    func holderInTheCoresWords() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let foreign = model.main.foreign
        #expect(await settle(seconds: 30) { foreign.entries.count == 1 })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let cases: [(shot: String, name: String, short: String, kind: String, label: String, title: String,
                     fine: String)] = [
            ("owner-named", "MacBook Pro", "MacBook", "computer", "MacBook", "MacBook Pro\u{2019}s slice B",
             "That slice belongs to MacBook Pro. It can be changed only there."),
            ("owner-core", "", "", "station", "the Core", "The Core\u{2019}s slice B",
             "That slice belongs to the Core. It can be changed only there."),
            ("owner-phone", "", "", "phone", "a phone", "A phone\u{2019}s slice B",
             "That slice belongs to a phone. It can be changed only there."),
            ("owner-tablet", "", "", "tablet", "a tablet", "A tablet\u{2019}s slice B",
             "That slice belongs to a tablet. It can be changed only there."),
            ("owner-computer", "", "", "computer", "a computer", "A computer\u{2019}s slice B",
             "That slice belongs to a computer. It can be changed only there."),
            ("owner-another", "", "", "", "another device", "Another device\u{2019}s slice B",
             "That slice belongs to another device. It can be changed only there."),
        ]
        for words in cases {
            await station.deliver(.delta(LinkMessage.Delta(key: "marker:1", properties: [
                .init(ordinal: 2, name: "ownerName", value: .utf8(words.name)),
                .init(ordinal: 3, name: "ownerShortName", value: .utf8(words.short)),
                .init(ordinal: 4, name: "ownerKind", value: .utf8(words.kind)),
            ])))
            #expect(await settle(seconds: 30) { foreign.entries.first?.slice.labelName == words.label }, "\(words.shot)")
            let entry = try #require(foreign.entries.first)
            #expect(ForeignSliceLabel.Note.title(entry) == words.title)
            #expect(ForeignSliceLabel.Note.fine(entry) == words.fine)
            for scheme in [ColorScheme.light, .dark] {
                try await shoot("c13abe-\(words.shot)-\(scheme == .light ? "light" : "dark")", model: model,
                                scheme: scheme) {
                    foreign.openNote = 1
                }
            }
        }
        await model.disconnect()
    }

    // MARK: The Core's questions

    @Test("every receiver in use lists who has each; Confirm sends the picked receiver's choice and waits for the Core")
    func takeAReceiver() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let devices = model.devices
        await station.deliver(Self.takeReceiver(id: 11))
        #expect(await settle(seconds: 30) { devices.question?.id == 11 })
        let question = try #require(devices.question)
        #expect(question.kind == .takeReceiver)
        #expect(question.firstTakeable?.choice == 0)
        #expect(question.choices.map(TakeReceiverSheet.title) == ["Receiver 2 \u{00B7} iPad Pro",
                                                                  "Receiver 3 \u{00B7} MacBook Pro"])
        #expect(model.main.slices.sendsOnlyFinalValue)
        try await shoot("mc-rx", model: model)

        // Confirm on the MacBook's receiver: confirm.proceed {id, choice}.
        let proceeding = Task { await devices.proceed(choice: 1) }
        let proceed = await station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "confirm.proceed" } else { return false }
        }
        guard case .commandInvoke(let invoke)? = proceed else {
            Issue.record("no confirm.proceed")
            return
        }
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "id", value: .i64(11)),
                                LinkMessage.PropertyEntry(name: "choice", value: .i64(1))])
        #expect(devices.answering == .proceeding)
        #expect(devices.question != nil)
        // The Core refuses it: its words stay on the sheet.
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "confirm.proceed", id: invoke.id, accepted: false,
            reason: "That question has expired. Make the change again.", affected: [])))
        await proceeding.value
        #expect(devices.answering == .refused("That question has expired. Make the change again."))
        try await shoot("mc-rx-refused", model: model)
        devices.closeQuestion()
        #expect(await settle(seconds: 30) { !model.main.slices.sendsOnlyFinalValue })
        await model.disconnect()
    }

    @Test("moving a shared receiver names the MacBook and its slice; Stay on 40 m sends confirm.cancel")
    func moveASharedReceiver() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let devices = model.devices
        await station.deliver(Self.panMove(id: 12, effect: "closes"))
        #expect(await settle(seconds: 30) { devices.question?.id == 12 })
        try await shoot("mc-move", model: model)
        devices.cancel()
        let cancel = await station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "confirm.cancel" } else { return false }
        }
        guard case .commandInvoke(let invoke)? = cancel else {
            Issue.record("no confirm.cancel")
            return
        }
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "id", value: .i64(12))])
        #expect(devices.question == nil)
        await model.disconnect()
    }

    @Test("a shared change shows the setting from and to and whose slice it reaches; the readback lands on Confirm")
    func sharedChange() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let devices = model.devices
        // The receive antenna relay is a change the Core asks about first
        // (ruling 7.1a); the attenuator and preamp now apply at once.
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "alexAntennas", className: "AlexAntennaFacade",
                                                                     properties: [
            .init(ordinal: 2, name: "useTxAntennaForRx", value: .bool(false)),
        ])))
        await station.deliver(Self.sharedSetting(id: 13))
        #expect(await settle(seconds: 30) { devices.question?.id == 13 })
        try await shoot("mc-set", model: model)
        let proceeding = Task { await devices.proceed() }
        let proceed = await station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "confirm.proceed" } else { return false }
        }
        guard case .commandInvoke(let invoke)? = proceed else {
            Issue.record("no confirm.proceed")
            return
        }
        #expect(invoke.args.last == LinkMessage.PropertyEntry(name: "choice", value: .i64(-1)))
        #expect(model.mirror.object("alexAntennas")?["useTxAntennaForRx"] == .bool(false))
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "confirm.proceed", id: invoke.id, accepted: true, reason: "", affected: ["alexAntennas"], values: [
                .init(ordinal: 0, name: "objectKey", value: .utf8("alexAntennas")),
                .init(ordinal: 2, name: "useTxAntennaForRx", value: .bool(true)),
            ])))
        await proceeding.value
        #expect(model.mirror.object("alexAntennas")?["useTxAntennaForRx"] == .bool(true))
        #expect(devices.question == nil)
        await model.disconnect()
    }

    @Test("a preamp change applies at once: no question, the Core's answer lands as applied, and the others are told")
    func preampAppliesAtOnce() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let devices = model.devices
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "stepAtt", className: "StepAttenuator",
                                                                     properties: [
            .init(ordinal: 2, name: "preampMode", value: .i64(0)),
        ])))
        #expect(await settle(seconds: 30) { model.mirror.object("stepAtt")?["preampMode"] == .int(0) })
        let writing = Task { await model.mirror.write("stepAtt", property: "preampMode", value: .int(1)) }
        let sent = await station.waitForMessage { message in
            if case .propertyWrite(let write) = message { return write.key == "stepAtt" } else { return false }
        }
        guard case .propertyWrite(let write)? = sent, let writeId = write.writeId else {
            Issue.record("no stepAtt write")
            return
        }
        // The Core takes it at once, as the shared-setting-notice session does: no confirm.request.
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "stepAtt", writeId: writeId, results: [
            .init(property: "preampMode", accepted: true, reason: "",
                  value: .init(ordinal: 2, name: "preampMode", value: .i64(1))),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "stepAtt", properties: [
            .init(ordinal: 2, name: "preampMode", value: .i64(1)),
        ])))
        let outcome = await writing.value
        #expect(outcome.accepted && outcome.reason != SeveralDevices.waitingReason)
        #expect(await settle(seconds: 30) { model.mirror.object("stepAtt")?["preampMode"] == .int(1) })
        #expect(devices.question == nil && devices.answering == .idle)
        // Another device's preamp change reaches this phone only as a notice.
        await station.deliver(Self.notice(id: 30, kind: "settingChanged", reason:
            "MacBook Pro changed Preamp, ADC 1 from Off to On.", by: true, secondsAgo: 0,
                                          change: ("Preamp, ADC 1", "Off", "On")))
        #expect(await settle(seconds: 30) { devices.notices.last?.id == 30 })
        #expect(devices.question == nil)
        #expect(NoticeBanner.text(try #require(devices.notices.last)).hasPrefix(
            "MacBook Pro changed Preamp, ADC 1 from Off to On."))
        await model.disconnect()
    }

    // MARK: The Core's notices

    @Test("each notice shows the Core's words and, for what another device did, when by this phone's clock")
    func notices() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let devices = model.devices

        // Told afterwards: another device changed a shared setting.
        await station.deliver(Self.notice(id: 20, kind: "settingChanged", reason:
            "MacBook Pro changed Attenuator, ADC 1 from 0 dB to 20 dB.", by: true, secondsAgo: 60,
                                          change: ("Attenuator, ADC 1", "0 dB", "20 dB")))
        #expect(await settle(seconds: 30) { devices.notices.count == 1 })
        let told = try #require(devices.notices.last)
        let at = SeveralDevicesWords.timeOfDay(told.happened)
        #expect(NoticeBanner.text(told) == "MacBook Pro changed Attenuator, ADC 1 from 0 dB to 20 dB. At \(at).")
        #expect(!told.notice.takeBack)
        try await shoot("mc-told", model: model)
        devices.dismiss(20)

        for (id, kind, reason, name) in [
            (21, "antennaKept", "The antenna stays on ANT1 while MacBook listens on it.", "mc-antenna"),
            (22, "graceEnded", "You were away for more than 3 minutes. Your slices are back.", "mc-back-late"),
            (23, "slicesNotRestored", "1 of your slices could not be restored: all the radio's receivers are in use.",
             "mc-slice-lost"),
        ] as [(Int64, String, String, String)] {
            await station.deliver(Self.notice(id: id, kind: kind, reason: reason, by: false, secondsAgo: 0))
            #expect(await settle(seconds: 30) { devices.notices.last?.id == id })
            #expect(NoticeBanner.text(try #require(devices.notices.last)) == reason)
            try await shoot(name, model: model)
            devices.dismiss(id)
        }

        // Your receiver taken: RECEIVER TAKEN over the band, with Take it back.
        await station.deliver(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:0", className: "SliceModel")))
        await station.deliver(Self.notice(id: 24, kind: "receiverTaken", reason:
            "iPad took the receiver your slice A was on.", by: true, secondsAgo: 30, takeBack: true))
        #expect(await settle(seconds: 30) { devices.notices.last?.id == 24 && model.main.slices.entries.isEmpty })
        let taken = try #require(devices.notices.last)
        #expect(ReceiverTakenOverlay.body(taken) == "Slice A closed on this phone at "
            + SeveralDevicesWords.timeOfDay(taken.happened)
            + ". Its frequency and settings are kept, so taking the receiver back puts it where it was.")
        try await shoot("mc-rxgone", model: model)
        let takingBack = Task { await devices.takeBack(24) }
        let takeBack = await station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "notice.takeBack" } else { return false }
        }
        guard case .commandInvoke(let invoke)? = takeBack else {
            Issue.record("no notice.takeBack")
            return
        }
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "id", value: .i64(24))])
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "notice.takeBack", id: invoke.id, accepted: false, reason: SeveralDevices.waitingReason,
            affected: [], values: [.init(name: "phase", value: .utf8("needsConfirmation"))])))
        await takingBack.value
        #expect(devices.notices.last?.takeBackRefusal == nil)
        await model.disconnect()
    }

    @Test("the Core's refusal of this phone's own tune shows over the band in its words, and the slice stays put")
    func ownSliceWriteRefused() async throws {
        let (model, station, suite) = try await connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        let before = try #require(slices.active?.slice.frequencyHz)
        slices.tap(to: before + 3_000)
        let sent = await station.waitForMessage { message in
            if case .propertyWrite(let write) = message {
                return write.key == "slice:0" && write.properties.first?.name == "frequency"
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent, let writeId = write.writeId else {
            Issue.record("no frequency write")
            return
        }
        let words = "That frequency is held by MacBook Pro while it transmits. Try again when it stops."
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: writeId, results: [
            .init(property: "frequency", accepted: false, reason: words, value: nil),
        ])))
        #expect(await settle(seconds: 30) { slices.refusal?.text == words })
        #expect(slices.active?.slice.frequencyHz == before)
        try await shoot("mc-own-write-refused", model: model)
        slices.dismissRefusal()
        #expect(slices.refusal == nil)
        await model.disconnect()
    }

    @Test("Your Cores says how many devices a Core has on it, and nothing without the count")
    func yourCoresCount() {
        #expect(YourStationsScreen.devicesOn(4) == "4 devices on it")
        #expect(YourStationsScreen.devicesOn(1) == "1 device on it")
        #expect(YourStationsScreen.devicesOn(0) == nil)
        #expect(YourStationsScreen.devicesOn(nil) == nil)
    }

    @Test("Your Cores says Waiting for a radio after the address and count while the record's radio is waiting")
    func yourCoresWaitingForARadio() throws {
        func found(_ radio: String?, devices: String? = "2") throws -> FoundStation {
            var txt = ["v": "1", "id": "AAAAAAAAAAAAAAAAAAAAAA", "claimed": "1", "pair": "code", "name": "KG4VCF/shack"]
            txt["devices"] = devices
            txt["radio"] = radio
            return try #require(FoundStation.parse(txt: txt, instanceName: "KG4VCF/shack"))
        }
        let address = "192.168.1.40"
        #expect(YourStationsScreen.waitingForRadioText == "Waiting for a radio")
        #expect(YourStationsScreen.detail(address, found: try found("waiting"))
                == "192.168.1.40 \u{00B7} 2 devices on it \u{00B7} Waiting for a radio")
        // Connected and offline add nothing: the record names no radio.
        #expect(YourStationsScreen.detail(address, found: try found("connected"))
                == "192.168.1.40 \u{00B7} 2 devices on it")
        #expect(YourStationsScreen.detail(address, found: try found("offline"))
                == "192.168.1.40 \u{00B7} 2 devices on it")
        // An unknown value, or none (an older Core), adds nothing either.
        #expect(try found("banana").radio == nil)
        #expect(YourStationsScreen.detail(address, found: try found("banana")) == "192.168.1.40 \u{00B7} 2 devices on it")
        #expect(YourStationsScreen.detail(address, found: try found(nil)) == "192.168.1.40 \u{00B7} 2 devices on it")
        // Without a count, or not found on this network at all.
        #expect(YourStationsScreen.detail(address, found: try found("waiting", devices: nil))
                == "192.168.1.40 \u{00B7} Waiting for a radio")
        #expect(YourStationsScreen.detail(address, found: nil) == "192.168.1.40")
        #expect(YourStationsScreen.detail("192.168.1.40 \u{00B7} pair again", found: try found("waiting"))
                == "192.168.1.40 \u{00B7} pair again \u{00B7} 2 devices on it \u{00B7} Waiting for a radio")
    }

    @Test("back within 3 minutes on a Core shared with other devices says it was the phone's own session")
    func backOnAirWords() {
        #expect(LinkLostBanner.ownSessionText == "It was this phone\u{2019}s own session, so it didn\u{2019}t ask.")
    }

    // MARK: Taking control of a slice

    /// This phone's device id, as the Core's `access:<id>` names it.
    static let phoneId = "phone-id"
    static let transmittingWords = "Slice B is transmitting. Take control once it stops."
    static let firstKeyWords = "You took this slice from another device. Choose it for transmit first with its TX button."

    /// The board's take-over cast (R-IOS-42): this phone has slice A, which
    /// has transmit, and listens to slice B, which the MacBook controls.
    /// Slice B's flag is the open one.
    func listening(version: Int64 = 2, more: FakeStation.Additions = []) async throws -> (AppModel, FakeStation, String) {
        let (model, station, suite) = try await connected(additions: FakeStation.Additions.all.union(.remoteTx).union(more))
        // Remote transmit, with no key on.
        model.main.transmit.thisDeviceId = Self.phoneId
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)), .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle(seconds: 30) { model.main.transmit.permitted })
        var capabilities = model.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        capabilities.append(LinkMessage.PropertyEntry(name: SliceAccess.capability, value: .i64(version)))
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        // In the mirror before anything rebuilds the set from what it holds.
        #expect(await settle(seconds: 30) {
            model.mirror.capabilityVersion(SliceAccess.capability) == version && SeveralDevices.available(in: model.mirror)
        })
        await station.deliver(.objectDestroy(LinkMessage.ObjectDestroy(key: "marker:1", className: "SliceMarker")))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(name: "txSlice", value: .bool(true)), .init(name: "active", value: .bool(false)),
        ])))
        await station.deliver(BandFlagShotTests.slice(1, active: true))
        await station.deliver(Self.access(0, incarnation: 5, controller: Self.phoneId, revision: 3))
        await station.deliver(Self.access(1, incarnation: 7, controller: Self.macBookId, revision: 12))
        model.main.slices.thisDeviceId = Self.phoneId
        let slices = model.main.slices
        #expect(await settle(seconds: 30) {
            slices.entries.count == 2 && slices.entries.first { $0.id == 1 }?.listening == true
                && slices.activeSliceId == 1
        })
        return (model, station, suite)
    }

    static func access(_ sliceId: Int64, incarnation: Int64, controller: String, revision: Int64) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "access:\(sliceId)", className: SliceAccess.accessClass, properties: [
            .init(ordinal: 0, name: "sliceId", value: .i64(sliceId)),
            .init(ordinal: 1, name: "incarnation", value: .i64(incarnation)),
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(controller)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
            .init(ordinal: 4, name: "listenerDeviceIds", value: .utf8(LinkJSON.array([.string(controller),
                                                                                        .string(phoneId)]).compactText)),
            .init(ordinal: 5, name: "activeRxDeviceIds", value: .utf8("[]")),
            .init(ordinal: 6, name: "txSelected", value: .bool(sliceId == 0)),
            .init(ordinal: 7, name: "onAir", value: .bool(false)),
        ]))
    }

    /// The slice's `onAir`, as the Core sends it while the slice transmits.
    static func onAir(_ sliceId: Int64, _ onAir: Bool) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "access:\(sliceId)", properties: [
            .init(ordinal: 7, name: "onAir", value: .bool(onAir)),
        ]))
    }

    static func controller(_ sliceId: Int64, _ controller: String, revision: Int64) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "access:\(sliceId)", properties: [
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(controller)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
        ]))
    }

    /// The Core's `controlTaken` at `sliceAccessVersion` 2, for slice B.
    static func controlTaken(id: Int64, takeBack: Bool, revision: Int64) -> LinkMessage {
        .notice(LinkMessage.Notice(
            id: id, kind: "controlTaken", reason: "MacBook Pro took control of slice B. You are still listening.",
            secondsAgo: 0, takeBack: takeBack, byDeviceId: macBookId, byName: "MacBook Pro", byShortName: "MacBook",
            byKind: "computer", bySource: "device",
            slices: [.object(["sliceId": .number(1), "letter": .string("B"), "frequencyHz": .number(7_249_000),
                              "mode": .number(0), "band": .number(3), "incarnation": .number(7),
                              "controlRevision": .number(Double(revision))])]))
    }

    private func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == verb {
                return invoke
            }
            return nil
        }
    }

    private func answer(_ station: FakeStation, _ verb: String, count: Int, accepted: Bool, reason: String = "",
                        values: [LinkMessage.PropertyEntry]? = nil) async throws {
        #expect(await settle(seconds: 30) { invokes(station, verb).count == count })
        let invoke = try #require(invokes(station, verb).last)
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: verb, id: invoke.id, accepted: accepted, reason: reason,
            affected: accepted ? ["slice:1", "access:1"] : [], values: values)))
    }

    @Test("a listened slice names who controls it, dims what changes it and never tunes; nobody reads as the Core words it")
    func listenedSlice() async throws {
        let (model, station, suite) = try await listening()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        let b = try #require(slices.entries.first { $0.id == 1 })
        #expect(b.control == .listening(ownerLine: "Slice B is controlled by MacBook Pro. Take control to change it."))
        #expect(!slices.isTunable(b))
        #expect(slices.entries.first { $0.id == 0 }?.control == .here)
        try await shoot("listening", model: model)
        try await shoot("listening-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-listening", model: model, sideways: true)
        try await shoot("landscape-large-type", model: model, sideways: true, typeSize: .accessibility1)

        // Frequency, mode and filter show as the Core sends them, and nothing tunes it.
        #expect(b.slice.frequencyHz == 7_249_000)
        slices.tap(to: 7_250_000)
        slices.drag(sliceId: 1, to: 7_251_000)
        slices.finishDrag()
        try? await Task.sleep(for: .milliseconds(200))
        #expect(!station.messages.contains { message in
            if case .propertyWrite(let write) = message { return write.key == "slice:1" }
            return false
        })

        // Nobody controls it.
        await station.deliver(Self.controller(1, "", revision: 13))
        #expect(await settle(seconds: 30) {
            slices.entries.first { $0.id == 1 }?.control
                == .listening(ownerLine: "Nobody controls slice B. Take control to change it.")
        })
        try await shoot("nobody", model: model)
        await model.disconnect()
    }

    @Test("a spot or FreeDV station tapped on a listened slice says who controls it, over the band, and tunes nothing")
    func spotOnAListenedSliceSaysWho() async throws {
        let (model, station, suite) = try await listening(more: .spots)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let owner = "Slice B is controlled by MacBook Pro. Take control to change it."
        let slices = model.main.slices
        #expect(slices.active?.locked == false)
        #expect(model.spots.tuneReason == owner)
        #expect(model.freedv.sliceReason == owner)
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle(seconds: 30) { !spots.spots.isEmpty })
        let spot = try #require(spots.spots.first)
        let before = slices.active?.slice.frequencyHz
        // From the Spot List, which shows the owner line as its own note, nothing is posted over the band.
        spots.tune(spot, onBand: false)
        #expect(slices.refusal == nil)
        #expect(slices.active?.slice.frequencyHz == before)
        spots.tune(spot, onBand: true)
        #expect(slices.refusal?.text == owner)
        await model.disconnect()
    }

    @Test("Level Cal never starts on a slice another device controls: Start is greyed with the slice's owner line")
    func levelCalNeverOnAListenedSlice() async throws {
        let (model, station, suite) = try await listening()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        var capabilities = model.mirror.capabilities
        capabilities["radioHardwareVersion"] = .int(12)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
        })))
        let levelCal = model.levelCal
        let owner = "Slice B is controlled by MacBook Pro. Take control to change it."
        #expect(await settle(seconds: 30) { levelCal.offered && levelCal.sliceLine == "Calibrates slice B." })
        #expect(!levelCal.startEnabled && levelCal.resetEnabled)
        #expect(levelCal.reasons == [owner, LevelCalModel.nothingToStopText])
        levelCal.start()
        levelCal.confirmStart()
        try? await Task.sleep(for: .milliseconds(200))
        #expect(levelCal.alert == nil)
        #expect(invokes(station, LevelCalModel.startVerb).isEmpty)
        // Slice A, this phone's own, may be calibrated.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(name: "active", value: .bool(false)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(name: "active", value: .bool(true)),
        ])))
        #expect(await settle(seconds: 30) { levelCal.startEnabled && levelCal.sliceLine == "Calibrates slice A." })
        #expect(levelCal.reasons == [LevelCalModel.nothingToStopText])

        // Started here on slice A: the band says which slice is being calibrated.
        levelCal.start()
        levelCal.confirmStart()
        #expect(await settle(seconds: 30) { invokes(station, LevelCalModel.startVerb).count == 1 })
        #expect(invokes(station, LevelCalModel.startVerb).first?.args.last
            == LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0)))
        try await answer(station, LevelCalModel.startVerb, count: 1, accepted: true, reason: "")
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 31, name: "levelCalRunning", value: .bool(true)),
            .init(ordinal: 32, name: "levelCalPercent", value: .i64(42)),
        ])))
        #expect(await settle(seconds: 30) { levelCal.bandLine == "Level calibration is running on slice A." })
        try await shoot("levelcal-band-line", model: model)
        try await shoot("levelcal-band-line-light", model: model, scheme: .light)
        try await shoot("levelcal-band-line-large-type", model: model, typeSize: .accessibility2)
        try await shoot("levelcal-band-line-landscape", model: model, sideways: true)
        await model.disconnect()
    }

    @Test("Take control sends the slice's incarnation and revision; refused shows the Core's words, accepted keeps every setting and transmit")
    func takeControl() async throws {
        let (model, station, suite) = try await listening()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices

        // Refused: the Core's words over the band, and the button stays.
        slices.takeControl(1)
        #expect(slices.taking == [1])
        #expect(await settle(seconds: 30) { invokes(station, SliceAccess.takeControlVerb).count == 1 })
        #expect(invokes(station, SliceAccess.takeControlVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "incarnation", value: .i64(7)),
            LinkMessage.PropertyEntry(name: "controlRevision", value: .i64(12)),
        ])
        try await shoot("taking", model: model)
        try await answer(station, SliceAccess.takeControlVerb, count: 1, accepted: false,
                         reason: Self.transmittingWords)
        #expect(await settle(seconds: 30) { slices.refusal?.text == Self.transmittingWords && slices.taking.isEmpty })
        #expect(slices.refusal?.takeOver == true)
        #expect(slices.entries.first { $0.id == 1 }?.listening == true)
        try await shoot("refused", model: model)
        slices.dismissRefusal()

        // The Core's hosted-slice words, as sent.
        let hosted = "Slice A is run by the Core itself, so control of it cannot pass to this device."
        slices.takeControl(1)
        try await answer(station, SliceAccess.takeControlVerb, count: 2, accepted: false, reason: hosted)
        #expect(await settle(seconds: 30) { slices.refusal?.text == hosted })
        slices.dismissRefusal()

        // Accepted: the slice is this phone's, as it was; transmit stays on A.
        slices.takeControl(1)
        try await answer(station, SliceAccess.takeControlVerb, count: 3, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        await station.deliver(Self.controller(1, Self.phoneId, revision: 13))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.control == .here })
        let b = try #require(slices.entries.first { $0.id == 1 })
        #expect(b.slice.frequencyHz == 7_249_000 && b.mode == 0)
        #expect(slices.takenHere == [1])
        #expect(slices.refusal == nil)
        #expect(slices.entries.first { $0.id == 0 }?.slice.txSlice == true)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
        #expect(!station.messages.contains { message in
            if case .propertyWrite(let write) = message { return write.properties.contains { $0.name == "txSlice" } }
            return false
        })
        try await shoot("taken", model: model)

        // The first key is the Core's to refuse: its words show as sent and
        // the taken slice's TX button is ringed.
        let transmit = model.main.transmit
        station.refuseNext("tx.key", reason: Self.firstKeyWords, code: "chooseTransmitSlice")
        transmit.tapPtt()
        let refusal = TxRefusalInfo(reason: Self.firstKeyWords, code: "chooseTransmitSlice")
        #expect(await settle(seconds: 30) { transmit.ptt.state == .refused(refusal) })
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == .refusal(refusal))
        #expect(BandSlicesModel.refusedForTransmitChoice(transmit.ptt))
        #expect(TxNoticeCard.takeOver(.refusal(refusal)))
        #expect(!station.keyed)
        try await shoot("first-key", model: model)
        try await shoot("first-key-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-first-key", model: model, sideways: true)

        // The TX button: tx.setTxSlice, only for this phone's slice that
        // has not got transmit; A already has it, so it sends nothing.
        slices.selectForTransmit(0)
        slices.selectForTransmit(1)
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: true)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1)),
        ])
        #expect(await settle(seconds: 30) { slices.takenHere.isEmpty })
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).count == 1)
        await model.disconnect()
    }

    @Test("the device that lost a slice is told with Take back; a refusal while it transmits keeps the card, a final one closes it")
    func oldOwner() async throws {
        let (model, station, suite) = try await listening()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        let devices = model.devices
        // Slice B was this phone's; the MacBook took it at revision 12.
        await station.deliver(Self.controlTaken(id: 30, takeBack: true, revision: 12))
        #expect(await settle(seconds: 30) { devices.notices.last?.id == 30 })
        let told = try #require(devices.notices.last)
        #expect(NoticeBanner.text(told) == "MacBook Pro took control of slice B. You are still listening.")
        #expect(devices.takeBackUnavailableReason(told) == nil)
        #expect(NoticeBanner.takeBackTitle(control: true, taking: false) == "Take back")
        #expect(slices.entries.first { $0.id == 1 }?.listening == true)
        try await shoot("old-owner", model: model)
        try await shoot("old-owner-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-old-owner", model: model, sideways: true)

        // Refused while slice B transmits: the card stays with the Core's words.
        let first = Task { await devices.takeBack(30) }
        try await answer(station, SeveralDevices.takeBackVerb, count: 1, accepted: false,
                         reason: Self.transmittingWords)
        await first.value
        #expect(devices.notices.last?.takeBackRefusal == Self.transmittingWords)
        try await shoot("old-owner-refused", model: model)

        // Tried again and accepted: the card closes and the slice is this phone's.
        let second = Task { await devices.takeBack(30) }
        try await answer(station, SeveralDevices.takeBackVerb, count: 2, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        await second.value
        await station.deliver(Self.controller(1, Self.phoneId, revision: 13))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.control == .here })
        #expect(devices.notices.isEmpty)
        #expect(slices.takenHere == [1])
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)

        // Taken again, and the Core cannot give it back: Take back is greyed with its words.
        await station.deliver(Self.controller(1, Self.macBookId, revision: 14))
        await station.deliver(Self.controlTaken(id: 31, takeBack: false, revision: 14))
        #expect(await settle(seconds: 30) { devices.notices.last?.id == 31 && slices.takenHere.isEmpty })
        #expect(devices.takeBackUnavailableReason(try #require(devices.notices.last))
                == "That can no longer be taken back.")
        try await shoot("old-owner-no-take-back", model: model)
        devices.dismiss(31)

        // A final refusal closes the card and shows the Core's words.
        await station.deliver(Self.controlTaken(id: 32, takeBack: true, revision: 14))
        #expect(await settle(seconds: 30) { devices.notices.last?.id == 32 })
        await station.deliver(Self.controller(1, "ipad-device-id", revision: 15))
        let third = Task { await devices.takeBack(32) }
        let moved = "Someone else changed who controls slice B. Look again and try once more."
        try await answer(station, SeveralDevices.takeBackVerb, count: 3, accepted: false, reason: moved)
        await third.value
        #expect(devices.notices.isEmpty)
        #expect(devices.endedTakeBack?.text == moved)
        #expect(slices.refusal == nil)
        try await shoot("old-owner-ended", model: model)
        await model.disconnect()
    }

    @Test("the flag's VoiceOver transmit action runs the guarded choice: nothing for a listened slice or the transmit slice")
    func transmitActionIsGuarded() async throws {
        let (model, station, suite) = try await listening()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        func action(_ id: Int) throws -> (name: String, run: () -> Void)? {
            let entry = try #require(slices.entries.first { $0.id == id })
            return VfoFlagView.transmitAction(entry, selectForTransmit: BandGestureLayer.selectForTransmit(slices,
                                                                                                          sliceId: id))
        }
        // Slice B, which the MacBook controls: the action is there and the choice refuses it.
        let listened = try #require(try action(1))
        #expect(listened.name == "Make slice B the transmit slice")
        listened.run()
        // Slice A already has transmit: no action.
        #expect(try action(0) == nil)
        try? await Task.sleep(for: .milliseconds(300))
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)

        // Taken: the same action now sends tx.setTxSlice for B, once.
        slices.takeControl(1)
        try await answer(station, SliceAccess.takeControlVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        await station.deliver(Self.controller(1, Self.phoneId, revision: 13))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.control == .here })
        try #require(try action(1)).run()
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: true)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1)),
        ])
        await model.disconnect()
    }

    @Test("sideways, the notice keeps clear of a listened slice's open flag; upright it goes below a flag it would cover")
    func noticeKeepsClearOfTakeControl() {
        // No flag in the way: where it always sat.
        let plain = SeveralDevicesLayers.noticePlace(bandWidth: 874, waterfallTop: 200, sideways: true,
                                                     leadingInset: 62, avoid: nil)
        #expect(plain.width == 546 && plain.x == 164 && plain.y == 214)
        // A flag reaching into the waterfall on the right: beside it on the left, clear of PTT.
        let flag = CGRect(x: 498, y: 0, width: 240, height: 300)
        let beside = SeveralDevicesLayers.noticePlace(bandWidth: 874, waterfallTop: 200, sideways: true,
                                                      leadingInset: 62, avoid: flag)
        #expect(beside.x + beside.width <= flag.minX)
        #expect(beside.x >= 62 + PttButton.inset + PttButton.diameter)
        #expect(beside.y == 214)
        // Upright, below the flag.
        let upright = SeveralDevicesLayers.noticePlace(bandWidth: 402, waterfallTop: 300, sideways: false,
                                                       leadingInset: 0, avoid: CGRect(x: 40, y: 0, width: 240, height: 340))
        #expect(upright.y == 348 && upright.x == 10 && upright.width == 382)
        // A flag that ends above the waterfall changes nothing.
        let above = SeveralDevicesLayers.noticePlace(bandWidth: 402, waterfallTop: 300, sideways: false,
                                                     leadingInset: 0, avoid: CGRect(x: 40, y: 0, width: 240, height: 290))
        #expect(above.y == 308)
    }

    // MARK: Taking any slice (sliceAccessVersion 3)

    static let coreItselfWords = "Slice B is run by the Core itself, so control of it cannot pass to this device."
    static let onAirWords = "Slice B is transmitting. Take control once it stops."

    @Test("at 3, Take control is live on the Core's own slice of a headless Core, greyed only while it is on the air")
    func coreSliceAtThree() async throws {
        let (model, station, suite) = try await listening(version: 3)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        #expect(SliceAccess.version(in: model.mirror) == 3)
        // Another device's slice: live, as before.
        #expect(slices.entries.first { $0.id == 1 }?.takeRefusal == nil)
        // The Core's own slice, nobody at its desktop.
        await station.deliver(Self.controller(1, SliceAccess.stationDeviceId, revision: 13))
        #expect(await settle(seconds: 30) {
            slices.entries.first { $0.id == 1 }?.control
                == .listening(ownerLine: "Slice B is controlled by the Core. Take control to change it.")
        })
        #expect(slices.entries.first { $0.id == 1 }?.takeRefusal == nil)
        try await shoot("core-slice-live", model: model)
        try await shoot("core-slice-live-light", model: model, scheme: .light)
        try await shoot("core-slice-live-large-type", model: model, typeSize: .accessibility1)

        // On the air: greyed with the Core's words, and a tap sends nothing.
        await station.deliver(Self.onAir(1, true))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.takeRefusal == Self.onAirWords })
        slices.takeControl(1)
        #expect(slices.taking.isEmpty)
        try await shoot("core-slice-on-air", model: model)
        try await shoot("core-slice-on-air-light", model: model, scheme: .light)
        try await shoot("core-slice-on-air-large-type", model: model, typeSize: .accessibility1)
        try? await Task.sleep(for: .milliseconds(200))
        #expect(invokes(station, SliceAccess.takeControlVerb).isEmpty)

        // Off the air: live again, and the take goes through at once.
        await station.deliver(Self.onAir(1, false))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.takeRefusal == nil })
        slices.takeControl(1)
        #expect(await settle(seconds: 30) { invokes(station, SliceAccess.takeControlVerb).count == 1 })
        #expect(invokes(station, SliceAccess.takeControlVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "incarnation", value: .i64(7)),
            LinkMessage.PropertyEntry(name: "controlRevision", value: .i64(13)),
        ])
        try await answer(station, SliceAccess.takeControlVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(14))])
        await station.deliver(Self.controller(1, Self.phoneId, revision: 14))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.control == .here })
        #expect(slices.refusal == nil && slices.takenHere == [1])
        await model.disconnect()
    }

    @Test("below 3, the Core's own slice of a headless Core greys Take control with the Core's words, on the air greys it too; hosted, it stays live")
    func coreSliceBelowThree() async throws {
        let (model, station, suite) = try await listening(version: 2)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        await station.deliver(Self.controller(1, SliceAccess.stationDeviceId, revision: 13))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.takeRefusal == Self.coreItselfWords })
        slices.takeControl(1)
        #expect(slices.taking.isEmpty)
        try await shoot("core-slice-older-core", model: model)
        // On the air greys Take control at every version, with the Core's words (JJ, 2026-09-30).
        await station.deliver(Self.controller(1, Self.macBookId, revision: 14))
        await station.deliver(Self.onAir(1, true))
        #expect(await settle(seconds: 30) {
            slices.entries.first { $0.id == 1 }?.access?.onAir == true
                && slices.entries.first { $0.id == 1 }?.takeRefusal == Self.onAirWords
        })
        // A desktop hosts the Core: its slice passes as any device's.
        await station.deliver(Self.onAir(1, false))
        await station.deliver(Self.controller(1, SliceAccess.stationDeviceId, revision: 15))
        await station.deliver(Self.connectedDevices(macBookAwayFor: nil, hostedBy: (name: "Shack PC", short: "Shack")))
        #expect(await settle(seconds: 30) {
            slices.entries.first { $0.id == 1 }?.control
                == .listening(ownerLine: "Slice B is controlled by Shack PC. Take control to change it.")
                && slices.entries.first { $0.id == 1 }?.takeRefusal == nil
        })
        try? await Task.sleep(for: .milliseconds(200))
        #expect(invokes(station, SliceAccess.takeControlVerb).isEmpty)
        await model.disconnect()
    }

    @Test("at 3, the desktop hosting the Core is named by its own name wherever the phone names the Core's position")
    func hostingDesktopNamed() async throws {
        let (model, station, suite) = try await listening(version: 3)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        await station.deliver(Self.connectedDevices(macBookAwayFor: nil, hostedBy: (name: "Shack PC", short: "Shack")))
        await station.deliver(Self.controller(1, SliceAccess.stationDeviceId, revision: 13))
        #expect(await settle(seconds: 30) {
            slices.entries.first { $0.id == 1 }?.control
                == .listening(ownerLine: "Slice B is controlled by Shack PC. Take control to change it.")
        })
        #expect(slices.entries.first { $0.id == 1 }?.takeRefusal == nil)
        try await shoot("hosted-desktop-named", model: model)
        try await shoot("hosted-desktop-named-light", model: model, scheme: .light)
        try await shoot("hosted-desktop-named-large-type", model: model, typeSize: .accessibility1)
        // Taken: the desktop stays on as a listener, named by its name.
        slices.takeControl(1)
        try await answer(station, SliceAccess.takeControlVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(14))])
        await station.deliver(Self.controller(1, Self.phoneId, revision: 14))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.control == .here })
        await station.deliver(.delta(LinkMessage.Delta(key: "access:1", properties: [
            .init(ordinal: 4, name: "listenerDeviceIds",
                  value: .utf8(LinkJSON.array([.string(Self.phoneId), .string("station")]).compactText)),
        ])))
        #expect(await settle(seconds: 30) {
            slices.entries.first { $0.id == 1 }?.access?.listenerDeviceIds == [Self.phoneId, "station"]
        })
        let access = try #require(slices.entries.first { $0.id == 1 }?.access)
        let devices = SeveralDevices.connectedDevices(in: model.mirror)
        #expect(SliceAccess.listenerWords(access, devices: devices) == ["Shack PC"])
        #expect(SliceAccess.listenerWords(access, devices: devices, short: true) == ["Shack"])
        await model.disconnect()
    }

    // MARK: The fake Core with other devices

    /// The app connected to a fake Core that shares itself (`sessionHolderVersion`
    /// 1), with the board's cast: slice A here, the MacBook's slice B beside it.
    private func connected(additions: FakeStation.Additions = .all) async throws -> (AppModel, FakeStation, String) {
        let suite = "SeveralDevicesScreenTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(mediaPeerFactory: { SilentPeer() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        // This phone has slice A only, and the MacBook has transmit, so A is
        // not the transmit slice.
        await station.deliver(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:1", className: "SliceModel")))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(name: "txSlice", value: .bool(false)),
        ])))
        // The Core's own capabilities are in the mirror before the set is
        // built from it: one built from an empty mirror drops them all.
        #expect(await settle(seconds: 30) { !model.mirror.capabilities.isEmpty })
        var capabilities = model.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        capabilities.append(LinkMessage.PropertyEntry(name: SeveralDevices.capability, value: .i64(1)))
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        // The shared Core is in the mirror before the cast builds on the
        // capabilities it holds: a set rebuilt from an older one drops it.
        #expect(await settle(seconds: 30) { SeveralDevices.available(in: model.mirror) })
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "marker:1", className: "SliceMarker",
                                                                     properties: [
            .init(ordinal: 0, name: "sliceId", value: .i64(1)),
            .init(ordinal: 1, name: "ownerDeviceId", value: .utf8(Self.macBookId)),
            .init(ordinal: 2, name: "ownerName", value: .utf8("MacBook Pro")),
            .init(ordinal: 3, name: "ownerShortName", value: .utf8("MacBook")),
            .init(ordinal: 4, name: "ownerKind", value: .utf8("computer")),
            .init(ordinal: 5, name: "ownerAway", value: .bool(false)),
            .init(ordinal: 6, name: "frequency", value: .f64(7_249_000)),
            .init(ordinal: 7, name: "dspMode", value: .enumeration(0)),
            .init(ordinal: 8, name: "filterLow", value: .i64(-3000)),
            .init(ordinal: 9, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 10, name: "txSlice", value: .bool(true)),
            .init(ordinal: 11, name: "band", value: .enumeration(3)),
            .init(ordinal: 12, name: "streamIndex", value: .i64(0)),
            .init(ordinal: 13, name: "psPaused", value: .bool(false)),
        ])))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "connectedDevices",
                                                                     className: "ConnectedDevicesFacade", properties: [
            .init(ordinal: 0, name: "listJson", value: .utf8("[]")),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
            .init(ordinal: 2, name: "deviceLimit", value: .i64(4)),
        ])))
        await station.deliver(Self.connectedDevices(macBookAwayFor: nil))
        #expect(await settle(seconds: 30) {
            model.main.slices.entries.count == 1 && model.main.band.catalog != nil && model.main.coreName != nil
        })
        return (model, station, suite)
    }

    /// Who is on the Core: this phone, the MacBook (away when `macBookAwayFor`) and the iPad.
    /// With `hostedBy`, a desktop hosts the Core: its entry carries
    /// `hostsCore` true and the id the Core gives its own position in the
    /// list (base64url of `station`), with the name and short name given.
    static func connectedDevices(macBookAwayFor away: Int64?, hostedBy desktop: (name: String, short: String)? = nil)
        -> LinkMessage {
        func device(_ id: String, _ name: String, _ short: String, _ kind: String, state: String, away: Int64 = 0,
                    holds: Bool = false, hosts: Bool = false) -> LinkJSON {
            .object(["deviceId": .string(id), "name": .string(name), "shortName": .string(short), "kind": .string(kind),
                     "paired": .bool(true), "hostsCore": .bool(hosts), "revocable": .bool(!hosts), "state": .string(state),
                     "holdsTransmit": .bool(holds), "lastActivitySeconds": .number(240),
                     "connectedForSeconds": .number(7200), "awayForSeconds": .number(Double(away)),
                     "transmittingForSeconds": .number(0), "listeningOn": .array([])])
        }
        var devices = [
            device("phone-id", "JJ's iPhone", "iPhone", "phone", state: "listening"),
            device(macBookId, "MacBook Pro", "MacBook", "computer", state: away == nil ? "listening" : "away",
                   away: away ?? 0, holds: true),
            device(iPadId, "iPad Pro", "iPad", "tablet", state: "listening"),
        ]
        if let desktop {
            devices.append(device(hostingId, desktop.name, desktop.short, "computer", state: "listening", hosts: true))
        }
        let list = LinkJSON.array(devices)
        return .delta(LinkMessage.Delta(key: "connectedDevices", properties: [
            .init(ordinal: 0, name: "listJson", value: .utf8(list.compactText)),
            .init(ordinal: 1, name: "revision", value: .i64(away == nil ? (desktop == nil ? 2 : 4) : 3)),
        ]))
    }

    /// Every receiver in use: the iPad's (Receiver 2) and the MacBook's (Receiver 3).
    static func takeReceiver(id: Int64) -> LinkMessage {
        func choice(_ number: Int, stream: Int, device: String, name: String, short: String, letter: String,
                    hz: Double, band: Int, active: Int) -> LinkJSON {
            .object(["choice": .number(Double(number)), "streamIndex": .number(Double(stream)), "adc": .number(0),
                     "centreHz": .number(hz), "rateHz": .number(192_000), "anchorName": .string(name),
                     "slices": .array([.object(["sliceId": .number(Double(letter.unicodeScalars.first!.value - 65)),
                                                "letter": .string(letter), "deviceId": .string(device),
                                                "deviceName": .string(name), "frequencyHz": .number(hz),
                                                "mode": .number(1), "band": .number(Double(band)),
                                                "txSlice": .bool(false)])]),
                     "devices": .array([.object(["deviceId": .string(device), "name": .string(name),
                                                 "shortName": .string(short), "state": .string("listening"),
                                                 "lastActivitySeconds": .number(Double(active))])]),
                     "takeable": .bool(true), "why": .string("")])
        }
        return .confirmRequest(LinkMessage.ConfirmRequest(
            id: id, kind: "takeReceiver", reason: SeveralDevices.waitingReason, affected: [], expiresInMs: 60_000,
            choices: [choice(0, stream: 1, device: iPadId, name: "iPad Pro", short: "iPad", letter: "C",
                             hz: 14_230_000, band: 5, active: 720),
                      choice(1, stream: 2, device: macBookId, name: "MacBook Pro", short: "MacBook", letter: "D",
                             hz: 18_130_000, band: 7, active: 240)],
            forCommandId: 900))
    }

    static func affectedMacBook(effect: String) -> LinkJSON {
        .object(["deviceId": .string(macBookId), "deviceName": .string("MacBook Pro"),
                 "deviceShortName": .string("MacBook"), "state": .string("listening"), "holdsTransmit": .bool(true),
                 "slices": .array([.object(["sliceId": .number(1), "letter": .string("B"),
                                            "frequencyHz": .number(7_249_000), "band": .number(3), "mode": .number(0),
                                            "adc": .number(0), "streamIndex": .number(0),
                                            "effect": .string(effect)])])])
    }

    /// Going to 20 m moves the receiver the MacBook shares.
    static func panMove(id: Int64, effect: String) -> LinkMessage {
        .confirmRequest(LinkMessage.ConfirmRequest(
            id: id, kind: "panMove", reason: SeveralDevices.waitingReason, affected: [affectedMacBook(effect: effect)],
            expiresInMs: 60_000,
            change: ["label": .string("Receiver 1"), "from": .string("40 m"), "to": .string("20 m")],
            forWriteId: 900))
    }

    /// Receiving on the transmit antenna, off to on, reaches the MacBook's
    /// slice B: the relay the shared-setting-confirm session asks about.
    static func sharedSetting(id: Int64) -> LinkMessage {
        .confirmRequest(LinkMessage.ConfirmRequest(
            id: id, kind: "sharedSetting", reason: SeveralDevices.waitingReason,
            affected: [affectedMacBook(effect: "changes")], expiresInMs: 60_000,
            change: ["label": .string("Receive on the transmit antenna"), "from": .string("Off"), "to": .string("On")],
            forWriteId: 901))
    }

    static func notice(id: Int64, kind: String, reason: String, by: Bool, secondsAgo: Int64, takeBack: Bool = false,
                       change: (String, String, String)? = nil) -> LinkMessage {
        let slice = LinkJSON.object(["sliceId": .number(0), "letter": .string("A"), "frequencyHz": .number(7_236_400),
                                     "mode": .number(0), "band": .number(3)])
        return .notice(LinkMessage.Notice(
            id: id, kind: kind, reason: reason, secondsAgo: secondsAgo, takeBack: takeBack,
            byDeviceId: by ? iPadId : nil, byName: by ? "iPad Pro" : nil, byShortName: by ? "iPad" : nil,
            byKind: by ? "tablet" : nil, bySource: by ? "device" : nil, slices: [slice],
            change: change.map { ["label": .string($0.0), "from": .string($0.1), "to": .string($0.2)] }))
    }

    // MARK: Pictures

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    /// Draws the upright main screen with the band fed synthetic rows, after `prepare`.
    /// With `deviceSize`, the window is the simulator's own screen (the
    /// iPhone the suite runs on), else an iPhone 17's. Returns how far each
    /// flag reaches past the top of the band's frequency scale strip, in
    /// points (zero or less: clear of it).
    @discardableResult
    func shoot(_ name: String, model: AppModel, scheme: ColorScheme = .dark, sideways: Bool = false,
               typeSize: DynamicTypeSize = .large, deviceSize: Bool = false, feed: Bool = true,
               prepare: () -> Void = {}) async throws -> [CGFloat] {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let screen = scene.screen.bounds.size
        let upright = deviceSize ? CGSize(width: min(screen.width, screen.height), height: max(screen.width, screen.height))
            : CGSize(width: 402, height: 874)
        let size = sideways ? CGSize(width: upright.height, height: upright.width) : upright
        let window = UIWindow(windowScene: scene)
        // Sideways, the window sits clear of the upright status bar and home
        // indicator, and takes the sideways phone's own safe area instead.
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = ScreenRoot(model: model, sideways: sideways).preferredColorScheme(scheme).dynamicTypeSize(typeSize)
        let host = UIHostingController(rootView: root)
        if sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        prepare()
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        if feed {
            band.reset()
            band.endpointId = 1
            band.receive(.context(try #require(BandFlagShotTests.context())))
            BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        }
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        await ShotWait.laidOut(window)
        var past: [CGFloat] = []
        if let view = ShotWait.bandView(for: band, in: window) {
            // As the band is laid out: while another slice's band shows, the split held down.
            let layout = BandLayout(size: view.bounds.size, scale: 1,
                                    settings: BandModel.raised(band.drawnSettings, floor: band.jumpedShareFloor))
            past = model.main.slices.flagRects.map { $0.maxY - layout.frequencyScale.minY }
            print("\(name): band \(view.bounds.size), scale strip at \(layout.frequencyScale.minY), flags reach \(past)")
        }
        // The Core's question is drawn in its own window, over this one.
        let image = ConfirmationWindows.draw(window)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return past
    }
}

/// The app's root as `RootView` lays it out.
private struct ScreenRoot: View {
    @ObservedObject var model: AppModel
    var sideways = false

    var body: some View {
        VStack(spacing: 0) {
            MainScreen(app: model, main: model.main)
            TabBar(selection: .constant(.panadapter), sideways: sideways)
                .background(ConfirmationWindowAnchor(app: model))
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

/// A media peer that never connects, so the fake Core starts nothing on
/// the network in these pictures.
private final class SilentPeer: MediaPeerConnection, @unchecked Sendable {
    let localDescription = AsyncStream<String> { _ in }
    let localCandidates = AsyncStream<(candidate: String, mid: String)> { _ in }
    let audioPackets = AsyncStream<RtpPacket> { _ in }
    let displayDatagrams = AsyncStream<Data> { _ in }
    let state = AsyncStream<MediaPeer.State> { _ in }

    func setRemoteDescription(_ sdp: String) throws {}
    func addRemoteCandidate(_ candidate: String, mid: String) throws {}
    func setExpectedAudioSsrc(_ ssrc: UInt32?) {}
    func close() {}
}

// MARK: The slice list and another slice's band, on this simulator's own iPhone (R-IOS-42)

extension SeveralDevicesScreenTests {
    /// The take-over cast with the two slices on their own receivers: A,
    /// this phone's with transmit, on receiver 0 and active; B, the
    /// MacBook's, which this phone listens to, on receiver 1.
    private func twoPans() async throws -> (AppModel, FakeStation, String) {
        let (model, station, suite) = try await listening(version: 3)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(name: "streamIndex", value: .i64(0)), .init(name: "active", value: .bool(true)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(name: "streamIndex", value: .i64(1)), .init(name: "active", value: .bool(false)),
        ])))
        let slices = model.main.slices
        #expect(await settle(seconds: 30) {
            slices.activeSliceId == 0 && slices.entries.first { $0.id == 1 }?.streamIndex == 1
        })
        return (model, station, suite)
    }

    @Test("Listen on another pan's slice jumps to its band with its full flag, clear of the scale strip upright at every text size")
    func slicelistJump() async throws {
        let (model, station, suite) = try await twoPans()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        slices.show(1)
        #expect(await settle(seconds: 30) { slices.jumped && slices.activeSliceId == 1 })
        for (name, scheme, type) in [("slicelist-build-jump", ColorScheme.dark, DynamicTypeSize.large),
                                     ("slicelist-build-jump-light", .light, .large),
                                     ("slicelist-build-jump-large-type", .dark, .accessibility1)] {
            let past = try await shoot(name, model: model, scheme: scheme, typeSize: type, deviceSize: true)
            #expect(!past.isEmpty && past.allSatisfy { $0 <= 0 }, "\(name): \(past)")
        }
        // Sideways the spectrum is shorter than a listened flag: measured, for the report.
        for (name, type) in [("slicelist-build-jump-sideways", DynamicTypeSize.large),
                             ("slicelist-build-jump-sideways-large-type", .accessibility1)] {
            _ = try await shoot(name, model: model, sideways: true, typeSize: type, deviceSize: true)
        }
        // The speaker tab stays live: its panel holds this phone's volume, mute and Stop listening.
        model.main.flagControls.toggle(.panel(1, .audio))
        try await shoot("slicelist-build-speaker-panel", model: model, deviceSize: true)
        model.main.flagControls.close()
        // On the air: Take control greys with the Core's words.
        await station.deliver(Self.onAir(1, true))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.takeRefusal != nil })
        try await shoot("slicelist-build-onair", model: model, deviceSize: true)
        await station.deliver(Self.onAir(1, false))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.takeRefusal == nil })
        // PTT goes back to A's band; B stays listened.
        slices.backForTransmit()
        #expect(await settle(seconds: 30) { !slices.jumped && slices.activeSliceId == 0 })
        #expect(slices.entries.first { $0.id == 1 }?.listening == true)
        let inView = try await shoot("slicelist-build-back-in-view", model: model, deviceSize: true)
        #expect(inView.allSatisfy { $0 <= 0 }, "\(inView)")
        // B outside the band's view: its marker at the edge.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(name: "frequency", value: .f64(14_074_000)),
        ])))
        #expect(await settle(seconds: 30) { slices.entries.first { $0.id == 1 }?.slice.frequencyHz == 14_074_000 })
        try await shoot("slicelist-build-edge-marker", model: model, deviceSize: true)
        try await shoot("slicelist-build-edge-marker-sideways", model: model, sideways: true, deviceSize: true)
        await model.disconnect()
    }

    @Test("jumped upright, the split moves down only as far as the listened flag and the listened RADE flag need, and Back puts the saved split back")
    func jumpedSplitShots() async throws {
        let (model, station, suite) = try await twoPans()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let main = model.main
        let slices = main.slices
        let saved = main.band.settings
        // The split this phone keeps, in this test's own settings, as a
        // change on the band keeps it.
        let defaults = try #require(UserDefaults(suiteName: suite))
        BandDisplaySettingsStore(defaults: defaults).setSettings(saved, forPan: BandSubscriber.panId)
        let savedData = defaults.dictionaryRepresentation()
            .filter { $0.key.hasPrefix(BandDisplaySettingsStore.keyPrefix) }.mapValues { "\($0)" }
        try #require(!savedData.isEmpty)
        slices.show(1)
        #expect(await settle(seconds: 30) { slices.jumped && slices.activeSliceId == 1 })
        let frames: [(String, ColorScheme, DynamicTypeSize)] = [("dark", .dark, .large), ("light", .light, .large),
                                                                ("large-type", .dark, .accessibility1)]
        for (suffix, scheme, type) in frames {
            let name = "jumpsplit-plain-\(suffix)"
            let past = try await shoot(name, model: model, scheme: scheme, typeSize: type, deviceSize: true)
            #expect(!past.isEmpty && past.allSatisfy { $0 <= 0 }, "\(name): \(past)")
            print("\(name): floor \(String(describing: main.band.jumpedShareFloor))")
        }
        // B in RADE, synced, with a callsign, from a Core that sends RADE
        // sync: the flag's one-line RADE row.
        var capabilities = model.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        capabilities.append(LinkMessage.PropertyEntry(name: BandSlicesModel.radeStatusCapability, value: .i64(1)))
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        #expect(await settle(seconds: 30) { model.mirror.capabilityVersion(BandSlicesModel.radeStatusCapability) == 1 })
        let catalog = try #require(slices.catalog)
        let radeU = try #require(RadeRowMirrorTests.mode("RADE-U", in: catalog))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(name: "dspMode", value: .enumeration(radeU)), .init(name: "snrDb", value: .f64(12.9)),
            .init(name: "lastRadeRxCallsign", value: .utf8("K1ABC")), .init(name: "radeSynced", value: .bool(true)),
            .init(name: "radeFreqOffsetHz", value: .f64(-12.4)),
        ])))
        #expect(await settle(seconds: 30) {
            if case .reading = slices.entries.first(where: { $0.id == 1 })?.rade?.row { return true }
            return false
        })
        for (suffix, scheme, type) in frames {
            let name = "jumpsplit-rade-\(suffix)"
            let past = try await shoot(name, model: model, scheme: scheme, typeSize: type, deviceSize: true)
            #expect(!past.isEmpty && past.allSatisfy { $0 <= 0 }, "\(name): \(past)")
            print("\(name): floor \(String(describing: main.band.jumpedShareFloor))")
        }
        // Back: the saved split, and nothing of the jumped view kept.
        slices.back()
        #expect(await settle(seconds: 30) { !slices.jumped && slices.activeSliceId == 0 })
        #expect(main.band.jumpedShareFloor == nil)
        #expect(main.band.shownSpectrumShare == saved.spectrumShare)
        #expect(main.band.settings == saved)
        let keptData = defaults.dictionaryRepresentation()
            .filter { $0.key.hasPrefix(BandDisplaySettingsStore.keyPrefix) }.mapValues { "\($0)" }
        #expect(keptData == savedData)
        try await shoot("jumpsplit-back", model: model, deviceSize: true)
        await model.disconnect()
    }

    @Test("the slice list and the Modes tab on a listened slice, drawn at this iPhone's width")
    func slicelistSheetAndModes() async throws {
        let (model, _, suite) = try await twoPans()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let main = model.main
        #expect(await settle(seconds: 30) { main.sliceList.rows.count >= 2 })
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let width = min(scene.screen.bounds.width, scene.screen.bounds.height)
        for (name, scheme, type) in [("slicelist-build-sheet", ColorScheme.dark, DynamicTypeSize.large),
                                     ("slicelist-build-sheet-light", .light, .large),
                                     ("slicelist-build-sheet-large-type", .dark, .accessibility1)] {
            try await standalone(name, size: CGSize(width: width, height: 700), scheme: scheme, type: type) {
                AnyView(SliceListSheet(model: main.sliceList, slices: main.slices) {})
            }
        }
        main.slices.show(1)
        #expect(await settle(seconds: 30) { main.slices.activeSliceId == 1 })
        for (name, type) in [("slicelist-build-modes-listening", DynamicTypeSize.large),
                             ("slicelist-build-modes-listening-large-type", .accessibility1)] {
            try await standalone(name, size: CGSize(width: width, height: 900), scheme: .dark, type: type) {
                AnyView(ScrollView { ModesPage(model: main.modes, micLevel: main.micLevel) })
            }
        }
        await model.disconnect()
    }

    private func standalone(_ name: String, size: CGSize, scheme: ColorScheme, type: DynamicTypeSize,
                            _ content: () -> AnyView) async throws {
        let window = try BandFlagShotTests.window(size: size)
        let root = content()
            .environment(\.dynamicTypeSize, type)
            .preferredColorScheme(scheme)
            .frame(width: size.width, height: size.height, alignment: .top)
            .background(ChromeColours.bar)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = scheme == .light ? .light : .dark
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: name)
    }
}

// MARK: Fix wave: markers that do not fit, and a split drag

extension SeveralDevicesScreenTests {
    /// Slice `id` on receiver `id`, which this phone listens to on the MacBook's pan.
    private func listenedSlice(_ id: Int64, hz: Double, station: FakeStation) async {
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "slice:\(id)", className: "SliceModel", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(hz)),
            .init(ordinal: 2, name: "dspMode", value: .enumeration(1)),
            .init(ordinal: 3, name: "filterLow", value: .i64(100)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(2_900)),
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 11, name: "active", value: .bool(false)),
            .init(ordinal: 12, name: "txSlice", value: .bool(false)),
            .init(ordinal: 13, name: "sliceIndex", value: .i64(id)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
            .init(ordinal: 40, name: "streamIndex", value: .i64(id)),
        ])))
        await station.deliver(Self.access(id, incarnation: 7 + id, controller: Self.macBookId, revision: 12 + id))
    }

    @Test("three listened slices off the band: the markers fold as the flags do and stay on the band, upright and sideways")
    func fixBandFoldedMarkers() async throws {
        let (model, station, suite) = try await twoPans()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let slices = model.main.slices
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(name: "frequency", value: .f64(14_074_000)),
        ])))
        await listenedSlice(2, hz: 18_100_000, station: station)
        await listenedSlice(3, hz: 21_074_000, station: station)
        #expect(await settle(seconds: 30) {
            slices.entries.filter { $0.listening }.map(\.id) == [1, 2, 3] && slices.activeSliceId == 0
        })
        let view = BandFlagShotTests.centre - BandFlagShotTests.span...BandFlagShotTests.centre + BandFlagShotTests.span
        #expect(slices.bandEntries(viewHz: view).edges.map(\.id) == [1, 2, 3])
        for (name, scheme, sideways) in [("fix-band-markers-dark", ColorScheme.dark, false),
                                         ("fix-band-markers-light", .light, false),
                                         ("fix-band-markers-sideways", .dark, true),
                                         ("fix-band-markers-sideways-light", .light, true)] {
            try await shoot(name, model: model, scheme: scheme, sideways: sideways, deviceSize: true)
        }
        await model.disconnect()
    }

    @Test("a split drag keeps the waterfall's history and storage, and the jumped view keeps its size")
    func fixBandSplitDragAndJump() async throws {
        let (model, _, suite) = try await twoPans()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let main = model.main
        let band = main.band
        try await shoot("fix-band-split-before", model: model, deviceSize: true)
        let capacity = band.state.history.capacity
        let generation = band.state.history.layoutGeneration
        let lines = band.state.history.count
        let start = band.settings.currentSpectrumSharePercent
        for percent in stride(from: start, through: start + 25, by: 5) {
            main.display.dragSpectrumHeight(percent)
            _ = try await shoot("fix-band-split-drag", model: model, deviceSize: true, feed: false)
            #expect(band.state.history.capacity == capacity, "at \(percent)%")
            #expect(band.state.history.layoutGeneration == generation, "at \(percent)%")
            #expect(band.state.history.count >= lines, "at \(percent)%")
        }
        try await shoot("fix-band-split-drag-light", model: model, scheme: .light, deviceSize: true, feed: false)
        main.slices.show(1)
        #expect(await settle(seconds: 30) { main.slices.jumped && main.slices.activeSliceId == 1 })
        for (name, scheme) in [("fix-band-jumped-dark", ColorScheme.dark), ("fix-band-jumped-light", .light)] {
            _ = try await shoot(name, model: model, scheme: scheme, deviceSize: true, feed: false)
            // The split held down under the listened flag sizes nothing anew.
            #expect(band.state.history.capacity == capacity)
        }
        main.display.dragSpectrumHeight(start)
        await model.disconnect()
    }
}
