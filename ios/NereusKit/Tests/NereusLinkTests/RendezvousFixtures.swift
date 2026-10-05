// NereusSDR for iOS: finds and reads the rendezvous conformance suite for the app's tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import LinkTestSupport

/// The rendezvous conformance suite, `rendezvous/conformance/v1`, read in
/// place (the rendezvous document, section 10). Never bundled into the app.
enum RendezvousFixtures {
    typealias Malformed = LinkFixtureLoader.Malformed

    /// `rendezvous/conformance/v1` in the checkout that holds this file.
    static let root: URL = {
        var url = URL(fileURLWithPath: #filePath)
        // RendezvousFixtures.swift, NereusLinkTests, Tests, NereusKit, ios.
        for _ in 0..<5 {
            url.deleteLastPathComponent()
        }
        return url.appendingPathComponent("rendezvous/conformance/v1", isDirectory: true)
    }()

    struct Entry: Sendable {
        let id: String
        let file: String
        let kind: String
    }

    /// Every fixture the manifest lists.
    static func manifest() throws -> [Entry] {
        let object = try LinkFixtureLoader.jsonObject(at: root.appendingPathComponent("manifest.json"))
        guard object["rendezvousVersions"] as? [Int] == [1], let fixtures = object["fixtures"] as? [[String: Any]] else {
            throw Malformed(description: "manifest.json: rendezvousVersions [1] and a fixtures array expected")
        }
        return try fixtures.map { entry in
            guard let id = entry["id"] as? String, let file = entry["file"] as? String,
                  let kind = entry["kind"] as? String, ["crypto", "control", "session"].contains(kind) else {
                throw Malformed(description: "manifest.json: a fixture lacks id, file or a known kind")
            }
            return Entry(id: id, file: file, kind: kind)
        }
    }

    static func object(_ file: String) throws -> [String: Any] {
        try LinkFixtureLoader.jsonObject(at: root.appendingPathComponent(file))
    }

    static func text(_ file: String) throws -> String {
        try String(contentsOf: root.appendingPathComponent(file), encoding: .utf8)
    }

    static func hex(_ text: String) -> Data? {
        guard text.count.isMultiple(of: 2) else {
            return nil
        }
        var bytes: [UInt8] = []
        var index = text.startIndex
        while index < text.endIndex {
            let next = text.index(index, offsetBy: 2)
            guard let byte = UInt8(text[index..<next], radix: 16) else {
                return nil
            }
            bytes.append(byte)
            index = next
        }
        return Data(bytes)
    }

    /// The relay credentials of the rendezvous document, section 8, as the
    /// service mints them: `"<expires>:<station id>"` and the standard
    /// base64 of its HMAC-SHA1 under `secret`. Test code only: the app never
    /// mints credentials.
    static func turnPassword(secret: String, username: String) -> String {
        let code = HMAC<Insecure.SHA1>.authenticationCode(for: Data(username.utf8),
                                                         using: SymmetricKey(data: Data(secret.utf8)))
        return Data(code).base64EncodedString()
    }
}
