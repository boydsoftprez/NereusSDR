// NereusSDR for iOS: the Pan 1 and Display sheets on screen, newer and older Cores, for comparing with picture 25
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

/// D73: the real main screen with the Pan 1 or Display sheet open, connected
/// to a fake Core that is, scene by scene, the newer Core (every addition),
/// the Core JJ's first install talks to (no `bands`, no band select, notch
/// control 1, display extras 1), or older still. With `NEREUS_MAIN_SHOTS`
/// set to a directory (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each
/// screen is written there as a PNG for comparing with
/// `25-pan-and-display-sheets.jpg`.
@Suite("Pan and Display sheets on screen", .serialized)
@MainActor
struct DropSheetShotTests {
    @Test("Pan 1 stays usable after this phone's last slice closes")
    func emptyPanShot() async throws {
        let suite = "DropSheetShotTests-empty-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(mediaPeerFactory: { IdlePeer() }, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle(seconds: 5) { model.main.pan.sliceLetter == "A" })
        await station.deliver(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:0", className: "SliceModel")))
        #expect(await settle(seconds: 5) { model.main.pan.sliceLetter == nil && model.main.pan.canAddSlice })
        #expect(!model.main.pan.canSelectBand)
        #expect(!model.main.pan.canAddNotch)
        try await shoot("pan-sheet-empty-slice", sheet: .pan, sideways: false, model: model, feed: { _ in })
        await model.disconnect()
    }

    /// What the Core offers in one scene.
    struct Core {
        var bands: Bool
        var capabilities: [String: Int64]
    }

    static let newer = Core(bands: true, capabilities: [
        "remoteMediaVersion": 1, "remoteWidebandDisplayVersion": 1, "displayExtrasVersion": 2,
        "notchControlVersion": 2, "bandSelectVersion": 1,
    ])
    /// Integration 0426fe6b, on the Pi for the first listening test.
    static let listeningBuild = Core(bands: false, capabilities: [
        "remoteMediaVersion": 1, "remoteWidebandDisplayVersion": 1, "displayExtrasVersion": 1,
        "notchControlVersion": 1, "bandSelectVersion": 0,
    ])
    static let bandsWithoutSelect = Core(bands: true, capabilities: listeningBuild.capabilities)
    static let noExtras = Core(bands: false, capabilities: [
        "remoteMediaVersion": 1, "remoteWidebandDisplayVersion": 0, "displayExtrasVersion": 0,
        "notchControlVersion": 1, "bandSelectVersion": 0,
    ])

    @Test("the Pan 1 and Display sheets, upright and sideways, on newer and older Cores")
    func sheetShots() async throws {
        let suite = "DropSheetShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(mediaPeerFactory: { IdlePeer() }, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        // Slice A is on 40 m, as the board draws it.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
        ])))
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        // The board's Display values: Fill 70, Top -40, Range 100, Clarity.
        model.main.changeDisplay {
            $0.traceFillOpacity = 0.7
            $0.extendedView = true
        }
        let base = model.mirror.capabilities
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))

        var revision: Int64 = 2
        func offer(_ core: Core) async throws {
            var properties = base.keys.sorted().compactMap { name -> LinkMessage.PropertyEntry? in
                guard core.capabilities[name] == nil, let value = base[name] else {
                    return nil
                }
                return LinkMessage.PropertyEntry(name: name, value: value.wireValue)
            }
            for (name, value) in core.capabilities.sorted(by: { $0.key < $1.key }) {
                properties.append(.init(name: name, value: .i64(value)))
            }
            await station.deliver(.capabilities(LinkMessage.Capabilities(properties: properties)))
            revision += 1
            let json = try #require(MainScreenTests.catalogueJSON())
            await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
                .init(ordinal: 0, name: "json", value: .utf8(FakeStation.catalogue(json, bands: core.bands))),
                .init(ordinal: 1, name: "revision", value: .i64(revision)),
            ])))
            #expect(await settle(seconds: 30) {
                (model.main.catalogFeed.catalog?.bands != nil) == core.bands
                    && model.mirror.capabilityVersion("displayExtrasVersion") == core.capabilities["displayExtrasVersion"]
            })
            try await Task.sleep(for: .milliseconds(100))
        }

        try await offer(Self.newer)
        #expect(model.main.pan.extendedViewOn)
        try await shoot("pan-sheet-upright", sheet: .pan, sideways: false, model: model)
        try await shoot("display-sheet-upright", sheet: .display, sideways: false, model: model)
        try await shoot("pan-sheet-sideways", sheet: .pan, sideways: true, model: model)
        try await shoot("display-sheet-sideways", sheet: .display, sideways: true, model: model)

        try await offer(Self.listeningBuild)
        #expect(model.main.pan.bandGrid == .needsNewerCore)
        try await shoot("pan-sheet-listening-build-no-bands", sheet: .pan, sideways: false, model: model)
        try await shoot("display-sheet-listening-build", sheet: .display, sideways: false, model: model)

        try await offer(Self.bandsWithoutSelect)
        try await shoot("pan-sheet-grid-greyed", sheet: .pan, sideways: false, model: model)

        try await offer(Self.noExtras)
        #expect(!model.main.display.extrasAvailable)
        try await shoot("display-sheet-extras-greyed", sheet: .display, sideways: false, model: model)
        await model.disconnect()
    }

    @Test("the Band plan and Size rows, the picker open, a plan picked, and the strip at Small and Huge")
    func bandPlanShots() async throws {
        let suite = "DropSheetShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(mediaPeerFactory: { IdlePeer() }, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: .all)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let display = model.main.display
        #expect(await settle(seconds: 30) { !display.plans.isEmpty })

        try await shoot("54d-display-sheet-band-plan", sheet: .display, sideways: false, model: model)
        display.planPickerOpen = true
        try await shoot("54d-band-plan-picker-open", sheet: .display, sideways: false, model: model)
        // Picked, the Core echoes it and the tick moves.
        let picked = try #require(display.plans.first { !$0.isDefault })
        display.pickPlan(picked)
        #expect(await settle(seconds: 30) { display.shownPlan?.id == picked.id })
        try await shoot("54d-band-plan-picked", sheet: .display, sideways: false, model: model)
        // A plan the Core refuses: its words in the picker, the tick left.
        let refused = try #require(display.plans.first { $0.id != picked.id && !$0.isDefault })
        station.refuseNext(FakeStation.bandPlanKey, reason: FakeStation.unknownBandPlanReason)
        display.pickPlan(refused)
        #expect(await settle(seconds: 30) { display.planNote == FakeStation.unknownBandPlanReason })
        #expect(display.shownPlan?.id == picked.id)
        try await shoot("54d-band-plan-refused", sheet: .display, sideways: false, model: model)
        display.planPickerOpen = false

        // The Core's plan back to the default, then the strip at each size.
        display.pickPlan(try #require(display.plans.first { $0.isDefault }))
        #expect(await settle(seconds: 30) { display.shownPlan?.isDefault == true })
        for size in [BandPlanSize.small, .huge] {
            display.setBandPlanSize(size)
            try await shoot("54d-band-plan-\(size.rawValue)", sheet: nil, sideways: false, model: model)
        }
        display.setBandPlanSize(.small)
        await model.disconnect()
    }

    @Test("Extended view On, zoomed out past the receiver, with the Core's rows across the wider span")
    func extendedViewZoomedOut() async throws {
        let suite = "DropSheetShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let station = try FakeStation(additions: .all)
        let model = AppModel(mediaPeerFactory: station.mediaPeerFactory,
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        #if DEBUG
        let receipts = ExtendedViewReceipts()
        let receiptBand = model.main.band
        let priorBefore = model.beforeMediaEventForTesting
        let priorAfter = model.afterMediaEventForTesting
        model.beforeMediaEventForTesting = { event in receipts.observe(event, stage: .before, band: receiptBand) }
        model.afterMediaEventForTesting = { event in receipts.observe(event, stage: .after, band: receiptBand) }
        defer {
            model.beforeMediaEventForTesting = priorBefore
            model.afterMediaEventForTesting = priorAfter
            receipts.mark(.final, band: receiptBand)
            receipts.noteMessages(station.messages)
            receipts.printSummary()
        }
        receipts.mark(.installed, band: receiptBand)
        #endif
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        let band = model.main.band
        model.main.pan.toggleExtendedView()
        #expect(await settle(seconds: 30) { band.extendedSpanCeilingHz == FakeStation.adcRateHz / 2 })
        let span = 8_000_000.0
        try await shoot("pan-extended-view-zoomed-out", sheet: nil, sideways: false, model: model) { bandDraw in
            band.requestView(TuneGestures.View(centerHz: 7_236_400, spanHz: span))
            #if DEBUG
            receipts.mark(.spanWaitEntry, band: band, requestedPixels: bandDraw.requestedPixels)
            #endif
            #expect(await settle(seconds: 30) { band.spanHz == span })
            #if DEBUG
            receipts.mark(.spanWaitReturn, band: band, requestedPixels: bandDraw.requestedPixels)
            receipts.mark(.burstEntry, band: band, requestedPixels: bandDraw.requestedPixels)
            #endif
            station.sendDisplayRows(2_400)
            #if DEBUG
            receipts.mark(.burstReturn, band: band, requestedPixels: bandDraw.requestedPixels)
            #endif
            // The rows arrive through the media client one at a time.
            for _ in 0..<400 where (band.state.frame?.encoderSequence ?? 0) < 2_400 {
                try await Task.sleep(for: .milliseconds(50))
            }
            #if DEBUG
            receipts.mark(.waitEnd, band: band, requestedPixels: bandDraw.requestedPixels)
            #endif
            #expect((band.state.frame?.encoderSequence ?? 0) >= 2_400)
            #expect(band.state.frame?.traceDbm.count == bandDraw.requestedPixels)
        }
        #if DEBUG
        receipts.mark(.beforeDisconnect, band: band)
        #endif
        await model.disconnect()
        #if DEBUG
        receipts.mark(.afterDisconnect, band: band)
        #endif
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

    /// Draws the main screen with `sheet` open. `feed` fills the band; the
    /// scene's synthetic rows by default.
    private func shoot(_ name: String, sheet: OpenSheet?, sideways: Bool, model: AppModel,
                       feed: ((ShotWait.BandDrawing) async throws -> Void)? = nil) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = SheetShotRoot(model: model, sheet: sheet).preferredColorScheme(.dark)
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
        if let feed {
            try await feed(bandDraw)
        } else {
            let band = model.main.band
            BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        }
        try await ShotWait.requireBandShown(model.main.band, in: window, after: bandDraw)
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
}

/// The app's root as `RootView` lays it out, with one sheet open.
private struct SheetShotRoot: View {
    @ObservedObject var model: AppModel
    let sheet: OpenSheet?

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, sheet: sheet)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

/// A media peer that never connects, so a Core that offers media starts
/// nothing on the network in these pictures.
private final class IdlePeer: MediaPeerConnection, @unchecked Sendable {
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

#if DEBUG
// Diagnostic-only: synchronous counters on the existing MainActor hooks. Clock
// reads occur at capped contexts/milestones, not on every one of the 2400 rows.
@MainActor
private final class ExtendedViewReceipts {
    enum Stage: String { case before, after }
    enum Phase: String {
        case installed, spanWaitEntry, spanWaitReturn, burstEntry, burstReturn, waitEnd
        case frame2400Before, frame2400After, beforeDisconnect, afterDisconnect, final
    }
    private struct Progress {
        let endpoint: UInt32
        let generation: UInt32
        var beforeCount = 0
        var afterCount = 0
        var beforeMax: UInt32 = 0
        var afterMax: UInt32 = 0
        var before2400 = false
        var after2400 = false
    }
    private let started = ContinuousClock.now
    private var progress: [Progress] = []
    private var contexts: [String] = []
    private var milestones: [String] = []
    private var operations: [String] = []
    private var beforeFrames = 0
    private var afterFrames = 0
    private var beforeContexts = 0
    private var afterContexts = 0
    private var omittedGenerations = 0
    private var omittedContexts = 0
    private var omittedMilestones = 0
    private var omittedOperations = 0
    private var subscribeCount = 0
    private var keyframeCount = 0

    func observe(_ event: MediaControlEvent, stage: Stage, band: BandModel) {
        switch event {
        case .context(let context):
            if stage == .before { beforeContexts += 1 } else { afterContexts += 1 }
            guard contexts.count < 16 else { omittedContexts += 1; return }
            contexts.append("context stage=\(stage.rawValue) elapsed=\(ContinuousClock.now - started) "
                + "endpoint=\(context.endpointId) generation=\(context.contextGeneration) revision=\(context.revision) "
                + "centreHz=\(context.centreHz) spanHz=\(context.spanHz) traceSamples=\(context.traceSamples) "
                + "waterfallSamples=\(context.waterfallSamples) fps=\(context.fps) framesPerLine=\(context.framesPerLine) "
                + "bandEndpoint=\(band.endpointId ?? 0) bandSpanHz=\(band.spanHz)")
        case .displayFrame(let frame):
            if stage == .before { beforeFrames += 1 } else { afterFrames += 1 }
            let found = progress.firstIndex { $0.endpoint == frame.endpointId && $0.generation == frame.contextGeneration }
            if found == nil {
                guard progress.count < 16 else { omittedGenerations += 1; return }
                progress.append(Progress(endpoint: frame.endpointId, generation: frame.contextGeneration))
            }
            let index = found ?? (progress.count - 1)
            var reached2400 = false
            if stage == .before {
                progress[index].beforeCount += 1
                progress[index].beforeMax = max(progress[index].beforeMax, frame.encoderSequence)
                if frame.encoderSequence >= 2_400, !progress[index].before2400 {
                    progress[index].before2400 = true
                    reached2400 = true
                }
            } else {
                progress[index].afterCount += 1
                progress[index].afterMax = max(progress[index].afterMax, frame.encoderSequence)
                if frame.encoderSequence >= 2_400, !progress[index].after2400 {
                    progress[index].after2400 = true
                    reached2400 = true
                }
            }
            if reached2400 {
                mark(stage == .before ? .frame2400Before : .frame2400After, band: band,
                     eventGeneration: frame.contextGeneration, eventSequence: frame.encoderSequence)
            }
        default: break
        }
    }

    func mark(_ phase: Phase, band: BandModel, requestedPixels: Int? = nil,
              eventGeneration: UInt32? = nil, eventSequence: UInt32? = nil) {
        let terminal = phase == .waitEnd || phase == .beforeDisconnect || phase == .afterDisconnect || phase == .final
        guard milestones.count < (terminal ? 32 : 24) else { omittedMilestones += 1; return }
        let frame = band.state.frame
        let revision = band.frameRevision
        var line = "milestone phase=\(phase.rawValue) elapsed=\(ContinuousClock.now - started) "
            + "bandEndpoint=\(band.endpointId ?? 0) centreHz=\(band.centerHz) spanHz=\(band.spanHz) "
            + "spanMatches=\(band.spanHz == 8_000_000) framePresent=\(frame != nil) committedSequence=\(frame?.encoderSequence ?? 0) "
            + "committedGeneration=\(frame?.contextGeneration ?? 0) committedEndpoint=\(frame?.endpointId ?? 0) "
            + "committedSerial=\(band.state.committedSerial) revisionPresent=\(revision != nil) frameRevision=\(revision ?? 0) "
            + "traceSamples=\(frame?.traceDbm.count ?? 0) beforeFrames=\(beforeFrames) afterFrames=\(afterFrames) "
            + "beforeContexts=\(beforeContexts) afterContexts=\(afterContexts)"
        if let requestedPixels { line += " requestedPixels=\(requestedPixels)" }
        if let eventGeneration { line += " eventGeneration=\(eventGeneration)" }
        if let eventSequence { line += " eventSequence=\(eventSequence)" }
        for item in progress {
            line += " progress(endpoint=\(item.endpoint),generation=\(item.generation),beforeCount=\(item.beforeCount),"
                + "beforeMax=\(item.beforeMax),afterCount=\(item.afterCount),afterMax=\(item.afterMax))"
        }
        milestones.append(line)
    }

    func noteMessages(_ messages: [LinkMessage]) {
        for message in messages {
            guard case .mediaControl(let control) = message else { continue }
            let payload = control.payload
            let operation: String
            if payload["op"] == .string("subscribe") {
                subscribeCount += 1
                operation = "subscribe"
            } else if payload["op"] == .string("keyframe") {
                keyframeCount += 1
                operation = "keyframe"
            } else { continue }
            guard operations.count < 32 else { omittedOperations += 1; continue }
            func number(_ key: String) -> Double {
                if case .number(let value)? = payload[key] { return value }
                return -1
            }
            operations.append("operation op=\(operation) endpoint=\(number("endpointId")) revision=\(number("revision")) "
                + "centreHz=\(number("centreHz")) spanHz=\(number("spanHz")) pixels=\(number("pixels")) "
                + "fps=\(number("fps")) framesPerLine=\(number("framesPerLine"))")
        }
    }

    func printSummary() {
        var lines = ["EXTENDED_VIEW_RECEIPT beforeFrames=\(beforeFrames) afterFrames=\(afterFrames) "
            + "beforeContexts=\(beforeContexts) afterContexts=\(afterContexts) subscribeCount=\(subscribeCount) "
            + "keyframeCount=\(keyframeCount) omittedGenerationEvents=\(omittedGenerations) "
            + "omittedContexts=\(omittedContexts) omittedMilestones=\(omittedMilestones) omittedOperations=\(omittedOperations)"]
        lines.append(contentsOf: contexts)
        lines.append(contentsOf: milestones)
        lines.append(contentsOf: operations)
        print(lines.joined(separator: "\n"))
    }
}
#endif
