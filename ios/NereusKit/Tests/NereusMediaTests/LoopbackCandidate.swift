// NereusSDR for iOS: puts a media candidate on the loopback interface for the media tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Keeps the media tests' peers on 127.0.0.1, whatever interfaces the
/// machine has.
enum LoopbackCandidate {
    /// The candidate with its IPv4 address replaced by 127.0.0.1, or nil for
    /// an IPv6 candidate.
    static func rewrite(_ candidate: String) -> String? {
        var fields = candidate.split(separator: " ", omittingEmptySubsequences: false).map(String.init)
        guard fields.count > 5, !fields[4].contains(":") else {
            return nil
        }
        fields[4] = "127.0.0.1"
        return fields.joined(separator: " ")
    }
}
