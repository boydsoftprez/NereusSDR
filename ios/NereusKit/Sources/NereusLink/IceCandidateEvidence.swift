// NereusSDR for iOS: the addresses each end offered through the service, and how each pair of them went
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What one connection through the remote access service offered and used
/// (R-IOS-16; link document section 20): the candidates this phone gathered,
/// the ones the Core sent, the pair the connection chose, and so an outcome
/// for each pair of one address family. The ICE library reports the pair it
/// chose and nothing about the others, so a pair not used is only that: its
/// check may have failed or been passed over. When no pair connected, every
/// pair failed. Kept in memory for the diagnostics page only; never logged.
public struct IceCandidateEvidence: Sendable, Equatable {
    /// One candidate: an address and UDP port, and how it was found.
    public struct Candidate: Sendable, Equatable {
        public enum Kind: Sendable, Equatable {
            /// The end's own interface address.
            case host
            /// The address the internet sees (server or peer reflexive).
            case reflexive
            /// An address on the relay.
            case relay
        }

        public var address: String
        public var port: UInt16
        public var kind: Kind

        public var isIPv6: Bool { AddressFamilies.ofLiteral(address).ipv6 }

        /// Reads an SDP candidate line (`candidate:...` or `a=candidate:...`);
        /// nil for an empty line (the end of candidates), a TCP candidate, or
        /// one it cannot read.
        public init?(line: String) {
            var text = Substring(line.trimmingCharacters(in: .whitespacesAndNewlines))
            if text.hasPrefix("a=") { text = text.dropFirst(2) }
            let fields = text.split(separator: " ")
            guard fields.count >= 8, fields[0].hasPrefix("candidate:"),
                  fields[2].uppercased() == "UDP", let port = UInt16(fields[5]), fields[6] == "typ" else {
                return nil
            }
            let address = String(fields[4])
            guard AddressFamilies.ofLiteral(address).known else {
                return nil
            }
            switch fields[7] {
            case "host": kind = .host
            case "srflx", "prflx": kind = .reflexive
            case "relay": kind = .relay
            default: return nil
            }
            self.address = address
            self.port = port
        }

        /// `address:port`, an IPv6 address in brackets.
        public var text: String {
            isIPv6 ? "[\(address)]:\(port)" : "\(address):\(port)"
        }

        var kindText: String {
            switch kind {
            case .host: "its own"
            case .reflexive: "as the internet sees it"
            case .relay: "a relay"
            }
        }

        func sameEndpoint(_ other: Candidate) -> Bool {
            port == other.port && address.lowercased() == other.address.lowercased()
        }
    }

    /// How the connection ended.
    public enum Ending: Sendable, Equatable {
        /// It opened, on the chosen pair.
        case connected
        /// It failed or ran out of time: no pair connected.
        case failed
        /// It was stopped before it finished (another path won, or the
        /// operator cancelled).
        case stopped
    }

    public struct Pair: Sendable, Equatable {
        public enum Outcome: Sendable, Equatable {
            /// The connection went over this pair.
            case used
            /// The connection went over another pair.
            case notUsed
            /// No pair connected.
            case failed
            /// The dial stopped before its checks finished.
            case unfinished
        }

        public var phone: Candidate
        public var core: Candidate
        public var outcome: Outcome
    }

    public var phone: [Candidate]
    public var core: [Candidate]
    public var pairs: [Pair]

    /// `phone` and `core` are candidate lines as they were gathered and
    /// received; `chosen` the pair libdatachannel reports as selected.
    public init(phone: [String], core: [String], chosen: (local: String, remote: String)?, ending: Ending) {
        self.phone = phone.compactMap(Candidate.init(line:))
        self.core = core.compactMap(Candidate.init(line:))
        let used = chosen.flatMap { pair -> (Candidate, Candidate)? in
            guard let local = Candidate(line: pair.local), let remote = Candidate(line: pair.remote) else {
                return nil
            }
            return (local, remote)
        }
        var pairs: [Pair] = []
        var chosenListed = false
        for mine in self.phone {
            for theirs in self.core where theirs.isIPv6 == mine.isIPv6 {
                let outcome: Pair.Outcome
                if let used, used.0.sameEndpoint(mine), used.1.sameEndpoint(theirs) {
                    outcome = .used
                    chosenListed = true
                } else {
                    switch ending {
                    case .connected: outcome = .notUsed
                    case .failed: outcome = .failed
                    case .stopped: outcome = .unfinished
                    }
                }
                pairs.append(Pair(phone: mine, core: theirs, outcome: outcome))
            }
        }
        // A pair found during the checks (peer reflexive) is listed too.
        if let used, !chosenListed {
            pairs.append(Pair(phone: used.0, core: used.1, outcome: .used))
        }
        self.pairs = pairs
    }

    /// Plain words for the diagnostics page.
    public var summary: String {
        func offered(_ candidates: [Candidate]) -> String {
            candidates.isEmpty ? "nothing"
                : candidates.map { "\($0.text) (\($0.kindText))" }.joined(separator: ", ")
        }
        var text = "This phone offered \(offered(phone)). The Core offered \(offered(core))."
        guard !pairs.isEmpty else {
            return text
        }
        let lines = pairs.map { pair -> String in
            let outcome: String
            switch pair.outcome {
            case .used: outcome = "used"
            case .notUsed: outcome = "not used"
            case .failed: outcome = "did not connect"
            case .unfinished: outcome = "not finished"
            }
            return "\(pair.phone.text) to \(pair.core.text): \(outcome)"
        }
        text += " " + lines.joined(separator: "; ") + "."
        if pairs.contains(where: { $0.outcome == .notUsed }) {
            text += " A pair not used either failed its check or was passed over; this phone cannot tell which."
        }
        return text
    }

    public static func == (lhs: IceCandidateEvidence, rhs: IceCandidateEvidence) -> Bool {
        lhs.phone == rhs.phone && lhs.core == rhs.core && lhs.pairs == rhs.pairs
    }
}
