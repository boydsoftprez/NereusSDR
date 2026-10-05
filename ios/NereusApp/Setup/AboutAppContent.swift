// NereusSDR for iOS: load offline About content and identify this bundle
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Project-history copy is an app resource, separate from executable code.
/// This loader does not import code from any credited upstream project.
struct AboutAppContent: Decodable {
    struct Group: Decodable {
        let title: String
        let names: [String]
    }

    struct Link: Decodable, Identifiable {
        let title: String
        let url: URL
        var id: String { title }
    }

    struct Texts: Decodable {
        let lineage: String
        let desktopLibraries: String
        let copyright: String
        let warranty: String
        let phoneLicence: String
        let desktopLicence: String
        let aiDisclosure: String
    }

    let roster: [Group]
    let links: [Link]
    /// This app's Corresponding Source page and its privacy page on the
    /// project's website.
    let sourceLink: Link
    let privacyLink: Link
    let legalLinks: [Link]
    let texts: Texts

    static func load(from bundle: Bundle = .main) throws -> AboutAppContent {
        guard let url = bundle.url(forResource: "AboutContent", withExtension: "json") else {
            throw CocoaError(.fileNoSuchFile)
        }
        return try JSONDecoder().decode(AboutAppContent.self, from: Data(contentsOf: url))
    }

    static func identity(in info: [String: Any]?) -> String {
        let version = info?["CFBundleShortVersionString"] as? String ?? "Unknown"
        let build = info?["CFBundleVersion"] as? String ?? "Unknown"
        return "iPhone/iPad app version \(version), build \(build)"
    }
}
