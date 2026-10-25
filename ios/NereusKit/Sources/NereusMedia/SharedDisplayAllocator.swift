// NereusSDR for iOS: one display budget for every pan the device shows: each pan's wish, its share and its endpoint IDs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One ``DisplayQualityAllocator`` for every pan this device shows (R-IOS-59,
/// spec section 6.3, "Each pan is a band"): the Core's display budget is
/// shared between the pans as the desktop's remote window shares it
/// between its pans (`src/gui/RemoteMediaController.cpp:6253-6255` and
/// `:6353`: one `allocateRemoteDisplay` over every pan's intent).
///
/// Each pan's subscriber offers its wish (``update(intent:forPan:)``), or
/// every wish goes at once (``update(intents:)``); the allocation settles
/// each pan's share, and every pan is told when it moves
/// (``observe(pan:_:)``) so it re-plans inside its new share. The pan the
/// operator is looking at (``activePanId``) is the one the policy keeps
/// whole longest; with none set, the first pan in pan-ID order.
///
/// Each pan's subscriber also says what it holds or has asked the Core for
/// (``claim(_:by:)``): an increase on one goes only into the headroom the
/// others leave (``headroom(for:)``). A claim is the subscriber's, not its
/// pan's: a closed pan's subscriber still letting its endpoint go and the
/// same pan reopened each hold their own.
///
/// Every pan's display endpoint IDs come from one counter for the media
/// connection (``takeEndpointId()``), as the desktop's remote window takes
/// each new binding's ID from one `nextEndpoint` for every pan
/// (`src/gui/RemoteMediaController.cpp:5784-5785`). The Core refuses an
/// endpoint ID new to it at or below the highest it has seen on the
/// connection, "This display had already been closed."
/// (`src/core/session/media/DaemonMediaController.cpp:3766-3802`), so IDs
/// only ever rise, whichever pan asks.
///
/// It is a calculation only: nothing it does sends anything.
@MainActor
public final class SharedDisplayAllocator {
    /// The settled qualities for the current budget and every pan's wish.
    public private(set) var allocator: DisplayQualityAllocator
    /// The pan the operator is looking at, by its pan ID; nil for the first.
    public var activePanId: String? {
        didSet {
            if activePanId != oldValue {
                recompute()
            }
        }
    }

    private var wishes: [String: DisplayQualityAllocator.Intent] = [:]
    private var claims: [ObjectIdentifier: DisplayQualityAllocator.Charge] = [:]
    /// The next endpoint ID on this media connection; 0 once every ID is used.
    private var nextEndpoint: UInt32 = 1
    private var observers: [String: @MainActor () -> Void] = [:]

    public init(budget: DisplayQualityAllocator.Budget? = nil) {
        allocator = DisplayQualityAllocator(budget: budget)
    }

    /// The Core's budget, nil without one.
    public var budget: DisplayQualityAllocator.Budget? { allocator.budget }
    /// Every pan's wish, as the policy reads it (in pan-ID order, with the
    /// active flag on the active pan alone).
    public var intents: [DisplayQualityAllocator.Intent] { allocator.intents }
    /// The settled qualities.
    public var allocation: DisplayQualityAllocator.Allocation { allocator.allocation }
    /// Set when the last recompute found the wishes malformed.
    public var lastProblem: DisplayQualityAllocator.Invalid? { allocator.lastProblem }

    /// The quality settled for `panId`, or nil for a pan with no wish.
    public func quality(forPan panId: String) -> DisplayQualityAllocator.Quality? {
        allocation.quality(forPan: panId)
    }

    /// A new budget from the Core's capabilities.
    public func update(budget: DisplayQualityAllocator.Budget?) {
        guard budget != allocator.budget else {
            return
        }
        let before = allocator.allocation
        allocator.update(budget: budget)
        notify(ifChangedFrom: before)
    }

    /// Every pan's wish at once: the pans not named have none.
    public func update(intents: [DisplayQualityAllocator.Intent]) {
        var next: [String: DisplayQualityAllocator.Intent] = [:]
        for intent in intents {
            next[intent.panId] = intent
        }
        wishes = next
        recompute()
    }

