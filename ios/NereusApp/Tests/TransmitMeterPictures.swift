// NereusSDR for iOS: approved meter state pictures in a local test window
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@MainActor
enum TransmitMeterPictures {
    static let states: [(name: String, operate: Bool, keyed: Bool, watts: Double)] = [
        ("operate-keyed", true, true, 42), ("standby-keyed", false, true, 98), ("operate-idle", true, false, 42)
    ]
    static func capture<V: View>(_ view: V, name: String, size: CGSize = CGSize(width: 402, height: 874), ready: () async throws -> Void = {}, check: (UIWindow) throws -> Void = { _ in }) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(view.preferredColorScheme(.dark))
        host.view.frame = window.bounds
        let wasAccessible = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true; window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasAccessible)
        }
        await ShotWait.laidOut(window)
        try await ready()
        await ShotWait.laidOut(window)
        try check(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty {
            let data = try #require(image.pngData())
            try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent(name + ".png"))
        }
    }
}
