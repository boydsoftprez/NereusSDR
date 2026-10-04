// NereusSDR for iOS: what the mirror's tests share, a sender that keeps what it sends and fixture replays
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink

/// Stands in for the session's `send`: keeps each message, or refuses them
/// all as a session with no connection does.
final class SentMessages: @unchecked Sendable {
    private let lock = NSLock()
    private var sent: [LinkMessage] = []
    private var refusing = false

    var messages: [LinkMessage] { lock.withLock { sent } }
    var count: Int { lock.withLock { sent.count } }

    func refuseAll(_ refuse: Bool = true) {
        lock.withLock { refusing = refuse }
    }

    var sender: @Sendable (LinkMessage) async throws -> Void {
        { [self] message in
            try lock.withLock {
                if refusing {
                    throw LinkSendError.notConnected
                }
                sent.append(message)
            }
        }
    }

    /// Waits, without sleeping, until `count` messages have been sent. Each
    /// turn also lets the main actor run what is queued on it, where the
    /// mirror's own tasks run.
    @discardableResult
    func settle(untilCount count: Int) async -> Bool {
        for _ in 0..<2_000 {
            if self.count >= count {
                return true
            }
            await Task.yield()
            await MainActor.run {}
        }
        return self.count >= count
    }
}

/// A session fixture's station messages, filled as an app's runner fills
/// them (link document section 16.3).
enum FixtureReplay {
    /// The station messages of the fixture, or with `throughSnapshot`
    /// only those up to and including its first `snapshot.complete`.
    static func stationMessages(_ fixtureId: String, throughSnapshot: Bool = false) throws -> [LinkMessage] {
        guard let fixture = try SessionFixtures.all().first(where: { $0.id == fixtureId }) else {
            throw LinkFixtureLoader.Malformed(description: "no fixture \(fixtureId)")
        }
        var placeholders = FixturePlaceholders(mirrorClasses: try SessionFixtures.mirrorClasses(),
                                               certificateSHA256: Data(repeating: 0, count: 32))
        var steps = Array(fixture.steps.enumerated())
        if throughSnapshot, let end = steps.firstIndex(where: { _, step in
            (step["message"] as? [String: Any])?["type"] as? String == "snapshot.complete"
        }) {
            steps = Array(steps[...end])
        }
        return try steps.compactMap { index, step in
            // The fixture's own client's messages; another client's are not the app's.
            guard !SessionFixturePlayer.isAnotherClients(step), step["from"] as? String == "station",
                  let raw = step["message"] else {
                return nil
            }
            let filled = try placeholders.fillStation(try LinkJSON(foundation: raw), at: "\(fixtureId) step \(index)")
            return try LinkCodec.decode(filled.compactText)
        }
    }

    static func stationHello(minor: UInt16 = 11) -> LinkMessage {
        .hello(LinkMessage.Hello(major: 1, minor: minor, settingsSchema: 0, peer: "nereusd", majors: [1],
                                 features: [:]))
    }

    static let accepted = LinkMessage.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false))

    static func capabilities(_ entries: [String: LinkMessage.PropertyValue]) -> LinkMessage {
        .capabilities(LinkMessage.Capabilities(properties: entries.keys.sorted().map {
            LinkMessage.PropertyEntry(name: $0, value: entries[$0]!)
        }))
    }
}
