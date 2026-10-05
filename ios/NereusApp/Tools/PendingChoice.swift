// NereusSDR for iOS: a choice a Tools page shows over the Core's value until the Core has answered it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror

/// The operator's choice on a Tools page, shown in place of the Core's
/// value from the tap until the Core has answered it. The Core answers by
/// sending the choice back, or by sending a value this run of choices never
/// asked for (another device's, or the one from before). Each value the
/// Core sends is heard as it lands, not only as the page next reads the
/// object, so an echo that a newer value overtakes within one turn of the
/// main queue still counts; without that the choice would stay over the
/// Core's value until the Core changed it again.
@MainActor
final class PendingChoice<Value: Equatable> {
    /// The choice on its way, shown over the Core's value; nil when none is.
    private(set) var value: Value?
    /// Every value asked for since the page last showed the Core's own.
    private var asked: [Value] = []
    /// The Core has answered ``value`` since it was chosen.
    private var answered = false
    private var watched: ObjectIdentifier?
    private var watch: AnyCancellable?

    /// Hears each value `read` finds in `object` from now on; a page calls
    /// it with its object on every read, and it follows a replacement.
    func follow(_ object: MirrorObject?, read: @escaping ([String: MirrorValue]) -> Value?) {
        let id = object.map(ObjectIdentifier.init)
        guard id != watched else {
            return
        }
        watched = id
        watch = object?.$values.map(read).removeDuplicates().dropFirst().sink { [weak self] next in
            self?.heard(next)
        }
    }

    /// The operator chose `next`: shown until the Core answers it.
    func choose(_ next: Value) {
        value = next
        asked.append(next)
        answered = false
    }

    /// Shows the Core's value again.
    func drop() {
        value = nil
        asked = []
        answered = false
    }

    /// Called on each read: once nothing is on its way, a choice the Core
    /// has answered, or whose value the Core now holds, gives way to it.
    func settle(mirrored: Value?, sending: Bool) {
        guard let value, !sending else {
            return
        }
        if answered || value == mirrored {
            drop()
        }
    }

    private func heard(_ next: Value?) {
        guard let value, let next else {
            return
        }
        if next == value || !asked.contains(next) {
            answered = true
        }
    }
}
