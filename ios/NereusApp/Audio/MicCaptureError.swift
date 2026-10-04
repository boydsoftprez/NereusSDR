// NereusSDR for iOS: why the microphone capture could not be built. For the log, never for the operator
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why ``MicCapture`` could not build its engine.
enum MicCaptureError: Error, Equatable, Sendable {
    /// The phone reports no usable input, or its format cannot be converted.
    case noInput
}
