// NereusSDR for iOS: the root every shot hosts its view under, the app's own presentation root
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

@testable import NereusSDR
import SwiftUI
import UIKit

/// The root a shot hosts its view under. The app's root and every sheet and
/// popover take ``SwiftUI/View/workspacePresentationRoot()`` (the workspace
/// switch and the workspace accent as the tint), so a shot hosted here
/// draws a switch, a slider or a tinted control as the app does. The Live
/// Activity's shots do not use it: the widget is drawn outside the app's root.
@MainActor
enum ShotHosting {
    /// A hosting controller for `content` under the app's presentation root.
    static func host<Content: View>(_ content: Content) -> UIHostingController<some View> {
        UIHostingController(rootView: wrap(content))
    }

    /// `content` under the app's presentation root.
    static func wrap<Content: View>(_ content: Content) -> some View {
        content.workspacePresentationRoot()
    }
}
