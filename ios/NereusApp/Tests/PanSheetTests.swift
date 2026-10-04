// NereusSDR for iOS: the Pan 1 sheet against a fake Core, newer and older: the band grid, a slice or notch added here, the extended view
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import Testing

/// D73, R-IOS-11, R-IOS-27, D23: the Pan 1 sheet lists the Core's bands and
/// lights the slice's band as the Core reports it, sends the Core's verbs
/// only when the Core offers them, shows the Core's refusals in its words,
/// and greys what an older Core cannot do with "Needs a newer Core".
@Suite("Pan 1 sheet", .serialized)
@MainActor
struct PanSheetTests {
    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func quiet() async {
        for _ in 0..<5_000 {
            await Task.yield()
        }
    }

    /// A model connected to a fake Core with `additions`, with the ANAN-G2
    /// catalogue as that Core sends it.
    private func connected(_ additions: FakeStation.Additions) async throws -> (AppModel, FakeStation) {
        let station = try FakeStation(additions: additions)
        let suite = "PanSheetTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle { model.main.catalogFeed.catalog != nil && model.main.pan.sliceLetter == "A" })
        await quiet()
        return (model, station)
    }

    private func invokes(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap {
            if case .commandInvoke(let invoke) = $0, invoke.verb == verb {
                return invoke
            }
            return nil
        }
    }

    private func buttons(_ pan: PanSheetModel) -> (buttons: [PanSheetModel.BandButton], enabled: Bool)? {
        if case .buttons(let buttons, let enabled) = pan.bandGrid {
            return (buttons, enabled)
        }
        return nil
    }

    // MARK: The band grid

    @Test("the grid is the Core's bands in its order, the slice's band lit, and a tap opens a band through the Core")
    func bandGrid() async throws {
        let (model, station) = try await connected(.all)
        let pan = model.main.pan
        let grid = try #require(buttons(pan))
        #expect(grid.enabled)
        #expect(grid.buttons.map(\.label) == FakeStation.bands.map(\.label))
        #expect(grid.buttons.map(\.id) == FakeStation.bands.map(\.id))
        // The fixture's slice is on the Core's band 5.
        #expect(grid.buttons.filter(\.lit).map(\.label) == ["20"])

        pan.selectBand(3)
        #expect(await settle { self.buttons(pan)?.buttons.first(where: \.lit)?.id == 3 })
        let sent = try #require(invokes(station, PanSheetModel.selectBandVerb).last)
        #expect(sent.args == [.init(name: "sliceId", value: .i64(0)), .init(name: "band", value: .i64(3))])
        #expect(pan.note == nil)
        await model.disconnect()
    }

    @Test("a Core with 2 m lists it after 6 m, lights it and opens band 27; one without it is never sent 27")
    func twoMetres() async throws {
        let (model, station) = try await connected([.all, .band2m])
        let pan = model.main.pan
        let grid = try #require(buttons(pan))
        #expect(grid.enabled)
        #expect(grid.buttons.map(\.label) == ["160", "80", "60", "40", "30", "20", "17", "15", "12", "10", "6", "2", "WWV"])
        pan.selectBand(27)
        #expect(await settle { self.buttons(pan)?.buttons.first(where: \.lit)?.label == "2" })
        let sent = try #require(invokes(station, PanSheetModel.selectBandVerb).last)
        #expect(sent.args == [.init(name: "sliceId", value: .i64(0)), .init(name: "band", value: .i64(27))])
        await model.disconnect()

        let (older, olderStation) = try await connected(.all)
        #expect(buttons(older.main.pan)?.buttons.contains { $0.id == 27 } == false)
        older.main.pan.selectBand(27)
        await quiet()
        #expect(invokes(olderStation, PanSheetModel.selectBandVerb).isEmpty)
        await older.disconnect()
    }

