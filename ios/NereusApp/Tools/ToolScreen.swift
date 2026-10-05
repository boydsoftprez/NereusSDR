// NereusSDR for iOS: one station tool page on the Tools tab, with its model, its scrolling and its number pad
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// A station tool page as the Tools tab shows it: its model made when the
/// page opens, the page scrolling under the navigation bar, and the number
/// pad the page opens at the foot of the screen.
struct ToolScreen<Model: ObservableObject, Page: View>: View {
    @StateObject private var model: Model
    private let pad: (Model) -> ValuePadModel?
    private let page: (Model) -> Page

    init(model: @autoclosure @escaping () -> Model, pad: @escaping (Model) -> ValuePadModel? = { _ in nil },
         @ViewBuilder page: @escaping (Model) -> Page) {
        _model = StateObject(wrappedValue: model())
        self.pad = pad
        self.page = page
    }

    var body: some View {
        ZStack(alignment: .bottom) {
            ScrollView {
                page(model)
                    .padding(12)
                    .padding(.bottom, 8)
            }
            ValuePadLayer(pad: pad(model))
        }
    }
}
