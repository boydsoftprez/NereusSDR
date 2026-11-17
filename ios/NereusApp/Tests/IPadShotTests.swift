// NereusSDR for iOS: the iPad on its side and upright, and Setup beside its page, for comparing with the boards
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 19, the ipad-recordings shots: the pan's
//   Record, Play, playing bar and VOICE, the column's Recordings panel, and
//   the Recordings and Recording settings pages, at the mockup's 13-inch
//   size. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: Screen walk for PR 808, the Recordings shots without the
//   column, upright and one text step larger. J.J. Boyd (KG4VCF),
//   AI-assisted via Claude Code.

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

/// R-IOS-24, D30, D31: the real main screen and tab bar on an 11-inch iPad,
/// connected to a fake Core with the ANAN-G2 catalogue and two slices where
/// the board draws them. On its side the band gives the applet column its
/// 320 points and takes them back when the column hides; upright the band
/// keeps the whole width over the front panel. With `NEREUS_IPAD_SHOTS` set
/// to a directory (through `TEST_RUNNER_NEREUS_IPAD_SHOTS`), each screen is
/// written there as a PNG for comparing with `18-ipad-landscape.jpg` and
/// `19-ipad-upright.jpg`. It runs on an iPad simulator only.
@Suite("iPad on screen", .serialized)
@MainActor
struct IPadShotTests {
    struct Screen {
        let name: String
        let onItsSide: Bool
        let columnShown: Bool
        /// The band column's open menu, or the Display pill (R-IOS-58).
        var columnMenu: BandColumnMenu? = nil
        /// With two pans, the pan whose column has `columnMenu` open.
        var columnPan: PanIdentity = .first
        var displayTab: DisplayPillTab = .spectrum
        /// On its side, a window shorter than the screen (Stage Manager).
        var height: CGFloat? = nil
        /// Another iPad's screen on its side (the recording mockup's 13-inch).
        var sideways: CGSize? = nil
        /// The text size (the screen walk draws one step larger too).
        var type: DynamicTypeSize = .large

        /// The 11-inch iPad's screen, either way up.
        var size: CGSize {
            if onItsSide, let sideways {
                return sideways
            }
            return onItsSide ? CGSize(width: 1210, height: height ?? 834) : CGSize(width: 834, height: 1210)
        }

        /// The band's width in the layout, in points.
        var bandWidth: CGFloat {
            let layout: IPadLayout = onItsSide ? .column : .frontPanel
            return layout.bandWidth(screenWidth: size.width, columnShown: columnShown)
        }
    }

