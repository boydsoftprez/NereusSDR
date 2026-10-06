// NereusSDR for iOS: the microphone's 20 ms frames, encoded as they fill and sent one per 20 ms tick of a steady clock, never in a burst
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// How one key's microphone sends were spaced: what the next test on the
/// air reads to prove the cadence.
public struct MicrophoneSendStats: Equatable, Sendable {
    /// Frames handed to the line.
    public var frames = 0
    /// Frames the line would not take (no line open, or the send failed).
    public var unsent = 0
    /// Frames still waiting when the key ended, dropped.
    public var dropped = 0
    /// The most frames waiting at once.
    public var maxBacklog = 0
    /// The gaps between one send and the next, in milliseconds; nil with
    /// fewer than two sends.
    public var minGapMs: Double?
    public var maxGapMs: Double?
    public var meanGapMs: Double?

    public init() {}
}

/// Cuts the microphone's 48 kHz mono samples into 20 ms frames, encodes
/// each as soon as it is full, and sends the frames at a steady 20 ms
/// cadence (the media control document, "Microphone line": one Opus frame
/// of 960 samples per RTP packet).
///
/// Sends keep time by the clock, not by how the samples arrive, so sound
/// that comes in lumps (iOS may hand the microphone over 100 ms at a time)
/// still leaves one frame every 20 ms and never five back to back. A frame
/// that arrives while nothing waits and the last send is at least 20 ms
/// old goes at once, so when the samples come in small pieces a frame
/// leaves as soon as it fills. Otherwise it waits its turn: each tick sends
/// exactly one, and a backlog carries over to the next ticks. After a
/// stall of the clock's own thread the cadence starts again from the late
/// tick instead of catching up in a burst, and two sends are never closer
/// than a quarter of a frame; no frame is dropped or sent twice while the
/// key lasts.
///
/// ``begin(encode:)`` starts a key with a fresh encoder, ``take(_:)`` hands
/// it samples (from one thread at a time), and ``end()`` stops it at once,
/// dropping whatever waits, and returns the key's ``MicrophoneSendStats``.
/// Everything is guarded by one lock, and sends go out under it, so frames
/// leave in the order they were encoded.
public final class MicrophoneSender: @unchecked Sendable {
    /// One frame: 20 ms at 48 kHz.
    public static let frameSamples = Int(MediaUplink.frameSamples)
    /// One frame's time, in nanoseconds.
    public static let frameNanoseconds: UInt64 = 20_000_000
    /// Ticks stay on their 20 ms grid while they run late by less than
    /// three quarters of a frame, so a clock that always wakes a little
    /// late keeps the full rate. A tick later than that (the clock's thread
    /// stalled) starts the grid again from itself, so the next send is a
    /// whole frame away and a stall never turns into a burst.
    static let minimumSpacingNanoseconds: UInt64 = frameNanoseconds / 4

    private let send: @Sendable (Data) -> Bool
    private let makeClock: @Sendable () -> any MicrophoneSendClock
    private let lock = NSLock()

    // Everything below is read and written under `lock`.
    private var clock: (any MicrophoneSendClock)?
    private var encode: (([Float]) throws -> Data)?
    /// Moves with each begin and end, so a tick from an earlier key does nothing.
    private var generation = 0
    private var pending: [Float] = []
    private var backlog: [Data] = []
    /// When the scheduled tick is due; nil while no tick is scheduled.
    private var nextDeadline: UInt64?
    private var lastSend: UInt64?
    private var stats = MicrophoneSendStats()
    private var gapTotalMs = 0.0
    private var gapCount = 0

    /// `send` puts one encoded frame on the line (``MediaUplink/sendMicrophone(_:)``);
    /// `makeClock` gives each key its own clock.
    public init(send: @escaping @Sendable (Data) -> Bool,
                makeClock: @escaping @Sendable () -> any MicrophoneSendClock = { HostSendClock() }) {
        self.send = send
        self.makeClock = makeClock
    }

    deinit {
        clock?.close()
    }

