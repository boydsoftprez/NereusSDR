// NereusSDR for iOS: the widget extension's entry point, holding the Live Activity
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import WidgetKit

@main
struct NereusActivityBundle: WidgetBundle {
    var body: some Widget {
        StationActivityWidget()
    }
}
