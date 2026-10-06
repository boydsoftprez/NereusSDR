// NereusSDR for iOS: the phone's half of the display budget: what quality each pan's display asks the Core for
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The phone's half of the display budget (R-IOS-11; the budget design,
/// `docs/architecture/2026-09-22-session-display-budget-design.md`, "Agreed
/// GUI quality policy", the policy the desktop's own allocator follows).
///
/// Given the Core's budget and what each pan would like, it settles the
/// quality each pan's display endpoint asks for:
///
/// * everything as asked while it fits;
/// * otherwise background frame rates first, then background pixels, then
///   the active band's frame rate and pixels, one unit per pane at a time
///   in stable pan-ID order, never below `min(requestedPixels, 256)` by
///   `min(requestedFps, 10)`;
/// * when even the floors do not fit, background displays are suspended in
///   reverse pan-ID order, and the active band only when its own floor does
///   not fit. A suspended pan keeps its slice and sound; its frozen band is
///   shown paused.
///
/// It never drops a requested wide or waterfall plane and never touches the
/// FFT size, crop, detectors or averaging; it recomputes `framesPerLine`
/// from the waterfall period at the settled frame rate. Its fit test counts
/// the display extras each endpoint asks for, as the Core charges them. It
/// is a calculation only: nothing it does sends anything.
public struct DisplayQualityAllocator: Equatable, Sendable {
    /// The Core's display budget, from its capabilities (budget wire only).
    public struct Budget: Equatable, Sendable {
        /// Why the Core's budget is below its ceiling, passed through as
        /// the Core sent it for the Sharing chip's note.
        public enum Reason: Equatable, Sendable {
            case none
            case coreBusy
            /// Another device is admitted and this device's share is below
            /// its request: the devices share what the Core sends.
            case sharedConnection
            /// The same, while the Core computer is short of processing time.
            case sharedProcessing
            /// A value this app does not know yet.
            case unknown(String)

            public init(wireName: String) {
                switch wireName {
                case "none":
                    self = .none
                case "coreBusy":
                    self = .coreBusy
                case "sharedConnection":
                    self = .sharedConnection
                case "sharedProcessing":
                    self = .sharedProcessing
                default:
                    self = .unknown(wireName)
                }
            }
        }

        public var applicationBytesPerSecond: UInt64
        public var spectrumSampleUnitsPerSecond: UInt64
        public var generation: UInt32
        public var reason: Reason

        public init(applicationBytesPerSecond: UInt64, spectrumSampleUnitsPerSecond: UInt64, generation: UInt32,
                    reason: Reason = .none) {
            self.applicationBytesPerSecond = applicationBytesPerSecond
            self.spectrumSampleUnitsPerSecond = spectrumSampleUnitsPerSecond
            self.generation = generation
            self.reason = reason
        }

        /// The budget in the Core's capabilities, or nil when the budget
        /// wire is off or the descriptor is partial or invalid (never
        /// mistaken for a zero or an unlimited budget). `integer` and `text`
        /// read one capability each, nil when it is absent or of another kind.
        public init?(gates: MediaFeatureGates, integer: (String) -> Int64?, text: (String) -> String?) {
            guard gates.displayBudget,
                  let bytes = integer("displayApplicationBytesPerSecond"),
                  let units = integer("spectrumSampleUnitsPerSecond"),
                  let generation = integer("displayBudgetGeneration"),
                  bytes > 0, bytes <= DisplayQualityAllocator.jsonSafeLimit,
                  units > 0, units <= DisplayQualityAllocator.jsonSafeLimit,
                  generation > 0, generation <= Int64(UInt32.max) else {
                return nil
            }
            self.init(applicationBytesPerSecond: UInt64(bytes), spectrumSampleUnitsPerSecond: UInt64(units),
                      generation: UInt32(generation),
                      reason: text("displayBudgetReason").map(Reason.init(wireName:)) ?? .none)
        }
    }

    /// What one pan would like, from its view's width and its settings.
    public struct Intent: Equatable, Sendable {
        /// The stable pan ID the policy orders pans by.
        public var panId: String
        public var pixels: Int
        public var fps: Int
        /// The subscription asks for a wide plane (`wideSpanFactor` above 1).
        public var includesWidePlane: Bool
        /// The waterfall's line period, from which `framesPerLine` follows.
        public var waterfallPeriodMs: Int
        /// The pan the operator is looking at.
        public var active: Bool
        /// The display extras the subscription asks for, or nil.
        public var extras: DisplayExtrasRequest?

