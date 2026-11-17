// NereusSDR for iOS: pictures of Tools > Recordings and Setup > Recording settings, for comparing with the mockups
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Recording plan Task 17: the Recordings page and the Recording settings
/// page drawn as the app draws them, on a phone, against a recordings
/// client fed by hand: the list, one playing, Select with two picked, light,
/// not connected and an older Core. With `NEREUS_MAIN_SHOTS` set, each
/// picture is written there. The app draws dark whatever the phone's own
/// setting, so the light pictures match the dark ones.
@Suite("Recordings pages pictures", .serialized)
@MainActor
struct RecordingsPagesShotTests {
    typealias Flag = RecordingFlagTests

    /// The Core's document with a disk to pick and totals worth reading.
    static func document() -> [String: Any] {
        var document = Flag.document()
        document["index"] = ["revision": 3, "count": 4, "usedBytes": 1_427_360_000, "freeBytes": 41_000_000_000,
                             "folder": "/r"]
        document["settings"] = ["keepMyVoice": true, "sidesApart": false, "autoStart": false, "autoStop": true,
                                "quietMinutes": 2, "format": "mp3", "disk": "/Volumes/T7",
                                "disks": [["root": "/", "name": "", "free": 41_000_000_000, "total": 64_000_000_000,
                                           "sd": true, "own": true],
                                          ["root": "/Volumes/T7", "name": "Samsung T7", "free": 812_000_000_000,
                                           "total": 1_000_000_000_000, "sd": false, "own": false]]]
        return document
    }

    static func listModel(version: Int64?, connected: Bool = true) async -> (Flag.Rig, RecordingsListModel) {
        let rig = await Flag.rig(version: version, connected: connected)
        if connected {
            Flag.publish(rig, document(), revision: 1)
        }
        let model = RecordingsListModel(client: rig.client)
        // Only a Core with recordings, connected, lists any.
        if connected, let version, version >= RecordingsClient.recordingsVersion {
            model.received(RecordingsPagesTests.entries())
        }
        return (rig, model)
    }

    @Test("Tools > Recordings draws its list, one playing, Select, light, and greyed")
    func recordingsPictures() async throws {
        let (rig, model) = await Self.listModel(version: 3)
        try await shoot("tools-recordings-list") { Self.page(model) }
        try await shoot("tools-recordings-list-light", scheme: .light) { Self.page(model) }

        Flag.playback(rig, id: "r-4", positionMs: 12_000, lengthMs: 205_000, playing: true, onAir: false)
        try await shoot("tools-recordings-playing") { Self.page(model) }
        Flag.playback(rig, id: "r-4", positionMs: 0, lengthMs: 205_000, playing: false, onAir: false)

        model.toggleSelecting()
        model.toggle("r-4")
        model.toggle("r-2")
        try await shoot("tools-recordings-select") { Self.page(model) }
        model.toggleSelecting()

        model.filter = .contact
        model.search = "K4"
        try await shoot("tools-recordings-filtered") { Self.page(model) }

        let (_, older) = await Self.listModel(version: 0)
        try await shoot("tools-recordings-older-core") { Self.page(older) }
        let (_, away) = await Self.listModel(version: 3, connected: false)
        try await shoot("tools-recordings-no-core") { Self.page(away) }
    }

    @Test("Setup > Recording settings draws its five groups, on an older Core and not connected")
    func settingsPictures() async throws {
        for (name, version, connected, scheme) in [
            ("setup-recording-settings", Int64(3), true, ColorScheme.dark),
            ("setup-recording-settings-light", Int64(3), true, ColorScheme.light),
            // Recordings but not the on-air switches: only those dim, their reason under them.
            ("setup-recording-settings-core-without-on-air-switches", RecordingsClient.recordingsVersion, true,
             ColorScheme.dark),
            // No recordings at all: every group dims, the reason at the top.
            ("setup-recording-settings-core-without-recordings", Int64(0), true, ColorScheme.dark),
            ("setup-recording-settings-not-connected", Int64(3), false, ColorScheme.dark),
        ] {
            let rig = await Flag.rig(version: version, connected: connected)
            if connected {
                Flag.publish(rig, Self.document(), revision: 1)
            }
            let model = RecordingSettingsModel(client: rig.client)
            try await shoot(name, height: 1900, scheme: scheme, tab: .setup) {
                NavigationStack {
                    RecordingSettingsPage(model: model)
                }
            }
        }
    }

    /// The screen walk (PR 808): Tools > Recordings (the list, one playing)
    /// at each phone shape at the screen's own height, and Setup > Recording
    /// settings drawn whole at each shape's width, connected and not, written
    /// as `tools-recordings-walk-<page>-<shape>.png` and
    /// `setup-recording-settings-walk-<page>-<shape>.png`.
    @Test(arguments: WalkShape.all)
    func pagesAtEachShape(_ shape: WalkShape) async throws {
        let screen = shape.screen
        let (rig, model) = await Self.listModel(version: 3)
        try await shoot("tools-recordings-walk-list-\(shape)", height: screen.height, width: screen.width,
                        type: shape.type, sideways: shape.sideways) { Self.page(model) }
        Flag.playback(rig, id: "r-4", positionMs: 12_000, lengthMs: 205_000, playing: true, onAir: false)
        try await shoot("tools-recordings-walk-playing-\(shape)", height: screen.height, width: screen.width,
                        type: shape.type, sideways: shape.sideways) { Self.page(model) }
        for (name, connected) in [("connected", true), ("not-connected", false)] {
            let settingsRig = await Flag.rig(version: 3, connected: connected)
            if connected {
                Flag.publish(settingsRig, Self.document(), revision: 1)
            }
            let settings = RecordingSettingsModel(client: settingsRig.client)
            try await shoot("setup-recording-settings-walk-\(name)-\(shape)", height: 1900, width: screen.width,
                            type: shape.type, sideways: shape.sideways, tab: .setup) {
                NavigationStack {
                    RecordingSettingsPage(model: settings)
                }
            }
        }
    }

    /// The page under the Tools tab's bar, as ToolsTab draws it.
    private static func page(_ model: RecordingsListModel) -> some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "Recordings", back: "Tools") {
                RecordingsListPage.Trailing(model: model)
            }
            RecordingsListPage(model: model)
        }
    }

    /// One page over the tab bar on an upright phone, written with `NEREUS_MAIN_SHOTS` set.
    private func shoot<Content: View>(_ name: String, height: CGFloat = 874, width: CGFloat = 402,
                                      scheme: ColorScheme = .dark, type: DynamicTypeSize = .large,
                                      sideways: Bool = false, tab: AppTab = .tools,
                                      @ViewBuilder content: () -> Content) async throws {
        let size = CGSize(width: width, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(tab), sideways: sideways)
        }
        .background(ChromeColours.page)
        .preferredColorScheme(scheme)
        .dynamicTypeSize(type)
        let host = ShotHosting.host(root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        #expect(image.size == size)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try? data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
