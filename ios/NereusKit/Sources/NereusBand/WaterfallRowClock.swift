// NereusSDR for iOS: the clock the band's waterfall lines go in by, steady at the Core's line period
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-26: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code (Claude Opus 5.5):
//   the desktop's remote row clock (src/gui/spectrum/RemoteWaterfallRowClock, NereusSDR's
//   own) written again for the phone, with the same rules and limits.

/// The clock the band's waterfall lines go in by (the desktop's
/// `RemoteWaterfallRowClock`, NereusSDR's own design, written again here
/// with its rules and limits unchanged). It runs at the Core's line period
/// on this phone's steady clock, is read on every frame of the screen, and
/// says when the next waiting line goes in and how far the glide toward it
/// has come.
///
/// JJ's recording of build 7950 (2026-10-25) showed the 2D waterfall and
/// the 3D stack moving in 16.7 and 50 ms pairs instead of a steady 33.3 ms:
/// each line went in the moment it came over Wi-Fi, and a few milliseconds
/// of jitter moved it across a screen refresh. No line was lost. The
/// desktop saw the same on its remote pans before this clock.
///
/// The rules: the first line starts the grid at its due time, and each
/// later slot is the one before plus the line period over the rate. A line
/// goes in one line period before its slot and glides in by its slot. For
/// each line put in, the clock measures how far its due time is from its
/// slot, smooths that, and moves the next slot by a fraction of it (phase)
/// and the rate by a bounded step (2% at most), a critically damped loop. A
/// due time a few lines off the grid starts the grid again at that line. A
/// line not in hand when its slide should start rests the glide; when it
/// comes it goes in at once, its slot one line period on. A line cannot be
/// due sooner than one line period and a margin for the hand-off's jitter
/// after it came: the phone's Core sends no due times, so each line is due
/// as it comes and the grid runs that far behind the lines' arrival,
/// steadily, its rate following the Core's clock. Lines far behind the grid
/// (the screen stalled) play out at most two a frame, each frame's last
/// line gliding in over half a line period.
///
/// Pure logic: the caller passes the time and its queue. Where the
/// desktop's clock tells an observer of its starves and new grids for its
/// trace, this one keeps only the counts (``starvedSlots``,
/// ``reanchors``), which the phone's trace reads between frames.
public struct WaterfallRowClock: Equatable, Sendable {
    /// The phase correction: this fraction of the smoothed error moves the
    /// next slot, at most ``maxPhaseStepRows`` of a line.
    public static let phaseGain = 0.1
    public static let maxPhaseStepRows = 0.25
    /// The rate correction per line, per line period of smoothed error:
    /// phaseGain² / 4 damps the loop critically, so a step of the due times
    /// is followed without overshoot.
    public static let rateGain = 0.0025
    /// The rate never moves more than 2% either way.
    public static let maxRateTrim = 0.02
    /// The smoothing of the measured error, per line put in.
    public static let errorSmoothing = 0.5
    /// A due time more than this many line periods from its slot starts the
    /// grid again at that line.
    public static let reanchorRows = 3.0
    /// Lines one frame may put in: lines far behind (after a stall) play
    /// out a few a frame, gliding, instead of all at once.
    public static let maxRowsPerFrame = 2
    /// A line goes in no sooner than a line period and this margin after it
    /// was handed over, so a line bound by its hand-off is in hand at its
    /// slide start, not pressed against it.
    public static let lateMarginNs: Int64 = 12_000_000

    public private(set) var periodNs: Int64 = 0
    public var running: Bool { periodNs > 0 }

    /// The grid has a slot.
    public private(set) var anchored = false
    /// Resting: the clock reached a slide start with no line in hand.
    public private(set) var starved = false
    private var slideStartNs = 0.0
    private var slotNs = 0.0
    /// The glide shown: the last line's slide to its slot, or, for a line
    /// put in behind the grid, part of a line period from the frame it went in.
    private var glideStartNs = 0.0
    private var glideEndNs = 0.0
    private var progressValue = 1.0
    /// The current rate, 1 ± ``maxRateTrim``.
    public private(set) var rate = 1.0
    /// The smoothed error (due time minus slot), in ns.
    public private(set) var smoothedErrorNs = 0.0
    /// Slots the clock reached with no line in hand since the last reset.
    public private(set) var starvedSlots = 0
    /// Times the grid started again at a line since the last reset.
    public private(set) var reanchors = 0

