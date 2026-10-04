// NereusSDR for iOS: what the control data channel reports to its transport
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What a ``ControlChannel`` receives, in order.
public enum ControlChannelEvent: Sendable, Equatable {
    /// One binary data-channel message.
    case binary(Data)
    /// A text data-channel message, which the control channel never
    /// carries; its content does not matter.
    case text
    /// The channel or its connection closed or failed.
    case closed
}
