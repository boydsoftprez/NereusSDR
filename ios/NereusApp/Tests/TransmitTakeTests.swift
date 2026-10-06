// NereusSDR for iOS: tapping TX on a flag takes transmit from the device that holds it, then moves it onto the slice
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

/// R-IOS-02, R-IOS-03, R-IOS-13: the real app connected to a fake Core at
/// `remoteTxVersion` 2 that shares itself, while the MacBook holds
/// transmit. A tap on slice A's TX button asks first, sends `tx.take` with
/// what it showed, then `tx.setTxSlice` for slice A; the Core's own
/// question is answered where it asks one; the take never keys. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each screen is written there as a PNG.
@Suite("Take transmit from the TX button", .serialized)
@MainActor
struct TransmitTakeTests {
    static let phoneId = "phone-id"
    static let macBookId = SeveralDevicesScreenTests.macBookId
    static let holdsWords = "MacBook has the transmitter."
    static let notHolderWords = "Take transmit on this device first."

    // MARK: The phone's own question

    @Test("held elsewhere: the tap asks first, then sends tx.take with what it showed, then tx.setTxSlice, and never keys")
    func askThenTakeThenChoose() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        let transmit = model.main.transmit
        #expect(take.available)
        // The flag's TX is live: the tap takes transmit instead of showing the reason.
        #expect(BandGestureLayer.transmitReason(transmit) == Self.holdsWords)
        #expect(BandGestureLayer.transmitReason(transmit, take: take) == nil)

