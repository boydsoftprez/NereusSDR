// NereusSDR for iOS: a flag's TX badge takes the slice and transmit it needs, then makes it the transmit slice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing

/// R-IOS-42, R-IOS-11, R-IOS-13 (JJ's ruling of 2026-09-30): the real app
/// on a fake Core that shares itself and takes `tx.take`, with this
/// phone's slice A and the MacBook's slice B, which this phone listens
/// to. A tap on a flag's TX badge takes what its slice needs, in order:
/// the slice (`slice.takeControl`), then transmit (`tx.take`, asked first
/// while another device holds it), then `tx.setTxSlice`. Nothing keys.
/// The wait for the holder's change after a slice take runs on the
/// suite's own clock (``BadgeClock``), and nothing here reads or waits on
/// the wall clock: each check waits for what the app did by letting
/// queued work run.
@Suite("The flag's TX badge takes the slice and transmit", .serialized)
@MainActor
struct TxBadgeTests {
    static let phoneId = SeveralDevicesScreenTests.phoneId
    static let macBookId = SeveralDevicesScreenTests.macBookId
    static let holdsWords = "MacBook has the transmitter."
    static let takeVerb = TransmitTakeModel.takeVerb
    static let setVerb = BandSlicesModel.setTxSliceVerb
    static let sliceVerb = SliceAccess.takeControlVerb
    static let takingControl = TransmitTakeModel.BadgeOffer.held(reason: VfoFlagView.takingControlTitle)
    static let takingTransmit = TransmitTakeModel.BadgeOffer.held(reason: TakeTransmitSheet.busyTitle(onAir: false))

    // MARK: What the badge offers

    @Test("the badge offers each case in the desktop's words, the holder named by its own name")
    func offers() async throws {
        let (model, station, suite, _) = try await rig(txSlice: 1)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        // Case 3 and case 2 while the MacBook holds transmit.
        #expect(take.badgeOffer(try entry(model, 1))
                == .take(hint: "Take control of this slice, then take transmit from MacBook"))
        #expect(take.badgeOffer(try entry(model, 0)) == .take(hint: "Take transmit from MacBook and make this the TX slice"))

        // Nobody holds it, and this phone may transmit.
        await freeTransmit(model, station)
        #expect(take.badgeOffer(try entry(model, 1)) == .take(hint: "Take control of this slice and make it the TX slice"))
        #expect(take.badgeOffer(try entry(model, 0)) == .take(hint: "Take transmit and make this the TX slice"))

        // This phone holds it: case 3 takes the slice alone, case 1 is the choice as before.
        await becomeHolder(model, station, txSlice: 0)
        #expect(take.badgeOffer(try entry(model, 1)) == .take(hint: TransmitTakeModel.takeSliceText))
        #expect(take.badgeOffer(try entry(model, 0)) == .choose)

