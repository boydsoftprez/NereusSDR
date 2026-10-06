// NereusSDR for iOS: the licences screen lists every bundled library with its notice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusSDR
import Testing

@Suite("Licences")
struct LicenseNoticeTests {
    /// The checkout's `ios/` directory.
    private var iosDirectory: URL {
        var url = URL(fileURLWithPath: #filePath)
        // LicenseNoticeTests.swift, Tests, NereusApp.
        for _ in 0..<3 {
            url.deleteLastPathComponent()
        }
        return url
    }

    /// The library names in `ios/THIRD-PARTY.md`, read from the checkout.
    private func thirdPartyNames() throws -> [String] {
        let table = try String(contentsOf: iosDirectory.appendingPathComponent("THIRD-PARTY.md"), encoding: .utf8)
        return table.split(separator: "\n").compactMap { line -> String? in
            let cells = line.split(separator: "|", omittingEmptySubsequences: false)
                .map { $0.trimmingCharacters(in: .whitespaces) }
            guard line.hasPrefix("|"), cells.count > 2, cells[1] != "Name", !cells[1].hasPrefix("---") else {
                return nil
            }
            return cells[1]
        }
    }

    @Test("the app bundles a notice for every third-party library, in the table's order")
    func everyLibraryHasItsNotice() throws {
        let notices = try LicenseNotice.load()
        let names = try thirdPartyNames()
        #expect(names.count == 9)
        #expect(notices.map(\.name) == names)
        for notice in notices {
            #expect(!notice.notice.isEmpty, "\(notice.name) has no notice")
            #expect(!notice.licence.isEmpty)
            #expect(!notice.version.isEmpty)
        }
    }

    @Test("a library whose source files carry notices of their own shows them after its licence")
    func sourceNoticesFollowTheLicence() throws {
        let notices = try LicenseNotice.load()
        let packaging = iosDirectory.deletingLastPathComponent()
            .appendingPathComponent("packaging/third-party-licenses")
        var withSourceNotices: [String] = []
        for notice in notices {
            let slug = notice.name.lowercased().replacingOccurrences(of: " ", with: "-")
            let file = packaging.appendingPathComponent("\(slug)-notices.txt")
            guard let text = try? String(contentsOf: file, encoding: .utf8) else {
                continue
            }
            withSourceNotices.append(notice.name)
            #expect(notice.notice.hasSuffix("\n" + text), "\(notice.name) lacks its source notices")
        }
        #expect(withSourceNotices == ["Opus", "libdatachannel", "libjuice", "libsrtp", "usrsctp", "libsodium"])
        // libsodium's SHA-256 and SHA-512 are always compiled, with Colin Percival's notice.
        let sodium = try #require(notices.first { $0.name == "libsodium" })
        #expect(sodium.notice.contains("Colin Percival"))
    }
}
