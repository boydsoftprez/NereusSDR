// NereusSDR for iOS: a flag drag's frequency writes keep their pace while the band draws and the Core's readings flow
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreFoundation
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

/// JJ on build 8130, 2026-10-26: "When i move the flag on the iphone there is
/// a significant delay before it actually tunes now". The Zephyrus Core's log
/// showed a drag's writes arriving 85-160 ms apart on the earlier builds and
/// 210-420 ms apart on 8130, with runs exactly 5 s apart (the answer
/// deadline), while the Core answered each in 1-2 ms.
///
/// The real main screen, connected to a fake Core, draws the band with the
/// Core's lines flowing at 30 a second; the fake Core sends a reading for
/// each slice every 50 ms (the Core's delta flush), and answers each
/// frequency write 60 ms after it arrives (the Core's 2 ms and the
/// Zephyrus link's round trip). A flag drag then moves slice A for 3 s at
/// 120 touches a second and lifts. The drag's writes must keep a round
/// trip's pace, and the final value must reach the Core soon after the
/// finger lifts. Simulator numbers, printed for the record.
@Suite("Tuning while the band draws", .serialized)
@MainActor
struct TuneUnderLoadTests {
    /// The fake Core's answer time: its own 2 ms and a 58 ms round trip.
    nonisolated static let answerDelay: Duration = .milliseconds(60)
    /// The Core's delta flush period (StationServer.h kDefaultDeltaFlushMs).
    nonisolated static let readingPeriod: Duration = .milliseconds(50)
    /// How long the finger drags, and how often it moves.
    static let dragSeconds = 3.0
    static let touchPeriod = 1.0 / 120

    /// What keeps the main thread busy while the finger drags.
    enum Load: CustomStringConvertible, Sendable {
        /// Nothing on screen: the control.
        case nothing
        /// Nothing on screen, and a display link that holds the main thread
        /// this many ms of every 60 Hz frame: a draw's cost without a GPU.
        case mainHeld(ms: Double)
        /// The main screen with the band in 2D, or in 3D.
        case flat, stacked

        var description: String {
            switch self {
            case .nothing: return "nothing drawn"
            case .mainHeld(let ms): return "main held \(ms) ms a frame"
            case .flat: return "2D"
            case .stacked: return "3D"
            }
        }
    }

    @Test("with nothing drawn, a flag drag's writes keep a round trip's pace", .timeLimit(.minutes(3)))
    func idleDragKeepsPace() async throws {
        Self.check(try await Self.measure(.nothing))
    }

    /// The main thread held 15 of each 16.7 ms frame, with no GPU: on the
    /// simulator 12 ms keeps a 67 ms pace, 14 ms slows it to 165 ms and 15 ms
    /// to 0.9-2 s, as the Core's messages queue behind each other.
    @Test("with the main thread held 15 ms of each frame, a flag drag's writes keep a round trip's pace",
          .timeLimit(.minutes(3)))
    func heldMainDragKeepsPace() async throws {
        Self.check(try await Self.measure(.mainHeld(ms: 15)))
    }

    @Test("a 2D flag drag's writes keep a round trip's pace and the final value goes at once",
          .timeLimit(.minutes(3)))
    func flatDragKeepsPace() async throws {
        Self.check(try await Self.measure(.flat))
    }

    @Test("a 3D flag drag's writes keep a round trip's pace and the final value goes at once",
          .timeLimit(.minutes(3)))
    func stackedDragKeepsPace() async throws {
        Self.check(try await Self.measure(.stacked))
    }

    // MARK: Inside

    struct Run {
        let label: String
        /// Milliseconds between the drag's writes reaching the Core.
        let spacingsMs: [Double]
        /// From the finger lifting to the final value reaching the Core.
        let liftToFinalMs: Double?
        /// The main thread's busy spans during the drag, in ms.
        let busyMs: [Double]
        /// From an answer reaching the app's session to the next write
        /// reaching the Core, for writes that waited on an answer, in ms.
        let answerToNextMs: [Double]
        let dragMs: Double
        let writes: Int
        let linesDrawn: Int
    }