    @Test("the iPad on its side with the column, without it, and upright with the front panel",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          })
    func iPadShots() async throws {
        let defaults = try #require(UserDefaults(suiteName: "IPadShotTests"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        // The board's reading: S8, -79 dBm, on slice A's flag and on the S-meter.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-79)),
            .init(ordinal: 16, name: "signalPeakDbm", value: .f64(-79)),
        ])))
        #expect(await settle(seconds: 30) {
            model.main.slices.entries.count == 2 && model.main.coreName != nil
                && model.main.slices.active.map { model.main.slices.reading($0.id).signalPeakDbm } == -79
        })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })

        let band = model.main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))

        for screen in [Screen(name: "ipad-landscape", onItsSide: true, columnShown: true),
                       Screen(name: "ipad-landscape-column-hidden", onItsSide: true, columnShown: false),
                       Screen(name: "ipad-upright", onItsSide: false, columnShown: true)] {
            try await shoot(screen, model: model)
        }
        await model.disconnect()
    }

    /// Recording plan Task 19 (design sections 3 and 6.8, A16 for the iPad):
    /// the 13-inch iPad on its side as the mockup has it, with slice A's
    /// flag column (×, lock, Record, Play, ···), the playing bar under the
    /// flag and VOICE beside PTT, and the Recordings panel in the column
    /// under the S-meter: off (the default), idle, recording, playing, on
    /// the air, a voice message going out and an older Core; then Tools >
    /// Recordings and Setup > Recording settings, the phone's pages, at the
    /// iPad's width. Written as `ipad-recordings-<name>.png`.
    @Test("recordings on the iPad: the flag, the playing bar, VOICE, the Recordings panel and the pages",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func recordingsShots() async throws {
        let mockup = CGSize(width: 1376, height: 1032)
        for older in [false, true] {
            let suite = "IPadShotTests-recordings-\(UUID().uuidString)"
            let defaults = try #require(UserDefaults(suiteName: suite))
            defer { defaults.removePersistentDomain(forName: suite) }
            let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
            let station = try FakeStation()
            await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                                transportFactory: station.transportFactory)
            #expect(await station.waitUntilLive())
            try await MainScreenShotTests.fill(station)
            #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
            let band = model.main.band
            band.endpointId = 1
            band.receive(.context(try #require(BandFlagShotTests.context())))
            Self.offerRecordings(model, version: older ? nil : 3)
            model.recordingFlag.blinks = false
            let settings = model.phoneSettings
            let entries = RecordingsPagesTests.entries()

            if older {
                settings.recordingsAppletShown = true
                #expect(model.recordingsApplet.reason == RecordingsClient.updateReason)
                try await shoot(Screen(name: "ipad-recordings-older-core", onItsSide: true, columnShown: true,
                                       sideways: mockup), model: model)
                await model.disconnect()
                continue
            }
            Self.publishRecordings(model, state: "idle", revision: 1)
            model.recordingsApplet.actions.received(entries)
            model.recordingsList.received(entries)
            model.recordingFlag.received(RecordingsPage(entries: entries.filter { $0.slice == 0 && $0.kind == .contact },
                                                        offset: 0, total: 1, reader: nil), slice: 0)

            settings.recordingsAppletShown = false
            try await shoot(Screen(name: "ipad-recordings-panel-off", onItsSide: true, columnShown: true,
                                   sideways: mockup), model: model)
            settings.recordingsAppletShown = true
            #expect(model.recordingsApplet.rows.count == entries.count)
            try await shoot(Screen(name: "ipad-recordings-idle", onItsSide: true, columnShown: true, sideways: mockup),
                            model: model)
            // The screen walk (PR 808): without the column, upright, and one text step larger.
            try await shoot(Screen(name: "ipad-recordings-walk-idle-column-hidden", onItsSide: true,
                                   columnShown: false, sideways: mockup), model: model)
            try await shoot(Screen(name: "ipad-recordings-walk-idle-upright", onItsSide: false, columnShown: true),
                            model: model)
            try await shoot(Screen(name: "ipad-recordings-walk-idle-larger", onItsSide: true, columnShown: true,
                                   sideways: mockup, type: .xLarge), model: model)

            Self.publishRecordings(model, state: "recording", revision: 2)
            #expect(model.recordingsApplet.rows.first == .recording(slice: 0, elapsedMs: 12_400))
            try await shoot(Screen(name: "ipad-recordings-recording", onItsSide: true, columnShown: true,
                                   sideways: mockup), model: model)
            Self.publishRecordings(model, state: "idle", revision: 3)

            for onAir in [false, true] {
                Self.playback(model, id: "r-4", positionMs: 12_000, onAir: onAir)
                #expect(model.recordingFlag.bar(slice: 0)?.detail
                    == (onAir ? "ON AIR · 0:12 / 3:25" : "0:12 / 3:25 · speaker"))
                try await shoot(Screen(name: onAir ? "ipad-recordings-onair" : "ipad-recordings-playing",
                                       onItsSide: true, columnShown: true, sideways: mockup), model: model)
                if !onAir {
                    try await shoot(Screen(name: "ipad-recordings-walk-playing-upright", onItsSide: false,
                                           columnShown: true), model: model)
                    try await shoot(Screen(name: "ipad-recordings-walk-playing-column-hidden", onItsSide: true,
                                           columnShown: false, sideways: mockup), model: model)
                }
            }
            Self.playback(model, id: "r-4", positionMs: 0, onAir: false, playing: false)

            Self.publishRecordings(model, state: "idle", revision: 4,
                                   sending: VoiceKeyerTests.sending(2, deviceId: "another-device", by: "Shack Mac"))
            try await shoot(Screen(name: "ipad-recordings-voice-sending", onItsSide: true, columnShown: true,
                                   sideways: mockup), model: model)
            Self.publishRecordings(model, state: "idle", revision: 5)

            try await shootPage("ipad-recordings-tools-page", size: mockup) {
                VStack(spacing: 0) {
                    ConnectChrome.NavBar(title: "Recordings", back: "Tools") {
                        RecordingsListPage.Trailing(model: model.recordingsList)
                    }
                    RecordingsListPage(model: model.recordingsList)
                }
            }
            try await shootPage("ipad-recordings-settings-page", size: mockup, tab: .setup) {
                NavigationStack {
                    RecordingSettingsPage(model: model.recordingSettings, phone: settings)
                }
            }
            let upright = CGSize(width: mockup.height, height: mockup.width)
            try await shootPage("ipad-recordings-walk-tools-page-upright", size: upright) {
                VStack(spacing: 0) {
                    ConnectChrome.NavBar(title: "Recordings", back: "Tools") {
                        RecordingsListPage.Trailing(model: model.recordingsList)
                    }
                    RecordingsListPage(model: model.recordingsList)
                }
            }
            try await shootPage("ipad-recordings-walk-settings-page-upright", size: upright, tab: .setup) {
                NavigationStack {
                    RecordingSettingsPage(model: model.recordingSettings, phone: settings)
                }
            }
            try await shootPage("ipad-recordings-walk-voice-keyer-page", size: mockup) {
                VStack(spacing: 0) {
                    ConnectChrome.NavBar(title: "Voice Keyer", back: "Tools") {
                        SetupTagBadge(tag: .core)
                    }
                    VoiceKeyerPage(model: model.voiceKeyer, take: model.main.take, settings: settings)
                }
            }
            await model.disconnect()
        }
    }

    /// Re-sends the fake Core's capabilities with `recordingsVersion` at
    /// `version` (none when nil), as a Core with recordings advertises it,
    /// and has the app's recordings client read them.
    private static func offerRecordings(_ model: AppModel, version: Int64?) {
        var entries: [LinkMessage.PropertyEntry] = []
        for (name, value) in model.mirror.capabilities.sorted(by: { $0.key < $1.key })
        where name != RecordingsClient.capabilityName {
            let wire: LinkMessage.PropertyValue
            switch value {
            case .bool(let flag): wire = .bool(flag)
            case .int(let whole): wire = .i64(whole)
            case .double(let real): wire = .f64(real)
            case .text(let text): wire = .utf8(text)
            case .enumeration(let raw): wire = .enumeration(raw)
            }
            entries.append(.init(ordinal: UInt16(entries.count), name: name, value: wire))
        }
        if let version {
            entries.append(.init(ordinal: UInt16(entries.count), name: RecordingsClient.capabilityName,
                                 value: .i64(version)))
        }
        model.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: entries)))
        model.recordings.handle(.stateChanged(.ready))
    }

    /// The Core's recordings document with slice A `state`, the totals the
    /// pages read, and the voice keyer's messages, one going out when `sending`.
    private static func publishRecordings(_ model: AppModel, state: String, revision: Int64,
                                          sending: [String: Any]? = nil) {
        var document = RecordingsPagesShotTests.document()
        document["slices"] = RecordingFlagTests.document(state: state, elapsedMs: 12_400)["slices"]
        document["voiceKeyer"] = VoiceKeyerTests.document(sending: sending)["voiceKeyer"]
        RecordingFlagTests.publish(document, mirror: model.mirror, client: model.recordings, revision: revision)
    }

    /// This iPad's playback, as the Core pushes it.
    private static func playback(_ model: AppModel, id: String, positionMs: Int64, onAir: Bool, playing: Bool = true) {
        model.recordings.handle(.message(.commandResult(LinkMessage.CommandResult(
            verb: RecordingsClient.playbackVerb, id: 0, accepted: true, reason: "", affected: [],
            values: [.init(name: "id", value: .utf8(id)), .init(name: "positionMs", value: .i64(positionMs)),
                     .init(name: "lengthMs", value: .i64(205_000)), .init(name: "playing", value: .bool(playing)),
                     .init(name: "onAir", value: .bool(onAir))]))))
    }

    /// One page over the iPad's tab bar, on its side, written with `NEREUS_IPAD_SHOTS` set.
    private func shootPage<Content: View>(_ name: String, size: CGSize, tab: AppTab = .tools,
                                          @ViewBuilder content: () -> Content) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: 120), size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(tab), sideways: size.width > size.height, iPad: true)
        }
        .background(ChromeColours.page)
        .environment(\.horizontalSizeClass, .regular)
        .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    /// R-IOS-58, DC7, DC8: the band column at the band's top-left, on its
    /// side and upright, with each of its menus open in turn and the Display
    /// pill on each of its five tabs, on a Core that describes Display at
    /// version 29 and opens bands for the phone; for comparing with the
    /// board's iPad frames. The PTT and the keyed meters sit right of
    /// whatever is open.
    @Test("the band column on the iPad with each menu open and the pill on each tab",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func bandColumnShots() async throws {
        let suite = "IPadShotTests-column-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: [.bands, .bandSelect, .notchAtSlice])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        try await Self.describeDisplayPill(model, station)

        try await shoot(Screen(name: "ipad-column", onItsSide: true, columnShown: true), model: model)
        let menus: [(BandColumnMenu, String)] = [(.band, "band"), (.antenna, "antenna"), (.vax, "vax"),
                                                 (.attenuator, "attenuator")]
        for (menu, name) in menus {
            try await shoot(Screen(name: "ipad-column-\(name)", onItsSide: true, columnShown: true, columnMenu: menu),
                            model: model)
            try await shoot(Screen(name: "ipad-upright-column-\(name)", onItsSide: false, columnShown: true,
                                   columnMenu: menu), model: model)
        }
        for tab in DisplayPillTab.allCases {
            try await shoot(Screen(name: "ipad-column-pill-\(tab.rawValue)", onItsSide: true, columnShown: true,
                                   columnMenu: .display, displayTab: tab), model: model)
        }
        try await shoot(Screen(name: "ipad-upright-column-pill-spectrum", onItsSide: false, columnShown: true,
                               columnMenu: .display), model: model)
        await model.disconnect()
    }

    /// The Band Stack applet (band stack spec section 8): upright under the
    /// S-meter in the first column, on its side in the column, and the BAND
    /// menu beside it; then a Core with no band stacks, where it stays, greyed.
    @Test("the Band Stack applet upright and on its side, on a Core with band stacks and an older one",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func bandStackAppletShots() async throws {
        for older in [false, true] {
            let suite = "IPadShotTests-stack-\(UUID().uuidString)"
            let defaults = try #require(UserDefaults(suiteName: suite))
            defer { defaults.removePersistentDomain(forName: suite) }
            let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
            let additions: FakeStation.Additions = older ? [.bands, .bandSelect, .notchAtSlice]
                : [.bands, .bandSelect, .notchAtSlice, .bandStack]
            let station = try FakeStation(additions: additions)
            let stack = model.main.controls(for: .first).bandStack
            await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                                transportFactory: station.transportFactory)
            #expect(await station.waitUntilLive())
            try await MainScreenShotTests.fill(station)
            #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
            let band = model.main.band
            band.endpointId = 1
            band.receive(.context(try #require(BandFlagShotTests.context())))
            let json = try #require(MainScreenTests.catalogueJSON())
            await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
                .init(ordinal: 1, name: "revision", value: .i64(2)),
            ])))
            await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
                .init(ordinal: 14, name: "band", value: .enumeration(3)),
            ])))
            #expect(await settle(seconds: 30) {
                older ? stack.availability == .needsNewerCore : stack.currentBand == "40m" && stack.stack != nil
            })
            let tag = older ? "-older-core" : ""
            try await shoot(Screen(name: "ipad-upright-band-stack\(tag)", onItsSide: false, columnShown: true),
                            model: model)
            try await shoot(Screen(name: "ipad-column-band-stack\(tag)", onItsSide: true, columnShown: true),
                            model: model)
            if !older {
                try await shoot(Screen(name: "ipad-upright-band-stack-band-menu", onItsSide: false, columnShown: true,
                                       columnMenu: .band), model: model)
            }
            await model.disconnect()
        }
    }

    /// R-IOS-59, DC9, DC10: two pans on the iPad, Pan 1 on top and Pan 2
    /// below sharing the band's height, each with its own column; Pan 1's
    /// pill open, then Pan 2's, the toolbar naming the pan last touched;
    /// for comparing with the board's "Two pans, two pills".
    @Test("two pans on the iPad, each pill open in turn",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func twoPanShots() async throws {
        // The pans' status line stays up while the shots draw (it shows 3.2 s on the band).
        let noteDuration = BandButtonColumn.noteDuration
        BandButtonColumn.noteDuration = .seconds(600)
        defer { BandButtonColumn.noteDuration = noteDuration }
        let suite = "IPadShotTests-pans-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: [.bands, .bandSelect, .notchAtSlice])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        try await Self.describeDisplayPill(model, station)
        let stack = model.main.panStack
        #expect(stack.allowsMorePans)
        stack.addPan()
        #expect(await settle(seconds: 30) { stack.status != nil && stack.open.count == 2 })
        let second = PanIdentity(number: 2)
        let pan2 = stack.band(for: second)
        #expect(await settle(seconds: 30) { !pan2.slices.entries.isEmpty })
        // Pan 2's slice on 40 m inside its view, with a receiver's rate.
        let sliceId = try #require(pan2.slices.entries.first?.id)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:\(sliceId)", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_247_500)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        for band in [model.main.band, pan2.band] {
            band.endpointId = 1
            band.receive(.context(try #require(BandFlagShotTests.context())))
        }
        try await shootPans(Screen(name: "ipad-two-pans-pill-1", onItsSide: true, columnShown: true,
                                   columnMenu: .display, columnPan: .first), model: model, touched: .first)
        #expect(model.main.toolbarPan == .first)
        try await shootPans(Screen(name: "ipad-two-pans-pill-2", onItsSide: true, columnShown: true,
                                   columnMenu: .display, columnPan: second), model: model, touched: second)
        #expect(model.main.toolbarPan == second)
        try await shootPans(Screen(name: "ipad-upright-two-pans-pill-2", onItsSide: false, columnShown: true,
                                   columnMenu: .display, columnPan: second), model: model, touched: second)
        // L3: in a short window Pan 2's flag, tuned near the band's left,
        // reaches down into the PTT's row; the PTT keeps right of it.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:\(sliceId)", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_227_500)),
        ])))
        #expect(await settle(seconds: 30) { pan2.slices.entries.first?.slice.frequencyHz == 7_227_500 })
        try await shootPans(Screen(name: "ipad-two-pans-ptt-past-pan-2-flag", onItsSide: true, columnShown: true,
                                   height: 600), model: model, touched: second, pttClearOf: "flagB")
        await model.disconnect()
    }

    /// A Core that refuses Pan 2's display: Pan 2 says so in the Core's
    /// words, in the house card over its band, and Pan 1 says nothing.
    @Test("a pan whose display the Core refused says so in the Core's words",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func refusedPanShot() async throws {
        // The pans' status line stays up while the shot draws, to show it clear of the refusal.
        let noteDuration = BandButtonColumn.noteDuration
        BandButtonColumn.noteDuration = .seconds(600)
        defer { BandButtonColumn.noteDuration = noteDuration }
        let suite = "IPadShotTests-refused-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let station = try FakeStation(additions: .wideband)
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults), mediaPeerFactory: station.mediaPeerFactory,
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        model.main.panStack.allowsMorePans = true
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle(seconds: 30) { model.main.catalogFeed.catalog != nil && model.main.pan.sliceLetter == "A" })
        _ = model.main.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        let words = "The Core has no display free for another pan."
        // Pan 1 subscribes on endpoint 1; Pan 2's first is the next.
        station.refuseNext(FakeStation.subscribeRefusalKey(2), reason: words)
        let stack = model.main.panStack
        stack.addPan()
        let second = PanIdentity(number: 2)
        #expect(await settle(seconds: 30) { stack.status != nil && station.panKey(ofSlice: 1) == "pan-1" })
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_213_000)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        _ = stack.band(for: second).band.prepareToDraw(size: CGSize(width: 1206, height: 700), scale: 3)
        #expect(await settle(seconds: 30) { stack.displayRefusals[second] == words })

        // SwiftUI builds its accessibility elements only with accessibility on (SetupTypedEntryTests).
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let size = CGSize(width: 1210, height: 834)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: 120), size: size)
        window.windowLevel = .alert + 1
        let root = IPadShotRoot(model: model, columnShown: true)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        _ = try await ShotWait.requireBandLaidOut(stack.band(for: second).band, in: window)
        let notice = await settleElement(seconds: 10, "panDisplayRefusal", in: window)
        #expect(notice != nil)
        #expect(notice?.accessibilityLabel == words)
        // The status line at Pan 2's lower right, clear of the refusal at its waterfall's top.
        let status = await settleElement(seconds: 10, "bandColumnPanStatus", in: window)
        #expect(status != nil)
        if let status, let notice {
            #expect(!status.accessibilityFrame.intersects(notice.accessibilityFrame))
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("ipad-two-pans-pan-2-refused.png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        await model.disconnect()
    }

    private func settleElement(seconds: Double, _ id: String, in window: UIWindow) async -> NSObject? {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if let found = Self.element(id, in: window) {
                return found
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return Self.element(id, in: window)
    }

    /// SetupTypedEntryTests' search without its 20,000-node cap: two pans'
    /// bands hold more elements than one Setup page.
    static func element(_ id: String, in root: NSObject) -> NSObject? {
        var queue: [NSObject] = [root]
        var next = 0
        while next < queue.count, next < 500_000 {
            let node = queue[next]
            next += 1
            if SetupTypedEntryTests.identifier(node) == id {
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

    /// Draws the two pans with `touched` the pan last touched, each band fed.
    /// With `pttClearOf`, a flag's accessibility identifier, the flag must
    /// reach the PTT's row and the PTT must not cover it.
    private func shootPans(_ screen: Screen, model: AppModel, touched: PanIdentity,
                           pttClearOf flagId: String? = nil) async throws {
        model.main.panStack.lastTouched = touched
        // SwiftUI builds its accessibility elements only with accessibility on (SetupTypedEntryTests).
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        if flagId != nil {
            SetupTypedEntryTests.setApplicationAccessibility(true)
        }
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: screen.onItsSide ? 120 : 0), size: screen.size)
        window.windowLevel = .alert + 1
        let root = IPadShotRoot(model: model, columnShown: screen.columnShown, columnMenu: screen.columnMenu,
                                columnPan: screen.columnPan, displayTab: screen.displayTab)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        if screen.onItsSide {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        }
        host.view.frame = CGRect(origin: .zero, size: screen.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bands = model.main.panStack.open.map { model.main.panStack.band(for: $0).band }
        #expect(bands.count == 2)
        for band in bands {
            let drawn = try await ShotWait.requireBandLaidOut(band, in: window)
            BandFlagShotTests.feed(band, width: drawn.requestedPixels, lines: Int(screen.size.height * 2))
            try await ShotWait.requireBandShown(band, in: window, after: drawn)
        }
        if let flagId {
            let flag = await settleElement(seconds: 10, flagId, in: window)
            let ptt = await settleElement(seconds: 10, "ptt", in: window)
            #expect(flag != nil && ptt != nil)
            if let flag, let ptt {
                let flagFrame = flag.accessibilityFrame
                let pttFrame = ptt.accessibilityFrame
                // The flag reaches the PTT's row, so the check means something.
                #expect(flagFrame.maxY > pttFrame.minY && flagFrame.minY < pttFrame.maxY)
                #expect(!flagFrame.intersects(pttFrame))
            }
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(screen.name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    /// The Core describes Display at version 29 with display extras 4, as
    /// DropSheetShotTests' pill shots have it, so the Display pill draws its
    /// rows; waits until the pill has the description.
    static func describeDisplayPill(_ model: AppModel, _ station: FakeStation) async throws {
        var properties = model.mirror.capabilities.keys.sorted().compactMap { name -> LinkMessage.PropertyEntry? in
            guard name != "setupDescriptionVersion", name != "displayExtrasVersion",
                  let value = model.mirror.capabilities[name] else { return nil }
            return LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        properties.append(.init(name: "setupDescriptionVersion", value: .i64(29)))
        // A Core with media on sends display extras 4
        // (src/core/session/StationServer.cpp:4776-4784).
        properties.append(.init(name: "displayExtrasVersion", value: .i64(4)))
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: properties)))
        let json = try String(contentsOf: DisplayPillTests.setupFolder.appendingPathComponent("display.json"),
                              encoding: .utf8)
        await station.deliverSetup([(id: "display", json: json)])
        let deadline = Date().addingTimeInterval(30)
        while model.displayPill.description == nil, Date() < deadline {
            try? await Task.sleep(for: .milliseconds(50))
        }
        #expect(model.displayPill.description != nil)
    }

    /// R-IOS-52, W8: Setup on the iPad on its side, the list beside DSP's
    /// AGC/ALC page with the page held to a readable width, compared with
    /// setup-board.html's chosen iPad frame; then the same router in a
    /// narrow, compact-width window: one list with the page slid in, still
    /// AGC/ALC.
    @Test("Setup on the iPad on its side beside its page, and the same page in a compact window",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          })
    func setupShots() async throws {
        typealias Described = SetupDescribedPagesTests
        let rig = try await Described.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup(try Described.coreCategories())
        let app = rig.app
        #expect(await Described.settle { app.setupPages.isCurrent && app.setupFeed.description(for: "dsp") != nil })
        let numbers = try SetupWorkspaceShotTests.numberRowIds(app, category: "dsp", page: "dsp.agcAlc")
        let agcAlc = SetupWorkspaceShotTests.agcAlc
        rig.router.path = agcAlc

        let wide = CGSize(width: 1210, height: 834)
        let split = try await shootSetup("ipad-setup-split", rig: rig, size: wide, widthClass: .regular)
        // Each number row is on the right of the list, inside the readable width.
        let left = SetupSplit.listWidth + (wide.width - SetupSplit.listWidth - SetupSplit.readableWidth) / 2
        var measured = 0
        for id in numbers {
            guard let label = split.frame(id, .label), let value = split.frame(id, .value) else {
                continue
            }
            measured += 1
            #expect(label.minX >= left - 1, "\(id): the row is not held to the readable width")
            #expect(value.maxX <= left + SetupSplit.readableWidth + 1, "\(id): the row runs past the readable width")
        }
        #expect(measured > 0, "No number row was laid out beside the list")
        #expect(rig.router.path == agcAlc)

        // The window narrowed to compact: one list, the same page still open.
        let narrow = CGSize(width: 375, height: 834)
        let compact = try await shootSetup("ipad-setup-compact", rig: rig, size: narrow, widthClass: .compact)
        #expect(rig.router.path == agcAlc, "The open page closed when the window narrowed")
        let row = try #require(numbers.lazy.compactMap { compact.frame($0, .label) }.first,
                               "AGC/ALC is not open in the compact window")
        #expect(row.minX < SetupSplit.listWidth, "The compact window still has the list beside the page")
        await app.disconnect()
    }

    /// Draws the Setup tab and the tab bar in a window `size` wide with the
    /// width class `widthClass`, as the app's root lays them out, and returns
    /// where the page's value rows were laid out.
    private func shootSetup(_ name: String, rig: SetupDescribedPagesTests.Rig, size: CGSize,
                            widthClass: UserInterfaceSizeClass) async throws -> SetupLayoutProbe {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        let onItsSide = size.width > size.height
        window.frame = CGRect(origin: CGPoint(x: 0, y: onItsSide ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let probe = SetupLayoutProbe()
        let iPad = IPadLayout.arrangement(horizontal: widthClass, size: size).isIPad
        let root = VStack(spacing: 0) {
            SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil)
            TabBar(selection: .constant(.setup), sideways: onItsSide, iPad: iPad)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .environment(\.horizontalSizeClass, widthClass)
        .environment(\.setupLayoutProbe, probe)
        .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        if onItsSide {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return probe
    }

    // MARK: Inside

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

    private func shoot(_ screen: Screen, model: AppModel) async throws {
        let size = screen.size
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        // On its side the window sits clear of the upright status bar and
        // home indicator, and takes the iPad's own on-its-side safe area.
        window.frame = CGRect(origin: CGPoint(x: 0, y: screen.onItsSide ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = IPadShotRoot(model: model, columnShown: screen.columnShown, columnMenu: screen.columnMenu,
                                displayTab: screen.displayTab)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
            .dynamicTypeSize(screen.type)
        let host = ShotHosting.host(root)
        if screen.onItsSide {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
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
        // The band asks for its width in pixels: the column's 320 points are
        // the band's while the column hides, and upright it has the whole width.
        let scale = window.screen.scale
        #expect(band.requestedPixels == Int((screen.bandWidth * scale).rounded()))
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        #expect(band.markers.count == 2)

        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(screen.name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out on an iPad, the column shown or not.
private struct IPadShotRoot: View {
    @ObservedObject var model: AppModel
    let columnShown: Bool
    var columnMenu: BandColumnMenu?
    var columnPan: PanIdentity = .first
    var displayTab: DisplayPillTab = .spectrum

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, displayTab: displayTab, columnShown: columnShown,
                           columnMenu: columnMenu, columnPan: columnPan)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height, iPad: true)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