        public init(panId: String, pixels: Int, fps: Int, includesWidePlane: Bool = false, waterfallPeriodMs: Int,
                    active: Bool, extras: DisplayExtrasRequest? = nil) {
            self.panId = panId
            self.pixels = pixels
            self.fps = fps
            self.includesWidePlane = includesWidePlane
            self.waterfallPeriodMs = waterfallPeriodMs
            self.active = active
            self.extras = extras
        }
    }

    /// The Core's charge for a display: bytes, samples and messages a second.
    public struct Charge: Equatable, Sendable {
        public var applicationBytesPerSecond: UInt64 = 0
        public var spectrumSampleUnitsPerSecond: UInt64 = 0
        public var messagesPerSecond: UInt64 = 0

        public init(applicationBytesPerSecond: UInt64 = 0, spectrumSampleUnitsPerSecond: UInt64 = 0,
                    messagesPerSecond: UInt64 = 0) {
            self.applicationBytesPerSecond = applicationBytesPerSecond
            self.spectrumSampleUnitsPerSecond = spectrumSampleUnitsPerSecond
            self.messagesPerSecond = messagesPerSecond
        }

        static func + (left: Charge, right: Charge) -> Charge {
            Charge(applicationBytesPerSecond: left.applicationBytesPerSecond + right.applicationBytesPerSecond,
                   spectrumSampleUnitsPerSecond: left.spectrumSampleUnitsPerSecond + right.spectrumSampleUnitsPerSecond,
                   messagesPerSecond: left.messagesPerSecond + right.messagesPerSecond)
        }
    }

    /// What one pan's endpoint asks for.
    public struct Quality: Equatable, Sendable {
        public var panId: String
        public var pixels: Int
        public var fps: Int
        public var framesPerLine: Int
        /// The display is suspended: no endpoint; the band is shown paused.
        public var suspended: Bool
        /// Below what the pan would like.
        public var reduced: Bool
        public var charge: Charge
    }

    /// The settled qualities, in pan-ID order.
    public struct Allocation: Equatable, Sendable {
        public var pans: [Quality]
        public var total: Charge
        /// The budget's reason, as the Core sent it; nil without a budget.
        public var reason: Budget.Reason?
        /// The budget generation this allocation was made for; nil without one.
        public var budgetGeneration: UInt32?

        public func quality(forPan panId: String) -> Quality? {
            pans.first(where: { $0.panId == panId })
        }
    }

    /// Why no allocation could be made. For the log and the app's own code.
    public enum Invalid: Error, Equatable, Sendable {
        case tooManyPans
        case duplicatePan
        case outOfRange
        case moreThanOneActive
    }

    /// The fewest pixels and frames a second a reduced display keeps: the
    /// operator-approved floor, `min(requested, 256)` by `min(requested, 10)`.
    public static let usefulPixels = 256
    public static let usefulFps = 10
    /// At most eight endpoints.
    public static let maximumPans = DisplayEndpointRequest.maximumEndpoints
    public static let maximumWaterfallPeriodMs = 65_535
    /// The display codec's frame header (NSDC v1) and the wide row's samples.
    public static let frameHeaderBytes: UInt64 = 42
    public static let wideSamples: UInt64 = 768
    /// The Core's display sender runs every 5 ms: at most 200 messages a second.
    public static let maximumMessagesPerSecond: UInt64 = 200
    /// The largest integer JSON carries exactly.
    static let jsonSafeLimit: Int64 = 9_007_199_254_740_991

    /// The Core's budget; nil when it advertises none (every pan as asked).
    public private(set) var budget: Budget?
    /// Each pan's wish.
    public private(set) var intents: [Intent] = []
    /// The settled qualities for the current budget and wishes.
    public private(set) var allocation: Allocation
    /// Set when the last recompute found the wishes malformed; the last
    /// good allocation stays.
    public private(set) var lastProblem: Invalid?

