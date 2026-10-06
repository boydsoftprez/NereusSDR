// NereusSDR for iOS: the Core's answer to one keying verb
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// What came of one keying verb (link document section 18.6).
public enum TransmitAnswer: Equatable, Sendable {
    /// Accepted. A key, TUNE on or two-tone on carries its epoch; an unkey,
    /// TUNE off or two-tone off carries none.
    case accepted(epoch: Int64?)
    /// Refused, with the Core's words, code and fix (section 18.3).
    case refused(TxRefusalInfo)
    /// Nothing was sent, the link went, or no answer came in time.
    case noAnswer
}