    /// One pan's wish, or nil when it has none; the other pans' stand.
    public func update(intent: DisplayQualityAllocator.Intent?, forPan panId: String) {
        guard wishes[panId] != intent else {
            return
        }
        wishes[panId] = intent
        recompute()
    }

    /// The pan closed: its wish and its observer go. Its subscriber's claim
    /// stands until that subscriber holds nothing (``claim(_:by:)``).
    public func remove(pan panId: String) {
        observers[panId] = nil
        guard wishes.removeValue(forKey: panId) != nil else {
            return
        }
        recompute()
    }

    /// What `claimant` (a pan's subscriber) holds at the Core or has asked
    /// it for; an empty charge drops its claim.
    public func claim(_ charge: DisplayQualityAllocator.Charge, by claimant: ObjectIdentifier) {
        let before = claims[claimant] ?? DisplayQualityAllocator.Charge()
        claims[claimant] = charge == DisplayQualityAllocator.Charge() ? nil : charge
        if charge.applicationBytesPerSecond < before.applicationBytesPerSecond
            || charge.spectrumSampleUnitsPerSecond < before.spectrumSampleUnitsPerSecond {
            // Headroom came back: a pan whose increase waited for it plans again.
            for id in observers.keys.sorted() {
                observers[id]?()
            }
        }
    }

    /// The budget less what every other claimant holds or has asked for;
    /// nil without a budget (every pan as asked).
    public func headroom(for claimant: ObjectIdentifier) -> (applicationBytesPerSecond: UInt64,
                                                             spectrumSampleUnitsPerSecond: UInt64)? {
        guard let budget = allocator.budget else {
            return nil
        }
        var bytes = budget.applicationBytesPerSecond
        var units = budget.spectrumSampleUnitsPerSecond
        for (other, charge) in claims where other != claimant {
            bytes -= min(bytes, charge.applicationBytesPerSecond)
            units -= min(units, charge.spectrumSampleUnitsPerSecond)
        }
        return (bytes, units)
    }

    /// An `allocation-result` from the Core, for any pan.
    @discardableResult
    public func note(_ result: MediaControlEvent.AllocationResult) -> Bool {
        allocator.note(result)
    }

    /// The next display endpoint ID on this media connection, for whichever
    /// pan subscribes a new endpoint now; nil once every ID is used, as the
    /// desktop stops at `nextEndpoint == 0`
    /// (`src/gui/RemoteMediaController.cpp:5784`). Taken only when the
    /// subscribe goes, so IDs reach the Core in rising order.
    public func takeEndpointId() -> UInt32? {
        guard nextEndpoint != 0 else {
            return nil
        }
        let id = nextEndpoint
        nextEndpoint &+= 1
        return id
    }

    /// The media connection ended: the next one's IDs start again from 1,
    /// as the Core forgets the old connection's IDs with its peer
    /// (`src/core/session/media/DaemonMediaController.cpp:1876-1881`, `:8426`).
    public func restartEndpointIds() {
        nextEndpoint = 1
    }

    /// `handler` runs whenever the allocation moves, so `panId` re-plans.
    public func observe(pan panId: String, _ handler: @escaping @MainActor () -> Void) {
        observers[panId] = handler
    }

    // MARK: Inside

    private func recompute() {
        let before = allocator.allocation
        let ids = wishes.keys.sorted()
        let active = activePanId.flatMap { ids.contains($0) ? $0 : nil } ?? ids.first
        let intents = ids.compactMap { id -> DisplayQualityAllocator.Intent? in
            guard var intent = wishes[id] else {
                return nil
            }
            intent.active = id == active
            return intent
        }
        allocator.update(intents: intents)
        notify(ifChangedFrom: before)
    }

    private func notify(ifChangedFrom before: DisplayQualityAllocator.Allocation) {
        guard allocator.allocation != before else {
            return
        }
        for id in observers.keys.sorted() {
            observers[id]?()
        }
    }
}