    public init(budget: Budget? = nil) {
        self.budget = budget
        allocation = Allocation(pans: [], total: Charge(), reason: budget?.reason, budgetGeneration: budget?.generation)
    }

    /// A new budget from the Core's capabilities: quality is recomputed, so
    /// a larger budget restores what was asked.
    public mutating func update(budget: Budget?) {
        self.budget = budget
        awaitingBudget = false
        recompute()
    }

    /// New wishes (focus, layout, geometry or settings changed).
    public mutating func update(intents: [Intent]) {
        self.intents = intents
        recompute()
    }

    /// An `allocation-result` from the Core. A refusal says the requested
    /// display does not fit; it is kept, and the same request is not sent
    /// again until the budget or the wishes change. A result made under a
    /// budget generation other than the one held says the Core's budget
    /// moved: its capabilities with the new figures follow, and the
    /// allocation is made again when they arrive. Returns true for a refusal.
    @discardableResult
    public mutating func note(_ result: MediaControlEvent.AllocationResult) -> Bool {
        if let budget, result.budgetGeneration != budget.generation {
            awaitingBudget = true
        }
        if result.accepted {
            lastRefusal = nil
            return false
        }
        lastRefusal = result
        return true
    }

    /// The Core's last refusal since the budget or the wishes changed.
    public private(set) var lastRefusal: MediaControlEvent.AllocationResult?
    /// A result came under a budget generation the app has not seen yet.
    public private(set) var awaitingBudget = false

    // MARK: The policy

    /// The allocation for `intents` under `budget`, or why there is none.
    public static func allocate(budget: Budget?, intents: [Intent]) -> Result<Allocation, Invalid> {
        guard intents.count <= maximumPans else {
            return .failure(.tooManyPans)
        }
        let sorted = intents.sorted { $0.panId < $1.panId }
        for (index, intent) in sorted.enumerated() {
            guard DisplayEndpointRequest.pixelRange.contains(intent.pixels),
                  DisplayEndpointRequest.fpsRange.contains(intent.fps),
                  (1...maximumWaterfallPeriodMs).contains(intent.waterfallPeriodMs) else {
                return .failure(.outOfRange)
            }
            if index > 0, sorted[index - 1].panId == intent.panId {
                return .failure(.duplicatePan)
            }
        }
        guard sorted.filter(\.active).count <= 1 else {
            return .failure(.moreThanOneActive)
        }
        guard let budget else {
            // No budget: every pan as asked.
            let pans = sorted.map { make($0, pixels: $0.pixels, fps: $0.fps, suspended: false) }
            return .success(Allocation(pans: pans, total: pans.reduce(Charge()) { $0 + $1.charge }, reason: nil,
                                       budgetGeneration: nil))
        }

        var suspended = [Bool](repeating: false, count: sorted.count)
        while true {
            var pixels = sorted.map(\.pixels)
            var fps = sorted.map(\.fps)
            let live = sorted.indices.filter { !suspended[$0] }
            let backgrounds = live.filter { !sorted[$0].active }
            let actives = live.filter { sorted[$0].active }
            func fits() -> Bool {
                Self.fits(budget, total(sorted, pixels: pixels, fps: fps, suspended: suspended))
            }
            // One frame a second, or one pixel, off each pane in turn.
            func lower(_ indexes: [Int], frameRate: Bool) -> Bool {
                var changed = true
                while changed {
                    changed = false
                    for index in indexes {
                        let intent = sorted[index]
                        if frameRate {
                            guard fps[index] > min(intent.fps, usefulFps) else {
                                continue
                            }
                            fps[index] -= 1
                        } else {
                            guard pixels[index] > min(intent.pixels, usefulPixels) else {
                                continue
                            }
                            pixels[index] -= 1
                        }
                        changed = true
                        if fits() {
                            return true
                        }
                    }
                }
                return fits()
            }
            let settled = fits()
                || lower(backgrounds, frameRate: true)
                || lower(backgrounds, frameRate: false)
                || lower(actives, frameRate: true)
                || lower(actives, frameRate: false)
            if settled {
                let pans = sorted.indices.map { make(sorted[$0], pixels: pixels[$0], fps: fps[$0], suspended: suspended[$0]) }
                return .success(Allocation(pans: pans, total: pans.reduce(Charge()) { $0 + $1.charge },
                                           reason: budget.reason, budgetGeneration: budget.generation))
            }
            // Suspend the last background display still live, then the active band.
            if let last = backgrounds.last {
                suspended[last] = true
                continue
            }
            if let active = actives.last {
                suspended[active] = true
                continue
            }
            // Every display suspended: nothing is asked of the budget.
            let pans = sorted.map { make($0, pixels: 0, fps: 0, suspended: true) }
            return .success(Allocation(pans: pans, total: Charge(), reason: budget.reason,
                                       budgetGeneration: budget.generation))
        }
    }

