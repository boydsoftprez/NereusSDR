// NereusSDR for iOS: one library's notice on the licences screen, read from Licenses.json
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One library the app bundles, with the notice its licence asks to be
/// shown. `Licenses.json` holds one for every row of `ios/THIRD-PARTY.md`;
/// the repository's provenance check keeps the two in step.
struct LicenseNotice: Decodable, Identifiable, Sendable, Equatable {
    let name: String
    let version: String
    let licence: String
    let notice: String

    var id: String { name }

    private struct File: Decodable {
        let libraries: [LicenseNotice]
    }

    /// Every notice in `Licenses.json` in `bundle`, in the file's order.
    static func load(from bundle: Bundle = .main) throws -> [LicenseNotice] {
        guard let url = bundle.url(forResource: "Licenses", withExtension: "json") else {
            throw CocoaError(.fileNoSuchFile)
        }
        return try JSONDecoder().decode(File.self, from: Data(contentsOf: url)).libraries
    }
}