    public init() {}

    /// The Core's line period in ns; 0 stops the clock. A new period starts
    /// the grid again.
    public mutating func setPeriodNs(_ periodNs: Int64) {
        let period = max(0, periodNs)
        guard period != self.periodNs else {
            return
        }
        self.periodNs = period
        resetState()
    }

    /// Start again: the next line starts the grid at its due time.
    public mutating func reset() {
        resetState()
    }

    /// How far the glide toward the last line put in has come, 0 to 1, as
    /// of the last ``advance(nowNs:queuedRows:frontDueNs:frontQueuedNs:maxRows:)``:
    /// 1 at that line's slot, and while the clock waits.
    public var progress: Double { progressValue }

    /// The slot of the last line put in, in ns; 0 before the first. The
    /// next line's slide starts there.
    public var lastSlotNs: Int64 { Int64(slotNs) }

    /// Advance to `nowNs`. `queuedRows` is the queue's depth; `frontDueNs`
    /// the time its oldest line is due on screen and `frontQueuedNs` the
    /// time that line was handed over, on the same clock (a line with no
    /// due time is due when it was handed over). Returns how many lines to
    /// put in now, oldest first. `maxRows` is what is left of the drawn
    /// frame's ``maxRowsPerFrame``.
    public mutating func advance(nowNs: Int64, queuedRows: Int, frontDueNs: Int64, frontQueuedNs: Int64,
                                 maxRows: Int = WaterfallRowClock.maxRowsPerFrame) -> Int {
        guard running else {
            return 0
        }
        let now = Double(nowNs)
        let period = Double(periodNs)
        let reanchorNs = Self.reanchorRows * period
        let budget = min(max(maxRows, 0), Self.maxRowsPerFrame)
        var rows = 0
        while rows < budget && queuedRows - rows > 0 {
            let step = period / rate
            // The slot this line should have: its due time, but never sooner
            // than a line period and the hand-off margin after it was handed
            // over. Only the oldest line's times are known; a second line
            // this frame (playing out lines far behind) takes the grid's next
            // slot.
            var target = slotNs + step
            if rows == 0 {
                let earliest = Double(frontQueuedNs) + period + Double(Self.lateMarginNs)
                target = max(Double(frontDueNs), earliest)
            }
            if !anchored {
                // The grid starts at this line: it slides in from one line
                // period before its due time.
                if now < target - step {
                    break
                }
                anchor(at: target)
                rows += 1
                continue
            }
            if starved && rows == 0 {
                // It was not in hand when its slide should have started, and
                // the glide rested. It goes in now, its slot a line period
                // on, unless its due time is later than that (the waterfall
                // rests rather than plays a line early) or a few lines
                // earlier: then the grid starts again at it. A line bound by
                // its hand-off takes the later of the two slots and keeps the
                // rate.
                starved = false
                let lateSlot = now + step
                if Double(frontDueNs) > lateSlot {
                    startAgain()
                    continue
                }
                if lateSlot - target > reanchorNs {
                    startAgain()
                    continue
                }
                slideStartNs = now
                slotNs = max(lateSlot, target)
                rows += 1
                continue
            }
            // The next line's slide starts at the last line's slot.
            if now < slotNs {
                break
            }
            let scheduled = slotNs + step
            let error = target - scheduled
            if rows == 0 && abs(error) > reanchorNs {
                startAgain()
                continue
            }
            // The error is taken both ways for the phase and the rate: a line
            // bound by its hand-off has the margin in its target, so the grid
            // settles that margin behind the lines' arrival.
            var correction = 0.0
            if rows == 0 {
                smoothedErrorNs += Self.errorSmoothing * (error - smoothedErrorNs)
                let maxStep = Self.maxPhaseStepRows * step
                correction = min(max(Self.phaseGain * smoothedErrorNs, -maxStep), maxStep)
                rate = min(max(rate - Self.rateGain * smoothedErrorNs / period, 1 - Self.maxRateTrim),
                           1 + Self.maxRateTrim)
            }
            slideStartNs = slotNs
            slotNs = scheduled + correction
            rows += 1
        }
        if rows == 0 && anchored && queuedRows <= 0 && now >= slotNs && !starved {
            // At the slide start with no line in hand: the glide rests at the
            // full line until one comes.
            starved = true
            starvedSlots += 1
        }
        if rows > 0 {
            if slotNs <= now {
                // Lines far behind the grid (a stall): the last one put in
                // this frame glides in over a part of a line period, so the
                // play-out moves on every frame instead of jumping whole
                // lines at rest.
                glideStartNs = now
                glideEndNs = now + period / rate / Double(Self.maxRowsPerFrame)
            } else {
                glideStartNs = slideStartNs
                glideEndNs = slotNs
            }
        }
        if anchored && glideEndNs > glideStartNs {
            progressValue = min(max((now - glideStartNs) / (glideEndNs - glideStartNs), 0), 1)
        } else {
            progressValue = 1
        }
        return rows
    }

