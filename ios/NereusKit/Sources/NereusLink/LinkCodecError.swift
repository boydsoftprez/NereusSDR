// NereusSDR for iOS: why a text is not a link message the app can read
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why `LinkCodec.decode` refused a message. These are for the log, never
/// for the operator.
public enum LinkCodecError: Error, Equatable, CustomStringConvertible {
    /// The text is not a JSON object.
    case notAnObject
    /// `type` is missing, not a string, or names no kind the link has.
    case unknownKind(String)
    /// A key the kind requires is missing.
    case missingKey(kind: String, key: String)
    /// A key holds the wrong JSON type or a value outside its range.
    case invalidValue(kind: String, key: String)
    /// A `media.control` or `station.metrics.v1` message over its cap.
    case overCap(kind: String, bytes: Int, cap: Int)

    public var description: String {
        switch self {
        case .notAnObject:
            return "not a JSON object"
        case .unknownKind(let name):
            return "unknown message kind \"\(name)\""
        case .missingKey(let kind, let key):
            return "\(kind): missing \"\(key)\""
        case .invalidValue(let kind, let key):
            return "\(kind): \"\(key)\" has the wrong type or is out of range"
        case .overCap(let kind, let bytes, let cap):
            return "\(kind): \(bytes) bytes, over its cap of \(cap)"
        }
    }
}
