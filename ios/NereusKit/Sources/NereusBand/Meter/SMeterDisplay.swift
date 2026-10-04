// NereusSDR for iOS: what the S-meter shows at one moment: the needle, the readouts, the peaks and the scales
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// One moment of the S-meter, ready to draw (D86): where the needle
/// points, the readouts either side and the caption, the Signal Peak
/// marker and the peak hold line, and the scales the face draws.
public struct SMeterDisplay: Equatable, Sendable {
    /// Where the needle points, 0 to 1 along the scale.
    public var needle: Double
    public var transmitting: Bool
    /// The classic face's caption: the RX Mode's name, or the TX Mode's on the air.
    public var caption: String
    /// The readouts: the S-units (or `TX`) on the left, the value on the right.
    public var left: String
    public var right: String
    /// Signal Peak's marker, when the held peak is above the reading.
    public var peakMarker: Double?
    /// The peak hold line, when Peak Hold is on.
    public var holdLine: Double?
    /// The receive scale and the TX Mode's scale in the face's style;
    /// nil where the catalogue has not come.
    public var receiveScale: SMeterScale?
    public var transmitScale: SMeterScale?
    /// A vintage card's title and legend.
    public var title: String
    public var legend: String
    /// What VoiceOver says.
    public var spoken: String

    public init(needle: Double, transmitting: Bool, caption: String, left: String, right: String,
                peakMarker: Double?, holdLine: Double?, receiveScale: SMeterScale?, transmitScale: SMeterScale?,
                title: String, legend: String, spoken: String) {
        self.needle = needle
        self.transmitting = transmitting
        self.caption = caption
        self.left = left
        self.right = right
        self.peakMarker = peakMarker
        self.holdLine = holdLine
        self.receiveScale = receiveScale
        self.transmitScale = transmitScale
        self.title = title
        self.legend = legend
        self.spoken = spoken
    }

    /// The scale the needle is on now.
    public var activeScale: SMeterScale? {
        transmitting ? transmitScale : receiveScale
    }
}
