// NereusSDR for iOS: one app connected to the Core's TCI server, read from the Core's tciClients record
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One app connected to the TCI server the Core runs for its station (the
/// accessory control document, "The `stationTci` object and the station TCI
/// server": stream `tciClients`, at `stationTciVersion` 2). The phone lists
/// what the Core sends and can ask it to close one app by `id`.
///
/// Reading is tolerant: a field that is missing or of another kind reads as
/// empty, never as a failure, so a record from a newer or older Core still
/// lists.
public struct StationTciClient: Sendable, Equatable, Identifiable {
    /// The Core's stable connection id (the record's id).
    public var id: String
    /// The name the app gave itself; empty when it gave none.
    public var name: String
    /// Where it connected from, as the Core shows it.
    public var address: String
    /// The audio, I/Q or sensor streams it asked for.
    public var subscriptions: [String]
    /// It is transmitting through the server.
    public var transmitting: Bool
    /// The last command it sent; empty before any.
    public var lastCommand: String

    /// The stream's name, and the most records it holds.
    public static let streamName = "tciClients"
    public static let capacity = 64
    /// The capability under which the Core runs its station TCI server.
    public static let capabilityName = "stationTciVersion"
    /// The first version with the clients stream, the options and the disconnect.
    public static let clientsVersion: Int64 = 2
    /// The Core's object for its station TCI server.
    public static let objectKey = "stationTci"
    /// Saves the switch and port, and starts or stops the server.
    public static let setVerb = "setStationTci"
    /// Closes one app on the server.
    public static let disconnectVerb = "disconnectStationTciClient"
    /// The port range the Core takes.
    public static let portRange: ClosedRange<Int64> = 1024...65535

    public init(id: String, name: String = "", address: String = "", subscriptions: [String] = [],
                transmitting: Bool = false, lastCommand: String = "") {
        self.id = id
        self.name = name
        self.address = address
        self.subscriptions = subscriptions
        self.transmitting = transmitting
        self.lastCommand = lastCommand
    }

    /// Reads one `tciClients` record.
    public init(record: LinkMessage.RecordBatch.Record) {
        let fields = record.fields
        func text(_ name: String) -> String {
            if case .string(let value)? = fields[name] {
                return value
            }
            return ""
        }
        var subscriptions: [String] = []
        if case .array(let items)? = fields["subscriptions"] {
            subscriptions = items.compactMap { item in
                if case .string(let value) = item {
                    return value
                }
                return nil
            }
        }
        var transmitting = false
        if case .bool(let value)? = fields["transmitting"] {
            transmitting = value
        }
        self.init(id: record.id, name: text("name"), address: text("address"), subscriptions: subscriptions,
                  transmitting: transmitting, lastCommand: text("lastCommand"))
    }
}
