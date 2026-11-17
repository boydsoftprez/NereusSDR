// NereusSDR for iOS: the flag's Record and Play, the playing bar and the Kept notice on screen, for comparing with the mockups
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Recording plan Task 16 (A16 for the iPhone): slice A's flag with the
/// column ×, lock, Record, Play, ··· drawn by the real band, flag, column,
/// playing bar and Kept notice, fed a mirror with the Core's catalogue and
/// a recordings document. With `NEREUS_BAND_SHOTS` set to a directory
/// (through `TEST_RUNNER_NEREUS_BAND_SHOTS`), each frame is written there as
/// `flagbtn-rec-<name>.png`, for comparing with the phone and
/// recording-together mockups.
@Suite("Recording flag on screen", .serialized)
@MainActor
struct RecordingFlagShotTests {
    enum Frame: String, CaseIterable, Sendable, CustomStringConvertible {
        case idle
        case recording
        case armed
        case playing
        case onair
        case kept
        case olderCore = "older-core"

        var description: String { "flagbtn-rec-\(rawValue)" }
        static let size = CGSize(width: 402, height: 640)
    }

    @Test(arguments: Frame.allCases)
    func theFrameShowsRecordAndPlay(_ frame: Frame) async throws {
        let mirror = MirrorStore(send: { _ in })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        var capabilities: [LinkMessage.PropertyEntry] = [
            .init(ordinal: 0, name: BandSlicesModel.remoteTxCapability, value: .i64(1)),
            .init(ordinal: 2, name: "stationCatalogVersion", value: .i64(1)),
        ]
        if frame != .olderCore {
            capabilities.append(.init(ordinal: 3, name: RecordingsClient.capabilityName, value: .i64(3)))
        }
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        let json = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        mirror.apply(.objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: "StationCatalog",
                                                           properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
        ])))
        mirror.apply(FlagButtonsShotTests.slice(0, hz: 7_236_400, active: true))
        let main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: nil,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in },
                                                                          retuneClarity: { _ in }))

        // The Core's recordings, as the session would feed them.
        let commands = CommandClient(send: { _ in })
        let client = RecordingsClient(mirror: mirror, commands: commands)
        await commands.handle(.stateChanged(.receivingSnapshot))
        client.handle(.stateChanged(.receivingSnapshot))
        await commands.handle(.stateChanged(.ready))
        client.handle(.stateChanged(.ready))
        let model = RecordingFlagModel(client: client)
        model.blinks = false
        let state: String
        switch frame {
        case .recording:
            state = "recording"
        case .armed:
            state = "armed"
        default:
            state = "idle"
        }
        if frame != .olderCore {
            RecordingFlagTests.publish(RecordingFlagTests.document(state: state, elapsedMs: 12_400,
                                                                   kept: RecordingFlagTests.kept(seq: 4)),
                                       mirror: mirror, client: client, revision: 1)
            model.keptChanged()
            model.received(RecordingsPage(entries: RecordingEntry.list(json: RecordingFlagTests.entriesJSON) ?? [],
                                          offset: 0, total: 2, reader: nil), slice: 0)
        }
        switch frame {
        case .playing, .onair:
            client.handle(.message(.commandResult(LinkMessage.CommandResult(
                verb: RecordingsClient.playbackVerb, id: 0, accepted: true, reason: "", affected: [],
                values: [.init(name: "id", value: .utf8("r-9")), .init(name: "positionMs", value: .i64(12_000)),
                         .init(name: "lengthMs", value: .i64(205_000)), .init(name: "playing", value: .bool(true)),
                         .init(name: "onAir", value: .bool(frame == .onair))]))))
        case .kept:
            RecordingFlagTests.publish(RecordingFlagTests.document(kept: RecordingFlagTests.kept(seq: 5)),
                                       mirror: mirror, client: client, revision: 2)
            model.keptChanged()
        case .olderCore:
            await model.tapRecord(slice: 0)
        default:
            break
        }

        let band = main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        #expect(await ShotWait.until { main.slices.entries.count == 1 && main.slices.catalog != nil })
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "RecordingFlagShotTests") ?? .standard)

        let window = try BandFlagShotTests.window(size: Frame.size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: main.slices, settings: settings, tuning: main.tuning,
                             foreign: main.foreign, flagControls: main.flagControls, recordings: model)
        }
        .frame(width: Frame.size.width, height: Frame.size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: Frame.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(Frame.size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        #expect(await ShotWait.until { main.slices.flagRects.count == 1 })

        // What the frame is for is there.
        let look = model.look(slice: 0)
        switch frame {
        case .idle:
            #expect(look.record == .idle && look.play == .idle && look.playReason == nil)
        case .recording:
            #expect(look.record == .recording(elapsedMs: 12_400))
        case .armed:
            #expect(look.record == .armed)
        case .playing:
            #expect(model.bar(slice: 0)?.detail == "0:12 / 3:25 · speaker")
        case .onair:
            #expect(model.bar(slice: 0)?.detail == "ON AIR · 0:12 / 3:25")
        case .kept:
            #expect(model.notice(slice: 0) != nil)
        case .olderCore:
            #expect(look.recordReason == RecordingsClient.updateReason)
            #expect(model.notice(slice: 0) == .reason(RecordingsClient.updateReason))
        }
        await ShotWait.laidOut(window)
        try RadeFlagShotTests.write(window, name: frame.description)
    }
}
