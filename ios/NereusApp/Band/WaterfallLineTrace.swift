// NereusSDR for iOS: a Debug build's timing trace of each waterfall line, from its arrival to the screen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if DEBUG
import Foundation
import NereusBand
import os

/// A Debug build's timing trace of the band's waterfall lines, off unless
/// asked for (the phone's counterpart of the desktop's
/// `NEREUS_TRACE_REMOTE_ROWS`). Each line's three times, on the steady
/// clock the display link keeps, go to the log under the subsystem
/// `NereusSDR`, category `rows`:
///
/// - `ROW`: the line's frame sequence; `recv`, when its frame was committed
///   (it came, with its datagram); `put`, when it went into the waterfall;
///   `present`, when the GPU finished the frame that drew it (it reaches
///   the screen on the refresh after: the iOS 27 SDK has no presented-time
///   handler on a drawable); `recvToPresent`; and `interval`, the time
///   since the line before's frame finished.
/// - `STARVE` and `REANCHOR`: the row clock rested for want of a line, or
///   started its grid again.
/// - `SUMMARY`, every 10 s: the presented-line intervals counted by screen
///   frames of 16.7 ms (`0` is a line drawn with the one before it), the
///   time from arrival to the screen (least, middle, most), and the clock's
///   counts. A steady waterfall at 30 lines a second shows every interval
///   in the 33.3 bin.
///
/// Turned on by the environment variable `NEREUS_TRACE_ROWS=1` or the
/// launch argument `-NereusTraceRows 1`.
@MainActor
final class WaterfallLineTrace {
    static func fromEnvironment() -> WaterfallLineTrace? {
        let asked = ProcessInfo.processInfo.environment["NEREUS_TRACE_ROWS"] == "1"
            || UserDefaults.standard.bool(forKey: "NereusTraceRows")
        return asked ? WaterfallLineTrace() : nil
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "rows")
    /// The summary's window, in seconds.
    static let windowSeconds = 10.0
    /// One screen frame at 60 a second, the histogram's bin.
    static let binMs = 1000.0 / 60

    /// The most lines kept for the next draw, two seconds of them. While
    /// the trace runs and the view does not draw (its tab hidden, the app
    /// in the background), the older ones are counted dropped.
    static let mostForDraw = 64

    /// Lines put in and not yet drawn.
    private var forDraw: [BandState.PutLine] = []
    private var lastPresented: Double?
    private var windowStart: Double?
    private var intervalBins: [Int: Int] = [:]
    private var latenciesMs: [Double] = []
    private var dropped = 0
    private var mostWaiting = 0
    private var starves = 0
    private var reanchors = 0
    private var lockMoves = 0
    private var lastClock = WaterfallRowClock()
    private var lastLock = WaterfallFrameLock()

    /// Lines the band put in, the clock and lock after them, and the lines
    /// still waiting.
    func note(_ puts: [BandState.PutLine], clock: WaterfallRowClock, lock: WaterfallFrameLock, waiting: Int) {
        forDraw.append(contentsOf: puts)
        if forDraw.count > Self.mostForDraw {
            let over = forDraw.count - Self.mostForDraw
            forDraw.removeFirst(over)
            dropped += over
        }
        mostWaiting = max(mostWaiting, waiting)
        if clock.starvedSlots > lastClock.starvedSlots {
            starves += clock.starvedSlots - lastClock.starvedSlots
            Self.logger.notice("STARVE lastSlot=\(Self.ms(Double(clock.lastSlotNs) / 1e9), privacy: .public) waiting=\(waiting, privacy: .public)")
        }
        if clock.reanchors > lastClock.reanchors {
            reanchors += clock.reanchors - lastClock.reanchors
            Self.logger.notice("REANCHOR rate=\(clock.rate, privacy: .public) waiting=\(waiting, privacy: .public)")
        }
        if lock.moves > lastLock.moves {
            lockMoves += lock.moves - lastLock.moves
        }
        lastClock = clock
        lastLock = lock
    }

    /// The lines the next draw shows.
    func takeLinesForDraw() -> [BandState.PutLine] {
        defer { forDraw.removeAll(keepingCapacity: true) }
        return forDraw
    }

    /// The GPU finished the draw that showed `lines` at `time` (0: it never did).
    func presented(_ lines: [BandState.PutLine], at time: Double) {
        guard time > 0 else {
            dropped += lines.count
            Self.logger.notice("DROPPED lines=\(lines.count, privacy: .public)")
            return
        }
        for line in lines {
            let intervalMs = lastPresented.map { (time - $0) * 1000 }
            let latencyMs = (time - line.arrivedAt) * 1000
            Self.logger.notice("""
                ROW seq=\(line.sequence, privacy: .public) recv=\(Self.ms(line.arrivedAt), privacy: .public) \
                put=\(Self.ms(line.putAt), privacy: .public) present=\(Self.ms(time), privacy: .public) \
                recvToPresent=\(Self.fixed(latencyMs), privacy: .public) \
                interval=\(intervalMs.map(Self.fixed) ?? "none", privacy: .public)
                """)
            if let intervalMs {
                intervalBins[Int((intervalMs / Self.binMs).rounded()), default: 0] += 1
            }
            latenciesMs.append(latencyMs)
            lastPresented = time
        }
        summarise(at: time)
    }

    private func summarise(at time: Double) {
        guard let start = windowStart else {
            windowStart = time
            return
        }
        guard time - start >= Self.windowSeconds else {
            return
        }
        let bins = intervalBins.keys.sorted().map { "\(Self.fixed(Double($0) * Self.binMs)):\(intervalBins[$0] ?? 0)" }
            .joined(separator: " ")
        let sorted = latenciesMs.sorted()
        let latency = sorted.isEmpty ? "none"
            : "\(Self.fixed(sorted[0]))/\(Self.fixed(sorted[sorted.count / 2]))/\(Self.fixed(sorted[sorted.count - 1]))"
        Self.logger.notice("""
            SUMMARY window=\(Self.fixed(time - start), privacy: .public)s lines=\(self.latenciesMs.count, privacy: .public) \
            intervalsByFrame=[\(bins, privacy: .public)] recvToPresent(min/med/max)=\(latency, privacy: .public) \
            dropped=\(self.dropped, privacy: .public) starves=\(self.starves, privacy: .public) \
            reanchors=\(self.reanchors, privacy: .public) lockMoves=\(self.lockMoves, privacy: .public) \
            mostWaiting=\(self.mostWaiting, privacy: .public) rate=\(self.lastClock.rate, privacy: .public)
            """)
        windowStart = time
        intervalBins.removeAll()
        latenciesMs.removeAll(keepingCapacity: true)
        dropped = 0
        mostWaiting = 0
        starves = 0
        reanchors = 0
        lockMoves = 0
    }

    private static func ms(_ seconds: Double) -> String {
        fixed(seconds * 1000)
    }

    private static func fixed(_ value: Double) -> String {
        String(format: "%.1f", value)
    }
}
#endif
