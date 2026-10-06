// NereusSDR for iOS: one object the Core mirrors to the app, its class and its values
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine

/// One mirrored object (link document section 7): its key (`radio`,
/// `slice:0`, ...), its class and the latest value of each property, by
/// name. `MirrorStore` keeps it current; screens observe it.
@MainActor
public final class MirrorObject: ObservableObject {
    public let key: String
    public let className: String
    @Published public private(set) var values: [String: MirrorValue]

    init(key: String, className: String, values: [String: MirrorValue]) {
        self.key = key
        self.className = className
        self.values = values
    }

    /// The value of one property, if the Core has sent it.
    public subscript(property: String) -> MirrorValue? {
        values[property]
    }

    /// A new full property set, from an `object.create`.
    func replace(_ next: [String: MirrorValue]) {
        if values != next {
            values = next
        }
    }

    /// The operator's value shown over the Core's while a write of it waits
    /// for its answer, or the Core's value put back (nil: the Core has none).
    func assign(_ changes: [String: MirrorValue?]) {
        var next = values
        for (name, value) in changes {
            next[name] = value
        }
        if next != values {
            values = next
        }
    }

    /// Changed properties, from a `delta` or a write's answer; the rest stay.
    func merge(_ changes: [String: MirrorValue]) {
        var next = values
        for (name, value) in changes {
            next[name] = value
        }
        if next != values {
            values = next
        }
    }
}