        slices.selectForTransmit(0)
        let asked = try #require(take.asked)
        #expect(asked.holder.shortName == "MacBook")
        #expect(asked.holder.name == "MacBook Pro")
        #expect(asked.holder.kind == "computer")
        #expect(!asked.holder.onAir)
        #expect(asked.epoch == 1)
        #expect(TakeTransmitSheet.kicker(asked.holder) == "Take transmit from the MacBook?")
        #expect(TakeTransmitSheet.goTitle(onAir: false) == "Take transmit")
        #expect(TakeTransmitSheet.note(asked.holder)
                == "The MacBook has the transmitter and is not on the air. Press PTT here when you want to transmit.")
        #expect(TakeTransmitSheet.detail(asked.holder) == "Has transmit, last active 4 minutes ago")
        // Nothing is sent until the operator answers.
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        try await shoot("take-question", model: model)
        try await shoot("take-question-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-take-question", model: model, sideways: true)
        try await shoot("landscape-take-question-large-type", model: model, sideways: true, typeSize: .accessibility1)

        let confirming = Task { await take.confirm() }
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: true,
                         values: [.init(name: "holderEpoch", value: .i64(2))])
        await confirming.value
        #expect(invokes(station, TransmitTakeModel.takeVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "holderEpoch", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "shownKeyed", value: .bool(false)),
        ])
        #expect(take.asked == nil)
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: true)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0)),
        ])
        #expect(try order(station) == [TransmitTakeModel.takeVerb, BandSlicesModel.setTxSliceVerb])
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(!station.keyed)

        // The Core says so: this phone holds transmit and A is the transmit slice.
        await becomeHolder(model, station)
        #expect(await settle(seconds: 30) { transmit.report.heldHere && slices.entries.first?.slice.txSlice == true })
        try await shoot("take-result", model: model)
        try await shoot("take-result-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-take-result", model: model, sideways: true)
        #expect(invokes(station, "tx.key").isEmpty)
        await model.disconnect()
    }

    @Test("held elsewhere: a PTT tap asks to take transmit instead of a note, sends nothing until answered, never keys")
    func pttTapAsksToTake() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let transmit = model.main.transmit
        #expect(take.available)
        #expect(await settle(seconds: 30) { transmit.ptt.state == .heldElsewhere(device: "MacBook", onAir: false) })

        transmit.tapPtt()
        let asked = try #require(take.asked, "the take's question is up")
        #expect(asked.holder.shortName == "MacBook")
        #expect(transmit.heldNote == nil, "the question shows, not the note")
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        take.cancel()
        #expect(take.asked == nil)
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(!station.keyed)
        #expect(transmit.ptt.state == .heldElsewhere(device: "MacBook", onAir: false))
        await model.disconnect()
    }

    @Test("VOX on a phone that does not hold transmit takes it first, then arms; a refused take arms nothing; nothing keys")
    func voxTakesTransmitFirst() async throws {
        let (model, station, suite) = try await connected(holder: "", keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let transmit = model.main.transmit
        transmit.microphoneLineChanged(true)
        #expect(!transmit.report.heldHere)

        // Refused: nothing arms.
        transmit.toggleVox()
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: false,
                         reason: Self.notHolderWords)
        #expect(await settle(seconds: 30) { !model.main.take.inFlight })
        #expect(voxWrites(station).isEmpty, "VOX is not armed without transmit")

        // Granted: VOX arms after the take, never before it.
        transmit.toggleVox()
        try await answer(station, TransmitTakeModel.takeVerb, count: 2, accepted: true)
        #expect(await settle(seconds: 30) { voxWrites(station).count == 1 })
        #expect(voxWrites(station).first?.value == .bool(true))
        let sequence = station.messages.compactMap { message -> String? in
            switch message {
            case .commandInvoke(let invoke) where invoke.verb == TransmitTakeModel.takeVerb:
                return invoke.verb
            case .propertyWrite(let write) where write.key == TransmitModel.transmitKey:
                return "voxEnabled"
            default:
                return nil
            }
        }
        #expect(sequence == [TransmitTakeModel.takeVerb, TransmitTakeModel.takeVerb, "voxEnabled"])
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(!station.keyed)
        await model.disconnect()
    }

    /// Fix wave (fix-tx2): a second VOX tap while the first tap's take is
    /// on its way does not fall through to arming VOX at once: VOX arms
    /// only once, after the Core gave this phone transmit.
    @Test("a second VOX tap while the take is on its way starts no arm; VOX arms once, after the take")
    func secondVoxTapWhileTaking() async throws {
        let (model, station, suite) = try await connected(holder: "", keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let transmit = model.main.transmit
        let take = model.main.take
        transmit.microphoneLineChanged(true)
        #expect(!transmit.report.heldHere)

        transmit.toggleVox()
        #expect(take.inFlight)
        transmit.toggleVox()
        #expect(transmit.armsInFlightForTesting == 0, "the second tap starts no arm")
        #expect(take.inFlight)

        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: true)
        #expect(await settle(seconds: 30) { voxWrites(station).count == 1 })
        await TransmitScreenTests.barrier(model, station)
        #expect(voxWrites(station).count == 1)
        #expect(voxWrites(station).first?.value == .bool(true))
        let sequence = station.messages.compactMap { message -> String? in
            switch message {
            case .commandInvoke(let invoke) where invoke.verb == TransmitTakeModel.takeVerb:
                return invoke.verb
            case .propertyWrite(let write) where write.key == TransmitModel.transmitKey
                && write.properties.contains(where: { $0.name == "voxEnabled" }):
                return "voxEnabled"
            default:
                return nil
            }
        }
        #expect(sequence == [TransmitTakeModel.takeVerb, "voxEnabled"])
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(!station.keyed)
        await model.disconnect()
    }

    @Test("a holder on the air: the question offers Unkey and take over in red; Cancel sends nothing and changes nothing")
    func onAirHolderAndCancel() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: true)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        #expect(await settle(seconds: 30) { take.holderOnAir })

        slices.selectForTransmit(0)
        let asked = try #require(take.asked)
        #expect(asked.holder.onAir)
        #expect(asked.keyed)
        #expect(TakeTransmitSheet.goTitle(onAir: true) == "Unkey and take over")
        #expect(TakeTransmitSheet.note(asked.holder)
                == "The MacBook is on the air now. Taking over unkeys it first. Press PTT here when you want to transmit.")
        try await shoot("take-question-on-air", model: model)
        try await shoot("take-question-on-air-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-take-question-on-air", model: model, sideways: true)

        take.cancel()
        #expect(take.asked == nil)
        #expect(take.pendingSliceId == nil)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(slices.refusal == nil)

        // Confirmed after all: shownKeyed says it was on the air.
        slices.selectForTransmit(0)
        let confirming = Task { await take.confirm() }
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: true)
        await confirming.value
        #expect(invokes(station, TransmitTakeModel.takeVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "holderEpoch", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "shownKeyed", value: .bool(true)),
        ])
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: true)
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(!station.keyed)
        await model.disconnect()
    }

    @Test("a refused take shows the Core's words as sent in the sheet, and Close changes nothing")
    func refusedInTheSheet() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        slices.selectForTransmit(0)
        let words = "Transmit is changing hands. Try again in a moment."
        let confirming = Task { await take.confirm() }
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: false, reason: words,
                         values: [.init(name: "refusalCode", value: .utf8("changingHands")),
                                  .init(name: "refusalFix", value: .utf8(""))])
        await confirming.value
        #expect(take.answering == .refused(words))
        #expect(take.asked != nil)
        try await shoot("take-refused", model: model)
        try await shoot("take-refused-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-take-refused", model: model, sideways: true)
        take.close()
        #expect(take.asked == nil)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
        #expect(invokes(station, "tx.key").isEmpty)
        await model.disconnect()
    }

    // MARK: The Core's question

    @Test("the Core asks its own question: Confirm, a question asked again, Cancel and a refusal each do what they say")
    func coreQuestionAnswers() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        let devices = model.devices

        // The MacBook keyed since the phone asked: the Core asks again, in its words.
        try await takeUntilTheCoreAsks(take, slices, devices, station, count: 1, questionId: 50, keyed: true)
        #expect(take.asked == nil)
        let question = try #require(devices.question)
        #expect(question.kind == .takeTransmit)
        #expect(ConfirmationLayer.shows(question.kind, takesTransmit: true))
        let holder = try #require(question.holder)
        #expect(holder.onAir)
        #expect(TakeTransmitSheet.kicker(holder) == "Take transmit from the MacBook?")
        #expect(TakeTransmitSheet.detail(holder) == "On the air for 40 seconds")
        try await shoot("core-question", model: model)
        try await shoot("core-question-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-core-question", model: model, sideways: true)

        // Confirm, and the Core asks once more (the holder changed again).
        let first = Task { await take.proceedCore() }
        try await answer(station, SeveralDevices.proceedVerb, count: 1, accepted: false,
                         reason: SeveralDevices.waitingReason)
        // The question asked again names the same held tx.take.
        let takeId = try lastTakeId(station)
        await station.deliver(Self.takeQuestion(id: 51, keyed: false, forCommandId: takeId))
        await first.value
        #expect(await settle(seconds: 30) { devices.question?.id == 51 })
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)

        // Confirmed: the Core gave transmit, and slice A gets it.
        let second = Task { await take.proceedCore() }
        try await answer(station, SeveralDevices.proceedVerb, count: 2, accepted: true,
                         values: [.init(name: "holderEpoch", value: .i64(2))])
        await second.value
        #expect(invokes(station, SeveralDevices.proceedVerb).last?.args.first
                == LinkMessage.PropertyEntry(name: "id", value: .i64(51)))
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: true)
        #expect(try order(station) == [TransmitTakeModel.takeVerb, SeveralDevices.proceedVerb,
                                       SeveralDevices.proceedVerb, BandSlicesModel.setTxSliceVerb])

        // Asked again later, and cancelled: confirm.cancel, and no transmit slice.
        try await takeUntilTheCoreAsks(take, slices, devices, station, count: 2, questionId: 52, keyed: false)
        take.cancelCore()
        #expect(devices.question == nil)
        #expect(await settle(seconds: 30) { invokes(station, SeveralDevices.cancelVerb).count == 1 })
        #expect(take.pendingSliceId == nil)

        // Asked again, and refused: the Core's words as sent, then Close.
        try await takeUntilTheCoreAsks(take, slices, devices, station, count: 3, questionId: 53, keyed: false)
        let refusal = "The MacBook did not let go of transmit. Try again."
        let third = Task { await take.proceedCore() }
        try await answer(station, SeveralDevices.proceedVerb, count: 3, accepted: false, reason: refusal)
        await third.value
        #expect(devices.answering == .refused(refusal))
        try await shoot("core-question-refused", model: model)
        take.closeCore()
        #expect(devices.question == nil)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).count == 1)
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(!station.keyed)
        await model.disconnect()
    }

    @Test("Take transmit greys while a question about the take is up, the phone's own or the Core's, and comes back when it goes")
    func greyedWhileAQuestionIsUp() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        let devices = model.devices
        #expect(TakeTransmitButton.enabled(take))

        // The phone's own question.
        slices.selectForTransmit(0)
        #expect(take.asked != nil)
        #expect(take.questionUp)
        #expect(!TakeTransmitButton.enabled(take))
        #expect(TakeTransmitButton.reason(take) == nil)
        take.cancel()
        #expect(!take.questionUp)
        #expect(TakeTransmitButton.enabled(take))

        // The Core's question about this phone's tx.take.
        try await takeUntilTheCoreAsks(take, slices, devices, station, count: 1, questionId: 50, keyed: false)
        #expect(take.questionUp)
        #expect(!TakeTransmitButton.enabled(take))
        take.cancelCore()
        #expect(!take.questionUp)
        #expect(TakeTransmitButton.enabled(take))
        await model.disconnect()
    }

    // MARK: Nobody holds it, this phone holds it, an older Core

    @Test("nobody holds transmit: the tap takes it at once, a second tap sends nothing more, then the slice is chosen; a refusal shows over the band as sent")
    func unheldTakesAtOnce() async throws {
        let (model, station, suite) = try await connected(holder: "", keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        slices.selectForTransmit(0)
        // The take is on its way before anything reaches the Core: a
        // double tap, on the flag or a Take transmit, starts nothing more.
        #expect(take.inFlight)
        #expect(!TakeTransmitButton.enabled(take))
        slices.selectForTransmit(0)
        #expect(!take.begin(sliceId: 0, offered: true))
        #expect(take.asked == nil)
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: true)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, TransmitTakeModel.takeVerb).count == 1)
        #expect(invokes(station, TransmitTakeModel.takeVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "holderEpoch", value: .i64(2)),
            LinkMessage.PropertyEntry(name: "shownKeyed", value: .bool(false)),
        ])
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: true)
        #expect(try order(station) == [TransmitTakeModel.takeVerb, BandSlicesModel.setTxSliceVerb])

        // Refused: the Core's words over the band, and no transmit slice.
        let words = "This app cannot transmit on this Core."
        slices.selectForTransmit(0)
        try await answer(station, TransmitTakeModel.takeVerb, count: 2, accepted: false, reason: words)
        #expect(await settle(seconds: 30) { slices.refusal?.text == words })
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).count == 1)
        #expect(invokes(station, "tx.key").isEmpty)
        await model.disconnect()
    }

    @Test("this phone already holds transmit: the tap sends tx.setTxSlice alone, as before; its refusal offers Take transmit")
    func alreadyHolder() async throws {
        let (model, station, suite) = try await connected(holder: Self.phoneId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        #expect(await settle(seconds: 30) { model.main.transmit.report.heldHere })
        slices.selectForTransmit(0)
        #expect(take.asked == nil)
        // The Core no longer counts this phone as the holder: its words and fix, as sent.
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: false,
                         reason: Self.notHolderWords,
                         values: [.init(name: "refusalCode", value: .utf8("notHolder")),
                                  .init(name: "refusalFix", value: .utf8(TxRefusalInfo.takeTransmit))])
        #expect(await settle(seconds: 30) { slices.refusal?.text == Self.notHolderWords })
        #expect(slices.refusal?.fix == TxRefusalInfo.takeTransmit)
        #expect(slices.refusal?.sliceId == 0)
        try await shoot("refusal-take-transmit", model: model)
        try await shoot("refusal-take-transmit-large-type", model: model, typeSize: .accessibility1)
        try await shoot("landscape-refusal-take-transmit", model: model, sideways: true)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)

        // The refusal's Take transmit: the phone still believes it holds
        // transmit, and sends tx.take all the same; the Core answers a
        // holder's take accepted, then slice A is chosen.
        #expect(TakeTransmitButton.enabled(take))
        #expect(take.begin(sliceId: slices.refusal?.sliceId, offered: true))
        #expect(take.asked == nil)
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: true,
                         values: [.init(name: "holderEpoch", value: .i64(1))])
        #expect(invokes(station, TransmitTakeModel.takeVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "holderEpoch", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "shownKeyed", value: .bool(false)),
        ])
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 2, accepted: true)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).last?.args == [
            LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0)),
        ])
        #expect(try order(station) == [BandSlicesModel.setTxSliceVerb, TransmitTakeModel.takeVerb,
                                       BandSlicesModel.setTxSliceVerb])
        #expect(invokes(station, "tx.key").isEmpty)
        await model.disconnect()
    }

    @Test("an older Core, one not shared, or a receive-only one: no tx.take; Take transmit is greyed with the reason in the phone's words")
    func olderCore() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false, remoteTxVersion: 1)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        let transmit = model.main.transmit
        #expect(!take.available)
        #expect(BandGestureLayer.transmitReason(transmit, take: take) == Self.holdsWords)
        #expect(!take.transmitOn(sliceId: 0))
        // The TX button's guarded choice sends tx.setTxSlice, and the Core refuses it.
        slices.selectForTransmit(0)
        try await answer(station, BandSlicesModel.setTxSliceVerb, count: 1, accepted: false,
                         reason: Self.notHolderWords,
                         values: [.init(name: "refusalFix", value: .utf8(TxRefusalInfo.takeTransmit))])
        #expect(await settle(seconds: 30) { slices.refusal?.text == Self.notHolderWords })
        #expect(take.asked == nil)
        // Its Take transmit is drawn, greyed, with the reason in the phone's words.
        #expect(!TakeTransmitButton.enabled(take))
        #expect(TakeTransmitButton.reason(take) == TransmitTakeModel.olderCoreText)
        #expect(TakeTransmitButton.reason(take)
                == "This Core can't hand transmit over from here. Updating the Core may help.")
        #expect(TxPanel.offersTake(model.main.transmit))
        try await shoot("older-core-take-transmit", model: model)
        try await shoot("older-core-take-transmit-large-type", model: model, typeSize: .accessibility1)
        #expect(!take.begin(sliceId: 0, offered: true))
        #expect(take.asked == nil)

        // Remote transmit 2, but a Core that does not share itself: still none.
        await setCapabilities(model, station, ["remoteTxVersion": .i64(2), SeveralDevices.capability: .i64(0)])
        #expect(await settle(seconds: 30) { model.mirror.capabilityVersion("remoteTxVersion") == 2 })
        #expect(await settle(seconds: 30) { take.unavailableReason == TransmitTakeModel.notSharedText })
        #expect(!take.available)
        #expect(!TakeTransmitButton.enabled(take))
        #expect(TakeTransmitButton.reason(take)
                == "This Core isn't shared between devices, so there is no transmit to take over.")
        try await shoot("not-shared-take-transmit", model: model)
        #expect(!take.transmitOn(sliceId: 0))
        #expect(!take.begin(sliceId: 0, offered: true))

        // Shared, but a receive-only station: nothing to take.
        await setCapabilities(model, station, [SeveralDevices.capability: .i64(1),
                                               "txRefusalCode": .utf8(TransmitTakeModel.receiveOnlyCode)])
        #expect(await settle(seconds: 30) { take.unavailableReason == TransmitTakeModel.receiveOnlyText })
        #expect(!take.available)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        #expect(invokes(station, "tx.key").isEmpty)
        await model.disconnect()
    }

    // MARK: Take transmit where a refusal offers it

    @Test("the PTT's notice and the TX panel's holder line offer Take transmit, which runs the same take")
    func takeTransmitButtons() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let transmit = model.main.transmit
        let offered = TxRefusalInfo(reason: Self.holdsWords, code: "otherDeviceHolds", fix: TxRefusalInfo.takeTransmit)
        #expect(transmit.permission == offered)
        // PTT's notice: the reason a tap sent nothing, and a key's refusal with the fix.
        #expect(TxNoticeCard.offersTake(.held(Self.holdsWords), permission: offered))
        #expect(TxNoticeCard.offersTake(.refusal(offered), permission: nil))
        #expect(!TxNoticeCard.offersTake(.refusal(TxRefusalInfo(reason: "Amp in standby.", fix: TxRefusalInfo.operateAmp)),
                                         permission: offered))
        // The TX panel's holder line, while the MacBook holds transmit.
        #expect(TxPanel.offersTake(transmit))
        #expect(await settle(seconds: 30) {
            if case .heldElsewhere = transmit.ptt.state {
                return true
            }
            return false
        })
        // A tap asks to take transmit where the take is offered (T5): the
        // question comes up in place of the note, and Cancel sends nothing.
        transmit.tapPtt()
        #expect(take.asked != nil)
        #expect(transmit.heldNote == nil)
        take.cancel()
        #expect(take.asked == nil)
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        try await shoot("ptt-take-transmit", model: model)
        try await shoot("tx-panel-take-transmit", model: model, txPanelOpen: true)
        try await shoot("tx-panel-take-transmit-large-type", model: model, typeSize: .accessibility1, txPanelOpen: true)
        try await shoot("landscape-tx-panel-take-transmit", model: model, sideways: true, txPanelOpen: true)

        // The button's take: asked first, and no transmit slice after it.
        take.begin(sliceId: nil)
        #expect(take.asked != nil)
        let confirming = Task { await take.confirm() }
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: true)
        await confirming.value
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
        #expect(invokes(station, "tx.key").isEmpty)
        #expect(!station.keyed)
        await model.disconnect()
    }

    // MARK: What the take keeps, and what it lets go

    @Test("the take carries what the question showed: epoch 1 unkeyed, though the Core moved on to epoch 2 keyed")
    func shownEpochIsSent() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let transmit = model.main.transmit
        model.main.slices.selectForTransmit(0)
        let asked = try #require(take.asked)
        #expect(asked.epoch == 1)
        #expect(!asked.keyed)
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(name: "keyed", value: .bool(true)),
            .init(name: "holderEpoch", value: .i64(2)),
        ]))
        #expect(await settle(seconds: 30) { transmit.report.holderEpoch == 2 && transmit.report.keyed })
        #expect(take.asked == asked)
        let confirming = Task { await take.confirm() }
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: false,
                         reason: SeveralDevices.waitingReason,
                         values: [.init(name: "phase", value: .utf8("needsConfirmation"))])
        await confirming.value
        #expect(invokes(station, TransmitTakeModel.takeVerb).first?.args == [
            LinkMessage.PropertyEntry(name: "holderEpoch", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "shownKeyed", value: .bool(false)),
        ])
        #expect(invokes(station, "tx.key").isEmpty)
        await model.disconnect()
    }

    @Test("only the Core's question about this phone's own tx.take gives its slice transmit; another takeTransmit question never does")
    func onlyItsOwnQuestionChoosesTheSlice() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        let devices = model.devices

        // The Core holds the take for its question, saying so with the
        // phase alone: the phone waits for the question, as it does for
        // the words.
        slices.selectForTransmit(0)
        let first = Task { await take.confirm() }
        try await answer(station, TransmitTakeModel.takeVerb, count: 1, accepted: false, reason: "",
                         values: [.init(name: "phase", value: .utf8(SeveralDevices.needsConfirmationPhase))])
        await first.value
        #expect(take.asked == nil)
        #expect(take.answering == .idle)
        #expect(slices.refusal == nil)
        #expect(take.pendingSliceId == 0)
        let takeId = try lastTakeId(station)
        #expect(take.takeCommandId == takeId)

        // A takeTransmit question about another command (a Take it back)
        // comes instead: the slice is let go, and its proceed chooses none.
        await station.deliver(Self.takeQuestion(id: 60, keyed: false, forCommandId: 7_777))
        #expect(await settle(seconds: 30) { devices.question?.id == 60 })
        #expect(take.pendingSliceId == nil)
        let other = Task { await take.proceedCore() }
        try await answer(station, SeveralDevices.proceedVerb, count: 1, accepted: true)
        await other.value
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)

        // Its own question, cancelled; then a Take it back's question,
        // proceeded: still no transmit slice.
        try await takeUntilTheCoreAsks(take, slices, devices, station, count: 2, questionId: 61, keyed: false)
        #expect(take.pendingSliceId == 0)
        take.cancelCore()
        #expect(take.pendingSliceId == nil)
        await station.deliver(Self.takeQuestion(id: 62, keyed: false, forCommandId: 7_778))
        #expect(await settle(seconds: 30) { devices.question?.id == 62 })
        let takeBack = Task { await take.proceedCore() }
        try await answer(station, SeveralDevices.proceedVerb, count: 2, accepted: true)
        await takeBack.value

        // Its own question, closed from the Core's side (replaced by
        // another question): the slice is let go with it.
        try await takeUntilTheCoreAsks(take, slices, devices, station, count: 3, questionId: 63, keyed: false)
        #expect(take.pendingSliceId == 0)
        await station.deliver(Self.takeQuestion(id: 64, keyed: false, forCommandId: 7_779))
        #expect(await settle(seconds: 30) { devices.question?.id == 64 })
        #expect(take.pendingSliceId == nil)
        let replaced = Task { await take.proceedCore() }
        try await answer(station, SeveralDevices.proceedVerb, count: 3, accepted: true)
        await replaced.value
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
        #expect(invokes(station, "tx.key").isEmpty)
        await model.disconnect()
    }

    @Test("transmit changing hands: Take transmit starts nothing, and the refusal it shows stays up")
    func changingHandsStaysUp() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: true)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        await station.deliver(TransmitScreenTests.txStateDelta([.init(name: "holderTransferring", value: .bool(true))]))
        #expect(await settle(seconds: 30) { model.main.transmit.report.holderTransferring })
        // The button's action: its refusal closes only when the take started.
        var closed = false
        if take.begin(sliceId: 0, offered: true) {
            closed = true
        }
        #expect(!closed)
        #expect(slices.refusal?.text == TransmitTakeModel.changingHandsText)
        #expect(slices.refusal?.text == "Transmit is changing hands. Try again in a moment.")
        #expect(take.asked == nil)
        try await shoot("take-changing-hands", model: model)
        try await LinkBarrier.roundTrip(model.commands)
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        await model.disconnect()
    }

    @Test("the question goes when the take stops being offered, and when the link is lost with it open")
    func linkLostWithTheSheetOpen() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices

        // The Core stops taking tx.take while the question is up.
        slices.selectForTransmit(0)
        #expect(take.asked != nil)
        await setCapabilities(model, station, ["remoteTxVersion": .i64(1)])
        #expect(await settle(seconds: 30) { !take.available })
        #expect(take.asked == nil)
        #expect(take.answering == .idle)
        #expect(take.pendingSliceId == nil)

        // Offered again, asked again, and the link drops.
        await setCapabilities(model, station, ["remoteTxVersion": .i64(2)])
        #expect(await settle(seconds: 30) { take.available })
        slices.selectForTransmit(0)
        #expect(take.asked != nil)
        #expect(take.pendingSliceId == 0)
        await station.dropLink()
        #expect(await settle(seconds: 30) { take.asked == nil })
        #expect(take.answering == .idle)
        #expect(take.pendingSliceId == nil)
        #expect(!take.inFlight)
        // The link is down, so no command can go after these; what the app
        // queued on reading the drop has run once the main queue is drained.
        await MainQueue.drained()
        #expect(invokes(station, TransmitTakeModel.takeVerb).isEmpty)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
        await model.disconnect()
    }

    @Test("ending the session lets the take go itself, before the session stops: nothing asked or sent carries to the next Core")
    func disconnectLetsTheTakeGoItself() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        slices.selectForTransmit(0)
        let confirming = Task { await take.confirm() }
        #expect(await settle(seconds: 30) { invokes(station, TransmitTakeModel.takeVerb).count == 1 })
        #expect(take.inFlight && take.pendingSliceId == 0)
        // Read while the session is still up, before its stop can reach the take.
        var beforeTheStop: Bool?
        model.disconnectBeforeSessionStopForTesting = {
            beforeTheStop = take.asked == nil && take.answering == .idle && !take.inFlight
                && take.pendingSliceId == nil && take.takeCommandId == nil
        }
        await model.disconnect()
        await confirming.value
        #expect(beforeTheStop == true)
        #expect(slices.refusal == nil)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
    }

    @Test("a link lost with a take on its way: the take is let go, with no refusal and no transmit slice after it")
    func linkLostWithATakeInFlight() async throws {
        let (model, station, suite) = try await connected(holder: Self.macBookId, keyed: false)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        #expect(TransmitTakeModel.takeTimeout == .seconds(14))
        slices.selectForTransmit(0)
        let confirming = Task { await take.confirm() }
        #expect(await settle(seconds: 30) { invokes(station, TransmitTakeModel.takeVerb).count == 1 })
        #expect(take.inFlight)
        #expect(take.answering == .taking)
        await station.dropLink()
        await confirming.value
        #expect(await settle(seconds: 30) { take.asked == nil && !take.inFlight })
        #expect(take.answering == .idle)
        #expect(take.pendingSliceId == nil)
        #expect(take.takeCommandId == nil)
        // The link is down: see linkLostWithTheSheetOpen.
        await MainQueue.drained()
        #expect(slices.refusal == nil)
        #expect(invokes(station, BandSlicesModel.setTxSliceVerb).isEmpty)
        #expect(invokes(station, "tx.key").isEmpty)
        // And a Core switch leaves nothing behind either.
        await model.disconnect()
        #expect(take.asked == nil && !take.inFlight && take.pendingSliceId == nil)
    }

    @Test("a pending OLD take parked before CommandClient admission never reaches a replacement owner")
    func pendingTakeCannotAcquireReplacementSession() async throws {
        let rig = await TransmitOwnershipRig.make()
        let gate = MicrophoneTransmitTests.Gate()
        rig.take.beforeTakeAdmissionForTesting = { await gate.wait() }
        var grants = 0
        #expect(rig.take.begin(sliceId: nil, granted: { grants += 1 }))
        let pending = try #require(rig.take.lastTakeOperationForTesting)
        await gate.whenWaiting()
        #expect(rig.transport.messages(owner: 1).isEmpty)

        await rig.replaceOwner()
        #expect(!rig.take.inFlight)
        rig.take.beforeTakeAdmissionForTesting = nil
        #expect(rig.take.begin(sliceId: nil, granted: { grants += 1 }))
        let fresh = try #require(rig.take.lastTakeOperationForTesting)
        await fresh.value
        #expect(grants == 1, "NEW's own take still succeeds")
        let before = rig.transport.messages(owner: 2)
        #expect(before.count == 1)
        gate.open()
        await pending.value

        #expect(rig.transport.messages(owner: 2).count == before.count,
                "OLD's epoch must not acquire a NEW command route at actor admission")
        #expect(rig.take.takeCommandId == nil)
        #expect(rig.take.pendingSliceId == nil)
        #expect(grants == 1, "OLD's completion cannot run its retired grant")
        #expect(rig.slices.refusal == nil)
        await rig.transmit.sessionChanged(.stopped, owner: 2)
    }

    @Test("reset revokes a take parked at the final transport handoff in the same session")
    func resetRevokesTakeAtFinalTransportHandoff() async throws {
        let rig = await TransmitOwnershipRig.make()
        let gate = MicrophoneTransmitTests.Gate()
        let takeVerb = TransmitTakeModel.takeVerb
        rig.transport.beforeHandoff = { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == takeVerb {
                await gate.wait()
            }
        }
        var grants = 0
        #expect(rig.take.begin(sliceId: nil, granted: { grants += 1 }))
        let pending = try #require(rig.take.lastTakeOperationForTesting)
        await gate.whenWaiting()
        rig.take.sessionChanged(.stopped)
        gate.open()
        await pending.value

        #expect(rig.transport.messages(owner: 1).isEmpty,
                "a reset intent must lose authority before synchronous transport handoff")
        #expect(grants == 0)
        #expect(rig.take.takeCommandId == nil)
        #expect(!rig.take.inFlight)
        #expect(rig.slices.refusal == nil)
        await rig.transmit.sessionChanged(.stopped, owner: 1)
    }

    // MARK: The fake Core

    /// The app on a fake Core that shares itself and takes `tx.take`
    /// (`remoteTxVersion` 2), with this phone's slice A, the MacBook's
    /// slice B, and transmit held by `holder` ("" for nobody).
    private func connected(holder: String, keyed: Bool,
                           remoteTxVersion: Int64 = 2) async throws -> (AppModel, FakeStation, String) {
        let suite = "TransmitTakeTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(mediaPeerFactory: { TakeSilentPeer() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: FakeStation.Additions.all.union(.remoteTx))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await station.deliver(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:1", className: "SliceModel")))
        // Slice A is not the transmit slice, whoever holds transmit.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(name: "txSlice", value: .bool(false)),
        ])))
        model.main.transmit.thisDeviceId = Self.phoneId
        let short = holder == Self.phoneId ? "iPhone" : holder.isEmpty ? "" : "MacBook"
        var holderProperties = TransmitScreenTests.holder(holder, short: short, keyed: keyed)
        holderProperties.append(.init(ordinal: 3, name: "txSliceId", value: .i64(1)))
        holderProperties.append(.init(ordinal: 17, name: "stopSerial", value: .i64(0)))
        holderProperties.append(.init(ordinal: 27, name: "keyedForSeconds", value: .i64(keyed ? 40 : 0)))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "txState", className: "TransmitState",
                                                                     properties: holderProperties)))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "connectedDevices",
                                                                     className: "ConnectedDevicesFacade", properties: [
            .init(ordinal: 0, name: "listJson", value: .utf8("[]")),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
            .init(ordinal: 2, name: "deviceLimit", value: .i64(4)),
        ])))
        await station.deliver(SeveralDevicesScreenTests.connectedDevices(macBookAwayFor: nil))
        let heldElsewhere = !holder.isEmpty && holder != Self.phoneId
        var changes: [String: LinkMessage.PropertyValue] = [
            SeveralDevices.capability: .i64(1), "remoteTxVersion": .i64(remoteTxVersion),
            "txPermitted": .bool(!heldElsewhere),
        ]
        if heldElsewhere {
            changes["txRefusalReason"] = .utf8(Self.holdsWords)
            changes["txRefusalCode"] = .utf8("otherDeviceHolds")
            changes["txRefusalFix"] = .utf8(TxRefusalInfo.takeTransmit)
        }
        await setCapabilities(model, station, changes)
        let transmit = model.main.transmit
        let slices = model.main.slices
        #expect(await settle(seconds: 30) {
            slices.entries.count == 1 && model.main.band.catalog != nil
                && transmit.report.held == !holder.isEmpty && transmit.permitted == !heldElsewhere
                && model.mirror.capabilityVersion("remoteTxVersion") == remoteTxVersion
                && model.main.take.available == (remoteTxVersion >= 2)
        })
        return (model, station, suite)
    }

    /// Replaces or adds the named capabilities, keeping the rest.
    private func setCapabilities(_ model: AppModel, _ station: FakeStation,
                                 _ changes: [String: LinkMessage.PropertyValue]) async {
        var capabilities = model.mirror.capabilities.compactMap { name, value in
            changes[name] == nil ? LinkMessage.PropertyEntry(name: name, value: value.wireValue) : nil
        }
        for (name, value) in changes.sorted(by: { $0.key < $1.key }) {
            capabilities.append(LinkMessage.PropertyEntry(name: name, value: value))
        }
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
    }

    /// The Core gave this phone transmit, on slice A.
    private func becomeHolder(_ model: AppModel, _ station: FakeStation) async {
        await station.deliver(TransmitScreenTests.txStateDelta(
            TransmitScreenTests.holder(Self.phoneId, short: "iPhone", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0))]))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(name: "txSlice", value: .bool(true)),
        ])))
        await setCapabilities(model, station, ["txPermitted": .bool(true), "txRefusalReason": .utf8(""),
                                               "txRefusalCode": .utf8(""), "txRefusalFix": .utf8("")])
    }

    /// Taps slice A's TX, confirms the phone's question, and the Core
    /// answers that it asks (`count` takes so far), then asks `questionId`.
    private func takeUntilTheCoreAsks(_ take: TransmitTakeModel, _ slices: BandSlicesModel,
                                      _ devices: SeveralDevicesClient, _ station: FakeStation, count: Int,
                                      questionId: Int64, keyed: Bool) async throws {
        slices.selectForTransmit(0)
        #expect(take.asked != nil)
        let confirming = Task { await take.confirm() }
        try await answer(station, TransmitTakeModel.takeVerb, count: count, accepted: false,
                         reason: SeveralDevices.waitingReason,
                         values: [.init(name: "phase", value: .utf8("needsConfirmation"))])
        await confirming.value
        #expect(take.asked == nil)
        let takeId = try lastTakeId(station)
        await station.deliver(Self.takeQuestion(id: questionId, keyed: keyed, forCommandId: takeId))
        #expect(await settle(seconds: 30) { devices.question?.id == questionId })
    }

    /// This phone's last `tx.take`, whose id the Core's question names.
    private func lastTakeId(_ station: FakeStation) throws -> Int64 {
        Int64(try #require(invokes(station, TransmitTakeModel.takeVerb).last).id)
    }

    /// The Core's `takeTransmit` question, the MacBook holding transmit,
    /// about the command `forCommandId` (this phone's `tx.take`, or
    /// another, such as a Take it back).
    static func takeQuestion(id: Int64, keyed: Bool, forCommandId: Int64) -> LinkMessage {
        .confirmRequest(LinkMessage.ConfirmRequest(
            id: id, kind: "takeTransmit", reason: SeveralDevices.waitingReason, affected: [], expiresInMs: 60_000,
            forCommandId: forCommandId,
            holder: ["deviceId": .string(macBookId), "name": .string("MacBook Pro"), "shortName": .string("MacBook"),
                     "kind": .string("computer"), "source": .string("device"),
                     "state": .string(keyed ? "transmitting" : "listening"), "keyed": .bool(keyed),
                     "connectedForSeconds": .number(7200), "lastActivitySeconds": .number(240),
                     "awayForSeconds": .number(0), "transmittingForSeconds": .number(keyed ? 40 : 0)]))
    }

    /// This phone's writes of `transmit.voxEnabled`.
    private func voxWrites(_ station: FakeStation) -> [LinkMessage.PropertyEntry] {
        station.messages.compactMap { message in
            guard case .propertyWrite(let write) = message, write.key == TransmitModel.transmitKey else {
                return nil
            }
            return write.properties.first { $0.name == "voxEnabled" }
        }
    }

    private func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == verb {
                return invoke
            }
            return nil
        }
    }

    /// The transmit verbs the phone sent, in order.
    private func order(_ station: FakeStation) throws -> [String] {
        let verbs: Set<String> = [TransmitTakeModel.takeVerb, BandSlicesModel.setTxSliceVerb, SeveralDevices.proceedVerb,
                                  "tx.key"]
        return station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, verbs.contains(invoke.verb) {
                return invoke.verb
            }
            return nil
        }
    }

    private func answer(_ station: FakeStation, _ verb: String, count: Int, accepted: Bool, reason: String = "",
                        values: [LinkMessage.PropertyEntry]? = nil) async throws {
        #expect(await settle(seconds: 30) { invokes(station, verb).count == count })
        let invoke = try #require(invokes(station, verb).last)
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: verb, id: invoke.id, accepted: accepted, reason: reason, affected: [], values: values)))
    }

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

    // MARK: Pictures

    /// Draws the main screen with the band fed synthetic rows, the question's window over it.
    private func shoot(_ name: String, model: AppModel, sideways: Bool = false, typeSize: DynamicTypeSize = .large,
                       txPanelOpen: Bool = false) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = TakeScreenRoot(model: model, sideways: sideways, txPanelOpen: txPanelOpen)
            .preferredColorScheme(.dark).dynamicTypeSize(typeSize)
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
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        band.reset()
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        if txPanelOpen {
            // The TX panel scrolled to its foot, where Take transmit sits
            // under the holder's line.
            #expect(Self.scrollPanelsToTheFoot(in: window, beyond: size.width / 5) > 0)
            try? await Task.sleep(for: .milliseconds(300))
            window.layoutIfNeeded()
        }
        let image = ConfirmationWindows.draw(window)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    /// Scrolls every vertical scroll view that starts right of `beyond`
    /// (the TX panel slides in from the right) to its foot; returns how
    /// many it moved.
    private static func scrollPanelsToTheFoot(in view: UIView, beyond: CGFloat) -> Int {
        var moved = 0
        if let scroll = view as? UIScrollView, let window = view.window {
            let frame = scroll.convert(scroll.bounds, to: window)
            let foot = scroll.contentSize.height + scroll.adjustedContentInset.bottom - scroll.bounds.height
            if frame.minX >= beyond, foot > 1 {
                scroll.setContentOffset(CGPoint(x: scroll.contentOffset.x, y: foot), animated: false)
                moved += 1
            }
        }
        for subview in view.subviews {
            moved += scrollPanelsToTheFoot(in: subview, beyond: beyond)
        }
        return moved
    }
}

/// The app's root as `RootView` lays it out, the TX panel open when asked.
private struct TakeScreenRoot: View {
    @ObservedObject var model: AppModel
    var sideways = false
    var txPanelOpen = false

    var body: some View {
        VStack(spacing: 0) {
            MainScreen(app: model, main: model.main, txPanelOpen: txPanelOpen)
            TabBar(selection: .constant(.panadapter), sideways: sideways)
                .background(ConfirmationWindowAnchor(app: model))
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

/// A media peer that never connects, so the fake Core starts nothing on the network.
private final class TakeSilentPeer: MediaPeerConnection, @unchecked Sendable {
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