    static func check(_ run: Run) {
        let median = percentile(run.spacingsMs, 0.5)
        let worst = run.spacingsMs.max() ?? 0
        // The earlier builds' pace on JJ's phone: a median of 114-123 ms.
        #expect(run.writes >= 10, "\(run.label): only \(run.writes) writes in a \(dragSeconds) s drag")
        #expect(median <= 150, "\(run.label): writes \(Int(median)) ms apart (median)")
        #expect(worst < 1_000, "\(run.label): a write waited \(Int(worst)) ms")
        #expect((run.liftToFinalMs ?? .infinity) <= 250,
                "\(run.label): the final value reached the Core \(run.liftToFinalMs.map { "\(Int($0)) ms" } ?? "never") after the lift")
    }

    static func percentile(_ values: [Double], _ p: Double) -> Double {
        guard !values.isEmpty else { return 0 }
        let sorted = values.sorted()
        return sorted[min(sorted.count - 1, Int((Double(sorted.count - 1) * p).rounded()))]
    }

    /// The fake Core's side: answers each frequency write after
    /// ``answerDelay`` and notes when each arrived.
    final class Answers: @unchecked Sendable {
        private let lock = NSLock()
        private var arrivals: [(at: Double, hz: Double)] = []
        private var answers: [Double] = []
        private var stopped = false

        var writes: [(at: Double, hz: Double)] { lock.withLock { arrivals } }
        /// When the fake Core's answers reached the app's session.
        var answered: [Double] { lock.withLock { answers } }
        func noteAnswered() { lock.withLock { answers.append(now()) } }
        var isStopped: Bool { lock.withLock { stopped } }
        func stop() { lock.withLock { stopped = true } }
        func note(_ hz: Double) { lock.withLock { arrivals.append((now(), hz)) } }
    }

    /// Bounded, test-only timestamps across the fake transport and answer
    /// task. Aggregate spacing alone cannot distinguish app work from a
    /// delayed fake-Core observer or an overdue answer task.
    final class Boundaries: @unchecked Sendable {
        enum Stage: String {
            case sent, observed, answerStarted, answerDue, answerReady, deliver, received, handled, delivered, writerResumed
        }
        private let lock = NSLock()
        private var rows: [UInt32: [Stage: Double]] = [:]
        private var writeIds: [Double: UInt32] = [:]

        func sent(_ id: UInt32, hz: Double) {
            lock.withLock {
                rows[id, default: [:]][.sent] = TuneUnderLoadTests.now()
                writeIds[hz] = id
            }
        }

        func writerResumed(_ hz: Double) {
            lock.withLock {
                guard let id = writeIds[hz] else { return }
                rows[id, default: [:]][.writerResumed] = TuneUnderLoadTests.now()
            }
        }

        func note(_ stage: Stage, _ id: UInt32, at: Double = TuneUnderLoadTests.now()) {
            lock.withLock { rows[id, default: [:]][stage] = at }
        }

        func report(_ label: String, since began: Double) {
            let rows = lock.withLock { self.rows }
                .filter { ($0.value[.sent] ?? 0) >= began }
                .sorted { ($0.value[.sent] ?? 0) < ($1.value[.sent] ?? 0) }
            func ms(_ end: Double?, _ start: Double?) -> String {
                guard let end, let start else { return "-" }
                return String(format: "%.2f", (end - start) * 1_000)
            }
            for (index, row) in rows.enumerated() {
                let t = row.value
                let next = index + 1 < rows.count ? rows[index + 1].value[.sent] : nil
                print("TuneBoundary \(label) id \(row.key): "
                      + "send-observer \(ms(t[.observed], t[.sent])) "
                      + "observer-answer-task \(ms(t[.answerStarted], t[.observed])) "
                      + "answer-overdue \(ms(t[.answerReady], t[.answerDue])) "
                      + "ready-deliver \(ms(t[.deliver], t[.answerReady])) "
                      + "deliver-receive \(ms(t[.received], t[.deliver])) "
                      + "receive-handled \(ms(t[.handled], t[.received])) "
                      + "handled-delivered \(ms(t[.delivered], t[.handled])) "
                      + "receive-writer-resumed \(ms(t[.writerResumed], t[.received])) "
                      + "writer-next-send \(ms(next, t[.writerResumed])) "
                      + "receive-next-send \(ms(next, t[.received])) ms")
            }
        }
    }

    /// Forwards the same fake connection unchanged, recording local admission
    /// and session ingress before an event is handled. Decodes only property
    /// frames; recording does not schedule another task or main-queue hop.
    final class BoundaryTransport: LinkTransport {
        let base: any LinkTransport
        let boundaries: Boundaries

        init(base: any LinkTransport, boundaries: Boundaries) {
            self.base = base
            self.boundaries = boundaries
        }

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            try await base.open { [boundaries] event in
                var resultId: UInt32?
                if case .text(let text) = event, text.contains("property.result"),
                   case .propertyResult(let result) = try? LinkCodec.decode(text), result.key == "slice:0" {
                    resultId = result.writeId
                    boundaries.note(.received, result.writeId)
                }
                await onEvent(event)
                if let resultId { boundaries.note(.handled, resultId) }
            }
        }

        @discardableResult func send(_ text: String) -> Bool {
            if text.contains("property.write"), case .propertyWrite(let write) = try? LinkCodec.decode(text),
               write.key == "slice:0", write.properties.contains(where: { $0.name == "frequency" }),
               let id = write.writeId, let entry = write.properties.first(where: { $0.name == "frequency" }),
               case .f64(let hz) = entry.value {
                boundaries.sent(id, hz: hz)
            }
            return base.send(text)
        }

        func ping() { base.ping() }
        func close() { base.close() }
        func setBinaryReceiver(_ receiver: (@Sendable (Data) -> Void)?) { base.setBinaryReceiver(receiver) }
    }

    /// How long a job put on the main queue waits to run: one every 20 ms
    /// from a thread of its own.
    final class MainHops: @unchecked Sendable {
        private let lock = NSLock()
        private var waits: [Double] = []
        private var running = true

        var waitsMs: [Double] { lock.withLock { waits } }

        func start() {
            Thread.detachNewThread { [self] in
                while lock.withLock({ running }) {
                    let sent = TuneUnderLoadTests.now()
                    DispatchQueue.main.async { [self] in
                        let wait = (TuneUnderLoadTests.now() - sent) * 1_000
                        lock.withLock { waits.append(wait) }
                    }
                    Thread.sleep(forTimeInterval: 0.02)
                }
            }
        }

        func stop() { lock.withLock { running = false } }
    }

    /// Holds the main thread a set time on every 60 Hz frame, as a draw
    /// on the main thread does.
    final class MainHolder: NSObject {
        private var link: CADisplayLink?
        private var holdSeconds = 0.0

        func start(holdingMs ms: Double) {
            holdSeconds = ms / 1_000
            let link = CADisplayLink(target: self, selector: #selector(step))
            link.preferredFrameRateRange = CAFrameRateRange(minimum: 60, maximum: 60, preferred: 60)
            link.add(to: .main, forMode: .common)
            self.link = link
        }

        @objc private func step(_ link: CADisplayLink) {
            let until = TuneUnderLoadTests.now() + holdSeconds
            while TuneUnderLoadTests.now() < until {}
        }

        func stop() {
            link?.invalidate()
            link = nil
        }
    }

    nonisolated static func now() -> Double { ProcessInfo.processInfo.systemUptime }

    /// The main thread's busy spans: from waking to going back to sleep.
    final class MainBusy {
        private var observer: CFRunLoopObserver?
        private var woke: Double?
        private(set) var spans: [(start: Double, ms: Double)] = []

        func start() {
            let observer = CFRunLoopObserverCreateWithHandler(
                nil, CFRunLoopActivity.afterWaiting.rawValue | CFRunLoopActivity.beforeWaiting.rawValue, true, 0
            ) { [weak self] _, activity in
                guard let self else { return }
                let now = TuneUnderLoadTests.now()
                if activity == .afterWaiting {
                    self.woke = now
                } else if let woke = self.woke {
                    self.spans.append((woke, (now - woke) * 1_000))
                    self.woke = nil
                }
            }
            CFRunLoopAddObserver(CFRunLoopGetMain(), observer, .commonModes)
            self.observer = observer
        }

        func stop() {
            if let observer {
                CFRunLoopRemoveObserver(CFRunLoopGetMain(), observer, .commonModes)
            }
            observer = nil
        }
    }

    static func measure(_ load: Load) async throws -> Run {
        let suite = "TuneUnderLoadTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let station = try FakeStation(additions: .all)
        let model = AppModel(mediaPeerFactory: station.mediaPeerFactory,
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let boundaries = Boundaries()
        let factory = station.transportFactory
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: { endpoint, trust in
            BoundaryTransport(base: factory(endpoint, trust), boundaries: boundaries)
        }, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        let main = model.main
        main.slices.frequencyWriteAnsweredForTesting = { hz in boundaries.writerResumed(hz) }
        #expect(await settle(seconds: 30) {
            main.slices.entries.count == 2 && main.coreName != nil && main.band.catalog != nil
        })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        main.band.stackOffered = true
        main.changeDisplay {
            $0.spectrumView = load.isStacked ? .stacked : .flat
        }

        // The real main screen on a window, as the phone shows it.
        var window: UIWindow?
        if load.drawsBand {
            let size = CGSize(width: 402, height: 874)
            let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
            let shown = UIWindow(windowScene: scene)
            shown.frame = CGRect(origin: .zero, size: size)
            shown.windowLevel = .alert + 1
            let host = ShotHosting.host(LoadRoot(model: model).preferredColorScheme(.dark))
            host.view.frame = CGRect(origin: .zero, size: size)
            shown.rootViewController = host
            shown.isHidden = false
            window = shown
        }
        defer {
            window?.isHidden = true
            window?.rootViewController = nil
        }
        let holder = MainHolder()
        if case .mainHeld(let ms) = load {
            holder.start(holdingMs: ms)
        }
        defer { holder.stop() }

        // The Core's lines at 30 a second, and a reading for each slice every 50 ms.
        if load.drawsBand {
            #expect(await settle(seconds: 30) { main.band.spanHz > 0 })
        }
        let answers = Answers()
        let lines = Task.detached {
            while !answers.isStopped {
                station.sendDisplayRows(1)
                try? await Task.sleep(for: .milliseconds(33))
            }
        }
        let readings = Task.detached {
            var level = -110.0
            while !answers.isStopped {
                level = level > -60 ? -110 : level + 0.5
                for slice in 0 ..< 2 {
                    await station.deliver(.delta(LinkMessage.Delta(key: "slice:\(slice)", properties: [
                        .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(level)),
                    ])))
                }
                try? await Task.sleep(for: readingPeriod)
            }
        }
        let core = Task.detached {
            var seen = 0
            while !answers.isStopped {
                // Woken by each message the app sends, never by polling.
                let known = seen
                _ = await station.waitUntil(within: .milliseconds(500)) { station.messages.count > known }
                let messages = station.messages
                for message in messages.dropFirst(seen) {
                    guard case .propertyWrite(let write) = message, write.key == "slice:0",
                          let writeId = write.writeId, let entry = write.properties.first,
                          entry.name == "frequency", case .f64(let hz) = entry.value else {
                        continue
                    }
                    answers.note(hz)
                    boundaries.note(.observed, writeId)
                    Task.detached {
                        boundaries.note(.answerStarted, writeId)
                        let delay = answerDelay.components
                        let seconds = Double(delay.seconds) + Double(delay.attoseconds) / 1e18
                        boundaries.note(.answerDue, writeId, at: now() + seconds)
                        try? await Task.sleep(for: answerDelay)
                        boundaries.note(.answerReady, writeId)
                        boundaries.note(.deliver, writeId)
                        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: writeId,
                                                                                         results: [
                            .init(property: entry.name, accepted: true, reason: "", value: entry),
                        ])))
                        boundaries.note(.delivered, writeId)
                        answers.noteAnswered()
                        await station.deliver(.delta(LinkMessage.Delta(key: write.key, properties: [entry])))
                    }
                }
                seen = messages.count
            }
        }
        defer { answers.stop() }

        // Let the lines flow and the screen settle before the finger lands.
        try await Task.sleep(for: .seconds(2))
        let linesBefore = main.band.state.history.linesAppended
        let costBefore = main.band.drawCost
        let busy = MainBusy()
        busy.start()
        let hops = MainHops()
        hops.start()

        let startHz = try #require(main.slices.entries.first { $0.id == 0 }?.slice.frequencyHz)
        let began = now()
        var lastHz = startHz
        var touches = 0
        while now() - began < dragSeconds {
            touches += 1
            lastHz = startHz + Double(touches) * 100
            main.slices.drag(sliceId: 0, to: lastHz)
            try await Task.sleep(for: .seconds(touchPeriod))
        }
        let lifted = now()
        main.slices.finishDrag()
        let finalHz = lastHz
        _ = await station.waitUntil(within: .seconds(8)) {
            answers.writes.contains { $0.hz == finalHz }
        }
        let ended = now()
        busy.stop()
        hops.stop()
        let hopMs = hops.waitsMs
        let linesDrawn = Int(main.band.state.history.linesAppended &- linesBefore)
        let cost = Self.cost(main.band.drawCost, since: costBefore, over: ended - began)
        answers.stop()
        lines.cancel()
        readings.cancel()
        core.cancel()

        let writes = answers.writes.filter { $0.at >= began }
        let spacings = zip(writes, writes.dropFirst()).map { ($1.at - $0.at) * 1_000 }
        let final = writes.first { $0.hz == finalHz }
        let busyMs = busy.spans.filter { $0.start >= began && $0.start <= ended }.map(\.ms)
        // Each write after the first waited for the answer before it: the
        // latest answer that reached the session before the write left.
        let answered = answers.answered
        let answerToNext = writes.dropFirst().compactMap { write -> Double? in
            answered.last { $0 <= write.at }.map { (write.at - $0) * 1_000 }
        }
        let label = load.description
        let run = Run(label: label, spacingsMs: spacings, liftToFinalMs: final.map { ($0.at - lifted) * 1_000 },
                      busyMs: busyMs, answerToNextMs: answerToNext, dragMs: (lifted - began) * 1_000,
                      writes: writes.count, linesDrawn: linesDrawn)
        let busyTotal = busyMs.reduce(0, +)
        print("""
            TuneUnderLoad \(label) (simulator): touches \(touches), writes \(run.writes), \
            spacing ms p10 \(Int(percentile(spacings, 0.1))) median \(Int(percentile(spacings, 0.5))) \
            p90 \(Int(percentile(spacings, 0.9))) max \(Int(spacings.max() ?? 0)); \
            lift to final \(run.liftToFinalMs.map { "\(Int($0)) ms" } ?? "never"); \
            answer to next write ms median \(Int(percentile(answerToNext, 0.5))) \
            p90 \(Int(percentile(answerToNext, 0.9))) max \(Int(answerToNext.max() ?? 0)); \
            main busy \(Int(busyTotal * 100 / max((ended - began) * 1_000, 1)))% of \(Int((ended - began) * 1_000)) ms, \
            spans \(busyMs.count) p50 \(String(format: "%.1f", percentile(busyMs, 0.5))) \
            p90 \(String(format: "%.1f", percentile(busyMs, 0.9))) max \(String(format: "%.1f", busyMs.max() ?? 0)) ms; \
            main queue wait ms median \(String(format: "%.1f", percentile(hopMs, 0.5))) \
            p90 \(String(format: "%.1f", percentile(hopMs, 0.9))) max \(String(format: "%.1f", hopMs.max() ?? 0)); \
            lines in \(linesDrawn)\(cost)
            """)
        boundaries.report(label, since: began)
        await model.disconnect()
        return run
    }

    /// The band's draws between two readings of its costs over `seconds`:
    /// how many a second, what each cost the main thread on average, and
    /// the main thread's drawing time per 60 Hz frame (empty when nothing
    /// drew).
    static func cost(_ after: BandModel.DrawCost, since before: BandModel.DrawCost, over seconds: Double) -> String {
        let draws = after.draws - before.draws
        let frames = after.linkFrames - before.linkFrames
        guard draws > 0 || frames > 0, seconds > 0 else {
            return ""
        }
        func each(_ value: Double, _ count: Int) -> String {
            String(format: "%.2f", count > 0 ? value * 1_000 / Double(count) : 0)
        }
        let drawMs = after.drawSeconds - before.drawSeconds
        let linkMs = after.linkSeconds - before.linkSeconds
        let held = after.drawableWaitSeconds - before.drawableWaitSeconds
            + after.frameWaitSeconds - before.frameWaitSeconds
        // The link's frames, and the draws the view made on its own.
        let outside = drawMs - (after.linkDrawSeconds - before.linkDrawSeconds)
        let perFrame = (linkMs + outside) * 1_000 / (seconds * 60)
        return "; draws \(draws) (\(String(format: "%.1f", Double(draws) / seconds))/s, link \(after.linkDraws - before.linkDraws), "
            + "skipped \(after.skipped - before.skipped)), link frames \(frames); per draw ms: drawable wait "
            + "\(each(after.drawableWaitSeconds - before.drawableWaitSeconds, draws)), prepare "
            + "\(each(after.prepareSeconds - before.prepareSeconds, draws)), overlays "
            + "\(each(after.overlaysSeconds - before.overlaysSeconds, draws)), frame wait "
            + "\(each(after.frameWaitSeconds - before.frameWaitSeconds, draws)), encode "
            + "\(each(after.encodeSeconds - before.encodeSeconds, draws)), whole \(each(drawMs, draws)); "
            + "row clock per link frame \(each(after.advanceSeconds - before.advanceSeconds, frames)) ms; "
            + "waiting \(String(format: "%.0f", held * 1_000)) ms; band on main per 16.7 ms frame "
            + String(format: "%.2f", perFrame) + " ms"
    }

    private static func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }
}

/// The app's root as `RootView` lays it out, connected and on the band.
private struct LoadRoot: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, rxPanelOpen: false)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

extension TuneUnderLoadTests.Load {
    var drawsBand: Bool {
        switch self {
        case .flat, .stacked: return true
        case .nothing, .mainHeld: return false
        }
    }

    var isStacked: Bool {
        if case .stacked = self { return true }
        return false
    }
}
