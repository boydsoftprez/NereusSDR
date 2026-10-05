// NereusSDR for iOS: what finds Cores on this network, so the connecting flow can be tested without Bonjour
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Something that finds Cores on this network: ``StationBrowser`` on a
/// phone, a stand-in in tests. `start` reports every change of what is
/// found until `stop`; starting again replaces the earlier report.
public protocol StationBrowsing: Sendable {
    func start(_ onUpdate: @escaping @Sendable (StationBrowser.Update) -> Void) async
    func stop() async
}