    @Test("a refused band shows the Core's words and the lit button stays with the slice's band")
    func refusedBand() async throws {
        let (model, station) = try await connected(.all)
        let pan = model.main.pan
        // The Core's refusal of a locked slice (RadioModel's words, with the desktop's band label).
        let locked = "Band 60m ignored: the slice is locked. Unlock it to change bands."
        station.refuseNext(PanSheetModel.selectBandVerb, reason: locked)
        pan.selectBand(2)
        #expect(await settle { pan.note == locked })
        #expect(buttons(pan)?.buttons.filter(\.lit).map(\.id) == [5])
        // The next band the Core opens clears the words.
        pan.selectBand(3)
        #expect(await settle { pan.note == nil })
        await model.disconnect()
    }

    @Test("with bands and no band select the grid is greyed and sends nothing")
    func gridGreyedWithoutBandSelect() async throws {
        let (model, station) = try await connected(.bands)
        let pan = model.main.pan
        let grid = try #require(buttons(pan))
        #expect(!grid.enabled)
        #expect(grid.buttons.map(\.label) == FakeStation.bands.map(\.label))
        pan.selectBand(3)
        await quiet()
        #expect(invokes(station, PanSheetModel.selectBandVerb).isEmpty)
        await model.disconnect()
    }

    @Test("without bands the grid is one greyed row, and the other older-Core controls are greyed too")
    func olderCore() async throws {
        let (model, station) = try await connected([])
        let pan = model.main.pan
        #expect(pan.bandGrid == .needsNewerCore)
        #expect(!pan.canAddNotch)
        #expect(!pan.extendedViewAvailable)
        pan.selectBand(3)
        pan.addNotch()
        pan.toggleExtendedView()
        await quiet()
        #expect(invokes(station, PanSheetModel.selectBandVerb).isEmpty)
        #expect(invokes(station, PanSheetModel.addNotchVerb).isEmpty)
        #expect(!model.main.band.settings.extendedView)
        // Add a slice runs on every Core.
        #expect(pan.panKey == "pan-0")
        await model.disconnect()
    }

    // MARK: A slice or a notch added here

    @Test("Add a slice here asks for a slice on this pan and shows the Core's refusal")
    func addSlice() async throws {
        let (model, station) = try await connected([])
        let pan = model.main.pan
        station.refuseNext(PanSheetModel.addSliceVerb, reason: "The radio has no more slices.")
        pan.addSlice()
        #expect(await settle { pan.note == "The radio has no more slices." })
        let sent = try #require(invokes(station, PanSheetModel.addSliceVerb).first)
        #expect(sent.args == [.init(name: "panId", value: .utf8("pan-0"))])
        await model.disconnect()
    }

