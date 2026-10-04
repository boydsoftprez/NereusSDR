// NereusSDR for iOS: pictures of the catalogue's ranges on screen: the TX panel per radio, NR settings, the TX zero line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// The TX panel on a Hermes Lite 2 and on a 100 W radio, each with its own
/// power ranges and readouts from the Core's catalogue; NR2's settings as
/// the Core describes them; and the band with the TX zero line on slice B,
/// the transmit slice, while A is active. The catalogues are the link's
/// conformance suite's, read at run time (D4). With
/// `NEREUS_CATALOGUE_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_CATALOGUE_SHOTS`), each picture is written there.
@Suite("Catalogue ranges on screen", .serialized)
@MainActor
struct CatalogueRangesShotTests {
    @Test("the TX panel shows each radio's own power range and readout", arguments: [
        ModesTabBindingTests.hermesLite, ModesTabBindingTests.anan,
    ])
    func txPanel(_ fixture: String) async throws {
        let (model, station) = try await connected(fixture)
        let transmit = model.main.transmit
        let hermesLite = fixture == ModesTabBindingTests.hermesLite
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: 2, name: "power", value: .i64(hermesLite ? 48 : 50)),
            .init(ordinal: 28, name: "tunePowerForTxBand", value: .i64(hermesLite ? 51 : 20)),
        ])))
        #expect(await ShotWait.until { transmit.rfPower != nil && transmit.powerControl.shown != nil })
        if hermesLite {
            #expect(transmit.powerControl.text(48) == "-3.5 dB")
        }
        let size = CGSize(width: TxPanel.width, height: 1_000)
        try await shoot("tx-panel-\(hermesLite ? "hermes-lite-2" : "anan-g2")", size: size) {
            TxPanel(transmit: transmit, accessories: model.main.accessories, micLevel: model.main.micLevel, modes: model.main.modes,
                    meters: model.main.band.catalog?.meters, scrolls: false)
        }
        await model.disconnect()
    }

    @Test("NR2's settings as the Core describes them, enabled")
    func noiseReductionSettings() async throws {
        let (model, _) = try await connected(ModesTabBindingTests.anan)
        let modes = model.main.modes
        #expect(await ShotWait.until { modes.noiseReduction?["nr2"] != nil && modes.nrValues["nr2GainMethod"] != nil })
        try await shoot("nr-settings-nr2", size: CGSize(width: 402, height: 560)) {
            VStack(alignment: .leading, spacing: 9) {
                NnrSettingsSection(model: modes, rx: modes.rx, open: "nr2")
            }
            .padding(10)
            .frame(maxHeight: .infinity, alignment: .top)
            .background(ChromeColours.panel)
        }
        await model.disconnect()
    }

    @Test("the TX zero line on slice B, the transmit slice, while A is active")
    func zeroLineOnTheTransmitSlice() async throws {
        let catalog = BandFlagShotTests.catalogue()
        let store = MirrorStore(send: { _ in })
        for index in 0..<2 {
            store.apply(BandFlagShotTests.slice(index, active: index == 0))
        }
        let slices = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        let band = BandModel()
        band.catalog = catalog
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "CatalogueRangesShotTests") ?? .standard)
        let size = CGSize(width: 402, height: 640)
        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: slices, settings: settings, txZeroLineSliceId: 1,
                             foreign: ForeignSlicesModel(store: MirrorStore(send: { _ in })))
        }
        .frame(width: size.width, height: size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        #expect(slices.activeSliceId == 0)
        write("band-zero-line-on-slice-b", window: window)
    }

    // MARK: Inside

    /// The app connected to a fake Core that sends `fixture`'s catalogue.
    private func connected(_ fixture: String) async throws -> (AppModel, FakeStation) {
        let station = try FakeStation()
        let defaults = try #require(UserDefaults(suiteName: "CatalogueRangesShotTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.connection == .connected })
        let json = try #require(ModesTabBindingTests.catalogueJSON(fixture))
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await ShotWait.until { model.main.catalogFeed.catalog?.board.transmit != nil })
        return (model, station)
    }

    private func shoot(_ name: String, size: CGSize, @ViewBuilder content: () -> some View) async throws {
        let window = try BandFlagShotTests.window(size: size)
        let host = UIHostingController(rootView: content()
            .frame(width: size.width, height: size.height, alignment: .top)
            .preferredColorScheme(.dark))
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        write(name, window: window)
    }

    private func write(_ name: String, window: UIWindow) {
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_CATALOGUE_SHOTS"], !directory.isEmpty,
              let data = image.pngData() else {
            return
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try? data.write(to: url)
        print("Wrote \(url.path)")
    }
}
