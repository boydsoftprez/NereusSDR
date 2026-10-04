// NereusSDR for iOS: where a list of settings opens its number pad: one pad at a time, at the foot of the screen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import SwiftUI

/// The number pad a screen of setting rows has open (every Setup number
/// row takes a typed value, JJ 2026-09-30). A row opens its pad here and
/// the screen draws it at its foot, over a clear layer whose tap closes it;
/// one pad is open at a time.
@MainActor
final class ValuePadHost: ObservableObject {
    @Published private(set) var pad: ValuePadModel?

    /// Opens `make`'s pad, handing it the way to close itself.
    func open(_ make: (_ close: @escaping () -> Void) -> ValuePadModel) {
        var opened: UUID?
        let pad = make { [weak self] in
            // Closes only the pad it opened; a later pad stays.
            if let self, self.pad?.id == opened {
                self.pad = nil
            }
        }
        opened = pad.id
        self.pad?.retire()
        self.pad = pad
    }

    /// Closes the open pad without sending anything.
    func close() {
        pad?.retire()
        pad = nil
    }
}

private struct ValuePadHostKey: EnvironmentKey {
    static let defaultValue: ValuePadHost? = nil
}

extension EnvironmentValues {
    /// Where a row opens its number pad; nil where no screen draws one.
    var valuePadHost: ValuePadHost? {
        get { self[ValuePadHostKey.self] }
        set { self[ValuePadHostKey.self] = newValue }
    }
}

extension View {
    /// This screen's rows open their number pad in `host`, drawn at its foot.
    func valuePads(_ host: ValuePadHost) -> some View {
        ZStack(alignment: .bottom) {
            environment(\.valuePadHost, host)
            ValuePadHostLayer(host: host)
        }
    }
}

/// The host's open pad, if any.
private struct ValuePadHostLayer: View {
    @ObservedObject var host: ValuePadHost

    var body: some View {
        ValuePadLayer(pad: host.pad)
    }
}
