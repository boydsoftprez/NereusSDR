// NereusSDR for iOS: About content and bundle identity checks
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
@testable import NereusSDR
import Testing

@Suite("About this app")
struct AboutAppTests {
    @Test("About launch fixtures select memory storage before the live Keychain flow")
    @MainActor
    func launchFixtures() {
        for argument in ["-NereusFreshWelcome", ConnectionFlow.showBandArgument] {
            let app = AppModel()
            var liveWasConstructed = false
            let flow = NereusSDRApp.launchFlow(app: app, kind: .phone, arguments: [argument]) {
                liveWasConstructed = true
                return ConnectionFlow(app: app, dependencies: ConnectionFlow.Dependencies(
                    keyStore: KeychainKeyStore(item: InMemorySecretItem()),
                    stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
                    transportFactory: WebSocketLinkTransport.factory, clock: SystemLinkClock(),
                    microphone: SystemMicrophoneAccess(), network: nil,
                    appMajors: LinkVersionPolicy.supportedMajors, now: Date.init, browser: nil))
            }
            #expect(!liveWasConstructed, "The \(argument) fixture must not read the simulator Keychain")
            #expect(flow.screen == .welcome)
            #expect(!flow.keyUnreadable)
        }
    }

    @Test("bundle marketing and build values are used, including an absent value")
    func identity() {
        #expect(AboutAppContent.identity(in: ["CFBundleShortVersionString": "2026.9.0", "CFBundleVersion": "10"])
                == "iPhone/iPad app version 2026.9.0, build 10")
        #expect(AboutAppContent.identity(in: [:]) == "iPhone/iPad app version Unknown, build Unknown")
        #expect(AboutAppContent.identity(in: Bundle.main.infoDictionary).contains("iPhone/iPad app version"))
    }

    @Test("the identity line shows the app's own Info.plist build number")
    func identityShowsBundleBuild() throws {
        let info = try #require(Bundle.main.infoDictionary)
        let version = try #require(info["CFBundleShortVersionString"] as? String)
        let build = try #require(info["CFBundleVersion"] as? String, "the app's Info.plist has no CFBundleVersion")
        #expect(!build.isEmpty && !build.contains("$("), "CFBundleVersion was not expanded: \(build)")
        #expect(Int(build) != nil, "CFBundleVersion is not a number: \(build)")
        #expect(AboutAppContent.identity(in: info) == "iPhone/iPad app version \(version), build \(build)")
    }

    @Test("complete desktop roster and source-backed links are retained")
    func desktopParity() throws {
        let content = try AboutAppContent.load()
        #expect(content.roster.count == 5)
        #expect(content.roster.map { $0.names.count } == [21, 1, 4, 1, 1])
        #expect(content.links.count == 13)
        #expect(content.links.first?.title == "NereusSDR releases / What's New")
        #expect(content.links.last?.title == "Apache-Labs Home")
        #expect(content.legalLinks.count == 4)
        #expect(content.texts.copyright.contains("Pierre-Philippe Coupard"))
        #expect(content.texts.warranty.contains("ABSOLUTELY NO WARRANTY"))
        #expect(content.texts.aiDisclosure.contains("AI-assisted"))
        #expect(content.texts.phoneLicence.contains("App Store"))
    }

    @Test("Corresponding Source and Privacy open this app's pages on nereussdr.com")
    func sourceAndPrivacyLinks() throws {
        let content = try AboutAppContent.load()
        #expect(content.sourceLink.title == "Corresponding Source")
        #expect(content.sourceLink.url == URL(string: "https://nereussdr.com/iphone-source"))
        #expect(content.privacyLink.title == "Privacy")
        #expect(content.privacyLink.url == URL(string: "https://nereussdr.com/iphone-privacy"))
        #expect(!content.links.contains { $0.title == content.sourceLink.title || $0.title == content.privacyLink.title })
    }

    @Test("phone Built With comes from all bundled licence notices")
    func phoneLibraries() throws {
        let notices = try LicenseNotice.load()
        #expect(notices.map(\.name) == ["Opus", "libdatachannel", "libjuice", "libsrtp", "usrsctp",
                                          "plog", "Mbed TLS", "libsodium", "spake2-ee"])
        let url = try #require(Bundle.main.url(forResource: "PhoneLicense", withExtension: "txt"))
        let licence = try String(contentsOf: url, encoding: .utf8)
        #expect(licence.contains("Additional permission under GNU GPL version 3 section 7"))
        #expect(licence.contains("complete Corresponding Source"))
    }
}
