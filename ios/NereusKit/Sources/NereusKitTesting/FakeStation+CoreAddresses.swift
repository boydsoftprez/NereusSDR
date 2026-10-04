// NereusSDR for iOS: the fake Core's own dialable addresses, in its devices object
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The fake Core's addresses (link document sections 6.1, 6.3 and 7.1, Core
/// branch `88b6cb5ad`): with ``Additions/coreAddresses`` it advertises
/// `coreAddressesVersion` 1 and its `devices` object carries
/// `coreAddresses`, ordinal 9, the list ``setCoreAddresses(_:)`` gave
/// (`{"addresses":[]}` until then); ``deliverCoreAddresses(_:)`` sends a
/// new list as the Core does when its interfaces renumber, a `delta`
/// carrying `coreAddresses` alone. The fake plays a Core to an app signed
/// in with its device key that declares `coreAddresses`, as the app always
/// does; it does not play the token sign-in that gets neither.
extension FakeStation {
    /// The value a Core with no address to name sends.
    public static let noCoreAddresses = #"{"addresses":[]}"#
    /// `coreAddresses`' ordinal in `StationDevicesFacade`, declared last.
    static let coreAddressesOrdinal: UInt16 = 9

    static func coreAddressesCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        additions.contains(.coreAddresses) ? [(CoreAddressList.capabilityName, 1)] : []
    }

    /// The `devices` schema with `coreAddresses` last, when the fake sends
    /// it and the suite's surface does not list it yet.
    static func devicesSchema(_ filled: String, additions: Additions) throws -> String {
        guard additions.contains(.coreAddresses), case .schema(var schema)? = try? LinkCodec.decode(filled),
              !schema.fields.contains(where: { $0.name == CoreAddressList.propertyName }) else {
            return filled
        }
        schema.fields.append(LinkMessage.SchemaField(ordinal: coreAddressesOrdinal, name: CoreAddressList.propertyName,
                                                     kind: .utf8))
        return LinkCodec.encode(.schema(schema))
    }

    /// The list the next `devices` snapshot carries, as the Core's compact JSON.
    public func setCoreAddresses(_ json: String) {
        lock.withLock { coreAddressesJson = json }
    }

    /// A new list, sent to the newest connection as the Core sends a renumbering.
    public func deliverCoreAddresses(_ json: String) async {
        setCoreAddresses(json)
        await deliver(.delta(LinkMessage.Delta(key: "devices", properties: [
            .init(ordinal: Self.coreAddressesOrdinal, name: CoreAddressList.propertyName, value: .utf8(json)),
        ])))
    }
}

extension FakeStation.Additions {
    /// `coreAddressesVersion` 1 and `devices`' `coreAddresses`. Not in ``all``.
    public static let coreAddresses = FakeStation.Additions(rawValue: 1 << 53)
}
