// NereusSDR for iOS: one band's latest frame, its extras and its waterfall, as the Core's display events arrive
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMedia

/// One band's state between the Core's display events and the renderer.
///
/// The Core sends a frame's display extras right after the frame, so a
/// frame waits for its datagram before its waterfall line is coloured: it
/// is committed when its datagram arrives, when the next frame arrives, or
/// when the band is drawn, whichever comes first. With no extras expected
/// (an older Core, or nothing asked) a frame is committed at once.
public struct BandState: Equatable, Sendable {
    public private(set) var history: WaterfallHistory
    /// The latest committed frame.
    public private(set) var frame: DisplayFrame?
    /// Advances only when a frame is committed, including after its
    /// matching extras arrive. A viewer can tell a fresh frame from a
    /// picture retained across a media connection.
    public private(set) var committedSerial: UInt64 = 0
    /// The committed frame's own datagram, or nil when it came without one.
    public private(set) var frameExtras: DisplayExtras?
    /// The frequencies the committed frame covers, when its context is known.
    public private(set) var frameCoverage: BandCoverage?
    /// Whether the Core sends extras beside this band's frames.
    public var expectsExtras: Bool
    /// The waterfall's Color Gain and Black Level, applied to each line's
    /// levels as it is coloured; the pan's settings set it.
    public var levelAdjustment: WaterfallHistory.Adjustment = .none
    /// The phone sets the waterfall's levels itself (Use spectrum min/max):
    /// the manual levels given colour each line, whatever the Core sent.
    public var phoneSetsLevels = false
    /// The waterfall stands still: no line is added (Stop on TX while keyed).
    public var waterfallHeld = false
    /// The classic peak hold's reset period in milliseconds, or nil while
    /// it is off (``BandDisplaySettings/peakHold``).
    public var peakHoldDelayMs: Int? {
        didSet {
            if peakHoldDelayMs == nil {
                peakHold = nil
            }
        }
    }
    /// The classic peak hold: the highest level at each trace sample since
    /// it last started afresh; nil while off or before a frame.
    public private(set) var peakHold: [Float]?
    /// The producer time, in nanoseconds, the peak hold last started afresh.
    private var peakHoldStart: UInt64 = 0
    /// The 3D view's rows, kept only while ``stacksRows`` is on.
    public private(set) var stack = StackedTraceHistory()
    /// The pan has 3D chosen: each waterfall line also becomes a row of the
    /// stack. Off clears the rows.
    public var stacksRows = false {
        didSet {
            if !stacksRows {
                stack.clear()
            }
        }
    }
    /// Now, in seconds since 1970, for each line's time.
    public var clock: @Sendable () -> Double = { Date().timeIntervalSince1970 }

    public static func == (lhs: BandState, rhs: BandState) -> Bool {
        lhs.history == rhs.history && lhs.frame == rhs.frame && lhs.committedSerial == rhs.committedSerial
            && lhs.frameExtras == rhs.frameExtras
            && lhs.frameCoverage == rhs.frameCoverage && lhs.expectsExtras == rhs.expectsExtras
            && lhs.levelAdjustment == rhs.levelAdjustment && lhs.phoneSetsLevels == rhs.phoneSetsLevels
            && lhs.waterfallHeld == rhs.waterfallHeld && lhs.peakHold == rhs.peakHold
            && lhs.contexts == rhs.contexts && lhs.pending == rhs.pending && lhs.pendingExtras == rhs.pendingExtras
            && lhs.stack == rhs.stack && lhs.stacksRows == rhs.stacksRows && lhs.wideContexts == rhs.wideContexts
    }

    /// The latest contexts' coverage, by context generation.
    private var contexts: [UInt32: BandCoverage] = [:]
    /// What each context's wide row covers, where it has one.
    private var wideContexts: [UInt32: BandCoverage] = [:]
    private var contextOrder: [UInt32] = []
    /// How many recent contexts are remembered for frames still arriving.
    static let rememberedContexts = 8

    private var pending: DisplayFrame?
    private var pendingExtras: DisplayExtras?

    public init(waterfallLines: Int, expectsExtras: Bool = false) {
        history = WaterfallHistory(capacity: waterfallLines)
        self.expectsExtras = expectsExtras
    }

