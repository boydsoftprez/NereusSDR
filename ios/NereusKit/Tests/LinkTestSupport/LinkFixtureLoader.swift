// NereusSDR for iOS: finds and reads the link's conformance fixtures for the app's tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The repository's conformance suite, `tests/data/link/v1`, read in place.
/// The fixtures are never bundled into the app; the tests find them from
/// this file's own path in the checkout.
public enum LinkFixtureLoader {
    /// `tests/data/link/v1` in the checkout that holds this file.
    public static let root: URL = {
        var url = URL(fileURLWithPath: #filePath)
        // LinkFixtureLoader.swift, LinkTestSupport, Tests, NereusKit, ios.
        for _ in 0..<5 {
            url.deleteLastPathComponent()
        }
        return url.appendingPathComponent("tests/data/link/v1", isDirectory: true)
    }()

    /// A fixture the suite does not describe the way the link document says.
    public struct Malformed: Error, CustomStringConvertible {
        public let description: String

        public init(description: String) {
            self.description = description
        }
    }

    /// One entry of `manifest.json`.
    public struct Fixture: Sendable {
        public let id: String
        public let file: String
        public let kind: String
    }

    /// One media vector: the packet as it travels and its expectation.
    public struct MediaVector: @unchecked Sendable {
        public let id: String
        public let bytes: Data
        public let codec: String
        /// The `expect` object of the vector's `.expect.json`.
        public let expect: [String: Any]

        public init(id: String, bytes: Data, codec: String, expect: [String: Any]) {
            self.id = id
            self.bytes = bytes
            self.codec = codec
            self.expect = expect
        }
    }

    /// Every fixture `manifest.json` lists.
    public static func manifest() throws -> [Fixture] {
        let object = try jsonObject(at: root.appendingPathComponent("manifest.json"))
        guard let fixtures = object["fixtures"] as? [[String: Any]] else {
            throw Malformed(description: "manifest.json: fixtures must be an array of objects")
        }
        return try fixtures.map { entry in
            guard let id = entry["id"] as? String,
                  let file = entry["file"] as? String,
                  let kind = entry["kind"] as? String else {
                throw Malformed(description: "manifest.json: a fixture lacks id, file or kind")
            }
            return Fixture(id: id, file: file, kind: kind)
        }
    }

    /// Every media vector in the suite, by fixture id.
    public static func mediaVectors() throws -> [String: MediaVector] {
        var vectors: [String: MediaVector] = [:]
        for fixture in try manifest() where fixture.kind == "media" {
            let packet = root.appendingPathComponent(fixture.file)
            let expectation = packet.deletingPathExtension().appendingPathExtension("expect.json")
            let object = try jsonObject(at: expectation)
            guard let codec = object["codec"] as? String,
                  let expect = object["expect"] as? [String: Any] else {
                throw Malformed(description: "\(fixture.id): the expectation lacks codec or expect")
            }
            let unknown = Set(object.keys).subtracting(["codec", "expect"])
            if let key = unknown.sorted().first {
                throw Malformed(description: "\(fixture.id): unknown field \"\(key)\"")
            }
            vectors[fixture.id] = MediaVector(id: fixture.id, bytes: try Data(contentsOf: packet),
                                              codec: codec, expect: expect)
        }
        return vectors
    }

    /// The vectors `vector`'s `after` names, in order: the packets a fresh
    /// decoder takes, unchecked, before `vector` itself. Not recursive.
    /// Throws for an `after` that names the vector itself, a missing vector,
    /// a vector of another codec, or that forms a cycle.
    public static func after(_ vector: MediaVector, in all: [String: MediaVector]) throws -> [MediaVector] {
        guard let value = vector.expect["after"] else {
            return []
        }
        guard let names = value as? [Any] else {
            throw Malformed(description: "\(vector.id): after must be an array of fixture ids")
        }
        var earlier: [MediaVector] = []
        for name in names {
            guard let id = name as? String, let named = all[id] else {
                throw Malformed(description: "\(vector.id): after names no media fixture \"\(name)\"")
            }
            if id == vector.id {
                throw Malformed(description: "\(vector.id): after names the fixture itself")
            }
            if named.codec != vector.codec {
                throw Malformed(description: "\(vector.id): after names \(id), a fixture of another codec")
            }
            earlier.append(named)
        }
        // A cycle: some vector reachable through "after" names this one again.
        var stack = earlier.map(\.id)
        var seen: Set<String> = []
        while let id = stack.popLast() {
            if id == vector.id {
                throw Malformed(description: "\(vector.id): after forms a cycle")
            }
            guard seen.insert(id).inserted else {
                continue
            }
            stack.append(contentsOf: (all[id]?.expect["after"] as? [Any] ?? []).compactMap { $0 as? String })
        }
        return earlier
    }

    /// The JSON object in the file at `url`.
    public static func jsonObject(at url: URL) throws -> [String: Any] {
        let data = try Data(contentsOf: url)
        guard let object = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw Malformed(description: "\(url.lastPathComponent): not a JSON object")
        }
        return object
    }
}
