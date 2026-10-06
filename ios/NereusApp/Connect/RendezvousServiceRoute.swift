// NereusSDR for iOS: the phone's own route to a paired Core through the remote access service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import NereusMedia
import os

/// The phone's own route: ``RendezvousDialer`` and the data channel it opens.
final class RendezvousServiceRoute: CoreServiceRoute {
    private let dialer: RendezvousDialer
    private let makeDirectOnly: (@Sendable () -> RendezvousServiceRoute)?
    private let coreCandidates = OSAllocatedUnfairLock<(@Sendable ([String]) -> Void)?>(initialState: nil)

    init(dialer: RendezvousDialer,
         makeDirectOnly: (@Sendable () -> RendezvousServiceRoute)? = nil) {
        self.dialer = dialer
        self.makeDirectOnly = makeDirectOnly
    }

    /// The route to `station` as `device`, through `servers` in order.
    convenience init(station: PairedStation, device: DeviceIdentity, servers: [RendezvousServer]) {
        let id = station.rendezvousId
        self.init(dialer: RendezvousDialer(servers: servers, stationId: id, device: device,
                                          relayAllowed: station.relayAllowed ?? true),
                  makeDirectOnly: {
                      RendezvousServiceRoute(dialer: RendezvousDialer(servers: servers, stationId: id,
                                                                     device: device, relayAllowed: false))
                  })
    }

    func makeTransport() -> any SessionTransport {
        DataChannelSessionTransport(connect: dialer.connector)
    }

    func mediaIceSettings() -> IceSettings? {
        dialer.mediaIceSettings()
    }

    var selectedPathRank: Int? { dialer.lastPathRank }
    func directOnlyRoute() -> (any CoreServiceRoute)? {
        let route = makeDirectOnly?()
        if let handler = coreCandidates.withLock({ $0 }) {
            route?.observeCoreCandidates(handler)
        }
        return route
    }
    func mediaRelayContext() -> RelayRouteContext? { dialer.mediaRelayContext() }

    var lastTry: ConnectionAttempt.Try? { dialer.attemptTry }
    var lastError: RendezvousDialError? { dialer.lastError }

    func observeCoreCandidates(_ handler: @escaping @Sendable ([String]) -> Void) {
        coreCandidates.withLock { $0 = handler }
        dialer.observeCoreCandidates(handler)
    }
}
