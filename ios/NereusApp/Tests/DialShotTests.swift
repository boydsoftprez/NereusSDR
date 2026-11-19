// NereusSDR for iOS: the three tuning dials on screen, for comparing with picture 3
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

/// D12, spec section 5.1 item 8: the real main screen connected to a fake
/// Core with no dial (a new install), the knob on the waterfall, its step
/// menu open, the knob in a sheet, the thumbwheel, and the knob sideways.
/// With VOICE switched off (Tools > Voice Keyer) it is not drawn and PTT and
/// the thumbwheel's step box stay where they were.
/// With `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each screen is written there as a PNG
/// for comparing with `03-tuning-dial.jpg`.
@Suite("The tuning dials on screen", .serialized)
@MainActor
struct DialShotTests {
    @Test("no dial, the knob, its step menu, the sheet, the thumbwheel and the knob sideways")
    func dialShots() async throws {
        let suite = "DialShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        let model = AppModel(phoneSettings: settings, mediaPeerFactory: { QuietDialPeer() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 9, name: "rxAntenna", value: .utf8("ANT1")),
            .init(ordinal: 10, name: "txAntenna", value: .utf8("ANT1")),
        ])))
        let main = model.main
        #expect(await settle(seconds: 30) {
            main.slices.entries.count == 2 && main.coreName != nil && main.slices.entries.first?.stepHz == 100
                && main.band.catalog != nil
        })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        main.changeDisplay { $0.traceFillOpacity = 0.7 }
        let band = main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        #expect(settings.dialKind == .off)
        #expect(settings.voiceButtonShown)
        let offWithVoice = try await shoot("dial-off", model: model, probe: true)

        settings.dialKind = .waterfallKnob
        try await shoot("dial-knob", model: model)
        main.tuning.toggleDialStepMenu()
        try await shoot("dial-knob-steps", model: model)
        main.tuning.closeStepMenu()

        settings.dialKind = .sheetKnob
        main.tuning.openDialSheet(sliceId: 0)
        try await shoot("dial-sheet", model: model)
        main.tuning.closeDialSheet()

        settings.dialKind = .thumbwheel
        let withVoice = try await shoot("dial-thumbwheel", model: model, probe: true)

        // VOICE switched off on Tools > Voice Keyer: it is gone, and PTT and
        // the thumbwheel's step box keep their places, with no dial and with
        // the thumbwheel.
        settings.voiceButtonShown = false
        let withoutVoice = try await shoot("dial-thumbwheel-no-voice", model: model, probe: true)
        #expect(withVoice[Self.voiceId] != nil && withoutVoice[Self.voiceId] == nil)
        #expect(withVoice[Self.pttId] != nil && withVoice[Self.pttId] == withoutVoice[Self.pttId])
        #expect(withVoice[Self.stepId] != nil && withVoice[Self.stepId] == withoutVoice[Self.stepId])
        settings.dialKind = .off
        let offWithoutVoice = try await shoot("dial-off-no-voice", model: model, probe: true)
        #expect(offWithVoice[Self.voiceId] != nil && offWithoutVoice[Self.voiceId] == nil)
        #expect(offWithVoice[Self.pttId] != nil && offWithVoice[Self.pttId] == offWithoutVoice[Self.pttId])
        settings.voiceButtonShown = true

        settings.dialKind = .waterfallKnob
        try await shoot("dial-knob-sideways", model: model, sideways: true)

        // Fix round A: sideways the thumbwheel keeps its upright width, at
        // the right, on this phone and on an iPhone 17 Pro Max.
        settings.dialKind = .thumbwheel
        try await shoot("dial-thumbwheel-sideways", model: model, sideways: true)
        try await shoot("dial-thumbwheel-promax", model: model, size: Self.proMax)
        try await shoot("dial-thumbwheel-promax-sideways", model: model, sideways: true, size: Self.proMax)
        settings.dialKind = .waterfallKnob
        try await shoot("dial-knob-promax", model: model, size: Self.proMax)
        try await shoot("dial-knob-promax-sideways", model: model, sideways: true, size: Self.proMax)
        await model.disconnect()
    }

    /// The screen walk (PR 808): the main screen with VOICE beside PTT
    /// (no dial), above PTT (the thumbwheel), and with VOICE switched off
    /// (PTT and the step box closed up), at each phone shape, written as
    /// `dial-walk-<state>-<shape>.png`.
    @Test(arguments: WalkShape.all)
    func voiceAtEachShape(_ shape: WalkShape) async throws {
        let suite = "DialShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        let model = AppModel(phoneSettings: settings, mediaPeerFactory: { QuietDialPeer() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
        ])))
        let main = model.main
        #expect(await settle(seconds: 30) {
            main.slices.entries.count == 2 && main.coreName != nil && main.slices.entries.first?.stepHz == 100
                && main.band.catalog != nil
        })
        let band = main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let upright = shape.sideways ? CGSize(width: shape.screen.height, height: shape.screen.width) : shape.screen
        for (state, dial, voice) in [("voice-beside", DialKind.off, true), ("voice-above", DialKind.thumbwheel, true),
                                     ("no-voice", DialKind.off, false), ("no-voice-thumbwheel", DialKind.thumbwheel, false)] {
            settings.dialKind = dial
            settings.voiceButtonShown = voice
            let frames = try await shoot("dial-walk-\(state)-\(shape)", model: model, sideways: shape.sideways,
                                         size: upright, probe: true, type: shape.type, insets: shape.safeArea)
            #expect((frames[Self.voiceId] != nil) == voice, "\(state) \(shape)")
            // VOICE and PTT side by side or one above the other, never overlapping, and on screen.
            if let ptt = frames[Self.pttId], let button = frames[Self.voiceId] {
                #expect(!ptt.intersects(button), "\(state) \(shape)")
            }
        }
        await model.disconnect()
    }

    /// The PR 808 sheet walk: the main screen with the recorder's strip on
    /// Pan 1 and the thumbwheel on, with every sheet, flag menu and panel,
    /// band panel and tab that opens over the band, LIVE and then in the past
    /// (PAST paused), written as `sheet-walk-<shape>-<live|past>-<state>.png`.
    /// Runs only with `NEREUS_SHEET_WALK` set (`TEST_RUNNER_NEREUS_SHEET_WALK`)
    /// to `phone` (the four iPhone shapes) or `ipad` (upright and on its side,
    /// on an iPad simulator); otherwise it does nothing.
    @Test(arguments: SheetWalkShape.allCases)
    func sheetsOverTheBand(_ shape: SheetWalkShape) async throws {
        guard let wanted = ProcessInfo.processInfo.environment["NEREUS_SHEET_WALK"],
              wanted == (shape.iPad ? "ipad" : "phone") else {
            return
        }
        let suite = "DialShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        let model = AppModel(phoneSettings: settings, mediaPeerFactory: { QuietDialPeer() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 6, name: "stepHz", value: .i64(1000)),
        ])))
        let main = model.main
        #expect(await settle(seconds: 30) {
            main.slices.entries.count == 2 && main.coreName != nil && main.band.catalog != nil
        })
        let band = main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let flow = ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
        // The tester's screen: the thumbwheel with its step box beside PTT.
        settings.dialKind = .thumbwheel
        Self.offerRecordings(model)
        let dsr = model.dsr
        #expect(dsr.usable, "the recorder takes Save on the walk's Core")
        for time in ["live", "past"] {
            if time == "past" {
                // Ten minutes back and let go: the picture stays in the past.
                dsr.drag(from: dsr.top, by: -600 / dsr.secondsPerPoint)
                dsr.dragEnded()
                #expect(dsr.inPast)
            }
            for state in SheetWalkState.all(column: shape.column) {
                let name = "sheet-walk-\(shape.rawValue)-\(time)-\(state.name)"
                try await shootWalk(name, model: model, flow: flow, shape: shape, state: state)
            }
        }
        await model.disconnect()
    }

    /// The fake Core has no recorder: give the mirror the recordings capability
    /// at the recorder's version on top of what it advertised, and the
    /// recordings document with the recorder keeping, so Save opens.
    private static func offerRecordings(_ model: AppModel) {
        var properties: [LinkMessage.PropertyEntry] = model.mirror.capabilities.compactMap { name, value in
            let wire: LinkMessage.PropertyValue
            switch value {
            case .bool(let flag): wire = .bool(flag)
            case .int(let whole): wire = .i64(whole)
            case .double(let real): wire = .f64(real)
            case .text(let text): wire = .utf8(text)
            case .enumeration(let whole): wire = .enumeration(whole)
            }
            return name == RecordingsClient.capabilityName ? nil : .init(ordinal: 0, name: name, value: wire)
        }
        properties.append(.init(ordinal: 0, name: RecordingsClient.capabilityName,
                                value: .i64(RecordingsClient.dsrVersion)))
        model.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: properties)))
        model.recordings.handle(.stateChanged(.ready))
        RecordingFlagTests.publish(DsrTests.document(dsr: DsrTests.dsrPart()), mirror: model.mirror,
                                   client: model.recordings, revision: 1)
    }

    /// JJ's phone run of PR 808, round B, on the whole screen of the small
    /// and the big iPhone upright, written as `jj-b-<shape>-<state>.png`:
    /// the past drawing (the dim covers the waterfall beside the strip),
    /// SAVE open while the sound plays (two frames three seconds apart, the
    /// picture and the handles held), the Save panel on Slice B, the Save
    /// panel and the Kept notice on a Core with MP3 and on one without it,
    /// the Kept notice beside Roundtable's tags, and Slice B's full flag
    /// near the band's right edge, clear of the dBm arrows. Runs only with
    /// `NEREUS_JJ_REWIND_B` set (`TEST_RUNNER_NEREUS_JJ_REWIND_B`).
    @Test(arguments: [SheetWalkShape.smallUpright, .bigUpright])
    func jjRoundB(_ shape: SheetWalkShape) async throws {
        guard let wanted = ProcessInfo.processInfo.environment["NEREUS_JJ_REWIND_B"], !wanted.isEmpty else {
            return
        }
        let suite = "DialShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        let model = AppModel(phoneSettings: settings, mediaPeerFactory: { QuietDialPeer() },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: FakeStation.Additions.all.union(.roundtable))
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        let main = model.main
        #expect(await settle(seconds: 30) {
            main.slices.entries.count == 2 && main.coreName != nil && main.band.catalog != nil
        })
        // Slice B where its full flag reaches the band's right edge (7.2205 to 7.2685 MHz).
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_265_000)),
        ])))
        // A Roundtable room on slice A's frequency: its tag at the spectrum's foot.
        let room = "lsb-7236000"
        try await station.deliverRoundtable(FakeStation.RoundtableScene(
            rooms: [FakeStation.roundtableSummary(id: room, members: 2, talkers: ["W1AW"])],
            roomRecords: [room: [FakeStation.roundtableMember(ref: "a/1", callsign: "W1AW", seq: 0),
                                 FakeStation.roundtableReading(ref: "a/1", dbm: -73.5, t: 1_000),
                                 FakeStation.roundtableTurn(id: 11, start: 900, end: nil, talkers: ["W1AW"])]],
            sliceRooms: [0: room]))
        let band = main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        #expect(await settle(seconds: 10) { main.slices.entries.first { $0.id == 1 }?.slice.frequencyHz == 7_265_000 })
        let flow = ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
        Self.offerRecordings(model)
        let dsr = model.dsr
        var now = DsrTests.now
        dsr.clock = { now }
        #expect(dsr.usable)

        // The past: ten minutes back and let go; the Core takes the rewind.
        dsr.drag(from: dsr.top, by: -600 / dsr.secondsPerPoint)
        let asked = station.messages.count
        dsr.dragEnded()
        #expect(await Self.answer(station, "dsr.rewind", after: asked))
        let moment = try #require(dsr.viewTop)
        #expect(await settle(seconds: 10) { dsr.mine.contains(0) })
        var revision: Int64 = 2
        func publish(_ dsr: [String: Any] = DsrTests.dsrPart(rewound: DsrTests.rewound(Int64((moment * 1e9).rounded()))),
                     kept: [String: Any]? = nil, mp3: Bool? = nil) {
            var document = DsrTests.document(dsr: dsr)
            if let kept {
                document["kept"] = kept
            }
            if let mp3, var settings = document["settings"] as? [String: Any] {
                settings["mp3"] = mp3
                document["settings"] = settings
            }
            RecordingFlagTests.publish(document, mirror: model.mirror, client: model.recordings, revision: revision)
            revision += 1
            model.dsr.documentChanged()
        }
        publish()
        #expect(await settle(seconds: 10) { dsr.followsSound && dsr.playedMoment != nil })
        try await shootScreen("jj-b-\(shape.rawValue)-past-drawing", model: model, flow: flow, shape: shape)

        // SAVE open while the sound plays: three seconds on, the picture and the bracket hold.
        dsr.toggleSave(stripPoints: dsr.waterfallPoints)
        #expect(dsr.saving)
        let heldTop = dsr.viewTop
        let heldStretch = dsr.stretch
        try await shootScreen("jj-b-\(shape.rawValue)-save-playing-1", model: model, flow: flow, shape: shape)
        now += 3
        dsr.tick()
        #expect(dsr.viewTop == heldTop)
        #expect(dsr.stretch?.from == heldStretch?.from && dsr.stretch?.to == heldStretch?.to)
        try await shootScreen("jj-b-\(shape.rawValue)-save-playing-2", model: model, flow: flow, shape: shape)
        dsr.cancelSave()

        // A Core with no MP3 encoder: the Save panel says WAV and why.
        publish(mp3: false)
        dsr.toggleSave(stripPoints: dsr.waterfallPoints)
        #expect(dsr.soundTitle == "Sound only (WAV)")
        try await shootScreen("jj-b-\(shape.rawValue)-save-no-mp3", model: model, flow: flow, shape: shape)
        dsr.cancelSave()
        // Its Kept notice, beside Roundtable's tag.
        publish(kept: RecordingFlagTests.kept(seq: 7, reason: "wav"), mp3: false)
        try await shootScreen("jj-b-\(shape.rawValue)-kept-no-mp3", model: model, flow: flow, shape: shape)

        // A Core with MP3 (the fake): the Save panel says MP3; the Kept notice as the Core words it.
        publish(kept: RecordingFlagTests.kept(seq: 8, reason: "here"), mp3: true)
        dsr.toggleSave(stripPoints: dsr.waterfallPoints)
        #expect(dsr.soundTitle == "Sound only (MP3)")
        try await shootScreen("jj-b-\(shape.rawValue)-save-mp3", model: model, flow: flow, shape: shape)
        dsr.cancelSave()
        try await shootScreen("jj-b-\(shape.rawValue)-kept-mp3", model: model, flow: flow, shape: shape)

        // On Slice B: its full flag in the past, then the Save panel names it.
        await station.deliver(FlagButtonsShotTests.active(0, false))
        await station.deliver(FlagButtonsShotTests.active(1, true))
        #expect(await settle(seconds: 10) { main.slices.active?.slice.letter == "B" })
        try await shootScreen("jj-b-\(shape.rawValue)-flag-b", model: model, flow: flow, shape: shape)
        dsr.toggleSave(stripPoints: dsr.waterfallPoints)
        try await shootScreen("jj-b-\(shape.rawValue)-save-slice-b", model: model, flow: flow, shape: shape)
        dsr.cancelSave()
        await model.disconnect()
    }

    /// Answers the first `verb` the app sent after `after` messages, accepted.
    private static func answer(_ station: FakeStation, _ verb: String, after: Int) async -> Bool {
        let deadline = Date().addingTimeInterval(10)
        while Date() < deadline {
            for message in station.messages.dropFirst(after) {
                if case .commandInvoke(let invoke) = message, invoke.verb == verb {
                    await station.deliver(.commandResult(LinkMessage.CommandResult(
                        verb: verb, id: invoke.id, accepted: true, reason: "", affected: [], values: nil)))
                    return true
                }
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return false
    }

    /// Draws the whole main screen at `shape`, nothing opened or closed.
    private func shootScreen(_ name: String, model: AppModel, flow: ConnectionFlow, shape: SheetWalkShape) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: shape.size)
        window.windowLevel = .alert + 1
        let root = SheetWalkRoot(model: model, flow: flow, state: SheetWalkState(name: name), iPad: false)
            .environment(\.horizontalSizeClass, .compact)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        host.additionalSafeAreaInsets = shape.safeArea
        host.view.frame = CGRect(origin: .zero, size: shape.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        // The band's own waterfall full of rows, so the past picture shows whether it covers it.
        // In the past the band holds its own picture back, so its draw is waited for, not required.
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        model.main.band.reset()
        BandFlagShotTests.feed(model.main.band, width: bandDraw.requestedPixels, lines: Int(shape.size.height * 3))
        _ = await ShotWait.bandShown(model.main.band, in: window, after: bandDraw, seconds: 3)
        try await Task.sleep(for: .milliseconds(700))
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    /// Opens `state` over the band, draws the screen and closes it again.
    private func shootWalk(_ name: String, model: AppModel, flow: ConnectionFlow, shape: SheetWalkShape,
                           state: SheetWalkState) async throws {
        let main = model.main
        let dsr = model.dsr
        let size = shape.size
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: shape.sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = SheetWalkRoot(model: model, flow: flow, state: state, iPad: shape.iPad)
            .environment(\.horizontalSizeClass, shape.iPad ? .regular : .compact)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        host.additionalSafeAreaInsets = shape.safeArea
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            main.flagControls.close()
            main.tuning.closePad()
            main.tuning.closeStepMenu()
            if dsr.saving {
                dsr.cancelSave()
            }
            window.isHidden = true
            window.rootViewController = nil
        }
        if state.tab == .panadapter {
            let bandDraw = try await ShotWait.requireBandLaidOut(main.band, in: window)
            main.band.reset()
            BandFlagShotTests.feed(main.band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
            try await ShotWait.requireBandShown(main.band, in: window, after: bandDraw)
        }
        switch state.opens {
        case .flag(let popover):
            main.flagControls.toggle(popover)
        case .save:
            dsr.toggleSave(stripPoints: dsr.waterfallPoints)
        case .saveAndFlag:
            dsr.toggleSave(stripPoints: dsr.waterfallPoints)
            main.flagControls.toggle(.panel(0, .dsp))
        case .pad:
            main.tuning.openPad(sliceId: 0)
        case .stepMenu:
            main.tuning.toggleDialStepMenu()
        case .tap(let id):
            // SwiftUI builds its accessibility elements only with accessibility on.
            let accessibility = SetupTypedEntryTests.applicationAccessibility()
            SetupTypedEntryTests.setApplicationAccessibility(true)
            defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
            #expect(await settle(seconds: 10) { IPadShotTests.element(id, in: window) != nil }, "\(name)")
            _ = IPadShotTests.element(id, in: window)?.accessibilityActivate()
        case .none:
            break
        }
        try await Task.sleep(for: .milliseconds(700))
        if state.scrolledToFoot {
            // Every scrolling view at its foot: the last row is where PTT must stay clear of it.
            await ShotWait.laidOut(window)
            for scroll in Self.scrollViews(in: window) where scroll.contentSize.height > scroll.bounds.height {
                scroll.setContentOffset(CGPoint(x: 0, y: scroll.contentSize.height - scroll.bounds.height
                                                    + scroll.adjustedContentInset.bottom), animated: false)
            }
            try await Task.sleep(for: .milliseconds(300))
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        (view as? UIScrollView).map { [$0] } ?? [] + view.subviews.flatMap { scrollViews(in: $0) }
    }

    /// Task 61, fix round 1: a flick on the pop-up knob at 30 rad/s, then
    /// the coast drawn at nine moments on a test clock, `coast-0.png` at
    /// the release to `coast-8.png` where it has stopped: the grip keeps
    /// turning, each step shorter, and the frequency follows it.
    @Test("a flick's coast, frame by frame")
    func coastShots() async throws {
        let rig = try BandTuningTests.Rig()
        #expect(await settle(seconds: 30) { rig.slices.entries.count == 2 })
        let suite = "DialShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: defaults)
        var now = 1_000.0
        let spinner = DialSpinner(clock: { now }, drivesItself: false)
        let tuning = rig.tuning
        let size = CGSize(width: 402, height: 460)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = SheetKnob(tuning: tuning, slices: rig.slices, settings: settings, spinner: spinner)
            .fixedSize(horizontal: false, vertical: true)
            .frame(width: size.width, height: size.height, alignment: .top)
            .background(Color.black)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        spinner.touchDown(tuning: tuning, reversed: false)
        for _ in 0..<8 {
            now += 1.0 / 120
            spinner.follow(byRadians: 30.0 / 120)
        }
        spinner.release()
        #expect(spinner.isCoasting)
        let start = now
        let moments = [0.0, 0.1, 0.2, 0.35, 0.5, 0.75, 1.0, 1.3, 1.7]
        var frame = 0
        for (index, moment) in moments.enumerated() {
            while now < start + moment - 1e-9 {
                now += 1.0 / 120
                frame += 1
                spinner.frame(at: now)
            }
            try await Task.sleep(for: .milliseconds(150))
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            let hz = rig.slices.entries.first?.slice.frequencyHz ?? 0
            print("coast-\(index): t=\(moment) s, frame \(frame), angle \(spinner.angle) rad, \(hz) Hz, "
                + "coasting \(spinner.isCoasting)")
            if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
               let data = image.pngData() {
                try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent("coast-\(index).png"))
            }
        }
        #expect(!spinner.isCoasting)
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

    /// An iPhone 17 Pro Max's window, upright.
    static let proMax = CGSize(width: 440, height: 956)

    /// Draws the main screen upright or sideways, the band fed synthetic
    /// rows, in a window `size` wide upright (an iPhone 17 Pro's by default).
    static let pttId = "ptt"
    static let stepId = "dialStep"
    static let voiceId = "voiceKeyer.voice"

    /// With `probe`, the frames of PTT, the dial's step box and VOICE on
    /// screen, by accessibility identifier; one that is not drawn is left out.
    @discardableResult
    private func shoot(_ name: String, model: AppModel, sideways: Bool = false,
                       size upright: CGSize = CGSize(width: 402, height: 874),
                       probe: Bool = false, type: DynamicTypeSize = .large,
                       insets: UIEdgeInsets? = nil) async throws -> [String: CGRect] {
        // SwiftUI builds its accessibility elements only with accessibility on (SetupTypedEntryTests).
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        if probe {
            SetupTypedEntryTests.setApplicationAccessibility(true)
        }
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let size = sideways ? CGSize(width: upright.height, height: upright.width) : upright
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = DialShotRoot(model: model, sideways: sideways).preferredColorScheme(.dark).dynamicTypeSize(type)
        let host = ShotHosting.host(root)
        if let insets {
            host.additionalSafeAreaInsets = insets
        } else if sideways {
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
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        var frames: [String: CGRect] = [:]
        if probe {
            // PTT is always drawn; once it is there VOICE, when shown, is too.
            #expect(await settle(seconds: 10) { IPadShotTests.element(Self.pttId, in: window) != nil })
            for id in [Self.pttId, Self.stepId, Self.voiceId] {
                if let element = IPadShotTests.element(id, in: window) {
                    frames[id] = element.accessibilityFrame
                }
            }
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return frames
    }
}

/// The app's root as `RootView` lays it out, on the Panadapter tab, with
/// the band's upright width from the window as `RootView` gives it.
private struct DialShotRoot: View {
    @ObservedObject var model: AppModel
    let sideways: Bool

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main)
                TabBar(selection: .constant(.panadapter), sideways: sideways)
            }
            .environment(\.uprightBandWidth, proxy.uprightBandWidth)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

/// A media peer that never connects, so the fake Core starts nothing on
/// the network in these pictures.
private final class QuietDialPeer: MediaPeerConnection, @unchecked Sendable {
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

/// The screens the sheet walk draws (``DialShotTests/sheetsOverTheBand(_:)``):
/// the smallest and largest iPhone upright and sideways, and an iPad
/// upright and on its side, with each one's safe area.
enum SheetWalkShape: String, CaseIterable, Sendable {
    case smallUpright = "small-upright"
    case smallSideways = "small-sideways"
    case bigUpright = "big-upright"
    case bigSideways = "big-sideways"
    case iPadUpright = "ipad-upright"
    case iPadSideways = "ipad-sideways"

    var iPad: Bool { self == .iPadUpright || self == .iPadSideways }
    var sideways: Bool { self == .smallSideways || self == .bigSideways || self == .iPadSideways }
    /// The band's button column shows: the iPad, and the phone held sideways.
    var column: Bool { iPad || sideways }

    var size: CGSize {
        switch self {
        case .iPadUpright: return CGSize(width: 1024, height: 1366)
        case .iPadSideways: return CGSize(width: 1366, height: 1024)
        default:
            let shape = WalkShape(small: self == .smallUpright || self == .smallSideways, sideways: sideways,
                                  larger: false)
            return shape.screen
        }
    }

    var safeArea: UIEdgeInsets {
        switch self {
        case .iPadUpright: return UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        case .iPadSideways: return UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        default:
            return WalkShape(small: self == .smallUpright || self == .smallSideways, sideways: sideways,
                             larger: false).safeArea
        }
    }
}

/// One thing open over the band in the sheet walk, or a tab in its place.
struct SheetWalkState {
    enum Opens {
        case none
        case flag(FlagPopover)
        case save
        case saveAndFlag
        case pad
        case stepMenu
        /// A button tapped through its accessibility element.
        case tap(String)
    }

    let name: String
    var opens: Opens = .none
    var sheet: OpenSheet?
    var rxPanelOpen = false
    var txPanelOpen = false
    var roundtable: RoundtableSheet.Detent?
    var columnMenu: BandColumnMenu?
    var tab: AppTab = .panadapter
    /// The open sheet's or panel's rows scrolled to their foot.
    var scrolledToFoot = false

    static func all(column: Bool) -> [SheetWalkState] {
        var states: [SheetWalkState] = [
            .init(name: "band"),
            .init(name: "flag-dsp", opens: .flag(.panel(0, .dsp))),
            .init(name: "flag-dsp-foot", opens: .flag(.panel(0, .dsp)), scrolledToFoot: true),
            .init(name: "flag-audio", opens: .flag(.panel(0, .audio))),
            .init(name: "flag-audio-foot", opens: .flag(.panel(0, .audio)), scrolledToFoot: true),
            .init(name: "flag-mode", opens: .flag(.panel(0, .mode))),
            .init(name: "flag-mode-foot", opens: .flag(.panel(0, .mode)), scrolledToFoot: true),
            .init(name: "flag-xrit", opens: .flag(.panel(0, .xrit))),
            .init(name: "flag-xrit-foot", opens: .flag(.panel(0, .xrit)), scrolledToFoot: true),
            .init(name: "flag-antennas", opens: .flag(.antennas(0))),
            .init(name: "flag-more", opens: .flag(.more(0))),
            .init(name: "save", opens: .save),
            .init(name: "save-flag-dsp", opens: .saveAndFlag),
            .init(name: "pad", opens: .pad),
            .init(name: "step-menu", opens: .stepMenu),
            .init(name: "voice-panel", opens: .tap("voiceKeyer.voice")),
            .init(name: "sound", opens: .tap(Toolbar.Item.speaker.rawValue)),
            .init(name: "sheet-pan", sheet: .pan),
            .init(name: "sheet-display", sheet: .display),
            .init(name: "sheet-display-foot", sheet: .display, scrolledToFoot: true),
            .init(name: "sheet-slices", sheet: .slices),
            .init(name: "rx-panel", rxPanelOpen: true),
            .init(name: "tx-panel", txPanelOpen: true),
            .init(name: "roundtable", roundtable: .half),
            .init(name: "tab-modes", tab: .modes),
            .init(name: "tab-tools", tab: .tools),
            .init(name: "tab-radio", tab: .radio),
            .init(name: "tab-setup", tab: .setup),
        ]
        if column {
            for (name, menu) in [("band", BandColumnMenu.band), ("antenna", .antenna), ("display", .display),
                                 ("vax", .vax), ("attenuator", .attenuator)] {
                states.append(.init(name: "column-\(name)", columnMenu: menu))
            }
        }
        return states
    }
}

/// The app's root as `RootView` lays it out: every tab in one stack, the
/// one chosen shown, the main screen with the walk's sheet or panel open.
private struct SheetWalkRoot: View {
    @ObservedObject var model: AppModel
    let flow: ConnectionFlow
    let state: SheetWalkState
    let iPad: Bool

    var body: some View {
        GeometryReader { proxy in
            let sideways = proxy.size.width > proxy.size.height
            VStack(spacing: 0) {
                ZStack {
                    MainScreen(app: model, main: model.main, rxPanelOpen: state.rxPanelOpen,
                               txPanelOpen: state.txPanelOpen,
                               sheet: state.sheet, columnMenu: state.columnMenu, roundtableSheet: state.roundtable)
                        .tabShown(state.tab == .panadapter)
                    switch state.tab {
                    case .panadapter:
                        EmptyView()
                    case .modes:
                        ModesTab(app: model, main: model.main, flow: flow)
                    case .tools:
                        ToolsTab(app: model, flow: flow, spots: model.spots)
                    case .radio:
                        RadioView(app: model, main: model.main, flow: flow)
                    case .setup:
                        SetupTab(app: model, flow: flow, router: SetupRouter(), buildTag: nil)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                TabBar(selection: .constant(state.tab), sideways: sideways, iPad: iPad)
            }
            .environment(\.uprightBandWidth, proxy.uprightBandWidth)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
