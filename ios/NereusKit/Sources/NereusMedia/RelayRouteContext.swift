// NereusSDR for iOS: shared lifetime of one grant-bound web relay route
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One introduction's web relay. The successful control peer and each media
/// connection retain this object independently. A media peer must release its
/// claim only after its ICE agent has stopped. The context can outlive the
/// control peer while media drains; dropping the last reference closes it.
public final class RelayRouteContext: @unchecked Sendable {
    private let leg: RelayLeg
    private let bridge: RelayICEBridge

    init(grant: RelayGrant, socketFactory: RelaySocketFactory?, clock: any LinkClock,
         onLifecycle: @escaping @Sendable (RelayLegEvent) async -> Void) throws {
        leg = RelayLeg(grant: grant, factory: socketFactory, clock: clock, onLifecycle: onLifecycle)
        bridge = try RelayICEBridge(leg: leg)
    }

    deinit {
        let bridge = bridge
        let leg = leg
        Task {
            await bridge.close()
            await leg.cancel()
        }
    }

    /// Begins the WSS opening before waiting for candidate admission.
    func start() async {
        await leg.start()
    }

    func claimControl() async throws -> RelayICEClaim {
        try await bridge.claimControl()
    }

    func releaseControl(_ claim: RelayICEClaim) async {
        await bridge.release(claim)
    }

    /// Claim a low-priority candidate for a media ICE agent. Pass its
    /// connection UUID only when the Core negotiated routed media. Legacy
    /// raw media uses nil and holds the sole raw media lane.
    public func claimMedia(connectionId: UUID?) async throws -> RelayICEClaim {
        try await bridge.claimMedia(connectionId: connectionId)
    }

    /// Call after the matching media ICE agent has been deleted.
    public func releaseMedia(_ claim: RelayICEClaim) async {
        await bridge.release(claim)
    }

    /// Ends an unsuccessful or cancelled dial promptly. The successful
    /// route keeps its context until its final owner lets it go.
    func cancel() async {
        await bridge.close()
        await leg.cancel()
    }
}