    @Test("closing this phone's last slice leaves Add a slice here aimed at its former pan")
    func addAfterLastSlice() async throws {
        let (model, station) = try await connected(.all)
        let pan = model.main.pan
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("phone:pan-7")),
        ])))
        #expect(await settle { pan.panKey == "phone:pan-7" })
        await station.deliver(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:0", className: "SliceModel")))
        #expect(await settle { model.main.slices.activeSliceId == nil })
        #expect(await settle { pan.sliceLetter == nil && !pan.canAddNotch })
        #expect(pan.panKey == "phone:pan-7")
        #expect(!pan.canSelectBand)
        pan.addSlice()
        #expect(await settle { !self.invokes(station, PanSheetModel.addSliceVerb).isEmpty })
        #expect(invokes(station, PanSheetModel.addSliceVerb).last?.args ==
                [.init(name: "panId", value: .utf8("phone:pan-7"))])
        await model.disconnect()
    }

    @Test("a cold empty pan uses this phone's first pan even when another device has a slice")
    func coldEmptyPan() async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        mirror.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: "marker:4", className: "SliceMarker", properties: [
            .init(ordinal: 1, name: "ownerDeviceId", value: .utf8("other-phone")),
            .init(ordinal: 4, name: "panKey", value: .utf8("foreign:pan-9")),
        ])))
        let sent = SentCommands()
        let commands = CommandClient(send: { message in await MainActor.run { sent.messages.append(message) } })
        await commands.handle(.stateChanged(.ready))
        let settings = SettingsProxyClient(send: { _ in })
        let main = MainScreenModel(mirror: mirror, settings: settings, commands: commands,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))
        #expect(!main.pan.canAddSlice)
        mirror.apply(.snapshotComplete)
        #expect(await settle { main.pan.canAddSlice })
        #expect(main.pan.panKey == "pan-0")
        #expect(!main.pan.canSelectBand)
        #expect(!main.pan.canAddNotch)
        main.pan.addSlice()
        #expect(await settle { !sent.messages.isEmpty })
        guard case .commandInvoke(let invoke)? = sent.messages.first else {
            Issue.record("Expected addSliceOnPan")
            return
        }
        #expect(invoke.args == [.init(name: "panId", value: .utf8("pan-0"))])
        await commands.handle(.stateChanged(.stopped))
        #expect(await settle { main.pan.note == "The connection to the Core was lost." })
        main.pan.addSlice()
        #expect(await settle { main.pan.note == "The request could not be sent to the Core." })
    }

    @Test("an owned slice with an empty pan key cannot erase this phone's first-pan target")
    func emptyOwnedPanKey() async throws {
        let (model, station) = try await connected(.all)
        let pan = model.main.pan
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 27, name: "panKey", value: .utf8("")),
        ])))
        #expect(await settle { model.main.slices.active?.panKey == "" })
        #expect(pan.panKey == "pan-0")
        pan.addSlice()
        #expect(await settle { !self.invokes(station, PanSheetModel.addSliceVerb).isEmpty })
        #expect(invokes(station, PanSheetModel.addSliceVerb).last?.args ==
                [.init(name: "panId", value: .utf8("pan-0"))])
        await model.disconnect()
    }

    @MainActor private final class SentCommands {
        var messages: [LinkMessage] = []
    }

    @Test("Add a notch asks the Core to place it at the active slice, with no frequency from the phone")
    func addNotch() async throws {
        let (model, station) = try await connected(.all)
        let pan = model.main.pan
        #expect(pan.canAddNotch)
        pan.addNotch()
        #expect(await settle { !self.invokes(station, PanSheetModel.addNotchVerb).isEmpty })
        let sent = try #require(invokes(station, PanSheetModel.addNotchVerb).first)
        #expect(sent.args == [.init(name: "sliceId", value: .i64(0))])
        // The Core's words, as the suite's verbs-notch-at-slice carries them.
        station.refuseNext(PanSheetModel.addNotchVerb, reason: "A notch already exists within 10 Hz")
        pan.addNotch()
        #expect(await settle { pan.note == "A notch already exists within 10 Hz" })
        await model.disconnect()
    }

    // MARK: The extended view

    @Test("the extended view is kept for the pan and rides in the band's subscription only on a Core that offers it")
    func extendedView() async throws {
        for wideband in [true, false] {
            let rig = try DisplaySheetTests.Rig(wideband: wideband)
            let pan = rig.main.pan
            await quiet()
            #expect(pan.extendedViewAvailable == wideband)
            rig.main.subscriber.receive(.mediaState(.connected))
            #expect(await settle { rig.sent.count == 1 })
            #expect(rig.sent.last?.extendedView == nil)
            pan.toggleExtendedView()
            await quiet()
            if wideband {
                #expect(pan.extendedViewOn)
                #expect(await settle { rig.sent.count == 2 })
                #expect(rig.sent.last?.extendedView == true)
                #expect(rig.store.settings(forPan: BandSubscriber.panId).extendedView)
                pan.toggleExtendedView()
                #expect(await settle { rig.sent.count == 3 })
                #expect(rig.sent.last?.extendedView == nil)
            } else {
                #expect(!pan.extendedViewOn)
                #expect(rig.sent.count == 1)
                #expect(!rig.store.settings(forPan: BandSubscriber.panId).extendedView)
            }
        }
    }

    // MARK: Zooming out past the receiver

    /// The subscriptions the app sent the fake, in order.
    private func subscribes(_ station: FakeStation) -> [[String: LinkJSON]] {
        station.messages.compactMap {
            if case .mediaControl(let control) = $0, control.payload["op"] == .string("subscribe") {
                return control.payload
            }
            return nil
        }
    }

    private func span(_ payload: [String: LinkJSON]?) -> Double? {
        if case .number(let value)? = payload?["spanHz"] {
            return value
        }
        return nil
    }

    /// A model with media to a fake Core that offers the extended view.
    private func withMedia() async throws -> (AppModel, FakeStation) {
        let station = try FakeStation(additions: .wideband)
        let suite = "PanSheetTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(mediaPeerFactory: station.mediaPeerFactory,
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        _ = model.main.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        #expect(await settle { model.main.band.spanHz == FakeStation.ddcRateHz })
        return (model, station)
    }

    @Test("Extended view On lets the band zoom out past the receiver to half the ADC rate and no further; Off zooms back in")
    func extendedViewZoomsOut() async throws {
        let (model, station) = try await withMedia()
        let band = model.main.band
        let ddc = FakeStation.ddcRateHz
        let widest = FakeStation.adcRateHz / 2
        #expect(band.spanLimits(sampleRateHz: ddc).upperBound == ddc)

        model.main.pan.toggleExtendedView()
        #expect(await settle { band.extendedSpanCeilingHz == widest })
        #expect(subscribes(station).last?["extendedView"] == .bool(true))
        #expect(band.spanLimits(sampleRateHz: ddc).upperBound == widest)

        // Asked for more than the ADC gives, the band asks for its half rate.
        band.requestView(TuneGestures.View(centerHz: band.centerHz, spanHz: widest * 4))
        #expect(await settle { band.spanHz == widest })
        #expect(span(subscribes(station).last) == widest)
        // The Core's rows across the wider span reach the band.
        station.sendDisplayRows(3)
        #expect(await settle { band.state.frame?.traceDbm.count == band.requestedPixels })

        // A tap in a wing tunes through the ordinary path.
        let wing = band.centerHz + widest / 4
        model.main.slices.tap(to: wing)
        #expect(await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.properties.first?.name == "frequency" && write.properties.first?.value == .f64(wing)
            }
            return false
        } != nil)

        // Off: back to the receiver's span.
        model.main.pan.toggleExtendedView()
        #expect(await settle { band.spanHz == ddc })
        #expect(span(subscribes(station).last) == ddc)
        #expect(subscribes(station).last?["extendedView"] == nil)
        #expect(band.extendedSpanCeilingHz == nil)
        #expect(band.spanLimits(sampleRateHz: ddc).upperBound == ddc)
        await model.disconnect()
    }

    @Test("with the extended view unavailable the band keeps the receiver's ceiling")
    func extendedViewUnavailable() async throws {
        let (model, station) = try await withMedia()
        station.widebandAvailable = false
        let band = model.main.band
        model.main.pan.toggleExtendedView()
        #expect(await settle { self.subscribes(station).last?["extendedView"] == .bool(true) })
        await quiet()
        #expect(band.extendedSpanCeilingHz == nil)
        band.requestView(TuneGestures.View(centerHz: band.centerHz, spanHz: 10_000_000))
        await quiet()
        #expect(band.spanLimits(sampleRateHz: FakeStation.ddcRateHz).upperBound == FakeStation.ddcRateHz)
        #expect(span(subscribes(station).last) == FakeStation.ddcRateHz)
        #expect(band.spanHz == FakeStation.ddcRateHz)
        await model.disconnect()
    }
}
