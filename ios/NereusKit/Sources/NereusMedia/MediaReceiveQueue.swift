// NereusSDR for iOS: a bounded receive queue that drops its oldest entries
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What the media peer holds between drains for one kind of media: at most
/// `maxCount` entries and, when `maxBytes` is set, at most that many bytes.
/// A new entry that would break a bound drops the oldest first, so a slow
/// reader always gets the newest media (latest value wins, as at the Core).
/// One reader drains it through ``stream``.
final class MediaReceiveQueue<Element: Sendable>: @unchecked Sendable {
    let maxCount: Int
    let maxBytes: Int?
    private let size: @Sendable (Element) -> Int

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var items: [Element] = []
    private var head = 0
    private var bytes = 0
    private var finished = false
    private var waiter: CheckedContinuation<Element?, Never>?
    private var waiterCancelled = false
    private var accepted = 0
    private var dropped = 0

    init(maxCount: Int, maxBytes: Int? = nil, size: @escaping @Sendable (Element) -> Int = { _ in 0 }) {
        precondition(maxCount > 0)
        self.maxCount = maxCount
        self.maxBytes = maxBytes
        self.size = size
    }

    /// The entries, in arrival order, for the one reader.
    var stream: AsyncStream<Element> {
        AsyncStream(unfolding: { [self] in await next() })
    }

    /// Entries taken since the start, and entries dropped to hold the bounds.
    var counts: (accepted: Int, dropped: Int, held: Int) {
        lock.withLock { (accepted, dropped, items.count - head) }
    }

    /// Adds an entry, dropping the oldest as needed. After ``finish()``
    /// entries are ignored. `maxCount` tightens the count bound for this
    /// entry; it never exceeds the queue's own.
    func push(_ element: Element, maxCount entryMaxCount: Int? = nil) {
        let elementBytes = size(element)
        let countBound = max(1, min(entryMaxCount ?? maxCount, maxCount))
        let resumed: CheckedContinuation<Element?, Never>? = lock.withLock {
            guard !finished else {
                return nil
            }
            accepted += 1
            if let waiter {
                self.waiter = nil
                return waiter
            }
            while items.count - head > 0
                && (items.count - head >= countBound
                    || (maxBytes.map { bytes + elementBytes > $0 } ?? false)) {
                bytes -= size(items[head])
                head += 1
                dropped += 1
            }
            if head > 64 && head * 2 > items.count {
                items.removeFirst(head)
                head = 0
            }
            items.append(element)
            bytes += elementBytes
            return nil
        }
        resumed?.resume(returning: element)
    }

    /// Ends the stream once the held entries are read.
    func finish() {
        let resumed: CheckedContinuation<Element?, Never>? = lock.withLock {
            finished = true
            let waiter = self.waiter
            self.waiter = nil
            return waiter
        }
        resumed?.resume(returning: nil)
    }

    private func next() async -> Element? {
        await withTaskCancellationHandler {
            await withCheckedContinuation { (continuation: CheckedContinuation<Element?, Never>) in
                let immediate: Element?? = lock.withLock {
                    if items.count - head > 0 {
                        let element = items[head]
                        head += 1
                        bytes -= size(element)
                        if head == items.count {
                            items.removeAll(keepingCapacity: true)
                            head = 0
                        }
                        return .some(element)
                    }
                    if finished || waiterCancelled {
                        waiterCancelled = false
                        return .some(nil)
                    }
                    waiter = continuation
                    return nil
                }
                if let immediate {
                    continuation.resume(returning: immediate)
                }
            }
        } onCancel: {
            let resumed: CheckedContinuation<Element?, Never>? = lock.withLock {
                guard let waiter else {
                    waiterCancelled = true
                    return nil
                }
                self.waiter = nil
                return waiter
            }
            resumed?.resume(returning: nil)
        }
    }
}