    /// What the Core charges an endpoint of `pixels` at `fps`: the display
    /// codec's worst frame (a 42-byte header, the trace and waterfall rows
    /// and the wide row when asked, each plane `3 + 5·ceil(n/128) + n`
    /// bytes at most) and, when extras are asked, their worst datagram, one
    /// message a frame more and the peak hold row's samples.
    public static func charge(pixels: Int, fps: Int, includesWidePlane: Bool,
                              extras: DisplayExtrasRequest?) -> Charge {
        guard pixels > 0, fps > 0 else {
            return Charge()
        }
        let p = UInt64(pixels)
        let f = UInt64(fps)
        let wide = includesWidePlane ? wideSamples : 0
        let frameBytes = frameHeaderBytes + 2 * planeBytes(p) + planeBytes(wide)
        var charge = Charge(applicationBytesPerSecond: frameBytes * f, spectrumSampleUnitsPerSecond: (2 * p + wide) * f,
                            messagesPerSecond: f)
        if let extras, extras.sections != 0 {
            charge.applicationBytesPerSecond += UInt64(extras.worstCaseBytesPerFrame(traceSamples: pixels)) * f
            charge.messagesPerSecond += f
            if extras.sections & DisplayExtrasRequest.peakHoldSection != 0 {
                charge.spectrumSampleUnitsPerSecond += p * f
            }
        }
        return charge
    }

    /// `ceil(periodMs × fps / 1000)`, at least 1.
    public static func framesPerLine(periodMs: Int, fps: Int) -> Int {
        guard fps > 0 else {
            return 0
        }
        let rounded = (periodMs * fps + 999) / 1000
        return min(max(rounded, 1), DisplayEndpointRequest.framesPerLineRange.upperBound)
    }

    // MARK: Inside

    private mutating func recompute() {
        lastRefusal = nil
        switch Self.allocate(budget: budget, intents: intents) {
        case .success(let next):
            allocation = next
            lastProblem = nil
        case .failure(let problem):
            lastProblem = problem
        }
    }

    private static func planeBytes(_ samples: UInt64) -> UInt64 {
        samples == 0 ? 0 : 3 + 5 * ((samples + 127) / 128) + samples
    }

    private static func make(_ intent: Intent, pixels: Int, fps: Int, suspended: Bool) -> Quality {
        guard !suspended else {
            return Quality(panId: intent.panId, pixels: 0, fps: 0, framesPerLine: 0, suspended: true, reduced: true,
                           charge: Charge())
        }
        return Quality(panId: intent.panId, pixels: pixels, fps: fps,
                       framesPerLine: framesPerLine(periodMs: intent.waterfallPeriodMs, fps: fps), suspended: false,
                       reduced: pixels < intent.pixels || fps < intent.fps,
                       charge: charge(pixels: pixels, fps: fps, includesWidePlane: intent.includesWidePlane,
                                      extras: intent.extras))
    }

    private static func total(_ intents: [Intent], pixels: [Int], fps: [Int], suspended: [Bool]) -> Charge {
        intents.indices.reduce(Charge()) { sum, index in
            suspended[index] ? sum : sum + charge(pixels: pixels[index], fps: fps[index],
                                                  includesWidePlane: intents[index].includesWidePlane,
                                                  extras: intents[index].extras)
        }
    }

    private static func fits(_ budget: Budget, _ charge: Charge) -> Bool {
        charge.applicationBytesPerSecond <= budget.applicationBytesPerSecond
            && charge.spectrumSampleUnitsPerSecond <= budget.spectrumSampleUnitsPerSecond
            && charge.messagesPerSecond <= maximumMessagesPerSecond
    }
}