        // The desktop hosting the Core holds transmit: named by its own name.
        await station.deliver(SeveralDevicesScreenTests.connectedDevices(macBookAwayFor: nil,
                                                                         hostedBy: (name: "Shack PC", short: "Shack")))
        await holdElsewhere(model, station, holder: SliceAccess.stationDeviceId, short: "", txSlice: 5)
        #expect(await settle { model.mirror.object("connectedDevices")?["revision"] == .int(4) })
        #expect(take.badgeOffer(try entry(model, 1))
                == .take(hint: "Take control of this slice, then take transmit from Shack"))
        #expect(take.badgeOffer(try entry(model, 0)) == .take(hint: "Take transmit from Shack and make this the TX slice"))
        await model.disconnect()
    }

    @Test("the holder's refusals are offered; every other refusal code greys the badge with the Core's words and asks nothing")
    func refusalCodes() async throws {
        let (model, station, suite, _) = try await rig(txSlice: 5)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        let transmit = model.main.transmit
        // The MacBook on the air: the holder's refusal too.
        await setCaps(model, station, ["txRefusalReason": .utf8("MacBook is on the air."),
                                       "txRefusalCode": .utf8("holderOnAir")])
        #expect(await settle { transmit.permission?.code == "holderOnAir" })
        #expect(take.takesTransmit)
        #expect(take.badgeOffer(try entry(model, 1))
                == .take(hint: "Take control of this slice, then take transmit from MacBook"))
        #expect(take.badgeOffer(try entry(model, 0)) == .take(hint: "Take transmit from MacBook and make this the TX slice"))

        // TxRefusal.h at Core trunk 01d797e56, less the holder's two, and the pairing refusal.
        let codes = ["notReady", "bandPlan", "interlock", "ampStandby", "paProtection", "swr", "programNeedsTransmit",
                     "micNotReady", "changingHands", "stopNotConfirmed", "notHolder", "keyEnded", "noTransmitSlice",
                     "chooseTransmitSlice", "deviceNotPaired"]
        for code in codes {
            let words = "The Core's words for \(code)."
            await setCaps(model, station, ["txRefusalReason": .utf8(words), "txRefusalCode": .utf8(code)])
            #expect(await settle { transmit.permission?.code == code })
            #expect(!take.takesTransmit, "\(code)")
            #expect(take.badgeOffer(try entry(model, 1)) == .held(reason: words), "\(code)")
            #expect(take.badgeOffer(try entry(model, 0)) == .held(reason: words), "\(code)")
            take.badgeTapped(1)
            #expect(slices.refusal?.text == words, "\(code)")
            slices.dismissRefusal()
            take.badgeTapped(0)
            #expect(slices.refusal?.text == words, "\(code)")
            slices.dismissRefusal()
            #expect(take.asked == nil && take.badge == nil && !take.inFlight && slices.taking.isEmpty, "\(code)")
        }

        // Receive only: no take at all, and the Core's words.
        let receiveOnly = "This station only listens."
        await setCaps(model, station, ["txRefusalReason": .utf8(receiveOnly),
                                       "txRefusalCode": .utf8(TransmitTakeModel.receiveOnlyCode)])
        #expect(await settle { !take.available })
        #expect(take.badgeOffer(try entry(model, 1)) == .held(reason: receiveOnly))
        #expect(take.badgeOffer(try entry(model, 0)) == .held(reason: receiveOnly))
        take.badgeTapped(1)
        #expect(slices.refusal?.text == receiveOnly)
        slices.dismissRefusal()

        // Slice B on the air: the take-over's own refusal, unchanged.
        await setCaps(model, station, ["txRefusalReason": .utf8(Self.holdsWords),
                                       "txRefusalCode": .utf8("otherDeviceHolds")])
        await station.deliver(SeveralDevicesScreenTests.onAir(1, true))
        #expect(await settle { take.available && (try? entry(model, 1))?.takeRefusal != nil })
        let onAir = try #require(try entry(model, 1).takeRefusal)
        #expect(take.badgeOffer(try entry(model, 1)) == .held(reason: onAir))
        take.badgeTapped(1)
        #expect(slices.refusal?.text == onAir)
        await drain()
        #expect(order(station).isEmpty)
        await model.disconnect()
    }

    // MARK: Case 3

    @Test("the MacBook's own transmit slice, freed as control passes: slice.takeControl, tx.take, tx.setTxSlice, no question")
    func freedHolderTakesAtOnce() async throws {
        let (model, station, suite, clock) = try await rig(txSlice: 1)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        take.badgeTapped(1)
        #expect(take.badge?.stage == .slice)
        // A take on its way greys the badge with its own words, and a second tap sends nothing more.
        #expect(take.badgeOffer(try entry(model, 1)) == Self.takingControl)
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        // The answer comes before the holder's change: the badge waits for it.
        #expect(await settle { take.badge?.stage == .holder && clock.waiting == 1 })
        #expect(take.asked == nil)
        #expect(take.badgeOffer(try entry(model, 1)) == Self.takingTransmit)
        // The Core frees transmit with the slice: the take goes at once, unasked.
        await station.deliver(SeveralDevicesScreenTests.controller(1, Self.phoneId, revision: 13))
        await freeTransmit(model, station)
        try await answer(station, Self.takeVerb, count: 1, accepted: true)
        #expect(take.asked == nil)
        // Granted: the slice is chosen once this phone shows as the holder, and not before.
        #expect(await settle { take.badge?.granted == true })
        await drain()
        #expect(invokes(station, Self.setVerb).isEmpty)
        await becomeHolder(model, station, txSlice: 0, asking: 1)
        try await answer(station, Self.setVerb, count: 1, accepted: true)
        #expect(invokes(station, Self.setVerb).first?.args == [LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1))])
        #expect(take.badge == nil)
        // The wait's end, now, changes nothing.
        clock.fire()
        await drain()
        #expect(order(station) == [Self.sliceVerb, Self.takeVerb, Self.setVerb])
        #expect(!station.keyed)
        await model.disconnect()
    }

    @Test("a stale holder is never asked: the badge waits for the holder's change, and asks only once the wait is over")
    func staleHolderWait() async throws {
        let (model, station, suite, clock) = try await rig(txSlice: 1)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        #expect(await settle { take.badge?.stage == .holder && clock.waiting == 1 })
        // Other news from the Core, the holder unchanged: still waiting, still no question.
        await station.deliver(SeveralDevicesScreenTests.controller(1, Self.phoneId, revision: 13))
        await station.deliver(TransmitScreenTests.txStateDelta([
            .init(ordinal: 27, name: "keyedForSeconds", value: .i64(0)),
        ]))
        #expect(await settle { (try? entry(model, 1))?.listening == false })
        await drain()
        #expect(take.badge?.stage == .holder)
        #expect(take.asked == nil)
        #expect(invokes(station, Self.takeVerb).isEmpty)
        // The wait ends with the MacBook still holding: the phone's own question.
        clock.fire()
        #expect(await settle { take.asked != nil })
        #expect(take.asked?.holder.shortName == "MacBook")
        #expect(take.badge?.stage == .transmit)
        let confirming = Task { await take.confirm() }
        try await answer(station, Self.takeVerb, count: 1, accepted: true)
        await confirming.value
        await becomeHolder(model, station, txSlice: 0, asking: 1)
        try await answer(station, Self.setVerb, count: 1, accepted: true)
        #expect(invokes(station, Self.setVerb).first?.args == [LinkMessage.PropertyEntry(name: "sliceId", value: .i64(1))])
        #expect(order(station) == [Self.sliceVerb, Self.takeVerb, Self.setVerb])
        await model.disconnect()
    }

    @Test("held on another slice: the question at once; the holder freed while it is up, the take goes unasked")
    func questionGivesWay() async throws {
        let (model, station, suite, clock) = try await rig(txSlice: 5)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        #expect(await settle { take.asked != nil })
        #expect(clock.waiting == 0)
        await freeTransmit(model, station)
        #expect(await settle { take.asked == nil && invokes(station, Self.takeVerb).count == 1 })
        try await answer(station, Self.takeVerb, count: 1, accepted: true)
        await becomeHolder(model, station, txSlice: 0, asking: 1)
        try await answer(station, Self.setVerb, count: 1, accepted: true)
        #expect(order(station) == [Self.sliceVerb, Self.takeVerb, Self.setVerb])
        await model.disconnect()
    }

    // MARK: Cases 1 and 2

    @Test("case 2 asks with the phone's own question; case 1 sends tx.setTxSlice alone")
    func ownSlice() async throws {
        let (model, station, suite, _) = try await rig(txSlice: 1)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        take.badgeTapped(0)
        #expect(take.asked?.holder.shortName == "MacBook")
        #expect(take.badgeOffer(try entry(model, 0)) == Self.takingTransmit)
        let confirming = Task { await take.confirm() }
        try await answer(station, Self.takeVerb, count: 1, accepted: true)
        await confirming.value
        try await answer(station, Self.setVerb, count: 1, accepted: true)
        #expect(invokes(station, Self.setVerb).first?.args == [LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0))])

        // This phone holds transmit, bound to another slice: A's badge is the choice alone.
        await becomeHolder(model, station, txSlice: 1)
        #expect(take.badgeOffer(try entry(model, 0)) == .choose)
        take.badgeTapped(0)
        try await answer(station, Self.setVerb, count: 2, accepted: true)
        #expect(invokes(station, Self.setVerb).last?.args == [LinkMessage.PropertyEntry(name: "sliceId", value: .i64(0))])
        #expect(order(station) == [Self.takeVerb, Self.setVerb, Self.setVerb])
        await model.disconnect()
    }

    // MARK: How a take ends

    @Test("a refused slice take, a cancelled question and a refused tx.take each end the badge with nothing pending")
    func endings() async throws {
        let (model, station, suite, _) = try await rig(txSlice: 5)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let slices = model.main.slices
        // The slice take refused: the Core's words, and nothing more.
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: false,
                         reason: SeveralDevicesScreenTests.transmittingWords)
        #expect(await settle { slices.refusal?.text == SeveralDevicesScreenTests.transmittingWords && take.badge == nil })
        #expect(take.asked == nil && slices.taking.isEmpty)
        slices.dismissRefusal()

        // Taken, then the question cancelled: nothing pending.
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 2, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        #expect(await settle { take.asked != nil })
        take.cancel()
        #expect(take.badge == nil && take.asked == nil && take.pendingSliceId == nil)

        // Nobody holds it, and the tx.take refused: its words over the band, nothing pending.
        await station.deliver(SeveralDevicesScreenTests.access(1, incarnation: 7, controller: Self.macBookId,
                                                                revision: 14))
        await freeTransmit(model, station)
        #expect(await settle { (try? entry(model, 1))?.listening == true })
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 3, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(15))])
        let words = "This app cannot transmit on this Core."
        try await answer(station, Self.takeVerb, count: 1, accepted: false, reason: words)
        #expect(await settle { slices.refusal?.text == words && take.badge == nil && !take.inFlight })
        #expect(take.pendingSliceId == nil && take.takeCommandId == nil)
        await drain()
        #expect(order(station) == [Self.sliceVerb, Self.sliceVerb, Self.sliceVerb, Self.takeVerb])
        await model.disconnect()
    }

    @Test("after a cancelled badge take, a later unrelated grant never sends a stale tx.setTxSlice")
    func noStaleChoice() async throws {
        let (model, station, suite, _) = try await rig(txSlice: 5)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        #expect(await settle { take.asked != nil })
        take.cancel()
        #expect(take.badge == nil)
        // The TX panel's Take transmit, granted: transmit here, and no slice chosen.
        #expect(take.begin(sliceId: nil, offered: true))
        let confirming = Task { await take.confirm() }
        try await answer(station, Self.takeVerb, count: 1, accepted: true)
        await confirming.value
        await becomeHolder(model, station, txSlice: 0)
        await drain()
        #expect(invokes(station, Self.setVerb).isEmpty)
        #expect(order(station) == [Self.sliceVerb, Self.takeVerb])
        await model.disconnect()
    }

    @Test("a link lost while the badge waits for the holder leaves nothing pending, and the wait's end does nothing")
    func linkLostWhileWaiting() async throws {
        let (model, station, suite, clock) = try await rig(txSlice: 1)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        #expect(await settle { take.badge?.stage == .holder && clock.waiting == 1 })
        await station.dropLink()
        #expect(await settle { take.badge == nil })
        clock.fire()
        await drain()
        #expect(take.badge == nil && take.asked == nil && !take.inFlight && take.pendingSliceId == nil)
        #expect(order(station) == [Self.sliceVerb])
        await model.disconnect()
    }

    @Test("a link lost after the grant, before this phone shows as the holder, sends no tx.setTxSlice")
    func linkLostAfterTheGrant() async throws {
        let (model, station, suite, _) = try await rig(txSlice: 5)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        await freeTransmit(model, station)
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        try await answer(station, Self.takeVerb, count: 1, accepted: true)
        #expect(await settle { take.badge?.granted == true })
        await station.dropLink()
        #expect(await settle { take.badge == nil })
        await drain()
        #expect(order(station) == [Self.sliceVerb, Self.takeVerb])
        await model.disconnect()
    }

    // MARK: Pictures

    @Test("pictures: the badge offered on the MacBook's slice, the transmit ask, greyed with a refusal; light, dark, large type")
    func pictures() async throws {
        let (model, station, suite, _) = try await rig(txSlice: 5)
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let take = model.main.take
        let shots = SeveralDevicesScreenTests()
        try await shootAll("txbadge-offered", model: model, shots: shots)
        take.badgeTapped(1)
        try await answer(station, Self.sliceVerb, count: 1, accepted: true,
                         values: [.init(name: "controlRevision", value: .i64(13))])
        await station.deliver(SeveralDevicesScreenTests.controller(1, Self.phoneId, revision: 13))
        #expect(await settle { take.asked != nil && (try? entry(model, 1))?.listening == false })
        try await shootAll("txbadge-ask", model: model, shots: shots)
        take.cancel()
        await station.deliver(SeveralDevicesScreenTests.access(1, incarnation: 7, controller: Self.macBookId,
                                                                revision: 14))
        let words = "Pair this device with the Core to transmit."
        await setCaps(model, station, ["txRefusalReason": .utf8(words), "txRefusalCode": .utf8("deviceNotPaired")])
        #expect(await settle { (try? take.badgeOffer(entry(model, 1))) == .held(reason: words) })
        take.badgeTapped(1)
        #expect(model.main.slices.refusal?.text == words)
        try await shootAll("txbadge-refused", model: model, shots: shots)
        await model.disconnect()
    }

    private func shootAll(_ name: String, model: AppModel, shots: SeveralDevicesScreenTests) async throws {
        for scheme in [ColorScheme.dark, .light] {
            let tag = scheme == .dark ? "dark" : "light"
            try await shots.shoot("\(name)-\(tag)", model: model, scheme: scheme, deviceSize: true)
            try await shots.shoot("\(name)-large-type-\(tag)", model: model, scheme: scheme,
                                  typeSize: .accessibility1, deviceSize: true)
        }
    }

    // MARK: The fake Core

    /// The take-over cast (``SeveralDevicesScreenTests/listening(version:)``)
    /// on a Core that takes `tx.take`, the MacBook holding transmit on
    /// `txSlice` (slice B's own when 1). The badge's wait runs on the
    /// returned clock.
    private func rig(txSlice: Int64) async throws -> (AppModel, FakeStation, String, BadgeClock) {
        let (model, station, suite) = try await SeveralDevicesScreenTests().listening(version: 3)
        let clock = BadgeClock()
        model.main.take.holderWait = { await clock.wait($0) }
        await station.deliver(SeveralDevicesScreenTests.connectedDevices(macBookAwayFor: nil))
        await setCaps(model, station, ["remoteTxVersion": .i64(2)])
        #expect(await settle {
            model.mirror.capabilityVersion(SliceAccess.capability) == 3 && SeveralDevices.available(in: model.mirror)
                && model.mirror.capabilityVersion("remoteTxVersion") == 2
        })
        await holdElsewhere(model, station, holder: Self.macBookId, short: "MacBook", txSlice: txSlice)
        #expect(await settle { model.main.take.available }, "\(model.main.take.unavailableReason ?? "")")
        return (model, station, suite, clock)
    }

    /// Another device holds transmit on `txSlice`, and the Core says so.
    private func holdElsewhere(_ model: AppModel, _ station: FakeStation, holder: String, short: String,
                               txSlice: Int64) async {
        await station.deliver(TransmitScreenTests.txStateDelta(
            TransmitScreenTests.holder(holder, short: short, keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(txSlice))]))
        await markTxSlice(station, txSlice)
        await setCaps(model, station, ["txPermitted": .bool(false), "txRefusalReason": .utf8(Self.holdsWords),
                                       "txRefusalCode": .utf8("otherDeviceHolds"),
                                       "txRefusalFix": .utf8(TxRefusalInfo.takeTransmit)])
        let transmit = model.main.transmit
        #expect(await settle {
            transmit.report.heldElsewhere && !transmit.permitted && transmit.permission?.code == "otherDeviceHolds"
        })
    }

    /// Nobody holds transmit, and this phone may transmit.
    private func freeTransmit(_ model: AppModel, _ station: FakeStation) async {
        await station.deliver(TransmitScreenTests.txStateDelta(
            TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(-1))]))
        await markTxSlice(station, -1)
        await setCaps(model, station, ["txPermitted": .bool(true), "txRefusalReason": .utf8(""),
                                       "txRefusalCode": .utf8(""), "txRefusalFix": .utf8("")])
        let transmit = model.main.transmit
        #expect(await settle { !transmit.report.held && transmit.permitted })
    }

    /// The Core gave this phone transmit, bound to `txSlice`. With
    /// `asking`, the phone's `tx.setTxSlice` for that slice goes as it
    /// becomes the holder and is not answered here: that slice shows as the
    /// transmit slice from the touch (JJ, 2026-10-01, the liveui rule,
    /// StationClient.cpp:1040-1068), over the Core's `txSlice` meanwhile.
    private func becomeHolder(_ model: AppModel, _ station: FakeStation, txSlice: Int64,
                              asking: Int64? = nil) async {
        await markTxSlice(station, txSlice)
        await station.deliver(TransmitScreenTests.txStateDelta(
            TransmitScreenTests.holder(Self.phoneId, short: "iPhone", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(txSlice))]))
        await setCaps(model, station, ["txPermitted": .bool(true), "txRefusalReason": .utf8(""),
                                       "txRefusalCode": .utf8(""), "txRefusalFix": .utf8("")])
        let transmit = model.main.transmit
        let slices = model.main.slices
        #expect(await settle {
            transmit.report.heldHere && transmit.permitted
                && slices.entries.first { $0.id == Int(asking ?? txSlice) }?.slice.txSlice == true
        })
    }

    /// Slice A and slice B's `txSlice`, as the Core binds transmit to `txSlice`.
    private func markTxSlice(_ station: FakeStation, _ txSlice: Int64) async {
        for id in [Int64(0), 1] {
            await station.deliver(.delta(LinkMessage.Delta(key: "slice:\(id)", properties: [
                .init(name: "txSlice", value: .bool(id == txSlice)),
            ])))
        }
    }

    private func entry(_ model: AppModel, _ id: Int) throws -> BandSlicesModel.Entry {
        try #require(model.main.slices.entries.first { $0.id == id })
    }

    /// Replaces or adds the named capabilities, keeping the rest.
    private func setCaps(_ model: AppModel, _ station: FakeStation,
                         _ changes: [String: LinkMessage.PropertyValue]) async {
        var capabilities = model.mirror.capabilities.compactMap { name, value in
            changes[name] == nil ? LinkMessage.PropertyEntry(name: name, value: value.wireValue) : nil
        }
        for (name, value) in changes.sorted(by: { $0.key < $1.key }) {
            capabilities.append(LinkMessage.PropertyEntry(name: name, value: value))
        }
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
    }

    private func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == verb {
                return invoke
            }
            return nil
        }
    }

    /// The take-over and transmit verbs the phone sent, in order.
    private func order(_ station: FakeStation) -> [String] {
        let verbs: Set<String> = [Self.sliceVerb, Self.takeVerb, Self.setVerb, SeveralDevices.proceedVerb, "tx.key"]
        return station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, verbs.contains(invoke.verb) {
                return invoke.verb
            }
            return nil
        }
    }

    private func answer(_ station: FakeStation, _ verb: String, count: Int, accepted: Bool, reason: String = "",
                        values: [LinkMessage.PropertyEntry]? = nil) async throws {
        #expect(await settle { invokes(station, verb).count == count }, "\(verb) \(count)")
        let invoke = try #require(invokes(station, verb).last)
        await station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: verb, id: invoke.id, accepted: accepted, reason: reason, affected: [], values: values)))
    }

    /// Waits for `condition` by letting the main run loop turn, never by the clock.
    private func settle(_ condition: () throws -> Bool) async -> Bool {
        for _ in 0..<Self.turns {
            if (try? condition()) == true {
                return true
            }
            await Self.turn()
        }
        return (try? condition()) == true
    }

    /// Lets queued work run, so what would have been sent has been.
    private func drain() async {
        for _ in 0..<2_000 {
            await Self.turn()
        }
    }

    /// How many turns of the main run loop a check waits at most.
    private static let turns = 300_000

    /// One turn of the main run loop: everything it has queued runs first.
    private static func turn() async {
        await withCheckedContinuation { (done: CheckedContinuation<Void, Never>) in
            RunLoop.main.perform { done.resume() }
        }
    }
}

/// The suite's own clock for the badge's wait: a wait lasts until the test ends it.
@MainActor
final class BadgeClock {
    private var sleepers: [CheckedContinuation<Void, Never>] = []

    /// How many waits are running.
    var waiting: Int { sleepers.count }

    func wait(_ duration: Duration) async {
        #expect(duration == TransmitTakeModel.holderWaitTime)
        await withCheckedContinuation { sleepers.append($0) }
    }

    /// Every running wait ends now.
    func fire() {
        let ending = sleepers
        sleepers = []
        ending.forEach { $0.resume() }
    }
}