    private mutating func anchor(at slot: Double) {
        slideStartNs = slot - Double(periodNs) / rate
        slotNs = slot
        anchored = true
        starved = false
    }

    /// The grid starts again at the next line, at the nominal rate: a rate
    /// learnt against lines that are gone says nothing of the next ones.
    private mutating func startAgain() {
        reanchors += 1
        smoothedErrorNs = 0
        rate = 1
        anchored = false
    }

    private mutating func resetState() {
        anchored = false
        starved = false
        slideStartNs = 0
        slotNs = 0
        glideStartNs = 0
        glideEndNs = 0
        progressValue = 1
        rate = 1
        smoothedErrorNs = 0
        starvedSlots = 0
        reanchors = 0
    }
}

/// Where the phone differs from the desktop's clock: the time the clock
/// reads is held up to half a screen frame off the frame's own, so the
/// slots stay clear of the screen's frame edges (2026-10-26).
///
/// The desktop's 2D waterfall glides by the clock's progress, so a line
/// that goes in one frame early or late shows only as an uneven glide step.
/// The phone's 2D waterfall moves a whole line (one pixel) at a time with
/// no glide, so the frame a line goes in on is what the eye sees. With the
/// grid a row period and a margin behind the lines' arrival, its slots
/// wander a millisecond or two with the arrival jitter; where they sit near
/// a frame edge, lines went in a frame early or late in turn, the same
/// 16.7/50 ms pairs (in a simulation of ±8 ms jitter at 60 Hz, up to one
/// line in thirteen at the worst phase). This lock reads the clock at the
/// frame's time plus an offset under half a frame, chosen so the next slide
/// start falls mid-frame, and moves the offset only when the slide start
/// comes within an eighth of a frame of an edge. The grid itself, its rate
/// and its rules are the clock's own; a slip between the Core's clock and
/// the screen's shows once, as it must, instead of as a run of pairs.
public struct WaterfallFrameLock: Equatable, Sendable {
    /// The offset added to the frame's time, in ns, within half a frame.
    public private(set) var offsetNs: Int64 = 0
    /// Times the offset moved since the last reset.
    public private(set) var moves = 0
    /// The share of a frame a slide start may come to an edge before the
    /// offset moves.
    public static let edgeShare = 0.125

    public init() {}

    public mutating func reset() {
        offsetNs = 0
        moves = 0
    }

    /// The time the clock reads for a frame at `nowNs`.
    public func clockTime(frameNs nowNs: Int64) -> Int64 {
        nowNs + offsetNs
    }

    /// After the clock advanced on the frame at `nowNs` of `frameNs`
    /// length: keeps the clock's next slide start clear of the frames' edges.
    public mutating func follow(_ clock: WaterfallRowClock, frameNs nowNs: Int64, frameLengthNs: Int64) {
        guard frameLengthNs > 0, clock.anchored, !clock.starved else {
            return
        }
        let frame = Double(frameLengthNs)
        // Where the next slide start falls after this frame, on the frames
        // read through the offset: 0 is a frame's own time.
        let ahead = Double(clock.lastSlotNs - offsetNs - nowNs)
        let phase = ahead - frame * (ahead / frame).rounded(.down)
        let edge = frame * Self.edgeShare
        guard phase < edge || phase > frame - edge else {
            return
        }
        // Mid-frame: the frame read through the new offset puts it there.
        var offset = Double(clock.lastSlotNs - nowNs) - frame / 2
        offset -= frame * (offset / frame).rounded(.down)
        if offset >= frame / 2 {
            offset -= frame
        }
        offsetNs = Int64(offset)
        moves += 1
    }
}
