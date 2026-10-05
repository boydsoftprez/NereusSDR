// NereusSDR for iOS: the media transport's own log on the device console, with addresses and secrets taken out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// libdatachannel's log (ICE, DTLS and SCTP progress), informational and
/// above, on the device console under the category `media.transport`, so a
/// slow reconnect can be read from a device afterwards. Every line is
/// scrubbed first: no address, host name, fingerprint, key, certificate or
/// candidate text reaches the console.
public enum MediaTransportLog {
    private static let logger = Logger(subsystem: "NereusSDR", category: "media.transport")

    /// Sends the transport's log to the console from now on, for the whole
    /// process. Calling it again changes nothing.
    public static func enable() {
        RtcBridge.setConsoleLog { line in
            logger.notice("\(scrub(line), privacy: .public)")
        }
    }

    // Each pattern and what stands in for it, applied in order.
    private static let rules: [(NSRegularExpression, String)] = {
        func rule(_ pattern: String, _ replacement: String) -> (NSRegularExpression, String) {
            // The patterns are fixed text; one that did not compile would be
            // a programming mistake, caught by the tests.
            (try! NSRegularExpression(pattern: pattern), replacement)
        }
        return [
            // An SDP attribute or a candidate: everything after its tag.
            rule(#"(?i)(a=[a-z-]+:|candidate:)\S.*$"#, "$1<removed>"),
            // Certificate or key material in PEM form.
            rule(#"-----BEGIN [A-Z ]+-----.*$"#, "<removed>"),
            // IPv4, with or without a port.
            rule(#"(?<![\w.])\d{1,3}(?:\.\d{1,3}){3}(?::\d{1,5})?(?![\w.])"#, "<address>"),
            // IPv6 (bracketed or not, with a zone or a port) and colon-
            // separated hex such as a fingerprint: two colons or more with a
            // hex digit among them, never inside a word such as a C++ scope
            // or after a dot, as in a source file and line.
            rule(#"\[?(?<![\w:.])(?=[0-9A-Fa-f:]*[0-9A-Fa-f])(?:[0-9A-Fa-f]{0,4}:){2,}[0-9A-Fa-f]{0,4}(?:%[\w.-]+)?\]?(?::\d{1,5})?(?![\w:])"#,
                 "<address>"),
            // A host name: labels joined by dots, ending in letters.
            rule(#"(?<![\w.-])(?:[A-Za-z0-9-]+\.)+[A-Za-z]{2,}(?::\d{1,5})?(?![\w.-])"#, "<host>"),
            // An opaque run of 16 characters or more with a digit in it: a
            // key, a token, a password, an ICE user fragment or a hash.
            rule(#"(?<![\w+/=-])(?=[A-Za-z0-9+/=_-]*\d)[A-Za-z0-9+/=_-]{16,}(?![\w+/=-])"#, "<removed>"),
        ]
    }()

    /// One line with every address, host name and secret replaced.
    public static func scrub(_ line: String) -> String {
        var text = line
        for (expression, replacement) in rules {
            let range = NSRange(text.startIndex..., in: text)
            text = expression.stringByReplacingMatches(in: text, range: range, withTemplate: replacement)
        }
        return text
    }
}
