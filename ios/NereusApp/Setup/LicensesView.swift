// NereusSDR for iOS: the licences screen, every bundled library with its notice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// About this app, Licences: each library the app bundles, its version
/// and licence, and its notice in full.
struct LicensesView: View {
    private let notices: [LicenseNotice] = (try? LicenseNotice.load()) ?? []

    var body: some View {
        List(notices) { notice in
            NavigationLink {
                ScrollView {
                    Text(notice.notice)
                        .font(.footnote.monospaced())
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding()
                }
                .navigationTitle(notice.name)
                .navigationBarTitleDisplayMode(.inline)
            } label: {
                VStack(alignment: .leading, spacing: 2) {
                    Text(notice.name)
                    Text("\(notice.licence), \(notice.version)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
            }
        }
        .navigationTitle("Licenses")
    }
}
