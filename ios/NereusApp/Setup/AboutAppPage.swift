// NereusSDR for iOS: app identity, project history, credits and licences
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit

struct AboutAppPage: View {
    @Environment(\.openURL) private var openURL
    @Environment(\.dismiss) private var dismiss
    @State private var failedLink: String?
    var buildTag: String? = BuildTag.current
    var showsDone = false
    private let content = try? AboutAppContent.load()

    private var phoneLicense: String {
        guard let url = Bundle.main.url(forResource: "PhoneLicense", withExtension: "txt"),
              let text = try? String(contentsOf: url, encoding: .utf8) else {
            return "The bundled license could not be read."
        }
        return text
    }

    var body: some View {
        List {
            Section {
                HStack(spacing: 14) {
                    Image("WelcomeMark")
                        .resizable()
                        .frame(width: 52, height: 52)
                        .clipShape(RoundedRectangle(cornerRadius: 10))
                        .accessibilityHidden(true)
                    VStack(alignment: .leading, spacing: 4) {
                        Text("NereusSDR").font(.title2.bold())
                        Text("Cross-platform SDR console, with a native iPhone/iPad app")
                            .font(.subheadline)
                    }
                }
                Text(AboutAppContent.identity(in: Bundle.main.infoDictionary))
                    .accessibilityIdentifier("AboutAppIdentity")
                Text("iOS \(UIDevice.current.systemVersion) on \(UIDevice.current.model)")
                if let buildTag {
                    Text(BuildTag.label(for: buildTag))
                        .accessibilityIdentifier("AboutBuildTag")
                }
                Text("Created by JJ Boyd · KG4VCF")
            }

            Section("Standing on the Shoulders of Giants") {
                Text(content?.texts.lineage ?? "Bundled About information could not be read.")
            }

            Section("Contributors") {
                ForEach((content?.roster ?? []).indices, id: \.self) { index in
                    let group = content!.roster[index]
                    VStack(alignment: .leading, spacing: 6) {
                        Text(group.title).font(.headline)
                        ForEach(group.names, id: \.self) { name in
                            Text(name).font(.subheadline)
                        }
                    }
                    .padding(.vertical, 4)
                }
            }

            Section("Links") {
                ForEach(content?.links ?? []) { link in
                    externalLink(link)
                }
                if let content {
                    externalLink(content.sourceLink)
                    externalLink(content.privacyLink)
                }
            }

            Section("Built With on this phone") {
                Text("The phone app uses native iOS frameworks. Its bundled third-party libraries are listed from this build's license manifest.")
                if let notices = try? LicenseNotice.load(), !notices.isEmpty {
                    ForEach(notices) { notice in
                        Text("\(notice.name) · \(notice.version) · \(notice.licence)")
                    }
                } else {
                    Text("The bundled library list could not be read.")
                }
                NavigationLink("Licenses and full third-party notices") {
                    LicensesView()
                }
                .accessibilityIdentifier("AboutLicences")
            }

            Section("Desktop and Core components") {
                Text(content?.texts.desktopLibraries ?? "Bundled About information could not be read.")
            }

            Section("Copyright and licenses") {
                Text(content?.texts.copyright ?? "Bundled About information could not be read.")
                Text(content?.texts.warranty ?? "Bundled About information could not be read.")
                Text(content?.texts.phoneLicence ?? "Bundled About information could not be read.")
                NavigationLink("NereusSDR for iOS license and App Store permission") {
                    ScrollView {
                        Text(phoneLicense)
                            .font(.footnote.monospaced())
                            .textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding()
                    }
                    .navigationTitle("iOS license")
                }
                Text(content?.texts.desktopLicence ?? "Bundled About information could not be read.")
                ForEach(content?.legalLinks ?? []) { link in
                    externalLink(link)
                }
                Text(content?.texts.aiDisclosure ?? "Bundled About information could not be read.")
                Text("HPSDR protocol © TAPR")
            }
        }
        .navigationTitle("About this app")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            if showsDone {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .safeAreaInset(edge: .bottom) {
            if let failedLink {
                Text("Could not open \(failedLink). Check your connection and try again.")
                    .font(.footnote)
                    .padding(10)
                    .frame(maxWidth: .infinity)
                    .background(.regularMaterial)
                    .accessibilityIdentifier("AboutLinkError")
            }
        }
    }

    private func externalLink(_ link: AboutAppContent.Link) -> some View {
        Button {
            failedLink = nil
            openURL(link.url) { accepted in
                if !accepted { failedLink = link.title }
            }
        } label: {
            Label(link.title, systemImage: "arrow.up.right.square")
        }
        .accessibilityLabel("Open \(link.title)")
        .accessibilityIdentifier("AboutLink-\(link.title)")
    }
}
