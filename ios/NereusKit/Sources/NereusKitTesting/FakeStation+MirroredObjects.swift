// NereusSDR for iOS: a fake Core's further mirrored objects, made from the link's surface
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink

/// Objects a newer Core adds to its snapshot beyond the fixture's, for a
/// test to ``deliver(_:)`` once the fixture has played: the class's schema
/// as `surface.json` lists it, and an `object.create` of every property at
/// its stand-in (false, 0, "", an enum's first value), with the values the
/// test gives. A test that plays a Core with the step attenuator and the
/// Alex antennas sends their capability first, as the Core does before the
/// objects it gates (link document section 7.1, "Minor 11 objects").
extension FakeStation {
    /// The class `className`'s schema, from the suite's `surface.json`.
    public static func schema(ofClass className: String) throws -> LinkMessage.Schema {
        LinkMessage.Schema(className: className, fields: try fields(ofClass: className).map(\.field))
    }

    /// An `object.create` of `className` at `key`: every property of the
    /// class at its stand-in, except those `values` names.
    public static func objectCreate(key: String, className: String,
                                    values: [String: LinkMessage.PropertyValue]) throws -> LinkMessage.ObjectCreate {
        let fields = try fields(ofClass: className)
        if let unknown = values.keys.sorted().first(where: { name in !fields.contains { $0.field.name == name } }) {
            throw LinkFixtureLoader.Malformed(description: "\(className) has no property \(unknown)")
        }
        return LinkMessage.ObjectCreate(key: key, className: className, properties: fields.map { entry in
            LinkMessage.PropertyEntry(ordinal: entry.field.ordinal, name: entry.field.name,
                                      value: values[entry.field.name] ?? entry.standIn)
        })
    }

    /// A class's fields with each one's stand-in value.
    private static func fields(ofClass className: String)
        throws -> [(field: LinkMessage.SchemaField, standIn: LinkMessage.PropertyValue)] {
        guard let properties = try SessionFixtures.mirrorClasses()[className] else {
            throw LinkFixtureLoader.Malformed(description: "surface.json has no class \(className)")
        }
        return try properties.map { property in
            guard let name = property["name"] as? String, let ordinal = property["ordinal"] as? Int,
                  let kindText = property["kind"] as? String,
                  let kind = LinkMessage.WireKind(rawValue: kindText) else {
                throw LinkFixtureLoader.Malformed(description: "surface.json: a \(className) property is malformed")
            }
            let standIn: LinkMessage.PropertyValue
            switch kind {
            case .bool:
                standIn = .bool(false)
            case .i64:
                standIn = .i64(0)
            case .f64:
                standIn = .f64(0)
            case .utf8:
                standIn = .utf8("")
            case .enumeration:
                standIn = .enumeration(Int64((property["enumValues"] as? [Int])?.first ?? 0))
            }
            return (LinkMessage.SchemaField(ordinal: UInt16(ordinal), name: name, kind: kind), standIn)
        }
    }
}