    /// The key is on: frames from ``take(_:)`` are encoded with `encode`
    /// and sent. A key already on ends first, its backlog dropped.
    public func begin(encode: @escaping ([Float]) throws -> Data) {
        let clock = makeClock()
        let previous: (any MicrophoneSendClock)? = lock.withLock {
            let previous = self.clock
            resetLocked()
            self.clock = clock
            self.encode = encode
            return previous
        }
        previous?.close()
    }

    /// The key is off, at once: nothing more is sent, and waiting frames
    /// and samples are dropped. Returns how this key's sends were spaced;
    /// with no key on, an empty ``MicrophoneSendStats``.
    @discardableResult
    public func end() -> MicrophoneSendStats {
        let (clock, result): ((any MicrophoneSendClock)?, MicrophoneSendStats) = lock.withLock {
            let clock = self.clock
            var result = stats
            result.dropped = clock == nil ? 0 : backlog.count
            resetLocked()
            return (clock, result)
        }
        clock?.close()
        return result
    }

    /// The key is on.
    public var isOn: Bool {
        lock.withLock { clock != nil }
    }

    /// Samples of the microphone, 48 kHz mono, any number; with no key on
    /// they are ignored. Full frames are encoded now and sent in turn.
    public func take(_ samples: [Float]) {
        lock.withLock {
            guard let clock, let encode else {
                return
            }
            pending.append(contentsOf: samples)
            var framed = 0
            while pending.count - framed >= Self.frameSamples {
                let frame = Array(pending[framed ..< framed + Self.frameSamples])
                framed += Self.frameSamples
                if let data = try? encode(frame) {
                    backlog.append(data)
                }
            }
            if framed > 0 {
                pending.removeFirst(framed)
            }
            stats.maxBacklog = max(stats.maxBacklog, backlog.count)
            // Nothing scheduled: the cadence is idle, and a waiting frame goes now.
            if nextDeadline == nil, !backlog.isEmpty {
                let now = clock.now
                sendOneLocked(at: now)
                scheduleLocked(clock, at: now + Self.frameNanoseconds)
            }
        }
    }

    /// Frames encoded and waiting to be sent (tests, diagnostics).
    public var waitingFrames: Int {
        lock.withLock { backlog.count }
    }

    // MARK: Under the lock

    private func scheduleLocked(_ clock: any MicrophoneSendClock, at deadline: UInt64) {
        nextDeadline = deadline
        let current = generation
        clock.schedule(at: deadline) { [weak self] in
            self?.tick(generation: current)
        }
    }

    /// One tick of the cadence: one frame if any waits; with none, the
    /// cadence goes idle and the next frame goes as it arrives.
    private func tick(generation ticked: Int) {
        lock.withLock {
            guard ticked == generation, let clock, let deadline = nextDeadline else {
                return
            }
            guard !backlog.isEmpty else {
                nextDeadline = nil
                return
            }
            let now = clock.now
            sendOneLocked(at: now)
            var next = deadline + Self.frameNanoseconds
            // A tick that ran most of a frame late (the clock's thread
            // stalled) restarts the cadence from now instead of catching up
            // in a burst.
            if next < now + Self.minimumSpacingNanoseconds {
                next = now + Self.frameNanoseconds
            }
            scheduleLocked(clock, at: next)
        }
    }

    private func sendOneLocked(at now: UInt64) {
        let frame = backlog.removeFirst()
        if send(frame) {
            stats.frames += 1
        } else {
            stats.unsent += 1
        }
        if let lastSend {
            let gap = Double(now &- lastSend) / 1_000_000
            stats.minGapMs = min(stats.minGapMs ?? gap, gap)
            stats.maxGapMs = max(stats.maxGapMs ?? gap, gap)
            gapTotalMs += gap
            gapCount += 1
            stats.meanGapMs = gapTotalMs / Double(gapCount)
        }
        lastSend = now
    }

    private func resetLocked() {
        generation += 1
        clock = nil
        encode = nil
        pending.removeAll(keepingCapacity: true)
        backlog.removeAll()
        nextDeadline = nil
        lastSend = nil
        stats = MicrophoneSendStats()
        gapTotalMs = 0
        gapCount = 0
    }
}