    /// The Core's context `generation` covers `coverage`: its frames are
    /// drawn there.
    /// `wide` is what its frames' wide row covers, nil for none.
    public mutating func note(context generation: UInt32, coverage: BandCoverage, wide: BandCoverage? = nil) {
        if contexts[generation] == nil {
            contextOrder.append(generation)
        }
        contexts[generation] = coverage
        wideContexts[generation] = wide.flatMap { $0.spanHz > 0 ? $0 : nil }
        while contextOrder.count > Self.rememberedContexts {
            let gone = contextOrder.removeFirst()
            contexts[gone] = nil
            wideContexts[gone] = nil
        }
    }

    /// A frame for this band.
    public mutating func receive(frame: DisplayFrame, manualLevels: WaterfallHistory.Levels) {
        commit(manualLevels: manualLevels)
        pending = frame
        pendingExtras = nil
        if !expectsExtras {
            commit(manualLevels: manualLevels)
        }
    }

    /// A datagram for this band: it goes with the waiting frame when it is
    /// that frame's.
    public mutating func receive(extras: DisplayExtras, manualLevels: WaterfallHistory.Levels) {
        guard let waiting = pending, WaterfallHistory.belongs(extras, to: waiting) else {
            return
        }
        pendingExtras = extras
        commit(manualLevels: manualLevels)
    }

    /// Commits the waiting frame, with its datagram if it came.
    public mutating func commit(manualLevels: WaterfallHistory.Levels) {
        guard let waiting = pending else {
            return
        }
        let coverage = contexts[waiting.contextGeneration]
        if waterfallHeld {
            // The line is dropped, but the Core's levels are still noted.
            history.append(frame: waiting, extras: pendingExtras, manualLevels: manualLevels, coverage: coverage,
                           addsLine: false)
        } else {
            let now = clock()
            history.append(frame: waiting, extras: pendingExtras, manualLevels: manualLevels, coverage: coverage,
                           adjustment: levelAdjustment, phoneLevels: phoneSetsLevels, time: now)
            if stacksRows {
                // The stack moves with the waterfall, and stops with it.
                stack.append(frame: waiting, coverage: coverage, wide: wideContexts[waiting.contextGeneration],
                             time: now)
            }
        }
        holdPeaks(waiting)
        frame = waiting
        committedSerial &+= 1
        frameCoverage = coverage
        frameExtras = pendingExtras
        pending = nil
        pendingExtras = nil
    }

    /// Retires a frame waiting for extras when its media or endpoint ends.
    /// The already committed picture and waterfall remain visible.
    public mutating func discardPending() {
        pending = nil
        pendingExtras = nil
    }

    /// The classic peak hold after `frame`: each sample's highest level,
    /// started afresh from this frame's trace every ``peakHoldDelayMs`` and
    /// whenever the trace's width or context changes.
    private mutating func holdPeaks(_ frame: DisplayFrame) {
        guard let delay = peakHoldDelayMs, !frame.traceDbm.isEmpty else {
            return
        }
        let elapsed = frame.producerTimestamp &- peakHoldStart
        let fresh = peakHold?.count != frame.traceDbm.count || self.frame?.contextGeneration != frame.contextGeneration
            || frame.producerTimestamp < peakHoldStart || elapsed >= UInt64(max(delay, 1)) * 1_000_000
        if fresh || peakHold == nil {
            peakHold = frame.traceDbm
            peakHoldStart = frame.producerTimestamp
            return
        }
        for index in frame.traceDbm.indices where frame.traceDbm[index] > peakHold![index] {
            peakHold![index] = frame.traceDbm[index]
        }
    }

    /// Keeps as many waterfall lines as the band has pixel rows.
    public mutating func resizeWaterfall(lines: Int) {
        history.resize(capacity: lines)
    }

    /// At a key and an unkey on this band: the peak hold starts afresh and
    /// the waterfall's held levels are forgotten, so nothing blends from
    /// the receiver into the transmit display or back (the desktop clears
    /// its averaging and resets its waterfall AGC at both edges). The
    /// waterfall's lines stay.
    public mutating func resetSmoothing() {
        peakHold = nil
        history.forgetLevels()
    }

    /// Starts over: a new session or a new endpoint.
    public mutating func clear() {
        history.clear()
        stack.clear()
        frame = nil
        peakHold = nil
        frameCoverage = nil
        frameExtras = nil
        pending = nil
        pendingExtras = nil
    }
}
